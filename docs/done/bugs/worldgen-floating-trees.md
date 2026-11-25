# World generation plants some trees one cell too high

**Status: FIXED** (found and fixed 2026-11-20)

## Description

`--scenario world` (HillsSoilsWater) can place a tree with an air gap under its trunk:
ground ends at z5, z6 is air, trunk starts at z7. Seed 6 at 128x128 has 10 such trunk
cells right after generation (tick 1), so it's the generator, not tree growth.

## Reproduce

```bash
./build/bin/path --headless --seed 6 --scenario world --set scenarioSize 128 --ticks 1 --save /tmp/w6.bin
./build/bin/path --inspect /tmp/w6.bin --cell 85,42,6   # air
./build/bin/path --inspect /tmp/w6.bin --cell 85,42,7   # tree trunk
```
(other columns: (83,51) trunk at z7, (38,92) trunk at z8)

## Cause and fix

Cave generation runs after trees, bushes, plants and loose items are placed. The
drunkard's walk can wander out of the hill along the surface level, and its 3x3 rooms
carved any wall cell with a wall below: the top layer of the ground, under whatever
stood on it. 4 of 8 seeds at 128x128 had floating trees (up to 10), 2 had floating bushes.

Fix (`terrain.c`, `CaveCanCarve`): a cave cell must have a wall below *and above*, so
caves keep a roof and never open the surface. All 8 seeds: 0 floating, still 6/6 caves
(smaller only where they used to break through the surface).
Test: `caves_keep_surface_support` in `tests/test_terrain.c`.
