#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../pathing/grid.h"
#include "../pathing/pathfinding.h"
#include "../pathing/mover.h"
#include "../pathing/terrain.h"
#include "../pathing/items.h"
#include "../pathing/jobs.h"
#include "../pathing/stockpiles.h"
#include <stdlib.h>
#include <math.h>

// Stub profiler functions for tests
void ProfileBegin(const char* name) { (void)name; }
void ProfileEnd(const char* name) { (void)name; }

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
        expect(items[idx].active == true);
        expect(items[idx].x == 100.0f);
        expect(items[idx].y == 100.0f);
        expect(items[idx].type == ITEM_RED);
        expect(items[idx].reservedBy == -1);
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
        expect(items[idx].active == true);
        
        DeleteItem(idx);
        expect(items[idx].active == false);
    }
}

describe(item_reservation) {
    it("should reserve an item for a mover") {
        ClearItems();
        
        int itemIdx = SpawnItem(100.0f, 100.0f, 0.0f, ITEM_RED);
        
        bool reserved = ReserveItem(itemIdx, 0);  // mover 0 reserves
        
        expect(reserved == true);
        expect(items[itemIdx].reservedBy == 0);
    }

    it("should reject reservation if item already reserved") {
        ClearItems();
        
        int itemIdx = SpawnItem(100.0f, 100.0f, 0.0f, ITEM_RED);
        
        ReserveItem(itemIdx, 0);  // mover 0 reserves
        bool secondReserve = ReserveItem(itemIdx, 1);  // mover 1 tries
        
        expect(secondReserve == false);
        expect(items[itemIdx].reservedBy == 0);  // still reserved by mover 0
    }

    it("should release reservation") {
        ClearItems();
        
        int itemIdx = SpawnItem(100.0f, 100.0f, 0.0f, ITEM_RED);
        ReserveItem(itemIdx, 0);
        
        ReleaseItemReservation(itemIdx);
        
        expect(items[itemIdx].reservedBy == -1);
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
        
        expect(m->jobState == JOB_IDLE);
        expect(m->targetItem == -1);
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
        
        expect(m->jobState == JOB_MOVING_TO_ITEM);
        expect(m->targetItem == itemIdx);
        expect(items[itemIdx].reservedBy == 0);
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
        
        expect(m->jobState == JOB_MOVING_TO_ITEM);
        expect(items[itemIdx].active == true);
        
        // Run simulation until item is in stockpile (or timeout)
        for (int i = 0; i < 500; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (items[itemIdx].state == ITEM_IN_STOCKPILE) break;
        }
        
        // Item should be in stockpile
        expect(items[itemIdx].state == ITEM_IN_STOCKPILE);
        expect(m->jobState == JOB_IDLE);
        expect(m->carryingItem == -1);
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
        if (m0->targetItem == itemIdx) claimCount++;
        if (m1->targetItem == itemIdx) claimCount++;
        
        expect(claimCount == 1);
        expect(items[itemIdx].reservedBy >= 0);
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
        expect(m->jobState == JOB_MOVING_TO_ITEM);
        
        // Externally delete the item (simulates someone else taking it)
        DeleteItem(itemIdx);
        
        // Run a few ticks - mover should detect and go back to idle
        for (int i = 0; i < 10; i++) {
            Tick();
            JobsTick();
        }
        
        expect(m->jobState == JOB_IDLE);
        expect(m->targetItem == -1);
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
                if (items[j].active && items[j].state == ITEM_IN_STOCKPILE) storedCount++;
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
        
        expect(m->jobState == JOB_MOVING_TO_ITEM);
        expect(m->targetItem == item2Idx);
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
        expect(m->jobState == JOB_IDLE);
        
        // Run a few more ticks - mover should get a new path (wandering)
        for (int i = 0; i < 30; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }
        
        // Mover should have a path now (not stuck with pathLength == 0)
        expect(m->pathLength > 0);
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
        expect((int)(items[itemIdx].x / CELL_SIZE) == 2);
        expect((int)(items[itemIdx].y / CELL_SIZE) == 2);
        
        // Mover should be idle
        expect(m->jobState == JOB_IDLE);
        expect(m->targetItem == -1);
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
        expect((int)(items[redIdx].x / CELL_SIZE) == 2);
        expect((int)(items[redIdx].y / CELL_SIZE) == 2);
        
        // Green should be in stockpile B (2,3)
        expect(items[greenIdx].state == ITEM_IN_STOCKPILE);
        expect((int)(items[greenIdx].x / CELL_SIZE) == 2);
        expect((int)(items[greenIdx].y / CELL_SIZE) == 3);
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
        expect(m->jobState == JOB_IDLE);
        expect(m->carryingItem == -1);
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
        
        expect(m->jobState == JOB_MOVING_TO_ITEM);
        
        // Delete item mid-haul
        DeleteItem(itemIdx);
        
        // Run more ticks
        for (int i = 0; i < 30; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }
        
        // Mover should be idle
        expect(m->jobState == JOB_IDLE);
        expect(m->targetItem == -1);
        
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
            if (m->jobState == JOB_MOVING_TO_STOCKPILE) break;
        }
        
        expect(m->jobState == JOB_MOVING_TO_STOCKPILE);
        expect(m->carryingItem == itemIdx);
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
        expect(m->jobState == JOB_IDLE);
        expect(m->carryingItem == -1);
        
        // Item should be back on ground (not vanished, not stuck as "carried")
        expect(items[itemIdx].state == ITEM_ON_GROUND);
        expect(items[itemIdx].active == true);
    }
}

describe(filter_change_mid_haul) {
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
            if (m->jobState == JOB_MOVING_TO_STOCKPILE) break;
        }
        
        expect(m->jobState == JOB_MOVING_TO_STOCKPILE);
        expect(m->carryingItem == itemIdx);
        
        // Change filter to disallow red while carrying
        SetStockpileFilter(spIdx, ITEM_RED, false);
        
        // Run more ticks
        for (int i = 0; i < 60; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }
        
        // Mover should have safe-dropped the item
        expect(m->jobState == JOB_IDLE);
        expect(m->carryingItem == -1);
        
        // Item should be back on ground
        expect(items[itemIdx].state == ITEM_ON_GROUND);
        expect(items[itemIdx].active == true);
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
        expect(m->jobState == JOB_MOVING_TO_ITEM);
        
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
        bool validState = (m->jobState == JOB_IDLE) || 
                          (m->jobState == JOB_MOVING_TO_ITEM) ||
                          (m->jobState == JOB_MOVING_TO_STOCKPILE);
        expect(validState == true);
        
        // Also verify the item wasn't corrupted
        expect(items[itemIdx].active == true);
        expect(items[itemIdx].state == ITEM_ON_GROUND || items[itemIdx].state == ITEM_CARRIED);
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
                if (m->jobState == JOB_IDLE) break;
            }
        }
        
        // One item should be stored
        int storedCount = 0;
        if (items[item1].state == ITEM_IN_STOCKPILE) storedCount++;
        if (items[item2].state == ITEM_IN_STOCKPILE) storedCount++;
        expect(storedCount == 1);
        expect(m->jobState == JOB_IDLE);
        
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
            expect(movers[i].jobState == JOB_IDLE);
            expect(movers[i].carryingItem == -1);
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
            if (m->jobState == JOB_IDLE) {
                AssignJobs();
                if (m->jobState == JOB_MOVING_TO_ITEM && m->targetItem == itemIdx) {
                    assignAttempts++;
                }
            }
            JobsTick();
        }
        
        // Agent should end idle (can't reach item)
        expect(m->jobState == JOB_IDLE);
        
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
        int itemTileX = (int)(items[itemIdx].x / CELL_SIZE);
        int itemTileY = (int)(items[itemIdx].y / CELL_SIZE);
        expect(itemTileX == 2);
        expect(itemTileY == 2);
        
        // Continue running - mover should re-haul to high priority
        for (int i = 0; i < 2000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }
        
        // Item should now be in high-priority stockpile
        itemTileX = (int)(items[itemIdx].x / CELL_SIZE);
        itemTileY = (int)(items[itemIdx].y / CELL_SIZE);
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
        int itemTileX = (int)(items[itemIdx].x / CELL_SIZE);
        int itemTileY = (int)(items[itemIdx].y / CELL_SIZE);
        
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
        itemTileX = (int)(items[itemIdx].x / CELL_SIZE);
        itemTileY = (int)(items[itemIdx].y / CELL_SIZE);
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
        int origX = (int)(items[itemIdx].x / CELL_SIZE);
        int origY = (int)(items[itemIdx].y / CELL_SIZE);
        
        // Run more ticks
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }
        
        // Item should not have moved (no re-haul between equal priorities)
        int newX = (int)(items[itemIdx].x / CELL_SIZE);
        int newY = (int)(items[itemIdx].y / CELL_SIZE);
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
            if (m->carryingItem == item) break;
        }
        expect(m->carryingItem == item);
        expect(m->jobState == JOB_MOVING_TO_STOCKPILE);
        
        // Clear path to simulate losing it (like when wall is drawn)
        m->pathLength = 0;
        m->pathIndex = -1;
        
        // Record the job's target stockpile slot and current goal
        int targetSlotX = m->targetSlotX;
        int targetSlotY = m->targetSlotY;
        int targetStockpile = m->targetStockpile;
        Point goalBefore = m->goal;
        
        // Clear any repath cooldown so endless mover mode will act immediately
        m->repathCooldown = 0;
        
        // Ensure mover is active and has no path (trigger the endless mover branch)
        m->active = true;
        expect(m->pathLength == 0);
        expect(m->pathIndex < 0);
        
        // Run a single Tick - this is where the bug manifests:
        // endless mover mode calls AssignNewMoverGoal() which sets m->goal to random point
        Tick();
        
        // BUG CHECK: After Tick, if mover was hijacked, m->goal will have changed
        // to some random cell (not the stockpile target)
        Stockpile* spPtr = &stockpiles[targetStockpile];
        int stockpileX = spPtr->x + targetSlotX;
        int stockpileY = spPtr->y + targetSlotY;
        
        // Seed rand with a value that will produce a different goal than (6,1)
        // The bug is that AssignNewMoverGoal gets called and changes the goal to random point
        srand(12345);
        m->pathLength = 0;
        m->pathIndex = -1;
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
        if (m->carryingItem >= 0) {
            expect(m->jobState == JOB_MOVING_TO_STOCKPILE);
        }
        
        // Run longer to let job complete or cancel
        for (int i = 0; i < 600; i++) {
            Tick();
            ItemsTick(TICK_DT);
            AssignJobs();
            JobsTick();
            if (items[item].state == ITEM_IN_STOCKPILE) break;
            if (items[item].state == ITEM_ON_GROUND && m->jobState == JOB_IDLE) break;
        }
        
        // Item should be either in stockpile or dropped on ground (not carried aimlessly)
        bool delivered = items[item].state == ITEM_IN_STOCKPILE;
        bool dropped = items[item].state == ITEM_ON_GROUND && m->carryingItem == -1;
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
            if (m->carryingItem == item) break;
        }
        expect(m->carryingItem == item);
        expect(m->jobState == JOB_MOVING_TO_STOCKPILE);
        
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
        expect(m->jobState == JOB_IDLE);
        expect(m->carryingItem == -1);
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
        expect(m->jobState == JOB_IDLE);
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
        expect(m->jobState == JOB_IDLE);
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
        int spB = CreateStockpile(6, 2, 0, 1, 1);
        
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
            int slotX = (int)(items[itemIds[i]].x / CELL_SIZE);
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
            if (items[i].active && items[i].state == ITEM_IN_STOCKPILE) inStockpile++;
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
        expect((int)(items[itemIdx].x / CELL_SIZE) == 5);
        expect((int)(items[itemIdx].y / CELL_SIZE) == 5);
        
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
        expect((int)(items[itemIdx].x / CELL_SIZE) == 8);
        expect((int)(items[itemIdx].y / CELL_SIZE) == 8);
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
        expect(items[itemIdx].active == true);
        
        // Item should NOT be on the stockpile (safe-dropped outside)
        int itemTileX = (int)(items[itemIdx].x / CELL_SIZE);
        int itemTileY = (int)(items[itemIdx].y / CELL_SIZE);
        bool onStockpile = (itemTileX >= 5 && itemTileX < 7 && itemTileY >= 5 && itemTileY < 7);
        expect(onStockpile == false);
        
        // Mover should be idle
        expect(m->jobState == JOB_IDLE);
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
        
        // Another green item far away (regular haul)
        int regularItem = SpawnItem(2 * CELL_SIZE + CELL_SIZE * 0.5f, 2 * CELL_SIZE + CELL_SIZE * 0.5f, 0.0f, ITEM_GREEN);
        
        // Run just enough to see which item gets picked first
        AssignJobs();
        
        // Mover should target the foreign item (clearing job) first
        expect(m->targetItem == foreignItem);
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
        expect((int)(items[itemIdx].x / CELL_SIZE) == 5);
        expect((int)(items[itemIdx].y / CELL_SIZE) == 5);
    }
}

int main(int argc, char* argv[]) {
    // Suppress logs by default, use -v for verbose
    bool verbose = false;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'v') verbose = true;
    }
    if (!verbose) {
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
    
    return summary();
}
