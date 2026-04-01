# NavKit Project Arc

> Where we came from, where we are, where we're going.
> Last updated: 2026-03-31.

---

## The Story So Far

NavKit started as a **3D grid-based pathfinding sandbox** — A*, HPA*, JPS, JPS+ on a voxel grid with movers that navigate it. From that foundation, it grew into a **colony simulation** inspired by Dwarf Fortress, Rimworld, and The Sims.

### Foundation Layer (early work)
The engine: raylib, C11, unity build. 3D grid with DF-style walkability (stand on top of solid ground). Four pathfinding algorithms, string pulling, mover avoidance. Save/load, inspector, headless mode.

### World Layer
Terrain types and materials (dirt, clay, sand, gravel, peat, stone). Z-levels, ramps, ladders, channels. Cellular automata for water, fire, smoke, steam, temperature. Weather and seasons (mud, snow, wind, cloud shadows). Lighting (sky light + colored block light). Fog of war and exploration.

### Economy Layer
Items with materials and stacking. Stockpiles with filtered storage and priority. Workshops with ASCII templates, recipes, bills, fuel. Multi-output recipes. Hauling system (stockpile-centric + item-centric). Construction system with material costs and blueprints.

### Survival Arc (sessions 1–16, the big push)
This was the focused roadmap that turned the sandbox into a game:

1. **Workshop costs & tool quality** — workshops need materials to build, tools affect work speed
2. **Shelter** — stick walls, leaf roofs, doors, ground fire, fire pit
3. **Food chain** — butchering, cooking, hunting, root foraging, spoilage, animal respawn
4. **Exploration** — fog of war, scouting designation, discovery events
5. **Farming** — tilling, composting, 3 crops (wheat/lentils/flax), growth modifiers, quern, cooking
6. **Clothing** — loom, tailor, 7 clothing items, temperature integration
7. **Thirst** — water need, pots, beverages (tea, juice), natural water fallback
8. **Loop closers** — glass (sand→kiln→window), lye, mortar, mud/cob, reeds

Save version went from ~63 to 90 through this arc. ~129K lines of C, 46+ test suites.

---

## Where We Are Now (March 2026)

### Just Shipped
- **Mood & moodlets** (save v90) — mood.h/c, 5 personality traits, squared-curve system, 31 tests
- **Dollhouse domestic life** research — action queues, freetime-first strategy, advertisement system design
- **Biome presets** — 8 named presets for worldgen/climate/vegetation
- **Trains** — horizontal multi-stop transport (phases 1-4 done, phase 5 partial)

### Active Fronts
- **Mood system wiring** — foundation shipped, needs to be connected to actual game events (weather, light, dirt, food, activity, room quality)
- **Room detection** — rooms identified and typed, quality scoring in place
- **Dollhouse vision** — shifting from "colony manager" toward "watch little people live their lives"

### Known Bugs
- Wall-build mover pop (mover teleports after building wall)
- Stockpile priority performance (correct search is 6-12x slower, needs pre-sorted list)
- High-speed stuck movers (speed 100 causes pathing failures)

---

## Where We're Going

### Near Term (next sessions)
From `todo/INDEX.md` — specs ready to build:

- **Onboarding & progression** — starting scenarios, progressive need unlocks, difficulty tuning
- **Schedule system** — clock-driven daily routines (work/meal/leisure/sleep blocks)
- **Village next steps** — room quality → mood, distance weighting, occupations, furniture
- **Fire improvements** — ground fire spread, fire pit tool gates
- **Jobs refactor** — modularize jobs.c (6k lines, ~85 functions)

### Medium Term (the "Sims feel")
The dollhouse vision doc signals a direction shift — less "optimize your colony" and more "watch interesting people":

- Mood drives behavior (already started)
- Furniture matters (comfort, quality, room scoring)
- Personality traits create emergent stories
- Skills develop through use
- Daily schedules create rhythm

### Long Term Vision
From `docs/vision/`:

- **Infinite world** — chunk streaming, delta persistence, overmap, cross-chunk pathfinding
- **Colony→Exodus→Colony cycle** — vehicle construction, journey events, trade routes
- **Steam power** — boilers, turbines, pistons (far future)
- **Sound & mind** — agents communicate with token palettes, birdsong evolves for mate choice
- **Entropy** — everything decays, maintenance is gameplay

### Design Principles
- **Autarky**: every item needs a source AND a sink before it ships
- **Stone age first**: earn your way up from sticks and rocks
- **Emergent over scripted**: personality traits + environmental systems → stories

---

## Map of the Docs

| Folder | What's in it | Count |
|--------|-------------|-------|
| `docs/done/` | Completed features with full implementation notes | ~170 files |
| `docs/todo/` | Planned work, specs, ideas — see `todo/INDEX.md` | ~50 files |
| `docs/todo/00-priority-order.md` | The survival roadmap (tier 0-3) | 1 file |
| `docs/vision/` | Far-future ideas and research | ~40 files |
| `docs/bugs/` | Known bugs with analysis | 3 files |
| `docs/doing/` | Currently in-flight work | ~5 files |
| `docs/research/` | Deep dives (rewind feature, DF tiles) | 2 files |
| `docs/marketing/` | Steam page, video, community strategy | 4 files |
| `soundsystem/docs/` | PixelSynth/DAW (separate subsystem) | ~50 files |
| `.claude/skills/` | Claude Code custom skills | 13 files |

### Key Entry Points
- **What's next?** → `docs/todo/INDEX.md`
- **Survival roadmap** → `docs/todo/00-priority-order.md`
- **Dollhouse direction** → `docs/todo/dollhouse-domestic-life.md`
- **Architecture decisions** → `docs/todo/INDEX.md` "Known Conflicts & Decisions"
- **What's been built** → `docs/done/` (organized by subsystem)
- **The big dream** → `docs/vision/vision.md`
