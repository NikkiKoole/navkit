#include "weather.h"
#include "temperature.h"
#include "water.h"
#include "../world/grid.h"
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
int baseSurfaceTemp = TEMP_AMBIENT_DEFAULT;  // 20, matching temperature system default
int seasonalAmplitude = 0;                   // 0 by default (flat temp, no seasons)

// =============================================================================
// Weather State
// =============================================================================

WeatherState weatherState;
bool weatherEnabled = false;  // Off by default; InitWeather() enables it
float weatherMinDuration = 30.0f;
float weatherMaxDuration = 120.0f;
float rainWetnessInterval = 5.0f;
float heavyRainWetnessInterval = 2.0f;
float intensityRampSpeed = 0.2f;

float windDryingMultiplier = 1.5f;  // How much wind accelerates drying (default 1.5x at strength 3)

// Internal accumulators
static float rainWetnessAccum = 0.0f;
static float weatherWindAccum = 0.0f;

// Target wind values (wind smoothly approaches these)
static float targetWindStrength = 0.0f;

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
    baseSurfaceTemp = TEMP_AMBIENT_DEFAULT;
    seasonalAmplitude = 0;

    memset(&weatherState, 0, sizeof(weatherState));
    weatherState.current = WEATHER_CLEAR;
    weatherState.previous = WEATHER_CLEAR;
    weatherState.transitionTimer = weatherMinDuration + ((float)(rand() % 1000) / 1000.0f) * (weatherMaxDuration - weatherMinDuration);
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
    if (seasonalAmplitude == 0) return ambientSurfaceTemp;
    float yearPhase = GetYearPhase();
    float offset = sinf((yearPhase - 0.25f) * 2.0f * (float)M_PI) * (float)seasonalAmplitude;
    return baseSurfaceTemp + (int)offset;
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
        CellType cell = grid[zz][y][x];
        if (cell != CELL_AIR) return false;  // Solid cell blocks sky
        if (HAS_FLOOR(x, y, zz)) return false;  // Constructed floor blocks sky
    }
    return true;
}

// =============================================================================
// Rain Wetness
// =============================================================================

static void ApplyRainWetness(void) {
    WeatherType w = weatherState.current;
    if (w != WEATHER_RAIN && w != WEATHER_HEAVY_RAIN && w != WEATHER_THUNDERSTORM) return;

    float interval = (w == WEATHER_HEAVY_RAIN || w == WEATHER_THUNDERSTORM)
                     ? heavyRainWetnessInterval : rainWetnessInterval;

    rainWetnessAccum += gameDeltaTime;
    if (rainWetnessAccum < interval) return;
    rainWetnessAccum -= interval;

    // Iterate surface cells: find topmost solid ground for each (x,y)
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            // Find the ground z (topmost solid cell)
            for (int z = gridDepth - 1; z >= 0; z--) {
                CellType cell = grid[z][y][x];
                if (cell == CELL_AIR) continue;
                // Found ground at z
                if (!IsSoilMaterial(GetWallMaterial(x, y, z))) break;
                if (!IsExposedToSky(x, y, z)) break;
                // Increase wetness
                int wetness = GET_CELL_WETNESS(x, y, z);
                if (wetness < 3) {
                    SET_CELL_WETNESS(x, y, z, wetness + 1);
                }
                break;  // Only process topmost ground
            }
        }
    }
}

// =============================================================================
// Weather-Driven Rain Spawning
// =============================================================================

static void ManageRainSpawning(void) {
    WeatherType w = weatherState.current;
    bool shouldRain = (w == WEATHER_RAIN || w == WEATHER_HEAVY_RAIN || w == WEATHER_THUNDERSTORM);

    if (shouldRain && !IsRaining()) {
        int coverage = 5;
        if (w == WEATHER_HEAVY_RAIN || w == WEATHER_THUNDERSTORM) coverage = 30;
        SpawnSkyWater(coverage);
    } else if (!shouldRain && IsRaining()) {
        StopRain();
    }
}

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
        weatherState.intensity += intensityRampSpeed * gameDeltaTime;
        if (weatherState.intensity > 1.0f) weatherState.intensity = 1.0f;
    }

    // Countdown transition timer
    weatherState.transitionTimer -= gameDeltaTime;
    if (weatherState.transitionTimer <= 0.0f) {
        // Time for a weather change
        weatherState.previous = weatherState.current;
        weatherState.current = PickNextWeather(weatherState.current);
        weatherState.transitionTimer = weatherMinDuration +
            ((float)(rand() % 1000)) / 1000.0f * (weatherMaxDuration - weatherMinDuration);
        weatherState.transitionDuration = weatherState.transitionTimer;
        weatherState.intensity = 0.0f;  // Start ramping up
    }

    // Update wind
    UpdateWind();

    // Rain wetness on exposed cells
    ApplyRainWetness();

    // Manage rain water spawning (connect to existing water.c rain system)
    ManageRainSpawning();
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
void SetRainWetnessAccum(float v) { rainWetnessAccum = v; }
void SetWeatherWindAccum(float v) { weatherWindAccum = v; }
