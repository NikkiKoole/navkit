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
#include "../src/entities/items.h"
#include "../src/entities/jobs.h"
#include "../src/simulation/balance.h"
#include "../src/simulation/needs.h"
#include "../src/entities/stockpiles.h"
#include "../src/entities/workshops.h"
#include "../src/core/time.h"
#include "../src/core/describe.h"
#include "../src/core/selection.h"
#include "../src/simulation/farming.h"
#include "../src/world/designations.h"
#include <stdlib.h>
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
    ClearItems();
    ClearStockpiles();
    ClearWorkshops();
    ClearTown();
}

static int CountFurnitureIn(int x1, int y1, int x2, int y2) {
    int n = 0;
    for (int i = 0; i < furnitureHighWater; i++)
        if (furniture[i].active && furniture[i].x >= x1 && furniture[i].x <= x2 && furniture[i].y >= y1 && furniture[i].y <= y2) n++;
    return n;
}

static int CountFurnitureType(FurnitureType t) {
    int n = 0;
    for (int i = 0; i < furnitureHighWater; i++) n += furniture[i].active && furniture[i].type == t;
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
    it("a work lot (bakery) gets work spots (stoves)") {
        SetupTown();
        TownAddRoad(0, 10, 39, 12, 1, NULL);
        int lot = TownAddLot(LOT_WORK, 2, 13, 12, 19, 1, NULL);
        expect(lot >= 0);
        expect(townLots[lot].beds >= 3);
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

// A street from the west edge, one home lot on it with `beds` beds wanted (lot size picks it)
static int SetupStreetWithHome(void) {
    SetupTown();
    ClearItems();
    ClearStockpiles();
    ClearWorkshops();
    ClearJobs();
    InitBalance();
    TownAddRoad(0, 10, 39, 12, 1, NULL);
    int lot = TownAddLot(LOT_HOME, 10, 13, 16, 17, 1, NULL);   // 7x5: inside 5x2 = 1 bed
    BuildEntrances();
    BuildGraph();
    return lot;
}

describe(town_residents) {
    it("a free bed draws a newcomer from the edge road, who gets that home") {
        int lot = SetupStreetWithHome();
        expect(lot >= 0);
        expect(townLots[lot].beds == 1);
        expect(TownFreeBeds() == 1);
        townArrivalGH = 0.05f;
        for (int t = 0; t < 600; t++) TownTick(1.0f);   // 10 game minutes in 1 s steps
        expect(moverCount == 1);
        expect(TownHomeOf(0) == lot);
        expect(TownResidents(lot) == 1);
        expect(TownFreeBeds() == 0);
        expect((int)(movers[0].x / CELL_SIZE) == 0);    // arrived at the map edge
    }
    it("only one newcomer per free bed") {
        SetupStreetWithHome();
        townArrivalGH = 0.05f;
        for (int t = 0; t < 6000; t++) TownTick(1.0f);
        expect(moverCount == 1);
    }
    it("someone else's bed is not for resting; a public bed is") {
        int lot = SetupStreetWithHome();
        townArrivalGH = 0.05f;
        for (int t = 0; t < 600; t++) TownTick(1.0f);
        int bed = -1;
        for (int i = 0; i < townLots[lot].furnCount; i++)
            if (furniture[townLots[lot].furn[i]].type == FURNITURE_PLANK_BED) bed = townLots[lot].furn[i];
        expect(bed >= 0);
        expect(TownMayUseFurniture(bed, 0));
        expect(!TownMayUseFurniture(bed, 1));
        int pub = SpawnFurniture(30, 30, 1, FURNITURE_PLANK_BED, MAT_OAK);
        expect(TownMayUseFurniture(pub, 1));
    }
    it("a home's fridge is a stockpile for bread only") {
        int lot = SetupStreetWithHome();
        int sp = townLots[lot].stockpile;
        expect(sp >= 0 && stockpiles[sp].active);
        const Furniture* f = &furniture[townLots[lot].fridge];
        expect(stockpiles[sp].x == f->x && stockpiles[sp].y == f->y);
        expect(stockpiles[sp].allowedTypes[ITEM_BREAD]);
        expect(!stockpiles[sp].allowedTypes[ITEM_FLOUR]);
        TownRemoveLot(lot);
        expect(!stockpiles[sp].active);
    }
    it("removing a home makes its residents homeless") {
        int lot = SetupStreetWithHome();
        townArrivalGH = 0.05f;
        for (int t = 0; t < 600; t++) TownTick(1.0f);
        TownRemoveLot(lot);
        expect(TownHomeOf(0) == -1);
        expect(TownHomeless() == 1);
    }
}

describe(town_bakery) {
    it("a bakery gets stoves that bake until the pantries are stocked, each with a counter within 2") {
        SetupTown();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        TownAddRoad(0, 10, 39, 12, 1, NULL);
        int lot = TownAddLot(LOT_WORK, 2, 13, 12, 18, 1, NULL);
        expect(lot >= 0);
        const TownLot* L = &townLots[lot];
        int stoves = 0, counters = 0;
        for (int i = 0; i < L->wsCount; i++) {
            const Workshop* w = &workshops[L->ws[i]];
            if (w->type == WORKSHOP_COUNTER) counters++;
            if (w->type != WORKSHOP_STOVE) continue;
            stoves++;
            expect(w->billCount == 1 && w->bills[0].mode == BILL_DO_UNTIL_X && w->bills[0].targetCount > 0);
            expect(strcmp(stoveRecipes[w->bills[0].recipeIdx].name, "Bake Loaves") == 0);
            bool near = false;
            for (int j = 0; j < L->wsCount; j++) {
                const Workshop* c = &workshops[L->ws[j]];
                if (c->type == WORKSHOP_COUNTER && abs(c->x - w->x) <= 2 && abs(c->y - w->y) <= 2) near = true;
            }
            expect(near);
        }
        expect(stoves >= 2);
        expect(counters >= stoves - 1);
        expect(L->beds == stoves);
    }
    it("flour is imported only when the whole town has run out, and counted") {
        SetupTown();
        TownAddRoad(0, 10, 39, 12, 1, NULL);
        int lot = TownAddLot(LOT_WORK, 2, 13, 12, 18, 1, NULL);
        townFlourDelivery = 20;
        townArrivalGH = 24.0f;
        for (int t = 0; t < 60; t++) TownTick(1.0f);
        int fl = townLots[lot].supplyItem;
        expect(fl >= 0 && items[fl].type == ITEM_FLOUR && items[fl].stackCount == 20);
        expect((int)(items[fl].x / CELL_SIZE) == townLots[lot].supplyX);
        expect(townImported[ITEM_FLOUR] == 20);
        items[fl].stackCount = 5;                      // used, but the town still has 5: no import
        for (int t = 0; t < 60; t++) TownTick(1.0f);
        expect(townImported[ITEM_FLOUR] == 20);
        items[fl].stackCount = 2;                      // a stack too small for a batch (4) ...
        int elsewhere = SpawnItem(35 * CELL_SIZE, 35 * CELL_SIZE, 1, ITEM_FLOUR);   // ... but a usable one across town
        items[elsewhere].stackCount = 5;
        for (int t = 0; t < 60; t++) TownTick(1.0f);
        expect(townImported[ITEM_FLOUR] == 20);
        items[elsewhere].stackCount = 3;               // two stacks (2 + 3), neither enough for a batch: import
        for (int t = 0; t < 60; t++) TownTick(1.0f);
        expect(townImported[ITEM_FLOUR] == 40);
    }
    it("removing a bakery removes its stoves and counters") {
        SetupTown();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        TownAddRoad(0, 10, 39, 12, 1, NULL);
        int lot = TownAddLot(LOT_WORK, 2, 13, 12, 18, 1, NULL);
        int ws0 = townLots[lot].ws[0];
        expect(workshops[ws0].active);
        TownRemoveLot(lot);
        expect(!workshops[ws0].active);
        int active = 0;
        for (int i = 0; i < MAX_WORKSHOPS; i++) active += workshops[i].active;
        expect(active == 0);
    }
}

describe(town_waste) {
    it("rotten food in a town lot is thrown out and counted; elsewhere it stays") {
        SetupTown();
        TownAddRoad(0, 10, 39, 12, 1, NULL);
        int lot = TownAddLot(LOT_WORK, 2, 13, 12, 18, 1, NULL);
        townFlourDelivery = 0;
        townArrivalGH = 24.0f;
        const TownLot* L = &townLots[lot];
        int in = SpawnItem((L->x1 + 3) * CELL_SIZE + 16, (L->y1 + 3) * CELL_SIZE + 16, 1, ITEM_BREAD);
        int out = SpawnItem(35 * CELL_SIZE + 16, 35 * CELL_SIZE + 16, 1, ITEM_BREAD);
        int fresh = SpawnItem((L->x1 + 4) * CELL_SIZE + 16, (L->y1 + 3) * CELL_SIZE + 16, 1, ITEM_BREAD);
        items[in].stackCount = 4;
        items[in].condition = CONDITION_ROTTEN;
        items[out].condition = CONDITION_ROTTEN;
        for (int t = 0; t < 60; t++) TownTick(1.0f);
        expect(!items[in].active);
        expect(items[out].active);
        expect(items[fresh].active);
        expect(townWasted[ITEM_BREAD] == 4);
    }
}

describe(town_grain) {
    it("a mill gets querns that grind until N flour is in store, a granary, a flour store, a miller per two querns") {
        SetupTown();
        TownAddRoad(0, 10, 39, 12, 1, NULL);
        int lot = TownAddLot(LOT_MILL, 2, 13, 9, 18, 1, NULL);
        expect(lot >= 0);
        const TownLot* L = &townLots[lot];
        int querns = 0;
        for (int i = 0; i < L->wsCount; i++) {
            const Workshop* w = &workshops[L->ws[i]];
            expect(w->type == WORKSHOP_QUERN);
            expect(w->billCount == 1 && w->bills[0].mode == BILL_DO_UNTIL_X);
            expect(strcmp(quernRecipes[w->bills[0].recipeIdx].name, "Mill Flour") == 0);
            expect(quernRecipes[w->bills[0].recipeIdx].inputCount == 8 && quernRecipes[w->bills[0].recipeIdx].outputCount == 4);
            querns++;
        }
        expect(querns >= 2 && L->beds == (querns + 1) / 2);   // a miller per two querns
        expect(L->storeCount == 2);
        const Stockpile* g = &stockpiles[L->stores[0]];
        expect(g->allowedTypes[ITEM_WHEAT] && !g->allowedTypes[ITEM_FLOUR]);
        const Stockpile* fs = &stockpiles[L->stores[1]];
        expect(fs->allowedTypes[ITEM_FLOUR] && !fs->allowedTypes[ITEM_WHEAT] && fs->priority < 7);
        expect(TownIsWorkplace(L->kind));
    }
    it("a farm marks its field for wheat, a barn row and a seed store with a seed per field cell") {
        SetupTown();
        TownAddRoad(0, 10, 39, 12, 1, NULL);
        int lot = TownAddLot(LOT_FARM, 2, 13, 8, 17, 1, NULL);   // 7x5 = 35 cells
        expect(lot >= 0);
        const TownLot* L = &townLots[lot];
        int marked = 0;
        for (int y = L->y1; y <= L->y2; y++)
            for (int x = L->x1; x <= L->x2; x++)
                if (HasFarmDesignation(x, y, 1)) {
                    marked++;
                    expect(FARM(x, y, 1).desiredCropType == CROP_WHEAT);
                }
        expect(marked == 28);   // every cell but the road-side row (seed store + barn)
        int seeds = 0;
        for (int i = 0; i < itemHighWaterMark; i++)
            if (items[i].active && items[i].type == ITEM_WHEAT_SEEDS) seeds += items[i].stackCount;
        expect(seeds == 28);
        expect(L->storeCount == 2 && stockpiles[L->stores[0]].allowedTypes[ITEM_WHEAT_SEEDS]);
        const Stockpile* barn = &stockpiles[L->stores[1]];
        expect(barn->allowedTypes[ITEM_WHEAT] && barn->width * barn->height == 4);   // the row minus seed store, tap, toilet
        expect(CountFurnitureType(FURNITURE_TAP) == 1 && CountFurnitureType(FURNITURE_TOILET) == 1);
        expect(GRID(L->x1 + 3, L->y1 + 2, 1) == CELL_AIR);   // no building on a farm
        expect(TownIsWorkplace(L->kind));
        expect(L->beds == 2);   // a farmhand per 24 field cells (28 cells)
    }
    it("weeds grow slower on a town farm, normal elsewhere") {
        SetupTown();
        TownAddRoad(0, 10, 39, 12, 1, NULL);
        int lot = TownAddLot(LOT_FARM, 2, 13, 8, 17, 1, NULL);
        townWeedRate = 0.2f;
        expect(TownWeedFactor(townLots[lot].x1 + 2, townLots[lot].y1 + 2, 1) == 0.2f);
        expect(TownWeedFactor(30, 30, 1) == 1.0f);
    }
    it("removing a farm takes its field back to grass") {
        SetupTown();
        TownAddRoad(0, 10, 39, 12, 1, NULL);
        int lot = TownAddLot(LOT_FARM, 2, 13, 8, 17, 1, NULL);
        int store = townLots[lot].stores[0], barn = townLots[lot].stores[1];
        TownRemoveLot(lot);
        expect(!stockpiles[barn].active);
        bool clean = true;
        for (int y = 13; y <= 17; y++)
            for (int x = 2; x <= 8; x++)
                if (HasFarmDesignation(x, y, 1) || FARM(x, y, 1).desiredCropType) clean = false;
        expect(clean);
        expect(!stockpiles[store].active);
    }
    it("a granary's floor stores wheat and flour, ranked between barn and mill, no jobs") {
        SetupTown();
        TownAddRoad(0, 10, 39, 12, 1, NULL);
        int lot = TownAddLot(LOT_GRANARY, 2, 13, 10, 18, 1, NULL);   // inside 7x3 (yard row + walls), aisle in the middle
        expect(lot >= 0);
        const TownLot* L = &townLots[lot];
        expect(L->storeCount == 2);
        int cells = 0;
        for (int i = 0; i < L->storeCount; i++) {
            const Stockpile* sp = &stockpiles[L->stores[i]];
            expect(sp->allowedTypes[ITEM_WHEAT] && sp->allowedTypes[ITEM_FLOUR] && !sp->allowedTypes[ITEM_BREAD]);
            expect(sp->priority == 5);
            cells += sp->width * sp->height;
        }
        expect(cells == 6 * 3);   // 7 wide minus the aisle, 3 deep
        expect(L->beds == 0 && !TownIsWorkplace(L->kind));
        expect(TownStoreCapacity() == cells * MAX_STACK_SIZE);
        int sp0 = L->stores[0];
        TownRemoveLot(lot);
        expect(!stockpiles[sp0].active);
    }
    it("a town farm's harvest is noted; harvests elsewhere are not") {
        SetupTown();
        TownAddRoad(0, 10, 39, 12, 1, NULL);
        int lot = TownAddLot(LOT_FARM, 2, 13, 8, 17, 1, NULL);
        expect(!TownMakesWheat());
        TownNoteHarvest(30, 30, 1);
        expect(!TownMakesWheat());
        TownNoteHarvest(townLots[lot].x1 + 2, townLots[lot].y1 + 2, 1);
        expect(TownMakesWheat());
    }
}

// A street, one 1-bed home and a bakery; time of day set by the caller
static int SetupHomeAndBakery(int* bakery) {
    int home = SetupStreetWithHome();
    *bakery = TownAddLot(LOT_WORK, 20, 13, 30, 18, 1, NULL);
    BuildEntrances();
    BuildGraph();
    townArrivalGH = 0.05f;
    for (int t = 0; t < 600; t++) TownTick(1.0f);   // the newcomer arrives, gets home and job
    return home;
}

describe(town_jobs) {
    it("a resident takes a job at the bakery, as many jobs as stoves") {
        int bakery;
        SetupHomeAndBakery(&bakery);
        expect(bakery >= 0);
        expect(TownJobOf(0) == bakery);
        expect(TownWorkers(bakery) == 1);
        expect(TownFreeJobs() == townLots[bakery].beds - 1);
        expect(TownUnemployed() == 0);
    }
    it("the bakery's stoves are for its workers while it is open; other workshops for anyone") {
        int bakery;
        SetupHomeAndBakery(&bakery);
        int stove = -1;
        for (int i = 0; i < townLots[bakery].wsCount; i++)
            if (workshops[townLots[bakery].ws[i]].type == WORKSHOP_STOVE) stove = townLots[bakery].ws[i];
        expect(stove >= 0);
        townOpenHour = 7; townCloseHour = 17;
        timeOfDay = 12.0f;
        expect(TownMayCraftAt(stove, 0));
        expect(!TownMayCraftAt(stove, 5));          // not a worker there
        timeOfDay = 20.0f;
        expect(!TownMayCraftAt(stove, 0));          // closed
        townOpenHour = 22; townCloseHour = 6;       // open through midnight
        timeOfDay = 23.0f;
        expect(TownMayCraftAt(stove, 0));
        timeOfDay = 12.0f;
        expect(!TownMayCraftAt(stove, 0));
        townOpenHour = 7; townCloseHour = 17;
        int other = CreateWorkshop(35, 35, 1, WORKSHOP_STOVE);
        expect(other >= 0 && TownMayCraftAt(other, 5));
    }
    it("the bakery bakes loaves in batches") {
        int bakery;
        SetupHomeAndBakery(&bakery);
        const Workshop* w = &workshops[townLots[bakery].ws[0]];
        const Recipe* r = &stoveRecipes[w->bills[0].recipeIdx];
        expect(strcmp(r->name, "Bake Loaves") == 0);
        expect(r->outputType == ITEM_BREAD && r->outputCount == 4 && r->inputCount == 4);
    }
    it("a newcomer brings groceries for the pantry") {
        townGroceries = 2;
        int bakery;
        int home = SetupHomeAndBakery(&bakery);
        const Furniture* f = &furniture[townLots[home].fridge];
        int bread = 0;
        for (int i = 0; i < itemHighWaterMark; i++)
            if (items[i].active && items[i].type == ITEM_BREAD && (int)(items[i].x / CELL_SIZE) == f->x
                && (int)(items[i].y / CELL_SIZE) == f->y) bread += items[i].stackCount;
        expect(bread == 2);
    }
    it("closing the bakery puts its workers out of work") {
        int bakery;
        SetupHomeAndBakery(&bakery);
        TownRemoveLot(bakery);
        expect(TownJobOf(0) == -1);
        expect(TownUnemployed() == 1);
    }
}

static bool DescHas(const DescLines* d, const char* text) {
    for (int i = 0; i < d->count; i++) if (strstr(d->lines[i].text, text)) return true;
    return false;
}

describe(town_inspector) {
    it("a click inside a lot selects the lot, even on its stove or store") {
        SetupTown();
        TownAddRoad(0, 10, 39, 12, 1, NULL);
        int lot = TownAddLot(LOT_WORK, 2, 13, 12, 18, 1, NULL);
        const Workshop* w = &workshops[townLots[lot].ws[0]];
        Selection sel = PickSelectionAt(w->x * CELL_SIZE + 16, w->y * CELL_SIZE + 16, 1);
        expect(sel.kind == SEL_LOT && sel.idx == lot);
        sel = PickSelectionAt(30 * CELL_SIZE + 16, 30 * CELL_SIZE + 16, 1);   // open ground
        expect(sel.kind == SEL_NONE);
    }
    it("describes a home: beds, who lives there, the pantry") {
        int lot = SetupStreetWithHome();
        townArrivalGH = 0.05f;
        for (int t = 0; t < 600; t++) TownTick(1.0f);
        DescLines d;
        DescribeLot(lot, &d);
        expect(DescHas(&d, "Home #"));
        expect(DescHas(&d, "Beds: 1 (0 free)"));
        expect(DescHas(&d, "Pantry:"));
        expect(d.count >= 5);   // title, size, beds, one resident, pantry
    }
    it("describes a bakery, a mill, a farm and a granary") {
        SetupTown();
        TownAddRoad(0, 10, 39, 12, 1, NULL);
        int bakery = TownAddLot(LOT_WORK, 2, 13, 12, 18, 1, NULL);
        int mill = TownAddLot(LOT_MILL, 14, 13, 20, 18, 1, NULL);
        int farm = TownAddLot(LOT_FARM, 22, 13, 28, 17, 1, NULL);
        int granary = TownAddLot(LOT_GRANARY, 2, 0, 10, 9, 1, NULL);
        DescLines d;
        DescribeLot(bakery, &d);
        expect(DescHas(&d, "Bakery #") && DescHas(&d, "Jobs: 0 of") && DescHas(&d, "Flour in store"));
        DescribeLot(mill, &d);
        expect(DescHas(&d, "Mill #") && DescHas(&d, "Wheat in the granary"));
        DescribeLot(farm, &d);
        expect(DescHas(&d, "Farm #") && DescHas(&d, "Field: 28 cells") && DescHas(&d, "Seed: 28"));
        DescribeLot(granary, &d);
        expect(DescHas(&d, "Granary #") && DescHas(&d, "Holds 0 wheat, 0 flour"));
        char line[256];
        DescribeTownOverview(line, sizeof(line));
        expect(strstr(line, "people") != NULL && strstr(line, "imported") != NULL);
    }
}

int main(int argc, char* argv[]) {
    test_verbose = c89spec_parse_args(argc, argv);
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);
    test(town_roads);
    test(town_lots);
    test(town_residents);
    test(town_bakery);
    test(town_jobs);
    test(town_grain);
    test(town_waste);
    test(town_inspector);
    return summary();
}
