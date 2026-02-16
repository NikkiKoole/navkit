// bench_items.c - Item system benchmarks (baseline before containers feature)
//
// Run with: make bench_items
// Or: ./bin/bench_items
//
// Purpose: Capture baseline performance of item-related hot paths before
// the containers & stacking refactor. Re-run after each phase to verify
// no regressions (and measure improvements from stackCount consolidation).
//
// Baseline (commit 95a869b, Feb 2026, pre-containers):
//
// | Benchmark                          | Value              |
// |------------------------------------|--------------------|
// | SpatialGrid rebuild  100 items     | 148us each         |
// | SpatialGrid rebuild  10000 items   | 158us each         |
// | Linear scan  100 items             | 0.08us each        |
// | Linear scan  10000 items           | 7.9us each         |
// | AssignJobs haul  50 items/10 mov   | 242ms per round    |
// | AssignJobs haul  200 items/10 mov  | 559ms per round    |
// | AssignJobs haul  500 items/10 mov  | 456ms per round    |
// | Stockpile cache rebuild            | 489us each         |
// | Stockpile cache lookup             | 2.4ns each         |
// | Craft input search  100 items      | 0.05us each        |
// | Craft input search  5000 items     | 2.7us each         |
//
// Key: linear scan and craft search scale linearly with highWaterMark.
// After stacking, fewer Item structs = proportionally faster.

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

static void SetupBenchGrid(void) {
    InitGridWithSizeAndChunkSize(100, 100, 10, 10);
}

// =============================================================================
// 1. BuildItemSpatialGrid - how fast we rebuild the spatial index
//    After stacking: fewer ITEM_ON_GROUND structs = faster rebuild
// =============================================================================
static void BenchBuildSpatialGrid(void) {
    printf("--- BuildItemSpatialGrid ---\n");

    SetupBenchGrid();
    InitItemSpatialGrid(100, 100, 4);

    int itemCounts[] = {100, 500, 2000, 5000, 10000};
    int numCounts = sizeof(itemCounts) / sizeof(itemCounts[0]);

    for (int c = 0; c < numCounts; c++) {
        int targetCount = itemCounts[c];
        ClearItems();

        SetRandomSeed(12345);
        for (int i = 0; i < targetCount; i++) {
            float x = GetRandomValue(0, 99) * CELL_SIZE + CELL_SIZE * 0.5f;
            float y = GetRandomValue(0, 99) * CELL_SIZE + CELL_SIZE * 0.5f;
            SpawnItem(x, y, 0.0f, GetRandomValue(0, 2));
        }

        int numIterations = 1000;
        double start = GetBenchTime();
        for (int iter = 0; iter < numIterations; iter++) {
            BuildItemSpatialGrid();
        }
        double elapsed = (GetBenchTime() - start) * 1000.0;

        printf("  %5d items: %8.3fms (%d rebuilds, %.3fus each)\n",
               targetCount, elapsed, numIterations,
               (elapsed / numIterations) * 1000.0);
    }

    FreeItemSpatialGrid();
    printf("\n");
}

// =============================================================================
// 2. Item linear scan - iterating items[0..highWaterMark] with filter checks
//    Simulates what IsItemHaulable + type check costs across many items.
//    After stacking: fewer structs = fewer iterations.
// =============================================================================
static void BenchItemLinearScan(void) {
    printf("--- Item linear scan (IsItemHaulable pattern) ---\n");

    SetupBenchGrid();

    int itemCounts[] = {100, 500, 2000, 5000, 10000};
    int numCounts = sizeof(itemCounts) / sizeof(itemCounts[0]);

    for (int c = 0; c < numCounts; c++) {
        int targetCount = itemCounts[c];
        ClearItems();

        SetRandomSeed(12345);
        for (int i = 0; i < targetCount; i++) {
            float x = GetRandomValue(1, 98) * CELL_SIZE + CELL_SIZE * 0.5f;
            float y = GetRandomValue(1, 98) * CELL_SIZE + CELL_SIZE * 0.5f;
            SpawnItem(x, y, 0.0f, (ItemType)(i % 3));
        }

        int numIterations = 10000;
        volatile int found = 0;
        double start = GetBenchTime();
        for (int iter = 0; iter < numIterations; iter++) {
            for (int i = 0; i < itemHighWaterMark; i++) {
                Item* item = &items[i];
                if (!item->active) continue;
                if (item->reservedBy != -1) continue;
                if (item->state != ITEM_ON_GROUND) continue;
                if (item->type == ITEM_RED) found++;
            }
        }
        double elapsed = (GetBenchTime() - start) * 1000.0;
        (void)found;

        printf("  %5d items (hwm=%5d): %8.3fms (%d scans, %.3fus each)\n",
               targetCount, itemHighWaterMark, elapsed, numIterations,
               (elapsed / numIterations) * 1000.0);
    }

    printf("\n");
}

// =============================================================================
// 3. AssignJobs P3 haul throughput - the main haul hot path
//    Many items on ground, multiple stockpiles accepting different types.
//    After stacking: fewer item structs to iterate in P3 spatial/linear scan.
// =============================================================================
static void BenchAssignJobsHaul(void) {
    printf("--- AssignJobs P3 Haul (many items, 3 stockpiles) ---\n");

    SetupBenchGrid();
    ClearMovers();
    ClearItems();
    ClearStockpiles();
    InitItemSpatialGrid(100, 100, 4);
    moverPathAlgorithm = PATH_ALGO_ASTAR;

    // 3 stockpiles for 3 item types
    int spRed = CreateStockpile(80, 10, 0, 10, 10);
    int spGreen = CreateStockpile(80, 30, 0, 10, 10);
    int spBlue = CreateStockpile(80, 50, 0, 10, 10);

    SetStockpileFilter(spRed, ITEM_RED, true);
    SetStockpileFilter(spGreen, ITEM_GREEN, true);
    SetStockpileFilter(spBlue, ITEM_BLUE, true);

    int itemCounts[] = {50, 200, 500};
    int numItemCounts = sizeof(itemCounts) / sizeof(itemCounts[0]);

    for (int ic = 0; ic < numItemCounts; ic++) {
        int targetItems = itemCounts[ic];

        // Reset state
        ClearItems();
        ClearMovers();
        ClearJobs();

        // Spawn items scattered across the map
        SetRandomSeed(99999);
        for (int i = 0; i < targetItems; i++) {
            float x = GetRandomValue(1, 70) * CELL_SIZE + CELL_SIZE * 0.5f;
            float y = GetRandomValue(1, 70) * CELL_SIZE + CELL_SIZE * 0.5f;
            SpawnItem(x, y, 0.0f, (ItemType)(i % 3));
        }

        // 10 idle movers
        for (int i = 0; i < 10; i++) {
            float mx = GetRandomValue(10, 60) * CELL_SIZE + CELL_SIZE * 0.5f;
            float my = GetRandomValue(10, 60) * CELL_SIZE + CELL_SIZE * 0.5f;
            Mover* m = &movers[moverCount];
            Point goal = {(int)(mx / CELL_SIZE), (int)(my / CELL_SIZE), 0};
            InitMover(m, mx, my, 0, goal, 100.0f);
            moverCount++;
        }

        BuildItemSpatialGrid();

        int numIterations = 10;
        double start = GetBenchTime();
        for (int iter = 0; iter < numIterations; iter++) {
            // Reset movers and items for fresh assignment each iteration
            for (int m = 0; m < 10; m++) {
                if (movers[m].currentJobId >= 0) ReleaseJob(movers[m].currentJobId);
                movers[m].currentJobId = -1;
            }
            for (int i = 0; i < itemHighWaterMark; i++) {
                if (items[i].active) {
                    items[i].reservedBy = -1;
                    items[i].unreachableCooldown = 0.0f;
                }
            }
            for (int m = 0; m < 10; m++) {
                ReleaseAllSlotsForMover(m);
            }

            AssignJobs();
        }
        double elapsed = (GetBenchTime() - start) * 1000.0;

        printf("  %4d items, 10 movers: %8.3fms (%d rounds, %.3fms each)\n",
               targetItems, elapsed, numIterations,
               elapsed / numIterations);
    }

    FreeItemSpatialGrid();
    printf("\n");
}

// =============================================================================
// 4. Stockpile slot cache rebuild + lookup
//    After containers: cache adds containerSlotCache but existing path unchanged.
// =============================================================================
static void BenchStockpileCache(void) {
    printf("--- Stockpile slot cache (rebuild + lookup) ---\n");

    SetupBenchGrid();
    ClearItems();
    ClearStockpiles();

    // Create stockpiles accepting various types
    int sp1 = CreateStockpile(10, 10, 0, 8, 8);
    int sp2 = CreateStockpile(30, 10, 0, 8, 8);
    int sp3 = CreateStockpile(50, 10, 0, 8, 8);
    int sp4 = CreateStockpile(10, 30, 0, 8, 8);

    SetStockpileFilter(sp1, ITEM_RED, true);
    SetStockpileFilter(sp1, ITEM_LOG, true);
    SetStockpileFilter(sp1, ITEM_ROCK, true);
    SetStockpileFilter(sp2, ITEM_GREEN, true);
    SetStockpileFilter(sp2, ITEM_PLANKS, true);
    SetStockpileFilter(sp3, ITEM_BLUE, true);
    SetStockpileFilter(sp3, ITEM_GRASS, true);
    SetStockpileFilter(sp3, ITEM_CORDAGE, true);
    SetStockpileFilter(sp4, ITEM_CLAY, true);
    SetStockpileFilter(sp4, ITEM_BRICKS, true);

    // Benchmark cache rebuild
    int numRebuilds = 10000;
    double rebuildStart = GetBenchTime();
    for (int iter = 0; iter < numRebuilds; iter++) {
        InvalidateStockpileSlotCacheAll();
        RebuildStockpileSlotCache();
    }
    double rebuildTime = (GetBenchTime() - rebuildStart) * 1000.0;

    printf("  Cache rebuild: %8.3fms (%d rebuilds, %.3fus each)\n",
           rebuildTime, numRebuilds, (rebuildTime / numRebuilds) * 1000.0);

    // Benchmark cached lookups
    int numLookups = 100000;
    volatile int foundCount = 0;
    RebuildStockpileSlotCache();
    double lookupStart = GetBenchTime();
    for (int iter = 0; iter < numLookups; iter++) {
        int slotX, slotY;
        ItemType type = (ItemType)(iter % ITEM_TYPE_COUNT);
        int sp = FindStockpileForItemCached(type, MAT_NONE, &slotX, &slotY);
        if (sp >= 0) foundCount++;
    }
    double lookupTime = (GetBenchTime() - lookupStart) * 1000.0;
    (void)foundCount;

    printf("  Cache lookup:  %8.3fms (%d lookups, %.3fns each)\n",
           lookupTime, numLookups, (lookupTime / numLookups) * 1000000.0);

    printf("\n");
}

// =============================================================================
// 5. Craft input search (WorkGiver_Craft linear scan pattern)
//    Simulates the linear scan that finds recipe inputs.
//    After containers: this scan may also check container contents.
// =============================================================================
static void BenchCraftInputSearch(void) {
    printf("--- Craft input search (linear scan pattern) ---\n");

    SetupBenchGrid();

    int itemCounts[] = {100, 500, 2000, 5000};
    int numCounts = sizeof(itemCounts) / sizeof(itemCounts[0]);

    for (int c = 0; c < numCounts; c++) {
        int targetCount = itemCounts[c];
        ClearItems();

        // Spawn items of various types, scattered
        SetRandomSeed(77777);
        ItemType types[] = {ITEM_LOG, ITEM_PLANKS, ITEM_ROCK, ITEM_CLAY,
                            ITEM_CORDAGE, ITEM_GRASS, ITEM_RED, ITEM_GREEN};
        int numTypes = sizeof(types) / sizeof(types[0]);

        for (int i = 0; i < targetCount; i++) {
            float x = GetRandomValue(1, 98) * CELL_SIZE + CELL_SIZE * 0.5f;
            float y = GetRandomValue(1, 98) * CELL_SIZE + CELL_SIZE * 0.5f;
            SpawnItem(x, y, 0.0f, types[i % numTypes]);
        }

        // Search for a specific type near a workshop-like position
        float wsX = 50.0f, wsY = 50.0f;
        int searchRadius = 100;
        int numIterations = 10000;

        volatile int foundSum = 0;
        double start = GetBenchTime();
        for (int iter = 0; iter < numIterations; iter++) {
            int bestIdx = -1;
            int bestDistSq = searchRadius * searchRadius;

            // Simulate WorkGiver_Craft input search for ITEM_PLANKS
            for (int i = 0; i < itemHighWaterMark; i++) {
                Item* item = &items[i];
                if (!item->active) continue;
                if (item->type != ITEM_PLANKS) continue;
                if (item->reservedBy != -1) continue;
                if (item->state != ITEM_ON_GROUND) continue;

                int dx = (int)(item->x / CELL_SIZE) - (int)(wsX);
                int dy = (int)(item->y / CELL_SIZE) - (int)(wsY);
                int distSq = dx * dx + dy * dy;
                if (distSq < bestDistSq) {
                    bestDistSq = distSq;
                    bestIdx = i;
                }
            }
            if (bestIdx >= 0) foundSum++;
        }
        double elapsed = (GetBenchTime() - start) * 1000.0;
        (void)foundSum;

        printf("  %5d items (hwm=%5d): %8.3fms (%d searches, %.3fus each)\n",
               targetCount, itemHighWaterMark, elapsed, numIterations,
               (elapsed / numIterations) * 1000.0);
    }

    printf("\n");
}

// =============================================================================
// Main
// =============================================================================
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    SetTraceLogLevel(LOG_NONE);

    printf("\n=== ITEM SYSTEM BENCHMARKS (pre-containers baseline) ===\n\n");

    BenchBuildSpatialGrid();
    BenchItemLinearScan();
    BenchAssignJobsHaul();
    BenchStockpileCache();
    BenchCraftInputSearch();

    printf("Done.\n");
    return 0;
}
