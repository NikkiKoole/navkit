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
5. Add WorkGiver call to `AssignJobs()` in priority order (a new phase also gets a row,
   with its priority level, in `jobPhases[]`)
6. Add RunJob dispatch in `JobsTick()`
7. If designation-driven: add `DESIGNATION_FOO` in `designations.h`, cache in jobs.c

## Job System Patterns

- **WorkGivers** try to create a job for a specific idle mover, return job ID or -1
- **Job drivers** (`RunJob_*`) are step-based state machines called each tick
- **Job cancellation**: always use `CancelJob()` — it safe-drops carried items, clears reservations, returns mover to idle list
- **Idle mover cache**: maintained incrementally via `AddMoverToIdleList`/`RemoveMoverFromIdleList` (don't scan all movers)
- **Active job list**: `activeJobList[]` for O(n) iteration where n = active jobs only
- **Distance weighting** (setting `jobDistanceWeight`, 0 = off): `AssignJobs` then runs the
  phases in distance bands (`AssignJobs_DistanceBands`): a job one priority level lower wins
  if it is `1/weight` cells closer. A WorkGiver must call `JobInRange(moverIdx, x, y, z)` right
  after choosing its target (before reserving or pathfinding) and return -1 if false;
  item-centric phases get it through `TryAssignItemToMover`. Code that changes an item before
  assigning it (split, take out of a container) checks `ItemHasHaulerInRange` first.
  Movers in mover-centric phase loops are wrapped in `PhaseMoverSkip/Begin/End`.

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

## Queues: Service Points & Ladders

- `service.c` — service points: places that serve one (or a few) movers at a time keep
  who waits, in arrival order (per end). Queue *shape* is not stored: movers line up
  behind the one ahead. Design: `docs/todo/pathfinding/service-points-and-queues.md`
- `ladders.c` — single-file ladders on top of that: one climber per segment, lines at
  both ends, the queue's route cost (`LadderQueueExtraCost`, the `NAV_CLIMB_EXTRA` hook),
  climbing speed. `mover.c` calls `LaddersUpdate`, `LadderHoldAtFoot`, `LadderClimbDone`,
  `LadderFollowLine`. Train stations keep their own (saved) waiting list in `trains.c`.
- Not saved: lines rebuild themselves after a load.

## Mover Paths (coarse routes)

- `MoverPlanPath(i, start, goal)` is how movers get a path. HPA* paths come back as a
  route of waypoints (`moverRoutes`, chunk-border/ladder/ramp entrances) and only the
  next legs are refined into `moverPaths` (a window, same goal-first format as a full
  path); movement refills the window when it runs out (`MoverContinueRoute`). Other
  algorithms stay full paths. Toggle: `useCoarsePaths`
  (console `coarsePaths`).
- Code that writes `moverPaths` directly must set `moverRouteLen[i] = 0` (a refill also
  checks the window still came from the route, but don't rely on it).
- `InvalidatePathsThroughCell` checks the straight segments between path points too:
  string-pulled paths skip the cells in between.

## Mover Needs & Death

- **Starvation**: `mover.starvationTimer` tracks game-seconds at hunger==0, resets when hunger > 0
- **Death**: movers deactivated on starvation timeout or trapped-in-wall detection
- **Needs transitions** are logged to event log (seeking food/rest, eating complete, waking up)

## Gotchas

- **Enum shifts**: adding ItemType/MaterialType shifts `ITEM_TYPE_COUNT`/`MAT_COUNT` — stockpile `allowedTypes[]` array sizes change, breaking save compat. Always add migration.
- **Mover struct is ~12.4KB** due to embedded `path[1024]` (Point = 12 bytes). See `docs/todo/architecture/simd-optimization-candidates.md`.
- **Stacked item blueprint delivery**: `DeliverMaterialToBlueprint` must increment `deliveredCount` by `stackCount`, not 1 — otherwise excess items silently destroyed
