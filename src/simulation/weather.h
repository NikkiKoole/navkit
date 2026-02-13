#ifndef WEATHER_H
#define WEATHER_H

#include <stdbool.h>
#include <stdint.h>
#include "../core/time.h"

// =============================================================================
// SEASONS
// =============================================================================

typedef enum {
    SEASON_SPRING = 0,
    SEASON_SUMMER,
    SEASON_AUTUMN,
    SEASON_WINTER,
    SEASON_COUNT
} Season;

// Configurable parameters (saved via SETTINGS_TABLE)
extern int daysPerSeason;           // Days per season (default 7, year = 4 * daysPerSeason)
extern int baseSurfaceTemp;         // Base surface temperature in Celsius (default 15)
extern int seasonalAmplitude;       // Temperature swing above/below base (default 20)

// Season calculation (pure functions, derived from dayNumber)
int GetYearDay(void);               // (dayNumber-1) % (daysPerSeason*4), range 0..daysPerYear-1
Season GetCurrentSeason(void);      // yearDay / daysPerSeason
float GetSeasonProgress(void);      // (yearDay % daysPerSeason) / daysPerSeason, range 0.0..~1.0
const char* GetSeasonName(Season s);

// Seasonal modulation
int GetSeasonalSurfaceTemp(void);   // baseSurfaceTemp + sine curve offset
float GetSeasonalDawn(void);        // Dawn hour (5.0 in summer, 8.0 in winter)
float GetSeasonalDusk(void);        // Dusk hour (21.0 in summer, 16.0 in winter)
float GetVegetationGrowthRate(void); // 0.0 (winter) to 1.5 (spring)

// =============================================================================
// WEATHER
// =============================================================================

typedef enum {
    WEATHER_CLEAR = 0,
    WEATHER_CLOUDY,
    WEATHER_RAIN,
    WEATHER_HEAVY_RAIN,
    WEATHER_THUNDERSTORM,
    WEATHER_SNOW,
    WEATHER_MIST,
    WEATHER_COUNT
} WeatherType;

typedef struct {
    WeatherType current;
    WeatherType previous;
    float transitionTimer;      // Countdown to next weather change (game-seconds)
    float transitionDuration;   // Total duration of current weather
    float intensity;            // 0.0-1.0, ramps up after transition

    // Wind (single global vector)
    float windDirX, windDirY;   // Normalized direction
    float windStrength;         // 0=calm, 1=breeze, 2=wind, 3=strong, 4=storm
    float windChangeTimer;      // Timer for gradual wind shifts
} WeatherState;

extern WeatherState weatherState;

// Weather tunables (saved via SETTINGS_TABLE)
extern bool weatherEnabled;
extern float weatherMinDuration;        // Min game-seconds per weather (default 30)
extern float weatherMaxDuration;        // Max game-seconds per weather (default 120)
extern float rainWetnessInterval;       // Game-seconds between wetness increments during rain (default 5)
extern float heavyRainWetnessInterval;  // Faster for heavy rain (default 2)
extern float intensityRampSpeed;        // How fast intensity ramps to 1.0 (default 0.2 per second)

// Weather API
void InitWeather(void);
void UpdateWeather(void);               // Call from TickWithDt, before UpdateRain
const char* GetWeatherName(WeatherType w);
bool IsExposedToSky(int x, int y, int z);  // True if no solid cell/floor above

// =============================================================================
// WIND EFFECTS (Phase 3)
// =============================================================================

// Wind dot product: how aligned is direction (dx,dy) with wind? Scaled by strength.
// Positive = downwind, negative = upwind, zero = perpendicular or no wind.
float GetWindDotProduct(int dx, int dy);

// Wind chill: effective temperature accounting for wind exposure.
// exposed=false means sheltered (no chill applied).
float GetWindChillTemp(float baseTemp, float windStrength, bool exposed);

// Wind drying multiplier: how much faster wetness dries due to wind.
// Returns 1.0 with no wind, >1.0 with wind on exposed cells.
extern float windDryingMultiplier;

// Accumulator getters/setters (for save/load)
float GetRainWetnessAccum(void);
float GetWeatherWindAccum(void);
void SetRainWetnessAccum(float v);
void SetWeatherWindAccum(float v);

// =============================================================================
// SNOW SYSTEM (Phase 4)
// =============================================================================

// Snow levels: 0=none, 1=light, 2=moderate, 3=heavy
// Stored in separate snowGrid (uint8_t 3D array, like vegetationGrid)

void InitSnow(void);                    // Initialize snow grid to zero
uint8_t GetSnowLevel(int x, int y, int z);
void SetSnowLevel(int x, int y, int z, uint8_t level);
void UpdateSnow(void);                  // Accumulation + melting logic
float GetSnowSpeedMultiplier(int x, int y, int z);  // Movement penalty: 1.0 (none) to 0.6 (heavy)

// Snow tunables (saved via SETTINGS_TABLE)
extern float snowAccumulationRate;      // How fast snow accumulates during WEATHER_SNOW (default 0.01)
extern float snowMeltingRate;           // How fast snow melts above freezing (default 0.005)

// =============================================================================
// CLOUD SHADOWS (Phase 4)
// =============================================================================

// Returns shadow intensity [0.0, 1.0] for a given cell and time
// 0.0 = no shadow (full brightness), 1.0 = maximum shadow (darkest)
// time parameter is game time (gameTime) for scrolling effect
float GetCloudShadow(int x, int y, float time);

// =============================================================================
// THUNDERSTORMS + MIST (Phase 5)
// =============================================================================

// Lightning tunables (saved via SETTINGS_TABLE)
extern float lightningInterval;         // Game-seconds between lightning strikes (default 5.0)

// Lightning API
void UpdateLightning(float dt);         // Call from UpdateWeather or separately
void ResetLightningTimer(void);         // Force immediate strike check (for tests)
void SetLightningInterval(float seconds);
float GetLightningFlashIntensity(void);  // 0.0-1.0, for visual flash effect
void UpdateLightningFlash(float dt);     // Decay flash over time
void TriggerLightningFlash(void);        // Manually trigger flash (for tests)

// Mist API
float GetMistIntensity(void);            // 0.0-1.0 based on weather + time of day

#endif // WEATHER_H
