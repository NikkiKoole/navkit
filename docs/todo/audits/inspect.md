# Audit: src/core/inspect.c

**Date**: 2026-02-07
**Auditor**: Claude (assumption audit)
**Scope**: Save file inspector -- version migration parity with saveload.c, display correctness, implicit contracts

---

## Findings

### Finding 1: V18 and V19 stockpile structs use current MAT_COUNT instead of hardcoded V21_MAT_COUNT

- **What**: The V18 and V19 legacy stockpile structs in inspect.c (and identically in saveload.c) define `bool allowedMaterials[MAT_COUNT]` using the current MAT_COUNT (14). At the time V18 and V19 saves were written, MAT_COUNT was 10 (the value now captured as `V21_MAT_COUNT` / `INSPECT_V21_MAT_COUNT`).
- **Assumption**: MAT_COUNT has not changed between V18/V19 and V22.
- **How it breaks**: MAT_COUNT was expanded from 10 to 14 in V22 (adding MAT_CLAY, MAT_GRAVEL, MAT_SAND, MAT_PEAT). If someone tries to load a V18 or V19 save file, the `allowedMaterials` array in the legacy struct is 14 bools instead of 10. This makes the struct 4 bytes too large, causing every field after `allowedMaterials` to be read from the wrong byte offset. Every stockpile's slots, reservations, counts, types, materials, and free slot data will be garbled.
- **Player impact**: Loading a V18/V19 save file after the V22 MAT_COUNT expansion would corrupt all stockpile data. Items in stockpiles would appear in wrong slots, reservations would be wrong, and the game would likely crash or exhibit severe visual glitches. Since this affects both inspect.c AND saveload.c, it also means the game itself cannot load these saves correctly.
- **Severity**: HIGH
- **Suggested fix**: In both inspect.c and saveload.c, change the V18 and V19 legacy stockpile structs to use `INSPECT_V21_MAT_COUNT` (or `V21_MAT_COUNT` in saveload.c) for `allowedMaterials`, since V18, V19, V20, and V21 all had the same MAT_COUNT of 10. The migration code should then copy the 10 old materials and default the new 4 to true, just like the V20/V21 path already does.
- **Files**: `src/core/inspect.c` lines ~1279, ~1329; `src/core/saveload.c` lines ~686, ~735

---

### Finding 2: V16 and V17 stockpile structs use current ITEM_TYPE_COUNT instead of hardcoded constant

- **What**: The V16 and V17 legacy stockpile structs define `bool allowedTypes[ITEM_TYPE_COUNT]` using the current ITEM_TYPE_COUNT. This is documented as a known latent bug in MEMORY.md.
- **Assumption**: ITEM_TYPE_COUNT has not changed since V16/V17.
- **How it breaks**: If ITEM_TYPE_COUNT has increased since V16/V17, the legacy struct's `allowedTypes` array would be too large, causing all subsequent fields to be read from wrong offsets. This corrupts the entire stockpile on load.
- **Player impact**: Loading a V16 or V17 save after adding new item types would corrupt all stockpile data. Currently this works because ITEM_TYPE_COUNT happens to still match, but adding any new item type in the future will break V16/V17 save loading.
- **Severity**: HIGH (latent -- will trigger on next item type addition)
- **Suggested fix**: Define hardcoded constants like `V17_ITEM_TYPE_COUNT` and `V16_ITEM_TYPE_COUNT` capturing the ITEM_TYPE_COUNT at those versions, and use them in the legacy structs. Both inspect.c and saveload.c need this fix.
- **Files**: `src/core/inspect.c` lines ~1377, ~1423; `src/core/saveload.c` lines ~783, ~827

---

### Finding 3: V20/V21 stockpile struct uses current ITEM_TYPE_COUNT for allowedTypes

- **What**: The V20/V21 legacy stockpile struct (`InspStockpileV21` / `StockpileV21`) defines `bool allowedTypes[ITEM_TYPE_COUNT]`. This is the same issue as Finding 2 but for a more recent version.
- **Assumption**: ITEM_TYPE_COUNT at V20/V21 equals the current ITEM_TYPE_COUNT.
- **How it breaks**: If a new item type is added (bumping ITEM_TYPE_COUNT from 23 to 24+), the V20/V21 legacy struct will be too large for `allowedTypes`, causing field misalignment for all fields after it. Since V20/V21 used `V21_MAT_COUNT` correctly for materials, only the types array is at risk.
- **Player impact**: Same as Finding 2 -- stockpile corruption on load. Currently works because ITEM_TYPE_COUNT hasn't changed since V20.
- **Severity**: HIGH (latent -- will trigger on next item type addition)
- **Suggested fix**: Define `V21_ITEM_TYPE_COUNT` (likely 23) and use it in both files.
- **Files**: `src/core/inspect.c` line ~1228; `src/core/saveload.c` line ~623

---

### Finding 4: Job type name bounds check uses stale constant (< 13), missing PLANT_SAPLING and CHOP_FELLED

- **What**: The `jobTypeNames` array contains 15 entries (indices 0-14), correctly listing all job types including PLANT_SAPLING (13) and CHOP_FELLED (14). However, all bounds checks in inspect.c use `job->type < 13`, which excludes the last two entries.
- **Assumption**: There are only 13 job types.
- **How it breaks**: Any job with type JOBTYPE_PLANT_SAPLING (13) or JOBTYPE_CHOP_FELLED (14) will display as "?" instead of its actual name.
- **Player impact**: When inspecting a save file with plant sapling or chop felled jobs active, the job type shows as "?" which makes debugging harder. The user cannot tell what job type a mover is working on.
- **Severity**: MEDIUM
- **Suggested fix**: Change all `job->type < 13` checks to `job->type < 15` (or better, define a constant like `JOB_TYPE_NAME_COUNT` matching the array size). Four locations need updating: lines ~146, ~199, ~726, ~759.
- **Files**: `src/core/inspect.c` lines 146, 199, 726, 759

---

### Finding 5: Designation type name bounds check uses stale constant (< 8), missing GATHER_SAPLING and PLANT_SAPLING

- **What**: The `designationTypeNames` array has 10 entries (indices 0-9), correctly listing all designation types. However, bounds checks use `desig.type < 8`, excluding DESIGNATION_GATHER_SAPLING (8) and DESIGNATION_PLANT_SAPLING (9).
- **Assumption**: There are only 8 designation types.
- **How it breaks**: Any designation of type GATHER_SAPLING or PLANT_SAPLING displays as "?" in both `print_cell` and `print_designations`.
- **Player impact**: When inspecting cells or designations, sapling-related designations show as "?" instead of their name. Confusing when debugging sapling placement/gathering issues.
- **Severity**: MEDIUM
- **Suggested fix**: Change `desig.type < 8` to `desig.type < 10` (or a constant). Two locations: lines ~434, ~646.
- **Files**: `src/core/inspect.c` lines 434, 646

---

### Finding 6: Workshop type names array only contains STONECUTTER, missing SAWMILL and KILN

- **What**: The `workshopTypeNames` array only has one entry: `{"STONECUTTER"}`. The bounds check is `ws->type < 1`. The workshop enum has WORKSHOP_STONECUTTER=0, WORKSHOP_SAWMILL=1, WORKSHOP_KILN=2, WORKSHOP_TYPE_COUNT=3.
- **Assumption**: There is only one workshop type.
- **How it breaks**: Any SAWMILL or KILN workshop displays type as "UNKNOWN" in `print_workshop`.
- **Player impact**: When inspecting workshop data, sawmills and kilns show as "UNKNOWN" type. Reduces the usefulness of the inspector for debugging workshop/crafting issues.
- **Severity**: MEDIUM
- **Suggested fix**: Update `workshopTypeNames` to `{"STONECUTTER", "SAWMILL", "KILN"}` and change the bounds check from `ws->type < 1` to `ws->type < 3` (or `ws->type < WORKSHOP_TYPE_COUNT`).
- **Files**: `src/core/inspect.c` lines 281, 294

---

### Finding 7: Inspect.c displays stale stockpile reservation counts that saveload.c clears on load

- **What**: After loading, saveload.c clears all stockpile `reservedBy` arrays to zero because reservation counts are transient runtime state (not meaningful across save/load boundaries). Inspect.c does NOT clear these, so it displays whatever stale reservation data was serialized.
- **Assumption**: The reservedBy values in the save file are meaningful.
- **How it breaks**: The reservation counts shown by `print_stockpile` reflect the moment the save was written, not a meaningful state. They may show reservations from movers that no longer have those jobs, or counts that would be zero after the normal post-load cleanup.
- **Player impact**: A developer using `--inspect --stockpile N` might see non-zero reservation counts and incorrectly think there's a reservation leak. This is misleading because saveload.c clears these on actual load. The displayed data doesn't match what the game would see.
- **Severity**: LOW
- **Suggested fix**: Either clear the reservedBy counts after loading (matching saveload.c behavior), or add a note in the output like "[NOTE: stale from save time]" next to reservation data.
- **Files**: `src/core/inspect.c` (in stockpile loading section, after all version paths)

---

### Finding 8: Inspect.c does not apply the post-load default material fixup for items

- **What**: After loading items, saveload.c iterates all active items and sets `material = DefaultMaterialForItemType(type)` for any item with `material == MAT_NONE`. Inspect.c does not do this fixup.
- **Assumption**: All items in the save file already have correct materials.
- **How it breaks**: If an item was saved with `material == MAT_NONE` (e.g., from an older version or a bug), saveload.c would fix it on load, but inspect.c would display it with MAT_NONE. The `InspectItemName` function already handles this for display purposes (it checks for MAT_NONE and uses the default), so the visual impact is minimal for the name. However, other displays that show the raw material value would show MAT_NONE.
- **Player impact**: Minor -- item names display correctly due to the InspectItemName fallback, but if raw material values were ever displayed, they'd differ from what the game sees after loading.
- **Severity**: LOW
- **Suggested fix**: Add the same post-load default material fixup loop after item loading in inspect.c, matching saveload.c behavior.
- **Files**: `src/core/inspect.c` (after item loading, before stockpile loading)

---

### Finding 9: print_cell walkability display has mismatched cellTypeNames index for "solid below" message

- **What**: In `print_cell`, when showing what's below a walkable cell, the code prints: `printf(" (solid below: %s)", cell < CELL_TYPE_COUNT ? cellTypeNames[cellBelow] : "?");`. The bounds check uses `cell` (the current cell) but prints `cellBelow`.
- **Assumption**: If the current cell type is in range, the cell below is also in range.
- **How it breaks**: If `cell` is in range but `cellBelow` is not (e.g., `cellBelow >= CELL_TYPE_COUNT`), the code accesses `cellTypeNames` out of bounds. Conversely, if `cell` is out of range but `cellBelow` is in range, it prints "?" when it could print the actual name.
- **Player impact**: Potential out-of-bounds array access leading to garbage text or crash when inspecting a cell above an unknown cell type.
- **Severity**: LOW
- **Suggested fix**: Change `cell < CELL_TYPE_COUNT` to `cellBelow < CELL_TYPE_COUNT` in the solid-below display.
- **Files**: `src/core/inspect.c` line ~384

---

## Summary

| # | Finding | Severity | Category |
|---|---------|----------|----------|
| 1 | V18/V19 stockpile MAT_COUNT mismatch | HIGH | Version migration (inspect.c + saveload.c) |
| 2 | V16/V17 stockpile ITEM_TYPE_COUNT mismatch (latent) | HIGH | Version migration (inspect.c + saveload.c) |
| 3 | V20/V21 stockpile ITEM_TYPE_COUNT mismatch (latent) | HIGH | Version migration (inspect.c + saveload.c) |
| 4 | Job type bounds check stale (< 13 vs 15 types) | MEDIUM | Display correctness |
| 5 | Designation type bounds check stale (< 8 vs 10 types) | MEDIUM | Display correctness |
| 6 | Workshop type names missing SAWMILL and KILN | MEDIUM | Display correctness |
| 7 | Stale stockpile reservation counts not cleared | LOW | Inspect/saveload parity |
| 8 | Missing post-load default material fixup | LOW | Inspect/saveload parity |
| 9 | Mismatched bounds check variable for cellBelow name | LOW | Display correctness |

**Critical takeaway**: Finding 1 is actively broken right now -- loading V18 or V19 saves will corrupt stockpile data because `MAT_COUNT` has already expanded from 10 to 14. Findings 2 and 3 are latent bombs that will detonate the next time a new item type is added. All three affect both inspect.c and saveload.c identically.
