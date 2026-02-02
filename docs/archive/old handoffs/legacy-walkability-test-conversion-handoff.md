# Legacy Walkability Test Conversion - Handoff

## Summary

We've been converting tests that explicitly use `g_legacyWalkability = true` to be mode-agnostic, so they pass in both legacy and standard (DF-style) walkability modes. This is preparation for eventually removing legacy mode entirely.

## Status: COMPLETE

All tests in `test_jobs.c` and `test_mover.c` have been converted to be mode-agnostic. No tests explicitly set `g_legacyWalkability = true` anymore.

## What Was Done

### Tests Converted (21 total)

**test_jobs.c (12 tests):**
1. "should retry unreachable item after cooldown expires" - removed legacy flag, use conditional cell type when opening walls
2. "should complete mine job and convert wall to walkable" - check `IsCellWalkableAt()` instead of `CELL_FLOOR`
3. "should process multiple mine designations sequentially" - check `IsCellWalkableAt()` instead of `CELL_FLOOR`
4. "should return CELL_AIR when no walkable exit at z+1" - conditional z-levels per mode
5. "should assign channel job to mover" - conditional z-levels (channelZ, moverZ)
6. "should complete channel job - floor removed after execution" - conditional z-levels
7. "should create ramp when wall adjacent at z-1" - conditional z-levels (channelZ, rampZ)
8. "should channel into open air - floor removed" - conditional z-levels
9. "should move channeler down to z-1 after completion" - conditional z-levels (channelZ, descendZ)
10. "should complete mine job via driver" - conditional z-levels (mineZ)
11. "should complete mine job faster at higher game speed" - conditional z-levels (mineZ)
12. "should complete build job faster at higher game speed" - conditional z-levels (buildZ)

**test_mover.c (9 tests):**
1. "should find paths when room entrance is in adjacent chunk" - check `IsCellWalkableAt()` instead of `CELL_WALKABLE`
2. "should fall to ground when placed in air" - conditional grid setup per mode
3. "should not fall when walking on walkable cells" - conditional grid setup per mode
4. "should stop falling when hitting a wall below" - conditional grid setup per mode
5. "should transition z-level when reaching ladder waypoint" - use `startZ`/`goalZ` variables based on mode
6. "should climb ladder when path goes through ladder cell" - use `startZ`/`goalZ` variables based on mode
7. "should handle JPS+ 3D path through Labyrinth3D" - use `baseZ` offset for z-levels
8. "JPS+ 3D path should match JPS 3D path structure" - use `baseZ` offset for z-levels
9. "should handle multiple random cross-z paths with JPS+" - removed legacy flag (uses `GenerateLabyrinth3D` which handles both modes)

### Patterns Used

**Pattern 1: Check walkability instead of cell type**
```c
// Before (legacy-specific)
expect(grid[0][1][1] == CELL_FLOOR);

// After (mode-agnostic)
expect(IsCellWalkableAt(0, 1, 1) == true);
```

**Pattern 2: Conditional cell type when setting cells**
```c
// When opening a wall or setting walkable ground
grid[0][3][2] = g_legacyWalkability ? CELL_WALKABLE : CELL_AIR;
```

**Pattern 3: Conditional grid setup for multi-z tests**
```c
if (g_legacyWalkability) {
    grid[0][y][x] = CELL_WALKABLE;
    grid[1][y][x] = CELL_WALKABLE;
} else {
    grid[0][y][x] = CELL_DIRT;      // Solid ground
    grid[1][y][x] = CELL_AIR;       // Walkable (above solid)
}
```

**Pattern 4: Use baseZ offset for generated terrain**
```c
// GenerateLabyrinth3D uses baseZ=0 in legacy, baseZ=1 in standard
int baseZ = g_legacyWalkability ? 0 : 1;
int lowerZ = baseZ;
int upperZ = baseZ + 1;
// Then use lowerZ/upperZ instead of hardcoded 0/1
```

**Pattern 5: Use SET_FLOOR for constructed floors in standard mode**
```c
if (!g_legacyWalkability) {
    grid[2][y][x] = CELL_AIR;
    SET_FLOOR(x, y, 2);  // Makes z=2 walkable without solid below
}
```

**Pattern 6: Conditional z-levels for channeling tests**
```c
int channelZ;   // Z-level where channeling happens
int belowZ;     // Z-level below the channel

if (g_legacyWalkability) {
    channelZ = 1;
    belowZ = 0;
    // Legacy: z=0 walls, z=1 floor
} else {
    channelZ = 2;
    belowZ = 1;
    // Standard: z=0 walls, z=1 walls, z=2 air + floor flag
}
```

## Key Insight for Channeling Tests

The channeling behavior differs between modes:
- **Legacy**: Walk at z=0 (CELL_FLOOR), channel at z=1 removes floor, mines z=0 below
- **Standard**: Walk at z=1 (above solid z=0), or z=2 (above solid z=1). Channel removes floor flag, mines solid below

The pattern is to use variables like `channelZ`, `moverZ`, `belowZ`, `descendZ` that are set based on `g_legacyWalkability`, then use those throughout the test.

## Files Modified

- `tests/test_jobs.c` - 12 tests converted
- `tests/test_mover.c` - 9 tests converted

## Next Steps

With all tests now mode-agnostic, the next step is to actually remove legacy walkability mode:
- See `/docs/ideas/plans/remove-legacy-walkability.md` for the full plan

## Running Tests

```bash
# Standard mode (default)
./bin/test_jobs
./bin/test_mover

# Legacy mode
./bin/test_jobs --legacy
./bin/test_mover --legacy

# Or use Makefile targets
make test_jobs
make test_mover
```

All tests pass in both modes (verified 2026-02-02).
