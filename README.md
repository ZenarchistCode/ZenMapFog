# ZenMapFog

Fog of war for DayZ maps. Players uncover the map as they explore, with progress saved individually for each player and terrain.

## Features

- Supports the vanilla map and DayZ Expansion Navigation map.
- Configurable exploration size, fog colours, opacity, rounded edges and grid lines.
- Tower bonuses: reveal larger areas at configured heights, or reveal the entire map.
- Height cutoff to prevent bonus reveals in modded sky areas.
- Optional discovery resets on death or mission-storage wipes.

## Setup

1. Install **CF (Community Framework)** and **ZenMapFog** on the server and clients. Load both with `-mod`, for example `-mod=@CF;@ZenMapFog`.
2. Start the server once to generate `Zenarchist/ZenMapFogConfig.json` inside your server's **profiles folder**.
3. Stop the server, edit the JSON, then restart to apply changes.

Expansion Navigation support activates when installed. **ZenModCore is optional** and enables the admin commands below.

## Configuration

Defaults below match the current source. Toggles use **1 = on / 0 = off**; `ResetMapFogOnDeath` uses `true` / `false`. Colour and opacity values use **0?255**, with opacity 0 transparent and 255 opaque.

### General and exploration

| Setting | Default | Description |
|---|---|---|
| `Enabled` | `1` | Enables map fog and exploration. |
| `CellSizeMeters` | `500.0` | Width of each exploration square in metres. Changing this resets saved exploration. |
| `RevealRadiusCells` | `0` | Extra squares revealed around the player: 0 = current square; 1 = 3 ? 3 area. |
| `BoundaryRevealTolerancePercent` | `20.0` | Reveals neighbouring squares near an edge; 0 disables. Range: 0?49.9%. |
| `TrackingIntervalSeconds` | `1.0` | Seconds between player-position checks. Minimum: 0.1. |
| `ResetMapFogOnDeath` | `false` | Clears a player's discoveries, including full-map reveals, when they die. |
| `ResetDiscoveryOnStorageWipe` | `0` | 0 = profiles storage; 1 = mission storage, so a storage wipe also wipes exploration. |
| `DebugLogging` | `0` | Logs extra information for troubleshooting. |

### Height bonuses

| Setting | Default | Description |
|---|---|---|
| `PlayerHeightMetersToTriggerReveal` | `{"20": 1000.0}` | Height-to-reveal-distance map. Use `{}` to disable height bonuses. |
| `PlayerHeightToIgnoreReveal` | `1000.0` | Blocks height bonuses at or above this height. Setting 0 disables all height bonuses. |

Replace these entries in your generated config to add more tower rewards:

```json
{
    "PlayerHeightMetersToTriggerReveal": {
        "20": 1000.0,
        "60": 10000.0,
        "100": -1.0
    },
    "PlayerHeightToIgnoreReveal": 1000.0
}
```

Heights are measured **above the terrain**, not sea level. This example rewards climbing **above** 20m with a 1km radius, above 60m with a 10km radius, and above 100m with **the entire map** (`-1`). Positive radii are capped at 64 ? `CellSizeMeters`.

Each tier triggers once per ascent and rearms after descending to or below it. Crossing several tiers at once grants the largest bonus. At **1000m or higher**, no height bonus is granted; returning downward from a sky area does not grant missed bonuses. Ordinary exploration still works.

### Fog appearance

| Setting | Default | Description |
|---|---|---|
| `FogAlpha` | `255` | Opacity of unexplored areas. |
| `FogRed`, `FogGreen`, `FogBlue` | `0` each | Fog colour; all zero gives black. |
| `AdjacentCellFogEnabled` | `1` | Uses separate opacity for cells bordering explored areas. |
| `AdjacentCellFogAlpha` | `254` | Opacity of those bordering cells. |
| `RoundedFogEdgesEnabled` | `1` | Rounds the edges around explored areas. |
| `RoundedFogEdgesBoundaryEnabled` | `1` | Rounds the outer adjacent-fog boundary; requires adjacent-cell fog. |
| `RoundedFogEdgeRadiusPercent` | `18.0` | Corner radius as a percentage of cell size. Range: 0?45. |
| `TileOverlapPixels` | `1.0` | Overlap to hide seams between fog tiles. Range: 0?4. |

### Grid and custom maps

| Setting | Default | Description |
|---|---|---|
| `GridLinesEnabled` | `1` | Displays the custom grid while fog is visible. |
| `GridCellSizeMeters` | `1000.0` | Grid spacing, independent of exploration-square size. |
| `GridLineWidthPixels` | `1.0` | Grid thickness. Range: 0?5; 0 hides lines. |
| `GridLineAlpha` | `160` | Grid opacity. |
| `GridLineRed`, `GridLineGreen`, `GridLineBlue` | `140` each | Grid colour. |
| `MapOriginX`, `MapOriginZ` | `0.0` each | Exploration-grid origin. Changing either resets saved exploration. |
| `MapSizeOverrideMeters` | `0.0` | Terrain width override; 0 uses automatic detection. |

## Saved exploration

- **Profiles mode (`ResetDiscoveryOnStorageWipe = 0`):** `Zenarchist/MapFogDB/MapFogDB.json` inside the profiles folder.
- **Mission-storage mode (`1`):** `storage_<instanceId>/zenarchist/mapfogdb.json` inside the mission folder.

Switching storage modes selects a separate database; progress is not merged. Changing the effective exploration cell size or either grid origin resets that database. A map-size override can also reset it if the effective cell size changes. Appearance, height-tier and cutoff changes preserve compatible discoveries.

## Admin commands

With **ZenModCore** installed and your account configured as an admin, use its chat-command prefix (default `!`):

- `!revealmap` ? reveals the entire map for yourself.
- `!mapfogheight` ? shows your height above terrain, useful for choosing tower thresholds.
