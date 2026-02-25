# Vertical Stack: 1 Mover Surviving

> **Goal**: Make the single-mover play mode into a real survival loop. One character, waking up naked, must eat, sleep, build shelter, and not die.

---

## Current State (what works today)

- **Play mode** (F1 toggle): minimal HUD, New Game generates 128x128 map, 1 mover, camera follows
- **Terrain gen**: hills, soils, water, trees, berry bushes, loose rocks & sticks scattered
- **Hunger**: drains over time (16 game-hours to starve), speed penalty below 0.2 (down to 50%), berries restore 0.3
- **Auto-eat**: movers autonomously seek food when hungry (<0.3), drop jobs when critical (<0.1); searches stockpiles first, ground items when starving; cooldown prevents search spam
- **Energy**: drains over time (faster when working), beds/furniture restore; exhausted movers drop jobs and seek rest
- **Foraging**: Harvest Berries designation, berry bushes via plant system (BARE→BUDDING→RIPE growth cycle)
- **Food items**: ITEM_BERRIES (nutrition 0.3), ITEM_DRIED_BERRIES (nutrition 0.25), both IF_EDIBLE | IF_STACKABLE
- **Food processing**: Drying rack converts 3 berries → 2 dried berries
- **Gathering**: Harvest Tree, Gather Grass designations in play UI
- **Crafting**: 8 workshop types, passive workshops (drying rack, charcoal pit)
- **Building**: walls, floors, ladders, ramps with material delivery
- **Weather**: rain, snow, wind, seasons, mud, temperature
- **Furniture**: beds (plank bed, leaf pile), chairs

## What's Missing for "Survive One Day"

These are the gaps that break the illusion when you press New Game and watch your mover:

### ~~1. The mover doesn't seek food autonomously~~ ✅ DONE
- Auto-eat implemented in `needs.c` — freetime state machine handles SEEKING_FOOD → EATING
- Searches stockpiles first, ground items when critical (<0.1 hunger)
- Cooldown prevents search spam, UnassignJob preserves designation progress

### 2. No way to drink
- Water is everywhere (fluid sim) but mover can't interact with it
- Not critical for v1 if we just skip thirst — but feels weird near rivers

### 3. No shelter detection
- Mover can build walls/floors but enclosed spaces have no gameplay meaning
- Feature 03 (doors + shelter) gives this — rain protection, heat retention, rest bonus
- Without doors, buildings are just wall mazes

### 4. No tools — everything is the same speed
- Mining, chopping, building all take identical time regardless of equipment
- Feature 04 (tools + knapping) is the progression hook
- Early loop: rocks + sticks + cordage -> stone tools -> faster everything

### 5. Temperature doesn't affect the mover
- Cell temperature is simulated but movers don't feel it
- Winter comes, snow falls, mover doesn't care
- Need: body temp affected by ambient, clothing/shelter as protection

### 6. No threats
- Nothing can kill the mover. No danger = no tension
- Even just "starvation kills you" would add stakes

## Proposed Priority Order

Focus on making one day feel real, top to bottom:

1. ~~**Auto-eat behavior**~~ ✅ DONE — freetime state machine in `needs.c`
2. **Death from starvation** — if hunger hits 0 for too long, mover dies. Game over screen.
3. **Doors + shelter** (Feature 03) — enclosed rooms protect from rain, retain heat
4. **Temperature affects mover** — hypothermia/heatstroke, shelter as protection
5. **Tools + knapping** (Feature 04) — stone-age tool progression, speed scaling
6. **Cooking** — raw meat from hunting, cooked food from hearth (needs animals to be huntable)

Item 2 is small and gives immediate survival tension. Items 3-5 are the "second day" — reasons to build and progress. Item 6 needs hunting to work first.

## Not in Scope (yet)

- Multiple movers / recruitment
- Trading / caravans
- Skills / personality
- Clothing / textiles
- Farming / crops
- Complex UI (menus, inventories)

---

## Session Log

*(Track what we work on each session)*

### Session 1 — 2026-02-17
- Created play mode (F1 toggle), New Game terrain gen, 1 mover
- Fixed terrain gen (trees/bushes noise, vegetation grid init)
- Added loose rocks/sticks scatter
- Fixed starving mover repath loop
- Created this doc
