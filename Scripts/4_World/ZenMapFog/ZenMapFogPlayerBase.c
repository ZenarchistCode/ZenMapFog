modded class PlayerBase
{
	void RevealZenMapFog(float distance)
	{
		if (!g_Game.IsServer())
			return;

		if (distance < 0.0)
			distance = 0.0;

		ZenMapFogServerManager.Get().RevealRadius(this, distance);
	}

	override void EEKilled(Object killer)
	{
		super.EEKilled(killer);

		ZenMapFogServerManager.Get().ResetPlayerFogOnDeath(this);
	}
};
