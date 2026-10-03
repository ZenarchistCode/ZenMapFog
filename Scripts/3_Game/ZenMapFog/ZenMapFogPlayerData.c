class ZenMapFogPlayerData
{
	string PlayerUID;
	string WorldName;
	int RevealAll = 0;
	ref array<int> DiscoveredCells;

	void ZenMapFogPlayerData()
	{
		DiscoveredCells = new array<int>;
	}
};

class ZenMapFogDatabase
{
	// Zero identifies legacy files whose cell geometry cannot be verified.
	int Version = 0;
	float CellSizeMeters = 0.0;
	float MapOriginX = 0.0;
	float MapOriginZ = 0.0;
	ref array<ref ZenMapFogPlayerData> Players;

	void ZenMapFogDatabase()
	{
		Players = new array<ref ZenMapFogPlayerData>;
	}
};
