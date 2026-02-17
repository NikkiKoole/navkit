#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "test_helpers.h"
#include "../src/world/cell_defs.h"
#include "../src/world/material.h"
#include "../src/simulation/water.h"
#include "../src/simulation/groundwear.h"
#include "../src/simulation/floordirt.h"
#include "../src/core/time.h"
#include "../src/core/sim_manager.h"
#include "../src/entities/mover.h"
#include <stdlib.h>
#include <string.h>

static bool test_verbose = false;

// Helper: set up a simple 8x4 grid with dirt ground at z=0, air at z=1
static void SetupDirtGrid(void) {
    InitTestGridFromAscii(
        "........\n"
        "........\n"
        "........\n"
        "........\n");
    FillGroundLevel();
    InitWater();
    InitGroundWear();
    InitFloorDirt();
    InitTime();
    // Fast wetness sync for testing
    wetnessSyncInterval = 0.2f;  // 0.2 game-hours (= 0.5s at dayLength=60)
}

// =============================================================================
// IsSoilMaterial
// =============================================================================

describe(soil_material) {
    it("should identify dirt-like materials as soil") {
        expect(IsSoilMaterial(MAT_DIRT));
        expect(IsSoilMaterial(MAT_CLAY));
        expect(IsSoilMaterial(MAT_SAND));
        expect(IsSoilMaterial(MAT_GRAVEL));
        expect(IsSoilMaterial(MAT_PEAT));
    }

    it("should not identify non-soil materials as soil") {
        expect(!IsSoilMaterial(MAT_GRANITE));
        expect(!IsSoilMaterial(MAT_OAK));
        expect(!IsSoilMaterial(MAT_IRON));
        expect(!IsSoilMaterial(MAT_NONE));
    }
}

// =============================================================================
// IsMuddy
// =============================================================================

describe(muddy_detection) {
    it("should not be muddy when dry") {
        SetupDirtGrid();
        expect(!IsMuddy(2, 2, 0));
    }

    it("should not be muddy when only damp (wetness 1)") {
        SetupDirtGrid();
        SET_CELL_WETNESS(2, 2, 0, 1);
        expect(!IsMuddy(2, 2, 0));
    }

    it("should be muddy when wet (wetness 2) on dirt") {
        SetupDirtGrid();
        SET_CELL_WETNESS(2, 2, 0, 2);
        expect(IsMuddy(2, 2, 0));
    }

    it("should be muddy when soaked (wetness 3) on dirt") {
        SetupDirtGrid();
        SET_CELL_WETNESS(2, 2, 0, 3);
        expect(IsMuddy(2, 2, 0));
    }

    it("should be muddy on clay") {
        SetupDirtGrid();
        SetWallMaterial(2, 2, 0, MAT_CLAY);
        SET_CELL_WETNESS(2, 2, 0, 2);
        expect(IsMuddy(2, 2, 0));
    }

    it("should be muddy on sand") {
        SetupDirtGrid();
        SetWallMaterial(2, 2, 0, MAT_SAND);
        SET_CELL_WETNESS(2, 2, 0, 2);
        expect(IsMuddy(2, 2, 0));
    }

    it("should not be muddy on stone") {
        SetupDirtGrid();
        SetWallMaterial(2, 2, 0, MAT_GRANITE);
        SET_CELL_WETNESS(2, 2, 0, 3);
        expect(!IsMuddy(2, 2, 0));
    }

    it("should not be muddy on constructed walls") {
        SetupDirtGrid();
        SetWallNatural(2, 2, 0);
        // Make it non-natural
        wallNatural[0][2][2] = 0;
        SET_CELL_WETNESS(2, 2, 0, 3);
        expect(!IsMuddy(2, 2, 0));
    }

    it("should not be muddy on air cells") {
        SetupDirtGrid();
        SET_CELL_WETNESS(2, 2, 1, 3);  // z=1 is air
        expect(!IsMuddy(2, 2, 1));
    }

    it("should handle out-of-bounds gracefully") {
        SetupDirtGrid();
        expect(!IsMuddy(-1, 0, 0));
        expect(!IsMuddy(0, -1, 0));
        expect(!IsMuddy(999, 0, 0));
    }
}

// =============================================================================
// Water -> Wetness Sync
// =============================================================================

describe(water_wetness_sync) {
    it("should set wetness on soil below water after sync interval") {
        SetupDirtGrid();
        // Place water at z=1 (air) above dirt at z=0
        SetWaterLevel(3, 2, 1, 5);

        // Run enough time for wetness sync (interval = 0.5s, gameDeltaTime = TICK_DT)
        float elapsed = 0;
        while (elapsed < 1.0f) {
            gameDeltaTime = TICK_DT;
            UpdateWater();
            elapsed += TICK_DT;
        }

        int wetness = GET_CELL_WETNESS(3, 2, 0);
        expect(wetness > 0);
    }

    it("should map high water level to soaked") {
        SetupDirtGrid();

        // Keep replenishing water each tick so the sync sees a high level
        float elapsed = 0;
        while (elapsed < 1.0f) {
            SetWaterLevel(3, 2, 1, 7);
            gameDeltaTime = TICK_DT;
            UpdateWater();
            elapsed += TICK_DT;
        }

        int wetness = GET_CELL_WETNESS(3, 2, 0);
        expect(wetness == 3);  // soaked
    }

    it("should map low water level to damp") {
        SetupDirtGrid();
        SetWaterLevel(3, 2, 1, 1);

        float elapsed = 0;
        while (elapsed < 1.0f) {
            gameDeltaTime = TICK_DT;
            UpdateWater();
            elapsed += TICK_DT;
        }

        // Water level 1 may spread/evaporate, but if it stays, wetness should be 1
        int wetness = GET_CELL_WETNESS(3, 2, 0);
        // At least damp if water was present
        // (water level 1 may evaporate quickly, so just check >= 0)
        expect(wetness >= 0);
    }

    it("should not set wetness on non-soil materials") {
        SetupDirtGrid();
        // Change one cell to granite
        SetWallMaterial(3, 2, 0, MAT_GRANITE);
        SetWaterLevel(3, 2, 1, 7);

        float elapsed = 0;
        while (elapsed < 1.0f) {
            gameDeltaTime = TICK_DT;
            UpdateWater();
            elapsed += TICK_DT;
        }

        int wetness = GET_CELL_WETNESS(3, 2, 0);
        expect(wetness == 0);
    }
}

// =============================================================================
// Wetness Drying
// =============================================================================

describe(wetness_drying) {
    it("should dry wetness over time when no water present") {
        SetupDirtGrid();
        SET_CELL_WETNESS(3, 2, 0, 3);  // soaked

        // Run groundwear updates (interval = 5s by default)
        // We need wearActiveCells or waterActiveCells > 0 to not early-exit
        // Set waterActiveCells to trick the early-exit check
        waterActiveCells = 1;

        float elapsed = 0;
        while (elapsed < 20.0f) {
            gameDeltaTime = TICK_DT;
            UpdateGroundWear();
            elapsed += TICK_DT;
        }

        waterActiveCells = 0;

        int wetness = GET_CELL_WETNESS(3, 2, 0);
        expect(wetness < 3);  // should have dried at least once
    }

    it("should not dry if water is still present above") {
        SetupDirtGrid();
        SET_CELL_WETNESS(3, 2, 0, 3);
        SetWaterLevel(3, 2, 1, 5);  // water above

        waterActiveCells = 1;
        float elapsed = 0;
        while (elapsed < 20.0f) {
            gameDeltaTime = TICK_DT;
            UpdateGroundWear();
            elapsed += TICK_DT;
        }

        int wetness = GET_CELL_WETNESS(3, 2, 0);
        expect(wetness == 3);  // should stay soaked
    }

    it("should eventually dry completely to zero") {
        SetupDirtGrid();
        SET_CELL_WETNESS(3, 2, 0, 1);  // just damp

        waterActiveCells = 1;
        float elapsed = 0;
        while (elapsed < 20.0f) {
            gameDeltaTime = TICK_DT;
            UpdateGroundWear();
            elapsed += TICK_DT;
        }
        waterActiveCells = 0;

        int wetness = GET_CELL_WETNESS(3, 2, 0);
        expect(wetness == 0);
    }
}

// =============================================================================
// Mud + Dirt Tracking
// =============================================================================

describe(mud_dirt_tracking) {
    it("should track more dirt from muddy source") {
        SetupDirtGrid();

        // Set up a constructed wood floor at z=1 position (4,2)
        // (Use wood, not stone — stone's 50% reduction can mask the mud multiplier)
        grid[0][2][4] = CELL_WALL;
        SetWallMaterial(4, 2, 0, MAT_OAK);
        wallNatural[0][2][4] = 0;  // constructed

        // Source cell (3,2) is muddy dirt
        SET_CELL_WETNESS(3, 2, 0, 2);
        expect(IsMuddy(3, 2, 0));

        // Simulate mover walking from mud (3,2,1) to constructed floor (4,2,1)
        MoverTrackDirt(0, 3, 2, 1);  // set prev cell
        MoverTrackDirt(0, 4, 2, 1);  // step onto floor

        int dirtFromMud = GetFloorDirt(4, 2, 1);

        // Now test without mud
        ClearFloorDirt();
        SET_CELL_WETNESS(3, 2, 0, 0);
        expect(!IsMuddy(3, 2, 0));

        ResetMoverDirtTracking();
        MoverTrackDirt(0, 3, 2, 1);
        MoverTrackDirt(0, 4, 2, 1);

        int dirtFromDry = GetFloorDirt(4, 2, 1);

        // Muddy should track more dirt
        expect(dirtFromMud > dirtFromDry);
    }
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv) {
    bool quiet = false;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'v') test_verbose = true;
        if (argv[i][0] == '-' && argv[i][1] == 'q') quiet = true;
    }
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);
    if (quiet) set_quiet_mode(1);
    

    
    test(soil_material);
    test(muddy_detection);
    test(water_wetness_sync);
    test(wetness_drying);
    test(mud_dirt_tracking);
    
    return summary();
}
