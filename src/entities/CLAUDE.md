# entities/

Movers, items, jobs, stockpiles, workshops, animals, trains, containers, stacking.

## Adding New Item Types

1. Add to `ItemType` enum in `items.h` — **before `ITEM_TYPE_COUNT`**
2. Add row to `itemDefs[]` in `item_defs.c` (name, weight, category, etc.)
3. Add sprite mapping in `ItemSpriteForTypeMaterial()` in `render/rendering.c` if material-specific
4. If stockpileable: existing stockpiles won't have it in `allowedTypes[]` — add save migration
5. Bump save version in `save_migrations.h`, add `StockpileVxx` struct for old format
6. Run `make path` (the Makefile tracks header changes, no `make clean` needed)

## Adding New Job Types

1. Add to `JobType` enum in `jobs.h` — **at the END** (save compat)
2. Add name to `JobTypeName()` in `jobs.h`
3. Write `WorkGiver_Foo()` — checks capability, finds target, creates job via `CreateJob()`
4. Write `RunJob_Foo()` — step-based state machine, returns `JOBRUN_RUNNING/DONE/FAIL`
5. Add WorkGiver call to `AssignJobs()` in priority order
6. Add RunJob dispatch in `JobsTick()`
7. If designation-driven: add `DESIGNATION_FOO` in `designations.h`, cache in jobs.c

## Job System Patterns

- **WorkGivers** try to create a job for a specific idle mover, return job ID or -1
- **Job drivers** (`RunJob_*`) are step-based state machines called each tick
- **Job cancellation**: always use `CancelJob()` — it safe-drops carried items, clears reservations, returns mover to idle list
- **Idle mover cache**: maintained incrementally via `AddMoverToIdleList`/`RemoveMoverFromIdleList` (don't scan all movers)
- **Active job list**: `activeJobList[]` for O(n) iteration where n = active jobs only

## Item Reservation Rules

- Check `reservedBy == -1` before using an item
- Use `ReserveItem()`/`ReleaseItemReservation()` — never set `reservedBy` directly
- Container accessibility: `IsItemAccessible()` walks `containedIn` chain — fails if any ancestor is reserved or carried

## Stockpile Patterns

- Slot state: use `IncrementStockpileSlot`/`DecrementStockpileSlot` for atomic updates
- Filters: `allowedTypes[ITEM_TYPE_COUNT]` + `allowedMaterials[MAT_COUNT]` bitmasks
- Ground item cache: `groundItemIdx[]` avoids expensive `FindGroundItemAtTile` calls
- Free slot cache: `freeSlotCount` rebuilt each frame

## Workshop Patterns

- Templates: ASCII art in `workshopDefs[]` — `'X'`=work tile, `'O'`=output, `'F'`=fuel, `'#'`=block
- Passive workshops (`passive=true`): auto-convert items on timer, no crafter needed after delivery
- Semi-passive: crafter does short active work (`JOBTYPE_IGNITE_WORKSHOP`), then passive timer runs
- `CELL_FLAG_WORKSHOP_BLOCK` set on `'#'` tiles, cleared on workshop delete

## Mover Needs & Death

- **Starvation**: `mover.starvationTimer` tracks game-seconds at hunger==0, resets when hunger > 0
- **Death**: movers deactivated on starvation timeout or trapped-in-wall detection
- **Needs transitions** are logged to event log (seeking food/rest, eating complete, waking up)

## Gotchas

- **Enum shifts**: adding ItemType/MaterialType shifts `ITEM_TYPE_COUNT`/`MAT_COUNT` — stockpile `allowedTypes[]` array sizes change, breaking save compat. Always add migration.
- **Mover struct is ~12.4KB** due to embedded `path[1024]` (Point = 12 bytes). See `docs/todo/architecture/simd-optimization-candidates.md`.
- **Stacked item blueprint delivery**: `DeliverMaterialToBlueprint` must increment `deliveredCount` by `stackCount`, not 1 — otherwise excess items silently destroyed
