# Construction Staging (CDDA‑style Phases, Minimal Scope)

Date: 2026-02-07
Scope: Make construction more involved without a large refactor. Use existing blueprints/jobs where possible.

## Why This Now
Your current build flow is “item → instant wall/floor.” The audit and design goals both want **multi‑step construction** without losing DF‑style stockpiles.

## Guiding Constraints (Current Code)
- Blueprints and construction jobs already exist.
- Items and materials already support wood/stone/clay outputs.
- We should avoid new, complex systems in the near term.

## Minimal Staging Model (2–3 Steps)

### A) Walls (Wood/Brick/Block)
**Stage 0**: Site cleared (already implicit).
**Stage 1**: **Frame** (uses sticks/planks as “frame”).
**Stage 2**: **Fill** (planks OR bricks OR blocks).

**Effect**: visually distinct intermediate cell, cancels cleanly if interrupted.

### B) Floors
**Stage 0**: Site cleared.
**Stage 1**: **Base layer** (dirt/gravel).
**Stage 2**: **Finish layer** (planks/bricks/blocks).

### C) Ladders/Ramps
Keep simple for now:
- Single stage (use existing logic)
- Optional later: add “support” stage if needed.

## Suggested Data Model (Small Change)
- Add `stage` to Blueprint or Job (0..N).
- Add `requiredInputs[stage]` per blueprint type.
- The job advances when all inputs for the current stage are delivered.

## WIP Tile Representation (Lightweight)
- Use a **temporary cell overlay** or a simple per‑tile stage map.
- Visuals can be minimal (e.g., “frame” sprite).

## Interactions with Stockpiles
- Inputs are hauled from stockpiles to the job site (already how build jobs work).
- Each stage is a distinct hauling burst, which makes the DF stockpile loop meaningful.

## Immediate Implementation Steps
1. Add a **2‑stage wall blueprint** (frame → fill).
2. Add a **2‑stage floor blueprint** (base → finish).
3. Add one intermediate sprite for “frame” and reuse it for both wall/floor.

## Deferred (Later)
- Weather/curing timers for mud walls.
- Stage‑specific tool checks (digging stick, mallet, etc.).
- Multi‑colonist stages (log bridge, heavy beams).

