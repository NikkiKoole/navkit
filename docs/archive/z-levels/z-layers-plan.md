# Z-Layers Plan: Multi-Floor Pathfinding

## Overview
Add discrete Z-layers (floors) to the pathfinding system, Dwarf Fortress style. Each floor is a 2D grid, connected by vertical transitions (stairs, ramps, ladders). Walking entities only - no free 3D/flying movement.

## Key Design Decisions
- **Discrete floors**: 1 Z-layer = 1 building floor (not fine-grained 80cm cells)
- **Vertical connections as HPA* entrances**: Stairs/ramps become edges in the abstract graph connecting floors
- **Stairs have horizontal span**: A stair connects `(x1, y1, z)` to `(x2, y2, z+1)` where x2/y2 differs from x1/y1

---

## Data Structure Changes

### Grid (`grid.h/c`)
```c
// Current: CellType grid[MAX_GRID_HEIGHT][MAX_GRID_WIDTH]
// New:     CellType grid[MAX_FLOORS][MAX_GRID_HEIGHT][MAX_GRID_WIDTH]

#define MAX_FLOORS 16  // Configurable, start small

int floorCount;  // Runtime number of active floors
```

### 3D Point
```c
// Current Point is 2D (x, y)
// Add Point3D for z-aware coordinates
typedef struct {
    int x, y, z;
} Point3D;
```

### Vertical Connections (`pathfinding.h/c`)
```c
typedef enum {
    VCONNECT_STAIR,
    VCONNECT_RAMP,
    VCONNECT_LADDER,
    VCONNECT_ELEVATOR
} VerticalConnectionType;

typedef struct {
    Point3D from;  // Entry point (lower floor)
    Point3D to;    // Exit point (upper floor)
    VerticalConnectionType type;
    int cost;      // Travel cost (distance + type modifier)
    bool bidirectional;  // Most stairs are, some ladders might be one-way
} VerticalConnection;

VerticalConnection verticalConnections[MAX_VERTICAL_CONNECTIONS];
int verticalConnectionCount;
```

### Movers (`mover.h/c`)
```c
typedef struct {
    float x, y;
    int z;           // Current floor (integer, not interpolated)
    Point3D goal;    // Goal now includes floor
    Point3D path[MAX_MOVER_PATH];  // Path waypoints are 3D
    // ... rest unchanged
} Mover;
```

---

## Pathfinding Changes

### HPA* Extension

**Per-floor chunks:**
- Each floor has its own set of chunks
- Horizontal entrances work as before, but scoped to a floor
- Entrance struct gains a `z` field

**Vertical entrances:**
- Vertical connections become special entrances in the abstract graph
- A stair creates TWO abstract graph nodes (one per floor) connected by an edge
- Cost includes horizontal distance + vertical transition penalty

**Abstract graph search:**
- Nodes: `(entrance_id, floor)` pairs + vertical connection endpoints
- Edges: horizontal (within floor) + vertical (between floors)
- Heuristic: 3D Manhattan distance (dx + dy + dz * floor_height_cost)

### Path Refinement
- When abstract path crosses floors, insert the vertical connection waypoints
- Local A* stays 2D (within a single floor)
- Mover follows path, changes `z` when reaching a vertical transition

---

## Demo Changes (`demo.c`)

### Floor Selection
```c
int currentViewFloor = 0;  // Which floor is displayed

// UI: Floor selector
// - Up/Down buttons or number keys
// - Display "Floor: 2 / 5" indicator
```

### Rendering Modes
1. **Single floor view** (default): Show only `currentViewFloor`
2. **Ghost layers** (optional): Faint overlay of floor above/below
3. **Side-by-side** (optional): Split view showing 2-3 floors

### Vertical Connection Visualization
- Stair entries: Special icon/color on the cell
- Stair exits: Corresponding icon on upper floor
- Lines connecting them when both floors visible

### New UI Controls
- Floor navigation (up/down)
- Tool: Place vertical connection (select type, place entry, place exit)
- Show which floor movers are on (color code or filter)

---

## Implementation Phases

### Phase 1: Multi-floor Grid
- [ ] Extend grid to 3D array `grid[z][y][x]`
- [ ] Add `floorCount` initialization
- [ ] Update `InitGridFromAscii()` to accept floor parameter or multi-floor format
- [ ] Update `IsWalkable()` to take z parameter
- [ ] Update terrain generators to work per-floor

### Phase 2: Vertical Connections Data
- [ ] Add `VerticalConnection` struct and storage
- [ ] Add functions: `AddVerticalConnection()`, `RemoveVerticalConnection()`
- [ ] Add `GetConnectionsAtCell(x, y, z)` lookup

### Phase 3: Pathfinding Integration
- [ ] Extend `Point` to `Point3D` where needed (or add parallel structs)
- [ ] Update entrance detection to be per-floor
- [ ] Add vertical connections as abstract graph edges
- [ ] Update `RunHPAStar()` to handle 3D start/goal
- [ ] Update heuristic for 3D distance
- [ ] Update path refinement to handle floor transitions

### Phase 4: Mover Updates
- [ ] Add `z` field to Mover
- [ ] Update `UpdateMovers()` to handle floor changes at vertical connections
- [ ] Update `MoverNeedsRepath()` to consider floor

### Phase 5: Demo Visualization
- [ ] Add floor selector UI
- [ ] Update `DrawCellGrid()` to draw `currentViewFloor`
- [ ] Add vertical connection icons
- [ ] Update mover rendering to show floor (color/filter)
- [ ] Add tool for placing stairs/ramps

### Phase 6: Testing & Polish
- [ ] Create multi-floor test map
- [ ] Test pathfinding across floors
- [ ] Test mover behavior at stairs
- [ ] Performance testing with multiple floors

---

## Files to Modify

| File | Changes |
|------|---------|
| `grid.h/c` | 3D grid array, floor count, per-floor init |
| `pathfinding.h/c` | VerticalConnection, 3D entrances, extended HPA* |
| `mover.h/c` | z coordinate, 3D path, floor transitions |
| `demo.c` | Floor selector, vertical connection tools, rendering |
| `terrain.h/c` | Per-floor generation (optional) |

---

## Design Decisions (Confirmed)

1. **Chunk boundaries across floors**: Aligned - same chunk grid on every floor
2. **Stair footprint**: Intermediate cells are **blocked** (walls) - stairs take up physical space
3. **Elevator behavior**: Defer to later - not in initial implementation

---

## Verification

To test the implementation:
1. Create a 2-floor test map with stairs
2. Spawn a mover on floor 0, set goal on floor 1
3. Verify path includes stair waypoints
4. Verify mover walks to stair, transitions to floor 1, reaches goal
5. Test reverse direction (floor 1 → floor 0)
6. Test with multiple floors and multiple stairs
