# Elevators

> Status: partial (2026-11-23: passenger elevator built: shaft cells, one car per shaft,
> waiting, riding, capacity, SCAN calls; scenario `elevator`)

## Built (2026-11-23)

Code: `src/entities/elevators.c/.h`; hooks in `mover.c` (movement loop), `cell_defs.h`
(`CF_SHAFT`, `CanClimbUpAt`/`CanClimbDownAt`), `rendering.c` (`DrawElevators`).

- **Shaft** = a column of `CELL_ELEVATOR` cells (one per level, self-supporting like a
  ladder). The pathfinder links neighbouring shaft cells like ladder cells (Option C below):
  every system (jobs, needs, wandering) reaches floors served only by an elevator.
- **Car**: one per shaft, found from the grid (`RebuildElevators`, also lazily when a mover
  finds a shaft with no car). Idle until called; moves at `elevatorSpeed` levels/s, doors open
  `elevatorDoorTime` s at a stop; SCAN: keeps its direction while anyone rides or waits that
  way, then turns.
- **Movers**: a path that changes level in a shaft (within 8 waypoints, 4 cells of the
  shaft) makes the mover join the **line at the door** of its level: it starts next to the
  shaft on the side with the longest straight run of free cells and goes straight out (a
  second row beside it when longer), first come first served; walkers go around the shaft,
  not through it. When the doors open, the first in line whose trip goes the car's way steps
  in, if there is room (`elevatorCapacity`); the doors wait for it. Riding, its z follows
  the car. It gets off at the last shaft waypoint of its path (a new path while aboard sets
  a new stop). A coarse path's window that ends at the shaft door is refined right away
  (`MoverRefillRouteNow`), so the ride is on the path before the mover lines up.
- **Starved floors and the box bug** (2026-11-23): measured with `ElevatorPrintWaitStats`
  (elevator-stairs, 15 movers): waits of 66 game hours on the middle floors, and waiters
  never switched to the stairs. Two causes:
  - The boarding cost only counted the line; a floor the car passes full looked cheap.
    Now it is at least the longest current wait in that line, and waiters re-plan every
    15 s after 10 s.
  - **Pathfinder bug**: a path inside one chunk (and each refined leg) came from a local
    search in a box around the chunk(s) and was taken whatever it cost, so stairs just
    outside the box were never seen. Now, when such a path uses a ladder link (whose cost
    has query-time parts: queues, an elevator's wait), the library also tries the abstract
    route / one chunk further and keeps the cheaper (`nav_LocalPath`, `NavFindPathHPA`,
    `NavFindRouteHPA`). Ladder queues had the same blind spot.
  - With that fixed, the 1.5 floors/s car with 1.5 s doors lost almost every trip to the
    stairs (14 rides vs 1218 climbs). New defaults: **3 floors/s, doors 0.75 s**:

    | car | rides | climbs | median wait | arrivals |
    |---|---|---|---|---|
    | 1.5 /s, 1.5 s | 14 | 1218 | 0 s | 1923 |
    | 3 /s, 0.75 s | 186 | 702 | 1.0 s | 1172 |
    | 5 /s, 0.5 s | 285 | 556 | 1.3 s | 930 |

    Faster cars mean *fewer* arrivals: the ride cost is optimistic (it leaves out the
    car's stops for others on the way and stepping in). Next: an honest ride cost.
- **Honest costs and real stairs** (2026-11-23), `elevator-stairs` with trip times:
  - Stairs were walked at full speed: movers cross a ramp in the walking code, so the
    climb-branch slowdown never applied. Now walking speed is multiplied by the stairs
    factor on a built ramp, and `stairsClimbSpeed` is 0.2 (a storey in one ramp cell is
    ladder-steep: ~5 cells of walking per level). Ladders are slower still:
    `ladderClimbSpeed` 0.15 (was 0.5), ~7 cells per level. Single-file ladder scenarios
    move far fewer people (ladders 161 -> 61, ladder-choice 225 -> 102, 48 movers
    328 -> 134 arrivals at 3000 ticks); the games have no ladders.
  - Ride cost: doors at both ends, the chance of stops for others on the way, and once
    measured (3+ rides) the car's real seconds per level and each level's real average
    wait (running averages on the Elevator).
  - A waiter re-planning in its own line doesn't count its own past wait and sees the
    line at 75% (leaves only for a clearly better way); first re-plan at 3 s at least.
  - 15 movers, 18000 ticks, car 3 / 8 floors/s: arrivals 1012 / 1008 (a faster car no
    longer means fewer); left a line without boarding 41% / 28% (was up to 54%);
    9-floor trip 9.5 / 6.9 s by elevator vs 11.4 / 8.9 s by stairs.
- **Leaving a line is left as is** (decided 2026-11-23): 28-41% of joins in
  `elevator-stairs` (15 movers riding one car nonstop) end without boarding; those trips
  take ~6 s more than the stairs, 8% of all trip time. It is what people do (see the
  crowd, take the stairs), and knowing who else is on the way (counting movers whose route
  passes the shaft) would be less realistic. If it ever looks odd in play: re-check when
  the line comes into view, a few cells before the door.
- **Sweep pickup** (`elevatorDirectionalPickup`, on): an empty car on its way to calls
  further on picks up only people going its way; the others ride on its way back. Before,
  an empty car stopped for anyone and turned round, and the calls further on waited.
  Scenario `elevator-tall` (10 floors, one car, no stairs), 18000 ticks:

  | movers | rule | detours | avg wait | longest | boardings |
  |---|---|---|---|---|---|
  | 8  | old / sweep | 0 / 0 | 11.1 / 11.1 s | 76 / 76 s | 132 / 132 |
  | 15 | old / sweep | 5 / 0 | 25.8 / 22.0 s | 186 / 135 s | 124 / 134 |
  | 30 | old / sweep | 4 / 0 | 30.2 / 33.5 s | 155 / 139 s | 139 / 135 |

  It matters at medium load. At 30 movers the one car is simply saturated (never idle,
  doors open 62% of the time, ~3 of 4 places taken, ~17 in line): more cars or capacity
  help there, not the order of service.
- **Not saved**: after a load the cars start at the bottom, nobody is aboard, and a mover
  caught between levels is put on the nearest level and re-plans.
- **Route cost**: the wait is paid **once**, the levels are travel. Two optional hooks in
  the nav library (`nav/nav.h`, default 0): `NAV_ENTER_EXTRA` (query time: entering a cell
  by any step but a ladder link; graph edges add it where they end at an entrance) and
  `NAV_RAMP_EXTRA` (static: a ramp level, cached in the graph).
  - Boarding (`ElevatorBoardCost`, `NAV_ENTER_EXTRA` on shaft cells): the expected wait at
    that level: doors, the car coming here, one round trip per full car already in the
    line (`elevatorWaitCost` off: the doors only).
  - Each level (`ElevatorExtraCost`, `NAV_CLIMB_EXTRA`): the car's travel time.
  - Before (until 2026-11-23): half the wait on every level, and the car's distance
    re-counted at every level: a 1->10 ride cost ~25 s for ~7.5 s of real time.
  - Movers in a line re-plan at 4 s and 10 s (x patience): stairs when the line is long.
  - Scenario `elevator`: 543 -> 591 arrivals with the one-time wait (nothing else changes).
- **Built ramps are stairs** (2026-11-23): a ramp built from an item (CONSTRUCTION_RAMP
  records its source item; scenarios use `MarkBuiltStairs`) is climbed at
  `stairsClimbSpeed` 0.7 of walking (down x1.4, at most walking), and routes see that time
  plus `stairsEffort` (2 cells up, 1 down) through `NAV_RAMP_EXTRA`. Terrain slopes and
  ramps dug into natural rock have no source item: unchanged. Ladders stay the slowest
  (0.5). (The natural flag can't tell: no generator marks its ramps natural.)
  `elevator-stairs` (10 floors, one car, one staircase), 6 movers, 18000 ticks:

  | | 1-2 floors ride/climb | 6+ floors ride/climb | long trips riding | arrivals |
  |---|---|---|---|---|
  | wait per level, free stairs (before) | 45 / 2 | 24 / 15 | 62% | 182 |
  | wait once | 33 / 8 | 27 / 17 | 61% | 174 |
  | wait once + built stairs | 44 / 9 | 33 / 11 | 75% | 174 |

  With 15 movers the line averages ~21 s and half the long trips climb: quicker then.
  scenarios-check: only the elevator scenarios change (elevator 591 -> 508: its 24 movers
  relied on a staircase that now takes real time; elevator-stairs 79 -> 71); hubs, island,
  subway and the games are unchanged.
- **Sweep pickup** (`elevatorDirectionalPickup`, on): an empty car on its way to calls
  further on picks up only people going its way; the others ride on its way back. Before,
  an empty car stopped for anyone and turned round, and the calls further on waited.
  Scenario `elevator-tall` (10 floors, one car, no stairs), 18000 ticks:

  | movers | rule | detours | avg wait | longest | boardings |
  |---|---|---|---|---|---|
  | 8  | old / sweep | 0 / 0 | 11.1 / 11.1 s | 76 / 76 s | 132 / 132 |
  | 15 | old / sweep | 5 / 0 | 25.8 / 22.0 s | 186 / 135 s | 124 / 134 |
  | 30 | old / sweep | 4 / 0 | 30.2 / 33.5 s | 155 / 139 s | 139 / 135 |

  It matters at medium load. At 30 movers the one car is simply saturated (never idle,
  doors open 62% of the time, ~3 of 4 places taken, ~17 in line): more cars or capacity
  help there, not the order of service.
- **Not saved**: after a load the cars start at the bottom, nobody is aboard, and a mover
  caught between levels is put on the nearest level and re-plans.
- **Route cost**: the wait is paid **once**, the levels are travel. Two optional hooks in
  the nav library (`nav/nav.h`, default 0): `NAV_ENTER_EXTRA` (query time: entering a cell
  by any step but a ladder link; graph edges add it where they end at an entrance) and
  `NAV_RAMP_EXTRA` (static: a ramp level, cached in the graph).
  - Boarding (`ElevatorBoardCost`, `NAV_ENTER_EXTRA` on shaft cells): the expected wait at
    that level: doors, the car coming here, one round trip per full car already in the
    line (`elevatorWaitCost` off: the doors only).
  - Each level (`ElevatorExtraCost`, `NAV_CLIMB_EXTRA`): the car's travel time.
  - Before (until 2026-11-23): half the wait on every level, and the car's distance
    re-counted at every level: a 1->10 ride cost ~25 s for ~7.5 s of real time.
  - Movers in a line re-plan at 4 s and 10 s (x patience): stairs when the line is long.
  - Scenario `elevator`: 543 -> 591 arrivals with the one-time wait (nothing else changes).
- **Stairs effort** (`stairsEffort`, cells of walking per level up a ramp, down half;
  `NAV_RAMP_EXTRA`): **0 by default, scope open.** Our ramps cost ~1.4 cells per level
  and are climbed at walking speed, so even a fair elevator loses long trips to the stairs.
  `elevator-stairs` (10 floors, one car, one straight staircase), 6 movers, 18000 ticks:

  | | 1-2 floors ride/climb | 6+ floors ride/climb | long trips riding | arrivals |
  |---|---|---|---|---|
  | old cost | 45 / 2 | 24 / 15 | 62% | 182 |
  | one-time wait | 33 / 8 | 27 / 17 | 61% | 174 |
  | + stairsEffort 4 | 37 / 3 | 32 / 9 | 78% | 162 |

  With effort on all ramps, routes avoid climbing but climbing is still as fast as
  walking, so detours make trips slower: hubs 910 -> 867, island 366 -> 341, elevator
  543 -> 472 arrivals. Options: effort on built stairs only (not hill slopes); climbing
  physically slower too (like ladders), so cost and time agree.
- Scenario `elevator`: a six-floor tower, two shafts and a straight staircase up the west
  side, 24 movers, half of their goals in the tower (up there 70% of next goals on the same
  floor). 12000 ticks with the wait cost: 2172 arrivals, 2-4 in line, 1926 levels on the
  stairs; without it: 1347 arrivals, 12-14 in line, 862 levels on the stairs.

Left (in order of use):
1. ~~Waiting line in front of the door~~ done.
2. ~~Route cost of the wait~~ done (per level; an exact one-off boarding cost would need
   the pathfinder to know where a ride starts).
3. ~~Building shafts in game~~ done: recipe `CONSTRUCTION_SHAFT` (4 planks + 2 cordage per
   level, `BUILD_SHAFT`), Work → Build → Elevator (key e) and play HUD "Build Elevator".
   Built like a ladder: on a floor cell a shaft from there up one level (cutting the floor
   above), on a shaft top one more level; the car runs once it spans two. Right-drag takes
   the whole column down (a plank back per level, floors restored, riders let off).
   Scenario `elevator-build`. Left: a proper shaft sprite (today `dark_shade`).
4. ~~Elevator settings in the save~~ done (v100). Left: freight elevator (crates in the car).

Full simulation elevators for colony sim: moving platforms with state, queuing, capacity, and wait times.

---

## Overview

Unlike ladders/stairs (instant z-transition), elevators are **simulated entities** that:
- Physically move between floors
- Have limited capacity
- Require movers to wait, board, ride, exit
- Create natural bottlenecks and queuing

---

## Elevator Entity

```c
typedef enum {
    ELEVATOR_IDLE,        // Waiting at a floor, doors closed
    ELEVATOR_DOORS_OPEN,  // Doors open, movers can board/exit
    ELEVATOR_MOVING_UP,
    ELEVATOR_MOVING_DOWN,
} ElevatorState;

typedef struct {
    int x, y;                    // Grid position (fixed)
    float z;                     // Current z-level (float for smooth movement)
    ElevatorState state;
    
    float speed;                 // Floors per second
    float doorTime;              // How long doors stay open
    float stateTimer;            // Time in current state
    
    int capacity;                // Max movers inside
    int* ridingMovers;           // Mover indices currently inside
    int ridingCount;
    
    int* floors;                 // Which z-levels this elevator stops at
    int floorCount;
    
    int targetFloor;             // Where it's going (-1 if idle)
    int callQueue[MAX_FLOORS];   // Floors that have been "called"
    int callQueueCount;
} Elevator;
```

---

## Mover Integration

### Path Planning

When pathfinder encounters an elevator:
1. Add edge in graph: `(x, y, z_from)` → `(x, y, z_to)`
2. Cost = `estimatedWaitTime + travelTime`
3. Estimated wait = function of distance from elevator's current position

```c
float EstimateElevatorCost(Elevator* e, int fromZ, int toZ) {
    // How far is elevator from our floor?
    float distToPickup = fabsf(e->z - fromZ);
    float waitTime = distToPickup / e->speed + e->doorTime;
    
    // Travel time
    float travelDist = abs(toZ - fromZ);
    float travelTime = travelDist / e->speed + e->doorTime;
    
    return waitTime + travelTime;
}
```

### Mover States

Movers need new states for elevator interaction:

```c
typedef enum {
    MOVER_WALKING,
    MOVER_WAITING_FOR_ELEVATOR,  // In queue at elevator cell
    MOVER_BOARDING_ELEVATOR,     // Moving into elevator
    MOVER_RIDING_ELEVATOR,       // Inside, position locked to elevator
    MOVER_EXITING_ELEVATOR,      // Moving out of elevator
} MoverElevatorState;
```

### Behavior Flow

```
1. Mover path includes elevator
2. Mover walks to elevator cell
3. Mover enters WAITING_FOR_ELEVATOR
   - Joins queue for this floor
   - Calls elevator (adds floor to callQueue)
4. Elevator arrives, doors open
5. If capacity allows: mover enters BOARDING_ELEVATOR
   - Moves into elevator cell
   - Added to ridingMovers
6. Mover enters RIDING_ELEVATOR
   - Position locked to (elevator.x, elevator.y, elevator.z)
7. Elevator reaches target floor, doors open
8. Mover enters EXITING_ELEVATOR
   - Removed from ridingMovers
9. Mover resumes WALKING on new z-level
```

---

## Elevator Logic

### State Machine

```
IDLE
  └─> call received → decide direction → MOVING_UP or MOVING_DOWN

MOVING_UP / MOVING_DOWN
  └─> reached target floor → DOORS_OPEN

DOORS_OPEN
  ├─> movers exit (those whose destination is this floor)
  ├─> movers board (those waiting, if capacity)
  └─> doorTime elapsed → check for more calls
        ├─> more calls → MOVING_UP or MOVING_DOWN
        └─> no calls → IDLE
```

### Call Scheduling (Simple)

Start with simple FIFO or nearest-floor:

```c
int GetNextTargetFloor(Elevator* e) {
    if (e->callQueueCount == 0) return -1;
    
    // Option A: FIFO - just take first call
    return e->callQueue[0];
    
    // Option B: Nearest floor
    int nearest = e->callQueue[0];
    float nearestDist = fabsf(e->z - nearest);
    for (int i = 1; i < e->callQueueCount; i++) {
        float dist = fabsf(e->z - e->callQueue[i]);
        if (dist < nearestDist) {
            nearest = e->callQueue[i];
            nearestDist = dist;
        }
    }
    return nearest;
}
```

### Call Scheduling (Better - SCAN/Elevator Algorithm)

Real elevators use SCAN: continue in one direction, servicing all calls, then reverse.

```c
int GetNextTargetFloor_SCAN(Elevator* e) {
    if (e->callQueueCount == 0) return -1;
    
    // Continue in current direction if possible
    int bestInDirection = -1;
    int bestReverse = -1;
    
    for (int i = 0; i < e->callQueueCount; i++) {
        int floor = e->callQueue[i];
        if (e->direction == DIR_UP && floor > e->z) {
            if (bestInDirection == -1 || floor < bestInDirection)
                bestInDirection = floor;
        } else if (e->direction == DIR_DOWN && floor < e->z) {
            if (bestInDirection == -1 || floor > bestInDirection)
                bestInDirection = floor;
        } else {
            // Track closest in opposite direction for later
            if (bestReverse == -1 || fabsf(floor - e->z) < fabsf(bestReverse - e->z))
                bestReverse = floor;
        }
    }
    
    if (bestInDirection != -1) return bestInDirection;
    
    // Reverse direction
    e->direction = (e->direction == DIR_UP) ? DIR_DOWN : DIR_UP;
    return bestReverse;
}
```

---

## Queuing at Floors

Each floor needs a queue of waiting movers:

```c
typedef struct {
    int elevatorIndex;
    int floorZ;
    int* waitingMovers;
    int waitingCount;
    int capacity;  // Max queue size (physical space)
} ElevatorQueue;
```

### Queue Position

Movers in queue need positions near the elevator, not all stacked on one cell:

```c
Vector2 GetQueuePosition(ElevatorQueue* q, int positionInQueue) {
    // Arrange in line or arc near elevator
    float offsetX = (positionInQueue % 3) - 1;  // -1, 0, 1
    float offsetY = (positionInQueue / 3) + 1;  // 1, 2, 3...
    return (Vector2){
        elevator.x + offsetX,
        elevator.y + offsetY
    };
}
```

---

## Pathfinder Integration

### Option A: Elevator as Special Entrance

Treat elevator like HPA* entrance but connecting multiple z-levels:

```c
typedef struct {
    int elevatorIndex;
    int x, y;
    int* connectedFloors;
    int floorCount;
} ElevatorNode;
```

Graph edges between all floor pairs this elevator connects.

### Option B: Dynamic Cost Edges

Edges exist but cost is queried at pathfind time:

```c
int GetEdgeCost(int fromNode, int toNode) {
    if (IsElevatorEdge(fromNode, toNode)) {
        Elevator* e = GetElevatorForEdge(fromNode, toNode);
        return EstimateElevatorCost(e, ...);
    }
    return staticEdgeCosts[fromNode][toNode];
}
```

This lets pathfinder prefer stairs when elevator is busy/far.

### Option C: Avoid in Pathfinder, Handle in Mover

Pathfinder just knows "elevator connects these floors with cost X".
Mover handles all the waiting/boarding/riding.
Simplest integration, might pick elevator even when stairs are faster.

**Recommendation:** Start with Option C, upgrade to B if needed.

---

## Visual Representation

### Elevator Shaft

Mark cells as `CELL_ELEVATOR_SHAFT` - not walkable except for elevator platform.

```
z=3:  [shaft]  ← elevator can be here
z=2:  [shaft]  ← elevator can be here  
z=1:  [shaft]  ← elevator currently here, doors open
z=0:  [shaft]  ← elevator can be here
```

### Rendering

- Draw shaft as vertical column
- Draw elevator platform at current z (interpolate for smooth movement)
- Draw doors (open/closed state)
- Draw waiting movers in queue
- Draw riding movers on platform

---

## Edge Cases

### Elevator Full
- Movers stay in queue, wait for next trip
- Maybe: repath to stairs if wait too long?

### Mover Destination Unreachable
- Elevator breaks down / disabled
- Need to detect and repath

### Multiple Elevators
- Movers pick nearest/fastest
- Or: assign elevators to regions

### Mover Pushed Out of Queue
- Other movers' avoidance pushes waiting mover
- Need to maintain queue position or re-queue

### Elevator Called While Moving
- Add to callQueue, service on the way or next trip

---

## Implementation Phases

### Phase 1: Basic Elevator Entity
- [ ] Elevator struct with state machine
- [ ] Smooth z movement
- [ ] Doors open/close timer
- [ ] Single elevator, hardcoded floors

### Phase 2: Mover Waiting
- [ ] Mover detects "need elevator" in path
- [ ] Mover waits at elevator cell
- [ ] Mover boards when doors open
- [ ] Mover rides (locked to elevator z)
- [ ] Mover exits at destination

### Phase 3: Queuing
- [ ] Queue structure per floor
- [ ] Queue positions (not stacked)
- [ ] Capacity limits (skip if full)
- [ ] Call button (mover adds to callQueue)

### Phase 4: Smart Scheduling
- [ ] SCAN algorithm
- [ ] Multi-call optimization
- [ ] Express elevators (skip floors)

### Phase 5: Pathfinder Integration
- [ ] Elevator edges in HPA* graph
- [ ] Dynamic cost based on elevator state
- [ ] Prefer stairs when elevator busy

### Phase 6: Multiple Elevators
- [ ] Elevator manager
- [ ] Assignment to movers
- [ ] Load balancing

---

## Questions to Decide

1. **Can movers take stairs instead?** If yes, need cost comparison.

2. **Elevator capacity** - 4? 8? Configurable?

3. **Door behavior** - Stay open while movers boarding? Close after timeout?

4. **What if mover's path changes while riding?** Exit early?

5. **Visual style** - Platform only? Full cabin? See-through shaft?

6. **Sound/feedback** - Ding on arrival? UI indicator?

---

## References

- Dwarf Fortress doesn't have elevators (minecarts on tracks instead)
- RimWorld has no elevators (single z-level mostly)
- Prison Architect has no elevators
- SimTower is THE elevator game - complex simulation

SimTower's elevator logic:
- Passengers have patience timers
- Elevators have express/local modes
- Rush hour patterns
- Maintenance/breakdowns
