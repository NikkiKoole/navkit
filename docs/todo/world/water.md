# Water simulation: flow, lakes, rivers, the map edge

> Status: partial (2026-11-23). Code: `src/simulation/water.c`, rivers in `src/world/terrain.c`
> (hills-water) and `src/world/island_gen.c`. Tide on top: `world/tide.md`.

DF-style cellular water: levels 0-7 per cell, falling, spreading, pressure (a full cell pushes
water up to the height it came from), sources and drains, evaporation of level-1 puddles. Cells
that have nothing to do are `stable` and skipped; at most `WATER_MAX_UPDATES_PER_TICK` (4096)
cells are processed per tick, scanning bottom to top.

## Done (2026-11-23)

- **Settled water rests** (2f8ec8b5): a full cell on full water no longer re-marks the cell below
  as under pressure every visit; lakes and seas come to rest. Island water 2.0-2.9 -> 0.4-0.55
  ms/tick. Test: `test_water` "water_rest".
- **Movers wade out of deep water** (a4287302, `WadeTowardDryCell` in `mover.c`): a mover in a
  cell that flooded under it (or a 1-wide river it was pushed over) walks to the nearest dry
  cell; a flooded ramp with a dry top counts. Radius 24 (raised for the tide's mud flats).
- **The map edge is an outlet** (1d2738fc, save v101): beyond the edge lies a sea at
  `waterEdgeSeaZ` (-1 = nothing). Edge water above it flows off, below it fills up. Generated
  worlds turn it on (`waterEdgeOutflow`); the island sets its sea level there and needs no ring
  of edge sources. Settings rows `waterEdgeOutflow`, `waterEdgeSeaZ`. Test: "water_map_edge".
- **Edge-fed rivers, the DF way** (f444e432, hills-water / survival): a river enters full at one
  map edge (sources only there) and leaves at the opposite edge; the surface only goes down and
  never above the ground beside it, so banks are one level above the water; rising terrain
  becomes a valley with one-level steps (no canyons). Lakes have no source. No rivers on maps
  under 24 cells. Survival seeds 0-4: water levels off at 2400-3200 cells (was growing to 8000+);
  reachable shore 470-655 cells (was 0-20).
- **Island:** rivers rise at springs at their heads (drainage-built, so they run downhill to the
  sea without a break); the sea beyond the edge takes what they bring. Levels off on seeds 1/3/5.

- **The sea doesn't freeze; snow lies on ice, not under water** (navkit-b4): at -6 C every water
  cell on the island froze in ~7 game hours, the sea included (91,000 cells, all walkable). Now
  water at or below `waterEdgeSeaZ` or marked `isSea` (tide) never freezes, and sea ice in older
  saves thaws; lakes and rivers still do (winter island: 465 frozen cells). Snow (`SnowRow` in
  `weather.c`) no longer settles on the bed under water, a flood clears snow it covers, and ice
  you can walk on (deep frozen water) carries snow, drawn over the ice (`DrawSnowOnIce`). Tests:
  `test_water` "should not freeze the sea", `test_snow` under water / on ice. Open: freezing is
  still instant (a lake is walkable ice hours after the first frost).

- **Water flow (current) layer + overlay** (bookkeeping only, the sim doesn't read it): every
  unit that moves sideways (spreading, a pressure push along its whole search path, the map edge)
  adds its direction to the cells it leaves and enters; the sum fades with `waterFlowHalfLife`
  water ticks (lazily). `GetWaterFlow(x, y, z, &fx, &fy)`, sparse layer `waterFlowLayer`, not
  saved. Overlay: setting `flowOverlay` (dev UI Overlays "Water Flow"), arrows on the view level
  and the one below. Cost on the 256 island: ~0.03 ms/tick (1.02 -> 1.05), ~3 MB. Screenshot
  with a built-up current: `--game island --seed 1 --exec reveal --exec "set flowOverlay 1"
  --exec "set viewZ 5" --exec "set zoom 0.25" --look-at 128,128 --screenshot-ticks 3600
  --screenshot f.png` (`--screenshot-ticks N` runs N ticks first; screenshots no longer follow a
  settler).
  **What it showed:** with the 4096 cap the island's rivers have no current at all. The cap is
  hit every tick (5,800 never-settling shallow cells on z4 plus the river mouths at z3), and the
  scan runs bottom to top, so nothing above z4 gets a turn: after 3600 ticks, 4,689 cells with
  flow, all on z3/z4. With the cap at 65536 for a test: 17,820 cells, ~31k updates/tick, and
  clean currents (the central valley drains south then west to the sea, the coasts flow out to
  the edge), but the tick goes 1.12 -> 2.04 ms. So "fair update order" (open item 2) is what
  rivers need before a current can mean anything, and the shallow churn on z4 is worth a look.

- **Fair update order** (`waterResumeRow` in `water.c`): a tick that runs out of budget remembers
  the row it stopped at, and the next tick starts there (bottom to top, wrapping once). Every
  unstable cell gets its turn, at the same cost. Island seed 1: rivers above z4 move again (z5:
  245 cells frozen -> 737 and changing); after 3600 ticks the flow overlay shows the same clean
  currents as the uncapped test (valley drains south then west, coasts flow out), tick 0.96 ->
  0.92 ms. Wetness sync (water wetting the soil) now runs on capped ticks too: it was skipped
  every tick the cap was hit. Survival seeds 0-2 still level off, but at 3,900-4,900 water cells
  (was 2,400-2,700: water reaches what was starved), water 0.23-0.31 -> 0.28-0.80 ms (seed 1
  highest). Test `test_water` pressure height: now a real pressure test (wall across the top).
  Cost of fairness: with ~25k unstable cells and 4096 updates a tick, a cell gets a turn about
  every 6 ticks, so water moves slower than real time; less churn would help (next item).
- **Less churn** (2026-11-24, on top of fair order; first tried alone on branch
  `water-churn-experiment`, ca3ffe80, superseded): most unstable island cells were a level-1 film
  over the lowlands that could never rest ("keep it unstable for evaporation") and 2|1 pairs
  swapping forever.
  - Level-1 water may rest; evaporation is a sweep every `waterEvapInterval` (`WaterEvaporateSweep`;
    also fixes: puddles past the update cap never evaporated).
  - A one-level step moves only when the unit can keep going (`WaterStepCarries`): the receiver
    has a lower neighbour, or the giver's current points that way (momentum from the flow layer).
    Terraces still flatten (the staircase test now ends 3 3 3 3 3 4 4 4 4 4: the momentum sloshes
    the water to the far end), a bump on a flat sheet runs on to an edge or a wall and rests.
    Tried first: a local lower-neighbour rule (left terraces), a bounded plateau search (2.5x cost).
  - **Run bits** (`waterRunBits`): one bit per 16-cell run of the scan, set by every wake, cleared
    when the run is found idle; the scan skips sleeping runs (island: 30,000 runs read per tick ->
    1,500). Woken all on clear, on load (`WaterWakeAll`) and every 256 water ticks as a safety
    net for wakes that bypass `DestabilizeWater`.
  - **Water depth tells the pathfinder** (`WaterDepthChanged`): every level write that crosses
    wading depth is reported (only the tide and freezing did; ordinary flow left the nav graph
    stale: in the tide scenario settlers waited on the islet for a path that was open). Batched:
    flowing water crosses ~180 times a tick in survival (river cells flicker 3 <-> 4); the first
    crossing in a window remembers what nav knew, and every 60 water ticks only cells whose state
    really differs are reported (HPA 0.92 -> 0.03-0.07 ms/tick).
  Results against fair order alone (min of 4 runs, 12k ticks): island unstable ~25k -> ~5.9k (a
  cell's turn every ~1.4 ticks instead of ~6), tick 1.20 -> 1.33 ms, water 0.39 -> 0.48; survival
  tick 0.92 -> 1.01 ms, water back to 2.3-3.6k cells (fair alone: 3.9-4.9k). Flow overlay: stronger,
  cleaner currents. Open: island lowland water still grows slowly (z4 22.6k -> 31k cells over
  36k ticks, slowing). Freezing with that much water: fixed 2026-11-24 (next item).
- **Freezing sweep cheaper** (2026-11-24): `GetSeasonalSurfaceTemp` remembers its last answer
  (two `sinf` per call, and every `GetTemperature` lands there), and the freezing sweep takes the
  ambient once per level and reads only the cell's offset. Island: Freezing 0.45 -> 0.16 ms/tick,
  tick 1.18 -> 0.87 ms; survival 0.086 -> 0.053 ms. Same temperatures (island-winter output
  identical).

- **The current is used** (2026-11-24): flowing water draws light flecks sliding downstream
  (`DrawWaterFlecks` in `rendering.c`, setting `waterFlecks`; still water unchanged, byte-identical;
  ~0.2 ms/frame on a view full of flow). **Off by default** (2026-11-24): the short axis-aligned
  dashes read as random pixels rather than flow; ideas if retried: longer thin streaks, tilted
  with the current, one per cell. **Drift** (`ItemsDriftTick` in `items.c`, setting
  `itemsDrift`): loose floating items (wood material, plant matter) go with the current, 0.15-1
  cell per game-second; they float up to the moving surface layer (a log on a full river bed sees
  no current), need water deep enough (see below; a full cell below counts), slide along banks when blocked
  diagonally, and stay put with no water ahead. Not when carried, stored, contained or reserved.
  How deep the water must be depends on the item (`ItemFloatDepth`): light plant matter (sticks,
  leaves, grass, reeds, bark, saplings) floats in any water, wood (logs, planks, poles) needs 2.
  Carried off the map edge where water flows off (`waterEdgeOutflow`, above `waterEdgeSeaZ`), an
  item is lost (event log line); an open edge cell counts as moving water though it is always
  empty. Scenario `drift` (island, a log and a bundle of sticks on 60 river spots, no settlers; in
  scenarios-check): seed 1, 36k ticks: logs 20 of 60 drift (max 53 cells), 3 reach the sea;
  sticks 30 (max 92 cells), 12 reach the sea. Most wash up; the rest lie in still water (lakes,
  pools at river heads). Nothing is lost there: the full sea is still, so things that reach it
  stay offshore. Tests `test_water` water_drift (downstream, a rock stays, a reserved log stays,
  a log rests at the water's edge, sticks in a thin stream lost off the edge while a log stays).

- **Trickling springs** (2026-11-24, save v103): with `springTrickle` on, a source inside the map
  adds `springRate` units per game-second (default 1; per game-second like the flow it feeds, so
  the day length doesn't change rivers) while it has room, instead of refilling to full every
  visit. Sources on the map edge stay taps (a river from beyond: hills-water/survival). Every
  terrain generation and scenario reset turns it off; the island generator turns it on (island
  game and island scenarios). Island seed 1, 12k ticks: lowland (z4) water 26,926 with taps ->
  1,030 with springs (generated: 700); river water above z4 stays near what was generated (930-963
  of 973). Still a slow growth (+200/12k ticks at 1/s, about proportional to the rate): basins with
  no outlet only lose level-1 water to evaporation; a sink (water soaking into soil) is the next
  step. Trade-off: rivers are shallower (the same water over more cells), so logs, which need
  water 2 deep, barely drift in the `drift` scenario anymore (taps: 20 logs up to 53 cells;
  springs: 10 up to 2.7; sticks 30 up to 92 -> 27 up to 7.8). Options: a rate per river by its
  catchment (the island has flow accumulation), a higher rate, or brooks as they are. Tests
  `test_water` water_springs.

- **Wading with the current** (2026-11-24): a settler's walking speed in water is multiplied by
  `WaterCurrentSpeedFactor` (water.c): 1 + `waterCurrentPush` (setting, default 0.5) x strength x
  how well the walking direction lines up with the current, clamped 0.3-1.8. Faster downstream,
  slower upstream, the same across; no sideways push (that would steer settlers off their path
  into deep water). Test: 8 cells in a thin stream take 118 ticks downstream, 348 upstream, 175
  both ways with the push off. scenarios-check: survival 18 -> 17 arrivals, tide 14 -> 11.

Lesson: the old update cap hid a flood. Water that churned used the whole budget, so real flow
far along the scan barely moved. With the churn gone, the water does what the rules say: any
source with no way out floods until it finds one. Every source needs an outlet.

## Open

Parked ideas (spring size by catchment, soaking, showcases, rafts, flecks): `water-ideas.md`.

1. ~~**Trickling springs**~~ done 2026-11-24 (see Done); **soaking** still open (only if we want brooks and ponds inside the map). A source
   today refills to 7 every visit: unlimited flow. A spring could add a little per tick instead,
   and water standing on soil could soak away slowly (ties into the wetness/mud layer), so a
   spring-fed pond settles where inflow = soaking + evaporation. Rain balance belongs here too.
2. ~~**Fair update order**~~ and ~~**less churn**~~ done (see Done). Threads (checkerboard chunks;
   the pressure search crosses chunks) only if water still binds at 1024+.
3. **Old saves:** hills-water saves made before f444e432 keep their old rivers (sources along the
   river, inland ends): they still flood slowly. New worlds only.
4. ~~**Inspector mismatch**~~ fixed 2026-11-25: `--inspect --cell` applies the water rules from the
   save's water (the game's rule reads the global water layer, which --inspect doesn't load):
   "NO (deep water 5/7)", and "YES (ice below)" over deep ice.
5. ~~**Flaky `test_terrain`**~~ fixed 2026-11-23 (6560fbc2: the connectivity tests set their own grid and seed). Was: "connectivity_fix_preserves_largest": varied run to run at 035ff9f8 on
   an 8x8 map (ASan/UBSan clean, so probably an uninitialised read). Moot since f444e432 (no rivers
   under 24 cells); the test should set its own grid size and seed.
