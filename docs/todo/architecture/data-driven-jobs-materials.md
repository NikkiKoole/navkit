# Data-Driven Jobs & Materials (Revised)

Separate **cell type** (what a tile IS) from **material** (what it's MADE OF), following Dwarf Fortress's approach. Currently these are conflated (e.g., `CELL_WALL` vs `CELL_WOOD_WALL`), and job outputs are hardcoded.

## Background: How Dwarf Fortress Does It

DF separates tile type from material using two systems:

1. **Tiletype**: What the cell IS (wall, floor, ramp, etc.) - stored per-tile
2. **Material**: What it's MADE OF - stored separately, looked up when needed

For **natural tiles**, material comes from geology (layer stone, veins).
For **constructed tiles**, material is stored in a `construction` struct with `mat_type`/`mat_index`.

DF uses two integers (`mat_type` + `mat_index`) to support thousands of materials. NavKit can use a simpler single `uint8_t` since we have fewer materials.

**Key insight**: Designations in DF are just **bitflags per tile** (`tile_designation.dig`), not separate structs. Jobs are created dynamically when dwarves pick up work. NavKit's current `Designation` struct approach is actually more flexible for tracking progress/assignment.

## Goals

1. Cell type (CELL_WALL) separate from material (stone, wood, iron)
2. Materials determine visual appearance and drops
3. Cells know what they drop when mined (based on material, not cell type)
4. Items know what material they produce when used for building
5. Single source of truth for work times

## Current Problems

| Location | Problem |
|----------|---------|
| `designations.c:128` | Mining always drops `ITEM_ORANGE` regardless of what was mined |
| `designations.c:721` | Chopping hardcodes `ITEM_WOOD` |
| `CompleteBlueprint()` | Always builds `CELL_WALL`, ignores material used |
| `CELL_WALL` vs `CELL_WOOD_WALL` | Material encoded in cell type (doesn't scale) |
| `designations.h` | Work times scattered as `#define`s |

---

## Phase 1: Add Material Grid

Create a separate grid to track what material each cell is made of.

### New files: `src/world/material.h` and `src/world/material.c`

```c
// material.h
#ifndef MATERIAL_H
#define MATERIAL_H

#include <stdint.h>
#include "grid.h"                  // For MAX_GRID_* constants
#include "../entities/items.h"    // For ItemType

// Material types - what a cell/item is made of
typedef enum {
    MAT_NATURAL = 0,   // Use cell type to determine (dirt=dirt, wall=stone, tree=wood)
    MAT_STONE,         // Generic stone (from stone blocks)
    MAT_WOOD,          // Wood material
    MAT_IRON,          // Metal
    MAT_GLASS,         // Future
    // ... add more as needed
    MAT_COUNT
} MaterialType;

// Material flags
#define MF_FLAMMABLE  (1 << 0)  // Can catch fire

// Material definitions
typedef struct {
    const char* name;
    int spriteOffset;     // Offset to add to base cell sprite (0 = default)
    uint8_t flags;        // MF_* flags (flammable, etc.)
    uint8_t fuel;         // Fuel value for fire system (0 = won't burn)
    ItemType dropsItem;   // What item this material drops when deconstructed
} MaterialDef;

extern MaterialDef materialDefs[];
extern uint8_t cellMaterial[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Accessors
#define GetCellMaterial(x,y,z)      (cellMaterial[z][y][x])
#define SetCellMaterial(x,y,z,m)    (cellMaterial[z][y][x] = (uint8_t)(m))
#define IsConstructedCell(x,y,z)    (cellMaterial[z][y][x] != MAT_NATURAL)

// Material property accessors
#define MaterialName(m)         (materialDefs[m].name)
#define MaterialSpriteOffset(m) (materialDefs[m].spriteOffset)
#define MaterialDropsItem(m)    (materialDefs[m].dropsItem)
#define MaterialFuel(m)         (materialDefs[m].fuel)
#define MaterialIsFlammable(m)  (materialDefs[m].flags & MF_FLAMMABLE)

void InitMaterials(void);

#endif
```

```c
// material.c
#include "material.h"
#include "grid.h"
#include "cell_defs.h"    // For CellDropsItem()
#include <string.h>

uint8_t cellMaterial[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

MaterialDef materialDefs[MAT_COUNT] = {
    //                 name       spriteOffset  flags         fuel  dropsItem
    [MAT_NATURAL] = {"natural",  0,            0,            0,    ITEM_NONE},        // Use CellDropsItem instead
    [MAT_STONE]   = {"stone",    0,            0,            0,    ITEM_STONE_BLOCKS},
    [MAT_WOOD]    = {"wood",     1,            MF_FLAMMABLE, 128,  ITEM_WOOD},        // spriteOffset=1 for wood variant
    [MAT_IRON]    = {"iron",     2,            0,            0,    ITEM_STONE_BLOCKS}, // TODO: ITEM_IRON_BAR when added
    [MAT_GLASS]   = {"glass",    3,            0,            0,    ITEM_STONE_BLOCKS}, // TODO: ITEM_GLASS when added
};

void InitMaterials(void) {
    memset(cellMaterial, MAT_NATURAL, sizeof(cellMaterial));
}
```

### Files to modify
- Create `src/world/material.h`
- Create `src/world/material.c`
- Update `src/unity.c` - include new .c file
- Update `src/world/grid.c` - call `InitMaterials()` in grid init
- Update `src/core/saveload.c` - save/load `cellMaterial` grid

---

## Phase 2: Extend CellDef with Drop Info

Add fields so cells know what they drop when destroyed. For `MAT_NATURAL` cells, this defines the default drop.

### First: Add `ITEM_NONE` to `src/entities/items.h`

```c
typedef enum {
    ITEM_NONE = -1,    // NEW: No item (sentinel value)
    ITEM_RED,
    ITEM_GREEN,
    // ... rest unchanged
} ItemType;
```

**Note**: Using `-1` works because `ItemType` is an `enum` (typically `int`). If you ever store `ItemType` in a `uint8_t`, use a different sentinel like `255` or `ITEM_COUNT`. The current codebase uses `ItemType` directly, so `-1` is safe.

### Update `src/world/cell_defs.h`

Add include and new fields:

```c
#include "../entities/items.h"    // NEW: For ItemType

typedef struct {
    const char* name;
    int sprite;
    uint8_t flags;
    uint8_t insulationTier;
    uint8_t fuel;
    CellType burnsInto;
    ItemType dropsItem;    // NEW: What item when mined (ITEM_NONE if nothing)
    uint8_t dropCount;     // NEW: Base drop count
} CellDef;

#define CellDropsItem(c)  (cellDefs[c].dropsItem)
#define CellDropCount(c)  (cellDefs[c].dropCount)
```

### Update `src/world/cell_defs.c`

```c
CellDef cellDefs[] = {
    // Existing fields...                                    + dropsItem,     dropCount
    [CELL_DIRT]       = {"dirt",       SPRITE_dirt,       ..., ITEM_NONE,       0},
    [CELL_WALL]       = {"wall",       SPRITE_wall,       ..., ITEM_ORANGE,     1},  // Default drop for MAT_NATURAL walls
    [CELL_TREE_TRUNK] = {"tree trunk", SPRITE_tree_trunk, ..., ITEM_WOOD,       1},  // Per trunk cell
    [CELL_SAPLING]    = {"sapling",    SPRITE_tree_sapling,..., ITEM_SAPLING,   1},
    [CELL_AIR]        = {"air",        SPRITE_air,        ..., ITEM_NONE,       0},
    [CELL_LADDER_BOTH]= {"ladder",     SPRITE_ladder,     ..., ITEM_STONE_BLOCKS, 1},
    [CELL_RAMP_N]     = {"ramp north", SPRITE_ramp_n,     ..., ITEM_ORANGE,     1},
    // ... etc
    // Note: CELL_WOOD_WALL removed - use CELL_WALL + MAT_WOOD instead
};
```

### Files to modify
- Update `src/entities/items.h` - add `ITEM_NONE = -1` to enum
- Update `src/world/cell_defs.h` - add include, add `dropsItem`, `dropCount` fields and accessors
- Update `src/world/cell_defs.c` - populate new fields for all cell types

---

## Phase 3: Extend ItemDef with Material Production

Add field so items know what material they produce when used for construction.

### Update `src/entities/item_defs.h`

```c
#include "../world/material.h"

typedef struct {
    const char* name;
    int sprite;
    uint8_t flags;
    uint8_t maxStack;
    MaterialType producesMaterial;  // NEW: What material when used to build
} ItemDef;

#define ItemProducesMaterial(t) (itemDefs[t].producesMaterial)
```

### Update `src/entities/item_defs.c`

```c
const ItemDef itemDefs[ITEM_TYPE_COUNT] = {
    //                                                        + producesMaterial
    [ITEM_RED]          = {"Red Crate",    ..., IF_STACKABLE,                10, MAT_NATURAL},
    [ITEM_GREEN]        = {"Green Crate",  ..., IF_STACKABLE,                10, MAT_NATURAL},
    [ITEM_BLUE]         = {"Blue Crate",   ..., IF_STACKABLE,                10, MAT_NATURAL},
    [ITEM_ORANGE]       = {"Raw Stone",    ..., IF_STACKABLE,                20, MAT_STONE},
    [ITEM_STONE_BLOCKS] = {"Stone Blocks", ..., IF_STACKABLE|IF_BUILDING_MAT,20, MAT_STONE},
    [ITEM_WOOD]         = {"Wood",         ..., IF_STACKABLE|IF_BUILDING_MAT|IF_FUEL, 20, MAT_WOOD},
    [ITEM_SAPLING]      = {"Sapling",      ..., IF_STACKABLE,                20, MAT_NATURAL},
};
```

### Files to modify
- Update `src/entities/item_defs.h` - add `producesMaterial` field and accessor
- Update `src/entities/item_defs.c` - populate new field

---

## Phase 4: Track Material in Blueprint

Track what material was delivered, and what cell existed before construction.

### Update `src/world/designations.h`

```c
typedef struct {
    int x, y, z;
    bool active;
    BlueprintState state;
    BlueprintType type;
    
    int requiredMaterials;
    int deliveredMaterials;
    int reservedItem;
    
    int assignedBuilder;
    float progress;
    
    // NEW field
    MaterialType deliveredMaterial;  // What material was delivered (for cell material)
} Blueprint;
```

### Files to modify
- Update `src/world/designations.h` - add new fields to Blueprint
- Update `src/world/designations.c` - initialize new fields in `Create*Blueprint()`
- Update `src/core/saveload.c` - save/load new Blueprint fields

---

## Phase 5: Wire Up Construction

Update blueprint completion to set material and track original cell.

### Update `CreateBuildBlueprint()` etc. in `designations.c`

```c
int CreateBuildBlueprint(int x, int y, int z) {
    // ... existing code ...
    
    bp->deliveredMaterial = MAT_NATURAL;  // Will be set when material delivered
    
    // ... rest of function
}
```

### Update `DeliverMaterialToBlueprint()` in `designations.c`

```c
void DeliverMaterialToBlueprint(int blueprintIdx, int itemIdx) {
    // ... existing code ...
    
    // Record the material type before deleting item
    Blueprint* bp = &blueprints[blueprintIdx];
    bp->deliveredMaterial = ItemProducesMaterial(items[itemIdx].type);
    
    DeleteItem(itemIdx);
    // ... rest of function
}
```

### Update `CompleteBlueprint()` in `designations.c`

```c
void CompleteBlueprint(int blueprintIdx) {
    Blueprint* bp = &blueprints[blueprintIdx];
    int x = bp->x, y = bp->y, z = bp->z;
    
    if (bp->type == BLUEPRINT_TYPE_WALL) {
        PushMoversOutOfCell(x, y, z);
        PushItemsOutOfCell(x, y, z);
        DisplaceWater(x, y, z);
        
        grid[z][y][x] = CELL_WALL;  // Always CELL_WALL now
        SetCellMaterial(x, y, z, bp->deliveredMaterial);  // Material determines appearance
        MarkChunkDirty(x, y, z);
        
    } else if (bp->type == BLUEPRINT_TYPE_LADDER) {
        PlaceLadder(x, y, z);
        SetCellMaterial(x, y, z, bp->deliveredMaterial);
        
    } else if (bp->type == BLUEPRINT_TYPE_FLOOR) {
        DisplaceWater(x, y, z);
        grid[z][y][x] = CELL_AIR;
        SET_FLOOR(x, y, z);
        SetCellMaterial(x, y, z, bp->deliveredMaterial);
        MarkChunkDirty(x, y, z);
    }
    
    // ... cleanup
}
```

### Update `InitDesignations()` in `designations.c`

```c
void InitDesignations(void) {
    // ... existing code ...
    
    for (int i = 0; i < MAX_BLUEPRINTS; i++) {
        blueprints[i].assignedBuilder = -1;
        blueprints[i].reservedItem = -1;
        blueprints[i].deliveredMaterial = MAT_NATURAL;  // NEW
    }
    // ...
}
```

### Files to modify
- Update `src/world/designations.c`:
  - Update `DeliverMaterialToBlueprint()` to record material
  - Update `CompleteBlueprint()` to use material
  - Update `InitDesignations()` to init new field
  - Update `Create*Blueprint()` functions to init new field

---

## Phase 6: Wire Up Mining/Deconstruction

Update mining and deconstruction to use cell material for drops.

### Update `DesignateMine()` to accept all wall types

Currently `DesignateMine()` only accepts `CELL_WALL`:

```c
// OLD - too restrictive
if (grid[z][y][x] != CELL_WALL) {
    return false;
}
```

**Note**: We can't just use `CellBlocksMovement()` because that includes:
- `CELL_TREE_TRUNK` (should be chopped, not mined)
- `CELL_TREE_LEAVES` (should decay or be chopped)
- `CELL_BEDROCK` (should be unmineable)

**With the new material system, there's only one wall type**: `CELL_WALL`. The material (stone, wood, iron) is stored separately in `cellMaterial`. So:

```c
// NEW - simple, just check for CELL_WALL
if (grid[z][y][x] != CELL_WALL) {
    return false;
}
```

The drop item is determined by `GetCellDropItem()` which checks `cellMaterial`.

### Helper function in `material.c`

```c
// Get what item a cell drops based on its material
// For natural cells, use CellDropsItem. For constructed, use MaterialDropsItem.
ItemType GetCellDropItem(int x, int y, int z) {
    MaterialType mat = GetCellMaterial(x, y, z);
    
    if (mat == MAT_NATURAL) {
        // Natural cell - use cell type's default drop
        return CellDropsItem(grid[z][y][x]);
    }
    
    // Constructed cell - drop based on material (data-driven, no switch needed)
    return MaterialDropsItem(mat);
}
```

### Update `CompleteMineDesignation()` in `designations.c`

```c
void CompleteMineDesignation(int x, int y, int z) {
    CellType oldCell = grid[z][y][x];
    ItemType dropItem = GetCellDropItem(x, y, z);
    int dropCount = CellDropCount(oldCell);
    
    // Convert to air with floor
    grid[z][y][x] = CELL_AIR;
    SET_FLOOR(x, y, z);
    SetCellMaterial(x, y, z, MAT_NATURAL);  // Reset material
    MarkChunkDirty(x, y, z);
    
    // Spawn drops
    if (dropItem != ITEM_NONE && dropCount > 0) {
        for (int i = 0; i < dropCount; i++) {
            SpawnItem(x * CELL_SIZE + CELL_SIZE * 0.5f, 
                      y * CELL_SIZE + CELL_SIZE * 0.5f, 
                      (float)z, dropItem);
        }
    }
    
    // ... rest of function (water destabilize, clear designation, validate ramps)
}
```

Similarly update:
- `CompleteChannelDesignation()` - spawns debris from floor and mined material
- `CompleteRemoveFloorDesignation()` - use `GetCellDropItem()`
- `CompleteRemoveRampDesignation()` - use `GetCellDropItem()`
- `CompleteChopDesignation()` - already uses ITEM_WOOD, but could check material

### Files to modify
- Update `src/world/material.c` - add `GetCellDropItem()` helper
- Update `src/world/material.h` - declare helper
- Update `src/world/designations.c`:
  - `DesignateMine()` stays as `== CELL_WALL` check (unchanged, but now correct)
  - Use `GetCellDropItem()` helper in all `Complete*Designation()` functions

---

## Phase 7: Consolidate Work Times (Optional)

Move work time defines to a simple array for easy balancing. This is optional - the defines work fine.

### Option A: Keep defines (current)
```c
#define MINE_WORK_TIME 2.0f
#define CHANNEL_WORK_TIME 2.0f
// etc.
```

### Option B: Array lookup
```c
// In designations.h or new file
static const float designationWorkTimes[] = {
    [DESIGNATION_NONE]           = 0.0f,
    [DESIGNATION_MINE]           = 2.0f,
    [DESIGNATION_CHANNEL]        = 2.0f,
    [DESIGNATION_REMOVE_FLOOR]   = 1.0f,
    [DESIGNATION_REMOVE_RAMP]    = 1.0f,
    [DESIGNATION_CHOP]           = 3.0f,
    [DESIGNATION_GATHER_SAPLING] = 1.0f,
    [DESIGNATION_PLANT_SAPLING]  = 1.5f,
};

#define DesignationWorkTime(d) (designationWorkTimes[d])
```

Then update `jobs.c` to use `DesignationWorkTime(designation->type)` instead of specific defines.

---

## Phase 8: Update Fire System for Material-Based Flammability

**Critical**: This must happen BEFORE removing `CELL_WOOD_WALL` in Phase 10.

Currently, fire uses `CellFuel(cell)` from `cell_defs.c`:
- `CELL_WOOD_WALL` has fuel=128 (burns)
- `CELL_WALL` has fuel=0 (doesn't burn)

After this phase, flammability is determined by material, not cell type.

**Note**: The `MaterialDef` struct already has `fuel` and `MF_FLAMMABLE` from Phase 1. This phase wires up the fire system to use them.

### Update `GetFuelAt()` in `src/simulation/fire.c`

```c
static int GetFuelAt(int x, int y, int z) {
    CellType cell = grid[z][y][x];
    int baseFuel = CellFuel(cell);
    
    // Check material for constructed cells
    MaterialType mat = GetCellMaterial(x, y, z);
    if (mat != MAT_NATURAL) {
        // Constructed cell - use material's fuel value
        baseFuel = MaterialFuel(mat);
    }
    
    // Grass surface on dirt adds extra fuel (existing logic)
    if (cell == CELL_DIRT || cell == CELL_AIR) {
        if (HAS_SURFACE(x, y, z)) {
            uint8_t surface = GetSurface(x, y, z);
            if (surface == SURFACE_GRASS) {
                baseFuel = 16;
            }
        }
    }
    
    return baseFuel;
}
```

### Files to modify
- Update `src/world/material.h` - add `MF_FLAMMABLE`, `fuel` field, accessors
- Update `src/world/material.c` - populate `fuel` values
- Update `src/simulation/fire.c` - modify `GetFuelAt()` to check material
- Add `#include "../world/material.h"` to `fire.c`

---

## Phase 9: Update Rendering (If Needed)

If you want different materials to render differently (stone wall vs wood wall), update rendering to check material.

```c
// In rendering.c
int GetCellSprite(int x, int y, int z) {
    CellType cell = grid[z][y][x];
    int baseSprite = CellSprite(cell);
    
    if (IsConstructedCell(x, y, z)) {
        MaterialType mat = GetCellMaterial(x, y, z);
        baseSprite += MaterialSpriteOffset(mat);
    }
    
    return baseSprite;
}
```

This allows `MAT_WOOD` to add an offset to the base wall sprite to show a wood texture.

---

## Phase 10: Remove CELL_WOOD_WALL

With the material system in place, `CELL_WOOD_WALL` is redundant. Remove it.

### Update `src/world/grid.h`
```c
typedef enum { 
    CELL_WALL, 
    CELL_AIR, 
    CELL_LADDER_UP,
    CELL_LADDER_DOWN,
    CELL_LADDER_BOTH,
    CELL_DIRT,
    // CELL_WOOD_WALL removed - use CELL_WALL + MAT_WOOD instead
    CELL_BEDROCK,
    // ... rest unchanged
} CellType;
```

### Update `src/world/cell_defs.c`
Remove the `[CELL_WOOD_WALL]` entry entirely.

### Search and replace any remaining `CELL_WOOD_WALL` usage
- If terrain generation creates wood walls, change to `CELL_WALL` + `SetCellMaterial(x, y, z, MAT_WOOD)`
- If fire simulation checks for `CELL_WOOD_WALL`, check `GetCellMaterial() == MAT_WOOD` instead

### Files to modify
- Update `src/world/grid.h` - remove `CELL_WOOD_WALL` from enum
- Update `src/world/cell_defs.c` - remove `CELL_WOOD_WALL` entry
- Search codebase for `CELL_WOOD_WALL` and update all usages

---

## Testing

1. **Build wall with stone blocks** → `CELL_WALL` + `MAT_STONE`
2. **Build wall with wood** → `CELL_WALL` + `MAT_WOOD`  
3. **Mine natural stone wall** → drops `ITEM_ORANGE` (from `CellDropsItem`)
4. **Mine wood wall** → drops `ITEM_WOOD` (from `MaterialDropsItem`)
5. **Mine `CELL_BEDROCK`** → should fail (not mineable)
6. **Mine `CELL_TREE_TRUNK`** → should fail (use chop instead)
7. **Deconstruct floor** → drops item based on material used to build it
8. **Wood wall burns** → fire checks `MaterialFuel(GetCellMaterial()) > 0`
9. **Stone wall doesn't burn** → `MaterialFuel(MAT_STONE) == 0`
10. **No references to `CELL_WOOD_WALL`** → grep returns nothing
11. **All existing tests pass**

---

## Summary: What Changed from Original Plan

| Original Plan | Revised Plan |
|---------------|--------------|
| 4 new files (`designation_defs`, `blueprint_defs`) | 2 new files (`material.h/c`) |
| `ItemDef.buildsWall`, `buildsLadder` | `ItemDef.producesMaterial` |
| Material encoded in cell type | Separate `cellMaterial` grid |
| Complex designation defs | Keep simple `#define` work times |
| Hardcoded switch for material drops | Data-driven `MaterialDef.dropsItem` |

The key insight from DF: **separate what a cell IS from what it's MADE OF**. This scales better than encoding material in cell type.

---

## Future Possibilities

Once this is in place:
- Add new materials easily (just add to `MaterialType` enum and `materialDefs`)
- Material-specific properties (wood burns, stone doesn't) via `MaterialDef.flags`
- Visual variety (different sprites per material)
- Deconstruction returns the exact material used
- Query "what's made of wood?" by scanning material grid
- Geology system for natural materials (different stone types per region)

---

# Staged Implementation Plan

This section breaks down the implementation into testable stages. Each stage should leave the codebase in a working state with all existing tests passing.

## Stage Overview

| Stage | Phases | Description | Risk Level | Existing Tests Affected |
|-------|--------|-------------|------------|-------------------------|
| A | 1 | Add material infrastructure (no behavior changes) | Low | None |
| B | 2, 3, 4 | Extend defs with new fields (no behavior changes) | Low | None |
| C | 5 | Wire construction to use materials | Medium | None |
| D | 6 | Wire mining to use material drops | Medium | test_jobs.c (if any mine tests) |
| E | 8 | Update fire to check material instead of CELL_WOOD_WALL | Medium | test_fire.c, test_temperature.c |
| F | 10 | Remove CELL_WOOD_WALL | High | All files referencing it |

**Note**: Phase 7 (Work Times) and Phase 9 (Rendering) are optional and can be done anytime after their dependencies are met.

---

## Stage A: Material Infrastructure (Foundation)

**Goal**: Add material grid and definitions without changing any existing behavior.

### Tasks
1. Add `ITEM_NONE = -1` to `ItemType` enum in `items.h`
2. Create `src/world/material.h` with `MaterialType` enum and `MaterialDef` struct
3. Create `src/world/material.c` with `cellMaterial` grid and `InitMaterials()`
4. Add `#include "material.c"` to `unity.c`
5. Call `InitMaterials()` from grid initialization (in `InitGrid()` or similar)
6. Update `saveload.c` to save/load `cellMaterial` grid

### What's NOT wired up yet
- Nothing uses `cellMaterial` yet
- No behavior changes

### Test Checkpoint A
```
✓ All existing tests pass (no behavior changed)
✓ Game compiles and runs
✓ Manual: Save game, load game - no crashes
✓ New unit test: InitMaterials() sets all cells to MAT_NATURAL
```

**New test to add** (`test_materials.c`):
```c
describe(material_grid_initialization) {
    it("should initialize all cells to MAT_NATURAL") {
        InitGridFromAsciiWithChunkSize("....\n", 4, 1);
        InitMaterials();
        
        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                for (int x = 0; x < gridWidth; x++) {
                    expect(GetCellMaterial(x, y, z) == MAT_NATURAL);
                }
            }
        }
    }
}
```

---

## Stage B: Extend Definitions (No Behavior Changes)

**Goal**: Add new fields to CellDef, ItemDef, and Blueprint without using them yet.

### Tasks
1. Add `dropsItem` and `dropCount` to `CellDef` struct
2. Populate new fields in `cell_defs.c` for all cell types
3. Add `producesMaterial` to `ItemDef` struct
4. Populate new field in `item_defs.c` for all item types
5. Add `deliveredMaterial` to `Blueprint` struct
6. Initialize `deliveredMaterial = MAT_NATURAL` in `InitDesignations()` and `Create*Blueprint()`
7. Update `saveload.c` for new Blueprint field

### What's NOT wired up yet
- New fields exist but aren't read/used anywhere
- Old hardcoded behavior still in place

### Test Checkpoint B
```
✓ All existing tests pass (no behavior changed)
✓ Game compiles and runs
✓ Manual: Create blueprints, build walls - works as before
✓ New unit test: CellDropsItem(CELL_WALL) returns expected value
✓ New unit test: ItemProducesMaterial(ITEM_WOOD) == MAT_WOOD
```

**New tests to add**:
```c
describe(cell_def_drops) {
    it("should have dropsItem for wall") {
        expect(CellDropsItem(CELL_WALL) == ITEM_ORANGE);  // or whatever default
    }
    
    it("should have dropsItem for tree trunk") {
        expect(CellDropsItem(CELL_TREE_TRUNK) == ITEM_WOOD);
    }
    
    it("should have ITEM_NONE for air") {
        expect(CellDropsItem(CELL_AIR) == ITEM_NONE);
    }
}

describe(item_def_materials) {
    it("should produce MAT_WOOD from wood item") {
        expect(ItemProducesMaterial(ITEM_WOOD) == MAT_WOOD);
    }
    
    it("should produce MAT_STONE from stone blocks") {
        expect(ItemProducesMaterial(ITEM_STONE_BLOCKS) == MAT_STONE);
    }
}
```

---

## Stage C: Wire Construction (Building)

**Goal**: Construction now sets cell material based on delivered item.

### Tasks
1. Update `DeliverMaterialToBlueprint()` to record `bp->deliveredMaterial = ItemProducesMaterial(item.type)`
2. Update `CompleteBlueprint()` to call `SetCellMaterial(x, y, z, bp->deliveredMaterial)`
3. Add `GetCellDropItem()` helper to `material.c` (for Stage D, but implement now)

### Behavior Change
- Walls built with wood now have `MAT_WOOD` in material grid
- Walls built with stone have `MAT_STONE` in material grid
- **Visual appearance unchanged** (rendering not updated yet)

### Test Checkpoint C
```
✓ All existing tests pass
✓ Game compiles and runs
✓ New unit test: Build wall with wood → GetCellMaterial returns MAT_WOOD
✓ New unit test: Build wall with stone → GetCellMaterial returns MAT_STONE
✓ Manual: Build walls, verify via debug inspect that material is set correctly
```

**New tests to add**:
```c
describe(blueprint_material_tracking) {
    it("should set wood material when building with wood") {
        // Setup: create blueprint, deliver wood item, complete it
        InitGridFromAsciiWithChunkSize("....\n", 4, 1);
        InitMaterials();
        InitDesignations();
        InitItems();
        
        int bp = CreateBuildBlueprint(1, 0, 0);
        expect(bp >= 0);
        
        // Spawn and deliver wood
        int item = SpawnItem(0, 0, 0, ITEM_WOOD);
        DeliverMaterialToBlueprint(bp, item);
        
        // Complete (simulate worker finishing)
        Blueprint* blueprint = GetBlueprint(bp);
        blueprint->progress = BUILD_WORK_TIME;
        CompleteBlueprint(bp);
        
        // Verify
        expect(grid[0][0][1] == CELL_WALL);
        expect(GetCellMaterial(1, 0, 0) == MAT_WOOD);
    }
    
    it("should set stone material when building with stone blocks") {
        // Similar test with ITEM_STONE_BLOCKS
        // ...
        expect(GetCellMaterial(1, 0, 0) == MAT_STONE);
    }
}
```

---

## Stage D: Wire Mining/Deconstruction (Drops)

**Goal**: Mining/deconstructing now drops items based on material, not just cell type.

### Tasks
1. Update `CompleteMineDesignation()` to use `GetCellDropItem()` and reset material
2. Update `CompleteChannelDesignation()` similarly
3. Update `CompleteRemoveFloorDesignation()` similarly
4. Update `CompleteRemoveRampDesignation()` similarly
5. Ensure all these reset `SetCellMaterial(x, y, z, MAT_NATURAL)` after destruction

### Behavior Change
- Mining a natural wall → drops ITEM_ORANGE (unchanged)
- Mining a wood wall (MAT_WOOD) → drops ITEM_WOOD (new!)
- Mining a stone wall (MAT_STONE) → drops ITEM_STONE_BLOCKS (new!)

### Test Checkpoint D
```
✓ All existing tests pass (mine tests should still work)
✓ Game compiles and runs
✓ New unit test: Mine natural wall → drops ITEM_ORANGE
✓ New unit test: Mine MAT_WOOD wall → drops ITEM_WOOD
✓ New unit test: Mine MAT_STONE wall → drops ITEM_STONE_BLOCKS
✓ New unit test: After mining, material resets to MAT_NATURAL
✓ Manual: Build wood wall, mine it, verify wood drops
```

**New tests to add**:
```c
describe(mining_material_drops) {
    it("should drop based on natural cell type for MAT_NATURAL") {
        InitGridFromAsciiWithChunkSize("#...\n", 4, 1);  // # = wall
        InitMaterials();
        InitItems();
        
        // Wall is natural (MAT_NATURAL)
        expect(GetCellMaterial(0, 0, 0) == MAT_NATURAL);
        
        CompleteMineDesignation(0, 0, 0);
        
        // Should have spawned ITEM_ORANGE (default wall drop)
        int itemCount = CountItemsOfType(ITEM_ORANGE);
        expect(itemCount > 0);
    }
    
    it("should drop ITEM_WOOD when mining MAT_WOOD wall") {
        InitGridFromAsciiWithChunkSize("#...\n", 4, 1);
        InitMaterials();
        InitItems();
        
        // Set wall to wood material
        SetCellMaterial(0, 0, 0, MAT_WOOD);
        
        CompleteMineDesignation(0, 0, 0);
        
        // Should have spawned ITEM_WOOD
        int itemCount = CountItemsOfType(ITEM_WOOD);
        expect(itemCount > 0);
    }
    
    it("should reset material to MAT_NATURAL after mining") {
        InitGridFromAsciiWithChunkSize("#...\n", 4, 1);
        InitMaterials();
        SetCellMaterial(0, 0, 0, MAT_WOOD);
        
        CompleteMineDesignation(0, 0, 0);
        
        expect(GetCellMaterial(0, 0, 0) == MAT_NATURAL);
    }
}
```

---

## Stage E: Update Fire System (Material-Based Flammability)

**Goal**: Fire checks material grid instead of CELL_WOOD_WALL for flammability.

### Background
Currently, fire uses `CellFuel(cell)` from `cell_defs.c`:
- `CELL_WOOD_WALL` has fuel=128
- `CELL_WALL` has fuel=0

After this stage:
- `CELL_WALL` with `MAT_WOOD` should burn
- `CELL_WALL` with `MAT_STONE` should not burn

### Tasks
1. Add `MF_FLAMMABLE` flag and `fuel` to `MaterialDef`
2. Update `GetFuelAt()` in `fire.c` to check material:
   ```c
   static int GetFuelAt(int x, int y, int z) {
       CellType cell = grid[z][y][x];
       int baseFuel = CellFuel(cell);
       
       // Constructed cells get fuel from material
       MaterialType mat = GetCellMaterial(x, y, z);
       if (mat != MAT_NATURAL && MaterialIsFlammable(mat)) {
           baseFuel = MaterialFuel(mat);
       }
       
       // Grass overlay logic...
       return baseFuel;
   }
   ```
3. Set `MAT_WOOD` to have fuel (e.g., 128) and flammable flag
4. Set `MAT_STONE`, `MAT_IRON` to have fuel=0, not flammable

### Critical: Update tests BEFORE removing CELL_WOOD_WALL
- `test_temperature.c:265` - currently just a placeholder, update to test material-based insulation
- Any fire tests using `CELL_WOOD_WALL` - convert to `CELL_WALL` + `MAT_WOOD`

### Test Checkpoint E
```
✓ All existing tests pass (after updating fire tests)
✓ Game compiles and runs
✓ New unit test: CELL_WALL + MAT_WOOD burns (has fuel)
✓ New unit test: CELL_WALL + MAT_STONE doesn't burn (no fuel)
✓ Manual: Build wood wall, set it on fire, verify it burns
✓ Manual: Build stone wall, try to ignite, verify it doesn't burn
```

**Tests to update**:
```c
// In test_fire.c, if any tests use CELL_WOOD_WALL, change them:
// BEFORE:
grid[0][0][1] = CELL_WOOD_WALL;
// AFTER:
grid[0][0][1] = CELL_WALL;
SetCellMaterial(1, 0, 0, MAT_WOOD);
```

**New tests to add**:
```c
describe(material_flammability) {
    it("should burn CELL_WALL with MAT_WOOD") {
        InitGridFromAsciiWithChunkSize("....\n", 4, 1);
        InitMaterials();
        InitFire();
        
        grid[0][0][1] = CELL_WALL;
        SetCellMaterial(1, 0, 0, MAT_WOOD);
        
        // Note: GetFuelAt() is static, so test via IgniteCell behavior
        // Wood material should be ignitable
        IgniteCell(1, 0, 0);
        expect(HasFire(1, 0, 0));
        
        // Run fire simulation and verify it burns
        RunFireTicks(10);
        expect(HasFire(1, 0, 0));  // Still burning (has fuel)
    }
    
    it("should NOT burn CELL_WALL with MAT_STONE") {
        InitGridFromAsciiWithChunkSize("....\n", 4, 1);
        InitMaterials();
        InitFire();
        
        grid[0][0][1] = CELL_WALL;
        SetCellMaterial(1, 0, 0, MAT_STONE);
        
        // Stone material should NOT be ignitable (no fuel)
        IgniteCell(1, 0, 0);
        expect(!HasFire(1, 0, 0));  // Should not ignite
    }
    
    it("should verify material fuel values directly") {
        // Test the public accessors
        expect(MaterialFuel(MAT_WOOD) > 0);    // Wood has fuel
        expect(MaterialFuel(MAT_STONE) == 0);  // Stone has no fuel
        expect(MaterialFuel(MAT_IRON) == 0);   // Iron has no fuel
        expect(MaterialIsFlammable(MAT_WOOD));
        expect(!MaterialIsFlammable(MAT_STONE));
    }
}
```

---

## Stage F: Remove CELL_WOOD_WALL (Final Cleanup)

**Goal**: Remove the redundant cell type, all usages converted to CELL_WALL + material.

### Prerequisites
- All tests updated to use material system
- Fire system checks material
- No functional code depends on CELL_WOOD_WALL

### Tasks
1. Search for all `CELL_WOOD_WALL` usages:
   - `src/world/grid.h` - enum definition
   - `src/world/cell_defs.c` - CellDef entry
   - `src/core/input.c:47,1100` - wall placement UI
   - `src/core/inspect.c:512` - debug inspection

2. Update `input.c` wall placement:
   ```c
   // BEFORE:
   CellType wallType = (selectedMaterial == 2) ? CELL_WOOD_WALL : CELL_WALL;
   grid[z][y][x] = wallType;
   
   // AFTER:
   grid[z][y][x] = CELL_WALL;
   SetCellMaterial(x, y, z, (selectedMaterial == 2) ? MAT_WOOD : MAT_STONE);
   ```

3. Update `inspect.c`:
   ```c
   // BEFORE:
   case CELL_WOOD_WALL: DrawText("Wood Wall", ...); break;
   
   // AFTER:
   case CELL_WALL:
       MaterialType mat = GetCellMaterial(x, y, z);
       DrawText(mat == MAT_WOOD ? "Wood Wall" : "Stone Wall", ...);
       break;
   ```

4. Remove `CELL_WOOD_WALL` from `CellType` enum in `grid.h`
5. Remove `[CELL_WOOD_WALL]` entry from `cell_defs.c`
6. Update any terrain generation that used `CELL_WOOD_WALL`

### Test Checkpoint F (Final)
```
✓ grep -r "CELL_WOOD_WALL" returns nothing
✓ All tests pass
✓ Game compiles and runs
✓ Manual: Full gameplay test - mining, building, fire all work correctly
✓ Manual: Build wood wall via UI, verify it's flammable
✓ Manual: Build stone wall via UI, verify it's not flammable
```

---

## Test Migration Summary

### Tests that need updates before Stage F:

| File | Current Usage | Migration |
|------|---------------|-----------|
| `test_temperature.c:265` | Placeholder comment only | No code change needed |
| `test_fire.c` | Uses `CELL_WALL` (no CELL_WOOD_WALL) | May need material setup for wood tests |

### New test file: `tests/test_materials.c`

Create this file incrementally as you complete each stage:
- Stage A: Grid initialization tests
- Stage B: Definition accessor tests
- Stage C: Blueprint material tracking tests
- Stage D: Mining drop tests
- Stage E: Material flammability tests

### Running Tests

After each stage:
```bash
# Build and run all tests
make test

# Or if using cmake
cmake --build build && ctest --test-dir build

# Run specific test file
./build/test_materials
```

---

## Rollback Points

If something goes wrong at any stage:

- **Stage A-B**: Safe to revert - no behavior changed
- **Stage C**: Revert `CompleteBlueprint()` and `DeliverMaterialToBlueprint()` changes
- **Stage D**: Revert `Complete*Designation()` changes, restore hardcoded drops
- **Stage E**: Revert `GetFuelAt()` changes in fire.c
- **Stage F**: Cannot easily rollback - test thoroughly before removing CELL_WOOD_WALL

---

## Time Estimates

Not provided - work at your own pace. Each stage should be:
- Independently testable
- Leaves codebase working
- Can pause between stages
