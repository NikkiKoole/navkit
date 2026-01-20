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
    // Knot detection: time spent near current waypoint without reaching it
    float timeNearWaypoint;
} Mover;

// Threshold for detecting "stuck" movers (knot issue)
#define KNOT_NEAR_RADIUS 30.0f      // Distance to be considered "near" waypoint
#define KNOT_STUCK_TIME 1.5f        // Seconds near waypoint before flagged as stuck

// Knot fix: larger waypoint arrival radius (Option 2 from knot-issue.md)
#define KNOT_FIX_ARRIVAL_RADIUS 16.0f  // Advance to next waypoint when within this distance
extern bool useKnotFix;                 // Toggle for knot fix

// Globals
extern Mover movers[MAX_MOVERS];
extern int moverCount;
extern unsigned long currentTick;
extern bool useStringPulling;
extern bool endlessMoverMode;
extern bool useMoverAvoidance;

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

// Check if mover is in open area (3x3 grid cells around it are all walkable)
// Returns true if full avoidance is safe, false if near walls
bool IsMoverInOpenArea(float x, float y);

// Vec2 for avoidance calculations
typedef struct {
    float x, y;
} Vec2;

// Check clearance in a specific direction (for directional avoidance)
// Returns true if the cells in that direction are walkable
// dir: 0=up, 1=right, 2=down, 3=left
bool HasClearanceInDirection(float x, float y, int dir);

// Filter avoidance vector based on directional clearance
// Reduces or zeroes components that would push toward walls
Vec2 FilterAvoidanceByWalls(float x, float y, Vec2 avoidance);

// Avoidance settings
#define AVOID_MAX_NEIGHBORS  10
#define AVOID_MAX_SCAN       48

extern float avoidStrengthOpen;    // Avoidance strength in open areas
extern float avoidStrengthClosed;  // Avoidance strength near walls
extern bool useDirectionalAvoidance;  // Filter avoidance by wall clearance

// Compute avoidance vector for a mover (boids-style separation)
// Returns a vector pointing away from nearby movers, strength based on proximity
Vec2 ComputeMoverAvoidance(int moverIndex);

#endif
