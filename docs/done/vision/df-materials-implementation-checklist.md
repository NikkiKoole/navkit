# DF-Style Materials Implementation Checklist

This is the concrete step-by-step plan to implement DF-style materials (wood species as materials) without breaking the build.

0. Audit current material usage
- Grep for MAT_WOOD / ITEM_WOOD / ITEM_STONE_BLOCKS / ITEM_WOOD_BLOCKS and list touchpoints.

1. Material enum expansion
- Add MAT_OAK, MAT_PINE, MAT_BIRCH, MAT_WILLOW, MAT_GRANITE.
- Add IsWoodMaterial, IsStoneMaterial, IsMetalMaterial helpers.
- Add material display names.

2. Item struct material
- Add material field to Item.
- Set default material on new items (fallback MAT_OAK).
- Update item stack/merge equality to include material.

3. Item name formatting
- Update item name helpers to include material ("Oak Wood", "Granite Blocks").
- Add IsLeafItem helper and ensure leaf naming works.

4. Item defs align with DF model
- Consolidate to ITEM_BLOCKS (material determines wood vs stone).
- Ensure ITEM_WOOD, ITEM_LEAF, ITEM_PLANKS are material-driven.
  - Caution: do this in a separate commit (high-impact step).

5. Recipe matching rules
- Add recipe input matching for:
  - specific material
  - material category (wood/stone)
  - any material
- Preserve material on outputs.

6. Tree -> item pipeline
- Tree trunks carry material = MAT_*.
- Chop-felled yields ITEM_WOOD with tree material.
- Leaves carry tree material.
  - Ensure trunk material is stored or derivable from TreeType.

7. Construction placement
- When consuming items, pass material into wall/floor.
- Ensure constructed walls store MAT_* (not MAT_WOOD).

8. Natural vs constructed
- Store natural flag separately from material.
- Natural rock = MAT_GRANITE + natural flag.
- Mining drops raw stone; workshop converts to blocks.
  - Prefer a bool flag for natural (cell flag for terrain, item flag for items).

9. UI + tooltips
- Tooltips show material explicitly.
- Stockpile filter: only "Wood (any)" group for now.

10. Save/load + migration
- Bump save version.
- Default old wood -> MAT_OAK, old rock -> MAT_GRANITE.
  - Migration must update items in all locations (ground, carried, stockpiles).

11. Tests
- Update materials tests for material preservation.
- Update tree tests for material propagation.
- Add a build test: MAT_PINE wall inherits material.
