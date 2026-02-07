# Outcome: src/core/input.c Assumption Audit

Date: 2026-02-07
Source: docs/todo/audits/input.md

## Goals
- Eliminate correctness bugs in input-driven terrain edits and drag actions.
- Prevent crashes or undefined behavior from out-of-bounds mouse drags.
- Align sandbox/edit behaviors with player expectations.

## Non-Goals
- Large refactors of the input system or mode architecture.
- Changing gameplay rules beyond fixing incorrect or unsafe behavior.
- Introducing new tools, UI, or additional input modes.

## Triage

### Easy (1-3 days each, low risk)
1. **Finding 1: Pile-drag material mismatch**
   - Fix continuous pile-drag to use correct materials for clay/gravel/sand/peat.
   - Expected outcome: consistent wall material metadata across drag paths.

2. **Finding 2: Erase skips rampCount updates**
   - Detect ramps in `ExecuteErase` and call `EraseRamp()`.
   - Expected outcome: rampCount stays accurate and any dependent logic remains correct.

3. **Finding 7: RemoveTree leaves chop/gather designations**
   - Call `CancelDesignation()` on removed tree cells.
   - Expected outcome: no ghost designations on air cells.

4. **Finding 8: LoadWorld does not reset input state**
   - Call `InputMode_Reset()` after successful load.
   - Expected outcome: no stale drag/action state after loading.

5. **Finding 10: Quick-edit erase leaves stale metadata**
   - Use a shared cell-clear helper to reset materials/flags/surface.
   - Expected outcome: metadata always matches cell type.

6. **Finding 11: Repath guard uses cumulative count**
   - Switch to per-cell change detection when deciding to scan movers.
   - Expected outcome: avoids unnecessary mover repath scans on large drags.

### Medium (3-7 days, moderate risk)
1. **Finding 6: ExecuteErase ignores designations/stockpiles/workshops**
   - Cancel designations on erased cells.
   - Consider removing stockpile cells in erased regions (or blocking erase on workshop tiles).
   - Expected outcome: no ghost designations or stockpile inconsistencies.

2. **Finding 9: Stockpile/workshop key conflicts on overlap**
   - Decide a precedence rule when both are hovered (likely workshop bill keys first).
   - Expected outcome: consistent, predictable key behavior.

### High (must fix first)
1. **Finding 5: ExecutePileSoil missing bounds checks**
   - Add bounds checks for x/y/z in `ExecutePileSoil` and the continuous drag path.
   - Expected outcome: prevent OOB memory access and potential crashes.

2. **Finding 3: Soil placement missing mover repathing**
   - Add mover repath invalidation to `ExecuteBuildSoil` and `ExecutePileSoil`.
   - Expected outcome: movers do not walk into newly solid terrain.

3. **Finding 4: ExecutePlaceGrass converts air to solid without full setup**
   - Use a proper placement path that sets wall material/flags and repaths.
   - Expected outcome: no hidden solid dirt cells with MAT_NONE, fewer stuck movers.

## Proposed Plan of Attack

### Phase 0: Correctness and crash prevention
- Add bounds checks for pile-drag coordinates.
- Ensure soil placement triggers mover repathing.
- Fix grass placement to use a full cell placement helper with proper metadata.

### Phase 1: Consistency cleanup
- Fix material mismatch in continuous pile-drag.
- Ensure ramp erase uses `EraseRamp()`.
- Clear designations on tree removal.
- Reset input state after LoadWorld.

### Phase 2: Structural consistency
- Ensure ExecuteErase cancels designations and removes/blocks stockpile/workshop tiles.
- Define consistent precedence for overlapping stockpile/workshop hover keys.
- Add shared cell-clear helper for quick-edit erase.
- Improve repath guard to avoid extra mover scans.

## Risks and Mitigations
- **Behavioral drift**: Use minimal changes scoped to the affected tools and paths.
- **Performance regressions**: Move expensive work behind per-cell change checks.
- **UI regressions**: Keep key precedence explicit and documented.

## Validation Plan
- Add a regression test or manual repro for each fixed behavior:
  - Drag pile soil past map edge (should not crash).
  - Place soil or grass while movers are active (no stuck movers).
  - Erase ramps and verify `rampCount` and ramps remain consistent.
  - Load a save mid-drag and confirm no stale drag actions.

## Open Questions
- Should ExecuteErase remove stockpile cells automatically, or block erase when a stockpile is present?
- When a stockpile overlaps a workshop, should workshop bill keys always win or require a modifier?
- Do we want a single shared helper for full cell placement to reduce future drift?
