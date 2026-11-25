# Throwing (and later ranged attacks)

Status: **experiment, slice 2 built** (2026-11-25). A mover throws a small item at a goal cell
as a job; the item flies in 3D, bounces off walls and the ground, rolls to a stop. A stone that
hits an animal scares it off.

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

## Not yet / ideas

- **Ranged hunting:** the hunt job throws (aim leading a moving animal) instead of walking
  up to it; a hit wounds (needs animal health), a scared animal is harder to get close to.
- Near misses could startle too (a stone landing within a cell or two).

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
