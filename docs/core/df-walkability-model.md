# DF-Style Walkability Model

This document explains the "DF mode" (Dwarf Fortress-style) walkability system, which is the **default** in this codebase.

## Core Concept

In DF mode, you stand **ON TOP** of solid ground, not inside it.

```
z=2:  [AIR]     <- can walk here if z=1 is solid
z=1:  [DIRT]    <- solid ground (CF_SOLID flag)
z=0:  [DIRT]    <- solid ground
```

This differs from legacy mode where you stand **INSIDE** walkable cells.

## Z-Level Layout

Typical terrain in DF mode:

| Z-Level | Contains | Notes |
|---------|----------|-------|
| z=0 | `CELL_DIRT` + grass surface | Ground level, filled by `FillGroundLevel()` |
| z=1 | `CELL_AIR` or buildings | Where movers walk on natural terrain |
| z=2+ | `CELL_AIR` or upper floors | Multi-story buildings |

**Key insight**: Movers at z=1 are walking on the dirt at z=0, not inside it.

## Walkability Rules

A cell at (x, y, z) is walkable if ALL of these are true:

1. The cell doesn't block movement (not a wall)
2. The cell isn't solid itself (can't walk inside dirt/walls)
3. One of these support conditions:
   - Cell below (z-1) is solid (`CF_SOLID` flag)
   - Cell has `HAS_FLOOR` flag (constructed floor)
   - Cell is a ladder or ramp (self-supporting)
   - z=0 (implicit bedrock below)

See `IsCellWalkableAt_Standard()` in `src/world/cell_defs.h`.

## Two Ways to Make a Floor

### 1. Natural Ground (Solid Cell Below)
```c
grid[0][y][x] = CELL_DIRT;  // z=0: solid ground
grid[1][y][x] = CELL_AIR;   // z=1: walkable (dirt below)
```

### 2. Constructed Floor (HAS_FLOOR Flag)
```c
grid[1][y][x] = CELL_AIR;   // z=1: air
SET_FLOOR(x, y, 1);         // z=1: now walkable (balcony/bridge)
grid[0][y][x] = CELL_AIR;   // z=0: can be empty!
```

Use `PlaceFloor()` helper in terrain.c - it handles both modes automatically.

## Cell Flags vs Cell Types

**Cell types** (`grid[z][y][x]`): What the cell IS (dirt, air, wall, ladder)

**Cell flags** (`cellFlags[z][y][x]`): Per-cell properties:
- `CELL_FLAG_HAS_FLOOR` - constructed floor (bit 5)
- `CELL_SURFACE_*` - grass overlay type (bits 3-4)
- `CELL_FLAG_BURNED` - fire damage (bit 0)
- `CELL_WETNESS_*` - water level (bits 1-2)

## Surface Overlays (Grass)

Grass is NOT a cell type in DF mode. It's a surface overlay on `CELL_DIRT`:

```c
grid[0][y][x] = CELL_DIRT;
SET_CELL_SURFACE(x, y, 0, SURFACE_TALL_GRASS);  // Visual only
```

Surface types: `SURFACE_BARE`, `SURFACE_TRAMPLED`, `SURFACE_GRASS`, `SURFACE_TALL_GRASS`

The ground wear system changes surface overlays based on foot traffic.

## Common Patterns

### Terrain Generator Setup
```c
InitGrid();                          // Clear everything
if (!g_legacyWalkability) {
    FillGroundLevel();               // Fill z=0 with dirt + grass
}
int baseZ = g_legacyWalkability ? 0 : 1;  // Buildings start at z=1 in DF mode
```

### Placing a Building Floor
```c
PlaceFloor(x, y, z);  // Handles both modes, clears grass overlay
```

### Checking Walkability
```c
if (IsCellWalkableAt(z, y, x)) { ... }  // Works in both modes
```

### Affecting Ground Below Mover
```c
// Movers walk at z=1, but ground is at z=0
int groundZ = (cell at z is not dirt && z > 0 && cell at z-1 is dirt) ? z-1 : z;
```

## Systems That Need Z-Awareness

These systems must account for movers being one z-level above the ground:

| System | How it handles DF mode |
|--------|----------------------|
| Ground wear | Checks z-1 for dirt when trampling |
| Fire | Burns surface at the dirt's z-level |
| Rendering | Draws grass overlay on z=0, movers at z=1 |

## Toggle

Press **F7** at runtime to toggle between DF mode and legacy mode.

Variable: `g_legacyWalkability` (false = DF mode, true = legacy)

## Quick Reference

```
DF MODE (default):
- Walk ON TOP of solid ground
- z=0 = dirt with grass overlay
- z=1 = where movers walk on natural terrain
- Constructed floors use HAS_FLOOR flag on CELL_AIR

LEGACY MODE:
- Walk INSIDE walkable cells  
- CELL_FLOOR and CELL_GRASS have CF_WALKABLE flag
- Simpler but less flexible for multi-level terrain
```
