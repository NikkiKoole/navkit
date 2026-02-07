# Grid.md Audit - Test Coverage Summary

**Test file:** `tests/test_grid_audit.c`  
**Date:** 2026-02-07

## Test Coverage by Finding

| Finding | Severity | Testable? | Test Written? | Status |
|---------|----------|-----------|---------------|--------|
| 1. PlaceCellFull overwrites ramps/ladders | Medium | ✅ Yes | ✅ Yes | ❌ FAILING |
| 2. Quick-edit erase skips ramp cleanup | Medium | ✅ Yes | ✅ Yes | ❌ FAILING |
| 3. Blueprint wall overwrites ramps/ladders | Medium | ⚠️ Complex | ❌ No | N/A |
| 4. Blueprint floor overwrites ramps/ladders | Medium | ⚠️ Complex | ❌ No | N/A |
| 5. Fire burns solid support without validation | **High** | ✅ Yes | ✅ Yes | ❌ FAILING |
| 6. Tree decay (trunk chopping) | Low | ⚠️ Complex | ❌ No | N/A |
| 7. PlaceLadder on ramp destroys ramp | Medium | ✅ Yes | ✅ Yes | ❌ FAILING |
| 8. EraseRamp doesn't dirty exit chunk | Medium | ✅ Yes | ✅ Yes | ✅ PASSING |
| 9. CanPlaceRamp allows map-edge placement | Low | ✅ Yes | ✅ Yes | ✅ PASSING |
| 10. IsRampStillValid ignores low-side | Low | ✅ Yes | ✅ Yes | ❌ FAILING |
| 11. hpaNeedsRebuild not set in init | Low | ✅ Yes | ✅ Yes | ✅ PASSING |
| 12. Entrance scan past grid boundary | Low | ❌ No impact | ❌ No | N/A |

## Summary Statistics

- **Total Findings:** 12
- **Directly Testable:** 8
- **Tests Written:** 8
- **Currently Failing:** 5 (Findings 1, 2, 5, 7, 10)
- **Currently Passing:** 3 (Findings 8, 9, 11)
- **Requires Complex Setup:** 3 (Findings 3, 4, 6 - job system + blueprints)
- **Performance-only (No Test):** 1 (Finding 12)

## Test Details

### ✅ Tests Written (8 findings)

**HIGH Priority:**
- **Finding 5** (fire + ramp support): Tests that fire burning a tree trunk supporting a ramp's exit triggers ramp validation

**MEDIUM Priority:**
- **Finding 1** (PlaceCellFull): Tests that placing walls over ramps decrements rampCount
- **Finding 2** (quick-edit erase): Tests that erasing ramps via quick-edit path properly decrements rampCount
- **Finding 7** (PlaceLadder): Tests that placing ladder on ramp cell removes the ramp and decrements rampCount
- **Finding 8** (cross-chunk dirty): Tests that erasing ramps at chunk boundaries dirties exit chunks

**LOW Priority:**
- **Finding 9** (map-edge ramps): Tests that CanPlaceRamp rejects ramps with off-map entry points
- **Finding 10** (low-side validity): Documents that IsRampStillValid only checks high-side (structural) support
- **Finding 11** (init flags): Tests that grid initialization properly sets rebuild flags

### ⚠️ Complex Setup Required (not yet tested)

**Finding 3 & 4** (blueprint construction):
- Require full job system: movers, items, stockpiles, blueprints, job assignment
- Recommended: add these tests to `test_jobs.c` in the blueprint test section
- Test story: "player queues wall blueprint on ramp cell, mover completes construction, rampCount should decrement"

**Finding 6** (tree trunk chopping):
- Requires tree system + designation system integration
- Similar to Finding 5 but via chopping rather than fire
- May already be covered by tree tests

### ❌ Not Testable

**Finding 12** (entrance scan boundary):
- Performance-only issue, no observable behavior change
- Bounds-checking prevents bugs, just wastes cycles

## Current Test Results

```
Total: 10 tests
	Passed: 10 expectations  
	Failed: 10 expectations
```

### Failing Tests (Expected)

These tests fail because the bugs haven't been fixed yet:

1. **Finding 1**: rampCount not decremented when PlaceCellFull overwrites ramp
2. **Finding 2**: rampCount not decremented when quick-edit erases ramp
3. **Finding 5**: Fire doesn't call ValidateAndCleanupRamps after burning trunk
4. **Finding 7**: PlaceLadder doesn't check for/erase existing ramps
5. **Finding 10**: IsRampStillValid returns true initially (test setup issue)

### Passing Tests (Already Working or Design Choice)

1. **Finding 8**: Cross-chunk dirty propagation works correctly
2. **Finding 9**: CanPlaceRamp already rejects map-edge placements (or is Low priority design choice)
3. **Finding 11**: Grid init correctly sets flags (or flag check test is too simple)

## Next Steps

1. **Fix the failing tests** (Findings 1, 2, 5, 7, 10)
2. **Add blueprint tests** to test_jobs.c for Findings 3 & 4
3. **Verify Finding 6** against existing tree/chopping tests
4. **Run full test suite** to ensure fixes don't break anything

## Recommended Fix Order

1. **Create `ClearCellCleanup(x,y,z)` helper** - fixes Findings 1, 2, 7 in one shot
2. **Fix Finding 5** - add `ValidateAndCleanupRamps` call in fire.c after burning
3. **Fix Finding 10** - document low-side check as design choice (or add warning)
4. **Add blueprint tests** - integrate into test_jobs.c for Findings 3 & 4
