// settings.c - Tunable game settings table (see settings.h)
#include "settings.h"
#include "../game_state.h"
#include "../core/time.h"
#include "../simulation/water.h"
#include "../simulation/fire.h"
#include "../simulation/smoke.h"
#include "../simulation/steam.h"
#include "../simulation/temperature.h"
#include "../simulation/balance.h"
#include "../simulation/weather.h"
#include "../simulation/lighting.h"
#include "../simulation/mood.h"
#include "../entities/mover.h"
#include "../entities/animals.h"
#include "../entities/items.h"
#include "../entities/trains.h"
#include "../entities/cars.h"
#include "../entities/belts.h"
#include "../entities/furniture.h"   // fridgeSpoilageFactor
#include "../entities/inventory.h"   // carryLimitKg
#include "../entities/elevators.h"
#include "../entities/ladders.h"
#include "../entities/tool_quality.h"
#include "../world/pathfinding.h"
#include "../world/biome.h"
#include "../simulation/trees.h"
#include "../simulation/groundwear.h"
#include "../ui/console.h"
#include "../simulation/tide.h"
#include <strings.h>

// ---------------------------------------------------------------------------
// Live tooltips: settings whose tooltip shows derived values (game-hours, tiles/s)
// ---------------------------------------------------------------------------

// Most intervals are in game hours (GH); show what that is in real seconds at the current
// day length. Movement is scaled to game hours too (speed is px/s at dayLength 60).
static float RealSeconds(float gameHours) { return gameHours * dayLength / 24.0f; }
static float BaseTilesPerGameHour(void) { return balance.baseMoverSpeed / (float)CELL_SIZE * 2.5f; }
static float BaseTilesPerRealSecond(void) { return balance.baseMoverSpeed / (float)CELL_SIZE * (60.0f / dayLength); }

static const char* DescWaterEvap(void) {
    return TextFormat("Puddles evaporate every %.1f game hours (%.0f real s at this day length).",
                      waterEvapInterval, RealSeconds(waterEvapInterval));
}
static const char* DescSpeedShallow(void) {
    return TextFormat("Mover speed in shallow water: %.0f%% (%.0f tiles per game hour).",
                      waterSpeedShallow * 100.0f, BaseTilesPerGameHour() * waterSpeedShallow);
}
static const char* DescSpeedMedium(void) {
    return TextFormat("Mover speed in medium water: %.0f%% (%.0f tiles per game hour).",
                      waterSpeedMedium * 100.0f, BaseTilesPerGameHour() * waterSpeedMedium);
}
static const char* DescSpeedDeep(void) {
    return TextFormat("Mover speed in deep water: %.0f%% (%.0f tiles per game hour).",
                      waterSpeedDeep * 100.0f, BaseTilesPerGameHour() * waterSpeedDeep);
}
static const char* DescSpeedMud(void) {
    return TextFormat("Mover speed on mud: %.0f%% (%.0f tiles per game hour).",
                      mudSpeedMultiplier * 100.0f, BaseTilesPerGameHour() * mudSpeedMultiplier);
}
static const char* DescFireSpread(void) {
    return TextFormat("Fire tries to spread every %.2f game hours (%.0f attempts per game hour).",
                      fireSpreadInterval, 1.0f / fireSpreadInterval);
}
static const char* DescFireFuel(void) {
    float woodBurnGH = 10.0f * fireFuelInterval;   // wood fuel ~10
    return TextFormat("Fuel consumed every %.2f game hours.\nWood wall burns for ~%.1f game hours.",
                      fireFuelInterval, woodBurnGH);
}
static const char* DescFireWater(void) {
    return TextFormat("Fire spreads %.1fx slower near water (%d%% chance reduction).",
                      100.0f / (float)fireWaterReduction, fireWaterReduction);
}
static const char* DescSmokeDissipation(void) {
    return TextFormat("Smoke clears one level per %.1f game hours.", smokeDissipationTime);
}
static const char* DescSmokeGeneration(void) {
    return TextFormat("Smoke per tick = fire level / %d. A level-6 fire produces %d smoke.",
                      smokeGenerationRate, 6 / smokeGenerationRate);
}
static const char* DescDepthDecay(void) {
    return TextFormat("%dC per Z-level underground. z=-10 = %dC.",
                      ambientDepthDecay, ambientSurfaceTemp - ambientDepthDecay * 10);
}
static const char* DescWoodInsulation(void) {
    return TextFormat("Wood passes %d%% of heat (blocks %d%%).", insulationTier1Rate, 100 - insulationTier1Rate);
}
static const char* DescStoneInsulation(void) {
    return TextFormat("Stone passes %d%% of heat (blocks %d%%).", insulationTier2Rate, 100 - insulationTier2Rate);
}

static const char* DescWeatherMin(void) {
    return TextFormat("Shortest weather spell: %.0f game hours (%.1f days).",
                      weatherMinDuration, weatherMinDuration / 24.0f);
}
static const char* DescWeatherMax(void) {
    return TextFormat("Longest weather spell: %.0f game hours (%.1f days).",
                      weatherMaxDuration, weatherMaxDuration / 24.0f);
}
static const char* DescSnowAccumulation(void) {
    return TextFormat("Snow builds at %.2f levels/gh. Full cover (7) in ~%.0f game-hours.",
                      snowAccumulationRate, snowAccumulationRate > 0 ? 7.0f / snowAccumulationRate : 999.0f);
}
static const char* DescSnowMelting(void) {
    return TextFormat("Snow melts at %.2f levels/gh. Full melt in ~%.0f game-hours.",
                      snowMeltingRate, snowMeltingRate > 0 ? 7.0f / snowMeltingRate : 999.0f);
}
static const char* DescGameSpeed(void) {
    return TextFormat("Simulation speed multiplier. At %.1fx a game hour takes %.1f real seconds.%s",
                      gameSpeed, gameSpeed > 0 ? RealSeconds(1.0f) / gameSpeed : 0.0f, gameSpeed == 0.0f ? " (PAUSED)" : "");
}
static const char* DescDayLength(void) {
    float realDuration = gameSpeed > 0 ? dayLength / gameSpeed : 0;
    return TextFormat("Real seconds per game day at 1x (a game hour = %.0f s).\nAt 1x speed: %.1f real-%s per day.\nAt current %.1fx: %.1f real-%s per day.",
                      RealSeconds(1.0f),
                      dayLength < 60 ? dayLength : dayLength / 60.0f, dayLength < 60 ? "seconds" : "minutes",
                      gameSpeed, realDuration < 60 ? realDuration : realDuration / 60.0f,
                      realDuration < 60 ? "seconds" : "minutes");
}
static const char* DescDaysPerSeason(void) {
    return TextFormat("Days per season. Year = %d days (4 seasons). Current: %s (day %d of season).",
                      daysPerSeason * SEASON_COUNT, GetSeasonName(GetCurrentSeason()), (GetYearDay() % daysPerSeason) + 1);
}
static const char* DescBaseTemp(void) {
    return TextFormat("Base surface temperature before seasonal modulation. Current seasonal temp: %dC.",
                      GetSeasonalSurfaceTemp());
}
static const char* DescSeasonalAmplitude(void) {
    return TextFormat("Temperature swing above/below base. 0=flat. Range: %dC to %dC.",
                      baseSurfaceTemp - seasonalAmplitude, baseSurfaceTemp + seasonalAmplitude);
}
static const char* DescBaseSpeed(void) {
    return TextFormat("Base mover speed (px/s at day length 60). = %.0f tiles per game hour;\non screen now %.1f tiles/s.",
                      BaseTilesPerGameHour(), BaseTilesPerRealSecond());
}
static const char* DescSpeedVariance(void) {
    return TextFormat("Random speed spread per mover: +/-%.0f%%. Range: %.0f - %.0f tiles per game hour.",
                      balance.moverSpeedVariance * 100.0f,
                      BaseTilesPerGameHour() * (1.0f - balance.moverSpeedVariance),
                      BaseTilesPerGameHour() * (1.0f + balance.moverSpeedVariance));
}

// ---------------------------------------------------------------------------
// Change hooks
// ---------------------------------------------------------------------------

// Balance inputs feed derived per-game-hour rates
static void RecalcBalance(float oldValue) { (void)oldValue; RecalcBalanceTable(); }

// Lighting toggles: recompute the light grid
static void RelightAll(float oldValue) { (void)oldValue; InvalidateLighting(); }

// Path stats are per algorithm
static void PathAlgoChanged(float oldValue) { (void)oldValue; ResetPathStats(); }

// Direction choice drives the pathfinder's 8-direction flag
static void PathDirChanged(float oldValue) { (void)oldValue; use8Dir = (currentDirection == 1); }

static const char* DescAnimalPop(void) {
    return TextFormat("Target: %d  Current: %d", animalTargetPopulation, CountActiveAnimals());
}
static const char* DescAnimalSpawn(void) {
    return TextFormat("A spawn attempt every %.0f game hours (%.1f per day).", animalSpawnIntervalGH,
                      animalSpawnIntervalGH > 0 ? 24.0f / animalSpawnIntervalGH : 0.0f);
}
static const char* DescTorchIntensity(void) {
    return TextFormat("Propagation radius for new torches. At %d, light reaches %d tiles away.",
                      lightDefaultIntensity, lightDefaultIntensity);
}

// Picking a biome loads its river/lake counts (still editable after)
static void ApplyBiomePreset(float oldValue) {
    (void)oldValue;
    hillsWaterRiverCount = biomePresets[selectedBiome].riverCount;
    hillsWaterLakeCount = biomePresets[selectedBiome].lakeCount;
}

static const char* DescSaplingGrow(void) {
    return TextFormat("Sapling to young tree: %.0f game-hours (%.1f days)", saplingGrowGH, saplingGrowGH / 24.0f);
}
static const char* DescTrunkGrow(void) {
    return TextFormat("Per growth stage: %.0f game-hours (%.1f days)", trunkGrowGH, trunkGrowGH / 24.0f);
}
static const char* DescYoungToMature(void) {
    return TextFormat("Young tree maturation wait: %.0f game-hours (%.1f days)", youngToMatureGH, youngToMatureGH / 24.0f);
}
static const char* DescRegrowthChance(void) {
    return TextFormat("Chance per 10000 per interval for a sapling to spawn on tall grass. At %d, roughly %.2f%% chance.",
                      saplingRegrowthChance, saplingRegrowthChance / 100.0f);
}
static const char* DescMinTreeDistance(void) {
    return TextFormat("Minimum tiles from existing trees/saplings for a new sapling. At %d, trees spread more slowly.",
                      saplingMinTreeDistance);
}
static const char* DescTrample(void) {
    int stepsToPath = wearTrampleAmount > 0 ? wearGrassToDirt / wearTrampleAmount : 9999;
    return TextFormat("%d wear per step. %d steps to form a path.", wearTrampleAmount, stepsToPath);
}
static const char* DescGrassToDirt(void) {
    return TextFormat("Grass becomes dirt at %d wear.", wearGrassToDirt);
}
static const char* DescDirtToGrass(void) {
    return TextFormat("Dirt regrows grass below %d wear. Gap of %d creates path persistence.",
                      wearDirtToGrass, wearGrassToDirt - wearDirtToGrass);
}

// Rescale every mover's speed proportionally (each keeps its own variance)
static void RescaleMoverSpeeds(float oldBaseSpeed) {
    if (oldBaseSpeed <= 0) return;
    float scale = balance.baseMoverSpeed / oldBaseSpeed;
    for (int i = 0; i < moverCount; i++)
        if (movers[i].active) movers[i].speed *= scale;
}

// ---------------------------------------------------------------------------
// The table. Order within a group = order in the sidebar.
// Console names kept from before the table existed: water, fire, smoke, steam,
// temp, fireSpread, fireFuel, surfTemp, weather, speed, dayLen, fixedTime,
// daysPerSzn, baseTemp, sznAmp.
// ---------------------------------------------------------------------------

#define SET_BOOL(name, label, group, var, help) \
    {name, label, group, SETTING_BOOL, &(var), 0, 0, 0, help, NULL, NULL, NULL, 0}
#define SET_BOOL_HOOK(name, label, group, var, help, hook) \
    {name, label, group, SETTING_BOOL, &(var), 0, 0, 0, help, NULL, hook, NULL, 0}
#define SET_INT(name, label, group, var, speed, lo, hi, help, desc) \
    {name, label, group, SETTING_INT, &(var), speed, lo, hi, help, desc, NULL, NULL, 0}
#define SET_FLOAT(name, label, group, var, speed, lo, hi, help, desc) \
    {name, label, group, SETTING_FLOAT, &(var), speed, lo, hi, help, desc, NULL, NULL, 0}
#define SET_FLOAT_HOOK(name, label, group, var, speed, lo, hi, help, desc, hook) \
    {name, label, group, SETTING_FLOAT, &(var), speed, lo, hi, help, desc, hook, NULL, 0}
#define SET_CHOICE(name, label, group, var, names, count, help, hook) \
    {name, label, group, SETTING_CHOICE, &(var), 0, 0, (count) - 1, help, NULL, hook, names, count}
// Mover balance input: changing it recalculates the derived rates
#define SET_BALANCE(name, label, group, field, speed, lo, hi, help) \
    SET_FLOAT_HOOK(name, label, group, balance.field, speed, lo, hi, help, NULL, RecalcBalance)

// Stairs effort is cached in the pathfinder's graph: rebuild it
static void RebuildGraphForStairs(float oldValue) {
    (void)oldValue;
    BuildEntrances();
    BuildGraph();
}

static const Setting settings[] = {
    // --- Water ---
    SET_BOOL("water", "Enabled", "Simulation/Water", waterEnabled,
      "Master toggle for water simulation. Water flows down, spreads horizontally, and uses pressure to rise through U-bends."),
    SET_BOOL("waterEvaporation", "Evaporation", "Simulation/Water", waterEvaporationEnabled,
      "When enabled, shallow water (level 1) has a chance to evaporate each tick. Disable for testing water mechanics."),
    SET_BOOL("waterEdgeOutflow", "Edge Outflow", "Simulation/Water", waterEdgeOutflow,
      "Water at the map edge trades with what lies beyond it: above waterEdgeSeaZ it flows off the map, below it the edge fills up. Generated worlds turn it on."),
    SET_INT("waterEdgeSeaZ", "Sea Beyond Edge (z)", "Simulation/Water", waterEdgeSeaZ, 0.1f, -1, 63,
      "Height of the sea beyond the map edge (-1 = none: all edge water flows off). The island sets its sea level.", NULL),
    SET_FLOAT("waterFlowHalfLife", "Current half-life (ticks)", "Simulation/Water", waterFlowHalfLife, 10.0f, 10.0f, 3600.0f,
      "Water ticks for the remembered current (water flow overlay) to fade to half. Longer = steadier arrows, slower to show a change.", NULL),
    SET_BOOL("itemsDrift", "Items drift", "Simulation/Water", itemsDrift,
      "Loose things that float go with the current until they wash up: light plant matter (sticks, leaves, grass, reeds) in any water, wood (logs, planks) in water 2 deep. Carried off the map edge they are lost. Not when carried, stored or reserved for a job."),
    SET_FLOAT("waterCurrentPush", "Current push", "Simulation/Water", waterCurrentPush, 0.05f, 0.0f, 0.9f,
      "How much a current changes the speed of someone wading: at full strength walking with it is this much faster, against it this much slower (0 = off).", NULL),
    SET_BOOL("springTrickle", "Springs trickle", "Simulation/Water", springTrickle,
      "Source cells inside the map are springs that add Spring Rate water per game-second instead of refilling to full every tick (an endless tap). Sources on the map edge stay taps. The island turns it on."),
    SET_FLOAT("springRate", "Spring rate (/s)", "Simulation/Water", springRate, 0.1f, 0.0f, 60.0f,
      "Water units each spring adds per game-second (a full cell is 7) while it has room. Per game-second like the flow it feeds, so a day length doesn't change the rivers.", NULL),
    SET_BOOL("tide", "Tide", "Simulation/Water", tideEnabled,
      "The sea's top level rises and falls (experiment, docs/todo/world/tide.md). Needs a marked sea: the tide scenario."),
    SET_FLOAT("tidePeriod", "Tide period (h)", "Simulation/Water", tidePeriodHours, 0.5f, 1.0f, 48.0f,
      "Game hours from high tide to high tide (North Sea: 12.42).", NULL),
    SET_INT("tideLowLevel", "Tide low level", "Simulation/Water", tideLowLevel, 1.0f, 0, 7,
      "Water level of the sea's top level at low tide (0 = the flats fall dry, 3 = wading).", NULL),
    SET_BOOL("islandTide", "Island tide", "Simulation/Water", islandTide,
      "The island game starts with the tide on its sea: mud flats fall dry twice a day, ramps from the beach down to them (experiment)."),
    SET_BOOL("tideSurf", "Surf", "Simulation/Water", tideSurf,
      "Real swash on the mud flats: waves run up from deep water and drain back, never past wading depth (the tide experiment). Not saved."),
    SET_INT("tideSurfReach", "Surf reach (cells)", "Simulation/Water", tideSurfReach, 1.0f, 1, 32,
      "How far up the flats a wave runs, in cells from deep water. Set before the sea is marked. Not saved.", NULL),
    SET_FLOAT("tideSurfPeriod", "Surf period (h)", "Simulation/Water", tideSurfPeriod, 0.25f, 0.25f, 6.0f,
      "Game hours from one wave to the next (game-like: closer to the tide than real waves). Not saved.", NULL),
    SET_INT("tideSurfHeight", "Surf height", "Simulation/Water", tideSurfHeight, 1.0f, 1, 3,
      "Water level a wave brings near the sea (1 at its tip); capped below wading depth. Not saved.", NULL),
    SET_INT("tideBeachReach", "Beach surf reach", "Simulation/Water", tideBeachReach, 1.0f, 1, 16,
      "At high tide the waves run this many cells up the beach (one level above the sea). Set before the sea is marked. Not saved.", NULL),
    SET_FLOAT("tideWaveHours", "Tide wave (h)", "Simulation/Water", tideWaveHours, 0.25f, 0.0f, 12.0f,
      "Game hours the tide takes to sweep across the map: the water crosses wading depth strip by strip (0 = all at once). Not saved.", NULL),
    SET_FLOAT("tideWaveAngle", "Tide wave angle", "Simulation/Water", tideWaveAngle, 15.0f, 0.0f, 359.0f,
      "Direction the tide wave travels (0 = from the west going east, 90 = from the north). Set before the sea is marked. Not saved.", NULL),
    SET_FLOAT("waterEvapInterval", "Evap Interval (GH)", "Simulation/Water", waterEvapInterval, 0.5f, 0.5f, 120.0f,
      "Game hours between evaporation attempts for level-1 water.", DescWaterEvap),
    SET_FLOAT("waterSpeedShallow", "Speed: Shallow (1-2)", "Simulation/Water/Advanced", waterSpeedShallow, 0.05f, 0.1f, 1.0f,
      "Mover speed multiplier in level 1-2 water.", DescSpeedShallow),
    SET_FLOAT("waterSpeedMedium", "Speed: Medium (3-4)", "Simulation/Water/Advanced", waterSpeedMedium, 0.05f, 0.1f, 1.0f,
      "Mover speed multiplier in level 3-4 water.", DescSpeedMedium),
    SET_FLOAT("waterSpeedDeep", "Speed: Deep (5-7)", "Simulation/Water/Advanced", waterSpeedDeep, 0.05f, 0.1f, 1.0f,
      "Mover speed multiplier in level 5-7 water.", DescSpeedDeep),
    SET_FLOAT("mudSpeed", "Speed: Mud", "Simulation/Water/Advanced", mudSpeedMultiplier, 0.05f, 0.1f, 1.0f,
      "Mover speed multiplier on mud.", DescSpeedMud),
    SET_FLOAT("wetnessSync", "Wetness Sync", "Simulation/Water/Advanced", wetnessSyncInterval, 0.5f, 0.5f, 30.0f,
      "Seconds between soil absorbing water. Lower = ground gets wet faster.", NULL),

    // --- Fire ---
    SET_BOOL("fire", "Enabled", "Simulation/Fire", fireEnabled,
      "Master toggle for fire simulation. Fire consumes fuel, spreads to neighbors, and generates smoke."),
    SET_FLOAT("fireSpread", "Spread Interval (GH)", "Simulation/Fire", fireSpreadInterval, 0.01f, 0.01f, 2.0f,
      "Game hours between fire spread attempts.", DescFireSpread),
    SET_FLOAT("fireFuel", "Fuel Interval (GH)", "Simulation/Fire", fireFuelInterval, 0.01f, 0.01f, 2.0f,
      "Game hours between fuel consumption.", DescFireFuel),
    SET_INT("fireWaterReduction", "Water Reduction %", "Simulation/Fire", fireWaterReduction, 1.0f, 1, 100,
      "Spread chance near water, in percent of normal.", DescFireWater),
    SET_INT("fireSpreadBase", "Base Chance %", "Simulation/Fire/Advanced", fireSpreadBase, 1.0f, 0, 50,
      "Base spread chance before fire level bonus. Formula: base + (level * perLevel).", NULL),
    SET_INT("fireSpreadPerLevel", "Per Level %", "Simulation/Fire/Advanced", fireSpreadPerLevel, 1.0f, 0, 30,
      "Additional spread chance per fire level.", NULL),

    // --- Smoke ---
    SET_BOOL("smoke", "Enabled", "Simulation/Smoke", smokeEnabled,
      "Master toggle for smoke simulation. Smoke rises, spreads horizontally, fills enclosed spaces, and gradually dissipates."),
    SET_FLOAT("smokeRise", "Rise Interval (GH)", "Simulation/Smoke", smokeRiseInterval, 0.01f, 0.01f, 2.0f,
      "Game hours for smoke to rise one Z-level.", NULL),
    SET_FLOAT("smokeDissipation", "Dissipation (GH)", "Simulation/Smoke", smokeDissipationTime, 0.1f, 0.5f, 30.0f,
      "Game hours for smoke to clear one level.", DescSmokeDissipation),
    SET_INT("smokeGeneration", "Generation Rate", "Simulation/Smoke", smokeGenerationRate, 1.0f, 1, 10,
      "Smoke per tick = fire level / this.", DescSmokeGeneration),

    // --- Steam ---
    SET_BOOL("steam", "Enabled", "Simulation/Steam", steamEnabled,
      "Master toggle for steam simulation. Steam rises from boiling water, spreads, and condenses back to water when cooled."),
    SET_FLOAT("steamRise", "Rise Interval (GH)", "Simulation/Steam", steamRiseInterval, 0.01f, 0.01f, 2.0f,
      "Game hours for steam to rise one Z-level.", NULL),
    SET_INT("steamCondensationTemp", "Condensation Temp", "Simulation/Steam/Advanced", steamCondensationTemp, 5.0f, 0, 100,
      "Steam condenses to water below this temperature (C).", NULL),
    SET_INT("steamGenerationTemp", "Generation Temp", "Simulation/Steam/Advanced", steamGenerationTemp, 5.0f, 80, 150,
      "Water boils to steam above this temperature (C).", NULL),
    SET_INT("steamCondensationChance", "Condensation Chance", "Simulation/Steam/Advanced", steamCondensationChance, 1.0f, 1, 600,
      "1-in-N chance per tick to condense. Higher = steam lingers longer.", NULL),
    SET_INT("steamRiseFlow", "Rise Flow", "Simulation/Steam/Advanced", steamRiseFlow, 1.0f, 1, STEAM_MAX_LEVEL,
      "Steam units that rise per tick. Higher = faster vertical movement.", NULL),

    // --- Temperature ---
    SET_BOOL("temp", "Enabled", "Simulation/Temperature", temperatureEnabled,
      "Master toggle for temperature simulation. Heat transfers between cells, affected by insulation."),
    SET_INT("surfTemp", "Surface Ambient", "Simulation/Temperature", ambientSurfaceTemp, 1.0f, -50, 200,
      "Surface temperature in C (0 = freeze, 20 = room temp, 100 = boiling).", NULL),
    SET_INT("depthDecay", "Depth Decay", "Simulation/Temperature", ambientDepthDecay, 1.0f, 0, 20,
      "Temperature drop per Z-level underground (C).", DescDepthDecay),
    SET_FLOAT("heatTransferInterval", "Transfer Interval (GH)", "Simulation/Temperature", heatTransferInterval, 0.01f, 0.01f, 2.0f,
      "Game hours between heat spreading steps. Lower = faster heat flow.", NULL),
    SET_FLOAT("tempDecayInterval", "Decay Interval (GH)", "Simulation/Temperature", tempDecayInterval, 0.01f, 0.01f, 2.0f,
      "Game hours between decay-toward-ambient steps. Lower = faster cooling.", NULL),
    SET_INT("woodInsulation", "Wood Insulation %", "Simulation/Temperature/Advanced", insulationTier1Rate, 1.0f, 1, 100,
      "Percent of heat that passes through wood.", DescWoodInsulation),
    SET_INT("stoneInsulation", "Stone Insulation %", "Simulation/Temperature/Advanced", insulationTier2Rate, 1.0f, 1, 100,
      "Percent of heat that passes through stone.", DescStoneInsulation),
    SET_INT("heatSourceTemp", "Heat Source Temp", "Simulation/Temperature/Advanced", heatSourceTemp, 5.0f, 100, 1000,
      "Fire/furnace temperature (C).", NULL),
    SET_INT("coldSourceTemp", "Cold Source Temp", "Simulation/Temperature/Advanced", coldSourceTemp, 5.0f, -100, 0,
      "Ice/freezer temperature (C).", NULL),
    SET_INT("heatRiseBoost", "Heat Rise Boost %", "Simulation/Temperature/Advanced", heatRiseBoost, 5.0f, 50, 300,
      "Upward transfer rate in percent (hot air rises).", NULL),
    SET_INT("heatSinkReduction", "Heat Sink Reduction %", "Simulation/Temperature/Advanced", heatSinkReduction, 5.0f, 10, 100,
      "Downward transfer rate in percent. Lower = heat stays up longer.", NULL),
    SET_INT("heatDecayPercent", "Decay Rate %", "Simulation/Temperature/Advanced", heatDecayPercent, 1.0f, 1, 50,
      "Percent of excess heat lost per interval. Higher = faster cooling.", NULL),
    SET_INT("diagonalTransfer", "Diagonal Transfer %", "Simulation/Temperature/Advanced", diagonalTransferPercent, 5.0f, 30, 100,
      "Diagonal transfer rate in percent of orthogonal (~1.4x distance).", NULL),

    // --- Weather ---
    SET_BOOL("weather", "Weather Enabled", "Weather & Time/Weather", weatherEnabled,
      "Weather changes over time (clear, cloudy, rain, storms, snow)."),
    SET_FLOAT("weatherMinDuration", "Min Duration", "Weather & Time/Weather", weatherMinDuration, 1.0f, 5.0f, 300.0f,
      "Shortest weather spell (game hours).", DescWeatherMin),
    SET_FLOAT("weatherMaxDuration", "Max Duration", "Weather & Time/Weather", weatherMaxDuration, 1.0f, 10.0f, 600.0f,
      "Longest weather spell (game hours).", DescWeatherMax),
    SET_FLOAT("rainWetnessInterval", "Rain Wetness Interval", "Weather & Time/Weather", rainWetnessInterval, 0.5f, 0.5f, 30.0f,
      "Game hours between ground wetness increments during rain.", NULL),
    SET_FLOAT("heavyRainWetnessInterval", "Heavy Rain Interval", "Weather & Time/Weather", heavyRainWetnessInterval, 0.5f, 0.5f, 15.0f,
      "Game hours between ground wetness increments during heavy rain.", NULL),
    SET_FLOAT("lightningInterval", "Lightning Interval", "Weather & Time/Weather", lightningInterval, 1.0f, 0.5f, 30.0f,
      "Game hours between lightning strikes in thunderstorms.", NULL),
    SET_FLOAT("snowAccumulation", "Snow Accumulation", "Weather & Time/Weather", snowAccumulationRate, 0.01f, 0.01f, 1.0f,
      "Snow levels gained per game-hour while snowing.", DescSnowAccumulation),
    SET_FLOAT("snowMelting", "Snow Melting", "Weather & Time/Weather", snowMeltingRate, 0.01f, 0.01f, 0.5f,
      "Snow levels lost per game-hour above freezing.", DescSnowMelting),

    // --- Time ---
    SET_FLOAT("speed", "Game Speed", "Weather & Time/Time", gameSpeed, 0.1f, 0.0f, 100.0f,
      "Simulation speed multiplier (0 = paused).", DescGameSpeed),
    SET_FLOAT("dayLen", "Day Length", "Weather & Time/Time", dayLength, 10.0f, 10.0f, 3600.0f,
      "Real seconds per game day at 1x speed.", DescDayLength),
    SET_BOOL("fixedTime", "Fixed Timestep", "Weather & Time/Time", useFixedTimestep,
      "Fixed 60Hz ticks (deterministic). Off = variable timestep (smoother, less deterministic)."),

    // --- Seasons ---
    SET_INT("daysPerSzn", "Days per Season", "Weather & Time/Seasons", daysPerSeason, 1.0f, 1, 30,
      "Days per season (a year is 4 seasons).", DescDaysPerSeason),
    SET_INT("baseTemp", "Base Temp (C)", "Weather & Time/Seasons", baseSurfaceTemp, 1.0f, -20, 40,
      "Base surface temperature before seasonal modulation.", DescBaseTemp),
    SET_INT("sznAmp", "Seasonal Amplitude", "Weather & Time/Seasons", seasonalAmplitude, 1.0f, 0, 40,
      "Temperature swing above/below base over the year. 0 = flat.", DescSeasonalAmplitude),
    SET_INT("diurnalAmplitude", "Diurnal Amplitude", "Weather & Time/Seasons", diurnalAmplitude, 1.0f, 0, 15,
      "Day/night temperature swing. Peak at 14:00, trough at 02:00. 0 = flat.", NULL),

    // --- Mover balance: movement ---
    SET_FLOAT_HOOK("baseMoverSpeed", "Base Speed (px/s)", "Mover Balance", balance.baseMoverSpeed, 10.0f, 50.0f, 3000.0f,
      "Base mover speed in pixels/sec. Changing it rescales every mover's speed.", DescBaseSpeed, RescaleMoverSpeeds),
    SET_FLOAT("moverSpeedVariance", "Speed Variance", "Mover Balance", balance.moverSpeedVariance, 0.05f, 0.0f, 0.5f,
      "Random speed spread per new mover (+/- fraction).", DescSpeedVariance),
    SET_FLOAT("jobDistanceWeight", "Job Distance Weight", "Mover Balance", jobDistanceWeight, 0.01f, 0.0f, 2.0f,
      "How much distance counts against job priority, in priority levels per cell. Levels: stockpile "
      "maintenance 0, crafting 1, hunting 2, hauling 3, mining/building/gathering 4, farming 5 "
      "(clothing and water pots: no limit). One level is worth 1/weight cells: at 0.2 one level = 5 cells, "
      "maintenance vs farming = 25 cells, so a job one level lower wins if it is 5 cells closer. "
      "0 = strict priority order (nearest within one type). Dollhouse and survival start at 0.2.", NULL),

    // --- Mover balance: day budget (game-hours) ---
    SET_BALANCE("workHoursPerDay", "Work Hours/Day", "Mover Balance/Day Budget", workHoursPerDay, 0.5f, 1.0f, 24.0f,
      "Design target: how many hours per day a mover should work."),
    SET_BALANCE("sleepHoursInBed", "Sleep (Plank Bed)", "Mover Balance/Day Budget", sleepHoursInBed, 0.5f, 1.0f, 24.0f,
      "Game-hours to recover from exhausted to rested in a plank bed."),
    SET_BALANCE("sleepOnGround", "Sleep (Ground)", "Mover Balance/Day Budget", sleepOnGround, 0.5f, 1.0f, 48.0f,
      "Game-hours to recover from exhausted to rested on bare ground."),
    SET_BALANCE("hoursToStarve", "Hours to Starve", "Mover Balance/Day Budget", hoursToStarve, 0.5f, 1.0f, 48.0f,
      "Game-hours from full hunger to starvation."),
    SET_BALANCE("hoursToExhaustWorking", "Exhaust (Working)", "Mover Balance/Day Budget", hoursToExhaustWorking, 0.5f, 1.0f, 48.0f,
      "Game-hours of continuous work before exhaustion."),
    SET_BALANCE("hoursToExhaustIdle", "Exhaust (Idle)", "Mover Balance/Day Budget", hoursToExhaustIdle, 0.5f, 1.0f, 48.0f,
      "Game-hours of idling before exhaustion."),
    SET_BALANCE("eatingDuration", "Eating Duration", "Mover Balance/Day Budget", eatingDurationGH, 0.05f, 0.05f, 4.0f,
      "Game-hours spent eating a meal."),
    SET_BALANCE("hoursToDehydrate", "Hours to Dehydrate", "Mover Balance/Day Budget", hoursToDehydrate, 0.5f, 1.0f, 48.0f,
      "Game-hours from full thirst to dehydration."),
    SET_BALANCE("drinkingDuration", "Drinking Duration", "Mover Balance/Day Budget", drinkingDurationGH, 0.05f, 0.05f, 4.0f,
      "Game-hours spent drinking from a pot."),
    SET_BALANCE("naturalDrinkDuration", "Natural Drink Duration", "Mover Balance/Day Budget", naturalDrinkDurationGH, 0.05f, 0.05f, 4.0f,
      "Game-hours spent drinking from natural water."),
    SET_BALANCE("naturalDrinkHydration", "Natural Drink Hydration", "Mover Balance/Day Budget", naturalDrinkHydration, 0.05f, 0.05f, 1.0f,
      "Hydration restored by drinking natural water."),
    SET_BALANCE("dehydrationDeath", "Dehydration Death", "Mover Balance/Day Budget", dehydrationDeathGH, 0.5f, 1.0f, 24.0f,
      "Game-hours at zero thirst before death."),

    // --- Mover balance: thresholds (0-1) ---
    SET_BALANCE("hungerSeek", "Hunger Seek", "Mover Balance/Thresholds", hungerSeekThreshold, 0.05f, 0.05f, 0.9f,
      "Seek food when hunger drops below this (idle)."),
    SET_BALANCE("hungerCritical", "Hunger Critical", "Mover Balance/Thresholds", hungerCriticalThreshold, 0.01f, 0.01f, 0.5f,
      "Cancel current job to find food."),
    SET_BALANCE("energyTired", "Energy Tired", "Mover Balance/Thresholds", energyTiredThreshold, 0.05f, 0.05f, 0.9f,
      "Seek rest when energy drops below this (idle)."),
    SET_BALANCE("energyExhausted", "Energy Exhausted", "Mover Balance/Thresholds", energyExhaustedThreshold, 0.01f, 0.01f, 0.5f,
      "Cancel current job to find rest."),
    SET_BALANCE("thirstSeek", "Thirst Seek", "Mover Balance/Thresholds", thirstSeekThreshold, 0.05f, 0.05f, 0.9f,
      "Seek drink when thirst drops below this (idle)."),
    SET_BALANCE("thirstCritical", "Thirst Critical", "Mover Balance/Thresholds", thirstCriticalThreshold, 0.01f, 0.01f, 0.5f,
      "Cancel current job to find drink."),
    SET_BALANCE("energyWake", "Energy Wake", "Mover Balance/Thresholds", energyWakeThreshold, 0.05f, 0.3f, 1.0f,
      "Stop sleeping when energy reaches this."),

    // --- Mover balance: multipliers ---
    SET_BALANCE("carryingEnergy", "Carrying Energy", "Mover Balance/Multipliers", carryingEnergyMult, 0.05f, 0.5f, 3.0f,
      "Energy drain multiplier while carrying an item (hauling, delivering, crafting inputs)."),
    SET_BALANCE("hungerSpeedMin", "Hunger Speed Min", "Mover Balance/Multipliers", hungerSpeedPenaltyMin, 0.05f, 0.1f, 1.0f,
      "Minimum speed multiplier when starving."),
    SET_BALANCE("sleepMetabolism", "Sleep Metabolism", "Mover Balance/Multipliers", sleepMetabolismMult, 0.05f, 0.0f, 1.0f,
      "Hunger and thirst drain multiplier while resting (0.5: a meal and a drink last the night)."),

    // --- Mover balance: sleep rhythm ---
    SET_BOOL("sleepRhythm", "Day Rhythm", "Mover Balance/Sleep Rhythm", sleepRhythmEnabled,
      "Movers stay in bed while it is dark (unless a need is critical) and go to bed when it is dark and there is nothing to do. Night never makes anyone more tired."),
    SET_BALANCE("idleBedEnergy", "Idle Bed Energy", "Mover Balance/Sleep Rhythm", idleBedEnergy, 0.05f, 0.0f, 1.0f,
      "In the dark with nothing to do, a mover below this energy goes to bed."),
    SET_BALANCE("idleBedDelay", "Idle Bed Delay (GH)", "Mover Balance/Sleep Rhythm", idleBedDelayGH, 0.05f, 0.0f, 4.0f,
      "Game hours a mover is idle in the dark before going to bed."),

    // --- Mover balance: body temperature ---
    SET_BALANCE("bodyCoolingRate", "Cooling Rate", "Mover Balance/Body Temperature", bodyTempCoolingRatePerGH, 0.5f, 0.5f, 20.0f,
      "Degrees C per game-hour when losing heat."),
    SET_BALANCE("bodyWarmingRate", "Warming Rate", "Mover Balance/Body Temperature", bodyTempWarmingRatePerGH, 0.5f, 0.5f, 20.0f,
      "Degrees C per game-hour when gaining heat."),
    SET_BALANCE("metabolicHeat", "Metabolic Heat", "Mover Balance/Body Temperature", metabolicHeatBonus, 1.0f, 0.0f, 40.0f,
      "Degrees C added to ambient from metabolism. Scaled by hunger."),
    SET_BALANCE("mildCold", "Mild Cold", "Mover Balance/Body Temperature", mildColdThreshold, 0.5f, 30.0f, 37.0f,
      "Body temp below this = speed penalty."),
    SET_BALANCE("moderateCold", "Moderate Cold", "Mover Balance/Body Temperature", moderateColdThreshold, 0.5f, 28.0f, 36.0f,
      "Body temp below this = 2x energy drain."),
    SET_BALANCE("severeCold", "Severe Cold", "Mover Balance/Body Temperature", severeColdThreshold, 0.5f, 25.0f, 34.0f,
      "Body temp below this = hypothermia death timer."),
    SET_BALANCE("coldSpeedMin", "Cold Speed Min", "Mover Balance/Body Temperature", coldSpeedPenaltyMin, 0.05f, 0.1f, 1.0f,
      "Minimum speed multiplier when cold."),
    SET_BALANCE("coldEnergyMult", "Cold Energy Mult", "Mover Balance/Body Temperature", coldEnergyDrainMult, 0.1f, 1.0f, 5.0f,
      "Energy drain multiplier when moderately cold."),
    SET_BALANCE("hypothermiaDeath", "Hypothermia Death", "Mover Balance/Body Temperature", hypothermiaDeathGH, 0.5f, 1.0f, 12.0f,
      "Game-hours at severe cold before death."),

    // --- World: terrain generator parameters (sidebar shows them per terrain) ---
    SET_FLOAT("rampDensity", "Ramp Density", "World/Terrain/Hills", rampDensity, 0.02f, 0.0f, 1.0f,
      "Hills ramp placement density (0=none, 1=all). Lower reduces HPA* graph size.", NULL),
    SET_FLOAT("rampNoiseScale", "Ramp Noise Scale", "World/Terrain/Hills", rampNoiseScale, 0.005f, 0.005f, 0.2f,
      "Controls ramp cluster size for hills generators (higher = larger clusters).", NULL),
    SET_INT("riverCount", "River Count", "World/Terrain/HillsWater", hillsWaterRiverCount, 1.0f, 0, 6,
      "HillsSoilsWater: number of rivers (0 disables rivers).", NULL),
    SET_INT("riverWidth", "River Width", "World/Terrain/HillsWater", hillsWaterRiverWidth, 1.0f, 1, 4,
      "HillsSoilsWater: river width radius (1-4).", NULL),
    SET_INT("lakeCount", "Lake Count", "World/Terrain/HillsWater", hillsWaterLakeCount, 1.0f, 0, 6,
      "HillsSoilsWater: number of lakes (0 disables lakes).", NULL),
    SET_INT("lakeRadius", "Lake Radius", "World/Terrain/HillsWater", hillsWaterLakeRadius, 1.0f, 3, 12,
      "HillsSoilsWater: lake radius (3-12).", NULL),
    SET_FLOAT("wetnessBias", "Wetness Bias", "World/Terrain/HillsWater", hillsWaterWetnessBias, 0.05f, 0.0f, 1.0f,
      "HillsSoilsWater: wetness boost near water (peat/clay bias).", NULL),
    SET_BOOL("connReport", "Conn Report", "World/Terrain/HillsWater", hillsWaterConnectivityReport,
      "Logs walkability connectivity stats after generation."),
    SET_BOOL("fixTinyPockets", "Fix Tiny Pockets", "World/Terrain/HillsWater", hillsWaterConnectivityFixSmall,
      "Fills tiny disconnected walkable pockets after generation."),
    SET_INT("tinyPocketSize", "Tiny Size", "World/Terrain/HillsWater", hillsWaterConnectivitySmallThreshold, 1.0f, 5, 200,
      "Size threshold for tiny pocket fill (cells).", NULL),
    SET_BOOL("skipBuildings", "Skip Buildings", "World/Terrain/HillsWater", hillsSkipBuildings,
      "Skip placing ruined buildings during terrain generation."),
    SET_CHOICE("biome", "Biome", "World/Terrain/Biome", selectedBiome, biomeNames, BIOME_COUNT,
      "Biome preset for HillsSoilsWater; picking one loads its river and lake counts.", ApplyBiomePreset),

    // --- World: trees ---
    SET_FLOAT("saplingGrow", "Sapling Grow", "World/Trees", saplingGrowGH, 5.0f, 0.01f, 1000.0f,
      "Game-hours before a sapling becomes a young tree.", DescSaplingGrow),
    SET_FLOAT("trunkGrow", "Trunk Grow", "World/Trees", trunkGrowGH, 5.0f, 0.01f, 1000.0f,
      "Game-hours between growth stages.", DescTrunkGrow),
    SET_FLOAT("youngToMature", "Young→Mature", "World/Trees", youngToMatureGH, 10.0f, 0.01f, 2000.0f,
      "Game-hours a young tree waits at full height before maturing.", DescYoungToMature),
    SET_BOOL("saplingRegrowth", "Sapling Regrowth", "World/Trees", saplingRegrowthEnabled,
      "Enable natural sapling spawning on untrampled grass. Saplings appear over time in wilderness areas."),
    SET_INT("regrowthChance", "Regrowth Chance", "World/Trees", saplingRegrowthChance, 1.0f, 0, 100,
      "Chance per 10000 per interval for a sapling to spawn on tall grass.", DescRegrowthChance),
    SET_INT("minTreeDistance", "Min Tree Distance", "World/Trees", saplingMinTreeDistance, 1.0f, 1, 10,
      "Minimum tiles from existing trees/saplings for a new sapling.", DescMinTreeDistance),

    // --- World: entropy (ground wear) ---
    SET_BOOL("wear", "Enabled", "World/Entropy", groundWearEnabled,
      "Creates emergent paths: grass becomes dirt when trampled, dirt recovers to grass when left alone."),
    SET_INT("wearTrample", "Trample Amount", "World/Entropy/Advanced", wearTrampleAmount, 1.0f, 1, 100,
      "Wear added per mover step.", DescTrample),
    SET_INT("wearGrassToDirt", "Grass->Dirt Threshold", "World/Entropy/Advanced", wearGrassToDirt, 50.0f, 100, 10000,
      "Wear at which grass becomes dirt.", DescGrassToDirt),
    SET_INT("wearDirtToGrass", "Dirt->Grass Threshold", "World/Entropy/Advanced", wearDirtToGrass, 50.0f, 0, 5000,
      "Wear below which dirt regrows grass.", DescDirtToGrass),
    SET_INT("wearDecayRate", "Decay Rate", "World/Entropy/Advanced", wearDecayRate, 1.0f, 1, 100,
      "Wear recovered per decay step.", NULL),
    SET_FLOAT("wearRecoveryInterval", "Recovery Interval (GH)", "World/Entropy/Advanced", wearRecoveryInterval, 0.5f, 0.1f, 60.0f,
      "Game hours between wear decay steps.", NULL),

    // --- Pathfinding ---
    SET_CHOICE("pathAlgo", "Algo", "Pathfinding", pathAlgorithm, algorithmNames, 2,
      "Path search for the Find Path button and agents: 0 = A*, 1 = HPA*.", PathAlgoChanged),
    SET_CHOICE("pathDir", "Dir", "Pathfinding", currentDirection, directionNames, 2,
      "Movement directions: 0 = 4-dir, 1 = 8-dir.", PathDirChanged),
    SET_INT("navThreads", "Graph Threads", "Pathfinding", navThreads, 1.0f, 0, 32,
      "Threads for HPA* graph builds: 0 = one per core, 1 = no threads. Same graph either way.", NULL),

    // --- Movers ---
    SET_BOOL("stringPull", "String Pulling", "Entities/Movers", useStringPulling,
      "Straighten paths by skipping waypoints with line of sight."),
    SET_BOOL("endlessMode", "Endless Mode", "Entities/Movers", endlessMoverMode,
      "Movers pick a new random goal when they arrive."),
    SET_BOOL("preferDiffZ", "Prefer Diff Z", "Entities/Movers", preferDifferentZ,
      "Endless movers prefer goals on a different z-level."),
    SET_INT("wanderRadius", "Wander Radius", "Entities/Movers", moverWanderRadius, 4.0f, 0, 4096,
      "Spawned and endless movers pick goals within this many cells (0 = anywhere). Cross-map paths on big maps cost tens of ms each.", NULL),
    SET_BOOL("allowFall", "Allow Falling", "Entities/Movers", allowFallingFromAvoidance,
      "Avoidance may push movers into air (they fall)."),

    // --- Movers: needs (rule toggles; games set these on start) ---
    SET_BOOL("hunger", "Hunger", "Entities/Movers/Needs", hungerEnabled, "Movers get hungry and seek food."),
    SET_BOOL("energy", "Energy", "Entities/Movers/Needs", energyEnabled, "Movers get tired and seek rest."),
    SET_BOOL("bodyTemp", "Body Temp", "Entities/Movers/Needs", bodyTempEnabled, "Movers feel cold and heat."),
    SET_BOOL("thirst", "Thirst", "Entities/Movers/Needs", thirstEnabled, "Movers get thirsty and seek water."),
    SET_BOOL("bladder", "Bladder", "Entities/Movers/Needs", bladderEnabled, "Movers need a toilet."),
    SET_BOOL("mood", "Mood", "Entities/Movers/Needs", moodEnabled, "Movers have moods from needs and surroundings."),
    SET_BOOL("toolReqs", "Tool Reqs", "Entities/Movers/Needs", toolRequirementsEnabled,
      "Jobs need the right tool (e.g. an axe to chop)."),
    SET_FLOAT("foodCooling", "Food Cooling (GH)", "Entities/Movers/Needs", foodCoolingHalfLifeGH, 0.05f, 0.05f, 12.0f,
      "Game hours for hot food to lose half its heat over room temperature. Eaten at 40C or more = hot meal mood.", NULL),
    SET_FLOAT("fridgeSpoil", "Fridge Spoilage", "Entities/Movers/Needs", fridgeSpoilageFactor, 0.05f, 0.0f, 1.0f,
      "How fast food on a fridge spoils, as a share of the normal rate (0 = keeps forever, 1 = no fridge).", NULL),
    SET_INT("latrineDirt", "Latrine Dirt", "Entities/Movers/Needs", latrineDirtPerUse, 0.2f, 0, 255,
      "Floor dirt a pit latrine's cell gets each time it is used. Filthy from 80, cleaners come from 30.", NULL),
    SET_BOOL("toiletsPlumbed", "Toilets Plumbed", "Entities/Movers/Needs", toiletsPlumbed,
      "The world has mains water: every toilet flushes. Off (survival, island): a toilet flushes only with a tap, sink or water pot within 3 cells."),
    SET_INT("toiletDryDirt", "Dry Toilet Dirt", "Entities/Movers/Needs", toiletDryDirtPerUse, 0.2f, 0, 255,
      "Floor dirt a toilet gets per use when it can't be flushed: no tap or sink, and no water item (a filled pot), within 3 cells.", NULL),
    SET_FLOAT("carryLimit", "Carry Limit (kg)", "Entities/Movers/Needs", carryLimitKg, 0.5f, 5.0f, 200.0f,
      "Most a mover carries in hands, pockets and on the back (the back counts half). Above it nothing more is picked up.", NULL),
    SET_INT("trashBinWalk", "Bin Walk (cells)", "Entities/Movers/Needs", trashBinWalkCells, 0.2f, 0, 64,
      "How far a mover walks to put a can or wrapper in a trash bin (Tidy: twice as far, Slob: 2). No bin with room that close: litter.", NULL),

    // --- Movers: avoidance ---
    SET_BOOL("avoidance", "Enabled", "Entities/Movers/Avoidance", useMoverAvoidance,
      "Movers steer around each other."),
    SET_BOOL("dirAvoid", "Directional", "Entities/Movers/Avoidance", useDirectionalAvoidance,
      "Filter avoidance by wall clearance."),
    SET_FLOAT("avoidOpen", "Open Strength", "Entities/Movers/Avoidance", avoidStrengthOpen, 0.01f, 0.0f, 2.0f,
      "Avoidance strength in open areas.", NULL),
    SET_FLOAT("avoidClosed", "Closed Strength", "Entities/Movers/Avoidance", avoidStrengthClosed, 0.01f, 0.0f, 2.0f,
      "Avoidance strength near walls.", NULL),

    // --- Movers: walls, passages, ladders ---
    SET_BOOL("wallRepulsion", "Repulsion", "Entities/Movers/Walls", useWallRepulsion,
      "Walls push movers away."),
    SET_FLOAT("wallRepStr", "Repel Strength", "Entities/Movers/Walls", wallRepulsionStrength, 0.01f, 0.0f, 2.0f,
      "Strength of the wall push.", NULL),
    SET_BOOL("wallSlide", "Sliding", "Entities/Movers/Walls", useWallSliding,
      "Movers slide along walls instead of stopping."),
    SET_BOOL("knotFix", "Knot Fix", "Entities/Movers/Walls", useKnotFix,
      "Movers count as arrived a bit farther from their goal, so crowds don't knot up there."),
    SET_BOOL("tunnelYield", "Tunnel Yielding", "Entities/Movers/Walls", useTunnelYielding,
      "Wait at 1-wide passages for oncoming movers."),
    SET_BOOL("singleFileLadders", "Single-file Ladders", "Entities/Movers/Walls", useSingleFileLadders,
      "One climber per ladder segment at a time."),
    SET_BOOL("ladderQueueCost", "Ladder Queue Cost", "Entities/Movers/Walls", useLadderQueueCost,
      "Routes see ladder queues as extra cost."),
    SET_FLOAT("ladderClimbSpeed", "Ladder Climb Speed", "Entities/Movers/Walls", ladderClimbSpeed, 0.01f, 0.1f, 1.0f,
      "Climbing speed vs walking (0.5 = twice as slow).", NULL),

    // --- Animals ---
    SET_BOOL("animalRespawn", "Respawn", "Entities/Animals", animalRespawnEnabled,
      "Animals respawn from map edges."),
    SET_INT("animalTargetPop", "Target Pop", "Entities/Animals", animalTargetPopulation, 1.0f, 1, 50,
      "Respawn keeps the animal count near this.", DescAnimalPop),
    SET_FLOAT("animalSpawnInterval", "Spawn Interval (GH)", "Entities/Animals", animalSpawnIntervalGH, 1.0f, 1.0f, 240.0f,
      "Game hours between spawn attempts.", DescAnimalSpawn),

    // --- Trains ---
    SET_BOOL("trainQueue", "Queue Line", "Entities/Trains", trainQueueEnabled,
      "Movers form a visible queue line at stations instead of spreading out."),
    SET_BOOL("trainFreight", "Freight", "Entities/Trains", trainFreight,
      "Trains carry loads between cargo stockpiles (a stockpile within 2 cells of a platform), to a stop whose cargo stockpile has a higher priority. Haulers that would ride with an item leave it at the boarding stop's cargo stockpile instead."),
    SET_BOOL("useTrainTransfers", "Transfers", "Entities/Trains", useTrainTransfers,
      "Trip choice may plan a change of line: ride one line, walk to another line's platform, ride on. Off = one line per trip (a mover may still change by chance after a ride)."),

    // --- Elevators ---
    SET_FLOAT("elevatorSpeed", "Speed (levels/s)", "Entities/Elevators", elevatorSpeed, 0.1f, 0.2f, 10.0f,
      "Elevator car speed in levels per second (at dayLength 60, like trains).", NULL),
    SET_INT("elevatorCapacity", "Capacity", "Entities/Elevators", elevatorCapacity, 1.0f, 1, ELEVATOR_MAX_RIDERS,
      "Movers one car carries at a time; the others wait for its next trip.", NULL),
    SET_FLOAT("elevatorDoorTime", "Door Time (s)", "Entities/Elevators", elevatorDoorTime, 0.1f, 0.2f, 10.0f,
      "Seconds the doors stay open at a stop (at dayLength 60).", NULL),
    SET_BOOL("elevatorDirectionalPickup", "Sweep Pickup", "Entities/Elevators", elevatorDirectionalPickup,
      "An empty car on its way to calls further on picks up only people going its way; the others ride on its way back. Off: it stops for anyone and turns round, and the calls further on wait."),
    SET_FLOAT_HOOK("stairsClimbSpeed", "Stairs Speed", "Entities/Elevators", stairsClimbSpeed, 0.05f, 0.1f, 1.0f,
      "Climbing speed on built ramps (stairs; not natural slopes) vs walking; down is a bit quicker. Ladders stay slower (ladderClimbSpeed). Rebuilds the pathfinder graph.", NULL, RebuildGraphForStairs),
    SET_FLOAT_HOOK("stairsEffort", "Stairs Effort (cells)", "Entities/Elevators", stairsEffort, 0.5f, 0.0f, 40.0f,
      "Route cost of a level up a built ramp (stairs) on top of its time, in cells of walking (down: half): movers take the elevator for long trips. Natural slopes: none. Rebuilds the pathfinder graph.", NULL, RebuildGraphForStairs),
    SET_BOOL("elevatorWaitCost", "Wait Cost", "Entities/Elevators", useElevatorWaitCost,
      "Routes count the expected wait for the car (doors, the car coming, the line ahead): with stairs nearby, movers take them when the elevator is busy. Waiting movers re-plan after a while."),

    // --- Cars experiment (src/entities/cars.c, scenario "cars") ---
    SET_FLOAT("carLength", "Length (cells)", "Experiments/Cars", carLength, 0.1f, 1.0f, 4.0f,
      "Car length in cells. The planner only checks the width; length matters for turning (wheelbase = 0.6 x length).", NULL),
    SET_FLOAT("carWidth", "Width (cells)", "Experiments/Cars", carWidth, 0.1f, 1.0f, 3.0f,
      "Car width in cells: the planner needs a ceil(width) square free at every point of the route.", NULL),
    SET_FLOAT("carMaxSpeed", "Top Speed (cells/s)", "Experiments/Cars", carMaxSpeed, 0.1f, 0.5f, 30.0f,
      "Top speed on a straight road, cells per game second.", NULL),
    SET_FLOAT("carAccel", "Acceleration", "Experiments/Cars", carAccel, 0.1f, 0.2f, 30.0f,
      "Speeding up, cells/s per second.", NULL),
    SET_FLOAT("carBrake", "Braking", "Experiments/Cars", carBrake, 0.1f, 0.5f, 60.0f,
      "Braking, cells/s per second. Also sets how early the car slows for corners and the end of the route.", NULL),
    SET_FLOAT("carLatAccel", "Cornering Grip", "Experiments/Cars", carLatAccel, 0.1f, 0.5f, 30.0f,
      "Sideways acceleration limit (cells/s^2): how fast it takes a corner of a given radius.", NULL),
    SET_FLOAT("carMaxSteer", "Steering Lock (deg)", "Experiments/Cars", carMaxSteerDeg, 1.0f, 10.0f, 60.0f,
      "Most the front wheels turn. With the wheelbase this sets the turning circle (radius printed in the headless stats).", NULL),
    SET_FLOAT("carSteerRate", "Steering Speed (deg/s)", "Experiments/Cars", carSteerRate, 5.0f, 20.0f, 720.0f,
      "How fast the wheel turns.", NULL),
    SET_FLOAT("carLookahead", "Lookahead (cells)", "Experiments/Cars", carLookahead, 0.1f, 0.5f, 6.0f,
      "Pure pursuit: the car aims at the route point this far ahead (more at speed). Small = follows corners tightly, large = smooth but cuts corners.", NULL),
    SET_FLOAT("carStopWait", "Wait at Stop (s)", "Experiments/Cars", carStopWait, 0.1f, 0.0f, 30.0f,
      "Seconds parked at each stop before driving to the next.", NULL),
    SET_FLOAT("carPlanMargin", "Plan Margin (px)", "Experiments/Cars", carPlanMargin, 0.1f, 0.0f, 8.0f,
      "The planner keeps the car body this many pixels (of 32 per cell) away from walls, so small steering errors don't scrape. 0 = exactly to the wall.", NULL),
    SET_INT("carTrafficCount", "Traffic Cars", "Experiments/Cars", carTrafficCount, 0.2f, 1, MAX_CARS,
      "How many cars the cars-traffic scenario spawns (restart the scenario to apply).", NULL),
    SET_INT("carCityCount", "City Cars", "Experiments/Cars", carCityCount, 0.5f, 1, MAX_CARS,
      "How many cars the cars-city scenario spawns on its road network (restart the scenario to apply).", NULL),
    SET_INT("carCitySeed", "City Layout Seed", "Experiments/Cars", carCitySeed, 0.2f, 1, 9999,
      "Road layout of the cars-city scenario (restart the scenario to apply).", NULL),
    SET_BOOL("carCityLights", "City Traffic Lights", "Experiments/Cars", carCityLights,
      "Traffic lights on the 4-way junctions of cars-city (others: first come, first served). Restart the scenario to apply."),
    SET_BOOL("carFollow", "Camera Follows Car", "Experiments/Cars", carFollow,
      "The camera follows car 0 (GUI). Turn off to pan freely."),
    SET_BOOL("carDebug", "Debug Draw", "Experiments/Cars", carDebug,
      "Draw route, pursuit point, state, stops, and where a car centre fits (dots; big = room to turn, the planner prefers those)."),

    // --- Belts ---
    SET_FLOAT("beltSpeed", "Speed (cells/s)", "Entities/Belts", beltSpeed, 0.1f, 0.1f, 20.0f,
      "Conveyor belt speed in cells per second (at dayLength 60, like trains). One item per cell.", NULL),
    SET_FLOAT("beltLiftFactor", "Lift speed (x belt)", "Entities/Belts", beltLiftFactor, 0.05f, 0.05f, 2.0f,
      "A lift (belt going up a level) moves at this fraction of the belt speed: lifting is slow.", NULL),
    SET_FLOAT("beltChuteFactor", "Chute speed (x belt)", "Entities/Belts", beltChuteFactor, 0.1f, 0.5f, 10.0f,
      "A chute (belt going down a level) moves at this multiple of the belt speed: things fall quickly.", NULL),

    // --- Lighting ---
    SET_BOOL_HOOK("lighting", "Enabled", "Rendering & Debug/Lighting", lightingEnabled,
      "Master toggle for lighting. When off, all tiles render at full brightness (no light/dark cycle).", RelightAll),
    SET_BOOL("shaderEffects", "Shader Effects", "Rendering & Debug/Lighting", shaderEffects,
      "Experiment: water ripples and fire flicker drawn by the world batch's shader (render/world_batch.c). Off = the reference look."),
    SET_BOOL("forceDaylight", "Force Daylight", "Rendering & Debug/Lighting", forceDaylight,
      "Override sky color to full white. Useful for building at night without waiting for day."),
    SET_BOOL_HOOK("skyLight", "Sky Light", "Rendering & Debug/Lighting", skyLightEnabled,
      "Compute sky light from open columns. Sky light follows time-of-day sky color.", RelightAll),
    SET_BOOL_HOOK("blockLight", "Block Light", "Rendering & Debug/Lighting", blockLightEnabled,
      "Compute light from placed sources (torches). Colored flood fill through open cells.", RelightAll),
    SET_INT("lightAmbientR", "Red", "Rendering & Debug/Lighting/Ambient", lightAmbientR, 1.0f, 0, 255,
      "Minimum red component. Prevents completely black tiles.", NULL),
    SET_INT("lightAmbientG", "Green", "Rendering & Debug/Lighting/Ambient", lightAmbientG, 1.0f, 0, 255,
      "Minimum green component. Prevents completely black tiles.", NULL),
    SET_INT("lightAmbientB", "Blue", "Rendering & Debug/Lighting/Ambient", lightAmbientB, 1.0f, 0, 255,
      "Minimum blue component. Slightly higher default gives a cool moonlight feel in darkness.", NULL),
    SET_INT("torchIntensity", "Intensity", "Rendering & Debug/Lighting/Torch", lightDefaultIntensity, 1.0f, 1, 15,
      "Propagation radius for new torches.", DescTorchIntensity),
    SET_INT("torchR", "R", "Rendering & Debug/Lighting/Torch", lightDefaultR, 1.0f, 0, 255, "Red component of new torches.", NULL),
    SET_INT("torchG", "G", "Rendering & Debug/Lighting/Torch", lightDefaultG, 1.0f, 0, 255, "Green component of new torches.", NULL),
    SET_INT("torchB", "B", "Rendering & Debug/Lighting/Torch", lightDefaultB, 1.0f, 0, 255, "Blue component of new torches.", NULL),

    // --- Debug view toggles ---
    SET_BOOL("movers", "Show Movers", "Rendering & Debug/Debug/Rendering", showMovers, "Draw movers."),
    SET_BOOL("pixelPerfect", "Pixel Perfect", "Rendering & Debug/Debug/Rendering", usePixelPerfectMovers,
      "Snap mover sprites to whole pixels."),
    SET_BOOL("items", "Show Items", "Rendering & Debug/Debug/Rendering", showItems, "Draw items."),
    SET_BOOL("cullDrawing", "Cull Drawing", "Rendering & Debug/Debug/Rendering", cullDrawing,
      "Only draw what is on screen."),
    SET_BOOL("cursorDebug", "Cursor Debug", "Rendering & Debug/Debug/Rendering", showCursorDebug,
      "Highlight the cell under the cursor."),
    SET_BOOL("graph", "Show Graph", "Rendering & Debug/Debug/Pathfinding", showGraph, "Draw the HPA* graph."),
    SET_BOOL("entrances", "Show Entrances", "Rendering & Debug/Debug/Pathfinding", showEntrances,
      "Draw HPA* chunk entrances."),
    SET_BOOL("chunks", "Show Chunks", "Rendering & Debug/Debug/Pathfinding", showChunkBoundaries,
      "Draw chunk boundaries."),
    SET_BOOL("paths", "Show Paths", "Rendering & Debug/Debug/Pathfinding", showMoverPaths, "Draw mover paths."),
    SET_BOOL("jobs", "Show Job Lines", "Rendering & Debug/Debug/Pathfinding", showJobLines,
      "Draw lines from movers to their job targets."),
    SET_BOOL("neighbors", "Show Neighbors", "Rendering & Debug/Debug/Mover Diagnostics", showNeighborCounts,
      "Debug overlay: each mover's neighbor count."),
    SET_BOOL("openArea", "Show Open Area", "Rendering & Debug/Debug/Mover Diagnostics", showOpenArea,
      "Debug overlay for open-area detection."),
    SET_BOOL("knots", "Show Knots", "Rendering & Debug/Debug/Mover Diagnostics", showKnotDetection,
      "Debug overlay for knot detection."),
    SET_BOOL("stuck", "Show Stuck", "Rendering & Debug/Debug/Mover Diagnostics", showStuckDetection,
      "Debug overlay for stuck detection."),
    SET_BOOL("simSources", "Sim Sources", "Rendering & Debug/Debug/Overlays", showSimSources,
      "Mark simulation source cells on the current z-level."),
    SET_BOOL("tempOverlay", "Temperature", "Rendering & Debug/Debug/Overlays", showTemperatureOverlay,
      "Color cells by temperature."),
    SET_BOOL("waterFlecks", "Water Flecks", "Rendering & Debug/Debug/Rendering", waterFlecks,
      "Light flecks slide along flowing water, the way the current runs (still water gets none). Off by default for now."),
    SET_BOOL("flowOverlay", "Water Flow", "Rendering & Debug/Debug/Overlays", showWaterFlow,
      "Arrows showing which way water has been moving (the current) on the current z-level and the one below. Zoomed out, an arrow sums a block of cells."),
};
#undef SET_BOOL
#undef SET_BOOL_HOOK
#undef SET_CHOICE
#undef SET_INT
#undef SET_FLOAT
#undef SET_FLOAT_HOOK
#undef SET_BALANCE
#define SETTING_COUNT ((int)(sizeof(settings) / sizeof(settings[0])))

int SettingCount(void) { return SETTING_COUNT; }

const Setting* SettingAt(int index) {
    return (index >= 0 && index < SETTING_COUNT) ? &settings[index] : NULL;
}

const Setting* SettingFind(const char* name) {
    for (int i = 0; i < SETTING_COUNT; i++)
        if (strcasecmp(settings[i].name, name) == 0) return &settings[i];
    return NULL;
}

const char* SettingTooltip(const Setting* s) {
    return s->describe ? s->describe() : s->help;
}

float SettingValue(const Setting* s) {
    switch (s->type) {
        case SETTING_BOOL:  return *(bool*)s->ptr ? 1.0f : 0.0f;
        case SETTING_INT:
        case SETTING_CHOICE: return (float)*(int*)s->ptr;
        case SETTING_FLOAT: return *(float*)s->ptr;
    }
    return 0.0f;
}

void Settings_RegisterConsoleVars(void) {
    for (int i = 0; i < SETTING_COUNT; i++) {
        const Setting* s = &settings[i];
        // A choice is an int index for the console (range 0..optionCount-1)
        CVarType t = s->type == SETTING_BOOL ? CVAR_BOOL : s->type == SETTING_FLOAT ? CVAR_FLOAT : CVAR_INT;
        Console_RegisterVarEx(s->name, s->ptr, t, s->min, s->max, s->help, s->onChange);
    }
}

void Settings_WarnOutOfRange(void) {
    // A default outside its own range gets clamped the first time anyone touches the
    // slider or uses --set (fire and heat intervals were, by 1.25-2.5x)
    for (int i = 0; i < SETTING_COUNT; i++) {
        const Setting* s = &settings[i];
        if ((s->type == SETTING_FLOAT || s->type == SETTING_INT) &&
            (SettingValue(s) < s->min || SettingValue(s) > s->max))
            TraceLog(LOG_WARNING, "setting %s: default %g is outside its range %g..%g",
                     s->name, SettingValue(s), s->min, s->max);
    }
}
