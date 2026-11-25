// throwing.h - throwing things (docs/todo/gameplay/throwing.md)
//
// A mover throws a small item at a goal cell (JOBTYPE_THROW): fetch it (or take it out of its
// hands / pockets), walk until a throw from where it stands would come down at the goal, wind
// up, let go. From then on the item flies by itself: real 3D flight in cells (x, y, height in
// levels, one level = one cell) under gravity, bouncing off walls, ground, floors and ceilings,
// losing speed each bounce, rolling to a stop. At rest it is an ordinary item on the ground.
//
// Whether it reaches the goal depends on: the item's weight (top speed), the distance and the
// height difference, walls in the way (the thrower picks the flattest arc that would get there,
// a lob over a wall if needed), and the throw going a bit wrong (direction and strength, more
// so far away). Beyond its range it falls short.
//
// Hitting: a stone flying (faster than a bump) through an animal's body scares it: it runs from
// the thrower for animalScareHours game hours (animals.c) and the stone bounces off it. One hit per
// throw. Movers aren't hit yet.
//
// Light and small enough: one unit of an item that isn't two-handed and weighs at most
// throwMaxWeight kg (a stack: one unit is split off). An item in flight is ITEM_IN_FLIGHT: no
// job takes it. Transient: the flight isn't saved, a load puts flying items on the ground.
#ifndef THROWING_H
#define THROWING_H

#include <stdbool.h>

extern float throwSpeed;       // cells/s: top launch speed for a 1 kg item
extern float throwGravity;     // cells/s^2
extern float throwBounce;      // 0..1: speed kept straight off a surface on a bounce
extern float throwSpread;      // how much a throw goes wrong (1 = normal, 0 = perfect)
extern float throwMaxWeight;   // kg per unit: heavier things aren't thrown

#define THROW_MAX_FLIGHTS 256

// Stats since ClearThrows (scenario checks)
extern long throwCount, throwBounceCount, throwHitCount;

bool ItemThrowable(int itemIdx);                 // light and small enough (one unit of it)
float ThrowRange(int itemIdx);                   // cells on flat ground, flattest useful arc

// Give the mover a throw job at goal cell (gx, gy, gz). itemIdx -1 = something throwable in its
// hands or pockets, else the nearest throwable item on the ground. Cancels its current job.
// Returns the job id, -1 (and *why set) if it can't.
int ThrowOrder(int moverIdx, int itemIdx, int gx, int gy, int gz, const char** why);

// How to throw: the arc (degrees up from level) and the point (cells, level) a perfect throw
// along it would first come down on. Aimed short of the goal by how far it hops on.
typedef struct { float angle, x, y, h; } ThrowAim;

// The flattest comfortable arc from (x, y, h) (cells, h in levels) whose perfect throw comes to
// rest within `tolerance` cells of goal cell (gx, gy, gz). False if none does (out of range,
// walled in).
bool ThrowPlan(int itemIdx, float x, float y, float h, int gx, int gy, int gz, float tolerance, ThrowAim* aim);

// Let go of an item at (x, y, h) along `aim`; `noisy`: the throw goes a bit wrong (direction,
// strength). thrower -1 = none.
void ThrowLaunch(int itemIdx, float x, float y, float h, ThrowAim aim, bool noisy, int thrower);

void ThrowsTick(float dt);         // after ItemsTick
void ClearThrows(void);
void ThrowLandAll(void);           // after a load: flying items lie where they are
int ThrowFlightCount(void);

// Drawing (render): flight i -> item, position in cells, height above its shadow (levels) and
// the shadow's level. False if i is not a live flight.
bool ThrowFlightInfo(int i, int* itemIdx, float* x, float* y, float* h, int* shadowZ);

// Job driver (jobs.c table)
#include "jobs.h"
JobRunResult RunJob_Throw(Job* job, void* mover, float dt);

#endif
