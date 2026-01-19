# Line-of-Sight Mismatch Bug

## Status: FIXED

The bug has been fixed by expanding the search bounds in `ReconstructLocalPath()`. See "The Fix" section below.

## Summary

Movers were getting stuck (yellow/needsRepath) on valid paths because:
1. `ReconstructLocalPath()` used bounds that were too restrictive
2. This caused paths to start from the wrong position (skipping the first segment)
3. The LOS check would then reject paths because movers were far from their waypoints

## Root Cause

When HPA* refines an abstract path into cell-level waypoints, it calls `ReconstructLocalPath()` for each segment. This function was constraining its A* search to the union of the start/goal chunk bounds.

**The problem:** In dungeon terrain, rooms often span chunk boundaries. A mover spawning in a room might need to reach an entrance on the chunk boundary, but the path between them could require going through an adjacent chunk (e.g., through a corridor).

With restricted bounds, `ReconstructLocalPath()` failed to find valid paths, causing:
1. **INIT MISMATCH bug** - Paths started at the wrong position (14-28 cells away from the mover)
2. **Oscillating yellow movers** - The LOS check rejected paths because movers were nowhere near their waypoints

## The Fix

In `pathfinding.c`, `ReconstructLocalPath()` now uses a two-phase approach:

1. **Try narrow bounds first** (fast path for most cases)
2. **If no path found, expand bounds by one chunk in all directions**

```c
// Try narrow bounds first (fast path for most cases)
int len = ReconstructLocalPathWithBounds(sx, sy, gx, gy, minX, minY, maxX, maxY, outPath, maxLen);
if (len > 0) return len;

// Narrow search failed - expand bounds by one chunk in all directions
int expandedMinX = minX - chunkWidth;
int expandedMinY = minY - chunkHeight;
int expandedMaxX = maxX + chunkWidth;
int expandedMaxY = maxY + chunkHeight;
// ... clamp to grid and retry
```

This keeps the common case fast while handling edge cases near chunk boundaries.

## Additional Fix

Movers with `needsRepath = true` now wait in place instead of continuing to walk on stale paths:

```c
// Don't move movers that are waiting for a repath - they'd walk on stale paths
if (m->needsRepath) continue;
```

## Original Problem Description (Historical)

### Two Different Corner-Cutting Rules

**1. Pathfinder (pathfinding.c)** - Uses 8-directional A* with this corner-cutting check:
```c
// Prevent corner cutting for diagonal movement
if (use8Dir && dx[i] != 0 && dy[i] != 0) {
    if (grid[bestY][adjX] == CELL_WALL || grid[adjY][bestX] == CELL_WALL)
        continue;
}
```

**2. Mover LOS check (mover.c)** - Uses Bresenham line-of-sight with corner checking.

These were originally thought to be the cause of the mismatch, but the real issue was the restricted search bounds in path refinement.

## Why The LOS Check Exists

The LOS check in `UpdateMovers()` detects **dynamic wall changes** - if a wall is drawn across a mover's path while it's walking, the LOS check triggers a repath.

```c
// Check line-of-sight to next waypoint (lenient - also checks from neighbors)
if (!HasLineOfSightLenient(currentX, currentY, target.x, target.y)) {
    m->needsRepath = true;
    continue;
}
```

This check is now working correctly with the path refinement fix.

## Files Involved

- `pathing/pathfinding.c` - `ReconstructLocalPath()`, `ReconstructLocalPathWithBounds()`
- `pathing/mover.c` - `HasLineOfSight()`, `HasLineOfSightLenient()`, `UpdateMovers()`
- `tests/test_mover.c` - `line_of_sight_repath` test

## Test Status

All tests pass:
- `line_of_sight_repath` - "should trigger repath when wall blocks path to next waypoint" ✓
- `string_pulling_narrow_gaps` - Movers no longer get stuck ✓
