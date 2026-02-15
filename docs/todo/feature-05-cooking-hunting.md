# Feature 05: Cooking & Hunting

> **Priority**: Tier 2 — Tool & Production Loop
> **Why now**: Animals exist but are decoration. Fire/hearth exist but produce nothing edible. Hunger exists (F1) but only berries feed colonists.
> **Systems used**: Animals (3 behavior types), fire system, hearth workshop, hunger system (F1), tools (F4)
> **Depends on**: F1 (hunger), F4 (tools — butchering needs cutting)
> **Opens**: Farming pressure (F6 — hunting doesn't scale), leather for clothing (F7)

---

## What Changes

**Hunting** lets movers kill animals for **raw meat**. A **campfire** (simple cooking spot) or the existing **hearth** turns raw meat into **cooked meat**, which restores more hunger. Animals become a food source, fire becomes useful for survival, and a **butchering** step connects the kill to the kitchen.

The loop: hunt → butcher → cook → eat. Each step uses an existing system.

---

## Design

### Hunting Job

New job type: JOBTYPE_HUNT

1. Mover assigned to hunt a target animal (designation or auto-assignment)
2. Mover walks toward animal
3. When adjacent, "attack" action (2s with cutting tool, 4s bare-handed)
4. Animal dies → becomes **carcass** (item on ground: ITEM_CARCASS)
5. Mover returns to other work

**Animal death**: Animal entity deactivated. ITEM_CARCASS spawned at animal position. Carcass has material type matching animal species.

### Butchering

New recipe at **knapping spot** (F4) or a **butcher spot** (1×1):

| Recipe | Input | Output | Tool | Time |
|--------|-------|--------|------|------|
| Butcher | ITEM_CARCASS × 1 | ITEM_RAW_MEAT × 3 + ITEM_HIDE × 1 | Cutting: 2s (4s bare) | |

Butchering at a designated spot, not a full workshop. Primitive and quick.

### Cooking

Two cooking methods:

**Campfire (new, simple)**:
- 1×1 designation on any cell with active fire
- Recipe: ITEM_RAW_MEAT → ITEM_COOKED_MEAT
- Passive: place meat near fire, it cooks over time (like drying rack pattern)
- Simple, no workshop building needed

**Hearth (existing workshop)**:
- Add cooking recipe: ITEM_RAW_MEAT × 2 → ITEM_COOKED_MEAT × 2
- Requires fuel (already supports fuel)
- Faster and batch-capable

### New Items

| Item | Flags | Stack | Hunger | Notes |
|------|-------|-------|--------|-------|
| ITEM_CARCASS | IF_SPOILS | 1 | — | Must be butchered quickly |
| ITEM_RAW_MEAT | IF_EDIBLE, IF_SPOILS, IF_STACKABLE | 5 | +0.2 | Edible raw but poor |
| ITEM_COOKED_MEAT | IF_EDIBLE, IF_STACKABLE | 5 | +0.5 | Best food so far |
| ITEM_HIDE | IF_STACKABLE | 5 | — | Future: leather (F7) |

### Food Quality Hierarchy

| Food | Hunger Restored | Spoils? | Notes |
|------|----------------|---------|-------|
| Raw berries | +0.3 | Yes (slow) | Easy to get, seasonal |
| Dried berries | +0.25 | No | Preserved, winter food |
| Raw meat | +0.2 | Yes (fast) | Emergency only |
| Cooked meat | +0.5 | Yes (slow) | Best early food |

Cooking is always worth it: raw meat gives +0.2 (and spoils fast), cooked gives +0.5 (spoils slower). This creates a real incentive to maintain a cooking operation.

### Hunting Pressure & Animal Ecology

- Animals already have grazer/predator behaviors
- Hunting reduces animal population
- Animals reproduce slowly (or new ones wander in from map edge)
- Over-hunting depletes local wildlife → pressure to develop farming (F6)
- Predators are dangerous to hunt (future: can injure movers)

### Hunt Designation

Two modes:
1. **Designate animal**: Click specific animal to mark for hunting
2. **Hunt zone**: Designate area — any animal entering zone is fair game

---

## Implementation Phases

### Phase 1: Animal Death & Carcass
1. Add `Kill(animalIdx)` function — deactivates animal, spawns ITEM_CARCASS
2. Add ITEM_CARCASS with IF_SPOILS
3. Carcass material = animal species (for future variety)
4. **Test**: Killing animal produces carcass at correct position

### Phase 2: Hunting Job
1. Add JOBTYPE_HUNT
2. Add DESIGNATION_HUNT (per-animal or zone-based)
3. Hunt job: walk to animal → attack action (tool-speed dependent)
4. On completion: Kill(animal)
5. WorkGiver for hunting assignment
6. **Test**: Mover hunts designated animal, carcass appears

### Phase 3: Butchering
1. Add ITEM_RAW_MEAT, ITEM_HIDE
2. Add butcher recipe at knapping spot (or new 1×1 butcher spot)
3. Butcher: CARCASS → RAW_MEAT × 3 + HIDE × 1
4. Requires QUALITY_CUTTING (tool speed bonus)
5. **Test**: Butchering carcass produces meat and hide

### Phase 4: Cooking
1. Add ITEM_COOKED_MEAT
2. Add cooking recipe to hearth: RAW_MEAT → COOKED_MEAT
3. Campfire cooking: passive recipe near active fire cells
4. **Test**: Raw meat becomes cooked meat at hearth

### Phase 5: Eating Integration
1. Cooked meat satisfies hunger at +0.5 (best food)
2. Movers prefer cooked > raw > berries in fallback chain
3. Spoilage: carcass spoils fast (60s), raw meat medium (120s), cooked slow (300s)
4. **Test**: Mover eats cooked meat, hunger restores correctly

---

## Connections

- **Uses animals**: Currently decorative — now a food source
- **Uses fire system**: Campfire cooking uses existing fire cells
- **Uses hearth**: Existing workshop gets cooking recipe
- **Uses tools (F4)**: Hunting and butchering speed depends on cutting quality
- **Uses hunger (F1)**: Cooked meat is the best hunger source
- **Opens for F6 (Farming)**: Hunting doesn't scale — over-hunting pushes toward agriculture
- **Opens for F7 (Clothing)**: ITEM_HIDE → leather → clothing
- **Opens for F9 (Loop Closers)**: Animal bones (future) as crafting material

---

## Design Notes

### Why Not a Full Kitchen Workshop?

A dedicated kitchen workshop comes later with farming (when you have diverse ingredients). For now, the hearth (already exists) handles cooking, and campfire cooking is the primitive option. Keep it simple — a stick with meat over fire.

### Animal Reproduction

Don't implement breeding yet. Simple respawn: new animals wander in from map edges at a slow rate. If population drops below threshold, increase spawn rate. This gives the feel of wildlife without complex ecology.

### Spoilage Timer

IF_SPOILS items track time since creation. When spoilage timer expires, item is deleted (or turns into ITEM_ROT for future composting). Spoilage is the pressure that makes preservation (drying) and cooking valuable.

---

## Save Version Impact

- New items: ITEM_CARCASS, ITEM_RAW_MEAT, ITEM_COOKED_MEAT, ITEM_HIDE
- New job type: JOBTYPE_HUNT
- New designation: DESIGNATION_HUNT
- Spoilage timer field on Item struct (float, seconds since spawn)
- Hearth recipe addition

## Test Expectations

- ~35-45 new assertions
- Animal death → carcass spawning
- Hunting job execution
- Butchering recipe (carcass → meat + hide)
- Cooking recipe (raw → cooked)
- Hunger restoration values
- Spoilage timer progression
- Food preference ordering in fallback chain
