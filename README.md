```
███╗   ██╗ █████╗ ██╗   ██╗██╗  ██╗██╗████████╗    ┌───┬───────┐
████╗  ██║██╔══██╗██║   ██║██║ ██╔╝██║╚══██╔══╝    │ @ │       │
██╔██╗ ██║███████║██║   ██║█████╔╝ ██║   ██║       │ · ├── │   │
██║╚██╗██║██╔══██║╚██╗ ██╔╝██╔═██╗ ██║   ██║       │ · · · │   │
██║ ╚████║██║  ██║ ╚████╔╝ ██║  ██╗██║   ██║       ├───┴── │ ◎ │
╚═╝  ╚═══╝╚═╝  ╚═╝  ╚═══╝  ╚═╝  ╚═╝╚═╝   ╚═╝       └───────┴───┘
```

3D grid-based colony simulation framework. Pathfinding (A*, HPA*, JPS), jobs, movers, items, stockpiles, and cellular automata simulations (water, fire, smoke). C + raylib.

See [src/README.md](src/README.md) for architecture details.

## Structure

| Directory | Description |
|-----------|-------------|
| **`/src`** | **Main project** — Grid pathfinding (A*, HPA*, JPS), movers, jobs, items, stockpiles, water/fire/smoke simulation — [overview](documentation/overview.md) |
| `/experiments/steering` | Old experiment: steering behaviors, flocking, social force model |
| `/experiments/crowd` | Old experiment: crowd simulation testbed |
| `/tools` | Atlas generator, font embedder |
| `/documentation` | Design docs — [todo](documentation/todo.md) |

## Build

```bash
make                        # builds path, steer, crowd, path8
make test                   # runs all tests
make debug                  # debug build with sanitizers
make clean                  # removes bin/
```

### Atlas

```bash
make atlas                  # generates 8x8 atlas only (default)
make atlas GENERATE_16X16=1 # generates both 8x8 and 16x16 atlases with validation
make clean-atlas            # removes all generated atlas files
make clean-atlas atlas      # clean rebuild of atlas
```

To change the default, edit `GENERATE_16X16 ?= 0` in the Makefile.

### Tile size variants

```bash
make path8                  # build with 8x8 tiles (included in default build)
make path16                 # build with 16x16 tiles
```

### Embedded assets

All assets (atlas textures, fonts) are embedded directly in the binary - no external files needed at runtime.

```bash
make embed                  # regenerate all embedded assets (atlas + font)
make embed_font             # regenerate embedded font only
make atlas                  # regenerate embedded atlas only
```

#### Changing the font

1. Get a BMFont-format font (`.fnt` + `.png` files). Tools like [BMFont](https://www.angelcode.com/products/bmfont/) or [Hiero](https://libgdx.com/wiki/tools/hiero) can generate these.
2. Place both files in `assets/fonts/` with the same base name (e.g., `myfont.fnt` and `myfont.png`)
3. Update the Makefile `embed_font` target to point to your font:
   ```makefile
   ./$(BINDIR)/font_embed assets/fonts/myfont.fnt assets/fonts/myfont_embedded.h
   ```
4. Update the include in `src/main.c`:
   ```c
   #include "assets/fonts/myfont_embedded.h"
   ```
5. Run `make embed_font && make`

#### Using 16x16 tiles

1. Create 16x16 versions of all sprites in `assets/textures/` (must have same filenames as `assets/textures8x8/`)
2. Generate both atlases with validation:
   ```bash
   make atlas GENERATE_16X16=1
   ```
3. Build the 16x16 variant:
   ```bash
   make path16
   ```
4. Run with `./bin/path16`

The atlas generator validates that both atlases have matching sprite names.

## Run

```bash
./bin/path    # pathfinding demo
./bin/steer   # steering demo
./bin/crowd   # crowd demo
```

Requires raylib.

## Save/Load & Inspector

In-game:
- **F5** - Quick save to `debug_save.bin` + archived copy to `saves/YYYY-MM-DD_HH-MM-SS.bin.gz`
- **F6** - Quick load from `debug_save.bin`

Inspect save files from the command line (works with `.bin` and `.gz` files):

```bash
./bin/path --inspect debug_save.bin                    # summary
./bin/path --inspect saves/bug_name.bin.gz             # works with gzipped files too
./bin/path --inspect debug_save.bin --mover 0          # mover details
./bin/path --inspect debug_save.bin --item 5           # item details
./bin/path --inspect debug_save.bin --job 3            # job details
./bin/path --inspect debug_save.bin --stockpile 0      # stockpile details
./bin/path --inspect debug_save.bin --cell 10,20,0     # cell at (x,y,z)
./bin/path --inspect debug_save.bin --designations     # all designations (MINE/CHANNEL/etc)
./bin/path --inspect debug_save.bin --stuck            # movers stuck > 2s
./bin/path --inspect debug_save.bin --reserved         # items with reservations
./bin/path --inspect debug_save.bin --jobs-active      # all active jobs
./bin/path --inspect debug_save.bin --entrances        # HPA* graph: chunk entrances summary
./bin/path --inspect debug_save.bin --entrances 1      # HPA* entrances at z=1 only

# Path testing with algorithm selection
./bin/path --inspect debug_save.bin --path 8,17,1 12,16,1              # test path (A* default)
./bin/path --inspect debug_save.bin --path 8,17,1 12,16,1 --algo hpa   # test with HPA*
./bin/path --inspect debug_save.bin --path 8,17,1 12,16,1 --algo jps   # test with JPS
```

Bug save workflow: F5 to save → rename `saves/*.bin.gz` to something meaningful → inspect later.

## Headless Mode

Run simulation without GUI for debugging and automated testing:

```bash
# Run 100 ticks (default)
./bin/path --headless --load save.bin.gz

# Run custom number of ticks
./bin/path --headless --load save.bin.gz --ticks 500

# Run and show mover state after
./bin/path --headless --load save.bin.gz --ticks 100 --mover all
./bin/path --headless --load save.bin.gz --ticks 100 --mover 0

# Run and save result
./bin/path --headless --load save.bin.gz --ticks 100 --save /tmp/after.bin
```

Output includes tick count, performance (ms/tick), movers stuck in non-walkable cells before/after, and movers with no pathfinding progress.
