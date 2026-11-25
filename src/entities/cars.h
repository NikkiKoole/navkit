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

#define MAX_CARS 16384
#define CAR_MAX_PATH 4096   // 4 px per point: routes up to ~500 cells
#define CAR_MAX_STOPS 16
#define CAR_MAX_ROUTE 512   // road segments in a road route

typedef enum {
    DRIVE_PARKED,     // waiting at a stop (or no stops)
    DRIVE_FOLLOW,     // following the route (forward or reverse stretch)
    DRIVE_STUCK,      // no route from here; plans again after a while
    DRIVE_ROAD,       // driving the road network (traffic.c)
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

    // Route: car-centre points (pixels, 4 px apart) with the gear for each step.
    // CAR_MAX_PATH each, allocated on the first plan (road cars never plan).
    float *pathX, *pathY;
    signed char* pathDir;   // +1 forward, -1 reverse (to reach the next point)
    float* pathH;           // planned heading at each point
    float* pathSteer;       // planned wheel angle to reach the next point
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

    // Road driving (src/entities/traffic.c): a route of road segments (lanes and the
    // connectors through junctions), the position along the current one
    bool onRoad;
    unsigned short route[CAR_MAX_ROUTE];   // RdSegId (roads.h)
    int routeLen, routeIdx;
    float s;               // pixels along route[routeIdx] (car centre)
    float destS;           // trip ends at destS on the last segment
    int holdNode;          // the connector segment we hold (-1: none)
    bool requested;        // asked the next junction for its connector
    float stillTime;       // seconds at (nearly) zero speed, this time
    float gap;             // pixels to what we follow (debug)
    int bay;               // the bay we're parked in or driving to (-1: none)
    int bayFrom;           // the bay we're pulling out of (freed once we're on the lane)
    float parkTime;        // seconds left parked (> 0: parked)
    float pullWait;        // seconds waiting for a gap to pull out
    bool cab;              // a yellow cab: no parking, stops in the lane at each fare
    float stopTime;        // cab: seconds left standing at a fare
    float noRoom;          // seconds our junction request waits for room past the junction
    int reroutes;          // times we planned round a full exit
    float latOff;          // px: changing lanes, our old lane's offset from the new one (shrinks to 0)
    int ghostLane;         // changing lanes: the lane we're leaving, still partly in it (-1: none)
    float lcCool;          // seconds before we may change lanes again
    float rerouteCool;     // seconds before we try another way round a full exit again
    int overtakes, keepRights;   // lane changes: out to overtake, back to the right-hand lane
    int why;               // TrafficWhy: why we brake (debug)

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
extern int carCityCount;       // cars in the cars-city scenario
extern int carCitySeed;        // its road layout
extern bool carCityLights;     // traffic lights on its 4-way junctions
extern int carCityRoundabouts; // 4-way junctions that become roundabouts
extern bool carCityAvenues;    // middle row + column: two lanes each way, overtaking
extern bool carCityParking;    // cars park in bays between trips (off: they just drive)
extern bool carCitySlots;      // the abstract slot traffic (slots.h) instead of steered cars
extern float carGapCells;      // road cars: standstill gap to the car ahead (cells)
extern float carCitySpacingX, carCitySpacingY;   // cars-city: cells between junctions
extern int carCityJitter;      // cars-city: junctions moved up to this many cells at random
extern float carCityBends;     // cars-city: share of roads with a bend
extern float carCitySetback;   // cars-city: lanes stop this far from a junction's centre
extern int carCityLayout;      // cars-city: 0 grid, 1 Manhattan (one-way avenues and streets, Broadway)
extern float carCityCabs;      // cars-city: share of yellow cabs
extern bool carCityGreenWave;  // cars-city: lights timed for a green wave along the avenues
#define CAR_DEADLOCK_S 10.0f       // stopped behind a car this long counts as a deadlock
void CarsFollowCamera(float frameTime);   // GUI camera follows car 0 (carFollow)
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

// Shared with traffic.c (road driving): the car model and its collision tests
float CarAngleWrap(float a);
void CarAdvancePose(float x, float y, float h, float dist, float steer, float* ox, float* oy, float* oh);
void CarSteerToward(Car* c, float want, float dt);   // turn the wheel toward want at carSteerRate
bool CarPoseBlockedMargin(float x, float y, int z, float heading, float margin);   // body (+margin px) vs walls
bool CarBodiesOverlap(float x1, float y1, float h1, float x2, float y2, float h2, float margin);

const char* DriveStateName(int state);
void CarsPrintStats(void);
void DrawCars(void);   // render/cars_draw.c

#endif
