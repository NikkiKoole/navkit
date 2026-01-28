# Task: Walkability From Below (Dwarf Fortress Model)

## Overview

Change the walkability model from "cell has walkable flag" to "cell is walkable if there's a solid block below it". This is how Dwarf Fortress handles terrain - you walk ON blocks, not IN them.

## Current Model

```
z=1:  [AIR]  [AIR]  [WALL]
z=0:  [GRASS][GRASS][GRASS]  ← entities walk HERE, cell has CF_WALKABLE flag
```

- `CELL_GRASS`, `CELL_FLOOR`, `CELL_DIRT` have `CF_WALKABLE` flag
- Pathfinding checks `CellIsWalkable(grid[z][y][x])`
- Entity at z=0 is standing "in" the grass cell

## Proposed Model

```
z=2:  [AIR]  [AIR]  [AIR]
z=1:  [AIR]  [AIR]  [WALL]   ← entities walk HERE (air above solid)
z=0:  [DIRT] [DIRT] [DIRT]   ← solid ground (with grass overlay)
```

- Walkability = cell is AIR (or traversable) AND cell below is SOLID
- `CELL_DIRT`, `CELL_FLOOR` become solid blocks (like walls, but shorter)
- Entity at z=1 stands "on top of" the dirt at z=0
- Grass overlay on z=0 is visible as the floor texture from z=1

## Benefits

1. **Natural multi-level terrain**: Hills are just stacked dirt blocks
2. **Consistent physics**: Everything is "stand on solid, walk through air"
3. **Digging makes sense**: Remove a dirt block → hole appears
4. **Building makes sense**: Place a floor block → walkable surface above it
5. **Water/lava pools**: Fill a pit with liquid blocks

## Complexities

### 1. Pathfinding Rewrite
**Files:** `src/world/pathfinding.c`, `src/world/cell_defs.h`

Current:
```c
static inline bool IsCellWalkableAt(int z, int y, int x) {
    return CellIsWalkable(grid[z][y][x]);
}
```

New:
```c
static inline bool IsCellWalkableAt(int z, int y, int x) {
    if (z <= 0) return false;  // Can't walk below the world
    CellType cellHere = grid[z][y][x];
    CellType cellBelow = grid[z-1][y][x];
    
    // Can walk if: current cell is traversable AND cell below is solid
    return !CellBlocksMovement(cellHere) && CellIsSolid(cellBelow);
}
```

Need new flag: `CF_SOLID` for blocks you can stand on (dirt, floor, wall tops?)

### 2. Cell Type Reclassification
**Files:** `src/world/cell_defs.c`, `src/world/cell_defs.h`

Current flags:
- `CF_WALKABLE` - can stand in this cell
- `CF_BLOCKS_MOVEMENT` - can't pass through

New flags needed:
- `CF_SOLID` - can stand ON this cell (from above)
- `CF_BLOCKS_MOVEMENT` - can't pass through horizontally
- `CF_BLOCKS_VERTICAL` - can't pass through vertically (fall through)

Cell type changes:
| Cell | Current | New |
|------|---------|-----|
| CELL_GRASS | CF_WALKABLE | CF_SOLID |
| CELL_DIRT | CF_WALKABLE | CF_SOLID |
| CELL_FLOOR | CF_WALKABLE | CF_SOLID |
| CELL_WALL | CF_BLOCKS_MOVEMENT | CF_SOLID + CF_BLOCKS_MOVEMENT |
| CELL_AIR | (none) | (none) - traversable |
| CELL_LADDER | CF_WALKABLE + CF_LADDER | CF_LADDER (special case) |

### 3. Terrain Generators
**Files:** `src/world/terrain.c`

All generators need updating:
- Instead of filling z=0 with `CELL_GRASS`
- Fill z=0 with `CELL_DIRT` (solid ground)
- Fill z=1+ with `CELL_AIR` (where entities walk)
- Walls extend from z=0 upward

Example change for flat terrain:
```c
// Old
for (y...) for (x...) {
    grid[0][y][x] = CELL_GRASS;  // walkable at z=0
}

// New
for (y...) for (x...) {
    grid[0][y][x] = CELL_DIRT;   // solid ground
    SET_CELL_SURFACE(x, y, 0, SURFACE_TALL_GRASS);
    grid[1][y][x] = CELL_AIR;    // walkable space above
}
```

### 4. Rendering Adjustments
**Files:** `src/render/rendering.c`

Current: Draw cell at z, then overlays
New: When at z, the "floor" visual comes from z-1

The existing "draw layer below with transparency" partially does this, but may need adjustment:
- Floor texture should come from z-1 (dirt + grass overlay)
- Current z shows walls, entities, items
- May want to show z-1 at full opacity as "floor", z at full opacity for walls

### 5. Entity Spawning
**Files:** `src/entities/mover.c`, anywhere entities are created

Entities currently spawn at z=0. Would need to spawn at z=1 (or wherever there's air above solid).

### 6. Falling Logic
**Files:** `src/entities/mover.c`

Current falling checks for walkable cell below. New model:
- Fall if current cell is air AND cell below is also air (no solid to land on)
- Land when cell below becomes solid

### 7. Ladder Handling
**Files:** `src/world/pathfinding.c`, `src/world/grid.c`

Ladders are special - they provide walkability without solid below:
- Ladder at z allows standing at z (even if z-1 is air)
- Ladder connects z to z+1 or z-1

### 8. Water/Fluid Interaction

Water currently fills cells. With new model:
- Water at z=1 means standing in water (on solid at z=0)
- Deep water = water at z=1 AND z=2 (over your head)
- Swimming vs wading becomes natural

### 9. Z=0 Edge Case

What's below z=0? Options:
- Implicit bedrock (always solid below z=0)
- z=0 is always solid, can't walk at z=0
- Special handling

## Files to Modify

| File | Changes |
|------|---------|
| `src/world/cell_defs.h` | Add CF_SOLID flag, update cell definitions |
| `src/world/cell_defs.c` | Update cell metadata with new flags |
| `src/world/pathfinding.c` | Rewrite walkability checks |
| `src/world/terrain.c` | Update all terrain generators |
| `src/world/grid.c` | Update PlaceLadder, EraseLadder |
| `src/entities/mover.c` | Update falling, spawning, movement |
| `src/render/rendering.c` | Adjust floor rendering |
| `src/core/input.c` | Update cell placement (walls, floors) |
| `tests/*.c` | Update all tests for new z conventions |

## Migration Strategy

1. **Phase 1**: Add `CF_SOLID` flag alongside existing flags
2. **Phase 2**: Create `IsCellWalkableAt_New()` that uses below-check
3. **Phase 3**: Update one generator as proof of concept
4. **Phase 4**: Add toggle to switch between old/new model
5. **Phase 5**: Update remaining generators
6. **Phase 6**: Remove old model, clean up

## Open Questions

1. Should wall tops be walkable? (Can you walk on top of a wall?)
2. How tall are walls? (1 block? 2 blocks? Variable?)
3. What about ramps/stairs? (Diagonal movement between z-levels)
4. Should the camera/view default to z=1 instead of z=0?
5. How does this affect the HPA* chunk/entrance system?

## Estimated Complexity

**High** - This touches nearly every system:
- Pathfinding (core algorithm change)
- All terrain generators  
- Entity movement and spawning
- Rendering perspective
- Many tests

Recommend treating as a major refactor with careful incremental changes.
