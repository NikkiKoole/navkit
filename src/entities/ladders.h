#ifndef LADDERS_H
#define LADDERS_H

// Single-file ladders (ladders.c): one climber per ladder segment, first-come-
// first-served lines at both ends (service points), the queue's route cost
// (NAV_CLIMB_EXTRA, see world/nav_host.h) and climbing speed.

#include <stdbool.h>
#include "../world/pathfinding.h"

extern bool useSingleFileLadders;    // One climber per ladder segment at a time
extern bool useLadderQueueCost;      // Routes see ladder queues as extra cost
extern float ladderClimbSpeed;       // Climbing speed vs walking (0.5 = twice as slow)

void LaddersClear(void);                       // ClearMovers
void LaddersUpdate(float dt);                  // once per tick, before movement
float LadderClimbSpeedFactor(void);            // ladderClimbSpeed, clamped
int LadderQueueExtraCost(int x, int y, int z, int dz);   // NAV_CLIMB_EXTRA hook (nav_host.h)

// Movement hooks (mover.c UpdateMovers)
bool LadderHoldAtFoot(int i, Point target, int segLow, float dist, float dxf, float dyf,
                      float effectiveSpeed, float dt);    // true = wait this tick
void LadderClimbDone(int i);
bool LadderFollowLine(int i, float effectiveSpeed, float* vx, float* vy);   // true = lining up

#endif
