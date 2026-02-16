# world/

Grid, cells, materials, pathfinding, terrain generation, designations, construction.

## Three-Axis Material System

Every cell has three independent axes:
1. **Shape** — `CellType` in `grid[z][y][x]` (CELL_WALL, CELL_RAMP, CELL_TREE_TRUNK, etc.)
2. **Substance** — `MaterialType` in `wallMaterial[z][y][x]` / `floorMaterial[z][y][x]`
3. **Form** — natural vs constructed (`wallNatural[z][y][x]`), source item (`wallSourceItem[z][y][x]`)

Accessors: `GetWallMaterial(x,y,z)`, `SetWallMaterial(x,y,z,m)`, `IsWallNatural(x,y,z)`, etc.

## Parallel Grids

All grids are `[z][y][x]` indexed. The full set:
`grid`, `cellFlags`, `wallMaterial`, `floorMaterial`, `wallNatural`, `floorNatural`, `wallFinish`, `floorFinish`, `wallSourceItem`, `floorSourceItem`, `vegetationGrid`, `snowGrid`, `waterGrid`, `fireGrid`, `smokeGrid`, `steamGrid`, `temperatureGrid`, `designations`, `wearGrid`, `growthTimer`, `targetHeight`, `treeHarvestState`, `floorDirtGrid`

## DF-Style Walkability

Movers stand ON TOP of solid ground, not inside it:
```
z=2: [AIR]  <- walkable (z=1 is solid below)
z=1: [DIRT] <- solid
z=0: [DIRT] <- solid
```

`IsCellWalkableAt_Standard()` in `cell_defs.h` checks: not blocked, not solid, has support below (solid, HAS_FLOOR, ladder, ramp, or z=0).

## Cell Flags (bitfield in `cellFlags[z][y][x]`)

- `CF_WALL`, `CF_SOLID` — most systems check these flags, not specific CellTypes
- 2-bit wetness: `GET_CELL_WETNESS` / `SET_CELL_WETNESS` (0-3: dry/damp/wet/soaked)
- 2-bit surface: `GET_CELL_SURFACE` (SURFACE_BARE / SURFACE_TRAMPLED)
- Single bits: `CELL_FLAG_BURNED`, `HAS_FLOOR`, `CELL_FLAG_WORKSHOP_BLOCK`

## Sprite Overrides

`spriteOverrides[CellType][MaterialType]` in `material.c` — maps (cell, material) to sprite index. Check this table first when debugging wrong sprites.

## Construction (Recipe System)

All construction uses `CreateRecipeBlueprint()` + `ConstructionRecipe` table.
- 10 recipes: dry stone wall, wattle & daub, plank wall, log wall, brick wall, plank floor, brick floor, thatch floor, ladder, ramp
- Blueprint struct: `recipeIndex`, `stage`, `stageDeliveries[]`, `consumedItems[]`
- Recipe cycling: M key per category, tracked in `input.c`

## Designation Patterns

- On **walkable** cell (gather grass): use `OnTileDesignationEntry` — mover stands on the cell
- On **non-walkable** cell (mine wall, gather sapling): use `AdjacentDesignationEntry` — mover stands next to it

## Gotchas

- After terrain generation, call `SyncMaterialsToTerrain()` or walls have `MAT_NONE`
- `IsRampStillValid()` must be called after terrain changes (high side needs solid support)
- `GetWallDropItem()` checks `wallSourceItem` grid to return correct item type on mining
- Vegetation (`vegetationGrid`) is separate from surface bits (`CELL_SURFACE`) — don't confuse them
