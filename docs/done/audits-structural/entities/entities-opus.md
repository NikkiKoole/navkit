 Plan: Fix High-Severity Structural Issues from Entities Audit

## Context

The structural audit of `src/entities/` found 15 patterns. This plan addresses the 3 high-severity findings that represent actual bugs or immediate maintenance traps, plus 2 medium-severity repeated code issues that are cheap to consolidate.

We skip Pattern 1 (DesignationType cascade), Pattern 2 (ItemType cascade), and Pattern 7 (saveload/inspect parallel updates) — those are inherent to the architecture and would require major refactoring to address. They're documented risks, not fixable with targeted changes.

---

## Fix 1: Make CancelJob public and use it from RemoveBill (Pattern 5 — HIGH)

**Bug**: `RemoveBill` in `workshops.c` reimplements CancelJob but misses `targetItem` release. If a craft job is in CRAFT_STEP_WORKING with a deposited input item, removing the bill leaks that item's reservation forever.

**Changes**:
- `src/entities/jobs.c`: Change `CancelJob` from `static` to public. Remove the forward declaration at line ~2043.
- `src/entities/jobs.h`: Add `void CancelJob(Mover* m, int moverIdx);` declaration.
- `src/entities/workshops.c:RemoveBill()`: Replace the inline CancelJob reimplementation (~30 lines) with a call to `CancelJob(&movers[moverIdx], moverIdx)`.

---

## Fix 2: Extract safe-drop helper to deduplicate CancelJob (Pattern 4 — MEDIUM)

**Problem**: The 20-line safe-drop pattern (check walkable, search 8 neighbors, fallback) is copy-pasted for `carryingItem` and `fuelItem` in CancelJob.

**Changes**:
- `src/entities/jobs.c`: Extract a `static void SafeDropItem(int itemIdx, Mover* m)` helper that:
  - Sets `item->state = ITEM_ON_GROUND`
  - Sets `item->reservedBy = -1`
  - Checks `IsCellWalkableAt` at mover position
  - Searches 8 neighbors if not walkable
  - Falls back to mover position
- Replace both carryingItem and fuelItem safe-drop blocks in `CancelJob` with calls to `SafeDropItem`.

---

## Fix 3: Make ClearSourceStockpileSlot call RemoveItemFromStockpileSlot (Pattern 3 — MEDIUM)

**Problem**: `ClearSourceStockpileSlot` in jobs.c is a near-exact copy of `RemoveItemFromStockpileSlot` in stockpiles.c. Both do the same slot cleanup but are maintained independently.

**Changes**:
- `src/entities/jobs.c:ClearSourceStockpileSlot()`: Replace the body with a call to `RemoveItemFromStockpileSlot(item->x, item->y, (int)item->z)` from stockpiles.c.

---

## Files Modified

1. `src/entities/jobs.h` — Add CancelJob declaration
2. `src/entities/jobs.c` — Make CancelJob public, extract SafeDropItem, simplify ClearSourceStockpileSlot
3. `src/entities/workshops.c` — Replace RemoveBill inline cancellation with CancelJob call

## Verification

1. `rm -f bin/test_unity.o && make test` — All ~1200 tests should pass
2. Specifically check `test_jobs` for craft job cancellation tests
3. `make path` — Build the game to verify no compilation errors
