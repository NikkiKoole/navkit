
# Freetime: Mover Personal Activities

What movers do when they're not working.

---

## Current State

Movers either:
1. Work (job assigned)
2. Wander randomly (no job, endless mode)

There's no "personal time" - no needs to satisfy, no reason to seek out specific places or objects.

---

## Goal

Give movers simple personal activities that:
- Create demand for furniture (beds, chairs, tables)
- Create demand for food (foraging, eating)
- Close production loops (wood → furniture → need satisfaction)
- Make the colony feel alive (movers doing things for themselves)
- Lay groundwork for a full needs system later

---

## Starting Small: Three Furniture Types

### Bed
- **Source**: Crafted from planks (sawmill → carpenter's bench)
- **Sink**: Movers sleep in beds to restore energy
- **Behavior**: Mover walks to bed, lies down, energy recovers faster than standing

### Chair
- **Source**: Crafted from planks
- **Sink**: Movers sit to rest / improve mood
- **Behavior**: Mover walks to chair, sits, comfort/mood increases

### Table
- **Source**: Crafted from planks
- **Sink**: Movers eat at tables (future: improves meal satisfaction)
- **Behavior**: For now, just a target for "sit near" behavior with chairs

---

## How It Fits Autarky

```
CELL_TREE_TRUNK → [chop] → ITEM_WOOD → [sawmill] → ITEM_PLANKS
                                                        ↓
                                              [carpenter's bench]
                                                        ↓
                                    ITEM_BED / ITEM_CHAIR / ITEM_TABLE
                                                        ↓
                                              [place as furniture]
                                                        ↓
                                              Mover uses furniture
                                                        ↓
                                              Need satisfied (energy/comfort)
                                                        ↓
                                              Mover returns to work
```

**Loop closed**: Wood has a sink beyond construction. Furniture has a use beyond decoration.

---

## Furniture vs Items vs Cells

Three options for representing placed furniture:

### Option A: Furniture as Cells
- `CELL_BED`, `CELL_CHAIR`, `CELL_TABLE`
- Pros: Simple, integrates with existing cell system
- Cons: Occupies the cell, can't have furniture on floors easily

### Option B: Furniture as Items (Ground)
- Furniture is an item that stays where placed
- Movers interact with item at location
- Pros: Items already have position, material, etc.
- Cons: Items are meant to be hauled; need "anchored" flag

### Option C: Furniture as Separate Entity Type
- New `Furniture` struct like `Workshop`
- Has position, type, material, user slot
- Pros: Clean separation, can track who's using it
- Cons: Another entity system to manage

**Recommendation**: Option C. Furniture is similar to workshops - placed objects that movers interact with at specific tiles. A small Furniture struct is cleaner than overloading items or cells.

---

## Furniture Entity (Sketch)

```c
typedef enum {
    FURNITURE_BED,
    FURNITURE_CHAIR,
    FURNITURE_TABLE,
} FurnitureType;

typedef struct {
    int x, y, z;
    FurnitureType type;
    MaterialType material;      // Wood species for tinting/value
    bool active;
    int assignedMover;          // Who "owns" this bed (-1 = unassigned)
    int currentUser;            // Who's currently using it (-1 = nobody)
} Furniture;
```

---

## Food: Foraging & Eating

### Wild Food Sources

Start simple: food grows in the world, movers pick and eat it.

#### Berry Bushes
- **Cell**: `CELL_BERRY_BUSH` - spawns in terrain gen on grass/dirt
- **Behavior**: Periodically produces berries (visible on bush)
- **Harvest**: Mover walks to bush, picks berries → `ITEM_BERRIES`
- **Regrowth**: Bush regrows berries over time (like tree leaf regrowth)

#### Fruit Trees
- **Cell**: New tree type or variant - apple, pear, cherry
- **Behavior**: Mature fruit trees produce fruit seasonally or periodically
- **Harvest**: Two options:
  - Pick from tree (like gathering leaves)
  - Fruit drops to ground, mover picks up `ITEM_FRUIT`
- **Variety**: `ITEM_APPLE`, `ITEM_PEAR`, etc. (or generic `ITEM_FRUIT` with material)

#### Dropped/Ground Fruit
- Fruit can fall from trees naturally (wind, overripe)
- Creates ground items movers can gather
- Adds foraging gameplay: wander the map, find food

### Food Items

| Item | Source | Nutrition | Notes |
|------|--------|-----------|-------|
| ITEM_BERRIES | Berry bush | Low | Abundant, quick snack |
| ITEM_APPLE | Apple tree | Medium | Seasonal? |
| ITEM_PEAR | Pear tree | Medium | Seasonal? |

Or simplified: `ITEM_FRUIT` with material = `MAT_APPLE`, `MAT_BERRY`, etc.

### Eating Behavior

```c
// On Mover struct
float hunger;           // 0.0 = starving, 1.0 = full
```

When `hunger < HUNGRY_THRESHOLD`:
1. Find nearest food (carried, ground, or on bush/tree)
2. If on bush/tree: walk there, harvest
3. If ground item: walk there, pick up
4. Eat food (brief animation/timer)
5. Hunger restored based on food nutrition value

### Eating at Tables (Bonus)

If table + chair available nearby:
- Mover prefers to sit and eat
- Eating at table = mood bonus (future)
- Eating standing/on ground = no bonus or slight penalty

This connects furniture and food systems elegantly.

### Food Autarky Loop

```
CELL_BERRY_BUSH → [harvest] → ITEM_BERRIES → [eat] → Hunger satisfied
      ↑                                                      ↓
   [regrows]  ←←←←←←←←←←←←← (time passes) ←←←←←←←←←←←←←←←←←←
```

```
CELL_FRUIT_TREE → [fruit drops or pick] → ITEM_FRUIT → [eat] → Hunger satisfied
      ↑                                                              ↓
   [regrows]  ←←←←←←←←←←←←←←←←←← (time passes) ←←←←←←←←←←←←←←←←←←←←←
```

**Loop closed**: Natural food source → consumable item → need satisfied → regrowth.

### Food Storage

- Food items can be stockpiled (new filter category: Food)
- Movers grab food from stockpile when hungry
- Future: food spoilage, preservation (cooking, drying)

---

## Two Needs: Energy and Hunger

Start with two needs: **Energy** and **Hunger**.

```c
// On Mover struct
float energy;           // 0.0 = exhausted, 1.0 = fully rested
float hunger;           // 0.0 = starving, 1.0 = full
```

### Energy
- **Drain**: Slowly over time while awake; working drains faster than idle
- **Threshold**: Below 0.3 mover becomes "tired"
- **Recovery**: Standing (very slow) < Chair (moderate) < Bed (fast)

### Hunger
- **Drain**: Steadily over time, regardless of activity
- **Threshold**: Below 0.3 mover becomes "hungry"
- **Recovery**: Eat food item (instant or brief consumption time)

### Priority
Hunger is more urgent than tiredness:
1. If starving (`hunger < 0.1`): interrupt and find food
2. If hungry (`hunger < 0.3`) and no job: seek food
3. If tired (`energy < 0.3`) and no job: seek rest

This is simpler than the full needs-as-interrupts model in needs-vs-jobs.md. Start soft: movers prefer to finish jobs before addressing needs, except when critical.

---

## Freetime Behavior State Machine

```c
typedef enum {
    FREETIME_NONE,          // Working or wandering normally
    FREETIME_SEEKING_FOOD,  // Walking to food source
    FREETIME_EATING,        // Consuming food
    FREETIME_SEEKING_REST,  // Walking to bed/chair
    FREETIME_RESTING,       // Sitting/sleeping, recovering energy
} FreetimeState;
```

### Flow (Hunger)
1. Mover finishes job (or has no job)
2. Check: `hunger < HUNGRY_THRESHOLD`?
3. If yes: find nearest food (stockpile, ground, bush/tree)
4. Walk to food
5. Pick up / harvest if needed
6. Eat (brief timer)
7. When done, check energy or return to normal

### Flow (Energy)
1. Mover finishes job (or has no job), not hungry
2. Check: `energy < TIRED_THRESHOLD`?
3. If yes: find nearest available bed (preferred) or chair
4. Walk to furniture
5. Enter resting state, recover energy
6. When `energy > RESTED_THRESHOLD`, return to normal

---

## Furniture Ownership (Optional, Deferred)

In DF/RimWorld, movers can "own" beds - they prefer their own bed and get mood penalties for sleeping elsewhere.

For now: skip ownership. Any mover can use any bed. Add ownership later when mood system exists.

---

## Integration with Job System

From needs-vs-jobs.md, the key insight: needs bypass WorkGivers.

```c
void UpdateMover(Mover* m) {
    // 1. Check if handling a need already
    if (m->freetimeState != FREETIME_NONE) {
        UpdateFreetimeActivity(m);
        return;
    }
    
    // 2. Critical need: starving interrupts everything
    if (m->hunger < STARVING_THRESHOLD) {
        StartSeekingFood(m);
        return;
    }
    
    // 3. If no job, check non-critical needs
    if (m->currentJobId < 0) {
        if (m->hunger < HUNGRY_THRESHOLD) {
            StartSeekingFood(m);
            return;
        }
        if (m->energy < TIRED_THRESHOLD) {
            StartSeekingRest(m);
            return;
        }
    }
    
    // 4. Normal job/wander logic
    // ...
}
```

Note: We don't interrupt active jobs. Mover finishes job, then rests if tired.

---

## Production: Carpenter's Bench

New workshop to craft furniture.

```
Carpenter's Bench (2x2):
  # X
  O .

Recipes:
  4 ITEM_PLANKS → 1 ITEM_BED        (material preserved)
  2 ITEM_PLANKS → 1 ITEM_CHAIR      (material preserved)
  3 ITEM_PLANKS → 1 ITEM_TABLE      (material preserved)
```

Furniture items are then placed via build menu, creating Furniture entities.

---

## Placement

Similar to workshop placement:
1. Player selects furniture type from build menu
2. Player clicks location
3. System checks: walkable floor, not occupied
4. Creates blueprint (needs materials hauled)
5. Builder places furniture

Or simplified: instant placement if item is nearby/carried.

---

## Rendering

- Beds: horizontal rectangle, tinted by wood material
- Chairs: small square, tinted by wood material
- Tables: medium square, tinted by wood material

Movers using furniture:
- Sleeping: mover sprite changes to horizontal/eyes closed
- Sitting: mover sprite at chair position, maybe slightly lowered

---

## What This Unlocks

1. **Furniture demand** - Reason to craft beyond construction
2. **Energy loop** - Movers need rest, furniture provides it
3. **Colony layout** - Players design bedrooms, dining areas
4. **Future mood** - Sleeping in bed vs floor, eating at table vs ground

---

## Implementation Priority

### Phase 1: Food (Survival Loop)
1. **Berry bush cell** - `CELL_BERRY_BUSH`, terrain gen placement
2. **Food items** - `ITEM_BERRIES`, basic nutrition value
3. **Hunger need** - Drain over time on movers
4. **Foraging behavior** - Seek bush, harvest, eat
5. **Food stockpile filter** - Store gathered food

### Phase 2: Furniture (Comfort Loop)
6. **Furniture entity system** - Struct, array, create/delete
7. **Carpenter's bench workshop** - Recipes for bed/chair/table
8. **Furniture placement** - Build menu integration
9. **Energy need** - Drain over time, recover when resting
10. **Freetime behavior** - Seek and use furniture
11. **Rendering** - Basic sprites, mover-on-furniture states

### Phase 3: Polish
12. **Fruit trees** - Additional food source variety
13. **Eating at tables** - Mood bonus for proper meals
14. **Food on ground** - Dropped fruit, foraging

---

## Open Questions

- Should furniture block movement? (Beds yes, chairs maybe, tables maybe)
- Can movers sleep on ground if no beds? (Yes, but slower recovery)
- Do chairs need to be "at" tables, or standalone?
- Quality system for furniture? (Deferred - affects comfort/recovery rate)
- Berry bush regrowth rate - balance gathering vs sustainability?
- Can movers starve to death? Or just slow down dramatically?
- Seasonal fruit? Or always available for simplicity?

---

## Non-Goals (For Now)

- Full mood system
- Furniture ownership
- Recreation activities (games, art)
- Social needs (conversations)
- Cooking / prepared meals
- Food spoilage
- Farming (planted crops)
