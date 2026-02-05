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

## Atlas / Tiles

- We are **8x8-only** for now. The 16x16 atlas is not maintained.
- If 16x16 is needed again, regenerate the atlas and update 16x16 sprite entries to match 8x8.

## Code Structure

```
src/
├── main.c              # Entry point
├── game_state.h        # Global state
├── unity.c             # Unity build (includes all .c files)
├── core/               # Time, input, save/load, inspector
├── world/              # Grid, cells, pathfinding, terrain
├── entities/           # Movers, jobs, items, stockpiles, workshops
├── simulation/         # Water, fire, smoke, temperature, trees
└── render/             # Drawing, UI, tooltips
```

~27,000 lines of C total.

## Key Systems

Pathfinding (4 algos), movers (agents with avoidance), jobs (designations → work), stockpiles (filtered storage), workshops (crafting), simulations (water/fire/smoke).

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

See `IsCellWalkableAt_Standard()` in `src/world/cell_defs.h`.

## Save/Load & Inspector

```bash
# In-game
F5                    # Quick save
F6                    # Quick load

# CLI inspection
./bin/path --inspect save.bin              # Summary
./bin/path --inspect save.bin --mover 0    # Mover details
./bin/path --inspect save.bin --stuck      # Movers stuck > 2s
./bin/path --inspect save.bin --jobs-active
./bin/path --inspect save.bin --path 8,17,1 12,16,1 --algo hpa
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
├── doing/          # Active work (one thing at a time)
├── done/           # Completed features
├── bugs/           # Known bugs
├── vision/         # Future ideas
└── checklists/     # Completion checklists
```

## Workflow

1. **Check `doing/`** - If something is there, continue working on it
2. **If `doing/` is empty** - Show summary of `todo/` items, let user pick
3. **Move picked item to `doing/`** - Work on it, update doc as you go
4. **When done** - Ask user permission, then move to `done/<category>/`

## Guidelines

- Only one thing in `doing/` at a time
- Never move to `done/` without user permission
- See `docs/checklists/feature-completion.md` before completing features
