# Feature 08: Containers & Water Carrying

> **Priority**: Tier 3 — Material & Comfort Loop
> **Why now**: Clay's only sink is bricks. Water simulation exists but colonists can't interact with it. No liquid storage.
> **Systems used**: Clay, kiln, water system, cordage
> **Depends on**: F1 (hunger — thirst is the companion need), F4 (tools)
> **Opens**: Irrigation (F6 enhancement), water-dependent crafting, future thirst need

---

## What Changes

**Pottery** at the kiln produces **clay pots** — the first containers. **Baskets** woven from cordage hold dry goods. Containers enable **water carrying** (fill pot at water source, deliver to stockpile/farm) and **bulk storage** (baskets hold more items per stockpile slot). A **thirst** need makes water essential.

Clay goes from "just bricks" to a critical resource. Water goes from decoration to survival necessity.

---

## Design

### Thirst Need

Companion to hunger (F1):

```
thirst: float 0.0 (dehydrated) → 1.0 (hydrated)
Drain rate: ~0.003/sec (faster than hunger — you need water more often)
```

**Thresholds** (same pattern as hunger):
- 1.0–0.3: Hydrated
- 0.3–0.1: Thirsty (seek water when idle)
- <0.1: Dehydrated (interrupt job)

**Water sources** (fallback chain):
1. Water pot in stockpile (ITEM_WATER_POT)
2. Natural water cell (walk to water, drink directly, slow)
3. Rain (exposed cell during rain, very slow)

### Container Items

| Item | Recipe | Holds | Weight | Notes |
|------|--------|-------|--------|-------|
| ITEM_CLAY_POT | Kiln: CLAY × 2 | Liquids (water) | 2 | First container |
| ITEM_WATER_POT | Fill pot at water | Water (drink source) | 3 | Pot + water content |
| ITEM_BASKET | Rope maker: CORDAGE × 3 | Dry goods | 1 | Stockpile efficiency |

### Pottery Recipes (Kiln)

The kiln already exists. Add recipes:

| Recipe | Input | Fuel | Output | Time |
|--------|-------|------|--------|------|
| Fire pot | CLAY × 2 | Yes | ITEM_CLAY_POT × 1 | 6s |
| Fire large pot | CLAY × 4 | Yes | ITEM_LARGE_POT × 1 | 8s |

### Water Carrying Job

New job: JOBTYPE_FETCH_WATER

1. Mover picks up empty ITEM_CLAY_POT from stockpile
2. Walks to nearest water cell (water level > 0)
3. "Fill" action (2s) → pot becomes ITEM_WATER_POT
4. Delivers ITEM_WATER_POT to stockpile
5. Water pot is consumed when mover drinks from it → empty pot remains

This creates a **water logistics chain**: pottery → fill → stockpile → drink. If the water source is far, you need dedicated water carriers.

### Drinking Behavior

Extend freetime from F1:

```
Priority: Dehydrated > Starving > Thirsty > Exhausted > Hungry > Tired
```

When seeking water:
1. ITEM_WATER_POT in stockpile (best — quick, nearby)
2. Natural water cell (walk to water, 3s to drink)
3. Nothing → mover gets progressively slower

### Basket Stockpile Enhancement

Baskets don't hold liquids but improve dry goods storage:
- A stockpile slot with a basket holds **2× items** per slot
- Baskets are placed in stockpiles as infrastructure, not consumed
- Visual: stockpile slots with baskets look fuller

This is a simple quality-of-life improvement that makes cordage more valuable.

---

## Implementation Phases

### Phase 1: Thirst Need
1. Add `float thirst` to Mover struct (init 1.0)
2. Drain thirst each tick (faster than hunger)
3. Extend freetime priorities
4. Drinking at natural water: walk to water cell, 3s action, restore thirst
5. **Test**: Thirst drains, mover seeks water, drinks at water source

### Phase 2: Clay Pots
1. Add ITEM_CLAY_POT, ITEM_WATER_POT
2. Add kiln recipes for pottery
3. Pot states: empty pot → water pot (filled) → empty pot (after drinking)
4. **Test**: Kiln produces pots, pots can be "filled" conceptually

### Phase 3: Water Carrying
1. Add JOBTYPE_FETCH_WATER
2. Mover picks up empty pot → walks to water → fills → delivers
3. Water pot in stockpile serves as drink source
4. Drinking from pot: restores thirst, pot becomes empty again
5. WorkGiver assigns water-fetching when stockpile has empty pots and few water pots
6. **Test**: Full water carrying loop works

### Phase 4: Baskets
1. Add ITEM_BASKET (crafted at rope maker from cordage)
2. Basket placement in stockpile slots
3. 2× capacity per slot with basket
4. **Test**: Basket doubles stockpile slot capacity

### Phase 5: Water Infrastructure
1. Movers prefer water pots over walking to water
2. Colony needs water carrier role (manual or auto-assigned)
3. Distance to water source creates settlement strategy
4. **Test**: Movers drink from stockpiled water pots first

---

## Connections

- **Uses clay**: Currently only → bricks. Now also → pots (huge new sink)
- **Uses kiln**: Existing workshop, new pottery recipes
- **Uses water system**: Water cells become a resource, not just decoration
- **Uses cordage**: Basket crafting at rope maker
- **Uses hunger system (F1)**: Thirst uses same freetime architecture
- **Opens for F6 (enhancement)**: Water pots for manual irrigation
- **Opens for future**: Cooking with water (soup, porridge), brewing, water-dependent crafts
- **Opens for AI discussion**: Waste loop needs water for sanitation

---

## Design Notes

### Why Pots Before Buckets?

Clay pots are the simplest container — clay already exists, kiln already exists. Wooden buckets need planks + metal bands (too advanced). Pots feel right for the stone-age tech level.

### Water Pot as Item State

ITEM_WATER_POT is a separate item type from ITEM_CLAY_POT (not a "filled" flag). This is simpler: the pot item is consumed/transformed. When you drink, ITEM_WATER_POT → ITEM_CLAY_POT (DeleteItem + SpawnItem). Clean and explicit.

### Settlement Near Water

This feature creates the first **settlement location pressure**. Build near water = short water-carrying trips. Build far = need many pots and dedicated water carriers. This is a meaningful strategic choice that emerges naturally.

---

## Save Version Impact

- New Mover field: thirst
- New items: ITEM_CLAY_POT, ITEM_WATER_POT, ITEM_LARGE_POT, ITEM_BASKET
- New job: JOBTYPE_FETCH_WATER
- Kiln and rope maker recipe additions
- Stockpile basket tracking (per-slot flag or item reference)

## Test Expectations

- ~35-45 new assertions
- Thirst drain and threshold behavior
- Kiln pottery recipes
- Water carrying job (pick up → fill → deliver)
- Drinking behavior (pot > natural water)
- Pot state transformation (water pot → empty pot)
- Basket stockpile capacity bonus
