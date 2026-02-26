# Reeds

> **Status**: Todo
> **Why**: No harvestable plant grows near water. Reeds fill the "waterside resource" gap and give a primitive weaving/crafting material.
> **Depends on**: Nothing — uses existing vegetation, gathering, and crafting systems.

---

## Concept

Reeds grow naturally at water edges (like grass grows on soil). They're gathered by hand and used for mats, baskets, roofing, and primitive containers.

## New Item

| Item | Source | Stackable | Notes |
|------|--------|-----------|-------|
| ITEM_REEDS | Gathered from reed plants | Yes | Primitive weaving material |

## World Generation

Reed plants spawn on walkable cells adjacent to water during terrain generation. Similar to how grass/saplings are placed, but specifically at water edges.

- New cell type or vegetation type: `VEGETATION_REEDS` (render as tall green stalks)
- Spawn condition: walkable cell with at least one cardinal neighbor that `HasWater()`
- Regrows over time (like grass regrowth)

## Gathering

Use existing gather designation system (like gather grass). Mover walks to reed cell, harvests, produces ITEM_REEDS.

## Uses

| Recipe | Workshop | Input | Output | Notes |
|--------|----------|-------|--------|-------|
| Weave Reed Mat | Rope Maker | REEDS × 4 | ITEM_REED_MAT × 1 | Floor/bedding material |
| Weave Reed Basket | Rope Maker | REEDS × 6 + CORDAGE × 1 | ITEM_BASKET × 1 | Alternative basket recipe (no planks needed) |
| Reed Roof | Construction | REEDS × 4 + POLES × 2 | BUILD_FLOOR | Alternative to leaf/bark roof |

The key value is giving players a **water-adjacent resource loop**:
- Settle near water → harvest reeds → make mats/baskets/roofing
- Alternative to grass/leaves for basic crafting
- Basket recipe without needing the full cordage chain

## Survival Progression

```
Leaf roof       → leaves + poles (from trees)
Reed roof       → reeds + poles (from waterside)
Bark roof       → bark + poles (need sawmill for bark)
Thatch floor    → dirt/gravel/sand + dried grass
Reed mat floor  → reeds (simpler, waterside alternative)
```

## Implementation

1. Add ITEM_REEDS (+ ITEM_REED_MAT if doing mats) to items.h + item_defs.c
2. Add reed vegetation to terrain generation (water-edge placement)
3. Add reed sprite (tall green stalks)
4. Add gather designation support for reeds (like gather grass)
5. Add recipes at rope maker (mat, basket alternative)
6. Add reed roof construction recipe
7. Add stockpile filter entries
8. Save version bump + migration
9. Tests: gathering, crafting, terrain generation placement

## Not In Scope

- Reed soaking / retting (moisture system, future)
- Blow pipes / musical instruments (future)
- Reed screens / wind barriers (future)
