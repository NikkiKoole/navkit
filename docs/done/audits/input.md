# Assumption Audit: src/core/input.c

Audited: 2026-02-07
Focus: "Correct but stupid" bugs -- code that is technically valid but violates player expectations or implicit contracts between systems.

---

## Finding 1: Continuous pile-drag uses wrong material for non-dirt soil types

- **What**: When shift-dragging to pile soil continuously (lines 1879-1890), clay/gravel/sand/peat all pass `MAT_DIRT` as the material. The on-release path (lines 1950-1979) correctly passes `MAT_CLAY`, `MAT_GRAVEL`, `MAT_SAND`, `MAT_PEAT`.
- **Assumption**: Both code paths for the same action use the same parameters.
- **How it breaks**: Player holds shift and drags to pile clay. Each tick of the drag places `CELL_CLAY` with `MAT_DIRT` material. When they release, the final block is placed with `MAT_CLAY`. The continuously-placed cells have wrong material metadata.
- **Player impact**: Subtle material inconsistency. If any system queries wall material (e.g., mining yields, tooltips, save/load inspection), the piled cells report as dirt material despite being clay/gravel/sand/peat cell types.
- **Severity**: Medium
- **Suggested fix**: Change the continuous drag cases (lines 1879-1890) to use the correct material constants: `MAT_CLAY` for clay, `MAT_GRAVEL` for gravel, `MAT_SAND` for sand, `MAT_PEAT` for peat.

---

## Finding 2: ExecuteErase does not decrement rampCount when erasing ramp cells

- **What**: `ExecuteErase` (line 474) handles ladder cells specially via `EraseLadder()`, but ramp cells are erased by directly setting `grid[z][dy][dx] = CELL_AIR` without calling `EraseRamp()` or decrementing `rampCount`.
- **Assumption**: Ramp cells will only be erased through `EraseRamp()` which maintains the counter.
- **How it breaks**: Player draws a ramp, then switches to wall mode and right-click-drags over it. The cell becomes air, but `rampCount` stays inflated. Over time, `rampCount` drifts increasingly positive. Any system using `rampCount` for optimization or display shows wrong numbers.
- **Player impact**: Cosmetic or performance issue depending on what uses `rampCount`. If used for validation or early-exit checks, could cause subtle logic errors.
- **Severity**: Medium
- **Suggested fix**: In `ExecuteErase`, check for `CellIsDirectionalRamp()` before the generic erase path, and call `EraseRamp()` instead (similar to the ladder special case), or at minimum decrement `rampCount`.

---

## Finding 3: ExecuteBuildSoil and ExecutePileSoil do not trigger mover repathing

- **What**: `ExecuteBuildWall` and `ExecuteBuildRock` both iterate movers and set `needsRepath = true` when they place solid cells over previously walkable terrain. `ExecuteBuildSoil` (line 311) and `ExecutePileSoil` (line 340) also convert `CELL_AIR` to solid soil types but never set `needsRepath`.
- **Assumption**: Only wall/rock placement needs mover repathing.
- **How it breaks**: Player pauses, draws a large soil rectangle. A mover's existing path passes through those cells. On unpause, the mover walks into a solid cell and gets stuck until the stuck detector fires after 3 seconds, cancelling the job.
- **Player impact**: Movers appear to walk into walls briefly, then abort their jobs. Work gets interrupted. With pile mode (continuous placement), this happens frequently as the player is sculpting terrain while movers are active.
- **Severity**: High
- **Suggested fix**: Add the same mover path-checking loop from `ExecuteBuildWall` to `ExecuteBuildSoil` and `ExecutePileSoil`. Alternatively, extract a helper like `InvalidateMoverPathsThroughCell(x, y, z)`.

---

## Finding 4: ExecutePlaceGrass converts air to solid dirt without mover repathing or wall material setup

- **What**: `ExecutePlaceGrass` (line 1142) converts `CELL_AIR` to `CELL_DIRT` by directly setting `grid[z][dy][dx] = CELL_DIRT`. It does not set wall material, does not trigger mover repathing, and does not check for workshops/stockpiles/items at the cell.
- **Assumption**: Grass placement only affects surface decoration on existing dirt.
- **How it breaks**: Player uses sandbox grass tool on air cells. Those cells become solid CELL_DIRT with `MAT_NONE` wall material. Movers pathing through those cells get stuck. Workshops or stockpile cells at those locations become buried inside solid terrain. Items on the ground get entombed.
- **Player impact**: Movers freeze for 3s then cancel jobs. Workshop tooltips still show but the workshop is unreachable. Items vanish inside solid terrain.
- **Severity**: High
- **Suggested fix**: Use `PlaceCellFull` with a proper spec (including `MAT_DIRT` wall material), add mover repath checking, and consider checking for workshops/items at the cell.

---

## Finding 5: ExecutePileSoil has no bounds check on initial x,y coordinates

- **What**: The continuous pile-drag path (line 1868) converts mouse position to grid coordinates and passes them directly to `ExecutePileSoil`. No bounds check ensures `mouseX`/`mouseY` are within `[0, gridWidth)` / `[0, gridHeight)`. `ExecutePileSoil` itself immediately accesses `grid[placeZ - 1][y][x]` (line 348) without bounds validation.
- **Assumption**: The mouse-to-grid conversion always produces valid coordinates.
- **How it breaks**: Player shift-drags past the edge of the map. `ScreenToGrid` can return negative values or values >= grid dimensions. The unclamped coordinates index into the 3D grid array out of bounds.
- **Player impact**: Undefined behavior -- most likely a crash or memory corruption.
- **Severity**: High
- **Suggested fix**: Add bounds checking at the top of `ExecutePileSoil`, or clamp `mouseX`/`mouseY` before calling it. Same applies to the on-release pile path which uses unclamped `dragStartX`/`dragStartY`.

---

## Finding 6: ExecuteErase does not clear designations, stockpile cells, or workshop tiles

- **What**: `ExecuteErase` (line 474) changes cell types and clears floor/material metadata but does not cancel any designations (mine, channel, chop, etc.), remove stockpile cells, or check for workshops on the erased area.
- **Assumption**: Designations and entity state will be cleaned up by other systems or the player.
- **How it breaks**: Player designates a wall for mining, then switches to wall mode and right-click erases over the same area. The mining designation persists on what is now an air cell. A mover walks to the (now air) cell, tries to mine it, and either does nothing or behaves unpredictably. Similarly, stockpile slot data persists referencing cells that no longer exist, and workshops can be left in an inconsistent state.
- **Player impact**: Ghost designations that movers try to fulfill on empty air. Stockpile UI shows slots that don't correspond to real terrain. Possible stuck movers.
- **Severity**: Medium
- **Suggested fix**: In `ExecuteErase`, call `CancelDesignation(dx, dy, z)` for any active designation, consider calling `RemoveStockpileCells` for overlapping stockpiles, and skip or warn about workshop tiles.

---

## Finding 7: ExecuteRemoveTree does not clear chop/gather designations on removed tree cells

- **What**: `ExecuteRemoveTree` (line 1214) sets tree cells to `CELL_AIR` and clears tree-specific grids, but does not cancel chop or gather-sapling designations that may exist on those cells.
- **Assumption**: The sandbox tree removal tool and the work designation system are used independently.
- **How it breaks**: Player designates trees for chopping (WORK > HARVEST > CHOP), then switches to sandbox mode and removes the trees. The chop designations remain on now-empty air cells. Movers walk to the cells, find no tree, and the job system has to handle the mismatch.
- **Player impact**: Movers walking to empty cells and potentially getting stuck or wasting time. Designation markers visible on empty air confuse the player.
- **Severity**: Medium
- **Suggested fix**: In `ExecuteRemoveTree`, call `CancelDesignation(dx, dy, z)` for each removed cell.

---

## Finding 8: LoadWorld does not reset input mode or drag state

- **What**: When the player presses F6 to load (line 1538), `LoadWorld` is called but `InputMode_Reset()` is never called afterward. The input mode, action, submode, drag state, and all selected parameters survive the load.
- **Assumption**: Loading a world only replaces world data, not UI state.
- **How it breaks**: Player is mid-drag in DRAW > WALL mode, presses F6 to load. The world changes entirely, but `isDragging` is still true, `inputAction` is still `ACTION_DRAW_WALL`, and `dragStartX/dragStartY` reference coordinates that may be out of bounds in the loaded world (which could have different dimensions).
- **Player impact**: On the next mouse release, the stale drag executes against the freshly loaded world at potentially invalid coordinates. Could produce wrong placement or crash.
- **Severity**: Medium
- **Suggested fix**: Call `InputMode_Reset()` after successful `LoadWorld()` calls (both the F6 handler in input.c and any other load paths in main.c).

---

## Finding 9: Stockpile hover key handlers steal number keys from workshop hover when both overlap

- **What**: Stockpile hover controls (line 1339) run before workshop hover controls (line 1415). Both use `IsKeyPressed(KEY_ONE)` through `KEY_FOUR` (stockpile material filters) and `KEY_ONE` through `KEY_NINE` (workshop bill assignment) in `MODE_NORMAL`. Since `IsKeyPressed` returns true only once per frame, and stockpile handlers `return` after consuming the key, workshop number keys are unreachable when the hovered cell is both a stockpile cell and a workshop tile.
- **Assumption**: Stockpile cells and workshop tiles never overlap.
- **How it breaks**: Player creates a stockpile adjacent to a workshop such that one of the workshop's 3x3 tiles overlaps a stockpile cell. Hovering that cell and pressing 1-4 toggles stockpile material filters instead of adding a workshop bill. The player cannot add bills to the workshop from that tile.
- **Player impact**: Confusing: pressing "1" on what looks like a workshop toggles an Oak wood filter on a nearby stockpile instead of adding a recipe. Player may not realize what happened.
- **Severity**: Low (workaround: hover a non-overlapping workshop tile)
- **Suggested fix**: When both `hoveredStockpile >= 0` and `hoveredWorkshop >= 0`, either prioritize workshop controls or require a modifier key for one of the two.

---

## Finding 10: Quick-edit right-click erase does not clear wall material, surface type, or cell flags

- **What**: Quick-edit right-click erase (line 1622) sets `grid[z][y][x] = CELL_AIR`, calls `MarkChunkDirty` and `DestabilizeWater`, but does not clear wall material, wall natural flag, wall finish, floor flags, surface type, burned flag, or any designation.
- **Assumption**: Setting cell type to CELL_AIR is sufficient cleanup.
- **How it breaks**: Player quick-erases a dirt cell with tall grass. The cell becomes CELL_AIR but the surface type metadata still reads SURFACE_TALL_GRASS. If any rendering or simulation code checks surface type without first checking cell type, it sees grass on an air cell. Similarly, wall material metadata persists as stale data.
- **Player impact**: Likely invisible in practice since most code checks cell type first. But metadata is inconsistent, and if the cell is later converted back (e.g., by fill), it could inherit stale properties.
- **Severity**: Low
- **Suggested fix**: Mirror the cleanup from `ExecuteErase`: clear wall material, wall natural, floor flags, surface type, and burned flag. Or better, extract a shared `ClearCellFull(x, y, z)` helper.

---

## Finding 11: Mover repath check in ExecuteBuildWall uses `count > 0` instead of per-cell change detection

- **What**: In `ExecuteBuildWall` (line 146) and `ExecuteBuildRock` (line 288), the mover repathing block is guarded by `if (count > 0)`. Since `count` is cumulative across the entire drag rectangle, once the first cell is placed, ALL subsequent cells trigger the mover scan -- even cells that were skipped (workshop) or unchanged (already correct type).
- **Assumption**: The `count > 0` check is a per-cell guard.
- **How it breaks**: Player draws a 50x50 wall rectangle where most cells are already walls. The first cell changes, setting `count = 1`. The remaining 2499 iterations each scan all movers, even though no cell changed. This is O(area * moverCount * maxPathLength) instead of O(changedCells * moverCount * maxPathLength).
- **Player impact**: Frame stutter on large drag operations with many movers. No functional bug, but unexpected performance cliff.
- **Severity**: Low
- **Suggested fix**: Use a local `bool cellChanged` flag per iteration instead of checking `count > 0`. Or move the repath check inside the `if` block that increments `count`.

---

## Summary

| # | Finding | Severity |
|---|---------|----------|
| 1 | Pile-drag wrong material for non-dirt soils | Medium |
| 2 | ExecuteErase skips rampCount decrement | Medium |
| 3 | Soil placement missing mover repathing | High |
| 4 | Grass placement: air-to-solid without cleanup | High |
| 5 | ExecutePileSoil missing bounds check | High |
| 6 | ExecuteErase ignores designations/stockpiles | Medium |
| 7 | RemoveTree ignores chop designations | Medium |
| 8 | LoadWorld doesn't reset input state | Medium |
| 9 | Stockpile/workshop key conflict on overlap | Low |
| 10 | Quick-edit erase: stale metadata | Low |
| 11 | Mover repath guard uses cumulative count | Low |
