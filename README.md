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
make          # builds path, steer, crowd
make atlas    # builds and runs texture atlas generator
make test     # runs all tests
make debug    # debug build with sanitizers
```

## Run

```bash
./bin/path    # pathfinding demo
./bin/steer   # steering demo
./bin/crowd   # crowd demo
```

Requires raylib.
