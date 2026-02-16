# NavKit

3D grid-based colony simulation framework in C with raylib. Pathfinding (A*, HPA*, JPS, JPS+), jobs, movers, items, stockpiles, workshops, and cellular automata (water, fire, smoke).

## Build & Run

```bash
make path              # build main binary (not just `make`)
./build/bin/path       # run
make test              # run tests
make debug             # debug build with sanitizers
```

## Unity Build

This project uses a **unity build** — `src/unity.c` and `tests/test_unity.c` `#include` all `.c` files into a single translation unit. This means:

- **`make path` is all you need** after editing any `.c` or `.h` file. The Makefile tracks all source files as dependencies, so it rebuilds automatically when anything changes.
- **`make clean` is NOT normally needed.** If a build fails with bizarre errors (wrong enum values, missing symbols), run `make clean && make path` — but this should be rare.
- **Do NOT create new `.c` files** without adding them to `src/unity.c` (and `tests/test_unity.c` if they contain game logic needed by tests). New headers are picked up automatically.

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

~50,000 lines of C total. Per-folder CLAUDE.md files in `src/entities/`, `src/world/`, `src/core/`, and `tests/` have detailed patterns and gotchas.

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
./build/bin/path --inspect save.bin              # Summary
./build/bin/path --inspect save.bin --mover 0    # Mover details
./build/bin/path --inspect save.bin --stuck       # Movers stuck > 2s
./build/bin/path --inspect save.bin --jobs-active
./build/bin/path --inspect save.bin --path 8,17,1 12,16,1 --algo hpa
```

## Headless Mode

```bash
./build/bin/path --headless --load save.bin.gz --ticks 100
```
