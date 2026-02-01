#ifndef MOVER_H
#define MOVER_H

#include "../world/grid.h"
#include "../world/pathfinding.h"
#include <stdbool.h>

// Cell size in pixels (for position calculations)
#define CELL_SIZE 32

// Mover constants
#define MAX_MOVERS 10000
#define MAX_MOVER_PATH 1024
#define MOVER_SPEED 200.0f
#define MAX_REPATHS_PER_FRAME 10
#define REPATH_COOLDOWN_FRAMES 30

// Spatial grid for neighbor queries (used by avoidance)
// Cell size ~2x AVOID_RADIUS keeps cell count manageable for large worlds
#define MOVER_AVOID_RADIUS 40.0f
#define MOVER_GRID_CELL_SIZE (MOVER_AVOID_RADIUS * 2.0f)  // 80

// Fixed timestep
#define TICK_RATE 60
#define TICK_DT (1.0f / TICK_RATE)

// Mover capabilities - determines what job types a mover can do
typedef struct {
    bool canHaul;       // Can pick up and deliver items
    bool canMine;       // Can dig/mine walls
    bool canBuild;      // Can construct blueprints
} MoverCapabilities;

// Mover struct (no Color - that's raylib specific, for rendering only)
typedef struct Mover {
    float x, y, z;              // z for future multi-level support (always 0 for now)
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
    // Stuck detection: track if mover is making progress
    float lastX, lastY, lastZ;  // Position at last progress check
    float timeWithoutProgress;  // Time since significant movement
    float fallTimer;            // Time since last fall (for visual feedback)
    // Cached avoidance vector (recomputed every N frames)
    float avoidX, avoidY;
    // Job system
    int currentJobId;    // Job pool index, -1 = no job (idle)
    // Capabilities
    MoverCapabilities capabilities;
} Mover;

// Stuck detection thresholds
#define STUCK_CHECK_INTERVAL 0.5f    // How often to check for progress (seconds)
#define STUCK_MIN_DISTANCE 1.0f      // Minimum distance to count as "progress"
#define STUCK_REPATH_TIME 2.0f       // Repath after this many seconds without progress

// Threshold for detecting "stuck" movers (knot issue)
#define KNOT_NEAR_RADIUS 30.0f      // Distance to be considered "near" waypoint
#define KNOT_STUCK_TIME 1.5f        // Seconds near waypoint before flagged as stuck

// Knot fix: larger waypoint arrival radius (Option 2 from knot-issue.md)
#define KNOT_FIX_ARRIVAL_RADIUS 16.0f  // Advance to next waypoint when within this distance
extern bool useKnotFix;                 // Toggle for knot fix

// Wall repulsion: push movers away from walls to prevent wall clipping
#define WALL_REPULSION_RADIUS 24.0f    // Distance at which wall repulsion starts
extern bool useWallRepulsion;           // Toggle for wall repulsion
extern float wallRepulsionStrength;     // Strength of wall repulsion force

// Wall sliding: slide along walls instead of penetrating them
extern bool useWallSliding;             // Toggle for wall sliding

// Globals
extern Mover movers[MAX_MOVERS];
extern int moverCount;
extern unsigned long currentTick;
extern bool useStringPulling;
extern bool endlessMoverMode;
extern bool useMoverAvoidance;
extern bool preferDifferentZ;  // When true, movers prefer goals on different z-levels
extern bool allowFallingFromAvoidance;  // When true, avoidance can push movers into air (they will fall)
extern PathAlgorithm moverPathAlgorithm;  // Algorithm used for mover pathfinding (default: HPA*)

// Core functions
void InitMover(Mover* m, float x, float y, float z, Point goal, float speed);
void InitMoverWithPath(Mover* m, float x, float y, float z, Point goal, float speed, Point* pathArr, int pathLen);
void UpdateMovers(void);
void ProcessMoverRepaths(void);
void Tick(void);
void TickWithDt(float dt);  // Tick with custom delta time (for variable timestep)
void RunTicks(int count);
void ClearMovers(void);

// Utility
int CountActiveMovers(void);
bool HasLineOfSight(int x0, int y0, int x1, int y1, int z);
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
bool IsMoverInOpenArea(float x, float y, int z);

// Vec2 for avoidance calculations
typedef struct {
    float x, y;
} Vec2;

// Check clearance in a specific direction (for directional avoidance)
// Returns true if the cells in that direction are walkable
// dir: 0=up, 1=right, 2=down, 3=left
bool HasClearanceInDirection(float x, float y, int z, int dir);

// Filter avoidance vector based on directional clearance
// Reduces or zeroes components that would push toward walls
Vec2 FilterAvoidanceByWalls(float x, float y, int z, Vec2 avoidance);

// Avoidance settings
#define AVOID_MAX_NEIGHBORS  10
#define AVOID_MAX_SCAN       48

extern float avoidStrengthOpen;    // Avoidance strength in open areas
extern float avoidStrengthClosed;  // Avoidance strength near walls
extern bool useDirectionalAvoidance;  // Filter avoidance by wall clearance

// Test/debug toggles for deterministic behavior
extern bool useRandomizedCooldowns;   // When true, repath cooldowns are randomized (avoids sync spikes)
extern bool useStaggeredUpdates;      // When true, LOS/avoidance are staggered across frames

// Compute avoidance vector for a mover (boids-style separation)
// Returns a vector pointing away from nearby movers, strength based on proximity
Vec2 ComputeMoverAvoidance(int moverIndex);

// Mover path state getters/setters (inline for zero overhead)
static inline int GetMoverPathLength(int moverIdx) { return movers[moverIdx].pathLength; }
static inline int GetMoverPathIndex(int moverIdx) { return movers[moverIdx].pathIndex; }
static inline bool GetMoverNeedsRepath(int moverIdx) { return movers[moverIdx].needsRepath; }
static inline void SetMoverNeedsRepath(int moverIdx, bool needsRepath) { movers[moverIdx].needsRepath = needsRepath; }
static inline void ClearMoverPath(int moverIdx) { movers[moverIdx].pathLength = 0; movers[moverIdx].pathIndex = -1; }

// Push all movers out of a cell to nearest walkable neighbor
void PushMoversOutOfCell(int x, int y, int z);

#endif
