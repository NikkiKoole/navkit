// test_thunderstorm.c - Phase 5: Thunderstorms + Mist
// Uses precompiled test_unity.o for game logic

#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/core/time.h"
#include "../src/entities/mover.h"
#include "../src/simulation/weather.h"
#include "../src/simulation/water.h"
#include "../src/simulation/temperature.h"
#include "../src/simulation/fire.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/world/material.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static bool test_verbose = false;

// Helper: set up a 16x16 grid with wood floors for lightning testing
static void SetupThunderstormGrid(void) {
    InitGridFromAsciiWithChunkSize(
        "################\n"
        "################\n"
        "################\n"
        "################\n"
        "################\n"
        "################\n"
        "################\n"
        "################\n"
        "################\n"
        "################\n"
        "################\n"
        "################\n"
        "################\n"
        "################\n"
        "################\n"
        "################\n", 16, 16);
    
    FillGroundLevel();
    InitWater();
    InitTemperature();
    InitTime();
    InitFire();
    InitWeather();
    
    // Set to summer for thunderstorms
    dayNumber = 8;  // Day 8 = summer (with daysPerSeason=7)
    timeOfDay = 14.0f;  // Afternoon
    
    // Place wooden floors on top of ground
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            if (grid[1][y][x] == CELL_AIR) {
                SET_CELL_FLAG(x, y, 1, CELL_FLAG_HAS_FLOOR);
                SetFloorMaterial(x, y, 1, MAT_OAK);
            }
        }
    }
}

// Helper: count active fires in the grid
static int CountFires(void) {
    int count = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (GetFireLevel(x, y, z) > 0) count++;
            }
        }
    }
    return count;
}

// =============================================================================
// Lightning Strike Tests
// =============================================================================

describe(lightning_strike_basics) {
    it("should only strike during THUNDERSTORM weather") {
        SetupThunderstormGrid();
        
        // Force CLEAR weather
        weatherState.current = WEATHER_CLEAR;
        weatherState.intensity = 1.0f;
        
        // Run UpdateLightning several times - should NOT strike
        for (int i = 0; i < 100; i++) {
            gameDeltaTime = 0.1f;
            UpdateLightning(gameDeltaTime);
        }
        
        expect(CountFires() == 0);
        
        // Now force THUNDERSTORM
        weatherState.current = WEATHER_THUNDERSTORM;
        weatherState.intensity = 1.0f;
        
        // Reset lightning timer to force immediate strike check
        ResetLightningTimer();
        SetLightningInterval(0.1f);
        
        // UpdateLightning should eventually strike something
        int fireFound = 0;
        for (int tick = 0; tick < 200 && !fireFound; tick++) {
            gameDeltaTime = 0.1f;
            UpdateLightning(gameDeltaTime);
            if (CountFires() > 0) {
                fireFound = 1;
                break;
            }
        }
        
        expect(fireFound == 1);
    }
    
    it("should only strike exposed cells") {
        SetupThunderstormGrid();
        
        // Create a small sheltered area - roof over center 4x4
        for (int yy = 6; yy <= 9; yy++) {
            for (int xx = 6; xx <= 9; xx++) {
                SET_CELL_FLAG(xx, yy, 2, CELL_FLAG_HAS_FLOOR);  // Roof at z=2
                SetFloorMaterial(xx, yy, 2, MAT_GRANITE);  // Stone roof (non-flammable)
            }
        }
        
        weatherState.current = WEATHER_THUNDERSTORM;
        weatherState.intensity = 1.0f;
        
        // Force many lightning strikes
        ResetLightningTimer();
        SetLightningInterval(0.01f);
        for (int i = 0; i < 500; i++) {
            gameDeltaTime = 0.01f;
            UpdateLightning(gameDeltaTime);
            // Don't update fire - we don't want spreading, just direct strikes
        }
        
        // Sheltered cells should have NO fire (no spreading happened)
        int fireInSheltered = 0;
        for (int yy = 6; yy <= 9; yy++) {
            for (int xx = 6; xx <= 9; xx++) {
                if (GetFireLevel(xx, yy, 1) > 0) fireInSheltered++;
            }
        }
        expect(fireInSheltered == 0);
        
        // But exposed cells around the edges SHOULD have fires
        int fireInExposed = 0;
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                // Skip the sheltered area
                if (x >= 6 && x <= 9 && y >= 6 && y <= 9) continue;
                if (GetFireLevel(x, y, 1) > 0) fireInExposed++;
            }
        }
        expect(fireInExposed > 0);  // Should have struck SOMETHING outside
    }
    
    it("should ignite flammable materials") {
        SetupThunderstormGrid();
        
        weatherState.current = WEATHER_THUNDERSTORM;
        weatherState.intensity = 1.0f;
        
        ResetLightningTimer();
        SetLightningInterval(0.5f);
        
        // Run many ticks - eventually something should catch fire
        int anyFire = 0;
        for (int i = 0; i < 500 && !anyFire; i++) {
            gameDeltaTime = 0.1f;
            UpdateLightning(gameDeltaTime);
            if (CountFires() > 0) {
                anyFire = 1;
                break;
            }
        }
        
        expect(anyFire == 1);
    }
    
    it("should not strike non-flammable materials") {
        SetupThunderstormGrid();
        
        // Replace all wood floors with granite (stone)
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (HAS_FLOOR(x, y, 1)) {
                    SetFloorMaterial(x, y, 1, MAT_GRANITE);
                }
            }
        }
        
        weatherState.current = WEATHER_THUNDERSTORM;
        weatherState.intensity = 1.0f;
        
        ResetLightningTimer();
        SetLightningInterval(0.01f);
        for (int i = 0; i < 1000; i++) {
            gameDeltaTime = 0.01f;
            UpdateLightning(gameDeltaTime);
            UpdateFire();
        }
        
        expect(CountFires() == 0);  // Should never catch fire
    }
    
    it("should respect configurable lightning interval") {
        SetupThunderstormGrid();
        
        weatherState.current = WEATHER_THUNDERSTORM;
        weatherState.intensity = 1.0f;
        
        // Set a long interval
        SetLightningInterval(10.0f);
        ResetLightningTimer();
        
        // 5 seconds should NOT trigger
        gameDeltaTime = 5.0f;
        UpdateLightning(gameDeltaTime);
        int fireCount1 = CountFires();
        expect(fireCount1 == 0);
        
        // Another 6 seconds (total 11) should trigger
        gameDeltaTime = 6.0f;
        UpdateLightning(gameDeltaTime);
        int fireCount2 = CountFires();
        expect(fireCount2 > 0);
    }
}

// =============================================================================
// Lightning Flash Tests
// =============================================================================

describe(lightning_flash_visuals) {
    it("should set flash timer on lightning strike") {
        SetupThunderstormGrid();
        
        weatherState.current = WEATHER_THUNDERSTORM;
        weatherState.intensity = 1.0f;
        
        SetLightningInterval(0.5f);  // Set interval first
        ResetLightningTimer();        // Then reset to that interval
        
        float flashBefore = GetLightningFlashIntensity();
        expect(flashBefore == 0.0f);
        
        // Trigger lightning (1.0s > 0.5s interval, should trigger)
        gameDeltaTime = 1.0f;
        UpdateLightning(gameDeltaTime);
        
        float flashAfter = GetLightningFlashIntensity();
        expect(flashAfter > 0.0f);  // Flash should be active
    }
    
    it("should decay flash over time") {
        SetupThunderstormGrid();
        
        // Manually trigger a flash
        TriggerLightningFlash();
        
        float intensity1 = GetLightningFlashIntensity();
        expect(intensity1 == 1.0f);  // Should start at max
        
        // Decay over several frames
        UpdateLightningFlash(0.1f);
        float intensity2 = GetLightningFlashIntensity();
        expect(intensity2 < intensity1);
        expect(intensity2 > 0.0f);
        
        UpdateLightningFlash(0.1f);
        float intensity3 = GetLightningFlashIntensity();
        expect(intensity3 < intensity2);
        
        // Eventually reaches zero
        for (int i = 0; i < 100; i++) {
            UpdateLightningFlash(0.1f);
        }
        expect(GetLightningFlashIntensity() == 0.0f);
    }
}

// =============================================================================
// Mist Intensity Tests
// =============================================================================

describe(mist_intensity_basics) {
    it("should have high intensity during WEATHER_MIST") {
        SetupThunderstormGrid();
        
        weatherState.current = WEATHER_MIST;
        weatherState.intensity = 1.0f;
        
        float mist = GetMistIntensity();
        expect(mist > 0.7f);  // Should be quite thick
    }
    
    it("should have zero intensity during WEATHER_CLEAR") {
        SetupThunderstormGrid();
        
        weatherState.current = WEATHER_CLEAR;
        weatherState.intensity = 1.0f;
        
        float mist = GetMistIntensity();
        expect(mist == 0.0f);
    }
    
    it("should have some mist during rainy weather") {
        SetupThunderstormGrid();
        
        weatherState.current = WEATHER_RAIN;
        weatherState.intensity = 1.0f;
        
        float mist = GetMistIntensity();
        expect(mist > 0.0f);
        expect(mist < 0.5f);  // Less than full MIST weather
    }
    
    it("should modulate mist with time of day") {
        SetupThunderstormGrid();
        
        weatherState.current = WEATHER_MIST;
        weatherState.intensity = 1.0f;
        
        // Dawn should have more mist
        timeOfDay = 6.0f;
        float mistDawn = GetMistIntensity();
        
        // Noon should have less
        timeOfDay = 12.0f;
        float mistNoon = GetMistIntensity();
        
        expect(mistDawn > mistNoon);
    }
}

// =============================================================================
// Full Year Cycle Test
// =============================================================================

describe(year_long_simulation) {
    it("should cycle through all weather types appropriately over a year") {
        SetupThunderstormGrid();
        
        dayNumber = 1;  // Start of spring
        
        // Track which weather types occur
        int weatherCounts[WEATHER_COUNT];
        memset(weatherCounts, 0, sizeof(weatherCounts));
        int snowInWinter = 0, snowOutsideWinter = 0;
        int thunderInSummer = 0, thunderOutsideSummer = 0;
        
        // Simulate a full year
        for (int day = 1; day <= 28; day++) {
            dayNumber = day;
            Season season = GetCurrentSeason();
            
            // Force several weather transitions per day
            for (int i = 0; i < 10; i++) {
                // Expire current weather
                weatherState.transitionTimer = -1.0f;
                gameDeltaTime = 1.0f;
                UpdateWeather();
                
                weatherCounts[weatherState.current]++;
                
                if (weatherState.current == WEATHER_SNOW) {
                    if (season == SEASON_WINTER) {
                        snowInWinter++;
                    } else {
                        snowOutsideWinter++;
                    }
                }
                
                if (weatherState.current == WEATHER_THUNDERSTORM) {
                    if (season == SEASON_SUMMER) {
                        thunderInSummer++;
                    } else {
                        thunderOutsideSummer++;
                    }
                }
            }
        }
        
        // Should have experienced multiple weather types
        expect(weatherCounts[WEATHER_CLEAR] > 0);
        expect(weatherCounts[WEATHER_CLOUDY] > 0);
        expect(weatherCounts[WEATHER_RAIN] > 0);
        
        // Snow should only happen in winter
        expect(snowInWinter > 0);
        expect(snowOutsideWinter == 0);
        
        // Thunderstorms should primarily happen in summer
        expect(thunderInSummer > 0);
        expect(thunderOutsideSummer == 0);
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

    printf("Running thunderstorm tests...\n");

    test(lightning_strike_basics);
    test(lightning_flash_visuals);
    test(mist_intensity_basics);
    test(year_long_simulation);

    return summary();
}
