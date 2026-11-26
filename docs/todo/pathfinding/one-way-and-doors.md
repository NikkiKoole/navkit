# One-way passages, doors and keys

Status: partial (one-way ladders and one-way doors built; keys parked)

Asked for 2026-11-24: ladders with a direction (one way), keys and doors, maybe one-way doors.

## Built: one-way ladders (save v110)

- `oneWayLayer` (grid.h, sparse, one byte per cell; empty chunks cost nothing): `ONEWAY_UP`,
  `ONEWAY_DOWN` on ladder pieces. N/E/S/W are reserved for one-way doors.
- Rule (`CanClimbUpAt` / `CanClimbDownAt`, cell_defs.h): a step between two ladder pieces is
  one-way if either piece is marked, so a piece added to a one-way ladder follows it.
  `SetLadderOneWay(x, y, z, dir)` sets the whole connected column; `PlaceLadder` clears stale
  values left on cells that are no longer ladders.
- nav library: a ladder link exists if it can be climbed either way; its graph edges follow
  the allowed directions; reachability regions join both ends (regions are "either way").
  Everything else in nav already asked up and down separately.
- Console `oneway up|down|both [x y]` (the ladder at the mouse), cell description line, an arrow
  drawn on the ladder (green up, orange down). Scenario `ladder-oneway` (in scenarios-check:
  fails on any climb against the direction). Tests: `one_way_ladders` in test_pathfinding,
  and the nav fuzz makes a quarter of its ladders one-way (2000-world sweep clean; putting the
  old two-way edges back makes it fail at once).
- Showcase scenario `oneway-tiers`: a stepped tower of three decks, up-only ladders on the
  north side, down-only on the south, a down-only "fire pole" from the top deck to the ground
  (the busiest way down). Arrows show through open air from above (up to 4 levels, dimmer).
- Not yet: a build tool / recipe option for a one-way ladder (only the console sets it).

## Built: one-way doors

- `ONEWAY_N/E/S/W` on a `CELL_DOOR` in `oneWayLayer` = the direction you may pass (N = y-1).
  `SetDoorOneWay(x, y, z, dir)`; a newly built door starts both ways.
- Rule `CanStepThroughDoors` (cell_defs.h), part of `CanStepAcrossRamps`, so the nav host's
  `NavCanEnterFrom` and the game's `CanStepTo` both use it: a step into or out of a one-way
  door must go its way; no diagonal steps into or out of it. No nav library change needed.
- Movers: `HasLineOfSight` (path smoothing, waypoint checks) refuses a line through a door
  the wrong way or diagonally.
- Console `oneway n|e|s|w|both` on a door (same command as ladders), describe line, light
  blue arrow. Scenario `door-oneway` (a building, door in at the south, door out at the north;
  scenarios-check fails on any wrong-way step; 48 movers: 0 wrong-way). Tests `one_way_doors`;
  the nav fuzz places one-way doors (with walls beside them) and edits them (2000 worlds clean).
- Not yet: a build option for a one-way door (only the console); movers pushed by crowd
  avoidance are not stopped physically (none went the wrong way in the scenario).
- dreamengine: its turnstile becomes a door + ONEWAY_<dir>.

## Parked: keys and locked doors (user: fix later)

> Now part of the joint plan `../world/infrastructure.md` (doors, locks, mechanisms, pipes): door
> states first, then household front doors and visiting (step 2), the rest of keys in step 5.

Decided so far: **keys are real items**, carried around (dropped, picked up, lost).
Open points, from the first look:
- A key item needs a lock id (an `Item` field, save bump); a door a lock id (small table).
- What a mover can open = the keys it carries at the moment it plans a path.
- The nav world is shared (one graph, regions, flow fields), so access can't be baked in.
  Likely: locked doors become point entrances like ladders, their edges checked per query
  against the mover's keys; "can this mover get there" via regions with locked doors as walls
  plus a small region-door graph.
- dreamengine's lock scheme (0 never locked, 1..999 household, 1000+g group key) maps onto
  key items carrying that id.
- Use case from dreamengine: the station FARE_GATE (proposed there): passed both ways by
  anyone holding a ticket, so a ticket is just another key item; plus an escape flag that
  opens it for everyone in a fire.
- Questions for later: keyring/pocket slot vs hands; doors that stay unlocked once opened;
  who gets keys at the start; fetch-the-key-first jobs (one level only).
