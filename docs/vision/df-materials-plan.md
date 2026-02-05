# DF-Style Materials Plan (Wood Species As Material)

## Goal
Adopt a Dwarf Fortress–style material model where every item carries a specific material (e.g., MAT_OAK) rather than using wood species as a separate subtype. This lets wood identity flow through crafting and construction consistently.

## Scope (Immediate)
- Add wood materials: MAT_OAK, MAT_PINE, MAT_BIRCH, MAT_WILLOW.
- Introduce a single common stone material: MAT_GRANITE (replacing generic “rock”).
- Preserve existing metal concepts and expand later.
- Keep existing item *forms* like logs, planks, blocks, etc., but encode the species via material.

## Data Model Changes
- Every item in the world carries a material field (if not already present), including wood, leaves, stone, and crafted goods.
- Wood items use MAT_OAK / MAT_PINE / MAT_BIRCH / MAT_WILLOW.
- Cell materials should store the actual wood material when built from wood.
- Any place that assumes MAT_WOOD becomes a single material must be updated to accept wood materials as a category.

## Material System Changes
- Expand MaterialType enum with specific wood materials.
- Replace generic rock with MAT_GRANITE as the baseline stone material.
- Add material category helpers like IsWoodMaterial(mat), IsStoneMaterial(mat), IsMetalMaterial(mat).
- Update material tables for density, value multiplier, color/tint, burn rate.

## Item Definitions
- ITEM_WOOD and related items become material-driven instead of type-driven.
- Prefer a single block item type (e.g., ITEM_BLOCKS) with material = MAT_GRANITE / MAT_OAK / etc.
- Leaf items also carry the tree material (e.g., MAT_OAK leaves), but should use an IsLeafItem() helper for behavior.
- Items should stack only when type and material match.
- Item names should include material when appropriate (e.g., "Oak Wood", "Pine Planks").

## Production Chain
- Felled tree trunk cells carry the wood material.
- Chop-felled produces ITEM_WOOD with matching material.
- Workshops preserve material on outputs (e.g., blocks/planks keep MAT_OAK).

## Construction / Build Flow
- When building from wood items, placed walls/floors store the wood material.
- Rendering should use wood material to pick sprite or tint.
- Tooltips should show "Wood Wall (Oak)" style labels.

## UI / Filters
- Stockpile filters should allow selecting wood materials as a group or individually.
- For now: expose a single "Wood (any)" group in stockpiles; add per-material filters later.
- Item lists and tooltips should display material names consistently.
## Rendering (Short-Term)
- It is acceptable to reuse existing sprites while materials are still being added.
- Tooltips and UI should always reflect the true material, even if sprites are generic for now.
## Rendering (Tint Table)
- Add a simple material->tint map for wood species (optional at first, but good for visual clarity).

## Material Display Rules
- Prefer "<Material> <ItemName>" for items (e.g., "Oak Wood", "Pine Planks", "Granite Blocks").
- Prefer "<Material> <Structure>" for constructions (e.g., "Oak Wall", "Pine Floor").
- Tooltips should show both the base item/type and material when relevant.

## Save/Load
- Bump save version.
- Save and load item materials and built cell materials.
- Provide migration for legacy MAT_WOOD -> MAT_OAK and legacy rock -> MAT_GRANITE.

## Natural vs Constructed
- Preserve "natural/unprocessed" status separately from material.
- Natural rock should be MAT_GRANITE with a natural flag; constructed/cut stone is still MAT_GRANITE but not natural.
- Use the natural flag to decide drop behavior and which workshop conversions are required.

## Recipes / Crafting Matching
- Recipes should be able to require:
  - Specific material (e.g., MAT_OAK).
  - Material category (e.g., any IsWoodMaterial()).
  - Any material (e.g., any ITEM_WOOD).

## Tests
- Update materials tests to assert wood materials are preserved through drop/craft/build.
- Update tree tests so saplings and felled outputs carry material.
- Add a construction test: build wood wall from MAT_PINE and ensure cell stores MAT_PINE.

## Open Questions
- Do we want distinct sprites per wood material now, or tinting only?

## Non-Goals (For Now)
- Full stone/metal material expansion beyond current needs.
- Balancing values (density/value/burn rate) beyond initial placeholders.
