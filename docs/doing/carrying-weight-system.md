# Carrying Weight System

**Status:** Design
**Priority:** Medium
**Estimated Effort:** 70-100 lines, 2-3 hours

## Overview

Movers currently move at constant speed regardless of what they're carrying. This makes hauling 20 stone blocks feel identical to carrying a handful of grass - no physical weight or consequence.

**Goal:** Movers slow down when carrying heavy items, making material choice matter and adding satisfying physical feedback.

## Current State

- All movers move at `BASE_MOVE_SPEED * gameSpeed`
- No concept of item weight
- Carrying 1 item vs 10 items feels identical
- No visual distinction between empty and loaded movers

## Proposed Design

### 1. Item Weights

Add `float weight` to item types. Weight represents relative heaviness (not realistic kg).

**Weight Scale:**
- `0.1-0.5` = Very light (grass, leaves, cordage)
- `0.5-1.0` = Light (planks, sticks, food)
- `1.0-2.0` = Medium (logs, tools, furniture)
- `2.0-4.0` = Heavy (stone blocks, ore, metal)
- `4.0+` = Very heavy (large furniture, machinery)

**Proposed weights by item type:**

```c
// In item_defs.c or items.h
static const float itemWeights[ITEM_TYPE_COUNT] = {
    [ITEM_NONE]             = 0.0f,

    // Natural materials - light organic
    [ITEM_GRASS_BUNDLE]     = 0.2f,
    [ITEM_STICK]            = 0.3f,
    [ITEM_LEAVES]           = 0.15f,
    [ITEM_SAPLING]          = 0.4f,
    [ITEM_BARK]             = 0.3f,

    // Wood products - medium
    [ITEM_LOG]              = 2.0f,   // Heavy raw log
    [ITEM_STRIPPED_LOG]     = 1.8f,   // Slightly lighter
    [ITEM_PLANKS]           = 0.8f,   // Processed, lighter per unit
    [ITEM_CORDAGE]          = 0.2f,   // Twisted grass, very light

    // Stone - heavy
    [ITEM_ROCK]             = 3.0f,   // Dense stone block
    [ITEM_SAND]             = 1.5f,   // Loose material
    [ITEM_CLAY]             = 1.8f,   // Wet clay, dense
    [ITEM_DIRT]             = 1.2f,   // Loose soil
    [ITEM_GRAVEL]           = 2.0f,   // Small stones
    [ITEM_PEAT]             = 1.0f,   // Organic soil, lighter

    // Processed materials
    [ITEM_CHARCOAL]         = 0.5f,   // Light, porous
    [ITEM_GLASS]            = 1.5f,   // Dense but small pieces

    // Fuel
    [ITEM_COAL]             = 1.2f,   // Dense carbon
};
```

### 2. Calculate Carried Weight

**When does a mover carry items?**
- `JOBTYPE_HAUL` - carrying item to stockpile
- `JOBTYPE_DELIVER` - bringing materials to workshop
- `JOBTYPE_DELIVER_FUEL` - bringing fuel to workshop
- Possibly `JOBTYPE_CRAFT` - if carrying materials to workshop (depends on implementation)

**Where to calculate:**
In `mover.c` - add helper function:

```c
float GetMoverCarriedWeight(Mover* m) {
    if (m->currentJobId < 0) return 0.0f;

    Job* job = &jobs[m->currentJobId];
    float totalWeight = 0.0f;

    // Check job type - only certain jobs carry items
    if (job->type == JOBTYPE_HAUL ||
        job->type == JOBTYPE_DELIVER ||
        job->type == JOBTYPE_DELIVER_FUEL) {

        // Sum weights of reserved items
        if (job->reservedItemId >= 0 && job->reservedItemId < MAX_ITEMS) {
            Item* item = &items[job->reservedItemId];
            if (item->active) {
                totalWeight += itemWeights[item->type];
            }
        }

        // If job can reserve multiple items (future feature), loop here
    }

    return totalWeight;
}
```

### 3. Apply Speed Modifier

**Formula options:**

**Option A: Linear decay** (simple, predictable)
```c
float weightFactor = 1.0f - (carriedWeight * 0.1f);
weightFactor = fmaxf(weightFactor, 0.3f);  // Never slower than 30%
// 0.0 weight = 1.0x speed (100%)
// 2.0 weight = 0.8x speed (80%)
// 4.0 weight = 0.6x speed (60%)
// 7.0+ weight = 0.3x speed (30% minimum)
```

**Option B: Diminishing returns** (feels more realistic)
```c
float weightFactor = 1.0f / (1.0f + carriedWeight * 0.15f);
// 0.0 weight = 1.00x speed
// 2.0 weight = 0.77x speed (23% slower)
// 4.0 weight = 0.63x speed (37% slower)
// 6.0 weight = 0.53x speed (47% slower)
```

**Recommendation:** Start with Option B (diminishing returns) - heavy loads feel *heavy* but not crippling.

**Where to apply:**
In `UpdateMovers()` where movement speed is calculated:

```c
// In movement update loop
float carriedWeight = GetMoverCarriedWeight(m);
float weightFactor = 1.0f / (1.0f + carriedWeight * 0.15f);
float effectiveSpeed = BASE_MOVE_SPEED * weightFactor;

// Apply to movement
Vec2 moveDir = {dx, dy};
float moveDist = effectiveSpeed * dt;
// ... rest of movement code
```

### 4. Visual Feedback (Optional but Recommended)

**Subtle indicators:**
1. **Slower animation** - Already happens naturally if speed changes
2. **Color tint** - Slightly darker/reddish when carrying heavy load
3. **Posture sprite** - Different sprite when carrying (hunched over)
4. **Particles** - Dust/effort particles when carrying 4.0+ weight

**Minimal MVP:** Just the speed change (no visual changes needed initially)

## Implementation Plan

### Phase 1: Core System (MVP)
1. Add `itemWeights[]` array to `items.h` or `item_defs.c`
2. Implement `GetMoverCarriedWeight()` in `mover.c`
3. Apply weight factor in movement calculation
4. Test with hauling different materials

### Phase 2: Tuning
1. Playtest with various materials
2. Adjust weights if stone feels too slow or grass too fast
3. Tune weight factor formula (maybe adjust 0.15 coefficient)

### Phase 3: Visual Feedback (Future)
1. Add color tint for heavy loads
2. Dust particles when carrying 4.0+ weight
3. Different mover sprite when loaded

## Code Locations

**Files to modify:**
- `src/entities/items.h` - Add `extern const float itemWeights[ITEM_TYPE_COUNT];`
- `src/entities/item_defs.c` - Define weight array (or inline in items.h)
- `src/entities/mover.c` - Add `GetMoverCarriedWeight()`, apply in `UpdateMovers()`

**Estimated lines:**
- Item weights array: ~30 lines
- GetMoverCarriedWeight(): ~20 lines
- Speed modifier application: ~5 lines
- Headers/declarations: ~5 lines
- **Total: ~60 lines**

## Testing Checklist

- [ ] Empty mover moves at full speed
- [ ] Mover hauling grass (0.2 weight) barely slowed
- [ ] Mover hauling log (2.0 weight) noticeably slower (~25%)
- [ ] Mover hauling stone (3.0 weight) significantly slower (~35%)
- [ ] Mover hauling multiple heavy items stacks weight correctly
- [ ] Speed returns to normal after dropping off item
- [ ] Crafting jobs (not carrying) don't slow mover
- [ ] Idle movers move at full speed

## Balance Considerations

**Too slow?**
- Reduce weight values (divide all by 2)
- Reduce weight factor coefficient (0.15 → 0.10)
- Increase minimum speed floor (0.3 → 0.5)

**Not noticeable enough?**
- Increase weight values (multiply by 1.5)
- Increase weight factor coefficient (0.15 → 0.20)
- Add visual feedback (color tint, particles)

**Feels unfair?**
- Make stockpiles closer to resource sources
- Add "strong" mover trait (future: some movers carry weight better)
- Add tools (future: carts, wheelbarrows reduce effective weight)

## Future Enhancements

1. **Tools reduce weight:**
   - Wheelbarrow: 0.5x weight multiplier
   - Cart: 0.3x weight multiplier
   - Hauling yoke: 0.7x weight multiplier

2. **Mover strength trait:**
   - Some movers naturally stronger (0.8x weight effect)
   - Skill-based: experienced haulers improve over time

3. **Fatigue system:**
   - Carrying heavy loads drains stamina faster
   - Tired movers move even slower
   - Need rest breaks

4. **Stack limits:**
   - Can't carry more than X total weight (e.g., 10.0)
   - Hauling heavy stone = 1-2 items per trip
   - Hauling grass = 10-20 items per trip

5. **Team hauling:**
   - Multiple movers assigned to single heavy item
   - Shared weight = faster movement

## Dependencies

- None! Self-contained feature.
- Works with existing job/item/mover systems.

## Notes

- Start conservative with weights - easier to increase than decrease
- Players should *feel* the difference but not be frustrated
- Heavy hauling becomes a strategic consideration (stockpile placement matters)
- Makes material choice meaningful (stone vs wood construction trade-offs)

---

## Open Questions

1. Should crafting jobs (JOBTYPE_CRAFT) slow movers when walking to workshop with materials?
2. Should mover speed be cached per frame or recalculated every movement update?
3. Do we want a UI indicator showing mover's carried weight? (e.g., tooltip)
4. Should some materials be *too heavy* for a single mover (require team hauling)?

---

**Next Steps:**
1. Get user feedback on proposed weights
2. Implement Phase 1 (MVP)
3. Playtest and tune
4. Add visual feedback if it feels good
