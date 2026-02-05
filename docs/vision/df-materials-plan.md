# DF-Style Materials Plan (Wood Species As Material)

## Goal
Adopt a Dwarf Fortress–style material model where every item carries a specific material (e.g., MAT_OAK) rather than using wood species as a separate subtype. This lets wood identity flow through crafting and construction consistently.

## Scope (Immediate)
- Add wood materials: MAT_OAK, MAT_PINE, MAT_BIRCH, MAT_WILLOW.
- Introduce a single common stone material: MAT_GRANITE (replacing generic “rock”).
- Preserve existing metal concepts and expand later.
- Keep existing items like ITEM_WOOD, ITEM_WOOD_BLOCKS, ITEM_PLANKS, etc.

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
- Leaf items also carry the tree material (e.g., MAT_OAK leaves).
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

## Save/Load
- Bump save version.
- Save and load item materials and built cell materials.
- Provide migration for legacy MAT_WOOD -> MAT_OAK and legacy rock -> MAT_GRANITE.

## Natural vs Constructed
- Preserve "natural/unprocessed" status separately from material.
- Natural rock should be MAT_GRANITE with a natural flag; constructed/cut stone is still MAT_GRANITE but not natural.
- Use the natural flag to decide drop behavior and which workshop conversions are required.

## Tests
- Update materials tests to assert wood materials are preserved through drop/craft/build.
- Update tree tests so saplings and felled outputs carry material.
- Add a construction test: build wood wall from MAT_PINE and ensure cell stores MAT_PINE.

## Open Questions
- Do we want distinct sprites per wood material now, or tinting only?

## Non-Goals (For Now)
- Full stone/metal material expansion beyond current needs.
- Balancing values (density/value/burn rate) beyond initial placeholders.
