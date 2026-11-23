# Todo Index

> One-line summaries of everything in `docs/todo/`, grouped by status, plus `docs/research/`.
> Start with **Recently active**: what is being worked on now. Each session keeps its own line
> there up to date when it finishes something (newest first; drop lines older than ~2 weeks into
> their section below). Last tidied 2026-11-23.

## Recently active

- **2026-11-23 Tide (getij)** — `world/tide.md`: spike works (scenario `tide`, `src/simulation/tide.c`): the sea's top level is one body (`WaterCell.isSea`, flood fill from the edge) whose level follows a 12.42 h cosine; flats fall dry, movers go out at low tide, get cut off on the islet, come back at the next low tide (no mover logic added). Step 0.03-0.1 ms for 1639 sea cells. The edge sea (navkit-8e) owns rivers and deeper levels, skips sea cells when the tide is on. Saved (v102). Scenario `tide-island`: the island's sea with ramps from the beach down to the flats, 7.7k cells fall dry at 256; spikes (HPA 28 ms) when the whole sea crosses wading depth at once. Tide wave (chunk-wide strips sweep across: biggest nav update 362 -> 34 chunks); caught movers wade up to 24 cells to dry ground; island game option `islandTide` (off by default). Not yet: food on the flats, dikes
- **2026-11-23 Conveyor belts** — `world/conveyors.md`: spike done (scenario `belt`), items and crates between stockpiles; stockpile transfer points (`StockpileTakeStack` / `StockpilePutLoad`); vertical belts: lifts and chutes (scenario `belt-lift`); built in game from blueprints, Work → Build → Belt (scenario `belt-build`); saved since v98; feeds a sawmill through a hopper stockpile, a belt on the output tile carries the planks (scenario `belt-workshop`); play HUD Build Belt tool (with planks); cleared on game start
- **2026-11-23 Trains + elevator** — `world/trains.md`: coloured lines, trip choice over a transit graph (any number of changes), subway across levels (scenarios `metro`, `transfer`, `transit-chain`, `subway`, `subway-deep`); freight: cargo stockpiles at stops (scenario `cargo`, save v97); lines rebuilt after a load (fixed: haulers stalled after loading); train settings saved (v99). `world/elevators.md`: passenger elevator as a pathfinder link (shaft cells, one car, capacity, SCAN), waiting line at the door, the expected wait in the route cost (movers take the stairs when the cars are busy), scenario `elevator` (two shafts + stairs); built in game (Work → Build → Elevator, play HUD), scenario `elevator-build`, settings saved (v100); empty cars sweep (no turning round for one person), scenario `elevator-tall`; the wait is paid once per ride (nav hooks NAV_ENTER_EXTRA, NAV_RAMP_EXTRA), scenario `elevator-stairs`; built ramps are stairs (climbed slower, cost more; slopes unchanged)
- **2026-11-23 Edge-fed rivers** (`world/water.md`, `terrain.c` hills-water): rivers enter full at one map edge and leave at the other, banks one level above the water (valleys, no canyons). Survival water levels off on all seeds; reachable shore 470-655 cells (was 0-20). Island keeps springs at river heads (drainage-built rivers, sea beyond the edge). Note: survival has fog of war, so tick-0 "find water" searches see nothing unexplored
- **2026-11-23 Water drains off the map edge (save v101)** (`water.c`, `terrain.c`, `island_gen.c`): beyond the edge lies a sea at `waterEdgeSeaZ` (island) or nothing; the island's sea levels off. Hills-water rivers end in an outlet on the edge, lakes have no source. Survival seed 0 levels off; seeds 1/3 still grow slowly (river heads on high ground). Next: spring/source model (flow rate, not infinite refill)
- **2026-11-23 Water 4-5x faster; movers wade out of deep water** (`water.c`, `mover.c`, chunked-world-plan.md "A lived-in island"): full cells on full water kept re-waking each other, so lakes/seas never rested and the 4096/tick budget starved real flow. Island water 2.0-2.9 -> 0.4-0.55 ms/tick. Water now spreads as the uncapped sim does (survival springs flood a basin); movers caught in deep water (floods, 1-wide rivers: island seeds 0/5/7) wade to dry ground. Open: seas/springs with no outlet keep growing
- **2026-11-23 Island one-way pockets fixed** (`world/island.md` "One-way pockets", with 8e): ramps are placed so their sides lead back; a forward/backward search over the pathfinder's own steps (`NavSteps`) gives every pocket or cut-off patch a two-way ramp. 256 islands: 68-437 pocket cells -> 0, main land unchanged. Open: 128 islands mostly cut off (before too)
- **2026-11-23 Island job spikes fixed** (`jobs.c`, chunked-world-plan.md "A lived-in island"): a settler stranded in a one-way ramp pocket re-ran ~250 failing blueprint-haul path searches every few ticks; now remembered per mover. Jobs 1.0 -> 0.14 ms/tick, slow ticks 215 -> 5. Open: one-way pockets (regions can't see them)
- **2026-11-23 Lived-in island measured** — `architecture/chunked-world-plan.md` "A lived-in island": scenario `island-colony` (20 settlers, 3 game days); world sim ~2.9 ms/tick (water 2.1) is the biggest cost, jobs 0.2-0.3 ms (an earlier 7.3 ms figure came from a dirty build, retracted); simulation layers live in under a third of the blocks; winter colony `island-winter` (fire pit, torches, snow): heat/light/fire/smoke under 3% of blocks, snow is a per-column surface layer (54% of blocks, one cell per column)
- **2026-11-23 Island** — `world/island.md`: terrain + game `island` (survival rules, 3 settlers ashore by a river mouth); sizes 256-1024
- **2026-11-23 Taller worlds** — `architecture/chunked-world-plan.md` "Taller worlds: block palettes": a much higher world costs little more (memory follows the surface); decision point 4
- **2026-11-23 Survival needs fixes** (no doc; 9d160592, cc7877b, b30a97d5, 5beed28d, 4a82460d, e5ff0b53): mover walked all day after supplies it couldn't reach; a mover with no food still drinks and sleeps; natural water found by walking distance (NavFindNearest); a need gives up at once when the pathfinder finds no way (moverRepathFails); ramps are no one-way doors (no stepping off sideways); the settler starts within ~24 cells of reachable water (seen past the fog: it only finds water it has seen). With navkit-8e's edge-fed rivers, seeds 0-7 over 5 days: all 8 live until the 5 berries are gone and starve on day 5. Open: food (forage when hungry? design question); idle wandering / exploring for water (design question)
- **2026-11-23 Game hours everywhere** — building times per recipe (tiers), animal timers, food cooling (setting `foodCooling`), dev sidebar intervals, tap furniture (no doc; commits a0b9be1c, 62a9599d, 0c3ef6a7, 2ba49ddf, caa6c8a)
- **2026-11-22 Day rhythm and needs** — `schedule-system.md`: soft rhythm (lie in while dark, early nights, sleep metabolism), 6-minute day with 3x walking, `--day-budget` tool (see root `CLAUDE.md`), tap furniture, tables, job distance weighting
- **2026-11-22 Mover stuck fixes** (no doc, `mover.c`): tunnel-yield jam (aa24841), deep ice walkable (f04d21a), ramp-top loop (d48c3f6), death releases bed/food (68b8976), final waypoint must be in the goal cell (c840424); new spawns only where they can reach the main area (5f201cd)
- **2026-11-22 Big maps, 5000 movers** — `architecture/chunked-world-plan.md` (end): yield cache, stranded spawns, steady state; next step only against a real-play target
- **2026-11-22 City 1999 (vision)** — `../vision/city-1999.md`: ownership, inventory, bins, coins, keys, two milestones
- **2026-11-22 Vehicles (research)** — `../research/vehicles.md`
- **2026-11-21 Rendering** — `architecture/rendering-optimization-candidates.md`: instanced world batch, per-column visibility, chunk cache design (parked); `--screenshot` (root `CLAUDE.md`)

Older history: `git log --since=YYYY-MM-DD --format="%ad %s" --date=short` (and `-- docs` for doc changes).

---

## Implemented (moved to done/)

- ~~`12-biome-presets.md`~~ → `docs/done/`
- ~~`feature-03-doors-shelter.md`~~ → `docs/done/` (superseded)
- ~~`feature-04-tools-knapping.md`~~ → `docs/done/` (superseded)
- ~~`primitives-missing.md`~~ → `docs/done/gameplay/`
- ~~`water-dependent-crafting.md`~~ → `docs/done/gameplay/`
- ~~`jobs-roadmap.md`~~ → `docs/done/jobs/`
- ~~`platform-cell.md`~~ → `docs/done/pathfinding/`

## Partial (code exists, more to build)

- `mood-moodlets-personality.md` — Mood system foundation shipped (mood.h/c, 5 traits, squared-curve, 31 tests). 8 phases: wire into game, hook needs triggers, weather/light/dirt/food/activity/room moodlets, more traits
- `10-personality-and-skills.md` — Names, skill progression (8 types), moods — the "character" layer
- `juice-effects.md` — Screen shake, sprite tweens, particles, screen flashes for game feel
- `world/island.md` — Finite island grown from a seed (drainage, rivers, cover, loose items); terrain, scenario and game `island`; next: measure a lived-in island
- `world/water.md` — Water simulation: settled water rests, map edge as outlet (v101), edge-fed rivers, wading out; open: trickling springs + soaking, fair update order
- `world/trains.md` — Horizontal multi-stop transport: track, stations, mover boarding, auto-decision (phases 1–4 done, phase 5 partial)

## Specs (ready to build)

- `village-next-steps.md` — Bridge from survival to settlement: room quality, mood, distance weighting, elevators, occupations, furniture (6-step build order)
- `09-onboarding-and-progression.md` — Starting scenarios, progressive need unlocks, difficulty tuning
- `11-stockpile-filter-redesign.md` — Hierarchical filter UI for growing item count
- `schedule-system.md` — Clock-driven daily routines (work/meal/leisure/sleep blocks) layered on existing job system; soft day rhythm (lying in while dark, early night when idle, sleep metabolism) done
- `transient-state-save.md` — Known save/load gaps (designation progress lost, etc.)
- `jobs/instant-reassignment.md` — Assign next job immediately on completion instead of waiting for AssignJobs()
- `jobs/jobs-c-refactor.md` — Concrete modularization plan for jobs.c (6k lines, ~85 functions)
- `environmental/fire-improvements.md` — Ground fire spread + fire pit tool requirement (deferred from shelter)
- `environmental/fire-ignition-materials.md` — Fix: wood walls won't ignite, floors don't burn, tree trunks only top segment
- `gameplay/water-placement-tools.md` — Player actions to place/remove water sources, springs, pumps, drains
- ~~`gameplay/water-dependent-crafting.md`~~ — moved to done/ (mud mixer implemented)
- `gameplay/seasoning-curing.md` — Material states (green/seasoned/rotten wood) changing over time
- `gameplay/more-tiles-design.md` — Next small tiles: hearth, clay pit, mud mixer
- `world/elevators.md` — (partial: passenger car as a pathfinder link built, scenario `elevator`) Moving platforms with state, capacity, queuing
- `world/conveyors.md` — Conveyor belts: spike done (scenarios `belt`, `belt-lift`: items + crates, lifts and chutes, stockpile pick-up/drop-off); brainstorm and open design questions
- `pathfinding/per-chunk-edge-storage.md` — Incremental HPA* updates touch all edges (67 ms per mining update on 512x512 hills); store edges per chunk
- `pathfinding/service-points-and-queues.md` — (steps 1, 2, 4 done; left: station waiting lists as service points, saved state) General wait-your-turn system (ladders, train doors, elevators, ...): explicit first-come-first-served lines, emergent follow-the-one-ahead queue shape, measured expected wait as route cost
- `pathfinding/astar-priority-queue-and-extraction.md` — `RunAStar()` linear-scans instead of using the existing heap (~32s/path at 256×256, measured); plus what it'd take to lift the pathfinder into another project
- `pathfinding/transport-layer.md` — Unified abstraction for vehicles, queues & multi-leg pathfinding (CapacityNode pattern)
- `pathfinding/social-navigation.md` — Queuing and crowd behavior (prerequisite for elevators/trains)
- `pathfinding/unified-spatial-queries.md` — Consolidate duplicated "find nearest" patterns
- `architecture/capability-based-tasks.md` — Tasks resolve based on nearby capabilities, not building gates
- `architecture/decoupled-simulation-plan.md` — Decouple pathfinding/sim from rendering for multiple views

## Ideas & Vision

- `world/tide.md` — Tide: sea as one body, level set by a 12.4 h cosine; mud flats, islets at low tide, dikes later; surf drawing as a side idea
- `board-for-parallel-sessions.md` — A work board for the parallel sessions, modelled on fp.engine.new's file-backed kanban (cards, claims with leases; `ready` not human-only here); three C-flavoured ways (shell, small C tool, raylib view); not built
- `endgame-village-vision.md` — 20th-century village destination picture: systems inventory, daily life, economy, social, housing
- `../vision/city-1999.md` — Homeless in a 1999 city: ownership, keys/locks, Cataclysm-style inventory (pockets, bags, cart), trash/bin/deposit loop, coins, city stations, missing systems, item cap analysis, two milestones
- `sims-inspired-priorities.md` — What makes The Sims work; reorder next sessions for "Sims feel"
- `naked-in-the-grass.md` — Gap analysis: 8/10 survival features complete, what's next
- `naked-in-the-grass-details.md` — Deep dive on tool qualities and animals design
- `workshop-evolution-plan.md` — Migration from DF-style templates to CDDA-style entity/tool workshops
- `mover-control-and-work-radius.md` — Constrain mover movement (leash radius, distance weighting, draft mode)
- `world/more-tiles.md` — Tier progression inventory (Survival → Farming → Industrial → Urban)
- `items/future-ideas.md` — Containers, tool durability, scattered brainstorm
- ~~`jobs/jobs-roadmap.md`~~ — moved to done/ (farming/hunting all implemented)
- `gameplay/more-tiles-design.md` — Mostly done (hearth+mud mixer shipped), only clay pit remains
- `pathfinding/future-ideas.md` — Flow fields, formations, territory costs
- `gameplay/flow-simulation.md` — Vector fields to bias water/smoke/fire spread deterministically
- `architecture/large-worlds.md` — What bigger maps cost (memory, graph updates, sims), improvements in order, estimates for 1024 and 2048
- `architecture/chunked-world-plan.md` — Big maps (partial): accessor, map-sized and sparse layers done, 2048 runs; latest measurements (5000 movers, 2026-11-22) and next step at the end
- `ui/game-structure.md` — One engine, several games: games table, settings table, actions, describe layer, SimStep, selection + panels + tab bar; what's left per step
- `ui-redesign.md` — Bottom panel + Huisje/Boompje/Beestje tabs + selection model (selection, panels, tabs done; icons, roster views, removing the text bar left)
- `architecture/unified-roadmap.md` — Master architecture plan tying 7 docs into Era 2 build order

## Research & Reference

- `00-priority-order.md` — Master priority list for survival mode feature roadmap (historical: Feb 2026)
- `gameplay/workshops-master.md` — Living doc tracking workshop additions, source-sink-feedback principles
- `gameplay/balance-and-rate-tuning.md` — Guide for tuning interconnected rate-based systems
- ~~`gameplay/primitives-missing.md`~~ — moved to done/ (all primitive items implemented)
- `ui/pie-menu.md` — Radial context menu research (Fitts's Law, mark-ahead); parked
- `ui/icon-list.md` — Icons to replace text buttons (themes + per-button list)
- `architecture/performance-dod-audit.md` — DOD struct analysis (A* nodes, item scanning bottlenecks)
- `architecture/performance-dod-audit-jobs.md` — Jobs.c deep-dive: O(workshops×bills×items) and fixes
- `architecture/rendering-optimization-candidates.md` — Rendering pipeline analysis (no active bottleneck yet)
- `architecture/simd-optimization-candidates.md` — SIMD opportunities if scale increases
- `sounds/sound-and-mind-research.md` — Index of 5 focused research tracks
- `sounds/sound-and-mind-design.md` — Design pillars: meaningful, minimal, local, visible
- `sounds/sound-and-mind-speech-comm.md` — Agent-to-agent communication with token palette
- `sounds/sound-and-mind-birdsong-sex.md` — Evolving song for mate choice
- `sounds/sound-concept-generator.md` — Tool to prevent concept gaps when adding items/jobs
- `sounds/sound-needs-emotions-groundwork.md` — Minimal needs + derived emotions for communication

## Research findings (`docs/research/`)

Measured findings and broad research; not plans by themselves.

- `../research/vehicles.md` — Vehicles: how other colony games do them, what fits (incl. pushables: sledge = wheelbarrow = cart); no plan picked (2026-11-22)
- `../research/metro.md` — Metro, tram, underground brainstorm (2026-11-22; trains work continues in `world/trains.md`)
- `../research/many-destinations-navigation.md` — Thousands of agents, many destinations: full vs hierarchical flow fields vs HPA* (2026-11-19)
- `../research/crowd-avoidance-comparison.md` — Which local avoidance for big crowds on flow fields (2026-11-19)
- `../research/3d-crowd-bottlenecks.md` — Ladder bottlenecks and queue-aware routing in 3D (2026-11-19)
- `../research/nav-fuzz-findings.md` — What fuzzing the nav library found and fixed (2026-11-19)
- `../research/df-tile-reference.md` — Dwarf Fortress tile system reference (Feb 2026)
- `../research/reverse-time-feature.md` — Rewind feature research (Feb 2026)
- `../reference/` — Older analyses (Feb 2026): gap matrix and implementation order vs vision docs, rendering performance

## Discussions with AI

- `discussions with ai/first fire.md` — Making fire discovery difficult: material science, tinder, ember physics
- `discussions with ai/farming.md` — Deep farming: soil NPK matrix, crop rotation, weather effects
- `discussions with ai/steampower.md` — Steam engines, boiler loops, power transmission, hazards
- `discussions with ai/wate into resurce.md` — Biology loop: needs + infrastructure + composting
- `discussions with ai/grand colony.md` — Scaling from 1 person to city: shadow labor, tech eras
- `discussions with ai/notdfworkshops.md` — CDDA-style freeform construction, tools as entities

## Superseded (moved to done/)

- ~~`feature-03-doors-shelter.md`~~ → `docs/done/`
- ~~`feature-04-tools-knapping.md`~~ → `docs/done/`

---

## Known Conflicts & Decisions

1. **Workshop architecture: templates vs freeform** — `workshop-evolution-plan.md` (keep templates) vs `notdfworkshops.md` (kill templates) vs `capability-based-tasks.md` (bridge both). **Decision:** follow capability-based-tasks — add capability tags to existing workshops, slowly phase out templates as capability system matures.

2. **Feature ordering: mood vs skills** — `10-personality-and-skills.md` (skills first) vs `sims-inspired-priorities.md` (mood first). **Decision:** mood comes first — it's the connective tissue that makes needs and skills meaningful. Names are already implemented.

3. **Comfort need ↔ furniture gap** — `sims-inspired-priorities.md` assumes furniture drives comfort, `capability-based-tasks.md` provides the mechanism (capability levels), but neither connects them explicitly. **Fix when implementing:** tie comfort recovery to furniture capability levels (chair = seating:1, throne = seating:3).

4. **Personality traits vs behavioral dithering** — `10-personality-and-skills.md` (explicit trait data) vs `sims-inspired-priorities.md` (cheap randomness in job selection). **Decision:** implement dithering first (cheap, emergent), defer trait data to later.

5. **Dev scenarios vs player scenarios** — `09-onboarding-and-progression.md` has both. **Decision:** ship both — player scenarios in survival mode, dev scenarios in sandbox mode.

6. **Mud/cob duplication** — `workshops-master.md` and `water-dependent-crafting.md` had identical sections. **Fixed:** workshops-master now links to water-dependent-crafting as source of truth.
