# Dwarf Fortress Tile System Reference

DF separates every tile into orthogonal properties:

| Property | What it describes | Examples |
|---|---|---|
| **Shape** | Physical form | `WALL`, `FLOOR`, `RAMP`, `STAIR_UP`, `STAIR_DOWN`, `EMPTY`, `FORTIFICATION`, `BOULDER`, `TRUNK_BRANCH`, `SAPLING` |
| **Material** | What it's made of | `SOIL`, `STONE`, `MINERAL`, `LAVA_STONE`, `FROZEN_LIQUID`, `CONSTRUCTION`, `GRASS_LIGHT`, `TREE`, `MUSHROOM` |
| **Special** | Surface treatment | `NORMAL`, `SMOOTH`, `FURROWED`, `WET`, `DEAD`, `WORN_1/2/3`, `TRACK` |
| **Variant** | Visual variation | `VAR_1` through `VAR_4` |

## Full Enums (from DFHack structures)

### tiletype_shape
NONE, EMPTY, FLOOR, BOULDER, PEBBLES, WALL, FORTIFICATION, STAIR_UP, STAIR_DOWN, STAIR_UPDOWN, RAMP, RAMP_TOP, BROOK_BED, BROOK_TOP, BRANCH, TRUNK_BRANCH, TWIG, SAPLING, SHRUB, ENDLESS_PIT

### tiletype_material
NONE, AIR, SOIL, STONE, FEATURE, LAVA_STONE, MINERAL, FROZEN_LIQUID, CONSTRUCTION, GRASS_LIGHT, GRASS_DARK, GRASS_DRY, GRASS_DEAD, PLANT, HFS, CAMPFIRE, FIRE, ASHES, MAGMA, DRIFTWOOD, POOL, BROOK, RIVER, ROOT, TREE, MUSHROOM, UNDERWORLD_GATE

### tiletype_special
NONE, NORMAL, RIVER_SOURCE, WATERFALL, SMOOTH, FURROWED, WET, DEAD, WORN_1, WORN_2, WORN_3, TRACK, SMOOTH_DEAD

### tiletype_variant
NONE, VAR_1, VAR_2, VAR_3, VAR_4

## How This Maps to NavKit

### Current vs Proposed

| DF Concept | NavKit Current | NavKit Proposed |
|---|---|---|
| `shape=WALL, material=SOIL` | `CELL_DIRT/CLAY/etc.` + `MAT_DIRT/CLAY/etc.` | `CELL_TERRAIN` + `MAT_DIRT/CLAY/etc.` |
| `shape=WALL, material=STONE` | `CELL_ROCK` + `MAT_GRANITE` | `CELL_TERRAIN` + `MAT_GRANITE` |
| `shape=WALL, material=CONSTRUCTION` | `CELL_WALL` + material | `CELL_WALL` + material (unchanged) |
| unmineable layer | `CELL_BEDROCK` (separate cell type) | `CELL_TERRAIN` + `MAT_BEDROCK` (MF_UNMINEABLE) |
| `shape=RAMP` | `CELL_RAMP_N/E/S/W` (no material) | `CELL_RAMP_N/E/S/W` + `wallMaterial` |
| `shape=STAIR_*` | `CELL_LADDER_UP/DOWN/BOTH` | unchanged |
| `shape=EMPTY, material=AIR` | `CELL_AIR` | unchanged |
| tree trunk/leaves/sapling | `CELL_TREE_*` + `treeTypeGrid` + `treePartGrid` | `CELL_TREE_TRUNK/BRANCH/ROOT/FELLED/LEAVES` + `wallMaterial` |
| `special=SMOOTH/ROUGH` | `FINISH_ROUGH/SMOOTH` | unchanged |
| grass materials | `SURFACE_GRASS/TALL_GRASS/BARE` | unchanged |

### NavKit Property Layers (Proposed)

| Layer | Grid | What it stores |
|---|---|---|
| **Shape** | `grid[z][y][x]` (CellType) | Physical form: TERRAIN, WALL, AIR, RAMP_*, LADDER_*, TREE_TRUNK/BRANCH/ROOT/FELLED/LEAVES, SAPLING |
| **Material** | `wallMaterial[z][y][x]` | What it's made of: MAT_DIRT, MAT_OAK, MAT_GRANITE, etc. |
| **Finish** | `wallFinish[z][y][x]` | Surface treatment: FINISH_ROUGH, FINISH_SMOOTH, etc. |
| **Surface** | `cellSurface` bits | Ground overlay: SURFACE_BARE, SURFACE_GRASS, SURFACE_TALL_GRASS |

Every cell's complete identity: **shape + material**. No extra parallel grids.

### Key Insight

DF has NO equivalent of `CELL_DIRT`, `CELL_CLAY`, `CELL_SAND`, `CELL_ROCK`. Those are all
`shape=WALL` with different materials. The material layer does all the differentiation.

NavKit adds one DF doesn't have: shape-level `CELL_TERRAIN` vs `CELL_WALL` to distinguish
natural terrain from player-built constructions. This replaces the `wallNatural` grid.

## Sources
- [DFHack tile-types structures](https://peridexiserrant.neocities.org/docs-structures-test/docs/_auto/structures/tile-types)
- [DFHack tiletypes tool](https://docs.dfhack.org/en/stable/docs/tools/tiletypes.html)
- [DF Wiki: Tile types in memory](https://dwarffortresswiki.org/index.php/DF2014:Tile_types_in_DF_memory)
