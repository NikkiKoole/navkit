#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/world/material.h"
#include "../src/simulation/groundwear.h"
#include <stdlib.h>
#include <string.h>


// Global flag for verbose output in tests
static bool test_verbose = false;

// =============================================================================
// Basic Initialization
// =============================================================================

describe(groundwear_initialization) {
    it("should initialize wear grid with all zeros") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n", 8, 2);
        
        InitGroundWear();
        
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                for (int x = 0; x < gridWidth; x++) {
                    expect(GetGroundWear(x, y, z) == 0);
                }
            }
        }
    }
    
    it("should clear all wear when ClearGroundWear is called") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n", 8, 2);
        
        InitGroundWear();
        
        // Manually set some wear at different z-levels
        wearGrid[0][0][2] = 100;
        wearGrid[0][1][4] = 200;
        wearGrid[1][0][3] = 150;
        
        expect(GetGroundWear(2, 0, 0) == 100);
        expect(GetGroundWear(4, 1, 0) == 200);
        expect(GetGroundWear(3, 0, 1) == 150);
        
        ClearGroundWear();
        
        expect(GetGroundWear(2, 0, 0) == 0);
        expect(GetGroundWear(4, 1, 0) == 0);
        expect(GetGroundWear(3, 0, 1) == 0);
    }
}

// =============================================================================
// Trampling (now operates on CELL_WALL with MAT_DIRT only)
// =============================================================================

describe(groundwear_trampling) {
    it("should increase wear when dirt is trampled") {
        InitGridFromAsciiWithChunkSize(
            "dddd\n"
            "dddd\n", 4, 2);
        
        // Set up dirt tiles
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                grid[0][y][x] = CELL_WALL; SetWallMaterial(x, y, 0, MAT_DIRT); SetWallNatural(x, y, 0);
            }
        }
        
        InitGroundWear();
        groundWearEnabled = true;
        
        expect(GetGroundWear(2, 1, 0) == 0);
        
        TrampleGround(2, 1, 0);
        
        expect(GetGroundWear(2, 1, 0) == wearTrampleAmount);
    }
    
    it("should accumulate wear over multiple tramplings") {
        InitGridFromAsciiWithChunkSize(
            "dddd\n", 4, 1);
        
        for (int x = 0; x < gridWidth; x++) {
            grid[0][0][x] = CELL_WALL; SetWallMaterial(x, 0, 0, MAT_DIRT); SetWallNatural(x, 0, 0);
        }
        
        InitGroundWear();
        groundWearEnabled = true;
        
        // Trample multiple times
        TrampleGround(2, 0, 0);
        TrampleGround(2, 0, 0);
        TrampleGround(2, 0, 0);
        
        expect(GetGroundWear(2, 0, 0) == wearTrampleAmount * 3);
    }
    
    it("should cap wear at wearMax") {
        InitGridFromAsciiWithChunkSize(
            "dddd\n", 4, 1);
        
        for (int x = 0; x < gridWidth; x++) {
            grid[0][0][x] = CELL_WALL; SetWallMaterial(x, 0, 0, MAT_DIRT); SetWallNatural(x, 0, 0);
        }
        
        InitGroundWear();
        groundWearEnabled = true;
        
        // Set trample amount high for this test
        int originalTrample = wearTrampleAmount;
        int originalMax = wearMax;
        wearTrampleAmount = 200;
        wearMax = 1000;
        
        // Set wear close to max
        wearGrid[0][0][2] = 900;
        
        // Trample - should cap at wearMax (900 + 200 would be 1100, but caps at 1000)
        TrampleGround(2, 0, 0);
        
        expect(GetGroundWear(2, 0, 0) == 1000);
        
        // Restore original values
        wearTrampleAmount = originalTrample;
        wearMax = originalMax;
    }
    
    it("should not trample wall cells") {
        InitGridFromAsciiWithChunkSize(
            "d#dd\n", 4, 1);
        
        grid[0][0][0] = CELL_WALL; SetWallMaterial(0, 0, 0, MAT_DIRT); SetWallNatural(0, 0, 0);
        grid[0][0][1] = CELL_WALL;
        grid[0][0][2] = CELL_WALL; SetWallMaterial(2, 0, 0, MAT_DIRT); SetWallNatural(2, 0, 0);
        grid[0][0][3] = CELL_WALL; SetWallMaterial(3, 0, 0, MAT_DIRT); SetWallNatural(3, 0, 0);
        
        InitGroundWear();
        groundWearEnabled = true;
        
        TrampleGround(1, 0, 0);  // Wall cell
        
        expect(GetGroundWear(1, 0, 0) == 0);
    }
    
    it("should not trample when disabled") {
        InitGridFromAsciiWithChunkSize(
            "dddd\n", 4, 1);
        
        for (int x = 0; x < gridWidth; x++) {
            grid[0][0][x] = CELL_WALL; SetWallMaterial(x, 0, 0, MAT_DIRT); SetWallNatural(x, 0, 0);
        }
        
        InitGroundWear();
        groundWearEnabled = false;
        
        TrampleGround(2, 0, 0);
        
        expect(GetGroundWear(2, 0, 0) == 0);
        
        groundWearEnabled = true;  // Re-enable for other tests
    }
    
    it("should work on any z-level") {
        InitGridFromAsciiWithChunkSize(
            "dddd\n", 4, 1);
        
        // Set up dirt at z=0 and z=1
        for (int x = 0; x < gridWidth; x++) {
            grid[0][0][x] = CELL_WALL; SetWallMaterial(x, 0, 0, MAT_DIRT); SetWallNatural(x, 0, 0);
            grid[1][0][x] = CELL_WALL; SetWallMaterial(x, 0, 1, MAT_DIRT); SetWallNatural(x, 0, 1);
        }
        
        InitGroundWear();
        groundWearEnabled = true;
        
        // Trample at z=0
        TrampleGround(2, 0, 0);
        expect(GetGroundWear(2, 0, 0) == wearTrampleAmount);
        expect(GetGroundWear(2, 0, 1) == 0);
        
        // Trample at z=1
        TrampleGround(2, 0, 1);
        expect(GetGroundWear(2, 0, 1) == wearTrampleAmount);
    }
}

// =============================================================================
// Surface Overlay Changes (replaces grass<->dirt cell transitions)
// =============================================================================

describe(surface_overlay_updates) {
    it("should update surface overlay based on wear level") {
        InitGridFromAsciiWithChunkSize(
            "dddd\n", 4, 1);
        
        for (int x = 0; x < gridWidth; x++) {
            grid[0][0][x] = CELL_WALL; SetWallMaterial(x, 0, 0, MAT_DIRT); SetWallNatural(x, 0, 0);
            SET_CELL_SURFACE(x, 0, 0, SURFACE_TALL_GRASS);
        }
        
        InitGroundWear();
        groundWearEnabled = true;
        
        // Start with tall grass (wear = 0)
        expect(GET_CELL_SURFACE(2, 0, 0) == SURFACE_TALL_GRASS);
        
        // Trample until surface changes
        // Set test thresholds: TALL_GRASS < 20, GRASS 20-59, TRAMPLED 60-99, BARE >= 100
        wearTallToNormal = 20;
        wearNormalToTrampled = 60;
        wearGrassToDirt = 100;
        wearTrampleAmount = 10;
        
        // 2 tramplings = 20 wear -> SURFACE_GRASS
        TrampleGround(2, 0, 0);
        TrampleGround(2, 0, 0);
        expect(GET_CELL_SURFACE(2, 0, 0) == SURFACE_GRASS);
        
        // More trampling to get to TRAMPLED (60)
        for (int i = 0; i < 4; i++) {
            TrampleGround(2, 0, 0);
        }
        expect(GET_CELL_SURFACE(2, 0, 0) == SURFACE_TRAMPLED);
        
        // More to get to BARE (100)
        for (int i = 0; i < 4; i++) {
            TrampleGround(2, 0, 0);
        }
        expect(GET_CELL_SURFACE(2, 0, 0) == SURFACE_BARE);
        
        // Restore
        wearTallToNormal = WEAR_TALL_TO_NORMAL_DEFAULT;
        wearNormalToTrampled = WEAR_NORMAL_TO_TRAMPLED_DEFAULT;
        wearGrassToDirt = WEAR_GRASS_TO_DIRT_DEFAULT;
        wearTrampleAmount = WEAR_TRAMPLE_AMOUNT_DEFAULT;
    }
    
    it("should recover grass overlay as wear decays") {
        InitGridFromAsciiWithChunkSize(
            "dddd\n", 4, 1);
        
        for (int x = 0; x < gridWidth; x++) {
            grid[0][0][x] = CELL_WALL; SetWallMaterial(x, 0, 0, MAT_DIRT); SetWallNatural(x, 0, 0);
        }
        
        InitGroundWear();
        groundWearEnabled = true;
        
        // Set test thresholds: TALL_GRASS < 20, GRASS 20-59, TRAMPLED 60-99, BARE >= 100
        wearTallToNormal = 20;
        wearNormalToTrampled = 60;
        wearGrassToDirt = 100;
        
        // Set high wear (bare dirt)
        wearGrid[0][0][2] = 150;
        SET_CELL_SURFACE(2, 0, 0, SURFACE_BARE);
        
        // Use fast decay for testing
        wearDecayRate = 50;
        wearRecoveryInterval = 0.01f;
        
        expect(GET_CELL_SURFACE(2, 0, 0) == SURFACE_BARE);
        
        // Decay: 150 -> 100 (still bare)
        UpdateGroundWear();
        expect(GET_CELL_SURFACE(2, 0, 0) == SURFACE_BARE);
        
        // Decay: 100 -> 50 (trampled: 60-99) - actually 50 is GRASS
        UpdateGroundWear();
        expect(GET_CELL_SURFACE(2, 0, 0) == SURFACE_GRASS);
        
        // Decay: 50 -> 0 (tall grass: < 20)
        UpdateGroundWear();
        expect(GET_CELL_SURFACE(2, 0, 0) == SURFACE_TALL_GRASS);
        
        // Restore
        wearTallToNormal = WEAR_TALL_TO_NORMAL_DEFAULT;
        wearNormalToTrampled = WEAR_NORMAL_TO_TRAMPLED_DEFAULT;
        wearGrassToDirt = WEAR_GRASS_TO_DIRT_DEFAULT;
        wearDecayRate = WEAR_DECAY_RATE_DEFAULT;
    }
}

// =============================================================================
// Wear Decay
// =============================================================================

describe(wear_decay) {
    it("should decay wear over time") {
        InitGridFromAsciiWithChunkSize(
            "dddd\n", 4, 1);
        
        for (int x = 0; x < gridWidth; x++) {
            grid[0][0][x] = CELL_WALL; SetWallMaterial(x, 0, 0, MAT_DIRT); SetWallNatural(x, 0, 0);
        }
        
        InitGroundWear();
        groundWearEnabled = true;
        
        // Set initial wear
        wearGrid[0][0][2] = 100;
        
        // Set decay interval to fast for testing
        float originalInterval = wearRecoveryInterval;
        wearRecoveryInterval = 0.01f;
        
        int initialWear = GetGroundWear(2, 0, 0);
        
        UpdateGroundWear();
        
        expect(GetGroundWear(2, 0, 0) < initialWear);
        
        // Restore original values
        wearRecoveryInterval = originalInterval;
    }
    
    it("should only decay every N ticks based on decay interval") {
        InitGridFromAsciiWithChunkSize(
            "dddd\n", 4, 1);
        
        for (int x = 0; x < gridWidth; x++) {
            grid[0][0][x] = CELL_WALL; SetWallMaterial(x, 0, 0, MAT_DIRT); SetWallNatural(x, 0, 0);
        }
        
        InitGroundWear();
        groundWearEnabled = true;
        
        // Set decay interval to longer than one tick
        wearRecoveryInterval = 0.1f;
        wearDecayRate = 10;
        
        // Set initial wear
        wearGrid[0][0][2] = 100;
        
        // First few ticks: no decay (accumulator < interval)
        for (int i = 0; i < 3; i++) {
            UpdateGroundWear();
            expect(GetGroundWear(2, 0, 0) == 100);
        }
        
        // After enough ticks, decay should happen
        for (int i = 0; i < 10; i++) {
            UpdateGroundWear();
        }
        expect(GetGroundWear(2, 0, 0) < 100);
        
        // Restore default values
        wearRecoveryInterval = 0.01f;
        wearDecayRate = WEAR_DECAY_RATE_DEFAULT;
    }
    
    it("should clamp wear to 0 on decay") {
        InitGridFromAsciiWithChunkSize(
            "dddd\n", 4, 1);
        
        for (int x = 0; x < gridWidth; x++) {
            grid[0][0][x] = CELL_WALL; SetWallMaterial(x, 0, 0, MAT_DIRT); SetWallNatural(x, 0, 0);
        }
        
        InitGroundWear();
        groundWearEnabled = true;
        
        wearDecayRate = 10;
        wearRecoveryInterval = 0.01f;
        
        // Set wear lower than decay rate
        wearGrid[0][0][2] = 5;
        
        UpdateGroundWear();
        
        expect(GetGroundWear(2, 0, 0) == 0);
        
        // Restore default values
        wearDecayRate = WEAR_DECAY_RATE_DEFAULT;
    }
    
    it("should decay wear at all z-levels") {
        InitGridFromAsciiWithChunkSize(
            "dddd\n", 4, 1);
        
        // Set up dirt at multiple z-levels
        for (int x = 0; x < gridWidth; x++) {
            grid[0][0][x] = CELL_WALL; SetWallMaterial(x, 0, 0, MAT_DIRT); SetWallNatural(x, 0, 0);
            grid[1][0][x] = CELL_WALL; SetWallMaterial(x, 0, 1, MAT_DIRT); SetWallNatural(x, 0, 1);
            grid[2][0][x] = CELL_WALL; SetWallMaterial(x, 0, 2, MAT_DIRT); SetWallNatural(x, 0, 2);
        }
        
        InitGroundWear();
        groundWearEnabled = true;
        
        // Set wear at different z-levels
        wearGrid[0][0][2] = 100;
        wearGrid[1][0][2] = 100;
        wearGrid[2][0][2] = 100;
        
        wearDecayRate = 10;
        wearRecoveryInterval = 0.01f;
        
        UpdateGroundWear();
        
        // All z-levels should have decayed
        expect(GetGroundWear(2, 0, 0) == 90);
        expect(GetGroundWear(2, 0, 1) == 90);
        expect(GetGroundWear(2, 0, 2) == 90);
        
        // Restore
        wearDecayRate = WEAR_DECAY_RATE_DEFAULT;
    }
}

// =============================================================================
// Full Cycle Tests
// =============================================================================

describe(groundwear_full_cycle) {
    it("should complete tall grass->bare->tall grass cycle via surface overlay") {
        InitGridFromAsciiWithChunkSize(
            "dddd\n", 4, 1);
        
        for (int x = 0; x < gridWidth; x++) {
            grid[0][0][x] = CELL_WALL; SetWallMaterial(x, 0, 0, MAT_DIRT); SetWallNatural(x, 0, 0);
            SET_CELL_SURFACE(x, 0, 0, SURFACE_TALL_GRASS);
        }
        
        InitGroundWear();
        groundWearEnabled = true;
        
        // Use test-friendly values
        wearTallToNormal = 20;
        wearNormalToTrampled = 60;
        wearGrassToDirt = 100;  // This is the threshold for BARE surface
        wearTrampleAmount = 50;
        wearDecayRate = 30;
        wearRecoveryInterval = 0.01f;
        
        // Start with tall grass
        expect(GET_CELL_SURFACE(2, 0, 0) == SURFACE_TALL_GRASS);
        expect(GetGroundWear(2, 0, 0) == 0);
        
        // Trample twice to reach bare (2*50=100 >= 100)
        TrampleGround(2, 0, 0);
        TrampleGround(2, 0, 0);
        expect(GET_CELL_SURFACE(2, 0, 0) == SURFACE_BARE);
        expect(GetGroundWear(2, 0, 0) == 100);
        
        // Let it decay back
        // 100 -> 70 (trampled)
        UpdateGroundWear();
        expect(GET_CELL_SURFACE(2, 0, 0) == SURFACE_TRAMPLED);
        
        // 70 -> 40 (grass)
        UpdateGroundWear();
        expect(GET_CELL_SURFACE(2, 0, 0) == SURFACE_GRASS);
        
        // 40 -> 10 (tall grass)
        UpdateGroundWear();
        expect(GET_CELL_SURFACE(2, 0, 0) == SURFACE_TALL_GRASS);
        
        // Restore default values
        wearTallToNormal = WEAR_TALL_TO_NORMAL_DEFAULT;
        wearNormalToTrampled = WEAR_NORMAL_TO_TRAMPLED_DEFAULT;
        wearGrassToDirt = WEAR_GRASS_TO_DIRT_DEFAULT;
        wearTrampleAmount = WEAR_TRAMPLE_AMOUNT_DEFAULT;
        wearDecayRate = WEAR_DECAY_RATE_DEFAULT;
    }
    
    it("should create worn path on heavily trafficked area") {
        InitGridFromAsciiWithChunkSize(
            "dddddddddd\n", 10, 1);
        
        for (int x = 0; x < gridWidth; x++) {
            grid[0][0][x] = CELL_WALL; SetWallMaterial(x, 0, 0, MAT_DIRT); SetWallNatural(x, 0, 0);
            SET_CELL_SURFACE(x, 0, 0, SURFACE_TALL_GRASS);
        }
        
        InitGroundWear();
        groundWearEnabled = true;
        
        // Use test-friendly values
        wearGrassToDirt = 50;
        wearTrampleAmount = 10;
        
        // Simulate heavy traffic on cells 3, 4, 5 (center path)
        for (int i = 0; i < 10; i++) {
            TrampleGround(3, 0, 0);
            TrampleGround(4, 0, 0);
            TrampleGround(5, 0, 0);
        }
        
        // Center cells should be bare (wear = 100 >= 50)
        expect(GET_CELL_SURFACE(3, 0, 0) == SURFACE_BARE);
        expect(GET_CELL_SURFACE(4, 0, 0) == SURFACE_BARE);
        expect(GET_CELL_SURFACE(5, 0, 0) == SURFACE_BARE);
        
        // Edge cells should still have tall grass
        expect(GET_CELL_SURFACE(0, 0, 0) == SURFACE_TALL_GRASS);
        expect(GET_CELL_SURFACE(1, 0, 0) == SURFACE_TALL_GRASS);
        expect(GET_CELL_SURFACE(8, 0, 0) == SURFACE_TALL_GRASS);
        expect(GET_CELL_SURFACE(9, 0, 0) == SURFACE_TALL_GRASS);
        
        // Restore default values
        wearTallToNormal = WEAR_TALL_TO_NORMAL_DEFAULT;
        wearNormalToTrampled = WEAR_NORMAL_TO_TRAMPLED_DEFAULT;
        wearGrassToDirt = WEAR_GRASS_TO_DIRT_DEFAULT;
        wearTrampleAmount = WEAR_TRAMPLE_AMOUNT_DEFAULT;
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

describe(groundwear_edge_cases) {
    it("should handle out-of-bounds queries gracefully") {
        InitGridFromAsciiWithChunkSize(
            "dddd\n", 4, 1);
        
        InitGroundWear();
        
        expect(GetGroundWear(-1, 0, 0) == 0);
        expect(GetGroundWear(100, 0, 0) == 0);
        expect(GetGroundWear(0, -1, 0) == 0);
        expect(GetGroundWear(0, 100, 0) == 0);
        expect(GetGroundWear(0, 0, -1) == 0);
        expect(GetGroundWear(0, 0, 100) == 0);
    }
    
    it("should handle out-of-bounds trampling gracefully") {
        InitGridFromAsciiWithChunkSize(
            "dddd\n", 4, 1);
        
        InitGroundWear();
        groundWearEnabled = true;
        
        // Should not crash
        TrampleGround(-1, 0, 0);
        TrampleGround(100, 0, 0);
        TrampleGround(0, -1, 0);
        TrampleGround(0, 100, 0);
        TrampleGround(0, 0, -1);
        TrampleGround(0, 0, 100);
        
        expect(true);  // If we get here, no crash
    }
    
    it("should not trample non-dirt cells (grass, floor, etc)") {
        InitGridFromAsciiWithChunkSize(
            "dfgw\n", 4, 1);
        
        grid[0][0][0] = CELL_WALL; SetWallMaterial(0, 0, 0, MAT_DIRT); SetWallNatural(0, 0, 0);
        grid[0][0][1] = CELL_AIR;
        SET_FLOOR(1, 0, 0);  // Floor uses flag system
        grid[0][0][2] = CELL_AIR;  // Air cell - not trampled
        grid[0][0][3] = CELL_WALL;
        
        InitGroundWear();
        groundWearEnabled = true;
        
        // Trample all - only dirt should accumulate wear
        TrampleGround(0, 0, 0);
        TrampleGround(1, 0, 0);
        TrampleGround(2, 0, 0);
        TrampleGround(3, 0, 0);
        
        expect(GetGroundWear(0, 0, 0) == wearTrampleAmount);  // Dirt - trampled
        expect(GetGroundWear(1, 0, 0) == 0);  // Floor - not trampled
        expect(GetGroundWear(2, 0, 0) == 0);  // Air - not trampled
        expect(GetGroundWear(3, 0, 0) == 0);  // Wall - not trampled
    }
    
    it("should trample dirt below when walking on floor above (DF mode)") {
        // DF mode: z=0 is dirt with grass, z=1 is where movers walk (air/floor above dirt)
        InitGridWithSizeAndChunkSize(8, 4, 8, 4);
        
        // Set up z=0 as dirt ground with tall grass
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 8; x++) {
                grid[0][y][x] = CELL_WALL; SetWallMaterial(x, y, 0, MAT_DIRT); SetWallNatural(x, y, 0);
                SET_CELL_SURFACE(x, y, 0, SURFACE_TALL_GRASS);
                grid[1][y][x] = CELL_AIR;  // Walking level is air above dirt
            }
        }
        
        InitGroundWear();
        groundWearEnabled = true;
        
        // Trample at z=1 (where movers walk in DF mode)
        // This should affect the dirt at z=0 below
        TrampleGround(2, 1, 1);
        
        // Wear should be applied to z=0 (the dirt below)
        expect(GetGroundWear(2, 1, 0) == wearTrampleAmount);
        expect(GetGroundWear(2, 1, 1) == 0);  // No wear at z=1 (air)
        
        // Surface on z=0 should update based on wear
        TrampleGround(2, 1, 1);  // Trample again
        expect(GetGroundWear(2, 1, 0) == wearTrampleAmount * 2);
    }
    
    it("should not trample when no dirt below floor") {
        InitGridWithSizeAndChunkSize(8, 4, 8, 4);
        
        // z=0 is stone/wall, z=1 is floor - no dirt to trample
        grid[0][1][2] = CELL_WALL;
        grid[1][1][2] = CELL_AIR;
        SET_FLOOR(2, 1, 1);  // Floor uses flag system
        
        InitGroundWear();
        groundWearEnabled = true;
        
        TrampleGround(2, 1, 1);
        
        expect(GetGroundWear(2, 1, 0) == 0);
        expect(GetGroundWear(2, 1, 1) == 0);
    }
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char* argv[]) {
    // Suppress logs by default, use -v for verbose
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
    
    // Run tests
    test(groundwear_initialization);
    test(groundwear_trampling);
    test(surface_overlay_updates);
    test(wear_decay);
    test(groundwear_full_cycle);
    test(groundwear_edge_cases);
    
    return summary();
}
