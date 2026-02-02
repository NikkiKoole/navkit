# Test Failures Handoff

## Summary

There are 10 failing tests in `test_jobs.c` related to channeling and mining. These appear to be pre-existing issues with the channeling implementation, not caused by recent changes.

## How to Run the Failing Tests

```bash
cd /Users/nikkikoole/Projects/navkit
make test_jobs 2>&1 | grep -E "^\[ \]"
```

## Failing Tests

### channel_ramp_detection
1. `"should return CELL_AIR when no adjacent wall at z-1"` - Tests `AutoDetectChannelRampDirection`

### channel_job_execution
2. `"should complete channel job - floor removed after execution"` - Main channeling test
3. `"should create ramp when wall adjacent at z-1"` - Ramp creation during channel
4. `"should channel into open air - floor removed"` - Channel without ramp
5. `"should move channeler down to z-1 after completion"` - Mover descent after channel

### job_drivers
6. `"should complete mine job via driver: move to adjacent -> mine -> done"` - Mining job completion

### job_game_speed
7-10. Mine job speed tests failing (ratio issues)

## Key Files to Investigate

- `tests/test_jobs.c` - The failing tests (search for test names above)
- `src/world/designations.c` - `CompleteChannelDesignation()`, `AutoDetectChannelRampDirection()`
- `src/entities/jobs.c` - `RunJob_Channel()`, `RunJob_Mine()`, `WorkGiver_Channel()`

## Test Pattern

Most failing tests follow this pattern:
```c
g_legacyWalkability = true;  // Tests use legacy mode

// Setup grid with walls at z=0, floor at z=1
for (int x = 0; x < 5; x++) {
    for (int y = 0; y < 5; y++) {
        grid[0][y][x] = CELL_WALL;
        grid[1][y][x] = CELL_FLOOR;
        SET_FLOOR(x, y, 1);
    }
}

// Create mover at target location
// Designate for channeling
// Run ticks
// Check results
```

## Likely Issues

1. **Mover can't reach target** - The mover may not be finding a path to the channel designation. Check `WorkGiver_Channel()` which requires same z-level.

2. **Job not progressing** - The job might be assigned but not completing. Check `RunJob_Channel()` step progression.

3. **Ramp detection logic** - `AutoDetectChannelRampDirection()` has two passes:
   - First pass: looks for adjacent solid walls
   - Second pass: looks for any walkable exit
   The test expects CELL_AIR when no walls, but second pass might find walkable cells.

4. **Game speed scaling** - Mine job speed tests check that jobs complete faster at 2x speed. The ratio should be ~2:1 but is showing 1:1.

## Recent Changes (for context)

We just added:
- `ValidateAndCleanupRamps()` - called after channeling/mining completion
- `IsRampStillValid()` - checks if ramp's high side has solid support
- Ramp cleanup on save load

These changes are in `src/world/grid.c` and `src/world/designations.c`. The ramp validation is called at the end of `CompleteMineDesignation()` and `CompleteChannelDesignation()`.

## Debugging Tips

Run a single test by adding debug output:
```c
it("should complete channel job...") {
    // Add printf statements to trace execution
    printf("Before: grid[0][2][2] = %d\n", grid[0][2][2]);
    // ... test code ...
    printf("After: grid[0][2][2] = %d\n", grid[0][2][2]);
}
```

Check if job is being assigned:
```c
printf("Mover job: %d\n", m->currentJobId);
if (m->currentJobId >= 0) {
    Job* j = GetJob(m->currentJobId);
    printf("Job type: %d, step: %d\n", j->type, j->step);
}
```
