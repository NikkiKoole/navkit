// bench_nav_update.c — HPA* on a big map: graph updates and path searches.
//
// Run with: make bench_nav_update            (512x512x16 hills+water, 20 updates)
//       or: ./build/bin/bench_nav_update [size] [updates] [terrain] [seed] [walls]
//           terrain: any generator name below (default hillswater); seed default 16
//           (the densest hills+water graph seen at 512); walls per update default 3
//
// Builds the graph, then does `updates` rounds of random wall placements on
// walkable cells (like mining/building), timing each NavUpdateDirty. Then times
// HPA* searches by trip length (short < 64 cells, medium 64-256, long > 256),
// split into the abstract search (chunk graph) and the refinement (cells).
// Finally checks the incrementally updated graph equals a fresh full build: same
// entrances, same directed edges and costs. Exit code is non-zero if they differ.

#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/world/material.h"
#include "../src/world/pathfinding.h"
#include "../src/world/terrain.h"
#include "../src/game_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double NowMs(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

typedef struct { const char* name; void (*gen)(void); } Gen;
static void GenSparse(void) { GenerateSparse(0.25f); }
static const Gen kGens[] = {
    {"sparse", GenSparse},           {"perlin", GeneratePerlin},         {"city", GenerateCity},
    {"mixed", GenerateMixed},        {"concentric", GenerateConcentricMaze}, {"dungeon", GenerateDungeonRooms},
    {"caves", GenerateCaves},        {"drunkard", GenerateDrunkard},     {"tunneler", GenerateTunneler},
    {"mixmax", GenerateMixMax},      {"towers", GenerateTowers},         {"galleryflat", GenerateGalleryFlat},
    {"castle", GenerateCastle},      {"labyrinth3d", GenerateLabyrinth3D}, {"spiral3d", GenerateSpiral3D},
    {"council", GenerateCouncilEstate}, {"hills", GenerateHills},        {"hillssoils", GenerateHillsSoils},
    {"hillswater", GenerateHillsSoilsWater},
};

static Point RandomWalkable(void) {
    for (int t = 0; t < 100000; t++) {
        Point p = {GetRandomValue(0, gridWidth - 1), GetRandomValue(0, gridHeight - 1), GetRandomValue(1, gridDepth - 1)};
        if (IsCellWalkableAt(p.z, p.y, p.x)) return p;
    }
    return (Point){-1, -1, 0};
}

// Walkable cell at Chebyshev distance [dMin, dMax) from p (any level), or x = -1
static Point RandomWalkableNear(Point p, int dMin, int dMax) {
    int span = gridWidth > gridHeight ? gridWidth : gridHeight;
    if (dMax > span) dMax = span;
    for (int t = 0; t < 20000; t++) {
        int dx = GetRandomValue(-dMax + 1, dMax - 1), dy = GetRandomValue(-dMax + 1, dMax - 1);
        int d = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
        if (d < dMin) continue;
        Point q = {p.x + dx, p.y + dy, GetRandomValue(0, gridDepth - 1)};
        if (q.x < 0 || q.y < 0 || q.x >= gridWidth || q.y >= gridHeight) continue;
        if (IsCellWalkableAt(q.z, q.y, q.x)) return q;
    }
    return (Point){-1, -1, 0};
}

// Graph snapshot by position (entrance indices differ between builds)
typedef struct { int x, y, z, c1, c2; } EntKey;
typedef struct { EntKey a, b; int cost; } EdgeKey;
static int CmpEnt(const EntKey* a, const EntKey* b) {
    if (a->z != b->z) return a->z - b->z;
    if (a->y != b->y) return a->y - b->y;
    if (a->x != b->x) return a->x - b->x;
    if (a->c1 != b->c1) return a->c1 - b->c1;
    return a->c2 - b->c2;
}
static int CmpEntV(const void* a, const void* b) { return CmpEnt(a, b); }
static int CmpEdgeV(const void* pa, const void* pb) {
    const EdgeKey* a = pa; const EdgeKey* b = pb;
    int c = CmpEnt(&a->a, &b->a);
    if (c) return c;
    if ((c = CmpEnt(&a->b, &b->b))) return c;
    return a->cost - b->cost;
}
typedef struct { EntKey* ents; int entNum; EdgeKey* edges; int edgeNum; } Snapshot;
static EntKey KeyOf(const NavWorld* w, int e) {
    const NavEntrance* E = &w->entranceList[e];
    return (EntKey){E->x, E->y, E->z, E->chunk1, E->chunk2};
}
static Snapshot Snap(NavWorld* w) {
    Snapshot s = {0};
    s.ents = malloc(sizeof(EntKey) * (size_t)(w->entranceNum + 1));
    for (int e = 0; e < w->entranceNum; e++) s.ents[s.entNum++] = KeyOf(w, e);
    int total;
    NavEdgesFlat(w, &total);
    s.edges = malloc(sizeof(EdgeKey) * (size_t)(total + 1));
    for (int e = 0; e < w->entranceNum; e++) {
        int n;
        const NavGraphEdge* out = NavOutEdges(w, e, &n);
        for (int k = 0; k < n; k++) s.edges[s.edgeNum++] = (EdgeKey){KeyOf(w, e), KeyOf(w, out[k].to), out[k].cost};
    }
    qsort(s.ents, (size_t)s.entNum, sizeof(EntKey), CmpEntV);
    qsort(s.edges, (size_t)s.edgeNum, sizeof(EdgeKey), CmpEdgeV);
    return s;
}

int main(int argc, char** argv) {
    int size = argc > 1 ? atoi(argv[1]) : 512;
    int updates = argc > 2 ? atoi(argv[2]) : 20;
    const char* terrain = argc > 3 ? argv[3] : "hillswater";
    uint64_t seed = argc > 4 ? (uint64_t)atoll(argv[4]) : 16;
    int walls = argc > 5 ? atoi(argv[5]) : 3;
    if (argc > 6) navThreads = atoi(argv[6]);   // graph build threads (0 = one per core)
    SetTraceLogLevel(LOG_WARNING);

    InitGridWithSizeAndChunkSize(size, size, 16, 16);
    gridDepth = 16;
    worldSeed = seed;
    const Gen* gen = NULL;
    for (size_t i = 0; i < sizeof(kGens) / sizeof(kGens[0]); i++)
        if (strcmp(kGens[i].name, terrain) == 0) gen = &kGens[i];
    if (!gen) {
        printf("unknown terrain '%s'; one of:", terrain);
        for (size_t i = 0; i < sizeof(kGens) / sizeof(kGens[0]); i++) printf(" %s", kGens[i].name);
        printf("\n");
        return 2;
    }
    gen->gen();

    double t0 = NowMs();
    BuildEntrances();
    double entMs = NowMs() - t0;
    BuildGraph();
    double fullMs = NowMs() - t0;
    NavWorld* w = NavGame();
    int edges;
    NavEdgesFlat(w, &edges);
    printf("%s seed %llu %dx%dx%d: %d entrances, %d edges, full build %.0f ms (entrances %.0f, graph %.0f)\n",
           terrain, (unsigned long long)seed, gridWidth, gridHeight, gridDepth, w->entranceNum, edges, fullMs,
           entMs, fullMs - entMs);

    SetRandomSeed(42);
    double sum = 0, worst = 0;
    for (int u = 0; u < updates; u++) {
        for (int k = 0; k < walls; k++) {
            Point p = RandomWalkable();
            if (p.x < 0) continue;
            GRID(p.x, p.y, p.z) = CELL_WALL;
            SetWallMaterial(p.x, p.y, p.z, MAT_DIRT);
            MarkChunkDirty(p.x, p.y, p.z);
        }
        double t = NowMs();
        UpdateDirtyChunks();
        double ms = NowMs() - t;
        sum += ms;
        if (ms > worst) worst = ms;
    }
    printf("incremental update (%d walls): avg %.2f ms, worst %.2f ms over %d updates\n", walls, sum / updates, worst, updates);

    // HPA* searches by trip length (found paths only: unreachable pairs are answered
    // by the region check without searching)
    static Point path[MAX_PATH];
    const struct { const char* name; int dMin, dMax; } kTrips[] = {
        {"short  (<64)", 8, 64}, {"medium (64-256)", 64, 256}, {"long   (>256)", 256, 100000},
    };
    for (int b = 0; b < 3; b++) {
        if (kTrips[b].dMin >= size) continue;
        double sum = 0, worst = 0, abs = 0, ref = 0; int n = 0, steps = 0;
        for (int tries = 0; tries < 400 && n < 40; tries++) {
            Point a = RandomWalkable(), g = RandomWalkableNear(a, kTrips[b].dMin, kTrips[b].dMax);
            if (a.x < 0 || g.x < 0) continue;
            double t = NowMs();
            int len = NavFindPath(w, NAV_ALGO_HPA, (NavPoint){a.x, a.y, a.z}, (NavPoint){g.x, g.y, g.z}, (NavPoint*)path, MAX_PATH);
            double ms = NowMs() - t;
            if (len <= 0) continue;
            n++; sum += ms; steps += len;
            abs += w->hpaAbstractMs; ref += w->hpaRefineMs;
            if (ms > worst) worst = ms;
        }
        if (n) printf("HPA* %s: avg %6.2f ms (abstract %.2f + refine %.2f), worst %6.2f ms, avg %d steps, %d paths\n",
                      kTrips[b].name, sum / n, abs / n, ref / n, worst, steps / n, n);
    }

    // Incremental result must equal a fresh full build
    Snapshot inc = Snap(w);
    BuildEntrances();
    BuildGraph();
    Snapshot full = Snap(w);
    bool same = inc.entNum == full.entNum && inc.edgeNum == full.edgeNum;
    if (same) same = memcmp(inc.ents, full.ents, sizeof(EntKey) * (size_t)inc.entNum) == 0;
    if (same) same = memcmp(inc.edges, full.edges, sizeof(EdgeKey) * (size_t)inc.edgeNum) == 0;
    printf("incremental == full build: %s (%d/%d entrances, %d/%d edges)\n", same ? "yes" : "NO",
           inc.entNum, full.entNum, inc.edgeNum, full.edgeNum);
    return same ? 0 : 1;
}
