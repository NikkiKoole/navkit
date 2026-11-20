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

Also in survival (found 2026-11-21 while chasing a flaky `make scenarios-check`; survival
used a random seed then). The settler ends on RAMP_N (54,34,z5), water 7/7, while
SEEKING_DRINK, so the path to water probably leads it onto the flooded ramp:

```bash
./build/bin/path --headless --game survival --seed 1655500174065297303 --ticks 3000 --save /tmp/surv.bin
./build/bin/path --inspect /tmp/surv.bin --mover 0        # cell (54,34,z5), needs repath
./build/bin/path --inspect /tmp/surv.bin --cell 54,34,5   # RAMP_N, water 7/7
```

1 of 40 random large seeds hits it (seeds 1-40 do not). scenarios-check runs survival on
the fixed headless seed 0, which does not hit it.

## Notes

- `tools/scenarios_check.sh` allows 1 mover in a non-walkable cell for `mood` because
  of this; remove that allowance once fixed.
- Likely directions: movers in rising water should step to the nearest dry walkable
  cell (or path from the cell they stand on even if it is flooded), or deep water
  shouldn't flow onto cells occupied by movers.
