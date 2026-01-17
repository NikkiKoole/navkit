# navkit

A fast, minimal navigation stack for 2D games. Pathfinding + steering + crowd simulation in C.

## What's Inside

### Pathing

A **hierarchical pathfinding system** with three algorithms: A*, HPA*, and JPS.

- **HPA* (Hierarchical Pathfinding A*)** - divides grid into chunks, finds path at entrance-level first, then refines with local A*
- **Entrance detection** - scans chunk borders for walkable transitions
- **Incremental updates** - rebuilds only dirty chunks when terrain changes
- **Binary heaps** for efficient priority queues
- **6 terrain generators** - sparse, city, mixed, perlin, concentric maze

Grid: Up to 512x512, configurable chunk sizes (default 32x32)

### Steering

A **comprehensive steering behaviors library** with 28+ behaviors and advanced algorithms.

- **Two agent models:** Boid (velocity-based) and Vehicle (explicit heading/unicycle)
- **Classic behaviors:** Seek, Flee, Arrive, Pursuit, Wander, Flocking, Leader Following
- **Social Force Model** (Helbing) - physics-based crowd with exponential repulsion
- **Context Steering** - interest/danger maps avoiding vector cancellation
- **IDM (Intelligent Driver Model)** for traffic
- **Couzin Zones** - biologically-grounded zones (repulsion/orientation/attraction)
- **47 demo scenarios** including evacuation, convoy escort, fish+shark, murmuration

### Crowd-Experiment

A **high-performance crowd simulation** using spatial hashing (~1000 agents at 60 FPS).

- **Bucketed prefix-sum spatial grid** for fast neighbor queries (cell size = 20 units)
- **Boids-style separation** with cached avoidance (recomputed every 3 frames, staggered)
- **Stuck detection & wiggle recovery** - applies random force when agents stop progressing
- **Y-sorted 2.5D rendering** using row-bucket insertion sort
- Agents have random goals, wait 1s on arrival, then pick new goal

Parameters: `AVOID_RADIUS=40`, `AVOID_MAX_NEIGHBORS=10`, `STUCK_THRESHOLD=1.5s`

## Build

```bash
make
```

Requires [raylib](https://www.raylib.com/).

## License

MIT
