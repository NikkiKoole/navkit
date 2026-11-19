#ifndef PATHFINDING_H
#define PATHFINDING_H

#include "grid.h"

// Pathfinding algorithm selection
typedef enum {
    PATH_ALGO_ASTAR = 0,
    PATH_ALGO_HPA = 1,
} PathAlgorithm;

#define MAX_ENTRANCE_WIDTH 6
#define MAX_ENTRANCES (4096*4)
#define MAX_PATH 65536
#define MAX_EDGES 65536*4
#define MAX_EDGES_PER_NODE 64  // Max edges per entrance for adjacency list

typedef struct { int x, y, z; } Point;

typedef struct {
    int x, y, z;
    int chunk1, chunk2;
} Entrance;

typedef struct {
    int g, f;
    int parentX, parentY, parentZ;
    bool open, closed;
} AStarNode;

typedef struct {
    int from, to;
    int cost;
} GraphEdge;

// Ladder links connect entrances on different z-levels
#define MAX_LADDERS 1024
typedef struct {
    int x, y;           // Position (same on both levels)
    int zLow, zHigh;    // The two z-levels connected
    int entranceLow;    // Entrance index on lower z
    int entranceHigh;   // Entrance index on upper z  
    int cost;           // Cost to climb (default 10)
} LadderLink;

extern LadderLink ladderLinks[MAX_LADDERS];
extern int ladderLinkCount;

// Ramp links connect entrances on different z-levels (similar to ladders but directional)
#define MAX_RAMP_LINKS 8192
typedef struct {
    int rampX, rampY, rampZ;  // Ramp cell position (on lower z)
    int exitX, exitY;          // Exit position at z+1 (high side of ramp)
    int entranceRamp;          // Entrance index for ramp cell
    int entranceExit;          // Entrance index for exit cell at z+1
    int cost;                  // Movement cost (14 for diagonal-equivalent)
} RampLink;

extern RampLink rampLinks[MAX_RAMP_LINKS];
extern int rampLinkCount;

// External state (defined in pathfinding.c)
extern Entrance entrances[MAX_ENTRANCES];
extern int entranceCount;
extern GraphEdge graphEdges[MAX_EDGES];
extern int graphEdgeCount;
extern Point path[MAX_PATH];
extern int pathLength;
extern int nodesExplored;
extern double lastPathTime;
extern int lastPathCost;       // g-cost at goal from last RunAStar(), -1 if unreachable
extern double hpaAbstractTime;    // Time for abstract graph search
extern double hpaRefinementTime;  // Time for path refinement (local A*)

// Path stats (updated every 5 seconds)
extern int pathStatsCount;        // Paths computed in last 5 second period
extern double pathStatsTotalMs;   // Total ms spent on paths in last period
extern double pathStatsAvgMs;     // Average ms per path
extern bool pathStatsUpdated;     // True when stats were just updated (reset after reading)
void UpdatePathStats(void);       // Call every frame
void ResetPathStats(void);        // Call when switching algorithms
extern Point startPos;
extern Point goalPos;
extern AStarNode nodeData[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
extern bool chunkDirty[MAX_GRID_DEPTH][MAX_CHUNKS_Y][MAX_CHUNKS_X];

// Abstract graph node for HPA* search
typedef struct {
    int g, f;
    int parent;
    bool open, closed;
} AbstractNode;

#define MAX_ABSTRACT_NODES (MAX_ENTRANCES + 2)  // +2 for temp start/goal
extern AbstractNode abstractNodes[MAX_ABSTRACT_NODES];
extern int abstractPath[MAX_ENTRANCES + 2];  // path through entrance indices
extern int abstractPathLength;

// Movement direction mode
extern bool use8Dir;  // false = 4-directional, true = 8-directional

// Functions
int Heuristic(int x1, int y1, int x2, int y2);
void MarkChunkDirty(int cellX, int cellY, int cellZ);
void BuildEntrances(void);
void BuildGraph(void);
void RunAStar(void);
void RunHPAStar(void);
int FindPathHPA(Point start, Point goal, Point* outPath, int maxLen);
int FindPath(PathAlgorithm algo, Point start, Point goal, Point* outPath, int maxLen);
int AStarChunk(int sx, int sy, int sz, int gx, int gy, int minX, int minY, int maxX, int maxY);
int AStarChunkMultiTarget(int sx, int sy, int sz, int* targetX, int* targetY, int* outCosts, int numTargets,
                          int minX, int minY, int maxX, int maxY);

// Incremental update functions
void UpdateDirtyChunks(void);

// Random utilities
void SeedRandom(unsigned int seed);
Point GetRandomWalkableCell(void);
Point GetRandomWalkableCellDifferentZ(int excludeZ);
Point GetRandomWalkableCellOnZ(int z);
Point GetRandomReachableCell(int startX, int startY, int startZ);

// Entrance getters (inline for zero overhead)
static inline int GetEntranceX(int idx) { return entrances[idx].x; }
static inline int GetEntranceY(int idx) { return entrances[idx].y; }
static inline int GetEntranceZ(int idx) { return entrances[idx].z; }
static inline int GetEntranceChunk1(int idx) { return entrances[idx].chunk1; }
static inline int GetEntranceChunk2(int idx) { return entrances[idx].chunk2; }

// Graph edge getters (inline for zero overhead)
static inline int GetGraphEdgeFrom(int idx) { return graphEdges[idx].from; }
static inline int GetGraphEdgeTo(int idx) { return graphEdges[idx].to; }
static inline int GetGraphEdgeCost(int idx) { return graphEdges[idx].cost; }

#endif // PATHFINDING_H
