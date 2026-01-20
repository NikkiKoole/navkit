# Collision Avoidance for Movers - Implementation Plan

## Current State Summary

### Pathfinding System (`pathing/`)
- **HPA*** hierarchical pathfinding with chunk-based abstraction
- **Movers** follow waypoint paths at 60Hz fixed timestep
- Movers have: position, goal, path array, speed, needsRepath flag
- Automatic replanning when line-of-sight breaks (max 10 repaths/frame)
- String pulling optimizes paths by removing unnecessary waypoints

### Crowd Experiment (`crowd-experiment/demo.c`)
Already has working collision avoidance for 1000 agents:

**Key Algorithm:**
```c
// Quadratic distance-based repulsion
float u = 1.0f - (dist / AVOID_RADIUS);  // Falloff [0,1]
float strength = u * u;                   // Quadratic curve
avoidance += direction_away * strength;
```

**Optimizations used:**
- Spatial grid bucketing (cell size = AVOID_RADIUS * 0.5 = 20)
- Prefix-sum based bucket building
- Avoidance cached every 3 frames (not computed every tick)
- Max 48 agents scanned, max 10 neighbors considered
- Agents sorted by grid cell every 60 frames for cache coherency

**Parameters:**
| Parameter | Value |
|-----------|-------|
| AVOID_RADIUS | 40.0 |
| AVOID_STRENGTH_SCALE | 0.5 |
| AVOID_MAX_NEIGHBORS | 10 |
| AVOID_PERIOD | 3 frames |
| STUCK_THRESHOLD | 1.5 seconds |
| WIGGLE_STRENGTH | 40.0 |

**Stuck recovery:** Random wiggle direction applied when agent makes no progress for 1.5s

### Steering Library (`steering/steering.h`)
28+ behaviors already implemented including:
- `steering_separation()` - Repulsion from neighbors
- `steering_collision_avoid()` - Predictive collision avoidance
- `steering_wall_avoid()` - Wall repulsion with feeler rays

---

## What Needs to Be Added to Movers

### 1. Spatial Data Structure
Currently movers have no efficient neighbor lookup. Need:
- Grid-based spatial hash (like crowd_experiment)
- Cell size ~20-40 pixels (half of avoidance radius)
- Rebuild buckets each tick or use incremental updates

### 2. Avoidance Force Calculation
In `UpdateMovers()` after computing desired direction to waypoint:
```c
// Pseudocode
Vector2 avoidance = ComputeAvoidance(mover, neighbors);
Vector2 desired = normalize(waypoint - position) * speed;
Vector2 final_vel = desired + avoidance * AVOID_STRENGTH;
position += final_vel * dt;
```

### 3. Mover Struct Extensions
```c
// Potential additions to Mover struct:
Vector2 vel;           // Current velocity (for prediction)
Vector2 avoidCache;    // Cached avoidance force
float stuck;           // Stuck counter
Vector2 wiggleDir;     // Random unstuck direction
```

### 4. Integration Points in `mover.c`

| Location | Change |
|----------|--------|
| `UpdateMovers()` ~line 165 | Add avoidance force after movement calculation |
| New function | `BuildMoverGrid()` - spatial bucketing |
| New function | `ComputeMoverAvoidance()` - separation force |
| `Tick()` | Call grid rebuild before UpdateMovers |

---

## Two Possible Approaches

### Option A: Port crowd_experiment directly
- Copy spatial grid + avoidance code from `crowd-experiment/demo.c`
- Adapt to Mover struct (currently uses separate Agent struct)
- Proven to work at 1000 agents / 60fps

### Option B: Use steering library
- Use `steering_separation()` from `steering/steering.h`
- Still need spatial grid for neighbor lookup
- More modular, could add other behaviors later

---

## Files That Would Be Modified

| File | Changes |
|------|---------|
| `pathing/mover.h` | Add velocity, avoidCache, stuck fields to Mover struct |
| `pathing/mover.c` | Add spatial grid, avoidance computation, integrate into UpdateMovers |
| `pathing/demo.c` | Possibly add debug visualization toggles |

---

## Key Insights

1. **crowd_experiment is the reference** - It already solves this problem efficiently
2. **Spatial grid is essential** - O(n^2) neighbor search won't scale to 10K movers
3. **Caching helps** - Computing avoidance every 3 frames instead of every frame
4. **Stuck recovery needed** - Wiggle behavior when agents can't make progress
5. **Wall sliding becomes important** - Once avoidance pushes movers, they need to slide along walls instead of getting stuck

---

## Reference: crowd_experiment Key Functions

From `crowd-experiment/demo.c`:

- `BuildGridBuckets()` - Builds spatial hash with prefix-sum
- `ComputeAvoidanceBucket()` - Calculates repulsion force from neighbors
- `SortAgentsByCell()` - Periodic sort for cache coherency
- Stuck detection in main update loop
