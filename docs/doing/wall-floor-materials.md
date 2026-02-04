# Separate Wall and Floor Materials

## Goal

Split the single `cellMaterial` grid into two: `wallMaterial` and `floorMaterial`. This follows Dwarf Fortress's approach where each tile has both a wall section and floor section, each with its own material.

## Why

Currently when you build a wall on a floor, the wall's material overwrites the floor's material. When you remove the wall, the floor material is lost.

With separate grids:
- Build wood wall on stone floor → wallMaterial=MAT_WOOD, floorMaterial=MAT_STONE
- Remove wood wall → wallMaterial=MAT_NONE, floorMaterial=MAT_STONE (preserved!)

## Also: Rename MAT_NATURAL to MAT_RAW

`MAT_NATURAL` is confusing. Rename to `MAT_RAW` to mean "unprocessed/rough".

For walls: `MAT_RAW` = natural rock (terrain-generated)
For floors: `MAT_RAW` = rough stone floor (exposed when mining natural rock)

## Mining Behavior (Option D)

**Mining natural rock:**
```
CELL_WALL + wallMaterial=MAT_RAW → CELL_AIR + floorMaterial=MAT_RAW (rough stone floor)
```

**Mining constructed wall:**
```
CELL_WALL + wallMaterial=MAT_STONE → CELL_AIR + floorMaterial unchanged (whatever was there)
CELL_WALL + wallMaterial=MAT_WOOD  → CELL_AIR + floorMaterial unchanged
```

**Digging dirt:**
```
CELL_DIRT → CELL_AIR + floorMaterial=MAT_DIRT (dirt floor)
```

## Future: Floor Designations

- **Smooth floor**: Convert MAT_RAW floor to MAT_STONE (mason labor, no items)
- **Remove floor**: Remove floor entirely (creates hole)
- **Build floor**: Place floor with material from item delivered

## Files to Change

### material.h
```c
// Old:
extern uint8_t cellMaterial[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
#define GetCellMaterial(x,y,z)      (cellMaterial[z][y][x])
#define SetCellMaterial(x,y,z,m)    (cellMaterial[z][y][x] = (uint8_t)(m))

// New:
extern uint8_t wallMaterial[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
extern uint8_t floorMaterial[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

#define GetWallMaterial(x,y,z)      (wallMaterial[z][y][x])
#define SetWallMaterial(x,y,z,m)    (wallMaterial[z][y][x] = (uint8_t)(m))
#define GetFloorMaterial(x,y,z)     (floorMaterial[z][y][x])
#define SetFloorMaterial(x,y,z,m)   (floorMaterial[z][y][x] = (uint8_t)(m))
```

### material.c
- Declare both grids
- InitMaterials() clears both to MAT_RAW (or MAT_NONE?)
- Update GetCellDropItem() to check wall vs floor context

### Call sites to update (~15)

| File | Function | Change |
|------|----------|--------|
| designations.c | CompleteMineDesignation | Set floorMaterial, clear wallMaterial |
| designations.c | CompleteChannelDesignation | Similar |
| designations.c | CompleteRemoveFloorDesignation | Clear floorMaterial |
| designations.c | CompleteRemoveRampDesignation | Similar |
| designations.c | CompleteBlueprint (wall) | Set wallMaterial |
| designations.c | CompleteBlueprint (floor) | Set floorMaterial |
| designations.c | CompleteBlueprint (ladder) | Set wallMaterial? |
| input.c | ExecuteBuildWall | Set wallMaterial |
| input.c | Quick edit wall | Set wallMaterial |
| fire.c | GetFuelAt | Check wallMaterial (walls burn, not floors) |
| tooltips.c | DrawCellTooltip | Show both materials |
| saveload.c | SaveWorld | Save both grids |
| saveload.c | LoadWorld | Load both grids, bump version |
| inspect.c | Load section | Load both grids |

### Materials enum update
```c
typedef enum {
    MAT_NONE = 0,    // No wall/floor present (renamed from thinking about this)
    MAT_RAW,         // Unprocessed - rough stone, natural (renamed from MAT_NATURAL)
    MAT_STONE,       // Processed stone blocks
    MAT_WOOD,        // Wood
    MAT_DIRT,        // Dirt (new)
    MAT_IRON,        // Metal
    MAT_GLASS,       // Glass
    MAT_COUNT
} MaterialType;
```

Wait - need to think about MAT_NONE vs MAT_RAW:
- `MAT_NONE` = no material (no wall exists, or no floor exists)
- `MAT_RAW` = raw/rough material (natural rock wall, rough stone floor)

So a cell with no wall would have `wallMaterial = MAT_NONE`.
A natural rock wall would have `wallMaterial = MAT_RAW`.

## Test Updates

tests/test_materials.c needs updates for:
- New grid names
- MAT_RAW rename
- Wall vs floor material tests

## Save Version

Bump SAVE_VERSION (currently 11 → 12) since grid format changes.

## Estimated Scope

~20 call sites, ~30 minutes of work. Medium refactor.
