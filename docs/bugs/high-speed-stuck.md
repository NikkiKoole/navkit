# Movers Stuck at High Game Speed

**Status: OPEN, partly fixed** (reproduced headless and two causes fixed 2026-11-25)

## Description

At game speed 100, movers get stuck - they appear yellow and not pathing. When game speed is changed, they can find their goal again.

## Reproduce

At speed N every tick moves the sim N ticks' worth (`gameDeltaTime = tickDt * gameSpeed`,
`src/core/time.c:38`; movers use it, `src/entities/mover.c` `UpdateMovers`). Headless runs take
the same path with `--set speed N`:

```bash
./build/bin/path --headless --scenario world --set speed 100 --ticks 3000 --check
for s in 1 2 3 4 5 6; do ./build/bin/path --headless --scenario world --seed $s --set speed 100 --ticks 3000 --check; done
```

Use a scenario: `--game survival` / `--game island` reset the speed to 1 at game start
(`InitTime()` runs after `--set`). Fair comparisons need the same game time: 3000 ticks at
speed 30 = 90000 ticks at speed 1 (25 game days: winter ice forms and thaws).

## Fixed (2026-11-25, `UpdateMovers`, the "mover in a non-walkable cell" branch)

1. **Stepping down onto a ramp fell through into the wall handling.** A mover in open air
   above a ramp is lowered onto it, but the code then ran the "blocked structure" push with
   the old level: it pushed the mover one cell sideways (a pop, at any speed), or removed it
   as "stuck in blocked cell with no escape" when no neighbour at that level was walkable
   (air over a valley). Now the push is skipped after the step down.
2. **A ramp's exit over thawed ice was treated as a platform.** The cell next to a lake that
   a ramp leads up to counted as a ramp exit even with water (no ice any more) below it, so
   the mover neither fell nor was pushed, and was removed. Now an exit counts only on solid
   ground; otherwise the mover falls in and wades out.

`--scenario world`, 3000 ticks, movers removed ("deactivated: stuck in blocked cell"):

| | speed 1 | 10 | 30 | 100 | 100, seeds 1-6 |
|---|---|---|---|---|---|
| before | 0 | 0 | 5 | 14 | - |
| after | 0 | 0 | 0 | 0 | 0 |

`doorway` and `hubs` at speed 100 failed (a mover in a wall) and pass now. `make test` and
`make scenarios-check` (speed 1) unchanged.

## Still open

- **Movers jump many cells per tick.** At speed 100 a walking mover moves ~11 cells in one
  tick (seen: (80.5, 8.5) -> (81.6, 19.9)), so it can end a tick in deep water or past a
  corner. Seeds 1-6 at speed 100 end with 16 movers in non-walkable cells in total (mostly
  water); speed 1 over the same game time ends with 0.
- **Tried and dropped: sub-stepping.** Running the movement N times per tick with dt/N
  (N = speed / 10) cut the movers in walls (16 -> 10 over seeds 1-6) but *raised* removals
  (0 -> 18): movers took ten steps on a route that repaths/LOS only re-check once per tick,
  and cut corners from a ramp onto the level above through solid ground. A working version
  needs the repath + line-of-sight checks inside each sub-step (or several whole sim ticks
  per frame at high speed, which costs CPU in proportion to the speed).
- `tests/test_high_speed.c` (small walled rooms, 10x/100x) passes; a test for the remaining
  case needs hills, ramps or water.
