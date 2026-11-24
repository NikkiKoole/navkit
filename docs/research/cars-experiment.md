# Cars experiment (2026-11-24)

> Status: step 1 of 4 done (one car, real steering, A to B and back). Branch `cars-experiment`.
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

## Open questions / next steps

- **Streets 3 wide for 2x2 cars** (or the car narrower than 2 cells)? Design call.
- **Step 2: a mover drives.** Walk to the parked car, get in (the train ride state),
  drive, park, get out; drive-or-walk choice next to `MaybeTakeTrain`.
- **Step 3: several cars.** Following (the IDM model in `experiments/steering`), giving way
  at crossings; plans that treat other cars as obstacles, or lanes.
- **Step 4: pedestrians.** Cars brake for movers on the arc; movers step aside.
- Parking: a goal heading (park facing the street) is a small planner change.
- Reeds-Shepp shots, for tight spots and fewer expansions.
