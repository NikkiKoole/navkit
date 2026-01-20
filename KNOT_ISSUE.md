# Mover Knot Issue

## Problem
When many movers converge on a chokepoint or shared waypoint, they can form "knots" - clusters of movers stuck orbiting a waypoint without ever reaching it. Avoidance pushes them away from each other, but they all need to hit the same waypoint center.

## Cause
Current waypoint arrival check requires movers to get within `m->speed * dt` (~1.6 pixels) of the waypoint center. When avoidance is active, movers can orbit the waypoint indefinitely as the avoidance force keeps pushing them away from neighbors who are also trying to reach the same point.

## Potential Solutions

1. **Larger waypoint threshold** - increase arrival distance to half a cell (16 pixels) so movers don't need exact center hits

2. **Reduce avoidance near waypoints** - when very close to a waypoint, prioritize reaching it over avoiding neighbors

3. **Pass-through detection** - instead of distance check, detect when mover has passed the waypoint relative to approach direction

## Status
To be addressed later. The directional avoidance system is working well otherwise.


so we tried the larger waypoint treshold, the fis  visible clue what happend , agents no jump a few pixels, whn reachin the waypoint. oesnt look attractive, might be easy to fix.
we dont know yet
