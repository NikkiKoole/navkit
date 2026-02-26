# Mud & Cob

> **Status**: Todo
> **Why**: Dirt and clay have limited sinks. Mud is a natural intermediate that enables primitive construction without heavy tools.
> **Depends on**: Nothing — uses existing items, workshops, and construction system.

---

## New Items

| Item | Source | Stackable | Notes |
|------|--------|-----------|-------|
| ITEM_MUD | Mud Mixer (dirt + clay) | Yes | Wet building material |
| ITEM_COB | Mud Mixer (mud + dried grass) | Yes | Stronger building material |

## Workshop: Mud Mixer (2x1)

Simple workshop, must be placed adjacent to water (check `HasWater` on neighbors).

```
X #
```

| Recipe | Input | Output | Time |
|--------|-------|--------|------|
| Mix Mud | DIRT × 2 + CLAY × 1 | MUD × 3 | 3s |
| Make Cob | MUD × 2 + DRIED_GRASS × 1 | COB × 2 | 4s |

Placement validation: at least one cardinal neighbor cell has water (reuse `HasWater()` from water sim).

## Construction Recipes

| Recipe | Input | Category | Time | Notes |
|--------|-------|----------|------|-------|
| Mud Wall | MUD × 4 | BUILD_WALL | 3s | Cheap, easy, weak |
| Cob Wall | COB × 3 | BUILD_WALL | 4s | Stronger, good insulation |
| Mud Floor | MUD × 2 | BUILD_FLOOR | 2s | Basic flooring |

These slot into the existing construction recipe system alongside leaf/stick/log/plank walls. No new cell types needed — they produce CELL_WALL/CELL_FLOOR with appropriate material.

## Material

Add `MAT_MUD` and `MAT_COB` to MaterialType enum. Walls inherit material from the items delivered.

## Survival Progression

```
Leaf wall       → leaves + sticks (immediate, flimsy)
Stick wall      → sticks + cordage (need rope maker)
Pole wall       → poles + cordage (sturdier)
Mud wall        → mud (need mud mixer near water, cheap but weak)
Cob wall        → cob (mud + dried grass, good insulation)
Log wall        → logs (need axe)
Wattle & daub   → sticks + cordage + dirt/clay (existing, 2-stage)
Plank wall      → planks (need sawmill)
```

Mud/cob walls give players a water-adjacent building option that doesn't require tools or cordage — just access to dirt, clay, and water.

## Implementation

1. Add ITEM_MUD, ITEM_COB to items.h + item_defs.c
2. Add MAT_MUD, MAT_COB to material.h
3. Add WORKSHOP_MUD_MIXER to workshops + template + recipes
4. Add construction recipe for mud mixer (placement near water)
5. Add 3 construction recipes (mud wall, cob wall, mud floor)
6. Add stockpile filter entries
7. Save version bump + migration
8. Tests: workshop crafting, construction, placement validation

## Not In Scope

- Moisture states / curing (see water-dependent-crafting.md for future vision)
- Mud bricks (separate from fired bricks — future)
- Quality based on drying speed (future)
