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
int baseSurfaceTemp = 15;  // Center of seasonal range: 15 ± 25 = -10C to 40C
int seasonalAmplitude = 25;                  // Temp swing ±25 from base (winter ~-5C, summer ~45C)

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
    
    // Initialize lightning (Phase 5)
    lightningTimer = lightningInterval;
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
    if (seasonalAmplitude == 0) return ambientSurfaceTemp;
    float yearPhase = GetYearPhase();
    float offset = sinf(yearPhase * 2.0f * (float)M_PI) * (float)seasonalAmplitude;
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

// =============================================================================
// SNOW SYSTEM (Phase 4)
// =============================================================================

// Snow grid: 0=none, 1=light, 2=moderate, 3=heavy
// Same layout as vegetationGrid
uint8_t snowGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Snow tunables
float snowAccumulationRate = 0.1f;    // Seconds per snow level increase (default 0.1 = 10s per level)
float snowMeltingRate = 0.05f;        // Seconds per snow level decrease (default 0.05 = 20s per level)

// Internal accumulator for snow changes
static float snowAccum = 0.0f;

// Per-cell accumulators for deterministic snow changes
static float snowAccumGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

void InitSnow(void) {
    memset(snowGrid, 0, sizeof(snowGrid));
    memset(snowAccumGrid, 0, sizeof(snowAccumGrid));
    snowAccum = 0.0f;
}

uint8_t GetSnowLevel(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return 0;
    }
    return snowGrid[z][y][x];
}

void SetSnowLevel(int x, int y, int z, uint8_t level) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return;
    }
    if (level > 3) level = 3;  // Clamp to max level
    snowGrid[z][y][x] = level;
}

void UpdateSnow(void) {
    if (gameDeltaTime <= 0.0f) return;
    
    snowAccum += gameDeltaTime;
    if (snowAccum < 0.1f) return;  // Update every 0.1 seconds
    float elapsedTime = snowAccum;
    snowAccum = 0.0f;
    
    WeatherType w = weatherState.current;
    bool isSnowing = (w == WEATHER_SNOW);
    int ambientTemp = GetAmbientTemperature(0);  // Surface temperature
    bool isFreezing = (ambientTemp <= 0);
    
    // Iterate surface cells
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            // Find topmost solid cell
            for (int z = gridDepth - 1; z >= 0; z--) {
                CellType cell = grid[z][y][x];
                if (cell == CELL_AIR) continue;
                
                // Found ground at z
                bool exposed = IsExposedToSky(x, y, z);
                uint8_t currentSnow = GetSnowLevel(x, y, z);
                
                // Snow accumulation
                if (isSnowing && exposed && isFreezing && currentSnow < 3) {
                    snowAccumGrid[z][y][x] += elapsedTime * weatherState.intensity;
                    float threshold = 1.0f / snowAccumulationRate;  // e.g., 0.1 -> 10 seconds per level
                    if (snowAccumGrid[z][y][x] >= threshold) {
                        snowAccumGrid[z][y][x] = 0.0f;
                        SetSnowLevel(x, y, z, currentSnow + 1);
                    }
                }
                
                // Snow melting
                if (!isFreezing && currentSnow > 0) {
                    snowAccumGrid[z][y][x] += elapsedTime;
                    float threshold = 1.0f / snowMeltingRate;  // e.g., 0.05 -> 20 seconds per level
                    if (snowAccumGrid[z][y][x] >= threshold) {
                        snowAccumGrid[z][y][x] = 0.0f;
                        SetSnowLevel(x, y, z, currentSnow - 1);
                        // Add wetness from melted snow
                        int wetness = GET_CELL_WETNESS(x, y, z);
                        if (wetness < 3) {
                            SET_CELL_WETNESS(x, y, z, wetness + 1);
                        }
                    }
                }
                
                // Reset accumulator if conditions don't match
                if ((!isSnowing || !exposed || !isFreezing) && isFreezing) {
                    snowAccumGrid[z][y][x] = 0.0f;
                }
                
                break;  // Only process topmost solid cell
            }
        }
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

// Simple drifting rectangular shadow patches.
// Placeholder approach: a handful of fixed-size rectangles that scroll with
// the wind.  Looks intentionally placeholder but reads clearly as "clouds".

#define CLOUD_PATCH_COUNT 6

typedef struct {
    float cx, cy;   // base center in world-cell coords
    float hw, hh;   // half-width / half-height in cells
} CloudPatch;

// Deterministic patches seeded from grid size
static const CloudPatch cloudPatches[CLOUD_PATCH_COUNT] = {
    {  20.0f,  15.0f, 12.0f,  8.0f },
    {  70.0f,  45.0f, 10.0f, 14.0f },
    {  40.0f,  80.0f, 15.0f,  9.0f },
    { 110.0f,  25.0f,  8.0f, 11.0f },
    {  85.0f,  90.0f, 13.0f,  7.0f },
    {  55.0f,  55.0f,  9.0f, 12.0f },
};

float GetCloudShadow(int x, int y, float time) {
    WeatherType w = weatherState.current;

    // Base intensity by weather type
    float baseIntensity = 0.0f;
    switch (w) {
        case WEATHER_CLEAR:         baseIntensity = 0.0f; break;
        case WEATHER_CLOUDY:        baseIntensity = 0.3f; break;
        case WEATHER_RAIN:          baseIntensity = 0.5f; break;
        case WEATHER_HEAVY_RAIN:    baseIntensity = 0.6f; break;
        case WEATHER_THUNDERSTORM:  baseIntensity = 0.7f; break;
        case WEATHER_SNOW:          baseIntensity = 0.4f; break;
        case WEATHER_MIST:          baseIntensity = 0.2f; break;
        default:                    baseIntensity = 0.0f; break;
    }

    if (baseIntensity < 0.01f) return 0.0f;

    // Scroll with wind
    float scrollX = time * weatherState.windDirX * weatherState.windStrength * 0.5f;
    float scrollY = time * weatherState.windDirY * weatherState.windStrength * 0.5f;

    float maxShadow = 0.0f;
    float fx = (float)x;
    float fy = (float)y;

    for (int i = 0; i < CLOUD_PATCH_COUNT; i++) {
        const CloudPatch* p = &cloudPatches[i];

        // Each patch drifts at a slightly different rate for variety
        float rate = 1.0f + (float)i * 0.15f;
        float pcx = fmodf(p->cx + scrollX * rate, (float)gridWidth  + p->hw * 4.0f) - p->hw * 2.0f;
        float pcy = fmodf(p->cy + scrollY * rate, (float)gridHeight + p->hh * 4.0f) - p->hh * 2.0f;

        // Soft-edged rectangle: full shadow inside, linear falloff at edges
        float dx = fabsf(fx - pcx) - p->hw;
        float dy = fabsf(fy - pcy) - p->hh;
        if (dx > 4.0f || dy > 4.0f) continue;  // 4-cell soft edge

        float sx = dx < 0.0f ? 1.0f : 1.0f - dx / 4.0f;
        float sy = dy < 0.0f ? 1.0f : 1.0f - dy / 4.0f;
        float s = sx * sy;
        if (s > maxShadow) maxShadow = s;
    }

    return maxShadow * baseIntensity;
}

// =============================================================================
// LIGHTNING SYSTEM (Phase 5)
// =============================================================================

float lightningInterval = 5.0f;  // Default 5 seconds between strikes

void SetLightningInterval(float seconds) {
    lightningInterval = seconds;
}

void ResetLightningTimer(void) {
    lightningTimer = lightningInterval;
}

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
                CellType cell = grid[z][y][x];
                
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
    
    // Trigger flash
    TriggerLightningFlash();
}

void UpdateLightning(float dt) {
    if (dt <= 0.0f) return;
    
    // Update flash decay first (before potentially creating a new flash)
    UpdateLightningFlash(dt);
    
    // Only strike during thunderstorms
    if (weatherState.current != WEATHER_THUNDERSTORM) {
        lightningTimer = lightningInterval;  // Reset timer when not storming
        return;
    }
    
    lightningTimer -= dt;
    if (lightningTimer <= 0.0f) {
        lightningTimer = lightningInterval;  // Reset for next strike
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
