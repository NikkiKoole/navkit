# Sprite Lookup: Rules + Overrides

## Decision: Rules + Overrides (not flat table)

We're going with a 3-step cascade for sprite resolution:

1. Check an **overrides table** for explicit (cell, material) entries
2. If cell has `CF_MATERIAL_SPRITE` flag, use the material's canonical sprite
3. Fall back to the cell's default sprite

This scales to 80+ cell types without a wall of table entries. Most new cell
types just set `CF_MATERIAL_SPRITE` and automatically look like their material.
Only the exceptions (where the shape demands a different sprite than the
material's canonical look) need explicit override entries.

## The Core Insight

The sprite for a cell is not a property of the material alone, nor of the cell
type alone. It's a property of the **combination**:

```
CELL_TREE_TRUNK  + MAT_OAK     = SPRITE_tree_trunk_oak    (oak's canonical -- rule handles it)
CELL_TREE_LEAVES + MAT_OAK     = SPRITE_tree_leaves_oak   (NOT oak's canonical -- override)
CELL_SAPLING     + MAT_OAK     = SPRITE_tree_sapling_oak  (NOT oak's canonical -- override)
CELL_TERRAIN     + MAT_DIRT    = SPRITE_dirt               (dirt's canonical -- rule handles it)
CELL_WALL (built)+ MAT_OAK     = SPRITE_wall_wood         (NOT oak's canonical -- override)
CELL_WALL (built)+ MAT_GRANITE = SPRITE_rock              (granite's canonical -- rule handles it)
CELL_LADDER      + anything    = SPRITE_ladder             (material-independent -- cell default)
```

## The Three Patterns

Every (cell, material) -> sprite mapping falls into one of three patterns:

### Pattern 1: Material IS the sprite (rule, no entry needed)

The material's canonical sprite (`MaterialSprite(mat)`) is used directly.
Cell type has `CF_MATERIAL_SPRITE` flag set. Examples:

```
CELL_TERRAIN     + MAT_DIRT    = SPRITE_dirt     (dirt's canonical)
CELL_TERRAIN     + MAT_CLAY    = SPRITE_clay     (clay's canonical)
CELL_WALL        + MAT_GRANITE = SPRITE_rock     (granite's canonical)
CELL_TREE_TRUNK  + MAT_OAK    = SPRITE_tree_trunk_oak  (oak's canonical)
CELL_TREE_TRUNK  + MAT_PINE   = SPRITE_tree_trunk_pine  (pine's canonical)
```

Adding a new material (e.g. MAT_MARBLE with canonical SPRITE_marble) makes
every CF_MATERIAL_SPRITE cell type automatically pick it up. Zero entries.

Adding a new cell type (e.g. CELL_PILLAR) that should look like its material
just needs `CF_MATERIAL_SPRITE` on its CellDef. Zero entries.

### Pattern 2: Shape overrides material (explicit override entry)

The (cell, material) pair has a specific sprite that is **neither** the
material's canonical look **nor** the cell's default. These are the exceptions:

| Cell | Material | Override sprite | Why not canonical? |
|------|----------|----------------|--------------------|
| CELL_WALL | MAT_OAK | SPRITE_wall_wood | wall shape, not trunk |
| CELL_WALL | MAT_PINE | SPRITE_wall_wood | wall shape, not trunk |
| CELL_WALL | MAT_BIRCH | SPRITE_wall_wood | wall shape, not trunk |
| CELL_WALL | MAT_WILLOW | SPRITE_wall_wood | wall shape, not trunk |
| CELL_TREE_LEAVES | MAT_OAK | SPRITE_tree_leaves_oak | leaves shape, not trunk |
| CELL_TREE_LEAVES | MAT_PINE | SPRITE_tree_leaves_pine | leaves shape, not trunk |
| CELL_TREE_LEAVES | MAT_BIRCH | SPRITE_tree_leaves_birch | leaves shape, not trunk |
| CELL_TREE_LEAVES | MAT_WILLOW | SPRITE_tree_leaves_willow | leaves shape, not trunk |
| CELL_SAPLING | MAT_OAK | SPRITE_tree_sapling_oak | sapling shape, not trunk |
| CELL_SAPLING | MAT_PINE | SPRITE_tree_sapling_pine | sapling shape, not trunk |
| CELL_SAPLING | MAT_BIRCH | SPRITE_tree_sapling_birch | sapling shape, not trunk |
| CELL_SAPLING | MAT_WILLOW | SPRITE_tree_sapling_willow | sapling shape, not trunk |

**12 overrides total.** At 80 cell types, expect maybe 30-50.

### Pattern 3: Material-independent (no lookup, cell default)

Cell always looks the same regardless of material. No `CF_MATERIAL_SPRITE`,
no override. Falls through to `CellSprite(cell)`. Examples:

```
CELL_AIR         = SPRITE_air
CELL_LADDER_UP   = SPRITE_ladder_up
CELL_RAMP_N      = SPRITE_ramp_n
```

## The Implementation

```c
// Sparse overrides table (only the exceptions, most entries are 0)
static int spriteOverrides[CELL_TYPE_COUNT][MAT_COUNT];

// New cell flag on CellDef.flags:
#define CF_MATERIAL_SPRITE (1 << 7)  // sprite varies by material

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
    // 1. Explicit override?
    int sprite = spriteOverrides[cell][mat];
    if (sprite) return sprite;

    // 2. Cell uses material sprites? Try material's canonical look.
    if (CellHasFlag(cell, CF_MATERIAL_SPRITE) && mat != MAT_NONE) {
        sprite = MaterialSprite(mat);
        if (sprite) return sprite;
    }

    // 3. Cell default
    return CellSprite(cell);
}
```

### CellDef Changes

Add `CF_MATERIAL_SPRITE` to cell types whose sprite varies by material:

```c
// These cell types use material sprites (pattern 1) + may have overrides (pattern 2):
[CELL_WALL]        = {"wall",        SPRITE_wall,             CF_WALL | CF_MATERIAL_SPRITE, ...},
[CELL_TERRAIN]     = {"terrain",     SPRITE_dirt,             CF_GROUND | CF_BLOCKS_FLUIDS | CF_MATERIAL_SPRITE, ...},
[CELL_TREE_TRUNK]  = {"tree trunk",  SPRITE_tree_trunk_oak,   CF_BLOCKS_MOVEMENT | CF_SOLID | CF_MATERIAL_SPRITE, ...},
[CELL_TREE_BRANCH] = {"tree branch", SPRITE_tree_trunk_oak,   CF_BLOCKS_MOVEMENT | CF_SOLID | CF_MATERIAL_SPRITE, ...},
[CELL_TREE_ROOT]   = {"tree root",   SPRITE_tree_trunk_oak,   CF_BLOCKS_MOVEMENT | CF_SOLID | CF_MATERIAL_SPRITE, ...},
[CELL_TREE_FELLED] = {"felled log",  SPRITE_tree_trunk_oak,   CF_MATERIAL_SPRITE, ...},
[CELL_TREE_LEAVES] = {"tree leaves", SPRITE_tree_leaves_oak,  CF_MATERIAL_SPRITE, ...},
[CELL_SAPLING]     = {"sapling",     SPRITE_tree_sapling_oak, CF_MATERIAL_SPRITE, ...},

// These cell types are material-independent (pattern 3):
[CELL_AIR]         = {"air",         SPRITE_air,              0, ...},
[CELL_LADDER_UP]   = {"ladder up",   SPRITE_ladder_up,        CF_WALKABLE|CF_LADDER, ...},
// etc.
```

## What Gets Removed

### Functions eliminated
- `MaterialWallSprite()` in rendering.c -- wood override now in overrides table
- `GetTreeSpriteAt()` in rendering.c -- replaced by `GetSpriteForCellMat()`
- `GetWallSpriteAt()` in rendering.c -- replaced by `GetSpriteForCellMat()` + natural check
- `GetCellSpriteAt()` in material.c -- replaced by `GetSpriteForCellMat()` + natural check

### MaterialDef fields eliminated
- `leavesSprite` -- moved to overrides table
- `saplingSprite` -- moved to overrides table

### MaterialDef fields kept
- `sprite` -- stays as the "canonical look" used in step 2 of the cascade

### Macros eliminated
- `MaterialLeavesSprite(m)`
- `MaterialSaplingSprite(m)`

### CellDef.sprite stays
As the fallback default (step 3 of the cascade). Useful for material-independent
cells and as the "what does this look like when material is unknown" answer.

## Current Bug This Fixes

`GetWallSpriteAt()` (rendering.c) and `GetCellSpriteAt()` (material.c)
disagree for constructed wood walls:

- `GetWallSpriteAt()` -> `MaterialWallSprite(MAT_OAK)` -> `SPRITE_wall_wood`
- `GetCellSpriteAt()` -> `MaterialSprite(MAT_OAK)` -> `SPRITE_tree_trunk_oak`

One table, one function, one answer. Bug gone.

## Decisions Made

### Floors: keep MaterialFloorSprite() as-is
Floors are a flag (`HAS_FLOOR`), not a cell type. Only 2 overrides (wood, stone).
3 lines of code. Not worth adding to the table. Leave it.

### Natural walls: caller responsibility
Natural walls are always `SPRITE_rock`. "Natural" is a position property, not
a (cell, material) property. The caller checks `IsWallNatural()` before the
table lookup. Could eliminate this with `CELL_NATURAL_WALL` someday.

### Items: separate concern
`ItemSpriteForTypeMaterial()` maps items, not cells. Leave it. Could do an
item sprite table later if item types * materials grows.

## The Bigger Picture: CellDef vs MaterialDef

The sprite table is part of a larger separation of concerns:

| Field            | CellDef          | MaterialDef        |
|------------------|------------------|--------------------|
| name             | shape name       | substance name     |
| sprite           | fallback default | canonical look     |
| flags            | physics (CF_*)   | properties (MF_*)  |
| fuel             | yes (legacy)     | yes (authoritative)|
| insulationTier   | yes (legacy)     | yes (authoritative)|
| dropsItem        | yes (legacy)     | yes (authoritative)|
| dropCount        | yes              | no (move here?)    |
| burnsInto        | CellType         | MaterialType       |
| ignitionRes      | no               | yes                |

`fuel`, `insulationTier`, and `dropsItem` on CellDef are legacy -- they
predate the material system and now serve as fallbacks. The material is always
checked first. The end goal:

**CellDef = pure physics shape** (name, flags, burnsInto)
**MaterialDef = pure substance** (name, sprite, properties, fuel, drops...)
**Sprite overrides = visual exceptions** (cell + material -> override sprite)

But that's Phase 3 (see below) and may not be worth the risk.

## Refactor Phases

### Phase 1: Sprite overrides (visual-only, zero gameplay risk)
```
1a. Add CF_MATERIAL_SPRITE flag to cell_defs.h
1b. Add InitSpriteOverrides() with the 12 current overrides
1c. Add GetSpriteForCellMat(cell, mat)
1d. Replace GetWallSpriteAt() to use GetSpriteForCellMat + natural wall check
1e. Replace GetTreeSpriteAt() -- just delete it, GetSpriteForCellMat handles it
1f. Replace GetCellSpriteAt() to use GetSpriteForCellMat + natural wall check
1g. Remove MaterialWallSprite()
1h. Update tests
```

### Phase 2: Clean MaterialDef sprite fields
```
2a. Remove leavesSprite, saplingSprite from MaterialDef
2b. Remove MaterialLeavesSprite(), MaterialSaplingSprite() macros
2c. Update materialDefs table (just drop two columns)
2d. Update tests
```

### Phase 3: (someday, optional) Clean CellDef legacy fields
```
3a. Remove CellDef.fuel (all fuel checks go through MaterialDef)
3b. Remove CellDef.insulationTier (all insulation through MaterialDef)
3c. Remove CellDef.dropsItem (all drops through material-aware functions)
3d. Audit all callers -- fire.c, temperature.c, mining, crafting
3e. HIGH RISK: touches gameplay systems. May not be worth doing.
```

Each phase is independently shippable. Phase 1 is safe and solves the
immediate frustration. Phase 2 is a clean follow-up. Phase 3 is optional
and should only happen when there's a concrete reason (not just aesthetics).

## Memory Cost

Overrides table: `16 * 15 * 4 = 960 bytes` (mostly zeros)
At scale (80 * 30): `9,600 bytes` (still mostly zeros, still negligible)
