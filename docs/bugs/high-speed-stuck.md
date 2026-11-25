# Movers Stuck at High Game Speed

**Status: OPEN** (reproduced headless 2026-11-25)

## Description

At game speed 100, movers get stuck - they appear yellow and not pathing. When game speed is changed, they can find their goal again.

## Reproduce (2026-11-25)

At speed N every tick moves the sim N ticks' worth (`gameDeltaTime = tickDt * gameSpeed`,
`src/core/time.c:38`; movers use it, `src/entities/mover.c:1555`), so at 100 a walking mover
covers many cells per tick. Headless runs take the same path with `--set speed N`:

```bash
./build/bin/path --headless --scenario world --set speed 100 --ticks 3000 --check
```

| `--scenario world`, 3000 ticks | speed 1 | 10 | 30 | 100 |
|---|---|---|---|---|
| "Mover N deactivated: stuck in blocked cell with no escape" | 0 | 0 | 5 | 14 |
| movers left in non-walkable cells | 0 | 0 | 0 | 5 (check fails) |

At speed 100 `doorway` and `hubs` also end with 1 mover in a non-walkable cell (check fails);
ladders, hallway, mood, crafting and maze pass. So today's symptom is movers ending up
inside blocked cells (and removed), not only the yellow "not pathing" of the original note.

Note: `--game survival` / `--game island` show the same numbers at speed 1 and 100: the game
start calls `InitTime()`, which sets `gameSpeed = 1` after `--set` ran. Use a scenario.

## Likely directions

- Sub-step movement: split a tick's `gameDeltaTime` into steps of at most ~half a cell per
  mover (or run several sim ticks per frame at high speed instead of one big tick).
- `tests/test_high_speed.c` passes (no clipping through walls at 10x/100x in small walled
  rooms, single and several movers), so the failing case needs more: hills with ramps and
  z-changes, crowd avoidance pushing movers sideways, or repaths. Turn one of the world-scenario
  movers above into a test first (`--save` at the tick before the deactivation, `--inspect --mover`).
