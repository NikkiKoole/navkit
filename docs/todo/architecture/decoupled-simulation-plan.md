 # Plan: Decoupled Simulation + Multiple Views Architecture

**Status**: Planning document (not yet implementing)

**Related future work**: Z-layers support (separate plan, will be built in src/demo first)

---

## Goal

> Status: spec

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

---

## Front View Mode (2026-03-19)

> Status: **implemented (v1)**

A second rendering mode alongside the existing top-down view. Not a replacement — both coexist, player toggles between them.

### Reference

SNES Zelda (A Link to the Past) style: **front-facing 3/4 view**. Not isometric, not side-on platformer. You see the front face of walls, a slight top-down angle on floors, characters facing you.

### Coordinate Mapping

```
Screen X  =  World X
Screen Y  =  World Z (height)
Depth     =  World Y (into the screen), rendered as layers
```

This is the simplest possible projection — no rotation, no skew.

### Dollhouse / Theatre Set Layering

Multiple world-Y slices visible simultaneously, like a diorama with the front wall removed:

```
                depth (world Y) →
                back rows (dimmed)       front row (bright)

z=3   [sky]  [sky]  [roof──────]    |  [sky]  [sky]  [sky]
z=2   [sky]  [sky]  [room  🧑 ]    |  [sky]  [tree] [sky]
z=1   [grass][grass][wall-front]    |  [grass][trunk][grass]
z=0   [dirt] [dirt] [dirt──────]    |  [dirt] [dirt] [dirt]
```

### What's Implemented (v1)

**Controls:**
- `V` — toggle front view mode on/off
- `,`/`.` — scroll Y-slice (depth) in front view
- `F` on mover — follow mode works in both views
- Zoom/pan work as normal

**Rendering (`DrawFrontView` in rendering.c):**
- 5 depth layers drawn back-to-front, back rows dimmed (30%-100% brightness)
- Depth offset per layer = floor thickness (`size * 0.25f`) — seamless tiling on flat ground
- Floor surfaces: squished top-down sprite (gives texture at a glance)
- Solid cells: existing 8x8 sprites, tinted per material
- Water: blue rectangles scaled by water level
- Movers/items drawn **per-layer** inside the layer loop for correct depth ordering
- Movers use **fractional Y interpolation** for smooth depth transitions (no snapping)
- Tooltips disabled in front view
- Follow mode tracks mover X/Z position + updates frontViewY to mover's depth slice

**Files changed:**
- `src/game_state.h` — `frontViewMode`, `frontViewY` globals
- `src/core/input.c` — V key toggle, depth scrolling
- `src/main.c` — front view branch in render loop, follow mode, tooltip suppression
- `src/render/rendering.c` — `DrawFrontView()` (~180 lines)

### Future Work

- **Front-facing wall sprites** — current top-down wall sprites work but aren't ideal
- **Front-facing mover sprites** — characters facing you, much more characterful
- **Fire/smoke/steam overlays** — not yet ported to front view
- **Grass/plant/tree overlays** — not yet ported
- **UI/input in front view** — currently observe-only, no designating/building
- **Wall occlusion** — decide if front-row walls should hide back rooms
- **Configurable depth layer count** — currently hardcoded to 5
