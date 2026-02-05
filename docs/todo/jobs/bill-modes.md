# Bill Modes

Workshop bill system - how players tell workshops what to produce.

---

## Why This Matters

**Bill modes are the critical enabler for workshop production.** Without them:
- Workshops can only do one-off crafting
- No automation, no stockpile maintenance
- Continuous processes (Firepit) impossible

With bill modes, workshops become the engine of production loops.

---

## Core Modes

### Do X Times
Produce exactly N items, then stop.

**Use case:** One-time crafting needs.
- "Make 10 planks for this building project"
- "Make 5 bricks to finish this wall"

**Behavior:**
- Counter decrements on each completion
- Bill becomes inactive at 0
- Player can re-enable with new count

### Do Until X
Produce until stockpile reaches N items, then pause. Resume when stockpile drops below N.

**Use case:** Stockpile maintenance (automation).
- "Keep 20 planks in stock"
- "Maintain 50 charcoal for fuel"

**Behavior:**
- Check stockpile count before starting job
- If stockpile >= target, skip this bill
- If stockpile < target, create crafting job
- Re-check after each completion

**Stockpile counting:**
- Count items in linked stockpile? Or global?
- Simplest: count all items of type on map
- Better: count items in designated stockpile zone

### Do Forever
Repeat indefinitely until manually stopped or inputs exhausted.

**Use case:** Continuous processes.
- Firepit burning fuel (heat + light output)
- Processing backlog of raw materials

**Behavior:**
- Always create job if inputs available
- Never auto-stop
- Player must pause/cancel manually

**Special case - Firepit:**
```
Bill: Burn 1 ITEM_WOOD -> Heat + Light + 1 ITEM_ASH
Mode: Do forever
Result: Continuous fire as long as fuel is hauled to input
```

This avoids needing a separate "fuel over time" mechanic.

---

## Bill Properties

### Priority / Order
Bills are processed in order. First bill with available inputs wins.

**Use case:** Prefer efficient recipes.
- Bill 1: "Make charcoal" (uses wood efficiently)
- Bill 2: "Burn wood directly" (fallback if no kiln time)

### Pause / Resume
Player can temporarily disable a bill without deleting it.

**Use case:** Seasonal production, resource conservation.

### Ingredient Search Radius
How far workers look for inputs.

**Options:**
- Workshop tile only (inputs must be pre-hauled)
- Nearby radius (e.g., 10 tiles)
- Linked stockpile only
- Anywhere on map

**Default:** Linked stockpile or nearby radius. Avoid "anywhere" for performance.

### Ingredient Filters (Future)
Restrict which items satisfy an input.

**Example:** "Use oak wood only" or "Use any leaves except pine"

[future] - adds complexity, defer until needed

---

## Multi-Input Recipes

Some recipes need multiple input types (e.g., Kiln needs clay + fuel).

### Option A: All inputs on input tile
Workshop has one input tile. All ingredients must be hauled there before crafting starts.

**Pro:** Simple mental model
**Con:** Crowded input tile, hauling bottleneck

### Option B: Multiple input tiles
Workshop has separate tiles for different input types (e.g., F = fuel, I = ingredients).

```
Kiln layout:
  # F #
  # X O
  . . .

F = fuel input
X = work tile (main ingredient here)
O = output
```

**Pro:** Cleaner hauling, fuel can be pre-stocked
**Con:** More complex workshop definitions

**Recommendation:** Option B for workshops with fuel. Fuel tile is a common pattern.

---

## Job Generation

When does a bill create a crafting job?

### Check Conditions
1. Bill is active (not paused)
2. Bill mode allows (count > 0, or stockpile below target, or "forever")
3. Inputs available (within search radius)
4. Workshop not already working
5. Output tile has space

### Job Lifecycle
1. Bill passes checks -> Job created in Job Pool
2. Worker claims job
3. Worker hauls inputs to workshop (if not already there)
4. Worker performs crafting work
5. Output appears at output tile
6. Bill updates (decrement count, re-check stockpile)
7. Repeat from step 1

---

## Edge Cases

### Inputs Disappear Mid-Job
Another worker takes the ingredients before crafter arrives.

**Solution:** Job fails, returns to pool, re-checks inputs.

### Output Tile Full
No space for output item.

**Solution:** Job blocks until output hauled away. Or: worker waits, then places output when space available.

### Multiple Bills Want Same Inputs
Two bills both need ITEM_WOOD.

**Solution:** First bill (by priority) claims inputs. Second bill waits.

### Stockpile Count Race Condition
"Do until 20" bill, stockpile has 19, two workers both start jobs.

**Solution:** Reserve items when job created, not when job completes. Or: accept slight overshoot.

---

## Connection to Other Systems

### Hauling
Bills generate implicit hauling jobs:
- Haul inputs to workshop
- Haul outputs to stockpile

Hauling priority should match bill urgency.

### Zones
Stockpile zones affect "Do until X" counting.
- Link workshop to specific stockpile
- Or count globally (simpler but less control)

### Instant Reassignment
See `instant-reassignment.md`. After crafting completes:
- Immediately check this bill again
- Or check next bill in workshop
- Or find different job

No idle ticks between crafting jobs.

---

## Implementation Notes

### Data Structure
```
Bill:
  - recipe_id
  - mode: DO_X_TIMES | DO_UNTIL_X | DO_FOREVER
  - count: int (for DO_X_TIMES)
  - target: int (for DO_UNTIL_X)
  - paused: bool
  - search_radius: int (or LINKED_STOCKPILE)
  - priority: int (order in workshop bill list)
```

### Workshop State
```
Workshop:
  - bills: Bill[]
  - current_job: Job | null
  - input_tiles: Tile[]
  - output_tile: Tile
  - work_tile: Tile
```

### Stockpile Counting
```
count_items(item_type, zone):
  if zone == GLOBAL:
    return count all items of type on map
  else:
    return count items of type in zone
```

---

## Summary

| Mode | Behavior | Use Case |
|------|----------|----------|
| Do X times | Produce N, stop | One-time needs |
| Do until X | Maintain stockpile at N | Automation |
| Do forever | Never stop | Continuous processes |

Bill modes are the foundation of workshop automation. Implement these first, then workshops become useful.
