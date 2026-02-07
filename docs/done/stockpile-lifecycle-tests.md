# Stockpile Lifecycle Tests - TDD Red Phase

## Summary

Created comprehensive behavior-driven tests for stockpile audit findings from `/Users/nikkikoole/Projects/navkit/docs/todo/audits/stockpiles.md`. All tests are currently **FAILING** (red phase), confirming the bugs exist.

## Test Results

**13 failing assertions** in the new `describe(stockpile_lifecycle)` block:

### HIGH Severity Failures (6 failures)

1. **Finding 2: DeleteStockpile doesn't drop IN_STOCKPILE items to ground**
   - Test: "deleting a stockpile should drop all stored items to ground"
   - 3 items remain in ITEM_IN_STOCKPILE state after stockpile deletion
   - Player impact: Items become invisible and inaccessible (item loss)

2. **Finding 3: PlaceItemInStockpile doesn't validate cell is active**
   - Test: "placing item in inactive stockpile cell should not corrupt slot data"
   - Item enters phantom ITEM_IN_STOCKPILE state in inactive cell
   - Player impact: Item effectively lost in non-existent stockpile

### MEDIUM Severity Failures (7 failures)

3. **Finding 1: RemoveStockpileCells doesn't release slot reservations**
   - Test: "removing stockpile cell should release slot reservations"
   - Item reservation not cleared when cell removed
   - Player impact: Phantom reservations block future hauling

4. **Finding 5: RemoveStockpileCells doesn't clear item reservations**
   - Test: "removing stockpile cell should clear item reservations"
   - Item reservation persists after being dropped to ground
   - Player impact: Items stuck as "reserved", blocking pickup

5. **Finding 6: PlaceItemInStockpile assumes slotTypes/Materials match**
   - Test: "placing mismatched item type in occupied slot should not corrupt slot data"
   - Slot type overwritten to GREEN when RED already present
   - Slot count incremented despite type mismatch
   - Player impact: Stacking invariant breaks, mixed types in one slot

6. **Finding 9: FindStockpileForItem doesn't respect priority**
   - Test: "movers should haul items to stockpiles in priority order"
   - Item went to priority-1 stockpile instead of priority-9
   - Player impact: High-priority stockpiles ignored during hauling

7. **Combined test: Findings 1, 2, 5**
   - Test: "removing stockpile cells with items should not leave items in limbo"
   - Multiple reservations not cleared
   - Haul job not cancelled properly
   - Player impact: Items in limbo, jobs stuck, reservations leaked

## Test File Location

- **File**: `/Users/nikkikoole/Projects/navkit/tests/test_jobs.c`
- **Block**: `describe(stockpile_lifecycle)` (lines 8363-8826)
- **Registration**: Added to main() at line 9087

## Test Coverage

The tests verify player expectations for:

1. **Stockpile deletion** - Items should become ground items
2. **Cell removal during active jobs** - Graceful handling, no corruption
3. **Reservation management** - All reservations cleared when cells removed
4. **Item reservation cleanup** - Reserved items unreserved when dropped
5. **Type safety** - Reject mismatched item types in occupied slots
6. **Coordinate validation** - Prevent wrong stockpile operations
7. **Priority system** - Respect stockpile priority when hauling
8. **Combined edge cases** - Mass removal while items being hauled

## Next Steps (Green Phase)

To fix these bugs:

1. **DeleteStockpile** - Loop through items, set ITEM_IN_STOCKPILE → ITEM_ON_GROUND for items in deleted stockpile
2. **PlaceItemInStockpile** - Add `if (!sp->cells[idx]) return;` validation at start
3. **RemoveStockpileCells** - Add `sp->reservedBy[idx] = 0;` before clearing cell
4. **RemoveStockpileCells** - Add `items[i].reservedBy = -1;` when dropping items
5. **PlaceItemInStockpile** - Add type/material validation, reject mismatches
6. **RemoveItemFromStockpileSlot** - Add item state/coordinate validation
7. **FindStockpileForItem** - Sort by priority or iterate priority values descending

## Build Commands

```bash
# Force test rebuild
rm -f bin/test_unity.o

# Run tests
make test

# Run only jobs tests
make test_jobs
```

## Test Philosophy

These tests follow TDD principles:

- **Red**: Tests written first, expect failure (CURRENT STATE)
- **Green**: Fix code to make tests pass
- **Refactor**: Clean up while keeping tests green

Tests describe **player expectations**, not current implementation. They answer:
- "If I did X, what would I expect to see happen?"
- "What would feel broken if it didn't work this way?"

## Expected Test Output (After Fixes)

```
stockpile_lifecycle
	[✓] "deleting a stockpile should drop all stored items to ground"
	[✓] "placing item in inactive stockpile cell should not corrupt slot data"
	[✓] "removing stockpile cell should release slot reservations"
	[✓] "removing stockpile cell should clear item reservations"
	[✓] "placing mismatched item type in occupied slot should not corrupt slot data"
	[✓] "removing item from stockpile should validate item state and coordinates"
	[✓] "movers should haul items to stockpiles in priority order"
	[✓] "removing stockpile cells with items should not leave items in limbo"

Total: 187
	Passed: 626
	Failed: 0
```
