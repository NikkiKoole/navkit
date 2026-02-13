# Mover Work Animations

## Goal
Add subtle movement to movers while they work, making them feel alive instead of frozen in place. Two animation types based on work position:

- **Sway** (back/forth toward target) — working on an adjacent tile
- **Bob** (up/down in place) — working on the tile they're standing on

## Current State
- Movers freeze completely during `STEP_WORKING` — position doesn't update
- No animation fields on `Mover` struct (only `fallTimer` for fall visual)
- Work progress tracked in `job->progress` (0.0–1.0) — not useful for animation timing
- Rendering in `rendering.c` lines 1102–1109: draws `SPRITE_head` at `(m->x, m->y)` scaled by zoom

## Design

### Animation Categories

**Sway (adjacent-tile work):** mover shifts toward/away from the target tile
| Job Type | Target |
|---|---|
| JOBTYPE_MINE | wall being mined |
| JOBTYPE_DIG_RAMP | cell being carved |
| JOBTYPE_REMOVE_RAMP | ramp being removed |
| JOBTYPE_CHOP | tree trunk |
| JOBTYPE_CHOP_FELLED | felled log |
| JOBTYPE_GATHER_SAPLING | sapling |
| JOBTYPE_GATHER_TREE | tree (branch harvesting) |
| JOBTYPE_BUILD | blueprint cell |
| JOBTYPE_CRAFT | workshop work tile |
| JOBTYPE_IGNITE_WORKSHOP | workshop work tile |

**Bob (on-tile work):** mover bounces gently in place
| Job Type | Context |
|---|---|
| JOBTYPE_CHANNEL | digging down |
| JOBTYPE_REMOVE_FLOOR | removing floor beneath |
| JOBTYPE_PLANT_SAPLING | planting on tile |
| JOBTYPE_GATHER_GRASS | harvesting grass underfoot |
| JOBTYPE_CLEAN | scrubbing floor |

### Animation Parameters

```
Sway:
  frequency:  3.0 Hz (3 cycles/sec — brisk working pace)
  amplitude:  0.15 * CELL_SIZE (~1.2 px at 8px cells)
  direction:  toward job target tile (dx, dy normalized)
  waveform:   sin(phase * 2π)

Bob:
  frequency:  4.0 Hz (slightly faster — small motion)
  amplitude:  0.10 * CELL_SIZE (~0.8 px)
  direction:  vertical (y-axis only)
  waveform:   |sin(phase * 2π)| (bounce — always upward from rest)
```

### Implementation

**1. Add field to Mover struct** (`mover.h`)
```c
float workAnimPhase;  // Accumulates time for work animation cycling
```

**2. Advance phase during rendering** (`rendering.c`, in DrawMovers)

Use real frame time (`GetFrameTime()`), **not** `gameDeltaTime`. The animation is purely visual — it should look the same at 1x, 5x, or 50x game speed. At high speeds the mover simply works for fewer real seconds, but the sway/bob rhythm stays natural.

```c
if (job && job->step == STEP_WORKING) {
    m->workAnimPhase += GetFrameTime();  // real time, not game time
} else {
    m->workAnimPhase = 0.0f;
}
```

Note: advancing in the render loop avoids touching every `RunJob_*` function. The phase resets naturally when the job ends or transitions out of `STEP_WORKING`.

**3. Apply offset during rendering** (`rendering.c`, in DrawMovers)

After computing `sx, sy` from `m->x, m->y`, checks both `STEP_WORKING` and `CRAFT_STEP_WORKING`:
```c
bool isWorking = job && (job->step == STEP_WORKING || job->step == CRAFT_STEP_WORKING);
```

For sway, the work target position is resolved per job type:
- `JOBTYPE_CRAFT` / `JOBTYPE_IGNITE_WORKSHOP` → workshop work tile (`ws->workTileX/Y`)
- `JOBTYPE_BUILD` → blueprint position (`blueprints[idx].x/y`)
- All others (mine, chop, etc.) → `job->targetMineX/Y`

### What This Doesn't Do
- No tool-specific animations (axe swing vs pickaxe)
- No particle effects (dust, sparks)
- No sprite changes (still uses SPRITE_head)
- No sound sync

These could be added later but are out of scope for the initial implementation.

## Files Modified
- `src/entities/mover.h` — add `float workAnimPhase`
- `src/render/rendering.c` — advance phase + apply sway/bob offset in DrawMovers

## Verification
- `make clean && make path` — builds
- `make test` — passes (animation is render-only, no gameplay change)
- In-game: designate mining next to a wall, observe mover sways toward wall while mining
- In-game: designate channel, observe mover bobs while digging down
