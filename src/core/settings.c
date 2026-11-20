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
#include "../ui/console.h"
#include <strings.h>

// ---------------------------------------------------------------------------
// Live tooltips: settings whose tooltip shows derived values (game-hours, tiles/s)
// ---------------------------------------------------------------------------

static float GameHours(float gameSeconds) { return gameSeconds / (dayLength / 24.0f); }
static float BaseTilesPerSecond(void) { return balance.baseMoverSpeed / (float)CELL_SIZE; }

static const char* DescWaterEvap(void) {
    return TextFormat("Puddles evaporate every %.1f game-seconds (%.2f game-hours).",
                      waterEvapInterval, GameHours(waterEvapInterval));
}
static const char* DescSpeedShallow(void) {
    return TextFormat("Mover speed in shallow water: %.0f%% (%.1f tiles/gs).",
                      waterSpeedShallow * 100.0f, BaseTilesPerSecond() * waterSpeedShallow);
}
static const char* DescSpeedMedium(void) {
    return TextFormat("Mover speed in medium water: %.0f%% (%.1f tiles/gs).",
                      waterSpeedMedium * 100.0f, BaseTilesPerSecond() * waterSpeedMedium);
}
static const char* DescSpeedDeep(void) {
    return TextFormat("Mover speed in deep water: %.0f%% (%.1f tiles/gs).",
                      waterSpeedDeep * 100.0f, BaseTilesPerSecond() * waterSpeedDeep);
}
static const char* DescSpeedMud(void) {
    return TextFormat("Mover speed on mud: %.0f%% (%.1f tiles/gs).",
                      mudSpeedMultiplier * 100.0f, BaseTilesPerSecond() * mudSpeedMultiplier);
}
static const char* DescFireSpread(void) {
    return TextFormat("Fire tries to spread every %.1f game-seconds (%.1f attempts/gs).",
                      fireSpreadInterval, 1.0f / fireSpreadInterval);
}
static const char* DescFireFuel(void) {
    float woodBurnGS = 10.0f * fireFuelInterval;   // wood fuel ~10
    return TextFormat("Fuel consumed every %.1f game-seconds.\nWood wall burns for ~%.0fs (~%.1f game-hours).",
                      fireFuelInterval, woodBurnGS, GameHours(woodBurnGS));
}
static const char* DescFireWater(void) {
    return TextFormat("Fire spreads %.1fx slower near water (%d%% chance reduction).",
                      100.0f / (float)fireWaterReduction, fireWaterReduction);
}
static const char* DescSmokeDissipation(void) {
    return TextFormat("Smoke fully clears in %.1f game-seconds (%.2f game-hours).",
                      smokeDissipationTime, GameHours(smokeDissipationTime));
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

// ---------------------------------------------------------------------------
// The table. Order within a group = order in the sidebar.
// Console names kept from before the table existed: water, fire, smoke, steam,
// temp, fireSpread, fireFuel, surfTemp.
// ---------------------------------------------------------------------------

#define SET_BOOL(name, label, group, var, help) \
    {name, label, group, SETTING_BOOL, &(var), 0, 0, 0, help, NULL}
#define SET_INT(name, label, group, var, speed, lo, hi, help, desc) \
    {name, label, group, SETTING_INT, &(var), speed, lo, hi, help, desc}
#define SET_FLOAT(name, label, group, var, speed, lo, hi, help, desc) \
    {name, label, group, SETTING_FLOAT, &(var), speed, lo, hi, help, desc}

static const Setting settings[] = {
    // --- Water ---
    SET_BOOL("water", "Enabled", "Simulation/Water", waterEnabled,
      "Master toggle for water simulation. Water flows down, spreads horizontally, and uses pressure to rise through U-bends."),
    SET_BOOL("waterEvaporation", "Evaporation", "Simulation/Water", waterEvaporationEnabled,
      "When enabled, shallow water (level 1) has a chance to evaporate each tick. Disable for testing water mechanics."),
    SET_FLOAT("waterEvapInterval", "Evap Interval (s)", "Simulation/Water", waterEvapInterval, 1.0f, 1.0f, 120.0f,
      "Game-seconds between evaporation attempts for level-1 water.", DescWaterEvap),
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
    SET_FLOAT("fireSpread", "Spread Interval (s)", "Simulation/Fire", fireSpreadInterval, 0.1f, 0.1f, 10.0f,
      "Game-seconds between fire spread attempts.", DescFireSpread),
    SET_FLOAT("fireFuel", "Fuel Interval (s)", "Simulation/Fire", fireFuelInterval, 0.1f, 0.1f, 10.0f,
      "Game-seconds between fuel consumption.", DescFireFuel),
    SET_INT("fireWaterReduction", "Water Reduction %", "Simulation/Fire", fireWaterReduction, 1.0f, 1, 100,
      "Spread chance near water, in percent of normal.", DescFireWater),
    SET_INT("fireSpreadBase", "Base Chance %", "Simulation/Fire/Advanced", fireSpreadBase, 1.0f, 0, 50,
      "Base spread chance before fire level bonus. Formula: base + (level * perLevel).", NULL),
    SET_INT("fireSpreadPerLevel", "Per Level %", "Simulation/Fire/Advanced", fireSpreadPerLevel, 1.0f, 0, 30,
      "Additional spread chance per fire level.", NULL),

    // --- Smoke ---
    SET_BOOL("smoke", "Enabled", "Simulation/Smoke", smokeEnabled,
      "Master toggle for smoke simulation. Smoke rises, spreads horizontally, fills enclosed spaces, and gradually dissipates."),
    SET_FLOAT("smokeRise", "Rise Interval (s)", "Simulation/Smoke", smokeRiseInterval, 0.01f, 0.01f, 2.0f,
      "Game-seconds for smoke to rise one Z-level.", NULL),
    SET_FLOAT("smokeDissipation", "Dissipation Time", "Simulation/Smoke", smokeDissipationTime, 0.1f, 0.5f, 30.0f,
      "Game-seconds for smoke to fully clear.", DescSmokeDissipation),
    SET_INT("smokeGeneration", "Generation Rate", "Simulation/Smoke", smokeGenerationRate, 1.0f, 1, 10,
      "Smoke per tick = fire level / this.", DescSmokeGeneration),

    // --- Steam ---
    SET_BOOL("steam", "Enabled", "Simulation/Steam", steamEnabled,
      "Master toggle for steam simulation. Steam rises from boiling water, spreads, and condenses back to water when cooled."),
    SET_FLOAT("steamRise", "Rise Interval (s)", "Simulation/Steam", steamRiseInterval, 0.01f, 0.01f, 2.0f,
      "Game-seconds for steam to rise one Z-level.", NULL),
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
    SET_FLOAT("heatTransferInterval", "Transfer Interval (s)", "Simulation/Temperature", heatTransferInterval, 0.1f, 0.1f, 60.0f,
      "Game-seconds between heat spreading steps. Lower = faster heat flow.", NULL),
    SET_FLOAT("tempDecayInterval", "Decay Interval (s)", "Simulation/Temperature", tempDecayInterval, 0.1f, 0.1f, 60.0f,
      "Game-seconds between decay-toward-ambient steps. Lower = faster cooling.", NULL),
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
};
#undef SET_BOOL
#undef SET_INT
#undef SET_FLOAT
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

void Settings_RegisterConsoleVars(void) {
    for (int i = 0; i < SETTING_COUNT; i++) {
        const Setting* s = &settings[i];
        CVarType t = s->type == SETTING_BOOL ? CVAR_BOOL : s->type == SETTING_INT ? CVAR_INT : CVAR_FLOAT;
        Console_RegisterVarEx(s->name, s->ptr, t, s->min, s->max, s->help);
    }
}
