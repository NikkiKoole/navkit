# Joint activities: a few movers doing something together

Status: **spec** (2026-11-25, not started; check with the user before code)

Several movers doing one thing together, each with a part, sometimes in a set order: a chat, a
gathering round the fire, a walk together, tag, hide and seek, follow the leader, the pack hunt;
later a meal together, cooking together, carrying a sofa. Today that is built three times over
in three shapes. Starting games in real play (instead of from scenarios) would make a fourth.
This doc pulls out the shared part first, using the two real cases.

Related: `noticing.md` (what's built), `../architecture/action-queue-and-need-index.md`
(the queue, advertisements step 4), `../dollhouse-domestic-life.md` (advertisements, planner),
`../../vision/city-1999.md` "Free time with meaning".

## What exists

| | talk.c (chat, gathering, walk) | chase.c (tag, hide and seek, follow, pack) | action_queue.c |
|---|---|---|---|
| Group record | `Talk` (512 of them): members, slots, kind, timers | file globals per game: `tagIt`, `hsSeeker`, `flOrder[]`, `packHunters[]`: **one of each in the world** | one queue per mover |
| Roles | speaker / onlooker / focus / leader + follower | it, seeker / hider, leader / place in line, driver / cutter / flanker / runner | none |
| Busy state | none: in a talk the mover stays `FREETIME_NONE`, talk.c holds it (`moverTalkIdx`) | `FREETIME_PLAY` | `FREETIME_QUEUE` |
| Who may join | `CanTalk` (free, no cooldown, not playing, ...) | `PlayerFree` | (commands, needs) |
| Starting | **yes**: `TryStartTalk`, a roll per check scored by affinity (relations), joining a running one | **no**: only scenarios / console | needs, player commands |
| Leaving | `CanStayInTalk` each tick | `PlaySweep`: state no longer `FREETIME_PLAY` | `QueueEnd`, suspend for a need |
| Running it | slots + turns, `TalkMoverTick` | per-tick goals, `ChaseMoverTick`, `HideControls` | steps in order |
| After | cooldown | none | none |
| Saved | no (transient) | no | yes (v112) |

The same five questions are answered in each: who may join, who does what, who leaves and why,
when it ends, what it leaves behind. Two of them also have their own "free?" test, and talk.c
asks chase.c (`ChaseIsPlaying`) and the queue (`QueueSuspended`) by name; every new kind would
add a name to every other kind's test.

## The shape

### A row per kind

```c
typedef struct {
    const char* name;            // "tag"
    const char* label;           // "Playing tag" (describe)
    uint8_t minMembers, maxMembers;
    uint8_t roleCount;           // roles[] below: "it", "runner"
    const char* roles[4];
    ActivityRun run;             // ACT_RUN_STEERED or ACT_RUN_STEPS
    uint8_t busy;                // what it counts as: ACT_BUSY_FREE (a chat: a job or need may
                                 // still pick it) or ACT_BUSY_PLAY (no jobs, no chat, needs end it)
    float minLiking;             // won't do it with someone it likes less than this
    // hooks (NULL = none)
    bool  (*canStart)(int proposer, int* invite, int* n);   // picks who to ask, where
    void  (*start)(int a);       // set up: roles, spots, the first goals
    void  (*tick)(int a, float dt);          // the game logic (per activity)
    bool  (*moverTick)(int a, int k, float dt);   // the movement loop hands it over (true = handled)
    void  (*memberLeft)(int a, int k);       // a new "it" / seeker / leader, or nothing
    void  (*end)(int a);         // effects: mood, liking
    float (*appeal)(int mover, int a);       // for joining a running one and for starting (see below)
} ActivityDef;
```

One table, `activityDefs[]` in a new `src/entities/activity.c`, the way games, settings,
commands and scenarios are tables.

### An instance

```c
typedef struct {
    bool active;
    uint8_t def;                 // activityDefs[] index
    int members[ACT_MAX_MEMBERS]; uint8_t role[ACT_MAX_MEMBERS]; int count;
    float age, timeLeft;
    int x, y, z;                 // where it is (a home, a centre, a place: the fire)
    union { TagState tag; HideState hide; FollowState follow; PackState pack; TalkState talk; } s;
} Activity;
```

The per-kind state moves from chase.c's globals into the union, so **two games of tag can run at
once** in different rooms (today they can't). `moverActivity[i]` (index + 1, 0 = none)
replaces `playing[]` and `moverTalkIdx[]`.

### One lifecycle

1. **Propose**: a free mover's check (staggered, like `TryStartTalk`) asks the defs whose
   `canStart` finds partners nearby, scores them, rolls. Or a scenario / console / player starts
   one directly (as now).
2. **Accept**: each invited mover accepts by its own `appeal` (liking for the others, mood,
   whether a need is close to its threshold) or declines; too few accept: nothing starts (a
   describe line "Mara didn't feel like it" later).
3. **Run**: `tick` per activity; the movement loop hands members over through `moverTick`
   (one hook in the mover loop instead of `TalkMoverTick` + `ChaseMoverTick`).
4. **Leave**: one sweep for every kind: a member whose state was taken over (a need, a job,
   drafting, a command) is dropped; `memberLeft` lets the game carry on.
5. **End**: below `minMembers` or out of time; `end` gives the effects, a cooldown per member.

### One "free?" test

`MoverFree(i)`: active, no job, `FREETIME_NONE`, not drafted, not travelling, not on a ladder,
no suspended queue, not in an activity. `CanTalk`, `PlayerFree` and the idle checks call it;
no module names another.

### Two ways to run

- **Steered** (`ACT_RUN_STEERED`): tag, the pack hunt, walks, follow the leader. Goals every
  tick from the game logic; the queue doesn't fit (a step "run from it" never finishes).
- **Steps** (`ACT_RUN_STEPS`): a meal together (each walks to a chair, sits, waits until
  everyone sits, eats, lingers until the others are done), cooking together, carrying something
  big. Each member gets action-queue steps; the activity adds the joins: a step
  `ACT_WAIT_FOR_GROUP` that finishes when every member got to the same step. The lingering that
  table talk does now by peeking at the others' queues becomes that step.

Talks are steered too (a slot to stand in, turns), but simple enough that they could later be
steps ("go to slot, face the middle, talk until the timer").

## Starting: what makes a mover want to

There is no "fun" or "company" need today. Options, smallest first:

1. **No new need**: the chance per check rises with liking for the ones nearby, good mood,
   time since its last activity, and falls with any need near its threshold (the way
   `talkChance x affinity` works now). Enough to see games start in the dollhouse.
2. **A soft "company" value** per mover, slowly going down when alone and up in activities;
   it scores activities like hunger scores food. This is the advertisement idea
   (`action-queue-and-need-index.md` step 4) applied to people: an activity advertises
   company / fun, a fire advertises warmth and company.
3. Traits: a loner scores company low, a playful one games high (`mood.c` traits).

Start with 1; 2 belongs with the advertisements work and City 1999's free-time scoring.

## Effects

At the end, per member: a moodlet ("Played tag", "Had a nice chat"), and liking up a little
between those who took part (relations.c). Being caught a lot could sting a little (later).
Today a talk leaves only a cooldown and a game leaves nothing.

## Not saved

As now: a load ends every activity and sets its members free (`FREETIME_PLAY` already does
this). Saving them later needs the per-kind state saved; not worth it yet.

## Moving over: steps, each with numbers unchanged

1. `activity.c` with the table, instances, the lifecycle, `MoverFree`, one movement hook,
   one busy state. Games move over: the four games' globals into instances, `ChaseJoin` /
   `PlaySweep` into the shared ones. Check: the game scenarios (`tag`, `hideseek`,
   `follow-leader`, `pack-hunt`, `hunt-cover`, `cover`, `play-hungry`) give the same numbers,
   `test_notice` passes. The chase / flee building blocks stay in chase.c (animals later).
2. Starting games: tag and hide and seek proposed by idle friends (option 1 above), in the
   dollhouse and survival (game rule, like `talkEnabled`). Describe lines for every role.
   Check: a scenario where friends start games by themselves; how often, how long, nobody
   misses a meal.
3. Effects: moodlet + liking at the end.
4. Talks, gatherings and walks move over (talk.c keeps the drawing-side getters).
   Check: `talk`, `gathering`, `walks`, `garden-party`, `mingle`, ... unchanged.
5. The first steps-run one: a meal together (`ACT_WAIT_FOR_GROUP`), replacing table talk's
   peeking.

## Open questions (for the user)

- Should a game ever pull in someone busy with a low-priority job (kids dropping their chores),
  or only the free?
- Can the player start one (select movers, "play tag")? Easy once it is a table row.
- How many games at once is "lively" vs "a playground": a cap per room / per world?
