# Primitive Ingredients Missing (Survival-Build Enablers)

Date: 2026-02-07
Updated: 2026-02-12
Scope: Identify the *smallest set* of primitive items/cells needed to unlock early survival builds, grounded in the current codebase.

## Current Baseline (Already in Code)
**Items**: logs, planks, sticks, leaves, dirt, clay, gravel, sand, peat, bricks, charcoal, ash, bark, stripped log, short string, cordage, dried grass, poles, saplings (unified with material).
**Cells**: sapling, tree trunk/branch/root/leaves/felled.
**Workshops**: Stonecutter, Sawmill, Kiln, Charcoal Pit, Hearth, Drying Rack, Rope Maker.

---

## Primitives Status

### 1) Cordage / Vines (Lashing) - DONE
- **ITEM_SHORT_STRING** and **ITEM_CORDAGE** implemented (save v35).
- Rope Maker workshop: Twist Bark, Twist Grass, Braid Cordage recipes.
- **Still needed**: cordage has no *sink* yet -- construction staging would use it.

### 2) Dried Grass / Thatch - DONE
- **ITEM_DRIED_GRASS** implemented. Drying Rack (passive) converts ITEM_GRASS.
- Used as Rope Maker input (Twist Grass -> string).
- **Still needed**: thatch as roofing/construction material.

### 3) Poles - DONE
- **ITEM_POLES** implemented. Harvested from tree branches via tree harvest system.
- Branch cells use thin per-species sprites. Tree trunk taper at canopy.
- Note: implemented differently than proposed (from branches, not young trunk growth stage).

### 4) Bark - DONE
- **ITEM_BARK** and **ITEM_STRIPPED_LOG** implemented (save v33).
- Sawmill recipe: Strip Bark (LOG -> STRIPPED_LOG x1 + BARK x2).
- Bark feeds into Rope Maker -> string -> cordage chain.
- Resin not implemented (deferred).

### 5) Mud / Cob Mix (Soil + Water) - TODO
- **Why**: base material for daub walls, kilns, hearths.
- **Use cases**: wattle-and-daub walls, kiln shell, firepit lining.
- **Depends on**: water-dependent crafting system (see water-dependent-crafting.md).

### 6) Reeds / Hollow Grass - TODO
- **Why**: acts like primitive tubing and weaving material.
- **Use cases**: reed pipes, blow-pipe, mats, screens.

### 7) Flat Stone / Grinding Stone - TODO
- **Why**: essential for early tool improvement and food processing.
- **Use cases**: quern/hand-mill, sharpening slabs, fire ring.

### 8) Simple Container (basket or clay pot) - TODO
- **Why**: enables water hauling and storage.
- **Use cases**: water transport, cooking prep, seed storage.
- **Depends on**: container/inventory system [future:containers].

---

## "Sticks" Clarification
**Sticks** exist as `ITEM_STICKS` (from the sawmill, Cut Sticks recipe). Also harvestable from tree branches alongside poles via the tree harvest system. Early-game sticks available without a sawmill.

---

## Minimal Survival Build Set (Status)
- **Shelter**: poles (done) + cordage (done) + thatch (dried grass exists, thatch roofing not yet)
- **Fire**: hearth (done) + tinder + fuel (done)
- **Water handling**: spring/drain (functions exist, UI not yet) + container (not yet)
- **Storage**: basket/pot (not yet)

---

## Remaining Priorities
The original top 3 picks (dried grass, cordage, poles) are **all done**. Next priorities:
1. **Construction staging** -- gives cordage a purpose (lashing for frames)
2. **Mud/cob** -- requires water-dependent crafting system
3. **Containers** -- requires inventory system (larger effort)
