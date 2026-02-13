#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/core/time.h"
#include "../src/entities/mover.h"
#include "../src/simulation/weather.h"
#include "../src/simulation/water.h"
#include "../src/simulation/temperature.h"
#include "../src/world/grid.h"
#include "test_helpers.h"
#include "../src/world/cell_defs.h"
#include "../src/world/material.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static bool test_verbose = false;

// Helper: set up a 3-level grid (z=0 dirt ground, z=1 air, z=2 air)
// gridDepth=3 so we have room for roofs
static void SetupWeatherGrid(void) {
    // 8x4 grid, 3 levels deep
    InitTestGridFromAscii(
        "........\n"
        "........\n"
        "........\n"
        "........\n");
    FillGroundLevel();
    InitWater();
    InitTemperature();
    InitTime();
    InitWeather();
    // Enable seasons for weather testing
    daysPerSeason = 7;
    baseSurfaceTemp = 15;
    seasonalAmplitude = 20;
}

// Helper: force weather to a specific type for deterministic testing
static void ForceWeather(WeatherType type) {
    weatherState.current = type;
    weatherState.intensity = 1.0f;
    weatherState.transitionTimer = 999.0f;  // Don't auto-transition
}

// Helper: place a solid wall cell as a roof at z=2 above (x,y)
static void PlaceRoof(int x, int y) {
    grid[2][y][x] = CELL_WALL;
    SetWallMaterial(x, y, 2, MAT_GRANITE);
}

// =============================================================================
// Weather Initialization
// =============================================================================

describe(weather_initialization) {
    it("should start with WEATHER_CLEAR") {
        SetupWeatherGrid();
        expect(weatherState.current == WEATHER_CLEAR);
    }

    it("should initialize wind to calm") {
        SetupWeatherGrid();
        expect(weatherState.windStrength < 0.01f);
    }

    it("should set a positive transition timer") {
        SetupWeatherGrid();
        expect(weatherState.transitionTimer > 0.0f);
    }

    it("should set intensity to 1.0 initially") {
        SetupWeatherGrid();
        expect(weatherState.intensity >= 0.9f);
    }
}

// =============================================================================
// Weather Transitions
// =============================================================================

describe(weather_transitions) {
    it("should not transition before timer expires") {
        SetupWeatherGrid();
        srand(42);
        weatherState.transitionTimer = 100.0f;
        WeatherType before = weatherState.current;
        // Advance 50 game-seconds (not enough to expire)
        for (int i = 0; i < 3000; i++) {  // 50s at TICK_DT
            gameDeltaTime = TICK_DT;
            UpdateWeather();
        }
        expect(weatherState.current == before);
    }

    it("should transition after timer expires") {
        SetupWeatherGrid();
        srand(42);
        weatherState.transitionTimer = 0.5f;  // About to expire
        // Advance past the timer
        for (int i = 0; i < 120; i++) {  // ~2s at TICK_DT
            gameDeltaTime = TICK_DT;
            UpdateWeather();
        }
        // Timer should have expired and triggered a transition
        // (may or may not have changed type depending on RNG, but timer should have reset)
        expect(weatherState.transitionTimer > 0.0f);
    }

    it("should never transition to SNOW outside winter") {
        SetupWeatherGrid();
        dayNumber = 11;  // Summer
        srand(123);
        int snowCount = 0;
        for (int i = 0; i < 1000; i++) {
            weatherState.transitionTimer = 0.0f;
            gameDeltaTime = TICK_DT;
            UpdateWeather();
            if (weatherState.current == WEATHER_SNOW) snowCount++;
        }
        expect(snowCount == 0);
    }

    it("should allow SNOW transitions in winter") {
        SetupWeatherGrid();
        dayNumber = 25;  // Winter
        srand(456);
        int snowCount = 0;
        for (int i = 0; i < 2000; i++) {
            // Force to CLOUDY (can transition to SNOW)
            weatherState.current = WEATHER_CLOUDY;
            weatherState.transitionTimer = 0.0f;
            gameDeltaTime = TICK_DT;
            UpdateWeather();
            if (weatherState.current == WEATHER_SNOW) snowCount++;
        }
        expect(snowCount > 0);
    }

    it("should never produce THUNDERSTORM outside summer") {
        SetupWeatherGrid();
        dayNumber = 25;  // Winter
        srand(789);
        int thunderCount = 0;
        for (int i = 0; i < 2000; i++) {
            weatherState.current = WEATHER_HEAVY_RAIN;
            weatherState.transitionTimer = 0.0f;
            gameDeltaTime = TICK_DT;
            UpdateWeather();
            if (weatherState.current == WEATHER_THUNDERSTORM) thunderCount++;
        }
        expect(thunderCount == 0);
    }

    it("should allow THUNDERSTORM in summer from HEAVY_RAIN") {
        SetupWeatherGrid();
        dayNumber = 11;  // Summer
        srand(101);
        int thunderCount = 0;
        for (int i = 0; i < 2000; i++) {
            weatherState.current = WEATHER_HEAVY_RAIN;
            weatherState.transitionTimer = 0.0f;
            gameDeltaTime = TICK_DT;
            UpdateWeather();
            if (weatherState.current == WEATHER_THUNDERSTORM) thunderCount++;
        }
        expect(thunderCount > 0);
    }

    it("should track previous weather type") {
        SetupWeatherGrid();
        srand(42);
        ForceWeather(WEATHER_CLOUDY);
        weatherState.transitionTimer = 0.0f;
        gameDeltaTime = TICK_DT;
        UpdateWeather();
        // After transition, previous should be CLOUDY
        expect(weatherState.previous == WEATHER_CLOUDY);
    }
}

// =============================================================================
// Weather Transition Probabilities
// =============================================================================

describe(weather_transition_probabilities) {
    it("should favor CLOUDY from CLEAR") {
        SetupWeatherGrid();
        dayNumber = 4;  // Spring
        srand(42);
        int counts[WEATHER_COUNT];
        memset(counts, 0, sizeof(counts));
        for (int i = 0; i < 1000; i++) {
            weatherState.current = WEATHER_CLEAR;
            weatherState.transitionTimer = 0.0f;
            gameDeltaTime = TICK_DT;
            UpdateWeather();
            counts[weatherState.current]++;
        }
        // CLOUDY should be most common transition from CLEAR
        expect(counts[WEATHER_CLOUDY] > counts[WEATHER_RAIN]);
        expect(counts[WEATHER_CLOUDY] > counts[WEATHER_HEAVY_RAIN]);
    }
}

// =============================================================================
// Weather Intensity
// =============================================================================

describe(weather_intensity) {
    it("should ramp intensity up from 0 after transition") {
        SetupWeatherGrid();
        srand(42);
        ForceWeather(WEATHER_CLOUDY);
        weatherState.transitionTimer = 0.0f;
        gameDeltaTime = TICK_DT;
        UpdateWeather();
        // Right after transition, intensity should start low
        float initialIntensity = weatherState.intensity;
        expect(initialIntensity < 0.5f);
    }

    it("should reach 1.0 after ramp-up period") {
        SetupWeatherGrid();
        srand(42);
        weatherState.intensity = 0.1f;
        // Run for a while, intensity should reach 1.0
        for (int i = 0; i < 600; i++) {  // 10s at TICK_DT
            gameDeltaTime = TICK_DT;
            UpdateWeather();
        }
        expect(weatherState.intensity >= 0.95f);
    }
}

// =============================================================================
// Roof Detection
// =============================================================================

describe(roof_detection) {
    it("should detect exposed cells with no roof") {
        SetupWeatherGrid();
        // z=1 is air above dirt z=0, no roof above
        expect(IsExposedToSky(3, 2, 1));
    }

    it("should detect sheltered cells under solid roof") {
        SetupWeatherGrid();
        PlaceRoof(3, 2);  // Wall at z=2
        // z=1 below the wall should be sheltered
        expect(!IsExposedToSky(3, 2, 1));
    }

    it("should detect sheltered cells under floor") {
        SetupWeatherGrid();
        SET_FLOOR(3, 2, 2);  // Floor at z=2
        expect(!IsExposedToSky(3, 2, 1));
    }

    it("should handle cells at top z-level as exposed") {
        SetupWeatherGrid();
        int topZ = gridDepth - 1;
        // Top z-level with air should be exposed
        expect(IsExposedToSky(3, 2, topZ));
    }

    it("should detect shelter even with gap between cell and roof") {
        SetupWeatherGrid();
        // Roof at z=2, cell at z=0 (gap at z=1) - still sheltered
        PlaceRoof(3, 2);
        expect(!IsExposedToSky(3, 2, 0));
    }
}

// =============================================================================
// Rain Wetness
// =============================================================================

describe(rain_wetness) {
    it("should increase wetness on exposed dirt during rain") {
        SetupWeatherGrid();
        ForceWeather(WEATHER_RAIN);
        rainWetnessInterval = 0.5f;  // Fast for testing
        // Run enough ticks for wetness to increase
        for (int i = 0; i < 120; i++) {
            gameDeltaTime = TICK_DT;
            UpdateWeather();
        }
        // Check some exposed dirt cells gained wetness
        int totalWetness = 0;
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                totalWetness += GET_CELL_WETNESS(x, y, 0);
            }
        }
        expect(totalWetness > 0);
    }

    it("should not increase wetness on sheltered cells") {
        SetupWeatherGrid();
        ForceWeather(WEATHER_RAIN);
        rainWetnessInterval = 0.5f;
        // Shelter cell (3,2)
        PlaceRoof(3, 2);
        SET_CELL_WETNESS(3, 2, 0, 0);
        for (int i = 0; i < 300; i++) {
            gameDeltaTime = TICK_DT;
            UpdateWeather();
        }
        expect(GET_CELL_WETNESS(3, 2, 0) == 0);
    }

    it("should create mud on exposed dirt during heavy rain") {
        SetupWeatherGrid();
        ForceWeather(WEATHER_HEAVY_RAIN);
        heavyRainWetnessInterval = 0.3f;
        // Run long enough for wetness to reach 2+ on some cells
        for (int i = 0; i < 600; i++) {
            gameDeltaTime = TICK_DT;
            UpdateWeather();
        }
        // Check at least one cell became muddy
        int muddyCount = 0;
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (IsMuddy(x, y, 0)) muddyCount++;
            }
        }
        expect(muddyCount > 0);
    }

    it("should not increase wetness on non-soil cells") {
        SetupWeatherGrid();
        // Change cell to granite
        SetWallMaterial(3, 2, 0, MAT_GRANITE);
        ForceWeather(WEATHER_RAIN);
        rainWetnessInterval = 0.5f;
        SET_CELL_WETNESS(3, 2, 0, 0);
        for (int i = 0; i < 300; i++) {
            gameDeltaTime = TICK_DT;
            UpdateWeather();
        }
        expect(GET_CELL_WETNESS(3, 2, 0) == 0);
    }

    it("should increase wetness faster during HEAVY_RAIN than RAIN") {
        // Test with RAIN - use long intervals so wetness doesn't cap at max(3)
        SetupWeatherGrid();
        ForceWeather(WEATHER_RAIN);
        rainWetnessInterval = 3.0f;
        heavyRainWetnessInterval = 1.5f;
        // 120 ticks = 2 seconds: RAIN gets 0 increments (interval=3s), HEAVY_RAIN gets 1 (interval=1.5s)
        for (int i = 0; i < 120; i++) {
            gameDeltaTime = TICK_DT;
            UpdateWeather();
        }
        int rainWetness = 0;
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++)
                rainWetness += GET_CELL_WETNESS(x, y, 0);

        // Test with HEAVY_RAIN (same duration)
        SetupWeatherGrid();
        ForceWeather(WEATHER_HEAVY_RAIN);
        rainWetnessInterval = 3.0f;
        heavyRainWetnessInterval = 1.5f;
        for (int i = 0; i < 120; i++) {
            gameDeltaTime = TICK_DT;
            UpdateWeather();
        }
        int heavyWetness = 0;
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++)
                heavyWetness += GET_CELL_WETNESS(x, y, 0);

        expect(heavyWetness > rainWetness);
    }
}

// =============================================================================
// Weather-Driven Rain Spawning
// =============================================================================

describe(rain_water_spawning) {
    it("should spawn sky water during rain weather") {
        SetupWeatherGrid();
        ForceWeather(WEATHER_RAIN);
        // Run a few ticks of weather update
        for (int i = 0; i < 60; i++) {
            gameDeltaTime = TICK_DT;
            UpdateWeather();
        }
        expect(IsRaining());
    }

    it("should stop sky water when weather changes to CLEAR") {
        SetupWeatherGrid();
        ForceWeather(WEATHER_RAIN);
        for (int i = 0; i < 60; i++) {
            gameDeltaTime = TICK_DT;
            UpdateWeather();
        }
        expect(IsRaining());
        // Switch to clear
        ForceWeather(WEATHER_CLEAR);
        for (int i = 0; i < 60; i++) {
            gameDeltaTime = TICK_DT;
            UpdateWeather();
        }
        expect(!IsRaining());
    }
}

// =============================================================================
// Weather Wind Basics
// =============================================================================

describe(weather_wind_basics) {
    it("should have low wind strength during CLEAR") {
        SetupWeatherGrid();
        ForceWeather(WEATHER_CLEAR);
        // Run to stabilize wind
        for (int i = 0; i < 300; i++) {
            gameDeltaTime = TICK_DT;
            UpdateWeather();
        }
        expect(weatherState.windStrength < 2.0f);
    }

    it("should have high wind during THUNDERSTORM") {
        SetupWeatherGrid();
        ForceWeather(WEATHER_THUNDERSTORM);
        // Run to let wind build up
        for (int i = 0; i < 600; i++) {
            gameDeltaTime = TICK_DT;
            UpdateWeather();
        }
        expect(weatherState.windStrength > 2.0f);
    }

    it("should have normalized wind direction") {
        SetupWeatherGrid();
        srand(42);
        ForceWeather(WEATHER_RAIN);
        weatherState.windDirX = 0.7f;
        weatherState.windDirY = 0.7f;
        for (int i = 0; i < 60; i++) {
            gameDeltaTime = TICK_DT;
            UpdateWeather();
        }
        float len = sqrtf(weatherState.windDirX * weatherState.windDirX +
                          weatherState.windDirY * weatherState.windDirY);
        // Should be roughly normalized (1.0) or zero
        expect(len < 1.1f);
    }
}

// =============================================================================
// Weather Display Names
// =============================================================================

describe(weather_display) {
    it("should return correct weather name strings") {
        expect(strcmp(GetWeatherName(WEATHER_CLEAR), "Clear") == 0);
        expect(strcmp(GetWeatherName(WEATHER_CLOUDY), "Cloudy") == 0);
        expect(strcmp(GetWeatherName(WEATHER_RAIN), "Rain") == 0);
        expect(strcmp(GetWeatherName(WEATHER_HEAVY_RAIN), "Heavy Rain") == 0);
        expect(strcmp(GetWeatherName(WEATHER_THUNDERSTORM), "Thunderstorm") == 0);
        expect(strcmp(GetWeatherName(WEATHER_SNOW), "Snow") == 0);
        expect(strcmp(GetWeatherName(WEATHER_MIST), "Mist") == 0);
    }

    it("should handle invalid weather type gracefully") {
        const char* name = GetWeatherName(WEATHER_COUNT);
        expect(name != NULL);
    }
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv) {
    bool quiet = false;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'v') test_verbose = true;
        if (argv[i][0] == '-' && argv[i][1] == 'q') quiet = true;
    }
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);
    if (quiet) set_quiet_mode(1);

    printf("Running weather tests...\n");

    test(weather_initialization);
    test(weather_transitions);
    test(weather_transition_probabilities);
    test(weather_intensity);
    test(roof_detection);
    test(rain_wetness);
    test(rain_water_spawning);
    test(weather_wind_basics);
    test(weather_display);

    return summary();
}
