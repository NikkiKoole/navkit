# Construction Staging — Recipe-Based Approach

Date: 2026-02-07, updated 2026-02-12

**Status: All 7 phases complete. Recipe system is the only construction path.**

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

The delivery checklist (all legacy single-item fields removed in Phase 7):

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

Legend: ✓ = tested, — = not yet tested

### Site clearing (Phase 5)
✓ 0a. Blueprint on cell with items → state = CLEARING
✓ 0b. All items removed → advances to AWAITING_MATERIALS
✓ 0c. Blueprint on empty cell → skips CLEARING, straight to AWAITING_MATERIALS
— 0d. Prevent non-blueprint item drops on active blueprint cells
✓ 0e. WorkGiver_BlueprintClear creates haul-away jobs for items at CLEARING blueprints

### Single-stage recipes (Phase 1+7)
✓ 1. Dry stone wall: deliver 3 rocks, build → wall with stone material
✓ 2. Brick floor: deliver 2 bricks, build → floor with MAT_BRICK
✓ 3. Ladder: deliver 1 log, build → ladder placed
✓ 4. Ramp with anyBuildingMat: deliver 1 rock → ramp placed (Phase 4)

### Multi-stage recipes (Phases 2+3)
✓ 5. Wattle & daub stage 0: deliver sticks + cordage, build → advances to stage 1
✓ 6. Wattle & daub stage 1: deliver 2 dirt, build → wall with MAT_DIRT
✓ 7. Plank wall end-to-end: sticks + cordage → build → planks → build → final wall
✓ 8. Thatch floor end-to-end: dirt → build base → dried grass → build → floor

### OR-materials (Phase 4)
✓ 9. Wattle fill with dirt available → hauler picks dirt → MAT_DIRT
✓ 10. Wattle fill with only clay available → hauler picks clay → MAT_CLAY
✓ 11. Stone wall: rocks and blocks both available → either satisfies the recipe
✓ 12. Thatch floor base: dirt, gravel, sand all valid → hauler picks closest

### Alternative + material locking (Phase 4)
✓ 13. First reservation to slot locks chosenAlternative
✓ 14. Second hauler for same slot must match chosenAlternative
✓ 15. Material also locked: won't reserve pine log if oak log was first reserved
✓ 16. Stage advance resets chosenAlternative and material locks for new stage
✓ 17. If locked alternative runs out, slot is stuck until cancel or more items

### Material inheritance (Phases 1-3+7)
✓ 18. Log wall with oak logs → MAT_OAK
✓ 19. Log wall with pine logs → MAT_PINE
✓ 20. Stone wall with granite rocks → MAT_GRANITE
✓ 21. Wattle wall: material comes from fill stage (materialFromStage=1), not frame
✓ 22. Brick wall: fixed material (MAT_BRICK), ignores item material
✓ 23. Thatch floor: fixed MAT_DIRT regardless of whether base was dirt/gravel/sand

### Delivery checklist (Phases 1-3)
✓ 24. Frame needs 2 sticks + 1 cordage: deliver 1 stick → still AWAITING_MATERIALS
✓ 25. Deliver second stick → still AWAITING (cordage missing)
✓ 26. Deliver cordage → all filled → READY_TO_BUILD
✓ 27. Need 3 rocks, deliver 2 → slot not filled, still waiting

### Reservations (Phases 1-3)
✓ 28. Hauler reserves item for slot → item.reserved = true, slot.reservedCount++
✓ 29. Slot with deliveredCount + reservedCount == required → no more haul jobs
✓ 30. Two haulers assigned to different slots of same blueprint simultaneously
✓ 31. Two blueprints competing for same item → only one gets reservation
✓ 32. Cancel haul mid-walk → reservation released, reservedCount--, slot unfilled

### Build time (Phase 1)
✓ 33. Build step uses per-stage buildTime from recipe, not flat constant

### Cancellation (Phase 6)
✓ 34. Cancel before any deliveries → no items dropped, blueprint removed
✓ 35. Cancel mid-stage, items delivered but not built → delivered items dropped
✓ 36. Cancel after stage 0 complete, during stage 1 → lossy refund from both stages
✓ 37. Cancel during build step → partial progress lost, lossy refund
✓ 38. Lossy refund: consumed items have recovery chance, not all always returned
✓ 39. Cancel releases all reservations on in-transit items

### Job assignment (Phases 1-3)
✓ 40. WorkGiver_BlueprintHaul finds AWAITING_MATERIALS → haul for unfilled slot
✓ 41. All slots filled → WorkGiver_Build creates build job, not haul
✓ 42. Stage advances → new haul jobs for next stage's inputs
✓ 43. Builder is independently assigned (not necessarily the last hauler)

### Edge cases (Phase 1)
✓ 44. Blueprint on cell that already has a blueprint → rejected
✓ 45. Blueprint on cell that already has a wall → rejected (must mine first)
✓ 46. Blueprint on cell that already has a floor → rejected (must remove first)
✓ 47. Materials run out entirely → blueprint sits in AWAITING_MATERIALS, no crash
✓ 48. Hauler carrying wrong item type → not assigned to any slot
✓ 49. anyBuildingMat slot accepts rock, blocks, dirt, etc. — any IF_BUILDING_MAT

### Save/load (Phase 1)
✓ 50. Save mid-stage with partial deliveries → load → resumes correctly
✓ 51. Save between stages (stage 0 done, stage 1 awaiting) → load → preserves
— 52. Save during CLEARING state → load → resumes clearing

## Implementation Phases

### Phase 1: Foundation — Dry Stone Wall (single-stage, single-input) ✓ COMPLETE

Completed 2026-02-12. Core recipe system implemented and verified in-game.

**What was built:**
- `construction.h` / `construction.c` — recipe data model + static table
- Blueprint struct extended with recipeIndex, stage, stageDeliveries, consumedItems
- `CreateRecipeBlueprint()` replaces wall path in `ExecuteDesignateBuild()`
- `WorkGiver_BlueprintHaul` rewritten with per-slot iteration, `FindNearestRecipeItem()`
- `RunJob_Build` uses per-stage buildTime
- `CompleteBlueprint` reads buildCategory + materialFromStage/materialFromSlot
- Save version bumped to v38
- Recipe cycling with M key in wall build mode
- Rendering, tooltips, inspect all updated for recipe display
- Legacy path preserved for ladder/floor/ramp (`recipeIndex == -1`)

**Tests passed:** 1, 20, 24, 27, 28, 29, 33, 34, 44, 45, 47, 50 (14 tests in
4 describe blocks, all green)

### Phase 2+3: Multi-input + Multi-stage ✓ COMPLETE

Completed 2026-02-12. Multi-input delivery, parallel hauling, and multi-stage
progression all verified. Phases 2 and 3 merged since the stage advancement
infrastructure was already in place from Phase 1.

**What was built:**
- `CONSTRUCTION_WATTLE_DAUB_WALL` recipe: 2 stages (frame: 2 sticks + 1 cordage
  → fill: 2 dirt), material inherited from fill stage (MAT_DIRT)
- `CONSTRUCTION_PLANK_WALL` recipe: 2 stages (frame: 2 sticks + 1 cordage
  → clad: 2 planks), material inherited from planks
- Stage indicator overlay in rendering ("S1"/"S2" in yellow, bottom-right)
- No runtime code changes needed — Phase 1's CompleteBlueprint already handled
  stage advancement, consumedItems tracking, stageDelivery reset, and cancel refund

**Tests passed:** 5, 6, 7, 16, 24, 25, 26, 30, 31, 36, 40, 42, 43, 48, 51
(24 tests in 5 describe blocks: construction_wattle_data,
construction_wattle_delivery, construction_wattle_parallel,
construction_multi_stage, construction_plank_wall)

### Phase 4: OR-materials + Locking ✓ COMPLETE

Completed 2026-02-12. OR-alternative inputs, alternative + material locking,
and anyBuildingMat all verified. No runtime code changes needed — Phase 1's
locking infrastructure (RecipeHaulItemFilter, FindNearestRecipeItem,
WorkGiver_BlueprintHaul reservation locking, DeliverMaterialToBlueprint
alternative locking) already handled everything correctly.

**What was built:**
- Dry stone wall updated: 3 rocks OR 3 blocks (altCount=2)
- Wattle & daub fill stage updated: 2 dirt OR 2 clay (altCount=2)
- `CONSTRUCTION_RAMP` recipe: 1 stage, 1 any-building-mat item (anyBuildingMat=true)
- End-to-end tests: wattle wall with clay (MAT_CLAY), dry stone wall with blocks
- Locking tests: chosenAlternative locks on first reservation, wrong alternative
  rejected, material locked (granite vs sandstone), stall when locked alt runs out
- anyBuildingMat tests: accepts all IF_BUILDING_MAT items, rejects non-building-mat,
  does NOT lock chosenAlternative

**Tests passed:** 9, 10, 11, 13, 14, 15, 16, 17, 49
(14 tests in 3 describe blocks: construction_or_materials,
construction_alternative_locking, construction_any_building_mat)

### Phase 5: Site Clearing  ✓ COMPLETE

Add the CLEARING state.

**What was built:**
- `BLUEPRINT_CLEARING` state added to `BlueprintState` enum
- `CreateRecipeBlueprint` checks for items at cell, starts in CLEARING if any exist
- `WorkGiver_BlueprintClear` — scans items at blueprint cell, creates haul-away jobs to stockpiles
- Transition from CLEARING → AWAITING_MATERIALS when cell is empty (checked each time WorkGiver runs)
- Wired into AssignJobs dispatch chain: `WorkGiver_BlueprintClear` → `WorkGiver_BlueprintHaul` → `WorkGiver_Build`
- CLEARING rendering: orange tint + "CLR" text overlay
- Inspect panel shows "CLEARING" state name

**Tests (6 tests):**
- Items at cell → CLEARING state; no items → AWAITING skip
- Manual item removal → transition on next WorkGiver call
- WorkGiver creates haul jobs to stockpile
- BlueprintHaul blocked while still CLEARING
- End-to-end: dirt cleared → rocks delivered → wall built

**What this proves:** Pre-construction site preparation works.

### Phase 6: Cancellation + Lossy Refund  ✓ COMPLETE

Add proper teardown with material recovery.

**What was built:**
- `CONSTRUCTION_REFUND_CHANCE` constant (75%) in construction.h
- Lossy refund: consumed items from completed stages roll per-item recovery chance
- Current stage delivered items: 100% refund (not yet built into anything)
- Proactive job cancellation: `CancelBlueprint` scans all movers and cancels
  jobs targeting the blueprint before deactivating
- No duplication: in-transit items aren't in `deliveredCount`, so job cancel
  safe-drop + blueprint refund don't double-spawn

**Tests (5 tests):**
- Cancel mid-stage with delivered items → 100% refund
- Lossy refund of consumed items from completed stages (seeded RNG)
- Cancel during BUILDING → builder job cancelled, progress reset, 100% refund
- Proactive cancel of in-transit haul jobs, reservations released
- Cancel haul mid-walk via WorkGiver → reservation released, mover idle

**What this proves:** Clean teardown at every state, lossy recovery works.

### Phase 7: Remaining Recipes + Migration  ✓ COMPLETE

Completed 2026-02-12. All remaining recipes added, all build types migrated to
recipes, all legacy code removed. Recipe system is the only construction path.

**What was built:**
- 6 new recipes: CONSTRUCTION_LOG_WALL (2 logs), CONSTRUCTION_BRICK_WALL (3 bricks),
  CONSTRUCTION_PLANK_FLOOR (2 planks), CONSTRUCTION_BRICK_FLOOR (2 bricks),
  CONSTRUCTION_THATCH_FLOOR (2 stages: 1 dirt/gravel/sand + 1 dried grass),
  CONSTRUCTION_LADDER (1 log or planks)
- CompleteBlueprint extended: BUILD_FLOOR, BUILD_LADDER, BUILD_RAMP handlers
- input.c migrated: ExecuteDesignateLadder/Floor/Ramp all use CreateRecipeBlueprint;
  selectedFloorRecipe/selectedLadderRecipe variables with M key cycling per category
- Removed: BlueprintType enum, CreateBuildBlueprint, CreateLadderBlueprint,
  CreateFloorBlueprint, CreateRampBlueprint, BlueprintUsesRecipe, BUILD_WORK_TIME,
  all legacy Blueprint fields (requiredMaterials, deliveredMaterialCount,
  reservedItem, deliveredMaterial, deliveredItemType, requiredItemType)
- Removed: all legacy branches in CancelBlueprint, FindBlueprintNeedingMaterials,
  DeliverMaterialToBlueprint, WorkGiver_BlueprintHaul, CancelJob, etc.
- Removed: FindNearestBuildingMat, BlueprintHaulItemFilter (dead code after migration)
- Save version bumped to v39

**Tests passed:** 2, 3, 8, 12, 18, 19, 22, 23, 46 (10 new tests in
construction_new_recipes describe block, plus all 27+ existing tests migrated
from CreateBuildBlueprint to CreateRecipeBlueprint)

**What this proves:** Full recipe coverage, old system fully replaced.

### Phase Summary

| Phase | Adds                        | Recipes              | Status   |
|-------|-----------------------------|-----------------------|----------|
| 1     | Core recipe system          | Dry stone wall        | ✓ Done   |
| 2     | Multi-input slots           | Wattle frame (temp)   | ✓ Done   |
| 3     | Multi-stage                 | Wattle & daub, plank  | ✓ Done   |
| 4     | OR-materials + locking      | Updates existing      | ✓ Done   |
| 5     | Site clearing               | —                     | ✓ Done   |
| 6     | Cancel + lossy refund       | —                     | ✓ Done   |
| 7     | All recipes + migration     | Everything else       | ✓ Done   |

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
