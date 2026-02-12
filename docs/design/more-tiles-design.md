# Design Doc: Next Tiles for Autarky-Scale Progression

Date: 2026-02-07
Scope: Propose the *next* placeable tiles/structures based on current code capabilities and the autarky philosophy. Avoid oversized “bakery”‑style buildings; favor smaller, composable tiles that form loops.

## Goals
- Add tiles that **consume existing items** and **activate existing systems**.
- Keep scale consistent: small, composable tiles that combine into larger production chains.
- Prefer loops that close themselves (autarky) rather than add dead‑end resources.

## Non‑Goals
- Full farming, animals, or large civic buildings.
- Single mega‑buildings that collapse multiple steps (e.g., “Bakery”).

---

## Current System Anchors (Code Reality)
**Terrain cells**: `DIRT/CLAY/GRAVEL/SAND/PEAT/ROCK`, trees, ramps/ladders.
**Items**: logs, planks, sticks, bricks, charcoal, blocks, peat, leaves.
**Workshops**: Stonecutter, Sawmill, Kiln.
**Systems**: water (sources/drains), fire/smoke/steam, temperature, ground wear.

These are the anchors for the next tiles.

---

## Proposed Next Tiles (Ordered by Value)

### 1) Hearth / Firepit (Tile)
**Why**: Consumes existing fuels (logs, sticks, leaves, peat, charcoal) and uses the fire/temperature/smoke systems you already have.
**Autarky role**: Fuel sink + heat source; later enables cooking, pottery, metalwork sequences.
**Dependencies**: None new (uses `IF_FUEL` items and fire system).

### 2) Water Source (Spring) Tile
**Why**: You already support `SetWaterSource`; this makes small maps and rivers reliable without manual sandboxing.
**Autarky role**: Stable input of water; enables irrigation and temperature control.
**Dependencies**: None new.

### 3) Water Drain (Outflow/Grate) Tile
**Why**: You already support `SetWaterDrain`; needed to control floods and close water loops.
**Autarky role**: Waste/water management foundation.
**Dependencies**: None new.

### 4) Constructed Floors (Material‑based)
**Tile set**: Brick floor, Plank floor, Block floor.
**Why**: You already have `HAS_FLOOR` + `floorMaterial`. This creates real sinks for bricks/planks/blocks.
**Autarky role**: Durable infrastructure and material progression.
**Dependencies**: None new; use existing build system and material grid.

### 5) Charcoal Pit (Tile, optional early alternative to kiln)
**Why**: Provides a primitive path to charcoal without requiring kiln fuel recipes.
**Autarky role**: Fuel efficiency upgrade early in the loop.
**Dependencies**: Minimal (reuse existing fuel items, workshop‑style bill or special job).

---

## Next Tier (Small New System, High Payoff)

### 6) Gravel Path / Packed Dirt Road
**Why**: Uses existing `ITEM_GRAVEL` / `ITEM_DIRT` as actual infrastructure sinks.
**Autarky role**: Logistics efficiency; improves hauling speed.
**Dependencies**: Movement cost modifier (can start as visual only, then add pathing cost).

### 7) Irrigation Ditch
**Why**: Leverages water simulation for controlled flow into fields or gardens later.
**Autarky role**: Water distribution without full farming system yet.
**Dependencies**: Simple trench cell type or flagged “ditch” cell; integrate with water flow rules.

---

## “Bakery” at the Correct Scale (Multi‑Tile Chain)
If/when baking is introduced, it should be a *chain of small tiles*, not a single building:

1. **Grain Field** (crop tile)
2. **Threshing Floor** (grain → usable grain)
3. **Hand Mill / Quern** (grain → flour)
4. **Hearth / Oven** (flour → bread)

This keeps scale consistent and obeys the autarky loop rule: each step has a clear input/output and can be expanded independently.

---

## Suggested Implementation Order
1. Hearth / Firepit
2. Water Source tile
3. Water Drain tile
4. Material floors (brick/plank/block)
5. Charcoal Pit (optional)
6. Gravel path / packed dirt road
7. Irrigation ditch

This order maximizes immediate payoff with minimal new systems.

---

## Open Questions
- Should Hearth be a workshop or a single‑tile “ambient” structure?
- Should roads start as visual only (no path cost changes) or immediately affect pathfinding?
- Do we want water source/drain tiles to be placeable by the player or only via terrain gen?

