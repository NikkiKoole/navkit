# NavKit now

> Read this first: what the project is today, in one minute. Keep it short and current
> (rewrite it, don't append; the changelog is "Recently active" in `todo/INDEX.md`).
> Last updated 2026-11-26.

## What this is

NavKit started as a 3D grid pathfinding library (`nav/`: A*, HPA*). A Dwarf-Fortress-like
colony sim grew on top of it: movers, jobs, items, stockpiles, workshops, water, fire, heat,
needs, social behaviour.

**The heart is `vision/vision.md`** (Koyaanisqatsi: watch systems interact, no fantasy, a closed
loop by necessity, logistics and entropy instead of combat, industrial pastoral). The user
confirmed on 2026-11-26 that it still holds.

**There is no single "the game" yet, on purpose.** The games and scenarios are *lenses*: each
one is a way of playing that pulls systems out of the engine and tests them. Two directions
meet in the middle:

- **bottom-up**, from bare survival (Cataclysm-like: one person, hunger, cold, scrounging)
- **top-down**, from a whole city (Town, City 1999, cars and roads)

The design is still in flux. Pick work that makes the engine better for *every* lens, not one.

## The lenses

| Lens | What it is | State | Where we are |
|---|---|---|---|
| Survival | One settler, 128x128 wilderness, fog of war | live | `todo/gameplay/` (survival-*), INDEX |
| Island | Three settlers on a generated island; survival rules | live | `todo/world/island.md` |
| Dollhouse | Two people in an apartment: needs, mood, stations | live | `todo/dollhouse-domestic-life.md` |
| Sandbox | Empty map, every tool, draw freely | live | - |
| Town | City builder where every citizen is a mover | live, newest | `vision/town.md` "Where we are" |
| City 1999 | One homeless person in a city: bins, cans, coins | paused (scenarios `trash`, `bottlerun`) | `vision/city-1999.md` "Where we are" |
| Cars | 2x2 steering cars on a lane network, road tool | experiment, branch `cars-experiment` | `research/cars-experiment.md` (on that branch) |
| Galerij | A block of flats: 8 households, front doors, friends visiting | scenarios `galerij`, `visiting` (newest) | `todo/architecture/ownership-and-access.md` "Where we are" |
| Scenarios | ~80 dev/test setups (`--scenario list`) | dev | `src/core/scenarios.c` |

Engine threads that the lenses share (each doc has its own state):
social behaviour (`todo/social/noticing.md`), action queue (`todo/architecture/action-queue-and-need-index.md`),
explosions (`todo/world/explosions.md`, paused), throwing (`todo/gameplay/throwing.md`),
water and tide (`todo/world/water.md`, `todo/world/tide.md` parked), belts and trains
(`todo/world/conveyors.md`, `todo/world/trains.md`), big worlds (`todo/architecture/chunked-world-plan.md`),
infrastructure: doors, locks, mechanisms, pipes (`todo/world/infrastructure.md`: doors open / closed / locked
and the door chore built, visiting built, levers / pressure plates / links built (save v125, built and
linked in play, the user's tiles), pipes laid by movers; next: gates), ownership and access: who may enter, use, take
what, where (`todo/architecture/ownership-and-access.md`: core + households built; Town's beds and City 1999's
shops to move onto it). Behaviours we hope to see emerge: `vision/hoped-for-behaviours.md`.

## Next: one UI for many ways of playing

The next big piece is a UI that supports different ways of playing on the same engine:

- **one designated player** (direct control, the action queue: survival, City 1999)
- **a designated group** (colony style: orders, jobs, work)
- **sandbox**: draw tiles, and place bigger blocks (rooms, buildings) at once
- **a UI built from data for a scenario/game**: a game says which tools, panels and verbs it shows

Already in place: games table (`src/core/games.c`), settings / commands / verbs as table rows,
find palette, selection + inspector, play HUD tabs. Plans: `todo/ui/game-structure.md`,
`todo/ui/find-palette.md`, `todo/ui-redesign.md`. Sketches live in **dreamengine**:
`docs/design/navkit-ui-sketches.md`, `docs/design/nk-navkit-wishlist.md`, and the playable
`tools/carts/nk-*.c` carts (nk-toolbar, nk-find, nk-build, nk-sandbox, nk-survival, nk-colony,
nk-sims, nk-tribe, nk-pie, nk-dev, ...).

## Standing decisions (the user's calls)

- The dev UI (TAB) is an overlay over any game, never a mode; it doesn't change the rules.
- A game's `start()` sets every rule toggle it relies on.
- Belts are core, as long as games without belts pay about nothing for them.
- The daily rhythm is soft: it comes from world facts (light, needs), not clock rules.
- Movers stay naive about the tide (being cut off by it is a feature).
- Explosions: no injuries, no grenades.
- Money is whole neutral coins (no guilders, no cents); price 0 = free.
- Pie menu: parked.

## Ground rules for sessions

- Several sessions work in parallel on master: commit only your own files, by explicit list.
- Before committing: `make test` and `make scenarios-check`.
- When you finish something: one line under "Recently active" in `todo/INDEX.md`, and update
  this page if the lenses, their states or the next focus changed.
