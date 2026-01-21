#include "pathfinding.h"
#include "../vendor/raylib.h"
#include <stdlib.h>

#define COST_INF 999999

// State
Entrance entrances[MAX_ENTRANCES];
int entranceCount = 0;
GraphEdge graphEdges[MAX_EDGES];
int graphEdgeCount = 0;
LadderLink ladderLinks[MAX_LADDERS];
int ladderLinkCount = 0;

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
#define ENTRANCE_HASH_SIZE 8192  // Power of 2, should be > 2x max entrances

typedef struct {
    int x, y;       // Position (key)
    int index;      // Entrance index (value), -1 if empty
} EntranceHashEntry;

static EntranceHashEntry entranceHash[ENTRANCE_HASH_SIZE];
static bool entranceHashBuilt = false;

// Simple hash function for (x, y) coordinates
static inline int HashPosition(int x, int y) {
    // Combine x and y with prime multipliers, mask to table size
    unsigned int h = (unsigned int)(x * 73856093) ^ (unsigned int)(y * 19349663);
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
        int h = HashPosition(entrances[i].x, entrances[i].y);
        // Linear probing for collision resolution
        while (entranceHash[h].index >= 0) {
            h = (h + 1) & (ENTRANCE_HASH_SIZE - 1);
        }
        entranceHash[h].x = entrances[i].x;
        entranceHash[h].y = entrances[i].y;
        entranceHash[h].index = i;
    }
    entranceHashBuilt = true;
}

// Look up entrance index by position, returns -1 if not found
static int HashLookupEntrance(int x, int y) {
    int h = HashPosition(x, y);
    int start = h;
    while (entranceHash[h].index >= 0) {
        if (entranceHash[h].x == x && entranceHash[h].y == y) {
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
        if (nodeData[0][cy][cx].f < nodeData[0][py][px].f) {
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
        int smallestF = nodeData[0][sy][sx].f;

        if (left < chunkHeap.size) {
            int lx = UnpackX(chunkHeap.nodes[left]);
            int ly = UnpackY(chunkHeap.nodes[left]);
            if (nodeData[0][ly][lx].f < smallestF) {
                smallest = left;
                smallestF = nodeData[0][ly][lx].f;
            }
        }
        if (right < chunkHeap.size) {
            int rx = UnpackX(chunkHeap.nodes[right]);
            int ry = UnpackY(chunkHeap.nodes[right]);
            if (nodeData[0][ry][rx].f < smallestF) {
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
static int Heuristic8Dir(int x1, int y1, int x2, int y2) {
    int dx = abs(x1 - x2);
    int dy = abs(y1 - y2);
    int minD = dx < dy ? dx : dy;
    int maxD = dx > dy ? dx : dy;
    return 10 * maxD + 4 * minD;
}

void MarkChunkDirty(int cellX, int cellY) {
    int cx = cellX / chunkWidth;
    int cy = cellY / chunkHeight;
    if (cx >= 0 && cx < chunksX && cy >= 0 && cy < chunksY) {
        chunkDirty[0][cy][cx] = true;
        needsRebuild = true;
        hpaNeedsRebuild = true;
        jpsNeedsRebuild = true;
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

// Helper to check if a cell is walkable (CELL_WALKABLE, CELL_FLOOR, or CELL_LADDER)
static bool IsCellWalkable(int z, int y, int x) {
    if (z < 0 || z >= gridDepth) return false;
    if (y < 0 || y >= gridHeight) return false;
    if (x < 0 || x >= gridWidth) return false;
    CellType cell = grid[z][y][x];
    return cell == CELL_WALKABLE || cell == CELL_FLOOR || cell == CELL_LADDER;
}

void BuildEntrances(void) {
    entranceCount = 0;
    ladderLinkCount = 0;
    
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
                    bool open = (IsCellWalkable(z, borderY - 1, x) && IsCellWalkable(z, borderY, x));
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
                    bool open = (IsCellWalkable(z, y, borderX - 1) && IsCellWalkable(z, y, borderX));
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
    
    // Detect ladders and create ladder links
    // A ladder link connects two z-levels where both have CELL_LADDER at the same (x,y)
    for (int z = 0; z < gridDepth - 1; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (grid[z][y][x] == CELL_LADDER && grid[z + 1][y][x] == CELL_LADDER) {
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
                                .cost = 10  // Same cost as one step
                            };
                        }
                    }
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
        nodeData[sz][sy][sx].f = Heuristic(sx, sy, gx, gy) * 10;
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
            if (!IsCellWalkable(sz, ny, nx) || nodeData[sz][ny][nx].closed) continue;

            // Prevent corner cutting for diagonal movement
            if (use8Dir && dx[i] != 0 && dy[i] != 0) {
                int adjX = bestX + dx[i], adjY = bestY + dy[i];
                // Check bounds before accessing grid for corner-cut check
                if (adjX < 0 || adjX >= gridWidth || adjY < 0 || adjY >= gridHeight)
                    continue;
                if (!IsCellWalkable(sz, bestY, adjX) || !IsCellWalkable(sz, adjY, bestX))
                    continue;
            }

            int moveCost = (dx[i] != 0 && dy[i] != 0) ? 14 : 10;
            int ng = nodeData[sz][bestY][bestX].g + moveCost;
            if (ng < nodeData[sz][ny][nx].g) {
                bool wasOpen = nodeData[sz][ny][nx].open;
                nodeData[sz][ny][nx].g = ng;
                if (use8Dir) {
                    nodeData[sz][ny][nx].f = ng + Heuristic8Dir(nx, ny, gx, gy);
                } else {
                    nodeData[sz][ny][nx].f = ng + Heuristic(nx, ny, gx, gy) * 10;
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
            if (!IsCellWalkable(sz, ny, nx) || nodeData[sz][ny][nx].closed) continue;

            // Prevent corner cutting for diagonal movement
            if (use8Dir && dx[i] != 0 && dy[i] != 0) {
                int adjX = bestX + dx[i], adjY = bestY + dy[i];
                // Check bounds before accessing grid for corner-cut check
                if (adjX < 0 || adjX >= gridWidth || adjY < 0 || adjY >= gridHeight)
                    continue;
                if (!IsCellWalkable(sz, bestY, adjX) || !IsCellWalkable(sz, adjY, bestX))
                    continue;
            }

            int moveCost = (dx[i] != 0 && dy[i] != 0) ? 14 : 10;
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
    
    TraceLog(LOG_INFO, "Built graph: %d edges (%d ladder links) in %.2fms", 
             graphEdgeCount, ladderLinkCount, (GetTime() - startTime) * 1000);
}

// ============== Incremental Update Functions ==============

// Get the set of chunks affected by dirty chunks (dirty + their neighbors)
static void GetAffectedChunks(bool affectedChunks[MAX_CHUNKS_Y][MAX_CHUNKS_X]) {
    for (int cy = 0; cy < chunksY; cy++)
        for (int cx = 0; cx < chunksX; cx++)
            affectedChunks[cy][cx] = false;

    for (int cy = 0; cy < chunksY; cy++) {
        for (int cx = 0; cx < chunksX; cx++) {
            if (chunkDirty[0][cy][cx]) {
                affectedChunks[cy][cx] = true;
                // Mark neighbors as affected too (they share borders)
                if (cy > 0) affectedChunks[cy-1][cx] = true;
                if (cy < chunksY-1) affectedChunks[cy+1][cx] = true;
                if (cx > 0) affectedChunks[cy][cx-1] = true;
                if (cx < chunksX-1) affectedChunks[cy][cx+1] = true;
            }
        }
    }
}

// Check if an entrance touches any affected chunk
// Note: affectedChunks is 2D (XY only), so we extract XY from 3D chunk IDs
static bool EntranceTouchesAffected(int entranceIdx, bool affectedChunks[MAX_CHUNKS_Y][MAX_CHUNKS_X]) {
    int chunksPerLevel = chunksX * chunksY;
    int c1 = entrances[entranceIdx].chunk1;
    int c2 = entrances[entranceIdx].chunk2;
    // Extract XY chunk indices (ignore z component)
    int xyChunk1 = c1 % chunksPerLevel;
    int xyChunk2 = c2 % chunksPerLevel;
    int cy1 = xyChunk1 / chunksX, cx1 = xyChunk1 % chunksX;
    int cy2 = xyChunk2 / chunksX, cx2 = xyChunk2 % chunksX;
    return affectedChunks[cy1][cx1] || affectedChunks[cy2][cx2];
}

// Rebuild entrances for affected chunks (simpler approach - no keeping/remapping)
static void RebuildAffectedEntrances(bool affectedChunks[MAX_CHUNKS_Y][MAX_CHUNKS_X]) {
    // Remove entrances that touch any affected chunk
    int newCount = 0;
    for (int i = 0; i < entranceCount; i++) {
        if (!EntranceTouchesAffected(i, affectedChunks)) {
            entrances[newCount++] = entrances[i];
        }
    }
    int keptCount = newCount;
    
    int chunksPerLevel = chunksX * chunksY;

    // Rebuild entrances for all z-levels where at least one chunk is affected
    for (int z = 0; z < gridDepth; z++) {
        // Horizontal borders (between cy and cy+1)
        for (int cy = 0; cy < chunksY - 1; cy++) {
            for (int cx = 0; cx < chunksX; cx++) {
                if (!affectedChunks[cy][cx] && !affectedChunks[cy+1][cx]) continue;

                int borderY = (cy + 1) * chunkHeight;
                int startX = cx * chunkWidth;
                int chunk1 = z * chunksPerLevel + cy * chunksX + cx;
                int chunk2 = z * chunksPerLevel + (cy + 1) * chunksX + cx;
                int runStart = -1;

                for (int i = 0; i < chunkWidth; i++) {
                    int x = startX + i;
                    bool open = (IsCellWalkable(z, borderY - 1, x) && IsCellWalkable(z, borderY, x));
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
                if (!affectedChunks[cy][cx] && !affectedChunks[cy][cx+1]) continue;

                int borderX = (cx + 1) * chunkWidth;
                int startY = cy * chunkHeight;
                int chunk1 = z * chunksPerLevel + cy * chunksX + cx;
                int chunk2 = z * chunksPerLevel + cy * chunksX + (cx + 1);
                int runStart = -1;

                for (int i = 0; i < chunkHeight; i++) {
                    int y = startY + i;
                    bool open = (IsCellWalkable(z, y, borderX - 1) && IsCellWalkable(z, y, borderX));
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
    
    // NOTE: Ladder entrances are not rebuilt incrementally yet.
    // For full ladder support, ladderLinkCount would need to be rebuilt here too.
    // For now, incremental updates don't affect ladders (which is fine if ladders don't change).

    entranceCount = newCount;
    TraceLog(LOG_INFO, "Incremental entrances: kept %d, rebuilt to %d total", keptCount, newCount);
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

// Check if an entrance (by new index) touches any affected chunk
// Note: affectedChunks is 2D (XY only), so we extract XY from 3D chunk IDs
static bool NewEntranceTouchesAffected(int idx, bool affectedChunks[MAX_CHUNKS_Y][MAX_CHUNKS_X]) {
    int chunksPerLevel = chunksX * chunksY;
    int c1 = entrances[idx].chunk1;
    int c2 = entrances[idx].chunk2;
    // Extract XY chunk indices (ignore z component)
    int xyChunk1 = c1 % chunksPerLevel;
    int xyChunk2 = c2 % chunksPerLevel;
    int cy1 = xyChunk1 / chunksX, cx1 = xyChunk1 % chunksX;
    int cy2 = xyChunk2 / chunksX, cx2 = xyChunk2 % chunksX;
    return affectedChunks[cy1][cx1] || affectedChunks[cy2][cx2];
}

// Rebuild graph edges - keep edges in unaffected chunks, rebuild affected
static void RebuildAffectedEdges(bool affectedChunks[MAX_CHUNKS_Y][MAX_CHUNKS_X]) {
    // Step 0: Build indexes for fast lookup
    // - Hash table: entrance position → index (for old→new mapping)
    // - Chunk index: chunk → list of entrance indices (for Step 3)
    BuildEntranceHash();
    BuildChunkEntranceIndex();

    for (int i = 0; i < oldEntranceCount; i++) {
        oldToNewEntranceIndex[i] = HashLookupEntrance(oldEntrances[i].x, oldEntrances[i].y);
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
    int dijkstraCalls = 0;
    int chunksPerLevel = chunksX * chunksY;

    for (int z = 0; z < gridDepth; z++) {
        for (int cy = 0; cy < chunksY; cy++) {
            for (int cx = 0; cx < chunksX; cx++) {
                int chunk = z * chunksPerLevel + cy * chunksX + cx;
                int numEnts = chunkEntranceCount[chunk];
                if (numEnts == 0) continue;

                // Skip chunks that don't need processing
                // Note: affectedChunks is 2D (XY only), same XY affects all z-levels
                if (!affectedChunks[cy][cx]) {
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
                    dijkstraCalls++;
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

    TraceLog(LOG_INFO, "Incremental edges: kept %d, total now %d, dijkstra calls=%d",
             keptEdges, graphEdgeCount, dijkstraCalls);
}

void UpdateDirtyChunks(void) {
    // Check if any chunks are dirty
    bool anyDirty = false;
    for (int cy = 0; cy < chunksY && !anyDirty; cy++)
        for (int cx = 0; cx < chunksX && !anyDirty; cx++)
            if (chunkDirty[0][cy][cx]) anyDirty = true;

    if (!anyDirty) return;

    double startTime = GetTime();

    // Get affected chunks (dirty + neighbors)
    bool affectedChunks[MAX_CHUNKS_Y][MAX_CHUNKS_X];
    GetAffectedChunks(affectedChunks);

    int dirtyCount = 0, affectedCount = 0;
    for (int cy = 0; cy < chunksY; cy++) {
        for (int cx = 0; cx < chunksX; cx++) {
            if (chunkDirty[0][cy][cx]) dirtyCount++;
            if (affectedChunks[cy][cx]) affectedCount++;
        }
    }

    // Save old entrances for edge remapping
    SaveOldEntrances();

    // Rebuild entrances for affected chunks
    RebuildAffectedEntrances(affectedChunks);

    // Rebuild edges (keeps edges in unaffected chunks, rebuilds affected)
    RebuildAffectedEdges(affectedChunks);

    // Clear dirty flags
    for (int cy = 0; cy < chunksY; cy++)
        for (int cx = 0; cx < chunksX; cx++)
            chunkDirty[0][cy][cx] = false;
    needsRebuild = false;
    hpaNeedsRebuild = false;

    double elapsed = (GetTime() - startTime) * 1000.0;
    TraceLog(LOG_INFO, "Incremental update: %d dirty, %d affected chunks in %.2fms",
             dirtyCount, affectedCount, elapsed);
}

// 3D heuristic - includes z-level difference
static int Heuristic3D(int x0, int y0, int z0, int x1, int y1, int z1) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int dz = abs(z1 - z0);
    // Manhattan on XY plane plus z-level cost
    // Treat z-transition as equivalent to 10 (one step)
    if (use8Dir) {
        int maxXY = (dx > dy) ? dx : dy;
        int minXY = (dx < dy) ? dx : dy;
        return (maxXY - minXY) * 10 + minXY * 14 + dz * 10;
    } else {
        return (dx + dy + dz) * 10;
    }
}

void RunAStar(void) {
    if (startPos.x < 0 || goalPos.x < 0) return;
    pathLength = 0;
    nodesExplored = 0;
    double startTime = GetTime();

    // Initialize nodeData for all z-levels
    for (int z = 0; z < gridDepth; z++)
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++)
                nodeData[z][y][x] = (AStarNode){COST_INF, COST_INF, -1, -1, -1, false, false};

    int startZ = startPos.z;
    nodeData[startZ][startPos.y][startPos.x].g = 0;
    nodeData[startZ][startPos.y][startPos.x].f = Heuristic3D(startPos.x, startPos.y, startZ, goalPos.x, goalPos.y, goalPos.z);
    nodeData[startZ][startPos.y][startPos.x].open = true;

    // Direction arrays for XY movement
    int dx4[] = {0, 1, 0, -1};
    int dy4[] = {-1, 0, 1, 0};
    int dx8[] = {0, 1, 1, 1, 0, -1, -1, -1};
    int dy8[] = {-1, -1, 0, 1, 1, 1, 0, -1};

    int* dx = use8Dir ? dx8 : dx4;
    int* dy = use8Dir ? dy8 : dy4;
    int numDirs = use8Dir ? 8 : 4;

    while (1) {
        // Find best open node across all z-levels
        int bestX = -1, bestY = -1, bestZ = -1, bestF = COST_INF;
        for (int z = 0; z < gridDepth; z++)
            for (int y = 0; y < gridHeight; y++)
                for (int x = 0; x < gridWidth; x++)
                    if (nodeData[z][y][x].open && nodeData[z][y][x].f < bestF) {
                        bestF = nodeData[z][y][x].f;
                        bestX = x;
                        bestY = y;
                        bestZ = z;
                    }
        if (bestX < 0) break;
        
        // Check if we reached the goal (must match z too!)
        if (bestX == goalPos.x && bestY == goalPos.y && bestZ == goalPos.z) {
            int cx = goalPos.x, cy = goalPos.y, cz = goalPos.z;
            while (cx >= 0 && cy >= 0 && cz >= 0 && pathLength < MAX_PATH) {
                path[pathLength++] = (Point){cx, cy, cz};
                int px = nodeData[cz][cy][cx].parentX;
                int py = nodeData[cz][cy][cx].parentY;
                int pz = nodeData[cz][cy][cx].parentZ;
                cx = px; cy = py; cz = pz;
            }
            break;
        }
        
        nodeData[bestZ][bestY][bestX].open = false;
        nodeData[bestZ][bestY][bestX].closed = true;
        nodesExplored++;
        
        // Expand XY neighbors on same z-level
        for (int i = 0; i < numDirs; i++) {
            int nx = bestX + dx[i], ny = bestY + dy[i], nz = bestZ;
            if (!IsCellWalkable(nz, ny, nx)) continue;
            if (nodeData[nz][ny][nx].closed) continue;

            // For diagonal movement, check that we can actually move diagonally
            // (not cutting corners through walls)
            if (use8Dir && dx[i] != 0 && dy[i] != 0) {
                if (!IsCellWalkable(bestZ, bestY, bestX + dx[i]) || 
                    !IsCellWalkable(bestZ, bestY + dy[i], bestX))
                    continue;
            }

            // Cost: 10 for cardinal, 14 for diagonal
            int moveCost = (dx[i] != 0 && dy[i] != 0) ? 14 : 10;
            int ng = nodeData[bestZ][bestY][bestX].g + moveCost;

            if (ng < nodeData[nz][ny][nx].g) {
                nodeData[nz][ny][nx].g = ng;
                nodeData[nz][ny][nx].f = ng + Heuristic3D(nx, ny, nz, goalPos.x, goalPos.y, goalPos.z);
                nodeData[nz][ny][nx].parentX = bestX;
                nodeData[nz][ny][nx].parentY = bestY;
                nodeData[nz][ny][nx].parentZ = bestZ;
                nodeData[nz][ny][nx].open = true;
            }
        }
        
        // Expand Z neighbors (ladder connections)
        if (grid[bestZ][bestY][bestX] == CELL_LADDER) {
            // Try going up (z+1)
            int nz = bestZ + 1;
            if (nz < gridDepth && grid[nz][bestY][bestX] == CELL_LADDER) {
                if (!nodeData[nz][bestY][bestX].closed) {
                    int moveCost = 10;  // Same cost as cardinal move
                    int ng = nodeData[bestZ][bestY][bestX].g + moveCost;
                    if (ng < nodeData[nz][bestY][bestX].g) {
                        nodeData[nz][bestY][bestX].g = ng;
                        nodeData[nz][bestY][bestX].f = ng + Heuristic3D(bestX, bestY, nz, goalPos.x, goalPos.y, goalPos.z);
                        nodeData[nz][bestY][bestX].parentX = bestX;
                        nodeData[nz][bestY][bestX].parentY = bestY;
                        nodeData[nz][bestY][bestX].parentZ = bestZ;
                        nodeData[nz][bestY][bestX].open = true;
                    }
                }
            }
            // Try going down (z-1)
            nz = bestZ - 1;
            if (nz >= 0 && grid[nz][bestY][bestX] == CELL_LADDER) {
                if (!nodeData[nz][bestY][bestX].closed) {
                    int moveCost = 10;
                    int ng = nodeData[bestZ][bestY][bestX].g + moveCost;
                    if (ng < nodeData[nz][bestY][bestX].g) {
                        nodeData[nz][bestY][bestX].g = ng;
                        nodeData[nz][bestY][bestX].f = ng + Heuristic3D(bestX, bestY, nz, goalPos.x, goalPos.y, goalPos.z);
                        nodeData[nz][bestY][bestX].parentX = bestX;
                        nodeData[nz][bestY][bestX].parentY = bestY;
                        nodeData[nz][bestY][bestX].parentZ = bestZ;
                        nodeData[nz][bestY][bestX].open = true;
                    }
                }
            }
        }
    }
    lastPathTime = (GetTime() - startTime) * 1000.0;
    TraceLog(LOG_INFO, "A* 3D (%s): time=%.2fms, nodes=%d, path=%d",
             use8Dir ? "8-dir" : "4-dir", lastPathTime, nodesExplored, pathLength);
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
        nodeData[sz][sy][sx].f = Heuristic(sx, sy, gx, gy) * 10;
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
            if (!IsCellWalkable(sz, ny, nx) || nodeData[sz][ny][nx].closed) continue;

            // Prevent corner cutting for diagonal movement
            if (use8Dir && dx[i] != 0 && dy[i] != 0) {
                int adjX = bestX + dx[i], adjY = bestY + dy[i];
                if (adjX < 0 || adjX >= gridWidth || adjY < 0 || adjY >= gridHeight)
                    continue;
                if (!IsCellWalkable(sz, bestY, adjX) || !IsCellWalkable(sz, adjY, bestX))
                    continue;
            }

            int moveCost = (dx[i] != 0 && dy[i] != 0) ? 14 : 10;
            int ng = nodeData[sz][bestY][bestX].g + moveCost;
            if (ng < nodeData[sz][ny][nx].g) {
                bool wasOpen = nodeData[sz][ny][nx].open;
                nodeData[sz][ny][nx].g = ng;
                if (use8Dir) {
                    nodeData[sz][ny][nx].f = ng + Heuristic8Dir(nx, ny, gx, gy);
                } else {
                    nodeData[sz][ny][nx].f = ng + Heuristic(nx, ny, gx, gy) * 10;
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

    // Special case: start and goal in same chunk (and same z-level) - just do local A*
    if (startChunk == goalChunk) {
        int minX, minY, maxX, maxY, chunkZ;
        GetChunkBounds(startChunk, &minX, &minY, &maxX, &maxY, &chunkZ);
        resultLen = ReconstructLocalPath(start.x, start.y, start.z, 
                                         goal.x, goal.y, goal.z, outPath, maxLen);
        lastPathTime = (GetTime() - startTime) * 1000.0;
        return resultLen;
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

    // ==========================================================================
    // CONNECT PHASE: Find costs from start/goal to all entrances in their chunks
    //
    // Currently comparing three approaches (TODO: pick one for production):
    //   1. Multi-target Dijkstra - single search, no heuristic, finds all targets
    //   2. N x A* - separate search per target with heuristic guidance
    //   3. (TODO) Add third approach here
    //
    // Results so far: Dijkstra is faster and simpler. A* heuristic doesn't help
    // much when targets are scattered around chunk borders.
    // ==========================================================================

    // ========== APPROACH 1: Multi-target Dijkstra ==========
    // double dijkstraStartTime = GetTime();
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
    // double dijkstraTime = (GetTime() - dijkstraStartTime) * 1000.0;

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

    lastPathTime = (GetTime() - startTime) * 1000.0;
    return resultLen;
}

// Wrapper that uses globals (for backward compatibility)
void RunHPAStar(void) {
    pathLength = FindPathHPA(startPos, goalPos, path, MAX_PATH);
}

// ============== JPS Implementation ==============

static bool IsWalkable(int x, int y) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight) return false;
    CellType cell = grid[0][y][x];
    return cell == CELL_WALKABLE || cell == CELL_FLOOR || cell == CELL_LADDER;
}

// Jump in a cardinal direction (4-dir or 8-dir)
static bool Jump(int x, int y, int dx, int dy, int gx, int gy, int* jx, int* jy) {
    int nx = x + dx;
    int ny = y + dy;

    if (!IsWalkable(nx, ny)) return false;

    if (nx == gx && ny == gy) {
        *jx = nx;
        *jy = ny;
        return true;
    }

    // Diagonal movement
    if (dx != 0 && dy != 0) {
        // Check for forced neighbors
        if ((!IsWalkable(nx - dx, ny) && IsWalkable(nx - dx, ny + dy)) ||
            (!IsWalkable(nx, ny - dy) && IsWalkable(nx + dx, ny - dy))) {
            *jx = nx;
            *jy = ny;
            return true;
        }
        // Recursively jump in cardinal directions
        int tempX, tempY;
        if (Jump(nx, ny, dx, 0, gx, gy, &tempX, &tempY)) {
            *jx = nx;
            *jy = ny;
            return true;
        }
        if (Jump(nx, ny, 0, dy, gx, gy, &tempX, &tempY)) {
            *jx = nx;
            *jy = ny;
            return true;
        }
    } else {
        // Horizontal movement
        if (dx != 0) {
            if ((!IsWalkable(nx, ny + 1) && IsWalkable(nx + dx, ny + 1)) ||
                (!IsWalkable(nx, ny - 1) && IsWalkable(nx + dx, ny - 1))) {
                *jx = nx;
                *jy = ny;
                return true;
            }
        }
        // Vertical movement
        if (dy != 0) {
            if ((!IsWalkable(nx + 1, ny) && IsWalkable(nx + 1, ny + dy)) ||
                (!IsWalkable(nx - 1, ny) && IsWalkable(nx - 1, ny + dy))) {
                *jx = nx;
                *jy = ny;
                return true;
            }
        }
    }

    // Continue jumping in this direction
    return Jump(nx, ny, dx, dy, gx, gy, jx, jy);
}

void RunJPS(void) {
    if (startPos.x < 0 || goalPos.x < 0) return;

    pathLength = 0;
    nodesExplored = 0;
    double startTime = GetTime();

    // Initialize node data
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++)
            nodeData[0][y][x] = (AStarNode){COST_INF, COST_INF, -1, -1, 0, false, false};

    nodeData[0][startPos.y][startPos.x].g = 0;
    if (use8Dir) {
        nodeData[0][startPos.y][startPos.x].f = Heuristic8Dir(startPos.x, startPos.y, goalPos.x, goalPos.y);
    } else {
        nodeData[0][startPos.y][startPos.x].f = Heuristic(startPos.x, startPos.y, goalPos.x, goalPos.y) * 10;
    }
    nodeData[0][startPos.y][startPos.x].open = true;

    // Direction arrays
    int dx4[] = {0, 1, 0, -1};
    int dy4[] = {-1, 0, 1, 0};
    int dx8[] = {0, 1, 1, 1, 0, -1, -1, -1};
    int dy8[] = {-1, -1, 0, 1, 1, 1, 0, -1};

    int* dx = use8Dir ? dx8 : dx4;
    int* dy = use8Dir ? dy8 : dy4;
    int numDirs = use8Dir ? 8 : 4;

    while (1) {
        // Find node with lowest f
        int bestX = -1, bestY = -1, bestF = COST_INF;
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++)
                if (nodeData[0][y][x].open && nodeData[0][y][x].f < bestF) {
                    bestF = nodeData[0][y][x].f;
                    bestX = x;
                    bestY = y;
                }

        if (bestX < 0) break;  // No path

        if (bestX == goalPos.x && bestY == goalPos.y) {
            // Reconstruct path
            int cx = goalPos.x, cy = goalPos.y;
            while (cx >= 0 && cy >= 0 && pathLength < MAX_PATH) {
                path[pathLength++] = (Point){cx, cy, 0};
                int px = nodeData[0][cy][cx].parentX;
                int py = nodeData[0][cy][cx].parentY;

                // For JPS, we need to fill in intermediate points
                if (px >= 0 && py >= 0) {
                    int stepX = (px > cx) ? 1 : (px < cx) ? -1 : 0;
                    int stepY = (py > cy) ? 1 : (py < cy) ? -1 : 0;
                    int ix = cx + stepX;
                    int iy = cy + stepY;
                    while ((ix != px || iy != py) && pathLength < MAX_PATH) {
                        path[pathLength++] = (Point){ix, iy, 0};
                        ix += stepX;
                        iy += stepY;
                    }
                }
                cx = px;
                cy = py;
            }
            break;
        }

        nodeData[0][bestY][bestX].open = false;
        nodeData[0][bestY][bestX].closed = true;
        nodesExplored++;

        // Explore neighbors using JPS
        for (int i = 0; i < numDirs; i++) {
            int jx, jy;

            if (use8Dir) {
                // Try to jump in this direction
                if (!Jump(bestX, bestY, dx[i], dy[i], goalPos.x, goalPos.y, &jx, &jy))
                    continue;
            } else {
                // For 4-dir, just do regular A* neighbor expansion (JPS needs diagonals)
                jx = bestX + dx[i];
                jy = bestY + dy[i];
                if (!IsWalkable(jx, jy)) continue;
            }

            if (nodeData[0][jy][jx].closed) continue;

            // Calculate cost
            int dist = abs(jx - bestX) + abs(jy - bestY);
            if (use8Dir) {
                // Diagonal distance
                int ddx = abs(jx - bestX);
                int ddy = abs(jy - bestY);
                dist = 10 * (ddx > ddy ? ddx : ddy) + 4 * (ddx < ddy ? ddx : ddy);
            } else {
                dist *= 10;
            }

            int ng = nodeData[0][bestY][bestX].g + dist;

            if (ng < nodeData[0][jy][jx].g) {
                nodeData[0][jy][jx].g = ng;
                if (use8Dir) {
                    nodeData[0][jy][jx].f = ng + Heuristic8Dir(jx, jy, goalPos.x, goalPos.y);
                } else {
                    nodeData[0][jy][jx].f = ng + Heuristic(jx, jy, goalPos.x, goalPos.y) * 10;
                }
                nodeData[0][jy][jx].parentX = bestX;
                nodeData[0][jy][jx].parentY = bestY;
                nodeData[0][jy][jx].open = true;
            }
        }
    }

    lastPathTime = (GetTime() - startTime) * 1000.0;
    TraceLog(LOG_INFO, "JPS (%s): time=%.2fms, nodes=%d, path=%d",
             use8Dir ? "8-dir" : "4-dir", lastPathTime, nodesExplored, pathLength);
}

// ============== JPS+ (Jump Point Search Plus - with preprocessing) ==============

// JPS+ precomputed jump distances for each cell in each of 8 directions
// Positive value: distance to wall (no jump point found)
// Negative value: distance to jump point (negate to get actual distance)
// 0: cell is a wall or immediately blocked
static int16_t jpsDist[MAX_GRID_HEIGHT][MAX_GRID_WIDTH][8];
static bool jpsPrecomputed = false;  // Set to false when needsRebuild triggers

// Direction indices: 0=N, 1=NE, 2=E, 3=SE, 4=S, 5=SW, 6=W, 7=NW
static const int jpsDx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
static const int jpsDy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

// Check if a cell is walkable (bounds-checked)
static bool JpsIsWalkable(int x, int y) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight) return false;
    CellType cell = grid[0][y][x];
    return cell == CELL_WALKABLE || cell == CELL_FLOOR || cell == CELL_LADDER;
}

// Check if diagonal move is allowed (no corner cutting)
static bool JpsDiagonalAllowed(int x, int y, int dx, int dy) {
    // Both adjacent cardinal cells must be walkable
    return JpsIsWalkable(x + dx, y) && JpsIsWalkable(x, y + dy);
}

// Compute jump distance for a diagonal direction
// Returns positive distance to wall, or negative distance to jump point
static int16_t ComputeDiagonalJumpDist(int x, int y, int dir) {
    int dx = jpsDx[dir];
    int dy = jpsDy[dir];
    int dist = 0;
    int nx = x + dx;
    int ny = y + dy;

    // Cardinal direction indices for this diagonal's components
    int cardinalH = (dx > 0) ? 2 : 6;  // E or W
    int cardinalV = (dy > 0) ? 4 : 0;  // S or N

    while (JpsIsWalkable(nx, ny) && JpsDiagonalAllowed(x + dist * dx, y + dist * dy, dx, dy)) {
        dist++;

        // Check for forced neighbors (diagonal)
        if ((!JpsIsWalkable(nx - dx, ny) && JpsIsWalkable(nx - dx, ny + dy)) ||
            (!JpsIsWalkable(nx, ny - dy) && JpsIsWalkable(nx + dx, ny - dy))) {
            return -dist;  // Jump point found
        }

        // Check if cardinal directions from here have jump points
        // If so, this is a jump point
        int16_t hDist = jpsDist[ny][nx][cardinalH];
        int16_t vDist = jpsDist[ny][nx][cardinalV];
        if (hDist < 0 || vDist < 0) {
            return -dist;  // Jump point found (cardinal component leads to one)
        }

        nx += dx;
        ny += dy;
    }

    return dist;  // Hit wall or corner-cut blocked
}

// Check if cell has a forced neighbor for cardinal movement
static bool HasForcedNeighborCardinal(int x, int y, int dir) {
    int dx = jpsDx[dir];
    int dy = jpsDy[dir];
    if (dir == 0 || dir == 4) {  // N or S (vertical)
        return (!JpsIsWalkable(x - 1, y) && JpsIsWalkable(x - 1, y + dy)) ||
               (!JpsIsWalkable(x + 1, y) && JpsIsWalkable(x + 1, y + dy));
    } else {  // E or W (horizontal)
        return (!JpsIsWalkable(x, y - 1) && JpsIsWalkable(x + dx, y - 1)) ||
               (!JpsIsWalkable(x, y + 1) && JpsIsWalkable(x + dx, y + 1));
    }
}

// Precompute JPS+ data using efficient row/column sweeps
// This is O(n²) instead of O(n² × avg_jump_dist)
static void PrecomputeJpsPlusRegion(int minX, int minY, int maxX, int maxY) {
    // Clamp to grid bounds
    if (minX < 0) minX = 0;
    if (minY < 0) minY = 0;
    if (maxX > gridWidth) maxX = gridWidth;
    if (maxY > gridHeight) maxY = gridHeight;

    // Initialize all to 0 (walls)
    for (int y = minY; y < maxY; y++) {
        for (int x = minX; x < maxX; x++) {
            for (int d = 0; d < 8; d++) jpsDist[y][x][d] = 0;
        }
    }

    // === CARDINAL DIRECTIONS (sweep-based) ===

    // East (dir=2): sweep left-to-right per row
    for (int y = 0; y < gridHeight; y++) {
        int distToJP = 0;  // positive = dist to wall, negative = dist to jump point
        bool countingFromWall = true;

        for (int x = gridWidth - 1; x >= 0; x--) {
            if (!JpsIsWalkable(x, y)) {
                distToJP = 0;
                countingFromWall = true;
                continue;
            }

            distToJP++;

            if (HasForcedNeighborCardinal(x, y, 2)) {
                // This cell is a jump point for westward travelers
                jpsDist[y][x][2] = countingFromWall ? distToJP : -distToJP;
                distToJP = 0;
                countingFromWall = false;  // Next cells count toward this jump point
            } else {
                jpsDist[y][x][2] = countingFromWall ? distToJP : -distToJP;
            }
        }
    }

    // West (dir=6): sweep right-to-left per row
    for (int y = 0; y < gridHeight; y++) {
        int distToJP = 0;
        bool countingFromWall = true;

        for (int x = 0; x < gridWidth; x++) {
            if (!JpsIsWalkable(x, y)) {
                distToJP = 0;
                countingFromWall = true;
                continue;
            }

            distToJP++;

            if (HasForcedNeighborCardinal(x, y, 6)) {
                jpsDist[y][x][6] = countingFromWall ? distToJP : -distToJP;
                distToJP = 0;
                countingFromWall = false;
            } else {
                jpsDist[y][x][6] = countingFromWall ? distToJP : -distToJP;
            }
        }
    }

    // South (dir=4): sweep top-to-bottom per column
    for (int x = 0; x < gridWidth; x++) {
        int distToJP = 0;
        bool countingFromWall = true;

        for (int y = gridHeight - 1; y >= 0; y--) {
            if (!JpsIsWalkable(x, y)) {
                distToJP = 0;
                countingFromWall = true;
                continue;
            }

            distToJP++;

            if (HasForcedNeighborCardinal(x, y, 4)) {
                jpsDist[y][x][4] = countingFromWall ? distToJP : -distToJP;
                distToJP = 0;
                countingFromWall = false;
            } else {
                jpsDist[y][x][4] = countingFromWall ? distToJP : -distToJP;
            }
        }
    }

    // North (dir=0): sweep bottom-to-top per column
    for (int x = 0; x < gridWidth; x++) {
        int distToJP = 0;
        bool countingFromWall = true;

        for (int y = 0; y < gridHeight; y++) {
            if (!JpsIsWalkable(x, y)) {
                distToJP = 0;
                countingFromWall = true;
                continue;
            }

            distToJP++;

            if (HasForcedNeighborCardinal(x, y, 0)) {
                jpsDist[y][x][0] = countingFromWall ? distToJP : -distToJP;
                distToJP = 0;
                countingFromWall = false;
            } else {
                jpsDist[y][x][0] = countingFromWall ? distToJP : -distToJP;
            }
        }
    }

    // === DIAGONAL DIRECTIONS (still need per-cell, but use precomputed cardinals) ===
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            if (!JpsIsWalkable(x, y)) continue;
            jpsDist[y][x][1] = ComputeDiagonalJumpDist(x, y, 1);
            jpsDist[y][x][3] = ComputeDiagonalJumpDist(x, y, 3);
            jpsDist[y][x][5] = ComputeDiagonalJumpDist(x, y, 5);
            jpsDist[y][x][7] = ComputeDiagonalJumpDist(x, y, 7);
        }
    }
}

// Precompute JPS+ data for the entire grid
// NOTE: JPS+ is optimized for STATIC maps. Preprocessing takes ~500ms on 512x512.
// For dynamic terrain (colony sims, etc.), use HPA* instead which supports incremental updates.
void PrecomputeJpsPlus(void) {
    double startTime = GetTime();
    PrecomputeJpsPlusRegion(0, 0, gridWidth, gridHeight);
    jpsPrecomputed = true;
    jpsNeedsRebuild = false;
    TraceLog(LOG_INFO, "JPS+ precomputed in %.2fms", (GetTime() - startTime) * 1000.0);
}

// JPS+ search with bounded region (can be used for full grid or chunk)
int JpsPlusChunk(int sx, int sy, int gx, int gy, int minX, int minY, int maxX, int maxY) {
    if (!jpsPrecomputed || jpsNeedsRebuild) {
        // Full recompute needed - jumps can span entire grid, so incremental updates are unsafe
        PrecomputeJpsPlus();
    }

    if (!JpsIsWalkable(sx, sy) || !JpsIsWalkable(gx, gy)) return -1;

    // Initialize node data for bounded region
    for (int y = minY; y < maxY; y++) {
        for (int x = minX; x < maxX; x++) {
            nodeData[0][y][x] = (AStarNode){COST_INF, COST_INF, -1, -1, 0, false, false};
            heapPos[y][x] = -1;
        }
    }

    ChunkHeapInit();

    nodeData[0][sy][sx].g = 0;
    nodeData[0][sy][sx].f = Heuristic8Dir(sx, sy, gx, gy);
    nodeData[0][sy][sx].open = true;
    ChunkHeapPush(sx, sy);

    while (chunkHeap.size > 0) {
        int bestX, bestY;
        ChunkHeapPop(&bestX, &bestY);

        if (bestX == gx && bestY == gy) {
            return nodeData[0][gy][gx].g;  // Found goal
        }

        nodeData[0][bestY][bestX].open = false;
        nodeData[0][bestY][bestX].closed = true;

        // Explore all 8 directions using precomputed jump distances
        for (int dir = 0; dir < 8; dir++) {
            int16_t dist = jpsDist[bestY][bestX][dir];
            if (dist == 0) continue;  // Blocked

            int actualDist = (dist < 0) ? -dist : dist;
            int dx = jpsDx[dir];
            int dy = jpsDy[dir];

            // Check if goal is along this direction (within jump distance)
            int toGoalX = gx - bestX;
            int toGoalY = gy - bestY;
            bool goalInDir = false;
            int goalDist = 0;

            if (dx == 0 && toGoalX == 0 && dy != 0) {
                // Vertical direction, goal is on this line
                if ((dy > 0 && toGoalY > 0) || (dy < 0 && toGoalY < 0)) {
                    goalDist = abs(toGoalY);
                    goalInDir = goalDist <= actualDist;
                }
            } else if (dy == 0 && toGoalY == 0 && dx != 0) {
                // Horizontal direction, goal is on this line
                if ((dx > 0 && toGoalX > 0) || (dx < 0 && toGoalX < 0)) {
                    goalDist = abs(toGoalX);
                    goalInDir = goalDist <= actualDist;
                }
            } else if (dx != 0 && dy != 0) {
                // Diagonal direction
                if (abs(toGoalX) == abs(toGoalY) &&
                    ((dx > 0) == (toGoalX > 0)) && ((dy > 0) == (toGoalY > 0))) {
                    goalDist = abs(toGoalX);
                    goalInDir = goalDist <= actualDist;
                }
            }

            // Determine target position
            int targetX, targetY;
            int moveDist;

            if (goalInDir) {
                targetX = gx;
                targetY = gy;
                moveDist = goalDist;
            } else if (dist < 0) {
                // Jump point
                targetX = bestX + dx * actualDist;
                targetY = bestY + dy * actualDist;
                moveDist = actualDist;
            } else if (dx != 0 && dy != 0) {
                // Diagonal direction with no jump point - check if goal requires a turn
                // If goal is in the "cone" of this diagonal, we may need to turn cardinal
                bool goalInCone = ((dx > 0) == (toGoalX > 0) || toGoalX == 0) &&
                                  ((dy > 0) == (toGoalY > 0) || toGoalY == 0) &&
                                  (toGoalX != 0 || toGoalY != 0);
                if (goalInCone) {
                    // Calculate where we'd turn to go cardinal toward goal
                    int diagDist = (abs(toGoalX) < abs(toGoalY)) ? abs(toGoalX) : abs(toGoalY);
                    if (diagDist > 0 && diagDist <= actualDist) {
                        // Turn point is reachable
                        targetX = bestX + dx * diagDist;
                        targetY = bestY + dy * diagDist;
                        moveDist = diagDist;
                    } else {
                        continue;
                    }
                } else {
                    continue;
                }
            } else {
                // Cardinal direction with no jump point and goal not in line
                continue;
            }

            // Check bounds
            if (targetX < minX || targetX >= maxX || targetY < minY || targetY >= maxY) {
                // Clamp to bounds - find last valid cell
                int clampDist = actualDist;
                if (dx > 0 && bestX + dx * clampDist >= maxX) clampDist = (maxX - 1 - bestX) / dx;
                if (dx < 0 && bestX + dx * clampDist < minX) clampDist = (bestX - minX) / (-dx);
                if (dy > 0 && bestY + dy * clampDist >= maxY) clampDist = (maxY - 1 - bestY) / dy;
                if (dy < 0 && bestY + dy * clampDist < minY) clampDist = (bestY - minY) / (-dy);

                if (clampDist <= 0) continue;
                targetX = bestX + dx * clampDist;
                targetY = bestY + dy * clampDist;
                moveDist = clampDist;
            }

            if (nodeData[0][targetY][targetX].closed) continue;

            // Calculate movement cost
            int cost;
            if (dx != 0 && dy != 0) {
                cost = moveDist * 14;  // Diagonal
            } else {
                cost = moveDist * 10;  // Cardinal
            }

            int ng = nodeData[0][bestY][bestX].g + cost;

            if (ng < nodeData[0][targetY][targetX].g) {
                nodeData[0][targetY][targetX].g = ng;
                nodeData[0][targetY][targetX].f = ng + Heuristic8Dir(targetX, targetY, gx, gy);
                nodeData[0][targetY][targetX].parentX = bestX;
                nodeData[0][targetY][targetX].parentY = bestY;

                if (nodeData[0][targetY][targetX].open) {
                    ChunkHeapDecreaseKey(targetX, targetY);
                } else {
                    nodeData[0][targetY][targetX].open = true;
                    ChunkHeapPush(targetX, targetY);
                }
            }
        }
    }

    return -1;  // No path found
}

// Standalone JPS+ runner (full grid)
void RunJpsPlus(void) {
    if (startPos.x < 0 || goalPos.x < 0) return;

    pathLength = 0;
    nodesExplored = 0;
    double startTime = GetTime();

    int cost = JpsPlusChunk(startPos.x, startPos.y, goalPos.x, goalPos.y,
                            0, 0, gridWidth, gridHeight);

    if (cost >= 0) {
        // Reconstruct path
        int cx = goalPos.x, cy = goalPos.y;
        while (cx >= 0 && cy >= 0 && pathLength < MAX_PATH) {
            path[pathLength++] = (Point){cx, cy, 0};
            int px = nodeData[0][cy][cx].parentX;
            int py = nodeData[0][cy][cx].parentY;

            // Fill in intermediate points between jump points
            if (px >= 0 && py >= 0) {
                int stepX = (px > cx) ? 1 : (px < cx) ? -1 : 0;
                int stepY = (py > cy) ? 1 : (py < cy) ? -1 : 0;
                int ix = cx + stepX;
                int iy = cy + stepY;
                while ((ix != px || iy != py) && pathLength < MAX_PATH) {
                    path[pathLength++] = (Point){ix, iy, 0};
                    ix += stepX;
                    iy += stepY;
                }
            }
            cx = px;
            cy = py;
        }
        nodesExplored = pathLength;  // Approximate
    }

    lastPathTime = (GetTime() - startTime) * 1000.0;
    TraceLog(LOG_INFO, "JPS+: time=%.2fms, cost=%d, path=%d", lastPathTime, cost, pathLength);
}

// Random utilities using stdlib for portability
static unsigned int randomSeed = 1;
static bool seedInitialized = false;

void SeedRandom(unsigned int seed) {
    srand(seed);
    randomSeed = seed;
    seedInitialized = true;
}

static int GetRandomInt(int min, int max) {
    if (!seedInitialized) {
        // Auto-seed with time on first use if not explicitly seeded
        srand((unsigned int)GetTime() * 1000);
        seedInitialized = true;
    }
    if (min > max) { int t = min; min = max; max = t; }
    return min + rand() % (max - min + 1);
}

Point GetRandomWalkableCell(void) {
    Point p;
    p.z = 0;
    int attempts = 0;
    do {
        p.x = GetRandomInt(0, gridWidth - 1);
        p.y = GetRandomInt(0, gridHeight - 1);
        attempts++;
    } while (grid[0][p.y][p.x] != CELL_WALKABLE && attempts < 1000);
    if (attempts >= 1000) {
        return (Point){-1, -1, 0};
    }
    return p;
}
