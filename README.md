```
███╗   ██╗ █████╗ ██╗   ██╗██╗  ██╗██╗████████╗    ┌───┬───────┐
████╗  ██║██╔══██╗██║   ██║██║ ██╔╝██║╚══██╔══╝    │ @ │       │
██╔██╗ ██║███████║██║   ██║█████╔╝ ██║   ██║       │ · ├── │   │
██║╚██╗██║██╔══██║╚██╗ ██╔╝██╔═██╗ ██║   ██║       │ · · · │   │
██║ ╚████║██║  ██║ ╚████╔╝ ██║  ██╗██║   ██║       ├───┴── │ ◎ │
╚═╝  ╚═══╝╚═╝  ╚═╝  ╚═══╝  ╚═╝  ╚═╝╚═╝   ╚═╝       └───────┴───┘
```

~~2D~~ 3D navigation and movement for games. C + raylib.

## Structure

| Directory | Description |
|-----------|-------------|
| `/pathing` | Grid pathfinding (A*, HPA*, JPS), movers, wall handling, terrain — [overview](documentation/overview.md) |
| `/steering` | Steering behaviors, flocking, social force model — [reference](documentation/done/steering.md) |
| `/crowd-experiment` | Crowd simulation testbed |
| `/tools` | Atlas generator |
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
./bin/path --inspect debug_save.bin --stuck            # movers stuck > 2s
./bin/path --inspect debug_save.bin --reserved         # items with reservations
./bin/path --inspect debug_save.bin --jobs-active      # all active jobs
```

Bug save workflow: F5 to save → rename `saves/*.bin.gz` to something meaningful → inspect later.
