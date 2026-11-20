# Service points and queues

> Status: partial (step 1 done: ladders)

One general way for movers to wait their turn at things that serve one (or a few)
at a time: ladder segments now; train doors, wells, workshops, doors, elevators
later. Membership is explicit and small; the queue's shape and behaviour emerge
from local rules.

## Why

Three ad-hoc versions exist or were tried:

- **Ladders** (`mover.c`, single-file): movers wait ON the ladder cell, so a queue
  is a pile on one spot (48 movers on one cell with 96 movers). Holding them a cell
  short (tried, dropped) spread the pile but kept no order and slowed hand-offs.
- **Train stations** (`trains.c`): first-come-first-served list, fixed slots in a
  straight line along the platform, movers glide into their slot. Neat, but only
  works where the line has an obvious straight direction.
- **Elevators** (`docs/todo/world/elevators.md`): planned, needs the same thing.

## Model

**Explicit (small, general):** a service point is a place that serves `capacity`
movers at a time.

| Field | Meaning |
|-------|---------|
| key | what it is: ladder segment (x, y, zLow), station, ... |
| capacity | 1 for a ladder segment; free seats for a train door |
| line | movers in arrival order (with the end they wait at: a ladder has two) |
| serving | movers being served right now (climbing) |
| avgServiceTime | measured (moving average), not guessed |

From that, everything else asks the point:

- **Who's next:** the front of the line (strict first-come-first-served; with two
  ends, the earliest arrival of the two fronts).
- **Expected wait:** `(line ahead + serving) x avgServiceTime`: route cost (the
  ladder queue cost becomes this), patience decisions, stats.

**Emergent (no slots, no snapping):** each waiting mover only looks at the one
directly ahead of it in the line.

- The front walks to the service spot (the ladder foot) and waits there.
- Everyone else stands a little behind the mover ahead of them, coming from
  wherever they came from: plain steering + mover avoidance + wall sliding. The
  line bends around walls and spreads where there's room, because every mover
  follows a real mover that walked there.

## Steps

Each step is measured with headless scenarios (`--headless --scenario ladder-choice
--set scenarioMovers N`): throughput, average and worst wait, give-ups, and the
most movers standing on one cell (no piles).

1. **Service points + follow-the-one-ahead for ladders.** Replaces waiting on the
   ladder cell. Joining: a mover whose route climbs a ladder joins its line when
   within a few cells of it. Leaving: when it starts climbing, re-plans away,
   deactivates, or times out. Not saved: lines rebuild themselves after a load.
   *Done* (`src/entities/service.c`, ladder rules in `mover.c`). Lessons: the
   front of each end may start only when it is next in arrival order among the
   fronts actually standing at their foot (ties by line order: equal join times
   deadlocked both ends); the "climb anyway" timeout must measure a stalled
   climb, not time in line (long fair lines tripped it); a follower within the
   arrival radius must stay put, not snap onto the foot. Measured (ladder-choice,
   6000 ticks): no shared segments at 16/48/96 movers; waiting movers no longer
   stack on the foot (only crowd pressure pushes one in at 96 movers).
2. **Measured expected wait as route cost** (replaces "10 per mover").
3. **Train stations on the same system:** the slot queue becomes follow-the-one-
   ahead; boarding uses the line order; a station's expected wait can feed route
   choice (and later replace the "goal 40+ cells away" rule in `ShouldUseTrain`).
4. **Patience trait:** wait, give up and re-plan, or leave and come back.
5. **More service points** as the game needs them (well, workshop, doors,
   elevators).

Later, if wanted, each a small local rule: queue-jumping by rude movers, chatting
in line (mood), avoiding a queue you can see is long.

## Open questions

- A ladder's two ends share one segment: alternate directions, or strict arrival
  order across both ends? (Step 1: strict arrival order.)
- How far from the point does a mover join? Too early and it queues while still
  walking; too late and it barges into the line. (Step 1: ~3 cells.)
