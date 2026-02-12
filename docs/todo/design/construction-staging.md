# Construction Staging — Recipe-Based Approach

Date: 2026-02-07, updated 2026-02-12

## Resolved Decisions (2026-02-12)

**Recipe selection UI**: Cycle through available recipes the same way material
selection works now (Work > Build > Wall, then cycle recipes with a key).
When in build-wall mode, player cycles through wall recipes (dry stone, wattle
& daub, log, plank, brick) instead of cycling item types. Same pattern for
floors, ladders, ramps.

**Item availability**: All items referenced by recipes already exist in the
ItemType enum: ITEM_ROCK, ITEM_BLOCKS, ITEM_LOG, ITEM_PLANKS, ITEM_STICKS,
ITEM_POLES, ITEM_CORDAGE, ITEM_DIRT, ITEM_CLAY, ITEM_SAND, ITEM_GRAVEL,
ITEM_BRICKS, ITEM_DRIED_GRASS, ITEM_GRASS, ITEM_BARK, ITEM_STRIPPED_LOG.
No new item types needed.

**IF_BUILDING_MAT flag**: Already exists (`item_defs.h:10`), with
`ItemIsBuildingMat()` macro. Currently set on: ITEM_BLOCKS, ITEM_LOG,
ITEM_PLANKS, ITEM_POLES, ITEM_BRICKS, ITEM_STRIPPED_LOG. Items like ITEM_ROCK,
ITEM_DIRT, ITEM_STICKS etc. do NOT have the flag — that's fine, because
`anyBuildingMat` is only used for the ramp catch-all slot. Recipe alternatives
arrays handle specific item matching regardless of the flag.

**Coexistence during migration**: Phase 1 replaces wall blueprints with the
recipe system. Ladder/floor/ramp keep the old code path temporarily. Each
subsequent phase migrates more build categories. Phase 7 removes all old paths
(BlueprintType, single-item fields, old Create*Blueprint functions). The game
is always buildable and testable between phases — progressive replacement,
not duplication.

## Core Idea

Construction isn't a universal "frame → fill" pipeline. Different wall/floor types
have different construction *methods*, each with their own stages and materials.
A **construction recipe** defines the full process for building a specific thing.

## Why Recipe-Based

- A wattle & daub wall needs cordage. A dry stone wall doesn't.
- A log wall is one stage. A plank wall is two.
- The stages, materials, and intermediate visuals are all per-recipe.
- This mirrors how workshop recipes already work — inputs → output, but in the world.

## Construction Recipes (Sketched)

### Walls

**Wattle & Daub Wall** (2 stages)
1. Frame: 2 sticks + 1 cordage → wattle frame
2. Fill: (2 dirt OR 2 clay) → finished wall
- Result: wall, material from fill (MAT_DIRT or MAT_CLAY)
- Early game, cheap materials, rough look

**Dry Stone Wall** (1 stage)
1. Stack: (3 rocks OR 3 blocks[stone]) → done
- Result: wall, inherits stone material (granite/sandstone/slate)
- No binding needed, slow work

**Log Wall** (1 stage)
1. Lay: 2 logs → done
- Result: wall, inherits wood material
- Heavy materials, sturdy

**Plank Wall** (2 stages)
1. Frame: 2 sticks + 1 cordage → skeleton
2. Clad: 2 planks → finished wall
- Result: wall, inherits wood material
- Refined look, more processing required

**Brick Wall** (1 stage)
1. Lay: 3 bricks → done
- Result: wall, MAT_BRICK
- Requires kiln infrastructure, high quality

### Floors

**Thatch/Mud Floor** (2 stages)
1. Base: (1 dirt OR 1 gravel OR 1 sand) → rough surface
2. Finish: 1 dried grass → packed/thatched floor
- Result: floor, fixed MAT_DIRT (the dried grass finish determines the look,
  not the base material)

**Plank Floor** (1 stage)
1. Lay: 2 planks → done
- Result: floor, inherits wood material

**Brick Floor** (1 stage)
1. Lay: 2 bricks → done
- Result: floor, MAT_BRICK

### Ladders / Ramps

Single-stage, 1-input recipes. They go through the same recipe system as
everything else — just simple recipes (1 stage, 1 input slot).

**Ladder** (1 stage)
1. Build: (1 log OR 1 planks) → done
- Result: ladder

**Ramp** (1 stage)
1. Build: 1 building material (any item with IF_BUILDING_MAT flag) → done
- Result: ramp

## Patterns

- Stages range from 1 to 2 (maybe 3 later for fancy construction)
- Cordage only appears in *framed* constructions (wattle, plank wall)
- Stacked/laid constructions (stone, brick, log) need no binding
- Natural progression: early game = wattle & daub, late game = brick/plank
- Some recipes share the same final result but use different paths
- Each stage has its own delivery + build step (existing haul → build job loop)

## Data Model (Conceptual)

Limits: MAX_STAGES = 3, MAX_INPUTS_PER_STAGE = 3, MAX_ALTERNATIVES = 5.
In practice most recipes use far less (typical: 1-2 stages, 1-2 inputs).

### Construction Recipe (static data, like workshop recipes)

```
ConstructionRecipe {
    name                          // "Wattle & Daub Wall"
    buildCategory                 // BUILD_WALL, BUILD_FLOOR, BUILD_LADDER, BUILD_RAMP
    stageCount                    // 1..MAX_STAGES
    stages[MAX_STAGES] {
        inputs[MAX_INPUTS_PER_STAGE] {
            alternatives[MAX_ALTERNATIVES] {
                itemType          // e.g. ITEM_DIRT, or ITEM_NONE to end list
            }
            altCount              // how many alternatives (1 = no choice)
            count                 // how many items needed
            anyBuildingMat        // if true, accept any IF_BUILDING_MAT item
                                  // (ignores alternatives array)
        }
        inputCount                // how many input slots this stage has
        buildTime                 // seconds (replaces flat BUILD_WORK_TIME)
    }
    resultMaterial                // MAT_BRICK = fixed, MAT_NONE = inherited
    materialFromStage             // which stage provides the final material (-1 if fixed)
    materialFromSlot              // which input slot within that stage (-1 if fixed)
}
```

`buildCategory` replaces the old `BlueprintType` — it tells CompleteBlueprint
whether to place a wall (CELL_WALL), set floor flags, or place a ladder/ramp.
The recipe is the single source of truth; `bp->type` is removed.

`anyBuildingMat` flag on an input slot means "accept any item with IF_BUILDING_MAT."
This covers ramps (and potentially other catch-all inputs) without listing every
alternative. When false, the alternatives array is used instead.

Material inheritance: when `materialFromStage`/`materialFromSlot` are set, the
final cell material comes from the *item's* material field of whatever was
actually delivered to that slot. So a log wall's material depends on whether
oak or pine logs were delivered — the recipe doesn't decide, the item does.

For recipes with fixed material (brick wall = MAT_BRICK, thatch floor = MAT_DIRT),
set `resultMaterial` directly and `materialFromStage`/`materialFromSlot` to -1.

### Blueprint (runtime, per-placed-construction)

Current single-item fields (`requiredMaterials`, `deliveredMaterialCount`,
`reservedItem`) get replaced with a delivery checklist:

```
Blueprint {
    ...existing fields (x, y, z, active, assignedBuilder, progress)...
    recipeIndex                   // which ConstructionRecipe
    stage                         // current recipe stage (0-indexed)
    stageDeliveries[MAX_INPUTS_PER_STAGE] {
        deliveredCount            // how many delivered so far
        reservedCount             // how many currently reserved (in transit)
        deliveredMaterial         // material of delivered items (for inheritance)
        chosenAlternative         // which alternative was picked (-1 = not yet chosen)
                                  // locked on first RESERVATION, not delivery
                                  // (prevents mixing before items arrive)
    }
    consumedItems[MAX_STAGES][MAX_INPUTS_PER_STAGE] {
        itemType                  // what was consumed (for lossy refund on cancel)
        count                     //
        material                  //
    }
}
```

Note: `stageDeliveries` is reset each time the stage advances. `consumedItems`
accumulates across all completed stages (needed for cancel refund).

Reservations: each input slot tracks `reservedCount` alongside `deliveredCount`.
Individual item reservations use the existing `item.reserved` flag + job reference
(same as current system). The slot just knows how many are in transit so it
doesn't over-request.

Locking order: first reservation to a slot locks both `chosenAlternative` (which
item type) and `deliveredMaterial` (which material within that type). All
subsequent reservations and deliveries to that slot must match both. This
prevents e.g. mixing oak and pine logs in the same "2 logs" slot.

### Blueprint States

```
BLUEPRINT_CLEARING              // NEW: hauling away pre-existing items
BLUEPRINT_AWAITING_MATERIALS    // existing: waiting for deliveries
BLUEPRINT_READY_TO_BUILD        // existing: all inputs delivered, waiting for builder
BLUEPRINT_BUILDING              // existing: builder working
```

CLEARING is entered only if items exist on the ground at the cell when the
blueprint is created. Otherwise it skips straight to AWAITING_MATERIALS.

### Flow

1. Player designates "build wattle & daub wall at (x,y,z)"
2. Blueprint created with recipeIndex, stage=0
3. If items on ground at location → state = CLEARING
   WorkGiver_BlueprintClear creates haul-away jobs for each item at the cell.
   Items are hauled to stockpiles (normal haul destinations).
   When cell is clear → advance to AWAITING_MATERIALS.
   If no items → skip straight to AWAITING_MATERIALS.
4. WorkGiver_BlueprintHaul loops current stage's inputs, finds unfilled slot
   (where deliveredCount + reservedCount < required count)
   → finds nearest matching item (checking all alternatives, or any IF_BUILDING_MAT)
   → if slot has chosenAlternative set, only match that alternative + material
   → first reservation to a slot locks chosenAlternative and deliveredMaterial
   → reserves item, creates haul job, reservedCount++
5. Hauler delivers item → deliveredCount++, reservedCount--
6. When all slots filled → state = READY_TO_BUILD
7. WorkGiver_Build assigns any free mover → state = BUILDING
   (builder is independently assigned, not necessarily the last hauler)
8. Build completes (duration = stage.buildTime) → consumed items recorded
9. Stage++ → reset stageDeliveries (all counts, chosenAlternative back to -1)
10. If more stages: state = AWAITING_MATERIALS, go to step 4
11. If final stage done: CompleteBlueprint
    → reads buildCategory to decide what to place (wall/floor/ladder/ramp)
    → reads materialFromStage/Slot to determine final material
12. On cancel at any point: release reservations, lossy refund from
    consumedItems + current stageDeliveries

### Construction on existing walls/floors

You cannot build on a cell that already has a wall or floor. The existing
structure must be mined/removed first. No in-place upgrading — always clear
then rebuild. (This is already enforced by IsCellWalkableAt check in
CreateBuildBlueprint.)

## Design Decisions

**Intermediate visuals**: No new CellTypes for now. Render a dynamic overlay on
the blueprint position showing construction stage (e.g. "1/2"). Design a proper
intermediate visual (frame sprite, scaffolding) later.

**Recipe selection**: Cycle through available recipes using the same UI pattern
as current material selection (Work > Build > Wall, then cycle with a key).
Player cycles wall recipes (dry stone, wattle & daub, log, plank, brick)
instead of cycling item types. Same pattern for floors, ladders, ramps.

**Auto-progression**: Yes. Player designates "build plank wall here" and it
auto-advances through all stages. No per-stage designation.

**Cancellation mid-build**: When undesignated halfway through, tear down and
drop materials — but not all of them. Each consumed item has a chance of being
recovered (say 50-75%), so deconstruction is lossy. You get most stuff back
but some is wasted. Exact tuning later, but the blueprint needs to track what
was consumed per stage so recovery is possible. No half-built ghost cells left.

**Flexible inputs (OR-materials)**: Support alternative materials per stage input
from the start. Like CDDA: "2 dirt OR 2 clay OR 2 sand" — up to MAX_ALTERNATIVES
options per input slot. Hauler picks whichever alternative is closest/available.
First reservation to a slot locks `chosenAlternative` and material — all
subsequent reservations and deliveries must match. No mixing types or materials
in one slot.

**anyBuildingMat flag**: For inputs that accept anything with IF_BUILDING_MAT
(like ramps), use the `anyBuildingMat` flag instead of listing every alternative.
This avoids needing a huge MAX_ALTERNATIVES and stays future-proof as new
building materials are added.

**Stalled blueprints**: If no matching materials exist anywhere, the blueprint
sits in AWAITING_MATERIALS indefinitely. This is fine — same as current behavior.
No timeout, no auto-cancel. Player can cancel manually if they want.

## Test Plan

### Site clearing
0a. Blueprint on cell with items → state = CLEARING
0b. All items removed → advances to AWAITING_MATERIALS
0c. Blueprint on empty cell → skips CLEARING, straight to AWAITING_MATERIALS
0d. Prevent non-blueprint item drops on active blueprint cells — CancelJob
    safe-drop and other placement should avoid cells with active blueprints
0e. WorkGiver_BlueprintClear creates haul-away jobs for items at CLEARING blueprints

### Single-stage recipes
1. Dry stone wall: deliver 3 rocks, build → wall with stone material
2. Brick floor: deliver 2 bricks, build → floor with MAT_BRICK
3. Ladder: deliver 1 log, build → ladder placed
4. Ramp with anyBuildingMat: deliver 1 rock → ramp placed

### Multi-stage recipes
5. Wattle & daub stage 0: deliver sticks + cordage, build → advances to stage 1,
   state resets to AWAITING_MATERIALS
6. Wattle & daub stage 1: deliver 2 dirt, build → wall with MAT_DIRT
7. Plank wall end-to-end: sticks + cordage → build → planks → build → final wall
8. Thatch floor end-to-end: dirt → build base → dried grass → build → floor

### OR-materials
9. Wattle fill with dirt available → hauler picks dirt → MAT_DIRT
10. Wattle fill with only clay available → hauler picks clay → MAT_CLAY
11. Stone wall: rocks and blocks both available → either satisfies the recipe
12. Thatch floor base: dirt, gravel, sand all valid → hauler picks closest

### Alternative + material locking
13. First reservation to slot locks chosenAlternative
14. Second hauler for same slot must match chosenAlternative (won't reserve clay
    if dirt was first reserved)
15. Material also locked: won't reserve pine log if oak log was first reserved
    for same slot
16. Stage advance resets chosenAlternative and material locks for new stage
17. If locked alternative runs out, slot is stuck until cancel or more items
    appear (no silent switch to different alternative)

### Material inheritance
18. Log wall with oak logs → MAT_OAK
19. Log wall with pine logs → MAT_PINE
20. Stone wall with granite rocks → MAT_GRANITE
21. Wattle wall: material comes from fill stage (materialFromStage=1), not frame
22. Brick wall: fixed material (MAT_BRICK), ignores item material
23. Thatch floor: fixed MAT_DIRT regardless of whether base was dirt/gravel/sand

### Delivery checklist
24. Frame needs 2 sticks + 1 cordage: deliver 1 stick → still AWAITING_MATERIALS
25. Deliver second stick → still AWAITING (cordage missing)
26. Deliver cordage → all filled → READY_TO_BUILD
27. Need 3 rocks, deliver 2 → slot not filled, still waiting

### Reservations
28. Hauler reserves item for slot → item.reserved = true, slot.reservedCount++
29. Slot with deliveredCount + reservedCount == required → no more haul jobs
30. Two haulers assigned to different slots of same blueprint simultaneously
31. Two blueprints competing for same item → only one gets reservation
32. Cancel haul mid-walk → reservation released, reservedCount--, slot unfilled

### Build time
33. Build step uses per-stage buildTime from recipe, not flat constant

### Cancellation
34. Cancel before any deliveries → no items dropped, blueprint removed
35. Cancel mid-stage, items delivered but not built → delivered items dropped
36. Cancel after stage 0 complete, during stage 1 → lossy refund from both stages
37. Cancel during build step → partial progress lost, lossy refund
38. Lossy refund: consumed items have recovery chance, not all always returned
39. Cancel releases all reservations on in-transit items

### Job assignment
40. WorkGiver_BlueprintHaul finds AWAITING_MATERIALS → haul for unfilled slot
41. All slots filled → WorkGiver_Build creates build job, not haul
42. Stage advances → new haul jobs for next stage's inputs
43. Builder is independently assigned (not necessarily the last hauler)

### Edge cases
44. Blueprint on cell that already has a blueprint → rejected
45. Blueprint on cell that already has a wall → rejected (must mine first)
46. Blueprint on cell that already has a floor → rejected (must remove first)
47. Materials run out entirely → blueprint sits in AWAITING_MATERIALS, no crash
48. Hauler carrying wrong item type → not assigned to any slot
49. anyBuildingMat slot accepts rock, blocks, dirt, etc. — any IF_BUILDING_MAT

### Save/load
50. Save mid-stage with partial deliveries → load → resumes correctly
51. Save between stages (stage 0 done, stage 1 awaiting) → load → preserves
    stage, consumedItems, stageDeliveries, and chosenAlternative
52. Save during CLEARING state → load → resumes clearing

## Implementation Phases

### Phase 1: Foundation — Dry Stone Wall (single-stage, single-input)

The simplest recipe that proves the new system works end-to-end.

**What to build:**
- ConstructionRecipe struct + static recipe table (start with just 1 recipe)
- New Blueprint fields (recipeIndex, stage, stageDeliveries, consumedItems)
- Migrate CreateBuildBlueprint to use recipe system
- WorkGiver_BlueprintHaul rewritten to use delivery slots instead of single item
- WorkGiver_Build uses per-stage buildTime
- CompleteBlueprint reads buildCategory + material inheritance
- Save/load for new Blueprint fields (save version bump)

**Recipe:** Dry stone wall — 1 stage, 1 input slot, 1 alternative (3 rocks).
No OR-materials, no multi-stage, no clearing. Just the core loop.

**Tests:** 1, 20, 24, 27, 28, 29, 33, 34, 44, 45, 47, 50

**What this proves:** The recipe system works, delivery slots work, material
inheritance works, save/load works. Old single-item blueprint code is replaced.

### Phase 2: Multi-input — Wattle Frame (single-stage, multiple inputs)

Add a second recipe with multiple input slots in one stage.

**What to build:**
- Wattle frame recipe: 1 stage with 2 slots (2 sticks + 1 cordage)
- WorkGiver_BlueprintHaul handles multiple unfilled slots
- Multiple haulers assigned to different slots simultaneously

But wait — wattle & daub is 2 stages. So for this phase, just test the
frame stage in isolation as a single-stage recipe that produces a wall.
(Temporary — gets replaced in phase 3.)

**Tests:** 24, 25, 26, 30, 31, 40, 43, 48

**What this proves:** Multiple input slots per stage, parallel hauling,
reservation counting across slots.

### Phase 3: Multi-stage — Full Wattle & Daub

Add stage advancement.

**What to build:**
- Stage progression: build completes → stage++ → reset stageDeliveries → AWAITING_MATERIALS
- consumedItems tracking across stages
- Stage overlay rendering (simple "1/2", "2/2" text)
- Full wattle & daub recipe (2 stages)
- Plank wall recipe (2 stages)

**Tests:** 5, 6, 7, 16, 33, 36, 37, 42, 51

**What this proves:** Multi-stage auto-progression, consumed items tracking,
intermediate visual feedback.

### Phase 4: OR-materials + Locking

Add alternative inputs and the locking mechanism.

**What to build:**
- alternatives[] array on input slots, altCount field
- chosenAlternative locking on first reservation
- Material locking (oak vs pine in same slot)
- Update wattle & daub fill stage: (2 dirt OR 2 clay)
- Dry stone wall: (3 rocks OR 3 blocks)
- anyBuildingMat flag for ramps

**Tests:** 9, 10, 11, 12, 13, 14, 15, 17, 49

**What this proves:** Flexible inputs, locking correctness, anyBuildingMat.

### Phase 5: Site Clearing

Add the CLEARING state.

**What to build:**
- BLUEPRINT_CLEARING state
- WorkGiver_BlueprintClear — scans items at blueprint cell, creates haul-away jobs
- Transition from CLEARING → AWAITING_MATERIALS when cell is empty
- Prevent item drops on active blueprint cells

**Tests:** 0a, 0b, 0c, 0d, 0e, 52

**What this proves:** Pre-construction site preparation works.

### Phase 6: Cancellation + Lossy Refund

Add proper teardown with material recovery.

**What to build:**
- Cancel at any state drops/refunds appropriately
- Lossy refund mechanism (per-item recovery chance)
- Release all in-transit reservations on cancel
- Cancel during BUILDING resets progress

**Tests:** 34, 35, 36, 37, 38, 39

**What this proves:** Clean teardown at every state, lossy recovery works.

### Phase 7: Remaining Recipes + Migration

Fill out the recipe table and migrate ladders/ramps/floors.

**What to build:**
- All remaining recipes: log wall, plank floor, brick wall, brick floor,
  thatch floor, ladder, ramp
- Migrate existing ladder/ramp/floor blueprint creation to use recipes
- Recipe selection UI (minimal — simple list/sub-menu)
- Remove old BlueprintType, old single-item fields

**Tests:** 2, 3, 4, 8, 18, 19, 21, 22, 23, 46

**What this proves:** Full recipe coverage, old system fully replaced.

### Phase Summary

| Phase | Adds                        | Recipes              | Key risk          |
|-------|-----------------------------|-----------------------|-------------------|
| 1     | Core recipe system          | Dry stone wall        | Data model right? |
| 2     | Multi-input slots           | Wattle frame (temp)   | Parallel hauling  |
| 3     | Multi-stage                 | Wattle & daub, plank  | Stage transitions |
| 4     | OR-materials + locking      | Updates existing      | Locking edge cases|
| 5     | Site clearing               | —                     | New WorkGiver     |
| 6     | Cancel + lossy refund       | —                     | State cleanup     |
| 7     | All recipes + migration     | Everything else       | Old code removal  |

Each phase is independently shippable — the game works after each one.
Phase 1-3 are the critical path. Phases 4-7 can be reordered.

## Deferred

- Weather/curing timers for mud walls
- Stage-specific tool checks (digging stick, mallet)
- Multi-colonist stages (heavy beams, log bridges)
- Scaffolding for tall walls
- Proper intermediate visuals (frame sprite, scaffolding)
- Deconstruction of finished walls (separate from cancel — planned teardown)
- In-place upgrade (e.g. dirt floor → plank floor without removing first)
