# Pathfinding — Future Ideas

> Status: idea

Small ideas not yet fleshed out. Each could become its own doc when implementation starts.

---

## Flow Fields
For many agents sharing one goal (more efficient than individual pathfinding).

## Formation System
Units moving in formation (wedge, line, box), leader-follower patterns.

## Multi-layer Navigation
Bridges, tunnels, teleporters, jump pads.

## Terrain Cost / Influence Maps
Threat/danger maps for tactical AI, heat maps showing congestion.

## Territory / Ownership Costs
Agent-specific movement costs. Examples:
- RimWorld-style: colonists avoid others' bedrooms
- Visitors stick to public areas

## Variable Movement Costs
Road (cheap), rubble/mud (expensive) terrain costs.
Note: disables JPS/JPS+ (those assume uniform cost). Need fallback to standard A*.

## Detour-aware long searches (parked 2026-11-21)
**Not needed now; build it when real games show stalls on detours.** On the 2048 hills map,
3% of path searches take 65% of path time: routes of ~200 cells that detour around lakes or
cliffs. The straight-line heuristic pulls the search the wrong way and it floods (20K-240K
abstract nodes, 30-140 ms). They are detours, not long trips: trips under ~128 cells cost
about 1 ms or less, and colonists mostly travel under ~100 cells. Measured with 3000
wanderers (`--exec "spawnmovers 3000"`); details in
`architecture/chunked-world-plan.md` ("Slow path requests with 3000 movers at 2048").
Options, cheapest first:
1. **Landmarks (ALT heuristic)**: exact distances from ~16 fixed entrances to every
   entrance (~16 MB at 2048, 16 Dijkstras at build, threadable). Admissible, so paths stay
   optimal; most effective exactly for detours. After edits that open shortcuts it can
   overestimate (slightly longer paths) until refreshed.
2. **Coarser HPA* level** (clusters of e.g. 4x4 chunks): the bigger change; paths a few
   percent longer, every terrain edit also updates the upper level.
Related open item: water never tells the pathfinder when a cell gets too deep to walk, so
regions ignore water and the first search to a goal across a lake floods the graph (later
ones answer from the remembered reach set). Notifying when a cell crosses the walking depth
would fix it; water moves through many code paths and flickers near the threshold.
