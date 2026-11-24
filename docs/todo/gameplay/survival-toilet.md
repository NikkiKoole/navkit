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

## What can be built now

- **Levels 0-2: yes, small.** One switch, one furniture type, toilet search by capability,
  comfort levels and moodlets, one craftable item and two construction recipes. No save bump
  except if the pit should fill up.
- **Level 3: medium.** The barrel and the fill job touch items/containers (coordinate with the
  City 1999 inventory work); the drain is the first pipe layer (save bump), a new blueprint kind
  and a placement tool.
- **Level 4: large-ish.** The supply network, intakes, the height check, the connected gate,
  and keeping every pipeless world working.

Suggested order: 0+1+2 as one step (playable: bush -> latrine -> toilet in the hut), then 3,
then 4 together with dreamengine's plumbing so the pipe model is built once.

## Open questions

- Should finding things (a toilet in a ruin, pipes in a dump) be part of survival? That needs
  found objects / ruins in world generation first; crafting covers the ladder without it.
- Pipe material per era: hollowed logs or bamboo, clay pipes from the kiln, iron later?
- Pollution: does a drain into the river make the water downstream undrinkable (boil it)?
