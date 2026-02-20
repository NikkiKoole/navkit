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

## Codebase Analysis & UI Integration

### Current State: Two Placement Paths

The codebase has two separate UI paths for placing things:

**1. MODE_DRAW (D key) — Sandbox cheat placement**
- `ACTION_DRAW_WORKSHOP` (T key) → category with children like `ACTION_DRAW_WORKSHOP_STONECUTTER`, `ACTION_DRAW_WORKSHOP_CAMPFIRE`, etc.
- Handler: `ExecutePlaceWorkshop()` in `src/core/input.c` — calls `CreateWorkshop()` directly, instant, free
- **Keep this as-is.** It's the sandbox cheat tool.
- Bug: `ExecutePlaceWorkshop()` hardcodes 3x3 validation but campfire is 2x1. Should use `workshopDefs[type].width/height`.

**2. MODE_WORK (W key) → SUBMODE_BUILD (B key) — Real construction**
- Currently has: Wall (W), Floor (F), Ladder (L), Ramp (R), Furniture (U)
- Uses `ConstructionRecipe` system with blueprints, haul jobs, and build jobs
- Recipe selection via R key cycling (e.g. `selectedWallRecipe` cycles through `BUILD_WALL` recipes)
- **Workshop placement goes HERE** — new category action alongside the existing ones

### Action Registry Changes

New entries in `src/core/input_mode.h` (enum) and `src/core/action_registry.c` (registry):

```c
// New InputAction enum values:
ACTION_BUILD_WORKSHOP,                    // Category (like ACTION_DRAW_WORKSHOP)
ACTION_BUILD_WORKSHOP_CAMPFIRE,           // Children
ACTION_BUILD_WORKSHOP_DRYING_RACK,
ACTION_BUILD_WORKSHOP_ROPE_MAKER,
ACTION_BUILD_WORKSHOP_CHARCOAL_PIT,
ACTION_BUILD_WORKSHOP_HEARTH,
ACTION_BUILD_WORKSHOP_STONECUTTER,
ACTION_BUILD_WORKSHOP_SAWMILL,
ACTION_BUILD_WORKSHOP_KILN,
ACTION_BUILD_WORKSHOP_CARPENTER,
// ... one per WorkshopType that has buildInputCount > 0
// Knapping spot excluded (it's a free designation, not a built workshop)
```

Registry entries follow the existing pattern:
```c
// Category action under Work → Build
{
    .action = ACTION_BUILD_WORKSHOP,
    .name = "WORKSHOP",
    .barDisplayText = "workshop(T)",
    .barKey = 't',
    .requiredMode = MODE_WORK,
    .requiredSubMode = SUBMODE_BUILD,
    .parentAction = ACTION_NONE,  // Top-level in build submode
    .canDrag = false,
    .canErase = false
},
// Child action
{
    .action = ACTION_BUILD_WORKSHOP_CAMPFIRE,
    .name = "CAMPFIRE",
    .barDisplayText = "Campfire",
    .barKey = 'c',
    .requiredMode = MODE_WORK,
    .requiredSubMode = SUBMODE_BUILD,
    .parentAction = ACTION_BUILD_WORKSHOP,
    .canDrag = false,    // Single-click placement, not drag
    .canErase = true     // Right-click to cancel blueprint
},
// ... same pattern for each workshop type
```

This creates a submenu: **Work (W) → Build (B) → Workshop (T) → Campfire (C) / Drying Rack (D) / ...**

Same depth as the Draw mode workshop menu. Without this, the Build menu would have Wall + Floor + Ladder + Ramp + Furniture + 9 workshop types all flat — far too crowded.

### Placement Handler

New function in `src/core/input.c`:

```c
static void ExecutePlaceWorkshopBlueprint(int x, int y, int z, WorkshopType type) {
    const WorkshopDef* def = &workshopDefs[type];
    
    // Validate footprint using def->width/height (NOT hardcoded 3x3)
    for (int dy = 0; dy < def->height; dy++) {
        for (int dx = 0; dx < def->width; dx++) {
            int cx = x + dx, cy = y + dy;
            if (!InBounds(cx, cy, z)) return;
            if (!IsCellWalkableAt(z, cy, cx)) return;
            if (FindWorkshopAt(cx, cy, z) >= 0) return;
        }
    }
    
    // Create workshop in blueprint state
    int idx = CreateWorkshopBlueprint(x, y, z, type);
    // Job system picks up haul/build jobs automatically via AssignJobs
}
```

Input handling switch case (alongside existing build actions):
```c
case ACTION_BUILD_WORKSHOP_CAMPFIRE:
    if (leftClick) ExecutePlaceWorkshopBlueprint(dragStartX, dragStartY, z, WORKSHOP_CAMPFIRE);
    else ExecuteCancelWorkshopBlueprint(dragStartX, dragStartY, z);
    break;
// ... same for each workshop type
```

### Workshop Build Cost Data

Add to `WorkshopDef` in `src/entities/workshops.h`:

```c
typedef struct {
    ItemType itemType;
    int count;
} WorkshopBuildInput;

#define MAX_WORKSHOP_BUILD_INPUTS 3

// Add to existing WorkshopDef struct:
WorkshopBuildInput buildInputs[MAX_WORKSHOP_BUILD_INPUTS];
int buildInputCount;     // 0 = free (knapping spot), >0 = needs construction
float buildTime;         // game-hours (converted via GameHoursToGameSeconds)
```

### Workshop Blueprint State

Add to `Workshop` struct in `src/entities/workshops.h`:

```c
// Add to existing Workshop struct:
bool constructed;                              // false = blueprint, true = active
int deliveredCounts[MAX_WORKSHOP_BUILD_INPUTS]; // How many of each input delivered
int assignedBuilder;                           // Mover doing the build (-1 = none)
float buildProgress;                           // 0.0 to 1.0
```

### New Functions in `src/entities/workshops.c`

```c
int CreateWorkshopBlueprint(int x, int y, int z, WorkshopType type);
// Like CreateWorkshop but sets constructed=false, doesn't set CELL_FLAG_WORKSHOP_BLOCK yet

void ActivateWorkshop(int workshopIdx);
// Sets constructed=true, applies CELL_FLAG_WORKSHOP_BLOCK, pushes movers out of machinery tiles

bool IsWorkshopBlueprint(int workshopIdx);
// Returns !workshops[workshopIdx].constructed

void CancelWorkshopBlueprint(int workshopIdx);
// Removes blueprint, drops delivered items, cleans up jobs
```

### Job Integration

Two approaches, from simplest to most reusable:

**Approach A: Reuse existing Blueprint system directly**
- Create a `Blueprint` (from `designations.h`) at the workshop's work tile
- Map `WorkshopBuildInput` → `ConstructionInput` equivalents
- The existing `JOBTYPE_HAUL_TO_BLUEPRINT` + `JOBTYPE_BUILD` flow handles everything
- On build complete, call `ActivateWorkshop()` instead of placing a wall/floor
- **Pro**: Zero new job types. **Con**: Blueprint system is cell-based (one cell), workshops span multiple cells.

**Approach B: Workshop-specific jobs**
- New `JOBTYPE_HAUL_TO_WORKSHOP` and `JOBTYPE_BUILD_WORKSHOP`
- WorkGiver scans for unconstructed workshops, assigns haul/build jobs
- More control over multi-cell footprint, delivery to specific tiles
- **Pro**: Cleaner separation. **Con**: New job types + WorkGiver code.

**Recommendation**: Approach A if we can make it work with the existing Blueprint at the workshop's work tile position. The haul destination is the work tile, and the build happens there. Multi-cell footprint doesn't matter for haul/build — movers walk to the work tile either way. Try Approach A first, fall back to B only if Blueprint assumptions break.

### Rendering

In `src/render/rendering.c`:
- Blueprint workshops: render template tiles with alpha ~0.4 (ghost), same tint as construction blueprints
- Check `workshops[i].constructed` — if false, use ghost rendering
- Show delivery progress in tooltip (e.g. "Campfire [3/5 sticks]")

### Save Version

- Bump `CURRENT_SAVE_VERSION` (62 → 63) in `src/core/save_migrations.h`
- Save new Workshop fields: `constructed`, `deliveredCounts[]`, `assignedBuilder`, `buildProgress`
- Migration: existing saves set `constructed = true` for all workshops (backward compatible)
- Update both `src/core/saveload.c` AND `src/core/inspect.c`

### Key Files Summary

| File | Changes |
|------|---------|
| `src/core/input_mode.h` | New `ACTION_BUILD_WORKSHOP*` enum values |
| `src/core/action_registry.c` | New registry entries for workshop build actions |
| `src/core/input.c` | `ExecutePlaceWorkshopBlueprint()`, case handlers |
| `src/entities/workshops.h` | `WorkshopBuildInput`, build cost fields on `WorkshopDef`, blueprint state on `Workshop` |
| `src/entities/workshops.c` | `CreateWorkshopBlueprint()`, `ActivateWorkshop()`, build cost data in `workshopDefs[]` |
| `src/render/rendering.c` | Ghost rendering for workshop blueprints |
| `src/render/tooltips.c` | Delivery progress display |
| `src/core/saveload.c` | Save version bump + new Workshop fields |
| `src/core/inspect.c` | Parallel save migration |
| `src/core/save_migrations.h` | Version constant |

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
