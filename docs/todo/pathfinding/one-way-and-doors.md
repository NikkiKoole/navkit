# One-way passages, doors and keys

Status: partial (one-way ladders built; one-way doors next; keys parked)

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
- Not yet: a build tool / recipe option for a one-way ladder (only the console sets it).

## Next: one-way doors

- Store the direction you may pass in `oneWayLayer` (`ONEWAY_N/E/S/W`) on `CELL_DOOR`.
- Rule in the host's `NavCanEnterFrom` (`nav_host.h`; today `CanStepAcrossRamps`, "both ways"):
  you can't enter the door from its exit side or leave it on its entry side, and no diagonal
  steps into or out of a one-way door. The library already handles directed steps (ramp side
  exits): reverse searches, HPA* edges built from both ends when a path isn't reversible.
- Movers cut corners with `HasLineOfSight` (`mover.c`), which only checks walkability: it must
  refuse a line through a one-way door in the wrong direction, or movers skip the check.
- Regions stay "either way" (a rare false "reachable", as with ramps and one-way ladders).
- Fuzz: add one-way door cells to `nav_fuzz_host.h` like the ladders.

## Parked: keys and locked doors (user: fix later)

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
- Questions for later: keyring/pocket slot vs hands; doors that stay unlocked once opened;
  who gets keys at the start; fetch-the-key-first jobs (one level only).
