# Tree Shapes: Branching + Roots (Notes)

## Current State (Code)
- **Growth system**: `src/simulation/trees.c`
  - Sapling -> trunk -> leaves; trunk is a single vertical column.
  - Canopy is radial around trunk top with a sparse skirt.
  - Leaves decay if not connected to a trunk (simple local checks).
- **Sapling regrowth + trampling**: `src/simulation/groundwear.c`
  - Saplings spawn on tall grass; trampling can kill saplings.
- **Manual tree placement**: `src/core/input.c` -> `TreeGrowFull()`.
- **Chop/fell**: `src/world/designations.c` (FellTree)
  - Removes trunk cells in the **column only** (vertical trunk).
  - Removes orphan leaves near the base.
- **Terrain generators**: `src/world/terrain.c`
  - Some generators place "trees" as `CELL_WALL` placeholders.

## Observations / Gaps
- Worldgen still uses **CELL_WALL** for tree placeholders (not real trees).
- Felling logic assumes **single vertical trunk**. If we add horizontal trunk branches,
  those will remain unless we update FellTree to remove connected trunk components.
- Leaves already connect to trunk cells; this can work with branches as long as
  branches are treated as trunk cells.

## Goal
- Add **branching trees** with side trunks/branches.
- Make branching **the default** tree type for growth and manual placement.
- Add **roots that are gameplay‑relevant** (wood underground now, possible ingredients later).
- Add **four tree types**: Oak, Pine, Birch, Willow.
- Remove the legacy generic tree (old saves not supported).

## Tree Types (Initial Set)
- **Oak**: broad canopy, heavier side branches, thicker roots (default broadleaf).
- **Pine**: tall straight trunk, narrow cone canopy, fewer branches, shallower roots.
- **Birch**: slim trunk, lighter canopy, small branches, thinner roots.
- **Willow**: light side branching, drooping leaves below branch height, prefers wet/peaty ground.

## Placement Bias (Logical Habitats)
- **Oak**: mid‑elevation, moderate moisture, good soil (dirt/clay).
- **Pine**: higher, drier ground; tolerates rocky/gravelly slopes.
- **Birch**: light soils, forest edges; tolerates sand/gravel.
- **Willow**: low elevation, wet/peaty ground, near rivers/lakes.

## Sprite Set (Minimum)
- **Trunks**: `tree_trunk_oak`, `tree_trunk_pine`, `tree_trunk_birch`, `tree_trunk_willow`
- **Leaves**: `tree_leaves_oak`, `tree_leaves_pine`, `tree_leaves_birch`, `tree_leaves_willow`
- **Saplings**: `tree_sapling_oak`, `tree_sapling_pine`, `tree_sapling_birch`, `tree_sapling_willow`
- **Roots (for now)**: reuse trunk sprites, but tag as roots in data so we can swap later.

## Root Differentiation (Future-proofing)
- Add a lightweight **root marker** even if roots use trunk sprites now.
  - Option A: a small `treePart` grid (TRUNK / ROOT / BRANCH / FELLED).
  - Option B: use unused cell flag bits to mark ROOT/BRANCH/FELLED.
- Roots and felled logs must be distinguishable from living trunks.

## Leaf Items (Per Type)
- Create **four leaf item types** so each tree drops its own leaves:
- `ITEM_LEAVES_OAK`, `ITEM_LEAVES_PINE`, `ITEM_LEAVES_BIRCH`, `ITEM_LEAVES_WILLOW`
- Each leaf item uses its matching sprite (`SPRITE_tree_leaves_*`).
- Leaf items are **stackable**, use `MAT_WOOD`, and are `IF_FUEL` (not buildable unless flagged).

## Sapling Items (Per Type)
- `ITEM_SAPLING_OAK`, `ITEM_SAPLING_PINE`, `ITEM_SAPLING_BIRCH`, `ITEM_SAPLING_WILLOW`
- Each sapling item uses its matching sprite (`SPRITE_tree_sapling_*`).

## UI / Placement
- **Tree Type buttons** in the Trees UI section: Oak / Pine / Birch / Willow.
- Manual placement uses the selected type (no generic tree).
- Legacy generic tree is removed from code (old saves not supported).

## Growth Algorithms (Per Type)

### Common Base (all types)
- Sapling -> trunk after `saplingGrowTicks`.
- Trunk grows upward every `trunkGrowTicks` until target height.
- Leaves decay if not connected to trunk within `LEAF_TRUNK_CHECK_DIST`.
- Roots placed when sapling becomes trunk:
  - 1–3 underground trunk cells around base (gameplay wood).
  - Optional 1 exposed surface root adjacent to base.

### Oak (Branching + Wide Canopy)
- **Target height**: 4–6.
- **Branching**:
  - Two mid‑levels: `baseZ + 2` and `baseZ + 3` (clamp to height).
  - Each level: place 1–2 side trunk cells (N/E/S/W), avoid duplicates.
  - For each branch: 40% chance to place a branch tip one level above if space is air.
- **Canopy**:
  - Radius 2–3, 1–2 levels above top.
  - Dense skirt at trunk top (skip rate ~40%).

### Pine (Tall + Narrow)
- **Target height**: 5–8.
- **Branching**:
  - 0–1 branch at `baseZ + (height - 2)` with 30% chance.
- **Canopy**:
  - Cone: radius 1–2, stacked 2–3 levels.
  - Minimal skirt at trunk top (skip rate ~60%).

### Birch (Slim + Airy)
- **Target height**: 4–7.
- **Branching**:
  - 0–1 short branch around `baseZ + 2` with 40% chance.
- **Canopy**:
  - Radius 1–2, sparse (skip rate ~50–60%).

### Willow (Drooping)
- **Target height**: 4–6.
- **Branching**:
  - 1 branch at mid‑level with 60% chance.
- **Canopy**:
  - Leaves spawn **1–2 levels below** branch height and trunk top for droop.
  - Wider horizontal spread (radius 2–3) but soft edges (skip rate ~50%).

## Placement Rules (Bias Sketch)
- Compute `wetness` from height + water proximity (as in HillsSoilsWater).
- Compute `slope` and `soil` from surface cells.
- Sample a `treeNoise` field for clumping.

Suggested rules (first pass):
- **Oak**: wetness 0.35–0.65, soil = dirt/clay, slope < 2.
- **Pine**: wetness < 0.35, slope >= 1, soil = gravel/rocky.
- **Birch**: wetness 0.3–0.6, soil = sand/gravel, edge zones.
- **Willow**: wetness > 0.65 OR near water, soil = peat/dirt.

## Felling Outputs (Detailed)

### Two‑Stage Loop
1) **Chop down tree** (existing job)
   - Tree falls; standing trunk + branches + roots are removed.
   - A **fallen trunk line** is created on the ground.
   - Leaves drop as items (small amount).
   - Saplings drop (smaller amount).

2) **Chop up fallen trunk** (new job)
   - Each fallen trunk segment becomes **log items**.
   - Logs are processed later into blocks/planks by carpentry/sawmill jobs.
   - Manual designation only for now (no auto-queue).

### Fallen Trunk Placement Rules
- Determine fall direction (same as current: away from chopper).
- Let `H` = standing trunk height (count of trunk cells).
- Fallen length = `H` (1 cell of fallen trunk per standing trunk cell).
- For each segment `i` in [0..H-1]:
  - Position: `x + dirX * i`, `y + dirY * i`.
  - Clamp to bounds; if blocked, stop early.
  - Find **surface z** at that (x,y) and place fallen trunk at `surfaceZ + 1`.
  - If that cell isn’t air, stop early (prevents overlap).
- Mark each fallen segment as **treePart=FELLED** (so it does not regrow).

### Leaf + Sapling Drops
- Leaf items: `leafCount / 8` (min 1 if any leaves).
- Saplings: `leafCount / 5` (min 1 if any leaves).
- Scatter around base using small radius (existing golden‑angle scatter ok).

## Tests (Suggested)
1. **FellTree removes connected trunks** (branches + roots), not just the vertical column.
2. **Fallen trunk placement**: length equals original trunk height; placed at surface; stops at obstruction.
3. **Felled trunk tagged** (treePart=FELLED) so it does not regrow.
4. **Leaf drops**: counts follow formula; item type matches tree type.
5. **Sapling drops**: counts follow formula.
6. **Growth shape**: oak creates branches, pine mostly vertical, willow droops leaves.
7. **Roots placed** at/under base and tagged ROOT.

## Artist Cheat Sheet (Sprites)

Oak
- Trunk: `tree_trunk_oak`
- Leaves: `tree_leaves_oak` (broad, dense canopy)
- Sapling: `tree_sapling_oak`

Pine
- Trunk: `tree_trunk_pine`
- Leaves: `tree_leaves_pine` (narrow, cone-like)
- Sapling: `tree_sapling_pine`

Birch
- Trunk: `tree_trunk_birch` (slimmer, lighter bark)
- Leaves: `tree_leaves_birch` (small, airy)
- Sapling: `tree_sapling_birch`

Willow
- Trunk: `tree_trunk_willow`
- Leaves: `tree_leaves_willow` (drooping / hanging feel)
- Sapling: `tree_sapling_willow`

## Next Steps (If We Proceed)
- Add tree type array + branching logic in `trees.c`.
- Add root placement when sapling becomes trunk.
- Update FellTree to remove connected trunk cluster.
- Add fallen trunk placement + new "Chop up fallen trunk" job.
- Replace `CELL_WALL` tree placeholders in terrain gen with saplings or grown trees.
