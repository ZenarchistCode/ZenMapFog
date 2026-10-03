#ifdef ZenModCore
modded class ZenAdminCommandHandler
{
	override bool HandleAdminCommand(PlayerBase player, string uid, string cmd, array<string> params)
	{
		if (super.HandleAdminCommand(player, uid, cmd, params))
			return true;

		if (cmd == "revealmap")
		{
			if (ZenMapFogServerManager.Get().RevealEntireMap(player))
				SendMsg(player, "Revealed the entire map.");
			else
				SendMsg(player, "Map fog reveal failed: feature disabled or player unavailable.");
			return true;
		}

		if (cmd == "mapfogheight")
		{
			vector heightPosition = player.GetPosition();
			float surfaceY = g_Game.SurfaceY(heightPosition[0], heightPosition[2]);
			float heightAboveSurface = heightPosition[1] - surfaceY;

			SendMsg(player, "Height above surface: " + heightAboveSurface.ToString());
			return true;
		}

		return false;
	}
};
#endif
