# Next Tiles + Ingredient Needs (Autarky‑Scale)

Date: 2026-02-07
Scope: A short, actionable list of new tiles and the *minimum* new ingredients needed, based on current systems.

## Tiles You Can Add Now (Use Existing Items/Systems)

### 1) Hearth / Firepit (Tile or Workshop)
- **Inputs**: any fuel item (logs, sticks, leaves, peat, charcoal)
- **Outputs**: ash (already in code), heat/smoke (already in systems)
- **Why**: creates a real fuel sink and ties into temperature/fire.

### 2) Water Source (Spring) Tile
- **Inputs**: none (placeable)
- **Effect**: `SetWaterSource` (already in water system)
- **Why**: enables small‑map rivers and stable water loops.

### 3) Water Drain (Outflow) Tile
- **Inputs**: none (placeable)
- **Effect**: `SetWaterDrain`
- **Why**: gives players control over flooding and closes water loops.

### 4) Material Floors (Wood / Brick / Block)
- **Inputs**: planks / bricks / blocks (existing items)
- **Effect**: uses `HAS_FLOOR` + `floorMaterial`
- **Why**: creates immediate sinks for current workshop outputs.

### 5) Charcoal Pit (Primitive Workshop)
- **Inputs**: logs / sticks / peat
- **Outputs**: charcoal
- **Why**: early fuel upgrade without relying on kiln.

## Tiles That Need Only 1–2 New Ingredients

### 6) Thatch Roof / Drying Mat
- **New item**: **Dried Grass**
- **Why**: unlocks simple shelter + drying loop (very low tech).

### 7) Rope / Cordage Node
- **New item**: **Vines** (or “Grass Fiber”)
- **Why**: unlocks framed constructions and staged building.

### 8) Reed Pipe / Aqueduct
- **New item**: **Reeds** (hollow grass)
- **Why**: enables primitive water routing without new mechanics.

## Tiles That Require a Small New Workshop

### 9) Pottery (Pottery Wheel)
- **New item**: **Pottery**
- **Inputs**: clay (existing)
- **Why**: container/trade good path; natural extension of kiln.

### 10) Glass (Glass Kiln)
- **New item**: **Glass**
- **Inputs**: sand + fuel
- **Why**: window material, greenhouse path.

## Notes
- Avoid “Bakery” as a single tile. Use a chain: **Field → Threshing → Mill → Hearth/Oven**.
- Keep new tiles small and composable; prefer 2×2 or 3×3 workshops.

