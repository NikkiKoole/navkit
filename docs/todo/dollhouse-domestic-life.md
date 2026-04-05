# Dollhouse: Domestic Life Vertical Slice

> Status: partial (Phases 1-3 done, Phases 4-5 next, future systems designed)

> A small apartment with 2 movers, a kitchen, a bathroom, and domestic routines. The goal is to make a tiny living space feel *alive* — before scaling to a village of 50. Think The Sims, not Dwarf Fortress.

---

## The Picture

A single apartment. Two movers wake up, use the toilet, boil water for tea, cook breakfast (the bread is warm, it smells good), eat at the table, go to work, come home, cook dinner, sit together, sleep. The apartment gets messy, someone cleans. Food left on the counter goes stale. The hot bread cools down.

This is a **vertical slice** — a small scene that exercises multiple new systems at once. If 2 movers in 1 apartment feel alive, scaling to a village is just more apartments.

---

## Done

### Phase 1: Kitchen Stations (save v91) ✅

1x1 workshops (WORKSHOP_STOVE, WORKSHOP_COUNTER) with capability-based proximity checks.

- `capabilityGrid[z][y][x]` — per-cell bitmask, O(radius²) lookups via `HasNearbyCapability()`
- `uint8_t capabilities` on FurnitureDef and WorkshopDef
- `requiredNearbyCapability` + `proximityRadius` on Recipe (0 = no requirement)
- Bill availability check: if recipe needs nearby capability and none found, bill won't run
- Counter does double duty: 1x1 workshop (sandwich recipe) + `CAP_PREP_SURFACE` provider
- Stove's "Bake Bread" requires `CAP_PREP_SURFACE` within 8 tiles
- New items: ITEM_SANDWICH (composite food, 0.7 nutrition), ITEM_BOILED_WATER (safe drinking)
- Kitchen item flow: stove makes bread → counter makes sandwich (items hauled between stations naturally)
- Future stations follow same pattern: anvil, spinning wheel, pottery wheel, workbench

### Phase 2: Food Temperature (save v92) ✅

- `float temperature` on Item (0.0 = not tracked, 80.0 = freshly cooked)
- Newton's cooling toward ambient room temp (`FOOD_COOLING_RATE 0.02f/s`)
- Food/drink spawned at heat-source workshops starts at 80°C
- `MOODLET_HOT_MEAL` (+1, 4h) when eating food ≥40°C
- Tooltip: "piping hot" (≥60°C), "warm" (≥40°C)

### Phase 3: Bladder + Bathroom (save v93) ✅

- `float bladder` on Mover (1.0=fine, drains over 6 game-hours), `bladderEnabled` toggle
- `FURNITURE_TOILET` — single tile, occupant tracking
- 3-tier relief: toilet (no penalty) > outdoor bush/tree (MOODLET_NO_PRIVACY -1) > emergency (MOODLET_NO_TOILET -2 + floor dirt)
- Freetime states: SEEKING_TOILET, USING_TOILET, SEEKING_BUSH, USING_BUSH
- Priority: desperate bladder interrupts jobs (after dehydrating), normal bladder only when idle
- Balance tuning in balance.h/c
- Tooltip: bladder %, status label, seeking state

### Other done this session

- Room detection + quality scoring + room moodlets (rooms.c, 25 tests)
- Mood indicator: colored dot above movers (green/yellow/orange/red)
- Stove (V) and Counter (U) in workshop placement menu, MAX_BAR_ITEMS 16→24

---

## Next: Buildable Phases

### Phase 3b: Sustainable Dollhouse (~0.5 session)

Movers currently die after ~2 game-days because food runs out. Fix this so the apartment is self-sustaining and you can actually observe domestic behavior long-term.

**Approach:** Berry bushes near the apartment. Movers already know how to harvest berries (DESIGNATION_HARVEST_BERRY). Berries regrow. Food from the environment becomes baseline survival; cooking at the stove becomes a luxury (better food = better mood, not survival necessity).

Implementation:
1. Spawn 2-3 berry bushes in the dollhouse setup (near the door, outside)
2. Pre-designate harvest on them, or let the player do it
3. More starting food (10+ each) to bridge until first harvest
4. Consider: auto-designate berry harvest when movers are hungry? (future)

### Phase 3c: Freetime Action Queue — sit-then-eat (~1 session)

The first Sims behavior: movers actively seek a chair before eating. Currently eating is "walk to food, eat standing wherever." With the queue: pick up food → walk to nearby chair (optional) → sit → eat. Graceful skip if no chair.

This is the moment the dollhouse clicks — you see movers walk to the table, sit down, eat. The kitchen matters. The chairs matter. Without this, domestic life is just survival meters draining.

Implementation: see Future Systems > Action Queue section below. ~500 lines (200 executor + 300 freetime migration), zero risk to jobs.

### Phase 3d: Stale Reservation Root Cause (~0.5 session)

Current band-aid: scan all items each tick for movers in FREETIME_NONE. Works but O(itemHighWaterMark) per mover per tick. The real fix: ensure every freetime state transition properly releases its reservation. More surgical, prevents future bugs.

### Phase 4: Daily Schedule (~1 session)

Per-mover schedule data (defaulting to same global schedule). Designed for future per-mover customization (night shifts, occupations).

```
 6:00 - 7:00   Wake + morning routine (toilet, eat)
 7:00 - 12:00  Work block 1
12:00 - 13:00  Lunch
13:00 - 17:00  Work block 2
17:00 - 18:00  Dinner
18:00 - 21:00  Leisure
21:00 -  6:00  Sleep
```

Implementation:
1. `Schedule` struct on Mover with time block breakpoints
2. `GetCurrentBlock(mover, gameHour)` → SLEEP/MEAL/WORK/LEISURE
3. AssignJobs filter: respect current block (meals prioritize food, sleep prioritizes bed)
4. Default schedule assigned on spawn, save/load the struct
5. ~50-80 lines in AssignJobs + schedule struct

### Phase 5: Food Quality (~0.5 session)

Quality tiers on cooked output:

| Quality | How achieved | Mood effect |
|---------|-------------|-------------|
| **Poor** | Raw food, stale ingredients | -1 (existing ATE_RAW_FOOD) |
| **Normal** | Basic cooking | 0 |
| **Good** | Complex recipe + fresh ingredients + good kitchen | +1 |
| **Excellent** | High skill (future) + good ingredients + proper kitchen | +2 |

Implementation:
1. Quality field on food items (enum)
2. Determined by: recipe complexity + ingredient freshness + kitchen room quality
3. ATE_GOOD_FOOD moodlet becomes quality-aware
4. Tooltip shows quality

---

## Future Systems (designed, not yet buildable)

Three interconnected systems that together create the full Sims feel. Can be built independently, but compose into one flow: **advertisements** choose *what* to do (toilet is urgent) → **planner** resolves *how* (need boiled water, stove nearby) → **action queue** executes *the steps* (walk, pick up, work, eat). Each system is useful on its own.

### 1. Action Queue

Replaces the hardcoded freetime state machine (11 FREETIME_* states) with a dynamic, mutable action sequence on each mover.

```c
typedef struct {
    ActionType type;    // ACT_WALK_TO, ACT_PICK_UP, ACT_WORK_AT, ACT_SIT, ACT_EAT, ACT_DROP, ACT_WAIT
    int targetIdx;
    float duration;
    float progress;
    bool optional;      // skip if can't be fulfilled
    float searchRadius;
} Action;
```

**Key behaviors:**
- **Optional steps** — "sit in chair before eating" inserted as optional; skipped if no chair nearby. Same plan, different outcomes depending on environment.
- **Queue mutation** — steps can be inserted/removed at runtime. Late-resolved ingredients, dynamic sub-tasks.
- **Interrupt-resume** — critical need (bladder) pushes current queue, executes interrupt, pops back. Mover takes a leak mid-mining and resumes at 60% progress. No job cancellation, no lost work.

**Implementation strategy (freetime first, ~500 lines, zero risk to jobs):**
1. Build action queue executor (~200 lines, generic)
2. Migrate freetime onto it (~300 lines) — unlocks sit-then-eat, optional steps
3. Multi-station crafting uses the queue for cross-station plans (new behavior)
4. Optionally migrate existing jobs later if queue proves itself (net -1700 lines, but deferred)

**First action queue behavior:** sit-then-eat. `[pick up food (req), walk to chair (opt), sit (opt), eat (req)]`

**Code audit (2026-03-31):** 35 job types, 8-10 primitive actions cover all of them. 90% of jobs are walk+work or haul patterns. Craft (13 conditional steps) is the only complex one. Full job migration would be 2-3 sessions, but is unnecessary for freetime queue.

### 2. Production Planner

Player sets a goal ("make sandwich"), system auto-creates sub-tasks by walking the recipe DAG backward.

**Recipes define WHAT, environment defines HOW.** A recipe says "I need ITEM_BOILED_WATER" — the planner scans nearby stations to find one that can produce it:

```
Goal: ITEM_PASTA
  Need: ITEM_DOUGH        → exists nearby? grab it
  Need: ITEM_BOILED_WATER → doesn't exist
    → stove nearby + water? sub-plan: [pick up water, walk to stove, work]
    → campfire nearby? also works, but further away
    → nothing available? can't make pasta, bail
```

Same pasta recipe works at a stove, campfire, or hearth. Different kitchens → different action queues → same recipe definition.

Three layers: recipe is *declarative* (what), planner is *contextual* (what's available here), queue is *imperative* (go do these steps).

**Option A and B coexist.** Current item-chain approach (A: separate bills, items hauled between stations) remains as fallback. Planner/queue approach (B: one mover carries multi-step plan) is an optimization on top. Mixed chains work — some steps are B, distant steps degrade to A.

### 3. Advertisement System

Objects broadcast need satisfaction. Unifies the four separate search functions in needs.c (`StartFoodSearch`, `StartDrinkSearch`, `StartRestSearch`, `StartWarmthSearch`) into one scored scan.

```c
typedef struct {
    NeedType need;   // HUNGER, THIRST, ENERGY, BLADDER, FUN...
    float rate;      // satisfaction quality
    float radius;    // broadcast range (0 = unlimited)
} Advertisement;
```

Scoring: `needUrgency * ad.rate / (1 + distance)` with behavioral dithering (pick from top 2-3, not always #1).

**Freetime only, never interferes with jobs.** Plugs into `ProcessMoverFreetime()` — replaces hardcoded search loops, doesn't touch AssignJobs or RunJob_*.

**Reservations: use existing mechanisms.** `ReserveItem()` for items (already used by freetime food/drink seeking), `furniture.occupant` for furniture (already used by rest seeking). No new reservation type needed.

---

## Scope Decisions (Open)

1. **Neolithic or modern?** Toilet/stove feel modern. Options: era-gated (late-game upgrade), rethemed (pit latrine, clay stove), or separate era setting.

2. **Sink water source:** Magic "has water" flag for v1, or proximity to water source? Simplest: sink within N tiles of well/river/pot counts as having water.

3. **Workshop-evolution relationship:** Kitchen stations ARE Phase 2 of `workshop-evolution-plan.md`. Same 1x1 workshop + proximity pattern extends to anvil, forge, etc.

---

## The Sims Moments This Enables

- **Morning:** Mover wakes up, goes to toilet, walks to kitchen, cooks bread on the stove. Bread is hot. Sits at table, eats. +HOT_MEAL, +ATE_AT_TABLE, +NICE_ROOM. Mood is great.
- **Evening:** Grabs leftover sandwich from counter. Room temperature. No hot meal bonus. "I should have cooked."
- **Neglect:** Food went stale. Kitchen floor dirty. ATE_STALE_FOOD + UGLY_ROOM. "This apartment needs attention."
- **Improvement:** Player places window. Light goes up, room quality improves. +NICE_ROOM. "That window made a difference."
- **Personality without traits:** One mover ate hot bread at the table. The other ate cold leftovers standing up (chair occupied). Same apartment, different experiences.

---

## Relationship to Other Docs

| Doc | Relationship |
|-----|-------------|
| village-next-steps.md | Village is *scale*, dollhouse is *depth* — complementary |
| workshop-evolution-plan.md | Kitchen stations implement Phase 2 of that plan |
| schedule-system.md | Dollhouse Phase 4 uses per-mover version |
| sims-inspired-priorities.md | Dollhouse is the concrete implementation of the Sims feel |
| mood-moodlets-personality.md | Dollhouse adds triggers (hot meal, bladder), mood system is done |
| endgame-village-vision.md | Dollhouse is a small-scale prototype of the endgame |
