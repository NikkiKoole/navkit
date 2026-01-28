// bench_jobs.c - Job system benchmarks
//
// Run with: make bench_jobs
// Or: ./bin/bench_jobs
//
// Historical baseline (commit de7e2a4, Jan 2026):
//
// | Scenario                 | Old (de7e2a4) | Current (f95cc1f) | Improvement |
// |--------------------------|---------------|-------------------|-------------|
// | 10 movers steady         | 349ms         | 31ms              | 11x faster  |
// | 10 movers filter change  | 6216ms        | 3609ms            | 1.7x faster |
// | 50 movers steady         | 349ms         | 31ms              | 11x faster  |
// | 50 movers filter change  | 28899ms       | 17448ms           | 1.7x faster |
//
// Note: The ratio (steady vs filter change) looks worse now because steady state
// improved dramatically. Both scenarios are faster in absolute terms.

#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/world/pathfinding.h"
#include "../src/entities/mover.h"
#include "../src/entities/items.h"
#include "../src/entities/jobs.h"
#include "../src/entities/stockpiles.h"
#include <stdio.h>
#include <time.h>

static double GetBenchTime(void) {
    return (double)clock() / CLOCKS_PER_SEC;
}

// Generate a 100x100 empty grid for benchmarks
static void SetupBenchGrid(void) {
    InitGridWithSizeAndChunkSize(100, 100, 10, 10);
}

// =============================================================================
// FindNearestUnreservedItem benchmark
// =============================================================================
static void BenchFindNearestItem(void) {
    printf("--- FindNearestUnreservedItem ---\n");
    
    SetupBenchGrid();
    
    int itemCounts[] = {100, 1000, 5000, 10000, 20000};
    int numCounts = sizeof(itemCounts) / sizeof(itemCounts[0]);
    
    for (int c = 0; c < numCounts; c++) {
        int targetCount = itemCounts[c];
        ClearItems();
        
        // Spawn items randomly across the grid
        SetRandomSeed(12345);
        for (int i = 0; i < targetCount; i++) {
            float x = GetRandomValue(0, 99) * CELL_SIZE + CELL_SIZE * 0.5f;
            float y = GetRandomValue(0, 99) * CELL_SIZE + CELL_SIZE * 0.5f;
            SpawnItem(x, y, 0.0f, GetRandomValue(0, 2));
        }
        
        // Build spatial grid for optimized version
        BuildItemSpatialGrid();
        
        // Test positions (corners and center)
        float testX[] = {5 * CELL_SIZE, 50 * CELL_SIZE, 95 * CELL_SIZE};
        float testY[] = {5 * CELL_SIZE, 50 * CELL_SIZE, 95 * CELL_SIZE};
        int numTests = 3;
        int numIterations = 1000;
        
        // Benchmark NAIVE
        volatile int naiveSum = 0;
        double naiveStart = GetBenchTime();
        for (int iter = 0; iter < numIterations; iter++) {
            for (int t = 0; t < numTests; t++) {
                naiveSum += FindNearestUnreservedItemNaive(testX[t], testY[t], 0);
            }
        }
        double naiveTime = (GetBenchTime() - naiveStart) * 1000.0;
        (void)naiveSum;
        
        // Benchmark SPATIAL
        volatile int spatialSum = 0;
        double spatialStart = GetBenchTime();
        for (int iter = 0; iter < numIterations; iter++) {
            for (int t = 0; t < numTests; t++) {
                spatialSum += FindNearestUnreservedItem(testX[t], testY[t], 0);
            }
        }
        double spatialTime = (GetBenchTime() - spatialStart) * 1000.0;
        (void)spatialSum;
        
        // Verify both return same result
        bool allMatch = true;
        for (int t = 0; t < numTests; t++) {
            int naiveResult = FindNearestUnreservedItemNaive(testX[t], testY[t], 0);
            int spatialResult = FindNearestUnreservedItem(testX[t], testY[t], 0);
            if (naiveResult != spatialResult) {
                float naiveDist = -1, spatialDist = -1;
                if (naiveResult >= 0) {
                    float dx = items[naiveResult].x - testX[t];
                    float dy = items[naiveResult].y - testY[t];
                    naiveDist = dx*dx + dy*dy;
                }
                if (spatialResult >= 0) {
                    float dx = items[spatialResult].x - testX[t];
                    float dy = items[spatialResult].y - testY[t];
                    spatialDist = dx*dx + dy*dy;
                }
                if (naiveDist != spatialDist) {
                    allMatch = false;
                }
            }
        }
        const char* match = allMatch ? "OK" : "MISMATCH!";
        
        double speedup = (spatialTime > 0.001) ? naiveTime / spatialTime : 0;
        printf("  %5d items: Naive=%8.3fms  Spatial=%8.3fms  Speedup=%6.1fx  [%s]\n",
               targetCount, naiveTime, spatialTime, speedup, match);
    }
    
    printf("\n");
}

// =============================================================================
// ItemsTick benchmark (highWaterMark optimization)
// =============================================================================
static void BenchItemsTick(void) {
    printf("--- ItemsTick (highWaterMark optimization) ---\n");
    
    SetupBenchGrid();
    
    int itemCounts[] = {100, 1000, 5000, 10000, 20000};
    int numCounts = sizeof(itemCounts) / sizeof(itemCounts[0]);
    
    for (int c = 0; c < numCounts; c++) {
        int targetCount = itemCounts[c];
        ClearItems();
        
        SetRandomSeed(12345);
        for (int i = 0; i < targetCount; i++) {
            float x = GetRandomValue(0, 99) * CELL_SIZE + CELL_SIZE * 0.5f;
            float y = GetRandomValue(0, 99) * CELL_SIZE + CELL_SIZE * 0.5f;
            int idx = SpawnItem(x, y, 0.0f, GetRandomValue(0, 2));
            if (idx >= 0 && (i % 3) == 0) {
                items[idx].unreachableCooldown = 5.0f;
            }
        }
        
        int numIterations = 10000;
        float dt = 1.0f / 60.0f;
        
        // Benchmark NAIVE
        volatile float naiveAccum = 0;
        double naiveStart = GetBenchTime();
        for (int iter = 0; iter < numIterations; iter++) {
            ItemsTickNaive(dt);
            naiveAccum += items[0].unreachableCooldown;
        }
        double naiveTime = (GetBenchTime() - naiveStart) * 1000.0;
        (void)naiveAccum;
        
        // Reset cooldowns
        for (int i = 0; i < targetCount; i++) {
            if ((i % 3) == 0) {
                items[i].unreachableCooldown = 5.0f;
            }
        }
        
        // Benchmark OPTIMIZED
        volatile float optAccum = 0;
        double optStart = GetBenchTime();
        for (int iter = 0; iter < numIterations; iter++) {
            ItemsTick(dt);
            optAccum += items[0].unreachableCooldown;
        }
        double optTime = (GetBenchTime() - optStart) * 1000.0;
        (void)optAccum;
        
        double speedup = (optTime > 0.001) ? naiveTime / optTime : 0;
        printf("  %5d items (hwm=%5d): Naive=%8.3fms  Optimized=%8.3fms  Speedup=%6.1fx\n",
               targetCount, itemHighWaterMark, naiveTime, optTime, speedup);
    }
    
    printf("\n");
}

// =============================================================================
// AssignJobs rehaul benchmark (filter change scenario)
// =============================================================================
static void BenchAssignJobsRehaul(void) {
    printf("--- AssignJobs Rehaul (filter change scenario) ---\n");
    
    SetupBenchGrid();
    ClearMovers();
    ClearItems();
    ClearStockpiles();
    InitItemSpatialGrid(100, 100, 4);
    moverPathAlgorithm = PATH_ALGO_ASTAR;
    
    // Create 3 stockpiles
    int spRed = CreateStockpile(5, 5, 0, 10, 10);
    int spGreen = CreateStockpile(20, 5, 0, 10, 10);
    int spBlue = CreateStockpile(35, 5, 0, 15, 15);
    
    // Set filters
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
    
    printf("  Setup: %d items in 3 stockpiles\n", itemIdx);
    
    int moverCounts[] = {10, 50};
    int numMoverCounts = 2;
    
    for (int mc = 0; mc < numMoverCounts; mc++) {
        int targetMovers = moverCounts[mc];
        ClearMovers();
        
        for (int i = 0; i < targetMovers; i++) {
            float mx = 50 * CELL_SIZE + CELL_SIZE / 2;
            float my = 50 * CELL_SIZE + CELL_SIZE / 2;
            Mover* m = &movers[moverCount];
            Point goal = {50, 50, 0};
            InitMover(m, mx, my, 0, goal, 100.0f);
            moverCount++;
        }
        
        BuildItemSpatialGrid();
        
        int numIterations = 100;
        
        // Benchmark: Steady state
        volatile int steadySum = 0;
        double steadyStart = GetBenchTime();
        for (int iter = 0; iter < numIterations; iter++) {
            AssignJobs();
            steadySum += movers[0].currentJobId;
        }
        double steadyTime = (GetBenchTime() - steadyStart) * 1000.0;
        (void)steadySum;
        
        // Reset movers
        for (int i = 0; i < targetMovers; i++) {
            if (movers[i].currentJobId >= 0) {
                ReleaseJob(movers[i].currentJobId);
            }
            movers[i].currentJobId = -1;
        }
        for (int i = 0; i < MAX_ITEMS; i++) {
            if (items[i].active) items[i].reservedBy = -1;
        }
        
        // Simulate filter change
        SetStockpileFilter(spBlue, ITEM_RED, true);
        SetStockpileFilter(spRed, ITEM_RED, false);
        
        // Benchmark: After filter change
        volatile int rehaulSum = 0;
        double rehaulStart = GetBenchTime();
        for (int iter = 0; iter < numIterations; iter++) {
            for (int m = 0; m < targetMovers; m++) {
                if (movers[m].currentJobId >= 0) {
                    ReleaseJob(movers[m].currentJobId);
                }
                movers[m].currentJobId = -1;
            }
            for (int i = 0; i < 256; i++) {
                if (items[i].active) {
                    ReleaseItemReservation(i);
                }
            }
            for (int m = 0; m < targetMovers; m++) {
                ReleaseAllSlotsForMover(m);
            }
            
            AssignJobs();
            rehaulSum += movers[0].currentJobId;
        }
        double rehaulTime = (GetBenchTime() - rehaulStart) * 1000.0;
        (void)rehaulSum;
        
        printf("  %2d movers: Steady=%.3fms  FilterChange=%.3fms  (%.1fx slower)\n",
               targetMovers, steadyTime, rehaulTime,
               (steadyTime > 0.001) ? rehaulTime / steadyTime : 0);
        
        // Reset filters
        SetStockpileFilter(spBlue, ITEM_RED, false);
        SetStockpileFilter(spRed, ITEM_RED, true);
    }
    
    FreeItemSpatialGrid();
    printf("\n");
}

// =============================================================================
// AssignJobs algorithm comparison
// =============================================================================
static void BenchAssignJobsAlgorithms(void) {
    printf("--- AssignJobsLegacy vs AssignJobsWorkGivers vs Hybrid ---\n");
    
    SetupBenchGrid();
    ClearMovers();
    ClearItems();
    ClearStockpiles();
    InitItemSpatialGrid(100, 100, 4);
    
    int sp = CreateStockpile(80, 80, 0, 10, 10);
    SetStockpileFilter(sp, ITEM_RED, true);
    
    SetRandomSeed(54321);
    for (int i = 0; i < 500; i++) {
        float x = GetRandomValue(5, 70) * CELL_SIZE + CELL_SIZE * 0.5f;
        float y = GetRandomValue(5, 70) * CELL_SIZE + CELL_SIZE * 0.5f;
        SpawnItem(x, y, 0.0f, ITEM_RED);
    }
    BuildItemSpatialGrid();
    
    int moverTestCounts[] = {10, 50, 100};
    int numMoverTests = 3;
    
    for (int mc = 0; mc < numMoverTests; mc++) {
        int targetMovers = moverTestCounts[mc];
        ClearMovers();
        ClearJobs();
        
        for (int i = 0; i < targetMovers; i++) {
            float mx = GetRandomValue(10, 60) * CELL_SIZE + CELL_SIZE * 0.5f;
            float my = GetRandomValue(10, 60) * CELL_SIZE + CELL_SIZE * 0.5f;
            Mover* m = &movers[moverCount];
            Point goal = {(int)(mx/CELL_SIZE), (int)(my/CELL_SIZE), 0};
            InitMover(m, mx, my, 0, goal, 100.0f);
            moverCount++;
        }
        
        int numIterations = 100;
        
        // Legacy
        volatile int legacySum = 0;
        for (int i = 0; i < targetMovers; i++) movers[i].currentJobId = -1;
        for (int i = 0; i < MAX_ITEMS; i++) if (items[i].active) items[i].reservedBy = -1;
        ClearJobs();
        
        double legacyStart = GetBenchTime();
        for (int iter = 0; iter < numIterations; iter++) {
            for (int m = 0; m < targetMovers; m++) {
                if (movers[m].currentJobId >= 0) ReleaseJob(movers[m].currentJobId);
                movers[m].currentJobId = -1;
            }
            for (int i = 0; i < 500; i++) if (items[i].active) items[i].reservedBy = -1;
            AssignJobsLegacy();
            legacySum += idleMoverCount;
        }
        double legacyTime = (GetBenchTime() - legacyStart) * 1000.0;
        (void)legacySum;
        
        // WorkGivers
        volatile int wgSum = 0;
        for (int i = 0; i < targetMovers; i++) movers[i].currentJobId = -1;
        for (int i = 0; i < MAX_ITEMS; i++) if (items[i].active) items[i].reservedBy = -1;
        ClearJobs();
        
        double wgStart = GetBenchTime();
        for (int iter = 0; iter < numIterations; iter++) {
            for (int m = 0; m < targetMovers; m++) {
                if (movers[m].currentJobId >= 0) ReleaseJob(movers[m].currentJobId);
                movers[m].currentJobId = -1;
            }
            for (int i = 0; i < 500; i++) if (items[i].active) items[i].reservedBy = -1;
            AssignJobsWorkGivers();
            wgSum += idleMoverCount;
        }
        double wgTime = (GetBenchTime() - wgStart) * 1000.0;
        (void)wgSum;
        
        // Hybrid
        volatile int hybridSum = 0;
        for (int i = 0; i < targetMovers; i++) movers[i].currentJobId = -1;
        for (int i = 0; i < MAX_ITEMS; i++) if (items[i].active) items[i].reservedBy = -1;
        ClearJobs();
        
        double hybridStart = GetBenchTime();
        for (int iter = 0; iter < numIterations; iter++) {
            for (int m = 0; m < targetMovers; m++) {
                if (movers[m].currentJobId >= 0) ReleaseJob(movers[m].currentJobId);
                movers[m].currentJobId = -1;
            }
            for (int i = 0; i < 500; i++) if (items[i].active) items[i].reservedBy = -1;
            AssignJobsHybrid();
            hybridSum += idleMoverCount;
        }
        double hybridTime = (GetBenchTime() - hybridStart) * 1000.0;
        (void)hybridSum;
        
        double wgRatio = (legacyTime > 0.001) ? wgTime / legacyTime : 0;
        double hybridRatio = (legacyTime > 0.001) ? hybridTime / legacyTime : 0;
        printf("  %3d movers: Legacy=%.3fms  WorkGivers=%.3fms (%.1fx)  Hybrid=%.3fms (%.1fx)\n",
               targetMovers, legacyTime, wgTime, wgRatio, hybridTime, hybridRatio);
    }
    
    FreeItemSpatialGrid();
    printf("\n");
}

// =============================================================================
// Main
// =============================================================================
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    SetTraceLogLevel(LOG_NONE);
    
    // Tests use legacy terrain (z=0 walkable), so use legacy mode
    g_useDFWalkability = false;
    
    printf("\n=== JOB SYSTEM BENCHMARKS ===\n\n");
    
    BenchFindNearestItem();
    BenchItemsTick();
    BenchAssignJobsRehaul();
    BenchAssignJobsAlgorithms();
    
    printf("Done.\n");
    return 0;
}
