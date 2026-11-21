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

void MarkChunkDirty(int cellX, int cellY, int cellZ) {
    NavMarkCellChanged(NavGame(), cellX, cellY, cellZ);
}

bool IsChunkDirty(int cx, int cy, int z) {
    return NavIsChunkDirty(NavGame(), cx, cy, z);
}

void SetChunkDirty(int cx, int cy, int z, bool dirty) {
    NavSetChunkDirty(NavGame(), cx, cy, z, dirty);
}

void ClearSharedGoalFields(void);
void TreesNoteWorldChanged(void);

void BuildEntrances(void) {
    NavWorld* w = NavGame();
    NavRegionsInvalidate(w);      // full rebuild = the world may have changed without notifications
    TreesNoteWorldChanged();      // ... so leaves anywhere may have lost their trunk
    ClearSharedGoalFields();      // ... and the shared-goal fields belong to the old one
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

// =============================================================================
// Shared-goal fields: many movers going to the same few places
// =============================================================================
//
// A goal requested SHARED_GOAL_PROMOTE times within SHARED_GOAL_WINDOW game-seconds
// gets a cached hierarchical flow field (NavHFlow); later HPA* requests for it
// walk the field instead of searching (paths within ~1% of optimal, see
// tests/test_nav_fuzz.c). Fields rebuild themselves when the graph changes and
// are refreshed every SHARED_GOAL_REFRESH game-seconds (ladder queue costs change).
// One-off goals keep using HPA* (a full field costs more than one search).
// Times are game time, not wall clock, so runs are reproducible (the same requests
// promote the same goals however busy the machine is).
// One request (a walk) may build at most SHARED_GOAL_FIELD_LIMIT chunk fields: on big
// maps building one can pull in nearly the whole map (docs/todo/architecture/
// large-worlds.md). Over the limit the request uses HPA*, the field is dropped and the
// goal isn't promoted again for SHARED_GOAL_BLOCK game-seconds.

bool useSharedGoalFields = true;
int sharedGoalHits = 0, sharedGoalBuilds = 0, sharedGoalFallbacks = 0, sharedGoalTooBig = 0;
double findPathTotalMs = 0.0;
int findPathCalls = 0;

#define SHARED_GOAL_SLOTS   32
#define SHARED_GOAL_SEEN    128
#define SHARED_GOAL_PROMOTE 3
#define SHARED_GOAL_WINDOW  10.0
#define SHARED_GOAL_REFRESH 2.0
#define SHARED_GOAL_FIELD_LIMIT 1000   // ~12 ms of field building
#define SHARED_GOAL_BLOCK   60.0

typedef struct { Point goal; NavHFlow* h; double lastUse, builtAt; } SharedGoal;
typedef struct { Point goal; int count; double firstAt, blockedUntil; } SeenGoal;
static SharedGoal sharedGoals[SHARED_GOAL_SLOTS];
static SeenGoal seenGoals[SHARED_GOAL_SEEN];
static NavWorld* sharedGoalWorld = NULL;

static double PathClockNow(void) {   // works headless (raylib's GetTime needs a window)
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

void ClearSharedGoalFields(void) {
    for (int i = 0; i < SHARED_GOAL_SLOTS; i++) {
        if (sharedGoals[i].h) NavHFlowDestroy(sharedGoals[i].h);
        sharedGoals[i] = (SharedGoal){{-1, -1, -1}, NULL, 0, 0};
    }
    for (int i = 0; i < SHARED_GOAL_SEEN; i++) seenGoals[i] = (SeenGoal){{-1, -1, -1}, 0, 0, 0};
    sharedGoalWorld = NULL;
}

static bool SamePoint(Point a, Point b) { return a.x == b.x && a.y == b.y && a.z == b.z; }

// Cached field for this goal: existing, or newly promoted after repeated requests
static NavHFlow* SharedGoalField(NavWorld* w, Point goal, double now) {
    if (sharedGoalWorld != w) { ClearSharedGoalFields(); sharedGoalWorld = w; }
    for (int i = 0; i < SHARED_GOAL_SLOTS; i++) {
        SharedGoal* g = &sharedGoals[i];
        if (!g->h || !SamePoint(g->goal, goal)) continue;
        if (now < g->builtAt || now - g->builtAt > SHARED_GOAL_REFRESH) { NavHFlowInvalidate(g->h); g->builtAt = now; }
        g->lastUse = now;
        return g->h;
    }
    // Count requests (small table, oldest entry recycled)
    int slot = -1, oldest = 0;
    for (int i = 0; i < SHARED_GOAL_SEEN; i++) {
        if (SamePoint(seenGoals[i].goal, goal)) { slot = i; break; }
        if (seenGoals[i].firstAt < seenGoals[oldest].firstAt) oldest = i;
    }
    if (slot < 0) { slot = oldest; seenGoals[slot] = (SeenGoal){goal, 0, now, 0}; }
    SeenGoal* sgl = &seenGoals[slot];
    if (now < sgl->firstAt || now - sgl->firstAt > SHARED_GOAL_WINDOW) { sgl->count = 0; sgl->firstAt = now; }
    if (now < sgl->blockedUntil && sgl->blockedUntil - now <= SHARED_GOAL_BLOCK) return NULL;   // too big recently
    if (++sgl->count < SHARED_GOAL_PROMOTE) return NULL;
    // Promote: take a free slot or the least recently used
    int lru = 0;
    for (int i = 0; i < SHARED_GOAL_SLOTS; i++) {
        if (!sharedGoals[i].h) { lru = i; break; }
        if (sharedGoals[i].lastUse < sharedGoals[lru].lastUse) lru = i;
    }
    if (sharedGoals[lru].h) NavHFlowDestroy(sharedGoals[lru].h);
    sharedGoals[lru] = (SharedGoal){goal, NavHFlowCreate(w, goal), now, now};
    sharedGoalBuilds++;
    return sharedGoals[lru].h;
}

// The field for goal got too big to build (buildLimit): drop it, don't promote it
// again for a while
static void DropSharedGoalField(Point goal, double now) {
    for (int i = 0; i < SHARED_GOAL_SLOTS; i++)
        if (sharedGoals[i].h && SamePoint(sharedGoals[i].goal, goal)) {
            NavHFlowDestroy(sharedGoals[i].h);
            sharedGoals[i] = (SharedGoal){{-1, -1, -1}, NULL, 0, 0};
        }
    for (int i = 0; i < SHARED_GOAL_SEEN; i++)
        if (SamePoint(seenGoals[i].goal, goal)) {
            seenGoals[i].count = 0;
            seenGoals[i].blockedUntil = now + SHARED_GOAL_BLOCK;
        }
    sharedGoalTooBig++;
}

// Walk the field from start to goal into outPath (goal-first, like the
// pathfinders). 0 if the walk doesn't reach the goal.
static int WalkSharedGoalField(NavWorld* w, NavHFlow* h, Point start, Point goal, Point* outPath, int maxLen) {
    int n = 0;
    Point cur = start, nxt;
    outPath[n++] = cur;
    while (!SamePoint(cur, goal)) {
        if (n >= maxLen || !NavHFlowNext(w, h, cur, &nxt)) return 0;
        cur = nxt;
        outPath[n++] = cur;
    }
    for (int i = 0; i < n / 2; i++) { Point t = outPath[i]; outPath[i] = outPath[n - 1 - i]; outPath[n - 1 - i] = t; }
    return n;
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
        fprintf(stderr, "SLOW PATH %.0f ms algo=%d (%d,%d,%d) -> (%d,%d,%d) len=%d\n", ms, (int)algo,
                start.x, start.y, start.z, goal.x, goal.y, goal.z, len);
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
    // Different reachability regions: no path exists, answer without searching
    // (a failed search is the most expensive kind)
    int rs = NavRegionId(w, start), rg = NavRegionId(w, goal);
    if (rs >= 0 && rg >= 0 && rs != rg) {
        statsPathCount++;
        return 0;
    }
    if (useSharedGoalFields && algo == PATH_ALGO_HPA && !SamePoint(start, goal)) {
        double now = gameTime;
        NavHFlow* h = SharedGoalField(w, goal, now);
        if (h) {
            h->buildBudget = SHARED_GOAL_FIELD_LIMIT;
            int n = WalkSharedGoalField(w, h, start, goal, outPath, maxLen);
            h->buildBudget = -1;
            if (n > 0) { sharedGoalHits++; statsPathCount++; return n; }
            sharedGoalFallbacks++;
            if (h->overLimit) DropSharedGoalField(goal, now);
        }
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

// Is z reachable via ladders or ramps?
static bool ZLevelIsConnected(int z) {
    NavWorld* w = NavGame();
    for (int i = 0; i < w->ladderNum; i++)
        if (w->ladderList[i].zLow == z || w->ladderList[i].zHigh == z) return true;
    for (int i = 0; i < w->rampNum; i++)
        if (w->rampList[i].rampZ == z || w->rampList[i].rampZ + 1 == z) return true;
    return false;
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
