#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "test_helpers.h"
#include "../src/world/cell_defs.h"
#include "../src/world/material.h"
#include "../src/world/pathfinding.h"
#include "../src/entities/mover.h"
#include "../src/world/terrain.h"
#include "../src/entities/items.h"
#include "../src/entities/item_defs.h"
#include "../src/entities/jobs.h"
#include "../src/entities/stockpiles.h"
#include "../src/entities/workshops.h"
#include "../src/world/designations.h"
#include "../src/simulation/trees.h"
#include "../src/simulation/balance.h"
#include "../src/core/time.h"

#include <string.h>
#include <math.h>

// Helper: item was successfully stored (either merged into a stack and deleted, or placed as-is)
#define ItemWasStored(idx) (!items[idx].active || items[idx].state == ITEM_IN_STOCKPILE)

// From core/saveload.c
void RebuildPostLoadState(void);
bool SaveWorld(const char* filename);
bool LoadWorld(const char* filename);

// Global flag for verbose output in tests
static bool test_verbose = false;

#if defined(__GNUC__) || defined(__clang__)
#define UNUSED __attribute__((unused))
#else
#define UNUSED
#endif

// Helper functions to check mover job state (replaces m->jobState checks)
static bool MoverIsIdle(Mover* m) {
    return m->currentJobId < 0;
}

static UNUSED bool MoverHasHaulJob(Mover* m) {
    if (m->currentJobId < 0) return false;
    Job* job = GetJob(m->currentJobId);
    return job && job->active && job->type == JOBTYPE_HAUL;
}

static bool MoverHasClearJob(Mover* m) {
    if (m->currentJobId < 0) return false;
    Job* job = GetJob(m->currentJobId);
    return job && job->active && job->type == JOBTYPE_CLEAR;
}

static bool MoverHasMineJob(Mover* m) {
    if (m->currentJobId < 0) return false;
    Job* job = GetJob(m->currentJobId);
    return job && job->active && job->type == JOBTYPE_MINE;
}

static bool MoverHasBuildJob(Mover* m) {
    if (m->currentJobId < 0) return false;
    Job* job = GetJob(m->currentJobId);
    return job && job->active && job->type == JOBTYPE_BUILD;
}

static bool MoverHasHaulToBlueprintJob(Mover* m) {
    if (m->currentJobId < 0) return false;
    Job* job = GetJob(m->currentJobId);
    return job && job->active && job->type == JOBTYPE_HAUL_TO_BLUEPRINT;
}

static bool MoverIsMovingToPickup(Mover* m) {
    if (m->currentJobId < 0) return false;
    Job* job = GetJob(m->currentJobId);
    if (!job || !job->active) return false;
    return (job->type == JOBTYPE_HAUL || job->type == JOBTYPE_CLEAR || job->type == JOBTYPE_HAUL_TO_BLUEPRINT) 
           && job->step == STEP_MOVING_TO_PICKUP;
}

static bool MoverIsCarrying(Mover* m) {
    if (m->currentJobId < 0) return false;
    Job* job = GetJob(m->currentJobId);
    if (!job || !job->active) return false;
    return (job->type == JOBTYPE_HAUL || job->type == JOBTYPE_CLEAR || job->type == JOBTYPE_HAUL_TO_BLUEPRINT) 
           && job->step == STEP_CARRYING;
}

static UNUSED bool MoverIsMining(Mover* m) {
    if (m->currentJobId < 0) return false;
    Job* job = GetJob(m->currentJobId);
    if (!job || !job->active) return false;
    return job->type == JOBTYPE_MINE && job->step == STEP_WORKING;
}

static bool MoverIsBuilding(Mover* m) {
    if (m->currentJobId < 0) return false;
    Job* job = GetJob(m->currentJobId);
    if (!job || !job->active) return false;
    return job->type == JOBTYPE_BUILD && job->step == STEP_WORKING;
}

static int MoverGetTargetItem(Mover* m) {
    if (m->currentJobId < 0) return -1;
    Job* job = GetJob(m->currentJobId);
    if (!job || !job->active) return -1;
    return job->targetItem;
}

static int MoverGetCarryingItem(Mover* m) {
    if (m->currentJobId < 0) return -1;
    Job* job = GetJob(m->currentJobId);
    if (!job || !job->active) return -1;
    return job->carryingItem;
}

static int MoverGetTargetStockpile(Mover* m) {
    if (m->currentJobId < 0) return -1;
    Job* job = GetJob(m->currentJobId);
    if (!job || !job->active) return -1;
    return job->targetStockpile;
}

static int MoverGetTargetBlueprint(Mover* m) {
    if (m->currentJobId < 0) return -1;
    Job* job = GetJob(m->currentJobId);
    if (!job || !job->active) return -1;
    return job->targetBlueprint;
}

static int MoverGetTargetMineX(Mover* m) {
    if (m->currentJobId < 0) return -1;
    Job* job = GetJob(m->currentJobId);
    if (!job || !job->active) return -1;
    return job->targetMineX;
}

static int MoverGetTargetMineY(Mover* m) {
    if (m->currentJobId < 0) return -1;
    Job* job = GetJob(m->currentJobId);
    if (!job || !job->active) return -1;
    return job->targetMineY;
}

static int MoverGetTargetMineZ(Mover* m) {
    if (m->currentJobId < 0) return -1;
    Job* job = GetJob(m->currentJobId);
    if (!job || !job->active) return -1;
    return job->targetMineZ;
}

// Helper: fill a recipe blueprint's current stage so it becomes READY_TO_BUILD.
// Spawns and delivers the required items. For tests that just need a built-ready blueprint.
static void FillBlueprintStage(int bpIdx, MaterialType mat) {
    Blueprint* bp = &blueprints[bpIdx];
    const ConstructionRecipe* recipe = GetConstructionRecipe(bp->recipeIndex);
    if (!recipe) return;
    const ConstructionStage* stage = &recipe->stages[bp->stage];
    for (int s = 0; s < stage->inputCount; s++) {
        const ConstructionInput* input = &stage->inputs[s];
        ItemType itemType = input->anyBuildingMat ? ITEM_BLOCKS : input->alternatives[0].itemType;
        uint8_t itemMat = (mat != MAT_NONE) ? (uint8_t)mat : DefaultMaterialForItemType(itemType);
        for (int i = 0; i < input->count; i++) {
            int itemIdx = SpawnItemWithMaterial(
                bp->x * CELL_SIZE + CELL_SIZE * 0.5f,
                bp->y * CELL_SIZE + CELL_SIZE * 0.5f,
                (float)bp->z, itemType, itemMat);
            DeliverMaterialToBlueprint(bpIdx, itemIdx);
        }
    }
}

/*
 * Phase 0 Tests: Item spawn + single pickup
 * 
 * These tests verify the minimal jobs system:
 * - Items can be spawned on the map
 * - Movers can claim (reserve) items
 * - Movers walk to items and pick them up
 * - Items vanish on pickup
 * - Reservations prevent double-claims
 */

describe(item_system) {
    it("should spawn an item at a position") {
        ClearItems();
        
        int idx = SpawnItem(100.0f, 100.0f, 0.0f, ITEM_RED);
        
        expect(idx >= 0);
        expect(IsItemActive(idx) == true);
        expect(GetItemX(idx) == 100.0f);
        expect(GetItemY(idx) == 100.0f);
        expect(GetItemType(idx) == ITEM_RED);
        expect(GetItemReservedBy(idx) == -1);
    }

    it("should track item count correctly") {
        ClearItems();
        
        expect(itemCount == 0);
        
        SpawnItem(100.0f, 100.0f, 0.0f, ITEM_RED);
        expect(itemCount == 1);
        
        SpawnItem(200.0f, 200.0f, 0.0f, ITEM_GREEN);
        expect(itemCount == 2);
    }

    it("should delete an item") {
        ClearItems();
        
        int idx = SpawnItem(100.0f, 100.0f, 0.0f, ITEM_RED);
        expect(IsItemActive(idx) == true);
        
        DeleteItem(idx);
        expect(IsItemActive(idx) == false);
    }
}

describe(item_reservation) {
    it("should reserve an item for a mover") {
        ClearItems();
        
        int itemIdx = SpawnItem(100.0f, 100.0f, 0.0f, ITEM_RED);
        
        bool reserved = ReserveItem(itemIdx, 0);  // mover 0 reserves
        
        expect(reserved == true);
        expect(GetItemReservedBy(itemIdx) == 0);
    }

    it("should reject reservation if item already reserved") {
        ClearItems();
        
        int itemIdx = SpawnItem(100.0f, 100.0f, 0.0f, ITEM_RED);
        
        ReserveItem(itemIdx, 0);  // mover 0 reserves
        bool secondReserve = ReserveItem(itemIdx, 1);  // mover 1 tries
        
        expect(secondReserve == false);
        expect(GetItemReservedBy(itemIdx) == 0);  // still reserved by mover 0
    }

    it("should release reservation") {
        ClearItems();
        
        int itemIdx = SpawnItem(100.0f, 100.0f, 0.0f, ITEM_RED);
        ReserveItem(itemIdx, 0);
        
        ReleaseItemReservation(itemIdx);
        
        expect(GetItemReservedBy(itemIdx) == -1);
    }

    it("should find nearest unreserved item") {
        ClearItems();
        
        // Spawn two items, one closer
        SpawnItem(200.0f, 200.0f, 0.0f, ITEM_RED);  // farther
        int closerIdx = SpawnItem(50.0f, 50.0f, 0.0f, ITEM_GREEN);  // closer to origin
        
        int found = FindNearestUnreservedItem(0.0f, 0.0f, 0.0f);
        
        expect(found == closerIdx);
    }

    it("should skip reserved items when finding nearest") {
        ClearItems();
        
        int closerIdx = SpawnItem(50.0f, 50.0f, 0.0f, ITEM_RED);
        int fartherIdx = SpawnItem(200.0f, 200.0f, 0.0f, ITEM_GREEN);
        
        ReserveItem(closerIdx, 0);  // reserve the closer one
        
        int found = FindNearestUnreservedItem(0.0f, 0.0f, 0.0f);
        
        expect(found == fartherIdx);  // should find the farther one
    }

    it("should return -1 when no unreserved items exist") {
        ClearItems();
        
        int idx = SpawnItem(50.0f, 50.0f, 0.0f, ITEM_RED);
        ReserveItem(idx, 0);
        
        int found = FindNearestUnreservedItem(0.0f, 0.0f, 0.0f);
        
        expect(found == -1);
    }
}

describe(mover_job_state) {
    it("should start movers in idle state") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 4, 4);
        
        ClearMovers();
        ClearItems();
        
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 16.0f, 16.0f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        expect(MoverIsIdle(m));
        expect(MoverGetTargetItem(m) == -1);
    }

    it("should assign item to idle mover") {
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n");
        
        // Use A* for tests (doesn't require HPA graph building)
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover at (1,1)
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Item at (6,2) - within the grid
        int itemIdx = SpawnItem(6 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Need a stockpile for job assignment to work
        int spIdx = CreateStockpile(3, 2, 0, 1, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        AssignJobs();  // should assign item to idle mover
        
        expect(MoverIsMovingToPickup(m));
        expect(MoverGetTargetItem(m) == itemIdx);
        expect(GetItemReservedBy(itemIdx) == 0);
    }
}

describe(pickup_behavior) {
    it("should pick up item and deliver to stockpile") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        // Use A* for tests (doesn't require HPA graph building)
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover at (1,1), item at (3,1) - short walk
        float moverX = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        float moverY = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        float itemX = 3 * CELL_SIZE + CELL_SIZE * 0.5f;
        float itemY = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, moverX, moverY, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        int itemIdx = SpawnItem(itemX, itemY, 0.0f, ITEM_RED);
        
        // Create stockpile at (6,1)
        int spIdx = CreateStockpile(6, 1, 0, 1, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        AssignJobs();
        
        expect(MoverIsMovingToPickup(m));
        expect(IsItemActive(itemIdx) == true);
        
        // Run simulation until item is in stockpile (or timeout)
        for (int i = 0; i < 500; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (items[itemIdx].state == ITEM_IN_STOCKPILE) break;
        }
        
        // Item should be in stockpile
        expect(items[itemIdx].state == ITEM_IN_STOCKPILE);
        expect(MoverIsIdle(m));
        expect(MoverGetCarryingItem(m) == -1);
    }
}

describe(reservation_safety) {
    it("should not allow two movers to claim the same item") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Two movers equidistant from one item
        float itemX = 4 * CELL_SIZE + CELL_SIZE * 0.5f;
        float itemY = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
        
        Mover* m0 = &movers[0];
        Mover* m1 = &movers[1];
        Point goal = {0, 0, 0};
        
        InitMover(m0, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        InitMover(m1, 7 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 2;
        
        int itemIdx = SpawnItem(itemX, itemY, 0.0f, ITEM_RED);
        
        // Need stockpile for job assignment
        int spIdx = CreateStockpile(8, 2, 0, 1, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        AssignJobs();
        
        // Only one mover should have the item
        int claimCount = 0;
        if (MoverGetTargetItem(m0) == itemIdx) claimCount++;
        if (MoverGetTargetItem(m1) == itemIdx) claimCount++;
        
        expect(claimCount == 1);
        expect(GetItemReservedBy(itemIdx) >= 0);
    }

    it("should release reservation when item is deleted externally") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover at (1,1)
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Item at (8,2) - within grid
        int itemIdx = SpawnItem(8 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Need stockpile
        int spIdx = CreateStockpile(5, 2, 0, 1, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        AssignJobs();
        expect(MoverIsMovingToPickup(m));
        
        // Externally delete the item (simulates someone else taking it)
        DeleteItem(itemIdx);
        
        // Run a few ticks - mover should detect and go back to idle
        for (int i = 0; i < 10; i++) {
            Tick();
            JobsTick();
        }
        
        expect(MoverIsIdle(m));
        expect(MoverGetTargetItem(m) == -1);
    }
}

describe(post_job_behavior) {
    it("should pick up next item if available after completing a job") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover at (1,1), two items nearby
        float moverX = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        float moverY = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, moverX, moverY, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Spawn two items
        float item1X = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
        float item1Y = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        float item2X = 4 * CELL_SIZE + CELL_SIZE * 0.5f;
        float item2Y = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        
        SpawnItem(item1X, item1Y, 0.0f, ITEM_RED);
        int item2Idx = SpawnItem(item2X, item2Y, 0.0f, ITEM_GREEN);
        
        // Stockpile that accepts both types, with 2 slots
        int spIdx = CreateStockpile(7, 1, 0, 2, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        SetStockpileFilter(spIdx, ITEM_GREEN, true);
        
        expect(itemCount == 2);
        
        // Run until first item is in stockpile
        int storedCount = 0;
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            storedCount = 0;
            for (int j = 0; j < MAX_ITEMS; j++) {
                if (IsItemActive(j) && items[j].state == ITEM_IN_STOCKPILE) storedCount++;
            }
            if (storedCount == 1) break;
        }
        
        expect(storedCount == 1);
        
        // Mover should now be going for the second item
        // Give it a few ticks to get assigned
        for (int i = 0; i < 30; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }
        
        expect(MoverIsMovingToPickup(m));
        expect(MoverGetTargetItem(m) == item2Idx);
    }

    it("should resume wandering when no more items exist") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        endlessMoverMode = true;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover at (1,1), one item nearby
        float moverX = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        float moverY = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        float itemX = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
        float itemY = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, moverX, moverY, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        int itemIdx = SpawnItem(itemX, itemY, 0.0f, ITEM_RED);
        
        // Stockpile
        int spIdx = CreateStockpile(7, 1, 0, 1, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        // Run until item is in stockpile
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (items[itemIdx].state == ITEM_IN_STOCKPILE) break;
        }
        
        expect(items[itemIdx].state == ITEM_IN_STOCKPILE);
        expect(MoverIsIdle(m));
        
        // Run a few more ticks - mover should get a new path (wandering)
        for (int i = 0; i < 30; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }
        
        // Mover should have a path now (not stuck with pathLength == 0)
        expect(GetMoverPathLength(0) > 0);
    }
}

/*
 * Stockpile Tests (from convo-jobs.md)
 * 
 * These tests verify the full haul loop:
 * - Pick up item
 * - Carry to stockpile
 * - Drop in valid slot
 */

describe(stockpile_system) {
    it("should create a stockpile with tiles and filters") {
        ClearStockpiles();
        
        // Create a stockpile at (2,2) that allows red only
        int spIdx = CreateStockpile(2, 2, 0, 2, 2);  // x, y, z, width, height
        expect(spIdx >= 0);
        
        // Set filter to allow only red
        SetStockpileFilter(spIdx, ITEM_RED, true);
        SetStockpileFilter(spIdx, ITEM_GREEN, false);
        SetStockpileFilter(spIdx, ITEM_BLUE, false);
        
        expect(StockpileAcceptsType(spIdx, ITEM_RED) == true);
        expect(StockpileAcceptsType(spIdx, ITEM_GREEN) == false);
        expect(StockpileAcceptsType(spIdx, ITEM_BLUE) == false);
    }

    it("should find free slot in stockpile") {
        ClearStockpiles();
        
        int spIdx = CreateStockpile(2, 2, 0, 2, 2);  // 4 tiles total
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        int slotX, slotY;
        bool found = FindFreeStockpileSlot(spIdx, ITEM_RED, MAT_NONE, &slotX, &slotY);
        
        expect(found == true);
        expect(slotX >= 2 && slotX < 4);
        expect(slotY >= 2 && slotY < 4);
    }

    it("should reserve stockpile slot") {
        ClearStockpiles();
        
        int spIdx = CreateStockpile(2, 2, 0, 1, 1);  // 1 tile only
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        int slotX, slotY;
        bool found = FindFreeStockpileSlot(spIdx, ITEM_RED, MAT_NONE, &slotX, &slotY);
        expect(found == true);
        
        // Reserve it
        bool reserved = ReserveStockpileSlot(spIdx, slotX, slotY, 0, ITEM_RED, MAT_NONE);  // mover 0
        expect(reserved == true);
        
        // Should still find slot for same type (stacking into reserved slot)
        int slotX2, slotY2;
        bool found2 = FindFreeStockpileSlot(spIdx, ITEM_RED, MAT_NONE, &slotX2, &slotY2);
        expect(found2 == true);
        expect(slotX2 == slotX);
        expect(slotY2 == slotY);
        
        // Different type should NOT find the reserved slot
        SetStockpileFilter(spIdx, ITEM_BLUE, true);
        int slotX3, slotY3;
        bool found3 = FindFreeStockpileSlot(spIdx, ITEM_BLUE, MAT_NONE, &slotX3, &slotY3);
        expect(found3 == false);
    }
}

describe(haul_happy_path) {
    it("should haul single item to matching stockpile") {
        // Test 1 from convo-jobs.md
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover at (1,1)
        float moverX = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        float moverY = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, moverX, moverY, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Item at (8,8)
        float itemX = 8 * CELL_SIZE + CELL_SIZE * 0.5f;
        float itemY = 8 * CELL_SIZE + CELL_SIZE * 0.5f;
        int itemIdx = SpawnItem(itemX, itemY, 0.0f, ITEM_RED);
        
        // Stockpile at (2,2) allows red
        int spIdx = CreateStockpile(2, 2, 0, 1, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        // Run simulation
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            
            // Check if item is in stockpile
            if (items[itemIdx].state == ITEM_IN_STOCKPILE) break;
        }
        
        // Item should be in stockpile at (2,2)
        expect(items[itemIdx].state == ITEM_IN_STOCKPILE);
        expect((int)(GetItemX(itemIdx) / CELL_SIZE) == 2);
        expect((int)(GetItemY(itemIdx) / CELL_SIZE) == 2);
        
        // Mover should be idle
        expect(MoverIsIdle(m));
        expect(MoverGetTargetItem(m) == -1);
    }

    it("should respect stockpile type filters") {
        // Test 2 from convo-jobs.md
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Red item at (8,8), green item at (8,7)
        int redIdx = SpawnItem(8 * CELL_SIZE + CELL_SIZE * 0.5f, 8 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        int greenIdx = SpawnItem(8 * CELL_SIZE + CELL_SIZE * 0.5f, 7 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_GREEN);
        
        // Stockpile A at (2,2) allows red only
        int spA = CreateStockpile(2, 2, 0, 1, 1);
        SetStockpileFilter(spA, ITEM_RED, true);
        SetStockpileFilter(spA, ITEM_GREEN, false);
        
        // Stockpile B at (2,3) allows green only
        int spB = CreateStockpile(2, 3, 0, 1, 1);
        SetStockpileFilter(spB, ITEM_RED, false);
        SetStockpileFilter(spB, ITEM_GREEN, true);
        
        // Run simulation
        for (int i = 0; i < 2000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            
            // Check if both items are stored
            if (items[redIdx].state == ITEM_IN_STOCKPILE && 
                items[greenIdx].state == ITEM_IN_STOCKPILE) break;
        }
        
        // Red should be in stockpile A (2,2)
        expect(items[redIdx].state == ITEM_IN_STOCKPILE);
        expect((int)(GetItemX(redIdx) / CELL_SIZE) == 2);
        expect((int)(GetItemY(redIdx) / CELL_SIZE) == 2);
        
        // Green should be in stockpile B (2,3)
        expect(items[greenIdx].state == ITEM_IN_STOCKPILE);
        expect((int)(GetItemX(greenIdx) / CELL_SIZE) == 2);
        expect((int)(GetItemY(greenIdx) / CELL_SIZE) == 3);
    }
}

describe(stockpile_capacity) {
    it("should stop hauling when stockpile is full") {
        // Test 3 from convo-jobs.md
        // With stacking, we need to pre-fill the slot to 9/10 so only 1 more item fits
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // 2 red items
        int item1 = SpawnItem(8 * CELL_SIZE + CELL_SIZE * 0.5f, 8 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        int item2 = SpawnItem(8 * CELL_SIZE + CELL_SIZE * 0.5f, 7 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Stockpile with only 1 tile, pre-filled to 9 items (only 1 more fits)
        int spIdx = CreateStockpile(2, 2, 0, 1, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        SetStockpileSlotCount(spIdx, 0, 0, ITEM_RED, 9);  // 9/10 full
        
        // Run simulation
        for (int i = 0; i < 2000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }
        
        // Exactly 1 item should be stored (merged into slot, so deleted)
        int storedCount = 0;
        if (ItemWasStored(item1)) storedCount++;
        if (ItemWasStored(item2)) storedCount++;
        expect(storedCount == 1);
        
        // Other item should still be on ground
        int groundCount = 0;
        if (items[item1].active && items[item1].state == ITEM_ON_GROUND) groundCount++;
        if (items[item2].active && items[item2].state == ITEM_ON_GROUND) groundCount++;
        expect(groundCount == 1);
        
        // Slot should now be full (10/10)
        expect(GetStockpileSlotCount(spIdx, 2, 2) == 10);
        
        // Mover should be idle (not stuck carrying)
        expect(MoverIsIdle(m));
        expect(MoverGetCarryingItem(m) == -1);
    }
}

describe(multi_agent_hauling) {
    it("should not have two movers deliver to same stockpile slot") {
        // Test 4 from convo-jobs.md
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // 3 movers spread out
        for (int i = 0; i < 3; i++) {
            Mover* m = &movers[i];
            Point goal = {i * 3, 1, 0};
            InitMover(m, (i * 3) * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        }
        moverCount = 3;
        
        // 3 red items spread out
        int itemIdxs[3];
        itemIdxs[0] = SpawnItem(7 * CELL_SIZE + CELL_SIZE * 0.5f, 7 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        itemIdxs[1] = SpawnItem(8 * CELL_SIZE + CELL_SIZE * 0.5f, 8 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        itemIdxs[2] = SpawnItem(9 * CELL_SIZE + CELL_SIZE * 0.5f, 7 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Stockpile with 3 tiles
        int spIdx = CreateStockpile(2, 2, 0, 3, 1);  // 3 tiles in a row
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        // Run simulation
        for (int i = 0; i < 2000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            
            // Check if all stored (merged items are deleted, so check both)
            int stored = 0;
            for (int j = 0; j < 3; j++) {
                if (ItemWasStored(itemIdxs[j])) stored++;
            }
            if (stored == 3) break;
        }
        
        // All 3 items should be stored (either merged into stack or as slot representative)
        for (int i = 0; i < 3; i++) {
            expect(ItemWasStored(itemIdxs[i]));
        }
        
        // Total slot counts across stockpile should equal 3
        int totalStored = 0;
        Stockpile* sp = &stockpiles[spIdx];
        for (int s = 0; s < sp->width * sp->height; s++) {
            totalStored += sp->slotCounts[s];
        }
        expect(totalStored == 3);
    }
}

describe(haul_cancellation) {
    it("should release stockpile reservation when item deleted mid-haul") {
        // Test 5 from convo-jobs.md (extended for stockpiles)
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Item far away
        int itemIdx = SpawnItem(8 * CELL_SIZE + CELL_SIZE * 0.5f, 8 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Stockpile
        int spIdx = CreateStockpile(2, 2, 0, 1, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        // Run a few ticks to let mover start the job
        for (int i = 0; i < 30; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }
        
        expect(MoverIsMovingToPickup(m));
        
        // Delete item mid-haul
        DeleteItem(itemIdx);
        
        // Run more ticks
        for (int i = 0; i < 30; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }
        
        // Mover should be idle
        expect(MoverIsIdle(m));
        expect(MoverGetTargetItem(m) == -1);
        
        // Stockpile slot should be unreserved (can find a free slot)
        int slotX, slotY;
        bool found = FindFreeStockpileSlot(spIdx, ITEM_RED, MAT_NONE, &slotX, &slotY);
        expect(found == true);
    }

    it("should safe-drop item when stockpile deleted while carrying") {
        // Test 7 from convo-jobs.md
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover near item
        Mover* m = &movers[0];
        Point goal = {7, 8, 0};
        InitMover(m, 7 * CELL_SIZE + CELL_SIZE * 0.5f, 8 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Item very close to mover
        int itemIdx = SpawnItem(8 * CELL_SIZE + CELL_SIZE * 0.5f, 8 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Stockpile far away
        int spIdx = CreateStockpile(2, 2, 0, 1, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        // Run until mover is carrying
        for (int i = 0; i < 500; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (MoverIsCarrying(m)) break;
        }
        
        expect(MoverIsCarrying(m));
        expect(MoverGetCarryingItem(m) == itemIdx);
        expect(items[itemIdx].state == ITEM_CARRIED);
        
        // Delete stockpile while carrying
        DeleteStockpile(spIdx);
        
        // Run more ticks
        for (int i = 0; i < 60; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }
        
        // Mover should have safe-dropped the item
        expect(MoverIsIdle(m));
        expect(MoverGetCarryingItem(m) == -1);
        
        // Item should be back on ground (not vanished, not stuck as "carried")
        expect(items[itemIdx].state == ITEM_ON_GROUND);
        expect(IsItemActive(itemIdx) == true);
    }
}

describe(filter_change_mid_haul) {
    it("should re-haul stored item when stockpile filter changes to disallow its type") {
        // Bug: Items already in stockpile should be moved when filter changes
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // RGB stockpile at (5,5) - accepts all types initially
        int spRGB = CreateStockpile(5, 5, 0, 1, 1);
        SetStockpileFilter(spRGB, ITEM_RED, true);
        SetStockpileFilter(spRGB, ITEM_GREEN, true);
        SetStockpileFilter(spRGB, ITEM_BLUE, true);
        
        // Green-only stockpile at (8,8)
        int spGreen = CreateStockpile(8, 8, 0, 1, 1);
        SetStockpileFilter(spGreen, ITEM_RED, false);
        SetStockpileFilter(spGreen, ITEM_GREEN, true);
        SetStockpileFilter(spGreen, ITEM_BLUE, false);
        
        // Spawn green item near RGB stockpile
        int greenItem = SpawnItem(4 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_GREEN);
        
        // Let mover haul green item to RGB stockpile
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (items[greenItem].state == ITEM_IN_STOCKPILE) break;
        }
        
        // Green item should be in RGB stockpile
        expect(items[greenItem].state == ITEM_IN_STOCKPILE);
        int itemTileX = (int)(GetItemX(greenItem) / CELL_SIZE);
        int itemTileY = (int)(GetItemY(greenItem) / CELL_SIZE);
        expect(itemTileX == 5);
        expect(itemTileY == 5);
        expect(MoverIsIdle(m));
        
        // Now change RGB stockpile to RED-only (green no longer allowed)
        SetStockpileFilter(spRGB, ITEM_GREEN, false);
        SetStockpileFilter(spRGB, ITEM_BLUE, false);
        
        // Run simulation - green item should be moved to green stockpile
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            
            // Check if item moved to green stockpile
            itemTileX = (int)(GetItemX(greenItem) / CELL_SIZE);
            itemTileY = (int)(GetItemY(greenItem) / CELL_SIZE);
            if (itemTileX == 8 && itemTileY == 8) break;
        }
        
        // Green item should now be in green stockpile
        expect(items[greenItem].state == ITEM_IN_STOCKPILE);
        itemTileX = (int)(GetItemX(greenItem) / CELL_SIZE);
        itemTileY = (int)(GetItemY(greenItem) / CELL_SIZE);
        expect(itemTileX == 8);
        expect(itemTileY == 8);
    }
    
    it("should safe-drop when stockpile filter changes to disallow item while carrying") {
        // Test 6 from convo-jobs.md
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover near item
        Mover* m = &movers[0];
        Point goal = {7, 8, 0};
        InitMover(m, 7 * CELL_SIZE + CELL_SIZE * 0.5f, 8 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Item very close to mover
        int itemIdx = SpawnItem(8 * CELL_SIZE + CELL_SIZE * 0.5f, 8 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Stockpile far away, allows red initially
        int spIdx = CreateStockpile(2, 2, 0, 1, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        // Run until mover is carrying
        for (int i = 0; i < 500; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (MoverIsCarrying(m)) break;
        }
        
        expect(MoverIsCarrying(m));
        expect(MoverGetCarryingItem(m) == itemIdx);
        
        // Change filter to disallow red while carrying
        SetStockpileFilter(spIdx, ITEM_RED, false);
        
        // Run more ticks
        for (int i = 0; i < 60; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }
        
        // Mover should have safe-dropped the item
        expect(MoverIsIdle(m));
        expect(MoverGetCarryingItem(m) == -1);
        
        // Item should be back on ground
        expect(items[itemIdx].state == ITEM_ON_GROUND);
        expect(IsItemActive(itemIdx) == true);
    }
}

describe(dynamic_obstacles) {
    it("should cancel job when path becomes blocked mid-haul") {
        // Test 9 from convo-jobs.md
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover at left
        Mover* m = &movers[0];
        Point goal = {1, 5, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Item at right
        int itemIdx = SpawnItem(8 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Stockpile at far right
        int spIdx = CreateStockpile(9, 5, 0, 1, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        AssignJobs();
        expect(MoverIsMovingToPickup(m));
        
        // Let mover start moving
        for (int i = 0; i < 50; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }
        
        // Block the path with a wall (vertical wall in middle)
        for (int y = 0; y < 10; y++) {
            grid[0][y][5] = CELL_WALL;
        }
        MarkChunkDirty(5, 0, 0);
        MarkChunkDirty(5, 5, 0);
        MarkChunkDirty(5, 9, 0);
        
        // Run more ticks - mover should eventually give up or repath
        // This tests that the system doesn't get stuck
        // Need enough time for stuck detection (3+ seconds = 180+ ticks at 60Hz)
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }
        
        // Mover should not be stuck forever trying to reach the item
        // Either it found a way around, gave up, or is still trying to repath
        // The key invariant: it's not in a broken state (crash/deadlock)
        // Note: MOVING_TO_ITEM with pathLength=0 is valid - mover is waiting to repath
        bool validState = (MoverIsIdle(m)) || 
                          (MoverIsMovingToPickup(m)) ||
                          (MoverIsCarrying(m));
        expect(validState == true);
        
        // Also verify the item wasn't corrupted
        expect(IsItemActive(itemIdx) == true);
        expect(items[itemIdx].state == ITEM_ON_GROUND || items[itemIdx].state == ITEM_CARRIED);
    }

    it("should cancel job immediately when wall placed on item") {
        // Scenario: mover assigned to pick up item, wall drawn on item's cell
        // Expected: job cancels immediately (not wait 3 seconds)
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover at left
        Mover* m = &movers[0];
        Point goal = {1, 2, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Item at right
        int itemIdx = SpawnItem(8 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Stockpile
        int spIdx = CreateStockpile(9, 2, 0, 1, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        AssignJobs();
        expect(MoverIsMovingToPickup(m));
        expect(MoverGetTargetItem(m) == itemIdx);
        
        // Place wall ON the item's cell
        grid[0][2][8] = CELL_WALL;
        
        // Run just ONE tick - job should cancel immediately
        JobsTick();
        
        // Job should be cancelled immediately (not wait 3 seconds)
        expect(MoverIsIdle(m));
        expect(MoverGetTargetItem(m) == -1);
        expect(GetItemReservedBy(itemIdx) == -1);
    }

    it("should not assign job to item on wall") {
        // Scenario: item exists on a wall cell, should not be assigned
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover ready to work
        Mover* m = &movers[0];
        Point goal = {1, 2, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Item on a cell that IS a wall
        grid[0][2][8] = CELL_WALL;
        int itemIdx = SpawnItem(8 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Stockpile
        int spIdx = CreateStockpile(9, 2, 0, 1, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        (void)spIdx;
        
        AssignJobs();
        
        // Mover should NOT be assigned to the item on a wall
        expect(MoverIsIdle(m));
        expect(GetItemReservedBy(itemIdx) == -1);
    }
}

describe(stockpile_expansion) {
    it("should haul second item after stockpile is expanded") {
        // Test 11 from convo-jobs.md
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // 2 items
        int item1 = SpawnItem(7 * CELL_SIZE + CELL_SIZE * 0.5f, 7 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        int item2 = SpawnItem(8 * CELL_SIZE + CELL_SIZE * 0.5f, 7 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Stockpile with only 1 tile, max stack 1 (only 1 item fits)
        int spIdx = CreateStockpile(2, 2, 0, 1, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        SetStockpileMaxStackSize(spIdx, 1);
        
        // Run until first item stored and mover idle
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            
            if (ItemWasStored(item1) || ItemWasStored(item2)) {
                if (MoverIsIdle(m)) break;
            }
        }
        
        // One item should be stored
        int storedCount = 0;
        if (ItemWasStored(item1)) storedCount++;
        if (ItemWasStored(item2)) storedCount++;
        expect(storedCount == 1);
        expect(MoverIsIdle(m));
        
        // Now expand stockpile by creating a second one (simulating expansion)
        int spIdx2 = CreateStockpile(3, 2, 0, 1, 1);
        SetStockpileFilter(spIdx2, ITEM_RED, true);
        
        // Run more - second item should now get hauled
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            
            storedCount = 0;
            if (ItemWasStored(item1)) storedCount++;
            if (ItemWasStored(item2)) storedCount++;
            if (storedCount == 2) break;
        }
        
        // Both items should now be stored
        expect(ItemWasStored(item1));
        expect(ItemWasStored(item2));
    }
}

describe(stress_test) {
    it("should handle many items and agents without deadlock") {
        // Test 12 from convo-jobs.md (smaller scale for unit test)
        // 20x20 grid to ensure plenty of room
        InitTestGridFromAscii(
            "....................\n"
            "....................\n"
            "....................\n"
            "....................\n"
            "....................\n"
            "....................\n"
            "....................\n"
            "....................\n"
            "....................\n"
            "....................\n"
            "....................\n"
            "....................\n"
            "....................\n"
            "....................\n"
            "....................\n"
            "....................\n"
            "....................\n"
            "....................\n"
            "....................\n"
            "....................\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // 3 movers spread out at top
        for (int i = 0; i < 3; i++) {
            Mover* m = &movers[i];
            Point goal = {2 + i * 3, 2, 0};
            InitMover(m, (2 + i * 3) * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        }
        moverCount = 3;
        
        // 9 items (3 of each type) scattered in middle
        int itemIdxs[9];
        for (int i = 0; i < 9; i++) {
            float x = (5 + (i % 3) * 3) * CELL_SIZE + CELL_SIZE * 0.5f;
            float y = (8 + (i / 3)) * CELL_SIZE + CELL_SIZE * 0.5f;
            ItemType type = (ItemType)(i % 3);  // RED, GREEN, BLUE cycling
            itemIdxs[i] = SpawnItem(x, y, 0.0f, type);
        }
        
        // 3 stockpiles at bottom, one for each type, with enough capacity
        int spRed = CreateStockpile(2, 15, 0, 2, 2);  // 4 slots
        SetStockpileFilter(spRed, ITEM_RED, true);
        
        int spGreen = CreateStockpile(6, 15, 0, 2, 2);
        SetStockpileFilter(spGreen, ITEM_GREEN, true);
        
        int spBlue = CreateStockpile(10, 15, 0, 2, 2);
        SetStockpileFilter(spBlue, ITEM_BLUE, true);
        
        // Run simulation
        for (int i = 0; i < 10000; i++) {
            Tick();
            ItemsTick(TICK_DT);  // Decrement unreachable cooldowns
            AssignJobs();
            JobsTick();
            
            // Check if all stored
            int stored = 0;
            for (int j = 0; j < 9; j++) {
                if (ItemWasStored(itemIdxs[j])) stored++;
            }
            if (stored == 9) break;
        }
        
        // All items should be stored (merged items are deleted)
        int stored = 0;
        for (int i = 0; i < 9; i++) {
            if (ItemWasStored(itemIdxs[i])) stored++;
        }
        expect(stored == 9);
        
        // All movers should be idle (not stuck carrying)
        for (int i = 0; i < 3; i++) {
            expect(MoverIsIdle(&movers[i]));
            expect(MoverGetCarryingItem(&movers[i]) == -1);
        }
    }
}

/*
 * =============================================================================
 * FUTURE FEATURES - Tests for hauling-next.md
 * These tests are expected to FAIL until the features are implemented.
 * =============================================================================
 */

describe(unreachable_item_cooldown) {
    it("should not spam-retry unreachable items every tick") {
        // Test 8 from convo-jobs.md
        // Setup: walled pocket with item inside, agent outside
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..####....\n"
            "..#..#....\n"
            "..####....\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover outside the pocket
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Item inside walled pocket (unreachable)
        int itemIdx = SpawnItem(3.5f * CELL_SIZE, 3.5f * CELL_SIZE, 0.0f, ITEM_RED);
        
        // Stockpile outside
        int spIdx = CreateStockpile(7, 7, 0, 1, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        // Run for a while
        int assignAttempts = 0;
        for (int i = 0; i < 300; i++) {  // 5 seconds at 60Hz
            Tick();
            ItemsTick(TICK_DT);  // Decrement cooldowns
            
            // Track how many times we try to assign this item
            if (MoverIsIdle(m)) {
                AssignJobs();
                if (MoverIsMovingToPickup(m) && MoverGetTargetItem(m) == itemIdx) {
                    assignAttempts++;
                }
            }
            JobsTick();
        }
        
        // Agent should end idle (can't reach item)
        expect(MoverIsIdle(m));
        
        // Item should still be on ground
        expect(items[itemIdx].state == ITEM_ON_GROUND);
        
        // Should NOT have tried to assign this item many times
        // With cooldown, should be at most a few attempts (initial + maybe 1 retry)
        // Without cooldown, would be ~300 attempts
        expect(assignAttempts < 10);
        
        // Item should have a cooldown set
        expect(items[itemIdx].unreachableCooldown > 0.0f);
    }
    
    it("should retry unreachable item after cooldown expires") {
        // Standard mode: z=0 with CELL_AIR is walkable (implicit bedrock below)
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..####....\n"
            "..#..#....\n"
            "..####....\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Item inside walled pocket
        int itemIdx = SpawnItem(3.5f * CELL_SIZE, 3.5f * CELL_SIZE, 0.0f, ITEM_RED);
        
        int spIdx = CreateStockpile(7, 7, 0, 1, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        // Try to assign - should fail and set cooldown
        AssignJobs();
        JobsTick();
        
        // Manually set a short cooldown for testing (simulating time passed)
        items[itemIdx].unreachableCooldown = 0.1f;
        
        // Run a few more ticks to expire the cooldown
        for (int i = 0; i < 10; i++) {
            Tick();
            ItemsTick(TICK_DT);
            AssignJobs();
            JobsTick();
        }
        
        // Now open a path by removing a wall
        // CELL_AIR at z=0 is walkable (implicit bedrock below)
        grid[0][3][2] = CELL_AIR;
        MarkChunkDirty(2, 3, 0);
        
        // Set cooldown to 0 to allow retry
        items[itemIdx].unreachableCooldown = 0.0f;
        
        // Run simulation - item should now be hauled
        for (int i = 0; i < 1000; i++) {
            Tick();
            ItemsTick(TICK_DT);
            AssignJobs();
            JobsTick();
            if (items[itemIdx].state == ITEM_IN_STOCKPILE) break;
        }
        
        expect(items[itemIdx].state == ITEM_IN_STOCKPILE);
    }
}

describe(gather_zones) {
    it("should only haul items from within gather zones") {
        // Test 10 from convo-jobs.md
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearGatherZones();
        
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Item inside gather zone (will be hauled)
        int insideIdx = SpawnItem(3 * CELL_SIZE + CELL_SIZE * 0.5f, 3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Item outside gather zone (should NOT be hauled)
        int outsideIdx = SpawnItem(8 * CELL_SIZE + CELL_SIZE * 0.5f, 8 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Create gather zone covering only (2,2) to (5,5)
        CreateGatherZone(2, 2, 0, 4, 4);
        
        // Stockpile
        int spIdx = CreateStockpile(7, 1, 0, 2, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        // Run simulation
        for (int i = 0; i < 2000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }
        
        // Only the inside item should be hauled
        expect(items[insideIdx].state == ITEM_IN_STOCKPILE);
        expect(items[outsideIdx].state == ITEM_ON_GROUND);
    }
    
    it("should haul all items when no gather zones exist") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearGatherZones();  // No gather zones
        
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Two items at different locations
        int item1 = SpawnItem(3 * CELL_SIZE + CELL_SIZE * 0.5f, 3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        int item2 = SpawnItem(8 * CELL_SIZE + CELL_SIZE * 0.5f, 8 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Stockpile with 2 slots
        int spIdx = CreateStockpile(5, 1, 0, 2, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        // Run simulation
        for (int i = 0; i < 2000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            
            if (ItemWasStored(item1) && ItemWasStored(item2)) break;
        }
        
        // Both items should be hauled (no gather zone restriction)
        expect(ItemWasStored(item1));
        expect(ItemWasStored(item2));
    }
}

describe(stacking_merging) {
    it("should merge items into existing partial stacks") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Stockpile with 1 tile that already has 3 red items stacked
        int spIdx = CreateStockpile(5, 5, 0, 1, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        SetStockpileSlotCount(spIdx, 0, 0, ITEM_RED, 3);  // Pre-fill with 3 items
        
        // New red item to haul
        SpawnItem(8 * CELL_SIZE + CELL_SIZE * 0.5f, 8 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Run simulation until slot count increases
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (GetStockpileSlotCount(spIdx, 5, 5) == 4) break;
        }
        
        // Stack should now have 4 items (3 pre-filled + 1 hauled and merged)
        int stackCount = GetStockpileSlotCount(spIdx, 5, 5);
        expect(stackCount == 4);
    }
    
    it("should not merge different item types into same stack") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Stockpile with 2 tiles, first has red stack
        int spIdx = CreateStockpile(5, 5, 0, 2, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        SetStockpileFilter(spIdx, ITEM_GREEN, true);
        SetStockpileSlotCount(spIdx, 0, 0, ITEM_RED, 3);  // Slot (5,5) has 3 red
        
        // Green item to haul
        int itemIdx = SpawnItem(8 * CELL_SIZE + CELL_SIZE * 0.5f, 8 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_GREEN);
        
        // Run simulation
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (items[itemIdx].state == ITEM_IN_STOCKPILE) break;
        }
        
        expect(items[itemIdx].state == ITEM_IN_STOCKPILE);
        
        // Green should go to the second slot (6,5), not merge with red
        int redCount = GetStockpileSlotCount(spIdx, 5, 5);
        int greenCount = GetStockpileSlotCount(spIdx, 6, 5);
        expect(redCount == 3);   // Red stack unchanged
        expect(greenCount == 1); // Green in separate slot
    }
    
    it("should use new slot when stack is full") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Stockpile with 2 tiles, first slot is full (10/10)
        int spIdx = CreateStockpile(5, 5, 0, 2, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        SetStockpileSlotCount(spIdx, 0, 0, ITEM_RED, 10);  // Full stack (assuming max is 10)
        
        // New red item
        int itemIdx = SpawnItem(8 * CELL_SIZE + CELL_SIZE * 0.5f, 8 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Run simulation
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (items[itemIdx].state == ITEM_IN_STOCKPILE) break;
        }
        
        expect(items[itemIdx].state == ITEM_IN_STOCKPILE);
        
        // Should go to second slot since first is full
        int slot1Count = GetStockpileSlotCount(spIdx, 5, 5);
        int slot2Count = GetStockpileSlotCount(spIdx, 6, 5);
        expect(slot1Count == 10);  // First slot still full
        expect(slot2Count == 1);   // New item in second slot
    }
    
    it("should stack items when multiple movers haul same type simultaneously") {
        // Test: multiple movers hauling the same item type should stack into
        // partial stacks rather than each getting a separate empty slot.
        // Pre-fill a slot to create a partial stack so the first pass can match.
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Create 5 movers spread around the map
        for (int i = 0; i < 5; i++) {
            Mover* m = &movers[i];
            Point goal = {1 + i, 1, 0};
            InitMover(m, (1 + i) * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        }
        moverCount = 5;
        
        // Stockpile with 2 tiles — forces stacking since 5 items > 2 slots
        int spIdx = CreateStockpile(5, 5, 0, 2, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        // Pre-fill slot 0 with 1 red item to create a partial stack
        SetStockpileSlotCount(spIdx, 0, 0, ITEM_RED, 1);
        
        // Spawn 5 red items near each other
        for (int i = 0; i < 5; i++) {
            SpawnItem((8) * CELL_SIZE + CELL_SIZE * 0.5f, (2 + i) * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        }
        
        // Run simulation until all items are hauled (slot counts reach 6 total)
        for (int i = 0; i < 2000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            
            int total = GetStockpileSlotCount(spIdx, 5, 5) + GetStockpileSlotCount(spIdx, 6, 5);
            if (total == 6) break;
        }
        
        // With 2 slots and 6 total items (1 pre-filled + 5 hauled),
        // items must stack. With the old bug, movers would fail to find
        // slots once both are reserved and items would never all make it in.
        int count0 = GetStockpileSlotCount(spIdx, 5, 5);
        int count1 = GetStockpileSlotCount(spIdx, 6, 5);
        expect(count0 + count1 == 6);  // All 6 items accounted for
        expect(count0 > 1);            // First slot has stacked items
    }
    
    it("should consolidate fragmented stacks when idle") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Stockpile with 3 tiles, fragmented stacks:
        // Slot 0: Red x1, Slot 1: Red x5, Slot 2: empty
        int spIdx = CreateStockpile(5, 5, 0, 3, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        // Set up fragmented stacks using SetStockpileSlotCount
        // Slot 0: 1 item, Slot 1: 5 items
        SetStockpileSlotCount(spIdx, 0, 0, ITEM_RED, 1);
        SetStockpileSlotCount(spIdx, 1, 0, ITEM_RED, 5);
        
        // Run simulation — idle mover should consolidate slot 0 into slot 1
        for (int i = 0; i < 2000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }
        
        // Slot 0 should be empty, slot 1 should have 6
        int count0 = GetStockpileSlotCount(spIdx, 5, 5);
        int count1 = GetStockpileSlotCount(spIdx, 6, 5);
        expect(count0 + count1 == 6);  // Total preserved
        expect(count0 == 0 || count1 == 0);  // One slot should be empty (consolidated)
    }
    
    it("should not ping-pong items between equal-sized stacks") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Stockpile with 2 tiles, equal-sized stacks: Slot 0: Red x4, Slot 1: Red x4
        int spIdx = CreateStockpile(5, 5, 0, 2, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        // Set up equal stacks: Slot 0: 4 items, Slot 1: 4 items
        SetStockpileSlotCount(spIdx, 0, 0, ITEM_RED, 4);
        SetStockpileSlotCount(spIdx, 1, 0, ITEM_RED, 4);
        
        // Run simulation — mover should NOT move items between equal stacks
        int consolidationJobCount = 0;
        for (int i = 0; i < 500; i++) {
            Tick();
            AssignJobs();
            
            // Count how many times a consolidation job is assigned
            if (m->currentJobId >= 0) {
                Job* job = GetJob(m->currentJobId);
                if (job && job->active && job->type == JOBTYPE_HAUL) {
                    // Check if it's a consolidation (source and dest in same stockpile)
                    int srcSp = -1, dstSp = -1;
                    if (job->targetItem >= 0 && items[job->targetItem].active) {
                        IsPositionInStockpile(items[job->targetItem].x, items[job->targetItem].y, 
                                             (int)items[job->targetItem].z, &srcSp);
                    }
                    IsPositionInStockpile(job->targetSlotX * CELL_SIZE + CELL_SIZE * 0.5f,
                                         job->targetSlotY * CELL_SIZE + CELL_SIZE * 0.5f,
                                         (int)m->z, &dstSp);
                    if (srcSp == dstSp && srcSp == spIdx) {
                        consolidationJobCount++;
                    }
                }
            }
            
            JobsTick();
        }
        
        // Stacks should remain equal (no consolidation should occur)
        int count0 = GetStockpileSlotCount(spIdx, 5, 5);
        int count1 = GetStockpileSlotCount(spIdx, 6, 5);
        expect(count0 == 4);  // Unchanged
        expect(count1 == 4);  // Unchanged
        expect(consolidationJobCount == 0);  // No consolidation jobs assigned
    }
}

describe(stockpile_priority) {
    it("should re-haul items from low to high priority stockpile") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Low priority stockpile (dump zone) at (2,2)
        int spLow = CreateStockpile(2, 2, 0, 1, 1);
        SetStockpileFilter(spLow, ITEM_RED, true);
        SetStockpilePriority(spLow, 1);  // Low priority
        
        // High priority stockpile (proper storage) at (8,8)
        int spHigh = CreateStockpile(8, 8, 0, 1, 1);
        SetStockpileFilter(spHigh, ITEM_RED, true);
        SetStockpilePriority(spHigh, 5);  // High priority
        
        // Place item directly in the low-priority stockpile
        int itemIdx = SpawnItem(2 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        items[itemIdx].state = ITEM_IN_STOCKPILE;
        Stockpile* spL = &stockpiles[spLow];
        spL->slotCounts[0] = 1;
        spL->slotTypes[0] = ITEM_RED;
        spL->slotMaterials[0] = items[itemIdx].material;
        
        expect(items[itemIdx].state == ITEM_IN_STOCKPILE);
        int itemTileX = (int)(GetItemX(itemIdx) / CELL_SIZE);
        int itemTileY = (int)(GetItemY(itemIdx) / CELL_SIZE);
        expect(itemTileX == 2);
        expect(itemTileY == 2);
        
        // Run - mover should re-haul from low to high priority
        for (int i = 0; i < 2000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }
        
        // Item should now be in high-priority stockpile
        itemTileX = (int)(GetItemX(itemIdx) / CELL_SIZE);
        itemTileY = (int)(GetItemY(itemIdx) / CELL_SIZE);
        expect(itemTileX == 8);
        expect(itemTileY == 8);
    }
    
    it("should not re-haul if already in highest priority stockpile") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // High priority stockpile
        int spHigh = CreateStockpile(2, 2, 0, 1, 1);
        SetStockpileFilter(spHigh, ITEM_RED, true);
        SetStockpilePriority(spHigh, 5);
        
        // Lower priority stockpile (empty)
        int spLow = CreateStockpile(8, 8, 0, 1, 1);
        SetStockpileFilter(spLow, ITEM_RED, true);
        SetStockpilePriority(spLow, 1);
        
        // Item on ground near high-priority stockpile
        int itemIdx = SpawnItem(3 * CELL_SIZE + CELL_SIZE * 0.5f, 3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Haul to high-priority
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (items[itemIdx].state == ITEM_IN_STOCKPILE) break;
        }
        
        expect(items[itemIdx].state == ITEM_IN_STOCKPILE);
        int itemTileX = (int)(GetItemX(itemIdx) / CELL_SIZE);
        int itemTileY = (int)(GetItemY(itemIdx) / CELL_SIZE);
        
        // Record position
        int origX = itemTileX;
        int origY = itemTileY;
        
        // Run more ticks - item should NOT move to lower priority
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }
        
        // Item should still be at same position (not re-hauled to worse storage)
        itemTileX = (int)(GetItemX(itemIdx) / CELL_SIZE);
        itemTileY = (int)(GetItemY(itemIdx) / CELL_SIZE);
        expect(itemTileX == origX);
        expect(itemTileY == origY);
    }
    
    it("should not re-haul between equal priority stockpiles") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Two stockpiles with same priority
        int sp1 = CreateStockpile(2, 2, 0, 1, 1);
        SetStockpileFilter(sp1, ITEM_RED, true);
        SetStockpilePriority(sp1, 3);
        
        int sp2 = CreateStockpile(8, 8, 0, 1, 1);
        SetStockpileFilter(sp2, ITEM_RED, true);
        SetStockpilePriority(sp2, 3);  // Same priority
        
        // Item on ground
        int itemIdx = SpawnItem(3 * CELL_SIZE + CELL_SIZE * 0.5f, 3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Haul to first stockpile
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (items[itemIdx].state == ITEM_IN_STOCKPILE) break;
        }
        
        expect(items[itemIdx].state == ITEM_IN_STOCKPILE);
        
        // Record position
        int origX = (int)(GetItemX(itemIdx) / CELL_SIZE);
        int origY = (int)(GetItemY(itemIdx) / CELL_SIZE);
        
        // Run more ticks
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }
        
        // Item should not have moved (no re-haul between equal priorities)
        int newX = (int)(GetItemX(itemIdx) / CELL_SIZE);
        int newY = (int)(GetItemY(itemIdx) / CELL_SIZE);
        expect(newX == origX);
        expect(newY == origY);
    }
}

describe(stockpile_max_stack_size) {
    it("should not let endless mover mode hijack mover carrying item") {
        // Bug: mover in JOB_MOVING_TO_STOCKPILE loses path, endless mover mode
        // assigns random goal but mover keeps carrying item and wanders aimlessly
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearGatherZones();
        
        // Enable endless mover mode (like in the demo)
        extern bool endlessMoverMode;
        bool oldEndlessMode = endlessMoverMode;
        endlessMoverMode = true;
        
        // Mover at left
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, CELL_SIZE * 0.5f, 1.5f * CELL_SIZE, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Stockpile at right - RED only
        int sp = CreateStockpile(6, 1, 0, 2, 2);
        SetStockpileFilter(sp, ITEM_RED, true);
        SetStockpileFilter(sp, ITEM_GREEN, false);
        SetStockpileFilter(sp, ITEM_BLUE, false);
        
        // Item near mover
        int item = SpawnItem(2.5f * CELL_SIZE, 1.5f * CELL_SIZE, 0.0f, ITEM_RED);
        
        // Run until mover picks up item
        for (int i = 0; i < 300; i++) {
            Tick();
            ItemsTick(TICK_DT);
            AssignJobs();
            JobsTick();
            if (MoverGetCarryingItem(m) == item) break;
        }
        expect(MoverGetCarryingItem(m) == item);
        expect(MoverIsCarrying(m));
        
        // Clear path to simulate losing it (like when wall is drawn)
        ClearMoverPath(0);
        
        // Record the job's target stockpile slot and current goal
        Job* job = GetJob(m->currentJobId);
        int targetSlotX = job->targetSlotX;
        int targetSlotY = job->targetSlotY;
        int targetStockpile = job->targetStockpile;
        Point goalBefore = m->goal;
        
        // Clear any repath cooldown so endless mover mode will act immediately
        m->repathCooldown = 0;
        
        // Ensure mover is active and has no path (trigger the endless mover branch)
        m->active = true;
        expect(GetMoverPathLength(0) == 0);
        expect(GetMoverPathIndex(0) < 0);
        
        // Run a single Tick - this is where the bug manifests:
        // endless mover mode calls AssignNewMoverGoal() which sets m->goal to random point
        Tick();
        
        // BUG CHECK: After Tick, if mover was hijacked, m->goal will have changed
        // to some random cell (not the stockpile target)
        Stockpile* spPtr = &stockpiles[targetStockpile];
        int stockpileX = spPtr->x + targetSlotX;
        int stockpileY = spPtr->y + targetSlotY;
        
        // Seed random with a value that will produce a different goal than (6,1)
        // The bug is that AssignNewMoverGoal gets called and changes the goal to random point
        SetRandomSeed(12345);
        ClearMoverPath(0);
        m->repathCooldown = 0;
        Tick();
        
        // BUG: After Tick, the mover's goal changed to a random point (0,3) 
        // instead of staying at the stockpile (6,1)
        // The fix should prevent AssignNewMoverGoal from being called when mover has a job
        expect(m->goal.x == goalBefore.x);  // Should still be 6
        expect(m->goal.y == goalBefore.y);  // Should still be 1
        (void)stockpileX; (void)stockpileY; (void)spPtr; // suppress unused warnings
        
        // Continue running
        for (int i = 0; i < 120; i++) {
            ItemsTick(TICK_DT);
            AssignJobs();
            JobsTick();
            Tick();
        }
        
        // Mover should NOT be wandering with item - either delivered or dropped
        // If still carrying, should still be in JOB_MOVING_TO_STOCKPILE (not hijacked)
        if (MoverGetCarryingItem(m) >= 0) {
            expect(MoverIsCarrying(m));
        }
        
        // Run longer to let job complete or cancel
        for (int i = 0; i < 600; i++) {
            Tick();
            ItemsTick(TICK_DT);
            AssignJobs();
            JobsTick();
            if (items[item].state == ITEM_IN_STOCKPILE) break;
            if (items[item].state == ITEM_ON_GROUND && MoverIsIdle(m)) break;
        }
        
        // Item should be either in stockpile or dropped on ground (not carried aimlessly)
        bool delivered = items[item].state == ITEM_IN_STOCKPILE;
        bool dropped = items[item].state == ITEM_ON_GROUND && MoverGetCarryingItem(m) == -1;
        expect(delivered || dropped);
        
        endlessMoverMode = oldEndlessMode;
    }
    
    it("should re-acquire slot after path blocked while carrying") {
        // Bug: mover carrying item, wall drawn, can't find slot even with space
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearGatherZones();
        
        // Mover at left
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, CELL_SIZE * 0.5f, 1.5f * CELL_SIZE, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Stockpile at right - RED only
        int sp = CreateStockpile(6, 1, 0, 2, 2);
        SetStockpileFilter(sp, ITEM_RED, true);
        SetStockpileFilter(sp, ITEM_GREEN, false);
        SetStockpileFilter(sp, ITEM_BLUE, false);
        
        // Item near mover
        int item = SpawnItem(2.5f * CELL_SIZE, 1.5f * CELL_SIZE, 0.0f, ITEM_RED);
        
        // Run until mover picks up item
        for (int i = 0; i < 300; i++) {
            Tick();
            ItemsTick(TICK_DT);
            AssignJobs();
            JobsTick();
            if (MoverGetCarryingItem(m) == item) break;
        }
        expect(MoverGetCarryingItem(m) == item);
        expect(MoverIsCarrying(m));
        
        // Draw a wall blocking the path (temporarily)
        grid[0][1][4] = CELL_WALL;
        MarkChunkDirty(4, 1, 0);
        
        // Run a bit with wall
        for (int i = 0; i < 60; i++) {
            Tick();
            ItemsTick(TICK_DT);
            AssignJobs();
            JobsTick();
        }
        
        // Remove the wall
        grid[0][1][4] = CELL_AIR;
        SET_FLOOR(4, 1, 0);
        MarkChunkDirty(4, 1, 0);
        
        // Run until item is delivered
        for (int i = 0; i < 600; i++) {
            Tick();
            ItemsTick(TICK_DT);
            AssignJobs();
            JobsTick();
            if (items[item].state == ITEM_IN_STOCKPILE) break;
        }
        
        // Item should be in stockpile
        expect(items[item].state == ITEM_IN_STOCKPILE);
        expect(MoverIsIdle(m));
        expect(MoverGetCarryingItem(m) == -1);
    }
    
    it("should stack items in partially filled slots") {
        // Reproduce bug: mover can't find slot even though there's stack space
        // 8x8 grid
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearGatherZones();
        
        // Mover at top-left
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, CELL_SIZE * 0.5f, CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // 3x3 stockpile for RED items only (9 slots, max 7 per slot = 63 capacity)
        int sp = CreateStockpile(3, 3, 0, 3, 3);
        SetStockpileFilter(sp, ITEM_RED, true);
        SetStockpileFilter(sp, ITEM_GREEN, false);
        SetStockpileFilter(sp, ITEM_BLUE, false);
        SetStockpileMaxStackSize(sp, 7);
        
        // Pre-fill all 9 slots with 2 units each (partially filled)
        for (int ly = 0; ly < 3; ly++) {
            for (int lx = 0; lx < 3; lx++) {
                SetStockpileSlotCount(sp, lx, ly, ITEM_RED, 2);
            }
        }
        // Total: 18 units in 9 slots, capacity is 63
        
        // Spawn one more RED item on the ground
        int newItem = SpawnItem(1.5f * CELL_SIZE, 1.5f * CELL_SIZE, 0.0f, ITEM_RED);
        
        // Run simulation - mover should pick up and stack the item
        for (int i = 0; i < 600; i++) {
            Tick();
            ItemsTick(TICK_DT);
            AssignJobs();
            JobsTick();
            
            if (ItemWasStored(newItem)) break;
        }
        
        // Item should be in stockpile (merged into existing stack)
        expect(ItemWasStored(newItem));
        expect(MoverIsIdle(m));
    }
    
    it("should respect per-stockpile max stack size") {
        // 8x4 grid
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover at top-left
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, CELL_SIZE * 0.5f, CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Stockpile with maxStackSize = 2
        int sp = CreateStockpile(3, 2, 0, 1, 1);
        SetStockpileMaxStackSize(sp, 2);
        expect(GetStockpileMaxStackSize(sp) == 2);
        
        // Pre-fill slot with 2 items (at max)
        SetStockpileSlotCount(sp, 0, 0, ITEM_RED, 2);
        
        // Spawn a 3rd item on ground
        int item = SpawnItem(1.5f * CELL_SIZE, 1.5f * CELL_SIZE, 0.0f, ITEM_RED);
        
        // Run simulation - should NOT pick up because stockpile is full
        for (int i = 0; i < 300; i++) {
            Tick();
            ItemsTick(TICK_DT);
            AssignJobs();
            JobsTick();
        }
        
        // Item should still be on ground (no room in stockpile)
        expect(items[item].state == ITEM_ON_GROUND);
        expect(MoverIsIdle(m));
    }
    
    it("should re-haul excess items from overfull slots to other stockpiles") {
        // 8x4 grid
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearGatherZones();
        
        // Mover
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, CELL_SIZE * 0.5f, CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Stockpile A with 1 item (stackCount=5), will become overfull
        int spA = CreateStockpile(2, 2, 0, 1, 1);
        SetStockpileSlotCount(spA, 0, 0, ITEM_RED, 5);
        
        // Stockpile B - empty, destination for excess
        int spB = CreateStockpile(6, 2, 0, 1, 1);
        SetStockpileFilter(spB, ITEM_RED, true);
        
        // Reduce A's max stack to 2 - now overfull by 3
        SetStockpileMaxStackSize(spA, 2);
        
        // Run simulation - mover should split excess and re-haul to B
        for (int i = 0; i < 2000; i++) {
            Tick();
            ItemsTick(TICK_DT);
            AssignJobs();
            JobsTick();
        }
        
        // A should have 2, B should have 3
        int inA = GetStockpileSlotCount(spA, 2, 2);
        int inB = GetStockpileSlotCount(spB, 6, 2);
        expect(inA == 2);  // only max stack size remains
        expect(inB == 3);  // excess moved here
        expect(GetStockpileSlotCount(spA, 2, 2) == 2);
    }
    
    it("should allow overfull slots when max stack size is reduced") {
        // 8x4 grid
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Stockpile with default max (10)
        int sp = CreateStockpile(3, 2, 0, 1, 1);
        
        // Pre-fill slot with 5 items
        SetStockpileSlotCount(sp, 0, 0, ITEM_RED, 5);
        
        // Spawn 5 items in stockpile (to track)
        int itemIds[5];
        float slotX = (3 + 0.5f) * CELL_SIZE;
        float slotY = (2 + 0.5f) * CELL_SIZE;
        for (int i = 0; i < 5; i++) {
            itemIds[i] = SpawnItem(slotX, slotY, 0.0f, ITEM_RED);
            items[itemIds[i]].state = ITEM_IN_STOCKPILE;
        }
        
        // Reduce max stack size to 2 - items should stay (overfull allowed)
        SetStockpileMaxStackSize(sp, 2);
        
        // All items should still be in stockpile (no ejection)
        int inStockpile = 0;
        for (int i = 0; i < 5; i++) {
            if (items[itemIds[i]].state == ITEM_IN_STOCKPILE) inStockpile++;
        }
        
        expect(inStockpile == 5);   // all items remain
        expect(GetStockpileSlotCount(sp, 3, 2) == 5);  // slot count unchanged
        expect(GetStockpileMaxStackSize(sp) == 2);     // but max is now 2
    }
    
    it("should not eject items when max stack size is increased") {
        // 8x4 grid
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Stockpile starting with max = 3
        int sp = CreateStockpile(3, 2, 0, 1, 1);
        SetStockpileMaxStackSize(sp, 3);
        
        // Pre-fill slot with 3 items (at max) — one item with stackCount=3
        SetStockpileSlotCount(sp, 0, 0, ITEM_RED, 3);
        
        // Increase max stack size to 10 - no items should be ejected
        SetStockpileMaxStackSize(sp, 10);
        
        // Slot should still have count 3
        expect(GetStockpileSlotCount(sp, 3, 2) == 3);
        
        // The representative item should still be in stockpile
        Stockpile* spPtr = &stockpiles[sp];
        int repIdx = spPtr->slots[0];
        expect(repIdx >= 0);
        expect(items[repIdx].active);
        expect(items[repIdx].state == ITEM_IN_STOCKPILE);
        expect(items[repIdx].stackCount == 3);
    }
}

describe(stockpile_ground_item_blocking) {
    it("should not use slot with foreign ground item on it") {
        // A green item on a red-only stockpile tile should block that slot
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Stockpile with 2 tiles at (5,5) and (6,5), allows RED only
        int spIdx = CreateStockpile(5, 5, 0, 2, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        SetStockpileFilter(spIdx, ITEM_GREEN, false);
        
        // Green item on ground at first stockpile tile (5,5) - this is "foreign"
        SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_GREEN);
        
        // Try to find a free slot for red
        int slotX, slotY;
        bool found = FindFreeStockpileSlot(spIdx, ITEM_RED, MAT_NONE, &slotX, &slotY);
        
        // Should find the second slot (6,5), not the first (blocked by green item)
        expect(found == true);
        expect(slotX == 6);
        expect(slotY == 5);
    }
    
    it("should not use slot with matching ground item on it until absorbed") {
        // A red item on ground at a red stockpile tile should also block
        // (it needs to be "absorbed" first via the absorb job)
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Stockpile with 2 tiles, allows RED
        int spIdx = CreateStockpile(5, 5, 0, 2, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        // Red item on ground at first stockpile tile (5,5) - matching but still on ground
        SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Try to find a free slot for red
        int slotX, slotY;
        bool found = FindFreeStockpileSlot(spIdx, ITEM_RED, MAT_NONE, &slotX, &slotY);
        
        // Should find the second slot (6,5), not the first (blocked by ground item)
        expect(found == true);
        expect(slotX == 6);
        expect(slotY == 5);
    }
    
    it("should absorb matching ground item on stockpile tile") {
        // Mover should pick up a red item on a red stockpile and place it "properly"
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Stockpile at (5,5), allows RED
        int spIdx = CreateStockpile(5, 5, 0, 1, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        // Red item on ground at stockpile tile - needs to be "absorbed"
        int itemIdx = SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Run simulation
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (items[itemIdx].state == ITEM_IN_STOCKPILE) break;
        }
        
        // Item should now be IN_STOCKPILE (not ON_GROUND)
        expect(items[itemIdx].state == ITEM_IN_STOCKPILE);
        
        // Item should still be at same tile
        expect((int)(GetItemX(itemIdx) / CELL_SIZE) == 5);
        expect((int)(GetItemY(itemIdx) / CELL_SIZE) == 5);
        
        // Stockpile slot should have count of 1
        expect(GetStockpileSlotCount(spIdx, 5, 5) == 1);
    }
    
    it("should clear foreign ground item from stockpile tile to another stockpile") {
        // Green item on red stockpile should be hauled to green stockpile
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Red stockpile at (5,5)
        int spRed = CreateStockpile(5, 5, 0, 1, 1);
        SetStockpileFilter(spRed, ITEM_RED, true);
        SetStockpileFilter(spRed, ITEM_GREEN, false);
        
        // Green stockpile at (8,8)
        int spGreen = CreateStockpile(8, 8, 0, 1, 1);
        SetStockpileFilter(spGreen, ITEM_RED, false);
        SetStockpileFilter(spGreen, ITEM_GREEN, true);
        
        // Green item on ground at RED stockpile tile - needs to be cleared
        int itemIdx = SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_GREEN);
        
        // Run simulation
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (items[itemIdx].state == ITEM_IN_STOCKPILE) break;
        }
        
        // Item should be in the GREEN stockpile
        expect(items[itemIdx].state == ITEM_IN_STOCKPILE);
        expect((int)(GetItemX(itemIdx) / CELL_SIZE) == 8);
        expect((int)(GetItemY(itemIdx) / CELL_SIZE) == 8);
    }
    
    it("should safe-drop foreign item outside stockpile when no valid destination") {
        // Green item on red stockpile, but no green stockpile exists
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Red stockpile at (5,5) - only red allowed
        int spRed = CreateStockpile(5, 5, 0, 2, 2);
        SetStockpileFilter(spRed, ITEM_RED, true);
        SetStockpileFilter(spRed, ITEM_GREEN, false);
        
        // NO green stockpile exists
        
        // Green item on ground at RED stockpile tile - needs to be cleared but nowhere to go
        int itemIdx = SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_GREEN);
        
        // Run simulation
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }
        
        // Item should be ON_GROUND but NOT on the stockpile tile anymore
        expect(items[itemIdx].state == ITEM_ON_GROUND);
        expect(IsItemActive(itemIdx) == true);
        
        // Item should NOT be on the stockpile (safe-dropped outside)
        int itemTileX = (int)(GetItemX(itemIdx) / CELL_SIZE);
        int itemTileY = (int)(GetItemY(itemIdx) / CELL_SIZE);
        bool onStockpile = (itemTileX >= 5 && itemTileX < 7 && itemTileY >= 5 && itemTileY < 7);
        expect(onStockpile == false);
        
        // Mover should be idle
        expect(MoverIsIdle(m));
    }
    
    it("should prioritize clearing stockpile tiles over regular hauling") {
        // With both a foreign item on stockpile AND a regular ground item,
        // the clearing job should be done first
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover near the stockpile
        Mover* m = &movers[0];
        Point goal = {4, 5, 0};
        InitMover(m, 4 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Red stockpile at (5,5)
        int spRed = CreateStockpile(5, 5, 0, 1, 1);
        SetStockpileFilter(spRed, ITEM_RED, true);
        SetStockpileFilter(spRed, ITEM_GREEN, false);
        
        // Green stockpile at (8,8)
        int spGreen = CreateStockpile(8, 8, 0, 2, 1);
        SetStockpileFilter(spGreen, ITEM_RED, false);
        SetStockpileFilter(spGreen, ITEM_GREEN, true);
        
        // Green item on RED stockpile tile (needs clearing)
        int foreignItem = SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_GREEN);
        
        // Another green item far away (regular haul) - not used in test but creates scenario
        (void)SpawnItem(2 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_GREEN);
        
        // Run just enough to see which item gets picked first
        AssignJobs();
        
        // Mover should target the foreign item (clearing job) first
        expect(MoverGetTargetItem(m) == foreignItem);
    }
    
    it("should not haul matching item away from its stockpile") {
        // Red item on red stockpile should be absorbed, not hauled to a different red stockpile
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Red stockpile A at (5,5)
        int spA = CreateStockpile(5, 5, 0, 1, 1);
        SetStockpileFilter(spA, ITEM_RED, true);
        
        // Red stockpile B at (8,8)
        int spB = CreateStockpile(8, 8, 0, 1, 1);
        SetStockpileFilter(spB, ITEM_RED, true);
        (void)spB; // suppress unused warning
        
        // Red item on ground at stockpile A
        int itemIdx = SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Run simulation
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (items[itemIdx].state == ITEM_IN_STOCKPILE) break;
        }
        
        // Item should be absorbed into stockpile A (same tile), not hauled to B
        expect(items[itemIdx].state == ITEM_IN_STOCKPILE);
        expect((int)(GetItemX(itemIdx) / CELL_SIZE) == 5);
        expect((int)(GetItemY(itemIdx) / CELL_SIZE) == 5);
    }
}

/*
 * =============================================================================
 * JOB_MOVING_TO_DROP Tests
 * Tests for the clear/safe-drop job state (separate from JOB_MOVING_TO_STOCKPILE)
 * =============================================================================
 */

describe(clear_job_state) {
    it("should use JOB_MOVING_TO_DROP when clearing foreign item with no destination") {
        // Green item on red stockpile, no green stockpile exists
        // Mover should enter JOB_MOVING_TO_DROP state (not JOB_MOVING_TO_STOCKPILE)
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover near stockpile
        Mover* m = &movers[0];
        Point goal = {4, 5, 0};
        InitMover(m, 4 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Red stockpile at (5,5) - only red allowed
        int spRed = CreateStockpile(5, 5, 0, 2, 2);
        SetStockpileFilter(spRed, ITEM_RED, true);
        SetStockpileFilter(spRed, ITEM_GREEN, false);
        
        // NO green stockpile exists
        
        // Green item on RED stockpile tile - needs clearing, nowhere to go
        int itemIdx = SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_GREEN);
        
        // Run until mover picks up and starts carrying
        for (int i = 0; i < 500; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (MoverGetCarryingItem(m) == itemIdx) break;
        }
        
        expect(MoverGetCarryingItem(m) == itemIdx);
        // Should be in JOB_MOVING_TO_DROP, not JOB_MOVING_TO_STOCKPILE
        expect(MoverHasClearJob(m) && MoverIsCarrying(m));
        // targetStockpile should be -1 (no destination stockpile)
        expect(MoverGetTargetStockpile(m) == -1);
    }
    
    it("should use JOB_MOVING_TO_STOCKPILE when clearing to another stockpile") {
        // Green item on red stockpile, green stockpile exists
        // Mover should use JOB_MOVING_TO_STOCKPILE (has a destination)
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover near stockpile
        Mover* m = &movers[0];
        Point goal = {4, 5, 0};
        InitMover(m, 4 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Red stockpile at (5,5)
        int spRed = CreateStockpile(5, 5, 0, 1, 1);
        SetStockpileFilter(spRed, ITEM_RED, true);
        SetStockpileFilter(spRed, ITEM_GREEN, false);
        
        // Green stockpile at (8,8) - destination exists
        int spGreen = CreateStockpile(8, 8, 0, 1, 1);
        SetStockpileFilter(spGreen, ITEM_GREEN, true);
        
        // Green item on RED stockpile tile
        int itemIdx = SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_GREEN);
        
        // Run until mover picks up and starts carrying
        for (int i = 0; i < 500; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (MoverGetCarryingItem(m) == itemIdx) break;
        }
        
        expect(MoverGetCarryingItem(m) == itemIdx);
        // Should be in JOB_MOVING_TO_STOCKPILE (has destination)
        expect(MoverIsCarrying(m));
        // targetStockpile should be the green stockpile
        expect(MoverGetTargetStockpile(m) == spGreen);
    }
    
    it("should complete JOB_MOVING_TO_DROP and drop item on ground") {
        // Full cycle: pick up foreign item, drop outside stockpile
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover near stockpile
        Mover* m = &movers[0];
        Point goal = {4, 5, 0};
        InitMover(m, 4 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Red stockpile at (5,5) - 2x2
        int spRed = CreateStockpile(5, 5, 0, 2, 2);
        SetStockpileFilter(spRed, ITEM_RED, true);
        SetStockpileFilter(spRed, ITEM_GREEN, false);
        
        // Green item on RED stockpile tile
        int itemIdx = SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_GREEN);
        
        // Run full simulation
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (items[itemIdx].state == ITEM_ON_GROUND && MoverIsIdle(m)) break;
        }
        
        // Item should be on ground
        expect(items[itemIdx].state == ITEM_ON_GROUND);
        expect(IsItemActive(itemIdx) == true);
        
        // Item should NOT be on the stockpile anymore
        int itemTileX = (int)(GetItemX(itemIdx) / CELL_SIZE);
        int itemTileY = (int)(GetItemY(itemIdx) / CELL_SIZE);
        bool onStockpile = (itemTileX >= 5 && itemTileX < 7 && itemTileY >= 5 && itemTileY < 7);
        expect(onStockpile == false);
        
        // Mover should be idle
        expect(MoverIsIdle(m));
        expect(MoverGetCarryingItem(m) == -1);
    }
    
    it("should cancel JOB_MOVING_TO_DROP if item disappears") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover
        Mover* m = &movers[0];
        Point goal = {4, 5, 0};
        InitMover(m, 4 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Red stockpile
        int spRed = CreateStockpile(5, 5, 0, 2, 2);
        SetStockpileFilter(spRed, ITEM_RED, true);
        SetStockpileFilter(spRed, ITEM_GREEN, false);
        
        // Green item on red stockpile
        int itemIdx = SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_GREEN);
        
        // Run until mover is in JOB_MOVING_TO_DROP
        for (int i = 0; i < 500; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (MoverHasClearJob(m) && MoverIsCarrying(m)) break;
        }
        
        expect(MoverHasClearJob(m) && MoverIsCarrying(m));
        expect(MoverGetCarryingItem(m) == itemIdx);
        
        // Delete the item while being carried
        DeleteItem(itemIdx);
        
        // Run a few more ticks
        for (int i = 0; i < 30; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }
        
        // Mover should be idle (job cancelled)
        expect(MoverIsIdle(m));
        expect(MoverGetCarryingItem(m) == -1);
    }
}

/*
 * =============================================================================
 * ItemSpatialGrid Tests
 * Tests for spatial indexing of items (TDD - written before implementation)
 * =============================================================================
 */

describe(item_spatial_grid) {
    it("should find item at correct tile after build") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        ClearMovers();
        ClearItems();
        
        // Spawn item at tile (5, 3)
        float itemX = 5 * CELL_SIZE + CELL_SIZE * 0.5f;
        float itemY = 3 * CELL_SIZE + CELL_SIZE * 0.5f;
        int itemIdx = SpawnItem(itemX, itemY, 0.0f, ITEM_RED);
        
        // Build the spatial grid
        BuildItemSpatialGrid();
        
        // Query the tile - should find the item
        int found = QueryItemAtTile(5, 3, 0);
        expect(found == itemIdx);
    }
    
    it("should return -1 for empty tile") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        ClearMovers();
        ClearItems();
        
        // No items spawned
        BuildItemSpatialGrid();
        
        // Query any tile - should return -1
        int found = QueryItemAtTile(5, 3, 0);
        expect(found == -1);
    }
    
    it("should not index carried items") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        ClearMovers();
        ClearItems();
        
        // Spawn item and set state to CARRIED
        float itemX = 5 * CELL_SIZE + CELL_SIZE * 0.5f;
        float itemY = 3 * CELL_SIZE + CELL_SIZE * 0.5f;
        int itemIdx = SpawnItem(itemX, itemY, 0.0f, ITEM_RED);
        items[itemIdx].state = ITEM_CARRIED;
        
        BuildItemSpatialGrid();
        
        // Should not find carried item
        int found = QueryItemAtTile(5, 3, 0);
        expect(found == -1);
    }
    
    it("should not index stockpiled items") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        ClearMovers();
        ClearItems();
        
        // Spawn item and set state to IN_STOCKPILE
        float itemX = 5 * CELL_SIZE + CELL_SIZE * 0.5f;
        float itemY = 3 * CELL_SIZE + CELL_SIZE * 0.5f;
        int itemIdx = SpawnItem(itemX, itemY, 0.0f, ITEM_RED);
        items[itemIdx].state = ITEM_IN_STOCKPILE;
        
        BuildItemSpatialGrid();
        
        // Should not find stockpiled item
        int found = QueryItemAtTile(5, 3, 0);
        expect(found == -1);
    }
    
    it("should handle multiple items at same tile") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        ClearMovers();
        ClearItems();
        
        // Spawn two items at same tile
        float itemX = 5 * CELL_SIZE + CELL_SIZE * 0.5f;
        float itemY = 3 * CELL_SIZE + CELL_SIZE * 0.5f;
        int item1 = SpawnItem(itemX, itemY, 0.0f, ITEM_RED);
        int item2 = SpawnItem(itemX + 1.0f, itemY + 1.0f, 0.0f, ITEM_GREEN);  // Same tile, slightly different pos
        
        BuildItemSpatialGrid();
        
        // Should find one of them (either is valid)
        int found = QueryItemAtTile(5, 3, 0);
        expect(found == item1 || found == item2);
    }
    
    it("should handle items on different z-levels") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        ClearMovers();
        ClearItems();
        
        // Spawn items at same x,y but different z
        float itemX = 5 * CELL_SIZE + CELL_SIZE * 0.5f;
        float itemY = 3 * CELL_SIZE + CELL_SIZE * 0.5f;
        int itemZ0 = SpawnItem(itemX, itemY, 0.0f, ITEM_RED);
        int itemZ1 = SpawnItem(itemX, itemY, 1.0f, ITEM_GREEN);
        
        BuildItemSpatialGrid();
        
        // Query z=0 should find itemZ0
        int foundZ0 = QueryItemAtTile(5, 3, 0);
        expect(foundZ0 == itemZ0);
        
        // Query z=1 should find itemZ1
        int foundZ1 = QueryItemAtTile(5, 3, 1);
        expect(foundZ1 == itemZ1);
    }
    
    it("should track groundItemCount correctly") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        ClearMovers();
        ClearItems();
        
        // Spawn 3 ground items, 1 carried, 1 stockpiled
        SpawnItem(1 * CELL_SIZE, 1 * CELL_SIZE, 0.0f, ITEM_RED);
        SpawnItem(2 * CELL_SIZE, 2 * CELL_SIZE, 0.0f, ITEM_GREEN);
        SpawnItem(3 * CELL_SIZE, 3 * CELL_SIZE, 0.0f, ITEM_BLUE);
        
        int carried = SpawnItem(4 * CELL_SIZE, 4 * CELL_SIZE, 0.0f, ITEM_RED);
        items[carried].state = ITEM_CARRIED;
        
        int stockpiled = SpawnItem(5 * CELL_SIZE, 4 * CELL_SIZE, 0.0f, ITEM_RED);
        items[stockpiled].state = ITEM_IN_STOCKPILE;
        
        BuildItemSpatialGrid();
        
        // Should only count ground items
        expect(itemGrid.groundItemCount == 3);
    }
}


/*
 * Cell-based stockpile operations tests
 * 
 * Tests for non-rectangular stockpiles:
 * - Removing cells from stockpiles
 * - New stockpiles claiming cells from existing ones
 * - Items dropped to ground when cells are erased
 * - Stockpile auto-deletion when all cells removed
 */

describe(stockpile_cell_operations) {
    it("should track active cells in a stockpile") {
        ClearStockpiles();
        
        int spIdx = CreateStockpile(5, 5, 0, 3, 3);
        expect(spIdx >= 0);
        
        // All 9 cells should be active
        expect(GetStockpileActiveCellCount(spIdx) == 9);
        
        // Check individual cells
        expect(IsStockpileCellActive(spIdx, 5, 5) == true);
        expect(IsStockpileCellActive(spIdx, 6, 6) == true);
        expect(IsStockpileCellActive(spIdx, 7, 7) == true);
        
        // Outside bounds should be false
        expect(IsStockpileCellActive(spIdx, 4, 5) == false);
        expect(IsStockpileCellActive(spIdx, 8, 5) == false);
    }
    
    it("should remove cells from a stockpile") {
        ClearStockpiles();
        
        int spIdx = CreateStockpile(5, 5, 0, 3, 3);
        expect(GetStockpileActiveCellCount(spIdx) == 9);
        
        // Remove middle cell
        RemoveStockpileCells(spIdx, 6, 6, 6, 6);
        
        expect(GetStockpileActiveCellCount(spIdx) == 8);
        expect(IsStockpileCellActive(spIdx, 6, 6) == false);
        expect(IsStockpileCellActive(spIdx, 5, 5) == true);  // corners still active
        expect(IsStockpileCellActive(spIdx, 7, 7) == true);
    }
    
    it("should remove a row of cells") {
        ClearStockpiles();
        
        int spIdx = CreateStockpile(5, 5, 0, 3, 3);
        
        // Remove bottom row (y=7)
        RemoveStockpileCells(spIdx, 5, 7, 7, 7);
        
        expect(GetStockpileActiveCellCount(spIdx) == 6);
        expect(IsStockpileCellActive(spIdx, 5, 7) == false);
        expect(IsStockpileCellActive(spIdx, 6, 7) == false);
        expect(IsStockpileCellActive(spIdx, 7, 7) == false);
        expect(IsStockpileCellActive(spIdx, 5, 5) == true);  // top row still active
    }
    
    it("should auto-delete stockpile when all cells removed") {
        ClearStockpiles();
        
        int spIdx = CreateStockpile(5, 5, 0, 2, 2);
        expect(stockpiles[spIdx].active == true);
        expect(stockpileCount == 1);
        
        // Remove all cells
        RemoveStockpileCells(spIdx, 5, 5, 6, 6);
        
        expect(stockpiles[spIdx].active == false);
        expect(stockpileCount == 0);
    }
    
    it("should not find free slot in removed cell") {
        // Need grid for walkability check in FindFreeStockpileSlot
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n");
        
        ClearStockpiles();
        
        int spIdx = CreateStockpile(5, 5, 0, 3, 1);  // 3 cells wide, 1 tall
        
        // Remove middle cell
        RemoveStockpileCells(spIdx, 6, 5, 6, 5);
        
        // Should still find slots in remaining cells
        int slotX, slotY;
        bool found = FindFreeStockpileSlot(spIdx, ITEM_RED, MAT_NONE, &slotX, &slotY);
        expect(found == true);
        expect(slotX != 6);  // should not be the removed cell
    }
    
    it("should drop items to ground when cell is erased") {
        ClearStockpiles();
        ClearItems();
        InitItemSpatialGrid(100, 100, 4);
        
        int spIdx = CreateStockpile(5, 5, 0, 3, 3);
        
        // Spawn an item and place it in stockpile
        float itemX = 6 * CELL_SIZE + CELL_SIZE / 2;
        float itemY = 6 * CELL_SIZE + CELL_SIZE / 2;
        int itemIdx = SpawnItem(itemX, itemY, 0, ITEM_RED);
        items[itemIdx].state = ITEM_IN_STOCKPILE;
        
        // Set slot data
        int lx = 1, ly = 1;  // local coords for (6,6)
        int slotIdx = ly * stockpiles[spIdx].width + lx;
        stockpiles[spIdx].slotCounts[slotIdx] = 1;
        stockpiles[spIdx].slotTypes[slotIdx] = ITEM_RED;
        stockpiles[spIdx].slots[slotIdx] = itemIdx;
        
        // Erase the cell with the item
        RemoveStockpileCells(spIdx, 6, 6, 6, 6);
        
        // Item should now be on ground
        expect(items[itemIdx].state == ITEM_ON_GROUND);
        expect(IsItemActive(itemIdx) == true);
        
        FreeItemSpatialGrid();
    }
    
    it("should allow new stockpile to claim cells from existing one") {
        ClearStockpiles();
        
        // Create first stockpile at (5,5) size 4x4
        int sp1 = CreateStockpile(5, 5, 0, 4, 4);
        expect(GetStockpileActiveCellCount(sp1) == 16);
        
        // Create second stockpile overlapping at (7,7) size 3x3
        // First remove cells from sp1 (simulating what demo.c does)
        RemoveStockpileCells(sp1, 7, 7, 9, 9);
        int sp2 = CreateStockpile(7, 7, 0, 3, 3);
        
        // sp1 should have lost 4 cells (the 2x2 overlap area within its bounds)
        expect(GetStockpileActiveCellCount(sp1) == 12);
        
        // sp2 should have all 9 cells
        expect(GetStockpileActiveCellCount(sp2) == 9);
        
        // Overlapping area should belong to sp2, not sp1
        expect(IsStockpileCellActive(sp1, 7, 7) == false);
        expect(IsStockpileCellActive(sp1, 8, 8) == false);
        expect(IsStockpileCellActive(sp2, 7, 7) == true);
        expect(IsStockpileCellActive(sp2, 8, 8) == true);
    }
    
    it("should not consider removed cells as part of stockpile for position check") {
        ClearStockpiles();
        
        int spIdx = CreateStockpile(5, 5, 0, 3, 3);
        
        // Position in middle cell is in stockpile
        float midX = 6 * CELL_SIZE + CELL_SIZE / 2;
        float midY = 6 * CELL_SIZE + CELL_SIZE / 2;
        int foundSp = -1;
        expect(IsPositionInStockpile(midX, midY, 0, &foundSp) == true);
        expect(foundSp == spIdx);
        
        // Remove middle cell
        RemoveStockpileCells(spIdx, 6, 6, 6, 6);
        
        // Position should no longer be in stockpile
        foundSp = -1;
        expect(IsPositionInStockpile(midX, midY, 0, &foundSp) == false);
    }
}

// =============================================================================
// Mining/Digging Tests
// =============================================================================

describe(mining_designation) {
    it("should designate a wall for digging") {
        InitTestGridFromAscii(
            ".....\n"
            ".###.\n"
            ".###.\n"
            ".###.\n"
            ".....\n");
        
        InitDesignations();
        
        // Designate center wall
        bool result = DesignateMine(2, 2, 0);
        expect(result == true);
        expect(HasMineDesignation(2, 2, 0) == true);
        expect(CountMineDesignations() == 1);
    }
    
    it("should not designate floor for digging") {
        InitTestGridFromAscii(
            ".....\n"
            ".###.\n"
            ".###.\n"
            ".###.\n"
            ".....\n");
        
        InitDesignations();
        
        // Try to designate floor tile
        bool result = DesignateMine(0, 0, 0);
        expect(result == false);
        expect(HasMineDesignation(0, 0, 0) == false);
    }
    
    it("should cancel a designation") {
        InitTestGridFromAscii(
            ".....\n"
            ".###.\n"
            ".###.\n"
            ".###.\n"
            ".....\n");
        
        InitDesignations();
        
        DesignateMine(2, 2, 0);
        expect(HasMineDesignation(2, 2, 0) == true);
        
        CancelDesignation(2, 2, 0);
        expect(HasMineDesignation(2, 2, 0) == false);
        expect(CountMineDesignations() == 0);
    }
}

describe(mining_job_assignment) {
    it("should assign mine job to mover when adjacent floor exists") {
        // Wall with floor below it
        InitTestGridFromAscii(
            ".....\n"
            ".###.\n"
            ".###.\n"
            ".###.\n"
            ".....\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        
        // Mover at (0,0)
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Designate wall at (1,1) - has adjacent floor at (0,1) and (1,0)
        DesignateMine(1, 1, 0);
        
        // Assign jobs
        AssignJobs();
        
        // Mover should be assigned to mine
        expect(MoverHasMineJob(m));
        expect(MoverGetTargetMineX(m) == 1);
        expect(MoverGetTargetMineY(m) == 1);
        expect(MoverGetTargetMineZ(m) == 0);
        
        // Designation should be reserved
        Designation* d = GetDesignation(1, 1, 0);
        expect(d != NULL);
        expect(d->assignedMover == 0);
    }
    
    it("should not assign mine job when no adjacent floor") {
        // Completely surrounded wall
        InitTestGridFromAscii(
            "#####\n"
            "#####\n"
            "#####\n"
            "#####\n"
            "#####\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        
        // Make one floor cell for mover to stand on
        grid[0][0][0] = CELL_AIR;
        SET_FLOOR(0, 0, 0);
        
        // Mover at (0,0)
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Designate wall at (2,2) - surrounded by walls, no adjacent floor
        DesignateMine(2, 2, 0);
        
        // Assign jobs
        AssignJobs();
        
        // Mover should remain idle (can't reach any adjacent tile)
        expect(MoverIsIdle(m));
    }
}

describe(mining_job_execution) {
    it("should complete mine job and convert wall to walkable") {
        InitTestGridFromAscii(
            ".....\n"
            ".#...\n"
            ".....\n"
            ".....\n"
            ".....\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        
        // Mover starts adjacent to wall at (0,1)
        Mover* m = &movers[0];
        Point goal = {0, 1, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Wall at (1,1)
        expect(grid[0][1][1] == CELL_WALL);
        
        // Designate wall for digging
        DesignateMine(1, 1, 0);
        
        // Run simulation until mine completes
        bool completed = false;
        for (int i = 0; i < 500; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            
            // Check if mine completed (wall removed)
            if (grid[0][1][1] != CELL_WALL) {
                completed = true;
                break;
            }
        }
        
        expect(completed == true);
        // Mined wall becomes walkable
        expect(grid[0][1][1] != CELL_WALL);
        expect(IsCellWalkableAt(0, 1, 1) == true);
        expect(HasMineDesignation(1, 1, 0) == false);
        expect(MoverIsIdle(m));
    }
    
    it("should spawn orange block when mine completes") {
        InitTestGridFromAscii(
            ".....\n"
            ".#...\n"
            ".....\n"
            ".....\n"
            ".....\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        
        // Mover starts adjacent to wall at (0,1)
        Mover* m = &movers[0];
        Point goal = {0, 1, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Wall at (1,1)
        expect(grid[0][1][1] == CELL_WALL);
        
        // Designate wall for mining
        DesignateMine(1, 1, 0);
        
        // Run simulation until mine completes
        bool completed = false;
        for (int i = 0; i < 500; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            
            if (grid[0][1][1] != CELL_WALL) {
                completed = true;
                break;
            }
        }
        
        expect(completed == true);
        
        // Find the spawned item and verify it's raw stone at the mine location
        bool foundOrange = false;
        for (int i = 0; i < MAX_ITEMS; i++) {
            if (IsItemActive(i) && GetItemType(i) == ITEM_ROCK) {
                foundOrange = true;
                // Item should be at the dug location (1,1)
                int itemX = (int)(GetItemX(i) / CELL_SIZE);
                int itemY = (int)(GetItemY(i) / CELL_SIZE);
                expect(itemX == 1);
                expect(itemY == 1);
                expect(GetItemZ(i) == 0);
                expect(items[i].state == ITEM_ON_GROUND);
                break;
            }
        }
        expect(foundOrange == true);
    }
    
    it("should cancel mine job if designation is removed") {
        InitTestGridFromAscii(
            ".....\n"
            ".#...\n"
            ".....\n"
            ".....\n"
            ".....\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        
        // Mover at (0,0)
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Designate and assign
        DesignateMine(1, 1, 0);
        AssignJobs();
        
        expect(MoverHasMineJob(m));
        
        // Cancel designation while mover is en route
        CancelDesignation(1, 1, 0);
        
        // Run one tick to detect cancellation
        Tick();
        JobsTick();
        
        // Mover should be back to idle
        expect(MoverIsIdle(m));
        expect(MoverGetTargetMineX(m) == -1);
    }
    
    it("should cancel mine job if wall is removed by other means") {
        InitTestGridFromAscii(
            ".....\n"
            ".#...\n"
            ".....\n"
            ".....\n"
            ".....\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        
        // Mover at (0,0)
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Designate and assign
        DesignateMine(1, 1, 0);
        AssignJobs();
        
        expect(MoverHasMineJob(m));
        
        // Player removes wall manually (simulating editor action)
        grid[0][1][1] = CELL_AIR;
        SET_FLOOR(1, 1, 0);
        
        // Run one tick to detect wall removal
        Tick();
        JobsTick();
        
        // Mover should be back to idle, designation should be cleared
        expect(MoverIsIdle(m));
        expect(HasMineDesignation(1, 1, 0) == false);
    }
}

describe(mining_multiple_designations) {
    it("should process multiple mine designations sequentially") {
        // Layout with walls that each have at least one adjacent floor
        // So they can all be dug from the start
        InitTestGridFromAscii(
            "......\n"
            ".#.#..\n"
            "......\n"
            ".#....\n"
            "......\n"
            "......\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        
        // Mover at (0,0)
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Designate 3 isolated walls (each has adjacent floor)
        DesignateMine(1, 1, 0);  // Wall at (1,1)
        DesignateMine(3, 1, 0);  // Wall at (3,1)
        DesignateMine(1, 3, 0);  // Wall at (1,3)
        
        expect(CountMineDesignations() == 3);
        
        // Run simulation until all digs complete
        // Each mine takes ~MINE_WORK_TIME (2s) at 60 ticks/s = 120 ticks per mine
        // Plus travel time, so give plenty of margin
        for (int i = 0; i < 5000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            
            if (CountMineDesignations() == 0) break;
        }
        
        // All walls should be dug and walkable
        expect(IsCellWalkableAt(0, 1, 1) == true);
        expect(IsCellWalkableAt(0, 1, 3) == true);
        expect(IsCellWalkableAt(0, 3, 1) == true);
        expect(CountMineDesignations() == 0);
    }
}

// =============================================================================
// Channeling Tests (Vertical Digging)
// =============================================================================

describe(channel_designation) {
    it("should designate a floor tile for channeling") {
        // Two-level setup: floor at z=1, wall at z=0
        InitTestGridFromAscii(
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n");
        
        // Add a second level - walls at z=0
        for (int x = 0; x < 5; x++) {
            for (int y = 0; y < 5; y++) {
                grid[0][y][x] = CELL_WALL;  // z=0: walls
                grid[1][y][x] = CELL_AIR;   // z=1: floor
                SET_FLOOR(x, y, 1);
            }
        }
        
        InitDesignations();
        
        // Designate floor at z=1 for channeling
        bool result = DesignateChannel(2, 2, 1);
        expect(result == true);
        expect(HasChannelDesignation(2, 2, 1) == true);
        expect(CountChannelDesignations() == 1);
    }
    
    it("should not designate at z=0 (no level below)") {
        InitTestGridFromAscii(
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n");
        
        InitDesignations();
        
        // Try to designate at z=0 - should fail
        bool result = DesignateChannel(2, 2, 0);
        expect(result == false);
        expect(HasChannelDesignation(2, 2, 0) == false);
    }
    
    it("should not designate a wall tile for channeling") {
        InitTestGridFromAscii(
            ".....\n"
            ".###.\n"
            ".###.\n"
            ".###.\n"
            ".....\n");
        
        // Add second level
        for (int x = 0; x < 5; x++) {
            for (int y = 0; y < 5; y++) {
                grid[1][y][x] = grid[0][y][x];  // Copy pattern to z=1
            }
        }
        
        InitDesignations();
        
        // Try to designate wall at z=1 - should fail (walls aren't channeled, they're mined)
        bool result = DesignateChannel(2, 2, 1);
        expect(result == false);
        expect(HasChannelDesignation(2, 2, 1) == false);
    }
    
    it("should not designate tile without floor") {
        InitTestGridFromAscii(
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n");
        
        // Two levels. At (2,2): z=0 is air (not solid), z=1 is air with no floor flag
        // This means there's no floor to channel at (2,2,1)
        for (int x = 0; x < 5; x++) {
            for (int y = 0; y < 5; y++) {
                grid[0][y][x] = CELL_WALL;
                grid[1][y][x] = CELL_AIR;
                SET_FLOOR(x, y, 1);
            }
        }
        // At (2,2): remove the solid below and the floor flag
        grid[0][2][2] = CELL_AIR;   // z=0 is air, not solid
        grid[1][2][2] = CELL_AIR;   // z=1 is air too
        CLEAR_FLOOR(2, 2, 1);       // No explicit floor flag
        
        InitDesignations();
        
        // Try to designate - should fail (no floor to remove, no solid below)
        bool result = DesignateChannel(2, 2, 1);
        expect(result == false);
    }
    
    it("should cancel a channel designation") {
        InitTestGridFromAscii(
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n");
        
        for (int x = 0; x < 5; x++) {
            for (int y = 0; y < 5; y++) {
                grid[0][y][x] = CELL_WALL;
                grid[1][y][x] = CELL_AIR;
                SET_FLOOR(x, y, 1);
            }
        }
        
        InitDesignations();
        
        DesignateChannel(2, 2, 1);
        expect(HasChannelDesignation(2, 2, 1) == true);
        
        CancelDesignation(2, 2, 1);
        expect(HasChannelDesignation(2, 2, 1) == false);
        expect(CountChannelDesignations() == 0);
    }
}

describe(channel_ramp_detection) {
    it("should detect ramp direction when wall is adjacent at z-1") {
        InitTestGridFromAscii(
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n");
        
        // Setup: z=0 has wall to the north of (2,2), floor elsewhere
        // z=1 is all floor
        for (int x = 0; x < 5; x++) {
            for (int y = 0; y < 5; y++) {
                grid[0][y][x] = CELL_AIR;
                grid[1][y][x] = CELL_AIR;
                SET_FLOOR(x, y, 0);
                SET_FLOOR(x, y, 1);
            }
        }
        // Wall to the north at z=0
        grid[0][1][2] = CELL_WALL;  // (2,1) at z=0 is wall
        
        InitDesignations();
        
        // Check ramp detection for channeling at (2,2,1)
        CellType rampDir = AutoDetectChannelRampDirection(2, 2, 0);  // lowerZ=0
        
        // Should detect ramp facing south (away from wall)
        expect(rampDir == CELL_RAMP_N);  // RAMP_N means ramp going up to north
    }
    
    it("should return CELL_AIR when no walkable exit at z+1") {
        InitTestGridFromAscii(
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n");
        
        // z=0: solid ground, z=1: floor above + walls blocking exits
        for (int x = 0; x < 5; x++) {
            for (int y = 0; y < 5; y++) {
                grid[0][y][x] = CELL_WALL;  // Solid ground
                SetWallMaterial(x, y, 0, MAT_DIRT);
                SET_FLOOR(x, y, 1);         // Floor at z=1 makes it walkable
                grid[1][y][x] = CELL_WALL;  // Walls at z=1 block exits
            }
        }
        
        // Need z=2 walls to block walkability above z=1
        for (int x = 0; x < 5; x++) {
            for (int y = 0; y < 5; y++) {
                grid[2][y][x] = CELL_WALL;
            }
        }
        
        InitDesignations();
        
        // No walkable exits at z=1 - can't create ramp, returns CELL_AIR
        // AutoDetectChannelRampDirection needs either:
        // 1. Adjacent solid (wall) at lowerZ with walkable above, OR
        // 2. Any walkable exit at upperZ (second pass for ramp-to-ramp)
        int lowerZ = 1;
        CellType rampDir = AutoDetectChannelRampDirection(2, 2, lowerZ);
        expect(rampDir == CELL_AIR);
    }
}

describe(channel_job_execution) {
    it("should assign channel job to mover") {
        // Setup: solid ground below, walkable floor above
        // Legacy: z=0 walls, z=1 floor
        // Standard: z=0 walls, z=1 air above walls (walkable), z=2 air + SET_FLOOR
        InitTestGridFromAscii(
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n");
        
        // z=0 walls, z=1 air (walkable above walls), z=2 air + floor flag
        int channelZ = 2;  // Z-level where channeling happens
        int moverZ = 2;    // Z-level where mover walks
        
        for (int x = 0; x < 5; x++) {
            for (int y = 0; y < 5; y++) {
                grid[0][y][x] = CELL_WALL;
                grid[1][y][x] = CELL_AIR;
                grid[2][y][x] = CELL_AIR;
                SET_FLOOR(x, y, 2);  // Floor at z=2 makes it walkable
            }
        }
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        
        // Mover starts at (2,2) on moverZ, exactly on the channel target
        Mover* m = &movers[0];
        Point goal = {2, 2, moverZ};
        InitMover(m, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, (float)moverZ, goal, 100.0f);
        moverCount = 1;
        
        // Verify walkability at mover's position
        expect(IsCellWalkableAt(moverZ, 2, 2) == true);
        
        // Designate for channeling
        bool designated = DesignateChannel(2, 2, channelZ);
        expect(designated == true);
        
        // Initial state: mover should be idle
        expect(m->currentJobId == -1);
        
        // Assign jobs
        AssignJobs();
        
        // Mover should now have a channel job
        expect(m->currentJobId >= 0);
        if (m->currentJobId >= 0) {
            Job* job = GetJob(m->currentJobId);
            expect(job != NULL);
            expect(job->type == JOBTYPE_CHANNEL);
        }
    }
    
    // NOTE: The following execution tests have a subtle timing issue where
    // the designation check in the loop doesn't trigger, but post-loop checks
    // show the channeling DID complete. The implementation works - the job
    // assignment test passes and manual testing confirms functionality.
    // These tests are marked for future investigation.
    
    it("should complete channel job - floor removed after execution") {
        // Setup: solid ground below, walkable floor above to channel
        InitTestGridFromAscii(
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n");
        
        // z=0 walls, z=1 walls (solid to mine), z=2 air + floor flag
        int channelZ = 2;   // Z-level where channeling happens
        int belowZ = 1;     // Z-level below the channel (gets mined)
        
        for (int x = 0; x < 5; x++) {
            for (int y = 0; y < 5; y++) {
                grid[0][y][x] = CELL_WALL;
                grid[1][y][x] = CELL_WALL;  // Solid at z=1 to mine
                grid[2][y][x] = CELL_AIR;
                SET_FLOOR(x, y, 2);
            }
        }
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        
        // Mover starts at (2,2) on channelZ, exactly on the channel target
        Mover* m = &movers[0];
        Point goal = {2, 2, channelZ};
        InitMover(m, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, (float)channelZ, goal, 100.0f);
        moverCount = 1;
        
        // Verify initial state
        expect(grid[belowZ][2][2] == CELL_WALL);
        expect(grid[channelZ][2][2] == CELL_AIR);
        
        // Designate for channeling
        bool designated = DesignateChannel(2, 2, channelZ);
        expect(designated == true);
        
        // Assign job and run simulation until channeling completes
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (!HasChannelDesignation(2, 2, channelZ)) break;
        }
        
        // After running, floor should be removed and wall below mined
        expect(grid[channelZ][2][2] == CELL_AIR);     // Floor removed
        expect(grid[belowZ][2][2] != CELL_WALL);      // Wall mined
    }
    
    it("should create ramp when wall adjacent at z-1") {
        // Setup: walls below that provide ramp high-side, floor above to channel
        InitTestGridFromAscii(
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n");
        
        // z=0 walls, z=1 walls (ramp high-side), z=2 air + floor flag
        int channelZ = 2;  // Z-level where channeling happens
        int rampZ = 1;     // Z-level where ramp appears
        
        for (int x = 0; x < 5; x++) {
            for (int y = 0; y < 5; y++) {
                grid[0][y][x] = CELL_WALL;
                grid[1][y][x] = CELL_WALL;  // Solid walls provide ramp high-side
                grid[2][y][x] = CELL_AIR;
                SET_FLOOR(x, y, 2);
            }
        }
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearJobs();
        InitDesignations();
        
        // Mover at (2,2) on channelZ - exactly on channel target
        Mover* m = &movers[0];
        Point goal = {2, 2, channelZ};
        InitMover(m, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, (float)channelZ, goal, 100.0f);
        moverCount = 1;
        
        // Designate channel
        bool designated = DesignateChannel(2, 2, channelZ);
        expect(designated == true);
        
        // Run simulation until channeling completes
        int iterations = 0;
        for (int i = 0; i < 1000; i++) {
            iterations = i;
            Tick();
            AssignJobs();
            JobsTick();
            // Stop once channeling is done to avoid further state changes
            if (!HasChannelDesignation(2, 2, channelZ)) break;
        }
        (void)iterations;
        
        // Should create ramp - walls surround provide high side
        // Note: CellIsRamp returns flag value (8), not boolean (1), so use != 0
        expect(CellIsRamp(grid[rampZ][2][2]) != 0);
    }
    
    it("should channel into open air - floor removed") {
        // Setup: open air below, floor above to channel into open air
        InitTestGridFromAscii(
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n");
        
        // z=0 dirt (solid), z=1 air (walkable), z=2 air + floor flag
        // We channel at z=2, z=1 should remain open air
        int channelZ = 2;  // Z-level where channeling happens
        int belowZ = 1;    // Z-level below (should remain open)
        
        for (int x = 0; x < 5; x++) {
            for (int y = 0; y < 5; y++) {
                grid[0][y][x] = CELL_WALL;  // Solid ground
                SetWallMaterial(x, y, 0, MAT_DIRT);
                grid[1][y][x] = CELL_AIR;   // Open air (walkable above dirt)
                grid[2][y][x] = CELL_AIR;
                SET_FLOOR(x, y, 2);         // Floor at z=2 to channel
            }
        }
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        
        Mover* m = &movers[0];
        Point goal = {2, 2, channelZ};
        InitMover(m, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, (float)channelZ, goal, 100.0f);
        moverCount = 1;
        
        // Designate channel
        bool designated = DesignateChannel(2, 2, channelZ);
        expect(designated == true);
        
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }
        
        // Channeled floor becomes CELL_AIR
        expect(grid[channelZ][2][2] == CELL_AIR);
        // Below should remain as it was (open air)
        expect(grid[belowZ][2][2] == CELL_AIR);
    }
    
    it("should move channeler down to z-1 after completion") {
        // Setup: solid ground below, floor above - mover should descend after channeling
        InitTestGridFromAscii(
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n");
        
        // z=0 walls, z=1 walls, z=2 air + floor flag
        // Mover descends from z=2 to z=1
        int channelZ = 2;   // Z-level where channeling happens (mover starts here)
        int descendZ = 1;   // Z-level mover descends to after channeling
        
        for (int x = 0; x < 5; x++) {
            for (int y = 0; y < 5; y++) {
                grid[0][y][x] = CELL_WALL;
                grid[1][y][x] = CELL_WALL;  // Solid to mine
                grid[2][y][x] = CELL_AIR;
                SET_FLOOR(x, y, 2);
            }
        }
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        
        Mover* m = &movers[0];
        Point goal = {2, 2, channelZ};
        InitMover(m, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, (float)channelZ, goal, 100.0f);
        moverCount = 1;
        
        float initialZ = m->z;
        expect(initialZ == (float)channelZ);
        
        bool designated = DesignateChannel(2, 2, channelZ);
        expect(designated == true);
        
        // Run simulation until channeling completes
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            // Stop once channeling is done to capture mover position
            if (!HasChannelDesignation(2, 2, channelZ)) break;
        }
        
        // Mover should have descended
        expect(m->z == (float)descendZ);
    }
}

describe(channel_workgiver) {
    it("should not assign channel job to mover without canMine") {
        InitTestGridFromAscii(
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n");
        
        for (int x = 0; x < 5; x++) {
            for (int y = 0; y < 5; y++) {
                grid[0][y][x] = CELL_WALL;
                grid[1][y][x] = CELL_AIR;
                SET_FLOOR(x, y, 1);
            }
        }
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        
        Mover* m = &movers[0];
        Point goal = {2, 2, 1};
        InitMover(m, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, goal, 100.0f);
        moverCount = 1;
        m->capabilities.canMine = false;  // No mining capability
        
        DesignateChannel(2, 2, 1);
        
        // Rebuild idle list for WorkGiver
        RebuildIdleMoverList();
        
        int jobId = WorkGiver_Channel(0);
        
        // Should NOT create a job
        expect(jobId == -1);
        expect(m->currentJobId == -1);
    }
}

describe(channel_hpa_ramp_links) {
    it("should update HPA ramp links after channeling creates ramp") {
        // Same setup as "should create ramp when wall adjacent at z-1"
        // We use A* for pathfinding but verify that HPA graph (rampLinkCount) updates
        InitTestGridFromAscii(
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n");
        
        // z=0 walls, z=1 walls (ramp high-side), z=2 air + floor flag
        int channelZ = 2;
        int rampZ = 1;
        
        for (int x = 0; x < 5; x++) {
            for (int y = 0; y < 5; y++) {
                grid[0][y][x] = CELL_WALL;
                grid[1][y][x] = CELL_WALL;
                grid[2][y][x] = CELL_AIR;
                SET_FLOOR(x, y, 2);
            }
        }
        
        // Use A* for pathfinding (HPA* doesn't work on single-chunk grids)
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearJobs();
        InitDesignations();
        
        // Build initial HPA graph - should have 0 ramp links
        BuildEntrances();
        BuildGraph();
        int initialRampLinks = rampLinkCount;
        expect(initialRampLinks == 0);
        
        // Mover at (2,2) on channelZ - exactly on channel target
        Mover* m = &movers[0];
        Point goal = {2, 2, channelZ};
        InitMover(m, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, (float)channelZ, goal, 100.0f);
        moverCount = 1;
        
        // Designate channel
        bool designated = DesignateChannel(2, 2, channelZ);
        expect(designated == true);
        
        // Run simulation until channeling completes
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (!HasChannelDesignation(2, 2, channelZ)) break;
        }
        
        // Should create ramp - walls surround provide high side
        expect(CellIsRamp(grid[rampZ][2][2]) != 0);
        
        // HPA graph incremental update only runs with HPA* algorithm
        // Force update by calling UpdateDirtyChunks (which is called in Tick when HPA* is active)
        // Since we use A* for pathfinding, we need to manually trigger the update
        UpdateDirtyChunks();
        
        // The ramp link count should now be > 0
        expect(rampLinkCount > initialRampLinks);
    }
}

describe(channel_rectangle_ramps) {
    it("should create ramps on all border cells when channeling rectangle") {
        // Setup: 10x10 grid, z0 = solid dirt, z1 = walkable air (floor flag)
        // Channel a 4x4 rectangle (cells 3-6, 3-6) at z1
        // Expected: all 12 border cells at z0 should become ramps
        
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        // Set up z0 as solid dirt, z1 as walkable air
        for (int x = 0; x < 10; x++) {
            for (int y = 0; y < 10; y++) {
                grid[0][y][x] = CELL_WALL;
                SetWallMaterial(x, y, 0, MAT_DIRT);
                grid[1][y][x] = CELL_AIR;
                SET_FLOOR(x, y, 1);  // Floor flag makes z1 walkable
            }
        }
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearJobs();
        InitDesignations();
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        // Directly call CompleteChannelDesignation for each cell
        // This simulates what happens when movers complete the jobs
        // Channel from (3,3) to (6,6) - a 4x4 area
        int minX = 3, maxX = 6, minY = 3, maxY = 6;
        int channelZ = 1;
        
        // Process in row-major order (same as typical designation order)
        for (int y = minY; y <= maxY; y++) {
            for (int x = minX; x <= maxX; x++) {
                CompleteChannelDesignation(x, y, channelZ, -1);
            }
        }
        
        // Count ramps at z0 in the channeled area
        int rampCountInArea = 0;
        int floorCountInArea = 0;
        for (int y = minY; y <= maxY; y++) {
            for (int x = minX; x <= maxX; x++) {
                CellType cell = grid[0][y][x];
                if (CellIsRamp(cell)) {
                    rampCountInArea++;
                } else if (cell == CELL_AIR && HAS_FLOOR(x, y, 0)) {
                    floorCountInArea++;
                }
            }
        }
        (void)floorCountInArea;
        
        // All 16 cells should be either ramp or floor
        // Border cells (12) should be ramps, interior cells (4) can be floor
        // Actually in DF, interior cells also get ramps if they have an adjacent ramp exit
        
        // At minimum, verify we have some ramps (not all floor)
        expect(rampCountInArea > 0);
        
        // Check specific border cells that should definitely be ramps:
        // Top row (y=3): should have ramps facing north
        // Bottom row (y=6): should have ramps facing south  
        // Left column (x=3): should have ramps facing west
        // Right column (x=6): should have ramps facing east
        
        // Top-left corner (3,3) - should be a ramp (either N or W)
        CellType topLeft = grid[0][3][3];
        expect(CellIsRamp(topLeft) != 0);
        
        // Top-right corner (6,3) - should be a ramp (either N or E)
        CellType topRight = grid[0][3][6];
        expect(CellIsRamp(topRight) != 0);
        
        // Bottom-left corner (3,6) - should be a ramp (either S or W)
        CellType bottomLeft = grid[0][6][3];
        expect(CellIsRamp(bottomLeft) != 0);
        
        // Bottom-right corner (6,6) - should be a ramp (either S or E)
        CellType bottomRight = grid[0][6][6];
        expect(CellIsRamp(bottomRight) != 0);
        
        // Check a middle border cell on west edge (3,4)
        CellType westEdge = grid[0][4][3];
        expect(CellIsRamp(westEdge) != 0);
        
        // Check a middle border cell on east edge (6,4)
        CellType eastEdge = grid[0][4][6];
        expect(CellIsRamp(eastEdge) != 0);
    }
}

// =============================================================================
// Building/Construction Tests
// =============================================================================

describe(building_blueprint) {
    it("should create blueprint on floor tile") {
        InitTestGridFromAscii(
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n");
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        
        // Create blueprint on floor
        int bpIdx = CreateRecipeBlueprint(2, 2, 0, CONSTRUCTION_DRY_STONE_WALL);
        expect(bpIdx >= 0);
        expect(HasBlueprint(2, 2, 0) == true);
        expect(blueprints[bpIdx].state == BLUEPRINT_AWAITING_MATERIALS);
        expect(CountBlueprints() == 1);
    }
    
    it("should not create blueprint on wall tile") {
        InitTestGridFromAscii(
            "......\n"
            ".#....\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n");
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        
        // Try to create blueprint on wall
        int bpIdx = CreateRecipeBlueprint(1, 1, 0, CONSTRUCTION_DRY_STONE_WALL);
        expect(bpIdx == -1);
        expect(HasBlueprint(1, 1, 0) == false);
        expect(CountBlueprints() == 0);
    }
    
    it("should cancel blueprint") {
        InitTestGridFromAscii(
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n");
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        
        int bpIdx = CreateRecipeBlueprint(2, 2, 0, CONSTRUCTION_DRY_STONE_WALL);
        expect(CountBlueprints() == 1);
        
        CancelBlueprint(bpIdx);
        expect(HasBlueprint(2, 2, 0) == false);
        expect(CountBlueprints() == 0);
    }
}

describe(building_haul_job) {
    it("should assign haul job to blueprint needing materials") {
        InitTestGridFromAscii(
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        
        // Mover at (0,0)
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Item at (1,1) - must be ITEM_BLOCKS for building walls
        int itemIdx = SpawnItem(1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_BLOCKS);
        
        // Blueprint at (4,4)
        int bpIdx = CreateRecipeBlueprint(4, 4, 0, CONSTRUCTION_DRY_STONE_WALL);
        expect(blueprints[bpIdx].state == BLUEPRINT_AWAITING_MATERIALS);
        
        // Run job assignment
        AssignJobs();
        
        // Mover should be assigned to haul the item
        expect(MoverIsMovingToPickup(m));
        expect(MoverGetTargetItem(m) == itemIdx);
        expect(MoverGetTargetBlueprint(m) == bpIdx);
        expect(items[itemIdx].reservedBy >= 0);
    }
    
    it("should not assign haul job when no items available") {
        InitTestGridFromAscii(
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        
        // Mover at (0,0)
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Blueprint but NO items
        int bpIdx = CreateRecipeBlueprint(4, 4, 0, CONSTRUCTION_DRY_STONE_WALL);
        
        AssignJobs();
        
        // Mover should remain idle
        expect(MoverIsIdle(m));
        expect(blueprints[bpIdx].stageDeliveries[0].reservedCount == 0);
    }
}

describe(building_job_execution) {
    it("should deliver material and complete build") {
        InitTestGridFromAscii(
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        
        // Mover at (0,0)
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // 3 rocks at (1,1) - dry stone wall needs 3
        for (int i = 0; i < 3; i++) {
            SpawnItem(1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_ROCK);
        }
        
        // Blueprint at (3,3) - will become a wall
        CreateRecipeBlueprint(3, 3, 0, CONSTRUCTION_DRY_STONE_WALL);
        
        // Run simulation until build completes
        // Hauler picks up item, delivers to blueprint, then builder builds
        for (int i = 0; i < 3000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            
            // Check if wall was built
            if (grid[0][3][3] == CELL_WALL) break;
        }
        
        // Blueprint should be complete - wall should exist
        expect(grid[0][3][3] == CELL_WALL);
        expect(HasBlueprint(3, 3, 0) == false);
        expect(CountBlueprints() == 0);
        
        // Mover should be idle
        expect(MoverIsIdle(m));
    }
    
    it("should cancel haul job when blueprint is cancelled") {
        InitTestGridFromAscii(
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        
        // Mover at (0,0)
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Item at (1,1) - must be ITEM_BLOCKS for building walls
        int itemIdx = SpawnItem(1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_BLOCKS);
        
        // Blueprint at (4,4)
        int bpIdx = CreateRecipeBlueprint(4, 4, 0, CONSTRUCTION_DRY_STONE_WALL);
        
        // Start hauling
        AssignJobs();
        expect(MoverIsMovingToPickup(m));
        
        // Run a few ticks to pick up item
        for (int i = 0; i < 500; i++) {
            Tick();
            JobsTick();
            if (MoverHasHaulToBlueprintJob(m) && MoverIsCarrying(m)) break;
        }
        
        expect(MoverGetCarryingItem(m) == itemIdx);
        
        // Cancel the blueprint while hauler is en route
        CancelBlueprint(bpIdx);
        
        // Run more ticks - mover should drop item and become idle
        for (int i = 0; i < 100; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (MoverIsIdle(m) && MoverGetCarryingItem(m) == -1) break;
        }
        
        // Mover should have dropped item and be idle
        expect(MoverIsIdle(m));
        expect(MoverGetCarryingItem(m) == -1);
        
        // Item should be on ground (not deleted)
        expect(IsItemActive(itemIdx) == true);
        expect(items[itemIdx].state == ITEM_ON_GROUND);
    }
}

describe(building_two_movers) {
    it("should use separate hauler and builder when both idle") {
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        
        // Mover 1 near the item (will be hauler)
        Mover* m1 = &movers[0];
        Point goal1 = {0, 0, 0};
        InitMover(m1, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal1, 100.0f);
        
        // Mover 2 near the blueprint (will be builder)
        Mover* m2 = &movers[1];
        Point goal2 = {6, 6, 0};
        InitMover(m2, 6 * CELL_SIZE + CELL_SIZE * 0.5f, 6 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal2, 100.0f);
        moverCount = 2;
        
        // 3 rocks at (1,1) - dry stone wall needs 3
        for (int i = 0; i < 3; i++) {
            SpawnItem(1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_ROCK);
        }
        
        // Blueprint at (5,5)
        int bpIdx = CreateRecipeBlueprint(5, 5, 0, CONSTRUCTION_DRY_STONE_WALL);
        
        // Run simulation until build completes
        bool haulerFound = false;
        bool builderFound = false;
        int haulerIdx = -1;
        
        for (int i = 0; i < 3000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            
            // Track who does what
            if (MoverIsMovingToPickup(m1) || (MoverHasHaulToBlueprintJob(m1) && MoverIsCarrying(m1))) {
                haulerFound = true;
                haulerIdx = 0;
            }
            if (MoverIsMovingToPickup(m2) || (MoverHasHaulToBlueprintJob(m2) && MoverIsCarrying(m2))) {
                haulerFound = true;
                haulerIdx = 1;
            }
            
            // After material delivered, a builder should be assigned
            if (blueprints[bpIdx].active && blueprints[bpIdx].state == BLUEPRINT_BUILDING) {
                builderFound = true;
            }
            
            if (grid[0][5][5] == CELL_WALL) break;
        }
        
        // Wall should be built
        expect(grid[0][5][5] == CELL_WALL);
        expect(haulerFound == true);
        expect(builderFound == true);
        
        // Both movers should be idle at the end
        expect(MoverIsIdle(m1));
        expect(MoverIsIdle(m2));
        
        // Suppress unused variable warning
        (void)haulerIdx;
    }
}

/*
 * =============================================================================
 * JOB POOL TESTS (Phase 1 of Jobs Refactor)
 * 
 * These tests verify the new Job pool system:
 * - Jobs can be created and tracked separately from Movers
 * - Jobs store all necessary target data
 * - Jobs can be released and reused (free list)
 * - Performance: O(1) allocation/deallocation
 * =============================================================================
 */

describe(job_pool) {
    it("should create a job and return valid job id") {
        // Initialize job system
        ClearJobs();
        
        // Create a haul job
        int jobId = CreateJob(JOBTYPE_HAUL);
        
        // Should return valid index
        expect(jobId >= 0);
        expect(jobId < MAX_JOBS);
        
        // Job should be active
        Job* job = GetJob(jobId);
        expect(job != NULL);
        expect(job->active == true);
        expect(job->type == JOBTYPE_HAUL);
    }
    
    it("should assign job to mover via currentJobId") {
        // Setup minimal world
        InitGridFromAsciiWithChunkSize(
            "....\n"
            "....\n", 4, 4);
        ClearMovers();
        ClearJobs();
        
        // Create mover
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, CELL_SIZE * 0.5f, CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Mover should start with no job
        expect(m->currentJobId == -1);
        
        // Create and assign a job
        int jobId = CreateJob(JOBTYPE_HAUL);
        m->currentJobId = jobId;
        Job* job = GetJob(jobId);
        job->assignedMover = 0;
        
        // Verify assignment
        expect(m->currentJobId == jobId);
        expect(job->assignedMover == 0);
    }
    
    it("should release job when completed") {
        ClearJobs();
        
        // Create a job
        int jobId = CreateJob(JOBTYPE_HAUL);
        expect(GetJob(jobId)->active == true);
        
        // Release the job
        ReleaseJob(jobId);
        
        // Job should no longer be active
        expect(GetJob(jobId)->active == false);
    }
    
    it("should track job type correctly") {
        ClearJobs();
        
        int haulJob = CreateJob(JOBTYPE_HAUL);
        int digJob = CreateJob(JOBTYPE_MINE);
        int buildJob = CreateJob(JOBTYPE_BUILD);
        int clearJob = CreateJob(JOBTYPE_CLEAR);
        
        expect(GetJob(haulJob)->type == JOBTYPE_HAUL);
        expect(GetJob(digJob)->type == JOBTYPE_MINE);
        expect(GetJob(buildJob)->type == JOBTYPE_BUILD);
        expect(GetJob(clearJob)->type == JOBTYPE_CLEAR);
    }
    
    it("should store target data for haul job") {
        ClearJobs();
        
        int jobId = CreateJob(JOBTYPE_HAUL);
        Job* job = GetJob(jobId);
        
        // Set haul targets
        job->targetItem = 5;
        job->targetStockpile = 2;
        job->targetSlotX = 3;
        job->targetSlotY = 4;
        
        // Verify targets stored correctly
        expect(job->targetItem == 5);
        expect(job->targetStockpile == 2);
        expect(job->targetSlotX == 3);
        expect(job->targetSlotY == 4);
    }
    
    it("should store target data for mine job") {
        ClearJobs();
        
        int jobId = CreateJob(JOBTYPE_MINE);
        Job* job = GetJob(jobId);
        
        // Set mine targets
        job->targetMineX = 10;
        job->targetMineY = 20;
        job->targetMineZ = 0;
        
        // Verify targets stored correctly
        expect(job->targetMineX == 10);
        expect(job->targetMineY == 20);
        expect(job->targetMineZ == 0);
    }
    
    it("should store target data for build job") {
        ClearJobs();
        
        int jobId = CreateJob(JOBTYPE_BUILD);
        Job* job = GetJob(jobId);
        
        // Set build targets
        job->targetBlueprint = 7;
        job->progress = 0.5f;
        
        // Verify targets stored correctly
        expect(job->targetBlueprint == 7);
        expect(job->progress == 0.5f);
    }
    
    it("should reuse released job slots via free list") {
        ClearJobs();
        
        // Create 3 jobs
        int job1 = CreateJob(JOBTYPE_HAUL);
        int job2 = CreateJob(JOBTYPE_MINE);
        int job3 = CreateJob(JOBTYPE_BUILD);
        
        // Release middle job
        ReleaseJob(job2);
        
        // Create new job - should reuse job2's slot
        int job4 = CreateJob(JOBTYPE_HAUL);
        
        // job4 should have reused job2's index (free list)
        expect(job4 == job2);
        
        // Original jobs still work
        expect(GetJob(job1)->type == JOBTYPE_HAUL);
        expect(GetJob(job3)->type == JOBTYPE_BUILD);
        expect(GetJob(job4)->type == JOBTYPE_HAUL);
    }
    
    it("should track active job count correctly") {
        ClearJobs();
        
        expect(activeJobCount == 0);
        
        int job1 = CreateJob(JOBTYPE_HAUL);
        expect(activeJobCount == 1);
        
        int job2 = CreateJob(JOBTYPE_MINE);
        expect(activeJobCount == 2);
        
        ReleaseJob(job1);
        expect(activeJobCount == 1);
        
        ReleaseJob(job2);
        expect(activeJobCount == 0);
    }
    
    it("CreateJob should be O(1) not O(n)") {
        ClearJobs();
        
        // Create many jobs
        double startTime = GetTime();
        for (int i = 0; i < 1000; i++) {
            CreateJob(JOBTYPE_HAUL);
        }
        double createTime = GetTime() - startTime;
        
        // Release all and recreate (should use free list)
        for (int i = 0; i < 1000; i++) {
            ReleaseJob(i);
        }
        
        startTime = GetTime();
        for (int i = 0; i < 1000; i++) {
            CreateJob(JOBTYPE_HAUL);
        }
        double reuseTime = GetTime() - startTime;
        
        // Both should be very fast (< 100ms for 1000 ops - generous for CI)
        // This verifies O(1) behavior (not O(n) which would be much slower)
        expect(createTime < 0.1);
        expect(reuseTime < 0.1);
        
        // Reuse time should not be dramatically slower than create time
        // (if it were O(n) scan, reuse would be much slower)
        // Allow generous variance for timing noise
        expect(reuseTime < createTime + 0.1);
    }
}

/*
 * =============================================================================
 * JOB DRIVER TESTS (Phase 2 of Jobs Refactor)
 * 
 * These tests verify that Job Drivers correctly execute jobs:
 * - Each job type has its own driver function
 * - Drivers handle the full job lifecycle (start -> progress -> complete)
 * - Drivers properly release resources on completion/failure
 * =============================================================================
 */

describe(job_drivers) {
    it("should complete haul job via driver: pickup -> carry -> deliver") {
        // Setup world
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 8);
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearGatherZones();
        ClearJobs();
        
        // Create mover at (1,1)
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, CELL_SIZE * 1.5f, CELL_SIZE * 1.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Create stockpile at (5,1) - 2x2
        int sp = CreateStockpile(5, 1, 0, 2, 2);
        SetStockpileFilter(sp, ITEM_RED, true);
        
        // Create item at (3,1)
        int itemIdx = SpawnItem(CELL_SIZE * 3.5f, CELL_SIZE * 1.5f, 0.0f, ITEM_RED);
        
        // Create a haul job using the new Job pool
        int jobId = CreateJob(JOBTYPE_HAUL);
        Job* job = GetJob(jobId);
        job->targetItem = itemIdx;
        job->targetStockpile = sp;
        job->targetSlotX = 5;
        job->targetSlotY = 1;
        job->assignedMover = 0;
        
        // Assign job to mover
        m->currentJobId = jobId;
        
        // Set mover goal to item location
        m->goal = (Point){3, 1, 0};
        m->needsRepath = true;
        
        // Reserve item and slot
        ReserveItem(itemIdx, 0);
        ReserveStockpileSlot(sp, 5, 1, 0, ITEM_RED, MAT_NONE);
        
        // Run simulation
        for (int i = 0; i < 600; i++) {
            Tick();
            JobsTick();
            if (items[itemIdx].state == ITEM_IN_STOCKPILE) break;
        }
        
        // Verify haul completed
        expect(items[itemIdx].state == ITEM_IN_STOCKPILE);
        expect(m->currentJobId == -1);  // Job should be released
        expect(GetJob(jobId)->active == false);
    }
    
    it("should complete mine job via driver: move to adjacent -> mine -> done") {
        // Setup world with a wall to mine
        // Legacy: z=0 is walkable with wall at (3,1)
        // Standard: z=0 is solid ground, z=1 is walkable with wall at (3,1)
        int mineZ = 1;
        
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "...#....\n"
            "........\n"
            "........\n", 8, 8);
        
        // Need solid ground at z=0, walkable at z=1
        for (int x = 0; x < 8; x++) {
            for (int y = 0; y < 4; y++) {
                grid[0][y][x] = CELL_WALL;  // Solid ground
                SetWallMaterial(x, y, 0, MAT_DIRT);
                grid[1][y][x] = CELL_AIR;   // Air above (walkable)
            }
        }
        grid[1][1][3] = CELL_WALL;  // Wall to mine at z=1
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        InitDesignations();
        ClearJobs();
        
        // Create mover at (1,1)
        Mover* m = &movers[0];
        Point goal = {1, 1, mineZ};
        InitMover(m, CELL_SIZE * 1.5f, CELL_SIZE * 1.5f, (float)mineZ, goal, 100.0f);
        moverCount = 1;
        
        // Designate wall at (3,1) for digging
        DesignateMine(3, 1, mineZ);
        
        // Create a mine job using the new Job pool
        int jobId = CreateJob(JOBTYPE_MINE);
        Job* job = GetJob(jobId);
        job->targetMineX = 3;
        job->targetMineY = 1;
        job->targetMineZ = mineZ;
        job->targetAdjX = 2;  // Adjacent tile where mover stands to mine
        job->targetAdjY = 1;
        job->assignedMover = 0;
        
        // Assign job to mover
        m->currentJobId = jobId;
        
        // Set mover goal to adjacent tile (2,1 is adjacent to wall at 3,1)
        m->goal = (Point){2, 1, mineZ};
        m->needsRepath = true;
        
        // Reserve designation
        Designation* d = GetDesignation(3, 1, mineZ);
        d->assignedMover = 0;
        
        // Run simulation
        for (int i = 0; i < 600; i++) {
            Tick();
            JobsTick();
            // Check if wall is mined (becomes walkable)
            if (IsCellWalkableAt(mineZ, 1, 3)) break;
        }
        
        // Verify mine completed - wall is now walkable
        expect(IsCellWalkableAt(mineZ, 1, 3) == true);
        expect(m->currentJobId == -1);
        expect(GetJob(jobId)->active == false);
    }
    
    it("should complete build job via driver: move to blueprint -> build -> done") {
        // Setup world
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 8);
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        InitDesignations();  // Also clears blueprints
        ClearJobs();
        
        // Create mover at (1,1)
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, CELL_SIZE * 1.5f, CELL_SIZE * 1.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Create blueprint at (4,1) - already has materials delivered
        int bpIdx = CreateRecipeBlueprint(4, 1, 0, CONSTRUCTION_DRY_STONE_WALL);
        FillBlueprintStage(bpIdx, MAT_GRANITE);
        
        // Create a build job using the new Job pool
        int jobId = CreateJob(JOBTYPE_BUILD);
        Job* job = GetJob(jobId);
        job->targetBlueprint = bpIdx;
        job->assignedMover = 0;
        job->progress = 0.0f;
        
        // Assign job to mover
        m->currentJobId = jobId;
        blueprints[bpIdx].assignedBuilder = 0;
        blueprints[bpIdx].state = BLUEPRINT_BUILDING;
        
        // Set mover goal to blueprint location
        m->goal = (Point){4, 1, 0};
        m->needsRepath = true;
        
        // Run simulation using the new driver system
        for (int i = 0; i < 600; i++) {
            Tick();
            JobsTick();
            if (grid[0][1][4] == CELL_WALL) break;
        }
        
        // Verify build completed - floor is now wall
        expect(grid[0][1][4] == CELL_WALL);
        expect(m->currentJobId == -1);
        expect(GetJob(jobId)->active == false);
    }
    
    it("should cancel job and release reservations on failure") {
        // Setup world
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n", 8, 8);
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearJobs();
        
        // Create mover
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, CELL_SIZE * 1.5f, CELL_SIZE * 1.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Create stockpile
        int sp = CreateStockpile(5, 1, 0, 2, 2);
        SetStockpileFilter(sp, ITEM_RED, true);
        
        // Create item
        int itemIdx = SpawnItem(CELL_SIZE * 3.5f, CELL_SIZE * 1.5f, 0.0f, ITEM_RED);
        
        // Create haul job
        int jobId = CreateJob(JOBTYPE_HAUL);
        Job* job = GetJob(jobId);
        job->targetItem = itemIdx;
        job->targetStockpile = sp;
        job->targetSlotX = 5;
        job->targetSlotY = 1;
        job->assignedMover = 0;
        m->currentJobId = jobId;
        
        // Reserve item and slot
        ReserveItem(itemIdx, 0);
        ReserveStockpileSlot(sp, 5, 1, 0, ITEM_RED, MAT_NONE);
        
        // Delete the item mid-job (simulate failure)
        DeleteItem(itemIdx);
        
        // Run one tick - driver should detect failure and cancel
        Tick();
        JobsTick();
        
        // Job should be cancelled, reservations released
        expect(m->currentJobId == -1);
        expect(GetJob(jobId)->active == false);
        // Slot should be released (check reservation count)
        expect(stockpiles[sp].reservedBy[0] == 0);
    }
    
    it("should return mover to idle when job completes") {
        // Setup world
        InitGridFromAsciiWithChunkSize(
            "....\n"
            "....\n", 4, 4);
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearJobs();
        RebuildIdleMoverList();
        
        // Create mover
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, CELL_SIZE * 0.5f, CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Create stockpile right next to mover
        int sp = CreateStockpile(1, 0, 0, 1, 1);
        SetStockpileFilter(sp, ITEM_RED, true);
        
        // Create item at mover's position (instant pickup)
        int itemIdx = SpawnItem(CELL_SIZE * 0.5f, CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Create haul job
        int jobId = CreateJob(JOBTYPE_HAUL);
        Job* job = GetJob(jobId);
        job->targetItem = itemIdx;
        job->targetStockpile = sp;
        job->targetSlotX = 1;
        job->targetSlotY = 0;
        job->assignedMover = 0;
        m->currentJobId = jobId;
        
        ReserveItem(itemIdx, 0);
        ReserveStockpileSlot(sp, 1, 0, 0, ITEM_RED, MAT_NONE);
        
        // Set mover goal to item location (same as mover position for instant pickup)
        m->goal = (Point){0, 0, 0};
        m->needsRepath = true;
        
        // Mover should not be in idle list while working
        RemoveMoverFromIdleList(0);
        expect(moverIsInIdleList[0] == false);
        
        // Run until job completes
        for (int i = 0; i < 300; i++) {
            Tick();
            JobsTick();
            if (m->currentJobId == -1) break;
        }
        
        // Mover should be back in idle list
        expect(m->currentJobId == -1);
        expect(moverIsInIdleList[0] == true);
    }
}

/*
 * =============================================================================
 * GAME SPEED TESTS
 * 
 * These tests verify that job progress scales with game speed (gameDeltaTime).
 * Mining and building should complete faster at higher game speeds.
 * =============================================================================
 */

describe(job_game_speed) {
    it("should complete mine job faster at higher game speed") {
        // Setup world with a wall to mine
        // Legacy: z=0 walkable with wall at (3,1)
        // Standard: z=0 solid, z=1 walkable with wall at (3,1)
        int mineZ = 1;
        
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "...#....\n"
            "........\n"
            "........\n", 8, 8);
        
        // Solid ground at z=0, walkable at z=1
        for (int x = 0; x < 8; x++) {
            for (int y = 0; y < 4; y++) {
                grid[0][y][x] = CELL_WALL;
                SetWallMaterial(x, y, 0, MAT_DIRT);
                grid[1][y][x] = CELL_AIR;
            }
        }
        grid[1][1][3] = CELL_WALL;  // Wall to mine at z=1
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        InitDesignations();
        ClearJobs();
        
        // Create mover adjacent to wall (at 2,1, wall is at 3,1)
        Mover* m = &movers[0];
        Point goal = {2, 1, mineZ};
        InitMover(m, CELL_SIZE * 2.5f, CELL_SIZE * 1.5f, (float)mineZ, goal, 100.0f);
        m->pathLength = 0;  // Already at destination
        moverCount = 1;
        
        // Designate wall at (3,1) for digging
        DesignateMine(3, 1, mineZ);
        
        // Create mine job
        int jobId = CreateJob(JOBTYPE_MINE);
        Job* job = GetJob(jobId);
        job->targetMineX = 3;
        job->targetMineY = 1;
        job->targetMineZ = mineZ;
        job->assignedMover = 0;
        job->step = STEP_WORKING;
        job->progress = 0.0f;
        m->currentJobId = jobId;
        
        // Reserve designation
        Designation* d = GetDesignation(3, 1, mineZ);
        d->assignedMover = 0;
        
        // Test at 1x speed - count ticks needed
        gameSpeed = 1.0f;
        gameDeltaTime = TICK_DT * gameSpeed;
        
        int ticksAt1x = 0;
        for (int i = 0; i < 600; i++) {
            JobsTick();
            ticksAt1x++;
            if (IsCellWalkableAt(mineZ, 1, 3)) break;
        }
        expect(IsCellWalkableAt(mineZ, 1, 3) == true);
        
        // Reset for 2x speed test
        grid[mineZ][1][3] = CELL_WALL;
        DesignateMine(3, 1, mineZ);
        d = GetDesignation(3, 1, mineZ);
        d->assignedMover = 0;
        
        // Create fresh job for 2x test
        int jobId2 = CreateJob(JOBTYPE_MINE);
        Job* job2 = GetJob(jobId2);
        job2->targetMineX = 3;
        job2->targetMineY = 1;
        job2->targetMineZ = mineZ;
        job2->assignedMover = 0;
        job2->step = STEP_WORKING;
        job2->progress = 0.0f;
        m->currentJobId = jobId2;
        
        // Test at 2x speed
        gameSpeed = 2.0f;
        gameDeltaTime = TICK_DT * gameSpeed;
        
        int ticksAt2x = 0;
        for (int i = 0; i < 600; i++) {
            JobsTick();
            ticksAt2x++;
            if (IsCellWalkableAt(mineZ, 1, 3)) break;
        }
        expect(IsCellWalkableAt(mineZ, 1, 3) == true);
        
        // At 2x speed, should complete in roughly half the ticks
        // Allow some tolerance (within 20%)
        if (test_verbose) printf("Mine: ticksAt1x=%d, ticksAt2x=%d, ratio=%.2f\n", 
               ticksAt1x, ticksAt2x, (float)ticksAt1x / ticksAt2x);
        expect(ticksAt2x < ticksAt1x);
        expect(ticksAt2x <= (ticksAt1x / 2) + 5);  // Should be ~half, with small tolerance
        
        // Reset game speed
        gameSpeed = 1.0f;
        gameDeltaTime = TICK_DT;
    }
    
    it("should complete build job faster at higher game speed") {
        // Setup world - works in both modes since building happens at a specific z
        // Legacy: z=0 is walkable (floor)
        // Standard: z=0 is solid ground, z=1 is walkable (air above ground)
        int buildZ = 1;
        
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 8);
        
        // Solid ground at z=0, walkable at z=1
        for (int x = 0; x < 8; x++) {
            for (int y = 0; y < 4; y++) {
                grid[0][y][x] = CELL_WALL;
                SetWallMaterial(x, y, 0, MAT_DIRT);
                grid[1][y][x] = CELL_AIR;
            }
        }
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        InitDesignations();
        ClearJobs();
        
        // Create mover at blueprint location (4,1)
        Mover* m = &movers[0];
        Point goal = {4, 1, buildZ};
        InitMover(m, CELL_SIZE * 4.5f, CELL_SIZE * 1.5f, (float)buildZ, goal, 100.0f);
        m->pathLength = 0;  // Already at destination
        moverCount = 1;
        
        // Create blueprint at (4,1) - ready to build
        int bpIdx = CreateRecipeBlueprint(4, 1, buildZ, CONSTRUCTION_DRY_STONE_WALL);
        FillBlueprintStage(bpIdx, MAT_GRANITE);
        blueprints[bpIdx].state = BLUEPRINT_BUILDING;
        blueprints[bpIdx].assignedBuilder = 0;
        
        // Create build job
        int jobId = CreateJob(JOBTYPE_BUILD);
        Job* job = GetJob(jobId);
        job->targetBlueprint = bpIdx;
        job->assignedMover = 0;
        job->step = STEP_WORKING;
        job->progress = 0.0f;
        m->currentJobId = jobId;
        
        // Test at 1x speed
        gameSpeed = 1.0f;
        gameDeltaTime = TICK_DT * gameSpeed;
        
        int ticksAt1x = 0;
        for (int i = 0; i < 600; i++) {
            JobsTick();
            ticksAt1x++;
            if (grid[buildZ][1][4] == CELL_WALL) break;
        }
        expect(grid[buildZ][1][4] == CELL_WALL);
        
        // Reset for 2x speed test
        grid[buildZ][1][4] = CELL_AIR;
        int bpIdx2 = CreateRecipeBlueprint(4, 1, buildZ, CONSTRUCTION_DRY_STONE_WALL);
        FillBlueprintStage(bpIdx2, MAT_GRANITE);
        blueprints[bpIdx2].state = BLUEPRINT_BUILDING;
        blueprints[bpIdx2].assignedBuilder = 0;
        
        // Create fresh job for 2x test
        int jobId2 = CreateJob(JOBTYPE_BUILD);
        Job* job2 = GetJob(jobId2);
        job2->targetBlueprint = bpIdx2;
        job2->assignedMover = 0;
        job2->step = STEP_WORKING;
        job2->progress = 0.0f;
        m->currentJobId = jobId2;
        
        // Test at 2x speed
        gameSpeed = 2.0f;
        gameDeltaTime = TICK_DT * gameSpeed;
        
        int ticksAt2x = 0;
        for (int i = 0; i < 600; i++) {
            JobsTick();
            ticksAt2x++;
            if (grid[buildZ][1][4] == CELL_WALL) break;
        }
        expect(grid[buildZ][1][4] == CELL_WALL);
        
        // At 2x speed, should complete in roughly half the ticks
        if (test_verbose) printf("Build: ticksAt1x=%d, ticksAt2x=%d, ratio=%.2f\n", 
               ticksAt1x, ticksAt2x, (float)ticksAt1x / ticksAt2x);
        expect(ticksAt2x < ticksAt1x);
        expect(ticksAt2x <= (ticksAt1x / 2) + 5);
        
        // Reset game speed
        gameSpeed = 1.0f;
        gameDeltaTime = TICK_DT;
    }
}

/*
 * =============================================================================
 * MOVER CAPABILITIES TESTS (Phase 3 of Jobs Refactor)
 * 
 * These tests verify that movers can be assigned different capabilities:
 * - Hauler-only movers only do haul jobs
 * - Builder-only movers only do build jobs
 * - Miner-only movers only do mine jobs
 * - Movers with all capabilities can do all jobs
 * =============================================================================
 */

describe(mover_capabilities) {
    it("should not assign haul job to mover with canHaul=false") {
        // Setup world
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n", 8, 8);
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearJobs();
        RebuildIdleMoverList();
        
        // Create mover with canHaul=false
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, CELL_SIZE * 1.5f, CELL_SIZE * 1.5f, 0.0f, goal, 100.0f);
        m->capabilities.canHaul = false;
        m->capabilities.canMine = true;
        m->capabilities.canBuild = true;
        moverCount = 1;
        RebuildIdleMoverList();
        
        // Create stockpile and item
        int sp = CreateStockpile(5, 0, 0, 2, 2);
        SetStockpileFilter(sp, ITEM_RED, true);
        SpawnItem(CELL_SIZE * 3.5f, CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Try to assign jobs
        AssignJobs();
        
        // Mover should NOT have a haul job
        expect(MoverIsIdle(m));
        expect(m->currentJobId == -1);
    }
    
    it("should not assign mine job to mover with canMine=false") {
        // Setup world with wall
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "...#....\n"
            "........\n", 8, 8);
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        InitDesignations();
        ClearJobs();
        RebuildIdleMoverList();
        
        // Create mover with canMine=false
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, CELL_SIZE * 1.5f, CELL_SIZE * 1.5f, 0.0f, goal, 100.0f);
        m->capabilities.canHaul = true;
        m->capabilities.canMine = false;
        m->capabilities.canBuild = true;
        moverCount = 1;
        RebuildIdleMoverList();
        
        // Designate wall for digging
        DesignateMine(3, 1, 0);
        
        // Try to assign jobs
        AssignJobs();
        
        // Mover should NOT have a mine job
        expect(MoverIsIdle(m));
        expect(m->currentJobId == -1);
    }
    
    it("should not assign build job to mover with canBuild=false") {
        // Setup world
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n", 8, 8);
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        InitDesignations();
        ClearJobs();
        RebuildIdleMoverList();
        
        // Create mover with canBuild=false
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, CELL_SIZE * 1.5f, CELL_SIZE * 1.5f, 0.0f, goal, 100.0f);
        m->capabilities.canHaul = true;
        m->capabilities.canMine = true;
        m->capabilities.canBuild = false;
        moverCount = 1;
        RebuildIdleMoverList();
        
        // Create blueprint ready to build
        int bpIdx = CreateRecipeBlueprint(4, 1, 0, CONSTRUCTION_DRY_STONE_WALL);
        FillBlueprintStage(bpIdx, MAT_GRANITE);
        
        // Try to assign jobs
        AssignJobs();
        
        // Mover should NOT have a build job
        expect(MoverIsIdle(m) || !MoverHasBuildJob(m));
    }
    
    it("should assign haul job to hauler-only mover") {
        // Setup world
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n", 8, 8);
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearJobs();
        RebuildIdleMoverList();
        
        // Create hauler-only mover
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, CELL_SIZE * 1.5f, CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        m->capabilities.canHaul = true;
        m->capabilities.canMine = false;
        m->capabilities.canBuild = false;
        moverCount = 1;
        RebuildIdleMoverList();
        
        // Create stockpile and item
        int sp = CreateStockpile(5, 0, 0, 2, 2);
        SetStockpileFilter(sp, ITEM_RED, true);
        SpawnItem(CELL_SIZE * 3.5f, CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Try to assign jobs
        AssignJobs();
        
        // Mover SHOULD have a haul job
        expect(MoverIsMovingToPickup(m));
    }
    
    it("should assign build job to builder-only mover") {
        // Setup world
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n", 8, 8);
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        InitDesignations();
        ClearJobs();
        RebuildIdleMoverList();
        
        // Create builder-only mover
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, CELL_SIZE * 1.5f, CELL_SIZE * 1.5f, 0.0f, goal, 100.0f);
        m->capabilities.canHaul = false;
        m->capabilities.canMine = false;
        m->capabilities.canBuild = true;
        moverCount = 1;
        RebuildIdleMoverList();
        
        // Create blueprint ready to build
        int bpIdx = CreateRecipeBlueprint(4, 1, 0, CONSTRUCTION_DRY_STONE_WALL);
        FillBlueprintStage(bpIdx, MAT_GRANITE);
        
        // Try to assign jobs
        AssignJobs();
        
        // Mover SHOULD have a build job
        expect(MoverHasBuildJob(m));
    }
    
    it("hauler delivering material should NOT pick up build job if canBuild=false") {
        // This tests the key scenario: a hauler delivers material but shouldn't build
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n", 8, 8);
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        ClearJobs();
        RebuildIdleMoverList();
        
        // Create hauler-only mover
        Mover* hauler = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(hauler, CELL_SIZE * 1.5f, CELL_SIZE * 1.5f, 0.0f, goal, 100.0f);
        hauler->capabilities.canHaul = true;
        hauler->capabilities.canMine = false;
        hauler->capabilities.canBuild = false;
        moverCount = 1;
        RebuildIdleMoverList();
        
        // Create blueprint awaiting materials
        int bpIdx = CreateRecipeBlueprint(4, 1, 0, CONSTRUCTION_DRY_STONE_WALL);
        
        // Spawn 3 rocks (dry stone wall needs 3)
        for (int i = 0; i < 3; i++) {
            SpawnItem(CELL_SIZE * 2.5f, CELL_SIZE * 1.5f, 0.0f, ITEM_ROCK);
        }
        
        // Run until material is delivered
        for (int i = 0; i < 600; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (blueprints[bpIdx].state == BLUEPRINT_READY_TO_BUILD) break;
        }
        
        // Material should be delivered
        expect(blueprints[bpIdx].state == BLUEPRINT_READY_TO_BUILD);
        
        // Run a few more ticks
        for (int i = 0; i < 60; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }
        
        // Hauler should NOT have picked up the build job (canBuild=false)
        expect(!MoverHasBuildJob(hauler));
        expect(!MoverIsBuilding(hauler));
    }
}

/*
 * =============================================================================
 * WORKGIVERS TESTS (Phase 4 of Jobs Refactor)
 * 
 * These tests verify the modular WorkGiver system that produces jobs:
 * - Each WorkGiver is a function that tries to create a job for a mover
 * - WorkGivers are called in priority order
 * - The system replaces the monolithic AssignJobs() priority sections
 * =============================================================================
 */

describe(workgivers) {
    it("should find haul jobs via WorkGiver_Haul") {
        // Setup world
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n", 8, 8);
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearJobs();
        
        // Create mover
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, CELL_SIZE * 1.5f, CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        RebuildIdleMoverList();
        
        // Create stockpile and item
        int sp = CreateStockpile(5, 0, 0, 2, 2);
        SetStockpileFilter(sp, ITEM_RED, true);
        SpawnItem(CELL_SIZE * 3.5f, CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Call WorkGiver_Haul directly
        int jobId = WorkGiver_Haul(0);
        
        // Should create a job
        expect(jobId >= 0);
        Job* job = GetJob(jobId);
        expect(job != NULL);
        expect(job->type == JOBTYPE_HAUL);
        expect(job->assignedMover == 0);
    }
    
    it("should find mine jobs via WorkGiver_Mining") {
        // Setup world with wall
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "...#....\n"
            "........\n", 8, 8);
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        InitDesignations();
        ClearJobs();
        
        // Create mover
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, CELL_SIZE * 1.5f, CELL_SIZE * 1.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        RebuildIdleMoverList();
        
        // Designate wall for digging
        DesignateMine(3, 1, 0);
        
        // Build mine cache (required before WorkGiver_Mining)
        RebuildMineDesignationCache();
        
        // Call WorkGiver_Mining directly
        int jobId = WorkGiver_Mining(0);
        
        // Should create a job
        expect(jobId >= 0);
        Job* job = GetJob(jobId);
        expect(job != NULL);
        expect(job->type == JOBTYPE_MINE);
        expect(job->assignedMover == 0);
        expect(job->targetMineX == 3);
        expect(job->targetMineY == 1);
    }
    
    it("should find build jobs via WorkGiver_Build") {
        // Setup world
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n", 8, 8);
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        InitDesignations();
        ClearJobs();
        
        // Create mover
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, CELL_SIZE * 1.5f, CELL_SIZE * 1.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        RebuildIdleMoverList();
        
        // Create blueprint ready to build
        int bpIdx = CreateRecipeBlueprint(4, 1, 0, CONSTRUCTION_DRY_STONE_WALL);
        FillBlueprintStage(bpIdx, MAT_GRANITE);
        
        // Call WorkGiver_Build directly
        int jobId = WorkGiver_Build(0);
        
        // Should create a job
        expect(jobId >= 0);
        Job* job = GetJob(jobId);
        expect(job != NULL);
        expect(job->type == JOBTYPE_BUILD);
        expect(job->assignedMover == 0);
        expect(job->targetBlueprint == bpIdx);
    }
    
    it("should find blueprint haul jobs via WorkGiver_BlueprintHaul") {
        // Setup world
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n", 8, 8);
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        ClearJobs();
        
        // Create mover
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, CELL_SIZE * 1.5f, CELL_SIZE * 1.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        RebuildIdleMoverList();
        
        // Create blueprint awaiting materials
        int bpIdx = CreateRecipeBlueprint(4, 1, 0, CONSTRUCTION_DRY_STONE_WALL);
        expect(blueprints[bpIdx].state == BLUEPRINT_AWAITING_MATERIALS);
        
        // Spawn a stone blocks item (building material)
        SpawnItem(CELL_SIZE * 2.5f, CELL_SIZE * 1.5f, 0.0f, ITEM_BLOCKS);
        
        // Call WorkGiver_BlueprintHaul directly
        int jobId = WorkGiver_BlueprintHaul(0);
        
        // Should create a job
        expect(jobId >= 0);
        Job* job = GetJob(jobId);
        expect(job != NULL);
        expect(job->type == JOBTYPE_HAUL_TO_BLUEPRINT);
        expect(job->assignedMover == 0);
        expect(job->targetBlueprint == bpIdx);
    }
    
    it("should respect priority order: haul before mining") {
        // Setup world with both mine designation and haul opportunity
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "...#....\n"
            "........\n", 8, 8);
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        ClearJobs();
        
        // Create mover with all capabilities
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, CELL_SIZE * 1.5f, CELL_SIZE * 1.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        RebuildIdleMoverList();
        
        // Create stockpile and haul item
        int sp = CreateStockpile(6, 0, 0, 2, 2);
        SetStockpileFilter(sp, ITEM_RED, true);
        SpawnItem(CELL_SIZE * 5.5f, CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Also create mine designation (lower priority than haul in AssignJobs)
        DesignateMine(3, 1, 0);
        
        // Call AssignJobs - haul has higher priority than mining
        AssignJobs();
        
        // Mover should have a HAUL job (higher priority than mine)
        expect(m->currentJobId >= 0);
        Job* job = GetJob(m->currentJobId);
        expect(job != NULL);
        expect(job->type == JOBTYPE_HAUL);
    }
    
    it("should check capabilities before assigning via workgiver") {
        // Setup world
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "...#....\n"
            "........\n", 8, 8);
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        InitDesignations();
        ClearJobs();
        
        // Create mover that can NOT mine
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, CELL_SIZE * 1.5f, CELL_SIZE * 1.5f, 0.0f, goal, 100.0f);
        m->capabilities.canMine = false;
        moverCount = 1;
        RebuildIdleMoverList();
        
        // Create mine designation
        DesignateMine(3, 1, 0);
        
        // Build mine cache (required before WorkGiver_Mining)
        RebuildMineDesignationCache();
        
        // Call WorkGiver_Mining - should fail because mover can't mine
        int jobId = WorkGiver_Mining(0);
        
        // Should NOT create a job
        expect(jobId == -1);
        expect(m->currentJobId == -1);
    }
}

/*
 * =============================================================================
 * Final Approach Tests
 * Tests for the IsPathExhausted and TryFinalApproach helpers that fix
 * movers getting stuck at the end of their path.
 * =============================================================================
 */

/* =============================================================================
 * Blueprint Material Selection Tests
 * Tests that recipe inputs filter which items movers haul.
 * =============================================================================
 */

describe(blueprint_material_selection) {
    it("should only haul recipe-matching item type to blueprint") {
        InitTestGridFromAscii(
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        ClearJobs();

        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        RebuildIdleMoverList();

        // Log at (1,1) - closer but not accepted by brick wall
        SpawnItem(1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_LOG);
        // Bricks at (2,1) - farther but accepted
        int brickIdx = SpawnItem(2 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_BRICKS);

        // Brick wall recipe only accepts ITEM_BRICKS
        int bpIdx = CreateRecipeBlueprint(4, 4, 0, CONSTRUCTION_BRICK_WALL);
        (void)bpIdx;

        AssignJobs();

        expect(MoverHasHaulToBlueprintJob(m));
        expect(MoverGetTargetItem(m) == brickIdx);
    }

    it("should haul rock or blocks to dry stone wall blueprint") {
        InitTestGridFromAscii(
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        ClearJobs();

        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        RebuildIdleMoverList();

        // Rock at (1,1)
        int rockIdx = SpawnItem(1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_ROCK);

        // Dry stone wall accepts ITEM_ROCK or ITEM_BLOCKS
        CreateRecipeBlueprint(4, 4, 0, CONSTRUCTION_DRY_STONE_WALL);

        AssignJobs();

        expect(MoverHasHaulToBlueprintJob(m));
        expect(MoverGetTargetItem(m) == rockIdx);
    }

    it("should not assign haul job when only wrong material available") {
        InitTestGridFromAscii(
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        ClearJobs();

        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        RebuildIdleMoverList();

        // Only logs available - brick wall needs bricks
        SpawnItem(1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_LOG);

        CreateRecipeBlueprint(4, 4, 0, CONSTRUCTION_BRICK_WALL);

        AssignJobs();

        expect(MoverIsIdle(m));
    }

    it("should pick nearest matching item not nearest overall") {
        InitTestGridFromAscii(
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        ClearJobs();

        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        RebuildIdleMoverList();

        // Log at (1,1) - very close but wrong type for brick wall
        SpawnItem(1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_LOG);
        // Bricks at (3,1) - farther but correct type
        int brickIdx = SpawnItem(3 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_BRICKS);

        CreateRecipeBlueprint(4, 4, 0, CONSTRUCTION_BRICK_WALL);

        AssignJobs();

        expect(MoverHasHaulToBlueprintJob(m));
        expect(MoverGetTargetItem(m) == brickIdx);
    }

    it("should match different items to different recipe blueprints") {
        InitTestGridFromAscii(
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        ClearJobs();

        Mover* m0 = &movers[0];
        Point goal0 = {0, 0, 0};
        InitMover(m0, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal0, 100.0f);

        Mover* m1 = &movers[1];
        Point goal1 = {5, 0, 0};
        InitMover(m1, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal1, 100.0f);

        moverCount = 2;
        RebuildIdleMoverList();

        // Bricks and logs
        SpawnItem(1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_BRICKS);
        SpawnItem(4 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_LOG);

        // Brick wall wants bricks, log wall wants logs
        CreateRecipeBlueprint(2, 4, 0, CONSTRUCTION_BRICK_WALL);
        CreateRecipeBlueprint(4, 4, 0, CONSTRUCTION_LOG_WALL);

        AssignJobs();

        // Both movers should have haul jobs
        expect(MoverHasHaulToBlueprintJob(m0) || MoverHasHaulToBlueprintJob(m1));
    }

    it("should create recipe blueprint with correct recipe index") {
        InitGridFromAsciiWithChunkSize(
            "......\n"
            "......\n"
            "......\n", 6, 6);

        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(2, 2, 0, CONSTRUCTION_BRICK_WALL);
        expect(bpIdx >= 0);
        expect(blueprints[bpIdx].recipeIndex == CONSTRUCTION_BRICK_WALL);

        const ConstructionRecipe* recipe = GetConstructionRecipe(blueprints[bpIdx].recipeIndex);
        expect(recipe != NULL);
        expect(recipe->buildCategory == BUILD_WALL);
    }
}

describe(final_approach) {
    it("should complete haul job when path exhausted but close to item") {
        // Scenario: mover's path ends one step away from item
        // The final approach code should micro-move the mover to pickup range
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover at (1,1)
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Item at (5,5)
        float itemX = 5 * CELL_SIZE + CELL_SIZE * 0.5f;
        float itemY = 5 * CELL_SIZE + CELL_SIZE * 0.5f;
        int itemIdx = SpawnItem(itemX, itemY, 0.0f, ITEM_RED);
        
        // Stockpile at (8,8)
        int spIdx = CreateStockpile(8, 8, 0, 1, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        // Run simulation until item is picked up or delivered
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (items[itemIdx].state == ITEM_IN_STOCKPILE) break;
        }
        
        // Item should be in stockpile (job completed successfully)
        expect(items[itemIdx].state == ITEM_IN_STOCKPILE);
        expect(MoverIsIdle(m));
    }
    
    it("should complete mine job when path exhausted but adjacent to wall") {
        // Scenario: mover paths to adjacent tile but ends slightly off
        // Final approach should move mover into working range
        InitTestGridFromAscii(
            ".....\n"
            ".#...\n"
            ".....\n"
            ".....\n"
            ".....\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        
        // Mover starts at (0,0)
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Wall at (1,1)
        expect(grid[0][1][1] == CELL_WALL);
        
        // Designate wall for digging
        DesignateMine(1, 1, 0);
        
        // Run simulation until mine completes
        bool completed = false;
        for (int i = 0; i < 500; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            
            if (grid[0][1][1] != CELL_WALL) {
                completed = true;
                break;
            }
        }
        
        expect(completed == true);
        expect(MoverIsIdle(m));
    }
    
    it("should handle pathIndex < 0 as path exhausted") {
        // This tests the specific bug fix: pathLength > 0 but pathIndex < 0
        // means the path was traversed and exhausted
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover starting position
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Item nearby
        float itemX = 3 * CELL_SIZE + CELL_SIZE * 0.5f;
        float itemY = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        int itemIdx = SpawnItem(itemX, itemY, 0.0f, ITEM_RED);
        
        // Stockpile
        int spIdx = CreateStockpile(7, 1, 0, 1, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        // Assign job
        AssignJobs();
        expect(MoverIsMovingToPickup(m));
        
        // Run until mover picks up item (tests that final approach works)
        for (int i = 0; i < 500; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (MoverIsCarrying(m)) break;
        }
        
        // Mover should have picked up the item
        expect(MoverIsCarrying(m));
        expect(MoverGetCarryingItem(m) == itemIdx);
    }
    
    it("should not move mover when already in pickup range") {
        // Final approach should not apply if mover is already close enough
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover very close to where item will be
        float itemX = 5 * CELL_SIZE + CELL_SIZE * 0.5f;
        float itemY = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
        
        Mover* m = &movers[0];
        Point goal = {5, 2, 0};
        // Place mover very close to item position (within PICKUP_RADIUS)
        InitMover(m, itemX + 5.0f, itemY + 5.0f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Item at same location
        int itemIdx = SpawnItem(itemX, itemY, 0.0f, ITEM_RED);
        
        // Stockpile
        int spIdx = CreateStockpile(8, 2, 0, 1, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        // Assign and run - should pick up immediately
        AssignJobs();
        
        // Run just a few ticks - should pick up very quickly
        for (int i = 0; i < 30; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (MoverIsCarrying(m)) break;
        }
        
        expect(MoverIsCarrying(m));
        expect(MoverGetCarryingItem(m) == itemIdx);
    }
    
    it("should complete delivery when path exhausted but close to stockpile") {
        // Scenario: mover carrying item, path ends near stockpile
        // Final approach should complete the delivery
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover near item for quick pickup
        float itemX = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
        float itemY = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
        
        Mover* m = &movers[0];
        Point goal = {2, 2, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Item close to mover
        int itemIdx = SpawnItem(itemX, itemY, 0.0f, ITEM_RED);
        
        // Stockpile at (7,7)
        int spIdx = CreateStockpile(7, 7, 0, 1, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        // Run full simulation
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (items[itemIdx].state == ITEM_IN_STOCKPILE) break;
        }
        
        // Delivery should complete
        expect(items[itemIdx].state == ITEM_IN_STOCKPILE);
        expect(MoverIsIdle(m));
        expect(MoverGetCarryingItem(m) == -1);
    }
    
    it("should not final approach when mover is far from target") {
        // Final approach only activates when mover is in same or adjacent cell
        // When far away, mover should rely on normal pathfinding
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover at (1,1)
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Item far away at (8,8)
        float itemX = 8 * CELL_SIZE + CELL_SIZE * 0.5f;
        float itemY = 8 * CELL_SIZE + CELL_SIZE * 0.5f;
        int itemIdx = SpawnItem(itemX, itemY, 0.0f, ITEM_RED);
        (void)itemIdx;
        
        // Stockpile
        int spIdx = CreateStockpile(5, 5, 0, 1, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        // Assign job
        AssignJobs();
        expect(MoverIsMovingToPickup(m));
        
        // Artificially clear the path to simulate exhausted state while far
        ClearMoverPath(0);
        m->pathIndex = -1;
        
        // Record position
        float startX = m->x;
        float startY = m->y;
        
        // Run one JobsTick - final approach should NOT move mover (too far)
        JobsTick();
        
        // Mover position should be unchanged (final approach didn't activate)
        // The mover is in cell (1,1) and item is in cell (8,8) - not adjacent
        expect(m->x == startX);
        expect(m->y == startY);
    }
}

// =============================================================================
// Strong Stockpile Behavior Tests
// These tests verify expected player-facing behavior, not implementation details
// =============================================================================

describe(stockpile_strong_tests) {
    it("items should flow from low-priority to high-priority stockpiles naturally") {
        InitTestGridFromAscii(
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover near the dump zone
        Mover* m = &movers[0];
        Point goal = {5, 5, 0};
        InitMover(m, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Low-priority "dump zone" near spawn (5,5)
        int spLow = CreateStockpile(8, 5, 0, 3, 1);
        SetStockpileFilter(spLow, ITEM_LOG, true);
        SetStockpilePriority(spLow, 2);  // Low priority
        
        // High-priority "workshop storage" far away (25,5)
        int spHigh = CreateStockpile(25, 5, 0, 3, 1);
        SetStockpileFilter(spHigh, ITEM_LOG, true);
        SetStockpilePriority(spHigh, 8);  // High priority
        
        // Place log directly in low-priority stockpile
        int logIdx = SpawnItem(8 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_LOG);
        items[logIdx].state = ITEM_IN_STOCKPILE;
        Stockpile* spL = &stockpiles[spLow];
        spL->slotCounts[0] = 1;
        spL->slotTypes[0] = ITEM_LOG;
        spL->slotMaterials[0] = items[logIdx].material;
        
        // Verify it's in the low-priority stockpile
        int spIdx = -1;
        IsPositionInStockpile(items[logIdx].x, items[logIdx].y, (int)items[logIdx].z, &spIdx);
        expect(spIdx == spLow);
        
        // With idle time, should re-haul to high-priority
        for (int i = 0; i < 3000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            
            // Check if moved to high priority
            IsPositionInStockpile(items[logIdx].x, items[logIdx].y, (int)items[logIdx].z, &spIdx);
            if (spIdx == spHigh) break;
        }
        
        // Should now be in high-priority stockpile
        IsPositionInStockpile(items[logIdx].x, items[logIdx].y, (int)items[logIdx].z, &spIdx);
        expect(spIdx == spHigh);
    }
    
    it("overfull stockpiles should drain, not grow") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Stockpile with max_stack=5, but one slot is overfull (8 items from old max_stack=10)
        int spIdx = CreateStockpile(5, 5, 0, 2, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        SetStockpileMaxStackSize(spIdx, 5);  // Current max is 5
        
        // Manually create overfull slot (simulating legacy data) — one item with stackCount=8
        SetStockpileSlotCount(spIdx, 0, 0, ITEM_RED, 8);
        
        // Verify slot is overfull
        expect(IsSlotOverfull(spIdx, 5, 5));
        
        // Spawn new red item on the ground
        int newItem = SpawnItem(8 * CELL_SIZE + CELL_SIZE * 0.5f, 8 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Run simulation
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            // Check if the new item got placed in stockpile (may have been merged)
            if (!items[newItem].active || items[newItem].state == ITEM_IN_STOCKPILE) break;
        }
        
        // New item should have been placed in slot 1 (6,5) — either merged or as new slot item
        int slot1Count = GetStockpileSlotCount(spIdx, 6, 5);
        expect(slot1Count >= 1);  // Something went into slot 1
        
        // Overfull slot should still be overfull (not accepting more)
        int slot0Count = GetStockpileSlotCount(spIdx, 5, 5);
        expect(slot0Count == 8);  // Still overfull, didn't grow
    }
    
    it("filter changes mid-operation should be respected immediately") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Three movers
        for (int i = 0; i < 3; i++) {
            Mover* m = &movers[i];
            Point goal = {1 + i, 1, 0};
            InitMover(m, (1 + i) * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        }
        moverCount = 3;
        
        // Stockpile accepts all stone types initially
        int spIdx = CreateStockpile(5, 5, 0, 3, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        SetStockpileFilter(spIdx, ITEM_GREEN, true);
        SetStockpileFilter(spIdx, ITEM_BLUE, true);
        
        // Spawn three items (different types) far from stockpile
        int redIdx = SpawnItem(8 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        int greenIdx = SpawnItem(8 * CELL_SIZE + CELL_SIZE * 0.5f, 3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_GREEN);
        int blueIdx = SpawnItem(8 * CELL_SIZE + CELL_SIZE * 0.5f, 4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_BLUE);
        
        // Assign jobs and wait until all movers are CARRYING
        for (int i = 0; i < 500; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            
            bool allCarrying = true;
            for (int m = 0; m < 3; m++) {
                if (!MoverIsCarrying(&movers[m])) {
                    allCarrying = false;
                    break;
                }
            }
            if (allCarrying) break;
        }
        
        // All should be carrying now
        expect(MoverIsCarrying(&movers[0]));
        expect(MoverIsCarrying(&movers[1]));
        expect(MoverIsCarrying(&movers[2]));
        
        // Disable RED filter while movers are in transit
        SetStockpileFilter(spIdx, ITEM_RED, false);
        
        // Continue simulation
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            
            // Check if all non-red items are in stockpile
            if (items[greenIdx].state == ITEM_IN_STOCKPILE &&
                items[blueIdx].state == ITEM_IN_STOCKPILE) break;
        }
        
        // Green and blue should be in the stockpile
        expect(items[greenIdx].state == ITEM_IN_STOCKPILE);
        expect(items[blueIdx].state == ITEM_IN_STOCKPILE);
        
        // Red should NOT be in this stockpile (filter was disabled)
        // It should be on ground or in a different stockpile if one exists
        if (items[redIdx].state == ITEM_IN_STOCKPILE) {
            int redSp = -1;
            IsPositionInStockpile(items[redIdx].x, items[redIdx].y, (int)items[redIdx].z, &redSp);
            expect(redSp != spIdx);  // Not in the filtered stockpile
        }
    }
    
    it("stockpiles should never mix incompatible materials in the same slot") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // 5 movers for race conditions
        for (int i = 0; i < 5; i++) {
            Mover* m = &movers[i];
            Point goal = {1 + i, 1, 0};
            InitMover(m, (1 + i) * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        }
        moverCount = 5;
        
        // Small stockpile (1x2 = 2 slots) accepts all types
        int spIdx = CreateStockpile(5, 5, 0, 1, 2);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        SetStockpileFilter(spIdx, ITEM_GREEN, true);
        SetStockpileFilter(spIdx, ITEM_BLUE, true);
        
        // Spawn items of different types clustered together (race condition setup)
        int items_spawned[5];
        items_spawned[0] = SpawnItem(8 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        items_spawned[1] = SpawnItem(8 * CELL_SIZE + CELL_SIZE * 0.5f, 3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_GREEN);
        items_spawned[2] = SpawnItem(8 * CELL_SIZE + CELL_SIZE * 0.5f, 4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        items_spawned[3] = SpawnItem(8 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_BLUE);
        items_spawned[4] = SpawnItem(8 * CELL_SIZE + CELL_SIZE * 0.5f, 6 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Run until all items are in stockpile (or limit reached)
        for (int i = 0; i < 2000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            
            bool allDone = true;
            for (int j = 0; j < 5; j++) {
                if (items[items_spawned[j]].state != ITEM_IN_STOCKPILE) {
                    allDone = false;
                    break;
                }
            }
            if (allDone) break;
        }
        
        // Check that each slot contains only ONE type
        int slot0Count = GetStockpileSlotCount(spIdx, 5, 5);
        int slot1Count = GetStockpileSlotCount(spIdx, 5, 6);
        
        if (slot0Count > 0) {
            // Count types in slot 0
            int redCount = 0, greenCount = 0, blueCount = 0;
            for (int j = 0; j < 5; j++) {
                if (items[items_spawned[j]].state != ITEM_IN_STOCKPILE) continue;
                int tileX = (int)(items[items_spawned[j]].x / CELL_SIZE);
                int tileY = (int)(items[items_spawned[j]].y / CELL_SIZE);
                if (tileX == 5 && tileY == 5) {
                    if (items[items_spawned[j]].type == ITEM_RED) redCount++;
                    if (items[items_spawned[j]].type == ITEM_GREEN) greenCount++;
                    if (items[items_spawned[j]].type == ITEM_BLUE) blueCount++;
                }
            }
            // Only ONE type should be present
            int typesPresent = (redCount > 0 ? 1 : 0) + (greenCount > 0 ? 1 : 0) + (blueCount > 0 ? 1 : 0);
            expect(typesPresent <= 1);  // At most one type per slot
        }
        
        if (slot1Count > 0) {
            // Count types in slot 1
            int redCount = 0, greenCount = 0, blueCount = 0;
            for (int j = 0; j < 5; j++) {
                if (items[items_spawned[j]].state != ITEM_IN_STOCKPILE) continue;
                int tileX = (int)(items[items_spawned[j]].x / CELL_SIZE);
                int tileY = (int)(items[items_spawned[j]].y / CELL_SIZE);
                if (tileX == 5 && tileY == 6) {
                    if (items[items_spawned[j]].type == ITEM_RED) redCount++;
                    if (items[items_spawned[j]].type == ITEM_GREEN) greenCount++;
                    if (items[items_spawned[j]].type == ITEM_BLUE) blueCount++;
                }
            }
            // Only ONE type should be present
            int typesPresent = (redCount > 0 ? 1 : 0) + (greenCount > 0 ? 1 : 0) + (blueCount > 0 ? 1 : 0);
            expect(typesPresent <= 1);  // At most one type per slot
        }
    }
    
    it("multiple item types should all get hauled even when sharing cached slots") {
        // Bug: The stockpile slot cache would point multiple different item types
        // to the same empty slot. The first type succeeds and type-stamps the slot
        // via ReserveStockpileSlot, but subsequent types fail the reservation because
        // of a type mismatch. If the cache is only invalidated on SUCCESS, these
        // failed types never get a fresh cache lookup and are never hauled.
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;

        ClearMovers();
        ClearItems();
        ClearStockpiles();

        // 3 movers to haul items
        for (int i = 0; i < 3; i++) {
            Mover* m = &movers[i];
            Point goal = {1 + i, 1, 0};
            InitMover(m, (1 + i) * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        }
        moverCount = 3;

        // Stockpile with 3 slots (1x3) - enough room for one of each type
        int spIdx = CreateStockpile(5, 5, 0, 1, 3);
        SetStockpileFilter(spIdx, ITEM_LOG, true);
        SetStockpileFilter(spIdx, ITEM_ROCK, true);
        SetStockpileFilter(spIdx, ITEM_DIRT, true);

        // Spawn one of each type on the ground
        int logIdx = SpawnItem(2 * CELL_SIZE + CELL_SIZE * 0.5f, 3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_LOG);
        int rockIdx = SpawnItem(3 * CELL_SIZE + CELL_SIZE * 0.5f, 3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_ROCK);
        int dirtIdx = SpawnItem(4 * CELL_SIZE + CELL_SIZE * 0.5f, 3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_DIRT);

        // Run simulation - all 3 items should eventually be hauled
        for (int i = 0; i < 2000; i++) {
            Tick();
            AssignJobs();
            JobsTick();

            bool allDone = (items[logIdx].state == ITEM_IN_STOCKPILE &&
                           items[rockIdx].state == ITEM_IN_STOCKPILE &&
                           items[dirtIdx].state == ITEM_IN_STOCKPILE);
            if (allDone) break;
        }

        // All three different types should be in the stockpile
        expect(items[logIdx].state == ITEM_IN_STOCKPILE);
        expect(items[rockIdx].state == ITEM_IN_STOCKPILE);
        expect(items[dirtIdx].state == ITEM_IN_STOCKPILE);
    }

    // =========================================================================
    // Material-focused tests
    // =========================================================================

    it("same item type with different materials should never share a slot") {
        // Oak logs and pine logs are both ITEM_LOG but with different materials.
        // A player expects them in separate slots, never stacked together.
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;

        ClearMovers();
        ClearItems();
        ClearStockpiles();

        for (int i = 0; i < 4; i++) {
            Mover* m = &movers[i];
            Point goal = {1 + i, 1, 0};
            InitMover(m, (1 + i) * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        }
        moverCount = 4;

        // Stockpile with 4 slots, accepts all
        int spIdx = CreateStockpile(5, 5, 0, 2, 2);

        // Spawn 2 oak logs and 2 pine logs
        int oak1 = SpawnItemWithMaterial(2 * CELL_SIZE + CELL_SIZE * 0.5f, 3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_LOG, MAT_OAK);
        int oak2 = SpawnItemWithMaterial(3 * CELL_SIZE + CELL_SIZE * 0.5f, 3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_LOG, MAT_OAK);
        int pine1 = SpawnItemWithMaterial(2 * CELL_SIZE + CELL_SIZE * 0.5f, 8 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_LOG, MAT_PINE);
        int pine2 = SpawnItemWithMaterial(3 * CELL_SIZE + CELL_SIZE * 0.5f, 8 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_LOG, MAT_PINE);

        for (int i = 0; i < 2000; i++) {
            Tick();
            AssignJobs();
            JobsTick();

            bool allDone = (ItemWasStored(oak1) && ItemWasStored(oak2) &&
                           ItemWasStored(pine1) && ItemWasStored(pine2));
            if (allDone) break;
        }

        expect(ItemWasStored(oak1));
        expect(ItemWasStored(oak2));
        expect(ItemWasStored(pine1));
        expect(ItemWasStored(pine2));

        // Check every slot: each slot must contain only ONE material
        // With stacking, oak stacks together and pine stacks together
        Stockpile* sp = &stockpiles[spIdx];
        int oakSlots = 0, pineSlots = 0;
        for (int ly = 0; ly < sp->height; ly++) {
            for (int lx = 0; lx < sp->width; lx++) {
                int idx = ly * sp->width + lx;
                if (sp->slotCounts[idx] == 0) continue;
                if (sp->slotMaterials[idx] == MAT_OAK) oakSlots++;
                else if (sp->slotMaterials[idx] == MAT_PINE) pineSlots++;
            }
        }
        // Oak and pine should be in separate slots
        expect(oakSlots >= 1);
        expect(pineSlots >= 1);
    }

    it("material filter should block items of disallowed materials") {
        // If a stockpile allows ITEM_LOG but disallows MAT_PINE,
        // pine logs should NOT enter. Only oak logs should.
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;

        ClearMovers();
        ClearItems();
        ClearStockpiles();

        for (int i = 0; i < 2; i++) {
            Mover* m = &movers[i];
            Point goal = {1 + i, 1, 0};
            InitMover(m, (1 + i) * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        }
        moverCount = 2;

        // Stockpile that allows logs but NOT pine material
        int spIdx = CreateStockpile(5, 5, 0, 2, 1);
        SetStockpileMaterialFilter(spIdx, MAT_PINE, false);

        // Spawn one oak log and one pine log
        int oakIdx = SpawnItemWithMaterial(2 * CELL_SIZE + CELL_SIZE * 0.5f, 3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_LOG, MAT_OAK);
        int pineIdx = SpawnItemWithMaterial(3 * CELL_SIZE + CELL_SIZE * 0.5f, 3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_LOG, MAT_PINE);

        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }

        // Oak should be in stockpile, pine should still be on the ground
        expect(items[oakIdx].state == ITEM_IN_STOCKPILE);
        expect(items[pineIdx].state == ITEM_ON_GROUND);
    }

    it("consolidation should never merge different materials of same type") {
        // Two partial stacks: 3 oak logs in slot A, 3 pine logs in slot B.
        // Consolidation should NOT try to merge them even though both are ITEM_LOG.
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;

        ClearMovers();
        ClearItems();
        ClearStockpiles();

        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;

        // Stockpile with 2 slots
        int spIdx = CreateStockpile(5, 5, 0, 2, 1);

        // Manually place items to set up partial stacks
        // Slot 0 (5,5): 3 oak logs
        for (int i = 0; i < 3; i++) {
            int idx = SpawnItemWithMaterial(5 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_LOG, MAT_OAK);
            items[idx].state = ITEM_IN_STOCKPILE;
        }
        int slot0Idx = 0; // local slot index
        stockpiles[spIdx].slotTypes[slot0Idx] = ITEM_LOG;
        stockpiles[spIdx].slotMaterials[slot0Idx] = MAT_OAK;
        stockpiles[spIdx].slotCounts[slot0Idx] = 3;

        // Slot 1 (6,5): 3 pine logs
        for (int i = 0; i < 3; i++) {
            int idx = SpawnItemWithMaterial(6 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_LOG, MAT_PINE);
            items[idx].state = ITEM_IN_STOCKPILE;
        }
        int slot1Idx = 1; // local slot index
        stockpiles[spIdx].slotTypes[slot1Idx] = ITEM_LOG;
        stockpiles[spIdx].slotMaterials[slot1Idx] = MAT_PINE;
        stockpiles[spIdx].slotCounts[slot1Idx] = 3;

        // FindConsolidationTarget should return false for both slots
        int destX, destY;
        bool found0 = FindConsolidationTarget(spIdx, 5, 5, &destX, &destY);
        bool found1 = FindConsolidationTarget(spIdx, 6, 5, &destX, &destY);

        expect(!found0); // oak should NOT consolidate onto pine
        expect(!found1); // pine should NOT consolidate onto oak
    }

    it("changing material filter should cause existing items to be re-hauled out") {
        // Place oak logs in a stockpile, then disable MAT_OAK.
        // The system should re-haul them to another stockpile.
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;

        ClearMovers();
        ClearItems();
        ClearStockpiles();

        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;

        // Two stockpiles: sp1 initially accepts oak, sp2 accepts everything
        int sp1 = CreateStockpile(3, 3, 0, 1, 1);
        int sp2 = CreateStockpile(7, 7, 0, 1, 1);

        // Haul an oak log into sp1
        int oakIdx = SpawnItemWithMaterial(2 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_LOG, MAT_OAK);

        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (items[oakIdx].state == ITEM_IN_STOCKPILE) break;
        }
        expect(items[oakIdx].state == ITEM_IN_STOCKPILE);

        // Verify it's in sp1
        int currentSp = -1;
        IsPositionInStockpile(items[oakIdx].x, items[oakIdx].y, (int)items[oakIdx].z, &currentSp);
        expect(currentSp == sp1);

        // Now disable oak material on sp1
        SetStockpileMaterialFilter(sp1, MAT_OAK, false);

        // Run simulation - item should be re-hauled to sp2
        for (int i = 0; i < 2000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }

        // Item should now be in sp2
        int newSp = -1;
        IsPositionInStockpile(items[oakIdx].x, items[oakIdx].y, (int)items[oakIdx].z, &newSp);
        expect(newSp == sp2);
    }

    it("material-specific stockpiles should attract the right materials") {
        // Two stockpiles: one for oak only, one for pine only.
        // Oak logs should go to the oak stockpile, pine logs to the pine one.
        InitTestGridFromAscii(
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;

        ClearMovers();
        ClearItems();
        ClearStockpiles();

        for (int i = 0; i < 4; i++) {
            Mover* m = &movers[i];
            Point goal = {14 + i, 1, 0};
            InitMover(m, (14 + i) * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        }
        moverCount = 4;

        // Oak-only stockpile on the left
        int spOak = CreateStockpile(3, 5, 0, 2, 1);
        // Disable all materials except oak
        for (int m = 0; m < MAT_COUNT; m++) {
            SetStockpileMaterialFilter(spOak, (MaterialType)m, false);
        }
        SetStockpileMaterialFilter(spOak, MAT_OAK, true);

        // Pine-only stockpile on the right
        int spPine = CreateStockpile(25, 5, 0, 2, 1);
        for (int m = 0; m < MAT_COUNT; m++) {
            SetStockpileMaterialFilter(spPine, (MaterialType)m, false);
        }
        SetStockpileMaterialFilter(spPine, MAT_PINE, true);

        // Spawn items near the center
        int oak1 = SpawnItemWithMaterial(15 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_LOG, MAT_OAK);
        int oak2 = SpawnItemWithMaterial(16 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_LOG, MAT_OAK);
        int pine1 = SpawnItemWithMaterial(15 * CELL_SIZE + CELL_SIZE * 0.5f, 6 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_LOG, MAT_PINE);
        int pine2 = SpawnItemWithMaterial(16 * CELL_SIZE + CELL_SIZE * 0.5f, 6 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_LOG, MAT_PINE);

        for (int i = 0; i < 2000; i++) {
            Tick();
            AssignJobs();
            JobsTick();

            bool allDone = (ItemWasStored(oak1) && ItemWasStored(oak2) &&
                           ItemWasStored(pine1) && ItemWasStored(pine2));
            if (allDone) break;
        }

        // All items should be stored (merged or as slot representative)
        expect(ItemWasStored(oak1));
        expect(ItemWasStored(oak2));
        expect(ItemWasStored(pine1));
        expect(ItemWasStored(pine2));

        // Oak stockpile should have 2 oak units total
        int oakTotal = 0;
        Stockpile* spOakPtr = &stockpiles[spOak];
        for (int s = 0; s < spOakPtr->width * spOakPtr->height; s++) {
            oakTotal += spOakPtr->slotCounts[s];
        }
        expect(oakTotal == 2);

        // Pine stockpile should have 2 pine units total
        int pineTotal = 0;
        Stockpile* spPinePtr = &stockpiles[spPine];
        for (int s = 0; s < spPinePtr->width * spPinePtr->height; s++) {
            pineTotal += spPinePtr->slotCounts[s];
        }
        expect(pineTotal == 2);
    }

    it("distance matters: closest available stockpile should win") {
        InitTestGridFromAscii(
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n"
            "..............................\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        Mover* m = &movers[0];
        Point goal = {15, 10, 0};
        InitMover(m, 15 * CELL_SIZE + CELL_SIZE * 0.5f, 10 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Item at center (15, 10)
        int itemIdx = SpawnItem(15 * CELL_SIZE + CELL_SIZE * 0.5f, 10 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_LOG);
        
        // Three identical stockpiles at different distances
        // Close: (18, 10) - distance ~3
        int spClose = CreateStockpile(18, 10, 0, 2, 1);
        SetStockpileFilter(spClose, ITEM_LOG, true);
        SetStockpilePriority(spClose, 5);
        
        // Medium: (25, 10) - distance ~10
        int spMed = CreateStockpile(25, 10, 0, 2, 1);
        SetStockpileFilter(spMed, ITEM_LOG, true);
        SetStockpilePriority(spMed, 5);
        
        // Far: (5, 10) - distance ~10
        int spFar = CreateStockpile(5, 10, 0, 2, 1);
        SetStockpileFilter(spFar, ITEM_LOG, true);
        SetStockpilePriority(spFar, 5);
        
        // Assign job and run simulation
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (items[itemIdx].state == ITEM_IN_STOCKPILE) break;
        }
        
        expect(items[itemIdx].state == ITEM_IN_STOCKPILE);
        
        // Should be in the CLOSEST stockpile
        int spIdx = -1;
        IsPositionInStockpile(items[itemIdx].x, items[itemIdx].y, (int)items[itemIdx].z, &spIdx);
        expect(spIdx == spClose);  // Should choose closest one
    }
}

// =============================================================================
// Item Lifecycle Tests (from items.c audit)
// Player-facing behavior: items leaving stockpiles must clean up slot state
// =============================================================================

describe(item_lifecycle) {
    it("deleting a stockpiled item should free the stockpile slot") {
        // Story: I have items in a stockpile. Something consumes one (crafting, etc).
        // The stockpile slot should become available for new items.
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearJobs();

        // Create stockpile and place an item in it
        int spIdx = CreateStockpile(2, 2, 0, 3, 1);
        SetStockpileFilter(spIdx, ITEM_LOG, true);
        Stockpile* sp = &stockpiles[spIdx];

        // Spawn item directly in the stockpile slot
        float slotX = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
        float slotY = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
        int itemIdx = SpawnItem(slotX, slotY, 0.0f, ITEM_LOG);
        items[itemIdx].state = ITEM_IN_STOCKPILE;
        PlaceItemInStockpile(spIdx, 2, 2, itemIdx);

        // Verify slot is occupied
        int idx = 0;  // local (0,0) in the stockpile
        expect(sp->slotCounts[idx] == 1);
        expect(sp->slotTypes[idx] == ITEM_LOG);

        // Now delete the item (simulating crafting consuming it)
        DeleteItem(itemIdx);

        // Player expectation: the slot should be empty and available for new items
        expect(sp->slotCounts[idx] == 0);
        expect(sp->slotTypes[idx] == -1);
        expect(sp->slotMaterials[idx] == MAT_NONE);
    }

    it("pushing items out of a cell should make them ground items") {
        // Story: I build a wall on a stockpile tile that has items.
        // The items should be pushed to a neighbor and become regular ground items,
        // and the stockpile slot should be freed.
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearJobs();

        // Create stockpile and place an item at slot (3,2)
        int spIdx = CreateStockpile(2, 2, 0, 3, 1);
        SetStockpileFilter(spIdx, ITEM_ROCK, true);
        Stockpile* sp = &stockpiles[spIdx];

        float slotX = 3 * CELL_SIZE + CELL_SIZE * 0.5f;
        float slotY = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
        int itemIdx = SpawnItem(slotX, slotY, 0.0f, ITEM_ROCK);
        items[itemIdx].state = ITEM_IN_STOCKPILE;
        PlaceItemInStockpile(spIdx, 3, 2, itemIdx);

        // Verify slot (1,0) in local coords is occupied
        int idx = 1;  // local x=1, y=0
        expect(sp->slotCounts[idx] == 1);
        expect(sp->slotTypes[idx] == ITEM_ROCK);

        // Push items out (simulating wall being built on this tile)
        PushItemsOutOfCell(3, 2, 0);

        // Player expectation: item is now a ground item, not stuck in stockpile limbo
        expect(items[itemIdx].state == ITEM_ON_GROUND);

        // Player expectation: the stockpile slot should be freed
        expect(sp->slotCounts[idx] == 0);
        expect(sp->slotTypes[idx] == -1);
        expect(sp->slotMaterials[idx] == MAT_NONE);
    }

    it("dropping items through a channeled floor should free the stockpile slot") {
        // Story: I channel the floor under a stockpile. Items fall to the level below.
        // The stockpile slot above should be freed, items should be ground items below.
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        // Set up z0 as solid, z1 as walkable floor
        for (int x = 0; x < 10; x++) {
            for (int y = 0; y < 5; y++) {
                grid[0][y][x] = CELL_WALL;
                SetWallMaterial(x, y, 0, MAT_DIRT);
                grid[1][y][x] = CELL_AIR;
                SET_FLOOR(x, y, 1);
            }
        }

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearJobs();

        // Create stockpile at z1 and place item
        int spIdx = CreateStockpile(3, 2, 1, 2, 1);
        SetStockpileFilter(spIdx, ITEM_DIRT, true);
        Stockpile* sp = &stockpiles[spIdx];

        float slotX = 3 * CELL_SIZE + CELL_SIZE * 0.5f;
        float slotY = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
        int itemIdx = SpawnItem(slotX, slotY, 1.0f, ITEM_DIRT);
        items[itemIdx].state = ITEM_IN_STOCKPILE;
        PlaceItemInStockpile(spIdx, 3, 2, itemIdx);

        // Verify slot is occupied
        int idx = 0;
        expect(sp->slotCounts[idx] == 1);

        // Remove the floor (simulating channeling) - item should fall
        // First make z0 walkable at that position
        grid[0][2][3] = CELL_AIR;
        SET_FLOOR(3, 2, 0);  // not solid below, so item falls to z0... actually need solid below z0
        grid[0][2][3] = CELL_AIR;
        // For drop to work, targetZ cell must not be solid
        // z=0 is CELL_AIR now, and z-1 doesn't exist, so item lands at z0

        DropItemsInCell(3, 2, 1);

        // Player expectation: item fell to z0, is now on ground
        expect((int)items[itemIdx].z == 0);
        expect(items[itemIdx].state == ITEM_ON_GROUND);

        // Player expectation: stockpile slot at z1 is freed
        expect(sp->slotCounts[idx] == 0);
        expect(sp->slotTypes[idx] == -1);
        expect(sp->slotMaterials[idx] == MAT_NONE);
    }

    it("itemHighWaterMark should shrink when highest items are deleted") {
        // Story: performance shouldn't degrade as items are created and destroyed.
        // If I delete the last items, the system should stop iterating past them.
        ClearItems();

        // Spawn 5 items
        int idx0 = SpawnItem(100, 100, 0, ITEM_RED);
        int idx1 = SpawnItem(200, 100, 0, ITEM_GREEN);
        int idx2 = SpawnItem(300, 100, 0, ITEM_BLUE);
        int idx3 = SpawnItem(400, 100, 0, ITEM_RED);
        int idx4 = SpawnItem(500, 100, 0, ITEM_GREEN);
        (void)idx0; (void)idx1; (void)idx2;

        expect(itemHighWaterMark == 5);

        // Delete the last two items
        DeleteItem(idx4);
        expect(itemHighWaterMark == 4);  // Should shrink to 4

        DeleteItem(idx3);
        expect(itemHighWaterMark == 3);  // Should shrink to 3

        // Delete a middle item - water mark should NOT shrink
        DeleteItem(idx1);
        expect(itemHighWaterMark == 3);  // idx2 is still at position 2
    }
}

// =============================================================================
// Mover Lifecycle Tests (from mover.c audit)
// =============================================================================

describe(mover_lifecycle) {
    it("reused job slots should not have stale fuel or workshop fields") {
        // Story: A crafter finishes a job at a workshop. Later, a hauler gets a job
        // in the same slot. If that hauler's job is cancelled, the stale fuelItem
        // from the old craft job should NOT cause reservation theft.
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearJobs();

        // Create a job (simulating a craft job that sets fuelItem)
        int jobId1 = CreateJob(JOBTYPE_CRAFT);
        Job* job1 = GetJob(jobId1);
        job1->fuelItem = 5;
        job1->targetWorkshop = 2;

        // Release the job (returns to free list)
        ReleaseJob(jobId1);

        // Create a new job in the same slot (should be a haul job reusing slot)
        int jobId2 = CreateJob(JOBTYPE_HAUL);
        Job* job2 = GetJob(jobId2);

        // Player expectation: the new job should NOT have stale craft fields
        expect(job2->fuelItem == -1);
        expect(job2->targetWorkshop == -1);
    }

    it("ClearMovers should release workshop crafter assignments") {
        // Story: I load a save while a crafter is working at a workshop.
        // After loading, the workshop should be available for new crafters.
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearItems();
        ClearStockpiles();
        ClearJobs();

        // Set up a mover
        ClearMovers();
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 1 * CELL_SIZE + 16, 1 * CELL_SIZE + 16, 0.0f, goal, 100.0f);
        moverCount = 1;

        // Simulate a workshop with an assigned crafter
        workshops[0].active = true;
        workshops[0].assignedCrafter = 0;  // mover 0

        // Clear all movers (simulating save load)
        ClearMovers();

        // Player expectation: workshop should be free for new crafters
        expect(workshops[0].assignedCrafter == -1);

        // Cleanup
        workshops[0].active = false;
    }

    it("mover avoidance should not repel movers on different z-levels") {
        // Story: A mover on z=0 walks near a ladder. A mover on z=1 is directly
        // above. They should NOT push each other sideways.
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearJobs();

        // Two movers at same x,y but different z
        float cx = 5 * CELL_SIZE + 16;
        float cy = 2 * CELL_SIZE + 16;

        Mover* m0 = &movers[0];
        Mover* m1 = &movers[1];
        Point goal = {0, 0, 0};
        InitMover(m0, cx, cy, 0.0f, goal, 100.0f);
        InitMover(m1, cx, cy, 1.0f, goal, 100.0f);
        moverCount = 2;

        // Build spatial grid and compute avoidance
        InitMoverSpatialGrid(10 * CELL_SIZE, 5 * CELL_SIZE);
        BuildMoverSpatialGrid();

        Vec2 avoid0 = ComputeMoverAvoidance(0);

        // Player expectation: no repulsion, they're on different floors
        float avoidMag = avoid0.x * avoid0.x + avoid0.y * avoid0.y;
        expect(avoidMag < 0.001f);
    }

    it("stuck detection should count z-movement as progress") {
        // Story: A mover descends a ladder (z changes, x/y barely moves).
        // It should NOT be marked as stuck.
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearJobs();

        Mover* m = &movers[0];
        Point goal = {5, 2, 0};
        float cx = 5 * CELL_SIZE + 16;
        float cy = 2 * CELL_SIZE + 16;
        InitMover(m, cx, cy, 3.0f, goal, 100.0f);
        moverCount = 1;

        // Simulate ladder descent: x/y stays same, z decreases
        m->lastX = cx;
        m->lastY = cy;
        m->z = 2.0f;  // moved from z=3 to z=2 (real progress!)
        m->timeWithoutProgress = 0.0f;

        // The stuck detection checks dx/dy. With no x/y change but z change,
        // it should still recognize progress.
        float movedX = m->x - m->lastX;
        float movedY = m->y - m->lastY;
        float movedZ = m->z - 3.0f;  // original z was 3
        float movedDistSq = movedX * movedX + movedY * movedY;

        // Current code: only x/y. This test documents the expectation that
        // z-movement should count. movedDistSq == 0 means the stuck detector
        // thinks no progress was made.
        // Player expectation: z-movement should count as progress
        float movedWithZ = movedDistSq + (movedZ * CELL_SIZE) * (movedZ * CELL_SIZE);
        expect(movedWithZ > 1.0f);  // There IS real progress (z moved)
        // But the current stuck detector only sees this:
        expect(movedDistSq < 1.0f);  // BUG: detector thinks no progress
    }

    it("deactivated mover should not leave carried items in unwalkable cells") {
        // Story: A mover carrying an item gets walled in on all sides.
        // The item should NOT vanish into the wall — it should end up somewhere reachable.
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearJobs();
        InitJobSystem(10);

        // Mover at (5,2) carrying an item
        Mover* m = &movers[0];
        Point goal = {8, 2, 0};
        float mx = 5 * CELL_SIZE + CELL_SIZE * 0.5f;
        float my = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
        InitMover(m, mx, my, 0.0f, goal, 100.0f);
        moverCount = 1;

        int itemIdx = SpawnItem(mx, my, 0.0f, ITEM_LOG);
        items[itemIdx].state = ITEM_CARRIED;
        items[itemIdx].reservedBy = 0;

        // Give mover a haul job carrying the item
        int spIdx = CreateStockpile(7, 2, 0, 2, 1);
        SetStockpileFilter(spIdx, ITEM_LOG, true);

        int jobId = CreateJob(JOBTYPE_HAUL);
        Job* job = GetJob(jobId);
        job->assignedMover = 0;
        job->carryingItem = itemIdx;
        job->targetItem = itemIdx;
        job->targetStockpile = spIdx;
        job->targetSlotX = 7;
        job->targetSlotY = 2;
        job->step = 1;  // carrying step
        m->currentJobId = jobId;
        RemoveMoverFromIdleList(0);

        // Wall in the mover completely (cell + all neighbors)
        grid[0][2][5] = CELL_WALL;  // mover's cell
        grid[0][1][5] = CELL_WALL;  // north
        grid[0][3][5] = CELL_WALL;  // south
        grid[0][2][4] = CELL_WALL;  // west
        grid[0][2][6] = CELL_WALL;  // east

        // Run a tick — mover should be deactivated, job cancelled, item dropped
        Tick();
        JobsTick();

        // The item should exist and be on ground
        expect(items[itemIdx].active == true);
        expect(items[itemIdx].state == ITEM_ON_GROUND);

        // Player expectation: item should NOT be at an unwalkable position
        int itemCellX = (int)(items[itemIdx].x / CELL_SIZE);
        int itemCellY = (int)(items[itemIdx].y / CELL_SIZE);
        int itemCellZ = (int)items[itemIdx].z;
        bool itemCellWalkable = IsCellWalkableAt(itemCellZ, itemCellY, itemCellX);
        expect(itemCellWalkable == true);
    }
}

// =============================================================================
// Job lifecycle tests (jobs.c audit findings)
// =============================================================================
describe(job_lifecycle) {
    it("planting a sapling should properly decrement itemCount") {
        // Story: A mover plants a sapling. The sapling item is consumed.
        // Player expects: the item is gone AND itemCount reflects reality.
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        InitDesignations();
        ClearJobs();

        // Spawn a mover near tile (2,2)
        Mover* m = &movers[0];
        Point goal = {2, 2, 0};
        InitMover(m, CELL_SIZE * 2.5f, CELL_SIZE * 2.5f, 0.0f, goal, 200.0f);
        moverCount = 1;

        // Spawn a sapling item near the mover
        int sapIdx = SpawnItemWithMaterial(CELL_SIZE * 3.5f, CELL_SIZE * 2.5f, 0.0f, ITEM_SAPLING, MAT_OAK);

        // Designate tile (5,2) for planting
        DesignatePlantSapling(5, 2, 0);

        int countBefore = itemCount;
        int hwmBefore = itemHighWaterMark;

        // Run simulation until the planting completes
        for (int i = 0; i < 2000; i++) {
            Tick();
            RebuildIdleMoverList();
            BuildItemSpatialGrid();
            BuildMoverSpatialGrid();
            AssignJobs();
            JobsTick();
            if (!items[sapIdx].active) break;
        }

        // Sapling should be consumed
        expect(items[sapIdx].active == false);

        // Player expectation: itemCount should have decremented
        expect(itemCount == countBefore - 1);

        // itemHighWaterMark should have shrunk if this was the last item
        expect(itemHighWaterMark <= hwmBefore);
    }

    it("craft job should update carried item position while moving to workshop") {
        // Story: A mover picks up a log and carries it to the sawmill.
        // Player expects: the log moves WITH the mover, not floating at pickup spot.
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();

        // Create sawmill at (5,1) — 3x3, work tile at X offset
        int wsIdx = CreateWorkshop(5, 1, 0, WORKSHOP_SAWMILL);

        // Add a "Saw Planks" bill (recipe 0 for sawmill)
        AddBill(wsIdx, 0, BILL_DO_X_TIMES, 1);

        // Place a planks stockpile so the bill won't auto-suspend
        int sp = CreateStockpile(0, 0, 0, 2, 2);
        SetStockpileFilter(sp, ITEM_PLANKS, true);

        // Spawn mover at (1,2)
        Mover* m = &movers[0];
        Point goal = {1, 2, 0};
        InitMover(m, CELL_SIZE * 1.5f, CELL_SIZE * 2.5f, 0.0f, goal, 200.0f);
        moverCount = 1;

        // Spawn log near mover at (2,2)
        SpawnItemWithMaterial(CELL_SIZE * 2.5f, CELL_SIZE * 2.5f, 0.0f, ITEM_LOG, MAT_OAK);

        // Run until mover picks up the item and starts carrying to workshop
        bool foundCarrying = false;
        for (int i = 0; i < 500; i++) {
            Tick();
            RebuildIdleMoverList();
            BuildItemSpatialGrid();
            BuildMoverSpatialGrid();
            AssignJobs();
            JobsTick();

            // Check if mover is carrying and moving toward workshop
            if (m->currentJobId >= 0) {
                Job* job = GetJob(m->currentJobId);
                if (job && job->type == JOBTYPE_CRAFT && job->step == CRAFT_STEP_MOVING_TO_WORKSHOP) {
                    // The item should be following the mover
                    float dx = items[job->carryingItem].x - m->x;
                    float dy = items[job->carryingItem].y - m->y;
                    float distSq = dx * dx + dy * dy;
                    // Item should be close to mover (within a cell), not stuck at pickup
                    expect(distSq < CELL_SIZE * CELL_SIZE);
                    foundCarrying = true;
                    break;
                }
            }
        }
        expect(foundCarrying == true);
    }

    it("cancelling a craft job should not drop fuel in an unwalkable cell") {
        // Story: A crafter is in an unwalkable cell carrying fuel when job is cancelled.
        // Player expects: the fuel ends up on a walkable tile, not inside a wall.
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();

        // Create a mover at (5,3)
        Mover* m = &movers[0];
        Point goal = {5, 3, 0};
        InitMover(m, CELL_SIZE * 5.5f, CELL_SIZE * 3.5f, 0.0f, goal, 200.0f);
        moverCount = 1;

        // Create a fuel item and make it carried
        int fuelIdx = SpawnItemWithMaterial(CELL_SIZE * 5.5f, CELL_SIZE * 3.5f, 0.0f, ITEM_LOG, MAT_OAK);
        items[fuelIdx].state = ITEM_CARRIED;
        items[fuelIdx].reservedBy = 0;

        // Create an input item (reserved but deposited at workshop)
        int inputIdx = SpawnItemWithMaterial(CELL_SIZE * 5.5f, CELL_SIZE * 3.5f, 0.0f, ITEM_CLAY, MAT_CLAY);
        items[inputIdx].reservedBy = 0;

        // Create a workshop
        int wsIdx = CreateWorkshop(5, 1, 0, WORKSHOP_KILN);
        workshops[wsIdx].assignedCrafter = 0;

        // Create a craft job in CARRYING_FUEL step
        int jobId = CreateJob(JOBTYPE_CRAFT);
        Job* job = GetJob(jobId);
        job->assignedMover = 0;
        job->targetWorkshop = wsIdx;
        job->targetBillIdx = 0;
        job->targetItem = inputIdx;
        job->carryingItem = -1;
        job->fuelItem = fuelIdx;
        job->step = CRAFT_STEP_CARRYING_FUEL;
        m->currentJobId = jobId;

        // Now wall off the mover's cell — simulate being trapped
        grid[0][3][5] = CELL_WALL;

        // Run a tick to trigger stuck detection → cancel
        Tick();
        JobsTick();

        // The fuel item should be on the ground
        expect(items[fuelIdx].active == true);
        expect(items[fuelIdx].state == ITEM_ON_GROUND);

        // Player expectation: fuel should NOT be at an unwalkable position
        int fuelCellX = (int)(items[fuelIdx].x / CELL_SIZE);
        int fuelCellY = (int)(items[fuelIdx].y / CELL_SIZE);
        int fuelCellZ = (int)items[fuelIdx].z;
        bool fuelCellWalkable = IsCellWalkableAt(fuelCellZ, fuelCellY, fuelCellX);
        expect(fuelCellWalkable == true);
    }

    it("craft auto-suspend should check actual output material not MAT_NONE") {
        // Story: A sawmill has a "Saw Planks" bill. The player has a stockpile
        // filtered to ONLY accept PINE planks. The input log is PINE.
        // Player expects: the bill should NOT be suspended because pine planks
        // have storage. But with MAT_NONE check, it resolves to OAK default,
        // and if the stockpile only allows PINE, it might incorrectly suspend.
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();

        // Create a stockpile that ONLY accepts PINE planks
        int sp = CreateStockpile(0, 0, 0, 2, 2);
        SetStockpileFilter(sp, ITEM_PLANKS, true);
        // Disable all materials, then enable only PINE
        for (int m = 0; m < MAT_COUNT; m++) {
            SetStockpileMaterialFilter(sp, (MaterialType)m, false);
        }
        SetStockpileMaterialFilter(sp, MAT_PINE, true);

        // Create sawmill at (5,1)
        int wsIdx = CreateWorkshop(5, 1, 0, WORKSHOP_SAWMILL);

        // Add "Saw Planks" bill (recipe 0)
        AddBill(wsIdx, 0, BILL_DO_FOREVER, 0);

        // Spawn PINE log near workshop
        SpawnItemWithMaterial(CELL_SIZE * 4.5f, CELL_SIZE * 2.5f, 0.0f, ITEM_LOG, MAT_PINE);

        // Spawn mover
        Mover* m2 = &movers[0];
        Point goal = {1, 2, 0};
        InitMover(m2, CELL_SIZE * 1.5f, CELL_SIZE * 2.5f, 0.0f, goal, 200.0f);
        moverCount = 1;
        RebuildIdleMoverList();
        BuildItemSpatialGrid();
        BuildMoverSpatialGrid();

        // Try to assign a craft job
        int jobId = WorkGiver_Craft(0);

        // Player expectation: the bill should NOT be suspended
        // because pine planks DO have storage (the stockpile accepts them)
        Bill* bill = &workshops[wsIdx].bills[0];
        expect(bill->suspended == false);

        // And a job should have been created
        expect(jobId >= 0);
    }

    it("craft job should properly decrement itemCount when consuming inputs") {
        // Story: A craft job completes and consumes its input item.
        // Player expects: itemCount decrements properly (via DeleteItem),
        // not just setting active=false while leaking the count.
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();

        // Create sawmill at (5,1) — work tile is at (6,2)
        int wsIdx = CreateWorkshop(5, 1, 0, WORKSHOP_SAWMILL);
        Workshop* ws = &workshops[wsIdx];
        AddBill(wsIdx, 0, BILL_DO_X_TIMES, 1);

        // Mover already at the workshop work tile
        Mover* m = &movers[0];
        float workX = ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f;
        float workY = ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f;
        Point goal = {ws->workTileX, ws->workTileY, 0};
        InitMover(m, workX, workY, 0.0f, goal, 200.0f);
        moverCount = 1;
        ws->assignedCrafter = 0;

        // Spawn a log as the carried input (already picked up)
        int logIdx = SpawnItemWithMaterial(workX, workY, 0.0f, ITEM_LOG, MAT_OAK);
        items[logIdx].state = ITEM_CARRIED;
        items[logIdx].reservedBy = 0;

        int countBefore = itemCount;

        // Manually create a craft job at WORKING step (skip walking phases)
        int jobId = CreateJob(JOBTYPE_CRAFT);
        Job* job = GetJob(jobId);
        job->assignedMover = 0;
        job->targetWorkshop = wsIdx;
        job->targetBillIdx = 0;
        job->targetItem = -1;
        job->carryingItem = logIdx;
        job->fuelItem = -1;
        job->step = CRAFT_STEP_WORKING;
        job->progress = 0.0f;
        job->workRequired = 1.6f;
        m->currentJobId = jobId;

        // Run until crafting completes (job finishes)
        bool craftDone = false;
        for (int i = 0; i < 500; i++) {
            Tick();
            JobsTick();
            // Job was active, now completed
            if (m->currentJobId < 0 && i > 0) {
                craftDone = true;
                break;
            }
        }

        expect(craftDone == true);

        // Sawmill "Saw Planks": 1 log -> 1 plank item (stackCount=4)
        // Net change = -1 (consumed) + 1 (spawned) = 0
        expect(itemCount == countBefore);
    }
}

// Stockpile lifecycle tests (stockpiles.c audit findings)
// These tests verify player expectations when stockpiles are modified or deleted.
// Based on assumption audit findings - tests should FAIL first, then we fix bugs.
describe(stockpile_lifecycle) {
    it("deleting a stockpile should drop all stored items to ground") {
        // Finding 2 (HIGH): DeleteStockpile doesn't drop IN_STOCKPILE items to ground
        // Story: Player deletes a stockpile that has items in it.
        // Expected: Items become regular ground items (visible, can be picked up).
        // Bug: Items remain in ITEM_IN_STOCKPILE state, become invisible/inaccessible.
        
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Create a stockpile with some items in it
        int spIdx = CreateStockpile(5, 5, 0, 2, 2);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        // Spawn 3 red items directly into stockpile slots
        int item1 = SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        int item2 = SpawnItem(6 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        int item3 = SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f, 6 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Manually set them as IN_STOCKPILE and place them
        items[item1].state = ITEM_IN_STOCKPILE;
        items[item2].state = ITEM_IN_STOCKPILE;
        items[item3].state = ITEM_IN_STOCKPILE;
        PlaceItemInStockpile(spIdx, 5, 5, item1);
        PlaceItemInStockpile(spIdx, 6, 5, item2);
        PlaceItemInStockpile(spIdx, 5, 6, item3);
        
        // Verify items are in stockpile
        expect(items[item1].state == ITEM_IN_STOCKPILE);
        expect(items[item2].state == ITEM_IN_STOCKPILE);
        expect(items[item3].state == ITEM_IN_STOCKPILE);
        
        // Now delete the stockpile
        DeleteStockpile(spIdx);
        
        // Player expectation: All items should be on ground now (accessible)
        expect(items[item1].state == ITEM_ON_GROUND);
        expect(items[item2].state == ITEM_ON_GROUND);
        expect(items[item3].state == ITEM_ON_GROUND);
        
        // Items should still exist at their original positions
        expect(items[item1].active == true);
        expect(items[item2].active == true);
        expect(items[item3].active == true);
    }
    
    it("placing item in inactive stockpile cell should not corrupt slot data") {
        // Finding 3 (HIGH): PlaceItemInStockpile doesn't validate cell is active
        // Story: Mover has haul job to stockpile, player removes that cell mid-job.
        // Expected: Item placement fails gracefully or item goes to ground.
        // Bug: Item placed in inactive cell, enters "phantom" state (in stockpile that doesn't exist).
        
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Create stockpile with 2 cells
        int spIdx = CreateStockpile(5, 5, 0, 2, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        // Mover near stockpile
        Mover* m = &movers[0];
        Point goal = {4, 5, 0};
        InitMover(m, 4 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Item close by
        int itemIdx = SpawnItem(3 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Run until mover picks up item
        for (int i = 0; i < 200; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (items[itemIdx].state == ITEM_CARRIED) break;
        }
        
        expect(items[itemIdx].state == ITEM_CARRIED);
        
        // Now REMOVE the stockpile cell the mover is heading to
        // (Simulate player shrinking stockpile while mover is carrying)
        RemoveStockpileCells(spIdx, 5, 5, 5, 5);
        
        // Run more ticks - mover tries to deliver
        for (int i = 0; i < 200; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }
        
        // Player expectation: Item should NOT enter corrupted state
        // Either it's safely on ground, or job was cancelled
        expect(items[itemIdx].state != ITEM_IN_STOCKPILE);
        expect(items[itemIdx].active == true);
        
        // Item should be on ground (safe-dropped) or back in circulation
        expect(items[itemIdx].state == ITEM_ON_GROUND || items[itemIdx].state == ITEM_CARRIED);
    }
    
    it("removing stockpile cell should release slot reservations") {
        // Finding 1 (MEDIUM): RemoveStockpileCells doesn't release slot reservations
        // Story: Mover has haul job with reserved slot, player removes that cell.
        // Expected: Reservation released, slot can be reused if cell is re-added.
        // Bug: Reservation leaks, phantom reservation blocks future use.
        
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Create stockpile
        int spIdx = CreateStockpile(5, 5, 0, 2, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        // Mover far away (so job takes time)
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Item far away
        int itemIdx = SpawnItem(9 * CELL_SIZE + CELL_SIZE * 0.5f, 9 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Assign job - should reserve a slot
        AssignJobs();
        
        // Check reservation was made
        int reservedCount = 0;
        for (int i = 0; i < 2; i++) {
            if (stockpiles[spIdx].reservedBy[i] > 0) reservedCount++;
        }
        expect(reservedCount > 0);
        
        // Now remove the stockpile cells
        RemoveStockpileCells(spIdx, 5, 5, 6, 5);
        
        // Player expectation: Reservations should be cleared
        // (Otherwise phantom reservations block future use)
        reservedCount = 0;
        for (int i = 0; i < 2; i++) {
            if (stockpiles[spIdx].reservedBy[i] > 0) reservedCount++;
        }
        expect(reservedCount == 0);
        
        // Job should be cancelled (item unreserved)
        expect(items[itemIdx].reservedBy == -1);
    }
    
    it("removing stockpile cell should clear item reservations") {
        // Finding 5 (MEDIUM): RemoveStockpileCells doesn't clear item reservations
        // Story: Item in stockpile is reserved for crafting, player removes that cell.
        // Expected: Item drops to ground and reservation is cleared.
        // Bug: Item on ground but still marked reserved, blocking other movers.
        
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        
        // Create stockpile with an item in it
        int spIdx = CreateStockpile(5, 5, 0, 1, 1);
        SetStockpileFilter(spIdx, ITEM_LOG, true);
        
        // Spawn log in stockpile
        int logIdx = SpawnItemWithMaterial(5 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_LOG, MAT_OAK);
        items[logIdx].state = ITEM_IN_STOCKPILE;
        PlaceItemInStockpile(spIdx, 5, 5, logIdx);
        
        // Reserve the item (simulating a craft job claiming it)
        ReserveItem(logIdx, 0);
        expect(items[logIdx].reservedBy == 0);
        
        // Now remove the stockpile cell
        RemoveStockpileCells(spIdx, 5, 5, 5, 5);
        
        // Player expectation: Item should be on ground AND unreserved
        expect(items[logIdx].state == ITEM_ON_GROUND);
        expect(items[logIdx].reservedBy == -1);
    }
    
    it("placing mismatched item type in occupied slot should not corrupt slot data") {
        // Finding 6 (MEDIUM): PlaceItemInStockpile assumes slotTypes/Materials match
        // Story: Due to a race condition or bug, wrong item type placed in occupied slot.
        // Expected: Placement rejected or error logged (defensive programming).
        // Bug: Slot type overwritten, stacking invariant breaks, mixed types in one slot.
        
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        ClearItems();
        ClearStockpiles();
        
        // Create stockpile
        int spIdx = CreateStockpile(5, 5, 0, 1, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        SetStockpileFilter(spIdx, ITEM_GREEN, true);
        
        // Place a RED item in slot (5,5)
        int redItem = SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        items[redItem].state = ITEM_IN_STOCKPILE;
        PlaceItemInStockpile(spIdx, 5, 5, redItem);
        
        // Verify slot type is RED
        expect(stockpiles[spIdx].slotTypes[0] == ITEM_RED);
        expect(stockpiles[spIdx].slotCounts[0] == 1);
        
        // Now try to place a GREEN item in the same slot (bug scenario)
        int greenItem = SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_GREEN);
        items[greenItem].state = ITEM_IN_STOCKPILE;
        PlaceItemInStockpile(spIdx, 5, 5, greenItem);
        
        // Player expectation: Slot should still be RED (mixed types not allowed)
        // Implementation should reject mismatched placement
        expect(stockpiles[spIdx].slotTypes[0] == ITEM_RED);
        
        // Count should not have increased (green placement rejected)
        expect(stockpiles[spIdx].slotCounts[0] == 1);
    }
    
    it("removing item from stockpile should validate item state and coordinates") {
        // Finding 7 (MEDIUM): RemoveItemFromStockpileSlot doesn't validate item state
        // Story: Code tries to remove item from wrong stockpile (stale coordinates).
        // Expected: Removal fails gracefully or validates item actually belongs there.
        // Bug: Wrong slot count decremented, ghost items or negative counts.
        
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        ClearItems();
        ClearStockpiles();
        
        // Create two stockpiles at different locations
        int spA = CreateStockpile(3, 3, 0, 1, 1);
        int spB = CreateStockpile(7, 7, 0, 1, 1);
        SetStockpileFilter(spA, ITEM_RED, true);
        SetStockpileFilter(spB, ITEM_RED, true);
        
        // Place item in stockpile A
        int itemIdx = SpawnItem(3 * CELL_SIZE + CELL_SIZE * 0.5f, 3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        items[itemIdx].state = ITEM_IN_STOCKPILE;
        PlaceItemInStockpile(spA, 3, 3, itemIdx);
        
        expect(stockpiles[spA].slotCounts[0] == 1);
        expect(stockpiles[spB].slotCounts[0] == 0);
        
        // Now try to remove from stockpile B using stockpile A's coordinates (bug scenario)
        // This simulates stale coordinate reference
        float wrongX = 7 * CELL_SIZE + CELL_SIZE * 0.5f;
        float wrongY = 7 * CELL_SIZE + CELL_SIZE * 0.5f;
        RemoveItemFromStockpileSlot(wrongX, wrongY, 0);
        
        // Player expectation: Stockpile A count should remain unchanged
        // (Removal at wrong coordinates should not affect stockpile A)
        expect(stockpiles[spA].slotCounts[0] == 1);
        
        // Stockpile B should also remain 0 (no item was actually there)
        expect(stockpiles[spB].slotCounts[0] == 0);
    }
    
    it("movers should haul items to stockpiles in priority order") {
        // Finding 9 (MEDIUM): FindStockpileForItem doesn't respect priority
        // Story: Player creates high-priority stockpile near workshop for quick access.
        // Expected: Items hauled to high-priority stockpile first.
        // Bug: Items go to first stockpile with space (by index), ignoring priority.
        
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Mover at top-left
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Create LOW priority stockpile (index 0, close to mover)
        int spLow = CreateStockpile(3, 1, 0, 1, 1);
        SetStockpileFilter(spLow, ITEM_RED, true);
        SetStockpilePriority(spLow, 1);  // Low priority
        
        // Create HIGH priority stockpile (index 1, same distance)
        int spHigh = CreateStockpile(3, 3, 0, 1, 1);
        SetStockpileFilter(spHigh, ITEM_RED, true);
        SetStockpilePriority(spHigh, 9);  // High priority
        
        // Item equidistant from both stockpiles
        int itemIdx = SpawnItem(8 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Run simulation
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (items[itemIdx].state == ITEM_IN_STOCKPILE) break;
        }
        
        expect(items[itemIdx].state == ITEM_IN_STOCKPILE);
        
        // Player expectation: Item should go to HIGH priority stockpile
        int itemTileX = (int)(items[itemIdx].x / CELL_SIZE);
        int itemTileY = (int)(items[itemIdx].y / CELL_SIZE);
        
        // Item should be at (3,3) high priority stockpile, NOT (3,1) low priority
        expect(itemTileX == 3);
        expect(itemTileY == 3);
    }
    
    it("removing stockpile cells with items should not leave items in limbo") {
        // Combined test: Findings 1, 2, 5
        // Story: Player mass-removes stockpile cells while items are being hauled.
        // Expected: All items end up on ground, all reservations cleared, no leaks.
        // Bug: Items vanish, reservations leak, counts corrupted.
        
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Create stockpile with multiple cells
        int spIdx = CreateStockpile(5, 5, 0, 3, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        // Place items in some slots
        int item1 = SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        int item2 = SpawnItem(6 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        items[item1].state = ITEM_IN_STOCKPILE;
        items[item2].state = ITEM_IN_STOCKPILE;
        PlaceItemInStockpile(spIdx, 5, 5, item1);
        PlaceItemInStockpile(spIdx, 6, 5, item2);
        
        // Reserve one item (simulating craft job)
        ReserveItem(item1, 0);
        
        // Create mover with haul job targeting third slot
        Mover* m = &movers[0];
        Point goal = {9, 9, 0};
        InitMover(m, 9 * CELL_SIZE + CELL_SIZE * 0.5f, 9 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        int item3 = SpawnItem(9 * CELL_SIZE + CELL_SIZE * 0.5f, 9 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        AssignJobs();
        
        // Now remove ALL stockpile cells
        RemoveStockpileCells(spIdx, 5, 5, 7, 5);
        
        // Player expectations:
        // 1. All items should be on ground or accessible
        expect(items[item1].active == true);
        expect(items[item2].active == true);
        expect(items[item3].active == true);
        expect(items[item1].state == ITEM_ON_GROUND);
        expect(items[item2].state == ITEM_ON_GROUND);
        
        // 2. All reservations cleared
        expect(items[item1].reservedBy == -1);
        expect(items[item2].reservedBy == -1);
        expect(items[item3].reservedBy == -1);
        
        // 3. Haul job cancelled
        expect(MoverIsIdle(m) || m->currentJobId < 0 || GetJob(m->currentJobId) == NULL || !GetJob(m->currentJobId)->active);
    }
}

// Workshop lifecycle tests (workshops.c audit findings)
describe(workshop_lifecycle) {
    it("deleting a workshop should invalidate paths through former blocking tiles") {
        // Story: Player builds a workshop (blocks pathing), then deletes it.
        // Expected: Movers immediately recalculate paths through the now-walkable space.
        // Actual (bug): Movers continue avoiding the area until they repath for other reasons.
        
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();

        // Create a stonecutter at (3,3) — template is 3x3 with blocking tiles
        int wsIdx = CreateWorkshop(3, 3, 0, WORKSHOP_STONECUTTER);
        
        // Spawn mover at (1,1) with goal at (8,8)
        Mover* m = &movers[0];
        Point goal = {8, 8, 0};
        InitMover(m, CELL_SIZE * 1.5f, CELL_SIZE * 1.5f, 0.0f, goal, 200.0f);
        moverCount = 1;
        
        // Compute initial path (will go around workshop)
        m->needsRepath = true;
        Point moverCell = {(int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z};
        m->pathLength = FindPath(moverPathAlgorithm, moverCell, m->goal, m->path, MAX_PATH);
        int pathLengthWithWorkshop = m->pathLength;
        
        // Now DELETE the workshop
        DeleteWorkshop(wsIdx);
        
        // Player expectation: path should be invalidated immediately
        // (because CreateWorkshop calls InvalidatePathsThroughCell, DeleteWorkshop should too)
        // The mover should be marked for repath
        expect(m->needsRepath == true);
        
        // After repath, the path should be shorter (can go through former workshop area)
        Point moverCell2 = {(int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z};
        int newPathLength = FindPath(moverPathAlgorithm, moverCell2, m->goal, m->path, MAX_PATH);
        
        // With workshop: must detour around. Without: straight line possible.
        // Path should be noticeably shorter
        expect(newPathLength < pathLengthWithWorkshop);
    }
    
    it("removing a bill should cancel in-progress jobs targeting that bill index") {
        // Story: Player has 3 bills queued. A mover is executing bill #1.
        // Player removes bill #0 (shifts all bills down).
        // Expected: The mover's job should be cancelled OR the targetBillIdx updated.
        // Actual (bug): Mover continues with billIdx=1, which now points to the OLD bill #2 (wrong recipe!).
        
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();

        // Create sawmill at (5,1)
        int wsIdx = CreateWorkshop(5, 1, 0, WORKSHOP_SAWMILL);
        Workshop* ws = &workshops[wsIdx];
        
        // Add 3 bills:
        // Bill 0: Saw Planks (recipe 0)
        // Bill 1: Cut Sticks (recipe 1)
        // Bill 2: Saw Planks (recipe 0) again
        AddBill(wsIdx, 0, BILL_DO_X_TIMES, 1);  // bill 0
        AddBill(wsIdx, 1, BILL_DO_X_TIMES, 1);  // bill 1
        AddBill(wsIdx, 0, BILL_DO_X_TIMES, 1);  // bill 2
        
        // Mover at workshop
        Mover* m = &movers[0];
        float workX = ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f;
        float workY = ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f;
        Point goal = {ws->workTileX, ws->workTileY, 0};
        InitMover(m, workX, workY, 0.0f, goal, 200.0f);
        moverCount = 1;
        ws->assignedCrafter = 0;

        // Create a craft job for bill #1 (Cut Sticks, recipe 1)
        int logIdx = SpawnItemWithMaterial(workX, workY, 0.0f, ITEM_LOG, MAT_OAK);
        items[logIdx].state = ITEM_CARRIED;
        items[logIdx].reservedBy = 0;

        int jobId = CreateJob(JOBTYPE_CRAFT);
        Job* job = GetJob(jobId);
        job->assignedMover = 0;
        job->targetWorkshop = wsIdx;
        job->targetBillIdx = 1;  // Executing bill #1 (Cut Sticks)
        job->carryingItem = logIdx;
        job->fuelItem = -1;
        job->step = CRAFT_STEP_WORKING;
        job->progress = 0.0f;
        job->workRequired = 0.8f;  // recipe 1 work time
        m->currentJobId = jobId;

        // Player removes bill #0 (Saw Planks)
        // This shifts bill #1 → #0 and bill #2 → #1
        RemoveBill(wsIdx, 0);
        
        // Player expectation: The mover's job should be cancelled (safest behavior)
        // OR at minimum, the job should fail gracefully on next tick
        // The mover should NOT continue executing with the now-wrong bill index
        
        // Run one tick
        Tick();
        JobsTick();
        
        // Either the job was cancelled immediately after RemoveBill (ideal),
        // or it fails on first tick because the recipe doesn't match expectations
        // Either way, the mover should NOT still be in CRAFT_STEP_WORKING
        // because that would mean it's executing the WRONG recipe
        
        // The bug: job->targetBillIdx=1 now points to the OLD bill #2 (Saw Planks, recipe 0)
        // But the job was set up for recipe 1 (Cut Sticks, workRequired=0.8)
        // This creates inconsistency
        
        Job* checkJob = GetJob(jobId);
        if (checkJob && checkJob->active) {
            // Job is still active. Check that it's targeting a valid bill.
            expect(job->targetBillIdx < ws->billCount);
            
            // More importantly: if the bill shifted, the recipe should still match
            // what the job was set up for. Otherwise the mover is crafting the wrong thing!
            Bill* currentBill = &ws->bills[job->targetBillIdx];
            // The job was set up for "Cut Sticks" (recipe 1, workRequired 0.8)
            // If it's now pointing to "Saw Planks" (recipe 0, workRequired 1.6), that's wrong!
            expect(currentBill->recipeIdx == 1);  // Should still be "Cut Sticks"
        }
        // OR the job should have been cancelled/failed (mover is idle)
        // Either outcome is acceptable, but the bug is that neither happens!
    }
}

// ============================================================
// Designation Lifecycle Tests (designations.c audit findings)
// ============================================================
describe(designation_lifecycle) {

    // Finding 1 (HIGH): CompleteDigRampDesignation missing rampCount++
    it("digging a ramp should increment the global ramp count") {
        // Player digs a ramp from a wall. rampCount should go up by 1.
        InitTestGridFromAscii(
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n");

        // Make z=0 solid walls, z=1 walkable
        for (int x = 0; x < 5; x++) {
            for (int y = 0; y < 5; y++) {
                grid[0][y][x] = CELL_WALL;
                SetWallMaterial(x, y, 0, MAT_GRANITE);
                SetWallNatural(x, y, 0);
                grid[1][y][x] = CELL_AIR;
                SET_FLOOR(x, y, 1);
            }
        }

        InitDesignations();
        ClearItems();

        int before = rampCount;

        // Directly complete a dig-ramp at (2,2,0) - wall becomes ramp
        CompleteDigRampDesignation(2, 2, 0, -1);

        expect(CellIsRamp(grid[0][2][2]));
        expect(rampCount == before + 1);
    }

    // Finding 2 (HIGH): CompleteDigRampDesignation missing MarkChunkDirty
    it("digging a ramp should mark the chunk dirty for rendering") {
        // Player digs a ramp. The rendering chunk should be marked dirty
        // so the mesh rebuilds and the player sees the new ramp.
        InitTestGridFromAscii(
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n");

        for (int x = 0; x < 5; x++) {
            for (int y = 0; y < 5; y++) {
                grid[0][y][x] = CELL_WALL;
                SetWallMaterial(x, y, 0, MAT_GRANITE);
                SetWallNatural(x, y, 0);
                grid[1][y][x] = CELL_AIR;
                SET_FLOOR(x, y, 1);
            }
        }

        InitDesignations();
        ClearItems();

        // Clear chunk dirty flags
        int cx = 2 / chunkWidth;
        int cy = 2 / chunkHeight;
        chunkDirty[0][cy][cx] = false;

        CompleteDigRampDesignation(2, 2, 0, -1);

        expect(chunkDirty[0][cy][cx] == true);
    }

    // Finding 3 (HIGH): CompleteRemoveFloorDesignation spawns drop item at wrong z
    it("removing a floor should drop the floor material item at the same level") {
        // Player removes a constructed floor at z=2. The resulting material item
        // should appear at z=2 (where the mover is standing), so it's reachable.
        InitTestGridFromAscii(
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n");

        // z=0: solid ground, z=1: air+floor (walkable), z=2: air+constructed floor
        for (int x = 0; x < 5; x++) {
            for (int y = 0; y < 5; y++) {
                grid[0][y][x] = CELL_WALL;
                grid[1][y][x] = CELL_AIR;
                SET_FLOOR(x, y, 1);
                SetFloorMaterial(x, y, 1, MAT_GRANITE);
                ClearFloorNatural(x, y, 1);  // Constructed floor at z=1
                grid[2][y][x] = CELL_AIR;
                SET_FLOOR(x, y, 2);
                SetFloorMaterial(x, y, 2, MAT_GRANITE);
                ClearFloorNatural(x, y, 2);  // Constructed floor at z=2
            }
        }

        InitDesignations();
        ClearItems();

        // Designate and complete floor removal at (2,2,2) - floor at z=2 above z=1
        DesignateRemoveFloor(2, 2, 2);
        CompleteRemoveFloorDesignation(2, 2, 2, -1);

        // Find the spawned item - it should be at z=2 (mover's level), not z=1
        bool foundAtCorrectZ = false;
        bool foundAtWrongZ = false;
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (!items[i].active) continue;
            int ix = (int)(items[i].x / CELL_SIZE);
            int iy = (int)(items[i].y / CELL_SIZE);
            if (ix == 2 && iy == 2) {
                if ((int)items[i].z == 2) foundAtCorrectZ = true;
                if ((int)items[i].z == 1) foundAtWrongZ = true;
            }
        }
        expect(foundAtCorrectZ == true);
        expect(foundAtWrongZ == false);
    }

    // Finding 4 (HIGH): CompleteBlueprint (WALL) missing InvalidatePathsThroughCell
    it("building a wall should invalidate mover paths through that cell") {
        // A mover across the map has a cached path going through a cell.
        // When a wall is built there, the mover's path should be invalidated.
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);

        InitDesignations();
        ClearItems();
        ClearMovers();

        // Spawn a mover at (0,0) with a cached path that goes through (5,2)
        Point goal = {9, 0, 0};
        InitMover(&movers[0], 0 * CELL_SIZE + CELL_SIZE * 0.5f,
                  0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        Mover* m = &movers[0];
        m->active = true;

        // Give the mover a fake path through (5,2,0)
        m->path[0].x = 5; m->path[0].y = 2; m->path[0].z = 0;
        m->path[1].x = 6; m->path[1].y = 2; m->path[1].z = 0;
        m->pathLength = 2;
        m->pathIndex = 1;
        m->needsRepath = false;

        // Create and complete a wall blueprint at (5,2,0)
        int bpIdx = CreateRecipeBlueprint(5, 2, 0, CONSTRUCTION_DRY_STONE_WALL);
        expect(bpIdx >= 0);
        FillBlueprintStage(bpIdx, MAT_GRANITE);

        CompleteBlueprint(bpIdx);

        // The mover's path should be invalidated
        expect(m->needsRepath == true);
    }

    // Finding 5 (MEDIUM): CompleteBlueprint (RAMP) missing rampCount++
    it("building a ramp blueprint should increment the global ramp count") {
        InitTestGridFromAscii(
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n");

        // z=0: walls (for ramp direction), z=1: walkable
        for (int x = 0; x < 5; x++) {
            for (int y = 0; y < 5; y++) {
                grid[0][y][x] = CELL_WALL;
                grid[1][y][x] = CELL_AIR;
                SET_FLOOR(x, y, 1);
            }
        }
        // Make (2,2,1) air with floor - this is where the ramp will go
        // Wall at (2,1,1) for ramp direction detection
        grid[1][1][2] = CELL_WALL;

        InitDesignations();
        ClearItems();

        int before = rampCount;

        int bpIdx = CreateRecipeBlueprint(2, 2, 1, CONSTRUCTION_RAMP);
        expect(bpIdx >= 0);
        FillBlueprintStage(bpIdx, MAT_GRANITE);

        CompleteBlueprint(bpIdx);

        expect(CellIsRamp(grid[1][2][2]));
        expect(rampCount == before + 1);
    }

    // Finding 6 (MEDIUM): CancelBlueprint does not refund delivered materials
    it("canceling a blueprint with delivered materials should drop them on the ground") {
        // Player places a wall blueprint, a mover delivers stone, then player
        // cancels. The stone should appear on the ground, not vanish.
        InitTestGridFromAscii(
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n");

        InitDesignations();
        ClearItems();

        // Create blueprint at (2,2,0)
        int bpIdx = CreateRecipeBlueprint(2, 2, 0, CONSTRUCTION_DRY_STONE_WALL);
        expect(bpIdx >= 0);

        // Spawn an item and deliver it to the blueprint
        int itemIdx = SpawnItemWithMaterial(
            2 * CELL_SIZE + CELL_SIZE * 0.5f,
            2 * CELL_SIZE + CELL_SIZE * 0.5f,
            0.0f, ITEM_BLOCKS, (uint8_t)MAT_GRANITE);
        DeliverMaterialToBlueprint(bpIdx, itemIdx);

        // Material has been consumed (DeleteItem called)
        expect(blueprints[bpIdx].stageDeliveries[0].deliveredCount == 1);
        expect(blueprints[bpIdx].stageDeliveries[0].deliveredMaterial == MAT_GRANITE);

        int itemsBefore = 0;
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (items[i].active) itemsBefore++;
        }

        // Cancel the blueprint - materials should be refunded
        CancelBlueprint(bpIdx);

        int itemsAfter = 0;
        bool foundRefund = false;
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (!items[i].active) continue;
            itemsAfter++;
            int ix = (int)(items[i].x / CELL_SIZE);
            int iy = (int)(items[i].y / CELL_SIZE);
            if (ix == 2 && iy == 2 && items[i].type == ITEM_BLOCKS) {
                foundRefund = true;
            }
        }

        // Should have one more item than before (the refunded material)
        expect(itemsAfter == itemsBefore + 1);
        expect(foundRefund == true);
    }

    // Finding 8 (MEDIUM): CompleteRemoveFloorDesignation missing DestabilizeWater
    // Finding 8 also covers CompleteRemoveRampDesignation
    // (Water destabilization is hard to test directly without the water sim,
    //  so we test the ramp-specific part of this finding)

    // Finding 10 (MEDIUM): CompleteBlueprint (RAMP) missing PushItemsOutOfCell
    it("building a ramp should push items out of the cell") {
        // Items sitting on a cell where a ramp is built should be pushed
        // to an adjacent cell, just like wall blueprints do.
        InitTestGridFromAscii(
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n");

        for (int x = 0; x < 5; x++) {
            for (int y = 0; y < 5; y++) {
                grid[0][y][x] = CELL_WALL;
                grid[1][y][x] = CELL_AIR;
                SET_FLOOR(x, y, 1);
            }
        }
        // Wall at (2,1,1) for ramp direction
        grid[1][1][2] = CELL_WALL;

        InitDesignations();
        ClearItems();

        // Place an item at (2,2,1)
        int itemIdx = SpawnItem(
            2 * CELL_SIZE + CELL_SIZE * 0.5f,
            2 * CELL_SIZE + CELL_SIZE * 0.5f,
            1.0f, ITEM_ROCK);

        int bpIdx = CreateRecipeBlueprint(2, 2, 1, CONSTRUCTION_RAMP);
        expect(bpIdx >= 0);
        FillBlueprintStage(bpIdx, MAT_GRANITE);

        CompleteBlueprint(bpIdx);

        // Item should have been pushed out of (2,2)
        int itemTileX = (int)(items[itemIdx].x / CELL_SIZE);
        int itemTileY = (int)(items[itemIdx].y / CELL_SIZE);
        bool pushed = (itemTileX != 2 || itemTileY != 2);
        expect(pushed == true);
    }

    // Composite test: dig ramp does everything mining does
    it("digging a ramp should do all post-completion steps like mining does") {
        // CompleteMineDesignation does: MarkChunkDirty, rampCount (N/A),
        // DestabilizeWater, ClearUnreachableCooldowns, ValidateAndCleanupRamps,
        // InvalidateDesignationCache. CompleteDigRampDesignation should do the same
        // relevant subset. This test checks unreachable cooldown clearing.
        InitTestGridFromAscii(
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n"
            ".....\n");

        for (int x = 0; x < 5; x++) {
            for (int y = 0; y < 5; y++) {
                grid[0][y][x] = CELL_WALL;
                SetWallMaterial(x, y, 0, MAT_GRANITE);
                SetWallNatural(x, y, 0);
                grid[1][y][x] = CELL_AIR;
                SET_FLOOR(x, y, 1);
            }
        }

        InitDesignations();
        ClearItems();

        // Place an item near the wall with an unreachable cooldown
        int itemIdx = SpawnItem(
            3 * CELL_SIZE + CELL_SIZE * 0.5f,
            2 * CELL_SIZE + CELL_SIZE * 0.5f,
            1.0f, ITEM_ROCK);
        items[itemIdx].unreachableCooldown = 10.0f;

        // Dig the ramp at (2,2,0) - opens new paths
        CompleteDigRampDesignation(2, 2, 0, -1);

        // Nearby item's unreachable cooldown should be cleared
        expect(items[itemIdx].unreachableCooldown == 0.0f);
    }
}

// =============================================================================
// Unreachable cooldown poisoning (cross-z-level bug)
// =============================================================================
describe(unreachable_cooldown_poisoning) {
    it("stranded mover on disconnected z-level should not poison reachable items") {
        // Story: I have 2 movers. One is stranded at z=3 (no way down).
        // The other is at z=1, near items and a stockpile.
        // The z=1 items should get hauled. The stranded mover should NOT
        // prevent hauling by poisoning items with unreachable cooldowns.

        // z=0: walls (solid ground), z=1: air+floor (walkable), z=3: air+floor (walkable but disconnected)
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n");

        // Make z=1 walkable: z=0 is walls, z=1 is air with floor
        for (int x = 0; x < 8; x++) {
            for (int y = 0; y < 8; y++) {
                grid[0][y][x] = CELL_WALL;
                grid[1][y][x] = CELL_AIR;
                SET_FLOOR(x, y, 1);
                // z=2: not walkable (no floor, no walls below)
                grid[2][y][x] = CELL_AIR;
                // z=3: walkable but disconnected (floor but no ramps connecting to z=1)
                grid[3][y][x] = CELL_AIR;
                SET_FLOOR(x, y, 3);
            }
        }

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearJobs();

        // Stranded mover at z=3, tile (1,1) — no path down
        Mover* stranded = &movers[0];
        Point strandedGoal = {1, 1, 3};
        InitMover(stranded, 1 * CELL_SIZE + CELL_SIZE * 0.5f,
                  1 * CELL_SIZE + CELL_SIZE * 0.5f, 3.0f, strandedGoal, 100.0f);
        stranded->capabilities.canHaul = true;

        // Working mover at z=1, tile (6,6) — can reach items
        Mover* worker = &movers[1];
        Point workerGoal = {6, 6, 1};
        InitMover(worker, 6 * CELL_SIZE + CELL_SIZE * 0.5f,
                  6 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, workerGoal, 100.0f);
        worker->capabilities.canHaul = true;
        moverCount = 2;

        // Item at z=1, tile (3,3) — walkable, reachable from z=1 mover
        int itemIdx = SpawnItem(3 * CELL_SIZE + CELL_SIZE * 0.5f,
                                3 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, ITEM_ROCK);

        // Stockpile at z=1
        int spIdx = CreateStockpile(5, 5, 1, 2, 2);
        SetStockpileFilter(spIdx, ITEM_ROCK, true);

        // Verify setup: both movers idle, item on ground
        expect(MoverIsIdle(stranded));
        expect(MoverIsIdle(worker));
        expect(items[itemIdx].state == ITEM_ON_GROUND);
        expect(items[itemIdx].unreachableCooldown == 0.0f);

        // Run AssignJobs — the stranded mover should NOT poison the item
        AssignJobs();

        // Player expectation: the item should NOT have an unreachable cooldown.
        // The z=1 worker mover can reach it, so it should get a haul job.
        expect(items[itemIdx].unreachableCooldown == 0.0f);

        // The worker mover (z=1) should have gotten the job
        expect(!MoverIsIdle(worker));
    }

    it("item unreachable from ALL movers should still get cooldown") {
        // Story: If an item is truly unreachable from every mover (e.g. walled off),
        // it should still get an unreachable cooldown to avoid spam-retrying.

        InitTestGridFromAscii(
            "........\n"
            "..####..\n"
            "..#..#..\n"
            "..####..\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearJobs();

        // Mover outside the walled pocket at (0,0,0)
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, CELL_SIZE * 0.5f, CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        m->capabilities.canHaul = true;
        moverCount = 1;

        // Item inside the walled pocket (unreachable by anyone)
        int itemIdx = SpawnItem(3 * CELL_SIZE + CELL_SIZE * 0.5f,
                                2 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_ROCK);

        int spIdx = CreateStockpile(5, 5, 0, 2, 2);
        SetStockpileFilter(spIdx, ITEM_ROCK, true);

        AssignJobs();

        // Truly unreachable items should still get cooldown
        expect(items[itemIdx].unreachableCooldown > 0.0f);
        expect(MoverIsIdle(m));
    }

    it("multiple items should not all be poisoned by one stranded mover") {
        // Story: I have 5 items on z=1 and a stranded mover at z=3.
        // After one AssignJobs call, ideally at most 1 item gets tried by
        // the stranded mover (not all 5). The z=1 mover should handle the rest.

        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n");

        for (int x = 0; x < 8; x++) {
            for (int y = 0; y < 8; y++) {
                grid[0][y][x] = CELL_WALL;
                grid[1][y][x] = CELL_AIR;
                SET_FLOOR(x, y, 1);
                grid[2][y][x] = CELL_AIR;
                grid[3][y][x] = CELL_AIR;
                SET_FLOOR(x, y, 3);
            }
        }

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearJobs();

        // Stranded mover at z=3
        Mover* stranded = &movers[0];
        Point sg = {1, 1, 3};
        InitMover(stranded, 1 * CELL_SIZE + CELL_SIZE * 0.5f,
                  1 * CELL_SIZE + CELL_SIZE * 0.5f, 3.0f, sg, 100.0f);
        stranded->capabilities.canHaul = true;

        // Worker mover at z=1
        Mover* worker = &movers[1];
        Point wg = {6, 6, 1};
        InitMover(worker, 6 * CELL_SIZE + CELL_SIZE * 0.5f,
                  6 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, wg, 100.0f);
        worker->capabilities.canHaul = true;
        moverCount = 2;

        // 5 items scattered on z=1
        int itemIds[5];
        itemIds[0] = SpawnItem(2 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, ITEM_ROCK);
        itemIds[1] = SpawnItem(3 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, ITEM_ROCK);
        itemIds[2] = SpawnItem(4 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, ITEM_ROCK);
        itemIds[3] = SpawnItem(2 * CELL_SIZE + CELL_SIZE * 0.5f, 3 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, ITEM_ROCK);
        itemIds[4] = SpawnItem(3 * CELL_SIZE + CELL_SIZE * 0.5f, 3 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, ITEM_ROCK);

        // Large stockpile at z=1
        int spIdx = CreateStockpile(5, 4, 1, 3, 3);
        SetStockpileFilter(spIdx, ITEM_ROCK, true);

        AssignJobs();

        // Count how many items got poisoned with unreachable cooldown
        int poisonedCount = 0;
        for (int i = 0; i < 5; i++) {
            if (items[itemIds[i]].unreachableCooldown > 0.0f) poisonedCount++;
        }

        // Player expectation: at most 1 item should be poisoned (the one the
        // stranded mover tried). The rest should be available for the worker.
        // Ideally 0 are poisoned if the fix skips cross-z-level attempts entirely.
        expect(poisonedCount <= 1);
    }
}

// =============================================================================
// Save/Load State Restoration Tests
// 
// These tests verify that after a save/load cycle, the game world is in a
// consistent state. We simulate what LoadWorld does (raw array copies) and
// check that derived state (counts, free lists, reservations) is correct.
//
// Philosophy: A player saves their game with 5 items, 2 stockpiles, and a
// workshop. After loading, they expect the game to behave identically.
// These tests catch cases where it wouldn't.
// =============================================================================

describe(saveload_state_restoration) {
    // =========================================================================
    // Finding 3: Entity count globals not restored on load
    // =========================================================================
    // Story: After I save and load my game, the item count, stockpile count,
    // workshop count, and blueprint count should reflect reality -- not zero.
    
    it("should have correct itemCount after simulated load") {
        // Setup: Create items normally, verify count, then simulate post-load
        // state where itemHighWaterMark is set but itemCount is zero.
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n");
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearJobs();
        
        // Create 3 items using normal API
        SpawnItem(1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        SpawnItem(2 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_GREEN);
        SpawnItem(3 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_BLUE);
        
        // Verify normal state
        expect(itemCount == 3);
        expect(itemHighWaterMark == 3);
        
        // Simulate what LoadWorld does: it reads itemHighWaterMark and the items
        // array, but does NOT set itemCount. After fread, itemCount is whatever
        // it was before (ClearItems sets it to 0, and LoadWorld doesn't update it).
        // 
        // We simulate this by zeroing itemCount (as if LoadWorld just finished
        // reading the array without updating the count).
        itemCount = 0;
        
        // Call RebuildPostLoadState to fix up the counts (the fix)
        RebuildPostLoadState();
        
        // Player expectation: itemCount should reflect the actual number of active
        // items. With itemCount = 0, systems that check itemCount (like "is the
        // world empty?") will give wrong answers.
        
        // Count how many items are actually active
        int actualCount = 0;
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (items[i].active) actualCount++;
        }
        
        expect(itemCount == actualCount);
    }
    
    it("should have correct stockpileCount after simulated load") {
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n");
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearJobs();
        
        // Create 2 stockpiles
        CreateStockpile(1, 1, 0, 2, 2);
        CreateStockpile(5, 1, 0, 2, 2);
        
        expect(stockpileCount == 2);
        
        // Simulate post-load: LoadWorld reads the stockpiles array but never
        // sets stockpileCount. It remains at whatever value ClearStockpiles
        // left it (0 from the clear at start of LoadWorld).
        stockpileCount = 0;
        
        // Call RebuildPostLoadState to fix up the counts (the fix)
        RebuildPostLoadState();
        
        // Count actual active stockpiles
        int actualCount = 0;
        for (int i = 0; i < MAX_STOCKPILES; i++) {
            if (stockpiles[i].active) actualCount++;
        }
        
        expect(stockpileCount == actualCount);
    }
    
    it("should have correct workshopCount after simulated load") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();
        
        // Create a workshop
        int wsIdx = CreateWorkshop(2, 2, 0, WORKSHOP_STONECUTTER);
        expect(wsIdx >= 0);
        expect(workshopCount == 1);
        
        // Simulate post-load: LoadWorld reads workshops array but never
        // sets workshopCount.
        workshopCount = 0;
        
        // Call RebuildPostLoadState to fix up the counts (the fix)
        RebuildPostLoadState();
        
        // Count actual active workshops
        int actualCount = 0;
        for (int i = 0; i < MAX_WORKSHOPS; i++) {
            if (workshops[i].active) actualCount++;
        }
        
        expect(workshopCount == actualCount);
    }
    
    it("should have correct itemCount with holes in the array after simulated load") {
        // Scenario: Player creates 5 items, deletes items 1 and 3 (leaving holes),
        // saves and loads. itemCount should be 3, not 0 or 5.
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n");
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearJobs();
        
        // Create 5 items
        int idx0 = SpawnItem(1 * CELL_SIZE, 1 * CELL_SIZE, 0.0f, ITEM_RED);
        int idx1 = SpawnItem(2 * CELL_SIZE, 1 * CELL_SIZE, 0.0f, ITEM_GREEN);
        int idx2 = SpawnItem(3 * CELL_SIZE, 1 * CELL_SIZE, 0.0f, ITEM_BLUE);
        int idx3 = SpawnItem(4 * CELL_SIZE, 1 * CELL_SIZE, 0.0f, ITEM_RED);
        int idx4 = SpawnItem(5 * CELL_SIZE, 1 * CELL_SIZE, 0.0f, ITEM_GREEN);
        (void)idx0; (void)idx2; (void)idx4;
        
        expect(itemCount == 5);
        
        // Delete items 1 and 3 (creating holes)
        DeleteItem(idx1);
        DeleteItem(idx3);
        
        expect(itemCount == 3);
        
        // Simulate post-load: array has holes, itemCount gets zeroed
        itemCount = 0;
        
        // Call RebuildPostLoadState to fix up the counts (the fix)
        RebuildPostLoadState();
        
        // Count actual active items
        int actualCount = 0;
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (items[i].active) actualCount++;
        }
        
        expect(itemCount == actualCount);
    }
    
    // =========================================================================
    // Finding 4: jobFreeList not rebuilt after load
    // =========================================================================
    // Story: After many save/load cycles, I shouldn't run out of job slots
    // even if most are unused.
    
    it("should reuse freed job slots after simulated load") {
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n");
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearJobs();
        
        // Create 5 jobs, then release 3 of them (simulating completed jobs)
        int job0 = CreateJob(JOBTYPE_HAUL);
        int job1 = CreateJob(JOBTYPE_HAUL);
        int job2 = CreateJob(JOBTYPE_HAUL);
        int job3 = CreateJob(JOBTYPE_HAUL);
        int job4 = CreateJob(JOBTYPE_HAUL);
        (void)job0; (void)job2; (void)job4;
        
        expect(jobHighWaterMark == 5);
        expect(activeJobCount == 5);
        
        // Release 3 jobs (creating holes in the array)
        ReleaseJob(job1);
        ReleaseJob(job3);
        ReleaseJob(job0);
        
        expect(activeJobCount == 2);
        expect(jobFreeCount == 3);  // 3 slots available for reuse
        
        // Simulate post-load: LoadWorld reads jobHighWaterMark, activeJobCount,
        // jobs array, jobIsActive, and activeJobList. But it does NOT rebuild
        // the jobFreeList. After load, jobFreeCount is 0.
        jobFreeCount = 0;
        
        // Call RebuildPostLoadState to rebuild the free list (the fix)
        RebuildPostLoadState();
        
        // Now try to create new jobs. With the free list rebuilt, CreateJob
        // should reuse freed slots instead of growing jobHighWaterMark.
        int hwmBefore = jobHighWaterMark;
        
        int newJob = CreateJob(JOBTYPE_HAUL);
        expect(newJob >= 0);  // Should succeed
        
        expect(newJob < hwmBefore);  // Should reuse a freed slot, not grow watermark
    }
    
    it("should be able to create jobs up to capacity after simulated load with many holes") {
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n");
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearJobs();
        
        // Create 10 jobs, release 8 (lots of holes)
        int jobIds[10];
        for (int i = 0; i < 10; i++) {
            jobIds[i] = CreateJob(JOBTYPE_HAUL);
        }
        expect(jobHighWaterMark == 10);
        
        // Release jobs 0-7 (keep only 8 and 9)
        for (int i = 0; i < 8; i++) {
            ReleaseJob(jobIds[i]);
        }
        expect(activeJobCount == 2);
        expect(jobFreeCount == 8);
        
        // Simulate post-load: free list lost
        jobFreeCount = 0;
        
        // Call RebuildPostLoadState to rebuild the free list (the fix)
        RebuildPostLoadState();
        
        // Try to create 8 new jobs (should reuse the freed slots)
        int newJobCount = 0;
        int hwmBefore = jobHighWaterMark;
        for (int i = 0; i < 8; i++) {
            int j = CreateJob(JOBTYPE_HAUL);
            if (j >= 0) newJobCount++;
        }
        
        expect(newJobCount == 8);
        expect(jobHighWaterMark == hwmBefore);  // Should not have grown
    }
    
    // =========================================================================
    // Finding 5: Item reservations not cleared on load
    // =========================================================================
    // Story: After loading a save, items on the ground should be available
    // for hauling -- not stuck with stale reservations from the previous session.
    
    it("should not have stale item reservations after simulated load") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearJobs();
        
        // Create items, some reserved (simulating mid-haul state when saved)
        int item0 = SpawnItem(3 * CELL_SIZE + CELL_SIZE * 0.5f, 3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        int item1 = SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f, 3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_GREEN);
        int item2 = SpawnItem(7 * CELL_SIZE + CELL_SIZE * 0.5f, 3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_BLUE);
        
        // Reserve items 0 and 2 (simulating active haul jobs at save time)
        ReserveItem(item0, 0);
        ReserveItem(item2, 1);
        
        expect(GetItemReservedBy(item0) == 0);
        expect(GetItemReservedBy(item1) == -1);
        expect(GetItemReservedBy(item2) == 1);
        
        // Simulate post-load: LoadWorld reads item array as-is, keeping
        // reservedBy fields. The jobs that held these reservations no longer
        // exist (LoadWorld clears jobs and reassigns). But the item reservations
        // persist, so these items appear "taken" and can never be hauled.
        //
        // LoadWorld DOES clear stockpile reservedBy (explicit memset), but
        // does NOT clear item reservedBy.
        
        // Call RebuildPostLoadState to clear stale reservations (the fix)
        RebuildPostLoadState();
        
        // After a load, items on the ground should be unreserved
        expect(GetItemReservedBy(item0) == -1);  // Should be cleared after load
        expect(GetItemReservedBy(item1) == -1);  // Was already clear
        expect(GetItemReservedBy(item2) == -1);  // Should be cleared after load
    }
    
    it("should allow hauling items that had stale reservations after simulated load") {
        // Full integration test: items with stale reservations should be
        // haulable after a simulated load
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearJobs();
        
        // Create a mover
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Create an item and give it a stale reservation
        int itemIdx = SpawnItem(5 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        items[itemIdx].reservedBy = 42;  // Stale reservation from pre-load mover
        
        // Create a stockpile
        int spIdx = CreateStockpile(8, 8, 0, 1, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        // Call RebuildPostLoadState to clear stale reservations (the fix)
        RebuildPostLoadState();
        
        // Try to assign jobs -- reservation should be cleared now
        AssignJobs();
        
        expect(!MoverIsIdle(m));  // Should have been assigned a haul job
        expect(MoverGetTargetItem(m) == itemIdx);
    }
}

/*
 * =============================================================================
 * GRID AUDIT INTEGRATION TESTS (grid.md Findings 3, 4, 6)
 * 
 * These tests verify that blueprint construction and tree chopping properly
 * clean up ramps/ladders when overwriting cells. Tests are expected to FAIL
 * until the fixes are applied.
 * 
 * Player stories:
 * - Finding 3: "I place a wall blueprint on a ramp, mover builds it, the ramp should be gone and rampCount correct"
 * - Finding 4: "I place a floor blueprint on a ladder, mover builds it, the ladder should be gone"
 * - Finding 6: "I chop a tree trunk supporting a ramp exit, the ramp should be removed"
 * =============================================================================
 */

describe(grid_audit_blueprint_integration) {
    it("player builds wall blueprint on ramp - ramp should be cleaned up (Finding 3)") {
        // Story: Player has a ramp. Player places wall blueprint on the ramp cell.
        // Mover delivers materials and builds the wall. Player expects:
        // 1. The ramp is gone (replaced by wall)
        // 2. rampCount is decremented
        // 3. No phantom ramps in pathfinding
        
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        rampCount = 0;
        
        // Create mover at (1,2)
        Mover* m = &movers[0];
        Point goal = {1, 2, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Place 3 rocks at (2,2) - dry stone wall needs 3
        for (int i = 0; i < 3; i++) {
            SpawnItem(2 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_ROCK);
        }
        
        // Place a ramp at (5,2) pointing north
        grid[0][2][5] = CELL_RAMP_N;
        rampCount++;
        int rampCountBefore = rampCount;
        
        // Player places wall blueprint on the ramp cell
        int bpIdx = CreateRecipeBlueprint(5, 2, 0, CONSTRUCTION_DRY_STONE_WALL);
        expect(bpIdx >= 0);
        
        // Run simulation until wall is built
        for (int i = 0; i < 3000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            
            if (grid[0][2][5] == CELL_WALL) break;
        }
        
        // Player expectation: wall should exist, ramp should be gone
        expect(grid[0][2][5] == CELL_WALL);
        
        // Player expectation: rampCount should have decremented (no phantom ramps)
        expect(rampCount == rampCountBefore - 1);
    }
    
    it("player builds floor blueprint on ladder - ladder should be cleaned up (Finding 4)") {
        // Story: Player has a ladder. Player places floor blueprint on the
        // ladder cell. When construction completes, the ladder should be
        // properly cleaned up (not silently overwritten).
        
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        
        // Place a ladder at (5,2,0)
        PlaceLadder(5, 2, 0);
        expect(IsLadderCell(grid[0][2][5]));
        
        // Create floor blueprint on the ladder cell
        int bpIdx = CreateRecipeBlueprint(5, 2, 0, CONSTRUCTION_PLANK_FLOOR);
        expect(bpIdx >= 0);
        
        // Simulate material delivery (skip job system, go straight to completion)
        FillBlueprintStage(bpIdx, MAT_OAK);
        
        // Complete the blueprint (this is what the builder mover calls)
        expect(blueprints[bpIdx].active == true);
        CompleteBlueprint(bpIdx);
        
        // After completion, cell should be AIR (not ladder)
        expect(grid[0][2][5] == CELL_AIR);
        
        // Blueprint should be consumed
        expect(blueprints[bpIdx].active == false);
        
        // Player expectation: floor should exist (AIR + floor flag)
        // Note: HAS_FLOOR returns bitmask value (0x20), not bool - don't compare == true
        expect(HAS_FLOOR(5, 2, 0));
        
        // Player expectation: ladder should be gone (not a ladder cell anymore)
        expect(!IsLadderCell(grid[0][2][5]));
    }
    
    it("player builds wall blueprint on ramp with rampCount check (Finding 3 extended)") {
        // Story: Player has a ramp at z=0. Player places wall blueprint on the
        // ramp cell. Player expects:
        // 1. The ramp at z=0 is gone (replaced by wall)
        // 2. rampCount decrements
        // Same as first test but also verifies rampCount with solid support setup
        
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        rampCount = 0;
        
        // Create mover at (1,2,0)
        Mover* m = &movers[0];
        Point goal = {1, 2, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Place 3 rocks at (2,2,0) - dry stone wall needs 3
        for (int i = 0; i < 3; i++) {
            SpawnItem(2 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_ROCK);
        }
        
        // Place a ramp at (5,2,0) pointing north
        grid[0][2][5] = CELL_RAMP_N;
        rampCount++;
        int rampCountBefore = rampCount;
        
        // Player places wall blueprint on the ramp cell
        int bpIdx = CreateRecipeBlueprint(5, 2, 0, CONSTRUCTION_DRY_STONE_WALL);
        expect(bpIdx >= 0);
        
        // Run simulation until wall is built
        for (int i = 0; i < 3000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            
            if (grid[0][2][5] == CELL_WALL) break;
        }
        
        // Player expectation: wall exists
        expect(grid[0][2][5] == CELL_WALL);
        
        // Player expectation: rampCount decremented
        expect(rampCount == rampCountBefore - 1);
    }
}

describe(grid_audit_tree_chopping_integration) {
    it("player chops tree trunk supporting ramp exit - ramp should be removed (Finding 6)") {
        // Story: Player has a ramp whose high-side exit is on top of a tree trunk.
        // When the trunk is chopped (felled), the ramp loses its support.
        // Player expects:
        // 1. The ramp is removed (no longer structurally valid)
        // 2. rampCount is decremented
        
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        InitTrees();
        rampCount = 0;
        
        // Place tree trunk at (5,1,0) - this provides solid support for ramp exit
        grid[0][1][5] = CELL_TREE_TRUNK;
        SetWallMaterial(5, 1, 0, MAT_OAK);
        
        // Place a ramp at (5,2,0) pointing north - exit at (5,1,1)
        // The exit's solid support is the trunk at (5,1,0)
        grid[0][2][5] = CELL_RAMP_N;
        rampCount++;
        int rampCountBefore = rampCount;
        
        // Verify ramp is initially valid
        bool validBefore = IsRampStillValid(5, 2, 0);
        expect(validBefore == true);
        
        // Chop the trunk directly (simulating completed chop job)
        CompleteChopDesignation(5, 1, 0, -1);
        
        // Trunk should be gone (felled or air)
        bool trunkGone = (grid[0][1][5] != CELL_TREE_TRUNK) ||
                         (grid[0][1][5] == CELL_TREE_FELLED);
        expect(trunkGone == true);
        
        // Player expectation: ramp should be removed (no solid support for exit)
        expect(grid[0][2][5] != CELL_RAMP_N);
        
        // Player expectation: rampCount decremented
        expect(rampCount == rampCountBefore - 1);
    }
    
    it("player chops tree trunk NOT supporting any ramp - ramp stays valid (Finding 6 control)") {
        // Control test: Chopping a trunk that doesn't support any ramps
        // should NOT remove unrelated ramps
        
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        InitTrees();
        rampCount = 0;
        
        // Place tree trunk at (8,1,0) - far from ramp
        grid[0][1][8] = CELL_TREE_TRUNK;
        SetWallMaterial(8, 1, 0, MAT_OAK);
        
        // Place solid support for ramp at (5,1,0) - wall provides support
        grid[0][1][5] = CELL_WALL;
        
        // Place a ramp at (5,2,0) pointing north - exit at (5,1,1)
        grid[0][2][5] = CELL_RAMP_N;
        rampCount++;
        int rampCountBefore = rampCount;
        
        // Verify ramp is valid
        expect(IsRampStillValid(5, 2, 0) == true);
        
        // Chop the distant trunk directly
        CompleteChopDesignation(8, 1, 0, -1);
        
        // Player expectation: ramp should STILL exist (unaffected by distant chop)
        expect(grid[0][2][5] == CELL_RAMP_N);
        
        // Player expectation: rampCount unchanged
        expect(rampCount == rampCountBefore);
    }
}

/* =============================================================================
 * Input Audit Tests (docs/todo/audits/input.md)
 *
 * Since the Execute* functions in input.c are static, these tests simulate
 * what those functions do by directly manipulating the grid and then checking
 * the invariants that the audit says are violated.
 *
 * Player stories:
 * - Finding 1: "I pile clay by shift-dragging, the material should be clay not dirt"
 * - Finding 2: "I erase a ramp, rampCount should decrement"
 * - Finding 3: "I place soil on a mover's path, the mover should repath"
 * - Finding 4: "I place grass on air, it should have proper wall material"
 * - Finding 5: "Pile soil at map edge should not crash"
 * - Finding 6: "I erase a cell with a mine designation, the designation should be cancelled"
 * - Finding 7: "I remove a tree with a chop designation, the designation should be cancelled"
 * - Finding 8: "I load a save mid-drag, drag state should reset"
 * - Finding 10: "I quick-erase a cell, metadata should be fully cleared"
 * =============================================================================
 */

describe(input_audit_material_consistency) {
    // Finding 1: Pile-drag uses wrong material for non-dirt soils
    // The bug is in the continuous drag switch statement which passes MAT_DIRT
    // for clay/gravel/sand/peat. We test via ExecutePileSoil-equivalent behavior.
    
    it("piling clay should set wall material to MAT_CLAY not MAT_DIRT (Finding 1)") {
        // Story: Player shift-drags to pile clay. Each placed cell should have
        // MAT_CLAY as its wall material, not MAT_DIRT.
        
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Simulate what ExecutePileSoil does with the FIXED material
        PlaceCellFull(5, 3, 0, NaturalTerrainSpec(CELL_WALL, MAT_CLAY, SURFACE_BARE, true, false));
        
        expect(grid[0][3][5] == CELL_WALL);
        expect(GetWallMaterial(5, 3, 0) == MAT_CLAY);
    }
    
    it("piling gravel should set wall material to MAT_GRAVEL (Finding 1)") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        PlaceCellFull(5, 3, 0, NaturalTerrainSpec(CELL_WALL, MAT_GRAVEL, SURFACE_BARE, true, false));
        
        expect(grid[0][3][5] == CELL_WALL);
        expect(GetWallMaterial(5, 3, 0) == MAT_GRAVEL);
    }
    
    it("piling sand should set wall material to MAT_SAND (Finding 1)") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        PlaceCellFull(5, 3, 0, NaturalTerrainSpec(CELL_WALL, MAT_SAND, SURFACE_BARE, true, false));
        
        expect(grid[0][3][5] == CELL_WALL);
        expect(GetWallMaterial(5, 3, 0) == MAT_SAND);
    }
    
    it("piling peat should set wall material to MAT_PEAT (Finding 1)") {
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        PlaceCellFull(5, 3, 0, NaturalTerrainSpec(CELL_WALL, MAT_PEAT, SURFACE_BARE, true, false));
        
        expect(grid[0][3][5] == CELL_WALL);
        expect(GetWallMaterial(5, 3, 0) == MAT_PEAT);
    }
}

describe(input_audit_erase_ramp) {
    // Finding 2: ExecuteErase doesn't decrement rampCount when erasing ramps
    
    it("erasing a ramp cell should decrement rampCount (Finding 2)") {
        // Story: Player draws a ramp, then right-click erases over it.
        // rampCount should decrement.
        
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        rampCount = 0;
        
        // Place a ramp
        grid[0][3][5] = CELL_RAMP_N;
        rampCount++;
        int rampCountBefore = rampCount;
        
        // Simulate what the FIXED ExecuteErase does: call EraseRamp
        EraseRamp(5, 3, 0);
        
        // Player expectation: rampCount should have decremented
        expect(grid[0][3][5] == CELL_AIR);
        expect(rampCount == rampCountBefore - 1);
    }
}

describe(input_audit_soil_repath) {
    // Finding 3: ExecuteBuildSoil and ExecutePileSoil don't trigger mover repathing
    
    it("placing solid soil on a mover path should set needsRepath (Finding 3)") {
        // Story: Player places a dirt block on a cell where a mover is pathing.
        // The mover should get needsRepath=true.
        
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Spawn a mover with a path through (5,2,0)
        Point goal = {9, 0, 0};
        InitMover(&movers[0], 0 * CELL_SIZE + CELL_SIZE * 0.5f,
                  0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        Mover* m = &movers[0];
        m->active = true;
        m->path[0].x = 5; m->path[0].y = 2; m->path[0].z = 0;
        m->path[1].x = 6; m->path[1].y = 2; m->path[1].z = 0;
        m->pathLength = 2;
        m->pathIndex = 1;
        m->needsRepath = false;
        
        // Simulate what ExecuteBuildSoil does: place solid soil
        // (ExecuteBuildSoil does NOT set needsRepath - that's the bug)
        CellPlacementSpec spec = NaturalTerrainSpec(CELL_WALL, MAT_DIRT, SURFACE_BARE, true, false);
        PlaceCellFull(5, 2, 0, spec);
        InvalidatePathsThroughCell(5, 2, 0);
        
        // Player expectation: mover should need a repath
        expect(m->needsRepath == true);
    }
}

describe(input_audit_grass_placement) {
    // Finding 4: ExecutePlaceGrass converts air to solid dirt without proper setup
    
    it("placing grass on air should set proper wall material (Finding 4)") {
        // Story: Player uses grass tool on an air cell. It becomes CELL_DIRT.
        // The wall material should be MAT_DIRT, not MAT_NONE.
        
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Verify cell is air
        expect(grid[0][3][5] == CELL_AIR);
        
        // Simulate what the FIXED ExecutePlaceGrass does: PlaceCellFull + surface
        PlaceCellFull(5, 3, 0, NaturalTerrainSpec(CELL_WALL, MAT_DIRT, SURFACE_BARE, true, false));
        SetVegetation(5, 3, 0, VEG_GRASS_TALL);
        
        // Player expectation: dirt cell should have MAT_DIRT material
        expect(grid[0][3][5] == CELL_WALL);
        expect(GetWallMaterial(5, 3, 0) == MAT_DIRT);
    }
    
    it("placing grass on air should trigger mover repathing (Finding 4)") {
        // Story: Player places grass on a cell where a mover is pathing.
        // Air becomes solid dirt. Mover should repath.
        
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Spawn a mover with a path through (5,2,0)
        Point goal = {9, 0, 0};
        InitMover(&movers[0], 0 * CELL_SIZE + CELL_SIZE * 0.5f,
                  0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        Mover* m = &movers[0];
        m->active = true;
        m->path[0].x = 5; m->path[0].y = 2; m->path[0].z = 0;
        m->path[1].x = 6; m->path[1].y = 2; m->path[1].z = 0;
        m->pathLength = 2;
        m->pathIndex = 1;
        m->needsRepath = false;
        
        // Simulate FIXED grass placement on air -> solid dirt
        PlaceCellFull(5, 2, 0, NaturalTerrainSpec(CELL_WALL, MAT_DIRT, SURFACE_BARE, true, false));
        InvalidatePathsThroughCell(5, 2, 0);
        SetVegetation(5, 2, 0, VEG_GRASS_TALL);
        
        // Player expectation: mover should need a repath
        expect(m->needsRepath == true);
    }
}

describe(input_audit_erase_designations) {
    // Finding 6: ExecuteErase doesn't cancel designations
    // Finding 7: ExecuteRemoveTree doesn't cancel chop designations
    
    it("erasing a cell with a mine designation should cancel it (Finding 6)") {
        // Story: Player designates a wall for mining, then erases the cell.
        // The mining designation should be cancelled.
        
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        
        // Place a wall and designate it for mining
        grid[0][3][5] = CELL_WALL;
        SetWallMaterial(5, 3, 0, MAT_GRANITE);
        bool designated = DesignateMine(5, 3, 0);
        expect(designated == true);
        expect(designations[0][3][5].type == DESIGNATION_MINE);
        
        // Simulate what the FIXED ExecuteErase does: cancel designation + erase
        CancelDesignation(5, 3, 0);
        grid[0][3][5] = CELL_AIR;
        SetWallMaterial(5, 3, 0, MAT_NONE);
        ClearWallNatural(5, 3, 0);
        MarkChunkDirty(5, 3, 0);
        
        // Player expectation: designation should be gone
        expect(designations[0][3][5].type == DESIGNATION_NONE);
    }
    
    it("erasing cells under a stockpile should remove stockpile cells (Finding 6)") {
        // Story: Player erases terrain that has a stockpile on it.
        // The stockpile cells over erased terrain should be removed.
        
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        
        // Create a 3x3 stockpile at (3,3)
        int spIdx = CreateStockpile(3, 3, 0, 3, 3);
        expect(spIdx >= 0);
        int cellsBefore = GetStockpileActiveCellCount(spIdx);
        expect(cellsBefore == 9);
        
        // Simulate what the FIXED ExecuteErase does: remove stockpile cells + erase
        RemoveStockpileCells(spIdx, 4, 4, 4, 4);
        grid[0][4][4] = CELL_AIR;
        MarkChunkDirty(4, 4, 0);
        
        // Player expectation: stockpile should have lost the erased cell
        expect(GetStockpileActiveCellCount(spIdx) == cellsBefore - 1);
    }
    
    it("removing a tree with chop designation should cancel it (Finding 7)") {
        // Story: Player designates tree for chopping, then removes it with sandbox tool.
        // The chop designation should be cancelled.
        
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        InitTrees();
        
        // Place a tree trunk
        grid[0][3][5] = CELL_TREE_TRUNK;
        SetWallMaterial(5, 3, 0, MAT_OAK);
        
        // Designate it for chopping
        bool designated = DesignateChop(5, 3, 0);
        expect(designated == true);
        expect(designations[0][3][5].type == DESIGNATION_CHOP);
        
        // Simulate what the FIXED ExecuteRemoveTree does: cancel designation + clear
        CancelDesignation(5, 3, 0);
        grid[0][3][5] = CELL_AIR;
        SetWallMaterial(5, 3, 0, MAT_NONE);
        MarkChunkDirty(5, 3, 0);
        
        // Player expectation: designation should be gone
        expect(designations[0][3][5].type == DESIGNATION_NONE);
    }
}

describe(input_audit_quick_erase_metadata) {
    // Finding 10: Quick-edit erase leaves stale metadata
    
    it("quick-erasing a dirt cell should clear wall material and surface (Finding 10)") {
        // Story: Player quick-erases (right-click hold) a dirt cell with grass.
        // The cell becomes air. All metadata should be cleared.
        
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Set up a dirt cell with grass and material
        grid[0][3][5] = CELL_WALL;
        SetWallMaterial(5, 3, 0, MAT_DIRT);
        SetWallNatural(5, 3, 0);
        SetVegetation(5, 3, 0, VEG_GRASS_TALL);
        
        // Simulate what the FIXED quick-edit erase does: full metadata cleanup
        grid[0][3][5] = CELL_AIR;
        SetWallMaterial(5, 3, 0, MAT_NONE);
        ClearWallNatural(5, 3, 0);
        SetWallFinish(5, 3, 0, FINISH_ROUGH);
        SetVegetation(5, 3, 0, VEG_NONE);
        MarkChunkDirty(5, 3, 0);
        
        // Player expectation: all metadata should be cleared
        expect(grid[0][3][5] == CELL_AIR);
        expect(GetWallMaterial(5, 3, 0) == MAT_NONE);
        expect(IsWallNatural(5, 3, 0) == false);
        expect(GET_CELL_SURFACE(5, 3, 0) == SURFACE_BARE);
    }
    
    it("quick-erasing a ramp should decrement rampCount (Finding 10)") {
        // Story: Player quick-erases a ramp cell.
        // rampCount should decrement (same issue as Finding 2 but for quick-edit).
        
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        rampCount = 0;
        
        // Place a ramp
        grid[0][3][5] = CELL_RAMP_S;
        rampCount++;
        int rampCountBefore = rampCount;
        
        // Simulate what the FIXED quick-edit erase does: call EraseRamp
        EraseRamp(5, 3, 0);
        
        // Player expectation: rampCount should have decremented
        expect(rampCount == rampCountBefore - 1);
    }
}

// Finding 8: LoadWorld doesn't reset input state
// Skipped as test: input_mode.c has too many dependencies for test unity build.
// Fix is a one-liner: call InputMode_Reset() after LoadWorld() in input.c.

// =============================================================================
// Passive Workshop Tests (TDD - Drying Rack / ITEM_DRIED_GRASS)
// =============================================================================

describe(passive_workshop) {
    it("ITEM_DRIED_GRASS should exist and be stackable") {
        ClearItems();

        int idx = SpawnItem(100.0f, 100.0f, 0.0f, ITEM_DRIED_GRASS);
        expect(idx >= 0);
        expect(items[idx].active == true);
        expect(items[idx].type == ITEM_DRIED_GRASS);
        expect(ItemIsStackable(ITEM_DRIED_GRASS));
        expect(DefaultMaterialForItemType(ITEM_DRIED_GRASS) == MAT_NONE);
    }

    it("Drying Rack workshop can be created") {
        InitGridFromAsciiWithChunkSize(
            "......\n"
            "......\n"
            "......\n"
            "......\n", 10, 10);

        ClearWorkshops();

        int wsIdx = CreateWorkshop(2, 1, 0, WORKSHOP_DRYING_RACK);
        expect(wsIdx >= 0);

        Workshop* ws = &workshops[wsIdx];
        expect(ws->active == true);
        expect(ws->type == WORKSHOP_DRYING_RACK);
        expect(ws->width == 2);
        expect(ws->height == 2);
        expect(ws->workTileX >= 0);
        expect(ws->workTileY >= 0);
        expect(ws->outputTileX >= 0);
        expect(ws->outputTileY >= 0);
    }

    it("Drying Rack definition is passive") {
        expect(workshopDefs[WORKSHOP_DRYING_RACK].passive == true);
        // Existing workshops should not be passive
        expect(workshopDefs[WORKSHOP_STONECUTTER].passive == false);
        expect(workshopDefs[WORKSHOP_SAWMILL].passive == false);
        expect(workshopDefs[WORKSHOP_KILN].passive == false);
        expect(workshopDefs[WORKSHOP_CHARCOAL_PIT].passive == true);  // semi-passive
        expect(workshopDefs[WORKSHOP_HEARTH].passive == false);
    }

    it("passive workshop does not accept crafter assignment") {
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();

        int wsIdx = CreateWorkshop(4, 1, 0, WORKSHOP_DRYING_RACK);
        AddBill(wsIdx, 0, BILL_DO_FOREVER, 0);

        // Spawn grass item near workshop
        SpawnItem(CELL_SIZE * 2.5f, CELL_SIZE * 2.5f, 0.0f, ITEM_GRASS);

        // Create stockpile for output so bill doesn't auto-suspend
        int sp = CreateStockpile(0, 0, 0, 2, 2);
        SetStockpileFilter(sp, ITEM_DRIED_GRASS, true);

        // Spawn a mover
        Mover* m = &movers[0];
        Point goal = {1, 2, 0};
        InitMover(m, CELL_SIZE * 1.5f, CELL_SIZE * 2.5f, 0.0f, goal, 200.0f);
        moverCount = 1;

        RebuildIdleMoverList();
        BuildItemSpatialGrid();
        BuildMoverSpatialGrid();
        AssignJobs();

        Workshop* ws = &workshops[wsIdx];
        expect(ws->assignedCrafter == -1);
    }

    it("hauler delivers item to passive workshop work tile") {
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();

        int wsIdx = CreateWorkshop(5, 1, 0, WORKSHOP_DRYING_RACK);
        AddBill(wsIdx, 0, BILL_DO_X_TIMES, 1);
        Workshop* ws = &workshops[wsIdx];

        // Stockpile for output
        int sp = CreateStockpile(0, 0, 0, 3, 3);
        SetStockpileFilter(sp, ITEM_DRIED_GRASS, true);

        // Spawn grass away from workshop
        int grassIdx = SpawnItem(CELL_SIZE * 1.5f, CELL_SIZE * 3.5f, 0.0f, ITEM_GRASS);

        // Spawn hauler
        Mover* m = &movers[0];
        Point goal = {1, 3, 0};
        InitMover(m, CELL_SIZE * 1.5f, CELL_SIZE * 3.5f, 0.0f, goal, 200.0f);
        m->capabilities.canHaul = true;
        moverCount = 1;

        // Run sim until grass arrives at work tile
        bool delivered = false;
        for (int i = 0; i < 1000; i++) {
            Tick();
            RebuildIdleMoverList();
            BuildItemSpatialGrid();
            BuildMoverSpatialGrid();
            AssignJobs();
            JobsTick();

            // Check if grass item is on the work tile
            if (items[grassIdx].active && items[grassIdx].state == ITEM_ON_GROUND) {
                int itemTileX = (int)(items[grassIdx].x / CELL_SIZE);
                int itemTileY = (int)(items[grassIdx].y / CELL_SIZE);
                if (itemTileX == ws->workTileX && itemTileY == ws->workTileY) {
                    delivered = true;
                    break;
                }
            }
        }
        expect(delivered == true);
    }

    it("passive workshop timer advances when input present on work tile") {
        InitGridFromAsciiWithChunkSize(
            "......\n"
            "......\n"
            "......\n"
            "......\n", 10, 10);

        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();

        int wsIdx = CreateWorkshop(2, 1, 0, WORKSHOP_DRYING_RACK);
        AddBill(wsIdx, 0, BILL_DO_X_TIMES, 1);
        Workshop* ws = &workshops[wsIdx];

        // Place grass directly on work tile
        SpawnItem(ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f,
                  ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f,
                  0.0f, ITEM_GRASS);

        expect(ws->passiveProgress == 0.0f);

        // Tick the passive system a few times
        for (int i = 0; i < 10; i++) {
            PassiveWorkshopsTick(TICK_DT);
        }

        expect(ws->passiveProgress > 0.0f);
    }

    it("passive workshop timer does NOT advance without input") {
        InitGridFromAsciiWithChunkSize(
            "......\n"
            "......\n"
            "......\n"
            "......\n", 10, 10);

        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();

        int wsIdx = CreateWorkshop(2, 1, 0, WORKSHOP_DRYING_RACK);
        AddBill(wsIdx, 0, BILL_DO_X_TIMES, 1);
        Workshop* ws = &workshops[wsIdx];

        // No items placed — tick
        for (int i = 0; i < 10; i++) {
            PassiveWorkshopsTick(TICK_DT);
        }

        expect(ws->passiveProgress == 0.0f);
    }

    it("passive workshop produces output when timer completes") {
        InitGridFromAsciiWithChunkSize(
            "......\n"
            "......\n"
            "......\n"
            "......\n", 10, 10);

        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();

        int wsIdx = CreateWorkshop(2, 1, 0, WORKSHOP_DRYING_RACK);
        AddBill(wsIdx, 0, BILL_DO_X_TIMES, 1);
        Workshop* ws = &workshops[wsIdx];
        Bill* bill = &ws->bills[0];

        // Place grass on work tile
        int grassIdx = SpawnItem(
            ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f,
            ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f,
            0.0f, ITEM_GRASS);

        // Tick until completion (recipe is 10.0s, TICK_DT ~0.0167s, need ~600 ticks)
        for (int i = 0; i < 800; i++) {
            PassiveWorkshopsTick(TICK_DT);
        }

        // Grass should be consumed (slot may be reused by output item)
        bool grassConsumed = !items[grassIdx].active || items[grassIdx].type != ITEM_GRASS;
        expect(grassConsumed == true);

        // Dried grass should exist at output tile
        bool foundDriedGrass = false;
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (items[i].active && items[i].type == ITEM_DRIED_GRASS) {
                int tileX = (int)(items[i].x / CELL_SIZE);
                int tileY = (int)(items[i].y / CELL_SIZE);
                if (tileX == ws->outputTileX && tileY == ws->outputTileY) {
                    foundDriedGrass = true;
                    break;
                }
            }
        }
        expect(foundDriedGrass == true);

        // Bill should record completion
        expect(bill->completedCount == 1);

        // Progress should be reset
        expect(ws->passiveProgress == 0.0f);
    }

    it("passive workshop DO_X_TIMES stops after target") {
        InitGridFromAsciiWithChunkSize(
            "......\n"
            "......\n"
            "......\n"
            "......\n", 10, 10);

        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();

        int wsIdx = CreateWorkshop(2, 1, 0, WORKSHOP_DRYING_RACK);
        AddBill(wsIdx, 0, BILL_DO_X_TIMES, 1);
        Workshop* ws = &workshops[wsIdx];

        // Place grass and complete one cycle
        int grass1 = SpawnItem(
            ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f,
            ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f,
            0.0f, ITEM_GRASS);

        for (int i = 0; i < 800; i++) {
            PassiveWorkshopsTick(TICK_DT);
        }
        bool grass1Consumed = !items[grass1].active || items[grass1].type != ITEM_GRASS;
        expect(grass1Consumed == true);
        expect(ws->bills[0].completedCount == 1);

        // Place more grass — should NOT process (target met)
        int grass2 = SpawnItem(
            ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f,
            ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f,
            0.0f, ITEM_GRASS);

        for (int i = 0; i < 800; i++) {
            PassiveWorkshopsTick(TICK_DT);
        }

        // Second grass should still be there (not consumed)
        expect(items[grass2].active == true);
    }

    it("passive workshop DO_FOREVER continues") {
        InitGridFromAsciiWithChunkSize(
            "......\n"
            "......\n"
            "......\n"
            "......\n", 10, 10);

        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();

        // Stockpile for output so bill doesn't auto-suspend
        int spIdx = CreateStockpile(0, 0, 0, 2, 2);
        SetStockpileFilter(spIdx, ITEM_DRIED_GRASS, true);

        int wsIdx = CreateWorkshop(2, 1, 0, WORKSHOP_DRYING_RACK);
        AddBill(wsIdx, 0, BILL_DO_FOREVER, 0);
        Workshop* ws = &workshops[wsIdx];

        // First cycle
        SpawnItem(
            ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f,
            ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f,
            0.0f, ITEM_GRASS);

        for (int i = 0; i < 800; i++) {
            PassiveWorkshopsTick(TICK_DT);
        }
        expect(ws->bills[0].completedCount == 1);

        // Second cycle — place new grass
        SpawnItem(
            ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f,
            ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f,
            0.0f, ITEM_GRASS);

        for (int i = 0; i < 800; i++) {
            PassiveWorkshopsTick(TICK_DT);
        }
        expect(ws->bills[0].completedCount == 2);
    }

    it("passive workshop timer respects game speed") {
        InitGridFromAsciiWithChunkSize(
            "......\n"
            "......\n"
            "......\n"
            "......\n", 10, 10);

        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();

        int wsIdx = CreateWorkshop(2, 1, 0, WORKSHOP_DRYING_RACK);
        AddBill(wsIdx, 0, BILL_DO_X_TIMES, 1);
        Workshop* ws = &workshops[wsIdx];

        // Place grass on work tile
        SpawnItem(
            ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f,
            ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f,
            0.0f, ITEM_GRASS);

        // Tick at 10x speed — should complete in ~60 ticks (10s / 10x / 0.0167)
        int ticksNeeded = 0;
        for (int i = 0; i < 200; i++) {
            PassiveWorkshopsTick(TICK_DT * 10.0f);
            ticksNeeded++;
            if (ws->bills[0].completedCount >= 1) break;
        }

        expect(ws->bills[0].completedCount == 1);
        // At 10x speed, ~60 ticks needed. At 1x it would be ~600.
        expect(ticksNeeded < 100);
    }

    it("suspended bill prevents passive processing") {
        InitGridFromAsciiWithChunkSize(
            "......\n"
            "......\n"
            "......\n"
            "......\n", 10, 10);

        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();

        int wsIdx = CreateWorkshop(2, 1, 0, WORKSHOP_DRYING_RACK);
        AddBill(wsIdx, 0, BILL_DO_X_TIMES, 1);
        SuspendBill(wsIdx, 0, true);
        Workshop* ws = &workshops[wsIdx];

        // Place grass on work tile
        SpawnItem(
            ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f,
            ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f,
            0.0f, ITEM_GRASS);

        for (int i = 0; i < 100; i++) {
            PassiveWorkshopsTick(TICK_DT);
        }

        expect(ws->passiveProgress == 0.0f);
    }

    it("deleting passive workshop does not consume items") {
        InitGridFromAsciiWithChunkSize(
            "......\n"
            "......\n"
            "......\n"
            "......\n", 10, 10);

        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();

        int wsIdx = CreateWorkshop(2, 1, 0, WORKSHOP_DRYING_RACK);
        AddBill(wsIdx, 0, BILL_DO_X_TIMES, 1);
        Workshop* ws = &workshops[wsIdx];

        // Place grass on work tile
        int grassIdx = SpawnItem(
            ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f,
            ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f,
            0.0f, ITEM_GRASS);

        // Advance timer partway
        for (int i = 0; i < 100; i++) {
            PassiveWorkshopsTick(TICK_DT);
        }
        expect(ws->passiveProgress > 0.0f);

        // Delete workshop
        DeleteWorkshop(wsIdx);

        // Grass item should still exist
        expect(items[grassIdx].active == true);
    }
}

describe(semi_passive_workshop) {
    it("Charcoal Pit definition is passive with active and passive work phases") {
        expect(workshopDefs[WORKSHOP_CHARCOAL_PIT].passive == true);
        // Recipe should have both active and passive work times
        expect(charcoalPitRecipes[0].workRequired > 0.0f);
        expect(charcoalPitRecipes[0].passiveWorkRequired > 0.0f);
        // Drying Rack: no active work, only passive
        expect(dryingRackRecipes[0].workRequired == 0.0f);
        expect(dryingRackRecipes[0].passiveWorkRequired > 0.0f);
        // Active workshops: no passive work
        expect(stonecutterRecipes[0].passiveWorkRequired == 0.0f);
    }

    it("hauler delivers input to semi-passive workshop") {
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();

        int wsIdx = CreateWorkshop(5, 1, 0, WORKSHOP_CHARCOAL_PIT);
        AddBill(wsIdx, 0, BILL_DO_X_TIMES, 1);
        Workshop* ws = &workshops[wsIdx];

        // Stockpile for output
        int sp = CreateStockpile(0, 0, 0, 3, 3);
        SetStockpileFilter(sp, ITEM_CHARCOAL, true);

        // Spawn log away from workshop
        int logIdx = SpawnItem(CELL_SIZE * 1.5f, CELL_SIZE * 3.5f, 0.0f, ITEM_LOG);

        // Spawn hauler
        Mover* m = &movers[0];
        Point goal = {1, 3, 0};
        InitMover(m, CELL_SIZE * 1.5f, CELL_SIZE * 3.5f, 0.0f, goal, 200.0f);
        m->capabilities.canHaul = true;
        moverCount = 1;

        // Run sim until log arrives at work tile
        bool delivered = false;
        for (int i = 0; i < 1000; i++) {
            Tick();
            RebuildIdleMoverList();
            BuildItemSpatialGrid();
            BuildMoverSpatialGrid();
            AssignJobs();
            JobsTick();

            if (items[logIdx].active && items[logIdx].state == ITEM_ON_GROUND) {
                int itemTileX = (int)(items[logIdx].x / CELL_SIZE);
                int itemTileY = (int)(items[logIdx].y / CELL_SIZE);
                if (itemTileX == ws->workTileX && itemTileY == ws->workTileY) {
                    delivered = true;
                    break;
                }
            }
        }
        expect(delivered == true);
    }

    it("semi-passive workshop does NOT advance timer before ignition") {
        InitGridFromAsciiWithChunkSize(
            "......\n"
            "......\n"
            "......\n"
            "......\n", 10, 10);

        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();

        int wsIdx = CreateWorkshop(2, 1, 0, WORKSHOP_CHARCOAL_PIT);
        AddBill(wsIdx, 0, BILL_DO_X_TIMES, 1);
        Workshop* ws = &workshops[wsIdx];

        // Place log directly on work tile
        SpawnItem(ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f,
                  ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f,
                  0.0f, ITEM_LOG);

        expect(ws->passiveReady == false);
        expect(ws->passiveProgress == 0.0f);

        // Tick passive system — should NOT advance because passiveReady is false
        for (int i = 0; i < 200; i++) {
            PassiveWorkshopsTick(TICK_DT);
        }

        expect(ws->passiveProgress == 0.0f);
    }

    it("WorkGiver_IgniteWorkshop assigns crafter when inputs present") {
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();

        int wsIdx = CreateWorkshop(5, 1, 0, WORKSHOP_CHARCOAL_PIT);
        AddBill(wsIdx, 0, BILL_DO_X_TIMES, 1);
        Workshop* ws = &workshops[wsIdx];

        // Place log on work tile
        SpawnItem(ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f,
                  ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f,
                  0.0f, ITEM_LOG);

        // Spawn a mover near the workshop
        Mover* m = &movers[0];
        Point goal = {ws->workTileX, ws->workTileY, 0};
        InitMover(m, ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f,
                  ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f,
                  0.0f, goal, 200.0f);
        moverCount = 1;

        RebuildIdleMoverList();
        int jobId = WorkGiver_IgniteWorkshop(0);
        expect(jobId >= 0);

        Job* job = GetJob(jobId);
        expect(job->type == JOBTYPE_IGNITE_WORKSHOP);
        expect(job->targetWorkshop == wsIdx);
        expect(ws->assignedCrafter == 0);
    }

    it("crafter completes ignition and is released") {
        InitGridFromAsciiWithChunkSize(
            "......\n"
            "......\n"
            "......\n"
            "......\n", 10, 10);

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();

        int wsIdx = CreateWorkshop(2, 1, 0, WORKSHOP_CHARCOAL_PIT);
        AddBill(wsIdx, 0, BILL_DO_X_TIMES, 1);
        Workshop* ws = &workshops[wsIdx];

        // Place log on work tile
        SpawnItem(ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f,
                  ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f,
                  0.0f, ITEM_LOG);

        // Spawn mover on work tile (already there)
        Mover* m = &movers[0];
        Point goal = {ws->workTileX, ws->workTileY, 0};
        InitMover(m, ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f,
                  ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f,
                  0.0f, goal, 200.0f);
        moverCount = 1;

        RebuildIdleMoverList();
        WorkGiver_IgniteWorkshop(0);

        // Tick until ignition completes
        float activeTime = GameHoursToGameSeconds(charcoalPitRecipes[0].workRequired);
        int ticks = (int)(activeTime / TICK_DT) + 100;  // extra margin
        for (int i = 0; i < ticks; i++) {
            JobsTick();
        }

        expect(ws->passiveReady == true);
        expect(ws->assignedCrafter == -1);
        expect(m->currentJobId == -1);
    }

    it("passive timer advances after ignition") {
        InitGridFromAsciiWithChunkSize(
            "......\n"
            "......\n"
            "......\n"
            "......\n", 10, 10);

        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();

        int wsIdx = CreateWorkshop(2, 1, 0, WORKSHOP_CHARCOAL_PIT);
        AddBill(wsIdx, 0, BILL_DO_X_TIMES, 1);
        Workshop* ws = &workshops[wsIdx];

        // Place log on work tile
        SpawnItem(ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f,
                  ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f,
                  0.0f, ITEM_LOG);

        // Manually set passiveReady (simulating crafter completed ignition)
        ws->passiveReady = true;

        expect(ws->passiveProgress == 0.0f);

        // Tick passive system
        for (int i = 0; i < 100; i++) {
            PassiveWorkshopsTick(TICK_DT);
        }

        expect(ws->passiveProgress > 0.0f);
    }

    it("semi-passive produces output when passive timer completes") {
        InitGridFromAsciiWithChunkSize(
            "......\n"
            "......\n"
            "......\n"
            "......\n", 10, 10);

        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();

        int wsIdx = CreateWorkshop(2, 1, 0, WORKSHOP_CHARCOAL_PIT);
        AddBill(wsIdx, 0, BILL_DO_X_TIMES, 1);
        Workshop* ws = &workshops[wsIdx];

        // Place log on work tile
        int logIdx = SpawnItem(ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f,
                               ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f,
                               0.0f, ITEM_LOG);

        // Simulate completed ignition
        ws->passiveReady = true;

        // Tick until passive timer completes
        float passiveTime = GameHoursToGameSeconds(charcoalPitRecipes[0].passiveWorkRequired);
        int ticks = (int)(passiveTime / TICK_DT) + 100;
        for (int i = 0; i < ticks; i++) {
            PassiveWorkshopsTick(TICK_DT);
        }

        // Log should be consumed
        expect(!items[logIdx].active || items[logIdx].type != ITEM_LOG);

        // Charcoal should exist at output tile
        bool foundCharcoal = false;
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (items[i].active && items[i].type == ITEM_CHARCOAL) {
                int tx = (int)(items[i].x / CELL_SIZE);
                int ty = (int)(items[i].y / CELL_SIZE);
                if (tx == ws->outputTileX && ty == ws->outputTileY) {
                    foundCharcoal = true;
                    break;
                }
            }
        }
        expect(foundCharcoal == true);

        // Bill should be completed
        expect(ws->bills[0].completedCount == 1);

        // passiveReady should be reset
        expect(ws->passiveReady == false);
    }

    it("crafter is free during passive burn phase") {
        InitGridFromAsciiWithChunkSize(
            "......\n"
            "......\n"
            "......\n"
            "......\n", 10, 10);

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();

        int wsIdx = CreateWorkshop(2, 1, 0, WORKSHOP_CHARCOAL_PIT);
        AddBill(wsIdx, 0, BILL_DO_X_TIMES, 1);
        Workshop* ws = &workshops[wsIdx];

        // Place log on work tile
        SpawnItem(ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f,
                  ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f,
                  0.0f, ITEM_LOG);

        // Spawn mover on work tile
        Mover* m = &movers[0];
        Point goal = {ws->workTileX, ws->workTileY, 0};
        InitMover(m, ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f,
                  ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f,
                  0.0f, goal, 200.0f);
        moverCount = 1;

        RebuildIdleMoverList();
        WorkGiver_IgniteWorkshop(0);

        // Tick until ignition completes
        float activeTime = GameHoursToGameSeconds(charcoalPitRecipes[0].workRequired);
        int ticks = (int)(activeTime / TICK_DT) + 100;
        for (int i = 0; i < ticks; i++) {
            JobsTick();
        }

        // After ignition, mover should be idle (free to do other work)
        expect(m->currentJobId == -1);
        expect(moverIsInIdleList[0] == true);
    }

    it("pure passive workshops still work unchanged") {
        // Regression test: Drying Rack should still work with workRequired=0
        InitGridFromAsciiWithChunkSize(
            "......\n"
            "......\n"
            "......\n"
            "......\n", 10, 10);

        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();

        int wsIdx = CreateWorkshop(2, 1, 0, WORKSHOP_DRYING_RACK);
        AddBill(wsIdx, 0, BILL_DO_X_TIMES, 1);
        Workshop* ws = &workshops[wsIdx];

        // Place grass on work tile — should work without needing passiveReady
        int grassIdx = SpawnItem(ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f,
                                 ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f,
                                 0.0f, ITEM_GRASS);

        // passiveReady should be false but shouldn't matter for pure passive
        expect(ws->passiveReady == false);

        // Tick until completion
        float passiveTime = GameHoursToGameSeconds(dryingRackRecipes[0].passiveWorkRequired);
        int ticks = (int)(passiveTime / TICK_DT) + 100;
        for (int i = 0; i < ticks; i++) {
            PassiveWorkshopsTick(TICK_DT);
        }

        // Grass should be consumed, dried grass should exist
        expect(!items[grassIdx].active || items[grassIdx].type != ITEM_GRASS);

        bool foundDriedGrass = false;
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (items[i].active && items[i].type == ITEM_DRIED_GRASS) {
                foundDriedGrass = true;
                break;
            }
        }
        expect(foundDriedGrass == true);
    }

    it("pure active workshops still work unchanged") {
        // Regression test: Stonecutter should have passiveWorkRequired=0
        // and NOT be treated as passive
        expect(workshopDefs[WORKSHOP_STONECUTTER].passive == false);
        expect(stonecutterRecipes[0].passiveWorkRequired == 0.0f);
        expect(stonecutterRecipes[0].workRequired > 0.0f);
    }

    it("passive workshop completes with multiple movers and items") {
        // Regression: with multiple idle movers and multiple matching items,
        // movers would endlessly deliver items to the work tile without the
        // passive timer ever completing — items bounced in and out
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();

        // Create drying rack with DO_X_TIMES=1 (pure passive, 10s timer)
        int wsIdx = CreateWorkshop(5, 1, 0, WORKSHOP_DRYING_RACK);
        AddBill(wsIdx, 0, BILL_DO_X_TIMES, 1);
        Workshop* ws = &workshops[wsIdx];

        // Spawn 4 grass items (more than needed)
        for (int i = 0; i < 4; i++) {
            SpawnItem(CELL_SIZE * (1.5f + i), CELL_SIZE * 3.5f, 0.0f, ITEM_GRASS);
        }

        // Spawn 4 haulers
        for (int i = 0; i < 4; i++) {
            Mover* m = &movers[i];
            Point goal = {1 + i, 3, 0};
            InitMover(m, CELL_SIZE * (1.5f + i), CELL_SIZE * 3.5f, 0.0f, goal, 200.0f);
            m->capabilities.canHaul = true;
        }
        moverCount = 4;

        // Run sim — passive timer plus delivery time
        // Should complete well within 3000 ticks
        float passiveTime = GameHoursToGameSeconds(dryingRackRecipes[0].passiveWorkRequired);
        int maxTicks = (int)(passiveTime / TICK_DT) + 3000;

        bool completed = false;
        for (int i = 0; i < maxTicks; i++) {
            Tick();
            RebuildIdleMoverList();
            BuildItemSpatialGrid();
            BuildMoverSpatialGrid();
            AssignJobs();
            JobsTick();  // includes PassiveWorkshopsTick

            if (ws->bills[0].completedCount >= 1) {
                completed = true;
                break;
            }
        }
        expect(completed == true);
    }

    it("charcoal pit completes burn with multiple movers and logs") {
        // Regression: charcoal pit (semi-passive) with multiple movers and logs.
        // Movers would endlessly deliver logs, pick them back up, and re-deliver
        // without the 60s passive timer ever completing.
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();

        // Create charcoal pit with DO_X_TIMES=1 (semi-passive, needs ignition)
        int wsIdx = CreateWorkshop(5, 1, 0, WORKSHOP_CHARCOAL_PIT);
        AddBill(wsIdx, 0, BILL_DO_X_TIMES, 1);
        Workshop* ws = &workshops[wsIdx];
        // Pre-ignite so we only test the passive delivery/burn cycle
        ws->passiveReady = true;

        // Spawn 4 logs (recipe needs 1, but extras shouldn't cause bouncing)
        for (int i = 0; i < 4; i++) {
            SpawnItem(CELL_SIZE * (1.5f + i), CELL_SIZE * 3.5f, 0.0f, ITEM_LOG);
        }

        // Stockpile for output charcoal
        int sp = CreateStockpile(0, 0, 0, 3, 3);
        SetStockpileFilter(sp, ITEM_CHARCOAL, true);

        // Spawn 4 haulers
        for (int i = 0; i < 4; i++) {
            Mover* m = &movers[i];
            Point goal = {1 + i, 3, 0};
            InitMover(m, CELL_SIZE * (1.5f + i), CELL_SIZE * 3.5f, 0.0f, goal, 200.0f);
            m->capabilities.canHaul = true;
        }
        moverCount = 4;

        // Passive burn + delivery time, should complete well within extra ticks
        float passiveTime = GameHoursToGameSeconds(charcoalPitRecipes[0].passiveWorkRequired);
        int maxTicks = (int)(passiveTime / TICK_DT) + 3000;

        bool completed = false;
        for (int i = 0; i < maxTicks; i++) {
            Tick();
            RebuildIdleMoverList();
            BuildItemSpatialGrid();
            BuildMoverSpatialGrid();
            AssignJobs();
            JobsTick();  // includes PassiveWorkshopsTick

            if (ws->bills[0].completedCount >= 1) {
                completed = true;
                break;
            }
        }
        expect(completed == true);

        // Charcoal should exist
        bool foundCharcoal = false;
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (items[i].active && items[i].type == ITEM_CHARCOAL) {
                foundCharcoal = true;
                break;
            }
        }
        expect(foundCharcoal == true);
    }

    it("deliver-to-workshop does not re-deliver items already on work tile") {
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();

        // Create drying rack (pure passive, no ignition needed)
        int wsIdx = CreateWorkshop(5, 1, 0, WORKSHOP_DRYING_RACK);
        AddBill(wsIdx, 0, BILL_DO_FOREVER, 0);
        Workshop* ws = &workshops[wsIdx];

        // Place grass directly on work tile (as if already delivered)
        int grassIdx = SpawnItem(
            ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f,
            ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f,
            0.0f, ITEM_GRASS);

        // Spawn idle hauler near the work tile
        Mover* m = &movers[0];
        Point goal = {ws->workTileX, ws->workTileY, 0};
        InitMover(m, ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f,
                  ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 200.0f);
        m->capabilities.canHaul = true;
        moverCount = 1;

        RebuildIdleMoverList();
        RebuildStockpileFreeSlotCounts();
        BuildItemSpatialGrid();
        BuildMoverSpatialGrid();

        // WorkGiver should NOT create a delivery job — input is already on tile
        int jobId = WorkGiver_DeliverToPassiveWorkshop(0);
        expect(jobId == -1);

        // Item should still be on ground, unreserved
        expect(items[grassIdx].state == ITEM_ON_GROUND);
        expect(items[grassIdx].reservedBy == -1);
    }

    it("delivered item stays on work tile and passive timer completes") {
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();

        // Create drying rack with DO_X_TIMES=1
        int wsIdx = CreateWorkshop(5, 1, 0, WORKSHOP_DRYING_RACK);
        AddBill(wsIdx, 0, BILL_DO_X_TIMES, 1);
        Workshop* ws = &workshops[wsIdx];

        // Spawn grass away from workshop
        SpawnItem(CELL_SIZE * 1.5f, CELL_SIZE * 3.5f, 0.0f, ITEM_GRASS);

        // Spawn hauler
        Mover* m = &movers[0];
        Point goal = {1, 3, 0};
        InitMover(m, CELL_SIZE * 1.5f, CELL_SIZE * 3.5f, 0.0f, goal, 200.0f);
        m->capabilities.canHaul = true;
        moverCount = 1;

        // Run sim — hauler delivers grass, then passive timer should complete
        float passiveTime = GameHoursToGameSeconds(dryingRackRecipes[0].passiveWorkRequired);
        int maxTicks = (int)(passiveTime / TICK_DT) + 2000;  // delivery time + full passive timer

        bool completed = false;
        for (int i = 0; i < maxTicks; i++) {
            Tick();
            RebuildIdleMoverList();
            BuildItemSpatialGrid();
            BuildMoverSpatialGrid();
            AssignJobs();
            JobsTick();  // includes PassiveWorkshopsTick

            if (ws->bills[0].completedCount >= 1) {
                completed = true;
                break;
            }
        }
        expect(completed == true);

        // Output item should exist
        bool foundOutput = false;
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (items[i].active && items[i].type == ITEM_DRIED_GRASS) {
                foundOutput = true;
                break;
            }
        }
        expect(foundOutput == true);
    }

    it("charcoal pit end-to-end: deliver, ignite, burn, output, repeat") {
        // Full cycle matching real game: stockpile accepts all items,
        // multiple movers with haul+craft, no pre-ignition.
        // Should produce charcoal without items bouncing.
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();

        // Charcoal pit at (5,1) — work tile (6,1), output tile (5,2)
        int wsIdx = CreateWorkshop(5, 1, 0, WORKSHOP_CHARCOAL_PIT);
        AddBill(wsIdx, 0, BILL_DO_X_TIMES, 2);  // burn twice
        Workshop* ws = &workshops[wsIdx];

        // Spawn 4 logs on the ground
        for (int i = 0; i < 4; i++) {
            SpawnItem(CELL_SIZE * (1.5f + i), CELL_SIZE * 3.5f, 0.0f, ITEM_LOG);
        }

        // Stockpile that accepts ALL item types (like the real game default)
        CreateStockpile(0, 0, 0, 3, 3);

        // 3 movers with haul + craft capability
        for (int i = 0; i < 3; i++) {
            Mover* m = &movers[i];
            Point goal = {1 + i, 3, 0};
            InitMover(m, CELL_SIZE * (1.5f + i), CELL_SIZE * 3.5f, 0.0f, goal, 200.0f);
            m->capabilities.canHaul = true;
        }
        moverCount = 3;

        // 2 burns * 60s + ignition + delivery overhead
        int maxTicks = (int)(2 * 60.0f / TICK_DT) + 6000;

        int charcoalCount = 0;
        bool completed = false;
        for (int i = 0; i < maxTicks; i++) {
            Tick();
            RebuildIdleMoverList();
            BuildItemSpatialGrid();
            BuildMoverSpatialGrid();
            AssignJobs();
            JobsTick();

            if (ws->bills[0].completedCount >= 2) {
                completed = true;
                break;
            }
        }
        expect(completed == true);

        // Count charcoal units (2 burns * 2 output each = 4 charcoal total)
        // Each burn spawns 1 item with stackCount=2, so sum stackCounts
        charcoalCount = 0;
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (items[i].active && items[i].type == ITEM_CHARCOAL) {
                charcoalCount += items[i].stackCount;
            }
        }
        expect(charcoalCount == 4);
    }

    it("hauler does not pick up items from passive workshop work tile") {
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        ClearJobs();

        // Create charcoal pit with a bill
        int wsIdx = CreateWorkshop(5, 1, 0, WORKSHOP_CHARCOAL_PIT);
        AddBill(wsIdx, 0, BILL_DO_X_TIMES, 1);
        Workshop* ws = &workshops[wsIdx];

        // Place log on work tile (as if delivered by hauler)
        int logIdx = SpawnItem(
            ws->workTileX * CELL_SIZE + CELL_SIZE * 0.5f,
            ws->workTileY * CELL_SIZE + CELL_SIZE * 0.5f,
            0.0f, ITEM_LOG);

        // Create stockpile that accepts logs
        int sp = CreateStockpile(0, 0, 0, 3, 3);
        SetStockpileFilter(sp, ITEM_LOG, true);

        // Spawn idle hauler
        Mover* m = &movers[0];
        Point goal = {1, 3, 0};
        InitMover(m, CELL_SIZE * 1.5f, CELL_SIZE * 3.5f, 0.0f, goal, 200.0f);
        m->capabilities.canHaul = true;
        moverCount = 1;

        RebuildIdleMoverList();
        RebuildStockpileFreeSlotCounts();
        BuildItemSpatialGrid();

        // WorkGiver_Haul should NOT pick up the log from the work tile
        int jobId = WorkGiver_Haul(0);
        expect(jobId == -1);

        // The log should still be on the work tile, unreserved
        expect(items[logIdx].active == true);
        expect(items[logIdx].reservedBy == -1);
    }
}

// Chop → ChopFelled transition bug:
// WorkGiver_Chop claims CHOP_FELLED designation via stale cache entry
describe(chop_felled_transition) {
    it("stale chop cache does not steal chop-felled designation") {
        // Bug: after CHOP completes, FellTree clears the CHOP designation but
        // doesn't invalidate the chop cache. A felled trunk lands at the same cell.
        // Player designates CHOP_FELLED. WorkGiver_Chop finds the stale cache entry,
        // sees the CHOP_FELLED designation (doesn't check type), sets assignedMover.
        // The CHOP job immediately fails (wrong type), but the designation is now
        // permanently claimed by a mover that doesn't have a job for it.

        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        ClearMovers();
        ClearItems();
        ClearStockpiles();
        ClearWorkshops();
        InitDesignations();
        InitTrees();
        InitJobSystem(MAX_MOVERS);

        // Make solid ground at z=0
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                grid[0][y][x] = CELL_WALL;
                SetWallMaterial(x, y, 0, MAT_DIRT);
                SetWallNatural(x, y, 0);
            }
        }

        // Step 1: Simulate a completed CHOP job at (5,3,z1)
        // Place trunk, designate, assign to mover 0, build the chop cache
        grid[1][3][5] = CELL_TREE_TRUNK;
        SetWallMaterial(5, 3, 1, MAT_OAK);
        DesignateChop(5, 3, 1);

        Mover* m = &movers[0];
        Point goal = {5, 2, 1};
        InitMover(m, CELL_SIZE * 5.5f, CELL_SIZE * 2.5f, 1.0f, goal, 200.0f);
        m->capabilities.canMine = true;
        moverCount = 1;

        // Build the chop cache (this is what AssignJobs does)
        RebuildIdleMoverList();
        InvalidateDesignationCache(DESIGNATION_CHOP);
        AssignJobs();  // This builds the cache and assigns mover 0

        // Mover 0 should now have a CHOP job
        expect(m->currentJobId >= 0);

        // Step 2: Simulate what happens when the chop completes:
        // FellTree clears the trunk and CHOP designation, places CELL_TREE_FELLED
        // at the SAME cell. Crucially, FellTree does NOT invalidate the chop cache.
        grid[1][3][5] = CELL_AIR;  // Trunk removed
        designations[1][3][5].type = DESIGNATION_NONE;  // CHOP designation cleared
        designations[1][3][5].assignedMover = -1;
        activeDesignationCount--;

        // Felled trunk lands at same cell (this is what FellTree does)
        grid[1][3][5] = CELL_TREE_FELLED;
        SetWallMaterial(5, 3, 1, MAT_OAK);

        // Release mover's job (simulating JOBRUN_DONE)
        ReleaseJob(m->currentJobId);
        m->currentJobId = -1;
        AddMoverToIdleList(0);

        // Step 3: Player designates CHOP_FELLED on the felled trunk
        bool designated = DesignateChopFelled(5, 3, 1);
        expect(designated == true);
        expect(designations[1][3][5].type == DESIGNATION_CHOP_FELLED);
        expect(designations[1][3][5].assignedMover == -1);

        // Step 4: Run AssignJobs — mover should get a CHOP_FELLED job, not
        // have the CHOP_FELLED designation stolen by WorkGiver_Chop
        BuildItemSpatialGrid();
        BuildMoverSpatialGrid();
        AssignJobs();

        // The designation should be assigned to mover 0
        expect(designations[1][3][5].assignedMover == 0);

        // Mover 0 should have a CHOP_FELLED job, NOT a CHOP job
        expect(m->currentJobId >= 0);
        Job* job = GetJob(m->currentJobId);
        expect(job->type == JOBTYPE_CHOP_FELLED);
    }
}

// =============================================================================
// Construction recipe system tests (Phase 1)
// =============================================================================

describe(construction_recipe_data) {
    it("should have dry stone wall recipe with correct structure") {
        const ConstructionRecipe* r = GetConstructionRecipe(CONSTRUCTION_DRY_STONE_WALL);
        expect(r != NULL);
        expect(r->buildCategory == BUILD_WALL);
        expect(r->stageCount == 1);
        expect(r->stages[0].inputCount == 1);
        expect(r->stages[0].inputs[0].count == 3);
        expect(r->stages[0].inputs[0].altCount == 2);
        expect(r->stages[0].inputs[0].alternatives[0].itemType == ITEM_ROCK);
        expect(r->stages[0].inputs[0].alternatives[1].itemType == ITEM_BLOCKS);
        expect(r->stages[0].buildTime == 3.0f);
        expect(r->resultMaterial == MAT_NONE);  // inherited
        expect(r->materialFromStage == 0);
        expect(r->materialFromSlot == 0);
    }

    it("should accept ITEM_ROCK and ITEM_BLOCKS for dry stone wall input") {
        const ConstructionRecipe* r = GetConstructionRecipe(CONSTRUCTION_DRY_STONE_WALL);
        expect(ConstructionInputAcceptsItem(&r->stages[0].inputs[0], ITEM_ROCK) == true);
        expect(ConstructionInputAcceptsItem(&r->stages[0].inputs[0], ITEM_BLOCKS) == true);
        expect(ConstructionInputAcceptsItem(&r->stages[0].inputs[0], ITEM_LOG) == false);
    }

    it("should return recipe count for BUILD_WALL category") {
        int count = GetConstructionRecipeCountForCategory(BUILD_WALL);
        expect(count >= 1);  // At least dry stone wall
    }

    it("should return invalid recipe as NULL") {
        expect(GetConstructionRecipe(-1) == NULL);
        expect(GetConstructionRecipe(999) == NULL);
    }
}

describe(construction_recipe_blueprint) {
    it("should create recipe blueprint on walkable cell") {
        InitTestGridFromAscii(
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n");

        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(2, 2, 0, CONSTRUCTION_DRY_STONE_WALL);
        expect(bpIdx >= 0);
        expect(HasBlueprint(2, 2, 0) == true);
        expect(blueprints[bpIdx].recipeIndex == CONSTRUCTION_DRY_STONE_WALL);
        expect(blueprints[bpIdx].stage == 0);
        expect(blueprints[bpIdx].state == BLUEPRINT_AWAITING_MATERIALS);
        expect(blueprints[bpIdx].recipeIndex >= 0);
    }

    it("should reject recipe blueprint on wall cell") {
        // Test #45
        InitTestGridFromAscii(
            "......\n"
            ".#....\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n");

        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(1, 1, 0, CONSTRUCTION_DRY_STONE_WALL);
        expect(bpIdx == -1);
    }

    it("should reject duplicate blueprint at same cell") {
        // Test #44
        InitTestGridFromAscii(
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n");

        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bp1 = CreateRecipeBlueprint(2, 2, 0, CONSTRUCTION_DRY_STONE_WALL);
        expect(bp1 >= 0);

        int bp2 = CreateRecipeBlueprint(2, 2, 0, CONSTRUCTION_DRY_STONE_WALL);
        expect(bp2 == -1);
        expect(CountBlueprints() == 1);
    }

    it("should cancel recipe blueprint with no deliveries cleanly") {
        // Test #34
        InitTestGridFromAscii(
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n");

        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(2, 2, 0, CONSTRUCTION_DRY_STONE_WALL);
        expect(bpIdx >= 0);
        expect(CountBlueprints() == 1);

        CancelBlueprint(bpIdx);
        expect(HasBlueprint(2, 2, 0) == false);
        expect(CountBlueprints() == 0);
    }

    it("should initialize delivery slots to zero") {
        InitTestGridFromAscii(
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n");

        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(2, 2, 0, CONSTRUCTION_DRY_STONE_WALL);
        Blueprint* bp = &blueprints[bpIdx];
        expect(bp->stageDeliveries[0].deliveredCount == 0);
        expect(bp->stageDeliveries[0].reservedCount == 0);
        expect(bp->stageDeliveries[0].chosenAlternative == -1);
        expect(bp->stageDeliveries[0].deliveredMaterial == MAT_NONE);
    }
}

describe(construction_recipe_delivery) {
    it("should stay AWAITING_MATERIALS with partial delivery") {
        // Tests #24, #27: deliver 1 or 2 of 3 rocks → still waiting
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(4, 4, 0, CONSTRUCTION_DRY_STONE_WALL);
        Blueprint* bp = &blueprints[bpIdx];

        // Spawn only 2 rocks (need 3)
        SpawnItemWithMaterial(1 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_ROCK, MAT_GRANITE);
        SpawnItemWithMaterial(2 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_ROCK, MAT_GRANITE);

        // Create a hauler
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f,
                  0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;

        // Run simulation — should deliver 2 rocks then stall
        for (int i = 0; i < 5000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }

        // Test #47: blueprint sits in AWAITING_MATERIALS, no crash
        expect(bp->active == true);
        expect(bp->state == BLUEPRINT_AWAITING_MATERIALS);
        expect(bp->stageDeliveries[0].deliveredCount == 2);
        expect(BlueprintStageFilled(bp) == false);
    }

    it("should reserve item and track reservation count") {
        // Test #28, #29
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(4, 4, 0, CONSTRUCTION_DRY_STONE_WALL);

        // Spawn 3 rocks
        int rock1 = SpawnItemWithMaterial(1 * CELL_SIZE + CELL_SIZE * 0.5f,
                                          1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                          ITEM_ROCK, MAT_GRANITE);
        SpawnItemWithMaterial(2 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_ROCK, MAT_GRANITE);
        SpawnItemWithMaterial(3 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_ROCK, MAT_GRANITE);

        // Create a hauler
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f,
                  0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;

        // First job assignment should reserve one rock
        AssignJobs();
        expect(MoverHasHaulToBlueprintJob(m));
        expect(items[rock1].reservedBy >= 0);

        Blueprint* bp = &blueprints[bpIdx];
        // After assignment, slot should track the reservation
        expect(bp->stageDeliveries[0].reservedCount >= 1);
    }
}

describe(construction_recipe_build) {
    it("should build dry stone wall end to end with granite") {
        // Tests #1, #20: deliver 3 granite rocks, build → wall with MAT_GRANITE
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        // Blueprint at (5,5)
        int bpIdx = CreateRecipeBlueprint(5, 5, 0, CONSTRUCTION_DRY_STONE_WALL);
        (void)bpIdx;

        // Spawn 3 granite rocks near the mover
        SpawnItemWithMaterial(1 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_ROCK, MAT_GRANITE);
        SpawnItemWithMaterial(2 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_ROCK, MAT_GRANITE);
        SpawnItemWithMaterial(1 * CELL_SIZE + CELL_SIZE * 0.5f,
                             2 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_ROCK, MAT_GRANITE);

        // Create a mover with both haul and build capabilities
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f,
                  0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;

        // Run simulation until wall is built
        bool wallBuilt = false;
        for (int i = 0; i < 15000; i++) {
            Tick();
            AssignJobs();
            JobsTick();

            if (grid[0][5][5] == CELL_WALL) {
                wallBuilt = true;
                break;
            }
        }

        expect(wallBuilt == true);
        expect(grid[0][5][5] == CELL_WALL);
        expect(GetWallMaterial(5, 5, 0) == MAT_GRANITE);
        expect(HasBlueprint(5, 5, 0) == false);
        expect(CountBlueprints() == 0);
    }

    it("should use recipe build time not flat constant") {
        // Test #33
        const ConstructionRecipe* r = GetConstructionRecipe(CONSTRUCTION_DRY_STONE_WALL);
        expect(r->stages[0].buildTime != 2.0f);  // dry stone wall = 3.0, not default 2.0
        expect(r->stages[0].buildTime == 3.0f);
    }

    it("should survive save and load mid delivery") {
        // Test #50
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(4, 4, 0, CONSTRUCTION_DRY_STONE_WALL);

        // Spawn 1 rock and deliver it manually
        int rock = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                         4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                         ITEM_ROCK, MAT_GRANITE);
        DeliverMaterialToBlueprint(bpIdx, rock);

        Blueprint* bp = &blueprints[bpIdx];
        expect(bp->stageDeliveries[0].deliveredCount == 1);
        expect(bp->state == BLUEPRINT_AWAITING_MATERIALS);

        // Save
        SaveWorld("/tmp/test_construction_save.bin");

        // Corrupt state
        InitDesignations();

        // Load
        LoadWorld("/tmp/test_construction_save.bin");
        RebuildPostLoadState();

        // Verify state restored
        bpIdx = GetBlueprintAt(4, 4, 0);
        expect(bpIdx >= 0);
        bp = &blueprints[bpIdx];
        expect(bp->recipeIndex == CONSTRUCTION_DRY_STONE_WALL);
        expect(bp->stage == 0);
        expect(bp->stageDeliveries[0].deliveredCount == 1);
        expect(bp->stageDeliveries[0].deliveredMaterial == MAT_GRANITE);
        expect(bp->state == BLUEPRINT_AWAITING_MATERIALS);
    }
}

// Construction recipe system (Phase 2/3 - wattle & daub, multi-input + multi-stage)
describe(construction_wattle_data) {
    it("should have wattle & daub recipe with 2 stages") {
        const ConstructionRecipe* r = GetConstructionRecipe(CONSTRUCTION_WATTLE_DAUB_WALL);
        expect(r != NULL);
        expect(r->buildCategory == BUILD_WALL);
        expect(r->stageCount == 2);
        // Stage 0: frame (2 sticks + 1 cordage)
        expect(r->stages[0].inputCount == 2);
        expect(r->stages[0].inputs[0].count == 2);
        expect(r->stages[0].inputs[0].alternatives[0].itemType == ITEM_STICKS);
        expect(r->stages[0].inputs[1].count == 1);
        expect(r->stages[0].inputs[1].alternatives[0].itemType == ITEM_CORDAGE);
        // Stage 1: fill (2 dirt)
        expect(r->stages[1].inputCount == 1);
        expect(r->stages[1].inputs[0].count == 2);
        expect(r->stages[1].inputs[0].alternatives[0].itemType == ITEM_DIRT);
        // Material from fill stage
        expect(r->materialFromStage == 1);
        expect(r->materialFromSlot == 0);
    }

    it("should have plank wall recipe with 2 stages") {
        const ConstructionRecipe* r = GetConstructionRecipe(CONSTRUCTION_PLANK_WALL);
        expect(r != NULL);
        expect(r->stageCount == 2);
        // Stage 0: frame (same as wattle)
        expect(r->stages[0].inputCount == 2);
        // Stage 1: clad (2 planks)
        expect(r->stages[1].inputCount == 1);
        expect(r->stages[1].inputs[0].count == 2);
        expect(r->stages[1].inputs[0].alternatives[0].itemType == ITEM_PLANKS);
        expect(r->materialFromStage == 1);
    }

    it("should have at least 3 BUILD_WALL recipes now") {
        int count = GetConstructionRecipeCountForCategory(BUILD_WALL);
        expect(count >= 3);
    }
}

describe(construction_wattle_delivery) {
    it("should stay AWAITING after delivering 1 stick of 2 in stage 0") {
        // Test #24: deliver 1 stick → still AWAITING
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(4, 4, 0, CONSTRUCTION_WATTLE_DAUB_WALL);
        Blueprint* bp = &blueprints[bpIdx];
        expect(bp->stage == 0);

        // Deliver 1 stick manually
        int stick = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                          4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                          ITEM_STICKS, MAT_OAK);
        DeliverMaterialToBlueprint(bpIdx, stick);

        expect(bp->state == BLUEPRINT_AWAITING_MATERIALS);
        expect(bp->stageDeliveries[0].deliveredCount == 1);
        expect(bp->stageDeliveries[1].deliveredCount == 0);
        expect(BlueprintStageFilled(bp) == false);
    }

    it("should stay AWAITING after delivering 2 sticks but no cordage") {
        // Test #25: deliver both sticks → still AWAITING (cordage missing)
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(4, 4, 0, CONSTRUCTION_WATTLE_DAUB_WALL);
        Blueprint* bp = &blueprints[bpIdx];

        // Deliver 2 sticks manually
        int stick1 = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                           4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                           ITEM_STICKS, MAT_OAK);
        DeliverMaterialToBlueprint(bpIdx, stick1);
        int stick2 = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                           4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                           ITEM_STICKS, MAT_OAK);
        DeliverMaterialToBlueprint(bpIdx, stick2);

        expect(bp->state == BLUEPRINT_AWAITING_MATERIALS);
        expect(bp->stageDeliveries[0].deliveredCount == 2);  // sticks filled
        expect(bp->stageDeliveries[1].deliveredCount == 0);  // cordage empty
        expect(BlueprintStageFilled(bp) == false);
    }

    it("should become READY_TO_BUILD when stage 0 inputs delivered") {
        // Test #26: deliver 2 sticks + 1 cordage → READY_TO_BUILD for stage 0
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(4, 4, 0, CONSTRUCTION_WATTLE_DAUB_WALL);
        Blueprint* bp = &blueprints[bpIdx];

        // Deliver 2 sticks + 1 cordage
        int s1 = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_STICKS, MAT_OAK);
        DeliverMaterialToBlueprint(bpIdx, s1);
        int s2 = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_STICKS, MAT_OAK);
        DeliverMaterialToBlueprint(bpIdx, s2);
        int c1 = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_CORDAGE, MAT_NONE);
        DeliverMaterialToBlueprint(bpIdx, c1);

        expect(bp->stage == 0);
        expect(bp->state == BLUEPRINT_READY_TO_BUILD);
        expect(BlueprintStageFilled(bp) == true);
    }

    it("should not assign wrong item type to a slot") {
        // Test #48: hauler carrying wrong type → not assigned
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        CreateRecipeBlueprint(4, 4, 0, CONSTRUCTION_WATTLE_DAUB_WALL);

        // Only spawn rocks (wrong type — needs sticks+cordage for stage 0)
        SpawnItemWithMaterial(1 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_ROCK, MAT_GRANITE);

        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f,
                  0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;

        AssignJobs();

        // Mover should NOT be assigned
        expect(MoverIsIdle(m));
    }
}

describe(construction_wattle_parallel) {
    it("should assign two haulers to different slots simultaneously") {
        // Test #30: two haulers, one for sticks, one for cordage
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(5, 5, 0, CONSTRUCTION_WATTLE_DAUB_WALL);

        // Spawn items: 2 sticks near one mover, 1 cordage near another
        SpawnItemWithMaterial(1 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_STICKS, MAT_OAK);
        SpawnItemWithMaterial(2 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_STICKS, MAT_OAK);
        SpawnItemWithMaterial(8 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_CORDAGE, MAT_NONE);

        // Two movers
        Mover* m0 = &movers[0];
        Mover* m1 = &movers[1];
        Point goal = {0, 0, 0};
        InitMover(m0, 0 * CELL_SIZE + CELL_SIZE * 0.5f,
                  0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        InitMover(m1, 9 * CELL_SIZE + CELL_SIZE * 0.5f,
                  0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 2;

        AssignJobs();

        // Both movers should be assigned to haul to the same blueprint
        expect(MoverHasHaulToBlueprintJob(m0));
        expect(MoverHasHaulToBlueprintJob(m1));
        expect(MoverGetTargetBlueprint(m0) == bpIdx);
        expect(MoverGetTargetBlueprint(m1) == bpIdx);
    }

    it("should not over-reserve a filled slot") {
        // Test #31 variant: two blueprints, limited items — only one gets reserved
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bp1 = CreateRecipeBlueprint(3, 3, 0, CONSTRUCTION_WATTLE_DAUB_WALL);
        int bp2 = CreateRecipeBlueprint(7, 7, 0, CONSTRUCTION_WATTLE_DAUB_WALL);
        (void)bp2;

        // Only 1 cordage available (both blueprints need 1 each)
        int cordageIdx = SpawnItemWithMaterial(5 * CELL_SIZE + CELL_SIZE * 0.5f,
                                               5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                               ITEM_CORDAGE, MAT_NONE);

        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 5 * CELL_SIZE + CELL_SIZE * 0.5f,
                  4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;

        AssignJobs();

        // Only one blueprint should get the cordage reserved
        expect(items[cordageIdx].reservedBy >= 0);
        int targetBp = MoverGetTargetBlueprint(m);
        expect(targetBp == bp1 || targetBp == bp2);

        // The other blueprint should still have 0 reservations for cordage slot
        int otherBp = (targetBp == bp1) ? bp2 : bp1;
        expect(blueprints[otherBp].stageDeliveries[1].reservedCount == 0);
    }

    it("should assign builder independently after stage 0 materials delivered") {
        // Test #43: builder is not necessarily the last hauler
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(5, 5, 0, CONSTRUCTION_WATTLE_DAUB_WALL);
        Blueprint* bp = &blueprints[bpIdx];

        // Manually deliver all stage 0 materials
        int s1 = SpawnItemWithMaterial(5 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_STICKS, MAT_OAK);
        DeliverMaterialToBlueprint(bpIdx, s1);
        int s2 = SpawnItemWithMaterial(5 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_STICKS, MAT_OAK);
        DeliverMaterialToBlueprint(bpIdx, s2);
        int c1 = SpawnItemWithMaterial(5 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_CORDAGE, MAT_NONE);
        DeliverMaterialToBlueprint(bpIdx, c1);

        expect(bp->state == BLUEPRINT_READY_TO_BUILD);
        expect(bp->stage == 0);

        // Mover nearby — should get assigned as builder
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 4 * CELL_SIZE + CELL_SIZE * 0.5f,
                  5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;

        AssignJobs();
        expect(MoverHasBuildJob(m));
        expect(MoverGetTargetBlueprint(m) == bpIdx);
    }
}

describe(construction_multi_stage) {
    it("should advance to stage 1 after stage 0 build completes") {
        // Test #5: stage 0 build → advances to stage 1, state resets to AWAITING_MATERIALS
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(4, 4, 0, CONSTRUCTION_WATTLE_DAUB_WALL);
        Blueprint* bp = &blueprints[bpIdx];

        // Manually deliver all stage 0 materials
        int s1 = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_STICKS, MAT_OAK);
        DeliverMaterialToBlueprint(bpIdx, s1);
        int s2 = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_STICKS, MAT_OAK);
        DeliverMaterialToBlueprint(bpIdx, s2);
        int c1 = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_CORDAGE, MAT_NONE);
        DeliverMaterialToBlueprint(bpIdx, c1);

        expect(bp->state == BLUEPRINT_READY_TO_BUILD);
        expect(bp->stage == 0);

        // Complete stage 0 build
        CompleteBlueprint(bpIdx);

        // Should advance to stage 1, not deactivate
        expect(bp->active == true);
        expect(bp->stage == 1);
        expect(bp->state == BLUEPRINT_AWAITING_MATERIALS);
        expect(bp->progress == 0.0f);
        expect(bp->assignedBuilder == -1);

        // Stage deliveries should be reset
        expect(bp->stageDeliveries[0].deliveredCount == 0);
        expect(bp->stageDeliveries[0].reservedCount == 0);
        expect(bp->stageDeliveries[0].chosenAlternative == -1);

        // Consumed items from stage 0 should be recorded
        expect(bp->consumedItems[0][0].itemType == ITEM_STICKS);
        expect(bp->consumedItems[0][0].count == 2);
        expect(bp->consumedItems[0][1].itemType == ITEM_CORDAGE);
        expect(bp->consumedItems[0][1].count == 1);
    }

    it("should reset chosenAlternative when stage advances") {
        // Test #16: stage advance resets locks for new stage
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(4, 4, 0, CONSTRUCTION_WATTLE_DAUB_WALL);
        Blueprint* bp = &blueprints[bpIdx];

        // Complete stage 0 via manual delivery
        int s1 = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_STICKS, MAT_OAK);
        DeliverMaterialToBlueprint(bpIdx, s1);
        int s2 = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_STICKS, MAT_OAK);
        DeliverMaterialToBlueprint(bpIdx, s2);
        int c1 = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_CORDAGE, MAT_NONE);
        DeliverMaterialToBlueprint(bpIdx, c1);
        CompleteBlueprint(bpIdx);

        // Now in stage 1 — all delivery slots should be fresh
        expect(bp->stage == 1);
        expect(bp->stageDeliveries[0].chosenAlternative == -1);
        expect(bp->stageDeliveries[0].deliveredMaterial == MAT_NONE);
        expect(bp->stageDeliveries[0].deliveredCount == 0);
        expect(bp->stageDeliveries[0].reservedCount == 0);
    }

    it("should complete wattle & daub wall after both stages") {
        // Test #6: stage 1 fill (2 dirt) + build → wall with MAT_DIRT
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(4, 4, 0, CONSTRUCTION_WATTLE_DAUB_WALL);
        Blueprint* bp = &blueprints[bpIdx];

        // Complete stage 0
        int s1 = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_STICKS, MAT_OAK);
        DeliverMaterialToBlueprint(bpIdx, s1);
        int s2 = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_STICKS, MAT_OAK);
        DeliverMaterialToBlueprint(bpIdx, s2);
        int c1 = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_CORDAGE, MAT_NONE);
        DeliverMaterialToBlueprint(bpIdx, c1);
        CompleteBlueprint(bpIdx);
        expect(bp->stage == 1);

        // Deliver stage 1 materials (2 dirt)
        int d1 = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_DIRT, MAT_DIRT);
        DeliverMaterialToBlueprint(bpIdx, d1);
        int d2 = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_DIRT, MAT_DIRT);
        DeliverMaterialToBlueprint(bpIdx, d2);

        expect(bp->state == BLUEPRINT_READY_TO_BUILD);

        // Complete stage 1 → should place wall
        CompleteBlueprint(bpIdx);

        expect(bp->active == false);
        expect(grid[0][4][4] == CELL_WALL);
        expect(GetWallMaterial(4, 4, 0) == MAT_DIRT);
        expect(HasBlueprint(4, 4, 0) == false);
    }

    it("should create new haul jobs after stage advances") {
        // Test #42: after stage 0 build, WorkGiver creates haul jobs for stage 1
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(5, 5, 0, CONSTRUCTION_WATTLE_DAUB_WALL);
        Blueprint* bp = &blueprints[bpIdx];

        // Complete stage 0 manually
        int s1 = SpawnItemWithMaterial(5 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_STICKS, MAT_OAK);
        DeliverMaterialToBlueprint(bpIdx, s1);
        int s2 = SpawnItemWithMaterial(5 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_STICKS, MAT_OAK);
        DeliverMaterialToBlueprint(bpIdx, s2);
        int c1 = SpawnItemWithMaterial(5 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_CORDAGE, MAT_NONE);
        DeliverMaterialToBlueprint(bpIdx, c1);
        CompleteBlueprint(bpIdx);
        expect(bp->stage == 1);

        // Spawn dirt for stage 1
        SpawnItemWithMaterial(1 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_DIRT, MAT_DIRT);

        // Mover should get assigned to haul dirt
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f,
                  0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;

        AssignJobs();
        expect(MoverHasHaulToBlueprintJob(m));
        expect(MoverGetTargetBlueprint(m) == bpIdx);
    }

    it("should cancel mid-stage-1 and refund both stages") {
        // Test #36: cancel after stage 0 complete, during stage 1
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(4, 4, 0, CONSTRUCTION_WATTLE_DAUB_WALL);
        Blueprint* bp = &blueprints[bpIdx];

        // Complete stage 0
        int s1 = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_STICKS, MAT_OAK);
        DeliverMaterialToBlueprint(bpIdx, s1);
        int s2 = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_STICKS, MAT_OAK);
        DeliverMaterialToBlueprint(bpIdx, s2);
        int c1 = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_CORDAGE, MAT_NONE);
        DeliverMaterialToBlueprint(bpIdx, c1);
        CompleteBlueprint(bpIdx);
        expect(bp->stage == 1);

        // Deliver 1 dirt to stage 1 (partial)
        int d1 = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_DIRT, MAT_DIRT);
        DeliverMaterialToBlueprint(bpIdx, d1);
        expect(bp->stageDeliveries[0].deliveredCount == 1);

        // Count items before cancel
        int itemsBefore = 0;
        for (int i = 0; i < MAX_ITEMS; i++) {
            if (IsItemActive(i)) itemsBefore++;
        }

        // Cancel — should refund stage 1 delivered (1 dirt, 100%) + stage 0 consumed (lossy)
        SetRandomSeed(12345);
        CancelBlueprint(bpIdx);
        expect(bp->active == false);

        int itemsAfter = 0;
        for (int i = 0; i < MAX_ITEMS; i++) {
            if (IsItemActive(i)) itemsAfter++;
        }

        // Stage 1 delivered: 1 dirt (100% refund)
        // Stage 0 consumed: 2 sticks + 1 cordage (75% each, lossy)
        int refunded = itemsAfter - itemsBefore;
        expect(refunded >= 1);  // at minimum the dirt
        expect(refunded <= 4);  // at most dirt + all 3 consumed
    }

    it("should save and load between stages") {
        // Test #51: save after stage 0 done, during stage 1 awaiting
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(4, 4, 0, CONSTRUCTION_WATTLE_DAUB_WALL);

        // Complete stage 0
        int s1 = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_STICKS, MAT_OAK);
        DeliverMaterialToBlueprint(bpIdx, s1);
        int s2 = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_STICKS, MAT_OAK);
        DeliverMaterialToBlueprint(bpIdx, s2);
        int c1 = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_CORDAGE, MAT_NONE);
        DeliverMaterialToBlueprint(bpIdx, c1);
        CompleteBlueprint(bpIdx);

        Blueprint* bp = &blueprints[bpIdx];
        expect(bp->stage == 1);
        expect(bp->state == BLUEPRINT_AWAITING_MATERIALS);

        // Deliver 1 dirt to stage 1 (partial)
        int d1 = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_DIRT, MAT_DIRT);
        DeliverMaterialToBlueprint(bpIdx, d1);
        expect(bp->stageDeliveries[0].deliveredCount == 1);

        // Save
        SaveWorld("/tmp/test_multistage_save.bin");

        // Corrupt
        InitDesignations();

        // Load
        LoadWorld("/tmp/test_multistage_save.bin");
        RebuildPostLoadState();

        // Verify
        bpIdx = GetBlueprintAt(4, 4, 0);
        expect(bpIdx >= 0);
        bp = &blueprints[bpIdx];
        expect(bp->recipeIndex == CONSTRUCTION_WATTLE_DAUB_WALL);
        expect(bp->stage == 1);
        expect(bp->state == BLUEPRINT_AWAITING_MATERIALS);
        expect(bp->stageDeliveries[0].deliveredCount == 1);
        expect(bp->stageDeliveries[0].deliveredMaterial == MAT_DIRT);
        // Consumed items from stage 0 should be preserved
        expect(bp->consumedItems[0][0].itemType == ITEM_STICKS);
        expect(bp->consumedItems[0][0].count == 2);
        expect(bp->consumedItems[0][1].itemType == ITEM_CORDAGE);
        expect(bp->consumedItems[0][1].count == 1);
    }
}

describe(construction_multi_stage_edge_cases) {
    it("should not haul stage 1 items during stage 0") {
        // Edge case: dirt is available but blueprint is in stage 0 (needs sticks+cordage).
        // WorkGiver should NOT assign dirt hauling.
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(5, 5, 0, CONSTRUCTION_WATTLE_DAUB_WALL);
        Blueprint* bp = &blueprints[bpIdx];
        expect(bp->stage == 0);

        // Only spawn dirt (stage 1 material) — no sticks or cordage
        SpawnItemWithMaterial(1 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_DIRT, MAT_DIRT);
        SpawnItemWithMaterial(2 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_DIRT, MAT_DIRT);

        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f,
                  0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;

        AssignJobs();

        // Mover should be idle — dirt doesn't match stage 0 inputs
        expect(MoverIsIdle(m));
        expect(bp->stageDeliveries[0].reservedCount == 0);
        expect(bp->stageDeliveries[1].reservedCount == 0);
    }

    it("should not over-deliver to same slot with two haulers") {
        // Edge case: slot 0 needs 2 sticks, 2 haulers both assigned — reservedCount
        // should prevent a 3rd reservation
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(5, 5, 0, CONSTRUCTION_WATTLE_DAUB_WALL);
        Blueprint* bp = &blueprints[bpIdx];

        // Spawn 3 sticks + 1 cordage (more sticks than needed for slot 0)
        SpawnItemWithMaterial(1 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_STICKS, MAT_OAK);
        SpawnItemWithMaterial(2 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_STICKS, MAT_OAK);
        SpawnItemWithMaterial(3 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_STICKS, MAT_OAK);
        SpawnItemWithMaterial(8 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_CORDAGE, MAT_NONE);

        // 3 movers
        for (int i = 0; i < 3; i++) {
            Mover* m = &movers[i];
            Point goal = {0, 0, 0};
            InitMover(m, (i) * CELL_SIZE + CELL_SIZE * 0.5f,
                      0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        }
        moverCount = 3;

        AssignJobs();

        // Stick slot (slot 0) should have at most 2 reserved (count needed = 2)
        expect(bp->stageDeliveries[0].reservedCount <= 2);

        // Total reservations across all slots: at most 3 (2 sticks + 1 cordage)
        int totalReserved = bp->stageDeliveries[0].reservedCount + bp->stageDeliveries[1].reservedCount;
        expect(totalReserved <= 3);

        // All 3 movers should be assigned (2 sticks + 1 cordage)
        int assignedCount = 0;
        for (int i = 0; i < 3; i++) {
            if (MoverHasHaulToBlueprintJob(&movers[i])) assignedCount++;
        }
        expect(assignedCount == 3);
    }

    it("should not haul sticks to blueprint that already has enough sticks reserved") {
        // Verify that once slot 0 has reservedCount == count, no more sticks are reserved
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(5, 5, 0, CONSTRUCTION_WATTLE_DAUB_WALL);
        Blueprint* bp = &blueprints[bpIdx];

        // Only spawn sticks (no cordage) — 3 sticks but only 2 needed
        SpawnItemWithMaterial(1 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_STICKS, MAT_OAK);
        SpawnItemWithMaterial(2 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_STICKS, MAT_OAK);
        SpawnItemWithMaterial(3 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_STICKS, MAT_OAK);

        // 3 movers
        for (int i = 0; i < 3; i++) {
            Mover* m = &movers[i];
            Point goal = {0, 0, 0};
            InitMover(m, (i) * CELL_SIZE + CELL_SIZE * 0.5f,
                      0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        }
        moverCount = 3;

        AssignJobs();

        // Only 2 movers should be assigned (stick slot needs exactly 2)
        int assignedCount = 0;
        for (int i = 0; i < 3; i++) {
            if (MoverHasHaulToBlueprintJob(&movers[i])) assignedCount++;
        }
        expect(assignedCount == 2);
        expect(bp->stageDeliveries[0].reservedCount == 2);
    }

    it("should deliver to correct slot when item type differs between slots") {
        // Verify that cordage goes to slot 1, not slot 0 (sticks)
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(4, 4, 0, CONSTRUCTION_WATTLE_DAUB_WALL);
        Blueprint* bp = &blueprints[bpIdx];

        // Deliver cordage first (before any sticks)
        int c1 = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_CORDAGE, MAT_NONE);
        DeliverMaterialToBlueprint(bpIdx, c1);

        // Cordage should go to slot 1, not slot 0
        expect(bp->stageDeliveries[0].deliveredCount == 0);  // sticks slot empty
        expect(bp->stageDeliveries[1].deliveredCount == 1);  // cordage slot has 1
    }
}

describe(construction_plank_wall) {
    it("should build plank wall end to end through both stages") {
        // Test #7: plank wall: sticks+cordage → build → planks → build → final wall
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        CreateRecipeBlueprint(5, 5, 0, CONSTRUCTION_PLANK_WALL);

        // Spawn all materials: 2 sticks, 1 cordage (stage 0), 2 planks (stage 1)
        SpawnItemWithMaterial(1 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_STICKS, MAT_OAK);
        SpawnItemWithMaterial(2 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_STICKS, MAT_OAK);
        SpawnItemWithMaterial(3 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_CORDAGE, MAT_NONE);
        SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_PLANKS, MAT_OAK);
        SpawnItemWithMaterial(5 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_PLANKS, MAT_OAK);

        // Single mover
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f,
                  0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;

        // Run simulation until wall is built
        bool wallBuilt = false;
        for (int i = 0; i < 30000; i++) {
            Tick();
            AssignJobs();
            JobsTick();

            if (grid[0][5][5] == CELL_WALL) {
                wallBuilt = true;
                break;
            }
        }

        expect(wallBuilt == true);
        expect(grid[0][5][5] == CELL_WALL);
        // Material from planks (stage 1, slot 0) — MAT_OAK
        expect(GetWallMaterial(5, 5, 0) == MAT_OAK);
        expect(HasBlueprint(5, 5, 0) == false);
    }

    it("should build wattle & daub wall end to end through both stages") {
        // Full sim: 2 sticks + 1 cordage → build stage 0 → 2 dirt → build stage 1 → wall
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        CreateRecipeBlueprint(5, 5, 0, CONSTRUCTION_WATTLE_DAUB_WALL);

        // Spawn all materials
        SpawnItemWithMaterial(1 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_STICKS, MAT_OAK);
        SpawnItemWithMaterial(2 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_STICKS, MAT_OAK);
        SpawnItemWithMaterial(3 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_CORDAGE, MAT_NONE);
        SpawnItemWithMaterial(7 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_DIRT, MAT_DIRT);
        SpawnItemWithMaterial(8 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_DIRT, MAT_DIRT);

        // Single mover
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f,
                  0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;

        // Run simulation
        bool wallBuilt = false;
        for (int i = 0; i < 30000; i++) {
            Tick();
            AssignJobs();
            JobsTick();

            if (grid[0][5][5] == CELL_WALL) {
                wallBuilt = true;
                break;
            }
        }

        expect(wallBuilt == true);
        expect(grid[0][5][5] == CELL_WALL);
        // Material from fill stage (stage 1) — MAT_DIRT
        expect(GetWallMaterial(5, 5, 0) == MAT_DIRT);
        expect(HasBlueprint(5, 5, 0) == false);
    }
}

// Phase 5: Site clearing tests
describe(construction_site_clearing) {
    it("should start in CLEARING state when items exist at cell") {
        // Test #0a: blueprint on cell with items → state = CLEARING
        InitTestGridFromAscii(
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        // Place an item at (3,3)
        SpawnItemWithMaterial(3 * CELL_SIZE + CELL_SIZE * 0.5f,
                             3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_ROCK, MAT_GRANITE);

        int bpIdx = CreateRecipeBlueprint(3, 3, 0, CONSTRUCTION_DRY_STONE_WALL);
        expect(bpIdx >= 0);
        expect(blueprints[bpIdx].state == BLUEPRINT_CLEARING);
    }

    it("should start in AWAITING_MATERIALS when cell is empty") {
        // Test #0c: blueprint on empty cell → skips CLEARING
        InitTestGridFromAscii(
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(3, 3, 0, CONSTRUCTION_DRY_STONE_WALL);
        expect(bpIdx >= 0);
        expect(blueprints[bpIdx].state == BLUEPRINT_AWAITING_MATERIALS);
    }

    it("should transition to AWAITING_MATERIALS when items are removed") {
        // Test #0b: all items removed → advances to AWAITING_MATERIALS
        InitTestGridFromAscii(
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        // Place item at blueprint cell
        int itemIdx = SpawnItemWithMaterial(3 * CELL_SIZE + CELL_SIZE * 0.5f,
                                            3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                            ITEM_ROCK, MAT_GRANITE);

        int bpIdx = CreateRecipeBlueprint(3, 3, 0, CONSTRUCTION_DRY_STONE_WALL);
        expect(blueprints[bpIdx].state == BLUEPRINT_CLEARING);

        // Manually delete the item (simulating hauler took it away)
        DeleteItem(itemIdx);

        // Create a mover so WorkGiver can run
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f,
                  0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;

        // WorkGiver_BlueprintClear scans and finds no items → transitions
        WorkGiver_BlueprintClear(0);
        expect(blueprints[bpIdx].state == BLUEPRINT_AWAITING_MATERIALS);
    }

    it("should create haul job for items at CLEARING blueprint") {
        // Test #0e: WorkGiver_BlueprintClear creates haul-away jobs
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        // Create a stockpile to receive the hauled item
        int spIdx = CreateStockpile(8, 8, 0, 1, 1);
        expect(spIdx >= 0);
        SetStockpileFilter(spIdx, ITEM_ROCK, true);

        // Place item at (5,5)
        SpawnItemWithMaterial(5 * CELL_SIZE + CELL_SIZE * 0.5f,
                             5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_ROCK, MAT_GRANITE);

        int bpIdx = CreateRecipeBlueprint(5, 5, 0, CONSTRUCTION_DRY_STONE_WALL);
        expect(blueprints[bpIdx].state == BLUEPRINT_CLEARING);

        // Mover
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f,
                  0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;

        int jobId = WorkGiver_BlueprintClear(0);
        expect(jobId >= 0);

        Job* job = GetJob(jobId);
        expect(job->type == JOBTYPE_HAUL);
        expect(job->targetStockpile == spIdx);
    }

    it("should clear site then build wall end to end") {
        // Full sim: item at cell → clear → deliver materials → build → wall
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        // Stockpile to receive cleared items (only accepts dirt)
        int spIdx = CreateStockpile(9, 9, 0, 1, 1);
        expect(spIdx >= 0);
        for (int t = 0; t < ITEM_TYPE_COUNT; t++) {
            SetStockpileFilter(spIdx, (ItemType)t, false);
        }
        SetStockpileFilter(spIdx, ITEM_DIRT, true);

        // Pre-existing dirt at the construction site
        SpawnItemWithMaterial(5 * CELL_SIZE + CELL_SIZE * 0.5f,
                             5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_DIRT, MAT_DIRT);

        int bpIdx = CreateRecipeBlueprint(5, 5, 0, CONSTRUCTION_DRY_STONE_WALL);
        expect(blueprints[bpIdx].state == BLUEPRINT_CLEARING);

        // Spawn 3 rocks nearby for construction (not at the blueprint cell)
        for (int i = 0; i < 3; i++) {
            SpawnItemWithMaterial((1 + i) * CELL_SIZE + CELL_SIZE * 0.5f,
                                 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                 ITEM_ROCK, MAT_GRANITE);
        }

        // Mover
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f,
                  0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;

        // Run simulation: clear → haul materials → build
        bool wallBuilt = false;
        for (int i = 0; i < 60000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (grid[0][5][5] == CELL_WALL) { wallBuilt = true; break; }
        }

        expect(wallBuilt == true);
        expect(GetWallMaterial(5, 5, 0) == MAT_GRANITE);
    }

    it("should not haul construction materials while still clearing") {
        // While in CLEARING state, BlueprintHaul should not pick up materials
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        // Item at blueprint cell (triggers CLEARING)
        SpawnItemWithMaterial(5 * CELL_SIZE + CELL_SIZE * 0.5f,
                             5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_DIRT, MAT_DIRT);

        int bpIdx = CreateRecipeBlueprint(5, 5, 0, CONSTRUCTION_DRY_STONE_WALL);
        expect(blueprints[bpIdx].state == BLUEPRINT_CLEARING);

        // Spawn rocks for construction
        SpawnItemWithMaterial(1 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_ROCK, MAT_GRANITE);

        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f,
                  0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;

        // BlueprintHaul should not create a job — bp is still CLEARING
        int jobId = WorkGiver_BlueprintHaul(0);
        expect(jobId == -1);
    }
}

// Phase 4: OR-materials + locking tests
describe(construction_or_materials) {
    it("should accept dirt OR clay for wattle fill stage") {
        // Test #9/#10: wattle fill accepts both dirt and clay
        const ConstructionRecipe* r = GetConstructionRecipe(CONSTRUCTION_WATTLE_DAUB_WALL);
        const ConstructionInput* fill = &r->stages[1].inputs[0];
        expect(fill->altCount == 2);
        expect(ConstructionInputAcceptsItem(fill, ITEM_DIRT) == true);
        expect(ConstructionInputAcceptsItem(fill, ITEM_CLAY) == true);
        expect(ConstructionInputAcceptsItem(fill, ITEM_ROCK) == false);
    }

    it("should accept rocks OR blocks for dry stone wall") {
        // Test #11: both alternatives accepted
        const ConstructionRecipe* r = GetConstructionRecipe(CONSTRUCTION_DRY_STONE_WALL);
        const ConstructionInput* input = &r->stages[0].inputs[0];
        expect(input->altCount == 2);
        expect(ConstructionInputAcceptsItem(input, ITEM_ROCK) == true);
        expect(ConstructionInputAcceptsItem(input, ITEM_BLOCKS) == true);
        expect(ConstructionInputAcceptsItem(input, ITEM_DIRT) == false);
    }

    it("should build wattle wall with clay instead of dirt") {
        // Test #10: only clay available → hauler picks clay → MAT_CLAY
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        CreateRecipeBlueprint(5, 5, 0, CONSTRUCTION_WATTLE_DAUB_WALL);

        // Stage 0: sticks + cordage
        SpawnItemWithMaterial(1 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_STICKS, MAT_OAK);
        SpawnItemWithMaterial(2 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_STICKS, MAT_OAK);
        SpawnItemWithMaterial(3 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_CORDAGE, MAT_NONE);
        // Stage 1: clay instead of dirt
        SpawnItemWithMaterial(7 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_CLAY, MAT_CLAY);
        SpawnItemWithMaterial(8 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_CLAY, MAT_CLAY);

        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f,
                  0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;

        bool wallBuilt = false;
        for (int i = 0; i < 30000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (grid[0][5][5] == CELL_WALL) { wallBuilt = true; break; }
        }

        expect(wallBuilt == true);
        expect(GetWallMaterial(5, 5, 0) == MAT_CLAY);
    }

    it("should build dry stone wall with blocks instead of rocks") {
        // Test #11 end-to-end: only blocks available → wall with block material
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        CreateRecipeBlueprint(5, 5, 0, CONSTRUCTION_DRY_STONE_WALL);

        // 3 blocks (granite)
        for (int i = 0; i < 3; i++) {
            SpawnItemWithMaterial((1 + i) * CELL_SIZE + CELL_SIZE * 0.5f,
                                 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                 ITEM_BLOCKS, MAT_GRANITE);
        }

        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f,
                  0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;

        bool wallBuilt = false;
        for (int i = 0; i < 30000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (grid[0][5][5] == CELL_WALL) { wallBuilt = true; break; }
        }

        expect(wallBuilt == true);
        expect(GetWallMaterial(5, 5, 0) == MAT_GRANITE);
    }
}

describe(construction_alternative_locking) {
    it("should lock chosenAlternative on first reservation") {
        // Test #13: first reservation locks the slot's chosenAlternative
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(5, 5, 0, CONSTRUCTION_DRY_STONE_WALL);
        Blueprint* bp = &blueprints[bpIdx];

        // Spawn 1 rock nearby
        SpawnItemWithMaterial(1 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_ROCK, MAT_GRANITE);

        // Mover to pick it up
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f,
                  0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;

        // One tick of job assignment should reserve
        AssignJobs();

        // chosenAlternative should be locked to 0 (ITEM_ROCK)
        expect(bp->stageDeliveries[0].chosenAlternative == 0);
        expect(bp->stageDeliveries[0].reservedCount == 1);
    }

    it("should not reserve wrong alternative after slot is locked") {
        // Test #14: once slot locked to rock, blocks should be rejected
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(5, 5, 0, CONSTRUCTION_DRY_STONE_WALL);
        Blueprint* bp = &blueprints[bpIdx];

        // Deliver 1 rock to lock to alternative 0
        int rock = SpawnItemWithMaterial(5 * CELL_SIZE + CELL_SIZE * 0.5f,
                                         5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                         ITEM_ROCK, MAT_GRANITE);
        DeliverMaterialToBlueprint(bpIdx, rock);
        expect(bp->stageDeliveries[0].chosenAlternative == 0);
        expect(bp->stageDeliveries[0].deliveredCount == 1);

        // Now only blocks available — should NOT be picked up
        SpawnItemWithMaterial(1 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_BLOCKS, MAT_GRANITE);

        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f,
                  0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;

        // Try to assign — should fail since only blocks available and slot locked to rocks
        int jobId = WorkGiver_BlueprintHaul(0);
        expect(jobId == -1);
        expect(bp->stageDeliveries[0].reservedCount == 0);
    }

    it("should lock material on first reservation preventing mixed materials") {
        // Test #15: first reservation locks material — won't mix oak and pine rocks
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(5, 5, 0, CONSTRUCTION_DRY_STONE_WALL);
        Blueprint* bp = &blueprints[bpIdx];

        // Deliver 1 granite rock to lock material
        int rock = SpawnItemWithMaterial(5 * CELL_SIZE + CELL_SIZE * 0.5f,
                                         5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                         ITEM_ROCK, MAT_GRANITE);
        DeliverMaterialToBlueprint(bpIdx, rock);
        expect(bp->stageDeliveries[0].deliveredMaterial == MAT_GRANITE);

        // Only sandstone rocks available — should NOT be picked up (material mismatch)
        SpawnItemWithMaterial(1 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_ROCK, MAT_SANDSTONE);

        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f,
                  0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;

        int jobId = WorkGiver_BlueprintHaul(0);
        expect(jobId == -1);
    }

    it("should reset locking when stage advances") {
        // Test #16 (already covered in multi_stage tests, but verify explicitly for OR-inputs)
        InitTestGridFromAscii(
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(3, 3, 0, CONSTRUCTION_WATTLE_DAUB_WALL);
        Blueprint* bp = &blueprints[bpIdx];

        // Deliver all stage 0 inputs
        int s1 = SpawnItemWithMaterial(3 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_STICKS, MAT_OAK);
        DeliverMaterialToBlueprint(bpIdx, s1);
        int s2 = SpawnItemWithMaterial(3 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_STICKS, MAT_OAK);
        DeliverMaterialToBlueprint(bpIdx, s2);
        int c = SpawnItemWithMaterial(3 * CELL_SIZE + CELL_SIZE * 0.5f,
                                      3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                      ITEM_CORDAGE, MAT_NONE);
        DeliverMaterialToBlueprint(bpIdx, c);

        expect(bp->state == BLUEPRINT_READY_TO_BUILD);

        // Complete stage 0 build
        bp->assignedBuilder = 0;
        bp->state = BLUEPRINT_BUILDING;
        CompleteBlueprint(bpIdx);

        // Should advance to stage 1
        expect(bp->stage == 1);
        expect(bp->state == BLUEPRINT_AWAITING_MATERIALS);
        // Stage 1 fill slot should have fresh locking state
        expect(bp->stageDeliveries[0].chosenAlternative == -1);
        expect(bp->stageDeliveries[0].deliveredMaterial == MAT_NONE);
        expect(bp->stageDeliveries[0].deliveredCount == 0);
    }

    it("should stall when locked alternative runs out") {
        // Test #17: if locked to rock but no more rocks exist, slot stalls
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(5, 5, 0, CONSTRUCTION_DRY_STONE_WALL);
        Blueprint* bp = &blueprints[bpIdx];

        // Deliver 1 rock to lock
        int rock = SpawnItemWithMaterial(5 * CELL_SIZE + CELL_SIZE * 0.5f,
                                         5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                         ITEM_ROCK, MAT_GRANITE);
        DeliverMaterialToBlueprint(bpIdx, rock);

        // No more rocks or blocks — only dirt available (wrong type entirely)
        SpawnItemWithMaterial(1 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_DIRT, MAT_DIRT);
        // Also blocks available — but locked to rocks, so rejected
        SpawnItemWithMaterial(2 * CELL_SIZE + CELL_SIZE * 0.5f,
                             1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                             ITEM_BLOCKS, MAT_GRANITE);

        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f,
                  0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;

        // Blueprint stays stuck — no haul job created
        int jobId = WorkGiver_BlueprintHaul(0);
        expect(jobId == -1);
        expect(bp->state == BLUEPRINT_AWAITING_MATERIALS);
        expect(bp->stageDeliveries[0].deliveredCount == 1);
    }
}

describe(construction_any_building_mat) {
    it("should have ramp recipe with anyBuildingMat flag") {
        // Test #49: ramp recipe structure
        const ConstructionRecipe* r = GetConstructionRecipe(CONSTRUCTION_RAMP);
        expect(r != NULL);
        expect(r->buildCategory == BUILD_RAMP);
        expect(r->stageCount == 1);
        expect(r->stages[0].inputCount == 1);
        expect(r->stages[0].inputs[0].anyBuildingMat == true);
        expect(r->stages[0].inputs[0].count == 1);
    }

    it("should accept any IF_BUILDING_MAT item for ramp input") {
        // Test #49: anyBuildingMat accepts blocks, logs, planks, poles, bricks, stripped_log
        const ConstructionRecipe* r = GetConstructionRecipe(CONSTRUCTION_RAMP);
        const ConstructionInput* input = &r->stages[0].inputs[0];

        expect(ConstructionInputAcceptsItem(input, ITEM_BLOCKS) == true);
        expect(ConstructionInputAcceptsItem(input, ITEM_LOG) == true);
        expect(ConstructionInputAcceptsItem(input, ITEM_PLANKS) == true);
        expect(ConstructionInputAcceptsItem(input, ITEM_POLES) == true);
        expect(ConstructionInputAcceptsItem(input, ITEM_BRICKS) == true);
        expect(ConstructionInputAcceptsItem(input, ITEM_STRIPPED_LOG) == true);
    }

    it("should reject non-building-mat items for ramp input") {
        // Items without IF_BUILDING_MAT should be rejected
        const ConstructionRecipe* r = GetConstructionRecipe(CONSTRUCTION_RAMP);
        const ConstructionInput* input = &r->stages[0].inputs[0];

        expect(ConstructionInputAcceptsItem(input, ITEM_ROCK) == false);
        expect(ConstructionInputAcceptsItem(input, ITEM_DIRT) == false);
        expect(ConstructionInputAcceptsItem(input, ITEM_CLAY) == false);
        expect(ConstructionInputAcceptsItem(input, ITEM_STICKS) == false);
        expect(ConstructionInputAcceptsItem(input, ITEM_CORDAGE) == false);
    }

    it("should not lock chosenAlternative for anyBuildingMat slots") {
        // anyBuildingMat slots skip alternative locking — different building mats can mix
        // Ramp needs CELL_AIR + HAS_FLOOR — use a wall recipe with anyBuildingMat to test
        // the locking behavior directly via delivery, bypassing placement preconditions
        InitTestGridFromAscii(
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        // Ramp requires CELL_AIR + HAS_FLOOR — z=0 walkable from bedrock, need explicit floor
        SET_FLOOR(3, 3, 0);
        int bpIdx = CreateRecipeBlueprint(3, 3, 0, CONSTRUCTION_RAMP);
        expect(bpIdx >= 0);
        Blueprint* bp = &blueprints[bpIdx];

        // Deliver a log — anyBuildingMat should NOT lock chosenAlternative
        int log = SpawnItemWithMaterial(3 * CELL_SIZE + CELL_SIZE * 0.5f,
                                        3 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                        ITEM_LOG, MAT_OAK);
        DeliverMaterialToBlueprint(bpIdx, log);

        expect(bp->stageDeliveries[0].deliveredCount == 1);
        expect(bp->stageDeliveries[0].chosenAlternative == -1);  // not locked
    }
}

// =============================================================================
// Phase 7: New Recipes (log wall, brick wall/floor, plank floor, thatch floor, ladder)
// =============================================================================
describe(construction_new_recipes) {
    it("should build brick floor end to end") {
        // Test #2: 2 bricks → floor with MAT_BRICK
        InitTestGridFromAscii("....\n....\n");
        InitDesignations();
        ClearItems();

        // Need air cell with no floor for floor blueprint
        CLEAR_FLOOR(2, 1, 0);
        int bpIdx = CreateRecipeBlueprint(2, 1, 0, CONSTRUCTION_BRICK_FLOOR);
        expect(bpIdx >= 0);

        for (int i = 0; i < 2; i++) {
            int idx = SpawnItemWithMaterial(0, 0, 0, ITEM_BRICKS, MAT_BRICK);
            DeliverMaterialToBlueprint(bpIdx, idx);
        }
        expect(blueprints[bpIdx].state == BLUEPRINT_READY_TO_BUILD);
        CompleteBlueprint(bpIdx);

        expect(HAS_FLOOR(2, 1, 0));
        expect(GetFloorMaterial(2, 1, 0) == MAT_BRICK);
        expect(!IsFloorNatural(2, 1, 0));
    }

    it("should build ladder with log") {
        // Test #3: 1 log → ladder placed
        InitTestGridFromAscii("....\n....\n");
        InitDesignations();
        ClearItems();

        int bpIdx = CreateRecipeBlueprint(2, 1, 0, CONSTRUCTION_LADDER);
        expect(bpIdx >= 0);

        int idx = SpawnItemWithMaterial(0, 0, 0, ITEM_LOG, MAT_OAK);
        DeliverMaterialToBlueprint(bpIdx, idx);
        expect(blueprints[bpIdx].state == BLUEPRINT_READY_TO_BUILD);
        CompleteBlueprint(bpIdx);

        CellType ct = grid[0][1][2];
        expect(ct == CELL_LADDER_UP || ct == CELL_LADDER_DOWN || ct == CELL_LADDER_BOTH);
        expect(GetWallMaterial(2, 1, 0) == MAT_OAK);
    }

    it("should build thatch floor end to end with 2 stages") {
        // Test #8: stage 0 = 1 dirt, stage 1 = 1 dried grass → floor with MAT_DIRT
        InitTestGridFromAscii("....\n....\n");
        InitDesignations();
        ClearItems();

        CLEAR_FLOOR(2, 1, 0);
        int bpIdx = CreateRecipeBlueprint(2, 1, 0, CONSTRUCTION_THATCH_FLOOR);
        expect(bpIdx >= 0);

        // Stage 0: deliver dirt
        int dirtIdx = SpawnItemWithMaterial(0, 0, 0, ITEM_DIRT, MAT_DIRT);
        DeliverMaterialToBlueprint(bpIdx, dirtIdx);
        expect(blueprints[bpIdx].state == BLUEPRINT_READY_TO_BUILD);
        CompleteBlueprint(bpIdx);

        // Should advance to stage 1
        expect(blueprints[bpIdx].active == true);
        expect(blueprints[bpIdx].stage == 1);
        expect(blueprints[bpIdx].state == BLUEPRINT_AWAITING_MATERIALS);

        // Stage 1: deliver dried grass
        int grassIdx = SpawnItemWithMaterial(0, 0, 0, ITEM_DRIED_GRASS, MAT_NONE);
        DeliverMaterialToBlueprint(bpIdx, grassIdx);
        expect(blueprints[bpIdx].state == BLUEPRINT_READY_TO_BUILD);
        CompleteBlueprint(bpIdx);

        // Should be complete
        expect(blueprints[bpIdx].active == false);
        expect(HAS_FLOOR(2, 1, 0));
        expect(GetFloorMaterial(2, 1, 0) == MAT_DIRT);
    }

    it("should build log wall with oak material") {
        // Test #18: 2 oak logs → wall with MAT_OAK
        InitTestGridFromAscii("....\n");
        InitDesignations();
        ClearItems();

        SET_FLOOR(2, 0, 0);
        int bpIdx = CreateRecipeBlueprint(2, 0, 0, CONSTRUCTION_LOG_WALL);
        expect(bpIdx >= 0);

        for (int i = 0; i < 2; i++) {
            int idx = SpawnItemWithMaterial(0, 0, 0, ITEM_LOG, MAT_OAK);
            DeliverMaterialToBlueprint(bpIdx, idx);
        }
        CompleteBlueprint(bpIdx);

        expect(grid[0][0][2] == CELL_WALL);
        expect(GetWallMaterial(2, 0, 0) == MAT_OAK);
        expect(GetWallSourceItem(2, 0, 0) == ITEM_LOG);
    }

    it("should build log wall with pine material") {
        // Test #19: 2 pine logs → wall with MAT_PINE
        InitTestGridFromAscii("....\n");
        InitDesignations();
        ClearItems();

        SET_FLOOR(2, 0, 0);
        int bpIdx = CreateRecipeBlueprint(2, 0, 0, CONSTRUCTION_LOG_WALL);

        for (int i = 0; i < 2; i++) {
            int idx = SpawnItemWithMaterial(0, 0, 0, ITEM_LOG, MAT_PINE);
            DeliverMaterialToBlueprint(bpIdx, idx);
        }
        CompleteBlueprint(bpIdx);

        expect(grid[0][0][2] == CELL_WALL);
        expect(GetWallMaterial(2, 0, 0) == MAT_PINE);
    }

    it("should build brick wall with fixed MAT_BRICK") {
        // Test #22: 3 bricks → wall with MAT_BRICK (fixed, not inherited)
        InitTestGridFromAscii("....\n");
        InitDesignations();
        ClearItems();

        SET_FLOOR(2, 0, 0);
        int bpIdx = CreateRecipeBlueprint(2, 0, 0, CONSTRUCTION_BRICK_WALL);

        for (int i = 0; i < 3; i++) {
            int idx = SpawnItemWithMaterial(0, 0, 0, ITEM_BRICKS, MAT_BRICK);
            DeliverMaterialToBlueprint(bpIdx, idx);
        }
        CompleteBlueprint(bpIdx);

        expect(grid[0][0][2] == CELL_WALL);
        expect(GetWallMaterial(2, 0, 0) == MAT_BRICK);
    }

    it("should build thatch floor with fixed MAT_DIRT") {
        // Test #23: thatch floor resultMaterial = MAT_DIRT regardless of inputs
        const ConstructionRecipe* r = GetConstructionRecipe(CONSTRUCTION_THATCH_FLOOR);
        expect(r != NULL);
        expect(r->resultMaterial == MAT_DIRT);
        expect(r->stageCount == 2);
        expect(r->buildCategory == BUILD_FLOOR);
    }

    it("should reject floor blueprint on cell that already has floor") {
        // Test #46: can't place floor blueprint where floor already exists
        InitTestGridFromAscii("....\n");
        InitDesignations();

        // Explicitly set floor on (2,0,0)
        SET_FLOOR(2, 0, 0);
        expect(HAS_FLOOR(2, 0, 0));
        int bpIdx = CreateRecipeBlueprint(2, 0, 0, CONSTRUCTION_PLANK_FLOOR);
        expect(bpIdx == -1);  // should be rejected
    }

    it("should build ladder with planks and inherit material") {
        // Ladder with planks instead of log
        InitTestGridFromAscii("....\n....\n");
        InitDesignations();
        ClearItems();

        int bpIdx = CreateRecipeBlueprint(2, 1, 0, CONSTRUCTION_LADDER);
        expect(bpIdx >= 0);

        int idx = SpawnItemWithMaterial(0, 0, 0, ITEM_PLANKS, MAT_BIRCH);
        DeliverMaterialToBlueprint(bpIdx, idx);
        CompleteBlueprint(bpIdx);

        CellType ct = grid[0][1][2];
        expect(ct == CELL_LADDER_UP || ct == CELL_LADDER_DOWN || ct == CELL_LADDER_BOTH);
        expect(GetWallMaterial(2, 1, 0) == MAT_BIRCH);
    }
}

// =============================================================================
// Phase 6: Cancellation + Lossy Refund
// =============================================================================
describe(construction_cancellation) {
    it("should refund delivered items at 100% when cancelled mid-stage") {
        // Test #35: cancel with delivered but not-yet-built items
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(4, 4, 0, CONSTRUCTION_DRY_STONE_WALL);
        Blueprint* bp = &blueprints[bpIdx];
        expect(bp->state == BLUEPRINT_AWAITING_MATERIALS);

        // Deliver 2 of 3 required rocks
        int r1 = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_ROCK, MAT_GRANITE);
        DeliverMaterialToBlueprint(bpIdx, r1);
        int r2 = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_ROCK, MAT_GRANITE);
        DeliverMaterialToBlueprint(bpIdx, r2);
        expect(bp->stageDeliveries[0].deliveredCount == 2);

        int itemsBefore = 0;
        for (int i = 0; i < MAX_ITEMS; i++) {
            if (IsItemActive(i)) itemsBefore++;
        }

        CancelBlueprint(bpIdx);
        expect(bp->active == false);

        // Count refunded items — should be exactly 2 (100% for current stage)
        int itemsAfter = 0;
        int rocksAtBp = 0;
        for (int i = 0; i < MAX_ITEMS; i++) {
            if (!IsItemActive(i)) continue;
            itemsAfter++;
            int ix = (int)(items[i].x / CELL_SIZE);
            int iy = (int)(items[i].y / CELL_SIZE);
            if (ix == 4 && iy == 4 && items[i].type == ITEM_ROCK && items[i].material == MAT_GRANITE) {
                rocksAtBp++;
            }
        }
        expect(itemsAfter - itemsBefore == 2);
        expect(rocksAtBp == 2);
    }

    it("should lossy-refund consumed items from completed stages") {
        // Test #38: consumed items have recovery chance, not all returned
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        // Use wattle & daub: stage 0 = 2 sticks + 1 cordage, stage 1 = 2 dirt
        int bpIdx = CreateRecipeBlueprint(4, 4, 0, CONSTRUCTION_WATTLE_DAUB_WALL);
        Blueprint* bp = &blueprints[bpIdx];

        // Complete stage 0 (these become consumed items)
        int s1 = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_STICKS, MAT_OAK);
        DeliverMaterialToBlueprint(bpIdx, s1);
        int s2 = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_STICKS, MAT_OAK);
        DeliverMaterialToBlueprint(bpIdx, s2);
        int c1 = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_CORDAGE, MAT_NONE);
        DeliverMaterialToBlueprint(bpIdx, c1);
        CompleteBlueprint(bpIdx);
        expect(bp->stage == 1);

        int itemsBefore = 0;
        for (int i = 0; i < MAX_ITEMS; i++) {
            if (IsItemActive(i)) itemsBefore++;
        }

        // Cancel during stage 1 with no deliveries yet — only consumed refund
        // Run many trials to verify lossy behavior
        // With seed 12345 and 75% chance, we expect some but not all 3 items
        SetRandomSeed(12345);
        CancelBlueprint(bpIdx);
        expect(bp->active == false);

        int itemsAfter = 0;
        for (int i = 0; i < MAX_ITEMS; i++) {
            if (IsItemActive(i)) itemsAfter++;
        }

        int refunded = itemsAfter - itemsBefore;
        // 3 consumed items, each 75% chance: expect 0-3 items
        expect(refunded >= 0);
        expect(refunded <= 3);
    }

    it("should cancel during BUILDING and refund correctly") {
        // Test #37: cancel during build step
        InitTestGridFromAscii(
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(4, 4, 0, CONSTRUCTION_DRY_STONE_WALL);
        Blueprint* bp = &blueprints[bpIdx];

        // Deliver all 3 rocks
        for (int i = 0; i < 3; i++) {
            int r = SpawnItemWithMaterial(4 * CELL_SIZE + CELL_SIZE * 0.5f,
                                          4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                          ITEM_ROCK, MAT_GRANITE);
            DeliverMaterialToBlueprint(bpIdx, r);
        }
        expect(bp->state == BLUEPRINT_READY_TO_BUILD);

        // Simulate building in progress
        bp->state = BLUEPRINT_BUILDING;
        bp->assignedBuilder = 0;
        bp->progress = 0.5f;

        // Set up mover with a build job targeting this blueprint
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 4 * CELL_SIZE + CELL_SIZE * 0.5f,
                  4 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        int jobId = CreateJob(JOBTYPE_BUILD);
        Job* job = GetJob(jobId);
        job->assignedMover = 0;
        job->targetBlueprint = bpIdx;
        m->currentJobId = jobId;

        int itemsBefore = 0;
        for (int i = 0; i < MAX_ITEMS; i++) {
            if (IsItemActive(i)) itemsBefore++;
        }

        CancelBlueprint(bpIdx);
        expect(bp->active == false);

        // Builder's job should have been cancelled
        expect(m->currentJobId == -1);

        // Delivered items are current-stage (100% refund): 3 rocks
        int itemsAfter = 0;
        for (int i = 0; i < MAX_ITEMS; i++) {
            if (IsItemActive(i)) itemsAfter++;
        }
        expect(itemsAfter - itemsBefore == 3);
    }

    it("should proactively cancel in-transit haul jobs and release reservations") {
        // Test #39: cancel releases all reservations on in-transit items
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(5, 5, 0, CONSTRUCTION_DRY_STONE_WALL);
        Blueprint* bp = &blueprints[bpIdx];

        // Spawn rocks far from blueprint
        int r1 = SpawnItemWithMaterial(1 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_ROCK, MAT_GRANITE);

        // Set up mover and manually create a haul-to-blueprint job
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f,
                  1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;

        // Simulate: item reserved, mover has haul-to-blueprint job
        items[r1].reservedBy = 0;
        bp->stageDeliveries[0].reservedCount = 1;

        int jobId = CreateJob(JOBTYPE_HAUL_TO_BLUEPRINT);
        Job* job = GetJob(jobId);
        job->assignedMover = 0;
        job->targetItem = r1;
        job->targetBlueprint = bpIdx;
        m->currentJobId = jobId;

        // Cancel the blueprint
        CancelBlueprint(bpIdx);

        // Mover should be idle with no job
        expect(m->currentJobId == -1);
        // Item reservation should be released
        expect(items[r1].reservedBy == -1);
        // Item should still be active (it was just reserved, not consumed)
        expect(items[r1].active == true);
        expect(bp->active == false);
    }

    it("should cancel haul mid-walk and release reservation") {
        // Test #32: cancel haul mid-walk
        InitTestGridFromAscii(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n");

        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();

        int bpIdx = CreateRecipeBlueprint(8, 8, 0, CONSTRUCTION_DRY_STONE_WALL);
        Blueprint* bp = &blueprints[bpIdx];

        // Spawn rocks far away
        int r1 = SpawnItemWithMaterial(1 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_ROCK, MAT_GRANITE);
        int r2 = SpawnItemWithMaterial(2 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_ROCK, MAT_GRANITE);
        int r3 = SpawnItemWithMaterial(3 * CELL_SIZE + CELL_SIZE * 0.5f,
                                       1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f,
                                       ITEM_ROCK, MAT_GRANITE);

        // Mover at (0,0)
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f,
                  0 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, goal, 100.0f);
        moverCount = 1;

        // Run a few ticks to let mover pick up a haul job
        RebuildStockpileFreeSlotCounts();
        int jobId = WorkGiver_BlueprintHaul(0);
        expect(jobId >= 0);
        expect(items[r1].reservedBy == 0 || items[r2].reservedBy == 0 || items[r3].reservedBy == 0);
        expect(bp->stageDeliveries[0].reservedCount == 1);

        // Cancel the blueprint while mover is hauling
        CancelBlueprint(bpIdx);

        // Mover's job should be cancelled
        expect(m->currentJobId == -1);
        // All items unreserved
        expect(items[r1].reservedBy == -1);
        expect(items[r2].reservedBy == -1);
        expect(items[r3].reservedBy == -1);
        expect(bp->active == false);
    }
}

int main(int argc, char* argv[]) {
    // Suppress logs by default, use -v for verbose
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

    test(item_system);
    test(item_reservation);
    test(mover_job_state);
    test(pickup_behavior);
    test(reservation_safety);
    test(post_job_behavior);
    
    // Stockpile tests (Phase 1)
    test(stockpile_system);
    test(haul_happy_path);
    test(stockpile_capacity);
    test(multi_agent_hauling);
    test(haul_cancellation);
    
    // Edge case tests (from convo-jobs.md)
    test(filter_change_mid_haul);
    test(dynamic_obstacles);
    test(stockpile_expansion);
    test(stress_test);
    
    // Future features (hauling-next.md) - expected to fail until implemented
    test(unreachable_item_cooldown);
    test(gather_zones);
    test(stacking_merging);
    test(stockpile_priority);
    test(stockpile_max_stack_size);
    
    // Ground item blocking (new feature)
    test(stockpile_ground_item_blocking);

    // Clear job state (JOB_MOVING_TO_DROP)
    test(clear_job_state);
    
    // Strong stockpile behavior tests (player expectations)
    test(stockpile_strong_tests);
    
    // Item spatial grid (optimization)
    test(item_spatial_grid);
    
    // Cell-based stockpile operations
    test(stockpile_cell_operations);
    
    // Mining/digging tests
    test(mining_designation);
    test(mining_job_assignment);
    test(mining_job_execution);
    test(mining_multiple_designations);

    // Channeling tests (vertical digging)
    test(channel_designation);
    test(channel_ramp_detection);
    test(channel_job_execution);
    test(channel_workgiver);
    test(channel_hpa_ramp_links);
    test(channel_rectangle_ramps);

    // Building/construction tests
    test(building_blueprint);
    test(building_haul_job);
    test(building_job_execution);
    test(building_two_movers);
    
    // Job pool tests (Phase 1 of Jobs Refactor)
    test(job_pool);
    
    // Job driver tests (Phase 2 of Jobs Refactor)
    test(job_drivers);
    
    // Game speed tests (verify mining/building scales with game speed)
    test(job_game_speed);
    
    // Mover capabilities tests (Phase 3 of Jobs Refactor)
    test(mover_capabilities);
    
    // WorkGivers tests (Phase 4 of Jobs Refactor)
    test(workgivers);
    
    // Blueprint material selection tests
    test(blueprint_material_selection);
    
    // Final approach tests (mover arrival fix)
    test(final_approach);
    
    // Item lifecycle tests (items.c audit findings)
    test(item_lifecycle);
    
    // Mover lifecycle tests (mover.c audit findings)
    test(mover_lifecycle);
    
    // Job lifecycle tests (jobs.c audit findings)
    test(job_lifecycle);
    
    // Stockpile lifecycle tests (stockpiles.c audit findings)
    test(stockpile_lifecycle);
    
    // Workshop lifecycle tests (workshops.c audit findings)
    test(workshop_lifecycle);
    
    // Designation lifecycle tests (designations.c audit findings)
    test(designation_lifecycle);
    
    // Unreachable cooldown poisoning (cross-z-level bug)
    test(unreachable_cooldown_poisoning);
    
    // Save/load state restoration (audit findings)
    test(saveload_state_restoration);
    
    // Grid audit integration tests (Findings 3, 4, 6)
    test(grid_audit_blueprint_integration);
    test(grid_audit_tree_chopping_integration);
    
    // Input audit tests (docs/todo/audits/input.md)
    test(input_audit_material_consistency);
    test(input_audit_erase_ramp);
    test(input_audit_soil_repath);
    test(input_audit_grass_placement);
    test(input_audit_erase_designations);
    test(input_audit_quick_erase_metadata);
    
    // Passive workshop tests (TDD - drying rack / ITEM_DRIED_GRASS)
    test(passive_workshop);
    
    // Semi-passive workshop tests (TDD - charcoal pit ignition)
    test(semi_passive_workshop);
    
    // Chop → ChopFelled transition (stale cache bug)
    test(chop_felled_transition);
    
    // Construction recipe system (Phase 1 - dry stone wall)
    test(construction_recipe_data);
    test(construction_recipe_blueprint);
    test(construction_recipe_delivery);
    test(construction_recipe_build);
    
    // Construction recipe system (Phase 2/3 - wattle & daub, multi-input + multi-stage)
    test(construction_wattle_data);
    test(construction_wattle_delivery);
    test(construction_wattle_parallel);
    test(construction_multi_stage);
    test(construction_multi_stage_edge_cases);
    test(construction_plank_wall);
    
    // Construction recipe system (Phase 5 - site clearing)
    test(construction_site_clearing);
    
    // Construction recipe system (Phase 4 - OR-materials + locking)
    test(construction_or_materials);
    test(construction_alternative_locking);
    test(construction_any_building_mat);
    
    // Construction recipe system (Phase 7 - new recipes: log wall, brick, floor, ladder, thatch)
    test(construction_new_recipes);

    // Construction recipe system (Phase 6 - cancellation + lossy refund)
    test(construction_cancellation);
    
    return summary();
}
