# Code Simplification Notes

Findings from analyzing the z-level implementation changes. These are potential improvements to consider, not urgent issues.

---

## 1. Duplicate Walkability Functions - FIXED

**Location:** `pathfinding.c`

~~There are two functions doing the same thing:~~
- ~~`IsCellWalkable(z, y, x)` around line 400~~
- ~~`IsWalkable3D(z, y, x)` around line 1135~~

**FIXED:** Removed `IsWalkable3D` and replaced all uses with `IsCellWalkable`.

---

## 2. ChunkHeap Uses Hardcoded z=0

**Location:** `pathfinding.c` lines 257-295

The heap bubble functions use `nodeData[0][cy][cx].f`:
```c
if (nodeData[0][cy][cx].f < nodeData[0][py][px].f) {
```

This works because the heap is only used within single z-level `AStarChunk` calls, but it's fragile. If someone tries to use the heap across z-levels, it will silently break.

**Suggestion:** Either:
- Add a comment explaining why this is safe
- Pass z to the heap functions
- Make the heap store f-values directly instead of looking them up

---

## 3. Inconsistent Walkability Checks

**Locations:** Throughout `pathfinding.c` and `mover.c`

Different patterns used:
- `grid[z][y][x] == CELL_WALL` (checks for wall)
- `grid[z][y][x] == CELL_WALKABLE` (misses ladders!)
- `IsCellWalkable(z, y, x)` (correct - includes ladders)
- `grid[0][y][x] == CELL_WALKABLE` in mover.c (hardcoded z=0)

**Suggestion:** Use `IsCellWalkable()` everywhere, or at minimum be consistent about whether ladders count as walkable.

---

## 4. Chunk ID Calculation Duplicated

**Location:** Multiple places in `pathfinding.c`

The formula `z * chunksPerLevel + cy * chunksX + cx` appears in:
- `GetChunk()` function
- `BuildEntrances()` 
- `AddLadderEntrance()`
- Various other places

**Suggestion:** Always use `GetChunk(x, y, z)` instead of inline calculation. Currently some places calculate chunk from cell coords, others from chunk coords - could add `GetChunkFromChunkCoords(cx, cy, z)` helper.

---

## 5. Magic Number 999999 - FIXED

**Location:** Throughout `pathfinding.c`

~~Used as "infinity" for pathfinding costs~~

**FIXED:** Added `#define COST_INF 999999` and replaced all occurrences.

---

## 6. Large Static Arrays

**Location:** `pathfinding.c` line 99

```c
static int chunkEntrances[MAX_GRID_DEPTH * MAX_CHUNKS_Y * MAX_CHUNKS_X][MAX_ENTRANCES_PER_CHUNK];
```

With MAX_GRID_DEPTH=16, MAX_CHUNKS=64, this is 16 * 64 * 64 * 64 * sizeof(int) = ~16MB just for this array.

**Suggestion:** Consider:
- Dynamic allocation
- Sparse storage (most chunks have few entrances)
- Or accept the memory cost if it's fine for target platform

---

## 7. Entrance Filtering by Z-Level

**Location:** Multiple loops in `pathfinding.c`

Pattern appears often:
```c
for (int i = 0; i < entranceCount; i++) {
    if (entrances[i].z != z) continue;
    // ... do work
}
```

**Suggestion:** Pre-index entrances by z-level:
```c
int entrancesByZ[MAX_GRID_DEPTH][MAX_ENTRANCES_PER_LEVEL];
int entranceCountByZ[MAX_GRID_DEPTH];
```

This would speed up loops that only care about one z-level.

---

## 8. Mover Code Still Uses z=0

**Location:** `mover.c` throughout

All grid access is hardcoded to z=0:
```c
if (grid[0][currentY][currentX] == CELL_WALL) {
```

```c
if (grid[0][ny][nx] == CELL_WALKABLE) {
```

**Note:** This is documented as "not yet implemented" in z-levels.md, but listing here for completeness.

---

## 9. Similar Functions Could Share Code

**Location:** `pathfinding.c`

These functions are very similar:
- `AStarChunk()` - bounded A* returning cost
- `AStarChunkMultiTarget()` - bounded Dijkstra to multiple targets  
- `ReconstructLocalPathWithBounds()` - bounded A* returning path

All three do bounded single-z-level search with slight variations.

**Suggestion:** Could extract common search loop, but might hurt readability. Low priority.

---

## 10. Commented Debug Code - FIXED

**Location:** Various

~~There's `#if 0` blocks and commented-out debug logging~~

**FIXED:** Removed the `#if 0` diagnostic block in RunHPAStar (~35 lines of dead code).

---

## 11. Duplicate Switch Statements in DrawCellGrid - FIXED

**Location:** `demo.c`

~~`DrawCellGrid()` had two nearly identical switch statements mapping CellType to texture - one for the transparent layer below, one for the current layer.~~

**FIXED:** Extracted `GetCellTexture(CellType)` helper function, reducing ~40 lines to ~15.

---

## Priority

If tackling these, suggested order:

1. ~~**High:** #3 (inconsistent walkability)~~ - reviewed, not a bug (checking for wall is correct)
2. ~~**Medium:** #1 (duplicate functions)~~ - FIXED
3. ~~**Medium:** #5 (magic numbers)~~ - FIXED
4. **Low:** #2, #4, #6, #7, #9 - reviewed, not worth the added complexity

None of these are blocking issues - all tests pass and performance is good.
