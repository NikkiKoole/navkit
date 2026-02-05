# NavKit

3D grid-based colony simulation framework in C with raylib. Pathfinding (A*, HPA*, JPS, JPS+), jobs, movers, items, stockpiles, workshops, and cellular automata (water, fire, smoke).

## Build & Run

```bash
make path         # build main binary (not just `make`)
./bin/path        # run
make test         # run tests
make debug        # debug build with sanitizers
```

## Debug Tips

- Use `LOG_INFO` or `LOG_WARNING` for debug output (`LOG_DEBUG` not visible by default)

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
├── todo/           # Actionable tasks (pick anything)
│   ├── architecture/   # Big refactors, roadmaps, tests
│   ├── environmental/  # Fire, water, temperature systems
│   ├── items/          # Items, stockpiles, containers
│   ├── jobs/           # Job system, zones, crafting
│   ├── nature/         # Plants, soil, moisture
│   ├── pathfinding/    # Navigation, movement, spatial
│   ├── rendering/      # Graphics, sorting, culling
│   └── world/          # Terrain, building, z-levels
├── doing/          # Active work (one thing at a time)
├── done/           # Completed features (same categories as todo/)
│   └── legacy/     # Old handoffs, archived docs
├── bugs/           # Known bugs (open and fixed)
├── vision/         # Future ideas, not yet planned
```

## Workflow

1. **Check `doing/`** - If something is there, continue working on it
2. **If `doing/` is empty** - Show a summary of all `todo/` items by category
3. **Pick anything from `todo/`** - All items have equal weight, no priority order
4. **Move the item to `doing/`** - This is now the active work
5. **Work on it** - Update the doc as you go
6. **When done** - Move the file to `done/<category>/`

## Default Start (when `doing/` is empty)

Show hierarchical overview of `todo/`:

```
todo/
├── architecture/   (N files)
│   └── file1.md - brief description
├── jobs/           (N files)
│   └── ...
...
```

Let user pick what to work on.

## Guidelines

- Only one thing in `doing/` at a time (focus)
- All items in `todo/` have equal weight - pick what feels right
- Update docs as you implement
- If a todo spawns new todos, add them to `todo/<category>/`
- When moving to `done/`, preserve the category folder structure

---

# Feature Completion Checklist

Before marking any job/feature as "done", verify:

## 1. Write End-to-End Tests

Create a test that runs the full flow:
- Minimal world setup
- Trigger job assignment
- Run simulation ticks
- Verify expected outcome

If the test doesn't pass, the feature isn't done.

## 2. Verify In-Game or Headless

```bash
./bin/path --headless --load save.bin.gz --ticks 500
```

Don't trust "it compiles" - actually see movers do the job.

## 3. Job System Wiring Checklist

For any new job type:
- [ ] Function declared in .h file
- [ ] Function implemented in .c file (not just declared!)
- [ ] Added to `jobDrivers[]` table
- [ ] Added to `AssignJobs()` with `hasXXXWork` flag
- [ ] WorkGiver checks the correct capability
- [ ] Capability field exists in `MoverCapabilities` struct
- [ ] Capability initialized in `InitMover()` and save/load
- [ ] Simulation tick function called in `TickWithDt()` if needed
- [ ] Inspector updated to show new capability/state

## 4. Use Inspector to Verify State

```bash
./bin/path --inspect save.bin.gz --designations    # See waiting work
./bin/path --inspect save.bin.gz --mover 0         # Check capabilities
./bin/path --inspect save.bin.gz --jobs-active     # See assigned jobs
```

## 5. UI for Testing

- Is there a way to trigger/test this feature from the UI?
- Are relevant parameters exposed in ui_panels.c for tuning?
- Does the inspector show the new state?

## 6. Reuse Existing Systems

Before writing new code, check if existing systems already handle it:
- Existing job patterns (haul, mine, build, chop)
- Existing designation patterns
- Existing item/stockpile patterns

Copy patterns from similar features rather than inventing new approaches.

## 7. Gate for Moving to `done/`

**Never move a doc to `done/` without explicit user permission.**

Before asking, verify:
- End-to-end test passes
- You've seen it work in-game or headless
- Inspector shows correct state

Then ask the user if it can be moved to `done/`.
