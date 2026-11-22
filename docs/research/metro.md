# Metro, tram, underground: brainstorm (2026-11-22)

> Status: research / brainstorm (nothing planned)

We already have most of a metro: trains, stations, boarding and the walk-or-ride decision
(`src/entities/trains.c`, `docs/todo/world/trains.md`), and a voxel world with z-levels has the
underground built in. What's missing is mostly what `trains.md` lists as gaps: separate
lines, loops, more than one train per line, timetables.

Related: [vehicles.md](vehicles.md), `docs/todo/pathfinding/transport-layer.md`,
`docs/todo/pathfinding/service-points-and-queues.md`, `docs/vision/city-1999.md`,
`docs/todo/endgame-village-vision.md` (transport network).

---

## Three flavours

| | Tram | Metro | "Abstract" metro |
|---|---|---|---|
| Where | street level, track in the road | tunnels at z below the street | not simulated underground at all |
| Track walkable? | yes: you cross it, and it can hit you | no (or dangerous) | — |
| Cost to build | track + stops | digging + stairs + platforms | station = a portal with a travel time |
| Big maps | fine | the tunnel crosses many chunks | cheapest: no tunnels to load |
| Feel | busy streets, bell, danger | vertical: stairs down, platform, crowds | quick to make, loses the underground |

Tram is closest to today: `CELL_TRACK` is already walkable. The abstract metro is a later
"long lines on big maps" option (see `docs/todo/architecture/chunked-world-plan.md`).

## What a metro adds to our sims

- **Digging is mining.** A tunnel is mined cells; lay track in it and you have a line.
  No new building system.
- **Water:** tunnels below the water table seep and flood. Pumps and drainage become
  infrastructure; a flooded station closes.
- **Fire and smoke:** smoke fills a tunnel. Ventilation shafts are a real reason to build upward.
- **Temperature:** deep underground stays mild, warm in winter and cool in summer,
  so stations are warm places.
- **Stairs are the bottleneck:** the platform fills and everyone squeezes up the same stairs.
  Service queues and single-file ladders already model this; escalators or elevators are the upgrade.

## City 1999: a whole place

- **Warm shelter:** sleeping in stations, or riding the circle line all night to stay warm
  (until the last train).
- **Rules:** the station closes at night, guards move you on, fare dodging and ticket
  inspectors. The ownership axis again, with the ticket as a key.
- **Money:** busking in the corridors (foot traffic sets the pay), begging in carriages,
  deposit bottles on platforms.
- **The NPC source:** commuters come out of the metro in waves (rush hour). That foot traffic
  fills bins and makes begging spots, so the metro drives the trash loop. The city doc already
  has NPCs arriving by train; this makes it physical.
- **The tunnels:** walking the dark tunnels is forbidden, dangerous and a shortcut.
  Abandoned stations become shelter ("mole people").

## Colony

The late village gets a reason to live farther out: commute time (farm stop, village centre,
mine). A metro under the hill to the mine fits, because the mine is underground already.

## Technical gaps (in order)

1. **Lines:** a named, coloured line with its own track, so trains don't share all track.
   The biggest one.
2. **Loops**, not just back-and-forth.
3. **More than one train per line** with spacing: Factorio-style blocks, one train per block.
4. **Track that pedestrians avoid**: a cost or "forbidden" flag for metro track, so movers
   don't use tunnels as footpaths (unless, in City 1999, that is the point).
5. **Better route choice:** the deeper the station, the more the stairs count in "is the train
   faster?" The `MaybeTakeTrain` heuristic may need the vertical cost.
6. **Timetables:** last train, opening hours. Needs the schedule system (`schedule-system.md`).

## Recommendation (if ever built)

1. **Tram first:** uses what exists, makes lines and loops necessary, and shows on screen.
2. **Metro** = the same lines in a tunnel plus "pedestrians avoid the track".
3. **Abstract metro** only if big maps need it.
