# Source Code Structure

```
src/
├── unity.c           # Unity build entry point - includes all .c files
├── main.c            # Game loop, global state definitions, helper functions
├── game_state.h      # Shared globals (extern declarations for all state)
│
├── core/             # Core systems
│   ├── input.c       # Keyboard/mouse input handling (HandleInput)
│   ├── saveload.c    # World save/load to disk (SaveWorld, LoadWorld)
│   ├── inspect.c/h   # Save file inspector tool
│
├── render/           # All drawing code
│   ├── rendering.c   # Grid, items, movers, water, fire, smoke drawing
│   ├── tooltips.c    # Hover tooltips for stockpiles, movers, items, water
│   ├── ui_panels.c   # UI panels (DrawUI, DrawProfilerPanel)
│
├── world/            # World/map systems
│   ├── grid.c/h      # Grid storage, cell types, chunks, ladder logic
│   ├── terrain.c/h   # Terrain generators (caves, dungeons, mazes, etc.)
│   ├── pathfinding.c/h  # A*, JPS, JPS+, HPA* pathfinding
│   ├── designations.c/h # Mining/building designations
│
├── entities/         # Game entities
│   ├── mover.c/h     # Mover movement, avoidance, spatial grid
│   ├── jobs.c/h      # Job system, work givers, task assignment
│   ├── items.c/h     # Items, item types, spatial grid
│   ├── stockpiles.c/h # Stockpile storage and filters
│
└── simulation/       # Cellular automata simulations
    ├── water.c/h     # Water pressure and flow
    ├── fire.c/h      # Fire spread and fuel consumption
    ├── smoke.c/h     # Smoke propagation
    └── groundwear.c/h # Ground wear from mover traffic
```

## Build

Uses unity build pattern - compile only `unity.c`:
```
cc -o demo src/unity.c [flags]
```

## Key Concepts

- **Unity build**: All .c files included into one translation unit via unity.c
- **Single-header libs**: ui.h and profiler.h use `#define *_IMPLEMENTATION` pattern
- **Shared state**: Global variables declared `extern` in game_state.h, defined in main.c
- **Chunk-based grid**: World divided into chunks for efficient pathfinding updates
