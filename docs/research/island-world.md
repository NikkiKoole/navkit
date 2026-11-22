# Island Studio in navkit? (research, 2026-11-22)

Read-only look at dreamengine's Island Studio (`dreamengine/tools/carts/islandstudio.c`, design in
`dreamengine/docs/design/island-studio.md` and `island-buildings.md`) and what navkit would need
to make a world like it. No code changed.

## What Island Studio is

A bench cart that grows a finite **40x40 km island** on a 256 / 512 / 1024 grid (156 / 78 / 39 m
cells), in passes:

1. **Shape and relief**: warped radial falloff, rolling hills plus ridged mountains in an
   interior mask.
2. **Drainage**: priority flood (Barnes 2014): downstream per cell, flow accumulation, valleys
   carved along smoothed river lines, breach pass, lakes = pits deeper than 3 m.
3. **Climate**: prevailing wind, rain shadow from the highest ridge upwind.
4. **Cover**: beach, marsh, rock, moor (tree line), forest, scrub, grassland, from height, slope,
   rain and distance to water (chamfer distance transforms).
5. **Settlements**: suitability = flat x low x near fresh water/coast x fertile; greedy placement
   with spacing; size sets town / village / hamlet; names from a per-seed flavour.
6. **Roads**: A* over a cost field (grade, cover, jitter, merge discount, bridges): coast ring,
   Gabriel graph between places, classes from traffic, expressway, farm/forestry/upland tracks.
   Then simplify + spline, junction consolidation, hairpins, graded heights, bridges, flyovers,
   route numbers and signposts.
7. **On top**: houses along roads, churches, farms and barns, work sites (sawmill, quarry,
   harbour, distillery) at the ends of tracks, hedges, walls and trees.

Calibrated against the Isle of Man and Sado (road density, junction mix, kinks, grades). Exported
as OSM-shaped vector data (`.json` / `.rvb`) for backroads. It is built to be **driven**, not lived
in.

## The mismatch: scale

Assuming a navkit cell is about one person wide (~1 m; not written down anywhere found), navkit's
largest map is **2048x2048 cells x 16 z-levels**: about 2x2 km with 16 steps of height, and 2048
only fits in 8 GB thanks to sparse layers (`docs/todo/architecture/chunked-world-plan.md`).
Island Studio is 40x40 km with peaks over 600 m. A whole island at 1:1 is ~400x the cells, and the
relief doesn't fit 16 levels.

## Three routes

| Route | What | Effort |
|---|---|---|
| **A. Port the ideas at navkit scale** | A new "small island" terrain generator for 512-2048 cell maps: priority-flood drainage, rivers carved along lines, climate-driven cover, settlement suitability, A* roads | Medium. `GenerateHillsSoilsWater` (`src/world/terrain.c`) paints rivers/lakes from masks, no drainage; no settlement/road/house generators exist yet |
| **B. Import a slice of an export** | Read an islandstudio `.json`/`.rvb`, crop ~2x2 km: heightfield to z-levels, roads to floor, buildings to walls + rooms, cover to trees/soil | Medium. Format is documented, but the heightfield is 156 m per sample: detail must be invented |
| **C. Two-level world** | Island Studio as the overworld, navkit maps as local sites (DF-style embark) | Large: world layer, site picker, travel/events between sites. Only route that keeps "40 km to explore" |

## What navkit needs regardless

- **Height**: 16 z-levels makes hills a few storeys tall. Real valleys need a squashed vertical
  scale or a higher `MAX_GRID_DEPTH` (memory per cell).
- **A road/path layer**: roads are just floor cells today; road classes and a junction graph have
  nothing to map onto.
- **Settlement / building stamping**: constructed buildings and room detection exist, but nothing
  stamps a village. `GenerateCity` / `GenerateCouncilEstate` are the nearest starting points.
- **Water**: rivers must agree with the water sim (static standing water, or a source-to-sea
  flow), or the sim drains them.
- **Travel**: walking around a village at 2 km scale is fine; crossing an island is not.
  (See `vehicles.md` for rafts/boats on the water sim.)

## Recommendation

Route A: a "small island" generator reusing the drainage, cover and settlement passes (simple
whole-map grid passes, easy to port to C), sized to a 512-1024 map and squashed into 16 levels.
Gets the "grown, not drawn" feel without fighting the scale. Keep route C as a later world-map idea.

## Status: route A, first rung built (2026-11-22)

`src/world/island_gen.c`, terrain **Island** (dev UI terrain list, console `terrain Island`),
scenario **island** (`--scenario island`, `scenarioSize` sets the map). Built: shape, relief,
priority-flood drainage, valleys carved along rivers, a slope cap, sea / lakes / rivers as real
water, rain shadow, cover (beach, marsh, rock, moor, forest, scrub, grassland), trees and berry
bushes by cover, ramps. Not yet: settlements, paths, buildings (islandstudio rungs 2b-3), loose
items (rocks, sticks) for survival, a game entry in `games.c`.

- **Map dump**: `ISLAND_MAP=map.png ./build/bin/path --headless --seed 3 --scenario island --ticks 0`
  writes a top-down cover map (the game's renderer dims deep levels, so it is hard to judge from).
- **Heights**: the sea surface is z 3 (sea floor to z 0); land uses z 3..14. Water tops sit at the
  solid top of their spill level, so the walk level is one above water everywhere.
- **Slope cap**: no land more than 0.9 level above a 4-neighbour before quantizing, so every step
  is one level and can take a ramp. Without it 15-40% of the land was cliff-locked.
- **Reachable dry ground** (scenario `printStats`, seeds 1-8, 256): 97-99%; the rest is mostly
  offshore islets and small pockets. Two causes of cut-off terraces, both fixed: the shared
  `ShouldPlaceRampAt` clusters ramps in noise blobs (a terrace edge inside one had no ramp at
  all), so the island scatters `rampDensity` per cell; and trees on ramp ends. The cut-off ground
  followed the forest, which grows on the windward side: with most seeds' wind from the north
  it looked like "the top of the island is cut off". Ramps are now only placed where the top end
  steps onto ground at the ramp's level, and no tree grows on or next to a ramp end.
- **Water**: 1.7 ms/tick at 256 (like the `world` scenario). Dry ground shrinks ~7% in the first
  3000 ticks as rivers spread over their valley floors, then ~1% more by 12000.
- The hills "fix tiny pockets" pass is skipped: it filled tree tops and islets with granite.
- **Drainage is randomised steepest descent** ("Rho8": a strictly lower neighbour picked at
  random, weighted by drop per distance). Plain D8 (the neighbour the flood came from) made
  streams on smooth slopes run in ruler-straight 45-degree lines.
- **Map size**: distances and widths (river and valley width, beach/marsh/forest bands, noise
  patch sizes, rain blur) are tuned on 256 and scale with the map (`cs` = map / 256), so 1024
  is the same island, sharper. 1024 (seed 3): 2.7 s world gen + graph, 311 MB peak, 4.1 ms per
  tick with 100 movers (water 2.4 ms), 98.5% of dry ground reachable.
- Still open: a few big rivers cross the coastal plain in straight lines where the slope cap
  turned steep ground into an even ramp; height stays 16 levels at any width (1024 is gentle).
