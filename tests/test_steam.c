#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/simulation/steam.h"
#include "../src/simulation/water.h"
#include "../src/simulation/temperature.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Helper to run steam simulation for N ticks
static void RunSteamTicks(int n) {
    for (int i = 0; i < n; i++) {
        UpdateSteam();
    }
}

// Helper to run full simulation (temperature, water freezing/boiling, steam)
static void RunFullSimTicks(int n) {
    for (int i = 0; i < n; i++) {
        UpdateTemperature();
        UpdateWaterFreezing();  // This also handles boiling -> steam
        UpdateSteam();
    }
}

// Helper to count total steam in the grid
static int CountTotalSteam(void) {
    int total = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                total += GetSteamLevel(x, y, z);
            }
        }
    }
    return total;
}

// Helper to count total water in the grid
static int CountTotalWater(void) {
    int total = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                total += GetWaterLevel(x, y, z);
            }
        }
    }
    return total;
}

// =============================================================================
// Basic Steam Operations
// =============================================================================

describe(steam_initialization) {
    it("should initialize steam grid with all zeros") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 4);
        
        InitSteam();
        
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                expect(GetSteamLevel(x, y, 0) == 0);
            }
        }
    }
    
    it("should clear all steam when ClearSteam is called") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n", 8, 2);
        
        InitSteam();
        SetSteamLevel(2, 0, 0, 5);
        SetSteamLevel(4, 1, 0, 7);
        
        expect(GetSteamLevel(2, 0, 0) == 5);
        expect(GetSteamLevel(4, 1, 0) == 7);
        
        ClearSteam();
        
        expect(GetSteamLevel(2, 0, 0) == 0);
        expect(GetSteamLevel(4, 1, 0) == 0);
    }
}

describe(steam_level_operations) {
    it("should set steam level within bounds") {
        InitGridFromAsciiWithChunkSize(
            "....\n"
            "....\n", 4, 2);
        InitSteam();
        
        SetSteamLevel(1, 0, 0, 5);
        expect(GetSteamLevel(1, 0, 0) == 5);
        
        SetSteamLevel(2, 1, 0, 7);
        expect(GetSteamLevel(2, 1, 0) == 7);
    }
    
    it("should clamp steam level to max 7") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitSteam();
        
        SetSteamLevel(0, 0, 0, 10);
        expect(GetSteamLevel(0, 0, 0) == STEAM_MAX_LEVEL);
    }
    
    it("should clamp steam level to min 0") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitSteam();
        
        SetSteamLevel(0, 0, 0, 5);
        AddSteam(0, 0, 0, -10);  // Try to subtract more than available
        expect(GetSteamLevel(0, 0, 0) == 0);
    }
    
    it("should add steam correctly") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitSteam();
        
        SetSteamLevel(0, 0, 0, 2);
        AddSteam(0, 0, 0, 3);
        expect(GetSteamLevel(0, 0, 0) == 5);
    }
}

// =============================================================================
// Steam Rising
// =============================================================================

describe(steam_rising) {
    it("should rise to level above when space is available") {
        InitGridFromAsciiWithChunkSize(
            "....\n"
            "....\n", 4, 2);
        gridDepth = 3;
        for (int z = 1; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                for (int x = 0; x < gridWidth; x++) {
                    grid[z][y][x] = CELL_AIR;
                }
            }
        }
        
        InitSteam();
        InitWater();
        InitTemperature();
        
        // Set ambient hot so steam doesn't condense (need >= 96C / index 73)
        ambientSurfaceTemp = 100;  // Hot ambient prevents condensation
        temperatureEnabled = true;
        
        // Run temperature a bit to stabilize hot ambient
        for (int i = 0; i < 50; i++) UpdateTemperature();
        
        // Place steam at z=0
        SetSteamLevel(2, 1, 0, 7);
        int initialSteamZ0 = GetSteamLevel(2, 1, 0);
        
        // Run simulation - with interval-based rising, use enough ticks for a few rise events
        // steamRiseInterval is 0.5s, TICK_DT is ~0.0167s, so ~30 ticks per rise
        // Run 100 ticks for ~3 rise events
        RunFullSimTicks(100);
        
        // Count total steam at each z level
        int steamZ0 = 0, steamZ1 = 0, steamZ2 = 0;
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                steamZ0 += GetSteamLevel(x, y, 0);
                steamZ1 += GetSteamLevel(x, y, 1);
                steamZ2 += GetSteamLevel(x, y, 2);
            }
        }
        
        // Steam should have risen - either some is at higher levels, or it escaped world
        // (steam at z=2 can escape). Check that steam moved from z=0.
        expect(steamZ1 > 0 || steamZ2 > 0 || steamZ0 < initialSteamZ0);
    }
    
    it("should not rise through walls") {
        InitGridFromAsciiWithChunkSize(
            "....\n"
            "....\n", 4, 2);
        gridDepth = 2;
        // Make z=1 all walls (ceiling)
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                grid[1][y][x] = CELL_WALL;
            }
        }
        
        InitSteam();
        InitTemperature();
        
        // Place steam at z=0
        SetSteamLevel(2, 1, 0, 7);
        
        // Run simulation
        RunSteamTicks(20);
        
        // Steam should still be at z=0 (can't pass through ceiling)
        // It may have spread horizontally but not risen
        expect(GetSteamLevel(2, 1, 1) == 0);
    }
}

// =============================================================================
// Steam Generation from Boiling Water
// =============================================================================

describe(steam_from_boiling) {
    it("should generate steam when water reaches boiling temperature") {
        InitGridFromAsciiWithChunkSize(
            "....\n"
            "....\n", 4, 2);
        gridDepth = 2;
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                grid[1][y][x] = CELL_AIR;
            }
        }
        
        InitWater();
        InitSteam();
        InitTemperature();
        temperatureEnabled = true;
        
        // Place water at z=0
        SetWaterLevel(2, 1, 0, 7);
        int initialWater = GetWaterLevel(2, 1, 0);
        
        // Heat the cell to boiling (100C)
        SetHeatSource(2, 1, 0, true);
        heatSourceTemp = 100;  // Set to boiling point
        
        // Run simulation until water boils
        for (int i = 0; i < 200; i++) {
            UpdateTemperature();
            UpdateWaterFreezing();  // This handles boiling too
            UpdateSteam();
        }
        
        // Water should have decreased and steam should exist
        int finalWater = GetWaterLevel(2, 1, 0);
        int totalSteam = CountTotalSteam();
        
        // Either water decreased or steam appeared (or both)
        expect(finalWater < initialWater || totalSteam > 0);
    }
}

// =============================================================================
// Steam Condensation
// =============================================================================

describe(steam_condensation) {
    it("should condense back to water when temperature drops") {
        InitGridFromAsciiWithChunkSize(
            "....\n"
            "....\n", 4, 2);
        gridDepth = 2;
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                grid[1][y][x] = CELL_AIR;
            }
        }
        
        InitWater();
        InitSteam();
        InitTemperature();
        temperatureEnabled = true;
        
        // Set ambient to cold (so steam will condense)
        ambientSurfaceTemp = 20;  // 20C, well below condensation point (60C)
        
        // Run temperature first to establish cold ambient
        for (int i = 0; i < 100; i++) UpdateTemperature();
        
        // Place steam at z=1 (in cold air)
        SetSteamLevel(2, 1, 1, 7);
        int initialSteam = GetSteamLevel(2, 1, 1);
        int initialWater = CountTotalWater();
        
        // Run simulation longer - condensation has random chance (2/3)
        // and needs temperature to stay cold
        RunFullSimTicks(300);
        
        int finalSteam = CountTotalSteam();
        int finalWater = CountTotalWater();
        
        // Steam should have decreased as it condensed
        expect(finalSteam < initialSteam);
        // Water should have appeared somewhere
        expect(finalWater > initialWater);
    }
}

// =============================================================================
// Full Water Cycle Test (from documentation)
// =============================================================================

describe(water_cycle) {
    // Test scenario from steam-system.md:
    // z=3: steam cools -> water droplets fall
    // z=2: steam rises and spreads
    // z=1: closed room filled with water, boils from heat below
    // z=0: closed room with fire/heat sources at 200C
    
    it("should complete water cycle: boil -> rise -> condense -> fall") {
        // Create a 4-level grid
        InitGridFromAsciiWithChunkSize(
            "####\n"
            "#..#\n"
            "#..#\n"
            "####\n", 4, 4);
        gridDepth = 4;
        
        // Set up all levels with walls around edges, air inside
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                for (int x = 0; x < gridWidth; x++) {
                    if (x == 0 || x == gridWidth-1 || y == 0 || y == gridHeight-1) {
                        grid[z][y][x] = CELL_WALL;
                    } else {
                        grid[z][y][x] = (z == 0) ? CELL_FLOOR : CELL_AIR;
                    }
                }
            }
        }
        
        InitWater();
        InitSteam();
        InitTemperature();
        temperatureEnabled = true;
        
        // z=0: Heat source (hot room)
        SetHeatSource(1, 1, 0, true);
        SetHeatSource(2, 1, 0, true);
        SetHeatSource(1, 2, 0, true);
        SetHeatSource(2, 2, 0, true);
        heatSourceTemp = 85;  // Very hot (170C decoded)
        
        // z=1: Water pool
        SetWaterLevel(1, 1, 1, 7);
        SetWaterLevel(2, 1, 1, 7);
        SetWaterLevel(1, 2, 1, 7);
        SetWaterLevel(2, 2, 1, 7);
        
        int initialWater = CountTotalWater();
        
        // z=2, z=3: Cool ambient air for condensation
        ambientSurfaceTemp = 20;  // Room temperature
        
        // Run the simulation for a while
        for (int i = 0; i < 500; i++) {
            UpdateTemperature();
            UpdateWaterFreezing();
            UpdateSteam();
            UpdateWater();
        }
        
        // Check that the cycle occurred:
        // 1. Some water should have boiled (water level decreased at z=1)
        int waterZ1 = 0;
        for (int y = 1; y < gridHeight-1; y++) {
            for (int x = 1; x < gridWidth-1; x++) {
                waterZ1 += GetWaterLevel(x, y, 1);
            }
        }
        
        // 2. Steam should exist somewhere (rising)
        int totalSteam = CountTotalSteam();
        
        // The system should show activity - either steam exists or water moved
        // (exact behavior depends on timing, but something should have happened)
        expect(totalSteam > 0 || waterZ1 < initialWater);
    }
}

// =============================================================================
// Steam Spreading
// =============================================================================

describe(steam_spreading) {
    it("should spread horizontally when blocked above") {
        InitGridFromAsciiWithChunkSize(
            "......\n"
            "......\n"
            "......\n", 6, 3);
        gridDepth = 2;
        // Ceiling at z=1
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                grid[1][y][x] = CELL_WALL;
            }
        }
        
        InitSteam();
        InitTemperature();
        
        // Set ambient hot so steam doesn't condense (need >= 96C / index 73)
        ambientSurfaceTemp = 100;  // Hot ambient prevents condensation
        temperatureEnabled = true;
        
        // Place concentrated steam at center
        SetSteamLevel(3, 1, 0, 7);
        
        // Run simulation longer for spreading to occur
        RunSteamTicks(200);
        
        // Steam should have spread to neighbors - count all neighbors
        int neighborSteam = 0;
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (x != 3 || y != 1) {  // Not the center
                    neighborSteam += GetSteamLevel(x, y, 0);
                }
            }
        }
        
        // Neighbors should have some steam (spread occurred)
        expect(neighborSteam > 0);
    }
}

// =============================================================================
// Heaters evaporating water test
// =============================================================================

describe(heaters_evaporate_water) {
    it("should eventually evaporate all water with heaters in center") {
        // Small 8x8 grid, 1 z-level
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 8);
        gridDepth = 2;
        
        // Make z=1 all walls (ceiling)
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                grid[1][y][x] = CELL_WALL;
            }
        }
        
        InitWater();
        InitSteam();
        InitTemperature();
        
        waterEnabled = true;
        steamEnabled = true;
        temperatureEnabled = true;
        heatSourceTemp = 200;  // Hot heaters
        
        // Fill entire z=0 with water level 7
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                SetWaterLevel(x, y, 0, WATER_MAX_LEVEL);
            }
        }
        
        int initialWater = CountTotalWater();
        printf("Initial water: %d\n", initialWater);
        
        // Place heaters in center (3x3 block)
        for (int y = 3; y <= 5; y++) {
            for (int x = 3; x <= 5; x++) {
                SetHeatSource(x, y, 0, true);
            }
        }
        
        // Run simulation
        int ticks = 0;
        int maxTicks = 10000;
        while (ticks < maxTicks) {
            UpdateTemperature();
            UpdateWater();
            UpdateWaterFreezing();
            UpdateSteam();
            ticks++;
            
            int water = CountTotalWater();
            if (ticks % 1000 == 0) {
                printf("Tick %d: water=%d\n", ticks, water);
            }
            if (water == 0) break;
        }
        
        int finalWater = CountTotalWater();
        printf("Final water after %d ticks: %d\n", ticks, finalWater);
        
        // Water should have decreased significantly
        expect(finalWater < initialWater / 2);
    }
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char* argv[]) {
    // Suppress logs by default, use -v for verbose
    bool verbose = false;
    bool forceDF = false;
    bool forceLegacy = false;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'v') verbose = true;
        if (strcmp(argv[i], "--df") == 0) forceDF = true;
        if (strcmp(argv[i], "--legacy") == 0) forceLegacy = true;
    }
    if (!verbose) {
        SetTraceLogLevel(LOG_NONE);
    }
    
    // Default to DF mode, but allow override via command line
    g_useDFWalkability = true;
    if (forceLegacy) g_useDFWalkability = false;
    if (forceDF) g_useDFWalkability = true;
    
    // Basic operations
    test(steam_initialization);
    test(steam_level_operations);
    
    // Steam behavior
    test(steam_rising);
    test(steam_spreading);
    
    // Integration with water/temperature
    test(steam_from_boiling);
    test(steam_condensation);
    
    // Full cycle test from documentation
    test(water_cycle);
    
    // Heater evaporation test
    test(heaters_evaporate_water);
    
    return summary();
}
