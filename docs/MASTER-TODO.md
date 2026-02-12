# NavKit Master TODO List

*Last Updated: 2026-02-12*  
*Auto-generated from comprehensive docs scan*

---

## 🔥 ACTIVE WORK (docs/doing/)

### Lighting System (PARTIAL - in progress)
- **File**: [`docs/doing/lighting-system.md`](doing/lighting-system.md)
- **Status**: Steps 1-5 complete (data structures, sky light, rendering integration)
- **Remaining Steps**:
  - Block light propagation
  - Colored light mixing
  - Torch placement UI
  - Fire auto-light
  - Dirty tracking optimization
- **Open Questions**:
  - Gameplay impact? (mover speed, enemy spawning, plant growth, or cosmetic only?)
  - Darkness level tuning (ambient minimum)
  - Light through floors/ramps behavior
  - Colored sky light at sunset/moonlight?
  - Performance budget for 512x512x16 grid

---

## 🐛 OPEN BUGS (docs/bugs/)

### 1. Wall-Build Mover Pop
- **File**: [`docs/bugs/wall-build-pop.md`](bugs/wall-build-pop.md)
- **Status**: OPEN
- **Issue**: After building a wall, mover cannot stand on it and pops to illogical locations
- **Proposed Fix**: Remember tile mover was standing on before building, default to that direction when displaced
- **Blocker**: Memory constraints (8 bytes/mover × 10K movers = 80KB overhead rolled back)

### 2. Stockpile Priority Performance vs Correctness
- **File**: [`docs/bugs/stockpile-priority-performance.md`](bugs/stockpile-priority-performance.md)
- **Status**: DEFERRED (Architectural Redesign Required)
- **Priority**: Medium | **Complexity**: High
- **Issue**: `FindStockpileForItem()` uses early-exit optimization, ignores priority for performance. Items hauled to wrong stockpile, then re-hauled.
- **Attempted Fix**: Search for highest priority first → **6-12x slowdown** (rolled back)
- **Recommended Solution**: Pre-sorted priority list (maintains sorted stockpiles by priority, updates only when priorities change)
- **Implementation Estimate**: 5-6 hours across 4 phases
- **Test Status**: Priority test disabled at `tests/test_jobs.c:8693` (wrapped in `if (0)`)
- **Related Code**: `src/entities/stockpiles.c::FindStockpileForItem()`
- **Also See**: [`docs/bugs/wrong-stockpile-priority.md`](bugs/wrong-stockpile-priority.md) (Won't Fix by design)

### 3. High-Speed Stuck Movers
- **File**: [`docs/bugs/high-speed-stuck.md`](bugs/high-speed-stuck.md)
- **Status**: OPEN
- **Issue**: At game speed 100, movers get stuck (appear yellow, don't path). Changing game speed allows them to path again.
- **Details**: No resolution details provided yet

---

## 🎯 HIGH PRIORITY GAMEPLAY GAPS (docs/todo/)

### Glass & Composting (HIGHEST PRIORITY)
- **File**: [`docs/todo/gameplay/progression-gaps-analysis.md`](todo/gameplay/progression-gaps-analysis.md)
- **Glass Kiln**: SAND + fuel → GLASS (material exists but no sink)
- **Composting**: LEAVES → dirt/mulch (leaves have no recipe sink)
- **Impact**: Closes critical autarky loops

### Tool System (Missing Entirely)
- **File**: [`docs/todo/gameplay/progression-gaps-analysis.md`](todo/gameplay/progression-gaps-analysis.md)
- No tools implemented
- No speed bonuses
- No durability system

### Water-Dependent Crafting
- **File**: [`docs/todo/gameplay/progression-gaps-analysis.md`](todo/gameplay/progression-gaps-analysis.md)
- Mud mixer not implemented
- Moisture states missing

### Materials Without Recipe Sinks
- **File**: [`docs/todo/gameplay/progression-gaps-analysis.md`](todo/gameplay/progression-gaps-analysis.md)
- **ASH**: No current uses
- **POLES**: Limited uses
- **SHORT_STRING**: Needs direct uses (note: bark lashing as future alternative)
- **Source**: [`docs/todo/gameplay/workshops-master.md`](todo/gameplay/workshops-master.md)

### Missing Primitive Items
- **File**: [`docs/todo/gameplay/primitives-missing.md`](todo/gameplay/primitives-missing.md)
- Mud / Cob Mix (Soil + Water)
- Reeds / Hollow Grass
- Flat Stone / Grinding Stone
- Simple Container (basket or clay pot)

### Fire Auto-Light
- **File**: [`docs/todo/gameplay/progression-gaps-analysis.md`](todo/gameplay/progression-gaps-analysis.md)
- **Blocked**: Pending lighting system completion
- Fire cells don't illuminate surroundings

---

## 🔬 RESEARCH PROJECTS (docs/research/)

### Reverse-Time / Rewind Feature
- **File**: [`docs/research/reverse-time-feature.md`](research/reverse-time-feature.md)
- **Scope**: ~900-1100 LOC total
- **Difficulty**: Medium
- **Total Phases**: 5

#### Phase 1: Infrastructure (200-300 LOC)
- Create `core/rewind.h` and `core/rewind.c`
- Implement snapshot API: `SaveSnapshot()`, `RestoreSnapshot()`, `RewindOneTick()`, `InvalidateTransientCaches()`

#### Phase 2: Snapshot System (400-500 LOC)
- Circular buffer of snapshots
- Per-tick or per-interval saving mechanism
- Memory pool for snapshot data

#### Phase 3: Deterministic RNG (50-100 LOC)
- Replace `rand()` calls in 6 system files:
  - `simulation/water.c` (1 call)
  - `simulation/fire.c` (3 calls)
  - `simulation/smoke.c` (2 calls)
  - `simulation/steam.c` (2 calls)
  - `simulation/groundwear.c` (1 call)
  - `entities/mover.c` (optional, 1 call)

#### Phase 4: Cache Invalidation (70-100 LOC)
- Invalidate pathfinding caches on restore
- Rebuild spatial grids (mover and item)
- Mark movers for repath

#### Phase 5: UI & Controls (200-300 LOC)
- Key binding for rewind (e.g., Z key)
- Timeline visualization
- History length indicator

#### Potential Pitfalls
- Non-deterministic physics (water, fire, smoke, steam use randomness)
- Spatial grid cache invalidation required
- Job system references (items/movers/stockpiles must restore atomically)
- Path cache staleness

#### Synergy Opportunity
- **Integration with Infinite World / Delta Persistence** (see vision docs)
- Could reduce snapshot memory from ~150MB to ~2-5KB per tick
- Enable multi-minute rewind capability
- Recommend implementing delta tracking infrastructure first if planning both features

### Dwarf Fortress Tile Reference
- **File**: [`docs/research/df-tile-reference.md`](research/df-tile-reference.md)
- **Note**: Reference documentation only, no outstanding action items
- Maps DF tile concepts to NavKit architecture

---

## 🌍 VISION / PLANNED FEATURES (docs/vision/)

*100+ planned features across 23 files. Organized by category:*

---

### Core Systems - Entropy & Simulation

#### Entropy System
- **File**: [`docs/vision/entropy.md`](vision/entropy.md)
- Maintenance jobs system (cleaning, weeding, repairs, refueling)
- Grass regrowth mechanics (worn grass → dirt → grass cycles)
- Dirt tracking and cleaning (movers tracking mud indoors)
- Fire spread system with environmental factors
- Temperature system with seasonal cycles
- Water flow and flooding mechanics
- Wind system affecting fire/sound/seeds
- Sound propagation system
- Smell system with mood effects
- Reclamation timeline (abandoned areas → nature)

#### Entropy Analysis
- **File**: [`docs/vision/entropy2-chatgpt.md`](vision/entropy2-chatgpt.md)
- Maintenance debt as design signal
- Threshold-based task spawning (not continuous)
- Structural counters (mudrooms, doormats, paving)

#### Entropy Systems Detail
- **File**: [`docs/vision/entropy_systems.md`](vision/entropy_systems.md)
- Temperature grid system (foundation for heat mechanics)
- Steam system (water phase change)
- Mud system (water + dirt)
- Vegetation growth/regrowth
- Mold/fungus spread in dark/damp areas
- Gas/poison mechanics
- Ice formation from freezing water

#### Steam System Roadmap
- **File**: [`docs/vision/steam-system.md`](vision/steam-system.md)
- **Phase 1**: Basic steam generation, rising, condensation
- **Phase 2**: Pressure tracking and flow
- **Phase 3**: Steam machines (turbines, pistons)

---

### Farming & Crops

#### Farming Overhaul
- **File**: [`docs/vision/farming-overhaul-ccdda.md`](vision/farming-overhaul-ccdda.md)
- Plant needs system (water, light, temperature, tend, health)
- Weather impact on crops
- Soil types (ground vs raised beds)
- Tool requirements for planting/tending
- Thrive metric for yield optimization
- Winter dormancy for perennials
- Pest/disease abstraction (no individual tracking)
- Fertilizer types
- Multi-harvest perennials

#### Crop Reference Libraries
- **Files**: 
  - [`docs/vision/crops-ccdda.md`](vision/crops-ccdda.md) - Cataclysm DDA crop reference
  - [`docs/vision/crops-df.md`](vision/crops-df.md) - Dwarf Fortress crop reference
- Reference libraries of crop types with growth times, nutrition, uses
- Medicinal plants (Bee Balm, Datura, Mugwort, etc.)

#### Workshop-Style Farming
- **File**: [`docs/vision/workshopdr.md`](vision/workshopdr.md)
- Greenhouse equivalent system

---

### Logistics & Job System

#### Logistics Influences
- **File**: [`docs/vision/logistics-influences.md`](vision/logistics-influences.md)
- Belt/conveyor systems as mid-game upgrade
- Vertical transport (elevators)
- Workshop status indicators (working/output-full/input-empty)
- Job thrashing prevention
- Starvation cascades and buffer stockpiles
- Visual diagnostics (full hands=downstream problem, empty hands=upstream problem)

#### Needs vs Jobs
- **File**: [`docs/vision/needs-vs-jobs.md`](vision/needs-vs-jobs.md)
- Personal needs system (hunger, sleep, mood) separate from job system
- Critical needs interrupt jobs
- Non-critical needs checked when idle
- Needs state machine (seeking, consuming)

---

### World & Exploration

#### Infinite World Exploration (11 Sections - Major Research)
- **File**: [`docs/vision/infinite-world-exploration.md`](vision/infinite-world-exploration.md)
- **Section 1**: Chunk-based streaming architecture (overmap + local chunks)
- **Section 2**: Delta-based persistence (store only changes vs procedural baseline)
- **Section 3**: Deterministic generation with per-chunk seeding
- **Section 4**: Spatial hashing for memory-efficient loading
- **Section 5**: Simulation LOD (abstract settlements for unloaded areas)
- **Section 6**: Entity-Component System considerations
- **Section 7**: World-bound vs entity-bound simulation
- **Section 8**: Memory analysis and optimization projections
- **Section 9**: Overmap generation techniques (Voronoi, noise, settlement placement)
- **Section 10**: Cross-chunk pathfinding with HPA*
- **Section 11**: Gap analysis vs current codebase

#### Colony Cycle Vision
- **File**: [`docs/vision/vision.md`](vision/vision.md)
- **Colony → Exodus → Colony cycle** with vehicle construction
- Vehicle types with capacity/speed/terrain tradeoffs
- Journey events (river crossings, breakdowns, weather, resources)
- Settlement ownership and "claiming" mechanics
- Trade routes between colonies
- Knowledge/blueprint persistence across colonies

---

### Architecture & Design Philosophy

#### Autarky Design Principle
- **File**: [`docs/vision/autarky.md`](vision/autarky.md)
- Every system element must have source AND sink
- Closed-loop design requirement before adding new items
- Terrain as valid source/sink

#### Stone Age Job Types
- **File**: [`docs/vision/stone-aged-dwarfjobs.md`](vision/stone-aged-dwarfjobs.md)
- Stone age appropriate job types (gatherer, hunter, fisher, toolmaker, builder, etc.)
- Building jobs: shelter builder, thatcher, wattle & daub, carpenter, mason, etc.
- Mining/excavation: digger, quarry worker, flint collector, clay digger

---

### Terrain & Tiles

#### Soil System
- **File**: [`docs/vision/terrain-and-tiles/soils.md`](vision/terrain-and-tiles/soils.md)
- 3×3 soil matrix (dry/normal/wet × poor/medium/rich)
- Edge transitions (soil↔grass, soil↔path, wall edges, waterlogged)
- Minimal starter set (5 core soil types)
- Palette guidance for visual clarity

#### Ground Cell Distribution
- **File**: [`docs/vision/terrain-and-tiles/ground-cells-distribution.md`](vision/terrain-and-tiles/ground-cells-distribution.md)
- Clay blob distribution (subsoil layer via Perlin noise)
- Gravel placement (high slope areas, rocky ground)
- Sand vs peat distinction (wetness-based)
- Soil band concept (topsoil vs subsoil)

#### Special Tiles
- **File**: [`docs/vision/terrain-and-tiles/special-tiles.md`](vision/terrain-and-tiles/special-tiles.md)
- Source/drain tiles (water sources, heat sources, cold sources, fire sources)
- Material tiles (wood vs stone variants with insulation differences)
- Tile art needed for all variants

#### Tile Catalog
- **File**: [`docs/vision/terrain-and-tiles/tiles.md`](vision/terrain-and-tiles/tiles.md)
- Comprehensive tile list (grass variants, water, structures, overlays)
- States/overlays (fire, smoke, ash, frost, wet, footprints)

#### Trees & Branching
- **File**: [`docs/vision/trees-branching.md`](vision/trees-branching.md)
- Tree type differentiation (Oak, Pine, Birch, Willow with unique branching)
- Root system gameplay (coppicing, regrowth)
- Fallen trunk mechanics (length = height, 2-stage chop loop)
- Leaf/sapling drops by tree type
- Stump regrowth tracking

#### Edible Plants
- **File**: [`docs/vision/terrain-and-tiles/edible-plants-plan.md`](vision/terrain-and-tiles/edible-plants-plan.md)
- Wild berry bushes (regrowable food source)
- Fruit trees as food source
- Foraging loop (pick→eat→regrow)
- Multi-plant types for variety

#### Material Finish Overlays
- **File**: [`docs/vision/terrain-and-tiles/material-finish-overlays.md`](vision/terrain-and-tiles/material-finish-overlays.md)
- Finish overlay system (rough/smooth/polished/engraved)
- Inset + border rendering for materials
- Sparse finish grids per tile
- Natural vs constructed defaults

---

### Freetime & Needs

#### Freetime System
- **File**: [`docs/vision/freetime.md`](vision/freetime.md)
- Furniture types (beds, chairs, tables) as entities
- Carpenter's bench for furniture crafting
- Berry bush cells with periodic regrowth
- Fruit tree variants (apples, pears, cherries)
- Energy and hunger needs (separate from job system)
- Freetime behavior state machine
- Phase 1-3 implementation roadmap

---

### Production & Workshops

#### Workshops Master Roadmap (15+ Production Chains)
- **File**: [`docs/vision/workshops-and-jobs.md`](vision/workshops-and-jobs.md)

**Phase 1**: Sawmill
- Wood → Planks/Sticks

**Phase 2**: Kiln
- Clay → Bricks
- Wood → Charcoal
- Sand → Glass

**Phase 3**: Firepit/Hearth
- Continuous fuel burn system

**Phase 4**: Stone Refinement
- Gravel crushing
- Block processing

**Phase 5**: Mason's Bench
- Packed earth
- Wattle & daub
- Concrete

**Phase 6**: Fiber Works
- Grass/Leaves → Rope/Cloth/Mulch

**Phase 7**: Toolmaker's Bench
- Stone tools
- Axes, pickaxes

**Phase 8**: Tree Roots System
- Stumps, uprooting, tannin extraction

**Phase 9**: Weaving Bench
- Withies → Baskets/Mats

**Additional Workshops**:
- **Tanning Vat**: Roots → Tannin (future: hides → leather)

**[OPEN LOOPS]**: Items needing future systems documented in file

---

## 🧹 DOCUMENTATION CLEANUP (docs/todo/)

### High Priority Stale Documentation
- **File**: [`docs/todo/cleanup.md`](todo/cleanup.md)

#### Needs Update (Features Already Implemented)
- `jobs/jobs-roadmap.md` - Bill modes listed as "pending" but all 3 are implemented
- `jobs/jobs-roadmap.md` - "More workshops" listed as pending but 7 workshops exist
- `jobs/overview.md` - Passive workshop system described as future work (fully implemented)
- `architecture/blueprint-material-selection.md` - Entire doc superseded by ConstructionRecipe system
- `rendering/material-based-rendering.md` - Marked TODO but features already exist

#### Medium Priority Stale Documentation
- `jobs/jobs-from-terrain.md` - Tree growth + grass gathering implemented but listed as future
- `nature/brush-bushes.md` - Doesn't account for vegetationGrid system
- `world/stairs.md` - Ladders + ramps already implemented as construction recipes

### Duplicated Content Needing Merge
- `world/stairs.md` + `world/multi-cell-stairs.md`
- `nature/wild-plants.md` + `nature/brush-bushes.md`
- `nature/soil-fertility.md` + `nature/moisture-irrigation.md`

### Stub Files (Too Minimal - Need Expansion or Removal)
- 9 pathfinding docs (various)
- `items/containers.md`
- `items/requester-stockpiles.md`
- `items/tool-qualities.md`
- (See cleanup.md for full list)

---

## 📂 OTHER TODO AREAS

### Deferred Features

#### Elevator System
- **File**: [`docs/todo/world/elevators.md`](todo/world/elevators.md)
- Elevator system design (deferred feature)

#### Sound & Mind System (6 Files)
- **Directory**: `docs/todo/sounds/`
- Multi-doc design system in progress

#### Pathfinding Enhancements
- **Directory**: `docs/todo/pathfinding/`
- Social navigation
- Unified spatial queries

#### Simulation Architecture
- **File**: [`docs/todo/architecture/decoupled-simulation-plan.md`](todo/architecture/decoupled-simulation-plan.md)
- Future simulation refactoring

#### UI - Pie Menu
- **File**: [`docs/todo/ui/pie-menu.md`](todo/ui/pie-menu.md)
- Right-click pie menu UI
- **Note**: Contradicts current sticky-mode model

---

## 📚 KEY REFERENCE FILES

### Project Overview & Workflow
- **File**: [`docs/CLAUDE.md`](CLAUDE.md)
- Contains project overview and docs workflow guidelines
- Recommends checking `doing/` first, then showing `todo/` summary to user
- **Important**: Never move to `done/` without explicit user permission

### Feature Completion Checklist
- **File**: [`docs/checklists/feature-completion.md`](checklists/feature-completion.md)
- Verification checklist before marking features as complete
- **Required Steps**:
  1. Write End-to-End Tests
  2. Verify In-Game or Headless
  3. Job System Wiring Checklist (9 sub-items)
  4. Use Inspector to Verify State
  5. UI for Testing (3 questions)
  6. Reuse Existing Systems (reference guidance)
  7. **Important Gate**: Never move a doc to `done/` without explicit user permission

---

## 📊 SUMMARY STATISTICS

| Category | Count |
|----------|-------|
| **Active Work Items** | 1 system (lighting - partial) |
| **Open Bugs** | 3 (1 deferred with solution, 2 active) |
| **High Priority Gaps** | 5 major systems |
| **Planned Features** | 100+ across vision docs |
| **Major Vision Systems** | 15 |
| **Workshop Types Planned** | 12+ |
| **Production Chains Planned** | 10+ |
| **Stale Docs** | 10+ needing cleanup/merge |
| **Research Projects** | 1 major (rewind feature ~1000 LOC) |

---

## 🎯 RECOMMENDED NEXT ACTIONS

1. **Glass Kiln** - Highest priority gameplay gap, closes sand autarky loop
2. **Stockpile Priority Fix** - Deferred bug with known solution (pre-sorted priority list, 5-6 hours)
3. **Lighting System Completion** - Finish remaining 5 steps of active work
4. **Composting** - Close leaves autarky loop (pairs well with glass kiln)
5. **Documentation Cleanup** - Update stale docs, merge duplicates

---

*This file was auto-generated by scanning all markdown files in the docs/ directory (excluding docs/done/). If you add new TODOs or complete work, consider regenerating this master list or updating it manually.*
