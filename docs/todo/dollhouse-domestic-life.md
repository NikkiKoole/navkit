# Dollhouse: Domestic Life Vertical Slice

> Status: idea

> A small apartment with 2 movers, a kitchen, a bathroom, and domestic routines. The goal is to make a tiny living space feel *alive* — before scaling to a village of 50. Think The Sims, not Dwarf Fortress.

---

## The Picture

A single apartment. Two movers wake up, use the toilet, boil water for tea, cook breakfast (the bread is warm, it smells good), eat at the table, go to work, come home, cook dinner, sit together, sleep. The apartment gets messy, someone cleans. Food left on the counter goes stale. The hot bread cools down.

This is a **vertical slice** — a small scene that exercises multiple new systems at once. If 2 movers in 1 apartment feel alive, scaling to a village is just more apartments.

---

## What This Tests

| System | What we learn |
|--------|--------------|
| Stations (workshop evolution Phase 2) | Does placing a stove + counter + table feel natural? Do capability-based recipes work? |
| Plumbing (light version) | Can a toilet/sink/water source work without a full pipe network? |
| Food temperature | Does hot/cold food affect mood? Does cooling over time create urgency? |
| Food quality tiers | Does baked bread feel different from raw berries, beyond just nutrition? |
| Schedule system (light) | Do movers wake, eat, work, eat, sleep on a daily rhythm? |
| Room quality (existing) | Does the apartment score well? Does improving it feel rewarding? |
| Mood (existing) | Do all these domestic details feed into mood legibly? |

---

## The Apartment Layout

```
 Rough sketch — not exact tiles, just the feel:

 +--+--+--+--+--+--+--+
 |  BED  |     |TOILET |
 |       | HALL|  SINK  |
 +--+--+-+    ++--+--+-+
 |  BED  |     |       |
 |       +--D--+ STOVE |
 +--+--+--+   | CNTR  |
 |  TABLE  |   |       |
 |  CHAIR  +D--+--+--+-+
 |  CHAIR  |
 +--+--+--+
```

3-4 rooms: bedroom(s), kitchen/dining, bathroom. Connected by doors. Small enough to see everything at once.

---

## New Systems Needed

### 1. Kitchen Stations

**Core decision: stations are 1x1 workshops with proximity checks.**

A stove is a `WORKSHOP_STOVE` with template `"X"` (1x1). It has its own bill queue, assigned crafter, all existing workshop flow. No new entity type needed.

Some recipes require nearby **furniture with capability tags**. The stove's "Bake Bread" recipe requires a `CAP_PREP_SURFACE` within 2 tiles. The counter provides that capability. If no counter is nearby, the recipe is unavailable.

**Multi-step cooking uses item chains, not compound recipes.** "Make a sandwich" is 3 separate jobs across 2 stations — items flow between them naturally, same as the existing wood loop (sawmill → rope maker → braid). The job state machine doesn't change.

#### Code changes

**FurnitureDef gains capability bits:**
```c
// furniture.h
typedef enum {
    CAP_NONE          = 0,
    CAP_PREP_SURFACE  = 1 << 0,  // counter, table
    CAP_WATER_SOURCE  = 1 << 1,  // sink, water barrel
} CapabilityFlag;

// Add to FurnitureDef:
uint8_t capabilities;  // bitmask
```

**Recipe gains optional proximity requirement:**
```c
// Add to Recipe struct:
uint8_t requiredNearbyCapability;  // 0 = no requirement
uint8_t proximityRadius;           // tiles (default 2)
```

**One new scan function (~20 lines):**
```c
// furniture.c
bool HasNearbyCapability(int x, int y, int z, uint8_t capBits, int radius);
```

**Bill availability check (~5 lines):** if recipe has `requiredNearbyCapability` and `HasNearbyCapability()` returns false, bill won't run.

#### New stations

| Station | Type | Template | Recipes it owns | Nearby requirement |
|---------|------|----------|----------------|-------------------|
| **Stove** | `WORKSHOP_STOVE` (1x1) | `"X"` | Cook Meat, Bake Bread, Boil Water, Brew Tea | Bread needs `CAP_PREP_SURFACE` within 2 |
| **Counter** | `WORKSHOP_COUNTER` (1x1) | `"X"` | Make Sandwich, Slice Bread (future) | None — it IS the prep surface |

The counter does **double duty**: it's a 1x1 workshop (owns no-heat recipes like sandwich) AND furniture with `CAP_PREP_SURFACE` (so the stove can see it for bread).

#### Kitchen item flow

```
Stove bill: "Cook Meat"         raw meat → cooked meat (sits at stove output)
Stove bill: "Bake Bread"        flour + water → bread [HOT] (needs counter nearby)
Counter bill: "Make Sandwich"   bread + cooked meat → sandwich

3 separate jobs. Items hauled between stations naturally.
Same pattern as: sawmill strips bark → rope maker twists → rope maker braids.
```

#### New recipes

| Station | Recipe | Input | Output | Nearby req | Notes |
|---------|--------|-------|--------|-----------|-------|
| Stove | Cook Meat | RAW_MEAT x1 | COOKED_MEAT x1 | — | Already exists on campfire/hearth |
| Stove | Bake Bread | FLOUR x1 | BREAD x1 | `CAP_PREP_SURFACE` | Already exists on campfire — now also at stove |
| Stove | Boil Water | WATER x1 | BOILED_WATER x1 | — | New item: safe drinking water |
| Stove | Brew Tea | WATER x1 + DRIED_GRASS x2 | HERBAL_TEA x1 | — | Already exists on campfire |
| Counter | Make Sandwich | BREAD x1 + COOKED_MEAT x1 | SANDWICH x1 | — | New composite food, no heat needed |

#### What doesn't change

- Workshop struct — stove is just another WorkshopType
- Bill struct — unchanged
- Craft job state machine (10 steps) — unchanged
- Passive workshop flow — unchanged
- Template workshops (stonecutter etc.) — unchanged
- Save format for workshops — just new enum values

#### Future stations (same pattern)

Once stove + counter work, the same 1x1-workshop + proximity-check pattern extends:

| Future station | Recipes | Nearby reqs |
|---------------|---------|-------------|
| Anvil | Forge tools, shape metal | `CAP_HEAT_SOURCE` (from forge furniture) |
| Spinning wheel | Spin fiber, spin wool | — |
| Pottery wheel | Shape clay | `CAP_WATER_SOURCE` (from sink/barrel) |
| Workbench | General crafting | Depends on recipe |

### 2. Food Temperature

Items gain a `float temperature` field. Hot food is better.

**Mechanics:**
- Freshly cooked food spawns HOT (e.g., 80.0)
- Temperature decays toward ambient room temperature over time (Newton's cooling — simple exponential decay)
- Hot food gives mood bonus: `MOODLET_HOT_MEAL` (+1, 4h)
- Room-temperature food: no bonus, no penalty
- Cold food in cold room: mild penalty (optional, defer)

**What temperature affects:**
- Mood only (not nutrition — a cold sandwich still feeds you)
- Tooltip shows temperature qualitatively: "piping hot", "warm", "room temperature", "cold"
- Visual: steam particles above hot food items (tiny cosmetic detail, big feel impact)

**Implementation notes:**
- `float temperature` on Item struct (only meaningful for food items, 0.0 = not tracked)
- Decay in item tick: `temp -= (temp - ambientTemp) * coolingRate * dt`
- Ambient temp from existing temperature grid
- Save/load: just another float field on Item

### 3. Food Quality Tiers

Beyond raw/cooked — how *well* was it made?

| Quality | How achieved | Mood effect | Tooltip |
|---------|-------------|-------------|---------|
| **Awful** | Burnt (left on heat too long — future) | -2 | "burnt to a crisp" |
| **Poor** | Raw food, monotonous diet | -1 (existing: ATE_RAW_FOOD) | "barely edible" |
| **Normal** | Basic cooking (current cooked meat, bread) | 0 | "decent" |
| **Good** | Cooked with skill (future: skill system) | +1 | "well-made" |
| **Excellent** | High skill + good ingredients + proper kitchen | +2 | "exquisite" |

For now (no skill system yet), quality is determined by:
- **Recipe complexity**: sandwich > cooked meat > raw berries (more steps = better base quality)
- **Kitchen quality**: room quality of the room where cooking happened influences output quality
- **Ingredient freshness**: FRESH inputs → normal+, STALE inputs → poor quality output

This creates a feedback loop: better kitchen → better food → better mood → faster work → resources for better kitchen.

### 4. Toilet / Bathroom (Light Version)

A new need axis. Simple implementation — no pipe network.

**The need:**
- `float bladder` on Mover (0.0 = fine, 1.0 = desperate)
- Drains slowly over time (similar to hunger/thirst)
- When urgent: mover seeks toilet furniture
- No toilet available: mover "goes outside" (mood penalty, floor dirt at location)
- Toilet available: walk to toilet, use it (brief animation/timer), bladder reset, no penalty

**Furniture:**
- `FURNITURE_TOILET` — single tile, satisfies bladder need
- `FURNITURE_SINK` — already needed for kitchen, also serves bathroom (hand washing after toilet = no mood penalty, skip for v1)

**Moodlets:**
- `MOODLET_USED_TOILET` — no effect (neutral, expected)
- `MOODLET_NO_TOILET` (-2, 4h) — had to go outside/in the open
- `MOODLET_CLEAN_BATHROOM` (+0.5, 2h) — bathroom room quality above threshold (future)

**Why include this:** The Sims proved that bladder is one of the most engaging needs because it's *urgent* and *frequent*. It creates constant small decisions and makes bathrooms matter as rooms. The sims-inspired doc originally punted this as "too modern for neolithic" — but in the dollhouse/apartment context, it fits perfectly.

**Scope decision:** Is the dollhouse a separate game mode? A future era? Or does the neolithic game eventually evolve into apartments? If neolithic stays the core setting, toilet could be era-gated (only available after certain tech). If we're building toward the endgame village vision, toilet is just an earlier-than-planned feature.

### 5. Light Schedule System (from schedule-system.md, minimal version)

Just enough to make the apartment day feel structured:

```
 6:00 - 7:00   Wake + morning routine (toilet, wash — future)
 7:00 - 7:30   Breakfast (find food, sit at table, eat)
 7:30 - 12:00  Work (leave apartment, do jobs)
12:00 - 13:00  Lunch
13:00 - 17:00  Work
17:00 - 18:00  Dinner (cook if ingredients available, eat at table)
18:00 - 21:00  Free time (sit, wander, socialize — future)
21:00 -  6:00  Sleep
```

**Implementation (minimal):**
- `ScheduleBlock GetCurrentBlock(float gameHour)` — returns SLEEP/MEAL/WORK/LEISURE
- In AssignJobs: during MEAL block, mover prioritizes finding food + eating. During SLEEP, prioritizes bed
- During WORK: existing behavior (grab nearest job)
- During LEISURE: idle (walk around, sit on chair — existing freetime behavior)

This is ~30-50 lines in AssignJobs. Not per-mover customization yet, just a global day structure.

---

## What Already Exists (Reusable)

| System | Status | Reuse |
|--------|--------|-------|
| Room detection + quality | Done (rooms.c) | Apartment rooms scored automatically |
| Mood + moodlets | Done (mood.c) | All new triggers feed into existing system |
| Furniture + occupant tracking | Done (furniture.c) | Bed, chair already work |
| Food items + cooking | Done | Berries, cooked meat, bread, etc. |
| Spoilage | Done (FRESH/STALE/ROTTEN) | Feeds into food quality |
| Temperature grid | Done | Ambient temp for food cooling |
| Floor dirt + cleaning | Done | Kitchen/bathroom get dirty |
| Freetime state machine | Done | Basis for schedule blocks |
| Needs (hunger, thirst, energy) | Done | Add bladder alongside |

---

## Build Order

### Phase 1: The Kitchen (~1-1.5 sessions)

Get the apartment cooking loop working:

1. **`uint8_t capabilities` on FurnitureDef** — bitmask, zero for all existing furniture
2. **`HasNearbyCapability()` scan** — ~20 lines in furniture.c
3. **`requiredNearbyCapability` + `proximityRadius` on Recipe** — zero = no requirement (backward compat)
4. **Bill availability check** — ~5 lines: if recipe needs nearby cap and none found, bill won't run
5. **WORKSHOP_STOVE** — 1x1 workshop, recipes: Cook Meat, Bake Bread (needs counter), Boil Water, Brew Tea
6. **WORKSHOP_COUNTER** — 1x1 workshop + `CAP_PREP_SURFACE` furniture, recipes: Make Sandwich
7. **ITEM_SANDWICH** — new composite food item (bread + cooked meat)
8. **"Ate at table" moodlet already works** — existing room+chair detection

Architecturally small: 2 new workshop types, 1 new furniture capability field, 1 scan function, 1 recipe field. Everything else is existing code.

### Phase 2: Food Temperature (~0.5 session)

Make hot food matter:

1. **Temperature field on Item** — float, decays toward ambient
2. **Hot food moodlet** — MOODLET_HOT_MEAL when eating food above threshold
3. **Tooltip display** — "piping hot" / "warm" / "room temperature"
4. **Steam particles** (optional, cosmetic)

### Phase 3: Bathroom (~0.5-1 session)

Add the bladder need:

1. **Bladder need** on Mover — drain rate, urgency threshold
2. **FURNITURE_TOILET** — satisfies bladder, single tile
3. **Freetime/need integration** — mover seeks toilet when urgent
4. **Moodlets** — NO_TOILET penalty
5. **Save version bump** — new need field

### Phase 4: Daily Schedule (~0.5 session)

Structure the day:

1. **Schedule blocks** — SLEEP/MEAL/WORK/LEISURE based on game hour
2. **AssignJobs filter** — respect current block
3. **Morning routine** — wake → toilet → eat → work (emergent from need priorities + schedule)

### Phase 5: Food Quality (~0.5 session)

Quality tiers on cooked output:

1. **Quality field on food items** — enum (AWFUL/POOR/NORMAL/GOOD/EXCELLENT)
2. **Quality determination** — recipe complexity + ingredient freshness + kitchen room quality
3. **Quality moodlets** — ATE_GOOD_FOOD becomes quality-aware
4. **Tooltip** — quality shown on food items

---

## Scope Decisions (Open)

1. **Is this a separate mode or part of the main game?** Could be a "domestic test scene" that loads a pre-built apartment. Or it could be what the endgame colony naturally evolves into (movers live in apartments you built). Probably: build the systems, test them in a pre-built scene, then they naturally appear in the colony when players build apartments.

2. **Neolithic or modern?** The toilet and stove feel modern. Options:
   - **Era-gated:** Stove is a late-game upgrade over campfire. Toilet needs pottery/plumbing tech
   - **From the start:** Pit latrine (primitive toilet), clay stove (primitive stove). Rename/retheme but same mechanics
   - **Separate setting:** The dollhouse is explicitly "later era" — apartments, plumbing, proper kitchens. The neolithic colony grows into this over time

3. **Sink water source:** Does the sink just "have water" magically, or does it need a water connection? For v1, simplest is: sink within N tiles of a water source (well, river, pot) counts as having water. Full plumbing later.

4. **How does this relate to the workshop-evolution plan?** This implements the spirit of Phase 2 (station entities) using the simplest possible mechanism: 1x1 workshops + proximity checks on furniture capabilities. The stove is the first station. If this works, the pattern extends to anvil, forge, spinning wheel, etc. The apartment kitchen is the proving ground. The full capability-scan system (recipes float free, no workshop ownership) can come later if/when an anvil+forge combo needs it — where neither station is the "primary."

---

## Future: Production Planner & Sims-Style Action Queue

Right now multi-step cooking is manual: the player sets up separate bills on each station (stove: "bake bread", counter: "make sandwich") and items flow between them via hauling. This works but doesn't scale to complex recipes.

### The Goal: "Make Lasagna"

The player says "make lasagna" (the end product). The game walks the recipe DAG backward:
- Lasagna needs pasta + sauce + cheese
- Pasta needs flour → queue "make pasta" on nearest counter
- Sauce needs tomatoes → queue "cook sauce" on nearest stove
- Once intermediates exist → queue "assemble lasagna" on counter

A **production planner** that decomposes a goal into sub-tasks across stations. The recipes already form a DAG through inputs/outputs — the solver just walks it backward.

### The Sims Action Queue

In The Sims, "make dinner" decomposes into a visible queue of sub-actions on one Sim:
1. Walk to fridge → get ingredients
2. Walk to counter → chop/prep
3. Walk to stove → cook
4. Walk to counter → plate it
5. Walk to table → serve

The key differences from our current system:
- **One mover owns the whole chain** — they walk between stations, carrying intermediates. No hauling jobs between steps.
- **Visible queue on the mover** — you see "cooking meat... next: assemble sandwich". The player understands the plan.
- **Cancellable mid-chain** — interrupt at any step, intermediates drop where they are.

This is the natural evolution of our current item-chain approach (Option A) toward a mover-carried-plan approach (Option B from the station design discussion). The architecture path:

1. **Now (done):** Separate bills on separate stations, items hauled between them
2. **Next:** Production planner auto-creates bills from a goal recipe (player sets goal, system manages bills)
3. **Later:** Mover action queue — one mover carries a multi-step plan, walks between stations. The craft job state machine grows from "go to one workshop, make one thing" to "execute a sequence of station visits"

Step 2 is a job-system feature (bill auto-generation). Step 3 is a mover-system feature (action queue). They're independent — step 2 alone removes most of the manual bill management pain.

### Action queue shape

The existing craft state machine (13 CRAFT_STEP_* states) is already an action queue — just hardcoded. The generalized version makes it dynamic:

```c
typedef struct {
    ActionType type;    // ACT_WALK_TO, ACT_PICK_UP, ACT_WORK_AT, ACT_SIT, ACT_EAT, ACT_DROP, ACT_WAIT
    int targetIdx;      // item, furniture, or workshop index
    float duration;     // for WORK_AT, WAIT
    float progress;     // 0 → duration
    bool optional;      // skip if can't be fulfilled (no chair nearby, etc.)
    float searchRadius; // how far to look for optional targets
} Action;
```

The executor is simple: each tick, look at the current action — ACT_WALK_TO sets goal and waits for arrival, ACT_PICK_UP grabs item, ACT_WORK_AT accumulates progress. When done, advance to next. Queue empty = mover is idle.

### Optional actions — graceful degradation

Not every step is required. Some are opportunities:

```
"Eat food" plan:
  [0] ACT_PICK_UP    (food)         — required
  [1] ACT_WALK_TO    (nearby chair) — optional, radius 6
  [2] ACT_SIT        (chair)        — optional
  [3] ACT_EAT        (food)         — required
```

Mover grabs sandwich, looks around — chair nearby? Sit, eat, get +ATE_AT_TABLE moodlet. No chair? Skip, eat standing. No failure, no replanning, just different mood outcome.

The executor for optional steps: `if (action.optional && !CanFulfill(action)) { advance; continue; }`.

This means the **plan compiler doesn't need to know** if a chair exists when building the queue. It always inserts "sit in chair" as optional. The runtime figures it out. Plans become reusable templates that produce different outcomes depending on what's available:

- Furnished kitchen: pick up bread → sit in chair → eat → +HOT_MEAL, +ATE_AT_TABLE, +NICE_ROOM
- Empty field: pick up bread → *(skip chair)* → eat → +HOT_MEAL only
- Same plan, different environment, different mood. That's the Sims feel.

Extends to everything:
- **Cook at stove:** required steps + optional "wash hands at sink" (mood bonus)
- **Go to sleep:** walk to bed (required) + optional "change clothes" (future cosmetic)
- **Multi-station crafting:** required forge steps + optional "put on apron" or "check bellows"

### Relationship to existing systems

### Why not replace existing jobs with the queue immediately?

The current craft/haul/mine/build state machines work, have 100+ tests, and handle edge cases (item gone mid-carry, workshop destroyed, mover died, cancellation cleanup). Rewriting them into queue format risks breaking all of that for zero gameplay benefit — they'd do the exact same thing, just structured differently.

The queue is worth building for things the current system **can't do**:
- Freetime "sit then eat" — can't today, hardcoded states
- Multi-station crafting in one flow — can't today, one workshop per job
- Optional opportunistic steps — can't today, every step is required

So: build the queue for new behaviors first (freetime, multi-station plans), prove it works. If it turns out simpler and more robust than the old state machines, migrate them over one by one. The craft state machine's 13 hardcoded steps are exactly what a 7-entry queue would do — that refactor would simplify jobs.c significantly. But it's earned through confidence in the new system, not imposed upfront.

The freetime state machine (SEEKING_FOOD → EATING → NONE) is the natural first migration target — it's simpler than craft, and it's where the queue unlocks new behavior (sit-then-eat) that players will immediately notice.

### Refactoring scope analysis (code audit, 2026-03-31)

The existing job system has 35 job types. Analyzed what a full migration to the action queue would look like:

**Job patterns** — most jobs are simple, one is complex:
- ~50% are walk + work (mine, chop, clean, gather, till) — 2 steps
- ~20% are pick up + carry + drop (haul, deliver, clear) — 3 steps
- ~10% are haul + work (plant, fertilize) — 4 steps
- 1 outlier: Craft with 13 conditional steps (0-3 inputs + optional fuel)

**8-10 primitive actions** cover all 35 job types: WALK_TO, PICK_UP, DROP, WORK_FOR, TOOL_FETCH, plus specials (hunt chase, explore beeline).

**Full migration cost:** Remove ~2500 lines of RunJob_* functions, add ~500-800 lines for generic executor. Net -1700 lines. But risky — touches the most tested code (100+ tests), craft job has conditional branching that's hard to express as a linear queue, hunt has dynamic re-targeting, clear computes drop location after pickup. 2-3 sessions of work.

**The key insight: we don't need to migrate jobs to get the queue for freetime.** The freetime state machine and the job system are separate code paths. Building the queue for freetime only:

- ~200 lines: action queue executor (shared between freetime and future job migration)
- ~300 lines: freetime migration (replace 11 hardcoded FREETIME_* states with queue-driven behavior)
- Zero changes to any RunJob_* function
- Zero risk to existing job tests

**Strategy: freetime first, job migration later (if ever).**

1. Build the action queue executor (~200 lines, generic)
2. Migrate freetime onto it (~300 lines) — this unlocks sit-then-eat, optional steps, the Sims feel
3. Multi-station crafting uses the queue for the mover's cross-station plan (new behavior, not refactoring)
4. Optionally migrate simple jobs (mine, haul) one at a time if the queue proves itself — each batch independently testable
5. Craft job migrates last (or never — it works fine as-is)

The queue earns trust through new behavior, not through rewriting old behavior.

### Option A and B coexist

Option B doesn't replace A — it's an acceleration layer on top. Both use the same stations, recipes, and bills.

- **Option A (item chains):** Stove job finishes → bread drops at output → hauling job moves it to counter → counter job makes sandwich. Three jobs, possibly three different movers.
- **Option B (mover action queue):** One mover gets "make sandwich" plan → cooks bread at stove → carries it directly to counter → assembles. One job, one mover, no hauling step.

They coexist because:
- **A is the fallback.** If no mover picks up the full chain (busy, no planner, player set up manual bills), items flow via hauling. Nothing breaks.
- **B is an optimization.** When available, one mover skips the drop-haul-pickup cycle by carrying intermediates between stations.
- **Mixed chains work.** Steps 1-2 might be B (one mover cooks and carries), but step 3 needs a distant station — that intermediate gets dropped and hauled via A. Graceful degradation.

Like a kitchen where sometimes the chef preps-cooks-plates in one flow (B), and sometimes one person preps, puts it on the pass, and someone else cooks (A). Same stations, same recipes — just different workflow depending on who's available.

---

## Future: Advertisement System (Object Broadcasting)

Objects broadcast what needs they can satisfy. Movers in freetime scan nearby advertisements and pick the best option. This replaces the current hardcoded need-seeking with a unified, extensible pattern.

### What already exists (proto-advertisements)

The furniture rest-seeking code in `needs.c` is already this pattern:
```c
// Score: restRate / (1 + dist/CELL_SIZE) — prefer better furniture nearby
float score = def->restRate / (1.0f + dist / CELL_SIZE);
```
This is an advertisement: the bed broadcasts "I satisfy energy at rate 0.040/s", the chair broadcasts "I satisfy energy at rate 0.015/s", and the mover picks the best score (quality / distance). But it's only implemented for energy/rest. Hunger, thirst, warmth all use separate hardcoded search logic.

### The generalized pattern

```c
typedef struct {
    NeedType need;          // NEED_HUNGER, NEED_THIRST, NEED_ENERGY, NEED_BLADDER, NEED_FUN...
    float rate;             // how well this satisfies the need (higher = better)
    float radius;           // how far the broadcast reaches (0 = unlimited)
} Advertisement;
```

Any entity can advertise: furniture (bed→energy, toilet→bladder, chair→comfort), items (food→hunger, water→thirst), stations (stove with hot food→hunger), even cells (campfire→warmth). A mover in freetime scans advertisements within range, scores them against current needs, and picks the best:

```
score = needUrgency * ad.rate / (1 + distance)
```

With behavioral dithering (pick randomly from top 2-3, not always #1) so movers feel like individuals.

### What advertises what

| Entity | Need | Rate | Notes |
|--------|------|------|-------|
| Plank bed | Energy | 0.040 | Already works this way |
| Leaf pile | Energy | 0.028 | Already works this way |
| Chair | Energy/Comfort | 0.015 | Already works this way |
| Toilet | Bladder | 1.0 | Future — dollhouse Phase 3 |
| Food items (IF_EDIBLE) | Hunger | nutrition value | Currently hardcoded search |
| Drink items (IF_DRINKABLE) | Thirst | hydration value | Currently hardcoded search |
| Campfire/hearth | Warmth | heat output | Currently hardcoded "walk to fire" |
| Bench (future) | Comfort/Fun | 0.5 | Leisure furniture |
| Other movers nearby | Social | proximity | Future — "idle near others = fun" |

### Critical boundary: advertisements do NOT interfere with jobs

This system is **freetime only**. The boundary is strict:

- **Mover has a job** → job system is in full control (WorkGivers, craft state machine, designations). Advertisements are invisible. The mover goes where the job says.
- **Mover in freetime / no job** → advertisement system drives autonomous behavior. Scan nearby objects, pick best need satisfaction.
- **Need interrupt** → existing pattern unchanged (starvation/exhaustion override jobs at critical thresholds). Advertisements don't create new interrupts — they just make the freetime *choices* smarter.

The existing `ProcessMoverFreetime()` in needs.c is exactly where advertisements plug in. It's already the "no job, what should I do?" code path. We're replacing the separate hardcoded search loops (scan furniture for rest, scan items for food, scan items for drink) with one unified scoring pass.

### Open question: freetime reservations

Advertisements drive freetime behavior — movers are "free" (no job) while acting on an advertisement. Multi-step freetime actions (cook meat, then assemble sandwich with bread) have a time gap where ingredients could get stolen.

**Preferred approach: just use existing reservation mechanisms.** Two mechanisms already exist and track different things:

- **`ReserveItem()`** = "I claim this item" (food, drink). Prevents two movers targeting the same bread. Already used by freetime food/drink seeking.
- **`furniture.occupant`** = "I'm physically in this" (bed, chair). Tracks who's sitting/sleeping. Released when they stand up.

Both are already used in freetime code — no new concept needed. The advertisement system just checks the right one when scoring: `reservedBy` for items, `occupant` for furniture. One-line branch, not a new system.

Note: there's no "walking toward" reservation for furniture — two movers can both start walking to the same chair. The second one arrives, finds it occupied, and picks another. This is fine (realistic even) and not worth adding complexity for.

### Implementation path

Currently needs.c has four separate search functions (`StartFoodSearch`, `StartDrinkSearch`, `StartRestSearch`, `StartWarmthSearch`) that all do the same thing: scan entities, score by quality/distance, pick best, reserve, walk there. The advertisement system consolidates them into one pass.

1. **Define `Advertisement` struct** and add it to FurnitureDef (replaces `restRate` with a more general concept)
2. **Add advertisements to ItemDef** — food items advertise hunger satisfaction, drinks advertise thirst
3. **Unify freetime need-seeking** — replace the four `Start*Search()` functions with one unified scan: collect all advertisements in range, score by `needUrgency * rate / distance`, pick top option with dithering. The freetime states (SEEKING_FOOD, RESTING, etc.) stay as-is — the advertisement replaces *how* the target is chosen, not the state machine that executes it.
4. **Extend to new needs** — bladder (toilet advertises), comfort (chair/bench), fun (future leisure objects)

Each step is backward compatible — the advertisement just formalizes what the code already does for furniture rest-seeking.

---

## The Sims Moments This Enables

- **Morning:** Mover wakes up, goes to toilet, walks to kitchen, cooks bread on the stove. Bread is hot. They sit at the table and eat. +HOT_MEAL, +ATE_AT_TABLE, +NICE_ROOM. Mood is great.

- **Evening:** Mover comes home tired, grabs a leftover sandwich from the counter. It's room temperature. No cooking bonus, no hot meal bonus. Mood is okay but not great. "I should have cooked."

- **Neglect:** Nobody cooked, food on the counter went stale. Mover eats stale sandwich. ATE_STALE_FOOD penalty. Kitchen floor is dirty (crumbs). UGLY_ROOM. "This apartment needs attention."

- **Improvement:** Player places a window in the kitchen. Light level goes up. Room quality improves. Next meal: +NICE_ROOM. "That window made a difference."

- **The Sims moment:** One mover is happy (ate hot bread at the table in a nice kitchen). The other is grumpy (ate cold leftovers standing up because the chair was occupied). Same apartment, different experiences. That's personality without a personality system.

---

## Relationship to Other Docs

| Doc | Overlap | Difference |
|-----|---------|------------|
| village-next-steps.md | Rooms, mood, schedules, occupations | Village is about *scale*, dollhouse is about *depth* |
| workshop-evolution-plan.md | Station entities, capability-based recipes | Dollhouse implements Phase 2 of that plan |
| schedule-system.md | Daily schedule blocks | Dollhouse uses minimal version (global, not per-mover) |
| sims-inspired-priorities.md | Mood, visual feedback, domestic feel | Dollhouse is the concrete implementation of the Sims feel |
| mood-moodlets-personality.md | New moodlets (hot meal, no toilet) | Dollhouse adds triggers, mood system is already done |
| endgame-village-vision.md | Apartments, plumbing, daily routines | Dollhouse is a small-scale prototype of the endgame |
