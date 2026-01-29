# NavKit Architecture

**READ THIS FIRST** - This document helps you quickly understand the codebase.

## What Is This?

A Dwarf Fortress-inspired simulation engine in C with:
- 3D grid-based world with z-levels
- Hierarchical pathfinding (A*, HPA*, JPS, JPS+)
- 10k movers with steering/avoidance
- Jobs system (haul, dig, build, craft)
- Workshops with bills and recipes
- Physics simulations (water, fire, temperature, smoke, steam)
- Ground wear (paths form from foot traffic)

## Docs Structure

```
docs/
├── ARCHITECTURE.md      <- YOU ARE HERE
├── FEATURE_PROCESS.md   <- How to build new features
├── current/             <- Active feature workspace (one at a time)
├── archive/             <- Completed features and old docs
│   ├── crafting-system/
│   ├── pathfinding/
│   ├── z-levels/
│   ├── jobs-and-hauling/
│   ├── cells-and-terrain/
│   ├── environmental-systems/
│   └── meta/
├── ideas/               <- Future ideas, brainstorming
│   ├── jobs/
│   ├── entropy-and-simulation/
│   ├── terrain-and-tiles/
│   ├── farming/
│   ├── architecture/
│   └── meta/
└── bugs.md              <- Known bugs
```

---

## DF-Style Walkability Model

This is the **default** and most important concept to understand.

### Core Concept

In DF mode, you stand **ON TOP** of solid ground, not inside it.

```
z=2:  [AIR]     <- can walk here if z=1 is solid
z=1:  [DIRT]    <- solid ground (CF_SOLID flag)
z=0:  [DIRT]    <- solid ground
```

Typical terrain layout:

| Z-Level | Contains | Notes |
|---------|----------|-------|
| z=0 | `CELL_DIRT` + grass surface | Ground level, filled by `FillGroundLevel()` |
| z=1 | `CELL_AIR` or buildings | Where movers walk on natural terrain |
| z=2+ | `CELL_AIR` or upper floors | Multi-story buildings |

**Key insight**: Movers at z=1 are walking on the dirt at z=0, not inside it.

### Walkability Rules

A cell at (x, y, z) is walkable if ALL of these are true:

1. The cell doesn't block movement (not a wall)
2. The cell isn't solid itself (can't walk inside dirt/walls)
3. One of these support conditions:
   - Cell below (z-1) is solid (`CF_SOLID` flag)
   - Cell has `HAS_FLOOR` flag (constructed floor)
   - Cell is a ladder or ramp (self-supporting)
   - z=0 (implicit bedrock below)

See `IsCellWalkableAt_Standard()` in `src/world/cell_defs.h`.

### Two Ways to Make a Floor

**1. Natural Ground** (solid cell below):
```c
grid[0][y][x] = CELL_DIRT;  // z=0: solid ground
grid[1][y][x] = CELL_AIR;   // z=1: walkable (dirt below)
```

**2. Constructed Floor** (HAS_FLOOR flag):
```c
grid[1][y][x] = CELL_AIR;   // z=1: air
SET_FLOOR(x, y, 1);         // z=1: now walkable (balcony/bridge)
grid[0][y][x] = CELL_AIR;   // z=0: can be empty!
```

### Cell Flags vs Cell Types

**Cell types** (`grid[z][y][x]`): What the cell IS - dirt, air, wall, ladder

**Cell flags** (`cellFlags[z][y][x]`): Per-cell properties:
- Bit 5: `HAS_FLOOR` - constructed floor
- Bits 3-4: Surface overlay (grass type)
- Bits 1-2: Wetness level
- Bit 0: Burned flag

### Surface Overlays (Grass)

Grass is NOT a cell type. It's a surface overlay on `CELL_DIRT`:

```c
grid[0][y][x] = CELL_DIRT;
SET_CELL_SURFACE(x, y, 0, SURFACE_TALL_GRASS);  // Visual only
```

Surface types: `SURFACE_BARE`, `SURFACE_TRAMPLED`, `SURFACE_GRASS`, `SURFACE_TALL_GRASS`

The ground wear system changes surface overlays based on foot traffic.

### Common Patterns

**Terrain generator setup:**
```c
InitGrid();
if (!g_legacyWalkability) FillGroundLevel();  // z=0 = dirt
int baseZ = g_legacyWalkability ? 0 : 1;      // buildings start at z=1
```

**Affecting ground below mover:**
```c
// Mover at z=1 walks on dirt at z=0
int groundZ = (z > 0 && grid[z-1][y][x] == CELL_DIRT) ? z-1 : z;
```

**Toggle:** Press **F7** to switch between DF mode and legacy mode.

---

## Main Loop (src/main.c)

```
while running:
  HandleInput()
  if !paused:
    Tick()              # Movers, physics simulations
    AssignJobs()        # Match idle movers to work
    JobsTick()          # Execute job state machines
  Render()
```

---

## Key Subsystems

| System | Location | Notes |
|--------|----------|-------|
| Grid/Terrain | `src/world/grid.c`, `terrain.c` | 3D cells + flags |
| Walkability | `src/world/cell_defs.h` | `IsCellWalkableAt()` |
| Pathfinding | `src/world/pathfinding.c` | A*, HPA*, JPS, JPS+ |
| Movers | `src/entities/mover.c` | Movement, avoidance |
| Jobs | `src/entities/jobs.c` | Haul, dig, build, craft |
| Workshops | `src/entities/workshops.c` | Bills, recipes, crafting |
| Items | `src/entities/items.c` | 25k items, spatial grid |
| Stockpiles | `src/entities/stockpiles.c` | Storage zones |
| Water | `src/simulation/water.c` | Pressure-based flow |
| Fire | `src/simulation/fire.c` | Spread, fuel burning |
| Temperature | `src/simulation/temperature.c` | Heat transfer |
| Ground Wear | `src/simulation/groundwear.c` | Trampling paths |

---

## File Structure

```
src/
├── main.c              # Entry point, main loop
├── core/               # Input, time, save/load
├── world/              # Grid, pathfinding, terrain
├── entities/           # Movers, items, jobs, stockpiles, workshops
├── simulation/         # Water, fire, temp, smoke, steam, ground wear
└── render/             # Drawing, UI panels
```

---

## Quick Reference

| Toggle | Key | Variable |
|--------|-----|----------|
| Walkability mode | F7 | `g_legacyWalkability` |
| Pause | Space | - |
| Z-level | < / > | `currentViewZ` |
| Save/Load | F5 / F6 | - |

---

## When Adding Features

1. Check if it needs z-level awareness (DF mode: ground at z-1)
2. Use existing spatial grids for queries (mover grid, item grid)
3. Follow sparse update pattern for simulations
4. Add tests in `tests/`
5. Follow the process in `FEATURE_PROCESS.md`
