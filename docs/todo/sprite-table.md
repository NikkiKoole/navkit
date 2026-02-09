# Sprite Lookup Table: (CellType, MaterialType) -> Sprite

## The Core Insight

The sprite for a cell is not a property of the material alone, nor of the cell
type alone. It's a property of the **combination**:

```
CELL_TREE_TRUNK  + MAT_OAK     = SPRITE_tree_trunk_oak
CELL_TREE_LEAVES + MAT_OAK     = SPRITE_tree_leaves_oak
CELL_SAPLING     + MAT_OAK     = SPRITE_tree_sapling_oak
CELL_TERRAIN     + MAT_DIRT    = SPRITE_dirt
CELL_WALL (built)+ MAT_OAK     = SPRITE_wall_wood
CELL_WALL (built)+ MAT_GRANITE = SPRITE_rock
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

That's **6 functions + 3 struct fields** encoding the same conceptual thing:
"what does this (shape, material) pair look like?"

## The Problem

1. **leavesSprite/saplingSprite** on MaterialDef are tree-specific fields that
   13 of 15 materials set to 0. They exist because the material alone can't
   express "I look different as leaves vs trunk."

2. **Two parallel sprite dispatchers** (`GetWallSpriteAt` in rendering.c,
   `GetCellSpriteAt` in material.c) that **disagree** for constructed wood
   walls: one returns `SPRITE_wall_wood`, the other `SPRITE_tree_trunk_oak`.

3. **Hardcoded switches everywhere**: `MaterialWallSprite()` has a wood
   override, `MaterialFloorSprite()` has a wood override,
   `ItemSpriteForTypeMaterial()` has a ITEM_LOG switch. All encoding
   (shape, material) -> sprite knowledge in ad-hoc ways.

4. **Adding a new material-dependent cell type** (e.g., doors, furniture)
   would require adding yet another field to MaterialDef or another switch.

## The Deeper Question: CellDef vs MaterialDef

Before solving sprites, it's worth seeing the conceptual overlap between the
two definition structs. Side by side:

| Field            | CellDef          | MaterialDef        |
|------------------|------------------|--------------------|
| name             | shape name       | substance name     |
| sprite           | fallback default | canonical + leaves/sapling |
| flags            | physics (CF_*)   | properties (MF_*)  |
| fuel             | yes              | yes                |
| insulationTier   | yes              | yes                |
| dropsItem        | yes              | yes                |
| dropCount        | yes              | no                 |
| burnsInto        | CellType         | MaterialType       |
| ignitionRes      | no               | yes                |

`fuel`, `insulationTier`, and `dropsItem` appear in **both**. The code already
knows which one is authoritative -- `GetInsulationAt()` checks material first,
falls back to cell. `GetWallDropItem()` checks material for constructed/terrain,
cell for natural walls. The material is the truth; the cell is the fallback.

**CellDef carries material-like data because it predates the material system.**
Those fields were on CellDef before MaterialDef existed. After the terrain
refactor, almost every cell that has fuel/insulation/drops also has a material,
so the CellDef values are mostly dead weight acting as fallbacks.

### The Clean Separation (End Goal)

Three orthogonal systems:

**CellDef = pure physics shape**
```c
typedef struct {
    const char* name;       // "wall", "tree trunk", "ladder up"
    uint8_t flags;          // CF_SOLID, CF_BLOCKS_MOVEMENT, CF_LADDER, CF_RAMP...
    CellType burnsInto;     // shape -> shape (trunk -> air, terrain stays terrain)
} CellDef;
```

**MaterialDef = pure substance**
```c
typedef struct {
    const char* name;       // "Oak", "Granite", "Dirt"
    int sprite;             // canonical look (for rules-based sprite resolution)
    uint8_t flags;          // MF_FLAMMABLE, MF_UNMINEABLE
    uint8_t fuel;
    uint8_t ignitionResistance;
    uint8_t insulationTier;
    ItemType dropsItem;
    uint8_t dropCount;
    MaterialType burnsIntoMat;
} MaterialDef;
```

**Sprite table = visual appearance** (cell + material -> sprite)

Shape knows nothing about materials. Materials know nothing about shapes.
The sprite table maps the combinations.

`CellDef.name` is still useful as the shape name ("tree trunk", "wall"),
while `MaterialDef.name` is the substance name ("Oak", "Granite").
`GetCellNameAt()` already combines them: "Oak tree trunk".

## Proposed Solution: Sprite Table

One 2D lookup table that maps `(CellType, MaterialType) -> sprite`:

```c
static int spriteTable[CELL_TYPE_COUNT][MAT_COUNT];

void InitSpriteTable(void) {
    memset(spriteTable, 0, sizeof(spriteTable));

    // Terrain
    spriteTable[CELL_TERRAIN][MAT_DIRT]    = SPRITE_dirt;
    spriteTable[CELL_TERRAIN][MAT_GRANITE] = SPRITE_rock;
    spriteTable[CELL_TERRAIN][MAT_CLAY]    = SPRITE_clay;
    spriteTable[CELL_TERRAIN][MAT_GRAVEL]  = SPRITE_gravel;
    spriteTable[CELL_TERRAIN][MAT_SAND]    = SPRITE_sand;
    spriteTable[CELL_TERRAIN][MAT_PEAT]    = SPRITE_peat;
    spriteTable[CELL_TERRAIN][MAT_BEDROCK] = SPRITE_bedrock;

    // Constructed walls
    spriteTable[CELL_WALL][MAT_OAK]     = SPRITE_wall_wood;
    spriteTable[CELL_WALL][MAT_PINE]    = SPRITE_wall_wood;
    spriteTable[CELL_WALL][MAT_BIRCH]   = SPRITE_wall_wood;
    spriteTable[CELL_WALL][MAT_WILLOW]  = SPRITE_wall_wood;
    spriteTable[CELL_WALL][MAT_GRANITE] = SPRITE_rock;
    spriteTable[CELL_WALL][MAT_DIRT]    = SPRITE_dirt;
    spriteTable[CELL_WALL][MAT_CLAY]    = SPRITE_clay;
    spriteTable[CELL_WALL][MAT_GRAVEL]  = SPRITE_gravel;
    spriteTable[CELL_WALL][MAT_SAND]    = SPRITE_sand;
    spriteTable[CELL_WALL][MAT_PEAT]    = SPRITE_peat;
    spriteTable[CELL_WALL][MAT_BEDROCK] = SPRITE_bedrock;
    // MAT_BRICK/IRON/GLASS -> 0, falls through to CellSprite(CELL_WALL) = SPRITE_wall

    // Tree trunks (trunk/branch/root/felled all share the trunk sprite)
    for (int cell = CELL_TREE_TRUNK; cell <= CELL_TREE_FELLED; cell++) {
        spriteTable[cell][MAT_OAK]    = SPRITE_tree_trunk_oak;
        spriteTable[cell][MAT_PINE]   = SPRITE_tree_trunk_pine;
        spriteTable[cell][MAT_BIRCH]  = SPRITE_tree_trunk_birch;
        spriteTable[cell][MAT_WILLOW] = SPRITE_tree_trunk_willow;
    }
    // NOTE: assumes CELL_TREE_TRUNK..CELL_TREE_FELLED are contiguous in enum

    // Leaves
    spriteTable[CELL_TREE_LEAVES][MAT_OAK]    = SPRITE_tree_leaves_oak;
    spriteTable[CELL_TREE_LEAVES][MAT_PINE]   = SPRITE_tree_leaves_pine;
    spriteTable[CELL_TREE_LEAVES][MAT_BIRCH]  = SPRITE_tree_leaves_birch;
    spriteTable[CELL_TREE_LEAVES][MAT_WILLOW] = SPRITE_tree_leaves_willow;

    // Saplings
    spriteTable[CELL_SAPLING][MAT_OAK]    = SPRITE_tree_sapling_oak;
    spriteTable[CELL_SAPLING][MAT_PINE]   = SPRITE_tree_sapling_pine;
    spriteTable[CELL_SAPLING][MAT_BIRCH]  = SPRITE_tree_sapling_birch;
    spriteTable[CELL_SAPLING][MAT_WILLOW] = SPRITE_tree_sapling_willow;
}
```

Lookup:

```c
int GetSpriteForCellMat(CellType cell, MaterialType mat) {
    int sprite = spriteTable[cell][mat];
    return sprite ? sprite : CellSprite(cell);
}
```

## What This Replaces

| Before                        | After                                           |
|-------------------------------|--------------------------------------------------|
| `MaterialDef.sprite`          | Entries in spriteTable (or kept for rules, see below) |
| `MaterialDef.leavesSprite`    | Entries in spriteTable                           |
| `MaterialDef.saplingSprite`   | Entries in spriteTable                           |
| `CellDef.sprite`              | Stays as fallback default                        |
| `MaterialWallSprite()`        | Entries in spriteTable                           |
| `MaterialFloorSprite()`       | Separate (see Floors below)                      |
| `GetTreeSpriteAt()`           | `GetSpriteForCellMat()`                          |
| `GetWallSpriteAt()`           | `GetSpriteForCellMat()` + natural wall check     |
| `GetCellSpriteAt()`           | `GetSpriteForCellMat()` + natural wall check     |

## Current Bug: Two Dispatchers Disagree

`GetWallSpriteAt()` (rendering.c) and `GetCellSpriteAt()` (material.c) are
near-identical but produce **different results for constructed wood walls**:

- `GetWallSpriteAt()` -> `MaterialWallSprite(MAT_OAK)` -> `SPRITE_wall_wood`
- `GetCellSpriteAt()` -> `MaterialSprite(MAT_OAK)` -> `SPRITE_tree_trunk_oak`

`GetCellSpriteAt()` is currently only called from tests, so this doesn't
cause a visual bug. But it's a latent divergence. The sprite table eliminates
it -- one table, one answer.

## Complete Current Sprite Mappings

For reference, every (cell, material) -> sprite mapping that exists today:

### CELL_WALL (natural)
All materials -> `SPRITE_rock` (hardcoded, ignores material)

### CELL_WALL (constructed)
| Material | Sprite | Source |
|----------|--------|--------|
| MAT_OAK/PINE/BIRCH/WILLOW | `SPRITE_wall_wood` | `MaterialWallSprite()` |
| MAT_GRANITE | `SPRITE_rock` | via `MaterialSprite()` |
| MAT_DIRT/CLAY/GRAVEL/SAND/PEAT/BEDROCK | material's canonical sprite | via `MaterialSprite()` |
| MAT_BRICK/IRON/GLASS | `SPRITE_wall` (default) | `MaterialSprite()` returns 0 |

### CELL_TERRAIN
| Material | Sprite |
|----------|--------|
| MAT_DIRT | `SPRITE_dirt` |
| MAT_GRANITE | `SPRITE_rock` |
| MAT_CLAY | `SPRITE_clay` |
| MAT_GRAVEL | `SPRITE_gravel` |
| MAT_SAND | `SPRITE_sand` |
| MAT_PEAT | `SPRITE_peat` |
| MAT_BEDROCK | `SPRITE_bedrock` |
| MAT_NONE | `SPRITE_dirt` (CellSprite fallback) |

### Tree cells
| Cell | MAT_OAK | MAT_PINE | MAT_BIRCH | MAT_WILLOW | Non-wood |
|------|---------|----------|-----------|------------|----------|
| TRUNK/BRANCH/ROOT/FELLED | trunk_oak | trunk_pine | trunk_birch | trunk_willow | trunk_oak (fallback) |
| LEAVES | leaves_oak | leaves_pine | leaves_birch | leaves_willow | leaves_oak (fallback) |
| SAPLING | sapling_oak | sapling_pine | sapling_birch | sapling_willow | sapling_oak (fallback) |

### Constructed floors (HAS_FLOOR flag)
| Material | Sprite | Source |
|----------|--------|--------|
| MAT_OAK/PINE/BIRCH/WILLOW | `SPRITE_floor_wood` | `MaterialFloorSprite()` |
| MAT_GRANITE | `SPRITE_rock` | via `MaterialSprite()` |
| Others | `SPRITE_floor` (default) | fallback |

### Material-independent cells (always use CellSprite)
CELL_AIR, CELL_LADDER_UP/DOWN/BOTH, CELL_RAMP_N/E/S/W

### Other sprite systems (not in the table)
- **Grass overlay**: CELL_TERRAIN + MAT_DIRT -> surface state -> grass sprites
- **Surface finish overlay**: CELL_WALL -> finish -> finish sprites
- **Color tinting**: wood materials get species-specific brown tints
- **Item sprites**: ITEM_LOG uses species trunk sprite, ITEM_BLOCKS uses floor sprite

## Open Questions

### Floors

Floors aren't a CellType -- they're a flag (`HAS_FLOOR`) on any cell.
`MaterialFloorSprite()` currently maps `(floor, material) -> sprite`.

Options:
- **A)** Add a FLOOR pseudo-index to the table (extra row at `CELL_TYPE_COUNT`)
- **B)** Keep `MaterialFloorSprite()` as-is (3 lines, stable, separate concern)

Leaning toward **B**. Floors are flag-based, not cell-based. Only 2 overrides
(wood -> floor_wood, stone -> rock). Not worth the abstraction.

### Natural vs Constructed Walls

Natural walls are always `SPRITE_rock` regardless of material. The table only
encodes constructed wall sprites. The caller still needs:

```c
if (cell == CELL_WALL && IsWallNatural(x, y, z)) return SPRITE_rock;
```

This is fine -- "natural" is a property of the position, not the (cell, material)
pair, so it doesn't belong in the table.

Alternative: `CELL_NATURAL_WALL` as a distinct cell type, eliminating the
`wallNatural` grid. Bigger change, separate discussion.

### Items

`ItemSpriteForTypeMaterial()` maps `(ItemType, MaterialType) -> sprite` for
logs and blocks. Conceptually a different table (item sprites, not cell sprites).
Could add `itemSpriteTable[ITEM_COUNT][MAT_COUNT]` later, or leave the small
switch as-is. Not blocking.

## Scaling: What If 80 Cell Types?

The flat table works at current size (16 * 15 = 240 slots, 960 bytes). At
80 cell types and 30 materials: 80 * 30 * 4 = 9,600 bytes. Still nothing
for memory.

But **maintainability** matters. 480 entries in `InitSpriteTable()` is a big
wall of assignments. The key insight: most cells follow one of three patterns:

1. **Material IS the sprite** -- CELL_TERRAIN + MAT_DIRT = SPRITE_dirt.
   The material's canonical sprite is used directly.
2. **Shape overrides material** -- CELL_WALL + MAT_OAK = SPRITE_wall_wood.
   The (cell, material) pair has a specific sprite that is neither the
   material's canonical look nor the cell's default.
3. **Material-independent** -- CELL_LADDER = SPRITE_ladder.
   Material is irrelevant; always use cell default.

Pattern 1 and 3 don't need explicit table entries -- they can be **rules**.
Only pattern 2 (the exceptions) needs explicit entries. Right now there are
only **12 explicit overrides**:

- CELL_WALL + 4 wood types -> SPRITE_wall_wood
- CELL_TREE_LEAVES + 4 wood types -> species leaves sprite
- CELL_SAPLING + 4 wood types -> species sapling sprite

Everything else follows from "use material's canonical sprite" or "use cell
default."

### Rules + Overrides Approach

```c
// Sparse overrides table (only the exceptions)
static int spriteOverrides[CELL_TYPE_COUNT][MAT_COUNT];

// Cell flag: does this cell type's sprite vary by material?
#define CF_MATERIAL_SPRITE (1 << 7)

int GetSprite(CellType cell, MaterialType mat) {
    // 1. Check explicit overrides (the exceptions)
    int sprite = spriteOverrides[cell][mat];
    if (sprite) return sprite;

    // 2. If cell uses material sprites, try material's canonical sprite
    if (CellHasFlag(cell, CF_MATERIAL_SPRITE) && mat != MAT_NONE) {
        sprite = MaterialSprite(mat);
        if (sprite) return sprite;
    }

    // 3. Cell's own default
    return CellSprite(cell);
}
```

At 80 cell types, you'd have maybe 30-50 overrides instead of 480 explicit
mappings. Adding a new cell type that follows the material rule (pattern 1)
requires **zero table entries** -- just set `CF_MATERIAL_SPRITE` on its CellDef.

**Trade-off**: 3-step cascade lookup vs flat table. For 16 cells the flat
table is simpler. For 80+ the rules approach scales better.

### MaterialDef.sprite in the Rules Approach

With rules + overrides, `MaterialDef.sprite` **stays** as the "canonical
sprite" used in step 2. Only `leavesSprite` and `saplingSprite` move to the
overrides table. This is the minimal change that kills the original frustration
(tree-specific fields polluting MaterialDef) while scaling well.

## Refactor Order (Safest Path)

The three concerns are: physics/shape (CellDef flags), substance (MaterialDef
properties), and visuals (sprites). Physics is already clean. The messy part
is visuals and substance tangled together across both structs.

**Sprites first.** Here's why:

1. **Purely additive.** Add `InitSpriteTable()` + `GetSpriteForCellMat()`
   alongside existing code. Nothing breaks. Migrate callers one at a time,
   test after each.

2. **Clearest boundary.** Sprites are read-only lookups at render time. No
   gameplay logic depends on them. If a sprite is wrong, you see it immediately
   -- it's visual, not a silent corruption.

3. **Unblocks MaterialDef cleanup.** Once sprites go through the table,
   remove `leavesSprite` and `saplingSprite`. Then optionally `sprite` too.
   Each removal is a small, testable step.

4. **Does NOT require touching CellDef.** The risky refactor would be removing
   `fuel`, `insulationTier`, `dropsItem` from CellDef -- that touches fire,
   temperature, mining, crafting. Lots of gameplay paths. Leave that for later.

### Concrete Steps

```
Phase 1: Sprite table (visual-only, zero gameplay risk)
  1a. Add InitSpriteTable() with all current mappings
  1b. Add GetSpriteForCellMat(cell, mat)
  1c. Replace GetWallSpriteAt() internals to use the table
  1d. Replace GetTreeSpriteAt() with table lookup
  1e. Replace GetCellSpriteAt() with table lookup (fixes wood wall bug)
  1f. Remove MaterialWallSprite() (logic now in table data)
  1g. Update tests

Phase 2: Clean MaterialDef sprite fields
  2a. Remove leavesSprite, saplingSprite from MaterialDef
  2b. Remove MaterialLeavesSprite(), MaterialSaplingSprite() macros
  2c. Decide: keep MaterialDef.sprite (for rules approach) or remove it too
  2d. Update materialDefs table and tests

Phase 3: (someday, optional) Clean CellDef material-like fields
  3a. Remove CellDef.fuel (all fuel checks go through MaterialDef)
  3b. Remove CellDef.insulationTier (all insulation goes through MaterialDef)
  3c. Remove CellDef.dropsItem (all drops go through material-aware functions)
  3d. Audit all callers of CellFuel/CellInsulationTier/CellDropsItem
  3e. This touches fire.c, temperature.c, mining, crafting -- high risk
  3f. May not be worth doing if the fallback pattern keeps working fine
```

Each phase is independently shippable. Phase 3 is the scary one. The fallback
pattern (check material, fall back to cell) works and might be good enough
forever. Don't fix what isn't hurting.

## Memory Cost

Flat table: `16 cell types * 15 materials * 4 bytes = 960 bytes`
At scale: `80 cell types * 30 materials * 4 bytes = 9,600 bytes`

Both negligible compared to the 6 grids of `512*512*16` bytes each.
