# Farming System Comparison: NavKit vs Stardew Valley

> Written 2026-02-25, after completing 06a+06b (land work + crops & processing).
> Different genres (colony sim vs farming RPG) but useful to compare farming depth.

---

## What NavKit Has (as of 06b)

### Core Loop
- Till soil (5 soil types, tool quality affects speed)
- Plant seeds (wheat, lentils, flax) with per-cell crop designation
- Growth through 5 stages with full environmental simulation
- Harvest with yields + automatic seed return (self-sustaining)
- Wild plants in worldgen as seed bootstrap

### Environmental Depth (stronger than Stardew)
- 5 growth modifiers stacking multiplicatively: season, temperature, wetness, fertility, weeds
- Frost damage halves yield but preserves seeds
- Seasonal crop death (lentils/flax die in autumn, wheat dies in winter)
- Soil types matter: peat is excellent, gravel is poor, clay risks waterlogging
- Weed pressure as ongoing maintenance threat
- Lentil nitrogen fixation rewards crop rotation (+60 fertility vs wheat -20)
- Rain from weather system waters crops automatically

### Processing Chains
- Wheat: harvest → quern → flour → bake → bread (4 steps, +0.6 nutrition)
- Lentils: harvest → cook → cooked lentils (2 steps, +0.4 nutrition)
- Flax: harvest → fiber → stockpile (ready for loom in 07)

### Colony Sim Advantages
- Multiple autonomous workers handle farming jobs
- Job priority system (harvest ripe > plant > tend > fertilize)
- Seed stack splitting — movers carry 1 seed at a time
- Fog of war: discovering wild plants rewards exploration
- Tool quality speeds up tilling and harvesting
- Compost pile workshop recycles rotten food into fertilizer

---

## What Stardew Has That We Don't

### More Content (breadth)
- 20+ crop types vs our 3
- Fruit trees (perennial, seasonal fruit)
- Flowers (honey production)
- Giant crops / rare mutations
- Quality levels on produce (normal/silver/gold/iridium)

### Automation
- Sprinklers (basic → quality → iridium) replace manual watering
- Junimo huts (auto-harvest)

### Processing Variety
- Preserves jar (fruits/vegetables → jelly/pickles)
- Kegs (fruit → wine, wheat → beer, hops → pale ale)
- Oil maker (truffle → oil, sunflower → oil)
- Aging casks (wine/cheese quality upgrades over time)
- Loom (wool → cloth) — we have this planned for 07

### Animal Husbandry
- Chickens, cows, goats, pigs, sheep, rabbits, ducks
- Barn/coop buildings, feeding, happiness
- Animal products (eggs, milk, wool, truffles)

### Infrastructure
- Greenhouse (year-round growing, unlockable)
- Scarecrows (protect crops from crows)
- Bee houses (flowers → honey)

### Economy
- Selling crops for gold
- Crop value varies by quality and processing
- Pierre's shop for buying seeds

---

## Where NavKit Is Deeper

| System | NavKit | Stardew |
|--------|--------|---------|
| Soil simulation | 5 types, fertility depletion/restoration, crop rotation matters | Flat — all soil identical |
| Weather impact | Temperature, wetness, frost damage, seasonal kill, rain watering | Rain skips watering, nothing else |
| Growth modifiers | 5 simultaneous (season × temp × water × fertility × weeds) | Season only (spring/summer/fall crops) |
| Worker management | Multiple movers, job priorities, tool quality | Single player character |
| World dimension | 3D z-levels, underground potential | 2D flat |
| Exploration | Fog of war, wild plant discovery | Map fully visible |

---

## Gaps to Consider for Future

Roughly ordered by what would add the most gameplay:

1. **More crop types** — low effort per crop once the system exists. Potatoes, carrots, berries-as-crop, herbs.
2. **Animal husbandry** — chickens/goats as tamed animals, eggs/milk. Big feature but huge gameplay value.
3. **Preserving/fermenting** — extends spoilage system. Drying rack already exists. Add pickling, salting, fermting.
4. **Irrigation** — water channels to farms. Extends 06c water carrying into proper infrastructure.
5. **Crop quality** — soil fertility + no frost + ideal conditions → better yield. Simpler than Stardew's quality tiers.
6. **Fruit trees** — perennial crops, seasonal harvest, years to mature. Different gameplay rhythm.
7. **Greenhouse** — glass (from 09-loop-closers) + construction = year-round growing.

---

## Design Philosophy Difference

Stardew Valley optimizes for **content breadth and player satisfaction** — many crops, many recipes, quality progression, festivals, relationships. The farming is a vehicle for a cozy RPG experience.

NavKit optimizes for **systemic depth and emergent gameplay** — fewer crops but each one responds to a web of environmental systems. A failed harvest happens because of real temperature/weather/soil interactions, not scripted events. The farming creates genuine survival pressure in a colony management context.

Both approaches are valid. NavKit should lean into its strength (deep systems, emergent stories) rather than chasing Stardew's content volume. Add crops and recipes when they create new gameplay dynamics, not just for variety.
