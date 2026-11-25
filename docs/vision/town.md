# Town: a city builder where every citizen is a person

> Status: steps 1-4b on master (2026-11-25): roads, homes, bakeries, jobs, opening hours; save v113. Next: step 5 (overview, play it), the grain chain. Game `town`.

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
