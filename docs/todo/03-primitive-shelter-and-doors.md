# Primitive Shelter & Doors — Survival-First Construction

> Written 2026-02-20. Extends feature-03-doors-shelter.md with a primitive tier that makes sense for a naked survivor.

---

## The Problem

Feature-03 designs doors and shelter around **planks** — but a naked survivor with only a sharp stone can't produce planks. Planks need a sawmill, which needs logs, which need chopping (slow bare-handed). That's a day 3+ activity at best.

Meanwhile, the survivor is freezing on night one. They need shelter **now**, from materials they can gather by hand: **sticks, poles, leaves, bark, grass, cordage**.

All of these items already exist in the codebase.

---

## Design: Two Shelter Tiers

### Tier 0: Primitive (gathered materials, no tools needed)

Built from what you can pick up off the ground or pull from trees.

| Construction | Materials | Result | Properties |
|-------------|-----------|--------|------------|
| **Stick Wall** | 4x STICKS + 2x CORDAGE | CELL_WALL (MAT_OAK) | Full wall, wood insulation, flimsy |
| **Leaf Roof** | 2x POLES + 3x LEAVES | Floor (HAS_FLOOR) on z+1 | Blocks rain/sky, poor insulation |
| **Bark Roof** | 2x POLES + 2x BARK | Floor (HAS_FLOOR) on z+1 | Blocks rain/sky, slightly better |
| **Leaf Door** | 2x POLES + 2x LEAVES | CELL_DOOR | Walkable, blocks wind/rain/fluids |
| **Thatch Roof** | Already exists! | CONSTRUCTION_THATCH_FLOOR | dirt/gravel/sand + dried grass |

### Tier 1: Constructed (requires tools / workshops)

The existing construction recipes — plank walls, brick walls, log walls, wattle & daub. These stay as-is and represent the "proper building" phase.

---

## What Already Exists (and almost works)

Looking at the current construction system:

1. **Wattle & Daub Wall** — 2x sticks + 1x cordage + 2x dirt/clay. This is *already* a primitive wall! The frame stage (sticks + cordage) is exactly what we need. The daub stage (dirt fill) is the upgrade.

2. **Thatch Floor** — 1x dirt/gravel/sand + 1x dried grass. Already a primitive floor/roof.

3. **Leaf Pile / Grass Pile** — Already exist as furniture (beds). The materials are there.

4. **Multi-stage construction** — The system already supports 2-stage builds (frame then fill). A "stick wall" could just be a wattle wall where you stop after the frame stage — or a simpler 1-stage recipe.

5. **Material inheritance** — Construction already tracks which materials were delivered and can inherit material type for the result.

### Key Insight: The Gap is Mostly Roofing + Doors

Walls are almost covered by wattle & daub (just needs the frame-only variant). The real gaps are:
- **No door cell type** at all (feature-03 core issue)
- **No primitive roof** — you can build thatch floors, but leaf/bark roofing would be cheaper and faster
- **No stick-only wall** — wattle & daub requires dirt/clay for the fill stage, which means digging

---

## New Construction Recipes

### Stick Wall (simplest wall)
```
Name: "Stick Wall"
Category: BUILD_WALL
Stages: 1
  Stage 0: 4x STICKS + 2x CORDAGE (build time: 2s)
Result: CELL_WALL
Insulation: WOOD tier (same as wattle & daub)
Material: inherited from sticks (oak/pine/birch/willow)
```

Why 4 sticks + 2 cordage: cheap but not free. Sticks are the most plentiful item (gathered from trees, scattered on ground). Cordage takes work (grass → dry → twist → braid) so there's still a cost. No dirt/clay needed — no digging required.

This is weaker than wattle & daub (which has dirt fill for better insulation), but it gets walls up fast.

### Leaf Roof (simplest overhead cover)
```
Name: "Leaf Roof"
Category: BUILD_FLOOR (placed on z+1 as ceiling)
Stages: 1
  Stage 0: 2x POLES + 3x LEAVES (build time: 2s)
Result: HAS_FLOOR flag on target cell
Insulation: AIR tier (leaky, but blocks rain and sky exposure)
Material: inherited from poles
```

Poles come from tree branches (already drop ITEM_POLES when chopped). Leaves come from tree gathering. Both available day one.

A leaf roof blocks `IsExposedToSky()` — meaning it stops rain, snow, and wind chill. But it has poor insulation (heat still escapes). It's a step up from nothing, but a plank/thatch floor above is much better.

### Bark Roof (slightly better)
```
Name: "Bark Roof"
Category: BUILD_FLOOR
Stages: 1
  Stage 0: 2x POLES + 2x BARK (build time: 2s)
Result: HAS_FLOOR flag on target cell
Insulation: WOOD tier (bark is decent insulation)
Material: inherited from bark
```

Bark comes from stripping logs at the sawmill — so this requires slightly more infrastructure than leaves. But it insulates better.

### Leaf Door (primitive door)
```
Name: "Leaf Door"
Category: BUILD_DOOR (new category from feature-03)
Stages: 1
  Stage 0: 2x POLES + 2x LEAVES (build time: 2s)
Result: CELL_DOOR
Insulation: new tier between AIR and WOOD (leaky but present)
Material: inherited from poles
```

A hanging curtain of leaves on a pole frame. Same gameplay function as a plank door (walkable by movers, blocks fluids/wind/light) but uses primitive materials.

### Plank Door (proper door, from feature-03)
```
Name: "Plank Door"
Category: BUILD_DOOR
Stages: 1
  Stage 0: 3x PLANKS (build time: 3s)
Result: CELL_DOOR
Insulation: WOOD tier
Material: inherited from planks
```

The "real" door. Requires sawmill infrastructure.

---

## Survival Day-by-Day Progression

```
Day 1 (hour 1-2): Immediate survival
  - Pick up sticks (scattered on ground)
  - Build GROUND FIRE on bare rock/dirt (3 sticks)     → warmth! (risky)
  - Gather leaves, build leaf pile bed (4 leaves)       → can rest
  - Gather grass → build drying rack (4 sticks) → start drying

Day 1 (hour 3-5): Cordage chain
  - Build rope maker (4 sticks)
  - Twist dried grass → short string → braid → cordage
  - Knap sharp stone at stone wall (rock + knap designation)

Day 1 (hour 5-8): First shelter
  - Build 3 stick walls around natural rock face or hillside
  - Build leaf roof overhead (poles + leaves on z+1)
  - Build leaf door to close the entrance
  → Now sheltered: rain blocked, wind chill blocked

Day 1 (evening): Safe fire
  - Craft DIGGING STICK (sticks + cordage, needs sharp stone)
  - Dig pit, build FIRE PIT with rocks (5 sticks + 3 rocks)  → safe warmth!
  - Move leaf pile bed inside shelter
  - Sleep in sheltered, heated space
  → Body temperature stays safe, no fire spread risk

Day 2-3: Upgrade
  - Craft stone tools (axe, pick) at knapping spot
  - Chop young trees → sticks + poles
  - Replace leaf roof with bark roof (better insulation)
  - Build wattle & daub walls (sticks + cordage + dirt, sturdier)

Day 4+: Proper building
  - Saw logs into planks at sawmill
  - Build plank walls, plank floors, plank doors
  - Full enclosed rooms with good insulation
```

---

## Implementation Plan

### Prerequisite: CELL_DOOR (from feature-03)

The door cell type must exist first. From feature-03:
- Add `CELL_DOOR` to CellType enum in `grid.h`
- Add cellDefs entry: no CF_BLOCKS_MOVEMENT, has CF_BLOCKS_FLUIDS, has CF_SOLID
- Add `BUILD_DOOR` to BuildCategory enum
- Handle in lighting (blocks sky light + block light)
- Handle in rendering (door sprite)
- Placement constraint: needs at least 1 cardinal wall neighbor

This is unchanged from feature-03. The primitive shelter work adds recipes that *use* CELL_DOOR, it doesn't change how CELL_DOOR works.

### Phase 1: Stick Wall + Leaf/Bark Roof Recipes

Add to `construction.h` enum:
```c
CONSTRUCTION_STICK_WALL,    // 4 sticks + 2 cordage → wall
CONSTRUCTION_LEAF_ROOF,     // 2 poles + 3 leaves → floor (roof)
CONSTRUCTION_BARK_ROOF,     // 2 poles + 2 bark → floor (roof)
```

Add recipe definitions in `construction.c`. These are simple 1-stage recipes, similar to dry stone wall or plank floor.

No new cell types needed — stick walls produce CELL_WALL (same as all walls), leaf/bark roofs set HAS_FLOOR (same as all floors).

**Save version**: No bump needed. New recipes are code-only changes (the construction recipe table is static, not saved).

### Phase 2: Door Recipes (Leaf + Plank)

Add to `construction.h` enum:
```c
CONSTRUCTION_LEAF_DOOR,     // 2 poles + 2 leaves → door
CONSTRUCTION_PLANK_DOOR,    // 3 planks → door
```

Both produce CELL_DOOR. The leaf door uses gathered materials, the plank door uses processed materials.

Placement handler in `designations.c`: same constraint as feature-03 (needs wall neighbor).

### Phase 3: Insulation Tuning

Consider a new insulation tier for primitive structures:

```c
#define INSULATION_TIER_AIR   0   // heat flows freely
#define INSULATION_TIER_LEAF  1   // leaves/thatch: leaky but present (NEW)
#define INSULATION_TIER_WOOD  2   // wood/bark: decent insulation (was 1)
#define INSULATION_TIER_STONE 3   // stone/brick: strong insulation (was 2)
```

This would mean:
- Leaf roof/door: tier 1 (some insulation, still leaky)
- Bark roof, stick wall, plank door: tier 2 (wood-grade)
- Stone/brick: tier 3 (best)

**Alternative**: Keep 3 tiers, put leaf structures at WOOD tier. Simpler, and the gameplay difference between leaf-insulation and wood-insulation may not be worth the complexity. The big jump is AIR → anything.

**Recommendation**: Start with 3 tiers (no new tier). Leaf roof = AIR tier but blocks `IsExposedToSky()`. Stick wall / bark roof = WOOD tier. The shelter benefit (blocking rain/wind/sky) is the main value; insulation refinement can come later.

---

## Ground Fire vs Fire Pit

The current campfire (WORKSHOP_CAMPFIRE) is a 2x1 passive workshop that burns sticks/logs. It appears instantly for free. With workshop construction costs, it would cost 5x sticks. But there's a nicer progression here:

### Ground Fire (minute 1, no tools)

The most desperate option. Pile sticks on bare ground and light them.

| Property | Value |
|----------|-------|
| Workshop | WORKSHOP_GROUND_FIRE (new, 1x1) |
| Template | `F` (just a fuel tile) |
| Cost | 3x STICKS |
| Build time | instant (or 1s) |
| Tool required | None |
| Behavior | Passive, burns sticks/logs → ash |
| Fire spread risk | **Yes** — sets adjacent flammable cells on fire (grass, trees, leaf piles) |
| Burn duration | Short (sticks burn fast, no containment) |
| Heat output | Same as campfire |

The ground fire is dangerous. It can set nearby grass on fire, burn down your leaf pile bed, even ignite your stick walls. You want to place it on bare rock or dirt, away from anything flammable. This creates real tension: "I need warmth NOW but I have to be careful where I put this."

### Fire Pit (day 1-2, needs digging stick)

A proper contained fire. Dig a shallow pit, ring it with rocks, then burn fuel safely.

| Property | Value |
|----------|-------|
| Workshop | WORKSHOP_CAMPFIRE (existing, renamed to Fire Pit) |
| Template | `FX` (fuel + work tile, existing) |
| Cost | 5x STICKS + 3x ROCK |
| Build time | 3s |
| Tool required | QUALITY_DIGGING >= 1 (digging stick or better) to dig the pit |
| Precondition | Digging step first (scrape a shallow pit) |
| Behavior | Passive, burns sticks/logs → ash (same as current) |
| Fire spread risk | **No** — contained in pit, rocks block spread |
| Burn duration | Longer (pit retains heat, less fuel waste) |
| Heat output | Same, but contained = safer |

The fire pit is the upgrade. It requires a digging stick (QUALITY_DIGGING:1) — this is the digging stick's first real use in the game. The tool makes the campfire *safe*, not just possible. You can still have fire without it (ground fire), but you're playing with fire (literally).

### The Digging Stick Moment

This creates a satisfying early-game arc:

```
Minute 1:   Gather sticks, build GROUND FIRE on bare rock    → warmth! (but risky)
Minute 5:   Gather grass, dry it, start cordage chain
Minute 15:  Knap sharp stone from rocks
Minute 20:  Craft DIGGING STICK (sticks + cordage, needs cutting:1)
Minute 22:  Dig pit, build FIRE PIT with rocks                → safe warmth!
```

The digging stick pays for itself immediately — it turns a dangerous ground fire into a safe fire pit. This is the CDDA-style progression the design docs describe: tools don't unlock things, they make things *better and safer*.

### Implementation Notes

**Ground fire spread**: The ground fire workshop should interact with the existing fire system. When active (burning fuel), it has a chance per tick to ignite adjacent flammable cells. Use the existing `fireGrid` and fire spread logic — the ground fire essentially acts as a fire source without the containment that a proper fire pit provides.

**Fire pit digging**: The "dig pit" step could be:
- A prerequisite for placing the fire pit workshop (designation: "dig fire pit here", then place workshop on the dug pit)
- Part of the workshop build time (the build action includes digging, tool quality speeds it up)
- A separate construction step like the 2-stage wattle & daub wall (stage 1: dig pit, stage 2: place rocks + sticks)

Simplest approach: make it part of the workshop build time. The fire pit has QUALITY_DIGGING as a build requirement. Without a digging tool, you can't build it at all (this is one of the few hard gates — you genuinely need to scrape earth to make a pit). With a digging stick it takes 3s, with a stone pick 2s.

This means: ground fire = no tool gate (anyone can pile sticks), fire pit = requires digging tool (first hard tool requirement in the game).

---

## How It Fits With Existing Construction System

The construction system already handles everything we need:

- **Multi-material alternatives**: `ConstructionInput.alternatives[]` — leaf roof could accept leaves OR dried grass
- **Material inheritance**: `materialFromStage/Slot` — stick walls inherit wood species from sticks
- **Build categories**: Just add BUILD_DOOR alongside BUILD_WALL, BUILD_FLOOR, etc.
- **Blueprint delivery**: Movers already haul materials to blueprint sites and build in stages
- **Action registry**: New actions slot into the existing `SUBMODE_BUILD` category menu

No architectural changes needed. Just new recipe entries and the CELL_DOOR cell type.

---

## Materials Budget

What does a minimal primitive shelter cost?

```
3 stick walls:    12x STICKS + 6x CORDAGE
1 leaf door:       2x POLES  + 2x LEAVES
1 leaf roof:       2x POLES  + 3x LEAVES
1 leaf pile bed:   4x LEAVES
─────────────────────────────────────────
Total:            12x STICKS, 6x CORDAGE, 4x POLES, 9x LEAVES
```

Cordage cost (6x) is the bottleneck. Each cordage takes: 3x grass → dry → 3x dried grass → twist → 3x short string → braid → 1x cordage. So 6 cordage = 18 grass gathered + drying + twisting + braiding. That's a meaningful amount of work — maybe 2-3 game-hours at the rope maker.

Sticks (12x) are plentiful — scattered on ground + gathered from trees. Poles (4x) come from tree branches. Leaves (9x) from tree gathering.

This feels right: the shelter is achievable on day 1 but takes real effort, especially the cordage.

---

## Connections to Other Features

- **Feature 03 (Doors/Shelter)**: This doc extends feature-03 with primitive-tier recipes. The CELL_DOOR design is unchanged.
- **Feature 04 (Tools)**: No tool requirement for primitive construction. Tool speed bonus would make building faster (QUALITY_HAMMERING) but isn't required.
- **Temperature system**: Shelter blocks wind chill via `IsExposedToSky()`. Insulation tiers retain hearth heat.
- **Weather system**: Rain/snow already check `IsExposedToSky()` — a leaf roof overhead is sufficient.
- **Campfire/Hearth**: Fire in a sheltered space heats the enclosed area. This is the survival loop: shelter + fire = warmth.

---

## Summary

| Recipe | Materials | Tier | Tool Required | Key Benefit |
|--------|-----------|------|--------------|-------------|
| Ground Fire | 3 sticks | Minute 1 | None | Immediate warmth, but fire spread risk |
| Stick Wall | 4 sticks + 2 cordage | Primitive | None | Walls without digging or sawing |
| Leaf Roof | 2 poles + 3 leaves | Primitive | None | Blocks rain/wind, cheapest overhead cover |
| Bark Roof | 2 poles + 2 bark | Primitive+ | None | Better insulation than leaves |
| Leaf Door | 2 poles + 2 leaves | Primitive | None | Seal a room with gathered materials |
| Fire Pit | 5 sticks + 3 rocks | Primitive | DIGGING:1 | Safe contained fire, no spread risk |
| Wattle & Daub | 2 sticks + 1 cordage + 2 dirt | Mid | None | Existing recipe, needs digging |
| Plank Wall | 2 sticks + 1 cordage + 2 planks | Advanced | None | Existing recipe, needs sawmill |
| Plank Door | 3 planks | Advanced | None | Proper door, needs sawmill |
| Thatch Floor | 1 dirt/gravel + 1 dried grass | Mid | None | Existing recipe |
| Plank Floor | 2 planks | Advanced | None | Existing recipe, needs sawmill |

The primitive tier costs **no save version bump** (no new cell types beyond CELL_DOOR, no new item types). The ground fire is a new workshop type (WORKSHOP_GROUND_FIRE). The fire pit reuses the existing WORKSHOP_CAMPFIRE with added construction costs and a tool requirement.
