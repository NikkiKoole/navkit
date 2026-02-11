#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/world/material.h"
#include "../src/simulation/floordirt.h"
#include "../src/simulation/groundwear.h"
#include "../src/core/sim_manager.h"
#include <stdlib.h>
#include <string.h>

static bool test_verbose = false;

// =============================================================================
// Initialization
// =============================================================================

describe(floordirt_initialization) {
    it("should initialize dirt grid with all zeros") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n", 8, 2);
        InitFloorDirt();

        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                for (int x = 0; x < gridWidth; x++) {
                    expect(GetFloorDirt(x, y, z) == 0);
                }
            }
        }
    }

    it("should clear all dirt when ClearFloorDirt is called") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n", 8, 2);
        InitFloorDirt();

        floorDirtGrid[0][0][2] = 100;
        floorDirtGrid[0][1][4] = 200;
        floorDirtGrid[1][0][3] = 150;

        expect(GetFloorDirt(2, 0, 0) == 100);
        expect(GetFloorDirt(4, 1, 0) == 200);
        expect(GetFloorDirt(3, 0, 1) == 150);

        ClearFloorDirt();

        expect(GetFloorDirt(2, 0, 0) == 0);
        expect(GetFloorDirt(4, 1, 0) == 0);
        expect(GetFloorDirt(3, 0, 1) == 0);
    }
}

// =============================================================================
// Dirt Source Detection
// =============================================================================

describe(floordirt_dirt_source) {
    it("should detect natural dirt as dirt source") {
        InitGridWithSizeAndChunkSize(8, 4, 8, 4);
        InitFloorDirt();

        grid[0][1][2] = CELL_WALL;
        SetWallMaterial(2, 1, 0, MAT_DIRT);
        SetWallNatural(2, 1, 0);

        expect(IsDirtSource(2, 1, 0) == true);
    }

    it("should detect soil types as dirt source") {
        InitGridWithSizeAndChunkSize(8, 4, 8, 4);
        InitFloorDirt();

        grid[0][0][0] = CELL_WALL; SetWallMaterial(0, 0, 0, MAT_CLAY); SetWallNatural(0, 0, 0);
        grid[0][0][1] = CELL_WALL; SetWallMaterial(1, 0, 0, MAT_SAND); SetWallNatural(1, 0, 0);
        grid[0][0][2] = CELL_WALL; SetWallMaterial(2, 0, 0, MAT_GRAVEL); SetWallNatural(2, 0, 0);
        grid[0][0][3] = CELL_WALL; SetWallMaterial(3, 0, 0, MAT_PEAT); SetWallNatural(3, 0, 0);

        expect(IsDirtSource(0, 0, 0) == true);
        expect(IsDirtSource(1, 0, 0) == true);
        expect(IsDirtSource(2, 0, 0) == true);
        expect(IsDirtSource(3, 0, 0) == true);
    }

    it("should detect dirt at z-1 as source when walking at z") {
        InitGridWithSizeAndChunkSize(8, 4, 8, 4);
        InitFloorDirt();

        // Dirt at z=0, air at z=1 (movers walk at z=1)
        grid[0][1][2] = CELL_WALL;
        SetWallMaterial(2, 1, 0, MAT_DIRT);
        SetWallNatural(2, 1, 0);
        grid[1][1][2] = CELL_AIR;

        expect(IsDirtSource(2, 1, 1) == true);
    }

    it("should not detect stone as dirt source") {
        InitGridWithSizeAndChunkSize(8, 4, 8, 4);
        InitFloorDirt();

        grid[0][1][2] = CELL_WALL;
        SetWallMaterial(2, 1, 0, MAT_GRANITE);
        SetWallNatural(2, 1, 0);

        expect(IsDirtSource(2, 1, 0) == false);
    }

    it("should not detect constructed wall as dirt source") {
        InitGridWithSizeAndChunkSize(8, 4, 8, 4);
        InitFloorDirt();

        grid[0][1][2] = CELL_WALL;
        SetWallMaterial(2, 1, 0, MAT_DIRT);
        // NOT calling SetWallNatural - this is constructed

        expect(IsDirtSource(2, 1, 0) == false);
    }

    it("should not detect air as dirt source") {
        InitGridWithSizeAndChunkSize(8, 4, 8, 4);
        InitFloorDirt();

        grid[0][1][2] = CELL_AIR;

        expect(IsDirtSource(2, 1, 0) == false);
    }
}

// =============================================================================
// Dirt Target Detection
// =============================================================================

describe(floordirt_dirt_target) {
    it("should detect constructed floor as dirt target") {
        InitGridWithSizeAndChunkSize(8, 4, 8, 4);
        InitFloorDirt();

        grid[1][1][2] = CELL_AIR;
        SET_FLOOR(2, 1, 1);

        expect(IsDirtTarget(2, 1, 1) == true);
    }

    it("should detect constructed wall top as dirt target") {
        InitGridWithSizeAndChunkSize(8, 4, 8, 4);
        InitFloorDirt();

        // Constructed (non-natural) wall at z=0, air at z=1
        grid[0][1][2] = CELL_WALL;
        SetWallMaterial(2, 1, 0, MAT_OAK);
        // NOT calling SetWallNatural - this is constructed
        grid[1][1][2] = CELL_AIR;

        expect(IsDirtTarget(2, 1, 1) == true);
    }

    it("should not detect natural wall top as dirt target") {
        InitGridWithSizeAndChunkSize(8, 4, 8, 4);
        InitFloorDirt();

        // Natural wall at z=0
        grid[0][1][2] = CELL_WALL;
        SetWallMaterial(2, 1, 0, MAT_GRANITE);
        SetWallNatural(2, 1, 0);
        grid[1][1][2] = CELL_AIR;

        expect(IsDirtTarget(2, 1, 1) == false);
    }

    it("should not detect air as dirt target") {
        InitGridWithSizeAndChunkSize(8, 4, 8, 4);
        InitFloorDirt();

        grid[1][1][2] = CELL_AIR;

        expect(IsDirtTarget(2, 1, 1) == false);
    }
}

// =============================================================================
// Dirt Tracking
// =============================================================================

describe(floordirt_tracking) {
    it("should track dirt from dirt to floor") {
        InitGridWithSizeAndChunkSize(8, 4, 8, 4);
        InitFloorDirt();
        InitGroundWear();
        floorDirtEnabled = true;
        dirtActiveCells = 0;

        // Dirt at (2,1) z=0, floor at (3,1) z=1
        grid[0][1][2] = CELL_WALL; SetWallMaterial(2, 1, 0, MAT_DIRT); SetWallNatural(2, 1, 0);
        grid[0][1][3] = CELL_WALL; SetWallMaterial(3, 1, 0, MAT_OAK); // constructed
        grid[1][1][2] = CELL_AIR;
        grid[1][1][3] = CELL_AIR;

        expect(GetFloorDirt(3, 1, 1) == 0);

        // Simulate mover stepping from dirt z=1 to constructed wall-top z=1
        MoverTrackDirt(0, 2, 1, 1);  // First call sets prev
        MoverTrackDirt(0, 3, 1, 1);  // Second call tracks dirt

        expect(GetFloorDirt(3, 1, 1) == DIRT_TRACK_AMOUNT);
        expect(dirtActiveCells == 1);
    }

    it("should not track dirt from floor to floor") {
        InitGridWithSizeAndChunkSize(8, 4, 8, 4);
        InitFloorDirt();
        floorDirtEnabled = true;
        dirtActiveCells = 0;

        // Two constructed walls at z=0
        grid[0][1][2] = CELL_WALL; SetWallMaterial(2, 1, 0, MAT_OAK); // constructed
        grid[0][1][3] = CELL_WALL; SetWallMaterial(3, 1, 0, MAT_OAK); // constructed
        grid[1][1][2] = CELL_AIR;
        grid[1][1][3] = CELL_AIR;

        MoverTrackDirt(0, 2, 1, 1);
        MoverTrackDirt(0, 3, 1, 1);

        expect(GetFloorDirt(3, 1, 1) == 0);
        expect(dirtActiveCells == 0);
    }

    it("should not track dirt from air to floor") {
        InitGridWithSizeAndChunkSize(8, 4, 8, 4);
        InitFloorDirt();
        floorDirtEnabled = true;
        dirtActiveCells = 0;

        grid[0][1][3] = CELL_WALL; SetWallMaterial(3, 1, 0, MAT_OAK); // constructed
        grid[1][1][2] = CELL_AIR;
        grid[1][1][3] = CELL_AIR;

        MoverTrackDirt(0, 2, 1, 1);  // air
        MoverTrackDirt(0, 3, 1, 1);  // floor

        expect(GetFloorDirt(3, 1, 1) == 0);
    }

    it("should accumulate dirt over multiple steps") {
        InitGridWithSizeAndChunkSize(8, 4, 8, 4);
        InitFloorDirt();
        floorDirtEnabled = true;
        dirtActiveCells = 0;

        grid[0][1][2] = CELL_WALL; SetWallMaterial(2, 1, 0, MAT_DIRT); SetWallNatural(2, 1, 0);
        grid[0][1][3] = CELL_WALL; SetWallMaterial(3, 1, 0, MAT_OAK); // constructed
        grid[1][1][2] = CELL_AIR;
        grid[1][1][3] = CELL_AIR;

        // Walk back and forth 3 times
        MoverTrackDirt(0, 2, 1, 1);
        MoverTrackDirt(0, 3, 1, 1);  // dirt -> floor
        MoverTrackDirt(0, 2, 1, 1);
        MoverTrackDirt(0, 3, 1, 1);  // dirt -> floor
        MoverTrackDirt(0, 2, 1, 1);
        MoverTrackDirt(0, 3, 1, 1);  // dirt -> floor

        expect(GetFloorDirt(3, 1, 1) == DIRT_TRACK_AMOUNT * 3);
    }

    it("should cap dirt at DIRT_MAX") {
        InitGridWithSizeAndChunkSize(8, 4, 8, 4);
        InitFloorDirt();
        floorDirtEnabled = true;
        dirtActiveCells = 0;

        grid[0][1][2] = CELL_WALL; SetWallMaterial(2, 1, 0, MAT_DIRT); SetWallNatural(2, 1, 0);
        grid[0][1][3] = CELL_WALL; SetWallMaterial(3, 1, 0, MAT_OAK); // constructed
        grid[1][1][2] = CELL_AIR;
        grid[1][1][3] = CELL_AIR;

        // Set dirt close to max
        floorDirtGrid[1][1][3] = 254;

        MoverTrackDirt(0, 2, 1, 1);
        MoverTrackDirt(0, 3, 1, 1);

        expect(GetFloorDirt(3, 1, 1) == DIRT_MAX);
    }

    it("should not track when disabled") {
        InitGridWithSizeAndChunkSize(8, 4, 8, 4);
        InitFloorDirt();
        floorDirtEnabled = false;
        dirtActiveCells = 0;

        grid[0][1][2] = CELL_WALL; SetWallMaterial(2, 1, 0, MAT_DIRT); SetWallNatural(2, 1, 0);
        grid[0][1][3] = CELL_WALL; SetWallMaterial(3, 1, 0, MAT_OAK);
        grid[1][1][2] = CELL_AIR;
        grid[1][1][3] = CELL_AIR;

        MoverTrackDirt(0, 2, 1, 1);
        MoverTrackDirt(0, 3, 1, 1);

        expect(GetFloorDirt(3, 1, 1) == 0);

        floorDirtEnabled = true;  // Re-enable
    }

    it("should track with HAS_FLOOR target") {
        InitGridWithSizeAndChunkSize(8, 4, 8, 4);
        InitFloorDirt();
        floorDirtEnabled = true;
        dirtActiveCells = 0;

        // Dirt at z=0, floor flag at z=1
        grid[0][1][2] = CELL_WALL; SetWallMaterial(2, 1, 0, MAT_DIRT); SetWallNatural(2, 1, 0);
        grid[1][1][2] = CELL_AIR;
        grid[1][1][3] = CELL_AIR;
        SET_FLOOR(3, 1, 1);
        SetFloorMaterial(3, 1, 1, MAT_OAK);

        MoverTrackDirt(0, 2, 1, 1);
        MoverTrackDirt(0, 3, 1, 1);

        expect(GetFloorDirt(3, 1, 1) == DIRT_TRACK_AMOUNT);
    }

    it("should apply stone floor multiplier") {
        InitGridWithSizeAndChunkSize(8, 4, 8, 4);
        InitFloorDirt();
        floorDirtEnabled = true;
        dirtActiveCells = 0;

        // Dirt at z=0, stone floor at z=1
        grid[0][1][2] = CELL_WALL; SetWallMaterial(2, 1, 0, MAT_DIRT); SetWallNatural(2, 1, 0);
        grid[1][1][2] = CELL_AIR;
        grid[1][1][3] = CELL_AIR;
        SET_FLOOR(3, 1, 1);
        SetFloorMaterial(3, 1, 1, MAT_GRANITE);

        MoverTrackDirt(0, 2, 1, 1);
        MoverTrackDirt(0, 3, 1, 1);

        // Stone floors get 50% rate: (2 * 50) / 100 = 1
        int expected = (DIRT_TRACK_AMOUNT * DIRT_STONE_MULTIPLIER) / 100;
        if (expected < 1) expected = 1;
        expect(GetFloorDirt(3, 1, 1) == expected);
    }
}

// =============================================================================
// Cleaning
// =============================================================================

describe(floordirt_cleaning) {
    it("should reduce dirt when CleanFloorDirt called") {
        InitGridWithSizeAndChunkSize(8, 4, 8, 4);
        InitFloorDirt();
        dirtActiveCells = 0;

        SetFloorDirt(2, 1, 0, 100);
        expect(GetFloorDirt(2, 1, 0) == 100);

        int remaining = CleanFloorDirt(2, 1, 0, 50);
        expect(remaining == 50);
        expect(GetFloorDirt(2, 1, 0) == 50);
    }

    it("should clamp dirt to zero when overcleaning") {
        InitGridWithSizeAndChunkSize(8, 4, 8, 4);
        InitFloorDirt();
        dirtActiveCells = 0;

        SetFloorDirt(2, 1, 0, 30);

        int remaining = CleanFloorDirt(2, 1, 0, 100);
        expect(remaining == 0);
        expect(GetFloorDirt(2, 1, 0) == 0);
    }

    it("should update dirtActiveCells when cleaning to zero") {
        InitGridWithSizeAndChunkSize(8, 4, 8, 4);
        InitFloorDirt();
        dirtActiveCells = 0;

        SetFloorDirt(2, 1, 0, 50);
        expect(dirtActiveCells == 1);

        CleanFloorDirt(2, 1, 0, 50);
        expect(dirtActiveCells == 0);
    }
}

// =============================================================================
// dirtActiveCells Counter
// =============================================================================

describe(floordirt_active_cells) {
    it("should increment when dirt goes from 0 to positive") {
        InitGridWithSizeAndChunkSize(8, 4, 8, 4);
        InitFloorDirt();
        dirtActiveCells = 0;

        expect(dirtActiveCells == 0);
        SetFloorDirt(2, 1, 0, 10);
        expect(dirtActiveCells == 1);
        SetFloorDirt(3, 1, 0, 20);
        expect(dirtActiveCells == 2);
    }

    it("should decrement when dirt goes to zero") {
        InitGridWithSizeAndChunkSize(8, 4, 8, 4);
        InitFloorDirt();
        dirtActiveCells = 0;

        SetFloorDirt(2, 1, 0, 10);
        SetFloorDirt(3, 1, 0, 20);
        expect(dirtActiveCells == 2);

        SetFloorDirt(2, 1, 0, 0);
        expect(dirtActiveCells == 1);

        SetFloorDirt(3, 1, 0, 0);
        expect(dirtActiveCells == 0);
    }

    it("should not change when overwriting nonzero with nonzero") {
        InitGridWithSizeAndChunkSize(8, 4, 8, 4);
        InitFloorDirt();
        dirtActiveCells = 0;

        SetFloorDirt(2, 1, 0, 10);
        expect(dirtActiveCells == 1);

        SetFloorDirt(2, 1, 0, 50);
        expect(dirtActiveCells == 1);
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

describe(floordirt_edge_cases) {
    it("should handle out-of-bounds queries gracefully") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitFloorDirt();

        expect(GetFloorDirt(-1, 0, 0) == 0);
        expect(GetFloorDirt(100, 0, 0) == 0);
        expect(GetFloorDirt(0, -1, 0) == 0);
        expect(GetFloorDirt(0, 100, 0) == 0);
        expect(GetFloorDirt(0, 0, -1) == 0);
        expect(GetFloorDirt(0, 0, 100) == 0);
    }

    it("should handle out-of-bounds MoverTrackDirt gracefully") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitFloorDirt();
        floorDirtEnabled = true;

        // Should not crash
        MoverTrackDirt(-1, 0, 0, 0);
        MoverTrackDirt(0, -1, 0, 0);
        MoverTrackDirt(0, 100, 0, 0);

        expect(true);
    }

    it("should handle MoverTrackDirt with invalid mover index") {
        InitGridFromAsciiWithChunkSize(
            "....\n", 4, 1);
        InitFloorDirt();
        floorDirtEnabled = true;

        // Should not crash with bad indices
        MoverTrackDirt(-1, 0, 0, 0);
        MoverTrackDirt(99999, 0, 0, 0);

        expect(true);
    }

    it("should work at different z-levels") {
        InitGridWithSizeAndChunkSize(8, 4, 8, 4);
        InitFloorDirt();
        dirtActiveCells = 0;

        SetFloorDirt(2, 1, 0, 10);
        SetFloorDirt(2, 1, 1, 20);
        SetFloorDirt(2, 1, 2, 30);

        expect(GetFloorDirt(2, 1, 0) == 10);
        expect(GetFloorDirt(2, 1, 1) == 20);
        expect(GetFloorDirt(2, 1, 2) == 30);
        expect(dirtActiveCells == 3);
    }
}

// =============================================================================
// Per-Mover Tracking
// =============================================================================

describe(floordirt_per_mover) {
    it("should track previous cell per mover independently") {
        InitGridWithSizeAndChunkSize(8, 4, 8, 4);
        InitFloorDirt();
        floorDirtEnabled = true;
        dirtActiveCells = 0;

        // Dirt at z=0, floor at z=1
        grid[0][1][2] = CELL_WALL; SetWallMaterial(2, 1, 0, MAT_DIRT); SetWallNatural(2, 1, 0);
        grid[0][1][4] = CELL_WALL; SetWallMaterial(4, 1, 0, MAT_DIRT); SetWallNatural(4, 1, 0);
        grid[0][1][3] = CELL_WALL; SetWallMaterial(3, 1, 0, MAT_OAK); // constructed
        grid[0][1][5] = CELL_WALL; SetWallMaterial(5, 1, 0, MAT_OAK); // constructed
        grid[1][1][2] = CELL_AIR;
        grid[1][1][3] = CELL_AIR;
        grid[1][1][4] = CELL_AIR;
        grid[1][1][5] = CELL_AIR;

        // Mover 0 walks dirt->floor at (3,1)
        MoverTrackDirt(0, 2, 1, 1);
        MoverTrackDirt(0, 3, 1, 1);

        // Mover 1 walks dirt->floor at (5,1)
        MoverTrackDirt(1, 4, 1, 1);
        MoverTrackDirt(1, 5, 1, 1);

        expect(GetFloorDirt(3, 1, 1) == DIRT_TRACK_AMOUNT);
        expect(GetFloorDirt(5, 1, 1) == DIRT_TRACK_AMOUNT);
    }

    it("should not track on first call (no previous cell)") {
        InitGridWithSizeAndChunkSize(8, 4, 8, 4);
        InitFloorDirt();
        floorDirtEnabled = true;
        dirtActiveCells = 0;

        grid[0][1][3] = CELL_WALL; SetWallMaterial(3, 1, 0, MAT_OAK);
        grid[1][1][3] = CELL_AIR;

        // First call for mover 0 - should not track (no prev cell)
        MoverTrackDirt(0, 3, 1, 1);

        expect(GetFloorDirt(3, 1, 1) == 0);
    }

    it("should not track when staying in same cell") {
        InitGridWithSizeAndChunkSize(8, 4, 8, 4);
        InitFloorDirt();
        floorDirtEnabled = true;
        dirtActiveCells = 0;

        grid[0][1][2] = CELL_WALL; SetWallMaterial(2, 1, 0, MAT_DIRT); SetWallNatural(2, 1, 0);
        grid[0][1][3] = CELL_WALL; SetWallMaterial(3, 1, 0, MAT_OAK);
        grid[1][1][2] = CELL_AIR;
        grid[1][1][3] = CELL_AIR;

        MoverTrackDirt(0, 2, 1, 1);
        // Staying in same cell
        MoverTrackDirt(0, 2, 1, 1);
        MoverTrackDirt(0, 2, 1, 1);

        // Now step to floor
        MoverTrackDirt(0, 3, 1, 1);

        // Should only have tracked once (the actual cell transition)
        expect(GetFloorDirt(3, 1, 1) == DIRT_TRACK_AMOUNT);
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

    test(floordirt_initialization);
    test(floordirt_dirt_source);
    test(floordirt_dirt_target);
    test(floordirt_tracking);
    test(floordirt_cleaning);
    test(floordirt_active_cells);
    test(floordirt_edge_cases);
    test(floordirt_per_mover);

    return summary();
}
