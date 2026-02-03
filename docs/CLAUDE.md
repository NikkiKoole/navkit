# NavKit

3D grid-based colony simulation framework in C with raylib. Pathfinding (A*, HPA*, JPS, JPS+), jobs, movers, items, stockpiles, workshops, and cellular automata (water, fire, smoke).

## Build & Run

```bash
make              # build
./bin/path        # run
make test         # run tests
make debug        # debug build with sanitizers
```

## Code Structure

```
src/
├── main.c              # Entry point
├── game_state.h        # Global state
├── unity.c             # Unity build (includes all .c files)
├── core/
│   ├── time.c/h        # Game time, day/night cycle
│   ├── input.c         # Input handling
│   ├── input_mode.c/h  # Mode state machine
│   ├── saveload.c      # Binary save/load
│   └── inspect.c/h     # CLI inspector
├── world/
│   ├── grid.c/h        # 3D grid, chunks, cell access
│   ├── cell_defs.c/h   # Cell types and flags
│   ├── pathfinding.c/h # A*, HPA*, JPS, JPS+
│   ├── terrain.c/h     # Procedural generators
│   └── designations.c/h # Mine/channel/chop designations
├── entities/
│   ├── mover.c/h       # Agents, avoidance, path following
│   ├── jobs.c/h        # Job system, drivers
│   ├── items.c/h       # Item spawning, spatial grid
│   ├── item_defs.c/h   # Item type definitions
│   ├── stockpiles.c/h  # Storage, filters, priorities
│   └── workshops.c/h   # Crafting, bills, recipes
├── simulation/
│   ├── water.c/h       # Cellular automata water
│   ├── fire.c/h        # Fire spread
│   ├── smoke.c/h       # Smoke propagation
│   ├── steam.c/h       # Steam from water+fire
│   ├── temperature.c/h # Heat simulation
│   ├── trees.c/h       # Tree growth
│   └── groundwear.c/h  # Path wear from foot traffic
└── render/
    ├── rendering.c     # Drawing, frustum culling
    ├── ui_panels.c     # Sidebar UI
    └── tooltips.c      # Hover tooltips
```

~27,000 lines of C total.

## Walkability Model (DF-style)

Movers stand **on top** of solid ground, not inside it:

```
z=2:  [AIR]     <- walkable if z=1 is solid
z=1:  [DIRT]    <- solid ground
z=0:  [DIRT]    <- solid ground
```

A cell is walkable if:
1. Not blocked (not a wall)
2. Not solid itself (can't walk inside dirt)
3. Has support: solid cell below, OR has `HAS_FLOOR` flag, OR is a ladder/ramp, OR z=0

**Cell types** (`grid[z][y][x]`): What the cell IS - dirt, air, wall, ladder

**Cell flags** (`cellFlags[z][y][x]`): Per-cell properties - `HAS_FLOOR`, surface overlay (grass), wetness, burned

See `IsCellWalkableAt_Standard()` in `src/world/cell_defs.h`.

## Key Systems

**Pathfinding**: 4 algorithms on a chunked 3D grid. JPS+ is fastest for static terrain. Ladders connect z-levels via a precomputed graph (Floyd-Warshall).

**Movers**: Agents that follow paths. Avoidance via spatial grid with prefix sums. Wall sliding, stuck detection, replanning.

**Jobs**: Designations (mine, channel, chop) generate jobs. Movers claim and execute jobs. Hauling moves items to stockpiles.

**Stockpiles**: Filtered storage with priorities. Higher priority stockpiles pull from lower. Slot reservation system.

**Workshops**: Crafting stations with bills. Modes: do X times, do until X, do forever. Haulers deliver inputs, crafters work, outputs go to stockpiles.

**Simulations**: Water flows, fire spreads, smoke rises. Ground wear creates visible paths. Trees grow from saplings.

## Save/Load & Inspector

```bash
# In-game
F5                    # Quick save
F6                    # Quick load

# CLI inspection
./bin/path --inspect debug_save.bin              # Summary
./bin/path --inspect debug_save.bin --mover 0    # Mover details
./bin/path --inspect debug_save.bin --item 5     # Item details
./bin/path --inspect debug_save.bin --job 3      # Job details
./bin/path --inspect debug_save.bin --stockpile 0
./bin/path --inspect debug_save.bin --stuck      # Movers stuck > 2s
./bin/path --inspect debug_save.bin --jobs-active
./bin/path --inspect debug_save.bin --path 8,17,1 12,16,1 --algo jps
```

## Headless Mode

```bash
./bin/path --headless --load save.bin.gz --ticks 100
```

---

# Docs Workflow

## Folder Structure

```
docs/
├── todo/     # Actionable tasks (pick anything)
├── doing/    # Active work (one thing at a time)
├── done/     # Completed features (archive)
├── bugs/     # Known bugs (open and fixed)
├── vision/   # Future ideas, not yet planned
```

## Workflow

1. **Check `doing/`** - If something is there, continue working on it
2. **If `doing/` is empty** - Pick anything from `todo/` (no priority order)
3. **Move the item to `doing/`** - This is now the active work
4. **Work on it** - Update the doc as you go
5. **When done** - Move the file to `done/`

## Guidelines

- Only one thing in `doing/` at a time (focus)
- All items in `todo/` have equal weight - pick what feels right
- Update docs as you implement
- If a todo spawns new todos, add them to `todo/`
