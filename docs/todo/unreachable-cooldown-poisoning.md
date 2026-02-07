# Bug: Unreachable Cooldown Poisoning by Stranded Movers

## Problem

When movers are stranded on a disconnected z-level (e.g., z=3 with no ramps/ladders down), they repeatedly poison reachable items with unreachable cooldowns, causing all other movers to ignore those items for 5 seconds at a time.

## How it works

1. Movers end up at z=3 (e.g., after terrain changes, tree felling exposing upper levels, or channeling)
2. They're idle — no way down, no jobs available at their level
3. `AssignJobs` runs `WorkGiver_Haul` for an idle z=3 mover
4. WorkGiver finds a ground item at z=1 (granite, clay, etc.)
5. `FindPath(z3_mover, z1_item)` fails — no path between z-levels
6. `SetItemUnreachableCooldown(item, 5.0f)` marks the item unreachable **globally**
7. ALL movers now skip this item for 5 seconds (including z=1 movers who CAN reach it)
8. Cooldown expires, z=3 mover tries again first (still idle), re-poisons the item
9. Repeat forever — items get hauled very slowly when z=1 movers occasionally win the race

## Observed behavior

Save file: `saves/2026-02-07_08-20-40.bin.gz`

- 20 movers, 4 stranded at z=3 (movers 7, 8, 15, 17)
- 16 granite/clay items on walkable z=1 cells — all paths valid from z=1 movers
- After 500 ticks (~8s): all 16 items marked `[UNREACHABLE 1.7s]`
- After 5000 ticks (~83s): some hauled, many still cycling through cooldowns
- After 30000 ticks (~8 min): all 16 eventually hauled, but took far too long

Additionally, items spawned by FellTree at z=2 (on top of neighboring tree leaves) are permanently unreachable by any mover, but the z=3 movers keep trying and resetting their cooldowns too.

## Root cause

The unreachable cooldown is **per-item**, not **per-mover**. One mover's failure to path blocks all movers from trying that item.

## Possible fixes

### Option 1: Per-mover unreachable tracking (best but most complex)
Instead of a global cooldown on the item, track which movers have failed to reach it. Other movers at different positions/z-levels can still try.

### Option 2: Only attempt items on same z-level
In `WorkGiver_Haul`, skip items that aren't on the mover's current z-level (or a z-level reachable via known ramps/ladders). Quick filter before the expensive `FindPath` call.

### Option 3: Don't set unreachable cooldown if mover is on disconnected z-level
Before setting the cooldown, check if the mover's z-level has any connections to the item's z-level via the HPA graph. If not, the failure is mover-specific, not item-specific — don't poison the item.

### Option 4: Shorter cooldown with mover-count scaling
Reduce cooldown when many movers are idle, so z=1 movers get more chances to win the race. Simple but doesn't fix the root cause.

## Recommendation

**Option 2** is the simplest and most effective fix. A z-level check in the WorkGiver is cheap and eliminates the entire class of cross-z-level poisoning. Option 3 is a good complement for edge cases where items are unreachable within the same z-level.

## Related issues

- Movers getting stranded at z=3 in the first place is a separate problem (likely from tree felling or terrain changes that don't push movers to valid locations)
- FellTree spawning items on non-walkable cells (z=2 on top of leaves, z=1 on air-above-air) — see audit Finding 9 in `docs/todo/audits/designations.md`
- Items spawned at z=2 are permanently unreachable but keep cycling through cooldowns (wasted work)

## Files involved

- `src/entities/jobs.c`: `WorkGiver_Haul` (line ~2915), `SetItemUnreachableCooldown` calls
- `src/entities/items.c`: `SetItemUnreachableCooldown` (line ~260)
- `src/entities/jobs.h`: `UNREACHABLE_COOLDOWN` = 5.0f
