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
#define NAV_ENTRANCE_HASH_SIZE 32768   // power of 2, > 2x NAV_MAX_ENTRANCES
#define NAV_ABS_CAP (NAV_MAX_ENTRANCES + 2)
#define NAV_CONNECT_MAX 128            // entrances considered per chunk in a query/full build

#define NAV_HOSTP (w->host)

static const int nav_kDx4[4] = {0, 1, 0, -1};
static const int nav_kDy4[4] = {-1, 0, 1, 0};
static const int nav_kDx8[8] = {0, 1, 1, 1, 0, -1, -1, -1};
static const int nav_kDy8[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

// =============================================================================
// Lifecycle
// =============================================================================

static void nav_NavFreeInternals(NavWorld* w) {
    free(w->entranceList); free(w->edgeList); free(w->ladderList); free(w->rampList);
    free(w->chunkDirtyFlags);
    free(w->adjList); free(w->adjListCount);
    free(w->absNodes); free(w->absPath); free(w->absHeap); free(w->absHeapPos);
    free(w->chunkNodes); free(w->chunkHeapPos); free(w->chunkHeap);
    free(w->entHash); free(w->oldToNew); free(w->oldEntrances);
    free(w->chunkEnts); free(w->chunkEntCount); free(w->affected);
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

    w->entranceList   = calloc(NAV_MAX_ENTRANCES, sizeof(NavEntrance));
    w->edgeList       = calloc(NAV_MAX_EDGES, sizeof(NavGraphEdge));
    w->ladderList     = calloc(NAV_MAX_LADDERS, sizeof(NavLadderLink));
    w->rampList       = calloc(NAV_MAX_RAMP_LINKS, sizeof(NavRampLink));
    w->chunkDirtyFlags= calloc(chunks, sizeof(bool));
    w->adjList        = calloc(NAV_MAX_ENTRANCES, sizeof(*w->adjList));
    w->adjListCount   = calloc(NAV_MAX_ENTRANCES, sizeof(int));
    w->absNodes       = calloc(NAV_ABS_CAP, sizeof(NavAbstractNode));
    w->absPath        = calloc(NAV_ABS_CAP, sizeof(int));
    w->absHeap        = calloc(NAV_ABS_CAP, sizeof(int));
    w->absHeapPos     = calloc(NAV_ABS_CAP, sizeof(int));
    w->chunkNodes     = calloc(layer, sizeof(NavChunkNode));
    w->chunkHeapPos   = calloc(layer, sizeof(int));
    w->chunkHeap      = calloc(layer, sizeof(int));
    w->chunkHeapCap   = (int)layer;
    w->entHash        = calloc(NAV_ENTRANCE_HASH_SIZE, sizeof(NavHashEntry));
    w->oldToNew       = calloc(NAV_MAX_ENTRANCES, sizeof(int));
    w->oldEntrances   = calloc(NAV_MAX_ENTRANCES, sizeof(NavEntrance));
    w->chunkEnts      = calloc(chunks, sizeof(*w->chunkEnts));
    w->chunkEntCount  = calloc(chunks, sizeof(int));
    w->affected       = calloc(chunks, sizeof(bool));
    w->tempPathCap    = (int)layer;
    w->tempPath       = calloc(layer, sizeof(NavPoint));
    // Full-grid A* buffers (W*H*D) are allocated lazily on the first A* query
    w->aG = w->aF = w->aParent = w->aHeap = w->aHeapIdx = NULL;
    w->aState = NULL;
    w->aGen = NULL;
    w->aHeapCount = 0;
    w->aCurGen = 0;

    w->entranceNum = w->edgeNum = w->ladderNum = w->rampNum = w->rampCellNum = 0;
    w->oldEntranceNum = 0;
    w->absPathLen = 0;
    w->dirty = true;
    w->hpaDirty = true;

    return w->entranceList && w->edgeList && w->ladderList && w->rampList && w->chunkDirtyFlags
        && w->adjList && w->adjListCount && w->absNodes && w->absPath && w->absHeap && w->absHeapPos
        && w->chunkNodes && w->chunkHeapPos && w->chunkHeap && w->entHash && w->oldToNew
        && w->oldEntrances && w->chunkEnts && w->chunkEntCount && w->affected && w->tempPath;
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
    memset(w, 0, sizeof(*w));
    w->host = host;
    w->eightDir = eightDir;
    w->lastCost = -1;
    return nav_NavAllocInternals(w, width, height, depth, chunkW, chunkH);
}

size_t NavMemoryUsage(const NavWorld* w) {
    size_t layer = (size_t)w->width * w->height;
    size_t cells = layer * w->depth;
    size_t chunks = (size_t)w->depth * w->chunksX * w->chunksY;
    return sizeof(NavWorld)
        + NAV_MAX_ENTRANCES * (sizeof(NavEntrance) * 2 + sizeof(*w->adjList) + sizeof(int) * 2)
        + NAV_MAX_EDGES * sizeof(NavGraphEdge)
        + NAV_MAX_LADDERS * sizeof(NavLadderLink)
        + NAV_MAX_RAMP_LINKS * sizeof(NavRampLink)
        + NAV_ABS_CAP * (sizeof(NavAbstractNode) + sizeof(int) * 3)
        + NAV_ENTRANCE_HASH_SIZE * sizeof(NavHashEntry)
        + chunks * (sizeof(bool) * 2 + sizeof(*w->chunkEnts) + sizeof(int))
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

static int nav_Heuristic3D(const NavWorld* w, int x0, int y0, int z0, int x1, int y1, int z1) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int dz = abs(z1 - z0);
    if (w->eightDir) {
        int maxXY = (dx > dy) ? dx : dy;
        int minXY = (dx < dy) ? dx : dy;
        return (maxXY - minXY) * NAV_MIN_CELL_COST + minXY * ((14 * NAV_MIN_CELL_COST) / 10)
             + dz * NAV_MIN_CELL_COST;
    }
    return (dx + dy + dz) * NAV_MIN_CELL_COST;
}

// =============================================================================
// Entrance hash (position -> entrance index) for incremental updates
// =============================================================================

static inline int nav_HashPosition(int x, int y, int z) {
    unsigned int h = (unsigned int)(x * 73856093) ^ (unsigned int)(y * 19349663) ^ (unsigned int)(z * 83492791);
    return (int)(h & (NAV_ENTRANCE_HASH_SIZE - 1));
}

static void nav_BuildEntranceHash(NavWorld* w) {
    for (int i = 0; i < NAV_ENTRANCE_HASH_SIZE; i++) w->entHash[i].index = -1;
    for (int i = 0; i < w->entranceNum; i++) {
        const NavEntrance* e = &w->entranceList[i];
        int h = nav_HashPosition(e->x, e->y, e->z);
        while (w->entHash[h].index >= 0) h = (h + 1) & (NAV_ENTRANCE_HASH_SIZE - 1);
        w->entHash[h] = (NavHashEntry){e->x, e->y, e->z, i, e->chunk1 == e->chunk2};
    }
}

static int nav_HashLookupEntrance(const NavWorld* w, int x, int y, int z, bool isLadder) {
    int h = nav_HashPosition(x, y, z);
    int start = h;
    while (w->entHash[h].index >= 0) {
        const NavHashEntry* e = &w->entHash[h];
        if (e->x == x && e->y == y && e->z == z && e->isLadder == isLadder) return e->index;
        h = (h + 1) & (NAV_ENTRANCE_HASH_SIZE - 1);
        if (h == start) break;
    }
    return -1;
}

// Chunk -> entrance index list
static void nav_BuildChunkEntranceIndex(NavWorld* w) {
    int totalChunks = w->depth * w->chunksX * w->chunksY;
    memset(w->chunkEntCount, 0, (size_t)totalChunks * sizeof(int));
    for (int i = 0; i < w->entranceNum; i++) {
        int c1 = w->entranceList[i].chunk1;
        int c2 = w->entranceList[i].chunk2;
        if (w->chunkEntCount[c1] < NAV_MAX_ENTRANCES_PER_CHUNK)
            w->chunkEnts[c1][w->chunkEntCount[c1]++] = i;
        if (c2 != c1 && w->chunkEntCount[c2] < NAV_MAX_ENTRANCES_PER_CHUNK)
            w->chunkEnts[c2][w->chunkEntCount[c2]++] = i;
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
    if (w->absHeapSize >= NAV_ABS_CAP) return;
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
static void nav_ChunkExpand(NavWorld* w, int bestX, int bestY, int sz, int gx, int gy,
                        int minX, int minY, int maxX, int maxY, bool dijkstra, bool trackParent) {
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

        int baseCost = (dx[i] != 0 && dy[i] != 0) ? 14 : 10;
        int ng = best->g + (baseCost * NavMoveCost(NAV_HOSTP, nx, ny, sz)) / 10;
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
        nav_ChunkExpand(w, bx, by, sz, gx, gy, minX, minY, maxX, maxY, false, false);
    }
    return -1;
}

int NavChunkCostsMulti(NavWorld* w, int sx, int sy, int sz,
                       const int* targetX, const int* targetY, int* outCosts, int numTargets,
                       int minX, int minY, int maxX, int maxY) {
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
        nav_ChunkExpand(w, bx, by, sz, 0, 0, minX, minY, maxX, maxY, true, false);
    }
    return found;
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
        nav_ChunkExpand(w, bx, by, sz, gx, gy, minX, minY, maxX, maxY, false, true);
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
    w->chunkDirtyFlags[nav_ChunkFlagIndex(w, cx, cy, z)] = true;

    int extraZ[4];
    int count = NavAffectedZ(NAV_HOSTP, z, extraZ);
    for (int i = 0; i < count && i < 4; i++) {
        if (extraZ[i] >= 0 && extraZ[i] < w->depth)
            w->chunkDirtyFlags[nav_ChunkFlagIndex(w, cx, cy, extraZ[i])] = true;
    }
    w->dirty = true;
    w->hpaDirty = true;
    NavOnChunkDirty(NAV_HOSTP, x, y, z);
}

// =============================================================================
// HPA*: entrances
// =============================================================================

static void nav_AddEntrance(NavWorld* w, int x, int y, int z, int chunk1, int chunk2) {
    if (w->entranceNum < NAV_MAX_ENTRANCES)
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
    if (w->entranceNum >= NAV_MAX_ENTRANCES) return -1;
    int chunk = z * (w->chunksX * w->chunksY) + (y / w->chunkH) * w->chunksX + (x / w->chunkW);
    w->entranceList[w->entranceNum] = (NavEntrance){x, y, z, chunk, chunk};
    return w->entranceNum++;
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
            int chunk1 = z * perLevel + cy * w->chunksX + cx;
            int chunk2 = z * perLevel + (cy + 1) * w->chunksX + cx;
            int runStart = -1;
            int span = w->chunkW;
            if (startX + span > w->width) span = w->width - startX;
            for (int i = 0; i < span; i++) {
                int x = startX + i;
                bool open = NavIsWalkable(NAV_HOSTP, x, borderY - 1, z) && NavIsWalkable(NAV_HOSTP, x, borderY, z);
                if (open && runStart < 0) runStart = i;
                else if (!open && runStart >= 0) {
                    nav_AddEntrancesForRun(w, startX + runStart, borderY, z, i - runStart, true, chunk1, chunk2);
                    runStart = -1;
                }
            }
            if (runStart >= 0)
                nav_AddEntrancesForRun(w, startX + runStart, borderY, z, span - runStart, true, chunk1, chunk2);
        }
    }
    // Vertical borders (between columns of chunks)
    for (int cy = 0; cy < w->chunksY; cy++) {
        for (int cx = 0; cx < w->chunksX - 1; cx++) {
            if (onlyAffected && !aff[nav_ChunkFlagIndex(w, cx, cy, z)] && !aff[nav_ChunkFlagIndex(w, cx + 1, cy, z)]) continue;
            int borderX = (cx + 1) * w->chunkW;
            int startY = cy * w->chunkH;
            int chunk1 = z * perLevel + cy * w->chunksX + cx;
            int chunk2 = z * perLevel + cy * w->chunksX + (cx + 1);
            int runStart = -1;
            int span = w->chunkH;
            if (startY + span > w->height) span = w->height - startY;
            for (int i = 0; i < span; i++) {
                int y = startY + i;
                bool open = NavIsWalkable(NAV_HOSTP, borderX - 1, y, z) && NavIsWalkable(NAV_HOSTP, borderX, y, z);
                if (open && runStart < 0) runStart = i;
                else if (!open && runStart >= 0) {
                    nav_AddEntrancesForRun(w, borderX, startY + runStart, z, i - runStart, false, chunk1, chunk2);
                    runStart = -1;
                }
            }
            if (runStart >= 0)
                nav_AddEntrancesForRun(w, borderX, startY + runStart, z, span - runStart, false, chunk1, chunk2);
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
                if (NavCanClimbUp(NAV_HOSTP, x, y, z) && w->ladderNum < NAV_MAX_LADDERS) {
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
                    if (w->rampNum < NAV_MAX_RAMP_LINKS && NavRampUp(NAV_HOSTP, x, y, z, &exitX, &exitY)) {
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

static bool nav_EdgeExists(const NavWorld* w, int e1, int e2) {
    for (int k = 0; k < w->adjListCount[e1]; k++)
        if (w->edgeList[w->adjList[e1][k]].to == e2) return true;
    return false;
}

static void nav_AddEdgePair(NavWorld* w, int e1, int e2, int cost) {
    if (w->edgeNum >= NAV_MAX_EDGES - 1) return;
    int i1 = w->edgeNum, i2 = w->edgeNum + 1;
    w->edgeList[w->edgeNum++] = (NavGraphEdge){e1, e2, cost};
    w->edgeList[w->edgeNum++] = (NavGraphEdge){e2, e1, cost};
    if (w->adjListCount[e1] < NAV_MAX_EDGES_PER_NODE) w->adjList[e1][w->adjListCount[e1]++] = i1;
    if (w->adjListCount[e2] < NAV_MAX_EDGES_PER_NODE) w->adjList[e2][w->adjListCount[e2]++] = i2;
}

static void nav_AddVerticalLinkEdges(NavWorld* w, bool skipExisting) {
    for (int i = 0; i < w->ladderNum; i++) {
        const NavLadderLink* l = &w->ladderList[i];
        if (skipExisting && nav_EdgeExists(w, l->entranceLow, l->entranceHigh)) continue;
        nav_AddEdgePair(w, l->entranceLow, l->entranceHigh, l->cost);
    }
    for (int i = 0; i < w->rampNum; i++) {
        const NavRampLink* r = &w->rampList[i];
        if (skipExisting && nav_EdgeExists(w, r->entranceRamp, r->entranceExit)) continue;
        nav_AddEdgePair(w, r->entranceRamp, r->entranceExit, r->cost);
    }
}

static void nav_WarnLimits(const NavWorld* w) {
    if (w->entranceNum >= NAV_MAX_ENTRANCES)
        NAV_LOG_WARN("HPA* LIMIT HIT: entrances=%d >= %d - pathfinding will fail!", w->entranceNum, NAV_MAX_ENTRANCES);
    if (w->edgeNum >= NAV_MAX_EDGES - 1)
        NAV_LOG_WARN("HPA* LIMIT HIT: edges=%d >= %d - pathfinding will fail!", w->edgeNum, NAV_MAX_EDGES);
    if (w->ladderNum >= NAV_MAX_LADDERS)
        NAV_LOG_WARN("HPA* LIMIT HIT: ladders=%d >= %d - pathfinding will fail!", w->ladderNum, NAV_MAX_LADDERS);
    if (w->rampNum >= NAV_MAX_RAMP_LINKS)
        NAV_LOG_WARN("HPA* LIMIT HIT: ramps=%d >= %d - pathfinding will fail!", w->rampNum, NAV_MAX_RAMP_LINKS);
}

void NavBuildGraph(NavWorld* w) {
    w->edgeNum = 0;
    for (int i = 0; i < w->entranceNum; i++) w->adjListCount[i] = 0;

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

        // Linear scan (not chunkEnts) so a full build allows up to 128 entrances per chunk
        int ents[NAV_CONNECT_MAX];
        int numEnts = 0;
        for (int i = 0; i < w->entranceNum && numEnts < NAV_CONNECT_MAX; i++) {
            const NavEntrance* e = &w->entranceList[i];
            if (e->z == z && (e->chunk1 == chunk || e->chunk2 == chunk)) ents[numEnts++] = i;
        }
        for (int i = 0; i < numEnts; i++) {
            for (int j = i + 1; j < numEnts; j++) {
                int e1 = ents[i], e2 = ents[j];
                if (nav_EdgeExists(w, e1, e2)) continue;   // entrance shared by 2 chunks
                int cost = NavChunkCost(w, w->entranceList[e1].x, w->entranceList[e1].y, z,
                                        w->entranceList[e2].x, w->entranceList[e2].y,
                                        minX, minY, maxX, maxY);
                if (cost >= 0) nav_AddEdgePair(w, e1, e2, cost);
            }
        }
    }

    nav_AddVerticalLinkEdges(w, false);

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
            }
}

static bool nav_ChunkIdAffected(const NavWorld* w, int chunk) {
    int perLevel = w->chunksX * w->chunksY;
    int z = chunk / perLevel, xy = chunk % perLevel;
    return w->affected[nav_ChunkFlagIndex(w, xy % w->chunksX, xy / w->chunksX, z)];
}

static bool nav_EntranceTouchesAffected(const NavWorld* w, int idx) {
    return nav_ChunkIdAffected(w, w->entranceList[idx].chunk1) || nav_ChunkIdAffected(w, w->entranceList[idx].chunk2);
}

static void nav_RebuildAffectedEntrances(NavWorld* w) {
    bool* aff = w->affected;

    // A ladder/ramp touching any affected level gets both ends rebuilt together
    for (int z = 0; z < w->depth - 1; z++)
        for (int y = 0; y < w->height; y++)
            for (int x = 0; x < w->width; x++) {
                if (NavCanClimbUp(NAV_HOSTP, x, y, z)) {
                    int a = nav_ChunkFlagIndex(w, x / w->chunkW, y / w->chunkH, z);
                    int b = nav_ChunkFlagIndex(w, x / w->chunkW, y / w->chunkH, z + 1);
                    if (aff[a] || aff[b]) aff[a] = aff[b] = true;
                }
                int ex, ey;
                if (NavRampUp(NAV_HOSTP, x, y, z, &ex, &ey)) {
                    int a = nav_ChunkFlagIndex(w, x / w->chunkW, y / w->chunkH, z);
                    int b = nav_ChunkFlagIndex(w, ex / w->chunkW, ey / w->chunkH, z + 1);
                    if (aff[a] || aff[b]) aff[a] = aff[b] = true;
                }
            }

    // Keep entrances that don't touch affected chunks
    int kept = 0;
    for (int i = 0; i < w->entranceNum; i++)
        if (!nav_EntranceTouchesAffected(w, i)) w->entranceList[kept++] = w->entranceList[i];
    w->entranceNum = kept;

    // New border entrances for affected chunks
    for (int z = 0; z < w->depth; z++) nav_BuildBorderEntrances(w, z, true);

    // Ladder links: new entrances for affected ladders, re-find kept ones
    w->ladderNum = 0;
    for (int z = 0; z < w->depth - 1; z++)
        for (int y = 0; y < w->height; y++)
            for (int x = 0; x < w->width; x++) {
                if (!NavCanClimbUp(NAV_HOSTP, x, y, z)) continue;
                int a = nav_ChunkFlagIndex(w, x / w->chunkW, y / w->chunkH, z);
                int b = nav_ChunkFlagIndex(w, x / w->chunkW, y / w->chunkH, z + 1);
                int entLow = -1, entHigh = -1;
                if (aff[a] || aff[b]) {
                    if (w->ladderNum >= NAV_MAX_LADDERS || w->entranceNum + 2 > NAV_MAX_ENTRANCES) continue;
                    entLow = nav_AddPointEntrance(w, x, y, z);
                    entHigh = nav_AddPointEntrance(w, x, y, z + 1);
                    aff[a] = aff[b] = true;
                } else {
                    for (int i = 0; i < w->entranceNum; i++) {
                        const NavEntrance* e = &w->entranceList[i];
                        if (e->x == x && e->y == y && e->chunk1 == e->chunk2) {
                            if (e->z == z) entLow = i;
                            else if (e->z == z + 1) entHigh = i;
                        }
                    }
                }
                if (entLow >= 0 && entHigh >= 0 && w->ladderNum < NAV_MAX_LADDERS) {
                    w->ladderList[w->ladderNum++] = (NavLadderLink){
                        .x = x, .y = y, .zLow = z, .zHigh = z + 1,
                        .entranceLow = entLow, .entranceHigh = entHigh,
                        .cost = NavMoveCost(NAV_HOSTP, x, y, z + 1)
                    };
                }
            }

    // Ramp links: same idea
    w->rampNum = 0;
    for (int z = 0; z < w->depth - 1; z++)
        for (int y = 0; y < w->height; y++)
            for (int x = 0; x < w->width; x++) {
                int ex, ey;
                if (!NavRampUp(NAV_HOSTP, x, y, z, &ex, &ey)) continue;
                int a = nav_ChunkFlagIndex(w, x / w->chunkW, y / w->chunkH, z);
                int b = nav_ChunkFlagIndex(w, ex / w->chunkW, ey / w->chunkH, z + 1);
                int entRamp = -1, entExit = -1;
                if (aff[a] || aff[b]) {
                    if (w->rampNum >= NAV_MAX_RAMP_LINKS || w->entranceNum + 2 > NAV_MAX_ENTRANCES) continue;
                    entRamp = nav_AddPointEntrance(w, x, y, z);
                    entExit = nav_AddPointEntrance(w, ex, ey, z + 1);
                    aff[a] = aff[b] = true;
                } else {
                    for (int i = 0; i < w->entranceNum; i++) {
                        const NavEntrance* e = &w->entranceList[i];
                        if (e->chunk1 != e->chunk2) continue;
                        if (e->x == x && e->y == y && e->z == z) entRamp = i;
                        if (e->x == ex && e->y == ey && e->z == z + 1) entExit = i;
                    }
                }
                if (entRamp >= 0 && entExit >= 0 && w->rampNum < NAV_MAX_RAMP_LINKS) {
                    w->rampList[w->rampNum++] = (NavRampLink){
                        .rampX = x, .rampY = y, .rampZ = z, .exitX = ex, .exitY = ey,
                        .entranceRamp = entRamp, .entranceExit = entExit,
                        .cost = (14 * NavMoveCost(NAV_HOSTP, ex, ey, z + 1)) / 10
                    };
                }
            }
}

static void nav_RebuildAffectedEdges(NavWorld* w) {
    nav_BuildEntranceHash(w);
    nav_BuildChunkEntranceIndex(w);

    for (int i = 0; i < w->oldEntranceNum; i++) {
        const NavEntrance* o = &w->oldEntrances[i];
        w->oldToNew[i] = nav_HashLookupEntrance(w, o->x, o->y, o->z, o->chunk1 == o->chunk2);
    }

    // 1. Keep edges whose (remapped) endpoints are both outside affected chunks
    int kept = 0;
    for (int i = 0; i < w->edgeNum; i++) {
        int n1 = w->oldToNew[w->edgeList[i].from];
        int n2 = w->oldToNew[w->edgeList[i].to];
        if (n1 < 0 || n2 < 0) continue;
        if (nav_EntranceTouchesAffected(w, n1) || nav_EntranceTouchesAffected(w, n2)) continue;
        w->edgeList[kept++] = (NavGraphEdge){n1, n2, w->edgeList[i].cost};
    }
    w->edgeNum = kept;

    // 2. Adjacency from kept edges
    for (int i = 0; i < w->entranceNum; i++) w->adjListCount[i] = 0;
    for (int i = 0; i < kept; i++) {
        int e1 = w->edgeList[i].from;
        if (w->adjListCount[e1] < NAV_MAX_EDGES_PER_NODE) w->adjList[e1][w->adjListCount[e1]++] = i;
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
                bool any = false;
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

                int tx[NAV_MAX_ENTRANCES_PER_CHUNK], ty[NAV_MAX_ENTRANCES_PER_CHUNK];
                int tIdx[NAV_MAX_ENTRANCES_PER_CHUNK], costs[NAV_MAX_ENTRANCES_PER_CHUNK];

                for (int i = 0; i < numEnts; i++) {
                    if (!entAffected[i]) continue;
                    int e1 = w->chunkEnts[chunk][i];
                    if (w->entranceList[e1].z != z) continue;
                    int nt = 0;
                    for (int j = 0; j < numEnts; j++) {
                        if (j == i) continue;
                        int e2 = w->chunkEnts[chunk][j];
                        if (w->entranceList[e2].z != z) continue;
                        if (nav_EdgeExists(w, e1, e2)) continue;
                        tx[nt] = w->entranceList[e2].x;
                        ty[nt] = w->entranceList[e2].y;
                        tIdx[nt] = j;
                        nt++;
                    }
                    if (nt == 0) continue;
                    NavChunkCostsMulti(w, w->entranceList[e1].x, w->entranceList[e1].y, z,
                                       tx, ty, costs, nt, minX, minY, maxX, maxY);
                    for (int t = 0; t < nt; t++) {
                        if (costs[t] < 0) continue;
                        nav_AddEdgePair(w, e1, w->chunkEnts[chunk][tIdx[t]], costs[t]);
                    }
                }
            }

    nav_AddVerticalLinkEdges(w, true);
}

void NavUpdateDirty(NavWorld* w) {
    bool anyDirty = false;
    int total = w->depth * w->chunksX * w->chunksY;
    for (int i = 0; i < total && !anyDirty; i++) anyDirty = w->chunkDirtyFlags[i];
    if (!anyDirty) return;

    nav_ComputeAffectedChunks(w);

    w->oldEntranceNum = w->entranceNum;
    memcpy(w->oldEntrances, w->entranceList, (size_t)w->entranceNum * sizeof(NavEntrance));

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

static inline void nav_ARelax(NavWorld* w, int cur, int nx, int ny, int nz, int stepCost, NavPoint goal) {
    int nid = nav_CellId3(w, nx, ny, nz);
    nav_ATouch(w, nid);
    if (w->aState[nid] == 2) return;
    int ng = w->aG[cur] + stepCost;
    if (ng >= w->aG[nid]) return;
    w->aG[nid] = ng;
    w->aF[nid] = ng + nav_Heuristic3D(w, nx, ny, nz, goal.x, goal.y, goal.z);
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

int NavFindPathAStar(NavWorld* w, NavPoint start, NavPoint goal, NavPoint* out, int maxLen) {
    w->explored = 0;
    w->lastCost = -1;
    if (!nav_InBounds(w, start) || !nav_InBounds(w, goal)) return 0;
    if (!nav_EnsureAStarBuffers(w)) return 0;
    double startTime = NavHostTime();

    if (++w->aCurGen == 0) {   // wrapped after 2^32 searches
        memset(w->aGen, 0, (size_t)w->width * w->height * w->depth * sizeof(uint32_t));
        w->aCurGen = 1;
    }

    int sid = nav_CellId3(w, start.x, start.y, start.z);
    nav_ATouch(w, sid);
    w->aG[sid] = 0;
    w->aF[sid] = nav_Heuristic3D(w, start.x, start.y, start.z, goal.x, goal.y, goal.z);
    w->aState[sid] = 1;
    w->aHeap[0] = sid;
    w->aHeapIdx[sid] = 0;
    w->aHeapCount = 1;

    const int* dx = w->eightDir ? nav_kDx8 : nav_kDx4;
    const int* dy = w->eightDir ? nav_kDy8 : nav_kDy4;
    int numDirs = w->eightDir ? 8 : 4;
    int layer = w->width * w->height;
    int goalId = nav_CellId3(w, goal.x, goal.y, goal.z);
    int len = 0;

    while (w->aHeapCount > 0) {
        int cur = nav_APop(w);
        if (w->aState[cur] == 2) continue;

        if (cur == goalId) {
            w->lastCost = w->aG[cur];
            for (int id = cur; id >= 0 && len < maxLen; id = w->aParent[id]) {
                int rem = id % layer;
                out[len++] = (NavPoint){rem % w->width, rem / w->width, id / layer};
            }
            break;
        }

        w->aState[cur] = 2;
        w->explored++;

        int bz = cur / layer;
        int rem = cur % layer;
        int by = rem / w->width;
        int bx = rem % w->width;

        for (int i = 0; i < numDirs; i++) {
            int nx = bx + dx[i], ny = by + dy[i];
            if (!NavIsWalkable(NAV_HOSTP, nx, ny, bz)) continue;
            if (w->eightDir && dx[i] != 0 && dy[i] != 0) {
                if (!NavIsWalkable(NAV_HOSTP, nx, by, bz) || !NavIsWalkable(NAV_HOSTP, bx, ny, bz)) continue;
            }
            if (!NavCanEnterFrom(NAV_HOSTP, nx, ny, bz, bx, by)) continue;
            int baseCost = (dx[i] != 0 && dy[i] != 0) ? 14 : 10;
            nav_ARelax(w, cur, nx, ny, bz, (baseCost * NavMoveCost(NAV_HOSTP, nx, ny, bz)) / 10, goal);
        }

        if (bz + 1 < w->depth && NavCanClimbUp(NAV_HOSTP, bx, by, bz))
            nav_ARelax(w, cur, bx, by, bz + 1, NavMoveCost(NAV_HOSTP, bx, by, bz + 1), goal);
        if (bz > 0 && NavCanClimbDown(NAV_HOSTP, bx, by, bz))
            nav_ARelax(w, cur, bx, by, bz - 1, NavMoveCost(NAV_HOSTP, bx, by, bz - 1), goal);

        int ex, ey;
        if (bz + 1 < w->depth && NavRampUp(NAV_HOSTP, bx, by, bz, &ex, &ey)) {
            if (ex >= 0 && ex < w->width && ey >= 0 && ey < w->height)
                nav_ARelax(w, cur, ex, ey, bz + 1, (14 * NavMoveCost(NAV_HOSTP, ex, ey, bz + 1)) / 10, goal);
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
                nav_ARelax(w, cur, rx, ry, rz, (14 * NavMoveCost(NAV_HOSTP, rx, ry, rz)) / 10, goal);
            }
        }
    }
    w->lastMs = (NavHostTime() - startTime) * 1000.0;
    return len;
}

// =============================================================================
// HPA* query
// =============================================================================

// Collect entrances of `chunk` and their costs from (p) via one Dijkstra.
static int nav_ConnectToChunk(NavWorld* w, NavPoint p, int chunk, int* outTargets, int* outCosts) {
    int tx[NAV_CONNECT_MAX], ty[NAV_CONNECT_MAX], tIdx[NAV_CONNECT_MAX], costs[NAV_CONNECT_MAX];
    int n = 0;
    for (int i = 0; i < w->entranceNum && n < NAV_CONNECT_MAX; i++) {
        if (w->entranceList[i].chunk1 == chunk || w->entranceList[i].chunk2 == chunk) {
            tx[n] = w->entranceList[i].x;
            ty[n] = w->entranceList[i].y;
            tIdx[n] = i;
            n++;
        }
    }
    if (n == 0) return 0;

    int minX, minY, maxX, maxY;
    nav_GetChunkBounds(w, chunk, &minX, &minY, &maxX, &maxY);
    if (maxX < w->width) maxX++;
    if (maxY < w->height) maxY++;
    NavChunkCostsMulti(w, p.x, p.y, p.z, tx, ty, costs, n,
                       minX > 0 ? minX - 1 : 0, minY > 0 ? minY - 1 : 0, maxX, maxY);

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
    if (w->entranceNum == 0) return 0;   // graph not built
    double startTime = NavHostTime();

    int startChunk = nav_GetChunk(w, start.x, start.y, start.z);
    int goalChunk = nav_GetChunk(w, goal.x, goal.y, goal.z);

    // Same chunk: try a local search first
    if (startChunk == goalChunk) {
        int len = nav_LocalPath(w, start.x, start.y, start.z, goal.x, goal.y, goal.z, out, maxLen);
        if (len > 0) {
            w->lastMs = (NavHostTime() - startTime) * 1000.0;
            return len;
        }
        // fall through: may need to route through neighbouring chunks
    }

    int startNode = w->entranceNum;
    int goalNode = w->entranceNum + 1;
    int totalNodes = w->entranceNum + 2;
    for (int i = 0; i < totalNodes; i++)
        w->absNodes[i] = (NavAbstractNode){NAV_COST_INF, NAV_COST_INF, -1, false, false};

    int startTargets[NAV_CONNECT_MAX], startCosts[NAV_CONNECT_MAX];
    int goalTargets[NAV_CONNECT_MAX], goalCosts[NAV_CONNECT_MAX];
    int startEdges = nav_ConnectToChunk(w, start, startChunk, startTargets, startCosts);
    int goalEdges = nav_ConnectToChunk(w, goal, goalChunk, goalTargets, goalCosts);
    w->explored += 2;

    // A* over the abstract graph
    double absStart = NavHostTime();
    nav_AbsHeapInit(w, totalNodes);
    w->absNodes[startNode].g = 0;
    w->absNodes[startNode].f = nav_Heuristic2D(start.x, start.y, goal.x, goal.y);
    w->absNodes[startNode].open = true;
    nav_AbsHeapPush(w, startNode);

    while (w->absHeapSize > 0) {
        int best = nav_AbsHeapPop(w);
        if (best == goalNode) {
            for (int c = goalNode; c >= 0 && w->absPathLen < NAV_ABS_CAP; c = w->absNodes[c].parent)
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
                         nav_Heuristic2D(w->entranceList[nb].x, w->entranceList[nb].y, goal.x, goal.y));
            }
        } else if (best < w->entranceNum) {
            for (int i = 0; i < w->adjListCount[best]; i++) {
                const NavGraphEdge* e = &w->edgeList[w->adjList[best][i]];
                nav_AbsRelax(w, best, e->to, e->cost,
                         nav_Heuristic2D(w->entranceList[e->to].x, w->entranceList[e->to].y, goal.x, goal.y));
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
            nav_FlowRelax(f, NavFlowCellId(f, x, y, z - 1), c, d + cellCost, maxCost);
        if (z + 1 < f->depth && NavCanClimbDown(NAV_HOSTP, x, y, z + 1))
            nav_FlowRelax(f, NavFlowCellId(f, x, y, z + 1), c, d + cellCost, maxCost);

        // Ramp up onto C: a ramp one level down whose exit is C
        if (z > 0) {
            for (int i = 0; i < 4; i++) {
                int rx = x + off4[i][0], ry = y + off4[i][1];
                int ex, ey;
                if (!NavRampUp(NAV_HOSTP, rx, ry, z - 1, &ex, &ey)) continue;
                if (ex != x || ey != y) continue;
                nav_FlowRelax(f, NavFlowCellId(f, rx, ry, z - 1), c, d + (14 * cellCost) / 10, maxCost);
            }
        }

        // Ramp down onto C: C is a ramp; its high-side cell one level up walks down onto it
        int hdx, hdy;
        if (z + 1 < f->depth && NavRampHighSide(NAV_HOSTP, x, y, z, &hdx, &hdy)) {
            int px = x + hdx, py = y + hdy;
            if (px >= 0 && px < f->width && py >= 0 && py < f->height
                && NavIsWalkable(NAV_HOSTP, px, py, z + 1)) {
                nav_FlowRelax(f, NavFlowCellId(f, px, py, z + 1), c, d + (14 * cellCost) / 10, maxCost);
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

#undef NAV_HOSTP
