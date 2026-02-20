# Workshop Construction Costs

> Workshops shouldn't materialize from thin air. Building one should cost materials, time, and sometimes tools.

---

## The Problem

Right now workshops are placed instantly for free. Click "rope maker" and it exists. This is fine for sandbox but breaks survival immersion — a drying rack is a physical object made of wood, and a stonecutter is a heavy piece of equipment. They should cost something to build.

This also affects the survival bootstrap. The primitive shelter plan (`primitive-shelter-and-doors.md`) assumes the survivor can get cordage on day 1. Cordage needs a drying rack + rope maker. If those workshops suddenly cost logs and planks, the day-1 timeline shifts significantly.

---

## Design

### Construction via Blueprint (same as walls/floors)

Workshops should use the **existing blueprint/construction system**. When the player places a workshop:

1. A blueprint appears (ghost outline, like wall blueprints)
2. Movers haul required materials to the blueprint site
3. A mover performs the build action (takes time)
4. Workshop activates when build completes

This reuses `JOBTYPE_HAUL_TO_BLUEPRINT` + `JOBTYPE_BUILD` — no new job types needed. The workshop just doesn't activate until construction finishes.

### Material Costs Per Workshop

Costs should reflect what the workshop physically *is*:

| Workshop | Materials | Build Time | Tool Quality | Rationale |
|----------|-----------|-----------|-------------|-----------|
| **Campfire** | 5x STICKS | 1s | None | Pile of sticks, anyone can do it |
| **Drying Rack** | 3x STICKS + 1x CORDAGE | 2s | None | Simple frame with cordage ties |
| **Charcoal Pit** | 2x LOG | 3s | None | Dig a pit, stack logs — primitive |
| **Rope Maker** | 3x STICKS + 1x CORDAGE | 2s | None | Simple spinning frame |
| **Knapping Spot** | None (designation) | 0s | None | Just a flat rock, no construction |
| **Hearth** | 5x ROCK | 4s | HAMMERING:1 | Stone fire pit, needs shaping |
| **Stonecutter** | 5x ROCK + 2x LOG | 5s | HAMMERING:1 | Heavy stone workbench |
| **Sawmill** | 3x LOG + 2x CORDAGE | 5s | CUTTING:1 | Log frame with saw mechanism |
| **Kiln** | 8x ROCK + 2x CLAY | 6s | HAMMERING:1 | Enclosed stone oven |
| **Carpenter** | 4x PLANKS + 2x CORDAGE | 4s | CUTTING:1 | Proper workbench, needs saw |

### Key Design Choices

**Campfire is almost free.** 5 sticks, 1 second. A naked survivor can build a campfire in the first minutes. This is critical — fire is the first survival tool. Don't gate it behind anything.

**Drying rack and rope maker cost cordage.** This creates a chicken-and-egg problem (see Bootstrap section below).

**Knapping spot costs nothing.** It's a designation on a flat rock, not a structure. This keeps the sharp stone accessible with zero infrastructure.

**Tool quality requirements are optional.** Without tools, the mover builds slower (0.5x via the tool quality speed formula). With tools, they build at normal or faster speed. Tools don't *gate* workshop construction — they speed it up. This means:
- Campfire, drying rack, charcoal pit, rope maker: buildable bare-handed at normal speed
- Hearth, stonecutter, sawmill, kiln, carpenter: buildable bare-handed at 0.5x speed, or with tools at full speed

---

## The Bootstrap Problem

The cordage chain is:

```
Gather grass → Dry at DRYING RACK → Twist at ROPE MAKER → Cordage
```

But the drying rack costs 1x cordage, and the rope maker costs 1x cordage. So you need 2 cordage before you can make cordage. That's a dead loop.

### Solutions

**Option A: First workshops are free (cordage-less variants)**

The drying rack and rope maker have a "primitive" version that costs only sticks (no cordage):
- Primitive Drying Rack: 4x STICKS (no cordage) — slower passive time (6s vs 4s)
- Primitive Rope Maker: 4x STICKS (no cordage) — slower craft time (2x vs 1.6x)

Later the player can upgrade to the proper version with cordage for faster processing. This adds item/recipe complexity but solves the bootstrap cleanly.

**Option B: Worldgen dried grass + hand-twisting**

Dried grass already spawns in worldgen (~0.5% on high dry ground). The survivor picks up dried grass, and there's a hand-craft recipe (at knapping spot or bare-handed) to twist it into short string. No workshop needed for the first string. Then:
- Hand-twist: 4x DRIED_GRASS → 2x SHORT_STRING (slow, no workshop)
- Hand-braid: 3x SHORT_STRING → 1x CORDAGE (slow, no workshop)

This gives enough cordage to bootstrap the drying rack + rope maker, after which the proper workshop chain takes over.

**Option C: Remove cordage cost from first workshops**

Drying rack: 4x STICKS (no cordage). Rope maker: 4x STICKS (no cordage). Simple, no new recipes, no chicken-and-egg. The cordage cost only applies to later workshops that are further up the tech tree.

**Option D: One free workshop**

The first drying rack and first rope maker are free (or spawn as part of worldgen at a "starting camp" site). After that, replacements cost materials.

**Recommendation**: Option C is simplest. The drying rack and rope maker are primitive objects — a frame of sticks. They don't need cordage to hold together (just lean the sticks). Save the cordage requirement for more advanced workshops like the sawmill and carpenter. This keeps the bootstrap clean with zero new mechanics.

If Option C feels too cheap, Option B is the next best — hand-crafting adds one new recipe but uses existing infrastructure (knapping spot or bare-handed crafting).

---

## Implementation

### Workshop Build Cost Data

Add to `WorkshopDef`:

```c
typedef struct {
    ItemType itemType;
    int count;
} WorkshopBuildInput;

#define MAX_WORKSHOP_BUILD_INPUTS 3

typedef struct {
    // ... existing fields ...
    WorkshopBuildInput buildInputs[MAX_WORKSHOP_BUILD_INPUTS];
    int buildInputCount;
    float buildTime;           // game-hours
    // QualityType requiredQuality;  // future: tool quality for speed bonus
} WorkshopDef;
```

### Workshop Placement Flow (changed)

Currently: player clicks → `CreateWorkshop()` → workshop is active immediately.

New flow:
1. Player clicks placement → `CreateWorkshopBlueprint()` (new function)
2. Blueprint stores: workshop type, position, build inputs needed
3. System creates `JOBTYPE_HAUL_TO_BLUEPRINT` jobs for each input
4. When all materials delivered, creates `JOBTYPE_BUILD` job
5. Build job completes → `ActivateWorkshop()` — workshop becomes functional

This mirrors the existing wall/floor construction flow exactly. The workshop exists in a "blueprint" state (ghost rendering, not functional) until built.

### Workshop Struct Changes

```c
typedef struct {
    // ... existing fields ...
    bool constructed;           // false = blueprint, true = active
    // Delivery tracking (same pattern as construction blueprints)
    int deliveredCounts[MAX_WORKSHOP_BUILD_INPUTS];
} Workshop;
```

### Rendering

- Blueprint state: render workshop template as ghost/transparent overlay (same as construction blueprints)
- Under construction: show partially built (optional, could just use ghost until done)
- Complete: normal rendering

### Save Version

- Save version bump for new Workshop fields (`constructed`, `deliveredCounts`)
- Migration: existing saves set `constructed = true` for all workshops (backward compatible)

---

## Interaction With Workshop Evolution Plan

From `workshop-evolution-plan.md`: the current template system stays for primitive-era workshops, and stations come later for freeform industrial stuff.

Workshop construction costs fit cleanly into the template system:
- Template workshops have build costs defined in `WorkshopDef` (data-driven)
- When stations arrive (Phase 2 of evolution plan), each station-as-furniture-item would have its own construction recipe in the construction system (like beds/chairs already do)
- The blueprint flow works for both — templates use workshop blueprints, stations use furniture blueprints

### Station Foreshadowing

The workshop-evolution-plan notes that some workshops are naturally "stations" (single-tile furniture):
- Drying rack → station (it's just furniture with a timer)
- Anvil → station
- Furnace → station

When these become stations, their "construction cost" is just the item cost of the furniture piece (like ITEM_PLANK_BED costs planks at the carpenter). The workshop build cost system bridges the gap — it gives workshops material costs now, and the data translates naturally to furniture crafting costs later.

---

## Effect on Survival Bootstrap

With Option C (no cordage on primitive workshops):

```
Minute 1-5:   Gather sticks (scattered on ground)
Minute 5:     Build campfire (5 sticks, 1s)          → warmth!
Minute 6:     Build drying rack (4 sticks, 2s)        → can dry grass/berries
Minute 7:     Build rope maker (4 sticks, 2s)         → can make cordage
Minute 8-15:  Gather grass, dry it, twist, braid      → cordage production online
Minute 15-30: Build primitive shelter (sticks + cordage + poles + leaves)
```

The first 7 minutes are just picking up sticks. That's intentional — sticks are the universal bootstrap material. Everything primitive is made of sticks.

---

## Costs Summary

| Workshop | Cost (Option C) | Era | Notes |
|----------|----------------|-----|-------|
| Knapping Spot | Free (designation) | Stone Age | Just a flat rock |
| Campfire | 5x STICKS | Stone Age | Pile of sticks |
| Drying Rack | 4x STICKS | Stone Age | Simple frame |
| Rope Maker | 4x STICKS | Stone Age | Simple spinning frame |
| Charcoal Pit | 2x LOG | Stone Age+ | Needs tree chopping |
| Hearth | 5x ROCK | Stone Age+ | Stone fire pit |
| Stonecutter | 5x ROCK + 2x LOG | Colony | Heavy equipment |
| Sawmill | 3x LOG + 2x CORDAGE | Colony | Needs cordage |
| Kiln | 8x ROCK + 2x CLAY | Colony | Enclosed oven |
| Carpenter | 4x PLANKS + 2x CORDAGE | Colony | Needs sawmill first |

Stone Age workshops (first 4) cost only sticks or nothing — achievable in the first minutes. Colony-era workshops need processed materials and represent real investment.
