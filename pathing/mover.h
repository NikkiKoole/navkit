#ifndef MOVER_H
#define MOVER_H

#include "grid.h"
#include "pathfinding.h"
#include <stdbool.h>

// Cell size in pixels (for position calculations)
#define CELL_SIZE 32

// Mover constants
#define MAX_MOVERS 10000
#define MAX_MOVER_PATH 1024
#define MOVER_SPEED 100.0f
#define MAX_REPATHS_PER_FRAME 10
#define REPATH_COOLDOWN_FRAMES 30

// Spatial grid for neighbor queries (used by avoidance)
// Cell size ~2x AVOID_RADIUS keeps cell count manageable for large worlds
#define MOVER_AVOID_RADIUS 40.0f
#define MOVER_GRID_CELL_SIZE (MOVER_AVOID_RADIUS * 2.0f)  // 80

// Fixed timestep
#define TICK_RATE 60
#define TICK_DT (1.0f / TICK_RATE)

// Mover struct (no Color - that's raylib specific, for rendering only)
typedef struct {
    float x, y;
    Point goal;
    Point path[MAX_MOVER_PATH];
    int pathLength;
    int pathIndex;
    bool active;
    bool needsRepath;
    int repathCooldown;
    float speed;
} Mover;

// Globals
extern Mover movers[MAX_MOVERS];
extern int moverCount;
extern unsigned long currentTick;
extern bool useStringPulling;
extern bool endlessMoverMode;

// Core functions
void InitMover(Mover* m, float x, float y, Point goal, float speed);
void InitMoverWithPath(Mover* m, float x, float y, Point goal, float speed, Point* pathArr, int pathLen);
void UpdateMovers(void);
void ProcessMoverRepaths(void);
void Tick(void);
void RunTicks(int count);
void ClearMovers(void);

// Utility
int CountActiveMovers(void);
bool HasLineOfSight(int x0, int y0, int x1, int y1);
void StringPullPath(Point* pathArr, int* pathLen);

// Spatial grid for neighbor queries
typedef struct {
    int* cellCounts;    // Number of movers per cell
    int* cellStarts;    // Prefix sum: start index for each cell in moverIndices
    int* moverIndices;  // Mover indices sorted by cell
    int gridW, gridH;   // Grid dimensions in cells
    int cellCount;      // Total cells (gridW * gridH)
    float invCellSize;  // 1.0 / MOVER_GRID_CELL_SIZE for fast division
} MoverSpatialGrid;

extern MoverSpatialGrid moverGrid;
extern double moverGridBuildTimeMs;  // Last build time in milliseconds

void InitMoverSpatialGrid(int worldPixelWidth, int worldPixelHeight);
void FreeMoverSpatialGrid(void);
void BuildMoverSpatialGrid(void);

// Query: calls callback for each mover within radius of (x, y), excluding excludeIndex
// Returns number of neighbors found
typedef void (*MoverNeighborCallback)(int moverIndex, float distSq, void* userData);
int QueryMoverNeighbors(float x, float y, float radius, int excludeIndex,
                        MoverNeighborCallback callback, void* userData);

#endif
