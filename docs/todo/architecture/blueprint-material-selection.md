# Blueprint Material Selection

Let players choose which building material to use when placing walls/floors/etc.

## Current State

- Blueprint struct has `BlueprintType` (wall, floor, ladder, ramp) but no material preference
- `BlueprintHaulItemFilter` accepts any item with `IF_BUILDING_MAT` flag
- Builders grab the nearest building material regardless of type
- `deliveredMaterial` is set after delivery, not before

## Problem

With multiple building materials (logs, planks, bricks, stone blocks, future tiles), players need control over which material is used. A brick wall should use bricks, not whatever log happens to be closest.

## How Dwarf Fortress Does It

- Per-tile material selection: place wall -> pick material from list of available items
- Game remembers last selection so it stays at top for next placement
- No global "only use X" setting
- Players use filtered stockpiles near build sites for bulk construction

## Proposed Design

### Blueprint Changes

Add to `Blueprint` struct:
```c
ItemType requiredItemType;  // ITEM_TYPE_COUNT = accept any building mat
```

Set when player places the construction. Default to `ITEM_TYPE_COUNT` (any).

### Job Assignment Changes (`WorkGiver_BlueprintHaul`)

Current flow:
1. Find nearest item with `IF_BUILDING_MAT` (spatial grid, then linear scan)
2. Find any blueprint needing materials (first reachable one)
3. Reserve item, create JOBTYPE_HAUL_TO_BLUEPRINT job
4. Item and blueprint are matched independently — no connection between which item type and which blueprint

Problem: step 1 finds the nearest building mat globally, step 2 finds any blueprint. There's no link between the blueprint's required material and the item search.

New flow:
1. For each blueprint needing materials:
   - If `requiredItemType != ITEM_TYPE_COUNT`, search only for items of that type
   - If `requiredItemType == ITEM_TYPE_COUNT`, search for any `IF_BUILDING_MAT` item (current)
2. Pick the best (nearest item + reachable blueprint) pair
3. Reserve and create job as before

This means `BlueprintHaulItemFilter` needs access to the blueprint's `requiredItemType`. Options:
- Pass it via the `userData` pointer (already exists but unused)
- Or filter in the linear scan loop with an extra `item->type` check

The `userData` approach is cleanest:
```c
static bool BlueprintHaulItemFilter(int itemIdx, void* userData) {
    ItemType required = *(ItemType*)userData;
    Item* item = &items[itemIdx];
    if (!item->active) return false;
    if (!ItemIsBuildingMat(item->type)) return false;
    if (required != ITEM_TYPE_COUNT && item->type != required) return false;
    if (item->reservedBy != -1) return false;
    if (item->state != ITEM_ON_GROUND) return false;
    if (item->unreachableCooldown > 0.0f) return false;
    return true;
}
```

The linear scan fallback also needs the same type check added.

### UI Flow

When entering wall/floor placement mode, show available building material types:
- Cycle with a key (e.g., M for material) through available `IF_BUILDING_MAT` item types
- Show current selection in the status bar: "Wall (Bricks)" / "Wall (Any)"
- All placed blueprints use the selected type until changed

### Save Migration

New field on Blueprint. Bump save version, default `requiredItemType = ITEM_TYPE_COUNT` for old saves.

## Consequences

### Performance

Naive approach: for each blueprint, do a separate item search. 50 blueprints = 50 searches. 

Better: the existing `reservedItem` field already helps. Once a mover is assigned and reserves an item for a blueprint, that blueprint is out of the "needs materials" pool. So at any given tick, only *unserviced* blueprints trigger searches.

Optimization: group blueprints by `requiredItemType`. Do one item search per unique type, not per blueprint. Could maintain a count per type (`blueprintsNeedingType[ITEM_TYPE_COUNT]`) updated on blueprint create/deliver/cancel, so the work giver skips types with zero demand in O(1).

### Reservation Flow

Current reservation flow already works well:
1. `WorkGiver_BlueprintHaul` finds item + blueprint pair
2. Reserves item (`item->reservedBy = moverIdx`) and links to blueprint (`bp->reservedItem = itemIdx`)
3. Other movers skip reserved items and blueprints with `reservedItem >= 0`

With material selection, the reservation logic doesn't change — it just filters which items are candidates *before* reserving. The `reservedBy` check already prevents two blueprints from competing for the same item.

### Deadlocks / Player Feedback

A blueprint requesting "Bricks" waits forever if no bricks exist. The blueprint tooltip should show which material it wants: "Awaiting: Bricks" instead of just "Awaiting materials". This helps the player know what to produce.

### "Any" vs Specific Priority

If some blueprints say "Any" and others say "Bricks", a mover might grab the last brick for an "Any" blueprint. Options:
- Prioritize exact-match blueprints over "Any" blueprints (slightly more complex)
- Accept it as player responsibility (simpler, DF-like)
- Recommendation: start simple, add priority later if it's a real problem

### Rendering

No changes needed. Wall/floor appearance already depends on `deliveredMaterial` which is set when the item arrives. Material selection just ensures the *right* item gets delivered.

### Save Migration

New field on every Blueprint. Bump save version, default `requiredItemType = ITEM_TYPE_COUNT` for old saves. Straightforward, same pattern as stockpile migrations.

## Also Consider

- **ITEM_TILES**: New kiln recipe "Fire Tiles" (clay -> tiles). Tiles and bricks both have `IF_BUILDING_MAT`, both work for walls and floors. Visual difference only.
- **Material species**: Could also filter by material species (Oak Planks vs Pine Planks) but that's probably overkill for now. Start with item type filtering only.
