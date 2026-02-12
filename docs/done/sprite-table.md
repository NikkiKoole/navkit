# Sprite Lookup: Rules + Overrides

## Decision: Rules + Overrides (not flat table, no new flags)

A 3-step cascade for sprite resolution:

1. Check an **overrides table** for explicit (cell, material) entries
2. Try the material's canonical sprite
3. Fall back to the cell's default sprite

No new flags needed. The cascade just tries each step and falls through
naturally. Materials like MAT_NONE or MAT_BRICK (with sprite=0) fall through
to the cell default automatically.

```c
int GetSpriteForCellMat(CellType cell, MaterialType mat) {
    // 1. Explicit override?
    int sprite = spriteOverrides[cell][mat];
    if (sprite) return sprite;

    // 2. Try material's canonical look
    if (mat != MAT_NONE) {
        sprite = MaterialSprite(mat);
        if (sprite) return sprite;
    }

    // 3. Cell default
    return CellSprite(cell);
}
```

This scales to 80+ cell types. Most new cell types need zero configuration --
they automatically look like their material. Only the exceptions (where the
shape demands a different sprite) need explicit override entries.

## The Core Insight

The sprite for a cell is not a property of the material alone, nor of the cell
type alone. It's a property of the **combination**:

```
CELL_TREE_TRUNK  + MAT_OAK     = SPRITE_tree_trunk_oak    (oak's canonical -- step 2)
CELL_TREE_LEAVES + MAT_OAK     = SPRITE_tree_leaves_oak   (NOT oak's canonical -- step 1 override)
CELL_SAPLING     + MAT_OAK     = SPRITE_tree_sapling_oak  (NOT oak's canonical -- step 1 override)
CELL_TERRAIN     + MAT_DIRT    = SPRITE_dirt               (dirt's canonical -- step 2)
CELL_WALL (built)+ MAT_OAK     = SPRITE_wall_wood         (NOT oak's canonical -- step 1 override)
CELL_WALL (built)+ MAT_GRANITE = SPRITE_rock              (granite's canonical -- step 2)
CELL_LADDER      + anything    = SPRITE_ladder             (no material sprite -- step 3)
```

Right now this knowledge is scattered across:
- `MaterialDef.sprite` (canonical/trunk sprite)
- `MaterialDef.leavesSprite` (only 4 wood types use it)
- `MaterialDef.saplingSprite` (only 4 wood types use it)
- `CellDef.sprite` (fallback defaults)
- `MaterialWallSprite()` in rendering.c (hardcoded wood override)
- `MaterialFloorSprite()` in rendering.c (hardcoded wood override)
- `GetTreeSpriteAt()` in rendering.c (switch on cell type)
- `GetWallSpriteAt()` in rendering.c (primary rendering dispatcher)
- `GetCellSpriteAt()` in material.c (public API, subtly different)
- `ItemSpriteForTypeMaterial()` in rendering.c (hardcoded ITEM_LOG switch)

That's **6 functions + 3 struct fields** encoding the same conceptual thing.

## The Three Patterns

Every (cell, material) -> sprite mapping falls into one of three patterns:

### Pattern 1: Material IS the sprite (step 2 handles it, no entry needed)

The material's canonical sprite is used directly. Examples:

```
CELL_TERRAIN     + MAT_DIRT    = SPRITE_dirt     (dirt's canonical)
CELL_TERRAIN     + MAT_CLAY    = SPRITE_clay     (clay's canonical)
CELL_WALL        + MAT_GRANITE = SPRITE_rock     (granite's canonical)
CELL_TREE_TRUNK  + MAT_OAK    = SPRITE_tree_trunk_oak  (oak's canonical)
```

Adding MAT_MARBLE with canonical SPRITE_marble makes every cell type
automatically pick it up. Adding CELL_PILLAR needs zero entries -- it just
looks like whatever material it's made of.

### Pattern 2: Shape overrides material (explicit override entry)

The (cell, material) pair has a specific sprite that is **neither** the
material's canonical look **nor** the cell's default:

| Cell | Material | Override sprite | Why not canonical? |
|------|----------|----------------|--------------------|
| CELL_WALL | MAT_OAK | SPRITE_wall_wood | wall shape, not trunk |
| CELL_WALL | MAT_PINE | SPRITE_wall_wood | wall shape, not trunk |
| CELL_WALL | MAT_BIRCH | SPRITE_wall_wood | wall shape, not trunk |
| CELL_WALL | MAT_WILLOW | SPRITE_wall_wood | wall shape, not trunk |
| CELL_TREE_LEAVES | MAT_OAK | SPRITE_tree_leaves_oak | leaves, not trunk |
| CELL_TREE_LEAVES | MAT_PINE | SPRITE_tree_leaves_pine | leaves, not trunk |
| CELL_TREE_LEAVES | MAT_BIRCH | SPRITE_tree_leaves_birch | leaves, not trunk |
| CELL_TREE_LEAVES | MAT_WILLOW | SPRITE_tree_leaves_willow | leaves, not trunk |
| CELL_SAPLING | MAT_OAK | SPRITE_tree_sapling_oak | sapling, not trunk |
| CELL_SAPLING | MAT_PINE | SPRITE_tree_sapling_pine | sapling, not trunk |
| CELL_SAPLING | MAT_BIRCH | SPRITE_tree_sapling_birch | sapling, not trunk |
| CELL_SAPLING | MAT_WILLOW | SPRITE_tree_sapling_willow | sapling, not trunk |

**12 overrides total.** At 80 cell types, expect maybe 30-50.

### Pattern 3: Material-independent (step 3, cell default)

Cell always looks the same. Material is MAT_NONE, or material's canonical
sprite is 0, so step 2 falls through to step 3. Examples:

```
CELL_AIR         = SPRITE_air           (no material)
CELL_LADDER_UP   = SPRITE_ladder_up     (no material)
CELL_RAMP_N      = SPRITE_ramp_n        (no material)
CELL_WALL        + MAT_BRICK = SPRITE_wall  (brick has sprite=0, falls to cell default)
```

## The Implementation

```c
// Sparse overrides table (only the exceptions, most entries are 0)
static int spriteOverrides[CELL_TYPE_COUNT][MAT_COUNT];

void InitSpriteOverrides(void) {
    memset(spriteOverrides, 0, sizeof(spriteOverrides));

    // Wood walls look different from wood's canonical (trunk) sprite
    spriteOverrides[CELL_WALL][MAT_OAK]    = SPRITE_wall_wood;
    spriteOverrides[CELL_WALL][MAT_PINE]   = SPRITE_wall_wood;
    spriteOverrides[CELL_WALL][MAT_BIRCH]  = SPRITE_wall_wood;
    spriteOverrides[CELL_WALL][MAT_WILLOW] = SPRITE_wall_wood;

    // Leaves look different from wood's canonical (trunk) sprite
    spriteOverrides[CELL_TREE_LEAVES][MAT_OAK]    = SPRITE_tree_leaves_oak;
    spriteOverrides[CELL_TREE_LEAVES][MAT_PINE]   = SPRITE_tree_leaves_pine;
    spriteOverrides[CELL_TREE_LEAVES][MAT_BIRCH]  = SPRITE_tree_leaves_birch;
    spriteOverrides[CELL_TREE_LEAVES][MAT_WILLOW] = SPRITE_tree_leaves_willow;

    // Saplings look different from wood's canonical (trunk) sprite
    spriteOverrides[CELL_SAPLING][MAT_OAK]    = SPRITE_tree_sapling_oak;
    spriteOverrides[CELL_SAPLING][MAT_PINE]   = SPRITE_tree_sapling_pine;
    spriteOverrides[CELL_SAPLING][MAT_BIRCH]  = SPRITE_tree_sapling_birch;
    spriteOverrides[CELL_SAPLING][MAT_WILLOW] = SPRITE_tree_sapling_willow;
}

int GetSpriteForCellMat(CellType cell, MaterialType mat) {
    int sprite = spriteOverrides[cell][mat];
    if (sprite) return sprite;

    if (mat != MAT_NONE) {
        sprite = MaterialSprite(mat);
        if (sprite) return sprite;
    }

    return CellSprite(cell);
}
```

No changes needed to CellDef flags. No new flag. The cascade handles
everything through fallthrough behavior.

## What Gets Removed

### Functions eliminated
- `MaterialWallSprite()` in rendering.c
- `GetTreeSpriteAt()` in rendering.c
- `GetWallSpriteAt()` in rendering.c
- `GetCellSpriteAt()` in material.c

All replaced by `GetSpriteForCellMat()` + natural wall check where needed.

### MaterialDef fields eliminated
- `leavesSprite` -- moved to overrides table
- `saplingSprite` -- moved to overrides table

### MaterialDef fields kept
- `sprite` -- stays as the "canonical look" used in step 2 of the cascade

### Macros eliminated
- `MaterialLeavesSprite(m)`
- `MaterialSaplingSprite(m)`

### CellDef.sprite stays
As the fallback default (step 3). Useful for material-independent cells and
as the "what does this look like when material is unknown" answer.

## Current Bug This Fixes

`GetWallSpriteAt()` (rendering.c) and `GetCellSpriteAt()` (material.c)
disagree for constructed wood walls:

- `GetWallSpriteAt()` -> `MaterialWallSprite(MAT_OAK)` -> `SPRITE_wall_wood`
- `GetCellSpriteAt()` -> `MaterialSprite(MAT_OAK)` -> `SPRITE_tree_trunk_oak`

One table, one function, one answer. Bug gone.

## Decisions Made

### No CF_MATERIAL_SPRITE flag
Originally considered adding a flag to CellDef to mark "this cell cares about
material." Dropped it. The cascade just always tries the material -- if it
returns 0 or mat is MAT_NONE, it falls through naturally. No flag needed.
Simpler, and future cell types that want material sprites need zero config.

### Floors: keep MaterialFloorSprite() as-is
Floors are a flag (`HAS_FLOOR`), not a cell type. Only 2 overrides (wood, stone).
3 lines of code. Not worth adding to the table. Leave it.

### Natural walls: caller responsibility
Natural walls are always `SPRITE_rock`. "Natural" is a position property, not
a (cell, material) property. The caller checks `IsWallNatural()` before the
table lookup.

### Items: separate concern
`ItemSpriteForTypeMaterial()` maps items, not cells. Leave it. Could do an
item sprite table later if item types * materials grows.

## CellDef vs MaterialDef: Where Fields Belong

| Field            | CellDef          | MaterialDef        | Why both? |
|------------------|------------------|--------------------|-----------|
| name             | shape name       | substance name     | Different things ("wall" vs "Oak") |
| sprite           | fallback default | canonical look     | Sprite table replaces both for rendering |
| flags            | physics (CF_*)   | properties (MF_*)  | Different concerns |
| fuel             | yes              | yes                | CellDef = shape default (0 for rock), MaterialDef = substance (128 for oak) |
| insulationTier   | yes              | yes                | Same pattern as fuel |
| dropsItem        | yes              | yes                | Natural wall drops by shape (ITEM_ROCK), constructed drops by material (ITEM_LOG) |
| dropCount        | yes              | no                 | Shape determines quantity (trunk > branch) |
| burnsInto        | CellType         | MaterialType       | Shape -> shape, substance -> substance |
| ignitionRes      | no               | yes                | Pure substance property |

The "check material first, fall back to cell" pattern in the gameplay code
(fire, drops, insulation) correctly models reality. Natural rock uses CellDef
defaults (0 fuel, ITEM_ROCK drops). Constructed oak walls use MaterialDef
values (128 fuel, ITEM_LOG drops). Both are needed.

## Refactor Phases

### Phase 1: Sprite overrides (visual-only, zero gameplay risk)

Safest to do first because:
- Purely additive (add new function alongside existing code)
- Visual-only (no gameplay logic depends on sprites)
- Wrong sprite = immediately visible, not silent corruption
- Migrate callers one at a time, test after each

```
1a. Add spriteOverrides table + InitSpriteOverrides() with 12 entries
1b. Add GetSpriteForCellMat(cell, mat)
1c. Replace GetWallSpriteAt() to use GetSpriteForCellMat + natural wall check
1d. Delete GetTreeSpriteAt() -- GetSpriteForCellMat handles it
1e. Replace GetCellSpriteAt() to use GetSpriteForCellMat + natural wall check
1f. Remove MaterialWallSprite()
1g. Update tests
```

### Phase 2: Clean MaterialDef sprite fields

```
2a. Remove leavesSprite, saplingSprite from MaterialDef
2b. Remove MaterialLeavesSprite(), MaterialSaplingSprite() macros
2c. Update materialDefs table (drop two columns)
2d. Update tests
```

Each phase is independently shippable and testable.

## Memory Cost

Overrides table: `16 * 15 * 4 = 960 bytes` (mostly zeros)
At scale (80 * 30): `9,600 bytes` (still mostly zeros, still negligible)
