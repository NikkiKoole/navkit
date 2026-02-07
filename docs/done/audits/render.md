Audit Results: src/render/rendering.c

**Finding 1: Dig ramp designation is invisible**
- **What**: `DrawMiningDesignations()` has overlay rendering for 8 designation types but completely skips `DESIGNATION_DIG_RAMP`. Active `JOBTYPE_DIG_RAMP` jobs also have no "assigned" overlay.
- **Assumption**: Every designation type the player can create has visual feedback.
- **How it breaks**: Player designates cells for ramp digging → no overlay appears → player thinks it didn't work or re-designates.
- **Player impact**: Invisible designation. No way to see pending ramp dig work.
- **Severity**: **HIGH**
- **Suggested fix**: Add a `DESIGNATION_DIG_RAMP` block in `DrawMiningDesignations()` (like the others), plus an active job overlay loop for `JOBTYPE_DIG_RAMP`.

**Finding 2: Idle movers flash RED between tasks**
- **What**: Default mover color logic: `pathLength == 0` → RED, `repathCooldown > 0 && pathLength == 0` → ORANGE. These states occur normally when a mover finishes a job or reaches a wander goal and waits for a new one.
- **Assumption**: RED means "broken/stuck." But idle movers legitimately sit at pathLength=0 for a few frames.
- **How it breaks**: After every completed/cancelled job (including consolidation), movers flash red for 1-2 frames before getting a new goal. This is the "flicker red" you saw with the consolidation movers.
- **Player impact**: Movers appear broken when they're fine. Creates false alarm.
- **Severity**: **MEDIUM**
- **Suggested fix**: Don't show RED/ORANGE for jobless movers. Could check `m->currentJobId < 0` — if no job, show WHITE (idle) instead of alarming colors. Or only show RED when `timeWithoutProgress > threshold`.

**Finding 3: Craft job line points to workshop during fuel fetch**
- **What**: `DrawJobLines()` for CRAFT jobs draws to the input item during steps 0-1, but falls through to "draw to workshop" for ALL other steps — including `CRAFT_STEP_MOVING_TO_FUEL` (step 4) and `CRAFT_STEP_PICKING_UP_FUEL` (step 5), when the mover is walking AWAY from the workshop to fetch fuel.
- **Assumption**: Job lines show where the mover is heading.
- **How it breaks**: Crafter walks east to get charcoal, but the job line points west to the workshop.
- **Player impact**: Confusing visual — line contradicts mover direction.
- **Severity**: **MEDIUM**
- **Suggested fix**: Add cases for `CRAFT_STEP_MOVING_TO_FUEL` and `CRAFT_STEP_PICKING_UP_FUEL` that draw to `job->fuelItem` position instead of the workshop.

**Finding 4: DrawItems iterates all 25000 slots instead of highWaterMark**
- **What**: `DrawItems()` uses `for (int i = 0; i < MAX_ITEMS; i++)` (25000) while simulation code uses `itemHighWaterMark` for efficiency.
- **Assumption**: Rendering should be at least as efficient as simulation loops.
- **How it breaks**: With only 50 items, still checks 25000 slots per frame.
- **Player impact**: Minor performance waste. Not visible but unnecessary.
- **Severity**: **LOW**
- **Suggested fix**: Change to `i < itemHighWaterMark`.
