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

## Action Queue (`action_queue.c`)

- A mover's list of small steps (walk to, pick up, sit, eat, wait); freetime runs it in
  `FREETIME_QUEUE` (counts as busy: no jobs, no talk). Optional steps are skipped when they
  can't happen; a failing required step ends the queue (`QueueEnd` releases the seat and
  reserved items). Saved after the mover paths (v112). First user: sit-then-eat
  (`QueueSitThenEat`, setting `sitToEat`). Plan and what's next (more freetime states onto it,
  player commands): `docs/todo/architecture/action-queue-and-need-index.md`.

## Money (`money.c`, City 1999)

- Money is a number on whoever holds it: `moverCoins[i]` (whole coins, saved after the action
  queues, v119). It becomes an item only when it lies in the world: `ITEM_COIN` with
  `stackCount` = coins (`IF_MONEY`: no job hauls it; picked up via the queue it goes straight in
  the purse, `MoverPocketCoins`). Code that sums `stackCount` as units must treat money as one
  (describe.c does). Shown as "3 coins" (`MoneyFormat`). Console `money`, `dropmoney`.
- Shops: a stockpile for sale (`stockpileForSale[]`, till `stockpileTill[]`, v121) charges
  `ItemPrice` (item_defs.c) when a mover takes a unit for itself: `MoverCanAfford` in a need
  search, `MoverBuy` where the unit is taken (need arrival, the queue's pick-up). A new way for
  a mover to take something out of a stockpile for itself must call `MoverBuy` too. Haulers and
  crafters don't pay (yet).
  Plan (bottle machine next): `docs/vision/city-1999.md` "Money and exchange".

## Social behaviour (`notice.c`, `talk.c`, `relations.c`, `chase.c`)

Overview, settings and scenarios: `docs/todo/social/noticing.md` (table "Built").
- Ticks: `NoticeTick` → `TalkTick` → `ChaseTick` in `sim_step.c` after movement. The mover loop
  hands a mover over at its top while it's in a talk/gathering/walk (`TalkMoverTick`) or a game
  (`ChaseMoverTick`), like the elevator hooks. Walking pace: `TalkWalkSpeedScale * ChaseSpeedScale`.
- Sideways drifts (polite passing, room round talks, round disliked movers) are summed in one
  block of the movement step and dropped when they point at a drop (walking into air = falling).
- "Free to socialise" = `CanTalk` in talk.c: no job, `FREETIME_NONE`, not drafted / travelling,
  no suspended action queue, not in a game. A new busy state must make this false.
- Games are joint activities (`activity.c`, docs/todo/social/joint-activities.md): a pool of
  running activities, each a kind (`ActivityDef` row of hooks) with its own state; several run at
  once. Being in one is the free-time state `FREETIME_PLAY`: only a free mover joins
  (`ActivityAdd`, or `ChaseJoin` = wait for the next game started), and anything that takes it
  over just changes its state (a need via `PlayWantsToStop` in needs.c, a job, drafting,
  `FREETIME_QUEUE`): the sweep in `ActivityTick` drops it (the kind's `memberLeft`: a new "it" /
  seeker / leader) and ends an activity too small to go on. A load leaves `FREETIME_PLAY`
  without a game: needs.c sets it back to `FREETIME_NONE`. A new kind: a row in
  `ActivityDefOf` + its state struct and hooks; iterate members by mover index
  (`ActivityHas(a, i)`). Movers propose games themselves (`ActPropose`, game rule `autoGames`,
  off in scenarios): a kind with `minPlayers` > 0 and a `begin` hook can be proposed; such a
  game has `timeLeft` and ends by itself.
- The social need (`social.c`, saved v120 after the purses): `moverSocial[i]`, down alone, up in
  company (`SocialCompany`: talk, gathering, walk, table talk, game), scaled by liking. Only when
  `SocialActive()` (setting `socialNeed`, off for now; `talkEnabled`; 2+ movers). Moodlets in
  mood.c. A new kind of company: add it to `SocialCompany`.
- In the idle branch, `HideControls(i)` (any player: its game gives the goals, a calm one strolls
  via `PlayWander`) comes before the deactivate-else.
- Game rules `talkEnabled`, `autoGatherings`: each game's start sets them; scenarios restore them
  (`ScenarioSettings` snapshot): add new social toggles there too.
- Transient (a load ends them): noticing, talks, gatherings, walks, games, greetings. Saved:
  relations (v111, after the belts; `appearanceSeed` guards a reused mover slot).
- Faces are decided when drawing (`MoverFaceView`/`MoverMouth` in rendering.c), from these
  modules' getters; nothing about facing is stored.
- Unity build: static helpers share one namespace (`RandRange`, `WorkshopBurning` clashed):
  prefix new ones.

## Mover Needs & Death

- **Starvation**: `mover.starvationTimer` tracks game-seconds at hunger==0, resets when hunger > 0
- **Death**: movers deactivated on starvation timeout or trapped-in-wall detection
- **Needs transitions** are logged to event log (seeking food/rest, eating complete, waking up)

## Gotchas

- **"Can it reach that?"**: call `MoverCanReach(m, x, y, z)` (mover.c: in the thing's cell or
  within 0.75 cells). Walking ends as soon as the mover is in the goal cell, so a bare radius
  misses things lying at the far edge (docs/bugs/food-off-centre-out-of-reach.md). Jobs use
  `TryFinalApproach` instead.
- **Enum shifts**: adding ItemType/MaterialType shifts `ITEM_TYPE_COUNT`/`MAT_COUNT` — stockpile `allowedTypes[]` array sizes change, breaking save compat. Always add migration.
- **Mover struct is ~12.4KB** due to embedded `path[1024]` (Point = 12 bytes). See `docs/todo/architecture/simd-optimization-candidates.md`.
- **Blueprint slot reservations are units**: `StageDelivery.reservedCount` counts units in
  transit (like `deliveredCount`), not haul jobs. A haul-to-blueprint job keeps its units in
  `targetAdjX` (`JobBlueprintUnits`); release goes through `ReleaseBlueprintHaulUnits`.
- **Stacked item blueprint delivery**: `DeliverMaterialToBlueprint` must increment `deliveredCount` by `stackCount`, not 1 — otherwise excess items silently destroyed
