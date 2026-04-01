# NavKit Master TODO List

*Last updated: 2026-03-31*

---

## Active Work (docs/doing/)

All current `doing/` items are soundsystem/PixelSynth related:
- **Interactive music system** — conductor architecture for game-reactive music
- **Song file format** — DAW save/load (phases 1-3 done, phase 4+ remaining)
- **SFX system cleanup** — preparation for conductor integration
- **Arrangement view** — GarageBand-style arrangement
- **MIDI to sequencer** — conversion pipeline

---

## Open Bugs (docs/bugs/)

### 1. Wall-Build Mover Pop
- **File**: `docs/bugs/wall-build-pop.md`
- **Issue**: After building a wall, mover pops to illogical location
- **Status**: OPEN

### 2. High-Speed Stuck Movers
- **File**: `docs/bugs/high-speed-stuck.md`
- **Issue**: At game speed 100, movers get stuck. Changing speed unsticks them.
- **Status**: OPEN

---

## Partial (code exists, more to build)

### Mood, Moodlets & Personality
- **File**: `docs/todo/mood-moodlets-personality.md`
- **Status**: Foundation shipped (save v90). mood.h/c, 5 traits, squared-curve, speed multiplier, 31 tests.
- **Phase 1 done**: wired into game loop, `moodEnabled = true` by default, save migration v89→v90
- **Remaining**: weather/light/dirt/food/activity/room moodlet triggers, more traits, UI indicators

### Dollhouse Domestic Life
- **File**: `docs/todo/dollhouse-domestic-life.md`
- **Status**: Phases 1-3 done, phases 4-5 next
- **Direction**: Shifting from colony manager toward "watch little people live" (Sims over DF)
- **Key ideas**: action queues, advertisement system, freetime-first strategy, kitchen stations

### Trains
- **File**: `docs/todo/world/trains.md`
- **Status**: Phases 1-4 done, phase 5 partial
- **What works**: track, stations, mover boarding, auto-decision

### Biome Presets
- **File**: `docs/todo/12-biome-presets.md`
- **Status**: Implemented, needs verification. 8 named presets (6 working, 2 need sprites)

### Juice Effects
- **File**: `docs/todo/juice-effects.md`
- **Status**: Partial. Screen shake, sprite tweens, particles for game feel.

---

## Specs (ready to build)

### Gameplay
- `village-next-steps.md` — Room quality → mood, distance weighting, occupations, furniture (6-step build order)
- `09-onboarding-and-progression.md` — Starting scenarios, progressive need unlocks, difficulty tuning
- `schedule-system.md` — Clock-driven daily routines (work/meal/leisure/sleep blocks)
- `10-personality-and-skills.md` — Skill progression (8 types), deeper personality. The "character" layer.
- `11-stockpile-filter-redesign.md` — Hierarchical filter UI for growing item count

### Environment & World
- `environmental/fire-improvements.md` — Ground fire spread + fire pit tool requirement
- `environmental/fire-ignition-materials.md` — Wood walls won't ignite, floors don't burn
- `gameplay/water-placement-tools.md` — Place/remove water sources, springs, pumps, drains
- `gameplay/water-dependent-crafting.md` — Workshops requiring location-based resources
- `gameplay/seasoning-curing.md` — Material states (green/seasoned/rotten wood)
- `gameplay/more-tiles-design.md` — Hearth, clay pit, mud mixer
- `world/elevators.md` — Moving platforms with state, capacity, queuing

### Architecture & Jobs
- `jobs/instant-reassignment.md` — Assign next job on completion (skip AssignJobs wait)
- `jobs/jobs-c-refactor.md` — Modularize jobs.c (6k lines, ~85 functions)
- `transient-state-save.md` — Known save/load gaps (designation progress lost, etc.)
- `architecture/capability-based-tasks.md` — Tasks resolve by nearby capabilities, not building gates
- `architecture/decoupled-simulation-plan.md` — Decouple pathfinding/sim from rendering

### Pathfinding
- `pathfinding/transport-layer.md` — Unified abstraction for vehicles, queues, multi-leg paths
- `pathfinding/social-navigation.md` — Queuing and crowd behavior (prerequisite for elevators/trains)
- `pathfinding/unified-spatial-queries.md` — Consolidate duplicated "find nearest" patterns

---

## Ideas & Vision (docs/vision/)

### Near-Future
- `endgame-village-vision.md` — 20th-century village destination: daily life, economy, social, housing
- `sims-inspired-priorities.md` — What makes The Sims work; reorder for "Sims feel"
- `workshop-evolution-plan.md` — Migration from DF-style templates to CDDA-style entity/tool workshops
- `mover-control-and-work-radius.md` — Leash radius, distance weighting, draft mode

### World & Exploration
- `infinite-world-exploration.md` — Chunk streaming, delta persistence, overmap, cross-chunk HPA*
- `vision.md` — Colony → Exodus → Colony cycle, vehicles, trade routes

### Simulation & Entropy
- `entropy.md` / `entropy_systems.md` — Everything decays; maintenance is gameplay
- `steam-system.md` — Steam generation, pressure, steam machines (far future)

### Farming & Nature
- `farming-overhaul-ccdda.md` — Plant needs, weather, soil types, pest/disease, multi-harvest perennials
- `trees-branching.md` — Tree differentiation, root systems, coppicing, stump regrowth
- `edible-plants-plan.md` — Wild berry bushes, fruit trees, foraging loop

### Terrain & Materials
- `terrain-and-tiles/soils.md` — 3x3 soil matrix, edge transitions
- `terrain-and-tiles/material-finish-overlays.md` — Rough/smooth/polished/engraved overlays
- `terrain-and-tiles/special-tiles.md` — Source/drain tiles, material variants

### Sound & Mind (6 research docs)
- Agent-to-agent communication with token palettes
- Evolving birdsong for mate choice
- Needs + derived emotions for communication
- Sound concept generator tool

### Far Future
- `far future/mechanisms-and-signals.md` — Mechanical logic
- `far future/gates-and-processors.md` — In-game computing

---

## Research & Reference (docs/todo/)

- `00-priority-order.md` — Master survival roadmap (tiers 0-2 complete, tier 3 in progress)
- `gameplay/workshops-master.md` — Workshop additions tracker, source-sink-feedback principles
- `gameplay/balance-and-rate-tuning.md` — Guide for tuning rate-based systems
- `gameplay/primitives-missing.md` — Primitive items/cells status tracker
- `architecture/performance-dod-audit.md` — DOD struct analysis, A* nodes, item scanning
- `architecture/simd-optimization-candidates.md` — SIMD opportunities at scale
- `ui/pie-menu.md` — Radial context menu research

---

## What's Been Completed (docs/done/)

~170 docs in `docs/done/`. Major milestones in reverse chronological order:

| Save Ver | Feature |
|----------|---------|
| v90 | Mood, moodlets, 5 personality traits |
| v82 | Loop closers: glass, lye, mortar, windows, mud/cob, reeds |
| v79 | Thirst, water pots, beverages (tea, juice) |
| v78 | Clothing & textiles: loom, tailor, 7 items, temperature |
| v77 | Farming crops: wheat/lentils/flax, growth, quern, cooking |
| v76 | Farming land: tilling, composting, fertilizing |
| v75 | Fog of war, exploration, scouting |
| v74 | Animal respawn (edge-spawn, population cap) |
| v73 | Food spoilage, item condition, refuse pile |
| v70 | Root foraging (dig roots, roast, dry) |
| v69 | Mover DOD optimization (12KB→152B per mover) |
| v67 | Primitive shelter, doors, ground fire, fire pit |
| v66 | Tool items (digging stick, stone axe/pick/hammer) |
| v65 | Tool quality framework (5 types, 3 levels, speed scaling) |
| v64 | Workshop deconstruction |
| v63 | Workshop construction costs |
| v55 | Balance system, game-time scaling |
| v54 | Furniture system |
| v53 | Energy/sleep need |
| v50 | Item containment (containers) |
| v49 | Item stacking |
| v45 | Weather & seasons (mud, snow, wind, cloud shadows) |
| v43 | Seasons |
| v37 | Lighting (sky light + colored block light) |

Also complete: rooms, terrain sculpting, shape/material refactor, floor dirt tracking, cutscenes, game modes (sandbox/survival), event log, state audit, 20+ audits.

---

## Known Conflicts & Decisions

1. **Workshop architecture**: templates vs freeform vs capability-based → follow capability-based-tasks (add capability tags, slowly phase out templates)
2. **Mood vs skills ordering**: mood first — it's the connective tissue that makes needs and skills meaningful
3. **Comfort ↔ furniture gap**: tie comfort recovery to furniture capability levels when implementing
4. **Personality traits vs behavioral dithering**: implement dithering first (cheap, emergent), defer trait data
5. **Dev vs player scenarios**: ship both — player scenarios in survival, dev scenarios in sandbox

---

## Summary

| Category | Count |
|----------|-------|
| Open bugs | 2 |
| Partial (in progress) | 5 |
| Specs (ready to build) | ~18 |
| Vision/ideas | ~40 docs |
| Completed features | ~170 docs |
| Save version | 90 |
| Source lines (C) | ~129K (69K src + 60K tests) |
| Test suites | 46+ |
