# 06a: Farming — Land Work

> Split from 06-farming-and-crops.md on 2026-02-24.
> **Deps**: 04d (spoilage — rotten items exist for compost input), tools (digging quality)
> **Opens**: 06b (crops & processing — needs tilled, fertile, watered land)

---

## Goal

Build the land preparation infrastructure for farming: designating farm plots, tilling soil, weeding, composting, and fertilizing. No plants yet — this is the "farmer's toolkit" that makes soil ready for crops in 06b.

After this feature, movers can:
1. Designate a farm zone on natural soil
2. Till the soil (needs digging tool)
3. Compost organic waste (rotten food, leaves, grass) into fertilizer
4. Spread fertilizer on farm cells
5. Weed farm cells to keep them maintained

Rain from the weather system handles watering. Manual watering deferred to 06c.

---

## Farm Grid

New per-cell grid tracking farm state. Follows the `sim_manager` pattern (like `floorDirtGrid`, `snowGrid`):

```c
typedef struct {
    uint8_t  fertility;       // 0-255, depletes on crop harvest (06b), restored by compost/lentils
    uint8_t  weedLevel;       // 0-255, accumulates over time on tilled cells, reset by tending
    uint8_t  tilled;          // 1 = has been tilled (uint8_t not bool, for packing)
    uint8_t  desiredCropType; // what player wants planted here (CROP_NONE = maintenance only)
    // 06b extends with: cropType, growthStage, frostDamaged, growthProgress
} FarmCell;

FarmCell farmGrid[MAX_Z][MAX_Y][MAX_X];  // zero-initialized = no farm data
int farmActiveCells;                      // count of tilled cells (for sim tick skipping)
```

**Packing note**: All `uint8_t` fields, no `bool` or `float`, to avoid alignment padding. `desiredCropType` is merged here instead of a separate grid — always accessed with other farm data. When 06b adds crop fields the full struct will be ~8 bytes per cell. For 128×128×32 cells = ~4MB (one grid for everything).

**Why a struct and not separate grids?** Farm cells are always accessed together (check tilled + read fertility + read weeds). A struct keeps them cache-friendly per cell. The grid is still zero-initialized so non-farm cells cost nothing.

**Fertility mechanics:**
- Starts at `FERTILITY_DEFAULT` (128) when first tilled
- Depleted once per harvest in 06b (wheat: -20, flax: -15, lentils: +60)
- Restored by fertilizing with compost (+80 per application, capped at 255)
- Restored by lentil harvest in 06b (+60, natural nitrogen fixation)
- At fertility 0: crops grow at 0.25x rate (stunted, not dead)
- Growth modifier: `fertilityMod = 0.25f + 0.75f * (fertility / 255.0f)`

**Weed mechanics:**
- Tilled cells accumulate weedLevel over time (weedGrowthRate per game-hour)
- Rate affected by season: spring/summer = 1.0x, autumn = 0.5x, winter = 0.0x
- At weedLevel > WEED_THRESHOLD (128): crop growth penalty 0.5x in 06b
- At weedLevel > WEED_SEVERE (200): crop growth penalty 0.25x in 06b
- Tending resets weedLevel to 0
- Untended farms visually show weeds (green tufts overlay)

**Watering (rain only for now):**
- Uses the existing `wetness` grid from the mud system — no new water tracking needed
- Rain automatically waters exposed farm cells (weather system already does this)
- Wetness affects crop growth in 06b: too dry = slow, good = 1.0x, waterlogged = slow
- Farm cells don't need their own water field — `GetCellWetness(x,y,z)` already works
- Manual watering (ITEM_WATER, JOBTYPE_WATER_CROP, JOBTYPE_FETCH_WATER) deferred to `06c-farm-watering.md`

---

## Soil Requirements

Only natural soil cells can be designated as farm plots:

| Material | Can Farm? | Initial Fertility | Notes |
|----------|-----------|-------------------|-------|
| MAT_DIRT | Yes | 128 (default) | Baseline, good all-around |
| MAT_CLAY | Yes | 110 | Holds water (risk of waterlogging in rain) |
| MAT_SAND | Yes | 90 | Drains fast, needs frequent watering |
| MAT_PEAT | Yes | 180 | Excellent but rare |
| MAT_GRAVEL | Yes | 64 | Poor soil, low starting fertility |
| Others | No | — | Rock, constructed floors, etc. not farmable |

Initial fertility varies by soil type — peat starts rich, gravel starts poor. This creates location strategy: seek out peat, avoid gravel, clay is risky in rainy seasons.

Check function:
```c
bool IsFarmableSoil(int x, int y, int z) {
    if (z == 0) return false;  // can't farm on bedrock layer
    // Must be walkable surface (air cell with solid below)
    // Solid cell below must be natural soil material
    uint8_t mat = grid[z-1][y][x].wallMaterial;
    return (mat == MAT_DIRT || mat == MAT_CLAY || mat == MAT_SAND ||
            mat == MAT_PEAT || mat == MAT_GRAVEL);
}
```

---

## Farm Designation

New designation: `DESIGNATION_FARM` — drag-rectangle on farmable soil cells.

- Mode: W → F (work menu → farm)
- Drag to designate rectangle
- Erase mode: un-farm (clears tilled state, kills any crop, resets farmZoneCropType)
- Only valid on farmable soil cells (others silently skipped)
- Designation is **consumed by tilling** (like mining designation). After tilling, the `tilled` flag + `farmZoneCropType` grid (06b) are the persistent farm state — no ongoing designation needed

Action registry entry:
```c
{ ACTION_FARM, "Farm Plot", "Farm Plot", "Farm", "f", 0, MODE_WORK, SUB_MODE_NONE, ACTION_WORK, true, true }
```

---

## New Jobs

### JOBTYPE_TILL

Mover tills a designated farm cell, preparing it for planting.

| Property | Value |
|----------|-------|
| Work time | 1.0 game-hours |
| Tool quality | QUALITY_DIGGING (speeds up) |
| Designation | DESIGNATION_FARM (consumed on completion) |
| Result | Cell marked as tilled, fertility set by soil type |

**WorkGiver_Till**: scan farm designations → find untilled cells → assign nearest mover with digging capability.

**Job steps:**
1. Walk to designated cell
2. Work (RunWorkProgress with digging tool speed)
3. On completion: `farmGrid[z][y][x].tilled = true`, `farmGrid[z][y][x].fertility = InitialFertilityForSoil(mat)`, `farmActiveCells++`, clear designation
4. Visual change: cell renders as tilled (darker, furrowed sprite)

### JOBTYPE_TEND_CROP

Mover weeds a farm cell that has become overgrown.

| Property | Value |
|----------|-------|
| Work time | 0.4 game-hours |
| Tool quality | None (bare hands) |
| Auto-assigned | Yes, when weedLevel > WEED_THRESHOLD |
| Result | weedLevel reset to 0 |

**WorkGiver_Tend**: scan tilled farm cells → find cells with weedLevel > WEED_THRESHOLD → assign nearest idle mover.

**Job steps:**
1. Walk to weedy farm cell
2. Work (0.4 game-hours)
3. On completion: `farmGrid[z][y][x].weedLevel = 0`

### JOBTYPE_FERTILIZE

Mover carries compost to a depleted farm cell.

| Property | Value |
|----------|-------|
| Work time | 0.3 game-hours |
| Requires | ITEM_COMPOST |
| Auto-assigned | Yes, when farm cell fertility < FERTILITY_LOW_THRESHOLD (64) and ITEM_COMPOST available |
| Result | Cell fertility increased by 80 (capped at 255) |

**WorkGiver_Fertilize**: scan tilled farm cells → find low-fertility cells → find available ITEM_COMPOST → assign mover.

**Job steps:**
1. Walk to compost item, pick up
2. Walk to low-fertility farm cell
3. Work (0.3 game-hours)
4. On completion: `farmGrid[z][y][x].fertility = MIN(255, fertility + 80)`, delete compost item

---

## New Items

| Item | Flags | Stack | Notes |
|------|-------|-------|-------|
| ITEM_COMPOST | IF_STACKABLE | 10 | Output of compost pile workshop. Used to fertilize. |

1 new item type. Save version bump required (item count 49 → 50).

**Water collection and manual watering deferred** to `06c-farm-watering.md`. For 06a, rain from the weather system is the only water source for farms. This is sufficient for dirt/clay/peat soils in normal climates. Sandy soil in dry seasons will struggle — that's intentional pressure to build watering infrastructure later.

---

## Compost Pile Workshop

New workshop: **WORKSHOP_COMPOST_PILE** (2x2)

| Property | Value |
|----------|-------|
| Template | `#X` / `O.` |
| Type | Semi-passive (ignite pattern: deliver + active start → passive timer) |
| Construction | 4 sticks, 0.5 game-hours build time |
| Location | Outdoor (no roof requirement, thematic) |

**Recipe: "Compost"**

| Field | Value |
|-------|-------|
| Input | 3× any item with CONDITION_ROTTEN |
| Output | 1× ITEM_COMPOST |
| workRequired | 0.5 game-hours (active: loading the pile) |
| passiveWorkRequired | 2.0 game-hours (decomposition timer) |
| Tool | None |

**Input matching**: The recipe input uses a new match mode — any organic waste. This is similar to how fuel matching works (any IF_FUEL item). Add a special sentinel:

```c
.inputType = ITEM_ANY_COMPOSTABLE,  // matches: rotten items OR leaves OR grass
```

In `DoesItemMatchRecipeInput()`: if recipe input is `ITEM_ANY_COMPOSTABLE`, accept any item where `GetItemCondition(idx) == CONDITION_ROTTEN` OR type is `ITEM_LEAVES` / `ITEM_GRASS`.

This ensures compost is producible even with good food management — players can always gather leaves/grass as compost input. Rotten food is a bonus input, not the only source.

---

## Farm Simulation Tick

New `FarmTick(float dt)` called from sim_manager, follows the same accumulator pattern:

```c
static float farmTickAccumulator = 0;
#define FARM_TICK_INTERVAL 0.5f  // game-hours

void FarmTick(float dt) {
    if (farmActiveCells == 0) return;  // skip if no farms
    farmTickAccumulator += dt;
    if (farmTickAccumulator < GameHoursToGameSeconds(FARM_TICK_INTERVAL)) return;
    farmTickAccumulator = 0;

    float seasonWeedRate = GetSeasonalWeedRate();  // 1.0 spring/summer, 0.5 autumn, 0.0 winter

    for (z, y, x) {
        if (!farmGrid[z][y][x].tilled) continue;
        // Weed growth
        int weedGrowth = (int)(WEED_GROWTH_PER_TICK * seasonWeedRate);
        farmGrid[z][y][x].weedLevel = MIN(255, farmGrid[z][y][x].weedLevel + weedGrowth);
    }
    // Crop growth handled in 06b
}
```

---

## Sprites & Visuals

### New Tiles (2 for 06a)

| Filename | Sprite Constant | Description |
|----------|----------------|-------------|
| `farm-tilled.png` | `SPRITE_farm_tilled` | Furrowed soil pattern — tinted by soil material color at render time (dirt/clay/sand/peat/gravel use existing `MaterialTint()`) |
| `farm-weedy.png` | `SPRITE_farm_weedy` | Green tuft overlay — alpha scales with weedLevel (drawn on top of tilled sprite) |

### Reused Sprites

| Item | Sprite | Tint |
|------|--------|------|
| ITEM_COMPOST | `SPRITE_finish_messy` | Dark brown {60, 45, 25} |
| ITEM_WATER | `SPRITE_division` | Blue {50, 120, 200} (same reuse pattern as berries with red) |

### Rendering Rules

| State | How |
|-------|-----|
| Tilled soil | Draw `SPRITE_farm_tilled` tinted with `MaterialTint(soilMat)` — one sprite works for all 5 soil types |
| Weedy (weedLevel > 128) | Overlay `SPRITE_farm_weedy` at alpha proportional to weedLevel (128→255 maps to alpha 80→200) |
| Fertilized recently | No visual (low priority, skip for now) |
| Wet farm cell | Already handled by mud system wetness rendering |

Farm cell rendering hooks into the existing terrain draw pass. Check `farmGrid[z][y][x].tilled` and overlay sprites accordingly.

---

## Save/Load

Save version bump: v75 → v76.

**New data in WORLD section:**
```c
// Save
fwrite(&farmActiveCells, sizeof(int), 1, f);
for (z, y, x) {
    fwrite(&farmGrid[z][y][x], sizeof(FarmCell), 1, f);
}

// Load (v76+)
fread(&farmActiveCells, sizeof(int), 1, f);
for (z, y, x) {
    fread(&farmGrid[z][y][x], sizeof(FarmCell), 1, f);
}
// Load (v75 and below): zero-initialize farmGrid, farmActiveCells = 0
```

Also save new items (ITEM_COMPOST, ITEM_WATER) — handled automatically by existing item save/load since they're just new enum values.

---

## Implementation Phases

### Phase 1: Farm Grid + Designation + Tilling
- Add `FarmCell` struct and `farmGrid` in new `farming.c`/`farming.h`
- Add `DESIGNATION_FARM` to designation enum
- Add `ACTION_FARM` to action registry (W → F)
- Add `IsFarmableSoil()` check
- Add `JOBTYPE_TILL` with WorkGiver_Till
- Tilling sets `tilled = true`, `fertility` by soil type
- Visual: tilled soil sprite/tint
- Add to `unity.c` and `test_unity.c`
- Save version bump for farmGrid
- **Test**: Designate farm on dirt/clay/sand/peat/gravel, verify tilling works, verify non-soil rejected

### Phase 2: Weeding
- Add weed accumulation in `FarmTick()`
- Add `JOBTYPE_TEND_CROP` with WorkGiver_Tend
- Seasonal weed rate (spring/summer = fast, winter = none)
- Visual: weed overlay on farm cells
- **Test**: Tilled cell accumulates weeds over time, tending resets, seasonal rate works

### Phase 3: Compost Pile + Fertilizing
- Add `WORKSHOP_COMPOST_PILE` (2x2, semi-passive)
- Add `ITEM_COMPOST` item type
- Add rotten-food input matching for compost recipe
- Add `JOBTYPE_FERTILIZE` with WorkGiver_Fertilize
- **Test**: Rotten food → compost pile → ITEM_COMPOST → apply to farm cell → fertility increases


---

## E2E Test Stories

### Story 1: Till farm on different soil types
- Create 5×1 strip with MAT_DIRT, MAT_CLAY, MAT_SAND, MAT_PEAT, MAT_GRAVEL below
- Designate all as farm
- Spawn mover with digging tool
- Tick until all tilled
- **Verify**: All 5 cells tilled, each has correct initial fertility (128, 110, 90, 180, 64)
- **Verify**: farmActiveCells == 5

### Story 2: Weeds accumulate and get cleared
- Till a farm cell
- Advance time in summer (fast weed growth)
- **Verify**: weedLevel increases each farm tick
- **Verify**: weedLevel reaches WEED_THRESHOLD
- Spawn mover, tick until tend job completes
- **Verify**: weedLevel == 0

### Story 3: Weeds don't grow in winter
- Till a farm cell
- Set season to winter
- Advance time
- **Verify**: weedLevel stays at 0

### Story 4: Compost rotten food
- Spawn 3 rotten items (berries with spoilageTimer >= spoilageLimit)
- Build compost pile workshop with "Compost" bill
- Spawn mover
- Tick through active + passive phases
- **Verify**: 1x ITEM_COMPOST spawned, 3 rotten items consumed

### Story 5: Fertilize depleted soil
- Till farm cell, manually set fertility to 30
- Spawn ITEM_COMPOST on ground
- Spawn mover
- Tick until fertilize job auto-assigned and completed
- **Verify**: fertility == 110 (30 + 80)

### Story 6: Fertilize caps at 255
- Till farm cell, set fertility to 220
- Spawn ITEM_COMPOST, spawn mover
- Tick until fertilize job completes
- **Verify**: fertility == 255 (not 300)

### Story 7: Non-farmable soil rejected
- Create cells with MAT_STONE, constructed floor, air
- Attempt to designate as farm
- **Verify**: designation not placed (silently skipped)

### Story 8: Digging tool speeds up tilling
- Set up two identical scenarios: farm designation + mover
- Mover A has digging stick (digging:1), Mover B is bare-handed
- Tick both, record completion time
- **Verify**: tool-equipped mover finishes tilling faster

---

## Job Priority in AssignJobs

Farm jobs slot into the existing priority order:

```
1.  StockpileMaintenance
2.  Crafting (includes compost pile)
2b. PassiveDelivery
2c. Ignition
3.  HARVEST_CROP (ripe crops waiting = time-sensitive, spoil risk)
4.  StockpileHaul
5.  ItemHaulFallback
6.  TILL / PLANT / TEND / FERTILIZE (farm maintenance — lower priority)
7.  ReHaul
```

Harvest is high priority because ripe crops can be killed by weather. Tilling/planting/tending are routine maintenance — important but not urgent.

---

## Farm Cell Tooltip

When hovering a tilled farm cell, show in tooltip:

```
Farm Plot (Dirt)
Fertility: ████░░░░ 80%
Weeds: Low
Wetness: Good
[Crop info from 06b: "Wheat (Growing - Stage 2/4)"]
```

Uses existing tooltip system. Fertility as a bar (like health bars), wetness and weeds as text labels. Crop info added in 06b.

---

## State Audit Integration

Add farm audit checks to `state_audit.c`:

1. **farmActiveCells matches reality**: recount all tilled cells, compare to `farmActiveCells`
2. **Crop only on tilled cells**: if `cropType != CROP_NONE` then `tilled` must be true
3. **Fertility bounds**: fertility must be 0-255 (implicit from uint8_t, but verify no corruption)

---

## Connections

- **Uses**: mud/wetness system (watering), spoilage system (rotten input for compost), weather (rain waters farms), seasons (weed growth rate), tool quality (tilling speed), balance system (game-hour conversions)
- **Opens**: 06b (crops need tilled + fertile + watered land to grow)
- **Opens**: 06c (manual watering — water collection + carrying to farms)
- **Opens**: 08 (thirst — water collection system reused for drinking water)
- **Deferred**: Irrigation channels (water flowing to farms), crop rotation UI, soil amendment types (lime, bone meal)
