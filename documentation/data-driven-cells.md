# Data-Driven Cell Types

## Problem

Cell type properties are scattered across many files:

| Property | Current Location |
|----------|------------------|
| Enum definition | `grid.h` |
| Sprite mapping | `main.c` GetCellSprite() |
| Blocks movement | `grid.h` IsWallCell() |
| Blocks water | `water.c` CanHoldWater() |
| Blocks smoke/steam | `smoke.c`, `steam.c` |
| Insulation tier | `temperature.c` GetInsulationTier() |
| Fuel amount | `fire.h` defines, `fire.c` GetBaseFuelForCellType() |
| Burns into | `fire.c` hardcoded |
| Is walkable | `grid.h` IsCellWalkableAt() |
| Is ladder | `grid.h` IsLadderCell() |

Adding a new cell type requires editing 6+ files. Easy to miss something.

## Proposed Solution

Define all cell properties in one data table:

```c
// cell_defs.h

typedef struct {
    const char* name;           // For debugging/tooltips
    int sprite;                 // SPRITE_xxx
    
    // Movement
    bool blocksMovement;        // Walls, closed doors
    bool isWalkable;            // Can stand on it
    bool isLadder;              // Ladder type
    
    // Fluids
    bool blocksWater;
    bool blocksSmoke;
    bool blocksSteam;
    
    // Temperature
    int insulationTier;         // INSULATION_TIER_AIR/WOOD/STONE
    
    // Fire
    int fuel;                   // 0 = not flammable
    CellType burnsInto;         // What it becomes when burned (or self if doesn't burn)
    
    // Future extensibility
    // int hardness;            // For mining
    // bool isTransparent;      // For lighting
    // int movementCost;        // For pathfinding weights
} CellDef;
```

## Data Table

```c
// cell_defs.c

static CellDef cellDefs[CELL_TYPE_COUNT] = {
    [CELL_WALKABLE] = {
        .name = "grass",
        .sprite = SPRITE_grass,
        .blocksMovement = false,
        .isWalkable = true,
        .isLadder = false,
        .blocksWater = false,
        .blocksSmoke = false,
        .blocksSteam = false,
        .insulationTier = INSULATION_TIER_AIR,
        .fuel = 16,
        .burnsInto = CELL_DIRT,
    },
    [CELL_WALL] = {
        .name = "stone wall",
        .sprite = SPRITE_wall,
        .blocksMovement = true,
        .isWalkable = false,
        .isLadder = false,
        .blocksWater = true,
        .blocksSmoke = true,
        .blocksSteam = true,
        .insulationTier = INSULATION_TIER_STONE,
        .fuel = 0,
        .burnsInto = CELL_WALL,  // doesn't burn
    },
    [CELL_WOOD_WALL] = {
        .name = "wood wall",
        .sprite = SPRITE_wood_wall,
        .blocksMovement = true,
        .isWalkable = false,
        .isLadder = false,
        .blocksWater = true,
        .blocksSmoke = true,
        .blocksSteam = true,
        .insulationTier = INSULATION_TIER_WOOD,
        .fuel = 128,
        .burnsInto = CELL_FLOOR,
    },
    [CELL_FLOOR] = {
        .name = "floor",
        .sprite = SPRITE_floor,
        .blocksMovement = false,
        .isWalkable = true,
        .isLadder = false,
        .blocksWater = false,
        .blocksSmoke = false,
        .blocksSteam = false,
        .insulationTier = INSULATION_TIER_WOOD,
        .fuel = 0,
        .burnsInto = CELL_FLOOR,
    },
    [CELL_AIR] = {
        .name = "air",
        .sprite = SPRITE_air,
        .blocksMovement = false,
        .isWalkable = false,
        .isLadder = false,
        .blocksWater = false,
        .blocksSmoke = false,
        .blocksSteam = false,
        .insulationTier = INSULATION_TIER_AIR,
        .fuel = 0,
        .burnsInto = CELL_AIR,
    },
    [CELL_GRASS] = {
        .name = "grass",
        .sprite = SPRITE_grass,
        .blocksMovement = false,
        .isWalkable = true,
        .isLadder = false,
        .blocksWater = false,
        .blocksSmoke = false,
        .blocksSteam = false,
        .insulationTier = INSULATION_TIER_AIR,
        .fuel = 16,
        .burnsInto = CELL_DIRT,
    },
    [CELL_DIRT] = {
        .name = "dirt",
        .sprite = SPRITE_dirt,
        .blocksMovement = false,
        .isWalkable = true,
        .isLadder = false,
        .blocksWater = false,
        .blocksSmoke = false,
        .blocksSteam = false,
        .insulationTier = INSULATION_TIER_AIR,
        .fuel = 1,
        .burnsInto = CELL_DIRT,
    },
    [CELL_LADDER_UP] = {
        .name = "ladder up",
        .sprite = SPRITE_ladder_up,
        .blocksMovement = false,
        .isWalkable = true,
        .isLadder = true,
        .blocksWater = false,
        .blocksSmoke = false,
        .blocksSteam = false,
        .insulationTier = INSULATION_TIER_AIR,
        .fuel = 0,
        .burnsInto = CELL_LADDER_UP,
    },
    // ... etc for other ladder types
};
```

## Accessor Macros/Functions

Replace scattered checks with centralized accessors:

```c
// cell_defs.h

// Direct field access (fast, inline)
#define CellDef(c)              (&cellDefs[c])
#define CellName(c)             (cellDefs[c].name)
#define CellSprite(c)           (cellDefs[c].sprite)
#define CellBlocksMovement(c)   (cellDefs[c].blocksMovement)
#define CellIsWalkable(c)       (cellDefs[c].isWalkable)
#define CellIsLadder(c)         (cellDefs[c].isLadder)
#define CellBlocksWater(c)      (cellDefs[c].blocksWater)
#define CellBlocksSmoke(c)      (cellDefs[c].blocksSmoke)
#define CellBlocksSteam(c)      (cellDefs[c].blocksSteam)
#define CellInsulationTier(c)   (cellDefs[c].insulationTier)
#define CellFuel(c)             (cellDefs[c].fuel)
#define CellBurnsInto(c)        (cellDefs[c].burnsInto)

// Compound checks (keep for convenience)
static inline bool IsWallCell(CellType c) {
    return CellBlocksMovement(c);
}

static inline bool IsCellWalkableType(CellType c) {
    return CellIsWalkable(c);
}
```

## Migration Plan

### Phase 1: Create infrastructure
1. Create `cell_defs.h` and `cell_defs.c`
2. Define CellDef struct and cellDefs table
3. Add to unity.c includes

### Phase 2: Migrate sprite mapping
1. Replace `GetCellSprite()` in main.c with `CellSprite()`
2. Test rendering still works

### Phase 3: Migrate movement checks
1. Replace `IsWallCell()` to use `CellBlocksMovement()`
2. Replace `IsCellWalkableAt()` to use `CellIsWalkable()`
3. Replace `IsLadderCell()` to use `CellIsLadder()`
4. Test pathfinding still works

### Phase 4: Migrate fluid checks
1. Update `CanHoldWater()` in water.c
2. Update `CanHoldSmoke()` in smoke.c
3. Update `CanHoldSteam()` in steam.c
4. Test fluids still work

### Phase 5: Migrate temperature
1. Update `GetInsulationTier()` in temperature.c
2. Test temperature still works

### Phase 6: Migrate fire
1. Update `GetBaseFuelForCellType()` in fire.c
2. Update burn-collapse logic to use `CellBurnsInto()`
3. Remove FUEL_* defines from fire.h
4. Test fire still works

### Phase 7: Cleanup
1. Remove old scattered functions
2. Update tooltips to use `CellName()`
3. Run all tests

## Benefits

1. **Single source of truth** - all cell properties in one place
2. **Easy to add new cells** - just add one table entry
3. **Easy to tweak** - change one number, affects all systems
4. **Self-documenting** - table shows all properties at a glance
5. **Future extensibility** - add new properties without touching existing code
6. **Modding potential** - could load from external files later

## Considerations

### Performance
- Macro/inline accessors compile to direct array access
- No function call overhead
- Cache-friendly (cellDefs fits in cache)
- No worse than current switch statements

### Memory
- CellDef ~40 bytes per type
- ~12 cell types = ~480 bytes total
- Negligible

### Save/Load Compatibility
- Cell enum values must stay stable
- Or save cell names and look up on load

### Validation
- Could add init-time validation that all sprites exist
- Could assert all enum values have entries

## Scaling Considerations (100+ cell types, 16+ properties)

At larger scales, the simple C table approach becomes unwieldy. Options:

### Option 1: Designated Initializers (current plan)
```c
[CELL_GRASS] = {
    .name = "grass",
    .sprite = SPRITE_grass,
    // only set non-default values...
},
```
- Pro: Explicit, compiler catches typos
- Con: Verbose for 100+ types

### Option 2: Inheritance/Templates
```c
static CellDef BASE_GROUND = {.isWalkable = true, ...};
cellDefs[CELL_GRASS] = BASE_GROUND;
cellDefs[CELL_GRASS].sprite = SPRITE_grass;
```
- Pro: Less repetition
- Con: Harder to see final values

### Option 3: External Data Files (JSON/TOML)
```json
{
  "grass": {"base": "ground", "sprite": "grass", "fuel": 16}
}
```
- Pro: Easy to edit, moddable, no recompile
- Con: Need parser, runtime validation

### Option 4: Code Generation
Python/script reads CSV/YAML, generates C code.
- Pro: Data file for editing, compiled for speed
- Con: Build complexity

### Option 5: Bitmask Tags
```c
typedef enum {
    TAG_WALKABLE = 1 << 0,
    TAG_BLOCKS_MOVEMENT = 1 << 1,
    // ...
} CellTags;
```
- Pro: Compact, fast tag checks
- Con: Only works for boolean properties

**Recommendation**: For 100+ types, use Option 3 (JSON) or Option 4 (code generation).

---

## How Other Games Handle This

### Dwarf Fortress - Custom Text Format ("Raws")

DF uses plain text files with bracket tokens:

```
filename
[OBJECT:INORGITE]
[INORGANIC:ITE]
    [STATE_NAME_ADJ:ALL_SOLID:iteite]
    [DISPLAY_COLOR:4:0:1]
    [TILE:156]
    [SOLID_DENSITY:ite]
    [MELTING_POINT:11000]
    [IS_STONE]
    [NO_STONE_STOCKPILE]
```

Key points:
- Text files in `data/vanilla/` directory
- Format: `[TOKEN:VALUE]` or `[TOKEN:VALUE1:VALUE2]`
- Object type declared with `[OBJECT:TYPE]`
- Highly moddable - players edit text files directly
- Templates exist for inheritance (`MATERIAL_TEMPLATE`)
- Parsing order determined by first line of file

Source: [Dwarf Fortress Wiki - Raw file](https://dwarffortresswiki.org/index.php/Raw_file)

### Cataclysm: Dark Days Ahead - JSON

CDDA uses JSON files with explicit type markers:

```json
{
  "type": "terrain",
  "id": "t_brick_wall",
  "name": "brick wall",
  "symbol": "#",
  "color": "red",
  "move_cost": 0,
  "flags": ["FLAMMABLE", "NOITEM", "SUPPORTS_ROOF"],
  "bash": {
    "str_min": 60,
    "str_max": 180,
    "ter_set": "t_rubble"
  },
  "connects_to": "WALL",
  "deconstruct": {
    "ter_set": "t_floor",
    "items": [{"item": "brick", "count": 8}]
  }
}
```

Key points:
- Standard JSON format
- `"type"` field identifies object kind
- `"id"` with prefix convention (`t_` for terrain, `f_` for furniture)
- Flags array for boolean properties
- Nested objects for complex behaviors (bash, deconstruct)
- `"connects_to"` for auto-tiling groups
- `"looks_like"` for tileset fallbacks

Source: [Cataclysm DDA - JSON_INFO](https://docs.cataclysmdda.org/JSON/JSON_INFO.html)

### Comparison

| Aspect | Dwarf Fortress | Cataclysm DDA | Our C Table |
|--------|---------------|---------------|-------------|
| Format | Custom brackets | JSON | C structs |
| Moddable | Very easy | Easy | Requires recompile |
| Parsing | Custom parser | JSON library | Compile-time |
| Validation | Runtime | Runtime | Compile-time |
| Inheritance | Templates | copy-from | None (yet) |
| Performance | Slower load | Slower load | Fastest |
| Type safety | None | None | Full |

### Lessons for Navkit

1. **Start simple** - C table is fine for <50 types
2. **Plan for JSON** - design struct to be JSON-serializable
3. **Use flags array** - `uint32_t flags` beats 16 bool fields
4. **Add inheritance later** - `"base"` or `"copy_from"` pattern
5. **String IDs** - consider using string IDs for save compatibility

---

## Open Questions

1. Should ladders be a flag or separate enum? (currently mixed)
2. Should we add `eraseInto` for what cells become when erased?
3. Should walkable ground types (grass/dirt/floor) share a base definition?
4. Load from external file (JSON/TOML) for modding? Or keep compiled?
5. Use string IDs instead of enum for future-proofing saves?
6. Add flags bitmask for boolean properties?

---

## Example: 32 Cell Types with C Table

Here's what the table looks like at 32 types. Notice it gets long but remains readable with designated initializers:

```c
// cell_defs.c

typedef struct {
    const char* name;
    int sprite;
    uint32_t flags;         // Bitmask of CELL_FLAG_*
    int insulationTier;
    int fuel;
    CellType burnsInto;
} CellDef;

// Flags (bitmask instead of many bools)
#define CF_BLOCKS_MOVEMENT  (1 << 0)
#define CF_WALKABLE         (1 << 1)
#define CF_LADDER           (1 << 2)
#define CF_BLOCKS_WATER     (1 << 3)
#define CF_BLOCKS_SMOKE     (1 << 4)
#define CF_BLOCKS_STEAM     (1 << 5)
#define CF_TRANSPARENT      (1 << 6)
#define CF_DIGGABLE         (1 << 7)

// Common flag combinations
#define CF_GROUND  (CF_WALKABLE)
#define CF_WALL    (CF_BLOCKS_MOVEMENT | CF_BLOCKS_WATER | CF_BLOCKS_SMOKE | CF_BLOCKS_STEAM | CF_DIGGABLE)
#define CF_FLUID   (0)  // blocks nothing

static CellDef cellDefs[] = {
    // === NATURAL GROUND ===
    [CELL_GRASS]          = {"grass",           SPRITE_grass,           CF_GROUND,  AIR,   16, CELL_DIRT},
    [CELL_DIRT]           = {"dirt",            SPRITE_dirt,            CF_GROUND,  AIR,    1, CELL_DIRT},
    [CELL_MUD]            = {"mud",             SPRITE_mud,             CF_GROUND,  AIR,    0, CELL_MUD},
    [CELL_SAND]           = {"sand",            SPRITE_sand,            CF_GROUND,  AIR,    0, CELL_SAND},
    [CELL_SNOW]           = {"snow",            SPRITE_snow,            CF_GROUND,  AIR,    0, CELL_DIRT},
    [CELL_ICE]            = {"ice",             SPRITE_ice,             CF_GROUND,  AIR,    0, CELL_ICE},
    
    // === STONE/ROCK ===
    [CELL_STONE_FLOOR]    = {"stone floor",     SPRITE_stone_floor,     CF_GROUND,  STONE,  0, CELL_STONE_FLOOR},
    [CELL_STONE_WALL]     = {"stone wall",      SPRITE_stone_wall,      CF_WALL,    STONE,  0, CELL_STONE_WALL},
    [CELL_MARBLE_FLOOR]   = {"marble floor",    SPRITE_marble_floor,    CF_GROUND,  STONE,  0, CELL_MARBLE_FLOOR},
    [CELL_MARBLE_WALL]    = {"marble wall",     SPRITE_marble_wall,     CF_WALL,    STONE,  0, CELL_MARBLE_WALL},
    
    // === WOOD ===
    [CELL_WOOD_FLOOR]     = {"wood floor",      SPRITE_wood_floor,      CF_GROUND,  WOOD,  32, CELL_DIRT},
    [CELL_WOOD_WALL]      = {"wood wall",       SPRITE_wood_wall,       CF_WALL,    WOOD, 128, CELL_FLOOR},
    [CELL_LOG]            = {"log",             SPRITE_log,             CF_WALL,    WOOD, 200, CELL_DIRT},
    
    // === METAL ===
    [CELL_METAL_FLOOR]    = {"metal floor",     SPRITE_metal_floor,     CF_GROUND,  STONE,  0, CELL_METAL_FLOOR},
    [CELL_METAL_WALL]     = {"metal wall",      SPRITE_metal_wall,      CF_WALL,    STONE,  0, CELL_METAL_WALL},
    [CELL_METAL_GRATE]    = {"metal grate",     SPRITE_metal_grate,     CF_GROUND | CF_TRANSPARENT, AIR, 0, CELL_METAL_GRATE},
    
    // === CONSTRUCTED ===
    [CELL_BRICK_WALL]     = {"brick wall",      SPRITE_brick_wall,      CF_WALL,    STONE,  0, CELL_BRICK_WALL},
    [CELL_BRICK_FLOOR]    = {"brick floor",     SPRITE_brick_floor,     CF_GROUND,  STONE,  0, CELL_BRICK_FLOOR},
    [CELL_TILE_FLOOR]     = {"tile floor",      SPRITE_tile_floor,      CF_GROUND,  STONE,  0, CELL_TILE_FLOOR},
    [CELL_CARPET]         = {"carpet",          SPRITE_carpet,          CF_GROUND,  WOOD,  10, CELL_FLOOR},
    
    // === DOORS ===
    [CELL_DOOR_WOOD]      = {"wood door",       SPRITE_door_wood,       CF_WALL,    WOOD,  64, CELL_FLOOR},
    [CELL_DOOR_METAL]     = {"metal door",      SPRITE_door_metal,      CF_WALL,    STONE,  0, CELL_METAL_FLOOR},
    [CELL_DOOR_WOOD_OPEN] = {"open wood door",  SPRITE_door_wood_open,  CF_GROUND,  WOOD,  64, CELL_FLOOR},
    [CELL_DOOR_METAL_OPEN]= {"open metal door", SPRITE_door_metal_open, CF_GROUND,  STONE,  0, CELL_METAL_FLOOR},
    
    // === LADDERS ===
    [CELL_LADDER_UP]      = {"ladder up",       SPRITE_ladder_up,       CF_GROUND | CF_LADDER, AIR, 16, CELL_FLOOR},
    [CELL_LADDER_DOWN]    = {"ladder down",     SPRITE_ladder_down,     CF_GROUND | CF_LADDER, AIR, 16, CELL_FLOOR},
    [CELL_LADDER_BOTH]    = {"ladder",          SPRITE_ladder,          CF_GROUND | CF_LADDER, AIR, 16, CELL_FLOOR},
    
    // === SPECIAL ===
    [CELL_AIR]            = {"air",             SPRITE_air,             CF_FLUID,   AIR,    0, CELL_AIR},
    [CELL_WATER_SHALLOW]  = {"shallow water",   SPRITE_water_shallow,   CF_GROUND,  AIR,    0, CELL_WATER_SHALLOW},
    [CELL_LAVA]           = {"lava",            SPRITE_lava,            CF_FLUID,   AIR,    0, CELL_LAVA},
    [CELL_VOID]           = {"void",            SPRITE_void,            CF_BLOCKS_MOVEMENT, AIR, 0, CELL_VOID},
};

// Accessor macros
#define CellHasFlag(c, f)       (cellDefs[c].flags & (f))
#define CellBlocksMovement(c)   CellHasFlag(c, CF_BLOCKS_MOVEMENT)
#define CellIsWalkable(c)       CellHasFlag(c, CF_WALKABLE)
#define CellIsLadder(c)         CellHasFlag(c, CF_LADDER)
#define CellBlocksWater(c)      CellHasFlag(c, CF_BLOCKS_WATER)
#define CellFuel(c)             (cellDefs[c].fuel)
#define CellBurnsInto(c)        (cellDefs[c].burnsInto)
#define CellSprite(c)           (cellDefs[c].sprite)
#define CellName(c)             (cellDefs[c].name)
```

**Observations at 32 types:**

1. **Still manageable** - fits on one screen, easy to scan
2. **Grouping helps** - comments separate categories
3. **Flags bitmask** - reduces struct from 8 bools to 1 uint32
4. **Flag combos** - `CF_WALL` and `CF_GROUND` reduce repetition
5. **Consistent columns** - alignment makes scanning easier
6. **~80 lines** - for 32 types with comments

**At 100+ types**, this would be ~250 lines. Still workable but:
- Harder to find specific types
- More likely to make copy-paste errors
- Might want to split into multiple files by category
- JSON/code-gen becomes more attractive

---

## Related Refactors

This pattern could apply to:
- Item types (currently in items.h)
- Mover types (if we add different creature types)
- Building types (for blueprints)
