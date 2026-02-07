Here's the audit of `src/entities/items.c`:

---

**Finding 1: `DeleteItem` doesn't release the stockpile slot**
- **What**: `DeleteItem` sets `active = false` and `reservedBy = -1`, but doesn't check if the item was `ITEM_IN_STOCKPILE` and release/clear the stockpile slot it occupied.
- **Assumption**: That the caller always handles stockpile slot cleanup before calling `DeleteItem`.
- **How it breaks**: If an item in a stockpile is deleted (e.g. consumed by crafting, or externally deleted during a job), the stockpile slot still thinks it contains an item. The slot's `itemType`/`material`/`count` stay set, so it appears occupied. New items can't be placed there, and the stockpile's free slot count is wrong.
- **Player impact**: Stockpile appears to have fewer free slots than it should. "Ghost" slots that look occupied but have no item. Eventually the player can't haul items to the stockpile even though it visually has empty space.
- **Severity**: **HIGH** — broken stockpile capacity over time, especially after crafting jobs consume stockpiled items.
- **Suggested fix**: In `DeleteItem`, check `if (items[index].state == ITEM_IN_STOCKPILE)` and call a stockpile slot clearing function (like `ClearSourceStockpileSlot` or similar) before deactivating.

---

**Finding 2: `PushItemsOutOfCell` doesn't clear stockpile slot or update item state**
- **What**: When items are pushed out of a cell (wall placed via blueprint), items at `ITEM_IN_STOCKPILE` state get moved to a neighbor tile, but their `state` stays `ITEM_IN_STOCKPILE` and the source stockpile slot isn't cleared.
- **Assumption**: That items being pushed are always `ITEM_ON_GROUND`.
- **How it breaks**: Player has a stockpile, then builds a wall on one of its tiles. Items on that tile get pushed to a neighbor. The items still say `ITEM_IN_STOCKPILE` but are now at a position outside the stockpile. The original stockpile slot still claims them. The item is now a phantom — hauling system thinks it's in a stockpile, but it's at a random neighbor tile.
- **Player impact**: Items stuck in limbo — not hauled (because state is `IN_STOCKPILE`), not visible in stockpile. Stockpile shows occupied slot with nothing visible there.
- **Severity**: **HIGH** — items become unhaulable and stockpile state corrupts.
- **Suggested fix**: In `PushItemsOutOfCell`, set `items[i].state = ITEM_ON_GROUND` and clear the stockpile slot (like `DropItemsInCell` already does for the state change, but neither does the slot cleanup).

---

**Finding 3: `DropItemsInCell` changes state to `ITEM_ON_GROUND` but doesn't release the stockpile slot**
- **What**: When floor is channeled, items drop to z-1 and state changes from `ITEM_IN_STOCKPILE` to `ITEM_ON_GROUND`. But the stockpile slot at the original position is never cleared.
- **Assumption**: That the stockpile ground item cache rebuild will eventually fix things.
- **How it breaks**: Channel a floor under a stockpile tile. Item drops and becomes `ON_GROUND`. The source stockpile slot still has `itemType`, `count`, `material` set, so it appears occupied. The slot won't accept new items.
- **Player impact**: Same as Finding 2 — ghost stockpile slots. The slot looks occupied but the item fell to a lower level.
- **Severity**: **HIGH** — stockpile slot leak after channeling.
- **Suggested fix**: When changing state from `ITEM_IN_STOCKPILE`, find and clear the stockpile slot (using `IsPositionInStockpile` + `ClearSlotData` or equivalent).

---

**Finding 4: `DeleteItem` doesn't decrement `itemHighWaterMark` when deleting the highest item**
- **What**: `itemHighWaterMark` only ever grows (in `SpawnItem`). `DeleteItem` never shrinks it. If items at the high end are deleted, all loops still iterate up to the old high water mark.
- **Assumption**: That items are deleted roughly uniformly, so the overhead is small.
- **How it breaks**: Spawn 1000 items, delete items 500-999. Now every loop (`ItemsTick`, `BuildItemSpatialGrid`, `ClearUnreachableCooldownsNearCell`, etc.) still iterates 1000 instead of 500. In extreme cases (spawn/delete cycles at high indices), the water mark could reach `MAX_ITEMS` and never come back.
- **Player impact**: Gradual performance degradation over long play sessions with lots of item creation/deletion. No visual bug, but frame rate slowly drops.
- **Severity**: **Low** — performance inefficiency, not broken behavior. Only matters in very long sessions.
- **Suggested fix**: In `DeleteItem`, if `index == itemHighWaterMark - 1`, scan backwards to find the new high water mark. Or periodically compact.

---

**Finding 5: `FindNearestUnreservedItem` expanding radius can miss items near radius boundary**
- **What**: The expanding radius search finds an item, then checks if it's within 50% (`0.5f`) of the radius squared. The comment says "70%" but the code uses `0.5f` (which is 50%). If the nearest item is between 50-100% of the radius, it expands to the next radius — but it doesn't reset `ctx`, so the search across the larger radius starts with the already-found item as the best candidate. This works correctly but the comment is misleading.
- **Assumption**: That expanding without resetting context is safe.
- **How it breaks**: It's actually fine — context carries forward so a closer item at larger radius will replace the existing best. The only issue: at the largest radius (120 tiles), if no item is found within 50% of 120 tiles, the function returns the best it found. Items beyond 120 tiles are never found. For a 32x32 map this is fine (diagonal is ~45 tiles).
- **Player impact**: On very large maps, items far away from any mover might never be picked up. Not an issue at current map sizes.
- **Severity**: **Low** — only matters on maps larger than current size, and the function isn't used in production code.
- **Suggested fix**: None needed at current scale. If maps grow, increase max radius or fall back to naive.

---

Summary ranked by severity:
1. **HIGH**: `DeleteItem` doesn't release stockpile slots (Finding 1)
2. **HIGH**: `PushItemsOutOfCell` doesn't clear state/slot for stockpiled items (Finding 2)
3. **HIGH**: `DropItemsInCell` clears state but not the stockpile slot (Finding 3)
4. **Low**: `itemHighWaterMark` never shrinks (Finding 4)
5. **Low**: Expanding radius search limited to 120 tiles (Finding 5)

The common theme across Findings 1-3: **item position/state changes don't propagate to the stockpile slot system.** The stockpile slot is a secondary data structure that mirrors item placement, but `items.c` doesn't keep it in sync during destructive operations.
