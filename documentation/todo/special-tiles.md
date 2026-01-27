# Special Tiles

This document describes planned tile types for sources/drains and material variants.

## Source/Drain Tiles

These are special tiles that affect simulation systems. Currently implemented as flags on cells, but need dedicated visual tiles.

### Water Source
- Refills to max water level (7) each tick
- Also generates pressure for U-bend mechanics
- Use case: springs, fountains, pipes

### Water Drain
- Removes all water each tick
- Use case: grates, sewers, holes

### Heat Source
- Maintains high temperature (default 200°C)
- Use case: furnaces, forges, lava vents, campfires

### Cold Source
- Maintains low temperature (default -30°C)
- Use case: ice blocks, cooling systems, cold storage

### Fire Source
- Permanent fire that never burns out
- Use case: torches, braziers, lava

## Material Tiles

Currently we have basic CELL_WALL and CELL_FLOOR. Need material variants for:
- Visual variety
- Insulation differences (affects heat transfer)
- Future: durability, flammability

### Proposed Cell Types

```c
// Floors (walkable)
CELL_WOOD_FLOOR,   // Wooden planks
CELL_STONE_FLOOR,  // Stone tiles

// Walls (blocking)
CELL_WOOD_WALL,    // Wooden walls - 20% heat transfer (HEAT_TRANSFER_WOOD)
CELL_STONE_WALL,   // Stone walls - 5% heat transfer (HEAT_TRANSFER_STONE)
```

### Insulation Values

From temperature.h:
- `HEAT_TRANSFER_AIR` = 100 (full transfer)
- `HEAT_TRANSFER_WOOD` = 20 (80% insulation)
- `HEAT_TRANSFER_STONE` = 5 (95% insulation)

Wood walls let some heat through. Stone walls are nearly perfect insulators.

## Tile Art Needed

1. Water source (blue with bubbles/waves?)
2. Water drain (grate pattern?)
3. Heat source (red/orange glow?)
4. Cold source (blue/white frost?)
5. Fire source (torch/brazier?)
6. Wood floor (planks)
7. Stone floor (tiles/cobbles)
8. Wood wall (logs/planks)
9. Stone wall (bricks/blocks)

## Implementation Notes

When implementing material walls:
1. Add new CELL_* enum values to grid.h
2. Update `IsWalkableCell()` to include new floor types
3. Update rendering to use appropriate textures
4. Update `GetHeatTransferRate()` to check wall material
5. Update pathfinding if needed (should work if IsWalkableCell is correct)
6. Add placement tools in input.c
