# Action queue and need index: player control and needs at scale

> Status: plan (2026-11-25), step 1 started. Decided with the user after the toolbar talk:
> City 1999 and Dollhouse need a way for the player to tell one person what to do, and the
> design for that (the action queue, `../dollhouse-domestic-life.md` "Future Systems") was never
> built. This doc checks it against the code of today and against thousands of agents.

## Why

- **Player control.** "Steer directly or suggest (Sims style)?" stops being a choice of two
  systems: the player's commands go into the person's queue (click a thing = do that, Shift
  = then also do that), and when the queue is empty the person fills it themselves. How much
  they do on their own is a dial per game: City 1999 low (you're in charge, but a cold person
  still looks for warmth), Dollhouse high (you nudge), Survival: jobs as now, the queue only
  for freetime.
- **The toolbar** follows from it: "act on a thing" = put an action in someone's queue (see
  `../ui/game-structure.md`, toolbars per game).

## What is there today (2026-11-25)

- Freetime is **23 states** in `src/simulation/needs.c` (11 when the queue was designed). Every
  "seeking X" state repeats the same steps (target still there? arrived within reach? timed
  out? mark unreachable); every "doing X" state counts a duration. One decision point: no
  need active -> the needs in a fixed order (starving ... chilly) -> a search.
- Partial interrupt: a critical need unassigns the job and keeps the designation's progress;
  no resume inside freetime.
- A Mover is 368 bytes now (paths live outside the struct; the 12 KB note is old). A queue of 8
  steps adds ~200 bytes: 2 MB for 10,000 movers.

## Measured: what needs cost with many movers

Island, `spawnmovers 2000 add` (~940 active after bad spawn cells), one game day (50400 ticks),
`--profile`:

| section | avg ms/tick | worst tick | ticks over 5 ms |
|---|---|---|---|
| Freetime | **1.6** | **83** | 5751 |
| Move | 0.5 | 76 | 67 |
| Jobs | 0.08 | 26 | 2 |

Everyone starved by the evening (the island can't feed 2000), so this is a harsh test, but it
shows where the cost sits: **not in the state machine, in the searches.** A food search scans
every item in the world (`FindNearestEdibleOnGround`, `FindNearestEdibleInStockpile`), the bed
search every furniture slot, and hungry movers who find nothing search again each time their
cooldown ends: movers x items, all at once when everyone gets hungry together.

## What that means for the design

1. **The queue itself scales**: per tick a mover looks at its current step (walk on, or count
   down): fixed work per mover. It also replaces most of the 23 copies of the same code.
2. **Choosing is what costs**: "where is the nearest food / bed / heat?" must not scan the world.
   - A **need index**: per chunk (or item spatial grid cell), what is there for each need.
     A search walks the buckets nearest first and stops early.
   - **Spread decisions**: plan only when the queue is empty or interrupted; a cap on plans per
     tick, the rest wait a tick.
   - **Remember "nothing here"**: a chunk with no food isn't searched again until something
     changes in it.
3. **Pathfinding stays the biggest cost per step**: one path per walk step, never extra repaths
   because of the queue; check "close enough" before planning another walk.
4. **Movers out of view** (`chunked-world-plan.md` step 7, "the hardest problem"): a queue of
   steps with known durations can be fast-forwarded coarsely ("arrives in 40 s, eats for 30 s"),
   much easier than 23 hand-written states.
5. **Player commands are cheap**: one step into one mover's queue.

## Steps

1. **Need index**, today's freetime searches use it. Measure before / after with the run above
   (same commit, same seed). Started 2026-11-25.
2. **Action queue + freetime on it** (executor + migration; jobs untouched). First new behaviour:
   sit-then-eat `[pick up food, walk to a chair (optional), sit (optional), eat]`. Interrupt and
   resume inside freetime come with it. Save: the queue is saved (Mover migration).
3. **Player commands into the queue**: act on a hovered or selected thing (hover + key or the
   inspector; no right mouse button needed), Shift appends; the queue shown in the character
   panel.
4. **Advertisements + autonomy per game**: objects say which need they serve; the dial per game.
5. Later: coarse fast-forward for movers out of view.

Coordinate with the noticing work (`notice.c`: movers notice each other, talk groups, doorway
yielding): it is behaviour too and belongs in the same queue eventually (step 2).

Related: `../dollhouse-domestic-life.md` (the original design: queue, planner, advertisements),
`../../vision/city-1999.md` (milestone 1 step 2), `../ui-redesign.md` (queue in the character
panel, Shift+click), `chunked-world-plan.md` (movers out of view).
