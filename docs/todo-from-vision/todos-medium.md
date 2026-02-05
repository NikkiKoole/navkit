# Medium TODOs Extracted From Vision

These are medium-sized features: new systems or cross-file changes, but still bounded in scope.

- [ ] Wetness/mud from water + movement slowdown + drying
  - Track per-cell wetness using existing `CELL_WETNESS_*` flags
  - Generate mud when water flows over dirt
  - Slow movers in muddy tiles and clear over time
  - Files: `src/simulation/water.*`, `src/world/grid.h`, `src/entities/mover.*`, `src/render/rendering.c`

- [ ] Simple cleaning job loop (floor dirtiness → cleaning job → clean)
  - Add a dirt/dirty overlay state to floors
  - Generate dirt from traffic (can reuse groundwear or a simple counter)
  - Add `JOBTYPE_CLEAN` and a workgiver for dirty tiles
  - Files: `src/entities/jobs.*`, `src/world/grid.h`, `src/simulation/groundwear.*`, `src/render/rendering.c`

- [ ] Basic weeding for farm/soil tiles
  - Add weed overlay state on farm tiles
  - Add `JOBTYPE_WEED` and a workgiver for weeded tiles
  - Files: `src/entities/jobs.*`, `src/world/grid.h`, `src/render/rendering.c`

- [ ] Simple refuel job for torches/fire sources
  - Add fuel items and a “refuel needed” flag
  - Add `JOBTYPE_REFUEL` and workgiver
  - Files: `src/entities/jobs.*`, `src/simulation/fire.*`, `src/entities/items.*`

- [ ] Simple repair job for damaged walls
  - Add a damaged wall state and decay timer
  - Add `JOBTYPE_REPAIR` and workgiver
  - Files: `src/entities/jobs.*`, `src/world/grid.h`, `src/world/material.*`

- [ ] Light/darkness grid v1 + visual overlay
  - Add a light grid (ambient + sources)
  - Use it for basic visibility overlay, no gameplay yet
  - Files: `src/simulation/*`, `src/render/rendering.c`

- [ ] Temperature-driven seasonal ambient profile
  - Add season curve to ambient surface temp
  - Changes to `UpdateTemperature` and time system
  - Files: `src/core/time.*`, `src/simulation/temperature.*`

