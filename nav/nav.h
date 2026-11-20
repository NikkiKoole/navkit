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
// Optional (macros; default off):
//   NAV_REGION_WALKABLE(host, x, y, z)   superset of walkable used for reachability regions
//   NAV_CLIMB_EXTRA(host, x, y, z, dz)   extra cost (>= 0) of changing level from (x,y,z)
//        by dz via a ladder/ramp there, e.g. a queue's expected wait. Read at query
//        time by all searches; cached NavHFlow results need NavHFlowInvalidate when
//        it changes.
// Out-of-bounds coordinates may be passed; return false / a safe default.
// =============================================================================

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Graph capacity limits (per world)
#define NAV_MAX_ENTRANCE_WIDTH      6
// Initial capacities: entrances, edges, ladder and ramp links all grow as needed
// (512x512x16 hills needs ~60k entrances; tiny worlds stay small)
#define NAV_MAX_ENTRANCES           (4096 * 4)
#define NAV_MAX_EDGES               (65536 * 4)
#define NAV_MAX_LADDERS             1024
#define NAV_MAX_RAMP_LINKS          8192
#define NAV_INITIAL_ENTRANCES       1024
#define NAV_INITIAL_LADDERS         64
#define NAV_INITIAL_RAMP_LINKS      256
#define NAV_MAX_ENTRANCES_PER_CHUNK 128   // per chunk edge region (full and incremental builds alike)

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
    int chunk;            // chunk the cost was computed in; -1 for ladder/ramp links
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
typedef struct { int x, y, z; int index; int chunk1, chunk2; bool used; } NavHashEntry;

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
    NavGraphEdge*  edgeList;       int edgeNum;   // sorted by (from, to); see adjStart
    NavLadderLink* ladderList;     int ladderNum;
    NavRampLink*   rampList;       int rampNum;
    int            rampCellNum;    // ramp cells seen by the last NavBuildEntrances
    bool*          chunkDirtyFlags;// [depth][chunksY][chunksX]
    bool           dirty;          // some chunk changed since last build/update
    bool           hpaDirty;       // HPA* graph is stale
    unsigned       graphVersion;   // bumped whenever the HPA* graph changes (cache invalidation)

    // ---- stats from the most recent query ----
    int    explored;
    int    lastCost;               // g-cost at goal from last A* (-1 = unreachable)
    double lastMs;
    double hpaAbstractMs;
    double hpaRefineMs;

    // ---- private scratch ----
    int* adjStart;                 // [entrance + 1]: out-edges of e are edgeList[adjStart[e] .. adjStart[e+1])
    int edgeCap;
    int entranceCap, ladderCap, rampCap, entHashSize;   // current capacities (grow as needed)
    NavGraphEdge* edgeScratch; int edgeScratchCap;      // merge buffer (swapped with edgeList)
    bool growFailed;               // out of memory growing a list (warned)
    bool edgeOverflow;             // couldn't grow edgeList (warned)

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
    bool chunkEntsOverflow;        // a chunk region had more entrances than fit (warned)
    int* chunkEntCount;
    bool* affected;                // [depth][chunksY][chunksX]

    NavPoint* tempPath; int tempPathCap;

    // reachability regions (built lazily on first query)
    uint16_t* regLocal;            // [cell] component id within its chunk, 0 = not walkable
    int* regCompBase;              // [chunk] first global component index
    int* regCompCount;             // [chunk] components in the chunk
    int* regRoot;                  // [global component] region id (flattened union-find)
    int regNodeCap, regCount;
    struct NavRegLinks* regLinks;  // [chunk] cross-level links (ladders/ramps) starting in it
    bool* regChunkDirty;
    int regDirtyCount;
    bool regBuilt;

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

    // Any-angle steering (Theta*-style, computed during the build when anyAngle):
    // steer[c] is the farthest cell along c's route that c can walk to in a
    // straight line on the same level, without crossing walls or terrain more
    // expensive than the route itself. Steering at it instead of `next` removes
    // the 8-direction zig-zag/lanes. -1 at goals / unreachable cells.
    bool anyAngle;                 // default true
    int* steer;

    // Optional per-cell extra cost [z][y][x] added when entering a cell, on top
    // of NavMoveCost (crowd density, danger, wall clearance, ...). Changes which
    // route is cheapest; does NOT restrict any-angle line-of-sight steering.
    // Owned by the caller; NULL = none.
    const int* extraCost;
    int* steerLimit;               // private: max cell cost along c -> steer[c]

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

// Distance (in cells, 8-connected, capped at 255) from every walkable cell to
// the nearest non-walkable cell or map edge on the same level; 0 for
// non-walkable cells. `out` must hold width*height*depth bytes. Useful as input
// for an extraCost wall-clearance penalty.
void NavComputeClearance(NavWorld* w, uint8_t* out);

// Straight-line steering target from p (see `steer`). False at a goal, if
// unreachable, out of bounds, or the field was built without anyAngle.
static inline bool NavFlowSteerTarget(const NavFlowField* f, NavPoint p, NavPoint* out) {
    if (!NavFlowInBounds(f, p) || !f->anyAngle) return false;
    int t = f->steer[NavFlowCellId(f, p.x, p.y, p.z)];
    if (t < 0) return false;
    *out = NavFlowCellPoint(f, t);
    return true;
}

// ---- reachability regions ----
// Every walkable cell gets a region id; two cells are mutually reachable iff
// they share a region (O(1)). Uses the same movement rules as A* (8-dir with
// no corner cutting, ladders, ramps); a connection counts if it works in at
// least one direction, so a one-way ramp side-exit can make "reachable" a rare
// false positive, while "unreachable" is always correct. Built on first query;
// NavMarkCellChanged makes the next query relabel only the nearby chunks.
int  NavRegionId(NavWorld* w, NavPoint p);          // -1 = not walkable / out of bounds
bool NavReachable(NavWorld* w, NavPoint a, NavPoint b);
int  NavRegionCount(NavWorld* w);                   // number of distinct regions
// The world changed in ways not reported through NavMarkCellChanged: relabel
// everything on the next query.
void NavRegionsInvalidate(NavWorld* w);
// Hosts may #define NAV_REGION_WALKABLE(host,x,y,z) as a superset of
// NavIsWalkable (e.g. ignoring dynamic blockers they don't notify about).

// ---- hierarchical flow fields ----
// For many different destinations: one flow field per goal over the whole
// world is too expensive, so a NavHFlow does a cheap Dijkstra over the HPA*
// entrance graph (cost-to-goal per entrance), then builds small per-chunk
// flow fields lazily, only for chunks agents actually ask about, and caches
// them. Requires NavBuildEntrances + NavBuildGraph. Rebuilds itself when the
// graph changes (graphVersion). Costs are HPA*-quality (near-optimal).
typedef struct NavHFlow {
    NavPoint goal;
    int goalChunk;
    unsigned graphVersion;
    bool valid;                    // goal walkable and connected to the graph
    int entNum;                    // entrance count the per-entrance arrays were built for
    int* dist;                     // [entrance] cost to goal (NAV_FLOW_UNREACHABLE if none)
    int* nextEnt;                  // [entrance] next entrance toward goal; -1 = the goal itself
    int* viaChunk;                 // [entrance] chunk of the edge to nextEnt (-1 = ladder/ramp link)
    int chunkCount;
    struct NavChunkFlow** chunks;  // [chunk id] lazily built local fields (NULL = not built yet)
    int chunksBuilt;               // stats
    double abstractMs;

    // Optional per-cell extra cost [z][y][x] (same meaning as NavFlowField.extraCost),
    // caller-owned. Local chunk fields add it to every cell entered; the entrance graph
    // adds the extra cost of each entrance cell an edge leads into (so connector costs
    // such as ladder queues steer the route choice). Call NavHFlowInvalidate after
    // changing the values.
    const int* extraCost;
    bool stale;                    // rebuild on next query (set by NavHFlowInvalidate)

    // private: lazy-build bookkeeping (fields built during one outer request)
    int buildDepth;
    int* fresh; int freshNum, freshCap;
} NavHFlow;

NavHFlow* NavHFlowCreate(NavWorld* w, NavPoint goal);
void NavHFlowDestroy(NavHFlow* h);
// Costs changed (extraCost values): drop cached results; rebuilt lazily on next query.
void NavHFlowInvalidate(NavHFlow* h);
// Next cell to step to from p (may be on another level via ladder/ramp).
// False at the goal, if unreachable, or out of bounds. Builds chunk fields on demand.
bool NavHFlowNext(NavWorld* w, NavHFlow* h, NavPoint p, NavPoint* out);
// Cost from p to the goal along the hierarchical route, or NAV_FLOW_UNREACHABLE.
int  NavHFlowCostAt(NavWorld* w, NavHFlow* h, NavPoint p);
size_t NavHFlowMemoryUsage(const NavHFlow* h);

// ---- nearest target by walking distance ----
// Search outward from start (same rules and costs as A*) and stop at the
// cheapest cell isTarget accepts: the truly nearest target by walking, on any
// level, in one search. maxCost > 0 bounds the search (e.g. 10 * cells).
// Returns the cost to the target, or -1 if none was reached. *found receives
// the target cell; if outPath is given, the path (goal-first) and *pathLen.
typedef bool (*NavTargetFn)(void* user, int x, int y, int z);
int NavFindNearest(NavWorld* w, NavPoint start, NavTargetFn isTarget, void* user, int maxCost,
                   NavPoint* found, NavPoint* outPath, int maxLen, int* pathLen);

// Bounded single-level searches (used internally; exposed for tests/tools).
// Bounds are [min, max). Return cost, or -1 if unreachable.
int NavChunkCost(NavWorld* w, int sx, int sy, int sz, int gx, int gy,
                 int minX, int minY, int maxX, int maxY);
// Dijkstra from (sx,sy) to many targets; outCosts[i] = -1 if unreachable. Returns #found.
int NavChunkCostsMulti(NavWorld* w, int sx, int sy, int sz,
                       const int* targetX, const int* targetY, int* outCosts, int numTargets,
                       int minX, int minY, int maxX, int maxY);

#endif // NAV_H
