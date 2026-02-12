# Next Tiles + Ingredient Needs (Autarky-Scale)

Date: 2026-02-07
Updated: 2026-02-12
Scope: A short, actionable list of new tiles and the *minimum* new ingredients needed, based on current systems.

## Tiles Already Added

### 1) Hearth / Firepit - DONE
- Burns any fuel item -> ash. Passive workshop (2x2).
- Tied into temperature/fire systems.

### 2) Charcoal Pit - DONE
- Primitive workshop (2x2). Char Logs, Char Peat, Char Sticks recipes.
- Semi-passive with ignition step.

### 3) Material Floors (Wood / Brick / Block) - DONE
- Uses `HAS_FLOOR` + `floorMaterial`. Planks/bricks/blocks as floor materials.
- Per-species wood floor sprites (oak/pine/birch/willow).

### 4) Drying Rack - DONE
- Passive workshop. GRASS -> DRIED_GRASS over time.

### 5) Rope Maker - DONE
- 2x2 workshop. Twist Bark, Twist Grass, Braid Cordage recipes.

## Tiles You Can Add Now (Use Existing Items/Systems)

### 6) Water Source (Spring) Tile
- **Inputs**: none (placeable)
- **Effect**: `SetWaterSource` (already in water system)
- **Why**: enables small-map rivers and stable water loops.
- **Status**: function exists, needs UI action.

### 7) Water Drain (Outflow) Tile
- **Inputs**: none (placeable)
- **Effect**: `SetWaterDrain`
- **Why**: gives players control over flooding and closes water loops.
- **Status**: function exists, needs UI action.

## Tiles That Need Only 1-2 New Ingredients

### 8) Thatch Roof / Drying Mat
- **Existing item**: DRIED_GRASS (already exists)
- **Needs**: MAT_THATCH material + construction recipe using dried grass
- **Why**: unlocks simple shelter + visual roof material.

### 9) Reed Pipe / Aqueduct
- **New item**: **Reeds** (hollow grass)
- **Why**: enables primitive water routing without new mechanics.

## Tiles That Require a Small New Workshop

### 10) Pottery (Pottery Wheel)
- **New item**: **Pottery**
- **Inputs**: clay (existing)
- **Why**: container/trade good path; natural extension of kiln.

### 11) Glass (Glass Kiln)
- **New item**: **Glass**
- **Inputs**: sand + fuel
- **Why**: window material, greenhouse path. Also closes sand loop (sand currently has no sink).

## Notes
- Avoid "Bakery" as a single tile. Use a chain: **Field -> Threshing -> Mill -> Hearth/Oven**.
- Keep new tiles small and composable; prefer 2x2 or 3x3 workshops.
