Assumption Audit: workshops.c

## Finding 1: Workshop deletion doesn't invalidate paths through un-blocked tiles

**What**: `DeleteWorkshop` clears `CELL_FLAG_WORKSHOP_BLOCK` for machinery tiles and marks chunks dirty, but doesn't call `InvalidatePathsThroughCell` for those tiles.

**Assumption**: HPA* chunk marking is sufficient to update pathfinding after removing blocking tiles.

**How it breaks**: If movers have pre-computed paths that avoided workshop tiles, they won't recalculate paths through newly-walkable cells until they happen to repath for other reasons. This is inconsistent with `CreateWorkshop` which explicitly calls `InvalidatePathsThroughCell`.

**Player impact**: After deleting a workshop, movers continue taking long detours around where the workshop used to be until something else triggers a repath. The workshop is visually gone but pathfinding treats it as still blocking.

**Severity**: Medium (inefficiency)

**Suggested fix**: In `DeleteWorkshop`, add `InvalidatePathsThroughCell(ws->x + tx, ws->y + ty, ws->z);` inside the loop where `CELL_FLAG_WORKSHOP_BLOCK` is cleared, matching the pattern in `CreateWorkshop`.

---

## Finding 2: Workshop diagnostics doesn't handle deleted workshop mid-update

**What**: `UpdateWorkshopDiagnostics` reads `workshops[w].assignedCrafter` and then dereferences `movers[ws->assignedCrafter]` without checking if that mover still exists.

**Assumption**: If `assignedCrafter >= 0`, the mover at that index is active and valid.

**How it breaks**: If a mover is deleted (e.g., `ClearMovers` or individual deletion) but `assignedCrafter` wasn't cleared first, the diagnostic reads invalid mover data. The check `ws->assignedCrafter < moverCount` is insufficient because `moverCount` is a high-water mark that doesn't shrink when movers are deleted.

**Player impact**: Likely a crash or garbage data when hovering over workshops after loading a save with different mover counts, or if movers are ever deleted during gameplay.

**Severity**: High (potential crash)

**Suggested fix**: Check `m->active` after getting the mover pointer: `if (m->active && m->currentJobId >= 0)`.

---

## Finding 3: Recipe input matching doesn't validate item is reachable on same z-level

**What**: `WorkshopHasInputForRecipe` checks `(int)item->z != ws->z` and continues if they don't match, but this is inside the loop. The function can still return `true` if it finds ANY item on the same z-level.

**Assumption**: All checks are correct as written.

**How it breaks**: This actually works correctly on second inspection - the `if ((int)item->z != ws->z) continue;` correctly skips items on wrong z-levels. No bug here.

**Player impact**: N/A

**Severity**: N/A (false alarm)

---

## Finding 4: Workshop deletion doesn't clear bills that movers might be executing

**What**: `DeleteWorkshop` sets `ws->active = false` but doesn't cancel any craft jobs that movers are currently executing for that workshop.

**Assumption**: Job validation in `RunJob_Craft` will catch `!ws->active` and fail gracefully.

**How it breaks**: This assumption is correct - `RunJob_Craft` checks `if (!ws->active) return JOBRUN_FAIL;` at the start. However, there's a timing issue: between the workshop deletion and the next `JobsTick`, the mover is in an inconsistent state where `assignedCrafter` is set but the workshop is inactive.

**Player impact**: One frame of inconsistency where workshop diagnostics might misbehave (see Finding 2). The job will fail on next tick and clean up properly via `CancelJob`.

**Severity**: Low (transient inconsistency, already has guards)

**Suggested fix**: In `DeleteWorkshop`, explicitly cancel jobs for `ws->assignedCrafter` before marking inactive: 
```c
if (ws->assignedCrafter >= 0 && ws->assignedCrafter < moverCount) {
    Mover* m = &movers[ws->assignedCrafter];
    if (m->active && m->currentJobId >= 0) {
        CancelJob(m, ws->assignedCrafter);
    }
}
```

---

## Finding 5: Bill removal doesn't invalidate running jobs targeting that bill

**What**: `RemoveBill` shifts bills down in the array but doesn't cancel craft jobs that reference bill indices after the removed one.

**Assumption**: Job validation will detect `job->targetBillIdx >= ws->billCount` and fail.

**How it breaks**: Worse than that - if you remove bill 0 and bills shift down, a mover executing bill 1 (now bill 0) continues with `job->targetBillIdx = 1`, which now points to the OLD bill 2. The mover executes the wrong recipe! The bounds check `job->targetBillIdx >= ws->billCount` only catches removal of the LAST bill or bills beyond the current last.

**Player impact**: Player removes a bill, but a mover continues crafting the wrong recipe because bill indices shifted. This is a serious logic bug that violates player expectations - removing a bill doesn't stop that bill, it shifts which recipe is being executed.

**Severity**: High (broken behavior)

**Suggested fix**: In `RemoveBill`, scan all movers for craft jobs targeting this workshop and cancel jobs where `job->targetBillIdx == billIdx` OR `job->targetBillIdx > billIdx` (shift their index down, or just cancel them all to be safe).

---

## Finding 6: `CountItemsInStockpiles` counts reserved items

**What**: `CountItemsInStockpiles` sums `sp->slotCounts[slotIdx]` for matching types but doesn't check if those items are reserved by other jobs.

**Assumption**: For bill mode `BILL_DO_UNTIL_X`, we want to count total items in stockpiles regardless of reservation.

**How it breaks**: If 10 crafters are all simultaneously making blocks, all 10 check "do we have < X blocks?" and all see the same count (excluding in-flight outputs). This actually seems intentional - you want to count what's stored, not what's currently being processed. But wait - this counts items IN stockpiles. Items on the ground being hauled aren't counted yet. So timing matters.

**Player impact**: "Make until 100 blocks" bills might overshoot by a few items because multiple crafters see the same count and all start jobs. But this is probably acceptable behavior - slight overshoot is better than undershoot.

**Severity**: Low (acceptable race condition)

**Suggested fix**: None needed - this is likely intentional design.

---

## Finding 7: Workshop diagnostics checks output storage for EVERY available input, not just ONE

**What**: In `UpdateWorkshopDiagnostics`, when checking if output storage is available, the code loops through ALL items matching the recipe, checks if EACH has storage, and sets `hasStorage = true` if ANY has storage. Then it only marks `anyOutputSpace = true` if `hasStorage` was set.

**Assumption**: We only need storage for the material we actually have as input.

**How it breaks**: This is actually correct logic! The outer loop is checking all bills, and for each bill, we want to know "is there any input material we have that ALSO has output storage?" If yes, the bill is runnable. The loop breaks early when it finds one valid material. No bug here.

**Player impact**: N/A

**Severity**: N/A (correct behavior)

---

## Finding 8: `ShouldBillRun` doesn't check if input materials or output storage exist

**What**: `ShouldBillRun` only checks bill mode completion targets (DO_X_TIMES, DO_UNTIL_X, DO_FOREVER) but doesn't validate that materials or storage are available.

**Assumption**: `ShouldBillRun` is only about bill-mode logic, not resource availability. Resource checks happen in `WorkGiver_Craft` and `UpdateWorkshopDiagnostics`.

**How it breaks**: This is correct separation of concerns - `ShouldBillRun` answers "does this bill's completion logic say to run?" while other code checks "CAN we run?"

**Player impact**: N/A

**Severity**: N/A (correct design)

---

## Finding 9: Bill suspension doesn't clear `assignedCrafter` for in-progress jobs

**What**: `SuspendBill` sets `bill->suspended = true` but doesn't cancel the mover currently executing that bill (if any).

**Assumption**: The mover will finish the current craft operation, and then `WorkGiver_Craft` won't assign new jobs for suspended bills.

**How it breaks**: This assumption is intentional behavior - suspending a bill doesn't cancel in-flight work, only prevents NEW work. The player pressed suspend expecting future crafts to stop, and that's what happens. Current craft finishing is probably desirable.

**Player impact**: Player suspends a bill, the current crafter finishes their item (next few seconds), then no more work happens. This seems like good UX - don't waste partial work.

**Severity**: N/A (intentional design)

---

## Finding 10: `FindNearestFuelItem` and `WorkshopHasFuelForRecipe` have identical search logic but separate implementations

**What**: Both functions loop through items checking IF_FUEL flag, unreserved, same z-level, within radius, but one returns bool and one returns index.

**Assumption**: Having separate functions is clearer than a shared helper.

**How it breaks**: If someone updates the search criteria in one function (e.g., adds a new filter), they must remember to update the other. This has already shown drift potential - both have identical structure but could diverge.

**Player impact**: Future bug risk - if search logic changes in one place but not the other, fuel availability checks become inconsistent with actual fuel item selection.

**Severity**: Low (code quality / future maintenance risk)

**Suggested fix**: Extract shared logic into a helper that takes a callback or returns a result struct. Or at minimum, add a comment linking the two functions so changes are synchronized.

---

## Summary

**High severity findings:**
- Finding 2: Workshop diagnostics crashes on invalid mover index
- Finding 5: Bill removal causes wrong recipe execution

**Medium severity findings:**
- Finding 1: Workshop deletion doesn't invalidate paths

**Low severity findings:**  
- Finding 4: Workshop deletion timing inconsistency
- Finding 10: Duplicate fuel search logic
