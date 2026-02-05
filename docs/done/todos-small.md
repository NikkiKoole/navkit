# Small TODOs Extracted From Vision

- [ ] Add workshop visual state overlay (working / input empty / output full / no worker). Files: `src/entities/workshops.*`, `src/entities/jobs.c`, `src/render/rendering.c`.
- [ ] Add workshop diagnostics to tooltips (last work time, input/output blocked timers). Files: `src/entities/workshops.*`, `src/render/tooltips.c`.
- [ ] Show stockpile fill meter and overfull warning in UI/tooltip. Files: `src/entities/stockpiles.*`, `src/render/tooltips.c`.
- [ ] Add job-line debug overlay from mover → pickup → dropoff. Files: `src/entities/jobs.*`, `src/render/rendering.c`, `src/render/ui_panels.c`.
- [ ] Add follow-mover camera toggle (click mover or hotkey). Files: `src/core/input.c`, `src/render/rendering.c`.
- [ ] Render wall/floor materials via `MaterialSpriteOffset` so wood/stone read visually. Files: `src/world/material.*`, `src/render/rendering.c`.
- [ ] Overlay icons for water sources/drains and heat/cold/fire sources. Files: `src/simulation/water.*`, `src/simulation/temperature.*`, `src/simulation/fire.*`, `src/render/rendering.c`.
- [ ] Add a “Road/Path” build shortcut that places a stone floor blueprint. Files: `src/world/designations.*`, `src/core/input.c`, `src/render/ui_panels.c`.
