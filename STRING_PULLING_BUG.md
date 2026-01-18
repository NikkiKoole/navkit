# String Pulling Bug: Yellow Stuck Movers

## Summary
When string pulling is enabled, movers get stuck in a yellow "needs repath" state at narrow gaps/chokepoints. Disabling string pulling eliminates the issue.

## Symptoms
- Many movers turn yellow (needsRepath = true) and stop moving
- Yellow movers cluster at chokepoints/narrow passages (1-cell gaps in walls)
- Drawing a wall through stuck movers and removing it "unsticks" them temporarily
- The problem is especially visible on maps with walls aligned to chunk boundaries

## Root Cause Analysis

### The Bug Flow:
1. `StringPullPath()` uses `HasLineOfSight()` to remove unnecessary waypoints
2. At string-pull time, the LOS check passes (barely) for path segments near corners
3. When the mover actually moves, they're at a slightly different sub-cell position
4. The runtime `HasLineOfSight()` check in `UpdateMovers()` fails from the new position
5. Mover gets `needsRepath = true`
6. Repath computes the same path, string pulls it the same way
7. Same LOS failure happens again - mover stays stuck yellow

### Code Locations:

**String pulling (pathfinding/mover.c:44-68):**
```c
// Checks LOS between waypoint cells
if (HasLineOfSight(pathArr[current].x, pathArr[current].y,
                   pathArr[i].x, pathArr[i].y))
```

**Runtime LOS check (pathfinding/mover.c:195-200):**
```c
int currentX = (int)(m->x / CELL_SIZE);
int currentY = (int)(m->y / CELL_SIZE);

// Check line-of-sight to next waypoint
if (!HasLineOfSight(currentX, currentY, target.x, target.y)) {
    m->needsRepath = true;
    continue;
}
```

The mover's actual cell position during movement may differ from the original waypoint where LOS was computed - especially near corners where the path grazes walls.

### The HasLineOfSight Function (pathfinding/mover.c:15-42):
Uses Bresenham line algorithm with diagonal corner-cutting prevention:
```c
// For diagonal movement, check both adjacent cells
if (e2 > -dy && e2 < dx) {
    int nx = x + sx;
    int ny = y + sy;
    if (grid[y][nx] == CELL_WALL || grid[ny][x] == CELL_WALL) {
        return false;
    }
}
```

## Reproduction

1. Use the test map below (32x32 with 8x8 chunks)
2. Enable endless mode
3. Spawn 1000 movers
4. Enable string pulling
5. Observe yellow movers clustering at narrow gaps

**Disabling string pulling eliminates the issue entirely.**

## Test Map

```
........#.......#.......#.......
........#.......#.......#.......
........#.......#.......#.......
................................
........#.......#.......#.......
........#.......#.......#.......
........#.......#.......#.......
#########.#############.#####.##
........#.......#.......#.......
........#.......#.......#.......
........#.......#.......#.......
........#...............#.......
........#.......#.......#.......
........#.......#.......#.......
........#.......#.......#.......
###.#######.##########.####.####
........#.......#.......#.......
........#.......#.......#.......
........#...............#.......
........#.......#.......#.......
................#.......#.......
........#.......#...............
........#.......#.......#.......
#.#########.#######.#########.##
........#.......#.......#.......
........#.......#.......#.......
........#.......#.......#.......
........#.......#.......#.......
........#.......#.......#.......
........#.......#.......#.......
........#.......#.......#.......
........#.......#.......#.......
```

This map has:
- Walls aligned with chunk boundaries (every 8 cells)
- Narrow 1-cell gaps at various positions
- Forces movers to funnel through chokepoints

## Possible Fixes

### Option 1: Conservative String Pulling
Add a margin when checking LOS for string pulling - require LOS from adjacent cells too, not just the exact waypoint.

```c
// Before removing a waypoint, check LOS from neighboring cells too
bool SafeToRemove(int fromX, int fromY, int toX, int toY) {
    // Check from all 4 cardinal neighbors of 'from'
    for (int d = 0; d < 4; d++) {
        int nx = fromX + dx[d];
        int ny = fromY + dy[d];
        if (IsWalkable(nx, ny) && !HasLineOfSight(nx, ny, toX, toY)) {
            return false;  // LOS fails from a neighbor, don't remove waypoint
        }
    }
    return HasLineOfSight(fromX, fromY, toX, toY);
}
```

### Option 2: Skip Runtime LOS Check for String-Pulled Paths
Trust the pre-computed path more. Only do LOS checks for non-string-pulled paths or when the grid changes.

### Option 3: Widen Runtime LOS Check
Before triggering repath, check LOS from neighboring cells too. Only repath if ALL checks fail.

### Option 4: Don't String-Pull Through Narrow Gaps
Detect 1-cell-wide passages and preserve waypoints in those areas.

### Option 5: Use Path Width in String Pulling
Only string-pull if there's a "corridor" of walkable cells along the path, not just a single line.

## Recommended Fix
**Option 1 (Conservative String Pulling)** is likely the cleanest fix - it addresses the root cause without changing runtime behavior or adding complexity to the movement code.

## Related Issues
- This may interact with future collision avoidance - movers pushed by avoidance forces may also fail LOS checks
- The repath queue starvation issue (always starts at index 0) may make this worse with many movers

## Test Added
See `tests/test_mover.c` - `describe(string_pulling_narrow_gaps)` for automated reproduction.
