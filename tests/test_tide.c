// Tide experiment (src/simulation/tide.c, docs/todo/world/tide.md)
#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/simulation/water.h"
#include "../src/simulation/tide.h"
#include "../src/core/time.h"
#include "test_helpers.h"

static bool test_verbose = false;

// 16x8: land x 0..4 (solid to z 2), flats x 5..15 (floor solid z 1, sea at z 2), a pond at
// (2,2,2) cut into the land (water, not connected to the edge: not sea)
static void TideTestWorld(void) {
    InitTestGrid(16, 8);
    ClearWater();
    ClearTide();
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++) {
            for (int z = 0; z < gridDepth; z++) GRID(x, y, z) = CELL_AIR;
            GRID(x, y, 0) = CELL_WALL;
            GRID(x, y, 1) = CELL_WALL;
            if (x <= 4) GRID(x, y, 2) = CELL_WALL;
        }
    GRID(2, 2, 2) = CELL_AIR;
    SetWaterLevel(2, 2, 2, WATER_MAX_LEVEL);
    for (int y = 0; y < gridHeight; y++)
        for (int x = 5; x < gridWidth; x++) SetWaterLevel(x, y, 2, WATER_MAX_LEVEL);
}

describe(tide) {
    it("marks the water connected to the map edge as sea, not a pond") {
        TideTestWorld();
        expect(TideMarkSea(2) == 11 * 8);
        expect(WATER(10, 3, 2).isSea);
        expect(!WATER(2, 2, 2).isSea);
    }

    it("is high at the start of the period, low half a period later") {
        tidePeriodHours = 12.0f;
        tideLowLevel = 0;
        expect(TideLevelAt(0.0f) == WATER_MAX_LEVEL);
        expect(TideLevelAt(6.0f) == 0);
        expect(TideLevelAt(12.0f) == WATER_MAX_LEVEL);
        tideLowLevel = 3;
        expect(TideLevelAt(6.0f) == 3);
        tideLowLevel = 0;
    }

    it("walks the sea level down a step per tick and the flats become walkable") {
        TideTestWorld();
        TideMarkSea(2);
        tideEnabled = true;
        tidePeriodHours = 12.0f;
        dayNumber = 0;
        timeOfDay = 6.0f;                      // low tide
        expect(!IsCellWalkableAt(2, 3, 10));   // deep sea
        for (int i = 0; i < 10; i++) TideTick();
        expect(tideLevel == 0);
        expect(tideSteps == WATER_MAX_LEVEL);
        expect(WATER(10, 3, 2).level == 0);
        expect(IsCellWalkableAt(2, 3, 10));    // dry flat
        expect(WATER(2, 2, 2).level == WATER_MAX_LEVEL);   // the pond keeps its water
        timeOfDay = 12.0f;                     // high again
        for (int i = 0; i < 10; i++) TideTick();
        expect(WATER(10, 3, 2).level == WATER_MAX_LEVEL);
        ClearTide();
        tidePeriodHours = 12.42f;
    }

    it("finds its sea again after a load (the isSea bits in the water cells)") {
        TideTestWorld();
        int marked = TideMarkSea(2);
        tideSeaCells = 0; tideSeaZ = -1;       // as after loading: the list is gone, the bits are not
        TideAfterLoad();
        expect(tideSeaCells == marked);
        expect(tideSeaZ == 2);
        expect(tideLevel == WATER_MAX_LEVEL);
        ClearWater();
        TideAfterLoad();                       // a save without a sea
        expect(tideSeaCells == 0 && tideSeaZ == -1);
    }

    it("does nothing when off") {
        TideTestWorld();
        TideMarkSea(2);
        tideEnabled = false;
        timeOfDay = 6.0f;
        TideTick();
        expect(WATER(10, 3, 2).level == WATER_MAX_LEVEL);
    }
}

int main(int argc, char* argv[]) {
    test_verbose = c89spec_parse_args(argc, argv);
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);
    test(tide);
    return summary();
}
