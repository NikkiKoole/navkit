# Soil moisture: water soaks into the ground, plants and sun take it out

Research 2026-11-26. Status: partial. Steps 1-3 built (measuring scenario, evaporation per surface, the moisture layer) and rain falls as water; next: grass and crops react. Pairs with "Soaking" in `water-ideas.md`
and the open "soaking sink" in `water.md`.

The user's ask: water on grass / open ground disappears too fast. Let the top soil absorb it,
let vegetation and evaporation take it out of the soil, let a concrete basin hold water
longer, show the ground as soaked / wet / humid / normal / dry / very dry, and tie it into
crops (a plant can be dead dry, very wet, anything in between).

## How it works today

**Evaporation** (`WaterEvaporateSweep`, `src/simulation/water.c`): every `waterEvapInterval`
(4 game hours) every level-1 water cell has a 50% chance to be deleted. That is all. So:

- It doesn't look at what the water sits on: a film on grass, sand or concrete dries the same.
- It doesn't look at sun, temperature, wind, roof: indoors evaporates as fast as outdoors.
- Only level 1 ever evaporates. Water flows flat, so a spilled puddle spreads into a level-1
  film and halves every 4 GH: with `dayLength` 360 (1 GH = 15 game-s) a puddle is ~98% gone
  within one game day (6 sweeps). That is the "too fast" the user sees.
- A level-2+ pond with no level-1 edge (a concrete basin, a closed pit) **never** evaporates.
  So it's too fast for thin water and zero for deep water at the same time.
- The water vanishes: it doesn't go into the soil.

**Soil wetness** already exists: 2 bits in `cellFlags` (0 dry, 1 damp, 2 wet, 3 soaked),
`GET/SET_CELL_WETNESS`, only on natural soil (dirt, clay, sand, gravel, peat).

- Set by: water standing on it (`wetnessSyncInterval` pass at the end of `UpdateWater`: level 1
  -> damp, 2-3 -> wet, 4+ -> soaked; only raises), rain (`weather.c`), the watering job
  (`WATER_POUR_WETNESS` = 2), Town's farms.
- Dried by: the ground-wear sweep (`groundwear.c`, every 2 GH): 50% chance of -1 when no water
  is on it, extra with wind. Soaked -> dry in ~16 GH. Same for every soil, grass or not.
- Read by: mud (wetness >= 2 slows walking, drawn as a brown overlay), fire and smoke (wet
  doesn't burn), crops (`CropWetnessModifier`: dry 0.3x, damp 0.7x, wet 1.0x, soaked 0.5x).
- One-way: water makes soil wet, but the soil never takes any water. Wetness is a picture of
  "water was here", not a store.
- Vegetation ignores it: grass grows back from wear only; reeds grow next to water.
- `cellFlags` is full (8 of 8 bits used), so finer moisture needs its own layer.

## Measured (scenario `soak`, 2026-11-26)

`./build/bin/path --headless --scenario soak --ticks 21700` (6 game days, no rain): six walled
4x4 basins, one row with a puddle (level 1, 16 units), one with a pond (level 4, 64 units).

```
puddle  GH   grass     dirt      sand      clay      concrete  roofed      (units / wetness 0-3)
         0   16 / 0.0  16 / 0.0  16 / 0.0  16 / 0.0  16 / 0.0  16 / 0.0
         4    8 / 0.8   7 / 0.5   9 / 0.7   6 / 0.9   4 / 0.0   6 / 0.0
         8    3 / 0.3   4 / 0.4   3 / 0.4   3 / 0.7   3 / 0.0   4 / 0.0
        16    0 / 0.2   3 / 0.3   0 / 0.1   2 / 0.2   1 / 0.0   2 / 0.0
        32    0 / 0.0   0 / 0.0   0 / 0.0   0 / 0.0   0 / 0.0   0 / 0.0
pond     0   64 / 0.0 ... 
       144   64 / 3.0  64 / 3.0  64 / 3.0  64 / 3.0  64 / 0.0  64 / 0.0   (never changes)
```

- A puddle halves in 4 GH and is gone in ~1 game day, on every ground alike, roofed included.
- The soil under it only gets damp (avg <= 0.9) and is dry again ~1 day later.
- A pond of level 4 loses nothing in 6 days, on soil or concrete; the soil under it stays soaked.

## Proposal: moisture as a store

One number per soil surface cell, `moisture` 0-255, that holds real water.

**1. Soaking in (infiltration).** Water standing on natural soil moves into the soil below it,
1 water unit (1/7 cell) = some moisture points, at a rate and up to a capacity set by the
material:

| Soil | Soaks in | Holds (water units) | Note |
|---|---|---|---|
| sand, gravel | fast | ~1 | drains, dries fast |
| dirt | medium | ~2 | |
| clay | slow | ~2.5 | a clay bed holds a pond |
| peat | medium | ~4 | sponge, stays wet |
| stone, concrete, any built floor | none | 0 | water stands, only evaporates |

Capacity is the key to the parked worry "soak too fast and rivers vanish": the soil is a
bucket, not a drain. A river bed fills its bucket once and then stops taking water. Only the
slow sinks below drain it continuously. (No deep percolation in v1; maybe later, very slow.)

A 1-unit puddle on dry grass is gone into the ground within the hour; a flood of 7 soaks ~2
units in and the rest stands until it soaks or evaporates.

**2. Evaporation from open water, per surface, any depth.** Replace "50% of level-1 cells die"
with: each top water cell loses 1 unit with a small chance per sweep, scaled by exposure
(sky-exposed vs roofed), temperature, sun (day/night), wind. Deep water evaporates too, but
slowly: a concrete basin loses about one level every few sunny days; indoors almost nothing.
The sea / tide water stays exempt as now.

**3. Moisture leaves the soil** (in the ground-wear sweep that already visits every cell, and
already spreads itself over the interval on big maps):

- **bare soil evaporation**: fast on bare / tilled / trampled soil, slower under grass (shade,
  mulch), much slower under a roof; temperature, sun, wind scale it.
- **transpiration**: vegetation drinks. Tall grass more than short, crops by growth stage,
  trees from the cells round them. So plants dry the soil, but also shade it: a meadow stays
  humid longer than a bare field after rain, and loses it steadily in a drought.
- optional: slow sideways spread between neighbouring soil cells, so a riverbank is a humid
  band (reeds, lush grass) fading out to normal ground.

**4. Inputs**: water soaking (1), rain (moisture + instead of wetness +1), the watering job
(adds a fixed amount of water to the soil instead of setting "wet"), and later irrigation.

**5. What the ground reads** (describe line on the cell, tooltip, tint):

| moisture | label | effects |
|---|---|---|
| top | Soaked | mud: slow walking, brown overlay (as now); standing water stays |
| high | Wet | mud (as now) |
| mid-high | Moist / humid | |
| mid | Normal | |
| low | Dry | grass yellows, fire spreads easier |
| bottom | Parched | cracked, grass dies back to dirt |

`GET_CELL_WETNESS` becomes "moisture bucketed into 0-3" so mud, fire, smoke and the old crop
code keep working unchanged in the first step; the 2 flag bits free up afterwards.

**6. Plants.** Grass: regrowth needs at least "dry", browns when parched, dies to dirt after
a long drought, reeds where it stays soaked. Crops: a growth curve over moisture with an
optimum per crop (rice wet, wheat normal), plus a slow health value: long parched -> wilting
-> dead; long soaked -> rotting. That gives the "dry dead ... very wet" range for farming and
makes watering, irrigation ditches and drainage real jobs.

## Numbers to aim for (to calibrate with a scenario)

- 1-unit puddle on dry grass: soaked in within ~1 GH. On concrete: dries in ~1 day of sun.
- Full concrete basin (level 7) outdoors: down ~1 level per 2-4 sunny days; indoors: weeks.
- Wet soil -> normal in ~1-2 days; normal -> parched in ~a week without rain in summer;
  under grass about half as fast as bare.
- A spring-fed pond in a closed basin settles where inflow = evaporation + transpiration
  around it (instead of growing forever, see `water.md`).

## Costs and risks

- Memory: one byte per cell like `wearGrid` / `vegetationGrid` (dense today). Check with the
  big-worlds work (`chunked-world-plan.md`, block palettes) whether surface-only storage fits.
- Save version bump; old saves map their 2-bit wetness onto moisture.
- Water is no longer conserved by "vanish": it moves into soil, then leaves through
  evaporation/plants. Easier to reason about, and `--validate` activity counters must count
  soaking as a change.
- Perf: soaking runs in the existing wetness sync pass over the sparse water layer; drying in
  the existing ground-wear sweep. No new full-map pass.
- Balance coupling: island rivers, tide beaches (keep the sea exempt), Town farms (set wetness
  2 directly today), dollhouse (no soil, unaffected).

## Suggested order

1. ~~Measure first~~ done: scenario `soak` (see Measured).
2. ~~Evaporation per surface and depth (2)~~ done 2026-11-26, see "Step 2 built".
3. ~~Moisture layer + soaking + drying, wetness derived from it (1, 3, 4, 5)~~ done 2026-11-26, see "Step 3 built".
4. Grass reacts to moisture (6, grass part).
5. Crops: curve, health, wilting/rot; watering job adds moisture (6, crops part).

## Step 2 built (2026-11-26): evaporation per surface, any depth

`WaterEvaporateSweep` (`water.c`): every `waterEvapInterval` (sweep, 4 GH) each **top** water cell
(no water above it) loses 1 unit with chance `waterEvapInterval / waterEvapHours` x factor.
Factor: temperature / 20 C (0 at or below freezing, max 2), x (1 + 0.25 x wind) under the sky
with weather on, x 0.1 under a roof (`IsExposedToSky`). Setting `waterEvapHours` (default 48,
not saved): game hours for an open surface at 20 C to lose one level. Skipped: sources, frozen
water, the tide's sea/surf, and with `waterEdgeOutflow` everything at or below `waterEdgeSeaZ`
(the island's sea is not marked `isSea` when the tide is off: it evaporated 31k units in 2 days
before that rule). Tests in `test_water.c` (open vs roofed, only the top of a column, not at
freezing, not the sea). Headless runs now print `Water per z-level (units, sea apart)`.

`soak` after (puddle row / pond row, units left):

```
GH     grass  dirt  sand  clay  concrete  roofed        pond: grass dirt sand clay concrete roofed
 0      16    16    16    16     16        16                 64    64   64   64    64      64
24       7     9    10     9     12        16                 57    55   58   56    57      64
48       3     6     3     7      7        15                 47    43   47   49    53      62
```

A pond in the open loses about one level per 2 days; under a roof almost nothing; a puddle lasts
2-3 days instead of halving every 4 GH. Soil makes no difference yet (that is step 3).

Island seed 1 (`--game island --seed 1`), water units by z, old -> new: the sea (z1-3) unchanged,
rivers and lakes above z4 about the same, **lowland z4 1.5k -> 5.1k**, settled (day 1 2.9k, day 3
5.2k, day 4 5.1k): shallow films on the coastal flats no longer vanish in a sweep. Soaking
(step 3) should take most of that film into the soil.

## Step 3 built (2026-11-26): the moisture layer (save v124)

`src/simulation/moisture.c/.h`. `moistureGrid`: a byte per cell (dense, like `wearGrid`), on natural
soil cells with open space above: how full the soil is, 0-255. Labels (`SoilMoistureState`):
parched < 26 <= dry < 64 <= normal < 128 <= moist < 180 <= wet < 230 <= soaked. The 2-bit wetness
is now a bucket of it (moist 1, wet 2 = mud, soaked 3), written by `SetSoilMoisture`, so mud,
fire, smoke and crops read it unchanged. Cell describe line: "Soil: Moist (62%)".

- **Soaking** (the old wetness sync pass in `water.c`, every `wetnessSyncInterval` 0.8 GH): water
  standing on soil soaks one unit in with a chance per soil (sand/gravel 1.0, dirt/peat 0.5, clay
  0.15, x `moistureSoak`), worth 255 / capacity points (capacity in water units: sand/gravel 1,
  dirt 2, clay 2.5, peat 4). A full soil takes nothing more. Counter `soilSoakedUnits`.
- **Drying** (`UpdateSoilMoisture`, its own sweep every 2 GH, levels interleaved, spread over the
  interval on big maps): toward a floor, "normal" (100) up to 20 C, 0 at 35 C (heat dries ground
  out; groundwater keeps temperate ground normal). Half-life `moistureDryHours` (36 GH) at 20 C on
  open bare dirt, x temperature / 20, x wind, x 0.1 under a roof, x soil (sand 2, clay 0.6, peat
  0.5), x vegetation (grass shades the soil but drinks: bare 1.0, short grass 0.8, grass 0.85,
  tall 0.9, taller 1.0, reeds 1.3). Below the floor it creeps back up at a quarter of that speed.
- **Wetting**: rain and snow melt +40 per step (`WetSoilAt`), watering and Town irrigation at least
  200 (`SoilWetAtLeast`, `WATER_POUR_MOISTURE`).
- **A new world** sets itself up on first use (`gridGeneration`): soil under water full, the rest
  normal (old saves: from their wetness bits). Saves v124 store the layer after the place layer.
- Settings: `soilMoisture`, `moistureDryHours`, `moistureSoak` (not saved).

`soak` (now at a steady 20 C) after step 3, units left / soil moisture %:

```
puddle  GH   grass     dirt      sand      clay      concrete  roofed
         0   16 / 39   16 / 39   16 / 39   16 / 39   16 / 0    16 / 0
        12    0 / 79    0 / 79    0 / 78    0 / 73   15 / 0    16 / 0
        48    0 / 59    0 / 58    0 / 49    0 / 61    7 / 0    16 / 0
       144    0 / 42    0 / 42    0 / 39    0 / 46    1 / 0    16 / 0
pond     8   46 / 96   46 / 99   47 / 100  40 / 83   63 / 0    63 / 0
       140   14 / 97    7 / 82   10 / 89    2 / 73   17 / 0    60 / 0
```

A puddle on soil is in the ground within half a game day and leaves wet (muddy) ground that is
back to normal in ~5 days (sand fastest, clay slowest); on concrete it stands for days; roofed it
stays. Ponds fill their soil first (dirt takes ~1 unit per cell from normal, clay 2), then
evaporate as in step 2.

Island seed 1, 2 game days: lowland (z4) water 4.6k -> 3.3k units (the flats soak some up), rivers
and sea unchanged. Cost: 1024 world, 0.27 ms/tick average, worst 4.9 ms after the first tick
(the first tick sets a new world up: 48 ms on 1024); before interleaving the levels a sweep tick
hit 26 ms.

Not done yet: no tint for dry/parched ground (only the mud overlay and the describe line), no
sideways spread (a riverbank isn't a humid band), trees don't drink, grass and crops don't react
(steps 4-5).

## Rain falls as water (2026-11-26)

User: rain should add some water that reacts with the soil like other water, but not a lot,
to be tweaked. Before: weather rain did two unrelated things: wetness +1 per sweep on soil, and
sky water blobs (`SpawnSkyWater`: 5% of the map's cells per 30 game-s in rain, 30% in heavy rain,
4-7 units each) which flooded flat maps (the Town game turns weather off for that reason).

Now (`RainWetnessRow`, `weather.c`): each rain sweep every column open to the sky gets one water
unit with chance `rainWater` (0.5) / `heavyRainWater` (1.5) units per cell per game day x the
sweep interval x the weather's intensity. Where it lands:
- natural soil with room: soaks in at once by that soil's soaking chance (`RainSoaksIn`), so clay
  lets most of it stand;
- otherwise (full soil, natural rock, the top of standing water): one water unit, which runs off
  downhill, soaks in later or evaporates like any water;
- built ground, paving and roofs catch nothing (gutters), the sea neither.
The sweep has its own random generator (a roll per column must not shift `rand()`: the weather's
own sequence changed and test_thunderstorm's summer storm went missing). Weather rain drops no sky
blobs any more; the console `rain` command still does. Snow melt still wets the soil directly.

Scenario `rainfall` (the soak basins, no water, a day of steady rain, then dry; 20 C), soil
moisture % after the day (normal is 39): rainWater 0.5 -> ~53 (moist); 1 -> ~70 (moist, a few
puddles where drops met full soil, clay 53, rock hollow 10 units); 3 -> ~97-100 (soaked, puddles
everywhere).

Island seed 1 with weather on, starting in heavy rain (it then rained 4 days, heavy and light):
lowland (z4) water, units:

| | day 1 | day 2 | day 4 | day 6 (dry since day 5) |
|---|---|---|---|---|
| no rain | 1.8k | | | |
| rain 1 / heavy 3 | 41k | 105k | 76k | |
| rain 0.5 / heavy 1.5 (default) | 21k | 63k | 46k | 9.7k |

The flats flood in a long wet spell and dry within ~2 days after. Weather is still off in every
game (only scenarios `island-winter` and `heat` turn it on); tick cost with heavy rain on the
island 0.53 -> 0.57 ms.

