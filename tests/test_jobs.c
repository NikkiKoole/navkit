#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../pathing/grid.h"
#include "../pathing/pathfinding.h"
#include "../pathing/mover.h"
#include "../pathing/terrain.h"
#include "../pathing/items.h"
#include "../pathing/jobs.h"
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
            "........\n", 4, 4);
        
        ClearMovers();
        ClearItems();
        
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 16.0f, 16.0f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        int itemIdx = SpawnItem(200.0f, 200.0f, 0.0f, ITEM_RED);
        
        AssignJobs();  // should assign item to idle mover
        
        expect(m->jobState == JOB_MOVING_TO_ITEM);
        expect(m->targetItem == itemIdx);
        expect(items[itemIdx].reservedBy == 0);
    }
}

describe(pickup_behavior) {
    it("should pick up item when mover arrives") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 4);
        
        // Use A* for tests (doesn't require HPA graph building)
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        
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
        
        AssignJobs();
        
        expect(m->jobState == JOB_MOVING_TO_ITEM);
        expect(items[itemIdx].active == true);
        
        // Run simulation until mover reaches item (or timeout)
        for (int i = 0; i < 300; i++) {  // ~5 seconds at 60 ticks/sec
            Tick();
            JobsTick();
            if (items[itemIdx].active == false) break;
        }
        
        // Item should be picked up (deleted)
        expect(items[itemIdx].active == false);
        expect(m->jobState == JOB_IDLE);
        expect(m->targetItem == -1);
    }
}

describe(reservation_safety) {
    it("should not allow two movers to claim the same item") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 4);
        
        ClearMovers();
        ClearItems();
        
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
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 4);
        
        ClearMovers();
        ClearItems();
        
        Mover* m = &movers[0];
        Point goal = {0, 0, 0};
        InitMover(m, 16.0f, 16.0f, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        int itemIdx = SpawnItem(200.0f, 200.0f, 0.0f, ITEM_RED);
        
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
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 4);
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        
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
        
        expect(itemCount == 2);
        
        // Run until first item picked up
        for (int i = 0; i < 300; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (itemCount == 1) break;
        }
        
        expect(itemCount == 1);
        
        // Mover should now be going for the second item
        // Give it a few ticks to get assigned
        for (int i = 0; i < 10; i++) {
            Tick();
            AssignJobs();
            JobsTick();
        }
        
        expect(m->jobState == JOB_MOVING_TO_ITEM);
        expect(m->targetItem == item2Idx);
    }

    it("should resume wandering when no more items exist") {
        InitGridFromAsciiWithChunkSize(
            "........\n"
            "........\n"
            "........\n"
            "........\n", 8, 4);
        
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        endlessMoverMode = true;
        
        ClearMovers();
        ClearItems();
        
        // Mover at (1,1), one item nearby
        float moverX = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        float moverY = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        float itemX = 2 * CELL_SIZE + CELL_SIZE * 0.5f;
        float itemY = 1 * CELL_SIZE + CELL_SIZE * 0.5f;
        
        Mover* m = &movers[0];
        Point goal = {1, 1, 0};
        InitMover(m, moverX, moverY, 0.0f, goal, 100.0f);
        moverCount = 1;
        
        SpawnItem(itemX, itemY, 0.0f, ITEM_RED);
        
        // Run until item picked up
        for (int i = 0; i < 300; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if (itemCount == 0) break;
        }
        
        expect(itemCount == 0);
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
    return summary();
}
