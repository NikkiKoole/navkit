# Jobs System Growth Checklist

Prioritized steps to expand the jobs system beyond hauling.

---

## Phase 1: UI for Existing Systems
- [x] Draw/remove stockpiles (click-drag rectangles)
- [x] Stockpile filters (R/G/B/O toggle with keyboard)
- [ ] Draw/remove gather zones
- [ ] Stockpile config panel (priority, stack size UI)
- [ ] Requester stockpiles - "keep N items here" mode

## Phase 2: Robustness
- [x] Unreachable cooldowns for items (don't spam-retry)
- [x] Unreachable cooldowns for designations
- [ ] Reservation timeouts (auto-release orphaned reservations)
- [ ] Connectivity regions (cheap reachability check before pathfinding)
- [ ] **Performance fixes** - see [todo/jobs-rehaul-performance.md](todo/jobs-rehaul-performance.md)

## Phase 3: Mining
- [x] Designation system (player marks tiles for work)
- [x] JOB_MOVING_TO_DIG + JOB_DIGGING states
- [x] Terrain modification (wall -> floor)
- [x] Spawn ITEM_ORANGE when mining completes
- [x] Unreachable cooldown for dig designations
- [x] Visual feedback (cyan tinted stockpile pattern)

## Phase 4: Construction (Building)
- [x] Blueprint system (BLUEPRINT_AWAITING_MATERIALS -> READY_TO_BUILD -> BUILDING)
- [x] JOB_HAULING_TO_BLUEPRINT state
- [x] JOB_MOVING_TO_BUILD + JOB_BUILDING states
- [x] Material delivery to blueprints
- [x] Require ITEM_ORANGE for building walls
- [x] Visual feedback (blue/green/yellow tinted stockpile pattern)
- [ ] Hauler/builder separation (currently same mover does both)

## Phase 5: Architecture Refactor
- [ ] Job pool separate from Mover (mover.currentJobId)
- [ ] Job Drivers (per-type step functions)
- [ ] WorkGivers (modular job producers)
- [ ] Mover capabilities/professions system

## Phase 6: Crafting
- [ ] Workshop system with recipes
- [ ] Bill modes: Do X times, Do until X, Do forever
- [ ] Input/output stockpile linking

## Optional / Later
- [ ] Zone activities (zones that generate jobs)
- [ ] Farming (multi-stage growth)
- [ ] Skills/labor priorities per mover
- [ ] Elevators for vertical transport
- [ ] Job priority reordering (mining vs hauling priority)

---

## Recently Fixed Bugs
- [x] StockpileAcceptsType rejected ITEM_ORANGE (hardcoded `type > 2`)
- [x] SpawnItem in CompleteDigDesignation used cell coords instead of pixel coords
- [x] Mover fields not initialized in InitMover (targetDigX/Y/Z, targetBlueprint, buildProgress)

---

**References:**
- [jobs-roadmap.md](jobs-roadmap.md) - Full design details
- [logistics-influences.md](logistics-influences.md) - Factorio/SimTower research
- [next-steps-analysis.md](next-steps-analysis.md) - External validation
