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

#endif
