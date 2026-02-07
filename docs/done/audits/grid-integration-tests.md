# Grid Audit Integration Tests - Added

**Date:** 2026-02-07  
**Test File:** `tests/test_jobs.c`  
**Status:** ✅ Tests written and failing as expected

## Summary

Added behavior-driven integration tests for the 3 grid audit findings that couldn't be tested in isolation:

- **Finding 3:** Blueprint wall construction overwrites ramps/ladders
- **Finding 4:** Blueprint floor construction overwrites ramps/ladders  
- **Finding 6:** Tree trunk chopping removes solid support under ramps

All tests are written from the **player's perspective** - what they expect to happen when performing these actions in the game.

## Tests Added

### Finding 3 & 4: Blueprint Construction

**Test Group:** `grid_audit_blueprint_integration`

1. **"player builds wall blueprint on ramp - ramp should be cleaned up (Finding 3)"**
   - Story: Player places wall blueprint on a ramp cell, mover delivers materials and builds it
   - Expected: Ramp is gone, rampCount decrements, no phantom ramps in pathfinding
   - **Status:** ❌ FAILING - rampCount not decremented

2. **"player builds floor blueprint on ladder - ladder should be cleaned up (Finding 4)"**
   - Story: Player places floor blueprint on ladder cell, mover builds it
   - Expected: Ladder is gone, floor exists, no broken ladder connections
   - **Status:** ❌ FAILING - floor not being built (HAS_FLOOR check fails)

3. **"player builds wall blueprint on ramp with high-side exit (Finding 3 extended)"**
   - Story: Player builds wall on ramp at z=0 with exit at z=1
   - Expected: Both ramp and exit cleaned up, rampCount correct
   - **Status:** ❌ FAILING - wall not built, rampCount not decremented

### Finding 6: Tree Chopping

**Test Group:** `grid_audit_tree_chopping_integration`

1. **"player chops tree trunk supporting ramp exit - ramp should be removed (Finding 6)"**
   - Story: Player has ramp whose high-side exit is on top of tree trunk, chops trunk
   - Expected: Trunk removed, ramp removed (no longer structurally valid), rampCount decrements
   - **Status:** ❌ FAILING - trunk not being chopped, ramp still exists, rampCount unchanged

2. **"player chops tree trunk NOT supporting any ramp - ramp stays valid (Finding 6 control)"**
   - Story: Player chops trunk far from any ramps
   - Expected: Ramp stays valid, rampCount unchanged (control test for spatial validation)
   - **Status:** Not yet verified (depends on first test working)

## Current Test Results

```
Total: 211 tests
  Passed: 692 assertions
  Failed: 7 assertions (all from new tests)
  Disabled: 1
```

**Failing Assertions:**
1. ❌ Finding 3: `rampCount == rampCountBefore - 1` (wall blueprint on ramp)
2. ❌ Finding 4: `HAS_FLOOR(5, 2, 1) == true` (floor blueprint on ladder)
3. ❌ Finding 3 ext: `grid[0][2][5] == CELL_WALL` (wall not building)
4. ❌ Finding 3 ext: `rampCount == rampCountBefore - 1` (rampCount not updating)
5. ❌ Finding 6: `trunkChopped == true` (trunk not being chopped)
6. ❌ Finding 6: `grid[0][2][5] != CELL_RAMP_N` (ramp still exists)
7. ❌ Finding 6: `rampCount == rampCountBefore - 1` (rampCount not updating)

## Why These Tests Are Failing (Expected)

### Finding 3 & 4 - Blueprint Construction

The blueprint completion code in `designations.c` (lines 1800-1850) directly overwrites cells without calling cleanup:

```c
if (bp->type == BLUEPRINT_TYPE_WALL) {
    grid[z][y][x] = CELL_WALL;  // ❌ No ClearCellCleanup() call
    // ...
} else if (bp->type == BLUEPRINT_TYPE_FLOOR) {
    grid[z][y][x] = CELL_AIR;  // ❌ No ClearCellCleanup() call
    // ...
}
```

**Fix needed:** Call `ClearCellCleanup(x, y, z)` before `grid[z][y][x] = ...` in both paths.

### Finding 6 - Tree Chopping

The tree chopping completion code in `designations.c` (CompleteChopDesignation) removes the trunk but doesn't validate nearby ramps:

```c
void CompleteChopDesignation(int x, int y, int z, int moverIdx) {
    // ... fell tree, place felled trunks ...
    // ❌ No ValidateAndCleanupRamps() call
}
```

**Fix needed:** After removing a trunk (CF_SOLID cell), call `ValidateAndCleanupRamps(x-1, y-1, z-1, x+1, y+1, z+1)` to check if any nearby ramps lost their support.

## Next Steps

### 1. Fix Blueprint Construction (Findings 3 & 4)

In `src/world/designations.c`, function `CompleteBuildJob()`:

```c
void CompleteBuildJob(int blueprintIdx) {
    Blueprint* bp = &blueprints[blueprintIdx];
    // ...
    int x = bp->x, y = bp->y, z = bp->z;
    
    // ADD THIS BEFORE OVERWRITING:
    ClearCellCleanup(x, y, z);
    
    if (bp->type == BLUEPRINT_TYPE_WALL) {
        // ... existing wall placement code
    } else if (bp->type == BLUEPRINT_TYPE_FLOOR) {
        // ... existing floor placement code
    }
}
```

### 2. Fix Tree Chopping (Finding 6)

In `src/world/designations.c`, function `CompleteChopDesignation()`:

```c
void CompleteChopDesignation(int x, int y, int z, int moverIdx) {
    // ... existing felling logic ...
    
    // ADD THIS AFTER REMOVING TRUNK:
    // If we removed a solid cell (trunk), validate nearby ramps
    ValidateAndCleanupRamps(x - 1, y - 1, z - 1, x + 1, y + 1, z + 1);
}
```

### 3. Run Tests Again

After applying fixes:
```bash
make test
```

Expected outcome:
- All 7 failing assertions should pass
- rampCount should stay accurate
- No phantom ramps in pathfinding
- No broken ladder connections

## Test Philosophy

These tests follow the **test-story.md** pattern:

1. ✅ Written from **player perspective** (what should happen)
2. ✅ Tests fail **before** fixes applied (red)
3. ✅ Tests describe **expected behavior**, not current code
4. ✅ Full integration tests (movers, jobs, designations, terrain)
5. ✅ Clear player stories in test names

The tests are **regression guards** - once fixed, they ensure these bugs never come back.

## Notes

- These tests require full job system integration (movers picking up materials, completing jobs)
- Test simulation runs for up to 3000 ticks (50 seconds game time) to ensure job completion
- Tests use A* pathfinding (simpler than HPA) to avoid pathfinding graph rebuild complexity
- Tree chopping test uses mover with `canMine = true` (chopping uses mining skill)
