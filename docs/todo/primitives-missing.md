# Primitive Ingredients Missing (Survival‑Build Enablers)

Date: 2026-02-07
Scope: Identify the *smallest set* of primitive items/cells needed to unlock early survival builds, grounded in the current codebase.

## Current Baseline (Already in Code)
**Items**: logs, planks, sticks, leaves, dirt, clay, gravel, sand, peat, bricks, charcoal, ash.
**Cells**: sapling, tree trunk/branch/root/leaves/felled.

This means **sticks already exist** (`ITEM_STICKS`) and can be used as early fuel or light construction input.

---

## Missing Primitives (High Leverage)
These are the minimum additions that unlock many survival constructions.

### 1) Cordage / Vines (Lashing)
- **Why**: required for tying frames, racks, mats, early tools.
- **Use cases**: lean‑to frame, drying rack, reed pipe connections.

### 2) Dried Grass / Thatch
- **Why**: needed for roofing, bedding, tinder, insulation.
- **Use cases**: thatch roof, sleeping mat, fire starting.

### 3) Poles / Young Trunk (Intermediate wood)
- **Why**: bridges the gap between saplings and full logs.
- **Use cases**: frames, scaffolds, wattle walls, racks, fences.

**Explicit proposal**:
- Add `CELL_TREE_YOUNG` as a growth stage: `SAPLING → YOUNG → TRUNK`.
- Add `ITEM_POLE` harvested from `CELL_TREE_YOUNG` (early structural wood).

### 4) Bark / Resin (Primitive glue + waterproofing)
- **Why**: enables early adhesives and weatherproofing.
- **Use cases**: lash reinforcement, shingles, torch/roof sealant.

### 5) Mud / Cob Mix (Soil + Water)
- **Why**: base material for daub walls, kilns, hearths.
- **Use cases**: wattle‑and‑daub walls, kiln shell, firepit lining.

### 6) Reeds / Hollow Grass
- **Why**: acts like primitive tubing and weaving material.
- **Use cases**: reed pipes, blow‑pipe, mats, screens.

### 7) Flat Stone / Grinding Stone
- **Why**: essential for early tool improvement and food processing.
- **Use cases**: quern/hand‑mill, sharpening slabs, fire ring.

### 8) Simple Container (basket or clay pot)
- **Why**: enables water hauling and storage.
- **Use cases**: water transport, cooking prep, seed storage.

---

## “Sticks” Clarification
You already have **sticks** as `ITEM_STICKS` (from the sawmill). If you want *early‑game* sticks without a sawmill, consider:
- **Spawn sticks** from tree leaf/felled interactions, or
- **Add “snap branch”** as a bare‑hands gather action producing `ITEM_STICKS`.

This preserves the early survival loop without introducing a new item.

---

## Minimal Survival Build Set (Once Added)
- **Shelter**: poles + cordage + thatch (+ mud for better tier)
- **Fire**: hearth + tinder + fuel
- **Water handling**: spring/drain or reed pipe + container
- **Storage**: basket/pot

---

## Suggested First Additions
If you only add a few right now, pick:
1. **Dried Grass**
2. **Cordage/Vines**
3. **Poles (young trunk)**

These three unlock most early survival builds with the least new systems.
