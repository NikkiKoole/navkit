Here are the findings from the jobs.c audit:

---

### HIGH

**Finding 1: PlantSapling consumes item without calling DeleteItem**
- `RunJob_PlantSapling` manually sets `items[idx].active = false` instead of calling `DeleteItem()`, so `itemCount` never decrements and `itemHighWaterMark` never shrinks. Over a long session, this is a monotonically growing leak.

### MEDIUM

**Finding 2: Carried item position not updated during craft job movement**
- `CRAFT_STEP_MOVING_TO_WORKSHOP` and `CRAFT_STEP_CARRYING_FUEL` never update the carried item's x/y/z each tick. Every other carrying phase in the codebase does this. Visually the item floats at its pickup location while the mover walks away.

**Finding 3: CancelJob fuel item drop skips walkability check**
- We just fixed the `carryingItem` safe-drop to check walkability, but the **fuel item** drop a few lines later does no such check. Same bug, second instance.

**Finding 5: Auto-suspend storage check uses MAT_NONE instead of actual output material**
- `WorkGiver_Craft` checks output stockpile space with `MAT_NONE`, but the actual output inherits the input material. This can incorrectly suspend bills (no slot for default material) or fail to suspend them (slot exists for default but not actual material).

**Finding 6: Cross-z hauling sets unreachable cooldowns on reachable items**
- The mover spatial grid is 2D, so a mover on z=0 can be selected to haul an item on z=2. Pathfinding fails and the item gets a 5-second unreachable cooldown, blocking correct-z movers too.

### LOW

**Finding 4**: Deposited input item at workshop can end up on unwalkable tile if workshop is destroyed mid-craft.
**Finding 7**: Designation cache stores first walkable adjacent tile, not closest to mover.
**Finding 8**: Haul job doesn't explicitly set `item->z` to stockpile z on placement.
