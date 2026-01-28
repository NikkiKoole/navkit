# Walkability From Below: Incremental Implementation Plan

This plan provides safe checkpoints where the system remains fully functional and testable at each step.

## Design Goals

1. **Incremental migration** - Each phase leaves the system working
2. **Keep pathfinding generic** - Pathfinding code stays unaware of DF-style rules
3. **Toggle between models** - Can switch old/new during development
4. **Testable at each step** - All existing tests pass until intentionally changed

## Architecture Insight

The current system is well-designed for this change:

```
Pathfinding → IsCellWalkableAt(z,y,x) → CellIsWalkable(cell) → cellDefs[].flags
```

Pathfinding only calls `IsCellWalkableAt()` - it has no knowledge of terrain rules. We can change walkability logic without touching pathfinding code.

---

## Phase 1: Add CF_SOLID Flag (No Behavior Change)

**Goal**: Add the new flag infrastructure without changing any behavior.

**Files to modify**:
- `src/world/cell_defs.h`
- `src/world/cell_defs.c`

**Changes**:

1. Add new flag in `cell_defs.h`:
```c
#define CF_SOLID (1 << 5)  // Can stand ON this cell (from above)
```

2. Add helper macro:
```c
#define CellIsSolid(c) CellHasFlag(c, CF_SOLID)
```

3. Update cell definitions in `cell_defs.c` to include CF_SOLID:
   - CELL_GRASS: add CF_SOLID
   - CELL_DIRT: add CF_SOLID  
   - CELL_FLOOR: add CF_SOLID
   - CELL_WALL: add CF_SOLID
   - CELL_WATER: no CF_SOLID (can't stand on water)
   - CELL_AIR: no CF_SOLID

**Verification**:
- All existing tests pass
- Game plays exactly as before
- `CellIsSolid()` returns expected values

**✅ CHECKPOINT: Commit "Add CF_SOLID flag to cell definitions"**

---

## Phase 2: Create Alternate Walkability Function

**Goal**: Implement DF-style walkability logic without activating it.

**Files to modify**:
- `src/world/cell_defs.h`

**Changes**:

Add new function alongside existing one:
```c
static inline bool IsCellWalkableAt_DFStyle(int z, int y, int x) {
    // Bounds check
    if (z < 0 || z >= gridDepth || y < 0 || y >= gridHeight || x < 0 || x >= gridWidth) 
        return false;
    
    CellType cellHere = grid[z][y][x];
    
    // Can't walk through solid blocks
    if (CellBlocksMovement(cellHere)) return false;
    
    // Ladders are always walkable (special case)
    if (CellIsLadder(cellHere)) return true;
    
    // Z=0 edge case: treat as having implicit bedrock below
    if (z == 0) {
        // At z=0, walkable if the cell itself is walkable (old behavior)
        // This allows gradual migration
        return CellIsWalkable(cellHere);
    }
    
    // DF-style: walkable if cell below is solid
    CellType cellBelow = grid[z-1][y][x];
    return CellIsSolid(cellBelow);
}
```

**Verification**:
- All existing tests pass (function exists but unused)
- Can write new unit tests for `IsCellWalkableAt_DFStyle()`
- Game plays exactly as before

**✅ CHECKPOINT: Commit "Add IsCellWalkableAt_DFStyle function"**

---

## Phase 3: Add Toggle to Switch Models

**Goal**: Allow runtime switching between old and new walkability models.

**Files to modify**:
- `src/world/cell_defs.h`
- `src/world/cell_defs.c` (or grid.c for the global)

**Changes**:

1. Add global toggle:
```c
// In header
extern bool g_useDFWalkability;

// In source
bool g_useDFWalkability = false;  // Default to old behavior
```

2. Modify `IsCellWalkableAt()` to use toggle:
```c
static inline bool IsCellWalkableAt(int z, int y, int x) {
    if (g_useDFWalkability) {
        return IsCellWalkableAt_DFStyle(z, y, x);
    }
    
    // Original implementation
    if (z < 0 || z >= gridDepth || y < 0 || y >= gridHeight || x < 0 || x >= gridWidth) 
        return false;
    return CellIsWalkable(grid[z][y][x]);
}
```

3. Add UI toggle or key binding to flip `g_useDFWalkability` for testing.

**Verification**:
- With toggle OFF: all tests pass, game plays as before
- With toggle ON: can observe new behavior (may be broken until terrain updated)
- Toggle can be flipped at runtime

**✅ CHECKPOINT: Commit "Add toggle for DF-style walkability"**

---

## Phase 4: Create DF-Style Test Terrain Generator

**Goal**: Create ONE terrain generator that produces DF-style terrain for testing.

**Files to modify**:
- `src/world/terrain.c`

**Changes**:

Add new generator (don't modify existing ones yet):
```c
void GenerateFlatDF(void) {
    // Clear everything to air
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                grid[z][y][x] = CELL_AIR;
            }
        }
    }
    
    // z=0: solid ground (dirt)
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            grid[0][y][x] = CELL_DIRT;
            // Add grass surface overlay
            SET_CELL_SURFACE(x, y, 0, SURFACE_TALL_GRASS);
        }
    }
    
    // z=1 and above: air (walkable space)
    // Already set to CELL_AIR above
    
    // Walls: extend from z=0 through z=1
    // (add some test walls here)
}
```

**Verification**:
- Existing generators unchanged, tests pass
- New generator produces DF-style terrain
- With toggle ON + DF terrain: entities can walk at z=1
- With toggle OFF + DF terrain: entities can't walk (expected)

**✅ CHECKPOINT: Commit "Add GenerateFlatDF terrain generator"**

---

## Phase 5: Update Entity Spawning

**Goal**: Entities spawn at correct z-level based on walkability mode.

**Files to modify**:
- `src/entities/mover.c`
- Any entity spawning code

**Changes**:

1. Add helper to find walkable z-level:
```c
int FindWalkableZ(int x, int y) {
    if (g_useDFWalkability) {
        // DF-style: find lowest z where cell below is solid
        for (int z = 1; z < gridDepth; z++) {
            if (IsCellWalkableAt(z, y, x)) {
                return z;
            }
        }
        return 1;  // Default fallback
    } else {
        // Old style: find lowest walkable cell
        for (int z = 0; z < gridDepth; z++) {
            if (IsCellWalkableAt(z, y, x)) {
                return z;
            }
        }
        return 0;
    }
}
```

2. Update entity spawn code to use `FindWalkableZ()`.

**Verification**:
- Toggle OFF: entities spawn at z=0 as before
- Toggle ON + DF terrain: entities spawn at z=1
- Entities are visible and can move

**✅ CHECKPOINT: Commit "Update entity spawning for DF walkability"**

---

## Phase 6: Update Falling Logic

**Goal**: Falling works correctly with DF-style walkability.

**Files to modify**:
- `src/entities/mover.c`

**Changes**:

Update `TryFallToGround()` or equivalent:
```c
static bool TryFallToGround(Mover* m, int cellX, int cellY) {
    int currentZ = (int)m->z;
    
    for (int checkZ = currentZ - 1; checkZ >= 0; checkZ--) {
        if (IsCellWalkableAt(checkZ, cellY, cellX)) {
            // Found walkable ground
            m->z = (float)checkZ;
            m->needsRepath = true;
            m->fallTimer = 1.0f;
            return true;
        }
        
        // In DF mode, stop at solid blocks even if not walkable
        if (g_useDFWalkability && CellBlocksMovement(grid[checkZ][cellY][cellX])) {
            break;
        }
    }
    return false;
}
```

**Verification**:
- Toggle OFF: falling works as before
- Toggle ON: entities fall until they reach air-above-solid
- Digging a hole causes entity to fall

**✅ CHECKPOINT: Commit "Update falling logic for DF walkability"**

---

## Phase 7: Update Remaining Terrain Generators

**Goal**: Convert existing terrain generators to DF-style, one at a time.

**Files to modify**:
- `src/world/terrain.c`

**Approach**:
- Convert one generator
- Test thoroughly
- Commit
- Repeat

**Order** (simplest first):
1. GenerateFlat
2. GenerateRandom  
3. GenerateCastle
4. GenerateGallery
5. (etc.)

**For each generator**:
- z=0 becomes solid ground (CELL_DIRT with surface overlay)
- z=1+ becomes air where entities walk
- Walls extend through multiple z-levels
- Ladders connect z-levels

**✅ CHECKPOINT after each generator: Commit "Convert GenerateX to DF-style"**

---

## Phase 8: Update Rendering

**Goal**: Floor visuals come from z-1 in DF mode.

**Files to modify**:
- `src/render/rendering.c`

**Changes**:
- When rendering at z, draw floor texture from z-1 cell
- The existing "layer below transparency" may already handle this
- May need to adjust opacity/blending

**Verification**:
- Floor looks correct (dirt/grass visible as floor)
- Walls render properly
- Entities visible at correct z-level

**✅ CHECKPOINT: Commit "Update rendering for DF walkability"**

---

## Phase 9: Update Tests

**Goal**: Tests work with new walkability model.

**Files to modify**:
- `tests/test_pathfinding.c`
- Other test files

**Approach**:
- Add new tests specifically for DF-style walkability
- Update existing tests to work with toggle
- Consider having test suites run twice (toggle ON and OFF)

**✅ CHECKPOINT: Commit "Update tests for DF walkability"**

---

## Phase 10: Remove Old Model (Optional)

**Goal**: Clean up if old model no longer needed.

**Changes**:
- Remove `g_useDFWalkability` toggle
- Remove `CF_WALKABLE` flag if unused
- Simplify `IsCellWalkableAt()` to only DF-style
- Update all documentation

**✅ FINAL CHECKPOINT: Commit "Remove legacy walkability model"**

---

## Summary: Safe Checkpoints

| Phase | Description | System State |
|-------|-------------|--------------|
| 1 | Add CF_SOLID flag | ✅ Fully working, no behavior change |
| 2 | Add DF walkability function | ✅ Fully working, new code unused |
| 3 | Add toggle | ✅ Fully working, toggle defaults OFF |
| 4 | DF test terrain | ✅ Fully working, new terrain optional |
| 5 | Entity spawning | ✅ Fully working in both modes |
| 6 | Falling logic | ✅ Fully working in both modes |
| 7 | Convert generators | ✅ Commit after each, working |
| 8 | Rendering | ✅ Visual polish |
| 9 | Tests | ✅ Full test coverage |
| 10 | Cleanup | ✅ Final state |

Each checkpoint is a commit where:
- All tests pass
- Game is playable
- Can ship if needed
- Safe to pause work

---

## Open Questions (Decide Before Phase 7)

1. **Wall height**: Are walls 1 block or 2+ blocks tall?
2. **Wall tops**: Can entities walk on top of walls?
3. **Default view z**: Should camera default to z=1?
4. **Water**: How deep before swimming vs wading?
5. **Ramps**: How do ramps work in the new model?

---

## Future Design Ideas

### Implicit Bedrock at z=-1

Currently z=0 is never walkable in DF mode because there's no z=-1 to stand on. We could treat z=-1 as an implicit solid "bedrock" layer, making z=0 walkable by default. This would:

- Allow entities to walk at z=0 (standing on implicit bedrock)
- Make the default terrain more intuitive (fill z=0 with air, entities can walk there)
- Match how Dwarf Fortress treats the bottom layer

Implementation would be a simple change in `IsCellWalkableAt_DFStyle`:
```c
if (z == 0) {
    // Treat z=-1 as implicit solid bedrock
    return true;  // or: return !CellBlocksMovement(cellHere);
}
```

This is a game design decision - postponed for now.

---

## Files Summary

| File | Phase(s) |
|------|----------|
| `src/world/cell_defs.h` | 1, 2, 3 |
| `src/world/cell_defs.c` | 1, 3 |
| `src/world/terrain.c` | 4, 7 |
| `src/entities/mover.c` | 5, 6 |
| `src/render/rendering.c` | 8 |
| `tests/*.c` | 9 |

---

## Implementation Progress & Findings

### Completed (Phases 1-6)

**Phase 1-3**: Core infrastructure in place:
- `CF_SOLID` flag added to cell definitions
- `IsCellWalkableAt_DFStyle()` function implemented
- `g_useDFWalkability` toggle with F7 key binding
- HPA graph rebuilds when toggling (calls `BuildEntrances()` + `BuildGraph()`)

**Phase 4**: `GenerateFlatDF()` terrain generator added:
- z=0: solid dirt with grass overlay
- z=1+: air (walkable in DF mode because solid below)

**Phase 5-6**: Entity spawning and falling updated:
- Movers spawn at correct z-level based on walkability mode
- Falling distinguishes between air (fall) and walls (push out)

### Key Implementation Details

**Walkability Logic** (`cell_defs.h`):
```c
static inline bool IsCellWalkableAt_DFStyle(int z, int y, int x) {
    if (z < 0 || z >= gridDepth || ...) return false;
    CellType cellHere = grid[z][y][x];
    if (CellBlocksMovement(cellHere)) return false;
    if (CellIsLadder(cellHere)) return true;   // Ladders always walkable
    if (CellIsRamp(cellHere)) return true;     // Ramps always walkable
    if (z == 0) return false;                  // z=0 never walkable in DF mode
    CellType cellBelow = grid[z-1][y][x];
    // Cells above ladders only walkable if also a ladder (prevents unreachable goals)
    if (CellIsLadder(cellBelow) && !CellIsLadder(cellHere)) return false;
    return CellIsSolid(cellBelow);
}
```

**Ladder Climbing in DF Mode** (`pathfinding.c`):
```c
// CanClimbUp: need ladder at BOTH levels (same as legacy)
if (g_useDFWalkability) {
    bool hasLadderHere = CellIsLadder(low);
    bool hasLadderAbove = CellIsLadder(high);
    return hasLadderHere && hasLadderAbove && IsCellWalkableAt(z + 1, y, x);
}

// CanClimbDown: need ladder at BOTH levels (same as legacy)
if (g_useDFWalkability) {
    bool hasLadderHere = CellIsLadder(high);
    bool hasLadderBelow = CellIsLadder(low);
    return hasLadderHere && hasLadderBelow && IsCellWalkableAt(z - 1, y, x);
}
```

**MarkChunkDirty in DF Mode** (`pathfinding.c`):
```c
// In DF mode, changing a cell affects walkability of the cell ABOVE it
if (g_useDFWalkability && cellZ + 1 < gridDepth) {
    chunkDirty[cellZ + 1][cy][cx] = true;
}
```

**HPA Visualization Fix** (`rendering.c`):
- `DrawEntrances()` and `DrawGraph()` now filter by `currentViewZ`
- Previously showed all z-levels overlapping

### Working Features

- DF mode enabled by default
- Default view at z=1, InitGrid() fills z=0 with dirt
- Toggle between legacy and DF mode with F7
- Basic walking on flat DF terrain (z=1 on z=0 dirt)
- Pathfinding works correctly with ladders
- Movers pathfind correctly across z-levels
- Incremental graph updates work in DF mode
- All tests pass (legacy tests use `g_useDFWalkability = false`)

### Known Limitations & Future Work

**Not Yet Implemented**:
- Phase 7: Converting other terrain generators to DF-style
- Phase 8: Rendering adjustments (floor textures from z-1)
- Phase 10: Removing legacy model (keeping toggle for now)

### Testing Commands

```bash
# Run all tests
make test

# Run pathfinding tests only
./bin/test_pathing

# In-game testing
# DF mode is now enabled by default
# 1. Start game - z=0 is dirt, view starts at z=1
# 2. Spawn movers - they walk on z=1
# 3. Build ladders - movers can climb between levels
# 4. Press F7 to toggle back to legacy mode if needed
```

### Debug Tips

When debugging DF mode issues:
1. Check `IsCellWalkableAt()` returns expected values for start/goal
2. Verify `ladderLinkCount` > 0 if ladders should exist
3. Check `entranceCount` is reasonable for the terrain
4. Ensure ladders are placed at BOTH z-levels (ladder shaft, not single cell)
5. Cells directly above ladder tops are NOT walkable unless they also have a ladder
