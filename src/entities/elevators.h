// elevators.h - passenger elevators: a shaft (a column of CELL_ELEVATOR cells, one per
// level) and one car per shaft that carries movers between levels.
//
// The pathfinder sees a shaft like a ladder (CanClimbUpAt/CanClimbDownAt link the
// shaft cells of neighbouring levels), so every system reaches every floor. A mover
// whose path changes level in a shaft waits there for the car, boards when its doors
// open on its level, rides, and gets off at the level where its path leaves the shaft.
// The car serves calls SCAN-style: on in one direction while anyone waits or rides that
// way, then turns.
//
// Shafts are found from the grid (RebuildElevators); nothing is saved: after a load the
// cars start at the bottom and nobody is aboard (ElevatorsAfterLoad).
#ifndef ELEVATORS_H
#define ELEVATORS_H

#include <stdbool.h>
#include "../world/grid.h"

#define MAX_ELEVATORS 64
#define ELEVATOR_MAX_RIDERS 8

typedef enum { CAR_IDLE, CAR_MOVING, CAR_DOORS } CarState;

typedef struct {
    int x, y, zMin, zMax;      // the shaft: CELL_ELEVATOR at (x, y, zMin..zMax)
    float z;                   // the car's level (fractional while moving)
    int dir;                   // +1 up, -1 down, 0 none yet
    CarState state;
    float timer;               // seconds the doors stay open
    int riders[ELEVATOR_MAX_RIDERS];
    int riderCount;
    int trips;                 // stats: stops with doors opened
} Elevator;

extern Elevator elevators[MAX_ELEVATORS];
extern int elevatorCount;

extern float elevatorSpeed;      // levels per second at dayLength 60 (setting)
extern int elevatorCapacity;     // riders per car (setting, <= ELEVATOR_MAX_RIDERS)
extern float elevatorDoorTime;   // seconds the doors stay open at a stop (dayLength 60)
extern int elevatorRides;        // stats: boardings since the last clear
extern bool useElevatorWaitCost; // routes count the expected wait for the car (setting)

int ElevatorExtraCost(int x, int y, int z, int dz);   // route cost of a shaft level change
int ClimbExtraCost(int x, int y, int z, int dz);      // NAV_CLIMB_EXTRA: ladders + elevators

void RebuildElevators(void);     // after shaft cells change (keeps cars that still exist)
void ClearElevators(void);
void ElevatorsAfterLoad(void);   // LoadWorld: rebuild, put movers caught mid-shaft on a level
void ElevatorsUpdate(float dt);  // once per tick, before movement: cars move, doors time out

int ElevatorAt(int x, int y, int z);   // the elevator whose shaft holds (x,y,z), or -1

// Mover hooks (mover.c movement loop), in this order; true = handled this tick (skip
// normal movement)
// A rider: walks into the car, then the car carries it (z follows the car) until it
// gets off where its path leaves the shaft.
bool ElevatorRiderTick(int moverIdx, float dt);
// The path changes level in a shaft a few waypoints ahead and the mover is near it:
// join the line at the door (first come, first served), step in when the doors open.
bool ElevatorQueueTick(int moverIdx, float dt);
// Fallback: the mover stands in the shaft cell with a level change next (it never
// passed the line): wait there (true) or board.
bool ElevatorWaitAt(int moverIdx, Point target, int fromZ);
bool ElevatorCarBoarding(int k);   // a rider is still stepping in: the doors wait
bool ElevatorMoverAboard(int moverIdx);
bool ElevatorMoverWaiting(int moverIdx);
void ElevatorForgetMover(int moverIdx);   // mover removed / teleported

#endif
