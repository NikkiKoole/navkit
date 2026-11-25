# Town: a city builder where every citizen is a person

> Status: steps 1-2 built on branch `town` (2026-11-25), step 3 next. Game `town`.

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
3. **People move in.** A home with a free bed draws a newcomer: spawned at a road on the map
   edge, walks to the house, it is now their home (`Mover.home`, a lot id). They sleep in their
   own home's bed; the beds of other homes are not theirs. Food: each home has a tap (mains
   water) and a fridge the town restocks (a stand-in for shops, which come with City 1999's coins).
4. **Work.** A Work lot has opening hours and a number of jobs. A citizen without a job takes
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
- **Not saved yet**: the cells and furniture are, the road and lot lists are not (a load
  forgets which building is a lot). Comes with step 3 (`Mover.home` needs a save change anyway).

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
