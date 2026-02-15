# Hunger & Foraging — Implementation Plan

> Goal: the first survival loop. Movers get hungry, player designates berry harvest, berries get stockpiled, movers eat from stockpile. Preserve food for winter.

---

## Design Decisions (Resolved)

These were discussed and settled before implementation:

| Decision | Resolution | Rationale |
|----------|-----------|-----------|
| **Player autonomy vs mover autonomy** | Player controls food production. Movers only autonomously *eat* from available food | Player designates bushes for harvest, haul system stocks food. Movers don't forage on their own. Autonomous foraging can be added later |
| **Eating fallback chain** | Stockpile → ground item → nothing | No emergency bush eating. If player didn't harvest/stockpile, movers suffer. That's the survival pressure |
| **Plant storage architecture** | Plant entity pool (not grid) | Future crops (potato, wheat, etc.) need per-plant type/state. A grid per plant type doesn't scale. Entity pool like movers/items/workshops |
| **Berry grid vs plant entities** | Plant entities for berry bushes too | Berry bushes are just the first PlantType. Same system handles future farming |
| **Tick order** | `NeedsTick()` before `AssignJobs()` in main loop | Needs sit above the job system as interrupts |
| **Pathfinding for hungry movers** | Direct `mover->goal` setting | Movers already pathfind without jobs. Freetime sets goal directly |
| **Idle list filtering** | `RebuildIdleMoverList()` skips movers with `freetimeState != FREETIME_NONE` | One-line addition to existing filter |
| **Nutrition storage** | `float nutrition` field on ItemDef | Zero for non-edible items, no cost. Cleaner than a switch |
| **Starvation** | Non-lethal. Speed penalty at hunger 0 | Death can be added later as a toggle |
| **Spoilage (IF_SPOILS)** | Defer to Phase 4. Separate save bump is fine | Save bumps are cheap (47 so far). Don't over-engineer Phase 1 |
| **Vegetation grid** | Stays as-is for grass. Plant entities sit next to it, not replacing it | Grass is dense ground cover. Plants are sparse discrete things. They coexist on the same tile |
| **Tree grids** | Stay as-is. Not migrating to plant entities | Trees work fine. Refactoring isn't worth it |

---

## Current State of the Code

### What exists and is ready to use
| Thing | Where | Status |
|-------|-------|--------|
| `CELL_BUSH` | `src/world/grid.h` | Walkable vegetation, slows movement, has sprite |
| `IF_EDIBLE` flag | `src/entities/item_defs.h` | Defined, unused |
| `IF_SPOILS` flag | `src/entities/item_defs.h` | Defined, unused |
| `ItemIsEdible()` macro | `src/entities/item_defs.h` | Defined, unused |
| Drying rack workshop | `src/entities/workshops.c` | Passive, currently only dries grass |
| Seasons + temperature | `src/simulation/weather.c` | Full seasonal cycle, temperature curves |
| Stockpile filter system | `src/entities/stockpiles.c` | Per-type filters with keys |
| Job assignment pipeline | `src/entities/jobs.c` | WorkGiver pattern, priority ordering |
| `CancelJob()` | `src/entities/jobs.c` | Releases all reservations cleanly |
| `DeleteItem()` | `src/entities/items.c` | Proper cleanup (count, high water mark, stockpile slots) |
| Designation system | `src/world/designations.c` | DESIGNATION_GATHER_GRASS as pattern to follow |
| Bush in terrain gen | `src/world/grid.c` | Bushes spawn on grass terrain |
| Item spatial grid | `src/entities/items.c` | `FindFirstItemInRadius()` for fast lookups |
| Save version 47 | `src/core/save_migrations.h` | Will need 48 |

### What does NOT exist yet
- No fields on Mover for hunger or freetime state
- No plant entity system
- No food items (ITEM_BERRIES, ITEM_DRIED_BERRIES)
- No eating behavior or freetime state machine
- No `MoverAvailableForWork()` check
- No `NeedsTick()` in main loop

---

## Implementation Phases

### Phase 1: Hunger Stat (Mover Struct + Drain)

Movers get hungry but can't eat yet — hunger drains to 0 and we observe.

**Mover struct additions (`src/entities/mover.h`):**
```c
// Needs
float hunger;               // 1.0 = full, 0.0 = starving

// Freetime state machine (personal needs, bypasses job system)
int freetimeState;          // FreetimeState enum (0 = NONE)
int needTarget;             // Item index for food source (-1 = none)
float needProgress;         // Timer for eating action
float needSearchCooldown;   // Seconds until next food search (prevents spam)
```

```c
typedef enum {
    FREETIME_NONE,
    FREETIME_SEEKING_FOOD,
    FREETIME_EATING,
} FreetimeState;
```

**Init (`mover.c`):** hunger=1.0, freetimeState=FREETIME_NONE, needTarget=-1, needProgress=0, needSearchCooldown=0

**New `NeedsTick(float dt)` called from main.c before `AssignJobs()`:**
- Drains hunger for all active movers: `hunger -= HUNGER_DRAIN_RATE * dt`, clamp at 0
- HUNGER_DRAIN_RATE tuned for ~8 real-time minutes full→starving (~0.002/sec)
- Starving movers (hunger < 0.1): speed penalty (50%)

**`RebuildIdleMoverList()` change (`jobs.c`):**
- Add `&& movers[i].freetimeState == FREETIME_NONE` to idle check

**ItemDef addition (`item_defs.h`):**
```c
float nutrition;  // Hunger restored when eaten (0 = not edible)
```
All existing items get `nutrition = 0`. Added to end of ItemDef struct.

**Save version bump to 48:**
- `save_migrations.h`: define V47_ITEM_TYPE_COUNT = 26
- `saveload.c` + `inspect.c`: save/load new mover fields, legacy migration (hunger=1.0, freetimeState=0, needTarget=-1)

**Tests (`tests/test_hunger.c`):**
- Mover hunger starts at 1.0
- Hunger drains over time
- Hunger clamps at 0.0
- Starving mover gets speed penalty

---

### Phase 2: Plant Entity System + Berry Bushes

New entity system for plants. Berry bushes are the first plant type.

**New files `src/simulation/plants.h` / `plants.c`:**

```c
typedef enum {
    PLANT_NONE = 0,
    PLANT_BERRY_BUSH,
    // Future: PLANT_POTATO, PLANT_WHEAT, etc.
    PLANT_TYPE_COUNT
} PlantType;

typedef enum {
    PLANT_STAGE_BARE,      // No produce
    PLANT_STAGE_BUDDING,   // Growing
    PLANT_STAGE_RIPE,      // Ready to harvest
} PlantStage;

typedef struct {
    int x, y, z;
    PlantType type;
    PlantStage stage;
    float growthProgress;  // 0.0 → 1.0, advances by season-modulated rate
    bool active;
} Plant;

#define MAX_PLANTS 10000

void InitPlants(void);
void PlantsTick(float dt);              // Advance growth, season-modulated
int SpawnPlant(int x, int y, int z, PlantType type);
void HarvestPlant(int plantIdx);        // Set to bare, reset growth
Plant* GetPlantAt(int x, int y, int z); // Lookup by position
bool IsPlantRipe(int plantIdx);
```

**Ripening rules:**
- Growth rate modulated by season: summer=1.0, spring=0.3, autumn=0.5, winter=0.0
- Probabilistic per-tick advancement (like snow accumulation pattern)
- When ripe: bush sprite changes (overlay or recolor)
- After harvest: returns to bare, growth restarts

**New items (`items.h`):**
```c
ITEM_BERRIES,       // Before ITEM_TYPE_COUNT
ITEM_DRIED_BERRIES,
```

**Item defs (`item_defs.c`):**
```c
[ITEM_BERRIES]       = { "Berries",       SPRITE_berries,       IF_STACKABLE | IF_EDIBLE, 10, MAT_NONE, 0.3f, 0.3f },
[ITEM_DRIED_BERRIES] = { "Dried Berries", SPRITE_dried_berries, IF_STACKABLE | IF_EDIBLE, 20, MAT_NONE, 0.2f, 0.25f },
```
(Last field is nutrition. IF_SPOILS deferred to Phase 4.)

**Harvest designation (`designations.h`, `jobs.h`):**
- `DESIGNATION_HARVEST_BERRY` — player designates ripe bushes (like DESIGNATION_GATHER_GRASS)
- `JOBTYPE_HARVEST_BERRY` — 2-step: walk to bush → harvest (1s)
- On completion: `HarvestPlant(idx)` + `SpawnItem(x, y, z, ITEM_BERRIES)`
- WorkGiver follows existing gather pattern

**Action registry (`action_registry.c`, `input_mode.h`):**
- `ACTION_HARVEST_BERRY` — player UI for designating harvest

**Stockpile filters (`stockpiles.c`):**
```c
{ITEM_BERRIES,       'k', "Berries",       "K", RED},
{ITEM_DRIED_BERRIES, 'l', "Dried Berries", "L", ORANGE},
```

**Terrain gen:** Spawn Plant entities at existing CELL_BUSH locations.

**Save version 48:** Plant entity pool needs saving (like items, workshops).

**Tests:**
- Plant spawns at bush location
- Berry bush ripens over time (summer)
- Berry bush doesn't ripen in winter
- Harvest sets plant to bare, spawns ITEM_BERRIES
- Plant regrows after harvest
- Non-bush cells can't have berry plants (or: plants can exist anywhere, CELL_BUSH is just the initial placement)

---

### Phase 3: Eating Behavior (Freetime State Machine)

Hungry movers find food and eat it. Player-supplied food only — no autonomous foraging.

**New file `src/simulation/needs.c` / `needs.h`:**

**NeedsTick update (extend from Phase 1):** After draining hunger, check thresholds:
```c
if (mover->freetimeState != FREETIME_NONE) {
    UpdateFreetime(mover, dt);  // Continue current need
} else if (mover->hunger < 0.1f && mover->currentJobId >= 0) {
    CancelJob(mover->currentJobId);  // STARVING — drop everything
    StartFoodSearch(mover);
} else if (mover->hunger < 0.3f && mover->currentJobId < 0) {
    StartFoodSearch(mover);  // HUNGRY and idle
}
```

**Food search (player-supplied food only):**
1. Find nearest stockpile with IF_EDIBLE item → reserve item, set goal, enter FREETIME_SEEKING_FOOD
2. Find nearest ground IF_EDIBLE item → reserve item, set goal, enter FREETIME_SEEKING_FOOD
3. Nothing found → set needSearchCooldown (5s), stay FREETIME_NONE, retry later

**FREETIME_SEEKING_FOOD:**
- Mover walks toward reserved food item
- On arrival → enter FREETIME_EATING
- If item disappears (deleted, someone else ate it) → release reservation, re-search
- Timeout → give up, release reservation, cooldown, return to FREETIME_NONE

**FREETIME_EATING:**
- Timer: 2 seconds (needProgress counts down)
- On complete: `hunger += ItemNutrition(item->type)`, `DeleteItem(needTarget)`, return to FREETIME_NONE

**Item reservation:** Use existing `ReserveItem()` / `ReleaseItemReservation()`. First-come-first-served — if reservation fails, re-search.

**Tests:**
- Hungry idle mover (hunger < 0.3) enters FREETIME_SEEKING_FOOD
- Starving mover (hunger < 0.1) cancels job, seeks food
- Mover finds berries in stockpile, walks to it, eats, hunger restores
- Mover finds berries on ground, walks to it, eats
- No food available → mover returns to FREETIME_NONE after cooldown
- `RebuildIdleMoverList` skips movers in freetime
- Full mover (hunger > 0.3) doesn't seek food
- Two movers competing for last food item: loser re-searches

---

### Phase 4: Drying & Preservation

Raw berries spoil, dried berries don't. Drying rack closes the loop.

**Drying rack recipe (`workshops.c`):**
```c
{ "Dry Berries", ITEM_BERRIES, 3, ITEM_NONE, 0, ITEM_DRIED_BERRIES, 2, ITEM_NONE, 0, 0, 10.0f, MAT_MATCH_ANY, MAT_NONE, 0, ITEM_MATCH_EXACT },
```

**Spoilage system (IF_SPOILS):**
- Add `float age` to Item struct
- In `ItemsTick()`: for items with IF_SPOILS, increment age. When age exceeds spoilage time → `DeleteItem()`
- Add IF_SPOILS to ITEM_BERRIES definition
- Spoilage time: ~120 game-seconds (tune later)
- Save version bump (49) for Item.age field

**Tests:**
- Berries spoil after spoilage time
- Dried berries don't spoil
- Drying rack converts 3 berries → 2 dried berries
- Spoiled item cleanup is correct (DeleteItem)

---

### Phase 5: Seasonal Polish

Fine-tune the seasonal pressure loop.

**Berry growth rates by season** (already wired in Phase 2, this is tuning):
- Spring: slow budding, no ripe berries until late spring
- Summer: fast ripening, peak harvest
- Autumn: slowing, last chance to stockpile
- Winter: no growth, existing ripe berries stay but no new ones

**Tests:**
- Full year cycle: bare → budding → ripe → harvested → regrow → winter bare → spring restart
- Colony survives winter on dried berry stockpile
- Colony without stockpile starves (speed penalty, no death)

---

## File Change Summary

| File | Changes |
|------|---------|
| `src/entities/mover.h` | hunger, freetimeState, needTarget, needProgress, needSearchCooldown, FreetimeState enum |
| `src/entities/mover.c` | Init new fields |
| `src/entities/items.h` | ITEM_BERRIES, ITEM_DRIED_BERRIES |
| `src/entities/item_defs.h` | Add `float nutrition` to ItemDef |
| `src/entities/item_defs.c` | Defs for berries/dried berries, nutrition values for all items |
| `src/entities/jobs.h` | JOBTYPE_HARVEST_BERRY |
| `src/entities/jobs.c` | WorkGiver_HarvestBerry, RunJob_HarvestBerry |
| `src/entities/workshops.c` | Drying rack berry recipe (Phase 4) |
| `src/entities/stockpiles.c` | Filter entries for berries/dried berries |
| `src/simulation/plants.h` (NEW) | Plant entity system API |
| `src/simulation/plants.c` (NEW) | Plant pool, growth tick, harvest, seasonal modulation |
| `src/simulation/needs.h` (NEW) | FreetimeState, NeedsTick, food search API |
| `src/simulation/needs.c` (NEW) | Hunger drain, freetime state machine, food search, eating |
| `src/world/designations.h` | DESIGNATION_HARVEST_BERRY |
| `src/world/designations.c` | Harvest designation logic |
| `src/core/input.c` | Harvest designation action handler |
| `src/core/input_mode.h` | ACTION_HARVEST_BERRY |
| `src/core/action_registry.c` | Registry entry for harvest action |
| `src/core/rendering.c` | Berry bush ripe/bare sprite variant, hunger indicator |
| `src/core/tooltips.c` | Mover hunger in tooltip |
| `src/core/saveload.c` | Mover hunger fields, plant entities, Item.age (Phase 4) |
| `src/core/inspect.c` | Parallel migration |
| `src/core/save_migrations.h` | V48, V47_ITEM_TYPE_COUNT=26 |
| `src/core/main.c` | Call NeedsTick() before AssignJobs(), PlantsTick() in sim loop |
| `src/unity.c` | Include plants.c, needs.c |
| `tests/test_unity.c` | Include test_hunger.c |
| `tests/test_hunger.c` (NEW) | All hunger/plant/eating tests |

---

## The Player Experience (Full Loop)

1. **Spring:** Bushes start budding. Movers aren't hungry yet (started full)
2. **Early summer:** Bushes ripen. Movers start getting hungry. Player notices hunger indicator
3. **Player action:** Designates bushes for harvest. Movers gather berries, haul to food stockpile
4. **Smart player:** Builds drying rack, sets bill "Dry Berries, DO_FOREVER". Dried berries accumulate
5. **Autumn:** Berry growth slows. Raw berries spoil. Dried berries are fine
6. **Winter:** No new berries. Colony lives off dried berry stockpile. Tension: is there enough?
7. **Late winter:** If stockpile runs out, movers slow down (starvation penalty). Player's problem
8. **Spring:** Bushes restart. Colony survived — or learned a hard lesson

Key: the player is in control. They decide when to harvest, how much to dry, whether to build enough drying racks. Movers don't solve hunger on their own — they just eat what's available and suffer if there's nothing.

---

## What This Opens Next

- **Feature 02 (Sleep):** Same freetime pattern, add FREETIME_SEEKING_REST / FREETIME_RESTING
- **Feature 05 (Cooking):** Hearth workshop gets food recipes. Cooked food > raw nutrition
- **Feature 06 (Farming):** Plant entity system already exists — add PLANT_POTATO etc. with farm plot designation
- **Waste loop:** Eating fills bowel → latrine → compost → fertilizer → farming
