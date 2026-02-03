# Pathfinding Fixes Handoff

## Session Summary

Fixed several bugs related to stuck movers and ramp handling after channeling.

## Commits Made

1. **`512bfab`** - Fix ramp-to-ramp connections: allow adjacent ramps as valid high-side base
2. **`204d5c0`** - Fix stuck movers: accumulate timeWithoutProgress during repath, A* fallback for HPA*

---

## Issue 1: Ramps Invalidated When Channeling Rectangle

### Problem
When channeling a rectangle, interior ramps were being removed by `ValidateAndCleanupRamps()`. This happened because `IsRampStillValid()` only accepted solid cells (walls, dirt) as valid high-side support. When adjacent cells were channeled and became ramps, the original ramps were invalidated.

### Fix (src/world/grid.c:IsRampStillValid)
```c
// OLD: Only solid was valid
return CellIsSolid(exitBase);

// NEW: Ramps are also valid high-side support
if (CellIsSolid(exitBase)) return true;
if (CellIsRamp(exitBase)) return true;
return false;
```

### Test Added
`channel_rectangle_ramps` in test_jobs.c - verifies all 16 cells become ramps when channeling a 4x4 area.

---

## Issue 2: Stuck Mover Detection Not Working

### Problem
Movers with `needsRepath=true` never accumulated `timeWithoutProgress` because the movement update was skipped entirely with `if (m->needsRepath) continue;`. This meant job stuck detection (which checks `timeWithoutProgress > JOB_STUCK_TIME`) never triggered.

### Fix (src/entities/mover.c:UpdateMovers ~line 917)
```c
// OLD
if (m->needsRepath) continue;

// NEW - still accumulate stuck time for job stuck detection
if (m->needsRepath) {
    if (m->currentJobId >= 0 && m->pathLength == 0) {
        m->timeWithoutProgress += dt;
    }
    continue;
}
```

---

## Issue 3: HPA* Can't Find Cross-Z Paths When Same-Z Blocked

### Problem
A mover at (8,17,z1) with goal at (12,16,z1) couldn't find a path because:
- The cells between them at z1 were non-walkable (ramps at z0 don't provide solid support for z1 walkability)
- HPA* is 2D-chunk-based and only considers same-z-level paths
- A path EXISTS via z0: go down ramp, walk at z0, go up ramp to z1
- A* finds this path, but HPA* doesn't consider it

### Fix (src/entities/mover.c:ProcessMoverRepaths ~line 1243)
```c
int len = FindPath(algo, start, m->goal, tempPath, MAX_PATH);

// NEW - A* fallback when HPA* fails but ramps exist
if (len == 0 && algo == PATH_ALGO_HPA && rampCount > 0) {
    len = FindPath(PATH_ALGO_ASTAR, start, m->goal, tempPath, MAX_PATH);
}
```

### IMPORTANT: This is a Workaround, Not a Proper Fix
The user does NOT want this A* fallback as the permanent solution. The proper fix would be to improve HPA* to handle these cases natively. This fallback has performance implications since A* is slower than HPA* on large maps.

### Why HPA* Fails in This Case
- HPA* works with chunks per z-level
- It has `rampLinks` for cross-z connections, but these only work when the path explicitly needs to change z-levels
- When start and goal are same z-level, HPA* doesn't consider going through another z-level as a detour
- The `rampLinks` (14 in the test save) exist but aren't utilized for same-z queries

### Potential Proper Fixes to Investigate
1. Make HPA* consider ramp links even for same-z paths when direct path fails
2. Add "virtual entrances" at ramp positions that connect z-levels
3. Modify chunk connectivity to include cross-z paths via ramps
4. Pre-compute which z-level pairs are connected and try multi-z HPA* queries

---

## Inspect Improvements Requested

The user wants to improve the `--inspect` mode. Current issues noted during debugging:

1. **Designation type not shown** - Currently prints "DIG" for all designations, doesn't distinguish MINE vs CHANNEL
2. **No quick way to find stuck movers** - Had to manually check mover 17; could add `--stuck` flag to only show stuck movers
3. **Path test uses A\* but movers use HPA\*** - The `--path` test in inspect uses `PATH_ALGO_ASTAR` directly, which can give misleading results when movers actually use HPA*

### Relevant Files
- `src/core/inspect.c` - Main inspect mode implementation
- Line 361: Designation display (always shows "DIG")
- Line 445: Path test uses `FindPath(PATH_ALGO_ASTAR, ...)`

---

## Key Code Locations

### Ramp Validation
- `src/world/grid.c:IsRampStillValid()` - Checks if ramp has valid high-side support
- `src/world/grid.c:ValidateAndCleanupRamps()` - Called after terrain changes

### Mover Pathfinding
- `src/entities/mover.c:ProcessMoverRepaths()` - Handles repath requests
- `src/entities/mover.c:UpdateMovers()` - Main mover update loop
- `moverPathAlgorithm` global - Default is `PATH_ALGO_HPA`

### Job Stuck Detection
- `src/entities/jobs.c` - Each job driver checks `timeWithoutProgress > JOB_STUCK_TIME` (3.0s)
- `JOB_STUCK_TIME` defined as 3.0f in jobs.c line 21

### HPA* Implementation
- `src/world/pathfinding.c:FindPathHPA()` - HPA* pathfinding
- `rampLinks[]` and `rampLinkCount` - Cross-z connections for HPA*
- `src/world/pathfinding.c:BuildGraph()` - Builds HPA* graph including ramp links

---

## Test Save Files Used
- `/Users/nikkikoole/Projects/navkit/saves/2026-02-02_22-01-11.bin.gz` - Mover 17 stuck at (8,17,z1) with goal at (12,16,z1)
- `/Users/nikkikoole/Projects/navkit/saves/2026-02-02_21-45-19.bin.gz` - Earlier save showing missing ramps after channeling rectangle
