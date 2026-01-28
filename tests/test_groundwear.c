#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/simulation/groundwear.h"
#include <stdlib.h>

// =============================================================================
// Basic Initialization
// =============================================================================

describe(groundwear_initialization) {
    it("should initialize wear grid with all zeros") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n", 8, 2);
        
        InitGroundWear();
        
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                expect(GetGroundWear(x, y) == 0);
            }
        }
    }
    
    it("should clear all wear when ClearGroundWear is called") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n", 8, 2);
        
        InitGroundWear();
        
        // Manually set some wear
        wearGrid[0][2] = 100;
        wearGrid[1][4] = 200;
        
        expect(GetGroundWear(2, 0) == 100);
        expect(GetGroundWear(4, 1) == 200);
        
        ClearGroundWear();
        
        expect(GetGroundWear(2, 0) == 0);
        expect(GetGroundWear(4, 1) == 0);
    }
}

// =============================================================================
// Trampling
// =============================================================================

describe(groundwear_trampling) {
    it("should increase wear when trampled") {
        InitGridFromAsciiWithChunkSize(
            "gggg\n"
            "gggg\n", 4, 2);
        
        // Convert to CELL_GRASS
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                grid[0][y][x] = CELL_GRASS;
            }
        }
        
        InitGroundWear();
        groundWearEnabled = true;
        
        expect(GetGroundWear(2, 1) == 0);
        
        TrampleGround(2, 1);
        
        expect(GetGroundWear(2, 1) == wearTrampleAmount);
    }
    
    it("should accumulate wear over multiple tramplings") {
        InitGridFromAsciiWithChunkSize(
            "gggg\n", 4, 1);
        
        for (int x = 0; x < gridWidth; x++) {
            grid[0][0][x] = CELL_GRASS;
        }
        
        InitGroundWear();
        groundWearEnabled = true;
        
        // Trample multiple times
        TrampleGround(2, 0);
        TrampleGround(2, 0);
        TrampleGround(2, 0);
        
        expect(GetGroundWear(2, 0) == wearTrampleAmount * 3);
    }
    
    it("should cap wear at wearMax") {
        InitGridFromAsciiWithChunkSize(
            "gggg\n", 4, 1);
        
        for (int x = 0; x < gridWidth; x++) {
            grid[0][0][x] = CELL_GRASS;
        }
        
        InitGroundWear();
        groundWearEnabled = true;
        
        // Set trample amount high for this test
        int originalTrample = wearTrampleAmount;
        int originalMax = wearMax;
        wearTrampleAmount = 200;
        wearMax = 1000;
        
        // Set wear close to max
        wearGrid[0][2] = 900;
        
        // Trample - should cap at wearMax (900 + 200 would be 1100, but caps at 1000)
        TrampleGround(2, 0);
        
        expect(GetGroundWear(2, 0) == 1000);
        
        // Restore original values
        wearTrampleAmount = originalTrample;
        wearMax = originalMax;
    }
    
    it("should not trample wall cells") {
        InitGridFromAsciiWithChunkSize(
            "g#gg\n", 4, 1);
        
        grid[0][0][0] = CELL_GRASS;
        grid[0][0][1] = CELL_WALL;
        grid[0][0][2] = CELL_GRASS;
        grid[0][0][3] = CELL_GRASS;
        
        InitGroundWear();
        groundWearEnabled = true;
        
        TrampleGround(1, 0);  // Wall cell
        
        expect(GetGroundWear(1, 0) == 0);
    }
    
    it("should not trample when disabled") {
        InitGridFromAsciiWithChunkSize(
            "gggg\n", 4, 1);
        
        for (int x = 0; x < gridWidth; x++) {
            grid[0][0][x] = CELL_GRASS;
        }
        
        InitGroundWear();
        groundWearEnabled = false;
        
        TrampleGround(2, 0);
        
        expect(GetGroundWear(2, 0) == 0);
        
        groundWearEnabled = true;  // Re-enable for other tests
    }
}

// =============================================================================
// Grass to Dirt Conversion
// =============================================================================

describe(grass_to_dirt_conversion) {
    it("should convert grass to dirt when wear exceeds threshold") {
        InitGridFromAsciiWithChunkSize(
            "gggg\n", 4, 1);
        
        for (int x = 0; x < gridWidth; x++) {
            grid[0][0][x] = CELL_GRASS;
        }
        
        InitGroundWear();
        groundWearEnabled = true;
        
        // Use low threshold for testing
        int originalThreshold = wearGrassToDirt;
        wearGrassToDirt = 10;
        wearTrampleAmount = 5;
        
        expect(grid[0][0][2] == CELL_GRASS);
        
        // First trample - wear = 5, below threshold
        TrampleGround(2, 0);
        expect(grid[0][0][2] == CELL_GRASS);
        
        // Second trample - wear = 10, at threshold
        TrampleGround(2, 0);
        expect(grid[0][0][2] == CELL_DIRT);
        
        // Restore original values
        wearGrassToDirt = originalThreshold;
        wearTrampleAmount = WEAR_TRAMPLE_AMOUNT_DEFAULT;
    }
    
    it("should convert CELL_WALKABLE to dirt (legacy support)") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        
        // CELL_WALKABLE is the legacy cell type
        for (int x = 0; x < gridWidth; x++) {
            grid[0][0][x] = CELL_WALKABLE;
        }
        
        InitGroundWear();
        groundWearEnabled = true;
        
        // Use low threshold for testing
        int originalThreshold = wearGrassToDirt;
        wearGrassToDirt = 5;
        wearTrampleAmount = 10;
        
        expect(grid[0][0][2] == CELL_WALKABLE);
        
        TrampleGround(2, 0);
        
        expect(grid[0][0][2] == CELL_DIRT);
        
        // Restore original values
        wearGrassToDirt = originalThreshold;
        wearTrampleAmount = WEAR_TRAMPLE_AMOUNT_DEFAULT;
    }
    
    it("should require many tramplings with default values") {
        InitGridFromAsciiWithChunkSize(
            "gggg\n", 4, 1);
        
        for (int x = 0; x < gridWidth; x++) {
            grid[0][0][x] = CELL_GRASS;
        }
        
        InitGroundWear();
        groundWearEnabled = true;
        
        // Reset to default values
        wearGrassToDirt = WEAR_GRASS_TO_DIRT_DEFAULT;  // 100
        wearTrampleAmount = WEAR_TRAMPLE_AMOUNT_DEFAULT;  // 1
        
        // Single trample should NOT convert grass to dirt
        TrampleGround(2, 0);
        expect(grid[0][0][2] == CELL_GRASS);
        
        // 50 tramplings shouldn't convert (50*1=50, still < 100)
        for (int i = 0; i < 49; i++) {
            TrampleGround(2, 0);
        }
        expect(grid[0][0][2] == CELL_GRASS);
        
        // Need 100 tramplings (100*1=100 >= threshold)
        for (int i = 0; i < 50; i++) {  // Already did 50, need 50 more
            TrampleGround(2, 0);
        }
        expect(grid[0][0][2] == CELL_DIRT);
    }
}

// =============================================================================
// Dirt to Grass Conversion (Decay)
// =============================================================================

describe(dirt_to_grass_conversion) {
    it("should decay wear over time") {
        InitGridFromAsciiWithChunkSize(
            "dddd\n", 4, 1);
        
        for (int x = 0; x < gridWidth; x++) {
            grid[0][0][x] = CELL_DIRT;
        }
        
        InitGroundWear();
        groundWearEnabled = true;
        
        // Set initial wear
        wearGrid[0][2] = 100;
        
        // Set decay interval to 1 for testing
        float originalInterval = wearRecoveryInterval;
        wearRecoveryInterval = 0.01f;
        
        int initialWear = GetGroundWear(2, 0);
        
        UpdateGroundWear();
        
        expect(GetGroundWear(2, 0) < initialWear);
        
        // Restore original values
        wearRecoveryInterval = originalInterval;
    }
    
    it("should convert dirt to grass when wear drops below threshold") {
        InitGridFromAsciiWithChunkSize(
            "dddd\n", 4, 1);
        
        for (int x = 0; x < gridWidth; x++) {
            grid[0][0][x] = CELL_DIRT;
        }
        
        InitGroundWear();
        groundWearEnabled = true;
        
        // Use easy-to-test values
        wearDirtToGrass = 50;
        wearDecayRate = 10;
        wearRecoveryInterval = 0.01f;
        
        // Set wear just above threshold
        wearGrid[0][2] = 55;
        
        expect(grid[0][0][2] == CELL_DIRT);
        
        // First decay: 55 - 10 = 45, below threshold
        UpdateGroundWear();
        
        expect(grid[0][0][2] == CELL_GRASS);
        
        // Restore default values
        wearDirtToGrass = WEAR_DIRT_TO_GRASS_DEFAULT;
        wearDecayRate = WEAR_DECAY_RATE_DEFAULT;
        wearRecoveryInterval = 0.01f;
    }
    
    it("should only decay every N ticks based on decay interval") {
        InitGridFromAsciiWithChunkSize(
            "dddd\n", 4, 1);
        
        for (int x = 0; x < gridWidth; x++) {
            grid[0][0][x] = CELL_DIRT;
        }
        
        InitGroundWear();
        groundWearEnabled = true;
        
        // Set decay interval to longer than one tick
        // With gameDeltaTime = TICK_DT (~0.0167s), interval of 0.1s takes ~6 ticks
        wearRecoveryInterval = 0.1f;
        wearDecayRate = 10;
        
        // Set initial wear
        wearGrid[0][2] = 100;
        
        // First few ticks: no decay (accumulator < interval)
        for (int i = 0; i < 3; i++) {
            UpdateGroundWear();
            expect(GetGroundWear(2, 0) == 100);
        }
        
        // After enough ticks, decay should happen
        for (int i = 0; i < 10; i++) {
            UpdateGroundWear();
        }
        expect(GetGroundWear(2, 0) < 100);
        
        // Restore default values
        wearRecoveryInterval = 0.01f;
        wearDecayRate = WEAR_DECAY_RATE_DEFAULT;
    }
    
    it("should clamp wear to 0 on decay") {
        InitGridFromAsciiWithChunkSize(
            "dddd\n", 4, 1);
        
        for (int x = 0; x < gridWidth; x++) {
            grid[0][0][x] = CELL_DIRT;
        }
        
        InitGroundWear();
        groundWearEnabled = true;
        
        wearDecayRate = 10;
        wearRecoveryInterval = 0.01f;
        
        // Set wear lower than decay rate
        wearGrid[0][2] = 5;
        
        UpdateGroundWear();
        
        expect(GetGroundWear(2, 0) == 0);
        
        // Restore default values
        wearDecayRate = WEAR_DECAY_RATE_DEFAULT;
        wearRecoveryInterval = 0.01f;
    }
}

// =============================================================================
// Full Cycle Tests
// =============================================================================

describe(groundwear_full_cycle) {
    it("should complete grass->dirt->grass cycle") {
        InitGridFromAsciiWithChunkSize(
            "gggg\n", 4, 1);
        
        for (int x = 0; x < gridWidth; x++) {
            grid[0][0][x] = CELL_GRASS;
        }
        
        InitGroundWear();
        groundWearEnabled = true;
        
        // Use test-friendly values
        wearGrassToDirt = 20;
        wearDirtToGrass = 10;
        wearTrampleAmount = 10;
        wearDecayRate = 5;
        wearRecoveryInterval = 0.01f;
        
        // Start as grass
        expect(grid[0][0][2] == CELL_GRASS);
        expect(GetGroundWear(2, 0) == 0);
        
        // Trample twice to become dirt (2*10=20 >= 20)
        TrampleGround(2, 0);
        TrampleGround(2, 0);
        expect(grid[0][0][2] == CELL_DIRT);
        expect(GetGroundWear(2, 0) == 20);
        
        // Let it decay back to grass
        // 20 -> 15 -> 10 -> 5 (at 10 becomes grass)
        UpdateGroundWear();  // 20 -> 15
        expect(grid[0][0][2] == CELL_DIRT);
        
        UpdateGroundWear();  // 15 -> 10, at threshold - becomes grass
        expect(grid[0][0][2] == CELL_GRASS);
        
        // Restore default values
        wearGrassToDirt = WEAR_GRASS_TO_DIRT_DEFAULT;
        wearDirtToGrass = WEAR_DIRT_TO_GRASS_DEFAULT;
        wearTrampleAmount = WEAR_TRAMPLE_AMOUNT_DEFAULT;
        wearDecayRate = WEAR_DECAY_RATE_DEFAULT;
        wearRecoveryInterval = 0.01f;
    }
    
    it("should create path on heavily trafficked area") {
        InitGridFromAsciiWithChunkSize(
            "gggggggggg\n", 10, 1);
        
        for (int x = 0; x < gridWidth; x++) {
            grid[0][0][x] = CELL_GRASS;
        }
        
        InitGroundWear();
        groundWearEnabled = true;
        
        // Use test-friendly values
        wearGrassToDirt = 50;
        wearTrampleAmount = 10;
        
        // Simulate heavy traffic on cells 3, 4, 5 (center path)
        for (int i = 0; i < 10; i++) {
            TrampleGround(3, 0);
            TrampleGround(4, 0);
            TrampleGround(5, 0);
        }
        
        // Center cells should be dirt
        expect(grid[0][0][3] == CELL_DIRT);
        expect(grid[0][0][4] == CELL_DIRT);
        expect(grid[0][0][5] == CELL_DIRT);
        
        // Edge cells should still be grass
        expect(grid[0][0][0] == CELL_GRASS);
        expect(grid[0][0][1] == CELL_GRASS);
        expect(grid[0][0][8] == CELL_GRASS);
        expect(grid[0][0][9] == CELL_GRASS);
        
        // Restore default values
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
            "gggg\n", 4, 1);
        
        InitGroundWear();
        
        expect(GetGroundWear(-1, 0) == 0);
        expect(GetGroundWear(100, 0) == 0);
        expect(GetGroundWear(0, -1) == 0);
        expect(GetGroundWear(0, 100) == 0);
    }
    
    it("should handle out-of-bounds trampling gracefully") {
        InitGridFromAsciiWithChunkSize(
            "gggg\n", 4, 1);
        
        InitGroundWear();
        groundWearEnabled = true;
        
        // Should not crash
        TrampleGround(-1, 0);
        TrampleGround(100, 0);
        TrampleGround(0, -1);
        TrampleGround(0, 100);
        
        expect(true);  // If we get here, no crash
    }
    
    it("should handle dirt cell trampling (keeps it dirt)") {
        InitGridFromAsciiWithChunkSize(
            "dddd\n", 4, 1);
        
        for (int x = 0; x < gridWidth; x++) {
            grid[0][0][x] = CELL_DIRT;
        }
        
        InitGroundWear();
        groundWearEnabled = true;
        
        // Trampling dirt should increase wear (keeps it from reverting)
        TrampleGround(2, 0);
        expect(GetGroundWear(2, 0) == wearTrampleAmount);
        expect(grid[0][0][2] == CELL_DIRT);
    }
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char* argv[]) {
    // Suppress logs by default, use -v for verbose
    bool verbose = false;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'v') verbose = true;
    }
    if (!verbose) {
        SetTraceLogLevel(LOG_NONE);
    }
    
    // Run tests
    test(groundwear_initialization);
    test(groundwear_trampling);
    test(grass_to_dirt_conversion);
    test(dirt_to_grass_conversion);
    test(groundwear_full_cycle);
    test(groundwear_edge_cases);
    
    return summary();
}
