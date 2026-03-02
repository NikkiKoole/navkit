# Steam Page Strategy

> Put the page up now, iterate later. Wishlists compound.

---

## Why Now (Before Art Direction Is Final)

- **A Steam page doesn't lock you into a visual style.** Screenshots, trailer, description, capsule art can all be updated unlimited times before launch.
- **Wishlists compound over time** — earlier is always better. Every week without a page is lost wishlists.
- **You get a URL** to put in every Reddit post, devlog, Discord message, YouTube video.
- **Steam's algorithm starts learning** about your game's audience from day one.
- **It's free.**

### Precedent

- Dwarf Fortress's Steam page existed for years before the pixel tileset was finished.
- RimWorld looked completely different in its early screenshots vs release.
- Plenty of games change art direction drastically during development. Nobody holds early screenshots against you.

## What You Need (Minimum)

| Requirement | Notes |
|-------------|-------|
| App name | Working title is fine. Can change later (costs nothing). |
| Short description | 1-2 sentences for the store listing. |
| Long description | A few paragraphs about what the game is. |
| 5 screenshots | Dev screenshots are fine. Debug lines and all — colony sim fans find that interesting, not off-putting. |
| Capsule image | The store banner (460x215, 231x87). Can be simple text+background to start. |
| $100 Steam Direct fee | One-time, refunded after $1k in sales. |

## What Doesn't Matter Yet

- Final art direction (top-down vs 2.5D side-view vs isometric)
- Polished art or final sprites
- A trailer (helps, but not required for "Coming Soon")
- A release date

## Visual Direction: Not Decided Yet (And That's Fine)

Currently exploring two directions:

### Current: Top-down DF-style
- Working, playable, 50k lines of simulation behind it
- 8x8 tile atlas, functional but not visually distinctive

### Exploring: 2.5D side-view cross-section
- "Ant farm" perspective — see all z-levels stacked vertically
- Movers walk left/right, stairs connect floors visually
- References: Oxygen Not Included, Terraria, SimTower, Fallout Shelter, Kingdom: Two Crowns, Sheltered, Lemmings
- Already have a working side-view rendering (see prototype screenshots)
- The decoupled-simulation architecture (`docs/todo/architecture/decoupled-simulation-plan.md`) enables switching views without changing simulation code

### For the Steam page: use whatever looks best right now
- The side-view cross-section with movers on stairs is visually striking even in dev state
- Colony sim fans are drawn to "look at this simulation running" more than polished art
- Update screenshots as art direction solidifies

## Game Name

### The naming problem

The game spans two extremes: primitive survival (naked in the grass, knapping stones) and modern life (20th century apartment buildings, elevators, tenants). The name needs to work for both ends, or at least not lock into one.

Development is now approaching from the modern/apartment end first, so names that sound purely primitive-survival are misleading.

### Candidates

**"Workers Need Burgers"** (current favorite)
- Based on a personal drawing with the same title — has authenticity
- Specific, funny, visual — you can picture a mover pathfinding toward a burger
- Natural meme energy ("my workers need burgers but the cook fell in the river")
- Works for both modes — primitive settlers need food, apartment tenants need food
- Googleable — no other game has this name
- Connects to actual game loop — hunger is the first need, food chains are central

**"People Need Things"**
- Describes the literal game loop (people need things, you figure out how to provide)
- Genre-agnostic — covers primitive survival AND apartment management
- Dry humor tone that fits the game's personality
- Memorable, stands out on a store page
- Risk: could sound like a mobile clicker game out of context, but screenshots fix that
- Same naming family as "People Playground", "Human Resource Machine", "Everything"

**Other candidates considered:**
- "Naked in the Grass" — evocative but misleading now that development starts from the modern end
- "Ground Up" — short, works for both directions, double meaning with construction
- "Floors" — ultra minimal, literally what z-levels are

### Character name: Coggies

The little guys are called **coggies**. Double meaning:
- **Cogs** — small parts of a bigger machine. Individually simple, together complex. Each one pathfinds, hauls, eats, sleeps — and the colony emerges from all of them running at once.
- **Cognitive** — they're AI beings with needs and decision-making. Not just units, but little minds navigating the world.

Pre-dates LLMs by years — refers to the in-game AI, not chatbots. The name is affectionate ("look at my coggies") like "dupes" (ONI) or "pawns" (RimWorld). Every colony sim needs a word for its little guys.

### What both modes share (the core identity)

The question that led to the name: what makes "naked in the grass knapping stones" and "managing an apartment building" feel like the same game?

- **Needs-driven agents** — movers that want things and go get them
- **Spatial problem-solving** — where you put things matters
- **Material chains** — raw input becomes useful output
- **Vertical space** — z-levels are the differentiator. Digging down for caves OR stacking floors for apartments — same system, different context. A burrow and a tower are the same thing rotated.
- **Simulation you watch unfold** — you set up conditions, then observe

The z-levels / vertical space is probably the deepest shared identity. Most colony sims are flat. This one is vertical in both directions.

---

## Store Description (Draft)

> **Short description:**
> A colony simulation where wood has species, fire has physics, and your settlers can starve two tiles from the berry bush.

> **Long description (rough):**
> Build and manage a colony from nothing. Your settlers start naked in the grass — knap stone tools, build shelter before winter, brew clay pots for water, and try not to starve.
>
> Every system simulates: water flows and floods, fire spreads through dry wood and creates smoke that rises through z-levels, rain creates mud, snow slows movement, seasons change the land.
>
> Features:
> - Full 3D grid with z-levels — dig down, build up, explore caves
> - Water, fire, and smoke cellular automata that interact with each other
> - Material-aware crafting — oak burns differently than pine, stone tools have quality levels
> - Weather and seasons — rain, snow, mud, temperature, wind affecting gameplay
> - Workshops and crafting chains — from raw stone to furnished rooms
> - Pathfinding that actually works — 4 algorithms (A*, HPA*, JPS, JPS+) handling complex multi-level terrain

## Pricing & Release Model

Too early to decide. Capture current thinking and revisit closer to release.

**Price gut feeling:** ~15 EUR. Sweet spot for indie colony sims — cheap enough for impulse buy, expensive enough to signal "real game." Reference points: RimWorld $30, ONI $25, most indie colony sims in Early Access $15-20.

**Early Access:** Almost certainly yes. Every colony sim does it (RimWorld, ONI, Going Medieval, Timberborn, Songs of Syx all launched EA). The genre audience expects it and actively enjoys watching games develop. Decide scope of EA launch later.

**Demo:** Yes, needed for Steam Next Fest. Scope it tight — one scenario, 15-30 min of gameplay.

**Not needed for the Coming Soon page.** Steam doesn't ask for a price until you're ready to release.

---

## Action Items

1. **Register on Steamworks** and pay the $100 fee
2. **Pick a name** (or use "NavKit" as working title — can change)
3. **Take 5 screenshots** showing the most visually interesting systems (water flood, fire spread, colony cross-section, pathfinding visualization, weather)
4. **Write store description** (draft above, refine later)
5. **Make a simple capsule image** (game name + one striking screenshot)
6. **Submit for review** (takes ~3-5 business days)
7. **Start linking the page** in every post from then on
