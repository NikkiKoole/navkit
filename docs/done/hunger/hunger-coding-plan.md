# Implementation Plan: Hunger & Foraging System

## Context

Movers are immortal robots. Hunger is the first survival pressure — it gives purpose to gathering, crafting, stockpiling, and seasons. The player controls food production (designate bushes → harvest → haul to stockpile). Movers only autonomously eat from available food. If the player didn't prepare, movers suffer.

This also introduces the **plant entity system** (for future crops) and the **freetime state machine** (for future sleep/hygiene/etc).

---

## Step 1: Hunger Stat on Mover

**Goal:** Movers get hungry over time. Speed penalty when starving. Freetime movers excluded from job assignment.

### Changes

**`src/entities/mover.h`** — Add FreetimeState enum (before Mover struct) and fields to Mover struct:
```c
typedef enum {
    FREETIME_NONE,
    FREETIME_SEEKING_FOOD,
    FREETIME_EATING,
} FreetimeState;

// Add to Mover struct after workAnimPhase:
float hunger;               // 1.0=full, 0.0=starving
int freetimeState;          // FreetimeState (int for save compat)
int needTarget;             // Item index (-1=none)
float needProgress;         // Eating timer
float needSearchCooldown;   // Cooldown between food searches
```

**`src/entities/mover.c`** — Init in `InitMover()`: hunger=1.0, freetimeState=0, needTarget=-1, needProgress=0, needSearchCooldown=0

**`src/entities/mover.c`** — New `NeedsTick(void)` function: drain hunger for all active movers (`hunger -= HUNGER_DRAIN_RATE`, clamp 0). Tick down needSearchCooldown. Declare in mover.h.

**`src/entities/mover.c`** — Speed penalty in `UpdateMovers()`: after weight slowdown, before `effectiveSpeed = m->speed * terrainSpeedMult`, multiply terrainSpeedMult by hunger penalty (linear 1.0→0.5 as hunger goes from 0.2→0.0).

**`src/entities/jobs.c`** — In `RebuildIdleMoverList()` (line ~2703), add `&& movers[i].freetimeState == FREETIME_NONE` to the idle check.

**`src/main.c`** — Call `NeedsTick()` before `AssignJobs()` in the main sim loop (after DesignationsTick, before AssignJobs).

**`src/core/save_migrations.h`** — Bump CURRENT_SAVE_VERSION to 48.

**`src/core/saveload.c`** — After bulk `fread(movers, ...)`, add `if (version < 48)` block to init hunger=1.0, freetimeState=0, needTarget=-1, needProgress=0, needSearchCooldown=0 for all active movers.

**`src/core/inspect.c`** — Mirror the same migration.

### Tests (`tests/test_hunger.c`)

```
describe(hunger_drain)
  it("mover hunger starts at 1.0")
  it("hunger drains over time to 0")
  it("hunger clamps at 0.0, doesn't go negative")

describe(hunger_speed_penalty)
  it("starving mover (hunger=0) moves ~50% speed of full mover")
  it("mover at hunger=0.2 moves at full speed (no penalty above threshold)")

describe(freetime_idle_list)
  it("mover with freetimeState=FREETIME_EATING not in idle list")
  it("mover with freetimeState=FREETIME_NONE IS in idle list")
```

---

## Step 2: New Item Types + Nutrition

**Goal:** ITEM_BERRIES and ITEM_DRIED_BERRIES exist. ItemDef gains nutrition field.

### Changes

**`src/entities/item_defs.h`** — Add `float nutrition;` to end of ItemDef struct. Add `#define ItemNutrition(t) (itemDefs[t].nutrition)`.

**`src/entities/item_defs.c`** — Add `, 0.0f` (nutrition) to all 26 existing item def initializers. Add new entries:
```c
[ITEM_BERRIES]       = { "Berries",       SPRITE_bush,         IF_STACKABLE | IF_EDIBLE, 20, MAT_NONE, 0.3f, 0.3f },
[ITEM_DRIED_BERRIES] = { "Dried Berries", SPRITE_grass_trampled, IF_STACKABLE | IF_EDIBLE, 20, MAT_NONE, 0.2f, 0.25f },
```
(Placeholder sprites — reuse existing, tint later.)

**`src/entities/items.h`** — Add `ITEM_BERRIES, ITEM_DRIED_BERRIES,` before ITEM_TYPE_COUNT. Count goes 26→28.

**`src/entities/stockpiles.c`** — Add filter entries. Available key: 'x' for berries. Use '1' or another for dried berries (or check what's free — most alpha keys taken).

**`src/core/save_migrations.h`** — Add `#define V47_ITEM_TYPE_COUNT 26`. Add `StockpileV47` legacy struct with `allowedTypes[V47_ITEM_TYPE_COUNT]` and all other Stockpile fields.

**`src/core/saveload.c`** — Stockpile migration: if `version < 48`, read StockpileV47, copy fields, default new items to false. (This and the mover migration both happen at v48.)

**`src/core/inspect.c`** — Mirror stockpile migration.

### Tests

```
describe(food_items)
  it("ITEM_BERRIES exists and is edible")
  it("ITEM_DRIED_BERRIES exists and is edible")
  it("ItemNutrition(ITEM_BERRIES) == 0.3")
  it("ItemNutrition(ITEM_DRIED_BERRIES) == 0.25")
  it("non-food items have nutrition 0")
```

---

## Step 3: Plant Entity System

**Goal:** Berry bushes are plant entities with growth stages. Seasonal modulation.

### New Files

**`src/simulation/plants.h`** + **`src/simulation/plants.c`**

```c
#define MAX_PLANTS 2000

typedef enum { PLANT_BERRY_BUSH, PLANT_TYPE_COUNT } PlantType;
typedef enum { PLANT_STAGE_BARE, PLANT_STAGE_BUDDING, PLANT_STAGE_RIPE } PlantStage;

typedef struct {
    int x, y, z;
    PlantType type;
    PlantStage stage;
    float growthProgress;  // 0→1 within current stage
    bool active;
} Plant;

extern Plant plants[MAX_PLANTS];
extern int plantCount;

void InitPlants(void);
void ClearPlants(void);
int SpawnPlant(int x, int y, int z, PlantType type);
void DeletePlant(int idx);
Plant* GetPlantAt(int x, int y, int z);
bool IsPlantRipe(int x, int y, int z);
void HarvestPlant(int x, int y, int z);  // Reset to bare, spawn ITEM_BERRIES
void PlantsTick(float dt);               // Advance growth, season-modulated
```

**PlantsTick:** Probabilistic advancement per tick. Growth rate = base_rate * seasonal_multiplier. Season multipliers: summer=1.0, spring=0.3, autumn=0.5, winter=0.0. Stage transitions: BARE→BUDDING→RIPE. After harvest, resets to BARE.

**HarvestPlant:** Sets stage=BARE, growthProgress=0, spawns ITEM_BERRIES at cell center.

**GetPlantAt:** Linear scan (plants are sparse, <2000). If perf matters later, add spatial lookup.

### Integration

**`src/unity.c`** — Add `#include "simulation/plants.c"`

**`tests/test_unity.c`** — Add `#include "../src/simulation/plants.c"`

**`src/main.c`** — Call `PlantsTick(tickDt)` in sim loop (after TreesTick). Call `InitPlants()` at startup.

**`src/core/saveload.c`** — Save: `fwrite(&plantCount, ...)` + `fwrite(plants, sizeof(Plant), MAX_PLANTS, f)`. Load: version-gated, init to 0 for old saves.

**`src/core/inspect.c`** — Mirror.

### Tests

```
describe(plant_spawn)
  it("SpawnPlant creates active plant at position")
  it("GetPlantAt finds plant by position")
  it("DeletePlant deactivates plant")

describe(plant_growth)
  it("berry bush goes BARE→BUDDING→RIPE over time in summer")
  it("berry bush does NOT grow in winter (rate=0)")
  it("spring growth is slower than summer")

describe(plant_harvest)
  it("HarvestPlant on ripe bush resets to BARE")
  it("HarvestPlant spawns ITEM_BERRIES on ground")
  it("HarvestPlant on non-ripe bush does nothing")
```

---

## Step 4: Harvest Designation + Job

**Goal:** Player designates ripe bushes for harvest. Movers walk there, pick berries.

### Changes

**`src/world/designations.h`** — Add `DESIGNATION_HARVEST_BERRY` to enum. Add `#define HARVEST_BERRY_WORK_TIME 1.0f`. Add function declarations: `DesignateHarvestBerry`, `CompleteHarvestBerryDesignation`.

**`src/world/designations.c`** — Implement:
- `DesignateHarvestBerry(x,y,z)`: check `IsPlantRipe(x,y,z)`, no existing designation, set type, invalidate cache
- `CompleteHarvestBerryDesignation(x,y,z)`: call `HarvestPlant(x,y,z)`, clear designation, invalidate cache

**`src/entities/jobs.h`** — Add `JOBTYPE_HARVEST_BERRY` to enum.

**`src/entities/jobs.c`** — Full designation→job pipeline:
- Static cache arrays: `harvestBerryCache[]`, `harvestBerryCacheCount`, `harvestBerryCacheDirty`
- `RebuildHarvestBerryDesignationCache()` — follow `RebuildGatherGrassDesignationCache` pattern
- Add entry to `designationSpecs[]` table
- `WorkGiver_HarvestBerry(moverIdx)` — follow `WorkGiver_GatherGrass` pattern (find nearest, pathfind, create job, reserve designation)
- `RunJob_HarvestBerry(job, mover, dt)` — follow `RunJob_GatherGrass` pattern: STEP_MOVING_TO_WORK → STEP_WORKING (1s) → `CompleteHarvestBerryDesignation`
- Add to job driver dispatch in `JobsTick()`

**`src/core/input_mode.h`** — Add `ACTION_WORK_HARVEST_BERRY` to enum.

**`src/core/action_registry.c`** — Add entry under SUBMODE_HARVEST: key='b', barDisplayText="harvest Berry", canDrag=true.

**`src/core/input.c`** — Add `ExecuteDesignateHarvestBerry(x1,y1,x2,y2,z)` and case in action dispatch.

### Tests

```
describe(harvest_designation)
  it("designating ripe bush succeeds")
  it("designating non-ripe bush fails")
  it("designating same cell twice fails")

describe(harvest_job)
  it("mover walks to designated bush, harvests, berries spawn")
    -- Full sim loop: SpawnPlant(ripe) → DesignateHarvestBerry → Tick/Assign/JobsTick loop
    -- Expect: ITEM_BERRIES on ground, plant back to BARE, designation cleared

describe(harvest_and_haul)
  it("berries from harvest get hauled to food stockpile")
    -- Setup: ripe plant, harvest designation, food stockpile accepting berries, mover
    -- Run sim loop until item is ITEM_IN_STOCKPILE
    -- Expect: berries in stockpile
```

---

## Step 5: Eating Behavior (Freetime State Machine)

**Goal:** Hungry movers find food and eat it. Starving movers cancel jobs.

### New Files

**`src/simulation/needs.h`** + **`src/simulation/needs.c`**

Functions:
- `ProcessFreetimeNeeds(void)` — iterate all movers, run freetime state machine
- `StartFoodSearch(Mover* m, int moverIdx)` — find nearest edible item (stockpile first, then ground), reserve it, set goal, enter FREETIME_SEEKING_FOOD. If nothing found, set cooldown.

State machine in `ProcessMoverFreetime(Mover* m, int moverIdx)`:

**FREETIME_NONE:**
- If hunger < 0.1 and has job → `CancelJob()`, then search for food
- If hunger < 0.3 and no job and no cooldown → search for food
- Search: find nearest edible item (scan stockpiles for IF_EDIBLE items, then ground items via spatial grid). Reserve item. Set mover->goal. Enter FREETIME_SEEKING_FOOD.
- If nothing found → set needSearchCooldown=5s

**FREETIME_SEEKING_FOOD:**
- If target item gone (deleted, reserved by someone else) → release, re-search or cooldown
- If mover within PICKUP_RADIUS of item → enter FREETIME_EATING, set needProgress=0
- Timeout after ~10s → give up, release reservation, cooldown

**FREETIME_EATING:**
- needProgress += TICK_DT
- When needProgress >= 2.0: hunger += ItemNutrition(type), DeleteItem, return to FREETIME_NONE
- Reset timeWithoutProgress each tick (prevent stuck detector)

### Integration

**`src/unity.c`** — Add `#include "simulation/needs.c"`

**`tests/test_unity.c`** — Add `#include "../src/simulation/needs.c"`

**`src/main.c`** — Call `ProcessFreetimeNeeds()` after `NeedsTick()`, before `AssignJobs()`.

### Tests

```
describe(eating_from_stockpile)
  it("hungry idle mover finds berries in stockpile, walks there, eats, hunger restored")
    -- Setup: 10x10 grid, mover at (1,1) with hunger=0.2, stockpile at (5,5) with ITEM_BERRIES
    -- Run sim loop (NeedsTick + ProcessFreetimeNeeds + Tick + Assign + JobsTick)
    -- Expect: hunger > 0.4, item deleted

describe(eating_from_ground)
  it("hungry mover eats berries lying on ground")
    -- Setup: mover with hunger=0.2, ITEM_BERRIES on ground nearby
    -- Run sim loop
    -- Expect: hunger restored, item deleted

describe(starving_cancels_job)
  it("starving mover cancels current job to seek food")
    -- Setup: mover with job (e.g. haul), hunger=0.05, berries in stockpile
    -- Run sim loop
    -- Expect: job cancelled, mover eats, hunger restored

describe(no_food_cooldown)
  it("no food available → cooldown, doesn't spam search every tick")
    -- Setup: mover with hunger=0.2, no food anywhere
    -- Call ProcessMoverFreetime once
    -- Expect: freetimeState still FREETIME_NONE, needSearchCooldown > 0
    -- Call again immediately
    -- Expect: no new search attempted (cooldown active)

describe(food_competition)
  it("two hungry movers, one berry — first reserves, second gets cooldown")
    -- Setup: two movers both hunger=0.2, one ITEM_BERRIES on ground
    -- Process both
    -- Expect: one has needTarget=itemIdx, item.reservedBy set. Other has cooldown.

describe(freetime_blocks_jobs)
  it("mover in FREETIME_EATING not assigned new jobs")
    -- Setup: mover with freetimeState=FREETIME_EATING, ground item needing haul
    -- Run AssignJobs
    -- Expect: mover still has no job (currentJobId == -1), item not reserved
```

---

## Step 6: Drying Rack Recipe

**Goal:** 3 berries → 2 dried berries on drying rack.

### Changes

**`src/entities/workshops.c`** — Add to `dryingRackRecipes[]`:
```c
{ "Dry Berries", ITEM_BERRIES, 3, ITEM_NONE, 0, ITEM_DRIED_BERRIES, 2, ITEM_NONE, 0, 0, 10.0f, MAT_MATCH_ANY, MAT_NONE, 0, ITEM_MATCH_EXACT },
```

Update `dryingRackRecipeCount` (or it's auto-calculated via sizeof).

### Tests

```
describe(drying_rack_berries)
  it("drying rack converts 3 berries to 2 dried berries")
    -- Setup: drying rack workshop, bill DO_X_TIMES 1, place 3 ITEM_BERRIES on work tile
    -- Run PassiveWorkshopsTick for 800 ticks (~13s, recipe is 10s)
    -- Expect: berries consumed, ITEM_DRIED_BERRIES on output tile, count=2
```

---

## End-to-End Integration Test

```
describe(full_survival_loop)
  it("bush ripens → player designates → mover harvests → haul to stockpile → mover gets hungry → eats → hunger restored")
    -- Setup: 20x10 grid
    -- Spawn ripe berry bush plant at (10,5)
    -- Food stockpile at (3,3) accepting berries
    -- Mover at (1,1) with hunger=0.4 (will get hungry soon), canPlant=true, canHaul=true
    -- Designate harvest at (10,5)
    -- Run full sim loop (NeedsTick + ProcessFreetimeNeeds + Tick + ItemsTick + AssignJobs + JobsTick)
    -- Phase 1: mover walks to bush, harvests → ITEM_BERRIES on ground
    -- Phase 2: mover hauls berries to stockpile
    -- Phase 3: hunger drains below 0.3 → mover seeks food from stockpile → eats
    -- Expect: hunger > 0.5 (restored), plant is BARE, stockpile had berries (now eaten)

  it("mover without food starves and slows down but doesn't die")
    -- Setup: mover with hunger=1.0, no food anywhere
    -- Run NeedsTick for many ticks until hunger=0
    -- Expect: hunger==0, mover still active (alive), speed reduced
```

---

## File Change Summary

| File | Change |
|------|--------|
| `src/entities/mover.h` | FreetimeState enum, hunger/freetime fields on Mover |
| `src/entities/mover.c` | InitMover fields, NeedsTick(), speed penalty in UpdateMovers |
| `src/entities/item_defs.h` | `float nutrition` on ItemDef, `ItemNutrition` macro |
| `src/entities/item_defs.c` | nutrition values for all items, new berry defs |
| `src/entities/items.h` | ITEM_BERRIES, ITEM_DRIED_BERRIES |
| `src/entities/stockpiles.c` | Filter entries for berries |
| `src/entities/jobs.h` | JOBTYPE_HARVEST_BERRY |
| `src/entities/jobs.c` | Cache, designationSpecs, WorkGiver, RunJob, idle list filter |
| `src/entities/workshops.c` | Drying rack berry recipe |
| `src/simulation/plants.h` (NEW) | Plant entity API |
| `src/simulation/plants.c` (NEW) | Plant pool, growth, harvest, seasonal modulation |
| `src/simulation/needs.h` (NEW) | Freetime processing API |
| `src/simulation/needs.c` (NEW) | Food search, eating state machine |
| `src/world/designations.h` | DESIGNATION_HARVEST_BERRY, function decls |
| `src/world/designations.c` | Designate, Complete functions |
| `src/core/input_mode.h` | ACTION_WORK_HARVEST_BERRY |
| `src/core/action_registry.c` | Harvest berry registry entry |
| `src/core/input.c` | ExecuteDesignateHarvestBerry, action dispatch |
| `src/core/save_migrations.h` | V48, V47_ITEM_TYPE_COUNT, StockpileV47 |
| `src/core/saveload.c` | Mover migration, stockpile migration, plant save/load |
| `src/core/inspect.c` | Mirror migrations |
| `src/core/rendering.c` | Ripe bush sprite variant (later, placeholder ok) |
| `src/core/tooltips.c` | Hunger line in mover tooltip |
| `src/main.c` | NeedsTick, ProcessFreetimeNeeds, PlantsTick calls |
| `src/unity.c` | Include plants.c, needs.c |
| `tests/test_unity.c` | Include plants.c, needs.c |
| `tests/test_hunger.c` (NEW) | All hunger/plant/eating/integration tests |

## Verification

1. `make test` — all existing 23 test suites still pass
2. `make test` — new test_hunger suite passes (~30+ assertions)
3. `make path` — builds cleanly with new files
4. Manual: place bushes in sandbox, watch them ripen, designate harvest, verify berries appear, haul to stockpile, watch mover eat when hungry
