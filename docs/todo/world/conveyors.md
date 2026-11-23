# Conveyor belts

Status: **spike** (2026-11-23). Items-only belts, one scenario. The design direction is still
open on purpose: the spike is there to find out which part is fun.

Related: `vision/logistics-influences.md` (haulers → belts/trains → elevators),
`world/trains.md` (open question 1: freight), `world/elevators.md`.

## What the spike does

Code: `src/entities/belts.c/.h`, drawn by `DrawBelts()` in `render/rendering.c`, ticked in
`SimStep` after trains. Scenario `belt` (48x24): two movers stock a log pile in the west,
a 49-cell belt snakes east to a second pile, a third mover stands by, a crate of berries rides along.

```bash
./build/bin/path --scenario belt                              # watch it
./build/bin/path --headless --scenario belt --ticks 12000     # all 40 logs arrive east
./build/bin/path --headless --scenario belt --ticks 3000 --set beltSpeed 4
```

Rules:
- **A belt is a cell with a direction (N/E/S/W) holding one item index**: a loose stack or a
  container. "Stack = slot": a stack of 10 logs takes one cell, a crate takes one cell and what
  is inside rides along (contents' positions catch up on each cell change, `MoveContainer`).
- Items move at `beltSpeed` cells/s (setting, dev sidebar "Belts", scaled by dayLength like
  trains) and only into a free cell. Belts are processed downstream first, so a full line
  moves as one; a blocked end backs the whole line up (backpressure, no spilling).
- **In**: an empty belt takes an unreserved ground item lying on its cell (drop a crate on it).
  The first belt of a line also takes a whole stack from the stockpile behind it: **the
  stockpile is the pick-up point** (any slot, the one right behind first).
- **Out**: past the last belt, a stockpile ahead takes the load into any slot that accepts it
  (the drop-off point; full or unwilling = the line waits). No stockpile: dropped on the ground
  ahead if no ground item lies there, else it waits.
- Items on a belt are `ITEM_ON_BELT`: not in the item spatial grid, so no job sees them;
  `IsItemAccessible` refuses items inside a crate that is on a belt. State audit stays clean
  with items riding (checked on a mid-run save).

Measured (seed 0): 3000 ticks: 37 loads on, 16 delivered, 21 riding (the movers fill A faster
than the line drains it). 12000 ticks: all 40 logs delivered. Haulers bring logs one at a time,
so they ride one per cell; the line would carry 10x as much with full stacks.

### Vertical belts (lifts and chutes)

`BELT_UP` / `BELT_DOWN` move the load to the cell above / below; the line goes on from whatever
belt is there, and a line can end going up or down (a stockpile above/below takes the load).
The load stays centred in the shaft. Drawn as a framed square with a triangle (up = pointing
north); belts on lower levels show when the cell is visible from above. No power and no speed
difference yet: a chute is just a line going down.

Scenario `belt-lift` (48x24): ground at z=1, a deck at z=3 (x >= 16) with two shaft holes.
Lift: logs the movers stock in A (ground) ride east, up two levels at (16,9), along the deck
into B. Chute: 8 rock stacks pre-stocked in C (deck) ride east, down at (34,15), into D on the
ground. 3000 ticks: 21 of 30 logs up, all 40 rocks down; 12000: all 30 logs up.
The deck has no way up for movers, so B and C get stockpile priority 1: otherwise haulers pick
B for a log and stand stuck below it (a hauler choosing an unreachable stockpile is an old gap,
not a belt one).

Found on the way: `ClearStockpileSlot` counted an emptied partial stack as a newly free slot
twice, and placing a load never updated `freeSlotCount`: the per-frame rebuild hid both, the
state audit caught them on mid-run saves. Both now adjust the count by the slot's before/after.

### Building belts in game

Dev UI (TAB) → Work → Build → **Belt** (`b`). Arrows/HJKL pick N/E/S/W, PageUp a lift, PageDown
a chute (the bar shows the direction). L-drag lays belt blueprints, R-drag cancels belt
blueprints and takes down built belts (the load drops, each belt gives back one plank).
Recipe `CONSTRUCTION_BELT`: 2 planks + 1 cordage, 0.5 game hours, on walkable ground, one belt
per cell. Blueprints draw as see-through belts pointing their way. The direction lives in
`Blueprint.workshopType` (unused by non-workshop blueprints, so no save change).

- A belt end with a blueprint ahead waits instead of dropping: an unfinished line doesn't
  spill onto its own building site (it did, and builders kept clearing it: an endless loop).
- Found on the way: two haulers could bring planks for the same blueprint slot; the second
  found the slot full and `DeliverMaterialToBlueprint` returned, leaving the planks CARRIED with
  no carrier (lost, and the blueprint waited forever). Now it drops them. The double reservation
  itself is fixed too: a slot's `reservedCount` counted haul *jobs* but `deliveredCount` counts
  *units*, so a stack of 5 for a slot of 2 left it looking half empty. It now reserves units
  (the job keeps them in `targetAdjX`, `JobBlueprintUnits`), and a job cancelled while carrying
  gives its units back (it kept them before: the slot waited forever).

Scenario `belt-build` (32x20): 12 belt blueprints, planks and cordage nearby, 3 movers. 3000
ticks: all 12 built, 20 of 40 rocks moved; 6000: all 40.
Play HUD (dev UI off): Huisje → **Build Belt**, shown once the world has planks. While it is
picked a **Dir** button next to it cycles N/E/S/W/lift/chute (arrows and PgUp/PgDn work too).
Not yet: deconstruction as a job (removal is instant).

### Belts and workshops (no new code)

Scenario `belt-workshop` (48x24): a belt carries logs from pile A into H, a 2x3 hopper
stockpile linked to a sawmill (Saw Planks forever); a second belt *starts on the sawmill's
output tile* and carries the planks to P. It needed no new belt rules: the hopper is an
ordinary stockpile (transfer point), and a belt already takes an unreserved ground item from
its own cell, which is what a finished craft is. 12000 ticks: all 30 logs sawn, 120 planks in
P. With `beltSpeed 0.01` P gets 0 planks: the belt carries them, not the haulers (the output
belt picks a new stack up before a hauler claims it).
Found on the way: *linking* a stockpile to a workshop is not enforced by the craft job (it
fetches from anywhere within the bill's `ingredientSearchRadius`), so the scenarios set that
radius to 5 cells (reaches the hopper, not pile A). Without it the crafter walked to A itself.

Scenario `belt-play` is the hand-built version: same pieces (logs in A, sawmill with an empty
hopper, empty plank pile P, a store with 60 planks + 30 cordage), no belts, starts in the play
HUD: Huisje -> Build Belt. The sawmill stays idle until a belt fills the hopper.
Not yet: a belt end pointing straight into a workshop (no hopper), recipes with two inputs
(one hopper per input works; a mixed hopper too).

### Not in the spike (known gaps)

- ~~Not saved~~ Saved since v98 (belts after furniture; `BeltsAfterLoad` rebuilds the lookup and
  puts items a pre-v98 save left `ITEM_ON_BELT` back on the ground). Checked: a mid-run save of
  `belt` loads and delivers all 40 logs, like an unbroken run; `belt-lift` and `belt-build` too.
  `beltSpeed` is not saved (the settings table has no per-row version).
- Belts don't care about walls, water or fire; movers walk over them and are not carried.
- Lifts and chutes are free (no power, no lift capacity), but not the same speed: a lift runs at
  `beltLiftFactor` (0.4) of the belt speed, a chute at `beltChuteFactor` (3x). In `belt-lift`, at
  1200 ticks 4 logs are up with a normal-speed lift vs 0 with factor 0.05.
- Belt ends find the stockpile slot by scanning it (fine at spike sizes).
- `ClearBelts` runs on scenario reset and on every game start (`StartGame`); a load replaces the belts.

## The design questions (from the brainstorm)

**What is the fantasy?** Not decided. The spike should tell:
- *Puzzle* (Factorio/shapez): you start wanting splitters, filters, ratios.
- *Colony efficiency*: you want movers to pack crates so the belt carries more.
- *World to watch*: you enjoy the flow; crates and people moving through a city, trains alongside.

**Loose items or boxes?** Spike answer: both, one slot each. Loose stacks work right away;
containers are the efficient upgrade without a rule saying so. Variants to try:
- *Size classes*: small things (berries, nails) only ride boxed; big things (logs, blocks)
  ride loose; barrels carry liquids.
- *Labelled boxes*: a crate gets a destination tag when packed; splitters route on the label,
  not the contents (postal service, baggage handling, City 1999).
- *Pallets*: a carrier for several boxes (forklift/train unit). Probably one nesting level too many.

**Containers as the unit of freight.** A train car is just a container of containers:
loading = put the crate in the car, `MoveContainer` moves everything. One standard box works for
belt, cart, train and mover (the shipping container idea).

**Dead end: stop or spill?** The spike stops (clear as a puzzle). Spilling (items tumble off the
end) is messier and more emergent. Could be per belt end, or a "chute" end that spills.

**Movers and belts.**
- Picking things off a moving belt: a sushi-belt kitchen in the dollhouse.
- Moving walkways (subway halls): needs **directional edge costs** in nav (cheap with the
  belt, dear against it). Check that A*/HPA* entrances can be asymmetric before promising it.
  If belts move movers or change walkability: `MarkChunkDirty`, and code writing `moverPaths`
  directly sets `moverRouteLen[i] = 0`.
- Animals riding belts: free comedy.

**Power and progression (fits the z-levels).**
1. Chutes / roller slides: no power, downhill only (mine shaft into a sorting room below).
2. Minecarts on rails: one car, pushed or animal-drawn, carries a crate (trains at their smallest).
3. Powered belts (water wheel, steam): flat or uphill.
4. Lifts / bucket elevators for going up; pneumatic tubes as the 1999 office/hospital extreme.

**Simulation crossovers**: wet belts slip, burning crates set the next one alight.

## Carriers: who builds what (agreed 2026-11-23)

|            | Items                                   | People                     |
|------------|-----------------------------------------|----------------------------|
| Horizontal | belts (this spike), train cargo (planned) | trains (built)           |
| Vertical   | vertical belts / hoist, chutes (done)   | elevator (`world/elevators.md`) |

- **One transfer rule for all item carriers**: a stockpile is the transfer point; loads go
  straight into a slot at the drop-off and haulers do the last mile
  (`pathfinding/transport-layer.md`, "Item Transport").
- **Vertical belts** (belts code): UP/DOWN directions, a load moves one level per step,
  stockpile transfer points on each floor. Chutes = down only, no power.
- **Train cargo** (trains code): a cargo stop = a stockpile within 2 cells of a platform; a train
  loads an item only if a stop further down the line has a higher-priority stockpile with room.
  The take/put code in `BeltTakeInput`/`BeltDropOut` then moves to a shared helper in `stockpiles.c`.
- **Elevator** (trains code): not stacked track. Changed 2026-11-23 (user's call): a
  *pathfinder link*, not a transit-graph line, so floors reachable only by elevator work for
  jobs and needs. A shaft is a column of `CELL_ELEVATOR` cells that the pathfinder links like a
  ladder; one car per shaft, doors, capacity (`src/entities/elevators.c`, scenario `elevator`).
  Elevator-to-train changes work through the walking cost to stations. A freight elevator
  (the car carrying crates) is still open.

## Next steps if the spike earns it

1. ~~Save/load belts~~ done (v98). ~~Clear them on game start~~ done.
2. ~~Build belts in game~~ done (dev UI and play HUD). Removal as a job.
3. ~~A belt feeding a workshop / taking from its output~~ works through a linked hopper
   stockpile and a belt on the output tile (scenario `belt-workshop`).
4. ~~Vertical belts + chutes~~ done (scenario `belt-lift`). Lifts slow / chutes fast done (settings).
   Next there: power (water wheel, steam) to speed lifts up.
5. Train cargo through the same transfer-point idea.
6. Only if routing becomes the fun part: splitters, filters, labels.
