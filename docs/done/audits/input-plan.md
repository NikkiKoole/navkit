# Plan: src/core/input.c Audit Fixes

Source: docs/todo/audits/input.md, docs/todo/audits/input-outcome.md
Date: 2026-02-07

## Goals
- Eliminate correctness bugs in input-driven terrain edits and drag actions.
- Prevent crashes or undefined behavior from out-of-bounds mouse drags.
- Align sandbox/edit behaviors with player expectations.

## Non-Goals
- Large refactors of the input system or mode architecture.
- Changing gameplay rules beyond fixing incorrect or unsafe behavior.
- Introducing new tools, UI, or additional input modes.

---

## Phase 1: Crash prevention and high-severity fixes

### 1a. Finding 5 (HIGH): Bounds check for ExecutePileSoil
- Add bounds check at top of `ExecutePileSoil` for x/y.
- Also clamp coordinates in the continuous pile-drag caller before calling `ExecutePileSoil`.
- **Why first**: Out-of-bounds memory access = potential crash.

### 1b. Extract `InvalidateMoverPathsThroughCell(x, y, z)` helper
- The inline repath pattern (iterate movers, check path points) exists in `ExecuteBuildWall` and `ExecuteBuildRock`.
- Extract into a shared helper in mover.c/mover.h.
- Replace existing inline copies in `ExecuteBuildWall`/`ExecuteBuildRock` with the helper.
- **Why now**: Findings 3, 4, and several others all need this. Do it once.

### 1c. Finding 3 (HIGH): Soil placement missing mover repathing
- Call `InvalidateMoverPathsThroughCell` after each cell placed in `ExecuteBuildSoil`.
- Call `InvalidateMoverPathsThroughCell` after each cell placed in `ExecutePileSoil`.

### 1d. Finding 4 (HIGH): ExecutePlaceGrass air-to-solid without cleanup
- Replace bare `grid[z][dy][dx] = CELL_DIRT` with `PlaceCellFull` using a proper spec (MAT_DIRT, FINISH_ROUGH, wallNatural=true, clearFloor=true).
- Add `InvalidateMoverPathsThroughCell` call for converted cells.

---

## Phase 2: Consistency fixes (easy, low risk)

### 2a. Finding 1 (MEDIUM): Pile-drag wrong material for non-dirt soils
- Change continuous pile-drag cases to pass correct material constants:
  - `ACTION_DRAW_SOIL_CLAY` -> `MAT_CLAY`
  - `ACTION_DRAW_SOIL_GRAVEL` -> `MAT_GRAVEL`
  - `ACTION_DRAW_SOIL_SAND` -> `MAT_SAND`
  - `ACTION_DRAW_SOIL_PEAT` -> `MAT_PEAT`

### 2b. Finding 2 (MEDIUM): ExecuteErase skips rampCount
- In `ExecuteErase`, add `CellIsDirectionalRamp()` check before the generic erase path.
- Call `EraseRamp()` for ramp cells (similar to the existing `EraseLadder()` special case).

### 2c. Finding 7 (MEDIUM): ExecuteRemoveTree leaves chop/gather designations
- Call `CancelDesignation(dx, dy, z)` for each removed tree cell.

### 2d. Finding 8 (MEDIUM): LoadWorld doesn't reset input state
- Call `InputMode_Reset()` after successful `LoadWorld()` (F6 handler).
- Check for any other load paths (e.g. main.c) and add reset there too.

### 2e. Finding 11 (LOW): Repath guard uses cumulative count
- In `ExecuteBuildWall`/`ExecuteBuildRock`, move the mover repath scan inside the per-cell change block (where `count++` happens) instead of checking `count > 0` outside.
- Once the helper from 1b exists, this becomes: call the helper right after each successful placement.

---

## Phase 3: Erase cleanup (moderate risk)

### 3a. Finding 6 (MEDIUM): ExecuteErase ignores designations/stockpiles
- Call `CancelDesignation(dx, dy, z)` for each erased cell.
- **Open question**: Should erase also remove stockpile cells? For now, cancel designations only — stockpile cleanup can be a follow-up if needed.

### 3b. Finding 10 (LOW): Quick-edit erase leaves stale metadata
- Add ramp check (`CellIsDirectionalRamp` -> `EraseRamp`) mirroring Finding 2 fix.
- Clear wall material, wall natural, wall finish, floor flags, surface type after setting CELL_AIR.
- Consider extracting a shared `ClearCellMetadata(x, y, z)` helper if the cleanup list is reused in 3+ places.

---

## Deferred

### Finding 9 (LOW): Stockpile/workshop key conflict on overlap
- Low severity, has workaround (hover a non-overlapping workshop tile).
- Requires a UI design decision: which system gets priority?
- Defer until the overlap case becomes a real player complaint.

---

## Validation
- Write tests (where feasible) for:
  - Pile soil past map edge -> no crash (Finding 5)
  - Place soil/grass while movers are active -> movers repath (Findings 3, 4)
  - Erase ramp -> rampCount correct (Finding 2)
  - Remove tree with chop designation -> designation cleared (Finding 7)
- Manual repro for:
  - Load save mid-drag -> no stale action (Finding 8)
  - Quick-edit erase -> no stale metadata (Finding 10)
  - Stockpile/workshop overlap keys (Finding 9, deferred)

## Open Questions
- Should `ExecuteErase` remove stockpile cells automatically, or just cancel designations?
- Do we want a single `ClearCellMetadata` helper for full cleanup, shared between `ExecuteErase` and quick-edit erase?
