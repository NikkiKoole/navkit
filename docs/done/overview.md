# NavKit Pathing

A 3D grid-based pathfinding system with multiple algorithms, mover simulation, and an interactive demo built with raylib.

## Overview

The pathing system provides four pathfinding algorithms (A*, HPA*, JPS, JPS+) operating on a chunked 3D grid. Movers are agents that follow computed paths with avoidance, wall sliding, and stuck detection. The demo lets you experiment with all of these features interactively.

## Grid System

The world is represented as a 3D grid of cells, organized into chunks for efficient incremental updates.

### Cell Types

- **WALKABLE** — Open ground that movers can traverse
- **WALL** — Impassable obstacle
- **FLOOR** — Walkable surface on upper z-levels
- **AIR** — Empty space (non-walkable, used above floors)
- **LADDER** — Vertical connection between z-levels
- **LADDER_UP / LADDER_DOWN / LADDER_BOTH** — Directional ladder variants

### Chunked Architecture

The grid divides into 32×32 chunks. When terrain changes, only affected chunks are marked dirty and rebuilt. This keeps pathfinding data current without full rebuilds.

## Pathfinding Algorithms

### A*

Standard grid-based A* with 8-directional movement. Explores neighbors using a priority queue ordered by f = g + h. Works well for small to medium maps.

### HPA* (Hierarchical Pathfinding A*)

Builds an abstract graph from chunk entrances. Pathfinding happens in two phases: first on the abstract graph to find which chunks to traverse, then locally within each chunk. Scales better to large maps than plain A*.

### JPS (Jump Point Search)

Optimized A* that exploits grid structure. Instead of exploring every neighbor, it jumps in straight lines until hitting walls or forced neighbors. Significantly reduces nodes explored on open maps.

### JPS+ (Precomputed JPS)

Precomputes jump distances for every cell in all 8 directions. Runtime pathfinding becomes a series of lookups rather than scans. The fastest algorithm for static maps.

### 3D Pathfinding with Ladders

JPS+ supports multi-level pathfinding through a ladder graph. The system:
1. Scans the grid for ladder connections between z-levels
2. Computes all-pairs shortest paths between ladder endpoints using Floyd-Warshall
3. At query time, connects start/goal to nearby ladder endpoints and finds the best route through the graph
4. Stitches together 2D path segments with ladder transitions

## Mover System

Movers are agents that follow paths with realistic movement behavior.

### Path Following

Movers advance through waypoints toward their goal. String pulling optimization allows movers to skip intermediate waypoints when they have line-of-sight to later ones.

### Avoidance

Movers maintain separation from each other using a spatial grid with prefix sums for O(1) neighbor queries. Configurable options include:
- Avoidance radius
- Directional filtering (ignore movers behind walls)
- Avoidance strength

### Wall Handling

- **Wall repulsion** — Gradual force pushing movers away from walls before collision
- **Wall sliding** — Movers slide along walls rather than stopping
- **Knot fix** — Prevents movers from bunching up at shared waypoints

### Stuck Detection

If a mover makes no progress for a configurable duration, it triggers replanning. The system also checks line-of-sight to the next waypoint and replans if blocked.

## Terrain Generators

The demo includes 13 procedural terrain generators:

| Generator | Description |
|-----------|-------------|
| Labyrinth3D | Multi-floor maze with connecting ladders |
| Spiral3D | Spiraling ramp structure |
| DungeonRooms | Connected rectangular rooms |
| Caves | Organic cave-like terrain |
| Towers | Vertical tower structures with ladder cores |
| GalleryFlat | Open areas with pillars |
| OfficeBuilding | Multi-story building with stairwells and corridors |
| Castle | Walled castle with towers, courtyard, and wall walks |
| CouncilEstate | UK-style estate with tower blocks and terraced housing |
| Mixed | Procedural mix of city and open terrain zones |

Each generator places appropriate ladders to connect floors, enabling full 3D navigation.

## Code Structure

```
src/
├── grid.c/h          # Grid representation, cell types, chunk management
├── pathfinding.c/h   # A*, HPA*, JPS, JPS+ implementations (~3100 lines)
├── mover.c/h         # Mover simulation, avoidance, path following (~1000 lines)
├── terrain.c/h       # Terrain generators (~2300 lines)
└── demo.c            # Interactive raylib demo
```

Total: approximately 7,000 lines of C with no dependencies beyond raylib.

## Running the Demo

```bash
make
./bin/path
```

### Demo Controls

- **Left click** — Set goal for all movers
- **Right click** — Paint/erase walls
- **Number keys** — Switch pathfinding algorithm
- **Sidebar** — Toggle avoidance, wall handling, debug visualization
- **Spawn controls** — Add/remove movers

The demo visualizes paths, explored nodes, chunk boundaries, and mover state in real-time.

## See Also

- [../todo/roadmap.md](../todo/roadmap.md) - Planned features and improvements
- Subfolders here contain design docs for implemented features
