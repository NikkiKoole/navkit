# 04a: Butchering & Cooking

> Split from 04-cooking-and-hunting.md on 2026-02-22.
> **Deps**: 03 (fire pit + ground fire exist)
> **Opens**: 04b (hunting), 07 (clothing via hides)

---

## Goal

Build the processing chain: carcass → butcher → raw meat → cook → cooked meat → eat. This is the smallest useful loop — once it works, animals become a food source. The hunting job (04b) connects animals to this chain later.

For now, carcasses can appear from predator kills or sandbox spawning.

---

## New Items

| Item | Flags | Stack | Nutrition | Notes |
|------|-------|-------|-----------|-------|
| ITEM_CARCASS | — | 1 | — | Non-stackable, must be butchered |
| ITEM_RAW_MEAT | IF_EDIBLE, IF_STACKABLE | 5 | +0.2 | Edible raw but poor nutrition |
| ITEM_COOKED_MEAT | IF_EDIBLE, IF_STACKABLE | 5 | +0.5 | Best food so far |
| ITEM_HIDE | IF_STACKABLE | 5 | — | No use until clothing (07) |

4 new item types. Save version bump required (item count 40 → 44).

---

## Butcher Workshop

New workshop: **WORKSHOP_BUTCHER** (1x1)

| Property | Value |
|----------|-------|
| Template | `X` (work tile only) |
| Passive | No (active crafter) |
| Construction | 2 sticks + 1 rock, 1s |

**Bill**: "Butcher Carcass" — input 1x CARCASS, work time 3s, tool: cutting quality speeds it up.

**Output is NOT a recipe.** Butchering uses a custom yield table instead of the Recipe output fields, because butchering can produce 2+ product types now and more later (bones, fat, sinew). The recipe system caps at 2 outputs — the yield table has no limit.

### Butcher Yield Table

```c
typedef struct {
    ItemType type;
    int      count;
} ButcherProduct;

typedef struct {
    int             productCount;
    ButcherProduct  products[MAX_BUTCHER_PRODUCTS]; // 8 is plenty
} ButcherYieldDef;
```

Yield is looked up by carcass material (= animal species). A `DEFAULT` entry covers any species without a specific entry.

**Phase 1 (this feature) — single default entry:**

| Product | Count | Notes |
|---------|-------|-------|
| ITEM_RAW_MEAT | 3 | Main food output |
| ITEM_HIDE | 1 | Clothing chain (07) |

**Future entries (when species are added in 04b+):**

| Species | Meat | Hide | Bones | Fat | Sinew |
|---------|------|------|-------|-----|-------|
| Default | 3 | 1 | — | — | — |
| Rabbit | 1 | 1 | — | — | — |
| Deer | 5 | 1 | 2 | 1 | 1 |
| Boar | 4 | 1 | 1 | 2 | — |
| Bear | 8 | 2 | 3 | 2 | 1 |

These are illustrative — actual values tuned when species are implemented. The point is: adding a row to this table is all it takes.

### How it integrates with workshops

The butcher workshop uses a normal Recipe for its **input side** (input matching, work time, tool requirements) with output fields set to `ITEM_NONE`. The **output side** is custom: in `CRAFT_STEP_WORKING` completion (jobs.c), when `ws->type == WORKSHOP_BUTCHER`, branch to yield table spawning instead of the normal `recipe->outputType` path. This reuses the entire craft job state machine (bill assignment, WorkGiver_Craft, material delivery, work progress, tool speed) with only the output step overridden.

```c
// In CRAFT_STEP_WORKING, after progress >= 1.0:
if (ws->type == WORKSHOP_BUTCHER) {
    // Look up yield by carcass material
    ButcherYieldDef* yield = GetButcherYield(inputMat);
    for (int i = 0; i < yield->productCount; i++) {
        int idx = SpawnItem(outX, outY, ws->z, yield->products[i].type);
        if (idx >= 0) items[idx].stackCount = yield->products[i].count;
    }
} else {
    // Normal recipe output spawning (existing code)
}
```

This is the smallest change to the craft job — one `if` branch. No new job type needed.

Why a workshop and not a designation: workshops have the bill system, material delivery, and work progress already working. A 1x1 workshop is minimal. The butcher spot is just a flat rock where you work.

**Note**: 1x1 is fine while there are only 2 output types (meat + hide). When bones/fat/sinew arrive, upgrade to 1x2 (`XO`) with a dedicated output tile to avoid piling 5+ item types on a single cell.

---

## Cooking Recipes

Add cooking recipes to **fire pit** and **ground fire** (both share `campfireRecipes[]`):

| Recipe | Input | Output | workRequired | passiveWorkRequired |
|--------|-------|--------|-------------|---------------------|
| Cook Meat | 1x RAW_MEAT | 1x COOKED_MEAT | 1.0f | 15.0f |

This uses the existing **semi-passive (ignite+passive) pattern**: mover gets assigned `JOBTYPE_IGNITE_WORKSHOP`, walks to fire, does 1s active work, sets `passiveReady=true`. Then `PassiveWorkshopsTick()` runs the 15s passive timer and spawns output. Same flow as "Burn Sticks" (`workRequired=3.0f, passiveWorkRequired=30.0f`) but faster.

The recipe is appended to the existing `campfireRecipes[]` array in workshops.c — both fire pit (WORKSHOP_CAMPFIRE) and ground fire (WORKSHOP_GROUND_FIRE) share this array, so both get the recipe automatically.

Also add to **hearthRecipes[]** for batch cooking:
- "Cook Meat": 2x RAW_MEAT → 2x COOKED_MEAT, `workRequired=2.0f`, `passiveWorkRequired=0.0f` (hearth is actively tended, no passive phase)

---

## Animal Kill → Carcass

Modify existing predator kill logic in `animals.c`:

**Current** (line ~665):
```c
animals[bestPrey].active = false;  // just disappears
```

**New**:
```c
KillAnimal(bestPrey);  // deactivates + spawns carcass
```

New function `KillAnimal(int animalIdx)` (public, declared in animals.h — reused by 04b hunting):
1. Get animal position
2. Set `animals[animalIdx].active = false` (do NOT decrement `animalCount` — it's a high-water mark like `itemHighWaterMark`, used for iteration bounds)
3. `SpawnItem(x, y, z, ITEM_CARCASS)` at animal position
4. Carcass material = `MAT_NONE` for Phase 1 (all carcasses use default yield). When 04b adds species, set material to species type for per-species yield lookup.

This means predator kills now produce free carcasses on the ground — movers can haul and butcher them without needing to hunt.

---

## Eating Integration

Cooked meat and raw meat both have IF_EDIBLE, so the existing auto-eat system picks them up automatically:
- Movers already search stockpiles then ground for nearest edible item
- No food preference needed yet (nearest-first is fine for now)
- Cooked meat (+0.5) vs berries (+0.3) vs raw meat (+0.2) — cooking is always worth it
- **Fast follow-up**: add food preference by nutrition value in needs.c food search, so movers prefer cooked meat over raw when both are reachable. Without this, movers eat the nearest food regardless of quality, wasting cooked meals.

---

## Food Quality Hierarchy (after this feature)

| Food | Nutrition | Available |
|------|-----------|-----------|
| Cooked meat | +0.5 | Cook raw meat at fire |
| Berries | +0.3 | Forage from bushes |
| Dried berries | +0.25 | Dry at drying rack |
| Raw meat | +0.2 | Butcher carcass |

---

## Implementation Phases

### Phase 1: Items + Butcher Workshop
- Add 4 item types to items.h enum (ITEM_CARCASS, ITEM_RAW_MEAT, ITEM_COOKED_MEAT, ITEM_HIDE)
- Add item definitions in item_defs.c (flags, nutrition, stack sizes)
- Add `WORKSHOP_BUTCHER` to WorkshopType enum in workshops.h
- Add workshop definition in workshops.c (1x1, template "X", active, not passive)
- Add butcher recipe to `butcherRecipes[]` (input: 1x CARCASS, output: ITEM_NONE — output handled by yield table)
- Add ButcherYieldDef table with default entry (3 meat + 1 hide) — new file `butchering.c`/`.h` or inline in workshops.c
- Add `CONSTRUCTION_WORKSHOP_BUTCHER` to construction enum in construction.h
- Add construction recipe in construction.c (2 sticks + 1 rock, buildTime 1.0f)
- Custom output branch in CRAFT_STEP_WORKING (jobs.c): `if (ws->type == WORKSHOP_BUTCHER)` → loop yield table
- Add workshop action in action_registry + input (under BUILD_WORKSHOP parent)
- Add to unity.c and test_unity.c if new .c files
- Save version bump
- **Test**: Spawn carcass, verify butcher job produces correct outputs from yield table

### Phase 2: Cooking Recipes
- Append "Cook Meat" recipe to `campfireRecipes[]` in workshops.c (workRequired=1.0f, passiveWorkRequired=15.0f)
- Append "Cook Meat" recipe to `hearthRecipes[]` (workRequired=2.0f, passiveWorkRequired=0.0f, 2x input → 2x output)
- **Test**: Raw meat → cooked meat at fire pit via ignite+passive flow, verify nutrition value

### Phase 3: Kill → Carcass
- Add `KillAnimal()` function in animals.c
- Modify predator kill to use KillAnimal()
- Carcass spawns at animal death position
- **Test**: Predator kills grazer → carcass appears at correct position

### Phase 4: Stockpile Filters + Polish
- Add stockpile filter keys for new items. **All single-char keys (a-z, 1-9) are taken.** Options: use `0` for one item, or implement a second filter page / shift-key modifier. Simplest: `0`=Carcass, and group meat/hide under a food/animal category toggle. Decide during implementation.
- Add items to tooltips display (item name, nutrition value for edibles)
- Verify auto-eat works with cooked/raw meat (needs.c food-seeking picks up IF_EDIBLE)
- **Test**: Mover eats cooked meat, hunger restores +0.5

---

## E2E Test Stories

### Story 1: Butcher carcass produces meat and hide
- Spawn carcass on ground near butcher workshop
- Set up bill (DO_X_TIMES, 1)
- Spawn idle mover nearby
- Tick until job completes
- **Verify**: 3x ITEM_RAW_MEAT + 1x ITEM_HIDE spawned at workshop position
- **Verify**: carcass consumed (deleted)
- **Verify**: bill completedCount == 1

### Story 2: Cook raw meat at fire pit (passive)
- Spawn raw meat on ground
- Set up fire pit with "Cook Meat" bill
- Spawn idle mover
- Tick through active phase (mover delivers meat, 1s work)
- Tick through passive phase (15s cooking timer)
- **Verify**: 1x ITEM_COOKED_MEAT spawned, raw meat consumed

### Story 3: Predator kill spawns carcass
- Spawn predator and grazer close together (within catch distance)
- Tick until predator catches prey
- **Verify**: grazer deactivated, animalCount decremented by 1
- **Verify**: ITEM_CARCASS spawned at grazer's death position
- **Verify**: carcass material == MAT_NONE (Phase 1 default)

### Story 4: Full chain — kill → butcher → cook → eat
- Spawn predator + grazer close together
- Spawn butcher workshop with "Butcher" bill (DO_FOREVER)
- Spawn fire pit with "Cook Meat" bill (DO_FOREVER)
- Spawn mover with hunger enabled, hunger near threshold
- Tick until mover has eaten
- **Verify**: mover's hunger increased (cooked meat nutrition applied)
- **Verify**: no carcass, no raw meat remaining (all processed through chain)

### Story 5: Cutting tool speeds up butchering
- Set up two identical scenarios: carcass + butcher workshop + mover
- Mover A has stone axe (cutting:2), mover B is bare-handed
- Tick both, record completion time
- **Verify**: tool-equipped mover finishes butchering faster (via GetJobToolSpeedMultiplier)

### Story 6: Mover eats raw meat when no cooking available
- Spawn raw meat on ground, no fire pit
- Spawn mover with hunger below eat threshold
- Tick until mover seeks and eats food
- **Verify**: hunger increased by raw meat nutrition (+0.2)
- **Verify**: raw meat item consumed

---

## Connections

- **Uses**: fire pit/ground fire (03), hearth workshop, hunger system, craft job state machine (bill/delivery side only — output bypasses recipe system)
- **Opens**: 04b (hunting adds the hunt job that feeds carcasses into this chain)
- **Opens**: 07 (ITEM_HIDE → leather → clothing)
- **Deferred**: spoilage (04d), food preference ordering, different meat per species (requires an animal species system — not in 04a or 04b, both only have GRAZER/PREDATOR types)

---

## Reference: How Other Games Handle Butchering

### Dwarf Fortress — Maximum Product Variety

A single butchered animal can produce up to 10 different product types. No skill effect on yield (butchering is binary). No quality on products — quality enters at the cooking/crafting stage.

| Product | Downstream Use |
|---------|---------------|
| Meat (muscle) | Cook into meals at kitchen |
| Prepared organs (brain, tripe, sweetbread) | Cook into meals |
| Fat → Tallow (rendered at kitchen) | Soap production, or low-value meals |
| Raw hide | Tan into leather → clothing, armor, bags |
| Bones | Bone crafts, bolts, crossbows, decorations |
| Skulls | Totems (trade goods) |
| Horns/Hooves | Craft trade goods |
| Teeth/Ivory | Craft trade goods |
| Hair | Spin into thread (only for suturing) |
| Cartilage/Nervous tissue | Waste (no use) |
| Shells (some creatures) | Strange mood demands, crafting |

- Yields scale with **creature body size**
- Killed-in-combat animals can produce **multiple hides** if chopped into pieces (each severed part has its own hide)
- Tame animals cleanly slaughtered produce exactly one hide
- Butcher's shop is the only facility needed — no multi-stage pipeline

### RimWorld — Skill-Scaled Efficiency

Only two products (meat + leather), but yield is heavily modified by skill and equipment.

**Products:**

| Product | Notes |
|---------|-------|
| Meat | Type varies by animal (pork, chicken, insect meat, etc.) |
| Leather | Type varies (thrumbofur, plainleather, etc. — 40+ types) |

**Base meat yields by animal body size:**

| Category | Examples | Base Meat |
|----------|----------|-----------|
| Huge | Elephant, Megasloth, Thrumbo | 560 |
| Large | Rhinoceros | 420 |
| Medium-Large | Cow, Horse, Bison, Muffalo | 336 |
| Medium | Pig | 238 |
| Small | Chicken, Duck | 42 |
| Tiny | Rat, Squirrel | 31 |

**Yield modifiers (all multiplicative):**

| Factor | Effect |
|--------|--------|
| Cooking skill | +2.5% per level (up to +50% at skill 20) |
| Manipulation | 90% importance weight |
| Sight | 40% importance weight (capped at 100%) |
| Butcher spot (vs table) | 70% efficiency (table = 100%) |
| Not carefully slaughtered (hunted/killed) | x0.66 penalty |
| Malnutrition | x0.4 at 100% malnutrition, linear scale |

- Efficiency **can exceed 100%** — a bionic butcher with cooking 20 gets ~170% yield
- No quality on butchered products — quality only applies to cooked meals
- Leather yield uses the same modifier stack as meat

### Cataclysm: DDA — Multi-Stage Processing Pipeline

Most realistic system with 5 distinct butchering actions. Tool quality and survival skill both matter.

**Butchering stages:**

| Stage | Requires | What It Does |
|-------|----------|-------------|
| Field dress | Cutting 1 | Removes guts, reduces weight and rot speed |
| Skin | Cutting 1 | Removes pelt/hide separately |
| Quarter | Cutting 1 | Splits field-dressed corpse into 4 portable pieces |
| Full butcher | Cutting 1 + table/rack | Maximum yield of all products |
| Dismember | Any cutting tool | Emergency — 1/6 meat, rest is refuse |

**Products from full butchery:**

| Product | Use |
|---------|-----|
| Meat | Cooking |
| Offal (organ meat) | Cooking (lower value) |
| Fat | Cooking, rendering |
| Bones | Crafting, glue |
| Sinew | Cordage, bowstrings |
| Skin/Pelt/Hide | Leather crafting |
| Feathers (birds only) | Arrow fletching, pillows |

**Quick butcher (no tools/table) yield penalties:**

| Product | Quick vs Full |
|---------|--------------|
| Meat | 25% |
| Bones | 50% |
| Organs | 20% |
| Skin | 50% |

**Corpse damage effects:**
- Damaged/pulped corpses: 50% meat, 50% skin, no usable organs
- Survival skill reduces chance of damaging corpse during butchering
- Tool quality affects speed but (debated) not yield amount

### Cross-Game Summary

| Aspect | DF | RimWorld | CDDA |
|--------|-----|----------|------|
| Product types | ~10 | 2 | ~7 |
| Skill affects yield | No | Yes (cooking) | Yes (survival) |
| Tool affects yield | No | No (spot vs table only) | Speed + stage access |
| Quality on products | No | No | No |
| Multi-stage pipeline | No | No | Yes (5 stages) |
| Yield scales with body size | Yes | Yes | Yes |
| Equipment tier matters | No | Spot (70%) vs Table (100%) | Need table/rack for full butcher |
| Can exceed base yield | No | Yes (170%+) | No |

**Common patterns across all three:**
1. No quality on butchered products — quality enters at cooking/crafting
2. Larger creatures yield more — body size is the main scaling factor
3. Hide/leather is always a byproduct — gateway to clothing/armor chain
4. Butchering station is simple — the complexity is in what you do with the products
