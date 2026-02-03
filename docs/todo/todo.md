# NavKit TODO

Small remaining items and future improvements.

---
## Prioritized TODO

### Short-term (meaningful improvements)
- [ ] **Variable movement costs** - road (cheap), rubble/mud (expensive) - NOTE: disables JPS/JPS+

### Medium-term (new capabilities)
- [ ] **Ramps** - directional entry (N/E/S/W), single cell z-transition
- [ ] **Multi-cell stairs** - horizontal offset while changing z-level (enter at one x,y, exit at different x,y)
- [ ] **Queuing system** - orderly waiting at bottlenecks (ladders, doors, narrow passages). Prerequisite for elevators and other social navigation features.
- [ ] **Refactoring** - dedupe heap implementations, direction arrays, entrance building (~200 lines saved)
- [ ] **Jobs expansion** - See [jobs-checklist.md](../jobs/jobs-checklist.md) for remaining work
  - [ ] Stockpile config panel (priority, stack size UI)
  - [ ] Requester stockpiles ("keep N items here")
  - [ ] More workshops (carpenter, smelter, forge)
  - [ ] Bill modes (Do X times, Do until X, Do forever)
- [ ] **Containers** - Portable storage (chests, barrels, wagons, bins, bags). Containers are items themselves (can be hauled, stored in stockpiles) but also hold other items inside.

### Larger efforts (build on queuing)
- [ ] **Elevators (full sim)** - moving elevator with state, queuing, capacity, wait times (requires queuing)
- [ ] **Social Navigation (full)** - yielding, lane preference, personal space (requires queuing)
- [ ] **Decoupled Simulation** - headless backend + 2.5D view (multi-phase)
- [ ] **Flow Fields** - for many agents sharing one goal (more efficient than individual pathfinding)
- [ ] **Territory/Ownership Costs** - agent-specific movement costs (RimWorld-style: colonists avoid others' bedrooms, visitors stick to public areas)
- [ ] **Formation System** - units moving in formation (wedge, line, box), leader-follower patterns
- [ ] **Spatial Queries Layer** - unified interface for "find nearest X", k-nearest neighbors, range queries
- [ ] **Terrain Cost / Influence Maps** - threat/danger maps for tactical AI, heat maps showing congestion
- [ ] **Multi-layer Navigation** - bridges, tunnels, teleporters, jump pads

### Optimizations
- [ ] **View frustum culling for movers** - only draw movers visible on screen
- [ ] **Spatial grid prefix sums for mover avoidance** - O(n) bucket sort for neighbor queries
- [ ] **Periodic mover sorting** - reorder mover array by cell every N frames for cache locality
- [ ] **Y-band sorting for movers** - hybrid bucket+insertion sort for 2.5D depth sorting

---

## Completed (Feb 2026)

See [/docs/done/](../done/) for detailed completion notes.

- [x] **Jobs/Hauling system** - Items, stockpiles, filters, stacking, priority, gather zones
- [x] **Stockpile/gather zone UI** - S+drag for stockpiles, G+drag for gather zones
- [x] **Mining system** - M+drag designations, channeling, remove floor/ramp
- [x] **Construction system** - B+drag blueprints, material delivery, buildable ladders/floors
- [x] **Architecture refactor** - Job pool, Job Drivers, WorkGivers, hybrid AssignJobs
- [x] **Basic crafting** - Workshops (stonecutter), recipes, bills, auto-suspend
- [x] **Trees & wood** - Saplings, growth, chopping designation, ITEM_WOOD
- [x] **Blocked movers handling** - Goal reassignment, job cancellation, messages
- [x] **Data-driven items** - item_defs.c with ItemDef structs

---

## Feature Details

### Social/Crowd Features
**Phase 1 - Queuing (foundation)**
- [ ] Mark cells as queue points (ladders, doorways, narrow passages)
- [ ] Track waiting movers per queue point (first-come-first-served)
- [ ] Movers wait at nearby positions instead of crowding
- [ ] Signal next mover when queue point clears

**Phase 2 - Full Social Navigation (requires queuing)**
- [ ] **Yielding**: Agents give way to each other
- [ ] **Lane Preference**: Keep-right/left for counter-flow
- [ ] **Personal Space**: Context-dependent spacing

---

## See Also (Design Docs)

- [jobs-checklist.md](jobs-checklist.md) - Remaining jobs work
- [jobs-roadmap.md](jobs-roadmap.md) - Jobs system full design
- [needs-vs-jobs.md](needs-vs-jobs.md) - Individual needs vs colony jobs (future)
- [elevators.md](elevators.md) - Full elevator simulation design
- [decoupled-simulation-plan.md](decoupled-simulation-plan.md) - Headless sim + 2.5D views
- [logistics-influences.md](logistics-influences.md) - Factorio/SimTower research
