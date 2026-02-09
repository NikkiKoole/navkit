# Workshop Roadmap (Autarky‑Scale, Primitive → Industrial)

Date: 2026-02-07
Scope: Propose the next workshops based on the existing codebase and the “primitive technology” progression. Focus on small, composable workshops rather than monolithic buildings.

## Current Baseline (Already in Code)
- **Stonecutter**: `ITEM_ROCK → ITEM_BLOCKS`
- **Sawmill**: `ITEM_LOG → ITEM_PLANKS / ITEM_STICKS`
- **Kiln**: `ITEM_CLAY → ITEM_BRICKS`, `ITEM_LOG → ITEM_CHARCOAL`

These three already form the first autarky loops for stone, wood, and clay.

---

## Immediate Next Workshops (Minimal New Systems)
These can be added with today’s systems and items.

### 1) Hearth / Firepit
- **Inputs**: Any `IF_FUEL` items (logs, sticks, leaves, peat, charcoal)
- **Outputs**: Heat + smoke (optional ash later)
- **Why**: Uses existing fire/temperature/smoke systems and provides a real fuel sink. It is the foundation for cooking later.

### 2) Glass Kiln / Glassblower
- **Inputs**: `ITEM_SAND` + fuel
- **Outputs**: `ITEM_GLASS` (or glass panes)
- **Why**: Natural next step from clay → bricks. Uses existing sand and kiln‑style fuel logic.

### 3) Charcoal Pit (primitive alternative)
- **Inputs**: `ITEM_LOG` / `ITEM_STICKS`
- **Outputs**: `ITEM_CHARCOAL`
- **Why**: Provides an early charcoal path without relying on the kiln. Fits “primitive tech” flavor.

---

## Near‑Term Workshops (Require One Small New Item/System)
These align with the primitive tech doc but need a small new ingredient or system.

### 4) Pottery Wheel
- **Needs**: `ITEM_POTTERY`
- **Why**: Completes the clay loop; pottery becomes a container / trade good later.

### 5) Bloomery / Primitive Smelter
- **Needs**: `ITEM_IRON_ORE` (or “iron bacteria sludge”)
- **Outputs**: `ITEM_IRON_BLOOM` → bars later
- **Why**: First iron milestone in the primitive chain.

### 6) Tanner’s Rack
- **Needs**: `ITEM_HIDE` (animal system later)
- **Outputs**: leather strips / hides
- **Why**: Enables tools and later clothing loops.

### 7) Loom / Weaver
- **Needs**: `ITEM_FIBER` (flax/reeds)
- **Outputs**: cloth / rope
- **Why**: Supports tool progression and shelter upgrades.

### 8) Hand Mill / Quern
- **Needs**: grain item (basic farming)
- **Outputs**: flour
- **Why**: Key mid‑step for any food chain; fits multi‑tile bakery loop.

---

## Correct‑Scale “Bakery” Chain (Multi‑Tile)
If baking exists, it should be a **chain**, not a single building:
1. **Grain Field** (crop tile)
2. **Threshing Floor** (grain → usable grain)
3. **Hand Mill / Quern** (grain → flour)
4. **Hearth / Oven** (flour → bread)

This keeps scale consistent and aligns with autarky philosophy.

---

## Suggested Implementation Order
1. Hearth / Firepit
2. Glass Kiln
3. Charcoal Pit (optional)
4. Pottery Wheel
5. Bloomery / Primitive Smelter
6. Loom / Weaver
7. Tanner’s Rack
8. Hand Mill / Quern

---

## Open Questions
- Should Hearth be a **workshop** or a **single‑tile structure**?
- Do we want Glass as a **new item** or as a **material variant** of existing blocks?
- Do we prefer “iron bacteria sludge” (biome‑tied) or classic ore nodes?

