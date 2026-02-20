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
- **Contention**: Tools use the existing item reservation system (`reservedBy`). If a tool is already reserved, the mover skips it and searches for another. If none available, proceed bare-handed (0.5x).

### 3. No durability — intentionally deferred

Tools are **permanent** for now. No wear, no breakage. This keeps the initial implementation simple and avoids adding a material sink before the crafting economy is fleshed out. Durability can be added in a later phase once the base tool system is stable and there are enough recipes to make replacement feel fair rather than tedious.

### 4. Speed formula only covers designation jobs — what about craft/build jobs?

The doc maps qualities to MINE/CHANNEL/DIG_RAMP/CHOP, but what about:
- **JOBTYPE_CRAFT**: Workshop crafting has `workRequired` on recipes. Does a sharp stone speed up crafting at a stonecutter? Does QUALITY_FINE affect carpentry recipes?
- **JOBTYPE_BUILD**: Construction has a work duration. Does a stone hammer speed up building walls?
- **JOBTYPE_CHOP_FELLED**: Chopping felled trunks — does cutting quality apply?
- **JOBTYPE_GATHER_***: Gathering grass, saplings, berries — any quality check?

The full job-type-to-quality mapping needs to be spelled out for every existing job type, not just the digging-related ones.

### 5. ITEM_ROCK having QUALITY_HAMMERING:1 undercuts progression

The doc proposes rocks get hammering:1. But rocks are everywhere from minute zero — every mover can just pick one up and mine at 1.0x immediately. That makes the "bare hands = 0.5x" penalty meaningless for mining, since rocks are the most common item in the game. The stone hammer (crafted, requires cordage chain) becomes pointless at level 1.

Options:
- (a) Rocks get NO quality (they're raw material, not tools)
- (b) Rocks get hammering:1 but the bare-hands penalty doesn't apply to mining (only to cutting/digging)
- (c) Accept that mining is easy from the start, progression is about *speed* (1.0x → 1.5x → 2.0x)

This directly affects how the early game feels.

### 6. No interaction with the existing mover capabilities system

Movers already have a `Capabilities` struct (`canHaul`, `canMine`, `canBuild`, `canPlant`). How does tool quality interact?
- Can a mover with `canMine=false` mine if they have a pick? (Probably not — capabilities are role restrictions, tools are speed modifiers)
- Does `canMine=true` but no tool mean 0.5x speed?
- Should capabilities be mentioned in this doc to avoid confusion during implementation?

Needs at least a sentence clarifying that capabilities and tool quality are orthogonal systems.

### 7. Stockpile and hauling behavior for tools

- Are tools hauled to stockpiles like normal items? If so, movers will haul away tools that other movers left on the ground after finishing a job.
- Should tools have a special reservation state ("in use as equipment") that prevents hauling?
- The doc proposes IF_TOOL + IF_STACKABLE together with max stack 5. But if movers need to grab individual tools, stacking means a mover grabs a stack of 5 digging sticks when they only need 1. Should tools be non-stackable, or should tool-seeking use SplitStack?
- Is there a dedicated "tool" stockpile filter, or do tools go in a general category?

### 8. Knapping spot workshop overlaps with existing knapping designation

Knapping sharp stones already works as a designation (DESIGNATION_KNAP → JOBTYPE_KNAP: walk to stone wall, knap). Adding WORKSHOP_KNAPPING_SPOT creates two ways to make sharp stones:
- (a) Keep both — designation for sharp stones at walls, workshop for crafting composite tools (axe, pick, etc.)
- (b) Remove the designation, move all knapping to the workshop
- (c) Keep the designation but don't add a "sharp stone" recipe to the workshop (avoid redundancy)

Also: the existing knapping designation requires a *stone wall* to knap against. A 1x1 workshop on flat ground is thematically different. Should the knapping spot require placement adjacent to a stone wall? Or is it a freestanding workbench?

---

## Where the Digging Stick Fits

### Item Definition

| Property | Value |
|----------|-------|
| Name | Digging Stick |
| Recipe | 1x STICKS + cutting:1 (use sharp stone to whittle point) |
| Qualities | QUALITY_DIGGING: 1 |
| Flags | IF_TOOL, IF_STACKABLE |
| Max stack | 5 |
| Weight | 1.5 kg |
| Workshop | Knapping spot (1x1) or hand-craft |

### Role in Progression

```
Bare hands         → 0.5x speed for everything
Sharp stone        → cutting:1, fine:1       (1x cutting speed)
Digging stick      → digging:1               (1x digging speed)
Stone axe          → cutting:2, hammering:1  (1.5x cutting)
Stone pick         → digging:2, hammering:1  (1.5x digging)
Stone hammer       → hammering:2             (1.5x hammering)
```

The digging stick is the **digging equivalent of the sharp stone** — cheapest possible tool for its quality type. It bridges bare-handed digging (0.5x) to normal speed (1x).

### What It Speeds Up

| Job Type | Quality Checked | Bare Hands | Digging Stick (dig:1) | Stone Pick (dig:2) |
|----------|----------------|------------|----------------------|-------------------|
| MINE | HAMMERING or DIGGING | 0.5x | 1.0x | 1.5x |
| CHANNEL | DIGGING | 0.5x | 1.0x | 1.5x |
| DIG_RAMP | DIGGING | 0.5x | 1.0x | 1.5x |
| Future: TILL | DIGGING | 0.5x | 1.0x | 1.5x |

### Bootstrap Loop

```
1. Gather sticks (bare-handed, slow)
2. Harvest grass → dry → twist → braid → CORDAGE
3. Knap rocks at stone wall → SHARP_STONE (cutting:1)
4. Craft digging stick: STICKS + CORDAGE (needs cutting:1 from sharp stone)
   → Now digging is 1x instead of 0.5x
5. Craft stone pick: ROCK + STICKS + CORDAGE
   → Now digging is 1.5x
6. Mine faster → more stone → more tools → build faster
```

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
    QUALITY_CUTTING,    // chopping, harvesting, butchering
    QUALITY_HAMMERING,  // mining, construction, stonecutting
    QUALITY_DIGGING,    // mining, channeling, farming
    QUALITY_SAWING,     // sawmill work
    QUALITY_FINE,       // crafting, carpentry
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

**Quality assignments for existing items**:
```
ITEM_ROCK:         { QUALITY_HAMMERING, 1 }
ITEM_SHARP_STONE:  { QUALITY_CUTTING, 1 }, { QUALITY_FINE, 1 }
ITEM_STICKS:       (no tool quality)
```

**Speed formula** (from feature-04 doc):
```c
float speedMultiplier = 0.5f + 0.5f * toolLevel;
// Level 0 (bare hands): 2x slower
// Level 1 (crude):      baseline
// Level 2 (good):       1.5x faster
// Level 3 (excellent):  2x faster
float effectiveWorkTime = baseWorkTime / speedMultiplier;
```

### Phase 2: Tool Items (~0.5 session)

Add 4 new item types (save version bump already done in Phase 1):

| Item | Recipe | Qualities |
|------|--------|-----------|
| ITEM_DIGGING_STICK | 1x STICKS + 1x CORDAGE | digging:1 |
| ITEM_STONE_AXE | 1x ROCK + 1x STICKS + 1x CORDAGE | cutting:2, hammering:1 |
| ITEM_STONE_PICK | 1x ROCK + 1x STICKS + 1x CORDAGE | digging:2, hammering:1 |
| ITEM_STONE_HAMMER | 1x ROCK + 1x STICKS + 1x CORDAGE | hammering:2 |

All require QUALITY_CUTTING >= 1 to craft (need sharp stone or better).

**Recipe struct change**: Add `QualityType requiredQuality; int requiredQualityLevel;` to Recipe. The crafter (or a nearby stockpile item) must provide the required quality level. This is a new concept — current recipes only check input items and fuel, not tool quality.

**Files**: items.h (enum), item_defs.c (definitions), stockpiles (filter keys), save_migrations.h (item count update), tooltips (show qualities), workshops.h (Recipe struct)

### Phase 3: Knapping Spot Workshop + Recipes (~0.5 session)

Add WORKSHOP_KNAPPING_SPOT as 1x1 workshop (template: just `X`).

Recipes:
- Sharp Stone: 2x ROCK → 1x SHARP_STONE (already exists as designation, add as workshop recipe too)
- Digging Stick: 1x STICKS + 1x CORDAGE → 1x DIGGING_STICK (requires cutting:1 nearby)
- Stone Axe: 1x ROCK + 1x STICKS + 1x CORDAGE → 1x STONE_AXE (requires cutting:1)
- Stone Pick: 1x ROCK + 1x STICKS + 1x CORDAGE → 1x STONE_PICK (requires cutting:1)
- Stone Hammer: 1x ROCK + 1x STICKS + 1x CORDAGE → 1x STONE_HAMMER (requires cutting:1)

### Phase 4: Tool Seeking (~0.5 session)

When assigned a job, mover checks:
1. Does my equipped tool have the right quality? → proceed
2. Is there a better tool in a nearby stockpile? → detour to pick it up
3. No tool available → proceed bare-handed (0.5x speed)

Tool-seeking is opportunistic, not mandatory. Movers never refuse work.

---

## What This Enables

Once the quality system + digging stick exist:

- **Every existing job immediately benefits** — mining, chopping, building, gathering all get tool-scaled speed
- **Feature 05 (Hunting)**: butchering checks QUALITY_CUTTING — sharp stone works, stone knife is better
- **Feature 06 (Farming)**: tilling checks QUALITY_DIGGING — digging stick is the minimum tool, stone pick is faster
- **Progression feel**: day 1 is slow (bare hands), day 2 has crude tools (1x), day 3+ has good tools (1.5x)
- **Material chain payoff**: the cordage chain (grass→dry→twist→braid) finally leads to something beyond construction

---

## Estimated Scope

| Phase | Effort | Save bump? | New files? |
|-------|--------|-----------|-----------|
| Phase 1: Quality framework | ~1 session | Yes (v65) | tool_quality.h/c |
| Phase 2: Tool items | ~0.5 session | No (same bump) | — |
| Phase 3: Knapping spot recipes | ~0.5 session | No | — |
| Phase 4: Tool seeking | ~0.5 session | No | — |
| **Total** | **~2.5 sessions** | **1 bump** | **1 new module** |

Test expectations: ~80-120 new assertions covering speed multiplier math, quality lookups, tool item creation, knapping recipes, tool-seeking behavior, sandbox vs survival speed policy, and save migration.
