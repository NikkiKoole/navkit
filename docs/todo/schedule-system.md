# Schedule System

> Status: partial (soft day rhythm done 2026-11-22; clock blocks below are still a spec)

> Clock-driven daily routines layered on top of the existing need/job system. The shift from "do whatever's urgent" to "it's 8am, time for work."

## Current System

Pure need/queue driven. `AssignJobs()` scans all idle movers, WorkGivers find best (mover, task) pairs, mover works until done or need interrupts. No concept of time-of-day — a mover will mine at 3am if there's mining to do.

## Done: Soft Day Rhythm (2026-11-22)

Design call: the rhythm must be soft but real, and night must **not** make anyone more
tired (no clock-driven energy drain). It comes from world facts instead. Setting
`sleepRhythm` (on by default), code in `src/simulation/needs.c` (see `needs.h`):

- **Lying in**: a mover in bed doesn't get up while it is dark (seasonal dawn/dusk),
  unless a need is critical (starving, dehydrating, desperate bladder, freezing).
- **Early night**: idle (no job, no need) for `idleBedDelayGH` (0.25) in the dark with
  energy below `idleBedEnergy` (0.5): go to bed.
- **Sleep metabolism**: hunger and thirst drain at `sleepMetabolismMult` (0.5) while
  resting, so a meal lasts the night. Without it hunger woke sleepers most nights and
  the rhythm drifted.
- When a need ends (ate, drank, woke), the other needs are weighed again in the same
  tick, before AssignJobs: a tired mover who just ate goes to bed instead of back to work.

Lying in can only make waking later, never earlier, so the anchor is dawn: a working
mover wakes at dawn, works ~14 game hours until exhausted, sleeps ~7, and is still in bed
when it is dark. Measured with scenario `dayrhythm` (8 movers, endless mining, dried
berries, dayLength 120, 14 days; movers in bed at 03:00 / 13:00 per day):

| | in bed at 03:00 | in bed at 13:00 | walls mined in 14 days |
|---|---|---|---|
| rhythm off, sleepMetabolism 1 | 0-5 of 8, drifting | 0-6 of 8 | 395 (food ran out day 13) |
| rhythm on | 5-8 of 8 | 0-2 of 8 | 490 |

Not done / next: going back to bed after waking at night for a drink; light at the
mover's cell (a lit room) instead of the sky; night owl / early bird traits as an offset
on lying in; City 1999 needs opening hours and spawn curves, not per-mover blocks.

## Village System: Schedule-Driven with Need Interrupts

Each mover has a daily schedule divided into time blocks. Every tick, the mover checks what block they're in and finds an appropriate activity. Critical needs (starvation, exhaustion) still interrupt regardless of block.

### Default Day

```
 6:00 - 7:00   Wake + morning meal
 7:00 - 12:00  Work block 1
12:00 - 13:00  Midday meal
13:00 - 17:00  Work block 2
17:00 - 18:00  Evening meal
18:00 - 21:00  Leisure
21:00 - 6:00   Sleep
```

Not hardcoded — a `Schedule` struct on each mover (or shared template). Different occupations can have different hours. Night guard works 21:00-6:00, sleeps during the day.

### How It Layers onto Existing AssignJobs

The WorkGiver pattern doesn't die. It gets a filter:

```c
ScheduleBlock block = GetCurrentBlock(mover, gameHour);

if (block == BLOCK_WORK) {
    // existing WorkGiver logic, but prefer mover's assigned occupation
    // farmer → prioritize farm WorkGivers
    // unassigned → current behavior (grab whatever)
}
else if (block == BLOCK_MEAL) {
    // find food, find seat at table, eat
    // no table → eat standing (mood penalty)
}
else if (block == BLOCK_LEISURE) {
    // pick leisure activity based on sub-needs
}
else if (block == BLOCK_SLEEP) {
    // go to assigned bed
}
```

Need interrupts stay exactly as-is. Starving overrides schedule.

## Occupations

Soft role preferences, not hard gates.

```c
typedef struct {
    const char* name;          // "Baker", "Farmer", "Builder"
    JobType preferred[8];      // prioritized job types
    int workplaceId;           // assigned workshop/farm/zone (-1 = any)
} Occupation;
```

During work blocks, a mover with an occupation checks preferred WorkGivers first. If nothing available in their specialty, they fall back to general labor (hauling, cleaning). No one stands idle because "that's not my job" — the baker hauls when there's no baking. But the baker gravitates toward baking, builds skill, gets faster, feels satisfaction.

## Leisure Activities

New WorkGiver category, only assigned during leisure blocks:

- **SitOnBench** — find bench, sit, +social if others nearby
- **VisitPub** — find pub, go there, drink, +fun +social
- **Wander** — pick random interesting spot, walk there
- **Socialize** — find friend, walk to them, talk
- **Rest** — find comfortable spot, sit/lie down

Each satisfies different sub-needs. High social need → prefers pub or socializing. High boredom → prefers wandering or something novel.

## Fun / Boredom

```c
float fun;                         // 0.0 (bored) to 1.0 (entertained)
float funDecayRate;                // drains during work, slower if varied
ActivityType recentActivities[8];  // ring buffer for diminishing returns
```

Fun decays during work (faster for repetitive, slower for varied). Leisure restores it. Same activity repeatedly → diminishing returns via `recentActivities` buffer. Pub three nights running? Less fun. Go sit in the park instead.

Creates emergent routines with variation — movers develop habits but break them when bored.

## Meals as Social Moments

Meals aren't just "eat food." They're:

1. Find food source (kitchen, communal hall, personal stockpile)
2. Find seat (table preferred, bench ok, ground = mood penalty)
3. Eat (duration based on food type)
4. Others eating nearby → social bonus

Communal dining hall = natural social mixing. Households eating together = family bonds. Eating alone on the ground = sad. Makes building a proper kitchen + dining room meaningful.

## Incremental Implementation

1. **Sleep/wake only** — movers go to bed at night, wake at dawn. Already changes the feel dramatically
2. **Work blocks** — movers only take jobs during work hours. Idle at night
3. **Meal blocks** — structured eating times, seat-finding
4. **Occupations** — soft role preferences in work blocks
5. **Leisure** — new WorkGiver category, fun/boredom need
6. **Social** — relationship tracking, conversation, group activities

Each step makes the village feel more alive. Step 1 is maybe 30 lines.

## Architectural Cost

- `Schedule` on Mover (~20 bytes: 6 time block entries)
- `Occupation` index on Mover (~4 bytes)
- Schedule check in AssignJobs (~15 lines: switch on current block)
- Leisure WorkGivers (~5-8 new, ~30-50 lines each)
- Fun need (~20 lines: decay + restore + diminishing returns)
- Meal behavior upgrade (~40 lines: seat-finding, social bonus)

Biggest change is the mindset shift in AssignJobs — from "find best task globally" to "find best task appropriate for this time block."
