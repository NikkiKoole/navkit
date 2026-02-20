# Workshop Deconstruction — Implementation Plan

**Status: COMPLETE (save version 64). 21 automated tests in `tests/test_workshop_deconstruction.c`.**

## Context

Workshop construction costs are already implemented (save version 63). Workshops placed via MODE_WORK → Build → Workshop (T) create blueprints that require material hauling and building. The existing Blueprint system at the work tile handles construction.

Deconstruction is now implemented. D key on a workshop in MODE_NORMAL marks it for job-based deconstruction with 75% material refund. MODE_DRAW keeps instant delete (it's the cheat tool).

Design doc: `01-workshop-construction-costs.md` (see "Workshop Deconstruction" section).

---

## What Already Exists

- **`DeleteWorkshop()`** (`src/entities/workshops.c:451`): Clears CELL_FLAG_WORKSHOP_BLOCK, marks HPA* chunks dirty, removes light source, sets active=false. Does NOT cancel jobs or handle items.
- **D key handler** (`src/core/input.c:1897`): Inside `hoveredWorkshop >= 0 && inputMode == MODE_NORMAL` block. Calls `DeleteWorkshop()` directly.
- **`CONSTRUCTION_REFUND_CHANCE`** (`src/world/construction.h:145`): `#define CONSTRUCTION_REFUND_CHANCE 75` — already used by blueprint cancellation.
- **Construction recipes** for workshops: `CONSTRUCTION_WORKSHOP_CAMPFIRE` through `CONSTRUCTION_WORKSHOP_CARPENTER` in `src/world/construction.h/c` — these define the materials each workshop costs.
- **`WorkGiver_Build`** pattern (`src/entities/jobs.c:5017`): Find blueprint → pathfind to it → reserve → create job → assign mover. Template for new WorkGiver.
- **`RunJob_Build`** (`src/entities/jobs.c:1247`): 2-step job (STEP_MOVING_TO_WORK → STEP_WORKING with progress timer). Template for deconstruct job driver.
- **`AssignJobs_P4_Designations`** (`src/entities/jobs.c:2864`): Runs designation WorkGivers + blueprint WorkGivers. Deconstruction fits here.

---

## Step-by-step

### Step 1: Workshop struct — add `markedForDeconstruct`

**`src/entities/workshops.h`** — add fields to Workshop struct:
```c
bool markedForDeconstruct;     // true = awaiting deconstruction job
int assignedDeconstructor;     // mover index doing teardown (-1 = none)
```

### Step 2: New job type

**`src/entities/jobs.h`** — add to JobType enum (before `JOBTYPE_COUNT`):
```c
JOBTYPE_DECONSTRUCT_WORKSHOP,
```

### Step 3: Job driver — `RunJob_DeconstructWorkshop`

**`src/entities/jobs.c`** — new function, follows `RunJob_Build` pattern:

Two steps:
1. **STEP_MOVING_TO_WORK**: Walk to workshop work tile. Check arrival (on or adjacent). Fail if stuck.
2. **STEP_WORKING**: Progress timer = half the original build time (from construction recipe). On completion:
   - Look up the construction recipe for this workshop type (map WorkshopType → ConstructionRecipeIndex)
   - For each input in each stage: roll `CONSTRUCTION_REFUND_CHANCE` (75%) per item, spawn refunded items at work tile or adjacent walkable cell
   - Call `DeleteWorkshop()` (improved version, see Step 5)
   - EventLog the deconstruction
   - Return JOBRUN_DONE

Register in `jobRunners[]` dispatch table:
```c
[JOBTYPE_DECONSTRUCT_WORKSHOP] = RunJob_DeconstructWorkshop,
```

**Build time lookup**: Need a helper `GetConstructionRecipeForWorkshopType(WorkshopType type)` that maps e.g. `WORKSHOP_CAMPFIRE` → `CONSTRUCTION_WORKSHOP_CAMPFIRE`. This mapping already exists implicitly in `input.c:ExecutePlaceWorkshopBlueprint` — extract to a shared helper in `construction.h/c`.

### Step 4: WorkGiver — `WorkGiver_DeconstructWorkshop`

**`src/entities/jobs.c`** — new function:

```
For each active workshop with markedForDeconstruct && assignedDeconstructor < 0:
  1. Check mover capability (canBuild)
  2. Pathfind to workshop work tile
  3. Create JOBTYPE_DECONSTRUCT_WORKSHOP job
     - job.targetWorkshop = wsIdx
     - job.workRequired = buildTime * 0.5
     - goal = workTileX, workTileY
  4. Set ws->assignedDeconstructor = moverIdx
  5. Return job ID
```

**Call site**: Add to `AssignJobs_P4_Designations()`, after blueprint work but before the free() call. Check if any workshops are marked, then iterate idle movers calling `WorkGiver_DeconstructWorkshop`.

### Step 5: Improve `DeleteWorkshop()`

**`src/entities/workshops.c`** — add job cancellation to existing function:

Before setting `active = false`, iterate all active jobs and cancel any that target this workshop:
```c
// Cancel all jobs targeting this workshop
for (int i = activeJobCount - 1; i >= 0; i--) {
    Job* job = &jobs[activeJobList[i]];
    if (job->targetWorkshop == index) {
        CancelJob(&movers[job->assignedMover], job->assignedMover);
    }
}
```

This fixes a pre-existing issue regardless of deconstruction.

### Step 6: D key behavior change

**`src/core/input.c:1897`** — change the D key handler.

The handler is inside `hoveredWorkshop >= 0 && inputMode == MODE_NORMAL`. Since MODE_DRAW uses D to toggle draw mode on/off (line 2094/2103), we can't reuse D for instant-delete in MODE_DRAW. That's fine — MODE_DRAW workshop deletion doesn't exist as a keybind anyway.

Replace the instant delete with mark-for-deconstruct:

```c
if (IsKeyPressed(KEY_D)) {
    Workshop* ws = &workshops[hoveredWorkshop];
    if (ws->markedForDeconstruct) {
        ws->markedForDeconstruct = false;
        if (ws->assignedDeconstructor >= 0) {
            // Cancel in-progress deconstruct job
            // (scan jobs for JOBTYPE_DECONSTRUCT_WORKSHOP targeting this workshop)
        }
        ws->assignedDeconstructor = -1;
        AddMessage("Deconstruction cancelled", YELLOW);
    } else {
        ws->markedForDeconstruct = true;
        AddMessage("Marked for deconstruction", ORANGE);
    }
    hoveredWorkshop = -1;
    return;
}
```

### Step 7: Cancel active craft/deliver jobs when marking

When a workshop is marked for deconstruction, cancel all active craft/deliver/ignite jobs targeting it. This goes in the mark-for-deconstruct handler (Step 6), before setting the flag:
```c
// Cancel all active jobs at this workshop
for (int i = activeJobCount - 1; i >= 0; i--) {
    Job* job = &jobs[activeJobList[i]];
    if (job->targetWorkshop == hoveredWorkshop) {
        CancelJob(&movers[job->assignedMover], job->assignedMover);
    }
}
```

### Step 8: Rendering — red tint for marked workshops

**`src/render/rendering.c`** — in the workshop rendering loop, check `ws->markedForDeconstruct`. If true, apply a red tint overlay (similar to how blueprint ghost rendering works but with red color and ~0.3 alpha).

**`src/render/tooltips.c`** — show "Marked for deconstruction" in workshop tooltip when flagged.

### Step 9: Save/load

**`src/core/save_migrations.h`**:
- Bump `CURRENT_SAVE_VERSION` from 63 → 64
- Add `WorkshopV63` legacy struct (current Workshop layout without markedForDeconstruct/assignedDeconstructor)

**`src/core/saveload.c`**:
- Version >= 64: read new Workshop fields
- Version < 64: read old layout, set `markedForDeconstruct = false`, `assignedDeconstructor = -1`

**`src/core/inspect.c`**:
- Parallel migration

### Step 10: CancelJob cleanup for deconstruct jobs

When a deconstruct job is cancelled (mover dies, player cancels), reset `ws->assignedDeconstructor = -1`. This goes in the CancelJob function — add a case for `JOBTYPE_DECONSTRUCT_WORKSHOP`:
```c
if (job->type == JOBTYPE_DECONSTRUCT_WORKSHOP && job->targetWorkshop >= 0) {
    workshops[job->targetWorkshop].assignedDeconstructor = -1;
}
```

---

## Files to modify

| File | Changes |
|------|---------|
| `src/entities/workshops.h` | Add `markedForDeconstruct`, `assignedDeconstructor` to Workshop struct |
| `src/entities/workshops.c` | Improve `DeleteWorkshop()` with job cancellation |
| `src/entities/jobs.h` | Add `JOBTYPE_DECONSTRUCT_WORKSHOP` to enum |
| `src/entities/jobs.c` | `RunJob_DeconstructWorkshop`, `WorkGiver_DeconstructWorkshop`, register in dispatch table, add to `AssignJobs_P4`, CancelJob cleanup |
| `src/world/construction.h` | Declare `GetConstructionRecipeForWorkshopType()` |
| `src/world/construction.c` | Implement `GetConstructionRecipeForWorkshopType()` mapping |
| `src/core/input.c` | D key: mark for deconstruct + cancel active jobs (replace instant delete) |
| `src/render/rendering.c` | Red tint overlay for marked workshops |
| `src/render/tooltips.c` | "Marked for deconstruction" text |
| `src/core/save_migrations.h` | Version 63→64, WorkshopV63 struct |
| `src/core/saveload.c` | Workshop migration |
| `src/core/inspect.c` | Parallel workshop migration |

---

## Verification

1. `make path` — builds clean
2. `make test` — all tests pass
3. Manual testing:
   - Hover workshop in MODE_NORMAL, press D → shows "Marked for deconstruction", red tint
   - Mover walks to workshop, tears it down, materials spawn at ~75% rate
   - Press D again on marked workshop → cancels deconstruction
   - Active craft jobs at workshop get cancelled when marked
   - Save/load with marked workshops preserves state
   - Load old v63 save — workshops load normally (markedForDeconstruct=false)
4. Edge cases:
   - Mark workshop during active craft → craft job cancelled, then deconstruct job assigned
   - Mover dies during deconstruction → assignedDeconstructor reset, another mover picks up
   - Multiple workshops marked → each gets its own deconstructor
