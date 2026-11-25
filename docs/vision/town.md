# Town: a city builder where every citizen is a person

> Status: steps 1-4b on master (2026-11-25); step 6 (the grain chain) on branch `town`. Next: overview / lot inspector, winter stores. Game `town`.

You draw roads and lots; houses and workplaces appear on them at once. People move into the
houses, sleep in their own bed, eat and drink at home, and walk to work while their workplace is
open. Every citizen is a mover with the same needs as in the dollhouse: the town works (or
doesn't) because of what you built, which you can see in the people living there.

`vision.md` says "Not a city builder (too small, too personal)" and lists "top-down god view, no
individual agents" as what to avoid in SimCity. Town keeps the personal part: no zone figures,
only people. SimCity's growth and money come later, and only as far as they come out of people.

## User decisions (2026-11-25)

- **Build instantly** (SimCity style), not orders carried out by workers.
- **Every citizen is a mover.** No crowds shown as numbers.
- Milestone 1 is the "Town" below. Leave the economy and services until that feels fun.
- **Food at first: fridges the town restocks** (stand-in, step 3).
- **Work makes what the town needs** (step 4), food first: a workplace's workers produce it and
  it reaches the homes' fridges, replacing the restock. Not desk-sitting for its own sake.

Earlier ones that apply here:
- Rhythm comes from world facts, not clock rules ([schedule decision](../todo/schedule-system.md)):
  a workplace has **opening hours** (a fact about the place); nobody gets a per-mover timetable.
- City 1999 shares the city: money, ownership, keys and shops are designed there
  ([city-1999.md](city-1999.md)). Town takes the smallest piece it needs (a mover's home) and
  leaves the rest to that doc.

## What Town reuses

| Need | Existing |
|---|---|
| A game mode | a row in `src/core/games.c` (`buildNow` = true), play HUD tab for its tools |
| Instant buildings | the dollhouse flat is built in code from walls, doors, windows, furniture: the same calls stamp a house template |
| People | movers, needs (hunger, thirst, energy, bladder), mood, talks, gatherings, day rhythm |
| Homes as rooms | room detection, types, quality |
| Traffic later | `cars-experiment` branch: road network with lanes, lights, slot traffic, movers driving their own car |

## Milestone 1: Town (steps, each testable on its own)

1. **(built) Game + roads.** Game `town`: flat 128x128 (later bigger), instant build, needs on, nobody
   dies (sandbox rules like the dollhouse). A road tool drags a road (a paved strip, 3 wide) on
   the ground. A road touching the map edge is where newcomers arrive.
2. **(built) Lots and buildings.** A lot tool drags a rectangle next to a road, kind Home or Work. A
   building from a template that fits the lot appears at once, its door facing the road.
   Templates: small house (1 bed), house (double bed), workshop / office. Deleting a lot
   removes its building.
3. **(built) People move in.** A home with a free bed draws a newcomer: spawned at a road on the map
   edge, walks to the house, it is now their home (`Mover.home`, a lot id). They sleep in their
   own home's bed; the beds of other homes are not theirs. Food: each home has a tap (mains
   water) and a fridge the town restocks (a stand-in for shops, which come with City 1999's coins).
4. **(built: the bakery) Work.** A Work lot has opening hours and a number of jobs. A citizen without a job takes
   one. While the workplace is open and no need is urgent, they go there and work at a work
   spot; otherwise free time. Rush hour should come out of this by itself.
5. **Look and feel.** A town overview: population, homeless newcomers waiting, empty jobs,
   who lives / works where (inspector on a lot). Then play it.

After milestone 1 (not planned in detail): shops and coins (with City 1999), growth (lots that
build themselves when people want homes / jobs), cars on the drawn roads (road network from the
cars branch, needs it merged first), services (water, power), bigger maps (2048 = ~2 km town).

### Built (2026-11-25): steps 1-2

- `src/entities/town.c/.h`: roads are a list of rectangles (paving = the ground block below
  turns to smooth concrete, grass off); lots are rectangles of 5x5 to 24x24 that must lie along
  a road. The side with the most road along it is the front: a yard row there, brick walls
  (concrete for work) round the rest, a door in the middle of the front wall, a paved cell from
  the door to the road, windows, a slate roof one level up. Homes: beds along the back wall then
  up the sides (area / 6, 1-6), a tap and a fridge either side of the door, a toilet, a table
  and chair when there's room. Work: desks along the back wall with a chair in front (the work
  spots), a tap, a toilet. The aisle from the door to the back wall stays free.
- Game `town`: flat 128x128, a main street in from the west edge, instant build, needs on,
  sandbox rules. Play HUD (Huisje tab): Road, Home, Work (left-drag place, right-drag remove);
  dev Draw bar: City (c) -> Road / Home / Work.
- Console `town road|home|work|remove x1 y1 x2 y2`, `town list` (viewed level).
- Tests: `tests/test_town.c` (paving, refusals, door facing the road on every side, beds
  reachable from the road, removal restores the ground).
- Saved since v113 (see "Save" below).

### Built (2026-11-25): step 3

- Every bed in a home is private: unowned beds are kept for newcomers (`TOWN_OWNER_NOBODY`), an
  owned bed is its owner's. The bed search in `needs.c` asks `TownMayUseFurniture`; everything
  outside town homes stays public. Kept in town.c (`townFurnOwner[]`, `townHome[]`), no Mover
  or Furniture struct change: City 1999's `owner` field can take it over later.
- `TownTick` (sim_step.c, every 0.1 game hours): owners who are gone free their bed, homeless
  movers in town take a free bed, and while a bed is free a newcomer arrives every
  `townArrivalGH` (setting, 0.5 h) on a road cell at the map edge, gets the bed and walks home
  (action queue "go there" to just inside the door). Newcomers come from main.c's
  `SpawnMoverAt` through the `townSpawnMover` hook (names, looks; tests get plain movers).
- Food stand-in: each home's fridge gets a bread stack of `townMealsPerResident` (setting, 3)
  per resident, topped up when half is gone. Movers eat the nearest food, usually their own
  fridge's, but a neighbour's is not off limits yet.
- The describe lines say "Lives in home #N" or "Homeless". Setting `townStarter` (0-12): homes
  already on the main street at the start. `scenarios-check` runs `--set townStarter 6 --game town`.
- Measured, 6 starter homes (12 beds), arrivals every 0.1 h: 12 residents, nobody stuck, town
  tick ~0 (whole game 0.02 ms/tick); `--day-budget` over 5 days: sleep 6.7 h, ~0.8 meals and 4.3
  drinks a day each.

### Built (2026-11-25): step 4, the bakery (user: "bakery first", the grain chain later)

- A Work lot is a **bakery** (play HUD "Bakery", dev Draw > City > Bakery). Stoves and counters
  along the back wall: the existing stove recipe "Bake Bread" (1 flour -> 1 bread, needs a prep
  surface within 2), one bill per stove, `BILL_DO_FOREVER`: it pauses by itself when no
  stockpile has room (`suspendedNoStorage`). Work spots = stoves.
- **Flour comes from outside town** (stand-in for a mill and farms): a stack of
  `townFlourDelivery` (setting, 20) on the delivery spot left of the door, topped up when half
  is gone or taken. The grain chain (farm -> quern/mill -> bakery) replaces this later.
- **Each home's fridge is a one-cell stockpile that takes only bread.** The existing haul jobs
  carry bread from the stoves to the homes. The bread restock of step 3 is gone: no bakery,
  no food.
- Nothing new in the job system: any idle mover bakes and hauls. Who works where and when
  (jobs, opening hours) is step 4b.
- Measured, 6 starter homes + the starter bakery (`townStarter` > 0 adds one), 12 residents,
  5 days: every pantry full (10 bread, a full stack), flour waiting at the stoves, hunger
  40-55% for everyone, audit 0 violations. Day budget per resident: work 5.2 h, walking with a
  job 4.1 h, sleep 7.6 h, idle 2.2 h (was 9.6 h idle with the restock). Meals 0.2/day: a bread
  is a big portion (1.0 nutrition).
- Known: a baker carries the whole flour stack (20) to their stove; the delivery then puts a new
  stack down. Flour in town stays bounded (one stack per stove + one at the spot), but it is
  generous.

### Built (2026-11-25): step 4b, jobs and opening hours

- Each bakery has a job per stove. Every 0.1 h a resident (with a home) without a job takes one,
  at the bakery with the most free jobs (`townJob[]`, not saved: handed out again after a load).
  Closing a bakery or a worker leaving frees the job.
- Opening hours are a fact about the place (settings `townOpenHour` 7, `townCloseHour` 17;
  before-the-opening-hour = open through midnight). `TownMayCraftAt` (jobs.c `WorkGiver_Craft`):
  a town bakery's stoves take only its own workers, only while open. Other workshops: anyone.
  The crafting phase tries every idle mover in a town (normally only as many as there are
  workshops waiting: those first few may not be the bakery's workers).
- Hauling stays open to everyone: whoever is idle carries bread home.
- **Found by measuring: one loaf per 2 game hours (the stove's "Bake Bread") can't feed a town.**
  A person eats about a loaf a day (hunger full to empty in 24 h, a loaf = 1.0). 4 bakers on
  7-17 made ~11 loaves a day for 12 people: everyone starved. New stove recipe **"Bake Loaves"**
  (4 flour -> 4 bread, 2 h, appended to `stoveRecipes`: indices unchanged); bakeries use it.
  One baker on a 10-hour day now feeds ~20. It is also on every stove (survival included).
- Newcomers bring `townGroceries` (setting, 2) loaves for their pantry: a new town doesn't
  starve before the first bake.
- Measured, 6 starter homes + bakery, 12 residents, 5 days, open 7-17: everyone Peckish or Full,
  4 bakers (~5.7 h baking a day each), 8 without a job; open 0-24: the same hunger, more baking.
  Describe: "Works at bakery #N (open/closed, 7-17 h)" / "No job".

### Built (2026-11-25): step 6, the grain chain (user: "lets do the grain chain")

farm -> mill -> bakery -> pantries, everything carried by the existing haul jobs.

- **Farm** lot (play HUD Boompje tab, dev Draw > City > Farm): no building. The road-side row is
  a seed store (corner) and a **barn** (wheat store); every other cell is marked for wheat with
  the existing farming designations (till, plant, tend, harvest: anyone idle does them). One
  seed per field cell to start with; each harvest gives 4 wheat + 1 seed back.
- **Mill** lot: querns along the back wall ("Grind Wheat", 2 wheat -> 1 flour, 1 h), a job per
  quern, opening hours like the bakery, a granary left of the door.
- **Bakery**: a flour store left of its delivery spot.
- **Bills run until there is enough** (the existing "Do Until X", `docs/done/jobs/bill-modes.md`;
  it counts every stockpile on the map): bakeries until the pantries hold `townBreadPerHome`
  (8) loaves per home, mills until `townFlourTarget` (40) flour is in store. Before this they ran
  forever: 137 bread piled up while the mill kept importing wheat.
- **Emergency imports**: when the whole town has less than a batch of flour (4) or wheat (2),
  a bakery / mill gets `townFlourDelivery` / `townWheatDelivery` (20) on its delivery spot,
  counted in `townImported[]` (not saved). Without them a town that runs dry locks up for good:
  starving haulers drop their jobs to look for food, so nothing reaches the bakery (seen: 12
  starving, 97 flour lying at the mill). First version ("until the upstream produces") cut
  the flour as soon as the mill had ground once and caused exactly that; second ("when the
  lot's own store is empty") imported 4180 wheat a year while 626 lay in town.
- **Climate**: the town is temperate (12 +- 10 C, day/night +-4): the default 15 +- 25 C gives
  40 C summers where wheat grows at 0.3x. Saved and restored with the scenario settings, so it
  doesn't leak into the next game (`ScenarioSettings` got the climate + `weatherEnabled`).
- **No weather in the town**: rain on a flat map with no drainage filled the whole town with
  water (7/7 everywhere after a week; people waded and could not reach their beds or food).
- **Irrigated farms** (setting `townIrrigation`, on): town farm cells are kept wet, a stand-in
  for a well and watering jobs (watering needs water items; the town has only taps). Dry soil
  grows wheat at 0.3x.
- **Why the barn**: a quern needs one stack of 2 wheat and a harvest drops single wheat, so
  only stored wheat can be ground. With only the mill's 4-cell granary (40 wheat, stacks of 10)
  most of the harvest lay loose on the field and the mill imported flour all year.
- **Harvests drop one stack** (jobs.c `RunJob_HarvestCrop`, all games): 4 single wheat per
  cell made 188 items a round, one haul trip each; the haulers fell behind and 272 wheat lay on
  the field while the mill imported flour.
- **A bread shelf in the bakery** (priority 3; pantries 7, so bread moves on home): bread used to
  wait by the stoves for haulers busy with the harvest and a third of it rotted; rotten bread
  isn't counted by "until N" nor taken by pantries, so the bakers kept baking (185 bread).
- **Measured, starter town (12 people), one year (28 days), `--set townStarter 6`:**

  | day | hunger (12 people) | wheat in town | flour imported (total) | wheat imported (total) |
  |---|---|---|---|---|
  | 7 | 8 Full, 4 Peckish | 150 | 60 | 160 (start-up) |
  | 14 | 10 Full, 1 Peckish, 1 Hungry | 258 | 100 | 160 |
  | 21 | 6 Full, 3 Peckish, 3 Hungry | 116 | 200 | 160 |
  | 28 (end of winter) | 6 Full, 4 Peckish, 2 Hungry | 20 | 280 | 640 |

  Bread stays at its target (40-48). Open: **winter** (no harvest for 7 days: 480 wheat
  imported; a 12-person town needs ~170 wheat in store for it, and the barn + granary hold 110)
  and **the mill** (still ~10 flour a day imported while wheat waits; 4 querns should grind 40).
- Headless runs print a `Town:` line (people, homeless, jobless, stocks, imports, harvests);
  console `town list` too. New option `--dump-events FILE` writes the event log after a run.
- Starter street (`townStarter` > 0): bakery, mill (4 querns) and an 8x7 farm (barn row + 48
  wheat cells) at the east end.

### The mill (2026-11-25, user: "look at mill next")

Measured on one summer day (load a day-10 save, run a day): the 4 querns ground **12-13 flour
of a possible 40**. What the millers did, from the event log:
- **They walked home to drink**, 35 game minutes each way, in the middle of a shift: the mill
  had no tap (the bakery lost its tap to the bread shelf). Everyone drinks 4-5 times a day.
- **The wheat sat in the farm's barn**: haulers put a harvest in the nearest store, so the mill's
  granary rarely got any and every grind started with a walk to the farm.
- A rule "on shift, only work inside your workplace" (`TownJobAllowed` in `JobInRange`, which
  every WorkGiver calls) changed nothing by itself (12 grinds), but it is what having a job means
  (off shift and jobless: any work), so it stays.

Fixed: **workplaces get a tap and a toilet** for the staff (one row in, right of the door), and
**store priorities pull goods toward their users**: the mill's granary and the bakery's flour
store 7, the farm's barn 4, the bread shelf 3, the pantries 7 (the existing stockpile
maintenance moves goods to higher-ranked stores). The summer day: **19 grinds**; by day 10,
140 (was 107).

Then winter starved everyone (day 28: 12 starving, "49 bread in stock"): all 49 loaves were
**rotten, in the pantries**. Rotten food filled every pantry slot (it only leaves for a refuse
pile; the town has none) and "Do Until X" counted it, so the bakers stopped at 48 rotten loaves.
Starving millers then dropped every job each 15 minutes (a critical need takes the job, the food
search finds nothing, the job comes back): nothing worked. Fixed:
- **Rotten food in a town lot is thrown out** (housekeeping, counted in `townWasted[]`, in the
  `Town:` line): a stand-in for bins and a garbage truck (City 1999).
- **Imports count usable stock only**: not rotten, in stacks big enough to use (20 single grains
  had blocked the wheat import while the mill stood still).

Also found and reported (fixed by navkit-e0, v118): `dayLength` was not saved, so after any load
days ran 6x fast (60 s default); every game that sets a 6-minute day starved after a load.
New: `--inspect --item` shows food condition; `--inspect --workshop` shows completions of
"until" bills.

**A year now** (`--set townStarter 6`, 12 people):

| day | hunger | wheat in town | flour imported | wheat imported | bread wasted |
|---|---|---|---|---|---|
| 7 | 6 Full, 6 Peckish | 142 | 100 | 400 (start-up) | 0 |
| 14 | 12 Full | 148 | 120 | 400 | 0 |
| 21 | 7 Full, 4 Peckish, 1 Hungry | 20 | 180 | 660 | 45 |
| 28 (winter) | 6 Full, 4 Peckish, 2 Hungry | 20 | 340 | 1500 | 45 |

Summer is self-sufficient but for ~3 flour a day. Open, and a design call: **autumn and winter**.
The farm (48 cells) can't build a winter stock: wheat runs out in autumn (season 0.8x, the mill
now grinds what there is), and winter has no harvest at all (840 wheat imported). Options: a
bigger farm, granaries big enough for 7 days (~170 wheat for 12 people), food that keeps
(bread spoils in 5 days; flour and wheat don't), or winter imports as a cost once there's money.

### Winter: a granary (2026-11-25, user: "lets just make a big enough storage")

- **Granary** lot (play HUD "Granary", dev Draw > City > Granary): its floor either side of the
  aisle stores wheat and flour (they keep; bread spoils in ~5 days), priority 5: between the
  farm's barn (4) and the mill's granary / bakery's flour store (7). No jobs. `Town:` line shows
  what the stores hold (`TownStoreCapacity`).
- Storage only helps with a surplus to store: the starter farm grew from 48 to 99 wheat cells
  (12x9) and got a 15x6 granary (stores hold 550 grain in all).
- **Two farming bugs that emptied the fields after winter** (both in shared farming code, so
  survival gets them too):
  - Wheat was planted in winter and killed by the season, seed and all.
  - Worse: every harvest's seed was replanted at once, so by the end of autumn all seed was in
    the ground, and the first day of winter killed it all (`killed by season` x96): spring had
    no seed. `CropWorthPlantingNow` (farming.c): plant only if the crop grows this season and,
    when the next season kills it, there is time to ripen first at this season's pace (wheat
    stops ~4 days before winter). Used by `WorkGiver_PlantCrop`.
- **Seed grain** (`TownKeepSeed`): a harvest gives back one seed, so a field that ever lost seed
  never got its planted area back. Each farm keeps a seed per field cell waiting to be planted,
  setting stored wheat aside as seed (counted in the `Town:` line).
- **42 days measured** (12 people, starter street):

  | day | hunger | wheat in town | wheat imported | harvests |
  |---|---|---|---|---|
  | 14 | 11 Full, 1 Hungry | 290 | 380 (start-up) | 147 |
  | 21 (winter starts) | 6 Full, 5 Peckish, 1 Hungry | 98 (+78 kept as seed) | 380 | 188 |
  | 28 (spring 2 starts) | 4 Full, 7 Peckish, 1 Hungry | 20 | 820 | 188 |
  | 35 | 8 Full, 3 Peckish, 1 Hungry | 194 | 1080 | 252 |
  | 42 | 8 Full, 2 Peckish, 2 Hungry | 304 | 1080 | 336 |

  Autumn and year two's summer need no wheat from outside; winter still does (440): the
  99-cell farm doesn't grow enough surplus to fill the granary (550 room, 98 + seed in it at
  winter). Levers: a bigger farm (the player can draw one), two farms. Also slowly falling:
  fertility (wheat takes 20 a harvest; 128 -> 88 after a year: growth 0.63x -> 0.5x).

### Save (v113)

After the relations: the road rectangles, then per lot its geometry and the owners of its
furniture in cell order (row-major over the lot). Furniture is saved compacted (indices change),
so after a load each lot finds its furniture, stoves/counters, pantry stockpile and flour stack
again from its cells (`TownRelinkLot`); `townHome[]` follows from the bed owners. Older saves: no
town. `inspect.c` reads past it (`TownSkipSave`). Checked: run, save, load, run on: everyone keeps
their home, no extra newcomers, audit 0 violations. Not saved: arrival / housekeeping timers.
After loading a town save the play HUD shows the sandbox tools (the game is found from
`gameMode`); the dev Draw bar has the town tools.

## Open questions

- Roads as a cell fact: a byte layer per (x, y) costs 4 MB at 2048; a ground material
  (asphalt) costs nothing extra but adds a material (dreamengine mirrors the enums).
- Does a home own its beds, or a mover their bed? Town needs only "my home"; City 1999's
  `owner` field would generalise it later.
- What a citizen does at work in milestone 1 (stand at a desk vs. make something).

## Coordination

- Code in worktree `.claude/worktrees/town`, branch `town`; merged to master per verified step.
- `games.c` gets one row; save changes (lots, `Mover.home`) get a save version when merged:
  check the current version on master then.
- Cars session: drawn roads should later feed its road network (`roads.c` builds from nodes +
  edges); keep town roads as straight axis-aligned strips so that conversion stays easy.
