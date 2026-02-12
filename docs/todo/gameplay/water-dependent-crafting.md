# Water-Dependent Crafting System

Date: 2026-02-11
Status: Design proposal - requires multiple new systems

---

## Core Principle: Proximity-Based Resource Access

**The Pattern:** When you lack containers/inventory, workshops gain resources through **location-based access**:

- **Workshop on riverbed** → water available
- **Workshop under tree** → shade affects drying
- **Stockpile in cave** → stays dry
- **Workshop near fire** → heat affects curing

This creates **meaningful placement decisions** without needing container/inventory systems.

---

## Three Water-Dependent Workshop Archetypes

### 1. Wet Workshop (Must be on water)
Requires standing water to function.

**Examples:**
- **Clay Pit** - softens clay from hard chunks → workable wet clay
- **Mud Mixer** - DIRT + CLAY → MUD (primary use case)
- **Tanning Vat** - requires water for hide processing [future:animals]
- **Reed Soaker** - softens reeds for weaving [needs:ITEM_REEDS]
- **Quenching Station** - cools hot metal [future:metalworking]

**Implementation:** Check `HasWater(workshopX, workshopY, workshopZ)` as placement requirement + recipe prerequisite.

**Code location:** `src/entities/workshops.c` - add `requiresWater` flag to `WorkshopDef` struct.

### 2. Moisture Workshop (Benefits from nearby water)
Works anywhere but faster/better near water.

**Examples:**
- **Mud Mixer** (variant) - faster if near water source
- **Cob Former** - MUD + DRIED_GRASS → COB_BRICK (needs moisture)
- **Plant Fiber Processor** - GRASS → FIBER (retting needs moisture)

**Implementation:** Recipe time modifier based on `GetNearbyWaterCells(radius=3)`.

### 3. Drying Workshop (Needs DRY location)
Opposite of wet - needs low moisture.

**Examples:**
- **Drying Rack** (already implemented!) - GRASS → DRIED_GRASS
- **Seasoning Shed** - speeds GREEN_LOG → SEASONED_LOG
- **Brick Drying Yard** - WET_MUD_BRICK → BONE_DRY_BRICK

**Implementation:** Check stockpile environment tag or `GetNearbyWaterCells() == 0`.

**Code location:** `src/entities/workshops.c` - add `requiresDry` flag to `WorkshopDef` struct.

---

## Material State System (Moisture-Driven)

Extend the existing material system with **hydration states**.

### Proposed Item Fields

Add to `Item` struct in `src/entities/items.h`:
```c
typedef struct {
    // ... existing fields
    uint8_t moisture;    // MoistureState enum
    float moistureTimer; // Time since last moisture change
} Item;
```

### Moisture States

```c
typedef enum {
    MOISTURE_NONE,      // Rock, charcoal, etc. (never changes)
    MOISTURE_BONE_DRY,  // Seasoned wood, fired clay
    MOISTURE_DRY,       // Normal state
    MOISTURE_DAMP,      // Slow to season
    MOISTURE_WET,       // Workable clay/mud
    MOISTURE_SOAKED,    // Too wet - may rot (failure state)
} MoistureState;
```

### Environment Effects on Moisture

**Stockpile environment tags** (new field on `Stockpile` struct):
```c
typedef enum {
    ENV_NORMAL,   // Default - slow moisture change
    ENV_DRY,      // Accelerated drying
    ENV_WET,      // Accelerated wetting
    ENV_COVERED,  // Protected from weather
    ENV_OPEN,     // Exposed to weather
} StockpileEnvironment;
```

**Moisture transitions** (new tick system in `src/entities/items.c`):
- Items in wet stockpile → gradual moisture increase
- Items in dry stockpile → gradual moisture decrease
- Items near fire → accelerated drying
- Items underwater → become SOAKED
- Items in open stockpile + rain → get WET

**Code location:** New function `ItemMoistureTick(float dt)` in `src/entities/items.c`, called from `ItemsTick()`.

---

## Mud/Cob Item Chain

### New Items Needed

Add to `ItemType` enum in `src/entities/items.h`:
```c
ITEM_MUD,          // DIRT + CLAY + water → wet mixture
ITEM_COB,          // MUD + DRIED_GRASS → building material
ITEM_DAUB,         // MUD → thin coating for wattle walls
ITEM_MUD_BRICK,    // MUD → formed brick (needs drying)
ITEM_WATTLE_PANEL, // STICKS → lightweight wall frame
ITEM_PACKED_EARTH, // DIRT + GRAVEL → tamped floor material
```

**Material inheritance:**
- MUD inherits material from source CLAY (determines color/properties)
- COB/DAUB/MUD_BRICK inherit from MUD material

**Moisture states by item:**
- ITEM_MUD: always MOISTURE_WET (spoils if dried)
- ITEM_MUD_BRICK: WET → DAMP → DRY → BONE_DRY (passive progression)
- ITEM_COB: starts WET, must be used before drying

---

## Workshop Proposals

### Mud Mixer (2x2) - Wet Workshop
**Must be placed on water tile or adjacent.**

```
Layout: `WX`  (W = water tile or floor, X = work)
        `O.`

Placement rules:
- At least one workshop tile must have water (HasWater check)
- OR be adjacent to water tile (GetNearbyWaterCells(radius=1) > 0)
```

**Recipes:**
| Recipe | Input | Output | Time | Notes |
|--------|-------|--------|------|-------|
| Mix Mud | DIRT x2 + CLAY x1 | MUD x3 | 3s | Requires water access |
| Make Cob | MUD x2 + DRIED_GRASS x1 | COB x2 | 4s | Must use wet mud |
| Form Mud Brick | MUD x1 | MUD_BRICK x1 | 2s | Outputs WET brick |

**Implementation notes:**
- Add `PlaceWorkshop_MudMixer()` validation in `src/core/input.c`
- Check water access in `CanStartCraftJob()` in `src/entities/jobs.c`
- If no water access, display error "Mud Mixer requires water"

### Brick Drying Yard (3x3) - Drying Workshop
**Must be DRY (no water nearby).**

```
Layout: `...`
        `.X.`
        `...`  (all floor, open to air)

Placement rules:
- No water tiles within radius 3
- GetNearbyWaterCells(radius=3) == 0
```

**Recipe:**
| Recipe | Input | Output | Time | Type |
|--------|-------|--------|------|------|
| Dry Bricks | MUD_BRICK (WET) | MUD_BRICK (BONE_DRY) | Passive, 2-3 in-game days | Passive conversion |

**Similar to:** Existing Drying Rack (GRASS → DRIED_GRASS)

**Implementation notes:**
- Passive workshop like Charcoal Pit
- Checks moisture state of items in output tile
- Advances MOISTURE_WET → DAMP → DRY → BONE_DRY over time
- Final BONE_DRY bricks can be used in construction or fired in kiln

### Wattle Frame Builder (2x2)
**No special placement requirements.**

```
Layout: `#X`
        `O.`

Recipe:
- Weave Wattle: STICKS x4 → WATTLE_PANEL x1 (3s)
```

**Purpose:** Lightweight wall frames for wattle-and-daub construction.

### Mason's Bench (3x3)
**No special placement requirements.**

```
Layout: `###`
        `#XO`
        `..#`

Recipes:
- Daub Wattle: WATTLE_PANEL x1 + MUD x2 → WATTLE_DAUB_WALL x1 (5s)
- Pack Earth: DIRT x3 + GRAVEL x1 → PACKED_EARTH x2 (4s)
- Cob Wall: COB x4 → COB_WALL x1 (6s)
```

**Purpose:** Combines primitive materials into construction items.

---

## Construction Applications

### Wattle-and-Daub Walls (Multi-Stage)
Uses construction-staging.md system:

1. **Frame stage:** Place WATTLE_PANEL (sticks)
2. **Fill stage:** Apply MUD (x2) as daub coating
3. **Cure stage:** Wait for passive drying (MOISTURE_WET → DRY)
4. **Finish stage** (optional): Lime wash or plaster

**Blueprint requirements:**
- Stage 0: WATTLE_PANEL x1
- Stage 1: MUD x2 (must be MOISTURE_WET)
- Stage 2: Passive time delay (no input, just wait for moisture → dry)

### Cob Walls (Single-Stage but Needs Curing)
1. Place COB_WALL (made from COB x4)
2. Wall starts in MOISTURE_WET state
3. Passively cures to MOISTURE_DRY over 1-2 in-game days
4. Only when DRY does it gain full insulation/strength properties

**Blueprint requirements:**
- Single stage: COB_WALL x1
- Wall has `moisture` field like items
- Wall properties (insulation, HP) improve as it dries

### Mud Floors
1. Place MUD directly as floor material
2. Movers "tamp" by walking over it (groundwear system)
3. Cures to PACKED_EARTH over time
4. Optional: sealed with oil/wax later

---

## Placement Validation System

### New Workshop Definition Fields

Add to `WorkshopDef` struct in `src/entities/workshop_defs.h`:
```c
typedef struct {
    // ... existing fields
    bool requiresWater;  // Must have water in/adjacent to footprint
    bool requiresDry;    // Must NOT have water nearby (radius 3)
    bool requiresCover;  // Must have roof/ceiling above [future]
} WorkshopDef;
```

### Validation Function

Add to `src/entities/workshops.c`:
```c
bool ValidateWorkshopPlacement(WorkshopType type, int x, int y, int z) {
    WorkshopDef* def = &workshopDefs[type];

    if (def->requiresWater) {
        // Check workshop footprint for water
        bool hasWater = false;
        for (int dy = 0; dy < def->height; dy++) {
            for (int dx = 0; dx < def->width; dx++) {
                if (HasWater(x + dx, y + dy, z)) {
                    hasWater = true;
                    break;
                }
            }
        }
        if (!hasWater) {
            // Check adjacent tiles (1-tile radius)
            if (GetNearbyWaterCells(x, y, z, 1) == 0) {
                AddMessage("Workshop requires water access", RED);
                return false;
            }
        }
    }

    if (def->requiresDry) {
        // Must NOT have water nearby
        if (GetNearbyWaterCells(x, y, z, 3) > 0) {
            AddMessage("Workshop requires dry location (no water nearby)", RED);
            return false;
        }
    }

    // ... existing checks (walkability, clearance, etc.)
    return true;
}
```

**Call site:** `PlaceWorkshop()` in `src/core/input.c` - add validation before blueprint creation.

---

## Implementation Order

### Phase 0: Foundation (No new workshops yet)
1. Add `moisture` and `moistureTimer` fields to `Item` struct
2. Add `MoistureState` enum
3. Add `ItemMoistureTick()` function (no-op for now, just tick timer)
4. **Test:** Items can have moisture states, timer advances

### Phase 1: Simple Passive Drying (Proof of Concept)
1. Extend `ItemMoistureTick()` to transition states over time
2. Add ITEM_MUD_BRICK with moisture states
3. Create **Brick Drying Yard** workshop (passive, like Drying Rack)
4. **Test:** Place wet mud bricks, watch them dry over time

### Phase 2: Water-Dependent Workshop
1. Add `requiresWater` and `requiresDry` flags to `WorkshopDef`
2. Implement `ValidateWorkshopPlacement()` function
3. Create **Mud Mixer** workshop (requires water)
4. Add ITEM_MUD, ITEM_COB recipes
5. **Test:** Can only place Mud Mixer near water, can craft mud

### Phase 3: Stockpile Environment Tags
1. Add `StockpileEnvironment` enum and field to `Stockpile` struct
2. Add UI for setting stockpile environment (keybind when hovering)
3. Modify `ItemMoistureTick()` to check item's stockpile environment
4. **Test:** Items in wet stockpile get wetter, items in dry stockpile dry faster

### Phase 4: Construction Integration
1. Add moisture checks to blueprint stages (construction-staging.md)
2. Add ITEM_WATTLE_PANEL, ITEM_DAUB
3. Create multi-stage wattle-and-daub wall blueprint
4. **Test:** Can build wattle walls with proper material states

### Phase 5: Advanced Features (Later)
1. Weather system affects moisture (rain makes open stockpiles wet)
2. Container system allows water hauling (removes "must be on water" restriction)
3. Quality system: slow-dried = high quality, fast-dried = cracks
4. Failure states: rotten wood, cracked clay, moldy storage

---

## Gameplay Implications

### Early Game Progression
**No sawmill yet:**
1. Gather sticks by hand (or snap branches)
2. Build Mud Mixer on riverbank
3. Mix mud (dirt + clay + water)
4. Build wattle frames from sticks
5. Daub frames with mud
6. Thatch roof with dried grass
7. **First shelter:** Wattle-and-daub hut with thatch roof

**First permanent structure:**
1. Form mud bricks at Mud Mixer
2. Haul wet bricks to Drying Yard (inland, dry location)
3. Wait 2-3 in-game days for drying
4. Fire dried bricks in Kiln → hard ceramic bricks
5. Build brick walls for permanent buildings

### Meaningful Placement Decisions

**Mud Mixer placement trade-offs:**
- **On riverbank:** Convenient water access, but flood risk if water level rises
- **Inland with water haul:** Safe from floods, but need containers (future)

**Drying Yard placement trade-offs:**
- **Near mud source:** Short haul from mixer, but may be too wet
- **Inland/elevated:** Ideal drying conditions, but long haul distance

**Stockpile strategy:**
- **Wet stockpile near river:** Keeps clay workable, prevents drying
- **Dry covered shed:** Seasons wood quickly, prevents rot
- **Open yard:** Free but exposed to weather (future rain/snow)

### Logistical Loops

**Brick production chain:**
```
River → Mud Mixer (wet) → MUD_BRICK (wet)
     → Drying Yard (dry) → MUD_BRICK (dry)
     → Kiln (fuel) → BRICKS (fired)
     → Construction
```

**Wattle-daub chain:**
```
Trees → STICKS → Wattle Builder → WATTLE_PANEL
Clay → Mud Mixer → MUD
     → Mason's Bench → WATTLE_DAUB_WALL
     → Construction (cure in place)
```

**Cob chain:**
```
Grass → Drying Rack → DRIED_GRASS
Clay + Dirt → Mud Mixer → MUD
     → Mud Mixer (cob recipe) → COB
     → Mason's Bench → COB_WALL
     → Construction (thick walls, good insulation)
```

---

## Future Extensions

### Container System (when added)
- **Bucket** carries water from river to workshop
- Removes "must be on riverbed" restriction
- Adds hauling job complexity: fetch water, deliver to workshop
- Still incentivizes riverside placement (less hauling labor)

**Implementation:** Add ITEM_BUCKET, add "fetch water" job step, workshop consumes water item instead of checking location.

### Weather System (when added)
- Rain floods open workshops
- Covered workshops stay dry automatically
- Seasonal moisture affects curing times
- Adds roof construction importance

**Implementation:** Global `isRaining` flag, apply to open stockpiles and workshops, accelerate moisture gain.

### Quality System (when added)
- Slow-dried bricks = higher quality (better insulation)
- Fast-dried (near fire) = cracks, lower durability
- Properly seasoned cob = durable walls
- Rushed mud work = weak structures, collapse risk

**Implementation:** Add `quality` field to items/walls, based on moisture transition speed.

---

## Prerequisites

### Terrain Sculpting (Recommended) ⭐
**Best approach:** General-purpose terrain editing enables water features naturally.

See: `docs/todo/design/terrain-sculpting-brushes.md`

**What you get:**
- **Lower Terrain brush** - carve river channels with freehand drawing + adjustable diameter
- **Raise Terrain brush** - build dams, embankments to control water flow
- Water flows into carved channels naturally (no special water code needed)
- Bonus: Also enables moats, terrain leveling, terraced hillsides, etc.

**Why this is better:**
- More flexible: one tool, many uses (not just water)
- Intuitive: "paint terrain up/down" with brush size control
- Natural water integration: carve low ground → water fills it

**Estimated time:** ~8 hours for creative mode (instant sculpting)

### Alternative: Water Placement Tools
**Simpler but less flexible** - specific water source/drain placement.

See: `docs/todo/design/water-placement-tools.md`

**What you get:**
- Place Water Source (spring) - infinite water at a point
- Place Water Drain (outflow) - removes water at a point

**When to use:**
- Just want to add water to existing terrain (no sculpting)
- Don't need terrain editing features

**Estimated time:** ~5 hours for basic placement

**Recommendation:** Implement terrain sculpting instead - it's more powerful and unlocks many use cases beyond water.

---

## Technical Dependencies

### Existing Systems Leveraged
- ✅ Water grid already tracks water levels (`src/simulation/water.c`)
- ✅ Passive workshop system (Drying Rack, Charcoal Pit)
- ✅ Material field on items (for clay types → mud colors)
- ✅ Stockpile filtering by item type
- ✅ Multi-input recipes (dirt + clay)
- ✅ Multi-output recipes (for future)

### New Systems Required
- ❌ Moisture state enum on items (MoistureState)
- ❌ Moisture tick system (like temperature/fire ticks)
- ❌ Workshop placement validators (water/dry requirements)
- ❌ Environment tags on stockpiles (isDry, isWet, isCovered)
- ❌ Construction staging with moisture checks (ties to construction-staging.md)
- ❌ Passive moisture transitions (WET → DAMP → DRY → BONE_DRY)

### Code Files to Modify

**Core changes:**
- `src/entities/items.h` - add moisture fields
- `src/entities/items.c` - add ItemMoistureTick()
- `src/entities/workshops.h` - add requiresWater/requiresDry flags
- `src/entities/workshops.c` - add ValidateWorkshopPlacement()
- `src/entities/workshop_defs.h` - update WorkshopDef struct
- `src/entities/stockpiles.h` - add StockpileEnvironment enum/field

**New workshops:**
- `src/entities/workshop_defs.c` - add Mud Mixer, Brick Yard, Wattle Builder, Mason's Bench definitions

**UI/Input:**
- `src/core/input.c` - add placement validation calls, stockpile environment keybinds
- `src/render/tooltips.c` - display moisture states, stockpile environment

**Save/Load:**
- `src/core/save_migrations.h` - bump SAVE_VERSION, add legacy structs if Item/Stockpile size changes
- `src/core/saveload.c` - save/load moisture fields

---

## Open Questions

1. **Moisture tick rate:** How often should moisture states update? (Every second? Every 10 seconds? Once per in-game hour?)

2. **Moisture radius checks:** How far should workshops search for water? (1 tile? 3 tiles? Entire footprint + 1?)

3. **Stockpile environment UI:** How should players set/change environment tags? (Keybind toggle? Menu? Auto-detect based on location?)

4. **Construction moisture:** Should walls/floors have moisture states, or only items? (If walls cure in place, they need moisture tracking)

5. **Water level changes:** If river water level drops, does Mud Mixer stop working? Or does it remember "was placed on water"?

6. **Flood damage:** If water rises and floods a Drying Yard, do bricks become WET again? (Probably yes, adds risk)

7. **Container hauling:** When containers are added, does water need to be "fresh" or can it be stored indefinitely?

---

## Summary

This system enables **primitive construction** (mud, cob, wattle-daub) by:
1. Using **location-based resource access** (water at workshop site, no containers needed)
2. Adding **moisture as a material state** (wet mud → dry brick)
3. Creating **meaningful placement decisions** (wet workshops near water, dry yards inland)
4. Integrating with **existing passive systems** (like Drying Rack, Charcoal Pit)
5. Supporting **future extensions** (containers, weather, quality) without breaking existing gameplay

**Key insight:** By making water a location-dependent resource rather than a hauled item, we enable water-based crafting *now* without waiting for containers/inventory systems. When containers are added later, they become an *optimization* (less hauling) rather than a requirement.
