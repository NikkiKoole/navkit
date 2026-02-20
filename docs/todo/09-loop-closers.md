# Feature 09: Glass, Compost & Loop Closers

> **Priority**: Tier 3 — Material & Comfort Loop
> **Why now**: Sand, leaves, ash, and poles have NO sinks. Every other item has a closed source→sink loop. These are the last open ends.
> **Systems used**: Kiln, hearth, farming (F6), existing item chains
> **Depends on**: F6 (farming — composting feeds farming)
> **Opens**: Complete autarky — every material has purpose

---

## What Changes

Four open material loops get closed:

1. **Sand → Glass kiln → Glass** (windows, bottles, future optics)
2. **Leaves → Compost bin → Fertilizer** (boosts farming)
3. **Ash → Lye/Cement** (mortar for construction, soap for future hygiene)
4. **Poles → Construction** (scaffolding, lean-tos, fences)

After this feature, every item in the game has at least one source and one sink. Full autarky.

---

## Design

### 1. Glass Kiln (Sand Sink)

Sand is mined but has zero uses. Glass is historically one of humanity's most important materials.

**Glass Kiln** — variant of existing kiln, or new recipe at kiln:

| Recipe | Input | Fuel | Output | Time |
|--------|-------|------|--------|------|
| Smelt glass | SAND × 3 | Yes | ITEM_GLASS × 1 | 8s |

**ITEM_GLASS** sinks:
- **Windows** (construction recipe): Glass + planks → window cell
  - Window: blocks rain, allows light, partial wind reduction
  - Works with shelter system (F3) — rooms with windows are enclosed
- **Glass bottle** (future): container for liquids, potions, medicine
- **Decorative** (future): glass panes as furniture, mood bonus

For now, just glass → windows gives sand its purpose.

### 2. Compost Bin (Leaves Sink)

Leaves pile up from tree gathering with nowhere to go. Composting turns waste into farming fuel.

**Compost Bin** (2×2, passive workshop like drying rack):
```
X .
# .
```

| Recipe | Input | Output | Passive Time |
|--------|-------|--------|-------------|
| Compost leaves | LEAVES × 4 | ITEM_FERTILIZER × 1 | 90s |
| Compost mixed | LEAVES × 2 + GRASS × 2 | ITEM_FERTILIZER × 2 | 60s |

**ITEM_FERTILIZER** use:
- Apply to farm plot (new designation: DESIGNATION_FERTILIZE)
- Mover carries fertilizer to farm cell, applies it
- Fertilized cell: crop growth rate × 1.5 for one growth cycle
- After harvest, fertilizer is consumed → need to reapply

This creates a loop: trees → leaves → compost → fertilizer → faster crops → more food.

### 3. Ash Uses (Ash Sink)

The hearth burns fuel and produces ash with no purpose. Historically, ash is incredibly useful.

**Lye production** (at hearth or new simple workshop):

| Recipe | Input | Output | Time |
|--------|-------|--------|------|
| Make lye | ASH × 2 + WATER_POT × 1 | ITEM_LYE × 1 + CLAY_POT × 1 | 4s |

**ITEM_LYE** sinks:
- **Cement/mortar** recipe: LYE × 1 + SAND × 1 → ITEM_MORTAR × 2
  - Mortar used in advanced stone construction (stronger walls, future)
- **Soap** (future, pairs with hygiene system from AI discussions)
- **Fertilizer boost**: LYE × 1 + compost → better fertilizer

For Phase 1: ash → lye → mortar → construction. Simple, effective.

### 4. Poles (Poles Sink)

Poles come from chopping branches but have no use.

**Construction recipes using poles:**

| Recipe | Input | Result | Notes |
|--------|-------|--------|-------|
| Fence | POLES × 2 | Fence cell | Blocks animals, not movers (or vice versa) |
| Lean-to | POLES × 4 + GRASS × 2 | Lean-to structure | Basic shelter (partial rain protection) |
| Scaffolding | POLES × 3 | Scaffold cell | Temporary construction aid |
| Drying frame | POLES × 2 | Drying rack variant | Alternative drying rack material |

The **fence** is the most immediately useful: keep animals out of farms (farming system from F6). Poles → fences closes the loop and creates gameplay value.

---

## Implementation Phases

### Phase 1: Glass (Sand Sink)
1. Add kiln recipe: sand → glass
2. Add ITEM_GLASS
3. Add window construction recipe: glass + planks → CELL_WINDOW
4. Window properties: blocks rain, allows light, partial wind block
5. Integrate with room detection (F3) — windows count as enclosure
6. **Test**: Sand → glass → window. Window blocks rain, passes light.

### Phase 2: Composting (Leaves Sink)
1. Add WORKSHOP_COMPOST_BIN (2×2 passive)
2. Add composting recipes (leaves → fertilizer)
3. Add ITEM_FERTILIZER
4. Add DESIGNATION_FERTILIZE and JOBTYPE_FERTILIZE
5. Fertilized farm cells grow 1.5× faster
6. **Test**: Leaves composted, fertilizer applied to farm, growth bonus confirmed

### Phase 3: Lye & Mortar (Ash Sink)
1. Add ITEM_LYE, ITEM_MORTAR
2. Add lye recipe at hearth (ash + water pot → lye + empty pot)
3. Add mortar recipe (lye + sand → mortar)
4. Mortar as construction material (future: stronger walls, or just a building material)
5. **Test**: Ash → lye → mortar production chain works

### Phase 4: Pole Uses
1. Add fence construction recipe (poles)
2. Fence cell type: blocks animal pathfinding, not movers (or configurable)
3. Add lean-to construction (basic partial shelter)
4. **Test**: Fences block animals, lean-to provides partial shelter

### Phase 5: Autarky Audit
1. Verify every ITEM_* type has at least one source and one sink
2. Update BACKLOG.md material loop table — all should be ✅
3. Document any remaining edge cases
4. **Test**: Material loop completeness check

---

## Material Loop Closures (Summary)

| Item | Was | Now |
|------|-----|-----|
| SAND | Source only (mining) | → Glass → Windows |
| LEAVES | Source only (gathering) | → Compost → Fertilizer → Farming |
| ASH | Source only (hearth) | → Lye → Mortar → Construction |
| POLES | Source only (chopping) | → Fences, Lean-tos, Scaffolding |

---

## Connections

- **Uses kiln**: Glass recipe (existing workshop, new recipe)
- **Uses hearth**: Lye recipe (existing workshop, new recipe)
- **Uses farming (F6)**: Fertilizer boosts crop growth
- **Uses containers (F8)**: Lye needs water pot
- **Uses animal system**: Fences keep animals out of farms
- **Completes autarky**: Every material now has purpose
- **Opens for future**: Glass bottles (medicine), soap (hygiene), mortar (advanced construction)

---

## Design Notes

### Glass Is Important

Glass seems like a luxury but it's foundational: windows make buildings better (light + shelter), bottles store liquids, and later glass is needed for optics, medicine containers, etc. It's one of the most impactful material unlocks.

### Composting Is The Farming Flywheel

Without fertilizer, farms deplete over time (future soil depletion mechanic). Composting creates a virtuous cycle: more trees → more leaves → more compost → better farms → more food → more workers → more trees. This is the autarkic loop at its best.

### Lye Is The Chemical Bootstrap

Lye (wood ash + water) is historically the first "chemical" people made. It enables soap, mortar, tanning improvement, and eventually gunpowder precursors. Even if most of those are future features, establishing lye now creates the foundation.

---

## Save Version Impact

- New items: ITEM_GLASS, ITEM_FERTILIZER, ITEM_LYE, ITEM_MORTAR
- New cell types: CELL_WINDOW, CELL_FENCE (or cell flags)
- New workshop: WORKSHOP_COMPOST_BIN
- New job/designation: JOBTYPE_FERTILIZE, DESIGNATION_FERTILIZE
- Kiln, hearth recipe additions

## Test Expectations

- ~30-40 new assertions
- Glass smelting and window properties
- Composting and fertilizer application
- Lye and mortar production chains
- Fence animal-blocking behavior
- Autarky completeness audit
