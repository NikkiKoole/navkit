# Mover Knot Issue

## Status: RESOLVED

The knot issue has been fixed using a combination of Option 2 + Option 3.

## Problem (Historical)

When many movers converge on a chokepoint or shared waypoint, they formed "knots" - clusters of movers stuck orbiting a waypoint without ever reaching it. Avoidance pushed them away from each other, but they all needed to hit the same waypoint center.

## Solution Implemented

### Knot Fix (toggleable via `useKnotFix`)

1. **Larger arrival radius without snap (Option 2)**: Movers advance to next waypoint when within 16px instead of ~1.67px, without snapping position. This lets them smoothly redirect.

2. **Reduced avoidance near waypoints (Option 3)**: Within 32px of the current waypoint, avoidance strength fades out quadratically. At 16px it's 25% strength, at waypoint it's 0%. This lets movers push through to reach waypoints.

### Related Fixes

- **Wall Repulsion**: Pushes movers away from walls before collision
- **Wall Sliding**: Slides along walls instead of penetrating
- **Stuck Detection**: Tracks movers not making progress, triggers repath after 2s

## Code Locations

- `mover.h`: `KNOT_FIX_ARRIVAL_RADIUS`, `useKnotFix`
- `mover.c`: Waypoint arrival check (~line 630), avoidance scaling (~line 670)
- `demo.c`: UI toggles in Movers > Walls section

## Visualization

Enable "Show Knots" in demo UI to see mover waypoint proximity:
- Green: moving normally
- Yellow: near waypoint, still progressing
- Orange: getting stuck (>0.75s)
- Red: stuck (>1.5s)
