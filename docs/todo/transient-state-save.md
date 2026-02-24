# Transient State: Save/Load Gaps

> Notes on runtime state that doesn't persist across save/load.
> Low priority — batch fix when it becomes annoying.
> Updated 2026-02-24 after investigation.

---

## Actually Lost (player-visible)

### Designation Progress
- `Designation.progress` is reset on load (cleared by `ClearMovers()`)
- Impact: active mining/digging jobs restart from 0% on load
- For short jobs (gather grass, dig roots) barely noticeable; for long mining on hard rock, annoying
- Fix: save `progress` field per designation. Simple — add to designation save/load block.

### Event Log
- Ring buffer (4096 entries) is not saved
- F5 auto-dumps to `navkit_events.log` file, but in-memory history is gone on load
- Impact: debug trail lost. Low priority — file dump covers the debugging use case.

---

## Verified as Saved (no action needed)

These were previously thought to be lost but are actually persisted:

| What | How |
|------|-----|
| Job progress (`Job.progress`) | Full `Job` struct is `fwrite`'d — progress survives save/load |
| Hunt designations (`markedForHunt`, `reservedByHunter`) | Saved as part of `Animal` struct |

---

## Intentionally Not Saved (correct behavior)

These are **cleared on load by design** and re-established by the assignment system next tick:

| What | Why it's fine |
|------|--------------|
| `Designation.assignedMover` | Cleared by `ClearMovers()`, re-assigned by WorkGivers next tick |
| Item reservations (`reservedBy`) | Re-assigned by WorkGivers on next job cycle |
| Stockpile slot reservations | Re-assigned on next haul assignment |
| Designation caches (chop, mine, etc.) | Rebuilt from designation grid, O(n) |
| Mover idle list | Rebuilt on first assignment phase |
| Pathfinding graph (HPA* entrances/edges) | Rebuilt from grid, chunks marked dirty |
| Spatial grids (mover, item) | Rebuilt in O(n) on load |
| Lighting grid | Recomputed from light sources on load |
| Furniture move cost grid | Recomputed from furniture positions |
| Entity counters | Recomputed by scanning arrays |
| Job free list | Reconstructed from gaps in job array |
| Active designation count | Scanned during designation load |

---

## When to Fix

Designation progress is the only one worth fixing proactively — it's a float per designation and the save/load change is trivial. Everything else is either already saved or correctly rebuilt on load.
