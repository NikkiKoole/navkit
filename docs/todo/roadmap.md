# NavKit Roadmap

Prioritized improvements and future work.

---

## Short-term (meaningful improvements)

- [ ] **Variable movement costs** - road (cheap), rubble/mud (expensive) - NOTE: disables JPS/JPS+

## Medium-term (new capabilities)

- [ ] **Multi-cell stairs** - horizontal offset while changing z-level (enter at one x,y, exit at different x,y)
- [ ] **Queuing system** - orderly waiting at bottlenecks (ladders, doors, narrow passages). Prerequisite for elevators and other social navigation features.
- [ ] **Refactoring** - dedupe heap implementations, direction arrays, entrance building (~200 lines saved)
- [ ] **Jobs expansion** - See [jobs-checklist.md](jobs-checklist.md) for remaining work
  - [ ] Stockpile config panel (priority, stack size UI)
  - [ ] Requester stockpiles ("keep N items here")
  - [ ] More workshops (carpenter, smelter, forge)
  - [ ] Bill modes (Do X times, Do until X, Do forever)
- [ ] **Containers** - Portable storage (chests, barrels, wagons, bins, bags). Containers are items themselves (can be hauled, stored in stockpiles) but also hold other items inside.

## Larger efforts (build on queuing)

- [ ] **Elevators (full sim)** - moving elevator with state, queuing, capacity, wait times (requires queuing). See [elevators.md](elevators.md)
- [ ] **Social Navigation (full)** - yielding, lane preference, personal space (requires queuing). See [social-navigation.md](social-navigation.md)
- [ ] **Decoupled Simulation** - headless backend + 2.5D view (multi-phase). See [decoupled-simulation-plan.md](decoupled-simulation-plan.md)
- [ ] **Flow Fields** - for many agents sharing one goal (more efficient than individual pathfinding)
- [ ] **Territory/Ownership Costs** - agent-specific movement costs (RimWorld-style: colonists avoid others' bedrooms, visitors stick to public areas)
- [ ] **Formation System** - units moving in formation (wedge, line, box), leader-follower patterns
- [ ] **Spatial Queries Layer** - unified interface for "find nearest X", k-nearest neighbors, range queries
- [ ] **Terrain Cost / Influence Maps** - threat/danger maps for tactical AI, heat maps showing congestion
- [ ] **Multi-layer Navigation** - bridges, tunnels, teleporters, jump pads

## Optimizations

- [ ] **View frustum culling for movers** - only draw movers visible on screen
- [ ] **Spatial grid prefix sums for mover avoidance** - O(n) bucket sort for neighbor queries
- [ ] **Periodic mover sorting** - reorder mover array by cell every N frames for cache locality
- [ ] **Y-band sorting for movers** - hybrid bucket+insertion sort for 2.5D depth sorting

---

## See Also

- [jobs-checklist.md](jobs-checklist.md) - Remaining jobs work
- [jobs-roadmap.md](jobs-roadmap.md) - Jobs system full design
- [../vision/](../vision/) - Future ideas and research
- [../done/](../done/) - Completed features
