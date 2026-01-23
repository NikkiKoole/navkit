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
            "........\n", 8, 8);
        
        ClearMovers();
        ClearItems();
        ClearStockpiles();
        
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 16.0f, 16.0f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        int itemIdx = SpawnItem(200.0f, 200.0f, 0.0f, ITEM_RED);
        
        // Need a stockpile for job assignment to work
        int spIdx = CreateStockpile(1, 1, 0, 1, 1);
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
        
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 16.0f, 16.0f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        int itemIdx = SpawnItem(200.0f, 200.0f, 0.0f, ITEM_RED);
        
        // Need stockpile
        int spIdx = CreateStockpile(8, 2, 0, 1, 1);
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
        
        // Stockpile with only 1 tile
        int spIdx = CreateStockpile(2, 2, 0, 1, 1);
        SetStockpileFilter(spIdx, ITEM_RED, true);
        
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
        
        // No two items should be at the same position
        for (int i = 0; i < 3; i++) {
            for (int j = i + 1; j < 3; j++) {
                bool samePos = (int)(items[itemIdxs[i]].x / CELL_SIZE) == (int)(items[itemIdxs[j]].x / CELL_SIZE) &&
                               (int)(items[itemIdxs[i]].y / CELL_SIZE) == (int)(items[itemIdxs[j]].y / CELL_SIZE);
                expect(samePos == false);
            }
        }
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
    return summary();
}
