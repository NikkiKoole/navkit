// bench_rehaul_mini.c - Minimal rehaul-only benchmark for bisecting regression
// Build: make bench_rehaul_mini
// Run: ./build/bin/bench_rehaul_mini
//
// Baseline (commit a91ef60, Phase 5, Feb 2026):
//
// | Scenario                 | Time (ms) |
// |--------------------------|-----------|
// | 10 movers steady         | 2.9       |
// | 10 movers filter change  | 19956     |
//
// Pre-containers (95a869b) was already ~35s — no regression from containers.
// Phase 3 freeSlotCount early exit improved it to ~21s.

#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/world/pathfinding.h"
#include "../src/entities/mover.h"
#include "../src/entities/items.h"
#include "../src/entities/jobs.h"
#include "../src/entities/stockpiles.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static double GetBenchTime(void) {
    return (double)clock() / CLOCKS_PER_SEC;
}

int main(void) {
    SetTraceLogLevel(LOG_NONE);

    // Setup grid
    InitGridWithSizeAndChunkSize(100, 100, 10, 10);
    ClearMovers();
    ClearItems();
    ClearStockpiles();
    InitItemSpatialGrid(100, 100, 4);
    moverPathAlgorithm = PATH_ALGO_ASTAR;

    // Create 3 stockpiles
    int spRed = CreateStockpile(5, 5, 0, 10, 10);
    int spGreen = CreateStockpile(20, 5, 0, 10, 10);
    int spBlue = CreateStockpile(35, 5, 0, 15, 15);

    SetStockpileFilter(spRed, ITEM_RED, true);
    SetStockpileFilter(spRed, ITEM_GREEN, false);
    SetStockpileFilter(spRed, ITEM_BLUE, false);
    SetStockpileFilter(spGreen, ITEM_RED, false);
    SetStockpileFilter(spGreen, ITEM_GREEN, true);
    SetStockpileFilter(spGreen, ITEM_BLUE, false);
    SetStockpileFilter(spBlue, ITEM_RED, false);
    SetStockpileFilter(spBlue, ITEM_GREEN, false);
    SetStockpileFilter(spBlue, ITEM_BLUE, true);

    // Fill stockpiles with items
    int itemIdx = 0;
    for (int ly = 0; ly < 8 && itemIdx < 80; ly++) {
        for (int lx = 0; lx < 10 && itemIdx < 80; lx++) {
            int slotX = 5 + lx;
            int slotY = 5 + ly;
            float x = slotX * CELL_SIZE + CELL_SIZE / 2;
            float y = slotY * CELL_SIZE + CELL_SIZE / 2;
            int idx = SpawnItem(x, y, 0, ITEM_RED);
            items[idx].state = ITEM_IN_STOCKPILE;
            PlaceItemInStockpile(spRed, slotX, slotY, idx);
            itemIdx++;
        }
    }
    for (int ly = 0; ly < 8 && itemIdx < 160; ly++) {
        for (int lx = 0; lx < 10 && itemIdx < 160; lx++) {
            int slotX = 20 + lx;
            int slotY = 5 + ly;
            float x = slotX * CELL_SIZE + CELL_SIZE / 2;
            float y = slotY * CELL_SIZE + CELL_SIZE / 2;
            int idx = SpawnItem(x, y, 0, ITEM_GREEN);
            items[idx].state = ITEM_IN_STOCKPILE;
            PlaceItemInStockpile(spGreen, slotX, slotY, idx);
            itemIdx++;
        }
    }
    for (int ly = 0; ly < 8 && itemIdx < 256; ly++) {
        for (int lx = 0; lx < 12 && itemIdx < 256; lx++) {
            int slotX = 35 + lx;
            int slotY = 5 + ly;
            float x = slotX * CELL_SIZE + CELL_SIZE / 2;
            float y = slotY * CELL_SIZE + CELL_SIZE / 2;
            int idx = SpawnItem(x, y, 0, ITEM_BLUE);
            items[idx].state = ITEM_IN_STOCKPILE;
            PlaceItemInStockpile(spBlue, slotX, slotY, idx);
            itemIdx++;
        }
    }

    printf("Setup: %d items in 3 stockpiles\n", itemIdx);

    // 10 movers
    for (int i = 0; i < 10; i++) {
        float mx = 50 * CELL_SIZE + CELL_SIZE / 2;
        float my = 50 * CELL_SIZE + CELL_SIZE / 2;
        Mover* m = &movers[moverCount];
        Point goal = {50, 50, 0};
        InitMover(m, mx, my, 0, goal, 100.0f);
        moverCount++;
    }
    BuildItemSpatialGrid();

    // Benchmark: Steady state (10 rounds)
    int numIterations = 10;
    double steadyStart = GetBenchTime();
    for (int iter = 0; iter < numIterations; iter++) {
        for (int m = 0; m < 10; m++) {
            if (movers[m].currentJobId >= 0) ReleaseJob(movers[m].currentJobId);
            movers[m].currentJobId = -1;
        }
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (items[i].active) items[i].reservedBy = -1;
        }
        for (int m = 0; m < 10; m++) ReleaseAllSlotsForMover(m);
        AssignJobs();
    }
    double steadyTime = (GetBenchTime() - steadyStart) * 1000.0;

    // Reset
    for (int m = 0; m < 10; m++) {
        if (movers[m].currentJobId >= 0) ReleaseJob(movers[m].currentJobId);
        movers[m].currentJobId = -1;
    }
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (items[i].active) items[i].reservedBy = -1;
    }

    // Simulate filter change
    SetStockpileFilter(spBlue, ITEM_RED, true);
    SetStockpileFilter(spRed, ITEM_RED, false);

    // Benchmark: After filter change (10 rounds)
    double rehaulStart = GetBenchTime();
    for (int iter = 0; iter < numIterations; iter++) {
        for (int m = 0; m < 10; m++) {
            if (movers[m].currentJobId >= 0) ReleaseJob(movers[m].currentJobId);
            movers[m].currentJobId = -1;
        }
        for (int i = 0; i < 256; i++) {
            if (items[i].active) ReleaseItemReservation(i);
        }
        for (int m = 0; m < 10; m++) ReleaseAllSlotsForMover(m);
        AssignJobs();
    }
    double rehaulTime = (GetBenchTime() - rehaulStart) * 1000.0;

    printf("10 movers: Steady=%.1fms  FilterChange=%.1fms  (%.1fx slower)\n",
           steadyTime, rehaulTime, (steadyTime > 0.001) ? rehaulTime / steadyTime : 0);

    FreeItemSpatialGrid();
    return 0;
}
