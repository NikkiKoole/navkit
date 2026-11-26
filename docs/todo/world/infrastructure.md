# Infrastructure: doors, locks, mechanisms, pipes (and what else is like them)

> Status: step 1 mostly done (2026-11-26): doors open / closed / locked for everyone, the
> open/close door chore (see "Where we are"). Otherwise a plan. Joins three older
> plans into one: keys (`../pathfinding/one-way-and-doors.md` "Parked: keys"), mechanisms layer 1
> (`../../vision/far future /mechanisms-and-signals.md`) and the pipe layer
> (`../gameplay/survival-toilet.md` "The pipe layer"). Those stay as the detail; this is the
> shared model and the order to build in.

## Where we are

**Step 1, doors open / closed (2026-11-26):** an open door is its own cell type, `CELL_DOOR_OPEN`
(solid, so the wall above keeps its support; lets water, smoke, steam, vapour, light and heat
through: insulation tier air). `CellIsDoor(c)` is true for both; every door check uses it, so rooms,
one-way doors, explosions and drawing treat open and closed alike. `SetDoorOpen(x, y, z, open)` in
`grid.c` flips it and wakes the sims and lighting at the cell; walking is the same either way, so
the pathfinder is not told. A mover whose next cell is a closed door stops for `doorOpenSeconds`
(setting, 0.5 s) and opens it (`mover.c`, before the move); the door stays open. Drawn with
`door-open-ns` / `door-open-ew` (named after the walls it hangs between). Console `door open|close
[x y]`, describe line "Open: smoke, water and cold get through". Scenario `doorsmoke` (in
scenarios-check): three people walk out of a smoke-filled room, no smoke out before the door
opens, smoke follows after, the door stays open. Tests: `test_doors` "door_states".
Side fix: movement now treats a one-way door as a wall from its wrong side (a push in a crowd
nudged someone back over the threshold in `door-oneway`).

**The open/close door chore (2026-11-26, user: opening on purpose is the same chore as closing):**
Work tool "dOors" (key O; play HUD Huisje "Open/Close Doors") marks each door for the other way
from how it is now: `DESIGNATION_CLOSE_DOOR` / `DESIGNATION_OPEN_DOOR` (`DesignateDoor` in
`designations.c`), one job `JOBTYPE_DOOR` (`RunJob_Door`, adjacent like gather tree; anyone does
it; work time `doorOpenSeconds`). The mark keeps the wanted state, so a passer-by opening the door
meanwhile doesn't flip it; already that way on arrival: the mark just clears. `doorsmoke` now
marks the door once everyone is out and checks one walks back and shuts it.

**Locked for everyone (2026-11-26):** a third door cell type, `CELL_DOOR_LOCKED` (`CF_WALL`: blocks
walking, fluids, light; `CellIsDoor` still true, so rooms keep it as a door). Walkability lists
only `CELL_DOOR` / `CELL_DOOR_OPEN`, so to the pathfinder a locked door is a wall.
`SetDoorLocked(x, y, z, locked)` (`grid.c`): locking shuts an open door, pushes anyone in the doorway
out, then `MarkChunkDirty` (a local HPA* rebuild, like placing a wall) and
`InvalidatePathsThroughCell` (paths through it are replanned); unlocking leaves it closed. A locked
door can't be opened (`SetDoorOpen` refuses, movers only open `CELL_DOOR`) or marked by the door
chore. Console `door lock|unlock`, describe "Locked: nobody gets through", drawn as the closed door
with a darker tint (until a `door-locked` sprite). Scenario `doorlock` (in scenarios-check): two
rooms, two doors, 16 wanderers; the north door locked at tick 600, unlocked at 1800; nobody in it
while locked, the south door takes the traffic, the north is used again after.

**Step 2 done (2026-11-26):** the galerij building and households on the ownership and access
core, and visiting (`visit.c`): knock, answer, a grant, stay, leave, the door closed or not;
scenarios `galerij` and `visiting` (`../architecture/ownership-and-access.md` "Where we are").

**Still open in step 1:** a small path cost for a
closed door (`NAV_ENTER_EXTRA`); the cold-snap and locked-door scenarios. Animals don't open doors
yet (they walk through closed ones).

## Why one plan

Doors with locks, levers and pressure plates, and water pipes look like three features, but they
are one kind of thing: **infrastructure with state that sits next to the cells**. Each has

- a **part that can switch**: a door open / closed / locked, a lever on / off, a valve open / shut;
- something that **switches it**: a mover by hand, a link from a lever or plate, a key, a sensor;
- **consequences in the sims we already have**: walking (nav), water, smoke and vapour, heat, light;
- often a **network**: a sparse per-cell layer, checked after each change, never per tick;
- **cost and visibility**: items plus a mover's labour to build, and drawn so you can see it work
  (the user's "see everything happening": visible pipes on bare ground, flow shown).

Built on one model they connect for free: a lever can shut a valve, a plate can open a
floodgate, a key and a lever both act on the same door, a closed door keeps gas in a room.

This fits `../../vision/vision.md` head on: closed loops, logistics, entropy ("pipes froze, water
damaged the stockpile"), self-sustaining systems you watch work.

## What exists

| Piece | Where | State |
|---|---|---|
| Doors (`CELL_DOOR`) | `grid.h`, `cell_defs.h` | walkable, block fluids, light and vapour; always "shut but passable"; rooms use them as room edges |
| One-way doors and ladders | `oneWayLayer`, `CanStepThroughDoors` | built (v110), console only, no build option yet |
| Sparse per-cell layers | `grid.h` (`SparseLayerRead/Write`), `LAYER_DEF` rows | built: empty chunks cost nothing; the pipe byte would be one more row |
| Water sim, tide, edge rivers | `water.c`, `tide.c` | built; what floodgates and intakes would act on |
| Vapour, firedamp, room pressure | `vapour.c` | built (2026-11-25); a door is a room's weak edge when it blows out |
| Heat, steam | `temperature.c`, `steam.c` | built; doors and pipes would carry or stop it |
| Belts, trains, elevator | `todo/world/conveyors.md`, `trains.md` | spikes; belts' line placement tool is meant for pipes too |
| Toilet ladder 0-3a | `../gameplay/survival-toilet.md` | built; `toiletsPlumbed` stands in for mains water |
| Pipe byte (generators) | dreamengine `docs/design/nk-plumbing.md`, `runtime/nkbuild/plumb.h` | built in dreamengine's building generators; navkit side not started |
| **Mechanisms sandbox** | `experiments/mechanisms/` (`make mechanisms`), `tests/test_mechanisms.c` | standalone toy, Feb 2026, ~3.4k lines + 1.2k tests: switches, buttons, wires, logic gates, latch, clock, processor; pipe, pump, valve, tank, pressure; dial, comparator, display; belts with loader / grabber / splitter / filter; shafts, clutch, flywheel, escapement, cam, hammer, governor. Not connected to the game |

## The shared model

### 1. Switchable parts

One small table of "things with a state" (doors, floodgates, valves, hatches, later lamps),
keyed by cell. Each state answers a few questions the sims already ask:

| Question | Asked by | Door open | Door closed | Door locked |
|---|---|---|---|---|
| can a mover pass? | nav, movers | yes | yes, after opening it (a small job; the door then stays open) | only with the key / not at all |
| do water, smoke, vapour pass? | water, smoke, vapour | yes | no | no |
| does heat pass? | temperature | yes | slowed | slowed |
| does light / sight pass? | lighting, noticing | yes | no | no |

**Doors don't close by themselves (user, 2026-11-26).** Opening and closing a door is a small
job. A mover whose path goes through a closed door stops, opens it (a short operate step) and
walks on; the door stays open until someone closes it. Closing is a chore of its own (a mover
closes it by hand, or a lever does). So doors left open let smoke, gas, cold and water through
until someone deals with it: entropy, and a reason to want a lever or a door-closing habit later.

**The key insight for pathfinding:** *closed* doesn't change walking (a closed door is passable
with a small extra cost for the opening step, nothing to rebuild); only the fluid, heat and light
sims care. Only *locked* changes walking. So opening and closing costs the nav nothing.

### 2. What switches a part

- **By hand**: a mover walks up and operates it (a step in the action queue, or a job).
- **Linked**: a lever or pressure plate flips its targets. A links table, run only when a source
  changes (the mechanisms doc's "Layer 1"): no cost per tick.
- **Sensors** (later, the same link): water level on a cell, weight, temperature, time of day.
- **Keys**: per-mover access to a locked part (see "Two kinds of locked").

### 3. Two kinds of locked

- **Locked for everyone** (a lever locked the gate, a drawbridge is up): to the nav it is a wall
  for now. **A door a lever holds shut is impassable for everyone, owners too (user, 2026-11-26).** Changes are rare and deliberate, so rebuild locally the way placing a wall does.
- **Locked unless you can open it** (keys, tickets, owners, people vs animals): per-mover access.
  The nav world is shared (one graph, regions, flow fields), so this can't be baked in. From the
  keys notes: locked doors become point entrances like ladders, checked per path query against
  what the mover carries; "can it get there at all" via regions with those doors as walls plus a
  small region-door graph. This is the one hard part of the whole plan; it comes last.

### 4. Networks (pipes first)

A network is a sparse byte layer (pipes: the agreed `pipeGrid`, bits floor / wall supply and
drain, stack, end, valve, valve closed) plus **a connectivity pass after every change** that
tells each fixture whether it is connected. Fixtures give their capability (flush, tap) only
when connected, and only in worlds that have pipes, so the dollhouse and city keep working
until they get real pipes. Physics as rules on top: drains fall, supply needs an intake higher
than the fixture. A valve is a switchable part inside a network, so a lever can drive it.

Ducts, shafts and wires (below) would be the same shape: one byte layer, one pass.

### 5. Built and seen

Every piece costs items and a mover's work (blueprints, like walls and belts). Everything draws
what it does: an open door stands open, a pulled lever leans over, supplied pipes are tinted,
water flecks run along a pipe when a toilet flushes.

**Links are seen in an overlay (user, 2026-11-26).** Not drawn all the time (a busy base would be
a web of lines), but there must always be a way to see them: a links overlay that shows every
link (source to targets), and the selected lever / plate / door showing its own links. The same
overlay idea serves hidden pipes (a pipe overlay) and later ducts, shafts and wires.

## Other systems of the same kind

Systems that would sit on the same model. Marked by how well they fit what's already there.

**Strong fits (lean on existing sims, small on top of the model):**

- **Floodgates and sluices.** A door for water only (movers walk over it). Dams, irrigation for
  Town's and survival's fields, a reservoir released on a lever, the DF flood trap. The water
  sim does all the work.
- **Ventilation: shafts, chimneys, flues.** Already the explosions doc's next step (firedamp
  needs a shaft to the surface). A chimney takes a hearth's smoke out of a room; a vent hatch is
  a switchable part. Smoke and vapour sims do the work.
- **Hatches, trapdoors, drawbridges.** A floor or bridge with a state: open = a hole (things fall,
  the support code already drops what loses ground), raised = no way across. "Locked for
  everyone" in nav terms.
- **Gates and fences for animals.** Pens that animals can't open and people can: the same
  per-agent access question as keys, with the cheapest possible rule (species, not items). A
  good first step toward keys.
- **Bells and alarms.** A bell (rung by hand, by a plate, by a fire) that movers *hear*: the
  noticing system makes them react (come to the hall, flee a fire, end the shift). A signal that
  goes through people instead of wires; fits the social work.

**Good fits, bigger:**

- **Mechanical power.** Windmill and water wheel, shafts, millstone, saw, pump. Town's mill and
  the vision's "nobody maintained the windmill, power stopped" want it; the sandbox already has
  shafts, clutch, flywheel. A network like pipes (shaft byte layer), with power instead of water.
- **Heating through pipes.** Hot water or steam from a boiler to radiators: pipes plus the
  temperature and steam sims. Frozen pipes in winter are the entropy side.
- **Rain gutters and cisterns.** The vision's own loop (rain, cistern, irrigation, crops). Roofs
  catch rain into a tank, a pipe takes it on. Needs rain to land somewhere countable.
- **Dirty water.** Already a user decision for pipes: a drain into the river dirties it
  downstream, boiling cleans it. A property of water, carried by pipes and the water sim.
- **Item chutes.** Gravity slides for items between levels: the drain rule (only downhill) for
  things instead of water; a cousin of belts.

**Later, from the sandbox:**

- **Signals and logic** (mechanisms doc Layer 2): wires, NOT / AND / OR, latch, clock. Airlocks,
  "open the floodgate when the reservoir is full", timers. Worth it once there are enough
  sources and targets to wire together.
- **Lamps and switches**, when there is something to power them.
- **The processor** stays a toy for now (Layer 3).

## Order to build

Each step is useful alone and ends in a scenario that `make scenarios-check` runs.

Where first: whichever lens is easiest to build it in (user doesn't mind). That is scenarios plus
the sandbox game: no needs, no rules to fit, everything can be placed from the dev tools.

1. **Door states.** Open / closed / locked-for-everyone on `CELL_DOOR`. A mover whose path goes
   through a closed door opens it (a small job, a short operate step) and leaves it open; closing
   is a job of its own (designate "close door", or a mover told to). Open lets water, smoke,
   vapour, heat and light through. Console `door open|close|lock`, describe line, a drawn open
   door. Scenarios: a smoky room whose door gets opened when people walk out (the smoke follows),
   a locked door that routes people round, doors left open in a cold snap.
2. **Front doors and visiting (the first showcase the user wants to see).** The apartment
   building (terrain `GalleryFlat`: flats off an outside gallery, stairs at both ends) with a
   household in each flat. Now and then someone visits a friend (relations already exist): walks
   to the friend's front door, knocks / rings / calls, and waits. The owner notices (noticing
   system), comes to the door, opens it, the visitor comes in; they talk, maybe sit and eat, then
   the guest leaves (and the door gets closed or not, by personality). Nobody home or nobody
   answering: the visitor waits a while and gives up. Needs:
   - **Household doors:** a front door belongs to a flat's household (lock id 1..999 in
     dreamengine's scheme, Town already has homes and bed owners). Locked to others; the owner
     can open it; once opened it is just an open door, anyone may pass.
   - **A cheap access shortcut for this step:** in a gallery flat every home is a dead end, so
     nobody needs a path *through* someone else's flat. A visitor's goal is the doorstep, not
     inside. So locked household doors can stay plain walls-to-strangers in nav for now (or
     passable with a check at the door), and the full per-mover path query (step 5) can wait.
   - **A visit activity** in the joint activities table (`activity.c`): knock, wait, answer, let
     in, host and guest together, leave. Knocking is a sound the owner notices like a bell.
   - Scenario `visiting`: the flat, ~8 households, friends among them; headless stats for visits
     made, answered, given up.
3. **Levers, plates, links, floodgates.** The links table (saved), a lever (a mover pulls it), a
   pressure plate (by weight: movers, items, water; the 1x1-stockpile trick to hold it down), a
   floodgate. Link by selecting the lever and clicking targets (the workshop-link pattern); the
   links overlay shows them all, a selected lever shows its own.
   Scenarios: lever opens a floodgate into a dry channel; plate opens a door; a flood trap.
4. **Pipes.** The toilet ladder's 3b and 4: `pipeGrid`, laying pipes with the line tool, the
   connectivity pass, intake / outlet / valve, visible pipes and flecks. A valve is a link target
   from step 3. Then dollhouse fixtures on real pipes.
5. **Access per mover (the rest of keys).** Animal gates first (rule by species), then keys as items (lock id on the
   item and the door, dreamengine's 0 / household / group scheme), owners, tickets (fare gate).
   The path-query work from "Two kinds of locked".
6. **Pick from the list above** by what the lenses need: ventilation for mining, mechanical power
   for Town's mill, bells for the social side, sensors and signals once there's enough to wire.

## Decisions (user, 2026-11-26)

- **Doors don't close by themselves.** Opening and closing is a small job; an opened door stays
  open until someone (or a lever) closes it.
- **Links are visible in an overlay**, not always: a way to see all links must exist, a selected
  source shows its own.
- **A lever-locked door is impassable** for everyone while it is shut.
- **The sandbox stays a reference.** `experiments/mechanisms/` is its own 32x32 toy world, so its
  code isn't ported; the game builds each piece on its real grid, water and movers, using the
  sandbox for what worked and felt good.
- **First lens: the easiest to build in** (scenarios + the sandbox game).
- **Some movers close doors behind them, some don't.** A personality difference (like Tidy /
  Slob for dirt): a door-closer shuts the door after passing, the others leave it open. Not
  needed for step 1 (closing by hand / lever first), but it is the intended behaviour.
- **Pressure plate weight: tune it once plates exist.** Start simple (movers and items count,
  one threshold) and adjust from play.
