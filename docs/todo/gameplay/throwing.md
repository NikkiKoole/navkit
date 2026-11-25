# Throwing (and later ranged attacks)

Status: **experiment, slice 3 built** (2026-11-25). A mover throws a small item at a goal cell
as a job; the item flies in 3D, bounces off walls and the ground, rolls to a stop. A stone that
hits an animal dazes it, one landing next to it scares it off; hunters throw stones at their
prey and walk in while it's dazed.

## Built (slice 1)

- `src/entities/throwing.c` / `.h`, job `JOBTYPE_THROW`, item state `ITEM_IN_FLIGHT` (no job
  takes it; both appended at the end of their enums).
- **What can be thrown:** one unit of anything not two-handed, at most `throwMaxWeight` kg
  (default 5). A stack gives up one unit (`SplitStack`). Sharp stone yes; raw stone (25 kg,
  two-handed) and logs no.
- **The job:** `ThrowOrder(mover, item or -1, gx, gy, gz)`. Item -1 = something throwable in its
  hands, then its pockets, then the nearest on the ground (same level). Steps: fetch it ->
  walk toward the goal, every 0.25 s asking "would a throw from here get there?" -> wind up
  (0.35 s) -> let go. If it gets as close as it can without a clean throw, it throws anyway
  (45 degrees, full strength: may fall short).
- **Planning (`ThrowPlan`):** tries arcs in the order 35, 30, 40, 25, 45, 50, 20, 55, 60, 15,
  65, 70 degrees. For each it simulates a perfect throw *to where the item comes to rest*
  (hops and rolls included) and aims shorter by the overshoot (4 iterations). The first arc
  whose perfect throw rests within 1 cell of the goal on the goal's level wins. That gives
  lobs over walls and under ceilings without special code.
- **Whether it gets there:** top speed from weight (`throwSpeed` for ~1 kg, 1/sqrt kg above,
  less for very light things); range on flat ground is speed^2 / gravity (~13 cells for a
  stone). The throw goes wrong in direction (1.5 deg + 0.15 deg per cell, x `throwSpread`) and
  strength (3% + 0.4% per cell).
- **Flight:** cells, height in levels (one level = one cell), gravity `throwGravity`, sub-steps of at
  most 0.2 cells. Walls, solid cells, closed doors, floors (`HAS_FLOOR`, from above and from
  below) and the map edge reflect the speed on the axis that hit (`throwBounce` kept straight
  off, 0.85 along a wall, 0.5 along the ground). A slow ground bounce turns into rolling
  (friction 30 cells/s^2); rolling off an edge falls again. Into water (level >= 2): it stops
  and sinks. At rest: `ITEM_ON_GROUND` at the lowest open level under it.
- **Drawing:** `DrawThrownItems` (rendering.c, from `DrawItems`): a shadow dot on the ground,
  the sprite raised half a cell per level of height.
- **Saving:** flights aren't saved. `ThrowLandAll()` after a load puts anything saved mid-air
  on the ground.
- **Dev:** console `throw [mover] [x y z]` (mover number, else the selected one; cell, else
  the mouse on the viewed level). Settings group "Entities/Movers/Throwing": `throwSpeed`,
  `throwGravity`, `throwBounce`, `throwSpread`, `throwMaxWeight`.
- **Scenario `throwing`** (in scenarios-check): four lanes, the stone goes back to the
  throwing spot after each throw. 3000 ticks, default settings:

  | lane | throws | within 1.5 cells | mean miss |
  |---|---|---|---|
  | open ground, 8 cells | 37 | 81% | 1.0 |
  | 13 cells, wall 3 behind the goal | 30 | 50% | 1.6 |
  | over a 2-high wall | 32 | 44% | 2.4 |
  | uphill onto a 2-high block (z3) | 34 | 41% | 2.1 |

  The scenario's re-throw loop is a headless hook: in the GUI each mover throws once.
- Tests: `tests/test_throwing.c` (what's throwable, perfect throw rests on the goal, wall
  bounce never ends inside, lob over a wall, out of range falls short, floors and edges, 200
  noisy throws in a walled room, load, the job, stack split, refusing).

## Built (slice 2): hits scare animals

- Each tick the flight's segment is tested against every animal (a body 0.45 cells round, 0.8
  levels tall, standing on its level). Faster than 3 cells/s it's a hit: `ScareAnimal` and the
  stone bounces back off it with a quarter of its speed. One hit per throw; slow rolling stones
  just bump. Counted in `throwHitCount`, logged in the event log.
- `ScareAnimal(idx, fromX, fromY)` (animals.c): for `animalScareHours` game hours (setting, 1.5
  = ~9 cells of running at any day length) the animal runs from the thrower (or from where the
  stone was let go). Simple grazers run straight away (like from a hunter, at any distance;
  they now stop at walls and water instead of running through them, hunted ones too); steering
  grazers panic (danger just behind them on the line to the thrower, interest a few cells the
  other way); predators drop their prey and run. Transient beside the saved `Animal` struct
  (not saved).
- Movers are never hit.
- Dev: in the game, select a mover, mouse on an animal, console `throw`.
- **Scenario `throwscare`** (in scenarios-check): three movers each throw at a grazer ~8 cells
  away, again and again at wherever it stands (headless loop). 3000 ticks: 64 throws, 19 hits
  (the grazers keep moving; aiming is at the cell it stood on); hit grazers ran 9-20 cells.
- Tests: `throw_hit` in `tests/test_throwing.c` (a hit grazer runs from the thrower and calms
  down; a short throw and a throw passing beside don't scare; steering grazer; predator stops
  hunting).

## Built (slice 3): hits daze, near misses scare, hunters throw (user's calls)

- **Every hit dazes** (`DazeAnimal`): the animal stands still for `animalDazeHours` game hours
  (1.5), head down, three yellow stars circling over it; its description says "Dazed". Then it
  comes to and runs (`ScareAnimal`, from the thrower). A dazed predator drops its prey.
- **A miss that comes down within 1.5 cells** of an animal (first ground touch) scares it: it
  runs (`throwStartleCount`).
- **Aiming at an animal** (`ThrowPlanHit`): flat arcs first (15, 20, 25, ... degrees: less time
  for it to move), aimed at the middle of its body where it will be when the stone gets there
  (its speed is measured each tick, `AnimalVelocity`). The arc must pass within 0.3 cells of
  that point before touching anything (no throws through walls). Then the usual noise.
- **Hunters throw** (setting `huntThrowStones`, on): `WorkGiver_Hunt` gives the hunter a stone
  first (hands, pockets, else lying within 10 cells; never its tool). Chasing, from 2.5 cells
  or further, every 0.25 s: would a throw from here hit it? Then stop, wind up 0.35 s, throw.
  A hit: it walks in while the animal stands dazed (the attack freezes it as before). A miss:
  it fetches the stone again (if it can get to it) and goes on. At the kill it puts the stone
  down. The thrown stone is kept in `job->targetMineX` (Y/Z stay -1): `targetItem` is released
  on a cancel and the stone isn't reserved while it flies.
- **`animalFleeSpeed`** (setting, 1.3 as before): a fleeing grazer's pace x walking speed.
  Above ~3.3 it outruns a person: then only stones (or a corner) get it.
- Fixed along the way: picking an item up counts when the mover stands in its cell (a mover
  stops anywhere in the goal cell; a stone in the corner was never picked up).
- **Scenario `throwhunt`** (in scenarios-check): two lanes walled apart, fast deer (flee 4.0).
  The hunter with a pile of stones vs one without. 3000 ticks: 5 kills vs 2; 12000 ticks: 18
  vs 7 (42 throws, 15 hits, 14 near misses). The chaser only gets deer it corners at the lane
  ends.
- Tests (`throw_hit`, `throw_hunt`): daze then run, near miss scares, aiming ahead of a running
  animal hits, no plan through a wall, a hunter dazes a deer that outruns it and kills it, no
  stone close by: chases as before.

## Not yet / ideas

- The simple flee runs straight away from the threat, so deer get cornered at map edges and
  walls; a smarter flee (along open ground, like `fleeSmart` in chase.c) would make stones
  matter even more.
- Hunters don't carry stones around between hunts (a pouch of stones, slings later).

- A hit could wound (needs animal health); bigger stones / spears daze longer.

- **Hitting movers.** No health yet (see `../world/explosions.md` "Gaps"); a hit mover could at
  least flinch / be annoyed at the thrower (relations).
- **The shadow** is hard to see on dark grass; maybe a ring or a lighter outline.
- **Skill / tiredness / wind** in the spread (weather has a wind vector).
- **Per-material bounce** (a stone on rock bounces, on mud it stops; a stick barely bounces).
- **Explosions** keep their own flat sliding for blasted items; they could use this flight
  (with height) instead.
- **Player tool:** E with a mover selected -> "throw at here" (action queue), a throw button.
- Non-combat uses: tossing trash into a bin from a distance, playing catch, passing a tool up
  a cliff, scaring off a predator. Growth path: sling (a tool), thrown spear, bow + arrows
  (ammo items), fire arrows, catapult.
