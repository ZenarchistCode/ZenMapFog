class ZenMapFogConfig
{
	int Enabled = 1;
	float CellSizeMeters = 500.0;
	bool ResetMapFogOnDeath = false;
	ref map<string, float> PlayerHeightMetersToTriggerReveal = new map<string, float>;
	float PlayerHeightToIgnoreReveal = 1000.0;
	int RevealRadiusCells = 0;
	float BoundaryRevealTolerancePercent = 20.0;
	float TrackingIntervalSeconds = 1.0;
	int ResetDiscoveryOnStorageWipe = 0;
	float MapOriginX = 0.0;
	float MapOriginZ = 0.0;
	float MapSizeOverrideMeters = 0.0;
	int FogAlpha = 255;
	int FogRed = 0;
	int FogGreen = 0;
	int FogBlue = 0;
	int AdjacentCellFogEnabled = 1;
	int AdjacentCellFogAlpha = 254;
	int RoundedFogEdgesEnabled = 1;
	int RoundedFogEdgesBoundaryEnabled = 1;
	float RoundedFogEdgeRadiusPercent = 18.0;
	float TileOverlapPixels = 1.0;
	int GridLinesEnabled = 1;
	float GridCellSizeMeters = 1000.0;
	float GridLineWidthPixels = 1.0;
	int GridLineAlpha = 160;
	int GridLineRed = 140;
	int GridLineGreen = 140;
	int GridLineBlue = 140;
	int DebugLogging = 0;

	void ZenMapFogConfig()
	{
		PlayerHeightMetersToTriggerReveal.Insert("20", 1000.0);
	}
};

class ZenMapFogConstants
{
	static const string MOD_NAME = "ZenMapFog";
	static const string PROFILE_ROOT = "$profile:Zenarchist";
	static const string CONFIG_FILE = "$profile:Zenarchist\\ZenMapFogConfig.json";
	static const string PROFILE_DB_DIRECTORY = "$profile:Zenarchist\\MapFogDB";
	static const string PROFILE_DB_FILE = "$profile:Zenarchist\\MapFogDB\\MapFogDB.json";
	static const int DATABASE_VERSION = 1;
	static const int MAX_REVEAL_RADIUS_CELLS = 64;
	static const int SYNC_RETRY_MS = 10000;
	static const int CELL_ID_MAX = 1073741823;
	static const int SYNC_BATCH_SIZE = 512;
	static const int DATABASE_SAVE_DELAY_MS = 30000;
	static const int CELL_COORD_MIN = -16384;
	static const int CELL_COORD_MAX = 16383;
	static const int CELL_COORD_BIAS = 16384;
	static const int CELL_COORD_STRIDE = 32768;
	static const float CLIENT_RENDER_INTERVAL_SECONDS = 0.016;
};
