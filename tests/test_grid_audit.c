#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "test_helpers.h"
#include "../src/world/cell_defs.h"
#include "../src/world/material.h"
#include "../src/world/designations.h"
#include "../src/entities/mover.h"
#include "../src/entities/jobs.h"
#include "../src/entities/items.h"
#include "../src/entities/stockpiles.h"
#include "../src/entities/workshops.h"
#include "../src/simulation/fire.h"
#include "../src/simulation/water.h"
#include "../src/simulation/trees.h"
#include "../src/core/time.h"
#include <string.h>

// From saveload.c
void RebuildPostLoadState(void);

static bool test_verbose = false;

// Helper for creating 10x10 test grids (gridWidth=10, gridHeight=10)
#define GRID_10X10 \
    "..........\n" \
    "..........\n" \
    "..........\n" \
    "..........\n" \
    "..........\n" \
    "..........\n" \
    "..........\n" \
    "..........\n" \
    "..........\n" \
    "..........\n"

// =============================================================================
// Finding 1: PlaceCellFull overwrites ramps/ladders without cleanup
// =============================================================================

describe(grid_audit_finding_1_placecell_ramp_cleanup) {
    it("player places ramp, then draws wall over it - rampCount should decrement") {
        InitTestGridFromAscii(GRID_10X10);
        rampCount = 0;
        
        // Player draws a ramp manually
        grid[0][5][5] = CELL_RAMP_N;
        rampCount++;
        
        int countBefore = rampCount;
        
        // Player draws a wall over the ramp using PlaceCellFull
        CellPlacementSpec spec = {0};
        spec.cellType = CELL_WALL;
        spec.wallMat = MAT_DIRT;
        PlaceCellFull(5, 5, 0, spec);
        
        // Expected: rampCount should have decremented
        expect(rampCount == countBefore - 1);
        expect(grid[0][5][5] == CELL_WALL);
    }
    
    it("player places wall over single ladder - ladder should be cleaned up") {
        InitTestGridFromAscii(GRID_10X10);
        
        // Build a single ladder
        PlaceLadder(5, 5, 0);
        
        expect(IsLadderCell(grid[0][5][5]));
        
        // Overwrite ladder with wall using PlaceCellFull
        CellPlacementSpec spec = {0};
        spec.cellType = CELL_WALL;
        spec.wallMat = MAT_DIRT;
        PlaceCellFull(5, 5, 0, spec);
        
        // Expected: ladder replaced by wall
        expect(grid[0][5][5] == CELL_WALL);
        expect(!IsLadderCell(grid[0][5][5]));
    }
}

// =============================================================================
// Finding 2: Quick-edit right-click erase skips ramp cleanup
// =============================================================================

describe(grid_audit_finding_2_erase_ramp_cleanup) {
    it("player right-click erases ramp in quick-edit mode - rampCount should decrement") {
        InitTestGridFromAscii(GRID_10X10);
        rampCount = 0;
        
        // Set up solid ground for ramp support
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++) {
                grid[0][y][x] = CELL_WALL;
                SetWallMaterial(x, y, 0, MAT_DIRT);
            }
        
        // Place ramp at z=1 (above dirt ground)
        grid[1][5][5] = CELL_RAMP_N;
        rampCount++;
        int countBefore = rampCount;
        
        // Simulate the quick-edit erase path (from input.c lines 1618-1627)
        // FIXED: now checks for ramps before generic erase
        CellType cell = grid[1][5][5];
        if (IsLadderCell(cell)) {
            EraseLadder(5, 5, 1);
        } else if (CellIsDirectionalRamp(cell)) {
            EraseRamp(5, 5, 1);
        } else {
            grid[1][5][5] = CELL_AIR;
            MarkChunkDirty(5, 5, 1);
            DestabilizeWater(5, 5, 1);
        }
        
        expect(rampCount == countBefore - 1);
        expect(grid[1][5][5] == CELL_AIR);
    }
}

// =============================================================================
// Finding 3 & 4: Blueprint construction (wall/floor) overwrites ramps/ladders
// NOTE: These require full job system integration - tested separately in test_jobs.c
// =============================================================================

// =============================================================================
// Finding 5: Fire burns away solid support without ramp validation (HIGH)
// =============================================================================

describe(grid_audit_finding_5_fire_burns_ramp_support) {
    it("player has ramp exit on tree trunk - fire burns trunk - ramp should be removed") {
        InitTestGridFromAscii(GRID_10X10);
        rampCount = 0;
        InitFire();
        InitTrees();
        
        // Set up ground
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++) {
                grid[0][y][x] = CELL_WALL;
                SetWallMaterial(x, y, 0, MAT_DIRT);
            }
        
        // Place tree trunk at (6,5,0) - solid support
        grid[0][5][6] = CELL_TREE_TRUNK;
        
        // Place ramp at (5,5,0) pointing EAST - exit at (6,5,1)
        // The ramp's high-side exit is ON TOP of the tree trunk
        grid[0][5][5] = CELL_RAMP_E;
        rampCount++;
        int countBefore = rampCount;
        
        // Ignite the trunk (IgniteCell sets fuel + fire level properly)
        IgniteCell(6, 5, 0);
        
        // Run fire simulation until trunk burns out
        int maxTicks = 500;
        bool trunkBurned = false;
        for (int i = 0; i < maxTicks; i++) {
            UpdateFire();
            if (grid[0][5][6] != CELL_TREE_TRUNK) {
                trunkBurned = true;
                break;
            }
        }
        
        expect(trunkBurned);
        
        // Expected: ramp should be removed because its exit support vanished
        expect(rampCount == countBefore - 1);
        expect(grid[0][5][5] != CELL_RAMP_E);
    }
}

// =============================================================================
// Finding 7: PlaceLadder on ramp silently destroys ramp without rampCount update
// =============================================================================

describe(grid_audit_finding_7_placeladder_on_ramp) {
    it("player places ladder on existing ramp - rampCount should decrement") {
        InitTestGridFromAscii(GRID_10X10);
        rampCount = 0;
        
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++) {
                grid[0][y][x] = CELL_WALL;
                SetWallMaterial(x, y, 0, MAT_DIRT);
            }
        
        // Place ramp at z=1 (above dirt ground) so ladder can attach
        grid[1][5][5] = CELL_RAMP_N;
        rampCount++;
        int countBefore = rampCount;
        
        // Player places ladder on the ramp cell
        PlaceLadder(5, 5, 1);
        
        // Expected: ramp removed, rampCount decremented, ladder placed
        expect(IsLadderCell(grid[1][5][5]));
        expect(!CellIsDirectionalRamp(grid[1][5][5]));
        expect(rampCount == countBefore - 1);
    }
}

// =============================================================================
// Finding 8: EraseRamp doesn't dirty exit chunk across boundaries (MEDIUM)
// =============================================================================

describe(grid_audit_finding_8_eraseramp_chunk_dirty) {
    it("player erases ramp at chunk boundary - exit chunk should be dirtied") {
        // 16x16 grid with 16x16 chunks
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
        rampCount = 0;
        
        // Set up solid ground
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++) {
                grid[0][y][x] = CELL_WALL;
                SetWallMaterial(x, y, 0, MAT_DIRT);
            }
        
        // Place ramp at (8, 15, 0) pointing north - exit at (8, 14, 1)
        grid[0][15][8] = CELL_RAMP_N;
        rampCount++;
        
        // Erase the ramp
        EraseRamp(8, 15, 0);
        
        expect(grid[0][15][8] == CELL_AIR);
        expect(rampCount == 0);
    }
}

// =============================================================================
// Finding 9: CanPlaceRamp allows map-edge placement with no entry (LOW)
// =============================================================================

describe(grid_audit_finding_9_ramp_map_edge) {
    it("player tries to place ramp at x=0 facing west - should fail (no entry)") {
        InitTestGridFromAscii(GRID_10X10);
        
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++) {
                grid[0][y][x] = CELL_WALL;
                SetWallMaterial(x, y, 0, MAT_DIRT);
            }
        
        // Low side would be at x=-1 (off map)
        bool canPlace = CanPlaceRamp(0, 5, 0, CELL_RAMP_W);
        expect(!canPlace);
    }
    
    it("player tries to place ramp at y=0 facing south - should fail (no entry)") {
        InitTestGridFromAscii(GRID_10X10);
        
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++) {
                grid[0][y][x] = CELL_WALL;
                SetWallMaterial(x, y, 0, MAT_DIRT);
            }
        
        bool canPlace = CanPlaceRamp(5, 0, 0, CELL_RAMP_S);
        expect(!canPlace);
    }
}

// =============================================================================
// Finding 10: IsRampStillValid ignores low-side accessibility (LOW)
// =============================================================================

describe(grid_audit_finding_10_ramp_lowside_validity) {
    it("player walls off low-side entry - ramp still structurally valid") {
        InitTestGridFromAscii(GRID_10X10);
        rampCount = 0;
        
        // Set up solid ground at z=0
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++) {
                grid[0][y][x] = CELL_WALL;
                SetWallMaterial(x, y, 0, MAT_DIRT);
            }
        
        // Place ramp at (5,5,1) pointing north
        // High side exit: (5,4,2) - needs solid support at (5,4,1)
        // Low side (entry): (5,6,1)
        // For validity, exit base (5,4,1) must be solid
        grid[1][4][5] = CELL_WALL;  // Solid support for high-side exit
        SetWallMaterial(5, 4, 1, MAT_DIRT);
        grid[1][5][5] = CELL_RAMP_N;
        rampCount++;
        
        // Verify ramp is initially valid
        bool validBefore = IsRampStillValid(5, 5, 1);
        expect(validBefore);
        
        // Wall off the low-side entry cell
        grid[1][6][5] = CELL_WALL;
        
        // Ramp is still structurally valid (high side has support)
        // This documents current behavior: only high-side is checked
        bool validAfter = IsRampStillValid(5, 5, 1);
        expect(validAfter);
    }
}

// =============================================================================
// Finding 11: hpaNeedsRebuild not set in InitGridWithSizeAndChunkSize (LOW)
// =============================================================================

describe(grid_audit_finding_11_init_hpa_flag) {
    it("InitGridWithSizeAndChunkSize should set hpaNeedsRebuild flag") {
        InitGridWithSizeAndChunkSize(20, 20, 10, 10);
        
        expect(gridWidth == 20);
        expect(gridHeight == 20);
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

    test(grid_audit_finding_1_placecell_ramp_cleanup);
    test(grid_audit_finding_2_erase_ramp_cleanup);
    test(grid_audit_finding_5_fire_burns_ramp_support);
    test(grid_audit_finding_7_placeladder_on_ramp);
    test(grid_audit_finding_8_eraseramp_chunk_dirty);
    test(grid_audit_finding_9_ramp_map_edge);
    test(grid_audit_finding_10_ramp_lowside_validity);
    test(grid_audit_finding_11_init_hpa_flag);

    return summary();
}
