# navkit

A fast, minimal navigation stack for 2D games. Pathfinding + crowd simulation in C.

## What's inside

**Pathing** — A*, HPA*, and Jump Point Search on grids up to 512x512. Chunked architecture with incremental updates when terrain changes.

**Steering** — Boids-style crowd simulation scaling to 100k agents. Spatial hashing, cache-friendly sorting, stuck detection with wiggle recovery.

## Build

```bash
make
```

Requires [raylib](https://www.raylib.com/).

## License

MIT
