# 06b: Farming — Crops & Processing

> Split from 06-farming-and-crops.md on 2026-02-24.
> **Deps**: 06a (farm grid, tilling, watering, fertilizing, compost all exist)
> **Opens**: 07 (clothing — flax fiber ready for loom), 09 (loop closers — compost demand drives waste recycling)

---

## Goal

Add 3 stone-age crops (emmer wheat, lentils, flax), wild plant spawning, crop growth with full environmental integration, harvesting, seed cycling, and processing chains. After this feature, farming is a complete gameplay loop: find wild plants → harvest seeds → till → plant → tend → water → harvest → process → eat/use → replant.

This is where seasons become life-or-death. A late frost kills crops. A dry summer stunts wheat. Lentils restore soil. Flax makes fiber for clothing. The player must plan rotations, time plantings, and manage resources across the year.

---

## Crop Types

Three crops chosen for stone-age Western Europe / Africa authenticity and gameplay variety:

| Crop | Type | Season | Growth Time | Yield | Soil Effect | Chain Length |
|------|------|--------|-------------|-------|-------------|-------------|
| Emmer Wheat | Annual grain | Spring–Autumn | 72 game-hours (~3 game-days) | 4× ITEM_WHEAT + 1× ITEM_WHEAT_SEEDS | Depletes fertility on harvest (-20) | 4 steps (harvest → quern → flour → bake → bread) |
| Lentils | Annual legume | Spring–Summer | 48 game-hours (~2 game-days) | 3× ITEM_LENTILS + 1× ITEM_LENTIL_SEEDS | **Restores** fertility on harvest (+60) | 3 steps (harvest → hearth → cooked lentils) |
| Flax | Annual fiber | Spring–Summer | 60 game-hours (~2.5 game-days) | 2× ITEM_FLAX_FIBER + 1× ITEM_FLAX_SEEDS | Depletes fertility on harvest (-15) | 2 steps for now (harvest → fiber → stockpile for 07) |

**Design rationale:**
- **Emmer wheat**: Long chain, staple food, drives quern workshop construction. The "main" crop.
- **Lentils**: Fast, forgiving, restores fertility. The strategic rotation crop — plant lentils after wheat to restore soil. Also decent food.
- **Flax**: Non-food crop, bridges to clothing (07). Shorter chain for now, expands when loom arrives.

### Growth Timing Analysis (first sketch — tune in playtesting)

Assumptions: `dayLength=120s` (survival), `daysPerSeason=7`, so 1 game-hour = 5 real seconds.

| Crop | Ideal growth | Real time | Realistic (~1.3x) | Fraction of season |
|------|-------------|-----------|-------------------|-------------------|
| Wheat | 72 game-hours (3 days) | 6 min | ~100 game-hours | 43% → 60% |
| Lentils | 48 game-hours (2 days) | 4 min | ~65 game-hours | 29% → 39% |
| Flax | 60 game-hours (2.5 days) | 5 min | ~80 game-hours | 36% → 48% |

Growing season = spring + summer = 14 days = 336 game-hours.

**Harvests per year (realistic conditions):**
- Wheat: ~3 harvests (plant spring → autumn, 0.8x autumn slows last cycle)
- Lentils: ~5 harvests (spring+summer only, fast turnaround)
- Flax: ~4 harvests (spring+summer only)

**Key timing dynamics:**
- First season is mostly bootstrap — find wild plants, harvest seeds, plant late
- Wheat survives autumn (0.8x growth) — can squeeze in a late harvest before winter kill
- Lentils/flax die at autumn transition → last planting deadline is ~summer day 5
- Realistic modifiers (soil quality, some weeds, imperfect weather) add ~30% to growth time
- All values are tunable constants — adjust after playtesting the feel

**Pace feel:** With 3-day wheat, a farmer is planting/harvesting every few days — busy but not frantic. Lentils being faster reinforces their "easy crop" role. The planting deadline before autumn creates natural seasonal tension.

---

## Crop Growth System

### Per-Cell Crop Data

Extends the `FarmCell` from 06a:

```c
typedef struct {
    // From 06a:
    uint8_t  fertility;
    uint8_t  weedLevel;
    uint8_t  tilled;
    uint8_t  desiredCropType; // what player wants planted (from 06a)
    // New in 06b:
    uint8_t  cropType;        // CROP_NONE, CROP_WHEAT, CROP_LENTILS, CROP_FLAX
    uint8_t  growthStage;     // 0=bare, 1=sprouted, 2=growing, 3=mature, 4=ripe
    uint8_t  frostDamaged;    // hit by frost, yield reduced
    uint8_t  _pad;            // alignment
    uint16_t growthProgress;  // 0-65535 maps to 0.0-1.0 (avoids float)
} FarmCell;
// Total: 10 bytes per cell. 128×128×32 = ~5MB. Single grid for all farm state.
```

### Growth Rate Formula

Each farm tick, growth advances:

```c
float rate = BASE_GROWTH_RATE
    * seasonModifier(season, cropType)     // per-crop seasonal preference
    * temperatureModifier(cellTemp)        // too cold or too hot = 0
    * wetnessModifier(cellWetness)         // too dry or waterlogged = penalty
    * fertilityModifier(fertility)         // low fertility = slow growth
    * weedModifier(weedLevel);             // weedy = slow growth

growthProgress += rate * tickInterval;
if (growthProgress >= 1.0f) {
    growthProgress = 0.0f;
    growthStage++;
}
```

### Modifier Details

**Season modifier** (per crop):

| Crop | Spring | Summer | Autumn | Winter |
|------|--------|--------|--------|--------|
| Wheat | 1.0 | 1.5 | 0.8 | 0.0 (dies) |
| Lentils | 1.2 | 1.0 | 0.0 (dies) | 0.0 (dies) |
| Flax | 1.0 | 1.2 | 0.0 (dies) | 0.0 (dies) |

Lentils and flax die in autumn/winter — must be harvested by end of summer. Wheat is hardier, surviving into autumn.

**Temperature modifier:**

```c
float temperatureModifier(float temp) {
    if (temp < FROST_THRESHOLD)  return 0.0f;  // freezing: no growth
    if (temp < COLD_THRESHOLD)   return 0.3f;  // cold: slow
    if (temp < WARM_THRESHOLD)   return 1.0f;  // ideal range
    if (temp < HOT_THRESHOLD)    return 0.5f;  // hot: stressed
    return 0.2f;                                // extreme heat: nearly stopped
}
```

Thresholds (in game temperature units, tunable):
- FROST_THRESHOLD: 0°C — crops freeze, frost damage flag set
- COLD_THRESHOLD: 5°C
- WARM_THRESHOLD: 25°C
- HOT_THRESHOLD: 35°C

**Wetness modifier** (uses existing `GetCellWetness()`):

| Wetness Range | Modifier | Description |
|---------------|----------|-------------|
| 0–20 | 0.3 | Parched — severe drought stress |
| 20–40 | 0.7 | Dry — needs watering |
| 40–80 | 1.0 | Ideal — well-watered |
| 80–100 | 0.7 | Wet — starting to waterlog |
| 100+ | 0.3 | Waterlogged — roots drowning |

Clay soil naturally stays wetter (from existing mud system) — makes clay risky in rainy seasons.

**Fertility modifier** (from 06a):
```c
float fertilityModifier(uint8_t fertility) {
    return 0.25f + 0.75f * (fertility / 255.0f);
    // fertility 255 = 1.0x, fertility 128 = 0.63x, fertility 0 = 0.25x
}
```

**Weed modifier** (from 06a):
```c
float weedModifier(uint8_t weedLevel) {
    if (weedLevel < WEED_THRESHOLD)  return 1.0f;   // < 128: no penalty
    if (weedLevel < WEED_SEVERE)     return 0.5f;    // 128-200: moderate penalty
    return 0.25f;                                     // > 200: severe penalty
}
```

### Growth Stages

| Stage | Name | Duration (fraction of total) | Visual |
|-------|------|------------------------------|--------|
| 0 | Bare | — | Tilled soil (from 06a) |
| 1 | Sprouted | 0.2 | Small green shoots |
| 2 | Growing | 0.3 | Medium plant |
| 3 | Mature | 0.3 | Full-size plant |
| 4 | Ripe | 0.2 | Golden/colored, ready to harvest |

Each stage requires `growthProgress` to reach 1.0 (65535 in uint16_t) before advancing. Total growth time in ideal conditions (all modifiers = 1.0) equals the crop's base growth time in game-hours. BASE_GROWTH_RATE per crop = 4.0 stages / totalGameHours, so each tick adds `rate * tickInterval` to progress.

### Frost Damage

When temperature drops below FROST_THRESHOLD on a cell with crops at stage 1+:
- Set `frostDamaged = true`
- Frost-damaged crops yield 50% output on harvest
- Crops at stage 0 (bare/seed) are not affected (seeds survive frost)
- Visual: brownish tint on frost-damaged crops

### Winter Kill

When season transitions to winter:
- All crops with season modifier 0.0 for winter are killed (stage reset to 0, cropType = CROP_NONE)
- Lentils/flax die at autumn transition if still planted
- Wheat survives autumn but dies at winter
- Event log: "X crops killed by seasonal change"

---

## Wild Plants (Seed Source)

All three crops occur as wild plants in worldgen, providing the initial seed bootstrap:

| Wild Plant | Biome | Rarity | Found On |
|------------|-------|--------|----------|
| Wild wheat | Grassland, hills | Common | MAT_DIRT, MAT_CLAY |
| Wild lentils | Rocky slopes, clay | Uncommon | MAT_CLAY, MAT_GRAVEL |
| Wild flax | Riverbanks, wetland | Uncommon | MAT_DIRT, MAT_PEAT (near water) |

**Implementation**: Extend the existing `Plant` system (currently only PLANT_BERRY_BUSH):

```c
typedef enum {
    PLANT_BERRY_BUSH,
    PLANT_WILD_WHEAT,
    PLANT_WILD_LENTILS,
    PLANT_WILD_FLAX,
    PLANT_TYPE_COUNT
} PlantType;
```

Wild plants:
- Spawn during terrain generation based on soil material and proximity to water
- Have the same growth stages as farm crops (start at RIPE in worldgen)
- Can be harvested with a new `DESIGNATION_HARVEST_WILD` or reuse `DESIGNATION_HARVEST_BERRY` extended
- Harvesting a wild plant yields 1× seed item (not full crop yield — just enough to start farming)
- Wild plants regrow slowly (seasonal, like berry bushes)

**Fog of war integration**: Wild plants in unexplored areas are invisible. Exploring new territory may reveal wild wheat or flax — rewarding exploration with new crop types.

**Plant pool capacity**: Scattering wild plants at 1-2% density on a 128×128 map could spawn 200-300 plants. Check that MAX_PLANTS is large enough (currently sized for berry bushes only). May need to bump or cap wild plant spawning to ~100 total across all 3 types.

---

## New Jobs

### JOBTYPE_PLANT_CROP

Mover plants a seed in a tilled farm cell.

| Property | Value |
|----------|-------|
| Work time | 0.3 game-hours |
| Requires | Seed item (ITEM_WHEAT_SEEDS, ITEM_LENTIL_SEEDS, or ITEM_FLAX_SEEDS) |
| Target | Tilled farm cell with cropType == CROP_NONE |
| Result | Seed consumed, cropType set, growthStage = 0, growthProgress = 0 |

**How does the mover know what to plant?** Each farm cell has `desiredCropType` (set during farm designation via sub-action, like soil type selection). All cells in a designated zone get the same desired crop type. When seeds are available, movers plant the desired type.

**WorkGiver_PlantCrop**: scan tilled farm cells where `desiredCropType != CROP_NONE` and `cropType == CROP_NONE` → find matching seed item → assign mover.

**Job steps:**
1. Walk to seed item, pick up
2. Walk to target farm cell
3. Work (0.3 game-hours)
4. On completion: delete seed item, set cropType + stage 0

### JOBTYPE_HARVEST

Mover harvests a ripe crop.

| Property | Value |
|----------|-------|
| Work time | 0.4 game-hours |
| Tool quality | QUALITY_CUTTING (optional, speeds up — using existing stone axe) |
| Target | Farm cell with growthStage == 4 (RIPE) |
| Auto-assigned | Yes |
| Result | Crop items + seed item spawned, cell reset to bare tilled |

**WorkGiver_Harvest**: scan farm cells with ripe crops → assign nearest mover.

**Job steps:**
1. Walk to ripe farm cell
2. Work (0.4 game-hours, cutting tool speeds up)
3. On completion: spawn yield items (see Yield Table), reset cell (cropType = CROP_NONE, stage = 0)
4. If crop was lentils: `farmGrid[z][y][x].fertility += LENTIL_FERTILITY_BONUS` (capped at 255)

### Yield Table

| Crop | Normal Yield | Frost-Damaged Yield | Seed Return |
|------|-------------|---------------------|-------------|
| Wheat | 4× ITEM_WHEAT | 2× ITEM_WHEAT | 1× ITEM_WHEAT_SEEDS |
| Lentils | 3× ITEM_LENTILS | 1× ITEM_LENTILS | 1× ITEM_LENTIL_SEEDS |
| Flax | 2× ITEM_FLAX_FIBER | 1× ITEM_FLAX_FIBER | 1× ITEM_FLAX_SEEDS |

Always returns 1 seed — the self-sustaining loop. Frost damage halves crop output but doesn't affect seed return (you can always replant).

### JOBTYPE_HARVEST_WILD

Mover harvests a wild plant for seeds. Reuses the pattern from JOBTYPE_HARVEST_BERRY.

| Property | Value |
|----------|-------|
| Work time | 0.4 game-hours |
| Designation | DESIGNATION_HARVEST_WILD (or extend existing berry designation) |
| Result | 1× seed item spawned, wild plant reset to BARE stage |

---

## New Items

| Item | Flags | Stack | Nutrition | Spoilage | Notes |
|------|-------|-------|-----------|----------|-------|
| ITEM_WHEAT_SEEDS | IF_STACKABLE | 20 | — | — | Planting material |
| ITEM_LENTIL_SEEDS | IF_STACKABLE | 20 | — | — | Planting material |
| ITEM_FLAX_SEEDS | IF_STACKABLE | 20 | — | — | Planting material |
| ITEM_WHEAT | IF_STACKABLE | 20 | — | — | Raw grain, not directly edible. Must be milled. |
| ITEM_LENTILS | IF_STACKABLE, IF_EDIBLE, IF_SPOILS | 20 | +0.15 | 480s | Raw legume, edible but low nutrition. Better cooked. |
| ITEM_FLAX_FIBER | IF_STACKABLE | 20 | — | — | Raw fiber. Stored until clothing (07). |
| ITEM_FLOUR | IF_STACKABLE | 20 | — | — | Milled wheat. Input for bread. |
| ITEM_BREAD | IF_STACKABLE, IF_EDIBLE, IF_SPOILS | 10 | +0.6 | 960s | Best food in game. Worth the long chain. |
| ITEM_COOKED_LENTILS | IF_STACKABLE, IF_EDIBLE, IF_SPOILS | 10 | +0.4 | 600s | Good food, easy to make. |

9 new item types. Save version bump required (item count 51 → 60, accounting for 06a's 2 items).

**Nutrition hierarchy after this feature:**

| Food | Nutrition | Chain Length | Notes |
|------|-----------|-------------|-------|
| Bread | +0.6 | 4 (wheat → mill → flour → bake) | Best food, hardest to make |
| Cooked meat | +0.5 | 3 (carcass → butcher → cook) | From hunting |
| Cooked lentils | +0.4 | 2 (lentils → cook) | Easy, sustainable |
| Roasted root | +0.35 | 2 (dig → roast) | From foraging |
| Berries | +0.3 | 1 (forage) | Baseline food |
| Dried berries | +0.25 | 2 (berries → dry) | Preserved |
| Raw lentils | +0.15 | 1 (harvest) | Emergency food |
| Raw meat | +0.2 | 2 (carcass → butcher) | Emergency |

Longer chains = better food. Bread is king.

---

## Quern Workshop

New workshop: **WORKSHOP_QUERN** (1x1)

| Property | Value |
|----------|-------|
| Template | `X` (work tile only, like butcher) |
| Type | Active (mover grinds by hand) |
| Construction | 2 rocks, 0.5 game-hours build time |

A quern is just two stones — one flat, one round. The simplest workshop.

**Recipe: "Mill Wheat"**

| Field | Value |
|-------|-------|
| Input | 2× ITEM_WHEAT |
| Output | 1× ITEM_FLOUR |
| workRequired | 1.0 game-hours |
| Tool | None (the quern IS the tool) |

**Recipe: "Mill Seeds"** (optional, lower priority)

| Field | Value |
|-------|-------|
| Input | 4× ITEM_FLAX_SEEDS |
| Output | 1× ITEM_FLAX_OIL |
| workRequired | 1.0 game-hours |

Flax oil is a future item (cooking fat, lamp fuel). Defer to later if scope is too big.

---

## Cooking Recipes

Add to existing **fire pit / ground fire** (`campfireRecipes[]`):

| Recipe | Input | Output | workRequired | passiveWorkRequired |
|--------|-------|--------|-------------|---------------------|
| Bake Bread | 1× ITEM_FLOUR | 1× ITEM_BREAD | 1.0f | 15.0f |
| Cook Lentils | 2× ITEM_LENTILS | 1× ITEM_COOKED_LENTILS | 0.5f | 10.0f |

Both use the semi-passive (ignite) pattern already established.

Add to **hearth** (`hearthRecipes[]`) for batch cooking:

| Recipe | Input | Output | workRequired | passiveWorkRequired |
|--------|-------|--------|-------------|---------------------|
| Bake Bread | 2× ITEM_FLOUR | 2× ITEM_BREAD | 2.0f | 0.0f |
| Cook Lentils | 4× ITEM_LENTILS | 2× ITEM_COOKED_LENTILS | 1.0f | 0.0f |

---

## Processing Chains (Full View)

```
WHEAT CHAIN (4 steps):
  Wild wheat → harvest → ITEM_WHEAT_SEEDS
  Plant seeds → grow → harvest → ITEM_WHEAT (×4) + ITEM_WHEAT_SEEDS (×1)
  ITEM_WHEAT → Quern → ITEM_FLOUR
  ITEM_FLOUR → Fire Pit → ITEM_BREAD (+0.6 nutrition)

LENTIL CHAIN (2 steps):
  Wild lentils → harvest → ITEM_LENTIL_SEEDS
  Plant seeds → grow → harvest → ITEM_LENTILS (×3) + ITEM_LENTIL_SEEDS (×1)
  ITEM_LENTILS → Fire Pit → ITEM_COOKED_LENTILS (+0.4 nutrition)
  BONUS: Soil fertility +60 on harvest (nitrogen fixation)

FLAX CHAIN (1 step for now):
  Wild flax → harvest → ITEM_FLAX_SEEDS
  Plant seeds → grow → harvest → ITEM_FLAX_FIBER (×2) + ITEM_FLAX_SEEDS (×1)
  ITEM_FLAX_FIBER → stockpile (→ Loom in 07 → ITEM_LINEN)
```

---

## Worldgen Integration

During terrain generation, scatter wild plants:

```c
// After terrain + vegetation generation:
for each surface cell (x, y, z) {
    if (!IsGrass(x, y, z)) continue;
    uint8_t soilMat = grid[z-1][y][x].wallMaterial;
    float roll = RandomFloat();

    // Wild wheat: 2% chance on dirt/clay in grassland
    if ((soilMat == MAT_DIRT || soilMat == MAT_CLAY) && roll < 0.02f)
        SpawnPlant(x, y, z, PLANT_WILD_WHEAT);

    // Wild lentils: 1% chance on clay/gravel on slopes
    else if ((soilMat == MAT_CLAY || soilMat == MAT_GRAVEL) && roll < 0.01f)
        SpawnPlant(x, y, z, PLANT_WILD_LENTILS);

    // Wild flax: 1.5% chance on dirt/peat near water
    else if ((soilMat == MAT_DIRT || soilMat == MAT_PEAT) && IsNearWater(x,y,z,3) && roll < 0.015f)
        SpawnPlant(x, y, z, PLANT_WILD_FLAX);
}
```

Wild plants spawn at RIPE stage so they're immediately harvestable. They regrow seasonally (spring → ripe by summer, like berry bushes).

---

## Sprites & Visuals

### New Tiles (11 for 06b)

All 8x8 PNGs in `assets/textures8x8/`. Minimal set — heavy reuse with tinting.

**Crop growth stages (4 shared sprites, tinted per crop):**

| Filename | Sprite Constant | Description |
|----------|----------------|-------------|
| `crop-sprout.png` | `SPRITE_crop_sprout` | Tiny green shoots — identical for all 3 crops at stage 1 |
| `crop-growing.png` | `SPRITE_crop_growing` | Mid-height green stalks — shared for all crops at stage 2 |
| `crop-mature.png` | `SPRITE_crop_mature` | Full-size plant — shared for all crops at stage 3 |
| `crop-ripe.png` | `SPRITE_crop_ripe` | Harvestable plant — tinted per crop at render time |

**Crop tints** (applied to shared growth sprites at ripe stage, and lightly at mature):

| Crop | Tint | Rationale |
|------|------|-----------|
| Wheat | Golden {220, 180, 80} | Classic wheat gold |
| Lentils | Brown-green {140, 120, 60} | Earthy legume pods |
| Flax | Pale blue {160, 170, 200} | Flax flower color |

Sprout and growing stages use plain GREEN for all crops (too small to distinguish). Mature stage gets a light tint. Ripe stage gets full crop-specific tint.

**Wild plant (1 shared sprite):**

| Filename | Sprite Constant | Description |
|----------|----------------|-------------|
| `plant-wild-crop.png` | `SPRITE_plant_wild_crop` | Messy/wild version of a crop plant — tinted per crop type. Visually distinct from neat farm sprites |

**Item sprites (5 new, rest reused):**

| Filename | Sprite Constant | Used For |
|----------|----------------|----------|
| `item-seeds.png` | `SPRITE_item_seeds` | All 3 seed types — tinted per crop (wheat=tan, lentil=brown, flax=dark) |
| `item-grain.png` | `SPRITE_item_grain` | ITEM_WHEAT + ITEM_LENTILS — tinted differently (wheat=golden, lentils=green-brown) |
| `item-flour.png` | `SPRITE_item_flour` | ITEM_FLOUR — white/cream, no tint needed |
| `item-bread.png` | `SPRITE_item_bread` | ITEM_BREAD — warm brown {190, 140, 70} |
| `workshop-quern.png` | `SPRITE_workshop_quern` | Quern workshop — two stacked round stones |

**Reused existing sprites:**

| Item | Reuses | Tint |
|------|--------|------|
| ITEM_FLAX_FIBER | `SPRITE_short_string` | Pale {200, 190, 170} |
| ITEM_COOKED_LENTILS | `SPRITE_item_grain` | Orange-brown {180, 120, 60} |
| Frost damage overlay | `SPRITE_light_shade` | White {220, 230, 255} at alpha 120 |
| Compost pile workshop | `SPRITE_finish_messy` | Dark brown {60, 45, 25} (reused from 06a ITEM_COMPOST) |

### Rendering Approach

Crop rendering follows the same pattern as `ItemSpriteForTypeMaterial()` — one sprite selected by growth stage, tinted by crop type:

```c
int CropSpriteForStage(uint8_t stage) {
    switch (stage) {
        case 1: return SPRITE_crop_sprout;
        case 2: return SPRITE_crop_growing;
        case 3: return SPRITE_crop_mature;
        case 4: return SPRITE_crop_ripe;
        default: return -1;  // bare = no crop sprite
    }
}

Color CropTint(uint8_t cropType, uint8_t stage) {
    if (stage < 3) return (Color){80, 160, 60, 255};  // generic green for early stages
    // Mature/ripe: crop-specific color
    switch (cropType) {
        case CROP_WHEAT:   return (Color){220, 180, 80, 255};
        case CROP_LENTILS: return (Color){140, 120, 60, 255};
        case CROP_FLAX:    return (Color){160, 170, 200, 255};
        default:           return WHITE;
    }
}
```

Crops render as inset sprites on tilled soil (like berry bushes render inset on their cell). Wild plants use `SPRITE_plant_wild_crop` with the same tint table.

### Total New Tiles: 13 (2 from 06a + 11 from 06b)

---

## Save/Load

Save version bump (on top of 06a's bump).

**Extended FarmCell** — the struct grows with cropType, growthStage, growthProgress, frostDamaged. Since 06a defines the struct and 06b extends it, both can be in a single save version if implemented together, or two sequential bumps if implemented separately.

New items handled automatically by item save/load. New plant types handled by existing plant save/load (PlantType enum extended).

---

## Implementation Phases

### Phase 1: Crop Data + Planting
- Extend FarmCell with cropType, growthStage, growthProgress, frostDamaged
- Add CropType enum (CROP_NONE, CROP_WHEAT, CROP_LENTILS, CROP_FLAX)
- Add seed items (ITEM_WHEAT_SEEDS, ITEM_LENTIL_SEEDS, ITEM_FLAX_SEEDS)
- Add farm zone crop selection (sub-action in farm designation: W → F → 1/2/3 for wheat/lentils/flax)
- Add JOBTYPE_PLANT_CROP with WorkGiver_PlantCrop
- Save version bump
- **Test**: Plant seeds in tilled cell, verify cropType and stage set correctly

### Phase 2: Growth System
- Add growth tick to FarmTick() — advance growthProgress per farm tick
- Implement all 5 modifiers (season, temperature, wetness, fertility, weeds)
- Stage progression (0 → 1 → 2 → 3 → 4)
- Frost damage mechanic
- Winter/seasonal kill
- Crop visuals per stage (sprites or tinted overlays)
- **Test**: Crop grows through stages in ideal conditions, stalls in bad conditions, dies in frost/winter

### Phase 3: Harvesting + Yield
- Add JOBTYPE_HARVEST with WorkGiver_Harvest
- Yield table (normal + frost-damaged)
- Seed return on harvest
- Lentil fertility bonus on harvest
- Add crop items (ITEM_WHEAT, ITEM_LENTILS, ITEM_FLAX_FIBER)
- Farm cell reset after harvest (tilled, ready for replanting)
- **Test**: Harvest ripe crop → correct items spawned, seed returned, fertility updated for lentils

### Phase 4: Wild Plants + Worldgen
- Extend PlantType enum with PLANT_WILD_WHEAT, PLANT_WILD_LENTILS, PLANT_WILD_FLAX
- Add wild plant spawning in terrain generation
- Add JOBTYPE_HARVEST_WILD (or extend berry harvest)
- Wild plant regrowth (seasonal)
- **Test**: Wild plant harvestable → gives seeds, regrows next season

### Phase 5: Quern + Cooking Recipes
- Add WORKSHOP_QUERN (1x1, active, 2 rocks construction)
- Add ITEM_FLOUR, ITEM_BREAD, ITEM_COOKED_LENTILS
- Add quern recipe: wheat → flour
- Add cooking recipes: flour → bread, lentils → cooked lentils (fire pit + hearth)
- Stockpile filter keys for all new items
- **Test**: Full chain — wheat → quern → flour → fire pit → bread, verify nutrition

---

## E2E Test Stories

### Story 1: Full wheat chain — seed to bread
- Till farm cell, plant ITEM_WHEAT_SEEDS
- Advance time through growth stages (spring/summer ideal conditions)
- Harvest ripe wheat → verify 4× ITEM_WHEAT + 1× ITEM_WHEAT_SEEDS
- Mill at quern → verify ITEM_FLOUR
- Bake at fire pit → verify ITEM_BREAD
- Mover eats bread → verify +0.6 nutrition

### Story 2: Lentils restore soil fertility
- Till farm cell, note initial fertility (128 on dirt)
- Plant wheat, grow, harvest → verify fertility decreased (128 - 20 = 108)
- Plant lentils, grow, harvest → verify fertility increased (108 + 60 = 168)
- **Verify**: Crop rotation strategy works — lentils compensate wheat depletion

### Story 3: Frost damage reduces yield
- Plant wheat in late autumn
- Temperature drops below FROST_THRESHOLD
- **Verify**: frostDamaged == true
- Harvest → verify 2× ITEM_WHEAT (half normal) + 1× ITEM_WHEAT_SEEDS (seed preserved)

### Story 4: Seasonal crop death
- Plant lentils in summer
- Advance to autumn
- **Verify**: lentil crop killed (stage reset to 0, cropType = CROP_NONE)
- Event logged

### Story 5: Drought stress
- Plant wheat on sandy soil (MAT_SAND, drains fast)
- No rain, no manual watering
- **Verify**: growth rate severely reduced (low wetness modifier)
- Water the cell manually
- **Verify**: growth rate recovers

### Story 6: Waterlogging on clay
- Plant wheat on clay soil (MAT_CLAY)
- Heavy rain for extended period
- **Verify**: wetness exceeds threshold, growth rate drops (waterlogging penalty)

### Story 7: Wild plant discovery and seed bootstrap
- Generate map with wild wheat
- Designate harvest on wild wheat
- Mover harvests → verify 1× ITEM_WHEAT_SEEDS (just seeds, not full crop yield)
- Plant those seeds on farm → verify crop starts growing
- **Verify**: Self-sustaining loop (harvest farm wheat → get seeds → replant)

### Story 8: Flax fiber stockpiling
- Plant flax, grow to ripe, harvest
- **Verify**: 2× ITEM_FLAX_FIBER spawned
- **Verify**: fiber is not edible, stockpiles correctly
- (Future: feeds into loom in 07)

### Story 9: Weedy farm slows crop growth
- Plant crop, let weeds accumulate (don't tend)
- **Verify**: growth rate reduced when weedLevel > WEED_THRESHOLD
- Tend the cell (clear weeds)
- **Verify**: growth rate returns to normal

### Story 10: Multiple growth modifiers stack
- Plant wheat in ideal conditions (spring, good temp, good wetness, full fertility, no weeds)
- Record growth rate
- Add penalties one at a time: bad season, cold temp, low wetness, low fertility, high weeds
- **Verify**: Each penalty reduces growth rate, all stack multiplicatively
- **Verify**: All penalties together = near-zero growth

---

## Connections

- **Uses**: 06a (farm grid, tilling, watering, fertilizing, weeding — all land infrastructure)
- **Uses**: Weather/seasons (growth modulation, frost, rain watering)
- **Uses**: Temperature grid (growth rate, frost damage)
- **Uses**: Wetness/mud system (water needs)
- **Uses**: Tool quality (cutting for harvest speed)
- **Uses**: Cooking system (fire pit/hearth recipes)
- **Uses**: Plant system (extended for wild crop plants)
- **Opens**: 07 (flax fiber → loom → linen → clothing)
- **Opens**: 09 (compost demand from 06a now fed by crop waste, closing the loop)
- **Opens**: 10 (food variety — bread vs lentils vs meat affects mood)
- **Deferred**: Flax oil (quern recipe, future), irrigation channels, crop diseases, scarecrows, more crop types

---

## Stockpile Filter Keys

All single-char keys (a-z, 0-9) may be taken. Options:
1. Use shift+key combinations for farm items (Shift+W=wheat, Shift+L=lentils, etc.)
2. Group farm items under a "Crops" category toggle
3. Add a second filter page (press Tab to flip between pages)

Decide during implementation based on what keys are actually available. At minimum need filters for: seeds (3 types), crops (wheat, lentils), fiber (flax), flour, bread, cooked lentils = 9 new items needing filter support.
