class ZenMapFogServerManager
{
	protected static ref ZenMapFogServerManager s_Instance;

	protected ref ZenMapFogConfig m_Config;
	protected ref ZenMapFogDatabase m_Database;
	protected ref map<string, ref ZenMapFogPlayerData> m_PlayerDataIndex;
	protected ref map<string, ref set<int>> m_PlayerCellSets;
	protected ref map<string, float> m_PlayerHeightRevealState;
	protected ref map<string, string> m_LastRevealState;
	protected bool m_Initialized;
	protected bool m_ConfigLoadFailed;
	protected bool m_DatabaseDirty;
	protected bool m_SaveScheduled;
	protected float m_MapSizeMeters;
	protected int m_CellColumns;
	protected int m_CellRows;
	protected string m_DatabasePath;
	protected string m_WorldName;

	void ZenMapFogServerManager()
	{
		m_Config = new ZenMapFogConfig;
		m_Database = new ZenMapFogDatabase;
		m_PlayerDataIndex = new map<string, ref ZenMapFogPlayerData>;
		m_PlayerCellSets = new map<string, ref set<int>>;
		m_LastRevealState = new map<string, string>;
		m_PlayerHeightRevealState = new map<string, float>;
	}

	static ZenMapFogServerManager Get()
	{
		if (!s_Instance)
			s_Instance = new ZenMapFogServerManager;

		return s_Instance;
	}

	void Init()
	{
		if (m_Initialized || !g_Game.IsServer())
			return;

		m_Initialized = true;
		m_WorldName = g_Game.GetWorldName();
		m_LastRevealState.Clear();
		m_PlayerHeightRevealState.Clear();
		LoadConfig();
		if (m_ConfigLoadFailed)
			return;

		ValidateConfig();
		m_MapSizeMeters = ResolveMapSize();
		if (m_MapSizeMeters < 20.0)
		{
			m_Config.Enabled = 0;
			m_ConfigLoadFailed = true;
			Print("[ZenMapFog] ERROR: Could not resolve a usable map size. Set MapSizeOverrideMeters to the terrain width (at least 20m). Config and database left untouched.");
			return;
		}

		// The outside-terrain renderer needs two distinct sample cells inside the map.
		float maximumCellSize = m_MapSizeMeters * 0.5;
		float minimumCellSize = Math.Max(10.0, m_MapSizeMeters / (ZenMapFogConstants.CELL_COORD_MAX + 1));
		float requestedCellSize = m_Config.CellSizeMeters;
		m_Config.CellSizeMeters = Math.Clamp(requestedCellSize, minimumCellSize, maximumCellSize);
		if (requestedCellSize != m_Config.CellSizeMeters)
			Print(string.Format("[ZenMapFog] WARNING: CellSizeMeters adjusted from %1 to %2 for map geometry.", requestedCellSize, m_Config.CellSizeMeters));
		ValidateConfig();
		SaveConfig();
		m_CellColumns = Math.Ceil(m_MapSizeMeters / m_Config.CellSizeMeters);
		m_CellRows = m_CellColumns;

		if (m_CellColumns < 1)
			m_CellColumns = 1;

		if (m_CellRows < 1)
			m_CellRows = 1;

		BuildDatabasePath();
		LoadDatabase();

		if (m_Config.Enabled)
		{
			int intervalMs = m_Config.TrackingIntervalSeconds * 1000.0;
			if (intervalMs < 100)
				intervalMs = 100;

			g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(TrackPlayers, intervalMs, true);
			TrackPlayers();
		}

		Log(string.Format("Initialized. World=%1 MapSize=%2 CellSize=%3 GridSize=%4 DB=%5", m_WorldName, m_MapSizeMeters, m_Config.CellSizeMeters, m_Config.GridCellSizeMeters, m_DatabasePath));
	}

	void Shutdown()
	{
		if (!m_Initialized)
			return;

		g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(TrackPlayers);
		g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(FlushDatabase);
		m_SaveScheduled = false;
		m_Initialized = false;
		FlushDatabase();
	}

	ZenMapFogConfig GetConfig()
	{
		return m_Config;
	}

	float GetMapSizeMeters()
	{
		return m_MapSizeMeters;
	}

	int GetCellColumns()
	{
		return m_CellColumns;
	}

	int GetCellRows()
	{
		return m_CellRows;
	}

	void ResetPlayerFogOnDeath(PlayerBase player)
	{
		if (!g_Game.IsServer() || !player)
			return;

		if (!m_Initialized)
			Init();

		if (!m_Config.Enabled || m_ConfigLoadFailed || !m_Config.ResetMapFogOnDeath)
			return;

		PlayerIdentity identity = player.GetIdentity();
		if (!identity)
			return;

		string uid = GetIdentityUID(identity);
		if (uid == "")
			return;

		string dataKey = BuildPlayerDataKey(m_WorldName, uid);

		ZenMapFogPlayerData data;
		if (!m_PlayerDataIndex.Find(dataKey, data) || !data)
			return;

		ResetPlayerDiscovery(data);
		m_LastRevealState.Remove(uid);
		m_PlayerHeightRevealState.Remove(uid);

		// Immediately replace the client's current discovery state with the now-empty state.
		SyncPlayerMapFog(player, identity);

		Log(string.Format("Reset map fog on death for %1", uid));
	}

	protected void ResetPlayerDiscovery(ZenMapFogPlayerData data)
	{
		if (!data)
			return;
		data.RevealAll = 0;
		if (data.DiscoveredCells)
			data.DiscoveredCells.Clear();
		set<int> cellSet;
		string dataKey = BuildPlayerDataKey(data.WorldName, data.PlayerUID);
		if (m_PlayerCellSets.Find(dataKey, cellSet) && cellSet)
			cellSet.Clear();
		MarkDatabaseDirty();
	}

	void ForgetPlayerSession(PlayerIdentity identity)
	{
		if (!identity)
			return;
		string uid = GetIdentityUID(identity);
		m_LastRevealState.Remove(uid);
		m_PlayerHeightRevealState.Remove(uid);
	}

	void SyncPlayerMapFog(PlayerBase player, PlayerIdentity identity)
	{
		if (!m_Initialized)
			Init();

		if (!player || !identity)
			return;

		ZenMapFogNetwork.Get().SendSyncBegin(player, identity, m_Config, m_MapSizeMeters);
		ZenMapFogNetwork.Get().SendSyncGridSettings(player, identity, m_Config);
		ZenMapFogNetwork.Get().SendSyncRoundedFogSettings(player, identity, m_Config);

		if (!m_Config.Enabled || m_ConfigLoadFailed)
		{
			ZenMapFogNetwork.Get().SendSyncEnd(player, identity, 0);
			return;
		}

		ZenMapFogPlayerData data = GetPlayerData(player);
		if (!data)
			return;

		if (data.RevealAll)
		{
			ZenMapFogNetwork.Get().SendSyncRevealAll(player, identity);
			ZenMapFogNetwork.Get().SendSyncEnd(player, identity, 0);
			return;
		}

		int totalCells = data.DiscoveredCells.Count();
		int index = 0;
		while (index < totalCells)
		{
			ref array<int> batch = new array<int>;
			int end = Math.Min(index + ZenMapFogConstants.SYNC_BATCH_SIZE, totalCells);

			for (int i = index; i < end; i++)
				batch.Insert(data.DiscoveredCells[i]);

			ZenMapFogNetwork.Get().SendSyncBatch(player, identity, batch);
			index = end;
		}

		ZenMapFogNetwork.Get().SendSyncEnd(player, identity, totalCells);
	}

	int RevealRadius(PlayerBase player, float distance)
	{
		if (!g_Game.IsServer() || !player)
			return 0;

		if (!m_Initialized)
			Init();

		if (!m_Config.Enabled || m_ConfigLoadFailed)
			return 0;

		distance = Math.Clamp(distance, 0.0, m_Config.CellSizeMeters * ZenMapFogConstants.MAX_REVEAL_RADIUS_CELLS);
		ZenMapFogPlayerData data = GetPlayerData(player);
		if (!data || data.RevealAll)
			return 0;

		vector position = player.GetPosition();
		float cellSize = m_Config.CellSizeMeters;
		int minCellX = Math.Floor((position[0] - distance - m_Config.MapOriginX) / cellSize) - 1;
		int maxCellX = Math.Floor((position[0] + distance - m_Config.MapOriginX) / cellSize) + 1;
		int minCellZ = Math.Floor((position[2] - distance - m_Config.MapOriginZ) / cellSize) - 1;
		int maxCellZ = Math.Floor((position[2] + distance - m_Config.MapOriginZ) / cellSize) + 1;

		minCellX = Math.Max(minCellX, ZenMapFogConstants.CELL_COORD_MIN);
		minCellZ = Math.Max(minCellZ, ZenMapFogConstants.CELL_COORD_MIN);
		maxCellX = Math.Min(maxCellX, ZenMapFogConstants.CELL_COORD_MAX);
		maxCellZ = Math.Min(maxCellZ, ZenMapFogConstants.CELL_COORD_MAX);

		float distanceSq = distance * distance;
		ref array<int> newlyDiscovered = new array<int>;

		for (int cellX = minCellX; cellX <= maxCellX; cellX++)
		{
			float cellMinX = m_Config.MapOriginX + (cellX * cellSize);
			float cellMaxX = cellMinX + cellSize;

			for (int cellZ = minCellZ; cellZ <= maxCellZ; cellZ++)
			{
				float cellMinZ = m_Config.MapOriginZ + (cellZ * cellSize);
				float cellMaxZ = cellMinZ + cellSize;

				float closestX = position[0];
				if (closestX < cellMinX)
					closestX = cellMinX;
				else if (closestX > cellMaxX)
					closestX = cellMaxX;

				float closestZ = position[2];
				if (closestZ < cellMinZ)
					closestZ = cellMinZ;
				else if (closestZ > cellMaxZ)
					closestZ = cellMaxZ;

				float dx = position[0] - closestX;
				float dz = position[2] - closestZ;
				if ((dx * dx) + (dz * dz) > distanceSq)
					continue;

				AddCell(data, cellX, cellZ, newlyDiscovered);
			}
		}

		CommitNewCells(player, data, newlyDiscovered);
		return newlyDiscovered.Count();
	}

	bool RevealEntireMap(PlayerBase player)
	{
		if (!g_Game.IsServer() || !player)
			return false;

		if (!m_Initialized)
			Init();

		if (!m_Config.Enabled || m_ConfigLoadFailed)
			return false;

		ZenMapFogPlayerData data = GetPlayerData(player);
		if (!data)
			return false;

		if (data.RevealAll)
			return true;

		data.RevealAll = 1;
		data.DiscoveredCells.Clear();

		string dataKey = BuildPlayerDataKey(data.WorldName, data.PlayerUID);
		set<int> cellSet;
		if (m_PlayerCellSets.Find(dataKey, cellSet) && cellSet)
			cellSet.Clear();

		MarkDatabaseDirty();

		PlayerIdentity identity = player.GetIdentity();
		if (identity)
			ZenMapFogNetwork.Get().SendRevealAll(player, identity);

		return true;
	}

	protected void LoadConfig()
	{
		m_Config = new ZenMapFogConfig;
		m_ConfigLoadFailed = false;
		MakeDirectory(ZenMapFogConstants.PROFILE_ROOT);
		if (!FileExist(ZenMapFogConstants.CONFIG_FILE))
			return;

		string error;
		if (!JsonFileLoader<ZenMapFogConfig>.LoadFile(ZenMapFogConstants.CONFIG_FILE, m_Config, error))
		{
			m_Config = new ZenMapFogConfig;
			m_Config.Enabled = 0;
			m_ConfigLoadFailed = true;
			Print("[ZenMapFog] ERROR: Invalid config; fog disabled for this session. Config and database left untouched. " + error);
		}
	}

	protected void ValidateConfig()
	{
		if (m_Config.CellSizeMeters < 10.0)
			m_Config.CellSizeMeters = 10.0;

		if (m_Config.GridCellSizeMeters < 10.0)
			m_Config.GridCellSizeMeters = 10.0;

		m_Config.TrackingIntervalSeconds = Math.Clamp(m_Config.TrackingIntervalSeconds, 0.1, 3600.0);
		m_Config.RevealRadiusCells = Math.Clamp(m_Config.RevealRadiusCells, 0, ZenMapFogConstants.MAX_REVEAL_RADIUS_CELLS);
		ValidateHeightRevealConfig();
		if (m_Config.MapSizeOverrideMeters < 0.0)
			m_Config.MapSizeOverrideMeters = 0.0;

		if (m_Config.RevealRadiusCells < 0)
			m_Config.RevealRadiusCells = 0;

		if (m_Config.BoundaryRevealTolerancePercent < 0.0)
			m_Config.BoundaryRevealTolerancePercent = 0.0;
		else if (m_Config.BoundaryRevealTolerancePercent > 49.9)
			m_Config.BoundaryRevealTolerancePercent = 49.9;

		if (m_Config.FogAlpha < 0)
			m_Config.FogAlpha = 0;
		else if (m_Config.FogAlpha > 255)
			m_Config.FogAlpha = 255;

		if (m_Config.FogRed < 0)
			m_Config.FogRed = 0;
		else if (m_Config.FogRed > 255)
			m_Config.FogRed = 255;

		if (m_Config.FogGreen < 0)
			m_Config.FogGreen = 0;
		else if (m_Config.FogGreen > 255)
			m_Config.FogGreen = 255;

		if (m_Config.FogBlue < 0)
			m_Config.FogBlue = 0;
		else if (m_Config.FogBlue > 255)
			m_Config.FogBlue = 255;

		if (m_Config.AdjacentCellFogAlpha < 0)
			m_Config.AdjacentCellFogAlpha = 0;
		else if (m_Config.AdjacentCellFogAlpha > 255)
			m_Config.AdjacentCellFogAlpha = 255;

		if (m_Config.RoundedFogEdgeRadiusPercent < 0.0)
			m_Config.RoundedFogEdgeRadiusPercent = 0.0;
		else if (m_Config.RoundedFogEdgeRadiusPercent > 45.0)
			m_Config.RoundedFogEdgeRadiusPercent = 45.0;

		if (m_Config.TileOverlapPixels < 0.0)
			m_Config.TileOverlapPixels = 0.0;
		else if (m_Config.TileOverlapPixels > 4.0)
			m_Config.TileOverlapPixels = 4.0;

		if (m_Config.GridLineWidthPixels < 0.0)
			m_Config.GridLineWidthPixels = 0.0;
		else if (m_Config.GridLineWidthPixels > 5.0)
			m_Config.GridLineWidthPixels = 5.0;

		if (m_Config.GridLineAlpha < 0)
			m_Config.GridLineAlpha = 0;
		else if (m_Config.GridLineAlpha > 255)
			m_Config.GridLineAlpha = 255;

		if (m_Config.GridLineRed < 0)
			m_Config.GridLineRed = 0;
		else if (m_Config.GridLineRed > 255)
			m_Config.GridLineRed = 255;

		if (m_Config.GridLineGreen < 0)
			m_Config.GridLineGreen = 0;
		else if (m_Config.GridLineGreen > 255)
			m_Config.GridLineGreen = 255;

		if (m_Config.GridLineBlue < 0)
			m_Config.GridLineBlue = 0;
		else if (m_Config.GridLineBlue > 255)
			m_Config.GridLineBlue = 255;
	}

	protected void ValidateHeightRevealConfig()
	{
		m_Config.PlayerHeightToIgnoreReveal = Math.Max(0.0, m_Config.PlayerHeightToIgnoreReveal);
		if (!m_Config.PlayerHeightMetersToTriggerReveal)
			m_Config.PlayerHeightMetersToTriggerReveal = new map<string, float>;

		for (int i = m_Config.PlayerHeightMetersToTriggerReveal.Count() - 1; i >= 0; i--)
		{
			string heightKey = m_Config.PlayerHeightMetersToTriggerReveal.GetKey(i);
			if (!IsValidHeightThreshold(heightKey))
			{
				Print("[ZenMapFog] WARNING: Ignoring invalid height reveal threshold: " + heightKey);
				m_Config.PlayerHeightMetersToTriggerReveal.Remove(heightKey);
				continue;
			}

			float distance = m_Config.PlayerHeightMetersToTriggerReveal.GetElement(i);
			// -1 explicitly selects the existing full-map reveal. Zero retains its
			// existing radius behavior, which can reveal the occupied cell.
			if (distance != -1.0)
				m_Config.PlayerHeightMetersToTriggerReveal.Set(heightKey, Math.Clamp(distance, 0.0, m_Config.CellSizeMeters * ZenMapFogConstants.MAX_REVEAL_RADIUS_CELLS));
		}
	}

	protected bool IsValidHeightThreshold(string heightKey)
	{
		if (heightKey.ToFloat() <= 0.0)
			return false;

		string digits = "0123456789";
		bool hasDecimalPoint;
		for (int i = 0; i < heightKey.Length(); i++)
		{
			string character = heightKey.Substring(i, 1);
			if (character == "." && !hasDecimalPoint)
				hasDecimalPoint = true;
			else if (!digits.Contains(character))
				return false;
		}
		return true;
	}

	protected void SaveConfig()
	{
		MakeDirectory(ZenMapFogConstants.PROFILE_ROOT);
		string error;
		if (!JsonFileLoader<ZenMapFogConfig>.SaveFile(ZenMapFogConstants.CONFIG_FILE, m_Config, error))
			Print("[ZenMapFog] ERROR: Could not save config. " + error);
	}

	protected float ResolveMapSize()
	{
		if (m_Config.MapSizeOverrideMeters > 0.0)
			return m_Config.MapSizeOverrideMeters;

		string worldPath = string.Format("CfgWorlds %1", m_WorldName);
		float mapSize = g_Game.ConfigGetFloat(worldPath + " mapSize");
		if (mapSize > 0.0)
			return mapSize;

		vector centerPosition = g_Game.ConfigGetVector(worldPath + " centerPosition");
		mapSize = Math.Max(centerPosition[0] - m_Config.MapOriginX, centerPosition[2] - m_Config.MapOriginZ) * 2.0;
		return mapSize;
	}

	protected void BuildDatabasePath()
	{
		if (m_Config.ResetDiscoveryOnStorageWipe)
		{
			int instanceId = g_Game.ServerConfigGetInt("instanceId");
			if (instanceId <= 0)
				instanceId = 1;

			string storageRoot = string.Format("$mission:storage_%1", instanceId);
			MakeDirectory(storageRoot);
			string databaseDirectory = storageRoot + "\\zenarchist";
			MakeDirectory(databaseDirectory);
			m_DatabasePath = databaseDirectory + "\\mapfogdb.json";
			return;
		}

		MakeDirectory(ZenMapFogConstants.PROFILE_DB_DIRECTORY);
		m_DatabasePath = ZenMapFogConstants.PROFILE_DB_FILE;
	}

	protected void LoadDatabase()
	{
		m_Database = new ZenMapFogDatabase;
		bool existed = FileExist(m_DatabasePath);
		bool reset = false;
		string error;
		if (existed && !JsonFileLoader<ZenMapFogDatabase>.LoadFile(m_DatabasePath, m_Database, error))
		{
			Print("[ZenMapFog] WARNING: Resetting unreadable database. " + error);
			reset = true;
		}

		if (!m_Database || m_Database.Version != ZenMapFogConstants.DATABASE_VERSION || m_Database.CellSizeMeters != m_Config.CellSizeMeters || m_Database.MapOriginX != m_Config.MapOriginX || m_Database.MapOriginZ != m_Config.MapOriginZ)
			reset = true;

		if (reset)
		{
			if (existed)
				Print("[ZenMapFog] WARNING: Resetting incompatible map fog database (legacy/version/cell size/map origin): " + m_DatabasePath);
			m_Database = new ZenMapFogDatabase;
		}

		m_Database.Version = ZenMapFogConstants.DATABASE_VERSION;
		m_Database.CellSizeMeters = m_Config.CellSizeMeters;
		m_Database.MapOriginX = m_Config.MapOriginX;
		m_Database.MapOriginZ = m_Config.MapOriginZ;
		if (!m_Database.Players)
			m_Database.Players = new array<ref ZenMapFogPlayerData>;

		m_PlayerDataIndex.Clear();
		m_PlayerCellSets.Clear();
		m_LastRevealState.Clear();
		m_PlayerHeightRevealState.Clear();
		ref array<ref ZenMapFogPlayerData> cleanPlayers = new array<ref ZenMapFogPlayerData>;
		bool cleaned = false;
		foreach (ZenMapFogPlayerData data : m_Database.Players)
		{
			if (!data || data.PlayerUID == "" || data.WorldName == "")
			{
				cleaned = true;
				continue;
			}

			string dataKey = BuildPlayerDataKey(data.WorldName, data.PlayerUID);
			ZenMapFogPlayerData existingData;
			if (m_PlayerDataIndex.Find(dataKey, existingData))
			{
				if (data.RevealAll)
					existingData.RevealAll = 1;
				if (data.DiscoveredCells)
				{
					foreach (int duplicateCell : data.DiscoveredCells)
						existingData.DiscoveredCells.Insert(duplicateCell);
				}
				cleaned = true;
				continue;
			}
			if (!data.DiscoveredCells)
			{
				data.DiscoveredCells = new array<int>;
				cleaned = true;
			}
			m_PlayerDataIndex.Insert(dataKey, data);
			cleanPlayers.Insert(data);
		}

		foreach (ZenMapFogPlayerData cleanData : cleanPlayers)
		{
			ref set<int> seen = new set<int>;
			ref array<int> cells = new array<int>;
			foreach (int cellId : cleanData.DiscoveredCells)
			{
				if (cleanData.RevealAll || cellId < 0 || cellId > ZenMapFogConstants.CELL_ID_MAX || seen.Find(cellId) != -1)
				{
					cleaned = true;
					continue;
				}
				seen.Insert(cellId);
				cells.Insert(cellId);
			}
			cleanData.DiscoveredCells = cells;
		}
		m_Database.Players = cleanPlayers;

		if (!existed || reset || cleaned)
		{
			m_DatabaseDirty = true;
			FlushDatabase();
			// A failed rewrite must not leave incompatible cells available for a later load.
			if (reset && m_DatabaseDirty && FileExist(m_DatabasePath))
			{
				if (!DeleteFile(m_DatabasePath))
					Print("[ZenMapFog] ERROR: Cannot remove incompatible database; check filesystem permissions: " + m_DatabasePath);
			}
		}
	}

	protected void TrackPlayers()
	{
		if (!m_Config.Enabled)
			return;

		ref array<Man> players = new array<Man>;
		g_Game.GetPlayers(players);

		foreach (Man man : players)
		{
			PlayerBase player = PlayerBase.Cast(man);
			if (!player || !player.GetIdentity() || !player.IsAlive())
				continue;

			RevealPositionIfChanged(player, player.GetPosition());
			CheckPlayerHeightReveal(player);
		}
	}

	protected void RevealPositionIfChanged(PlayerBase player, vector position)
	{
		PlayerIdentity identity = player.GetIdentity();
		if (!identity)
			return;

		float cellSize = m_Config.CellSizeMeters;
		int centerCellX = Math.Floor((position[0] - m_Config.MapOriginX) / cellSize);
		int centerCellZ = Math.Floor((position[2] - m_Config.MapOriginZ) / cellSize);
		if (!IsCellCoordinateSupported(centerCellX, centerCellZ))
			return;

		float toleranceDistance = cellSize * (m_Config.BoundaryRevealTolerancePercent / 100.0);
		float localX = (position[0] - m_Config.MapOriginX) - (centerCellX * cellSize);
		float localZ = (position[2] - m_Config.MapOriginZ) - (centerCellZ * cellSize);
		int boundaryFlags = 0;

		if (toleranceDistance > 0.0)
		{
			if (localX <= toleranceDistance)
				boundaryFlags = boundaryFlags | 1;

			if ((cellSize - localX) <= toleranceDistance)
				boundaryFlags = boundaryFlags | 2;

			if (localZ <= toleranceDistance)
				boundaryFlags = boundaryFlags | 4;

			if ((cellSize - localZ) <= toleranceDistance)
				boundaryFlags = boundaryFlags | 8;
		}

		string uid = GetIdentityUID(identity);
		if (uid == "")
			return;

		string state = string.Format("%1_%2_%3", centerCellX, centerCellZ, boundaryFlags);
		string previousState;
		if (m_LastRevealState.Find(uid, previousState) && previousState == state)
			return;

		m_LastRevealState.Set(uid, state);
		RevealPosition(player, centerCellX, centerCellZ, boundaryFlags);
	}

	protected void CheckPlayerHeightReveal(PlayerBase player)
	{
		if (!player)
			return;

		if (!m_Config.PlayerHeightMetersToTriggerReveal || m_Config.PlayerHeightMetersToTriggerReveal.Count() == 0 || m_Config.PlayerHeightToIgnoreReveal <= 0.0)
			return;

		PlayerIdentity heightIdentity = player.GetIdentity();
		if (!heightIdentity)
			return;

		string heightUID = GetIdentityUID(heightIdentity);
		if (heightUID == "")
			return;

		vector heightPosition = player.GetPosition();

		float surfaceY = g_Game.SurfaceY(heightPosition[0], heightPosition[2]);
		float heightAboveSurface = heightPosition[1] - surfaceY;

		float revealDistance;
		if (!TryRevealAtHeight(player, heightUID, heightAboveSurface, revealDistance))
			return;

		Log(string.Format("Height reveal triggered for %1. PlayerY=%2 SurfaceY=%3 Height=%4 RevealDistance=%5", heightUID, heightPosition[1], surfaceY, heightAboveSurface, revealDistance));
	}

	protected bool TryRevealAtHeight(PlayerBase player, string uid, float heightAboveSurface, out float revealDistance)
	{
		if (!player || !m_Config.Enabled || m_ConfigLoadFailed)
			return false;

		if (!ShouldRevealAtHeight(uid, heightAboveSurface, revealDistance))
			return false;

		if (revealDistance == -1.0)
			return RevealEntireMap(player);

		RevealRadius(player, revealDistance);
		return true;
	}

	protected bool ShouldRevealAtHeight(string uid, float heightAboveSurface, out float revealDistance)
	{
		revealDistance = 0.0;
		float previousHeight = 0.0;
		m_PlayerHeightRevealState.Find(uid, previousHeight);
		m_PlayerHeightRevealState.Set(uid, heightAboveSurface);

		// Record ignored heights too: returning down from a sky area must not
		// trigger tiers crossed while above the cutoff.
		if (m_Config.PlayerHeightToIgnoreReveal <= 0.0 || heightAboveSurface >= m_Config.PlayerHeightToIgnoreReveal)
			return false;
		if (!m_Config.PlayerHeightMetersToTriggerReveal)
			return false;

		bool triggered;
		foreach (string heightKey, float distance : m_Config.PlayerHeightMetersToTriggerReveal)
		{
			float threshold = heightKey.ToFloat();
			if (threshold <= 0.0 || heightAboveSurface <= threshold || previousHeight > threshold)
				continue;

			triggered = true;
			// Several tiers can be crossed in one tracking tick. Their circles
			// share a center, so reveal once using the largest newly crossed bonus.
			if (distance == -1.0)
				revealDistance = -1.0;
			else if (revealDistance != -1.0)
				revealDistance = Math.Max(revealDistance, distance);
		}
		return triggered;
	}

	protected void RevealPosition(PlayerBase player, int centerCellX, int centerCellZ, int boundaryFlags)
	{
		ZenMapFogPlayerData data = GetPlayerData(player);
		if (!data || data.RevealAll)
			return;

		ref array<int> newlyDiscovered = new array<int>;
		RevealCellArea(data, centerCellX, centerCellZ, newlyDiscovered);

		bool nearWest = (boundaryFlags & 1) != 0;
		bool nearEast = (boundaryFlags & 2) != 0;
		bool nearSouth = (boundaryFlags & 4) != 0;
		bool nearNorth = (boundaryFlags & 8) != 0;

		if (nearWest)
			RevealCellArea(data, centerCellX - 1, centerCellZ, newlyDiscovered);

		if (nearEast)
			RevealCellArea(data, centerCellX + 1, centerCellZ, newlyDiscovered);

		if (nearSouth)
			RevealCellArea(data, centerCellX, centerCellZ - 1, newlyDiscovered);

		if (nearNorth)
			RevealCellArea(data, centerCellX, centerCellZ + 1, newlyDiscovered);

		if (nearWest && nearSouth)
			RevealCellArea(data, centerCellX - 1, centerCellZ - 1, newlyDiscovered);

		if (nearWest && nearNorth)
			RevealCellArea(data, centerCellX - 1, centerCellZ + 1, newlyDiscovered);

		if (nearEast && nearSouth)
			RevealCellArea(data, centerCellX + 1, centerCellZ - 1, newlyDiscovered);

		if (nearEast && nearNorth)
			RevealCellArea(data, centerCellX + 1, centerCellZ + 1, newlyDiscovered);

		CommitNewCells(player, data, newlyDiscovered);
	}

	protected void RevealCellArea(ZenMapFogPlayerData data, int centerCellX, int centerCellZ, array<int> newlyDiscovered)
	{
		if (!data || !newlyDiscovered)
			return;

		for (int x = centerCellX - m_Config.RevealRadiusCells; x <= centerCellX + m_Config.RevealRadiusCells; x++)
		{
			for (int z = centerCellZ - m_Config.RevealRadiusCells; z <= centerCellZ + m_Config.RevealRadiusCells; z++)
				AddCell(data, x, z, newlyDiscovered);
		}
	}

	protected bool AddCell(ZenMapFogPlayerData data, int cellX, int cellZ, array<int> newlyDiscovered)
	{
		if (!data || !newlyDiscovered || !IsCellCoordinateSupported(cellX, cellZ))
			return false;

		int cellId = BuildCellId(cellX, cellZ);
		set<int> cellSet = GetPlayerCellSet(data);
		if (!cellSet || cellSet.Find(cellId) != -1)
			return false;

		cellSet.Insert(cellId);
		data.DiscoveredCells.Insert(cellId);
		newlyDiscovered.Insert(cellId);
		return true;
	}

	protected void CommitNewCells(PlayerBase player, ZenMapFogPlayerData data, array<int> newlyDiscovered)
	{
		if (!player || !data || !newlyDiscovered || newlyDiscovered.Count() == 0)
			return;

		MarkDatabaseDirty();

		PlayerIdentity identity = player.GetIdentity();
		if (!identity)
			return;

		int index = 0;
		while (index < newlyDiscovered.Count())
		{
			ref array<int> batch = new array<int>;
			int end = Math.Min(index + ZenMapFogConstants.SYNC_BATCH_SIZE, newlyDiscovered.Count());

			for (int i = index; i < end; i++)
				batch.Insert(newlyDiscovered[i]);

			ZenMapFogNetwork.Get().SendCellsRevealed(player, identity, batch);
			index = end;
		}
	}

	protected ZenMapFogPlayerData GetPlayerData(PlayerBase player)
	{
		PlayerIdentity identity = player.GetIdentity();
		if (!identity)
			return null;

		string uid = GetIdentityUID(identity);
		if (uid == "")
			return null;

		string dataKey = BuildPlayerDataKey(m_WorldName, uid);
		ZenMapFogPlayerData data;
		if (m_PlayerDataIndex.Find(dataKey, data))
			return data;

		data = new ZenMapFogPlayerData;
		data.PlayerUID = uid;
		data.WorldName = m_WorldName;
		m_Database.Players.Insert(data);
		m_PlayerDataIndex.Insert(dataKey, data);
		MarkDatabaseDirty();
		return data;
	}

	protected set<int> GetPlayerCellSet(ZenMapFogPlayerData data)
	{
		if (!data)
			return null;

		string dataKey = BuildPlayerDataKey(data.WorldName, data.PlayerUID);
		set<int> cellSet;
		if (m_PlayerCellSets.Find(dataKey, cellSet))
			return cellSet;

		cellSet = new set<int>;
		foreach (int cellId : data.DiscoveredCells)
			cellSet.Insert(cellId);

		m_PlayerCellSets.Insert(dataKey, cellSet);
		return cellSet;
	}

	protected void MarkDatabaseDirty()
	{
		m_DatabaseDirty = true;

		if (m_SaveScheduled)
			return;

		m_SaveScheduled = true;
		g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(FlushDatabase, ZenMapFogConstants.DATABASE_SAVE_DELAY_MS, false);
	}

	void FlushDatabase()
	{
		g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(FlushDatabase);
		m_SaveScheduled = false;

		if (!m_DatabaseDirty || !m_Database || m_DatabasePath == "")
			return;

		string error;
		if (!JsonFileLoader<ZenMapFogDatabase>.SaveFile(m_DatabasePath, m_Database, error))
		{
			Print("[ZenMapFog] ERROR: Database save failed; retaining dirty state. " + error);
			if (m_Initialized)
				MarkDatabaseDirty();
			return;
		}
		m_DatabaseDirty = false;
		Log(string.Format("Saved map fog database: %1 player/world records", m_Database.Players.Count()));
	}

	protected bool IsCellCoordinateSupported(int cellX, int cellZ)
	{
		return cellX >= ZenMapFogConstants.CELL_COORD_MIN && cellX <= ZenMapFogConstants.CELL_COORD_MAX && cellZ >= ZenMapFogConstants.CELL_COORD_MIN && cellZ <= ZenMapFogConstants.CELL_COORD_MAX;
	}

	protected int BuildCellId(int cellX, int cellZ)
	{
		int encodedX = cellX + ZenMapFogConstants.CELL_COORD_BIAS;
		int encodedZ = cellZ + ZenMapFogConstants.CELL_COORD_BIAS;
		return (encodedZ * ZenMapFogConstants.CELL_COORD_STRIDE) + encodedX;
	}

	protected string BuildPlayerDataKey(string worldName, string uid)
	{
		return worldName + "|" + uid;
	}

	protected string GetIdentityUID(PlayerIdentity identity)
	{
		string uid = identity.GetPlainId();
		if (uid == "")
			uid = identity.GetId();

		return uid;
	}

	protected void Log(string message)
	{
		if (!m_Config.DebugLogging)
			return;

		Print("[ZenMapFog] " + message);
	}
};
