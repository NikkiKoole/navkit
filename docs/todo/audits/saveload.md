# Saveload Audit Report

**File**: `src/core/saveload.c`
**Cross-referenced**: `src/core/inspect.c`, entity headers, material.h
**Date**: 2026-02-07

---

## Finding 1: V19/V18 legacy stockpile structs use current MAT_COUNT instead of hardcoded value

- **What**: The V19 and V18 stockpile migration paths define legacy structs with `bool allowedMaterials[MAT_COUNT]`. The current `MAT_COUNT` is 14 (after clay/gravel/sand/peat were added), but at V18/V19 time `MAT_COUNT` was 10 (matching `V21_MAT_COUNT`).
- **Assumption**: The legacy struct size matches what was actually written to disk at that version.
- **How it breaks**: Loading a genuine V18 or V19 save file. The `allowedMaterials` array in the legacy struct has 14 entries instead of 10, making the struct 4 bools too large. Every field after `allowedMaterials` reads from the wrong byte offset. The entire stockpile data (cells, slots, reservedBy, slotCounts, slotTypes, slotMaterials, etc.) is garbled.
- **Player impact**: Loading an old V18/V19 save corrupts all stockpile data. Items appear in wrong slots, reservation counts are nonsense, slot types are random. Likely crash or severe gameplay corruption.
- **Severity**: **HIGH** (data corruption on V18/V19 save load)
- **Suggested fix**: Replace `MAT_COUNT` with `V21_MAT_COUNT` (10) in the V19 and V18 `StockpileV19` and `StockpileV18` legacy struct definitions for the `allowedMaterials` field. Then in the migration loop, copy the 10 old materials and default the new 4 to `true`, matching the V20/V21 migration pattern.
- **Files affected**: `src/core/saveload.c` (lines ~686, ~735), `src/core/inspect.c` (lines ~1279, ~1329)

---

## Finding 2: V17/V16 legacy stockpile structs use current ITEM_TYPE_COUNT instead of hardcoded value

- **What**: The V17 and V16 (version < 17) stockpile migration paths define legacy structs with `bool allowedTypes[ITEM_TYPE_COUNT]`. The current `ITEM_TYPE_COUNT` is 25, but V16/V17 saves were written when `ITEM_TYPE_COUNT` was 21 (matching `V18_ITEM_TYPE_COUNT`).
- **Assumption**: The legacy struct matches the binary layout when the save was written.
- **How it breaks**: Loading a V16 or V17 save file. The `allowedTypes` array is 4 bools too large, shifting all subsequent fields. All stockpile slot data, reservation counts, and ground item caches read garbage.
- **Player impact**: Same as Finding 1 -- stockpile corruption on old save load. Currently a latent bug (documented in MEMORY.md) because no one is loading V16/V17 saves with the current binary. Will break immediately if ITEM_TYPE_COUNT changes again.
- **Severity**: **HIGH** (data corruption on V16/V17 save load; latent time bomb for V20/V21 paths too)
- **Suggested fix**: Define `V17_ITEM_TYPE_COUNT = 21` and `V16_ITEM_TYPE_COUNT = 21` (or whatever the correct values were) and use those in the legacy struct definitions. The V20/V21 path also uses `ITEM_TYPE_COUNT` and should use a hardcoded `V20_ITEM_TYPE_COUNT = 25`.
- **Files affected**: `src/core/saveload.c` (lines ~783, ~827), `src/core/inspect.c` (lines ~1377, ~1423)

---

## Finding 3: itemCount, stockpileCount, workshopCount, and blueprintCount not restored on load

- **What**: `LoadWorld()` restores entity arrays by raw `fread` (items, stockpiles, workshops, blueprints) but never recalculates the running count globals (`itemCount`, `stockpileCount`, `workshopCount`, `blueprintCount`). These counters are maintained by `SpawnItem`/`DeleteItem`, `CreateStockpile`/`DeleteStockpile`, etc. -- none of which are called during load.
- **Assumption**: The count globals reflect the number of active entities in their arrays.
- **How it breaks**: After loading a save, `itemCount` is 0 (from ClearMovers->ClearJobs chain), `stockpileCount` is 0, `workshopCount` is 0, and `blueprintCount` is 0, even though the arrays are full of active entities.
- **Player impact**: The UI panel shows "Jobs (0 items)" even when items exist. Any code that uses these counters for iteration limits or capacity checks will malfunction. For example, if `CreateStockpile` uses `stockpileCount` to check capacity, it would allow creating more stockpiles than `MAX_STOCKPILES`. If any loop iterates up to `stockpileCount` instead of `MAX_STOCKPILES`, it would skip all loaded stockpiles.
- **Severity**: **HIGH** (affects UI display and any logic depending on these counters)
- **Suggested fix**: After loading each entity array, scan the loaded data and reconstruct the count. For items: `itemCount = 0; for (i = 0; i < itemHighWaterMark; i++) if (items[i].active) itemCount++;`. Same pattern for stockpiles, workshops, blueprints.

---

## Finding 4: jobFreeList not rebuilt after load -- job slot leak across save/load cycles

- **What**: `LoadWorld()` calls `ClearJobs()` which sets `jobFreeCount = 0`, then overwrites `jobs[]`, `jobIsActive[]`, `activeJobList[]`, `jobHighWaterMark`, and `activeJobCount` from the save file. But `jobFreeList` (the reuse pool of freed job indices) is never populated with the inactive gaps between index 0 and `jobHighWaterMark`.
- **Assumption**: `CreateJob()` can reuse freed slots via the free list.
- **How it breaks**: After loading, any inactive job slots in the range [0, jobHighWaterMark) are orphaned -- they're not in the free list and not in the active list. `CreateJob()` always falls through to `jobHighWaterMark++`. Over repeated save/load cycles, `jobHighWaterMark` creeps toward `MAX_JOBS` while unused slots accumulate in the middle.
- **Player impact**: After many save/load cycles, job creation starts failing ("pool full"), even though most slots are unused. The player sees movers standing idle because no new jobs can be created. This is a slow leak that only manifests after prolonged play with saves.
- **Severity**: **MEDIUM** (gradual degradation, not immediate crash)
- **Suggested fix**: After loading jobs, rebuild the free list: `jobFreeCount = 0; for (int i = 0; i < jobHighWaterMark; i++) if (!jobs[i].active) jobFreeList[jobFreeCount++] = i;`

---

## Finding 5: Stockpile reservations cleared but item reservations are not reset on load

- **What**: `LoadWorld()` explicitly clears stockpile `reservedBy` counts after load ("Clear transient reservation counts -- not meaningful across save/load"). However, the `items[i].reservedBy` field is loaded as-is from the save file and never cleared. If the save was made mid-operation (mover was carrying an item, had reserved another), those reservations persist.
- **Assumption**: All transient reservations are cleared on load so the simulation starts clean.
- **How it breaks**: A save made while a mover was mid-haul will have `items[targetItem].reservedBy = moverIdx`. After load, the movers get their `currentJobId` set from the save, but the job system state may not perfectly resume (e.g., the mover's path is loaded but may be stale). If the job gets cancelled on the first tick due to a stuck detection or path failure, `CancelJob` would normally release the reservation -- but if the job doesn't get cancelled, the item stays reserved forever by a mover that may never pick it up.
- **Player impact**: Items appear unreserved but the job system skips them because `reservedBy` is set. The item sits on the ground and nobody picks it up. The player sees items being ignored.
- **Severity**: **MEDIUM** (stale reservations block item pickup; may self-correct if jobs resume correctly)
- **Suggested fix**: Clear all item reservations after load, same as stockpile reservations: `for (int i = 0; i < itemHighWaterMark; i++) items[i].reservedBy = -1;` This is safe because `AssignJobs()` will re-reserve items as needed.

---

## Finding 6: Workshop, Blueprint, and Designation assignments not cleared on load

- **What**: Workshops have `assignedCrafter`, blueprints have `assignedBuilder` and `reservedItem`, and designations have `assignedMover` -- all referencing mover indices. These are loaded from the save file as-is. The save could have been made mid-operation with movers assigned to these targets.
- **Assumption**: Mover-to-target assignments are consistent after load.
- **How it breaks**: Consider: Mover 3 was assigned to workshop 0 (`ws.assignedCrafter = 3`) with craft job 7. After load, mover 3 has `currentJobId = 7`, job 7 references workshop 0. If the mover's path is stale and it gets stuck, the stuck detector cancels the job. `CancelJob` should release `ws.assignedCrafter`, but if the cancellation path doesn't match the exact state (e.g., the job step doesn't match what CancelJob expects), the workshop stays permanently assigned to mover 3 and no other mover can ever craft there.
- **Player impact**: Workshop appears "in use" but nobody is working. No crafting happens at that workshop ever again.
- **Severity**: **MEDIUM** (can permanently disable a workshop if job cancellation doesn't clean up perfectly)
- **Suggested fix**: After load, clear all transient assignments: `workshops[i].assignedCrafter = -1`, `blueprints[i].assignedBuilder = -1`, `blueprints[i].reservedItem = -1`, `designations[z][y][x].assignedMover = -1` and reset `designations[z][y][x].progress = 0`. Then cancel all active jobs and let `AssignJobs()` reassign from scratch on the next tick. Alternatively, accept the risk and rely on existing job resumption working correctly.

---

## Finding 7: wearGrid (ground wear) not saved or loaded

- **What**: The `wearGrid[z][y][x]` array tracks how worn down ground is (grass -> dirt paths). It is never written in `SaveWorld()` or read in `LoadWorld()`.
- **Assumption**: Ground wear persists across save/load.
- **How it breaks**: Every save/load resets all ground wear to 0. Paths that movers have worn down to dirt revert to grass.
- **Player impact**: After loading a save, all the naturally-worn paths that formed from mover foot traffic disappear. Cosmetic grass-to-dirt transitions reset. Not gameplay-breaking but immersion-breaking for players who enjoy watching paths form.
- **Severity**: **LOW** (cosmetic loss, no gameplay impact)
- **Suggested fix**: Add `wearGrid` to save/load alongside the other grids.

---

## Finding 8: currentTick not saved/loaded -- tick-based scheduling resets

- **What**: `currentTick` (the global simulation tick counter) is set to 0 by `ClearMovers()` during load and never restored. It is not saved to disk.
- **Assumption**: Tick-based scheduling (staggered updates, frame-offset computations) works correctly regardless of tick count.
- **How it breaks**: If any system uses `currentTick % N` for staggered updates, all movers will sync up on the same frame after load, causing a spike. Also, any time-based progress that uses tick counting (as opposed to float accumulators) would reset.
- **Player impact**: Brief frame rate stutter right after loading as all staggered updates fire simultaneously. Probably imperceptible in practice.
- **Severity**: **LOW** (minor frame spike after load)
- **Suggested fix**: Save and restore `currentTick`. Or accept the minor stutter.

---

## Finding 9: Raw blob save for Workshops, Jobs, and Movers -- no version migration path

- **What**: Workshops, Jobs, and Movers are saved/loaded via raw `fwrite`/`fread` of the entire struct array with no version checks. If the struct layout changes (fields added, removed, or reordered), old saves become unreadable with no migration path.
- **Assumption**: These structs never change.
- **How it breaks**: Adding any field to `Workshop`, `Job`, or `Mover` struct will silently corrupt all existing saves. The read will deserialize bytes at the wrong offsets.
- **Player impact**: Old saves crash or produce garbage data after a code update that changes these structs.
- **Severity**: **MEDIUM** (not a current bug, but a structural risk -- any Workshop/Job/Mover struct change will break saves)
- **Suggested fix**: When modifying these structs, add a version-gated migration path similar to what exists for Stockpile and Blueprint. Alternatively, add a structural hash or size check to detect mismatches early.

---

## Finding 10: Inspect.c V19/V18 paths have same MAT_COUNT bug as saveload.c

- **What**: The inspector's V19 and V18 stockpile migration paths (`InspStockpileV19`, `InspStockpileV18`) use `MAT_COUNT` (14) for `allowedMaterials`, but the correct value at those save versions was 10 (`INSPECT_V21_MAT_COUNT`).
- **Assumption**: Inspect.c and saveload.c use identical struct layouts for the same version.
- **How it breaks**: Inspecting a V18/V19 save file produces garbled stockpile data -- same root cause as Finding 1.
- **Player impact**: The `--inspect` diagnostic tool gives wrong information for old saves, making debugging harder.
- **Severity**: **HIGH** (parallel bug to Finding 1, must be fixed in tandem)
- **Suggested fix**: Use `INSPECT_V21_MAT_COUNT` (10) in the V19 and V18 legacy structs, matching the saveload.c fix.

---

## Finding 11: V20/V21 stockpile legacy struct uses current ITEM_TYPE_COUNT (fragile)

- **What**: The V20/V21 stockpile migration path (`StockpileV21`) uses `bool allowedTypes[ITEM_TYPE_COUNT]`. This is currently correct (ITEM_TYPE_COUNT was 25 at V20/V21, same as now). But it will silently break the next time a new item type is added.
- **Assumption**: ITEM_TYPE_COUNT hasn't changed since V20.
- **How it breaks**: Adding ITEM_POTTERY or any new item type would change ITEM_TYPE_COUNT to 26, making the V20/V21 legacy struct 1 bool too large, corrupting all subsequent field reads when loading a V20/V21 save.
- **Player impact**: Same as Finding 2 -- silent stockpile corruption on next item type addition.
- **Severity**: **MEDIUM** (not broken now, but guaranteed to break on next ITEM_TYPE_COUNT change)
- **Suggested fix**: Define `V20_ITEM_TYPE_COUNT = 25` and use it in the legacy struct. Same in inspect.c.

---

## Summary

| # | Finding | Severity | Broken Now? |
|---|---------|----------|-------------|
| 1 | V19/V18 MAT_COUNT in stockpile structs | HIGH | Yes (V18/V19 saves) |
| 2 | V17/V16 ITEM_TYPE_COUNT in stockpile structs | HIGH | Yes (V16/V17 saves) |
| 3 | Entity count globals not restored on load | HIGH | Yes (every load) |
| 4 | jobFreeList not rebuilt after load | MEDIUM | Yes (every load, slow leak) |
| 5 | Item reservations not cleared on load | MEDIUM | Conditional (mid-operation saves) |
| 6 | Workshop/Blueprint/Designation assignments stale | MEDIUM | Conditional (mid-operation saves) |
| 7 | wearGrid not saved/loaded | LOW | Yes (cosmetic) |
| 8 | currentTick not saved/loaded | LOW | Yes (minor stutter) |
| 9 | No version migration for Workshop/Job/Mover blobs | MEDIUM | No (structural risk) |
| 10 | Inspect.c same MAT_COUNT bug as saveload.c | HIGH | Yes (V18/V19 inspect) |
| 11 | V20/V21 ITEM_TYPE_COUNT fragile (not hardcoded) | MEDIUM | No (time bomb) |

**Highest priority fixes**: Findings 1, 2, 3, 10 (data corruption on load).
**Next priority**: Findings 4, 5, 11 (gradual degradation and time bombs).
