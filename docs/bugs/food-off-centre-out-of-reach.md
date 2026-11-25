# Food off the cell centre can be out of reach

**Status: FIXED** (found and fixed 2026-11-25)

A mover seeking food walks to the food's **cell** (`m->goal`), but the arrival test was a
radius around the **item position** (0.75 cells). Walking ends as soon as the mover is in the
goal cell, so an item near one edge of its cell can be ~0.9 cells from a mover that came in
from the other side. The path counts as done (no repath), the item is out of reach: the mover
stood there until the food search timed out (`foodSeekTimeoutGH`) and tried again.

## A class of bugs

Every "close enough to the thing?" test written as a bare radius had it. Found and fixed:

- needs.c: food (`FREETIME_SEEKING_FOOD`), drink (`FREETIME_SEEKING_WATER`), the trash bin
  (`FREETIME_SEEKING_BIN`)
- action_queue.c: a player's "eat that" (the walk counted "in the cell" as arrived, the pick-up
  wanted the radius: the command failed)
- already patched locally with "or in its cell": throwing.c pick-up, fetching a set-aside item

Jobs solved it long ago another way: `TryFinalApproach` (jobs.c) steps the mover closer when
its path is done but the item is not yet in reach.

## Fix

One rule, `MoverCanReach(m, x, y, z)` in mover.c: same level, and in the thing's cell or within
`MOVER_REACH` (0.75 cells). All the above call it. **New code that asks "can it reach that?"
should call it**, not write its own radius.

Tests: test_hunger "food at the far edge of its cell ...", test_action_queue "food lying at the
far edge of its cell ...". Scenario `play-hungry` puts its berries 4 px into their cells again
and checks the lowest hunger stays above 0.1: before the fix it was 0.05, after 0.20 (0.21 with
the berries at the cell centres).
