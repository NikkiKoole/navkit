# Smoke activity counter drift

**Status: OPEN** (found 2026-11-25, not investigated)

`smokeActiveCells` (the count of cells with smoke, smoke.c) ends far above the real number in
every run with smoke; `ValidateSimActivityCounts` corrects it. The windowed loop checks every
60 s (the "Activity counter drift" warnings); headless runs now check once at the end of the
run (main.c, 2026-11-25), so it shows in plain headless runs:

```
./build/bin/path --headless --scenario campfire --ticks 4000
WARNING: Activity counter drift: smokeActiveCells = 15849, actual = 634 (correcting)
./build/bin/path --headless --scenario island-winter --ticks 4000
WARNING: Activity counter drift: smokeActiveCells = 15761, actual = 2893 (correcting)
```

Explosions (`explosions`, `collapse`, `blasting`) show it too, smaller (60-900 counted, 0 real),
but campfire and island-winter have no explosions: it's in the smoke simulation itself.
`SetSmokeLevel` keeps the count; the other `smokeActiveCells++` sites in smoke.c (spread, rise,
generation) are the place to look: some path increments without a matching decrement (or
writes `level` without going through `SetSmokeLevel`).

Cost: while the count is high, the smoke update doesn't take its "nothing to do" exit.
