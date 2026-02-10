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
- Five workshops: Stonecutter, Sawmill, Kiln, Charcoal Pit, Hearth.
- Multi-input recipes (e.g., Bind Gravel: gravel + clay).
- Fuel system with matching (any IF_FUEL item satisfies fuel requirements).
- ASH item produced by the Hearth.
- Stone loop fully closed (rock to blocks to construction).
- Clay loop closed (clay to bricks; clay + gravel to blocks).
- Wood loop mostly closed (logs to planks/sticks to construction/charcoal).
- Material floors exist in the cell/rendering system (HAS_FLOOR + floorMaterial).
- Water, fire/smoke/steam, and temperature systems are in place.
- 10-step craft job state machine with item reservation.

**Not yet implemented:**
- No bark, cordage, dried grass, poles, or reeds items.
- No Rope Maker, Glass Kiln, Pottery Wheel, or any tier 2+ workshop.
- No construction staging (walls/floors are still single-step).
- No seasoning/curing (no item condition states or timers).
- No stockpile environment tags.
- No young-tree growth stage or tree stumps/coppicing.
- No containers, tools, durability, or quality systems.
- Sand and dirt have no recipe sinks.

---

## Suggested Next Moves

1. **Material floor/wall sinks.** Planks, bricks, and blocks can already be produced but construction options are limited. Adding explicit wood/brick/block floor and wall types using the existing HAS_FLOOR and material systems would close the most loops with the least new code.

2. **Three gateway items: dried grass, cordage, poles.** These appear in every design doc and are the minimum additions that unlock thatch roofing, lashing/frames, and the rope-maker workshop. They require no new major systems.

3. **Rope Maker workshop (2x2).** Once bark and cordage items exist, this small workshop closes the leaf and bark loops and feeds into future construction staging.

4. **Two-stage construction (frame then fill).** Add a `stage` field to blueprints/jobs. Frame stage uses sticks or planks; fill stage uses planks, bricks, or blocks. This is the smallest change that makes building feel earned and makes stockpile logistics matter.

5. **Item condition states (seasoning/curing).** Add a lightweight condition enum and timer to items. Start with wood only (green/seasoned). Hook into stockpile environment once staging is working.

6. **Sand and dirt sinks.** Glass Kiln (sand + fuel) and farming (dirt as soil) are the proposed solutions. Glass Kiln is simpler and self-contained; farming is a larger system.
