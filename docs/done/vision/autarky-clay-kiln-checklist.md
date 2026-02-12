# Autarky Loop: Clay + Charcoal + Kiln (Checklist)

## Goal
Introduce a minimal closed loop:
- **Clay**: `CELL_CLAY` → mining → `ITEM_CLAY` → kiln → `ITEM_POTTERY` (or `ITEM_BRICK`).
- **Charcoal**: `ITEM_WOOD` → kiln → `ITEM_CHARCOAL` (fuel).

## 1) Cell: Clay Terrain
- [ ] Add `CELL_CLAY` enum in `src/world/cell_defs.h`.
- [ ] Add `CELL_CLAY` definition in `src/world/cell_defs.c`:
  - Diggable like dirt/stone.
  - Sprite placeholder: `SPRITE_dirt` or `SPRITE_grass_trampled`.
  - Drops `ITEM_CLAY` when mined.
- [ ] Add clay blobs in terrain gen:
  - Use existing Perlin noise to create patchy “blobs.”
  - Apply in a depth band just below topsoil (e.g., z = surface-1..surface-3).
  - Keep parameters small and explicit (scale, threshold, band thickness).
  - See `docs/vision/terrain-and-tiles/ground-cells-distribution.md`.

## 1b) Related Ground Cells (Autarky-Ready)
- [ ] Add `CELL_GRAVEL` (surface patches, movement penalty or cosmetic).
- [ ] Add `CELL_SAND` (surface patches, low fertility / poor growth).
- [ ] Add `CELL_PEAT` (surface patches, potential fuel source).
- [ ] Use the same distribution sketch (Perlin + height/slope) for all.

## 2) Items
- [ ] Add item enums in `src/entities/items.h`:
  - `ITEM_CLAY`
  - `ITEM_POTTERY` (or `ITEM_BRICK`)
  - `ITEM_CHARCOAL`
- [ ] Add item defs in `src/entities/item_defs.c`:
  - `ITEM_CLAY`: stackable
  - `ITEM_POTTERY`/`ITEM_BRICK`: stackable (optionally `IF_BUILDING_MAT`)
  - `ITEM_CHARCOAL`: stackable, `IF_FUEL`
- [ ] Choose placeholder sprites (e.g., `SPRITE_generic`, `SPRITE_stone_block`).

## 3) Mining Source
- [ ] Ensure mining a `CELL_CLAY` tile drops `ITEM_CLAY`.
  - Likely in mining completion path in `src/world/designations.c`.

## 4) Workshop: Kiln
- [ ] Add kiln workshop type in `src/entities/workshops.h` and `src/entities/workshops.c`.
- [ ] Add workshop template with basic layout (like stonecutter).
- [ ] Add kiln recipes:
  - `ITEM_CLAY` → `ITEM_POTTERY` (or `ITEM_BRICK`)
  - `ITEM_WOOD` → `ITEM_CHARCOAL`

## 5) Stockpiles (Optional)
- [ ] No changes needed; stockpiles accept all items by default.
- [ ] (Optional) Add filter hotkeys for the new item types.

## Notes
- Prefer placeholders for sprites until art is available.
- Keep the loop closed: do not add items without a source + sink.
