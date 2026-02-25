# 06c: Farm Watering (DONE)

> Extracted from 06a on 2026-02-24. Optional enhancement — farms work with rain alone (06a/06b).
> **Deps**: 06a (farm grid, tilled cells exist), wetness system (mud), 08 (thirst — ITEM_WATER already exists)

---

## What Was Built

A single new job type `JOBTYPE_WATER_CROP` that lets movers manually water dry farm cells using existing ITEM_WATER items from the thirst system (08).

### No new items or designations needed
- **ITEM_WATER** already exists (from 08-thirst)
- **Water collection** already exists via JOBTYPE_FILL_WATER_POT
- Job is **auto-assigned** when dry tilled farm cells + loose water exist

### JOBTYPE_WATER_CROP

| Property | Value |
|----------|-------|
| Work time | 0.3 game-hours |
| Requires | ITEM_WATER (on ground or in stockpile) |
| Auto-assigned | Yes, when tilled farm cell has wetness == 0 and ITEM_WATER available |
| Result | Cell wetness set to 2 (wet = ideal 1.0x growth), water consumed |
| Priority | Lowest farm job (after harvest > plant > tend > fertilize) |

**Job steps** (mirrors JOBTYPE_FERTILIZE pattern):
1. Walk to water item, pick up
2. Carry water to dry farm cell
3. Work (pour water, 0.3 GH)
4. On completion: SET_CELL_WETNESS at z-1 = 2, delete water item

### Supply chain
Pots filled at water → water stored in stockpiles → watering job picks up loose water → pours on farm

---

## Files Changed

| File | Change |
|------|--------|
| `src/entities/jobs.h` | JOBTYPE_WATER_CROP enum, RunJob/WorkGiver declarations |
| `src/entities/jobs.c` | RunJob_WaterCrop, WorkGiver_WaterCrop, jobDrivers, AssignJobs |
| `src/simulation/farming.h` | WATER_CROP_WORK_TIME, WATER_POUR_WETNESS constants |
| `tests/test_farming.c` | 9 watering tests (tests 23-31) |

No save migration needed — just a new enum value at the end.

---

## Tests (9 tests, in test_farming.c)

1. Watering sets cell wetness and consumes water item
2. WorkGiver assigns job for dry farm cell
3. No job when cells already wet
4. No job when no water items
5. No job for untilled cells
6. Cancel releases water reservation
7. No duplicate jobs for same cell
8. Multiple dry cells get separate jobs
9. Water from stockpile is also found
