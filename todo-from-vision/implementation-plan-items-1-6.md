# Implementation Plan (Vision Items 1–6)

Scope: concrete, file-by-file plan for the first six smallest-change vision features.

---

## 1) Workshop Status Indicators + Diagnostics
Goal: display workshop state (working/input empty/output full/no worker) and show diagnostics in tooltip.

Files + changes:
- `src/entities/workshops.h`
  - Add diagnostic fields on `Workshop` (or a parallel array):
    - `float inputStarvationTime`
    - `float outputBlockedTime`
    - `float lastWorkTime`
  - Add an enum for visual state if helpful.
- `src/entities/workshops.c`
  - Add helpers:
    - `bool WorkshopHasInput(Workshop* ws)`
    - `bool WorkshopOutputBlocked(Workshop* ws)`
    - `WorkshopVisualState GetWorkshopVisualState(Workshop* ws)`
  - Update diagnostics every tick (if a workshop tick exists) or from job driver.
- `src/entities/jobs.c`
  - In `RunJob_Craft`, update:
    - `lastWorkTime` when progress > 0
    - `inputStarvationTime` when missing input
    - `outputBlockedTime` when output tile blocked or storage full
- `src/render/rendering.c`
  - Draw a small colored indicator at work tile or output tile.
  - Colors per vision doc: green/yellow/red/gray.
- `src/render/tooltips.c`
  - On workshop hover, show state + timers.

Notes:
- Keep heuristics simple at first; refine once visible.

---

## 2) Stockpile Fill Meters + Overfull Warnings
Goal: show fill % for stockpiles and warn when overfull.

Files + changes:
- `src/entities/stockpiles.h`
  - Add helpers:
    - `float GetStockpileFillRatio(int stockpileIdx)`
    - `bool IsStockpileOverfull(int stockpileIdx)`
- `src/entities/stockpiles.c`
  - Implement fill ratio from `freeSlotCount` and active cells.
  - Implement overfull check by scanning `slotCounts` vs `maxStackSize`.
- `src/render/tooltips.c`
  - Show meter text like `[||||||||  ] 80%`.
- `src/render/ui_panels.c`
  - Optional: list stockpiles with % full in a panel.

---

## 3) Job Lines (Debug Overlay)
Goal: draw lines from movers to job targets (pickup/dropoff/worksite).

Files + changes:
- `src/render/ui_panels.c`
  - Add toggle `showJobLines`.
- `src/render/rendering.c`
  - When enabled, draw per-mover line(s):
    - Haul: mover → item, item → stockpile slot
    - Mine/Build: mover → target tile
    - Craft: mover → input item / workshop work tile
- `src/entities/jobs.h`
  - Optional helper `GetJobTargetPosition(job, &x, &y, &z)`.

---

## 4) Follow-Mover Camera Mode
Goal: camera locks to a selected mover.

Files + changes:
- `src/core/input.c`
  - Add hotkey or click+modifier to set `followMoverIdx`.
- `src/render/rendering.c` or `src/main.c`
  - If follow enabled, update camera offset each frame to center on mover.
- `src/entities/mover.h`
  - Ensure position accessors exist for mover.

---

## 5) Material-Aware Wall/Floor Rendering
Goal: render material variants for walls/floors using `MaterialSpriteOffset`.

Files + changes:
- `src/world/material.h`
  - Confirm `MaterialSpriteOffset` available for all materials.
- `src/render/rendering.c`
  - When drawing a wall or floor, apply material sprite offsets:
    - `GetWallMaterial()` for walls
    - `GetFloorMaterial()` for floors
- `assets/atlas.h`
  - Confirm sprite ordering supports offsets.

---

## 6) Source/Drain/Heat/Cold/Fire Overlay Icons
Goal: show icons or tint overlays for special tiles.

Files + changes:
- `src/simulation/water.h/.c`
  - Add helpers: `IsWaterSourceAt`, `IsWaterDrainAt`.
- `src/simulation/temperature.h/.c`
  - Add helpers: `IsHeatSourceAt`, `IsColdSourceAt`.
- `src/simulation/fire.h/.c`
  - Add helper: `IsFireSourceAt`.
- `src/render/rendering.c`
  - Overlay small icon sprites or tints after base tile draw.
- `assets/atlas.h`
  - Confirm icon sprites exist or add new ones.

---

## Suggested Implementation Order
1. Workshop visual state + tooltip diagnostics
2. Stockpile fill meter + overfull warning
3. Job-line overlay toggle + render
4. Follow-mover camera toggle
5. Material-aware wall/floor rendering
6. Source/drain/heat/cold/fire overlays

