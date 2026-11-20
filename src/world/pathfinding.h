#ifndef PATHFINDING_H
#define PATHFINDING_H

// =============================================================================
// Game-side pathfinding layer over the nav library (nav/nav.h)
// =============================================================================
//
// The colony sim has exactly one NavWorld, returned by NavGame(). It is created
// on first use and kept in sync with the game grid's shape (gridWidth/Height/
// Depth, chunkWidth/Height): if the grid is re-initialised with other
// dimensions, the world is resized and the HPA* graph must be rebuilt.
//
// The macros below keep the pre-library global names working (entrances,
// entranceCount, use8Dir, needsRebuild, ...) by forwarding to NavGame()'s
// fields. nav/nav.c deliberately uses different field names so these macros
// can never collide with library code in the unity build.
//
// New code should prefer the Nav* API directly: NavFindPath(NavGame(), ...).
// =============================================================================

#include "grid.h"
#include "../../nav/nav.h"

// ---- types & limits (aliases for the library's) ----
typedef NavPoint      Point;
typedef NavEntrance   Entrance;
typedef NavGraphEdge  GraphEdge;
typedef NavLadderLink LadderLink;
typedef NavRampLink   RampLink;

typedef enum {
    PATH_ALGO_ASTAR = NAV_ALGO_ASTAR,
    PATH_ALGO_HPA = NAV_ALGO_HPA,
} PathAlgorithm;

#define MAX_ENTRANCE_WIDTH  NAV_MAX_ENTRANCE_WIDTH
#define MAX_ENTRANCES       NAV_MAX_ENTRANCES
#define MAX_EDGES           NAV_MAX_EDGES
#define MAX_LADDERS         NAV_MAX_LADDERS
#define MAX_RAMP_LINKS      NAV_MAX_RAMP_LINKS
#define MAX_PATH 65536      // game path buffer size (debug path, temp buffers)

// ---- the game's world ----
NavWorld* NavGame(void);

// ---- legacy global names -> NavGame() fields ----
#define entrances         (NavGame()->entranceList)
#define entranceCount     (NavGame()->entranceNum)
#define graphEdges        (NavGame()->edgeList)
#define graphEdgeCount    (NavGame()->edgeNum)
#define ladderLinks       (NavGame()->ladderList)
#define ladderLinkCount   (NavGame()->ladderNum)
#define rampLinks         (NavGame()->rampList)
#define rampLinkCount     (NavGame()->rampNum)
#define use8Dir           (NavGame()->eightDir)
#define needsRebuild      (NavGame()->dirty)
#define hpaNeedsRebuild   (NavGame()->hpaDirty)
#define nodesExplored     (NavGame()->explored)
#define lastPathTime      (NavGame()->lastMs)
#define lastPathCost      (NavGame()->lastCost)
#define hpaAbstractTime   (NavGame()->hpaAbstractMs)
#define hpaRefinementTime (NavGame()->hpaRefineMs)

// ---- debug/demo path (the "Find Path" button, agent demo) ----
extern Point path[MAX_PATH];
extern int pathLength;
extern Point startPos;
extern Point goalPos;

// ---- path stats (updated every 5 seconds) ----
extern int pathStatsCount;
extern double pathStatsTotalMs;
extern double pathStatsAvgMs;
extern bool pathStatsUpdated;
void UpdatePathStats(void);       // call every frame
void ResetPathStats(void);        // call when switching algorithms

// ---- graph maintenance ----
void MarkChunkDirty(int cellX, int cellY, int cellZ);
bool IsChunkDirty(int cx, int cy, int z);
void SetChunkDirty(int cx, int cy, int z, bool dirty);
void BuildEntrances(void);
void BuildGraph(void);
void UpdateDirtyChunks(void);

// ---- queries ----
void RunAStar(void);              // startPos -> goalPos into path/pathLength
void RunHPAStar(void);            // startPos -> goalPos into path/pathLength
int FindPathHPA(Point start, Point goal, Point* outPath, int maxLen);
int FindPath(PathAlgorithm algo, Point start, Point goal, Point* outPath, int maxLen);
bool IsReachable(Point a, Point b);   // O(1) via reachability regions ("false" is always right)
int AStarChunk(int sx, int sy, int sz, int gx, int gy, int minX, int minY, int maxX, int maxY);
int AStarChunkMultiTarget(int sx, int sy, int sz, int* targetX, int* targetY, int* outCosts, int numTargets,
                          int minX, int minY, int maxX, int maxY);

// ---- random cell utilities (demo spawning, tests) ----
void SeedRandom(unsigned int seed);
Point GetRandomWalkableCell(void);
Point GetRandomWalkableCellDifferentZ(int excludeZ);
Point GetRandomWalkableCellOnZ(int z);
Point GetRandomReachableCell(int startX, int startY, int startZ);

// ---- entrance / edge getters ----
static inline int GetEntranceX(int idx) { return entrances[idx].x; }
static inline int GetEntranceY(int idx) { return entrances[idx].y; }
static inline int GetEntranceZ(int idx) { return entrances[idx].z; }
static inline int GetEntranceChunk1(int idx) { return entrances[idx].chunk1; }
static inline int GetEntranceChunk2(int idx) { return entrances[idx].chunk2; }
static inline int GetGraphEdgeFrom(int idx) { return graphEdges[idx].from; }
static inline int GetGraphEdgeTo(int idx) { return graphEdges[idx].to; }
static inline int GetGraphEdgeCost(int idx) { return graphEdges[idx].cost; }

#endif // PATHFINDING_H
