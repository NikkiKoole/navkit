// pathfinding.c — game-side layer over the nav library (see pathfinding.h).
//
// The algorithms live in nav/nav.c (included by the unity builds, with the
// game's host contract in world/nav_host.h). This file owns the game's single
// NavWorld, keeps it in sync with the grid's shape, and provides the
// pre-library API (RunAStar, FindPath, BuildEntrances, ...) plus a few
// demo/test helpers that use game RNG.

#include "pathfinding.h"
#include "nav_host.h"
#include "../core/time.h"
#include "../../vendor/raylib.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <stdatomic.h>
#ifndef _WIN32
#include <unistd.h>
#endif

// =============================================================================
// The game's world
// =============================================================================

static NavWorld* gameNav = NULL;

// ---- Threads for HPA* graph builds (NavWorld.parallelFor) ----
// The nav library hands over per-chunk edge searches that only read the grid; it adds
// their results in a fixed order, so the graph is the same with any thread count.
int navThreads = 0;   // 0 = one per core, 1 = no threads (setting "navThreads")

#define NAV_MAX_THREADS 32
typedef struct { void (*fn)(void*, int); void* ctx; int n; atomic_int next; } NavParJob;

static void* NavParWorker(void* p) {
    NavParJob* j = p;
    for (;;) {
        int i = atomic_fetch_add(&j->next, 1);
        if (i >= j->n) return NULL;
        j->fn(j->ctx, i);
    }
}

static int NavThreadCount(void) {
    if (navThreads > 0) return navThreads < NAV_MAX_THREADS ? navThreads : NAV_MAX_THREADS;
    static int cores = 0;
    if (cores == 0) {
#ifdef _WIN32
        const char* env = getenv("NUMBER_OF_PROCESSORS");   // (no windows.h: clashes with raylib)
        cores = env ? atoi(env) : 1;
#else
        cores = (int)sysconf(_SC_NPROCESSORS_ONLN);
#endif
        if (cores < 1) cores = 1;
        if (cores > NAV_MAX_THREADS) cores = NAV_MAX_THREADS;
    }
    return cores;
}

static void NavParallelFor(void* user, int n, void (*fn)(void* ctx, int i), void* ctx) {
    (void)user;
    NavParJob job;
    job.fn = fn; job.ctx = ctx; job.n = n;
    atomic_init(&job.next, 0);
    int threads = NavThreadCount();
    if (threads > n) threads = n;
    pthread_t th[NAV_MAX_THREADS];
    int started = 0;
    for (int k = 1; k < threads; k++)
        if (pthread_create(&th[started], NULL, NavParWorker, &job) == 0) started++;
    NavParWorker(&job);   // this thread works too (and does everything if none started)
    for (int k = 0; k < started; k++) pthread_join(th[k], NULL);
}

NavWorld* NavGame(void) {
    NavWorld* w = gameNav;
    if (w && w->width == gridWidth && w->height == gridHeight && w->depth == gridDepth
          && w->chunkW == chunkWidth && w->chunkH == chunkHeight) {
        return w;
    }
    if (!w) {
        gameNav = NavCreate(gridWidth, gridHeight, gridDepth, chunkWidth, chunkHeight, NULL);
    } else {
        NavResize(w, gridWidth, gridHeight, gridDepth, chunkWidth, chunkHeight);
    }
    if (!gameNav || !gameNav->entranceList) {
        TraceLog(LOG_FATAL, "NavGame: out of memory for %dx%dx%d world", gridWidth, gridHeight, gridDepth);
    }
    // Natural terrain costs >= 10 per cell, the exact HPA* estimate assumes 8 (floors): with
    // 1.25 cross-map routes on a 2048 world searched 27% faster with the same costs
    // (docs/todo/architecture/chunked-world-plan.md)
    gameNav->absHeuristicPct = 125;
    gameNav->parallelFor = NavParallelFor;
    return gameNav;
}

// =============================================================================
// Debug/demo path + stats
// =============================================================================

Point path[MAX_PATH];
int pathLength = 0;
Point startPos = {-1, -1, 0};
Point goalPos = {-1, -1, 0};

static int statsPathCount = 0;
static double statsTotalTime = 0.0;
static double statsLastReportTime = 0.0;
int pathStatsCount = 0;
double pathStatsTotalMs = 0.0;
double pathStatsAvgMs = 0.0;
bool pathStatsUpdated = false;

void UpdatePathStats(void) {
    double now = GetTime();
    if (statsLastReportTime == 0.0) statsLastReportTime = now;
    if (now - statsLastReportTime >= 5.0) {
        pathStatsCount = statsPathCount;
        pathStatsTotalMs = statsTotalTime;
        pathStatsAvgMs = (statsPathCount > 0) ? (statsTotalTime / statsPathCount) : 0.0;
        pathStatsUpdated = true;
        statsPathCount = 0;
        statsTotalTime = 0.0;
        statsLastReportTime = now;
    }
}

void ResetPathStats(void) {
    statsPathCount = 0;
    statsTotalTime = 0.0;
    statsLastReportTime = 0.0;
    pathStatsCount = 0;
    pathStatsTotalMs = 0.0;
    pathStatsAvgMs = 0.0;
}

// =============================================================================
// Graph maintenance
// =============================================================================

unsigned walkabilityChangeCount = 0;

void MarkChunkDirty(int cellX, int cellY, int cellZ) {
    walkabilityChangeCount++;
    NavMarkCellChanged(NavGame(), cellX, cellY, cellZ);
}

bool IsChunkDirty(int cx, int cy, int z) {
    return NavIsChunkDirty(NavGame(), cx, cy, z);
}

void SetChunkDirty(int cx, int cy, int z, bool dirty) {
    NavSetChunkDirty(NavGame(), cx, cy, z, dirty);
}

void TreesNoteWorldChanged(void);

void BuildEntrances(void) {
    NavWorld* w = NavGame();
    walkabilityChangeCount++;
    NavRegionsInvalidate(w);      // full rebuild = the world may have changed without notifications
    TreesNoteWorldChanged();      // ... so leaves anywhere may have lost their trunk
    NavBuildEntrances(w);
    rampCount = w->rampCellNum;   // game's ramp counter is re-synced on full rebuilds
}

void BuildGraph(void) {
    NavBuildGraph(NavGame());
    // Reachability regions are built lazily on the first query; on a 2048 map that is
    // ~1 s, which landed in the first tick's path requests. Build them with the graph.
    NavRegionCount(NavGame());
}

void UpdateDirtyChunks(void) {
    NavUpdateDirty(NavGame());
}

// =============================================================================
// Queries
// =============================================================================

void RunAStar(void) {
    if (startPos.x < 0 || goalPos.x < 0) return;
    pathLength = NavFindPathAStar(NavGame(), startPos, goalPos, path, MAX_PATH);
}

void RunHPAStar(void) {
    pathLength = NavFindPathHPA(NavGame(), startPos, goalPos, path, MAX_PATH);
}

int FindPathHPA(Point start, Point goal, Point* outPath, int maxLen) {
    if (start.x < 0 || goal.x < 0) return 0;
    return NavFindPathHPA(NavGame(), start, goal, outPath, maxLen);
}

bool IsReachable(Point a, Point b) {
    return NavReachable(NavGame(), a, b);
}

// Path request statistics (wall clock, works headless)
// (Shared-goal flow fields used to live here: removed 2026-11-21, slower than HPA* at every
// map size; numbers in docs/todo/architecture/chunked-world-plan.md, "Shared goal fields".
// The library keeps NavHFlow, fuzz-tested, if many movers to one point ever need it.)
double findPathTotalMs = 0.0;
int findPathCalls = 0;

static double PathClockNow(void) {   // works headless (raylib's GetTime needs a window)
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

// Movers can take HPA* paths as a coarse route (waypoints) and refine it leg by leg
// as they walk (mover.c): no length cap, little memory. Off = full cell paths.
bool useCoarsePaths = true;

static int FindPathImpl(PathAlgorithm algo, Point start, Point goal, Point* outPath, int maxLen,
                        Point* outRoute, int maxRoute, int* routeLen);

int FindPath(PathAlgorithm algo, Point start, Point goal, Point* outPath, int maxLen) {
    return FindPathOrRoute(algo, start, goal, outPath, maxLen, NULL, 0, NULL);
}

int FindPathOrRoute(PathAlgorithm algo, Point start, Point goal, Point* outPath, int maxLen,
                    Point* outRoute, int maxRoute, int* routeLen) {
    double t0 = PathClockNow();
    int len = FindPathImpl(algo, start, goal, outPath, maxLen, outRoute, maxRoute, routeLen);
    double ms = (PathClockNow() - t0) * 1000.0;
    findPathTotalMs += ms;
    // Slow request: say where, so an accidental repro can be replayed with
    // --inspect save --path x,y,z x,y,z (see docs/todo/architecture/large-worlds.md)
    if (ms > 200.0)
        fprintf(stderr, "SLOW PATH %.0f ms algo=%d (%d,%d,%d) -> (%d,%d,%d) len=%d route=%d explored=%d search=%.0f ms\n",
                ms, (int)algo, start.x, start.y, start.z, goal.x, goal.y, goal.z, len,
                routeLen ? *routeLen : -1, NavGame()->explored, NavGame()->lastMs);
    findPathCalls++;
    return len;
}

int FindPathUntimed(PathAlgorithm algo, Point start, Point goal, Point* outPath, int maxLen) {
    return FindPathImpl(algo, start, goal, outPath, maxLen, NULL, 0, NULL);
}

int RefineLeg(Point from, Point to, Point* out, int maxLen) {
    return NavRefineLeg(NavGame(), from, to, out, maxLen);
}

static int FindPathImpl(PathAlgorithm algo, Point start, Point goal, Point* outPath, int maxLen,
                        Point* outRoute, int maxRoute, int* routeLen) {
    if (routeLen) *routeLen = 0;
    if (start.x < 0 || goal.x < 0) return 0;
    NavWorld* w = NavGame();
    w->explored = 0;   // what this request explored (callers budget by it)
    // Different reachability regions: no path exists, answer without searching
    // (a failed search is the most expensive kind)
    int rs = NavRegionId(w, start), rg = NavRegionId(w, goal);
    if (rs >= 0 && rg >= 0 && rs != rg) {
        statsPathCount++;
        return 0;
    }
    if (outRoute && routeLen && useCoarsePaths && algo == PATH_ALGO_HPA) {
        *routeLen = NavFindRouteHPA(w, start, goal, outRoute, maxRoute);
        statsPathCount++;
        statsTotalTime += w->lastMs;
        return 0;
    }
    int len = NavFindPath(w, (NavAlgorithm)algo, start, goal, outPath, maxLen);

    if (len > 0 && len >= maxLen * 9 / 10) {
        TraceLog(LOG_WARNING, "Path near MAX_PATH limit: %d/%d (%.0f%%) from (%d,%d,%d) to (%d,%d,%d)",
                 len, maxLen, (float)len / maxLen * 100.0f,
                 start.x, start.y, start.z, goal.x, goal.y, goal.z);
    }
    statsPathCount++;
    statsTotalTime += w->lastMs;
    return len;
}

int AStarChunk(int sx, int sy, int sz, int gx, int gy, int minX, int minY, int maxX, int maxY) {
    return NavChunkCost(NavGame(), sx, sy, sz, gx, gy, minX, minY, maxX, maxY);
}

int AStarChunkMultiTarget(int sx, int sy, int sz, int* targetX, int* targetY, int* outCosts, int numTargets,
                          int minX, int minY, int maxX, int maxY) {
    return NavChunkCostsMulti(NavGame(), sx, sy, sz, targetX, targetY, outCosts, numTargets,
                              minX, minY, maxX, maxY);
}

// =============================================================================
// Random cell utilities (game RNG)
// =============================================================================

void SeedRandom(unsigned int seed) {
    SetRandomSeed(seed);
}

// Is z reachable via ladders or ramps? Answered for every level in one pass over the
// link lists and kept until the graph changes: random-cell pickers ask once per attempt,
// and a 2048 map has ~500K ramp links (a level without ramps scanned them all).
static bool ZLevelIsConnected(int z) {
    static NavWorld* cachedWorld = NULL;
    static unsigned cachedVersion = 0;
    static int cachedDepth = 0, cachedLadders = -1, cachedRamps = -1;
    static bool* connected = NULL;
    NavWorld* w = NavGame();
    if (z < 0 || z >= w->depth) return false;
    if (w != cachedWorld || w->graphVersion != cachedVersion || w->depth != cachedDepth
        || w->ladderNum != cachedLadders || w->rampNum != cachedRamps || !connected) {
        bool* c = realloc(connected, (size_t)w->depth * sizeof(bool));
        if (!c) return true;
        connected = c;
        memset(connected, 0, (size_t)w->depth * sizeof(bool));
        for (int i = 0; i < w->ladderNum; i++) {
            const NavLadderLink* l = &w->ladderList[i];
            if (l->zLow >= 0 && l->zLow < w->depth) connected[l->zLow] = true;
            if (l->zHigh >= 0 && l->zHigh < w->depth) connected[l->zHigh] = true;
        }
        for (int i = 0; i < w->rampNum; i++) {
            int rz = w->rampList[i].rampZ;
            if (rz >= 0 && rz < w->depth) connected[rz] = true;
            if (rz + 1 >= 0 && rz + 1 < w->depth) connected[rz + 1] = true;
        }
        cachedWorld = w; cachedVersion = w->graphVersion; cachedDepth = w->depth;
        cachedLadders = w->ladderNum; cachedRamps = w->rampNum;
    }
    return connected[z];
}

Point GetRandomWalkableCell(void) {
    Point p;
    // Skip z-levels with no vertical connections (stranded spawns)
    bool checkConnectivity = (ladderLinkCount > 0 || rampLinkCount > 0);
    for (int attempts = 0; attempts < 1000; attempts++) {
        p.x = GetRandomValue(0, gridWidth - 1);
        p.y = GetRandomValue(0, gridHeight - 1);
        p.z = GetRandomValue(0, gridDepth - 1);
        if (checkConnectivity && !ZLevelIsConnected(p.z)) continue;
        if (NavIsValidDestination(p.x, p.y, p.z)) return p;
    }
    return (Point){-1, -1, 0};
}

Point GetRandomWalkableCellDifferentZ(int excludeZ) {
    Point p;
    bool checkConnectivity = (ladderLinkCount > 0 || rampLinkCount > 0);
    for (int attempts = 0; attempts < 1000; attempts++) {
        p.x = GetRandomValue(0, gridWidth - 1);
        p.y = GetRandomValue(0, gridHeight - 1);
        p.z = GetRandomValue(0, gridDepth - 1);
        if (checkConnectivity && !ZLevelIsConnected(p.z)) continue;
        if (p.z != excludeZ && NavIsValidDestination(p.x, p.y, p.z)) return p;
    }
    return GetRandomWalkableCell();
}

Point GetRandomGoalNear(Point from, int radius, bool differentZ) {
    bool checkConnectivity = (ladderLinkCount > 0 || rampLinkCount > 0);
    int x0 = 0, y0 = 0, x1 = gridWidth - 1, y1 = gridHeight - 1;
    if (radius > 0) {
        x0 = from.x - radius > 0 ? from.x - radius : 0;
        y0 = from.y - radius > 0 ? from.y - radius : 0;
        x1 = from.x + radius < gridWidth - 1 ? from.x + radius : gridWidth - 1;
        y1 = from.y + radius < gridHeight - 1 ? from.y + radius : gridHeight - 1;
    }
    Point fallback = {-1, -1, 0};
    for (int attempts = 0; attempts < 1000; attempts++) {
        Point p = {GetRandomValue(x0, x1), GetRandomValue(y0, y1), GetRandomValue(0, gridDepth - 1)};
        if (differentZ && p.z == from.z && attempts < 500) continue;
        if (checkConnectivity && !ZLevelIsConnected(p.z)) continue;
        if (!NavIsValidDestination(p.x, p.y, p.z)) continue;
        if (from.x < 0 || IsReachable(from, p)) return p;   // (O(1): reachability regions)
        if (fallback.x < 0) fallback = p;
    }
    return fallback.x >= 0 ? fallback : GetRandomWalkableCell();
}

// A cell in the biggest reachability region: the region most of 256 random walkable
// cells fall in (a sample hits a region in proportion to its size). {-1,-1,0} if none.
Point GetMainRegionAnchor(void) {
    enum { SAMPLES = 256 };
    int ids[SAMPLES], counts[SAMPLES];
    Point at[SAMPLES];
    int n = 0, best = -1;
    for (int s = 0; s < SAMPLES; s++) {
        Point p = GetRandomWalkableCell();
        if (p.x < 0) continue;
        int id = NavRegionId(NavGame(), p);
        if (id < 0) continue;
        int k = 0;
        while (k < n && ids[k] != id) k++;
        if (k == n) { ids[n] = id; counts[n] = 0; at[n] = p; n++; }
        counts[k]++;
        if (best < 0 || counts[k] > counts[best]) best = k;
    }
    return best >= 0 ? at[best] : (Point){-1, -1, 0};
}

// Random walkable cell connected to `anchor` (spawning: nobody starts on a patch they
// can't leave). Any walkable cell if there's no anchor or nothing connected turns up.
Point GetRandomWalkableCellReachableFrom(Point anchor) {
    if (anchor.x < 0) return GetRandomWalkableCell();
    for (int attempts = 0; attempts < 1000; attempts++) {
        Point p = GetRandomWalkableCell();
        if (p.x < 0) break;
        if (IsReachable(anchor, p)) return p;
    }
    return GetRandomWalkableCell();
}

// Same-z goals: no ladder check needed.
Point GetRandomWalkableCellOnZ(int z) {
    Point p;
    p.z = z;
    for (int attempts = 0; attempts < 1000; attempts++) {
        p.x = GetRandomValue(0, gridWidth - 1);
        p.y = GetRandomValue(0, gridHeight - 1);
        if (NavIsValidDestination(p.x, p.y, p.z)) return p;
    }
    return (Point){-1, -1, z};
}

// BFS from a starting cell, collect reachable walkable cells, pick one randomly.
// Respects walls, doors, room boundaries — mover in a sealed room only gets cells inside.
#define REACHABLE_MAX 256
Point GetRandomReachableCell(int startX, int startY, int startZ) {
    static Point reachable[REACHABLE_MAX];
    static bool visited[MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
    CLEAR_GRID_PLANE(visited, 0);

    static Point queue[REACHABLE_MAX];
    int qHead = 0, qTail = 0;
    int found = 0;

    queue[qTail++] = (Point){startX, startY, startZ};
    visited[startY][startX] = true;

    while (qHead < qTail && found < REACHABLE_MAX) {
        Point cur = queue[qHead++];
        reachable[found++] = cur;

        static const int dx[] = {1, -1, 0, 0};
        static const int dy[] = {0, 0, 1, -1};
        for (int d = 0; d < 4; d++) {
            int nx = cur.x + dx[d];
            int ny = cur.y + dy[d];
            if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
            if (visited[ny][nx]) continue;
            if (!IsCellWalkableAt(cur.z, ny, nx)) continue;
            visited[ny][nx] = true;
            if (qTail < REACHABLE_MAX) queue[qTail++] = (Point){nx, ny, cur.z};
        }
    }

    if (found <= 1) return (Point){startX, startY, startZ};
    int pick = GetRandomValue(1, found - 1);
    return reachable[pick];
}
