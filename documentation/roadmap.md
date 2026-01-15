# Roadmap

Components to add to the navigation system.

---

# Priority: Path to Social Navigation

These must be implemented in order - each layer builds on the previous.

```
┌─────────────────────────────────────────────────────────────────┐
│  LAYER 4: Social Navigation                                     │
│  Queuing, yielding, lane preference, following                  │
│  Requires: steering behaviors + obstacles + pathfinding         │
├─────────────────────────────────────────────────────────────────┤
│  LAYER 3: Pathfinding Integration                               │
│  Path following behavior, path smoothing, dynamic replanning    │
│  Requires: steering behaviors + obstacles                       │
├─────────────────────────────────────────────────────────────────┤
│  LAYER 2: Static Obstacles & Walls                              │
│  Wall representation, obstacle shapes, ray casting, wall slide  │
│  Requires: steering behaviors                                   │
├─────────────────────────────────────────────────────────────────┤
│  LAYER 1: Full Steering Behaviors                    ✓ DONE     │
│  All Craig Reynolds behaviors implemented                       │
│  See steering/steering.h for full API                           │
└─────────────────────────────────────────────────────────────────┘
```

### Layer 1: Full Steering Behaviors ✓

**Individual Behaviors:**
- [x] **Seek** - move toward a target position
- [x] **Flee** - move away from a target position
- [x] **Arrive** - seek with deceleration (smooth stop)
- [x] **Pursuit** - seek a moving target's predicted position
- [x] **Evasion** - flee from a moving target's predicted position
- [x] **Offset Pursuit** - pursue with lateral offset (strafing, fly-by)
- [x] **Wander** - natural-looking random movement
- [x] **Containment** - stay within boundaries
- [x] **Face** - rotate to look at target (orientation only)
- [x] **Look Where You're Going** - face movement direction
- [x] **Match Velocity** - match another agent's velocity
- [x] **Interpose** - position between two agents
- [x] **Hide** - use obstacles to hide from pursuer
- [x] **Shadow** - approach then match target's heading

**Obstacle/Wall Behaviors:**
- [x] **Obstacle Avoidance** - feeler rays to avoid circular obstacles
- [x] **Wall Avoidance** - steer away from line segment walls
- [x] **Wall Following** - maintain offset from walls
- [x] **Path Following** - follow waypoints smoothly
- [x] **Flow Field Following** - align with vector field

**Group Behaviors:**
- [x] **Separation** - repel from nearby agents
- [x] **Cohesion** - move toward group center
- [x] **Alignment** - match neighbors' heading
- [x] **Flocking** - separation + cohesion + alignment combined
- [x] **Leader Following** - follow behind a leader
- [x] **Collision Avoidance** - predict and avoid collisions

**Combination Helpers:**
- [x] **Blended Steering** - weighted sum of behaviors
- [x] **Priority Steering** - first non-zero behavior wins

**Files:** `steering/steering.h`, `steering/steering.c`
**Demo:** `make steer` then press 1-9, 0, Q, W, E for scenarios

### Layer 2: Static Obstacles & Walls
- [x] **Wall representation** - line segments (Wall struct)
- [x] **Obstacle shapes** - circles (CircleObstacle struct)
- [ ] **Spatial queries** - "what obstacles are near me?"
- [ ] **Ray casting** - line-of-sight, wall detection ahead
- [x] **Obstacle avoidance steering** - feeler rays or force fields
- [ ] **Wall sliding** - slide along walls rather than stopping

### Layer 3: Pathfinding Integration
- [x] **Path following behavior** - smoothly traverse waypoints
- [ ] **Path smoothing** - string-pulling, remove unnecessary waypoints
- [ ] **Dynamic replanning** - request new path when blocked

### Layer 4: Social Navigation
- [ ] **Queuing** - orderly lines at bottlenecks
- [ ] **Yielding** - agents give way to each other
- [ ] **Lane preference** - keep-right/left for counter-flow
- [x] **Following** - agents follow others (Leader Following)
- [ ] **Personal space** - context-dependent spacing

See [social-navigation.md](social-navigation.md) for detailed design.

---

# Other Components (no strict order)

## Flow Fields / Vector Fields
- Pre-computed direction maps for mass unit movement toward a single target
- Much more efficient than individual pathfinding when many units share a goal
- Used in RTS games (Supreme Commander, StarCraft 2)

## Navigation Mesh (NavMesh)
- Polygon-based walkable areas instead of grid cells
- More memory efficient for large open spaces
- Better for 3D or irregular terrain
- Pairs well with HPA* for hierarchical queries

## ~~ORCA/RVO~~ — REMOVED
We experimented with ORCA but removed it because:
- Linear programming solver produces jerky velocity changes
- Falls back to heuristics when constraints become infeasible
- Agents appear to "teleport" their velocity direction

## Formation System
- Units moving in formation (wedge, line, box)
- Leader-follower patterns
- Formation pathfinding (the formation as a single entity)

## Spatial Queries Layer
- Unified interface for "find nearest X" queries
- K-nearest neighbors, range queries, ray/line-of-sight
- Currently have bucket grids; could add quadtrees, R-trees, or BVH

## Terrain Cost / Influence Maps
- Variable movement costs (mud slows, roads speed up)
- Threat/danger maps for tactical AI
- Heat maps showing congestion

## Multi-layer Navigation
- Bridges, tunnels, multi-floor buildings
- Teleporters, ladders, jump pads
- Each layer has its own navmesh/grid with connections

## Debug Visualization Tools
- Path visualization (implemented)
- Velocity vectors, avoidance radii
- Spatial grid overlay
- Profiling overlays (implemented in steering)

---

# Architecture

```
┌─────────────────────────────────────────────────┐
│              Navigation System                  │
├─────────────────────────────────────────────────┤
│  Spatial Index    │  World Representation       │
│  - Bucket Grid ✓  │  - Grid ✓                   │
│  - Quadtree       │  - NavMesh                  │
│  - BVH            │  - Flow Fields              │
├───────────────────┼─────────────────────────────┤
│  Pathfinding      │  Local Avoidance            │
│  - A* ✓           │  - Separation ✓             │
│  - HPA* ✓         │  - Steering behaviors ✓     │
│  - JPS ✓          │  - Social navigation        │
│  - Flow Fields    │                             │
├───────────────────┴─────────────────────────────┤
│  Movement Layer                                 │
│  - Path smoothing                               │
│  - Formations                                   │
│  - Animation integration                        │
└─────────────────────────────────────────────────┘
```
