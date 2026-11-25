# Town: a city builder where every citizen is a person

> Status: plan + step 1 in progress (2026-11-25). Game `town`.

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

1. **Game + roads.** Game `town`: flat 128x128 (later bigger), instant build, needs on, nobody
   dies (sandbox rules like the dollhouse). A road tool drags a road (a paved strip, 3 wide) on
   the ground. A road touching the map edge is where newcomers arrive.
2. **Lots and buildings.** A lot tool drags a rectangle next to a road, kind Home or Work. A
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
