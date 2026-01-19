# Line-of-Sight Mismatch Bug

## Summary
Movers get stuck (yellow/needsRepath) on valid paths because the runtime LOS check in `UpdateMovers()` rejects diagonal movements that the pathfinder created.

## The Problem

### Two Different Corner-Cutting Rules

**1. Pathfinder (pathfinding.c)** - Uses 8-directional A* with this corner-cutting check:
```c
// Prevent corner cutting for diagonal movement
if (use8Dir && dx[i] != 0 && dy[i] != 0) {
    if (grid[bestY][adjX] == CELL_WALL || grid[adjY][bestX] == CELL_WALL)
        continue;
}
```
This checks the two cells **adjacent to the current position** when moving diagonally.

**2. Mover LOS check (mover.c)** - Uses Bresenham line-of-sight:
```c
// For diagonal movement, check corner cutting
if (e2 > -dy && e2 < dx) {
    // Moving diagonally - check both adjacent cells
    int nx = x + sx;
    int ny = y + sy;
    if (grid[y][nx] == CELL_WALL || grid[ny][x] == CELL_WALL) {
        return false;
    }
}
```
This traces a line using Bresenham's algorithm, checking corners along the way.

### Why They Disagree

The pathfinder checks corners **at each step** as it builds the path. It allows diagonal movement from (5,5) to (6,6) if cells (5,6) and (6,5) are walkable.

But `HasLineOfSight` traces a Bresenham line from the mover's current position to a potentially distant waypoint. The Bresenham algorithm steps through cells differently and may encounter different corner configurations.

**Example scenario:**
- Pathfinder creates path: (5,5) → (6,6) → (7,7) → (8,8)
- With string pulling OFF, mover at (5,5) checks LOS to (6,6) - might pass
- With string pulling ON, mover at (5,5) checks LOS to (8,8) - Bresenham traces a different set of cells and may fail

## When It Happens

- **Terrain:** Dungeon (rooms with corridors), any terrain with tight corners
- **Location:** Room corners where diagonal paths cross near walls
- **Symptoms:** Movers turn yellow (needsRepath=true) immediately after spawning or during movement

## Debug Output Example
```
Mover 2639 stuck: pos=(256,253) target=(273,246) pathIdx=3 centerLOS=0
Mover 2861 stuck: pos=(244,272) target=(254,255) pathIdx=13 centerLOS=0
```
All show `centerLOS=0` meaning even center-to-center LOS fails.

## Why The LOS Check Exists

The LOS check in `UpdateMovers()` was added to detect **dynamic wall changes** - if a wall is drawn across a mover's path while it's walking, the LOS check would trigger a repath.

```c
// Check line-of-sight to next waypoint (lenient - also checks from neighbors)
if (!HasLineOfSightLenient(currentX, currentY, target.x, target.y)) {
    m->needsRepath = true;
    continue;
}
```

## Current Workaround

The LOS check is disabled with `#if 0` in mover.c. Movers now work correctly on all terrains, but they won't detect walls placed ahead of them (only when they step on a wall).

## Test Impact

Disabling the LOS check breaks this test:
```
line_of_sight_repath
    [ ] "should trigger repath when wall blocks path to next waypoint"
```

## Possible Fixes

### Option 1: Remove LOS Check Entirely
- Simplest solution
- Movers trust the pathfinder completely
- Wall detection only happens when mover steps on a wall (already handled)
- **Downside:** Movers won't preemptively repath when walls are placed ahead

### Option 2: Simpler Adjacent-Cell Check
Instead of checking LOS to distant waypoints, just check if the **next cell** the mover would step into is walkable:
```c
// Check if immediate next step is blocked
Point nextWaypoint = m->path[m->pathIndex];
int dx = nextWaypoint.x - currentX;
int dy = nextWaypoint.y - currentY;
// Clamp to single step
dx = (dx > 0) ? 1 : (dx < 0) ? -1 : 0;
dy = (dy > 0) ? 1 : (dy < 0) ? -1 : 0;
int nextX = currentX + dx;
int nextY = currentY + dy;
if (grid[nextY][nextX] == CELL_WALL) {
    m->needsRepath = true;
    continue;
}
```

### Option 3: Fix HasLineOfSight to Match Pathfinder
Make `HasLineOfSight` use the same corner-cutting rules as the pathfinder. This is more complex because Bresenham doesn't naturally align with grid-based A*.

### Option 4: Different Check for String Pulling vs Raw Paths
- With string pulling: Use current LOS check (waypoints are far apart)
- Without string pulling: Use simple adjacent-cell check (waypoints are neighbors)

But this still doesn't solve the fundamental mismatch for string-pulled paths.

### Option 5: Validate Paths at Creation Time
When string pulling creates a path, verify each segment passes `HasLineOfSight`. If not, keep more waypoints. This adds overhead but ensures runtime checks always pass.

## Files Involved

- `pathing/mover.c` - `HasLineOfSight()`, `HasLineOfSightLenient()`, `HasClearCorridor()`, `UpdateMovers()`
- `pathing/pathfinding.c` - `AStarChunk()`, `ReconstructLocalPath()`, corner-cutting logic
- `tests/test_mover.c` - `line_of_sight_repath` test

## Reproduction Steps

1. Run the demo
2. Select "Dungeon" terrain
3. Spawn 1000+ movers with string pulling disabled
4. Observe yellow (stuck) movers at room corners
5. Console shows: `Mover X stuck: pos=(...) target=(...) centerLOS=0`

## Related Code Locations

- Corner-cutting in pathfinder: `pathfinding.c:476`, `pathfinding.c:568`, `pathfinding.c:1230`
- LOS check in mover: `mover.c:263` (currently disabled)
- HasLineOfSight: `mover.c:15-56`
- HasClearCorridor (for string pulling): `mover.c:62-89`
- HasLineOfSightLenient: `mover.c:92-110`
