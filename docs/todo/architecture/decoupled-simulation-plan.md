# Plan: Decoupled Simulation + Multiple Views Architecture

**Status**: Planning document (not yet implementing)

**Related future work**: Z-layers support (separate plan, will be built in src/demo first)

---

## Goal

Decouple the pathfinding simulation from rendering so that:
- The simulation runs as a "headless backend" in grid-cell coordinates
- Multiple views can observe the same simulation from different perspectives
- Views use 2.5D projection (32px wide × 16px deep cells)
- One mover at x=0→100 can appear left-to-right in one view, right-to-left in another

## Architecture Overview

```
navkit/
├── src/              # Existing - keep working
│   ├── grid.c/h
│   ├── pathfinding.c/h
│   ├── mover.c/h         # Refactor: remove CELL_SIZE dependency
│   ├── terrain.c/h
│   └── demo.c            # Keep as-is (legacy top-down demo)
│
├── simulation/           # NEW - headless simulation layer
│   ├── simulation.c/h    # Owns grid + movers, tick-based updates
│
└── game/                 # NEW - 2.5D game using simulation
    ├── main.c            # Entry point, creates simulation + view
    ├── view.c/h          # 2.5D rendering with projection
    └── Makefile
```

## Implementation Phases

### Phase 1: Refactor mover.c to use grid-cell coordinates

**Files to modify**: `src/mover.h`, `src/mover.c`, `src/test_mover.c`

**Changes**:
- Change mover positions from pixels to grid cells (floats, e.g., `x=5.3` means 30% into cell 5)
- Change speed from pixels/second to cells/second
- Remove `CELL_SIZE` from mover logic entirely
- Update all position calculations: waypoint targeting, cell detection, line-of-sight
- Update tests to use grid-cell coordinates

**Before**:
```c
#define CELL_SIZE 32
float x, y;  // pixels
float speed = 100.0f;  // pixels/sec
int cellX = (int)(m->x / CELL_SIZE);
```

**After**:
```c
float x, y;  // grid cells (e.g., 5.3, 2.7)
float speed = 3.0f;  // cells/sec
int cellX = (int)m->x;
```

### Phase 2: Update demo.c to handle the new mover coordinates

**Files to modify**: `src/demo.c`

**Changes**:
- Keep `CELL_SIZE` in demo.c only (for rendering)
- Convert mover grid positions to screen pixels when rendering
- Update mouse click handling: screen pixels → grid cells
- Update mover spawning to use grid coordinates

### Phase 3: Create simulation layer

**New files**: `simulation/simulation.h`, `simulation/simulation.c`

**Contents**:
```c
typedef struct Simulation {
    CellType grid[MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
    int gridWidth, gridHeight;
    
    Mover movers[MAX_MOVERS];
    int moverCount;
    
    // HPA* state (chunks, graph, entrances)
    // ... imported from pathfinding
} Simulation;

void Sim_Init(Simulation* sim, int width, int height);
void Sim_LoadMap(Simulation* sim, const char* mapFile);
void Sim_Update(Simulation* sim, float dt);
int  Sim_SpawnMover(Simulation* sim, float x, float y, int goalX, int goalY);
void Sim_SetMoverGoal(Simulation* sim, int moverIndex, int goalX, int goalY);

// Read-only accessors for views
const Mover* Sim_GetMovers(Simulation* sim, int* count);
CellType Sim_GetCell(Simulation* sim, int x, int y);
```

### Phase 4: Create 2.5D game/view

**New files**: `game/main.c`, `game/view.h`, `game/view.c`, `game/Makefile`

**View configuration**:
```c
typedef struct View {
    float cellWidth;   // 32 for 2.5D
    float cellHeight;  // 16 for 2.5D
    float offsetX, offsetY;
    float zoom;
    bool flipX;        // flip for "other side" view
} View;

Vector2 View_GridToScreen(View* v, float gridX, float gridY);
```

**Depth sorting**: Render movers sorted by Y (higher Y = render first, appears behind)

**Directional facing**: Calculate from velocity, use for sprite selection later

### Phase 5: Add sprite support (later, when assets are ready)

- Sprite sheet loading
- Direction-based frame selection (8 directions or 4)
- Animation frames for walking

## Key Files to Modify/Create

| File | Action | Purpose |
|------|--------|---------|
| `src/mover.h` | Modify | Remove CELL_SIZE, document grid-cell coords |
| `src/mover.c` | Modify | All positions in grid cells, speed in cells/sec |
| `src/test_mover.c` | Modify | Update tests for new coordinate system |
| `src/demo.c` | Modify | Add grid→pixel conversion for rendering |
| `simulation/simulation.h` | Create | Simulation interface |
| `simulation/simulation.c` | Create | Simulation implementation |
| `game/main.c` | Create | 2.5D game entry point |
| `game/view.h` | Create | View/projection interface |
| `game/view.c` | Create | 2.5D rendering implementation |
| `game/Makefile` | Create | Build the 2.5D game |

## Verification

After each phase:

1. **Phase 1**: Run `test_mover` - all tests should pass with grid-cell coordinates
2. **Phase 2**: Run `demo` - should work exactly as before visually
3. **Phase 3**: Write a simple test that creates a simulation, spawns a mover, updates it
4. **Phase 4**: Run the new game - see movers rendered in 2.5D with depth sorting
5. **Phase 5**: Movers display directional sprites instead of shapes
