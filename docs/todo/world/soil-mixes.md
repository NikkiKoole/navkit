# Soil mixes: sandy clay, clay loam, silty loam ...

Status: partial (2026-11-27): the table, the saved mix layer (v127), moisture / fertility / trees /
descriptions read it, the field's "causes" soils make them. Next: the island, the farming soil view.

## Why

The field game (`src/world/field_gen.c:415-423`) gives each cell one of five soils by noise
thresholds: loam (`MAT_DIRT`), clay, sand, gravel, peat. The edges are hard: loam next to clay,
nothing in between. Real ground is a mix that changes gradually (loam -> clay loam -> clay), and
the mix decides how wet it stays, how fertile it is and what grows on it. That's what the field
and the moisture layer (`soil-moisture.md`) should show.

Salt (salty loam) is a different axis, not a texture: later, with the island and the tide.

## Decision: keep the 5 materials, add a mix on top

**Not** a `MAT_` per mix: every material check (clay alone is tested in 13 files), the save
format, the dreamengine mirror (`project-dreamengine-mirror`) and the items you dig would all
grow with it.

Instead a **soil mix** per surface soil cell: one byte, an index into a table of named mixes
(the soil texture triangle). The cell's material stays the *dominant* soil, which is what all
existing code reads and what you get when you dig (sandy clay is still `MAT_CLAY`, still clay
items, still bricks). Code that wants the finer answer asks the mix.

| mix | base material | sand / silt / clay (rough) | character |
|---|---|---|---|
| loamy sand | SAND | 80 / 10 / 10 | drains fast, poor |
| sandy loam | DIRT | 65 / 20 / 15 | drains well, fair |
| loam | DIRT | 40 / 40 / 20 | the default, good |
| silt loam | DIRT | 20 / 65 / 15 | holds water, fertile (stream banks) |
| clay loam | DIRT | 30 / 35 / 35 | holds water, fertile, slow to soak |
| sandy clay | CLAY | 50 / 5 / 45 | heavy, cracks when dry |
| silty clay | CLAY | 5 / 45 / 50 | wet, sticky |
| clay | CLAY | 20 / 20 / 60 | waterlogs, slow |
| peaty loam | PEAT | organic | wet, rich |
| peat | PEAT | organic | soaked, very rich, fuel |
| gravelly loam | GRAVEL | + stones | dry, thin |
| gravel | GRAVEL | stones | barren |

Mix 0 = "just the material" (what every cell has today), so old saves and every other
generator keep working unchanged.

Each table row: name, base material, moisture capacity / soak / dry (today `soilDefs[]` in
`src/simulation/moisture.c`, per material), fertility (today `InitialFertilityForSoil` in
`src/simulation/farming.c`), grass vigour, tree preference (today `PickTreeTypeForSoil` in
`groundwear.c` and `PickTreeTypeForSoilSimple` in `terrain.c`), a colour hint for the grass
ground (`GrassGroundColor` in `rendering.c`).

## Steps

1. **The table + the layer** (save version bump): `soilMixGrid`, one byte per cell like
   `moistureGrid` (`LAYER_DEF` in `grid_layers.c`); `SoilMixOf(x, y, z)` returns the row (mix 0
   -> the material's default row). Saved; old saves load as 0.
2. **Readers switch to the row**: moisture (capacity, soak, dry), fertility, grass regrowth,
   tree pick, grass ground colour, the cell description ("Sandy clay, moist"). With every cell
   at mix 0 nothing changes: check with `make scenarios-check` and identical screenshots.
3. **The field generator makes gradients**: smooth noise for sand, clay and organic fractions
   (organic and clay up near water, silt along the stream: the open item in `field.md`), pick
   the nearest mix; base material from the mix. The hard edges become bands.
4. **Digging and building**: digging returns the base material's item (unchanged); a dug cell
   that is filled back in (or a built dirt wall) gets mix 0. Later maybe: sandy clay gives clay
   with sand mixed in (fewer bricks).
5. Later: island and survival terrain use it; salt as its own layer near the sea.

## Open questions for the user

- How many mixes: the full list above (~12) or start with ~6 (sandy loam, loam, silt loam,
  clay loam, sandy clay, clay) and keep peat / gravel / sand as they are?
- Should mixes show beyond the grass colour (bare soil sprites per mix, or a tint)?

## Prototype built (2026-11-27)

- `src/world/soil_mix.h/.c`: the table above (name, base material, moisture, fertility, a quiet
  hint colour, a loud view colour), `soilMixGrid` (one byte per cell, `grid_layers.c`) that belongs
  to the world it was made for (`gridGeneration`) and is **not saved**: after a load every cell
  is its material's plain mix. `SoilMixAt` also gives none once the material changed (dug, filled).
- Setting `fieldSoils`: `patches` (default, unchanged: byte-identical screenshot) or `causes`
  (`FieldSoilsByCauses` in `field_gen.c`): rock (slow noise, sandy to clay areas), height (the map's
  range + the cell against its 7x7 neighbourhood: rises sandy and stony, hollows clay), water (silt
  on stream banks, clay round ponds, organic where wet and low), a little small noise; then
  `SoilMixClassify`. The mix's base material becomes the cell's material; grass height is nudged
  by the mix's fertility, starting moisture comes from the mix.
- Drawing: grass ground hint from the mix; bare ground with a non-plain mix gets a faint veil;
  setting `soilView` colours every soil cell by its mix (also deeper levels).
- Compare: `./build/bin/path --game field --set fieldSoils 1` (0 = patches), console `set soilView 1`.
  Scenario `field --set fieldSoils 1 --check` passes (same 3 cut-off cells as patches).

## Made real (2026-11-27, save v127)

- Saved: `soilMixGrid` after the pipe layer (`V127_SOIL_MIX`; `--inspect` skips it); older saves
  load with no mixes. Every new world clears it (`ClearSoilMixes` in `InitGridWithDimensions`), so
  the `gridGeneration` trick of the prototype is gone.
- The table now also holds `capacity` / `soak` / `dry` (moisture.c reads the cell's row through
  `SoilMixDefAt` instead of its own per-material table) and `tree` (saplings from seed in
  groundwear.c, field trees). Plain rows carry the old per-material numbers, so worlds without
  mixes behave exactly as before (`make scenarios-check` all pass, patches field byte-identical).
- Tilled plots start with the mix's fertility (`CompleteFarmDesignation`).
- Descriptions: "Soil: Silt loam, Moist (45%)", "Farm Plot (Clay loam)"; plain dirt reads "Loam".
- Tests: `test_farming` "soil_mixes" (plain rows = old numbers, classify, material change, tilled
  fertility, rain worth more in sandy loam, save / load, new world clears).
- Not yet: other generators (island, survival terrain); the soil view as a farming tool (auto on
  with the farm plot tool, fertility colouring); building / digging keeps "plain" (a filled cell
  reads its material's plain row).
