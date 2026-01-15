# Overview

This is a **game AI/agent simulation system** built with **raylib** (a C game development library). It has two main components:

### 1. Pathing System (`/pathing`)
A sophisticated **pathfinding framework** supporting multiple algorithms:

- **A\* (A-Star)** - Classic grid-based pathfinding with 4-dir or 8-dir movement
- **HPA\* (Hierarchical Pathfinding A\*)** - A hierarchical approach that divides the grid into chunks, finds "entrances" (transition points) between chunks, builds an abstract graph, then refines paths locally. Great for large maps.
- **JPS (Jump Point Search)** - An optimized A* variant that "jumps" over empty areas

**Key features:**
- Chunked grid system (configurable sizes up to 512x512)
- Multiple terrain generators (sparse, city, perlin noise, concentric maze)
- Incremental updates - when you modify walls, only dirty chunks are rebuilt
- Multi-agent pathfinding support (spawn multiple agents with paths)
- Binary heaps for efficient priority queues

### 2. Steering/Crowd Simulation (`/steering`)
A **large-scale agent crowd simulation** (100,000 agents!) with:

- **Boids-style separation** - Agents avoid each other using spatial queries
- **Bucketed spatial grid** - Fast neighbor lookups using prefix sums
- **Cache optimizations** - Agents sorted by grid cell for memory coherency
- **Y-sorting for 2.5D rendering** - Correct draw order using band-bucket sort
- **Stuck detection & wiggle recovery** - Agents detect when stuck and apply random perturbations
- **Speed-relative avoidance** - Slower agents avoid differently than faster ones

**Performance tuning:**
- Configurable avoidance radius, neighbor caps, scan limits
- Staggered avoidance updates (AVOID_PERIOD = 3)
- Real-time profiler showing grid build, update, y-sort, and draw times

## In Summary

This is a **2D game engine subsystem** focused on AI movement - combining:
1. **Strategic pathfinding** (finding routes through obstacles)
2. **Local steering** (crowd behavior, collision avoidance)

This is essentially the navigation stack you'd need for an RTS, city builder, or any game with many moving units.
