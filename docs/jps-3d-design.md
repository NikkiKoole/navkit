# JPS and JPS+ 3D Design

## Overview

This document describes the design for extending JPS (Jump Point Search) and JPS+ to support 3D navigation via z-levels connected by ladders. The core insight is to treat the world as **layered 2D maps** connected by **ladder portals**, rather than a true 3D grid.

## Design Philosophy

### Ladders as Portals, Not Directions

In 2D JPS, you can move in 8 directions. It might seem natural to add "up" and "down" as directions 9 and 10 for 3D. But this breaks the JPS model:

- JPS relies on symmetry and predictable jump patterns
- Ladders are sparse and specific - you can't "go up" anywhere
- Vertical movement has different semantics than horizontal movement

Instead, we treat ladders as **portals** between independent 2D search spaces. A ladder is a point where you transition from one z-level's 2D grid to another.

### Ladders as Forced Jump Points

In JPS terminology, a **forced neighbor** creates a **jump point** - a location where the optimal path might change direction and therefore must be considered.

Ladders are natural jump points because:
- You MUST stop and decide: climb or keep walking?
- The optimal path might go through this ladder, or might not
- There's no way to "skip over" a ladder decision

When the JPS scan hits a ladder cell, it stops and returns that cell as a jump point, regardless of whether there are wall-based forced neighbors.

### The Heuristic Problem

With a goal on a different z-level, 2D heuristics break down:

```
Goal is at z=2, you're at z=0
Manhattan distance says: 5 tiles
Actual path: 200 tiles through multiple ladders and corridors
```

The heuristic becomes so loose it provides almost no guidance. But we can solve this with precomputation.

## Architecture

### Two Variants

**Vanilla JPS (3D)** - for dynamic terrain or when preprocessing isn't desired
- Computes jump points at runtime (standard JPS behavior)
- Ladders detected as forced jump points during scan
- Ladder graph can be precomputed or computed on-demand

**JPS+ (3D)** - for static terrain, maximum query speed
- Full preprocessing of jump distances per z-level
- Precomputed ladder graph with all-pairs distances
- Query time is essentially: two JPS+ lookups + tiny graph traversal

### The Ladder Graph

Central to both variants is the **ladder graph** - a small graph where:
- **Nodes** are ladder endpoints (each ladder has 2 endpoints, one per z-level)
- **Edges** are:
  - Vertical connections: ladder_low ↔ ladder_high (cost: climb cost, e.g., 10)
  - Horizontal connections: ladder_A ↔ ladder_B on same z-level (cost: JPS/JPS+ distance)

For a typical map with 50-200 ladders, this graph has 100-400 nodes. All-pairs shortest paths on this is trivial to precompute and store.

```
Example ladder graph:

    z=0                 z=1                 z=2
    
    [L1_low]---100---[L2_low]    [L1_high]---50---[L3_low]    [L3_high]
        |                            |                            |
       10 (climb)                   10                           10
        |                            |                            |
    [L1_high]                   [L2_high]---75---[L3_low]    [goal area]
```

### Query Flow

**At query time (JPS+ 3D):**

1. **Connect start to ladder graph**
   - Use JPS+ to find distances from start to all reachable ladders on start's z-level
   - These become temporary edges into the ladder graph
   
2. **Connect goal to ladder graph**
   - Use JPS+ to find distances from all ladders on goal's z-level to the goal
   - These become temporary edges out of the ladder graph

3. **Search the ladder graph**
   - Now we have: start → [ladder graph] → goal
   - Run A*/Dijkstra on this small graph (100-400 nodes)
   - Result: the sequence of ladders to use

4. **Reconstruct full path**
   - For each segment (start→ladder, ladder→ladder, ladder→goal)
   - Use JPS+ to get the actual cell-by-cell path

**For vanilla JPS 3D:**

Same flow, but steps 1 and 2 use runtime JPS instead of precomputed JPS+. The ladder graph horizontal edges can either be precomputed or computed on-demand.

## Data Structures

### Per Z-Level (JPS+ only)

```c
// Existing JPS+ structure, now per z-level
int16_t jpsDist[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH][8];
```

Each z-level has independent jump distance tables. Ladders are marked so the jump scan stops at them.

### Ladder Graph

```c
// Ladder endpoint - one per z-level that a ladder touches
typedef struct {
    int x, y, z;           // Position
    int ladderIndex;       // Which ladder this belongs to
    bool isLow;            // Is this the lower or upper endpoint?
} LadderEndpoint;

// Precomputed distances between ladder endpoints on same z-level
// Using compressed storage since this is sparse
typedef struct {
    int from;              // LadderEndpoint index
    int to;                // LadderEndpoint index  
    int cost;              // JPS+ distance between them (-1 if unreachable)
} LadderEdge;

// The complete ladder graph
typedef struct {
    LadderEndpoint endpoints[MAX_LADDER_ENDPOINTS];
    int endpointCount;
    
    LadderEdge edges[MAX_LADDER_EDGES];
    int edgeCount;
    
    // For fast lookup: which endpoints are on each z-level?
    int endpointsByLevel[MAX_GRID_DEPTH][MAX_ENDPOINTS_PER_LEVEL];
    int endpointsPerLevelCount[MAX_GRID_DEPTH];
} LadderGraph;
```

### All-Pairs Ladder Distances (JPS+ only)

```c
// Precomputed shortest path between any two ladder endpoints
// Through the ladder graph (including climbs)
int ladderAllPairs[MAX_LADDER_ENDPOINTS][MAX_LADDER_ENDPOINTS];
```

With 200 endpoints, this is 200×200×4 = 160KB. Acceptable.

## Preprocessing Steps (JPS+)

### Step 1: Standard JPS+ Per Level

For each z-level, compute jump distances in 8 directions. This is the existing `PrecomputeJpsPlus()` logic, extended to all z-levels.

**Modification:** When computing jump distances, treat ladder cells as forced stop points. The jump scan should:
- Stop at ladder cells (return them as jump points)
- Not jump "through" a ladder to the cell beyond

### Step 2: Build Ladder Endpoints

Scan the grid for ladder pairs (CELL_LADDER at z and z+1). For each:
- Create endpoint at (x, y, z) marked as "low"
- Create endpoint at (x, y, z+1) marked as "high"
- Record which ladder they belong to

### Step 3: Compute Same-Level Ladder Distances

For each z-level:
- Get all ladder endpoints on this level
- For each pair, run JPS+ to find the distance
- Store in LadderEdge array

This is O(L² × JPS+) per level where L is ladders per level. With JPS+ being nearly instant, this is fast even for many ladders.

### Step 4: Compute All-Pairs Through Graph

The ladder graph is now complete:
- Nodes: all endpoints
- Edges: vertical (climb cost) + horizontal (JPS+ distances from step 3)

Run Floyd-Warshall or repeated Dijkstra to compute all-pairs shortest paths. Store in `ladderAllPairs[][]`.

This is O(E³) for Floyd-Warshall where E is endpoint count. With 200 endpoints, this is 8 million operations - instant.

## Query Algorithm (JPS+ 3D)

```
function FindPath3D_JpsPlus(start, goal):
    if start.z == goal.z:
        // Same level - pure JPS+
        return JpsPlus2D(start, goal, start.z)
    
    // Different levels - use ladder graph
    
    // Step 1: Connect start to ladder graph
    startDistances = []
    for each endpoint E on level start.z:
        dist = JpsPlus2D(start, E.position, start.z)
        if dist >= 0:
            startDistances.append((E.index, dist))
    
    // Step 2: Connect goal to ladder graph  
    goalDistances = []
    for each endpoint E on level goal.z:
        dist = JpsPlus2D(E.position, goal, goal.z)
        if dist >= 0:
            goalDistances.append((E.index, dist))
    
    // Step 3: Find best path through ladder graph
    bestCost = INFINITY
    bestStartEndpoint = -1
    bestGoalEndpoint = -1
    
    for each (startEP, startDist) in startDistances:
        for each (goalEP, goalDist) in goalDistances:
            totalCost = startDist + ladderAllPairs[startEP][goalEP] + goalDist
            if totalCost < bestCost:
                bestCost = totalCost
                bestStartEndpoint = startEP
                bestGoalEndpoint = goalEP
    
    // Step 4: Reconstruct path
    // - JPS+ from start to bestStartEndpoint
    // - Follow ladder graph path to bestGoalEndpoint
    // - JPS+ from bestGoalEndpoint to goal
    return ReconstructFullPath(...)
```

### Optimization: Early Termination

In step 3, if we sort `startDistances` and `goalDistances` by distance, we can potentially terminate early when the remaining combinations can't beat the current best.

### Optimization: Sparse Start/Goal Connection

Instead of connecting to ALL endpoints on a level, only connect to "nearby" ones. Use spatial hashing or a simple distance threshold. Most endpoints will be unreachable or suboptimal anyway.

## Query Algorithm (Vanilla JPS 3D)

Same structure, but:
- Replace `JpsPlus2D()` with `JPS2D()` (runtime jump detection)
- Ladder graph horizontal edges can be:
  - Precomputed (hybrid approach)
  - Computed on-demand and cached
  - Computed fresh each query (slower but simplest)

The vanilla variant is useful when:
- Terrain changes occasionally (rebuild ladder graph, not full JPS+ tables)
- Memory is constrained
- Preprocessing time must be minimal

## Implementation Plan

### Phase 1: JPS+ Per-Level Foundation
- [ ] Extend `jpsDist` array to be per-z-level
- [ ] Update `PrecomputeJpsPlus()` to iterate all z-levels
- [ ] Treat ladder cells as forced jump points in scan
- [ ] Add `JpsPlus2D(start, goal, z)` function that operates on single level
- [ ] Tests: verify same-level paths work correctly

### Phase 2: Ladder Graph Structure
- [ ] Define `LadderEndpoint`, `LadderEdge`, `LadderGraph` structures
- [ ] Build ladder endpoints from existing `ladderLinks` data
- [ ] Index endpoints by z-level for fast lookup
- [ ] Tests: verify graph structure matches ladder layout

### Phase 3: Ladder Graph Edge Computation
- [ ] Compute same-level distances between ladder endpoints using JPS+
- [ ] Store horizontal edges
- [ ] Add vertical edges (climb costs)
- [ ] Tests: verify edge costs are correct

### Phase 4: All-Pairs Precomputation
- [ ] Implement Floyd-Warshall or repeated Dijkstra on ladder graph
- [ ] Store `ladderAllPairs[][]`
- [ ] Tests: verify shortest paths through multiple ladders

### Phase 5: 3D Query Implementation
- [ ] Implement `FindPath3D_JpsPlus()`
- [ ] Connect start/goal to ladder graph
- [ ] Look up best route through all-pairs table
- [ ] Reconstruct full cell-level path
- [ ] Tests: verify paths across z-levels

### Phase 6: Vanilla JPS 3D
- [ ] Add ladder-as-jump-point detection to runtime JPS
- [ ] Implement `FindPath3D_JPS()` using same ladder graph
- [ ] Option for precomputed vs on-demand ladder edges
- [ ] Tests: verify matches JPS+ results

### Phase 7: Optimization and Polish
- [ ] Sparse start/goal connection (don't check all endpoints)
- [ ] Early termination in path search
- [ ] Benchmark and profile
- [ ] Update demo to visualize 3D JPS paths

## Memory Budget

For a 512×512×16 grid with ~100 ladders:

| Data | Size |
|------|------|
| JPS+ tables (16 levels) | 512×512×8×2 bytes × 16 = 64 MB |
| Ladder endpoints | 200 × 16 bytes = 3.2 KB |
| Ladder edges | ~2000 × 12 bytes = 24 KB |
| All-pairs distances | 200×200×4 bytes = 160 KB |
| **Total additional** | ~65 MB |

The JPS+ tables dominate. This is the same cost as 16× single-level JPS+ - no additional memory for the 3D support beyond the small ladder graph.

## Comparison with HPA*

| Aspect | HPA* 3D | JPS+ 3D |
|--------|---------|---------|
| Preprocessing | Fast (seconds) | Slow (can be minutes for large grids) |
| Terrain changes | Incremental updates | Full rebuild required |
| Query speed | Fast | Faster |
| Memory | Lower | Higher (JPS+ tables) |
| Path quality | Near-optimal | Optimal |
| Best for | Dynamic terrain, colony sims | Static terrain, shipping product |

Both use the same conceptual model (2D layers + ladder portals). HPA* builds a coarser abstraction (chunks), JPS+ precomputes exact jump distances.

## Open Questions

1. **Jump through ladders?** Should the JPS scan be allowed to "see past" a ladder cell, or does the ladder completely block the scan? Current design: ladder blocks the scan (cleaner semantics).

2. **One-way ladders?** Drop-down holes, up-only climbs. The ladder graph handles this naturally (directed edges), but the preprocessing needs to track direction.

3. **Multi-level ladders?** Elevators going z=0 to z=5 directly. Could be a single edge in the ladder graph with appropriate cost.

4. **Caching partial results?** If many queries have similar start/goal areas, could we cache the "connect to ladder graph" results?
