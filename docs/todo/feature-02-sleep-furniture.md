# Feature 02: Sleep & Furniture

> **Priority**: Tier 1 — Survival Loop
> **Why now**: Colonists never rest. Buildings have no interior purpose. Planks have one sink (construction). Leaves have no recipe sink.
> **Builds on**: Feature 1 (freetime state machine — COMPLETE, `src/simulation/needs.c`)
> **Opens**: Buildings need rooms (F3), furniture needs tools to craft faster (F4)

---

## What Changes

Movers get an **energy** stat that drains while awake. When exhausted, they seek rest. **Furniture** is a new entity pool. Primitive rest (leaf pile) uses raw materials via the construction system. Proper furniture (plank bed, chair) is crafted at a **carpenter's bench** then installed via construction blueprint. Furniture placement lives under the existing Construction action menu.

---

## Design Decisions (Resolved)

1. **Furniture walkability**: per-type flag. Beds block (`CELL_FLAG_WORKSHOP_BLOCK`). Chairs add movement cost via `GetCellMoveCost()`.
2. **Primitive beds**: leaf pile (LEAVES x4, direct construction). No workshop needed.
3. **Placement flow**: all furniture placed via construction blueprints under the BUILD menu. Raw-material furniture (leaf pile) = mover hauls leaves to site and builds. Pre-crafted furniture (plank bed) = mover hauls the item to site and installs.
4. **Shared freetime fields**: sleep reuses `needTarget`, `needProgress`, `needSearchCooldown` from F1. Only new Mover field is `float energy`.
5. **Furniture pool**: fixed-size array like workshops/animals. Linear scan for lookup (MAX_FURNITURE is small).
6. **Sprites**: 3 new sprites needed in atlas. Placeholder color blocks are fine for Phase 1.

---

## Energy Numbers

Default day = 60 real seconds. Night = ~20s real. Goal: work one day, sleep one night.

```
energy: float 0.0 (exhausted) → 1.0 (rested)

Drain:
  idle:    0.010/s
  working: 0.018/s

Recovery:
  ground:     0.012/s
  leaf pile:  0.020/s
  chair:      0.015/s
  plank bed:  0.040/s
```

### Worked Example (default 60s day, 1x speed — all rates use `gameDeltaTime` so they scale with game speed automatically)

```
Mover spawns at energy 1.0.
Works all day: 0.018/s * 39s = 0.70 drained → energy 0.30 (TIRED threshold)
Seeks bed. Walks 3s. Energy now 0.27.
Sleeps in plank bed: 0.040/s * 18s → energy 0.99. Wakes (>0.8 threshold).
Total cycle: ~60s real = 1 game day. Feels right.

Without bed (ground): 0.012/s * 58s to recover 0.3→1.0. Almost a full day wasted.
Leaf pile: 0.020/s * 35s. Better but still slow.
Chair: 0.015/s * 47s. Worse than leaf pile — chairs aren't for sleeping.
```

### Thresholds
- 1.0–0.3: **Rested** — normal work
- 0.3–0.1: **Tired** — seek rest when idle (same priority as Hungry)
- <0.1: **Exhausted** — interrupt job, seek rest immediately (same priority as Starving)

### Priority Order
```
Starving (hunger < 0.1)  >  Exhausted (energy < 0.1)  >  Hungry (hunger < 0.3)  >  Tired (energy < 0.3)
```

Hunger always wins over energy at equal severity — you can't sleep if you're starving.

---

## Struct Definitions

### Furniture

```c
// src/entities/furniture.h

#define MAX_FURNITURE 512

typedef enum {
    FURNITURE_NONE = 0,
    FURNITURE_LEAF_PILE,
    FURNITURE_PLANK_BED,
    FURNITURE_CHAIR,
    FURNITURE_TYPE_COUNT,
} FurnitureType;

typedef struct {
    const char* name;
    float restRate;       // Energy recovery per second (0 = no rest)
    bool blocking;        // true = CELL_FLAG_WORKSHOP_BLOCK, false = movement penalty
    int moveCost;         // GetCellMoveCost value when non-blocking (0 = no penalty)
} FurnitureDef;

typedef struct {
    int x, y, z;
    bool active;
    FurnitureType type;
    uint8_t material;     // MaterialType (inherited from construction input)
    int occupant;         // Mover index (-1 = unoccupied)
} Furniture;

extern Furniture furniture[MAX_FURNITURE];
extern int furnitureCount;
```

### FurnitureDef Table

```c
// src/entities/furniture.c

static const FurnitureDef furnitureDefs[FURNITURE_TYPE_COUNT] = {
    [FURNITURE_NONE]       = { "None",       0.0f,   false, 0  },
    [FURNITURE_LEAF_PILE]  = { "Leaf Pile",  0.020f, false, 12 },  // 0.83x speed
    [FURNITURE_PLANK_BED]  = { "Plank Bed",  0.040f, true,  0  },  // blocking
    [FURNITURE_CHAIR]      = { "Chair",      0.015f, false, 11 },  // 0.91x speed, rest worse than leaf pile
};
```

### Mover Change (single field)

```c
// In Mover struct, after hunger field:
float energy;               // 1.0=rested, 0.0=exhausted
```

### New FreetimeState Values

```c
// Extend existing enum in mover.h:
typedef enum {
    FREETIME_NONE,
    FREETIME_SEEKING_FOOD,
    FREETIME_EATING,
    FREETIME_SEEKING_REST,   // NEW — walking to furniture/ground spot
    FREETIME_RESTING,        // NEW — sleeping/sitting, recovering energy
} FreetimeState;
```

`needTarget` repurposed: in SEEKING_REST/RESTING, it holds a furniture pool index (not item index). Value -1 = ground rest (no furniture). This is safe because a mover is never eating and sleeping at the same time.

### New Constants

```c
// Energy drain rates
#define ENERGY_DRAIN_IDLE    0.010f   // Per real second
#define ENERGY_DRAIN_WORKING 0.018f   // Per real second

// Energy thresholds
#define ENERGY_TIRED_THRESHOLD    0.3f  // Seek rest when idle
#define ENERGY_EXHAUSTED_THRESHOLD 0.1f // Interrupt job

// Rest ground rate (no furniture)
#define ENERGY_GROUND_RATE 0.012f

// Wake threshold
#define ENERGY_WAKE_THRESHOLD 0.8f

// Rest search cooldown (same as food)
#define REST_SEARCH_COOLDOWN 5.0f

// Rest seek timeout (same as food)
#define REST_SEEK_TIMEOUT 10.0f
```

---

## New Items

```c
// Add to ItemType enum in items.h:
ITEM_PLANK_BED,     // Crafted at carpenter, placed as furniture
ITEM_CHAIR,         // Crafted at carpenter, placed as furniture
```

Both get `IF_STACKABLE | IF_BUILDING_MAT` flags. Material inherited from input.

---

## New Construction Recipes

### BuildCategory Extension

```c
// Add to BuildCategory enum in construction.h:
BUILD_FURNITURE,   // NEW — completion spawns Furniture entity
```

### Recipe Definitions

```c
// Add to ConstructionRecipeIndex enum:
CONSTRUCTION_LEAF_PILE,     // LEAVES x4 → FURNITURE_LEAF_PILE
CONSTRUCTION_PLANK_BED,     // ITEM_PLANK_BED x1 → FURNITURE_PLANK_BED
CONSTRUCTION_CHAIR,         // ITEM_CHAIR x1 → FURNITURE_CHAIR
```

Each recipe needs a `furnitureType` field on ConstructionRecipe (or derive from recipe index via lookup table).

**Leaf pile recipe** (raw materials, single stage):
```
name: "Leaf Pile"
buildCategory: BUILD_FURNITURE
stageCount: 1
stages[0]: { inputs: [{ ITEM_LEAVES x4 }] }
resultFurniture: FURNITURE_LEAF_PILE
```

**Pre-crafted furniture recipes** (single item, single stage):
```
name: "Plank Bed"
buildCategory: BUILD_FURNITURE
stageCount: 1
stages[0]: { inputs: [{ ITEM_PLANK_BED x1 }] }
resultFurniture: FURNITURE_PLANK_BED
```

### CompleteBlueprint Modification

In `designations.c:CompleteBlueprint()`, add an `if` block for `BUILD_FURNITURE` (existing code uses if-chains, not switch/case):

```c
if (recipe->buildCategory == BUILD_FURNITURE) {
    FurnitureType ftype = GetFurnitureTypeForRecipe(recipe->recipeIndex);
    int fi = SpawnFurniture(bp->x, bp->y, bp->z, ftype, finalMat);
    if (fi >= 0 && furnitureDefs[ftype].blocking) {
        SET_CELL_FLAG(bp->x, bp->y, bp->z, CELL_FLAG_WORKSHOP_BLOCK);
        PushMoversOutOfCell(bp->x, bp->y, bp->z);
    }
    InvalidatePathsThroughCell(bp->x, bp->y, bp->z);
}
```

Note: use `finalMat` (the local variable from `GetRecipeFinalMaterial`), not `resultMaterial`.

### Placement Validation

Before creating a furniture blueprint, validate that the target cell:
1. Is walkable (same as existing blueprint validation)
2. Has no existing furniture (`GetFurnitureAt(x, y, z) < 0`)
3. Has no `CELL_FLAG_WORKSHOP_BLOCK` set (prevents placing inside workshops)

This check goes in `input.c` where the blueprint is created, not in `CompleteBlueprint`. Reject silently or show a status message.

---

## New Workshop: Carpenter's Bench

```c
// Add to WorkshopType enum:
WORKSHOP_CARPENTER,
```

**Template (3x3):**
```
. O .
# X #
. . .
```

**Recipes:**
| Recipe | Input | Output | Time |
|--------|-------|--------|------|
| Craft Plank Bed | PLANKS x4 | ITEM_PLANK_BED | 8s |
| Craft Chair | PLANKS x2 | ITEM_CHAIR | 5s |

---

## Action Registry

Furniture placement goes under the existing BUILD submode (MODE_WORK + SUBMODE_BUILD), alongside walls/floors/ladders/ramps.

```c
// New actions in input_mode.h:
ACTION_WORK_FURNITURE,   // Category — shows furniture sub-options

// New entries in action_registry.c:
{
    .action = ACTION_WORK_FURNITURE,
    .name = "FURNITURE",
    .barDisplayText = "Furniture",
    .barKey = 'f',
    .requiredMode = MODE_WORK,
    .requiredSubMode = SUBMODE_BUILD,
    .parentAction = ACTION_NONE,
    .canDrag = true,
    .canErase = true,
}
```

Inside ACTION_WORK_FURNITURE, player presses R to cycle furniture recipes (same pattern as wall recipe cycling with `GetConstructionRecipeIndicesForCategory(BUILD_FURNITURE, ...)`).

**Action bar shows**: `Furniture: Leaf Pile [R]cycle  L-drag designate  R-drag cancel  [ESC]Back`

**Recipe selection**: `static int selectedFurnitureRecipe = CONSTRUCTION_LEAF_PILE;`

---

## Sleep Behavior

### Energy Drain (in NeedsTick — mover.c, not needs.c)

```c
// After hunger drain, add:
if (m->freetimeState != FREETIME_RESTING) {
    float drainRate = (m->currentJobId >= 0) ? ENERGY_DRAIN_WORKING : ENERGY_DRAIN_IDLE;
    m->energy -= drainRate * gameDeltaTime;
    if (m->energy < 0.0f) m->energy = 0.0f;
}
```

Energy does NOT drain during RESTING state.

### State Machine (in ProcessMoverFreetime)

Add cases after existing FREETIME_EATING handling:

**FREETIME_NONE — full priority chain (replaces existing hunger-only checks):**
```c
// Priority: starving > exhausted > hungry > tired
// Use else-if so only one action fires per tick.
if (m->hunger < HUNGER_CANCEL_THRESHOLD) {
    // STARVING — cancel job, seek food
    if (m->currentJobId >= 0) CancelJob(m, moverIdx);
    if (m->needSearchCooldown <= 0.0f) StartFoodSearch(m, moverIdx);
} else if (m->energy < ENERGY_EXHAUSTED_THRESHOLD) {
    // EXHAUSTED — cancel job, seek rest
    if (m->currentJobId >= 0) CancelJob(m, moverIdx);
    if (m->needSearchCooldown <= 0.0f) StartRestSearch(m, moverIdx);
} else if (m->hunger < HUNGER_SEARCH_THRESHOLD && m->currentJobId < 0) {
    // HUNGRY — seek food (don't cancel jobs)
    if (m->needSearchCooldown <= 0.0f) StartFoodSearch(m, moverIdx);
} else if (m->energy < ENERGY_TIRED_THRESHOLD && m->currentJobId < 0) {
    // TIRED — seek rest (don't cancel jobs)
    if (m->needSearchCooldown <= 0.0f) StartRestSearch(m, moverIdx);
}
```

This replaces the existing hunger-only checks in FREETIME_NONE — it's not added after them.

**StartRestSearch:**
1. Scan furniture pool for unoccupied furniture, prefer highest restRate, weight by distance
2. If found: reserve (`furniture[fi].occupant = moverIdx`), set goal, enter FREETIME_SEEKING_REST, `needTarget = fi`
3. If not found: enter FREETIME_SEEKING_REST with `needTarget = -1` (ground rest at current position, no movement needed — skip straight to RESTING)
4. If not found and cooldown: set `needSearchCooldown = REST_SEARCH_COOLDOWN`

**FREETIME_SEEKING_REST:**
```c
// Validate target still exists (if furniture)
if (m->needTarget >= 0) {
    Furniture* f = &furniture[m->needTarget];
    if (!f->active || f->occupant != moverIdx) {
        // Lost reservation — reset
        m->freetimeState = FREETIME_NONE;
        m->needSearchCooldown = REST_SEARCH_COOLDOWN;
        break;
    }
    // Check arrival (same pattern as SEEKING_FOOD)
    float dx = m->x - (f->x * CELL_SIZE + CELL_SIZE/2);
    float dy = m->y - (f->y * CELL_SIZE + CELL_SIZE/2);
    if (dz_same && (dx*dx + dy*dy) < (CELL_SIZE * 0.75f) * (CELL_SIZE * 0.75f)) {
        m->freetimeState = FREETIME_RESTING;
        m->needProgress = 0.0f;
        break;
    }
}
// Timeout
m->needProgress += gameDeltaTime;
if (m->needProgress > REST_SEEK_TIMEOUT) {
    ReleaseFurniture(m->needTarget, moverIdx);
    m->needTarget = -1;
    m->freetimeState = FREETIME_NONE;
    m->needSearchCooldown = REST_SEARCH_COOLDOWN;
}
```

**FREETIME_RESTING:**
```c
m->timeWithoutProgress = 0.0f;  // Prevent stuck detector

float rate = ENERGY_GROUND_RATE;
if (m->needTarget >= 0) {
    rate = furnitureDefs[furniture[m->needTarget].type].restRate;
}
m->energy += rate * gameDeltaTime;
if (m->energy > 1.0f) m->energy = 1.0f;

// Wake conditions
if (m->energy >= ENERGY_WAKE_THRESHOLD) {
    ReleaseFurniture(m->needTarget, moverIdx);
    m->needTarget = -1;
    m->freetimeState = FREETIME_NONE;
}
// Starvation interrupt
if (m->hunger < 0.1f) {
    ReleaseFurniture(m->needTarget, moverIdx);
    m->needTarget = -1;
    m->freetimeState = FREETIME_NONE;
    // Next tick: hunger check will trigger SEEKING_FOOD
}
```

### Helper Functions

```c
void ReleaseFurniture(int furnitureIdx, int moverIdx) {
    if (furnitureIdx >= 0 && furniture[furnitureIdx].occupant == moverIdx) {
        furniture[furnitureIdx].occupant = -1;
    }
}

// Call when a mover is deleted/deactivated — scans furniture pool for stale occupants
void ReleaseFurnitureForMover(int moverIdx) {
    for (int i = 0; i < MAX_FURNITURE; i++) {
        if (furniture[i].active && furniture[i].occupant == moverIdx) {
            furniture[i].occupant = -1;
        }
    }
}
```

**Important:** `ReleaseFurnitureForMover` must be called from any mover deletion/deactivation path (e.g., `ClearMovers()`, mover death if added later). Otherwise a deleted mover leaves a permanently reserved furniture slot.

---

## GetCellMoveCost Modification

In `cell_defs.h:GetCellMoveCost()`, add after existing checks (bush, snow, etc.):

```c
// Furniture movement penalty (non-blocking furniture only)
int fi = GetFurnitureAt(x, y, z);
if (fi >= 0) {
    int furnCost = furnitureDefs[furniture[fi].type].moveCost;
    if (furnCost > cost) cost = furnCost;
}
```

This automatically propagates to A*/HPA* pathfinding and mover movement. Movers prefer to path around furniture but can walk through non-blocking pieces.

**Note:** `GetCellMoveCost` is a `static inline` function in `cell_defs.h`. Calling `GetFurnitureAt()` there would require `cell_defs.h` to include `furniture.h`, creating a header dependency. Two options:
- **Option A (recommended):** Add a `uint8_t furnitureMoveCostGrid[z][y][x]` (0=no furniture, >0=move cost). Updated on SpawnFurniture/RemoveFurniture. O(1) lookup, no header dependency — `GetCellMoveCost` just reads the grid directly. Same pattern as mudGrid/snowGrid.
- **Option B:** Forward-declare `GetFurnitureAt()` and `GetFurnitureMoveCost()` in `cell_defs.h` with `int` return types (no struct dependency). Works but slightly awkward.

Start with Option A. The grid costs `MAX_GRID_DEPTH * MAX_GRID_HEIGHT * MAX_GRID_WIDTH` bytes (same as snowGrid) and keeps the hot path fast.

---

## Furniture Deconstruction

**Designation**: `DESIGNATION_DECONSTRUCT` (new). Player selects "Deconstruct" action under Work mode, designates furniture tile.

**Job**: `JOBTYPE_DECONSTRUCT` — 2-step (walk → work 2s). On completion:
1. If blocking: `CLEAR_CELL_FLAG(x, y, z, CELL_FLAG_WORKSHOP_BLOCK)`
2. Remove furniture from pool (`furniture[fi].active = false; furnitureCount--`)
3. Optionally drop partial materials (leaf pile → 2 leaves, bed → 2 planks). Or drop nothing for simplicity.
4. `InvalidatePathsThroughCell(x, y, z)`

Can defer to Phase 6 if needed — not critical for the sleep loop to function.

---

## Tooltip

Mover tooltip extension (in tooltips.c):

```
Energy: 72%  Tired in 4.2h
```

Calculation: `hoursUntilTired = (energy - 0.3) / (drainRate * secondsPerGameHour)` where `secondsPerGameHour = dayLength / 24.0`.

When RESTING:
```
Energy: 45%  Resting (Plank Bed)  Full in 3.5h
```

---

## Save Version Impact

Bump save version (currently 52 → 53). No old saves exist, so no migration code needed — just bump the version and write the new format.

**New Mover field**: `float energy` (written/read alongside existing fields, init 1.0).

**New entity section**: FURNITURE
```
furnitureCount
furniture[0..count-1]: { x, y, z, active, type, material, occupant }
```

**New items**: ITEM_PLANK_BED, ITEM_CHAIR — increases ITEM_TYPE_COUNT from 31 to 33. Stockpile `allowedTypes` array grows accordingly.

---

## Sprites Needed

3 new entries in `atlas8x8.h`:
- `SPRITE_furniture_leaf_pile`
- `SPRITE_furniture_bed`
- `SPRITE_furniture_chair`

Placeholder: use existing sprites with tint (e.g., `SPRITE_crate_green` tinted brown for bed). Replace with real art later.

---

## File-by-File Change List

### New Files
| File | Contents |
|------|----------|
| `src/entities/furniture.h` | Furniture struct, FurnitureDef, enums, pool globals, function declarations |
| `src/entities/furniture.c` | FurnitureDef table, SpawnFurniture, RemoveFurniture, GetFurnitureAt, ReleaseFurniture, ClearFurniture, save/load helpers |
| `tests/test_furniture.c` | Test suite (see below) |

### Modified Files
| File | Changes |
|------|---------|
| `src/entities/mover.h` | Add `float energy` to Mover struct. Add FREETIME_SEEKING_REST/RESTING to enum. Add energy constants. |
| `src/entities/mover.c` | Init energy=1.0 in InitMover(). Energy drain in NeedsTick() (lives here, not needs.c). Call `ReleaseFurnitureForMover()` in ClearMovers(). |
| `src/simulation/needs.h` | No new declarations needed (NeedsTick is in mover.h). |
| `src/simulation/needs.c` | SEEKING_REST/RESTING states in ProcessMoverFreetime(). StartRestSearch(), rest search helpers. Starvation interrupt during rest. |
| `src/entities/items.h` | Add ITEM_PLANK_BED, ITEM_CHAIR to enum. |
| `src/entities/item_defs.c` | Definitions for new items (stackable, building_mat). |
| `src/entities/workshops.h` | Add WORKSHOP_CARPENTER to enum. |
| `src/entities/workshops.c` | Carpenter workshop def (template, size). Carpenter recipes (plank bed, chair). |
| `src/world/construction.h` | Add BUILD_FURNITURE to BuildCategory. Add CONSTRUCTION_LEAF_PILE/PLANK_BED/CHAIR to recipe enum. Add furnitureType field to ConstructionRecipe (or lookup table). |
| `src/world/construction.c` | Define 3 furniture construction recipes. |
| `src/world/designations.c` | Handle BUILD_FURNITURE in CompleteBlueprint(). |
| `src/world/cell_defs.h` | Add furniture check in GetCellMoveCost(). |
| `src/core/input_mode.h` | Add ACTION_WORK_FURNITURE. |
| `src/core/action_registry.c` | Registry entry for ACTION_WORK_FURNITURE. |
| `src/core/input.c` | Handle ACTION_WORK_FURNITURE (recipe cycling, blueprint creation). Add selectedFurnitureRecipe static. |
| `src/core/saveload.c` | Bump SAVE_VERSION to 53. Save/load furniture pool. Energy field on movers. New item types in format. |
| `src/core/inspect.c` | Furniture inspection output. |
| `src/render/rendering.c` | Render furniture sprites at furniture positions. |
| `src/render/tooltips.c` | Energy display in mover tooltip. |
| `assets/atlas8x8.h` | 3 new sprite entries. |
| `src/unity.c` | Add `#include "entities/furniture.c"` |
| `tests/test_unity.c` | Add `#include "test_furniture.c"` |

---

## Implementation Phases

### Phase 1: Energy + Ground Sleep (~1-2 sessions)
**Goal**: Movers get tired and sleep on the ground. No furniture yet.

1. Add `float energy` to Mover, init 1.0
2. Energy drain in NeedsTick() (working vs idle rate)
3. Add FREETIME_SEEKING_REST, FREETIME_RESTING to enum
4. Priority logic in ProcessMoverFreetime: starving > exhausted > hungry > tired
5. Ground rest: SEEKING_REST with needTarget=-1 skips to RESTING immediately at current position
6. RESTING: recover at ENERGY_GROUND_RATE, wake at 0.8, starvation interrupts
7. Bump save version, migration (energy defaults to 1.0)
8. Energy in mover tooltip
9. Sleeping visual cue: tint sleeping movers blue-gray or draw "Z" text above them (a stationary mover with no feedback looks like a bug during playtesting)
10. **Tests**: drain rates, threshold triggers, ground rest recovery, wake condition, starvation interrupt, priority ordering (6-8 tests, ~20 assertions)

### Phase 2: Furniture Entity Pool (~1 session)
**Goal**: Furniture can be created programmatically and affects walkability/movement.

1. New files: furniture.h, furniture.c
2. FurnitureDef table, Furniture pool
3. SpawnFurniture, RemoveFurniture, GetFurnitureAt, ClearFurniture
4. Blocking: SET/CLEAR CELL_FLAG_WORKSHOP_BLOCK + PushMoversOutOfCell
5. Movement penalty: GetCellMoveCost check
6. InvalidatePathsThroughCell on spawn/remove
7. Save/load furniture pool
8. Rendering (placeholder sprites)
9. **Tests**: spawn/remove, blocking affects walkability, non-blocking affects move cost, occupant reserve/release, GetFurnitureAt lookup (5-6 tests, ~15 assertions)

### Phase 3: Furniture Rest Seeking (~1 session)
**Goal**: Tired movers find and use furniture to rest.

1. StartRestSearch: scan furniture pool, prefer best restRate, reserve occupant
2. SEEKING_REST with furniture target: validate each tick, arrival check, timeout
3. RESTING with furniture: recover at furniture rate (not ground rate)
4. Priority: plank bed > leaf pile > chair > ground
5. Release furniture on wake/interrupt
6. **Tests**: mover finds best furniture, reserves it, sleeps, recovers at correct rate, wakes, releases. Two movers competing for one bed. Starvation interrupt releases bed. (5-6 tests, ~20 assertions)

### Phase 4: Construction Integration (~1-2 sessions)
**Goal**: Player can place furniture via construction menu.

1. BUILD_FURNITURE category, 3 construction recipes
2. CompleteBlueprint handles BUILD_FURNITURE → SpawnFurniture
3. ACTION_WORK_FURNITURE with recipe cycling
4. Leaf pile recipe (LEAVES x4)
5. New items: ITEM_PLANK_BED, ITEM_CHAIR
6. Item defs, stockpile filters
7. **Tests**: blueprint created, materials delivered, completion spawns furniture entity, furniture is functional for rest (3-4 tests, ~10 assertions)

### Phase 5: Carpenter's Bench (~1 session)
**Goal**: Workshop produces furniture items.

1. WORKSHOP_CARPENTER: template, workshop def
2. Carpenter recipes: plank bed, chair
3. Action registry entry for placing carpenter (under WORKSHOP category)
4. **Tests**: carpenter crafts bed item, carpenter crafts chair (2-3 tests, ~8 assertions)

### Phase 6: Hunger/Sleep Interaction (~0.5 session)
**Goal**: Hunger drains during sleep, starvation wakes mover.

1. Hunger continues draining during FREETIME_RESTING
2. Mover wakes when starving, seeks food, then re-seeks bed if still tired
3. **Tests**: sleeping mover's hunger drains, wakes when starving, eats, returns to bed (2-3 tests, ~8 assertions)

### Deferred
- Grass mat (FURNITURE_GRASS_MAT, ITEM_GRASS_MAT, rope maker recipe — add when rope maker has more recipes to justify the supply chain)
- Table (FURNITURE_TABLE, ITEM_TABLE, carpenter recipe — add when mood/room system gives it a purpose)
- Night-time drain multiplier (wait for gameplay feel)
- Furniture deconstruction (add when needed)

---

## Test Scenarios (Concrete)

All tests use `InitGridWithSizeAndChunkSize(16, 16, 16, 16)` unless noted. Test setup must call `ClearFurniture()` alongside `ClearMovers()`, `ClearItems()`, `ClearStockpiles()`, `ClearWorkshops()`.

### Energy Drain
```
Setup: 1 mover at (5,5,1), energy=1.0, no job
Tick 60 frames (1 real second)
Expect: energy ≈ 0.99 (1.0 - 0.010)

Setup: 1 mover at (5,5,1), energy=1.0, currentJobId=0 (has job)
Tick 60 frames
Expect: energy ≈ 0.982 (1.0 - 0.018)
```

### Threshold Triggers
```
Setup: mover, energy=0.09, no job, no furniture
Expect: freetimeState == FREETIME_SEEKING_REST (exhausted, immediate)

Setup: mover, energy=0.25, has job
Expect: job gets cancelled (exhausted < 0.1? No, 0.25 > 0.1)
Actually: 0.25 is TIRED not EXHAUSTED. Mover keeps working.

Setup: mover, energy=0.05, has job
Expect: job cancelled, enters SEEKING_REST
```

### Ground Rest Recovery
```
Setup: mover, energy=0.1, freetimeState=FREETIME_RESTING, needTarget=-1
Tick 60 frames (1 second)
Expect: energy ≈ 0.112 (0.1 + 0.012)

Tick until energy >= 0.8
Expect: freetimeState == FREETIME_NONE (woke up)
```

### Furniture Rest
```
Setup: spawn furniture PLANK_BED at (8,8,1), mover at (5,5,1), energy=0.2
ProcessFreetimeNeeds()
Expect: freetimeState == FREETIME_SEEKING_REST, needTarget == bed index
Expect: furniture[bed].occupant == mover index

// Simulate arrival
Move mover to (8,8,1), ProcessFreetimeNeeds()
Expect: freetimeState == FREETIME_RESTING

Tick 60 frames
Expect: energy ≈ 0.24 (0.2 + 0.040) — bed rate, not ground rate
```

### Priority: Hunger > Energy
```
Setup: mover, energy=0.05, hunger=0.05, food item nearby, bed nearby
ProcessFreetimeNeeds()
Expect: freetimeState == FREETIME_SEEKING_FOOD (not SEEKING_REST)
```

### Starvation Interrupts Sleep
```
Setup: mover, freetimeState=FREETIME_RESTING, hunger=0.08, food nearby
Set hunger to 0.05 (below starving threshold after drain during sleep)
ProcessFreetimeNeeds()
Expect: freetimeState != FREETIME_RESTING (woke up)
Expect: furniture occupant released
Next tick: expect FREETIME_SEEKING_FOOD
```

---

## Total Estimates

- ~25-30 tests, ~80-100 assertions
- ~500-650 lines new code (furniture.c ~200, needs.c extensions ~150, construction/workshop ~80, input/action ~60, save/render/tooltip ~80)
- 1 new entity pool, 1 new workshop, 2 new items, 3 construction recipes, 3 sprites
- Save version 52 → 53 (no migration needed — no old saves)
