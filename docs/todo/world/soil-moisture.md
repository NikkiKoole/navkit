# Soil moisture: water soaks into the ground, plants and sun take it out

Research 2026-11-26. Status: idea, nothing built. Pairs with "Soaking" in `water-ideas.md`
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
3. Moisture layer + soaking + drying, wetness derived from it (1, 3, 4, 5).
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

