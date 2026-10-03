class ZenMapFogNetwork
{
	protected static ref ZenMapFogNetwork s_Instance;
	protected bool m_Registered;

	static ZenMapFogNetwork Get()
	{
		if (!s_Instance)
			s_Instance = new ZenMapFogNetwork;

		return s_Instance;
	}

	void Register()
	{
		if (m_Registered)
			return;

		m_Registered = true;
		GetRPCManager().AddRPC(ZenMapFogConstants.MOD_NAME, "RPC_RequestSync", this, SingleplayerExecutionType.Server);
		GetRPCManager().AddRPC(ZenMapFogConstants.MOD_NAME, "RPC_SyncBegin", this, SingleplayerExecutionType.Client);
		GetRPCManager().AddRPC(ZenMapFogConstants.MOD_NAME, "RPC_SyncGridSettings", this, SingleplayerExecutionType.Client);
		GetRPCManager().AddRPC(ZenMapFogConstants.MOD_NAME, "RPC_SyncRoundedFogSettings", this, SingleplayerExecutionType.Client);
		GetRPCManager().AddRPC(ZenMapFogConstants.MOD_NAME, "RPC_SyncRevealAll", this, SingleplayerExecutionType.Client);
		GetRPCManager().AddRPC(ZenMapFogConstants.MOD_NAME, "RPC_SyncBatch", this, SingleplayerExecutionType.Client);
		GetRPCManager().AddRPC(ZenMapFogConstants.MOD_NAME, "RPC_SyncEnd", this, SingleplayerExecutionType.Client);
		GetRPCManager().AddRPC(ZenMapFogConstants.MOD_NAME, "RPC_CellsRevealed", this, SingleplayerExecutionType.Client);
		GetRPCManager().AddRPC(ZenMapFogConstants.MOD_NAME, "RPC_RevealAll", this, SingleplayerExecutionType.Client);
	}

	void RequestSync(PlayerBase player)
	{
		if (!g_Game.IsClient() || !player)
			return;

		GetRPCManager().SendRPC(ZenMapFogConstants.MOD_NAME, "RPC_RequestSync", new Param1<int>(1), true, null, player);
	}

	void SendSyncBegin(PlayerBase player, PlayerIdentity identity, ZenMapFogConfig config, float mapSizeMeters)
	{
		if (!g_Game.IsServer() || !player || !identity || !config)
			return;

		int fogColor = ARGB(config.FogAlpha, config.FogRed, config.FogGreen, config.FogBlue);
		Param7<int, float, float, float, float, int, float> data = new Param7<int, float, float, float, float, int, float>(config.Enabled, config.CellSizeMeters, config.MapOriginX, config.MapOriginZ, mapSizeMeters, fogColor, config.TileOverlapPixels);
		GetRPCManager().SendRPC(ZenMapFogConstants.MOD_NAME, "RPC_SyncBegin", data, true, identity, player);
	}

	void SendSyncGridSettings(PlayerBase player, PlayerIdentity identity, ZenMapFogConfig config)
	{
		if (!g_Game.IsServer() || !player || !identity || !config)
			return;

		int gridColor = ARGB(config.GridLineAlpha, config.GridLineRed, config.GridLineGreen, config.GridLineBlue);
		int adjacentCellFogColor = ARGB(config.AdjacentCellFogAlpha, config.FogRed, config.FogGreen, config.FogBlue);
		Param6<int, int, float, float, int, int> data = new Param6<int, int, float, float, int, int>(config.GridLinesEnabled, gridColor, config.GridLineWidthPixels, config.GridCellSizeMeters, config.AdjacentCellFogEnabled, adjacentCellFogColor);
		GetRPCManager().SendRPC(ZenMapFogConstants.MOD_NAME, "RPC_SyncGridSettings", data, true, identity, player);
	}

	void SendSyncRoundedFogSettings(PlayerBase player, PlayerIdentity identity, ZenMapFogConfig config)
	{
		if (!g_Game.IsServer() || !player || !identity || !config)
			return;

		Param3<int, int, float> data = new Param3<int, int, float>(config.RoundedFogEdgesEnabled, config.RoundedFogEdgesBoundaryEnabled, config.RoundedFogEdgeRadiusPercent);
		GetRPCManager().SendRPC(ZenMapFogConstants.MOD_NAME, "RPC_SyncRoundedFogSettings", data, true, identity, player);
	}

	void SendSyncRevealAll(PlayerBase player, PlayerIdentity identity)
	{
		if (!g_Game.IsServer() || !player || !identity)
			return;

		GetRPCManager().SendRPC(ZenMapFogConstants.MOD_NAME, "RPC_SyncRevealAll", new Param1<int>(1), true, identity, player);
	}

	void SendSyncBatch(PlayerBase player, PlayerIdentity identity, array<int> cellIds)
	{
		if (!g_Game.IsServer() || !player || !identity || !cellIds)
			return;

		GetRPCManager().SendRPC(ZenMapFogConstants.MOD_NAME, "RPC_SyncBatch", new Param1<ref array<int>>(cellIds), true, identity, player);
	}

	void SendSyncEnd(PlayerBase player, PlayerIdentity identity, int totalCells)
	{
		if (!g_Game.IsServer() || !player || !identity)
			return;

		GetRPCManager().SendRPC(ZenMapFogConstants.MOD_NAME, "RPC_SyncEnd", new Param1<int>(totalCells), true, identity, player);
	}

	void SendCellsRevealed(PlayerBase player, PlayerIdentity identity, array<int> cellIds)
	{
		if (!g_Game.IsServer() || !player || !identity || !cellIds || cellIds.Count() == 0)
			return;

		GetRPCManager().SendRPC(ZenMapFogConstants.MOD_NAME, "RPC_CellsRevealed", new Param1<ref array<int>>(cellIds), true, identity, player);
	}

	void SendRevealAll(PlayerBase player, PlayerIdentity identity)
	{
		if (!g_Game.IsServer() || !player || !identity)
			return;

		GetRPCManager().SendRPC(ZenMapFogConstants.MOD_NAME, "RPC_RevealAll", new Param1<int>(1), true, identity, player);
	}

	void RPC_RequestSync(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		if (type != CallType.Server || !sender)
			return;

		Param1<int> request;
		if (!ctx.Read(request))
			return;

		PlayerBase player = PlayerBase.Cast(target);
		if (!player || !player.GetIdentity())
			return;

		if (player.GetIdentity().GetId() != sender.GetId())
			return;

		ZenMapFogServerManager.Get().SyncPlayerMapFog(player, sender);
	}

	void RPC_SyncBegin(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		if (type != CallType.Client)
			return;

		Param7<int, float, float, float, float, int, float> data;
		if (!ctx.Read(data))
			return;

		ZenMapFogClientData.Get().BeginSync(data.param1, data.param2, data.param3, data.param4, data.param5, data.param6, data.param7);
	}

	void RPC_SyncGridSettings(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		if (type != CallType.Client)
			return;

		Param6<int, int, float, float, int, int> data;
		if (!ctx.Read(data))
			return;

		ZenMapFogClientData.Get().SetGridSettings(data.param1, data.param2, data.param3, data.param4, data.param5, data.param6);
	}

	void RPC_SyncRoundedFogSettings(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		if (type != CallType.Client)
			return;

		Param3<int, int, float> data;
		if (!ctx.Read(data))
			return;

		ZenMapFogClientData.Get().SetRoundedFogSettings(data.param1, data.param2, data.param3);
	}

	void RPC_SyncRevealAll(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		if (type != CallType.Client)
			return;

		Param1<int> data;
		if (!ctx.Read(data))
			return;

		ZenMapFogClientData.Get().SetAllRevealed();
	}

	void RPC_SyncBatch(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		if (type != CallType.Client)
			return;

		Param1<ref array<int>> data;
		if (!ctx.Read(data))
			return;

		ZenMapFogClientData.Get().AddSyncBatch(data.param1);
	}

	void RPC_SyncEnd(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		if (type != CallType.Client)
			return;

		Param1<int> data;
		if (!ctx.Read(data))
			return;

		ZenMapFogClientData.Get().CompleteSync(data.param1);
	}

	void RPC_CellsRevealed(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		if (type != CallType.Client)
			return;

		Param1<ref array<int>> data;
		if (!ctx.Read(data))
			return;

		ZenMapFogClientData.Get().AddSyncBatch(data.param1);
	}

	void RPC_RevealAll(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		if (type != CallType.Client)
			return;

		Param1<int> data;
		if (!ctx.Read(data))
			return;

		ZenMapFogClientData.Get().SetAllRevealed();
	}
};
