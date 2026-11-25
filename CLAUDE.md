# NavKit

3D grid-based colony simulation framework in C with raylib. Pathfinding (A*, HPA*), jobs, movers, items, stockpiles, workshops, and cellular automata (water, fire, smoke).

## Build & Run

```bash
make path              # build main binary (not just `make`)
./build/bin/path       # run
make test              # run tests
make test-tap          # run tests with machine-readable TAP output (no ANSI)
make scenarios-check   # every scenario headless with --check (also proves the GAME builds:
                       # make test doesn't compile main.c / scenarios.c) - run before committing
make debug             # debug build with sanitizers
```

## Unity Build

This project uses a **unity build** — `src/unity.c` and `tests/test_unity.c` `#include` all `.c` files into a single translation unit. This means:

- **`make path` is all you need** after editing any `.c` or `.h` file. The Makefile tracks all source files as dependencies, so it rebuilds automatically when anything changes.
- **`make clean` is NOT normally needed.** If a build fails with bizarre errors (wrong enum values, missing symbols), run `make clean && make path` — but this should be rare.
- **Do NOT create new `.c` files** without adding them to `src/unity.c` (and `tests/test_unity.c` if they contain game logic needed by tests). New headers are picked up automatically.

## LSP / Code Intelligence

A `compile_commands.json` is generated so that **clangd** can index each `.c` file individually (the unity build would otherwise prevent cross-file references). A `.clangd` config force-includes `game_state.h` to suppress false diagnostics from standalone compilation. This powers the LSP tool:

- **`findReferences`** — find all callers/usages of a function or symbol
- **`goToDefinition`** — jump to a symbol's definition
- **`incomingCalls`** — call hierarchy (who calls this function?)
- **`documentSymbol`** — list all symbols in a file
- **`workspaceSymbol`** — search symbols by name across the workspace
- **`hover`** — get type info for a symbol

**Prefer LSP over Grep** for navigating code (finding callers, definitions, references). Use Grep for pattern/text searches that aren't symbol-based.

### ast-grep (`sg`)

**`ast-grep`** (`sg`) is installed for **structural code search** — matches AST patterns, not text. **Use `sg` instead of Grep** when searching for code structures (function calls, for-loops, if-blocks, assignments, etc.). Grep is better for string/comment/text searches.

Run via Bash with `sg run`:
```bash
# Find all calls to a function (use bare name, not $A — metavars don't match single args well)
sg run -p 'DeleteItem' -l c src/

# Find for-loops with a specific shape
sg run -p 'for(int $I = 0; $I < $N; $I++) { $$$ }' -l c src/entities/jobs.c

# Find if-blocks that return early
sg run -p 'if ($COND) { return $RET; }' -l c src/

# Find struct field access patterns
sg run -p 'items[$IDX].active' -l c src/

# Rewrite (dry-run: omit --update-all)
sg run -p 'OLD_FUNC($$$)' -r 'NEW_FUNC($$$)' -l c src/ --update-all
```

Key syntax: `$X` = single node metavar, `$$$` = multi-node (variadic), bare names = literal match.

**When to use which tool:**
| Task | Tool |
|---|---|
| Find callers/definition of a symbol | LSP (`findReferences`, `goToDefinition`) |
| Find code patterns (loops, if-blocks, call shapes) | `sg run -p '...' -l c src/` |
| Find text/strings/comments/regex | Grep |

If new `.c` files are added, regenerate:
```bash
make compile_commands.json
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

`nav/` (repo root) is the pathfinding **library** (A*, HPA*): no game code, all state in a
`NavWorld`. The game talks to it through `src/world/pathfinding.h` (one world via `NavGame()`,
legacy names like `entrances`/`use8Dir` are macros onto it) and answers its world queries in
`src/world/nav_host.h`. Never include game headers from `nav/` — `make test_nav_standalone` guards that.

~69,000 lines of C in src/, ~60,000 in tests/ (~129K total). Per-folder CLAUDE.md files in `src/entities/`, `src/world/`, `src/core/`, and `tests/` have detailed patterns and gotchas.

## Key Systems

Pathfinding (A*, HPA*), movers (agents with avoidance), jobs (designations → work), stockpiles (filtered storage), workshops (crafting), simulations (water/fire/smoke).

Explosions, structural support (unsupported walls fall), landslides, fuel vapour / firedamp
with flash fires and rooms blowing out, and the dithered effects layer: code map, scenarios and
what's next in `docs/todo/world/explosions.md` ("Where we are").

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

**Per-cell layers are accessed through macros**: `GRID(x, y, z)`, `WATER(x, y, z)`, `CELL_FLAGS(x, y, z)`, ...
(x, y, z order), never `grid[z][y][x]`. `make test` enforces it. Details in `src/world/CLAUDE.md`.

## Save/Load, Inspector & Debugging

```bash
# In-game
F5                    # Quick save (+ auto event log dump + state audit)
F6                    # Quick load
F7                    # Run state audit on demand (prints to console)
F8                    # Dump event log to navkit_events.log
TAB                   # Toggle dev UI vs play HUD

# CLI inspection
./build/bin/path --inspect save.bin              # Summary
./build/bin/path --inspect save.bin --audit       # Run 6 state consistency checks
./build/bin/path --inspect save.bin --mover 0    # Mover details
./build/bin/path --inspect save.bin --stuck       # Movers stuck > 2s
./build/bin/path --inspect save.bin --jobs-active
./build/bin/path --inspect save.bin --path 8,17,1 12,16,1 --algo hpa
```

### Debugging Workflow

1. Play → hit bug → F5 (saves + dumps event log + runs audit)
2. Hand `.bin.gz` + `navkit_events.log` to AI
3. AI runs `--inspect save.bin --audit` to find invariant violations
4. Event log shows timestamped trail of state-changing events

### Event Log (`core/event_log.c`)

Ring buffer (4096 entries), game-time timestamped. Records job lifecycle, item deletion, mover death/starvation, needs transitions, stockpile operations. Use `EventLog(fmt, ...)` to instrument new events.

### State Audit (`core/state_audit.c`)

6 invariant checks: item↔stockpile consistency, item reservations↔jobs, mover↔job bidirectional, blueprint reservations, stockpile slot reservations, freeSlotCount recompute. Returns violation count.

## Games

The games the player can start live in one table in `src/core/games.c` (`GameDef`: name,
label, description, `start()`, whether the dev overlay opens). Today: **survival**, **island**
(survival rules on a 256 island from `src/world/island_gen.c`, 3 settlers ashore by a river
mouth), **dollhouse**, **sandbox**. Started from the play HUD, the dev UI "Games" list, console `game <name>`, or
`--game <name>` (also `--headless --game <name> --ticks N`; `--game list` shows them).

- A game's `start()` builds its world and sets **every** rule toggle it relies on
  (`gameMode`, `hungerEnabled`, ...): it may not assume what the previous game left.
- `gameMode` (`GAME_MODE_SANDBOX` / `GAME_MODE_SURVIVAL`) is the rules switch for death and
  fog of war; dollhouse runs on `GAME_MODE_SANDBOX` rules.
- `GameDef.buildNow`: whether the dev build tools (Work bar: wall, floor, workshop, ...) work
  instantly or as orders the workers carry out (I flips it in game). Player toolbars always order.
- **The dev UI (TAB) is an overlay over any game**: toggling it does not change the game
  or its rules. Use it to inspect/cheat inside survival without leaving survival.
- Scenarios (below) are dev/test setups; games are player-facing.

## Settings (tunables)

Tunable values live in one table in `src/core/settings.c` (name, label, sidebar group, type,
range, help, optional live-tooltip function). One row gives you: a console variable
(`get`/`set`, `--set`, clamped to the range, `get` shows the help) and a row in the dev sidebar
(`DrawSettingsGroup("Simulation/Fire", x, &y)` in `ui_panels.c` draws a whole group).
**Add new tunables as a row there**, not as a hand-placed widget plus a separate
`Console_RegisterVar`. A row can have an `onChange(oldValue)` hook, run on both sidebar and
console changes (mover balance rows recalc derived rates; base speed rescales movers).
Types: bool, int, float, choice (int index into option names). Every dev sidebar section
draws its tunables from the table; spawn counts, editor tool/terrain pickers, buttons and
readouts stay hand-written. Saved settings are still a separate list
(`SETTINGS_TABLE` in `saveload.c`).

## Commands (dev / cheat commands)

Console commands live in one table in `src/ui/commands.c` (name, group, help, typed arguments
from `src/core/command_args.h`: int / float range, choice, item by name, name from another
table; plus a run function). One row gives you a console command with parsing, usage text and
error messages made from its spec. Dev sidebar buttons that do a command run the line through
the table (`CommandButton(x, y, "Clear Water", "clear water")`). Global keys (F5-F8, Space, Tab, , . F)
are rows in `src/core/global_keys.c` that run a command line: add a key there, not as an
`IsKeyPressed` block in input.c. **Add new commands as a row
there**, not with `Console_RegisterCmd` + strcmp parsing. Every command, tool, setting, verb, item, scenario
and game is also a row in the find index (`src/core/find_index.c`, filled by `src/ui/find.c`;
console `find <text>`, `find /<path>`; in game `/` opens the find palette, `src/ui/palette.c`): new
table rows show up there by themselves. Plan for the rest (tool options, verb
tables, keys as data, the find palette): `docs/todo/ui/find-palette.md`.

## Descriptions (what the game says about an entity)

`src/core/describe.c` builds an entity's text as lines with a semantic tone (good / warn /
bad / info / muted ...), no drawing: `DescribeMover(idx, DESC_PLAYER or DESC_DEBUG, &lines)`.
The hover tooltip maps tones to colors; `--headless ... --describe-mover N|all` prints the
same lines (after the run; `--ticks 0` = as loaded from `--load`). Need labels ("Peckish",
"Parched", ...) live there too (`HungerLabel` etc.). **New screens that show an entity should
call its describe function** rather than formatting fields again. Done: movers, animals, items, cells, water, blueprints, designations, stations, workshops, stockpiles.

## Social behaviour

Movers notice each other (`src/entities/notice.c`) and act on it: polite passing, talks and
gatherings (also round fires by themselves), walks, greetings, doorway manners, relations
(saved), mingling, and games (tag, hide and seek, follow the leader in `chase.c`). What's built,
settings, scenarios and next steps: `docs/todo/social/noticing.md`; code patterns:
`src/entities/CLAUDE.md` "Social behaviour".

## Selection and the inspector panel

Left-click in normal mode selects a mover, animal, workshop, stockpile or blueprint (`src/core/selection.c`);
`src/render/inspector.c` outlines it and shows a fixed panel bottom-right: the describe lines
plus buttons. Actions behind buttons live in `selection.c` (`ToggleMoverDraft`, `WorkshopAddBill`,
...) and the old hover keys call the same functions, so keys and buttons can't drift apart.
With a mover selected, **E** puts what fits the cell under the mouse in its action queue (eat
that, sit there, go there; Shift+E appends): `src/entities/action_queue.c`.
New per-entity actions go there too; object-level ones as a row in the `SelectionVerb` table
(name, label, key, run): the hover keys, the inspector's button row and the `?` help panel read it.

With the dev UI off, the play HUD's tab bar (`src/render/play_hud.c`, bottom left) has
Huisje (build) / Boompje (nature) / Beestje (movers + animals roster). A game's tools pick
their tab with `GameTool.tab` in `games.c`. Plan and what's left: `docs/todo/ui/game-structure.md`.

## Scenarios

Built-in demo/test setups live in one table in `src/core/scenarios.c` (`list` shows them all).

```bash
./build/bin/path --scenario list                          # names + descriptions
./build/bin/path --scenario ladders                       # GUI, starts in the scenario
./build/bin/path --headless --scenario ladders --ticks 3000   # no save needed; prints stats
./build/bin/path --headless --scenario ladders --ticks 3000 --set singleFileLadders 0
```

In-game: dev UI (TAB) → terrain section → **Scenarios** buttons (hover for description), or console `scenario <name>` / `scenario list`.
`--set <var> <value>` sets any console variable at startup (GUI or headless).
Load test per map size: `--headless --scenario world --set scenarioSize 512 --ticks 1200 --profile`
(`--profile` prints CPU ms per tick per profiler section; see `docs/todo/architecture/large-worlds.md`).

**Adding one:** write `static void SetupFoo(void)` in `scenarios.c` and add a row
`{"foo", "one-line description", "TerrainName" or NULL, SetupFoo, tickStats, printStats}`.
`RunScenario()` first calls `ScenarioResetWorld(terrain)` (clears entities/sims, generates the
named `terrainNames[]` entry or the flat 32x32 startup map, rebuilds spatial grids + HPA* graph,
restores settings earlier scenarios changed), then your setup. Setup only adds cells/movers/settings:
spawn movers with `SpawnMoverAt()` (names + traits included), call `BuildEntrances(); BuildGraph();`
again if it edits cells, and set every toggle it depends on. `tickStats`/`printStats` are optional
hooks for headless runs (ladders uses them for queue length).

## Headless Mode

```bash
./build/bin/path --headless --load save.bin.gz --ticks 100
./build/bin/path --headless --scenario <name> --ticks 100   # see Scenarios
```

`--events FILE` (any headless run) writes the event log to FILE after the run (what F8 does in game).

`--validate [N]` (any headless run) checks the sim activity counters against the grids every N
ticks (default 600; the windowed game does it every 60 s) and prints any drift. `--check` always
validates once at the end and fails on drift, so `make scenarios-check` covers it.

`--day-budget` (any headless run) prints where the movers' days go: game hours per mover per
day sleeping, working, walking to work, walking to needs (split by food/drink/toilet/bed),
eating, drinking, toilet, idle, plus meals/drinks/jobs per day (`src/core/day_budget.c`).
Use it before and after a balance change, e.g. `--scenario dayrhythm --ticks 50400 --day-budget`.

## Checking rendering changes

`--screenshot file.png` (GUI) keeps the sim paused, centres the camera for the current zoom,
saves the world (no UI) of the third frame, prints the world drawing time and the whole frame
time (no fps cap) averaged over the next 60 frames, plus the time per render profiler section,
and quits. Use a fixed `--seed` (the world scenario is random otherwise):

```bash
./build/bin/path --scenario world --seed 7 --set scenarioSize 2048 --exec "set viewZ 8" \
    --exec "set zoom 0.25" --exec "set forceDaylight 1" --screenshot before.png
```

`--look-at X,Y` centres the camera on a cell (also in the normal GUI), `--screenshot-frames N`
times N frames. Shader effects (setting `shaderEffects`, off by default) are an experiment in
`src/render/world_batch.c`: with them off the image must stay byte-identical.

A drawing change that should not change the picture must give byte-identical pixels for a
few views (zoomed in/out, day/night, high and low `viewZ`) against the previous build. Look at
the images too: a nearly black image hides differences (dark tints round many colours to the
same value). Lighting is computed once before the first frame; `viewZ 15` looks down on every
hill level, which exercises the deeper-level passes most.
