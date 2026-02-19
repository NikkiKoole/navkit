# Survival Design

> One mover, naked in the hills, trying not to die.

## Core Philosophy

- Slow progression. The mover should struggle with basic survival for a long time before unlocking real tools.
- Systems should interact: cold pushes you toward fire, fire needs fuel, fuel needs tools, tools need materials.
- Big trees are endgame. You don't chop an oak on day one.

## Current Loop (what works)

- Forage berries, eat, build grass pile bed, sleep, repeat
- Temperature is a real threat — cold nights drain energy faster and slow you down
- Fire heats nearby cells, making it a survival necessity for cold seasons
- Shelter blocks wind chill and rain, helping maintain body temperature
- No tools, no clothing, no fire dependency loop yet (fire exists but no fuel pressure)

## Temperature & Cold (implemented)

- **Diurnal cycle**: temperature swings ±5°C over the day (peak 14:00, trough 02:00), with ±25°C seasonal amplitude around 15°C base
- **Body temperature**: mover has `bodyTemp` (normal 37°C, range 20-42°C) that drifts toward ambient at 8°C/game-hour
- **Wind chill**: -2°C per wind strength level when exposed to sky
- **Shelter**: `IsExposedToSky()` check — any solid cell or constructed floor above blocks wind chill and rain
- **Fire warming**: fire heats cells (100°C + 20°C per fire level), heat spreads via 3D diffusion with insulation tiers (air 100%, wood 20%, stone 5%)
- **Hypothermia stages**:
  - Mild (< 35°C): movement slowdown begins (linear 1.0x → 0.6x)
  - Moderate (< 33°C): 2x energy drain, max movement penalty (0.6x)
  - Severe (< 30°C): death timer starts (4 game-hours), survival mode only
- **Heat penalty**: above 40°C, movement slows (1.0x → 0.7x at 42°C cap). No heat death.
- All thresholds tunable via balance parameters in `balance.h`

### Temperature — not yet done

- **Shivering**: visual effect + can't-work state at moderate cold (currently only slowdown)
- **Clothing/warmth**: no insulation items or warmth modifiers on movers
- **Temperature HUD**: no indicator for body temp or hypothermia warning
- **Heat death**: overheating only slows, never kills
- **Indoor heating bonus**: shelter blocks wind chill but doesn't provide warmth bonus beyond that

## Tools & Progression

### Stone Age (week 1-2)
- **Knapping**: bash rocks together → sharp stone (crude tool)
- **Sharp stone**: slow but functional — can cut cordage from grass, scrape bark, process small branches
- **Stone knife**: sharp stone + stick + cordage → first real tool, slightly faster
- These tools wear out. You're constantly replacing them.

### Young Trees (early resource)
- Small/young trees — thinner, shorter, maybe 1-2 block tall
- Can be cut with a sharp stone (slow) or stone knife (faster)
- Drop: sticks (2-3), maybe a thin log or poles
- Big mature trees remain uncuttable without a proper axe (much later)
- Young trees regrow over time from saplings

### Tool Progression (later)
- Stone axe: sharp stone + stick + cordage → can cut young trees faster, still can't touch big trees
- Proper axe (bronze/iron): much later, requires smelting → can finally fell big trees

## Food & Spoilage (future)

- Berries spoil after ~2 days on the ground, ~1 day if rained on
- Dried berries don't spoil (drying rack already exists)
- Creates urgency: forage → process → store, not just hoard
- Cooking (needs fire + huntable animals) is a later system

## Shelter

- Enclosed space = "indoors" (walls on all sides + roof/floor above)
- Indoor bonus: blocks wind chill, blocks rain/snow, no direct warmth bonus yet
- Even partial shelter (roof only) blocks precipitation and wind chill
- Doors needed to make real enclosed spaces (currently buildings are mazes)

## Day/Night Rhythm (goal feel)

- **Dawn**: wake up, eat, head out to forage/gather
- **Midday**: productive hours — gathering, crafting, building
- **Dusk**: head back to shelter, stoke the fire, eat
- **Night**: rest by the fire, sleep. Going out at night is dangerous (cold, slow, dark)

## Not Yet

- Multiple movers / recruitment
- Hunting / animals as food
- Farming / crops
- Clothing / textiles
- Metal tools / smelting
- Trading
