// nav.c — 3D grid pathfinding library (A*, HPA*). See nav.h.
//
// Ported from NavKit's src/world/pathfinding.c (2026-11). All state lives in
// NavWorld; nothing here may use the old global names (entrances, use8Dir, ...)
// because the game's compatibility layer defines macros with those names.

#include "nav.h"

#ifdef NAV_HOST_HEADER
#include NAV_HOST_HEADER
#else
#include "nav_host.h"
#endif

#include <stdlib.h>
#include <string.h>

#define NAV_COST_INF 999999
#define NAV_CONNECT_MAX NAV_MAX_ENTRANCES_PER_CHUNK   // entrances considered per chunk region

#define NAV_HOSTP (w->host)

// Walkability used for reachability regions. A host may define it as a
// superset of NavIsWalkable (e.g. ignoring dynamic blockers it doesn't report
// through NavMarkCellChanged), which keeps "unreachable" answers correct.
#ifndef NAV_REGION_WALKABLE
#define NAV_REGION_WALKABLE NavIsWalkable
#endif

// Optional extra cost of changing level from (x,y,z) by dz (+1 up, -1 down) via a
// ladder or ramp there, e.g. the expected wait in a ladder queue. Added to that
// step by every search (A*, nearest, flow fields, HPA* link edges at query time,
// hierarchical flow), so they stay consistent. Must be >= 0.
#ifndef NAV_CLIMB_EXTRA
#define NAV_CLIMB_EXTRA(host, x, y, z, dz) 0
#endif

// Forward declarations (reachability regions, defined further down)
static void nav_RegMarkDirty(NavWorld* w, int x, int y, int z);
static void nav_RegFree(NavWorld* w);

static const int nav_kDx4[4] = {0, 1, 0, -1};
static const int nav_kDy4[4] = {-1, 0, 1, 0};
static const int nav_kDx8[8] = {0, 1, 1, 1, 0, -1, -1, -1};
static const int nav_kDy8[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

// =============================================================================
// Lifecycle
// =============================================================================

static void nav_NavFreeInternals(NavWorld* w) {
    nav_RegFree(w);
    if (w->adj) for (int e = 0; e < w->entranceCap; e++) free(w->adj[e].e);
    free(w->adj); free(w->flatEdges);
    free(w->entranceList); free(w->ladderList); free(w->rampList);
    free(w->chunkDirtyFlags);
    free(w->absNodes); free(w->absPath); free(w->absHeap); free(w->absHeapPos);
    free(w->chunkNodes); free(w->chunkHeapPos); free(w->chunkHeap);
    free(w->entScratch); free(w->entMoved); free(w->entStamp);
    free(w->chunkEnts); free(w->chunkEntCount); free(w->affected); free(w->rebuiltChunks);
    free(w->tempPath);
    free(w->aG); free(w->aF); free(w->aParent); free(w->aState); free(w->aGen);
    free(w->aHeap); free(w->aHeapIdx);
}

static bool nav_NavAllocInternals(NavWorld* w, int width, int height, int depth, int chunkW, int chunkH) {
    if (width < 1) width = 1;
    if (height < 1) height = 1;
    if (depth < 1) depth = 1;
    if (chunkW < 1 || chunkW > width) chunkW = width;
    if (chunkH < 1 || chunkH > height) chunkH = height;

    w->width = width; w->height = height; w->depth = depth;
    w->chunkW = chunkW; w->chunkH = chunkH;
    w->chunksX = (width + chunkW - 1) / chunkW;
    w->chunksY = (height + chunkH - 1) / chunkH;

    size_t layer = (size_t)width * height;
    size_t chunks = (size_t)depth * w->chunksX * w->chunksY;

    w->flatEdges      = NULL; w->flatCap = 0; w->flatVersion = (unsigned)-1;
    w->growFailed     = false;
    w->entranceCap    = NAV_INITIAL_ENTRANCES;
    w->ladderCap      = NAV_INITIAL_LADDERS;
    w->rampCap        = NAV_INITIAL_RAMP_LINKS;
    w->entranceList   = calloc((size_t)w->entranceCap, sizeof(NavEntrance));
    w->adj            = calloc((size_t)w->entranceCap, sizeof(NavEdgeVec));
    w->ladderList     = calloc((size_t)w->ladderCap, sizeof(NavLadderLink));
    w->rampList       = calloc((size_t)w->rampCap, sizeof(NavRampLink));
    w->chunkDirtyFlags= calloc(chunks, sizeof(bool));
    w->absNodes       = calloc((size_t)w->entranceCap + 2, sizeof(NavAbstractNode));
    w->absPath        = calloc((size_t)w->entranceCap + 2, sizeof(int));
    w->absHeap        = calloc((size_t)w->entranceCap + 2, sizeof(int));
    w->absHeapPos     = calloc((size_t)w->entranceCap + 2, sizeof(int));
    w->chunkNodes     = calloc(layer, sizeof(NavChunkNode));
    w->chunkHeapPos   = calloc(layer, sizeof(int));
    w->chunkHeap      = calloc(layer, sizeof(int));
    w->chunkHeapCap   = (int)layer;
    w->entScratch     = calloc((size_t)w->entranceCap, sizeof(int));
    w->entMoved       = calloc((size_t)w->entranceCap, sizeof(int));
    w->entStamp       = calloc((size_t)w->entranceCap, sizeof(unsigned));
    w->stampNow       = 0;
    w->chunkEnts      = calloc(chunks, sizeof(*w->chunkEnts));
    w->chunkEntCount  = calloc(chunks, sizeof(int));
    w->affected       = calloc(chunks, sizeof(bool));
    w->rebuiltChunks  = calloc(chunks, sizeof(bool));
    w->tempPathCap    = (int)layer;
    w->tempPath       = calloc(layer, sizeof(NavPoint));
    // Full-grid A* buffers (W*H*D) are allocated lazily on the first A* query
    w->aG = w->aF = w->aParent = w->aHeap = w->aHeapIdx = NULL;
    w->aState = NULL;
    w->aGen = NULL;
    w->aHeapCount = 0;
    w->aCurGen = 0;

    w->entranceNum = w->edgeNum = w->ladderNum = w->rampNum = w->rampCellNum = 0;
    w->absPathLen = 0;
    w->dirty = true;
    w->hpaDirty = true;

    return w->entranceList && w->adj && w->ladderList && w->rampList && w->chunkDirtyFlags
        && w->absNodes && w->absPath && w->absHeap && w->absHeapPos
        && w->chunkNodes && w->chunkHeapPos && w->chunkHeap && w->entScratch && w->entMoved
        && w->entStamp && w->chunkEnts && w->chunkEntCount && w->affected && w->rebuiltChunks && w->tempPath;
}

NavWorld* NavCreate(int width, int height, int depth, int chunkW, int chunkH, void* host) {
    NavWorld* w = calloc(1, sizeof(NavWorld));
    if (!w) return NULL;
    w->host = host;
    w->eightDir = true;
    w->lastCost = -1;
    if (!nav_NavAllocInternals(w, width, height, depth, chunkW, chunkH)) {
        nav_NavFreeInternals(w);
        free(w);
        return NULL;
    }
    return w;
}

void NavDestroy(NavWorld* w) {
    if (!w) return;
    nav_NavFreeInternals(w);
    free(w);
}

bool NavResize(NavWorld* w, int width, int height, int depth, int chunkW, int chunkH) {
    nav_NavFreeInternals(w);
    void* host = w->host;
    bool eightDir = w->eightDir;
    unsigned version = w->graphVersion;
    memset(w, 0, sizeof(*w));
    w->host = host;
    w->eightDir = eightDir;
    w->graphVersion = version + 1;   // cached hierarchical fields must notice
    w->lastCost = -1;
    return nav_NavAllocInternals(w, width, height, depth, chunkW, chunkH);
}

size_t NavMemoryUsage(const NavWorld* w) {
    size_t layer = (size_t)w->width * w->height;
    size_t cells = layer * w->depth;
    size_t chunks = (size_t)w->depth * w->chunksX * w->chunksY;
    return sizeof(NavWorld)
        + (size_t)w->entranceCap * (sizeof(NavEntrance) + sizeof(int) * 2 + sizeof(unsigned))
        + (size_t)w->entranceCap * sizeof(NavEdgeVec) + (size_t)w->edgeNum * sizeof(NavGraphEdge)
        + (size_t)w->flatCap * sizeof(NavGraphEdge)
        + (size_t)w->ladderCap * sizeof(NavLadderLink)
        + (size_t)w->rampCap * sizeof(NavRampLink)
        + ((size_t)w->entranceCap + 2) * (sizeof(NavAbstractNode) + sizeof(int) * 3)
        + chunks * (sizeof(bool) * 3 + sizeof(*w->chunkEnts) + sizeof(int))
        + layer * (sizeof(NavChunkNode) + sizeof(int) * 2 + sizeof(NavPoint))
        + (w->aG ? cells * (sizeof(int) * 5 + 1 + sizeof(uint32_t)) : 0);
}

// =============================================================================
// Small helpers
// =============================================================================

static inline int nav_ChunkFlagIndex(const NavWorld* w, int cx, int cy, int z) {
    return (z * w->chunksY + cy) * w->chunksX + cx;
}

bool NavIsChunkDirty(const NavWorld* w, int cx, int cy, int z) {
    if (cx < 0 || cx >= w->chunksX || cy < 0 || cy >= w->chunksY || z < 0 || z >= w->depth) return false;
    return w->chunkDirtyFlags[nav_ChunkFlagIndex(w, cx, cy, z)];
}

void NavSetChunkDirty(NavWorld* w, int cx, int cy, int z, bool dirty) {
    if (cx < 0 || cx >= w->chunksX || cy < 0 || cy >= w->chunksY || z < 0 || z >= w->depth) return;
    w->chunkDirtyFlags[nav_ChunkFlagIndex(w, cx, cy, z)] = dirty;
}

static void nav_ClearAllChunkDirty(NavWorld* w) {
    memset(w->chunkDirtyFlags, 0, (size_t)w->depth * w->chunksX * w->chunksY * sizeof(bool));
    w->dirty = false;
    w->hpaDirty = false;
}

static int nav_Heuristic2D(int x1, int y1, int x2, int y2) {
    return abs(x1 - x2) + abs(y1 - y2);
}

// 8-dir heuristic scaled by NAV_MIN_CELL_COST so it stays admissible with
// variable terrain cost: cardinal = MIN, diagonal = 14*MIN/10.
static int nav_Heuristic8Dir(int x1, int y1, int x2, int y2) {
    int dx = abs(x1 - x2);
    int dy = abs(y1 - y2);
    int minD = dx < dy ? dx : dy;
    int maxD = dx > dy ? dx : dy;
    return NAV_MIN_CELL_COST * maxD + ((14 * NAV_MIN_CELL_COST) / 10 - NAV_MIN_CELL_COST) * minD;
}

static int nav_ChunkHeuristic(const NavWorld* w, int x1, int y1, int x2, int y2) {
    return w->eightDir ? nav_Heuristic8Dir(x1, y1, x2, y2) : nav_Heuristic2D(x1, y1, x2, y2) * NAV_MIN_CELL_COST;
}

// Admissible with ramps: a ramp step moves one cell across AND one level for
// 1.4 x cell cost, i.e. at most 1 x MIN of horizontal progress plus 0.4 x MIN for
// the level (a ladder charges a full MIN per level). Charging a full MIN per level
// on top of the horizontal distance overestimated, and A* returned longer paths.
static int nav_Heuristic3D(const NavWorld* w, int x0, int y0, int z0, int x1, int y1, int z1) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int dz = abs(z1 - z0);
    int perLevel = (4 * NAV_MIN_CELL_COST) / 10;
    if (w->eightDir) {
        int maxXY = (dx > dy) ? dx : dy;
        int minXY = (dx < dy) ? dx : dy;
        return (maxXY - minXY) * NAV_MIN_CELL_COST + minXY * ((14 * NAV_MIN_CELL_COST) / 10)
             + dz * perLevel;
    }
    return (dx + dy) * NAV_MIN_CELL_COST + dz * perLevel;
}

// Chunks whose edge-search region contains cell (x,y,z). A chunk's edges are
// searched over the chunk plus the first row/column of its right/lower
// neighbours, so a cell in a chunk's first row/column also belongs to the
// chunk(s) above/left of it. Border entrances sit exactly there (the lower/right
// side of the border); ladder and ramp cells can too, and may be reachable only
// from the neighbour's side. Returns the count (1..4).
static int nav_EdgeChunksOfCell(const NavWorld* w, int x, int y, int z, int* out) {
    int perLevel = w->chunksX * w->chunksY;
    int cx = x / w->chunkW, cy = y / w->chunkH, n = 0;
    bool left = (x % w->chunkW == 0) && cx > 0, up = (y % w->chunkH == 0) && cy > 0;
    out[n++] = z * perLevel + cy * w->chunksX + cx;
    if (left) out[n++] = z * perLevel + cy * w->chunksX + cx - 1;
    if (up) out[n++] = z * perLevel + (cy - 1) * w->chunksX + cx;
    if (left && up) out[n++] = z * perLevel + (cy - 1) * w->chunksX + cx - 1;
    return n;
}

// Chunk -> entrance index list (every chunk whose edge region holds the entrance)
static void nav_BuildChunkEntranceIndex(NavWorld* w) {
    int totalChunks = w->depth * w->chunksX * w->chunksY;
    memset(w->chunkEntCount, 0, (size_t)totalChunks * sizeof(int));
    w->chunkEntsOverflow = false;
    for (int i = 0; i < w->entranceNum; i++) {
        const NavEntrance* e = &w->entranceList[i];
        int cs[4];
        int n = nav_EdgeChunksOfCell(w, e->x, e->y, e->z, cs);
        for (int k = 0; k < n; k++) {
            if (w->chunkEntCount[cs[k]] < NAV_MAX_ENTRANCES_PER_CHUNK)
                w->chunkEnts[cs[k]][w->chunkEntCount[cs[k]]++] = i;
            else
                w->chunkEntsOverflow = true;
        }
    }
}

// =============================================================================
// Abstract-graph binary heap (indexed, decrease-key)
// =============================================================================

static void nav_AbsHeapInit(NavWorld* w, int numNodes) {
    w->absHeapSize = 0;
    for (int i = 0; i < numNodes; i++) w->absHeapPos[i] = -1;
}

static void nav_AbsHeapSwap(NavWorld* w, int i, int j) {
    int a = w->absHeap[i], b = w->absHeap[j];
    w->absHeap[i] = b; w->absHeap[j] = a;
    w->absHeapPos[a] = j; w->absHeapPos[b] = i;
}

static void nav_AbsHeapUp(NavWorld* w, int idx) {
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (w->absNodes[w->absHeap[idx]].f < w->absNodes[w->absHeap[parent]].f) {
            nav_AbsHeapSwap(w, idx, parent);
            idx = parent;
        } else break;
    }
}

static void nav_AbsHeapDown(NavWorld* w, int idx) {
    for (;;) {
        int l = 2 * idx + 1, r = l + 1, s = idx;
        if (l < w->absHeapSize && w->absNodes[w->absHeap[l]].f < w->absNodes[w->absHeap[s]].f) s = l;
        if (r < w->absHeapSize && w->absNodes[w->absHeap[r]].f < w->absNodes[w->absHeap[s]].f) s = r;
        if (s == idx) break;
        nav_AbsHeapSwap(w, idx, s);
        idx = s;
    }
}

static void nav_AbsHeapPush(NavWorld* w, int node) {
    if (w->absHeapSize >= w->entranceCap + 2) return;
    int idx = w->absHeapSize++;
    w->absHeap[idx] = node;
    w->absHeapPos[node] = idx;
    nav_AbsHeapUp(w, idx);
}

static int nav_AbsHeapPop(NavWorld* w) {
    if (w->absHeapSize == 0) return -1;
    int result = w->absHeap[0];
    w->absHeapPos[result] = -1;
    w->absHeapSize--;
    if (w->absHeapSize > 0) {
        w->absHeap[0] = w->absHeap[w->absHeapSize];
        w->absHeapPos[w->absHeap[0]] = 0;
        nav_AbsHeapDown(w, 0);
    }
    return result;
}

static void nav_AbsHeapDecreaseKey(NavWorld* w, int node) {
    int idx = w->absHeapPos[node];
    if (idx >= 0 && idx < w->absHeapSize) nav_AbsHeapUp(w, idx);
}

// =============================================================================
// Chunk-local heap over chunkNodes (single z-layer, index = y*width + x)
// =============================================================================

static inline int nav_CellIdx(const NavWorld* w, int x, int y) { return y * w->width + x; }

static void nav_ChunkHeapSwap(NavWorld* w, int i, int j) {
    int a = w->chunkHeap[i], b = w->chunkHeap[j];
    w->chunkHeap[i] = b; w->chunkHeap[j] = a;
    w->chunkHeapPos[a] = j; w->chunkHeapPos[b] = i;
}

static void nav_ChunkHeapUp(NavWorld* w, int idx) {
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (w->chunkNodes[w->chunkHeap[idx]].f < w->chunkNodes[w->chunkHeap[parent]].f) {
            nav_ChunkHeapSwap(w, idx, parent);
            idx = parent;
        } else break;
    }
}

static void nav_ChunkHeapDown(NavWorld* w, int idx) {
    for (;;) {
        int l = 2 * idx + 1, r = l + 1, s = idx;
        if (l < w->chunkHeapSize && w->chunkNodes[w->chunkHeap[l]].f < w->chunkNodes[w->chunkHeap[s]].f) s = l;
        if (r < w->chunkHeapSize && w->chunkNodes[w->chunkHeap[r]].f < w->chunkNodes[w->chunkHeap[s]].f) s = r;
        if (s == idx) break;
        nav_ChunkHeapSwap(w, idx, s);
        idx = s;
    }
}

static void nav_ChunkHeapPush(NavWorld* w, int x, int y) {
    if (w->chunkHeapSize >= w->chunkHeapCap) return;
    int c = nav_CellIdx(w, x, y);
    int idx = w->chunkHeapSize++;
    w->chunkHeap[idx] = c;
    w->chunkHeapPos[c] = idx;
    nav_ChunkHeapUp(w, idx);
}

static bool nav_ChunkHeapPop(NavWorld* w, int* outX, int* outY) {
    if (w->chunkHeapSize == 0) return false;
    int c = w->chunkHeap[0];
    *outX = c % w->width;
    *outY = c / w->width;
    w->chunkHeapPos[c] = -1;
    w->chunkHeapSize--;
    if (w->chunkHeapSize > 0) {
        w->chunkHeap[0] = w->chunkHeap[w->chunkHeapSize];
        w->chunkHeapPos[w->chunkHeap[0]] = 0;
        nav_ChunkHeapDown(w, 0);
    }
    return true;
}

static void nav_ChunkHeapDecreaseKey(NavWorld* w, int x, int y) {
    int idx = w->chunkHeapPos[nav_CellIdx(w, x, y)];
    if (idx >= 0 && idx < w->chunkHeapSize) nav_ChunkHeapUp(w, idx);
}

static void nav_ChunkSearchInit(NavWorld* w, int minX, int minY, int maxX, int maxY) {
    for (int y = minY; y < maxY; y++)
        for (int x = minX; x < maxX; x++) {
            int c = nav_CellIdx(w, x, y);
            w->chunkNodes[c] = (NavChunkNode){NAV_COST_INF, NAV_COST_INF, -1, -1, false, false};
            w->chunkHeapPos[c] = -1;
        }
    w->chunkHeapSize = 0;
}

// Expand (bestX,bestY) inside bounds. mode: 0 = A* to (gx,gy), 1 = Dijkstra.
// reverse: relax the cells that can step ONTO best (g = cost from that cell to the
// search origin), for "cost from each of these cells to here" queries.
static void nav_ChunkExpand(NavWorld* w, int bestX, int bestY, int sz, int gx, int gy,
                        int minX, int minY, int maxX, int maxY, bool dijkstra, bool trackParent,
                        bool reverse) {
    const int* dx = w->eightDir ? nav_kDx8 : nav_kDx4;
    const int* dy = w->eightDir ? nav_kDy8 : nav_kDy4;
    int numDirs = w->eightDir ? 8 : 4;
    NavChunkNode* best = &w->chunkNodes[nav_CellIdx(w, bestX, bestY)];

    for (int i = 0; i < numDirs; i++) {
        int nx = bestX + dx[i], ny = bestY + dy[i];
        if (nx < minX || nx >= maxX || ny < minY || ny >= maxY) continue;
        NavChunkNode* n = &w->chunkNodes[nav_CellIdx(w, nx, ny)];
        if (!NavIsWalkable(NAV_HOSTP, nx, ny, sz) || n->closed) continue;

        // No corner cutting on diagonals
        if (w->eightDir && dx[i] != 0 && dy[i] != 0) {
            if (!NavIsWalkable(NAV_HOSTP, nx, bestY, sz) || !NavIsWalkable(NAV_HOSTP, bestX, ny, sz)) continue;
        }
        // Same movement rules as full A* (was missing: HPA* edges/refinement could
        // enter ramps from the side, which A* and the flow fields forbid)
        if (reverse ? !NavCanEnterFrom(NAV_HOSTP, bestX, bestY, sz, nx, ny)
                    : !NavCanEnterFrom(NAV_HOSTP, nx, ny, sz, bestX, bestY)) continue;

        int baseCost = (dx[i] != 0 && dy[i] != 0) ? 14 : 10;
        int entered = reverse ? NavMoveCost(NAV_HOSTP, bestX, bestY, sz) : NavMoveCost(NAV_HOSTP, nx, ny, sz);
        int ng = best->g + (baseCost * entered) / 10;
        if (ng < n->g) {
            bool wasOpen = n->open;
            n->g = ng;
            n->f = dijkstra ? ng : ng + nav_ChunkHeuristic(w, nx, ny, gx, gy);
            if (trackParent) { n->parentX = bestX; n->parentY = bestY; }
            n->open = true;
            if (wasOpen) nav_ChunkHeapDecreaseKey(w, nx, ny);
            else nav_ChunkHeapPush(w, nx, ny);
        }
    }
}

int NavChunkCost(NavWorld* w, int sx, int sy, int sz, int gx, int gy,
                 int minX, int minY, int maxX, int maxY) {
    nav_ChunkSearchInit(w, minX, minY, maxX, maxY);
    NavChunkNode* s = &w->chunkNodes[nav_CellIdx(w, sx, sy)];
    s->g = 0;
    s->f = nav_ChunkHeuristic(w, sx, sy, gx, gy);
    s->open = true;
    nav_ChunkHeapPush(w, sx, sy);

    int bx, by;
    while (nav_ChunkHeapPop(w, &bx, &by)) {
        if (bx == gx && by == gy) return w->chunkNodes[nav_CellIdx(w, gx, gy)].g;
        NavChunkNode* b = &w->chunkNodes[nav_CellIdx(w, bx, by)];
        b->open = false;
        b->closed = true;
        nav_ChunkExpand(w, bx, by, sz, gx, gy, minX, minY, maxX, maxY, false, false, false);
    }
    return -1;
}

static int nav_ChunkCostsMultiDir(NavWorld* w, int sx, int sy, int sz,
                       const int* targetX, const int* targetY, int* outCosts, int numTargets,
                       int minX, int minY, int maxX, int maxY, bool reverse, bool trackParent) {
    nav_ChunkSearchInit(w, minX, minY, maxX, maxY);
    for (int i = 0; i < numTargets; i++) outCosts[i] = -1;

    NavChunkNode* s = &w->chunkNodes[nav_CellIdx(w, sx, sy)];
    s->g = 0;
    s->f = 0;
    s->open = true;
    nav_ChunkHeapPush(w, sx, sy);

    int found = 0;
    int bx, by;
    while (nav_ChunkHeapPop(w, &bx, &by)) {
        NavChunkNode* b = &w->chunkNodes[nav_CellIdx(w, bx, by)];
        // Check ALL targets: duplicates at the same coords are allowed
        for (int t = 0; t < numTargets; t++) {
            if (bx == targetX[t] && by == targetY[t] && outCosts[t] < 0) {
                outCosts[t] = b->g;
                if (++found == numTargets) return found;
            }
        }
        b->open = false;
        b->closed = true;
        nav_ChunkExpand(w, bx, by, sz, 0, 0, minX, minY, maxX, maxY, true, trackParent, reverse);
    }
    return found;
}

int NavChunkCostsMulti(NavWorld* w, int sx, int sy, int sz,
                       const int* targetX, const int* targetY, int* outCosts, int numTargets,
                       int minX, int minY, int maxX, int maxY) {
    return nav_ChunkCostsMultiDir(w, sx, sy, sz, targetX, targetY, outCosts, numTargets,
                                  minX, minY, maxX, maxY, false, false);
}

// A* within bounds on one level, writes goal-first path. Returns length (0 = none).
static int nav_LocalPathWithBounds(NavWorld* w, int sx, int sy, int sz, int gx, int gy,
                               int minX, int minY, int maxX, int maxY,
                               NavPoint* out, int maxLen) {
    nav_ChunkSearchInit(w, minX, minY, maxX, maxY);
    NavChunkNode* s = &w->chunkNodes[nav_CellIdx(w, sx, sy)];
    s->g = 0;
    s->f = nav_ChunkHeuristic(w, sx, sy, gx, gy);
    s->open = true;
    nav_ChunkHeapPush(w, sx, sy);

    int bx, by;
    while (nav_ChunkHeapPop(w, &bx, &by)) {
        if (bx == gx && by == gy) {
            int len = 0;
            int cx = gx, cy = gy;
            while (cx >= 0 && cy >= 0 && len < maxLen) {
                out[len++] = (NavPoint){cx, cy, sz};
                const NavChunkNode* n = &w->chunkNodes[nav_CellIdx(w, cx, cy)];
                cx = n->parentX;
                cy = n->parentY;
            }
            return len;
        }
        NavChunkNode* b = &w->chunkNodes[nav_CellIdx(w, bx, by)];
        b->open = false;
        b->closed = true;
        nav_ChunkExpand(w, bx, by, sz, gx, gy, minX, minY, maxX, maxY, false, true, false);
    }
    return 0;
}

// Chunk id = z * (chunksX*chunksY) + cy * chunksX + cx
static int nav_GetChunk(const NavWorld* w, int x, int y, int z) {
    int cx = x / w->chunkW;
    int cy = y / w->chunkH;
    if (cx < 0) cx = 0;
    if (cx >= w->chunksX) cx = w->chunksX - 1;
    if (cy < 0) cy = 0;
    if (cy >= w->chunksY) cy = w->chunksY - 1;
    if (z < 0) z = 0;
    if (z >= w->depth) z = w->depth - 1;
    return z * (w->chunksX * w->chunksY) + cy * w->chunksX + cx;
}

static void nav_GetChunkBounds(const NavWorld* w, int chunk, int* minX, int* minY, int* maxX, int* maxY) {
    int perLevel = w->chunksX * w->chunksY;
    int xy = chunk % perLevel;
    int cx = xy % w->chunksX;
    int cy = xy / w->chunksX;
    *minX = cx * w->chunkW;
    *minY = cy * w->chunkH;
    *maxX = (cx + 1) * w->chunkW;
    *maxY = (cy + 1) * w->chunkH;
    if (*maxX > w->width) *maxX = w->width;
    if (*maxY > w->height) *maxY = w->height;
}

// Same-level local path: try the union of both chunks first, then expand by
// one chunk in every direction (paths near chunk borders may need a detour
// through a neighbouring chunk).
static int nav_LocalPath(NavWorld* w, int sx, int sy, int sz, int gx, int gy, int gz, NavPoint* out, int maxLen) {
    if (sz != gz) return 0;

    int a0, a1, a2, a3, b0, b1, b2, b3;
    nav_GetChunkBounds(w, nav_GetChunk(w, sx, sy, sz), &a0, &a1, &a2, &a3);
    nav_GetChunkBounds(w, nav_GetChunk(w, gx, gy, gz), &b0, &b1, &b2, &b3);
    int minX = a0 < b0 ? a0 : b0;
    int minY = a1 < b1 ? a1 : b1;
    int maxX = a2 > b2 ? a2 : b2;
    int maxY = a3 > b3 ? a3 : b3;

    int len = nav_LocalPathWithBounds(w, sx, sy, sz, gx, gy, minX, minY, maxX, maxY, out, maxLen);
    if (len > 0) return len;

    minX -= w->chunkW; minY -= w->chunkH;
    maxX += w->chunkW; maxY += w->chunkH;
    if (minX < 0) minX = 0;
    if (minY < 0) minY = 0;
    if (maxX > w->width) maxX = w->width;
    if (maxY > w->height) maxY = w->height;
    return nav_LocalPathWithBounds(w, sx, sy, sz, gx, gy, minX, minY, maxX, maxY, out, maxLen);
}

// =============================================================================
// Dirty tracking
// =============================================================================

void NavMarkCellChanged(NavWorld* w, int x, int y, int z) {
    int cx = x / w->chunkW;
    int cy = y / w->chunkH;
    if (x < 0 || y < 0 || cx >= w->chunksX || cy >= w->chunksY || z < 0 || z >= w->depth) return;
    // (the chunks above/left whose edge regions include this chunk's first
    // row/column are covered by nav_ComputeAffectedChunks' neighbourhood)
    w->chunkDirtyFlags[nav_ChunkFlagIndex(w, cx, cy, z)] = true;
    int extraZ[4];
    int count = NavAffectedZ(NAV_HOSTP, z, extraZ);
    for (int i = 0; i < count && i < 4; i++)
        if (extraZ[i] >= 0 && extraZ[i] < w->depth)
            w->chunkDirtyFlags[nav_ChunkFlagIndex(w, cx, cy, extraZ[i])] = true;
    w->dirty = true;
    w->hpaDirty = true;
    nav_RegMarkDirty(w, x, y, z);
    NavOnChunkDirty(NAV_HOSTP, x, y, z);
}

// =============================================================================
// HPA*: entrances
// =============================================================================

// Grow every per-entrance array together (entrances, the incremental-update
// copies, adjacency starts, abstract-search scratch). false = out of memory.
#define NAV_GROW(ptr, n) do { void* np_ = realloc((ptr), (size_t)(n) * sizeof(*(ptr))); \
                              if (!np_) { w->growFailed = true; return false; } (ptr) = np_; } while (0)
static bool nav_EnsureEntranceCap(NavWorld* w, int need) {
    if (need <= w->entranceCap) return true;
    int cap = w->entranceCap * 2;
    while (cap < need) cap *= 2;
    NAV_GROW(w->entranceList, cap);
    NAV_GROW(w->entScratch, cap);
    NAV_GROW(w->entMoved, cap);
    NAV_GROW(w->entStamp, cap);
    memset(w->entStamp + w->entranceCap, 0, (size_t)(cap - w->entranceCap) * sizeof(unsigned));
    NAV_GROW(w->adj, cap);
    memset(w->adj + w->entranceCap, 0, (size_t)(cap - w->entranceCap) * sizeof(NavEdgeVec));
    NAV_GROW(w->absNodes, cap + 2);
    NAV_GROW(w->absPath, cap + 2);
    NAV_GROW(w->absHeap, cap + 2);
    NAV_GROW(w->absHeapPos, cap + 2);
    w->entranceCap = cap;
    return true;
}
static bool nav_EnsureLadderCap(NavWorld* w, int need) {
    if (need <= w->ladderCap) return true;
    int cap = w->ladderCap * 2;
    while (cap < need) cap *= 2;
    NAV_GROW(w->ladderList, cap);
    w->ladderCap = cap;
    return true;
}
static bool nav_EnsureRampCap(NavWorld* w, int need) {
    if (need <= w->rampCap) return true;
    int cap = w->rampCap * 2;
    while (cap < need) cap *= 2;
    NAV_GROW(w->rampList, cap);
    w->rampCap = cap;
    return true;
}
#undef NAV_GROW

static void nav_AddEntrance(NavWorld* w, int x, int y, int z, int chunk1, int chunk2) {
    if (nav_EnsureEntranceCap(w, w->entranceNum + 1))
        w->entranceList[w->entranceNum++] = (NavEntrance){x, y, z, chunk1, chunk2};
}

// Split an open border run into entrances of at most NAV_MAX_ENTRANCE_WIDTH
static void nav_AddEntrancesForRun(NavWorld* w, int startX, int startY, int z, int length,
                               bool horizontal, int chunk1, int chunk2) {
    int remaining = length, pos = 0;
    while (remaining > 0) {
        int segLen = (remaining > NAV_MAX_ENTRANCE_WIDTH) ? NAV_MAX_ENTRANCE_WIDTH : remaining;
        int mid = pos + segLen / 2;
        int ex = horizontal ? startX + mid : startX;
        int ey = horizontal ? startY : startY + mid;
        nav_AddEntrance(w, ex, ey, z, chunk1, chunk2);
        pos += segLen;
        remaining -= segLen;
    }
}

// Entrance owned by one chunk (ladder/ramp endpoints). Returns index or -1.
static int nav_AddPointEntrance(NavWorld* w, int x, int y, int z) {
    if (!nav_EnsureEntranceCap(w, w->entranceNum + 1)) return -1;
    int chunk = z * (w->chunksX * w->chunksY) + (y / w->chunkH) * w->chunksX + (x / w->chunkW);
    w->entranceList[w->entranceNum] = (NavEntrance){x, y, z, chunk, chunk};
    return w->entranceNum++;
}

// A cell the host restricts entry to (e.g. a ramp: low or high side only)
static bool nav_CellRestricted(NavWorld* w, int x, int y, int z) {
    static const int ox[4] = {1, -1, 0, 0}, oy[4] = {0, 0, 1, -1};
    for (int d = 0; d < 4; d++)
        if (!NavCanEnterFrom(NAV_HOSTP, x, y, z, x + ox[d], y + oy[d])) return true;
    return false;
}

// Entrances along one chunk border: runs of open cell pairs (b = the cell on the
// second chunk's side, a = b + (ox,oy) on the first's), one entrance per run
// segment. A run assumes its cells reach each other along the border, so a
// restricted cell (ramp) can't be part of one: it gets its own entrance if the
// border can be crossed there, else it breaks the run.
static void nav_BuildBorderRuns(NavWorld* w, int bx0, int by0, int stepX, int stepY, int ox, int oy,
                                int span, int z, int chunk1, int chunk2) {
    int runStart = -1;
    for (int i = 0; i <= span; i++) {
        bool open = false, restricted = false;
        if (i < span) {
            int bx = bx0 + stepX * i, by = by0 + stepY * i, ax = bx + ox, ay = by + oy;
            open = NavIsWalkable(NAV_HOSTP, ax, ay, z) && NavIsWalkable(NAV_HOSTP, bx, by, z)
                && (NavCanEnterFrom(NAV_HOSTP, bx, by, z, ax, ay) || NavCanEnterFrom(NAV_HOSTP, ax, ay, z, bx, by));
            restricted = open && (nav_CellRestricted(w, ax, ay, z) || nav_CellRestricted(w, bx, by, z));
        }
        if ((!open || restricted) && runStart >= 0) {
            nav_AddEntrancesForRun(w, bx0 + stepX * runStart, by0 + stepY * runStart, z, i - runStart, stepX != 0, chunk1, chunk2);
            runStart = -1;
        }
        if (restricted) nav_AddEntrancesForRun(w, bx0 + stepX * i, by0 + stepY * i, z, 1, stepX != 0, chunk1, chunk2);
        else if (open && runStart < 0) runStart = i;
    }
}

// Border entrances for one z-level; `onlyAffected` restricts to borders touching w->affected
static void nav_BuildBorderEntrances(NavWorld* w, int z, bool onlyAffected) {
    int perLevel = w->chunksX * w->chunksY;
    const bool* aff = w->affected;

    // Horizontal borders (between rows of chunks)
    for (int cy = 0; cy < w->chunksY - 1; cy++) {
        for (int cx = 0; cx < w->chunksX; cx++) {
            if (onlyAffected && !aff[nav_ChunkFlagIndex(w, cx, cy, z)] && !aff[nav_ChunkFlagIndex(w, cx, cy + 1, z)]) continue;
            int borderY = (cy + 1) * w->chunkH;
            int startX = cx * w->chunkW;
            int span = w->chunkW;
            if (startX + span > w->width) span = w->width - startX;
            nav_BuildBorderRuns(w, startX, borderY, 1, 0, 0, -1, span, z,
                                z * perLevel + cy * w->chunksX + cx, z * perLevel + (cy + 1) * w->chunksX + cx);
        }
    }
    // Vertical borders (between columns of chunks)
    for (int cy = 0; cy < w->chunksY; cy++) {
        for (int cx = 0; cx < w->chunksX - 1; cx++) {
            if (onlyAffected && !aff[nav_ChunkFlagIndex(w, cx, cy, z)] && !aff[nav_ChunkFlagIndex(w, cx + 1, cy, z)]) continue;
            int borderX = (cx + 1) * w->chunkW;
            int startY = cy * w->chunkH;
            int span = w->chunkH;
            if (startY + span > w->height) span = w->height - startY;
            nav_BuildBorderRuns(w, borderX, startY, 0, 1, -1, 0, span, z,
                                z * perLevel + cy * w->chunksX + cx, z * perLevel + cy * w->chunksX + (cx + 1));
        }
    }
}

void NavBuildEntrances(NavWorld* w) {
    w->entranceNum = 0;
    w->ladderNum = 0;
    w->rampNum = 0;
    w->rampCellNum = 0;

    for (int z = 0; z < w->depth; z++) nav_BuildBorderEntrances(w, z, false);

    // Ladders and ramps between z and z+1
    for (int z = 0; z < w->depth - 1; z++) {
        for (int y = 0; y < w->height; y++) {
            for (int x = 0; x < w->width; x++) {
                if (NavCanClimbUp(NAV_HOSTP, x, y, z) && nav_EnsureLadderCap(w, w->ladderNum + 1)) {
                    int entLow = nav_AddPointEntrance(w, x, y, z);
                    int entHigh = nav_AddPointEntrance(w, x, y, z + 1);
                    if (entLow >= 0 && entHigh >= 0) {
                        w->ladderList[w->ladderNum++] = (NavLadderLink){
                            .x = x, .y = y, .zLow = z, .zHigh = z + 1,
                            .entranceLow = entLow, .entranceHigh = entHigh,
                            .cost = NavMoveCost(NAV_HOSTP, x, y, z + 1)
                        };
                    }
                }

                int rdx, rdy;
                if (NavRampHighSide(NAV_HOSTP, x, y, z, &rdx, &rdy)) {
                    w->rampCellNum++;
                    int exitX, exitY;
                    if (NavRampUp(NAV_HOSTP, x, y, z, &exitX, &exitY) && nav_EnsureRampCap(w, w->rampNum + 1)) {
                        int entRamp = nav_AddPointEntrance(w, x, y, z);
                        int entExit = nav_AddPointEntrance(w, exitX, exitY, z + 1);
                        if (entRamp >= 0 && entExit >= 0) {
                            w->rampList[w->rampNum++] = (NavRampLink){
                                .rampX = x, .rampY = y, .rampZ = z,
                                .exitX = exitX, .exitY = exitY,
                                .entranceRamp = entRamp, .entranceExit = entExit,
                                .cost = (14 * NavMoveCost(NAV_HOSTP, exitX, exitY, z + 1)) / 10
                            };
                        }
                    }
                }
            }
        }
    }
    // Top level ramps can't lead anywhere, but count them
    {
        int z = w->depth - 1;
        for (int y = 0; y < w->height; y++)
            for (int x = 0; x < w->width; x++) {
                int rdx, rdy;
                if (NavRampHighSide(NAV_HOSTP, x, y, z, &rdx, &rdy)) w->rampCellNum++;
            }
    }

    nav_ClearAllChunkDirty(w);
}

// =============================================================================
// HPA*: graph edges
// =============================================================================

// Edges are directed: ramps can be entered only from their low or high side, so a
// route inside a chunk may exist one way only, and costs differ by direction (each
// step pays for the cell it enters). Each entrance keeps its out-edges sorted by
// target; an entrance pair connected in several chunk regions keeps the cheapest.
// No per-entrance cap: dropping edges silently would make routes vanish.
static void nav_AddEdge(NavWorld* w, int from, int to, int cost, int chunk) {
    NavEdgeVec* v = &w->adj[from];
    int lo = 0, hi = v->num;                 // first edge with target >= to
    while (lo < hi) { int mid = (lo + hi) / 2; if (v->e[mid].to < to) lo = mid + 1; else hi = mid; }
    if (lo < v->num && v->e[lo].to == to) {  // pair seen in another chunk region: keep the cheapest
        NavGraphEdge* x = &v->e[lo];
        if (cost < x->cost || (cost == x->cost && chunk < x->chunk)) { x->cost = cost; x->chunk = chunk; }
        return;
    }
    if (v->num == v->cap) {
        int cap = v->cap ? v->cap * 2 : 8;
        NavGraphEdge* ne = realloc(v->e, (size_t)cap * sizeof(NavGraphEdge));
        if (!ne) { w->edgeOverflow = true; return; }
        v->e = ne;
        v->cap = cap;
    }
    memmove(&v->e[lo + 1], &v->e[lo], (size_t)(v->num - lo) * sizeof(NavGraphEdge));
    v->e[lo] = (NavGraphEdge){from, to, cost, chunk};
    v->num++;
    w->edgeNum++;
}

// Empty every entrance's edge list (full build). Lists past entranceNum are freed:
// they must be empty, see the incremental update.
static void nav_ClearEdges(NavWorld* w) {
    for (int e = 0; e < w->entranceCap; e++) {
        if (e < w->entranceNum) { w->adj[e].num = 0; continue; }
        if (w->adj[e].e) { free(w->adj[e].e); w->adj[e] = (NavEdgeVec){0}; }
    }
    w->edgeNum = 0;
}

// Extra cost of a ladder/ramp link edge right now (NAV_CLIMB_EXTRA at its start)
static inline int nav_LinkExtra(NavWorld* w, const NavGraphEdge* e) {
    if (e->chunk >= 0) return 0;
    const NavEntrance* F = &w->entranceList[e->from];
    const NavEntrance* T = &w->entranceList[e->to];
    (void)F; (void)T;
    return NAV_CLIMB_EXTRA(NAV_HOSTP, F->x, F->y, F->z, T->z > F->z ? 1 : -1);
}

static inline bool nav_EntranceTouchesAffected(const NavWorld* w, int idx);

// Ladder and ramp links, both directions, each paying for the cell it enters
// (same step rules as A*). onlyAffected (incremental update): just the links with an
// end entrance in an affected chunk; the others kept their edges.
static void nav_AddVerticalLinkEdges(NavWorld* w, bool onlyAffected) {
    for (int i = 0; i < w->ladderNum; i++) {
        const NavLadderLink* l = &w->ladderList[i];
        if (onlyAffected && !nav_EntranceTouchesAffected(w, l->entranceLow) && !nav_EntranceTouchesAffected(w, l->entranceHigh)) continue;
        nav_AddEdge(w, l->entranceLow, l->entranceHigh, NavMoveCost(NAV_HOSTP, l->x, l->y, l->zHigh), -1);
        nav_AddEdge(w, l->entranceHigh, l->entranceLow, NavMoveCost(NAV_HOSTP, l->x, l->y, l->zLow), -1);
    }
    for (int i = 0; i < w->rampNum; i++) {
        const NavRampLink* r = &w->rampList[i];
        if (onlyAffected && !nav_EntranceTouchesAffected(w, r->entranceRamp) && !nav_EntranceTouchesAffected(w, r->entranceExit)) continue;
        nav_AddEdge(w, r->entranceRamp, r->entranceExit,
                    (14 * NavMoveCost(NAV_HOSTP, r->exitX, r->exitY, r->rampZ + 1)) / 10, -1);
        nav_AddEdge(w, r->entranceExit, r->entranceRamp,
                    (14 * NavMoveCost(NAV_HOSTP, r->rampX, r->rampY, r->rampZ)) / 10, -1);
    }
}

static void nav_WarnLimits(const NavWorld* w) {
    if (w->growFailed)
        NAV_LOG_WARN("HPA* LIMIT HIT: out of memory growing entrance/link lists - pathfinding will fail!");
    if (w->edgeOverflow)
        NAV_LOG_WARN("HPA* LIMIT HIT: out of memory for edges (%d) - pathfinding will fail!", w->edgeNum);
    if (w->chunkEntsOverflow)
        NAV_LOG_WARN("HPA* LIMIT HIT: a chunk region has more than %d entrances - some edges are missing!", NAV_MAX_ENTRANCES_PER_CHUNK);
}

// Fixed order of entrances by position/chunks (not by index, so incremental and
// full builds make the same choices below)
static bool nav_EntBefore(const NavWorld* w, int a, int b) {
    const NavEntrance* A = &w->entranceList[a];
    const NavEntrance* B = &w->entranceList[b];
    if (A->y != B->y) return A->y < B->y;
    if (A->x != B->x) return A->x < B->x;
    if (A->chunk1 != B->chunk1) return A->chunk1 < B->chunk1;
    if (A->chunk2 != B->chunk2) return A->chunk2 < B->chunk2;
    return a < b;   // same cell, same chunks (equal costs either way)
}

// The search path to (tx,ty) can be walked backwards: every cell on it can be
// entered from the next one (only ramps refuse). Then the reverse edge exists.
static bool nav_PathReversible(NavWorld* w, int tx, int ty, int z) {
    int cx = tx, cy = ty;
    for (;;) {
        const NavChunkNode* n = &w->chunkNodes[nav_CellIdx(w, cx, cy)];
        if (n->parentX < 0) return true;
        if (!NavCanEnterFrom(NAV_HOSTP, n->parentX, n->parentY, z, cx, cy)) return false;
        cx = n->parentX; cy = n->parentY;
    }
}

// Directed edges between the entrances of one chunk region. needed[i] marks
// entrances whose pairs must be (re)computed (NULL = all). Pairs are visited
// from the entrance that comes first in nav_EntBefore order: if its search path
// to the other can be walked backwards, the reverse edge gets the mirrored cost
// (HPA*'s usual approximation: exact costs differ slightly by direction);
// otherwise (a ramp in the way, or no path) the other entrance searches too.
static void nav_ChunkEdges(NavWorld* w, int chunk, int z, int minX, int minY, int maxX, int maxY,
                           const int* ents, const bool* needed, int n) {
    int order[NAV_CONNECT_MAX];
    int m = 0;
    for (int i = 0; i < n; i++) {
        if (w->entranceList[ents[i]].z != z) continue;
        int k = m++;
        while (k > 0 && nav_EntBefore(w, ents[i], ents[order[k - 1]])) { order[k] = order[k - 1]; k--; }
        order[k] = i;
    }
    bool mirrored[NAV_CONNECT_MAX][NAV_CONNECT_MAX];          // [earlier][later] in order (16 KB)
    int tx[NAV_CONNECT_MAX], ty[NAV_CONNECT_MAX], tPos[NAV_CONNECT_MAX], costs[NAV_CONNECT_MAX];
    for (int a = 0; a < m; a++)
        for (int b = 0; b < m; b++) mirrored[a][b] = false;
    for (int a = 0; a < m; a++) {
        int s = ents[order[a]], nt = 0;
        for (int b = 0; b < m; b++) {
            if (b == a) continue;
            if (needed && !needed[order[a]] && !needed[order[b]]) continue;
            if (b < a && mirrored[b][a]) continue;     // already have s -> t from t's search
            int t = ents[order[b]];
            tx[nt] = w->entranceList[t].x; ty[nt] = w->entranceList[t].y; tPos[nt++] = b;
        }
        if (nt == 0) continue;
        nav_ChunkCostsMultiDir(w, w->entranceList[s].x, w->entranceList[s].y, z, tx, ty, costs, nt,
                               minX, minY, maxX, maxY, false, true);
        for (int k = 0; k < nt; k++) {
            if (costs[k] < 0) continue;
            int b = tPos[k], t = ents[order[b]];
            nav_AddEdge(w, s, t, costs[k], chunk);
            if (b > a && nav_PathReversible(w, tx[k], ty[k], z)) {
                nav_AddEdge(w, t, s, costs[k], chunk);
                mirrored[a][b] = true;
            }
        }
    }
}

void NavBuildGraph(NavWorld* w) {
    nav_ClearEdges(w);
    w->edgeOverflow = false;
    // Entrances per chunk edge region (also used by the hierarchical fields). It lists
    // exactly the entrances a chunk's search region holds, in index order; scanning
    // every entrance per chunk instead made the full build ~O(chunks x entrances).
    nav_BuildChunkEntranceIndex(w);

    double startTime = NavHostTime();
    int perLevel = w->chunksX * w->chunksY;
    int totalChunks = w->depth * perLevel;

    for (int chunk = 0; chunk < totalChunks; chunk++) {
        int z = chunk / perLevel;
        int xy = chunk % perLevel;
        int cx = xy % w->chunksX;
        int cy = xy / w->chunksX;

        int minX = cx * w->chunkW;
        int minY = cy * w->chunkH;
        int maxX = (cx + 1) * w->chunkW + 1;
        int maxY = (cy + 1) * w->chunkH + 1;
        if (maxX > w->width) maxX = w->width;
        if (maxY > w->height) maxY = w->height;

        if (w->chunkEntCount[chunk] == 0) continue;
        nav_ChunkEdges(w, chunk, z, minX, minY, maxX, maxY, w->chunkEnts[chunk], NULL, w->chunkEntCount[chunk]);
    }

    nav_AddVerticalLinkEdges(w, false);
    w->graphVersion++;
    w->chunkEntsVersion = w->chunkEntsOverflow ? w->graphVersion - 1 : w->graphVersion;

    NAV_LOG_INFO("Built graph: %d edges (%d ladder links, %d ramp links) in %.2fms",
                 w->edgeNum, w->ladderNum, w->rampNum, (NavHostTime() - startTime) * 1000.0);
    (void)startTime;   // unused when the host compiles NAV_LOG_INFO out
    nav_WarnLimits(w);
}

// =============================================================================
// HPA*: incremental update
// =============================================================================

static void nav_ComputeAffectedChunks(NavWorld* w) {
    bool* aff = w->affected;
    memset(aff, 0, (size_t)w->depth * w->chunksX * w->chunksY * sizeof(bool));
    for (int z = 0; z < w->depth; z++)
        for (int cy = 0; cy < w->chunksY; cy++)
            for (int cx = 0; cx < w->chunksX; cx++) {
                if (!w->chunkDirtyFlags[nav_ChunkFlagIndex(w, cx, cy, z)]) continue;
                aff[nav_ChunkFlagIndex(w, cx, cy, z)] = true;
                if (cy > 0)               aff[nav_ChunkFlagIndex(w, cx, cy - 1, z)] = true;
                if (cy < w->chunksY - 1)  aff[nav_ChunkFlagIndex(w, cx, cy + 1, z)] = true;
                if (cx > 0)               aff[nav_ChunkFlagIndex(w, cx - 1, cy, z)] = true;
                if (cx < w->chunksX - 1)  aff[nav_ChunkFlagIndex(w, cx + 1, cy, z)] = true;
                if (z > 0)                aff[nav_ChunkFlagIndex(w, cx, cy, z - 1)] = true;
                if (z < w->depth - 1)     aff[nav_ChunkFlagIndex(w, cx, cy, z + 1)] = true;
                // up-left: its edge region includes this chunk's corner cell
                // (see nav_EdgeChunksOfCell; left and up are covered above)
                if (cx > 0 && cy > 0)     aff[nav_ChunkFlagIndex(w, cx - 1, cy - 1, z)] = true;
            }
}

// Chunk ids and the affected/dirty flag index are the same layout
// ((z * chunksY + cy) * chunksX + cx), so this is a plain lookup. It runs a few
// times per edge on every incremental update (millions on big maps).
static inline bool nav_ChunkIdAffected(const NavWorld* w, int chunk) {
    return w->affected[chunk];
}

static inline bool nav_EntranceTouchesAffected(const NavWorld* w, int idx) {
    return nav_ChunkIdAffected(w, w->entranceList[idx].chunk1) || nav_ChunkIdAffected(w, w->entranceList[idx].chunk2);
}

// ---- Incremental update: entrance ids stay put ----
// An update removes the entrances touching affected chunks and fills their slots
// with entrances from the end of the list: only those few change id, so nothing
// proportional to the whole graph is touched. Edges into an entrance can only come
// from entrances in the same chunk regions (edges are computed per region; the
// chunk index lists them) or from its ladder/ramp partner, which it links back to.

// Remove the edge f -> t (at most one: a pair keeps one edge)
static void nav_StripEdgeTo(NavWorld* w, int f, int t) {
    NavEdgeVec* v = &w->adj[f];
    int lo = 0, hi = v->num;
    while (lo < hi) { int mid = (lo + hi) / 2; if (v->e[mid].to < t) lo = mid + 1; else hi = mid; }
    if (lo < v->num && v->e[lo].to == t) {
        memmove(&v->e[lo], &v->e[lo + 1], (size_t)(v->num - lo - 1) * sizeof(NavGraphEdge));
        v->num--;
        w->edgeNum--;
    }
}

// Remove f's edges computed in chunk region `chunk`
static void nav_StripEdgesTagged(NavWorld* w, int f, int chunk) {
    NavEdgeVec* v = &w->adj[f];
    int k = 0;
    for (int j = 0; j < v->num; j++)
        if (v->e[j].chunk != chunk) v->e[k++] = v->e[j];
    w->edgeNum -= v->num - k;
    v->num = k;
}

// Every edge into and out of entrance e goes
static void nav_IsolateEntrance(NavWorld* w, int e) {
    const NavEntrance* E = &w->entranceList[e];
    int cs[4];
    int nc = nav_EdgeChunksOfCell(w, E->x, E->y, E->z, cs);
    for (int i = 0; i < nc; i++)
        for (int k = 0; k < w->chunkEntCount[cs[i]]; k++) {
            int f = w->chunkEnts[cs[i]][k];
            if (f != e) nav_StripEdgeTo(w, f, e);
        }
    NavEdgeVec* v = &w->adj[e];
    for (int j = 0; j < v->num; j++)
        if (v->e[j].chunk < 0) nav_StripEdgeTo(w, v->e[j].to, e);   // ladder/ramp partner
    w->edgeNum -= v->num;
    v->num = 0;
}

// The edge f -> from now points at `to` (keeps the list sorted by target)
static void nav_RetargetEdge(NavWorld* w, int f, int from, int to) {
    NavEdgeVec* v = &w->adj[f];
    int j = 0;
    while (j < v->num && v->e[j].to != from) j++;
    if (j == v->num) return;
    NavGraphEdge ed = v->e[j];
    ed.to = to;
    while (j > 0 && v->e[j - 1].to > to) { v->e[j] = v->e[j - 1]; j--; }
    while (j + 1 < v->num && v->e[j + 1].to < to) { v->e[j] = v->e[j + 1]; j++; }
    v->e[j] = ed;
}

// Chunk index upkeep (lists of the chunk edge regions holding each entrance)
static void nav_IndexAdd(NavWorld* w, int e) {
    const NavEntrance* E = &w->entranceList[e];
    int cs[4];
    int n = nav_EdgeChunksOfCell(w, E->x, E->y, E->z, cs);
    for (int i = 0; i < n; i++) {
        if (w->chunkEntCount[cs[i]] < NAV_MAX_ENTRANCES_PER_CHUNK) w->chunkEnts[cs[i]][w->chunkEntCount[cs[i]]++] = e;
        else w->chunkEntsOverflow = true;
    }
}
static void nav_IndexReplace(NavWorld* w, int e, int with) {   // with = -1: remove
    const NavEntrance* E = &w->entranceList[with >= 0 ? with : e];
    int cs[4];
    int n = nav_EdgeChunksOfCell(w, E->x, E->y, E->z, cs);
    for (int i = 0; i < n; i++) {
        int* list = w->chunkEnts[cs[i]];
        for (int k = 0; k < w->chunkEntCount[cs[i]]; k++) {
            if (list[k] != e) continue;
            if (with >= 0) list[k] = with;
            else list[k] = list[--w->chunkEntCount[cs[i]]];
            break;
        }
    }
}

// Entrance m takes the (emptied) slot h: its edges, the edges into it, its index entries
static void nav_MoveEntrance(NavWorld* w, int m, int h) {
    w->entranceList[h] = w->entranceList[m];
    NavEdgeVec v = w->adj[m];
    w->adj[m] = w->adj[h];               // h's list is empty (isolated): a spare for slot m
    w->adj[h] = v;
    for (int j = 0; j < v.num; j++) w->adj[h].e[j].from = h;
    const NavEntrance* E = &w->entranceList[h];
    int cs[4];
    int nc = nav_EdgeChunksOfCell(w, E->x, E->y, E->z, cs);
    for (int i = 0; i < nc; i++)
        for (int k = 0; k < w->chunkEntCount[cs[i]]; k++) {
            int f = w->chunkEnts[cs[i]][k];
            if (f != m) nav_RetargetEdge(w, f, m, h);
        }
    for (int j = 0; j < w->adj[h].num; j++)
        if (w->adj[h].e[j].chunk < 0) nav_RetargetEdge(w, w->adj[h].e[j].to, m, h);   // ladder/ramp partner
    nav_IndexReplace(w, m, h);
}

// Point entrance for one end of a ladder/ramp link during an incremental update:
// new if its chunk's entrances were rebuilt, else an existing one at that cell that
// no kept link uses (left by an old link). A new link ending in an unaffected chunk
// has none yet: create it, and mark the chunk affected so its edges get computed.
static int nav_LinkEnd(NavWorld* w, const bool* rebuilt, int flag, int x, int y, int z) {
    if (rebuilt[flag]) return nav_AddPointEntrance(w, x, y, z);
    int c = nav_GetChunk(w, x, y, z), best = -1;
    for (int k = 0; k < w->chunkEntCount[c]; k++) {
        int e = w->chunkEnts[c][k];
        const NavEntrance* E = &w->entranceList[e];
        if (E->x == x && E->y == y && E->z == z && E->chunk1 == c && E->chunk2 == c
            && w->entStamp[e] != w->stampNow && (best < 0 || e < best)) best = e;
    }
    if (best >= 0) { w->entStamp[best] = w->stampNow; return best; }
    w->affected[flag] = true;
    return nav_AddPointEntrance(w, x, y, z);
}

static void nav_RebuildAffectedEntrances(NavWorld* w) {
    bool* aff = w->affected;

    // Ladder/ramp links keep the point entrance of an end whose chunk is not
    // affected (found again by position below). A link that no longer exists
    // would leave that end's entrance behind, so its other end is rebuilt too.
    for (bool grew = true; grew;) {
        grew = false;
        for (int i = 0; i < w->ladderNum; i++) {
            const NavLadderLink* l = &w->ladderList[i];
            int a = nav_ChunkFlagIndex(w, l->x / w->chunkW, l->y / w->chunkH, l->zLow);
            int b = nav_ChunkFlagIndex(w, l->x / w->chunkW, l->y / w->chunkH, l->zHigh);
            if (aff[a] == aff[b] || NavCanClimbUp(NAV_HOSTP, l->x, l->y, l->zLow)) continue;
            aff[a] = aff[b] = true; grew = true;
        }
        for (int i = 0; i < w->rampNum; i++) {
            const NavRampLink* r = &w->rampList[i];
            int a = nav_ChunkFlagIndex(w, r->rampX / w->chunkW, r->rampY / w->chunkH, r->rampZ);
            int b = nav_ChunkFlagIndex(w, r->exitX / w->chunkW, r->exitY / w->chunkH, r->rampZ + 1);
            int ex, ey;
            if (aff[a] == aff[b]) continue;
            if (NavRampUp(NAV_HOSTP, r->rampX, r->rampY, r->rampZ, &ex, &ey) && ex == r->exitX && ey == r->exitY) continue;
            aff[a] = aff[b] = true; grew = true;
        }
    }

    // Remove the entrances touching affected chunks: cut their edges, take them out
    // of the chunk index, then fill their slots from the end of the list
    int oldNum = w->entranceNum, removed = 0;
    int* gone = w->entScratch;              // ascending
    for (int i = 0; i < oldNum; i++)
        if (nav_EntranceTouchesAffected(w, i)) gone[removed++] = i;
    for (int r = 0; r < removed; r++) nav_IsolateEntrance(w, gone[r]);
    for (int r = 0; r < removed; r++) nav_IndexReplace(w, gone[r], -1);
    int newNum = oldNum - removed;
    int* moved = w->entMoved;               // [id - newNum] for ids past the new end: new id, -1 = removed
    for (int i = 0; i < removed; i++) moved[i] = -1;
    w->stampNow++;
    for (int r = 0; r < removed; r++) w->entStamp[gone[r]] = w->stampNow;   // stamp = removed (for now)
    int end = oldNum;
    for (int r = 0; r < removed && gone[r] < newNum; r++) {
        do { end--; } while (w->entStamp[end] == w->stampNow);
        nav_MoveEntrance(w, end, gone[r]);
        moved[end - newNum] = gone[r];
    }
    w->entranceNum = newNum;
    w->updNewNum = newNum;
    w->stampNow++;                          // stamp = claimed by a link from here on

    // Chunks whose entrances were dropped (link ends there get new entrances).
    // aff[] may still grow below, but only to recompute edges.
    int totalChunks = w->depth * w->chunksX * w->chunksY;
    bool* rebuilt = w->rebuiltChunks;
    memcpy(rebuilt, aff, (size_t)totalChunks * sizeof(bool));

    // New border entrances for affected chunks
    for (int z = 0; z < w->depth; z++) nav_BuildBorderEntrances(w, z, true);

    // Ladder and ramp links. Links with both ends in chunks whose entrances were
    // kept stay as they were (their entrances found again by position + chunk);
    // only the rebuilt chunks are scanned for links. Each link is found once: from
    // its low end, or from its high end when the low end's chunk wasn't rebuilt.
    // (Scanning the whole world here cost ~100 ms per update on 512x512x16 hills.)
    int perLevel = w->chunksX * w->chunksY;
    int oldLadderNum = w->ladderNum, oldRampNum = w->rampNum;
    NavLadderLink* oldLadders = oldLadderNum ? malloc((size_t)oldLadderNum * sizeof(NavLadderLink)) : NULL;
    NavRampLink* oldRamps = oldRampNum ? malloc((size_t)oldRampNum * sizeof(NavRampLink)) : NULL;
    if ((oldLadderNum && !oldLadders) || (oldRampNum && !oldRamps)) { w->growFailed = true; free(oldLadders); free(oldRamps); return; }
    if (oldLadders) memcpy(oldLadders, w->ladderList, (size_t)oldLadderNum * sizeof(NavLadderLink));
    if (oldRamps) memcpy(oldRamps, w->rampList, (size_t)oldRampNum * sizeof(NavRampLink));
    w->ladderNum = 0;
    w->rampNum = 0;
    #define NAV_REBUILT(x, y, z) (rebuilt[nav_ChunkFlagIndex(w, (x) / w->chunkW, (y) / w->chunkH, (z))])
    #define NAV_NEWID(e) ((e) < newNum ? (e) : moved[(e) - newNum])

    for (int k = 0; k < oldLadderNum; k++) {
        const NavLadderLink* l = &oldLadders[k];
        if (NAV_REBUILT(l->x, l->y, l->zLow) || NAV_REBUILT(l->x, l->y, l->zHigh)) continue;
        if (!nav_EnsureLadderCap(w, w->ladderNum + 1)) break;
        int entLow = NAV_NEWID(l->entranceLow), entHigh = NAV_NEWID(l->entranceHigh);
        if (entLow < 0 || entHigh < 0) continue;
        w->entStamp[entLow] = w->entStamp[entHigh] = w->stampNow;
        NavLadderLink kept = *l;
        kept.entranceLow = entLow;
        kept.entranceHigh = entHigh;
        w->ladderList[w->ladderNum++] = kept;
    }
    for (int k = 0; k < oldRampNum; k++) {
        const NavRampLink* r = &oldRamps[k];
        if (NAV_REBUILT(r->rampX, r->rampY, r->rampZ) || NAV_REBUILT(r->exitX, r->exitY, r->rampZ + 1)) continue;
        if (!nav_EnsureRampCap(w, w->rampNum + 1)) break;
        int entRamp = NAV_NEWID(r->entranceRamp), entExit = NAV_NEWID(r->entranceExit);
        if (entRamp < 0 || entExit < 0) continue;
        w->entStamp[entRamp] = w->entStamp[entExit] = w->stampNow;
        NavRampLink kept = *r;
        kept.entranceRamp = entRamp;
        kept.entranceExit = entExit;
        w->rampList[w->rampNum++] = kept;
    }

    for (int chunk = 0; chunk < w->depth * perLevel; chunk++) {
        int z = chunk / perLevel, xy = chunk % perLevel;
        int cx = xy % w->chunksX, cy = xy / w->chunksX;
        if (!rebuilt[nav_ChunkFlagIndex(w, cx, cy, z)]) continue;
        int x0 = cx * w->chunkW, y0 = cy * w->chunkH;
        int x1 = x0 + w->chunkW < w->width ? x0 + w->chunkW : w->width;
        int y1 = y0 + w->chunkH < w->height ? y0 + w->chunkH : w->height;
        for (int y = y0; y < y1; y++)
            for (int x = x0; x < x1; x++) {
                // ladder from here up / onto here from below (low end not rebuilt)
                for (int zl = z - 1; zl <= z; zl++) {
                    if (zl < 0 || zl + 1 >= w->depth) continue;
                    if (zl == z - 1 && NAV_REBUILT(x, y, zl)) continue;   // found from its low end
                    if (!NavCanClimbUp(NAV_HOSTP, x, y, zl)) continue;
                    if (!nav_EnsureLadderCap(w, w->ladderNum + 1)) continue;
                    int entLow = nav_LinkEnd(w, rebuilt, nav_ChunkFlagIndex(w, x / w->chunkW, y / w->chunkH, zl), x, y, zl);
                    int entHigh = nav_LinkEnd(w, rebuilt, nav_ChunkFlagIndex(w, x / w->chunkW, y / w->chunkH, zl + 1), x, y, zl + 1);
                    if (entLow >= 0 && entHigh >= 0)
                        w->ladderList[w->ladderNum++] = (NavLadderLink){
                            .x = x, .y = y, .zLow = zl, .zHigh = zl + 1,
                            .entranceLow = entLow, .entranceHigh = entHigh,
                            .cost = NavMoveCost(NAV_HOSTP, x, y, zl + 1)
                        };
                }
                // ramp here (low end), or a ramp below whose exit is here (ramp chunk not rebuilt)
                static const int ox[5] = {0, 0, 1, 0, -1}, oy[5] = {0, -1, 0, 1, 0};
                for (int d = 0; d < 5; d++) {
                    int rx = x - ox[d], ry = y - oy[d], rz = d == 0 ? z : z - 1;
                    if (rz < 0 || rz + 1 >= w->depth || rx < 0 || ry < 0 || rx >= w->width || ry >= w->height) continue;
                    if (d > 0 && NAV_REBUILT(rx, ry, rz)) continue;           // found from its low end
                    int ex, ey;
                    if (!NavRampUp(NAV_HOSTP, rx, ry, rz, &ex, &ey)) continue;
                    if (d > 0 && (ex != x || ey != y)) continue;
                    if (!nav_EnsureRampCap(w, w->rampNum + 1)) continue;
                    int entRamp = nav_LinkEnd(w, rebuilt, nav_ChunkFlagIndex(w, rx / w->chunkW, ry / w->chunkH, rz), rx, ry, rz);
                    int entExit = nav_LinkEnd(w, rebuilt, nav_ChunkFlagIndex(w, ex / w->chunkW, ey / w->chunkH, rz + 1), ex, ey, rz + 1);
                    if (entRamp >= 0 && entExit >= 0)
                        w->rampList[w->rampNum++] = (NavRampLink){
                            .rampX = rx, .rampY = ry, .rampZ = rz, .exitX = ex, .exitY = ey,
                            .entranceRamp = entRamp, .entranceExit = entExit,
                            .cost = (14 * NavMoveCost(NAV_HOSTP, ex, ey, rz + 1)) / 10
                        };
                }
            }
    }
    #undef NAV_REBUILT
    #undef NAV_NEWID
    free(oldLadders);
    free(oldRamps);
}

static void nav_RebuildAffectedEdges(NavWorld* w) {
    int newNum = w->updNewNum;              // entrances from here on are new: no edges yet
    w->edgeOverflow = false;
    for (int e = newNum; e < w->entranceNum; e++) { w->adj[e].num = 0; nav_IndexAdd(w, e); }

    int totalChunks = w->depth * w->chunksX * w->chunksY;
    const bool* aff = w->affected;
    const bool* rebuilt = w->rebuiltChunks;
    // 1. Kept entrances touching a chunk that only became affected while placing
    //    link ends lose all their edges (recomputed below, like the new ones')
    for (int c = 0; c < totalChunks; c++) {
        if (!aff[c] || rebuilt[c]) continue;
        for (int k = 0; k < w->chunkEntCount[c]; k++) {
            int f = w->chunkEnts[c][k];
            const NavEntrance* F = &w->entranceList[f];
            if (f < newNum && (F->chunk1 == c || F->chunk2 == c)) nav_IsolateEntrance(w, f);
        }
    }
    // 2. Edges computed in affected chunk regions go (recomputed below)
    for (int c = 0; c < totalChunks; c++) {
        if (!aff[c]) continue;
        for (int k = 0; k < w->chunkEntCount[c]; k++) nav_StripEdgesTagged(w, w->chunkEnts[c][k], c);
    }

    // 3. One multi-target Dijkstra per affected entrance
    int perLevel = w->chunksX * w->chunksY;
    for (int z = 0; z < w->depth; z++)
        for (int cy = 0; cy < w->chunksY; cy++)
            for (int cx = 0; cx < w->chunksX; cx++) {
                int chunk = z * perLevel + cy * w->chunksX + cx;
                int numEnts = w->chunkEntCount[chunk];
                if (numEnts == 0) continue;

                bool entAffected[NAV_MAX_ENTRANCES_PER_CHUNK];
                bool chunkAffected = nav_ChunkIdAffected(w, chunk);   // all its edges were dropped
                bool any = chunkAffected;
                for (int i = 0; i < numEnts; i++) {
                    entAffected[i] = nav_EntranceTouchesAffected(w, w->chunkEnts[chunk][i]);
                    any |= entAffected[i];
                }
                if (!any) continue;

                int minX = cx * w->chunkW;
                int minY = cy * w->chunkH;
                int maxX = (cx + 1) * w->chunkW + 1;
                int maxY = (cy + 1) * w->chunkH + 1;
                if (maxX > w->width) maxX = w->width;
                if (maxY > w->height) maxY = w->height;

                // Pairs with an affected end (edges are directed, so unaffected
                // entrances need their edges INTO affected ones too)
                bool needed[NAV_MAX_ENTRANCES_PER_CHUNK];
                for (int i = 0; i < numEnts; i++) needed[i] = chunkAffected || entAffected[i];
                nav_ChunkEdges(w, chunk, z, minX, minY, maxX, maxY, w->chunkEnts[chunk], needed, numEnts);
            }

    nav_AddVerticalLinkEdges(w, true);
    w->graphVersion++;
    // index kept up to date above
    w->chunkEntsVersion = w->chunkEntsOverflow ? w->graphVersion - 1 : w->graphVersion;
}

const NavGraphEdge* NavOutEdges(const NavWorld* w, int e, int* n) {
    *n = w->adj[e].num;
    return w->adj[e].e;
}

const NavGraphEdge* NavEdgesFlat(NavWorld* w, int* n) {
    if (w->flatVersion != w->graphVersion || !w->flatEdges) {
        if (w->flatCap < w->edgeNum || !w->flatEdges) {
            int cap = w->edgeNum > 0 ? w->edgeNum : 1;
            NavGraphEdge* f = realloc(w->flatEdges, (size_t)cap * sizeof(NavGraphEdge));
            if (!f) { *n = 0; return w->flatEdges; }
            w->flatEdges = f;
            w->flatCap = cap;
        }
        int k = 0;
        for (int e = 0; e < w->entranceNum; e++) {
            if (w->adj[e].num) memcpy(w->flatEdges + k, w->adj[e].e, (size_t)w->adj[e].num * sizeof(NavGraphEdge));
            k += w->adj[e].num;
        }
        w->flatVersion = w->graphVersion;
    }
    *n = w->edgeNum;
    return w->flatEdges;
}

void NavUpdateDirty(NavWorld* w) {
    bool anyDirty = false;
    int total = w->depth * w->chunksX * w->chunksY;
    for (int i = 0; i < total && !anyDirty; i++) anyDirty = w->chunkDirtyFlags[i];
    if (!anyDirty) return;

    // The update finds edges through the chunk index; if that is incomplete (a chunk
    // region over NAV_MAX_ENTRANCES_PER_CHUNK, warned), rebuild everything instead
    if (w->chunkEntsVersion != w->graphVersion) {
        NavBuildEntrances(w);
        NavBuildGraph(w);
        return;
    }

    nav_ComputeAffectedChunks(w);
    nav_RebuildAffectedEntrances(w);
    nav_RebuildAffectedEdges(w);

    nav_ClearAllChunkDirty(w);
    nav_WarnLimits(w);
}

// =============================================================================
// Full-grid A* (indexed binary heap + generation stamps)
// =============================================================================

static inline int nav_CellId3(const NavWorld* w, int x, int y, int z) {
    return (z * w->height + y) * w->width + x;
}

static inline void nav_ATouch(NavWorld* w, int id) {
    if (w->aGen[id] != w->aCurGen) {
        w->aGen[id] = w->aCurGen;
        w->aG[id] = NAV_COST_INF;
        w->aF[id] = NAV_COST_INF;
        w->aParent[id] = -1;
        w->aState[id] = 0;
        w->aHeapIdx[id] = -1;
    }
}

static void nav_AUp(NavWorld* w, int i) {
    while (i > 0) {
        int p = (i - 1) / 2;
        if (w->aF[w->aHeap[p]] <= w->aF[w->aHeap[i]]) break;
        int t = w->aHeap[p]; w->aHeap[p] = w->aHeap[i]; w->aHeap[i] = t;
        w->aHeapIdx[w->aHeap[p]] = p;
        w->aHeapIdx[w->aHeap[i]] = i;
        i = p;
    }
}

static void nav_ADown(NavWorld* w, int i) {
    for (;;) {
        int l = 2 * i + 1, r = l + 1, s = i;
        if (l < w->aHeapCount && w->aF[w->aHeap[l]] < w->aF[w->aHeap[s]]) s = l;
        if (r < w->aHeapCount && w->aF[w->aHeap[r]] < w->aF[w->aHeap[s]]) s = r;
        if (s == i) break;
        int t = w->aHeap[s]; w->aHeap[s] = w->aHeap[i]; w->aHeap[i] = t;
        w->aHeapIdx[w->aHeap[s]] = s;
        w->aHeapIdx[w->aHeap[i]] = i;
        i = s;
    }
}

static int nav_APop(NavWorld* w) {
    int top = w->aHeap[0];
    w->aHeapCount--;
    if (w->aHeapCount > 0) {
        w->aHeap[0] = w->aHeap[w->aHeapCount];
        w->aHeapIdx[w->aHeap[0]] = 0;
        nav_ADown(w, 0);
    }
    w->aHeapIdx[top] = -1;
    return top;
}

static inline void nav_ARelax(NavWorld* w, int cur, int nx, int ny, int nz, int stepCost, NavPoint goal, bool useH) {
    int nid = nav_CellId3(w, nx, ny, nz);
    nav_ATouch(w, nid);
    if (w->aState[nid] == 2) return;
    int ng = w->aG[cur] + stepCost;
    if (ng >= w->aG[nid]) return;
    w->aG[nid] = ng;
    w->aF[nid] = useH ? ng + nav_Heuristic3D(w, nx, ny, nz, goal.x, goal.y, goal.z) : ng;
    w->aParent[nid] = cur;
    if (w->aHeapIdx[nid] >= 0) {
        nav_AUp(w, w->aHeapIdx[nid]);
    } else {
        w->aState[nid] = 1;
        w->aHeap[w->aHeapCount] = nid;
        w->aHeapIdx[nid] = w->aHeapCount;
        w->aHeapCount++;
        nav_AUp(w, w->aHeapCount - 1);
    }
}

static bool nav_EnsureAStarBuffers(NavWorld* w) {
    if (w->aG) return true;
    size_t cells = (size_t)w->width * w->height * w->depth;
    w->aG       = malloc(cells * sizeof(int));
    w->aF       = malloc(cells * sizeof(int));
    w->aParent  = malloc(cells * sizeof(int));
    w->aState   = malloc(cells);
    w->aGen     = calloc(cells, sizeof(uint32_t));
    w->aHeap    = malloc(cells * sizeof(int));
    w->aHeapIdx = malloc(cells * sizeof(int));
    w->aCurGen = 0;
    if (w->aG && w->aF && w->aParent && w->aState && w->aGen && w->aHeap && w->aHeapIdx) return true;
    free(w->aG); free(w->aF); free(w->aParent); free(w->aState); free(w->aGen); free(w->aHeap); free(w->aHeapIdx);
    w->aG = w->aF = w->aParent = w->aHeap = w->aHeapIdx = NULL;
    w->aState = NULL;
    w->aGen = NULL;
    return false;
}

static bool nav_InBounds(const NavWorld* w, NavPoint p) {
    return p.x >= 0 && p.x < w->width && p.y >= 0 && p.y < w->height && p.z >= 0 && p.z < w->depth;
}

// Shared forward search (A* when useH, Dijkstra otherwise). Stops at `goal`
// (useH) or at the first cell isTarget accepts, or when the cheapest open cell
// costs more than maxCost (> 0). Returns the reached cell id or -1; parents
// and costs stay in the A* buffers for path reconstruction.
static int nav_ForwardSearch(NavWorld* w, NavPoint start, NavPoint goal, bool useH,
                             NavTargetFn isTarget, void* user, int maxCost) {
    w->explored = 0;
    if (!nav_InBounds(w, start)) return -1;
    if (useH && !nav_InBounds(w, goal)) return -1;
    if (!nav_EnsureAStarBuffers(w)) return -1;

    if (++w->aCurGen == 0) {   // wrapped after 2^32 searches
        memset(w->aGen, 0, (size_t)w->width * w->height * w->depth * sizeof(uint32_t));
        w->aCurGen = 1;
    }

    int sid = nav_CellId3(w, start.x, start.y, start.z);
    nav_ATouch(w, sid);
    w->aG[sid] = 0;
    w->aF[sid] = useH ? nav_Heuristic3D(w, start.x, start.y, start.z, goal.x, goal.y, goal.z) : 0;
    w->aState[sid] = 1;
    w->aHeap[0] = sid;
    w->aHeapIdx[sid] = 0;
    w->aHeapCount = 1;

    const int* dx = w->eightDir ? nav_kDx8 : nav_kDx4;
    const int* dy = w->eightDir ? nav_kDy8 : nav_kDy4;
    int numDirs = w->eightDir ? 8 : 4;
    int layer = w->width * w->height;
    int goalId = useH ? nav_CellId3(w, goal.x, goal.y, goal.z) : -1;

    while (w->aHeapCount > 0) {
        int cur = nav_APop(w);
        if (w->aState[cur] == 2) continue;
        if (maxCost > 0 && w->aG[cur] > maxCost) return -1;

        int bz = cur / layer;
        int rem = cur % layer;
        int by = rem / w->width;
        int bx = rem % w->width;

        if (cur == goalId || (isTarget && isTarget(user, bx, by, bz))) return cur;

        w->aState[cur] = 2;
        w->explored++;

        for (int i = 0; i < numDirs; i++) {
            int nx = bx + dx[i], ny = by + dy[i];
            if (!NavIsWalkable(NAV_HOSTP, nx, ny, bz)) continue;
            if (w->eightDir && dx[i] != 0 && dy[i] != 0) {
                if (!NavIsWalkable(NAV_HOSTP, nx, by, bz) || !NavIsWalkable(NAV_HOSTP, bx, ny, bz)) continue;
            }
            if (!NavCanEnterFrom(NAV_HOSTP, nx, ny, bz, bx, by)) continue;
            int baseCost = (dx[i] != 0 && dy[i] != 0) ? 14 : 10;
            nav_ARelax(w, cur, nx, ny, bz, (baseCost * NavMoveCost(NAV_HOSTP, nx, ny, bz)) / 10, goal, useH);
        }

        if (bz + 1 < w->depth && NavCanClimbUp(NAV_HOSTP, bx, by, bz))
            nav_ARelax(w, cur, bx, by, bz + 1, NavMoveCost(NAV_HOSTP, bx, by, bz + 1)
                       + NAV_CLIMB_EXTRA(NAV_HOSTP, bx, by, bz, 1), goal, useH);
        if (bz > 0 && NavCanClimbDown(NAV_HOSTP, bx, by, bz))
            nav_ARelax(w, cur, bx, by, bz - 1, NavMoveCost(NAV_HOSTP, bx, by, bz - 1)
                       + NAV_CLIMB_EXTRA(NAV_HOSTP, bx, by, bz, -1), goal, useH);

        int ex, ey;
        if (bz + 1 < w->depth && NavRampUp(NAV_HOSTP, bx, by, bz, &ex, &ey)) {
            if (ex >= 0 && ex < w->width && ey >= 0 && ey < w->height)
                nav_ARelax(w, cur, ex, ey, bz + 1, (14 * NavMoveCost(NAV_HOSTP, ex, ey, bz + 1)) / 10
                           + NAV_CLIMB_EXTRA(NAV_HOSTP, bx, by, bz, 1), goal, useH);
        }

        // Down a ramp whose high side points at us
        if (bz > 0) {
            static const int off[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
            for (int i = 0; i < 4; i++) {
                int rx = bx + off[i][0], ry = by + off[i][1], rz = bz - 1;
                int hdx, hdy;
                if (!NavRampHighSide(NAV_HOSTP, rx, ry, rz, &hdx, &hdy)) continue;
                if (rx + hdx != bx || ry + hdy != by) continue;
                if (!NavIsWalkable(NAV_HOSTP, rx, ry, rz)) continue;
                nav_ARelax(w, cur, rx, ry, rz, (14 * NavMoveCost(NAV_HOSTP, rx, ry, rz)) / 10
                           + NAV_CLIMB_EXTRA(NAV_HOSTP, bx, by, bz, -1), goal, useH);
            }
        }
    }
    return -1;
}

static int nav_ReconstructFromSearch(NavWorld* w, int id, NavPoint* out, int maxLen) {
    int layer = w->width * w->height, len = 0;
    for (; id >= 0 && len < maxLen; id = w->aParent[id]) {
        int rem = id % layer;
        out[len++] = (NavPoint){rem % w->width, rem / w->width, id / layer};
    }
    return len;
}

int NavFindPathAStar(NavWorld* w, NavPoint start, NavPoint goal, NavPoint* out, int maxLen) {
    w->lastCost = -1;
    double startTime = NavHostTime();
    int reached = nav_ForwardSearch(w, start, goal, true, NULL, NULL, 0);
    int len = 0;
    if (reached >= 0) {
        w->lastCost = w->aG[reached];
        len = nav_ReconstructFromSearch(w, reached, out, maxLen);
    }
    w->lastMs = (NavHostTime() - startTime) * 1000.0;
    return len;
}

int NavFindNearest(NavWorld* w, NavPoint start, NavTargetFn isTarget, void* user, int maxCost,
                   NavPoint* found, NavPoint* outPath, int maxLen, int* pathLen) {
    w->lastCost = -1;
    if (pathLen) *pathLen = 0;
    double startTime = NavHostTime();
    int reached = nav_ForwardSearch(w, start, start, false, isTarget, user, maxCost);
    int cost = -1;
    if (reached >= 0) {
        cost = w->lastCost = w->aG[reached];
        int layer = w->width * w->height, rem = reached % layer;
        if (found) *found = (NavPoint){rem % w->width, rem / w->width, reached / layer};
        if (outPath && pathLen) *pathLen = nav_ReconstructFromSearch(w, reached, outPath, maxLen);
    }
    w->lastMs = (NavHostTime() - startTime) * 1000.0;
    return cost;
}

// =============================================================================
// HPA* query
// =============================================================================

// Collect entrances of `chunk` and their costs from (p) via one Dijkstra.
// toP: costs from each entrance TO p instead (for goals: routes can be one-way).
// Entrances whose own chunk is one of the 3x3 around `chunk` (same level), ascending by
// index: a superset of the entrances in the chunk's region plus a 1-cell ring, read from
// the chunk index. -1 if the index is stale (callers then scan every entrance).
// out: room for 9 * NAV_MAX_ENTRANCES_PER_CHUNK.
static int nav_EntrancesAround(const NavWorld* w, int chunk, int* out) {
    if (w->chunkEntsVersion != w->graphVersion) return -1;
    int perLevel = w->chunksX * w->chunksY;
    int z = chunk / perLevel, xy = chunk % perLevel;
    int ccx = xy % w->chunksX, ccy = xy / w->chunksX, n = 0;
    for (int dcy = -1; dcy <= 1; dcy++)
        for (int dcx = -1; dcx <= 1; dcx++) {
            int ncx = ccx + dcx, ncy = ccy + dcy;
            if (ncx < 0 || ncy < 0 || ncx >= w->chunksX || ncy >= w->chunksY) continue;
            int nc = z * perLevel + ncy * w->chunksX + ncx;
            for (int k = 0; k < w->chunkEntCount[nc]; k++) {
                int e = w->chunkEnts[nc][k];
                const NavEntrance* E = &w->entranceList[e];
                // listed under every chunk whose edge region holds it: take it once,
                // from its own chunk
                if (E->x / w->chunkW != ncx || E->y / w->chunkH != ncy) continue;
                int j = n++;
                while (j > 0 && out[j - 1] > e) { out[j] = out[j - 1]; j--; }
                out[j] = e;
            }
        }
    return n;
}

static int nav_ConnectToChunk(NavWorld* w, NavPoint p, int chunk, int* outTargets, int* outCosts, bool toP) {
    int tx[NAV_CONNECT_MAX], ty[NAV_CONNECT_MAX], tIdx[NAV_CONNECT_MAX], costs[NAV_CONNECT_MAX];
    int minX, minY, maxX, maxY;
    nav_GetChunkBounds(w, chunk, &minX, &minY, &maxX, &maxY);
    if (maxX < w->width) maxX++;
    if (maxY < w->height) maxY++;
    if (minX > 0) minX--;
    if (minY > 0) minY--;
    // Every entrance of this level inside the search area (the chunk's own, and
    // neighbours' ladder/ramp/border cells on the ring). Candidates from the chunk
    // index (same order as scanning all entrances, which every search used to do)
    int cand[9 * NAV_MAX_ENTRANCES_PER_CHUNK];
    int candNum = nav_EntrancesAround(w, chunk, cand);
    int scanNum = candNum >= 0 ? candNum : w->entranceNum;
    int n = 0;
    for (int ci = 0; ci < scanNum && n < NAV_CONNECT_MAX; ci++) {
        int i = candNum >= 0 ? cand[ci] : ci;
        const NavEntrance* e = &w->entranceList[i];
        if (e->z != p.z || e->x < minX || e->x >= maxX || e->y < minY || e->y >= maxY) continue;
        tx[n] = e->x;
        ty[n] = e->y;
        tIdx[n] = i;
        n++;
    }
    if (n == 0) return 0;
    nav_ChunkCostsMultiDir(w, p.x, p.y, p.z, tx, ty, costs, n, minX, minY, maxX, maxY, toP, false);

    int count = 0;
    for (int i = 0; i < n; i++) {
        if (costs[i] < 0) continue;
        outTargets[count] = tIdx[i];
        outCosts[count] = costs[i];
        count++;
    }
    return count;
}

static void nav_AbsRelax(NavWorld* w, int from, int to, int stepCost, int h) {
    NavAbstractNode* n = &w->absNodes[to];
    if (n->closed) return;
    int ng = w->absNodes[from].g + stepCost;
    if (ng >= n->g) return;
    bool wasOpen = n->open;
    n->g = ng;
    n->f = ng + h;
    n->parent = from;
    n->open = true;
    if (wasOpen) nav_AbsHeapDecreaseKey(w, to);
    else nav_AbsHeapPush(w, to);
}

int NavFindPathHPA(NavWorld* w, NavPoint start, NavPoint goal, NavPoint* out, int maxLen) {
    w->explored = 0;
    w->absPathLen = 0;
    w->hpaAbstractMs = 0.0;
    w->hpaRefineMs = 0.0;
    if (!nav_InBounds(w, start) || !nav_InBounds(w, goal)) return 0;
    double startTime = NavHostTime();

    int startChunk = nav_GetChunk(w, start.x, start.y, start.z);
    int goalChunk = nav_GetChunk(w, goal.x, goal.y, goal.z);

    // Same chunk: try a local search first (also when there are no entrances at
    // all, e.g. one chunk or chunks walled off from each other)
    if (startChunk == goalChunk) {
        int len = nav_LocalPath(w, start.x, start.y, start.z, goal.x, goal.y, goal.z, out, maxLen);
        if (len > 0) {
            w->lastMs = (NavHostTime() - startTime) * 1000.0;
            return len;
        }
        // fall through: may need to route through neighbouring chunks
    }
    if (w->entranceNum == 0) return 0;   // no entrances (or graph not built)

    int startNode = w->entranceNum;
    int goalNode = w->entranceNum + 1;
    int totalNodes = w->entranceNum + 2;
    for (int i = 0; i < totalNodes; i++)
        w->absNodes[i] = (NavAbstractNode){NAV_COST_INF, NAV_COST_INF, -1, false, false};

    int startTargets[NAV_CONNECT_MAX], startCosts[NAV_CONNECT_MAX];
    int goalTargets[NAV_CONNECT_MAX], goalCosts[NAV_CONNECT_MAX];
    int startEdges = nav_ConnectToChunk(w, start, startChunk, startTargets, startCosts, false);
    int goalEdges = nav_ConnectToChunk(w, goal, goalChunk, goalTargets, goalCosts, true);
    w->explored += 2;

    // A* over the abstract graph
    double absStart = NavHostTime();
    nav_AbsHeapInit(w, totalNodes);
    w->absNodes[startNode].g = 0;
    // Same admissible estimate as A* (scaled by the cheapest cell cost). The old
    // unscaled |dx|+|dy| was ~10x too weak: near-Dijkstra over every entrance.
    w->absNodes[startNode].f = nav_Heuristic3D(w, start.x, start.y, start.z, goal.x, goal.y, goal.z);
    w->absNodes[startNode].open = true;
    nav_AbsHeapPush(w, startNode);

    while (w->absHeapSize > 0) {
        int best = nav_AbsHeapPop(w);
        if (best == goalNode) {
            for (int c = goalNode; c >= 0 && w->absPathLen < w->entranceCap + 2; c = w->absNodes[c].parent)
                w->absPath[w->absPathLen++] = c;
            break;
        }
        w->absNodes[best].open = false;
        w->absNodes[best].closed = true;
        w->explored++;

        if (best == startNode) {
            for (int i = 0; i < startEdges; i++) {
                int nb = startTargets[i];
                nav_AbsRelax(w, best, nb, startCosts[i],
                         nav_Heuristic3D(w, w->entranceList[nb].x, w->entranceList[nb].y, w->entranceList[nb].z,
                                         goal.x, goal.y, goal.z));
            }
        } else if (best < w->entranceNum) {
            const NavEdgeVec* out = &w->adj[best];
            for (int i = 0; i < out->num; i++) {
                const NavGraphEdge* e = &out->e[i];
                nav_AbsRelax(w, best, e->to, e->cost + nav_LinkExtra(w, e),
                         nav_Heuristic3D(w, w->entranceList[e->to].x, w->entranceList[e->to].y, w->entranceList[e->to].z,
                                         goal.x, goal.y, goal.z));
            }
            for (int i = 0; i < goalEdges; i++)
                if (goalTargets[i] == best) nav_AbsRelax(w, best, goalNode, goalCosts[i], 0);
        }
    }
    w->hpaAbstractMs = (NavHostTime() - absStart) * 1000.0;

    // Refine abstract path (stored goal-first) into cells, start-first, then reverse
    double refineStart = NavHostTime();
    int len = 0;
    if (w->absPathLen > 0) {
        for (int i = w->absPathLen - 1; i > 0; i--) {
            int fromNode = w->absPath[i];
            int toNode = w->absPath[i - 1];
            NavPoint f = (fromNode == startNode) ? start
                       : (NavPoint){w->entranceList[fromNode].x, w->entranceList[fromNode].y, w->entranceList[fromNode].z};
            NavPoint t = (toNode == goalNode) ? goal
                       : (NavPoint){w->entranceList[toNode].x, w->entranceList[toNode].y, w->entranceList[toNode].z};

            if (f.z != t.z) {   // ladder/ramp hop: the mover handles the transition
                if (len < maxLen) out[len++] = t;
                continue;
            }

            int localLen = nav_LocalPath(w, f.x, f.y, f.z, t.x, t.y, t.z, w->tempPath, w->tempPathCap);
            if (localLen == 0) continue;   // shouldn't happen with a valid abstract path

            // tempPath is goal-first; append source->dest, skipping the shared source point
            int skipSource = (i == w->absPathLen - 1) ? 0 : 1;
            for (int j = localLen - 1 - skipSource; j >= 0 && len < maxLen; j--)
                out[len++] = w->tempPath[j];
        }
        for (int i = 0; i < len / 2; i++) {
            NavPoint tmp = out[i];
            out[i] = out[len - 1 - i];
            out[len - 1 - i] = tmp;
        }
    }
    w->hpaRefineMs = (NavHostTime() - refineStart) * 1000.0;
    w->lastMs = (NavHostTime() - startTime) * 1000.0;
    return len;
}

// =============================================================================
// Flow fields (reverse Dijkstra over the same edges as NavFindPathAStar)
// =============================================================================

NavFlowField* NavFlowCreate(const NavWorld* w) {
    NavFlowField* f = calloc(1, sizeof(NavFlowField));
    if (!f) return NULL;
    size_t cells = (size_t)w->width * w->height * w->depth;
    f->width = w->width; f->height = w->height; f->depth = w->depth;
    f->cost = malloc(cells * sizeof(int));
    f->next = malloc(cells * sizeof(int));
    f->steer = malloc(cells * sizeof(int));
    f->steerLimit = malloc(cells * sizeof(int));
    f->anyAngle = true;
    f->heapCap = 1024;
    f->heapCost = malloc((size_t)f->heapCap * sizeof(int));
    f->heapCell = malloc((size_t)f->heapCap * sizeof(int));
    if (!f->cost || !f->next || !f->steer || !f->steerLimit || !f->heapCost || !f->heapCell) {
        NavFlowDestroy(f);
        return NULL;
    }
    for (size_t i = 0; i < cells; i++) { f->cost[i] = NAV_FLOW_UNREACHABLE; f->next[i] = -1; f->steer[i] = -1; }
    return f;
}

void NavFlowDestroy(NavFlowField* f) {
    if (!f) return;
    free(f->cost); free(f->next); free(f->steer); free(f->steerLimit);
    free(f->heapCost); free(f->heapCell);
    free(f);
}

static bool nav_FlowPush(NavFlowField* f, int cost, int cell) {
    if (f->heapSize == f->heapCap) {
        int cap = f->heapCap * 2;
        int* hc = realloc(f->heapCost, (size_t)cap * sizeof(int));
        if (!hc) return false;
        f->heapCost = hc;
        int* hl = realloc(f->heapCell, (size_t)cap * sizeof(int));
        if (!hl) return false;
        f->heapCell = hl;
        f->heapCap = cap;
    }
    int i = f->heapSize++;
    while (i > 0) {
        int p = (i - 1) / 2;
        if (f->heapCost[p] <= cost) break;
        f->heapCost[i] = f->heapCost[p];
        f->heapCell[i] = f->heapCell[p];
        i = p;
    }
    f->heapCost[i] = cost;
    f->heapCell[i] = cell;
    return true;
}

static void nav_FlowPop(NavFlowField* f, int* cost, int* cell) {
    *cost = f->heapCost[0];
    *cell = f->heapCell[0];
    int lastCost = f->heapCost[--f->heapSize];
    int lastCell = f->heapCell[f->heapSize];
    int i = 0;
    for (;;) {
        int l = 2 * i + 1, r = l + 1, s = l;
        if (l >= f->heapSize) break;
        if (r < f->heapSize && f->heapCost[r] < f->heapCost[l]) s = r;
        if (f->heapCost[s] >= lastCost) break;
        f->heapCost[i] = f->heapCost[s];
        f->heapCell[i] = f->heapCell[s];
        i = s;
    }
    f->heapCost[i] = lastCost;
    f->heapCell[i] = lastCell;
}

// Straight walk on level z from (x0,y0) to (x1,y1) through cell centres: every
// cell the segment touches (supercover; both sides when it passes exactly
// through a corner) must be walkable with move cost <= limit.
static bool nav_FlowLineClear(NavWorld* w, int x0, int y0, int x1, int y1, int z, int limit) {
    #define NAV_LOS_CELL(cx, cy) do { \
        if (!NavIsWalkable(NAV_HOSTP, (cx), (cy), z) || NavMoveCost(NAV_HOSTP, (cx), (cy), z) > limit) return false; \
    } while (0)
    int dx = x1 - x0, dy = y1 - y0;
    int sx = dx < 0 ? -1 : 1, sy = dy < 0 ? -1 : 1;
    dx = abs(dx); dy = abs(dy);
    int ddx = 2 * dx, ddy = 2 * dy;
    int x = x0, y = y0;
    if (ddx >= ddy) {
        int err = dx, prev = dx;
        for (int i = 0; i < dx; i++) {
            x += sx;
            err += ddy;
            if (err > ddx) {
                y += sy;
                err -= ddx;
                if (err + prev < ddx) NAV_LOS_CELL(x, y - sy);
                else if (err + prev > ddx) NAV_LOS_CELL(x - sx, y);
                else { NAV_LOS_CELL(x, y - sy); NAV_LOS_CELL(x - sx, y); }
            }
            NAV_LOS_CELL(x, y);
            prev = err;
        }
    } else {
        int err = dy, prev = dy;
        for (int i = 0; i < dy; i++) {
            y += sy;
            err += ddx;
            if (err > ddy) {
                x += sx;
                err -= ddy;
                if (err + prev < ddy) NAV_LOS_CELL(x - sx, y);
                else if (err + prev > ddy) NAV_LOS_CELL(x, y - sy);
                else { NAV_LOS_CELL(x - sx, y); NAV_LOS_CELL(x, y - sy); }
            }
            NAV_LOS_CELL(x, y);
            prev = err;
        }
    }
    #undef NAV_LOS_CELL
    return true;
}

// Called when cell c is finalised (popped). Its `next` was finalised earlier,
// so steer[next] is known: inherit it if c can see it, else aim at next.
static void nav_FlowSetSteer(NavWorld* w, NavFlowField* f, int c, NavPoint C, int cellCost) {
    int n = f->next[c];
    if (n < 0) { f->steer[c] = -1; f->steerLimit[c] = cellCost; return; }
    NavPoint N = NavFlowCellPoint(f, n);
    int nCost = NavMoveCost(NAV_HOSTP, N.x, N.y, N.z);
    f->steer[c] = n;
    f->steerLimit[c] = cellCost > nCost ? cellCost : nCost;
    if (!f->anyAngle || N.z != C.z) return;
    int t = f->steer[n];
    if (t < 0) return;
    NavPoint T = NavFlowCellPoint(f, t);
    if (T.z != C.z) return;
    int limit = cellCost > f->steerLimit[n] ? cellCost : f->steerLimit[n];
    if (nav_FlowLineClear(w, C.x, C.y, T.x, T.y, C.z, limit)) {
        f->steer[c] = t;
        f->steerLimit[c] = limit;
    }
}

// Predecessor p can step to cell c (= p's `next`) for `step`; record if better.
static inline void nav_FlowRelax(NavFlowField* f, int p, int c, int newCost, int maxCost) {
    if (maxCost > 0 && newCost > maxCost) return;
    if (newCost >= f->cost[p]) return;
    f->cost[p] = newCost;
    f->next[p] = c;
    nav_FlowPush(f, newCost, p);
}

void NavComputeClearance(NavWorld* w, uint8_t* out) {
    int W = w->width, Hh = w->height;
    size_t layer = (size_t)W * Hh;
    int* queue = malloc(layer * sizeof(int));
    if (!queue) return;
    for (int z = 0; z < w->depth; z++) {
        uint8_t* o = out + layer * z;
        int qh = 0, qt = 0;
        for (int y = 0; y < Hh; y++)
            for (int x = 0; x < W; x++) {
                int i = y * W + x;
                if (!NavIsWalkable(NAV_HOSTP, x, y, z)) { o[i] = 0; continue; }
                o[i] = 255;
                // Seed: walkable cells touching a blocked cell or the map edge get 1
                for (int dy = -1; dy <= 1 && o[i] == 255; dy++)
                    for (int dx = -1; dx <= 1; dx++) {
                        if (!dx && !dy) continue;
                        if (!NavIsWalkable(NAV_HOSTP, x + dx, y + dy, z)) { o[i] = 1; queue[qt++] = i; break; }
                    }
            }
        while (qh < qt) {
            int i = queue[qh++];
            int x = i % W, y = i / W;
            int nd = o[i] + 1;
            if (nd > 255) continue;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    int nx = x + dx, ny = y + dy;
                    if ((!dx && !dy) || nx < 0 || ny < 0 || nx >= W || ny >= Hh) continue;
                    int j = ny * W + nx;
                    if (o[j] > nd) { o[j] = (uint8_t)nd; queue[qt++] = j; }
                }
        }
    }
    free(queue);
}

int NavFlowBuild(NavWorld* w, NavFlowField* f, const NavPoint* goals, int goalCount, int maxCost) {
    if (f->width != w->width || f->height != w->height || f->depth != w->depth) return 0;
    double startTime = NavHostTime();
    size_t cells = (size_t)f->width * f->height * f->depth;
    for (size_t i = 0; i < cells; i++) { f->cost[i] = NAV_FLOW_UNREACHABLE; f->next[i] = -1; f->steer[i] = -1; }
    f->heapSize = 0;
    f->reached = 0;

    for (int i = 0; i < goalCount; i++) {
        NavPoint g = goals[i];
        if (!NavFlowInBounds(f, g) || !NavIsWalkable(NAV_HOSTP, g.x, g.y, g.z)) continue;
        int id = NavFlowCellId(f, g.x, g.y, g.z);
        if (f->cost[id] == 0) continue;
        f->cost[id] = 0;
        nav_FlowPush(f, 0, id);
    }

    const int* dx = w->eightDir ? nav_kDx8 : nav_kDx4;
    const int* dy = w->eightDir ? nav_kDy8 : nav_kDy4;
    int numDirs = w->eightDir ? 8 : 4;
    static const int off4[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};

    while (f->heapSize > 0) {
        int d, c;
        nav_FlowPop(f, &d, &c);
        if (d > f->cost[c]) continue;   // stale entry
        f->reached++;
        NavPoint C = NavFlowCellPoint(f, c);
        int x = C.x, y = C.y, z = C.z;
        int cellCost = NavMoveCost(NAV_HOSTP, x, y, z);
        nav_FlowSetSteer(w, f, c, C, cellCost);       // steering uses terrain cost only
        if (f->extraCost) cellCost += f->extraCost[c];  // routing uses terrain + extra

        // Same level: P = C - dir steps onto C (mirror of A*'s neighbour step)
        for (int i = 0; i < numDirs; i++) {
            int px = x - dx[i], py = y - dy[i];
            if (!NavIsWalkable(NAV_HOSTP, px, py, z)) continue;
            if (w->eightDir && dx[i] != 0 && dy[i] != 0) {
                if (!NavIsWalkable(NAV_HOSTP, x, py, z) || !NavIsWalkable(NAV_HOSTP, px, y, z)) continue;
            }
            if (!NavCanEnterFrom(NAV_HOSTP, x, y, z, px, py)) continue;
            int base = (dx[i] != 0 && dy[i] != 0) ? 14 : 10;
            nav_FlowRelax(f, NavFlowCellId(f, px, py, z), c, d + (base * cellCost) / 10, maxCost);
        }

        // Ladders: climb up from below / down from above onto C
        if (z > 0 && NavCanClimbUp(NAV_HOSTP, x, y, z - 1))
            nav_FlowRelax(f, NavFlowCellId(f, x, y, z - 1), c, d + cellCost + NAV_CLIMB_EXTRA(NAV_HOSTP, x, y, z - 1, 1), maxCost);
        if (z + 1 < f->depth && NavCanClimbDown(NAV_HOSTP, x, y, z + 1))
            nav_FlowRelax(f, NavFlowCellId(f, x, y, z + 1), c, d + cellCost + NAV_CLIMB_EXTRA(NAV_HOSTP, x, y, z + 1, -1), maxCost);

        // Ramp up onto C: a ramp one level down whose exit is C
        if (z > 0) {
            for (int i = 0; i < 4; i++) {
                int rx = x + off4[i][0], ry = y + off4[i][1];
                int ex, ey;
                if (!NavRampUp(NAV_HOSTP, rx, ry, z - 1, &ex, &ey)) continue;
                if (ex != x || ey != y) continue;
                nav_FlowRelax(f, NavFlowCellId(f, rx, ry, z - 1), c,
                              d + (14 * cellCost) / 10 + NAV_CLIMB_EXTRA(NAV_HOSTP, rx, ry, z - 1, 1), maxCost);
            }
        }

        // Ramp down onto C: C is a ramp; its high-side cell one level up walks down onto it
        int hdx, hdy;
        if (z + 1 < f->depth && NavRampHighSide(NAV_HOSTP, x, y, z, &hdx, &hdy)) {
            int px = x + hdx, py = y + hdy;
            if (px >= 0 && px < f->width && py >= 0 && py < f->height
                && NavIsWalkable(NAV_HOSTP, px, py, z + 1)) {
                nav_FlowRelax(f, NavFlowCellId(f, px, py, z + 1), c,
                              d + (14 * cellCost) / 10 + NAV_CLIMB_EXTRA(NAV_HOSTP, px, py, z + 1, -1), maxCost);
            }
        }
    }

    f->buildMs = (NavHostTime() - startTime) * 1000.0;
    return f->reached;
}

int NavFindPath(NavWorld* w, NavAlgorithm algo, NavPoint start, NavPoint goal, NavPoint* out, int maxLen) {
    switch (algo) {
        case NAV_ALGO_ASTAR: return NavFindPathAStar(w, start, goal, out, maxLen);
        case NAV_ALGO_HPA:   return NavFindPathHPA(w, start, goal, out, maxLen);
    }
    return 0;
}

// =============================================================================
// Reachability regions: per-chunk components + union-find across chunk borders
// and ladder/ramp links. Relabelling a few chunks after an edit is cheap; the
// global join only touches chunk-border cells and links.
// =============================================================================

typedef struct NavRegLinks { int* a; int* b; int n, cap; } NavRegLinks;   // cell-id pairs

static inline int nav_Cell3(const NavWorld* w, int x, int y, int z) { return (z * w->height + y) * w->width + x; }
static inline int nav_ChunkOfCell(const NavWorld* w, int x, int y, int z) {
    return z * (w->chunksX * w->chunksY) + (y / w->chunkH) * w->chunksX + (x / w->chunkW);
}

static void nav_RegFree(NavWorld* w) {
    if (w->regLinks) {
        int chunks = w->depth * w->chunksX * w->chunksY;
        for (int c = 0; c < chunks; c++) { free(w->regLinks[c].a); free(w->regLinks[c].b); }
    }
    free(w->regLinks); free(w->regLocal); free(w->regCompBase); free(w->regCompCount);
    free(w->regRoot); free(w->regChunkDirty);
    w->regLinks = NULL; w->regLocal = NULL; w->regCompBase = w->regCompCount = w->regRoot = NULL;
    w->regChunkDirty = NULL; w->regBuilt = false; w->regNodeCap = w->regCount = w->regDirtyCount = 0;
}

// Can an agent step between same-level neighbours a and b in at least one direction?
static bool nav_RegStep(NavWorld* w, int ax, int ay, int bx, int by, int z) {
    if (!NAV_REGION_WALKABLE(NAV_HOSTP, ax, ay, z) || !NAV_REGION_WALKABLE(NAV_HOSTP, bx, by, z)) return false;
    if (ax != bx && ay != by) {
        if (!w->eightDir) return false;
        if (!NAV_REGION_WALKABLE(NAV_HOSTP, bx, ay, z) || !NAV_REGION_WALKABLE(NAV_HOSTP, ax, by, z)) return false;
    }
    return NavCanEnterFrom(NAV_HOSTP, bx, by, z, ax, ay) || NavCanEnterFrom(NAV_HOSTP, ax, ay, z, bx, by);
}

static void nav_RegAddLink(NavRegLinks* L, int a, int b) {
    if (L->n == L->cap) {
        int cap = L->cap ? L->cap * 2 : 8;
        int* na = realloc(L->a, (size_t)cap * sizeof(int));
        if (!na) return;
        L->a = na;
        int* nb = realloc(L->b, (size_t)cap * sizeof(int));
        if (!nb) return;
        L->b = nb;
        L->cap = cap;
    }
    L->a[L->n] = a; L->b[L->n] = b; L->n++;
}

static void nav_RegLabelChunk(NavWorld* w, int chunk, int* queue) {
    int perLevel = w->chunksX * w->chunksY;
    int z = chunk / perLevel, xy = chunk % perLevel;
    int x0 = (xy % w->chunksX) * w->chunkW, y0 = (xy / w->chunksX) * w->chunkH;
    int x1 = x0 + w->chunkW, y1 = y0 + w->chunkH;
    if (x1 > w->width) x1 = w->width;
    if (y1 > w->height) y1 = w->height;
    const int* dx = nav_kDx8;
    const int* dy = nav_kDy8;

    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) w->regLocal[nav_Cell3(w, x, y, z)] = 0;

    int comp = 0;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            int c = nav_Cell3(w, x, y, z);
            if (w->regLocal[c] || !NAV_REGION_WALKABLE(NAV_HOSTP, x, y, z)) continue;
            if (comp == 65535) break;
            comp++;
            int qh = 0, qt = 0;
            w->regLocal[c] = (uint16_t)comp;
            queue[qt++] = (y - y0) * w->chunkW + (x - x0);
            while (qh < qt) {
                int q = queue[qh++];
                int qx = x0 + q % w->chunkW, qy = y0 + q / w->chunkW;
                for (int k = 0; k < 8; k++) {
                    int nx = qx + dx[k], ny = qy + dy[k];
                    if (nx < x0 || ny < y0 || nx >= x1 || ny >= y1) continue;
                    int nc = nav_Cell3(w, nx, ny, z);
                    if (w->regLocal[nc] || !nav_RegStep(w, qx, qy, nx, ny, z)) continue;
                    w->regLocal[nc] = (uint16_t)comp;
                    queue[qt++] = (ny - y0) * w->chunkW + (nx - x0);
                }
            }
        }
    w->regCompCount[chunk] = comp;

    // Cross-level links starting in this chunk
    NavRegLinks* L = &w->regLinks[chunk];
    L->n = 0;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            int c = nav_Cell3(w, x, y, z), ex, ey;
            if (z + 1 < w->depth && NavCanClimbUp(NAV_HOSTP, x, y, z)) nav_RegAddLink(L, c, nav_Cell3(w, x, y, z + 1));
            if (z + 1 < w->depth && NavRampUp(NAV_HOSTP, x, y, z, &ex, &ey) &&
                ex >= 0 && ey >= 0 && ex < w->width && ey < w->height)
                nav_RegAddLink(L, c, nav_Cell3(w, ex, ey, z + 1));
        }
}

static int nav_RegNode(const NavWorld* w, int cell) {
    int layer = w->width * w->height;
    int z = cell / layer, rem = cell % layer;
    int local = w->regLocal[cell];
    if (!local) return -1;
    return w->regCompBase[nav_ChunkOfCell(w, rem % w->width, rem / w->width, z)] + local - 1;
}

static int nav_UFFind(int* p, int i) {
    while (p[i] != i) { p[i] = p[p[i]]; i = p[i]; }
    return i;
}

static void nav_UFUnion(int* p, int a, int b) {
    if (a < 0 || b < 0) return;
    a = nav_UFFind(p, a); b = nav_UFFind(p, b);
    if (a != b) p[a < b ? b : a] = a < b ? a : b;
}

static void nav_RegJoin(NavWorld* w) {
    int chunks = w->depth * w->chunksX * w->chunksY, nodes = 0;
    for (int c = 0; c < chunks; c++) { w->regCompBase[c] = nodes; nodes += w->regCompCount[c]; }
    if (nodes > w->regNodeCap) {
        int* r = realloc(w->regRoot, (size_t)(nodes > 0 ? nodes : 1) * sizeof(int));
        if (!r) return;
        w->regRoot = r;
        w->regNodeCap = nodes;
    }
    int* p = w->regRoot;
    for (int i = 0; i < nodes; i++) p[i] = i;

    for (int z = 0; z < w->depth; z++) {
        // Vertical chunk borders: (bx-1, y) | (bx, y) and the two diagonals across
        for (int bx = w->chunkW; bx < w->width; bx += w->chunkW)
            for (int y = 0; y < w->height; y++) {
                if (nav_RegStep(w, bx - 1, y, bx, y, z))
                    nav_UFUnion(p, nav_RegNode(w, nav_Cell3(w, bx - 1, y, z)), nav_RegNode(w, nav_Cell3(w, bx, y, z)));
                if (y + 1 < w->height) {
                    if (nav_RegStep(w, bx - 1, y, bx, y + 1, z))
                        nav_UFUnion(p, nav_RegNode(w, nav_Cell3(w, bx - 1, y, z)), nav_RegNode(w, nav_Cell3(w, bx, y + 1, z)));
                    if (nav_RegStep(w, bx - 1, y + 1, bx, y, z))
                        nav_UFUnion(p, nav_RegNode(w, nav_Cell3(w, bx - 1, y + 1, z)), nav_RegNode(w, nav_Cell3(w, bx, y, z)));
                }
            }
        // Horizontal chunk borders
        for (int by = w->chunkH; by < w->height; by += w->chunkH)
            for (int x = 0; x < w->width; x++) {
                if (nav_RegStep(w, x, by - 1, x, by, z))
                    nav_UFUnion(p, nav_RegNode(w, nav_Cell3(w, x, by - 1, z)), nav_RegNode(w, nav_Cell3(w, x, by, z)));
                if (x + 1 < w->width) {
                    if (nav_RegStep(w, x, by - 1, x + 1, by, z))
                        nav_UFUnion(p, nav_RegNode(w, nav_Cell3(w, x, by - 1, z)), nav_RegNode(w, nav_Cell3(w, x + 1, by, z)));
                    if (nav_RegStep(w, x + 1, by - 1, x, by, z))
                        nav_UFUnion(p, nav_RegNode(w, nav_Cell3(w, x + 1, by - 1, z)), nav_RegNode(w, nav_Cell3(w, x, by, z)));
                }
            }
    }
    for (int c = 0; c < chunks; c++)
        for (int k = 0; k < w->regLinks[c].n; k++)
            nav_UFUnion(p, nav_RegNode(w, w->regLinks[c].a[k]), nav_RegNode(w, w->regLinks[c].b[k]));

    w->regCount = 0;
    for (int i = 0; i < nodes; i++) { p[i] = nav_UFFind(p, i); if (p[i] == i) w->regCount++; }
}

static bool nav_RegEnsure(NavWorld* w) {
    int chunks = w->depth * w->chunksX * w->chunksY;
    if (!w->regBuilt) {
        nav_RegFree(w);
        w->regLocal = calloc((size_t)w->width * w->height * w->depth, sizeof(uint16_t));
        w->regCompBase = calloc((size_t)chunks, sizeof(int));
        w->regCompCount = calloc((size_t)chunks, sizeof(int));
        w->regChunkDirty = calloc((size_t)chunks, sizeof(bool));
        w->regLinks = calloc((size_t)chunks, sizeof(NavRegLinks));
        if (!w->regLocal || !w->regCompBase || !w->regCompCount || !w->regChunkDirty || !w->regLinks) {
            nav_RegFree(w);
            return false;
        }
        for (int c = 0; c < chunks; c++) w->regChunkDirty[c] = true;
        w->regDirtyCount = chunks;
        w->regBuilt = true;
    }
    if (w->regDirtyCount > 0) {
        int* queue = malloc((size_t)w->chunkW * w->chunkH * sizeof(int));
        if (!queue) return false;
        for (int c = 0; c < chunks; c++)
            if (w->regChunkDirty[c]) { nav_RegLabelChunk(w, c, queue); w->regChunkDirty[c] = false; }
        free(queue);
        w->regDirtyCount = 0;
        nav_RegJoin(w);
    }
    return true;
}

// Called from NavMarkCellChanged: relabel the 3x3x3 chunk neighbourhood (links
// such as ramps and ladders reach into neighbouring chunks and levels).
static void nav_RegMarkDirty(NavWorld* w, int x, int y, int z) {
    if (!w->regBuilt) return;
    int cx = x / w->chunkW, cy = y / w->chunkH;
    for (int dz = -1; dz <= 1; dz++)
        for (int dcy = -1; dcy <= 1; dcy++)
            for (int dcx = -1; dcx <= 1; dcx++) {
                int nz = z + dz, ncx = cx + dcx, ncy = cy + dcy;
                if (nz < 0 || nz >= w->depth || ncx < 0 || ncy < 0 || ncx >= w->chunksX || ncy >= w->chunksY) continue;
                int c = nz * (w->chunksX * w->chunksY) + ncy * w->chunksX + ncx;
                if (!w->regChunkDirty[c]) { w->regChunkDirty[c] = true; w->regDirtyCount++; }
            }
}

int NavRegionId(NavWorld* w, NavPoint p) {
    if (!nav_InBounds(w, p) || !nav_RegEnsure(w)) return -1;
    int node = nav_RegNode(w, nav_Cell3(w, p.x, p.y, p.z));
    return node < 0 ? -1 : w->regRoot[node];
}

bool NavReachable(NavWorld* w, NavPoint a, NavPoint b) {
    int ra = NavRegionId(w, a);
    return ra >= 0 && ra == NavRegionId(w, b);
}

void NavRegionsInvalidate(NavWorld* w) {
    if (w) w->regBuilt = false;
}

int NavRegionCount(NavWorld* w) {
    return nav_RegEnsure(w) ? w->regCount : 0;
}

// =============================================================================
// Hierarchical flow fields (abstract Dijkstra over HPA* entrances + lazy
// per-chunk local fields)
// =============================================================================
//
// Seeding rule for the local field of chunk C: entrance e of C is a SEED (an
// exit, with its abstract cost-to-goal) unless its next hop lies inside C too
// (then e is just a cell on the way). Following the abstract chain from any
// entrance of C eventually leaves C or reaches the goal, so every chunk with
// a route has at least one seed. Standing on a seed, the next step comes from
// the chunk shared with the next entrance (where e is NOT a seed), or from
// the ladder/ramp link.

typedef struct NavChunkFlow {
    int minX, minY, w, h, z;       // region: chunk expanded by 1 cell (clipped)
    int* cost;                     // cost to goal via this chunk's exits
    int* next;                     // region index of next cell; -1 at seeds / unreachable
    int* seed;                     // entrance index at seed cells, -2 = goal, -1 = not a seed
    bool provisional;              // read a field that was mid-build or may still change
    int* deps; int depNum, depCap; // chunks whose fields this one's seeds read
} NavChunkFlow;

// Sentinel (identity only, never written) while a chunk field is being built. Exit
// seed values come from the field that continues the route, and chunk-level
// dependencies can be cyclic (different routes cross the same chunks in different
// orders, neighbours seed each other's ring cells), so a field may ask for one that
// is still being built. It then leaves that exit out (marked provisional); once the
// outermost build finishes, a worklist rebuilds fields whose inputs changed.
static NavChunkFlow nav_ChunkFlowBuilding;

typedef struct { int* cost; int* item; int size, cap; } nav_LazyHeap;

static bool nav_LHPush(nav_LazyHeap* q, int cost, int item) {
    if (q->size == q->cap) {
        int cap = q->cap ? q->cap * 2 : 256;
        int* c = realloc(q->cost, (size_t)cap * sizeof(int));
        if (!c) return false;
        q->cost = c;
        int* it = realloc(q->item, (size_t)cap * sizeof(int));
        if (!it) return false;
        q->item = it;
        q->cap = cap;
    }
    int i = q->size++;
    while (i > 0) {
        int p = (i - 1) / 2;
        if (q->cost[p] <= cost) break;
        q->cost[i] = q->cost[p]; q->item[i] = q->item[p];
        i = p;
    }
    q->cost[i] = cost; q->item[i] = item;
    return true;
}

static void nav_LHPop(nav_LazyHeap* q, int* cost, int* item) {
    *cost = q->cost[0]; *item = q->item[0];
    int lc = q->cost[--q->size], li = q->item[q->size];
    int i = 0;
    for (;;) {
        int l = 2 * i + 1, r = l + 1, s = l;
        if (l >= q->size) break;
        if (r < q->size && q->cost[r] < q->cost[l]) s = r;
        if (q->cost[s] >= lc) break;
        q->cost[i] = q->cost[s]; q->item[i] = q->item[s];
        i = s;
    }
    q->cost[i] = lc; q->item[i] = li;
}

static void nav_LHFree(nav_LazyHeap* q) { free(q->cost); free(q->item); q->cost = q->item = NULL; q->size = q->cap = 0; }

static void nav_FreeChunkFlow(NavChunkFlow* cf) {
    if (!cf) return;
    free(cf->cost); free(cf->next); free(cf->seed); free(cf->deps); free(cf);
}

static void nav_HFlowClearChunks(NavHFlow* h) {
    for (int i = 0; i < h->chunkCount; i++) {
        if (h->chunks[i] != &nav_ChunkFlowBuilding) nav_FreeChunkFlow(h->chunks[i]);
        h->chunks[i] = NULL;
    }
    h->chunksBuilt = 0;
}

static inline int nav_HFlowExtra(const NavWorld* w, const NavHFlow* h, int x, int y, int z) {
    return h->extraCost ? h->extraCost[(z * w->height + y) * w->width + x] : 0;
}

static void nav_HFlowRebuild(NavWorld* w, NavHFlow* h) {
    double t0 = NavHostTime();
    h->stale = false;
    nav_HFlowClearChunks(h);
    int perLevel = w->chunksX * w->chunksY;
    if (h->chunkCount != w->depth * perLevel) {
        free(h->chunks);
        h->chunkCount = w->depth * perLevel;
        h->chunks = calloc((size_t)h->chunkCount, sizeof(NavChunkFlow*));
    }
    h->graphVersion = w->graphVersion;
    h->valid = false;
    int n = w->entranceNum > 0 ? w->entranceNum : 1;
    if (n > h->entNum || !h->dist) {
        free(h->dist); free(h->nextEnt); free(h->viaChunk);
        h->dist = malloc((size_t)n * sizeof(int));
        h->nextEnt = malloc((size_t)n * sizeof(int));
        h->viaChunk = malloc((size_t)n * sizeof(int));
    }
    h->entNum = w->entranceNum;
    if (!h->chunks || !h->dist || !h->nextEnt || !h->viaChunk) return;
    for (int e = 0; e < w->entranceNum; e++) { h->dist[e] = NAV_FLOW_UNREACHABLE; h->nextEnt[e] = -2; h->viaChunk[e] = -1; }

    NavPoint g = h->goal;
    if (!nav_InBounds(w, g) || !NavIsWalkable(NAV_HOSTP, g.x, g.y, g.z)) return;
    h->goalChunk = nav_GetChunk(w, g.x, g.y, g.z);

    // Entrances of the goal's chunk -> goal
    nav_LazyHeap q = {0};
    if (w->entranceNum > 0) {
        int targets[NAV_CONNECT_MAX], costs[NAV_CONNECT_MAX];
        int k = nav_ConnectToChunk(w, g, h->goalChunk, targets, costs, true);
        for (int i = 0; i < k; i++) {
            int e = targets[i];
            if (costs[i] < h->dist[e]) { h->dist[e] = costs[i]; h->nextEnt[e] = -1; h->viaChunk[e] = h->goalChunk; nav_LHPush(&q, costs[i], e); }
        }
    }
    // Backward Dijkstra over the directed entrance graph: needs each entrance's
    // INCOMING edges (adjacency stores outgoing), indexed here per rebuild
    int ne = w->entranceNum;
    int* inStart = calloc((size_t)ne + 1, sizeof(int));
    const NavGraphEdge** inEdge = malloc((size_t)(w->edgeNum > 0 ? w->edgeNum : 1) * sizeof(*inEdge));
    if (!inStart || !inEdge) { free(inStart); free(inEdge); nav_LHFree(&q); return; }
    for (int f = 0; f < ne; f++)
        for (int k = 0; k < w->adj[f].num; k++) inStart[w->adj[f].e[k].to + 1]++;
    for (int e = 0; e < ne; e++) inStart[e + 1] += inStart[e];
    {
        int* fill = malloc((size_t)(ne > 0 ? ne : 1) * sizeof(int));
        if (!fill) { free(inStart); free(inEdge); nav_LHFree(&q); return; }
        memcpy(fill, inStart, (size_t)ne * sizeof(int));
        for (int f = 0; f < ne; f++)
            for (int k = 0; k < w->adj[f].num; k++) inEdge[fill[w->adj[f].e[k].to]++] = &w->adj[f].e[k];
        free(fill);
    }
    while (q.size > 0) {
        int d, e;
        nav_LHPop(&q, &d, &e);
        if (d > h->dist[e]) continue;
        // Relax predecessors f: walking f -> e costs the edge plus entering e's cell
        const NavEntrance* E = &w->entranceList[e];
        int enterE = nav_HFlowExtra(w, h, E->x, E->y, E->z);
        for (int k = inStart[e]; k < inStart[e + 1]; k++) {
            const NavGraphEdge* ed = inEdge[k];
            int f = ed->from, nd = d + ed->cost + enterE + nav_LinkExtra(w, ed);
            if (nd < h->dist[f]) { h->dist[f] = nd; h->nextEnt[f] = e; h->viaChunk[f] = ed->chunk; nav_LHPush(&q, nd, f); }
        }
    }
    free(inStart); free(inEdge);
    nav_LHFree(&q);
    h->valid = true;
    h->abstractMs = (NavHostTime() - t0) * 1000.0;
}

NavHFlow* NavHFlowCreate(NavWorld* w, NavPoint goal) {
    NavHFlow* h = calloc(1, sizeof(NavHFlow));
    if (!h) return NULL;
    h->goal = goal;
    h->buildBudget = -1;
    h->graphVersion = w->graphVersion - 1;   // force a build
    nav_HFlowRebuild(w, h);
    return h;
}

void NavHFlowDestroy(NavHFlow* h) {
    if (!h) return;
    nav_HFlowClearChunks(h);
    free(h->chunks); free(h->dist); free(h->nextEnt); free(h->viaChunk); free(h->fresh);
    free(h);
}

static inline int nav_ChunkFlowIndex(const NavChunkFlow* cf, NavPoint p) {
    int lx = p.x - cf->minX, ly = p.y - cf->minY;
    if (p.z != cf->z || lx < 0 || ly < 0 || lx >= cf->w || ly >= cf->h) return -1;
    return ly * cf->w + lx;
}

static NavChunkFlow* nav_GetChunkFlow(NavWorld* w, NavHFlow* h, int chunk);
static void nav_ChunkFlowAddDep(NavChunkFlow* cf, int chunk);
static int nav_HFlowSeedValue(NavWorld* w, NavHFlow* h, int e, NavChunkFlow* building);

static NavChunkFlow* nav_BuildChunkFlow(NavWorld* w, NavHFlow* h, int chunk) {
    int perLevel = w->chunksX * w->chunksY;
    int z = chunk / perLevel, xy = chunk % perLevel;
    int ccx = xy % w->chunksX, ccy = xy / w->chunksX;
    int minX = ccx * w->chunkW - 1, minY = ccy * w->chunkH - 1;
    int maxX = (ccx + 1) * w->chunkW + 1, maxY = (ccy + 1) * w->chunkH + 1;
    if (minX < 0) minX = 0;
    if (minY < 0) minY = 0;
    if (maxX > w->width) maxX = w->width;
    if (maxY > w->height) maxY = w->height;

    NavChunkFlow* cf = calloc(1, sizeof(NavChunkFlow));
    if (!cf) return NULL;
    cf->minX = minX; cf->minY = minY; cf->w = maxX - minX; cf->h = maxY - minY; cf->z = z;
    int cells = cf->w * cf->h;
    cf->cost = malloc((size_t)cells * sizeof(int));
    cf->next = malloc((size_t)cells * sizeof(int));
    cf->seed = malloc((size_t)cells * sizeof(int));
    if (!cf->cost || !cf->next || !cf->seed) { nav_FreeChunkFlow(cf); return NULL; }
    for (int i = 0; i < cells; i++) { cf->cost[i] = NAV_FLOW_UNREACHABLE; cf->next[i] = -1; cf->seed[i] = -1; }

    nav_LazyHeap q = {0};
    // Seeds: exits of this chunk (see rule above)
    // (any entrance inside the region: neighbours' ladder/ramp/border cells on the
    // ring can be where the route leaves, see nav_EdgeChunksOfCell)
    // Candidates: the entrances of the 3x3 chunks around this one (the region is the
    // chunk + a 1-cell ring), from the per-chunk index, in ascending index order so
    // ties pick the same seed as a full scan. A full scan per chunk field costs
    // O(all entrances); a walk can build tens of thousands of fields on a big map.
    // (heap, not stack: building a field can recurse into neighbour fields deeply)
    int* cand = malloc(9 * NAV_MAX_ENTRANCES_PER_CHUNK * sizeof(int));
    int candNum = cand ? nav_EntrancesAround(w, chunk, cand) : -1;
    int scanNum = candNum >= 0 ? candNum : w->entranceNum;
    for (int ci = 0; ci < scanNum; ci++) {
        int e = candNum >= 0 ? cand[ci] : ci;
        if (h->dist[e] == NAV_FLOW_UNREACHABLE) continue;
        const NavEntrance* E = &w->entranceList[e];
        if (E->z != z) continue;
        // The route continues inside this chunk (the edge to the next entrance was
        // computed here, or the goal is here): not an exit. Two entrances on the same
        // chunk border share both chunks, so membership alone can't tell.
        if (h->viaChunk[e] == chunk) continue;
        int lx = E->x - minX, ly = E->y - minY;
        if (lx < 0 || ly < 0 || lx >= cf->w || ly >= cf->h) continue;
        int i = ly * cf->w + lx;
        // Seed with what continuing from this exit REALLY costs (the next chunk's field,
        // or the ladder/ramp step + the far side), not the abstract estimate: graph
        // edges are symmetric while walking costs aren't, and an underestimated exit
        // would pull agents back after they step across.
        int v = nav_HFlowSeedValue(w, h, e, cf);
        if (v != NAV_FLOW_UNREACHABLE && v < cf->cost[i]) { cf->cost[i] = v; cf->seed[i] = e; nav_LHPush(&q, v, i); }
    }
    free(cand);
    if (chunk == h->goalChunk) {
        int i = (h->goal.y - minY) * cf->w + (h->goal.x - minX);
        cf->cost[i] = 0; cf->seed[i] = -2; nav_LHPush(&q, 0, i);
    }

    // Fields may route through the 1-cell ring too (the HPA* edges they realise were
    // computed over the same area). Neighbouring fields can disagree about ring cells;
    // NavHFlowNext resolves that by always following the lowest-cost covering field.

    // Reverse Dijkstra inside the region, same-level edges only (cross-level is abstract)
    const int* dx = w->eightDir ? nav_kDx8 : nav_kDx4;
    const int* dy = w->eightDir ? nav_kDy8 : nav_kDy4;
    int numDirs = w->eightDir ? 8 : 4;
    while (q.size > 0) {
        int d, i;
        nav_LHPop(&q, &d, &i);
        if (d > cf->cost[i]) continue;
        int x = minX + i % cf->w, y = minY + i / cf->w;
        int cellCost = NavMoveCost(NAV_HOSTP, x, y, z) + nav_HFlowExtra(w, h, x, y, z);
        for (int k = 0; k < numDirs; k++) {
            int px = x - dx[k], py = y - dy[k];
            if (px < minX || py < minY || px >= maxX || py >= maxY) continue;
            int pi = (py - minY) * cf->w + (px - minX);
            if (!NavIsWalkable(NAV_HOSTP, px, py, z)) continue;
            if (w->eightDir && dx[k] != 0 && dy[k] != 0) {
                if (!NavIsWalkable(NAV_HOSTP, x, py, z) || !NavIsWalkable(NAV_HOSTP, px, y, z)) continue;
            }
            if (!NavCanEnterFrom(NAV_HOSTP, x, y, z, px, py)) continue;
            int base = (dx[k] != 0 && dy[k] != 0) ? 14 : 10;
            int nd = d + (base * cellCost) / 10;
            if (nd < cf->cost[pi]) { cf->cost[pi] = nd; cf->next[pi] = i; cf->seed[pi] = -1; nav_LHPush(&q, nd, pi); }
        }
    }
    nav_LHFree(&q);
    h->chunksBuilt++;
    return cf;
}

static void nav_HFlowNoteFresh(NavHFlow* h, int chunk) {
    if (h->freshNum == h->freshCap) {
        int cap = h->freshCap ? h->freshCap * 2 : 64;
        int* nf = realloc(h->fresh, (size_t)cap * sizeof(int));
        if (!nf) return;
        h->fresh = nf; h->freshCap = cap;
    }
    h->fresh[h->freshNum++] = chunk;
}

// Fields built during one outer request may have read fields that were mid-build
// (left out) or provisional. Settle them: a worklist rebuilds a provisional field,
// and if its values changed, the fields that read it. Values only ever drop (more
// and lower seeds), so this terminates; only real changes propagate. Fields built
// by earlier requests never read these, so they're unaffected.
static void nav_HFlowSettle(NavWorld* w, NavHFlow* h) {
    int n = h->chunkCount;
    int* head = malloc((size_t)n * sizeof(int));       // dependents: linked lists
    bool* inQ = calloc((size_t)n, sizeof(bool));
    int edgeCap = 256, edgeNum = 0, qCap = 64, qHead = 0, qTail = 0;
    int* edgeTo = malloc((size_t)edgeCap * sizeof(int));
    int* edgeNext = malloc((size_t)edgeCap * sizeof(int));
    int* queue = malloc((size_t)qCap * sizeof(int));
    if (!head || !inQ || !edgeTo || !edgeNext || !queue) goto done;
    for (int c = 0; c < n; c++) head[c] = -1;

    // register fresh fields (the list grows while we rebuild: new lazy builds)
    int seen = 0;
    for (;;) {
        for (; seen < h->freshNum; seen++) {
            int c = h->fresh[seen];
            NavChunkFlow* cf = h->chunks[c];
            if (!cf || cf == &nav_ChunkFlowBuilding) continue;
            for (int k = 0; k < cf->depNum; k++) {
                if (edgeNum == edgeCap) {
                    edgeCap *= 2;
                    int* a = realloc(edgeTo, (size_t)edgeCap * sizeof(int));
                    if (a) edgeTo = a;
                    int* b = realloc(edgeNext, (size_t)edgeCap * sizeof(int));
                    if (b) edgeNext = b;
                    if (!a || !b) goto done;
                }
                int d = cf->deps[k];
                edgeTo[edgeNum] = c; edgeNext[edgeNum] = head[d]; head[d] = edgeNum++;
            }
            if (cf->provisional && !inQ[c]) {
                if (qTail - qHead == qCap) break;   // grown below
                inQ[c] = true;
                queue[qTail % qCap] = c; qTail++;
            }
        }
        if (qHead == qTail && seen == h->freshNum) break;
        if (qTail - qHead == qCap) {               // grow the ring buffer
            int* nq = malloc((size_t)qCap * 2 * sizeof(int));
            if (!nq) goto done;
            for (int k = 0; k < qTail - qHead; k++) nq[k] = queue[(qHead + k) % qCap];
            qTail -= qHead; qHead = 0;
            free(queue); queue = nq; qCap *= 2;
            continue;
        }
        if (qHead == qTail) continue;
        if (h->overLimit) break;                   // out of budget: the fields get dropped anyway
        int c = queue[qHead % qCap]; qHead++;
        inQ[c] = false;
        NavChunkFlow* old = h->chunks[c];
        h->buildDepth++;
        NavChunkFlow* nf = nav_BuildChunkFlow(w, h, c);   // old stays visible meanwhile
        h->buildDepth--;
        if (!nf) continue;
        bool changed = memcmp(nf->cost, old->cost, (size_t)nf->w * nf->h * sizeof(int)) != 0;
        // new dependencies (rebuilds can read chunks the first build couldn't)
        for (int k = 0; k < nf->depNum; k++) {
            bool known = false;
            for (int m = 0; m < old->depNum && !known; m++) known = old->deps[m] == nf->deps[k];
            if (known) continue;
            if (edgeNum == edgeCap) {
                edgeCap *= 2;
                int* a = realloc(edgeTo, (size_t)edgeCap * sizeof(int));
                if (a) edgeTo = a;
                int* b = realloc(edgeNext, (size_t)edgeCap * sizeof(int));
                if (b) edgeNext = b;
                if (!a || !b) goto done;
            }
            int d = nf->deps[k];
            edgeTo[edgeNum] = c; edgeNext[edgeNum] = head[d]; head[d] = edgeNum++;
        }
        h->chunks[c] = nf;
        nav_FreeChunkFlow(old);
        if (!changed) continue;
        for (int e = head[c]; e >= 0; e = edgeNext[e]) {
            int x = edgeTo[e];
            if (inQ[x]) continue;
            if (qTail - qHead == qCap) {
                int* nq = malloc((size_t)qCap * 2 * sizeof(int));
                if (!nq) goto done;
                for (int k = 0; k < qTail - qHead; k++) nq[k] = queue[(qHead + k) % qCap];
                qTail -= qHead; qHead = 0;
                free(queue); queue = nq; qCap *= 2;
            }
            inQ[x] = true;
            queue[qTail % qCap] = x; qTail++;
        }
    }
done:
    for (int k = 0; k < h->freshNum; k++) {
        NavChunkFlow* cf = h->chunks[h->fresh[k]];
        if (cf && cf != &nav_ChunkFlowBuilding) cf->provisional = false;
    }
    h->freshNum = 0;
    free(head); free(inQ); free(edgeTo); free(edgeNext); free(queue);
}

static NavChunkFlow* nav_GetChunkFlow(NavWorld* w, NavHFlow* h, int chunk) {
    if (chunk < 0 || chunk >= h->chunkCount) return NULL;
    if (h->chunks[chunk] == &nav_ChunkFlowBuilding) return NULL;
    if (h->chunks[chunk]) return h->chunks[chunk];
    if (h->buildBudget == 0) { h->overLimit = true; return NULL; }
    if (h->buildBudget > 0) h->buildBudget--;
    h->buildDepth++;
    h->chunks[chunk] = &nav_ChunkFlowBuilding;
    h->chunks[chunk] = nav_BuildChunkFlow(w, h, chunk);
    h->buildDepth--;
    if (h->chunks[chunk]) nav_HFlowNoteFresh(h, chunk);
    if (h->buildDepth == 0) nav_HFlowSettle(w, h);
    return h->chunks[chunk];
}

static void nav_ChunkFlowAddDep(NavChunkFlow* cf, int chunk) {
    for (int k = 0; k < cf->depNum; k++) if (cf->deps[k] == chunk) return;
    if (cf->depNum == cf->depCap) {
        int cap = cf->depCap ? cf->depCap * 2 : 8;
        int* nd = realloc(cf->deps, (size_t)cap * sizeof(int));
        if (!nd) return;
        cf->deps = nd; cf->depCap = cap;
    }
    cf->deps[cf->depNum++] = chunk;
}

// Cost to the goal from entrance e when it is an exit of the current chunk:
// taken from the field that continues the route, so all fields agree.
// NAV_FLOW_UNREACHABLE = unknown: the exit is left out rather than guessed, since an
// underestimate (e.g. the symmetric abstract dist) makes a trap agents can't leave.
static int nav_HFlowSeedValue(NavWorld* w, NavHFlow* h, int e, NavChunkFlow* building) {
    const NavEntrance* E = &w->entranceList[e];
    int f = h->nextEnt[e];
    int target;                                    // chunk whose field continues from e
    NavPoint at = {E->x, E->y, E->z};              // cell whose value in that field we need
    int step = 0;
    if (f == -1) {
        target = h->goalChunk;
    } else {
        const NavEntrance* F = &w->entranceList[f];
        if (F->z != E->z) {
            // Ladder/ramp: the step onto F (A* step rules) + F's value on its own level
            int c = NavMoveCost(NAV_HOSTP, F->x, F->y, F->z) + nav_HFlowExtra(w, h, F->x, F->y, F->z);
            step = ((F->x == E->x && F->y == E->y) ? c : (14 * c) / 10)
                 + NAV_CLIMB_EXTRA(NAV_HOSTP, E->x, E->y, E->z, F->z > E->z ? 1 : -1);
            target = nav_GetChunk(w, F->x, F->y, F->z);
            at = (NavPoint){F->x, F->y, F->z};
        } else {
            target = h->viaChunk[e];
        }
    }
    bool pending = target >= 0 && target < h->chunkCount && h->chunks[target] == &nav_ChunkFlowBuilding;
    if (target >= 0 && target < h->chunkCount) nav_ChunkFlowAddDep(building, target);
    NavChunkFlow* cf = nav_GetChunkFlow(w, h, target);
    if (!cf) { if (pending) building->provisional = true; return NAV_FLOW_UNREACHABLE; }
    if (cf->provisional) building->provisional = true;   // read a value that may still drop
    int i = nav_ChunkFlowIndex(cf, at);
    if (i < 0 || cf->cost[i] == NAV_FLOW_UNREACHABLE) return NAV_FLOW_UNREACHABLE;
    return step + cf->cost[i];
}

void NavHFlowInvalidate(NavHFlow* h) {
    if (h) h->stale = true;
}

static bool nav_HFlowPrepare(NavWorld* w, NavHFlow* h, NavPoint p) {
    if (h->stale || h->graphVersion != w->graphVersion) nav_HFlowRebuild(w, h);
    return h->valid && nav_InBounds(w, p);
}

// All chunk fields whose region (chunk + 1-cell ring) contains p: its own chunk,
// plus neighbours when p lies on their ring. Built lazily.
static int nav_HFlowCovering(NavWorld* w, NavHFlow* h, NavPoint p, NavChunkFlow** out) {
    int n = 0;
    int cx = p.x / w->chunkW, cy = p.y / w->chunkH;
    for (int dcy = -1; dcy <= 1; dcy++)
        for (int dcx = -1; dcx <= 1; dcx++) {
            int ncx = cx + dcx, ncy = cy + dcy;
            if (ncx < 0 || ncy < 0 || ncx >= w->chunksX || ncy >= w->chunksY) continue;
            // region of the neighbour: [ncx*cw - 1, (ncx+1)*cw + 1)
            if (p.x < ncx * w->chunkW - 1 || p.x >= (ncx + 1) * w->chunkW + 1) continue;
            if (p.y < ncy * w->chunkH - 1 || p.y >= (ncy + 1) * w->chunkH + 1) continue;
            NavChunkFlow* cf = nav_GetChunkFlow(w, h, p.z * (w->chunksX * w->chunksY) + ncy * w->chunksX + ncx);
            if (cf) out[n++] = cf;
        }
    return n;
}

static inline NavPoint nav_ChunkFlowPoint(const NavChunkFlow* cf, int i) {
    return (NavPoint){cf->minX + i % cf->w, cf->minY + i / cf->w, cf->z};
}

// Next step from p using the local fields (no rescue). Follows the covering field
// with the lowest cost at p: each step strictly lowers that minimum, so agents
// can't cycle between fields that disagree about border cells.
static bool nav_HFlowFieldStep(NavWorld* w, NavHFlow* h, NavPoint p, NavPoint* out) {
    NavChunkFlow* cov[9];
    int n = nav_HFlowCovering(w, h, p, cov);
    NavChunkFlow* best = NULL;
    int bestIdx = -1, bestCost = NAV_FLOW_UNREACHABLE;
    bool bestHasNext = false;
    for (int k = 0; k < n; k++) {
        int i = nav_ChunkFlowIndex(cov[k], p);
        if (i < 0) continue;
        int c = cov[k]->cost[i];
        if (c == NAV_FLOW_UNREACHABLE) continue;
        bool hasNext = cov[k]->next[i] >= 0;
        if (c < bestCost || (c == bestCost && hasNext && !bestHasNext)) {
            best = cov[k]; bestIdx = i; bestCost = c; bestHasNext = hasNext;
        }
    }
    if (!best) return false;
    if (bestHasNext) { *out = nav_ChunkFlowPoint(best, best->next[bestIdx]); return true; }

    // Lowest-cost answer is an exit (seed): take the ladder/ramp link, or step on via
    // the chunk shared with the next entrance. Entrances can share a cell (a ladder
    // foot on a chunk border), so the hand-over may land on another seed: follow on.
    NavChunkFlow* cur = best;
    int idx = bestIdx;
    for (int hop = 0; hop < 8; hop++) {
        int e = cur->seed[idx];
        if (e < 0) return false;                   // goal
        int f = h->nextEnt[e];
        int shared;
        if (f == -1) {
            shared = h->goalChunk;
        } else {
            const NavEntrance* F = &w->entranceList[f];
            if (F->z != w->entranceList[e].z) { *out = (NavPoint){F->x, F->y, F->z}; return true; }
            shared = h->viaChunk[e];
        }
        NavChunkFlow* cf2 = nav_GetChunkFlow(w, h, shared);
        if (!cf2 || cf2 == cur) return false;
        int j = nav_ChunkFlowIndex(cf2, p);
        if (j < 0 || cf2->cost[j] == NAV_FLOW_UNREACHABLE) return false;
        if (cf2->next[j] >= 0) { *out = nav_ChunkFlowPoint(cf2, cf2->next[j]); return true; }
        cur = cf2; idx = j;
    }
    return false;
}

// NavHFlowCostAt without the per-call buildBudget bookkeeping (used inside a call)
static int nav_HFlowCostAt(NavWorld* w, NavHFlow* h, NavPoint p) {
    if (!nav_HFlowPrepare(w, h, p)) return NAV_FLOW_UNREACHABLE;
    NavChunkFlow* cov[9];
    int n = nav_HFlowCovering(w, h, p, cov), best = NAV_FLOW_UNREACHABLE;
    for (int k = 0; k < n; k++) {
        int i = nav_ChunkFlowIndex(cov[k], p);
        if (i >= 0 && cov[k]->cost[i] < best) best = cov[k]->cost[i];
    }
    return best;
}

// Rare fallback when p's local fields have no answer (a pocket that only reaches
// its chunk's exits through the neighbour's ring, or a failed hand-over): forward
// Dijkstra within the surrounding 3x3 chunks to the cell q minimising
// travel(p,q) + cost(q) among cells whose own field works; step toward it.
static bool nav_HFlowRescue(NavWorld* w, NavHFlow* h, NavPoint p, NavPoint* out) {
    int minX = (p.x / w->chunkW - 1) * w->chunkW, minY = (p.y / w->chunkH - 1) * w->chunkH;
    int maxX = minX + 3 * w->chunkW, maxY = minY + 3 * w->chunkH;
    if (minX < 0) minX = 0;
    if (minY < 0) minY = 0;
    if (maxX > w->width) maxX = w->width;
    if (maxY > w->height) maxY = w->height;
    int W = maxX - minX, Hh = maxY - minY, cells = W * Hh;
    int* g = malloc((size_t)cells * sizeof(int));
    int* parent = malloc((size_t)cells * sizeof(int));
    if (!g || !parent) { free(g); free(parent); return false; }
    for (int i = 0; i < cells; i++) { g[i] = NAV_FLOW_UNREACHABLE; parent[i] = -1; }
    const int* dx = w->eightDir ? nav_kDx8 : nav_kDx4;
    const int* dy = w->eightDir ? nav_kDy8 : nav_kDy4;
    int numDirs = w->eightDir ? 8 : 4;

    nav_LazyHeap q = {0};
    int s = (p.y - minY) * W + (p.x - minX);
    g[s] = 0;
    nav_LHPush(&q, 0, s);
    int best = -1, bestTotal = NAV_FLOW_UNREACHABLE;
    while (q.size > 0) {
        int d, i;
        nav_LHPop(&q, &d, &i);
        if (d > g[i] || d >= bestTotal) continue;
        NavPoint c = {minX + i % W, minY + i / W, p.z};
        if (i != s) {
            NavPoint tmp;
            int cc = nav_HFlowCostAt(w, h, c);
            bool isGoal = c.x == h->goal.x && c.y == h->goal.y && c.z == h->goal.z;
            if (cc != NAV_FLOW_UNREACHABLE && (isGoal || nav_HFlowFieldStep(w, h, c, &tmp))) {
                if (d + cc < bestTotal) { bestTotal = d + cc; best = i; }
                continue;
            }
        }
        for (int k = 0; k < numDirs; k++) {
            int nx = c.x + dx[k], ny = c.y + dy[k];
            if (nx < minX || ny < minY || nx >= maxX || ny >= maxY) continue;
            if (!NavIsWalkable(NAV_HOSTP, nx, ny, p.z)) continue;
            if (w->eightDir && dx[k] != 0 && dy[k] != 0) {
                if (!NavIsWalkable(NAV_HOSTP, nx, c.y, p.z) || !NavIsWalkable(NAV_HOSTP, c.x, ny, p.z)) continue;
            }
            if (!NavCanEnterFrom(NAV_HOSTP, nx, ny, p.z, c.x, c.y)) continue;
            int base = (dx[k] != 0 && dy[k] != 0) ? 14 : 10;
            int ni = (ny - minY) * W + (nx - minX);
            int nd = d + (base * (NavMoveCost(NAV_HOSTP, nx, ny, p.z) + nav_HFlowExtra(w, h, nx, ny, p.z))) / 10;
            if (nd < g[ni]) { g[ni] = nd; parent[ni] = i; nav_LHPush(&q, nd, ni); }
        }
    }
    nav_LHFree(&q);
    bool ok = false;
    if (best >= 0) {
        int i = best;
        while (parent[i] != s && parent[i] >= 0) i = parent[i];   // first step from p
        *out = (NavPoint){minX + i % W, minY + i / W, p.z};
        ok = true;
    }
    free(g); free(parent);
    return ok;
}

// buildBudget bookkeeping around one public call. Out of budget, fields built during
// the call may have missed seeds (unbuilt neighbours read as unreachable): drop them all.
static void nav_HFlowCallBegin(NavHFlow* h) { h->overLimit = false; }
static bool nav_HFlowCallOver(NavHFlow* h) {
    if (!h->overLimit) return false;
    h->stale = true;
    return true;
}

bool NavHFlowNext(NavWorld* w, NavHFlow* h, NavPoint p, NavPoint* out) {
    nav_HFlowCallBegin(h);
    if (!nav_HFlowPrepare(w, h, p)) return false;
    if (p.x == h->goal.x && p.y == h->goal.y && p.z == h->goal.z) return false;
    bool ok = nav_HFlowFieldStep(w, h, p, out);
    if (nav_HFlowCallOver(h)) return false;
    if (ok) return true;
    ok = nav_HFlowRescue(w, h, p, out);
    if (nav_HFlowCallOver(h)) return false;
    return ok;
}

int NavHFlowCostAt(NavWorld* w, NavHFlow* h, NavPoint p) {
    nav_HFlowCallBegin(h);
    int best = nav_HFlowCostAt(w, h, p);
    if (nav_HFlowCallOver(h)) return NAV_FLOW_UNREACHABLE;
    return best;
}

size_t NavHFlowMemoryUsage(const NavHFlow* h) {
    size_t bytes = sizeof(NavHFlow) + (size_t)h->chunkCount * sizeof(NavChunkFlow*)
                 + (size_t)(h->entNum > 0 ? h->entNum : 1) * 3 * sizeof(int);
    for (int i = 0; i < h->chunkCount; i++)
        if (h->chunks[i]) bytes += sizeof(NavChunkFlow) + (size_t)h->chunks[i]->w * h->chunks[i]->h * 3 * sizeof(int);
    return bytes;
}

#undef NAV_HOSTP
