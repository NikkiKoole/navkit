# Survival toilet: from the bush to a flushing toilet

> Status: partial (2026-11-24). Levels 0-2 built (bush in survival, pit latrine, toilet) and
> 3a (bucket flush); 3b (cesspit + drain pipes) and 4 (supply pipes) are plans. Pipes share their model with dreamengine's
> plumbing proposal (`~/Projects/dreamengine/docs/design/nk-plumbing.md`): one `pipeGrid` layer,
> stacks as wall cells, a fixture only works when connected.

## The idea

You survive, build a hut, and want a toilet in it. The steps up are physics you can see, not
unlock rules ("physics, not rules", `architecture/capability-based-tasks.md`):

- **Waste has to go somewhere lower:** a pit under the seat, or a drain pipe falling to a
  cesspit or the river downstream.
- **Flushing needs water:** carried in by hand, or by a pipe from an intake *higher* than the
  toilet (gravity; the world already has z-levels and flowing water).

Every level is the same need (bladder) met better: more comfort, better mood, less dirt.
The tooltip always says why a toilet doesn't work ("intake is lower than the toilet", "drain has
no fall", "cistern empty"), so the answer to "why not?" is something on the map.

## Where we are (2026-11-24)

- Bladder need exists (dollhouse): a mover uses a free `FURNITURE_TOILET`, else a walkable
  outdoor cell next to a tree (`FindNearbyBush` in `needs.c`), else relieves in place when desperate.
- **Survival has the bladder need off** (`bladderEnabled = false` in the survival rules, `games.c`).
- A toilet can't be built in survival (no recipe, no construction entry), and survival has no
  ruins or loot, so "finding" one would need found objects first.
- What we can reuse: furniture placed by construction (the plank bed: item -> construction
  recipe in `construction.c`), workshops and the kiln (clay), water in clay pots, haul jobs,
  fixed containers (trash bins, `IF_FIXED`), belts built as a line of blueprints with a
  placement tool, floor dirt, mood moodlets, capabilities (`CAP_TOILET`).

## Built (levels 0-2)

- Survival and island have the bladder need on (`SetSurvivalRules`, `games.c`).
- `FURNITURE_PIT_LATRINE` (CAP_TOILET, comfort 1), `CONSTRUCTION_PIT_LATRINE` (4 sticks) and
  `CONSTRUCTION_TOILET` (3 planks or 3 clay) build furniture in place; play HUD "Build Pit
  Latrine" (once there are sticks) and "Build Toilet" (planks or clay).
- Toilets are found by `CAP_TOILET`, scored comfort / (1 + cells away) (`FindBestFreeToilet`,
  `needs.c`; `FurnitureDef.comfort`: 1 latrine, 2 toilet).
- Each use of a latrine adds `latrineDirt` (40) floor dirt to its cell: the filthy moodlet,
  cleaning and room quality follow from floor dirt. A toilet stays clean.
- A room with a latrine reads as a WC. Tests: `tests/test_toilet.c`.
- **3a, bucket flush:** after each use a toilet flushes if there is water within 3 cells: a tap
  or sink (CAP_WATER_SOURCE, plumbing for now: costs nothing) or a free water item (a pot kept
  full by the existing fill-water-pot job, one water used per flush). Unflushed it is a dry
  toilet: `toiletDryDirt` (15) per use. So: latrine 40, dry toilet 15, flushed 0. The
  dollhouse toilet flushes from its bathroom sink. Keep a stockpile with water pots next to a
  survival toilet and it stays clean.
- **Mains water is a game rule** (`toiletsPlumbed`): on in the dollhouse and sandbox (and city
  buildings: their WCs have their own cistern pipe, which the 3-cell stand-in missed in many of
  dreamengine's kinds), every toilet flushes; off in survival and island. pipeGrid's
  "connected" replaces it later.
- No new items, no save bump. Not yet: the pit filling up (needs a counter per furniture).

## The ladder

| level | what | needs | what's new in navkit |
|---|---|---|---|
| 0 | the bush | trees nearby | turn the bladder on in survival (+ balance) |
| 1 | pit latrine | dig a hole, sticks for a seat | a furniture type built in place; toilet search by capability; comfort by level |
| 2 | dry toilet | a toilet (crafted: clay at the kiln, or planks) over a pit | the toilet as a craftable item + construction recipe |
| 3 | bucket flush + cesspit | a water barrel by the toilet, a short drain to a dug cesspit | barrel as fixed water container, "fill the barrel" haul, flush uses a water item; drain pipe (first pipes) |
| 4 | piped flush toilet | an intake in the brook upstream and higher, a supply pipe down | supply pipes, intakes, the network check by height |

Levels 1-2 need no new systems. 3 brings the first pipes (drain only), 4 the second network.

## Level by level

### 0. The bush in survival

- Switch `bladderEnabled` on in the survival (and island) rules.
- Check the balance: how often, how far movers walk (`--day-budget` shows "toilet" and
  ">needs toilet"), the mood for relieving in place. No new code beyond the switch and tuning.

### 1. Pit latrine

- New furniture `FURNITURE_PIT_LATRINE`: `CAP_TOILET`, comfort level 1. Built in place from a
  construction recipe (dig the cell's floor, a few sticks), like other construction, no item.
- Bladder search by capability: the nearest free furniture with `CAP_TOILET`, preferring a
  higher level (like rest prefers `restRate`). Then any future toilet type works without code.
- Comfort per level on the def (a `comfort` or reuse a level byte): moodlets "used a pit",
  "used a proper toilet"; the bush and in-place stay as they are.
- Smell: some floor dirt around a latrine over time (floor dirt exists).
- Later, not now: the pit fills and a mover digs a new one (needs a per-furniture counter:
  a `Furniture` field, so a save bump).

### 2. Dry toilet

- `ITEM_TOILET` crafted (clay toilet at the kiln, or a plank box at the carpenter), placed
  over a pit by a construction recipe, like the plank bed. Comfort level 2.
- Rooms already read a room with a toilet as a WC.

### 3. Bucket flush and a cesspit (3a, the flush, is built: see above; 3b is the rest)

- A water barrel next to the toilet: a fixed container (like the trash bin) that holds water;
  a haul job keeps it filled from the brook (movers already carry water in pots).
- Flushing takes one water item from a barrel within reach; no water = it works as a dry toilet.
- The cesspit: a dug pit cell marked as an outlet.
- The drain: the first pipes. A `pipeGrid` layer (one byte per cell, save bump), pipe items,
  laid as a line of blueprints with the belt placement tool. Rule: along a drain each next cell
  is the same height or lower. Connected = the toilet's drain reaches an outlet (cesspit, or a
  river cell downstream).
- Comfort level 3 (flushed, no smell).

### 4. Piped flush toilet

- An intake: placed in a water cell of the brook. A supply pipe from it to the toilet.
- Gravity rule: the intake's water surface must be higher than the toilet. Then no barrel needed.
- A small network pass after any pipe change: for each fixture, is supply connected (reaches an
  intake higher up with water) and is the drain connected (reaches an outlet lower down).
  Same gate dreamengine's buildings use: a fixture gives its capability only when connected,
  **and only in worlds that have pipes** (dollhouse/city fixtures without pipes stay working).
- Later: frozen pipes in winter (temperature), a burst pipe as a water source in the water sim,
  river pollution downstream of an outlet.

## The pipe layer (`pipeGrid`, agreed with dreamengine)

> Part of the joint plan `../world/infrastructure.md` (pipes are its step 4; a valve is a
> switchable part a lever can drive).

One byte per cell, a grid layer of its own (`cellFlags` is full): one `LAYER_DEF` row, untouched
pages cost no memory, saves run-length code it (a save bump; older saves: no pipes). Pipes take
no cell of their own, so walking, rooms and the pathfinder don't change. Same layout as
dreamengine's proposed vocabulary (`docs/design/nk-plumbing.md` there), so a generated flat
and a survival hut run on one system.

| bit | meaning |
|---|---|
| 0 FLOOR_SUPPLY | a water pipe in this cell's floor |
| 1 FLOOR_DRAIN | a drain in this cell's floor |
| 2 WALL_SUPPLY | a water pipe in this wall cell |
| 3 WALL_DRAIN | a drain in this wall cell |
| 4 STACK | this wall cell's pipes go on to the STACK cells straight above and below (riser, soil stack) |
| 5 END | where the network meets the outside: with supply an intake (brook, barrel, water main), with drain an outlet (cesspit, sewer, river) |
| 6 VALVE | a stopcock on the branches leaving this cell (a stack runs on through it) |
| 7 VALVE_CLOSED | that stopcock is shut |

- **Floor bits** always sit on the cell standing on that floor (z 0 included: the crawl space).
- **Connecting** (like RimWorld conduits): pipes of one network join their 4 neighbours on the
  same level; STACK cells join up and down. A fixture is on a network if its own floor, or a
  wall cell next to it, carries that network's bit.
- **Physics on top, as rules, not bits:** a drain never runs uphill on its way to an outlet; a
  supply works if its intake is on water whose surface is higher than the fixture, or on a
  source piece (water main, barrel, tank). An intake with no water source is an error
  (dreamengine checks the same norm).
- **A network pass after every pipe change** (not per tick): for each fixture, is supply
  connected, is the drain connected. A fixture gives its capability (toilet flush, tap, sink)
  only when connected, and only in worlds that have pipes; until then `toiletsPlumbed` stands in.
- **Laying pipes:** pipe items (clay pipes from the kiln; hollow logs earlier?), laid as a line
  of blueprints a mover builds, with the belt placement tool; the drain's fall is checked while
  drawing. Pieces: intake, outlet / cesspit, valve.

## Seeing the water (open pipes)

Hidden or visible is drawing, not the layer, so pipes can show everything:

1. **Pipes on bare ground are visible.** A floor pipe in a cell without a constructed floor
   lies on the ground and is drawn; lay a plank or concrete floor over it and it is "in the
   floor", hidden. Likewise a wall pipe drawn along a wall you build, hidden in a generated
   wall. In survival (dirt) every pipe is out in the open by default; dreamengine's flats keep
   theirs in walls and screed, as real ones do. No extra bit: visibility follows from where it runs.
2. **Show what flows.** The network pass knows per pipe whether it is supplied and connected:
   draw supplied pipes blue-tinted, dry ones grey, mark a broken run or a closed valve. When a
   toilet flushes or a tap runs, small water flecks run along the pipe (the trick rivers use,
   `waterFlecks`), and waste goes down the drain to the pit. Drawing only, no extra simulation.
3. **A pipe overlay** shows hidden pipes too, for planning and debugging.

## Open channels (real water, no pipes)

The most visible version needs almost nothing new: dig a ditch from the brook to the hut and
the water sim fills it, and the water really flows. If an intake may sit on any water cell, a
dug channel *is* the supply line, no pipe items needed; an open ditch away from the hut can be
the drain to a pit. A good early rung before clay pipes:

- **Level 3.5, channel and basin:** a channel from the brook to a basin by the hut; a toilet
  (or tap) next to the basin flushes from it, like from a pot but endless while the brook runs.
  Needs: the flush (3a) counting a water cell within reach as a source, and digging a channel
  (digging exists; maybe a "dig channel" line tool). Check how the water sim fills a long
  dug channel, and that it doesn't flood the hut (a channel's water level follows the brook's).

## What can be built now

- **Levels 0-2: yes, small.** One switch, one furniture type, toilet search by capability,
  comfort levels and moodlets, one craftable item and two construction recipes. No save bump
  except if the pit should fill up.
- **Level 3a (built):** the flush from water within 3 cells; `toiletsPlumbed` for mains worlds.
- **Level 3.5, channel and basin: small.** A water cell as a flush source plus a channel tool.
- **Level 3b: medium.** The pipe layer (save bump), the network pass, pipe items, the blueprint
  line with the belt tool, the cesspit as outlet; visible pipes and flecks.
- **Level 4: large-ish.** The supply network, intakes, the height check, the connected gate,
  and keeping every pipeless world working.

Suggested order: 0+1+2 (done), 3a (done), then open channels (3.5), then the pipe layer with
drains and a cesspit (3b), then supply pipes (4), the pipe model shared with dreamengine's plumbing.

## Decisions (user, 2026-11-24)

- **Pipes are not free:** pipe items and a mover's time to lay them (blueprints, like belts).
- **Clean and dirty water, yes:** a drain into the river dirties the water downstream (see below).
- **The dollhouse gets real, visible pipes** once the layer exists (it moves off `toiletsPlumbed`).

### Clean and dirty water (notes)

- Today: drinking from natural water already gives `MOODLET_DRANK_DIRTY_WATER`; the hearth has
  "Boil Water"; the tap is clean water without end.
- Water cells: `WaterCell` is 16 bits with 1 spare, enough for a dirty/clean bit; levels of
  dirtiness (a little, filthy) need a separate sparse layer like `waterFlowLayer`.
- Rules to work out with the water sim (`world/water.md`): an outlet's drain water marks the
  cells it flows into as dirty; dirt moves with the water and fades with distance, dilution or
  time (a brook cleans itself downstream, a still pond doesn't); rain and springs are clean.
- Consequences: drinking dirty water (sickness later, a worse moodlet now), boiling cleans it,
  an intake on dirty water feeds dirty water to the hut. So: put the cesspit downstream of the
  intake, as real settlements learned to.
- The water sim belongs to whoever works on water: coordinate before touching `water.c`.

## Open questions

- Should finding things (a toilet in a ruin, pipes in a dump) be part of survival? That needs
  found objects / ruins in world generation first; crafting covers the ladder without it.
- Pipe material per era: hollowed logs or bamboo, clay pipes from the kiln, iron later?
- How dirty is dirty: one bit, or levels that fade? And sickness, or mood only at first?
