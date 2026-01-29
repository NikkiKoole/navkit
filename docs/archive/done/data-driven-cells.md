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

Define all cell properties in one data table using a flags bitmask for boolean properties:

```c
// cell_defs.h

typedef struct {
    const char* name;           // For debugging/tooltips
    int sprite;                 // SPRITE_xxx
    uint8_t flags;              // Bitmask of CF_* flags
    uint8_t insulationTier;     // INSULATION_TIER_AIR/WOOD/STONE
    uint8_t fuel;               // 0 = not flammable
    CellType burnsInto;         // What it becomes when burned (or self if doesn't burn)
    
    // Future extensibility (add when needed)
    // uint8_t hardness;        // 0 = not mineable, 1-255 = mining difficulty
    // uint8_t cutTime;         // 0 = not cuttable, 1-255 = cutting time
} CellDef;

// Physics flags (5 bits used, 3 reserved)
#define CF_BLOCKS_MOVEMENT  (1 << 0)  // Can't walk through (walls, closed doors, grates)
#define CF_WALKABLE         (1 << 1)  // Can stand on (ground, floors, ladders)
#define CF_LADDER           (1 << 2)  // Vertical climbing - slower movement
#define CF_RAMP             (1 << 3)  // Vertical walking - normal speed (stairs)
#define CF_BLOCKS_FLUIDS    (1 << 4)  // Blocks water/smoke/steam

// Common flag combinations
#define CF_GROUND  (CF_WALKABLE)
#define CF_WALL    (CF_BLOCKS_MOVEMENT | CF_BLOCKS_FLUIDS)
```

### Flag Design Rationale

We consolidated the original 8 boolean fields into 5 flags:

| Original Fields | Consolidated To | Rationale |
|-----------------|-----------------|-----------|
| `blocksMovement` | `CF_BLOCKS_MOVEMENT` | Kept - walls, doors, grates |
| `isWalkable` | `CF_WALKABLE` | Kept - distinct from "doesn't block" (air doesn't block but isn't walkable) |
| `isLadder` | `CF_LADDER` | Kept - special pathfinding behavior, slower movement |
| (new) | `CF_RAMP` | Added - stairs/ramps connect Z-levels at normal speed |
| `blocksWater`, `blocksSmoke`, `blocksSteam` | `CF_BLOCKS_FLUIDS` | Merged - currently identical logic (`!IsWallCell`). Split later if needed (e.g., grate blocks movement but not fluids) |

This allows for combinations like:
- **Grate**: `CF_BLOCKS_MOVEMENT` only - can't walk through, but water/smoke passes
- **Glass wall**: `CF_BLOCKS_MOVEMENT | CF_BLOCKS_FLUIDS` - blocks everything
- **Open door**: `CF_WALKABLE` only - can walk through, fluids pass

## Data Table

```c
// cell_defs.c

static CellDef cellDefs[] = {
    // === GROUND TYPES ===
    [CELL_GRASS]       = {"grass",       SPRITE_grass,       CF_GROUND,            AIR,   16, CELL_DIRT},
    [CELL_DIRT]        = {"dirt",        SPRITE_dirt,        CF_GROUND,            AIR,    1, CELL_DIRT},
    [CELL_FLOOR]       = {"floor",       SPRITE_floor,       CF_GROUND,            WOOD,   0, CELL_FLOOR},
    
    // === WALLS ===
    [CELL_WALL]        = {"stone wall",  SPRITE_wall,        CF_WALL,              STONE,  0, CELL_WALL},
    [CELL_WOOD_WALL]   = {"wood wall",   SPRITE_wood_wall,   CF_WALL,              WOOD, 128, CELL_FLOOR},
    
    // === VERTICAL MOVEMENT ===
    [CELL_LADDER_UP]   = {"ladder up",   SPRITE_ladder_up,   CF_GROUND|CF_LADDER,  AIR,    0, CELL_LADDER_UP},
    [CELL_LADDER_DOWN] = {"ladder down", SPRITE_ladder_down, CF_GROUND|CF_LADDER,  AIR,    0, CELL_LADDER_DOWN},
    [CELL_LADDER_BOTH] = {"ladder",      SPRITE_ladder,      CF_GROUND|CF_LADDER,  AIR,    0, CELL_LADDER_BOTH},
    
    // === SPECIAL ===
    [CELL_AIR]         = {"air",         SPRITE_air,         0,                    AIR,    0, CELL_AIR},
    
    // === LEGACY ALIASES ===
    [CELL_WALKABLE]    = {"grass",       SPRITE_grass,       CF_GROUND,            AIR,   16, CELL_DIRT},
    [CELL_LADDER]      = {"ladder",      SPRITE_ladder,      CF_GROUND|CF_LADDER,  AIR,    0, CELL_LADDER},
    
    // === FUTURE EXAMPLES ===
    // [CELL_GRATE]    = {"grate",       SPRITE_grate,       CF_BLOCKS_MOVEMENT,   AIR,    0, CELL_GRATE},
    // [CELL_RAMP_UP]  = {"ramp up",     SPRITE_ramp_up,     CF_GROUND|CF_RAMP,    AIR,    0, CELL_RAMP_UP},
};
```

## Accessor Macros/Functions

Replace scattered checks with centralized accessors:

```c
// cell_defs.h

// Flag checks
#define CellHasFlag(c, f)       (cellDefs[c].flags & (f))
#define CellBlocksMovement(c)   CellHasFlag(c, CF_BLOCKS_MOVEMENT)
#define CellIsWalkable(c)       CellHasFlag(c, CF_WALKABLE)
#define CellIsLadder(c)         CellHasFlag(c, CF_LADDER)
#define CellIsRamp(c)           CellHasFlag(c, CF_RAMP)
#define CellBlocksFluids(c)     CellHasFlag(c, CF_BLOCKS_FLUIDS)

// Direct field access
#define CellName(c)             (cellDefs[c].name)
#define CellSprite(c)           (cellDefs[c].sprite)
#define CellInsulationTier(c)   (cellDefs[c].insulationTier)
#define CellFuel(c)             (cellDefs[c].fuel)
#define CellBurnsInto(c)        (cellDefs[c].burnsInto)

// Compound checks (keep for convenience/backwards compat)
static inline bool IsWallCell(CellType c) {
    return CellBlocksMovement(c);
}

static inline bool IsCellWalkableType(CellType c) {
    return CellIsWalkable(c);
}

// For fluid systems - replaces CanHoldWater/Smoke/Steam checks
static inline bool CellAllowsFluids(CellType c) {
    return !CellBlocksFluids(c);
}
```

## Migration Plan

### Phase 1: Create infrastructure
1. Create `cell_defs.h` with CellDef struct and flag definitions
2. Create `cell_defs.c` with cellDefs table
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
1. Update `CanHoldWater()` in water.c to use `CellAllowsFluids()`
2. Update `CanHoldSmoke()` in smoke.c to use `CellAllowsFluids()`
3. Update `CanHoldSteam()` in steam.c to use `CellAllowsFluids()`
4. Test fluids still work

### Phase 5: Migrate temperature
1. Update `GetInsulationTier()` in temperature.c to use `CellInsulationTier()`
2. Test temperature still works

### Phase 6: Migrate fire
1. Update `GetBaseFuelForCellType()` in fire.c to use `CellFuel()`
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

1. ~~Should ladders be a flag or separate enum?~~ **Resolved**: Flag (`CF_LADDER`)
2. Should we add `eraseInto` for what cells become when erased?
3. Should walkable ground types (grass/dirt/floor) share a base definition?
4. Load from external file (JSON/TOML) for modding? Or keep compiled?
5. Use string IDs instead of enum for future-proofing saves?
6. ~~Add flags bitmask for boolean properties?~~ **Resolved**: Yes, using `uint8_t flags`
7. ~~Separate blocksWater/blocksSmoke/blocksSteam?~~ **Resolved**: Merged into `CF_BLOCKS_FLUIDS` (split later if needed for grates/vents)

---

## Example: 32 Cell Types with C Table

Here's what the table looks like at 32 types using the consolidated flags:

```c
// cell_defs.c

// Uses the CellDef struct and flags from cell_defs.h

static CellDef cellDefs[] = {
    // === NATURAL GROUND ===
    [CELL_GRASS]          = {"grass",           SPRITE_grass,           CF_GROUND,              AIR,   16, CELL_DIRT},
    [CELL_DIRT]           = {"dirt",            SPRITE_dirt,            CF_GROUND,              AIR,    1, CELL_DIRT},
    [CELL_MUD]            = {"mud",             SPRITE_mud,             CF_GROUND,              AIR,    0, CELL_MUD},
    [CELL_SAND]           = {"sand",            SPRITE_sand,            CF_GROUND,              AIR,    0, CELL_SAND},
    [CELL_SNOW]           = {"snow",            SPRITE_snow,            CF_GROUND,              AIR,    0, CELL_DIRT},
    [CELL_ICE]            = {"ice",             SPRITE_ice,             CF_GROUND,              AIR,    0, CELL_ICE},
    
    // === STONE/ROCK ===
    [CELL_STONE_FLOOR]    = {"stone floor",     SPRITE_stone_floor,     CF_GROUND,              STONE,  0, CELL_STONE_FLOOR},
    [CELL_STONE_WALL]     = {"stone wall",      SPRITE_stone_wall,      CF_WALL,                STONE,  0, CELL_STONE_WALL},
    [CELL_MARBLE_FLOOR]   = {"marble floor",    SPRITE_marble_floor,    CF_GROUND,              STONE,  0, CELL_MARBLE_FLOOR},
    [CELL_MARBLE_WALL]    = {"marble wall",     SPRITE_marble_wall,     CF_WALL,                STONE,  0, CELL_MARBLE_WALL},
    
    // === WOOD ===
    [CELL_WOOD_FLOOR]     = {"wood floor",      SPRITE_wood_floor,      CF_GROUND,              WOOD,  32, CELL_DIRT},
    [CELL_WOOD_WALL]      = {"wood wall",       SPRITE_wood_wall,       CF_WALL,                WOOD, 128, CELL_FLOOR},
    [CELL_LOG]            = {"log",             SPRITE_log,             CF_WALL,                WOOD, 200, CELL_DIRT},
    
    // === METAL ===
    [CELL_METAL_FLOOR]    = {"metal floor",     SPRITE_metal_floor,     CF_GROUND,              STONE,  0, CELL_METAL_FLOOR},
    [CELL_METAL_WALL]     = {"metal wall",      SPRITE_metal_wall,      CF_WALL,                STONE,  0, CELL_METAL_WALL},
    [CELL_METAL_GRATE]    = {"metal grate",     SPRITE_metal_grate,     CF_BLOCKS_MOVEMENT,     AIR,    0, CELL_METAL_GRATE},  // blocks movement, not fluids!
    
    // === CONSTRUCTED ===
    [CELL_BRICK_WALL]     = {"brick wall",      SPRITE_brick_wall,      CF_WALL,                STONE,  0, CELL_BRICK_WALL},
    [CELL_BRICK_FLOOR]    = {"brick floor",     SPRITE_brick_floor,     CF_GROUND,              STONE,  0, CELL_BRICK_FLOOR},
    [CELL_TILE_FLOOR]     = {"tile floor",      SPRITE_tile_floor,      CF_GROUND,              STONE,  0, CELL_TILE_FLOOR},
    [CELL_CARPET]         = {"carpet",          SPRITE_carpet,          CF_GROUND,              WOOD,  10, CELL_FLOOR},
    
    // === DOORS ===
    [CELL_DOOR_WOOD]      = {"wood door",       SPRITE_door_wood,       CF_WALL,                WOOD,  64, CELL_FLOOR},
    [CELL_DOOR_METAL]     = {"metal door",      SPRITE_door_metal,      CF_WALL,                STONE,  0, CELL_METAL_FLOOR},
    [CELL_DOOR_WOOD_OPEN] = {"open wood door",  SPRITE_door_wood_open,  CF_GROUND,              WOOD,  64, CELL_FLOOR},
    [CELL_DOOR_METAL_OPEN]= {"open metal door", SPRITE_door_metal_open, CF_GROUND,              STONE,  0, CELL_METAL_FLOOR},
    
    // === VERTICAL MOVEMENT ===
    [CELL_LADDER_UP]      = {"ladder up",       SPRITE_ladder_up,       CF_GROUND|CF_LADDER,    AIR,    0, CELL_LADDER_UP},
    [CELL_LADDER_DOWN]    = {"ladder down",     SPRITE_ladder_down,     CF_GROUND|CF_LADDER,    AIR,    0, CELL_LADDER_DOWN},
    [CELL_LADDER_BOTH]    = {"ladder",          SPRITE_ladder,          CF_GROUND|CF_LADDER,    AIR,    0, CELL_LADDER_BOTH},
    [CELL_RAMP_UP]        = {"ramp up",         SPRITE_ramp_up,         CF_GROUND|CF_RAMP,      AIR,    0, CELL_RAMP_UP},
    [CELL_RAMP_DOWN]      = {"ramp down",       SPRITE_ramp_down,       CF_GROUND|CF_RAMP,      AIR,    0, CELL_RAMP_DOWN},
    
    // === SPECIAL ===
    [CELL_AIR]            = {"air",             SPRITE_air,             0,                      AIR,    0, CELL_AIR},
    [CELL_WATER_SHALLOW]  = {"shallow water",   SPRITE_water_shallow,   CF_GROUND,              AIR,    0, CELL_WATER_SHALLOW},
    [CELL_LAVA]           = {"lava",            SPRITE_lava,            0,                      AIR,    0, CELL_LAVA},
    [CELL_VOID]           = {"void",            SPRITE_void,            CF_BLOCKS_MOVEMENT|CF_BLOCKS_FLUIDS, AIR, 0, CELL_VOID},
};
```

**Observations at 32 types:**

1. **Still manageable** - fits on one screen, easy to scan
2. **Grouping helps** - comments separate categories
3. **5 flag bits** - compact, readable combinations like `CF_GROUND|CF_LADDER`
4. **Flag combos** - `CF_WALL` and `CF_GROUND` reduce repetition
5. **Grate example** - shows `CF_BLOCKS_MOVEMENT` without `CF_BLOCKS_FLUIDS`
6. **~55 lines** - for 32 types with comments

**At 100+ types**, this would be ~170 lines. Still workable but:
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
