# Refactor: Shape/Material Separation

See `docs/doing/df-tile-reference.md` for the DF comparison that motivated this.

## Motivation

Cell type and material are tangled. We have 6 cell types for natural terrain
(`CELL_DIRT`, `CELL_CLAY`, `CELL_GRAVEL`, `CELL_SAND`, `CELL_PEAT`, `CELL_ROCK`) that
duplicate data already in the material system. Trees use a separate `treeTypeGrid` that
maps 1:1 to materials (`TREE_TYPE_OAK` -> `MAT_OAK`). Bedrock is a separate cell type
just because it's unmineable.

The fix: **shape and material become orthogonal**. Cell type = shape (physical form).
`wallMaterial` = what it's made of. Everything else follows.

## Decisions

| Question | Decision |
|---|---|
| Natural vs built distinction | Shape level: `CELL_TERRAIN` (natural) vs `CELL_WALL` (built) |
| CELL_BEDROCK | Becomes `CELL_TERRAIN` + `MAT_BEDROCK` with `MF_UNMINEABLE` flag |
| Ramp materials | Yes — `CELL_RAMP_*` gets `wallMaterial` like everything else |
| Tree species | `treeTypeGrid` eliminated, use `wallMaterial` (MAT_OAK etc.) |
| Tree parts | `treePartGrid` eliminated — expand to separate cell types |
| wallNatural grid | Eliminated — `CELL_TERRAIN` = natural, `CELL_WALL` = built |
| CellSprite API | New `GetCellSprite(x,y,z)` that dispatches by cell type + material |

## What the CellType Enum Becomes

```
Before (19 types + treePartGrid):
  CELL_WALL, CELL_AIR,
  CELL_LADDER_UP, CELL_LADDER_DOWN, CELL_LADDER_BOTH,
  CELL_DIRT, CELL_CLAY, CELL_GRAVEL, CELL_SAND, CELL_PEAT, CELL_ROCK,  <-- 6 soil types
  CELL_BEDROCK,
  CELL_RAMP_N, CELL_RAMP_E, CELL_RAMP_S, CELL_RAMP_W,
  CELL_SAPLING, CELL_TREE_TRUNK, CELL_TREE_LEAVES
  + treePartGrid distinguishes TRUNK/BRANCH/ROOT/FELLED for CELL_TREE_TRUNK

After (16 types, no extra grids):
  CELL_WALL, CELL_AIR,
  CELL_LADDER_UP, CELL_LADDER_DOWN, CELL_LADDER_BOTH,
  CELL_TERRAIN,                                                         <-- replaces 6 + bedrock
  CELL_RAMP_N, CELL_RAMP_E, CELL_RAMP_S, CELL_RAMP_W,
  CELL_SAPLING, CELL_TREE_TRUNK, CELL_TREE_BRANCH, CELL_TREE_ROOT,     <-- trunk split into 3
  CELL_TREE_FELLED, CELL_TREE_LEAVES
```

The principle: every cell's complete identity is `grid[z][y][x]` (shape) +
`wallMaterial[z][y][x]` (material). No extra parallel grids needed.

Current `treePartGrid` combinations that become separate cell types:

| Current | After |
|---|---|
| `CELL_TREE_TRUNK` + `TREE_PART_TRUNK` | `CELL_TREE_TRUNK` |
| `CELL_TREE_TRUNK` + `TREE_PART_BRANCH` | `CELL_TREE_BRANCH` |
| `CELL_TREE_TRUNK` + `TREE_PART_ROOT` | `CELL_TREE_ROOT` |
| `CELL_TREE_TRUNK` + `TREE_PART_FELLED` | `CELL_TREE_FELLED` |
| `CELL_TREE_LEAVES` + `TREE_PART_NONE` | `CELL_TREE_LEAVES` (unchanged) |
| `CELL_SAPLING` + `TREE_PART_NONE` | `CELL_SAPLING` (unchanged) |

## What Gets Eliminated

| Thing | Why it's redundant |
|---|---|
| `CELL_DIRT/CLAY/GRAVEL/SAND/PEAT/ROCK` | Replaced by `CELL_TERRAIN` + material |
| `CELL_BEDROCK` | Replaced by `CELL_TERRAIN` + `MAT_BEDROCK` (MF_UNMINEABLE) |
| `treeTypeGrid[z][y][x]` | Replaced by `wallMaterial[z][y][x]` |
| `treePartGrid[z][y][x]` | Replaced by separate cell types (BRANCH/ROOT/FELLED) |
| `TreeType` enum | `MAT_OAK/PINE/BIRCH/WILLOW` already exist |
| `TreePart` enum | Encoded in cell type instead |
| `MaterialFromTreeType()` | No longer needed |
| `MaterialForGroundCell()` | No longer needed — material is just `GetWallMaterial()` |
| `wallNatural[z][y][x]` grid | `CELL_TERRAIN` = natural, `CELL_WALL` = built |
| `IsWallNatural/SetWallNatural/ClearWallNatural` | Replaced by cell type check |
| `IsGroundCell()` (6-way check) | Becomes `cell == CELL_TERRAIN` |

## New Things

### MAT_BEDROCK
```c
MAT_BEDROCK  // in MaterialType enum
```
With `MF_UNMINEABLE` flag in materialDefs. Designation system checks this flag instead
of checking `cell == CELL_BEDROCK`.

### MaterialDef new fields
```c
typedef struct {
    const char* name;
    int spriteOffset;           // existing — sprite variant for CELL_WALL
    int terrainSprite;          // NEW — sprite when used as CELL_TERRAIN
    int treeTrunkSprite;        // NEW — sprite for CELL_TREE_TRUNK
    int treeBranchSprite;       // NEW — sprite for CELL_TREE_BRANCH
    int treeRootSprite;         // NEW — sprite for CELL_TREE_ROOT
    int treeFelledSprite;       // NEW — sprite for CELL_TREE_FELLED
    int treeLeavesSprite;       // NEW — sprite for CELL_TREE_LEAVES
    int treeSaplingSprite;      // NEW — sprite for CELL_SAPLING
    uint8_t flags;              // existing + MF_UNMINEABLE
    uint8_t fuel;               // existing
    uint8_t insulationTier;     // NEW — insulation when used as terrain
    uint8_t ignitionResistance; // existing
    ItemType dropsItem;         // existing
    MaterialType burnsIntoMat;  // NEW — material after burning (peat -> dirt)
} MaterialDef;
```

Most materials only populate a few sprite fields (e.g. MAT_DIRT only needs terrainSprite,
MAT_OAK needs tree* sprites, MAT_GRANITE needs terrainSprite). Zero = use cellDef default.

### Position-aware helpers
```c
int  GetCellSprite(int x, int y, int z);       // dispatches by cell type + material
int  GetCellFuel(int x, int y, int z);          // material fuel for terrain, cell fuel otherwise
int  GetCellInsulation(int x, int y, int z);     // material insulation for terrain
const char* GetCellName(int x, int y, int z);   // material name for terrain
```

These replace the current position-free `CellSprite(cell)`, `CellFuel(cell)` etc. for
terrain cells. Non-terrain cells still use the simple cellDefs lookup internally.

### NaturalTerrainSpec simplifies
```c
// cellType is always CELL_TERRAIN now, so only material + surface matter
static inline CellPlacementSpec NaturalTerrainSpec(MaterialType mat, uint8_t surface,
                                                    bool clearFloor, bool clearWater) {
    return (CellPlacementSpec){
        .cellType = CELL_TERRAIN, .wallMat = mat, .wallNatural = true,
        .wallFinish = FINISH_ROUGH, .clearFloor = clearFloor, .floorMat = MAT_NONE,
        .floorNatural = false, .clearWater = clearWater, .surfaceType = surface
    };
}
```
Note: `wallNatural` field stays in the struct temporarily during migration, removed in
Phase 4 when the wallNatural grid is deleted.

## Impact by File

### HIGH impact

**terrain.c** — 68 refs
- All generators: `grid[z][y][x] = CELL_DIRT` becomes
  `grid[z][y][x] = CELL_TERRAIN; SetWallMaterial(x,y,z, MAT_DIRT);`
- Tree type selection by soil: switch on `GetWallMaterial` instead of cell type
- Soil surface selection: same logic, switch on material
- Terrain generators that set `treeTypeGrid`: use `SetWallMaterial` instead

**input.c** — 33 refs
- ExecuteBuildSoil/ExecutePileSoil: drop CellType param, just take MaterialType
- NaturalTerrainSpec: drops cellType param (always CELL_TERRAIN)
- Grass checks (`== CELL_DIRT`): `== CELL_TERRAIN && GetWallMaterial == MAT_DIRT`

**trees.c** — ~35 refs to treeTypeGrid + ~35 refs to treePartGrid
- All `treeTypeGrid[z][y][x] = (uint8_t)type` become `SetWallMaterial(x,y,z, mat)`
- All `(TreeType)treeTypeGrid[z][y][x]` become `GetWallMaterial(x,y,z)`
- All `treePartGrid[z][y][x] = TREE_PART_BRANCH` become `grid[z][y][x] = CELL_TREE_BRANCH`
- All `treePartGrid[z][y][x] == TREE_PART_TRUNK` become `grid[z][y][x] == CELL_TREE_TRUNK`
- `MaterialFromTreeType()` calls: replaced by direct material use
- Tree sprite lookup: check `wallMaterial` instead of `treeTypeGrid`

**designations.c** — ~20 refs to treeTypeGrid + ~15 refs to treePartGrid + 3 soil refs
- Tree felling: `treeTypeGrid` checks become `GetWallMaterial` checks
- Tree felling: `treePartGrid` checks become cell type checks
- Mining: `ct == CELL_ROCK` becomes `ct == CELL_TERRAIN` (material determines drops)
- `ct == CELL_BEDROCK` becomes `MaterialIsUnmineable(GetWallMaterial(x,y,z))`

### MODERATE impact

**cell_defs.c/h** — 14 refs
- Remove 7 enum values (6 soils + bedrock), add CELL_TERRAIN
- Remove 7 cellDef entries, add 1 (CELL_TERRAIN)
- Add 3 tree cell type entries (CELL_TREE_BRANCH, CELL_TREE_ROOT, CELL_TREE_FELLED)
  with appropriate flags (branch/root block movement like trunk, felled is walkable)
- Remove TreePart enum
- `IsGroundCell()` → `cell == CELL_TERRAIN`
- Add `IsTreeCell()` helper if needed

**material.c/h** — 6 refs
- Remove `MaterialForGroundCell`
- Add MAT_BEDROCK + MF_UNMINEABLE
- Add new MaterialDef fields
- Remove TreeType enum, MaterialFromTreeType

**rendering.c** — 5 refs
- `GetTreeSpriteAt()`: use `wallMaterial` instead of `treeTypeGrid`
- Terrain depth sprite: use material-aware helper
- `CellSprite()` calls: review for terrain cells

**groundwear.c** — 8 refs
- Tree type selection by soil: switch on material
- Grass checks: material check
- `treeTypeGrid` clear → `SetWallMaterial(x,y,z, MAT_NONE)`

### LOW impact (1-3 refs each)

**fire.c** — grass fuel, burn result material
**sim_manager.c** — grass growth eligibility
**tooltips.c** — display name from material
**inspect.c** — save migration
**main.c** — demo terrain
**saveload.c** — remove treeTypeGrid save/load, add migration

### SAVE MIGRATION

- Bump SAVE_VERSION
- Legacy load must:
  1. Map old CellType ordinals → CELL_TERRAIN + correct material
  2. Convert treeTypeGrid values → wallMaterial for tree cells
  3. Convert treePartGrid values → new cell types (BRANCH/ROOT/FELLED)
  4. Remove wallNatural from save format
  5. Remove treeTypeGrid from save format
  6. Remove treePartGrid from save format
- Old enum values shift, so legacy struct needs hardcoded ordinal mapping

### TESTS

- ~30 soil refs in test_jobs.c, test_materials.c, test_grid_audit.c
- Tree tests that use treeTypeGrid
- All straightforward: `CELL_DIRT` → `CELL_TERRAIN` + `MAT_DIRT`

## Phase Plan

### Phase 0: Material system preparation
- Add new MaterialDef fields (terrainSprite, treeTrunkSprite, etc.)
- Populate from current cellDefs/tree sprite values
- Add `MAT_BEDROCK` with `MF_UNMINEABLE` flag
- Add position-aware helpers (`GetCellSprite`, `GetCellFuel`, etc.)
- Keep everything backward compatible — old code still works
- Tests: new helpers return correct values

### Phase 1: CELL_TERRAIN coexists with old types
- Add CELL_TERRAIN to enum (keep old types too)
- `IsGroundCell()` accepts both old types and CELL_TERRAIN
- All helpers work with both old and new
- Tests: CELL_TERRAIN behaves correctly alongside old types

### Phase 2: Migrate terrain generation + trees
- Generators use CELL_TERRAIN + SetWallMaterial instead of CELL_DIRT etc.
- Tree code uses wallMaterial instead of treeTypeGrid
- Tree code uses CELL_TREE_BRANCH/ROOT/FELLED instead of treePartGrid
- SyncMaterialsToTerrain simplifies
- Dual support means nothing breaks yet

### Phase 3: Migrate input/gameplay
- input.c, designations.c, fire.c, groundwear.c
- All soil comparisons → material checks
- All treeTypeGrid reads → GetWallMaterial
- ExecuteBuildSoil/ExecutePileSoil take only MaterialType

### Phase 4: Remove old types + grids
- Remove CELL_DIRT through CELL_ROCK + CELL_BEDROCK from enum
- Remove treeTypeGrid, TreeType enum, MaterialFromTreeType
- Remove treePartGrid, TreePart enum
- Remove wallNatural grid + accessors
- Remove MaterialForGroundCell
- `IsGroundCell()` → `cell == CELL_TERRAIN`
- NaturalTerrainSpec drops cellType param
- Save migration for old saves

### Phase 5: Ramp materials
- CELL_RAMP_* cells get wallMaterial set during placement
- Ramp sprites can vary by material
- Mining ramps drops material-appropriate items

### Phase 6: Cleanup
- Remove dual-path code
- Update all documentation
- Final test pass
