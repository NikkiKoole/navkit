# Pathfinding Assumption Audit

**File**: `src/world/pathfinding.c` (plus `pathfinding.h`, `cell_defs.h`, `grid.h`)
**Date**: 2026-02-07
**Scope**: HPA* hierarchical pathfinding, incremental updates, A*/JPS/JPS+ algorithms, ladder/ramp link lifecycle

---

## Finding 1: chunkHeapZ not set by AStarChunk / AStarChunkMultiTarget / ReconstructLocalPathWithBounds

- **What**: The chunk-level binary heap (`ChunkHeapBubbleUp`, `ChunkHeapBubbleDown`) reads f-values from `nodeData[chunkHeapZ][y][x]` to determine priority ordering. The global `chunkHeapZ` is initialized to 0 and is only ever set in `JpsPlusChunk2D` (line 2922). The functions `AStarChunk`, `AStarChunkMultiTarget`, and `ReconstructLocalPathWithBounds` all use the chunk heap but write their node data to `nodeData[sz]` where `sz` is their z-level parameter -- they never set `chunkHeapZ = sz`.
- **Assumption**: The heap assumes `chunkHeapZ` matches the z-level being searched.
- **How it breaks**: `BuildGraph` iterates all z-levels and calls `AStarChunk` for each. For z=0 it works (chunkHeapZ defaults to 0). For z=1, z=2, etc., the heap compares f-values from `nodeData[0]` (stale/leftover data) instead of `nodeData[z]`. The heap ordering becomes random, producing incorrect shortest-path costs between entrances. The same bug affects `RebuildAffectedEdges` (calls `AStarChunkMultiTarget` per z-level), `FindPathHPA` connect phase (calls `AStarChunkMultiTarget` with `start.z`/`goal.z`), and `ReconstructLocalPath` (calls `ReconstructLocalPathWithBounds` with `sz`).
- **Player impact**: On multi-z maps, HPA* graph edges have wrong costs. Movers take suboptimal or bizarre paths between z-levels. Paths may fail to be found entirely if the heap incorrectly closes the goal node early with a worse cost. The bug is invisible on single-z maps (z=0 matches default chunkHeapZ=0).
- **Severity**: **HIGH**
- **Suggested fix**: Add `chunkHeapZ = sz;` at the top of `AStarChunk`, `AStarChunkMultiTarget`, and `ReconstructLocalPathWithBounds`, right before or after `ChunkHeapInit()`. This is a one-line fix in each function.

---

## Finding 2: rampCount not updated during incremental updates (UpdateDirtyChunks path)

- **What**: `BuildEntrances()` resets `rampCount = 0` and recounts all ramps in the grid (lines 513, 593, 632). However, `RebuildAffectedEntrances()` (called by `UpdateDirtyChunks()`) never touches `rampCount` at all. Meanwhile, other systems modify `rampCount` when placing/erasing ramps (grid.c, designations.c, input.c).
- **Assumption**: `rampCount` accurately reflects the number of directional ramps in the grid.
- **How it breaks**: If a ramp is destroyed by terrain changes (mining, channeling) that trigger `UpdateDirtyChunks` instead of a full `BuildEntrances`, `rampCount` may become stale. More critically, `BuildEntrances()` overwrites `rampCount` with a full recount. If `BuildEntrances()` is called after some ramps were placed via `grid.c` (which increments rampCount), the recount is correct. But if `UpdateDirtyChunks` runs instead, rampCount may drift. The real danger: `rampCount` is decremented manually in `input.c` (line 394, 466) and `designations.c` (line 866) on erasure, but if `BuildEntrances` then recounts, these decrements are lost (double-counting or under-counting).
- **Player impact**: `rampCount` controls behavior in `mover.c`: `hasZConnections` (line 734) uses it to decide whether to pick cross-z goals, and line 1260 uses it to decide whether to fall back to A* when HPA* fails. A stale rampCount could cause movers to attempt cross-z goals when no ramps exist (stuck movers), or fail to use the A* fallback when ramps exist (movers unable to path). Also affects `IsValidDestination` which skips wall-tops when `rampCount == 0`.
- **Severity**: **MEDIUM**
- **Suggested fix**: Either (a) remove `rampCount` recount from `BuildEntrances` and rely solely on increment/decrement at place/erase sites, or (b) add a `rampCount` recount to `UpdateDirtyChunks` as well. Option (a) is cleaner since `BuildEntrances` doing a recount contradicts the manual tracking elsewhere.

---

## Finding 3: Hash table returns wrong entrance when multiple entrances share (x,y,z)

- **What**: A cell can have both a border entrance (connecting two horizontal chunks) AND a ladder/ramp entrance (connecting two z-levels) at the same (x,y,z) position. `BuildEntranceHash` inserts all entrances with linear probing, and `HashLookupEntrance` returns the first match for a given (x,y,z).
- **Assumption**: Each (x,y,z) position has at most one entrance. The hash lookup is used in `RebuildAffectedEdges` to build `oldToNewEntranceIndex[]` -- mapping old entrance indices to new indices after incremental rebuild.
- **How it breaks**: If a ladder sits exactly on a chunk border, there will be two entrances at (x,y,z): one border entrance (chunk1 != chunk2) and one ladder entrance (chunk1 == chunk2). During incremental update, `HashLookupEntrance` maps both old entrances to the same new entrance (whichever happens to be first in the hash table). This means one old entrance's edges get remapped to the wrong new entrance. A ladder edge could get connected to a border entrance or vice versa, creating incorrect graph connectivity.
- **Player impact**: After an incremental update near a ladder on a chunk border, pathfinding through that ladder may break silently -- the abstract graph has edges connecting to wrong entrance types. Movers near such ladders may fail to find cross-z paths or take incorrect routes. This is a rare geometry (ladder exactly on chunk boundary) but becomes more likely with many ladders.
- **Severity**: **MEDIUM**
- **Suggested fix**: The hash lookup should be entrance-identity-aware. Options: (a) include the entrance's chunk1/chunk2 in the hash key, (b) have `HashLookupEntrance` return multiple matches and let the caller pick the right one, or (c) ensure ladder/ramp entrances are never placed on chunk borders (add deduplication or offset logic in `AddLadderEntrance`/`AddRampEntrance`).

---

## Finding 4: HPA* abstract search uses 2D heuristic for 3D paths (performance, not correctness)

- **What**: `FindPathHPA` uses `Heuristic(x1,y1,x2,y2)` (2D Manhattan distance, unscaled) for the abstract graph A* search (lines 2155, 2187, 2207). Graph edge costs are scaled (10 per cardinal step, 14 per diagonal). The heuristic returns raw Manhattan distance without the 10x scaling.
- **Assumption**: The heuristic should be comparable in scale to the g-values for A* to be efficient.
- **How it breaks**: With h-values ~10x smaller than g-values, the search degenerates toward Dijkstra (uniform-cost search). It still finds optimal paths (the heuristic is admissible) but explores far more nodes than necessary. Additionally, z-distance is completely ignored, so cross-z paths get no heuristic guidance at all.
- **Player impact**: HPA* abstract search is slower than it needs to be, especially on maps with many entrances or many z-levels. Players may see longer pathfinding pauses when many movers repath simultaneously. The effect is proportional to entrance count -- on small maps it's negligible, on large multi-z maps it could be noticeable.
- **Severity**: **LOW** (correctness is fine, performance only)
- **Suggested fix**: Use `Heuristic3D(x1,y1,z1,x2,y2,z2)` which is already defined and properly scaled. Replace `Heuristic(entrances[neighbor].x, entrances[neighbor].y, goal.x, goal.y)` with `Heuristic3D(entrances[neighbor].x, entrances[neighbor].y, entrances[neighbor].z, goal.x, goal.y, goal.z)` in the three locations.

---

## Finding 5: Silent capacity drops in HeapPush and ChunkHeapPush

- **What**: Both `HeapPush` (line 278) and `ChunkHeapPush` (line 399) silently return when the heap is full (`if (heap.size >= heap.capacity) return`). No warning, no error, no flag. The node is simply never added to the open set.
- **Assumption**: The heap capacity is always sufficient for the search space.
- **How it breaks**: `ChunkHeapPush` has capacity `MAX_GRID_WIDTH * MAX_GRID_HEIGHT / 4 = 65536`. For `ReconstructLocalPathWithBounds` with expanded bounds (up to 3 chunks x 3 chunks = potentially 48x48 = 2304 cells at chunk size 16, well within limits), this is fine. But if chunk sizes are set small (e.g., 8x8) and the expanded bounds cover a large area, or for the main A* search which uses the full grid, silently dropping nodes means valid paths are missed without any indication. The caller gets `-1` (no path) and has no way to know it was a capacity issue vs. genuinely no path.
- **Player impact**: If the capacity is hit, movers would fail to find paths that actually exist. Since there's no logging, this would appear as mysterious "stuck mover" bugs. With current default settings (16x16 chunks, 512x512 grid) the capacity should be sufficient, but it's a latent issue if grid/chunk parameters change.
- **Severity**: **LOW** (unlikely with current parameters, but dangerous if parameters change)
- **Suggested fix**: Add `TraceLog(LOG_WARNING, ...)` when capacity is hit, similar to the MAX_EDGES warning. This at least makes the failure diagnosable.

---

## Finding 6: Unaffected ladder entrance lookup uses linear scan with ambiguous matching

- **What**: In `RebuildAffectedEntrances`, when rebuilding ladder links for unaffected chunks (lines 1183-1191), the code scans all `newCount` entrances with `entrances[i].x == x && entrances[i].y == y` to find the ladder entrance indices. It does NOT check chunk1/chunk2 or whether the entrance is actually a ladder entrance vs. a border entrance.
- **Assumption**: Only one entrance exists per (x, y, z) combination -- so matching on (x, y) + z is sufficient.
- **How it breaks**: This is the same underlying issue as Finding 3 but from a different code path. If a border entrance happens to be at the same (x,y) as a ladder, the linear scan may pick the border entrance index for `entLow` or `entHigh`. The resulting `LadderLink` would reference a border entrance, and when `RebuildAffectedEdges` later adds the ladder edge between these entrances, the ladder edge would connect to a border entrance node. This could create a shortcut in the abstract graph that doesn't correspond to any real ladder.
- **Assumption overlap**: This is closely related to Finding 3. Both stem from the same root cause: entrances are not uniquely identifiable by position alone.
- **Severity**: **MEDIUM** (same root cause as Finding 3)
- **Suggested fix**: Same fix as Finding 3. Additionally, the linear scan should match on chunk1==chunk2 (ladder entrances always have chunk1==chunk2) to disambiguate from border entrances.

---

## Finding 7: Ramp links in RebuildAffectedEntrances can match wrong entrance for exit position

- **What**: When rebuilding ramp links for unaffected ramps (lines 1254-1262 in RebuildAffectedEntrances), the code searches for `entrances[i].x == exitX && entrances[i].y == exitY && entrances[i].z == z + 1` to find the exit entrance. If another entrance (border or different ramp) exists at that same position, it picks the wrong one.
- **Assumption**: The exit position (exitX, exitY, z+1) has exactly one entrance.
- **How it breaks**: A ramp exit at the top of a chunk border would have both a ramp entrance and a border entrance at (exitX, exitY, z+1). The linear scan finds whichever comes first. If it picks the border entrance, the ramp link's `entranceExit` points to a border entrance node, and the ramp's graph edge is connected to the wrong node.
- **Player impact**: Similar to Findings 3 and 6 -- cross-z pathfinding via ramps near chunk borders may produce incorrect routes after incremental updates.
- **Severity**: **MEDIUM**
- **Suggested fix**: Same as Finding 6 -- match on chunk1==chunk2 for ramp entrances.

---

## Finding 8: BuildGraph entrance collection scans all entrances linearly per chunk

- **What**: In `BuildGraph` (line 838-843), entrances for each chunk are collected by scanning the entire `entrances[]` array: `for (int i = 0; i < entranceCount && numEnts < 128; i++)`. With `MAX_ENTRANCES = 16384`, this is O(entranceCount) per chunk, and with `totalChunks` chunks, the total is O(entranceCount * totalChunks).
- **Assumption**: The chunk-entrance index (`chunkEntrances[]`) is available for fast lookup.
- **How it breaks**: `BuildChunkEntranceIndex()` exists and is used in `RebuildAffectedEdges`, but `BuildGraph` does NOT use it. On a large map with many z-levels (e.g., 512x512 with 16 z-levels = 32768 chunks, 16384 entrances), the full build does 32768 * 16384 = ~500M iterations just for entrance collection. This is pure wasted work.
- **Player impact**: Map load time is longer than necessary. On large multi-z maps, `BuildGraph` could take noticeably longer (seconds vs. milliseconds).
- **Severity**: **LOW** (performance only, full rebuild is infrequent)
- **Suggested fix**: Call `BuildChunkEntranceIndex()` at the start of `BuildGraph` and use `chunkEntrances[chunk]` instead of the linear scan.

---

## Finding 9: RunAStar uses O(W*H*D) open-list scan instead of a heap

- **What**: `RunAStar` (line 1631-1640) finds the best open node by scanning the entire `nodeData` array across all z-levels every iteration: `for z... for y... for x... if open && f < bestF`. On a 512x512x16 grid, this is 4.2M comparisons per node expansion.
- **Assumption**: This is only used for debug/visualization, not for gameplay pathfinding.
- **How it breaks**: If `RunAStar` is used as the actual pathfinding algorithm (via `FindPath(PATH_ALGO_ASTAR, ...)`), it runs at O(n^2) where n = grid size. A 512x512 path could require expanding ~250K nodes, each scanning 4.2M cells = ~1 trillion operations.
- **Player impact**: If a player or mover uses A* mode on a large grid, the game would freeze for seconds or minutes. The mover fallback in `mover.c` line 1260 falls back to A* when HPA* fails and ramps exist, which could trigger this on large maps.
- **Severity**: **MEDIUM** (the A* fallback path in mover.c can trigger this)
- **Suggested fix**: Either add a binary heap to RunAStar (like the chunk-level A*), or limit the fallback in mover.c to use a bounded search area rather than the full grid A*.

---

## Finding 10: Entrance hash table not cleared/invalidated after BuildEntrances

- **What**: `entranceHashBuilt` is set to `true` after `BuildEntranceHash()` and `false` after `ClearEntranceHash()`. However, `BuildEntrances()` (full rebuild) never calls `ClearEntranceHash()` or sets `entranceHashBuilt = false`. The hash table from a previous `RebuildAffectedEdges` call persists with stale data.
- **Assumption**: The hash table is always rebuilt before use. Currently it IS rebuilt at the start of `RebuildAffectedEdges`, so this is not an active bug.
- **How it breaks**: If any code path were to call `HashLookupEntrance` after `BuildEntrances()` (full rebuild) but before `RebuildAffectedEdges` (which rebuilds the hash), it would get results from the old entrance layout. Currently no code does this, but it's a latent trap for future development.
- **Severity**: **LOW** (latent, not currently triggered)
- **Suggested fix**: Call `ClearEntranceHash()` at the start of `BuildEntrances()`, or add `entranceHashBuilt = false` there, so stale data can't be accidentally used.

---

## Summary

| # | Finding | Severity | Category |
|---|---------|----------|----------|
| 1 | chunkHeapZ not set by AStarChunk/MultiTarget/ReconstructLocal | **HIGH** | State leak / wrong z-level |
| 2 | rampCount not updated in incremental path | **MEDIUM** | Cache staleness |
| 3 | Hash table returns wrong entrance for duplicate positions | **MEDIUM** | Bidirectional consistency |
| 4 | Abstract search uses unscaled 2D heuristic | **LOW** | Performance |
| 5 | Silent capacity drops in heap push | **LOW** | Silent failure |
| 6 | Unaffected ladder entrance lookup matches ambiguously | **MEDIUM** | Bidirectional consistency |
| 7 | Ramp link exit entrance matches ambiguously | **MEDIUM** | Bidirectional consistency |
| 8 | BuildGraph does not use chunk-entrance index | **LOW** | Performance |
| 9 | RunAStar has no heap (O(n^2) on full grid) | **MEDIUM** | Performance / fallback path |
| 10 | Entrance hash not invalidated after full rebuild | **LOW** | Latent stale cache |

**Critical fix**: Finding 1 should be fixed immediately -- it causes incorrect pathfinding on any multi-z map using HPA*. The fix is trivial (add `chunkHeapZ = sz;` in 3 functions).

**Cluster fix**: Findings 3, 6, and 7 all stem from the same root cause (entrances not uniquely identifiable by position). Fixing the entrance identity model once resolves all three.
