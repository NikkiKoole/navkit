#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "test_helpers.h"
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
        InitTestGridFromAscii(
            "........\n"
            "........\n");
        
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
        InitTestGridFromAscii(
            "........\n"
            "........\n");
        
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
        InitTestGridFromAscii(
            "dddd\n"
            "dddd\n");
        
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
        InitTestGridFromAscii(
            "dddd\n");
        
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
        InitTestGridFromAscii(
            "dddd\n");
        
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
        InitTestGridFromAscii(
            "d#dd\n");
        
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
        InitTestGridFromAscii(
            "dddd\n");
        
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
        InitTestGridFromAscii(
            "dddd\n");
        
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
        InitTestGridFromAscii(
            "dddd\n");
        
        for (int x = 0; x < gridWidth; x++) {
            grid[0][0][x] = CELL_WALL; SetWallMaterial(x, 0, 0, MAT_DIRT); SetWallNatural(x, 0, 0);
            SetVegetation(x, 0, 0, VEG_GRASS_TALL);
        }
        
        InitGroundWear();
        groundWearEnabled = true;
        
        // Start with tall grass (wear = 0)
        expect(GetVegetation(2, 0, 0) == VEG_GRASS_TALL);
        
        // Trample until surface changes
        // Set test thresholds: TALLER < 5, TALL 5-19, SHORT 20-59, TRAMPLED 60-99, BARE >= 100
        wearTallerToTall = 5;
        wearTallToNormal = 20;
        wearNormalToTrampled = 60;
        wearGrassToDirt = 100;
        wearTrampleAmount = 10;
        
        // 2 tramplings = 20 wear -> VEG_GRASS_SHORT
        TrampleGround(2, 0, 0);
        TrampleGround(2, 0, 0);
        expect(GetVegetation(2, 0, 0) == VEG_GRASS_SHORT);
        
        // More trampling to get to TRAMPLED (60) -> no vegetation
        for (int i = 0; i < 4; i++) {
            TrampleGround(2, 0, 0);
        }
        expect(GET_CELL_SURFACE(2, 0, 0) == SURFACE_TRAMPLED);
        expect(GetVegetation(2, 0, 0) == VEG_NONE);
        
        // More to get to BARE (100) -> no vegetation
        for (int i = 0; i < 4; i++) {
            TrampleGround(2, 0, 0);
        }
        expect(GET_CELL_SURFACE(2, 0, 0) == SURFACE_BARE);
        expect(GetVegetation(2, 0, 0) == VEG_NONE);
        
        // Restore
        wearTallerToTall = WEAR_TALLER_TO_TALL_DEFAULT;
        wearTallToNormal = WEAR_TALL_TO_NORMAL_DEFAULT;
        wearNormalToTrampled = WEAR_NORMAL_TO_TRAMPLED_DEFAULT;
        wearGrassToDirt = WEAR_GRASS_TO_DIRT_DEFAULT;
        wearTrampleAmount = WEAR_TRAMPLE_AMOUNT_DEFAULT;
    }
    
    it("should recover grass overlay as wear decays") {
        InitTestGridFromAscii(
            "dddd\n");
        
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
        SetVegetation(2, 0, 0, VEG_NONE);
        
        // Use fast decay for testing
        wearDecayRate = 50;
        wearRecoveryInterval = 0.01f;
        
        expect(GetVegetation(2, 0, 0) == VEG_NONE);
        
        // Decay: 150 -> 100 (still bare)
        UpdateGroundWear();
        expect(GetVegetation(2, 0, 0) == VEG_NONE);
        
        // Decay: 100 -> 50 (short grass: 20-59)
        UpdateGroundWear();
        expect(GetVegetation(2, 0, 0) == VEG_GRASS_SHORT);
        
        // Decay: 50 -> 0 (taller grass: wear == 0)
        UpdateGroundWear();
        expect(GetVegetation(2, 0, 0) == VEG_GRASS_TALLER);
        
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
        InitTestGridFromAscii(
            "dddd\n");
        
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
        InitTestGridFromAscii(
            "dddd\n");
        
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
        InitTestGridFromAscii(
            "dddd\n");
        
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
        InitTestGridFromAscii(
            "dddd\n");
        seasonalAmplitude = 0;
        
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
        InitTestGridFromAscii(
            "dddd\n");
        seasonalAmplitude = 0;
        
        for (int x = 0; x < gridWidth; x++) {
            grid[0][0][x] = CELL_WALL; SetWallMaterial(x, 0, 0, MAT_DIRT); SetWallNatural(x, 0, 0);
            SetVegetation(x, 0, 0, VEG_GRASS_TALL);
        }
        
        InitGroundWear();
        groundWearEnabled = true;
        
        // Use test-friendly values
        wearTallerToTall = 5;
        wearTallToNormal = 20;
        wearNormalToTrampled = 60;
        wearGrassToDirt = 100;  // This is the threshold for BARE surface
        wearTrampleAmount = 50;
        wearDecayRate = 30;
        wearRecoveryInterval = 0.01f;
        
        // Start with tall grass
        expect(GetVegetation(2, 0, 0) == VEG_GRASS_TALL);
        expect(GetGroundWear(2, 0, 0) == 0);
        
        // Trample twice to reach bare (2*50=100 >= 100)
        TrampleGround(2, 0, 0);
        TrampleGround(2, 0, 0);
        expect(GET_CELL_SURFACE(2, 0, 0) == SURFACE_BARE);
        expect(GetVegetation(2, 0, 0) == VEG_NONE);
        expect(GetGroundWear(2, 0, 0) == 100);
        
        // Let it decay back
        // 100 -> 70 (trampled)
        UpdateGroundWear();
        expect(GET_CELL_SURFACE(2, 0, 0) == SURFACE_TRAMPLED);
        expect(GetVegetation(2, 0, 0) == VEG_NONE);
        
        // 70 -> 40 (short grass)
        UpdateGroundWear();
        expect(GetVegetation(2, 0, 0) == VEG_GRASS_SHORT);
        
        // 40 -> 10 (tall grass)
        UpdateGroundWear();
        expect(GetVegetation(2, 0, 0) == VEG_GRASS_TALL);
        
        // Restore default values
        wearTallerToTall = WEAR_TALLER_TO_TALL_DEFAULT;
        wearTallToNormal = WEAR_TALL_TO_NORMAL_DEFAULT;
        wearNormalToTrampled = WEAR_NORMAL_TO_TRAMPLED_DEFAULT;
        wearGrassToDirt = WEAR_GRASS_TO_DIRT_DEFAULT;
        wearTrampleAmount = WEAR_TRAMPLE_AMOUNT_DEFAULT;
        wearDecayRate = WEAR_DECAY_RATE_DEFAULT;
    }
    
    it("should create worn path on heavily trafficked area") {
        InitTestGridFromAscii(
            "dddddddddd\n");
        
        for (int x = 0; x < gridWidth; x++) {
            grid[0][0][x] = CELL_WALL; SetWallMaterial(x, 0, 0, MAT_DIRT); SetWallNatural(x, 0, 0);
            SetVegetation(x, 0, 0, VEG_GRASS_TALL);
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
        expect(GetVegetation(3, 0, 0) == VEG_NONE);
        
        // Edge cells should still have tall grass
        expect(GetVegetation(0, 0, 0) == VEG_GRASS_TALL);
        expect(GetVegetation(1, 0, 0) == VEG_GRASS_TALL);
        expect(GetVegetation(8, 0, 0) == VEG_GRASS_TALL);
        expect(GetVegetation(9, 0, 0) == VEG_GRASS_TALL);
        
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
        InitTestGridFromAscii(
            "dddd\n");
        
        InitGroundWear();
        
        expect(GetGroundWear(-1, 0, 0) == 0);
        expect(GetGroundWear(100, 0, 0) == 0);
        expect(GetGroundWear(0, -1, 0) == 0);
        expect(GetGroundWear(0, 100, 0) == 0);
        expect(GetGroundWear(0, 0, -1) == 0);
        expect(GetGroundWear(0, 0, 100) == 0);
    }
    
    it("should handle out-of-bounds trampling gracefully") {
        InitTestGridFromAscii(
            "dddd\n");
        
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
        InitTestGridFromAscii(
            "dfgw\n");
        
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
        InitTestGrid(8, 4);
        
        // Set up z=0 as dirt ground with tall grass
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 8; x++) {
                grid[0][y][x] = CELL_WALL; SetWallMaterial(x, y, 0, MAT_DIRT); SetWallNatural(x, y, 0);
                SetVegetation(x, y, 0, VEG_GRASS_TALL);
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
        InitTestGrid(8, 4);
        
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
