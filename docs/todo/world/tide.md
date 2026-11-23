# Tide (getij)

**Status: idea, measured; next experiment** (2026-11-23). Started from a brainstorm on sea, beach
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

3000 ticks is about 20 game hours: only ~2,800 water cells left. **A tide can't be "move the edge
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
