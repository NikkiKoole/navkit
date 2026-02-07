# Outcome: src/world/grid.c Assumption Audit

Date: 2026-02-07
Source: docs/todo/audits/grid.md

## Goals
- Prevent ramp/ladder state drift when cells are overwritten.
- Ensure terrain mutations keep pathing data consistent.
- Remove correctness hazards from fire/trunk removal.

## Non-Goals
- Redesigning the pathfinding or chunk system.
- Refactoring all terrain mutation sites in one pass.
- Changing ramp semantics beyond fixing clear inconsistencies.

## Triage

### High (must fix first)
1. **Finding 5: Fire burns solid support without ramp validation**
   - After a fire burn removes a CF_SOLID cell, call `ValidateAndCleanupRamps()` around the affected area.
   - Expected outcome: no ramps leading to unsupported exits after burn events.

### Medium (3-7 days, moderate risk)
1. **Findings 1/3/4/7: Overwrite without cleanup**
   - Add a shared `ClearCellCleanup(x, y, z)` helper that erases ramps/ladders before overwriting.
   - Use it in `PlaceCellFull`, blueprint wall/floor completion, `PlaceLadder`, and quick‑edit erase.
   - Expected outcome: rampCount stays accurate; ladder chains are recalculated.

2. **Finding 8: EraseRamp does not dirty exit chunk**
   - Mark both the ramp chunk and the exit chunk dirty (z+1, exit tile).
   - Expected outcome: HPA graph updates correctly at chunk boundaries.

### Low (polish / consistency)
1. **Finding 9: CanPlaceRamp allows map‑edge placement with no entry**
   - Return false if low‑side entry is out of bounds.

2. **Finding 10: IsRampStillValid ignores low‑side accessibility**
   - Consider adding a low‑side walkability check or a visual warning.

3. **Finding 11: InitGridWithSizeAndChunkSize missing hpaNeedsRebuild**
   - Set `hpaNeedsRebuild = true;` alongside the other flags.

4. **Finding 12: Entrance scan past grid bounds**
   - Clamp the inner loop to avoid scanning beyond `gridWidth`.

## Proposed Plan of Attack

### Phase 0: Correctness fixes
- Add ramp validation on fire burnouts.

### Phase 1: Cleanup-before-overwrite pattern
- Implement `ClearCellCleanup` helper.
- Wire it into all cell overwrite paths (PlaceCellFull, blueprints, PlaceLadder, quick-edit erase).

### Phase 2: Pathing consistency
- Dirty exit chunks when ramps are erased/invalidated.
- Validate trunk removal paths where CF_SOLID is removed.

### Phase 3: Low-risk consistency
- Clamp entrance scanning.
- Map‑edge ramp placement checks.
- Add missing `hpaNeedsRebuild` flag.
- Optional low‑side checks for ramp validity.

## Risks and Mitigations
- **Behavior drift**: Keep the cleanup helper minimal (only ramp/ladder erase) to avoid unexpected side effects.
- **Performance**: Cleanup helper is constant time; no significant impact expected.
- **Regression risk**: Add focused tests around ramp erase and fire burn to verify paths update.

## Validation Plan
- Build a repro map with ramps crossing chunk boundaries; erase ramp and confirm HPA graph updates.
- Burn a tree trunk supporting a ramp exit; confirm the ramp is removed or invalidated.
- Overwrite ramps/ladders via wall placement and blueprints; confirm counts and ladder chains remain correct.

## Open Questions
- Should low‑side inaccessibility invalidate ramps or merely mark them as one‑way?
- Should quick‑edit erase also cancel designations, stockpiles, or workshops in the same cell?
