# Unify Rock with Soil Types

## Problem

Currently rock (granite) is treated specially as `CELL_WALL + MAT_GRANITE`, while soil types (dirt, clay, sand, gravel, peat) are dedicated cell types. But they behave identically:

- Both block horizontal movement (via CF_SOLID)
- Both provide support from below
- Both leave floors when mined
- Both can be walked on top of

There's no functional reason for rock to be different.

## Current State

**Soil types:**
```c
CELL_DIRT, CELL_CLAY, CELL_SAND, CELL_GRAVEL, CELL_PEAT
Flags: CF_GROUND | CF_BLOCKS_FLUIDS
Where: CF_GROUND = CF_WALKABLE | CF_SOLID
```

**Rock:**
```c
CELL_WALL with MAT_GRANITE material
Flags: CF_WALL
Where: CF_WALL = CF_BLOCKS_MOVEMENT | CF_BLOCKS_FLUIDS | CF_SOLID
```

But in practice, both block movement via the `CF_SOLID` check in `IsCellWalkableAt()`.

## Proposed Solution

### Option A: Add CELL_ROCK/CELL_GRANITE as a proper soil type

```c
// In grid.h, add to the soil types section:
CELL_ROCK,         // Rock/stone (was CELL_WALL + MAT_GRANITE)

// In cell_defs.c:
[CELL_ROCK] = {"rock", SPRITE_rock, CF_GROUND | CF_BLOCKS_FLUIDS, 
               INSULATION_TIER_STONE, 0, CELL_ROCK, ITEM_ROCK, 1}
```

Benefits:
- Consistent with other soil types
- Simpler to understand
- Rock is just "another ground type"

### Option B: Keep CELL_WALL but make it semantically a ground type

Change CELL_WALL flags from `CF_WALL` to `CF_GROUND`:

```c
[CELL_WALL] = {"wall", SPRITE_wall, CF_GROUND | CF_BLOCKS_FLUIDS, ...}
```

Benefits:
- Less code change
- CELL_WALL name still makes sense for constructed walls

### Option C: Make all soil types use CELL_WALL + materials

Convert dirt, clay, sand, gravel, peat to use `CELL_WALL` with different materials:
- CELL_WALL + MAT_DIRT
- CELL_WALL + MAT_CLAY
- etc.

Benefits:
- Single cell type for all natural terrain
- Material system handles variety

Drawbacks:
- Loses cell type granularity
- Need to add materials for clay, sand, gravel, peat

## Recommendation

**Option A** - Add CELL_ROCK as a proper soil type.

This is the cleanest approach that matches the existing pattern. Rock becomes just another type of natural ground, alongside dirt, clay, sand, etc.

## Implementation Steps

1. Add `CELL_ROCK` to CellType enum in grid.h
2. Add cell definition in cell_defs.c with CF_GROUND flags
3. Update `IsGroundCell()` helper to include CELL_ROCK
4. Replace ExecuteBuildRock() to use new CELL_ROCK
5. Update terrain generation that uses CELL_WALL for natural rock
6. Search for uses of `CELL_WALL + natural flag` and consider if they should be CELL_ROCK
7. Test that rock behaves identically (walk on top, mine to floor, etc.)
8. Update draw mode: "rocK" button creates CELL_ROCK instead of CELL_WALL

## Open Questions

- Should CELL_WALL still exist for constructed walls (wood, stone blocks)?
  - Or do we want "constructed rock wall" vs "natural rock"?
- Do we want different rock types (granite, marble, limestone) as separate cell types?
  - Or keep them as materials on CELL_ROCK?
- What about rendering - does rock need special sprites vs dirt?

## Benefits

- Conceptually clearer: rock is just dense/hard soil
- Consistent draw mode behavior
- Simpler mental model
- No special cases in code

## Risks

- Large refactor touching terrain generation
- Might break saves (CELL_WALL → CELL_ROCK transition)
- Need to audit all CELL_WALL usage to determine natural vs constructed

## Related

- Material system overhaul (if we want granite/marble/limestone variety)
- Terrain generation (uses CELL_WALL for mountains currently)
- Mining/channeling (already handles both soil and walls)
