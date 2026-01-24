# Jobs Rehaul Performance Investigation

## Completed Optimizations

### hasGroundItem Cache (commits `165b846`, `3ba0158`)

**Problem:** `FindFreeStockpileSlot()` called `FindGroundItemAtTile()` for every slot it checked. With many stockpile tiles and items, this became O(tiles × items) per assignment attempt - **79% of frame time** in pathological cases.

**Solution:** Added `hasGroundItem[slot]` bool array to Stockpile struct.
- `RebuildStockpileGroundItemCache()` does a full rebuild once per frame at start of `AssignJobs()`
- `SpawnItem()` marks cache incrementally for immediate correctness
- `FindFreeStockpileSlot()` now does O(1) bool check instead of O(items) lookup
- `FindGroundItemOnStockpile()` skips tiles entirely when cache shows no ground item

**Result:** Steady-state with 500 items, 200 movers, 2 stockpiles - rendering is now the top cost instead of job assignment.

---

## Known Issues

### 1. Deadlock when swapping filters on full stockpiles
**Repro:** Two full stockpiles (red and blue). Swap their filters (red→blue, blue→red). Nothing happens - items don't get rehauled.

**Cause:** Both stockpiles are full. To move a red item out of the (now blue) stockpile, we need a free slot in a red stockpile. But the red stockpile is also full of blue items that need to move out. Classic deadlock - no empty cell to start the swap.

**Possible fix:** Temporary "swap buffer" - allow one item to be dropped to ground to break the deadlock. Or detect this case and force a safe-drop.

### 2. ~~FindFreeStockpileSlot still expensive when stockpiles are full~~ FIXED (commit `205dfc7`)
**Repro:** Many items on ground, but all stockpiles are full (no room). Profiler shows:
- `AssignJobs` → `FindStockpileForItem` → `FindFreeStockpileSlot` taking 35%+ of frame time

**Cause:** For each ground item, we call `FindStockpileForItem` which scans all stockpiles calling `FindFreeStockpileSlot` on each. Even though slots are full, we still iterate all tiles checking for free slots.

**Fix:** Added `freeSlotCount` per stockpile, rebuilt once per frame via `RebuildStockpileFreeSlotCounts()`. `FindStockpileForItem` now skips stockpiles with `freeSlotCount <= 0`.

---

## Remaining Problem (filter change rehaul storm)
When stockpile filters change (e.g., red stockpile no longer accepts red items), `AssignJobs` becomes slow because it scans all stockpiled items every frame to check if they need rehauling.

## Benchmark Results
With 256 items in 3 stockpiles, 50 movers:
- Steady state: ~350ms for 100 iterations (~3.5ms/frame)
- After filter change: ~30,000ms for 100 iterations (~300ms/frame) - **89x slower**

## Root Cause
The rehaul loop in `AssignJobs` (PRIORITY 3) iterates through all items every frame:
1. For each stockpiled item, checks `StockpileAcceptsType()` 
2. If no longer allowed, calls `FindStockpileForItem()` to find new home
3. Calls `TryAssignItemToMover()` which does pathfinding

With 80 items needing rehaul and 50 movers, that's 50 pathfinding calls per frame.

## Attempted Optimizations (reverted - didn't help this case)
1. **freeSlotCount cache per stockpile** - Early exit when stockpile is full
2. **Max priority cache** - Early exit in `FindHigherPriorityStockpile` when already at max

These didn't help because:
- Stockpiles weren't full (60% capacity)
- All stockpiles had same priority

## Ideas for Future Optimization (Simple Language)

### 1. Dirty Marking (don't check everything every frame)
**Current:** Every frame, look at ALL items asking "do you need to move?"
**Better:** When a filter changes, mark only those affected items. Next frame, only check marked items.
*Like a to-do list instead of checking every item in your house every day.*

### 2. Connectivity Regions (cheap "can reach?" check)
**Current:** To know if mover can reach an item, run full pathfinding (expensive).
**Better:** Pre-compute "zones". If mover is in zone 1 and item is in zone 2 with no connection, instantly say "unreachable".
*Like knowing "the bridge is out" without driving there to check.*
*(Already in jobs-roadmap.md Phase 2)*

### 3. WorkGivers (split the giant function)
**Current:** One giant `AssignJobs()` does everything - clearing, hauling, rehauling, all mixed together.
**Better:** Separate modules: `WorkGiver_Haul`, `WorkGiver_Rehaul`, `WorkGiver_Mine`, etc. Each one independent.
*Easier to optimize individually, easier to add new job types.*
*(Already in jobs-roadmap.md Phase 4)*

### 4. Job Pool (jobs exist separately from movers)
**Current:** Jobs only exist when assigned to a mover. Mover holds all job data.
**Better:** Jobs exist in a list waiting to be done. Movers grab from the list.
*Like a job board - jobs are posted, workers pick them up.*
*(Already in jobs-roadmap.md Phase 4)*

### 5. Throttle rehaul checks
Only check for priority/overfull rehaul every N frames, not every single frame.

### 6. Skip pathfinding on assignment
Let movers discover unreachable items themselves (already have stuck detection) instead of checking upfront.

## Files Involved
- `pathing/jobs.c`: `AssignJobs()`, PRIORITY 3 section (Jobs_FindRehaulItem)
- `pathing/stockpiles.c`: `FindStockpileForItem()`, `FindHigherPriorityStockpile()`
