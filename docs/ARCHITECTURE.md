# NavKit Architecture

**READ THIS FIRST** - This document helps you quickly understand the codebase.

## What Is This?

A Dwarf Fortress-inspired simulation engine in C with:
- 3D grid-based world with z-levels
- Hierarchical pathfinding (A*, HPA*, JPS, JPS+)
- 10k movers with steering/avoidance
- Jobs system (haul, dig, build)
- Physics simulations (water, fire, temperature, smoke, steam)
- Ground wear (paths form from foot traffic)

## Docs Structure

```
docs/
├── ARCHITECTURE.md      <- YOU ARE HERE - read first
├── core/                <- Current authoritative docs (READ THESE)
├── archive/             <- Historical docs (SKIP unless debugging old decisions)
│   ├── done/            <- Completed implementation plans
│   └── old-plans/       <- Superseded plans
├── ideas/               <- Future ideas, brainstorming (LOW PRIORITY)
└── bugs.md              <- Known bugs
```

## Core Concepts

### DF-Style Walkability (IMPORTANT)

**Read `core/df-walkability-model.md` for full details.**

Key insight: Movers stand ON TOP of solid ground, not inside it.

```
z=1: [AIR]   <- movers walk here
z=0: [DIRT]  <- solid ground with grass overlay
```

This affects many systems - when code needs to affect "the ground a mover is on", it often needs to check z-1.

### Main Loop (src/main.c)

```
while running:
  HandleInput()
  if !paused:
    Tick()              # Movers, physics simulations
    AssignJobs()        # Match idle movers to work
    JobsTick()          # Execute job state machines
  Render()
```

### Key Subsystems

| System | Location | Notes |
|--------|----------|-------|
| Grid/Terrain | `src/world/grid.c`, `terrain.c` | 3D cells + flags |
| Walkability | `src/world/cell_defs.h` | `IsCellWalkableAt()` |
| Pathfinding | `src/world/pathfinding.c` | A*, HPA*, JPS, JPS+ |
| Movers | `src/entities/mover.c` | Movement, avoidance |
| Jobs | `src/entities/jobs.c` | Haul, dig, build |
| Items | `src/entities/items.c` | 25k items, spatial grid |
| Stockpiles | `src/entities/stockpiles.c` | Storage zones |
| Water | `src/simulation/water.c` | Pressure-based flow |
| Fire | `src/simulation/fire.c` | Spread, fuel burning |
| Temperature | `src/simulation/temperature.c` | Heat transfer |
| Ground Wear | `src/simulation/groundwear.c` | Trampling paths |

### Data Storage

**Cell types** (`grid[z][y][x]`): What the cell IS - dirt, air, wall, ladder

**Cell flags** (`cellFlags[z][y][x]`): Per-cell properties:
- Bit 5: `HAS_FLOOR` - constructed floor
- Bits 3-4: Surface overlay (grass type)
- Bits 1-2: Wetness level
- Bit 0: Burned flag

### Common Patterns

**Terrain generators:**
```c
InitGrid();
if (!g_legacyWalkability) FillGroundLevel();  // z=0 = dirt
int baseZ = g_legacyWalkability ? 0 : 1;      // buildings start at z=1
```

**Affecting ground below mover:**
```c
// Mover at z=1 walks on dirt at z=0
if (cell != CELL_DIRT && z > 0 && grid[z-1][y][x] == CELL_DIRT) {
    targetZ = z - 1;
}
```

**Sparse simulation updates:**
- Use stability flags - only process unstable cells
- BFS with generation counters to avoid revisiting

## File Structure

```
src/
├── main.c              # Entry point, main loop
├── core/               # Input, time, save/load
├── world/              # Grid, pathfinding, terrain
├── entities/           # Movers, items, jobs, stockpiles  
├── simulation/         # Water, fire, temp, smoke, steam, ground wear
└── render/             # Drawing, UI panels
```

## Quick Reference

| Toggle | Key | Variable |
|--------|-----|----------|
| Walkability mode | F7 | `g_legacyWalkability` |
| Pause | Space | - |
| Z-level | < / > | `currentViewZ` |
| Save/Load | F5 / F6 | - |

## When Adding Features

1. Check if it needs z-level awareness (DF mode: ground at z-1)
2. Use existing spatial grids for queries (mover grid, item grid)
3. Follow sparse update pattern for simulations
4. Add tests in `tests/`
