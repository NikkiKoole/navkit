# Feature 12: Biome Presets

> **Priority**: Tier 3 — World Variety
> **Why now**: All 5 soils, 4 tree species, 3 stone types, weather/seasons, and temperature already exist. Worldgen distributes them by elevation+noise. Most biomes are just named presets for knobs that already work. A few (beach, desert) need new sprites to feel right.
> **Systems used**: Terrain generation, material system, weather, temperature, vegetation, tree placement
> **Depends on**: Nothing — purely a worldgen + climate config feature
> **Opens**: Biome zones (future), biome-specific flora/fauna, trade route variety

---

## What Changes

Instead of one hardcoded terrain generation profile, the player picks a **biome** at world creation. Each biome is a preset that tunes: soil distribution weights, tree species weights, stone type, vegetation density, climate (base temperature + seasonal amplitude), water features (river/lake count), and a grass color tint. No new cell types, materials, or items needed.

The world goes from "always temperate grassland" to 8 distinct starting environments. Six work with existing assets; two (beach, desert) need new sprites/items to feel right.

---

## Design

### BiomePreset Struct

```c
typedef struct {
    const char* name;
    const char* description;

    // Climate
    int baseSurfaceTemp;        // Center of seasonal range (C)
    int seasonalAmplitude;      // Swing above/below base (C)
    int diurnalAmplitude;       // Day/night swing (C)

    // Terrain shape
    int heightVariation;        // 0=flat, 1=rolling, 2=hilly, 3=mountainous

    // Soil weights (0.0-1.0, normalized at generation time)
    float soilDirt;
    float soilClay;
    float soilSand;
    float soilGravel;
    float soilPeat;

    // Underground stone
    MaterialType stoneType;     // MAT_GRANITE, MAT_SANDSTONE, or MAT_SLATE

    // Tree weights (0.0-1.0, normalized)
    float treeOak;
    float treePine;
    float treeBirch;
    float treeWillow;
    float treePalm;             // Only used in beach/desert biomes (needs MAT_PALM)
    float treeDensity;          // Multiplier on base placement chance (1.0 = normal)

    // Vegetation
    float grassDensity;         // 0.0-1.0, fraction of dirt cells that get grass
    Color grassTint;            // Color multiplier on grass sprites

    // Water
    int riverCount;             // 0-6
    int lakeCount;              // 0-6

    // Bush/crop density multipliers
    float bushDensity;          // Multiplier on berry bush placement (1.0 = normal)
    float wildCropDensity;      // Multiplier on wild wheat/lentil/flax (1.0 = normal)

    // Scatter
    float boulderDensity;       // Multiplier (1.0 = normal)

    // Special worldgen flags
    bool hasCoastline;          // Beach: one edge becomes ocean
    bool hasOases;              // Desert: small water pools instead of rivers
    int oasisCount;             // Number of oases (1-3)
    float cactusDensity;        // Desert: cactus placement (0.0 = none)
    float deadBushDensity;      // Desert/arid: dead bush scatter (0.0 = none)
    float driftwoodDensity;     // Beach: driftwood along waterline (0.0 = none)
    float reedRadius;           // How far reeds extend from water (default 1.0)
} BiomePreset;
```

### Biome Definitions

#### 1. Temperate Grassland (default — current behavior)

The baseline. Gentle rolling hills, mixed forests, rivers, mild climate.

```
baseSurfaceTemp: 15    seasonalAmplitude: 25    diurnalAmplitude: 5
heightVariation: 1 (rolling)
soils: dirt 0.5, clay 0.2, sand 0.1, gravel 0.1, peat 0.1
stone: MAT_GRANITE
trees: oak 0.35, pine 0.20, birch 0.30, willow 0.15, palm 0.0, density 1.0
grassDensity: 1.0    grassTint: WHITE (no change)
rivers: 2    lakes: 2
bushDensity: 1.0    wildCropDensity: 1.0    boulderDensity: 1.0
special: none
```

**Feels like**: Current game. Green, forested, mild.

#### 2. Arid Scrubland

Hot, dry, sandy terrain with sparse vegetation. Sandstone underground. Few trees (mostly birch on sandy soil), scattered bushes. Almost no water.

```
baseSurfaceTemp: 28    seasonalAmplitude: 30    diurnalAmplitude: 12
heightVariation: 1 (rolling)
soils: sand 0.45, gravel 0.25, dirt 0.15, clay 0.10, peat 0.05
stone: MAT_SANDSTONE
trees: oak 0.05, pine 0.15, birch 0.60, willow 0.20, palm 0.0, density 0.25
grassDensity: 0.3    grassTint: {220, 200, 120} (dry yellow-brown)
rivers: 1    lakes: 0
bushDensity: 0.5    wildCropDensity: 0.3    boulderDensity: 1.0
deadBushDensity: 0.03
special: none
```

**Feels like**: Dry steppe / scrubland. Yellow grass, sandstone cliffs, sparse birch stands. Water is precious — the single river is a lifeline.

**Survival challenge**: Water scarcity, less foraging, extreme day/night temperature swings.

#### 3. Boreal / Taiga

Cold, long winters. Dense pine forests on peaty, gravelly soil. Slate bedrock. Rivers freeze early.

```
baseSurfaceTemp: 2     seasonalAmplitude: 30    diurnalAmplitude: 4
heightVariation: 1 (rolling)
soils: peat 0.30, gravel 0.25, dirt 0.25, clay 0.10, sand 0.10
stone: MAT_SLATE
trees: oak 0.05, pine 0.60, birch 0.25, willow 0.10, palm 0.0, density 1.4
grassDensity: 0.6    grassTint: {160, 190, 140} (muted cool green)
rivers: 2    lakes: 3
bushDensity: 0.8    wildCropDensity: 0.5    boulderDensity: 1.0
special: none
```

**Feels like**: Northern forest. Pine-dominated, snowy winters, dark slate rock. Peat everywhere.

**Survival challenge**: Long cold winters with deep snow, short growing season. Peat as fuel is critical.

#### 4. Wetland / Marsh

Waterlogged lowlands. Willow-dominated, lots of peat and clay. Many rivers and lakes. Flat terrain, constant mud.

```
baseSurfaceTemp: 12    seasonalAmplitude: 20    diurnalAmplitude: 4
heightVariation: 0 (flat)
soils: peat 0.35, clay 0.25, dirt 0.25, sand 0.05, gravel 0.10
stone: MAT_SLATE
trees: oak 0.10, pine 0.05, birch 0.10, willow 0.75, palm 0.0, density 0.8
grassDensity: 0.8    grassTint: {140, 200, 160} (lush blue-green)
rivers: 4    lakes: 4
bushDensity: 1.2    wildCropDensity: 0.8    boulderDensity: 0.5
reedRadius: 2.0
special: none
```

**Feels like**: Bog / fenland. Willows, reeds everywhere, mud. Water isn't scarce — dry ground is.

**Survival challenge**: Constant mud slowing movement, few dry building sites, dampness.

#### 5. Highland / Rocky

Mountainous terrain with thin soil over granite. Pine trees cling to slopes. Sparse vegetation, lots of exposed rock and boulders.

```
baseSurfaceTemp: 8     seasonalAmplitude: 25    diurnalAmplitude: 8
heightVariation: 3 (mountainous)
soils: gravel 0.40, dirt 0.20, sand 0.15, clay 0.10, peat 0.15
stone: MAT_GRANITE
trees: oak 0.10, pine 0.55, birch 0.25, willow 0.10, palm 0.0, density 0.5
grassDensity: 0.4    grassTint: {180, 200, 160} (pale grey-green)
rivers: 1    lakes: 1
bushDensity: 0.4    wildCropDensity: 0.3    boulderDensity: 3.0
special: none
```

**Feels like**: Scottish highlands. Craggy, steep, windswept. Stone is abundant, wood is scarce.

**Survival challenge**: Steep terrain limits building sites, scarce wood, cold exposure, lots of stone but little soil for farming.

#### 6. Beach / Coastal

A long sandy coastline with a large body of water on one side. Sand dominates, with driftwood scattered along the shore. Sparse birch and willow near the waterline. Seashells and driftwood as unique resources. Mild, maritime climate.

```
baseSurfaceTemp: 18    seasonalAmplitude: 12    diurnalAmplitude: 4
heightVariation: 0 (flat)
soils: sand 0.60, gravel 0.15, dirt 0.15, clay 0.05, peat 0.05
stone: MAT_SANDSTONE
trees: oak 0.05, pine 0.0, birch 0.15, willow 0.20, palm 0.60, density 0.2
grassDensity: 0.2    grassTint: {200, 210, 150} (pale sandy-green)
rivers: 1    lakes: 0
bushDensity: 0.3    wildCropDensity: 0.2    boulderDensity: 0.3
driftwoodDensity: 0.15
special: hasCoastline
```

**Special worldgen**: One map edge is a large body of water (ocean/sea) — a wide strip of sand slopes down to water level. Driftwood (ITEM_LOG, ITEM_STICKS) scattered densely along the waterline. Seashells (new item) scattered on sand near water.

**Feels like**: Exposed sandy shore. Lots of water, almost no trees, sand everywhere. Building materials are scarce — driftwood and sandstone are your main resources.

**Survival challenge**: Very little wood (no forests), minimal foraging, wind exposure. The ocean provides water for drinking but you need to collect it. Sand is poor farmland.

**Needs new sprites/items**: Palm tree (5th species), seashell, driftwood pile, beach grass (tinted grass or new VEG type), coconut/tropical fruit (optional).

#### 7. Desert

Extreme heat, almost no water, vast sand expanses broken by sandstone formations. Cacti replace trees. Scorching days, freezing nights. Oasis as rare life-giving landmarks.

```
baseSurfaceTemp: 35    seasonalAmplitude: 15    diurnalAmplitude: 18
heightVariation: 1 (rolling)
soils: sand 0.70, gravel 0.15, dirt 0.05, clay 0.05, peat 0.05
stone: MAT_SANDSTONE
trees: oak 0.0, pine 0.0, birch 0.0, willow 0.0, palm 0.0, density 0.0
  (oasis trees placed separately: willow 0.4, palm 0.6)
grassDensity: 0.05   grassTint: {210, 190, 120} (bleached yellow)
rivers: 0    lakes: 0
bushDensity: 0.1    wildCropDensity: 0.05    boulderDensity: 2.0
cactusDensity: 0.06    deadBushDensity: 0.02
special: hasOases (count: 2)
```

**Special worldgen**: No rivers or lakes by default — instead, 1-2 **oases** (small pools of water with a ring of dirt/grass around them, a few willow/birch trees, and berry bushes). Cactus plants replace trees as the main vegetation (new PLANT_CACTUS). Sandstone rock formations (tall exposed sandstone pillars/mesas) placed instead of caves.

**Feels like**: Barren, hostile, beautiful. Golden sand to the horizon, punctuated by sandstone outcrops and the rare green oasis.

**Survival challenge**: The hardest biome. No wood (cacti aren't choppable for logs), almost no water, extreme temperature swings. You must reach an oasis to survive. Sandstone is your building material. Cactus fruit is your food.

**Needs new sprites/items**: Cactus (new plant/bush sprite + new item ITEM_CACTUS_FRUIT), dead bush/tumbleweed sprite, sand dune variant (optional, can tint existing sand), oasis palm or date palm tree, scorpion/snake (future fauna).

#### 8. Riverlands

A broad, fertile river valley. Multiple rivers converge, creating rich alluvial soil. Dense mixed forests, abundant wildlife, great farmland. The easiest biome — a breadbasket.

```
baseSurfaceTemp: 16    seasonalAmplitude: 22    diurnalAmplitude: 5
heightVariation: 0 (flat)
soils: dirt 0.40, clay 0.30, sand 0.10, gravel 0.10, peat 0.10
stone: MAT_GRANITE
trees: oak 0.35, pine 0.10, birch 0.20, willow 0.35, palm 0.0, density 1.2
grassDensity: 1.0    grassTint: {120, 210, 100} (rich dark green)
rivers: 5    lakes: 3
bushDensity: 1.5    wildCropDensity: 2.0    boulderDensity: 0.3
reedRadius: 2.0
special: wide rivers (baseWidth 3-4), extra clay (threshold 0.45)
```

**Special worldgen**: Rivers are wider (baseWidth 2-4). The high river count + flat terrain creates a network of waterways with fertile islands and peninsulas between them. Extra reeds along all waterbanks. Clay is abundant in the subsoil (great for pottery and bricks).

**Feels like**: Lush, green, abundant. Rivers everywhere, willows drooping over the water, thick grass, wild wheat and flax growing naturally. The "easy mode" biome.

**Survival challenge**: Low — this is the gentle start. Flooding from rain is the main hazard (flat terrain + many rivers = mud everywhere when it rains). Building dry foundations is the key challenge.

**Needs new sprites/items**: Mostly works with existing assets. Optional: river fish sprite (future), water lily/lotus (decorative water plant), wider river bed textures.

---

## Biome Summary Table

| # | Biome | Difficulty | Key Material | Key Tree | New Assets Needed |
|---|-------|-----------|-------------|----------|-------------------|
| 1 | Temperate Grassland | Medium | Granite | Oak/Birch mix | None |
| 2 | Arid Scrubland | Hard | Sandstone | Sparse birch | Dry grass tint only |
| 3 | Boreal / Taiga | Hard | Slate | Pine dominant | None |
| 4 | Wetland / Marsh | Medium | Slate | Willow dominant | None |
| 5 | Highland / Rocky | Hard | Granite | Sparse pine | None |
| 6 | Beach / Coastal | Hard | Sandstone | Sparse birch/willow | Palm tree, seashell |
| 7 | Desert | Very Hard | Sandstone | None (cacti) | Cactus, dead bush, oasis |
| 8 | Riverlands | Easy | Granite | Oak/Willow mix | None (optional: water lily) |

---

## New Assets Needed

### New Sprites (8x8 textures)

| # | Sprite Name | Description | Used By | Biome |
|---|-------------|-------------|---------|-------|
| 1 | `SPRITE_cactus` | Tall cactus silhouette (like tree trunk but prickly) | CELL_BUSH or new PLANT_CACTUS | Desert |
| 2 | `SPRITE_cactus_fruit` | Small round fruit (recolor of SPRITE_division) | ITEM_CACTUS_FRUIT | Desert |
| 3 | `SPRITE_dead_bush` | Dry leafless shrub (tumbleweed-ish) | CELL_BUSH variant or scatter | Desert, Arid |
| 4 | `SPRITE_palm_trunk` | Tall thin trunk with ring texture | CELL_TREE_TRUNK + MAT_PALM | Beach, Desert (oasis) |
| 5 | `SPRITE_palm_leaves` | Fan-shaped frond canopy | CELL_TREE_LEAVES + MAT_PALM | Beach, Desert (oasis) |
| 6 | `SPRITE_palm_sapling` | Small palm sprout | CELL_SAPLING + MAT_PALM | Beach, Desert (oasis) |
| 7 | `SPRITE_palm_planks` | Light-colored planks | Constructed walls/floors | Beach, Desert |
| 8 | `SPRITE_palm_bark` | Fibrous bark texture | ITEM_BARK from palm | Beach, Desert |
| 9 | `SPRITE_palm_branch` | Thin palm branch/frond | CELL_TREE_BRANCH + MAT_PALM | Beach, Desert |
| 10 | `SPRITE_palm_stripped_log` | Stripped palm log | ITEM_STRIPPED_LOG | Beach, Desert |
| 11 | `SPRITE_seashell` | Small spiral shell | ITEM_SEASHELL | Beach |
| 12 | `SPRITE_coconut` | Round fruit (optional) | ITEM_COCONUT (optional) | Beach |
| 13 | `SPRITE_dry_grass` | Yellowed/dead grass blades | VEG_DRY_GRASS or grass tint | Desert, Arid |

**Minimum viable set**: Sprites 1, 3, 4, 5, 6 (cactus + dead bush + palm trunk/leaves/sapling). Everything else is optional or can be achieved with tinting.

### New Items

| # | Item | Sprite | Flags | Stack | Source | Biome |
|---|------|--------|-------|-------|--------|-------|
| 1 | ITEM_CACTUS_FRUIT | SPRITE_cactus_fruit | IF_FOOD, IF_STACKABLE | 10 | Harvest cactus plant | Desert |
| 2 | ITEM_SEASHELL | SPRITE_seashell | IF_STACKABLE, IF_BUILDING_MAT | 20 | Scatter on beach sand | Beach |
| 3 | ITEM_COCONUT | SPRITE_coconut | IF_FOOD, IF_STACKABLE | 5 | Harvest palm tree (optional) | Beach |

**ITEM_CACTUS_FRUIT**: Edible raw (like berries), satisfies hunger + small thirst bonus. Grows on PLANT_CACTUS.

**ITEM_SEASHELL**: Decorative building material. Could be used in crafting (shell mortar? shell jewelry? lime from burning shells at kiln?). Scattered on sand cells near water during beach worldgen.

**ITEM_COCONUT** (optional): Food item from palm trees. Would need a harvest mechanic for trees (shake/climb). Can defer.

### New Materials

| # | Material | Sprite | Category | Drops | Notes |
|---|----------|--------|----------|-------|-------|
| 1 | MAT_PALM | SPRITE_palm_trunk | Wood | ITEM_LOG | 5th wood species, follows existing pattern |

Palm follows the same 7-sprite pattern as oak/pine/birch/willow. Needs:
- `spriteOverrides` entries for CELL_TREE_TRUNK, CELL_TREE_LEAVES, CELL_TREE_BRANCH, CELL_SAPLING + MAT_PALM
- `IsWoodMaterial()` update to include MAT_PALM
- `MaterialTint()` entry: `{200, 180, 130}` (warm sandy tan)
- Tree growth parameters in `trees.c`: tall trunk (4+ cells), no taper, small canopy

### New Plants

| # | Plant | Sprite | Stages | Harvest | Notes |
|---|-------|--------|--------|---------|-------|
| 1 | PLANT_CACTUS | SPRITE_cactus | BARE → BUDDING → RIPE | ITEM_CACTUS_FRUIT | Like berry bush. Placed on sand in desert. Slow growth, survives heat. |

### New Vegetation Type (optional)

| # | VegType | Sprite | Notes |
|---|---------|--------|-------|
| 1 | VEG_DRY_GRASS | SPRITE_dry_grass or tinted SPRITE_grass | Alternative: just use grassTint per biome (cheaper, no new enum needed) |

**Recommendation**: Skip VEG_DRY_GRASS as a separate type. The biome grassTint achieves 90% of the visual effect. If you want more visual variety later, add it then.

### Summary: What to Draw

**Must-have sprites** (5 sprites to unlock Beach + Desert):
1. `cactus` — spiky column, ~6x8 pixels, green with lighter spines
2. `dead_bush` — dry branching shrub, brown/grey, 5x5 pixels
3. `palm_trunk` — thin vertical with horizontal ring marks, tan/brown
4. `palm_leaves` — wide fan/frond shape, bright green, fills most of 8x8
5. `palm_sapling` — tiny palm sprout, 3x4 pixels

**Nice-to-have sprites** (5 more for polish):
6. `palm_planks` — light tan planks (or just tint existing planks)
7. `palm_bark` — fibrous texture
8. `palm_branch` — single frond
9. `palm_stripped_log` — pale cylindrical
10. `seashell` — tiny spiral, white/pink

**Can skip entirely** (use tinting or existing sprites):
- `dry_grass` — use grassTint {210, 190, 120}
- `coconut` — defer to later
- `cactus_fruit` — recolor SPRITE_division

---

## Implementation

### Step 1: BiomePreset struct + presets table

**`src/world/terrain.h`**: Add BiomePreset typedef and extern declarations.

```c
typedef struct { ... } BiomePreset;

extern const BiomePreset biomePresets[];
extern int biomePresetCount;
extern int selectedBiome;  // Index into biomePresets[], saved with world
```

**`src/world/terrain.c`**: Define the presets table (static const array of 8 presets).

### Step 1b: New assets (Beach + Desert biomes)

**`src/world/material.h`**: Add `MAT_PALM` before MAT_BEDROCK. Update `IsWoodMaterial()`.

**`src/world/material.c`**: Add MAT_PALM materialDef (sprite: SPRITE_palm_trunk, FLAMMABLE, drops ITEM_LOG). Add spriteOverrides for CELL_TREE_TRUNK/LEAVES/BRANCH/SAPLING + MAT_PALM.

**`assets/textures8x8/`**: Add new PNG files (cactus, dead-bush, palm-trunk, palm-leaves, palm-sapling + optionally palm-planks, palm-bark, palm-branch, palm-stripped-log, seashell).

**`assets/atlas.h`**: Add SPRITE_cactus, SPRITE_dead_bush, SPRITE_palm_trunk, SPRITE_palm_leaves, SPRITE_palm_sapling (+ optional others). Regenerate atlas.

**`src/entities/items.h`**: Add ITEM_CACTUS_FRUIT, ITEM_SEASHELL (+ optional ITEM_COCONUT).

**`src/entities/item_defs.c`**: Add definitions.

**`src/simulation/plants.h`**: Add PLANT_CACTUS to PlantType enum.

**`src/simulation/plants.c`**: Add cactus plant definition (growth rate, harvest yield ITEM_CACTUS_FRUIT, prefers sand soil, heat tolerant).

### Step 2: Wire presets into GenerateHillsSoilsWater

Replace hardcoded soil thresholds with preset-driven logic. The current soil assignment code in `GenerateHillsSoilsWater` (lines ~2449-2463) uses noise thresholds to pick soil. Change this to a **weighted random pick** driven by the preset's soil weights, modulated by local conditions:

```c
// Instead of: if (wetness > 0.7 && peatN > peatNoise) surfaceMat = MAT_PEAT;
// Do: weighted pick using preset soil weights, adjusted by local wetness/slope/noise
```

The key insight: keep the *environmental modulation* (peat near water, sand on dry ground, gravel on slopes) but scale the base probabilities per biome. A biome with `soilPeat: 0.05` will still put peat near water — just much less of it.

**Soil selection algorithm:**
```
For each surface cell:
    weights[5] = { preset.soilDirt, preset.soilClay, preset.soilSand,
                   preset.soilGravel, preset.soilPeat }

    // Environmental modifiers (same logic as today, but multiplicative)
    if (wetness > 0.6)  weights[PEAT] *= 2.0
    if (wetness < 0.3)  weights[SAND] *= 2.0
    if (slope >= 2)     weights[GRAVEL] *= 2.5
    if (nearWater)      weights[CLAY] *= 1.5

    // Normalize and pick (using per-cell noise as random source)
    surfaceMat = WeightedPick(weights, noise)
```

### Step 3: Wire tree species weights

Replace `PickTreeTypeForWorldGen()` with a preset-aware version:

```c
static const MaterialType treeSpecies[5] = {
    MAT_OAK, MAT_PINE, MAT_BIRCH, MAT_WILLOW, MAT_PALM
};

static MaterialType PickTreeTypeForBiome(const BiomePreset* bp,
    MaterialType soil, float wetness, int slope, bool nearWater, float noise)
{
    float w[5] = { bp->treeOak, bp->treePine, bp->treeBirch, bp->treeWillow, bp->treePalm };

    // Environmental modifiers (same spirit as current logic)
    if (nearWater || wetness > 0.7f) w[3] *= 3.0f;  // willow near water
    if (slope >= 1 && wetness < 0.45f) w[1] *= 2.0f; // pine on slopes
    if (soil == MAT_SAND) { w[2] *= 2.0f; w[4] *= 2.0f; }  // birch+palm on sand

    // Normalize weights, pick using noise as deterministic random
    float total = w[0] + w[1] + w[2] + w[3] + w[4];
    if (total <= 0.0f) return MAT_OAK;  // fallback
    float pick = noise * total;
    float accum = 0.0f;
    for (int i = 0; i < 5; i++) {
        accum += w[i];
        if (pick < accum) return treeSpecies[i];
    }
    return treeSpecies[4];
}
```

Tree density: multiply the existing chance values by `bp->treeDensity`.

### Step 4: Wire climate parameters

At world creation, copy preset climate values to the existing globals:

```c
baseSurfaceTemp = biomePresets[selectedBiome].baseSurfaceTemp;
seasonalAmplitude = biomePresets[selectedBiome].seasonalAmplitude;
diurnalAmplitude = biomePresets[selectedBiome].diurnalAmplitude;
```

These are already saved per-world via SETTINGS_TABLE — no save migration needed.

### Step 5: Wire stone type

Replace the hardcoded `MAT_GRANITE` in the rock layer pass (~line 2083-2094) with `biomePresets[selectedBiome].stoneType`. Sandstone and slate already have full material definitions, sprites, and drops — they've been waiting for this.

### Step 6: Grass tinting

**`src/render/rendering.c`** in `DrawGrassOverlay()`: multiply the grass sprite tint by the biome's grassTint color. Currently grass is always drawn with WHITE tint — change to:

```c
Color grassColor = biomePresets[selectedBiome].grassTint;
// Then use grassColor where WHITE is currently used for grass sprite tinting
```

This single change makes arid biomes feel yellow-brown and boreal biomes feel muted green.

### Step 7: Height variation

The current heightmap uses random scale/octaves/persistence. Map the preset's `heightVariation` to parameter ranges:

| heightVariation | scale | octaves | maxHeight | Notes |
|-----------------|-------|---------|-----------|-------|
| 0 (flat) | 0.005-0.015 | 2-3 | minHeight+2 | Wetland — almost no hills |
| 1 (rolling) | 0.01-0.05 | 2-6 | current | Default — current behavior |
| 2 (hilly) | 0.02-0.06 | 3-6 | gridDepth-2 | More dramatic terrain |
| 3 (mountainous) | 0.03-0.07 | 4-8 | gridDepth-1 | Steep, high peaks |

### Step 8: Water/bush/scatter density

Wire preset river/lake counts to the existing `hillsWaterRiverCount` / `hillsWaterLakeCount` globals. Multiply bush/boulder/wild crop placement chances by preset multipliers.

### Step 9: UI — biome selection

Add biome picker to the terrain generation UI (where river/lake sliders already live). Simple dropdown or number key selection showing name + 1-line description.

### Step 10: Special worldgen — Beach

The beach biome needs a custom terrain shape: one edge of the map is ocean.

After the normal heightmap generation, apply a **coastal gradient**:
- Pick a random edge (N/S/E/W) as the coastline
- Apply a height ramp: cells within ~30% of that edge are forced low (below water level)
- Fill the low area with water + water sources (the "ocean")
- The shoreline strip (5-10 cells from water edge) gets forced to MAT_SAND
- Scatter ITEM_STICKS and ITEM_LOG densely along the waterline (driftwood)
- Scatter ITEM_SEASHELL on sand cells within 3 cells of water
- Place a few palm trees (MAT_PALM) along the shore

This is a post-pass on the normal terrain, so it works with any heightmap.

### Step 11: Special worldgen — Desert

The desert biome needs oases instead of rivers/lakes:

- Skip normal river/lake generation (riverCount=0, lakeCount=0)
- Place 1-3 **oases**: small circular areas (radius 4-8) with:
  - A shallow pool of water at center (with water source)
  - A ring of MAT_DIRT around the pool (2-3 cells wide)
  - VEG_GRASS on the dirt ring
  - 3-6 trees (willow/palm) around the pool
  - Berry bushes on the dirt
- Place PLANT_CACTUS on sand cells (replace tree placement pass):
  - Use tree noise field but with different thresholds
  - ~5-8% placement in low-noise areas (clustered)
- Place dead bushes (CELL_BUSH with dead bush sprite or scatter item) on sand at ~2%
- Place sandstone rock formations: tall exposed CELL_WALL + MAT_SANDSTONE pillars (2-4 cells high, like boulders but taller)

### Step 12: Special worldgen — Riverlands

Mostly uses existing generation but with tweaks:
- Rivers use wider baseWidth (3-4 instead of 1-2)
- Extra reeds: extend VEG_REEDS placement to 2-cell radius from water (vs current 1-cell adjacency)
- Extra wild crops: double the base chance for wheat/lentils/flax
- Extra clay in subsoil: lower clay noise threshold to 0.45 (from 0.58)

These are simple parameter overrides, no new special passes needed.

### Step 13: Save selectedBiome

Add `selectedBiome` to the save file (single int). On load, the climate globals are already restored via SETTINGS_TABLE. The biome index is needed for grass tint (rendering) and any future per-biome behavior.

Save migration: add `selectedBiome` field, default 0 (temperate) for old saves.

---

## What's Free (no new assets) — 6 biomes

These biomes work entirely with existing sprites and materials:

1. **Temperate Grassland** — current game
2. **Arid Scrubland** — existing sand/gravel + grass tint
3. **Boreal / Taiga** — existing pine/peat/slate
4. **Wetland / Marsh** — existing willow/peat/reeds
5. **Highland / Rocky** — existing granite/gravel/pine
6. **Riverlands** — existing oak/willow/clay/rivers

Why they work:
- **5 soil textures** already look distinct (dirt brown, clay red, sand tan, gravel grey, peat dark)
- **3 stone textures** already exist (granite grey, sandstone warm, slate dark)
- **4 tree species** already have 7 sprites each
- **Grass tinting** uses the existing color multiply path
- **Snow, mud, reeds** already render and create visual variety
- **Weather/seasons** already work — biomes just shift the temperature baseline

## What Needs New Assets — 2 biomes

**Beach** and **Desert** need new sprites to have identity. See the full "New Assets Needed" section above.

## Future Sprites (not needed now, nice later)

| Sprite | What it enables | Biome |
|--------|----------------|-------|
| Mushroom (ground item) | Forest floor foraging | Boreal, wetland |
| Spruce/fir (6th tree species) | True boreal canopy shape | Boreal/taiga |
| Autumn leaf tints | Seasonal color on existing leaf sprites | All (seasonal rendering) |
| Tall reeds / cattails | Wetland vegetation variety | Wetland |
| Heather / low scrub | Highland ground cover | Highland |
| Water lily / lotus | Decorative water plant | Riverlands, wetland |
| Scorpion / snake | Desert fauna | Desert |
| Fish | Aquatic fauna | Beach, riverlands |

---

## Effort Estimate

**Phase 1 (~1.5 sessions)**: BiomePreset struct + 6 free biomes (temperate, arid, boreal, wetland, highland, riverlands). Refactor `GenerateHillsSoilsWater` to read from presets. Grass tinting. UI. Save migration.

**Phase 2 (~1 session)**: New assets (palm tree, cactus, dead bush, seashell). MAT_PALM material + sprites. PLANT_CACTUS. ITEM_CACTUS_FRUIT + ITEM_SEASHELL.

**Phase 3 (~1 session)**: Beach + desert special worldgen (coastal gradient, oases, cactus placement, driftwood scatter, sandstone formations).

## Tests

- `test_biome_presets.c`: Verify preset values are valid (weights > 0, stone type is valid stone, temperatures in range)
- Verify soil assignment produces expected dominant materials per biome (generate small map, count surface materials)
- Verify tree assignment produces expected dominant species per biome
- Verify grass tint is applied in rendering (unit test: check tint != WHITE for non-temperate biomes)

## Future: Biome Zones (not this feature)

The next evolution would be having multiple biomes within one map — a `uint8_t biomeGrid[HEIGHT][WIDTH]` assigned by macro-scale noise. Each column gets its biome's soil/tree/grass rules. Weather stays global (one sky). This is a natural extension of the preset system but adds ~200 more lines and isn't needed to get variety into the game.
