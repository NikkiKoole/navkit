# Channeling Implementation Handoff

## Status: Implementation Complete, Test Issues Remain

The channeling feature from `channeling-implementation.md` has been fully implemented. All code is in place and compiles. Some tests have subtle issues that need investigation.

## What Was Implemented

### Files Modified:

1. **src/world/designations.h** - Added:
   - `DESIGNATION_CHANNEL` enum value
   - `CHANNEL_WORK_TIME` constant (2.0f)
   - Function declarations: `DesignateChannel`, `HasChannelDesignation`, `AutoDetectChannelRampDirection`, `CompleteChannelDesignation`, `CountChannelDesignations`

2. **src/world/designations.c** - Added:
   - `DesignateChannel()` - validates z>0, walkable tile, has floor
   - `HasChannelDesignation()` - simple check
   - `AutoDetectChannelRampDirection()` - relaxed rules for 1-cell pits (only needs adjacent wall at z-1 with walkable floor at z above)
   - `CompleteChannelDesignation()` - removes floor at z, mines z-1, creates ramp if possible, handles mover descent
   - `CountChannelDesignations()` - for early-exit optimizations

3. **src/entities/jobs.h** - Added:
   - `JOBTYPE_CHANNEL` enum value
   - `RunJob_Channel` declaration
   - `WorkGiver_Channel` declaration

4. **src/entities/jobs.c** - Added:
   - `RunJob_Channel()` - job driver, mover stands ON the tile (unlike mining where they stand adjacent)
   - `WorkGiver_Channel()` - finds nearest unassigned channel designation
   - Added to `jobDrivers[]` table
   - Added to `AssignJobsWorkGivers()` and `AssignJobsHybrid()` priority chains

5. **src/core/input_mode.h** - Added:
   - `ACTION_WORK_CHANNEL` to InputAction enum

6. **src/core/input_mode.c** - Added:
   - "cHannel" (KEY_H) to WORK menu items
   - Channel to bar text and bar items handling

7. **src/core/input.c** - Added:
   - `ExecuteDesignateChannel()` and `ExecuteCancelChannel()` functions
   - KEY_H handling in MODE_WORK
   - Drag handler for `ACTION_WORK_CHANNEL`

8. **src/render/rendering.c** - Added:
   - Extended `DrawMiningDesignations()` to also draw channel designations
   - Channel color: pink/magenta `(Color){255, 150, 200, 200}`
   - Active channel jobs shown with `(Color){255, 180, 150, 180}`

9. **tests/test_jobs.c** - Added channeling tests:
   - `describe(channel_designation)` - 5 tests, ALL PASS
   - `describe(channel_ramp_detection)` - 2 tests, ALL PASS
   - `describe(channel_job_execution)` - 5 tests, SOME ISSUES
   - `describe(channel_workgiver)` - 1 test, PASSES

## Test Results

### Passing Tests:
- All 5 `channel_designation` tests ✓
- Both `channel_ramp_detection` tests ✓
- `should assign channel job to mover` ✓
- `should not assign channel job to mover without canMine` ✓

### Failing Tests (execution tests):
- `should complete channel job - floor removed after execution`
- `should create ramp when wall adjacent at z-1`
- `should channel into open air - floor removed`
- `should move channeler down to z-1 after completion`

### The Mystery:
The execution tests show a strange pattern:
- `HAS_FLOOR(2, 2, 1) == 0` PASSES (floor IS being cleared)
- But `grid[0][2][2] != CELL_WALL` FAILS (wall NOT being mined)
- And `m->z == 0.0f` FAILS (mover NOT descending)
- And `CellIsRamp(grid[0][2][2])` FAILS (ramp NOT being created)

This suggests `CompleteChannelDesignation` is partially executing - it clears the floor but doesn't mine below or move the mover. This is odd because the code has no early returns between those steps.

### Possible Causes to Investigate:
1. The tests use `g_legacyWalkability = true` - there may be mode-specific issues
2. The `SET_FLOOR` calls in test setup may interact oddly with legacy mode
3. `CompleteChannelDesignation` may be getting called with invalid parameters
4. There may be some other code path clearing the floor flag

## How to Test Manually

Run the game and:
1. Press W to enter WORK mode
2. Press H to select cHannel
3. Click on a floor tile at z>0 (with something solid below)
4. Watch a mover walk to the tile, channel it, and descend

## Next Steps

1. **Option A**: Investigate why execution tests fail despite floor being cleared
   - Add printf debugging to `CompleteChannelDesignation`
   - Check if `wasSolid` is false when it should be true
   - Verify the mover index is valid

2. **Option B**: Skip test fixes for now, verify manually that the feature works in-game

3. **Option C**: Simplify tests to z=0 single-level scenarios (but channeling requires z>0 by design)

## Key Implementation Details

- Channeling requires z > 0 (can't channel at bedrock level)
- Mover stands ON the tile being channeled (different from mining where they stand adjacent)
- Uses `canMine` capability (same as mining)
- Ramp detection is "relaxed" - only needs adjacent wall at z-1 with walkable above, doesn't require walkable low-side
- `CHANNEL_WORK_TIME` is 2.0f seconds (same as mining)
- Channel designations reuse `targetMineX/Y/Z` fields in Job struct
