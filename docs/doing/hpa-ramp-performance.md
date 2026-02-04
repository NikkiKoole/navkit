# HPA* Cross-Z Performance Fix

## Problem Statement

With 1683 movers on a 256x256x16 hilly terrain map, the game experiences **multi-second hangs** caused by pathfinding.

## Verified Root Cause

### The Smoking Gun

Console logs during spikes show:
```
WARNING: SLOW A* fallback: mover 127, 611.1ms, start(187,84,z9)->goal(247,124,z7), len=63
WARNING: SLOW A* fallback: mover 1476, 451.8ms, start(162,56,z9)->goal(75,55,z7), len=89
WARNING: SLOW A* fallback: mover 1121, 1660.5ms, start(109,61,z9)->goal(112,215,z8), len=155
```

**A* fallback is taking 450ms to 1.6 SECONDS per path** for cross-z queries.

### Why This Happens

1. **HPA* fails for cross-z paths** - `ReconstructLocalPath()` at line 1888 bails when `sz != gz`
2. **A* fallback triggers** - code at mover.c:1242-1246 calls full A* when HPA* returns 0
3. **A* searches entire 3D grid** - 256×256×16 = 1,048,576 cells
4. **Catastrophic slowdown** - even finding a valid path takes 0.5-1.6 seconds

### Key Data Points

From headless testing (600 ticks):
- **HPA* success rate: 98.9%** (922 successes)
- **A* fallback rate: 1.1%** (10 fallbacks)
- **Average tick time: 27ms** (acceptable)

But those 1.1% of fallbacks cause the spikes because each one can take **seconds**.

### The Core Bug

`ReconstructLocalPath()` in pathfinding.c:1888:
```c
static int ReconstructLocalPath(...) {
    if (sz != gz) return 0;  // <-- PROBLEM: Can't handle cross-z
    ...
}
```

This function is used for:
1. Same-chunk local paths (line 1947)
2. Refining abstract path segments (line 2189)

When a path segment crosses z-levels, it returns 0, which eventually causes HPA* to fail.

---

## The Fix

### Why Full A* Fallback is Wrong

The A* fallback searches the **entire** 256×256×16 grid. But we only need to search a **local area** around the ramp/ladder connecting the z-levels.

### Solution: Bounded 3D Local Search

Create `ReconstructLocalPathCrossZ()` that:
1. Uses bounded 3D A* (chunk + 1 neighbor = ~32×32×2 cells max)
2. Handles ramp up/down transitions (copy logic from RunAStar lines 1688-1759)
3. Handles ladder up/down transitions

**Search space comparison:**
- Current A* fallback: 1,048,576 cells (256×256×16)
- New local 3D search: ~2,048 cells (32×32×2)
- **500x smaller search space**

---

## Implementation Plan

### Step 1: Add `ReconstructLocalPathCrossZ()`

New function in pathfinding.c that:
- Takes start (sx,sy,sz) and goal (gx,gy,gz) with different z-levels
- Computes expanded bounds covering both z-levels
- Runs bounded 3D A* with:
  - Same-level XY movement (existing logic)
  - Ramp up/down transitions (from RunAStar)
  - Ladder up/down transitions (from RunAStar)
- Returns path length, or 0 if no path found in bounds

### Step 2: Integrate into `ReconstructLocalPath()`

```c
static int ReconstructLocalPath(int sx, int sy, int sz, int gx, int gy, int gz, 
                                Point* outPath, int maxLen) {
    if (sz != gz) {
        return ReconstructLocalPathCrossZ(sx, sy, sz, gx, gy, gz, outPath, maxLen);
    }
    // existing 2D code unchanged
    ...
}
```

### Step 3: Test

- Verify existing pathfinding tests still pass
- Verify the two known failing tests (lines 2407, 2472) now pass
- Run stress test with save file - no more multi-second spikes

### Step 4: Remove A* Fallback (Optional)

Once HPA* handles cross-z properly, the A* fallback in mover.c should rarely/never trigger. Could remove it or keep as safety net with a timeout.

---

## Files to Modify

| File | Changes |
|------|---------|
| `src/world/pathfinding.c` | Add `ReconstructLocalPathCrossZ()`, modify `ReconstructLocalPath()` |
| `tests/test_pathfinding.c` | Verify tests pass |

---

## Code to Copy From RunAStar

The ramp/ladder handling logic (pathfinding.c lines 1655-1759):

**Ladder up/down:**
```c
if (CanClimbUp(bestX, bestY, bestZ)) {
    // transition to (bestX, bestY, bestZ+1)
}
if (CanClimbDown(bestX, bestY, bestZ)) {
    // transition to (bestX, bestY, bestZ-1)
}
```

**Ramp up:**
```c
if (CanWalkUpRampAt(bestX, bestY, bestZ)) {
    GetRampHighSideOffset(grid[bestZ][bestY][bestX], &highDx, &highDy);
    int exitX = bestX + highDx;
    int exitY = bestY + highDy;
    int exitZ = bestZ + 1;
    // transition to (exitX, exitY, exitZ)
}
```

**Ramp down:**
```c
// Check 4 adjacent cells at z-1 for ramps pointing to current position
int checkOffsets[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
CellType matchingRamps[4] = {CELL_RAMP_S, CELL_RAMP_W, CELL_RAMP_N, CELL_RAMP_E};
for (int i = 0; i < 4; i++) {
    int rampX = bestX + checkOffsets[i][0];
    int rampY = bestY + checkOffsets[i][1];
    int rampZ = bestZ - 1;
    if (grid[rampZ][rampY][rampX] == matchingRamps[i]) {
        // transition to (rampX, rampY, rampZ)
    }
}
```

---

## Success Criteria

1. No more multi-second spikes in stress test
2. All existing pathfinding tests pass
3. Known failing tests (lines 2407, 2472) pass
4. Headless 600-tick test: < 30ms/tick average (currently 27ms)

---

## Questions Resolved

1. **Heap vs linear scan?** Use existing `ChunkHeap` infrastructure - it's already there and fast
2. **How far to expand bounds?** Start chunk + goal chunk + 1 cell border for ramp exits
3. **CanEnterRampFromSide checks?** Yes, copy from RunAStar for correctness
