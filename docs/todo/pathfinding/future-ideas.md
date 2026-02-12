# Pathfinding — Future Ideas

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
