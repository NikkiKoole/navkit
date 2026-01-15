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
│  LAYER 1: Full Steering Behaviors                    ← START    │
│  Seek, flee, arrive, pursuit, evasion, wander, containment      │
│  Currently have: separation only                                │
└─────────────────────────────────────────────────────────────────┘
```

### Layer 1: Full Steering Behaviors
- [ ] **Seek** - move toward a target position
- [ ] **Flee** - move away from a target position
- [ ] **Arrive** - seek with deceleration (smooth stop)
- [ ] **Pursuit** - seek a moving target's predicted position
- [ ] **Evasion** - flee from a moving target's predicted position
- [ ] **Wander** - natural-looking random movement
- [ ] **Containment** - stay within boundaries

### Layer 2: Static Obstacles & Walls
- [ ] **Wall representation** - line segments or grid-based
- [ ] **Obstacle shapes** - circles, AABBs, convex polygons
- [ ] **Spatial queries** - "what obstacles are near me?"
- [ ] **Ray casting** - line-of-sight, wall detection ahead
- [ ] **Obstacle avoidance steering** - feeler rays or force fields
- [ ] **Wall sliding** - slide along walls rather than stopping

### Layer 3: Pathfinding Integration
- [ ] **Path following behavior** - smoothly traverse waypoints
- [ ] **Path smoothing** - string-pulling, remove unnecessary waypoints
- [ ] **Dynamic replanning** - request new path when blocked

### Layer 4: Social Navigation
- [ ] **Queuing** - orderly lines at bottlenecks
- [ ] **Yielding** - agents give way to each other
- [ ] **Lane preference** - keep-right/left for counter-flow
- [ ] **Following** - agents follow others
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
│  - HPA* ✓         │  - Steering behaviors       │
│  - JPS ✓          │  - Social navigation        │
│  - Flow Fields    │                             │
├───────────────────┴─────────────────────────────┤
│  Movement Layer                                 │
│  - Path smoothing                               │
│  - Formations                                   │
│  - Animation integration                        │
└─────────────────────────────────────────────────┘
```
