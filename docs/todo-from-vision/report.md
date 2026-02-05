# Vision → Implementation Order Report

Scope: reviewed `docs/vision/**` against current implementation in `src/**` to identify what’s already in place and rank remaining vision features by smallest required change first.

**What’s already implemented (directly or close)**
- Core logistics loop: mining, hauling, stockpiles, blueprints, building, and crafting with a stonecutter workshop. Files: `src/entities/jobs.*`, `src/world/designations.*`, `src/entities/stockpiles.*`, `src/entities/workshops.*`.
- Simulation stack: water, fire, smoke, steam, temperature, trees, and ground wear/grass. Files: `src/simulation/*.c`, `src/simulation/*.h`.
- Materials tracked per cell for walls/floors (but not yet rendered distinctly). Files: `src/world/material.*`.
- Z-level traversal with ladders/ramps and DF-style walkability. Files: `src/world/grid.*`, `src/world/cell_defs.*`.

**Doc drift worth noting**
- `docs/vision/entropy-and-simulation/water-system-differences-with-df.md` says there is no temperature/steam, but both exist in `src/simulation/temperature.*` and `src/simulation/steam.*`.

**Ordered build list (smallest change → largest change)**

| Order | Feature | Why it’s small (or smaller) | Likely touchpoints |
|---|---|---|---|
| 1 | Workshop status indicators + simple diagnostics | Uses existing workshop/bill/job data; mostly rendering/tooltip work | `src/entities/workshops.*`, `src/entities/jobs.c`, `src/render/rendering.c`, `src/render/tooltips.c` |
| 2 | Stockpile fill meters + overfull warnings | Stockpile counts and priorities already tracked; add UI/tooltip display | `src/entities/stockpiles.*`, `src/render/tooltips.c`, `src/render/ui_panels.c` |
| 3 | Job lines (debug overlay) | Job targets are already in `Job`; just render line from mover to target | `src/entities/jobs.*`, `src/entities/mover.*`, `src/render/rendering.c` |
| 4 | Follow-mover camera mode | Requires only camera offset logic and input toggle | `src/core/input.c`, `src/render/rendering.c`, `src/main.c` |
| 5 | Material-aware wall/floor rendering | Materials already stored; just apply `MaterialSpriteOffset` when drawing | `src/world/material.*`, `src/render/rendering.c` |
| 6 | Visual tiles/overlays for sources & drains (water/heat/cold/fire) | Sources already exist as flags; add icon overlay on draw | `src/simulation/water.*`, `src/simulation/temperature.*`, `src/simulation/fire.*`, `src/render/rendering.c` |
| 7 | “Road/path” tool as a floor blueprint alias | Floor blueprints exist; add a material preset and UI affordance | `src/world/designations.*`, `src/core/input.c`, `src/render/ui_panels.c` |
| 8 | Wetness/mud flags from water + movement slowdown | Cell wetness flags exist but unused; link to water presence and mover speed | `src/world/grid.h`, `src/simulation/water.c`, `src/entities/mover.c` |
| 9 | Dirt/cleaning loop (maintenance job) | Needs new job type and dirt state, but uses existing job framework | `src/entities/jobs.*`, `src/world/grid.h`, `src/render/rendering.c` |
| 10 | Needs system (hunger/sleep) interrupting jobs | Requires per-mover stats + interrupts; fits `needs-vs-jobs` design but bigger logic | `src/entities/mover.*`, `src/entities/jobs.*`, `src/entities/items.*` |
| 11 | Light/darkness grid + overlays | New grid system + rendering + interaction rules | `src/simulation/*`, `src/render/rendering.c`, `src/world/cell_defs.*` |
| 12 | Farming v1 (plots, growth, harvest) | New entities and jobs; larger feature set | `src/entities/*`, `src/world/*`, `src/render/*` |
| 13 | Seasons/weather | Cross-cuts temperature, water, farming; needs new time/state | `src/core/time.*`, `src/simulation/*` |
| 14 | Infinite world streaming | Major architecture change (overmap, deltas, streaming) | `src/world/*`, `src/core/saveload.c` |
| 15 | Belts/elevators/advanced logistics | New transport systems and pathing rules | `src/entities/*`, `src/world/pathfinding.*`, `src/render/*` |

If you want, I can take items 1–6 and turn them into a concrete, file-by-file implementation plan next.
