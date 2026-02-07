# Audit: designations.c

Audited file: `src/world/designations.c` (plus `designations.h`)
Related files traced: `src/entities/jobs.c` (CancelJob, WorkGivers, job drivers), `src/entities/items.c`, `src/entities/mover.c`, `src/world/grid.c`

---

## HIGH Severity

**Finding 1: CompleteDigRampDesignation missing rampCount increment**
- **What**: `CompleteDigRampDesignation` converts a wall/dirt cell to a ramp (`grid[z][y][x] = rampType`) but never increments the global `rampCount`.
- **Assumption**: Any code that creates a ramp cell also updates `rampCount`. `CompleteChannelDesignation` correctly does `rampCount++` when creating a ramp, and `CompleteRemoveRampDesignation` does `rampCount--`. `RemoveInvalidRamp` in grid.c also does `rampCount--`.
- **How it breaks**: Player designates "dig ramp" on a wall. Mover carves the ramp. `rampCount` is now wrong (too low). If ramp removal later runs, or `ValidateAndCleanupRamps` runs, `rampCount` can go negative. Any system that uses `rampCount` for optimization or display will show wrong data.
- **Player impact**: `rampCount` drift accumulates over time. Could cause pathfinding issues if rampCount is used for early-exit optimizations, or visual/UI inconsistencies.
- **Severity**: High
- **Suggested fix**: Add `rampCount++;` after `grid[z][y][x] = rampType;` in `CompleteDigRampDesignation`.

**Finding 2: CompleteDigRampDesignation missing MarkChunkDirty**
- **What**: `CompleteDigRampDesignation` converts a wall to a ramp and creates a floor at z+1, but never calls `MarkChunkDirty(x, y, z)` or `MarkChunkDirty(x, y, z + 1)`.
- **Assumption**: Any function that modifies grid cells must call `MarkChunkDirty` so the rendering mesh is rebuilt. Every other Complete* function (mine, channel, remove floor, remove ramp, chop, etc.) calls MarkChunkDirty.
- **How it breaks**: Player orders "dig ramp", mover completes it, but the rendering chunk is not marked dirty. The visual mesh is stale -- the player still sees a wall where the ramp should be, until something else (e.g., mining a neighbor, water flow) dirties that chunk.
- **Player impact**: Invisible ramp. Player thinks the dig failed. Movers may walk up/down the "invisible" ramp confusingly.
- **Severity**: High
- **Suggested fix**: Add `MarkChunkDirty(x, y, z);` and conditionally `MarkChunkDirty(x, y, z + 1);` after the ramp and floor creation in `CompleteDigRampDesignation`.

**Finding 3: CompleteRemoveFloorDesignation spawns drop item at wrong z-level**
- **What**: `CompleteRemoveFloorDesignation` first drops existing items to z-1 via `DropItemsInCell(x, y, z)`, then removes the floor at z, then spawns the floor's drop item at `(float)z` -- the level where the floor was just removed.
- **Assumption**: Items spawned after floor removal should be at the level where they'd actually land. The floor is gone, so there's nothing at z to support the item.
- **How it breaks**: The floor drop item is spawned at z with no floor beneath it. It floats in mid-air. It may or may not fall depending on whether the mover update or item system handles falling items (only `DropItemsInCell` explicitly moves items, and it's already been called before the spawn).
- **Player impact**: A stone block floats visibly in the air where the floor used to be. It can't be reached by movers (no walkable surface). The item is effectively lost.
- **Severity**: High
- **Suggested fix**: Spawn the drop item at `(float)(z - 1)` instead of `(float)z`, consistent with how `CompleteChannelDesignation` spawns floor debris at `lowerZ`. Or call `DropItemsInCell` after spawning the item.

**Finding 4: CompleteBlueprint (WALL) does not call InvalidatePathsThroughCell**
- **What**: When a wall blueprint is completed, `CompleteBlueprint` calls `PushMoversOutOfCell` (to eject the builder) and `PushItemsOutOfCell`, but does NOT call `InvalidatePathsThroughCell`. The workshops system correctly calls `InvalidatePathsThroughCell` when placing blocks.
- **Assumption**: When a cell becomes impassable (wall placed), all movers whose cached paths traverse that cell need their paths invalidated so they repath around the new wall.
- **How it breaks**: A mover on the other side of the map has a cached path that goes through cell (x,y,z). A wall is built there. The mover follows its stale path, walks into the wall, gets stuck, eventually triggers stuck detection and repaths -- but wastes several seconds.
- **Player impact**: Movers briefly walk into newly-built walls and get stuck before recovering. Looks buggy, especially during large construction projects with many walls being built.
- **Severity**: High
- **Suggested fix**: Add `InvalidatePathsThroughCell(x, y, z);` after placing the wall in `CompleteBlueprint` for `BLUEPRINT_TYPE_WALL` and `BLUEPRINT_TYPE_RAMP` (ramp also changes cell passability).

---

## MEDIUM Severity

**Finding 5: CompleteBlueprint (RAMP) missing rampCount increment**
- **What**: `CompleteBlueprint` for `BLUEPRINT_TYPE_RAMP` creates a ramp cell (`grid[z][y][x] = rampType`) but never increments `rampCount`. Same class of bug as Finding 1.
- **Assumption**: Same as Finding 1.
- **How it breaks**: Same as Finding 1, for blueprint-built ramps.
- **Player impact**: `rampCount` drift.
- **Severity**: Medium
- **Suggested fix**: Add `rampCount++;` after setting the ramp cell type in the `BLUEPRINT_TYPE_RAMP` case.

**Finding 6: CancelBlueprint does not refund delivered materials**
- **What**: `CancelBlueprint` releases the reserved item (unreserves it so it's available again) but does NOT respawn items that were already delivered. `DeliverMaterialToBlueprint` calls `DeleteItem(itemIdx)` to consume materials permanently when they arrive.
- **Assumption**: Players expect canceling construction to return materials, especially since materials were visibly hauled to the site. In Dwarf Fortress and RimWorld, canceling construction drops materials.
- **How it breaks**: Player places a wall blueprint, a mover hauls a stone block to it, the blueprint transitions to READY_TO_BUILD. Player changes their mind and cancels. The stone block is gone forever.
- **Player impact**: Materials vanish when canceling blueprints that already received deliveries. Player loses resources with no feedback about why.
- **Severity**: Medium
- **Suggested fix**: In `CancelBlueprint`, if `bp->deliveredMaterialCount > 0`, spawn `bp->deliveredMaterialCount` items of the appropriate type/material at the blueprint location using `SpawnItemWithMaterial`. Use `bp->deliveredMaterial` for the material type.

**Finding 7: CompleteChannelDesignation missing ClearUnreachableCooldownsNearCell**
- **What**: `CompleteMineDesignation` calls `ClearUnreachableCooldownsNearCell(x, y, z, 5)` after opening up terrain, allowing items/designations that were previously unreachable to be retried. `CompleteChannelDesignation` does NOT call this, despite opening up terrain at two z-levels.
- **Assumption**: When terrain is removed, nearby items and designations that were marked unreachable may now be reachable. All terrain-modifying completions should clear cooldowns.
- **How it breaks**: Player channels near an item that was marked unreachable (e.g., item on other side of wall that was just channeled through). The cooldown timer (typically several seconds) must expire before any mover will try to path to it, even though the path is now clear.
- **Player impact**: Items near channeled areas are temporarily ignored by haulers for no visible reason. Movers stand idle while items sit on the ground nearby.
- **Severity**: Medium
- **Suggested fix**: Add `ClearUnreachableCooldownsNearCell(x, y, z, 5);` and `ClearUnreachableCooldownsNearCell(x, y, lowerZ, 5);` after the terrain modifications in `CompleteChannelDesignation`.

**Finding 8: CompleteRemoveFloorDesignation and CompleteRemoveRampDesignation missing DestabilizeWater**
- **What**: `CompleteMineDesignation` and `CompleteChannelDesignation` both call `DestabilizeWater` after modifying terrain, allowing water to flow into newly-opened spaces. `CompleteRemoveFloorDesignation` and `CompleteRemoveRampDesignation` do NOT, despite creating new vertical openings (floor removal) or changing terrain shape (ramp removal).
- **Assumption**: When terrain changes create new paths for water flow, `DestabilizeWater` should be called so the water simulation reacts.
- **How it breaks**: Player removes a floor above a water-filled area. Water should flow down through the hole, but the water simulation doesn't know the floor is gone. Water remains frozen in place until something else destabilizes it.
- **Player impact**: Water doesn't flow through removed floors. Looks like a water simulation bug.
- **Severity**: Medium
- **Suggested fix**: Add `DestabilizeWater(x, y, z);` in both `CompleteRemoveFloorDesignation` and `CompleteRemoveRampDesignation`.

**Finding 9: FellTree flood-fill can drop trunk cells when stack overflows**
- **What**: `FellTree` uses a DFS flood-fill with `TREE_STACK_MAX = 4096` to remove all connected trunk cells. The stack pushes 6 neighbors per cell WITHOUT a visited set. Already-processed cells are skipped (because they're changed to CELL_AIR before neighbors are pushed), but the same unvisited cell can be pushed multiple times from different neighbors.
- **Assumption**: 4096 entries is enough for any tree. But without deduplication, the effective capacity is much lower. A tree with N trunks can generate up to 6N stack entries. Trees with ~680+ trunk cells could overflow.
- **How it breaks**: Very large or complex multi-trunk trees overflow the stack. The `if (stackCount >= TREE_STACK_MAX) break;` exits the inner neighbor loop but the outer loop continues processing what's already on the stack. Some trunk cells are never visited and remain as orphaned CELL_TREE_TRUNK cells floating in the world.
- **Player impact**: Chopping a large tree leaves ghost trunk segments floating in the air that can't be interacted with. The warning log fires but the player doesn't see it.
- **Severity**: Medium (only affects unusually large trees, and a warning is logged)
- **Suggested fix**: Either add a visited bitmap (`bool visited[gridDepth][gridHeight][gridWidth]` or a hash set), or increase TREE_STACK_MAX, or both. The simplest fix is to check `grid[nz][ny][nx] == CELL_TREE_TRUNK` before pushing to avoid pushing non-trunk cells.

**Finding 10: CompleteBlueprint (RAMP) does not push movers or items out of cell**
- **What**: `CompleteBlueprint` for `BLUEPRINT_TYPE_WALL` correctly calls `PushMoversOutOfCell` and `PushItemsOutOfCell` before placing the wall. The `BLUEPRINT_TYPE_RAMP` case does not, despite converting the cell from floor to a ramp (which changes passability characteristics).
- **Assumption**: When a cell's type changes to something that blocks normal standing (a ramp is traversable but movers "stand" differently on it vs floor), movers and items should be handled.
- **How it breaks**: Builder mover standing on the cell completes the ramp. The floor is cleared (`CLEAR_FLOOR`). Items on the old floor may now be at an invalid position. Movers standing there may behave unexpectedly since the cell is now a ramp, not walkable floor.
- **Player impact**: Items could get stuck on newly-built ramps. Movers may briefly glitch when the cell transitions under them.
- **Severity**: Medium
- **Suggested fix**: Add `PushItemsOutOfCell` before ramp placement, similar to the wall case.

---

## LOW Severity

**Finding 11: Multiple Complete* functions missing InvalidateDesignationCache**
- **What**: Several Complete* functions clear the designation directly (`designations[z][y][x].type = DESIGNATION_NONE`) without calling `InvalidateDesignationCache`. Affected: `CompleteChannelDesignation`, `CompleteDigRampDesignation`, `CompleteRemoveFloorDesignation`, `CompleteRemoveRampDesignation`, `CompleteGatherSaplingDesignation`, `CompletePlantSaplingDesignation`. Only `CompleteMineDesignation` correctly calls it.
- **Assumption**: Caches should be invalidated when designations are removed so stale entries are cleaned up.
- **How it breaks**: The WorkGiver cache contains stale entries for completed designations. The WorkGiver functions do re-validate each cache entry (`GetDesignation` returns NULL for cleared designations, `d->assignedMover` check), so no incorrect behavior occurs. But stale entries cause wasted iteration in the WorkGiver loops.
- **Player impact**: Minor performance waste. No visible behavior change.
- **Severity**: Low
- **Suggested fix**: Add `InvalidateDesignationCache(DESIGNATION_XXX)` in each Complete* function, or have them route through `CancelDesignation` for cleanup.

**Finding 12: CompleteDigRampDesignation missing ClearUnreachableCooldownsNearCell**
- **What**: Like Finding 7, `CompleteDigRampDesignation` converts a wall to a ramp (opening new paths) but does not clear unreachable cooldowns for nearby items.
- **Assumption**: Same as Finding 7 -- terrain changes should clear cooldowns.
- **How it breaks**: Items near a newly-carved ramp remain marked unreachable even though new paths may exist via the ramp.
- **Player impact**: Temporary hauling delay near newly dug ramps.
- **Severity**: Low
- **Suggested fix**: Add `ClearUnreachableCooldownsNearCell(x, y, z, 5);`

**Finding 13: CompleteDigRampDesignation missing ValidateAndCleanupRamps**
- **What**: `CompleteMineDesignation` and `CompleteChannelDesignation` call `ValidateAndCleanupRamps` after terrain modification. `CompleteDigRampDesignation` converts a wall to a ramp but does NOT validate nearby ramps. The wall that was removed may have been the "high side" support for an adjacent ramp.
- **Assumption**: Removing solid cells should trigger ramp validation in the surrounding area.
- **How it breaks**: Adjacent ramps that relied on the now-removed wall as their high-side support become structurally invalid but remain in the world.
- **Player impact**: Floating/invalid ramps that movers might path through incorrectly.
- **Severity**: Low (ramp is replaced, not removed -- likely the new ramp provides valid support)
- **Suggested fix**: Add `ValidateAndCleanupRamps(x - 2, y - 2, z - 1, x + 2, y + 2, z + 1);`

---

## Summary

| Severity | Count | Key themes |
|----------|-------|------------|
| High     | 4     | Missing rampCount, missing MarkChunkDirty, wrong spawn z-level, missing path invalidation |
| Medium   | 6     | Missing rampCount (blueprint), no material refund on cancel, missing cooldown clears, missing water destabilize, flood-fill overflow, missing mover push |
| Low      | 3     | Stale caches, missing cooldown clear, missing ramp validation |

The most impactful fixes are Findings 1-3 (dig ramp rendering/tracking broken, floor items spawn in air) and Finding 4 (wall construction doesn't invalidate paths). These are all straightforward one-line or few-line additions following patterns already established by `CompleteMineDesignation` and `CompleteChannelDesignation`.
