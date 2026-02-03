# Data-Driven Jobs & Materials

Centralize job/material definitions following the `cell_defs` and `item_defs` pattern. Currently job inputs/outputs are scattered across `designations.c`, `jobs.c`, and `workshops.c` with hardcoded values.

## Goals

1. Single source of truth for job I/O
2. Materials determine what gets built (wood → wood wall, stone → stone wall)
3. Cells know what they drop when mined/deconstructed
4. Easy to balance (all work times, yields in one place)

## Phase 1: Add Designation Defs

Create `src/world/designation_defs.h` and `src/world/designation_defs.c`:

```c
// designation_defs.h
typedef struct {
    const char* name;
    float workTime;
    ItemType outputItem;
    int outputCount;      // Base count (can be modified by job logic)
    CellType requiresCell;
    CellType producesCell;
} DesignationDef;

extern DesignationDef designationDefs[];

#define DesignationName(d)        (designationDefs[d].name)
#define DesignationWorkTime(d)    (designationDefs[d].workTime)
#define DesignationOutputItem(d)  (designationDefs[d].outputItem)
#define DesignationOutputCount(d) (designationDefs[d].outputCount)
#define DesignationRequiresCell(d)(designationDefs[d].requiresCell)
#define DesignationProducesCell(d)(designationDefs[d].producesCell)
```

```c
// designation_defs.c
DesignationDef designationDefs[] = {
    [DESIGNATION_NONE]           = {"None",           0.0f, ITEM_NONE,    0, CELL_AIR,        CELL_AIR},
    [DESIGNATION_MINE]           = {"Mine",           2.0f, ITEM_ORANGE,  1, CELL_WALL,       CELL_AIR},
    [DESIGNATION_CHANNEL]        = {"Channel",        2.0f, ITEM_NONE,    0, CELL_AIR,        CELL_AIR},  // Special logic
    [DESIGNATION_REMOVE_FLOOR]   = {"Remove Floor",   1.0f, ITEM_STONE_BLOCKS, 1, CELL_AIR,   CELL_AIR},
    [DESIGNATION_REMOVE_RAMP]    = {"Remove Ramp",    1.0f, ITEM_ORANGE,  1, CELL_AIR,        CELL_AIR},
    [DESIGNATION_CHOP]           = {"Chop",           3.0f, ITEM_WOOD,    2, CELL_TREE_TRUNK, CELL_AIR},
    [DESIGNATION_GATHER_SAPLING] = {"Gather Sapling", 1.0f, ITEM_SAPLING, 1, CELL_SAPLING,    CELL_AIR},
    [DESIGNATION_PLANT_SAPLING]  = {"Plant Sapling",  1.5f, ITEM_NONE,    0, CELL_AIR,        CELL_SAPLING},
};
```

Update `Complete*Designation()` functions in `designations.c` to use these accessors instead of hardcoded values.

### Files to modify
- Create `src/world/designation_defs.h`
- Create `src/world/designation_defs.c`
- Update `src/world/designations.h` - remove `#define MINE_WORK_TIME` etc.
- Update `src/world/designations.c` - use `DesignationWorkTime()` etc.
- Update `src/unity.c` - include new .c file

## Phase 2: Add Blueprint Defs

Create `src/world/blueprint_defs.h` and `src/world/blueprint_defs.c`:

```c
// blueprint_defs.h
typedef struct {
    const char* name;
    float workTime;
    uint8_t inputFlags;   // IF_BUILDING_MAT, etc.
    int inputCount;
    CellType producesCell; // Default cell (may be overridden by material)
} BlueprintDef;

extern BlueprintDef blueprintDefs[];

#define BlueprintName(b)        (blueprintDefs[b].name)
#define BlueprintWorkTime(b)    (blueprintDefs[b].workTime)
#define BlueprintInputFlags(b)  (blueprintDefs[b].inputFlags)
#define BlueprintInputCount(b)  (blueprintDefs[b].inputCount)
#define BlueprintProducesCell(b)(blueprintDefs[b].producesCell)
```

```c
// blueprint_defs.c
BlueprintDef blueprintDefs[] = {
    [BLUEPRINT_TYPE_WALL]   = {"Wall",   2.0f, IF_BUILDING_MAT, 1, CELL_WALL},
    [BLUEPRINT_TYPE_LADDER] = {"Ladder", 2.0f, IF_BUILDING_MAT, 1, CELL_LADDER_BOTH},
    [BLUEPRINT_TYPE_FLOOR]  = {"Floor",  2.0f, IF_BUILDING_MAT, 1, CELL_AIR},  // Sets HAS_FLOOR flag
};
```

Update `CreateBuildBlueprint()`, `CreateLadderBlueprint()`, `CreateFloorBlueprint()`, and `CompleteBlueprint()` to use these.

### Files to modify
- Create `src/world/blueprint_defs.h`
- Create `src/world/blueprint_defs.c`
- Update `src/world/designations.h` - remove `#define BUILD_WORK_TIME`
- Update `src/world/designations.c` - use `BlueprintWorkTime()` etc.
- Update `src/unity.c` - include new .c file

## Phase 3: Extend ItemDef with Build Targets

Add fields to `ItemDef` so items know what cells they can build:

```c
// item_defs.h
typedef struct {
    const char* name;
    int sprite;
    uint8_t flags;
    uint8_t maxStack;
    CellType buildsWall;    // CELL_AIR if can't build walls
    CellType buildsLadder;  // CELL_AIR if can't build ladders
} ItemDef;

#define ItemBuildsWall(t)   (itemDefs[t].buildsWall)
#define ItemBuildsLadder(t) (itemDefs[t].buildsLadder)
```

```c
// item_defs.c
const ItemDef itemDefs[ITEM_TYPE_COUNT] = {
    [ITEM_STONE_BLOCKS] = {"Stone Blocks", SPRITE_stone_block, 
        IF_STACKABLE | IF_BUILDING_MAT, 20, CELL_WALL, CELL_LADDER_BOTH},
    [ITEM_WOOD] = {"Wood", SPRITE_wood_block,
        IF_STACKABLE | IF_BUILDING_MAT | IF_FUEL, 20, CELL_WOOD_WALL, CELL_LADDER_BOTH},
    // Non-building items have CELL_AIR for both
    [ITEM_RED] = {"Red Crate", SPRITE_crate_red, IF_STACKABLE, 10, CELL_AIR, CELL_AIR},
    ...
};
```

### Files to modify
- Update `src/entities/item_defs.h` - add fields and accessors
- Update `src/entities/item_defs.c` - populate new fields

## Phase 4: Extend CellDef with Drop Items

Add fields to `CellDef` so cells know what they drop when mined/deconstructed:

```c
// cell_defs.h
typedef struct {
    const char* name;
    int sprite;
    uint8_t flags;
    uint8_t insulationTier;
    uint8_t fuel;
    CellType burnsInto;
    ItemType dropsItem;   // What item when mined/deconstructed (ITEM_NONE if nothing)
    uint8_t dropCount;    // Base drop count
} CellDef;

#define CellDropsItem(c)  (cellDefs[c].dropsItem)
#define CellDropCount(c)  (cellDefs[c].dropCount)
```

```c
// cell_defs.c
CellDef cellDefs[] = {
    [CELL_WALL]       = {"stone wall",  SPRITE_wall,      CF_WALL, ..., ITEM_ORANGE, 1},
    [CELL_WOOD_WALL]  = {"wood wall",   SPRITE_wood_wall, CF_WALL, ..., ITEM_WOOD, 1},
    [CELL_TREE_TRUNK] = {"tree trunk",  SPRITE_tree_trunk, ...,         ITEM_WOOD, 2},
    [CELL_SAPLING]    = {"sapling",     SPRITE_tree_sapling, ...,       ITEM_SAPLING, 1},
    [CELL_AIR]        = {"air",         SPRITE_air, ...,                ITEM_NONE, 0},
    ...
};
```

### Files to modify
- Update `src/world/cell_defs.h` - add fields and accessors
- Update `src/world/cell_defs.c` - populate new fields

## Phase 5: Wire It Up — Material Affects Output

Track delivered material type in Blueprint:

```c
// designations.h - Blueprint struct
typedef struct {
    ...
    ItemType deliveredMaterialType;  // NEW: what material was delivered
    ...
} Blueprint;
```

Update `CompleteBlueprint()` to use the material:

```c
void CompleteBlueprint(int blueprintIdx) {
    Blueprint* bp = &blueprints[blueprintIdx];
    ItemType mat = bp->deliveredMaterialType;
    
    switch (bp->type) {
        case BLUEPRINT_TYPE_WALL:
            grid[bp->z][bp->y][bp->x] = ItemBuildsWall(mat);
            break;
        case BLUEPRINT_TYPE_LADDER:
            grid[bp->z][bp->y][bp->x] = ItemBuildsLadder(mat);
            break;
        case BLUEPRINT_TYPE_FLOOR:
            SET_FLOOR(bp->x, bp->y, bp->z);
            // Could store floor material in cellFlags if needed later
            break;
    }
    ...
}
```

Update `DeliverMaterialToBlueprint()` to record the material type.

### Files to modify
- Update `src/world/designations.h` - add `deliveredMaterialType` to Blueprint
- Update `src/world/designations.c` - use material in `CompleteBlueprint()`
- Update `src/entities/jobs.c` - record material in `DeliverMaterialToBlueprint()`
- Update `src/core/saveload.c` - save/load `deliveredMaterialType`

## Phase 6: Update Mining to Use CellDef Drops

Update `CompleteMineDesignation()` to use `CellDropsItem()`:

```c
void CompleteMineDesignation(int x, int y, int z) {
    CellType oldCell = grid[z][y][x];
    ItemType dropItem = CellDropsItem(oldCell);
    int dropCount = CellDropCount(oldCell);
    
    // Convert to air
    grid[z][y][x] = CELL_AIR;
    
    // Spawn drops
    if (dropItem != ITEM_NONE) {
        for (int i = 0; i < dropCount; i++) {
            SpawnItem(x, y, z, dropItem);
        }
    }
    ...
}
```

Similarly update chopping, deconstruction, etc.

### Files to modify
- Update `src/world/designations.c` - use `CellDropsItem()` in `Complete*Designation()`

## Testing

1. Build wall with stone blocks → should create `CELL_WALL`
2. Build wall with wood → should create `CELL_WOOD_WALL`
3. Mine stone wall → should drop `ITEM_ORANGE`
4. Mine wood wall → should drop `ITEM_WOOD`
5. Chop tree → should drop `ITEM_WOOD` (count from CellDef)
6. All existing tests pass

## Future Possibilities

Once this is in place:
- Add new materials easily (metal blocks → metal wall)
- Add material-specific properties (wood burns, stone doesn't)
- Track floor materials in cellFlags for visual variety
- Query "what produces wood?" by scanning CellDefs
- Data-driven modding support
