# Transport Layer — Unified Abstraction for Vehicles, Queues & Multi-Leg Pathfinding

Shared foundation for all systems where movers (or items) use infrastructure instead of walking directly: elevators, trains, ferries, drawbridges, stairs, doors, hoists, pack animals.

---

## The Pattern

Every transport system follows the same shape:

1. Mover walks to an **entry point**
2. Mover **waits** (queue, schedule, availability)
3. Mover **boards** (capacity check)
4. Mover is **transported** (position locked to vehicle / instant transition)
5. Mover **exits** at destination
6. Mover resumes walking

The pathfinder's job: decide whether `walk(A→B)` is cheaper than `walk(A→entry) + wait + ride(entry→exit) + walk(exit→B)`.

---

## CapacityNode — The Shared Abstraction

Every transport instance is a **CapacityNode**: a point (or pair of points) in the world with limited throughput.

```c
typedef struct {
    int nodeId;
    TransportType type;

    // Where movers enter/exit
    int entryCount;
    TransportEntry entries[MAX_TRANSPORT_ENTRIES];  // position + z-level per stop

    // Throughput
    int capacity;           // how many movers at once (1 for ladder, 8 for elevator)
    float throughputTime;   // seconds per use (0 for instant, 3.0 for elevator ride)

    // Availability
    Availability availability;  // ALWAYS, PERIODIC, ON_DEMAND
    float waitEstimate;         // current estimated wait (dynamic, updated each tick)

    // Directionality
    Directionality direction;   // BIDIRECTIONAL, ONE_WAY_FORWARD, ONE_WAY_DOWN

    // What it moves
    TransportPayload payload;   // MOVERS, ITEMS, BOTH

    // Queue
    int waitingMovers[MAX_QUEUE];
    int waitingCount;
} CapacityNode;
```

### Concrete Instances

| System | Type | Entries | Capacity | Throughput | Availability | Direction | Payload |
|--------|------|---------|----------|------------|--------------|-----------|---------|
| **Elevator** | vehicle | N floors (same x,y) | 4-8 | ~1 floor/sec + door time | on-demand (call) | bidirectional | both |
| **Train** | vehicle | N stations (track path) | 8-12 | ~3 cells/sec + stop time | periodic (bounce/loop) | bidirectional | both |
| **Ferry** | vehicle | 2 docks (across water) | 4-6 | ~2 cells/sec + dock time | periodic (shuttle) | bidirectional | both |
| **Drawbridge** | gate | 1 cell (spans gap) | 2-3 | instant crossing | periodic (open/close cycle) | bidirectional | movers |
| **Stairs** | passive | 2 (top + bottom) | 2-3 | ~0.5 sec per floor | always | bidirectional | movers |
| **Ladder** | passive | 2 (top + bottom) | 1 | ~1.0 sec per floor | always | bidirectional | movers |
| **Door** | passive | 1 cell | 1 | ~0.3 sec | always (unless locked) | bidirectional | movers |
| **Hoist** | vehicle | 2 (top + bottom) | 1 item stack | ~2 sec/floor | on-demand | bidirectional | items only |
| **Chute** | passive | 2 (top + bottom) | unlimited | instant | always | **one-way down** | items only |
| **Canal** | passive | N (along water) | unlimited | current speed | always | **one-way downstream** | items only |
| **Pack animal** | vehicle | 2+ (waypoints) | 4-6 stacks | animal walk speed | on-demand | bidirectional | items only |

---

## Mover State Machine (shared)

All vehicle-type transport uses the same 5 states:

```c
typedef enum {
    TRANSPORT_NONE,
    TRANSPORT_WALKING_TO_ENTRY,    // normal pathfinding to entry point
    TRANSPORT_WAITING,             // in queue at entry, vehicle not here yet
    TRANSPORT_BOARDING,            // entering vehicle (short transition)
    TRANSPORT_RIDING,              // position locked to vehicle
    TRANSPORT_EXITING,             // leaving vehicle (short transition)
} TransportState;
```

Stored on the mover:

```c
// added to Mover struct
TransportState transportState;
int transportNodeId;        // which CapacityNode we're using
int transportEntryIdx;      // which entry we're waiting at
int transportExitIdx;       // which entry we're exiting at
```

Passive nodes (doors, ladders, stairs) don't need the full state machine — they just need queue-aware throughput limiting.

---

## Pathfinder Integration

### Transport Graph

CapacityNodes form a **transport overlay graph** — a small set of portal edges layered on top of the walking graph (similar to how HPA* already uses chunk-boundary portals).

```c
typedef struct {
    int nodeCount;
    CapacityNode nodes[MAX_TRANSPORT_NODES];

    // Precomputed: which entries connect to which
    // entry[i] → entry[j] with cost = rideCost(i, j)
    float edgeCost[MAX_ENTRIES][MAX_ENTRIES];  // updated periodically
} TransportGraph;
```

### Route Decision (the key algorithm)

At pathfind time, for a given start→goal:

```
1. directCost = walkCost(start → goal)                    // standard A*

2. for each CapacityNode reachable from start:
     for each entry pair (entryA, entryB) where entryB is closer to goal:
       legCost = walkCost(start → entryA)
               + node.waitEstimate
               + rideCost(entryA → entryB)
               + walkCost(entryB → goal)
       if legCost < bestCost: bestCost = legCost, bestRoute = this

3. if bestCost < directCost: use transport route
   else: walk directly
```

### Cost Estimation

The dynamic part is `waitEstimate`. Different availability types compute this differently:

```c
float EstimateWait(CapacityNode* node) {
    switch (node->availability) {
        case ALWAYS:
            // Queue delay only: how many movers ahead of us × throughputTime
            return node->waitingCount * node->throughputTime;

        case PERIODIC: {
            // Vehicle on a schedule: time until next arrival at this entry
            float timeToArrival = EstimateArrivalTime(node, entryIdx);
            float queueDelay = node->waitingCount * node->throughputTime;
            return timeToArrival + queueDelay;
        }

        case ON_DEMAND: {
            // Vehicle must be summoned: travel time to us + queue
            float travelToUs = DistanceToEntry(node, entryIdx) / node->speed;
            float queueDelay = node->waitingCount * node->throughputTime;
            return travelToUs + queueDelay;
        }
    }
}
```

### Pruning

Don't check all CapacityNodes for every pathfind call. Use spatial hashing or distance thresholds:
- Only consider entries within `walkRadius` of start/goal (e.g. 30 cells)
- Skip nodes whose minimum ride distance is shorter than just walking
- Skip item-only nodes when pathfinding for movers (and vice versa)

---

## Queuing System (prerequisite — from social-navigation.md)

All of this requires a generic queuing primitive:

```c
typedef struct {
    int nodeId;
    int entryIdx;
    int movers[MAX_QUEUE_SIZE];
    int count;
    int capacity;           // physical space limit
} TransportQueue;

// Queue positions: movers line up near the entry, not stacked on one cell
Vector2 GetQueuePosition(TransportQueue* q, int positionInQueue);

// Queue operations
void   EnqueueMover(TransportQueue* q, int moverIdx);
void   DequeueMover(TransportQueue* q);              // FIFO
void   RemoveMover(TransportQueue* q, int moverIdx); // mover cancelled/repathed
int    QueuePosition(TransportQueue* q, int moverIdx);
```

Queuing also applies to non-transport bottlenecks (wells, workshops) — same data structure, different context.

---

## Multi-Leg Journeys

Current movers walk a single contiguous path. Transport requires splitting a journey into legs:

```c
typedef struct {
    int legCount;
    JourneyLeg legs[MAX_LEGS];  // typically 1-3
} Journey;

typedef struct {
    JourneyLegType type;   // WALK, WAIT_AND_RIDE
    // WALK leg:
    int goalX, goalY, goalZ;
    // WAIT_AND_RIDE leg:
    int nodeId;
    int entryIdx;
    int exitIdx;
} JourneyLeg;
```

A simple walk is a 1-leg journey. A train ride is 3 legs (walk → ride → walk). Chaining transport (elevator then train) is 5 legs.

---

## Item Transport

Some nodes carry items, not movers. The item equivalent of "mover rides" is:

1. Hauler carries item to entry (existing haul job)
2. Item placed on transport (hoist, chute, pack animal, canal)
3. Item travels to exit
4. Item appears at exit cell (ground item or stockpile)
5. Another hauler picks it up (existing haul job)

This decomposes into two standard haul jobs with a transport step in between. The job system doesn't need to understand transport — it just sees "item appeared at (x, y, z)".

The interesting decision: does the **job system** route items via transport, or does the **pathfinder** expose item-transport edges so `FindNearestItem()` can account for them? Probably the job system, since items don't pathfind themselves.

---

## What Already Exists

| Component | Status | Location |
|-----------|--------|----------|
| CELL_TRACK + autotile | done | cell_defs.c |
| Train entity (bounce, no stations) | done | trains.c, trains.h |
| Train rendering + save/load | done | rendering.c, saveload.c |
| Ladders (single-file vertical) | done | cell_defs.h, pathfinding |
| Ramps (directional vertical) | done | cell_defs.h, pathfinding |
| Doors (single cell, no queue) | done | cell_defs.h |
| Mover avoidance (basic) | done | mover.c |
| HPA* portal graph | done | hpa.c |
| Queuing system | **not started** | — |
| Multi-leg journey | **not started** | — |
| Transport overlay graph | **not started** | — |
| Stations / mover boarding | **not started** | — |
| Elevators | **not started** | — |
| Ferry / drawbridge / hoist | **not started** | — |

---

## Implementation Order

### Layer 0: Queuing Primitive
**Prerequisite for everything else.** Generic queue at a cell. Movers wait in line instead of crowding. Apply to existing bottlenecks first (ladders, doors) for immediate value.

See: `pathfinding/social-navigation.md` Phase 1.

### Layer 1: Multi-Leg Journey
Mover can execute a `Journey` with multiple legs. State machine handles transitions between WALK and WAIT_AND_RIDE legs. No pathfinder changes yet — journeys are manually constructed (e.g., player orders mover to take elevator).

### Layer 2: First Vehicle (elevator or train Phase 2-3)
Pick one and build it end-to-end: entity + stations + boarding + riding. This validates the CapacityNode abstraction and the mover state machine. Elevator is simpler (fewer entries, no track path), train is already half-built.

### Layer 3: Transport Graph in Pathfinder
CapacityNodes become portal edges in the pathfinding graph. Route decision algorithm. Dynamic cost estimation. Movers automatically choose transport when beneficial.

### Layer 4: Second Vehicle
Build the other one (train or elevator). Should be fast — reuses all of Layer 0-1-3. Validates that the abstraction actually generalizes.

### Layer 5: Passive Node Upgrades
Retrofit doors, ladders, stairs with queue-aware throughput. Pathfinder accounts for congestion at bottlenecks. "That doorway is busy, go around."

### Layer 6: Item Transport
Hoists, chutes, canals, pack animals. Two-haul-job pattern with transport step in between. Job system routes items via transport nodes.

### Layer 7: Exotic Transport
Ferries (water crossing), drawbridges (periodic availability), ziplines (one-way fast). Each is a new CapacityNode type but reuses all existing infrastructure.

### Layer 8: Personal Vehicles & Timetables (late game)
See "Future Concepts" section below — personal carts/cars with parking and collision, train timetables with service hours.

---

## Design Decisions to Make

1. **Generic first or concrete first?** ~~Build CapacityNode abstraction upfront, or build elevator concretely and extract the pattern after?~~ **Decision: concrete first.** Build elevator (or train Phase 2+) end-to-end with hardcoded structs. Extract the CapacityNode abstraction only after the second vehicle proves the pattern generalizes. Avoids premature abstraction.

2. **Pathfinder coupling.** Does the transport graph live inside the pathfinder (new edge type in A*/HPA*) or outside it (post-processing step that compares walk vs ride)? Outside is simpler. Inside is more accurate.

3. **When does a mover give up waiting?** Timeout? Re-evaluate cost periodically? What if their job changes?

4. **Congestion feedback.** Should high queue counts propagate back as pathfinder costs? This creates adaptive routing (movers avoid busy elevators) but might oscillate.

5. **Item routing.** Does a hauler know to use a hoist, or does the job system discover item-transport as a plan? Former is simpler, latter is more emergent.

---

## Relationship to Other Docs

- `world/elevators.md` — Concrete elevator design (instance of this pattern)
- `world/trains.md` — Concrete train design (instance of this pattern)
- `pathfinding/social-navigation.md` — Queuing prerequisite (Layer 0)
- `endgame-village-vision.md` — Vertical circulation, commute distance, infrastructure
- `schedule-system.md` — Commute time matters for mood (transport = quality of life)
- `architecture/capability-based-tasks.md` — Provider registry pattern could serve transport node lookup
- `vision/logistics-influences.md` — Factorio/SimTower progression framing

---

## Open Questions

1. **Stairs.** Currently no stair cell type. Stairs would be a wide (2-3 capacity), fast ladder — a passive CapacityNode. Design needed before elevators make sense (elevators compete with stairs, not ladders).

2. **Multiple vehicles per node.** Two elevator cars in one shaft? Two carts on one track? Collision avoidance, or just disallow?

3. **Freight vs passenger.** Same vehicle carrying both, or separate modes? A freight elevator is the same struct with `payload = ITEMS`.

4. **Power source.** Manual, animal, water, steam? Affects availability and era gating. Cosmetic or gameplay-relevant?

5. **Construction.** How does a player build an elevator shaft? Multi-cell vertical designation? Blueprint that spans z-levels?

---

## Future Concepts (Layer 8 — late game)

These are interesting extensions that fit the pattern but add significant complexity. Captured here so we don't design ourselves into a corner, but not part of the near-term plan.

### Personal Vehicles (carts, cars, horses)

A mover owns or claims a vehicle, drives it themselves. Unlike trains/elevators (shared, fixed-route), personal vehicles are **free-roaming on the road network**.

**What's different from trains:**
- No fixed route — vehicle pathfinds like a mover but faster and wider
- **Parking.** Vehicle needs a place to stop. Mover walks from parking spot to destination. Home has a parking spot, workplace has a parking spot. Parking is a resource that can be scarce.
- **Collision / traffic.** Multiple vehicles on the same road. Can't overlap. Need lane discipline or at minimum no-overlap movement (like mover avoidance but for 1x1 or 2x1 vehicles).
- **Ownership.** Vehicle belongs to a mover (or is shared pool like a taxi). Mover walks to where their vehicle is parked, drives, parks at destination, walks the last bit.

**Journey shape:**
```
walk(home → home parking) → drive(home parking → work parking) → walk(work parking → workplace)
```

**New problems this introduces:**
- Road cells (CELL_ROAD?) — faster movement, vehicle-only?
- Parking cells/zones — limited supply, assignment
- Vehicle entity with position, speed, owner
- Traffic: vehicles blocking each other, overtaking, intersections
- Pathfinder now has 3 options: walk, public transport, personal vehicle

**Why it's late-game:** Traffic simulation is a whole domain. The village works fine with walking + trains/elevators for a long time. Personal vehicles only matter when the map is large and movers live far from work — which implies a mature settlement with housing districts, commutes, and a road network. Also fits era-wise: animal-drawn carts (Era 3), motorized (Era 4-5).

**Simpler version first:** Pack animals that follow a mover (already in the table as item transport). A mover with a donkey moves faster on roads. No parking needed — the donkey just follows. This tests the "personal speed boost" concept without traffic.

### Train Timetables (service hours)

Trains currently bounce endlessly. With timetables, trains run on a schedule:

**What changes:**
- Train has **operating hours** (e.g., 6am–10pm). Outside those hours, cart parks at a depot station.
- Movers need to **know the schedule** when deciding to use the train. A mover at 9:45pm might choose to walk because the last train is about to stop.
- `waitEstimate` now includes "is the train even running?" — if not, cost is infinite (or walk).

**Impact on pathfinder:**
- `EstimateWait()` checks game clock against timetable
- Outside service hours: transport edge cost = infinity → pathfinder picks walking
- Near end of service: cost spikes (risk of being stranded?) or pathfinder just sees high wait time

**Why it's interesting:** Creates daily rhythm. Morning rush at stations. Evening last-train pressure. Ties into schedule-system.md — movers plan their commute around transport availability. A mover who works late might live closer to work because the train doesn't run at night.

**Why it's late-game:** Requires the schedule system to exist first. Also requires movers to reason about future time ("will the train be running when I need to come home?"), which is a planning horizon the current job system doesn't have. Current movers are reactive (pick best option now), not predictive.

**Simpler version first:** Train just runs 24/7 (current bounce behavior + stations). Timetables are a tuning knob added later when the schedule system exists and daily rhythm matters.
