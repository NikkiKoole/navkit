# Next Up - NavKit Pathfinding

## Current State

You have a solid foundation:
- **HPA*** hierarchical pathfinding working
- **Movers** with path following, string pulling, automatic replanning
- **Visual debugging** with color-coded states
- **Deterministic simulation** with tests
- **Steering behaviors** in `steering/` (28+ behaviors including separation, flocking, etc.)
- **Crowd simulation** in `crowd-experiment/` (optimized for 1000+ agents)

## Suggested Next Steps

| Feature | Effort | Impact | Description |
|---------|--------|--------|-------------|
| **Mover Avoidance** | Medium | High | Add collision/separation to movers using existing steering code |
| **Wall Sliding** | Low | Medium | Agents slide along walls - becomes useful once avoidance pushes movers off-path |
| **Lazy Refinement** | Medium | High | Only refine 1-2 segments ahead; faster response, less wasted work |
| **Path Caching** | Low | Medium | Cache recent paths with LRU eviction |
| **Parallel Pathfinding** | Low | Medium | Thread-local node data for concurrent queries |
| **Z-Levels** | High | High | Multi-floor support (design already documented) |

## Social/Crowd Features (longer term)

- Queuing at bottlenecks
- Agent yielding
- Lane preference (keep-right)
- Personal space management

## Recommendation

1. **Mover avoidance** - Integrate separation/collision from steering into movers so they don't overlap
2. **Wall sliding** - Once movers get pushed around by avoidance, wall sliding prevents them getting stuck
3. Then continue with lazy refinement or other optimizations
