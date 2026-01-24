# Jobs Rehaul Performance Investigation

## Problem
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

## Ideas for Future Optimization
1. **Dirty marking**: When filter changes, mark affected items as "needs rehaul check" instead of scanning all items every frame
2. **Throttle rehaul checks**: Only check for rehaul every N frames, not every frame
3. **Batch assignments**: Assign jobs in batches rather than one-at-a-time with full slot search each time
4. **Skip pathfinding on assignment**: Let movers discover unreachable items themselves (already have stuck detection)

## Files Involved
- `pathing/jobs.c`: `AssignJobs()`, PRIORITY 3 section (Jobs_FindRehaulItem)
- `pathing/stockpiles.c`: `FindStockpileForItem()`, `FindHigherPriorityStockpile()`
