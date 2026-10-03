class ZenMapFogClientData
{
	protected static ref ZenMapFogClientData s_Instance;

	protected ref set<int> m_DiscoveredCells;
	protected ref set<int> m_AdjacentCells;
	protected bool m_Ready;
	protected bool m_RequestPending;
	protected int m_LastRequestTime;
	protected bool m_Enabled = true;
	protected bool m_AllRevealed;
	protected float m_CellSizeMeters = 1000.0;
	protected float m_MapOriginX;
	protected float m_MapOriginZ;
	protected float m_MapSizeMeters;
	protected int m_FogColor = ARGB(255, 0, 0, 0);
	protected bool m_AdjacentCellFogEnabled = true;
	protected int m_AdjacentCellFogColor = ARGB(230, 0, 0, 0);
	protected bool m_RoundedFogEdgesEnabled = true;
	protected bool m_RoundedFogEdgesBoundaryEnabled = true;
	protected float m_RoundedFogEdgeRadiusPercent = 18.0;
	protected float m_TileOverlapPixels = 1.0;
	protected bool m_GridLinesEnabled = true;
	protected float m_GridCellSizeMeters = 1000.0;
	protected int m_GridLineColor = ARGB(160, 140, 140, 140);
	protected float m_GridLineWidthPixels = 1.0;
	protected int m_Revision;

	void ZenMapFogClientData()
	{
		m_DiscoveredCells = new set<int>;
		m_AdjacentCells = new set<int>;
	}

	static ZenMapFogClientData Get()
	{
		if (!s_Instance)
			s_Instance = new ZenMapFogClientData;

		return s_Instance;
	}

	static void ResetSession()
	{
		s_Instance = null;
	}

	void RequestSyncIfNeeded()
	{
		if (!g_Game.IsClient())
			return;

		if (m_Ready)
			return;

		int now = g_Game.GetTime();
		if (m_RequestPending && now - m_LastRequestTime < ZenMapFogConstants.SYNC_RETRY_MS)
			return;

		PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
		if (!player)
			return;

		m_RequestPending = true;
		m_LastRequestTime = now;
		ZenMapFogNetwork.Get().RequestSync(player);
	}

	void BeginSync(int enabled, float cellSizeMeters, float mapOriginX, float mapOriginZ, float mapSizeMeters, int fogColor, float tileOverlapPixels)
	{
		m_DiscoveredCells.Clear();
		m_AdjacentCells.Clear();
		m_Enabled = enabled != 0;
		m_AllRevealed = false;
		m_CellSizeMeters = cellSizeMeters;
		m_MapOriginX = mapOriginX;
		m_MapOriginZ = mapOriginZ;
		m_MapSizeMeters = mapSizeMeters;
		m_FogColor = fogColor;
		m_TileOverlapPixels = tileOverlapPixels;
		m_Ready = false;
		m_RequestPending = true;
		m_LastRequestTime = g_Game.GetTime();
		m_Revision++;
	}

	void SetGridSettings(int enabled, int color, float widthPixels, float gridCellSizeMeters, int adjacentCellFogEnabled, int adjacentCellFogColor)
	{
		m_GridLinesEnabled = enabled != 0;
		m_GridLineColor = color;
		m_GridLineWidthPixels = widthPixels;
		m_GridCellSizeMeters = gridCellSizeMeters;
		m_AdjacentCellFogEnabled = adjacentCellFogEnabled != 0;
		m_AdjacentCellFogColor = adjacentCellFogColor;
		m_AdjacentCells.Clear();

		if (m_AdjacentCellFogEnabled)
		{
			foreach (int cellId : m_DiscoveredCells)
				AddAdjacentCells(cellId);
		}

		m_Revision++;
	}

	void SetRoundedFogSettings(int enabled, int boundaryEnabled, float radiusPercent)
	{
		m_RoundedFogEdgesEnabled = enabled != 0;
		m_RoundedFogEdgesBoundaryEnabled = boundaryEnabled != 0;
		m_RoundedFogEdgeRadiusPercent = radiusPercent;
		m_Revision++;
	}

	void SetAllRevealed()
	{
		m_AllRevealed = true;
		m_DiscoveredCells.Clear();
		m_AdjacentCells.Clear();
		m_Revision++;
	}

	void AddSyncBatch(array<int> cellIds)
	{
		if (!cellIds || m_AllRevealed)
			return;

		foreach (int cellId : cellIds)
		{
			if (cellId < 0 || cellId > ZenMapFogConstants.CELL_ID_MAX || m_DiscoveredCells.Find(cellId) != -1)
				continue;

			m_DiscoveredCells.Insert(cellId);

			if (m_AdjacentCellFogEnabled)
				AddAdjacentCells(cellId);
		}

		m_Revision++;
	}

	void CompleteSync(int expectedCells = -1)
	{
		if (expectedCells >= 0 && m_DiscoveredCells.Count() != expectedCells)
		{
			m_Ready = false;
			m_RequestPending = false;
			m_Revision++;
			return;
		}
		m_Ready = true;
		m_RequestPending = false;
		m_Revision++;
	}

	bool IsReady()
	{
		return m_Ready;
	}

	bool IsEnabled()
	{
		return m_Enabled;
	}

	bool IsAllRevealed()
	{
		return m_AllRevealed;
	}

	bool HasCell(int cellId)
	{
		if (m_AllRevealed)
			return true;

		return m_DiscoveredCells.Find(cellId) != -1;
	}

	int BuildCellId(int cellX, int cellZ)
	{
		int encodedX = cellX + ZenMapFogConstants.CELL_COORD_BIAS;
		int encodedZ = cellZ + ZenMapFogConstants.CELL_COORD_BIAS;
		return (encodedZ * ZenMapFogConstants.CELL_COORD_STRIDE) + encodedX;
	}

	bool IsCellCoordinateSupported(int cellX, int cellZ)
	{
		return cellX >= ZenMapFogConstants.CELL_COORD_MIN && cellX <= ZenMapFogConstants.CELL_COORD_MAX && cellZ >= ZenMapFogConstants.CELL_COORD_MIN && cellZ <= ZenMapFogConstants.CELL_COORD_MAX;
	}

	protected int DecodeCellX(int cellId)
	{
		int encodedZ = cellId / ZenMapFogConstants.CELL_COORD_STRIDE;
		int encodedX = cellId - (encodedZ * ZenMapFogConstants.CELL_COORD_STRIDE);
		return encodedX - ZenMapFogConstants.CELL_COORD_BIAS;
	}

	protected int DecodeCellZ(int cellId)
	{
		int encodedZ = cellId / ZenMapFogConstants.CELL_COORD_STRIDE;
		return encodedZ - ZenMapFogConstants.CELL_COORD_BIAS;
	}

	int GetCellColumns()
	{
		if (m_CellSizeMeters <= 0.0 || m_MapSizeMeters <= 0.0)
			return 1;

		return Math.Ceil(m_MapSizeMeters / m_CellSizeMeters);
	}

	int GetCellRows()
	{
		return GetCellColumns();
	}

	float GetCellSizeMeters()
	{
		return m_CellSizeMeters;
	}

	float GetMapOriginX()
	{
		return m_MapOriginX;
	}

	float GetMapOriginZ()
	{
		return m_MapOriginZ;
	}

	float GetMapSizeMeters()
	{
		return m_MapSizeMeters;
	}

	int GetFogColor()
	{
		return m_FogColor;
	}

	bool GetAdjacentCellFogEnabled()
	{
		return m_AdjacentCellFogEnabled;
	}

	int GetAdjacentCellFogColor()
	{
		return m_AdjacentCellFogColor;
	}

	bool GetRoundedFogEdgesEnabled()
	{
		return m_RoundedFogEdgesEnabled;
	}

	bool GetRoundedFogEdgesBoundaryEnabled()
	{
		return m_RoundedFogEdgesBoundaryEnabled;
	}

	float GetRoundedFogEdgeRadiusPercent()
	{
		return m_RoundedFogEdgeRadiusPercent;
	}

	bool IsAdjacentCell(int cellId)
	{
		if (!m_AdjacentCellFogEnabled || m_AllRevealed)
			return false;

		return m_AdjacentCells.Find(cellId) != -1;
	}

	protected void AddAdjacentCells(int cellId)
	{
		int cellX = DecodeCellX(cellId);
		int cellZ = DecodeCellZ(cellId);

		for (int zOffset = -1; zOffset <= 1; zOffset++)
		{
			for (int xOffset = -1; xOffset <= 1; xOffset++)
			{
				if (xOffset == 0 && zOffset == 0)
					continue;

				int adjacentX = cellX + xOffset;
				int adjacentZ = cellZ + zOffset;
				if (!IsCellCoordinateSupported(adjacentX, adjacentZ))
					continue;

				int adjacentCellId = BuildCellId(adjacentX, adjacentZ);
				m_AdjacentCells.Insert(adjacentCellId);
			}
		}
	}

	float GetTileOverlapPixels()
	{
		return m_TileOverlapPixels;
	}

	bool GetGridLinesEnabled()
	{
		return m_GridLinesEnabled;
	}

	float GetGridCellSizeMeters()
	{
		return m_GridCellSizeMeters;
	}

	int GetGridLineColor()
	{
		return m_GridLineColor;
	}

	float GetGridLineWidthPixels()
	{
		return m_GridLineWidthPixels;
	}

	int GetRevision()
	{
		return m_Revision;
	}
};
