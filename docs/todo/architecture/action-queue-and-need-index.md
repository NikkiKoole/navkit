# Action queue and need index: player control and needs at scale

> Status: step 1 done, step 2 started: the queue + sit-then-eat built (2026-11-25). Decided with the user after the toolbar talk:
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

### Step 1, first finding (2026-11-25): it was the berry search

Profiled per search (`--profile` now has summed sections per need search: FoodSearch,
Forage, Scrounge, FoodInStockpile, FoodOnGround, DrinkSearch, RestSearch, BladderSearch,
WarmthSearch, StaleReservations, FreetimeDecide, FreetimeStates). The plain item scans hardly
show; nearly all of it was **foraging**: hungry movers with no food flood-search ~48 cells
for a free ripe bush, and when none is free (all taken, none ripe) every search walks its
whole budget, again after every cooldown. Fixes: count free ripe bushes once per tick and
skip the flood at zero; who forages which bush is a claim table built once per tick (was:
every bush check went through every mover). Same island run, built side by side on one
commit (behaviour identical: 26984 arrivals both):

| | before | after |
|---|---|---|
| Freetime avg | 1.80 ms | 0.55 ms |
| Forage | 1.30 ms | 0.02 ms |
| Freetime ticks over 5 ms | 6943 | 1202 |
| whole run | 184 s | 122 s |

Then the ongoing states (timed per state): nearly all of it was **resting**: a starving
sleeper checks every cooldown whether there is food to wake for, and that check called the
berry flood directly (past the new gate). The gate now sits in the berry search itself:
resting 21 s -> 0.2 s over the run. Same run after both fixes:

| | start | now |
|---|---|---|
| Freetime avg | 1.80 ms | **0.11 ms** |
| Freetime ticks over 5 ms | 6943 | **57** |
| arrivals | 26984 | 26984 |

What is left in freetime is deciding (no need active) at ~0.06 ms. The plain item scans
(FindNearestEdibleOnGround / InStockpile, bins) never showed in this run; they are the next
thing to index when a run with many items shows them (a city block with full bins and
shops). So the "need index" of step 1 became two targeted fixes; a general index waits for a
measurement that needs it.

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
   **Built 2026-11-25 (first part):** `src/entities/action_queue.c/.h`: per mover a queue of up
   to 8 steps (walk to an item / furniture / cell, pick up one unit, sit, eat, wait), optional
   steps skipped, a failing required step ends it; freetime runs it in `FREETIME_QUEUE` (not
   idle: no jobs, no talk); saved (v112, after the mover paths). Sit-then-eat: at its food, a
   mover with a free hand and a free seat (CAP_SEATING) within 10 cells takes one unit there
   (`[pick up, walk to seat (opt), sit (opt), eat]`), setting `sitToEat`. In the dollhouse
   every meal of a day was eaten at the chair. Tests: `tests/test_action_queue.c`. Not yet:
   the other 23 freetime states moved onto it (one at a time), interrupt-resume.
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
