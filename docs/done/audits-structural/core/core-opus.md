# Core Audit Fix Plan (Opus-Level)

**Date**: 2026-02-07  
**Source**: structure-core.md findings  
**Scope**: Systematic remediation of 12 parallel-update patterns in src/core/

---

## Overview

This document provides a detailed plan of attack for fixing all issues identified in the structure-core.md audit. The fixes are organized by priority (High → Medium → Low) and grouped by remediation strategy to minimize cross-file conflicts.

**Total patterns**: 12  
**High severity**: 3 (Patterns 1, 2, 7)  
**Medium severity**: 7 (Patterns 3, 4, 5, 6, 8, 10, 12)  
**Low severity**: 2 (Patterns 9, 11)

---

## Phase 1: Critical Bugs (High Severity)

### Fix 1.1: Pattern 7 - cellTypeNames Active Bug

**Problem**: `inspect.c:56` has `cellTypeNames[]` with 18 entries but CellType enum has 20 values. Array is missing CELL_ROCK at index 10, causing all names from index 10+ to be wrong.

**Impact**: Inspect tool displays incorrect cell type names for CELL_ROCK and everything >= index 10.

**Fix Strategy**: Immediate correction (5 minutes)

**Steps**:
1. Read `src/world/grid.h` to confirm current CellType enum (should have 20 values: CELL_WALL=0 through CELL_TREE_LEAVES=19)
2. Read `src/core/inspect.c:56` cellTypeNames array
3. Insert "ROCK" at index 10 (between "PEAT" and "BEDROCK")
4. Verify array now has 20 entries matching enum exactly
5. Update `#define CELL_TYPE_COUNT 18` to `20`
6. Build and run: `make clean && make path`
7. Test inspect tool: `./bin/path --inspect <save_file>` and verify cell type names display correctly

**Verification**:
```bash
# Create a test save with various cell types, then inspect
./bin/path --inspect save.bin | grep -E "CELL_TYPE|ROCK|BEDROCK"
```

**Files modified**: `src/core/inspect.c` (2 lines changed)

---

### Fix 1.2: Pattern 2 - V16/V17 Structs Use Live ITEM_TYPE_COUNT

**Problem**: Legacy save structs for V16/V17 use `ITEM_TYPE_COUNT` instead of hardcoded counts. When ITEM_TYPE_COUNT changes, these structs silently change size and no longer match on-disk layout.

**Impact**: Loading V16/V17 saves after adding a new ItemType will read garbled data.

**Fix Strategy**: Hardcode historical counts (15 minutes)

**Steps**:
1. **Determine historical counts**:
   - Read git history: `git log --all --source --full-history -- src/entities/item_defs.h` to find ITEM_TYPE_COUNT at V16 and V17
   - Or: inspect existing V16/V17 save files to measure allowedTypes array size
   - Expected: V16 likely had 18-20 items, V17 likely 20-22 items
   
2. **Add hardcoded constants** (in both saveload.c and inspect.c):
   ```c
   #define V16_ITEM_TYPE_COUNT <determined_value>
   #define V17_ITEM_TYPE_COUNT <determined_value>
   ```

3. **Update struct definitions** (4 sites total):
   - `src/core/saveload.c:821` - `StockpileV17.allowedTypes[V17_ITEM_TYPE_COUNT]`
   - `src/core/saveload.c:865` - `StockpileV16.allowedTypes[V16_ITEM_TYPE_COUNT]`
   - `src/core/inspect.c:1377` - `InspStockpileV17.allowedTypes[V17_ITEM_TYPE_COUNT]`
   - `src/core/inspect.c:1423` - `InspStockpileV16.allowedTypes[V16_ITEM_TYPE_COUNT]`

4. **Update MEMORY.md**: Remove "latent bug" entry since it's now fixed

5. **Test**:
   - If V16/V17 saves exist: `./bin/path --load old_v16.bin` and verify no corruption
   - If not: document assumption of count values in commit message

**Verification**:
```bash
# Grep for remaining uses of bare ITEM_TYPE_COUNT in legacy structs
grep -n "allowedTypes\[ITEM_TYPE_COUNT\]" src/core/saveload.c src/core/inspect.c
# Should return zero matches in V16/V17 structs
```

**Files modified**: 
- `src/core/saveload.c` (4 lines: 2 defines, 2 struct arrays)
- `src/core/inspect.c` (4 lines: 2 defines, 2 struct arrays)
- `docs/memory/MEMORY.md` (remove latent bug note)

---

### Fix 1.3: Pattern 1 - Shared Save Migration Header

**Problem**: saveload.c and inspect.c have ~500 lines of parallel save migration code with independently-defined legacy structs (StockpileV21 vs InspStockpileV21). Every save version bump requires editing both files identically.

**Impact**: Missing inspect.c update means inspector silently reads corrupt data or crashes.

**Fix Strategy**: Extract shared migration structs into a common header (2-3 hours)

**Steps**:

1. **Create shared header**: `src/core/save_migration.h`
   - Move all `V##_*_COUNT` constants to this header
   - Move all legacy struct definitions (StockpileV21, StockpileV19, etc.) to this header
   - Use single struct names (not StockpileV21 vs InspStockpileV21)
   - Add header guards and includes for dependent types (ItemType, MaterialType, etc.)

2. **Refactor saveload.c**:
   - Include `save_migration.h`
   - Remove all local legacy struct typedefs
   - Remove all local V##_*_COUNT defines
   - Use shared struct names in migration code
   - Verify all `fread()`/`fwrite()` calls still reference correct struct sizes

3. **Refactor inspect.c**:
   - Include `save_migration.h`
   - Remove all local legacy struct typedefs (InspStockpileV21 → StockpileV21)
   - Remove all local V##_*_COUNT defines
   - Update all inspect code to use shared struct names
   - Keep flat array allocations (`insp_wallCells` etc.) but use shared counts

4. **Add compile-time guards**:
   - In `save_migration.h`, add static assertions to verify struct sizes match expectations:
   ```c
   _Static_assert(sizeof(StockpileV21) == <expected_size>, "StockpileV21 size mismatch");
   _Static_assert(V21_MAT_COUNT == 10, "V21 material count changed");
   ```

5. **Update MEMORY.md**:
   - Update saveload.c/inspect.c parallel update note to reference the new shared header
   - Note that adding a NEW version still requires updating both files, but struct definitions are now shared

6. **Build and test**:
   ```bash
   make clean && make path
   make test
   ./bin/path --load <save_file>
   ./bin/path --inspect <save_file>
   ```

7. **Verify no drift**: 
   - Diff the struct definitions between old saveload.c and inspect.c to confirm they were identical
   - Verify all legacy versions (V16, V17, V18, V19, V21) are represented in shared header

**Files modified**:
- `src/core/save_migration.h` (new file, ~200 lines)
- `src/core/saveload.c` (remove ~100 lines of struct defs, add 1 include)
- `src/core/inspect.c` (remove ~100 lines of struct defs, add 1 include, update struct names)
- `docs/memory/MEMORY.md` (update parallel update note)

**Testing strategy**:
- Load saves from each version V16-V22
- Run inspect on saves from each version
- Compare inspect output before/after to ensure identical results

---

## Phase 2: Medium Severity - Parallel Updates

### Fix 2.1: Pattern 3 - Settings Save/Load Macro

**Problem**: SaveWorld() and LoadWorld() have 43 parallel `fwrite`/`fread` calls for settings. Adding a new setting requires updating both in identical positions.

**Fix Strategy**: Generate both save/load from a single macro table (1 hour)

**Steps**:

1. **Create settings table macro** (in saveload.c):
   ```c
   #define SETTINGS_TABLE(X) \
       X(float, waterSpreadRate) \
       X(float, waterEvapRate) \
       X(float, waterDrainRate) \
       X(int, waterTicksPerUpdate) \
       X(bool, waterEnabled) \
       /* ... 38 more entries ... */ \
       X(bool, treeGrowthEnabled)
   ```

2. **Replace SaveWorld settings section**:
   ```c
   // SETTINGS
   #define WRITE_SETTING(type, name) fwrite(&name, sizeof(type), 1, file);
   SETTINGS_TABLE(WRITE_SETTING)
   #undef WRITE_SETTING
   ```

3. **Replace LoadWorld settings section**:
   ```c
   // SETTINGS
   #define READ_SETTING(type, name) fread(&name, sizeof(type), 1, file);
   SETTINGS_TABLE(READ_SETTING)
   #undef READ_SETTING
   ```

4. **Remove comment guards**: Delete "NOTE: When adding new tweakable settings, add them here AND in LoadWorld below!" since the macro enforces this.

5. **Test**:
   ```bash
   make clean && make path
   ./bin/path  # Create a save
   ./bin/path --load quicksave.bin.gz  # Verify settings load correctly
   ```

**Files modified**: `src/core/saveload.c` (replace ~80 lines with ~50)

---

### Fix 2.2: Pattern 12 - Grid Save/Load Macro

**Problem**: 16 triple-nested loops for saving grids, 16 for loading, 16 flat reads in inspect.c. Adding a new grid requires 3+ parallel additions.

**Fix Strategy**: Grid table macro (2 hours)

**Steps**:

1. **Create grid table macro** (in saveload.c):
   ```c
   #define GRID_TABLE(X) \
       X(wallCells, "WALL", gridDepth * gridHeight * sizeof(CellType)) \
       X(wallMaterialCells, "WALL_MAT", gridDepth * gridHeight * sizeof(MaterialType)) \
       X(wallNaturalCells, "WALL_NAT", gridDepth * gridHeight * sizeof(bool)) \
       X(wallFinishCells, "WALL_FIN", gridDepth * gridHeight * sizeof(MaterialType)) \
       /* ... 12 more grids ... */
   ```

2. **Replace SaveWorld grid loops**:
   ```c
   #define SAVE_GRID(name, label, rowsize) \
       fwrite(label, strlen(label), 1, file); \
       for(int z = 0; z < gridDepth; z++) { \
           for(int y = 0; y < gridHeight; y++) { \
               fwrite(name[z][y], rowsize, 1, file); \
           } \
       }
   GRID_TABLE(SAVE_GRID)
   #undef SAVE_GRID
   ```

3. **Replace LoadWorld grid loops**: Similar macro with `fread`

4. **Update inspect.c**: Create parallel `GRID_TABLE_INSPECT` macro using flat arrays

5. **Test**: Save/load and inspect across all grid types

**Files modified**: 
- `src/core/saveload.c` (replace ~150 lines with ~40)
- `src/core/inspect.c` (replace ~50 lines with ~20)

**Risk**: This is a large mechanical refactor. Consider doing this incrementally (convert 2-3 grids first, verify, then convert the rest).

---

### Fix 2.3: Pattern 4 - InputAction Dispatch Table

**Problem**: InputAction enum requires updates to 8-10 switch statements across 3 files.

**Fix Strategy**: Centralized action registry table (3-4 hours)

**Steps**:

1. **Create action registry** (new file `src/core/action_registry.h`):
   ```c
   typedef struct ActionDef {
       InputAction action;
       const char* name;           // For GetActionName()
       const char* barText;        // For GetBarText()
       char barKey;                // For GetBarItems()
       int barUnderlinePos;        // For GetBarItems()
       InputMode targetMode;       // For mode transitions
       WorkSubMode targetSubMode;  // For mode transitions
       bool canDrag;               // For drag execution
       bool canRepeatTap;          // For re-tap-to-back
   } ActionDef;
   
   // Master table - define once, use everywhere
   extern const ActionDef ACTION_REGISTRY[];
   extern const int ACTION_REGISTRY_COUNT;
   ```

2. **Populate registry** (in `src/core/action_registry.c`):
   ```c
   const ActionDef ACTION_REGISTRY[] = {
       {ACTION_NONE, "None", "", 0, 0, MODE_NORMAL, SUBMODE_NONE, false, false},
       {ACTION_DRAW_WALL, "Draw Wall", "Draw Stone Wall", 'S', 5, MODE_NORMAL, SUBMODE_NONE, true, false},
       // ... all 47 actions ...
   };
   ```

3. **Refactor input_mode.c**:
   - `GetActionName()`: lookup in ACTION_REGISTRY
   - `InputMode_GetBarText()`: lookup in ACTION_REGISTRY
   - `InputMode_GetBarItems()`: loop through ACTION_REGISTRY filtering by mode/submode

4. **Refactor input.c**:
   - Mode transition switches: lookup `targetMode`/`targetSubMode` in registry
   - Drag execution: check `canDrag` flag in registry

5. **Refactor pie_menu.c**:
   - `PieMenu_ApplyAction()`: lookup in registry instead of switch

6. **Add compile-time validation**:
   ```c
   _Static_assert(ACTION_REGISTRY_COUNT == 47, "Action registry incomplete");
   ```

7. **Update Makefile**: Add `src/core/action_registry.c` to build

**Files modified**:
- `src/core/action_registry.h` (new, ~30 lines)
- `src/core/action_registry.c` (new, ~250 lines)
- `src/core/input_mode.c` (replace ~300 lines of switches with lookups)
- `src/core/input.c` (replace ~200 lines of switches with lookups)
- `src/core/pie_menu.c` (replace ~30 lines of switches with lookups)
- `Makefile` (add action_registry.c)

**Testing**: Full UI smoke test - verify every action works via keyboard, bar clicks, and pie menu.

**Risk**: This is the largest refactor. Consider prototyping with 5-10 actions first.

---

### Fix 2.4: Pattern 5 - Cell Placement Helper Function

**Problem**: 8 near-identical cell placement sequences in input.c. Each new cell property requires updating all 8.

**Fix Strategy**: Centralized placement function (1 hour)

**Steps**:

1. **Create placement helper** (in `src/world/grid.h` or new `src/world/cell_placement.h`):
   ```c
   typedef struct CellPlacementSpec {
       CellType cellType;
       MaterialType wallMat;
       bool wallNatural;
       MaterialType wallFinish;
       bool clearFloor;
       MaterialType floorMat;
       bool floorNatural;
       bool clearWater;
       bool setSurface;
   } CellPlacementSpec;
   
   void PlaceCellFull(int x, int y, int z, CellPlacementSpec spec);
   ```

2. **Implement function** (in `src/world/grid.c` or `cell_placement.c`):
   ```c
   void PlaceCellFull(int x, int y, int z, CellPlacementSpec spec) {
       gridCells[z][y][x] = spec.cellType;
       SetWallMaterial(x, y, z, spec.wallMat);
       if (spec.wallNatural) SetWallNatural(x, y, z);
       else ClearWallNatural(x, y, z);
       SetWallFinish(x, y, z, spec.wallFinish);
       if (spec.clearFloor) {
           CLEAR_FLOOR(x, y, z);
           SetFloorMaterial(x, y, z, spec.floorMat);
           if (spec.floorNatural) SetFloorNatural(x, y, z);
           else ClearFloorNatural(x, y, z);
       }
       if (spec.clearWater) {
           SetWaterLevel(x, y, z, 0);
           SetWaterSource(x, y, z, false);
           SetWaterDrain(x, y, z, false);
           DestabilizeWater(x, y, z);
       }
       if (spec.setSurface) SET_CELL_SURFACE(x, y, z);
       CLEAR_CELL_FLAG(x, y, z, CELL_FLAG_PATH_DIRTY);
       MarkChunkDirty(x, y, z);
   }
   ```

3. **Replace all 8 sequences in input.c**:
   ```c
   // Before: 13 lines of grid[z][y][x] = ...; SetWallMaterial(...); etc.
   // After:
   PlaceCellFull(x, y, z, (CellPlacementSpec){
       .cellType = CELL_DIRT,
       .wallMat = MAT_DIRT,
       .wallNatural = true,
       .wallFinish = MAT_NONE,
       .clearFloor = true,
       .floorMat = MAT_DIRT,
       .floorNatural = true,
       .clearWater = true,
       .setSurface = true
   });
   ```

4. **Document inconsistencies**: Add comment explaining why some paths clear water and some don't (if intentional).

5. **Test**: Verify all cell placement paths (walls, dirt, rock, soil, pile) still work correctly.

**Files modified**:
- `src/world/grid.h` (add struct + function declaration)
- `src/world/grid.c` (add function implementation, ~30 lines)
- `src/core/input.c` (replace 8 sequences, net reduction ~60 lines)

---

### Fix 2.5: Pattern 6 - Stockpile Filter Table

**Problem**: Stockpile filter keybindings (input.c) and display (tooltips.c) must stay synchronized.

**Fix Strategy**: Shared filter definition table (30 minutes)

**Steps**:

1. **Create filter table** (in `src/entities/stockpiles.h` or new `stockpile_filters.h`):
   ```c
   typedef struct StockpileFilterDef {
       ItemType itemType;
       char key;
       const char* displayName;
   } StockpileFilterDef;
   
   extern const StockpileFilterDef STOCKPILE_FILTERS[];
   extern const int STOCKPILE_FILTER_COUNT;
   ```

2. **Populate table** (in `src/entities/stockpiles.c` or new file):
   ```c
   const StockpileFilterDef STOCKPILE_FILTERS[] = {
       {ITEM_RED, 'r', "Red"},
       {ITEM_GREEN, 'g', "Green"},
       {ITEM_BLUE, 'b', "Blue"},
       // ... 15 entries ...
   };
   const int STOCKPILE_FILTER_COUNT = sizeof(STOCKPILE_FILTERS) / sizeof(STOCKPILE_FILTERS[0]);
   ```

3. **Refactor input.c**: Loop through `STOCKPILE_FILTERS` to generate keybindings instead of 15 hardcoded if-statements.

4. **Refactor tooltips.c**: Loop through `STOCKPILE_FILTERS` to generate tooltip text instead of 15 hardcoded lines.

5. **Test**: Verify all stockpile filters still toggle correctly and display in tooltips.

**Files modified**:
- `src/entities/stockpiles.h` (add struct + extern)
- `src/entities/stockpiles.c` (add table definition, ~20 lines)
- `src/core/input.c` (replace ~50 lines with loop)
- `src/render/tooltips.c` (replace ~40 lines with loop)

---

### Fix 2.6: Pattern 8 - Simulation Activity Counter Self-Heal

**Problem**: Activity counters scattered across 7+ files. If a counter drifts, no automatic correction exists during gameplay.

**Fix Strategy**: Periodic validation + self-heal (1 hour)

**Steps**:

1. **Add validation function** (in `src/core/sim_manager.c`):
   ```c
   // Returns true if counters are valid, false if drift detected
   bool ValidateSimActivityCounts(void) {
       int actualWater = 0, actualFire = 0, actualSmoke = 0; // etc.
       
       // Scan grids to count actual active cells
       for (int z = 0; z < gridDepth; z++) {
           for (int y = 0; y < gridHeight; y++) {
               for (int x = 0; x < gridWidth; x++) {
                   if (waterLevels[z][y][x] > 0) actualWater++;
                   if (fireIntensity[z][y][x] > 0) actualFire++;
                   // ... etc for all 8 counters
               }
           }
       }
       
       bool valid = true;
       if (waterActiveCells != actualWater) {
           LOG_WARNING("Water counter drift: %d vs %d", waterActiveCells, actualWater);
           waterActiveCells = actualWater;
           valid = false;
       }
       // ... repeat for all 8 counters
       
       return valid;
   }
   ```

2. **Add periodic check** (in `src/simulation/update.c` or main loop):
   ```c
   // Every 60 seconds of game time
   if (gameTime % (60 * TARGET_FPS) == 0) {
       ValidateSimActivityCounts();
   }
   ```

3. **Add debug command**: Allow manual validation via F-key or console command.

4. **Test**: 
   - Manually corrupt a counter: `waterActiveCells = 9999;`
   - Wait 60 seconds of game time
   - Verify LOG_WARNING appears and counter self-heals

**Files modified**:
- `src/core/sim_manager.c` (add validation function, ~40 lines)
- `src/core/sim_manager.h` (add function declaration)
- `src/simulation/update.c` (add periodic call)

**Note**: This is a safety net, not a fix for the root cause. The root cause is the scattered increment/decrement sites.

---

### Fix 2.7: Pattern 10 - Post-Load Rebuild Checklist

**Problem**: Implicit contract - new systems must remember to add rebuild calls to LoadWorld().

**Fix Strategy**: Explicit registration system (1-2 hours)

**Steps**:

1. **Create rebuild registry** (new file `src/core/rebuild_registry.h`):
   ```c
   typedef void (*RebuildFunc)(void);
   
   typedef struct RebuildEntry {
       const char* name;
       RebuildFunc func;
       int priority;  // 0 = first, higher = later
   } RebuildEntry;
   
   void RegisterRebuildFunc(const char* name, RebuildFunc func, int priority);
   void ExecuteAllRebuilds(void);
   ```

2. **Implement registry** (new file `src/core/rebuild_registry.c`):
   ```c
   static RebuildEntry rebuilds[32];
   static int rebuildCount = 0;
   
   void RegisterRebuildFunc(const char* name, RebuildFunc func, int priority) {
       rebuilds[rebuildCount++] = (RebuildEntry){name, func, priority};
   }
   
   void ExecuteAllRebuilds(void) {
       // Sort by priority
       qsort(rebuilds, rebuildCount, sizeof(RebuildEntry), compare_priority);
       
       // Execute in order
       for (int i = 0; i < rebuildCount; i++) {
           LOG_INFO("Rebuild: %s", rebuilds[i].name);
           rebuilds[i].func();
       }
   }
   ```

3. **Register all existing rebuilds** (in respective system init functions):
   ```c
   // In items.c init:
   RegisterRebuildFunc("Items", RebuildPostLoadState, 10);
   
   // In smoke.c init:
   RegisterRebuildFunc("Smoke", ResetSmokeAccumulators, 20);
   
   // ... etc for all 9 systems
   ```

4. **Replace LoadWorld() sequence**:
   ```c
   // Before: 9 individual function calls
   // After:
   ExecuteAllRebuilds();
   ```

5. **Document priority levels**: Add comment explaining why certain rebuilds must happen before others (e.g., item rebuild before spatial grid rebuild).

**Files modified**:
- `src/core/rebuild_registry.h` (new, ~20 lines)
- `src/core/rebuild_registry.c` (new, ~40 lines)
- `src/core/saveload.c` (replace 9 calls with 1)
- 9 system files (add RegisterRebuildFunc calls)
- `Makefile` (add rebuild_registry.c)

**Testing**: Load a save and verify all rebuilds execute in correct order (check log output).

---

## Phase 3: Low Severity - Cleanups

### Fix 3.1: Pattern 9 - Workshop Name Table Consolidation

**Problem**: 3 copies of workshop type names (inspect.c, input.c, tooltips.c).

**Fix Strategy**: Single source of truth (15 minutes)

**Steps**:

1. **Add name field to WorkshopDef** (in `src/entities/workshop_defs.h`):
   ```c
   typedef struct WorkshopDef {
       WorkshopType type;
       const char* name;           // Add this
       const char* displayName;    // Add this (title case)
       // ... existing fields
   } WorkshopDef;
   ```

2. **Populate names in workshop table**:
   ```c
   [WORKSHOP_STONECUTTER] = {
       .type = WORKSHOP_STONECUTTER,
       .name = "STONECUTTER",
       .displayName = "Stonecutter",
       // ... existing fields
   }
   ```

3. **Replace 3 local arrays**:
   - `inspect.c`: Use `WORKSHOP_DEFS[ws->type].name`
   - `input.c`: Use `WORKSHOP_DEFS[type].displayName`
   - `tooltips.c`: Use `WORKSHOP_DEFS[ws->type].displayName`

4. **Test**: Verify workshop names display correctly in inspect, input bar, and tooltips.

**Files modified**:
- `src/entities/workshop_defs.h` (add 2 fields to struct, populate in table)
- `src/core/inspect.c` (remove local array, use WORKSHOP_DEFS)
- `src/core/input.c` (remove local array, use WORKSHOP_DEFS)
- `src/render/tooltips.c` (remove local array, use WORKSHOP_DEFS)

---

### Fix 3.2: Pattern 11 - Action-to-Mode Mapping Documentation

**Problem**: Pie menu and keyboard input must map actions to modes consistently, but relationship is implicit.

**Fix Strategy**: Documentation + validation (30 minutes)

**Steps**:

1. **Document the mapping** (add to `docs/architecture/input-system.md` or create new doc):
   ```markdown
   ## InputAction → Mode Mapping
   
   Each InputAction must map to exactly one (InputMode, WorkSubMode) pair.
   This mapping is enforced in two places:
   
   1. `pie_menu.c:PieMenu_ApplyAction()` - explicit switch statement
   2. `input.c:HandleInput()` - implicit via key binding location in mode hierarchy
   
   When adding a new action:
   - Add to pie_menu.c switch with correct mode/submode
   - Add key binding in input.c under correct mode/submode section
   - Verify both paths produce identical state
   ```

2. **Add validation test** (in test suite):
   ```c
   // For each action, verify pie menu and keyboard produce same state
   TEST(InputActionModeMapping) {
       for (int a = 0; a < ACTION_COUNT; a++) {
           // Simulate pie menu selection
           PieMenu_ApplyAction(a);
           InputMode modeFromPie = inputMode;
           WorkSubMode submodeFromPie = workSubMode;
           
           // Simulate keyboard selection (harder - requires mode switching)
           // ... test logic ...
           
           ASSERT_EQ(modeFromPie, modeFromKeyboard);
           ASSERT_EQ(submodeFromPie, submodeFromKeyboard);
       }
   }
   ```

3. **Test**: Run validation test after every action addition.

**Files modified**:
- `docs/architecture/input-system.md` (new or update existing)
- `tests/test_input.c` (add validation test, ~50 lines)

**Note**: If Fix 2.3 (Action Registry) is implemented, this pattern is automatically solved since the registry centralizes the mapping.

---

## Phase 4: Long-Term Structural Improvements

These are not immediate fixes but recommendations for future architectural work.

### Recommendation 4.1: Save Format Versioning Strategy

**Problem**: Every save version bump requires hand-editing 500+ lines of migration code in 2 files.

**Long-term solution**: Schema-driven serialization

- Define save schema in a declarative format (JSON, custom DSL, or C macros)
- Generate save/load/inspect code from schema
- Version bumps become schema additions instead of code changes

**Effort**: 1-2 weeks (requires significant refactor)

**Benefit**: Eliminates Patterns 1, 2, 12 entirely

---

### Recommendation 4.2: Input Action System Rewrite

**Problem**: InputAction dispatch is spread across 8-10 switch statements.

**Long-term solution**: Data-driven action system (partially addressed by Fix 2.3)

- Actions define their behavior declaratively (callbacks, mode transitions, bar display)
- Input system queries action properties instead of hardcoded switches
- New actions become data additions instead of code additions

**Effort**: 3-5 days (if building on Fix 2.3, otherwise 1-2 weeks)

**Benefit**: Eliminates Pattern 4 entirely

---

### Recommendation 4.3: Cell Property System

**Problem**: Every new cell property (like wallFinish in V22) requires updating 8+ placement sites, 16+ save/load loops, and migration paths.

**Long-term solution**: Component-based cell system

- Cells are entity IDs with components (WallMaterial, WallNatural, WallFinish, etc.)
- Generic component serialization handles save/load
- Placement becomes: `SetComponent(cell, COMP_WALL_MATERIAL, MAT_STONE)`

**Effort**: 2-4 weeks (massive refactor)

**Benefit**: Eliminates Patterns 5, 12, and simplifies Pattern 1

---

## Implementation Schedule

### Week 1: Critical Fixes
- **Day 1**: Fix 1.1 (cellTypeNames bug) - 5 min
- **Day 1**: Fix 1.2 (V16/V17 structs) - 15 min
- **Day 1-2**: Fix 1.3 (shared migration header) - 2-3 hours
- **Day 2**: Test all high-severity fixes together

### Week 2: Parallel Update Fixes (Quick Wins)
- **Day 3**: Fix 2.1 (settings macro) - 1 hour
- **Day 3**: Fix 2.5 (stockpile filter table) - 30 min
- **Day 3**: Fix 3.1 (workshop name table) - 15 min
- **Day 4**: Fix 2.6 (sim counter self-heal) - 1 hour
- **Day 4**: Fix 3.2 (action-mode documentation) - 30 min
- **Day 5**: Test all week 2 fixes

### Week 3: Major Refactors
- **Day 6-7**: Fix 2.4 (action registry) - 3-4 hours
- **Day 8**: Fix 2.3 (cell placement helper) - 1 hour
- **Day 9**: Fix 2.7 (rebuild registry) - 1-2 hours
- **Day 10**: Integration testing

### Week 4: Grid Macro (Optional - High Risk)
- **Day 11-12**: Fix 2.2 (grid save/load macro) - 2 hours implementation
- **Day 13-14**: Extensive testing (save/load all versions, inspect all grids)

**Total effort**: ~15-20 hours of focused work spread across 2-3 weeks with testing.

---

## Testing Strategy

### Per-Fix Testing
Each fix has specific test steps listed in its section.

### Integration Testing (After All Fixes)
1. **Save compatibility**: Load saves from V16, V17, V18, V19, V21, V22
2. **Inspect tool**: Run `--inspect` on all version saves
3. **Input system**: Full UI smoke test (every action via keyboard, bar, pie menu)
4. **Simulation**: Let simulation run for 10 minutes, verify no counter drift warnings
5. **Save/Load cycle**: Save, load, save again, verify binary identical
6. **Test suite**: `make test` must pass all 1200+ tests

### Regression Risks
- **Save/Load**: Most changes touch serialization - save corruption is primary risk
- **Input**: Action registry refactor could break UI flows
- **Simulation**: Counter changes could affect performance

---

## Rollback Plan

Each fix should be a separate commit so it can be reverted independently:

```bash
git log --oneline
# 4653578 Fix 3.2: Document action-mode mapping
# 4653579 Fix 3.1: Consolidate workshop name tables
# 4653580 Fix 2.7: Add rebuild registry system
# 4653581 Fix 2.6: Add sim counter self-heal
# 4653582 Fix 2.5: Create stockpile filter table
# 4653583 Fix 2.4: Create InputAction registry
# 4653584 Fix 2.3: Add cell placement helper
# 4653585 Fix 2.2: Add grid save/load macro
# 4653586 Fix 2.1: Add settings save/load macro
# 4653587 Fix 1.3: Extract shared save migration header
# 4653588 Fix 1.2: Hardcode V16/V17 ITEM_TYPE_COUNT
# 4653589 Fix 1.1: Fix cellTypeNames missing CELL_ROCK

# To revert a specific fix:
git revert 4653585  # Revert grid macro while keeping other fixes
```

---

## Open Questions

1. **V16/V17 ITEM_TYPE_COUNT values**: Do we have historical saves to measure, or should we inspect git history? (Required for Fix 1.2)

2. **Cell placement water clearing**: Is the inconsistency between ExecuteBuildDirt (no water clear) and ExecuteBuildWall (water clear) intentional? (Affects Fix 2.4 documentation)

3. **Leaf item filters**: Should ITEM_LEAVES_* have stockpile keybindings, or is omission intentional? (Affects Fix 2.5 scope)

4. **Grid macro risk**: Fix 2.2 is mechanically simple but touches 35+ loops. Should it be split into incremental commits (3 grids at a time) or done atomically?

5. **Action registry testing**: Fix 2.3 requires full UI smoke test. Should we build automated UI tests first, or rely on manual testing?

---

## Success Criteria

After all fixes are complete:

✅ **High severity issues resolved**:
- inspect.c displays correct cell type names
- V16/V17 saves load correctly after ItemType additions
- saveload.c and inspect.c share struct definitions

✅ **Parallel update risk reduced**:
- Settings use macro (1 site instead of 2)
- Grids use macro (1 site instead of 35+)
- Actions use registry (1 site instead of 8-10)
- Stockpile filters use table (1 site instead of 2)
- Workshop names use WORKSHOP_DEFS (1 site instead of 3)

✅ **Safety nets added**:
- Sim counters self-heal every 60 seconds
- Rebuild registry prevents missing post-load steps
- Static assertions guard struct sizes

✅ **Documentation updated**:
- MEMORY.md reflects new patterns
- Architecture docs explain input action system
- Commit messages document all assumptions

✅ **Tests pass**:
- `make test` passes 1200+ tests
- Save/load works for all versions V16-V22
- Inspect tool works for all versions
- Full UI smoke test passes

---

## Notes

- This plan prioritizes **correctness** over **elegance**. Some fixes (like macros) may seem crude compared to a full rewrite, but they're low-risk incremental improvements.

- The long-term recommendations (Phase 4) would eliminate most patterns entirely, but require 4-6 weeks of work. This plan focuses on **tactical fixes** that can be done in 2-3 weeks.

- Some fixes have dependencies:
  - Fix 1.3 (shared header) makes Fix 1.2 easier (constants go in one place)
  - Fix 2.3 (action registry) subsumes Fix 3.2 (action-mode mapping)
  - Fix 2.2 (grid macro) is the highest risk and should be done last

- Each fix is designed to be **independently revertible** via git revert, allowing fast rollback if issues arise.

---

**Total Lines of Code Impact**:
- Lines removed: ~800-1000 (mostly duplicate switch statements and struct definitions)
- Lines added: ~600-800 (mostly new registry/table systems)
- Net reduction: ~200 lines
- Maintenance burden reduction: ~60% (12 parallel patterns → 5 patterns)
