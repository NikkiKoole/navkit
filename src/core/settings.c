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
#include "../entities/mover.h"
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

static const char* DescWeatherMin(void) {
    return TextFormat("Shortest weather spell: %.0f game-seconds (%.1f game-hours).",
                      weatherMinDuration, GameHours(weatherMinDuration));
}
static const char* DescWeatherMax(void) {
    return TextFormat("Longest weather spell: %.0f game-seconds (%.1f game-hours).",
                      weatherMaxDuration, GameHours(weatherMaxDuration));
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
    return TextFormat("Simulation speed multiplier. At %.1fx, 1 real-second = %.1f game-seconds.%s",
                      gameSpeed, gameSpeed, gameSpeed == 0.0f ? " (PAUSED)" : "");
}
static const char* DescDayLength(void) {
    float realDuration = gameSpeed > 0 ? dayLength / gameSpeed : 0;
    return TextFormat("Game-seconds per full day.\nAt 1x speed: %.1f real-%s per day.\nAt current %.1fx: %.1f real-%s per day.",
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
    return TextFormat("Base mover speed in pixels/sec. = %.1f tiles/game-sec.", BaseTilesPerSecond());
}
static const char* DescSpeedVariance(void) {
    return TextFormat("Random speed spread per mover: +/-%.0f%%. Range: %.1f - %.1f tiles/game-sec.",
                      balance.moverSpeedVariance * 100.0f,
                      BaseTilesPerSecond() * (1.0f - balance.moverSpeedVariance),
                      BaseTilesPerSecond() * (1.0f + balance.moverSpeedVariance));
}

// ---------------------------------------------------------------------------
// Change hooks
// ---------------------------------------------------------------------------

// Balance inputs feed derived per-game-hour rates
static void RecalcBalance(float oldValue) { (void)oldValue; RecalcBalanceTable(); }

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
    {name, label, group, SETTING_BOOL, &(var), 0, 0, 0, help, NULL, NULL}
#define SET_INT(name, label, group, var, speed, lo, hi, help, desc) \
    {name, label, group, SETTING_INT, &(var), speed, lo, hi, help, desc, NULL}
#define SET_FLOAT(name, label, group, var, speed, lo, hi, help, desc) \
    {name, label, group, SETTING_FLOAT, &(var), speed, lo, hi, help, desc, NULL}
#define SET_FLOAT_HOOK(name, label, group, var, speed, lo, hi, help, desc, hook) \
    {name, label, group, SETTING_FLOAT, &(var), speed, lo, hi, help, desc, hook}
// Mover balance input: changing it recalculates the derived rates
#define SET_BALANCE(name, label, group, field, speed, lo, hi, help) \
    SET_FLOAT_HOOK(name, label, group, balance.field, speed, lo, hi, help, NULL, RecalcBalance)

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

    // --- Weather ---
    SET_BOOL("weather", "Weather Enabled", "Weather & Time/Weather", weatherEnabled,
      "Weather changes over time (clear, cloudy, rain, storms, snow)."),
    SET_FLOAT("weatherMinDuration", "Min Duration", "Weather & Time/Weather", weatherMinDuration, 1.0f, 5.0f, 300.0f,
      "Shortest weather spell (game-seconds).", DescWeatherMin),
    SET_FLOAT("weatherMaxDuration", "Max Duration", "Weather & Time/Weather", weatherMaxDuration, 1.0f, 10.0f, 600.0f,
      "Longest weather spell (game-seconds).", DescWeatherMax),
    SET_FLOAT("rainWetnessInterval", "Rain Wetness Interval", "Weather & Time/Weather", rainWetnessInterval, 0.5f, 0.5f, 30.0f,
      "Game-seconds between ground wetness increments during rain.", NULL),
    SET_FLOAT("heavyRainWetnessInterval", "Heavy Rain Interval", "Weather & Time/Weather", heavyRainWetnessInterval, 0.5f, 0.5f, 15.0f,
      "Game-seconds between ground wetness increments during heavy rain.", NULL),
    SET_FLOAT("lightningInterval", "Lightning Interval", "Weather & Time/Weather", lightningInterval, 1.0f, 0.5f, 30.0f,
      "Game-seconds between lightning strikes in thunderstorms.", NULL),
    SET_FLOAT("snowAccumulation", "Snow Accumulation", "Weather & Time/Weather", snowAccumulationRate, 0.01f, 0.01f, 1.0f,
      "Snow levels gained per game-hour while snowing.", DescSnowAccumulation),
    SET_FLOAT("snowMelting", "Snow Melting", "Weather & Time/Weather", snowMeltingRate, 0.01f, 0.01f, 0.5f,
      "Snow levels lost per game-hour above freezing.", DescSnowMelting),

    // --- Time ---
    SET_FLOAT("speed", "Game Speed", "Weather & Time/Time", gameSpeed, 0.1f, 0.0f, 100.0f,
      "Simulation speed multiplier (0 = paused).", DescGameSpeed),
    SET_FLOAT("dayLen", "Day Length", "Weather & Time/Time", dayLength, 10.0f, 10.0f, 3600.0f,
      "Game-seconds per full day.", DescDayLength),
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
    SET_FLOAT_HOOK("baseMoverSpeed", "Base Speed (px/s)", "Mover Balance", balance.baseMoverSpeed, 10.0f, 50.0f, 800.0f,
      "Base mover speed in pixels/sec. Changing it rescales every mover's speed.", DescBaseSpeed, RescaleMoverSpeeds),
    SET_FLOAT("moverSpeedVariance", "Speed Variance", "Mover Balance", balance.moverSpeedVariance, 0.05f, 0.0f, 0.5f,
      "Random speed spread per new mover (+/- fraction).", DescSpeedVariance),

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
    SET_BALANCE("nightEnergy", "Night Energy", "Mover Balance/Multipliers", nightEnergyMult, 0.05f, 0.5f, 3.0f,
      "Energy drain multiplier at night."),
    SET_BALANCE("carryingEnergy", "Carrying Energy", "Mover Balance/Multipliers", carryingEnergyMult, 0.05f, 0.5f, 3.0f,
      "Energy drain multiplier when hauling."),
    SET_BALANCE("hungerSpeedMin", "Hunger Speed Min", "Mover Balance/Multipliers", hungerSpeedPenaltyMin, 0.05f, 0.1f, 1.0f,
      "Minimum speed multiplier when starving."),

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
};
#undef SET_BOOL
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
        case SETTING_INT:   return (float)*(int*)s->ptr;
        case SETTING_FLOAT: return *(float*)s->ptr;
    }
    return 0.0f;
}

void Settings_RegisterConsoleVars(void) {
    for (int i = 0; i < SETTING_COUNT; i++) {
        const Setting* s = &settings[i];
        CVarType t = s->type == SETTING_BOOL ? CVAR_BOOL : s->type == SETTING_INT ? CVAR_INT : CVAR_FLOAT;
        Console_RegisterVarEx(s->name, s->ptr, t, s->min, s->max, s->help, s->onChange);
    }
}
