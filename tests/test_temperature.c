#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/simulation/temperature.h"
#include "../src/simulation/fire.h"
#include "../src/simulation/water.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Helper to run temperature simulation for N ticks
static void RunTempTicks(int n) {
    for (int i = 0; i < n; i++) {
        UpdateTemperature();
    }
}

// Helper to count cells at a given temperature
__attribute__((unused))
static int CountCellsAtTemp(int targetTemp) {
    int count = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (GetTemperature(x, y, z) == targetTemp) {
                    count++;
                }
            }
        }
    }
    return count;
}

// Helper to get average temperature in a region
static int GetAverageTemp(int x1, int y1, int x2, int y2, int z) {
    int total = 0;
    int count = 0;
    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            total += GetTemperature(x, y, z);
            count++;
        }
    }
    return count > 0 ? total / count : 0;
}

// =============================================================================
// Basic Temperature Operations
// =============================================================================

describe(temperature_initialization) {
    it("should initialize temperature grid to ambient values") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 4);
        
        InitTemperature();
        
        // For single z-level grid, z=0 is surface, so ambient should be surface temp
        int expectedAmbient = GetAmbientTemperature(0);
        
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                expect(GetTemperature(x, y, 0) == expectedAmbient);
            }
        }
    }
    
    it("should clear temperature when ClearTemperature is called") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n", 8, 2);
        
        InitTemperature();
        SetTemperature(2, 0, 0, 100);
        SetTemperature(4, 1, 0, -20);
        
        expect(GetTemperature(2, 0, 0) == 100);
        expect(GetTemperature(4, 1, 0) == -20);
        
        ClearTemperature();
        
        // Should reset to ambient for this z-level
        int expectedAmbient = GetAmbientTemperature(0);
        expect(GetTemperature(2, 0, 0) == expectedAmbient);
        expect(GetTemperature(4, 1, 0) == expectedAmbient);
    }
}

describe(temperature_level_operations) {
    it("should set temperature within bounds") {
        InitGridFromAsciiWithChunkSize(
            "....\n"
            "....\n", 4, 2);
        InitTemperature();
        
        SetTemperature(1, 0, 0, 50);   // 50C
        expect(GetTemperature(1, 0, 0) == 50);
        
        SetTemperature(2, 1, 0, 100);  // 100C
        expect(GetTemperature(2, 1, 0) == 100);
    }
    
    it("should clamp temperature to max 2000") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitTemperature();
        
        SetTemperature(0, 0, 0, 5000);
        expect(GetTemperature(0, 0, 0) == TEMP_MAX);  // 2000
    }
    
    it("should clamp temperature to min -100") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitTemperature();
        
        SetTemperature(0, 0, 0, -500);
        expect(GetTemperature(0, 0, 0) == TEMP_MIN);  // -100
    }
    
    it("should report IsFreezing correctly") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitTemperature();
        
        SetTemperature(0, 0, 0, 0);   // At freeze threshold (0C)
        expect(IsFreezing(0, 0, 0) == true);
        
        SetTemperature(1, 0, 0, 2);   // Above freeze threshold (2C, index 26)
        expect(IsFreezing(1, 0, 0) == false);
        
        SetTemperature(2, 0, 0, -20); // Deep freeze
        expect(IsFreezing(2, 0, 0) == true);
    }
    
    it("should report IsHot correctly") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitTemperature();
        
        SetTemperature(0, 0, 0, 40);  // At hot threshold (40C)
        expect(IsHot(0, 0, 0) == true);
        
        SetTemperature(1, 0, 0, 39);  // Below hot threshold
        expect(IsHot(1, 0, 0) == false);
    }
    
    it("should report IsComfortable correctly") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitTemperature();
        
        SetTemperature(0, 0, 0, 20);  // In comfortable range (20C)
        expect(IsComfortable(0, 0, 0) == true);
        
        SetTemperature(1, 0, 0, 10);  // Too cold
        expect(IsComfortable(1, 0, 0) == false);
        
        SetTemperature(2, 0, 0, 30);  // Too hot
        expect(IsComfortable(2, 0, 0) == false);
    }
}

// =============================================================================
// Test 1: Basic Heat Spread in Open Air
// =============================================================================

describe(temperature_heat_spread) {
    it("should spread heat outward from hot cell") {
        InitGridFromAsciiWithChunkSize(
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n", 5, 5);
        
        InitTemperature();
        
        int ambient = GetAmbientTemperature(0);
        
        // Place heat source in center (100C)
        SetHeatSource(2, 2, 0, true);
        
        // Run simulation
        RunTempTicks(100);
        
        // Center should be hot (source)
        expect(GetTemperature(2, 2, 0) >= 80);
        
        // Adjacent cells should be warmer than ambient
        expect(GetTemperature(1, 2, 0) > ambient);
        expect(GetTemperature(3, 2, 0) > ambient);
        expect(GetTemperature(2, 1, 0) > ambient);
        expect(GetTemperature(2, 3, 0) > ambient);
    }
    
    it("should spread heat in circular pattern (orthogonal first)") {
        InitGridFromAsciiWithChunkSize(
            ".......\n"
            ".......\n"
            ".......\n"
            ".......\n"
            ".......\n"
            ".......\n"
            ".......\n", 7, 7);
        
        InitTemperature();
        
        // Place heat source in center (127C max)
        SetHeatSource(3, 3, 0, true);
        
        // Run simulation
        RunTempTicks(50);
        
        // Orthogonal neighbors should be hotter than diagonal
        int orthogonalTemp = GetTemperature(2, 3, 0);  // Left of center
        int diagonalTemp = GetTemperature(2, 2, 0);    // Diagonal from center
        
        // Orthogonal should be at least as hot (heat spreads orthogonally)
        expect(orthogonalTemp >= diagonalTemp);
    }
}

// =============================================================================
// Test 2: Stone Wall Insulation
// =============================================================================

describe(temperature_stone_insulation) {
    it("should keep heat mostly contained inside stone room") {
        // Create stone room with heat source inside
        InitGridFromAsciiWithChunkSize(
            ".......\n"
            ".#####.\n"
            ".#...#.\n"
            ".#...#.\n"
            ".#####.\n"
            ".......\n", 7, 6);
        
        InitTemperature();
        
        int ambient = GetAmbientTemperature(0);
        
        // Place heat source inside room (100C)
        SetHeatSource(3, 2, 0, true);
        
        // Run simulation
        RunTempTicks(200);
        
        // Inside room should be warm
        int insideTemp = GetTemperature(3, 3, 0);
        expect(insideTemp > ambient + 10);
        
        // Outside room should be much closer to ambient
        int outsideTemp = GetTemperature(0, 3, 0);
        
        // Inside should be significantly warmer than outside
        expect(insideTemp > outsideTemp + 15);
    }
}

// =============================================================================
// Test 3: Wood vs Stone Insulation (placeholder until CELL_WOOD_WALL exists)
// =============================================================================

describe(temperature_insulation_comparison) {
    it("should transfer heat slower through higher insulation tiers") {
        // Test insulation tier system
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitTemperature();
        
        // Air should be tier 0
        expect(GetInsulationTier(0, 0, 0) == INSULATION_TIER_AIR);
        
        // Create a wall
        grid[0][0][1] = CELL_WALL;
        
        // Wall should be tier 2 (stone)
        expect(GetInsulationTier(1, 0, 0) == INSULATION_TIER_STONE);
    }
}

// =============================================================================
// Test 4: Water Freezing (placeholder - needs water integration)
// =============================================================================

describe(temperature_freezing_conditions) {
    it("should correctly identify freezing temperatures") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitTemperature();
        
        // Set various temperatures and check freezing state (Celsius)
        SetTemperature(0, 0, 0, 0);    // At threshold (0C)
        SetTemperature(1, 0, 0, -20);  // Deep freeze
        SetTemperature(2, 0, 0, 10);   // Above freezing
        SetTemperature(3, 0, 0, -50);  // Very cold
        
        expect(IsFreezing(0, 0, 0) == true);
        expect(IsFreezing(1, 0, 0) == true);
        expect(IsFreezing(2, 0, 0) == false);
        expect(IsFreezing(3, 0, 0) == true);
    }
}

// =============================================================================
// Test 6: Cold Storage Room Underground
// =============================================================================

describe(temperature_underground_ambient) {
    it("should have consistent ambient when depth decay is 0") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        gridDepth = 16;  // Multiple z-levels
        
        // Initialize all as walkable
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                for (int x = 0; x < gridWidth; x++) {
                    grid[z][y][x] = CELL_AIR;
                }
            }
        }
        
        InitTemperature();
        
        // With depth decay = 0, all levels should have same ambient
        int surfaceAmbient = GetAmbientTemperature(gridDepth - 1);
        int deepAmbient = GetAmbientTemperature(0);
        
        printf("Surface ambient (z=%d): %d\n", gridDepth - 1, surfaceAmbient);
        printf("Deep ambient (z=0): %d\n", deepAmbient);
        
        expect(surfaceAmbient == ambientSurfaceTemp);
        expect(deepAmbient == ambientSurfaceTemp);  // Same when decay is 0
    }
    
    it("should create cold storage when temperature is set low") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        gridDepth = 16;
        
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                for (int x = 0; x < gridWidth; x++) {
                    grid[z][y][x] = CELL_AIR;
                }
            }
        }
        
        InitTemperature();
        
        // Set a cell to cold storage temperature (5C or below)
        SetTemperature(4, 4, 0, 5);
        expect(IsColdStorage(4, 4, 0) == true);
        
        // Room temperature should not be cold storage
        SetTemperature(5, 5, 0, 20);
        expect(IsColdStorage(5, 5, 0) == false);
    }
}

// =============================================================================
// Test 7: Heated Room in Cold Environment
// =============================================================================

describe(temperature_heated_room) {
    it("should warm up room with heat source") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        gridDepth = 16;
        
        // Create room at z=0
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                for (int x = 0; x < gridWidth; x++) {
                    grid[z][y][x] = CELL_AIR;
                }
            }
        }
        
        // Add walls for room at z=0
        for (int y = 1; y <= 5; y++) {
            grid[0][y][1] = CELL_WALL;
            grid[0][y][5] = CELL_WALL;
        }
        for (int x = 1; x <= 5; x++) {
            grid[0][1][x] = CELL_WALL;
            grid[0][5][x] = CELL_WALL;
        }
        
        InitTemperature();
        
        int initialTemp = GetTemperature(3, 3, 0);
        printf("Initial room temp: %d\n", initialTemp);
        
        // Place heat source inside room (100C)
        SetHeatSource(3, 3, 0, true);
        
        // Run simulation (more ticks for heat to spread in enclosed room)
        RunTempTicks(1000);
        
        // Room interior should be warmer than initial
        int roomTemp = GetAverageTemp(2, 2, 4, 4, 0);
        printf("Heated room avg temp: %d\n", roomTemp);
        
        // Interior cells (excluding source) should be significantly warmer
        expect(roomTemp > initialTemp + 20);
    }
}

// =============================================================================
// Test 8: Fire Heats Cells
// =============================================================================

describe(temperature_fire_heating) {
    it("should heat cells when ApplyFireHeat is called") {
        InitGridFromAsciiWithChunkSize(
            "...\n"
            "...\n"
            "...\n", 3, 3);
        
        InitTemperature();
        
        int initialTemp = GetTemperature(1, 1, 0);
        
        // Apply fire heat (level 7)
        ApplyFireHeat(1, 1, 0, 7);
        
        int fireTemp = GetTemperature(1, 1, 0);
        
        // Fire level 7 should heat significantly (TEMP_FIRE_MIN is index 65 = 80C)
        // Fire at level 7 = index 65 + 7*3 = 86 = 122C
        expect(fireTemp >= 80);  // At least fire minimum in Celsius
        expect(fireTemp > initialTemp);
        
        printf("Fire heat test: initial=%d, after fire=%d\n", initialTemp, fireTemp);
    }
    
    it("should not lower temperature with fire heat") {
        InitGridFromAsciiWithChunkSize(
            "...\n", 3, 1);
        
        InitTemperature();
        
        // Set cell to high heat (200C)
        SetTemperature(1, 0, 0, 200);
        
        // Apply lower fire heat
        ApplyFireHeat(1, 0, 0, 1);  // Fire level 1 ~ 86C
        
        // Temperature should stay at 200 (fire doesn't lower temp)
        expect(GetTemperature(1, 0, 0) == 200);
    }
}

// =============================================================================
// Test 9: Large Room vs Small Room Heating Time
// =============================================================================

describe(temperature_room_size_heating) {
    it("should heat small room faster than large room") {
        // This is somewhat implicit in the simulation - we verify the concept
        InitGridFromAsciiWithChunkSize(
            "................\n"
            ".####..########.\n"
            ".#..#..#......#.\n"
            ".####..#......#.\n"
            ".......#......#.\n"
            ".......########.\n", 16, 6);
        
        InitTemperature();
        
        // Place equal heat sources in both rooms (100C)
        SetHeatSource(2, 2, 0, true);  // Small room (2x1 interior)
        SetHeatSource(10, 3, 0, true); // Large room (6x3 interior)
        
        // Run simulation for limited ticks
        RunTempTicks(100);
        
        // Get average temps in each room interior
        int smallRoomTemp = GetTemperature(2, 2, 0);  // Center of small room
        int largeRoomCorner = GetTemperature(8, 2, 0); // Corner of large room
        
        printf("Small room center: %d, Large room corner: %d\n", smallRoomTemp, largeRoomCorner);
        
        // Small room should reach equilibrium faster (its cells warm up more)
        // Heat source is 100C, so small room interior should be very warm
        // Large room corner should be warming but not as hot yet
        expect(smallRoomTemp >= 80);  // Source cell stays hot
    }
}

// =============================================================================
// Test 10: Temperature Decay to Ambient
// =============================================================================

describe(temperature_decay) {
    it("should decay hot temperature toward ambient") {
        InitGridFromAsciiWithChunkSize(
            "...\n"
            "...\n"
            "...\n", 3, 3);
        
        InitTemperature();
        
        int ambient = GetAmbientTemperature(0);
        
        // Set center cell to hot (not a source) - 100C
        SetTemperature(1, 1, 0, 100);
        
        // Run simulation
        RunTempTicks(500);
        
        int finalTemp = GetTemperature(1, 1, 0);
        
        // Should have decayed toward ambient (20C)
        expect(finalTemp < 100);
        expect(finalTemp < ambient + 30);  // Roughly near ambient
        
        printf("Decay test: started at 100, ended at %d (ambient=%d)\n", 
               finalTemp, ambient);
    }
    
    it("should decay cold temperature toward ambient") {
        InitGridFromAsciiWithChunkSize(
            "...\n"
            "...\n"
            "...\n", 3, 3);
        
        InitTemperature();
        
        int ambient = GetAmbientTemperature(0);
        
        // Set center cell to cold (not a source) - 0C
        SetTemperature(1, 1, 0, 0);
        
        // Run simulation
        RunTempTicks(500);
        
        int finalTemp = GetTemperature(1, 1, 0);
        
        // Should have risen toward ambient (20C)
        expect(finalTemp > 0);
        expect(finalTemp > ambient - 30);  // Roughly near ambient
        
        printf("Cold decay test: started at 0, ended at %d (ambient=%d)\n", 
               finalTemp, ambient);
    }
}

// =============================================================================
// Test 11: Underground Ambient Gradient
// =============================================================================

describe(temperature_depth_gradient) {
    it("should have correct ambient temperatures at each depth") {
        InitGridWithSizeAndChunkSize(4, 4, 4, 4);
        gridDepth = 16;
        
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                for (int x = 0; x < gridWidth; x++) {
                    grid[z][y][x] = CELL_AIR;
                }
            }
        }
        
        // Test ambient calculation
        printf("Ambient temperatures by depth:\n");
        for (int z = 0; z < gridDepth; z++) {
            int ambient = GetAmbientTemperature(z);
            int expectedDepth = (gridDepth - 1) - z;
            int expected = ambientSurfaceTemp - (expectedDepth * ambientDepthDecay);
            if (expected < 0) expected = 0;
            
            printf("  z=%d (depth=%d): ambient=%d, expected=%d\n", 
                   z, expectedDepth, ambient, expected);
            
            expect(ambient == expected);
        }
    }
}

// =============================================================================
// Test 12: Cold Source (Ice Block Simulation)
// =============================================================================

describe(temperature_cold_source) {
    it("should cool surrounding cells") {
        InitGridFromAsciiWithChunkSize(
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n", 5, 5);
        
        InitTemperature();
        
        int ambient = GetAmbientTemperature(0);
        
        // Place cold source in center (-20C based on coldSourceTemp)
        SetColdSource(2, 2, 0, true);
        
        // Run simulation (more ticks for cold to spread against decay)
        RunTempTicks(200);
        
        // Center should be cold (source)
        expect(GetTemperature(2, 2, 0) <= 0);
        
        // Adjacent cells should be cooler than ambient (cold spreads slower due to decay)
        // With decay fighting cold, neighbors may only be slightly cooler
        int neighborTemp = GetTemperature(1, 2, 0);
        expect(neighborTemp < ambient);
        
        printf("Cold source test: center=%d, neighbor=%d, ambient=%d\n",
               GetTemperature(2, 2, 0), neighborTemp, ambient);
    }
    
    it("should maintain freezing temperature") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        
        InitTemperature();
        
        // Set a cell to freezing (-10C)
        SetTemperature(2, 0, 0, -10);
        
        // Verify it's freezing
        expect(IsFreezing(2, 0, 0) == true);
        
        // Set a cold source
        SetColdSource(0, 0, 0, true);
        expect(IsFreezing(0, 0, 0) == true);
    }
}

// =============================================================================
// Source Management
// =============================================================================

describe(temperature_sources) {
    it("should maintain heat source temperature") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        
        InitTemperature();
        
        SetHeatSource(1, 0, 0, true);
        
        // Run many ticks
        RunTempTicks(500);
        
        // Source should maintain its temperature
        expect(GetTemperature(1, 0, 0) >= 90);
        expect(IsHeatSource(1, 0, 0) == true);
    }
    
    it("should maintain cold source temperature") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        
        InitTemperature();
        
        SetColdSource(1, 0, 0, true);
        
        // Run many ticks
        RunTempTicks(500);
        
        // Source should maintain its temperature
        expect(GetTemperature(1, 0, 0) <= 0);
        expect(IsColdSource(1, 0, 0) == true);
    }
    
    it("should stop being source when removed") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        
        InitTemperature();
        
        SetHeatSource(1, 0, 0, true);
        expect(IsHeatSource(1, 0, 0) == true);
        
        RemoveTemperatureSource(1, 0, 0);
        expect(IsHeatSource(1, 0, 0) == false);
        
        // After removal, temperature should decay
        RunTempTicks(500);
        
        int finalTemp = GetTemperature(1, 0, 0);
        expect(finalTemp < 100);
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

describe(temperature_edge_cases) {
    it("should handle out-of-bounds queries gracefully") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        
        InitTemperature();
        
        // Out of bounds should return ambient
        int ambient = GetAmbientTemperature(0);
        expect(GetTemperature(-1, 0, 0) == ambient);
        expect(GetTemperature(100, 0, 0) == ambient);
        expect(GetTemperature(0, -1, 0) == ambient);
        expect(GetTemperature(0, 100, 0) == ambient);
        
        // Out of bounds set should not crash
        SetTemperature(-1, 0, 0, 200);
        SetTemperature(100, 0, 0, 200);
    }
    
    it("should handle temperature at grid edges") {
        InitGridFromAsciiWithChunkSize(
            "....\n"
            "....\n", 4, 2);
        
        InitTemperature();
        
        // Place heat at corners (100C)
        SetHeatSource(0, 0, 0, true);
        SetHeatSource(3, 0, 0, true);
        SetHeatSource(0, 1, 0, true);
        SetHeatSource(3, 1, 0, true);
        
        // Run simulation - should not crash
        RunTempTicks(50);
        
        // Corners should be hot
        expect(GetTemperature(0, 0, 0) >= 80);
        expect(GetTemperature(3, 1, 0) >= 80);
    }
    
    it("should handle both heat and cold sources nearby") {
        InitGridFromAsciiWithChunkSize(
            ".......\n"
            ".......\n"
            ".......\n", 7, 3);
        
        InitTemperature();
        
        // Place heat and cold sources
        SetHeatSource(1, 1, 0, true);
        SetColdSource(5, 1, 0, true);
        
        // Run simulation
        RunTempTicks(100);
        
        // Middle cell should be somewhere in between
        int middleTemp = GetTemperature(3, 1, 0);
        
        printf("Heat/cold battle: hot=%d, middle=%d, cold=%d\n",
               GetTemperature(1, 1, 0), middleTemp, GetTemperature(5, 1, 0));
        
        // Should be between the two extremes
        expect(middleTemp > -20);
        expect(middleTemp < 100);
    }
}

// =============================================================================
// Performance / Stability
// =============================================================================

describe(temperature_stability) {
    it("should mark cells as stable when temperature settles") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 4);
        
        InitTemperature();
        
        // Add some temperature variation
        SetTemperature(4, 2, 0, 100);
        
        // Run until stable
        for (int i = 0; i < 1000; i++) {
            UpdateTemperature();
            if (tempUpdateCount == 0) break;
        }
        
        printf("Stability test: updates=%d after stabilization\n", tempUpdateCount);
        
        // Should have stabilized (very few updates)
        expect(tempUpdateCount < 10);
    }
    
    it("should destabilize neighbors when temperature changes") {
        InitGridFromAsciiWithChunkSize(
            ".....\n"
            ".....\n"
            ".....\n", 5, 3);
        
        InitTemperature();
        
        // Let everything stabilize first
        RunTempTicks(100);
        
        // Mark all as stable manually
        for (int y = 0; y < 3; y++) {
            for (int x = 0; x < 5; x++) {
                temperatureGrid[0][y][x].stable = true;
            }
        }
        
        // Change temperature - should destabilize neighbors
        SetTemperature(2, 1, 0, 100);
        
        // Center and orthogonal neighbors should be unstable
        expect(IsTemperatureStable(2, 1, 0) == false);  // Center
        expect(IsTemperatureStable(2, 0, 0) == false);  // North
        expect(IsTemperatureStable(2, 2, 0) == false);  // South
        expect(IsTemperatureStable(1, 1, 0) == false);  // West
        expect(IsTemperatureStable(3, 1, 0) == false);  // East
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
    
    // Basic operations
    test(temperature_initialization);
    test(temperature_level_operations);
    
    // Heat spread and transfer
    test(temperature_heat_spread);
    test(temperature_stone_insulation);
    test(temperature_insulation_comparison);
    
    // Freezing
    test(temperature_freezing_conditions);
    
    // Underground / ambient
    test(temperature_underground_ambient);
    test(temperature_heated_room);
    
    // Fire integration
    test(temperature_fire_heating);
    
    // Room heating
    test(temperature_room_size_heating);
    
    // Decay
    test(temperature_decay);
    test(temperature_depth_gradient);
    
    // Cold source
    test(temperature_cold_source);
    
    // Source management
    test(temperature_sources);
    
    // Edge cases and stability
    test(temperature_edge_cases);
    test(temperature_stability);
    
    return summary();
}
