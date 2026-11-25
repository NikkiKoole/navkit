# Food off the cell centre can be out of reach

Status: open (found 2026-11-25 while building the `play-hungry` scenario; not fixed)

A mover seeking food walks to the food's **cell** (`m->goal`), but `FREETIME_SEEKING_FOOD`
checks arrival against the **item position** with `pickupR = 0.75 cells` (needs.c). Walking
stops once the mover is in the goal cell and near its centre (about 0.5 cells), so an item lying
near one edge of its cell can be ~0.9 cells away from a mover that came from the other side.
The path counts as done (`PointsEqual(goal, current)`: no repath), the item is out of reach:
the mover stands there until the food search times out (`foodSeekTimeoutGH`).

Repro: `play-hungry` with the berries at `(cell * CELL_SIZE) + 4` (as `dayrhythm` places them)
instead of the cell centre: movers arriving from the east stand at x = 5.99 for the item at
x = 5.125 (cells), hunger runs down to 0.05 instead of 0.21.

Likely the same for drinks and other `needTarget` items. Fix options: walk to the item position
(not just the cell) on the last leg, or test arrival as "in the item's cell" like the path does.
