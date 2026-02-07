# Structural Audit: src/entities/

Date: 2026-02-07

## File Discovery

### Core files (src/entities/)
- `item_defs.h` / `item_defs.c` -- Item definition table (flags, sprites, stacking)
- `items.h` / `items.c` -- Item pool, spatial grid, spawn/delete/reserve
- `stockpiles.h` / `stockpiles.c` -- Stockpile pool, slot management, caches
- `workshops.h` / `workshops.c` -- Workshop pool, recipes, bills
- `jobs.h` / `jobs.c` -- Job pool, job drivers, WorkGivers, AssignJobs, CancelJob
- `mover.h` / `mover.c` -- Mover pool, movement, spatial grid, Tick loop

### Dependencies (systems entities call into)
- `src/world/grid.h` / `grid.c` -- World grid, cell types, walkability
- `src/world/cell_defs.h` -- Cell type queries (IsCellWalkableAt, CellBlocksMovement, etc.)
- `src/world/pathfinding.h` -- FindPath, HPA* graph
- `src/world/designations.h` / `designations.c` -- Designation grid, completion functions
- `src/world/material.h` -- MaterialType enum, material queries
- `src/simulation/trees.c` -- Tree types, sapling placement
- `src/simulation/water.c`, `fire.c`, `smoke.c`, `steam.c`, `temperature.c`, `groundwear.c` -- Simulation ticks called from mover.c:TickWithDt

### Dependents (systems that call into entities)
- `src/core/saveload.c` -- Save/load all entity state with version migration
- `src/core/inspect.c` -- Debug inspection with parallel save migration
- `src/core/input.c` -- Workshop creation, stockpile management
- `src/render/rendering.c` -- Renders items, movers, jobs, workshops, stockpiles
- `src/render/tooltips.c` -- Tooltip display for workshops, stockpiles, items
- `src/render/ui_panels.c` -- UI panels that call Clear* functions
- `src/world/terrain.c` -- World generation calls Clear* functions
- `src/world/designations.c` -- Reads items[] for spawn-on-mine

### Coupled via shared state (direct global access)
- `items[]` / `itemHighWaterMark` / `itemCount`: accessed from jobs.c, stockpiles.c, workshops.c, mover.c, designations.c, saveload.c, inspect.c, rendering.c, tooltips.c, trees.c, groundwear.c
- `movers[]` / `moverCount`: accessed from jobs.c, stockpiles.c, workshops.c, saveload.c, inspect.c, rendering.c, tooltips.c, ui_panels.c, input.c, designations.c
- `stockpiles[]` / `stockpileCount`: accessed from jobs.c, workshops.c, mover.c, saveload.c, inspect.c, rendering.c, tooltips.c, input.c
- `workshops[]` / `workshopCount`: accessed from jobs.c, mover.c, saveload.c, inspect.c, rendering.c, tooltips.c, input.c
- `jobs[]` / `jobHighWaterMark` / `activeJobList` / `activeJobCount`: accessed from workshops.c, mover.c, stockpiles.c, saveload.c, inspect.c, rendering.c, tooltips.c

---

## Findings

### Pattern 1: Adding a new DesignationType requires 9+ coordinated changes in jobs.c alone

- **Type**: Parallel update / Growing dispatch
- **Where**:
  1. `src/world/designations.h`: Add enum value to `DesignationType`
  2. `src/entities/jobs.h`: Add `JOBTYPE_*` enum value
  3. `src/entities/jobs.h`: Declare `RunJob_*` driver function
  4. `src/entities/jobs.h`: Declare `WorkGiver_*` function
  5. `src/entities/jobs.c`: Add static cache array + count + dirty flag (3 declarations per cache type)
  6. `src/entities/jobs.c`: Add `Rebuild*DesignationCache()` function
  7. `src/entities/jobs.c`: Add `case DESIGNATION_*` to `InvalidateDesignationCache()` switch (line ~252)
  8. `src/entities/jobs.c`: Write `RunJob_*` driver function (~50-80 lines)
  9. `src/entities/jobs.c`: Add entry to `jobDrivers[]` table (line ~2024)
  10. `src/entities/jobs.c`: Write `WorkGiver_*` function (~60-100 lines)
  11. `src/entities/jobs.c`: Add `Rebuild*DesignationCache()` call in `AssignJobs()` (line ~2783)
  12. `src/entities/jobs.c`: Add `has*Work` bool check in `AssignJobs()` (line ~2793)
  13. `src/entities/jobs.c`: Add `WorkGiver_*()` call in priority chain in `AssignJobs()` (line ~2832)
  14. `src/render/rendering.c`: Add `case JOBTYPE_*` to job rendering switch
- **Risk**: Missing any one of these 14 sites means the new designation either never gets assigned, never runs, never caches, never invalidates on removal, or never renders. The `InvalidateDesignationCache` switch is particularly easy to miss because it silently falls through to `default: break`.
- **Current drift**: None found -- all 9 current designation types are present in all sites. But the pattern has been replicated 9 times already, making it increasingly error-prone.
- **Severity**: High -- this is the most frequently extended pattern in the codebase. Every new terrain interaction feature (chop, channel, dig ramp, remove ramp, etc.) went through this exact gauntlet.

---

### Pattern 2: Adding a new ItemType requires updates across 6+ files

- **Type**: Parallel update / Growing dispatch
- **Where**:
  1. `src/entities/items.h`: Add enum value to `ItemType` (before `ITEM_TYPE_COUNT`)
  2. `src/entities/item_defs.c`: Add row to `itemDefs[ITEM_TYPE_COUNT]` table (name, sprite, flags, maxStack)
  3. `src/entities/items.c`: Add case to `DefaultMaterialForItemType()` switch (~25 cases currently)
  4. `src/entities/items.h`: Possibly update `IsSaplingItem()`, `IsLeafItem()`, or `ItemTypeUsesMaterialName()` inline functions if the new type belongs to a group
  5. `src/core/saveload.c`: Bump `SAVE_VERSION`, add new `V*_ITEM_TYPE_COUNT` constant, write new legacy Stockpile struct for old version path
  6. `src/core/inspect.c`: Bump `INSPECT_SAVE_VERSION`, add matching `INSPECT_V*_ITEM_TYPE_COUNT`, write parallel legacy Stockpile struct
  7. `src/entities/stockpiles.h`: `Stockpile.allowedTypes[ITEM_TYPE_COUNT]` array auto-sizes but existing saves need migration
  8. `assets/atlas.h`: Need a `SPRITE_*` constant for the new item
- **Risk**: The `Stockpile` struct contains `bool allowedTypes[ITEM_TYPE_COUNT]`. Because stockpiles are saved as binary blobs, adding a new item type changes the struct size, requiring a new legacy struct in both `saveload.c` AND `inspect.c`. Forgetting `inspect.c` causes the debug inspector to crash or show corrupt data. Forgetting `DefaultMaterialForItemType` causes the new item to spawn with `MAT_NONE`.
- **Current drift**: The V17 and V16 legacy Stockpile structs in `saveload.c` (lines ~821, ~865) use `ITEM_TYPE_COUNT` instead of hardcoded constants. This is a latent bug documented in MEMORY.md -- it works only because no save files from those versions are expected to still exist with different enum counts. The V18 and V19 paths correctly use `V18_ITEM_TYPE_COUNT` and `V19_ITEM_TYPE_COUNT`.
- **Severity**: High -- has already caused bugs (per MEMORY.md). Every new item type triggers this cascade.

---

### Pattern 3: Stockpile slot cleanup logic duplicated across 4+ code paths

- **Type**: Repeated code sequence
- **Where**:
  1. `src/entities/stockpiles.c:RemoveItemFromStockpileSlot()` (lines ~670-685) -- The canonical version
  2. `src/entities/jobs.c:ClearSourceStockpileSlot()` (lines ~2200-2215) -- Local helper, same logic
  3. `src/entities/jobs.c:RunJob_Haul()` (lines ~522-532) -- Inline copy for source stockpile slot clearing during re-haul pickup
  4. `src/entities/jobs.c:RunJob_Craft() CRAFT_STEP_PICKING_UP` (lines ~1770-1790) -- Inline copy scanning all stockpiles
  5. `src/entities/jobs.c:RunJob_Craft() CRAFT_STEP_PICKING_UP_FUEL` (lines ~1900-1910) -- Same inline copy for fuel pickup
- **The pattern**: Find stockpile containing item by position -> compute local slot index -> decrement `slotCounts[idx]` -> if zero, clear `slotTypes[idx]=-1`, `slots[idx]=-1`, `slotMaterials[idx]=MAT_NONE`
- **Risk**: If the slot cleanup logic changes (e.g., a new field is added to slot state), some copies will be updated and others won't. The CRAFT_STEP_PICKING_UP copies (4, 5) do a full `MAX_STOCKPILES` scan instead of using `IsPositionInStockpile`, which is both slower and a different code path that could diverge.
- **Current drift**: The inline copies in RunJob_Craft (4, 5) use a different search pattern than `ClearSourceStockpileSlot` (3). Sites 4 and 5 scan all stockpiles with a nested loop, while site 3 uses `IsPositionInStockpile()`. Both reach the same result but through different paths. Also, `RemoveItemFromStockpileSlot` (1) resets `slotCounts` to 0 on underflow, while `ClearSourceStockpileSlot` (3) checks `<= 0`.
- **Severity**: Medium -- the divergent search patterns haven't caused a bug yet, but adding a new slot field (e.g., slot quality, slot age) would require updating all 5 sites.

---

### Pattern 4: CancelJob safe-drop pattern duplicated for carryingItem and fuelItem

- **Type**: Repeated code sequence
- **Where**:
  1. `src/entities/jobs.c:CancelJob()` -- carryingItem safe-drop (lines ~2237-2268)
  2. `src/entities/jobs.c:CancelJob()` -- fuelItem safe-drop (lines ~2293-2322)
- **The pattern**: Set `state = ITEM_ON_GROUND`, set `reservedBy = -1`, get cell coords, check `IsCellWalkableAt`, if walkable drop at mover position, else search 8 neighbors (cardinal+diagonal), if no walkable found drop at mover position anyway.
- **Risk**: If the safe-drop logic needs to change (e.g., handle z-level changes, notify stockpile system, trigger item spatial grid rebuild), both copies must be updated identically. The neighbor search arrays are independently declared (`dx[]`/`dy[]` for carryingItem, `fdx[]`/`fdy[]` for fuelItem).
- **Current drift**: None currently -- both copies are structurally identical. But the variable naming differs (`dx[]`/`dy[]` vs `fdx[]`/`fdy[]`, `found` vs `fFound`), suggesting they were copy-pasted and renamed rather than extracted to a shared function.
- **Severity**: Medium -- two copies is manageable but adding a third resource type (e.g., a tool item) would make this worse.

---

### Pattern 5: RemoveBill in workshops.c reimplements CancelJob

- **Type**: Repeated code sequence / Implicit contract
- **Where**:
  1. `src/entities/jobs.c:CancelJob()` (lines ~2220-2340) -- Canonical job cancellation with full reservation cleanup
  2. `src/entities/workshops.c:RemoveBill()` (lines ~296-360) -- Partial reimplementation, comment says "Need to call CancelJob, but it's static in jobs.c"
- **The pattern**: Release workshop reservation, release item reservations, drop carried items at mover position, release job, reset mover state, add to idle list.
- **Risk**: `RemoveBill` is a simplified copy of `CancelJob` that handles only craft job fields. If `CancelJob` gains new cleanup steps (e.g., releasing a new resource type, updating a new cache), `RemoveBill` won't have them. The comment explicitly acknowledges this is a workaround for `CancelJob` being `static`.
- **Current drift**: `RemoveBill` does NOT: (a) search 8 neighbors for safe-drop location (drops at mover position unconditionally), (b) release mine/designation reservations (not relevant for craft, but fragile if job types change), (c) call `InvalidateDesignationCache`. It also does not handle the `job->targetItem` field that `CancelJob` releases -- if a craft job has deposited input at the workshop (targetItem is set), `RemoveBill` will leave that item reserved.
- **Severity**: High -- the missing `targetItem` release in `RemoveBill` is a potential reservation leak. When a craft job is in `CRAFT_STEP_WORKING` with fuel, `targetItem` holds the deposited input item. If the bill is removed during this step, that item remains reserved forever.

---

### Pattern 6: Adding a new WorkshopType requires coordinated updates

- **Type**: Parallel update / Growing dispatch
- **Where**:
  1. `src/entities/workshops.h`: Add enum value to `WorkshopType` (before `WORKSHOP_TYPE_COUNT`)
  2. `src/entities/workshops.h`: Declare extern recipe array + count
  3. `src/entities/workshops.c`: Define recipe array + count
  4. `src/entities/workshops.c`: Add template string to `workshopTemplates[]` array
  5. `src/entities/workshops.c:GetRecipesForWorkshop()`: Add `case WORKSHOP_*` to switch (line ~262)
  6. `src/core/input.c`: Add workshop type to creation UI
  7. `src/render/rendering.c`: Add workshop rendering (if visually distinct)
  8. `src/render/tooltips.c`: Add workshop tooltip display
- **Risk**: No compile-time check ensures `workshopTemplates[]` has an entry for every `WorkshopType`. Missing a template entry means `CreateWorkshop` reads garbage memory for the template. Missing a `GetRecipesForWorkshop` case means the workshop has no recipes and the `default` branch returns NULL with `*outCount = 0`.
- **Current drift**: None -- only 3 workshop types exist. But the pattern is already established.
- **Severity**: Medium -- small enum currently, but the `workshopTemplates[]` array has no bounds checking and no compile-time size assertion.

---

### Pattern 7: saveload.c and inspect.c require parallel save migration code

- **Type**: Parallel update
- **Where**:
  1. `src/core/saveload.c`: `SAVE_VERSION`, legacy struct definitions, migration paths for each version
  2. `src/core/inspect.c`: `INSPECT_SAVE_VERSION`, parallel legacy struct definitions, parallel migration paths
- **What ties them together**: Both files parse the same binary save format. When a struct changes (e.g., Stockpile gains `allowedMaterials` in V17, or Blueprint gains `requiredItemType` in V21), both files need identical legacy struct definitions and migration logic.
- **Risk**: Updating `saveload.c` but forgetting `inspect.c` causes the inspector to misparse saves. The version constants are independently maintained (`SAVE_VERSION` vs `INSPECT_SAVE_VERSION`) so they can silently diverge.
- **Current drift**: Both are currently at version 22. The legacy constants are independently defined: `saveload.c` uses `V19_ITEM_TYPE_COUNT = 23`, `V18_ITEM_TYPE_COUNT = 21`; `inspect.c` uses `INSPECT_V19_ITEM_TYPE_COUNT = 23`, `INSPECT_V18_ITEM_TYPE_COUNT = 21`. Same values, different names -- which adds cognitive load and makes verification harder.
- **Severity**: High -- documented in MEMORY.md as a known gotcha. Has required parallel updates on every version bump.

---

### Pattern 8: CancelJob must release every resource type a job can hold

- **Type**: Implicit contract
- **Where**: `src/entities/jobs.c:CancelJob()` (lines ~2220-2340)
- **What must be paired**: Every field in the `Job` struct that represents a reservation must have a corresponding release in `CancelJob`:
  - `job->targetItem` -> `ReleaseItemReservation()`
  - `job->targetStockpile` + `targetSlotX/Y` -> `ReleaseStockpileSlot()`
  - `job->carryingItem` -> safe-drop + `reservedBy = -1`
  - `job->targetMineX/Y/Z` -> `designation.assignedMover = -1` + `InvalidateDesignationCache()`
  - `job->targetBlueprint` -> `bp->reservedItem = -1` or `bp->assignedBuilder = -1`
  - `job->fuelItem` -> safe-drop + `reservedBy = -1`
  - `job->targetWorkshop` -> `ws->assignedCrafter = -1`
- **Risk**: Adding a new resource type to the `Job` struct (e.g., a tool item, a secondary material) requires adding cleanup to `CancelJob`. Nothing in the code connects "this field was set in WorkGiver_X" with "this field must be released in CancelJob". A developer adding a field to `Job` and setting it in a WorkGiver could easily forget the matching cleanup in `CancelJob`.
- **Current drift**: None found -- all current fields are properly released. But this is the function that is most likely to have bugs when new features are added, because it's a single function that must know about every WorkGiver's reservation pattern.
- **Severity**: Medium -- well-maintained currently, but structurally fragile.

---

### Pattern 9: items[] directly accessed and mutated from 11+ files

- **Type**: Cross-file state coupling
- **Where** (files that directly read/write `items[]` fields):
  - `src/entities/items.c` -- Owner. Manages active/reservedBy/state/position.
  - `src/entities/jobs.c` -- Reads and writes `items[].state`, `items[].reservedBy`, `items[].x/y/z` (for carried items, safe-drop)
  - `src/entities/stockpiles.c` -- Reads and writes `items[].state` (ON_GROUND when deleting stockpile), `items[].reservedBy`
  - `src/entities/workshops.c` -- Reads and writes `items[].reservedBy`, `items[].state` (in RemoveBill)
  - `src/entities/mover.c` -- Reads and writes `items[].state` (drop on ClearMovers), `items[].reservedBy`, `items[].x/y/z`
  - `src/world/designations.c` -- Calls SpawnItem/SpawnItemWithMaterial (through function API)
  - `src/core/saveload.c` -- Reads/writes entire `items[]` array, modifies `items[].reservedBy`, `items[].material`
  - `src/core/inspect.c` -- Reads `items[]` for display
  - `src/render/rendering.c` -- Reads `items[]` for rendering
  - `src/render/tooltips.c` -- Reads `items[]` for display
  - `src/simulation/trees.c` -- Calls SpawnItemWithMaterial (through function API)
- **Risk**: The `items[].state` and `items[].reservedBy` fields are modified from 5 different files without going through the items.c API. For example, `jobs.c:CancelJob` sets `item->state = ITEM_ON_GROUND` and `item->reservedBy = -1` directly, bypassing `ReleaseItemReservation()` for the carried item. `stockpiles.c:RemoveStockpileCells` sets `items[i].state = ITEM_ON_GROUND` and `items[i].reservedBy = -1` directly. This means adding a side effect to state transitions (e.g., "update spatial grid when item changes state") would require finding every direct mutation site.
- **Current drift**: `jobs.c:CancelJob` manually sets `item->reservedBy = -1` for carryingItem (line ~2241) instead of calling `ReleaseItemReservation()`. The effect is the same, but if `ReleaseItemReservation` ever gained side effects, this site would be missed.
- **Severity**: Medium -- the direct access is widespread but currently consistent. The risk is future-facing.

---

### Pattern 10: Stockpile slot state managed by 3 coordinating data structures

- **Type**: Implicit contract
- **Where**:
  - `stockpiles[sp].slots[idx]` -- item index at slot
  - `stockpiles[sp].slotCounts[idx]` -- number of stacked items
  - `stockpiles[sp].slotTypes[idx]` -- ItemType of items in slot
  - `stockpiles[sp].slotMaterials[idx]` -- MaterialType of items in slot
  - `stockpiles[sp].reservedBy[idx]` -- reservation count
  - `stockpiles[sp].groundItemIdx[idx]` -- ground item cache
  - `stockpiles[sp].freeSlotCount` -- aggregate free slot count
- **What must be paired**: When an item enters or leaves a stockpile slot, up to 5 fields must be updated atomically:
  - Enter: `slots = itemIdx`, `slotCounts++`, `slotTypes = type`, `slotMaterials = mat`, `reservedBy--`
  - Leave: `slotCounts--`, then if zero: `slots = -1`, `slotTypes = -1`, `slotMaterials = MAT_NONE`
  - `freeSlotCount` is rebuilt each frame (not maintained incrementally), so it's less fragile
  - `groundItemIdx` is rebuilt each frame from item positions
- **Risk**: The enter path is consolidated in `PlaceItemInStockpile()`. But the leave path is scattered (Pattern 3 above). Missing any field on leave causes phantom items, type mismatches, or slots that appear occupied but are empty.
- **Severity**: Medium -- the per-frame rebuilds of `freeSlotCount` and `groundItemIdx` provide safety nets for some fields, but `slotCounts`/`slotTypes`/`slotMaterials` are not rebuilt and must be correct at all times.

---

### Pattern 11: ClearMovers must coordinate with 5 other subsystems

- **Type**: Implicit contract
- **Where**: `src/entities/mover.c:ClearMovers()` (lines ~606-670)
- **What it coordinates**:
  1. Drop carried items (accesses `items[]`, `jobs[]`)
  2. `ReleaseAllSlotsForMover()` (clears all stockpile reservations)
  3. Clear all `items[].reservedBy` (linear scan to `itemHighWaterMark`)
  4. Reset all designation `assignedMover` and `progress` (triple-nested z/y/x loop)
  5. Reset all blueprint `assignedBuilder`, `reservedItem`, `progress`
  6. Reset all workshop `assignedCrafter`
  7. `ClearJobs()` (resets entire job pool)
  8. `InitJobSystem(MAX_MOVERS)` (reinitializes idle mover cache)
  9. Reinitialize mover spatial grid
- **Risk**: If a new reservation type is added (e.g., movers reserving a water source, or a rest area), `ClearMovers` must be updated to release it. Nothing connects "WorkGiver_X reserves resource Y" with "ClearMovers must release Y". Callers of `ClearMovers` (terrain.c, ui_panels.c, saveload.c) assume it fully resets all mover-related state.
- **Current drift**: `ClearMovers` does not call `ClearWorkshops` or `ClearStockpiles` -- it expects those to be called separately by the caller. Some callers do both (`terrain.c:InitWorld` calls `ClearMovers`, `ClearItems`, `ClearWorkshops`, `ClearStockpiles`), while `saveload.c:LoadWorld` calls `ClearMovers` and `ClearJobs` but not `ClearStockpiles` or `ClearItems` (it overwrites them from the save file instead). This asymmetry is currently correct but fragile.
- **Severity**: Medium -- working correctly now, but the coordination burden grows with each new subsystem.

---

### Pattern 12: Job driver table (jobDrivers[]) must match JobType enum

- **Type**: Growing dispatch
- **Where**: `src/entities/jobs.c` line ~2024
- **Enum**: `JobType` (currently 14 entries + `JOBTYPE_NONE`)
- **Current size**: 15 entries
- **Compile-time check**: None. Uses designated initializers (`[JOBTYPE_HAUL] = RunJob_Haul`), which means missing entries are silently NULL. The runtime check is `if (!driver)` which triggers `CancelJob`, so a missing driver won't crash but will silently cancel jobs.
- **Risk**: Adding a `JOBTYPE_*` without a corresponding driver entry means the new job type is always cancelled immediately. The error is silent -- no warning, no crash, just jobs that never complete.
- **Current drift**: None -- all current types have drivers. But `JOBTYPE_CHOP_FELLED` is notably listed last in the enum but not last in `jobDrivers[]`, showing that entries are added in insertion order, not enum order. This is fine with designated initializers but suggests the table is maintained manually.
- **Severity**: Low -- designated initializers make this relatively safe, but the silent failure mode is concerning.

---

### Pattern 13: DefaultMaterialForItemType must have a case for every ItemType

- **Type**: Growing dispatch
- **Where**: `src/entities/items.c:DefaultMaterialForItemType()` (~30 lines)
- **Enum**: `ItemType` (24 values)
- **Current size**: 23 explicit cases + `default: return MAT_NONE`
- **Compile-time check**: None. The `default` case silently returns `MAT_NONE` for unhandled types.
- **Risk**: A new ItemType without a `DefaultMaterialForItemType` case will have `MAT_NONE` as its default material. This affects stockpile slot matching (items with `MAT_NONE` may not match material filters) and crafting (output material inheritance).
- **Severity**: Low -- the `default` fallback is reasonable for many item types, but items that should have a material (e.g., a new wood product) would silently get the wrong one.

---

### Pattern 14: The "move to target, check distance, pick up" sequence repeated across 9 job drivers

- **Type**: Repeated code sequence
- **Where**: Every haul-like job driver's STEP_MOVING_TO_PICKUP phase:
  1. `RunJob_Haul` (lines ~475-545)
  2. `RunJob_Clear` (lines ~555-670)
  3. `RunJob_HaulToBlueprint` (lines ~1475-1540)
  4. `RunJob_PlantSapling` (lines ~1280-1330)
  5. `RunJob_Craft CRAFT_STEP_MOVING_TO_INPUT` (lines ~1720-1760)
  6. `RunJob_Craft CRAFT_STEP_MOVING_TO_FUEL` (lines ~1840-1870)
  And every work-at-site driver's STEP_MOVING_TO_WORK phase:
  7. `RunJob_Mine` (lines ~850-900)
  8. `RunJob_Channel` (lines ~930-970)
  9. `RunJob_Chop` (lines ~1090-1130)
  10. (Plus DigRamp, RemoveFloor, RemoveRamp, ChopFelled, GatherSapling -- all structurally identical)
- **The pattern**: Check target still valid -> set goal if not already -> compute distance -> `TryFinalApproach()` -> check `IsPathExhausted && timeWithoutProgress > JOB_STUCK_TIME` -> if in range, advance step.
- **Risk**: Each copy has slight variations in validation checks (some check `IsCellWalkableAt` on the target, some don't; some set unreachable cooldowns, some set designation cooldowns). Adding a new behavior to the approach phase (e.g., abort if target is on fire) would require updating 10+ copies.
- **Current drift**: The `correctZ` check is present in work-at-site drivers but not in pickup drivers (pickup drivers assume z matches because items are filtered by z-level at assignment time). This is an intentional difference but not documented.
- **Severity**: Low -- the `TryFinalApproach` helper already extracted the most fragile part. The remaining boilerplate is annoying but unlikely to cause bugs because each driver's validation needs genuinely differ.

---

### Pattern 15: WorkGiver priority chain in AssignJobs is an open-coded if-else ladder

- **Type**: Growing dispatch
- **Where**: `src/entities/jobs.c:AssignJobs()` (lines ~2780-2880)
- **Pattern**: A long sequence of `if (jobId < 0 && has*Work) { jobId = WorkGiver_*(); }` calls, one per designation type, followed by blueprint WorkGivers.
- **Current size**: 11 WorkGiver calls in the priority chain (Mining, Channel, DigRamp, RemoveFloor, RemoveRamp, Chop, ChopFelled, GatherSapling, PlantSapling, BlueprintHaul, Build), plus Craft and hauling handled separately.
- **Risk**: Adding a new WorkGiver requires: (a) declaring a `has*Work` boolean, (b) adding a Rebuild call, (c) adding the WorkGiver call in the correct priority position. The priority order is implicit in the code order -- there's no priority table or documentation of relative priorities.
- **Current drift**: None -- all WorkGivers are present. But the section has grown from 3 entries (original mining/build) to 11, and each addition required finding the right insertion point.
- **Severity**: Low -- the pattern is verbose but straightforward. The main risk is inserting at the wrong priority position.

---

## Summary by Severity

### High (has caused bugs or is very fragile)
1. **Pattern 1**: Adding a new DesignationType requires 14 coordinated changes
2. **Pattern 2**: Adding a new ItemType requires 6+ file updates with save migration
3. **Pattern 5**: RemoveBill reimplements CancelJob with missing targetItem release
4. **Pattern 7**: saveload.c and inspect.c require parallel save migration

### Medium (likely future bug)
5. **Pattern 3**: Stockpile slot cleanup duplicated across 5 code paths
6. **Pattern 4**: CancelJob safe-drop duplicated for carryingItem and fuelItem
7. **Pattern 6**: Adding a new WorkshopType requires 8 coordinated changes
8. **Pattern 8**: CancelJob must release every resource type (implicit contract)
9. **Pattern 9**: items[] directly mutated from 11+ files
10. **Pattern 10**: Stockpile slot state managed by 5 coordinating fields
11. **Pattern 11**: ClearMovers must coordinate with 5 other subsystems

### Low (minor inconvenience)
12. **Pattern 12**: jobDrivers[] dispatch table must match JobType enum
13. **Pattern 13**: DefaultMaterialForItemType must cover every ItemType
14. **Pattern 14**: Move-to-target sequence repeated across 9+ drivers
15. **Pattern 15**: WorkGiver priority chain is an open-coded if-else ladder
