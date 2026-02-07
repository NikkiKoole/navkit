# Structural Audit: src/core/

**Date**: 2026-02-07
**Scope**: src/core/ (saveload.c, inspect.c, input.c, input_mode.c, pie_menu.c, sim_manager.c, time.c) plus traced dependencies and dependents.

---

## Discovered File Map

**Core files** (primary analysis targets):
- `src/core/saveload.c` -- Save/load with versioned migration (46KB)
- `src/core/inspect.c` -- Save file inspector, parallel deserialization (72KB)
- `src/core/input.c` -- Input handling with hierarchical mode dispatch (89KB)
- `src/core/input_mode.c` -- Mode/action state machine and bar rendering (27KB)
- `src/core/pie_menu.c` -- Radial pie menu with action dispatch (20KB)
- `src/core/sim_manager.c` -- Simulation activity counters (3KB)
- `src/core/sim_manager.h` -- Activity counter externs
- `src/core/time.c` / `time.h` -- Game time system
- `src/core/input_mode.h` -- InputAction/InputMode/WorkSubMode enums
- `src/core/pie_menu.h` -- Pie menu types
- `src/core/inspect.h` -- Inspect entry point

**Dependencies** (systems core/ calls into):
- `src/world/grid.h` -- CellType enum, grid globals
- `src/world/material.h` -- MaterialType enum, material grids, finish grids
- `src/world/designations.h` -- DesignationType enum, designation functions
- `src/world/pathfinding.h` -- HPA* graph, FindPath
- `src/entities/items.h` / `item_defs.h` -- ItemType enum, Item struct, item table
- `src/entities/stockpiles.h` -- Stockpile struct, MAX_STOCKPILE_SIZE
- `src/entities/mover.h` -- Mover struct, spatial grid
- `src/entities/jobs.h` -- JobType enum, Job struct
- `src/entities/workshops.h` -- WorkshopType enum, Recipe, Bill
- `src/simulation/water.h`, `fire.h`, `smoke.h`, `steam.h`, `temperature.h`
- `src/simulation/trees.h`, `groundwear.h`
- `src/game_state.h` -- Master header pulling in all of the above

**Dependents** (systems that call into core/):
- `src/main.c` -- Calls InitSimActivity, uses input_mode.h, pie_menu.h
- `src/render/rendering.c` -- Uses input_mode.h, time.h
- `src/render/ui_panels.c` -- Calls InitSimActivity (4 call sites)
- `src/simulation/*.c` -- All sim systems read/write sim_manager.h counters, use time.h

**Coupled via shared state**:
- sim_manager.h counters (8 extern ints) -- written by sim_manager.c, read/written by all simulation/*.c files
- input_mode.h state (inputMode, inputAction, workSubMode, etc.) -- written by input_mode.c, input.c, pie_menu.c
- game_state.h globals -- shared across everything

---

## Findings

### Pattern 1: saveload.c / inspect.c Parallel Save Migration

- **Type**: Parallel update
- **Where**:
  1. `src/core/saveload.c` -- `SAVE_VERSION 22`, `V21_MAT_COUNT 10`, `V19_ITEM_TYPE_COUNT 23`, `V18_ITEM_TYPE_COUNT 21`, all migration paths in `LoadWorld()`
  2. `src/core/inspect.c` -- `INSPECT_SAVE_VERSION 22`, `INSPECT_V21_MAT_COUNT 10`, `INSPECT_V19_ITEM_TYPE_COUNT 23`, `INSPECT_V18_ITEM_TYPE_COUNT 21`, all migration paths in `InspectSaveFile()`
- **Risk**: Bumping SAVE_VERSION or adding a new grid/entity field in saveload.c requires the exact same structural change in inspect.c. The migration code is ~500 lines of versioned if/else chains that must stay in lockstep. Missing inspect.c means the inspector silently reads corrupt data or crashes on newer saves.
- **Current drift**: No version drift detected -- both files are at version 22 with matching constants. However, the two files have independently-defined legacy struct typedefs (e.g., `StockpileV21` in saveload.c vs `InspStockpileV21` in inspect.c) that were manually copy-pasted and must remain structurally identical. The code comment in MEMORY.md explicitly documents this as a known hazard: "Both saveload.c AND inspect.c need parallel save migration updates."
- **Severity**: **High** -- documented in project memory as a known gotcha, which means it has required attention before. Every save version bump requires editing two files with complex struct definitions.

---

### Pattern 2: Legacy Stockpile Structs Use Current ITEM_TYPE_COUNT

- **Type**: Parallel update (latent time bomb)
- **Where**:
  1. `src/core/saveload.c:821` -- `StockpileV17.allowedTypes[ITEM_TYPE_COUNT]` (should be hardcoded V17 count)
  2. `src/core/saveload.c:865` -- `StockpileV16.allowedTypes[ITEM_TYPE_COUNT]` (should be hardcoded V16 count)
  3. `src/core/inspect.c:1377` -- `StockpileV17.allowedTypes[ITEM_TYPE_COUNT]` (same bug)
  4. `src/core/inspect.c:1423` -- `StockpileV16.allowedTypes[ITEM_TYPE_COUNT]` (same bug)
- **Risk**: When a new ItemType is added and ITEM_TYPE_COUNT increments, these legacy struct typedefs silently change size. Since the structs are used to `fread()` binary data that was written with the old count, the struct size no longer matches the on-disk layout. Loading a V16 or V17 save file after an enum change will read garbled data. This is explicitly documented in MEMORY.md as a "latent bug that works only if enum hasn't changed between those versions."
- **Current drift**: V18 and V19 paths correctly use hardcoded `V18_ITEM_TYPE_COUNT` (21) and `V19_ITEM_TYPE_COUNT` (23). V16 and V17 paths do not -- they use the live `ITEM_TYPE_COUNT` (currently 24). This works today only because the item count at V16/V17 time happened to match the value range, but the struct sizes are wrong.
- **Severity**: **High** -- this is a known bug that will silently corrupt save loading when the next ItemType is added, unless someone also fixes V16/V17 structs at the same time. The newer migration paths (V18, V19, V21) prove the developer knows the pattern -- V16/V17 were written before the pattern was established.

---

### Pattern 3: Settings Save/Load Parallelism

- **Type**: Parallel update
- **Where**:
  1. `src/core/saveload.c:SaveWorld()` SETTINGS section (~lines 200-290) -- 43 individual `fwrite()` calls for simulation settings
  2. `src/core/saveload.c:LoadWorld()` SETTINGS section (~lines 975-1060) -- 43 individual `fread()` calls that must match 1:1 in order and type
- **Risk**: Adding a new tweakable setting requires adding `fwrite` in SaveWorld AND `fread` in LoadWorld, in exactly the same position. If added in one but not the other, or added in different positions, all subsequent settings in the file shift by the size of the missing field. The code has comments: "NOTE: When adding new tweakable settings, add them here AND in LoadWorld below!" -- proving the developer is aware.
- **Current drift**: No drift detected -- both sections have identical settings in identical order. The water, fire, smoke, steam, temperature, ground wear, and tree settings all match.
- **Severity**: **Medium** -- the comment-based guard is the only protection. The settings are simple scalar types so the risk is limited to positional misalignment, but the 43-entry list makes it easy to miss one.

---

### Pattern 4: InputAction Enum Dispatch Chain (7 locations)

- **Type**: Growing dispatch / Parallel update
- **Where** -- every InputAction value must appear in ALL of these locations:
  1. `src/core/input_mode.h` -- InputAction enum definition (47 values)
  2. `src/core/input_mode.c:GetActionName()` -- switch returning display name for each action
  3. `src/core/input_mode.c:InputMode_GetBarText()` -- switch generating bar text per action
  4. `src/core/input_mode.c:InputMode_GetBarItems()` -- switch generating clickable bar items per action (action-selected section, ~lines 280-390)
  5. `src/core/input_mode.c:InputMode_GetBarItems()` -- switch mapping action to key/underline (action-key section, ~lines 246-278)
  6. `src/core/input.c:HandleInput()` -- action selection switch in mode selection (~line 1560)
  7. `src/core/input.c:HandleInput()` -- re-tap-to-back switch (~lines 1590-1640)
  8. `src/core/input.c:HandleInput()` -- drag execution switch (~lines 1700-1850)
  9. `src/core/pie_menu.c:PieMenu_ApplyAction()` -- mapping action to mode/submode
  10. `src/core/pie_menu.c:menus[]` -- static menu definitions referencing action values
- **Risk**: Adding a new action (e.g., a new draw or sandbox tool) requires updating at minimum 7-8 switch/if-else chains across 3 files. Missing one produces silent failures: the action exists in the enum but does nothing when selected, or is selectable but has no bar text, etc.
- **Current drift**: None detected -- all 47 InputAction values appear consistently across all dispatch sites. The `ACTION_DRAW_SOIL` sub-action pattern (category + specific type) was added across all sites correctly.
- **Severity**: **Medium** -- the large number of sites (8-10) makes this fragile, but the pattern has been followed consistently so far. No compile-time enforcement exists.

---

### Pattern 5: Cell Placement Sequence (Wall Material + Natural + Finish + Chunk Dirty + Flags)

- **Type**: Repeated code sequence
- **Where** (in `src/core/input.c`):
  1. `ExecuteBuildWall()` dirt path (~line 115-128) -- 13 lines: grid set, SetWallMaterial, SetWallNatural, SetWallFinish, CLEAR_FLOOR, SetFloorMaterial, ClearFloorNatural, SET_CELL_SURFACE, MarkChunkDirty, CLEAR_CELL_FLAG, SetWaterLevel, SetWaterSource, SetWaterDrain, DestabilizeWater
  2. `ExecuteBuildWall()` stone/wood path (~line 133-142) -- 8 lines: grid set, SetWallMaterial, ClearWallNatural, SetWallFinish, MarkChunkDirty, CLEAR_CELL_FLAG, SetWaterLevel+Source+Drain, DestabilizeWater
  3. `ExecuteBuildDirt()` (~line 268-276) -- 8 lines: grid set, SetWallMaterial, SetWallNatural, SetWallFinish, MarkChunkDirty, CLEAR_CELL_FLAG, SET_CELL_SURFACE
  4. `ExecuteBuildRock()` (~line 296-310) -- 13 lines: same as ExecuteBuildWall dirt path
  5. `ExecuteBuildSoil()` (~line 344-358) -- 10 lines: grid set, SetWallMaterial, SetWallNatural, SetWallFinish, CLEAR_FLOOR, SetFloorMaterial, ClearFloorNatural, MarkChunkDirty, CLEAR_CELL_FLAG, SET_CELL_SURFACE
  6. `ExecutePileSoil()` direct placement (~line 395-410) -- same sequence
  7. `ExecutePileSoil()` spread placement (~line 467-482) -- same sequence again
  8. Quick-edit wall placement in HandleInput (~line 1660-1665) -- 4 lines: grid set, SetWallMaterial, ClearWallNatural, SetWallFinish, MarkChunkDirty
- **Risk**: When a new property is added to cells (like `wallFinish` was in V22), every cell-placement site must be updated. Missing one means the new property has stale/default values for cells placed via that specific code path.
- **Current drift**: Minor drift exists. `ExecuteBuildDirt` and `ExecuteBuildSoil` do NOT clear water (SetWaterLevel/Source/Drain + DestabilizeWater) while `ExecuteBuildWall` and `ExecuteBuildRock` do. This may be intentional (soil doesn't displace water?) or a missed update. The quick-edit path (HandleInput, line 1660) also does not call SET_CELL_SURFACE, unlike the formal wall-placement paths, though it does call `DisplaceWater` which the formal paths do not.
- **Severity**: **Medium** -- 8 near-identical sequences across input.c with minor inconsistencies. The wallFinish addition (V22) required touching all of them.

---

### Pattern 6: Stockpile Filter Keybindings / Tooltip Display Parallelism

- **Type**: Parallel update
- **Where**:
  1. `src/core/input.c:HandleInput()` stockpile hover controls (~lines 1419-1468) -- 15 keybindings mapping keys to `sp->allowedTypes[ITEM_*]` toggles
  2. `src/render/tooltips.c` stockpile tooltip (~lines 164-200) -- 15 display entries showing filter status
- **Risk**: Adding a new item type that should be filterable requires adding a key binding in input.c AND a display indicator in tooltips.c. Missing tooltips.c means the filter works but the user can't see its state. Missing input.c means it shows in the tooltip but can't be toggled.
- **Current drift**: Both files cover the same 15 item types (ITEM_RED, GREEN, BLUE, ROCK, BLOCKS, LOG, SAPLING_*, DIRT, PLANKS, STICKS, BRICKS, CHARCOAL) and the same material filters (OAK, PINE, BIRCH, WILLOW). No drift detected. However, the leaf item types (ITEM_LEAVES_OAK etc.) have no keybinding or tooltip display -- this may be intentional but is undocumented.
- **Severity**: **Medium** -- two files, ~30 lines each, that must stay synchronized. The pattern has been maintained so far.

---

### Pattern 7: Name Table / Enum Count Mismatch in inspect.c

- **Type**: Growing dispatch / Parallel update
- **Where**:
  1. `src/core/inspect.c:56` -- `cellTypeNames[]` array with 18 entries and `#define CELL_TYPE_COUNT 18`
  2. `src/world/grid.h:27` -- CellType enum with 20 values (CELL_WALL=0 through CELL_TREE_LEAVES=19, but no explicit CELL_TYPE_COUNT or CELL_ROCK at position 10)
  3. `src/core/inspect.c:79` -- `jobTypeNames[]` with 15 entries, guarded by hardcoded `job->type < 15`
  4. `src/core/inspect.c:80` -- `designationTypeNames[]` with 10 entries, guarded by hardcoded `desig.type < 10`
  5. `src/render/tooltips.c:230` -- separate `jobTypeNames[]` with 15 entries (uses sizeof-based count)
- **Risk**: The `cellTypeNames` array is **wrong today**. It has 18 entries but the CellType enum has 20 values. The array at index 10 says "BEDROCK" but `CELL_ROCK=10` and `CELL_BEDROCK=11`. This means inspect.c will display "BEDROCK" for CELL_ROCK cells and index-out-of-bounds for CELL_TREE_TRUNK (17) and CELL_TREE_LEAVES (18). The jobTypeNames and designationTypeNames arrays use hardcoded size guards (< 15, < 10) instead of deriving from the enum, so adding a new job type or designation type requires updating the magic number.
- **Current drift**: **Active bug.** The cellTypeNames array is missing CELL_ROCK (between CELL_PEAT=9 and CELL_BEDROCK=11). All cell types from index 10 onward display the wrong name. CELL_TYPE_COUNT=18 is also wrong (should be 20). The jobTypeNames duplications between inspect.c and tooltips.c are identical today but independently maintained.
- **Severity**: **High** -- cellTypeNames is actively wrong. The inspect tool displays incorrect cell type names for CELL_ROCK and all values >= 10.

---

### Pattern 8: Simulation Activity Counter Increment/Decrement Scatter

- **Type**: Cross-file state coupling / Implicit contract
- **Where** -- the 8 counters declared in `src/core/sim_manager.h` and defined in `src/core/sim_manager.c` are directly modified by:
  1. `src/simulation/water.c` -- increments/decrements `waterActiveCells`
  2. `src/simulation/fire.c:51,166,168,223,225` -- resets and modifies `fireActiveCells`
  3. `src/simulation/smoke.c` -- modifies `smokeActiveCells`
  4. `src/simulation/steam.c` -- modifies `steamActiveCells`
  5. `src/simulation/temperature.c:61,62,96,213,218,245,250,287,367,520` -- modifies `tempSourceCount` and `tempUnstableCells` (10+ sites)
  6. `src/simulation/trees.c:404,421,468,473,508,526` -- modifies `treeActiveCells` (6 sites)
  7. `src/simulation/groundwear.c` -- modifies `wearActiveCells`
  8. `src/core/sim_manager.c:RebuildSimActivityCounts()` -- resets all 8 counters from grid state
  9. `src/render/ui_panels.c` -- calls `InitSimActivity()` in 4 places
- **Risk**: Every code path that adds/removes a fire cell, water cell, sapling, etc. must also update the corresponding counter. If a counter gets out of sync, the early-exit optimization silently skips the entire simulation system. `RebuildSimActivityCounts()` exists as a corrective (called after load), but during normal gameplay the counters must be manually maintained at every mutation site.
- **Current drift**: No known drift. The `RebuildSimActivityCounts()` function serves as a safety net after load, but there is no equivalent periodic self-heal during gameplay.
- **Severity**: **Medium** -- the counters are an optimization, not a correctness concern (a wrong count means a sim runs when it shouldn't need to, or doesn't run when it should). But silent-skip bugs would be very hard to diagnose.

---

### Pattern 9: Workshop Type Name Tables (3 independent copies)

- **Type**: Parallel update
- **Where**:
  1. `src/core/inspect.c:281` -- `workshopTypeNames[] = {"STONECUTTER", "SAWMILL", "KILN"}` (uppercase)
  2. `src/core/input.c:934` -- `workshopTypeNamesInput[] = {"Stonecutter", "Sawmill", "Kiln"}` (title case)
  3. `src/render/tooltips.c:665` -- `workshopTypeNames[] = {"Stonecutter", "Sawmill", "Kiln"}` (title case)
- **Risk**: Adding a new WorkshopType (e.g., WORKSHOP_FORGE) requires updating all 3 local arrays plus the `WorkshopType` enum. The arrays are all guarded by hardcoded size checks (`ws->type < 3`, `type < WORKSHOP_TYPE_COUNT`) that will silently fall through to "UNKNOWN" if missed.
- **Current drift**: All 3 arrays have 3 entries matching the 3 workshop types. The casing differs between inspect.c (uppercase) and the other two (title case) -- this is intentional (inspect is a CLI tool).
- **Severity**: **Low** -- only 3 sites, and workshop types change infrequently. The `WORKSHOP_TYPE_COUNT` enum value provides some guard.

---

### Pattern 10: Post-Load Rebuild Sequence (Implicit Call Contract)

- **Type**: Implicit contract
- **Where** (all called at the end of `LoadWorld()` in `src/core/saveload.c`, lines 1070-1100):
  1. `RebuildPostLoadState()` -- rebuilds itemCount, stockpileCount, workshopCount, blueprintCount, job free list, clears item reservations
  2. `ResetSmokeAccumulators()` -- resets smoke sim timing
  3. `ResetSteamAccumulators()` -- resets steam sim timing
  4. `RebuildSimActivityCounts()` -- rebuilds all 8 activity counters from grid state
  5. `hpaNeedsRebuild = true` + `MarkChunkDirty()` loops -- marks pathfinding for rebuild
  6. `BuildMoverSpatialGrid()` -- rebuilds mover spatial hash
  7. `BuildItemSpatialGrid()` -- rebuilds item spatial hash
  8. `ValidateAllRamps()` -- fixes invalid ramps from old saves
  9. `BuildEntrances()` + `BuildGraph()` -- rebuilds HPA* pathfinding
- **Risk**: This 9-step sequence must be called after any bulk state restoration. If a new system adds transient state derived from saved state (e.g., a spatial index, a counter, a free list), it must be added to this sequence. Nothing enforces the ordering or completeness. If someone adds a new entity type with a spatial grid and forgets to rebuild it here, loaded games will have broken queries.
- **Current drift**: No known drift. The sequence has grown organically (the comments suggest features were added incrementally).
- **Severity**: **Medium** -- the sequence is centralized in one place (LoadWorld), which helps, but the "add new system, remember to add rebuild here" contract is implicit.

---

### Pattern 11: InputAction-to-Mode Mapping (Pie Menu vs Input.c)

- **Type**: Implicit contract
- **Where**:
  1. `src/core/pie_menu.c:PieMenu_ApplyAction()` (~lines 148-178) -- maps each InputAction to the correct (inputMode, workSubMode) pair
  2. `src/core/input.c:HandleInput()` action-selection section (~lines 1530-1580) -- implicitly establishes the same mapping by where in the mode hierarchy each action's key binding appears
- **Risk**: The pie menu and keyboard input are two independent paths that must arrive at the same (mode, submode, action) state. If a new action is added to the keyboard path under SUBMODE_HARVEST but the pie menu maps it to SUBMODE_DIG, the UI state will be inconsistent depending on how the user selected it.
- **Current drift**: No drift detected. Both paths produce the same state for all current actions.
- **Severity**: **Low** -- two sites, and the structure is clear enough that the pattern is visible.

---

### Pattern 12: Grid Save/Load Loop Repetition

- **Type**: Repeated code sequence
- **Where** (in `src/core/saveload.c`):
  - `SaveWorld()` has 16 triple-nested `for(z) for(y) fwrite(grid[z][y], ...)` loops for 16 different grids
  - `LoadWorld()` has the same 16 triple-nested `for(z) for(y) fread(grid[z][y], ...)` loops
  - `src/core/inspect.c` has another 16 flat `fread(insp_*Cells, ...)` calls (using flat arrays instead of 3D)
- **Risk**: Adding a new per-cell grid (like wallFinish/floorFinish in V22) requires adding a save loop, a load loop, a version-guarded migration path, and an inspect read -- 4 sites minimum. The 35 triple-nested loops in saveload.c alone make this the largest repetitive pattern in the codebase.
- **Current drift**: None detected -- all 16 grids are saved, loaded, and inspected. The V22 addition of wallFinish/floorFinish was done correctly across all sites.
- **Severity**: **Medium** -- highly repetitive but well-established. Each new grid requires 4+ parallel additions, but the pattern is mechanically simple.

---

## Summary Table

| # | Pattern | Type | Severity | Sites |
|---|---------|------|----------|-------|
| 1 | saveload.c / inspect.c parallel migration | Parallel update | **High** | 2 files, ~500 lines each |
| 2 | V16/V17 legacy structs use live ITEM_TYPE_COUNT | Parallel update | **High** | 4 sites (2 per file) |
| 7 | cellTypeNames missing CELL_ROCK (active bug) | Growing dispatch | **High** | 1 site (inspect.c) + enum |
| 3 | Settings save/load 43-entry list | Parallel update | Medium | 2 sites in 1 file |
| 4 | InputAction 7+ dispatch chains | Growing dispatch | Medium | 8-10 sites across 3 files |
| 5 | Cell placement 8-line repeated sequence | Repeated sequence | Medium | 8 sites in input.c |
| 6 | Stockpile filter keybindings / tooltip sync | Parallel update | Medium | 2 files |
| 8 | Sim activity counter scatter | State coupling | Medium | 7+ files |
| 10 | Post-load rebuild implicit sequence | Implicit contract | Medium | 1 site, 9 calls |
| 12 | Grid save/load loop repetition | Repeated sequence | Medium | 35+ loops across 3 files |
| 9 | Workshop name tables (3 copies) | Parallel update | Low | 3 sites |
| 11 | Pie menu vs keyboard action-to-mode mapping | Implicit contract | Low | 2 sites |
