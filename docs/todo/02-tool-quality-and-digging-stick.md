# Digging Stick & Tool Quality System — Codebase Fit Analysis

> Written 2026-02-20. Cross-references feature-04, naked-in-the-grass-details.md, and current codebase state.

---

## CDDA Inspiration

In Cataclysm: DDA, the digging stick is a primitive bootstrap tool:
- Crafted from a stick (sharpened end)
- Has `DIG` tool quality at level 1 (lowest)
- Used for: digging pits (finding stone/clay), tilling farm plots, removing grass for straw
- The key design: tools provide **quality levels**, not binary unlock. A digging stick does the same work as a shovel, just slower.

NavKit's design docs (naked-in-the-grass-details.md) already propose this exact pattern.

---

## Reference Research: Dwarf Fortress & CDDA

> Added 2026-02-21. How both games handle tool quality, to inform NavKit's design.

### Dwarf Fortress

**Tool model**: Binary requirement — you either have the tool or you can't do the job. No bare-hands fallback for tool-labors.

**Three exclusive tool-labors** (a dwarf can only hold one at a time):
- **Mining** → requires **pick** (mandatory, cannot mine without one)
- **Woodcutting** → requires **battle axe** (metal only)
- **Hunting** → requires **crossbow** + quiver + bolts

**Everything else is tool-free**: construction, crafting, hauling, masonry, carpentry — all done bare-handed.

**Mining specifics**:
- Same tool (pick) for both rock and soil — no distinction in tool requirement
- Soil mines **much faster** than stone (implicit terrain hardness)
- Soil produces nothing; stone has 25% chance to yield a boulder

**Speed modifiers stack**:
- Skill: biggest factor (legendary = 5x faster than novice)
- Tool quality: up to 2x at masterwork (20% per quality tier, 6 tiers)
- Tool material: steel > iron > copper (matters more in modern versions)
- Terrain type: soil >> stone

**Tool ownership**: Tools are personal, not shared. Dwarves carry their tool and don't put it down until unassigned. Assigning a second tool-labor silently unassigns the first and drops the old tool.

**Key takeaway for NavKit**: DF treats tools as hard gates (no pick = no mining). NavKit's "bare hands at 0.5x" is more forgiving and better suited to the survival bootstrap loop.

### Cataclysm: Dark Days Ahead

**Tool model**: Quality-level requirements. Each recipe/action requires a specific quality type at a minimum level. Having a higher level than required gives **no speed bonus** — it's binary (meets threshold or not).

**80+ quality types** — very granular. Relevant ones for NavKit:
- **CUT** (Cutting): knives, sharp stones — levels 1-2
- **DIG** (Digging): digging stick (1), shovel (2), pickaxe (3)
- **HAMMER** (Hammering): rock (1), hammer (2-3)
- **SAW_W** (Wood Sawing): handsaw (1-2)
- **AXE** (Tree Cutting): hatchet (1), axe (2)
- **CHISEL** (Chiseling): separate from cutting
- **BUTCHER** (Butchering): separate from cutting
- **COOK** (Cooking): separate quality entirely
- **PRY** (Prying): crowbars, levers
- **WRENCH**, **SCREW**, **DRILL** — for mechanical work

**DIG quality specifically**:
- Level 1 (digging stick): pits, channels, charcoal kilns — soft ground
- Level 2 (shovel): clearing rubble, standard construction
- Level 3 (pickaxe): mining through rock walls, heavy excavation

**Rocks as tools**: Rocks have HAMMER:1. CDDA recently **removed the stone hammer** because rocks at HAMMER:1 made it redundant — the crafted item added nothing.

**Tool proximity**: Tools auto-detected within 6-tile radius — don't need to be wielded or carried for crafting. NavKit's "equipped tool" model is closer to DF's personal-carry approach.

**Item quality vs tool quality**: CDDA separates these — tool quality level (1-3, requirement threshold) is independent of item quality (shoddy/normal/good/exquisite, which gives minor speed bonuses of -5% to +5%).

**Key takeaway for NavKit**: CDDA's granular quality types are overkill for NavKit's scope, but the DIG-vs-HAMMER split for terrain types is directly applicable. NavKit's speed-scaling formula (0.5x → 1.0x → 1.5x → 2.0x) is more interesting than CDDA's binary model.

### Design Lessons Applied

1. **Split digging from hammering by terrain material** (from both games)
2. **Rocks as HAMMER:1 is fine** — just make sure hammering alone doesn't cover everything (CDDA's mistake with the stone hammer)
3. **Keep quality types minimal** — CDDA's 80+ is way too many. NavKit needs ~5-6 max for the stone age
4. **Tools as speed multipliers, not hard gates** — more forgiving than DF, more interesting than CDDA's binary
5. **Skill matters more than tools in DF** — NavKit doesn't have skills yet, so tools carry all the progression weight for now. If skills are added later, rebalance tool impact downward
6. **Separate butchering from cutting** — CDDA does this and it's useful. A sharp stone can cut but butchering might want its own quality (future feature 05)

---

## Current Codebase State (as of save v64)

### Already Implemented
- **ITEM_SHARP_STONE** — exists (save v61), first tool item, spawned by knapping
- **DESIGNATION_KNAP + JOBTYPE_KNAP** — fully working; mover picks up rock, walks to stone wall, knaps, produces sharp stone with wall's material
- **KNAP_WORK_TIME** = 0.8 game-hours
- **All tool recipe inputs exist**: ITEM_ROCK, ITEM_STICKS, ITEM_CORDAGE
- **9 workshop types** including WORKSHOP_CAMPFIRE
- **Survival systems**: hunger, energy, body temperature, weather/seasons
- **34 item types**, 23 job types, 16 designation types
- **Item flags**: bits 0-6 used (IF_STACKABLE through IF_CONTAINER), bit 7 free
- **Workshop construction costs** — blueprint-based building with material requirements (v63)
- **Workshop deconstruction** — tear down workshops for material refund (v64)

### NOT Implemented (the gap)
- No tool quality enum (QUALITY_CUTTING, QUALITY_DIGGING, etc.)
- No quality data on item definitions
- No IF_TOOL flag
- No equippedTool slot on Mover struct
- No speed multiplier in job execution — all movers work at identical speed
- No tool-seeking behavior
- No tool items beyond sharp stone (no digging stick, stone axe, stone pick, stone hammer)
- No workshop/knapping recipes for crafting tools from components

### Key Observation
Sharp stones exist but are **inert** — they don't speed up any job. The entire tool quality framework from feature-04 is unbuilt.

### Survival vs Sandbox
- **Sandbox**: MODE_DRAW allows instant terrain/workshop placement (creative cheat mode). Tool speed is irrelevant since workers are bypassed.
- **Survival**: Should use MODE_WORK exclusively (designation → job → mover). Tool speed matters here — it's the core progression mechanic.
- **Current gap**: MODE_DRAW and MODE_SANDBOX are not gated by game mode. Survival players can press `D` and bypass the entire worker/material system. This should be restricted (separate concern from tool quality, but worth noting — MODE_DRAW/MODE_SANDBOX should be hidden from keyboard when `gameMode == GAME_MODE_SURVIVAL`).
- **Sandbox workers**: When sandbox players do use workshops/designations, movers should work at 1.0x baseline regardless of tools (no penalty for missing tools in sandbox).
- **Tool requirements toggle**: A UI toggle (`toolRequirementsEnabled`, default true in survival, false in sandbox) that disables all tool checks — hard gates become passable, speed penalties disappear, all movers work at 1.0x. This lets sandbox play work exactly as it does now, and is useful during development for testing systems without needing the full tool chain. Saved per-game (part of game settings, not global preference).

---

## Open Questions (RESOLVE BEFORE IMPLEMENTING)

### 1. How does "requires cutting:1 to craft" actually work mechanically? RESOLVED

The tool must be the mover's **carried tool**. Movers carry one tool at a time (no clothes/inventory yet). For a recipe requiring cutting:1, the mover must be carrying a sharp stone (or better) when they craft.

**Crafting flow**: mover checks recipe's required quality → if their carried tool doesn't satisfy it, they search for a suitable tool on the ground or in stockpiles → walk to pick it up → then proceed to the workshop with the tool.

Tools are **never consumed** by crafting — only input items and fuel are consumed. The tool stays in the mover's hand after crafting completes.

### 2. Tool lifecycle — equip, carry, drop, return RESOLVED

**Carry model**: A mover carries one tool at a time (`equippedTool` item index, -1 = none). No inventory/clothing yet — just one hand slot.

**Lifecycle**:
- **Pick up**: When assigned a job that needs a tool quality they don't have, mover detours to grab a suitable tool from the ground or stockpile (nearest first). Tool gets reserved on pickup (like item hauling).
- **Carry**: Mover keeps the tool across jobs. If the next job needs the same tool, no detour needed.
- **Drop**: When the mover needs a *different* tool (e.g. had a pick, now needs a sharp stone for cutting), they drop the current tool at their feet and go pick up the new one.
- **Death**: Tool drops at mover's position (like any carried item).
- **Save/load**: `equippedTool` is saved/loaded as part of Mover struct. Migration from v64 sets it to -1.
- **Contention**: Tools use the existing item reservation system (`reservedBy`). If a tool is already reserved, the mover skips it and searches for another. If none available: for soft jobs, proceed bare-handed (0.5x); for hard-gated jobs, skip the job entirely.

### 3. No durability — intentionally deferred

Tools are **permanent** for now. No wear, no breakage. This keeps the initial implementation simple and avoids adding a material sink before the crafting economy is fleshed out. Durability can be added in a later phase once the base tool system is stable and there are enough recipes to make replacement feel fair rather than tedious.

### 4. Speed formula only covers designation jobs — what about craft/build jobs? RESOLVED

**Resolution**: Full job-type-to-quality mapping, informed by DF/CDDA research and the digging-vs-hammering terrain split.

**Principle**: Some jobs are **hard-gated** (impossible without the right tool — like DF), others are **soft** (always doable bare-handed, just slower). This gives the bootstrap loop real teeth while keeping movers productive for basic tasks.

**Hard-gated jobs** — tool required, no fallback:

| Job Type | Quality Required | Min Level | Notes |
|----------|-----------------|-----------|-------|
| JOBTYPE_MINE (rock) | QUALITY_HAMMERING | 2 | Need stone hammer or stone pick. Rock (hammer:1) isn't enough. |
| JOBTYPE_CHANNEL (rock) | QUALITY_HAMMERING | 2 | Follows material like MINE |
| JOBTYPE_DIG_RAMP (rock) | QUALITY_HAMMERING | 2 | Follows material like MINE |
| JOBTYPE_CHOP | QUALITY_CUTTING | 1 | Need sharp stone or axe to fell trees |
| JOBTYPE_CHOP_FELLED | QUALITY_CUTTING | 1 | Need sharp stone or axe to process trunks |
| JOBTYPE_CRAFT | *per-recipe* | *per-recipe* | Recipe struct gets `requiredQuality` + `requiredLevel`. Knapping spot recipes need cutting:1. Sawmill needs sawing:1. Campfire recipes may need none. |
| Future: JOBTYPE_BUTCHER | QUALITY_CUTTING | 1 | Or separate QUALITY_BUTCHERING if needed |
| Future: JOBTYPE_TILL | QUALITY_DIGGING | 1 | Farming — digging stick's key use |

**Soft jobs** — bare hands = 0.5x, tools speed it up:

| Job Type | Quality Checked | Notes |
|----------|----------------|-------|
| JOBTYPE_MINE (soil) | QUALITY_DIGGING | dirt/clay/sand/peat/gravel — you can scoop with hands |
| JOBTYPE_CHANNEL (soil) | QUALITY_DIGGING | Excavating downward into soft ground |
| JOBTYPE_CHANNEL (rock) | QUALITY_HAMMERING | Hard-gated (min 2), follows material like MINE |
| JOBTYPE_DIG_RAMP (soil) | QUALITY_DIGGING | Follows material like MINE |
| JOBTYPE_BUILD | QUALITY_HAMMERING | Placing walls/floors — stacking materials, tools help |
| JOBTYPE_KNAP | none | Bootstrap action — smash rock against wall, no tool needed |

**Tool-free jobs** — no quality check, no speed scaling:

| Job Type | Notes |
|----------|-------|
| JOBTYPE_HAUL | DF precedent: hauling is always tool-free |
| JOBTYPE_GATHER_GRASS | Bare-handed work, trivial effort |
| JOBTYPE_GATHER_SAPLING | Bare-handed work |
| JOBTYPE_GATHER_BERRIES | Bare-handed work |
| JOBTYPE_CLEAN | Sweeping, no tool needed |
| JOBTYPE_DECONSTRUCT | Tear-down, no tool needed (could add hammering later) |

**Material check for MINE/CHANNEL/DIG_RAMP**: The job system already knows the target cell. At job creation or work-start, check `GetWallMaterial(x,y,z)` — if it's a stone material (`IsStoneMaterial()`), use QUALITY_HAMMERING (hard-gated, min level 2); otherwise use QUALITY_DIGGING (soft, bare hands OK at 0.5x). This means MINE, CHANNEL, and DIG_RAMP all require different tools depending on what's being excavated.

**When a mover can't do a hard-gated job**: The job stays unassigned until a mover with the right tool is available. Movers without the tool skip it and look for other work. This mirrors DF behavior — no pick, no mining.

### 5. ITEM_ROCK having QUALITY_HAMMERING:1 undercuts progression — RESOLVED

**Resolution**: Rock keeps QUALITY_HAMMERING:1, but **rock mining is hard-gated at hammering:2** while **soft terrain (dirt/clay/sand) uses QUALITY_DIGGING (soft, bare hands OK)**. This splits the problem cleanly:

- Rocks have hammering:1 — useful for soft-hammering jobs (building at 1.0x) but **not enough for rock mining** (needs hammer:2). A loose rock is not a mining tool.
- Soft terrain digging requires QUALITY_DIGGING → bare hands = 0.5x → digging stick = 1.0x → stone pick = 1.5x. Digging is the bootstrapping bottleneck (channels, ramps, farming).
- Stone hammer (hammering:2) **unlocks** rock mining. This makes it a real milestone — the colony must craft proper tools before accessing underground stone.

**Precedent**: Both DF and CDDA support this split:
- **DF**: Mining requires a pick — no bare-hands fallback, hard gate. Same tool for rock and soil, but soil is much faster.
- **CDDA**: DIG quality levels map to terrain difficulty: DIG:1 = soft ground (digging stick), DIG:3 = rock walls (pickaxe). Rocks have HAMMER:1 in CDDA too, and they recently **removed the stone hammer item** because rocks at HAMMER:1 made it redundant. NavKit avoids this by hard-gating rock mining at hammer:2 — the rock's hammer:1 is useful for building, not for mining.

**Terrain-to-quality mapping**:
| Terrain | Quality checked | Gate type | Why |
|---------|----------------|-----------|-----|
| Rock/stone walls | QUALITY_HAMMERING (min 2) | Hard | You're breaking hard material — need a real tool |
| Dirt/clay/sand/peat/gravel | QUALITY_DIGGING | Soft | You're moving soft material — hands work, tools help |
| Channeling (rock) | QUALITY_HAMMERING (min 2) | Hard | Follows material |
| Channeling (soil) | QUALITY_DIGGING | Soft | Follows material |
| Ramp carving (rock) | QUALITY_HAMMERING (min 2) | Hard | Follows material |
| Ramp carving (soil) | QUALITY_DIGGING | Soft | Follows material |

### 6. No interaction with the existing mover capabilities system — DEFERRED

Currently all movers have all professions enabled. Capabilities (`canHaul`, `canMine`, etc.) are role restrictions; tools are speed modifiers. These are orthogonal systems. When profession specialization is added later, capabilities gate *whether* a mover can take a job, tools gate *how fast* (or whether, for hard-gated jobs). No action needed now.

### 7. Stockpile and hauling behavior for tools — RESOLVED

Tools are just items. They can be hauled, stockpiled, and filtered like any other item. The existing `equippedTool` field on Mover uses the item reservation system (`reservedBy`) — a tool that's equipped/reserved won't be hauled by other movers. When a mover drops a tool, it becomes unreserved and haulable again. Tools should be **non-stackable** (IF_TOOL but NOT IF_STACKABLE) — a mover needs exactly one tool, and SplitStack complexity isn't worth it for tools. Stockpile filters get a "Tools" category.

### 8. Knapping spot workshop overlaps with existing knapping designation — DEFERRED

The existing knapping designation (walk to stone wall, knap rocks → sharp stone) works fine for the tool quality bootstrap. A dedicated knapping workshop would need "environmental requirements" (adjacent stone wall or boulder) — this is a broader workshop feature that also applies to future workshops like mud-making (needs water access), clay filtering, etc. Defer the knapping spot workshop to that feature.

For now, composite tool crafting (digging stick, stone axe, stone pick, stone hammer) can use any simple existing workshop — e.g. the campfire or a new generic "crafting spot" — since they only need cutting:1 + basic materials, not a rock surface.

---

## Where the Digging Stick Fits

### Item Definition

| Property | Value |
|----------|-------|
| Name | Digging Stick |
| Recipe | 1x STICKS, requires cutting:1 (use sharp stone to whittle the point) |
| Qualities | QUALITY_DIGGING: 1 |
| Flags | IF_TOOL |
| Weight | 1.5 kg |
| Workshop | Any simple workshop (campfire, crafting spot) |

### Role in Progression

```
Bare hands         → 0.5x soft jobs, CAN'T do hard-gated jobs
Sharp stone        → cutting:1, fine:1       (unlocks chopping, 1x cutting)
Digging stick      → digging:1               (1x soil digging)
Stone hammer       → hammering:2             (unlocks rock mining at 1x)
Stone axe          → cutting:2, hammering:1  (1.5x cutting, hammer:1 for building)
Stone pick         → digging:2, hammering:2  (1.5x soil, unlocks rock mining at 1x)
```

The digging stick is the **digging equivalent of the sharp stone** — cheapest possible tool for its quality type. It bridges bare-handed soil digging (0.5x) to normal speed (1x). Rock mining requires hammering:2 (stone hammer or stone pick) — it's a harder milestone in the bootstrap.

### What It Speeds Up

| Job Type | Quality Checked | Bare Hands | Digging Stick (dig:1) | Stone Pick (dig:2) |
|----------|----------------|------------|----------------------|-------------------|
| MINE (soil) | QUALITY_DIGGING | 0.5x | 1.0x | 1.5x |
| CHANNEL | QUALITY_DIGGING | 0.5x | 1.0x | 1.5x |
| DIG_RAMP (soil) | QUALITY_DIGGING | 0.5x | 1.0x | 1.5x |
| Future: TILL | QUALITY_DIGGING | 0.5x | 1.0x | 1.5x |

Note: rock mining and rock ramp carving use QUALITY_HAMMERING instead — the digging stick doesn't help there. A rock (hammer:1) or stone hammer (hammer:2) is needed for rock.

### Bootstrap Loop

```
1. Gather sticks, grass, berries (bare-handed, tool-free)
2. Dig soil bare-handed at 0.5x (slow but possible — for clay, sand, etc.)
3. Harvest grass → dry → twist → braid → CORDAGE
4. Knap rocks at stone wall → SHARP_STONE (cutting:1) — knapping is tool-free
   → Unlocks: tree chopping, felled trunk processing, tool crafting
5. Craft digging stick: STICKS (needs cutting:1 — use sharp stone to whittle)
   → Soil digging now 1x instead of 0.5x (cheapest tool, no cordage needed)
6. Craft stone hammer: ROCK + STICKS + CORDAGE (needs cutting:1)
   → Unlocks: ROCK MINING (was impossible before!)
7. Craft stone pick: ROCK + STICKS + CORDAGE (needs cutting:1)
   → Soil digging 1.5x, rock mining 1x (replaces hammer for mining)
8. Craft stone axe: ROCK + STICKS + CORDAGE (needs cutting:1)
   → Tree chopping 1.5x
```

Note: rock mining is **completely locked** until step 6 (stone hammer) or step 7 (stone pick). This creates a meaningful early-game milestone — the colony must invest in the cordage chain before accessing stone resources underground.

---

## Dependency Chain

```
Feature 04 Phase 1: Tool Quality Framework     ← REQUIRED FIRST
  │  (quality enum, IF_TOOL, equippedTool on Mover, speed formula)
  │
  ├── Feature 04 Phase 2: Tool Items            ← digging stick lives here
  │     (ITEM_DIGGING_STICK, ITEM_STONE_AXE, ITEM_STONE_PICK, ITEM_STONE_HAMMER)
  │
  ├── Feature 04 Phase 3: Knapping Spot Recipes
  │     (craft tools at 1x1 workshop from rocks+sticks+cordage)
  │
  ├── Feature 04 Phase 4: Tool Seeking
  │     (movers grab tools from stockpile before starting jobs)
  │
  ├── Feature 05 (Cooking/Hunting)
  │     (butchering needs QUALITY_CUTTING >= 1)
  │
  └── Feature 06 (Farming)
      (tilling needs QUALITY_DIGGING >= 1 — digging stick's moment to shine)
```

Feature 03 (Doors/Shelter) has **no tool dependency** and can be done independently.

---

## Implementation Plan for Tool Quality + Digging Stick

### Phase 1: Quality Framework (~1 session)

**New files**: `src/entities/tool_quality.h`, `src/entities/tool_quality.c`

```c
// tool_quality.h
typedef enum {
    QUALITY_CUTTING,    // chopping trees, harvesting, butchering, whittling
    QUALITY_HAMMERING,  // rock mining, construction, knapping, stonecutting
    QUALITY_DIGGING,    // soil mining, channeling, ramp carving, farming
    QUALITY_SAWING,     // sawmill work (planks from logs) — per-recipe check on JOBTYPE_CRAFT
    QUALITY_FINE,       // precision crafting, carpentry — per-recipe check on JOBTYPE_CRAFT
    QUALITY_COUNT
} QualityType;

typedef struct {
    QualityType type;
    int level;          // 0=none, 1=crude, 2=good, 3=excellent
} ItemQuality;

#define MAX_ITEM_QUALITIES 3  // max qualities per item type
```

**Changes to existing files**:
- `item_defs.h/c`: Add `ItemQuality qualities[MAX_ITEM_QUALITIES]` to ItemDef table
- `items.h`: Add `IF_TOOL (1 << 7)`
- `mover.h`: Add `int equippedTool;` field (item index, -1 = none)
- `jobs.c`: In `RunWorkProgress()`, look up equipped tool quality vs job's required quality, apply speed formula: `effectiveTime = baseTime / (0.5 + 0.5 * toolLevel)`
- `designations.h`: Add `QualityType requiredQuality` per designation/job type
- `save_migrations.h`: Bump to v65 for new Mover field + new item types
- `saveload.c` + `inspect.c`: Save/load equippedTool, migration from v64
- `unity.c` + `test_unity.c`: Include new .c file
- `tooltips.c`: Show equipped tool in mover tooltip (e.g. "Equipped: Stone Pick"). Sprite overlay for tools is a future polish task.

**Quality assignments for existing items**:
```
ITEM_ROCK:         { QUALITY_HAMMERING, 1 }
ITEM_SHARP_STONE:  { QUALITY_CUTTING, 1 }, { QUALITY_FINE, 1 }
ITEM_STICKS:       (no tool quality)
```

**Speed formula** (updated for hard-gate model):
```c
// For hard-gated jobs: check tool meets minimum level first
if (job is hard-gated && toolLevel < job.minLevel) {
    // Mover can't do this job — skip it, find other work
    return;
}

// For soft jobs OR hard-gated jobs where tool qualifies:
// Speed scales with how far the tool level exceeds the job's minimum
int levelsAboveMin = toolLevel - job.minLevel;  // 0 = just meets requirement
float speedMultiplier = 1.0f + 0.5f * levelsAboveMin;
// levelsAboveMin  0 (meets minimum):    1.0x  (baseline)
// levelsAboveMin +1 (one above min):    1.5x
// levelsAboveMin +2 (two above min):    2.0x
float effectiveWorkTime = baseWorkTime / speedMultiplier;

// Soft jobs have minLevel = 0. Bare hands = toolLevel 0 → meets min → 1.0x
// But we WANT bare hands to be 0.5x for soft jobs. So for soft jobs,
// treat "no tool" as not meeting the min — apply 0.5x penalty:
if (job is soft && toolLevel == 0) speedMultiplier = 0.5f;

// Examples:
// Soft (soil mining, minLevel=0): no tool → 0.5x, dig:1 → 1.0x, dig:2 → 1.5x
// Hard (rock mining, minLevel=2): below 2 → can't do, hammer:2 → 1.0x, hammer:3 → 1.5x
```

### Phase 2: Tool Items (~0.5 session)

Add 4 new item types (save version bump already done in Phase 1):

| Item | Recipe | Qualities |
|------|--------|-----------|
| ITEM_DIGGING_STICK | 1x STICKS | digging:1 |
| ITEM_STONE_AXE | 1x ROCK + 1x STICKS + 1x CORDAGE | cutting:2, hammering:1 |
| ITEM_STONE_PICK | 1x ROCK + 1x STICKS + 1x CORDAGE | digging:2, hammering:2 |
| ITEM_STONE_HAMMER | 1x ROCK + 1x STICKS + 1x CORDAGE | hammering:2 |

All require QUALITY_CUTTING >= 1 to craft (need sharp stone or better).

**Recipe struct change**: Add `QualityType requiredQuality; int requiredQualityLevel;` to Recipe. The crafter (or a nearby stockpile item) must provide the required quality level. This is a new concept — current recipes only check input items and fuel, not tool quality.

**Files**: items.h (enum), item_defs.c (definitions), stockpiles (filter keys), save_migrations.h (item count update), tooltips (show qualities), workshops.h (Recipe struct)

### Phase 3: Tool Recipes on Existing Workshops (~0.5 session)

Add tool recipes to an existing workshop (campfire, or a new generic "crafting spot" if needed). No new workshop type required — the deferred knapping spot workshop (see Q8) can be added later as part of the "environmental requirements" feature.

Recipes (all require QUALITY_CUTTING >= 1 — mover must carry a sharp stone or better):
- Digging Stick: 1x STICKS → 1x DIGGING_STICK
- Stone Hammer: 1x ROCK + 1x STICKS + 1x CORDAGE → 1x STONE_HAMMER
- Stone Axe: 1x ROCK + 1x STICKS + 1x CORDAGE → 1x STONE_AXE
- Stone Pick: 1x ROCK + 1x STICKS + 1x CORDAGE → 1x STONE_PICK

Sharp stone crafting stays as the existing knapping designation (walk to stone wall). No workshop recipe for sharp stones yet.

### Phase 4: Tool Seeking (~0.5 session)

When considering a job, mover checks:
1. Is this a hard-gated job? → check if equipped tool (or nearest available tool) meets the minimum level. If not, **skip this job** and look for other work.
2. Is this a soft job? → check equipped tool. If it has the right quality, proceed. If not, search for a suitable tool nearby. If none found, proceed bare-handed (0.5x).
3. Does my equipped tool already have the right quality? → proceed directly, no detour.
4. Is there a suitable tool on the ground or in a stockpile? → detour to pick it up, then proceed.

Tool-seeking is opportunistic. Movers never refuse soft work (worst case: bare-handed at 0.5x). Movers skip hard-gated jobs they can't fulfill — the job stays in the queue for a mover with the right tool.

---

## What This Enables

Once the quality system + digging stick exist:

- **Hard gates create real milestones** — rock mining and tree chopping are locked behind tool crafting, giving the bootstrap loop dramatic progression beats
- **Soft jobs still benefit** — soil digging, building, channeling all scale with tool quality (0.5x → 1.0x → 1.5x)
- **Feature 05 (Hunting)**: butchering is hard-gated behind QUALITY_CUTTING — sharp stone works, stone knife is better
- **Feature 06 (Farming)**: tilling is hard-gated behind QUALITY_DIGGING — digging stick is the minimum tool, stone pick is faster
- **Progression feel**: day 1 is gathering + soil work (bare hands). Day 2 has sharp stones (chopping unlocked). Day 3+ has hammer/pick (rock mining unlocked)
- **Material chain payoff**: the cordage chain (grass→dry→twist→braid) is now the critical path to unlocking stone-age tools

---

## Estimated Scope — Context Breakdown

Each "context" = one Claude conversation session focused on a coherent chunk of work. Estimated 5 contexts total.

### Context 1: Quality data model + speed formula + unit tests — COMPLETE ✅

**Commit**: `ba69ce3` — Tool quality framework: data model, speed formula, IF_TOOL flag (save v65)

**Goal**: The data layer exists and the math works. No behavior changes yet — movers still ignore tools.

**Delivered**:
- New files: `tool_quality.h`, `tool_quality.c`, `tests/test_tool_quality.c`
- Quality enum (CUTTING, HAMMERING, DIGGING, SAWING, FINE), ItemQuality struct, MAX_ITEM_QUALITIES=3
- `GetItemQualityLevel(itemType, qualityType)` lookup from static table
- `ItemHasAnyQuality(itemType)` helper
- `GetToolSpeedMultiplier(toolLevel, minLevel, isSoft)` — soft/hard gate model
- Quality assignments: ITEM_ROCK → hammer:1, ITEM_SHARP_STONE → cutting:1 + fine:1
- `IF_TOOL` flag (bit 7) on item_defs.h, `ItemIsTool()` macro
- `equippedTool` field on Mover struct (-1 = none), initialized in `InitMover()`
- `toolRequirementsEnabled` global toggle (saved/loaded with game settings)
- Save version bump to v65, `MoverV64` legacy struct, migration in saveload.c + inspect.c
- Inspector: `--mover N` shows equipped tool name, summary shows tools toggle status
- Unity includes for new files
- Makefile: `test_tool_quality` target added to test suite
- **31 tests, 51 assertions**: quality lookups, IF_TOOL flag, speed formula (soft/hard/bare/tool), mover init, game scenarios

**Deferred to Context 2**:
- Job-to-quality mapping table (which job type needs which quality)
- Inspector `--audit` check for equippedTool validity (needs tool seeking first)
- Inspector item quality display (nice-to-have, not blocking)

### Context 2: Speed scaling wired into jobs + hard gates

**Goal**: Tool quality actually affects job speed. Hard-gated jobs are skippable. The game plays differently.

- Wire speed formula into `RunWorkProgress()` — check mover's equippedTool quality vs job requirement
- Material check for MINE/CHANNEL/DIG_RAMP: `IsStoneMaterial()` → HAMMERING hard, else DIGGING soft
- Hard gate logic in job assignment: mover skips jobs it can't fulfill (no tool for hard-gated)
- Soft job bare-hands penalty: 0.5x when no tool and toolRequirementsEnabled
- Toggle: `toolRequirementsEnabled = false` → all jobs 1.0x, no gates
- Tooltip: show "Equipped: [tool name]" in mover tooltip
- **E2E tests**: Stories 1-5 (bare hands slow, digging stick fast, rock mining blocked, rock mining with hammer, chop blocked)
- **Story 9** (toggle disables everything)

### Context 3: Tool items + recipes + new item types

**Goal**: The 4 new tool items exist and can be crafted at a workshop.

- 4 new item types: ITEM_DIGGING_STICK, ITEM_STONE_AXE, ITEM_STONE_PICK, ITEM_STONE_HAMMER
- Item definitions: names, sprites, IF_TOOL flag, quality assignments
- Recipe struct change: add `requiredQuality` + `requiredQualityLevel` fields
- Tool recipes on campfire (or crafting spot): digging stick (sticks), hammer/axe/pick (rock+sticks+cordage), all need cutting:1
- Stockpile filter: "Tools" category with filter keys for each tool type
- Save migration: new item type count
- **Tests**: item spawning, quality lookups for new types, recipe requires cutting:1 check
- **Story 11** (rock helps building but not mining — tests rock's hammer:1 vs hammer:2 gate)

### Context 4: Tool seeking + carry/drop lifecycle

**Goal**: Movers autonomously find, pick up, carry, and swap tools. This is the hardest context — new job steps.

- Tool seeking in job assignment: before accepting a hard-gated job, check equipped tool or search nearby
- New craft/designation job steps: STEP_SEEK_TOOL, STEP_PICKUP_TOOL (or piggyback on existing walk-to-item)
- Tool carry across jobs: mover keeps tool, no drop between same-quality jobs
- Tool swap: drop current tool (unreserve, place on ground), seek new one
- Death drops equipped tool (unreserve, place at mover position)
- Reservation: equipped tool is reserved via existing `reservedBy` system
- **E2E tests**: Stories 6 (seek tool before chop), 7 (keep tool across jobs), 8 (drop and swap), 12 (death drops tool)

### Context 5: Full bootstrap integration + polish

**Goal**: The whole chain works end to end. Play-test, fix edge cases, polish.

- **Story 10** (full bootstrap: knap → craft digging stick → dig soil)
- Edge cases: all tools reserved (mover proceeds bare-handed on soft, skips hard), multiple movers competing for same tool, tool on different z-level
- Sandbox mode: verify toolRequirementsEnabled=false works for full gameplay
- State audit: add tool-related invariant checks (equippedTool points to valid reserved item, dead movers don't hold tools)
- Event log: instrument tool pickup/drop/swap events
- Play-test in survival mode: verify the bootstrap loop feels right timing-wise
- Final test sweep: run all 31+ test suites, confirm no regressions

### Summary

| Context | Focus | Stories | Key risk |
|---------|-------|---------|----------|
| 1 | Data model + math | (unit tests only) | ✅ DONE — `ba69ce3` |
| 2 | Speed + hard gates | 1-5, 9 | Medium — touching job execution core |
| 3 | Tool items + recipes | 11 | Low — follows existing item/recipe patterns |
| 4 | Tool seeking + lifecycle | 6-8, 12 | **High** — new job steps, reservation complexity |
| 5 | Full bootstrap + polish | 10 | Medium — integration of everything, edge cases |

Contexts 1-3 can likely be done in quick succession. Context 4 is the most complex and may need the most back-and-forth. Context 5 is where things either come together or reveal hidden issues.

---

## End-to-End Test Stories

Player-facing scenarios that exercise the full tool system. Each test sets up a grid, movers, items, and workshops, then runs the simulation loop and checks the outcome. Written in the test-story style (expect failure first, fix code, verify green).

### Story 1: Mover digs soil bare-handed at 0.5x speed

> "I designate a dirt wall for mining. My mover has no tools. She should still dig it out, but it should take twice as long as baseline."

```
Setup: flat grid with one dirt wall at (5,3,0). Mover at (1,3,0), no equipped tool.
       toolRequirementsEnabled = true.
Action: create MINE designation at the dirt wall, run sim loop.
Expect: wall is removed after ~2x the base MINE_WORK_TIME ticks.
        Mover is idle afterward. Cell at (5,3,0) is air.
```

### Story 2: Mover with digging stick digs soil at 1.0x speed

> "Same setup, but the mover already carries a digging stick. Digging should take baseline time — noticeably faster than bare-handed."

```
Setup: flat grid, dirt wall at (5,3,0). Mover at (1,3,0).
       Spawn ITEM_DIGGING_STICK, set mover.equippedTool to it.
Action: create MINE designation, run sim loop.
Expect: wall removed in ~1x base MINE_WORK_TIME (half the ticks of Story 1).
        Mover still holds the digging stick after.
```

### Story 3: Mover cannot mine rock without hammer:2

> "I designate a stone wall for mining. My mover has no tools. The job should sit there unassigned — nobody can do it."

```
Setup: flat grid, stone wall at (5,3,0). Mover at (1,3,0), no tool.
Action: create MINE designation, run sim loop for 200 ticks.
Expect: wall is still there. Mover is idle (skipped the job).
        Job is still in the queue, unassigned.
```

### Story 4: Mover with stone hammer mines rock at 1.0x

> "Same rock wall, but now my mover has a stone hammer (hammering:2). She should walk over and mine it at baseline speed."

```
Setup: flat grid, stone wall at (5,3,0). Mover at (1,3,0).
       Spawn ITEM_STONE_HAMMER, set mover.equippedTool to it.
Action: create MINE designation, run sim loop.
Expect: wall removed in ~1x base MINE_WORK_TIME. Mover idle, still holds hammer.
```

### Story 5: Mover cannot chop tree without cutting:1

> "I designate a tree for chopping. Mover has no tools. The job should be skipped — you can't chop a tree bare-handed."

```
Setup: flat grid with a tree at (5,3). Mover at (1,3,0), no tool.
Action: create CHOP designation, run sim loop for 200 ticks.
Expect: tree still standing. Mover idle. Job unassigned.
```

### Story 6: Mover seeks out a tool before starting a hard-gated job

> "I designate a tree for chopping. There's a sharp stone on the ground nearby. The mover should walk to the sharp stone, pick it up, then go chop the tree."

```
Setup: flat grid, tree at (7,3). Mover at (1,3,0), no tool.
       Spawn ITEM_SHARP_STONE at (3,3,0) on the ground.
Action: create CHOP designation, run sim loop.
Expect: mover walks to (3,3) first, picks up sharp stone (equippedTool set),
        then walks to tree, chops it. Tree removed. Mover holds sharp stone.
```

### Story 7: Mover keeps tool across consecutive jobs

> "My mover has a sharp stone and chops a tree. Then I designate another tree. She should go chop it immediately — no detour to pick up another tool."

```
Setup: flat grid, two trees at (5,3) and (7,3). Mover at (1,3,0) with sharp stone equipped.
Action: create CHOP designations for both trees, run sim loop.
Expect: both trees chopped. Mover never dropped the sharp stone.
        No tool-seeking detour between jobs (track mover path or tick count).
```

### Story 8: Mover drops current tool when a different quality is needed

> "My mover has a sharp stone (cutting:1) and I designate a stone wall for mining. She needs hammering:2 and there's a stone hammer on the ground. She should drop the sharp stone and pick up the hammer."

```
Setup: flat grid, stone wall at (7,3,0). Mover at (1,3,0) with sharp stone equipped.
       Spawn ITEM_STONE_HAMMER at (3,3,0).
Action: create MINE designation, run sim loop.
Expect: sharp stone dropped near mover's position (on ground, unreserved).
        Mover picks up stone hammer, mines the rock wall. Wall gone.
        Mover holds stone hammer. Sharp stone is on ground somewhere.
```

### Story 9: Tool toggle disables all tool requirements

> "I toggle off toolRequirementsEnabled. Now movers should mine rock, chop trees, and do everything at 1.0x with no tools at all."

```
Setup: flat grid, stone wall at (5,3,0), tree at (7,3).
       Mover at (1,3,0), no tool. toolRequirementsEnabled = false.
Action: create MINE designation + CHOP designation, run sim loop.
Expect: both wall and tree removed at ~1x speed. No tool seeking.
        Mover idle at end, equippedTool still -1.
```

### Story 10: Full bootstrap — knap, craft digging stick, dig soil

> "Starting from nothing: mover knaps a sharp stone at a rock wall, then crafts a digging stick at a workshop, then digs a dirt wall. The complete early-game loop."

```
Setup: flat grid. Stone wall at (3,3,0) for knapping. Dirt wall at (8,3,0) to dig.
       Workshop (campfire) at (5,1,0) with digging stick bill queued.
       ITEM_ROCK on ground at (2,3,0). ITEM_STICKS on ground at (5,2,0).
       Mover at (1,3,0), no tool.
Action: create KNAP designation at stone wall, run sim loop until sharp stone exists.
        Then let crafting job auto-assign (digging stick recipe needs cutting:1).
        Then create MINE designation at dirt wall.
Expect: 1. Sharp stone spawned after knapping.
        2. Mover picks up sharp stone (equippedTool = sharp stone, cutting:1).
        3. Mover crafts digging stick at workshop (sharp stone is the tool, sticks consumed).
        4. Mover equips digging stick (or keeps sharp stone — depends on next job).
        5. Dirt wall mined out. Cell is air.
This is the most complex test — validates the entire progression chain end to end.
```

### Story 11: Rock (hammer:1) helps with building but not rock mining

> "My mover picks up a rock. She can build a wall at 1.0x speed (hammer:1 meets soft building requirement). But she still can't mine rock (needs hammer:2)."

```
Setup: flat grid. Mover at (1,3,0).
       ITEM_ROCK at (2,3,0). Stone wall at (7,3,0). Building materials available.
       Build designation at (5,3,0), Mine designation at (7,3,0).
Action: run sim loop.
Expect: mover picks up rock, builds wall at (5,3,0) at 1.0x speed.
        Mine job at (7,3,0) stays unassigned — rock's hammer:1 < required hammer:2.
        Stone wall still there.
```

### Story 12: Dead mover drops equipped tool

> "A mover carrying a stone pick starves to death. The pick should drop on the ground at her position, unreserved and haulable."

```
Setup: flat grid. Mover at (3,3,0) with stone pick equipped.
       hunger = 0, starvationTimer near death threshold. Survival mode.
Action: run sim loop until mover dies.
Expect: mover removed/dead. Stone pick is on the ground at ~(3,3,0).
        items[pickIdx].state == ITEM_ON_GROUND.
        items[pickIdx].reservedBy == -1. Pick is haulable.
```
