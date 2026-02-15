# Feature 07: Clothing & Textiles

> **Priority**: Tier 3 — Material & Comfort Loop
> **Why now**: Temperature system exists. Wind chill exists. Snow exists. But colonists walk naked through blizzards. Cordage/fiber processing exists but leads nowhere beyond rope.
> **Systems used**: Rope maker, drying rack, cordage, temperature, weather, wind chill
> **Depends on**: F3 (shelter — clothing is the outdoor complement), F5 (hunting — hides for leather)
> **Opens**: Trade value (future), mood bonuses (F10), armor (future threats)

---

## What Changes

A **loom** workshop weaves dried grass and cordage into **cloth**. Cloth is sewn into **clothing** at a **tailor's bench**. Wearing clothing provides **cold resistance** — reducing the temperature at which movers get debuffs. **Leather** from animal hides (F5) makes better clothing.

Shelter (F3) protects indoors. Clothing protects outdoors. Together they close the temperature loop.

---

## Design

### Temperature Effect on Movers

Before clothing can matter, temperature must affect movers:

```
Comfortable range: 10°C – 30°C (no effect)
Cold (<10°C): work speed penalty, increasing with cold
  - 5°C:  0.9× speed
  - 0°C:  0.75× speed
  - -5°C: 0.5× speed + seek shelter/warmth
Freezing (<-10°C): health damage over time (future)
```

**Wind chill** (already calculated by weather system) effectively lowers temperature for exposed movers.

**Clothing modifier**: Each clothing piece adds cold resistance (shifts comfortable range downward).

### Clothing System

Movers have **clothing slots** (simplified):
- **Body**: shirt/tunic/coat (1 slot)
- **Feet**: sandals/boots (1 slot, future)

Each clothing item has:
```c
float coldResistance;  // degrees of cold protection
float durability;      // future: wears out over time
MaterialType material; // grass cloth, leather, etc.
```

| Clothing | Material | Cold Resistance | Recipe |
|----------|----------|-----------------|--------|
| Grass tunic | Cloth (grass) | +5°C | 3× ITEM_CLOTH |
| Leather vest | Leather | +10°C | 2× ITEM_LEATHER |
| Leather coat | Leather + cloth | +15°C | 2× ITEM_LEATHER + 1× ITEM_CLOTH |

### Production Chain

```
Existing chain:
  Grass → Drying rack → Dried grass → Rope maker → String → Cordage

New extension:
  Dried grass → Loom → ITEM_CLOTH (weaving)
  ITEM_HIDE (from F5) → Tanning rack → ITEM_LEATHER
  ITEM_CLOTH → Tailor's bench → Grass tunic
  ITEM_LEATHER → Tailor's bench → Leather vest
```

### New Workshops

**Loom** (2×2):
```
X O
# #
```

| Recipe | Input | Output | Time |
|--------|-------|--------|------|
| Weave cloth | DRIED_GRASS × 4 | ITEM_CLOTH × 1 | 6s |
| Weave rope cloth | CORDAGE × 2 | ITEM_CLOTH × 1 | 4s |

**Tanning Rack** (2×2, passive like drying rack):
```
X O
. .
```

| Recipe | Input | Output | Time |
|--------|-------|--------|------|
| Tan hide | ITEM_HIDE × 1 | ITEM_LEATHER × 1 | Passive, 60s |

The existing **drying rack** pattern works perfectly here — deliver hide, wait, collect leather.

**Tailor's Bench** (2×2):
```
X O
# .
```

| Recipe | Input | Output | Time |
|--------|-------|--------|------|
| Grass tunic | CLOTH × 3 | ITEM_GRASS_TUNIC × 1 | 8s |
| Leather vest | LEATHER × 2 | ITEM_LEATHER_VEST × 1 | 10s |
| Leather coat | LEATHER × 2 + CLOTH × 1 | ITEM_LEATHER_COAT × 1 | 12s |

### Clothing Equipping

- Movers auto-equip clothing from stockpiles when cold (similar to tool seeking from F4)
- Clothing goes in clothing slot, separate from tool slot
- When temperature drops below comfortable range, mover seeks clothing
- Clothing is persistent — doesn't need re-equipping

### Seasonal Clothing Loop

```
Summer: Nobody needs clothing (comfortable temperature)
Autumn: Smart player starts making tunics (preparation)
Winter: Clothing essential — unclothed movers work at 0.5× speed outdoors
Spring: Still useful until temperatures rise
```

---

## Implementation Phases

### Phase 1: Temperature Effects on Movers
1. Calculate effective temperature for mover (cell temp + wind chill)
2. Apply speed penalty below comfortable range
3. Cold movers seek shelter (sheltered rooms from F3) when idle
4. **Test**: Cold movers work slower, seek shelter

### Phase 2: Clothing Items & Slots
1. Add clothing slot to Mover struct
2. Add ITEM_CLOTH, ITEM_LEATHER, ITEM_GRASS_TUNIC, ITEM_LEATHER_VEST, ITEM_LEATHER_COAT
3. Cold resistance per clothing item
4. Equipped clothing shifts comfortable range down
5. **Test**: Clothed mover tolerates colder temperatures

### Phase 3: Loom & Tanning
1. Add WORKSHOP_LOOM (weaving recipes)
2. Add WORKSHOP_TANNING_RACK (passive hide → leather)
3. **Test**: Weaving produces cloth, tanning produces leather

### Phase 4: Tailor's Bench
1. Add WORKSHOP_TAILOR
2. Clothing recipes using cloth and leather
3. **Test**: Recipes produce correct clothing items

### Phase 5: Auto-Equip
1. Movers check clothing when temperature drops
2. Seek and equip clothing from stockpile
3. Prioritize better clothing (leather coat > vest > grass tunic)
4. **Test**: Mover auto-equips clothing when cold

---

## Connections

- **Uses rope maker/drying rack**: Dried grass → cloth is the textile bootstrap
- **Uses hunting (F5)**: Hides → tanning → leather
- **Uses temperature**: Already simulated, now affects movers
- **Uses weather/wind**: Wind chill makes clothing more valuable
- **Uses shelter (F3)**: Indoor = shelter protection, outdoor = clothing protection
- **Opens for F10 (Moods)**: Clothing quality affects comfort/mood
- **Opens for future**: Armor (leather → hide armor), trade goods

---

## Design Notes

### Why Not Full Body Slots?

Head, body, legs, feet, hands — that's 5 slots and a lot of items. Start with just body slot. Add feet (boots) later. The important thing is that temperature affects gameplay and clothing helps. Detail can come incrementally.

### Tanning Without Chemicals

Historically, tanning requires chemicals (urine, tannin from bark, etc.). For now, the tanning rack just takes time (passive workshop). When the waste/chemistry system comes (from the AI discussions), tanning can require ITEM_BARK or urine as input. The workshop structure supports adding inputs later.

### Grass Cloth Is Rough

Grass cloth should be the worst clothing — it's scratchy, provides minimal warmth. It exists so players have *something* before hunting provides hides. The progression is: naked → grass tunic (bad) → leather vest (good) → leather coat (great) → woven wool (future, excellent).

---

## Save Version Impact

- New Mover field: clothingSlot (item index)
- New items: ITEM_CLOTH, ITEM_LEATHER, ITEM_GRASS_TUNIC, ITEM_LEATHER_VEST, ITEM_LEATHER_COAT
- New workshops: WORKSHOP_LOOM, WORKSHOP_TANNING_RACK, WORKSHOP_TAILOR
- Cold resistance data on clothing items

## Test Expectations

- ~30-40 new assertions
- Temperature speed penalty calculation
- Cold resistance from clothing
- Weaving, tanning, tailoring recipes
- Auto-equip behavior
- Seasonal clothing pressure
