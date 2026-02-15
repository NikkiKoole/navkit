# Feature 06: Farming & Crops

> **Priority**: Tier 2 — Tool & Production Loop
> **Why now**: Foraging (F1) and hunting (F5) don't scale. Weather/seasons exist but create no food pressure. Soil types exist but don't matter.
> **Systems used**: Weather, seasons, temperature, water/wetness, mud, soil materials, tools (F4)
> **Depends on**: F1 (hunger), F4 (tools — digging for tilling), F5 (cooking — raw vs cooked)
> **Opens**: Fertilizer demand (F9 composting), food variety for moods (F10)

---

## What Changes

**Farm plots** are designated areas where movers till soil, plant seeds, and harvest crops. Crop growth is affected by **season, temperature, water, and soil type** — all systems that already exist but had no gameplay impact on food. Farming creates sustainable food that scales with colony size, replacing the foraging/hunting that only feeds a few.

This is where weather and seasons become *real*. A failed harvest means winter starvation.

---

## Design

### Farm Plots

A **farm plot** is a designation (like mining/chopping) on soil cells:

1. **Designate farm zone** (drag rectangle on dirt/clay/sand/peat cells)
2. Movers **till** each cell (JOBTYPE_TILL, needs digging tool, 3s work)
3. Tilled cell becomes **CELL_FARM** (or flag on existing soil cell)
4. Movers **plant** seeds (JOBTYPE_PLANT_CROP, 1s)
5. Crops **grow** over time (affected by environment)
6. Movers **harvest** when ripe (JOBTYPE_HARVEST, 2s)
7. Cycle repeats

### Crop Growth

Each farm cell tracks:
```c
uint8_t cropType;      // what's planted
uint8_t growthStage;   // 0=bare, 1=sprouted, 2=growing, 3=mature, 4=ripe
float growthProgress;  // 0.0-1.0 within current stage
```

**Growth rate** per tick:
```c
float rate = baseGrowthRate
    * seasonModifier      // summer=1.5, spring/autumn=1.0, winter=0.0
    * temperatureModifier // too cold or too hot = 0, sweet spot = 1.0
    * waterModifier;      // too dry=0.5, good=1.0, waterlogged=0.5
```

**Existing systems feeding into growth:**
- `GetSeasonalGrowthRate()` — already exists for vegetation
- `GetCellTemperature()` — temperature grid exists
- `GetCellWetness()` — wetness tracking exists (mud system)
- Rain automatically waters exposed crops (weather system)

### Crop Types (Start Simple)

| Crop | Growth Time | Season | Yield | Notes |
|------|-------------|--------|-------|-------|
| Wheat | 3 days | Spring-Autumn | ITEM_WHEAT × 4 | Staple grain, robust |
| Potato | 2 days | Spring-Summer | ITEM_POTATO × 3 | Fast, high calorie |
| Berry bush | Plant once | Spring-Autumn | ITEM_BERRIES × 2/harvest | Perennial, multiple harvests |

Start with just **2-3 crop types**. Expand later.

### Seed Items

| Item | Source | Notes |
|------|--------|-------|
| ITEM_WHEAT_SEEDS | Harvesting wheat (1 per harvest) | Self-sustaining after first |
| ITEM_POTATO_SEEDS | Harvesting potato (1 per harvest) | Potato eyes |

Seeds come from harvesting. First seeds come from... wild plants (berry bushes drop seeds when harvested, wheat/potato found as rare wild plants on map gen). This closes the bootstrap loop.

### Soil Type Effects

Existing soil materials finally matter:

| Soil | Water Retention | Growth Bonus | Notes |
|------|----------------|-------------|-------|
| MAT_DIRT | Medium | 1.0× | Baseline, good all-around |
| MAT_CLAY | High | 0.8× | Holds water (risk: waterlogging in rain) |
| MAT_SAND | Low | 0.7× | Drains fast (needs frequent watering) |
| MAT_PEAT | Very high | 1.2× | Excellent but rare |
| MAT_GRAVEL | Very low | 0.5× | Poor farming soil |

This creates **location strategy**: peat is best, clay is risky in rainy seasons, sand needs irrigation.

### Seasonal Farming Calendar

The existing season system creates natural pressure:

- **Spring**: Plant crops. Growth starts slow. Last frost risk.
- **Summer**: Peak growth. Must keep watered. Harvest early crops.
- **Autumn**: Final harvest before frost. Preserve food for winter.
- **Winter**: Nothing grows. Colony lives off stored food.

This is the first time seasons create real gameplay consequences.

### Processing Chain

Harvested crops feed into existing systems:

```
Wheat → Hearth (bake) → ITEM_BREAD (+0.6 hunger)
Potato → Hearth (roast) → ITEM_ROASTED_POTATO (+0.4 hunger)
Berries → Drying rack → ITEM_DRIED_BERRIES (preservation)
```

---

## Implementation Phases

### Phase 1: Farm Plot Designation
1. Add DESIGNATION_FARM (drag-rectangle, like mining)
2. Add JOBTYPE_TILL (mover tills soil cell, needs QUALITY_DIGGING)
3. Tilled cell gets farm flag (reuse cell flags or new grid)
4. Visual: tilled soil looks different (darker, furrowed)
5. **Test**: Can designate farm, movers till soil cells

### Phase 2: Planting & Growth
1. Add crop tracking per farm cell (type, stage, progress)
2. Add JOBTYPE_PLANT_CROP (mover plants seed item)
3. Growth tick: update progress based on season × temp × water
4. Stage progression: bare → sprouted → growing → mature → ripe
5. Visual: different sprite per growth stage
6. **Test**: Planted crop grows through stages, rate affected by season

### Phase 3: Harvesting
1. Add JOBTYPE_HARVEST (mover harvests ripe crop)
2. Harvesting produces crop items + seed item
3. Farm cell returns to tilled state (ready for replanting)
4. Auto-assignment: WorkGiver assigns harvest jobs when crops are ripe
5. **Test**: Ripe crops harvested correctly, items spawned

### Phase 4: Crop Items & Cooking
1. Add ITEM_WHEAT, ITEM_POTATO, ITEM_WHEAT_SEEDS, ITEM_POTATO_SEEDS
2. Add cooking recipes to hearth: wheat → bread, potato → roasted potato
3. Add ITEM_BREAD, ITEM_ROASTED_POTATO
4. **Test**: Cooking recipes work, hunger values correct

### Phase 5: Environmental Integration
1. Temperature affects growth (too cold = frost damage, kills crop)
2. Wetness affects growth (rain waters crops, excess = waterlogging)
3. Soil type modifies growth rate
4. Winter kills all unharvested crops
5. **Test**: Crops grow faster in summer, die in frost, affected by wetness

---

## Connections

- **Uses weather/seasons**: Growth modulation, rain watering, frost damage
- **Uses temperature**: Growth rate, frost kill threshold
- **Uses water/wetness**: Rain waters crops, mud system interaction
- **Uses soil materials**: Dirt/clay/sand/peat affect farming quality
- **Uses tools (F4)**: Tilling needs digging tool for speed bonus
- **Uses cooking (F5)**: Raw crops → cooked food at hearth
- **Uses hunger (F1)**: Farming is the scalable food source
- **Opens for F9 (Composting)**: Depleted soil needs fertilizer
- **Opens for F10 (Moods)**: Food variety affects mood

---

## Design Notes

### Why Start Simple (2-3 Crops)?

The AI discussions proposed a complex 5-variable soil system (clay/stone/fertilizer/wetness/calcium). That's brilliant but too much for the first farming implementation. Start with: "plant seed, it grows, harvest it, seasons matter." Add soil chemistry later when the base loop is fun.

### Irrigation (Not Yet)

Water placement tools exist in the todo list. Irrigation (channels, water delivery to farms) is a natural extension but not needed for Phase 1. Rain provides baseline watering. Manual watering with containers (F8) comes later.

### Crop Failure

Crops should be able to fail: frost kills them, drought stunts them, waterlogging rots them. This creates tension and makes weather forecasting (checking season progression) a real player skill. But failures should be partial — a bad season reduces yield, it doesn't destroy everything.

---

## Save Version Impact

- New cell data: crop type, growth stage, growth progress per farm cell
- New items: ITEM_WHEAT, ITEM_POTATO, ITEM_WHEAT_SEEDS, ITEM_POTATO_SEEDS, ITEM_BREAD, ITEM_ROASTED_POTATO
- New job types: JOBTYPE_TILL, JOBTYPE_PLANT_CROP, JOBTYPE_HARVEST
- New designation: DESIGNATION_FARM
- Hearth recipe additions

## Test Expectations

- ~40-50 new assertions
- Farm designation and tilling
- Crop planting and growth stages
- Seasonal growth modulation
- Temperature and wetness effects
- Harvesting and item production
- Cooking integration (crop → food)
- Frost damage and crop death
- Seed production loop (harvest → seed → replant)
