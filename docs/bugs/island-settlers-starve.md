# Island settlers starve by day three

**Status: OPEN** (found 2026-11-25 while measuring the social need, not investigated)

All three settlers of the `island` game die of starvation on day 3, in every run tried. It is
not the social need (same with `socialNeed` on and off) and not the games (they don't start:
nobody idles near a friend).

## Repro

```bash
./build/bin/path --headless --game island --seed 3 --ticks 64800 --events ev.log
grep -iE "died|ate item" ev.log
```

| seed | deaths (event log) | meals in 3 days (3 settlers) | drinks |
|---|---|---|---|
| 3 | D3 10:18, D3 19:45, D3 19:47, all starvation | 5 | 46 |
| 1 | D3 00:34, D3 09:53, D3 10:03, all starvation | 2 | 37 |

They drink plenty (the river) but hardly eat: every meal is raw food standing up ("Ate raw
food", "Ate without table"). So food is found rarely or not at all. Places to look first: what
food the island has near the landing (berry bushes, forage), whether the settlers' jobs leave
them time to look for it, and whether food searches fail (unreachable, timeouts).

## Also seen

The 2,000-mover load test on the island (`--game island --exec "spawnmovers 2000 add"`, one
game day, `--profile`) ended with every mover dead (mostly starvation, some thirst), so that load
test no longer measures a busy colony over a whole day. The action-queue doc's measurement
(`../todo/architecture/action-queue-and-need-index.md`, ~940 movers) was taken when they lived.
Unknown whether the island used to keep its settlers alive (worth checking an older commit
before digging into the food code).
