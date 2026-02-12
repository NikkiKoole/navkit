# Design Overview

These documents describe the production and construction systems for a DF/CDDA-inspired colony sim. Together they outline how raw materials flow from the environment through workshops into buildings and useful goods, how construction should feel multi-step and earned, and what small set of new items and systems would unlock the most gameplay depth. The recurring theme is **complete loops over long chains**: every new item should have a clear source, a clear sink, and ideally strengthen an existing cycle.

---

## Document Summaries

### [Progression Story](progression-story.md) ⭐
A narrative walkthrough showing how a player progresses from a naked mover with nothing to an established colony.
- Demonstrates the full arc: gathering → workshops → construction → closed loops.
- Uses only systems that exist in the codebase today (no speculative features).
- Shows four phases: bare-hands gathering, first workshop chain, fire/permanence, expanding colony.
- Useful as a reference for what gameplay loops are already complete and working.

### [Progression Gaps Analysis](progression-gaps-analysis.md) ⭐⭐⭐
Critical analysis of what's missing from current progression and how to complete it.
- Identifies 8 major gaps: cordage has no use, rock gathering unrealistic, no primitive shelter, sand/dirt/leaves dead ends, construction too simple, no water interaction, no tools, blocks feel identical.
- Proposes solutions with implementation notes for each gap.
- Priority-ranked phases: A) Complete fiber loop (cordage use), B) Bare-hands rock gathering, C) Water access, D) Close open loops, E) Tools.
- Shows complete Era 0→4 progression arc after all gaps filled.
- **Recommended first step:** Construction staging + cordage use (~2-3 days, biggest impact).

### [Cutscene Panel System](cutscene-panel-system.md)
ASCII-art storytelling system with typewriter effects and sequential panels.
- Aesthetic: CP437/box-drawing chars, monospace, resolution-independent virtual grid (80x30 scaled to screen).
- Use cases: intro sequence, milestone celebrations (first workshop, first wall, etc.), recipe discoveries, atmospheric moments.
- Triggers: first launch, job completion milestones, workshop placement, item crafting.
- Tone: minimalist, poetic, grounded ("Rock becomes block." "The fire feeds itself.")
- Phase 1 (~3-4 days): Core system + typewriter + 5-6 milestone cutscenes.
- Includes visual mockups, content examples, and writing guidelines.

### [Workshops & Item Loops (Master)](workshops-master.md)
The central reference. Tracks everything that exists and everything proposed.
- Lists all 7 implemented workshops (Stonecutter, Sawmill, Kiln, Charcoal Pit, Hearth, Drying Rack, Rope Maker) and their 18 recipes.
- Maps every item's source and sink; flags remaining open loops (cordage, poles, ash, sand, dirt need sinks).
- Bark/cordage chain fully implemented (strip logs at sawmill, twist bark/dried grass at Rope Maker, braid into cordage).
- All Tier 1 workshops done. Next tier: Glass Kiln, Pottery Wheel, then systems-heavy ones (Bloomery, Loom, Tanner, Hand Mill).
- Documents design principles: every addition needs a source, a sink, and feedback into existing loops.

### [Water-Dependent Crafting](water-dependent-crafting.md)
Enables mud/cob crafting through location-based resource access (no containers needed).
- Three workshop archetypes: Wet (must be on water), Moisture (benefits from water), Drying (needs dry location).
- Mud Mixer on riverbed: DIRT + CLAY + water → MUD; MUD + DRIED_GRASS → COB.
- Material moisture states: WET → DAMP → DRY → BONE_DRY (passive transitions over time).
- Stockpile environment tags (dry/wet/covered) drive conditioning without full weather system.
- Unlocks wattle-and-daub construction (sticks + mud + thatch) for early shelter.
- Requires new systems: moisture states, placement validators, stockpile tags, passive moisture ticks.
- **Prerequisite:** Water placement tools (see below) - players need to control where rivers flow.

### Terrain Sculpting -- DONE (moved to docs/done/)
**Implemented.** Instant sandbox mode sculpting with circular brush (1x1 to 7x7).
- Left-drag raise, right-drag lower. Freehand stroke interpolation.
- Material preservation from source terrain.
- Enables river carving, moats, terrain leveling.
- Full design doc moved to `docs/done/terrain-sculpting-brushes.md`.
- Survival mode (designation-based, resource-consuming) still possible as future extension.

### [Water Placement Tools](water-placement-tools.md)
Alternative/supplement to terrain brushes: specific water source/drain placement.
- Place Water Source (spring) + Place Water Drain (outflow) for controlled water spawning.
- Less flexible than terrain brushes but simpler for just adding water to existing terrain.
- Functions already exist (SetWaterSource/SetWaterDrain) - just needs UI actions.
- **Recommendation:** Terrain brushes are more powerful; water tools optional supplement.

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
- **Top three picks ALL DONE:** dried grass (Drying Rack), cordage (Rope Maker), poles (tree harvest from branches).
- **Also done:** bark (Sawmill strip bark), short string (Rope Maker), stripped log.
- Remaining: mud/cob mix (needs water-dependent crafting), reeds, flat stone, simple container.
- Sticks harvestable from living trees via tree harvest system (no sawmill needed).
- Minimal survival build set: shelter (poles+cordage done, thatch roofing not yet), fire (hearth done), water (UI not yet), storage (not yet).

### [Construction Staging](construction-staging.md) — IMPLEMENTED
Multi-step build process replacing instant construction. **Moved to `docs/done/design/`.**
- 3 multi-stage recipes: wattle & daub (2-stage), plank wall (2-stage), thatch floor (2-stage).
- Frame stage uses sticks + cordage; fill stage uses planks/dirt/clay/dried grass.
- Single-stage recipes: dry stone wall, log wall, brick wall, plank floor, brick floor, ladder, ramp.
- Blueprint struct has `stage`, `stageDeliveries[]`, `consumedItems[]`.
- Lossy refund (75%) for completed stages on cancellation, 100% for current stage.
- UI: "S1/S2" overlay, "Stage: X/Y" tooltip.
- Deferred: weather/curing timers, tool checks, multi-colonist stages.

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

**Three gateway items -- ALL DONE.** Dried grass, cordage, and poles were identified across nearly every document as the minimum additions. All three are now implemented: dried grass (Drying Rack), cordage (Rope Maker chain), poles (tree harvest). The next bottleneck is giving cordage a *sink* via construction staging.

**Construction as a pipeline, not an instant.** Construction staging is **implemented** — multi-stage frame-then-fill for wattle & daub, plank wall, and thatch floor. Each stage triggers a separate hauling burst, making stockpile logistics meaningful. Seasoning/curing (the other half of the pair) is still future — would make materials require time in stockpiles before use.

**Stockpiles become meaningful.** Construction staging already turns stockpiles into logistics hubs (each stage triggers a hauling burst). Seasoning/curing (future) would further turn stockpiles into conditioning zones (dry shed vs wet riverbank).

**Fuel loop closure.** The hearth closes the fuel loop by consuming any fuel item and producing ash. All fuel items now have a sink. ASH itself still needs sinks (cement, fertilizer).

**Small workshops, composable chains.** Every doc prefers 2x2 or 3x3 workshops with 1-3 recipes each, chained together. The charcoal pit (primitive, no fuel needed) feeds the kiln (needs fuel). The sawmill feeds the rope maker. No single workshop should be a dead end.

---

## Where We Are Now

**Implemented and working:**
- Seven workshops: Stonecutter, Sawmill, Kiln, Charcoal Pit, Hearth, Drying Rack, Rope Maker.
- Multi-input recipes (e.g., Bind Gravel: gravel + clay).
- Multi-output recipes (outputType2/outputCount2 on Recipe struct, spawning + storage checks + tooltip display all handled).
- Fuel system with matching (any IF_FUEL item satisfies fuel requirements).
- Passive workshop system (Drying Rack: pure passive, Charcoal Pit: semi-passive with ignition).
- ASH item produced by the Hearth.
- ITEM_DRIED_GRASS produced by Drying Rack (passive conversion from ITEM_GRASS).
- ITEM_BARK and ITEM_STRIPPED_LOG (save v33). Sawmill recipes: Strip Bark (LOG → STRIPPED_LOG + BARK), Saw Stripped (STRIPPED_LOG → PLANKS x5). Both items have per-species sprites.
- ITEM_SHORT_STRING and ITEM_CORDAGE (save v35). Rope Maker recipes: Twist Bark (bark x2 → string x3), Twist Grass (dried grass x4 → string x2), Braid Cordage (string x3 → cordage x1). Closes the bark loop.
- Stone loop fully closed (rock to blocks to construction). Three stone types: granite, sandstone, slate.
- Clay loop closed (clay to bricks; clay + gravel to blocks).
- Wood loop fully closed (logs to planks/sticks to construction/charcoal; bark to string to cordage).
- Material floors exist in the cell/rendering system (HAS_FLOOR + floorMaterial).
- wallSourceItem/floorSourceItem grids track which item type built each wall/floor.
- Plank walls/floors render with per-species sprites (oak/pine/birch/willow).
- Mining drops the correct source item (planks from plank walls, logs from log walls, etc.).
- ITEM_POLES from tree branches. Branch cells use thin per-species sprites.
- Tree trunk taper: top 1-2 trunk cells become CELL_TREE_BRANCH (thin sprite at canopy, thick at base).
- Water, fire/smoke/steam, and temperature systems are in place.
- Lighting system: sky light (column scan + BFS spread), block light (colored BFS from sources), depth-tint integration, UI panel with toggles, sandbox torch placement, save/load (v37), 48 tests. Only missing: fire cells don't auto-emit light.
- Floor dirt tracking and cleaning system (save v36). Movers track dirt from natural soil onto constructed floors. Designation-based cleaning jobs.
- Data-driven action registry (action_registry.c): single source of truth for all action metadata.
- Data-driven item metadata with designated initializers.
- 10-step craft job state machine with item reservation.
- Vegetation grid (`vegetationGrid[]`) — dedicated uint8_t 3D grid for ground cover, separate from cellFlags surface bits. Five stages: VEG_NONE, VEG_GRASS_SHORT, VEG_GRASS, VEG_GRASS_TALL, VEG_GRASS_TALLER. Surface bits simplified to ground condition only (BARE/TRAMPLED).
- ITEM_GRASS — harvestable from VEG_GRASS_TALLER cells via Work > Harvest > Gather Grass. Full designation/job/action pipeline (DESIGNATION_GATHER_GRASS, JOBTYPE_GATHER_GRASS). Groundwear system migrated to write vegetation grid; trampling thresholds control grass stage transitions.
- Tree harvest system (save v34): gather sticks + leaves from living trees without chopping. `treeHarvestState` uint8_t grid on trunk base cells, max 2 harvests before depletion, ~60s regen per level (reuses growthTimer on idle mature trunks). Full pipeline: DESIGNATION_GATHER_TREE → JOBTYPE_GATHER_TREE → ACTION_WORK_GATHER_TREE under Work > Harvest > gather [T]ree. Overlay + progress bar, tooltip shows harvest state.
- Terrain sculpting (sandbox mode): instant lower/raise/smooth terrain with circular brush. Brush sizes 1x1 to 7x7. Left-drag raise, right-drag lower, hold S to smooth. Freehand stroke interpolation. Enables river carving, moats, terrain leveling.
- Sapling/leaf item unification (save v32): consolidated 8 separate types into 2 unified types (ITEM_SAPLING, ITEM_LEAVES) using material field for species.
- Construction staging (multi-stage building): Blueprint struct has `stage` field, `stageDeliveries[]`, `consumedItems[]`. 3 multi-stage recipes: wattle & daub (frame: sticks+cordage, fill: dirt/clay), plank wall (frame: sticks+cordage, fill: planks), thatch floor (base: dirt/gravel/sand, finish: dried grass). Stage progression resets delivery tracking. Lossy refund (75%) for completed stages on cancellation. UI shows "S1/S2" overlay and "Stage: X/Y" tooltip.
- Cordage has sinks: used in wattle & daub wall frame (1 cordage) and plank wall frame (1 cordage). Fiber loop closed.
- Sand and dirt have construction sinks: dirt used in wattle & daub fill + thatch floor base, sand used in thatch floor base.
- Water placement tools in sandbox UI: ACTION_SANDBOX_WATER with click=water, shift+click=source, right-click=remove, shift+right=drain.
- Rock gathering: mining is bare-hands (all movers have canMine=true by default, no tools required). Rocks are regular ground items hauled normally.

**Not yet implemented:**
- No Glass Kiln, Pottery Wheel, or any tier 2+ workshop beyond Rope Maker.
- No seasoning/curing (no item condition states or timers).
- No moisture states (no MoistureState enum or moisture ticks).
- No stockpile environment tags (dry/wet/covered).
- No water-dependent workshops (mud mixer, brick drying, etc.).
- No tree stumps/coppicing.
- No containers, tools, durability, or quality systems.
- ASH, POLES still have no recipe sinks.
- Sand/dirt partially addressed (construction sinks) but no Glass Kiln or farming yet.
- Block walls/floors don't have distinct sprites yet (blocks use the material's default sprite).
- Brick walls/floors don't have distinct sprites yet either.
- Terrain generation only uses MAT_GRANITE — sandstone/slate don't appear naturally yet.
- Fire cells don't auto-emit light (fire is visually bright but doesn't illuminate surroundings).

---

## Suggested Next Moves

### Recently Completed
1. ~~**Cordage chain.**~~ **DONE.** Rope Maker workshop with Twist Bark, Twist Grass, Braid Cordage recipes. Bark loop closed.
2. ~~**Construction staging.**~~ **DONE.** Multi-stage frame-then-fill for wattle & daub, plank wall, thatch floor. Cordage used as frame lashing. Fiber loop closed.
3. ~~**Cordage sinks.**~~ **DONE.** Wattle & daub + plank wall frames consume cordage.
4. ~~**Sand/dirt partial sinks.**~~ **DONE.** Used in construction recipes (thatch floor base, wattle & daub fill).
5. ~~**Water placement tools.**~~ **DONE.** ACTION_SANDBOX_WATER in sandbox mode.
6. ~~**Material floor/wall sinks.**~~ **Mostly done.** Still open: distinct block and brick wall/floor sprites, and natural generation of sandstone/slate.

### Visual Polish
1. **Block/brick wall sprites.** Blocks and bricks render with generic wall sprite. Need distinct SPRITE_wall_stone_block, SPRITE_wall_brick, etc.
2. **Sandstone/slate worldgen.** MAT_SANDSTONE and MAT_SLATE exist and are placeable but don't appear in terrain generation. Add geological layers.

### Production Chain Extensions
3. **Remaining sand/dirt sinks.** Glass Kiln (sand + fuel → GLASS) is simplest. Farming (dirt as soil) is a larger system. Mud Mixer (dirt + clay + water → MUD) bridges both.
4. **ASH sinks.** Cement recipe (ASH + CLAY → BLOCKS) at Stonecutter, or fertilizer for future farming.
5. **POLES sinks.** Could be used in lean-to or scaffold construction recipes.
6. **Fire auto-light.** Active fire cells should emit proportional block light. Small integration task.

### Larger Systems (Later)
7. **Water-dependent crafting (mud/cob system).** Requires moisture states, placement validators, stockpile tags, passive moisture ticks. Prerequisites: terrain sculpting (done), water placement (done in sandbox).

8. **Item condition states (seasoning/curing).** Lightweight condition enum + timer on items. Start with wood (green/seasoned). Hook into stockpile environment tags.

9. **Containers and tools.** No inventory/equipment systems yet. Containers enable water hauling (removes "must be on water" restriction for mud mixer). Tools enable mining, faster chopping, etc.
