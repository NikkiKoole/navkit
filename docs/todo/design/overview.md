# Design Overview

These six documents describe the production and construction systems for a DF/CDDA-inspired colony sim. Together they outline how raw materials flow from the environment through workshops into buildings and useful goods, how construction should feel multi-step and earned, and what small set of new items and systems would unlock the most gameplay depth. The recurring theme is **complete loops over long chains**: every new item should have a clear source, a clear sink, and ideally strengthen an existing cycle.

---

## Document Summaries

### [Workshops & Item Loops (Master)](workshops-master.md)
The central reference. Tracks everything that exists and everything proposed.
- Lists all implemented workshops (Stonecutter, Sawmill, Kiln, Charcoal Pit, Hearth) and their recipes.
- Maps every item's source and sink; flags open loops (sand, dirt, leaves have no recipe sink yet).
- Proposes a bark/cordage chain (strip logs at sawmill, twist bark into cordage at a Rope Maker).
- Prioritizes next workshops: Rope Maker, Glass Kiln, Pottery Wheel, then systems-heavy ones (Bloomery, Loom, Tanner, Hand Mill).
- Documents design principles: every addition needs a source, a sink, and feedback into existing loops.

### [Tile Ingredients Next](tile-ingredients-next.md)
A short, actionable list of new tiles and the minimum new ingredients they require.
- Identifies tiles addable right now with existing items: material floors (wood/brick/block), water source/drain.
- Tiles needing one new item: thatch (dried grass), rope node (cordage/vines), reed pipe (reeds).
- Tiles needing a new workshop: pottery (clay at pottery wheel), glass (sand at glass kiln).
- Emphasizes composability: keep workshops small (2x2 or 3x3), avoid monolithic "bakery" tiles.

### [Tech Tree Outline](tech-tree-outline.md)
Frames the existing and proposed content as an era progression from bare hands to stone construction.
- Era 0 (Bare Hands): gather leaves, sticks, dirt, clay; light a hearth for ash.
- Era 0.5 (Fire): charcoal pit and hearth close the fuel loop.
- Era 1 (Wood): sawmill produces planks/sticks; needs floor/wall sinks.
- Era 1.5 (Clay/Brick): kiln produces bricks; needs brick floors/walls.
- Era 2 (Stone): stonecutter produces blocks; needs block floors/walls.
- Identifies four small items that unlock the most progression: dried grass, cordage, bark/resin, reeds.

### [Primitives Missing](primitives-missing.md)
Identifies the smallest set of items needed to enable early survival builds.
- Top three picks: **dried grass** (roofing, bedding), **cordage/vines** (lashing, frames), **poles** (from a new young-tree growth stage).
- Secondary picks: bark/resin (waterproofing, glue), mud/cob mix (daub walls), reeds (pipes, mats), flat stone (grinding), simple container (basket/pot).
- Notes that sticks already exist but may need a bare-hands gather path (snap branch) for pre-sawmill play.
- Minimal survival build set once added: shelter (poles + cordage + thatch), fire (hearth + fuel), water (spring/drain + container).

### [Construction Staging](construction-staging.md)
Replaces instant "item becomes wall" with a multi-step build process.
- Walls: frame stage (sticks/planks) then fill stage (planks/bricks/blocks).
- Floors: base layer (dirt/gravel) then finish layer (planks/bricks/blocks).
- Ladders/ramps stay single-stage for now.
- Data model: add a `stage` field to Blueprint/Job plus per-stage input requirements.
- Each stage is a distinct hauling burst, making stockpile logistics meaningful.
- Defers: weather/curing timers, tool checks, multi-colonist stages.

### [Seasoning & Curing](seasoning-curing.md)
Adds material states that evolve over time, making stockpiles feel like conditioning zones.
- Wood states: green, seasoned, brittle, rotten. Clay states: wet, leathery, bone-dry, cracked.
- Stockpile environment tags (dry/covered/wet/open) drive condition timers without a full weather system.
- Ties into construction staging: frame stage requires seasoned wood, daub stage requires wet clay.
- Proposes "intermezzo" maintenance jobs (log-turner, mud-mister) for flavor and player agency.
- Phased implementation: conditions + timers first, then stockpile tags, then construction gating, then maintenance jobs.

---

## Key Themes and Connections

**Closed loops everywhere.** The master doc and tile-ingredients doc both insist that every new item must have both a source and a sink. Sand, dirt, and leaves are flagged repeatedly as open-ended -- each doc proposes a concrete fix (glass kiln, farming, composting or cordage).

**Three gateway items.** Dried grass, cordage, and poles appear in nearly every document. They are the minimum additions that unlock early shelter, multi-stage construction, and the rope-maker workshop chain. The primitives doc, tech tree, and tile-ingredients doc all converge on this same short list.

**Construction as a pipeline, not an instant.** Construction staging and seasoning/curing are designed as a pair. Staging splits building into frame-then-fill steps; seasoning makes the materials for those steps require time in stockpiles. Together they turn "place wall" into "harvest, condition, haul frame materials, haul fill materials, wait for cure."

**Stockpiles become meaningful.** Seasoning/curing turns stockpiles into conditioning zones (dry shed vs wet riverbank). Construction staging turns them into logistics hubs (each stage triggers a hauling burst). Multiple docs reference this as the key payoff.

**Fuel loop closure.** The hearth (now implemented) was the most-requested addition across the master doc, tile-ingredients, and tech tree. It closes the fuel loop by consuming any fuel item and producing ash, which feeds back into cement and future farming.

**Small workshops, composable chains.** Every doc prefers 2x2 or 3x3 workshops with 1-3 recipes each, chained together. The charcoal pit (primitive, no fuel needed) feeds the kiln (needs fuel). The sawmill feeds the rope maker. No single workshop should be a dead end.

---

## Where We Are Now

**Implemented and working:**
- Six workshops: Stonecutter, Sawmill, Kiln, Charcoal Pit, Hearth, Drying Rack.
- Multi-input recipes (e.g., Bind Gravel: gravel + clay).
- Multi-output recipes (outputType2/outputCount2 on Recipe struct, spawning + storage checks + tooltip display all handled).
- Fuel system with matching (any IF_FUEL item satisfies fuel requirements).
- Passive workshop system (Drying Rack: pure passive, Charcoal Pit: semi-passive with ignition).
- ASH item produced by the Hearth.
- ITEM_DRIED_GRASS produced by Drying Rack (passive conversion from ITEM_GRASS).
- Stone loop fully closed (rock to blocks to construction). Three stone types: granite, sandstone, slate.
- Clay loop closed (clay to bricks; clay + gravel to blocks).
- Wood loop mostly closed (logs to planks/sticks to construction/charcoal).
- Material floors exist in the cell/rendering system (HAS_FLOOR + floorMaterial).
- wallSourceItem/floorSourceItem grids track which item type built each wall/floor.
- Plank walls/floors render with per-species sprites (oak/pine/birch/willow).
- Mining drops the correct source item (planks from plank walls, logs from log walls, etc.).
- ITEM_POLES from tree branches. Branch cells use thin per-species sprites.
- Tree trunk taper: top 1-2 trunk cells become CELL_TREE_BRANCH (thin sprite at canopy, thick at base).
- Water, fire/smoke/steam, and temperature systems are in place.
- 10-step craft job state machine with item reservation.
- Vegetation grid (`vegetationGrid[]`) — dedicated uint8_t 3D grid for ground cover, separate from cellFlags surface bits. Five stages: VEG_NONE, VEG_GRASS_SHORT, VEG_GRASS, VEG_GRASS_TALL, VEG_GRASS_TALLER. Surface bits simplified to ground condition only (BARE/TRAMPLED).
- ITEM_GRASS — harvestable from VEG_GRASS_TALLER cells via Work > Harvest > Gather Grass. Full designation/job/action pipeline (DESIGNATION_GATHER_GRASS, JOBTYPE_GATHER_GRASS). Groundwear system migrated to write vegetation grid; trampling thresholds control grass stage transitions.

**Not yet implemented:**
- No bark, cordage, reeds items. Bark and stripped log sprites exist in atlas but ITEM_BARK, ITEM_STRIPPED_LOG, ITEM_CORDAGE not yet added.
- No Rope Maker, Glass Kiln, Pottery Wheel, or any tier 2+ workshop.
- No construction staging (walls/floors are still single-step).
- No seasoning/curing (no item condition states or timers).
- No stockpile environment tags.
- No tree stumps/coppicing.
- No containers, tools, durability, or quality systems.
- Sand and dirt have no recipe sinks.
- Block walls/floors don't have distinct sprites yet (blocks use the material's default sprite).
- Brick walls/floors don't have distinct sprites yet either.
- Terrain generation only uses MAT_GRANITE — sandstone/slate don't appear naturally yet.

---

## Suggested Next Moves

1. ~~**Material floor/wall sinks.**~~ **Mostly done.** wallSourceItem/floorSourceItem grids track which item type (planks/logs/blocks/bricks) built each wall/floor. Plank walls and floors render with per-species sprites. Mining drops the correct source item. Three stone types exist (granite, sandstone, slate). Still open: distinct block and brick wall/floor sprites, and natural generation of sandstone/slate.

2. **One remaining gateway item: cordage.** Poles are done (from tree branches). Dried grass is done (Drying Rack converts ITEM_GRASS → ITEM_DRIED_GRASS). Cordage (for lashing/frames) is the last gateway item needed — requires ITEM_BARK, ITEM_STRIPPED_LOG, and ITEM_CORDAGE plus Sawmill strip recipes. Multi-output recipe support is ready (Strip Bark: LOG → STRIPPED_LOG + BARK).

3. **Rope Maker workshop (2x2).** Once bark and cordage items exist, this small workshop closes the leaf and bark loops and feeds into future construction staging.

4. **Two-stage construction (frame then fill).** Add a `stage` field to blueprints/jobs. Frame stage uses sticks or planks; fill stage uses planks, bricks, or blocks. This is the smallest change that makes building feel earned and makes stockpile logistics matter.

5. **Item condition states (seasoning/curing).** Add a lightweight condition enum and timer to items. Start with wood only (green/seasoned). Hook into stockpile environment once staging is working.

6. **Sand and dirt sinks.** Glass Kiln (sand + fuel) and farming (dirt as soil) are the proposed solutions. Glass Kiln is simpler and self-contained; farming is a larger system.
