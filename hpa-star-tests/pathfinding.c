#include "pathfinding.h"
#include "raylib.h"
#include <stdlib.h>

// State
Entrance entrances[MAX_ENTRANCES];
int entranceCount = 0;
GraphEdge graphEdges[MAX_EDGES];
int graphEdgeCount = 0;

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
Point startPos = {-1, -1};
Point goalPos = {-1, -1};
AStarNode nodeData[MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
bool chunkDirty[MAX_CHUNKS_Y][MAX_CHUNKS_X];

// HPA* abstract graph search state
AbstractNode abstractNodes[MAX_ABSTRACT_NODES];
int abstractPath[MAX_ENTRANCES + 2];
int abstractPathLength = 0;

// Binary heap for priority queue (used in abstract graph search)
typedef struct {
    int* nodes;      // Array of node indices
    int size;        // Current number of elements
    int capacity;    // Max capacity
} BinaryHeap;

static BinaryHeap heap;
static int heapStorage[MAX_ABSTRACT_NODES];

static void HeapInit(void) {
    heap.nodes = heapStorage;
    heap.size = 0;
    heap.capacity = MAX_ABSTRACT_NODES;
}

static void HeapSwap(int i, int j) {
    int temp = heap.nodes[i];
    heap.nodes[i] = heap.nodes[j];
    heap.nodes[j] = temp;
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
    heap.nodes[heap.size] = node;
    HeapBubbleUp(heap.size);
    heap.size++;
}

static int HeapPop(void) {
    if (heap.size == 0) return -1;
    int result = heap.nodes[0];
    heap.size--;
    if (heap.size > 0) {
        heap.nodes[0] = heap.nodes[heap.size];
        HeapBubbleDown(0);
    }
    return result;
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
    int cx = cellX / CHUNK_SIZE;
    int cy = cellY / CHUNK_SIZE;
    if (cx >= 0 && cx < chunksX && cy >= 0 && cy < chunksY) {
        chunkDirty[cy][cx] = true;
        needsRebuild = true;
    }
}

static void AddEntrance(int x, int y, int chunk1, int chunk2) {
    if (entranceCount < MAX_ENTRANCES)
        entrances[entranceCount++] = (Entrance){x, y, chunk1, chunk2};
}

static void AddEntrancesForRun(int startX, int startY, int length, int horizontal, int chunk1, int chunk2) {
    int remaining = length, pos = 0;
    while (remaining > 0) {
        int segLen = (remaining > MAX_ENTRANCE_WIDTH) ? MAX_ENTRANCE_WIDTH : remaining;
        int mid = pos + segLen / 2;
        int ex = horizontal ? startX + mid : startX;
        int ey = horizontal ? startY : startY + mid;
        AddEntrance(ex, ey, chunk1, chunk2);
        pos += segLen;
        remaining -= segLen;
    }
}

void BuildEntrances(void) {
    entranceCount = 0;
    for (int cy = 0; cy < chunksY - 1; cy++) {
        for (int cx = 0; cx < chunksX; cx++) {
            int borderY = (cy + 1) * CHUNK_SIZE;
            int startX = cx * CHUNK_SIZE;
            int chunk1 = cy * chunksX + cx;
            int chunk2 = (cy + 1) * chunksX + cx;
            int runStart = -1;
            for (int i = 0; i < CHUNK_SIZE; i++) {
                int x = startX + i;
                bool open = (grid[borderY - 1][x] == CELL_WALKABLE && grid[borderY][x] == CELL_WALKABLE);
                if (open && runStart < 0) runStart = i;
                else if (!open && runStart >= 0) {
                    AddEntrancesForRun(startX + runStart, borderY, i - runStart, 1, chunk1, chunk2);
                    runStart = -1;
                }
            }
            if (runStart >= 0)
                AddEntrancesForRun(startX + runStart, borderY, CHUNK_SIZE - runStart, 1, chunk1, chunk2);
        }
    }
    for (int cy = 0; cy < chunksY; cy++) {
        for (int cx = 0; cx < chunksX - 1; cx++) {
            int borderX = (cx + 1) * CHUNK_SIZE;
            int startY = cy * CHUNK_SIZE;
            int chunk1 = cy * chunksX + cx;
            int chunk2 = cy * chunksX + (cx + 1);
            int runStart = -1;
            for (int i = 0; i < CHUNK_SIZE; i++) {
                int y = startY + i;
                bool open = (grid[y][borderX - 1] == CELL_WALKABLE && grid[y][borderX] == CELL_WALKABLE);
                if (open && runStart < 0) runStart = i;
                else if (!open && runStart >= 0) {
                    AddEntrancesForRun(borderX, startY + runStart, i - runStart, 0, chunk1, chunk2);
                    runStart = -1;
                }
            }
            if (runStart >= 0)
                AddEntrancesForRun(borderX, startY + runStart, CHUNK_SIZE - runStart, 0, chunk1, chunk2);
        }
    }
    for (int cy = 0; cy < chunksY; cy++)
        for (int cx = 0; cx < chunksX; cx++)
            chunkDirty[cy][cx] = false;
    needsRebuild = false;
}

int AStarChunk(int sx, int sy, int gx, int gy, int minX, int minY, int maxX, int maxY) {
    for (int y = minY; y < maxY; y++)
        for (int x = minX; x < maxX; x++)
            nodeData[y][x] = (AStarNode){999999, 999999, -1, -1, false, false};

    nodeData[sy][sx].g = 0;
    if (use8Dir) {
        nodeData[sy][sx].f = Heuristic8Dir(sx, sy, gx, gy);
    } else {
        nodeData[sy][sx].f = Heuristic(sx, sy, gx, gy) * 10;
    }
    nodeData[sy][sx].open = true;

    int dx4[] = {0, 1, 0, -1};
    int dy4[] = {-1, 0, 1, 0};
    int dx8[] = {0, 1, 1, 1, 0, -1, -1, -1};
    int dy8[] = {-1, -1, 0, 1, 1, 1, 0, -1};
    
    int* dx = use8Dir ? dx8 : dx4;
    int* dy = use8Dir ? dy8 : dy4;
    int numDirs = use8Dir ? 8 : 4;

    while (1) {
        int bestX = -1, bestY = -1, bestF = 999999;
        for (int y = minY; y < maxY; y++)
            for (int x = minX; x < maxX; x++)
                if (nodeData[y][x].open && nodeData[y][x].f < bestF) {
                    bestF = nodeData[y][x].f;
                    bestX = x;
                    bestY = y;
                }
        if (bestX < 0) return -1;
        if (bestX == gx && bestY == gy) return nodeData[gy][gx].g;
        nodeData[bestY][bestX].open = false;
        nodeData[bestY][bestX].closed = true;
        for (int i = 0; i < numDirs; i++) {
            int nx = bestX + dx[i], ny = bestY + dy[i];
            if (nx < minX || nx >= maxX || ny < minY || ny >= maxY) continue;
            if (grid[ny][nx] == CELL_WALL || nodeData[ny][nx].closed) continue;
            
            // Prevent corner cutting for diagonal movement
            if (use8Dir && dx[i] != 0 && dy[i] != 0) {
                if (grid[bestY][bestX + dx[i]] == CELL_WALL || grid[bestY + dy[i]][bestX] == CELL_WALL)
                    continue;
            }
            
            int moveCost = (dx[i] != 0 && dy[i] != 0) ? 14 : 10;
            int ng = nodeData[bestY][bestX].g + moveCost;
            if (ng < nodeData[ny][nx].g) {
                nodeData[ny][nx].g = ng;
                if (use8Dir) {
                    nodeData[ny][nx].f = ng + Heuristic8Dir(nx, ny, gx, gy);
                } else {
                    nodeData[ny][nx].f = ng + Heuristic(nx, ny, gx, gy) * 10;
                }
                nodeData[ny][nx].open = true;
            }
        }
    }
}

void BuildGraph(void) {
    graphEdgeCount = 0;
    
    // Clear adjacency list
    for (int i = 0; i < entranceCount; i++) {
        adjListCount[i] = 0;
    }
    
    double startTime = GetTime();
    for (int chunk = 0; chunk < chunksX * chunksY; chunk++) {
        int cx = chunk % chunksX;
        int cy = chunk / chunksX;
        int minX = cx * CHUNK_SIZE;
        int minY = cy * CHUNK_SIZE;
        int maxX = (cx + 1) * CHUNK_SIZE + 1;
        int maxY = (cy + 1) * CHUNK_SIZE + 1;
        if (maxX > gridWidth) maxX = gridWidth;
        if (maxY > gridHeight) maxY = gridHeight;

        int chunkEnts[128];
        int numEnts = 0;
        for (int i = 0; i < entranceCount && numEnts < 128; i++) {
            if (entrances[i].chunk1 == chunk || entrances[i].chunk2 == chunk)
                chunkEnts[numEnts++] = i;
        }
        for (int i = 0; i < numEnts; i++) {
            for (int j = i + 1; j < numEnts; j++) {
                int e1 = chunkEnts[i], e2 = chunkEnts[j];
                int cost = AStarChunk(entrances[e1].x, entrances[e1].y, entrances[e2].x, entrances[e2].y, minX, minY, maxX, maxY);
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
    TraceLog(LOG_INFO, "Built graph: %d edges in %.2fms", graphEdgeCount, (GetTime() - startTime) * 1000);
}

// ============== Incremental Update Functions ==============

// Get the set of chunks affected by dirty chunks (dirty + their neighbors)
static void GetAffectedChunks(bool affectedChunks[MAX_CHUNKS_Y][MAX_CHUNKS_X]) {
    for (int cy = 0; cy < chunksY; cy++)
        for (int cx = 0; cx < chunksX; cx++)
            affectedChunks[cy][cx] = false;
    
    for (int cy = 0; cy < chunksY; cy++) {
        for (int cx = 0; cx < chunksX; cx++) {
            if (chunkDirty[cy][cx]) {
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

// Index remapping after entrance compaction (used by RebuildAffectedEdges)
static int entranceOldToNew[MAX_ENTRANCES];
static int entrancesKeptCount = 0;

// Rebuild entrances for borders of dirty chunks only
static void RebuildDirtyEntrances(void) {
    // Mark which entrances to keep (those not touching dirty chunks)
    bool keepEntrance[MAX_ENTRANCES];
    for (int i = 0; i < entranceCount; i++) {
        int c1 = entrances[i].chunk1;
        int c2 = entrances[i].chunk2;
        int cy1 = c1 / chunksX, cx1 = c1 % chunksX;
        int cy2 = c2 / chunksX, cx2 = c2 % chunksX;
        // Keep if neither chunk is dirty
        keepEntrance[i] = !chunkDirty[cy1][cx1] && !chunkDirty[cy2][cx2];
    }
    
    // Compact entrances array - remove those we're not keeping
    int newCount = 0;
    int oldEntranceCount = entranceCount;
    for (int i = 0; i < oldEntranceCount; i++) {
        if (keepEntrance[i]) {
            entranceOldToNew[i] = newCount;
            entrances[newCount++] = entrances[i];
        } else {
            entranceOldToNew[i] = -1;  // Removed
        }
    }
    
    entrancesKeptCount = newCount;
    int keptCount = newCount;
    
    // Now rebuild entrances for dirty chunk borders
    // Horizontal borders (between cy and cy+1)
    for (int cy = 0; cy < chunksY - 1; cy++) {
        for (int cx = 0; cx < chunksX; cx++) {
            // Only rebuild if either chunk is dirty
            if (!chunkDirty[cy][cx] && !chunkDirty[cy+1][cx]) continue;
            
            int borderY = (cy + 1) * CHUNK_SIZE;
            int startX = cx * CHUNK_SIZE;
            int chunk1 = cy * chunksX + cx;
            int chunk2 = (cy + 1) * chunksX + cx;
            int runStart = -1;
            
            for (int i = 0; i < CHUNK_SIZE; i++) {
                int x = startX + i;
                bool open = (grid[borderY - 1][x] == CELL_WALKABLE && grid[borderY][x] == CELL_WALKABLE);
                if (open && runStart < 0) runStart = i;
                else if (!open && runStart >= 0) {
                    // Add entrances for this run
                    int length = i - runStart;
                    int pos = 0;
                    while (length > 0 && newCount < MAX_ENTRANCES) {
                        int segLen = (length > MAX_ENTRANCE_WIDTH) ? MAX_ENTRANCE_WIDTH : length;
                        int mid = pos + segLen / 2;
                        entrances[newCount++] = (Entrance){startX + runStart + mid, borderY, chunk1, chunk2};
                        pos += segLen;
                        length -= segLen;
                    }
                    runStart = -1;
                }
            }
            if (runStart >= 0) {
                int length = CHUNK_SIZE - runStart;
                int pos = 0;
                while (length > 0 && newCount < MAX_ENTRANCES) {
                    int segLen = (length > MAX_ENTRANCE_WIDTH) ? MAX_ENTRANCE_WIDTH : length;
                    int mid = pos + segLen / 2;
                    entrances[newCount++] = (Entrance){startX + runStart + mid, borderY, chunk1, chunk2};
                    pos += segLen;
                    length -= segLen;
                }
            }
        }
    }
    
    // Vertical borders (between cx and cx+1)
    for (int cy = 0; cy < chunksY; cy++) {
        for (int cx = 0; cx < chunksX - 1; cx++) {
            // Only rebuild if either chunk is dirty
            if (!chunkDirty[cy][cx] && !chunkDirty[cy][cx+1]) continue;
            
            int borderX = (cx + 1) * CHUNK_SIZE;
            int startY = cy * CHUNK_SIZE;
            int chunk1 = cy * chunksX + cx;
            int chunk2 = cy * chunksX + (cx + 1);
            int runStart = -1;
            
            for (int i = 0; i < CHUNK_SIZE; i++) {
                int y = startY + i;
                bool open = (grid[y][borderX - 1] == CELL_WALKABLE && grid[y][borderX] == CELL_WALKABLE);
                if (open && runStart < 0) runStart = i;
                else if (!open && runStart >= 0) {
                    int length = i - runStart;
                    int pos = 0;
                    while (length > 0 && newCount < MAX_ENTRANCES) {
                        int segLen = (length > MAX_ENTRANCE_WIDTH) ? MAX_ENTRANCE_WIDTH : length;
                        int mid = pos + segLen / 2;
                        entrances[newCount++] = (Entrance){borderX, startY + runStart + mid, chunk1, chunk2};
                        pos += segLen;
                        length -= segLen;
                    }
                    runStart = -1;
                }
            }
            if (runStart >= 0) {
                int length = CHUNK_SIZE - runStart;
                int pos = 0;
                while (length > 0 && newCount < MAX_ENTRANCES) {
                    int segLen = (length > MAX_ENTRANCE_WIDTH) ? MAX_ENTRANCE_WIDTH : length;
                    int mid = pos + segLen / 2;
                    entrances[newCount++] = (Entrance){borderX, startY + runStart + mid, chunk1, chunk2};
                    pos += segLen;
                    length -= segLen;
                }
            }
        }
    }
    
    entranceCount = newCount;
    TraceLog(LOG_INFO, "Incremental entrances: kept %d, rebuilt to %d total", keptCount, newCount);
}

// Rebuild graph edges for affected chunks only
static void RebuildAffectedEdges(bool affectedChunks[MAX_CHUNKS_Y][MAX_CHUNKS_X]) {
    // Remove edges that involve affected chunks, and remap indices for kept edges
    int newEdgeCount = 0;
    
    // First pass: identify edges to keep, remap indices, and compact
    for (int i = 0; i < graphEdgeCount; i++) {
        int e1 = graphEdges[i].from;
        int e2 = graphEdges[i].to;
        
        // Remap to new indices
        int newE1 = entranceOldToNew[e1];
        int newE2 = entranceOldToNew[e2];
        
        // Skip if either entrance was removed
        if (newE1 < 0 || newE2 < 0) continue;
        
        // An edge exists because two entrances share a chunk.
        // We need to find the shared chunk and check if it's affected.
        int c1a = entrances[newE1].chunk1, c1b = entrances[newE1].chunk2;
        int c2a = entrances[newE2].chunk1, c2b = entrances[newE2].chunk2;
        
        // Find the shared chunk between the two entrances
        int sharedChunk = -1;
        if (c1a == c2a || c1a == c2b) sharedChunk = c1a;
        else if (c1b == c2a || c1b == c2b) sharedChunk = c1b;
        
        // Only discard edge if its shared chunk is affected
        bool affected = false;
        if (sharedChunk >= 0) {
            int scx = sharedChunk % chunksX;
            int scy = sharedChunk / chunksX;
            affected = affectedChunks[scy][scx];
        }
        
        if (!affected) {
            // Keep edge with remapped indices
            graphEdges[newEdgeCount++] = (GraphEdge){newE1, newE2, graphEdges[i].cost};
        }
    }
    
    int keptEdges = newEdgeCount;
    graphEdgeCount = newEdgeCount;
    
    // Clear and rebuild adjacency list for all nodes
    for (int i = 0; i < entranceCount; i++) {
        adjListCount[i] = 0;
    }
    
    // Re-add kept edges to adjacency list
    for (int i = 0; i < keptEdges; i++) {
        int e1 = graphEdges[i].from;
        if (adjListCount[e1] < MAX_EDGES_PER_NODE) {
            adjList[e1][adjListCount[e1]++] = i;
        }
    }
    
    // Now rebuild edges for affected chunks
    for (int cy = 0; cy < chunksY; cy++) {
        for (int cx = 0; cx < chunksX; cx++) {
            if (!affectedChunks[cy][cx]) continue;
            
            int chunk = cy * chunksX + cx;
            int minX = cx * CHUNK_SIZE;
            int minY = cy * CHUNK_SIZE;
            int maxX = (cx + 1) * CHUNK_SIZE + 1;
            int maxY = (cy + 1) * CHUNK_SIZE + 1;
            if (maxX > gridWidth) maxX = gridWidth;
            if (maxY > gridHeight) maxY = gridHeight;
            
            // Find entrances for this chunk
            int chunkEnts[128];
            int numEnts = 0;
            for (int i = 0; i < entranceCount && numEnts < 128; i++) {
                if (entrances[i].chunk1 == chunk || entrances[i].chunk2 == chunk)
                    chunkEnts[numEnts++] = i;
            }
            
            // Build edges between all pairs
            for (int i = 0; i < numEnts; i++) {
                for (int j = i + 1; j < numEnts; j++) {
                    int e1 = chunkEnts[i], e2 = chunkEnts[j];
                    
                    // Check if this edge already exists (in kept edges or newly added)
                    bool exists = false;
                    for (int k = 0; k < graphEdgeCount && !exists; k++) {
                        if ((graphEdges[k].from == e1 && graphEdges[k].to == e2) ||
                            (graphEdges[k].from == e2 && graphEdges[k].to == e1)) {
                            exists = true;
                        }
                    }
                    if (exists) continue;
                    
                    int cost = AStarChunk(entrances[e1].x, entrances[e1].y, 
                                         entrances[e2].x, entrances[e2].y, 
                                         minX, minY, maxX, maxY);
                    if (cost >= 0 && graphEdgeCount < MAX_EDGES - 1) {
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
    
    TraceLog(LOG_INFO, "Incremental edges: kept %d, total now %d", keptEdges, graphEdgeCount);
}

void UpdateDirtyChunks(void) {
    // Check if any chunks are dirty
    bool anyDirty = false;
    for (int cy = 0; cy < chunksY && !anyDirty; cy++)
        for (int cx = 0; cx < chunksX && !anyDirty; cx++)
            if (chunkDirty[cy][cx]) anyDirty = true;
    
    if (!anyDirty) return;
    
    double startTime = GetTime();
    
    // Get affected chunks (dirty + neighbors)
    bool affectedChunks[MAX_CHUNKS_Y][MAX_CHUNKS_X];
    GetAffectedChunks(affectedChunks);
    
    int dirtyCount = 0, affectedCount = 0;
    for (int cy = 0; cy < chunksY; cy++) {
        for (int cx = 0; cx < chunksX; cx++) {
            if (chunkDirty[cy][cx]) dirtyCount++;
            if (affectedChunks[cy][cx]) affectedCount++;
        }
    }
    
    // Rebuild entrances for dirty chunk borders
    RebuildDirtyEntrances();
    
    // Rebuild edges for affected chunks
    RebuildAffectedEdges(affectedChunks);
    
    // Clear dirty flags
    for (int cy = 0; cy < chunksY; cy++)
        for (int cx = 0; cx < chunksX; cx++)
            chunkDirty[cy][cx] = false;
    needsRebuild = false;
    
    double elapsed = (GetTime() - startTime) * 1000.0;
    TraceLog(LOG_INFO, "Incremental update: %d dirty, %d affected chunks in %.2fms", 
             dirtyCount, affectedCount, elapsed);
}

void RunAStar(void) {
    if (startPos.x < 0 || goalPos.x < 0) return;
    pathLength = 0;
    nodesExplored = 0;
    double startTime = GetTime();

    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++)
            nodeData[y][x] = (AStarNode){999999, 999999, -1, -1, false, false};

    nodeData[startPos.y][startPos.x].g = 0;
    if (use8Dir) {
        nodeData[startPos.y][startPos.x].f = Heuristic8Dir(startPos.x, startPos.y, goalPos.x, goalPos.y);
    } else {
        nodeData[startPos.y][startPos.x].f = Heuristic(startPos.x, startPos.y, goalPos.x, goalPos.y) * 10;
    }
    nodeData[startPos.y][startPos.x].open = true;

    // Direction arrays
    int dx4[] = {0, 1, 0, -1};
    int dy4[] = {-1, 0, 1, 0};
    int dx8[] = {0, 1, 1, 1, 0, -1, -1, -1};
    int dy8[] = {-1, -1, 0, 1, 1, 1, 0, -1};
    
    int* dx = use8Dir ? dx8 : dx4;
    int* dy = use8Dir ? dy8 : dy4;
    int numDirs = use8Dir ? 8 : 4;

    while (1) {
        int bestX = -1, bestY = -1, bestF = 999999;
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++)
                if (nodeData[y][x].open && nodeData[y][x].f < bestF) {
                    bestF = nodeData[y][x].f;
                    bestX = x;
                    bestY = y;
                }
        if (bestX < 0) break;
        if (bestX == goalPos.x && bestY == goalPos.y) {
            int cx = goalPos.x, cy = goalPos.y;
            while (cx >= 0 && cy >= 0 && pathLength < MAX_PATH) {
                path[pathLength++] = (Point){cx, cy};
                int px = nodeData[cy][cx].parentX;
                int py = nodeData[cy][cx].parentY;
                cx = px; cy = py;
            }
            break;
        }
        nodeData[bestY][bestX].open = false;
        nodeData[bestY][bestX].closed = true;
        nodesExplored++;
        for (int i = 0; i < numDirs; i++) {
            int nx = bestX + dx[i], ny = bestY + dy[i];
            if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
            if (grid[ny][nx] == CELL_WALL || nodeData[ny][nx].closed) continue;
            
            // For diagonal movement, check that we can actually move diagonally
            // (not cutting corners through walls)
            if (use8Dir && dx[i] != 0 && dy[i] != 0) {
                if (grid[bestY][bestX + dx[i]] == CELL_WALL || grid[bestY + dy[i]][bestX] == CELL_WALL)
                    continue;
            }
            
            // Cost: 10 for cardinal, 14 for diagonal
            int moveCost = (dx[i] != 0 && dy[i] != 0) ? 14 : 10;
            int ng = nodeData[bestY][bestX].g + moveCost;
            
            if (ng < nodeData[ny][nx].g) {
                nodeData[ny][nx].g = ng;
                if (use8Dir) {
                    nodeData[ny][nx].f = ng + Heuristic8Dir(nx, ny, goalPos.x, goalPos.y);
                } else {
                    nodeData[ny][nx].f = ng + Heuristic(nx, ny, goalPos.x, goalPos.y) * 10;
                }
                nodeData[ny][nx].parentX = bestX;
                nodeData[ny][nx].parentY = bestY;
                nodeData[ny][nx].open = true;
            }
        }
    }
    lastPathTime = (GetTime() - startTime) * 1000.0;
    TraceLog(LOG_INFO, "A* (%s): time=%.2fms, nodes=%d, path=%d", 
             use8Dir ? "8-dir" : "4-dir", lastPathTime, nodesExplored, pathLength);
}

// Get chunk index from cell coordinates
static int GetChunk(int x, int y) {
    int cx = x / CHUNK_SIZE;
    int cy = y / CHUNK_SIZE;
    if (cx < 0) cx = 0;
    if (cx >= chunksX) cx = chunksX - 1;
    if (cy < 0) cy = 0;
    if (cy >= chunksY) cy = chunksY - 1;
    return cy * chunksX + cx;
}

// Get chunk bounds
static void GetChunkBounds(int chunk, int* minX, int* minY, int* maxX, int* maxY) {
    int cx = chunk % chunksX;
    int cy = chunk / chunksX;
    *minX = cx * CHUNK_SIZE;
    *minY = cy * CHUNK_SIZE;
    *maxX = (cx + 1) * CHUNK_SIZE;
    *maxY = (cy + 1) * CHUNK_SIZE;
    if (*maxX > gridWidth) *maxX = gridWidth;
    if (*maxY > gridHeight) *maxY = gridHeight;
}

// Reconstruct cell-level path between two points within a chunk
static int ReconstructLocalPath(int sx, int sy, int gx, int gy, Point* outPath, int maxLen) {
    // Get bounds that cover both start and goal chunks
    int startChunk = GetChunk(sx, sy);
    int goalChunk = GetChunk(gx, gy);
    
    int minX1, minY1, maxX1, maxY1;
    int minX2, minY2, maxX2, maxY2;
    GetChunkBounds(startChunk, &minX1, &minY1, &maxX1, &maxY1);
    GetChunkBounds(goalChunk, &minX2, &minY2, &maxX2, &maxY2);
    
    // Union of both chunk bounds
    int minX = minX1 < minX2 ? minX1 : minX2;
    int minY = minY1 < minY2 ? minY1 : minY2;
    int maxX = maxX1 > maxX2 ? maxX1 : maxX2;
    int maxY = maxY1 > maxY2 ? maxY1 : maxY2;
    
    // Expand bounds generously to allow paths that may need to go around obstacles
    // Entrances on chunk borders may need to path through adjacent chunks
    int expand = CHUNK_SIZE / 2;
    minX -= expand; if (minX < 0) minX = 0;
    minY -= expand; if (minY < 0) minY = 0;
    maxX += expand; if (maxX > gridWidth) maxX = gridWidth;
    maxY += expand; if (maxY > gridHeight) maxY = gridHeight;
    
    // Run A* in this region
    for (int y = minY; y < maxY; y++)
        for (int x = minX; x < maxX; x++)
            nodeData[y][x] = (AStarNode){999999, 999999, -1, -1, false, false};
    
    nodeData[sy][sx].g = 0;
    if (use8Dir) {
        nodeData[sy][sx].f = Heuristic8Dir(sx, sy, gx, gy);
    } else {
        nodeData[sy][sx].f = Heuristic(sx, sy, gx, gy) * 10;
    }
    nodeData[sy][sx].open = true;
    
    int dx4[] = {0, 1, 0, -1};
    int dy4[] = {-1, 0, 1, 0};
    int dx8[] = {0, 1, 1, 1, 0, -1, -1, -1};
    int dy8[] = {-1, -1, 0, 1, 1, 1, 0, -1};
    
    int* dx = use8Dir ? dx8 : dx4;
    int* dy = use8Dir ? dy8 : dy4;
    int numDirs = use8Dir ? 8 : 4;
    
    while (1) {
        int bestX = -1, bestY = -1, bestF = 999999;
        for (int y = minY; y < maxY; y++)
            for (int x = minX; x < maxX; x++)
                if (nodeData[y][x].open && nodeData[y][x].f < bestF) {
                    bestF = nodeData[y][x].f;
                    bestX = x;
                    bestY = y;
                }
        if (bestX < 0) return 0;  // No path
        if (bestX == gx && bestY == gy) {
            // Reconstruct path
            int len = 0;
            int cx = gx, cy = gy;
            while (cx >= 0 && cy >= 0 && len < maxLen) {
                outPath[len++] = (Point){cx, cy};
                int px = nodeData[cy][cx].parentX;
                int py = nodeData[cy][cx].parentY;
                cx = px;
                cy = py;
            }
            return len;
        }
        nodeData[bestY][bestX].open = false;
        nodeData[bestY][bestX].closed = true;
        for (int i = 0; i < numDirs; i++) {
            int nx = bestX + dx[i], ny = bestY + dy[i];
            if (nx < minX || nx >= maxX || ny < minY || ny >= maxY) continue;
            if (grid[ny][nx] == CELL_WALL || nodeData[ny][nx].closed) continue;
            
            // Prevent corner cutting for diagonal movement
            if (use8Dir && dx[i] != 0 && dy[i] != 0) {
                if (grid[bestY][bestX + dx[i]] == CELL_WALL || grid[bestY + dy[i]][bestX] == CELL_WALL)
                    continue;
            }
            
            int moveCost = (dx[i] != 0 && dy[i] != 0) ? 14 : 10;
            int ng = nodeData[bestY][bestX].g + moveCost;
            if (ng < nodeData[ny][nx].g) {
                nodeData[ny][nx].g = ng;
                if (use8Dir) {
                    nodeData[ny][nx].f = ng + Heuristic8Dir(nx, ny, gx, gy);
                } else {
                    nodeData[ny][nx].f = ng + Heuristic(nx, ny, gx, gy) * 10;
                }
                nodeData[ny][nx].parentX = bestX;
                nodeData[ny][nx].parentY = bestY;
                nodeData[ny][nx].open = true;
            }
        }
    }
}

void RunHPAStar(void) {
    if (startPos.x < 0 || goalPos.x < 0) return;
    if (entranceCount == 0) return;  // Need to build entrances first
    
    pathLength = 0;
    abstractPathLength = 0;
    nodesExplored = 0;
    hpaAbstractTime = 0.0;
    hpaRefinementTime = 0.0;
    double startTime = GetTime();
    
    int startChunk = GetChunk(startPos.x, startPos.y);
    int goalChunk = GetChunk(goalPos.x, goalPos.y);
    
    // Special case: start and goal in same chunk - just do local A*
    if (startChunk == goalChunk) {
        int minX, minY, maxX, maxY;
        GetChunkBounds(startChunk, &minX, &minY, &maxX, &maxY);
        pathLength = ReconstructLocalPath(startPos.x, startPos.y, goalPos.x, goalPos.y, path, MAX_PATH);
        lastPathTime = (GetTime() - startTime) * 1000.0;
        return;
    }
    
    // Temporary entrance indices for start and goal
    int startNode = entranceCount;      // Index for start as temp node
    int goalNode = entranceCount + 1;   // Index for goal as temp node
    int totalNodes = entranceCount + 2;
    
    // Initialize abstract nodes
    for (int i = 0; i < totalNodes; i++) {
        abstractNodes[i] = (AbstractNode){999999, 999999, -1, false, false};
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
    
    double connectStartTime = GetTime();
    
    // Connect start to entrances in start chunk
    GetChunkBounds(startChunk, &minX, &minY, &maxX, &maxY);
    if (maxX < gridWidth) maxX++;
    if (maxY < gridHeight) maxY++;
    
    for (int i = 0; i < entranceCount && startEdgeCount < 128; i++) {
        if (entrances[i].chunk1 == startChunk || entrances[i].chunk2 == startChunk) {
            int cost = AStarChunk(startPos.x, startPos.y, entrances[i].x, entrances[i].y, 
                                  minX > 0 ? minX - 1 : 0, minY > 0 ? minY - 1 : 0, maxX, maxY);
            if (cost >= 0) {
                startEdgeTargets[startEdgeCount] = i;
                startEdgeCosts[startEdgeCount] = cost;
                startEdgeCount++;
                nodesExplored++;
            }
        }
    }
    
    // Connect goal to entrances in goal chunk
    GetChunkBounds(goalChunk, &minX, &minY, &maxX, &maxY);
    if (maxX < gridWidth) maxX++;
    if (maxY < gridHeight) maxY++;
    
    for (int i = 0; i < entranceCount && goalEdgeCount < 128; i++) {
        if (entrances[i].chunk1 == goalChunk || entrances[i].chunk2 == goalChunk) {
            int cost = AStarChunk(goalPos.x, goalPos.y, entrances[i].x, entrances[i].y,
                                  minX > 0 ? minX - 1 : 0, minY > 0 ? minY - 1 : 0, maxX, maxY);
            if (cost >= 0) {
                goalEdgeTargets[goalEdgeCount] = i;
                goalEdgeCosts[goalEdgeCount] = cost;
                goalEdgeCount++;
                nodesExplored++;
            }
        }
    }
    
    double connectTime = (GetTime() - connectStartTime) * 1000.0;
    
    // A* on abstract graph using binary heap
    double abstractStartTime = GetTime();
    HeapInit();
    
    abstractNodes[startNode].g = 0;
    abstractNodes[startNode].f = Heuristic(startPos.x, startPos.y, goalPos.x, goalPos.y);
    abstractNodes[startNode].open = true;
    HeapPush(startNode);
    
    while (heap.size > 0) {
        // Pop best node from heap
        int best = HeapPop();
        
        // Skip if already closed (duplicate entry in heap)
        if (abstractNodes[best].closed) continue;
        
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
                    abstractNodes[neighbor].g = ng;
                    abstractNodes[neighbor].f = ng + Heuristic(entrances[neighbor].x, entrances[neighbor].y, goalPos.x, goalPos.y);
                    abstractNodes[neighbor].parent = best;
                    abstractNodes[neighbor].open = true;
                    HeapPush(neighbor);
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
                    abstractNodes[neighbor].g = ng;
                    abstractNodes[neighbor].f = ng + Heuristic(entrances[neighbor].x, entrances[neighbor].y, goalPos.x, goalPos.y);
                    abstractNodes[neighbor].parent = best;
                    abstractNodes[neighbor].open = true;
                    HeapPush(neighbor);
                }
            }
            // Also check if this entrance can reach goal
            for (int i = 0; i < goalEdgeCount; i++) {
                if (goalEdgeTargets[i] == best) {
                    int neighbor = goalNode;
                    if (abstractNodes[neighbor].closed) continue;
                    int ng = abstractNodes[best].g + goalEdgeCosts[i];
                    if (ng < abstractNodes[neighbor].g) {
                        abstractNodes[neighbor].g = ng;
                        abstractNodes[neighbor].f = ng;  // h=0 at goal
                        abstractNodes[neighbor].parent = best;
                        abstractNodes[neighbor].open = true;
                        HeapPush(neighbor);
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
            
            int fx, fy, tx, ty;
            
            // Get coordinates for from node
            if (fromNode == startNode) {
                fx = startPos.x;
                fy = startPos.y;
            } else {
                fx = entrances[fromNode].x;
                fy = entrances[fromNode].y;
            }
            
            // Get coordinates for to node
            if (toNode == goalNode) {
                tx = goalPos.x;
                ty = goalPos.y;
            } else {
                tx = entrances[toNode].x;
                ty = entrances[toNode].y;
            }
            
            // Reconstruct local path for this segment
            int localLen = ReconstructLocalPath(fx, fy, tx, ty, tempPath, MAX_PATH);
            
            if (localLen == 0) {
                // No path found for this segment - shouldn't happen with valid abstract path
                TraceLog(LOG_WARNING, "HPA* refinement failed: no path from (%d,%d) to (%d,%d)", fx, fy, tx, ty);
                continue;
            }
            
            // tempPath is in reverse order: [0]=destination, [localLen-1]=source
            // We iterate from source to destination (high index to low)
            // Skip source point for subsequent segments (it's the destination of previous segment)
            int skipSource = (i == abstractPathLength - 1) ? 0 : 1;
            for (int j = localLen - 1 - skipSource; j >= 0 && pathLength < MAX_PATH; j--) {
                path[pathLength++] = tempPath[j];
            }
        }
        
        // Reverse path so it goes from goal to start (matching RunAStar behavior)
        for (int i = 0; i < pathLength / 2; i++) {
            Point tmp = path[i];
            path[i] = path[pathLength - 1 - i];
            path[pathLength - 1 - i] = tmp;
        }
    }
    hpaRefinementTime = (GetTime() - refineStartTime) * 1000.0;
    
    lastPathTime = (GetTime() - startTime) * 1000.0;
    TraceLog(LOG_INFO, "HPA*: total=%.2fms (connect=%.2fms, search=%.2fms, refine=%.2fms), nodes=%d, path=%d", 
             lastPathTime, connectTime, hpaAbstractTime, hpaRefinementTime, nodesExplored, pathLength);
}

// ============== JPS Implementation ==============

static bool IsWalkable(int x, int y) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight) return false;
    return grid[y][x] == CELL_WALKABLE;
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
            nodeData[y][x] = (AStarNode){999999, 999999, -1, -1, false, false};
    
    nodeData[startPos.y][startPos.x].g = 0;
    if (use8Dir) {
        nodeData[startPos.y][startPos.x].f = Heuristic8Dir(startPos.x, startPos.y, goalPos.x, goalPos.y);
    } else {
        nodeData[startPos.y][startPos.x].f = Heuristic(startPos.x, startPos.y, goalPos.x, goalPos.y) * 10;
    }
    nodeData[startPos.y][startPos.x].open = true;
    
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
        int bestX = -1, bestY = -1, bestF = 999999;
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++)
                if (nodeData[y][x].open && nodeData[y][x].f < bestF) {
                    bestF = nodeData[y][x].f;
                    bestX = x;
                    bestY = y;
                }
        
        if (bestX < 0) break;  // No path
        
        if (bestX == goalPos.x && bestY == goalPos.y) {
            // Reconstruct path
            int cx = goalPos.x, cy = goalPos.y;
            while (cx >= 0 && cy >= 0 && pathLength < MAX_PATH) {
                path[pathLength++] = (Point){cx, cy};
                int px = nodeData[cy][cx].parentX;
                int py = nodeData[cy][cx].parentY;
                
                // For JPS, we need to fill in intermediate points
                if (px >= 0 && py >= 0) {
                    int stepX = (px > cx) ? 1 : (px < cx) ? -1 : 0;
                    int stepY = (py > cy) ? 1 : (py < cy) ? -1 : 0;
                    int ix = cx + stepX;
                    int iy = cy + stepY;
                    while ((ix != px || iy != py) && pathLength < MAX_PATH) {
                        path[pathLength++] = (Point){ix, iy};
                        ix += stepX;
                        iy += stepY;
                    }
                }
                cx = px;
                cy = py;
            }
            break;
        }
        
        nodeData[bestY][bestX].open = false;
        nodeData[bestY][bestX].closed = true;
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
            
            if (nodeData[jy][jx].closed) continue;
            
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
            
            int ng = nodeData[bestY][bestX].g + dist;
            
            if (ng < nodeData[jy][jx].g) {
                nodeData[jy][jx].g = ng;
                if (use8Dir) {
                    nodeData[jy][jx].f = ng + Heuristic8Dir(jx, jy, goalPos.x, goalPos.y);
                } else {
                    nodeData[jy][jx].f = ng + Heuristic(jx, jy, goalPos.x, goalPos.y) * 10;
                }
                nodeData[jy][jx].parentX = bestX;
                nodeData[jy][jx].parentY = bestY;
                nodeData[jy][jx].open = true;
            }
        }
    }
    
    lastPathTime = (GetTime() - startTime) * 1000.0;
    TraceLog(LOG_INFO, "JPS (%s): time=%.2fms, nodes=%d, path=%d", 
             use8Dir ? "8-dir" : "4-dir", lastPathTime, nodesExplored, pathLength);
}
