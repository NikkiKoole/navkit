# Elevators

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
