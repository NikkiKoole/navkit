# Transient State: Save/Load Gaps

> Notes on runtime state that doesn't persist across save/load.
> Low priority — batch fix when it becomes annoying.

---

## Actually Lost (player-visible)

### Job Progress
- `Job.progress` (float 0.0–1.0) is **not saved**
- Impact: active jobs restart from 0% on load (building, crafting, mining, etc.)
- Fix: save `progress` field per job. Simple — just add to job save/load block.

### Hunt Designations (04b, when implemented)
- `markedForHunt` and `reservedByHunter` on Animal struct — runtime-only
- Impact: all hunt marks lost on load, hunters abandon chase
- Fix: save both fields with animal data

### Event Log
- Ring buffer (4096 entries) is not saved
- F5 auto-dumps to `navkit_events.log` file, but in-memory history is gone on load
- Impact: debug trail lost. Low priority — file dump covers the debugging use case.

---

## Intentionally Not Saved (correct behavior)

These are **cleared on load by design** and re-established by the assignment system next tick:

| What | Why it's fine |
|------|--------------|
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

Job progress is the only one worth fixing proactively — it's a single float per job and the save/load change is trivial. The rest can wait until they cause actual gameplay friction.
