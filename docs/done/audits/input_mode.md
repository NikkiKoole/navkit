# Assumption Audit: src/core/input_mode.c (and src/core/input.c)

Date: 2026-02-07

Scope: input_mode.c state management, input.c consumption of that state, drag lifecycle, pending key system, mode transitions.

---

## Finding 1: Drag stuck forever when mouse released over UI

- **What**: The drag lifecycle (start at line 1860, end at line 1899 of input.c) is gated behind `if (ui_wants_mouse()) return;` at line 1855. The mouse-release check that clears `isDragging` is below that guard.
- **Assumption**: The mouse button release will always be detected on the game grid, not over UI.
- **How it breaks**: Player starts a drag on the grid (left-click to place walls). While holding the mouse button, they move the cursor over the bottom bar or any UI element. They release the mouse button while hovering UI. `ui_wants_mouse()` returns true, so `HandleInput` returns before reaching the `IsMouseButtonReleased` check. `isDragging` remains `true` permanently.
- **Player impact**: A green drag-preview rectangle is permanently stuck on screen stretching from the original drag start to the current cursor position. More critically, `InputMode_Back()` and `InputMode_ExitToNormal()` both early-return when `isDragging` is true, meaning ESC and mode-key navigation are **completely blocked**. The player is soft-locked into their current mode and action. The only escape is clicking on the grid again (which starts a new drag that can end), but this is not discoverable.
- **Severity**: **HIGH**
- **Suggested fix**: Move the drag-end check (`IsMouseButtonReleased`) above the `ui_wants_mouse()` guard, or at minimum always clear `isDragging` when `IsMouseButtonReleased` is detected regardless of UI hover state. The drag action execution itself can remain gated.

---

## Finding 2: Bar buttons "dIrt" (KEY_I) and "rocK" (KEY_K) do nothing

- **What**: `InputMode_GetBarItems()` in input_mode.c (lines 200-201) registers two clickable buttons for the DRAW mode menu: `"dIrt"` mapped to `KEY_I` and `"rocK"` mapped to `KEY_K`. When clicked, these call `InputMode_TriggerKey(KEY_I)` / `InputMode_TriggerKey(KEY_K)`, setting `pendingKey`.
- **Assumption**: Every key shown in a bar button has a corresponding `CheckKey()` handler in input.c's action selection logic.
- **How it breaks**: In the MODE_DRAW action selection (input.c lines 1639-1649), there is no `CheckKey(KEY_I)` or `CheckKey(KEY_K)`. The pending key is loaded into `currentPendingKey` but never consumed by any `CheckKey` call in the DRAW menu path. Furthermore, there are no `ACTION_DRAW_DIRT` or `ACTION_DRAW_ROCK` enum values to transition to -- these actions simply don't exist.
- **Player impact**: Player clicks the "dIrt" or "rocK" button in the bottom bar. Nothing happens. No feedback. The button appears interactive but is dead. The `currentPendingKey` persists as a stale value until the next frame (see Finding 3).
- **Severity**: **MEDIUM** (UI promises functionality that doesn't exist)
- **Suggested fix**: Either (a) remove the "dIrt" and "rocK" bar items since direct dirt/rock are available via the soil submenu already, or (b) add `ACTION_DRAW_DIRT` / `ACTION_DRAW_ROCK` actions and wire them up. Option (a) is consistent with the MEMORY.md note about removing top-level duplicates.

---

## Finding 3: Stale pendingKey persists across frames

- **What**: `currentPendingKey` (input.c line 1238) is a static variable. It's set from `InputMode_GetPendingKey()` at the start of `HandleInput()`. It's only cleared when a `CheckKey()` call matches it. If no `CheckKey()` matches (see Finding 2), the stale key persists.
- **Assumption**: Every pending key will be consumed within the same frame it's set.
- **How it breaks**: (1) Player clicks "dIrt" button (KEY_I) -- not consumed. Next frame, if no new pending key arrives, `currentPendingKey` remains KEY_I. It sits inert since nothing checks KEY_I in the current code paths. (2) More dangerously: if a pending key matches a key used in a *different* mode/state than the one the player was in when they clicked, it could fire in the wrong context. Example: player clicks a bar button, then rapidly presses ESC to back out before the frame processes. The pending key fires in the parent menu context instead of the intended sub-menu.
- **Player impact**: In the current codebase, primarily manifests as the dead buttons from Finding 2. But it's a latent bug that could cause wrong-context key activation if new bar items are added.
- **Severity**: **LOW** (latent, currently masked)
- **Suggested fix**: Clear `currentPendingKey` at the end of `HandleInput()` unconditionally, or at least at the start of each frame before loading a new one: `currentPendingKey = 0;` before `if (pending != 0) currentPendingKey = pending;`.

---

## Finding 4: selectedRampDirection not reset on mode exit

- **What**: `selectedRampDirection` (defined in input.c line 215) is set when the player manually picks a ramp direction (N/E/S/W) during `ACTION_DRAW_RAMP`. It's never reset by `InputMode_Back()`, `InputMode_ExitToNormal()`, or `InputMode_Reset()`.
- **Assumption**: Each time the player enters ramp mode, they want the same direction as last time.
- **How it breaks**: Player enters DRAW > Ramp, selects "East", places some ramps, then ESCs out. Later they enter DRAW > Ramp again expecting auto-detect behavior (the default). Instead, all ramps are forced East. The bar does show the current selection, but if the player doesn't notice the highlight, they'll get unexpected ramp directions.
- **Player impact**: Ramps placed in wrong direction. Player confusion about why auto-detect stopped working. They have to manually press 'A' to reset.
- **Severity**: **MEDIUM**
- **Suggested fix**: Reset `selectedRampDirection = CELL_AIR` in `InputMode_Back()` when clearing `inputAction` from `ACTION_DRAW_RAMP`, or in `InputMode_Reset()`.

---

## Finding 5: selectedBuildMaterial never reset on mode exit

- **What**: `selectedBuildMaterial` (input.c line 23) is cycled with 'M' during build actions (WORK > BUILD > Wall/Floor/Ladder/Ramp). It's `static` and persists for the entire session. Neither `InputMode_Reset()`, `InputMode_Back()`, nor `InputMode_ExitToNormal()` touch it.
- **Assumption**: Build material is a session-wide preference the player always wants preserved.
- **How it breaks**: Player sets build material to "Oak" for a wall job. They exit, do other things, come back to WORK > BUILD > Floor. The material is still "Oak" from before. This is arguably intentional (comment says "persists during session"), but it creates an asymmetry: `selectedMaterial` (for DRAW wall/floor) IS reset to 1 by `InputMode_Back()`, while `selectedBuildMaterial` (for WORK build) is NOT. Two material-selection systems with different persistence semantics for the same conceptual operation (picking a material).
- **Player impact**: Minor confusion. The bar does show the current material, so it's visible. But the inconsistency between DRAW material reset and WORK material persistence could trip up players who switch between the two systems.
- **Severity**: **LOW**
- **Suggested fix**: Either make both persist (remove the `selectedMaterial = 1` reset from `InputMode_Back`) or make both reset. The current asymmetry is the worst option.

---

## Finding 6: Stockpile priority keys don't early-return, potentially double-firing

- **What**: Stockpile hover controls (input.c lines 1341-1346) for `KEY_EQUAL`/`KEY_MINUS` (priority +/-) use `IsKeyPressed()` but do NOT `return` after handling. The `KEY_RIGHT_BRACKET`/`KEY_LEFT_BRACKET` (stack size) DO `return`.
- **Assumption**: The `+`/`-` keys aren't used by anything downstream, so not returning is safe.
- **How it breaks**: Currently benign since `+`/`-` aren't mode keys. But raylib's `IsKeyPressed()` is consumed-on-read, so the key won't fire again downstream anyway. The real issue is inconsistency: if `=`/`-` keys are ever added as mode shortcuts, the stockpile handler would still fire first without returning.
- **Player impact**: None currently. Maintenance hazard.
- **Severity**: **LOW**
- **Suggested fix**: Add `return;` after the priority change handlers for consistency with the stack size handlers.

---

## Finding 7: Z-level can change mid-drag

- **What**: The Z-level keys (`.` and `,` for `currentViewZ`) are processed at line ~1522 of input.c, which runs before the drag handling section. There is no guard against changing Z while `isDragging` is true.
- **Assumption**: The player won't change Z-level during a drag operation.
- **How it breaks**: Player starts dragging to designate mining on Z=5. While holding the mouse button, they press `.` to go to Z=6. The drag completes on Z=6 with the original start coordinates from Z=5. The designation is applied to Z=6 at coordinates that were chosen while looking at Z=5.
- **Player impact**: Designations, wall placements, stockpiles, etc. get applied to wrong Z-level. For DRAW operations this is immediately visible and confusing. For WORK designations it could be invisible (designating mining on an air layer).
- **Severity**: **MEDIUM**
- **Suggested fix**: Either block Z-level changes while `isDragging`, or capture `dragStartZ` alongside `dragStartX`/`dragStartY` and use it for execution instead of `currentViewZ`.

---

## Finding 8: showQuitConfirm not cleared on mode changes

- **What**: `showQuitConfirm` is set to `true` when ESC is pressed at root (MODE_NORMAL, no action). It's dismissed by "any other key" (input.c line 1563). However, it's never cleared by `InputMode_Reset()` or `InputMode_ExitToNormal()`.
- **Assumption**: The quit confirm dialog and mode system are independent.
- **How it breaks**: This is currently fine because quit confirm can only be shown at MODE_NORMAL root, and any key dismisses it. However, if `InputMode_Reset()` is called externally (e.g., after loading a save), `showQuitConfirm` could theoretically remain `true` from a prior state.
- **Player impact**: After a save load or external reset, the quit confirmation overlay might be visible without the player having pressed ESC. Minor since it's dismissed by any key.
- **Severity**: **LOW**
- **Suggested fix**: Clear `showQuitConfirm = false` in `InputMode_Reset()`.

---

## Finding 9: Workshop/stockpile hover keys fire in non-NORMAL modes (partial)

- **What**: Stockpile hover controls for priority (`+`/`-`) and stack size (`[`/`]`) run for ANY `inputMode` (no mode check). Only the filter toggles are gated behind `if (inputMode == MODE_NORMAL)`. Workshop hover controls are correctly gated behind `inputMode == MODE_NORMAL`.
- **Assumption**: Priority and stack size keys don't conflict with other modes.
- **How it breaks**: Player is in MODE_DRAW or MODE_WORK, cursor happens to be over a stockpile. They press `]` or `[` to... well, these keys aren't used by other modes currently. But `return` is called, which means the mode-specific key handling below never runs. If the player is hovering a stockpile and presses `]`, nothing visible happens in their current mode context, and the frame is consumed.
- **Player impact**: Mysterious "nothing happened" when pressing keys while hovering a stockpile in non-NORMAL modes. The stack size changes silently in the background.
- **Severity**: **LOW**
- **Suggested fix**: Gate priority and stack size controls behind `inputMode == MODE_NORMAL` like the filter toggles.

---

## Summary

| # | Finding | Severity |
|---|---------|----------|
| 1 | Drag stuck forever when mouse released over UI | **HIGH** |
| 2 | Bar buttons "dIrt" and "rocK" do nothing | MEDIUM |
| 3 | Stale pendingKey persists across frames | LOW |
| 4 | selectedRampDirection not reset on mode exit | MEDIUM |
| 5 | selectedBuildMaterial asymmetric persistence | LOW |
| 6 | Stockpile priority keys missing early-return | LOW |
| 7 | Z-level can change mid-drag | MEDIUM |
| 8 | showQuitConfirm not cleared on reset | LOW |
| 9 | Stockpile hover keys fire in non-NORMAL modes | LOW |
