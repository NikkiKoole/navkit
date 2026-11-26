#include "weather.h"
#include "moisture.h"
#include "temperature.h"
#include "water.h"
#include "balance.h"
#include "../world/grid.h"
#include "../core/event_log.h"
#include "../world/cell_defs.h"
#include "../world/material.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// =============================================================================
// Season Parameters
// =============================================================================

int daysPerSeason = 7;
int baseSurfaceTemp = 15;  // Center of seasonal range: 15 ± 25 = -10C to 40C
int seasonalAmplitude = 25;                  // Temp swing ±25 from base (winter ~-5C, summer ~45C)
int diurnalAmplitude = 5;                    // Day/night temp swing ±5C (peak 14:00, trough 02:00)

// =============================================================================
// Weather State
// =============================================================================

WeatherState weatherState;
bool weatherEnabled = false;  // Off by default; InitWeather() enables it
float weatherMinDuration = 12.0f;          // 12.0 game-hours (was 30.0 game-seconds)
float weatherMaxDuration = 48.0f;          // 48.0 game-hours (was 120.0 game-seconds)
float rainWetnessInterval = 2.0f;          // 2.0 game-hours (was 5.0 game-seconds)
float heavyRainWetnessInterval = 0.8f;     // 0.8 game-hours (was 2.0 game-seconds)
float intensityRampSpeed = 0.5f;           // 0.5 per game-hour (was 0.2 per game-second, rate conversion)

float windDryingMultiplier = 1.5f;  // How much wind accelerates drying (default 1.5x at strength 3)

// Internal accumulators
static float rainWetnessAccum = 0.0f;
static float weatherWindAccum = 0.0f;

// Target wind values (wind smoothly approaches these)
static float targetWindStrength = 0.0f;

// Lightning state (Phase 5)
static float lightningTimer = 5.0f;
static float lightningFlashTimer = 0.0f;

// =============================================================================
// Transition Probability Tables
// =============================================================================

// Base transition weights from each weather type to each other type
// Rows = current weather, Columns = target weather
// These get modulated by season
static const float transitionWeights[WEATHER_COUNT][WEATHER_COUNT] = {
    // To:   CLEAR  CLOUDY  RAIN  H_RAIN  THUNDER  SNOW  MIST
    /*CLEAR*/      { 0.0f, 6.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f },
    /*CLOUDY*/     { 4.0f, 0.0f, 3.0f, 0.5f, 0.0f, 1.0f, 0.5f },
    /*RAIN*/       { 1.0f, 3.0f, 0.0f, 2.0f, 0.0f, 0.0f, 0.0f },
    /*HEAVY_RAIN*/ { 0.0f, 1.0f, 3.0f, 0.0f, 1.0f, 0.0f, 0.0f },
    /*THUNDER*/    { 0.0f, 0.5f, 3.0f, 2.0f, 0.0f, 0.0f, 0.0f },
    /*SNOW*/       { 2.0f, 3.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    /*MIST*/       { 3.0f, 2.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f },
};

// Season multipliers for certain weather types
static float GetSeasonMultiplier(WeatherType target) {
    Season season = GetCurrentSeason();
    switch (target) {
        case WEATHER_SNOW:
            return (season == SEASON_WINTER) ? 3.0f : 0.0f;  // Snow only in winter
        case WEATHER_THUNDERSTORM:
            return (season == SEASON_SUMMER) ? 2.0f : 0.0f;  // Thunder only in summer
        case WEATHER_RAIN:
        case WEATHER_HEAVY_RAIN:
            // More rain in spring/autumn
            if (season == SEASON_SPRING || season == SEASON_AUTUMN) return 1.5f;
            if (season == SEASON_WINTER) return 0.5f;
            return 1.0f;
        case WEATHER_MIST:
            // Mist in spring/autumn mornings
            if (season == SEASON_SPRING || season == SEASON_AUTUMN) return 2.0f;
            return 0.5f;
        default:
            return 1.0f;
    }
}

// Pick next weather type based on transition probabilities
static WeatherType PickNextWeather(WeatherType current) {
    float weights[WEATHER_COUNT];
    float totalWeight = 0.0f;

    for (int i = 0; i < WEATHER_COUNT; i++) {
        weights[i] = transitionWeights[current][i] * GetSeasonMultiplier((WeatherType)i);
        totalWeight += weights[i];
    }

    if (totalWeight <= 0.0f) return WEATHER_CLEAR;

    float roll = ((float)(rand() % 10000)) / 10000.0f * totalWeight;
    float cumulative = 0.0f;
    for (int i = 0; i < WEATHER_COUNT; i++) {
        cumulative += weights[i];
        if (roll < cumulative) return (WeatherType)i;
    }
    return WEATHER_CLEAR;
}

// Target wind strength for each weather type
static float GetTargetWindStrength(WeatherType type) {
    switch (type) {
        case WEATHER_CLEAR:         return 0.5f;
        case WEATHER_CLOUDY:        return 1.0f;
        case WEATHER_RAIN:          return 1.5f;
        case WEATHER_HEAVY_RAIN:    return 2.5f;
        case WEATHER_THUNDERSTORM:  return 4.0f;
        case WEATHER_SNOW:          return 1.0f;
        case WEATHER_MIST:          return 0.2f;
        default:                    return 0.5f;
    }
}

// =============================================================================
// Initialization
// =============================================================================

void InitWeather(void) {
    daysPerSeason = 7;
    baseSurfaceTemp = 15;
    seasonalAmplitude = 25;
    diurnalAmplitude = 5;

    memset(&weatherState, 0, sizeof(weatherState));
    weatherState.current = WEATHER_CLEAR;
    weatherState.previous = WEATHER_CLEAR;
    float minDurGS = GameHoursToGameSeconds(weatherMinDuration);
    float maxDurGS = GameHoursToGameSeconds(weatherMaxDuration);
    weatherState.transitionTimer = minDurGS + ((float)(rand() % 1000) / 1000.0f) * (maxDurGS - minDurGS);
    weatherState.transitionDuration = weatherState.transitionTimer;
    weatherState.intensity = 1.0f;
    weatherState.windDirX = 0.0f;
    weatherState.windDirY = 0.0f;
    weatherState.windStrength = 0.0f;
    weatherState.windChangeTimer = 10.0f;

    weatherEnabled = true;
    rainWetnessAccum = 0.0f;
    weatherWindAccum = 0.0f;
    targetWindStrength = 0.0f;
    
    // Initialize lightning (Phase 5)
    lightningTimer = GameHoursToGameSeconds(lightningInterval);
    lightningFlashTimer = 0.0f;
}

// =============================================================================
// Season Calculation (unchanged from Phase 1)
// =============================================================================

int GetYearDay(void) {
    int daysPerYear = daysPerSeason * SEASON_COUNT;
    if (daysPerYear <= 0) return 0;
    return (dayNumber - 1) % daysPerYear;
}

Season GetCurrentSeason(void) {
    if (daysPerSeason <= 0) return SEASON_SPRING;
    int yearDay = GetYearDay();
    int seasonIndex = yearDay / daysPerSeason;
    if (seasonIndex >= SEASON_COUNT) seasonIndex = SEASON_COUNT - 1;
    return (Season)seasonIndex;
}

float GetSeasonProgress(void) {
    if (daysPerSeason <= 0) return 0.0f;
    int yearDay = GetYearDay();
    int dayInSeason = yearDay % daysPerSeason;
    return (float)dayInSeason / (float)daysPerSeason;
}

const char* GetSeasonName(Season s) {
    switch (s) {
        case SEASON_SPRING: return "Spring";
        case SEASON_SUMMER: return "Summer";
        case SEASON_AUTUMN: return "Autumn";
        case SEASON_WINTER: return "Winter";
        default: return "Unknown";
    }
}

// =============================================================================
// Seasonal Modulation (unchanged from Phase 1)
// =============================================================================

static float GetYearPhase(void) {
    int daysPerYear = daysPerSeason * SEASON_COUNT;
    if (daysPerYear <= 0) return 0.0f;
    return (float)GetYearDay() / (float)daysPerYear;
}

int GetSeasonalSurfaceTemp(void) {
    // When both amplitudes are 0, use ambientSurfaceTemp (backward compatible)
    if (seasonalAmplitude == 0 && diurnalAmplitude == 0) return ambientSurfaceTemp;

    // Every GetTemperature call lands here (the freezing sweep alone: ~20k cells a tick on the
    // island), and the answer only changes with the clock: two sinf per call cost ~0.4 ms a tick.
    // Remember the last answer and its inputs.
    static int lastSeasonal = -1, lastDiurnal = -1, lastBase = 0, lastDaysPerSeason = -1, lastDay = -1;
    static float lastTimeOfDay = -1.0f;
    static int lastTemp = 0;
    if (seasonalAmplitude == lastSeasonal && diurnalAmplitude == lastDiurnal && baseSurfaceTemp == lastBase &&
        daysPerSeason == lastDaysPerSeason && dayNumber == lastDay && timeOfDay == lastTimeOfDay) {
        return lastTemp;
    }

    int temp = baseSurfaceTemp;

    // Seasonal cycle: sine over the year (peak summer, trough winter)
    if (seasonalAmplitude != 0) {
        float yearPhase = GetYearPhase();
        float seasonalOffset = sinf(yearPhase * 2.0f * (float)M_PI) * (float)seasonalAmplitude;
        temp += (int)seasonalOffset;
    }

    // Diurnal cycle: sine over the day, peak at 14:00, trough at 02:00
    if (diurnalAmplitude != 0) {
        float dayPhase = (timeOfDay - 8.0f) / 24.0f; // 0 at 08:00, peak at 14:00
        float diurnalOffset = sinf(dayPhase * 2.0f * (float)M_PI) * (float)diurnalAmplitude;
        temp += (int)diurnalOffset;
    }

    lastSeasonal = seasonalAmplitude; lastDiurnal = diurnalAmplitude; lastBase = baseSurfaceTemp;
    lastDaysPerSeason = daysPerSeason; lastDay = dayNumber; lastTimeOfDay = timeOfDay;
    lastTemp = temp;
    return temp;
}

float GetSeasonalDawn(void) {
    float yearPhase = GetYearPhase();
    float offset = sinf((yearPhase - 0.25f) * 2.0f * (float)M_PI);
    return 6.5f - offset * 1.5f;
}

float GetSeasonalDusk(void) {
    float yearPhase = GetYearPhase();
    float offset = sinf((yearPhase - 0.25f) * 2.0f * (float)M_PI);
    return 18.5f + offset * 2.5f;
}

float GetVegetationGrowthRate(void) {
    if (seasonalAmplitude == 0) return 1.0f;
    float yearPhase = GetYearPhase();
    float rate = 0.75f + 0.75f * cosf((yearPhase - 0.125f) * 2.0f * (float)M_PI);
    if (rate < 0.0f) rate = 0.0f;
    return rate;
}

// =============================================================================
// Weather Names
// =============================================================================

const char* GetWeatherName(WeatherType w) {
    switch (w) {
        case WEATHER_CLEAR:         return "Clear";
        case WEATHER_CLOUDY:        return "Cloudy";
        case WEATHER_RAIN:          return "Rain";
        case WEATHER_HEAVY_RAIN:    return "Heavy Rain";
        case WEATHER_THUNDERSTORM:  return "Thunderstorm";
        case WEATHER_SNOW:          return "Snow";
        case WEATHER_MIST:          return "Mist";
        default:                    return "Unknown";
    }
}

// =============================================================================
// Roof Detection
// =============================================================================

bool IsExposedToSky(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    // Trace upward from z+1 to gridDepth-1 looking for any solid cell or floor
    for (int zz = z + 1; zz < gridDepth; zz++) {
        CellType cell = GRID(x, y, zz);
        if (cell != CELL_AIR) return false;  // Solid cell blocks sky
        if (HAS_FLOOR(x, y, zz)) return false;  // Constructed floor blocks sky
    }
    return true;
}

// =============================================================================
// Rain Wetness
// =============================================================================

// Surface sweeps (rain wetness, snow) visit every column: ~180 ms on a 2048 map. Maps with
// more than weatherSpreadMinColumns columns spread a sweep evenly over its interval (rows due
// = rows x elapsed / interval, every row still once per interval); smaller maps sweep at the
// interval's end as before. The position follows from the interval's accumulator.
int weatherSpreadMinColumns = 1 << 16;
static int WeatherRowsDue(float accum, float interval) {
    if (accum >= interval) return 0;
    if ((long long)gridWidth * gridHeight <= weatherSpreadMinColumns) return 0;
    int due = (int)((double)gridHeight * accum / interval);
    return due < 0 ? 0 : due > gridHeight ? gridHeight : due;
}
static int rainSweepRow = 0;

// Rain falls as water (docs/todo/world/soil-moisture.md): each sweep every column open to the sky
// gets one water unit (1/7 cell) with a small chance, rainWater units per cell per game day of
// rain (heavyRainWater in heavy rain and storms), times the weather's intensity. On natural soil
// with room it soaks in where it lands (at that soil's speed: clay lets most of it stand); on
// full soil, natural rock or water it lands as water and runs off downhill like any other.
// Built ground and roofs catch nothing (gutters), the sea neither.
float rainWater = 1.5f;        // under a raining cloud's core (cloudRain): less at its edge, none outside
float heavyRainWater = 4.5f;
static float rainChance = 0.0f;   // per column, this sweep
static void UpdateClouds(void);   // the cloud map drifts (below)
// Its own random numbers: a roll per column per sweep must not shift rand()'s sequence (the
// weather itself, fire, everything else would change with the map size)
static uint32_t rainRng = 0x9E3779B9u;
static float RainRand01(void) {
    rainRng ^= rainRng << 13; rainRng ^= rainRng >> 17; rainRng ^= rainRng << 5;
    return (float)(rainRng >> 8) / 16777216.0f;
}

static void RainWetnessRow(int y) {
        for (int x = 0; x < gridWidth; x++) {
            float roll = RainRand01();
            if (roll >= rainChance) continue;
            if (cloudRain && roll >= rainChance * GetCloudRain(x, y)) continue;   // not under a raining cloud
            // Find the ground z (topmost solid cell; water and floors in air cells don't count)
            for (int z = gridDepth - 1; z >= 0; z--) {
                CellType cell = GRID(x, y, z);
                if (cell == CELL_AIR) continue;
                // Found ground at z
                if (!IsExposedToSky(x, y, z)) break;      // under a roof or a floor
                if (!IsWallNatural(x, y, z)) break;       // built: gutters take it
                int wz = z + 1;
                if (wz >= gridDepth) break;
                while (wz + 1 < gridDepth && WATER(x, y, wz).level >= WATER_MAX_LEVEL) wz++;   // the water's surface
                if (waterEdgeOutflow && wz <= waterEdgeSeaZ) break;   // the sea
                if (WATER(x, y, wz).level >= WATER_MAX_LEVEL) break;
                if (wz == z + 1 && WATER(x, y, wz).level == 0 && RainSoaksIn(x, y, z)) break;
                AddWater(x, y, wz, 1);
                break;  // Only process topmost ground
            }
        }
}

static void ApplyRainWetness(void) {
    WeatherType w = weatherState.current;
    if (w != WEATHER_RAIN && w != WEATHER_HEAVY_RAIN && w != WEATHER_THUNDERSTORM) return;

    float intervalGH = (w == WEATHER_HEAVY_RAIN || w == WEATHER_THUNDERSTORM)
                     ? heavyRainWetnessInterval : rainWetnessInterval;
    float intervalGS = GameHoursToGameSeconds(intervalGH);
    float perDay = (w == WEATHER_HEAVY_RAIN || w == WEATHER_THUNDERSTORM) ? heavyRainWater : rainWater;
    rainChance = perDay * intervalGH / 24.0f * weatherState.intensity;

    rainWetnessAccum += gameDeltaTime;
    if (rainSweepRow > gridHeight) rainSweepRow = gridHeight;
    // The rows due so far; at the interval's end finish the sweep (every topmost soil cell
    // exposed to the sky gets wetter once per interval) and start the next
    for (;;) {
        int due = rainWetnessAccum >= intervalGS ? gridHeight : WeatherRowsDue(rainWetnessAccum, intervalGS);
        for (int y = rainSweepRow; y < due; y++) RainWetnessRow(y);
        if (due > rainSweepRow) rainSweepRow = due;
        if (rainWetnessAccum < intervalGS) break;
        rainWetnessAccum -= intervalGS;
        rainSweepRow = 0;
        if (rainWetnessAccum >= intervalGS) break;
    }
}

// =============================================================================
// Weather-Driven Rain Spawning
// =============================================================================

// (Weather rain used to drop sky water blobs here, SpawnSkyWater on 5-30% of the map in 4-7
// unit drops, which flooded flat maps; it falls as rainWater in RainWetnessRow now. The console
// `rain` command still drops them, and the weather leaves that alone.)

// =============================================================================
// Wind Update
// =============================================================================

static void UpdateWind(void) {
    // Smoothly approach target wind strength
    targetWindStrength = GetTargetWindStrength(weatherState.current);
    float strengthDiff = targetWindStrength - weatherState.windStrength;
    weatherState.windStrength += strengthDiff * gameDeltaTime * 0.5f;  // Smooth approach

    // Gradually shift wind direction
    weatherWindAccum += gameDeltaTime;
    if (weatherWindAccum >= weatherState.windChangeTimer) {
        weatherWindAccum -= weatherState.windChangeTimer;
        // Random walk: nudge direction
        float nudgeX = ((float)(rand() % 200) - 100.0f) / 100.0f;
        float nudgeY = ((float)(rand() % 200) - 100.0f) / 100.0f;
        weatherState.windDirX += nudgeX * 0.3f;
        weatherState.windDirY += nudgeY * 0.3f;
        // Normalize
        float len = sqrtf(weatherState.windDirX * weatherState.windDirX +
                          weatherState.windDirY * weatherState.windDirY);
        if (len > 0.01f) {
            weatherState.windDirX /= len;
            weatherState.windDirY /= len;
        } else {
            // Pick random direction if collapsed to zero
            float angle = ((float)(rand() % 628)) / 100.0f;
            weatherState.windDirX = cosf(angle);
            weatherState.windDirY = sinf(angle);
        }
        // Next wind change in 5-15 seconds
        weatherState.windChangeTimer = 5.0f + ((float)(rand() % 100)) / 10.0f;
    }
}

// =============================================================================
// Main Weather Update
// =============================================================================

void UpdateWeather(void) {
    if (!weatherEnabled) return;
    if (gameDeltaTime <= 0.0f) return;

    // Ramp intensity toward 1.0
    if (weatherState.intensity < 1.0f) {
        weatherState.intensity += RatePerGameSecond(intensityRampSpeed) * gameDeltaTime;
        if (weatherState.intensity > 1.0f) weatherState.intensity = 1.0f;
    }

    // Countdown transition timer
    weatherState.transitionTimer -= gameDeltaTime;
    if (weatherState.transitionTimer <= 0.0f) {
        // Time for a weather change
        weatherState.previous = weatherState.current;
        weatherState.current = PickNextWeather(weatherState.current);
        EventLog("Weather: %s -> %s", GetWeatherName(weatherState.previous), GetWeatherName(weatherState.current));
        float minDurGS = GameHoursToGameSeconds(weatherMinDuration);
        float maxDurGS = GameHoursToGameSeconds(weatherMaxDuration);
        weatherState.transitionTimer = minDurGS +
            ((float)(rand() % 1000)) / 1000.0f * (maxDurGS - minDurGS);
        weatherState.transitionDuration = weatherState.transitionTimer;
        weatherState.intensity = 0.0f;  // Start ramping up
    }

    // Update wind
    UpdateWind();
    UpdateClouds();

    // Rain wetness on exposed cells
    ApplyRainWetness();
}

// =============================================================================
// Wind Effects (Phase 3)
// =============================================================================

float GetWindDotProduct(int dx, int dy) {
    if (weatherState.windStrength < 0.01f) return 0.0f;
    return (weatherState.windDirX * (float)dx + weatherState.windDirY * (float)dy)
           * weatherState.windStrength;
}

float GetWindChillTemp(float baseTemp, float windStrength, bool exposed) {
    if (!exposed || windStrength < 0.01f) return baseTemp;
    // Simple wind chill: -2C per unit of wind strength
    return baseTemp - windStrength * 2.0f;
}

// =============================================================================
// Accumulator Getters/Setters (for save/load)
// =============================================================================

float GetRainWetnessAccum(void) { return rainWetnessAccum; }
float GetWeatherWindAccum(void) { return weatherWindAccum; }
void SetRainWetnessAccum(float v) {
    rainWetnessAccum = v;
    rainSweepRow = WeatherRowsDue(v, GameHoursToGameSeconds(rainWetnessInterval));   // (saved: position follows)
}
void SetWeatherWindAccum(float v) { weatherWindAccum = v; }

// =============================================================================
// SNOW SYSTEM (Phase 4)
// =============================================================================

// Snow grid: 0=none, 1=light, 2=moderate, 3=heavy
// Same layout as vegetationGrid
uint8_t* snowGrid = NULL;   // allocated in grid_layers.c

// Snow tunables (game-hours): 1/rate = game-hours per snow level change
float snowAccumulationRate = 0.25f;   // 1/0.25 = 4 GH per level (was 0.1, 1/0.1 = 10 game-seconds)
float snowMeltingRate = 0.125f;       // 1/0.125 = 8 GH per level (was 0.05, 1/0.05 = 20 game-seconds)

// Internal accumulator for snow changes
#define SNOW_TICK_HOURS 0.25f
static float snowAccum = 0.0f;

// Per-cell accumulators for deterministic snow changes
float* snowAccumGrid = NULL;   // per-cell scratch layer (allocated in grid_layers.c)
#define SNOW_ACCUM(x, y, z) (snowAccumGrid CELL_AT((x), (y), (z)))

// UpdateSnow only has work while it snows, or while snow or snowfall progress lies
// somewhere. Without these flags it searched every column for the ground several
// times a second (~2.6 ms per tick at 1024x1024) even with weather off. Conservative:
// set whenever snow may have appeared (SetSnowLevel, init, load), cleared only by a
// full pass that found none.
static bool snowMayExist = true, snowAccumMayExist = true;

void SnowGridChanged(void) { snowMayExist = snowAccumMayExist = true; }

void InitSnow(void) {
    CLEAR_GRID_LAYER(snowGrid, 0);
    CLEAR_GRID_LAYER(snowAccumGrid, 0);
    snowAccum = 0.0f;
    SnowGridChanged();
}

uint8_t GetSnowLevel(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return 0;
    }
    return SNOW(x, y, z);
}

void SetSnowLevel(int x, int y, int z, uint8_t level) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return;
    }
    if (level > 3) level = 3;  // Clamp to max level
    SNOW(x, y, z) = level;
    if (level > 0) snowMayExist = true;
}

// One row of the snow sweep: the topmost solid cell of each column grows, melts or resets
static void SnowRow(int y, float elapsedTime, bool isSnowing, bool isFreezing, bool* anySnow, bool* anyAccum) {
        for (int x = 0; x < gridWidth; x++) {
            // Find topmost solid cell. Water is not a cell type, so a sea or lake is air with
            // water in it: snow doesn't settle through it onto the bed below, and snow already
            // there (water rose over snowy ground) is gone. Ice you can walk on (deep frozen
            // water) is the surface itself: snow lies on it, and goes when it thaws.
            bool underWater = false;
            for (int z = gridDepth - 1; z >= 0; z--) {
                CellType cell = GRID(x, y, z);
                if (cell == CELL_AIR) {
                    int wl = GetWaterLevel(x, y, z);
                    if (wl == 0) continue;
                    bool ice = !underWater && wl >= WATER_BLOCKS_MOVEMENT && IsWaterFrozen(x, y, z);
                    if (!ice) {
                        underWater = true;
                        if (SNOW(x, y, z) > 0) SetSnowLevel(x, y, z, 0);   // the ice under it thawed
                        if (SNOW_ACCUM(x, y, z) != 0.0f) SNOW_ACCUM(x, y, z) = 0.0f;
                        continue;
                    }
                    // ice: this cell is the surface, handled as ground below
                } else if (underWater) {
                    if (SNOW(x, y, z) > 0) SetSnowLevel(x, y, z, 0);
                    if (SNOW_ACCUM(x, y, z) != 0.0f) SNOW_ACCUM(x, y, z) = 0.0f;
                    break;
                }
                
                // Found ground at z
                bool exposed = IsExposedToSky(x, y, z);
                uint8_t currentSnow = GetSnowLevel(x, y, z);
                
                // Snow accumulation
                if (isSnowing && exposed && isFreezing && currentSnow < 3) {
                    SNOW_ACCUM(x, y, z) += elapsedTime * weatherState.intensity;
                    float threshold = GameHoursToGameSeconds(1.0f / snowAccumulationRate);
                    if (SNOW_ACCUM(x, y, z) >= threshold) {
                        SNOW_ACCUM(x, y, z) = 0.0f;
                        SetSnowLevel(x, y, z, currentSnow + 1);
                    }
                }
                
                // Snow melting
                if (!isFreezing && currentSnow > 0) {
                    SNOW_ACCUM(x, y, z) += elapsedTime;
                    float threshold = GameHoursToGameSeconds(1.0f / snowMeltingRate);
                    if (SNOW_ACCUM(x, y, z) >= threshold) {
                        SNOW_ACCUM(x, y, z) = 0.0f;
                        SetSnowLevel(x, y, z, currentSnow - 1);
                        // Melted snow soaks into the soil (moisture.c)
                        WetSoilAt(x, y, z, MOISTURE_RAIN_STEP);
                    }
                }
                
                // Reset accumulator if conditions don't match (only write if set: a write
                // per column would dirty the memory of every chunk)
                if ((!isSnowing || !exposed || !isFreezing) && isFreezing && SNOW_ACCUM(x, y, z) != 0.0f) {
                    SNOW_ACCUM(x, y, z) = 0.0f;
                }
                if (SNOW(x, y, z) > 0) *anySnow = true;
                if (SNOW_ACCUM(x, y, z) != 0.0f) *anyAccum = true;
                
                break;  // Only process topmost solid cell
            }
        }
}

static int snowSweepRow = 0;
static bool snowCycleAnySnow = false, snowCycleAnyAccum = false;

void UpdateSnow(void) {
    if (gameDeltaTime <= 0.0f) return;
    
    snowAccum += gameDeltaTime;
    // Every 0.25 game-hours: a snow level takes 4 (grow) / 8 (melt) game-hours, so finer
    // steps change nothing visible; at 0.04 the column sweep ran every 0.1 s (~28 ms per tick
    // on a 2048 map all winter)
    float snowTickGS = GameHoursToGameSeconds(SNOW_TICK_HOURS);
    WeatherType w = weatherState.current;
    bool isSnowing = (w == WEATHER_SNOW);
    int ambientTemp = GetAmbientTemperature(0);  // Surface temperature
    bool isFreezing = (ambientTemp <= 0);

    if ((long long)gridWidth * gridHeight <= weatherSpreadMinColumns) {
        // Small map: the whole sweep at the interval's end (time since the last sweep)
        if (snowAccum < snowTickGS) return;
        float elapsedTime = snowAccum;
        snowAccum = 0.0f;
        if (!isSnowing && !snowMayExist && !snowAccumMayExist) return;   // nothing to grow, melt or reset
        bool anySnow = false, anyAccum = false;
        for (int y = 0; y < gridHeight; y++) SnowRow(y, elapsedTime, isSnowing, isFreezing, &anySnow, &anyAccum);
        // (only topmost cells are ever processed here, so snow elsewhere doesn't count)
        snowMayExist = anySnow;
        snowAccumMayExist = anyAccum;
        return;
    }

    // Big map: rows due so far (each row once per interval, an interval's time each)
    if (!isSnowing && !snowMayExist && !snowAccumMayExist) {   // nothing to grow, melt or reset
        if (snowAccum >= snowTickGS) snowAccum = 0.0f;
        snowSweepRow = 0;
        snowCycleAnySnow = snowCycleAnyAccum = false;
        return;
    }
    if (snowSweepRow > gridHeight) snowSweepRow = gridHeight;
    for (;;) {
        int due = snowAccum >= snowTickGS ? gridHeight : WeatherRowsDue(snowAccum, snowTickGS);
        for (int y = snowSweepRow; y < due; y++)
            SnowRow(y, snowTickGS, isSnowing, isFreezing, &snowCycleAnySnow, &snowCycleAnyAccum);
        if (due > snowSweepRow) snowSweepRow = due;
        if (snowAccum < snowTickGS) break;
        snowAccum -= snowTickGS;
        snowSweepRow = 0;
        snowMayExist = snowCycleAnySnow;
        snowAccumMayExist = snowCycleAnyAccum;
        snowCycleAnySnow = snowCycleAnyAccum = false;
        if (snowAccum >= snowTickGS) { snowAccum = 0.0f; break; }   // (behind by a whole interval: drop it, as before)
    }
}

float GetSnowSpeedMultiplier(int x, int y, int z) {
    uint8_t snow = GetSnowLevel(x, y, z);
    switch (snow) {
        case 0: return 1.0f;   // No snow
        case 1: return 0.85f;  // Light snow
        case 2: return 0.75f;  // Moderate snow
        case 3: return 0.6f;   // Heavy snow
        default: return 1.0f;
    }
}

// =============================================================================
// CLOUD SHADOWS (Phase 4)
// =============================================================================

// The cloud map (docs/todo/world/soil-moisture.md "Clouds"): one invisible field of cloud
// density, 0..1 per cell, that drifts with the wind and slowly changes shape. The weather state
// sets how much of the sky it covers; rain falls only under the dense part of a cloud (cloudRain)
// and the cloud shadows are drawn from it, so you see a cloud by its shadow and by the wet
// ground it leaves. Value noise from a hash (no shared state with the terrain's Perlin), three
// layers, each drifting a little differently so the shapes change as they go.
float cloudSize = 40.0f;    // cells across a typical cloud
float cloudSpeed = 4.0f;    // cells per game hour at wind strength 1 (a calm day still drifts)
bool cloudRain = true;      // off: rain falls everywhere evenly
static float cloudOffX[3], cloudOffY[3];   // each layer's drift, cells

static float CloudHash(int x, int y, int layer) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)layer * 2246822519u + (uint32_t)worldSeed * 3266489917u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (float)(h & 0xFFFFFF) / 16777216.0f;
}

static float CloudValueNoise(float x, float y, int layer) {
    float fx = floorf(x), fy = floorf(y);
    int ix = (int)fx, iy = (int)fy;
    float tx = x - fx, ty = y - fy;
    tx = tx * tx * (3.0f - 2.0f * tx);
    ty = ty * ty * (3.0f - 2.0f * ty);
    float a = CloudHash(ix, iy, layer), b = CloudHash(ix + 1, iy, layer);
    float c = CloudHash(ix, iy + 1, layer), d = CloudHash(ix + 1, iy + 1, layer);
    return (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty;
}

// How much of the sky this weather covers (0 = none)
static float CloudCover(WeatherType w) {
    switch (w) {
        case WEATHER_CLOUDY:       return 0.55f;
        case WEATHER_RAIN:         return 0.6f;
        case WEATHER_HEAVY_RAIN:   return 0.75f;
        case WEATHER_THUNDERSTORM: return 0.7f;
        case WEATHER_SNOW:         return 0.6f;
        case WEATHER_MIST:         return 0.35f;
        default:                   return 0.0f;
    }
}

float GetCloudDensity(int x, int y) {
    float cover = CloudCover(weatherState.current);
    if (cover <= 0.0f) return 0.0f;
    float s = cloudSize > 1.0f ? cloudSize : 1.0f;
    float n = 0.55f * CloudValueNoise((x + cloudOffX[0]) / s, (y + cloudOffY[0]) / s, 0)
            + 0.30f * CloudValueNoise((x + cloudOffX[1]) * 2.0f / s, (y + cloudOffY[1]) * 2.0f / s, 1)
            + 0.15f * CloudValueNoise((x + cloudOffX[2]) * 4.0f / s, (y + cloudOffY[2]) * 4.0f / s, 2);
    float t0 = 0.75f - 0.5f * cover;   // the more cover, the lower the noise a cloud needs
    float d = (n - t0) / 0.2f;
    return d < 0.0f ? 0.0f : d > 1.0f ? 1.0f : d;
}

// Rain under a cloud: none at its thin edge, full at its dense core
float GetCloudRain(int x, int y) {
    if (!cloudRain) return 1.0f;
    float d = GetCloudDensity(x, y);
    float r = (d - 0.4f) / 0.6f;
    return r < 0.0f ? 0.0f : r;
}

// The layers drift with the wind, each at its own speed and a little off its direction
static void UpdateClouds(void) {
    float wind = 0.3f + weatherState.windStrength;
    float cellsPerSec = cloudSpeed * wind / GameHoursToGameSeconds(1.0f);
    static const float rate[3] = { 1.0f, 1.3f, 1.7f }, skew[3] = { 0.0f, 0.35f, -0.5f };
    for (int i = 0; i < 3; i++) {
        float dx = weatherState.windDirX - skew[i] * weatherState.windDirY;
        float dy = weatherState.windDirY + skew[i] * weatherState.windDirX;
        // move the noise against the wind: what is upwind now arrives here later
        cloudOffX[i] -= dx * cellsPerSec * rate[i] * gameDeltaTime;
        cloudOffY[i] -= dy * cellsPerSec * rate[i] * gameDeltaTime;
        if (fabsf(cloudOffX[i]) > 1.0e6f) cloudOffX[i] = 0.0f;   // keep floats precise
        if (fabsf(cloudOffY[i]) > 1.0e6f) cloudOffY[i] = 0.0f;
    }
}

float GetCloudShadow(int x, int y, float time) {
    (void)time;   // the clouds' position is their drift (UpdateClouds), not the clock
    float d = GetCloudDensity(x, y);
    if (d <= 0.0f) return 0.0f;
    float darkest;
    switch (weatherState.current) {
        case WEATHER_CLOUDY:        darkest = 0.4f; break;
        case WEATHER_RAIN:          darkest = 0.6f; break;
        case WEATHER_HEAVY_RAIN:    darkest = 0.7f; break;
        case WEATHER_THUNDERSTORM:  darkest = 0.8f; break;
        case WEATHER_SNOW:          darkest = 0.5f; break;
        case WEATHER_MIST:          darkest = 0.25f; break;
        default:                    darkest = 0.0f; break;
    }
    return d * darkest;
}

// =============================================================================
// LIGHTNING SYSTEM (Phase 5)
// =============================================================================

float lightningInterval = 2.0f;  // Default 2.0 game-hours between strikes

void SetLightningInterval(float gameHours) {
    lightningInterval = gameHours;
}

void ResetLightningTimer(void) {
    lightningTimer = GameHoursToGameSeconds(lightningInterval);
}

void TriggerScreenShake(float intensity, float duration);   // main.c (a stub in the test build)

void TriggerLightningFlash(void) {
    lightningFlashTimer = 1.0f;  // Start at max intensity
}

float GetLightningFlashIntensity(void) {
    return lightningFlashTimer;
}

void UpdateLightningFlash(float dt) {
    if (lightningFlashTimer > 0.0f) {
        lightningFlashTimer -= dt * 5.0f;  // Decay fast (5x speed)
        if (lightningFlashTimer < 0.0f) {
            lightningFlashTimer = 0.0f;
        }
    }
}

// Check if a cell is flammable (wood, vegetation, etc.)
static bool IsFlammableMaterial(MaterialType mat) {
    switch (mat) {
        case MAT_OAK:
        case MAT_PINE:
        case MAT_BIRCH:
        case MAT_WILLOW:
            return true;
        default:
            return false;
    }
}

// Attempt to strike lightning at a random exposed flammable cell
static void TryLightningStrike(void) {
    // Build list of valid strike targets (exposed + flammable)
    typedef struct { int x, y, z; } Pos;
    static Pos candidates[1024];  // Max candidates
    int candidateCount = 0;
    
    for (int y = 0; y < gridHeight && candidateCount < 1024; y++) {
        for (int x = 0; x < gridWidth && candidateCount < 1024; x++) {
            // Scan from top down to find the highest flammable surface
            for (int z = gridDepth - 1; z >= 0; z--) {
                CellType cell = GRID(x, y, z);
                
                // Check if exposed to sky
                if (!IsExposedToSky(x, y, z)) {
                    // If not exposed at this level, nothing below will be exposed either
                    break;
                }
                
                // Check for flammable materials at this level
                bool flammable = false;
                
                // Check solid cell material
                if (cell == CELL_WALL || cell >= CELL_TREE_TRUNK) {
                    MaterialType wallMat = GetWallMaterial(x, y, z);
                    flammable = IsFlammableMaterial(wallMat);
                }
                
                // Check floor on AIR or solid cells
                if (!flammable && HAS_FLOOR(x, y, z)) {
                    MaterialType floorMat = GetFloorMaterial(x, y, z);
                    flammable = IsFlammableMaterial(floorMat);
                }
                
                // If this level is flammable and exposed, add it as candidate
                if (flammable) {
                    candidates[candidateCount].x = x;
                    candidates[candidateCount].y = y;
                    candidates[candidateCount].z = z;
                    candidateCount++;
                    break;  // Found the topmost flammable surface for this column
                }
                
                // If we hit a solid cell, stop scanning down (can't strike through it)
                if (cell != CELL_AIR) {
                    break;
                }
            }
        }
    }
    
    if (candidateCount == 0) return;  // No valid targets
    
    // Pick random candidate and ignite
    int idx = rand() % candidateCount;
    int x = candidates[idx].x;
    int y = candidates[idx].y;
    int z = candidates[idx].z;
    
    // Ignite via fire system
    extern void SetFireLevel(int x, int y, int z, int level);
    SetFireLevel(x, y, z, 5);  // Start with level 5 fire
    
    // Trigger flash, and the thunder shakes the screen a little (heard everywhere, like the flash)
    TriggerLightningFlash();
    TriggerScreenShake(3.0f, 0.35f);
}

void UpdateLightning(float dt) {
    if (dt <= 0.0f) return;
    
    // Update flash decay first (before potentially creating a new flash)
    UpdateLightningFlash(dt);
    
    // Only strike during thunderstorms
    if (weatherState.current != WEATHER_THUNDERSTORM) {
        lightningTimer = GameHoursToGameSeconds(lightningInterval);  // Reset timer when not storming
        return;
    }

    lightningTimer -= dt;
    if (lightningTimer <= 0.0f) {
        lightningTimer = GameHoursToGameSeconds(lightningInterval);  // Reset for next strike
        TryLightningStrike();  // This triggers a new flash
    }
}

// =============================================================================
// MIST SYSTEM (Phase 5)
// =============================================================================

float GetMistIntensity(void) {
    WeatherType w = weatherState.current;
    
    float baseMist = 0.0f;
    switch (w) {
        case WEATHER_MIST:          baseMist = 0.9f; break;
        case WEATHER_RAIN:          baseMist = 0.15f; break;
        case WEATHER_HEAVY_RAIN:    baseMist = 0.25f; break;
        case WEATHER_THUNDERSTORM:  baseMist = 0.2f; break;
        case WEATHER_SNOW:          baseMist = 0.1f; break;
        default:                    baseMist = 0.0f; break;
    }
    
    // Modulate by time of day (more mist at dawn/dusk)
    float hour = timeOfDay;
    float dawn = GetSeasonalDawn();
    float dusk = GetSeasonalDusk();
    float timeModulator = 1.0f;
    
    // More mist near dawn/dusk
    if (hour >= dawn - 1.0f && hour <= dawn + 1.0f) {
        // Near dawn: ramp up then down
        float t = (hour - (dawn - 1.0f)) / 2.0f;  // 0.0 to 1.0
        timeModulator = 1.0f + 0.5f * sinf(t * (float)M_PI);  // Peak at dawn
    } else if (hour >= dusk - 1.0f && hour <= dusk + 1.0f) {
        // Near dusk: ramp up then down
        float t = (hour - (dusk - 1.0f)) / 2.0f;
        timeModulator = 1.0f + 0.5f * sinf(t * (float)M_PI);  // Peak at dusk
    } else if (hour < dawn || hour > dusk) {
        // Night time: slight reduction
        timeModulator = 0.8f;
    }
    
    return baseMist * timeModulator * weatherState.intensity;
}
