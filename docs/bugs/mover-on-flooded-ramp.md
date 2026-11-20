# Mover stuck on a flooded ramp

**Status: OPEN** (found 2026-11-20, not investigated)

## Description

In `--scenario mood`, after 3000 ticks one mover stands on a ramp cell (36,19,z1) that
deep water (level 7/7) has flooded. The flooded cell counts as not walkable, so every
re-plan from it fails; the mover keeps re-planning (path length 0, repath cooldown)
and never leaves. Most likely the water flowed in while it stood there.

## Reproduce

```bash
./build/bin/path --headless --scenario mood --ticks 3000 --save /tmp/mood.bin
./build/bin/path --inspect /tmp/mood.bin --mover 7      # position (36,19,z1), needs repath
./build/bin/path --inspect /tmp/mood.bin --cell 36,19,1  # RAMP_S, water 7/7
```

## Notes

- `tools/scenarios_check.sh` allows 1 mover in a non-walkable cell for `mood` because
  of this; remove that allowance once fixed.
- Likely directions: movers in rising water should step to the nearest dry walkable
  cell (or path from the cell they stand on even if it is flooded), or deep water
  shouldn't flow onto cells occupied by movers.
