# Grid.c Assumption Audit

Audited: `src/world/grid.c` + related callers in `input.c`, `designations.c`, `fire.c`, `trees.c`
Date: 2026-02-07

---

## Finding 1: PlaceCellFull overwrites ramps/ladders without cleanup

- **What**: `PlaceCellFull()` in grid.c unconditionally sets `grid[z][y][x] = spec.cellType`. It does not check what was previously in the cell.
- **Assumption**: The cell being overwritten is plain terrain (air, dirt, wall). Never a ramp or ladder.
- **How it breaks**: Player uses draw-wall tool (`ExecuteBuildWall` in input.c) on a cell that contains a ramp. `PlaceCellFull` overwrites CELL_RAMP_N with CELL_WALL. `rampCount` is not decremented. Ladder cascade logic is not invoked if a ladder is overwritten.
- **Player impact**: `rampCount` drifts positive. Since `rampCount` gates `IsValidDestination()` (cell_defs.h:139) and HPA fallback logic (mover.c:1260), phantom ramp count causes pathfinder to consider wall-tops as valid destinations when no ramps actually exist, leading to movers pathing to unreachable locations. The count self-corrects on next full HPA entrance rebuild (`pathfinding.c:516` resets and recounts), but between mutation and rebuild, behavior is wrong.
- **Severity**: Medium
- **Suggested fix**: At the top of `PlaceCellFull`, check if the existing cell is a ramp (`CellIsDirectionalRamp`) and decrement `rampCount`. Check if it's a ladder (`IsLadderCell`) and call `EraseLadder` first. Alternatively, add a helper `ClearCellCleanup(x,y,z)` that all terrain mutation paths call before overwriting.

---

## Finding 2: Quick-edit right-click erase skips ramp cleanup

- **What**: In `input.c:1618-1627`, right-click erase in quick-edit mode checks for ladders (calls `EraseLadder`) but not ramps. A ramp cell falls through to the generic `grid[z][y][x] = CELL_AIR` path.
- **Assumption**: Non-ladder cells can be safely set to AIR without side effects.
- **How it breaks**: Player right-click erases a ramp in quick-edit mode. The cell becomes AIR, but `rampCount` is not decremented, and `EraseRamp()` is never called.
- **Player impact**: Same `rampCount` drift as Finding 1. Additionally, if the erased ramp was supporting a ramp chain (ramp-to-ramp staircase), `ValidateAndCleanupRamps` is never called, leaving dependent ramps with no solid support.
- **Severity**: Medium
- **Suggested fix**: Add a `CellIsDirectionalRamp` check before the generic erase, calling `EraseRamp(x, y, z)` for ramps (parallel to the existing `EraseLadder` check for ladders).

---

## Finding 3: Blueprint wall construction overwrites ramps/ladders without cleanup

- **What**: In `designations.c:1810-1828`, the BLUEPRINT_TYPE_WALL completion path checks `IsCellWalkableAt` then directly sets the cell to CELL_WALL or CELL_DIRT. Ramps and ladders ARE walkable, so they pass the check.
- **Assumption**: Walkable cells being overwritten are only AIR or floor cells, never ramps or ladders.
- **How it breaks**: Player queues a build-wall blueprint on a cell currently containing a ramp or ladder. Mover delivers material and the blueprint completes. The ramp/ladder is silently overwritten. `rampCount` not decremented for ramps; ladder cascade not invoked for ladders.
- **Player impact**: Same count drift. For ladders, the ladder above/below now has a broken connection but still displays as LADDER_DOWN/UP, creating a visual ladder that pathfinding cannot use. Movers see a connected ladder visually but pathfinding treats it as disconnected.
- **Severity**: Medium
- **Suggested fix**: Before wall placement, check for and properly erase ramps/ladders using `EraseRamp`/`EraseLadder`.

---

## Finding 4: Blueprint floor placement overwrites ramps/ladders without cleanup

- **What**: In `designations.c:1841-1849`, BLUEPRINT_TYPE_FLOOR completion unconditionally sets `grid[z][y][x] = CELL_AIR` and adds HAS_FLOOR flag, regardless of what was there before.
- **Assumption**: The cell is empty air.
- **How it breaks**: Player builds a floor on a cell that currently has a ladder or ramp. The special cell type is silently replaced with AIR+floor.
- **Player impact**: Same as Finding 3 -- rampCount drift and broken ladder connections.
- **Severity**: Medium
- **Suggested fix**: Same pattern: check and clean up existing ramps/ladders before overwriting.

---

## Finding 5: Fire burns away solid support without ramp validation

- **What**: In `fire.c:358-360`, when a cell burns out, `grid[z][y][x] = burnResult` replaces the cell type. `CELL_TREE_TRUNK` burns into `CELL_AIR` (cell_defs.c). Tree trunks have CF_SOLID, so they provide support for walkability and ramp exits above. Neither `ValidateAndCleanupRamps` nor `RecalculateLadderColumn` is called after burning.
- **Assumption**: Terrain changes from fire don't affect ramp validity or ladder connections.
- **How it breaks**: Player has a ramp whose high-side exit is on top of a tree trunk. Fire burns the trunk to air. The ramp's exit now has no solid support, but `IsRampStillValid` is never called. The ramp remains in the grid, and pathfinding still considers the z+1 transition valid, but walking up the ramp would place the mover on an unsupported cell.
- **Player impact**: Mover walks up a ramp after a fire and ends up on an unwalkable cell (air with no floor). Mover gets stuck or falls. The ramp appears functional but leads nowhere.
- **Severity**: High
- **Suggested fix**: After fire transforms a CF_SOLID cell to a non-solid cell, call `ValidateAndCleanupRamps` on the surrounding region (similar to how mining/channeling do it in designations.c).

---

## Finding 6: Tree leaf decay removes cells without ramp/ladder validation

- **What**: In `trees.c:409`, when tree leaves lose connection to their trunk, `grid[z][y][x] = CELL_AIR`. No validation of nearby ramps or ladders is performed.
- **Assumption**: Tree leaves (which are not CF_SOLID) don't affect ramps. This is actually correct for leaves since they lack CF_SOLID. However, the tree TRUNK burning/chopping path should be checked.
- **How it breaks**: Leaves themselves are not CF_SOLID so this is low risk. But if the chopping/felling system similarly removes trunks (CF_SOLID) without ramp validation, it would have the same issue as Finding 5.
- **Player impact**: Low for leaves alone. Potentially high if trunk removal paths share the same pattern.
- **Severity**: Low (leaves), potentially High (trunks -- verify chopping code separately)
- **Suggested fix**: Audit the tree chopping/felling code path for the same solid-support issue. Leaves are safe to skip.

---

## Finding 7: PlaceLadder on ramp cell silently destroys ramp without rampCount update

- **What**: `PlaceLadder()` at grid.c:167 checks `IsWallCell(current)` and `IsLadderCell(current)` but NOT `CellIsDirectionalRamp(current)`. A ramp is neither a wall nor a ladder, so PlaceLadder proceeds to overwrite it with a ladder cell.
- **Assumption**: The cell being overwritten is air or ground, never a ramp.
- **How it breaks**: Player places a ladder on a cell that already contains a ramp. The ramp is silently replaced. `rampCount` is not decremented.
- **Player impact**: `rampCount` goes permanently out of sync (until next HPA full rebuild). Pathfinder may make incorrect decisions about z-connections.
- **Severity**: Medium
- **Suggested fix**: Add `if (CellIsDirectionalRamp(current)) { EraseRamp(x, y, z); }` before ladder placement logic, or return early with no-op if ramp exists.

---

## Finding 8: EraseRamp/RemoveInvalidRamp only dirty the ramp's own chunk, not the exit chunk

- **What**: `EraseRamp()` (grid.c:396) and `RemoveInvalidRamp()` (grid.c:446) call `MarkChunkDirty(x, y, z)` only for the ramp's own cell. A ramp at (x,y,z) pointing north has its exit at (x,y-1,z+1). If the exit cell is in a different chunk (e.g., ramp at chunk boundary), that chunk is not dirtied.
- **Assumption**: The ramp and its exit are always in the same chunk, or `MarkChunkDirty(x,y,z)` with z+1 propagation is sufficient.
- **How it breaks**: Ramp at (x, 16, z) pointing north has exit at (x, 15, z+1). With chunkHeight=16, (x,16) is in chunk cy=1 but (x,15) is in chunk cy=0. Erasing the ramp dirties chunk cy=1 but not cy=0. The pathfinding graph for chunk cy=0 at z+1 still has a ramp-link entry to a ramp that no longer exists.
- **Player impact**: Pathfinder routes movers through a z-transition that no longer exists. Mover arrives at the exit cell and finds no ramp, gets stuck or pathing fails silently. Self-corrects on next full HPA rebuild, but may cause temporary stuck movers.
- **Severity**: Medium
- **Suggested fix**: In `EraseRamp` and `RemoveInvalidRamp`, also call `MarkChunkDirty(exitX, exitY, z+1)` for the exit cell. Compute exit from `GetRampHighSideOffset` before erasing.

---

## Finding 9: CanPlaceRamp allows placement at map edge with unreachable low side

- **What**: `CanPlaceRamp()` at grid.c:337-340 checks if the low side (entry point) is within bounds. If out of bounds, it skips the low-side walkability check entirely (does not return false).
- **Assumption**: A ramp is valid even if its low side is off the map edge.
- **How it breaks**: Player places a ramp at x=0 facing east (CELL_RAMP_E). High side is at (1, y, z+1) -- valid. Low side would be at (-1, y, z) -- off map. `CanPlaceRamp` skips the check and returns true. The ramp is placed but has no entry point; movers can walk DOWN the ramp from z+1 to z but cannot walk UP from z because there is no walkable low-side cell.
- **Player impact**: Ramp appears placeable and renders normally, but movers can only use it in one direction. Player may not understand why movers won't go up. The ramp IS functional for downward travel, so this is arguably "half-broken" rather than fully broken.
- **Severity**: Low
- **Suggested fix**: If `lowX`/`lowY` is out of bounds, return false from `CanPlaceRamp` (treat map edge as unwalkable for entry purposes).

---

## Finding 10: IsRampStillValid only checks high-side support, not low-side accessibility

- **What**: `IsRampStillValid()` (grid.c:400-430) validates that the high-side exit has solid ground below it. It does NOT check whether the low side (entry) is still walkable.
- **Assumption**: If the high side is structurally sound, the ramp is valid. Low-side changes are not a validity concern.
- **How it breaks**: Player mines out or walls off the low-side cell after ramp placement. The ramp's high side is fine, so `ValidateAndCleanupRamps` keeps it. But the ramp is now unreachable from below -- movers can only descend the ramp, never ascend it.
- **Player impact**: A ramp that was previously usable in both directions becomes one-directional after terrain changes nearby. Movers on the lower level can no longer reach the upper level via this ramp, but the ramp still appears fully functional. Player may not realize the entry was blocked.
- **Severity**: Low (pathfinding handles one-directional transitions; movers will find alternate routes. But player expectation is that the ramp "works" in both directions.)
- **Suggested fix**: Consider adding a low-side walkability check to `IsRampStillValid`, or at minimum display a visual indicator when a ramp's low side is inaccessible.

---

## Finding 11: hpaNeedsRebuild not set in InitGridWithSizeAndChunkSize

- **What**: `InitGridWithSizeAndChunkSize()` (grid.c:63-65) sets `needsRebuild = true` and `jpsNeedsRebuild = true` but NOT `hpaNeedsRebuild = true`.
- **Assumption**: HPA rebuild will be triggered by subsequent `MarkChunkDirty` calls during grid population.
- **How it breaks**: If code calls `InitGridWithSizeAndChunkSize` and then immediately attempts HPA pathfinding without any `MarkChunkDirty` calls (e.g., empty grid), the HPA graph is stale from a previous initialization.
- **Player impact**: Extremely unlikely in practice since grid population always dirties chunks. But violates the principle that initialization should leave all subsystems in a consistent state.
- **Severity**: Low
- **Suggested fix**: Add `hpaNeedsRebuild = true;` alongside the other two flags.

---

## Finding 12: Entrance scanning iterates chunkWidth past grid boundary for non-divisible grid sizes

- **What**: In `pathfinding.c:531-542`, entrance scanning iterates `for (int i = 0; i < chunkWidth; i++)` from `startX = cx * chunkWidth`. With `chunksX` computed via ceiling division, the last chunk may be narrower than `chunkWidth`. The scan reads cells beyond `gridWidth`.
- **Assumption**: Grid width is always a multiple of chunk width, OR `IsCellWalkableAt` bounds checking makes out-of-bounds reads safe.
- **How it breaks**: Grid is 100 wide with chunkWidth=16. `chunksX = ceil(100/16) = 7`. Last chunk starts at x=96. Scan goes from x=96 to x=111, but gridWidth=100. Cells 100-111 are out of bounds. `IsCellWalkableAt` returns false for these, so no invalid entrances are created, but unnecessary iterations occur.
- **Player impact**: No functional bug -- bounds checking in `IsCellWalkableAt` prevents corruption. Mild performance waste scanning non-existent cells.
- **Severity**: Low
- **Suggested fix**: Clamp the inner loop: `int maxI = (startX + chunkWidth > gridWidth) ? gridWidth - startX : chunkWidth;`

---

## Summary

| # | Finding | Severity |
|---|---------|----------|
| 1 | PlaceCellFull overwrites ramps/ladders without cleanup | Medium |
| 2 | Quick-edit right-click erase skips ramp cleanup | Medium |
| 3 | Blueprint wall overwrites ramps/ladders without cleanup | Medium |
| 4 | Blueprint floor overwrites ramps/ladders without cleanup | Medium |
| 5 | Fire burns solid support without ramp validation | **High** |
| 6 | Tree decay -- leaves safe, verify trunk paths | Low |
| 7 | PlaceLadder on ramp silently destroys ramp | Medium |
| 8 | EraseRamp doesn't dirty exit chunk across boundaries | Medium |
| 9 | CanPlaceRamp allows map-edge placement with no entry | Low |
| 10 | IsRampStillValid ignores low-side accessibility | Low |
| 11 | hpaNeedsRebuild not set in init | Low |
| 12 | Entrance scan past grid boundary (harmless) | Low |

### Recommended priority:
1. **Finding 5** (High) -- Fire + ramp interaction can strand movers
2. **Findings 1-4, 7** (Medium, clustered) -- All stem from missing "cleanup before overwrite" pattern. A single `ClearCellCleanup(x,y,z)` helper would fix all five.
3. **Finding 8** (Medium) -- Cross-chunk dirty propagation for ramp exits
