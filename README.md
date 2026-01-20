# navkit

A fast, minimal navigation stack for 2D games. Pathfinding + steering + crowd simulation in C.

## Features

### Pathfinding (`/pathing`)

- **A*, HPA*, JPS** algorithms
- **Movers** with path following, avoidance, and auto-replanning
- **Wall handling** - repulsion + sliding to prevent clipping
- **Stuck detection** - auto-repath when agents can't progress
- Incremental chunk updates, string pulling, 60Hz fixed timestep

### Steering (`/steering`)

- **28+ behaviors** - seek, flee, arrive, pursuit, wander, flocking, etc.
- **Social Force Model**, **Context Steering**, **IDM traffic**
- **47 demo scenarios** - evacuation, convoy, predator/prey, murmuration

### Crowd Simulation (`/crowd-experiment`)

- **1000+ agents at 60 FPS** using spatial hashing
- Boids separation, stuck recovery, Y-sorted 2.5D rendering

## Build & Run

```bash
make              # requires raylib
./bin/path        # pathfinding demo
./bin/steer       # steering behaviors
./bin/crowd       # crowd simulation
```

## Documentation

See `/documentation` for detailed design docs:
- `overview.md` - system architecture
- `steering.md` - all 28+ steering behaviors
- `roadmap.md` - what's done, what's planned

## License

MIT
