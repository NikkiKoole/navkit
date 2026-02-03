# Moving-to-Item Stuckness

**Status: FIXED**

## Description

Sometimes movers get stuck in a moving-to-item state. The object they want can be right next to them.

## Cause

Knot-fix skips to waypoints without snapping position, leaving mover close but out of pickup range.

## Fix History

1. **Attempted:** Increasing `KNOT_FIX_ARRIVAL_RADIUS` to match `PICKUP_RADIUS` - too simple
2. **Attempted:** Added "final approach" repath when path exhausted but within 2 cells - didn't work because knot-fix would skip again
3. **Fixed:** Direct micro-movement when path exhausted and mover is in same/adjacent cell as item but out of pickup range - moves directly toward item each tick (bypassing pathfinding)
4. **Extended (2026-02-02):** Added same final approach logic to `RunJob_Craft` for `CRAFT_STEP_MOVING_TO_INPUT` and `CRAFT_STEP_MOVING_TO_WORKSHOP` steps

## Debug Info

Screenshot: `/Users/nikkikoole/Projects/navkit/documentation/bugs screenshot/outofrange.png`

Disabling knot-fix made the mover work, confirming the cause.
