# Transport Layer — Unified Abstraction for Vehicles & Multi-Leg Pathfinding

> Status: spec

Shared foundation for all systems where movers (or items) use infrastructure instead of walking directly: elevators, trains, ferries, drawbridges, stairs, doors, hoists, pack animals.

---

## The Pattern

Every transport system follows the same shape:

1. Mover walks to an **entry point**
2. Mover **waits** (idles nearby, avoidance spreads them out)
3. Mover **boards** (capacity check, FIFO or proximity)
4. Mover is **transported** (position locked to vehicle / instant transition)
5. Mover **exits** at destination
6. Mover resumes walking

The pathfinder's job: decide whether `walk(A→B)` is cheaper than `walk(A→entry) + wait + ride(entry→exit) + walk(exit→B)`.

---

## Waiting Model — No Explicit Queues (for transport)

Research into real-world queuing (pedestrian dynamics, train platforms, elevator lobbies) and how other games handle it (DF, RimWorld, Theme Hospital, SimAirport, Planet Zoo) leads to a key simplification:

**Transport doesn't need spatial queues.** What looks like a "queue" at a train platform or elevator lobby is actually just a **waiting set** — a list of movers waiting for a resource, with timestamps. The spatial layout (where movers stand while waiting) emerges from existing steering/avoidance behavior, not from assigned queue positions.

> **Note: real queues do exist — just not here.** Ordered single-file lines form in real life (and would look great in-game) when service is 1-at-a-time, face-to-face, and takes non-trivial time: food stalls, merchant counters, tavern bars, barbers, doctors. The pattern is "wait your turn → get served → step aside for the next person." This is a different system from transport — it's about **service buildings**, not movement infrastructure. Transport is either batch (elevator, train) or near-instant (door, ladder), so spatial lines don't form naturally. A dedicated service-queue design doc may be needed later when service buildings are implemented, but it's out of scope for the transport layer.

### Why this works

- **Elevator lobbies**: People mill around, not in a line. When doors open, they board roughly FIFO. The "queue" is just who's been waiting longest.
- **Train platforms**: People spread along the platform edge near where doors will stop. This is just "idle near goal + avoidance" — no queue struct needed.
- **Doors**: In DF and RimWorld, movers just crowd at doors. Neither game has explicit queues. Players learn to build wider corridors. The bottleneck is the design pressure.
- **Workshops**: The queue is purely logical (job reservation). Only the assigned mover walks to the work tile. Others do different jobs. This is what we already have.
- **Wells/shared resources**: People form a loose arc. Next user = longest waiting. Again, just a waiting set.

### The actual primitive

```c
typedef struct {
    int waitingMovers[MAX_WAITING];  // mover indices
    float waitingSince[MAX_WAITING]; // arrival timestamps (game time)
    int waitingCount;
    int capacity;                    // how many can use/board at once
    TransportTrigger trigger;        // ALWAYS, PERIODIC, ON_DEMAND
} WaitingSet;
```

Operations:
- `AddWaiter(set, moverIdx)` — mover starts waiting, timestamp recorded
- `RemoveWaiter(set, moverIdx)` — mover cancelled/repathed/died
- `GetNextBoarders(set, count)` — returns oldest N waiters (FIFO)
- `EstimateWait(set)` — waitingCount × averageServiceTime (for pathfinder cost)

Spatial behavior is handled entirely by steering: waiting movers idle near the entry point, avoidance keeps them from stacking. No `GetQueuePosition()` needed.

### Per-context spatial behavior

The waiting set is context-free. What differs per transport type is where movers choose to idle:

| Context | Idle behavior | How it emerges |
|---------|--------------|----------------|
| **Door** | Stand 1 tile back, wait for it to clear | Mover avoidance — can't walk into occupied cell |
| **Ladder** | Wait at top or bottom | 1-per-segment limit, avoidance handles the rest |
| **Elevator** | Mill around lobby area | Idle near entry + avoidance spreads them out |
| **Train platform** | Spread along platform edge near door positions | Each mover picks nearest door cell as idle target, avoidance spreads them |
| **Well/resource** | Loose arc around access point | Idle near resource + avoidance from multiple approach directions |
| **Workshop** | No spatial waiting at all | Job system handles reservation, mover does other work |

---

## Passive Bottlenecks — Doors, Ladders, Stairs

These don't need the full transport state machine. They're just cells with limited throughput.

### Ladders

**1 mover per ladder segment.** If a ladder cell is occupied, other movers wait at the top or bottom until it clears. This mostly works already via mover avoidance (movers don't walk into occupied cells).

What's needed:
- Pathfinder cost increase when a ladder is occupied (so movers prefer alternate routes if available)
- Possibly: brief "claim" on the ladder so a mover ascending doesn't collide with one descending (one-at-a-time)
- The player's design pressure: if a ladder is a bottleneck, build a second one or use stairs/ramps

Bidirectional traffic on a single-tile ladder is inherently low-throughput. Research shows alternating direction batches don't help much — it's better to just have separate up/down paths (which is a player layout decision, not a system one).

### Doors

Doors already have a throughput limit (one mover at a time while the door is opening/closing). The main issue is movers crowding on both sides.

What's needed:
- Pathfinder cost based on door congestion (movers waiting nearby)
- Avoidance already prevents stacking, so movers will naturally spread behind the door
- Player design pressure: 2-wide doorways, double doors, alternate routes

### Stairs (future)

No stair cell type yet. Stairs would be a 2-3 capacity passive bottleneck — faster than ladders, wider throughput. Essentially a "wide ladder" from the transport perspective.

---

## CapacityNode — The Shared Abstraction (extract later)

**Decision: concrete first.** Build the first vehicle (train stations or elevator) with hardcoded structs. Extract this abstraction only after a second vehicle proves the pattern generalizes.

The eventual shape, for reference:

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
    TransportTrigger trigger;   // ALWAYS, PERIODIC, ON_DEMAND

    // Directionality
    Directionality direction;   // BIDIRECTIONAL, ONE_WAY_FORWARD, ONE_WAY_DOWN

    // What it moves
    TransportPayload payload;   // MOVERS, ITEMS, BOTH

    // Waiting
    WaitingSet waiters;
} CapacityNode;
```

### Concrete Instances

| System | Type | Entries | Capacity | Throughput | Trigger | Direction | Payload |
|--------|------|---------|----------|------------|---------|-----------|---------|
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
    TRANSPORT_WAITING,             // idling near entry, in waiting set
    TRANSPORT_BOARDING,            // entering vehicle (short transition)
    TRANSPORT_RIDING,              // position locked to vehicle
    TRANSPORT_EXITING,             // leaving vehicle (short transition)
} TransportState;
```

Stored on the mover:

```c
// added to Mover struct
TransportState transportState;
int transportNodeId;        // which node we're using
int transportEntryIdx;      // which entry we're waiting at
int transportExitIdx;       // which entry we're exiting at
```

Passive bottlenecks (doors, ladders, stairs) don't need this state machine — throughput limiting + avoidance handles them.

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
               + EstimateWait(node.waiters)
               + rideCost(entryA → entryB)
               + walkCost(entryB → goal)
       if legCost < bestCost: bestCost = legCost, bestRoute = this

3. if bestCost < directCost: use transport route
   else: walk directly
```

The comparison itself is simple. The hard part is **getting reliable inputs**, and this depends heavily on whether vehicle routes are deterministic:

#### The deterministic route prerequisite

With **deterministic routes** (train follows a fixed path, visits stations in a known order):
- `rideCost(station1→station2)` is precomputable (known track distance / speed)
- `waitEstimate` can be computed from train position on its fixed loop (known ETA at each station)
- `walkCost` calls are just standard A*
- The whole comparison becomes straightforward — just arithmetic on known values

With **random routes** (current train behavior: 90% straight, 10% random at junctions):
- `rideCost` is unpredictable — you don't know which path the train will take between stations
- `waitEstimate` is fuzzy — the train could wander off at any junction
- Re-evaluation is needed when the train takes an unexpected turn (does the mover give up and walk?)
- Station visit order isn't even guaranteed

**Implication: deterministic train routes are essentially a prerequisite for pathfinder integration.** The current random-bounce behavior is fine for a decorative train but can't support route planning. Before Layer 3, trains need fixed routes (or at minimum, deterministic junction behavior when stations are involved).

#### Other costs of route comparison

- **Multiple pathfind calls**: each station pair near start × each station pair near goal. With 5 stations that's up to 25 combinations, though distance pruning cuts this down heavily.
- **Staleness**: a mover picks "take the train," but conditions change (train breaks down, gets delayed). Need a re-evaluation policy — timeout? periodic recheck? abandon on job change?

### Simple heuristic first (defer full cost comparison)

In practice, people don't compute optimal routes — they just think "that's far and there's a station nearby, I'll take the train." Even if walking would have been marginally faster, avoiding the long walk has value. Nobody does the math.

This means a simple distance-threshold heuristic is good enough for initial train usage:

```
if (walkDistance(start, goal) > FAR_THRESHOLD
    && nearestStation(start) exists within STATION_RADIUS
    && nearestStation(goal) exists within STATION_RADIUS):
    take the train
else:
    walk directly
```

This works even with random-bounce trains — the mover doesn't need to predict the route. They walk to the nearest station, wait, ride until the train reaches the station closest to their goal, and get off. If the train takes a weird detour at a junction, whatever — the mover is sitting on the train, not walking.

The full cost comparison (Layer 3) becomes an **optimization for later**, not a prerequisite for usable trains. Layer 2 can ship with the simple heuristic and still feel right.

### Cost Estimation (Layer 3 — deferred)

```c
float EstimateWait(WaitingSet* set) {
    switch (set->trigger) {
        case ALWAYS:
            // Passive throughput: how many ahead × service time
            return set->waitingCount * averageServiceTime;

        case PERIODIC: {
            // Vehicle on a fixed route: time until next arrival (precomputed from position on loop)
            float timeToArrival = EstimateArrivalTime(node, entryIdx);
            return timeToArrival + (set->waitingCount / capacity) * stopTime;
        }

        case ON_DEMAND: {
            // Vehicle must be summoned: travel time + boarding
            float travelToUs = DistanceToEntry(node, entryIdx) / speed;
            return travelToUs + (set->waitingCount / capacity) * stopTime;
        }
    }
}
```

### Congestion Cost for Passive Bottlenecks

Doors, ladders, and stairs don't use the full transport graph. Instead, their congestion feeds back as **increased pathfinding edge costs**:

```
ladderCost = baseLadderCost + (moversNearby * congestionPenalty)
doorCost   = baseDoorCost   + (moversNearby * congestionPenalty)
```

This makes movers prefer alternate routes when a bottleneck is busy — "that doorway is crowded, go around."

### Pruning

Don't check all CapacityNodes for every pathfind call:
- Only consider entries within `walkRadius` of start/goal (e.g. 30 cells)
- Skip nodes whose minimum ride distance is shorter than just walking
- Skip item-only nodes when pathfinding for movers (and vice versa)

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
| Train entity (bounce + stations) | done | trains.c, trains.h |
| CELL_PLATFORM + station auto-detect | done | trains.c (RebuildStations) |
| Multi-cell platforms | done | trains.c (platformCells, queueDir) |
| Cart state machine (moving/doors-open) | done | trains.c (TrainsTick) |
| Train rendering + save/load | done | rendering.c, saveload.c (v86+, v88 multi-car) |
| Waiting set (inline on station) | done | trains.c (StationAddWaiter etc.) |
| Multi-leg journey (walk→wait→ride→walk) | done | mover.c (transportState machine) |
| Mover boarding/exiting/riding | done | trains.c (Board/Exit/DismountAll) |
| Transport heuristic (ShouldUseTrain) | done | trains.c + mover.c |
| Multi-car trains (trailing cars) | done | trains.c (carCount, trail history) |
| Train-mover collision push | done | trains.c (TrainsTick) |
| Haul + cancel job integration | done | mover.c, jobs.c |
| Ladders (single-file vertical) | done | cell_defs.h, pathfinding |
| Ramps (directional vertical) | done | cell_defs.h, pathfinding |
| Doors (single cell, no queue) | done | cell_defs.h |
| Mover avoidance (basic) | done | mover.c |
| HPA* portal graph | done | hpa.c |
| Transport overlay graph | **not started** | — |
| Full pathfinder cost comparison | **not started** | — (heuristic works for now) |
| Elevators | **not started** | — |
| Ladder congestion cost | **not started** | — |
| Door congestion cost | **not started** | — |
| Ferry / drawbridge / hoist | **not started** | — |

---

## Implementation Order

### Layer 0: Passive Bottleneck Improvements
Ladders: 1 mover per segment, others wait. Doors: congestion-aware pathfinding cost. No new data structures needed — just pathfinder cost adjustments and letting existing avoidance do the spatial work. Immediate value, no transport dependency.

### Layer 1: WaitingSet + Multi-Leg Journey — DONE
The `WaitingSet` primitive (mover list + timestamps + FIFO boarding). Mover can execute a `Journey` with multiple legs. State machine handles transitions between WALK and WAIT_AND_RIDE legs. Built inline on TrainStation struct. Mover transport state machine in mover.c handles walk→wait→ride→walk transitions.

### Layer 2: First Vehicle (train stations) — DONE
Train stations (CELL_PLATFORM adjacent to CELL_TRACK), waiting sets at platforms, boarding/exiting, mover riding state — all built end-to-end with hardcoded structs. Multi-cell platforms, pull-forward logic, multi-car trains, train-mover collision push, haul/cancel integration. Movers decide to use the train via a **simple distance-threshold heuristic** (`ShouldUseTrain()` in trains.c). 58+ tests in test_trains.c.

### Layer 3: Optimal Route Comparison in Pathfinder (deferred, optional)
Full cost comparison: `walkCost` vs `walkCost + waitEstimate + rideCost + walkCost`. Requires deterministic train routes to be useful (random bounce makes ride cost unpredictable). Transport nodes become portal edges in the pathfinding graph. This is an optimization — the simple heuristic from Layer 2 already gives good-enough behavior. Only build this if movers are making noticeably bad decisions about when to take the train.

### Layer 4: Second Vehicle (elevator)
Build elevator end-to-end. Validates that the pattern generalizes. Extract CapacityNode abstraction if it makes sense.

**What carries over from trains (reuse directly):**
- WaitingSet — same struct, same FIFO boarding
- Transport state machine — WALKING_TO_ENTRY → WAITING → BOARDING → RIDING → EXITING, identical
- Multi-leg Journey struct — walk → ride → walk, same shape
- Mover riding state — position locked to vehicle
- Distance heuristic — "it's far vertically and there's an elevator nearby"
- Save/load pattern — same shape (entity + waiting set + mover transport fields)

**What's actually new:**
- **Vertical movement** — train moves on 2D track at one z-level, elevator moves through z-levels. Entity tracks current z, moves between floors. This is the big difference.
- **On-demand vs periodic** — train bounces constantly, elevator sits idle until called (or runs a simple floor loop). Needs call/dispatch logic.
- **Shaft construction** — vertical column spanning multiple z-levels, new placement UI. Train tracks are 2D.
- **No track pathfinding** — elevator just goes up and down in a shaft. Much simpler movement than train junction navigation.
- **Entry points at different z-levels** — train doors are on the side at one z. Elevator doors are at each floor — same x,y but different z. This is conceptually different but the WaitingSet doesn't care.

**What's surprisingly the same:** the mover's experience is almost identical. Walk to entry, wait, get on, ride, get off, walk away. The transport state machine and WaitingSet are reused wholesale. The elevator is really just: new vehicle entity (vertical shaft) + new placement UI + call/dispatch. All mover-side infrastructure transfers directly. This is exactly what "concrete first, extract later" is betting on — after building both, the shared parts become obvious.

### Layer 5: Congestion Feedback
Passive bottleneck congestion (doors, ladders) feeds back into pathfinder costs. Movers route around busy bottlenecks. "That doorway is crowded, go around."

### Layer 6: Item Transport
Hoists, chutes, canals, pack animals. Two-haul-job pattern with transport step in between. Job system routes items via transport nodes.

### Layer 7: Exotic Transport
Ferries (water crossing), drawbridges (periodic availability), ziplines (one-way fast). Each is a new node type but reuses all existing infrastructure.

### Layer 8: Personal Vehicles & Timetables (late game)
See "Future Concepts" section below.

---

## Design Decisions

1. **Generic first or concrete first?** ~~Build CapacityNode abstraction upfront, or build elevator concretely and extract the pattern after?~~ **Decision: concrete first.** Build train stations end-to-end with hardcoded structs. Extract the CapacityNode abstraction only after the second vehicle (elevator) proves the pattern generalizes.

2. **Explicit queues or emergent waiting?** ~~Build TransportQueue with assigned positions, or let steering handle spatial layout?~~ **Decision: emergent.** A `WaitingSet` tracks who's waiting and when they arrived. Movers idle near entry points using existing steering/avoidance. No assigned queue positions, no `GetQueuePosition()`. Research shows real-world queues at elevators, train platforms, and doorways are unstructured — people mill around and board in rough FIFO order.

3. **Pathfinder coupling.** Does the transport graph live inside the pathfinder (new edge type in A*/HPA*) or outside it (post-processing step that compares walk vs ride)? Outside is simpler. Inside is more accurate. Decide at Layer 3.

4. **When does a mover give up waiting?** Timeout? Re-evaluate cost periodically? What if their job changes?

5. **Congestion feedback.** Should high waiter counts propagate back as pathfinder costs? This creates adaptive routing (movers avoid busy elevators) but might oscillate.

6. **Item routing.** Does a hauler know to use a hoist, or does the job system discover item-transport as a plan? Former is simpler, latter is more emergent.

---

## Relationship to Other Docs

- `world/elevators.md` — Concrete elevator design (instance of this pattern)
- `world/trains.md` — Concrete train design (instance of this pattern)
- `pathfinding/social-navigation.md` — Social navigation (yielding, lanes, personal space)
- `endgame-village-vision.md` — Vertical circulation, commute distance, infrastructure
- `schedule-system.md` — Commute time matters for mood (transport = quality of life)
- `architecture/capability-based-tasks.md` — Provider registry pattern could serve transport node lookup
- `vision/logistics-influences.md` — Factorio/SimTower progression framing

---

## Open Questions

1. **Stairs.** Currently no stair cell type. Stairs would be a wide (2-3 capacity), fast ladder. Design needed before elevators make sense (elevators compete with stairs, not ladders).

2. **Multiple vehicles per node.** Two elevator cars in one shaft? Two carts on one track? Collision avoidance, or just disallow?

3. **Freight vs passenger.** Same vehicle carrying both, or separate modes? A freight elevator is the same struct with `payload = ITEMS`.

4. **Power source.** Manual, animal, water, steam? Affects availability and era gating. Cosmetic or gameplay-relevant?

5. **Construction.** How does a player build an elevator shaft? Multi-cell vertical designation? Blueprint that spans z-levels?

---

## Research Notes — Real-World & Game Queuing Behavior

Captured during design research. Informed the "no explicit queues" decision.

### Real-world queue formations

- **Single-file line**: Only forms when physically constrained (narrow corridor). Otherwise people naturally spread out.
- **Fan-out / semicircle**: Default formation in open space around a single service point. People minimize distance to service while maintaining personal space.
- **Lobby / distributed waiting**: Elevators, airports. No line at all — people spread out and respond when called. The queue is logical (FIFO by arrival), not spatial.
- **Platform spread**: Train platforms — people distribute along the platform edge near predicted door positions. Two short columns per door in the most structured version (Japanese stations).
- **Funnel**: Wide area narrowing to a bottleneck. Self-organizing, optimal angle 46-65 degrees.

### How other games handle it

- **Dwarf Fortress / RimWorld / Prison Architect**: No explicit queues. Movers crowd at bottlenecks. Player learns to build wider corridors and alternate routes. Traffic designation system (DF) lets players adjust pathfinding costs per tile.
- **Theme Hospital / Two Point Hospital**: Logical queue decoupled from spatial position — patients keep their place while wandering to eat/drink. Benches near rooms serve as informal waiting areas.
- **SimAirport / Planet Zoo / Parkitect**: Explicit player-placed queue paths (serpentine). Guests line up along the path. This is a theme-park-specific pattern — doesn't fit a colony sim.

### Academic findings

- **Social Force Model** (Helbing 1995): Queue-like formations emerge naturally from goal attraction + collision avoidance. No explicit queue data structure needed.
- **Bottleneck research**: Arch shapes form in front of narrow exits. A column placed slightly before a bottleneck can actually improve flow (breaks up the arch, prevents clogging).
- **Bidirectional narrow passage**: Extremely low throughput. Separate up/down paths are much better than alternating direction.

### Key takeaway

For a colony sim: don't build queue infrastructure. Build good steering/avoidance and let waiting behavior emerge. The player's job is to design layouts that minimize bottlenecks. Explicit queues are only needed for batch-processing transport (elevator arrives, N movers board) — and even there it's a waiting set, not a spatial line.

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
