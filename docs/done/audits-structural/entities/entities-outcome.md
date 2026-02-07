# Outcome: src/entities/ Structural Audit

Date: 2026-02-07
Source: docs/todo/audits/structure-entities.md

## Goals
- Provide a concrete plan to reduce coordination overhead and bug risk in `src/entities/`.
- Triage the findings into easy, medium, hard, and not-worth-doing-now items.
- Define a phased plan that can be executed incrementally without destabilizing saves.

## Non-Goals
- Rewriting the entire entity system or removing global arrays in one step.
- Changing gameplay behavior or balancing job priorities beyond what is necessary for refactors.
- Introducing new save formats in the same pass as large architectural refactors.

## Triage

### Easy (1-3 days each, low risk)
1. **Pattern 5: RemoveBill reimplements CancelJob**
   - Fix: expose a shared cancellation path so `RemoveBill()` can call it, or extract a `CancelJobInternal()` helper that both use.
   - Also fix the missing `targetItem` release and align safe-drop behavior with `CancelJob()`.
   - Expected outcome: eliminate a known reservation leak and remove a fragile copy.

2. **Pattern 4: Safe-drop logic duplicated**
   - Fix: extract a helper (e.g., `SafeDropItemNearMover(moverId, itemIdx)`) and use it for both `carryingItem` and `fuelItem`.
   - Expected outcome: one authoritative drop policy, no divergence.

3. **Pattern 3: Stockpile slot cleanup duplicated**
   - Fix: move slot-removal to a single public function in `stockpiles.c` and use it in jobs, including craft pickup phases.
   - Expected outcome: single place to update slot fields when new data is added.

4. **Pattern 12: jobDrivers[] coverage**
   - Fix: add `_Static_assert(ARRAY_LEN(jobDrivers) == JOBTYPE_COUNT)`.
   - Expected outcome: compile-time guard against missing drivers.

5. **Pattern 13: DefaultMaterialForItemType coverage**
   - Fix: in debug builds, add a warning when `default` returns `MAT_NONE` for an item that is expected to be material-backed.
   - Expected outcome: earlier detection of missing cases without changing behavior.

### Medium (3-10 days, moderate risk)
1. **Pattern 6: New WorkshopType requires many changes**
   - Fix: introduce a `WorkshopDef` table in `workshops.c` containing template, recipe list, optional render data, and name.
   - Update `GetRecipesForWorkshop()` and creation UI to read from the table.
   - Expected outcome: fewer manual edits when adding a workshop, reduced chance of missing templates.

2. **Pattern 1 + Pattern 15: DesignationType fan-out in jobs.c**
   - Fix: create a `DesignationJobSpec` table mapping designation to job type, cache rebuild function, and workgiver function.
   - Convert `AssignJobs()` to iterate the table instead of open-coded `if` ladder.
   - Expected outcome: adding a designation becomes a single-table edit plus the driver/WorkGiver.

3. **Pattern 7: saveload.c and inspect.c parallel migrations**
   - Fix: move legacy struct definitions and version constants into a shared header (e.g., `src/core/save_migrations.h`).
   - Both saveload and inspect include the same definitions, preventing version drift.
   - Expected outcome: single source of truth for legacy struct layouts.

4. **Pattern 10: Stockpile slot state contract**
   - Fix: introduce a `StockpileSlot` struct or helper APIs that update `slots`, `slotCounts`, `slotTypes`, `slotMaterials` together.
   - This can be incremental by adding helpers and routing new code through them.
   - Expected outcome: fewer partial updates and easier future extensions.

### Hard (multi-week or higher migration risk)
1. **Pattern 2: ItemType addition requires save migration**
   - Root cause: `Stockpile.allowedTypes[ITEM_TYPE_COUNT]` is embedded in the saved struct.
   - Options:
     - Change save format to store `allowedTypes` as a variable-length blob with explicit count.
     - Replace array with a fixed-size bitset that does not change with enum growth, with a remap layer.
     - Move `allowedTypes` into a parallel array serialized separately from `Stockpile`.
   - This is the highest-impact fix but touches save format and migration code.

2. **Pattern 9: Direct mutation of `items[]` from many files**
   - Fix requires enforcing item mutations through an API (or event system) and auditing all call sites.
   - High risk because it spans simulation, rendering, and core save logic.

3. **Pattern 11: ClearMovers cross-system reset**
   - Fix suggests a unified world reset orchestrator or a dependency registry of cleanup hooks.
   - High risk, touches multiple systems and save/load paths.

### Defer / Not Worth Doing Now
- **Pattern 14: Repeated move-to-target sequences**
  - These sequences have meaningful per-job differences. Extracting them risks removing needed variation.
  - Defer unless the behavior needs to change globally.

- **Pattern 15 (if Table-driven DesignationSpec is adopted)**
  - If we do Pattern 1, Pattern 15 is naturally solved; otherwise, the existing ladder is readable enough.

## Proposed Plan of Attack

### Phase 0: Safety fixes and quick wins
- Implement shared `CancelJob` logic and update `RemoveBill()` to call it.
- Extract `SafeDropItemNearMover()` and reuse it for `carryingItem` and `fuelItem`.
- Consolidate stockpile slot removal to a single public helper and update all call sites.
- Add `_Static_assert` for `jobDrivers[]` coverage.
- Add debug warning for `DefaultMaterialForItemType` fallthrough.

### Phase 1: Structural tables
- Add `WorkshopDef` table and replace `GetRecipesForWorkshop()` switch.
- Add `DesignationJobSpec` table and replace the `AssignJobs()` ladder.

### Phase 2: Save migration unification
- Move version constants and legacy struct definitions into a shared header.
- Update both saveload and inspect to consume the shared definitions.

### Phase 3: High-impact migration (optional)
- Choose a path to decouple `ItemType` growth from stockpile save layout.
- Implement migration and compatibility tests before enabling.

## Risks and Mitigations
- **Save format changes**: isolate into Phase 3, add versioned migrations with explicit tests.
- **Behavioral drift in job assignment**: in Phase 1, preserve the existing priority order by encoding it in the table in the same order as today.
- **Refactor scope creep**: keep changes in small phases with limited files per PR.

## Validation Plan
- Add a debug-only check that `CancelJob()` releases all reserved fields for a job (assert or log in debug builds).
- Add a quick save/load round-trip test for entities after any migration changes.
- During Phase 0, create a repro test for the `RemoveBill` targetItem reservation leak and verify the fix.

## Open Questions
- Do we want to expose `CancelJob()` publicly in `jobs.h`, or keep it internal and add a narrower helper for `RemoveBill()`?
- Should `DesignationJobSpec` include priority values or be ordered implicitly by the array?
- If we pursue Phase 3, which migration path is preferred for `Stockpile.allowedTypes`?
