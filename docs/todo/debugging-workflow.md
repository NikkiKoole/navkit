# Debugging Workflow

## How It Works

1. **Play** the game normally — the event log records state changes in the background
2. **Hit a bug** — press F5 to save
3. **Hand both files** to the AI — the `.bin.gz` save + the `.events.log` written alongside it
4. AI uses:
   - `--inspect save.bin --audit` to find **what's broken** (state invariant violations)
   - The event log to find **how it broke** (trail of events leading up to the issue)

## Keybindings

- **F5** — Save game + auto-dump event log + auto-run audit (warns if violations found)
- **F7** — Run state audit on demand (prints violations to in-game console)
- **F8** — Dump event log to `navkit_events.log`

## State Audit (DONE)

Checks invariants on demand. Each sub-check returns violation count.

- `AuditItemStockpileConsistency` — ITEM_IN_STOCKPILE items at active stockpile cells, slots with counts point to active items
- `AuditItemReservations` — reservedBy matches active jobs, job item references are valid
- `AuditMoverJobConsistency` — mover↔job bidirectional consistency
- `AuditBlueprintReservations` — blueprint reservedCount matches active haul-to-bp jobs
- `AuditStockpileSlotReservations` — slot reservedBy matches active haul jobs
- `AuditStockpileFreeSlotCounts` — recomputed freeSlotCount matches stored value

CLI: `./build/bin/path --inspect save.bin --audit`

Files: `src/core/state_audit.h`, `src/core/state_audit.c`

## Event Log (DONE)

Ring buffer of 4096 entries x 200 chars with game-time timestamps.

### Phase 1 Instrumentation (DONE)

| Location | Events |
|----------|--------|
| `stockpiles.c` | CreateStockpile, DeleteStockpile, RemoveStockpileCells, PlaceItemInStockpile (success + rejection) |
| `jobs.c` | CreateJob, ReleaseJob, JOBRUN_DONE, JOBRUN_FAIL, CancelJob, UnassignJob |
| `items.c` | DeleteItem (type, position, state) |
| `mover.c` | Starvation death, trapped-in-wall deactivation |
| `needs.c` | SEEKING_FOOD, SEEKING_REST, ground rest, eating complete, waking up |

Files: `src/core/event_log.h`, `src/core/event_log.c`

### Phase 2 Instrumentation (TODO)

- Item state transitions (ON_GROUND → CARRIED → IN_STOCKPILE)
- Blueprint stage changes (AWAITING_MATERIALS → READY_TO_BUILD → UNDER_CONSTRUCTION)
- Pathfinding failures (mover couldn't reach target)
- Weather transitions
- Stockpile merge/split events (stacking)

## Bugs Found With This System

### Stacked item blueprint delivery (FIXED)
- **Symptom**: Mover hauled grass to blueprint, grass vanished, only 1/10 counted as delivered
- **Cause**: `DeliverMaterialToBlueprint` did `deliveredCount++` ignoring `stackCount`. A stack of 8 grass counted as 1 delivery, the other 7 were silently destroyed.
- **Fix**: Deliver `min(stackCount, remaining)`, split off excess. In `src/world/designations.c`.
