# Overview

This is a **game AI/agent simulation system** built with **raylib** (a C game development library). It has two main components:

## 1. Pathing System (`/pathing`)

A sophisticated **pathfinding framework** with multiple algorithms and mover simulation.

### Pathfinding Algorithms
- **A\*** - Classic grid-based pathfinding with 4-dir or 8-dir movement
- **HPA\*** - Hierarchical approach: chunks, entrances, abstract graph, local refinement
- **JPS** - Jump Point Search, optimized A* that skips empty areas
- **JPS+** - JPS with precomputed jump distances

### Grid Features
- Chunked grid system (configurable sizes up to 512x512)
- Multiple terrain generators (sparse, city, perlin, maze, dungeon, caves, etc.)
- Incremental updates - only dirty chunks rebuilt when terrain changes

### Mover System
Movers are agents that follow paths with realistic behavior:

- **Path following** with string pulling optimization
- **Mover avoidance** - boids-style separation from neighbors
- **Wall handling**:
  - Wall repulsion (push away before collision)
  - Wall sliding (slide along walls)
- **Knot fix** - prevents movers getting stuck at shared waypoints
- **Stuck detection** - auto-repath when no progress made
- **Automatic replanning** when path is blocked (LOS check)
- 60Hz fixed timestep simulation

### Demo Controls (Movers section)
- **Avoidance**: Enable/disable mover separation
- **Directional**: Filter avoidance by wall clearance
- **Walls**: Repulsion, sliding, knot fix toggles
- **Debug Views**: Visualize neighbors, open areas, knots, stuck movers

## 2. Steering/Crowd Simulation (`/steering`)

A **large-scale agent crowd simulation** (100,000 agents!) with:

- **28+ steering behaviors** - seek, flee, arrive, pursuit, wander, flocking, etc.
- **Boids-style separation** - agents avoid each other using spatial queries
- **Bucketed spatial grid** - fast neighbor lookups using prefix sums
- **Y-sorting for 2.5D rendering** - correct draw order
- **Context steering** - interest/danger maps for intelligent navigation
- **Social Force Model** - physics-based crowd simulation
- **Vehicle behaviors** - pure pursuit, stanley controller, IDM

See `steering.md` for full behavior reference.

## Architecture

```
navkit/
├── pathing/           # Pathfinding + movers
│   ├── grid.c/h       # Grid representation
│   ├── pathfinding.c/h # A*, HPA*, JPS algorithms
│   ├── mover.c/h      # Mover simulation with avoidance
│   ├── terrain.c/h    # Map generators
│   └── demo.c         # Interactive demo
│
├── steering/          # Steering behaviors library
│   ├── steering.c/h   # 28+ behaviors
│   └── demo.c         # Behavior showcase
│
├── crowd-experiment/  # Large-scale crowd test
│
└── documentation/     # Design docs and plans
```

## Running the Demos

```bash
make           # Build all
./bin/path     # Pathfinding demo with movers
./bin/steer    # Steering behaviors showcase
./bin/crowd    # Large crowd simulation
```
