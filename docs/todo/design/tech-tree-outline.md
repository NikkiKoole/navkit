# Tech Tree Outline (From Current Codebase)

Date: 2026-02-07
Scope: Untangle the “bare hands → sticks → mud → stone” discussion into a short, actionable tech outline grounded in current systems.

## Current Baseline (What Exists Now)
- **Items**: rock, blocks, logs, saplings, leaves, dirt, clay, gravel, sand, peat, planks, sticks, bricks, charcoal, ash.
- **Materials**: wood species, granite, dirt/brick/iron/glass, soil mats.
- **Cells**: wall/air, ladders, ramps, trees (trunk/branch/root/leaves/felled).
- **Workshops**: stonecutter, sawmill, kiln, charcoal pit, hearth (with recipes).
- **Systems**: water, fire/smoke/steam, temperature, ground wear.

## Era 0: Bare Hands (Gather & Shape)
**Goal**: Get the first fuel loop and construction inputs.
- **Doable actions (no tools)**:
  - Gather leaves, saplings, sticks, dirt/clay/gravel/sand/peat (already spawned items).
  - Start **Hearth** (burn fuel → ash) using simple fuel items.
- **Outputs you already support**: sticks/leaves as fuel, ash as byproduct.

## Era 0.5: Fire & Fuel Control
**Goal**: Reliable heat and charcoal.
- **Workshop unlocks**:
  - **Charcoal Pit**: log/stick/peat → charcoal (primitive, slower).
  - **Hearth**: any fuel → ash (fuel sink).
- **Autarky payoff**: fuel loop closed, ash available for later soil/soap loops.

## Era 1: Wood Processing
**Goal**: Structural wood outputs.
- **Workshop**: **Sawmill**
- **Outputs**: planks, sticks (already implemented).
- **Next step**: enable **wood floors** and **wood blocks** as construction sinks.

## Era 1.5: Clay & Brick
**Goal**: Durable materials.
- **Workshop**: **Kiln**
- **Outputs**: bricks, charcoal (already implemented).
- **Next step**: brick floors/walls (material-based floors are already supported).

## Era 2: Stone Processing
**Goal**: Reliable stone construction.
- **Workshop**: **Stonecutter**
- **Outputs**: blocks (already implemented).
- **Next step**: stone floors/walls as sinks for blocks.

## Immediate Tech Gaps (Small, Actionable)
These are the *smallest* new additions that unlock more loops:
- **Dried Grass** (item) → enables thatch, bedding, and primitive roofing.
- **Vines / Cordage** (item) → simple “binding” for staged construction.
- **Bark / Resin** (item) → primitive glue, early tool progression.
- **Reed / Hollow Grass** (item) → enables simple pipes / blow‑pipe later.

## Short‑Term Unlock Plan (Actionable)
1. **Add dried grass** (item) + simple drying action.
2. **Add a “cordage” item** (crafted from grass/vines).
3. **Introduce staged construction** for walls/floors (see construction‑staging doc).
4. **Add material‑based floors** (wood/plank, brick, block) as sinks.

