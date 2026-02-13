# Train Tracks & Stations

Horizontal multi-stop transport system. Conceptually: a horizontal elevator with a fixed track and multiple stations.

---

## Core Concept

Players draw train tracks, place stations along them, and a cart travels back and forth. Movers decide at pathfinding time whether walking directly or taking the train is faster.

```
Walking network:  [mover at A] --walk--> [station X]
Transport graph:  [station X]  --ride--> [station Y]  (cost = wait + travel time)
Walking network:  [station Y]  --walk--> [destination B]
```

The mover's decision: is walk(A->B) cheaper than walk(A->X) + wait + ride(X->Y) + walk(Y->B)?

---

## Relationship to Elevators

A train is a horizontal, multi-stop elevator on a fixed track.

| Elevator concept | Train equivalent |
|---|---|
| Shaft (vertical cells) | Track (horizontal cells) |
| Floors it stops at | Stations along the track |
| Single elevator car | Train cart entity |
| Call queue per floor | Waiting queue per station |
| SCAN algorithm (up/down) | Back-and-forth on track (or loop) |
| Capacity limit | Seat count |
| `MOVER_RIDING_ELEVATOR` | `MOVER_RIDING_TRAIN` |

Both systems share the same mover state machine (wait/board/ride/exit) and the same queuing prerequisite.

---

## Data Model

### New Cell Types

```c
CELL_TRACK      // Walkable, drawn as rails, cart travels along these
CELL_STATION    // Walkable, drawn as platform, movers board/exit here
```

### Train Line

A named route connecting stations along a track.

```c
typedef struct {
    int stationCount;
    int stationPositions[MAX_STATIONS];  // indices into track path
    int trackPath[MAX_TRACK_LENGTH];     // ordered cell positions along track
    int trackLength;
    float speed;                         // cells per second
    bool loop;                           // true = loop, false = bounce back-and-forth
} TrainLine;
```

### Train Cart

The physical entity that moves along the track.

```c
typedef struct {
    int lineIndex;                       // which TrainLine
    float trackPosition;                 // float position along trackPath
    int direction;                       // +1 or -1 along track
    TrainCartState state;
    float stateTimer;
    float doorTime;                      // how long doors stay open at station

    int capacity;
    int ridingMovers[MAX_CART_CAPACITY];
    int ridingCount;
} TrainCart;

typedef enum {
    CART_MOVING,
    CART_ARRIVING,       // decelerating into station
    CART_DOORS_OPEN,     // movers exit then board
    CART_DEPARTING,      // accelerating out of station
} TrainCartState;
```

### Station

```c
typedef struct {
    int x, y, z;
    int lineIndex;
    int waitingMovers[MAX_STATION_QUEUE];
    int waitingCount;
} TrainStation;
```

---

## Mover State Machine

Same pattern as the elevator doc. New mover states:

```c
MOVER_WAITING_FOR_TRAIN    // in queue at station
MOVER_BOARDING_TRAIN       // moving onto cart
MOVER_RIDING_TRAIN         // position locked to cart
MOVER_EXITING_TRAIN        // moving off cart
```

### Behavior Flow

```
1. Pathfinder determines train is faster
2. Mover walks to boarding station
3. Mover enters WAITING_FOR_TRAIN, joins station queue
4. Cart arrives, doors open
5. Riders whose exit is this station exit first
6. If capacity allows: waiting movers board (FIFO)
7. Mover enters RIDING_TRAIN, position locked to cart
8. Cart reaches mover's exit station, doors open
9. Mover enters EXITING_TRAIN
10. Mover resumes walking to final destination
```

---

## Pathfinder Integration

### Station Graph (same pattern as JPS+ Ladder Graph)

Train stations are portals in a small transport graph, exactly like ladders are portals between z-levels in the JPS+ 3D design.

```c
typedef struct {
    int stationCount;
    int stationPositions[MAX_STATIONS];  // world positions
    float stationToStation[MAX_STATIONS][MAX_STATIONS];  // ride cost between pairs
} StationGraph;
```

### Query: Should the mover take the train?

At pathfinding time:

1. Compute direct walking cost: walk(start -> goal)
2. For each reachable station pair (entry, exit):
   - walkCost = walk(start -> entry) + walk(exit -> goal)
   - rideCost = EstimateTrainCost(entry, exit)
   - totalCost = walkCost + rideCost
3. If best totalCost < direct walking cost: use train

```c
float EstimateTrainCost(TrainLine* line, int fromStation, int toStation) {
    // Wait time depends on where cart currently is
    float distToPickup = TrackDistanceTo(line->cart, fromStation);
    float waitTime = distToPickup / line->speed + line->cart->doorTime;

    // Ride time
    float rideDist = TrackDistanceBetween(fromStation, toStation);
    float rideTime = rideDist / line->speed;

    // Account for intermediate station stops
    int stopsInBetween = CountStopsBetween(fromStation, toStation);
    float stopDelay = stopsInBetween * line->cart->doorTime;

    return waitTime + rideTime + stopDelay;
}
```

### Pruning

Don't check all station pairs. Only consider stations within reasonable walking distance of start and goal. Use spatial hashing or a distance threshold.

### Integration Options (same as elevator doc)

**Option A (recommended to start):** Pathfinder knows "stations connect with cost X". Mover handles waiting/boarding/riding. Simple, works, might sometimes pick train when walking is faster.

**Option B (upgrade later):** Dynamic cost edges queried at pathfind time based on cart position. Lets pathfinder prefer walking when train is far away.

---

## Cart Scheduling

### Simple: Bounce

Cart moves forward along track, stops at each station, reverses at the end. No call system needed.

```
Station A ----> Station B ----> Station C
Station A <---- Station B <---- Station C
```

### Better: Loop

Track forms a loop. Cart always moves in one direction. Movers can predict arrival times.

### Advanced: On-Demand

Cart idles until called (like elevator). Moves to nearest caller, services stations along the way (SCAN algorithm from elevator doc).

Start with bounce. It's simple and creates interesting decisions (do I wait for the return trip or just walk?).

---

## Track Drawing

### Player Interaction

Similar to wall drawing. Player selects track mode, clicks/drags to place track cells. Track must form a connected path (no branching for v1).

Station placement: player marks specific track cells as stations (like designating a cell).

### Constraints

- Track cells must be on walkable ground (needs floor below)
- No branching (single path or loop) for simplicity
- Stations must be on track cells
- Minimum 2 stations per line

---

## Visual Representation

### Track

Rail sprites on ground. Could reuse floor-layer rendering. Two parallel lines or a simple rail texture in the 8x8 atlas.

### Cart

Small sprite that moves along the track. Could show riding movers on top (stacked sprites).

### Station

Platform sprite on the track cell. Queue positions rendered near the platform.

### Rendering Order

Track is floor-level. Cart renders above floor, below walls. Riding movers render on top of cart.

---

## Prerequisites

These systems are needed first (shared with elevators):

### 1. Queuing System (social-navigation.md Phase 1)
Movers waiting at a point without crowding. FIFO ordering, queue positions near the station.

### 2. Multi-Leg Journey
Movers currently walk a single path. They need the ability to split a journey into: walk -> wait -> ride -> walk. This is the same state machine needed for elevators.

### 3. Variable Cost in Pathfinder (nice-to-have)
Currently moveCost is uniform (10/14) in the pathfinder. Speed modifiers (mud, floor, water) only apply at movement time. For the train decision to be accurate, the pathfinder should account for terrain costs. Without this, the train-vs-walk comparison uses distance instead of true travel time. Workable but less accurate.

---

## Implementation Phases

### Phase 1: Track & Cart Entity
- [ ] CELL_TRACK cell type
- [ ] Track drawing in input (similar to wall drawing)
- [ ] TrainCart struct with position along track
- [ ] Cart moves back and forth at fixed speed
- [ ] Cart rendering (sprite sliding along track)

### Phase 2: Stations
- [ ] CELL_STATION cell type (mark track cells as stations)
- [ ] Cart stops at stations (DOORS_OPEN state, timer)
- [ ] Station rendering

### Phase 3: Mover Boarding (requires queuing)
- [ ] Mover states: WAITING / BOARDING / RIDING / EXITING
- [ ] Movers can walk to station and wait
- [ ] Movers board when cart doors open (capacity check)
- [ ] Riding movers locked to cart position
- [ ] Movers exit at target station
- [ ] Manual usage: player orders mover to take train

### Phase 4: Pathfinder Decision
- [ ] Station graph with ride costs
- [ ] At pathfind time: compare walk vs train
- [ ] Mover automatically chooses train when beneficial
- [ ] Cost estimation includes wait time based on cart position

### Phase 5: Polish
- [ ] Multiple train lines
- [ ] Loop tracks
- [ ] Multiple carts per line
- [ ] Cart capacity UI
- [ ] Station queue visualization

---

## Edge Cases

- **Cart full**: Mover stays in queue, waits for next pass
- **Mover's job changes while waiting**: Leave queue, repath
- **Mover's job changes while riding**: Exit at nearest useful station, or ride to end
- **Track destroyed while cart moving**: Cart stops, riding movers dismount
- **Station removed**: Movers in queue repath, cart skips that stop
- **No path to any station**: Fall back to direct walking
- **Train slower than walking**: Pathfinder should never pick it (cost comparison handles this)

---

## Relationship to Other Transport

```
Early game:    Haulers walk everywhere
Mid game:      Train tracks for long horizontal distances
               Elevators for vertical transport
Late game:     Multiple lines, express routes, freight trains (items only)
```

This matches the logistics progression from vision/logistics-influences.md: haulers -> conveyors/trains -> elevators.

---

## Open Questions

1. **Item transport?** Freight mode where cart carries items instead of/in addition to movers. Would integrate with hauling jobs.
2. **Track material?** Wood tracks early, iron tracks later? Or abstract it.
3. **Power source?** Manual push cart, animal-drawn, steam? Cosmetic or gameplay-relevant?
4. **Track switches / branching?** V1 says no. But eventually junctions would enable complex networks.
5. **Multiple carts per track?** Collision avoidance? Passing sidings?
6. **Sound?** Clickety-clack when cart moves, bell at station.
