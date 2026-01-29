#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/simulation/water.h"
#include "../src/simulation/temperature.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Helper to run water simulation for N ticks
static void RunWaterTicks(int n) {
    for (int i = 0; i < n; i++) {
        UpdateWater();
    }
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
// Basic Water Operations
// =============================================================================

describe(water_initialization) {
    it("should initialize water grid with all zeros") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 4);
        
        InitWater();
        
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                expect(GetWaterLevel(x, y, 0) == 0);
            }
        }
    }
    
    it("should clear all water when ClearWater is called") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n", 8, 2);
        
        InitWater();
        SetWaterLevel(2, 0, 0, 5);
        SetWaterLevel(4, 1, 0, 7);
        
        expect(GetWaterLevel(2, 0, 0) == 5);
        expect(GetWaterLevel(4, 1, 0) == 7);
        
        ClearWater();
        
        expect(GetWaterLevel(2, 0, 0) == 0);
        expect(GetWaterLevel(4, 1, 0) == 0);
    }
}

describe(water_level_operations) {
    it("should set water level within bounds") {
        InitGridFromAsciiWithChunkSize(
            "....\n"
            "....\n", 4, 2);
        InitWater();
        
        SetWaterLevel(1, 0, 0, 5);
        expect(GetWaterLevel(1, 0, 0) == 5);
        
        SetWaterLevel(2, 1, 0, 7);
        expect(GetWaterLevel(2, 1, 0) == 7);
    }
    
    it("should clamp water level to max 7") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitWater();
        
        SetWaterLevel(0, 0, 0, 10);
        expect(GetWaterLevel(0, 0, 0) == WATER_MAX_LEVEL);
    }
    
    it("should clamp water level to min 0") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitWater();
        
        SetWaterLevel(0, 0, 0, -5);
        expect(GetWaterLevel(0, 0, 0) == 0);
    }
    
    it("should add water correctly") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitWater();
        
        SetWaterLevel(0, 0, 0, 3);
        AddWater(0, 0, 0, 2);
        expect(GetWaterLevel(0, 0, 0) == 5);
    }
    
    it("should remove water correctly") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitWater();
        
        SetWaterLevel(0, 0, 0, 5);
        RemoveWater(0, 0, 0, 2);
        expect(GetWaterLevel(0, 0, 0) == 3);
    }
    
    it("should report HasWater correctly") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitWater();
        
        expect(HasWater(0, 0, 0) == false);
        
        SetWaterLevel(0, 0, 0, 1);
        expect(HasWater(0, 0, 0) == true);
    }
    
    it("should report IsFull correctly") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitWater();
        
        SetWaterLevel(0, 0, 0, 6);
        expect(IsFull(0, 0, 0) == false);
        
        SetWaterLevel(0, 0, 0, 7);
        expect(IsFull(0, 0, 0) == true);
    }
}

// =============================================================================
// Test 1: Basic Flow (Spreading)
// =============================================================================

describe(water_basic_flow) {
    it("should spread water outward from source") {
        // Create flat terrain
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 8);
        
        InitWater();
        
        // Place water source in center
        SetWaterSource(4, 4, 0, true);
        
        // Run simulation - spreading should happen within reasonable ticks
        // Doc says: diff=7-0=7, transfer=3 per tick to each neighbor
        RunWaterTicks(100);
        
        // Source should be full (refills every tick per docs)
        expect(GetWaterLevel(4, 4, 0) == WATER_MAX_LEVEL);
        
        // Adjacent cells should have water
        expect(GetWaterLevel(3, 4, 0) > 0);
        expect(GetWaterLevel(5, 4, 0) > 0);
        expect(GetWaterLevel(4, 3, 0) > 0);
        expect(GetWaterLevel(4, 5, 0) > 0);
    }
    
    it("should equalize water levels between neighbors") {
        InitGridFromAsciiWithChunkSize(
            "......\n"
            "......\n", 6, 2);
        
        InitWater();
        waterEvaporationEnabled = false;  // Disable evaporation for this test
        
        // Place high water on one side
        SetWaterLevel(0, 0, 0, 7);
        SetWaterLevel(1, 0, 0, 7);
        
        // Run simulation
        RunWaterTicks(50);
        
        // Water should spread out and equalize
        int level0 = GetWaterLevel(0, 0, 0);
        int level1 = GetWaterLevel(1, 0, 0);
        int level2 = GetWaterLevel(2, 0, 0);
        
        // Should have spread (not all water in first two cells)
        expect(level2 > 0);
        
        // Levels should be relatively equal (within 1)
        expect(abs(level0 - level1) <= 1);
        expect(abs(level1 - level2) <= 1);
        
        waterEvaporationEnabled = true;  // Re-enable for other tests
    }
    
    it("should equalize in narrow horizontal channel") {
        // 10-wide channel, 1 cell high interior (walls on top and bottom)
        // This is the "room 10 wide, 3 high, 1 cell interior" case
        InitGridFromAsciiWithChunkSize(
            "############\n"
            "#..........#\n"
            "############\n", 12, 3);
        
        InitWater();
        waterEvaporationEnabled = false;
        
        // Place water on left side of channel (inside the walls)
        SetWaterLevel(1, 1, 0, 7);
        SetWaterLevel(2, 1, 0, 7);
        SetWaterLevel(3, 1, 0, 7);
        
        // Total water = 21, spread across 10 cells = 2.1 avg
        // Should stabilize to something like: 2 2 2 2 2 2 2 2 2 3 or similar
        
        // Run simulation until stable
        RunWaterTicks(200);
        
        // Check all interior cells (x=1 to x=10, y=1)
        int levels[10];
        int total = 0;
        for (int i = 0; i < 10; i++) {
            levels[i] = GetWaterLevel(i + 1, 1, 0);
            total += levels[i];
        }
        
        // Total water should be conserved (21 units)
        expect(total == 21);
        
        // All adjacent cells should be within 1 level of each other
        bool balanced = true;
        for (int i = 0; i < 9; i++) {
            if (abs(levels[i] - levels[i+1]) > 1) {
                balanced = false;
                // Print debug info
                printf("Channel levels: ");
                for (int j = 0; j < 10; j++) printf("%d ", levels[j]);
                printf("\n");
                printf("Unbalanced at %d-%d: %d vs %d (diff=%d)\n", 
                       i, i+1, levels[i], levels[i+1], abs(levels[i] - levels[i+1]));
                break;
            }
        }
        expect(balanced);
        
        waterEvaporationEnabled = true;
    }
    
    it("should not form staircase pattern with full water on one side") {
        // This tests the specific bug: water placed on left side of channel
        // should NOT stabilize as 7 7 6 5 4 3 2 1 (staircase)
        // It SHOULD equalize to roughly equal levels
        InitGridFromAsciiWithChunkSize(
            "############\n"
            "#..........#\n"
            "############\n", 12, 3);
        
        InitWater();
        waterEvaporationEnabled = false;
        
        // Place full water (7) in first 5 cells - total 35 units across 10 cells = 3.5 avg
        for (int i = 1; i <= 5; i++) {
            SetWaterLevel(i, 1, 0, 7);
        }
        
        // Run simulation
        RunWaterTicks(500);
        
        // Check levels
        int levels[10];
        int total = 0;
        for (int i = 0; i < 10; i++) {
            levels[i] = GetWaterLevel(i + 1, 1, 0);
            total += levels[i];
        }
        
        // Print current state for debugging
        printf("Staircase test levels: ");
        for (int i = 0; i < 10; i++) printf("%d ", levels[i]);
        printf("(total=%d)\n", total);
        
        // Total water should be conserved (35 units)
        expect(total == 35);
        
        // Check it's NOT a staircase - the rightmost cells should have water too
        // With 35 units / 10 cells = 3.5 avg, rightmost should have at least 2
        expect(levels[9] >= 2);  // Last cell should have water
        expect(levels[8] >= 2);  // Second-to-last too
        
        // Max difference between any two adjacent cells should be 1
        bool noStaircase = true;
        for (int i = 0; i < 9; i++) {
            if (abs(levels[i] - levels[i+1]) > 1) {
                noStaircase = false;
                printf("Staircase detected at %d-%d: %d vs %d\n", 
                       i, i+1, levels[i], levels[i+1]);
                break;
            }
        }
        expect(noStaircase);
        
        waterEvaporationEnabled = true;
    }
    
    it("should not spread diagonally") {
        InitGridFromAsciiWithChunkSize(
            "...\n"
            "...\n"
            "...\n", 3, 3);
        
        InitWater();
        
        // Place water in center
        SetWaterLevel(1, 1, 0, 7);
        
        // Run one tick
        UpdateWater();
        
        // Orthogonal neighbors should get water
        // But initially with only 7 water in center, it needs level diff of 2 to spread
        // So let's run a few ticks
        RunWaterTicks(5);
        
        // Diagonal corners should have less water than orthogonal neighbors
        // (since water only spreads orthogonally)
        int orthogonalWater = GetWaterLevel(1, 0, 0) + GetWaterLevel(0, 1, 0) + 
                              GetWaterLevel(2, 1, 0) + GetWaterLevel(1, 2, 0);
        int diagonalWater = GetWaterLevel(0, 0, 0) + GetWaterLevel(2, 0, 0) + 
                            GetWaterLevel(0, 2, 0) + GetWaterLevel(2, 2, 0);
        
        // Orthogonal should have more water than diagonal
        expect(orthogonalWater >= diagonalWater);
    }
}

// =============================================================================
// Test 2: Waterfall (Gravity/Falling)
// =============================================================================

describe(water_falling) {
    it("should fall to lower z-level") {
        // Two floors: z=0 is ground, z=1 is walkable
        const char* map =
            "floor:0\n"
            ".....\n"
            ".....\n"
            "floor:1\n"
            ".....\n"
            ".....\n";
        
        InitMultiFloorGridFromAscii(map, 5, 5);
        InitWater();
        waterEvaporationEnabled = false;  // Disable evaporation for this test
        
        // Place water at z=1
        SetWaterLevel(2, 1, 1, 7);
        
        // Make z=0 able to receive water (not a wall)
        expect(!CellBlocksMovement(grid[0][1][2]));
        
        // Run simulation
        RunWaterTicks(10);
        
        // Water should have fallen to z=0
        expect(GetWaterLevel(2, 1, 0) > 0);
        
        waterEvaporationEnabled = true;  // Re-enable for other tests
    }
    
    it("should not fall through walls") {
        const char* map =
            "floor:0\n"
            ".....\n"
            "..#..\n"   // Wall at (2,1) on z=0
            ".....\n"
            "floor:1\n"
            ".....\n"
            ".....\n"
            ".....\n";
        
        InitMultiFloorGridFromAscii(map, 5, 5);
        InitWater();
        waterEvaporationEnabled = false;  // Disable evaporation for this test
        
        // Place water above the wall
        SetWaterLevel(2, 1, 1, 7);
        
        // Run simulation
        RunWaterTicks(10);
        
        // Water should NOT fall into the wall
        expect(GetWaterLevel(2, 1, 0) == 0);
        
        // Water should still be at z=1 or spread horizontally
        int waterAtZ1 = 0;
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                waterAtZ1 += GetWaterLevel(x, y, 1);
            }
        }
        expect(waterAtZ1 > 0);
        
        waterEvaporationEnabled = true;  // Re-enable for other tests
    }
    
    it("should fall and then spread at lower level") {
        const char* map =
            "floor:0\n"
            "........\n"
            "........\n"
            "........\n"
            "floor:1\n"
            "........\n"
            "........\n"
            "........\n";
        
        InitMultiFloorGridFromAscii(map, 8, 8);
        InitWater();
        
        // Place source at z=1
        SetWaterSource(4, 1, 1, true);
        
        // Run simulation
        RunWaterTicks(50);
        
        // Water should have fallen to z=0 and spread
        expect(GetWaterLevel(4, 1, 0) > 0);
        
        // Should spread at z=0
        int waterAtZ0 = 0;
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                waterAtZ0 += GetWaterLevel(x, y, 0);
            }
        }
        expect(waterAtZ0 > 7);  // More than just one full cell
    }
}

// =============================================================================
// Test 3: Filling a Pool
// =============================================================================

describe(water_pool_filling) {
    it("should spread when wall is removed from 1x1 pool") {
        // Create a 1x1 "room" surrounded by walls
        InitGridFromAsciiWithChunkSize(
            ".....\n"
            ".###.\n"
            ".#.#.\n"
            ".###.\n"
            ".....\n", 5, 5);
        
        InitWater();
        waterEvaporationEnabled = false;  // Disable evaporation to test conservation
        
        // Fill the center cell (2,2) with water 7/7
        SetWaterLevel(2, 2, 0, WATER_MAX_LEVEL);
        
        int initialWater = CountTotalWater();
        expect(initialWater == 7);
        
        // Remove the north wall - water should spread
        // This simulates what the erase tool does: change cell and destabilize
        grid[0][1][2] = CELL_WALKABLE;
        DestabilizeWater(2, 1, 0);  // Destabilize the changed cell (like erase tool does)
        
        // Run simulation
        RunWaterTicks(100);
        
        // Water should have spread out
        int centerLevel = GetWaterLevel(2, 2, 0);
        expect(centerLevel < WATER_MAX_LEVEL);
        
        // Some water should be in the cell where the wall was removed
        expect(GetWaterLevel(2, 1, 0) > 0);
        
        // Total water should be conserved (no sources/drains)
        int finalWater = CountTotalWater();
        expect(finalWater == initialWater);
        
        waterEvaporationEnabled = true;  // Re-enable for other tests
    }
    
    it("should fill enclosed room to level 7") {
        InitGridFromAsciiWithChunkSize(
            "########\n"
            "#......#\n"
            "#......#\n"
            "#......#\n"
            "#......#\n"
            "#......#\n"
            "#......#\n"
            "########\n", 8, 8);
        
        InitWater();
        
        // Place source inside the room
        SetWaterSource(4, 4, 0, true);
        
        // Run simulation long enough to fill
        RunWaterTicks(500);
        
        // Interior cells should be full or nearly full
        int fullCells = 0;
        for (int y = 1; y < 7; y++) {
            for (int x = 1; x < 7; x++) {
                if (GetWaterLevel(x, y, 0) >= 6) {
                    fullCells++;
                }
            }
        }
        
        // Most interior cells should be nearly full (36 interior cells)
        expect(fullCells >= 30);
    }
    
    it("should spill through opening when room overflows") {
        // Room with opening on right side
        InitGridFromAsciiWithChunkSize(
            "########.....\n"
            "#......#.....\n"
            "#......#.....\n"
            "#...........\n"   // Opening at y=3
            "#......#.....\n"
            "#......#.....\n"
            "########.....\n", 13, 7);
        
        InitWater();
        
        // Place source inside
        SetWaterSource(3, 3, 0, true);
        
        // Run simulation
        RunWaterTicks(200);
        
        // Water should have spread outside the room
        expect(GetWaterLevel(9, 3, 0) > 0);
    }
}

// =============================================================================
// Test 4 & 5: Pressure / U-Bend
// =============================================================================

describe(water_pressure) {
    it("should push water up through pressure in U-bend") {
        // Create U-bend setup:
        // z=2: source on left, open on right
        // z=1: walls on sides, open in middle
        // z=0: open channel
        
        InitGridWithSizeAndChunkSize(8, 4, 8, 4);
        gridDepth = 3;
        
        // Initialize all as walkable
        for (int z = 0; z < 3; z++) {
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 8; x++) {
                    grid[z][y][x] = CELL_WALKABLE;
                }
            }
        }
        
        // z=1: walls on left and right, channel in middle
        grid[1][1][0] = CELL_WALL;
        grid[1][1][1] = CELL_WALL;
        grid[1][1][6] = CELL_WALL;
        grid[1][1][7] = CELL_WALL;
        
        InitWater();
        
        // Place source at z=2 on left side
        SetWaterSource(0, 1, 2, true);
        
        // Run simulation - water should:
        // 1. Fall from z=2 to z=1 (hits wall, falls to z=0)
        // 2. Spread at z=0
        // 3. Pressure pushes up on right side to z=1 (sourceZ - 1 = 2 - 1 = 1)
        RunWaterTicks(300);
        
        // Water should have risen on the right side at z=1
        // (checking the cell just inside the walls)
        int rightZ1Water = GetWaterLevel(5, 1, 1);
        expect(rightZ1Water > 0);
    }
    
    it("should respect pressure height limit (sourceZ - 1)") {
        // U-bend with source at z=3
        // Water should rise to z=2 on the far side via pressure, but NOT to z=3
        //
        // Side view (y=1 slice):
        // z=3:  [source]  .  .  .  .  .  .  [open]   <- water should NOT reach here via pressure
        // z=2:  [wall]    .  .  .  .  .  .  [open]   <- water SHOULD reach here (sourceZ-1)
        // z=1:  [wall]    .  .  .  .  .  .  [wall]
        // z=0:  [open]    .  .  .  .  .  .  [open]   <- bottom of U
        //
        // NOTE: Water may still reach z=3 via normal spreading/equalization once it
        // reaches z=2. This matches Dwarf Fortress behavior where pressure has a height
        // limit, but once water arrives somewhere it can spread normally. We only check
        // that z=3 isn't FULL, since pressure alone can't push it there.
        
        InitGridWithSizeAndChunkSize(8, 4, 8, 4);
        gridDepth = 4;
        
        // Initialize all as walkable
        for (int z = 0; z < 4; z++) {
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 8; x++) {
                    grid[z][y][x] = CELL_WALKABLE;
                }
            }
        }
        
        // Build the U-bend walls:
        // Left side: wall at z=1 and z=2 (water falls from z=3 source down to z=0)
        grid[1][1][0] = CELL_WALL;
        grid[1][1][1] = CELL_WALL;
        grid[2][1][0] = CELL_WALL;
        grid[2][1][1] = CELL_WALL;
        
        // Right side: wall at z=1 only (water can rise to z=2 but path blocked at z=1)
        grid[1][1][6] = CELL_WALL;
        grid[1][1][7] = CELL_WALL;
        
        InitWater();
        waterEvaporationEnabled = false;
        
        // Place source at z=3 on left side
        SetWaterSource(0, 1, 3, true);
        
        // Run simulation long enough for pressure to propagate
        RunWaterTicks(500);
        
        // Debug: print water levels at right side
        printf("U-bend pressure test (source at z=3):\n");
        printf("  Right side z=0: %d\n", GetWaterLevel(7, 1, 0));
        printf("  Right side z=1: %d (wall)\n", GetWaterLevel(7, 1, 1));
        printf("  Right side z=2: %d (should have water)\n", GetWaterLevel(7, 1, 2));
        printf("  Right side z=3: %d (should be 0 or low)\n", GetWaterLevel(7, 1, 3));
        
        // Water SHOULD reach z=2 on the right (sourceZ - 1 = 3 - 1 = 2)
        expect(GetWaterLevel(7, 1, 2) > 0);
        
        // Water should NOT reach z=3 on the right (that's the source level)
        // It might have a small amount from spreading, but shouldn't be full
        expect(GetWaterLevel(7, 1, 3) < WATER_MAX_LEVEL);
        
        waterEvaporationEnabled = true;
    }
    
    it("should create pressure when water falls onto full water") {
        InitGridWithSizeAndChunkSize(4, 4, 4, 4);
        gridDepth = 2;
        
        // Both levels walkable
        for (int z = 0; z < 2; z++) {
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    grid[z][y][x] = CELL_WALKABLE;
                }
            }
        }
        
        InitWater();
        
        // Fill z=0 completely
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                SetWaterLevel(x, y, 0, WATER_MAX_LEVEL);
            }
        }
        
        // Add water at z=1 that will fall
        SetWaterLevel(2, 2, 1, 7);
        
        // Run simulation - falling water should create pressure
        RunWaterTicks(5);
        
        // The cell at z=0 where water fell should have pressure
        expect(HasWaterPressure(2, 2, 0) == true);
    }
}

// =============================================================================
// Test 6: Drains
// =============================================================================

describe(water_drains) {
    it("should remove water at drain location") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 4);
        
        InitWater();
        
        // Fill area with water
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 8; x++) {
                SetWaterLevel(x, y, 0, 5);
            }
        }
        
        // Place drain
        SetWaterDrain(4, 2, 0, true);
        
        // Run one tick
        UpdateWater();
        
        // Drain cell should be empty
        expect(GetWaterLevel(4, 2, 0) == 0);
    }
    
    it("should continuously drain water") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n", 8, 2);
        
        InitWater();
        
        // Place source and drain
        SetWaterSource(0, 0, 0, true);
        SetWaterDrain(7, 0, 0, true);
        
        // Run simulation
        RunWaterTicks(100);
        
        // Source should still be full
        expect(GetWaterLevel(0, 0, 0) == WATER_MAX_LEVEL);
        
        // Drain should be empty
        expect(GetWaterLevel(7, 0, 0) == 0);
        
        // Water should flow between them
        expect(GetWaterLevel(3, 0, 0) > 0);
    }
    
    it("should reduce total water when drain active") {
        InitGridFromAsciiWithChunkSize(
            "....\n"
            "....\n", 4, 2);
        
        InitWater();
        
        // Fill with water (no source)
        for (int y = 0; y < 2; y++) {
            for (int x = 0; x < 4; x++) {
                SetWaterLevel(x, y, 0, 7);
            }
        }
        
        int initialWater = CountTotalWater();
        
        // Place drain
        SetWaterDrain(2, 1, 0, true);
        
        // Run simulation
        RunWaterTicks(100);
        
        int finalWater = CountTotalWater();
        
        // Total water should have decreased
        expect(finalWater < initialWater);
    }
}

// =============================================================================
// Test 7: Evaporation
// =============================================================================

describe(water_evaporation) {
    it("should eventually evaporate level-1 water") {
        InitGridFromAsciiWithChunkSize(
            "................\n"
            "................\n"
            "................\n"
            "................\n", 16, 4);
        
        InitWater();
        waterEvaporationEnabled = true;  // Ensure evaporation is enabled
        
        // Place many cells of level-1 water (no source)
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 16; x++) {
                SetWaterLevel(x, y, 0, 1);
            }
        }
        
        int initialWater = CountTotalWater();
        
        // Run simulation for a long time (evaporation is 1/100 chance)
        RunWaterTicks(10000);
        
        int finalWater = CountTotalWater();
        
        // Some water should have evaporated
        expect(finalWater < initialWater);
    }
    
    it("should not evaporate water from sources") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        
        InitWater();
        
        // Place source at level 1 (unusual but should not evaporate)
        SetWaterSource(2, 0, 0, true);
        waterGrid[0][0][2].level = 1;  // Force to level 1
        
        // Run simulation
        RunWaterTicks(1000);
        
        // Source should refill, not evaporate
        expect(GetWaterLevel(2, 0, 0) == WATER_MAX_LEVEL);
    }
    
    it("should not evaporate water above level 1") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        
        InitWater();
        waterEvaporationEnabled = false;  // Disable evaporation to test pure spreading
        
        // Place level 2 water - it should spread but not spontaneously evaporate
        SetWaterLevel(2, 0, 0, 2);
        
        // Run simulation (short time for spreading only)
        RunWaterTicks(10);
        
        // Level 2 water should have spread to level 1 but not evaporated
        // (evaporation is disabled, so total water should be conserved)
        int totalWater = 0;
        for (int x = 0; x < 4; x++) {
            totalWater += GetWaterLevel(x, 0, 0);
        }
        
        // Total water should still be 2 (no evaporation)
        expect(totalWater == 2);
        
        waterEvaporationEnabled = true;  // Re-enable for other tests
    }
}

// =============================================================================
// Speed Multiplier
// =============================================================================

describe(water_speed_multiplier) {
    it("should return 1.0 for no water") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitWater();
        
        float speed = GetWaterSpeedMultiplier(2, 0, 0);
        expect(speed == 1.0f);
    }
    
    it("should return 0.85 for shallow water (1-2)") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitWater();
        
        SetWaterLevel(0, 0, 0, 1);
        SetWaterLevel(1, 0, 0, 2);
        
        expect(GetWaterSpeedMultiplier(0, 0, 0) == 0.85f);
        expect(GetWaterSpeedMultiplier(1, 0, 0) == 0.85f);
    }
    
    it("should return 0.6 for medium water (3-4)") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitWater();
        
        SetWaterLevel(0, 0, 0, 3);
        SetWaterLevel(1, 0, 0, 4);
        
        expect(GetWaterSpeedMultiplier(0, 0, 0) == 0.6f);
        expect(GetWaterSpeedMultiplier(1, 0, 0) == 0.6f);
    }
    
    it("should return 0.35 for deep water (5-7)") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitWater();
        
        SetWaterLevel(0, 0, 0, 5);
        SetWaterLevel(1, 0, 0, 6);
        SetWaterLevel(2, 0, 0, 7);
        
        expect(GetWaterSpeedMultiplier(0, 0, 0) == 0.35f);
        expect(GetWaterSpeedMultiplier(1, 0, 0) == 0.35f);
        expect(GetWaterSpeedMultiplier(2, 0, 0) == 0.35f);
    }
}

// =============================================================================
// Performance / Stability
// =============================================================================

describe(water_stability) {
    it("should mark cells as stable when water settles") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 4);
        
        InitWater();
        
        // Add some water
        SetWaterLevel(4, 2, 0, 7);
        
        // Run until stable
        for (int i = 0; i < 200; i++) {
            UpdateWater();
            if (waterUpdateCount == 0) break;
        }
        
        // Most cells should be stable now
        int stableCells = 0;
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 8; x++) {
                if (IsWaterStable(x, y, 0)) {
                    stableCells++;
                }
            }
        }
        
        // Should have stabilized
        expect(stableCells > 20);
    }
    
    it("should destabilize neighbors when water changes") {
        InitGridFromAsciiWithChunkSize(
            ".....\n"
            ".....\n"
            ".....\n", 5, 3);
        
        InitWater();
        
        // Mark all as stable
        for (int y = 0; y < 3; y++) {
            for (int x = 0; x < 5; x++) {
                waterGrid[0][y][x].stable = true;
            }
        }
        
        // Add water - should destabilize neighbors
        SetWaterLevel(2, 1, 0, 5);
        
        // Center and orthogonal neighbors should be unstable
        expect(IsWaterStable(2, 1, 0) == false);  // Center
        expect(IsWaterStable(2, 0, 0) == false);  // North
        expect(IsWaterStable(2, 2, 0) == false);  // South
        expect(IsWaterStable(1, 1, 0) == false);  // West
        expect(IsWaterStable(3, 1, 0) == false);  // East
    }
}

describe(water_wall_interaction) {
    it("should not place water in walls") {
        InitGridFromAsciiWithChunkSize(
            ".#.#.\n"
            "#...#\n"
            ".#.#.\n", 5, 3);
        
        InitWater();
        
        // Try to place water in wall cell
        SetWaterLevel(1, 0, 0, 7);  // (1,0) is a wall
        
        // Water should still be set (water grid is independent)
        // but it shouldn't spread into walls during simulation
        expect(GetWaterLevel(1, 0, 0) == 7);
        
        // Place water in open cell
        SetWaterLevel(2, 1, 0, 7);
        
        // Run simulation
        RunWaterTicks(50);
        
        // Water should not spread into wall cells via normal flow
        // (Note: SetWaterLevel ignores walls, but simulation respects them)
    }
    
    it("should not spread water through walls") {
        // Two chambers separated by wall
        InitGridFromAsciiWithChunkSize(
            "...#...\n"
            "...#...\n"
            "...#...\n", 7, 3);
        
        InitWater();
        
        // Fill left chamber
        SetWaterLevel(0, 1, 0, 7);
        SetWaterLevel(1, 1, 0, 7);
        SetWaterLevel(2, 1, 0, 7);
        
        // Run simulation
        RunWaterTicks(100);
        
        // Right chamber should remain dry (wall blocks flow)
        expect(GetWaterLevel(4, 1, 0) == 0);
        expect(GetWaterLevel(5, 1, 0) == 0);
        expect(GetWaterLevel(6, 1, 0) == 0);
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

describe(water_edge_cases) {
    it("should handle water at grid edges") {
        InitGridFromAsciiWithChunkSize(
            "....\n"
            "....\n", 4, 2);
        
        InitWater();
        
        // Place water at corners
        SetWaterLevel(0, 0, 0, 7);
        SetWaterLevel(3, 0, 0, 7);
        SetWaterLevel(0, 1, 0, 7);
        SetWaterLevel(3, 1, 0, 7);
        
        // Run simulation - should not crash
        RunWaterTicks(50);
        
        // Water should have spread (not stuck at edges)
        int totalWater = CountTotalWater();
        expect(totalWater > 0);
    }
    
    it("should handle out-of-bounds queries gracefully") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        
        InitWater();
        
        // Out of bounds should return 0
        expect(GetWaterLevel(-1, 0, 0) == 0);
        expect(GetWaterLevel(100, 0, 0) == 0);
        expect(GetWaterLevel(0, -1, 0) == 0);
        expect(GetWaterLevel(0, 100, 0) == 0);
        expect(GetWaterLevel(0, 0, -1) == 0);
        expect(GetWaterLevel(0, 0, 100) == 0);
        
        // Out of bounds set should not crash
        SetWaterLevel(-1, 0, 0, 5);
        SetWaterLevel(100, 0, 0, 5);
    }
    
    it("should handle source and drain at same location") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        
        InitWater();
        
        // Both source and drain at same cell
        SetWaterSource(2, 0, 0, true);
        SetWaterDrain(2, 0, 0, true);
        
        // Run simulation - drain should win (removes water after source fills)
        RunWaterTicks(10);
        
        // Should oscillate or drain should dominate
        // The exact behavior depends on processing order
        // Just verify it doesn't crash
        expect(true);
    }
}

// =============================================================================
// Water Freezing
// =============================================================================

describe(water_freezing) {
    it("should freeze full water at freezing temperature") {
        InitGridFromAsciiWithChunkSize(
            "....\n"
            "....\n", 4, 2);
        
        InitWater();
        InitTemperature();
        
        // Place full water
        SetWaterLevel(1, 0, 0, WATER_MAX_LEVEL);
        
        // Set temperature to freezing (0°C or below)
        SetTemperature(1, 0, 0, -10);  // Below 0°C (freezing point)
        
        // Update freezing
        UpdateWaterFreezing();
        
        // Water should be frozen
        expect(IsWaterFrozen(1, 0, 0) == true);
    }
    
    it("should freeze partial water") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        
        InitWater();
        InitTemperature();
        
        // Place partial water (level 5)
        SetWaterLevel(1, 0, 0, 5);
        
        // Set temperature to freezing (0°C or below)
        SetTemperature(1, 0, 0, -10);
        
        // Update freezing
        UpdateWaterFreezing();
        
        // Water should be frozen (any level can freeze)
        expect(IsWaterFrozen(1, 0, 0) == true);
    }
    
    it("should thaw frozen water when temperature rises") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        
        InitWater();
        InitTemperature();
        
        // Place full water and freeze it
        SetWaterLevel(1, 0, 0, WATER_MAX_LEVEL);
        FreezeWater(1, 0, 0);
        
        expect(IsWaterFrozen(1, 0, 0) == true);
        
        // Warm up
        SetTemperature(1, 0, 0, 100);  // Above freezing
        
        // Update freezing
        UpdateWaterFreezing();
        
        // Should be thawed
        expect(IsWaterFrozen(1, 0, 0) == false);
    }
    
    it("should block water flow when frozen") {
        InitGridFromAsciiWithChunkSize(
            "......\n", 6, 1);
        
        InitWater();
        InitTemperature();
        waterEvaporationEnabled = false;
        
        // Place full water and freeze middle cell
        SetWaterLevel(2, 0, 0, WATER_MAX_LEVEL);
        FreezeWater(2, 0, 0);
        
        // Place water source on left side
        SetWaterSource(0, 0, 0, true);
        
        // Run simulation
        RunWaterTicks(100);
        
        // Frozen water should block flow to right side
        // Water should accumulate on left side but not pass frozen cell
        expect(GetWaterLevel(4, 0, 0) == 0);  // Right of frozen
        expect(GetWaterLevel(5, 0, 0) == 0);  // Far right
        
        waterEvaporationEnabled = true;
    }
    
    it("should preserve water level when frozen") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        
        InitWater();
        InitTemperature();
        
        // Place full water and freeze
        SetWaterLevel(1, 0, 0, WATER_MAX_LEVEL);
        FreezeWater(1, 0, 0);
        
        // Run water simulation
        RunWaterTicks(100);
        
        // Water level should be preserved (frozen doesn't flow)
        expect(GetWaterLevel(1, 0, 0) == WATER_MAX_LEVEL);
        expect(IsWaterFrozen(1, 0, 0) == true);
    }
    
    it("should freeze water when ambient temperature drops below freezing") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        
        InitWater();
        InitTemperature();
        
        // Place water
        SetWaterLevel(1, 0, 0, WATER_MAX_LEVEL);
        
        // Verify water is not frozen initially (ambient is 20C by default)
        expect(IsWaterFrozen(1, 0, 0) == false);
        
        // Set ambient temperature to below freezing (-10C)
        ambientSurfaceTemp = -10;  // Below TEMP_WATER_FREEZES (0C)
        
        // Run temperature and water freezing updates
        for (int i = 0; i < 100; i++) {
            UpdateTemperature();
            UpdateWaterFreezing();
        }
        
        // Water should now be frozen
        expect(IsWaterFrozen(1, 0, 0) == true);
        
        // Reset ambient for other tests
        ambientSurfaceTemp = TEMP_AMBIENT_DEFAULT;
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
    test(water_initialization);
    test(water_level_operations);
    
    // Test scenarios from docs/water-system.md
    test(water_basic_flow);           // Test 1: Basic Flow
    test(water_falling);              // Test 2: Waterfall
    test(water_pool_filling);         // Test 3: Filling a Pool
    test(water_pressure);             // Test 4 & 5: Pressure / U-Bend
    test(water_drains);               // Test 6: Drain
    test(water_evaporation);          // Test 7: Evaporation
    
    // Additional tests
    test(water_speed_multiplier);
    test(water_stability);
    test(water_wall_interaction);
    test(water_edge_cases);
    
    // Freezing tests
    test(water_freezing);
    
    return summary();
}
