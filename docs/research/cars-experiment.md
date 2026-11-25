# Cars experiment (2026-11-24)

> Status (2026-11-24): one car with real steering; long roads + speed; 30-car jam test; **a road
> network (`cars-city`): 100 cars with lanes, reserved junctions, parking bays, traffic lights,
> roundabouts and overtaking on avenues - no touches, no wall contacts, no red-light runs**.
> Scales to 4000 cars on a 1024 map at ~2.3 ms/tick. Next: pedestrians, movers driving,
> parking lots. Branch `cars-experiment`.
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

## Road network: `cars-city` (2026-11-24)

`src/entities/roads.c/.h` (the network), `src/entities/traffic.c/.h` (driving it). 330x260
cells, a jittered 9x7 grid of junctions ~36 cells apart, ~30% of the roads bend, ~12% left
out (T-junctions, never a dead end, all connected). Roads 6 wide: one 3-wide lane each way,
right-hand traffic. Settings `carCityCount` (100), `carCitySeed` (layout).

- **Network:** a road is a centre line between two junctions; a lane is it offset 1.5 cells
  to the right, ending 7 cells before each junction. Through a junction, cubic curves
  ("connectors") join each incoming lane to each outgoing lane of another road (no U-turns).
  Two connectors conflict when they come within 2.6 cells or merge into one lane;
  connectors from the same lane don't (cars follow each other).
- **Driving** (trackgen's brain): IDM car-following (1 s time gap, 1 cell standstill gap)
  to the car ahead on the route, bend limits per segment, Stanley steering at the front axle
  with curvature feedforward on the same bicycle model as the free car.
- **Junctions are reserved.** A car asks when within braking distance + 6 cells and stops at
  the line until granted. Requests are served in arrival order; one blocked by a crossing
  path claims its connector (no starving), one only waiting for room past the junction
  doesn't (it isn't in anyone's way). A connector is granted only if its exit lane has room
  for the car: nobody stops inside a junction.
- **Routes** are shortest paths over lanes; a slow car on a lane adds 8 cells to its cost,
  so new trips go round queues. Each car drives trip after trip to random lane spots.

Results, 10 game minutes per run:

| layout seed | 100 cars: trips, mean speed, longest junction wait | 128 cars |
|---|---|---|
| 1 | 907, 4.1 cells/s, 32 s | 931, 2.9, 57 s |
| 2 | 945, 4.4, 32 s | 975, 2.8, 39 s |
| 3 | 939, 3.3, 43 s | 1049, 3.1, 38 s |
| 4 | 1021, 4.2, 20 s | 1046, 2.8, 34 s |
| 5 | 987, 4.3, 24 s | 1055, 3.4, 31 s |

**0 car pairs touching and 0 cars touching a wall in every run** (sampled every half second,
2 px tolerance). 0.04 ms per tick for 100 cars.

What it took (each measured):
1. Pure pursuit cut the inside of junction curves by more than the lane's slack (wall
   touches on every tight turn): **Stanley + curvature feedforward** fixed it.
2. Slanted roads carved in whole cells left the edge 2.5 cells from the centre line in
   places: carve half a cell wider, plus 2.5 cells round every lane and connector.
3. Lanes ending 5.5 cells from the junction made sharp turns too tight: **7 cells**.
4. A car waiting for room past a junction blocked every crossing path behind it (60 s
   waits with free exits): such a request no longer claims its connector.
5. Two cars from one lane splitting onto two curves: the follower lost sight of the leader
   on the sibling curve and hit it: cars on sibling curves count as ahead until the curves
   part by a car width.
6. Junctions 30 cells apart left lanes of 8-11 cells (2-3 cars): queues spilled back
   fast. 36 apart. Then **traffic-aware routes** halved the worst waits (100 s -> 26 s).

Diagnostics (env, headless): `TR_TRACE=<car>`, `TR_WALLS`, `TR_TOUCH`, `TR_LONG`, `TR_JDUMP`,
`TR_PULL`, `TR_HOLDS`, `TR_STILL`, `TR_RING`, `TR_BAY=<bay segment>` (see traffic.c's header).

## Parking bays (2026-11-24)

Lay-bys along the lanes: a lane of 18+ cells gets one bay, 32+ two (~160 bays in `cars-city`).
A bay is an in-curve from the lane (at s0) to a spot 2.5 cells to the right and an out-curve
back into the lane 12 cells further on. Cars drive to a free bay (reserved when the trip is
planned), stop exactly in the spot, park 10-40 s, then plan the next trip. They start parked,
leaving over the first 30 s.

- Route steps join lanes part-way (the step before a bay ends at its s0, the step after one
  starts at its s1): `StepStart` / `StepEnd` in traffic.c.
- **Pulling out** waits for the lane to be clear from beside the spot to 3 cells past the exit,
  and for moving cars behind (8 cells + 2 s at their speed). Stopped cars behind (a queue)
  don't block: they see the car from its first move and give way.
- **Being let in:** a car waiting 3 s to pull out makes approaching lane cars stop a car length
  short of its spot (bays near a lane's end sit in the junction queue, which crept past the
  exit forever: waits of 140 s before, 15-50 s after).
- Lane cars see a car pulling in for most of its curve and a car pulling out from its first
  move; a car pulling in or out sees the lane cars ahead of it (two touches found and fixed:
  each side was blind to the other half-way through the curve).

Results, 10 game minutes, 5 layouts x 100 / 128 cars: 755-895 trips (each a drive + a park),
junction waits <= 26 s, pull-out waits <= 50 s, **0 touches, 0 wall contacts in every run**.

## Traffic lights (2026-11-24)

Every 4-way junction gets lights (setting `carCityLights`, 17 of 63 junctions in `cars-city`);
T-junctions stay first come, first served. Opposite approaches are green together: 12 s green,
3 s amber, 1 s all-red per half of a 32 s cycle, each junction at a random offset. Left turns
on green still give way to oncoming traffic (the reservation's conflicts). A request is granted
only on green and only if the car reaches the line before green + amber run out; a request on
red waits without claiming its connector.

Results, 10 game minutes, 5 layouts x 100 / 128 cars: 0 touches, 0 wall contacts; **0 red
lights run** out of ~2000-2300 entries into lit junctions per run (counted, and the scenario
check fails on any). Throughput drops ~27% against first-come junctions (555 vs 765 trips at
100 cars); junction waits 27-41 s (a red phase + the queue).

## Roundabouts (2026-11-24)

Setting `carCityRoundabouts` (3): 4-way junctions become a one-way ring (radius 12 cells, round
a concrete island), anticlockwise on screen (right-hand traffic); each road ends at its own ring
junction. New in the network: one-way roads, arc roads, a per-junction setback (3.5 cells on a
ring), lanes with right of way (the ring).

- **Ring has right of way:** its requests are served first; an entering car gives way to any
  ring car whose path conflicts and is within 3 cells + 3 s of the junction (it may not have
  asked yet).
- **At most one car per ring stretch on the ring** (entering waits otherwise). "Capacity minus
  one space per stretch" still let two cars per stretch lock the whole ring.
- **Full exit: go round again.** A ring car whose exit has no room for 1 s takes one more lap
  (it would otherwise stop on the ring and block it). Elsewhere a car waiting 15 s for room past
  a junction plans another way round, if there is one.
- **A junction only becomes a roundabout if every ring stretch is 6+ cells**: jittered roads
  arriving at close angles made a 2.6-cell stretch nobody could ever enter (a permanent block).

Found on the way (all junctions): a grant needs room for the car + its gap + **the distance to
get its rear out of the junction** (0.6 cell more). With exactly a car + gap it could stop half
in and keep holding the junction, blocking every crossing path.

Results, 10 game minutes, 5 layouts x 100 / 128 cars, with lights and 3 roundabouts each:
546-681 trips, junction waits 28-62 s, pull-out waits 21-59 s, longest standstill 28-58 s,
**0 touches, 0 wall contacts, 0 red-light runs, no deadlocks**.

## Avenues and overtaking (2026-11-24)

Setting `carCityAvenues`: the middle row and column of `cars-city` are avenues, 12 cells wide,
two lanes each way: the right-hand lane by the kerb (bays along it) and an overtaking lane
inside. Each lane has its own curves through a junction (to the lane with the same index, or a
smaller road's only lane); the conflict check lets side-by-side straight paths go together.
Avenue junctions get 9 cells of setback; no roundabouts on avenues.

- **Overtaking:** behind a slow car (one pulling into a bay, or slower than us with road in
  front of it) within 6 cells + 2 s, move to the overtaking lane if it's clear (behind: a car
  length + 2 cells + 1.5 s at the other car's speed; ahead: a car length + 3 cells + 1 s at
  ours; nobody at its bays). Not near the lane's end, not on the road we park on, not once
  we've asked for / hold the next junction; 4 s between changes. The route is planned again
  from the new lane.
- **Back to the right-hand lane** when it's clear and has no slow car just ahead.
- **A lane change is a drift, not a jump:** the steering aims at a line that moves from the old
  lane to the new one over 6 cells. Until the car is within a cell of the new lane it still
  counts as a car on its old lane for the cars behind it there.
- Found on the way: I had put the second lane outside the first, so the bays along the
  right-hand lane sat on top of it (cars pulling out touched cars passing).

Results, 10 game minutes, 5 layouts x 100 / 128 cars, with lights, roundabouts and avenues:
503-690 trips, ~36 overtakes per run, junction waits 27-42 s, **0 touches, 0 wall contacts**.

## Scale: thousands of cars (2026-11-25)

`--set scenarioSize N` makes `cars-city` fill an N x N map (1024: 28 x 28 junctions, 932 junctions
with the ring junctions, 3092 lanes, ~14.7k segments, ~1860 bays; avenues every 8th row / column,
roundabouts per 63 junctions). Limits now: 4096 cars, 4096 junctions, 32000 segments (routes
store segments as short: about a 1024 city). `carCityParking 0`: cars start on the roads and
just drive (with parking, more cars than bays end up nearly all parked, waiting for a bay).

CPU, 1024 city, every car driving (`carCityParking 0`), 5 game minutes, `--profile`:

| cars | Cars ms/tick (avg) | moving at the end | trips |
|---|---|---|---|
| 1000 | 0.38 | 578 | 825 |
| 2000 | 0.98 | 904 | 1055 |
| 4000 | 2.27 | 1252 | 907 (heavy congestion: mean speed 1.1 cells/s) |

Almost all of it is driving the cars (`TrafficDrive`); junctions, lists and the touch sampling
are ~0.04-0.16 ms. Worst ticks of 20-57 ms are machine noise (other sessions building): the
trivial `TrafficLists` loop shows the same 30-70x spikes. **0 touches, 0 wall contacts at every
size.** Past ~3000 cars on this map it's the roads that saturate, not the CPU.

What scaling found (each measured, each fixed):
1. **Routing** was a linear scan over all lanes per step (Dijkstra without a heap): now a heap.
2. **99% of route searches failed** on big maps: avenue overtaking lanes where an avenue starts
   had no way in (curves only joined lane k to lane k), cutting off every lane behind them.
   A one-lane road's lane now also feeds an avenue's overtaking lane. `TR_NET=1` checks every
   lane is reachable.
3. **Parked cars with every bay taken** fell back to a lane trip from the bay's curve (no way
   on): 20 failing searches every 5 s each. They now just stay parked a while longer; picking
   a free bay tries 300 random bays (free to skip) instead of 30.
4. A car that found no other way round a full exit searched again **every tick**: 5 s pause.
5. **Lane changes:** only with room to pull out past the car ahead; while still partly in the
   old lane the cars ahead there count; the drift shrinks with distance driven, not time (a car
   changing lanes in a queue was told to be in the new lane while standing still, ended across
   both and was hit).
6. **Turns sharper than 120 degrees** swept the stop lines of other roads: built only where a lane
   would otherwise have no way in or out.
7. **Cramped junctions:** a curve passing within 2.6 cells of where a car waits at another
   lane's stop line pushes that junction's lanes back a cell (the network is built again, up to
   6 times).
8. Touch sampling uses an 8-cell bucket grid; roundabout occupancy is counted once per tick.

Diagnostics added: `TR_NET` (reachability), `TR_FAILS` (failing route searches); the stats show
route searches per caller and the most in one tick. Profiler sections: `Cars`,
`TrafficLists/Junctions/Drive/Sample`, `DrawCars`.

## Slot traffic: the "Game & Watch" idea (2026-11-25)

The user's idea: every road position is a fixed spot; simulate which spots hold a car, and give
cars a real pose only where someone looks. `src/entities/slots.c/.h`, setting `carCitySlots`
(same network, lights, roundabouts). Every lane and junction curve is cut into slots of ~3 cells;
each 1 s step all cars at once (Nagel-Schreckenberg): speed +1 slot (max 3), slowed to the free
slots ahead, a 20% dawdle, move - two cars never share a slot. A curve is entered only when
granted: green, no crossing curve occupied or granted, the exit lane with more free slots at
its start than cars already on the curve; longest waiting first, ring traffic first. Routes:
the same A*, 12 searches per tick from a queue; queue-aware costs; a car refused for want of room
for 15 steps re-routes. Poses are computed only when drawn, for cars on screen, moving smoothly
between slots.

2048 city, 16000 cars, 5 game minutes:

| | full sim | slot sim |
|---|---|---|
| CPU for the cars, avg per tick | ~9 ms | 0.43 ms (~20x less) |
| trips | 1192 | 1275 |
| mean speed now | 1.2 cells/s | 0.9 |
| moving now | 5052 | 2280 |
| longest standstill | 295 s | 299 s |

Both are congested at this density; trips are about the same; the slot sim's junctions pass
fewer cars (more queueing). 4000 cars on a 1024 city: full 1.24 ms, slot ~0 (under noise).

Found building it: 500 route searches in one step made a 140 ms tick every second (now spread
over ticks); "exit's first slot free" let two cars of one lane end up on one curve waiting for
one slot, stopping inside the junction (now: free slots > cars on the curve); grants given to
cars that couldn't reach the curve that step blocked crossing traffic for nothing; without the
full sim's "find another way round a full exit" gridlock loops never broke.

Not done yet: real (steered) cars in the watched area with a hand-over; parking in slot mode;
the slot sim gridlocks at ~50000 cars on the 2048 city (a full network).

## City shape and compact junctions (2026-11-25)

The user: roads should have longer straights; junctions leave a big empty square. Settings now:
`carCitySpacingX/Y` (default 72 x 40: long blocks east-west), `carCityJitter` (2), `carCityBends`
(0.1), `carCitySetback` (lanes stop this far from a junction's centre: default 4, was 7).
Bays: one per 16 cells of lane, up to 4 (long roads have more parking; with one or two per lane
the long-block city had only as many bays as cars, and parked cars never found a free one).

With near-square junctions even steered 2x2 cars fit setback 4 (cramped junctions are still
pushed back as needed): 5 layouts x 100/128 cars, 0 touches, 0 wall contacts, junction waits
<= 61 s, more trips than at 7. Slot traffic (250 cars, 2-cell slots): 398 trips at setback 4,
314 at 7. The old city: spacing 36/36, jitter 5, bends 0.3, setback 7.

Long blocks mean less road per area (34 junctions / 104 lanes instead of 63 / 226 on the test
map): fewer alternative routes, busier junctions.

## Manhattan layout (2026-11-25)

The user asked what would add a New York driving vibe; first pick: the layout. Setting
`carCityLayout 1`: avenues north-south every 72 cells, one-way with two lanes, alternating
directions; cross streets every 24 cells (3:1 blocks), one-way one lane, alternating, every fifth
two-way; Broadway, a two-way diagonal whose slope (one avenue per three streets) makes it cross
avenues exactly at street junctions (6-way, Times Square style) and cut the streets in between at
4-way junctions of its own; lights on every 4-way junction. The outermost avenues and streets are
two-way (West Side Highway / FDR Drive): a one-way grid's corners otherwise have nothing arriving
or nothing leaving (`TR_NET` found it). New in the network: one-way roads with two lanes, road
width per road, 40 curves per junction.

Results: fully connected at 330 ... 2048 (2406 junctions); steered, 5 x 100/128 cars: 0 touches,
0 wall contacts, 0 red runs, junction waits <= 54 s; slot traffic 250 cars: 492 trips (grid: 398).

## Green waves and yellow cabs (2026-11-25)

- **Green wave** (`carCityGreenWave`, on): each lit junction's offset is set so a car at 80% of
  the limit, driving along the junction's most north-south road (south from the top, north
  from the bottom), arrives as its light turns green. Manhattan, 128 cars, 3 seeds: 605-609
  trips with it, 587-603 without (~+1.5%): trips are short with many turns, so the wave helps
  little in numbers; it's for the platoons you see rolling down an avenue.
- **Yellow cabs** (`carCityCabs`, 0.3): yellow, never park; at each fare they brake and stand in
  the lane 3-8 s; cars behind wait (one-lane streets) or overtake (a standing cab counts as a slow
  car). Slot mode: cabs stand 3-8 steps at each fare. 20 runs (Manhattan + grid): 0 touches,
  0 wall contacts; in the grid city, cabs in its single lanes lengthen queues (pull-out waits up
  to ~100 s).

## Real cars in view, and a more physical slot model (2026-11-25)

`carSlotReal`: slot cars in the camera view also get a physical car (bicycle model, steering,
smooth speed) chasing their slot's pose; snapped back when 3+ cells off. Cheap (~0.01 ms for
~100 cars in view). With the plain cellular model it looked wrong (the user saw cars jump near
junctions): slot cars stop dead from full speed and take junction curves at full speed, which no
real car can follow.

`carSlotPhysical` (on): each step a car looks up to 24 slots ahead and takes the fastest speed
from which it can still stop behind the car ahead even if that one brakes too (Gipps), braking
at most `carBrake` per step; junction curves have a speed (from their curvature) cars slow to in
time; a junction that doesn't look passable (red / crossing traffic / no room) is braked for as
a stop line. Manhattan, 250 cars, 5 minutes, view 140 x 100 cells:

| slot model | slot | snaps | tracking error | jumps in view | touching pairs max | trips |
|---|---|---|---|---|---|---|
| plain | 2 | 142 | 0.64 | ~220 | 27 | 528 |
| plain | 3 | 360 | 0.60 | ~530 | 10 | 466 |
| physical | 2 | 4 | 0.36 | 2 | 16 | 459 |
| physical | 3 | 22 | 0.42 | 15 | 4 | 366 |

(2-cell slots are bumper to bumper by definition: real cars touch.) Cost: none in CPU (16000
cars, 2048 city: 0.15 ms/tick either way); throughput down 13-25% (cars brake for junctions).
Diagnostics: SL_VIEW, SL_JUMPS (counts only jumps inside the view when there is one), SL_SNAPS.

## Slot traffic: lane changes (2026-11-25)

The user: leave the physical layer off and grow what the slot model can do. First: lane changes
(`carSlotLaneChange`, on). On a lane with a parallel neighbour a car moves diagonally, one slot
forward into the lane beside, when that slot and the one beside it are free and the first car
behind could stop in time; it then plans from the new lane. Safe anywhere on the lane (but its
last slot): speeds are worked out after the lane-change pass, so every car behind sees the car
in its new slot. Why: overtake a slow / standing car just ahead (a cab at a fare, a queue) when
the other lane has 2+ more free slots; keep right onto clearly free road (4 slots, or free to a
short lane's end). 5 steps between changes, at most 40 a step (each plans a route). Drawn as an
S-drift with the car turned toward its new lane.

Results (250 cars, 3-cell slots, 5 minutes): grid 26 overtakes / 93 keep-rights, Manhattan 0 /
65 (its avenue lanes are short and mostly queues at lights); 0 jumps; trips grid 276 -> 268 avg
over 4 seeds (-3%: cars behind brake), Manhattan 366 -> 395. 16000 cars: 0.2 ms/tick, worst tick
18 ms. Keeping right "as soon as it had as much room" made cars swap lanes all the time.

## Lane rules: turn from the right lane, get into it in time (2026-11-25)

From the user's look at Trafficity ("cars do their best to get in lane in time to make a
turning", lane connections). `carCityLaneRules` (on), **slot traffic only for now**:
- Lane connections: from a road with two lanes this way, right turns only from the right-hand
  lane, left turns only from the left lane, straight on from both; from a one-lane road a right
  turn into the right-hand lane and a left turn into the left lane of a two-lane road. Curves
  from two lanes side by side conflict unless both go straight (a turning car's rear sweeps
  toward the lane beside).
- Routes may change lanes: a lane followed by its parallel lane (cost 4 cells). The lane's end
  (or where the route leaves the lane beside, e.g. a bay entrance: the deadline) is a stop line
  until the car has merged; it merges as soon as there is a gap; no gap for 8 s / 8 steps, or
  past the deadline: it goes another way (a bay on that lane: given up, a new trip).
- A voluntary change (overtake, keep right) is called off if the route from the new lane would
  need the old one straight back.

Slot traffic, 4 seeds x 2 layouts, 250 cars, 10 minutes: 0 jumps, checks pass, longest
standstill 94-160 s, 330-830 merges per run (~13% give up), trips grid ~670, Manhattan ~820.

**Steered cars keep the old rules**: with the lane rules the 10-minute sweep had standstills of
300-540 s in about half the runs and touches in 9 of 20. Found and fixed on the way (both sims):
curves' `parallel` was 0, not -1 (curves into lane 0 looked like lane changes); a merge done
past its deadline put a car "into" a bay curve while it stood in the lane. Still open for the
steered sim: merges at a stop line beside a queue, cars holding a junction while queued behind
a merging car - work to do.

## Slot traffic: no more overlapping cars (2026-11-25)

The user saw a straight-on car drive through a car turning right ahead of it (both from one
lane). `SL_TOUCH=1` now counts drawn cars that overlap (every half second). Three causes, fixed:
1. **Curves from one lane start together** but have their own slots: a car may enter a curve
   only when the shared start of its sibling curves (slots within 2.6 cells of a sibling,
   worked out once) is empty and nobody was let onto a sibling this step; such a junction
   counts as not passable for braking.
2. **A lane change clipped the car right ahead in the old lane** (half-way the changer is half a
   slot on): changes need that slot free (overtaking with one free slot, like pulling out).
3. **The car behind drove into the slot being vacated** while the changer was still drawn half
   in it: the old slot stays the changer's until the step's moves are done.

8 runs (2 layouts x 4 seeds, 250 cars, 10 minutes): 0 touches in 7, one run 1 pair in 3 of
1200 samples (a curve passing an exit lane's first slot at a junction with an awkward angle -
compact junctions; left as is), 0 jumps, all checks pass.

## Slot traffic: kerb spots for cabs and parking (2026-11-25)

Setting `carSlotKerbs` (on). Every second slot of a right-hand lane gets a spot beside it at
the kerb (RD_BAY_OFF = 2.5 cells right), if it is 3.2+ cells from every other lane and curve
(bucket grid; so none near junctions) and not in the lane's first 2 / last 2 slots. The strip
is carved (`SlotsCarveKerbs`). A parked car is in `slKerbCar`, not in the lane's slots, so it
blocks nobody.
- Trips of cabs (always) and of 40% of the others (City Parking) end on the slot before a
  spot; arriving, the car pulls in diagonally (its lane slot and the one ahead held for the
  step, as a lane change) - spot taken: a cab stands in the lane as before, a parker drives on.
- Cabs stand 3-8 s, parked cars 20-90 s; then the next trip is planned and the car pulls out
  diagonally to the slot ahead when that slot and the one beside are free and the first car
  behind can brake in the gap (near the lane's start: nobody on a curve coming into it).
- 30% of the non-cabs start parked (leaving over the first minute).

250 cars, 10 minutes: 0 touches, 0 jumps on both layouts; grid 608 trips (648 without kerbs:
parked time), Manhattan 746 (692: cabs no longer block a lane). 16000 cars on 2048: same cost.

## Slot traffic: indicators, brake lights (2026-11-25)

`SlotCarSignals(i)`: indicators ~4 slots before a turn and through the curve, while waiting for /
doing a lane change, before pulling in (right) and out (left) at the kerb; hazards for a cab at a
fare; brake lights when slower than last step or standing in traffic. Drawing only.

## Slot traffic: pedestrians (2026-11-25)

Setting `carCityPeds` (80): movers walking between random pavement spots (endless mode).
- **Pavements**: two cells of concrete floor round every road (`CityPavements`). Walking in the
  road costs 26x on routes only (`navPathExtra`, nav_host.h: a scenario's route-only cost per cell;
  string pulling counts it too), except on **crossings**: across each lane's mouth just past its
  end, where they don't touch another road's lanes. Crossings need room in front of the junction:
  with pedestrians the junctions are set back 6 cells (4 is too compact: the crossing lay in the
  other road's lanes and pedestrians stood in its traffic).
- **Cars**: each step the slots a pedestrian in the road walks (now + 1.2 s along its last
  velocity; distance to the slot's whole stretch, not its centre - slots are up to 6 cells long)
  count as taken: cars brake for them (Gipps, as for a standing car) and a curve with someone on it
  is not granted.
- **Pedestrians** (`moverStepHold`, mover.h: a scenario's hold, NULL normally): on the pavement
  about to step off, wait for a gap (no moving car there or up to 4 slots back), at most 6 s; at
  lights, cross only while the road crossed has red; stepping off is announced and happens at the
  next step's start (going mid-step, cars already moving there hit them). In the road they wait
  only for a moving car (waiting for standing ones locked whole districts: cars stood for
  pedestrians who stood for cars).

Lessons: every "wait for the other" between cars and pedestrians must be one way (pedestrians in
the road never wait for standing cars); a waiting pedestrian must not project its walk (it
blocked the car it waited for); "in the road" is the map's floor, not distance to a slot (corner
pavements are within 1.5 cells of tight right-turn curves).

250 cars, 80 pedestrians, 10 minutes, setback 6 (trips without pedestrians, same setback):
grid 470 (537), Manhattan 501 (532); pedestrians inside a car 85 / 56 of 96000 samples (mostly
brushing a standing car while crossing). Setback alone costs: Manhattan 746 trips at 4, 334 at 8.

## Slot traffic: movers drive (2026-11-25)

Setting `carCityDrivers` (20, out of the pedestrians): a mover owns a car parked at the kerb
(spawned at its door, the pavement cell nearest it). Owned cars never leave on their own.
- The goal picker (`CityWalkGoal`): a goal 50+ cells away with the car within 30 cells: walk to
  the door first (the goal kept in `transportFinalGoal`); at the door `SlotsMoverBoard`.
- Boarding: the mover is `TRANSPORT_RIDING` (the train ride state: not walking, not drawn, needs
  paused; set again every tick - the picker's path planning resets it via MaybeTakeTrain), its
  position follows the car. The car plans to the free kerb spot nearest the goal (buckets in
  growing squares; a spot taken on arrival: the next one), pulls out, drives with the traffic,
  pulls in; the step after, the owner gets out on the nearest pavement cell and walks to the goal.

Two bugs hid the drives at first (the "drives" counted were fake): the owner got out again while
the car still stood at its spot (now: after a pull-in after boarding), and the jobs' idle pass
reset the ride state of a jobless mover every tick (a drive is now marked as the mover's own trip,
`TrainsMarkPersonalTrip`) - the drivers stood in the road and the cars waited for them. Keeping
the ride state by clearing the path each tick made the repath pass plan 50000 paths (148 ms/tick).

Drawing: a driven car has the mover's head sprite on its seat and a white ring (far out: a ring).
Camera follow (`carFollow`) in slot mode follows mover 0, a driver.

Manhattan, 10 minutes: 250 cars + 20 drivers: 51 drives, 38 done; 80 cars + 60 drivers (of 100
pedestrians): 281 drives, 244 done, 0.15 ms/tick; mover 0 drove 8 trips of 45-110 s.

## Open questions / next steps

- **Streets 3 wide for 2x2 cars** (or the car narrower than 2 cells)? Design call.
- **Step 2: a mover drives.** Walk to the parked car, get in (the train ride state),
  drive, park, get out; drive-or-walk choice next to `MaybeTakeTrain`.
- Parking lots off the network (Hybrid A* into a space and back out, the driveway reserved
  like a junction); kerbside bays are done.
- Actuated lights (green on demand) or a green wave; priority roads (PriorityComing already
  handles any lane with right of way); lane choice before turns on avenues.
- Pedestrians (step 4 of the original plan); movers deciding to drive (step 2).
- **Step 4: pedestrians.** Cars brake for movers on the arc; movers step aside.
- Parking: a goal heading (park facing the street) is a small planner change.
- Reeds-Shepp shots, for tight spots and fewer expansions.
