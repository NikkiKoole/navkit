# Terrain System Audit

**File**: `src/world/terrain.c`
**Date**: 2026-02-07
**Scope**: Terrain generation, connectivity validation, surface management, building/ramp/water coordination

---

## Finding 1: Connectivity fix can misidentify the largest component and fill it with rock

- **What**: `ReportWalkableComponents()` runs two flood-fill passes. Pass 1 identifies all components and records `largestIndex` (the scan-order index of the biggest component). Pass 2 resets `visited`, re-floods from scratch, and fills any component whose `compIndex != largestIndex` and `size < smallThreshold` with solid rock/dirt. Crucially, the filling happens **inline during iteration** -- cells are converted to solid while the scan is still discovering new components.
- **Assumption**: The component at index `largestIndex` in pass 2 is the same component that was largest in pass 1.
- **How it breaks**: In pass 2, when a small component (discovered before the largest) is filled with rock, those cells become solid. This can change the walkability of adjacent unvisited cells at z+1 (a cell above a newly-solid cell gains "standing on solid" walkability that it didn't have before). This could cause a previously-isolated cell to be absorbed into a later component, shifting all subsequent component indices. If the shift causes `compIndex` to skip past `largestIndex`, the actual largest component would not match `largestIndex` and could itself be partially filled.
- **Player impact**: On rare terrain seeds, the connectivity fix could destroy the main walkable area of the map, leaving the player with a mostly solid, unplayable world.
- **Severity**: Medium (requires specific terrain geometry; the threshold is typically small, so only tiny pockets are filled, reducing the chance of cascading index shifts)
- **Suggested fix**: In pass 2, first flood-fill all components without modifying anything (just identify which are small). Then, in a separate loop over the identified small components, fill them in. Alternatively, re-identify the largest component in pass 2 rather than reusing `largestIndex` from pass 1.

---

## Finding 2: GenerateCaves uses mixed z-levels -- border at z=1, caves at z=0

- **What**: `GenerateCaves()` initializes random rock/dirt noise on `grid[0]` (z=0) for all interior cells, runs cellular automata on `grid[0]`, and carves a connectivity path on `grid[0]`. But the border cells (`x==0 || y==0 || x==gridWidth-1 || y==gridHeight-1`) are placed on `grid[1]` (z=1), not `grid[0]`.
- **Assumption**: The border and the cave interior are on the same z-level.
- **How it breaks**: The border rock is one z-level above the cave floor. At z=0, border cells remain `CELL_AIR` (from `InitGrid`). At z=1, only the border perimeter is rock; all interior cells are `CELL_AIR`. The cave terrain at z=0 has no containment border, so movers can walk off the edge of the map (z=0 is walkable everywhere via implicit bedrock). The z=1 border ring is disconnected from the z=0 cave and serves no purpose.
- **Player impact**: Caves have no border walls. Movers can walk to the absolute edge of the map at z=0. The floating ring of rock at z=1 looks visually broken.
- **Severity**: High (cave generator produces fundamentally broken terrain)
- **Suggested fix**: Change the border placement from `grid[1][y][x] = CELL_ROCK` to `grid[0][y][x] = CELL_ROCK` so the border is on the same z-level as the cave.

---

## Finding 3: Ramp placement in GenerateHills has asymmetric duplicate-prevention

- **What**: When placing ramps in the heightmap-based generators (`GenerateHills`, `GenerateHillsSoils`, `GenerateHillsSoilsWater`), the East/South/West direction checks include `!CellIsRamp(grid[rampZ][...])` to avoid overwriting an existing ramp at that cell. The North direction check does NOT include this guard.
- **Assumption**: Each cell should have at most one ramp direction.
- **How it breaks**: The scan iterates y=0..gridHeight, x=0..gridWidth. When processing cell `(x, y)` and finding that the north neighbor `(x, y-1)` is higher, it places `CELL_RAMP_N` at `(x, y-1)` without checking if that cell already has a ramp. If an earlier iteration (processing a cell east/south/west of `(x, y-1)`) already placed a different ramp direction there, the North check silently overwrites it.
- **Player impact**: A ramp that was correctly placed for one transition gets overwritten with a North-facing ramp. The overwritten transition becomes broken: movers can no longer use the original ramp to cross that height difference. This creates isolated terrain pockets where some z-level transitions become one-way or unreachable.
- **Severity**: Medium (causes occasional disconnected terrain pockets; mitigated by connectivity fix if enabled)
- **Suggested fix**: Add `!CellIsRamp(grid[rampZ][y - 1][x])` to the North ramp check, matching the pattern used by East/South/West.

---

## Finding 4: Terrain generators don't clear entity state (movers, items, workshops, stockpiles)

- **What**: When the player clicks "Generate Terrain" in the UI, `GenerateCurrentTerrain()` dispatches to the appropriate generator. Each generator calls `InitGrid()` which clears the grid, cell flags, and tree grids. However, none of the generators (except `GenerateCraftingTest`) clear movers, items, workshops, stockpiles, designations, or water state.
- **Assumption**: Entity state from the previous map is compatible with the new terrain.
- **How it breaks**: If the player had movers, items, workshops, or stockpiles on the old map, they persist into the new terrain. Movers end up inside solid walls. Items float in rock. Workshops reference grid positions that are now walls. Stockpile slots point to non-existent floor cells. Jobs reference invalid positions.
- **Player impact**: After regenerating terrain, movers are stuck inside walls, items are invisible inside solid terrain, workshops are broken, and the job system throws errors or produces stuck movers.
- **Severity**: High (happens every time terrain is regenerated during a session with active entities)
- **Suggested fix**: Either have `GenerateCurrentTerrain()` (or the UI button handler) clear all entity state before generating, or have each generator call the appropriate clear functions. The UI handler already calls `InitMoverSpatialGrid`, `InitSimActivity`, `InitTemperature`, `BuildEntrances`, and `BuildGraph` after generation -- entity clears should be added there too.

---

## Finding 5: GenerateHillsSoilsWater calls InitWater() but other generators don't clear water

- **What**: `GenerateHillsSoilsWater()` calls `InitWater()` to reset water state before placing rivers/lakes. Other generators that don't use water (like `GenerateHills`, `GenerateTowers`, etc.) do not call `InitWater()` or `ClearWater()`.
- **Assumption**: Water state from a previous `GenerateHillsSoilsWater` run is cleared when switching to a non-water terrain type.
- **How it breaks**: Generate a hills+water terrain (rivers, lakes). Then switch terrain type to "Towers" and regenerate. The grid is cleared (`InitGrid`), but `waterGrid` retains the old river/lake water data. The water simulation continues running on the new terrain, placing water in cells that are now solid walls or in mid-air.
- **Player impact**: Phantom rivers and lakes from the previous terrain type persist on the new map. Water appears to flow through walls or float in the air.
- **Severity**: High (happens every time switching from water terrain to non-water terrain)
- **Suggested fix**: Call `ClearWater()` or `InitWater()` in `InitGrid()` or in the UI handler's terrain generation path, so water is always reset when terrain changes.

---

## Finding 6: Surface array not updated after building construction in GenerateHillsSoilsWater

- **What**: `GenerateHillsSoilsWater` computes a `surface[]` array (topmost solid z per column) after ramp placement. It then places buildings (towers, gallery flats, council blocks). `FlattenAreaTo` updates `surface[]` for the building footprint. However, the actual building construction (walls at `baseZ+1`, floors, ladders) modifies cells above the flattened surface without updating `surface[]`.
- **Assumption**: `surface[]` accurately reflects the topmost solid cell in each column.
- **How it breaks**: After placing a 3-floor tower, the `surface[]` for that column still says `baseZ` (the flattened ground level), not `baseZ+3` (the tower roof). When `AreaIsFlatAndDry` is called for a subsequent building placement attempt, it checks `surface[]` and sees the old baseZ value for cells where a tower already exists. It could decide the area is "flat" and attempt to place a second building overlapping the first.
- **Player impact**: Buildings can overlap each other on rare occasions, creating impossible merged structures with walls inside walls and unreachable interiors.
- **Severity**: Low (the random placement typically avoids overlap through position randomization, and only 6 buildings are placed total; but it is possible)
- **Suggested fix**: After each building placement, update `surface[]` for the building footprint to reflect the new topmost solid z-level. Alternatively, keep a separate "occupied" mask for placed buildings and check it in the placement loop.

---

## Finding 7: Connectivity fix fills ramp cells but doesn't update rampCount

- **What**: When `ReportWalkableComponents` fills small components with rock/dirt, it iterates over the component's cells and sets them to `CELL_ROCK` or `CELL_DIRT`. If any of those cells were ramps (`CELL_RAMP_*`), the ramp is replaced with solid, but `rampCount` is not decremented.
- **Assumption**: `rampCount` accurately reflects the number of ramps in the grid.
- **How it breaks**: `rampCount` becomes inflated (higher than the actual number of ramps). Since `rampCount` is recomputed in `BuildEntrances()` (called after terrain generation), this is only an issue during the brief window between the connectivity fix and the entrance rebuild. However, `rampCount` also affects `IsValidDestination()` -- with inflated `rampCount`, wall-tops are treated as valid destinations during any pathfinding that happens in this window.
- **Player impact**: Negligible in practice because `BuildEntrances()` is called immediately after generation. But if any other system queries destinations between the fix and the rebuild, movers could pathfind to wall tops.
- **Severity**: Low (timing window is very short, and the value is corrected by `BuildEntrances`)
- **Suggested fix**: In the connectivity fix loop, check if the cell being filled is a ramp and decrement `rampCount` accordingly. Or document that `rampCount` is only valid after `BuildEntrances()`.

---

## Finding 8: Terrain generators place ramps without incrementing rampCount

- **What**: `GenerateHills`, `GenerateHillsSoils`, and `GenerateHillsSoilsWater` place ramps by directly assigning `grid[rampZ][y][x] = CELL_RAMP_*`. They do not call `PlaceRamp()` (from grid.c) which would increment `rampCount`, nor do they manually increment `rampCount`.
- **Assumption**: `rampCount` is correct after terrain generation.
- **How it breaks**: After generation and before `BuildEntrances()`, `rampCount` is 0 (reset by `InitGrid` -> `InitGridWithSizeAndChunkSize`). `IsValidDestination()` checks `rampCount == 0` to filter out wall-tops. With `rampCount == 0` despite ramps existing, wall-tops are incorrectly excluded as destinations. `BuildEntrances()` (called from the UI after generation) recounts ramps, fixing the issue.
- **Player impact**: Same as Finding 7 -- brief window of incorrect behavior. In practice, no visible impact because `BuildEntrances()` runs immediately after.
- **Severity**: Low (corrected by `BuildEntrances` recount)
- **Suggested fix**: Either call `PlaceRamp()` instead of direct grid assignment, or add a ramp-counting pass at the end of each terrain generator. The current approach relies on `BuildEntrances()` for correction, which works but is fragile.

---

## Finding 9: GenerateSpiral3D and GenerateLabyrinth3D access z-levels without gridDepth bounds checks

- **What**: `GenerateSpiral3D` hardcodes `z3 = baseZ + 3 = 4` and accesses `grid[z3][...]` directly without checking `z3 < gridDepth`. The ring wall placement and ladder placement at `z3` have no z-bounds guard. Similarly, `GenerateLabyrinth3D` uses `z3 = baseZ + 3` with direct grid access.
- **Assumption**: `gridDepth` is always >= 5 (baseZ=1 + 4 levels).
- **How it breaks**: If `gridDepth` were reduced to 4, `z3 = 4` would be out of the logical grid range. Since `MAX_GRID_DEPTH = 16` and the arrays are statically allocated, this wouldn't cause a memory access violation, but cells would be placed in z-levels that are logically "outside" the grid and would never be rendered or interacted with. `PlaceFloor` does have a z-bounds check and would silently skip, but direct `grid[z3][y][x] = CELL_ROCK` assignments would write to unused z-levels.
- **Player impact**: With current `gridDepth = 16`, no impact. If `gridDepth` were ever reduced (e.g., for small test maps), the spiral/labyrinth generators would silently produce broken levels.
- **Severity**: Low (current gridDepth is always 16; only affects hypothetical future changes)
- **Suggested fix**: Clamp `z3` against `gridDepth` or skip level-3 content when `baseZ + 3 >= gridDepth`. More robustly, check all direct `grid[z]` assignments against `gridDepth`.

---

## Finding 10: FlattenAreaTo clears surface overlays above targetZ but doesn't clear water

- **What**: When `FlattenAreaTo` raises terrain (filling dirt from `current+1` to `targetZ`), it sets `grid[z][y][x] = CELL_DIRT` and clears the surface overlay. When it lowers terrain (removing from `targetZ+1` to `current`), it sets cells to `CELL_AIR` and clears surface. Neither path checks for or clears water in the affected cells.
- **Assumption**: Flattened cells don't contain water.
- **How it breaks**: If a river carved through an area (setting water at certain z-levels), and then a building attempts to flatten that area, `AreaIsFlatAndDry` would reject cells with water (via `waterMask`). However, `CarveEntryApron` calls `FlattenAreaTo` for the entry area *outside* the building footprint. These apron cells were not checked against `waterMask`. If an entry apron overlaps a river, the terrain is flattened but the water remains, creating dirt blocks submerged in water, or air cells with orphaned water data.
- **Player impact**: Building entry aprons near rivers could have water floating in the air or water inside solid dirt blocks. Movers trying to enter the building wade through phantom water.
- **Severity**: Low (entry aprons are small and rivers are narrow; overlap is rare)
- **Suggested fix**: In `FlattenAreaTo`, clear water data for any cell being modified: `SetWaterLevel(x, y, z, 0)` when converting cells.

---

## Summary

| # | Finding | Severity |
|---|---------|----------|
| 1 | Connectivity fix can misidentify largest component | Medium |
| 2 | GenerateCaves uses mixed z-levels (border at z=1, caves at z=0) | High |
| 3 | North ramp placement missing duplicate-prevention guard | Medium |
| 4 | Terrain generators don't clear entity state | High |
| 5 | Water state not cleared when switching terrain types | High |
| 6 | Surface array stale after building construction | Low |
| 7 | Connectivity fix doesn't update rampCount when filling ramps | Low |
| 8 | Terrain generators place ramps without incrementing rampCount | Low |
| 9 | Spiral3D/Labyrinth3D access z-levels without bounds checks | Low |
| 10 | FlattenAreaTo doesn't clear water in affected cells | Low |

**High severity**: 3 findings (2, 4, 5)
**Medium severity**: 2 findings (1, 3)
**Low severity**: 5 findings (6, 7, 8, 9, 10)

The most impactful issues are the entity state leaking across terrain regeneration (Finding 4), water state persistence across terrain types (Finding 5), and the GenerateCaves z-level mix-up (Finding 2). These affect the player every time terrain is regenerated during a gameplay session.
