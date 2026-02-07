# Core Audit Implementation Plan

**Date**: 2026-02-07  
**Source**: Merged from structure-core.md findings, core-opus.md detailed fixes, and core-outcome.md triage  
**Goal**: Systematic remediation of 12 parallel-update patterns in src/core/

---

## Executive Summary

**Scope**: Fix high-risk parallel update patterns discovered in src/core/ structural audit

**Total effort**: ~15-20 hours focused work over 2-3 weeks with testing

**Success metrics**:
- ✅ 3 high-severity bugs fixed (cellTypeNames, V16/V17 structs, migration drift risk)
- ✅ Parallel update sites reduced from 12 patterns to ~5
- ✅ ~200 net LOC reduction, ~60% maintenance burden reduction
- ✅ All 1200+ tests pass, save/load works for V16-V22

**Non-goals**:
- Large-scale architectural rewrites
- Changing gameplay behavior or UI flows
- Combining save-format changes with unrelated refactors

---

## Phase 0: Critical Correctness Patches (Day 1, ~30 min)

### Fix 0.1: Pattern 7 - cellTypeNames Active Bug

**Problem**: inspect.c has wrong cell type names - array has 18 entries but enum has 20, missing CELL_ROCK at index 10

**Priority**: 🔴 HIGH - Active bug causing incorrect inspect output

**Effort**: 5 minutes

**Steps**:
1. Read `src/world/grid.h` to confirm CellType enum (20 values: CELL_WALL=0 through CELL_TREE_LEAVES=19)
2. Edit `src/core/inspect.c:56`:
   - Insert `"ROCK"` at index 10 (between "PEAT" and "BEDROCK")
   - Update `#define CELL_TYPE_COUNT 18` to `20`
3. Build: `make clean && make path`
4. Test: `./bin/path --inspect <save_file> | grep -E "CELL_TYPE|ROCK|BEDROCK"`

**Files**:
- `src/core/inspect.c` (2 lines changed)

**Verification**: Inspect tool displays correct cell type names for all 20 types

---

### Fix 0.2: Pattern 2 - V16/V17 Struct ITEM_TYPE_COUNT Time Bomb

**Problem**: Legacy structs use live `ITEM_TYPE_COUNT` instead of hardcoded historical values. Adding a new ItemType will corrupt V16/V17 save loading.

**Priority**: 🔴 HIGH - Latent bug waiting to trigger

**Effort**: 15 minutes

**Steps**:
1. **Determine historical counts**: Check git history or inspect binary V16/V17 saves
   ```bash
   git log --all --source -- src/entities/item_defs.h
   # Look for ITEM_TYPE_COUNT at V16 and V17 time
   ```
   Expected: V16 ≈ 20-21, V17 ≈ 21-22 items

2. **Add constants** (in both saveload.c and inspect.c):
   ```c
   #define V16_ITEM_TYPE_COUNT 20  // Adjust based on git history
   #define V17_ITEM_TYPE_COUNT 21  // Adjust based on git history
   ```

3. **Update struct definitions** (4 sites):
   - `src/core/saveload.c:821` → `StockpileV17.allowedTypes[V17_ITEM_TYPE_COUNT]`
   - `src/core/saveload.c:865` → `StockpileV16.allowedTypes[V16_ITEM_TYPE_COUNT]`
   - `src/core/inspect.c:1377` → `InspStockpileV17.allowedTypes[V17_ITEM_TYPE_COUNT]`
   - `src/core/inspect.c:1423` → `InspStockpileV16.allowedTypes[V16_ITEM_TYPE_COUNT]`

4. **Test**: If V16/V17 saves exist: `./bin/path --load old_v16.bin && ./bin/path --inspect old_v16.bin`

5. **Update MEMORY.md**: Remove "latent bug" entry

**Files**:
- `src/core/saveload.c` (4 lines: 2 defines, 2 array sizes)
- `src/core/inspect.c` (4 lines: 2 defines, 2 array sizes)
- `docs/memory/MEMORY.md` (remove note)

**Verification**: 
```bash
# Verify no more bare ITEM_TYPE_COUNT in legacy structs
grep -n "allowedTypes\[ITEM_TYPE_COUNT\]" src/core/saveload.c src/core/inspect.c
# Should return zero matches in V16/V17 contexts
```

---

## Phase 1: Low-Hanging Fruit (Days 2-3, ~3 hours)

### Fix 1.1: Pattern 9 - Workshop Name Tables (3 copies → 1)

**Problem**: Workshop names duplicated in inspect.c, input.c, tooltips.c

**Priority**: 🟡 LOW - Stable but redundant

**Effort**: 15 minutes

**Steps**:
1. Add fields to `WorkshopDef` in `src/entities/workshop_defs.h`:
   ```c
   typedef struct WorkshopDef {
       WorkshopType type;
       const char* name;           // Uppercase for inspect
       const char* displayName;    // Title case for UI
       // ... existing fields
   } WorkshopDef;
   ```

2. Populate in workshop table:
   ```c
   [WORKSHOP_STONECUTTER] = {
       .type = WORKSHOP_STONECUTTER,
       .name = "STONECUTTER",
       .displayName = "Stonecutter",
       // ...
   }
   ```

3. Replace 3 local arrays:
   - `inspect.c:281` → `WORKSHOP_DEFS[ws->type].name`
   - `input.c:934` → `WORKSHOP_DEFS[type].displayName`
   - `tooltips.c:665` → `WORKSHOP_DEFS[ws->type].displayName`

**Files**:
- `src/entities/workshop_defs.h` (add 2 fields, populate)
- `src/core/inspect.c` (remove array, use WORKSHOP_DEFS)
- `src/core/input.c` (remove array, use WORKSHOP_DEFS)
- `src/render/tooltips.c` (remove array, use WORKSHOP_DEFS)

**Test**: Verify workshop names in inspect output, input bar, and tooltips

---

### Fix 1.2: Pattern 6 - Stockpile Filter Table

**Problem**: Filter keybindings (input.c) and display (tooltips.c) must stay synchronized

**Priority**: 🟡 MEDIUM - No current drift but fragile

**Effort**: 30 minutes

**Steps**:
1. Create shared table in `src/entities/stockpiles.h`:
   ```c
   typedef struct StockpileFilterDef {
       ItemType itemType;
       char key;
       const char* displayName;
   } StockpileFilterDef;
   
   extern const StockpileFilterDef STOCKPILE_FILTERS[];
   extern const int STOCKPILE_FILTER_COUNT;
   ```

2. Populate in `src/entities/stockpiles.c`:
   ```c
   const StockpileFilterDef STOCKPILE_FILTERS[] = {
       {ITEM_RED, 'r', "Red"},
       {ITEM_GREEN, 'g', "Green"},
       {ITEM_BLUE, 'b', "Blue"},
       {ITEM_ROCK, 'o', "Rock"},
       {ITEM_BLOCKS, 'l', "Blocks"},
       // ... 10 more items ...
   };
   const int STOCKPILE_FILTER_COUNT = 
       sizeof(STOCKPILE_FILTERS) / sizeof(STOCKPILE_FILTERS[0]);
   ```

3. Refactor `input.c:1419-1468`: Replace 15 if-statements with loop over STOCKPILE_FILTERS

4. Refactor `tooltips.c:164-200`: Replace 15 display lines with loop over STOCKPILE_FILTERS

**Files**:
- `src/entities/stockpiles.h` (add struct + externs)
- `src/entities/stockpiles.c` (add table, ~20 lines)
- `src/core/input.c` (replace ~50 lines with loop)
- `src/render/tooltips.c` (replace ~40 lines with loop)

**Test**: Verify all stockpile filters toggle correctly and display in tooltips

**Open question**: Should ITEM_LEAVES_* have filters? Currently they don't - document if intentional.

---

### Fix 1.3: Pattern 8 - Sim Activity Counter Helpers

**Problem**: Activity counters modified directly across 7+ files. Drifted counter = silent simulation skip.

**Priority**: 🟡 MEDIUM - Risk mitigation (safety net)

**Effort**: 1 hour

**Steps**:
1. Add validation function in `src/core/sim_manager.c`:
   ```c
   bool ValidateSimActivityCounts(void) {
       int actualWater = 0, actualFire = 0, actualSmoke = 0;
       int actualSteam = 0, actualTempSource = 0, actualTempUnstable = 0;
       int actualTree = 0, actualWear = 0;
       
       // Count actual active cells from grids
       for (int z = 0; z < gridDepth; z++) {
           for (int y = 0; y < gridHeight; y++) {
               for (int x = 0; x < gridWidth; x++) {
                   if (waterLevels[z][y][x] > 0) actualWater++;
                   if (fireIntensity[z][y][x] > 0) actualFire++;
                   if (smokeDensity[z][y][x] > 0) actualSmoke++;
                   if (steamDensity[z][y][x] > 0) actualSteam++;
                   if (tempSources[z][y][x]) actualTempSource++;
                   if (tempUnstable[z][y][x]) actualTempUnstable++;
                   if (gridCells[z][y][x] == CELL_SAPLING || 
                       gridCells[z][y][x] == CELL_TREE_TRUNK) actualTree++;
                   if (groundWear[z][y][x] > 0) actualWear++;
               }
           }
       }
       
       bool valid = true;
       #define CHECK_COUNTER(name, actual) \
           if (name != actual) { \
               LOG_WARNING(#name " drift: %d vs %d", name, actual); \
               name = actual; \
               valid = false; \
           }
       
       CHECK_COUNTER(waterActiveCells, actualWater);
       CHECK_COUNTER(fireActiveCells, actualFire);
       CHECK_COUNTER(smokeActiveCells, actualSmoke);
       CHECK_COUNTER(steamActiveCells, actualSteam);
       CHECK_COUNTER(tempSourceCount, actualTempSource);
       CHECK_COUNTER(tempUnstableCells, actualTempUnstable);
       CHECK_COUNTER(treeActiveCells, actualTree);
       CHECK_COUNTER(wearActiveCells, actualWear);
       
       #undef CHECK_COUNTER
       return valid;
   }
   ```

2. Add periodic check (in main loop or simulation update):
   ```c
   // Every 60 seconds of game time
   if (gameTime % (60 * TARGET_FPS) == 0) {
       ValidateSimActivityCounts();
   }
   ```

3. Add debug command (optional): F-key or console command for manual validation

**Files**:
- `src/core/sim_manager.c` (add function, ~40 lines)
- `src/core/sim_manager.h` (add declaration)
- `src/main.c` or `src/simulation/update.c` (add periodic call)

**Test**: 
- Manually corrupt: `waterActiveCells = 9999;`
- Wait 60 sec game time
- Verify LOG_WARNING + self-heal

**Note**: This is a safety net, not a root-cause fix. Root cause is scattered increment sites.

---

### Fix 1.4: Pattern 3 - Settings Save/Load Macro

**Problem**: 43 parallel fwrite/fread calls. Adding a setting requires updating both in identical order.

**Priority**: 🟡 MEDIUM - Documented but fragile

**Effort**: 1 hour

**Steps**:
1. Create settings macro table in `src/core/saveload.c`:
   ```c
   #define SETTINGS_TABLE(X) \
       X(float, waterSpreadRate) \
       X(float, waterEvapRate) \
       X(float, waterDrainRate) \
       X(int, waterTicksPerUpdate) \
       X(bool, waterEnabled) \
       X(float, fireSpreadRate) \
       X(float, fireDecayRate) \
       X(int, fireTicksPerUpdate) \
       X(bool, fireEnabled) \
       X(float, smokeRiseRate) \
       X(float, smokeDiffuseRate) \
       X(float, smokeDecayRate) \
       X(int, smokeTicksPerUpdate) \
       X(bool, smokeEnabled) \
       X(float, steamRiseRate) \
       X(float, steamDiffuseRate) \
       X(float, steamDecayRate) \
       X(int, steamTicksPerUpdate) \
       X(bool, steamEnabled) \
       X(float, tempDiffuseRate) \
       X(float, tempCoolRate) \
       X(int, tempTicksPerUpdate) \
       X(bool, tempEnabled) \
       X(float, groundWearRate) \
       X(int, groundWearTicksPerUpdate) \
       X(bool, groundWearEnabled) \
       X(float, treeGrowthRate) \
       X(int, treeTicksPerUpdate) \
       X(bool, treeGrowthEnabled) \
       /* ... add remaining 15 settings ... */
   ```

2. Replace SaveWorld settings section (~lines 200-290):
   ```c
   // SETTINGS
   fwrite("SETTINGS", 8, 1, file);
   #define WRITE_SETTING(type, name) fwrite(&name, sizeof(type), 1, file);
   SETTINGS_TABLE(WRITE_SETTING)
   #undef WRITE_SETTING
   ```

3. Replace LoadWorld settings section (~lines 975-1060):
   ```c
   // SETTINGS
   fread(marker, 8, 1, file);  // Read "SETTINGS" marker
   #define READ_SETTING(type, name) fread(&name, sizeof(type), 1, file);
   SETTINGS_TABLE(READ_SETTING)
   #undef READ_SETTING
   ```

4. Remove comment guard: Delete "NOTE: When adding new tweakable settings..."

**Files**:
- `src/core/saveload.c` (replace ~80 lines with ~50)

**Test**:
```bash
make clean && make path
./bin/path  # Create save
./bin/path --load quicksave.bin.gz  # Verify settings load
# Check a few settings values in-game to confirm correctness
```

**Verification**: Save/load maintains all 43 settings correctly

---

## Phase 2: Structural Refactors (Days 4-8, ~8-10 hours)

### Fix 2.1: Pattern 1 - Shared Save Migration Header

**Problem**: saveload.c and inspect.c have ~500 lines of parallel migration code with duplicate legacy struct definitions.

**Priority**: 🔴 HIGH - Documented as critical parallel-update hazard

**Effort**: 2-3 hours

**Steps**:
1. Create `src/core/save_migration.h`:
   ```c
   #ifndef SAVE_MIGRATION_H
   #define SAVE_MIGRATION_H
   
   #include "src/entities/item_defs.h"
   #include "src/world/material.h"
   // ... other includes
   
   // Version constants
   #define V16_ITEM_TYPE_COUNT 20
   #define V17_ITEM_TYPE_COUNT 21
   #define V18_ITEM_TYPE_COUNT 21
   #define V19_ITEM_TYPE_COUNT 23
   #define V21_MAT_COUNT 10
   
   // Legacy struct definitions (V16-V21)
   typedef struct {
       int x, y, z;
       bool allowedTypes[V21_ITEM_TYPE_COUNT];
       bool allowedMaterials[V21_MAT_COUNT];
       int slotCount;
       // ...
   } StockpileV21;
   
   typedef struct {
       int x, y, z;
       bool allowedTypes[V19_ITEM_TYPE_COUNT];
       // ...
   } StockpileV19;
   
   // ... V18, V17, V16 structs
   
   // Static assertions for safety
   _Static_assert(sizeof(StockpileV21.allowedTypes) == V21_ITEM_TYPE_COUNT * sizeof(bool),
                  "StockpileV21 item array size mismatch");
   
   #endif // SAVE_MIGRATION_H
   ```

2. Refactor `src/core/saveload.c`:
   - Include `save_migration.h`
   - Remove all local legacy struct typedefs
   - Remove all local V##_*_COUNT defines
   - Use shared struct names in migration paths

3. Refactor `src/core/inspect.c`:
   - Include `save_migration.h`
   - Remove all `Insp*V##` struct typedefs
   - Remove all local V##_*_COUNT defines
   - Update migration code to use shared structs

4. Update `docs/memory/MEMORY.md`:
   - Update parallel update note to reference shared header
   - Note: Adding NEW version still requires both files, but structs are shared

**Files**:
- `src/core/save_migration.h` (new, ~200 lines)
- `src/core/saveload.c` (remove ~100 lines, add include)
- `src/core/inspect.c` (remove ~100 lines, add include, rename refs)
- `docs/memory/MEMORY.md` (update note)

**Test**:
```bash
make clean && make path
make test
./bin/path --load save_v16.bin  # If available
./bin/path --load save_v17.bin
./bin/path --load save_v18.bin
./bin/path --load save_v19.bin
./bin/path --load save_v21.bin
./bin/path --load save_v22.bin
./bin/path --inspect save_v16.bin
./bin/path --inspect save_v22.bin
```

**Verification**: 
- All version saves load correctly
- Inspect output matches pre-refactor output
- Net LOC reduction: ~100 lines

**Risk**: High - touches save/load core. Test exhaustively.

---

### Fix 2.2: Pattern 5 - Cell Placement Helper Function

**Problem**: 8 near-identical cell placement sequences. Each new property requires 8 updates.

**Priority**: 🟡 MEDIUM - Maintainability concern

**Effort**: 1 hour

**Steps**:
1. Create placement spec in `src/world/grid.h`:
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

2. Implement in `src/world/grid.c`:
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

3. Replace 8 sequences in `src/core/input.c`:
   - ExecuteBuildWall dirt path (~line 115)
   - ExecuteBuildWall stone/wood path (~line 133)
   - ExecuteBuildDirt (~line 268)
   - ExecuteBuildRock (~line 296)
   - ExecuteBuildSoil (~line 344)
   - ExecutePileSoil direct (~line 395)
   - ExecutePileSoil spread (~line 467)
   - Quick-edit wall (~line 1660)

   Example replacement:
   ```c
   // Before: 13 lines
   gridCells[z][y][x] = CELL_DIRT;
   SetWallMaterial(x, y, z, MAT_DIRT);
   SetWallNatural(x, y, z);
   // ... 10 more lines
   
   // After: 10 lines
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

4. Document water-clearing inconsistency:
   Add comment explaining why ExecuteBuildDirt doesn't clear water (if intentional)

**Files**:
- `src/world/grid.h` (add struct + declaration)
- `src/world/grid.c` (add function, ~30 lines)
- `src/core/input.c` (replace 8 sequences, net -60 lines)

**Test**: Verify all placement actions (walls, dirt, rock, soil, piles) work identically to before

**Open question**: Is water-clearing difference between dirt/soil and wall/rock intentional?

---

### Fix 2.3: Pattern 4 + 11 - InputAction Registry

**Problem**: InputAction requires 8-10 switch statement updates across 3 files. Pie menu and keyboard must map actions identically.

**Priority**: 🟡 MEDIUM - Large change, high maintenance benefit

**Effort**: 3-4 hours

**Steps**:
1. Create `src/core/action_registry.h`:
   ```c
   #ifndef ACTION_REGISTRY_H
   #define ACTION_REGISTRY_H
   
   #include "src/core/input_mode.h"
   
   typedef struct ActionDef {
       InputAction action;
       const char* name;           // For GetActionName()
       const char* barText;        // For GetBarText()
       char barKey;                // Key for bar shortcut
       int barUnderlinePos;        // Position in barText to underline
       InputMode targetMode;       // Mode transition target
       WorkSubMode targetSubMode;  // Submode transition target
       bool canDrag;               // Allow drag execution
       bool canRepeatTap;          // Allow re-tap-to-back
   } ActionDef;
   
   extern const ActionDef ACTION_REGISTRY[];
   extern const int ACTION_REGISTRY_COUNT;
   
   // Lookup helpers
   const ActionDef* GetActionDef(InputAction action);
   
   #endif
   ```

2. Create `src/core/action_registry.c`:
   ```c
   #include "action_registry.h"
   
   const ActionDef ACTION_REGISTRY[] = {
       {ACTION_NONE, "None", "", 0, 0, MODE_NORMAL, SUBMODE_NONE, false, false},
       
       {ACTION_DRAW_WALL, "Draw Wall", "Draw Stone Wall", 'S', 5, 
        MODE_NORMAL, SUBMODE_NONE, true, false},
       
       {ACTION_DRAW_WOOD_WALL, "Draw Wood Wall", "Draw Wood Wall", 'W', 5,
        MODE_NORMAL, SUBMODE_NONE, true, false},
       
       {ACTION_DRAW_DIRT, "Draw Dirt", "Pile Dirt", 'D', 5,
        MODE_NORMAL, SUBMODE_NONE, true, false},
        
       // ... all 47 actions with complete metadata ...
   };
   
   const int ACTION_REGISTRY_COUNT = sizeof(ACTION_REGISTRY) / sizeof(ACTION_REGISTRY[0]);
   
   const ActionDef* GetActionDef(InputAction action) {
       for (int i = 0; i < ACTION_REGISTRY_COUNT; i++) {
           if (ACTION_REGISTRY[i].action == action) {
               return &ACTION_REGISTRY[i];
           }
       }
       return &ACTION_REGISTRY[0];  // Return ACTION_NONE if not found
   }
   ```

3. Refactor `src/core/input_mode.c`:
   - `GetActionName()`: Use `GetActionDef(action)->name`
   - `InputMode_GetBarText()`: Use `GetActionDef(inputAction)->barText`
   - `InputMode_GetBarItems()`: Loop through registry filtering by current mode/submode

4. Refactor `src/core/input.c`:
   - Mode transitions: Use `GetActionDef()->targetMode/targetSubMode`
   - Drag execution: Check `GetActionDef()->canDrag`
   - Re-tap-to-back: Check `GetActionDef()->canRepeatTap`

5. Refactor `src/core/pie_menu.c`:
   - `PieMenu_ApplyAction()`: Use `GetActionDef()->targetMode/targetSubMode`

6. Add compile-time validation:
   ```c
   _Static_assert(ACTION_REGISTRY_COUNT == 47, "Action registry count mismatch");
   ```

7. Update `Makefile`: Add `src/core/action_registry.c` to build

**Files**:
- `src/core/action_registry.h` (new, ~30 lines)
- `src/core/action_registry.c` (new, ~250 lines)
- `src/core/input_mode.c` (replace ~300 lines with lookups)
- `src/core/input.c` (replace ~200 lines with lookups)
- `src/core/pie_menu.c` (replace ~30 lines with lookups)
- `Makefile` (add action_registry.c)

**Test**: FULL UI smoke test
- Verify every action works via keyboard
- Verify every action works via bar clicks
- Verify every action works via pie menu
- Verify all three produce identical state

**Risk**: LARGE refactor. Consider prototype with 5-10 actions first to validate approach.

**Note**: This fix also solves Pattern 11 (pie menu vs keyboard mapping) automatically.

---

## Phase 3: Optional Deep Refactors (Days 9-14, ~4-6 hours)

### Fix 3.1: Pattern 12 - Grid Save/Load Macro

**Problem**: 16 triple-nested save loops, 16 load loops, 16 inspect reads. Adding a grid = 3+ updates.

**Priority**: 🟠 HARD - High regression risk but high benefit

**Effort**: 2 hours + extensive testing

**Strategy**: Table-driven with incremental rollout

**Steps**:
1. Create grid table macro in `src/core/saveload.c`:
   ```c
   #define GRID_TABLE(X) \
       X(wallCells, "WALL", CellType) \
       X(wallMaterialCells, "WALL_MAT", MaterialType) \
       X(wallNaturalCells, "WALL_NAT", bool) \
       X(wallFinishCells, "WALL_FIN", MaterialType) \
       X(floorCells, "FLOOR", CellType) \
       X(floorMaterialCells, "FLOOR_MAT", MaterialType) \
       X(floorNaturalCells, "FLOOR_NAT", bool) \
       X(floorFinishCells, "FLOOR_FIN", MaterialType) \
       X(waterLevels, "WATER", int) \
       X(waterSources, "WATER_SRC", bool) \
       X(waterDrains, "WATER_DRN", bool) \
       X(fireIntensity, "FIRE", int) \
       X(smokeDensity, "SMOKE", int) \
       X(steamDensity, "STEAM", int) \
       X(temperatures, "TEMP", float) \
       X(groundWear, "WEAR", int)
   ```

2. Replace SaveWorld grid section:
   ```c
   #define SAVE_GRID(name, label, type) \
       fwrite(label, strlen(label), 1, file); \
       for(int z = 0; z < gridDepth; z++) { \
           for(int y = 0; y < gridHeight; y++) { \
               fwrite(name[z][y], gridWidth * sizeof(type), 1, file); \
           } \
       }
   
   GRID_TABLE(SAVE_GRID)
   #undef SAVE_GRID
   ```

3. Replace LoadWorld grid section (similar with fread)

4. Update `src/core/inspect.c`:
   Create parallel `GRID_TABLE_INSPECT` macro for flat array reads

**Files**:
- `src/core/saveload.c` (replace ~150 lines with ~40)
- `src/core/inspect.c` (replace ~50 lines with ~20)

**Test strategy**:
1. **Incremental approach**: Convert 2-3 grids first, verify save/load works
2. Convert remaining grids
3. **Exhaustive testing**:
   ```bash
   make clean && make path
   # Create save with varied cell types
   ./bin/path --save test.bin
   ./bin/path --load test.bin
   # Binary diff original and reloaded save
   ./bin/path --inspect test.bin > before.txt
   # After refactor:
   ./bin/path --inspect test.bin > after.txt
   diff before.txt after.txt  # Should be identical
   ```

**Risk**: HIGH - This touches every grid serialization. A single type mismatch = corruption.

**Mitigation**: 
- Use sizeof(type) in macros to catch type errors at compile time
- Test each grid individually during incremental rollout
- Keep original code in git history for easy revert

**Decision point**: Only proceed if Phase 0-2 are stable and well-tested.

---

### Fix 3.2: Pattern 10 - Post-Load Rebuild Registry

**Problem**: Implicit contract - new systems must remember to add rebuild to LoadWorld().

**Priority**: 🟠 HARD - Multi-system coordination

**Effort**: 1-2 hours

**Steps**:
1. Create `src/core/rebuild_registry.h`:
   ```c
   #ifndef REBUILD_REGISTRY_H
   #define REBUILD_REGISTRY_H
   
   typedef void (*RebuildFunc)(void);
   
   typedef enum {
       REBUILD_PHASE_COUNTS = 0,      // Item/stockpile/workshop counts
       REBUILD_PHASE_SIMULATION = 1,   // Sim accumulators, activity counters
       REBUILD_PHASE_SPATIAL = 2,      // Spatial grids
       REBUILD_PHASE_PATHFINDING = 3,  // HPA* graph
       REBUILD_PHASE_VALIDATION = 4,   // Ramp validation, etc.
   } RebuildPhase;
   
   void RegisterRebuildFunc(const char* name, RebuildFunc func, RebuildPhase phase);
   void ExecuteAllRebuilds(void);
   
   #endif
   ```

2. Implement in `src/core/rebuild_registry.c`:
   ```c
   #include "rebuild_registry.h"
   #include "src/core/logging.h"
   
   typedef struct {
       const char* name;
       RebuildFunc func;
       RebuildPhase phase;
   } RebuildEntry;
   
   static RebuildEntry rebuilds[32];
   static int rebuildCount = 0;
   
   void RegisterRebuildFunc(const char* name, RebuildFunc func, RebuildPhase phase) {
       rebuilds[rebuildCount++] = (RebuildEntry){name, func, phase};
   }
   
   static int ComparePhase(const void* a, const void* b) {
       return ((RebuildEntry*)a)->phase - ((RebuildEntry*)b)->phase;
   }
   
   void ExecuteAllRebuilds(void) {
       qsort(rebuilds, rebuildCount, sizeof(RebuildEntry), ComparePhase);
       
       for (int i = 0; i < rebuildCount; i++) {
           LOG_INFO("Rebuild [%d]: %s", rebuilds[i].phase, rebuilds[i].name);
           rebuilds[i].func();
       }
   }
   ```

3. Register existing rebuilds in system init functions:
   ```c
   // In src/entities/items.c init:
   RegisterRebuildFunc("Items", RebuildPostLoadState, REBUILD_PHASE_COUNTS);
   
   // In src/simulation/smoke.c init:
   RegisterRebuildFunc("Smoke", ResetSmokeAccumulators, REBUILD_PHASE_SIMULATION);
   
   // In src/entities/mover.c init:
   RegisterRebuildFunc("MoverSpatial", BuildMoverSpatialGrid, REBUILD_PHASE_SPATIAL);
   
   // In src/world/pathfinding.c init:
   RegisterRebuildFunc("HPAGraph", BuildGraph, REBUILD_PHASE_PATHFINDING);
   
   // ... etc for all 9 systems
   ```

4. Replace LoadWorld() rebuild sequence:
   ```c
   // Before: 9 individual calls
   // After:
   ExecuteAllRebuilds();
   ```

5. Document phase ordering in rebuild_registry.h comments

**Files**:
- `src/core/rebuild_registry.h` (new, ~30 lines)
- `src/core/rebuild_registry.c` (new, ~50 lines)
- `src/core/saveload.c` (replace 9 calls with 1)
- 9 system files (add RegisterRebuildFunc calls in init)
- `Makefile` (add rebuild_registry.c)

**Test**:
```bash
./bin/path --load save.bin
# Check log output for rebuild sequence
# Verify all systems operational after load
```

**Risk**: MEDIUM - Requires touching 9 systems. Phase ordering must be correct.

**Decision point**: Only if Phase 0-2 complete and need to add new systems frequently.

---

## Testing & Validation

### Per-Phase Testing
- **Phase 0**: Quick verify after each 5-15 min fix
- **Phase 1**: Test after each fix (15-60 min each)
- **Phase 2**: Extensive testing after each multi-hour refactor
- **Phase 3**: Full regression suite for deep refactors

### Integration Testing (After all selected phases complete)

1. **Save compatibility across versions**:
   ```bash
   ./bin/path --load save_v16.bin
   ./bin/path --load save_v17.bin
   ./bin/path --load save_v18.bin
   ./bin/path --load save_v19.bin
   ./bin/path --load save_v21.bin
   ./bin/path --load save_v22.bin
   ```

2. **Inspect tool verification**:
   ```bash
   ./bin/path --inspect save_v16.bin > v16_inspect.txt
   ./bin/path --inspect save_v22.bin > v22_inspect.txt
   # Verify cell type names, workshop names, etc. are correct
   ```

3. **Full input system smoke test**:
   - [ ] Every action works via keyboard (all 47 actions)
   - [ ] Every action works via bottom bar clicks
   - [ ] Every action works via pie menu
   - [ ] All three input methods produce identical state
   - [ ] Bar text displays correctly for all actions
   - [ ] Keybindings display correctly in bar

4. **Simulation system verification**:
   - [ ] Run simulation for 10 minutes game time
   - [ ] No counter drift warnings appear
   - [ ] Water, fire, smoke, steam, temperature, trees all functioning
   - [ ] Activity counters match actual cell counts

5. **Save/Load cycle**:
   ```bash
   ./bin/path  # Play for a bit
   # F5 to save
   ./bin/path --load quicksave.bin.gz
   # F5 to save again
   # Binary diff the two saves - should be identical (modulo timestamps)
   ```

6. **Test suite**:
   ```bash
   make test
   # All 1200+ tests must pass
   ```

### Regression Risk Areas

**High risk** (test extensively):
- Save/Load (Phases 0.2, 2.1, 3.1)
- Cell placement behavior (Phase 2.2)
- Input action dispatch (Phase 2.3)

**Medium risk**:
- Stockpile filter toggling (Phase 1.2)
- Simulation counter accuracy (Phase 1.3)
- Settings persistence (Phase 1.4)

**Low risk**:
- Workshop name display (Phase 1.1)
- Inspect tool output (Phase 0.1)

---

## Rollback Strategy

Each fix is a separate commit for independent reversion:

```bash
git log --oneline
# abcd123 Fix 3.2: Add rebuild registry
# abcd124 Fix 3.1: Add grid save/load macro
# abcd125 Fix 2.3: Create InputAction registry
# abcd126 Fix 2.2: Add cell placement helper
# abcd127 Fix 2.1: Extract shared save migration header
# abcd128 Fix 1.4: Add settings save/load macro
# abcd129 Fix 1.3: Add sim counter self-heal
# abcd12a Fix 1.2: Create stockpile filter table
# abcd12b Fix 1.1: Consolidate workshop names
# abcd12c Fix 0.2: Hardcode V16/V17 ITEM_TYPE_COUNT
# abcd12d Fix 0.1: Fix cellTypeNames missing CELL_ROCK

# Revert a specific fix:
git revert abcd124  # Revert grid macro while keeping others

# Revert entire phase:
git revert abcd127..abcd125  # Revert all of Phase 2
```

### Emergency Rollback Procedure
1. Identify broken commit via `git log` or `git bisect`
2. `git revert <commit-hash>`
3. `make clean && make path && make test`
4. Verify system stable
5. Create issue to track fix and re-attempt

---

## Open Questions & Decisions

### Must resolve before starting:

1. **Q: V16/V17 ITEM_TYPE_COUNT historical values?**
   - A: Check `git log src/entities/item_defs.h` or measure binary saves
   - Required for: Fix 0.2
   - Estimated values: V16=20, V17=21

2. **Q: Cell placement water-clearing behavior intentional?**
   - ExecuteBuildDirt/Soil don't clear water, ExecuteBuildWall/Rock do
   - A: Document as intentional OR unify behavior
   - Required for: Fix 2.2

3. **Q: Should ITEM_LEAVES_* have stockpile filters?**
   - Currently omitted from keybindings and tooltips
   - A: Document if intentional exclusion
   - Required for: Fix 1.2 (scope decision)

### Decide during implementation:

4. **Q: Grid macro - atomic or incremental?**
   - Option A: Convert all 16 grids in one commit (faster, higher risk)
   - Option B: Convert 2-3 grids per commit (slower, safer)
   - Recommendation: Option B for first attempt, Option A if no issues found

5. **Q: InputAction registry - prototype first?**
   - Option A: Full 47-action conversion in one pass
   - Option B: Prototype with 10 actions, validate, then convert rest
   - Recommendation: Option B given complexity and risk

6. **Q: Should Fix 3.1 and 3.2 be done at all?**
   - These are optional deep refactors with multi-week payback period
   - Decide based on: development velocity, frequency of grid/system additions
   - Recommendation: Skip unless actively adding many grids or systems

---

## Success Criteria

### Minimum viable completion (Phase 0-1):
- ✅ cellTypeNames bug fixed (Pattern 7)
- ✅ V16/V17 time bomb defused (Pattern 2)
- ✅ Workshop names unified (Pattern 9)
- ✅ Stockpile filters unified (Pattern 6)
- ✅ Settings macro added (Pattern 3)
- ✅ Sim counter self-heal added (Pattern 8)
- ✅ All tests pass
- ✅ Save/load works for V16-V22

### Recommended completion (Phase 0-2):
- ✅ All Phase 0-1 criteria
- ✅ Save migration header shared (Pattern 1)
- ✅ Cell placement helper added (Pattern 5)
- ✅ InputAction registry added (Pattern 4, 11)
- ✅ Full UI smoke test passes
- ✅ Net ~200 LOC reduction
- ✅ Parallel update patterns reduced from 12 → ~5

### Stretch completion (Phase 0-3):
- ✅ All Phase 0-2 criteria
- ✅ Grid save/load macro (Pattern 12)
- ✅ Rebuild registry (Pattern 10)
- ✅ All 12 patterns addressed
- ✅ Net ~300 LOC reduction
- ✅ Parallel update patterns reduced to ~3

---

## Estimated Timeline

### Conservative (thorough testing):
- **Week 1**: Phase 0 (30 min) + Phase 1 (3 hours) = ~4 hours
- **Week 2**: Phase 2 (8-10 hours spread across week)
- **Week 3**: Testing, bug fixes, documentation
- **Week 4** (optional): Phase 3 (4-6 hours + extensive testing)

### Aggressive (experienced developer):
- **Day 1-2**: Phase 0 + Phase 1 (4 hours)
- **Day 3-5**: Phase 2 (10 hours)
- **Day 6-7**: Integration testing
- **Day 8-10** (optional): Phase 3

### Recommended Approach:
- Complete Phase 0 immediately (30 min)
- Complete Phase 1 over next few days (3 hours)
- Evaluate results, then decide whether to proceed to Phase 2
- Skip Phase 3 unless high development velocity justifies it

---

## Next Steps

1. **Immediate**: Execute Fix 0.1 (cellTypeNames bug) - 5 minutes
2. **Today**: Execute Fix 0.2 (V16/V17 structs) - 15 minutes
3. **This week**: Execute Phase 1 fixes (low-hanging fruit) - 3 hours
4. **Next week**: Evaluate Phase 1 results and decide on Phase 2
5. **Ongoing**: Commit each fix separately for easy rollback

**First command to run**:
```bash
# Start with the active bug
vim src/core/inspect.c  # Fix cellTypeNames array
make clean && make path
./bin/path --inspect <save_file>
git commit -m "Fix 0.1: Fix cellTypeNames missing CELL_ROCK"
```
