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

### Not in the spike (known gaps)

- **Not saved.** Belts vanish on load, and items that were riding stay `ITEM_ON_BELT` where
  they were (invisible to jobs). Fix: save belts (version bump) or turn riding items back to
  ground on save.
- **No way to build belts in game**: only the scenario places them (`PlaceBelt`). Needs a
  blueprint/recipe (and materials).
- Belts don't care about walls, water or fire; movers walk over them and are not carried.
- One level only: no chutes, ramps or lifts.
- Belt ends find the stockpile slot by scanning it (fine at spike sizes).
- `ClearBelts` runs on scenario reset only, not on game start or load.

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

## Overlap with train cargo

navkit-59 (trains) plans **cargo platforms = a stockpile next to the track, marked as a stop**:
trains load from it and unload at the next stop whose stockpile filter accepts the item, and
haulers do the rest. The belt spike uses the same idea: a belt end's stockpile is its pick-up
or drop-off point. Keep it one concept ("a stockpile is a transfer point for a carrier") so a
belt can feed a cargo platform and a train can unload onto a belt's pick-up pile.

## Next steps if the spike earns it

1. Save/load belts; clear them on game start.
2. Build belts in game (recipe + blueprint), remove them (drops the load).
3. A belt end feeding a workshop input / taking from a workshop output.
4. Chutes (downhill, no power).
5. Train cargo through the same transfer-point idea.
6. Only if routing becomes the fun part: splitters, filters, labels.
