# Outcome: src/core/input_mode.c Assumption Audit

Date: 2026-02-07
Source: docs/todo/audits/input_mode.md

## Goals
- Eliminate state‑machine traps and UI dead‑ends in input handling.
- Make drag lifecycle robust across UI hover and view changes.
- Provide a practical testing path for input issues without full UI automation.

## Non-Goals
- Large refactors of the input architecture or mode registry.
- New UI systems or additional input modes.
- Full end‑to‑end UI automation.

## Triage

### High (must fix first)
1. **Finding 1: Drag stuck forever when mouse released over UI**
   - Ensure drag release is processed even if `ui_wants_mouse()` is true.
   - Expected outcome: no soft‑lock when releasing over UI.

### Medium (3-7 days, moderate risk)
1. **Finding 2: “dIrt” and “rocK” bar buttons do nothing**
   - Remove dead buttons or wire real actions.
   - Expected outcome: bar only shows actionable controls.

2. **Finding 4: selectedRampDirection not reset**
   - Reset on mode exit or action change.
   - Expected outcome: ramp mode behaves predictably when re‑entered.

3. **Finding 7: Z‑level change mid‑drag**
   - Block z‑changes while dragging or capture `dragStartZ` and use it.
   - Expected outcome: drags apply to the intended z‑level.

### Low (polish / consistency)
1. **Finding 3: stale pendingKey persists across frames**
   - Clear pending key each frame (or at least before loading a new one).

2. **Finding 5: build material persistence asymmetry**
   - Decide consistent behavior: both draw/build persist or both reset.

3. **Finding 6: stockpile priority keys missing early‑return**
   - Add `return;` for consistency with stack size handlers.

4. **Finding 8: showQuitConfirm not cleared on reset**
   - Clear in `InputMode_Reset()`.

5. **Finding 9: stockpile hover keys fire in non‑NORMAL modes**
   - Gate priority/stack keys behind `inputMode == MODE_NORMAL`.

## Proposed Plan of Attack

### Phase 0: Correctness fix
- Move drag release handling before `ui_wants_mouse()` so drag state always clears.

### Phase 1: UX‑level correctness
- Remove or wire “dIrt” / “rocK” bar items.
- Reset `selectedRampDirection` on exit.
- Prevent z‑change mid‑drag (block or store dragStartZ).

### Phase 2: Consistency & cleanup
- Clear pending key each frame.
- Normalize build material persistence.
- Add missing early‑return for stockpile priority keys.
- Clear `showQuitConfirm` on reset.
- Gate stockpile hover keys to MODE_NORMAL.

## Testing Strategy (Scripted Input Harness)

These issues are hard to test with pure unit tests because `HandleInput()` reads raylib input directly. The recommended approach is a lightweight scripted‑input harness that stubs raylib input functions and drives `HandleInput()` across frames.

### Option A: Simple unit tests (no raylib input)
Use pure state tests against `input_mode.c`:
- `InputMode_Reset()` clears state (`isDragging`, `inputAction`, `workSubMode`, `selectedMaterial`, `showQuitConfirm` if added).
- `InputMode_Back()` and `InputMode_ExitToNormal()` do nothing while `isDragging` is true.
- `InputMode_GetBarItems()` no longer includes `KEY_I` / `KEY_K` if those buttons are removed.

### Option B: Scripted input integration tests (recommended)
Create a test harness that replaces raylib input calls:

1. **New unity test file** (example: `tests/test_input_unity.c`):
   - Include `tests/test_unity.c` plus `src/core/input_mode.c`, `src/core/pie_menu.c`, `src/core/input.c`.
   - Before includes, `#define` or stub raylib input calls used by input.c:
     - `IsKeyPressed`, `IsMouseButtonPressed`, `IsMouseButtonReleased`, `GetMousePosition` (and `GetMouseX/GetMouseY` if needed).

2. **Scripted input globals**:
   - `static bool test_keys_pressed[512];`
   - `static bool test_mouse_pressed, test_mouse_released;`
   - `static Vector2 test_mouse_pos;`

3. **Per‑frame driver**:
   - Set test inputs → call `HandleInput()` → clear inputs.

### Example test cases enabled by the harness
- **Drag release over UI**: start drag on grid, set `ui_wants_mouse()` to true on release, verify `isDragging` is cleared.
- **Z‑level change mid‑drag**: start drag at z=3, press `.` during drag, verify the placement uses original z or is blocked.
- **PendingKey stale**: trigger a bar button and ensure key doesn’t fire in a later unrelated mode.

### Optional test‑only accessors
Some state in `input.c` is `static` (e.g., `selectedBuildMaterial`). If needed, add `#ifdef TESTING` accessors:
- `Input_Test_GetSelectedBuildMaterial()`
- `Input_Test_SetSelectedBuildMaterial()`

## Risks and Mitigations
- **Test maintenance**: keep harness minimal and only stub functions used by input.c.
- **Behavior drift**: keep tests focused on state changes, not rendering/UI.

## Validation Plan
- Manual repro for Phase 0: drag across UI and release should always clear drag state.
- Automated checks (harness): verify drag state, z‑level behavior, and bar button handling.

## Open Questions
- Should `selectedBuildMaterial` persist across modes, or reset like draw material?
- Do we want `ACTION_DRAW_DIRT/ROCK`, or remove the dead buttons permanently?
