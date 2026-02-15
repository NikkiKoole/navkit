# Feature 02: Sleep & Furniture

> **Priority**: Tier 1 — Survival Loop
> **Why now**: Colonists never rest. Buildings have no interior purpose. Planks have one sink (construction).
> **Systems used**: Sawmill (planks), construction system, day/night cycle
> **Depends on**: Feature 1 (freetime state machine)
> **Opens**: Buildings need rooms (F3), furniture needs tools to craft faster (F4)

---

## What Changes

Movers get an **energy** stat that drains while awake (faster when working). When exhausted, they seek rest. **Furniture** — beds, chairs, tables — are new craftable entities that provide rest at different rates. A **carpenter's bench** workshop turns planks into furniture.

This gives buildings purpose: a room with beds is now worth building.

---

## Design

### Energy Mechanic

```
energy: float 0.0 (exhausted) → 1.0 (rested)
Drain rate (idle):  ~0.001/sec
Drain rate (working): ~0.002/sec (jobs drain energy faster)
Recovery rate: bed 0.005/sec, chair 0.002/sec, ground 0.001/sec
```

**Thresholds:**
- 1.0–0.3: **Rested** — normal work
- 0.3–0.1: **Tired** — seek rest when idle
- <0.1: **Exhausted** — interrupt job, seek rest immediately

### Freetime Extension

Add to the existing freetime state machine from F1:

```
States: SEEKING_FOOD, EATING, SEEKING_REST, RESTING
Priority: Starving > Exhausted > Hungry > Tired
```

Hunger and energy interact:
- Hunger drains even during sleep → long naps make you wake hungry
- Starving interrupts sleep → mover wakes to eat, returns to bed

### Furniture Entities

New entity type: **Furniture**. Not items (can't be carried once placed), not cells (don't replace terrain).

| Furniture | Size | Craft Input | Rest Rate | Notes |
|-----------|------|-------------|-----------|-------|
| Bed | 1×1 | 4 planks | 0.005/s | Primary rest, mover lies down |
| Chair | 1×1 | 2 planks | 0.002/s | Secondary rest, future: eating bonus |
| Table | 1×1 | 3 planks | — | No rest, future: eating at table mood bonus |

Furniture is placed like construction blueprints, crafted at the **carpenter's bench**.

### Carpenter's Bench (New Workshop)

```
Template (3×3):
. O .
# X #
. . .
```

**Recipes:**
| Recipe | Input | Output | Work Time |
|--------|-------|--------|-----------|
| Craft Bed | PLANKS × 4 | Furniture: Bed | 8s |
| Craft Chair | PLANKS × 2 | Furniture: Chair | 5s |
| Craft Table | PLANKS × 3 | Furniture: Table | 6s |

Output is a **furniture item** (IF_BUILDING_MAT equivalent) that gets placed via construction blueprint, similar to how blocks become walls.

### Sleep Behavior

When seeking rest:
1. Find closest **unoccupied bed** (reserved system like workshops)
2. If no bed: find closest **chair**
3. If no chair: sleep on ground at current position
4. Walk to rest spot, enter RESTING state
5. Energy recovers at furniture-determined rate
6. Wake when energy > 0.8 (or interrupted by starvation)

### Day/Night Integration

The time system already tracks hours. Optional: energy drain increases at night (natural tiredness), making sleep align with day/night. Not required for Phase 1 — the drain rate alone creates a work/rest cycle.

---

## Implementation Phases

### Phase 1: Energy Stat
1. Add `float energy` to Mover struct (init 1.0)
2. Drain energy each tick (faster when job active)
3. Extend freetime state machine with SEEKING_REST, RESTING
4. Priority logic: starving > exhausted > hungry > tired
5. Bump save version
6. **Test**: Energy drains, thresholds trigger state changes

### Phase 2: Furniture Entity System
1. Define Furniture struct: type, position (x,y,z), occupant (mover idx or -1)
2. Furniture pool (similar to items/workshops: array + count)
3. Furniture placement via construction system
4. Walkability: furniture cells remain walkable (mover stands/lies on them)
5. Rendering: sprite per furniture type
6. Save/load furniture array
7. **Test**: Furniture can be placed, occupant tracking works

### Phase 3: Carpenter's Bench
1. Add WORKSHOP_CARPENTER to workshop types
2. Define template and recipes
3. Output: ITEM_BED, ITEM_CHAIR, ITEM_TABLE (furniture items)
4. These items get placed via blueprint system → spawn Furniture entity
5. **Test**: Crafting produces furniture items, placement spawns entity

### Phase 4: Rest Behavior
1. SEEKING_REST: search for unoccupied bed → chair → ground
2. Reserve furniture (occupant field)
3. Walk to furniture, enter RESTING
4. Energy recovers at furniture rate
5. Wake at 0.8 energy or starvation interrupt
6. Release furniture reservation
7. **Test**: Mover finds bed, sleeps, energy recovers, wakes, returns to work

### Phase 5: Hunger/Sleep Interaction
1. Hunger drains during sleep
2. Starving interrupts RESTING state
3. After eating, mover re-seeks bed if still tired
4. **Test**: Sleeping mover wakes when starving, eats, returns to bed

---

## Connections

- **Uses sawmill**: Planks are the input for all furniture
- **Uses construction**: Furniture placed via blueprint system
- **Uses F1 freetime**: Extends the state machine, shares priority logic
- **Opens for F3 (Shelter)**: Beds in sheltered rooms could give bonus rest
- **Opens for F4 (Tools)**: Carpenter's bench benefits from tool quality
- **Opens for F10 (Personality)**: Bed ownership, comfort preferences

---

## Save Version Impact

- New Mover field: energy
- New entity type: Furniture (position, type, occupant)
- New workshop: WORKSHOP_CARPENTER
- New items: ITEM_BED, ITEM_CHAIR, ITEM_TABLE
- New furniture types enum

## Test Expectations

- ~30-40 new assertions
- Energy drain rates (idle vs working)
- Freetime priority (hunger vs energy)
- Furniture placement and occupant tracking
- Rest behavior (bed vs chair vs ground rates)
- Carpenter's bench recipes
- Sleep interruption by hunger
