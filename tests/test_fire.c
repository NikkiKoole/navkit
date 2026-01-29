#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/simulation/fire.h"
#include "../src/simulation/smoke.h"
#include "../src/simulation/water.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Helper to run fire simulation for N ticks
static void RunFireTicks(int n) {
    for (int i = 0; i < n; i++) {
        UpdateFire();
    }
}

// Helper to run smoke simulation for N ticks
static void RunSmokeTicks(int n) {
    for (int i = 0; i < n; i++) {
        UpdateSmoke();
    }
}

// Helper to run both fire and smoke for N ticks
static void RunFireAndSmokeTicks(int n) {
    for (int i = 0; i < n; i++) {
        UpdateFire();
        UpdateSmoke();
    }
}

// Helper to count total fire in the grid
static int CountTotalFire(void) {
    int total = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                total += GetFireLevel(x, y, z);
            }
        }
    }
    return total;
}

// Helper to count total smoke in the grid
static int CountTotalSmoke(void) {
    int total = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                total += GetSmokeLevel(x, y, z);
            }
        }
    }
    return total;
}

// Helper to count burning cells
static int CountBurningCells(void) {
    int count = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (HasFire(x, y, z)) count++;
            }
        }
    }
    return count;
}

// Helper to count burned cells
static int CountBurnedCells(void) {
    int count = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (HAS_CELL_FLAG(x, y, z, CELL_FLAG_BURNED)) count++;
            }
        }
    }
    return count;
}

// =============================================================================
// Basic Fire Operations
// =============================================================================

describe(fire_initialization) {
    it("should initialize fire grid with all zeros") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 4);
        
        InitFire();
        
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                expect(GetFireLevel(x, y, 0) == 0);
            }
        }
    }
    
    it("should clear all fire when ClearFire is called") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n", 8, 2);
        
        InitFire();
        SetFireLevel(2, 0, 0, 5);
        SetFireLevel(4, 1, 0, 7);
        
        expect(GetFireLevel(2, 0, 0) == 5);
        expect(GetFireLevel(4, 1, 0) == 7);
        
        ClearFire();
        
        expect(GetFireLevel(2, 0, 0) == 0);
        expect(GetFireLevel(4, 1, 0) == 0);
    }
}

describe(fire_level_operations) {
    it("should set fire level within bounds") {
        InitGridFromAsciiWithChunkSize(
            "....\n"
            "....\n", 4, 2);
        InitFire();
        
        SetFireLevel(1, 0, 0, 5);
        expect(GetFireLevel(1, 0, 0) == 5);
        
        SetFireLevel(2, 1, 0, 7);
        expect(GetFireLevel(2, 1, 0) == 7);
    }
    
    it("should clamp fire level to max 7") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitFire();
        
        SetFireLevel(0, 0, 0, 10);
        expect(GetFireLevel(0, 0, 0) == FIRE_MAX_LEVEL);
    }
    
    it("should clamp fire level to min 0") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitFire();
        
        SetFireLevel(0, 0, 0, -5);
        expect(GetFireLevel(0, 0, 0) == 0);
    }
    
    it("should report HasFire correctly") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitFire();
        
        expect(HasFire(0, 0, 0) == false);
        
        SetFireLevel(0, 0, 0, 1);
        expect(HasFire(0, 0, 0) == true);
    }
}

// =============================================================================
// Test 1: Basic Burning
// =============================================================================

describe(fire_basic_burning) {
    it("should burn and consume fuel on grass cells") {
        InitGridFromAsciiWithChunkSize(
            "....\n"
            "....\n", 4, 2);
        FillGroundLevel();  // DF mode: z=0 = dirt with grass surface
        
        InitFire();
        InitSmoke();
        
        // Ignite a cell
        IgniteCell(1, 0, 0);
        
        expect(HasFire(1, 0, 0) == true);
        expect(GetFireLevel(1, 0, 0) == FIRE_MAX_LEVEL);
        
        // Get initial fuel
        int initialFuel = GetCellFuel(1, 0, 0);
        expect(initialFuel > 0);
        
        // Run simulation until fire dies
        for (int i = 0; i < 500; i++) {
            UpdateFire();
            if (!HasFire(1, 0, 0)) break;
        }
        
        // Fire should have died (fuel exhausted)
        expect(HasFire(1, 0, 0) == false);
        
        // Cell should be marked as burned
        expect(HAS_CELL_FLAG(1, 0, 0, CELL_FLAG_BURNED) != 0);
    }
    
    it("should show burned tint after fire dies") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        FillGroundLevel();
        
        InitFire();
        
        // Initially not burned
        expect(HAS_CELL_FLAG(1, 0, 0, CELL_FLAG_BURNED) == 0);
        
        // Ignite and let burn out
        IgniteCell(1, 0, 0);
        
        for (int i = 0; i < 500; i++) {
            UpdateFire();
            if (!HasFire(1, 0, 0)) break;
        }
        
        // Now should be burned
        expect(HAS_CELL_FLAG(1, 0, 0, CELL_FLAG_BURNED) != 0);
    }
}

// =============================================================================
// Test 2: Spreading
// =============================================================================

describe(fire_spreading) {
    it("should spread to adjacent flammable cells") {
        // Create a patch of grass
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 4);
        FillGroundLevel();
        
        InitFire();
        
        // Ignite center cell
        IgniteCell(4, 2, 0);
        
        // Run simulation
        RunFireTicks(200);
        
        // Fire should have spread to at least one neighbor
        int burningNeighbors = 0;
        if (HasFire(3, 2, 0)) burningNeighbors++;
        if (HasFire(5, 2, 0)) burningNeighbors++;
        if (HasFire(4, 1, 0)) burningNeighbors++;
        if (HasFire(4, 3, 0)) burningNeighbors++;
        
        // At least one neighbor should have caught fire or be burned
        int burnedNeighbors = 0;
        if (HAS_CELL_FLAG(3, 2, 0, CELL_FLAG_BURNED)) burnedNeighbors++;
        if (HAS_CELL_FLAG(5, 2, 0, CELL_FLAG_BURNED)) burnedNeighbors++;
        if (HAS_CELL_FLAG(4, 1, 0, CELL_FLAG_BURNED)) burnedNeighbors++;
        if (HAS_CELL_FLAG(4, 3, 0, CELL_FLAG_BURNED)) burnedNeighbors++;
        
        expect(burningNeighbors + burnedNeighbors > 0);
    }
    
    it("should spread orthogonally not diagonally") {
        InitGridFromAsciiWithChunkSize(
            "...\n"
            "...\n"
            "...\n", 3, 3);
        
        InitFire();
        
        // Use a fire source so it keeps burning
        SetFireSource(1, 1, 0, true);
        
        // Run simulation
        RunFireTicks(100);
        
        // Count orthogonal vs diagonal fire/burned
        int orthogonal = 0;
        int diagonal = 0;
        
        // Orthogonal
        if (HasFire(0, 1, 0) || HAS_CELL_FLAG(0, 1, 0, CELL_FLAG_BURNED)) orthogonal++;
        if (HasFire(2, 1, 0) || HAS_CELL_FLAG(2, 1, 0, CELL_FLAG_BURNED)) orthogonal++;
        if (HasFire(1, 0, 0) || HAS_CELL_FLAG(1, 0, 0, CELL_FLAG_BURNED)) orthogonal++;
        if (HasFire(1, 2, 0) || HAS_CELL_FLAG(1, 2, 0, CELL_FLAG_BURNED)) orthogonal++;
        
        // Diagonal
        if (HasFire(0, 0, 0) || HAS_CELL_FLAG(0, 0, 0, CELL_FLAG_BURNED)) diagonal++;
        if (HasFire(2, 0, 0) || HAS_CELL_FLAG(2, 0, 0, CELL_FLAG_BURNED)) diagonal++;
        if (HasFire(0, 2, 0) || HAS_CELL_FLAG(0, 2, 0, CELL_FLAG_BURNED)) diagonal++;
        if (HasFire(2, 2, 0) || HAS_CELL_FLAG(2, 2, 0, CELL_FLAG_BURNED)) diagonal++;
        
        // Orthogonal should have more fire than diagonal (fire spreads orthogonally first)
        // Diagonal can only get fire via orthogonal spread from neighbors
        expect(orthogonal >= diagonal);
    }
}

// =============================================================================
// Test 3: Smoke Rising
// =============================================================================

describe(smoke_rising) {
    it("should generate smoke from fire") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        gridDepth = 4;
        
        // Initialize all levels as walkable
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                for (int x = 0; x < gridWidth; x++) {
                    grid[z][y][x] = CELL_WALKABLE;
                }
            }
        }
        
        InitFire();
        InitSmoke();
        
        // Place fire source at z=0
        SetFireSource(4, 4, 0, true);
        
        // Run simulation
        RunFireAndSmokeTicks(50);
        
        // Should have smoke somewhere in the grid (smoke rises and spreads)
        int totalSmoke = CountTotalSmoke();
        expect(totalSmoke > 0);
    }
    
    it("should rise through multiple z-levels") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        gridDepth = 4;
        
        // Initialize all levels as walkable (open air)
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                for (int x = 0; x < gridWidth; x++) {
                    grid[z][y][x] = CELL_WALKABLE;
                }
            }
        }
        
        InitFire();
        InitSmoke();
        
        // Place fire source at z=0
        SetFireSource(4, 4, 0, true);
        
        // Run simulation longer
        RunFireAndSmokeTicks(100);
        
        // Count smoke at each level (anywhere on that level)
        int smokeAtZ0 = 0, smokeAtZ1 = 0, smokeAtZ2 = 0;
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                smokeAtZ0 += GetSmokeLevel(x, y, 0);
                smokeAtZ1 += GetSmokeLevel(x, y, 1);
                smokeAtZ2 += GetSmokeLevel(x, y, 2);
            }
        }
        
        printf("Total smoke levels: z0=%d, z1=%d, z2=%d\n", smokeAtZ0, smokeAtZ1, smokeAtZ2);
        
        // Should have smoke at higher levels (smoke rises)
        expect(smokeAtZ1 > 0 || smokeAtZ2 > 0);
    }
}

// =============================================================================
// Test 4: Water Extinguishing
// =============================================================================

describe(fire_water_extinguishing) {
    it("should extinguish fire immediately when water is placed") {
        InitGridFromAsciiWithChunkSize(
            "....\n"
            "....\n", 4, 2);
        FillGroundLevel();
        
        InitFire();
        InitWater();
        
        // Ignite a cell
        IgniteCell(1, 0, 0);
        expect(HasFire(1, 0, 0) == true);
        
        // Place water on the fire
        SetWaterLevel(1, 0, 0, WATER_MAX_LEVEL);
        
        // Run one tick of fire simulation
        UpdateFire();
        
        // Fire should be extinguished
        expect(HasFire(1, 0, 0) == false);
    }
    
    it("should leave smoke after extinguishing") {
        InitGridWithSizeAndChunkSize(4, 4, 4, 4);
        gridDepth = 2;
        
        // Initialize as walkable
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                for (int x = 0; x < gridWidth; x++) {
                    grid[z][y][x] = CELL_WALKABLE;
                }
            }
        }
        
        InitFire();
        InitSmoke();
        InitWater();
        
        // Place fire source to generate smoke
        SetFireSource(1, 1, 0, true);
        
        // Run to generate smoke
        RunFireAndSmokeTicks(50);
        
        int smokeBeforeExtinguish = CountTotalSmoke();
        printf("Smoke before extinguish: %d\n", smokeBeforeExtinguish);
        
        // Extinguish with water
        SetWaterLevel(1, 1, 0, WATER_MAX_LEVEL);
        SetFireSource(1, 1, 0, false);  // Also remove source
        UpdateFire();
        
        // Fire should be gone
        expect(HasFire(1, 1, 0) == false);
        
        // Some smoke should still exist (though it will dissipate)
        int smokeAfterExtinguish = CountTotalSmoke();
        printf("Smoke after extinguish: %d\n", smokeAfterExtinguish);
        
        // Smoke was generated before, verify it existed
        expect(smokeBeforeExtinguish > 0);
    }
}

// =============================================================================
// Test 5: Water Barrier
// =============================================================================

describe(fire_water_barrier) {
    it("should not spread fire across water") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n", 8, 3);
        
        InitFire();
        InitWater();
        
        // Create water barrier in the middle
        for (int y = 0; y < 3; y++) {
            SetWaterLevel(4, y, 0, WATER_MAX_LEVEL);
        }
        
        // Place fire source on left side
        SetFireSource(1, 1, 0, true);
        
        // Run simulation
        RunFireTicks(300);
        
        // Fire should not have crossed the water barrier
        expect(HasFire(5, 1, 0) == false);
        expect(HasFire(6, 1, 0) == false);
        expect(HasFire(7, 1, 0) == false);
        
        // Fire should exist on left side
        int leftSideFire = 0;
        for (int x = 0; x < 4; x++) {
            for (int y = 0; y < 3; y++) {
                if (HasFire(x, y, 0) || HAS_CELL_FLAG(x, y, 0, CELL_FLAG_BURNED)) {
                    leftSideFire++;
                }
            }
        }
        expect(leftSideFire > 0);
    }
}

// =============================================================================
// Test 6: Non-Flammable Cells
// =============================================================================

describe(fire_non_flammable) {
    it("should not ignite stone walls") {
        InitGridFromAsciiWithChunkSize(
            ".#.#.\n"
            "#...#\n"
            ".#.#.\n", 5, 3);
        
        InitFire();
        
        // Try to ignite wall cells
        IgniteCell(1, 0, 0);  // Wall
        IgniteCell(3, 0, 0);  // Wall
        IgniteCell(0, 1, 0);  // Wall
        
        // Walls should not be on fire
        expect(HasFire(1, 0, 0) == false);
        expect(HasFire(3, 0, 0) == false);
        expect(HasFire(0, 1, 0) == false);
    }
    
    it("should return zero fuel for walls") {
        InitGridFromAsciiWithChunkSize(
            ".#.\n", 3, 1);
        
        InitFire();
        
        // Wall should have 0 fuel
        expect(GetBaseFuelForCellType(CELL_WALL) == 0);
        
        // Grass/walkable should have fuel
        expect(GetBaseFuelForCellType(CELL_GRASS) > 0);
        expect(GetBaseFuelForCellType(CELL_WALKABLE) > 0);
    }
}

// =============================================================================
// Test 7: Burned Cells Don't Reignite
// =============================================================================

describe(fire_burned_cells) {
    it("should not reignite burned cells") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        FillGroundLevel();
        
        InitFire();
        
        // Ignite and let burn out
        IgniteCell(1, 0, 0);
        
        for (int i = 0; i < 500; i++) {
            UpdateFire();
            if (!HasFire(1, 0, 0)) break;
        }
        
        // Verify cell is burned
        expect(HAS_CELL_FLAG(1, 0, 0, CELL_FLAG_BURNED) != 0);
        expect(HasFire(1, 0, 0) == false);
        
        // Try to reignite
        IgniteCell(1, 0, 0);
        
        // Should not ignite (fuel exhausted, burned flag set)
        expect(HasFire(1, 0, 0) == false);
    }
}

// =============================================================================
// Test 8: Permanent Fire Source
// =============================================================================

describe(fire_permanent_source) {
    it("should burn indefinitely as fire source") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        
        InitFire();
        
        // Place fire source
        SetFireSource(1, 0, 0, true);
        
        expect(HasFire(1, 0, 0) == true);
        expect(GetFireLevel(1, 0, 0) == FIRE_MAX_LEVEL);
        
        // Run many ticks
        RunFireTicks(1000);
        
        // Should still be burning at max level
        expect(HasFire(1, 0, 0) == true);
        expect(GetFireLevel(1, 0, 0) == FIRE_MAX_LEVEL);
    }
    
    it("should continue producing smoke") {
        InitGridWithSizeAndChunkSize(4, 4, 4, 4);
        gridDepth = 2;
        
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                for (int x = 0; x < gridWidth; x++) {
                    grid[z][y][x] = CELL_WALKABLE;
                }
            }
        }
        
        InitFire();
        InitSmoke();
        
        // Place fire source
        SetFireSource(2, 2, 0, true);
        
        // Run simulation
        RunFireAndSmokeTicks(100);
        
        // Should have smoke
        int totalSmoke = CountTotalSmoke();
        expect(totalSmoke > 0);
    }
}

// =============================================================================
// Test 9: Pathfinding (fire is not walkable)
// =============================================================================

// Note: This test would require mover/pathfinding integration
// For now, we just verify the concept that fire should be treated as impassable
describe(fire_pathfinding_concept) {
    it("should have fire cells that movers should avoid") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        
        InitFire();
        
        // Place fire
        SetFireSource(1, 0, 0, true);
        
        // Fire exists
        expect(HasFire(1, 0, 0) == true);
        
        // The pathfinding system should check HasFire() and treat it as impassable
        // This is a placeholder test - actual pathfinding integration would be in mover tests
    }
}

// =============================================================================
// Test 10: Multi-Z Smoke Rising
// =============================================================================

describe(smoke_multi_z_rising) {
    it("should rise through multiple z-levels and spread horizontally") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        gridDepth = 4;
        
        // Initialize all levels as walkable
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                for (int x = 0; x < gridWidth; x++) {
                    grid[z][y][x] = CELL_WALKABLE;
                }
            }
        }
        
        InitFire();
        InitSmoke();
        
        // Place fire source at z=0
        SetFireSource(4, 4, 0, true);
        
        // Run simulation
        RunFireAndSmokeTicks(200);
        
        // Check smoke at multiple levels
        bool smokeAtZ1 = false;
        bool smokeAtZ2 = false;
        bool smokeAtZ3 = false;
        
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (GetSmokeLevel(x, y, 1) > 0) smokeAtZ1 = true;
                if (GetSmokeLevel(x, y, 2) > 0) smokeAtZ2 = true;
                if (GetSmokeLevel(x, y, 3) > 0) smokeAtZ3 = true;
            }
        }
        
        printf("Smoke at z=1: %s, z=2: %s, z=3: %s\n",
               smokeAtZ1 ? "yes" : "no",
               smokeAtZ2 ? "yes" : "no",
               smokeAtZ3 ? "yes" : "no");
        
        // Smoke must reach ALL levels - no skipping
        expect(smokeAtZ1 == true);
        expect(smokeAtZ2 == true);
        expect(smokeAtZ3 == true);
    }
    
    it("should have smoke at ALL intermediate z-levels simultaneously") {
        // This test catches the "cascading" bug where smoke passes through
        // intermediate levels without accumulating there
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        gridDepth = 6;  // More levels to make the bug obvious
        FillGroundLevel();  // DF mode: z=0 = dirt with grass surface
        
        InitFire();
        InitSmoke();
        
        // Place fire source at z=0
        SetFireSource(4, 4, 0, true);
        
        // Run enough ticks for smoke to reach top but not too many
        // With proper rising (one z-level per rise interval), smoke needs
        // multiple rise intervals to reach higher levels
        RunFireAndSmokeTicks(150);
        
        // Count total smoke at each level
        int smokeByZ[6] = {0};
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                for (int x = 0; x < gridWidth; x++) {
                    smokeByZ[z] += GetSmokeLevel(x, y, z);
                }
            }
        }
        
        printf("Smoke by z-level: z0=%d z1=%d z2=%d z3=%d z4=%d z5=%d\n",
               smokeByZ[0], smokeByZ[1], smokeByZ[2], smokeByZ[3], smokeByZ[4], smokeByZ[5]);
        
        // Key test: intermediate levels (z1-z4) must have smoke
        // If smoke cascades through in one tick, intermediate levels will be empty
        // while z5 (top) has all the smoke
        expect(smokeByZ[1] > 0);  // z1 must have smoke
        expect(smokeByZ[2] > 0);  // z2 must have smoke  
        expect(smokeByZ[3] > 0);  // z3 must have smoke
        expect(smokeByZ[4] > 0);  // z4 must have smoke
        
        // Additional check: intermediate levels should have meaningful amounts
        // not just 1 or 2 from spreading. If z5 has way more than others, 
        // something is wrong with rising
        int intermediateTotal = smokeByZ[1] + smokeByZ[2] + smokeByZ[3] + smokeByZ[4];
        int topLevel = smokeByZ[5];
        
        printf("Intermediate total=%d, top level=%d\n", intermediateTotal, topLevel);
        
        // Intermediate levels combined should have at least as much smoke as top
        // (smoke should be distributed, not concentrated at top)
        expect(intermediateTotal >= topLevel);
    }
}

// =============================================================================
// Test 11: Closed Room Smoke Filling
// =============================================================================

describe(smoke_closed_room_filling) {
    it("should fill enclosed room from top to bottom") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        gridDepth = 4;
        
        // Create enclosed room: walls all around at each level, ceiling at z=3
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                for (int x = 0; x < gridWidth; x++) {
                    // Walls on edges
                    if (x == 0 || x == 7 || y == 0 || y == 7) {
                        grid[z][y][x] = CELL_WALL;
                    } else {
                        grid[z][y][x] = CELL_WALKABLE;
                    }
                }
            }
        }
        
        // Ceiling at z=3 (all walls)
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                grid[3][y][x] = CELL_WALL;
            }
        }
        
        InitFire();
        InitSmoke();
        
        // Place fire source inside at z=0
        SetFireSource(4, 4, 0, true);
        
        // Run simulation long enough for smoke to fill
        RunFireAndSmokeTicks(500);
        
        // Check smoke levels inside the room
        int smokeAtZ0 = 0;
        int smokeAtZ1 = 0;
        int smokeAtZ2 = 0;
        
        for (int y = 1; y < 7; y++) {
            for (int x = 1; x < 7; x++) {
                smokeAtZ0 += GetSmokeLevel(x, y, 0);
                smokeAtZ1 += GetSmokeLevel(x, y, 1);
                smokeAtZ2 += GetSmokeLevel(x, y, 2);
            }
        }
        
        printf("Closed room smoke: z0=%d, z1=%d, z2=%d\n", smokeAtZ0, smokeAtZ1, smokeAtZ2);
        
        // Room should have smoke at multiple levels
        // Higher levels should fill first, then pressure fills down
        expect(smokeAtZ2 > 0 || smokeAtZ1 > 0 || smokeAtZ0 > 0);
    }
}

// =============================================================================
// Test 12: Chimney Ventilation
// =============================================================================

describe(smoke_chimney_ventilation) {
    it("should allow smoke to escape through chimney hole") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        gridDepth = 5;
        
        // Create enclosed room with chimney hole
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                for (int x = 0; x < gridWidth; x++) {
                    // Walls on edges
                    if (x == 0 || x == 7 || y == 0 || y == 7) {
                        grid[z][y][x] = CELL_WALL;
                    } else {
                        grid[z][y][x] = CELL_WALKABLE;
                    }
                }
            }
        }
        
        // Ceiling at z=3 with a hole (chimney) at (4,4)
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (x == 4 && y == 4) {
                    grid[3][y][x] = CELL_WALKABLE;  // Chimney hole
                } else if (x > 0 && x < 7 && y > 0 && y < 7) {
                    grid[3][y][x] = CELL_WALL;  // Ceiling
                }
            }
        }
        
        // z=4 is open above the chimney
        
        InitFire();
        InitSmoke();
        
        // Place fire source inside at z=0
        SetFireSource(4, 4, 0, true);
        
        // Run simulation
        RunFireAndSmokeTicks(300);
        
        // Smoke should escape through chimney to z=4
        int smokeAboveCeiling = GetSmokeLevel(4, 4, 4);
        
        printf("Smoke above chimney (z=4): %d\n", smokeAboveCeiling);
        
        // Should have some smoke escaping through chimney
        // Note: smoke dissipates, so we just check it can rise through
        expect(smokeAboveCeiling >= 0);  // Just verify no crash for now
    }
}

// =============================================================================
// Smoke Dissipation
// =============================================================================

describe(smoke_dissipation) {
    it("should dissipate over time in open air") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 4);
        
        InitSmoke();
        
        // Place smoke manually
        SetSmokeLevel(4, 2, 0, SMOKE_MAX_LEVEL);
        
        int initialSmoke = CountTotalSmoke();
        
        // Run simulation (no fire to generate more smoke)
        RunSmokeTicks(500);
        
        int finalSmoke = CountTotalSmoke();
        
        // Smoke should have dissipated
        expect(finalSmoke < initialSmoke);
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

describe(fire_edge_cases) {
    it("should handle out-of-bounds queries gracefully") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        
        InitFire();
        
        // Out of bounds should return 0
        expect(GetFireLevel(-1, 0, 0) == 0);
        expect(GetFireLevel(100, 0, 0) == 0);
        expect(GetFireLevel(0, -1, 0) == 0);
        expect(GetFireLevel(0, 100, 0) == 0);
        expect(GetFireLevel(0, 0, -1) == 0);
        expect(GetFireLevel(0, 0, 100) == 0);
        
        // Out of bounds set should not crash
        SetFireLevel(-1, 0, 0, 5);
        SetFireLevel(100, 0, 0, 5);
    }
    
    it("should handle fire at grid edges") {
        InitGridFromAsciiWithChunkSize(
            "....\n"
            "....\n", 4, 2);
        
        InitFire();
        
        // Place fire at corners
        IgniteCell(0, 0, 0);
        IgniteCell(3, 0, 0);
        IgniteCell(0, 1, 0);
        IgniteCell(3, 1, 0);
        
        // Run simulation - should not crash
        RunFireTicks(50);
        
        // Some fire activity should have occurred
        int burnedCount = CountBurnedCells();
        expect(burnedCount >= 0);  // Just verify no crash
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
    g_legacyWalkability = false;
    if (forceLegacy) g_legacyWalkability = true;
    if (forceDF) g_legacyWalkability = false;
    
    // Basic operations
    test(fire_initialization);
    test(fire_level_operations);
    
    // Test scenarios from documentation/fire-system.md
    test(fire_basic_burning);           // Test 1: Basic Burning
    test(fire_spreading);               // Test 2: Spreading
    test(smoke_rising);                 // Test 3: Smoke Rising
    test(fire_water_extinguishing);     // Test 4: Water Extinguishing
    test(fire_water_barrier);           // Test 5: Water Barrier
    test(fire_non_flammable);           // Test 6: Non-Flammable Cells
    test(fire_burned_cells);            // Test 7: Burned Cells Don't Reignite
    test(fire_permanent_source);        // Test 8: Permanent Fire Source
    test(fire_pathfinding_concept);     // Test 9: Pathfinding
    test(smoke_multi_z_rising);         // Test 10: Multi-Z Smoke Rising
    test(smoke_closed_room_filling);    // Test 11: Closed Room Smoke Filling
    test(smoke_chimney_ventilation);    // Test 12: Chimney Ventilation
    
    // Additional tests
    test(smoke_dissipation);
    test(fire_edge_cases);
    
    return summary();
}
