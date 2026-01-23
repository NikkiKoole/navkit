# Jobs System Growth Checklist

Prioritized steps to expand the jobs system beyond hauling.

---

## Phase 1: UI for Existing Systems
- [ ] Draw/remove stockpiles (click-drag rectangles)
- [ ] Draw/remove gather zones
- [ ] Stockpile config panel (filters, priority, stack size)
- [ ] Requester stockpiles - "keep N items here" mode

## Phase 2: Robustness
- [ ] Reservation timeouts (auto-release orphaned reservations)
- [ ] Connectivity regions (cheap reachability check before pathfinding)

## Phase 3: Mining
- [ ] Designation system (player marks tiles for work)
- [ ] JOB_MOVING_TO_DESIGNATION + JOB_WORKING states
- [ ] Terrain modification (wall -> floor + spawn item)

## Phase 4: Architecture Refactor
- [ ] Job pool separate from Mover (mover.currentJobId)
- [ ] Job Drivers (per-type step functions)
- [ ] WorkGivers (modular job producers)

## Phase 5: Construction
- [ ] Blueprint system
- [ ] Chained jobs (deliver materials -> build)
- [ ] Material reservations

## Phase 6: Crafting
- [ ] Workshop system with recipes
- [ ] Bill modes: Do X times, Do until X, Do forever
- [ ] Input/output stockpile linking

## Optional / Later
- [ ] Zone activities (zones that generate jobs)
- [ ] Farming (multi-stage growth)
- [ ] Skills/labor priorities per mover
- [ ] Elevators for vertical transport

---

**References:**
- [jobs-roadmap.md](jobs-roadmap.md) - Full design details
- [logistics-influences.md](logistics-influences.md) - Factorio/SimTower research
- [next-steps-analysis.md](next-steps-analysis.md) - External validation
