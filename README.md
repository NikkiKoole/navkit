```
███╗   ██╗ █████╗ ██╗   ██╗██╗  ██╗██╗████████╗    ┌───┬───────┐
████╗  ██║██╔══██╗██║   ██║██║ ██╔╝██║╚══██╔══╝    │ @ │       │
██╔██╗ ██║███████║██║   ██║█████╔╝ ██║   ██║       │ · ├── │   │
██║╚██╗██║██╔══██║╚██╗ ██╔╝██╔═██╗ ██║   ██║       │ · · · │   │
██║ ╚████║██║  ██║ ╚████╔╝ ██║  ██╗██║   ██║       ├───┴── │ ◎ │
╚═╝  ╚═══╝╚═╝  ╚═╝  ╚═══╝  ╚═╝  ╚═╝╚═╝   ╚═╝       └───────┴───┘
```

3D grid-based colony simulation framework. Pathfinding (A*, HPA*, JPS), jobs, movers, items, stockpiles, and cellular automata simulations (water, fire, smoke). C + raylib.

## Structure

| Directory | Description |
|-----------|-------------|
| **`/src`** | **Main project** (~50k lines of C) — grid pathfinding (A*, HPA*, JPS, JPS+), movers, jobs, items, stockpiles, workshops, water/fire/smoke/temperature simulation |
| `/tests` | Unit tests and benchmarks (c89spec framework) |
| `/docs` | Design docs, task tracking (todo/doing/done workflow) |
| `/tools` | Atlas generator, font embedder |
| `/experiments` | Old experiments: steering behaviors, crowd simulation |

## Build

```bash
make                        # builds path, steer, crowd, path8
make test                   # runs all tests (~9s, mover stress tests use 5 iterations)
make test-quick             # fast tests (~4s, skips mover tests entirely)
make test-full              # full tests (~37s, mover stress tests use 20 iterations)
make debug                  # debug build with sanitizers
make clean                  # removes build/
```

### Windows Cross-Compilation

Build a Windows `.exe` from macOS using mingw-w64:

```bash
brew install mingw-w64      # install cross-compiler (one-time)
make windows                # builds build/win64/path.exe
```

The output is a statically linked single `.exe` — no DLLs needed. Raylib is compiled from the vendored source for the Windows target automatically.

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
   ./build/bin/font_embed assets/fonts/myfont.fnt assets/fonts/myfont_embedded.h
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
4. Run with `./build/bin/path16`

The atlas generator validates that both atlases have matching sprite names.

## Run

```bash
./build/bin/path                # pathfinding demo
./build/bin/path --seed 12345   # run with specific world seed (for reproducible terrain)
./build/bin/steer               # steering demo
./build/bin/crowd               # crowd demo
```

Raylib is vendored in `vendor/raylib/` and built automatically — no external dependencies needed.

## Save/Load & Inspector

In-game:
- **F5** - Quick save to `debug_save.bin` + archived copy to `saves/YYYY-MM-DD_HH-MM-SS.bin.gz`
- **F6** - Quick load from `debug_save.bin`

Inspect save files from the command line (works with `.bin` and `.gz` files):

```bash
./build/bin/path --inspect debug_save.bin                    # summary (includes world seed)
./build/bin/path --inspect saves/bug_name.bin.gz             # works with gzipped files too
./build/bin/path --inspect debug_save.bin --mover 0          # mover details
./build/bin/path --inspect debug_save.bin --item 5           # item details
./build/bin/path --inspect debug_save.bin --job 3            # job details
./build/bin/path --inspect debug_save.bin --stockpile 0      # stockpile details
./build/bin/path --inspect debug_save.bin --cell 10,20,0     # cell at (x,y,z)
./build/bin/path --inspect debug_save.bin --designations     # all designations (MINE/CHANNEL/etc)
./build/bin/path --inspect debug_save.bin --stuck            # movers stuck > 2s
./build/bin/path --inspect debug_save.bin --reserved         # items with reservations
./build/bin/path --inspect debug_save.bin --jobs-active      # all active jobs
./build/bin/path --inspect debug_save.bin --entrances        # HPA* graph: chunk entrances summary
./build/bin/path --inspect debug_save.bin --entrances 1      # HPA* entrances at z=1 only

# Path testing with algorithm selection
./build/bin/path --inspect debug_save.bin --path 8,17,1 12,16,1              # test path (A* default)
./build/bin/path --inspect debug_save.bin --path 8,17,1 12,16,1 --algo hpa   # test with HPA*
./build/bin/path --inspect debug_save.bin --path 8,17,1 12,16,1 --algo jps   # test with JPS
```

Bug save workflow: F5 to save → rename `saves/*.bin.gz` to something meaningful → inspect later.

## Headless Mode

Run simulation without GUI for debugging and automated testing:

```bash
# Run 100 ticks (default)
./build/bin/path --headless --load save.bin.gz

# Run custom number of ticks
./build/bin/path --headless --load save.bin.gz --ticks 500

# Run and show mover state after
./build/bin/path --headless --load save.bin.gz --ticks 100 --mover all
./build/bin/path --headless --load save.bin.gz --ticks 100 --mover 0

# Run and save result
./build/bin/path --headless --load save.bin.gz --ticks 100 --save /tmp/after.bin
```

Output includes tick count, performance (ms/tick), movers stuck in non-walkable cells before/after, and movers with no pathfinding progress.
