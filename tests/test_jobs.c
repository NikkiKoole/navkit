#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/world/pathfinding.h"
#include "../src/entities/mover.h"
#include "../src/world/terrain.h"
#include "../src/entities/items.h"
#include "../src/entities/jobs.h"
#include "../src/entities/stockpiles.h"
#include "../src/world/designations.h"
#include "../src/core/time.h"
#include <string.h>
#include <math.h>

// Helper functions to check mover job state (replaces m->jobState checks)
static bool MoverIsIdle(Mover* m) {
    return m->currentJobId < 0;
}

static bool MoverHasHaulJob(Mover* m) {
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

static bool MoverIsMining(Mover* m) {
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
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 4);
        
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 8);
        
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 4);
        
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 4);
        
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 8);
        
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 8);
        
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
        bool found = FindFreeStockpileSlot(spIdx, ITEM_RED, &slotX, &slotY);
        
        expect(found == true);
        expect(slotX >= 2 && slotX < 4);
        expect(slotY >= 2 && slotY < 4);
    }

    it("should reserve stockpile slot") {
        ClearStockpiles();
        
        int spIdx = CreateStockpile(2, 2, 0, 1, 1);  // 1 tile only
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        int slotX, slotY;
        bool found = FindFreeStockpileSlot(spIdx, ITEM_RED, &slotX, &slotY);
        expect(found == true);
        
        // Reserve it
        bool reserved = ReserveStockpileSlot(spIdx, slotX, slotY, 0);  // mover 0
        expect(reserved == true);
        
        // Should not find another free slot now
        int slotX2, slotY2;
        bool found2 = FindFreeStockpileSlot(spIdx, ITEM_RED, &slotX2, &slotY2);
        expect(found2 == false);
    }
}

describe(haul_happy_path) {
    it("should haul single item to matching stockpile") {
        // Test 1 from convo-jobs.md
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        
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
        
        // Exactly 1 item should be stored
        int storedCount = 0;
        if (items[item1].state == ITEM_IN_STOCKPILE) storedCount++;
        if (items[item2].state == ITEM_IN_STOCKPILE) storedCount++;
        expect(storedCount == 1);
        
        // Other item should still be on ground
        int groundCount = 0;
        if (items[item1].state == ITEM_ON_GROUND) groundCount++;
        if (items[item2].state == ITEM_ON_GROUND) groundCount++;
        expect(groundCount == 1);
        
        // Mover should be idle (not stuck carrying)
        expect(MoverIsIdle(m));
        expect(MoverGetCarryingItem(m) == -1);
    }
}

describe(multi_agent_hauling) {
    it("should not have two movers deliver to same stockpile slot") {
        // Test 4 from convo-jobs.md
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        
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
            
            // Check if all stored
            int stored = 0;
            for (int j = 0; j < 3; j++) {
                if (items[itemIdxs[j]].state == ITEM_IN_STOCKPILE) stored++;
            }
            if (stored == 3) break;
        }
        
        // All 3 items should be stored
        for (int i = 0; i < 3; i++) {
            expect(items[itemIdxs[i]].state == ITEM_IN_STOCKPILE);
        }
        
        // With stacking enabled, items CAN be at the same position (stacked)
        // Just verify all items are stored (checked above)
    }
}

describe(haul_cancellation) {
    it("should release stockpile reservation when item deleted mid-haul") {
        // Test 5 from convo-jobs.md (extended for stockpiles)
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        
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
        bool found = FindFreeStockpileSlot(spIdx, ITEM_RED, &slotX, &slotY);
        expect(found == true);
    }

    it("should safe-drop item when stockpile deleted while carrying") {
        // Test 7 from convo-jobs.md
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 5);
        
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 5);
        
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        
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
        
        // Stockpile with only 1 tile initially
        int spIdx = CreateStockpile(2, 2, 0, 1, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
        // Run until first item stored and mover idle
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            
            if (items[item1].state == ITEM_IN_STOCKPILE || items[item2].state == ITEM_IN_STOCKPILE) {
                if (MoverIsIdle(m)) break;
            }
        }
        
        // One item should be stored
        int storedCount = 0;
        if (items[item1].state == ITEM_IN_STOCKPILE) storedCount++;
        if (items[item2].state == ITEM_IN_STOCKPILE) storedCount++;
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
            if (items[item1].state == ITEM_IN_STOCKPILE) storedCount++;
            if (items[item2].state == ITEM_IN_STOCKPILE) storedCount++;
            if (storedCount == 2) break;
        }
        
        // Both items should now be stored
        expect(items[item1].state == ITEM_IN_STOCKPILE);
        expect(items[item2].state == ITEM_IN_STOCKPILE);
    }
}

describe(stress_test) {
    it("should handle many items and agents without deadlock") {
        // Test 12 from convo-jobs.md (smaller scale for unit test)
        // 20x20 grid to ensure plenty of room
        InitGridFromAsciiWithChunkSize(
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
            "....................\n", 20, 20);
        
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
                if (items[itemIdxs[j]].state == ITEM_IN_STOCKPILE) stored++;
            }
            if (stored == 9) break;
        }
        
        // All items should be stored
        int stored = 0;
        for (int i = 0; i < 9; i++) {
            if (items[itemIdxs[i]].state == ITEM_IN_STOCKPILE) stored++;
        }
        expect(stored == 9);
        
        // All movers should be idle (not stuck carrying)
        for (int i = 0; i < 3; i++) {
            expect(MoverIsIdle(&movers[i]));
            expect(MoverGetCarryingItem(&movers[i]) == -1);
        }
        
        // With stacking enabled, items CAN be at the same position (stacked)
        // Just verify all items are stored and movers are idle (checked above)
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..####....\n"
            "..#..#....\n"
            "..####....\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        
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
        g_legacyWalkability = true;  // Use legacy mode for this complex job test
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..####....\n"
            "..#..#....\n"
            "..####....\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        
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
        grid[0][3][2] = CELL_WALKABLE;  // Open the left wall
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        
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
            
            if (items[item1].state == ITEM_IN_STOCKPILE && items[item2].state == ITEM_IN_STOCKPILE) break;
        }
        
        // Both items should be hauled (no gather zone restriction)
        expect(items[item1].state == ITEM_IN_STOCKPILE);
        expect(items[item2].state == ITEM_IN_STOCKPILE);
    }
}

describe(stacking_merging) {
    it("should merge items into existing partial stacks") {
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        
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
        int itemIdx = SpawnItem(8 * CELL_SIZE + CELL_SIZE * 0.5f, 8 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // Run simulation
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (items[itemIdx].state == ITEM_IN_STOCKPILE) break;
        }
        
        // Item should be merged into existing stack
        expect(items[itemIdx].state == ITEM_IN_STOCKPILE);
        
        // Stack should now have 4 items
        int stackCount = GetStockpileSlotCount(spIdx, 5, 5);
        expect(stackCount == 4);
    }
    
    it("should not merge different item types into same stack") {
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        
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
}

describe(stockpile_priority) {
    it("should re-haul items from low to high priority stockpile") {
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        
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
        
        // Item on ground
        int itemIdx = SpawnItem(3 * CELL_SIZE + CELL_SIZE * 0.5f, 5 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_RED);
        
        // First, item should be hauled to nearest stockpile (low priority)
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (items[itemIdx].state == ITEM_IN_STOCKPILE) break;
        }
        
        expect(items[itemIdx].state == ITEM_IN_STOCKPILE);
        // Should be in low-priority first (closer/first available)
        int itemTileX = (int)(GetItemX(itemIdx) / CELL_SIZE);
        int itemTileY = (int)(GetItemY(itemIdx) / CELL_SIZE);
        expect(itemTileX == 2);
        expect(itemTileY == 2);
        
        // Continue running - mover should re-haul to high priority
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        
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
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 4);
        
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
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 4);
        
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
        grid[0][1][4] = CELL_FLOOR;
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
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 8);
        
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
        
        // Pre-fill all 9 slots with 1-2 items each (partially filled)
        for (int ly = 0; ly < 3; ly++) {
            for (int lx = 0; lx < 3; lx++) {
                SetStockpileSlotCount(sp, lx, ly, ITEM_RED, 2);
                // Spawn actual items in the slots
                float slotX = (3 + lx + 0.5f) * CELL_SIZE;
                float slotY = (3 + ly + 0.5f) * CELL_SIZE;
                for (int n = 0; n < 2; n++) {
                    int id = SpawnItem(slotX, slotY, 0.0f, ITEM_RED);
                    items[id].state = ITEM_IN_STOCKPILE;
                }
            }
        }
        // Total: 18 items in 9 slots, capacity is 63
        
        // Spawn one more RED item on the ground
        int newItem = SpawnItem(1.5f * CELL_SIZE, 1.5f * CELL_SIZE, 0.0f, ITEM_RED);
        
        // Run simulation - mover should pick up and stack the item
        for (int i = 0; i < 600; i++) {
            Tick();
            ItemsTick(TICK_DT);
            AssignJobs();
            JobsTick();
            
            if (items[newItem].state == ITEM_IN_STOCKPILE) break;
        }
        
        // Item should be in stockpile (stacked with existing items)
        expect(items[newItem].state == ITEM_IN_STOCKPILE);
        expect(MoverIsIdle(m));
    }
    
    it("should respect per-stockpile max stack size") {
        // 8x4 grid
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 4);
        
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
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 4);
        
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
        
        // Stockpile A with 5 items, will become overfull
        int spA = CreateStockpile(2, 2, 0, 1, 1);
        SetStockpileSlotCount(spA, 0, 0, ITEM_RED, 5);
        float slotAx = (2 + 0.5f) * CELL_SIZE;
        float slotAy = (2 + 0.5f) * CELL_SIZE;
        int itemIds[5];
        for (int i = 0; i < 5; i++) {
            itemIds[i] = SpawnItem(slotAx, slotAy, 0.0f, ITEM_RED);
            items[itemIds[i]].state = ITEM_IN_STOCKPILE;
        }
        
        // Stockpile B - empty, destination for excess
        (void)CreateStockpile(6, 2, 0, 1, 1);
        
        // Reduce A's max stack to 2 - now overfull by 3
        SetStockpileMaxStackSize(spA, 2);
        
        // Run simulation - movers should re-haul 3 excess items to B
        for (int i = 0; i < 2000; i++) {
            Tick();
            ItemsTick(TICK_DT);
            AssignJobs();
            JobsTick();
        }
        
        // Count items in each stockpile
        int inA = 0, inB = 0;
        for (int i = 0; i < 5; i++) {
            int slotX = (int)(GetItemX(itemIds[i]) / CELL_SIZE);
            if (slotX == 2) inA++;
            if (slotX == 6) inB++;
        }
        
        expect(inA == 2);  // only max stack size remains
        expect(inB == 3);  // excess moved here
        expect(GetStockpileSlotCount(spA, 2, 2) == 2);
    }
    
    it("should allow overfull slots when max stack size is reduced") {
        // 8x4 grid
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 4);
        
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
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 4);
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        // Stockpile starting with max = 3
        int sp = CreateStockpile(3, 2, 0, 1, 1);
        SetStockpileMaxStackSize(sp, 3);
        
        // Pre-fill slot with 3 items (at max)
        SetStockpileSlotCount(sp, 0, 0, ITEM_RED, 3);
        
        // Spawn items in stockpile
        float slotX = (3 + 0.5f) * CELL_SIZE;
        float slotY = (2 + 0.5f) * CELL_SIZE;
        for (int i = 0; i < 3; i++) {
            int id = SpawnItem(slotX, slotY, 0.0f, ITEM_RED);
            items[id].state = ITEM_IN_STOCKPILE;
        }
        
        // Increase max stack size to 10 - no items should be ejected
        SetStockpileMaxStackSize(sp, 10);
        
        // All items should still be in stockpile
        int inStockpile = 0;
        for (int i = 0; i < MAX_ITEMS; i++) {
            if (IsItemActive(i) && items[i].state == ITEM_IN_STOCKPILE) inStockpile++;
        }
        
        expect(inStockpile == 3);
        expect(GetStockpileSlotCount(sp, 3, 2) == 3);
    }
}

describe(stockpile_ground_item_blocking) {
    it("should not use slot with foreign ground item on it") {
        // A green item on a red-only stockpile tile should block that slot
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        
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
        bool found = FindFreeStockpileSlot(spIdx, ITEM_RED, &slotX, &slotY);
        
        // Should find the second slot (6,5), not the first (blocked by green item)
        expect(found == true);
        expect(slotX == 6);
        expect(slotY == 5);
    }
    
    it("should not use slot with matching ground item on it until absorbed") {
        // A red item on ground at a red stockpile tile should also block
        // (it needs to be "absorbed" first via the absorb job)
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        
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
        bool found = FindFreeStockpileSlot(spIdx, ITEM_RED, &slotX, &slotY);
        
        // Should find the second slot (6,5), not the first (blocked by ground item)
        expect(found == true);
        expect(slotX == 6);
        expect(slotY == 5);
    }
    
    it("should absorb matching ground item on stockpile tile") {
        // Mover should pick up a red item on a red stockpile and place it "properly"
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 10);
        
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 5);
        
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 5);
        
        ClearMovers();
        ClearItems();
        
        // No items spawned
        BuildItemSpatialGrid();
        
        // Query any tile - should return -1
        int found = QueryItemAtTile(5, 3, 0);
        expect(found == -1);
    }
    
    it("should not index carried items") {
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 5);
        
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 5);
        
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 5);
        
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 5);
        
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
        InitGridFromAsciiWithChunkSize(
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n"
            "..........\n", 10, 5);
        
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
        ClearStockpiles();
        
        int spIdx = CreateStockpile(5, 5, 0, 3, 1);  // 3 cells wide, 1 tall
        
        // Remove middle cell
        RemoveStockpileCells(spIdx, 6, 5, 6, 5);
        
        // Should still find slots in remaining cells
        int slotX, slotY;
        bool found = FindFreeStockpileSlot(spIdx, ITEM_RED, &slotX, &slotY);
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
        InitGridFromAsciiWithChunkSize(
            ".....\n"
            ".###.\n"
            ".###.\n"
            ".###.\n"
            ".....\n", 5, 5);
        
        InitDesignations();
        
        // Designate center wall
        bool result = DesignateMine(2, 2, 0);
        expect(result == true);
        expect(HasMineDesignation(2, 2, 0) == true);
        expect(CountMineDesignations() == 1);
    }
    
    it("should not designate floor for digging") {
        InitGridFromAsciiWithChunkSize(
            ".....\n"
            ".###.\n"
            ".###.\n"
            ".###.\n"
            ".....\n", 5, 5);
        
        InitDesignations();
        
        // Try to designate floor tile
        bool result = DesignateMine(0, 0, 0);
        expect(result == false);
        expect(HasMineDesignation(0, 0, 0) == false);
    }
    
    it("should cancel a designation") {
        InitGridFromAsciiWithChunkSize(
            ".....\n"
            ".###.\n"
            ".###.\n"
            ".###.\n"
            ".....\n", 5, 5);
        
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
        InitGridFromAsciiWithChunkSize(
            ".....\n"
            ".###.\n"
            ".###.\n"
            ".###.\n"
            ".....\n", 5, 5);
        
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
        InitGridFromAsciiWithChunkSize(
            "#####\n"
            "#####\n"
            "#####\n"
            "#####\n"
            "#####\n", 5, 5);
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        
        // Make one floor cell for mover to stand on
        grid[0][0][0] = CELL_FLOOR;
        
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
    it("should complete mine job and convert wall to floor") {
        InitGridFromAsciiWithChunkSize(
            ".....\n"
            ".#...\n"
            ".....\n"
            ".....\n"
            ".....\n", 5, 5);
        
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
            
            // Check if mine completed (wall became floor)
            if (grid[0][1][1] != CELL_WALL) {
                completed = true;
                break;
            }
        }
        
        expect(completed == true);
        expect(grid[0][1][1] == CELL_FLOOR);
        expect(HasMineDesignation(1, 1, 0) == false);
        expect(MoverIsIdle(m));
    }
    
    it("should spawn orange block when mine completes") {
        InitGridFromAsciiWithChunkSize(
            ".....\n"
            ".#...\n"
            ".....\n"
            ".....\n"
            ".....\n", 5, 5);
        
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
        
        // Find the spawned item and verify it's orange at the mine location
        bool foundOrange = false;
        for (int i = 0; i < MAX_ITEMS; i++) {
            if (IsItemActive(i) && GetItemType(i) == ITEM_ORANGE) {
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
        InitGridFromAsciiWithChunkSize(
            ".....\n"
            ".#...\n"
            ".....\n"
            ".....\n"
            ".....\n", 5, 5);
        
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
        InitGridFromAsciiWithChunkSize(
            ".....\n"
            ".#...\n"
            ".....\n"
            ".....\n"
            ".....\n", 5, 5);
        
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
        grid[0][1][1] = CELL_FLOOR;
        
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
        InitGridFromAsciiWithChunkSize(
            "......\n"
            ".#.#..\n"
            "......\n"
            ".#....\n"
            "......\n"
            "......\n", 6, 6);
        
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
        
        // All walls should be dug
        expect(grid[0][1][1] == CELL_FLOOR);
        expect(grid[0][1][3] == CELL_FLOOR);
        expect(grid[0][3][1] == CELL_FLOOR);
        expect(CountMineDesignations() == 0);
    }
}

// =============================================================================
// Building/Construction Tests
// =============================================================================

describe(building_blueprint) {
    it("should create blueprint on floor tile") {
        InitGridFromAsciiWithChunkSize(
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n", 6, 6);
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        
        // Create blueprint on floor
        int bpIdx = CreateBuildBlueprint(2, 2, 0);
        expect(bpIdx >= 0);
        expect(HasBlueprint(2, 2, 0) == true);
        expect(blueprints[bpIdx].state == BLUEPRINT_AWAITING_MATERIALS);
        expect(CountBlueprints() == 1);
    }
    
    it("should not create blueprint on wall tile") {
        InitGridFromAsciiWithChunkSize(
            "......\n"
            ".#....\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n", 6, 6);
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        
        // Try to create blueprint on wall
        int bpIdx = CreateBuildBlueprint(1, 1, 0);
        expect(bpIdx == -1);
        expect(HasBlueprint(1, 1, 0) == false);
        expect(CountBlueprints() == 0);
    }
    
    it("should cancel blueprint") {
        InitGridFromAsciiWithChunkSize(
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n", 6, 6);
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        InitDesignations();
        
        int bpIdx = CreateBuildBlueprint(2, 2, 0);
        expect(CountBlueprints() == 1);
        
        CancelBlueprint(bpIdx);
        expect(HasBlueprint(2, 2, 0) == false);
        expect(CountBlueprints() == 0);
    }
}

describe(building_haul_job) {
    it("should assign haul job to blueprint needing materials") {
        InitGridFromAsciiWithChunkSize(
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n", 6, 6);
        
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
        
        // Item at (1,1) - must be ITEM_STONE_BLOCKS for building walls
        int itemIdx = SpawnItem(1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_STONE_BLOCKS);
        
        // Blueprint at (4,4)
        int bpIdx = CreateBuildBlueprint(4, 4, 0);
        expect(blueprints[bpIdx].state == BLUEPRINT_AWAITING_MATERIALS);
        
        // Run job assignment
        AssignJobs();
        
        // Mover should be assigned to haul the item
        expect(MoverIsMovingToPickup(m));
        expect(MoverGetTargetItem(m) == itemIdx);
        expect(MoverGetTargetBlueprint(m) == bpIdx);
        expect(blueprints[bpIdx].reservedItem == itemIdx);
    }
    
    it("should not assign haul job when no items available") {
        InitGridFromAsciiWithChunkSize(
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n", 6, 6);
        
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
        int bpIdx = CreateBuildBlueprint(4, 4, 0);
        
        AssignJobs();
        
        // Mover should remain idle
        expect(MoverIsIdle(m));
        expect(blueprints[bpIdx].reservedItem == -1);
    }
}

describe(building_job_execution) {
    it("should deliver material and complete build") {
        InitGridFromAsciiWithChunkSize(
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n", 6, 6);
        
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
        
        // Item at (1,1) - must be ITEM_STONE_BLOCKS for building walls
        int itemIdx = SpawnItem(1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_STONE_BLOCKS);
        (void)itemIdx;  // Used only in setup
        
        // Blueprint at (3,3) - will become a wall
        CreateBuildBlueprint(3, 3, 0);
        
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
        
        // Item should be consumed
        expect(IsItemActive(itemIdx) == false);
        
        // Mover should be idle
        expect(MoverIsIdle(m));
    }
    
    it("should cancel haul job when blueprint is cancelled") {
        InitGridFromAsciiWithChunkSize(
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n"
            "......\n", 6, 6);
        
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
        
        // Item at (1,1) - must be ITEM_STONE_BLOCKS for building walls
        int itemIdx = SpawnItem(1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_STONE_BLOCKS);
        
        // Blueprint at (4,4)
        int bpIdx = CreateBuildBlueprint(4, 4, 0);
        
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
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 8);
        
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
        
        // Item at (1,1) - must be ITEM_STONE_BLOCKS for building walls
        SpawnItem(1 * CELL_SIZE + CELL_SIZE * 0.5f, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_STONE_BLOCKS);
        
        // Blueprint at (5,5)
        int bpIdx = CreateBuildBlueprint(5, 5, 0);
        
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
        ReserveStockpileSlot(sp, 5, 1, 0);
        
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
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "...#....\n"
            "........\n"
            "........\n", 8, 8);
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        InitDesignations();
        ClearJobs();
        
        // Create mover at (1,1)
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, CELL_SIZE * 1.5f, CELL_SIZE * 1.5f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        // Designate wall at (3,1) for digging
        DesignateMine(3, 1, 0);
        
        // Create a mine job using the new Job pool
        int jobId = CreateJob(JOBTYPE_MINE);
        Job* job = GetJob(jobId);
        job->targetMineX = 3;
        job->targetMineY = 1;
        job->targetMineZ = 0;
        job->assignedMover = 0;
        
        // Assign job to mover
        m->currentJobId = jobId;
        
        // Set mover goal to adjacent tile (2,1 is adjacent to wall at 3,1)
        m->goal = (Point){2, 1, 0};
        m->needsRepath = true;
        
        // Reserve designation
        Designation* d = GetDesignation(3, 1, 0);
        d->assignedMover = 0;
        
        // Run simulation
        for (int i = 0; i < 600; i++) {
            Tick();
            JobsTick();
            if (grid[0][1][3] == CELL_FLOOR) break;
        }
        
        // Verify mine completed - wall is now floor
        expect(grid[0][1][3] == CELL_FLOOR);
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
        int bpIdx = CreateBuildBlueprint(4, 1, 0);
        blueprints[bpIdx].state = BLUEPRINT_READY_TO_BUILD;
        blueprints[bpIdx].deliveredMaterials = 1;
        
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
        ReserveStockpileSlot(sp, 5, 1, 0);
        
        // Delete the item mid-job (simulate failure)
        DeleteItem(itemIdx);
        
        // Run one tick - driver should detect failure and cancel
        Tick();
        JobsTick();
        
        // Job should be cancelled, reservations released
        expect(m->currentJobId == -1);
        expect(GetJob(jobId)->active == false);
        // Slot should be released (check reservation)
        expect(stockpiles[sp].reservedBy[0] == -1);
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
        ReserveStockpileSlot(sp, 1, 0, 0);
        
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
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "...#....\n"
            "........\n"
            "........\n", 8, 8);
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        InitDesignations();
        ClearJobs();
        
        // Create mover adjacent to wall (at 2,1, wall is at 3,1)
        Mover* m = &movers[0];
        Point goal = {2, 1, 0};
        InitMover(m, CELL_SIZE * 2.5f, CELL_SIZE * 1.5f, 0.0f, goal, 100.0f);
        m->pathLength = 0;  // Already at destination
        moverCount = 1;
        
        // Designate wall at (3,1) for digging
        DesignateMine(3, 1, 0);
        
        // Create mine job
        int jobId = CreateJob(JOBTYPE_MINE);
        Job* job = GetJob(jobId);
        job->targetMineX = 3;
        job->targetMineY = 1;
        job->targetMineZ = 0;
        job->assignedMover = 0;
        job->step = STEP_WORKING;
        job->progress = 0.0f;
        m->currentJobId = jobId;
        
        // Reserve designation
        Designation* d = GetDesignation(3, 1, 0);
        d->assignedMover = 0;
        
        // Test at 1x speed - count ticks needed
        gameSpeed = 1.0f;
        gameDeltaTime = TICK_DT * gameSpeed;
        
        int ticksAt1x = 0;
        for (int i = 0; i < 600; i++) {
            JobsTick();
            ticksAt1x++;
            if (grid[0][1][3] == CELL_FLOOR) break;
        }
        expect(grid[0][1][3] == CELL_FLOOR);
        
        // Reset for 2x speed test
        grid[0][1][3] = CELL_WALL;
        DesignateMine(3, 1, 0);
        d = GetDesignation(3, 1, 0);
        d->assignedMover = 0;
        
        // Create fresh job for 2x test
        int jobId2 = CreateJob(JOBTYPE_MINE);
        Job* job2 = GetJob(jobId2);
        job2->targetMineX = 3;
        job2->targetMineY = 1;
        job2->targetMineZ = 0;
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
            if (grid[0][1][3] == CELL_FLOOR) break;
        }
        expect(grid[0][1][3] == CELL_FLOOR);
        
        // At 2x speed, should complete in roughly half the ticks
        // Allow some tolerance (within 20%)
        printf("Mine: ticksAt1x=%d, ticksAt2x=%d, ratio=%.2f\n", 
               ticksAt1x, ticksAt2x, (float)ticksAt1x / ticksAt2x);
        expect(ticksAt2x < ticksAt1x);
        expect(ticksAt2x <= (ticksAt1x / 2) + 5);  // Should be ~half, with small tolerance
        
        // Reset game speed
        gameSpeed = 1.0f;
        gameDeltaTime = TICK_DT;
    }
    
    it("should complete build job faster at higher game speed") {
        g_legacyWalkability = true;  // Use legacy mode for this complex job test
        // Setup world
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 8);
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        ClearMovers();
        ClearItems();
        InitDesignations();
        ClearJobs();
        
        // Create mover at blueprint location (4,1)
        Mover* m = &movers[0];
        Point goal = {4, 1, 0};
        InitMover(m, CELL_SIZE * 4.5f, CELL_SIZE * 1.5f, 0.0f, goal, 100.0f);
        m->pathLength = 0;  // Already at destination
        moverCount = 1;
        
        // Create blueprint at (4,1) - ready to build
        int bpIdx = CreateBuildBlueprint(4, 1, 0);
        blueprints[bpIdx].state = BLUEPRINT_BUILDING;
        blueprints[bpIdx].deliveredMaterials = 1;
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
            if (grid[0][1][4] == CELL_WALL) break;
        }
        expect(grid[0][1][4] == CELL_WALL);
        
        // Reset for 2x speed test
        grid[0][1][4] = CELL_WALKABLE;
        int bpIdx2 = CreateBuildBlueprint(4, 1, 0);
        blueprints[bpIdx2].state = BLUEPRINT_BUILDING;
        blueprints[bpIdx2].deliveredMaterials = 1;
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
            if (grid[0][1][4] == CELL_WALL) break;
        }
        expect(grid[0][1][4] == CELL_WALL);
        
        // At 2x speed, should complete in roughly half the ticks
        printf("Build: ticksAt1x=%d, ticksAt2x=%d, ratio=%.2f\n", 
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
        int bpIdx = CreateBuildBlueprint(4, 1, 0);
        blueprints[bpIdx].state = BLUEPRINT_READY_TO_BUILD;
        blueprints[bpIdx].deliveredMaterials = 1;
        
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
        int bpIdx = CreateBuildBlueprint(4, 1, 0);
        blueprints[bpIdx].state = BLUEPRINT_READY_TO_BUILD;
        blueprints[bpIdx].deliveredMaterials = 1;
        
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
        int bpIdx = CreateBuildBlueprint(4, 1, 0);
        
        // Spawn a stone blocks item (building material)
        int itemIdx = SpawnItem(CELL_SIZE * 2.5f, CELL_SIZE * 1.5f, 0.0f, ITEM_STONE_BLOCKS);
        (void)itemIdx;  // Suppress unused warning
        
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
        int bpIdx = CreateBuildBlueprint(4, 1, 0);
        blueprints[bpIdx].state = BLUEPRINT_READY_TO_BUILD;
        blueprints[bpIdx].deliveredMaterials = 1;
        
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
        int bpIdx = CreateBuildBlueprint(4, 1, 0);
        expect(blueprints[bpIdx].state == BLUEPRINT_AWAITING_MATERIALS);
        
        // Spawn a stone blocks item (building material)
        SpawnItem(CELL_SIZE * 2.5f, CELL_SIZE * 1.5f, 0.0f, ITEM_STONE_BLOCKS);
        
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

int main(int argc, char* argv[]) {
    // Suppress logs by default, use -v for verbose
    bool verbose = false;
    bool legacyMode = false;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'v') verbose = true;
        if (strcmp(argv[i], "--legacy") == 0) legacyMode = true;
    }
    if (!verbose) {
        SetTraceLogLevel(LOG_NONE);
    }
    
    // Standard (DF-style) walkability is the default
    g_legacyWalkability = legacyMode;

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
    
    // Item spatial grid (optimization)
    test(item_spatial_grid);
    
    // Cell-based stockpile operations
    test(stockpile_cell_operations);
    
    // Mining/digging tests
    test(mining_designation);
    test(mining_job_assignment);
    test(mining_job_execution);
    test(mining_multiple_designations);

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
    
    return summary();
}
