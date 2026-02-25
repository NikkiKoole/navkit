# 05a: Explore Designation (Straight-Line Scouting) — ✅ DONE

> Click into the unknown. Your mover walks blind in a direction, discovers what's there, and stops when blocked.

---

## The Problem

Right now movers pathfind everywhere using A*/HPA*. There's no way to say "go that way and see what's there." When fog of war lands (05b), players need a way to push into unexplored territory — but even without fog, a manual "walk in a straight line" command is useful for scouting and directing movers to map edges.

This is the only job type that uses **no pathfinding at all**. The mover walks cell-by-cell along a Bresenham line, stops when blocked by a wall, water, or map edge. The player manually guides exploration with repeated clicks.

---

## Why Straight-Line?

- **Narratively honest**: The mover doesn't know what's ahead. They can't plan a route through terrain they've never seen.
- **Zero HPA* cost**: No pathfinding means no chunk invalidation. The HPA* graph is untouched.
- **Player agency**: Exploration is a series of manual decisions. "Go that way." Hit a cliff. "Okay, try left." This feels like actual scouting, not "computer, find a route."
- **Simple**: No special pathfinding mode. Step along a line, check walkability.

---

## Design

### How It Works

1. Player enters explore mode (Work menu, key `e`)
2. Player clicks on any cell (near or far)
3. `DESIGNATION_EXPLORE` is placed on the target cell
4. WorkGiver picks it up, creates `JOBTYPE_EXPLORE` for an idle mover
5. Mover walks **cell-by-cell** along a Bresenham line toward the designation
6. Each step: check if next cell is walkable and not water
7. If blocked (wall, water, solid, map edge) → **job done**, designation cleared
8. If mover reaches the target cell → **job done**, designation cleared
9. Player sees where the mover ended up, clicks again to continue

### Movement Model

Unlike every other job, the explore job does **not** use A*/HPA* pathfinding. Instead:

- `RunJob_Explore()` computes the next Bresenham cell from the mover's current position toward the target
- Each tick, the mover moves toward the next waypoint cell (same smooth movement as other jobs)
- When the mover arrives at a cell, compute the next cell on the Bresenham line
- Before advancing: check if the next cell is walkable + no water. If not → done.
- The mover uses normal movement speed

Direction is recomputed from (currentCell → target) each time the mover reaches a waypoint. No Bresenham state needs to be stored. Slight drift is fine for exploration.

### Standard Designation Pattern

Uses the same designation model as gather grass, dig roots, etc:

- `DESIGNATION_EXPLORE` placed on target cell
- `OnTileDesignationEntry` cache (mover walks toward the tile, though may not reach it)
- `WorkGiver_Explore()` finds nearest unassigned explore designation, creates job
- `RunJob_Explore()` walks the Bresenham line — the only difference from other jobs is the movement phase
- Designation cleared on completion (reached target or blocked)

The designation is on the **target** cell, but the mover may never reach it (stopped by obstacle). That's fine — completion clears it either way.

### Designation Validation

Unlike most designations, explore has **minimal validation**:

- Target must be in grid bounds
- No existing designation on that cell
- That's it — the target cell itself can be unwalkable (a wall you're scouting toward)

The mover will walk toward it and stop when they can't continue. The designation is a waypoint/direction, not a work site.

### Single-Mover Focus

In current survival mode there's one mover. The explore command targets the nearest idle mover. When multi-mover arrives, explore could use a `canExplore` capability.

---

## Implementation

### Phase 1: Designation + Job Type

**`src/world/designations.h`** — add to DesignationType enum:
```c
DESIGNATION_EXPLORE,
```

Add work time constant (instant — no work phase):
```c
#define EXPLORE_WORK_TIME 0.0f
```

**`src/world/designations.c`** — add:
```c
bool DesignateExplore(int x, int y, int z);           // Minimal validation (bounds + no existing)
void CompleteExploreDesignation(int x, int y, int z);  // Just clears designation
```

**`src/entities/jobs.h`** — add at end of JobType enum (before JOBTYPE_COUNT):
```c
JOBTYPE_EXPLORE,
```

Add to `JobTypeName()` switch.

### Phase 2: Cache + WorkGiver + RunJob

**`src/entities/jobs.c`** — add:

**Cache**: `OnTileDesignationEntry` cache + rebuild function, added to `designationSpecs[]` table.

**`WorkGiver_Explore()`**:
- Find nearest unassigned `DESIGNATION_EXPLORE` from cache
- No capability check needed (any mover can explore)
- No pathfinding reachability check (the whole point is walking blind)
- Create job with `targetMineX/Y/Z` = designation coordinates
- Assign mover, remove from idle list

**`RunJob_Explore()`** — the unique part:

```
STEP_MOVING_TO_WORK:
  - Compute next Bresenham cell from mover's current cell toward target
  - If next cell is walkable + no water:
      - Set mover->goal to next cell (single cell waypoint)
      - Move mover toward it (reuse normal movement, no needsRepath)
      - When arrived, repeat
  - If next cell is NOT walkable, or has water, or out of bounds:
      - Complete designation → JOBRUN_DONE
  - If mover's current cell == target cell:
      - Complete designation → JOBRUN_DONE
```

No `STEP_WORKING` phase — explore is pure movement.

**Bresenham next-cell**: Given current (cx, cy) and target (tx, ty), compute the next cell on the Bresenham line. This is a single step of the algorithm — just need dx, dy, and determine which cell comes next. Recomputed fresh each step from current position.

**Movement without pathfinding**: Set `mover->goal` to the next cell. Set `mover->pathLength = 1` with the single waypoint, `mover->pathIndex = 0`. This reuses the existing mover movement tick (walk toward path[pathIndex]) without invoking A*/HPA*. When the mover arrives, `RunJob_Explore` sets the next waypoint.

**Add to job dispatch**: In `JobsTick()`, add `case JOBTYPE_EXPLORE: result = RunJob_Explore(job, mover, dt); break;`

### Phase 3: Action + Input

**`src/core/input_mode.h`** — add to InputAction enum:
```c
ACTION_WORK_EXPLORE,
```

**`src/core/action_registry.c`** — add registry entry:
```c
{
    .action = ACTION_WORK_EXPLORE,
    .name = "EXPLORE",
    .barDisplayText = "Explore",
    .barText = "L-click designate  R-click cancel  [ESC]Back",
    .barKey = 'e',
    .barUnderlinePos = 0,
    .requiredMode = MODE_WORK,
    .requiredSubMode = SUBMODE_NONE,  // Top-level work action
    .parentAction = ACTION_NONE,
    .canDrag = false,  // Single click, not drag
    .canErase = true,  // Can cancel with R-click
},
```

**`src/core/input.c`** — add:
- `ExecuteDesignateExplore()` — calls `DesignateExplore(x, y, z)` on clicked cell
- `ExecuteCancelExplore()` — cancels explore designation at clicked cell
- Add case to main input dispatch switch

### Phase 4: Visual Feedback (polish)

**Explore mode cursor**: When in explore mode, draw a line from the nearest mover toward the cursor. Shows intended direction.

**Blocked message**: When explore job ends because of obstacle, show message ("Path blocked").

---

## Key Files

| File | Changes |
|------|---------|
| `src/world/designations.h` | `DESIGNATION_EXPLORE` enum |
| `src/world/designations.c` | `DesignateExplore()`, `CompleteExploreDesignation()` |
| `src/entities/jobs.h` | `JOBTYPE_EXPLORE` enum + name |
| `src/entities/jobs.c` | Cache, `WorkGiver_Explore()`, `RunJob_Explore()`, dispatch |
| `src/core/input_mode.h` | `ACTION_WORK_EXPLORE` enum |
| `src/core/action_registry.c` | Registry entry |
| `src/core/input.c` | Click handlers + dispatch |

No save version bump needed (no new grids or struct changes).

---

## Test Plan

- Bresenham next-cell: correct direction for horizontal, vertical, diagonal, and steep angles
- Mover walks straight line on open terrain, arrives at target → job done, designation cleared
- Mover stops one cell before wall → job done, designation cleared
- Mover stops one cell before water → job done
- Mover stops at map edge → job done
- Explore designation on unwalkable cell (wall) is valid — mover walks toward it and stops
- Mover can be given new explore job immediately after previous one ends
- Explore job doesn't call pathfinding (no chunk invalidation)
- Cancel explore designation with R-click removes it
- WorkGiver assigns idle mover to explore designation

~15-20 assertions expected.

---

## Connections

- **Fog of war (05b)**: When fog lands, each explore step also reveals cells within vision radius. The explore job becomes the primary way to push into unknown territory.
- **Animals**: Mover walking through terrain may encounter animals — flee behavior already handles this.
- **Hunting**: Explore + spot animal + designate hunt is a natural gameplay loop.

---

## Not In Scope

- Fog of war grid / rendering (that's 05b)
- Line-of-sight / vision radius (that's 05b)
- WorkGiver filters for explored state (that's 05b)
- Multi-mover explore assignment
- Visual feedback line (Phase 4 polish, can defer)
