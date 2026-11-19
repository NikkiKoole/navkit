/*
 * =============================================================================
 * PATHFINDING
 * =============================================================================
 *
 *   - A*   : full-grid search, binary heap + generation stamps, variable cost
 *   - HPA* : hierarchical (chunk entrances + ladder/ramp links), variable cost,
 *            incremental rebuild of dirty chunks. Default for movers.
 *
 * Movement cost comes from NavMoveCost() (cell_defs.h), shared with the
 * movement layer. JPS/JPS+ were removed 2026-11 (uniform-cost only, so they gave
 * wrong paths once terrain cost existed); see git tag `jps-archive`.
 * =============================================================================
 */
#include "grid.h"
#include "nav_host.h"   // ALL world queries go through Nav* (see nav_host.h)

#include "pathfinding.h"
#include "../../vendor/raylib.h"
#include <stdlib.h>
#include <string.h>

#define COST_INF 999999


// State
Entrance entrances[MAX_ENTRANCES];
int entranceCount = 0;
GraphEdge graphEdges[MAX_EDGES];
int graphEdgeCount = 0;
LadderLink ladderLinks[MAX_LADDERS];
int ladderLinkCount = 0;
RampLink rampLinks[MAX_RAMP_LINKS];
int rampLinkCount = 0;

// Adjacency list for fast edge lookup: adjList[node][i] gives edge index
// adjListCount[node] gives number of edges for that node
static int adjList[MAX_ENTRANCES][MAX_EDGES_PER_NODE];
static int adjListCount[MAX_ENTRANCES];
Point path[MAX_PATH];
int pathLength = 0;
int nodesExplored = 0;
double lastPathTime = 0.0;
double hpaAbstractTime = 0.0;
double hpaRefinementTime = 0.0;

// Path stats for performance comparison (reset every 5 seconds)
static int statsPathCount = 0;
static double statsTotalTime = 0.0;
static double statsLastReportTime = 0.0;
int pathStatsCount = 0;          // Paths computed since last report
double pathStatsTotalMs = 0.0;   // Total ms spent on paths since last report
double pathStatsAvgMs = 0.0;     // Average ms per path
bool pathStatsUpdated = false;   // Flag set when stats are updated
Point startPos = {-1, -1, 0};
Point goalPos = {-1, -1, 0};
AStarNode nodeData[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
bool chunkDirty[MAX_GRID_DEPTH][MAX_CHUNKS_Y][MAX_CHUNKS_X];

// HPA* abstract graph search state
AbstractNode abstractNodes[MAX_ABSTRACT_NODES];
int abstractPath[MAX_ENTRANCES + 2];
int abstractPathLength = 0;

// ============================================================================
// Hash table for O(1) entrance position lookup (used in incremental updates)
// ============================================================================
#define ENTRANCE_HASH_SIZE 32768  // Power of 2, should be > 2x max entrances (MAX_ENTRANCES=16384)

typedef struct {
    int x, y, z;    // Position (key)
    int index;      // Entrance index (value), -1 if empty
    bool isLadder;  // true if chunk1 == chunk2 (ladder/ramp entrance)
} EntranceHashEntry;

static EntranceHashEntry entranceHash[ENTRANCE_HASH_SIZE];
static bool entranceHashBuilt = false;

// Simple hash function for (x, y, z) coordinates
static inline int HashPosition(int x, int y, int z) {
    // Combine x, y, z with prime multipliers, mask to table size
    unsigned int h = (unsigned int)(x * 73856093) ^ (unsigned int)(y * 19349663) ^ (unsigned int)(z * 83492791);
    return (int)(h & (ENTRANCE_HASH_SIZE - 1));
}

// Clear the hash table
static void ClearEntranceHash(void) {
    for (int i = 0; i < ENTRANCE_HASH_SIZE; i++) {
        entranceHash[i].index = -1;
    }
    entranceHashBuilt = false;
}

// Build hash table from current entrances array
static void BuildEntranceHash(void) {
    ClearEntranceHash();
    for (int i = 0; i < entranceCount; i++) {
        int h = HashPosition(entrances[i].x, entrances[i].y, entrances[i].z);
        // Linear probing for collision resolution
        while (entranceHash[h].index >= 0) {
            h = (h + 1) & (ENTRANCE_HASH_SIZE - 1);
        }
        entranceHash[h].x = entrances[i].x;
        entranceHash[h].y = entrances[i].y;
        entranceHash[h].z = entrances[i].z;
        entranceHash[h].index = i;
        entranceHash[h].isLadder = (entrances[i].chunk1 == entrances[i].chunk2);
    }
    entranceHashBuilt = true;
}

// Look up entrance index by position and type, returns -1 if not found
static int HashLookupEntrance(int x, int y, int z, bool isLadder) {
    int h = HashPosition(x, y, z);
    int start = h;
    while (entranceHash[h].index >= 0) {
        if (entranceHash[h].x == x && entranceHash[h].y == y && entranceHash[h].z == z
            && entranceHash[h].isLadder == isLadder) {
            return entranceHash[h].index;
        }
        h = (h + 1) & (ENTRANCE_HASH_SIZE - 1);
        if (h == start) break;  // Full loop, not found
    }
    return -1;
}

// Mapping from old entrance indices to new indices (built during incremental update)
static int oldToNewEntranceIndex[MAX_ENTRANCES];

// ============================================================================
// Chunk → Entrances index for O(1) lookup of entrances per chunk
// ============================================================================
#define MAX_ENTRANCES_PER_CHUNK 64

static int chunkEntrances[MAX_GRID_DEPTH * MAX_CHUNKS_Y * MAX_CHUNKS_X][MAX_ENTRANCES_PER_CHUNK];
static int chunkEntranceCount[MAX_GRID_DEPTH * MAX_CHUNKS_Y * MAX_CHUNKS_X];

// Build the chunk→entrances index from current entrances array
static void BuildChunkEntranceIndex(void) {
    int totalChunks = gridDepth * chunksX * chunksY;
    for (int c = 0; c < totalChunks; c++) {
        chunkEntranceCount[c] = 0;
    }

    for (int i = 0; i < entranceCount; i++) {
        int c1 = entrances[i].chunk1;
        int c2 = entrances[i].chunk2;

        // Add to chunk1's list
        if (chunkEntranceCount[c1] < MAX_ENTRANCES_PER_CHUNK) {
            chunkEntrances[c1][chunkEntranceCount[c1]++] = i;
        }
        // Add to chunk2's list (entrance belongs to both chunks)
        if (c2 != c1 && chunkEntranceCount[c2] < MAX_ENTRANCES_PER_CHUNK) {
            chunkEntrances[c2][chunkEntranceCount[c2]++] = i;
        }
    }
}

// Binary heap for priority queue (used in abstract graph search)
typedef struct {
    int* nodes;      // Array of node indices
    int size;        // Current number of elements
    int capacity;    // Max capacity
} BinaryHeap;

static BinaryHeap heap;
static int heapStorage[MAX_ABSTRACT_NODES];
static int abstractHeapPos[MAX_ABSTRACT_NODES];  // Track position in heap for decrease-key

static void HeapInit(int numNodes) {
    heap.nodes = heapStorage;
    heap.size = 0;
    heap.capacity = MAX_ABSTRACT_NODES;
    // Initialize all positions to -1 (not in heap)
    for (int i = 0; i < numNodes; i++) {
        abstractHeapPos[i] = -1;
    }
}

static void HeapSwap(int i, int j) {
    int nodeI = heap.nodes[i];
    int nodeJ = heap.nodes[j];
    heap.nodes[i] = nodeJ;
    heap.nodes[j] = nodeI;
    // Update position tracking
    abstractHeapPos[nodeI] = j;
    abstractHeapPos[nodeJ] = i;
}

static void HeapBubbleUp(int idx) {
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (abstractNodes[heap.nodes[idx]].f < abstractNodes[heap.nodes[parent]].f) {
            HeapSwap(idx, parent);
            idx = parent;
        } else {
            break;
        }
    }
}

static void HeapBubbleDown(int idx) {
    while (1) {
        int left = 2 * idx + 1;
        int right = 2 * idx + 2;
        int smallest = idx;

        if (left < heap.size && abstractNodes[heap.nodes[left]].f < abstractNodes[heap.nodes[smallest]].f) {
            smallest = left;
        }
        if (right < heap.size && abstractNodes[heap.nodes[right]].f < abstractNodes[heap.nodes[smallest]].f) {
            smallest = right;
        }

        if (smallest != idx) {
            HeapSwap(idx, smallest);
            idx = smallest;
        } else {
            break;
        }
    }
}

static void HeapPush(int node) {
    if (heap.size >= heap.capacity) return;
    int idx = heap.size;
    heap.nodes[idx] = node;
    abstractHeapPos[node] = idx;
    heap.size++;
    HeapBubbleUp(idx);
}

static int HeapPop(void) {
    if (heap.size == 0) return -1;
    int result = heap.nodes[0];
    abstractHeapPos[result] = -1;  // No longer in heap
    heap.size--;
    if (heap.size > 0) {
        heap.nodes[0] = heap.nodes[heap.size];
        abstractHeapPos[heap.nodes[0]] = 0;
        HeapBubbleDown(0);
    }
    return result;
}

static void HeapDecreaseKey(int node) {
    int idx = abstractHeapPos[node];
    if (idx >= 0 && idx < heap.size) {
        HeapBubbleUp(idx);
    }
}

// ============================================================================
// Binary heap for chunk-level A* (uses grid coordinates packed as x + y * MAX_GRID_WIDTH)
// ============================================================================
#define CHUNK_HEAP_CAPACITY (MAX_GRID_WIDTH * MAX_GRID_HEIGHT / 4)  // Quarter of max grid

typedef struct {
    int* nodes;      // Packed coordinates (x + y * MAX_GRID_WIDTH)
    int size;
    int capacity;
} ChunkHeap;

static ChunkHeap chunkHeap;
static int chunkHeapStorage[CHUNK_HEAP_CAPACITY];

// Heap position tracking for decrease-key operation
static int heapPos[MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

static inline int PackCoord(int x, int y) { return x + y * MAX_GRID_WIDTH; }
static inline int UnpackX(int packed) { return packed % MAX_GRID_WIDTH; }
static inline int UnpackY(int packed) { return packed / MAX_GRID_WIDTH; }

// Z-level for chunk heap operations. Safe to use single z because AStarChunk
// calls only operate within a single z-level (chunk-local pathfinding).
static int chunkHeapZ = 0;

static void ChunkHeapInit(void) {
    chunkHeap.nodes = chunkHeapStorage;
    chunkHeap.size = 0;
    chunkHeap.capacity = CHUNK_HEAP_CAPACITY;
}

static void ChunkHeapSwap(int i, int j) {
    int nodeI = chunkHeap.nodes[i];
    int nodeJ = chunkHeap.nodes[j];
    chunkHeap.nodes[i] = nodeJ;
    chunkHeap.nodes[j] = nodeI;
    // Update position tracking
    heapPos[UnpackY(nodeI)][UnpackX(nodeI)] = j;
    heapPos[UnpackY(nodeJ)][UnpackX(nodeJ)] = i;
}

static void ChunkHeapBubbleUp(int idx) {
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        int cx = UnpackX(chunkHeap.nodes[idx]);
        int cy = UnpackY(chunkHeap.nodes[idx]);
        int px = UnpackX(chunkHeap.nodes[parent]);
        int py = UnpackY(chunkHeap.nodes[parent]);
        if (nodeData[chunkHeapZ][cy][cx].f < nodeData[chunkHeapZ][py][px].f) {
            ChunkHeapSwap(idx, parent);
            idx = parent;
        } else {
            break;
        }
    }
}

static void ChunkHeapBubbleDown(int idx) {
    while (1) {
        int left = 2 * idx + 1;
        int right = 2 * idx + 2;
        int smallest = idx;

        int sx = UnpackX(chunkHeap.nodes[smallest]);
        int sy = UnpackY(chunkHeap.nodes[smallest]);
        int smallestF = nodeData[chunkHeapZ][sy][sx].f;

        if (left < chunkHeap.size) {
            int lx = UnpackX(chunkHeap.nodes[left]);
            int ly = UnpackY(chunkHeap.nodes[left]);
            if (nodeData[chunkHeapZ][ly][lx].f < smallestF) {
                smallest = left;
                smallestF = nodeData[chunkHeapZ][ly][lx].f;
            }
        }
        if (right < chunkHeap.size) {
            int rx = UnpackX(chunkHeap.nodes[right]);
            int ry = UnpackY(chunkHeap.nodes[right]);
            if (nodeData[chunkHeapZ][ry][rx].f < smallestF) {
                smallest = right;
            }
        }

        if (smallest != idx) {
            ChunkHeapSwap(idx, smallest);
            idx = smallest;
        } else {
            break;
        }
    }
}

static void ChunkHeapPush(int x, int y) {
    if (chunkHeap.size >= chunkHeap.capacity) return;
    int packed = PackCoord(x, y);
    int idx = chunkHeap.size;
    chunkHeap.nodes[idx] = packed;
    heapPos[y][x] = idx;
    chunkHeap.size++;
    ChunkHeapBubbleUp(idx);
}

static bool ChunkHeapPop(int* outX, int* outY) {
    if (chunkHeap.size == 0) return false;
    int packed = chunkHeap.nodes[0];
    *outX = UnpackX(packed);
    *outY = UnpackY(packed);
    heapPos[*outY][*outX] = -1;
    chunkHeap.size--;
    if (chunkHeap.size > 0) {
        chunkHeap.nodes[0] = chunkHeap.nodes[chunkHeap.size];
        int nx = UnpackX(chunkHeap.nodes[0]);
        int ny = UnpackY(chunkHeap.nodes[0]);
        heapPos[ny][nx] = 0;
        ChunkHeapBubbleDown(0);
    }
    return true;
}

static void ChunkHeapDecreaseKey(int x, int y) {
    int idx = heapPos[y][x];
    if (idx >= 0 && idx < chunkHeap.size) {
        ChunkHeapBubbleUp(idx);
    }
}

// Movement direction mode
bool use8Dir = true;

int Heuristic(int x1, int y1, int x2, int y2) {
    return abs(x1 - x2) + abs(y1 - y2);
}

// 8-directional heuristic (Chebyshev/diagonal distance)
// Scaled by NAV_MIN_CELL_COST to stay admissible with variable terrain costs
// Cardinal = NAV_MIN_CELL_COST(8), Diagonal = (14*8)/10 = 11, Extra = 11-8 = 3
static int Heuristic8Dir(int x1, int y1, int x2, int y2) {
    int dx = abs(x1 - x2);
    int dy = abs(y1 - y2);
    int minD = dx < dy ? dx : dy;
    int maxD = dx > dy ? dx : dy;
    return NAV_MIN_CELL_COST * maxD + 3 * minD;
}

void MarkChunkDirty(int cellX, int cellY, int cellZ) {
    int cx = cellX / chunkWidth;
    int cy = cellY / chunkHeight;
    if (cx >= 0 && cx < chunksX && cy >= 0 && cy < chunksY && cellZ >= 0 && cellZ < gridDepth) {
        chunkDirty[cellZ][cy][cx] = true;
        
        // Mark any additional z-levels affected by this cell change
        // (walkability model determines which levels are affected)
        int additionalZ[4];
        int count = NavAffectedZ(cellZ, additionalZ);
        for (int i = 0; i < count; i++) {
            chunkDirty[additionalZ[i]][cy][cx] = true;
        }
        
        needsRebuild = true;
        hpaNeedsRebuild = true;
        NavOnChunkDirty(cellX, cellY, cellZ);
    }
}

static void AddEntrance(int x, int y, int z, int chunk1, int chunk2) {
    if (entranceCount < MAX_ENTRANCES)
        entrances[entranceCount++] = (Entrance){x, y, z, chunk1, chunk2};
}

static void AddEntrancesForRun(int startX, int startY, int z, int length, int horizontal, int chunk1, int chunk2) {
    int remaining = length, pos = 0;
    while (remaining > 0) {
        int segLen = (remaining > MAX_ENTRANCE_WIDTH) ? MAX_ENTRANCE_WIDTH : remaining;
        int mid = pos + segLen / 2;
        int ex = horizontal ? startX + mid : startX;
        int ey = horizontal ? startY : startY + mid;
        AddEntrance(ex, ey, z, chunk1, chunk2);
        pos += segLen;
        remaining -= segLen;
    }
}

// Add a ladder entrance at given position connecting two z-levels
// Returns the entrance index, or -1 if no room
static int AddLadderEntrance(int x, int y, int z) {
    if (entranceCount >= MAX_ENTRANCES) return -1;
    
    // For ladder entrances, chunk1 and chunk2 are the same chunk
    // (the ladder connects vertically, not horizontally between chunks)
    int chunk = z * (chunksX * chunksY) + (y / chunkHeight) * chunksX + (x / chunkWidth);
    entrances[entranceCount] = (Entrance){x, y, z, chunk, chunk};
    return entranceCount++;
}

// Add a ramp entrance at given position (similar to ladder entrance)
// Returns the entrance index, or -1 if no room
static int AddRampEntrance(int x, int y, int z) {
    if (entranceCount >= MAX_ENTRANCES) return -1;
    
    int chunk = z * (chunksX * chunksY) + (y / chunkHeight) * chunksX + (x / chunkWidth);
    entrances[entranceCount] = (Entrance){x, y, z, chunk, chunk};
    return entranceCount++;
}


void BuildEntrances(void) {
    entranceCount = 0;
    ladderLinkCount = 0;
    rampLinkCount = 0;
    rampCount = 0;  // Recount ramps when rebuilding entrances
    
    int chunksPerLevel = chunksX * chunksY;
    
    // Build entrances for each z-level
    for (int z = 0; z < gridDepth; z++) {
        // Horizontal borders (between rows of chunks)
        for (int cy = 0; cy < chunksY - 1; cy++) {
            for (int cx = 0; cx < chunksX; cx++) {
                int borderY = (cy + 1) * chunkHeight;
                int startX = cx * chunkWidth;
                int chunk1 = z * chunksPerLevel + cy * chunksX + cx;
                int chunk2 = z * chunksPerLevel + (cy + 1) * chunksX + cx;
                int runStart = -1;
                for (int i = 0; i < chunkWidth; i++) {
                    int x = startX + i;
                    bool open = (NavIsWalkable(x, borderY - 1, z) && NavIsWalkable(x, borderY, z));
                    if (open && runStart < 0) runStart = i;
                    else if (!open && runStart >= 0) {
                        AddEntrancesForRun(startX + runStart, borderY, z, i - runStart, 1, chunk1, chunk2);
                        runStart = -1;
                    }
                }
                if (runStart >= 0)
                    AddEntrancesForRun(startX + runStart, borderY, z, chunkWidth - runStart, 1, chunk1, chunk2);
            }
        }
        // Vertical borders (between columns of chunks)
        for (int cy = 0; cy < chunksY; cy++) {
            for (int cx = 0; cx < chunksX - 1; cx++) {
                int borderX = (cx + 1) * chunkWidth;
                int startY = cy * chunkHeight;
                int chunk1 = z * chunksPerLevel + cy * chunksX + cx;
                int chunk2 = z * chunksPerLevel + cy * chunksX + (cx + 1);
                int runStart = -1;
                for (int i = 0; i < chunkHeight; i++) {
                    int y = startY + i;
                    bool open = (NavIsWalkable(borderX - 1, y, z) && NavIsWalkable(borderX, y, z));
                    if (open && runStart < 0) runStart = i;
                    else if (!open && runStart >= 0) {
                        AddEntrancesForRun(borderX, startY + runStart, z, i - runStart, 0, chunk1, chunk2);
                        runStart = -1;
                    }
                }
                if (runStart >= 0)
                    AddEntrancesForRun(borderX, startY + runStart, z, chunkHeight - runStart, 0, chunk1, chunk2);
            }
        }
    }
    
    // Detect ladders and ramps, create ladder links
    // A ladder link connects two z-levels using NavCanClimbUp
    for (int z = 0; z < gridDepth - 1; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                // Detect ladder connections
                if (NavCanClimbUp(x, y, z)) {
                    // Found a ladder connection between z and z+1
                    if (ladderLinkCount < MAX_LADDERS) {
                        // Create entrances on both z-levels for this ladder
                        int entLow = AddLadderEntrance(x, y, z);
                        int entHigh = AddLadderEntrance(x, y, z + 1);
                        
                        if (entLow >= 0 && entHigh >= 0) {
                            ladderLinks[ladderLinkCount++] = (LadderLink){
                                .x = x,
                                .y = y,
                                .zLow = z,
                                .zHigh = z + 1,
                                .entranceLow = entLow,
                                .entranceHigh = entHigh,
                                .cost = NavMoveCost(x, y, z + 1)  // Cost of upper cell
                            };
                        }
                    }
                }
                
                // Detect ramp connections (directional ramps connecting z to z+1)
                int rdx, rdy;
                if (NavRampHighSide(x, y, z, &rdx, &rdy)) {
                    rampCount++;
                    
                    // Check if this ramp can be walked up (valid connection to z+1)
                    int exitX, exitY;
                    if (rampLinkCount < MAX_RAMP_LINKS && NavRampUp(x, y, z, &exitX, &exitY)) {
                        
                        // Create entrances for ramp (at z) and exit cell (at z+1)
                        int entRamp = AddRampEntrance(x, y, z);
                        int entExit = AddRampEntrance(exitX, exitY, z + 1);
                        
                        if (entRamp >= 0 && entExit >= 0) {
                            rampLinks[rampLinkCount++] = (RampLink){
                                .rampX = x,
                                .rampY = y,
                                .rampZ = z,
                                .exitX = exitX,
                                .exitY = exitY,
                                .entranceRamp = entRamp,
                                .entranceExit = entExit,
                                .cost = (14 * NavMoveCost(exitX, exitY, z + 1)) / 10
                            };
                        }
                    }
                }
            }
        }
    }
    // Also check the top z-level for ramps (loop above stops at gridDepth-1)
    // These can't connect to z+1 (no z+1 exists) so just count them
    if (gridDepth > 0) {
        int z = gridDepth - 1;
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                int rdx, rdy;
                if (NavRampHighSide(x, y, z, &rdx, &rdy)) {
                    rampCount++;
                }
            }
        }
    }
    
    // Clear dirty flags for all z-levels
    for (int z = 0; z < gridDepth; z++)
        for (int cy = 0; cy < chunksY; cy++)
            for (int cx = 0; cx < chunksX; cx++)
                chunkDirty[z][cy][cx] = false;
    needsRebuild = false;
    hpaNeedsRebuild = false;
}

int AStarChunk(int sx, int sy, int sz, int gx, int gy, int minX, int minY, int maxX, int maxY) {
    chunkHeapZ = sz;
    // Initialize node data and heap positions
    for (int y = minY; y < maxY; y++)
        for (int x = minX; x < maxX; x++) {
            nodeData[sz][y][x] = (AStarNode){COST_INF, COST_INF, -1, -1, 0, false, false};
            heapPos[y][x] = -1;
        }

    ChunkHeapInit();

    nodeData[sz][sy][sx].g = 0;
    if (use8Dir) {
        nodeData[sz][sy][sx].f = Heuristic8Dir(sx, sy, gx, gy);
    } else {
        nodeData[sz][sy][sx].f = Heuristic(sx, sy, gx, gy) * NAV_MIN_CELL_COST;
    }
    nodeData[sz][sy][sx].open = true;
    ChunkHeapPush(sx, sy);

    int dx4[] = {0, 1, 0, -1};
    int dy4[] = {-1, 0, 1, 0};
    int dx8[] = {0, 1, 1, 1, 0, -1, -1, -1};
    int dy8[] = {-1, -1, 0, 1, 1, 1, 0, -1};

    int* dx = use8Dir ? dx8 : dx4;
    int* dy = use8Dir ? dy8 : dy4;
    int numDirs = use8Dir ? 8 : 4;

    int bestX, bestY;
    while (ChunkHeapPop(&bestX, &bestY)) {
        if (bestX == gx && bestY == gy) {
            return nodeData[sz][gy][gx].g;
        }
        nodeData[sz][bestY][bestX].open = false;
        nodeData[sz][bestY][bestX].closed = true;

        for (int i = 0; i < numDirs; i++) {
            int nx = bestX + dx[i], ny = bestY + dy[i];
            if (nx < minX || nx >= maxX || ny < minY || ny >= maxY) continue;
            if (!NavIsWalkable(nx, ny, sz) || nodeData[sz][ny][nx].closed) continue;

            // Prevent corner cutting for diagonal movement
            if (use8Dir && dx[i] != 0 && dy[i] != 0) {
                int adjX = bestX + dx[i], adjY = bestY + dy[i];
                // Check bounds before accessing grid for corner-cut check
                if (adjX < 0 || adjX >= gridWidth || adjY < 0 || adjY >= gridHeight)
                    continue;
                if (!NavIsWalkable(adjX, bestY, sz) || !NavIsWalkable(bestX, adjY, sz))
                    continue;
            }

            int baseCost = (dx[i] != 0 && dy[i] != 0) ? 14 : 10;
            int moveCost = (baseCost * NavMoveCost(nx, ny, sz)) / 10;
            int ng = nodeData[sz][bestY][bestX].g + moveCost;
            if (ng < nodeData[sz][ny][nx].g) {
                bool wasOpen = nodeData[sz][ny][nx].open;
                nodeData[sz][ny][nx].g = ng;
                if (use8Dir) {
                    nodeData[sz][ny][nx].f = ng + Heuristic8Dir(nx, ny, gx, gy);
                } else {
                    nodeData[sz][ny][nx].f = ng + Heuristic(nx, ny, gx, gy) * NAV_MIN_CELL_COST;
                }
                nodeData[sz][ny][nx].open = true;
                if (wasOpen) {
                    ChunkHeapDecreaseKey(nx, ny);
                } else {
                    ChunkHeapPush(nx, ny);
                }
            }
        }
    }
    return -1;  // No path found
}

// Multi-target Dijkstra within chunk bounds - finds costs to all targets in a single search
// Returns number of targets found. Costs are written to outCosts array (-1 if unreachable)
int AStarChunkMultiTarget(int sx, int sy, int sz,
                          int* targetX, int* targetY, int* outCosts, int numTargets,
                          int minX, int minY, int maxX, int maxY) {
    chunkHeapZ = sz;
    // Initialize node data and heap positions
    for (int y = minY; y < maxY; y++)
        for (int x = minX; x < maxX; x++) {
            nodeData[sz][y][x] = (AStarNode){COST_INF, COST_INF, -1, -1, 0, false, false};
            heapPos[y][x] = -1;
        }

    // Initialize output costs to -1 (unreachable)
    for (int i = 0; i < numTargets; i++) {
        outCosts[i] = -1;
    }

    ChunkHeapInit();

    // Use Dijkstra (no heuristic) since we have multiple targets
    nodeData[sz][sy][sx].g = 0;
    nodeData[sz][sy][sx].f = 0;
    nodeData[sz][sy][sx].open = true;
    ChunkHeapPush(sx, sy);

    int dx4[] = {0, 1, 0, -1};
    int dy4[] = {-1, 0, 1, 0};
    int dx8[] = {0, 1, 1, 1, 0, -1, -1, -1};
    int dy8[] = {-1, -1, 0, 1, 1, 1, 0, -1};

    int* dx = use8Dir ? dx8 : dx4;
    int* dy = use8Dir ? dy8 : dy4;
    int numDirs = use8Dir ? 8 : 4;

    int targetsFound = 0;

    int bestX, bestY;
    while (ChunkHeapPop(&bestX, &bestY)) {
        // Check if this is one of our targets (check ALL - there may be duplicates)
        for (int t = 0; t < numTargets; t++) {
            if (bestX == targetX[t] && bestY == targetY[t]) {
                if (outCosts[t] < 0) {
                    outCosts[t] = nodeData[sz][bestY][bestX].g;
                    targetsFound++;
                    if (targetsFound == numTargets) {
                        return targetsFound;  // All targets found
                    }
                }
                // Don't break - there may be duplicate targets at same coords
            }
        }

        nodeData[sz][bestY][bestX].open = false;
        nodeData[sz][bestY][bestX].closed = true;

        for (int i = 0; i < numDirs; i++) {
            int nx = bestX + dx[i], ny = bestY + dy[i];
            if (nx < minX || nx >= maxX || ny < minY || ny >= maxY) continue;
            if (!NavIsWalkable(nx, ny, sz) || nodeData[sz][ny][nx].closed) continue;

            // Prevent corner cutting for diagonal movement
            if (use8Dir && dx[i] != 0 && dy[i] != 0) {
                int adjX = bestX + dx[i], adjY = bestY + dy[i];
                // Check bounds before accessing grid for corner-cut check
                if (adjX < 0 || adjX >= gridWidth || adjY < 0 || adjY >= gridHeight)
                    continue;
                if (!NavIsWalkable(adjX, bestY, sz) || !NavIsWalkable(bestX, adjY, sz))
                    continue;
            }

            int baseCost = (dx[i] != 0 && dy[i] != 0) ? 14 : 10;
            int moveCost = (baseCost * NavMoveCost(nx, ny, sz)) / 10;
            int ng = nodeData[sz][bestY][bestX].g + moveCost;
            if (ng < nodeData[sz][ny][nx].g) {
                bool wasOpen = nodeData[sz][ny][nx].open;
                nodeData[sz][ny][nx].g = ng;
                nodeData[sz][ny][nx].f = ng;  // Dijkstra: f = g (no heuristic)
                nodeData[sz][ny][nx].open = true;
                if (wasOpen) {
                    ChunkHeapDecreaseKey(nx, ny);
                } else {
                    ChunkHeapPush(nx, ny);
                }
            }
        }
    }

    return targetsFound;
}

void BuildGraph(void) {
    graphEdgeCount = 0;

    // Clear adjacency list
    for (int i = 0; i < entranceCount; i++) {
        adjListCount[i] = 0;
    }

    double startTime = GetTime();
    int chunksPerLevel = chunksX * chunksY;
    int totalChunks = gridDepth * chunksPerLevel;
    
    // Build intra-level edges (within each z-level)
    for (int chunk = 0; chunk < totalChunks; chunk++) {
        // Decode chunk ID to get z, cx, cy
        int z = chunk / chunksPerLevel;
        int xyChunk = chunk % chunksPerLevel;
        int cx = xyChunk % chunksX;
        int cy = xyChunk / chunksX;
        
        int minX = cx * chunkWidth;
        int minY = cy * chunkHeight;
        int maxX = (cx + 1) * chunkWidth + 1;
        int maxY = (cy + 1) * chunkHeight + 1;
        if (maxX > gridWidth) maxX = gridWidth;
        if (maxY > gridHeight) maxY = gridHeight;

        int chunkEnts[128];
        int numEnts = 0;
        for (int i = 0; i < entranceCount && numEnts < 128; i++) {
            // Only consider entrances on the same z-level
            if (entrances[i].z != z) continue;
            if (entrances[i].chunk1 == chunk || entrances[i].chunk2 == chunk)
                chunkEnts[numEnts++] = i;
        }
        for (int i = 0; i < numEnts; i++) {
            for (int j = i + 1; j < numEnts; j++) {
                int e1 = chunkEnts[i], e2 = chunkEnts[j];

                // Check if edge already exists (can happen when entrances share 2 chunks)
                bool exists = false;
                for (int k = 0; k < adjListCount[e1] && !exists; k++) {
                    int edgeIdx = adjList[e1][k];
                    if (graphEdges[edgeIdx].to == e2) exists = true;
                }
                if (exists) continue;

                int cost = AStarChunk(entrances[e1].x, entrances[e1].y, z, entrances[e2].x, entrances[e2].y, minX, minY, maxX, maxY);
                if (cost >= 0 && graphEdgeCount >= MAX_EDGES - 1) {
                    static bool warned = false;
                    if (!warned) {
                        TraceLog(LOG_WARNING, "MAX_EDGES limit (%d) reached at chunk %d! Graph will be incomplete.", MAX_EDGES, chunk);
                        warned = true;
                    }
                }
                if (cost >= 0 && graphEdgeCount < MAX_EDGES - 1) {
                    int edgeIdx1 = graphEdgeCount;
                    int edgeIdx2 = graphEdgeCount + 1;
                    graphEdges[graphEdgeCount++] = (GraphEdge){e1, e2, cost};
                    graphEdges[graphEdgeCount++] = (GraphEdge){e2, e1, cost};

                    // Add to adjacency list
                    if (adjListCount[e1] < MAX_EDGES_PER_NODE) {
                        adjList[e1][adjListCount[e1]++] = edgeIdx1;
                    }
                    if (adjListCount[e2] < MAX_EDGES_PER_NODE) {
                        adjList[e2][adjListCount[e2]++] = edgeIdx2;
                    }
                }
            }
        }
    }
    
    // Add edges for ladder links (cross z-level connections)
    for (int i = 0; i < ladderLinkCount; i++) {
        LadderLink* link = &ladderLinks[i];
        int e1 = link->entranceLow;
        int e2 = link->entranceHigh;
        int cost = link->cost;
        
        if (graphEdgeCount < MAX_EDGES - 1) {
            int edgeIdx1 = graphEdgeCount;
            int edgeIdx2 = graphEdgeCount + 1;
            graphEdges[graphEdgeCount++] = (GraphEdge){e1, e2, cost};
            graphEdges[graphEdgeCount++] = (GraphEdge){e2, e1, cost};
            
            // Add to adjacency list
            if (adjListCount[e1] < MAX_EDGES_PER_NODE) {
                adjList[e1][adjListCount[e1]++] = edgeIdx1;
            }
            if (adjListCount[e2] < MAX_EDGES_PER_NODE) {
                adjList[e2][adjListCount[e2]++] = edgeIdx2;
            }
        }
    }
    
    // Add edges for ramp links (cross z-level connections via directional ramps)
    for (int i = 0; i < rampLinkCount; i++) {
        RampLink* link = &rampLinks[i];
        int e1 = link->entranceRamp;  // Lower entrance (on ramp at z)
        int e2 = link->entranceExit;  // Upper entrance (exit at z+1)
        int cost = link->cost;
        
        if (graphEdgeCount < MAX_EDGES - 1) {
            int edgeIdx1 = graphEdgeCount;
            int edgeIdx2 = graphEdgeCount + 1;
            // Bidirectional: can walk up and down the ramp
            graphEdges[graphEdgeCount++] = (GraphEdge){e1, e2, cost};
            graphEdges[graphEdgeCount++] = (GraphEdge){e2, e1, cost};
            
            // Add to adjacency list
            if (adjListCount[e1] < MAX_EDGES_PER_NODE) {
                adjList[e1][adjListCount[e1]++] = edgeIdx1;
            }
            if (adjListCount[e2] < MAX_EDGES_PER_NODE) {
                adjList[e2][adjListCount[e2]++] = edgeIdx2;
            }
        }
    }
    
    TraceLog(LOG_INFO, "Built graph: %d edges (%d ladder links, %d ramp links) in %.2fms", 
             graphEdgeCount, ladderLinkCount, rampLinkCount, (GetTime() - startTime) * 1000);
    
    // Warn if any limits were hit - these cause pathfinding failures
    if (entranceCount >= MAX_ENTRANCES) {
        TraceLog(LOG_WARNING, "HPA* LIMIT HIT: entranceCount=%d >= MAX_ENTRANCES=%d - pathfinding will fail!", 
                 entranceCount, MAX_ENTRANCES);
    }
    if (graphEdgeCount >= MAX_EDGES) {
        TraceLog(LOG_WARNING, "HPA* LIMIT HIT: graphEdgeCount=%d >= MAX_EDGES=%d - pathfinding will fail!", 
                 graphEdgeCount, MAX_EDGES);
    }
    if (ladderLinkCount >= MAX_LADDERS) {
        TraceLog(LOG_WARNING, "HPA* LIMIT HIT: ladderLinkCount=%d >= MAX_LADDERS=%d - pathfinding will fail!", 
                 ladderLinkCount, MAX_LADDERS);
    }
    if (rampLinkCount >= MAX_RAMP_LINKS) {
        TraceLog(LOG_WARNING, "HPA* LIMIT HIT: rampLinkCount=%d >= MAX_RAMP_LINKS=%d - pathfinding will fail!", 
                 rampLinkCount, MAX_RAMP_LINKS);
    }
}

// ============== Incremental Update Functions ==============

// Get the set of chunks affected by dirty chunks (dirty + their neighbors)
// Now 3D: checks all z-levels
static void GetAffectedChunks(bool affectedChunks[MAX_GRID_DEPTH][MAX_CHUNKS_Y][MAX_CHUNKS_X]) {
    for (int z = 0; z < gridDepth; z++)
        for (int cy = 0; cy < chunksY; cy++)
            for (int cx = 0; cx < chunksX; cx++)
                affectedChunks[z][cy][cx] = false;

    for (int z = 0; z < gridDepth; z++) {
        for (int cy = 0; cy < chunksY; cy++) {
            for (int cx = 0; cx < chunksX; cx++) {
                if (chunkDirty[z][cy][cx]) {
                    affectedChunks[z][cy][cx] = true;
                    // Mark XY neighbors as affected too (they share borders)
                    if (cy > 0) affectedChunks[z][cy-1][cx] = true;
                    if (cy < chunksY-1) affectedChunks[z][cy+1][cx] = true;
                    if (cx > 0) affectedChunks[z][cy][cx-1] = true;
                    if (cx < chunksX-1) affectedChunks[z][cy][cx+1] = true;
                    // Mark Z neighbors as affected too (for ladders/ramps)
                    if (z > 0) affectedChunks[z-1][cy][cx] = true;
                    if (z < gridDepth-1) affectedChunks[z+1][cy][cx] = true;
                }
            }
        }
    }
}

// Check if an entrance touches any affected chunk (now 3D)
static bool EntranceTouchesAffected(int entranceIdx, bool affectedChunks[MAX_GRID_DEPTH][MAX_CHUNKS_Y][MAX_CHUNKS_X]) {
    int chunksPerLevel = chunksX * chunksY;
    int c1 = entrances[entranceIdx].chunk1;
    int c2 = entrances[entranceIdx].chunk2;
    // Extract z, y, x chunk indices
    int z1 = c1 / chunksPerLevel, xyChunk1 = c1 % chunksPerLevel;
    int z2 = c2 / chunksPerLevel, xyChunk2 = c2 % chunksPerLevel;
    int cy1 = xyChunk1 / chunksX, cx1 = xyChunk1 % chunksX;
    int cy2 = xyChunk2 / chunksX, cx2 = xyChunk2 % chunksX;
    return affectedChunks[z1][cy1][cx1] || affectedChunks[z2][cy2][cx2];
}

// Rebuild entrances for affected chunks (simpler approach - no keeping/remapping)
static void RebuildAffectedEntrances(bool affectedChunks[MAX_GRID_DEPTH][MAX_CHUNKS_Y][MAX_CHUNKS_X]) {
    // First pass: if a ladder or ramp has ANY of its z-levels in an affected chunk,
    // mark ALL its z-levels as affected. This ensures both entrances
    // get filtered out together, preventing duplicates.
    for (int z = 0; z < gridDepth - 1; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                // Check ladders
                if (NavCanClimbUp(x, y, z)) {
                    int cx = x / chunkWidth;
                    int cy = y / chunkHeight;
                    // If either z-level is affected, mark both as affected
                    if (affectedChunks[z][cy][cx] || affectedChunks[z + 1][cy][cx]) {
                        affectedChunks[z][cy][cx] = true;
                        affectedChunks[z + 1][cy][cx] = true;
                    }
                }
                
                // Check ramps (ramp at z, exit at z+1 with different x,y)
                int exitX, exitY;
                if (NavRampUp(x, y, z, &exitX, &exitY)) {
                    
                    int cxRamp = x / chunkWidth;
                    int cyRamp = y / chunkHeight;
                    int cxExit = exitX / chunkWidth;
                    int cyExit = exitY / chunkHeight;
                    
                    // If either endpoint's chunk is affected, mark both as affected
                    if (affectedChunks[z][cyRamp][cxRamp] || affectedChunks[z + 1][cyExit][cxExit]) {
                        affectedChunks[z][cyRamp][cxRamp] = true;
                        affectedChunks[z + 1][cyExit][cxExit] = true;
                    }
                }
            }
        }
    }

    // Remove entrances that touch any affected chunk
    int newCount = 0;
    for (int i = 0; i < entranceCount; i++) {
        if (!EntranceTouchesAffected(i, affectedChunks)) {
            entrances[newCount++] = entrances[i];
        }
    }

    
    int chunksPerLevel = chunksX * chunksY;

    // Rebuild entrances for all z-levels where at least one chunk is affected
    for (int z = 0; z < gridDepth; z++) {
        // Horizontal borders (between cy and cy+1)
        for (int cy = 0; cy < chunksY - 1; cy++) {
            for (int cx = 0; cx < chunksX; cx++) {
                if (!affectedChunks[z][cy][cx] && !affectedChunks[z][cy+1][cx]) continue;

                int borderY = (cy + 1) * chunkHeight;
                int startX = cx * chunkWidth;
                int chunk1 = z * chunksPerLevel + cy * chunksX + cx;
                int chunk2 = z * chunksPerLevel + (cy + 1) * chunksX + cx;
                int runStart = -1;

                for (int i = 0; i < chunkWidth; i++) {
                    int x = startX + i;
                    bool open = (NavIsWalkable(x, borderY - 1, z) && NavIsWalkable(x, borderY, z));
                    if (open && runStart < 0) runStart = i;
                    else if (!open && runStart >= 0) {
                        int length = i - runStart;
                        int pos = 0;
                        while (length > 0 && newCount < MAX_ENTRANCES) {
                            int segLen = (length > MAX_ENTRANCE_WIDTH) ? MAX_ENTRANCE_WIDTH : length;
                            int mid = pos + segLen / 2;
                            entrances[newCount++] = (Entrance){startX + runStart + mid, borderY, z, chunk1, chunk2};
                            pos += segLen;
                            length -= segLen;
                        }
                        runStart = -1;
                    }
                }
                if (runStart >= 0) {
                    int length = chunkWidth - runStart;
                    int pos = 0;
                    while (length > 0 && newCount < MAX_ENTRANCES) {
                        int segLen = (length > MAX_ENTRANCE_WIDTH) ? MAX_ENTRANCE_WIDTH : length;
                        int mid = pos + segLen / 2;
                        entrances[newCount++] = (Entrance){startX + runStart + mid, borderY, z, chunk1, chunk2};
                        pos += segLen;
                        length -= segLen;
                    }
                }
            }
        }

        // Vertical borders (between cx and cx+1)
        for (int cy = 0; cy < chunksY; cy++) {
            for (int cx = 0; cx < chunksX - 1; cx++) {
                if (!affectedChunks[z][cy][cx] && !affectedChunks[z][cy][cx+1]) continue;

                int borderX = (cx + 1) * chunkWidth;
                int startY = cy * chunkHeight;
                int chunk1 = z * chunksPerLevel + cy * chunksX + cx;
                int chunk2 = z * chunksPerLevel + cy * chunksX + (cx + 1);
                int runStart = -1;

                for (int i = 0; i < chunkHeight; i++) {
                    int y = startY + i;
                    bool open = (NavIsWalkable(borderX - 1, y, z) && NavIsWalkable(borderX, y, z));
                    if (open && runStart < 0) runStart = i;
                    else if (!open && runStart >= 0) {
                        int length = i - runStart;
                        int pos = 0;
                        while (length > 0 && newCount < MAX_ENTRANCES) {
                            int segLen = (length > MAX_ENTRANCE_WIDTH) ? MAX_ENTRANCE_WIDTH : length;
                            int mid = pos + segLen / 2;
                            entrances[newCount++] = (Entrance){borderX, startY + runStart + mid, z, chunk1, chunk2};
                            pos += segLen;
                            length -= segLen;
                        }
                        runStart = -1;
                    }
                }
                if (runStart >= 0) {
                    int length = chunkHeight - runStart;
                    int pos = 0;
                    while (length > 0 && newCount < MAX_ENTRANCES) {
                        int segLen = (length > MAX_ENTRANCE_WIDTH) ? MAX_ENTRANCE_WIDTH : length;
                        int mid = pos + segLen / 2;
                        entrances[newCount++] = (Entrance){borderX, startY + runStart + mid, z, chunk1, chunk2};
                        pos += segLen;
                        length -= segLen;
                    }
                }
            }
        }
    }
    
    // Rebuild ladder links
    // We need to:
    // 1. Keep ladder links in unaffected chunks (and remap their entrance indices)
    // 2. Only rebuild ladder links in affected chunks
    // 
    // First pass: count kept ladder entrances and build index remapping
    // Ladder entrances were already filtered out above if they touched affected chunks.
    // Now we scan all ladders and only add entrances for those in affected chunks.
    
    ladderLinkCount = 0;
    for (int z = 0; z < gridDepth - 1; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (NavCanClimbUp(x, y, z)) {
                    int cx = x / chunkWidth;
                    int cy = y / chunkHeight;
                    
                    // Check if this ladder is in an affected chunk
                    bool ladderAffected = affectedChunks[z][cy][cx] || affectedChunks[z + 1][cy][cx];
                    
                    if (ladderAffected) {
                        // Ladder is in affected chunk - add new entrances
                        if (ladderLinkCount < MAX_LADDERS && newCount + 2 <= MAX_ENTRANCES) {
                            int chunkLow = z * (chunksX * chunksY) + cy * chunksX + cx;
                            int chunkHigh = (z + 1) * (chunksX * chunksY) + cy * chunksX + cx;
                            
                            int entLow = newCount;
                            entrances[newCount++] = (Entrance){x, y, z, chunkLow, chunkLow};
                            int entHigh = newCount;
                            entrances[newCount++] = (Entrance){x, y, z + 1, chunkHigh, chunkHigh};
                            
                            // Mark both chunks as affected so edges get built for new ladder entrances
                            affectedChunks[z][cy][cx] = true;
                            affectedChunks[z + 1][cy][cx] = true;
                            
                            ladderLinks[ladderLinkCount++] = (LadderLink){
                                .x = x,
                                .y = y,
                                .zLow = z,
                                .zHigh = z + 1,
                                .entranceLow = entLow,
                                .entranceHigh = entHigh,
                                .cost = NavMoveCost(x, y, z + 1)
                            };
                        }
                    } else {
                        // Ladder is in unaffected chunk - find existing entrances
                        // They were kept during the filter pass, we just need to find their indices
                        int entLow = -1, entHigh = -1;
                        for (int i = 0; i < newCount; i++) {
                            if (entrances[i].x == x && entrances[i].y == y
                                && entrances[i].chunk1 == entrances[i].chunk2) {
                                if (entrances[i].z == z) entLow = i;
                                else if (entrances[i].z == z + 1) entHigh = i;
                            }
                        }
                        
                        if (entLow >= 0 && entHigh >= 0 && ladderLinkCount < MAX_LADDERS) {
                            ladderLinks[ladderLinkCount++] = (LadderLink){
                                .x = x,
                                .y = y,
                                .zLow = z,
                                .zHigh = z + 1,
                                .entranceLow = entLow,
                                .entranceHigh = entHigh,
                                .cost = NavMoveCost(x, y, z + 1)
                            };
                        }
                    }
                }
            }
        }
    }
    
    // Rebuild ramp links (similar to ladder links but with different x,y for exit)
    rampLinkCount = 0;
    for (int z = 0; z < gridDepth - 1; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                int exitX, exitY;
                if (NavRampUp(x, y, z, &exitX, &exitY)) {
                    
                    int cxRamp = x / chunkWidth;
                    int cyRamp = y / chunkHeight;
                    int cxExit = exitX / chunkWidth;
                    int cyExit = exitY / chunkHeight;
                    
                    // Check if this ramp is in an affected chunk
                    bool rampAffected = affectedChunks[z][cyRamp][cxRamp] || affectedChunks[z + 1][cyExit][cxExit];
                    
                    if (rampAffected) {
                        // Ramp is in affected chunk - add new entrances
                        if (rampLinkCount < MAX_RAMP_LINKS && newCount + 2 <= MAX_ENTRANCES) {
                            int chunkRamp = z * (chunksX * chunksY) + cyRamp * chunksX + cxRamp;
                            int chunkExit = (z + 1) * (chunksX * chunksY) + cyExit * chunksX + cxExit;
                            
                            int entRamp = newCount;
                            entrances[newCount++] = (Entrance){x, y, z, chunkRamp, chunkRamp};
                            int entExit = newCount;
                            entrances[newCount++] = (Entrance){exitX, exitY, z + 1, chunkExit, chunkExit};
                            
                            // Mark both chunks as affected so edges get built
                            affectedChunks[z][cyRamp][cxRamp] = true;
                            affectedChunks[z + 1][cyExit][cxExit] = true;
                            
                            rampLinks[rampLinkCount++] = (RampLink){
                                .rampX = x,
                                .rampY = y,
                                .rampZ = z,
                                .exitX = exitX,
                                .exitY = exitY,
                                .entranceRamp = entRamp,
                                .entranceExit = entExit,
                                .cost = (14 * NavMoveCost(exitX, exitY, z + 1)) / 10
                            };
                        }
                    } else {
                        // Ramp is in unaffected chunk - find existing entrances
                        int entRamp = -1, entExit = -1;
                        for (int i = 0; i < newCount; i++) {
                            if (entrances[i].x == x && entrances[i].y == y && entrances[i].z == z
                                && entrances[i].chunk1 == entrances[i].chunk2) {
                                entRamp = i;
                            }
                            if (entrances[i].x == exitX && entrances[i].y == exitY && entrances[i].z == z + 1
                                && entrances[i].chunk1 == entrances[i].chunk2) {
                                entExit = i;
                            }
                        }
                        
                        if (entRamp >= 0 && entExit >= 0 && rampLinkCount < MAX_RAMP_LINKS) {
                            rampLinks[rampLinkCount++] = (RampLink){
                                .rampX = x,
                                .rampY = y,
                                .rampZ = z,
                                .exitX = exitX,
                                .exitY = exitY,
                                .entranceRamp = entRamp,
                                .entranceExit = entExit,
                                .cost = (14 * NavMoveCost(exitX, exitY, z + 1)) / 10
                            };
                        }
                    }
                }
            }
        }
    }

    entranceCount = newCount;
}

// Storage for old entrances before rebuild (used for edge remapping)
static Entrance oldEntrances[MAX_ENTRANCES];
static int oldEntranceCount = 0;

// Save entrances before rebuilding (call before RebuildAffectedEntrances)
static void SaveOldEntrances(void) {
    oldEntranceCount = entranceCount;
    for (int i = 0; i < entranceCount; i++) {
        oldEntrances[i] = entrances[i];
    }
}

// Check if an entrance (by new index) touches any affected chunk (now 3D)
static bool NewEntranceTouchesAffected(int idx, bool affectedChunks[MAX_GRID_DEPTH][MAX_CHUNKS_Y][MAX_CHUNKS_X]) {
    int chunksPerLevel = chunksX * chunksY;
    int c1 = entrances[idx].chunk1;
    int c2 = entrances[idx].chunk2;
    // Extract z, y, x chunk indices
    int z1 = c1 / chunksPerLevel, xyChunk1 = c1 % chunksPerLevel;
    int z2 = c2 / chunksPerLevel, xyChunk2 = c2 % chunksPerLevel;
    int cy1 = xyChunk1 / chunksX, cx1 = xyChunk1 % chunksX;
    int cy2 = xyChunk2 / chunksX, cx2 = xyChunk2 % chunksX;
    return affectedChunks[z1][cy1][cx1] || affectedChunks[z2][cy2][cx2];
}

// Rebuild graph edges - keep edges in unaffected chunks, rebuild affected
static void RebuildAffectedEdges(bool affectedChunks[MAX_GRID_DEPTH][MAX_CHUNKS_Y][MAX_CHUNKS_X]) {
    // Step 0: Build indexes for fast lookup
    // - Hash table: entrance position → index (for old→new mapping)
    // - Chunk index: chunk → list of entrance indices (for Step 3)
    BuildEntranceHash();
    BuildChunkEntranceIndex();

    for (int i = 0; i < oldEntranceCount; i++) {
        bool isLadder = (oldEntrances[i].chunk1 == oldEntrances[i].chunk2);
        oldToNewEntranceIndex[i] = HashLookupEntrance(oldEntrances[i].x, oldEntrances[i].y, oldEntrances[i].z, isLadder);
    }

    // Step 1: Keep edges where both entrances don't touch any affected chunk
    int newEdgeCount = 0;

    for (int i = 0; i < graphEdgeCount; i++) {
        int oldE1 = graphEdges[i].from;
        int oldE2 = graphEdges[i].to;

        // Find new indices using precomputed mapping (O(1) instead of O(n))
        int newE1 = oldToNewEntranceIndex[oldE1];
        int newE2 = oldToNewEntranceIndex[oldE2];

        // Skip if either entrance no longer exists
        if (newE1 < 0 || newE2 < 0) continue;

        // Skip if either entrance touches an affected chunk
        if (NewEntranceTouchesAffected(newE1, affectedChunks)) continue;
        if (NewEntranceTouchesAffected(newE2, affectedChunks)) continue;

        // Keep this edge with remapped indices
        graphEdges[newEdgeCount++] = (GraphEdge){newE1, newE2, graphEdges[i].cost};
    }

    int keptEdges = newEdgeCount;
    graphEdgeCount = newEdgeCount;

    // Step 2: Rebuild adjacency lists from kept edges
    for (int i = 0; i < entranceCount; i++) {
        adjListCount[i] = 0;
    }
    for (int i = 0; i < keptEdges; i++) {
        int e1 = graphEdges[i].from;
        if (adjListCount[e1] < MAX_EDGES_PER_NODE) {
            adjList[e1][adjListCount[e1]++] = i;
        }
    }

    // Step 3: Rebuild edges using multi-target Dijkstra (one search per affected entrance)
    // Instead of O(n²) A* calls per chunk, we do O(n) Dijkstra calls
    int chunksPerLevel = chunksX * chunksY;

    for (int z = 0; z < gridDepth; z++) {
        for (int cy = 0; cy < chunksY; cy++) {
            for (int cx = 0; cx < chunksX; cx++) {
                int chunk = z * chunksPerLevel + cy * chunksX + cx;
                int numEnts = chunkEntranceCount[chunk];
                if (numEnts == 0) continue;

                // Skip chunks that don't need processing
                if (!affectedChunks[z][cy][cx]) {
                    // Check if any entrance in this chunk touches an affected chunk
                    bool needsProcessing = false;
                    for (int i = 0; i < numEnts && !needsProcessing; i++) {
                        int entIdx = chunkEntrances[chunk][i];
                        if (NewEntranceTouchesAffected(entIdx, affectedChunks)) {
                            needsProcessing = true;
                        }
                    }
                    if (!needsProcessing) continue;
                }

                int minX = cx * chunkWidth;
                int minY = cy * chunkHeight;
                int maxX = (cx + 1) * chunkWidth + 1;
                int maxY = (cy + 1) * chunkHeight + 1;
                if (maxX > gridWidth) maxX = gridWidth;
                if (maxY > gridHeight) maxY = gridHeight;

                // Collect which entrances in this chunk are affected
                // and which need edges rebuilt
                bool entAffected[MAX_ENTRANCES_PER_CHUNK];
                for (int i = 0; i < numEnts; i++) {
                    int entIdx = chunkEntrances[chunk][i];
                    entAffected[i] = NewEntranceTouchesAffected(entIdx, affectedChunks);
                }

                // For each AFFECTED entrance, run ONE multi-target Dijkstra to find
                // costs to ALL other entrances in the chunk (not just affected ones)
                // This is more efficient because:
                // - We only run Dijkstra from affected entrances (fewer calls)
                // - Each call finds edges to both affected and unaffected entrances
                int targetX[MAX_ENTRANCES_PER_CHUNK];
                int targetY[MAX_ENTRANCES_PER_CHUNK];
                int targetIdx[MAX_ENTRANCES_PER_CHUNK];
                int outCosts[MAX_ENTRANCES_PER_CHUNK];

                for (int i = 0; i < numEnts; i++) {
                    if (!entAffected[i]) continue;  // Only run from affected entrances

                    int e1 = chunkEntrances[chunk][i];

                    // Build target list: all OTHER entrances that don't have edges yet
                    int numTargets = 0;
                    for (int j = 0; j < numEnts; j++) {
                        if (j == i) continue;  // Skip self

                        int e2 = chunkEntrances[chunk][j];

                        // Check if edge already exists (from kept edges or earlier in this loop)
                        bool exists = false;
                        for (int k = 0; k < adjListCount[e1] && !exists; k++) {
                            int edgeIdx = adjList[e1][k];
                            if (graphEdges[edgeIdx].to == e2) exists = true;
                        }
                        if (exists) continue;

                        targetX[numTargets] = entrances[e2].x;
                        targetY[numTargets] = entrances[e2].y;
                        targetIdx[numTargets] = j;  // Store chunk-local index
                        numTargets++;
                    }

                    if (numTargets == 0) continue;

                    // Run ONE multi-target Dijkstra from e1 to all targets
                    AStarChunkMultiTarget(entrances[e1].x, entrances[e1].y, z,
                                          targetX, targetY, outCosts, numTargets,
                                          minX, minY, maxX, maxY);

                    // Add edges for all reachable targets
                    for (int t = 0; t < numTargets; t++) {
                        int cost = outCosts[t];
                        if (cost < 0) continue;  // Unreachable

                        int e2 = chunkEntrances[chunk][targetIdx[t]];

                        if (graphEdgeCount >= MAX_EDGES - 1) continue;

                        int edgeIdx1 = graphEdgeCount;
                        int edgeIdx2 = graphEdgeCount + 1;
                        graphEdges[graphEdgeCount++] = (GraphEdge){e1, e2, cost};
                        graphEdges[graphEdgeCount++] = (GraphEdge){e2, e1, cost};

                        if (adjListCount[e1] < MAX_EDGES_PER_NODE) {
                            adjList[e1][adjListCount[e1]++] = edgeIdx1;
                        }
                        if (adjListCount[e2] < MAX_EDGES_PER_NODE) {
                            adjList[e2][adjListCount[e2]++] = edgeIdx2;
                        }
                    }
                }
            }
        }
    }

    // Add edges for ladder links (cross z-level connections)
    for (int i = 0; i < ladderLinkCount; i++) {
        LadderLink* link = &ladderLinks[i];
        int e1 = link->entranceLow;
        int e2 = link->entranceHigh;
        int cost = link->cost;
        
        // Check if edge already exists
        bool exists = false;
        for (int k = 0; k < adjListCount[e1] && !exists; k++) {
            int edgeIdx = adjList[e1][k];
            if (graphEdges[edgeIdx].to == e2) exists = true;
        }
        if (exists) continue;
        
        if (graphEdgeCount < MAX_EDGES - 1) {
            int edgeIdx1 = graphEdgeCount;
            int edgeIdx2 = graphEdgeCount + 1;
            graphEdges[graphEdgeCount++] = (GraphEdge){e1, e2, cost};
            graphEdges[graphEdgeCount++] = (GraphEdge){e2, e1, cost};
            
            if (adjListCount[e1] < MAX_EDGES_PER_NODE) {
                adjList[e1][adjListCount[e1]++] = edgeIdx1;
            }
            if (adjListCount[e2] < MAX_EDGES_PER_NODE) {
                adjList[e2][adjListCount[e2]++] = edgeIdx2;
            }
        }
    }
    
    // Add edges for ramp links (cross z-level connections via directional ramps)
    for (int i = 0; i < rampLinkCount; i++) {
        RampLink* link = &rampLinks[i];
        int e1 = link->entranceRamp;
        int e2 = link->entranceExit;
        int cost = link->cost;
        
        // Check if edge already exists
        bool exists = false;
        for (int k = 0; k < adjListCount[e1] && !exists; k++) {
            int edgeIdx = adjList[e1][k];
            if (graphEdges[edgeIdx].to == e2) exists = true;
        }
        if (exists) continue;
        
        if (graphEdgeCount < MAX_EDGES - 1) {
            int edgeIdx1 = graphEdgeCount;
            int edgeIdx2 = graphEdgeCount + 1;
            graphEdges[graphEdgeCount++] = (GraphEdge){e1, e2, cost};
            graphEdges[graphEdgeCount++] = (GraphEdge){e2, e1, cost};
            
            if (adjListCount[e1] < MAX_EDGES_PER_NODE) {
                adjList[e1][adjListCount[e1]++] = edgeIdx1;
            }
            if (adjListCount[e2] < MAX_EDGES_PER_NODE) {
                adjList[e2][adjListCount[e2]++] = edgeIdx2;
            }
        }
    }

}

void UpdateDirtyChunks(void) {
    // Check if any chunks are dirty (across all z-levels)
    bool anyDirty = false;
    for (int z = 0; z < gridDepth && !anyDirty; z++)
        for (int cy = 0; cy < chunksY && !anyDirty; cy++)
            for (int cx = 0; cx < chunksX && !anyDirty; cx++)
                if (chunkDirty[z][cy][cx]) anyDirty = true;

    if (!anyDirty) return;

    double startTime = GetTime();

    // Get affected chunks (dirty + neighbors) - now 3D
    bool affectedChunks[MAX_GRID_DEPTH][MAX_CHUNKS_Y][MAX_CHUNKS_X];
    GetAffectedChunks(affectedChunks);


    // Save old entrances for edge remapping
    SaveOldEntrances();

    // Rebuild entrances for affected chunks
    RebuildAffectedEntrances(affectedChunks);

    // Rebuild edges (keeps edges in unaffected chunks, rebuilds affected)
    RebuildAffectedEdges(affectedChunks);

    // Clear dirty flags (all z-levels)
    for (int z = 0; z < gridDepth; z++)
        for (int cy = 0; cy < chunksY; cy++)
            for (int cx = 0; cx < chunksX; cx++)
                chunkDirty[z][cy][cx] = false;
    needsRebuild = false;
    hpaNeedsRebuild = false;

    // Warn if any limits were hit during incremental update
    if (entranceCount >= MAX_ENTRANCES) {
        TraceLog(LOG_WARNING, "HPA* LIMIT HIT: entranceCount=%d >= MAX_ENTRANCES=%d - pathfinding will fail!", 
                 entranceCount, MAX_ENTRANCES);
    }
    if (graphEdgeCount >= MAX_EDGES) {
        TraceLog(LOG_WARNING, "HPA* LIMIT HIT: graphEdgeCount=%d >= MAX_EDGES=%d - pathfinding will fail!", 
                 graphEdgeCount, MAX_EDGES);
    }
    if (ladderLinkCount >= MAX_LADDERS) {
        TraceLog(LOG_WARNING, "HPA* LIMIT HIT: ladderLinkCount=%d >= MAX_LADDERS=%d - pathfinding will fail!", 
                 ladderLinkCount, MAX_LADDERS);
    }
    if (rampLinkCount >= MAX_RAMP_LINKS) {
        TraceLog(LOG_WARNING, "HPA* LIMIT HIT: rampLinkCount=%d >= MAX_RAMP_LINKS=%d - pathfinding will fail!", 
                 rampLinkCount, MAX_RAMP_LINKS);
    }

    (void)startTime;  // Unused now
}

// 3D heuristic - includes z-level difference
// Scaled by NAV_MIN_CELL_COST for admissibility with variable terrain costs
static int Heuristic3D(int x0, int y0, int z0, int x1, int y1, int z1) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int dz = abs(z1 - z0);
    if (use8Dir) {
        int maxXY = (dx > dy) ? dx : dy;
        int minXY = (dx < dy) ? dx : dy;
        // Cardinal = 8, Diagonal = 11, z-transition = 8
        return (maxXY - minXY) * NAV_MIN_CELL_COST + minXY * 11 + dz * NAV_MIN_CELL_COST;
    } else {
        return (dx + dy + dz) * NAV_MIN_CELL_COST;
    }
}

// ============================================================================
// Full-grid A* state (heap + generation stamps)
// ============================================================================
// Sized to the actual grid (W*H*D) on demand, not the compile-time maxima.
// Generation stamps make per-call init O(1): a node whose gen != astarCurGen
// is treated as fresh the first time it is touched in a search.
static int*      astarG = NULL;
static int*      astarF = NULL;
static int*      astarParent = NULL;
static uint8_t*  astarState = NULL;   // 0=unseen 1=open 2=closed
static uint32_t* astarGen = NULL;
static int*      astarHeap = NULL;
static int*      astarHeapIdx = NULL;
static int       astarHeapCount = 0;
static int       astarCap = 0;
static uint32_t  astarCurGen = 0;
int lastPathCost = -1;

static void AStarEnsureCapacity(int n) {
    if (n <= astarCap) return;
    free(astarG); free(astarF); free(astarParent); free(astarState);
    free(astarGen); free(astarHeap); free(astarHeapIdx);
    astarG       = malloc((size_t)n * sizeof(int));
    astarF       = malloc((size_t)n * sizeof(int));
    astarParent  = malloc((size_t)n * sizeof(int));
    astarState   = malloc((size_t)n);
    astarGen     = calloc((size_t)n, sizeof(uint32_t));
    astarHeap    = malloc((size_t)n * sizeof(int));
    astarHeapIdx = malloc((size_t)n * sizeof(int));
    astarCap = n;
    astarCurGen = 0;
}

static inline int AStarNodeId(int x, int y, int z) {
    return (z * gridHeight + y) * gridWidth + x;
}

static inline void AStarTouch(int id) {
    if (astarGen[id] != astarCurGen) {
        astarGen[id] = astarCurGen;
        astarG[id] = COST_INF;
        astarF[id] = COST_INF;
        astarParent[id] = -1;
        astarState[id] = 0;
        astarHeapIdx[id] = -1;
    }
}

static void AStarBubbleUp(int i) {
    while (i > 0) {
        int p = (i - 1) / 2;
        if (astarF[astarHeap[p]] <= astarF[astarHeap[i]]) break;
        int t = astarHeap[p]; astarHeap[p] = astarHeap[i]; astarHeap[i] = t;
        astarHeapIdx[astarHeap[p]] = p;
        astarHeapIdx[astarHeap[i]] = i;
        i = p;
    }
}

static void AStarBubbleDown(int i) {
    for (;;) {
        int l = 2 * i + 1, r = l + 1, s = i;
        if (l < astarHeapCount && astarF[astarHeap[l]] < astarF[astarHeap[s]]) s = l;
        if (r < astarHeapCount && astarF[astarHeap[r]] < astarF[astarHeap[s]]) s = r;
        if (s == i) break;
        int t = astarHeap[s]; astarHeap[s] = astarHeap[i]; astarHeap[i] = t;
        astarHeapIdx[astarHeap[s]] = s;
        astarHeapIdx[astarHeap[i]] = i;
        i = s;
    }
}

static int AStarPop(void) {
    int top = astarHeap[0];
    astarHeapCount--;
    if (astarHeapCount > 0) {
        astarHeap[0] = astarHeap[astarHeapCount];
        astarHeapIdx[astarHeap[0]] = 0;
        AStarBubbleDown(0);
    }
    astarHeapIdx[top] = -1;
    return top;
}

// Relax edge cur -> (nx,ny,nz) with the given step cost. Caller has already
// checked walkability / transition legality and bounds.
static inline void AStarRelax(int cur, int nx, int ny, int nz, int stepCost) {
    int nid = AStarNodeId(nx, ny, nz);
    AStarTouch(nid);
    if (astarState[nid] == 2) return;
    int ng = astarG[cur] + stepCost;
    if (ng >= astarG[nid]) return;
    astarG[nid] = ng;
    astarF[nid] = ng + Heuristic3D(nx, ny, nz, goalPos.x, goalPos.y, goalPos.z);
    astarParent[nid] = cur;
    if (astarHeapIdx[nid] >= 0) {
        AStarBubbleUp(astarHeapIdx[nid]);
    } else {
        astarState[nid] = 1;
        astarHeap[astarHeapCount] = nid;
        astarHeapIdx[nid] = astarHeapCount;
        astarHeapCount++;
        AStarBubbleUp(astarHeapCount - 1);
    }
}

void RunAStar(void) {
    if (startPos.x < 0 || goalPos.x < 0) return;
    pathLength = 0;
    nodesExplored = 0;
    lastPathCost = -1;
    double startTime = GetTime();

    AStarEnsureCapacity(gridWidth * gridHeight * gridDepth);
    if (++astarCurGen == 0) {
        // Wrapped after 2^32 searches: reset stamps so stale nodes can't alias.
        memset(astarGen, 0, (size_t)astarCap * sizeof(uint32_t));
        astarCurGen = 1;
    }
    astarHeapCount = 0;

    int sid = AStarNodeId(startPos.x, startPos.y, startPos.z);
    AStarTouch(sid);
    astarG[sid] = 0;
    astarF[sid] = Heuristic3D(startPos.x, startPos.y, startPos.z, goalPos.x, goalPos.y, goalPos.z);
    astarState[sid] = 1;
    astarHeap[0] = sid;
    astarHeapIdx[sid] = 0;
    astarHeapCount = 1;

    // Direction arrays for XY movement
    int dx4[] = {0, 1, 0, -1};
    int dy4[] = {-1, 0, 1, 0};
    int dx8[] = {0, 1, 1, 1, 0, -1, -1, -1};
    int dy8[] = {-1, -1, 0, 1, 1, 1, 0, -1};

    int* dx = use8Dir ? dx8 : dx4;
    int* dy = use8Dir ? dy8 : dy4;
    int numDirs = use8Dir ? 8 : 4;

    int layer = gridWidth * gridHeight;
    int goalId = AStarNodeId(goalPos.x, goalPos.y, goalPos.z);

    while (astarHeapCount > 0) {
        int cur = AStarPop();
        if (astarState[cur] == 2) continue;

        // Reached the goal (must match z too!)
        if (cur == goalId) {
            lastPathCost = astarG[cur];
            int id = cur;
            while (id >= 0 && pathLength < MAX_PATH) {
                int z = id / layer;
                int rem = id % layer;
                path[pathLength++] = (Point){rem % gridWidth, rem / gridWidth, z};
                id = astarParent[id];
            }
            break;
        }

        astarState[cur] = 2;
        nodesExplored++;

        int bestZ = cur / layer;
        int rem = cur % layer;
        int bestY = rem / gridWidth;
        int bestX = rem % gridWidth;

        // Expand XY neighbors on same z-level
        for (int i = 0; i < numDirs; i++) {
            int nx = bestX + dx[i], ny = bestY + dy[i], nz = bestZ;
            if (!NavIsWalkable(nx, ny, nz)) continue;

            // For diagonal movement, don't cut corners through walls
            if (use8Dir && dx[i] != 0 && dy[i] != 0) {
                if (!NavIsWalkable(bestX + dx[i], bestY, bestZ) ||
                    !NavIsWalkable(bestX, bestY + dy[i], bestZ))
                    continue;
            }

            // Block side entry to ramps (can only enter from low or high side)
            if (!NavCanEnterFrom(nx, ny, nz, bestX, bestY)) continue;

            // Cost: base 10/14 scaled by terrain cost
            int baseCost = (dx[i] != 0 && dy[i] != 0) ? 14 : 10;
            AStarRelax(cur, nx, ny, nz, (baseCost * NavMoveCost(nx, ny, nz)) / 10);
        }

        // Ladder connections (cost of destination cell)
        if (NavCanClimbUp(bestX, bestY, bestZ)) {
            AStarRelax(cur, bestX, bestY, bestZ + 1, NavMoveCost(bestX, bestY, bestZ + 1));
        }
        if (NavCanClimbDown(bestX, bestY, bestZ)) {
            AStarRelax(cur, bestX, bestY, bestZ - 1, NavMoveCost(bestX, bestY, bestZ - 1));
        }

        // === RAMP Z-TRANSITIONS ===
        // Going UP via ramp (current cell is ramp, move to exit tile at z+1)
        int exitX, exitY;
        if (NavRampUp(bestX, bestY, bestZ, &exitX, &exitY)) {
            int exitZ = bestZ + 1;
            if (exitX >= 0 && exitX < gridWidth && exitY >= 0 && exitY < gridHeight && exitZ < gridDepth) {
                AStarRelax(cur, exitX, exitY, exitZ, (14 * NavMoveCost(exitX, exitY, exitZ)) / 10);
            }
        }

        // Going DOWN via ramp (a ramp below whose high side we're on)
        if (bestZ > 0) {
            int checkOffsets[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};  // N, E, S, W
            for (int i = 0; i < 4; i++) {
                int rampX = bestX + checkOffsets[i][0];
                int rampY = bestY + checkOffsets[i][1];
                int rampZ = bestZ - 1;
                int hdx, hdy;
                if (!NavRampHighSide(rampX, rampY, rampZ, &hdx, &hdy)) continue;
                if (rampX + hdx != bestX || rampY + hdy != bestY) continue;  // high side must point at us
                if (!NavIsWalkable(rampX, rampY, rampZ)) continue;
                AStarRelax(cur, rampX, rampY, rampZ, (14 * NavMoveCost(rampX, rampY, rampZ)) / 10);
            }
        }
    }
    lastPathTime = (GetTime() - startTime) * 1000.0;
}

// Get chunk index from cell coordinates (includes z-level in ID)
// Chunk ID = z * (chunksX * chunksY) + cy * chunksX + cx
static int GetChunk(int x, int y, int z) {
    int cx = x / chunkWidth;
    int cy = y / chunkHeight;
    if (cx < 0) cx = 0;
    if (cx >= chunksX) cx = chunksX - 1;
    if (cy < 0) cy = 0;
    if (cy >= chunksY) cy = chunksY - 1;
    if (z < 0) z = 0;
    if (z >= gridDepth) z = gridDepth - 1;
    return z * (chunksX * chunksY) + cy * chunksX + cx;
}

// Get chunk bounds (extracts z from chunk ID, returns XY bounds)
static void GetChunkBounds(int chunk, int* minX, int* minY, int* maxX, int* maxY, int* outZ) {
    int chunksPerLevel = chunksX * chunksY;
    int z = chunk / chunksPerLevel;
    int xyChunk = chunk % chunksPerLevel;
    int cx = xyChunk % chunksX;
    int cy = xyChunk / chunksX;
    *minX = cx * chunkWidth;
    *minY = cy * chunkHeight;
    *maxX = (cx + 1) * chunkWidth;
    *maxY = (cy + 1) * chunkHeight;
    if (*maxX > gridWidth) *maxX = gridWidth;
    if (*maxY > gridHeight) *maxY = gridHeight;
    if (outZ) *outZ = z;
}

// ============================================================================
// ReconstructLocalPath - Find cell-level path between two points
//
// PROBLEM SOLVED: When movers spawn near chunk boundaries (especially in
// dungeon rooms that span multiple chunks), the path from the mover's position
// to the first entrance may require going through an adjacent chunk. If we
// only search within the immediate chunk bounds, we fail to find valid paths,
// causing:
//   1. Paths that start at the wrong position (INIT MISMATCH bug)
//   2. The LOS check rejecting valid paths (oscillating yellow movers)
//
// SOLUTION: Try narrow bounds first (fast common case), then expand bounds
// by one chunk in all directions if no path is found. This handles cases where:
//   - Entrances sit ON chunk boundaries and belong to two chunks
//   - The path between points near a boundary goes through an adjacent chunk
//   - Dungeon rooms span chunk boundaries with corridors in other chunks
// ============================================================================

// Helper: Run A* within given bounds on specified z-level
static int ReconstructLocalPathWithBounds(int sx, int sy, int sz, int gx, int gy, 
                                          int minX, int minY, int maxX, int maxY,
                                          Point* outPath, int maxLen) {
    chunkHeapZ = sz;
    // Initialize node data and heap positions
    for (int y = minY; y < maxY; y++)
        for (int x = minX; x < maxX; x++) {
            nodeData[sz][y][x] = (AStarNode){COST_INF, COST_INF, -1, -1, 0, false, false};
            heapPos[y][x] = -1;
        }

    ChunkHeapInit();

    nodeData[sz][sy][sx].g = 0;
    if (use8Dir) {
        nodeData[sz][sy][sx].f = Heuristic8Dir(sx, sy, gx, gy);
    } else {
        nodeData[sz][sy][sx].f = Heuristic(sx, sy, gx, gy) * NAV_MIN_CELL_COST;
    }
    nodeData[sz][sy][sx].open = true;
    ChunkHeapPush(sx, sy);

    int dx4[] = {0, 1, 0, -1};
    int dy4[] = {-1, 0, 1, 0};
    int dx8[] = {0, 1, 1, 1, 0, -1, -1, -1};
    int dy8[] = {-1, -1, 0, 1, 1, 1, 0, -1};

    int* dx = use8Dir ? dx8 : dx4;
    int* dy = use8Dir ? dy8 : dy4;
    int numDirs = use8Dir ? 8 : 4;

    int bestX, bestY;
    while (ChunkHeapPop(&bestX, &bestY)) {
        if (bestX == gx && bestY == gy) {
            // Reconstruct path
            int len = 0;
            int cx = gx, cy = gy;
            while (cx >= 0 && cy >= 0 && len < maxLen) {
                outPath[len++] = (Point){cx, cy, sz};
                int px = nodeData[sz][cy][cx].parentX;
                int py = nodeData[sz][cy][cx].parentY;
                cx = px;
                cy = py;
            }
            return len;
        }
        nodeData[sz][bestY][bestX].open = false;
        nodeData[sz][bestY][bestX].closed = true;

        for (int i = 0; i < numDirs; i++) {
            int nx = bestX + dx[i], ny = bestY + dy[i];
            if (nx < minX || nx >= maxX || ny < minY || ny >= maxY) continue;
            if (!NavIsWalkable(nx, ny, sz) || nodeData[sz][ny][nx].closed) continue;

            // Prevent corner cutting for diagonal movement
            if (use8Dir && dx[i] != 0 && dy[i] != 0) {
                int adjX = bestX + dx[i], adjY = bestY + dy[i];
                if (adjX < 0 || adjX >= gridWidth || adjY < 0 || adjY >= gridHeight)
                    continue;
                if (!NavIsWalkable(adjX, bestY, sz) || !NavIsWalkable(bestX, adjY, sz))
                    continue;
            }

            int baseCost = (dx[i] != 0 && dy[i] != 0) ? 14 : 10;
            int moveCost = (baseCost * NavMoveCost(nx, ny, sz)) / 10;
            int ng = nodeData[sz][bestY][bestX].g + moveCost;
            if (ng < nodeData[sz][ny][nx].g) {
                bool wasOpen = nodeData[sz][ny][nx].open;
                nodeData[sz][ny][nx].g = ng;
                if (use8Dir) {
                    nodeData[sz][ny][nx].f = ng + Heuristic8Dir(nx, ny, gx, gy);
                } else {
                    nodeData[sz][ny][nx].f = ng + Heuristic(nx, ny, gx, gy) * NAV_MIN_CELL_COST;
                }
                nodeData[sz][ny][nx].parentX = bestX;
                nodeData[sz][ny][nx].parentY = bestY;
                nodeData[sz][ny][nx].open = true;
                if (wasOpen) {
                    ChunkHeapDecreaseKey(nx, ny);
                } else {
                    ChunkHeapPush(nx, ny);
                }
            }
        }
    }
    return 0;  // No path found
}

// Main entry point: tries narrow bounds first, expands if needed
// Note: This operates on a single z-level. For 3D HPA*, ladder transitions
// happen at the abstract graph level, not during local path refinement.
static int ReconstructLocalPath(int sx, int sy, int sz, int gx, int gy, int gz, Point* outPath, int maxLen) {
    // For now, local path refinement stays on the same z-level
    // (ladders are handled as abstract graph edges)
    if (sz != gz) return 0;  // Can't reconstruct across z-levels locally
    
    // Get bounds that cover both start and goal chunks
    int startChunk = GetChunk(sx, sy, sz);
    int goalChunk = GetChunk(gx, gy, gz);

    int minX1, minY1, maxX1, maxY1, z1;
    int minX2, minY2, maxX2, maxY2, z2;
    GetChunkBounds(startChunk, &minX1, &minY1, &maxX1, &maxY1, &z1);
    GetChunkBounds(goalChunk, &minX2, &minY2, &maxX2, &maxY2, &z2);

    // Union of both chunk bounds (narrow search area)
    int minX = minX1 < minX2 ? minX1 : minX2;
    int minY = minY1 < minY2 ? minY1 : minY2;
    int maxX = maxX1 > maxX2 ? maxX1 : maxX2;
    int maxY = maxY1 > maxY2 ? maxY1 : maxY2;

    // Clamp to grid
    if (minX < 0) minX = 0;
    if (minY < 0) minY = 0;
    if (maxX > gridWidth) maxX = gridWidth;
    if (maxY > gridHeight) maxY = gridHeight;

    // Try narrow bounds first (fast path for most cases)
    int len = ReconstructLocalPathWithBounds(sx, sy, sz, gx, gy, minX, minY, maxX, maxY, outPath, maxLen);
    if (len > 0) return len;

    // Narrow search failed - expand bounds by one chunk in all directions
    // This handles paths that need to go through adjacent chunks
    int expandedMinX = minX - chunkWidth;
    int expandedMinY = minY - chunkHeight;
    int expandedMaxX = maxX + chunkWidth;
    int expandedMaxY = maxY + chunkHeight;

    // Clamp expanded bounds to grid
    if (expandedMinX < 0) expandedMinX = 0;
    if (expandedMinY < 0) expandedMinY = 0;
    if (expandedMaxX > gridWidth) expandedMaxX = gridWidth;
    if (expandedMaxY > gridHeight) expandedMaxY = gridHeight;

    return ReconstructLocalPathWithBounds(sx, sy, sz, gx, gy, expandedMinX, expandedMinY, expandedMaxX, expandedMaxY, outPath, maxLen);
}

int FindPathHPA(Point start, Point goal, Point* outPath, int maxLen) {
    if (start.x < 0 || goal.x < 0) return 0;
    if (entranceCount == 0) return 0;  // Need to build entrances first

    int resultLen = 0;
    abstractPathLength = 0;
    nodesExplored = 0;
    hpaAbstractTime = 0.0;
    hpaRefinementTime = 0.0;
    double startTime = GetTime();

    int startChunk = GetChunk(start.x, start.y, start.z);
    int goalChunk = GetChunk(goal.x, goal.y, goal.z);

    // Debug logging for cross-z paths (disabled by default)
    bool debugCrossZ = false;

    // Special case: start and goal in same chunk - try local A* first
    if (startChunk == goalChunk) {
        resultLen = ReconstructLocalPath(start.x, start.y, start.z, 
                                         goal.x, goal.y, goal.z, outPath, maxLen);
        if (resultLen > 0) {
            lastPathTime = (GetTime() - startTime) * 1000.0;
            return resultLen;  // Found path locally, done
        }
        // Local failed - fall through to full HPA* abstract search
        // Path might need to route through neighboring chunks, even on same z-level
        // (e.g., walls blocking direct path within chunk bounds)
    }

    // Temporary entrance indices for start and goal
    int startNode = entranceCount;      // Index for start as temp node
    int goalNode = entranceCount + 1;   // Index for goal as temp node
    int totalNodes = entranceCount + 2;

    // Initialize abstract nodes
    for (int i = 0; i < totalNodes; i++) {
        abstractNodes[i] = (AbstractNode){COST_INF, COST_INF, -1, false, false};
    }

    // Build temporary edges from start to entrances in its chunk
    // and from goal to entrances in its chunk
    // We'll store these costs in arrays
    int startEdgeCosts[128];
    int startEdgeTargets[128];
    int startEdgeCount = 0;

    int goalEdgeCosts[128];
    int goalEdgeTargets[128];
    int goalEdgeCount = 0;

    int minX, minY, maxX, maxY;

    // Gather entrances for start chunk
    int startTargetX[128], startTargetY[128], startTargetIdx[128];
    int startTargetCount = 0;
    for (int i = 0; i < entranceCount && startTargetCount < 128; i++) {
        if (entrances[i].chunk1 == startChunk || entrances[i].chunk2 == startChunk) {
            startTargetX[startTargetCount] = entrances[i].x;
            startTargetY[startTargetCount] = entrances[i].y;
            startTargetIdx[startTargetCount] = i;
            startTargetCount++;
        }
    }

    // Gather entrances for goal chunk
    int goalTargetX[128], goalTargetY[128], goalTargetIdx[128];
    int goalTargetCount = 0;
    for (int i = 0; i < entranceCount && goalTargetCount < 128; i++) {
        if (entrances[i].chunk1 == goalChunk || entrances[i].chunk2 == goalChunk) {
            goalTargetX[goalTargetCount] = entrances[i].x;
            goalTargetY[goalTargetCount] = entrances[i].y;
            goalTargetIdx[goalTargetCount] = i;
            goalTargetCount++;
        }
    }

    // Debug: report entrances found
    if (debugCrossZ) {
        TraceLog(LOG_INFO, "  startChunk=%d, entrances found: %d", startChunk, startTargetCount);
        TraceLog(LOG_INFO, "  goalChunk=%d, entrances found: %d", goalChunk, goalTargetCount);
        // Show entrance positions and where their edges lead
        for (int i = 0; i < startTargetCount && i < 3; i++) {
            int ent = startTargetIdx[i];
            TraceLog(LOG_INFO, "    start ent[%d]: (%d,%d,z%d) edges=%d", 
                     ent, entrances[ent].x, entrances[ent].y, entrances[ent].z, adjListCount[ent]);
            // Show first few edge destinations
            for (int j = 0; j < adjListCount[ent] && j < 3; j++) {
                int edgeIdx = adjList[ent][j];
                int neighbor = graphEdges[edgeIdx].to;
                TraceLog(LOG_INFO, "      -> ent[%d]: (%d,%d,z%d)", 
                         neighbor, entrances[neighbor].x, entrances[neighbor].y, entrances[neighbor].z);
            }
        }
        for (int i = 0; i < goalTargetCount && i < 3; i++) {
            int ent = goalTargetIdx[i];
            TraceLog(LOG_INFO, "    goal ent[%d]: (%d,%d,z%d) edges=%d", 
                     ent, entrances[ent].x, entrances[ent].y, entrances[ent].z, adjListCount[ent]);
        }
    }

    // ==========================================================================
    // CONNECT PHASE: Find costs from start/goal to all entrances in their chunks
    // Uses multi-target Dijkstra - single search finds all targets efficiently
    // ==========================================================================
    int dijkstraStartCosts[128], dijkstraGoalCosts[128];
    int chunkZ;

    GetChunkBounds(startChunk, &minX, &minY, &maxX, &maxY, &chunkZ);
    if (maxX < gridWidth) maxX++;
    if (maxY < gridHeight) maxY++;
    if (startTargetCount > 0) {
        AStarChunkMultiTarget(start.x, start.y, start.z,
                              startTargetX, startTargetY, dijkstraStartCosts, startTargetCount,
                              minX > 0 ? minX - 1 : 0, minY > 0 ? minY - 1 : 0, maxX, maxY);
    }

    GetChunkBounds(goalChunk, &minX, &minY, &maxX, &maxY, &chunkZ);
    if (maxX < gridWidth) maxX++;
    if (maxY < gridHeight) maxY++;
    if (goalTargetCount > 0) {
        AStarChunkMultiTarget(goal.x, goal.y, goal.z,
                              goalTargetX, goalTargetY, dijkstraGoalCosts, goalTargetCount,
                              minX > 0 ? minX - 1 : 0, minY > 0 ? minY - 1 : 0, maxX, maxY);
    }

    // Use Dijkstra results for connect phase
    for (int i = 0; i < startTargetCount; i++) {
        if (dijkstraStartCosts[i] >= 0) {
            startEdgeTargets[startEdgeCount] = startTargetIdx[i];
            startEdgeCosts[startEdgeCount] = dijkstraStartCosts[i];
            startEdgeCount++;
        }
    }
    nodesExplored++;

    for (int i = 0; i < goalTargetCount; i++) {
        if (dijkstraGoalCosts[i] >= 0) {
            goalEdgeTargets[goalEdgeCount] = goalTargetIdx[i];
            goalEdgeCosts[goalEdgeCount] = dijkstraGoalCosts[i];
            goalEdgeCount++;
        }
    }
    nodesExplored++;

    // Debug: report connect phase results
    if (debugCrossZ) {
        TraceLog(LOG_INFO, "  start connects to %d entrances", startEdgeCount);
        TraceLog(LOG_INFO, "  goal connects to %d entrances", goalEdgeCount);
        // Report if any connected entrances have cross-z edges and what z-levels they reach
        int startCrossZEdges = 0;
        int startReachesUp = 0, startReachesDown = 0;
        for (int i = 0; i < startEdgeCount; i++) {
            int ent = startEdgeTargets[i];
            for (int j = 0; j < adjListCount[ent]; j++) {
                int edgeIdx = adjList[ent][j];
                int neighbor = graphEdges[edgeIdx].to;
                if (entrances[neighbor].z != entrances[ent].z) {
                    startCrossZEdges++;
                    if (entrances[neighbor].z > entrances[ent].z) startReachesUp++;
                    else startReachesDown++;
                }
            }
        }
        int goalCrossZEdges = 0;
        int goalReachesUp = 0, goalReachesDown = 0;
        for (int i = 0; i < goalEdgeCount; i++) {
            int ent = goalEdgeTargets[i];
            for (int j = 0; j < adjListCount[ent]; j++) {
                int edgeIdx = adjList[ent][j];
                int neighbor = graphEdges[edgeIdx].to;
                if (entrances[neighbor].z != entrances[ent].z) {
                    goalCrossZEdges++;
                    if (entrances[neighbor].z > entrances[ent].z) goalReachesUp++;
                    else goalReachesDown++;
                }
            }
        }
        TraceLog(LOG_INFO, "  start z=%d has %d cross-z edges (up:%d, down:%d), goal z=%d has %d (up:%d, down:%d)",
                 start.z, startCrossZEdges, startReachesUp, startReachesDown,
                 goal.z, goalCrossZEdges, goalReachesUp, goalReachesDown);
    }

    // A* on abstract graph using binary heap
    double abstractStartTime = GetTime();
    HeapInit(totalNodes);

    abstractNodes[startNode].g = 0;
    abstractNodes[startNode].f = Heuristic(start.x, start.y, goal.x, goal.y);
    abstractNodes[startNode].open = true;
    HeapPush(startNode);

    while (heap.size > 0) {
        // Pop best node from heap
        int best = HeapPop();

        if (best == goalNode) {
            // Reconstruct abstract path
            int current = goalNode;
            while (current >= 0 && abstractPathLength < MAX_ENTRANCES + 2) {
                abstractPath[abstractPathLength++] = current;
                current = abstractNodes[current].parent;
            }
            break;
        }

        abstractNodes[best].open = false;
        abstractNodes[best].closed = true;
        nodesExplored++;

        // Expand neighbors
        if (best == startNode) {
            // Expand from start to its connected entrances
            for (int i = 0; i < startEdgeCount; i++) {
                int neighbor = startEdgeTargets[i];
                if (abstractNodes[neighbor].closed) continue;
                int ng = abstractNodes[best].g + startEdgeCosts[i];
                if (ng < abstractNodes[neighbor].g) {
                    bool wasOpen = abstractNodes[neighbor].open;
                    abstractNodes[neighbor].g = ng;
                    abstractNodes[neighbor].f = ng + Heuristic(entrances[neighbor].x, entrances[neighbor].y, goal.x, goal.y);
                    abstractNodes[neighbor].parent = best;
                    abstractNodes[neighbor].open = true;
                    if (wasOpen) {
                        HeapDecreaseKey(neighbor);
                    } else {
                        HeapPush(neighbor);
                    }
                }
            }
        } else if (best < entranceCount) {
            // Expand from a regular entrance using adjacency list
            for (int i = 0; i < adjListCount[best]; i++) {
                int edgeIdx = adjList[best][i];
                int neighbor = graphEdges[edgeIdx].to;
                if (abstractNodes[neighbor].closed) continue;
                int ng = abstractNodes[best].g + graphEdges[edgeIdx].cost;
                if (ng < abstractNodes[neighbor].g) {
                    bool wasOpen = abstractNodes[neighbor].open;
                    abstractNodes[neighbor].g = ng;
                    abstractNodes[neighbor].f = ng + Heuristic(entrances[neighbor].x, entrances[neighbor].y, goal.x, goal.y);
                    abstractNodes[neighbor].parent = best;
                    abstractNodes[neighbor].open = true;
                    if (wasOpen) {
                        HeapDecreaseKey(neighbor);
                    } else {
                        HeapPush(neighbor);
                    }
                }
            }
            // Also check if this entrance can reach goal
            for (int i = 0; i < goalEdgeCount; i++) {
                if (goalEdgeTargets[i] == best) {
                    int neighbor = goalNode;
                    if (abstractNodes[neighbor].closed) continue;
                    int ng = abstractNodes[best].g + goalEdgeCosts[i];
                    if (ng < abstractNodes[neighbor].g) {
                        bool wasOpen = abstractNodes[neighbor].open;
                        abstractNodes[neighbor].g = ng;
                        abstractNodes[neighbor].f = ng;  // h=0 at goal
                        abstractNodes[neighbor].parent = best;
                        abstractNodes[neighbor].open = true;
                        if (wasOpen) {
                            HeapDecreaseKey(neighbor);
                        } else {
                            HeapPush(neighbor);
                        }
                    }
                }
            }
        }
    }
    hpaAbstractTime = (GetTime() - abstractStartTime) * 1000.0;

    // Debug: report abstract search result
    if (debugCrossZ) {
        if (abstractPathLength > 0) {
            TraceLog(LOG_INFO, "  abstract path found, length: %d", abstractPathLength);
        } else {
            TraceLog(LOG_INFO, "  FAILED: abstract search found no path");
        }
    }

    // Now refine abstract path to cell-level path
    double refineStartTime = GetTime();
    if (abstractPathLength > 0) {
        // Abstract path is in reverse order (goal to start)
        // We need to walk it forward and reconstruct local paths
        Point tempPath[MAX_PATH];

        for (int i = abstractPathLength - 1; i > 0; i--) {
            int fromNode = abstractPath[i];
            int toNode = abstractPath[i - 1];

            int fx, fy, fz, tx, ty, tz;

            // Get coordinates for from node
            if (fromNode == startNode) {
                fx = start.x;
                fy = start.y;
                fz = start.z;
            } else {
                fx = entrances[fromNode].x;
                fy = entrances[fromNode].y;
                fz = entrances[fromNode].z;
            }

            // Get coordinates for to node
            if (toNode == goalNode) {
                tx = goal.x;
                ty = goal.y;
                tz = goal.z;
            } else {
                tx = entrances[toNode].x;
                ty = entrances[toNode].y;
                tz = entrances[toNode].z;
            }

            // Check if this is a ladder transition (z-level change)
            if (fz != tz) {
                // Ladder transition: just add the destination point (same x,y, different z)
                // The mover will handle the actual ladder climbing
                if (resultLen < maxLen) {
                    outPath[resultLen++] = (Point){tx, ty, tz};
                }
                continue;
            }

            // Reconstruct local path for this segment (same z-level)
            int localLen = ReconstructLocalPath(fx, fy, fz, tx, ty, tz, tempPath, MAX_PATH);

            if (localLen == 0) {
                // No path found for this segment - shouldn't happen with valid abstract path
                // (If this triggers, the expanded bounds in ReconstructLocalPath may need adjustment)
                continue;
            }

            // tempPath is in reverse order: [0]=destination, [localLen-1]=source
            // We iterate from source to destination (high index to low)
            // Skip source point for subsequent segments (it's the destination of previous segment)
            int skipSource = (i == abstractPathLength - 1) ? 0 : 1;
            for (int j = localLen - 1 - skipSource; j >= 0 && resultLen < maxLen; j--) {
                outPath[resultLen++] = tempPath[j];
            }
        }

        // Reverse path so it goes from goal to start (matching RunAStar behavior)
        for (int i = 0; i < resultLen / 2; i++) {
            Point tmp = outPath[i];
            outPath[i] = outPath[resultLen - 1 - i];
            outPath[resultLen - 1 - i] = tmp;
        }
    }
    hpaRefinementTime = (GetTime() - refineStartTime) * 1000.0;

    // Debug: report refinement result
    if (debugCrossZ && abstractPathLength > 0) {
        if (resultLen > 0) {
            TraceLog(LOG_INFO, "  refined path length: %d", resultLen);
        } else {
            TraceLog(LOG_INFO, "  FAILED: refinement returned 0");
        }
    }

    lastPathTime = (GetTime() - startTime) * 1000.0;
    return resultLen;
}

// Unified path finding function that dispatches to the selected algorithm
int FindPath(PathAlgorithm algo, Point start, Point goal, Point* outPath, int maxLen) {
    if (start.x < 0 || goal.x < 0) return 0;
    
    // Save globals so mover pathfinding doesn't pollute the debug visualization
    Point savedStart = startPos;
    Point savedGoal = goalPos;
    int savedPathLength = pathLength;
    
    // Set globals for algorithms that need them
    startPos = start;
    goalPos = goal;
    
    int len = 0;
    bool usesGlobalPath = true;
    
    switch (algo) {
        case PATH_ALGO_ASTAR:
            RunAStar();
            break;
        case PATH_ALGO_HPA:
            len = FindPathHPA(start, goal, outPath, maxLen);
            usesGlobalPath = false;
            break;
    }
    
    // Copy from global path array to output (for algorithms that use globals)
    if (usesGlobalPath) {
        len = (pathLength < maxLen) ? pathLength : maxLen;
        for (int i = 0; i < len; i++) {
            outPath[i] = path[i];
        }
    }
    
    // Restore globals so debug visualization isn't affected by mover pathfinding
    startPos = savedStart;
    goalPos = savedGoal;
    pathLength = savedPathLength;
    
    // Warn if path length is near capacity (could indicate truncation)
    if (len > 0 && len >= maxLen * 9 / 10) {
        TraceLog(LOG_WARNING, "Path near MAX_PATH limit: %d/%d (%.0f%%) from (%d,%d,%d) to (%d,%d,%d)",
                 len, maxLen, (float)len / maxLen * 100.0f,
                 start.x, start.y, start.z, goal.x, goal.y, goal.z);
    }

    // Track stats
    statsPathCount++;
    statsTotalTime += lastPathTime;
    
    return len;
}

// Update path stats - call this every frame, reports every 5 seconds
void UpdatePathStats(void) {
    double now = GetTime();
    if (statsLastReportTime == 0.0) {
        statsLastReportTime = now;
    }
    
    if (now - statsLastReportTime >= 5.0) {
        pathStatsCount = statsPathCount;
        pathStatsTotalMs = statsTotalTime;
        pathStatsAvgMs = (statsPathCount > 0) ? (statsTotalTime / statsPathCount) : 0.0;
        pathStatsUpdated = true;  // Signal that new stats are available
        
        // Reset for next period
        statsPathCount = 0;
        statsTotalTime = 0.0;
        statsLastReportTime = now;
    }
}

// Reset path stats (call when switching algorithms)
void ResetPathStats(void) {
    statsPathCount = 0;
    statsTotalTime = 0.0;
    statsLastReportTime = 0.0;
    pathStatsCount = 0;
    pathStatsTotalMs = 0.0;
    pathStatsAvgMs = 0.0;
}

// Wrapper that uses globals (for backward compatibility)
void RunHPAStar(void) {
    pathLength = FindPathHPA(startPos, goalPos, path, MAX_PATH);
}

void SeedRandom(unsigned int seed) {
    SetRandomSeed(seed);
}

// Check if a z-level has any ladder connections
static bool ZLevelHasLadderLinks(int z) {
    for (int i = 0; i < ladderLinkCount; i++) {
        if (ladderLinks[i].zLow == z || ladderLinks[i].zHigh == z) {
            return true;
        }
    }
    return false;
}

// Check if a z-level has any ramp connections
static bool ZLevelHasRampLinks(int z) {
    for (int i = 0; i < rampLinkCount; i++) {
        // Ramp is at rampZ, exit is at rampZ+1
        if (rampLinks[i].rampZ == z || rampLinks[i].rampZ + 1 == z) {
            return true;
        }
    }
    return false;
}

// Check if a z-level is reachable via ladders or ramps
static bool ZLevelIsConnected(int z) {
    return ZLevelHasLadderLinks(z) || ZLevelHasRampLinks(z);
}

Point GetRandomWalkableCell(void) {
    Point p;
    // Check z-level connectivity when vertical connections exist
    bool checkConnectivity = (ladderLinkCount > 0 || rampLinkCount > 0);
    for (int attempts = 0; attempts < 1000; attempts++) {
        p.x = GetRandomValue(0, gridWidth - 1);
        p.y = GetRandomValue(0, gridHeight - 1);
        p.z = GetRandomValue(0, gridDepth - 1);
        // Skip z-levels with no vertical connections (stranded spawns)
        if (checkConnectivity && !ZLevelIsConnected(p.z)) continue;
        // Use NavIsValidDestination to filter wall-tops and other non-goal cells
        if (NavIsValidDestination(p.x, p.y, p.z)) {
            return p;
        }
    }
    return (Point){-1, -1, 0};
}

Point GetRandomWalkableCellDifferentZ(int excludeZ) {
    Point p;
    // Check z-level connectivity when vertical connections exist
    bool checkConnectivity = (ladderLinkCount > 0 || rampLinkCount > 0);
    for (int attempts = 0; attempts < 1000; attempts++) {
        p.x = GetRandomValue(0, gridWidth - 1);
        p.y = GetRandomValue(0, gridHeight - 1);
        p.z = GetRandomValue(0, gridDepth - 1);
        // Skip z-levels with no vertical connections (stranded spawns)
        if (checkConnectivity && !ZLevelIsConnected(p.z)) continue;
        if (p.z != excludeZ && NavIsValidDestination(p.x, p.y, p.z)) {
            return p;
        }
    }
    // Fallback to any walkable cell if no different-z cell found
    return GetRandomWalkableCell();
}

// NOTE: This function is called for same-z goals, so no ladder check needed.
// If mover is already on a z-level, they can reach any walkable cell on that level.
Point GetRandomWalkableCellOnZ(int z) {
    Point p;
    p.z = z;
    for (int attempts = 0; attempts < 1000; attempts++) {
        p.x = GetRandomValue(0, gridWidth - 1);
        p.y = GetRandomValue(0, gridHeight - 1);
        if (NavIsValidDestination(p.x, p.y, p.z)) {
            return p;
        }
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

    // BFS queue (reuse reachable array + separate head/tail)
    static Point queue[REACHABLE_MAX];
    int qHead = 0, qTail = 0;
    int found = 0;

    queue[qTail++] = (Point){startX, startY, startZ};
    visited[startY][startX] = true;

    while (qHead < qTail && found < REACHABLE_MAX) {
        Point cur = queue[qHead++];
        reachable[found++] = cur;

        // 4-directional neighbors
        static const int dx[] = {1, -1, 0, 0};
        static const int dy[] = {0, 0, 1, -1};
        for (int d = 0; d < 4; d++) {
            int nx = cur.x + dx[d];
            int ny = cur.y + dy[d];
            if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
            if (visited[ny][nx]) continue;
            if (!NavIsWalkable(nx, ny, cur.z)) continue;
            visited[ny][nx] = true;
            if (qTail < REACHABLE_MAX) {
                queue[qTail++] = (Point){nx, ny, cur.z};
            }
        }
    }

    if (found <= 1) return (Point){startX, startY, startZ};
    // Pick random, but not the start cell
    int pick = GetRandomValue(1, found - 1);
    return reachable[pick];
}
