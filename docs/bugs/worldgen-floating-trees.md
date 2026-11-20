# World generation plants some trees one cell too high

**Status: OPEN** (found 2026-11-20, not investigated)

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

## Notes

- Likely the tree placement uses a surface height that is off by one in some spots
  (e.g. after ramps/rivers/caves change the terrain, or the walkable cell above a ramp).
