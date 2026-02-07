# Comprehensive Plan: Fix src/entities/ Structural Issues

Date: 2026-02-07  
Source: Merged from structure-entities.md audit, entities-opus.md, and entities-outcome.md

## Overview

The structural audit of `src/entities/` found 15 patterns ranging from actual bugs to architectural coordination burdens. This plan provides a phased approach to address them, from quick safety fixes to longer-term architectural improvements.

**Key insight**: Many "inherent architecture" problems can be improved through **consolidation** (tables, shared headers) without major rewrites. We're not eliminating coordination points, just making them explicit and centralized.

---

## Goals
- Fix actual bugs (RemoveBill reservation leak)
- Reduce duplication (safe-drop, stockpile slot cleanup)
- Centralize coordination points (designation specs, workshop defs, save migrations)
- Add compile-time safety where possible (static assertions)
- Enable incremental progress without destabilizing saves

## Non-Goals
- Rewriting the entire entity system or removing global arrays
- Changing gameplay behavior or job priorities beyond what refactors require
- Introducing new save formats in the same pass as large architectural refactors

---

## Triage Summary

### Easy (1-3 days each, low risk)
- Pattern 5: RemoveBill reimplements CancelJob → **expose shared cancellation**
- Pattern 4: Safe-drop duplicated → **extract helper**
- Pattern 3: Stockpile slot cleanup duplicated → **single public API**
- Pattern 12: jobDrivers[] coverage → **static assertion**
- Pattern 13: DefaultMaterialForItemType coverage → **debug warning**

### Medium (3-10 days, moderate risk)
- Pattern 6: WorkshopType fan-out → **WorkshopDef table**
- Pattern 1 + 15: DesignationType fan-out → **DesignationJobSpec table**
- Pattern 7: saveload/inspect parallel → **shared save_migrations.h header**
- Pattern 10: Stockpile slot state → **StockpileSlot helpers**

### Hard (multi-week or save migration risk)
- Pattern 2: ItemType → save format change for `allowedTypes`
- Pattern 9: items[] direct mutation → API enforcement audit
- Pattern 11: ClearMovers coordination → world reset orchestrator

### Defer / Not Worth Doing Now
- Pattern 14: Repeated move-to-target sequences (meaningful per-job differences)
- Pattern 8: CancelJob implicit contract (well-maintained, structurally sound for now)

---

## Phase 0: Safety Fixes & Quick Wins (1-2 weeks)

**Goal**: Fix actual bugs and eliminate obvious duplication with minimal risk.

### Fix 0.1: Make CancelJob public, use from RemoveBill (Pattern 5 - HIGH)

**Bug**: `RemoveBill` in workshops.c reimplements CancelJob but misses `targetItem` release. If a craft job is in CRAFT_STEP_WORKING with a deposited input item, removing the bill leaks that item's reservation forever.

**Changes**:
1. `src/entities/jobs.c`:
   - Change `CancelJob` from `static` to public
   - Remove forward declaration at line ~2043
2. `src/entities/jobs.h`:
   - Add `void CancelJob(Mover* m, int moverIdx);` declaration
3. `src/entities/workshops.c:RemoveBill()`:
   - Replace inline cancellation (~30 lines, lines ~296-360) with `CancelJob(&movers[moverIdx], moverIdx)`

**Verification**:
- Write test: Create craft job at CRAFT_STEP_WORKING with targetItem set, remove bill, verify item's reservedBy is cleared
- Run full test suite

---

### Fix 0.2: Extract SafeDropItem helper (Pattern 4 - MEDIUM)

**Problem**: 20-line safe-drop pattern (check walkable, search 8 neighbors, fallback) is copy-pasted for carryingItem and fuelItem in CancelJob (lines ~2237-2268, ~2293-2322).

**Changes**:
1. `src/entities/jobs.c`:
   - Extract `static void SafeDropItem(int itemIdx, Mover* m)` that:
     - Sets `item->state = ITEM_ON_GROUND`
     - Sets `item->reservedBy = -1`
     - Checks `IsCellWalkableAt` at mover position
     - Searches 8 neighbors (cardinal + diagonal) if not walkable
     - Falls back to mover position if no walkable neighbor
   - Replace both carryingItem and fuelItem safe-drop blocks with `SafeDropItem(job->carryingItem, m)` and `SafeDropItem(job->fuelItem, m)`

**Verification**:
- Existing tests should cover this (test_jobs)
- Manually verify safe-drop behavior in-game

---

### Fix 0.3: Consolidate stockpile slot cleanup (Pattern 3 - MEDIUM)

**Problem**: Stockpile slot removal logic duplicated across 5 sites:
1. `stockpiles.c:RemoveItemFromStockpileSlot()` (canonical)
2. `jobs.c:ClearSourceStockpileSlot()` (helper)
3. `jobs.c:RunJob_Haul()` (inline)
4. `jobs.c:RunJob_Craft CRAFT_STEP_PICKING_UP` (inline)
5. `jobs.c:RunJob_Craft CRAFT_STEP_PICKING_UP_FUEL` (inline)

**Changes**:
1. `src/entities/stockpiles.h`:
   - Ensure `RemoveItemFromStockpileSlot(int x, int y, int z)` is declared public
2. `src/entities/jobs.c`:
   - Replace `ClearSourceStockpileSlot()` body with call to `RemoveItemFromStockpileSlot(item->x, item->y, (int)item->z)`
   - Replace inline slot cleanup in RunJob_Haul (~lines 522-532) with `RemoveItemFromStockpileSlot()`
   - Replace inline slot cleanup in RunJob_Craft CRAFT_STEP_PICKING_UP (~lines 1770-1790) with `RemoveItemFromStockpileSlot()`
   - Replace inline slot cleanup in RunJob_Craft CRAFT_STEP_PICKING_UP_FUEL (~lines 1900-1910) with `RemoveItemFromStockpileSlot()`

**Verification**:
- Run test suite, especially stockpile and craft tests
- Verify hauling and crafting work correctly in-game

---

### Fix 0.4: Add static assertion for jobDrivers[] (Pattern 12)

**Problem**: No compile-time check that jobDrivers[] has an entry for every JobType. Missing entries silently return NULL and cancel jobs.

**Changes**:
1. `src/entities/jobs.c`:
   - After jobDrivers[] definition (line ~2024), add:
   ```c
   _Static_assert(ARRAY_LEN(jobDrivers) >= JOBTYPE_COUNT, 
                  "jobDrivers[] must have entry for every JobType");
   ```
   - Note: Uses `>=` because designated initializers may leave gaps, but JOBTYPE_COUNT is the minimum

**Verification**:
- Build should succeed with current code
- Test by temporarily removing a driver entry - build should fail

---

### Fix 0.5: Add debug warning for DefaultMaterialForItemType (Pattern 13)

**Problem**: DefaultMaterialForItemType has `default: return MAT_NONE`. New ItemTypes without explicit cases silently get wrong material.

**Changes**:
1. `src/entities/items.c:DefaultMaterialForItemType()`:
   - In the `default` case, add:
   ```c
   default:
       #ifdef DEBUG
       LOG_WARNING("DefaultMaterialForItemType: unhandled ItemType %d, returning MAT_NONE", itemType);
       #endif
       return MAT_NONE;
   ```

**Verification**:
- Build in debug mode, verify existing types don't trigger warning
- Test by adding a new ItemType without a case - should see warning

---

## Phase 1: Structural Tables (2-4 weeks)

**Goal**: Centralize coordination points into data tables, reducing the number of scattered update sites.

### Fix 1.1: Add WorkshopDef table (Pattern 6)

**Problem**: Adding a WorkshopType requires updates to workshopTemplates[], GetRecipesForWorkshop() switch, rendering, tooltips - 8 coordinated changes.

**Changes**:
1. `src/entities/workshops.h`:
   ```c
   typedef struct {
       const char* name;
       const char* template;
       const Recipe* recipes;
       int recipeCount;
       // Optional: sprite overrides, tooltip data
   } WorkshopDef;
   
   extern const WorkshopDef workshopDefs[WORKSHOP_TYPE_COUNT];
   ```

2. `src/entities/workshops.c`:
   - Define `workshopDefs[]` table with entries for each WorkshopType:
   ```c
   const WorkshopDef workshopDefs[WORKSHOP_TYPE_COUNT] = {
       [WORKSHOP_CARPENTRY] = {
           .name = "Carpentry",
           .template = "###\n"
                      "#X#\n"
                      "#O#",
           .recipes = carpentryRecipes,
           .recipeCount = ARRAY_LEN(carpentryRecipes)
       },
       // ... etc for WORKSHOP_MASONRY, WORKSHOP_SMELTER
   };
   ```
   - Update `GetRecipesForWorkshop()` to read from table instead of switch
   - Replace `workshopTemplates[]` usage in `CreateWorkshop()` with `workshopDefs[type].template`

3. `src/render/tooltips.c`:
   - Use `workshopDefs[ws->type].name` for display

**Verification**:
- Add `_Static_assert(ARRAY_LEN(workshopDefs) == WORKSHOP_TYPE_COUNT, ...)`
- Test workshop creation and recipe display
- Verify tooltips show correct names

---

### Fix 1.2: Add DesignationJobSpec table (Pattern 1 + Pattern 15)

**Problem**: Adding a DesignationType requires 14 coordinated changes across jobs.c (cache arrays, rebuild functions, InvalidateDesignationCache switch, AssignJobs priority chain, etc.).

**Changes**:
1. `src/entities/jobs.c`:
   ```c
   typedef struct {
       DesignationType desigType;
       JobType jobType;
       void (*RebuildCache)(void);
       int (*WorkGiver)(int moverIdx);
       DesignationCell** cacheArray;    // pointer to the static cache array
       int* cacheCount;                  // pointer to the static count
       bool* cacheDirty;                 // pointer to the static dirty flag
   } DesignationJobSpec;
   
   static DesignationJobSpec designationSpecs[] = {
       {DESIGNATION_MINE, JOBTYPE_MINE, RebuildMineDesignationCache, WorkGiver_Mine, 
        &mineDesignationCache, &mineDesignationCount, &mineDesignationsDirty},
       {DESIGNATION_CHANNEL, JOBTYPE_CHANNEL, RebuildChannelDesignationCache, WorkGiver_Channel,
        &channelDesignationCache, &channelDesignationCount, &channelDesignationsDirty},
       // ... etc for all 9 designation types
   };
   ```

2. Update `InvalidateDesignationCache(DesignationType type)`:
   - Replace switch with loop over `designationSpecs[]` matching `desigType`
   - Set corresponding `*cacheDirty = true`

3. Update `AssignJobs()`:
   - Replace the `has*Work` booleans + rebuild calls with:
   ```c
   // Rebuild all designation caches that are dirty
   for (int i = 0; i < ARRAY_LEN(designationSpecs); i++) {
       if (*designationSpecs[i].cacheDirty) {
           designationSpecs[i].RebuildCache();
       }
   }
   ```
   - Replace the priority chain if-else ladder with:
   ```c
   // Try WorkGivers in priority order (table order = priority)
   if (jobId < 0) {
       for (int i = 0; i < ARRAY_LEN(designationSpecs); i++) {
           if (*designationSpecs[i].cacheCount > 0) {
               jobId = designationSpecs[i].WorkGiver(idxMover);
               if (jobId >= 0) break;
           }
       }
   }
   ```

**Note**: Table order defines priority. Maintain current priority by keeping table entries in same order as current if-else chain.

**Verification**:
- Add `_Static_assert` comparing table size to expected designation count
- Test job assignment for all designation types
- Verify priority order matches previous behavior

---

### Fix 1.3: Unify save migration structs (Pattern 7)

**Problem**: saveload.c and inspect.c both define legacy structs (V18_ITEM_TYPE_COUNT, V19_ITEM_TYPE_COUNT, etc.) and can drift. Currently both at version 22 but with different constant names.

**Changes**:
1. Create `src/core/save_migrations.h`:
   ```c
   #ifndef SAVE_MIGRATIONS_H
   #define SAVE_MIGRATIONS_H
   
   // Version constants
   #define CURRENT_SAVE_VERSION 22
   
   // Legacy ITEM_TYPE_COUNT values for each version
   #define V19_ITEM_TYPE_COUNT 23
   #define V18_ITEM_TYPE_COUNT 21
   #define V17_ITEM_TYPE_COUNT 19  // example, adjust to actual
   #define V16_ITEM_TYPE_COUNT 18  // example, adjust to actual
   
   // Legacy struct definitions
   typedef struct {
       bool allowedTypes[V19_ITEM_TYPE_COUNT];
       bool allowedMaterials[MAX_MATERIAL_TYPES];
       // ... rest of V19 Stockpile fields
   } Stockpile_V19;
   
   typedef struct {
       bool allowedTypes[V18_ITEM_TYPE_COUNT];
       // ... rest of V18 Stockpile fields
   } Stockpile_V18;
   
   // ... etc for other versioned structs
   
   #endif
   ```

2. `src/core/saveload.c`:
   - Replace local V*_ITEM_TYPE_COUNT definitions with `#include "save_migrations.h"`
   - Use `Stockpile_V19`, `Stockpile_V18` structs from header
   - Replace `SAVE_VERSION` constant with `CURRENT_SAVE_VERSION`

3. `src/core/inspect.c`:
   - Replace local INSPECT_V*_ITEM_TYPE_COUNT definitions with `#include "save_migrations.h"`
   - Use shared legacy struct definitions
   - Replace `INSPECT_SAVE_VERSION` with `CURRENT_SAVE_VERSION`

**Verification**:
- Both saveload and inspect should compile
- Load an old save (V19, V18) and verify migration works in both game and inspector
- `./bin/path --inspect save.bin` should show correct data

---

### Fix 1.4: Add StockpileSlot helpers (Pattern 10)

**Problem**: Stockpile slot state has 5 coordinating fields (slots, slotCounts, slotTypes, slotMaterials, reservedBy). Updates are scattered and error-prone.

**Changes**:
1. `src/entities/stockpiles.h`:
   ```c
   // Helper functions for slot manipulation
   void IncrementStockpileSlot(Stockpile* sp, int slotIdx, int itemIdx, ItemType type, MaterialType mat);
   void DecrementStockpileSlot(Stockpile* sp, int slotIdx);
   void ClearStockpileSlot(Stockpile* sp, int slotIdx);
   ```

2. `src/entities/stockpiles.c`:
   - Implement helpers:
   ```c
   void IncrementStockpileSlot(Stockpile* sp, int slotIdx, int itemIdx, ItemType type, MaterialType mat) {
       if (sp->slotCounts[slotIdx] == 0) {
           sp->slots[slotIdx] = itemIdx;
           sp->slotTypes[slotIdx] = type;
           sp->slotMaterials[slotIdx] = mat;
       }
       sp->slotCounts[slotIdx]++;
   }
   
   void DecrementStockpileSlot(Stockpile* sp, int slotIdx) {
       if (sp->slotCounts[slotIdx] > 0) {
           sp->slotCounts[slotIdx]--;
           if (sp->slotCounts[slotIdx] == 0) {
               ClearStockpileSlot(sp, slotIdx);
           }
       }
   }
   
   void ClearStockpileSlot(Stockpile* sp, int slotIdx) {
       sp->slots[slotIdx] = -1;
       sp->slotTypes[slotIdx] = -1;
       sp->slotMaterials[slotIdx] = MAT_NONE;
       sp->slotCounts[slotIdx] = 0;
   }
   ```
   - Update `PlaceItemInStockpile()` to use `IncrementStockpileSlot()`
   - Update `RemoveItemFromStockpileSlot()` to use `DecrementStockpileSlot()`

**Note**: This is incremental - existing code continues to work, but new code should use helpers.

**Verification**:
- Test stockpile operations (hauling, removing items, deleting stockpiles)
- Verify slot counts remain accurate

---

## Phase 2: High-Impact Migrations (Optional, multi-week)

**Goal**: Address deep architectural issues that require save format changes or extensive audits.

### Fix 2.1: Decouple ItemType from Stockpile save layout (Pattern 2)

**Problem**: `Stockpile.allowedTypes[ITEM_TYPE_COUNT]` is embedded in the saved struct. Adding a new ItemType changes struct size, requiring version bump and migration in both saveload.c and inspect.c.

**Options**:

**Option A: Variable-length allowedTypes blob**
- Save `allowedTypes` as `{ int count; bool types[count]; }`
- On load, resize to current `ITEM_TYPE_COUNT`, default new types to true
- Pros: Clean, extensible
- Cons: Requires new serialization logic, all existing saves need migration

**Option B: Fixed-size bitset with remap layer**
- Replace `bool allowedTypes[ITEM_TYPE_COUNT]` with `uint64_t allowedTypesBits[4]` (supports 256 types)
- Add enum-to-bit mapping layer
- Pros: Fixed struct size, no migration on enum growth
- Cons: Bit manipulation, 256 type limit

**Option C: Separate allowedTypes serialization**
- Move `allowedTypes` out of `Stockpile` struct into parallel array `stockpileFilters[]`
- Save separately from main Stockpile data
- Pros: Decouples from struct layout
- Cons: More complex save logic, two storage locations

**Recommendation**: Start with Option B (bitset) as it has fixed struct size and reasonable limits.

**Verification**:
- Requires extensive migration testing
- Test saves from each previous version
- Verify new ItemTypes default correctly

---

### Fix 2.2: Audit items[] direct mutations (Pattern 9)

**Problem**: `items[]` state (especially `state` and `reservedBy`) is directly mutated from 11+ files. Adding side effects to state transitions requires finding every mutation site.

**Approach**:
1. Audit all direct writes to `items[].state` and `items[].reservedBy`
2. Create API functions in items.c:
   ```c
   void SetItemState(int itemIdx, ItemState newState);
   void ReserveItem(int itemIdx, int moverIdx);
   void ReleaseItemReservation(int itemIdx);  // already exists, but not always used
   ```
3. Gradually migrate call sites to use API
4. Add `#ifdef STRICT_ITEM_API` mode that makes `items[]` opaque (requires getters)

**This is a long audit** - defer until other phases are stable.

---

### Fix 2.3: Unified world reset orchestrator (Pattern 11)

**Problem**: `ClearMovers()` must coordinate with 5+ subsystems. Adding new reservation types requires updating ClearMovers.

**Approach**:
1. Create `src/core/world_reset.h`:
   ```c
   void RegisterResetHook(void (*hook)(void));
   void ResetWorld(void);
   ```
2. Each subsystem registers cleanup hooks
3. `ResetWorld()` calls all hooks in dependency order

**This is architectural** - defer until current patterns stabilize.

---

## Phase 3: Deferred Items

### Pattern 14: Move-to-target sequences
- **Decision**: Defer. The sequences have meaningful per-job differences (validation, z-level handling). Extracting risks removing needed variation.
- **Revisit if**: A bug pattern emerges across multiple drivers

### Pattern 8: CancelJob implicit contract
- **Decision**: Defer. Currently well-maintained and comprehensive. Document the contract in comments rather than enforcing it structurally.
- **Revisit if**: A reservation leak occurs due to missing cleanup

---

## Execution Order

1. **Phase 0** (all fixes can be done in parallel):
   - 0.1 (RemoveBill) - most urgent, actual bug
   - 0.2 (SafeDropItem) - quick win
   - 0.3 (stockpile cleanup) - quick win
   - 0.4 (static assertion) - trivial
   - 0.5 (debug warning) - trivial

2. **Phase 1** (do in order):
   - 1.1 (WorkshopDef table) - isolated, good practice for 1.2
   - 1.2 (DesignationJobSpec table) - largest impact on maintainability
   - 1.3 (save migrations header) - prevents future drift
   - 1.4 (StockpileSlot helpers) - incremental improvement

3. **Phase 2** (choose based on need):
   - 2.1 if ItemType additions become frequent
   - 2.2 if items[] state bugs emerge
   - 2.3 if reservation types proliferate

---

## Risks and Mitigations

- **Save format changes**: Isolate to Phase 2, add versioned migrations with explicit tests
- **Behavioral drift in job assignment**: In Phase 1.2, preserve existing priority order by encoding table in same order as current if-else chain
- **Refactor scope creep**: Keep changes in small phases with limited files per PR/commit
- **Test coverage**: After each fix, run `make test` and verify specific affected systems in-game

---

## Validation Plan

### Per-fix validation
- Each fix has specific verification steps (see individual fix sections)

### Cross-cutting validation
- Add debug-only check that `CancelJob()` releases all reserved fields (assert or log in debug builds)
- Add quick save/load round-trip test for entities after any migration changes
- Create repro test for RemoveBill targetItem reservation leak (Phase 0.1)

### Regression testing
- After Phase 0: Run full test suite, verify crafting and hauling in-game
- After Phase 1: Test job assignment, workshop creation, save/load/inspect
- After Phase 2: Migration stress test (load saves from every previous version)

---

## Open Questions

1. **CancelJob exposure** (Phase 0.1): Expose publicly in jobs.h, or keep internal and add narrower helper for RemoveBill?
   - **Recommendation**: Expose publicly - other systems may need it too

2. **DesignationJobSpec priority** (Phase 1.2): Should table include explicit priority values or be ordered implicitly by array position?
   - **Recommendation**: Implicit (array order) - simpler, matches current pattern

3. **Stockpile allowedTypes migration** (Phase 2.1): Which option (A, B, or C)?
   - **Recommendation**: Option B (bitset) - fixed size, reasonable limits, no ongoing migration cost

4. **Phase execution**: Should we do all of Phase 0 before starting Phase 1, or interleave?
   - **Recommendation**: Complete Phase 0 first - establishes safety baseline before larger refactors

---

## Success Metrics

- **Phase 0**: RemoveBill reservation leak fixed, duplication reduced, compile-time checks added
- **Phase 1**: Adding a new WorkshopType or DesignationType requires 2-3 changes instead of 8-14
- **Phase 2**: Adding new ItemTypes doesn't require version bumps (if 2.1 completed)
- **Overall**: Test suite remains green, saves load correctly, no new reservation leaks

---

## Estimated Timeline

- **Phase 0**: 1-2 weeks (5 small fixes, can parallelize)
- **Phase 1**: 3-6 weeks (4 medium refactors, some dependencies)
- **Phase 2**: 4-8 weeks per fix (optional, choose based on need)

**Total for Phases 0-1**: ~2 months of focused work, assuming single developer
