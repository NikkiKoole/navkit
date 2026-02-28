# Todo Index

> One-line summaries of everything in `docs/todo/`. Updated 2026-02-28.

---

## Next Up (Tier 3 — ready to implement)

- `09-onboarding-and-progression.md` — Starting scenarios, progressive need unlocks, difficulty tuning
- `10-personality-and-skills.md` — Names, skill progression (8 types), moods — the "character" layer
- `11-stockpile-filter-redesign.md` — Hierarchical filter UI for growing item count
- `12-biome-presets.md` — 8 named biome presets tuning worldgen/climate/vegetation (6 free, 2 need sprites)

## Small / Concrete Tasks

- `jobs/instant-reassignment.md` — Assign next job immediately on completion instead of waiting for AssignJobs()
- `environmental/fire-ignition-materials.md` — Fix: wood walls won't ignite, floors don't burn, tree trunks only top segment
- `environmental/fire-improvements.md` — Ground fire spread + fire pit tool requirement (deferred from shelter)
- `gameplay/water-placement-tools.md` — Player actions to place/remove water sources, springs, pumps, drains
- `mover-control-and-work-radius.md` — Constrain mover movement (leash radius, distance weighting, draft mode)
- `transient-state-save.md` — Known save/load gaps (designation progress lost, etc.)

## Architecture / Performance

- `architecture/unified-roadmap.md` — Master architecture plan tying 7 docs into Era 2 build order
- `architecture/capability-based-tasks.md` — Tasks resolve based on nearby capabilities, not building gates
- `architecture/performance-dod-audit.md` — DOD struct analysis (A* nodes, item scanning bottlenecks)
- `architecture/performance-dod-audit-jobs.md` — Jobs.c deep-dive: O(workshops×bills×items) and fixes
- `architecture/rendering-optimization-candidates.md` — Rendering pipeline analysis (no active bottleneck yet)
- `architecture/simd-optimization-candidates.md` — SIMD opportunities if scale increases
- `architecture/decoupled-simulation-plan.md` — Decouple pathfinding/sim from rendering for multiple views

## Gameplay Design (needs scoping before implementing)

- `gameplay/workshops-master.md` — Living doc tracking workshop additions, source-sink-feedback principles
- `gameplay/more-tiles-design.md` — Next small tiles: hearth, clay pit, mud mixer
- `gameplay/water-dependent-crafting.md` — Workshops requiring location-based resources (riverbed, shade, fire)
- `gameplay/flow-simulation.md` — Vector fields to bias water/smoke/fire spread deterministically
- `gameplay/seasoning-curing.md` — Material states (green/seasoned/rotten wood) changing over time
- `gameplay/balance-and-rate-tuning.md` — Guide for tuning interconnected rate-based systems
- `gameplay/primitives-missing.md` — Status tracker: which primitive items/cells are done vs needed

## Vision / Long-term

- `sims-inspired-priorities.md` — What makes The Sims work; reorder next sessions for "Sims feel"
- `naked-in-the-grass.md` — Gap analysis: 8/10 survival features complete, what's next
- `naked-in-the-grass-details.md` — Deep dive on tool qualities and animals design
- `workshop-evolution-plan.md` — Migration from DF-style templates to CDDA-style entity/tool workshops
- `world/elevators.md` — Moving platforms with state, capacity, queuing
- `world/trains.md` — Horizontal multi-stop transport (track + station)
- `world/more-tiles.md` — Tier progression inventory (Survival → Farming → Industrial → Urban)
- `items/future-ideas.md` — Containers, tool durability, scattered brainstorm
- `pathfinding/future-ideas.md` — Flow fields, formations, territory costs
- `pathfinding/social-navigation.md` — Queuing and crowd behavior (prerequisite for elevators/trains)
- `pathfinding/unified-spatial-queries.md` — Consolidate duplicated "find nearest" patterns
- `ui/pie-menu.md` — Radial context menu research (Fitts's Law, mark-ahead)

## Discussions with AI (research / brainstorms)

- `discussions with ai/first fire.md` — Making fire discovery difficult: material science, tinder, ember physics
- `discussions with ai/farming.md` — Deep farming: soil NPK matrix, crop rotation, weather effects
- `discussions with ai/steampower.md` — Steam engines, boiler loops, power transmission, hazards
- `discussions with ai/wate into resurce.md` — Biology loop: needs + infrastructure + composting
- `discussions with ai/grand colony.md` — Scaling from 1 person to city: shadow labor, tech eras
- `discussions with ai/notdfworkshops.md` — CDDA-style freeform construction, tools as entities

## Sound System (research track)

- `sounds/sound-and-mind-research.md` — Index of 5 focused research tracks
- `sounds/sound-and-mind-design.md` — Design pillars: meaningful, minimal, local, visible
- `sounds/sound-and-mind-speech-comm.md` — Agent-to-agent communication with token palette
- `sounds/sound-and-mind-birdsong-sex.md` — Evolving song for mate choice
- `sounds/sound-concept-generator.md` — Tool to prevent concept gaps when adding items/jobs
- `sounds/sound-needs-emotions-groundwork.md` — Minimal needs + derived emotions for communication

## Known Conflicts & Decisions

1. **Workshop architecture: templates vs freeform** — `workshop-evolution-plan.md` (keep templates) vs `notdfworkshops.md` (kill templates) vs `capability-based-tasks.md` (bridge both). **Decision:** follow capability-based-tasks — add capability tags to existing workshops, slowly phase out templates as capability system matures.

2. **Feature ordering: mood vs skills** — `10-personality-and-skills.md` (skills first) vs `sims-inspired-priorities.md` (mood first). **Decision:** mood comes first — it's the connective tissue that makes needs and skills meaningful. Names are already implemented.

3. **Comfort need ↔ furniture gap** — `sims-inspired-priorities.md` assumes furniture drives comfort, `capability-based-tasks.md` provides the mechanism (capability levels), but neither connects them explicitly. **Fix when implementing:** tie comfort recovery to furniture capability levels (chair = seating:1, throne = seating:3).

4. **Personality traits vs behavioral dithering** — `10-personality-and-skills.md` (explicit trait data) vs `sims-inspired-priorities.md` (cheap randomness in job selection). **Decision:** implement dithering first (cheap, emergent), defer trait data to later.

5. **Dev scenarios vs player scenarios** — `09-onboarding-and-progression.md` has both. **Decision:** ship both — player scenarios in survival mode, dev scenarios in sandbox mode.

6. **Mud/cob duplication** — `workshops-master.md` and `water-dependent-crafting.md` had identical sections. **Fixed:** workshops-master now links to water-dependent-crafting as source of truth.

## Superseded (keep as reference)

- `feature-03-doors-shelter.md` — Original F03 design, superseded by `docs/done/03-*`
- `feature-04-tools-knapping.md` — Original F04 design, superseded by `02-tool-quality-*`
