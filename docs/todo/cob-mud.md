# Mud & Cob System — Implementation Plan

## Context

Dirt and clay have limited sinks. Mud is a natural intermediate material that enables primitive construction near water without heavy tools. Cob (mud + dried grass) is a stronger building material. This adds a new workshop (mud mixer), 2 items, 2 materials, and 3 construction recipes.

## Changes Summary

| What | Details |
|------|---------|
| 2 new items | ITEM_MUD, ITEM_COB |
| 2 new materials | MAT_MUD, MAT_COB |
| 1 new workshop | WORKSHOP_MUD_MIXER (2x1, near water) |
| 3 construction recipes | Mud wall, cob wall, mud floor |
| 1 workshop construction | Mud mixer build cost (4 sticks) |
| Save version | 79 → 80 |

## Steps

### 1. Materials — `material.h`, `material.c`

Add `MAT_MUD` and `MAT_COB` before `MAT_COUNT` in enum. Add to `materialDefs[]`:
- Both non-flammable, drops respective item on deconstruct
- MAT_MUD: `INSULATION_TIER_AIR` (weak), MAT_COB: `INSULATION_TIER_STONE` (good insulator)

### 2. Items — `items.h`, `item_defs.c`

Add `ITEM_MUD` and `ITEM_COB` before `ITEM_TYPE_COUNT`. Both: `IF_STACKABLE | IF_BUILDING_MAT`, maxStack 20, default material MAT_MUD/MAT_COB.

### 3. Save migration — `save_migrations.h`, `saveload.c`, `inspect.c`

- Bump SAVE_VERSION to 80
- Define `V79_ITEM_TYPE_COUNT = 65`, `V79_MAT_COUNT = 17`
- **Critical**: Fix ALL legacy StockpileVxx structs that use bare `MAT_COUNT` — replace with `V79_MAT_COUNT` (V31, V32, V34, V47, V50, V54, V60, V65, V67, V69, V75, V76, V77, V78). Without this, changing MAT_COUNT from 17→19 corrupts all old save migrations.
- Add `StockpileV79` struct with old sizes
- Add migration branch in both `saveload.c` and `inspect.c`

### 4. Workshop — `workshops.h`, `workshops.c`

Add `WORKSHOP_MUD_MIXER` to enum. Define recipes:
- Mix Mud: DIRT×2 + CLAY×1 → MUD×3 (3s)
- Make Cob: MUD×2 + DRIED_GRASS×1 → COB×2 (4s)

Template: `"XO"` (2×1, work tile + output tile). Not passive.

### 5. Construction recipes — `construction.h`, `construction.c`

Add 4 new entries to enum and recipe table:
- `CONSTRUCTION_MUD_WALL`: 4 mud, BUILD_WALL, 3s, fixed MAT_MUD
- `CONSTRUCTION_COB_WALL`: 3 cob, BUILD_WALL, 4s, fixed MAT_COB
- `CONSTRUCTION_MUD_FLOOR`: 2 mud, BUILD_FLOOR, 2s, fixed MAT_MUD
- `CONSTRUCTION_WORKSHOP_MUD_MIXER`: 4 sticks, BUILD_WORKSHOP, 2s

Add `WORKSHOP_MUD_MIXER` case to `GetConstructionRecipeForWorkshopType()`.

### 6. Stockpile filters — `stockpiles.c`

Add ITEM_MUD and ITEM_COB to `STOCKPILE_FILTERS[]` under `FILTER_CAT_EARTH`.

### 7. Actions & input — `input_mode.h`, `action_registry.c`, `input.c`

- Add `ACTION_DRAW_WORKSHOP_MUD_MIXER` and `ACTION_WORK_WORKSHOP_MUD_MIXER` enums
- Add registry entries (key 'm', no conflicts)
- Add dispatch cases in input.c for both draw and work modes

### 8. Water proximity validation — `input.c`

When placing mud mixer, check that at least one cardinal neighbor of the workshop footprint has water (`HasWater()`). Show error message if not. Extract as `HasWaterNearWorkshop(x, y, z, w, h)` helper for testability.

### 9. Tests — new `tests/test_mud_cob.c`

- Item/material definitions correct (flags, insulation, drops)
- Workshop def correct (size, recipe count, not passive)
- Recipes have correct inputs/outputs
- Construction recipes exist with correct categories and materials
- Workshop-to-construction mapping works
- Stockpile filter coverage
- Water proximity helper (pass/fail cases)

Add to test_unity.c includes and Makefile test target.

## Verification

1. `make path` — builds clean
2. `make test` — all existing + new tests pass
3. Manual test: start game, place mud mixer near water, craft mud and cob, build walls and floor
