# Clothing & Textiles: Cross-Game Research

> Written 2026-02-25. Research for feature 07 (clothing & textiles).
> Games analyzed: Dwarf Fortress, Cataclysm: DDA, RimWorld, The Sims 4.

---

## Quick Comparison Matrix

| Feature | DF | CDDA | RimWorld | Sims | NavKit (planned) |
|---------|-----|------|----------|------|-----------------|
| Body slots | ~8 + layers | 12 + sub-parts | 4-5 + layers | Category-based | 1 (body) → expand |
| Layers | 4 (under/over/armor/cover) | 7 layers | 3 (skin/middle/outer) | None (binary) | None (start simple) |
| Materials | Plant/animal/silk/leather | 10+ (cotton→kevlar) | Cloth/wool/leather/exotic | Cosmetic only | Grass cloth/leather/flax |
| Quality tiers | 7 (standard→artifact) | Normal/excellent/legendary | 7 (awful→legendary) | Normal/excellent/legendary | Existing tool quality |
| Degradation | 4 wear stages, time-based | 4 HP, repair system | HP-based, daily wear | None | Planned (future) |
| Temperature | None (gap in DF!) | Warmth per item | Cold/heat insulation values | Binary (right outfit or not) | Cold resistance value |
| Crafting depth | 4-6 steps per garment | Skill-gated recipes | Tailor bench + materials | Knitting skill (DLC) | Loom + tanning + tailor |

---

## Dwarf Fortress

### Key Takeaways

**Production depth is DF's strength.** The full plant fiber chain is: grow crop → thresh into thread → (dye thread) → weave into cloth → (dye cloth) → sew into garment. That's 4-6 steps across 5+ workshops with 5+ labor skills. Four parallel material paths (plant fiber, animal wool, silk, leather) all produce clothing through different workshop chains.

**Wear creates perpetual demand.** Clothing degrades through 4 stages (pristine → slightly worn → tattered → destroyed). Tattered clothing causes mood penalties. Organic materials degrade passively over ~100 in-game years even in storage. This means the textile industry is *never* "done" — it's an ongoing colony need.

**Quality cascades across production steps.** Quality is determined at weaving, dyeing, AND tailoring — each step's multiplier stacks. A masterwork-woven, masterwork-dyed, masterwork-sewn dress is worth far more than the same dress at normal quality. This rewards investing in skilled workers throughout the chain.

**Clothing drives mood significantly.** Each uncovered body category (head, torso, legs, feet, hands) generates a *separate* negative thought. Walking around with no shoes AND no hat is worse than just missing one. Tattered clothing also penalizes mood.

**Notable gap: no temperature protection.** Despite DF's simulation depth, clothing provides NO temperature protection. This is a known gap. NavKit should not follow this — our temperature system is already more gameplay-relevant.

### Relevant for NavKit
- Wear/degradation creating ongoing demand (but simpler — maybe 2-3 stages, not 4)
- Mood penalties for nudity and tattered clothing (when mood system arrives in 10)
- Quality at craft step (already have tool quality — extend to crafting quality)
- Dyeing as a future luxury layer (not for 07, but cool for later)

---

## Cataclysm: Dark Days Ahead

### Key Takeaways

**Layering + encumbrance is CDDA's core tension.** 7 layers from skintight to belted, each body part independently tracked. Stacking items on the same layer adds encumbrance penalties. This creates real tradeoffs: more protection = more encumbrance = slower/clumsier. The system rewards carefully curating a loadout rather than piling on everything.

**Coverage math is exponential.** Going from 80% to 95% coverage quadruples effective protection. Small coverage improvements at the high end are extremely valuable. This creates interesting min-maxing.

**Material thickness drives protection.** Protection = thickness × material resistance. Multi-material items split thickness proportionally. This is data-driven and extensible — add a new material, define its resistance values, done.

**Repair and modification system.** Items have 4 HP, can be reinforced to 5. Padding with fur/leather/kevlar adds warmth or protection at the cost of encumbrance. Fitting reduces encumbrance by 50%. These modifications create meaningful player choices without adding new items.

**Crafting is skill-gated.** Tailoring levels 0-6 progressively unlock better recipes. Level 0 = bandages, level 6 = survivor armor. Tools required escalate too (bone needle → sewing kit → tailor's kit).

### Relevant for NavKit
- Encumbrance as a tradeoff for heavy clothing (not for 07, but good design principle)
- Repair system (when degradation is added — simple repair at tailor bench)
- Skill-gated crafting (already have tool quality; could gate clothing recipes by crafter skill later)
- Material properties as data (fits our item_defs pattern)

---

## RimWorld

### Key Takeaways

**Material hierarchy creates progression.** Cloth (easy, weak) → Devilstrand (mid-game, strong, slow to grow) → Synthread/Hyperweave (trade-only, best). Leather provides a parallel path gated by available animals. Each material has different cold insulation, heat insulation, armor, beauty, and flammability values. This means material choice is always a tradeoff.

**Quality is the deepest multiplier.** Legendary = 1.8× protection, 1.8× insulation, 0.1× deterioration rate, 5× market value vs normal. A legendary devilstrand duster is comparable to armor. This makes skilled crafters extremely valuable colonists and creates a long-term progression arc.

**Temperature drives material choice.** Arctic colonies need megasloth wool (best cold insulation). Desert colonies need devilstrand (best heat insulation). This is a natural fit for NavKit where we already have seasonal temperature variation.

**Deterioration creates ongoing demand.** 40% chance per day for 1 HP damage on worn clothing. Combat and fire accelerate degradation. Quality dramatically affects lifespan (legendary lasts 10× longer than normal). Tattered clothing (below 50% HP) gives mood debuffs.

**Outfit policies automate management.** Players define rules ("wear clothing above 51% HP, prefer devilstrand, no tainted apparel") and colonists follow them autonomously. This prevents tedious micromanagement while keeping player agency.

**Tainted clothing prevents scavenging.** Dead people's clothing gets "tainted" and causes mood debuffs if worn. This forces players to build production infrastructure rather than just looting enemies.

**27+ leather types from different animals.** Each animal's leather has different stats. Bears = warm + tough. Guinea pig = warmest but fragile. This makes animal choice for husbandry or hunting strategically interesting.

### Relevant for NavKit
- Numeric cold/heat insulation per item (not binary) — fits our temperature system
- Material affecting insulation values (grass cloth = weak, leather = good, flax linen = medium)
- Quality affecting stats (extend existing tool quality system)
- Deterioration with mood penalties (when mood system arrives)
- Outfit policies for autonomous equipping (like auto-eat, auto-equip tools)
- Leather from different animals having different stats (when more animals arrive)

---

## The Sims

### Key Takeaways

**Context-driven outfit switching.** Sims have outfit categories (everyday, athletic, sleepwear, cold weather, hot weather) and auto-switch based on activity/temperature. The design lesson: the *context* drives what's worn, not player micromanagement.

**Binary temperature protection.** With Seasons expansion, you either have the right outfit category or you don't. Right = protected, wrong = die in extreme weather. Simple but effective for a game that isn't about survival depth.

**Auto-switching is notoriously buggy.** NPC townies frequently die in The Sims because the auto-switch fails. Community mods exist to fix it. Lesson: make outfit switching deterministic and rule-based, not probabilistic.

**Mood from clothing preferences.** Small mood bonuses for wearing liked fashion styles, penalties for disliked ones. Low-cost personality flavor.

**Knitting as a crafting skill.** Sims 4 DLC added a 10-level knitting skill with quality tiers (normal → legendary) affecting item value and comfort. Maps well to workshop-based crafting in a colony sim.

### Relevant for NavKit
- Auto-equip should be deterministic (if cold AND owns warm clothing → equip it, always)
- Context categories as a future concept (work clothes vs cold weather gear)
- Keep it simpler than The Sims — no per-mover fashion preferences (at least not in 07)

---

## Synthesis: What This Means for NavKit 07

### Our Strengths (already built)
- Temperature system with seasonal variation and wind chill
- Workshop + recipe system (loom/tanning/tailor fit naturally)
- Tool quality framework (extend to crafting quality)
- Material system (grass, leather, flax fiber from 06b)
- Hide from butchering (04a) ready for tanning
- Auto-equip pattern (movers already seek tools)

### Recommended Design for 07 (simple, expandable)

**Start minimal like our current doc (07-clothing-and-textiles.md) but informed by research:**

1. **Temperature effects on movers** — speed penalty below comfort range (already planned)
2. **1 body slot** — keep it simple, expand later. DF's 8 slots and CDDA's 12 is way too much for now
3. **3-4 clothing items** — grass tunic (worst), leather vest (good), leather coat (great), flax tunic (medium) — flax fiber from 06b finally has a use
4. **Numeric cold resistance** per item, modified by material (RimWorld approach, not Sims' binary)
5. **Auto-equip when cold** — deterministic, not probabilistic (lesson from Sims bugs)
6. **No degradation yet** — add in a later pass. DF and RimWorld show it's great for ongoing demand, but it adds complexity. Get the core loop working first.
7. **No layering yet** — one slot, one item. Layering is powerful (CDDA/RimWorld) but premature for our first clothing pass.
8. **Flax linen as upgrade** — dried grass → loom → cloth (basic), flax fiber → loom → linen (better). Creates a farming-to-clothing pipeline that rewards 06b investment.

### Future Expansion Path (not for 07)
- **Degradation + repair** → creates ongoing demand (DF/RimWorld pattern)
- **More body slots** (head, feet) → hats, boots for cold
- **Quality from crafter skill** → extends tool quality to crafted goods
- **Layering** (under + outer) → meaningful equipment choices
- **Wool from animals** → when animal husbandry arrives
- **Dyeing** → luxury/mood system, uses existing quern for pigment grinding
- **Encumbrance** → heavy clothing = slower movement (CDDA tradeoff)
- **Tainted clothing** → prevents scavenging from dead enemies (RimWorld balance)

---

## Production Chain Overview (07 scope)

```
GRASS PATH (immediate, worst):
  Grass → Drying rack → Dried grass → Loom → ITEM_CLOTH
  ITEM_CLOTH × 3 → Tailor bench → Grass tunic (+5°C)

FLAX PATH (requires 06b farming, medium):
  Flax fiber (from 06b harvest) → Loom → ITEM_LINEN
  ITEM_LINEN × 2 → Tailor bench → Flax tunic (+8°C)

LEATHER PATH (requires hunting 04b, best):
  ITEM_HIDE (from butchering) → Tanning rack → ITEM_LEATHER
  ITEM_LEATHER × 2 → Tailor bench → Leather vest (+10°C)
  ITEM_LEATHER × 2 + ITEM_CLOTH × 1 → Tailor bench → Leather coat (+15°C)
```

Three parallel paths converging on the tailor bench. Grass is the bootstrap, flax rewards farming, leather rewards hunting. Matches our existing multi-path pattern (berries vs roots vs meat for food).
