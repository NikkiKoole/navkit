// bench_pathfinding.c - Pathfinding benchmarks
//
// Run with: make bench_pathfinding
// Or: ./bin/bench_pathfinding

#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/world/pathfinding.h"
#include "../src/simulation/water.h"
#include "../src/simulation/weather.h"
#include <stdio.h>
#include <time.h>

static double GetBenchTime(void) {
    return (double)clock() / CLOCKS_PER_SEC;
}

// =============================================================================
// A* variable cost vs uniform cost overhead
// =============================================================================
static void BenchAStarVariableCost(void) {
    printf("--- A* Variable Cost vs Uniform Cost ---\n");

    const char* map =
        "................................\n"
        "................................\n"
        "................................\n"
        "................................\n"
        "................................\n"
        "................................\n"
        "................................\n"
        "................................\n"
        "................................\n"
        "................................\n"
        "................................\n"
        "................................\n"
        "................................\n"
        "................................\n"
        "................................\n"
        "................................\n"
        "................................\n"
        "................................\n"
        "................................\n"
        "................................\n"
        "................................\n"
        "................................\n"
        "................................\n"
        "................................\n"
        "................................\n"
        "................................\n"
        "................................\n"
        "................................\n"
        "................................\n"
        "................................\n"
        "................................\n"
        "................................\n";
    InitGridFromAsciiWithChunkSize(map, 16, 16);
    InitWater();
    InitSnow();
    BuildEntrances();
    BuildGraph();

    // Scatter terrain variety
    for (int y = 8; y < 24; y++)
        for (int x = 8; x < 24; x++)
            grid[0][y][x] = CELL_BUSH;
    for (int x = 0; x < 32; x++) {
        SetSnowLevel(x, 4, 0, 2);
        SetSnowLevel(x, 5, 0, 2);
    }

    int iters = 100000;
    double start = GetBenchTime();
    for (int i = 0; i < iters; i++) {
        AStarChunk(0, 0, 0, 31, 31, 0, 0, 32, 32);
    }
    double elapsed = GetBenchTime() - start;

    // Reset to uniform terrain
    for (int y = 8; y < 24; y++)
        for (int x = 8; x < 24; x++)
            grid[0][y][x] = CELL_AIR;
    for (int x = 0; x < 32; x++) {
        SetSnowLevel(x, 4, 0, 0);
        SetSnowLevel(x, 5, 0, 0);
    }

    double startUniform = GetBenchTime();
    for (int i = 0; i < iters; i++) {
        AStarChunk(0, 0, 0, 31, 31, 0, 0, 32, 32);
    }
    double elapsedUniform = GetBenchTime() - startUniform;

    printf("  Variable cost: %.1f ms (%d iters, %.4f ms/iter)\n",
           elapsed * 1000.0, iters, elapsed * 1000.0 / iters);
    printf("  Uniform cost:  %.1f ms (%d iters, %.4f ms/iter)\n",
           elapsedUniform * 1000.0, iters, elapsedUniform * 1000.0 / iters);
    printf("  Overhead: %.1f%%\n",
           ((elapsed - elapsedUniform) / elapsedUniform) * 100.0);
}

// =============================================================================
// HPA* full pathfind with variable terrain
// =============================================================================
static void BenchHPAStarVariableCost(void) {
    printf("--- HPA* Variable Cost vs Uniform Cost ---\n");

    const char* map =
        "................................................................\n"
        "................................................................\n"
        "................................................................\n"
        "................................................................\n"
        "................................................................\n"
        "................................................................\n"
        "................................................................\n"
        "................................................................\n"
        "................................................................\n"
        "................................................................\n"
        "................................................................\n"
        "................................................................\n"
        "................................................................\n"
        "................................................................\n"
        "................................................................\n"
        "................................................................\n"
        "................................................................\n"
        "................................................................\n"
        "................................................................\n"
        "................................................................\n"
        "................................................................\n"
        "................................................................\n"
        "................................................................\n"
        "................................................................\n"
        "................................................................\n"
        "................................................................\n"
        "................................................................\n"
        "................................................................\n"
        "................................................................\n"
        "................................................................\n"
        "................................................................\n"
        "................................................................\n";
    InitGridFromAsciiWithChunkSize(map, 16, 16);
    InitWater();
    InitSnow();

    // Mixed terrain: bushes in center, snow band, floor corridor
    for (int y = 12; y < 20; y++)
        for (int x = 12; x < 20; x++)
            grid[0][y][x] = CELL_BUSH;
    for (int x = 0; x < 64; x++) {
        SetSnowLevel(x, 6, 0, 3);
        SetSnowLevel(x, 7, 0, 3);
    }
    for (int x = 0; x < 64; x++) SET_FLOOR(x, 0, 0);

    BuildEntrances();
    BuildGraph();

    Point tempPath[MAX_PATH];
    int iters = 50000;

    double start = GetBenchTime();
    for (int i = 0; i < iters; i++) {
        Point s = {0, 0, 0};
        Point g = {63, 31, 0};
        FindPath(PATH_ALGO_HPA, s, g, tempPath, MAX_PATH);
    }
    double elapsed = GetBenchTime() - start;

    // Clear terrain for uniform comparison
    for (int y = 12; y < 20; y++)
        for (int x = 12; x < 20; x++)
            grid[0][y][x] = CELL_AIR;
    for (int x = 0; x < 64; x++) {
        SetSnowLevel(x, 6, 0, 0);
        SetSnowLevel(x, 7, 0, 0);
        CLEAR_FLOOR(x, 0, 0);
    }
    BuildEntrances();
    BuildGraph();

    double startUniform = GetBenchTime();
    for (int i = 0; i < iters; i++) {
        Point s = {0, 0, 0};
        Point g = {63, 31, 0};
        FindPath(PATH_ALGO_HPA, s, g, tempPath, MAX_PATH);
    }
    double elapsedUniform = GetBenchTime() - startUniform;

    printf("  Variable: %.1f ms (%d iters, %.4f ms/iter)\n",
           elapsed * 1000.0, iters, elapsed * 1000.0 / iters);
    printf("  Uniform:  %.1f ms (%d iters, %.4f ms/iter)\n",
           elapsedUniform * 1000.0, iters, elapsedUniform * 1000.0 / iters);
    printf("  Overhead: %.1f%%\n",
           ((elapsed - elapsedUniform) / elapsedUniform) * 100.0);
}

int main(void) {
    SetTraceLogLevel(LOG_NONE);
    printf("=== Pathfinding Benchmarks ===\n\n");
    BenchAStarVariableCost();
    printf("\n");
    BenchHPAStarVariableCost();
    printf("\n");
    return 0;
}
