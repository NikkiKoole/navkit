# Remove Legacy Walkability Mode

## Overview

The codebase currently supports two walkability models controlled by `g_legacyWalkability`:

- **Legacy mode** (`g_legacyWalkability = true`): You stand INSIDE walkable cells (CELL_FLOOR, CELL_WALKABLE)
- **Standard mode** (`g_legacyWalkability = false`): You stand ON TOP of solid cells (CELL_AIR above CELL_WALL/CELL_DIRT)

Standard mode is more powerful and is the default. Legacy mode adds complexity without benefit.

## Why Remove Legacy Mode

1. **39 conditional branches** across 10 source files
2. Every new feature must consider both modes
3. Conceptually different models cause subtle bugs
4. Standard mode has more features (constructed floors, proper ramps, depth rendering)
5. ~200 lines of code could be removed

## What Would Be Removed

### Code Changes

**Files affected:**
- `src/world/cell_defs.h` - 7 conditions (core walkability functions)
- `src/world/designations.c` - 8 conditions (floor/ramp removal, channeling)
- `src/world/grid.c` - 4 conditions (initialization, ramp cleanup)
- `src/world/terrain.c` - 7 conditions (terrain generation)
- `src/core/input.c` - 4 conditions (painting, F7 toggle)
- `src/render/rendering.c` - 4 conditions (depth tints, fire rendering)
- `src/render/ui_panels.c` - 3 conditions (FillGroundLevel calls)
- `src/entities/jobs.c` - 3 conditions (floor validation)
- `src/world/pathfinding.c` - 2 conditions (ladder link optimization)
- `src/simulation/fire.c` - 1 condition (z-level adjustment)

**Variables to delete:**
- `bool g_legacyWalkability` in `grid.c`
- `extern bool g_legacyWalkability` in `cell_defs.h`

**Cell types to consider removing:**
- `CELL_LADDER_UP`, `CELL_LADDER_DOWN`, `CELL_LADDER_BOTH` (legacy directional ladders)
- `CELL_WALKABLE` (alias, could keep as CELL_GRASS alias)

### Test Changes

**19 tests explicitly use legacy mode:**

In `test_jobs.c` (11 tests):
- "should retry unreachable item after cooldown expires"
- "should return CELL_AIR when no walkable exit at z+1"
- "should assign channel job to mover"
- "should complete channel job - floor removed after execution"
- "should create ramp when wall adjacent at z-1"
- "should channel into open air - floor removed"
- "should move channeler down to z-1 after completion"
- "should complete mine job via driver"
- "should complete mine job faster at higher game speed"
- "should complete build job faster at higher game speed"

In `test_mover.c` (8 tests):
- 1 chunk boundary test
- 2 falling tests  
- 5 ladder transition tests

In `test_pathfinding.c`:
- Runs entire suite twice (both modes) - would run once

**What test rewrites involve:**

Currently legacy mode tests do:
```c
g_legacyWalkability = true;
for (int x = 0; x < 5; x++) {
    for (int y = 0; y < 5; y++) {
        grid[0][y][x] = CELL_WALL;
        grid[1][y][x] = CELL_FLOOR;  // Simple: just set CELL_FLOOR
    }
}
```

Standard mode equivalent:
```c
for (int x = 0; x < 5; x++) {
    for (int y = 0; y < 5; y++) {
        grid[0][y][x] = CELL_WALL;   // Solid ground
        grid[1][y][x] = CELL_AIR;    // Air above
        SET_FLOOR(x, y, 1);          // Floor flag for walkability
    }
}
```

The main difference: standard mode requires explicit `SET_FLOOR()` calls or solid cells below for walkability.

## Migration Steps

1. **Update tests first** - Rewrite the 19 tests to use standard mode grid setup
2. **Remove conditionals** - Go through each file and remove `if (g_legacyWalkability)` branches, keeping standard mode code
3. **Remove variable** - Delete `g_legacyWalkability` declaration and extern
4. **Remove F7 toggle** - Remove the debug key that switches modes
5. **Clean up cell types** - Decide whether to keep/remove legacy ladder types
6. **Update documentation** - Remove dual-mode explanations from `cell_defs.h`

## Features Only in Standard Mode (Will Become Default)

- Constructed floors via `HAS_FLOOR` flag (balconies, bridges)
- `DESIGNATION_REMOVE_FLOOR` for floor removal
- Implicit bedrock at z=0
- Wall-top avoidance (prevents spawning on precarious edges)
- Advanced ramp auto-detection
- Depth tinting in rendering

## Estimated Effort

- Test rewrites: ~2 hours (19 tests, mostly mechanical changes)
- Code removal: ~1-2 hours (remove conditionals, straightforward)
- Testing & validation: ~1 hour
- **Total: ~4-5 hours**

## Open Questions

1. Should `CELL_WALKABLE` be kept as an alias for `CELL_GRASS`?
2. Should the directional ladder types (`CELL_LADDER_UP/DOWN/BOTH`) be removed entirely or kept for potential future use?
3. Should `test_pathfinding.c` still run twice (for regression testing) or just once?
