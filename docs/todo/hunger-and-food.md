# Hunger & Food System — Design Document

Everything we know, gathered from vision docs, codebase, and discussion.

---

## The Big Picture

Movers currently have no biological needs. They work or wander forever with no urgency. Adding hunger creates the first survival pressure and gives purpose to gathering, crafting, and building.

---

## Design Principle: Needs Are Not Jobs

From `docs/vision/needs-vs-jobs.md` — this is the key architectural decision:

| | Job System | Needs System |
|---|---|---|
| **Driver** | "Colony needs X done" | "This mover needs Y" |
| **Assignment** | Any capable mover | Only that specific mover |
| **Mechanism** | WorkGivers → Job Pool | Personal state machine |
| **Examples** | Mining, hauling, crafting | Eating, sleeping |

**Why separate?** If "eat food" is a job, movers compete for it. One mover might "steal" another's meal assignment. Hunger is personal — a mover gets hungry, walks to food, eats. No job reservation, no WorkGiver, no queue.

Needs sit *above* the job system as interrupts:

```
┌─────────────────────────────────────┐
│           Mover Update              │
├─────────────────────────────────────┤
│  1. Already handling a need?        │
│     → Continue (eat, walk to food)  │
│     → Skip job logic entirely       │
│                                     │
│  2. Starving? (hunger < 0.1)        │
│     → CancelJob(), seek food        │
│                                     │
│  3. Hungry + idle? (hunger < 0.3)   │
│     → Seek food instead of new job  │
│                                     │
│  4. Otherwise                       │
│     → Normal job/wander logic       │
└─────────────────────────────────────┘
```

---

## Mover Struct Additions

```c
// Urges
float hunger;           // 1.0 = full, 0.0 = starving (drains over time)
float energy;           // 1.0 = rested, 0.0 = exhausted (drains while awake)
float bowel;            // 1.0 = empty, 0.0 = desperate (fills after eating) [Phase 2]

// Freetime state machine (bypasses job system)
FreetimeState freetimeState;
int needTarget;                // Item index, cell position, or furniture index
float needProgress;            // Timer for eating/sleeping action
```

```c
typedef enum {
    FREETIME_NONE,
    FREETIME_SEEKING_FOOD,
    FREETIME_EATING,
    FREETIME_SEEKING_REST,
    FREETIME_RESTING,
    // Phase 2:
    FREETIME_SEEKING_LATRINE,
    FREETIME_USING_LATRINE,
} FreetimeState;
```

---

## Needs Priority

Hunger is more urgent than tiredness. A starving mover wakes up to eat.

```
┌─────────────────────────────────────┐
│           Mover Update              │
├─────────────────────────────────────┤
│  1. Already handling a need?        │
│     → Continue (unless starving     │
│        interrupts sleep)            │
│                                     │
│  2. Starving? (hunger < 0.1)        │
│     → Cancel job AND cancel sleep   │
│     → Seek food                     │
│                                     │
│  3. Exhausted? (energy < 0.1)       │
│     → Cancel job, seek rest         │
│                                     │
│  4. Hungry + idle? (hunger < 0.3)   │
│     → Seek food                     │
│                                     │
│  5. Tired + idle? (energy < 0.3)    │
│     → Seek rest                     │
│                                     │
│  6. Otherwise                       │
│     → Normal job/wander logic       │
└─────────────────────────────────────┘
```

---

## Hunger Mechanics

### Drain
- Hunger drains steadily over time, regardless of activity
- Rate: TBD — tune so movers eat roughly every few game-minutes
- Same drain whether working, idle, or sleeping

### Thresholds
| Hunger Value | State | Behavior |
|---|---|---|
| 1.0 – 0.3 | Full / satisfied | Normal work |
| 0.3 – 0.1 | Hungry | Seek food when idle (won't interrupt jobs) |
| < 0.1 | Starving | Interrupt job AND sleep, seek food immediately |

### Recovery
- Eating a food item restores hunger by a nutrition value (e.g., berries = +0.3)
- Eating takes a brief time (e.g., 2 seconds)

### Can movers die of starvation?
- Open question. Options:
  - **No death**: hunger clamps at 0.0, mover moves very slowly
  - **Yes death**: mover dies after extended starvation (adds stakes)
  - Start with no death, add later

---

## Energy / Sleep Mechanics

### Drain
- Energy drains while awake — working drains faster than idling
- Rate: TBD — tune so movers need sleep roughly once per game-day
- Hunger still drains during sleep (a long nap can make you wake up hungry)

### Thresholds
| Energy Value | State | Behavior |
|---|---|---|
| 1.0 – 0.3 | Rested / fine | Normal work |
| 0.3 – 0.1 | Tired | Seek rest when idle (won't interrupt jobs) |
| < 0.1 | Exhausted | Interrupt current job, seek rest immediately |

### Recovery
Recovery speed depends on where the mover rests:

| Location | Recovery Speed | Notes |
|---|---|---|
| Standing/ground | Very slow | Barely recovers, emergency only |
| Chair | Moderate | Better than nothing |
| Bed | Fast | Full recovery in reasonable time |

### Sleep Behavior

**Rest search — fallback chain:**
1. **Owned bed** (future — skip for now)
2. **Any available bed** — walk to nearest unoccupied bed
3. **Any chair** — sit and rest, slower recovery
4. **Ground** — stand/lie on ground, very slow recovery

Like food, this degrades gracefully:
- No furniture at all? Movers rest on the ground (slowly)
- A few chairs? Better than nothing
- Beds for everyone? Colony runs smoothly

### Sleep State Machine

**FREETIME_NONE → FREETIME_SEEKING_REST**
- Triggered when tired + idle, or exhausted (interrupts job)

**FREETIME_SEEKING_REST**
- Search fallback chain (bed → chair → ground spot)
- Pathfind to rest location

**FREETIME_RESTING**
- Energy recovers at rate determined by furniture type
- Mover stays until `energy > 0.8` (well rested threshold)
- Hunger still drains — if starving interrupts sleep
- Mover sprite could change (lying down on bed, sitting on chair)

**FREETIME_RESTING → FREETIME_NONE**
- Energy recovered, return to normal
- Or: interrupted by starvation → FREETIME_SEEKING_FOOD

### What If No Rest Location?

- Mover rests where they are (ground)
- Very slow recovery but not zero
- Player incentive to build beds: colony is more productive

---

## Sleep and Hunger Interaction

These two needs create interesting tension:
- Hunger drains during sleep → long sleeps make you hungry
- Starving interrupts sleep → mover wakes up to eat, then goes back to bed
- Working drains energy faster → busy colonies need more beds
- No food AND no beds → movers spiral: too tired to forage efficiently, too hungry to sleep well

This is emergent gameplay from just two numbers and a priority order

---

## Food Sources

### Berry Bushes (Primary Early Source)

The existing `CELL_BUSH` could be extended, or a new `CELL_BERRY_BUSH` added.

**Option A: Extend CELL_BUSH**
- Add a "has berries" state (e.g., vegetation overlay or flag)
- Pros: no new cell type, bush already exists
- Cons: mixes decorative and food bushes

**Option B: New CELL_BERRY_BUSH**
- Separate cell type with its own sprite
- Has a "fruiting" state and regrowth timer
- Pros: clear separation, can have distinct behavior
- Cons: another cell type

**Behavior:**
- Bushes periodically produce berries (regrowth timer)
- Mover walks to bush, harvests → berries disappear from bush, bush enters cooldown
- Bush regrows berries after N game-seconds
- Terrain generation scatters berry bushes on grass/dirt

### Food Items

**Option A: Single generic item**
- `ITEM_BERRIES` with `IF_EDIBLE` flag
- Simple, sufficient for the first loop

**Option B: Generic ITEM_FOOD with material**
- `ITEM_FOOD` + `MAT_BERRY`, `MAT_APPLE`, etc.
- More extensible for future food types
- Follows the sapling/leaf unification pattern

For the first pass, `ITEM_BERRIES` is simplest. Can unify later if more food types warrant it.

### Future Food Sources (Not Phase 1)
- Fruit trees (apple, pear) — seasonal fruit production
- Hunting — animals drop meat
- Farming — planted crops
- Cooking — workshop that processes raw food into meals

---

## Two Sides of Food: Harvesting (Job) and Eating (Need)

Food has two distinct flows that use different systems:

### Harvesting → Stockpiling (JOB SYSTEM)

Gathering food from bushes is **colony work** — it's a job like mining or hauling:
- Player designates berry bushes for harvest (like tree chopping designation)
- Or: standing "gather food" bill on a stockpile / zone
- WorkGiver assigns an available mover to harvest
- Mover walks to bush, harvests → `ITEM_BERRIES` spawns on ground
- Haul system moves berries to food stockpile
- Bush enters regrowth cooldown

This fills stockpiles with food *before* anyone gets hungry. The colony prepares.

### Eating (NEEDS SYSTEM — NOT a job)

When a mover gets hungry, they seek food through a **personal state machine**:

**Food search — fallback chain:**
1. **Food stockpile** — walk to nearest stockpile with food, take an item, eat
2. **Food on ground** — walk to nearest loose food item, pick up, eat
3. **Berry bush with berries** — walk to bush, eat directly (emergency foraging)
4. **Nothing found** — give up, wander, retry later

The fallback chain means early game (no stockpiles yet) movers eat from bushes directly. Mid game they eat from stockpiles. The system degrades gracefully.

**What if no food exists at all?**
- Mover searches briefly, finds nothing, gives up
- Returns to FREETIME_NONE, resumes wandering/working
- Retries after a cooldown (don't spam search every tick)
- Hunger keeps draining — mover gets slower (or eventually dies, TBD)
- This is a real survival pressure: no bushes + no stockpile = trouble

### Eating at Tables (Optional Comfort Bonus)

If a table + chair exist near the food source (or anywhere reachable):
- Mover **prefers** to carry food to a table and eat there
- Eating at table = future mood/comfort bonus
- Eating standing or on the ground = no penalty, just no bonus
- This is NOT required — movers eat fine without furniture

This connects the food system to the future furniture system elegantly. A dining hall with tables becomes desirable but not mandatory.

**Search with table preference:**
1. Find food (stockpile → ground → bush)
2. Pick up / harvest food item
3. Is there a nearby table+chair? → walk there, sit, eat
4. No table? → eat where you are, standing

Tables are a quality-of-life upgrade, not a gate.

---

## Eating State Machine (Personal, Not Job)

This runs in the mover update loop, bypasses the job system entirely.

### FREETIME_NONE → FREETIME_SEEKING_FOOD

Triggered when:
- `hunger < 0.3` and mover has no job (soft trigger — eat between jobs)
- `hunger < 0.1` regardless of job state (hard trigger — `CancelJob()`, eat NOW)

### FREETIME_SEEKING_FOOD

- Mover searches the fallback chain (stockpile → ground → bush)
- If found: pathfind to food source
- If not found: wait briefly, return to FREETIME_NONE, retry later

### FREETIME_SEEKING_FOOD → FREETIME_EATING

When mover arrives at food:
- Stockpile: take item from slot
- Ground item: pick up item
- Bush: eat directly (no item created, bush enters cooldown)
- Optional: if table nearby, carry food to table first

### FREETIME_EATING → FREETIME_NONE

- Eating takes ~2 seconds (timer)
- When done: hunger += nutrition value, delete food item (if holding one)
- Mover returns to FREETIME_NONE
- Job system can assign new work on next tick

### Failure / Timeout

- If mover can't reach food (path blocked), give up after timeout
- If food disappears while walking (another mover took it), re-search
- Never get stuck in an infinite food-seeking loop
- Cooldown between search attempts (e.g., 5 game-seconds)

---

## Integration Points

### Job System
- `MoverAvailableForWork()` returns false when `freetimeState != FREETIME_NONE`
- `AssignJobs()` skips movers currently handling needs
- Optional: avoid assigning long jobs to nearly-hungry movers

### Stockpile System
- New stockpile filter category: **Food**
- Food items can be hauled to food stockpiles
- Hungry movers check stockpiles as a food source

### Item System
- `IF_EDIBLE` flag already defined (currently unused)
- Add nutrition value to item definition
- `ITEM_BERRIES`: stackable, edible, nutrition = 0.3

### Save/Load
- New fields on Mover struct → bump save version
- Save `hunger`, `freetimeState`, `needTarget`, `needProgress`
- Berry bush state (regrowth timer) needs saving too

### Rendering
- Visual indicator when mover is eating (brief animation, or just stationary)
- Berry bush sprite variants: with berries / without berries
- Hunger indicator on mover tooltip (hunger bar or text)

---

## What We Already Have

| Component | Status | Notes |
|---|---|---|
| `CELL_BUSH` | Exists | Walkable, slows movement, has sprite |
| `IF_EDIBLE` flag | Defined | In item_defs.h, unused |
| `IF_SPOILS` flag | Defined | In item_defs.h, unused (future) |
| `CancelJob()` | Works | Releases all reservations cleanly |
| Job priority system | Works | Sequential assignment, no interrupts yet |
| Vegetation regrowth | Works | Grass grows back via groundwear system |
| Weather/seasons | Works | Could tie berry growth to seasons |
| Animals graze vegetation | Works | Shows vegetation consumption pattern |
| Stockpile filters | Works | Can add food category |

---

## Decisions Made

- **Harvest model**: Both. Harvesting is a **job** (gather → stockpile). Eating from bush is an **emergency fallback** when no stockpiled food exists.
- **Eating at tables**: Optional comfort bonus, not required. Movers prefer tables but eat anywhere.
- **Food search**: Fallback chain — stockpile → ground → bush → give up. Degrades gracefully.

## Open Questions

1. **Bush design**: Extend `CELL_BUSH` or new `CELL_BERRY_BUSH`?
2. **Item design**: `ITEM_BERRIES` or generic `ITEM_FOOD` with material?
3. **Starvation death**: Yes or no? Start without it?
4. **Berry regrowth**: Fixed timer? Seasonal? Tied to moisture/soil?
5. **How many bushes**: What density in terrain gen makes foraging viable without trivializing it?
6. **Multiple food types**: When do we add variety? Just berries for now?
7. **Food spoilage**: `IF_SPOILS` exists but when to implement? (deferred)
8. **Mover tooltip**: Show hunger as number, bar, or descriptive text?
9. **Harvest designation**: Player-designated bushes? Auto-gather zone? Standing bill?
10. **Table search radius**: How far will a mover walk to find a table? (nearby only, or any reachable?)

---

## Implementation Phases

### Phase 1: Needs System + Food + Sleep
1. Hunger and energy fields on Mover, both drain over time
2. Freetime state machine (all 5 states)
3. Needs priority check in mover update, CancelJob on starving/exhausted
4. Berry bush cell with berries state + regrowth timer
5. Food item (`ITEM_BERRIES`, `IF_EDIBLE`, nutrition value)
6. Harvest job (gather berries → stockpile)
7. Eating behavior (fallback chain: stockpile → ground → bush)
8. Furniture entity system (bed, chair, table — minimal)
9. Sleep behavior (fallback chain: bed → chair → ground)
10. Food stockpile filter
11. Berry bush + furniture placement (terrain gen + build menu)
12. Save/load for new fields
13. Tests

### Phase 2: Bowel & Waste Loop
1. Bowel urge on Mover (fills after eating, drains to 0)
2. Latrine construction (workshop-style, produces `ITEM_WASTE`)
3. Compost pile passive workshop (waste → compost over time)
4. Bowel fallback chain (latrine → outhouse → outdoors → accident)
5. Accident mechanic (floor dirt + mood penalty)
6. Animal waste (periodic `ITEM_WASTE` spawn from grazers)
7. `ITEM_COMPOST` item (future farming input)
8. Hauling jobs for waste → compost pile

### Phase 3: Furniture & Comfort
- Carpenter's bench workshop (craft furniture from planks)
- Bed, chair, table entities
- Eating at tables (comfort bonus)
- Sleep quality from furniture type
- Comfort/beauty urge (environment quality)

### Phase 4: Farming & Food Variety
- Farm plots (designate area, till soil)
- Compost as fertilizer (growth speed boost)
- Crop types (from CDDA/DF reference lists)
- Cooking workshop (raw → meals)
- Fruit trees (seasonal production)
- Food spoilage (`IF_SPOILS`)
- Hunting → meat from animals

### Phase 5: Full Survival (LATER)
- Hygiene urge (wash at water source)
- Social urge (stand near other movers)
- Recreation urge (variety requirement)
- Mood as composite score
- Personality variation (DF-inspired tolerance differences)
- Starvation / exhaustion death
- Furniture ownership (my bed)

---

## Future Needs — "Urges" (Same Architecture, Later)

We call these **urges** — personal, biological drives that override colony work. All follow the same pattern: float on Mover, drain rate, soft + hard threshold, fallback chain, personal state machine.

| Urge | Drain | Satisfier | Existing Hook |
|---|---|---|---|
| **Hunger** | Constant | Food (stockpile → ground → bush) | Phase 1 |
| **Energy** | While awake | Sleep (bed → chair → ground) | Phase 1 |
| **Bowel** | After eating | Latrine → outhouse → bush/outdoors | See waste section below |
| **Warmth** | Cold cells | Warm cells, hearths, indoors, clothing | Per-cell temperature exists |
| **Hygiene** | Time / dirt exposure | Wash at water source | Floor dirt tracking exists |
| **Social** | Time alone | Stand near another mover briefly | Movers already have positions |
| **Recreation** | Time without fun | Fun objects / activities | — |
| **Mood** | Composite score | Good food, nice room, met urges | Emerges from other urges |

None beyond hunger/energy are Phase 1. Listed here so the architecture accounts for them — the `FreetimeState` enum and mover update loop should be extensible.

---

## Learnings from Other Games

### What Dwarf Fortress Does
- **Core needs**: hunger, thirst, alcohol preference, sleep, stress
- **Personality system**: 50+ personality facets (anxiety, anger threshold, etc.) modify how much events affect mood
- **Thoughts/memories**: movers remember bad things (ate without table, saw corpse, slept in rain) — thoughts decay over time
- **Trauma**: extended exposure to horror can cause permanent personality changes
- **No bladder need** — DF skips this entirely. Dwarves don't poop (despite the memes). But contamination tracking (blood, vomit, forgotten beast syndrome) is detailed.
- **Key lesson**: DF's depth comes from personality variation more than need count. Two dwarves with identical needs react differently.

### What RimWorld Does
- **8 needs**: food, rest, comfort, beauty, recreation, social (broken into sub-types: talked, had space), chemical (if addicted)
- **Recreation variety**: pawns need *different types* of fun (social, solitary, cerebral, dextrous) — doing the same thing gets boring
- **Comfort**: separate from rest. A pawn can be well-rested but uncomfortable (bad chair)
- **Beauty**: environment quality around the pawn — ugly surroundings drain mood
- **Chemical dependency**: addictions create new mandatory needs (alcohol, drugs)
- **Mood breaks**: at extreme low mood, pawns snap — binge eating, wandering, mental breaks
- **Key lesson**: RimWorld's "variety" requirements (recreation types) prevent degenerate solutions. One billiard table isn't enough.

### What The Sims Does
- **6 classic needs**: hunger, energy, bladder, hygiene, social, fun
- **Bladder is central**: Sims who can't find a toilet wet themselves — creates urgency and comedy
- **Hygiene degrades visibly**: smelly Sims repel others → social need harder to fill
- **Need decay chains**: low hygiene → can't socialize → mood drops → less effective at everything
- **Key lesson**: The Sims shows that needs are most fun when they *interact*. Bowel → hygiene → social → mood is a cascade.

### What Oxygen Not Included Does
- **Stress**: the master need — everything feeds into it
- **Bladder/waste**: dupes use outhouses, produce polluted dirt → compost → fertilizer
- **Water contamination**: polluted water from waste → sieve → clean water (closed loop)
- **Decor**: environmental beauty affects stress (like RimWorld)
- **Key lesson**: ONI treats waste as a *resource pipeline*, not just a penalty. Poop is genuinely useful.

### What We're Taking
1. **Interactions between urges** (Sims-style cascades) — not just independent bars
2. **Waste as a resource** (ONI-style closed loop) — not just a penalty
3. **Personality variation later** (DF-inspired) — same urges, different tolerances
4. **Keep it simple at first** — start with hunger + energy, add bowel next, then comfort/social

---

## Bowel / Waste — The Closed Loop

The user's philosophy: **autarky and entropy**. Nothing should be truly wasted. Poop is a resource.

### The Loop

```
Food → Mover eats → Bowel fills → Mover uses latrine → Waste produced
  ↑                                                          ↓
  │                                                    Compost pile
  │                                                          ↓
  │                                                     Fertilizer
  │                                                          ↓
Crops ← ← ← ← ← ← ← ← ← ← ← ← ← ← ← ← ← ← ← ← Farm plots
```

### Bowel Mechanics

```c
float bowel;  // 1.0 = empty, 0.0 = urgent
```

- Bowel fills (drains toward 0) only after eating — rate proportional to food consumed
- Not a constant drain like hunger — it's event-triggered, then decays

| Bowel Value | State | Behavior |
|---|---|---|
| 1.0 – 0.3 | Fine | Normal activity |
| 0.3 – 0.1 | Needs toilet | Seek latrine when idle |
| < 0.1 | Desperate | Interrupt job, seek latrine NOW |
| 0.0 + timeout | Accident | Soils current cell (hygiene hit, mood hit, floor dirt) |

### Facilities — Fallback Chain

1. **Latrine** (constructed, indoors) — enclosed pit with a seat. Fast, clean, produces waste item
2. **Outhouse** (constructed, outdoors) — simpler. Slower, produces waste
3. **Outdoors / bush** (emergency) — mover goes behind nearest bush or at edge of map. Mood penalty, hygiene hit, leaves waste on ground
4. **Accident** — too late. Soils the cell. Big mood penalty. Other movers who see it: mood hit too

### Waste Items & Processing

| Stage | Item/Cell | Process | Output |
|---|---|---|---|
| **Waste** | `ITEM_WASTE` (from latrine/outhouse) | Haul to compost pile | — |
| **Compost pile** | Workshop (passive, like drying rack) | Time-based conversion | `ITEM_COMPOST` |
| **Fertilizer** | `ITEM_COMPOST` applied to farm plot | Boosts crop growth speed/yield | Crops → food |

The compost pile is a **passive workshop** — dump waste in, wait, get compost out. Follows the drying rack / charcoal pit pattern perfectly.

### Why This Matters

- **Closed loop**: food → poop → compost → fertilizer → crops → food
- **Connects farming** to everything else — farms aren't just "add water"
- **Creates hauling jobs**: waste needs to be moved to compost, compost to farms
- **Incentivizes building latrines**: no latrine = accidents = dirty floors = unhappy movers
- **Floor dirt system already exists**: accidents can use the existing `floorDirtGrid` with high values

### Animal Waste

Animals also produce waste:
- Grazers poop while grazing (at some interval)
- Predators poop less frequently
- Animal waste is the same `ITEM_WASTE` — can be hauled to compost
- **Pastures near compost piles** = efficient waste collection
- Early game: animal poop on the ground is just mess. Mid game: it's a composting resource

This mirrors Farthest Frontier's system: animals in pastures produce manure, collected by compost yards, spread on fields.

---

## Animal Needs in Context

Animals already consume vegetation (grazers) and hunt (predators). Extending to full needs:

| Animal Need | Behavior | Interaction |
|---|---|---|
| **Hunger** | Already exists — grazers eat vegetation | Could overgraze → food shortage |
| **Waste** | New — periodic `ITEM_WASTE` spawn at animal location | Resource for compost |
| **Sleep** | New — animals rest at night, inactive | Predators hunt at dusk/dawn? |
| **Water** | Animals seek water cells periodically | Could drive migration patterns |

Animal needs are simpler than mover needs — no fallback chains, no mood. Animals just *do* things at intervals. But their waste output connects them to the colony's resource loop.

### Taming (Future)

Tamed animals could be:
- Penned in pastures (waste concentrated → easy composting)
- Fed from stockpiles (in winter when vegetation is scarce)
- Milked/sheared (livestock production)

Not Phase 1, but the waste loop makes taming meaningful beyond "pet."

---

## References

- `docs/vision/needs-vs-jobs.md` — Core architecture: needs vs jobs separation
- `docs/vision/freetime.md` — Full freetime design with state machines and phases
- `docs/vision/terrain-and-tiles/edible-plants-plan.md` — Berry bushes, fruit trees, plant lists
- `docs/vision/farming/crops-ccdda.md` — CDDA crop reference (long-term)
- `docs/vision/farming/crops-df.md` — DF crop reference (long-term)
- `docs/todo/naked-in-the-grass.md` — Feature gap analysis identifying hunger as #1 priority
