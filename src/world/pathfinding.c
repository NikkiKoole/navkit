// pathfinding.c — game-side layer over the nav library (see pathfinding.h).
//
// The algorithms live in nav/nav.c (included by the unity builds, with the
// game's host contract in world/nav_host.h). This file owns the game's single
// NavWorld, keeps it in sync with the grid's shape, and provides the
// pre-library API (RunAStar, FindPath, BuildEntrances, ...) plus a few
// demo/test helpers that use game RNG.

#include "pathfinding.h"
#include "nav_host.h"
#include "../../vendor/raylib.h"
#include <stdlib.h>
#include <string.h>

// =============================================================================
// The game's world
// =============================================================================

static NavWorld* gameNav = NULL;

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

void BuildEntrances(void) {
    NavWorld* w = NavGame();
    NavRegionsInvalidate(w);      // full rebuild = the world may have changed without notifications
    NavBuildEntrances(w);
    rampCount = w->rampCellNum;   // game's ramp counter is re-synced on full rebuilds
}

void BuildGraph(void) {
    NavBuildGraph(NavGame());
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

int FindPath(PathAlgorithm algo, Point start, Point goal, Point* outPath, int maxLen) {
    if (start.x < 0 || goal.x < 0) return 0;
    NavWorld* w = NavGame();
    // Different reachability regions: no path exists, answer without searching
    // (a failed search is the most expensive kind)
    int rs = NavRegionId(w, start), rg = NavRegionId(w, goal);
    if (rs >= 0 && rg >= 0 && rs != rg) {
        statsPathCount++;
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
    memset(visited, 0, sizeof(visited));

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
