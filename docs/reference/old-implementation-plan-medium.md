# Implementation Plans (Medium Items)

## 1) Wetness/Mud From Water + Movement Slowdown + Drying

Goal: represent wetness levels on dirt tiles, create mud where water flows, slow movers on muddy tiles, dry over time.

Files + changes:
- `src/world/grid.h`
  - Use `CELL_WETNESS_*` flags (`GET_CELL_WETNESS`, `SET_CELL_WETNESS`).
- `src/simulation/water.c`
  - When water level > 0 on a dirt cell, set wetness to wet/soaked.
  - When water level drops to 0, decay wetness over time (interval-based).
  - Add a small accumulator for drying (similar to groundwear).
- `src/entities/mover.c`
  - Apply movement speed modifier based on wetness level on the tile a mover stands on.
- `src/render/rendering.c`
  - Add a subtle overlay/tint or alternate sprite for wet/muddy tiles.

Notes:
- Keep wetness purely visual + movement penalty at first.
- If you want mud to persist, decay rate should be slow and time-driven.

---

## 2) Simple Cleaning Job Loop (Dirty Floors → Clean)

Goal: track floor dirtiness, spawn cleaning jobs, clear dirt when cleaned.

Files + changes:
- `src/world/grid.h`
  - Introduce a new cell flag for dirty floors (e.g., `CELL_FLAG_DIRTY`).
- `src/simulation/groundwear.c`
  - When traffic passes on indoor floors, set dirty flag (threshold-based).
- `src/entities/jobs.h/.c`
  - Add `JOBTYPE_CLEAN`.
  - Add workgiver to find dirty tiles and assign jobs.
  - Add job driver to “clean” (clear dirty flag after work time).
- `src/render/rendering.c`
  - Show dirt overlay on floors.

Notes:
- Start with a single dirty stage (binary); add multiple stages later if needed.

---

## 3) Basic Weeding (Farm/Soil Tiles)

Goal: add weed state to farm tiles and a weeding job.

Files + changes:
- `src/world/grid.h`
  - Add a weed flag or reuse an existing overlay bit.
- `src/simulation/groundwear.c` or new file
  - If a farm tile is untended for N days, set weed flag.
- `src/entities/jobs.h/.c`
  - Add `JOBTYPE_WEED`.
  - Workgiver finds weeded tiles.
  - Job driver clears weed flag after work time.
- `src/render/rendering.c`
  - Draw weed overlay on farm tiles.

---

## 4) Simple Refuel Job for Torches/Fire Sources

Goal: add a resource loop for maintaining fire sources.

Files + changes:
- `src/entities/items.h/.c`
  - Add a basic fuel item type (e.g., `ITEM_WOOD` already exists).
- `src/simulation/fire.h/.c`
  - Add per-cell “fuel remaining” for fire sources.
  - When low/empty, mark as needing refuel.
- `src/entities/jobs.h/.c`
  - Add `JOBTYPE_REFUEL`.
  - Workgiver finds refuel-needed fire sources and assigns.
  - Job driver consumes a fuel item and restores fuel time.
- `src/render/rendering.c`
  - Visual indicator for “needs fuel.”

---

## 5) Simple Repair Job for Damaged Walls

Goal: track wall damage and allow repair jobs.

Files + changes:
- `src/world/grid.h`
  - Add a damage state for walls (flag or small int array).
- `src/world/material.c`
  - If desired, store per-cell durability/repair cost per material.
- `src/entities/jobs.h/.c`
  - Add `JOBTYPE_REPAIR` and workgiver.
  - Job driver clears damage state after work.
- `src/render/rendering.c`
  - Show cracked/damaged wall overlay.

---

## 6) Light/Darkness Grid v1 + Visual Overlay

Goal: create a basic light grid and show a visibility overlay (no gameplay yet).

Files + changes:
- `src/simulation/light.h/.c` (new)
  - Store light levels per cell; ambient + simple point lights.
  - Simple flood or raycast for light propagation.
- `src/render/rendering.c`
  - Apply a darkening overlay based on light level.
- `src/core/time.c` (optional)
  - Modify ambient light by time-of-day.

---

## 7) Seasonal Ambient Temperature Curve

Goal: vary ambient temperature by day-of-year.

Files + changes:
- `src/core/time.h/.c`
  - Add `yearDay` or `seasonPhase` based on `dayNumber`.
- `src/simulation/temperature.c`
  - In `GetAmbientTemperature`, apply a seasonal curve.
  - Start with a simple sine wave (winter low, summer high).
- `src/render/ui_panels.c`
  - Optional: display season in UI.

