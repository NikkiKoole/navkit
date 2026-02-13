#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/core/time.h"
#include "../src/entities/mover.h"
#include "../src/simulation/weather.h"
#include "../src/simulation/smoke.h"
#include "../src/simulation/steam.h"
#include "../src/simulation/fire.h"
#include "../src/simulation/water.h"
#include "../src/simulation/temperature.h"
#include "../src/simulation/groundwear.h"
#include "../src/world/grid.h"
#include "test_helpers.h"
#include "../src/world/cell_defs.h"
#include "../src/world/material.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static bool test_verbose = false;

// Helper: set up a grid with weather + wind configured
static void SetupWindGrid(void) {
    InitTestGridFromAscii(
        "................\n"
        "................\n"
        "................\n"
        "................\n"
        "................\n"
        "................\n"
        "................\n"
        "................\n"
        "................\n"
        "................\n"
        "................\n"
        "................\n"
        "................\n"
        "................\n"
        "................\n"
        "................\n");
    FillGroundLevel();
    InitWater();
    InitTemperature();
    InitTime();
    InitWeather();
    InitSmoke();
    InitFire();
    // Disable weather auto-transitions for deterministic testing
    weatherState.transitionTimer = 999.0f;
    weatherState.current = WEATHER_CLEAR;
    weatherState.intensity = 1.0f;
}

// Helper: set wind to a specific direction and strength
static void SetWind(float dirX, float dirY, float strength) {
    weatherState.windDirX = dirX;
    weatherState.windDirY = dirY;
    weatherState.windStrength = strength;
}

// =============================================================================
// Wind Dot Product
// =============================================================================

describe(wind_dot_product) {
    it("should return positive value when moving downwind") {
        SetupWindGrid();
        SetWind(1.0f, 0.0f, 3.0f);  // Wind blowing east
        float dot = GetWindDotProduct(1, 0);  // Moving east (downwind)
        expect(dot > 0.0f);
    }

    it("should return negative value when moving upwind") {
        SetupWindGrid();
        SetWind(1.0f, 0.0f, 3.0f);  // Wind blowing east
        float dot = GetWindDotProduct(-1, 0);  // Moving west (upwind)
        expect(dot < 0.0f);
    }

    it("should return zero when moving perpendicular") {
        SetupWindGrid();
        SetWind(1.0f, 0.0f, 3.0f);  // Wind blowing east
        float dot = GetWindDotProduct(0, 1);  // Moving south (perpendicular)
        expect(fabsf(dot) < 0.01f);
    }

    it("should scale with wind strength") {
        SetupWindGrid();
        SetWind(1.0f, 0.0f, 1.0f);
        float dot1 = GetWindDotProduct(1, 0);
        SetWind(1.0f, 0.0f, 3.0f);
        float dot3 = GetWindDotProduct(1, 0);
        expect(dot3 > dot1);
    }

    it("should return zero when wind strength is zero") {
        SetupWindGrid();
        SetWind(1.0f, 0.0f, 0.0f);
        float dot = GetWindDotProduct(1, 0);
        expect(fabsf(dot) < 0.01f);
    }
}

// =============================================================================
// Wind Smoke Bias
// =============================================================================

describe(wind_smoke_bias) {
    it("should drift smoke downwind over time") {
        // Statistical test: run many times, count east vs west spread
        int eastCount = 0;
        int westCount = 0;

        for (int trial = 0; trial < 50; trial++) {
            SetupWindGrid();
            srand(trial * 17 + 42);
            SetWind(1.0f, 0.0f, 3.0f);  // Strong east wind

            // Continuously add smoke at center to maintain spreading
            int cx = 8, cy = 8;
            int cz = 1;  // Use z=1 (above ground) so smoke can spread
            
            // Run smoke spread for multiple ticks, adding smoke each tick
            for (int i = 0; i < 20; i++) {
                SetSmokeLevel(cx, cy, cz, 3);  // Keep replenishing center
                gameDeltaTime = TICK_DT;
                smokeRiseInterval = 999.0f;  // Disable rising
                smokeDissipationTime = 999.0f;  // Disable dissipation
                if (test_verbose && trial == 0 && i == 0) {
                    printf("Before UpdateSmoke: center=%d\n",
                           GetSmokeLevel(cx, cy, cz));
                }
                UpdateSmoke();
                if (test_verbose && trial == 0 && i == 0) {
                    printf("After UpdateSmoke: center=%d, east=%d, west=%d\n",
                           GetSmokeLevel(cx, cy, cz),
                           GetSmokeLevel(cx+1, cy, cz),
                           GetSmokeLevel(cx-1, cy, cz));
                }
            }

            // Count smoke east vs west of center
            int east = 0, west = 0;
            for (int x = 0; x < gridWidth; x++) {
                for (int y = 0; y < gridHeight; y++) {
                    int level = GetSmokeLevel(x, y, cz);
                    if (level > 0 && x != cx) {
                        if (x > cx) east += level;
                        else west += level;
                    }
                }
            }
            if (test_verbose && trial == 0) {
                printf("Trial 0: east=%d, west=%d, center=%d\n", 
                       east, west, GetSmokeLevel(cx, cy, cz));
            }
            if (east > west) eastCount++;
            else if (west > east) westCount++;
        }

        // With east wind, smoke should drift east more often
        if (test_verbose) {
            printf("Smoke bias test: eastCount=%d, westCount=%d\n", eastCount, westCount);
        }
        expect(eastCount > westCount);
    }

    it("should spread evenly with no wind") {
        // Statistical test: with no wind, east and west should be roughly equal
        int eastTotal = 0;
        int westTotal = 0;

        for (int trial = 0; trial < 100; trial++) {
            SetupWindGrid();
            srand(trial * 31 + 7);
            SetWind(0.0f, 0.0f, 0.0f);  // No wind

            int cx = 8, cy = 8;
            SetSmokeLevel(cx, cy, 0, SMOKE_MAX_LEVEL);

            for (int i = 0; i < 10; i++) {
                gameDeltaTime = TICK_DT;
                smokeRiseInterval = 999.0f;
                smokeDissipationTime = 999.0f;
                UpdateSmoke();
            }

            for (int x = 0; x < gridWidth; x++) {
                for (int y = 0; y < gridHeight; y++) {
                    int level = GetSmokeLevel(x, y, 0);
                    if (level > 0 && x != cx) {
                        if (x > cx) eastTotal += level;
                        else westTotal += level;
                    }
                }
            }
        }

        // Should be roughly equal (within 30% of each other)
        float ratio = (eastTotal > 0 && westTotal > 0)
                      ? (float)(eastTotal > westTotal ? eastTotal : westTotal) /
                        (float)(eastTotal > westTotal ? westTotal : eastTotal)
                      : 1.0f;
        expect(ratio < 1.3f);
    }
}

// =============================================================================
// Wind Fire Spread
// =============================================================================

describe(wind_fire_spread) {
    it("should spread fire more downwind than upwind") {
        // Statistical test over many trials
        int downwindIgnitions = 0;
        int upwindIgnitions = 0;

        for (int trial = 0; trial < 200; trial++) {
            SetupWindGrid();
            srand(trial * 13 + 99);
            SetWind(1.0f, 0.0f, 3.0f);  // Strong east wind

            int cx = 8, cy = 8;
            // Make surrounding cells burnable (wood walls)
            for (int dx = -2; dx <= 2; dx++) {
                for (int dy = -2; dy <= 2; dy++) {
                    grid[0][cy + dy][cx + dx] = CELL_WALL;
                    SetWallMaterial(cx + dx, cy + dy, 0, MAT_OAK);
                    wallNatural[0][cy + dy][cx + dx] = 1;
                }
            }

            // Ignite center at LOW level so spread is marginal
            SetFireLevel(cx, cy, 0, 2);  // Low fire level = low spread chance
            fireGrid[0][cy][cx].fuel = 100;

            // Run fire spread for fewer ticks
            fireSpreadInterval = TICK_DT;  // Spread every tick
            for (int i = 0; i < 5; i++) {  // Only 5 ticks, not 30
                gameDeltaTime = TICK_DT;
                UpdateFire();
            }

            // Check east vs west ignitions
            if (fireGrid[0][cy][cx + 1].level > 0) downwindIgnitions++;
            if (fireGrid[0][cy][cx - 1].level > 0) upwindIgnitions++;
        }

        // Downwind should have more ignitions
        if (test_verbose) {
            printf("Fire spread test: downwindIgnitions=%d, upwindIgnitions=%d\n", 
                   downwindIgnitions, upwindIgnitions);
        }
        expect(downwindIgnitions > upwindIgnitions);
    }

    it("should not change fire spread with zero wind") {
        // With zero wind, downwind and upwind should be roughly equal
        int eastIgnitions = 0;
        int westIgnitions = 0;

        for (int trial = 0; trial < 200; trial++) {
            SetupWindGrid();
            srand(trial * 7 + 55);
            SetWind(0.0f, 0.0f, 0.0f);  // No wind

            int cx = 8, cy = 8;
            for (int dx = -2; dx <= 2; dx++) {
                for (int dy = -2; dy <= 2; dy++) {
                    grid[0][cy + dy][cx + dx] = CELL_WALL;
                    SetWallMaterial(cx + dx, cy + dy, 0, MAT_OAK);
                    wallNatural[0][cy + dy][cx + dx] = 1;
                }
            }

            SetFireLevel(cx, cy, 0, 5);
            fireGrid[0][cy][cx].fuel = 100;

            fireSpreadInterval = TICK_DT;
            for (int i = 0; i < 30; i++) {
                gameDeltaTime = TICK_DT;
                UpdateFire();
            }

            if (fireGrid[0][cy][cx + 1].level > 0) eastIgnitions++;
            if (fireGrid[0][cy][cx - 1].level > 0) westIgnitions++;
        }

        // Should be roughly equal (within 40%)
        int bigger = eastIgnitions > westIgnitions ? eastIgnitions : westIgnitions;
        int smaller = eastIgnitions > westIgnitions ? westIgnitions : eastIgnitions;
        float ratio = (smaller > 0) ? (float)bigger / (float)smaller : 1.0f;
        expect(ratio < 1.4f);
    }
}

// =============================================================================
// Wind Drying
// =============================================================================

describe(wind_drying) {
    it("should dry exposed cells faster with strong wind") {
        // Test with no wind
        SetupWindGrid();
        SetWind(0.0f, 0.0f, 0.0f);
        groundWearEnabled = true;
        wearRecoveryInterval = TICK_DT;  // Process every tick

        // Wet some cells
        for (int x = 0; x < 8; x++) {
            SET_CELL_WETNESS(x, 3, 0, 3);  // Max wetness
        }

        // Run groundwear for some ticks
        for (int i = 0; i < 60; i++) {
            gameDeltaTime = TICK_DT;
            UpdateGroundWear();
        }

        int noWindWetness = 0;
        for (int x = 0; x < 8; x++) {
            noWindWetness += GET_CELL_WETNESS(x, 3, 0);
        }

        // Test with strong wind
        SetupWindGrid();
        SetWind(1.0f, 0.0f, 3.0f);
        groundWearEnabled = true;
        wearRecoveryInterval = TICK_DT;

        for (int x = 0; x < 8; x++) {
            SET_CELL_WETNESS(x, 3, 0, 3);
        }

        for (int i = 0; i < 60; i++) {
            gameDeltaTime = TICK_DT;
            UpdateGroundWear();
        }

        int windyWetness = 0;
        for (int x = 0; x < 8; x++) {
            windyWetness += GET_CELL_WETNESS(x, 3, 0);
        }

        // Windy cells should have less remaining wetness (dried faster)
        expect(windyWetness <= noWindWetness);
    }

    it("should not accelerate drying on sheltered cells") {
        SetupWindGrid();
        SetWind(1.0f, 0.0f, 3.0f);
        groundWearEnabled = true;
        wearRecoveryInterval = TICK_DT;

        // Place roof over cell (3,3)
        grid[2][3][3] = CELL_WALL;
        SetWallMaterial(3, 3, 2, MAT_GRANITE);

        SET_CELL_WETNESS(3, 3, 0, 3);  // Sheltered
        SET_CELL_WETNESS(5, 3, 0, 3);  // Exposed

        for (int i = 0; i < 60; i++) {
            gameDeltaTime = TICK_DT;
            UpdateGroundWear();
        }

        int shelteredWetness = GET_CELL_WETNESS(3, 3, 0);
        int exposedWetness = GET_CELL_WETNESS(5, 3, 0);

        // Exposed cell should dry faster (or equal) than sheltered
        expect(exposedWetness <= shelteredWetness);
    }
}

// =============================================================================
// Wind Chill
// =============================================================================

describe(wind_chill) {
    it("should lower effective temperature with wind") {
        SetupWindGrid();
        int baseTemp = GetSeasonalSurfaceTemp();
        float chilled = GetWindChillTemp((float)baseTemp, 3.0f, true);
        expect(chilled < (float)baseTemp);
    }

    it("should not apply wind chill when sheltered") {
        SetupWindGrid();
        int baseTemp = GetSeasonalSurfaceTemp();
        float chilled = GetWindChillTemp((float)baseTemp, 3.0f, false);
        expect(fabsf(chilled - (float)baseTemp) < 0.01f);
    }

    it("should increase chill effect with wind strength") {
        SetupWindGrid();
        float baseTemp = 20.0f;
        float chill1 = GetWindChillTemp(baseTemp, 1.0f, true);
        float chill3 = GetWindChillTemp(baseTemp, 3.0f, true);
        expect(chill3 < chill1);
    }

    it("should not apply chill with zero wind") {
        SetupWindGrid();
        float baseTemp = 20.0f;
        float chilled = GetWindChillTemp(baseTemp, 0.0f, true);
        expect(fabsf(chilled - baseTemp) < 0.01f);
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

    printf("Running wind tests...\n");

    test(wind_dot_product);
    test(wind_smoke_bias);
    test(wind_fire_spread);
    test(wind_drying);
    test(wind_chill);

    return summary();
}
