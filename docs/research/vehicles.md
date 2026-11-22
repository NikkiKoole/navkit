# Vehicles: broad research (2026-11-22)

> Status: research (no plan picked yet)

How other colony games do vehicles, what our world already has, and which vehicles fit a
stone age → village DF-like sim. Existing design docs this builds on:
`docs/todo/pathfinding/transport-layer.md` (shared transport pattern, "Layer 8" personal vehicles),
`docs/todo/world/trains.md`, `docs/vision/vision.md` (Exodus vehicles, CDDA-style),
`docs/vision/logistics-influences.md`.

---

## 1. What our world already has

| Hook | Where | Relevance |
|---|---|---|
| Trains: track, stations, boarding, walk-vs-ride decision | `src/entities/trains.c`, `MaybeTakeTrain` in `mover.c` | The only vehicle. Movers only, no freight, all trains share all track, bounce only |
| Waiting sets, multi-leg journeys, `TRANSPORT_*` states | `transport-layer.md` Layers 1-2 (done) | Any boarding vehicle reuses this |
| Weight slows carriers: `speed *= 1/(1 + w*0.02)` | `mover.c` effectiveSpeed block (~1819) | The natural place for "a cart ignores load weight" |
| Carrying drains energy faster (`carryingEnergyMult` 1.1x, keyed on `job->carryingItem`) | `MoverEnergyDrainPerGH` in `needs.c` | A cart/sled could lower or skip it |
| Job distance weighting (`jobDistanceWeight`) | `AssignJobs_DistanceBands` / `JobInRange` in `jobs.c` | Faster travel changes how far "far" is |
| Seek timeouts allow 3x the straight walk at the mover's speed | `StartSeekClock` in `needs.c` | Must use vehicle speed, or riders give up early / wait too long |
| Containers: basket 15, chest 20, clay pot 5 | `containers.c` | Already "carry many items in one trip"; a cart is a big container you push |
| Terrain cost: base 10; water 12/17/29, mud 17, snow 12-17, bush 20, furniture | `GetCellMoveCost` in `cell_defs.h` | Nothing is cheaper than 10, so **roads can't be faster yet** |
| One cost for all movers in A*/HPA* | pathfinder + `nav_host.h` | A cart that hates mud more than a walker needs its own costs (the biggest structural question) |
| Single-lane yielding (tunnels), single-file ladders | `MoverShouldYield`, `ladders.c` | Passing rules for carts on narrow paths |
| Walkable deep ice (new, f04d21a) | `cell_defs.h`, `water.c` | Sleds on ice, and boats in summer on the same spot |
| Animals: grazers + predators, no taming | `animals.c` | Pack animals need taming/ownership first |
| Materials: logs, planks, poles, sticks, cordage, bark | `items.h` | Enough for travois, sledge, raft, wheelbarrow (needs a wheel) |

## 2. How other games do it (short)

Full notes with sources in §6. The main points:

- **DF wheelbarrow**: a tool assigned to a stockpile (max 3), used for heavy items. The pusher moves at **full speed whatever the load**. Pain: the per-stockpile cap quietly limits throughput, and items stay stuck in the wheelbarrow after an interrupted job.
- **DF minecart**: 5x a wheelbarrow's load, carved or built track, **hauling routes = ordered stops with departure conditions** (timer, load at least/at most), track stops can dump the load. Carts hit dwarves, and z-changes only via ramps that fail silently.
- **DF wagon**: 3x3 footprint, needs a 3-wide path from the map edge, with no indicator; a classic complaint.
- **RimWorld caravans**: capacity = body size x 35 kg, speed falls with mass, roads halve terrain cost, -17°C hurts. Pain: **loading** takes hours and colonists ignore needs while doing it.
- **Manor Lords**: villagers **can't carry logs**, only an ox at a hitching post can. Hand carts are free, horses pull trade carts. Pain: work stalls when there's no ox and the game doesn't show why.
- **Settlers II**: one carrier per road segment, plus a donkey when traffic is busy. Shortest-path routing jams main roads.
- **Factorio**: rail blocks + signals; fixed schedules, no free-roaming AI.
- **CDDA**: vehicles built tile by tile; multi-tile pathing with rotation is called hard by the devs, and big vehicles cost a lot of CPU.
- **Valheim**: cart (18 slots, harder to drag when full), boats you can roll a cart onto.
- **Kenshi**: pack animals are walking backpacks.
- **Ostriv**: carts/wheelbarrows are crafted items that wear out and need repair.
- **Banished, Foundation, Going Medieval, Timberborn**: no goods vehicles. Going Medieval and Medieval Dynasty players keep asking for them.

### Patterns
1. **Two models.** (a) the vehicle is a **carry modifier on the agent** (DF wheelbarrow, Valheim, Manor Lords hand cart): cheap, 1-cell footprint, normal pathing. (b) the vehicle is **its own entity** (minecarts, trains, CDDA, RimWorld vehicle mod): its own pathing, collisions, and most of the bugs.
2. **Fixed routes beat free AI.** Stop conditions (DF), schedules (Factorio), segments (Settlers). Automation comes from rules at stops, not from planning.
3. **Footprints bigger than one cell hurt everywhere.**
4. **Loading/unloading is where the pain is**, not driving.
5. **"Ignore weight while pushing" is a cheap, readable rule.**
6. **Hard requirements make good "aha" moments** ("logs need a sledge/ox"), but only if the UI says why work is stalled.
7. **Capacity caps turn into hidden throughput limits.**

## 3. Fit with our world (ranked)

| # | Vehicle | Era | Model | What it needs from us | Why |
|---|---|---|---|---|---|
| 1 | **Sledge / travois** (poles + cordage) | Stone age | carry modifier | Tool item a hauler takes along; in the weight formula, ignore weight or multiply capacity; bonus on snow/ice/mud | Earliest fit, no pathing change, makes snow + new ice matter. Could gate logs/big stones (Manor Lords "aha") |
| 2 | **Wheelbarrow / handcart** | Village (needs a wheel) | carry modifier = pushable container | A container you hold while walking (basket/chest code + held tool); penalty on bush/mud/stairs, no ladders | Fits our hauler/stockpile code almost for free. Avoid DF's per-stockpile cap |
| 3 | **Freight on trains / minecart with stops** | Village+ | entity, fixed route | Trains carry items; DF-style stop conditions; two haul jobs with a ride in between (`transport-layer.md` "Item Transport") | Builds on what exists; open question #1 in `trains.md` |
| 4 | **Pack animal** (follows a mover) | Village | follower | Taming, ownership, animal follows its leader, carries a container | `transport-layer.md` already calls this the simple first step for personal vehicles |
| 5 | **Raft / dugout canoe** | Stone age → village | entity on water | Cells with water ≥ N are its "walkable"; ride like a train with free stops, or float logs downstream | Uses the water sim; rivers become routes; ice in winter flips raft ↔ sled |
| 6 | **Ox cart on roads** | Late village | entity (1-cell lead, cosmetic trailer) | Roads cheaper than 10, own cost table, parking | Real "commute" vehicle; fake the footprint to avoid DF's 3x3 wagon trouble |
| 7 | **Caravans / exodus wagon** | Endgame | mostly off-map | Edge-of-map access check that the player can see | `vision.md` Exodus; keep it out of the local pathing |

## 4. Structural questions (decide before building anything past #1-#2)

1. **Per-agent move costs.** Today A* and HPA* share one `GetCellMoveCost`. Options:
   (a) same graph, the vehicle only changes speed at runtime (paths are a bit wrong but cheap);
   (b) a cost multiplier per "move class" in A* only, HPA* keeps walker costs;
   (c) a separate HPA* graph per move class (memory/time on big maps, see `chunked-world-plan.md`).
   Carry modifiers (#1, #2) work with (a).
2. **Roads.** Base cost 10 is the minimum, so a road needs the scale moved (e.g. base 20, road 10) or a speed bonus outside the cost. A scale change touches every cost constant and the heuristics.
3. **Footprint stays one cell.** Every bigger-footprint example in other games was a source of pain.
4. **Loading.** Make it a short, normal haul job that keeps needs active, so we don't repeat RimWorld's caravan loading.
5. **Where vehicles may go:** carts on ladders? ramps only? stairs? (DF: ramps only.)
6. **Visibility.** If something needs a vehicle, the UI must say why work is stalled.

## 5. Suggested first experiment

A **sledge** as a tool item:
- A hauler picks it up for heavy loads (or any multi-item haul).
- While pulling it, the weight slowdown is ignored and capacity rises (for example, one sledge = one basket's worth plus logs).
- It is faster on snow and ice and slower on bare rock.
- No pathfinder changes.

This tests the "carry modifier" model, fits the stone age, and is measurable with a scenario (hauling time for N logs over distance D, with and without a sledge).

## 6. Pushables: sledge = wheelbarrow = shopping cart

A sledge, a wheelbarrow and a City 1999 shopping cart are one object: **a container
you move by holding on to it**. Build it once and it serves both games (colony and
`docs/vision/city-1999.md`).

### One mechanic, different numbers

| | Sledge | Wheelbarrow | Shopping cart |
|---|---|---|---|
| Made from | poles + cordage | planks + a wheel | found or stolen (coin lock) |
| Capacity | big | medium | big |
| Good on | snow, ice, mud, grass | packed ground, floors | smooth floor, pavement |
| Bad on | bare rock, floors | mud, bush | everything else (wheels jam) |
| Stairs and ladders | no | no | no |
| Belongs to | whoever made it | the colony | the supermarket |

This becomes a small `PushableDef` table next to `containerDefs`: capacity, a speed factor
per ground type, and whether it ignores load weight.

### Two states

- **Held:** it moves with the mover, sets their speed by what's under it, and the mover
  avoids ladders and stairs.
- **Let go:** it's just a container on the ground. Anyone standing next to it can reach in,
  and anyone can take it. In the city, that's where ownership and theft come from. In the
  colony, it's a mobile stockpile you can leave at the mine.

### What already exists

- Containers in containers, carried containers with their total weight
  (`GetContainerTotalWeight` in the `mover.c` speed block), save/load.
- `IsItemAccessible()` already keeps other movers out of a carried container.

### What's missing (the same list for both games)

1. **A "holding" state on the mover:** the object follows them and its type sets the speed.
   This replaces the weight slowdown (and `carryingEnergyMult`) while held.
2. **Per-mover pathing restriction:** "avoid ladders and stairs." It's the same per-mover
   question as keys and locked doors in City 1999, so one answer serves both. Speed by ground
   type is runtime speed only (option (a) in §4), with no separate path costs.
3. **A trip job that loads several items** (colony only). Haul jobs today move one item or one
   whole container. A wheelbarrow only pays off with "take the cart, load N stones at the mine,
   push to the stockpile, unload." In the city the player does this by hand.

### Order (if built)

1. The holding state.
2. The shopping cart in a city-block scenario. The player drives it by hand, so there's no job AI.
3. The sledge in the colony, with the trip job.

The city version proves the mechanic cheaply; the colony version adds the automation.

Cars (City 1999, Cataclysm style) are parked for now.

## 7. Sources

- DF: https://wikiwiki.jp/dwarffort/Minecart, https://dwarffortresswiki.org/index.php/DF2014:Wheelbarrow, https://dwarffortresswiki.org/index.php/Wagon, https://dwarffortresswiki.org/index.php/DF2014:Trade_depot, https://dwarffortresswiki.org/Roller
- RimWorld: https://rimworldwiki.com/wiki/Caravan, https://rimworldwiki.com/wiki/Pack_animal, https://rimworldwiki.com/wiki/Transport_pod, https://steamcommunity.com/workshop/filedetails/discussions/3014906877
- Manor Lords: https://wiki.hoodedhorse.com/Manor_Lords/Hitching_post/en, https://pcgamer.com/games/city-builder/manor-lords-sawpit-not-working
- Farthest Frontier: https://www.gamepressure.com/newsroom/farthest-frontier-how-do-wagon-shops-work/z64ed9
- Settlers II: https://settlers2.net/guides/roads/
- Factorio: https://wiki.factorio.com/Tutorial:Train_signals
- CDDA: https://srgnis.github.io/cdda-wiki/cdda_wiki/Vehicle_construction, https://discourse.cataclysmdda.org/t/vehicle-size-shape-and-auto-drive-experience/25345
- Prison Architect: https://prison-architect.fandom.com/wiki/Deliveries
- Valheim: https://gamerant.com/valheim-craft-use-cart/
- Kenshi: https://kenshi.wiki/w/Pack_Beast
- Timberborn: https://timberborn.org/wiki/buildings/zipline-station
- Ostriv: https://store.steampowered.com/app/773790/Ostriv/
- Foundation: https://www.pcgamer.com/you-dont-have-to-place-roads-in-this-medieval-city-builder-thanks-to-your-citizens-feet
