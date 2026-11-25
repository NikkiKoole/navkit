// Survival toilet ladder (docs/todo/gameplay/survival-toilet.md): bush < pit latrine < toilet.
// Toilets are found by CAP_TOILET and scored comfort / (1 + cells away); a latrine dirties its floor.
#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"

static bool test_verbose = false;

#include "../src/game_state.h"
#include "../src/world/grid.h"
#include "../src/world/material.h"
#include "../src/world/cell_defs.h"
#include "../src/world/construction.h"
#include "../src/entities/mover.h"
#include "../src/entities/items.h"
#include "../src/entities/jobs.h"
#include "../src/entities/stockpiles.h"
#include "../src/entities/containers.h"
#include "../src/entities/workshops.h"
#include "../src/entities/furniture.h"
#include "../src/simulation/needs.h"
#include "../src/simulation/balance.h"
#include "../src/simulation/floordirt.h"
#include "../src/simulation/rooms.h"
#include "../src/world/designations.h"
#include "../src/core/time.h"
#include "test_helpers.h"
#include <string.h>

static void SetupGround(void) {
    InitTestGrid(16, 16);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            GRID(x, y, 0) = CELL_WALL;
            SetWallMaterial(x, y, 0, MAT_DIRT);
            SetWallNatural(x, y, 0);
            GRID(x, y, 1) = CELL_AIR;
        }
    ClearMovers();
    ClearItems();
    ClearStockpiles();
    ClearWorkshops();
    ClearFurniture();
    ClearJobs();
    InitDesignations();
    InitRooms();
    InitFloorDirt();
    InitBalance();
    bladderEnabled = true;
    toiletsPlumbed = false;   // survival: no mains
    hungerEnabled = false;
    thirstEnabled = false;
    energyEnabled = false;
    gameDeltaTime = TICK_DT;
    gameSpeed = 1.0f;
    dayLength = 60.0f;
}

// A mover at (cx, cy) who needs to go now
static int SetupNeedyMover(int cx, int cy) {
    int idx = moverCount++;
    Point goal = {cx, cy, 1};
    InitMover(&movers[idx], cx * CELL_SIZE + CELL_SIZE * 0.5f, cy * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, goal, 100.0f);
    movers[idx].bladder = balance.bladderSeekThreshold * 0.5f;
    movers[idx].currentJobId = -1;
    movers[idx].needSearchCooldown = 0.0f;
    return idx;
}

// Deliver a blueprint's current stage with the first alternative of each input
static void FillStage(int bpIdx) {
    Blueprint* bp = &blueprints[bpIdx];
    const ConstructionStage* stage = &GetConstructionRecipe(bp->recipeIndex)->stages[bp->stage];
    for (int s = 0; s < stage->inputCount; s++)
        for (int i = 0; i < stage->inputs[s].count; i++) {
            ItemType t = stage->inputs[s].alternatives[0].itemType;
            int it = SpawnItemWithMaterial(bp->x * CELL_SIZE + CELL_SIZE * 0.5f, bp->y * CELL_SIZE + CELL_SIZE * 0.5f,
                                           (float)bp->z, t, DefaultMaterialForItemType(t));
            DeliverMaterialToBlueprint(bpIdx, it);
        }
}

static int WaterUnits(void) {
    int n = 0;
    for (int i = 0; i < itemHighWaterMark; i++)
        if (items[i].active && items[i].type == ITEM_WATER) n += items[i].stackCount;
    return n;
}

static void UseToiletUntilDone(int mi) {
    for (int i = 0; i < 2000 && movers[mi].bladder < 1.0f; i++) ProcessFreetimeNeeds();
}

describe(survival_toilet) {
    it("a pit latrine blueprint (sticks) and a toilet blueprint (planks) become furniture") {
        SetupGround();
        int bp = CreateRecipeBlueprint(4, 4, 1, CONSTRUCTION_PIT_LATRINE);
        expect(bp >= 0);
        FillStage(bp);
        CompleteBlueprint(bp);
        int fi = GetFurnitureAt(4, 4, 1);
        expect(fi >= 0 && furniture[fi].type == FURNITURE_PIT_LATRINE);

        bp = CreateRecipeBlueprint(8, 4, 1, CONSTRUCTION_TOILET);
        expect(bp >= 0);
        FillStage(bp);
        CompleteBlueprint(bp);
        fi = GetFurnitureAt(8, 4, 1);
        expect(fi >= 0 && furniture[fi].type == FURNITURE_TOILET);
    }

    it("a burning barrel is built from planks and starts empty, waiting for fuel") {
        SetupGround();
        expect(constructionRecipes[CONSTRUCTION_FIRE_BARREL].stages[0].inputs[0].alternatives[0].itemType == ITEM_PLANKS);
        int bp = CreateRecipeBlueprint(4, 4, 1, CONSTRUCTION_FIRE_BARREL);
        expect(bp >= 0);
        FillStage(bp);
        CompleteBlueprint(bp);
        int fi = GetFurnitureAt(4, 4, 1);
        expect(fi >= 0 && furniture[fi].type == FURNITURE_FIRE_BARREL);
        expect(!FurnitureLit(fi));
        expect(FurnitureNeedsFuel(fi));
        FurnitureAddFuel(fi, ITEM_LOG);
        expect(FurnitureLit(fi));
    }

    it("the latrine and the toilet are furniture you build in place") {
        expect(constructionRecipes[CONSTRUCTION_PIT_LATRINE].buildCategory == BUILD_FURNITURE);
        expect(constructionRecipes[CONSTRUCTION_TOILET].buildCategory == BUILD_FURNITURE);
        expect(constructionRecipes[CONSTRUCTION_PIT_LATRINE].stages[0].inputs[0].alternatives[0].itemType == ITEM_STICKS);
        expect(GetFurnitureDef(FURNITURE_PIT_LATRINE)->capabilities & CAP_TOILET);
        expect(GetFurnitureDef(FURNITURE_TOILET)->capabilities & CAP_TOILET);
        expect(!GetFurnitureDef(FURNITURE_PIT_LATRINE)->blocking);
    }

    it("a mover uses a pit latrine when that is all there is") {
        SetupGround();
        int latrine = SpawnFurniture(6, 5, 1, FURNITURE_PIT_LATRINE, MAT_OAK);
        int mi = SetupNeedyMover(5, 5);
        ProcessFreetimeNeeds();
        expect(movers[mi].freetimeState == FREETIME_SEEKING_TOILET);
        expect(movers[mi].needTarget == latrine);
    }

    it("a toilet beats a latrine nearby, a latrine wins when the toilet is far") {
        SetupGround();
        SpawnFurniture(6, 5, 1, FURNITURE_PIT_LATRINE, MAT_OAK);     // 1 cell: 1 / 2 = 0.5
        int toilet = SpawnFurniture(5, 7, 1, FURNITURE_TOILET, MAT_OAK);   // 2 cells: 2 / 3 = 0.67
        int mi = SetupNeedyMover(5, 5);
        ProcessFreetimeNeeds();
        expect(movers[mi].needTarget == toilet);

        SetupGround();
        int latrine = SpawnFurniture(6, 5, 1, FURNITURE_PIT_LATRINE, MAT_OAK);
        SpawnFurniture(5, 11, 1, FURNITURE_TOILET, MAT_OAK);         // 6 cells: 2 / 7 = 0.29
        mi = SetupNeedyMover(5, 5);
        ProcessFreetimeNeeds();
        expect(movers[mi].needTarget == latrine);
    }

    it("using a pit latrine dirties its floor most, a dry toilet a little") {
        SetupGround();
        int latrine = SpawnFurniture(6, 5, 1, FURNITURE_PIT_LATRINE, MAT_OAK);
        int mi = SetupNeedyMover(5, 5);
        UseToiletUntilDone(mi);
        expect(movers[mi].bladder >= 1.0f);
        expect(GetFloorDirt(6, 5, 1) == latrineDirtPerUse);
        expect(furniture[latrine].occupant == -1);

        SetupGround();
        SpawnFurniture(6, 5, 1, FURNITURE_TOILET, MAT_OAK);
        mi = SetupNeedyMover(5, 5);
        UseToiletUntilDone(mi);
        expect(movers[mi].bladder >= 1.0f);
        expect(GetFloorDirt(6, 5, 1) == toiletDryDirtPerUse);
        expect(toiletDryDirtPerUse < latrineDirtPerUse);
    }

    it("a tap or sink within 3 cells flushes a toilet clean") {
        SetupGround();
        SpawnFurniture(6, 5, 1, FURNITURE_TOILET, MAT_OAK);
        SpawnFurniture(9, 5, 1, FURNITURE_SINK, MAT_GRANITE);
        int mi = SetupNeedyMover(5, 5);
        UseToiletUntilDone(mi);
        expect(GetFloorDirt(6, 5, 1) == 0);
    }

    it("with mains water (toiletsPlumbed) every toilet flushes, a latrine still smells") {
        SetupGround();
        toiletsPlumbed = true;
        SpawnFurniture(6, 5, 1, FURNITURE_TOILET, MAT_OAK);
        int mi = SetupNeedyMover(5, 5);
        UseToiletUntilDone(mi);
        expect(GetFloorDirt(6, 5, 1) == 0);

        SetupGround();
        toiletsPlumbed = true;
        SpawnFurniture(6, 5, 1, FURNITURE_PIT_LATRINE, MAT_OAK);
        mi = SetupNeedyMover(5, 5);
        UseToiletUntilDone(mi);
        expect(GetFloorDirt(6, 5, 1) == latrineDirtPerUse);
        toiletsPlumbed = false;
    }

    it("a pot of water by the toilet flushes it, one water at a time") {
        SetupGround();
        SpawnFurniture(6, 5, 1, FURNITURE_TOILET, MAT_OAK);
        int pot = SpawnItem(7 * CELL_SIZE + CELL_SIZE * 0.5f, 6 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, ITEM_CLAY_POT);
        expect(pot >= 0);
        for (int i = 0; i < 2; i++) {
            int w = SpawnItem(items[pot].x, items[pot].y, 1.0f, ITEM_WATER);
            PutItemInContainer(w, pot);
        }
        expect(WaterUnits() == 2);
        int mi = SetupNeedyMover(5, 5);
        UseToiletUntilDone(mi);
        expect(GetFloorDirt(6, 5, 1) == 0);
        expect(WaterUnits() == 1);

        // far away water doesn't count
        SetupGround();
        SpawnFurniture(6, 5, 1, FURNITURE_TOILET, MAT_OAK);
        SpawnItem(12 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, ITEM_WATER);
        mi = SetupNeedyMover(5, 5);
        UseToiletUntilDone(mi);
        expect(GetFloorDirt(6, 5, 1) == toiletDryDirtPerUse);
    }

    it("a room with a pit latrine reads as a WC") {
        SetupGround();
        for (int y = 2; y <= 6; y++)
            for (int x = 2; x <= 6; x++) {
                if (x == 2 || x == 6 || y == 2 || y == 6) { GRID(x, y, 1) = CELL_WALL; SetWallMaterial(x, y, 1, MAT_OAK); }
                SET_FLOOR(x, y, 2);
            }
        SpawnFurniture(3, 3, 1, FURNITURE_PIT_LATRINE, MAT_OAK);
        UpdateRooms();
        DetectedRoom* r = GetRoom(GetRoomAt(4, 4, 1));
        expect(r != NULL);
        expect(r->type == ROOM_TYPE_WC);
    }
}

int main(int argc, char *argv[]) {
    test_verbose = c89spec_parse_args(argc, argv);
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);
    test(survival_toilet);
    return summary();
}
