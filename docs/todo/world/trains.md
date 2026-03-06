# Train Tracks & Stations

> Status: partial

Horizontal multi-stop transport system. Conceptually: a horizontal elevator with a fixed track and multiple stations.

Part of the unified transport layer — see `pathfinding/transport-layer.md` for the shared abstraction (concrete-first approach: build trains end-to-end, extract pattern later).

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

## Current State (Phases 1–4 — done)

What's already in the codebase (`src/entities/trains.h`, `src/entities/trains.c`, `src/entities/mover.c`):

### Phase 1: Track & Cart Entity
- **Train struct**: pixel position (smooth interpolation), z-level, current/prev cell, speed, progress, locomotive light
- **CELL_TRACK**: walkable cell type with 16-sprite autotiling (N/E/S/W connectivity)
- **FindNextTrackCell()**: follows track, prefers straight (90%), random at junctions (10%), reverses at dead ends
- **TrainsTick()**: advances trains each frame, interpolates position, updates locomotive light
- **SpawnTrain()** / **SpawnTrainWithCars()**: places train on track cell, validates bounds
- **Track drawing UI**: ACTION_DRAW_TRACK (place/remove), ACTION_DRAW_TRAIN (spawn/remove)
- **Save/load**: v46+ (v47 light fields, v86 station/transport fields, v88 multi-car)
- **Rendering**: SPRITE_train_loc with rotation based on heading, depth tint, viewport culling
- **Limits**: MAX_TRAINS 32, TRAIN_DEFAULT_SPEED 9.0 cells/sec

### Phase 2: Stations
- **CELL_PLATFORM**: walkable cell type adjacent to track, auto-detected as station
- **RebuildStations()**: scans grid for track+platform adjacency, builds station list, preserves waiters across rebuilds
- **Multi-cell platforms**: contiguous CELL_PLATFORM cells parallel to track (up to MAX_PLATFORM_CELLS=8), with directional queue layout
- **Cart state machine**: CART_MOVING → CART_DOORS_OPEN (TRAIN_DOOR_TIME=3s timer)
- **Station detection during movement**: cart checks `GetStationAt()` each cell, stops only if waiters or riders want this station
- **Pull-forward logic**: cart continues through multi-cell stations, only stops at the last track cell adjacent to the platform
- **Station ejection**: movers waiting at destroyed stations get transport state reset and repath
- **Worldgen stations**: terrain.c places example platforms in generated worlds

### Phase 3: Mover Boarding
- **WaitingSet** (inline on TrainStation): `StationAddWaiter/RemoveWaiter/GetNextBoarder` with FIFO ordering
- **Transport state machine on Mover**: `TRANSPORT_NONE → TRANSPORT_WALKING_TO_STATION → TRANSPORT_WAITING → TRANSPORT_RIDING`
- **Mover struct fields**: `transportState`, `transportStation`, `transportExitStation`, `transportTrainIdx`, `transportFinalGoal`
- **BoardMoverOnTrain()**: removes from waiting set, adds to riding list, clears path
- **ExitMoverFromTrain()**: places mover on spread-out platform cell, restores original goal, triggers repath
- **DismountAllRiders()**: emergency dismount when track destroyed
- **Riding movers locked to cart position** (updated every tick in TrainsTick)
- **Capacity limit**: MAX_CART_CAPACITY=8, excess waiters remain in queue
- **Train-mover collision**: moving trains push non-riding movers out of the way
- **Queue positions**: `StationGetQueuePosition()` for visual queue layout along platform
- **Multi-car trains**: `carCount` field, trailing cars follow via trail cell history (MAX_TRAIL_LENGTH=8)
- **Haul integration**: transport only triggers on carry/delivery legs (`STEP_MOVING_TO_PICKUP` excluded)
- **Cancel job cleanup**: CancelJob properly resets transport state (waiting or riding)

### Phase 4: Pathfinder Decision (heuristic)
- **ShouldUseTrain()**: distance-threshold heuristic (TRANSPORT_FAR_THRESHOLD=40, TRANSPORT_STATION_RADIUS=20)
- **Auto-triggers in mover.c**: idle/walking movers automatically evaluate train usage each tick
- **Same-z only**: cross-z goals skip train consideration
- **Entry/exit station selection**: FindNearestStation() for both mover position and goal
- Uses the simple heuristic from transport-layer.md, not full pathfinder cost comparison (deferred to Layer 3)

### What trains don't do yet
- No multiple independent train lines (all trains share all tracks)
- No loop tracks (bounce only)
- No timetables / service hours
- No freight/item transport
- No full pathfinder cost comparison (heuristic works well enough)

### Test Coverage
58+ tests in `tests/test_trains.c` covering: station detection, waiting set FIFO, train stopping/resuming, ShouldUseTrain heuristic, boarding/exiting, capacity limits, mover transport state machine, haul integration, cancel job cleanup, platform/track destruction, multi-cell platforms, queue positions, track connections.

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
CELL_TRACK      // Done — walkable, drawn as rails, cart travels along these
CELL_STATION    // Needed — walkable, drawn as platform, movers board/exit here
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

The physical entity that moves along the track. Evolves from the existing `Train` struct.

```c
// Current Train struct (trains.h) becomes this:
typedef struct {
    // --- existing fields ---
    float x, y;                          // Pixel position (smooth interpolation)
    int z;
    int cellX, cellY;
    int prevCellX, prevCellY;
    float speed;
    float progress;
    int lightCellX, lightCellY;
    bool active;

    // --- Phase 2 additions ---
    TrainCartState state;
    float stateTimer;
    float doorTime;                      // how long doors stay open at station

    // --- Phase 3 additions ---
    int capacity;
    int ridingMovers[MAX_CART_CAPACITY];
    int ridingCount;
} Train;

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

### Simple: Bounce (current behavior, keep for Phase 2)

Cart moves forward along track, stops at each station, reverses at the end. No call system needed.

```
Station A ----> Station B ----> Station C
Station A <---- Station B <---- Station C
```

### Better: Loop

Track forms a loop. Cart always moves in one direction. Movers can predict arrival times.

### Advanced: On-Demand

Cart idles until called (like elevator). Moves to nearest caller, services stations along the way (SCAN algorithm from elevator doc).

### Timetable (late game — requires schedule system)

Cart runs on a daily schedule with operating hours:

- **Service hours**: e.g. 6am–10pm. Outside those hours, cart parks at a depot station.
- **Pathfinder impact**: `EstimateWait()` checks game clock against timetable. Outside service hours, transport edge cost = infinity → pathfinder picks walking. Near end of service, cost spikes (long wait until morning).
- **Mover planning**: a mover at 9:45pm might walk because the last train is about to stop. A mover who works late might prefer housing near their workplace because the train doesn't run at night.
- **Gameplay effect**: creates daily rhythm — morning rush at stations, evening last-train pressure. Ties into `schedule-system.md` — commute planning around transport availability.

**Why it's late-game**: requires schedule system, and requires movers to reason about future time ("will the train run when I need to come home?"). Current movers are reactive (pick best option now), not predictive. Start with 24/7 bounce, add timetables when daily rhythm matters.

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

Rail sprites on ground. Could reuse floor-layer rendering. Two parallel lines or a simple rail texture in the 8x8 atlas. **Already implemented** — 16 autotile sprites for all cardinal connectivity combos.

### Cart

Small sprite that moves along the track. Could show riding movers on top (stacked sprites). **Already implemented** — SPRITE_train_loc with rotation, locomotive light.

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

### Phase 1: Track & Cart Entity — DONE
- [x] CELL_TRACK cell type with 16-sprite autotiling
- [x] Track drawing in input (ACTION_DRAW_TRACK, ACTION_DRAW_TRAIN)
- [x] Train struct with pixel position, speed, smooth interpolation
- [x] Cart moves along track (FindNextTrackCell: straight preference, junction randomness, dead-end reversal)
- [x] Cart rendering (SPRITE_train_loc with rotation + depth tint)
- [x] Locomotive light (AddLightSource/RemoveLightSource as cart moves)
- [x] Save/load (v46+, v47 struct upgrade for light fields)

### Phase 2: Stations — DONE
- [x] CELL_PLATFORM cell type (walkable, auto-detected as station when adjacent to track)
- [x] Cart state machine: CART_MOVING → CART_DOORS_OPEN (simplified from 4-state design)
- [x] Cart detects station cells and stops (TRAIN_DOOR_TIME=3s timer)
- [x] Multi-cell platform detection (contiguous platforms parallel to track, up to 8 cells)
- [x] Pull-forward logic (cart continues through multi-cell stations, stops at last track cell)
- [x] Station rendering (platform sprite) + worldgen placement
- [x] RebuildStations() with waiter preservation across rebuilds
- [x] Save/load for station data (v86+)

### Phase 3: Mover Boarding — DONE
- [x] WaitingSet at stations (inline: waitingMovers[], waitingSince[], FIFO)
- [x] Transport state machine on Mover (NONE → WALKING_TO_STATION → WAITING → RIDING)
- [x] Movers walk to nearest platform cell and wait in queue
- [x] Movers board when cart doors open (capacity check, FIFO via StationGetNextBoarder)
- [x] Riding movers locked to cart position (updated every tick)
- [x] Movers exit at target station, placed on spread-out platform cells, resume walking
- [x] DismountAllRiders for emergency dismount (track destroyed)
- [x] Train-mover collision push (non-riders shoved out of way)
- [x] Multi-car trains (SpawnTrainWithCars, trailing car rendering via trail history)
- [x] Haul integration (transport only on carry/delivery legs, not pickup walks)
- [x] Cancel job cleanup (transport state properly reset)
- [ ] Manual usage: player orders drafted mover to take train

### Phase 4: Pathfinder Decision — DONE (heuristic)
- [x] ShouldUseTrain() distance-threshold heuristic (far enough + stations near both ends)
- [x] Movers automatically choose train when beneficial (auto-triggers in mover.c)
- [x] Same-z-only constraint (cross-z goals skip train)
- [ ] Full pathfinder cost comparison with ride time estimation (deferred — heuristic works well)
- [ ] Dynamic cost edges based on cart position (Layer 3 optimization)

### Phase 5: Polish — PARTIAL
- [ ] Multiple train lines (all trains share all tracks currently)
- [ ] Loop tracks (bounce only)
- [x] Multiple carts per line (multi-car trains with trailing cars)
- [ ] Cart capacity UI
- [x] Station queue visualization (StationGetQueuePosition)
- [ ] Timetables / service hours (requires schedule system)
- [ ] Freight mode (item transport without movers)

---

## Edge Cases

- **Cart full**: Mover stays in queue, waits for next pass
- **Mover's job changes while waiting**: Leave queue, repath
- **Mover's job changes while riding**: Exit at nearest useful station, or ride to end
- **Track destroyed while cart moving**: Cart stops, riding movers dismount (current: cart deactivates)
- **Station removed**: Movers in queue repath, cart skips that stop
- **No path to any station**: Fall back to direct walking
- **Train slower than walking**: Pathfinder should never pick it (cost comparison handles this)
- **Train not running (timetable)**: Transport edge cost = infinity, pathfinder picks walking

---

## Relationship to Other Transport

```
Early game:    Haulers walk everywhere
Mid game:      Train tracks for long horizontal distances
               Elevators for vertical transport
Late game:     Multiple lines, express routes, freight trains (items only)
               Timetables, personal vehicles (see transport-layer.md)
```

This matches the logistics progression from vision/logistics-influences.md: haulers -> conveyors/trains -> elevators.

See also: `pathfinding/transport-layer.md` for the unified abstraction covering trains, elevators, ferries, personal vehicles, and other transport types.

---

## Open Questions

1. **Item transport?** Freight mode where cart carries items instead of/in addition to movers. Would integrate with hauling jobs.
2. **Track material?** Wood tracks early, iron tracks later? Or abstract it.
3. **Power source?** Manual push cart, animal-drawn, steam? Cosmetic or gameplay-relevant?
4. **Track switches / branching?** V1 says no. But eventually junctions would enable complex networks.
5. **Multiple carts per track?** Collision avoidance? Passing sidings?
6. **Sound?** Clickety-clack when cart moves, bell at station.
