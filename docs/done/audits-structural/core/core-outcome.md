# Outcome: src/core/ Structural Audit

Date: 2026-02-07
Source: docs/todo/audits/structure-core.md

## Goals
- Reduce high-risk parallel update patterns in `src/core/`.
- Fix active correctness issues discovered in the audit.
- Provide a phased plan that can be executed safely and incrementally.

## Non-Goals
- Large-scale architectural rewrite of input, save/load, or simulation systems.
- Changing gameplay behavior or UI flows beyond refactor needs.
- Introducing save-format changes in the same pass as unrelated refactors.

## Triage

### Easy (1-3 days each, low risk)
1. **Pattern 7: cellTypeNames mismatch in inspect.c (active bug)**
   - Fix: Add `CELL_TYPE_COUNT` to the CellType enum and build name arrays sized to that count.
   - Add the missing `CELL_ROCK` entry and ensure the array length matches the enum.
   - Expected outcome: inspector displays correct cell types; no out-of-bounds risk.

2. **Pattern 2: V16/V17 legacy stockpile structs use live ITEM_TYPE_COUNT**
   - Fix: Replace `ITEM_TYPE_COUNT` with `V16_ITEM_TYPE_COUNT` and `V17_ITEM_TYPE_COUNT` in both saveload.c and inspect.c.
   - Expected outcome: legacy save compatibility no longer depends on current enum size.

3. **Pattern 9: Workshop name tables (3 independent copies)**
   - Fix: Move `WorkshopType` display names into a single shared helper or array (e.g., in `workshops.c/h`).
   - Expected outcome: single source of truth used by input, tooltips, and inspect.

4. **Pattern 6: Stockpile filter keybindings vs tooltip display**
   - Fix: Define a shared list of filterable item types (and optional keys) in one file and consume it in both input.c and tooltips.c.
   - Expected outcome: less chance of drift, easier to extend.

### Medium (3-10 days, moderate risk)
1. **Pattern 1: saveload.c / inspect.c parallel migration**
   - Fix: Extract legacy struct definitions and version constants into a shared header (e.g., `save_migrations.h`).
   - Move shared typedefs and constants out of both files so they cannot drift.
   - Expected outcome: single source of truth for legacy layouts.

2. **Pattern 3: Settings save/load 43-entry list**
   - Fix: Group settings into a struct and serialize that struct, or define a shared macro list used by both SaveWorld and LoadWorld.
   - Expected outcome: fewer ordering mistakes, easier to add new settings.

3. **Pattern 4: InputAction dispatch chain**
   - Fix: Create an `InputActionDef` table that includes name, bar text, key binding, and handler references.
   - Drive `GetActionName`, bar text, bar items, and action selection off the table.
   - Expected outcome: adding a new action becomes a single-table change plus any bespoke handler logic.

4. **Pattern 5: Cell placement sequence repetition**
   - Fix: Extract a shared helper for “place wall/soil/rock” that sets wall material/natural/finish and handles floor/water adjustments based on parameters.
   - Expected outcome: consistent behavior across all placement paths, fewer missed updates when new cell properties are added.

5. **Pattern 8: Simulation activity counters scatter**
   - Fix: Centralize counter updates in `sim_manager.c` via functions/macros (e.g., `Sim_IncWater()`, `Sim_DecWater()`).
   - Optionally add debug-only asserts that counters never drop below zero.
   - Expected outcome: clearer contracts for mutation sites and easier auditing.

6. **Pattern 11: Pie menu vs keyboard action-to-mode mapping**
   - Fix: Use the same `InputActionDef` table (or a dedicated mapping table) for pie menu action mapping and keyboard mapping.
   - Expected outcome: no divergence between mouse and keyboard action selection.

### Hard (multi-week or higher migration risk)
1. **Pattern 12: Grid save/load loop repetition**
   - Fix: Build a `GridSaveDef` table that declares grid name, pointer, element size, and version gating.
   - Generate Save/Load loops from that table and mirror in inspect.c.
   - This is a large refactor touching every per-cell grid; higher regression risk.

2. **Pattern 10: Post-load rebuild implicit sequence**
   - Fix: Introduce a post-load registry where each system registers a rebuild callback.
   - Enforce ordering via phases (e.g., “core counts”, “spatial indices”, “pathfinding”).
   - Higher scope: touches multiple systems and needs careful ordering.

### Defer / Not Worth Doing Now
- **Cell placement micro-differences** (Pattern 5) if current behavioral differences are intentional (e.g., water displacement). If those differences are design choices, only refactor after documenting the intended behavior.
- **Full InputAction table refactor** (Pattern 4) if the current dispatch logic remains stable and no new action types are planned soon.

## Proposed Plan of Attack

### Phase 0: Correctness patches
- Fix `cellTypeNames` and `CELL_TYPE_COUNT` mismatch (Pattern 7).
- Fix V16/V17 legacy struct sizes (Pattern 2).

### Phase 1: Small unifications
- Unify workshop names (Pattern 9).
- Unify stockpile filter lists (Pattern 6).
- Add centralized sim activity counter helpers (Pattern 8).

### Phase 2: Structural refactors
- Share save migration definitions between saveload and inspect (Pattern 1).
- Consolidate settings Save/Load into a single shared definition (Pattern 3).
- Introduce InputAction definitions for pie menu + keyboard path (Pattern 4/11).
- Extract cell placement helper with explicit water/finish flags (Pattern 5).

### Phase 3: Optional deep refactors
- Table-driven grid save/load definitions (Pattern 12).
- Post-load rebuild registry with phased ordering (Pattern 10).

## Risks and Mitigations
- **Save compatibility regressions**: isolate migration refactors and add targeted tests.
- **UI behavior drift**: preserve current action ordering and shortcuts when moving to tables.
- **Water/cell placement behavior changes**: explicitly encode intended behavior in helper parameters and add comments.

## Validation Plan
- Add a unit-like test harness or debug mode that loads known saves across versions and checks key counts (itemCount, stockpileCount, etc.).
- Add a test save with all cell types to verify inspect output aligns with enum names.
- Add a verification pass after input refactors to ensure bar text, pie menu, and key mappings are identical to current behavior.

## Open Questions
- Do we want `CELL_TYPE_COUNT` added to the enum, or a separate constant in grid.h?
- For settings serialization, do we prefer a flat struct (simpler) or a macro list (more flexible)?
- If we centralize InputAction, should handlers live in input.c or be function pointers in the action table?
