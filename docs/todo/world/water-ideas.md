# Water ideas: showcases, rafts, the next river steps

Status: idea (parked 2026-11-24)

Parked (2026-11-24). Ideas from the water-flow session that sound cool but are finicky: each one
touches several systems, and most need tuning by eye in the game. Nothing here is started. The
water sim itself, and what is done, is in `water.md`.

## What already exists to build on

| System | Already does |
|---|---|
| Water | falling, pressure, freezing, tide, surf, outflow at the map edge, a flow layer (`GetWaterFlow`), trickling springs (`springTrickle`, v103) |
| Drift | loose floating items go with the current, wash up, lost off the map edge (`ItemsDriftTick`, scenario `drift`) |
| Steam | water at 100 C boils, steam rises and blows with the wind, below 60 C condenses back with `AddWater`: water -> steam -> rain already loops |
| Temperature | permanent heat/cold source cells, insulation per material, `TEMP_MAGMA` (1200 C) defined but unused |
| Fire | permanent fire sources (comment says "torch/lava"), spreading pushed by wind, gives light |
| Smoke | rises, trapped under roofs, blown by wind |
| Weather | rain, snow, mist (map-wide, not local), thunderstorms, lightning fires |
| Vehicles | train cars and elevator cars: entities carrying passengers and cargo, pathfinder links with boarding and waiting |

## 1. Rivers: spring size versus drift (next, if water work resumes)

Trickling springs fixed the lowland flood (island seed 1, 12k ticks: 27k -> 1k units), but rivers
got shallower, so logs (need water 2 deep) barely drift: `drift` scenario logs 53 -> 2.7 cells.

- **Spring rate from catchment** (preferred): the island generator already computes flow
  accumulation per river; give each spring its own rate from it. Big rivers deep enough for logs,
  brooks elsewhere. Finicky part: a per-source rate has to be stored and saved (save version), and
  tuned so the big rivers don't flood the lowlands again.
- Alternatives: one higher `springRate` (fuller rivers, lowlands fill faster), or accept brooks.
- **Soaking** (pairs with the above): standing water on soil soaks away slowly (ties into the
  wetness/mud layer), so a closed basin settles where inflow = soaking + evaporation. Today basins
  without an outlet only lose level-1 water to evaporation: lowland water still grows ~+200 units
  per 12k ticks. Finicky: soak too fast and rivers vanish, too slow and nothing changes; rain could
  then add water too (a real water cycle; rain only sets wetness today).
- **Seasonal springs**: once a spring has a rate, it can run strong after rain and dry up in summer.
- **The sea is still**: floating things reaching the sea just sit offshore. A sea current, or the
  falling tide pulling water (and items) out, would make "lost to the sea" happen on the island.

## 2. Showcase terrain (cheapest first)

1. **Hot spring / geyser** - almost free, worldgen only. A few heat source cells under a
   spring-fed pool: it boils, steam drifts with the wind, cools and rains back down. Geyser: heat
   on a timer. Stays open water in winter while the rest freezes, so settlers and animals gather
   there. Needs trickling springs (done) so it doesn't flood. Finicky: steam amounts vs. the pool's
   inflow, and keeping it from boiling dry or flooding.
2. **Volcano without flowing lava** - cheap. Crater of permanent fire + heat sources, a smoke
   column bent by the wind, glow at night, nearby forest catching fire now and then. New materials
   (basalt, ash soil). Middle option: a "lava" cell that is hot and lit but doesn't move. Real
   flowing lava is a second fluid: expensive, not planned. Finicky: wildfire spread balance (a
   volcano that burns the whole island once is a bad start).
3. **Waterfall with mist** - medium, first new code. Cliffs and falling water work, but nothing
   reacts to water landing. Steam can't be the spray (at normal temperature it condenses at once).
   Needs a local mist effect where water lands: drawn only, or a cold variant of steam.

## 3. Rafts and floating structures

Drift is the physics rafts would use. Three tiers:

1. **Fixed floors on water (piers, jetties, pontoons)** - cheap. A built floor over water,
   walkable, water still flowing underneath. `HAS_FLOOR` exists; the work is allowing it over water
   and making the water sim treat it right (today a floor stops water from falling). Tide twist: a
   pontoon that floats up a level at high tide and down at low tide (the tide already tells the
   pathfinder when walkability changes).
2. **Rafts as vehicles, not grid cells** - medium, the fun one. Don't build a raft from grid cells
   and move them: every move changes walkability, and settlers, items, stockpiles and workshops are
   tied to grid coordinates. A raft is an entity on the water like a train or elevator car.
   - Drifts with `GetWaterFlow` like logs, only bigger.
   - Moored at a pier it stays; unmoored it drifts off with whatever is on it ("who forgot to tie
     up the raft with all our logs on it" - the kind of story the tide gives us).
   - A ferry between two piers: the elevator idea turned sideways, a pathfinder link with a wait.
   - Next step after: a raft with a few cargo slots (a stockpile on the raft), not walls.
3. **Buildings that float away** (houseboats with walls and a workshop) - expensive: moving a block
   of grid cells, or a vehicle with its own small grid. Far goal only.

Finicky across all tiers: what the water does under a floor, mover boarding on a moving thing,
and saves (a raft's position and cargo).

## 4. Flecks redesign

Flecks (light dashes sliding down flowing water, `waterFlecks`, off since f4e2deda) read as
random pixels. A redesign would need to read as flow at a glance: longer streaks, aligned over
several cells, only on strong current, or a subtle shift in the water tile instead of particles.
Check in the game before turning back on.

## Housekeeping

- Branch `water-churn-experiment` is superseded by master (less churn landed in 739ea550) and can
  be deleted.
