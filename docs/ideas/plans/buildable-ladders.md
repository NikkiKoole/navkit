# Buildable Ladders Plan

Ladders as items that can be crafted, hauled, and placed - instead of instant drawing.

## Current State

- Ladders exist as cell types: `CELL_LADDER_UP`, `CELL_LADDER_DOWN`, `CELL_LADDER_BOTH`
- Placed instantly via input (drawing mode)
- No material cost, no construction job

## Proposed Change

Ladders become items that are:
1. **Crafted** at a workshop (wood → ladder item)
2. **Hauled** to blueprint location
3. **Installed** by a builder (like wall construction)
4. **Removable** (uninstall → get ladder item back)

## Implementation Steps

### 1. Add ladder item type
- `ITEM_LADDER` in items.h
- Stackable? Probably not (bulky)

### 2. Add ladder blueprint type
- Currently blueprints only build walls
- Need `BlueprintType` enum: `BLUEPRINT_WALL`, `BLUEPRINT_LADDER`
- Or: generic blueprint with `targetCellType` field

### 3. Modify CompleteBlueprint
- Check blueprint type
- Place appropriate cell type instead of always `CELL_WALL`

### 4. Ladder removal
- New designation or tool: "deconstruct"
- Removes ladder cell, spawns ladder item
- Reuses existing job patterns

## Open Questions

- Should ladder direction (up/down/both) be chosen at placement time?
- Or always place BOTH and let pathfinding handle it?
- Workshop recipe: what input material? Wood seems natural.
- What happens to "drawn" ladders when we add buildable ones?
  - Keep both? (debug/sandbox mode vs survival mode)
  - Or deprecate instant placement?

## Estimated Effort

| Area | Changes |
|------|---------|
| Code | ~100 lines |
| Tests | ~50 lines |
| Risk | Low |
