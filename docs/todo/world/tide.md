# Tide (getij)

**Status: spike running** (2026-11-23). Range one level, no dikes yet (defaults, open below). Started from a brainstorm on sea, beach
and surf ("branding"); the user picked tide: "interesting gameplay, emergent behaviors". Surf
(foam drawing, wash-up items) is a cheap side idea, kept at the end.

## The idea

The sea rises and falls twice a day (North Sea: every 12 h 25 min, range 1.5-3 m, about one level
here). At low tide shallow sea floor falls dry: mud flats (wad). What should come out of it on its
own, without scripting:

- **Wadlopen**: islets reachable only at low tide; a path that is open now and closed later.
- **Food on the flats**: cockles, mussels, worms dug at low tide (a reason to go out there).
- **Danger**: the water comes back; from depth 4 a mover can't wade (`WATER_BLOCKS_MOVEMENT`).
  Movers caught in deep water already wade to dry ground (navkit-8e, 2026-11-23): the base of
  "running back before the tide".
- **Building against it**: a dike that keeps a polder dry; a jetty out to deep water. Later:
  storm surge (weather) one level higher.
- **Timing as a resource**: jobs on the flats are only possible part of the day; haulers and
  pathfinding have to know a route closes.

## Measured (island seed 3, 256x256, after 3000 ticks)

Sea surface is z 3; the map edge is a sea up to `waterEdgeSeaZ` (a console setting).
Crude low tide = load the save with `--set waterEdgeSeaZ 2`:

| sea level | ticks | cells with water | unstable | water ms/tick |
|---|---|---|---|---|
| 3 (normal) | 3000 | 95,311 | 11,888 | 0.36 |
| 2 (low) | 600 | 93,559 | 36,554 | 0.34 |
| 2 (low) | 3000 | 92,506 | 37,495 | 0.33 |

3000 ticks is about 20 game hours: only ~2,800 water cells left. (Measured on navkit-8e's then
uncommitted edge-sea build; the edge sea stays for rivers running off and deeper levels, the
tide owns the sea's top level: agreed with 8e, their edge exchange skips sea cells when the tide
is on.) **A tide can't be "move the edge
level"**: water leaves the map only through edge cells, one unit per visit, under the 4096
cells/tick budget, so the sea drains from the edge inward over days. Cost stays flat (capped);
speed is the problem.

Pathfinding: a full graph build of this island is 7.3 ms. A tide step changes walkability only
on coastal chunks (`MarkChunkDirty` + `UpdateDirtyChunks`), a few times per tide: affordable.

## Design for a spike

1. **The sea is one body.** At world generation (and load) flood-fill from the map edge through
   water at sea level: those cells are sea. A spare bit in `WaterCell` (one is free) or a sparse
   mask marks them. The water sim leaves sea cells alone (stable); the tide sets their level.
2. **Tide sets the level directly.** `tide(t)` = cosine with period 12.42 game hours. The top sea
   level (z 3) goes from 7 (high) to 0 (low); a bigger range could also lower z 2. Levels move
   one step at a time (7 steps per half tide), so walkability changes in steps:
   - level >= 4: deep, blocks walking
   - level 1-3: wading (slow; `waterSpeedShallow`/`Medium`)
   - level 0: dry flat, walkable where z 2 below is solid
   Each step: set the levels of sea cells, mark the coastal chunks whose walkability changed
   dirty. Rivers flowing into the sea and dike gaps keep the normal sim (sea cells are their
   outlet: at low tide a river runs out over the flats).
3. **Flats are where they are.** Cells at z 3 above solid z 2 fall dry; the island generator
   decides how many (a shelf in front of beaches; maybe tune it for flats and a tidal islet).
4. **Movers.** First: what happens with nothing added (do they get stranded, does wading out
   work). Then maybe: a route cost for cells that flood soon; jobs on the flats only when
   enough low tide is left.
5. **Save.** The tide phase follows game time; the sea mask is recomputed on load. Probably
   nothing new to save (a tide on/off setting).

Scenario `tide`: an island piece with a beach, a shelf of flats and an islet with something on
it (berries, cockles); movers with jobs out there. Measure: flats area dry over a day, mover
time on the flats, strandings, water and pathfinding ms per tide step.

## Spike (scenario `tide`)

`src/simulation/tide.c`: `TideMarkSea(z)` flood-fills water at level z from the map edge and sets
`WaterCell.isSea`; `TideTick` (top of `UpdateWater`) walks the level one step toward
`TideLevelAt(game hours)` and sets every sea cell, marking chunks dirty where a cell crosses
wading depth; `ProcessWaterCell` puts a visited sea cell back to the tide level. Settings
`tide`, `tidePeriod`, `tideLowLevel`. Tests: `test_tide`.

Scenario `tide` (64x40): land in the west (walk level 3), flats (floor z 1, sea z 2), a channel
two levels deep with a ford, an islet in the east with 12 logs, a log stockpile on land, 4
movers, ramps down to the flats. `TIDE_TRACE=1` prints every 100 ticks where the movers are.

What happened with no mover logic added (3000 ticks):

| ticks | tide level | movers |
|---|---|---|
| 300 | 0 (low) | step onto the dry flats, off to the islet for logs |
| 400-600 | 1-3 | the water rises while they wade (slow) |
| 700-800 | 4-5 | some in water too deep to walk, push on to the islet |
| 900-1600 | high | cut off on the islet, waiting with their logs |
| 1700-2300 | falling | walk back over the flats |
| 2400 | 2 | logs in the stockpile |

Going out at low tide, getting cut off, waiting for the next low tide: emergent. 1639 sea
cells, a tide step 0.03 ms (max 0.1); 14 steps per tide. 6 of 12 logs on land after 6000 ticks.

Saved since v102 (settings after the v101 water-edge fields; the sea rides in the water cells'
`isSea` bit, `TideAfterLoad` rebuilds the list). Checked: a save at tick 1500 loaded and run on
has the flats dry at 2000 and flooded at 2800, like an unbroken run; a v101 save loads (no tide).
Not yet:
movers walking on in water deeper than wading depth (they were already on their way); a route
cost for cells the tide will close; the island (its shore has no ramps down to z 3 flats).

## On the island (scenario `tide-island`)

The island already has flats: the shallowest sea floor band (`h` in [-1, 0) off the coast) has its
top one level under the sea surface (z 2 under z 3). The beach is one level higher, so
`TidePrepareShore` turns land cells at the sea's level between a flat and land into ramps rising
from the flat (a beach is a slope); `island_gen.c` is untouched (the island without tide is the
same). Seed 3, 256, 2000 ticks, 100 wandering movers:

- 37,533 sea cells on z 3, 861 shore ramps; 7,692 cells walkable at low tide (a quarter of the
  island's dry ground appears twice a day)
- up to 6 movers out on the flats at once, up to 5 caught in deep water; none stuck
- tide step 0.8 ms (max 2.2), 14 per tide; average tick unchanged (1.17 vs 1.13 ms, water
  0.45 vs 0.47)
- **spikes** (fixed): when the whole sea crossed wading depth in one step, every coastal chunk
  was rebuilt at once: HPA up to 28 ms. Bands by distance to deep water don't help (each band
  is a ring round the island: every coastal chunk again). Fixed with a **tide wave**: the sea is
  cut into strips about a chunk wide across the map (`tideWaveAngle`, default from the west);
  each follows the tide `tideWaveHours` (2) times its position later, so the water crosses
  wading depth strip by strip. Measured with the nav update timed (seed 3, 3000 ticks): the
  biggest update went from 362 chunks (14.8 + 11.3 ms) to 34 chunks (~5 ms on a quiet machine).
  About 0.5 ms per coastal chunk (open flats have many entrances). `TideSettle` sets the sea to
  the current tide before the first graph build (otherwise the first ticks step it all at once).
- Caught movers: the rising water caught movers more than 8 cells from dry ground and
  `WadeTowardDryCell` found nothing, so they stood in deep water until it ebbed. `WADE_RADIUS`
  8 -> 24 (with navkit-8e): they wade to the islet and wait out high tide (deep-water
  mover-ticks 3045 -> 1294 in `tide`; its arrivals floor 16 -> 5: waiting isn't arriving).

**In the island game**: setting `islandTide` (off by default): `StartIsland` places the settlers
and supplies with the sea full, then marks the sea, adds the shore ramps, settles the tide and
rebuilds the nav graph, so nobody starts on a flat that floods. `StartGame` clears any tide a
scenario left on. `--game island --set islandTide 1`, 3000 ticks: check passes (32 arrivals vs
29 without); water 113k cells at tick 300 vs 126k without. The water sim reports ~90k
unstable cells with the tide on (vs 2-9k): sea cells woken by the steps; the water cost was no
higher in the profile (0.73 vs 1.19 ms, noisy machine), worth a look if it shows later.

Not yet: food on the flats, dikes, storm surge; `islandTide` on by default (the user's call).

## Open questions

- Range: one level (flats only) or two (a real intertidal zone)?
- Do islets that are only reachable at low tide feel fun or annoying?
- Dikes and polders: in scope, or a later game?
- Speed: a full tide in 12.4 game hours; is that too slow or fast to notice in play?

## Side idea: surf (branding), cheap

- Drawing only: per sea cell the distance to shore and the shore direction (computed once);
  the water shader (`shaderEffects`, world_batch.c) draws foam bands rolling in and breaking on
  the first one or two cells, wet sand that grows and shrinks. Fallback without shader: a few
  animated foam pixels on shore cells.
- Wash-up: driftwood, seaweed, shells now and then on the beach line (wood on a beach start).
- Sound: surf noise by camera distance to the coast (the synth can make noise).
