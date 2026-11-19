#ifndef NAV_H
#define NAV_H

// =============================================================================
// nav — 3D grid pathfinding (A* and HPA*) with variable terrain cost,
//       ladders and directional ramps.
// =============================================================================
//
// Everything lives in a NavWorld you create with NavCreate(); all memory is
// sized to the world you ask for. Several worlds can coexist. A single world
// is NOT safe to query from several threads at once (searches use per-world
// scratch buffers).
//
// The library knows nothing about your game. It asks the world questions
// through a small host interface (see "Host contract" below), compiled in as
// static inline functions for zero call overhead. Point the build at your host
// header with -DNAV_HOST_HEADER="\"path/to/my_nav_host.h\"" (default:
// "nav_host.h" on the include path).
//
// Coordinates are (x, y, z) with z = vertical level. Paths are returned
// GOAL-FIRST: out[0] = goal, out[len-1] = start.
//
// Costs are fixed-point: 10 = 1.0x. Cardinal step = 10 * cellCost / 10,
// diagonal = 14 * cellCost / 10, ladder = cellCost, ramp = 14 * cellCost / 10.
//
// Host contract (your nav host header must define these; `host` is the
// pointer you passed to NavCreate):
//   NAV_MIN_CELL_COST                       cheapest possible cell cost (heuristic)
//   bool NavIsWalkable(void* host, x, y, z)
//   int  NavMoveCost(void* host, x, y, z)   >= NAV_MIN_CELL_COST
//   bool NavCanClimbUp(void* host, x, y, z)     (x,y,z) -> (x,y,z+1)
//   bool NavCanClimbDown(void* host, x, y, z)   (x,y,z) -> (x,y,z-1)
//   bool NavRampHighSide(void* host, x, y, z, int* dx, int* dy)
//   bool NavRampUp(void* host, x, y, z, int* exitX, int* exitY)
//   bool NavCanEnterFrom(void* host, x, y, z, fromX, fromY)
//   int  NavAffectedZ(void* host, z, int* outZ)    extra levels a change at z affects
//   void NavOnChunkDirty(void* host, x, y, z)      notification hook
//   double NavHostTime(void)                       seconds, for timing stats
//   NAV_LOG_INFO(...), NAV_LOG_WARN(...)           printf-style logging
// Out-of-bounds coordinates may be passed; return false / a safe default.
// =============================================================================

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Graph capacity limits (per world)
#define NAV_MAX_ENTRANCE_WIDTH      6
#define NAV_MAX_ENTRANCES           (4096 * 4)
#define NAV_MAX_EDGES               (65536 * 4)
#define NAV_MAX_EDGES_PER_NODE      64
#define NAV_MAX_LADDERS             1024
#define NAV_MAX_RAMP_LINKS          8192
#define NAV_MAX_ENTRANCES_PER_CHUNK 64

typedef enum {
    NAV_ALGO_ASTAR = 0,
    NAV_ALGO_HPA = 1,
} NavAlgorithm;

typedef struct NavPoint { int x, y, z; } NavPoint;

typedef struct {
    int x, y, z;
    int chunk1, chunk2;   // equal for ladder/ramp entrances
} NavEntrance;

typedef struct {
    int from, to;
    int cost;
} NavGraphEdge;

typedef struct {
    int x, y;             // same on both levels
    int zLow, zHigh;
    int entranceLow, entranceHigh;
    int cost;
} NavLadderLink;

typedef struct {
    int rampX, rampY, rampZ;   // ramp cell (lower level)
    int exitX, exitY;          // exit cell at rampZ + 1
    int entranceRamp, entranceExit;
    int cost;
} NavRampLink;

// Private scratch types (layout may change)
typedef struct { int g, f; int parentX, parentY; bool open, closed; } NavChunkNode;
typedef struct { int g, f; int parent; bool open, closed; } NavAbstractNode;
typedef struct { int x, y, z; int index; bool isLadder; } NavHashEntry;

typedef struct NavWorld {
    // ---- shape (read-only; change with NavResize) ----
    int width, height, depth;
    int chunkW, chunkH;
    int chunksX, chunksY;
    void* host;

    // ---- settings ----
    bool eightDir;                 // 8-directional movement (default true)

    // ---- HPA* graph (read-only for users) ----
    NavEntrance*   entranceList;   int entranceNum;
    NavGraphEdge*  edgeList;       int edgeNum;
    NavLadderLink* ladderList;     int ladderNum;
    NavRampLink*   rampList;       int rampNum;
    int            rampCellNum;    // ramp cells seen by the last NavBuildEntrances
    bool*          chunkDirtyFlags;// [depth][chunksY][chunksX]
    bool           dirty;          // some chunk changed since last build/update
    bool           hpaDirty;       // HPA* graph is stale

    // ---- stats from the most recent query ----
    int    explored;
    int    lastCost;               // g-cost at goal from last A* (-1 = unreachable)
    double lastMs;
    double hpaAbstractMs;
    double hpaRefineMs;

    // ---- private scratch ----
    int (*adjList)[NAV_MAX_EDGES_PER_NODE];
    int* adjListCount;

    NavAbstractNode* absNodes;
    int* absPath; int absPathLen;
    int* absHeap; int absHeapSize; int* absHeapPos;

    NavChunkNode* chunkNodes;      // [height][width], single z-layer
    int* chunkHeapPos;             // [height][width]
    int* chunkHeap; int chunkHeapSize; int chunkHeapCap;

    NavHashEntry* entHash;
    int* oldToNew;
    NavEntrance* oldEntrances; int oldEntranceNum;
    int (*chunkEnts)[NAV_MAX_ENTRANCES_PER_CHUNK];
    int* chunkEntCount;
    bool* affected;                // [depth][chunksY][chunksX]

    NavPoint* tempPath; int tempPathCap;

    int* aG; int* aF; int* aParent;
    uint8_t* aState; uint32_t* aGen;
    int* aHeap; int* aHeapIdx; int aHeapCount;
    uint32_t aCurGen;
} NavWorld;

// ---- lifecycle ----
NavWorld* NavCreate(int width, int height, int depth, int chunkW, int chunkH, void* host);
void      NavDestroy(NavWorld* w);
// Re-shape the world. Drops the HPA* graph (call NavBuildEntrances/NavBuildGraph again).
bool      NavResize(NavWorld* w, int width, int height, int depth, int chunkW, int chunkH);
size_t    NavMemoryUsage(const NavWorld* w);

// ---- world changes ----
// Tell the library a cell changed (walkability or cost). Marks chunks dirty.
void NavMarkCellChanged(NavWorld* w, int x, int y, int z);
bool NavIsChunkDirty(const NavWorld* w, int cx, int cy, int z);
void NavSetChunkDirty(NavWorld* w, int cx, int cy, int z, bool dirty);

// ---- HPA* graph ----
void NavBuildEntrances(NavWorld* w);   // full rebuild, step 1
void NavBuildGraph(NavWorld* w);       // full rebuild, step 2
void NavUpdateDirty(NavWorld* w);      // incremental rebuild of dirty chunks

// ---- queries ---- (return path length, 0 = no path; path is goal-first)
int NavFindPath(NavWorld* w, NavAlgorithm algo, NavPoint start, NavPoint goal, NavPoint* out, int maxLen);
int NavFindPathAStar(NavWorld* w, NavPoint start, NavPoint goal, NavPoint* out, int maxLen);
int NavFindPathHPA(NavWorld* w, NavPoint start, NavPoint goal, NavPoint* out, int maxLen);

// ---- flow fields ----
// One reverse Dijkstra from the goal(s) over the whole world, using exactly the
// same movement rules and costs as A* (variable cost, ladders, ramps). Afterwards
// every cell knows its cost to the nearest goal and the next cell to step to, so
// any number of agents sharing those goals navigate with an O(1) lookup each.
// Memory: 8 bytes per cell (+ a build heap). Rebuild after the world changes.
#define NAV_FLOW_UNREACHABLE 0x7fffffff

typedef struct NavFlowField {
    int width, height, depth;      // shape of the world it was created for
    int* cost;                     // [z][y][x] cost to nearest goal, or NAV_FLOW_UNREACHABLE
    int* next;                     // [z][y][x] cell id of next step; -1 at goals / unreachable
    int reached;                   // cells reached by the last build
    double buildMs;                // duration of the last build

    // private build heap (lazy deletion: entries may be stale)
    int* heapCost; int* heapCell; int heapSize; int heapCap;
} NavFlowField;

NavFlowField* NavFlowCreate(const NavWorld* w);
void NavFlowDestroy(NavFlowField* f);
// Rebuild for these goals (unwalkable / out-of-bounds goals are ignored).
// Returns the number of reachable cells. maxCost > 0 stops expanding beyond that
// cost (cheap local fields); pass 0 for no limit.
int  NavFlowBuild(NavWorld* w, NavFlowField* f, const NavPoint* goals, int goalCount, int maxCost);

static inline int NavFlowCellId(const NavFlowField* f, int x, int y, int z) {
    return (z * f->height + y) * f->width + x;
}
static inline NavPoint NavFlowCellPoint(const NavFlowField* f, int id) {
    int layer = f->width * f->height;
    int rem = id % layer;
    NavPoint p = {rem % f->width, rem / f->width, id / layer};
    return p;
}
static inline bool NavFlowInBounds(const NavFlowField* f, NavPoint p) {
    return p.x >= 0 && p.y >= 0 && p.z >= 0 && p.x < f->width && p.y < f->height && p.z < f->depth;
}
// Cost from p to the nearest goal (NAV_FLOW_UNREACHABLE if none / out of bounds)
static inline int NavFlowCostAt(const NavFlowField* f, NavPoint p) {
    return NavFlowInBounds(f, p) ? f->cost[NavFlowCellId(f, p.x, p.y, p.z)] : NAV_FLOW_UNREACHABLE;
}
// Next cell to step to from p. False at a goal, if unreachable, or out of bounds.
static inline bool NavFlowNext(const NavFlowField* f, NavPoint p, NavPoint* out) {
    if (!NavFlowInBounds(f, p)) return false;
    int n = f->next[NavFlowCellId(f, p.x, p.y, p.z)];
    if (n < 0) return false;
    *out = NavFlowCellPoint(f, n);
    return true;
}

// Bounded single-level searches (used internally; exposed for tests/tools).
// Bounds are [min, max). Return cost, or -1 if unreachable.
int NavChunkCost(NavWorld* w, int sx, int sy, int sz, int gx, int gy,
                 int minX, int minY, int maxX, int maxY);
// Dijkstra from (sx,sy) to many targets; outCosts[i] = -1 if unreachable. Returns #found.
int NavChunkCostsMulti(NavWorld* w, int sx, int sy, int sz,
                       const int* targetX, const int* targetY, int* outCosts, int numTargets,
                       int minX, int minY, int maxX, int maxY);

#endif // NAV_H
