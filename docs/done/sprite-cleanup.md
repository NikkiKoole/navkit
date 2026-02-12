# MaterialDef Sprite Cleanup

## Problem

MaterialDef has 5 sprite fields, most of which are unused per material:

```c
int spriteOffset;       // DEAD CODE - never read
int terrainSprite;      // only soil/stone materials use this
int treeTrunkSprite;    // only 4 wood materials use this
int treeLeavesSprite;   // only 4 wood materials use this
int treeSaplingSprite;  // only 4 wood materials use this
```

Plus there are 3 duplicated sprite lookup functions in rendering.c:
- `MaterialWallSprite()` - hardcoded switch
- `MaterialFloorSprite()` - hardcoded switch
- `GetTreeSpriteAt()` - hardcoded switch

All doing the same thing: map (cell type, material) -> sprite.

## Goal

MaterialDef gets ONE sprite field:

```c
int sprite;  // canonical look: SPRITE_rock, SPRITE_dirt, SPRITE_tree_trunk_oak, etc.
```

All shape-specific variants (leaves, sapling, wall, floor) resolved at lookup time.

## The leaves/sapling problem

Oak as trunk = `SPRITE_tree_trunk_oak` (can be `.sprite`)
Oak as leaves = `SPRITE_tree_leaves_oak` (where does this live?)
Oak as sapling = `SPRITE_tree_sapling_oak` (where does this live?)

Only 4 materials (oak, pine, birch, willow) need this. Every other material
uses the same sprite regardless of cell shape.

### Options considered

**A) Keep leavesSprite + saplingSprite on MaterialDef**
- Simple, works, but 13/15 materials have them as 0
- Goes from 5 fields to 3 (still an improvement)

**B) Separate materials: MAT_OAK_LEAVES, MAT_OAK_SAPLING**
- Pure "one sprite per material" but 4 wood types become 12
- Need parent-material links, IsWoodMaterial() gets complicated

**C) Cell x Material sprite table**
- `int spriteTable[CELL_COUNT][MAT_COUNT]` or sparse version
- Populated at init time, lookup is just `spriteTable[cell][mat]`
- MaterialDef has zero tree sprite fields
- Same data, different location
- Could be a small table for just the cell types that vary by material

**D) Convention-based lookup at render time**
- Only a few cell types vary by material (trunk, leaves, sapling, wall, floor)
- Hardcode those mappings in one central function
- No extra data structures, just one switch/lookup

## How DF does it

DF doesn't store material per-tile at all:
- Tile = just a `tiletype` enum (shape + broad material category)
- Natural stone: mineral "events" on the block with bitmasks
- Trees: separate `plant` entity objects that know their species
- Buildings: separate building objects with mat_type/mat_index

DF's `tiletype_material` has `TREE`, not `OAK`. Species comes from the plant entity.

Our system puts species directly in wallMaterial (simpler but means the material
must carry species-specific sprite info somehow).

## Current sprite mapping (full picture)

| Cell Type | Material | Sprite | Source |
|-----------|----------|--------|--------|
| CELL_TERRAIN | MAT_DIRT | SPRITE_dirt | materialDef.terrainSprite |
| CELL_TERRAIN | MAT_GRANITE | SPRITE_rock | materialDef.terrainSprite |
| CELL_TERRAIN | MAT_CLAY | SPRITE_clay | materialDef.terrainSprite |
| CELL_WALL (natural) | any | SPRITE_rock | hardcoded |
| CELL_WALL (built) | MAT_OAK | SPRITE_wall_wood | MaterialWallSprite() switch |
| CELL_WALL (built) | MAT_GRANITE | SPRITE_rock | MaterialWallSprite() switch |
| CELL_WALL (built) | MAT_BRICK | SPRITE_wall (default) | MaterialWallSprite() switch |
| Floor (built) | MAT_OAK | SPRITE_floor_wood | MaterialFloorSprite() switch |
| Floor (built) | MAT_GRANITE | SPRITE_rock | MaterialFloorSprite() switch |
| CELL_TREE_TRUNK | MAT_OAK | SPRITE_tree_trunk_oak | materialDef.treeTrunkSprite / GetTreeSpriteAt() |
| CELL_TREE_LEAVES | MAT_OAK | SPRITE_tree_leaves_oak | materialDef.treeLeavesSprite / GetTreeSpriteAt() |
| CELL_SAPLING | MAT_OAK | SPRITE_tree_sapling_oak | materialDef.treeSaplingSprite / GetTreeSpriteAt() |

## Decision: Option A

Go with the pragmatic approach. 5 fields become 3:

```c
typedef struct {
    const char* name;
    int sprite;           // canonical sprite (was terrainSprite for soil, treeTrunkSprite for wood)
    int leavesSprite;     // CELL_TREE_LEAVES variant (only wood materials, 0 for rest)
    int saplingSprite;    // CELL_SAPLING variant (only wood materials, 0 for rest)
    uint8_t flags;
    uint8_t fuel;
    uint8_t ignitionResistance;
    ItemType dropsItem;
    uint8_t insulationTier;
    MaterialType burnsIntoMat;
} MaterialDef;
```

Removed:
- `spriteOffset` (dead code, never read)
- `terrainSprite` (merged into `sprite`)
- `treeTrunkSprite` (merged into `sprite`)

Renamed:
- `treeLeavesSprite` -> `leavesSprite`
- `treeSaplingSprite` -> `saplingSprite`

Example data:
```
[MAT_OAK]     = {"Oak",     SPRITE_tree_trunk_oak, SPRITE_tree_leaves_oak, SPRITE_tree_sapling_oak, MF_FLAMMABLE, ...}
[MAT_GRANITE] = {"Granite", SPRITE_rock,           0,                      0,                       0,            ...}
[MAT_DIRT]    = {"Dirt",    SPRITE_dirt,            0,                      0,                       0,            ...}
[MAT_BRICK]   = {"Brick",  0,                      0,                      0,                       0,            ...}
```

Also clean up:
- Remove `MaterialWallSprite()` switch in rendering.c (use `materialDefs[mat].sprite`)
- Remove `MaterialFloorSprite()` switch in rendering.c (use `materialDefs[mat].sprite`)
- Remove `GetTreeSpriteAt()` switch in rendering.c (use sprite/leavesSprite/saplingSprite)
- Collapse `GetCellSpriteAt()` to use the new fields
- Update all macros and tests

Two mostly-zero fields on a 15-entry struct is not worth building
infrastructure to avoid. Revisit if/when there are many more shape variants.
