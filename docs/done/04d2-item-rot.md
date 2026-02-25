# 04d2: ItemCondition System (was: Item Rot)

> Extension of 04d-spoilage.md — refactored from ITEM_ROT approach
> **Status**: ✅ Done (save v72→v73)
> **Deps**: 04d (spoilage timer — done)
> **Opens**: Curing/seasoning (material conditioning), composting (09 loop closers)

---

## What Was Built

Instead of spawning a separate `ITEM_ROT` type when food spoils, items now keep their original type and gain an `ItemCondition` field. This is more extensible — curing, aging, and other condition states can reuse the same pattern.

### ItemCondition enum (`items.h`)

```c
typedef enum {
    CONDITION_FRESH = 0,
    CONDITION_STALE,
    CONDITION_ROTTEN
} ItemCondition;
```

- `uint8_t condition` field on Item struct
- `IsItemRotten(idx)` inline helper
- Condition derived from spoilageTimer ratio: Fresh <50%, Stale >=50%, Rotten >=100%

### SpoilageTick behavior

- Sets `condition` based on timer ratio instead of DeleteItem + SpawnItemWithMaterial
- Rotten items are **skipped** (timer stops advancing)
- Rotten items keep their original type (ITEM_BERRIES stays ITEM_BERRIES)
- EventLog fires when condition transitions to ROTTEN

### Rotten = fuel (`workshops.c`)

- `ItemIsFuelOrRotten()` helper — checks `IF_FUEL` flag OR `condition == CONDITION_ROTTEN`
- Used in `WorkshopHasFuelForRecipe()`, `FindNearestFuelItem()`, `RecipeInputMatches()`
- No new item type needed — rotten food is directly burnable

### Eating refuses rotten (`needs.c`)

- `FindNearestEdibleInStockpile()` and `FindNearestEdibleOnGround()` skip `condition == CONDITION_ROTTEN`

### Stockpile rejectsRotten (`stockpiles.h`)

- `bool rejectsRotten` on Stockpile struct, defaults `true` in `CreateStockpile()`
- Rehaul jobs (`AssignJobs_P3c_Rehaul`) check rotten condition + rejectsRotten for eviction
- Container cleanup (`AssignJobs_P3e_ContainerCleanup`) same check
- Rotten items routed to stockpiles with `rejectsRotten = false`

### Refuse pile (`input.c`)

- ACTION_DRAW_REFUSE_PILE in Draw menu (key 'r')
- Creates stockpile with `rejectsRotten = false` + enables all spoilable food types via `ItemSpoils(t)`
- Existing JOBTYPE_HAUL automatically picks up and delivers rotten items

### Stack handling (`stacking.c`)

- Merge: takes worse (higher) condition AND worse timer
- Split: copies both condition and timer to new item

### Tooltip (`tooltips.c`)

- Shows "Fresh"/"Stale"/"Rotten" label based on condition field
- Color tint: green for fresh, yellow for stale, red for rotten

### Removed

- `ITEM_ROT` enum value (ITEM_TYPE_COUNT: 46→45)
- `MAT_ROTTEN_MEAT`, `MAT_ROTTEN_PLANT` (MAT_COUNT: 19→17)
- `GetRotMaterial()` function

### Save migration v72→v73 (`save_migrations.h`, `saveload.c`, `inspect.c`)

- `ItemV72` struct: reads old items, derives condition from spoilageTimer, deletes old ITEM_ROT items
- `StockpileV72` struct: shrinks allowedTypes (46→45), remaps allowedMaterials (19→17, drops rotten mats, shifts MAT_BEDROCK), remaps slot materials
- All older stockpile migrations set `rejectsRotten = true`

---

## Tests (`test_spoilage.c`)

34 tests, 94 assertions:

- `spoilage_item_defs` — IF_SPOILS flags, spoilage limits
- `spoilage_condition` — FRESH/STALE/ROTTEN thresholds, rotten retains type, rotten stops timer
- `spoilage_timer` — timer advances, rotten at limit, carried skip, reserved spoil, stack rot
- `spoilage_containers` — clay pot halves rate, basket no benefit, nested uses outermost, carried skip
- `spoilage_stacking` — merge takes worse timer+condition, split copies both
- `spoilage_rotten_fuel` — rotten items fuel-eligible
- `spoilage_stockpile` — new stockpiles default rejectsRotten=true
- `spoilage_e2e` — drying preserves, cooking extends, container chain

---

## Key Files

- `src/entities/items.h` / `items.c` — ItemCondition enum, condition field, SpoilageTick
- `src/entities/item_defs.h` / `item_defs.c` — spoilageLimit field, IF_SPOILS flags
- `src/entities/stacking.c` — condition-aware merge/split
- `src/entities/stockpiles.h` / `stockpiles.c` — rejectsRotten field
- `src/entities/jobs.c` — rotten eviction in rehaul + container cleanup
- `src/entities/workshops.c` — ItemIsFuelOrRotten helper
- `src/simulation/needs.c` — eating skips rotten
- `src/core/input.c` — refuse pile creation
- `src/render/tooltips.c` — condition label + tint
- `src/core/save_migrations.h` / `saveload.c` / `inspect.c` — v73 migration
