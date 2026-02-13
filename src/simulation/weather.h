#ifndef WEATHER_H
#define WEATHER_H

#include <stdbool.h>
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

#endif // WEATHER_H
