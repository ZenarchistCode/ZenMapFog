class CfgPatches
{
	class ZenMapFog
	{
		units[] = {"ZenMapFogRevealer"};
		weapons[] = {};
		requiredVersion = 0.1;
		requiredAddons[] =
		{
			"DZ_Data",
			"DZ_Scripts",
			"DZ_Gear_Navigation",
			"JM_CF_Scripts"
		};
	};
};

class CfgMods
{
	class ZenMapFog
	{
		dir = "ZenMapFog";
		name = "ZenMapFog";
		author = "Zenarchist";
		type = "mod";
		dependencies[] =
		{
			"Game",
			"World",
			"Mission"
		};

		class defs
		{
			class gameScriptModule
			{
				value = "";
				files[] =
				{
					"ZenMapFog/Scripts/3_Game"
				};
			};

			class worldScriptModule
			{
				value = "";
				files[] =
				{
					"ZenMapFog/Scripts/4_World"
				};
			};

			class missionScriptModule
			{
				value = "";
				files[] =
				{
					"ZenMapFog/Scripts/5_Mission"
				};
			};
		};
	};
};

class CfgVehicles
{
	class Inventory_Base;
	class ZenMapFogRevealer: Inventory_Base
	{
		scope = 2;
		displayName = "$STR_CfgVehicles_ZenMapFogRevealer0";
		descriptionShort = "$STR_CfgVehicles_ZenMapFogRevealer1";
		model = "\dz\gear\navigation\Map_chernarus_animated.p3d";
		rotationFlags = 17;
		itemSize[] = {1, 2};
		weight = 40;
		absorbency = 1;
		hiddenSelections[] = {"texture_map_closed", "texture_map_opened", "texture_legend"};
		hiddenSelectionsTextures[] = {"dz\gear\navigation\data\map_chernarus_co.paa", "dz\structures\signs\tourist\data\karta_co.paa", "dz\structures\signs\tourist\data\karta_side_co.paa"};

		class DamageSystem
		{
			class GlobalHealth
			{
				class Health
				{
					hitpoints = 50;
					healthLevels[] =
					{
						{1.0, {"DZ\gear\navigation\data\Map_chernarus.rvmat"}},
						{0.7, {"DZ\gear\navigation\data\Map_chernarus.rvmat"}},
						{0.5, {"DZ\gear\navigation\data\Map_chernarus_damage.rvmat"}},
						{0.3, {"DZ\gear\navigation\data\Map_chernarus_damage.rvmat"}},
						{0.0, {"DZ\gear\navigation\data\Map_chernarus_destruct.rvmat"}}
					};
				};
			};
		};

		class AnimationSources
		{
			class map_closed
			{
				source = "user";
				animPeriod = 0.1;
				initPhase = 0;
			};
			class map_opened
			{
				source = "user";
				animPeriod = 0.1;
				initPhase = 1;
			};
		};
	};
};
