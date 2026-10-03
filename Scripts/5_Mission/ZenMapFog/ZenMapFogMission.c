modded class MissionServer
{
	override void OnClientDisconnectedEvent(PlayerIdentity identity, PlayerBase player, int logoutTime, bool authFailed)
	{
		ZenMapFogServerManager.Get().ForgetPlayerSession(identity);
		super.OnClientDisconnectedEvent(identity, player, logoutTime, authFailed);
	}

	override void OnInit()
	{
		super.OnInit();
		ZenMapFogNetwork.Get().Register();
		ZenMapFogServerManager.Get().Init();
	}

	override void OnMissionFinish()
	{
		ZenMapFogServerManager.Get().Shutdown();
		super.OnMissionFinish();
	}
};

modded class MissionGameplay
{
	override void OnMissionFinish()
	{
		ZenMapFogClientData.ResetSession();
		super.OnMissionFinish();
	}
	override void OnInit()
	{
		super.OnInit();
		ZenMapFogClientData.ResetSession();
		ZenMapFogNetwork.Get().Register();
	}
};
