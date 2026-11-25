// Town (docs/vision/town.md): roads pave the ground; a lot along a road gets a building at once,
// its door facing the road, furniture inside; removing a lot or road puts the grass back.
#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"

static bool test_verbose = false;

#include "../src/game_state.h"
#include "../src/world/grid.h"
#include "../src/world/material.h"
#include "../src/world/cell_defs.h"
#include "../src/world/pathfinding.h"
#include "../src/entities/mover.h"
#include "../src/entities/furniture.h"
#include "../src/entities/town.h"
#include "test_helpers.h"
#include <string.h>

#define TW 40

static void SetupTown(void) {
    InitTestGrid(TW, TW);
    for (int y = 0; y < TW; y++)
        for (int x = 0; x < TW; x++) {
            GRID(x, y, 0) = CELL_WALL;
            SetWallMaterial(x, y, 0, MAT_DIRT);
            SetWallNatural(x, y, 0);
            for (int z = 1; z < gridDepth; z++) GRID(x, y, z) = CELL_AIR;
        }
    ClearMovers();
    ClearFurniture();
    ClearTown();
}

static int CountFurnitureIn(int x1, int y1, int x2, int y2) {
    int n = 0;
    for (int i = 0; i < MAX_FURNITURE; i++)
        if (furniture[i].active && furniture[i].x >= x1 && furniture[i].x <= x2 && furniture[i].y >= y1 && furniture[i].y <= y2) n++;
    return n;
}

static int CountFurnitureType(FurnitureType t) {
    int n = 0;
    for (int i = 0; i < MAX_FURNITURE; i++) n += furniture[i].active && furniture[i].type == t;
    return n;
}

describe(town_roads) {
    it("paves the ground under a road and knows its cells") {
        SetupTown();
        int r = TownAddRoad(0, 10, 20, 12, 1, NULL);
        expect(r == 0);
        expect(TownIsRoad(5, 11, 1));
        expect(!TownIsRoad(5, 13, 1));
        expect(GetWallMaterial(5, 11, 0) == MAT_CONCRETE);
        expect(IsCellWalkableAt(1, 11, 5));
    }
    it("removing a road puts the dirt back, but not under another road") {
        SetupTown();
        TownAddRoad(0, 10, 20, 12, 1, NULL);
        TownAddRoad(10, 0, 12, 30, 1, NULL);   // crosses the first
        expect(TownRemoveRoadsIn(2, 11, 2, 11, 1) == 1);
        expect(GetWallMaterial(2, 11, 0) == MAT_DIRT);
        expect(GetWallMaterial(11, 11, 0) == MAT_CONCRETE);   // the crossing: still the other road
        expect(townRoadCount == 1);
    }
}

describe(town_lots) {
    it("refuses a lot that is not along a road") {
        SetupTown();
        TownAddRoad(0, 10, 39, 12, 1, NULL);
        const char* why = NULL;
        expect(TownAddLot(LOT_HOME, 2, 20, 8, 26, 1, &why) < 0);
        expect(why != NULL);
        expect(TownAddLot(LOT_HOME, 2, 2, 3, 9, 1, &why) < 0);    // too small
        expect(TownAddLot(LOT_HOME, 2, 9, 8, 15, 1, &why) < 0);   // on the road
    }
    it("builds a home whose door faces the road, for every side") {
        // Road in the middle both ways; one lot on each side of it
        static const struct { int x1, y1, x2, y2; TownSide front; } lots[] = {
            { 22,  2, 28,  8, SIDE_W },   // east of the north-south road
            {  2, 22,  8, 28, SIDE_N },   // south of the east-west road
            { 22, 22, 28, 28, SIDE_N },   // also south (touches both roads: more road along N)
            {  2, 12,  8, 18, SIDE_S },   // north of the east-west road
        };
        for (int k = 0; k < 4; k++) {
            SetupTown();
            TownAddRoad(0, 19, 39, 21, 1, NULL);   // east-west
            TownAddRoad(19, 0, 21, 18, 1, NULL);   // north-south (north half)
            int lot = TownAddLot(LOT_HOME, lots[k].x1, lots[k].y1, lots[k].x2, lots[k].y2, 1, NULL);
            expect(lot >= 0);
            if (lot < 0) continue;
            const TownLot* L = &townLots[lot];
            expect(L->front == lots[k].front);
            expect(GRID(L->doorX, L->doorY, 1) == CELL_DOOR);
            expect(L->beds >= 1);
            expect(CountFurnitureType(FURNITURE_PLANK_BED) == L->beds);
            expect(CountFurnitureType(FURNITURE_TAP) == 1);
            // all furniture inside the lot, none on the door
            expect(CountFurnitureIn(L->x1, L->y1, L->x2, L->y2) == L->furnCount);
            expect(GetFurnitureAt(L->doorX, L->doorY, 1) < 0);
            // every bed reachable from the road
            BuildEntrances();
            BuildGraph();
            Point road = { L->front == SIDE_W ? 20 : 5, L->front == SIDE_W ? 5 : 20, 1 };
            for (int i = 0; i < L->furnCount; i++) {
                const Furniture* f = &furniture[L->furn[i]];
                if (f->type != FURNITURE_PLANK_BED) continue;
                Point bed = { f->x, f->y, 1 };
                expect(IsReachable(road, bed));
            }
        }
    }
    it("a work lot gets work spots (desks)") {
        SetupTown();
        TownAddRoad(0, 10, 39, 12, 1, NULL);
        int lot = TownAddLot(LOT_WORK, 2, 13, 12, 19, 1, NULL);
        expect(lot >= 0);
        expect(townLots[lot].beds >= 4);
        expect(CountFurnitureType(FURNITURE_DESK) == townLots[lot].beds);
        expect(TownLotCount(LOT_WORK) == 1);
    }
    it("removing a lot clears its building and furniture") {
        SetupTown();
        TownAddRoad(0, 10, 39, 12, 1, NULL);
        int lot = TownAddLot(LOT_HOME, 2, 13, 9, 19, 1, NULL);
        expect(lot >= 0);
        expect(TownRemoveLotsIn(5, 15, 5, 15, 1) == 1);
        expect(TownLotAt(5, 15, 1) < 0);
        expect(CountFurnitureIn(2, 13, 9, 19) == 0);
        bool clear = true;
        for (int y = 13; y <= 19; y++)
            for (int x = 2; x <= 9; x++)
                if (GRID(x, y, 1) != CELL_AIR || HAS_FLOOR(x, y, 1) || HAS_FLOOR(x, y, 2)) clear = false;
        expect(clear);
        // and the lot can be built again
        expect(TownAddLot(LOT_HOME, 2, 13, 9, 19, 1, NULL) >= 0);
    }
}

int main(int argc, char* argv[]) {
    test_verbose = c89spec_parse_args(argc, argv);
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);
    test(town_roads);
    test(town_lots);
    return summary();
}
