// cars.h - Cars experiment (docs/research/cars-experiment.md)
//
// A car is its own entity with a real steering model (kinematic bicycle: it turns
// around its rear axle, so it has a turning circle and can't turn on the spot).
//
// Planning is Hybrid A*: a search over (x, y, heading) whose moves are short arcs the
// car can drive, forward and in reverse, checked against the real rotated body. So a
// route already contains its 3-point turns and back-ups. Its heuristic is a grid
// distance around the buildings (Dijkstra over cells where a ceil(carWidth) square
// fits, on one z level, no ladders / ramps / shafts).
//
// Following: pure pursuit on each forward or reverse stretch of the route, stopping at
// each gear change. Blocked for a moment: plan again from where it stands.
//
// Experiment only: cars are not saved and only the "cars" scenario spawns them.
#ifndef CARS_H
#define CARS_H

#include <stdbool.h>

#define MAX_CARS 64
#define CAR_MAX_PATH 4096   // 4 px per point: routes up to ~500 cells
#define CAR_MAX_STOPS 16

typedef enum {
    DRIVE_PARKED,     // waiting at a stop (or no stops)
    DRIVE_FOLLOW,     // following the route (forward or reverse stretch)
    DRIVE_STUCK,      // no route from here; plans again after a while
} DriveState;

typedef struct {
    bool active;
    float x, y;            // centre, pixels
    int z;
    float heading;         // radians, 0 = east (+x), pi/2 = south (+y, screen down)
    float speed;           // pixels/s along the heading (negative = reversing)
    float steer;           // front wheel angle, radians (+ = heading turns toward +angle)
    int state;             // DriveState
    float stateTime;       // seconds in this state

    // Route: car-centre points (pixels, ~8 px apart) with the gear for each step
    float pathX[CAR_MAX_PATH], pathY[CAR_MAX_PATH];
    signed char pathDir[CAR_MAX_PATH];   // +1 forward, -1 reverse (to reach the next point)
    float pathH[CAR_MAX_PATH];           // planned heading at each point
    float pathSteer[CAR_MAX_PATH];       // planned wheel angle to reach the next point
    int pathLen, pathIdx;  // pathIdx: start of the segment the car is on
    int goalCellX, goalCellY;
    float lookX, lookY;    // pursuit target this tick (debug draw)
    float toCusp;          // pixels to the next gear change (or the end)
    float blockedTime;     // seconds stopped by something in the way
    int whyCar;            // the car in our way (-1: none), for the debug line
    float waitNow;         // seconds stopped behind another car, this time
    float waitTime;        // seconds stopped behind other cars, in all
    int deadlocks;         // times waitNow passed CAR_DEADLOCK_S
    int replanStreak;      // replans without getting anywhere (a car length) in between
    float replanX, replanY;

    // Stops: drives stop 0 -> 1 -> ... -> wraps, waiting carStopWait at each
    int stopX[CAR_MAX_STOPS], stopY[CAR_MAX_STOPS];
    char stopName[CAR_MAX_STOPS];   // one letter, for the debug draw
    int stopCount, stopIdx;

    // Stats (this leg, and totals)
    float legTime;
    int legsDone, gearChanges, replans, bumps, stuck, planFails;
    float driveTime;
} Car;

extern Car cars[MAX_CARS];
extern int carCount;

// Settings (console / --set; rows in settings.c, group "Experiments/Cars")
extern float carLength;        // cells
extern float carWidth;         // cells
extern float carMaxSpeed;      // cells/s
extern float carAccel;         // cells/s^2
extern float carBrake;         // cells/s^2
extern float carLatAccel;      // cells/s^2: cornering limit
extern float carMaxSteerDeg;   // front wheel lock
extern float carSteerRate;     // degrees/s
extern float carLookahead;     // cells (minimum; grows with speed)
extern float carStopWait;      // seconds parked at each stop
extern float carPlanMargin;    // pixels: the planner keeps the body this far from walls
extern bool carFollow;
extern int carTrafficCount;    // cars in the cars-traffic scenario
#define CAR_DEADLOCK_S 10.0f       // stopped behind a car this long counts as a deadlock         // GUI camera follows car 0
void CarsFollowCamera(float frameTime);
extern bool carDebug;          // draw route, lookahead, clearance

void ClearCars(void);
int SpawnCar(int cellX, int cellY, int z, float headingDeg);   // centre on that cell's centre
void CarAddStop(int carIdx, int cellX, int cellY, char name);
bool CarDriveTo(int carIdx, int cellX, int cellY);              // plan; false = no route
void CarsTick(float dt);

// Planner view (debug draw + tests)
int CarNodeSize(void);                          // ceil(carWidth)
int CarClearance(int x, int y, int z);          // largest drivable square with (x,y) top-left
bool CarCellDrivable(int x, int y, int z);
bool CarPoseBlocked(float x, float y, int z, float heading);   // body vs cells, exact

// Planner stats (headless output)
extern int carPlanCount, carPlanExpansionsMax;
extern double carPlanMsTotal, carPlanMsMax;

const char* DriveStateName(int state);
void CarsPrintStats(void);
void DrawCars(void);   // render/cars_draw.c

#endif
