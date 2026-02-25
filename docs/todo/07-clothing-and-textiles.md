# Feature 07: Clothing & Textiles

> **Priority**: Tier 2b — Material & Comfort Loop
> **Updated**: 2026-02-25 (rewritten with cross-game research and 06b integration)
> **Deps**: 03 (shelter), 04a (butchering → ITEM_HIDE), 06b (flax fiber)
> **Opens**: 10 (mood from clothing quality), future armor, trade value
> **Research**: `docs/vision/clothing-textiles-research.md` (DF/CDDA/RimWorld/Sims analysis)

---

## What Changes

A **loom** weaves dried grass or flax fiber into cloth/linen. A **tanning rack** converts animal hides into leather. A **tailor's bench** sews cloth and leather into clothing. Wearing clothing **slows body heat loss** — movers stay warm longer in cold weather, delaying speed penalties and hypothermia.

Shelter (03) protects indoors. Clothing protects outdoors. Together they close the temperature survival loop.

---

## Existing Systems (already built, no work needed)

The old doc treated temperature effects as Phase 1 work. **This is already done:**

- **Body temperature**: `bodyTemp` on Mover struct (37°C normal, range 20-42°C)
- **Cold speed penalty**: Linear from `mildColdThreshold` (35°C) to `moderateColdThreshold` (33°C)
- **Heat speed penalty**: Linear from `heatThreshold` (40°C) to 42°C
- **Hypothermia death**: `hypothermiaTimer` counts at `severeColdThreshold` (32°C), death after `hypothermiaDeathGH` (8 GH)
- **Wind chill**: `GetWindChillTemp()` lowers effective ambient for exposed movers
- **Seek warmth**: Movers trigger seek-warmth freetime state when `bodyTemp < moderateColdThreshold`
- **Cooling/warming rates**: `bodyTempCoolingRatePerGH` (2.0) and `bodyTempWarmingRatePerGH` (8.0) in balance system

**What clothing needs to do**: Reduce the effective cooling rate when equipped. A clothed mover cools slower, so their body temp stays higher longer in cold conditions. This is simple — multiply `bodyTempCoolingRatePerGH` by a clothing factor (e.g., 0.5 for leather coat = cools at half rate).

---

## How Clothing Works

### Mechanic: Cooling Rate Reduction

Each clothing item has a `coolingReduction` value (0.0 to 1.0). This multiplies the body temperature cooling rate:

```c
float clothingFactor = 1.0f;  // no clothing = full cooling
if (m->equippedClothing >= 0) {
    clothingFactor = 1.0f - GetClothingCoolingReduction(m->equippedClothing);
}
float effectiveCoolingRate = balance.bodyTempCoolingRatePerGH * clothingFactor;
```

| Clothing | coolingReduction | Effect |
|----------|-----------------|--------|
| None | 0.0 | Full cooling (2.0 °C/GH) |
| Grass tunic | 0.25 | Cools at 1.5 °C/GH |
| Flax tunic | 0.40 | Cools at 1.2 °C/GH |
| Leather vest | 0.50 | Cools at 1.0 °C/GH |
| Leather coat | 0.65 | Cools at 0.7 °C/GH |

**Why cooling rate, not a temperature offset?** A temperature offset (+5°C, +10°C) would be a static bonus that makes clothing equally valuable at 5°C and -20°C. Reducing cooling rate is more realistic — clothing doesn't make the air warmer, it slows how fast you lose body heat. In mild cold, clothing barely matters. In extreme cold, it's the difference between life and death. This creates the right gameplay pressure.

**Why not heat protection?** In a stone-age setting, you protect against heat by seeking shade and water, not by wearing more clothing. Adding heat insulation would mean clothing is useful year-round, removing the seasonal pressure that makes the system interesting. Cold-only for now.

### Body Slot

**1 slot: body.** Add `int equippedClothing` to Mover struct (same pattern as `equippedTool`, -1 = none).

**Why just 1 slot?** DF has 8+ slots, CDDA has 12, RimWorld has 4-5. But all those games have hundreds of clothing items to fill those slots. We have 4. One slot with 4 tiers (grass → flax → leather vest → leather coat) provides a clear progression without empty slots or meaningless choices. Add head/feet slots later when we have items to put in them.

### Equipping

Follow the `equippedTool` pattern exactly:
- Movers auto-seek clothing when `bodyTemp < mildColdThreshold` and `equippedClothing == -1`
- Search stockpiles for best available clothing item (highest coolingReduction)
- Walk to item → pick up → equip (set `equippedClothing = itemIdx`)
- Clothing persists until mover dies or item degrades (future)
- **Deterministic, not probabilistic** — research shows The Sims' probabilistic outfit switching causes NPCs to die. If cold AND clothing exists → equip it, always.

**No unequipping in warm weather** (for now). Movers keep clothing on year-round once equipped. Adds simplicity. If we add encumbrance or heat penalties later, we'd add seasonal un-equipping.

---

## Items

### Raw Materials (already exist)

| Item | Source | Status |
|------|--------|--------|
| ITEM_DRIED_GRASS | Drying rack (from ITEM_GRASS) | Exists |
| ITEM_FLAX_FIBER | Farm harvest (from 06b) | Exists |
| ITEM_HIDE | Butchering animals (from 04a) | Exists |
| ITEM_CORDAGE | Rope maker | Exists |

### New Intermediate Materials (3 items)

| Item | Flags | Stack | Source |
|------|-------|-------|--------|
| ITEM_CLOTH | IF_STACKABLE | 10 | Loom (from dried grass or cordage) |
| ITEM_LINEN | IF_STACKABLE | 10 | Loom (from flax fiber) |
| ITEM_LEATHER | IF_STACKABLE | 10 | Tanning rack (from hide) |

### New Clothing Items (4 items)

| Item | Flags | Stack | coolingReduction | Recipe |
|------|-------|-------|-----------------|--------|
| ITEM_GRASS_TUNIC | IF_CLOTHING | 1 | 0.25 | 3× ITEM_CLOTH |
| ITEM_FLAX_TUNIC | IF_CLOTHING | 1 | 0.40 | 2× ITEM_LINEN |
| ITEM_LEATHER_VEST | IF_CLOTHING | 1 | 0.50 | 2× ITEM_LEATHER |
| ITEM_LEATHER_COAT | IF_CLOTHING | 1 | 0.65 | 2× ITEM_LEATHER + 1× ITEM_CLOTH |

**7 new items total** (3 materials + 4 clothing). Save version bump v77 → v78.

Clothing items are NOT stackable (like tools). Each is a distinct item that gets equipped. `IF_CLOTHING` is a new item flag (bit 7, after IF_CONTAINER on bit 6) used for stockpile filtering and equip logic.

### Progression

```
Naked           → 0.0 reduction → full cooling, dies in winter
Grass tunic     → 0.25          → survives mild cold, struggles in winter storms
Flax tunic      → 0.40          → comfortable in autumn/spring cold, manageable in winter
Leather vest    → 0.50          → handles winter, still vulnerable to blizzards
Leather coat    → 0.65          → handles most winter conditions
```

The gap between grass and leather is intentional. Grass tunic is the "I have *something*" tier. Flax fills the middle — rewarding players who invested in 06b farming. Leather is the real goal but requires successful hunting.

---

## Production Chains

```
GRASS PATH (immediate bootstrap, weakest):
  ITEM_GRASS → Drying rack → ITEM_DRIED_GRASS
  ITEM_DRIED_GRASS × 4 → Loom → ITEM_CLOTH
  ITEM_CLOTH × 3 → Tailor bench → ITEM_GRASS_TUNIC

CORDAGE PATH (alternative cloth source):
  ITEM_CORDAGE × 2 → Loom → ITEM_CLOTH

FLAX PATH (requires farming from 06b, medium):
  ITEM_FLAX_FIBER × 3 → Loom → ITEM_LINEN
  ITEM_LINEN × 2 → Tailor bench → ITEM_FLAX_TUNIC

LEATHER PATH (requires hunting from 04b, strongest):
  ITEM_HIDE × 1 → Tanning rack (passive) → ITEM_LEATHER
  ITEM_LEATHER × 2 → Tailor bench → ITEM_LEATHER_VEST
  ITEM_LEATHER × 2 + ITEM_CLOTH × 1 → Tailor bench → ITEM_LEATHER_COAT
```

Three parallel paths converging at the tailor bench, mirroring the food system (berries vs roots vs meat). Grass is always available. Flax rewards farming investment. Leather rewards hunting. The leather coat requires BOTH leather and cloth — encouraging players to run multiple paths.

---

## New Workshops

### Loom (2×1, active)

```
X O
```

| Property | Value |
|----------|-------|
| Construction | 4 sticks + 2 cordage, 1.0 GH build |
| Type | Active (mover operates) |

| Recipe | Input | Output | workRequired |
|--------|-------|--------|-------------|
| Weave cloth | ITEM_DRIED_GRASS × 4 | ITEM_CLOTH × 1 | 1.0 GH |
| Weave from cordage | ITEM_CORDAGE × 2 | ITEM_CLOTH × 1 | 0.8 GH |
| Weave linen | ITEM_FLAX_FIBER × 3 | ITEM_LINEN × 1 | 1.2 GH |

### Tanning Rack (2×1, passive like drying rack)

```
X O
```

| Property | Value |
|----------|-------|
| Construction | 4 sticks, 0.5 GH build |
| Type | Semi-passive (deliver hide, wait, collect leather) |

| Recipe | Input | Output | workRequired | passiveWorkRequired |
|--------|-------|--------|-------------|---------------------|
| Tan hide | ITEM_HIDE × 1 | ITEM_LEATHER × 1 | 0.3 GH | 4.0 GH |

Follows the drying rack pattern exactly. Deliver, passive timer, output appears.

**Tanning without chemicals**: Historically requires bark tannins or urine. For now, the tanning rack is time-only. Future enhancement: require ITEM_BARK as input (already exists from tree processing). The workshop/recipe system supports adding inputs later without rework.

### Tailor's Bench (2×1, active)

```
X O
```

| Property | Value |
|----------|-------|
| Construction | 4 sticks + 2 planks, 1.0 GH build |
| Type | Active (mover sews) |

| Recipe | Input | Output | workRequired |
|--------|-------|--------|-------------|
| Grass tunic | ITEM_CLOTH × 3 | ITEM_GRASS_TUNIC × 1 | 1.5 GH |
| Flax tunic | ITEM_LINEN × 2 | ITEM_FLAX_TUNIC × 1 | 2.0 GH |
| Leather vest | ITEM_LEATHER × 2 | ITEM_LEATHER_VEST × 1 | 2.5 GH |
| Leather coat | ITEM_LEATHER × 2 + ITEM_CLOTH × 1 | ITEM_LEATHER_COAT × 1 | 3.0 GH |

---

## Auto-Equip Job

### JOBTYPE_EQUIP_CLOTHING

Follows the tool-seeking pattern from `STEP_FETCHING_TOOL`:

| Property | Value |
|----------|-------|
| Trigger | `bodyTemp < mildColdThreshold` AND `equippedClothing == -1` |
| Priority | Between crafting and hauling (priority ~2.5) |
| Behavior | Find best clothing in stockpile → walk → equip |

**WorkGiver_EquipClothing**: Scan movers where `equippedClothing == -1` and `bodyTemp < mildColdThreshold`. Find nearest unreserved clothing item (prefer highest coolingReduction). Create job.

**Job steps:**
1. Walk to clothing item
2. Pick up + equip (`m->equippedClothing = itemIdx`, `items[itemIdx].state = ITEM_EQUIPPED`)

Clothing items get `ITEM_EQUIPPED` state (or reuse `ITEM_CARRIED` semantics — item is owned by mover, moves with them, not visible on ground).

**Upgrade logic**: If a mover has clothing but better clothing exists in stockpiles, they should upgrade. Check periodically (e.g., during idle time): if stockpile has clothing with `coolingReduction > current + 0.1`, seek upgrade. Drop old clothing on ground for others to use.

---

## What We're NOT Doing (and why)

### No degradation (yet)
DF and RimWorld both use clothing degradation to create perpetual textile demand. It's great design. But it adds: wear tracking per item, repair jobs, replacement logic, mood penalties for tattered clothing. **Defer to a later pass.** Get the core equip-and-stay-warm loop working first. When mood system arrives (10), add degradation + "tattered clothing" mood penalty together.

### No layering (yet)
CDDA's 7-layer system and RimWorld's 3-layer system enable deep equipment choices. But with 4 clothing items, layering is meaningless — there's nothing to combine. **Add when we have 8+ clothing items** spanning different body regions.

### No encumbrance (yet)
CDDA shows encumbrance creates great tradeoffs (protection vs mobility). Not needed with 1 slot and cold-only protection. **Add when layering arrives**, so players face real armor-vs-speed decisions.

### No heat protection
Stone-age heat protection = shade and water, not clothing. Adding heat insulation makes clothing useful year-round, removing the seasonal tension. **Cold-only keeps the survival pressure seasonal** — you MUST prepare for winter.

### No quality (yet)
DF cascades quality across production steps. RimWorld makes quality the deepest multiplier (legendary = 1.8× stats). Both are great but require a crafting skill system we don't have. **Add when personality/skills (10) arrives** — crafting skill → clothing quality → better stats.

### No tainted clothing
RimWorld taints dead people's clothing to prevent scavenging. We don't have combat looting yet. **Add when enemies/combat arrives.**

---

## Stockpile Filters

Add `FILTER_CAT_TEXTILES` category for new items:

| Key | Item |
|-----|------|
| C | ITEM_CLOTH |
| N | ITEM_LINEN |
| L | ITEM_LEATHER |
| G | ITEM_GRASS_TUNIC |
| X | ITEM_FLAX_TUNIC |
| V | ITEM_LEATHER_VEST |
| O | ITEM_LEATHER_COAT |

---

## Save/Load

**Save version**: v77 → v78

- New Mover field: `int equippedClothing` (default -1)
- 7 new item types: ITEM_CLOTH, ITEM_LINEN, ITEM_LEATHER, ITEM_GRASS_TUNIC, ITEM_FLAX_TUNIC, ITEM_LEATHER_VEST, ITEM_LEATHER_COAT
- 3 new workshops: WORKSHOP_LOOM, WORKSHOP_TANNING_RACK, WORKSHOP_TAILOR
- Stockpile migration: expand allowedTypes[] from 66 → 73
- MoverV77 struct: old mover without equippedClothing field
- `saveload.c` + `inspect.c`: parallel migration

---

## Implementation Phases

### Phase 1: Items + Materials + Workshops
- Add 7 item types to `items.h` + `item_defs.c`
- Add IF_CLOTHING flag
- Add WORKSHOP_LOOM, WORKSHOP_TANNING_RACK, WORKSHOP_TAILOR to `workshops.h/c`
- Add construction costs in `construction.h/c`
- Add workshop placement in `input.c`, `action_registry.c`, `designations.c`
- Add loom/tanning/tailor recipes
- Save version bump + migration
- Stockpile filters
- **Test**: Weaving, tanning, tailoring recipes produce correct items

### Phase 2: Clothing Equipping + Temperature Integration
- Add `equippedClothing` to Mover struct
- Add `GetClothingCoolingReduction()` function
- Modify body temp cooling calculation in `mover.c` to apply clothing factor
- Add JOBTYPE_EQUIP_CLOTHING with WorkGiver
- Add to AssignJobs priority order
- **Test**: Clothed mover cools slower than naked mover, equips from stockpile

### Phase 3: Rendering + Tooltips
- Clothing items: sprites in rendering.c (reuse/tint existing sprites)
- Mover clothing overlay (tinted rectangle or sprite on mover when equipped)
- Tooltip: show equipped clothing name + coolingReduction in mover tooltip
- Workshop sprites for loom/tanning rack/tailor bench

### Phase 4: Upgrade Logic
- Movers with clothing check for better options during idle time
- Drop old clothing, equip new
- **Test**: Mover upgrades from grass tunic to leather coat when available

---

## E2E Test Stories

### Story 1: Full grass tunic chain
- Spawn ITEM_DRIED_GRASS × 12, build loom + tailor bench
- Tick until cloth produced (loom) → tunic produced (tailor)
- Verify: ITEM_GRASS_TUNIC created

### Story 2: Flax linen path
- Spawn ITEM_FLAX_FIBER × 6, build loom + tailor bench
- Tick until linen produced → flax tunic produced
- Verify: ITEM_FLAX_TUNIC created, flax fiber consumed

### Story 3: Leather path
- Spawn ITEM_HIDE × 2, build tanning rack + tailor bench
- Tick through passive tanning → leather vest at tailor
- Verify: ITEM_LEATHER_VEST created, hides consumed

### Story 4: Clothing reduces cooling rate
- Spawn mover, set ambient temp to 5°C (cold), equip leather vest
- Spawn second mover, same conditions, no clothing
- Tick both for N game-hours
- Verify: clothed mover bodyTemp > naked mover bodyTemp

### Story 5: Auto-equip when cold
- Spawn mover, set ambient temp to cold, clothing in stockpile
- Tick until equip job assigned and completed
- Verify: mover.equippedClothing != -1, item removed from stockpile

### Story 6: Upgrade to better clothing
- Equip mover with grass tunic, place leather coat in stockpile
- Tick until upgrade happens
- Verify: mover now has leather coat, grass tunic dropped

### Story 7: Seasonal pressure
- Set season to winter, spawn movers without clothing
- Verify: movers get cold speed penalties
- Produce and equip clothing
- Verify: speed penalty reduced/eliminated

---

## Connections

- **Uses 04a** (butchering): ITEM_HIDE already in game, waiting for tanning
- **Uses 06b** (farming): ITEM_FLAX_FIBER finally has a purpose beyond stockpiling
- **Uses existing workshops**: Follows established construction/recipe/bill patterns
- **Uses existing tool-equip pattern**: `equippedClothing` mirrors `equippedTool`
- **Uses existing body temp system**: Just modifies cooling rate, no new simulation
- **Opens 10** (personality): Clothing quality + mood penalties for tattered/naked
- **Opens future**: More slots (head/feet), layering, degradation, armor, dyeing
- **Deferred**: `06c-farm-watering.md` is independent, can be done before or after 07

---

## Future Expansion Path (not for 07)

From the cross-game research (`docs/vision/clothing-textiles-research.md`):

1. **Degradation + repair** (DF/RimWorld) → ongoing demand, tattered mood penalty
2. **More body slots** (head, feet) → hats, boots → more item variety
3. **Layering** (CDDA/RimWorld) → under + outer → meaningful equipment choices
4. **Quality from crafter skill** (RimWorld) → legendary clothing = much better stats
5. **Wool from animals** → when animal husbandry arrives (shearing sheep/goats)
6. **Dyeing** (DF) → luxury/mood, uses quern for pigment grinding
7. **Encumbrance** (CDDA) → heavy armor = slower movement, tradeoff
8. **Tainted clothing** (RimWorld) → prevents scavenging from dead enemies
