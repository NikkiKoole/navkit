# Temperature activity counter drift in the GUI

**Status: OPEN** (seen 2026-11-21, not investigated)

## Description

A GUI session started with `--game dollhouse` logged, several times:

```
Activity counter drift: tempUnstableCells = 0, actual = 16384 (correcting)
```

The temperature sim's count of unstable cells was 0 while every cell was unstable.
The audit corrects the count each time, so the cost is skipped temperature updates
until then, not a crash. 16384 = 32x32x16: the size of the sandbox/startup flat map,
not the 16x16 dollhouse map, so it probably follows switching to Sandbox (or another
reset of the startup map) inside the window.

## Reproduce

Not reproduced yet. Headless runs of survival, dollhouse, sandbox and the mood scenario
show no drift. Unknown which buttons were used in the GUI session.

## Notes

- Places that reset the world: `StartGame` (src/core/games.c), `ScenarioResetWorld`,
  the dev UI Generate Terrain button, grid-size buttons. Check which of them rebuilds
  temperature (`InitTemperature`) without rebuilding the sim activity counts
  (`RebuildSimActivityCounts`), or does it in the wrong order.
- Per-cell layers moved to a per-map-size arena on 2026-11-21 (7a2a281, e37f632);
  worth ruling out a layer that is reallocated after the counts are rebuilt.
