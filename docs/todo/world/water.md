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

Lesson: the old update cap hid a flood. Water that churned used the whole budget, so real flow
far along the scan barely moved. With the churn gone, the water does what the rules say: any
source with no way out floods until it finds one. Every source needs an outlet.

## Open

1. **Trickling springs + soaking** (only if we want brooks and ponds inside the map). A source
   today refills to 7 every visit: unlimited flow. A spring could add a little per tick instead,
   and water standing on soil could soak away slowly (ties into the wetness/mud layer), so a
   spring-fed pond settles where inflow = soaking + evaporation. Rain balance belongs here too.
2. **Fair update order** (measured 2026-11-23 with the flow overlay: on the 256 island the cap
   starves every river above z4, see "Water flow" above; still measure at 1024). The 4096 cap with a fixed bottom-to-top
   scan starves cells late in the order whenever more than 4096 are active; the island still hits
   the cap after settling (5-11k cells active: real river flow). An active-cell list processed in
   rotation would give every cell its turn.
3. **Old saves:** hills-water saves made before f444e432 keep their old rivers (sources along the
   river, inland ends): they still flood slowly. New worlds only.
4. **Inspector mismatch:** `--inspect --cell` says "Walkable: YES" for cells in deep water (or on a
   flooded ramp) that the game treats as not walkable.
5. **Flaky `test_terrain`** "connectivity_fix_preserves_largest": varied run to run at 035ff9f8 on
   an 8x8 map (ASan/UBSan clean, so probably an uninitialised read). Moot since f444e432 (no rivers
   under 24 cells); the test should set its own grid size and seed.
