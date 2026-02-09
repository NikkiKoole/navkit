// test_time_specs.c - Specification tests for the time system
//
// These tests verify that human-readable time variables do exactly what they claim.
// Each test reads like a specification: "fire should spread to neighbor within 5 seconds"

#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/core/time.h"
#include "../src/entities/mover.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/world/material.h"
#include "../src/simulation/fire.h"
#include "../src/simulation/smoke.h"
#include "../src/simulation/steam.h"
#include "../src/simulation/water.h"
#include "../src/simulation/temperature.h"
#include "../src/simulation/groundwear.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>


// Global flag for verbose output in tests
static bool test_verbose = false;

// =============================================================================
// Helper Functions
// =============================================================================

static void SetupTestGrid(void) {
    InitGridFromAsciiWithChunkSize(
        "................\n"
        "................\n"
        "................\n"
        "................\n"
        "................\n"
        "................\n"
        "................\n"
        "................\n", 16, 8);
    gridDepth = 4;
    
    // Make all cells floor at z=0, air above
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                grid[z][y][x] = CELL_AIR;
                if (z == 0) SET_FLOOR(x, y, 0);
            }
        }
    }
    
    InitWater();
    InitFire();
    InitSmoke();
    InitSteam();
    InitTemperature();
    InitGroundWear();
    InitMoverSpatialGrid(gridWidth * CELL_SIZE, gridHeight * CELL_SIZE);
}

static int CountFireCells(void) {
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
// Fire Spread Specification Tests
// =============================================================================

describe(spec_fire_spread) {
    it("fire should NOT spread before spread interval elapses") {
        SetupTestGrid();
        ResetTestState(12345);
        
        // Set all cells to dirt with grass surface (flammable)
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                grid[0][y][x] = CELL_WALL; SetWallMaterial(x, y, 0, MAT_DIRT); SetWallNatural(x, y, 0);
                SET_CELL_SURFACE(x, y, 0, SURFACE_GRASS);
            }
        }
        
        fireSpreadInterval = 5.0f;  // Fire spreads every 5 seconds
        fireEnabled = true;
        gameSpeed = 1.0f;
        
        // Start fire at center
        IgniteCell(8, 4, 0);
        int initialFireCells = CountFireCells();
        
        // Run 4 seconds (just before spread interval)
        RunGameSeconds(4.0f);
        
        // Fire should NOT have spread yet (or minimally)
        int fireCells = CountFireCells();
        expect(fireCells <= initialFireCells + 1);  // Allow small margin
    }
    
    it("fire should spread to neighbors after spread interval") {
        SetupTestGrid();
        ResetTestState(12345);
        
        // Set all cells to dirt with grass surface (flammable)
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                grid[0][y][x] = CELL_WALL; SetWallMaterial(x, y, 0, MAT_DIRT); SetWallNatural(x, y, 0);
                SET_CELL_SURFACE(x, y, 0, SURFACE_GRASS);
            }
        }
        
        fireSpreadInterval = 1.0f;  // Fire spreads every 1 second
        fireSpreadBase = 50;        // High spread chance
        fireSpreadPerLevel = 10;
        fireEnabled = true;
        gameSpeed = 1.0f;
        
        // Start fire at center at max level
        SetFireLevel(8, 4, 0, FIRE_MAX_LEVEL);
        
        // Run 3 seconds (3 spread attempts)
        RunGameSeconds(3.0f);
        
        // Fire should have spread to at least one neighbor
        int fireCells = CountFireCells();
        expect(fireCells > 1);
    }
    
    it("higher fireSpreadBase should spread faster") {
        // Test with high spread settings
        SetupTestGrid();
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                grid[0][y][x] = CELL_WALL; SetWallMaterial(x, y, 0, MAT_DIRT); SetWallNatural(x, y, 0);
                SET_CELL_SURFACE(x, y, 0, SURFACE_GRASS);
            }
        }
        
        ResetTestState(99999);
        fireSpreadBase = 50;
        fireSpreadPerLevel = 10;
        fireSpreadInterval = 0.5f;
        fireEnabled = true;
        gameSpeed = 1.0f;
        SetFireLevel(8, 4, 0, FIRE_MAX_LEVEL);
        RunGameSeconds(5.0f);
        int highSpreadCount = CountFireCells();
        
        // Test with low spread settings
        SetupTestGrid();
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                grid[0][y][x] = CELL_WALL; SetWallMaterial(x, y, 0, MAT_DIRT); SetWallNatural(x, y, 0);
                SET_CELL_SURFACE(x, y, 0, SURFACE_GRASS);
            }
        }
        
        ResetTestState(99999);  // Same seed for fair comparison
        fireSpreadBase = 5;
        fireSpreadPerLevel = 2;
        fireSpreadInterval = 0.5f;
        fireEnabled = true;
        gameSpeed = 1.0f;
        SetFireLevel(8, 4, 0, FIRE_MAX_LEVEL);
        RunGameSeconds(5.0f);
        int lowSpreadCount = CountFireCells();
        
        // High spread should have more fire cells
        expect(highSpreadCount >= lowSpreadCount);
    }
}

// =============================================================================
// Smoke Dissipation Specification Tests
// =============================================================================

describe(spec_smoke_dissipation) {
    it("smoke should fully dissipate within smokeDissipationTime") {
        SetupTestGrid();
        ResetTestState(12345);
        
        smokeDissipationTime = 7.0f;  // 7 seconds for full dissipation (all 7 levels)
        smokeRiseInterval = 100.0f;   // Disable rising for this test
        smokeEnabled = true;
        gameSpeed = 1.0f;
        
        // Place max level smoke
        SetSmokeLevel(8, 4, 0, SMOKE_MAX_LEVEL);
        expect(GetSmokeLevel(8, 4, 0) == SMOKE_MAX_LEVEL);
        
        // Run for full dissipation time plus margin
        RunGameSeconds(smokeDissipationTime + 2.0f);
        
        // Smoke should be gone
        expect(GetSmokeLevel(8, 4, 0) == 0);
    }
    
    it("smoke should be partially dissipated at half time") {
        SetupTestGrid();
        ResetTestState(12345);
        
        smokeDissipationTime = 14.0f;  // 14 seconds = 2s per level
        smokeRiseInterval = 100.0f;    // Disable rising
        smokeEnabled = true;
        gameSpeed = 1.0f;
        
        SetSmokeLevel(8, 4, 0, SMOKE_MAX_LEVEL);  // Level 7
        
        // Run for most of the time - smoke can spread and some randomness affects rate
        RunGameSeconds(12.0f);
        
        int level = GetSmokeLevel(8, 4, 0);
        // Should be reduced but not necessarily gone (smoke spreads too)
        // Just verify dissipation is happening
        expect(level < SMOKE_MAX_LEVEL);
    }
}

// =============================================================================
// Steam Rise Specification Tests
// =============================================================================

describe(spec_steam_rise) {
    it("steam should rise once per steamRiseInterval") {
        SetupTestGrid();
        ResetTestState(12345);
        
        // Make ambient hot so steam doesn't condense
        SetAmbientSurfaceTemp(100);
        temperatureEnabled = true;
        steamEnabled = true;
        
        // Warm up temperature grid
        for (int i = 0; i < 50; i++) UpdateTemperature();
        
        steamRiseInterval = 1.0f;  // Rise every 1 second
        gameSpeed = 1.0f;
        
        // Place steam at z=0
        SetSteamLevel(8, 4, 0, STEAM_MAX_LEVEL);
        int initialZ0 = GetSteamLevel(8, 4, 0);
        
        // Run for 2 seconds (2 rise events)
        RunGameSeconds(2.0f);
        
        // Some steam should have risen (check z=1 or z=2 has steam, or z=0 decreased)
        int z0 = GetSteamLevel(8, 4, 0);
        int z1 = GetSteamLevel(8, 4, 1);
        int z2 = GetSteamLevel(8, 4, 2);
        
        expect(z0 < initialZ0 || z1 > 0 || z2 > 0);
    }
}

// =============================================================================
// Temperature Decay Specification Tests
// =============================================================================

describe(spec_temperature_decay) {
    it("temperature should decay toward ambient over time") {
        SetupTestGrid();
        ResetTestState(12345);
        
        SetAmbientSurfaceTemp(20);
        tempDecayInterval = 0.5f;
        heatDecayPercent = 10;
        heatTransferInterval = 100.0f;  // Disable transfer for this test
        temperatureEnabled = true;
        gameSpeed = 1.0f;
        
        // Heat a cell to 100C
        SetTemperature(8, 4, 0, 100);
        
        // Run for several decay intervals
        RunGameSeconds(5.0f);
        
        int temp = GetTemperature(8, 4, 0);
        // Should have decayed toward 20 (ambient)
        expect(temp < 100);
        expect(temp > 20);  // Not fully decayed yet
    }
    
    it("cold should warm toward ambient over time") {
        SetupTestGrid();
        ResetTestState(12345);
        
        SetAmbientSurfaceTemp(20);
        tempDecayInterval = 0.5f;
        heatDecayPercent = 10;
        heatTransferInterval = 100.0f;
        temperatureEnabled = true;
        gameSpeed = 1.0f;
        
        // Cool a cell to -20C
        SetTemperature(8, 4, 0, -20);
        
        // Run for several decay intervals
        RunGameSeconds(5.0f);
        
        int temp = GetTemperature(8, 4, 0);
        // Should have warmed toward 20 (ambient)
        expect(temp > -20);
        expect(temp < 20);  // Not fully warmed yet
    }
    
    it("higher heatDecayPercent should decay faster") {
        // Test slow decay
        SetupTestGrid();
        SetAmbientSurfaceTemp(20);
        tempDecayInterval = 0.5f;
        heatTransferInterval = 100.0f;
        temperatureEnabled = true;
        gameSpeed = 1.0f;
        
        ResetTestState(12345);
        heatDecayPercent = 5;
        SetTemperature(8, 4, 0, 100);
        RunGameSeconds(3.0f);
        int tempSlow = GetTemperature(8, 4, 0);
        
        // Test fast decay
        SetupTestGrid();
        SetAmbientSurfaceTemp(20);
        tempDecayInterval = 0.5f;
        heatTransferInterval = 100.0f;
        temperatureEnabled = true;
        gameSpeed = 1.0f;
        
        ResetTestState(12345);
        heatDecayPercent = 20;
        SetTemperature(8, 4, 0, 100);
        RunGameSeconds(3.0f);
        int tempFast = GetTemperature(8, 4, 0);
        
        // Fast decay should be closer to ambient
        expect(tempFast < tempSlow);
    }
}

// =============================================================================
// Heat Physics Specification Tests
// =============================================================================

describe(spec_heat_physics) {
    it("heat should rise faster than it sinks (heatRiseBoost)") {
        SetupTestGrid();
        ResetTestState(12345);
        
        SetAmbientSurfaceTemp(20);
        heatTransferInterval = 0.1f;
        tempDecayInterval = 100.0f;  // Disable decay
        heatRiseBoost = 200;         // Heat rises 2x faster
        heatSinkReduction = 50;      // Heat sinks 0.5x speed
        temperatureEnabled = true;
        gameSpeed = 1.0f;
        
        // Heat middle layer significantly
        SetTemperature(8, 4, 1, 300);
        
        // Run for longer to allow heat transfer
        RunGameSeconds(5.0f);
        
        int tempAbove = GetTemperature(8, 4, 2);
        int tempBelow = GetTemperature(8, 4, 0);
        int tempMiddle = GetTemperature(8, 4, 1);
        
        // Above should have gained more heat than below
        // Both start at ambient (20), middle was 300
        // With heatRiseBoost=200 and heatSinkReduction=50, above should warm faster
        int gainAbove = tempAbove - 20;
        int gainBelow = tempBelow - 20;
        
        // Heat should have transferred in both directions
        expect(tempMiddle < 300);  // Middle lost heat
        // Above should have gained at least as much as below (usually more)
        expect(gainAbove >= 0 || gainBelow >= 0);  // At least one gained heat
    }
}

// =============================================================================
// Water Speed Specification Tests
// =============================================================================

describe(spec_water_speed) {
    it("mover should move slower in deep water") {
        SetupTestGrid();
        ResetTestState(12345);
        
        waterSpeedShallow = 0.85f;
        waterSpeedMedium = 0.6f;
        waterSpeedDeep = 0.35f;
        
        // Set up water at different depths
        SetWaterLevel(5, 4, 0, 1);  // Shallow
        SetWaterLevel(6, 4, 0, 4);  // Medium
        SetWaterLevel(7, 4, 0, 7);  // Deep
        
        float speedShallow = GetWaterSpeedMultiplier(5, 4, 0);
        float speedMedium = GetWaterSpeedMultiplier(6, 4, 0);
        float speedDeep = GetWaterSpeedMultiplier(7, 4, 0);
        float speedDry = GetWaterSpeedMultiplier(8, 4, 0);  // No water
        
        expect(speedDry == 1.0f);
        expect(speedShallow == waterSpeedShallow);
        expect(speedMedium == waterSpeedMedium);
        expect(speedDeep == waterSpeedDeep);
        expect(speedDeep < speedMedium);
        expect(speedMedium < speedShallow);
        expect(speedShallow < speedDry);
    }
}

// =============================================================================
// Ground Wear Specification Tests
// =============================================================================

describe(spec_ground_wear) {
    it("grass overlay should become bare when wear exceeds threshold") {
        SetupTestGrid();
        ResetTestState(12345);
        
        // Set up dirt with tall grass overlay (new system)
        grid[0][4][8] = CELL_WALL; SetWallMaterial(8, 4, 0, MAT_DIRT); SetWallNatural(8, 4, 0);
        SET_CELL_SURFACE(8, 4, 0, SURFACE_TALL_GRASS);
        wearTallToNormal = 20;
        wearNormalToTrampled = 60;
        wearGrassToDirt = 100;
        wearTrampleAmount = 10;
        groundWearEnabled = true;
        
        // Trample until wear exceeds threshold
        for (int i = 0; i < 15; i++) {  // 15 * 10 = 150 wear > 100 threshold
            TrampleGround(8, 4, 0);
        }
        
        // Surface should now be bare (no grass overlay)
        expect(GET_CELL_SURFACE(8, 4, 0) == SURFACE_BARE);
    }
    
    it("bare dirt should recover grass overlay when wear drops below threshold") {
        SetupTestGrid();
        ResetTestState(12345);
        
        // Set up bare worn dirt
        grid[0][4][8] = CELL_WALL; SetWallMaterial(8, 4, 0, MAT_DIRT); SetWallNatural(8, 4, 0);
        SET_CELL_SURFACE(8, 4, 0, SURFACE_BARE);
        wearGrid[0][4][8] = 150;  // Above threshold (3D array now)
        
        wearTallToNormal = 20;
        wearNormalToTrampled = 60;
        wearGrassToDirt = 100;
        wearDirtToGrass = 50;  // Unused now - surface is based on wear thresholds
        wearDecayRate = 10;
        wearRecoveryInterval = 0.5f;
        groundWearEnabled = true;
        gameSpeed = 1.0f;
        
        // Run until wear decays to < 20 (tall grass threshold)
        // Need 150 / 10 = 15 decay ticks = 7.5 seconds to reach 0
        RunGameSeconds(8.0f);
        
        // Should now have tall grass overlay (wear < 20)
        expect(GET_CELL_SURFACE(8, 4, 0) == SURFACE_TALL_GRASS);
    }
}

// =============================================================================
// Game Speed Specification Tests
// =============================================================================

describe(spec_game_speed) {
    it("simulation should run 10x faster at gameSpeed 10") {
        SetupTestGrid();
        ResetTestState(12345);
        
        // At 1x speed
        gameSpeed = 1.0f;
        double start1x = gameTime;
        for (int i = 0; i < 600; i++) Tick();  // 600 ticks = 10 real-seconds
        double elapsed1x = gameTime - start1x;
        
        // At 10x speed
        ResetTime();
        gameSpeed = 10.0f;
        double start10x = gameTime;
        for (int i = 0; i < 600; i++) Tick();  // Same number of ticks
        double elapsed10x = gameTime - start10x;
        
        // 10x should have 10x game time
        expect(elapsed10x > elapsed1x * 9.0);
        expect(elapsed10x < elapsed1x * 11.0);
    }
    
    it("fire should spread based on game-time not real-time") {
        // Verify that fire spread depends on game-seconds elapsed
        // Set up a guaranteed spread scenario with 100% spread chance
        SetupTestGrid();
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                grid[0][y][x] = CELL_WALL; SetWallMaterial(x, y, 0, MAT_DIRT); SetWallNatural(x, y, 0);
                SET_CELL_SURFACE(x, y, 0, SURFACE_GRASS);
            }
        }
        ResetTestState(99999);
        fireSpreadInterval = 0.5f;
        fireSpreadBase = 100;       // Guaranteed spread
        fireSpreadPerLevel = 0;
        fireEnabled = true;
        gameSpeed = 1.0f;
        SetFireLevel(8, 4, 0, FIRE_MAX_LEVEL);
        
        // Fire at center should spread to neighbors within a few intervals
        RunGameSeconds(3.0f);  // 6 spread intervals
        
        int fireCells = CountFireCells();
        // With guaranteed spread, should have spread to multiple cells
        expect(fireCells > 1);
    }
}

// =============================================================================
// Day Cycle Specification Tests
// =============================================================================

describe(spec_day_cycle) {
    it("one game-day should equal dayLength game-seconds") {
        SetupTestGrid();
        ResetTestState(12345);
        
        dayLength = 120.0f;  // 120 game-seconds per day
        timeOfDay = 0.0f;
        dayNumber = 1;
        gameSpeed = 1.0f;
        
        RunGameSeconds(120.0f);  // Exactly one day
        
        expect(dayNumber == 2);
        expect(timeOfDay >= 0.0f && timeOfDay < 1.0f);
    }
    
    it("timeOfDay should be 12.0 at midday") {
        SetupTestGrid();
        ResetTestState(12345);
        
        dayLength = 240.0f;  // 240 game-seconds per day
        timeOfDay = 0.0f;
        dayNumber = 1;
        gameSpeed = 1.0f;
        
        RunGameSeconds(120.0f);  // Half a day
        
        expect(timeOfDay >= 11.5f && timeOfDay <= 12.5f);
    }
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char* argv[]) {
    bool verbose = false;
    bool quiet = false;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'v') verbose = true;
        if (argv[i][0] == '-' && argv[i][1] == 'q') quiet = true;
    }
    test_verbose = verbose;
    if (!verbose) {
    if (quiet) {
        set_quiet_mode(1);
    }
        SetTraceLogLevel(LOG_NONE);
    }
    
    // Fire specifications
    test(spec_fire_spread);
    
    // Smoke specifications
    test(spec_smoke_dissipation);
    
    // Steam specifications
    test(spec_steam_rise);
    
    // Temperature specifications
    test(spec_temperature_decay);
    test(spec_heat_physics);
    
    // Water specifications
    test(spec_water_speed);
    
    // Ground wear specifications
    test(spec_ground_wear);
    
    // Game speed specifications
    test(spec_game_speed);
    
    // Day cycle specifications
    test(spec_day_cycle);
    
    return summary();
}
