# Cars experiment (2026-11-24)

> Status: step 1 done (one car, real steering); long roads + speed done; jam test with 30 cars done
> (2026-11-24). Next: lanes on a road network. Branch `cars-experiment`.
> Builds on [vehicles.md](vehicles.md) ("cars are parked for now"). Target game: City 1999.

## Goal

Movers driving cars from A to B and back, with real steering. A car is **2x2 cells**.
Decisions so far (user, 2026-11-24): real steering (not cell-to-cell), drive anywhere
walkable (no ladders / ramps / shafts), City 1999 as the end target; the first experiments
cover one car, several cars in traffic, cars and pedestrians, a mover deciding to drive.

## How to run it

```bash
make path
./build/bin/path --scenario cars                          # GUI
./build/bin/path --headless --scenario cars --ticks 60000 # stats
./build/bin/path --headless --scenario cars --ticks 60000 --set carWidth 1.8
CAR_TRACE=1 ./build/bin/path --headless --scenario cars --ticks 3000   # every plan + state change
```

Settings (dev sidebar group *Experiments/Cars*, or `--set` / console): `carLength`,
`carWidth`, `carMaxSpeed`, `carAccel`, `carBrake`, `carLatAccel`, `carMaxSteer`,
`carSteerRate`, `carLookahead`, `carStopWait`, `carPlanMargin`, `carDebug`.

The `cars` scenario: a 64x40 town of concrete blocks on z=1 with streets carved out. One
car tours from its home lot A to a stop and back, again and again:

| Stop | Street | Result (2.0 wide car) |
|---|---|---|
| B | 3 wide, sharp 90-degree corner, to a square | works |
| C | 2-wide dead end | no route (skipped) |
| D | 2 wide, sharp 90-degree corner | no route |
| E | 2 wide, cut corner, to a lot | no route |
| F | 3 wide, a corner, to a lot | works |

Debug draw: route (forward in the car's colour, reverse white, gear changes ringed in
yellow), the pursuit point, state, stop letters, and dots where a car centre fits (big =
room to turn).

## What's built (all in new files, nothing saved)

- `src/entities/cars.c/.h`: the car entity, planner, follower. `src/render/cars_draw.c`.
- Hooks: `CarsTick` in `SimStep` (only when there are cars), `DrawCars` after trains,
  `ClearCars` on scenario reset / game start / load. Settings rows. The `cars` scenario
  and its check (`scenarios-check`: at least 4 legs in 3000 ticks, never stuck).

**Car model:** kinematic bicycle (turns about the rear axle). Wheelbase 0.6 x length,
35 degree lock: a 2-long car turns on a 1.71-cell radius (rear axle).

**Body collision:** the rotated rectangle against every non-drivable cell under it,
exact separating-axis test. A car can't be inside a wall, ever.

**Planner: Hybrid A\*** (as in self-driving / parking research). States (x, y, heading),
binned at half a cell x 32 headings. Each expansion drives 14 short arcs (20 px):
wheel at 0, +-lock/8, +-lock/2, +-lock, forward and reverse. Reverse costs double, a
gear change 1.5 cells. The body (grown by `carPlanMargin`, default 3 px) must be free
every 4 px. Heuristic: grid distance to the goal around the buildings (Dijkstra over
cells where a 2x2 square fits, the "clearance map"). A route therefore already holds its
3-point turns and back-ups.

**Follower:** per stretch of one gear: the planned wheel angle (feedforward) plus a
correction for heading and sideways error (Stanley-style), slowing for bends and for the
gear change, stopping at each gear change and turning the wheel before pulling away.
Blocked for 0.5 s or more than half a cell off the route: plan again from where it stands.

## Results (default settings, 60000 ticks = 1000 game seconds)

```
car 0: 76 legs done, 623.9 s driving (avg 8.2 s/leg), 82 gear changes, 29 replans,
       27 bumps, 0 stuck, 33 plan fails (C, D, E: no route)
plans: 1-20 ms when a route exists; a failing plan searches 36k states, ~200-300 ms
```

## Findings

1. **A grid route + reactive steering doesn't work for a 2x2 car.** First version:
   clearance A* on the grid, pure pursuit, K-turns and backing up as reflexes. The car cut
   corners into buildings, wedged itself in street mouths and looped between K-turn and
   driving (100+ K-turns, 0 legs). The grid route ignores the turning circle, so the
   follower is set problems it can't solve. Planning with the car's own motion
   (Hybrid A*) fixed it: routes are drivable by construction.
2. **A car as wide as the street can't use it.** A 2.0-wide car needs exact alignment
   (< 2 px, < 2 degrees) to enter a 2.0-wide street, and turning at full lock sweeps
   ~2.25 cells. With any safety margin there is no route. Real cars have slack (1.8 m car,
   3 m lane). For 2x2 cars, **streets should be 3 wide** (2 wide works only as straight,
   aligned, parking-style spots).
3. **Narrower bodies (1.4-1.8) get into 2-wide streets but rarely back out**: the search
   has to hit a tiny window of heading and position. Small wheel angles (lock/8) helped
   (the car can correct a 2-degree tilt), but it is still brittle. The textbook fix is an
   analytic "shot" to the goal (Reeds-Shepp curves) during the search.
4. **The planning margin must exceed the follower's tracking error.** Margin 2 px on a
   2.0 car: 775 replans and 22k bumps (scrapes, replans, scrapes). Margin 3 px: 29 replans.
5. **Same collision test everywhere.** A planner that checks every 5 px and a follower
   that checks from 2 px disagreed when the car grazed a wall: endless replanning. Fixed
   by finer plan checks plus a 1 px grazing tolerance while driving.
6. **Cost.** A plan is 1-20 ms on 64x40 when a route exists. Failing searches are the
   expensive ones (~200-300 ms, 36k states). Big maps need a cap or a quicker "no" (for
   example: no 3-wide street to the goal, don't search).

## Long roads: `cars-town` (2026-11-24)

192x128 cells: a 5-wide ring road with rounded corners, a downtown grid, a diagonal
boulevard through a roundabout, a winding country road; lots A (home), B (mall), C, E (3-wide
gate). One car, camera follows it (setting `carFollow`). All roads 5 wide (the user's call).
Settings: top 20 cells/s (~160 km/h at 2.25 m per cell), accel 4, brake 6, grip 4.

**Why the route looked jittery:** Hybrid A* bins every pose (half a cell x 32 headings); when the
straight-ahead bin is taken the search must steer a little, so straight roads came out as
zigzags, and the car braked for every kink (2/3 of the A-B route capped it below 5 cells/s).
Raising the steering costs did not fix it (and made plans up to 10x slower).

What fixed speed, measured over 500 game seconds:

| | before | after |
|---|---|---|
| trips | 8 | 18 |
| avg trip | 60 s | 23.5 s |
| top speed reached | 5.7 | 20 cells/s |
| bumps / stuck | 0 / 0 | 0 / 0 |

1. **Keep off the walls** (the key one): arcs whose body comes within 0.75 cell of a wall cost
   one arc length extra. The planner had hugged walls at the 3 px margin (shortest route), so
   at speed any tracking error scraped. Every speed-up failed (scrapes, replan loops) until this.
2. **Smoothing after planning**, open road only (tight places keep the planned route): pull
   points to their neighbours with a weak hold, pin points back where the result is too close to
   a wall or too sharp for the wheel. Helps a little (18 vs 15 trips without it).
3. **Bends judged over half a cell** of route (headings), not point to point.
4. Safety: a car that scraped can plan again (start may graze); blocked within a car length of
   the stop = arrived; 6 replans in the same spot = give the stop up.

Plans now 50-110 ms (was ~10). Still: a third of the route asks for < 10 cells/s. A shortcut
pass (a straight line between two route points where it's clear) would make straight roads straight.

## Jam test: `cars-traffic` (2026-11-24)

30 cars (setting `carTrafficCount`), each with its own space along lot edges, touring home -> a
road point -> home. Cars brake to stop half a cell behind a car on their arc ahead and wait (no
replan: the planner doesn't see cars). Debug: red line = waiting for that car, red ring = stopped
behind a car > 10 s ("deadlocked").

**Result: all 30 deadlocked within about a minute;** 14 trips in 10 minutes. 0.1-0.7 ms per tick.

| where | cars | why |
|---|---|---|
| ring road top | 7 | head-on: east- and westbound on the same (middle) line |
| lot A driveway | 4 | leaving meets arriving; each waits for the other |
| lot B entrance | 7 | same, a bigger knot |
| lot C | 8 | all leave at once into a 3-cell aisle, noses block neighbours in pairs |
| lot E gate | 6 | leaving and arriving cars in a 3-wide gate |

Findings: no lanes = certain head-on deadlock; mutual waits need a tie-break or (better)
reservations; lots need rules (one-way aisles, gates, staggered leaving). The planner itself
held up: every failure is cars meeting cars.

**dreamengine `trackgen` (TRAFFIC mode, `docs/design/traffic-ai.md` there)** solved this:
roads are centre lines, lanes are offsets; the brain follows its lane, keeps an IDM time-gap
to the leader in its lane, overtakes into a clear lane; junction right of way is a
**reservation** (gap acceptance could never guarantee no crash); a debug overlay shows why each
car brakes. No free-space planner. `trafficjam.c` there has 1-D per-lane car-following.

## Open questions / next steps

- **Streets 3 wide for 2x2 cars** (or the car narrower than 2 cells)? Design call.
- **Step 2: a mover drives.** Walk to the parked car, get in (the train ride state),
  drive, park, get out; drive-or-walk choice next to `MaybeTakeTrain`.
- **Step 3: traffic (next).** A road network with lanes on 6-wide roads (two 3-wide lanes for
  2x2 cars), trackgen's brain (lane following + IDM + junction/driveway reservations); Hybrid A*
  only from a parking space to the road and back.
- **Step 4: pedestrians.** Cars brake for movers on the arc; movers step aside.
- Parking: a goal heading (park facing the street) is a small planner change.
- Reeds-Shepp shots, for tight spots and fewer expansions.
