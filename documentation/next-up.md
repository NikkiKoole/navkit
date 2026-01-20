# Next Up - NavKit Pathfinding

## Current State

The pathfinding system is feature-complete for basic use:

- **HPA*** hierarchical pathfinding with incremental updates
- **Movers** with path following, string pulling, automatic replanning
- **Mover Avoidance** - separation from other movers (toggleable)
- **Wall Handling** - repulsion and sliding to prevent wall clipping
- **Knot Fix** - larger waypoint arrival + reduced avoidance near waypoints
- **Stuck Detection** - auto-repath when movers can't make progress
- **Visual debugging** with color-coded states
- **Deterministic simulation** with tests

## Suggested Next Steps

| Feature | Effort | Impact | Description |
|---------|--------|--------|-------------|
| **Lazy Refinement** | Medium | High | Only refine 1-2 path segments ahead; faster response, less wasted work |
| **Path Caching** | Low | Medium | Cache recent paths with LRU eviction |
| **Parallel Pathfinding** | Low | Medium | Thread-local node data for concurrent queries |
| **Z-Levels** | High | High | Multi-floor support (see z-layers-plan.md) |
| **Decoupled Simulation** | Medium | Medium | Headless sim with 2.5D views (see decoupled-simulation-plan.md) |

## Social/Crowd Features (longer term)

- Queuing at bottlenecks
- Agent yielding
- Lane preference (keep-right)
- Personal space management

## Completed Features

- [x] Mover avoidance (separation from other movers)
- [x] Wall repulsion (push away from walls)
- [x] Wall sliding (slide along walls instead of clipping)
- [x] Knot fix (larger waypoint arrival + reduced avoidance near waypoints)
- [x] Stuck detection with auto-repath
