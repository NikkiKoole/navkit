Assumption Audit: stockpiles.c

## Finding 1: RemoveStockpileCells doesn't release slot reservations

**What**: `RemoveStockpileCells` removes cells from a stockpile, drops items to ground state, and clears slot data. However, it doesn't clear `sp->reservedBy[idx]`.

**Assumption**: Clearing the cell and slot data is sufficient; reservations don't need explicit cleanup.

**How it breaks**: If a slot has `reservedBy > 0` when the cell is removed, a mover has a haul job targeting that slot. When the mover arrives and calls `PlaceItemInStockpile`, the `reservedBy` decrement happens on an inactive cell. The reservation count leaks, and the cache thinks the slot is still reserved even though the cell no longer exists.

**Player impact**: After removing stockpile cells, movers may complete haul jobs but reservation counts leak. If the cells are later re-added via `AddStockpileCells`, they start with phantom reservations. This could make newly-added cells appear full when they're actually empty.

**Severity**: Medium (reservation count corruption)

**Suggested fix**: In `RemoveStockpileCells`, add `sp->reservedBy[idx] = 0;` before setting `sp->cells[idx] = false;`.

---

## Finding 2: DeleteStockpile doesn't drop IN_STOCKPILE items to ground

**What**: `DeleteStockpile` just sets `active = false` without touching items that are `ITEM_IN_STOCKPILE` state in that stockpile.

**Assumption**: Items in deleted stockpiles will be handled by some other cleanup path.

**How it breaks**: When a stockpile is deleted, all items with `state == ITEM_IN_STOCKPILE` at those coordinates become orphaned. They're marked as in a stockpile that no longer exists. `IsPositionInStockpile` will return false for their positions, but their state is still `ITEM_IN_STOCKPILE`. They won't be picked up by hauling jobs because those only target `ITEM_ON_GROUND`.

**Player impact**: Deleting a stockpile makes all items in it invisible and inaccessible. They exist in the world but can't be interacted with. The player loses items.

**Severity**: High (item loss)

**Suggested fix**: In `DeleteStockpile`, loop through all items, check if they're `ITEM_IN_STOCKPILE` at coordinates within the deleted stockpile's bounds, and set them to `ITEM_ON_GROUND`.

---

## Finding 3: PlaceItemInStockpile doesn't validate cell is active

**What**: `PlaceItemInStockpile` updates slot state but doesn't check if `sp->cells[idx]` is true.

**Assumption**: The caller only passes valid, active cell coordinates.

**How it breaks**: If a mover has a haul job to a cell that gets removed mid-job (via `RemoveStockpileCells`), the mover will still try to place the item there. `PlaceItemInStockpile` will happily update `slots[idx]`, `slotCounts[idx]`, etc. on an inactive cell. The item's state becomes `ITEM_IN_STOCKPILE` at coordinates that aren't part of any active stockpile.

**Player impact**: Items vanish into inactive stockpile cells. The slot counts show items exist, but the cell isn't active, so the items are effectively lost.

**Severity**: High (item loss)

**Suggested fix**: Add a check `if (!sp->cells[idx]) return;` at the start of `PlaceItemInStockpile` before modifying any state.

---

## Finding 4: ReleaseStockpileSlot doesn't invalidate slot cache

**What**: `ReleaseStockpileSlot` decrements `reservedBy[idx]` and clears type stamps on empty slots, but doesn't call `InvalidateStockpileSlotCache`.

**Assumption**: The slot cache will be rebuilt on the next frame via `RebuildStockpileSlotCache`, so manual invalidation isn't needed.

**How it breaks**: If a slot was cached as "no space available" (because it was full or reserved), and then a reservation is released mid-frame, the cache still thinks there's no space. New haul jobs won't be created until the next frame's rebuild.

**Player impact**: One-frame delay in assigning haul jobs after a reservation is released. This is actually acceptable behavior and not really a bug—the cache is explicitly documented as "may be briefly stale." But it's worth noting for completeness.

**Severity**: Low (cosmetic/acceptable inefficiency)

**Suggested fix**: None needed—this is intentional design. The cache trades stale-ness for performance.

---

## Finding 5: RemoveStockpileCells doesn't check if items are reserved

**What**: When `RemoveStockpileCells` finds items in a slot and drops them to `ITEM_ON_GROUND`, it doesn't check if those items are `reservedBy != -1`.

**Assumption**: Items in stockpiles aren't reserved (only ground items are reserved for hauling).

**How it breaks**: Items can be reserved while in a stockpile if a craft job or other system has reserved them. If the stockpile cell is removed, the item is set to `ITEM_ON_GROUND` but the reservation persists. Now the item shows as reserved on the ground, blocking other movers from picking it up, but the mover that reserved it may not have a valid path anymore (since the stockpile tile changed).

**Player impact**: Removing stockpile cells can leave items stuck as "reserved" on the ground, preventing any mover from picking them up. The items are visible but untouchable.

**Severity**: Medium (items stuck in reserved state)

**Suggested fix**: In `RemoveStockpileCells`, release item reservations: `items[i].reservedBy = -1;` when dropping items to ground.

---

## Finding 6: PlaceItemInStockpile assumes slotTypes/Materials match

**What**: `PlaceItemInStockpile` sets `sp->slotTypes[idx]` and `sp->slotMaterials[idx]` to the incoming item's type/material unconditionally.

**Assumption**: The slot is either empty or already contains the same type/material.

**How it breaks**: If due to a race condition or bug elsewhere, an item of a different type/material is placed in a slot that already has items, this will overwrite the slot's type/material to match the last item. The `slotTypes` field no longer matches the items actually in the slot, breaking the stacking invariant.

**Player impact**: Stockpile slots can end up with mixed types/materials even though the code is supposed to prevent this. Consolidation and stacking logic breaks, leading to items of different types merging or being rejected incorrectly.

**Severity**: Medium (data corruption leading to broken stacking)

**Suggested fix**: Add validation: `if (sp->slotTypes[idx] >= 0 && sp->slotTypes[idx] != items[itemIdx].type) return;` before updating. Or assert that the types match in debug builds.

---

## Finding 7: RemoveItemFromStockpileSlot doesn't check if item is actually in that stockpile

**What**: `RemoveItemFromStockpileSlot` takes a world position and decrements `slotCounts` for the stockpile at that position. It doesn't verify that the item being removed is actually in `ITEM_IN_STOCKPILE` state or that its coordinates match the slot.

**Assumption**: This function is only called for items that are truly in the stockpile at that position.

**How it breaks**: If an item is deleted or moved but the caller still has stale coordinates, `RemoveItemFromStockpileSlot` will decrement the count for whatever slot exists at those coordinates now. If the stockpile was deleted and recreated, or if coordinates were reused, the wrong slot loses a count.

**Player impact**: Stockpile slot counts become negative or incorrectly decremented, causing ghost items (count says items exist but there are none) or preventing new items from being added (count thinks slot is full).

**Severity**: Medium (slot count corruption)

**Suggested fix**: Pass the item index to `RemoveItemFromStockpileSlot` and validate that `items[itemIdx].state == ITEM_IN_STOCKPILE` and the item's coordinates match before decrementing.

---

## Finding 8: RebuildStockpileGroundItemCache doesn't check cell activity

**What**: `RebuildStockpileGroundItemCache` calls `MarkStockpileGroundItem` for all ground items, which loops through stockpiles checking if the tile is within bounds. `MarkStockpileGroundItem` checks `sp->cells[idx]` and returns early if inactive, but only after computing the bounds check.

**Assumption**: This is fine—the bounds check is cheap.

**How it breaks**: Actually, this is correct behavior. No bug here on second inspection—the `if (!sp->cells[idx]) continue;` guards against marking inactive cells.

**Player impact**: N/A

**Severity**: N/A (correct behavior)

---

## Finding 9: FindStockpileForItem doesn't respect priority

**What**: `FindStockpileForItem` loops through stockpiles in index order (0 to MAX_STOCKPILES), not priority order.

**Assumption**: Priority doesn't matter for initial placement—only for rehaul operations.

**How it breaks**: The comment in the code even notes this as a TODO! When placing items, they go to the first stockpile with space, regardless of priority. High-priority stockpiles aren't filled preferentially. Players set priority expecting items to favor certain stockpiles, but initial haul jobs ignore priority entirely.

**Player impact**: Priority system doesn't work as expected for initial hauling. Players create a high-priority stockpile near their workshop expecting items to go there first, but items fill a low-priority distant stockpile instead just because it has a lower index.

**Severity**: Medium (feature doesn't work as designed)

**Suggested fix**: Sort active stockpiles by priority (descending) before the loop, or iterate through priority values (9 down to 1) and scan stockpiles at each priority level.

---

## Finding 10: IsItemInGatherZone returns true when no zones exist

**What**: `IsItemInGatherZone` has a special case: `if (gatherZoneCount == 0) return true;`.

**Assumption**: When no gather zones are defined, all items are eligible for hauling (permissive default).

**How it breaks**: This assumption is probably intentional design, but it means you can't create a "no gathering allowed" state by deleting all zones. If the player wants to temporarily disable all gathering, they'd expect deleting zones to prevent hauling, but it does the opposite.

**Player impact**: Deleting all gather zones doesn't stop hauling—it makes ALL items eligible. Counterintuitive behavior.

**Severity**: Low (design choice, possibly intentional, but potentially confusing UX)

**Suggested fix**: If this is intentional, document it clearly. If not, change the logic to return false when no zones exist (prohibit all gathering).

---

## Summary

**High severity findings:**
- Finding 2: DeleteStockpile doesn't drop IN_STOCKPILE items to ground (item loss)
- Finding 3: PlaceItemInStockpile doesn't validate cell is active (item loss)

**Medium severity findings:**
- Finding 1: RemoveStockpileCells doesn't release slot reservations
- Finding 5: RemoveStockpileCells doesn't clear item reservations
- Finding 6: PlaceItemInStockpile assumes slotTypes/Materials match
- Finding 7: RemoveItemFromStockpileSlot doesn't validate item state
- Finding 9: FindStockpileForItem doesn't respect priority

**Low severity findings:**
- Finding 4: ReleaseStockpileSlot doesn't invalidate slot cache (acceptable design)
- Finding 10: IsItemInGatherZone permissive default (possibly intentional)
