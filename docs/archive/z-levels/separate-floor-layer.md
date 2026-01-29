# Separate Floor Layer Implementation Plan

## Overview

Add a separate floor layer to match DF's model where each cell has both a "wall-part" (what fills the space) and a "floor-part" (what you stand on).

This enables:
- Balconies (floor over empty space)
- Bridges (floor spanning a gap)
- Catwalks
- Mining leaving floors behind

## Current State

```c
CellType grid[z][y][x];      // What fills the cell: AIR, WALL, DIRT, etc.
uint8_t cellFlags[z][y][x];  // Per-cell flags: burned, wetness, surface overlay
```

`cellFlags` bits currently used:
- Bit 0: CELL_FLAG_BURNED
- Bits 1-2: wetness (0-3)
- Bits 3-4: surface overlay (0-3)
- Bits 5-7: reserved

## Design

### Option A: Use cellFlags bit 5 for floor

```c
#define CELL_FLAG_HAS_FLOOR (1 << 5)  // Cell has a constructed floor

#define HAS_FLOOR(x,y,z)    (cellFlags[z][y][x] & CELL_FLAG_HAS_FLOOR)
#define SET_FLOOR(x,y,z)    (cellFlags[z][y][x] |= CELL_FLAG_HAS_FLOOR)
#define CLEAR_FLOOR(x,y,z)  (cellFlags[z][y][x] &= ~CELL_FLAG_HAS_FLOOR)
```

Pros: No new arrays, minimal memory impact
Cons: Only one floor type (no wood vs stone floors)

### Option B: Separate floor type array

```c
typedef enum {
    FLOOR_NONE = 0,
    FLOOR_WOOD,
    FLOOR_STONE,
    FLOOR_METAL
} FloorType;

FloorType floors[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
```

Pros: Multiple floor types, cleaner separation
Cons: More memory (1 byte per cell)

### Recommendation: Option A for now

Start with a single bit. Can expand to Option B later if needed for floor materials.

---

## Implementation Phases

### Phase 1: Add Floor Flag (No Behavior Change)

**Files**: `src/world/grid.h`

```c
// Add to cellFlags bit definitions
#define CELL_FLAG_HAS_FLOOR (1 << 5)  // Cell has a floor (can stand on from above in DF mode)

// Add helper macros
#define HAS_FLOOR(x,y,z)    (cellFlags[z][y][x] & CELL_FLAG_HAS_FLOOR)
#define SET_FLOOR(x,y,z)    (cellFlags[z][y][x] |= CELL_FLAG_HAS_FLOOR)
#define CLEAR_FLOOR(x,y,z)  (cellFlags[z][y][x] &= ~CELL_FLAG_HAS_FLOOR)
```

**Verification**: All tests pass, no behavior change

**Checkpoint**: Commit "Add HAS_FLOOR flag to cellFlags"

---

### Phase 2: Update DF Walkability to Check Floor Flag

**Files**: `src/world/cell_defs.h`

Update `IsCellWalkableAt_DFStyle`:

```c
static inline bool IsCellWalkableAt_DFStyle(int z, int y, int x) {
    if (z < 0 || z >= gridDepth || y < 0 || y >= gridHeight || x < 0 || x >= gridWidth) 
        return false;
    
    CellType cellHere = grid[z][y][x];
    
    // Can't walk through solid blocks
    if (CellBlocksMovement(cellHere)) return false;
    
    // Ladders are always walkable
    if (CellIsLadder(cellHere)) return true;
    
    // Ramps are always walkable  
    if (CellIsRamp(cellHere)) return true;
    
    // NEW: This cell has a floor - walkable (standing on this cell's floor)
    if (HAS_FLOOR(x, y, z)) return true;
    
    // Z=0: not walkable (no z=-1 to stand on) unless has floor
    if (z == 0) return false;
    
    // DF-style: walkable if cell below is solid
    CellType cellBelow = grid[z-1][y][x];
    
    // Cells above ladders only walkable if also a ladder
    if (CellIsLadder(cellBelow) && !CellIsLadder(cellHere)) return false;
    
    return CellIsSolid(cellBelow);
}
```

**Verification**: 
- Cells with HAS_FLOOR are walkable even with AIR below
- Existing behavior unchanged (no floors set yet)

**Checkpoint**: Commit "Update DF walkability to check HAS_FLOOR"

---

### Phase 3: Add Floor Construction

**Files**: `src/core/input.c` (or wherever building happens)

Add ability to place floors:
- New build option or extend existing floor placement
- When placing floor in DF mode: set HAS_FLOOR flag on the cell (not CELL_FLOOR type)

```c
// When player builds a floor at (x, y, z):
if (g_legacyWalkability) {
    // DF mode: set floor flag, keep cell as AIR
    grid[z][y][x] = CELL_AIR;
    SET_FLOOR(x, y, z);
} else {
    // Legacy mode: use CELL_FLOOR type
    grid[z][y][x] = CELL_FLOOR;
}
```

**Verification**: Can build floors over empty space, movers walk on them

**Checkpoint**: Commit "Add floor construction in DF mode"

---

### Phase 4: Update Rendering

**Files**: `src/render/rendering.c`

Draw floors as a separate layer:

```c
// When rendering cell at z:
if (HAS_FLOOR(x, y, z)) {
    // Draw floor sprite (use existing floor texture)
    DrawFloorAt(x, y, z);
}
```

**Verification**: Constructed floors are visible

**Checkpoint**: Commit "Render HAS_FLOOR cells"

---

### Phase 5: Migrate CELL_FLOOR Usage

**Files**: Various

Decide what to do with existing CELL_FLOOR:
1. Keep for legacy mode only
2. Convert CELL_FLOOR cells to AIR + HAS_FLOOR on load/terrain gen
3. Remove CELL_FLOOR entirely

Recommended: Option 2 - when terrain generates or loads, convert:
```c
if (grid[z][y][x] == CELL_FLOOR) {
    grid[z][y][x] = CELL_AIR;
    SET_FLOOR(x, y, z);
}
```

**Checkpoint**: Commit "Migrate CELL_FLOOR to HAS_FLOOR"

---

### Phase 6: Update Mining/Digging (Future)

When mining solid rock:
- Remove the solid cell (becomes AIR)
- Optionally leave a floor behind (like DF)

```c
void MineCell(int x, int y, int z) {
    // Was solid, now air
    grid[z][y][x] = CELL_AIR;
    // Leave a floor behind
    SET_FLOOR(x, y, z);
    // Mark pathfinding dirty
    MarkChunkDirty(x, y, z);
}
```

**Checkpoint**: Commit "Mining leaves floors behind"

---

## Data Model Summary

### Before (current)
```
z=2:  CELL_FLOOR        ← walkable in legacy, not in DF (problem!)
z=1:  CELL_AIR
z=0:  CELL_DIRT
```

### After (with floor layer)
```
z=2:  CELL_AIR + HAS_FLOOR   ← walkable in DF (standing on this cell's floor)
z=1:  CELL_AIR
z=0:  CELL_DIRT
```

### Walkability Logic

| Cell Type | Has Floor? | Below Solid? | Walkable (DF)? |
|-----------|------------|--------------|----------------|
| AIR       | No         | No           | No             |
| AIR       | No         | Yes          | Yes            |
| AIR       | Yes        | No           | Yes (balcony!) |
| AIR       | Yes        | Yes          | Yes            |
| WALL      | -          | -            | No (blocks)    |
| LADDER    | -          | -            | Yes (special)  |

---

## Files to Modify

| Phase | File | Changes |
|-------|------|---------|
| 1 | `src/world/grid.h` | Add CELL_FLAG_HAS_FLOOR and macros |
| 2 | `src/world/cell_defs.h` | Update IsCellWalkableAt_DFStyle |
| 3 | `src/core/input.c` | Floor construction logic |
| 4 | `src/render/rendering.c` | Draw floors |
| 5 | `src/world/terrain.c` | Convert CELL_FLOOR in generators |
| 5 | `src/world/grid.c` | Convert on load if needed |
| 6 | Mining code | Leave floors when mining |

---

## Testing Scenarios

1. **Flat terrain**: Works as before (standing on solid dirt)
2. **Walls with no floor flag**: Wall tops not walkable (unless they have a floor)
3. **Balcony**: AIR + HAS_FLOOR over empty space - walkable
4. **Bridge**: Row of AIR + HAS_FLOOR spanning a gap - walkable
5. **Multi-story**: Each floor level has HAS_FLOOR set

---

## Open Questions

1. **Wall tops**: Should walls automatically have an implicit floor on top? Or require explicit floor construction?
   - Option A: Walls get implicit floor (current behavior preserved)
   - Option B: Walls need explicit floor to walk on (more DF-like)

2. **Floor removal**: How to remove a floor? Channeling/digging down?

3. **Floor materials**: Do we need different floor types (wood, stone) now or later?

4. **Rendering**: Does existing floor sprite work, or need adjustment for DF mode?
