# Capability-Based Task Resolution

Unified system for workshop crafting AND daily life activities. Extends [workshop-evolution-plan.md](../../todo/workshop-evolution-plan.md) with a concrete capability model that covers both job-driven production and need-driven survival.

## Core Principle

**Everything is possible with the right capabilities nearby.** No workshop gates — only capability level gates. A campfire provides `heat_source:1`, which is enough to cook food but not smelt iron (`heat_source:3`). This is physics, not an arbitrary building requirement. Workshops still "win" in practice because they cluster capabilities in one spot, giving them better spatial scores.

---

## A. Capability System

### CapabilityType Enum

```c
typedef enum {
    CAP_HEAT_SOURCE,       // Campfire:1, Hearth:2, Kiln:3
    CAP_CUTTING_SURFACE,   // Ground+stone:0, Stonecutter:2
    CAP_STRIKING_SURFACE,  // Ground+rock:0, Anvil:2
    CAP_WATER_SOURCE,      // River cell:1, Well:2
    CAP_GRINDING,          // Quern:1, Millstone:2
    CAP_FLAT_SURFACE,      // Ground:0, Table:1, Workbench:2
    CAP_SEATING,           // Ground:0, Log:1, Chair:2
    CAP_SLEEPING,          // Ground:0, Leaf pile:1, Bed:2
    CAP_DRYING,            // Drying rack:1
    CAP_SPINNING,          // Rope maker:1, Spinning wheel:2
    CAP_WEAVING,           // Loom:1, Power loom:2
    CAP_TANNING,           // Tanning rack:1, Tannery:2
    CAP_CONTAINMENT,       // Pit:1, Charcoal pit:2, Kiln:3
    CAP_TYPE_COUNT
} CapabilityType;
```

### Levels 0-3

| Level | Meaning | Examples |
|-------|---------|---------|
| 0 | Ground / bare hands | Knapping on a rock, sleeping on dirt |
| 1 | Crude / improvised | Campfire, leaf pile, ground quern |
| 2 | Standard / purpose-built | Hearth, plank bed, workshop stonecutter |
| 3 | Superior / industrial | Kiln, forge with bellows, powered machinery |

### Providers

Capabilities are declared on **defs** (static data), not instances.

```c
typedef struct {
    CapabilityType type;
    int level;
} CapabilityEntry;

// On existing defs:
typedef struct {
    // ... existing WorkshopDef fields ...
    CapabilityEntry caps[MAX_CAPS_PER_DEF];
    int capCount;
} WorkshopDef;

typedef struct {
    // ... existing FurnitureDef fields ...
    CapabilityEntry caps[MAX_CAPS_PER_DEF];
    int capCount;
} FurnitureDef;

// Items-as-tools already have tool quality; capability is a superset:
// A stone axe provides CAP_CUTTING_SURFACE:1 when placed/held near work
```

**Implicit level-0 providers:** The world itself. Any walkable cell with a solid surface below provides `flat_surface:0`, `seating:0`, `sleeping:0`. A river cell provides `water_source:1`. These aren't stored — the resolver checks world state.

---

## B. Task Chains

A **TaskChainDef** is a static sequence of steps that together accomplish one goal. Like recipes, these are data — not runtime objects.

```c
typedef struct {
    CapabilityType requiredCap;   // What capability this step needs
    int requiredLevel;            // Minimum level
    ItemType inputItem;           // ITEM_NONE if no item consumed
    float workTime;               // Seconds of work at this step
} TaskStep;

typedef struct {
    const char* name;
    TaskStep steps[MAX_TASK_STEPS];
    int stepCount;
    ItemType outputItem;          // Final output (ITEM_NONE for needs)
    int outputCount;
} TaskChainDef;
```

### Examples

**"Saw Planks"** — 1 step, same as current recipe:
```
steps: [ { CAP_CUTTING_SURFACE, 2, ITEM_LOG, 3.0 } ]
output: ITEM_PLANKS x4
```

**"Cook Meat"** — 1 step:
```
steps: [ { CAP_HEAT_SOURCE, 1, ITEM_RAW_MEAT, 4.0 } ]
output: ITEM_COOKED_MEAT x1
```

**"Make Breakfast"** — 3 steps (daily life):
```
steps: [
    { CAP_HEAT_SOURCE, 1, ITEM_RAW_MEAT, 4.0 },  // Cook
    { CAP_SEATING, 1, ITEM_NONE, 0.0 },           // Sit down
    { CAP_FLAT_SURFACE, 0, ITEM_COOKED_MEAT, 2.0 } // Eat
]
output: ITEM_NONE (need satisfaction, not item)
```

Task chains are the unifying abstraction. A recipe IS a 1-step task chain. A need satisfaction IS a multi-step task chain. The difference is what triggers them.

---

## C. Two Drivers: Jobs vs Needs

Both use the same capability resolution mechanism but are fundamentally different:

### Job Driver (Colony Work)

```
Bill (player-created)
  → WorkGiver scans for available mover + capability cluster
  → Capability resolver finds best provider(s)
  → Creates Job with resolved positions
  → Mover executes via craft state machine
```

- Triggered by: player bills on workshops/workspaces
- Priority: normal work queue (current system)
- Quality matters for: output item quality, craft speed
- Interruptible by: needs at critical thresholds (existing behavior)

### Need Driver (Personal Survival)

```
Need threshold crossed (hunger < 0.3, energy < 0.3, thirst < 0.4)
  → Need system interrupts current job (or not, depending on severity)
  → Capability resolver finds nearest acceptable provider
  → Mover walks there and satisfies need
  → Resumes previous activity
```

- Triggered by: need value dropping below threshold (existing system)
- Priority: above normal work, below critical survival
- Quality matters for: satisfaction amount, mood effect
- Interrupts: jobs at normal threshold, ALL jobs at critical threshold

### What Changes from Current Needs

Currently (`needs.c`), each need has bespoke search functions: `FindNearestEdibleInStockpile`, `FindNearestFurnitureForRest`, `FindNearestDrinkable`, etc. Each reimplements spatial scoring.

With capabilities, these collapse into one resolver call:
- Hungry → resolve `CAP_FLAT_SURFACE:0` + edible item nearby
- Tired → resolve `CAP_SLEEPING:0` (ground) or `CAP_SLEEPING:1+` (furniture)
- Thirsty → resolve `CAP_WATER_SOURCE:1` or drinkable item
- Cold → resolve `CAP_HEAT_SOURCE:1`

The specialized search functions become thin wrappers over the general resolver, preserving the threshold/priority logic that's already solid.

---

## D. Spatial Resolution

### Scoring Formula

```c
float score = capLevel / (1.0f + dist / CELL_SIZE);
```

This is the same pattern already used in `needs.c` for rest and drink searches. Higher capability level is better, closer is better, and they trade off smoothly.

### Cluster Bonus

When a task chain has multiple steps, providers that are close to each other score better because the mover walks less total distance. This is why workshops naturally "win" — a hearth provides `heat_source:2` AND has a flat surface AND has an output tile, all within 3 tiles.

```c
// For multi-step chains, score = sum of per-step scores
// where distance for step N is measured from step N-1's provider
float totalScore = 0;
Point prevPos = moverPos;
for (int i = 0; i < chain->stepCount; i++) {
    Provider* best = FindBestProvider(chain->steps[i], prevPos);
    totalScore += best->capLevel / (1.0f + Distance(prevPos, best->pos) / CELL_SIZE);
    prevPos = best->pos;
}
```

### Need vs Job Resolution Strategy

- **Needs prefer urgency:** weight distance higher (nearest acceptable provider). A starving mover doesn't walk across the map for a better kitchen.
- **Jobs prefer quality:** weight capability level higher (best available provider). A crafter should use the best tools available, even if slightly farther.

This is a tuning knob on the scoring formula, not a separate system:
```c
// Need resolution: distance matters 2x more
float needScore = capLevel / (1.0f + 2.0f * dist / CELL_SIZE);

// Job resolution: standard weighting
float jobScore = capLevel / (1.0f + dist / CELL_SIZE);
```

---

## E. Quality Aggregation

### Per-Step Quality

Each step in a task chain produces a quality value derived from the capability level of the provider used:

```c
// 0.0 to 1.0 scale
float stepQuality = (float)actualCapLevel / MAX_CAP_LEVEL;  // e.g., level 2/3 = 0.67
```

### Chain Quality

Average of all contributing steps' quality values. For a 1-step recipe, chain quality = step quality.

### Job Quality Effects

- **Speed:** `speedMultiplier = 0.5f + 0.5f * chainQuality` — level 0 is half speed, level 3 is full speed. Meshes with existing `RunWorkProgress(dt, speedMultiplier)` pattern.
- **Output quality** (future): chain quality feeds into output item quality tier. A sword forged at a proper anvil (level 2) is better than one hammered on a rock (level 0).

### Need Quality Effects

- **Satisfaction:** `satisfactionMultiplier = 0.5f + 0.5f * chainQuality` — sleeping on dirt (level 0) restores half as much energy as a plank bed (level 2).
- **Mood** (future): higher quality need satisfaction gives mood buffs. Eating at a table vs eating on the ground.

---

## F. Workshop Compatibility Bridge

The capability system layers onto existing code without replacing it.

### Data-Only Additions

```c
// workshops.h — add to WorkshopDef
CapabilityEntry capabilities[MAX_CAPS_PER_DEF];
int capabilityCount;

// Example: Hearth provides heat_source:2 and flat_surface:1
workshopDefs[WORKSHOP_HEARTH].capabilities[0] = (CapabilityEntry){ CAP_HEAT_SOURCE, 2 };
workshopDefs[WORKSHOP_HEARTH].capabilities[1] = (CapabilityEntry){ CAP_FLAT_SURFACE, 1 };
workshopDefs[WORKSHOP_HEARTH].capabilityCount = 2;

// furniture.h — add to FurnitureDef
CapabilityEntry capabilities[MAX_CAPS_PER_DEF];
int capabilityCount;

// Example: Plank bed provides sleeping:2
furnitureDefs[FURNITURE_PLANK_BED].capabilities[0] = (CapabilityEntry){ CAP_SLEEPING, 2 };
furnitureDefs[FURNITURE_PLANK_BED].capabilityCount = 1;
```

### Recipe Compatibility

```c
// Recipe struct gains optional capability requirements
typedef struct {
    // ... existing fields (workshopType, inputs, outputs, fuel, etc.) ...
    CapabilityEntry requiredCaps[MAX_RECIPE_CAPS];  // New
    int requiredCapCount;                             // New, 0 = use workshopType
} Recipe;
```

**Rule:** if `requiredCapCount > 0`, use capability resolution. If `requiredCapCount == 0`, use existing `workshopType` matching. Old recipes need zero changes.

New recipes can use either system. This means we can migrate one recipe at a time, verify it works, and never break existing content.

---

## G. Daily Life Examples

### "Make Breakfast"

```
Trigger: hunger < 0.3 AND cooked food exists somewhere accessible
Chain:
  1. Find edible item (existing stockpile/ground search)
  2. If raw: resolve CAP_HEAT_SOURCE:1 → walk to campfire/hearth → cook (4s)
  3. Resolve CAP_SEATING:0 → walk to chair/ground → sit
  4. Eat (2s) → hunger restored by nutrition value × quality multiplier
```

Without capability system this is exactly what happens now, but steps 2-3 are hardcoded in `needs.c`. With capabilities, the "find a heat source" and "find a seat" parts use the general resolver, and adding a new cooking method (clay oven, solar cooker) just means declaring capabilities on a new def.

### "Sleep"

```
Trigger: energy < 0.3
Chain:
  1. Resolve CAP_SLEEPING → best of: plank bed (level 2), leaf pile (level 1), ground (level 0)
  2. Walk to provider
  3. Rest until energy > 0.8
  4. Satisfaction = restRate × qualityMultiplier
```

Currently: `FindNearestFurnitureForRest()` searches furniture array with `restRate / (1 + dist/CELL_SIZE)` scoring. This IS the capability pattern already — just not named as such. Migration is renaming, not rewriting.

### "Drink"

```
Trigger: thirst < 0.4
Chain:
  1. Resolve: drinkable item in stockpile/ground OR CAP_WATER_SOURCE:1 (river)
  2. Walk to provider
  3. Consume (item deleted or drink from river)
  4. Hydration restored × quality multiplier
```

Currently: `FindNearestDrinkable()` + `FindNearestNaturalWater()` with separate scoring. Capability system unifies these — a river cell implicitly provides `CAP_WATER_SOURCE:1`, a well provides `CAP_WATER_SOURCE:2`.

### "Socialize" (Future)

```
Trigger: social need < threshold (not yet implemented)
Chain:
  1. Resolve CAP_SEATING:1 near another idle mover
  2. Walk to seat
  3. Chat (timer, mood buff)
```

The capability resolver naturally finds seats near other movers because the scoring considers both the provider AND the mover's goal. This kind of emergent behavior falls out of the general system.

---

## H. Migration Path

### Phase A: Capability Declarations (Data Only)

Add `CapabilityEntry` arrays to `WorkshopDef` and `FurnitureDef`. Populate them for all existing types. No behavior changes — this is just tagging what already exists.

**Validation:** print capability tables at startup, verify they match intuitive expectations.

**Scope:** ~100 lines across `workshops.c`, `furniture.c`, new `capability.h`.

### Phase B: Resolver for One Recipe

Implement the spatial resolver. Pick one simple recipe (e.g., "cook meat" on campfire/hearth) and make it work via `requiredCaps` instead of `workshopType`. Keep the old path working — the recipe has both fields, capability takes priority when present.

**Validation:** cook meat at campfire (CAP_HEAT_SOURCE:1) and hearth (CAP_HEAT_SOURCE:2). Verify speed difference from quality. Verify old workshopType recipes still work unchanged.

**Scope:** `capability.c` (~200 lines resolver), recipe struct changes, one recipe migration.

### Phase C: Needs Use Capability Resolution

Replace the bespoke search functions in `needs.c` with resolver calls:
- `FindNearestFurnitureForRest` → resolve `CAP_SLEEPING`
- `FindNearestDrinkable` / `FindNearestNaturalWater` → resolve `CAP_WATER_SOURCE` + drinkable items
- Warmth search → resolve `CAP_HEAT_SOURCE`

**Validation:** all existing need tests pass with new resolver. Behavior should be identical — the scoring formula is already the same pattern.

**Scope:** refactor `needs.c` search functions (~150 lines changed), add world-implicit capability checks.

### Phase D: New Recipes Use Capabilities

New crafting content written with `requiredCaps` instead of `workshopType`. Existing recipes can be migrated one at a time or left alone — both paths work.

**Scope:** ongoing, per-recipe.

### Phase E: Freeform Station Furniture

This is Phase 2 from `workshop-evolution-plan.md`. Single-tile station entities (anvil, furnace, saw blade) that declare capabilities and cluster spatially to form ad-hoc workspaces.

**Scope:** large feature, separate design doc when Phase D is stable.

---

## Relationship to Existing Systems

| System | Current | With Capabilities |
|--------|---------|-------------------|
| Workshop recipes | `workshopType` match | `requiredCaps[]` match (or workshopType fallback) |
| Need satisfaction | Bespoke per-need search functions | General resolver with need-specific parameters |
| Tool quality | `GetJobToolSpeedMultiplier()` | Folds into capability level (tool provides cap level) |
| Furniture rest | `restRate / (1 + dist)` scoring | Same formula, generalized to all capabilities |
| Passive workshops | Timer on workshop entity | Timer on capability provider (same thing, different name) |

## What Does NOT Change

- **Bill system:** bills still live on workshops/workspaces, still drive job creation
- **Craft state machine:** 10-step sequence unchanged, just resolves positions differently
- **Need thresholds:** hunger/energy/thirst values and priority logic stay exactly as-is
- **Save format:** capability declarations are on defs (compile-time), not on instances (save data)
- **Event logging:** same `EventLog()` calls, just with capability info added to messages

---

## Design Risks & Mitigations

### Risk: New Player Legibility

The workshop system teaches players what's possible — build a Hearth, open its menu, see the recipes. The capability system is intuition-driven ("fire + meat = cooking") but risks the **CDDA wiki problem**: the game is deep but players bounce off it because they can't figure out what's possible without external help.

**Mitigations (actionable):**
- [ ] **Tools as discovery points.** Hovering any tool or capability provider shows the recipes it enables. Hover a saw: "Saw Planks, Saw Beams, Cut Firewood." Hover a campfire: "Cook Meat, Boil Water, Roast Roots." This works better than workshops for teaching because tools are things players pick up early and carry around — discovery happens sooner and more naturally than "build a building first, then click it."
- [ ] **Instant reward loop.** When a player crafts a new tool, hovering it immediately shows what new recipes they just unlocked. "You made a stone axe — now you can: Chop Tree, Split Kindling, Carve Bowl." The tool is both the reward and the teaser for what's next. Workshops can't do this because they're static once placed.
- [ ] **Combined view at stations.** When tools are placed at or near a station, hovering shows the *union* of what everything there can do together. Anvil alone: "Hammer Nails, Flatten Plate." Anvil next to forge: those PLUS "Forge Blade, Smelt Iron." This is where spatial clustering becomes visible — the player sees new recipes appear as they arrange their workspace.
- [ ] **Handle tool mobility.** Tools move around (carried, hauled, dropped), so the recipe list is position-dependent. The hover tooltip must recalculate based on what's *currently nearby*, not a static list. Show this clearly: "Recipes here (with nearby forge):" vs "Recipes (this tool alone):". If a tool gets carried away from a station, the combined recipes disappear from that station's tooltip — which teaches the player that layout matters.
- [ ] **Workshops as curated bundles.** Workshops still have value as pre-arranged capability clusters with a bill menu. Think of them as "starter kits" — a Hearth is a campfire + flat surface + output spot bundled into one placement. The bill menu is a curated subset of what the capabilities allow. New players use workshops, experienced players freeform.
- [ ] **Pinned tools.** Since tools move around (hauled, borrowed, dropped), players need a way to say "this saw stays at the carpenter station." A pin/lock action on a placed tool marks it as spatially reserved — movers skip pinned tools when looking for haulable items or crafting inputs elsewhere. Think of it as a stockpile filter but for individual tools at a location. If a mover takes a pinned tool for a job, they return it when done (like borrowing from a stockpile). This solves the mobility problem: freeform layout works because players can stabilize it once they're happy with the arrangement.
- [ ] **Unmet requirement feedback.** When a recipe fails capability check, tell the player exactly what's missing: "Needs heat_source:3, best available: campfire (heat_source:1)." Never just "can't craft."

### Risk: Level 0 Makes Workshops Feel Pointless

If a campfire + flat rock does everything a Hearth does (just slower), why build the Hearth? The capability system must make the quality/speed difference dramatic enough that upgrades feel like genuine relief, not minor optimization.

**Mitigations (actionable):**
- [ ] **Steep speed curve.** Level 0 = 50% speed, level 1 = 67%, level 2 = 83%, level 3 = 100%. Working at level 0 should feel *painful* — your crafter spends twice as long on every job. Players should actively want to upgrade.
- [ ] **Failure chance at low levels** (future). Level 0 has a chance to waste inputs. Knapping stone on the ground sometimes shatters it. This makes level 0 viable for emergencies but punishing for sustained use.
- [ ] **Quality tiers on output** (future). Level 0 produces "crude" items, level 2 produces "standard." A crude stone axe breaks faster. This cascades — bad tools make future crafting worse.
- [ ] **Need satisfaction scaling.** Sleeping on dirt (level 0) restores 50% as much energy per hour as a plank bed (level 2). Eating on the ground gives less satisfaction than eating at a table. Players feel the difference in how often movers interrupt work for needs.

### Risk: Invisible Cluster Scoring

The spatial puzzle ("put the anvil near the forge for faster crafting") is the system's best emergent feature — but only if players know it exists. Hidden scoring means hidden gameplay.

**Mitigations (actionable):**
- [ ] **Workspace efficiency overlay.** A debug/planning view that color-codes tiles by aggregate capability score. Bright = good workshop spot, dim = isolated. Toggle with a key in sandbox mode.
- [ ] **Crafter feedback.** When a crafter starts a multi-step job, brief floating text: "Good workspace (+15% speed)" or "Scattered tools (-20% speed)." Teaches spatial layout through play, not menus.
- [ ] **Proximity indicator on placement.** When placing furniture/stations, show which nearby capabilities it complements. Placing an anvil near a forge: "Near heat_source:2 — enables iron working." This turns placement into an informed decision.

### Risk: Resolver Performance (Implicit Level-0 Everywhere)

If every walkable tile is `flat_surface:0` and every river cell is `water_source:1`, the resolver could score thousands of candidates per query.

**Mitigations (actionable):**
- [ ] **Explicit-first search.** Only check implicit (world-cell) providers if no explicit provider (workshop, furniture, placed item) found within search radius. Most of the time, explicit providers exist.
- [ ] **Bounded search radius.** Needs use a tight radius (16 tiles), jobs use a wider one (32 tiles). Don't scan the whole map.
- [ ] **Provider registry.** Maintain a spatial index of placed capability providers (workshops + furniture). This is a small list (maybe 50-100 entries in a mid-game colony). Implicit providers only checked as fallback.

---

## Open Questions

1. **Tool-as-provider:** Does a stone axe in inventory provide `CAP_CUTTING_SURFACE:1` wherever the mover stands? Or must it be placed/held at a specific spot? Leaning toward: equipped tool adds its caps to the mover's current position.

2. **Capability stacking:** Do two campfires adjacent count as `heat_source:2`? Leaning toward: no, take the max level from any single provider. Stacking is for Phase E freeform stations (bellows + furnace = higher level).

3. **Need chains vs single resolution:** Current needs are single-step (find food, eat). Multi-step chains ("cook then eat at table") are a quality-of-life upgrade, not a survival necessity. Phase C should start with single-step resolver, add chains later.

4. **Implicit level-0 everywhere:** If every flat surface is `CAP_FLAT_SURFACE:0`, the resolver might waste time scoring thousands of cells. Solution: only check implicit providers when no explicit provider found within search radius, OR only check cells near the mover. (See "Resolver Performance" risk above for concrete plan.)
