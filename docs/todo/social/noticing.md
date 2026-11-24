# Noticing: agents that see each other and do something with it

> Status: spec (2026-11-24). First place: the dollhouse.

One loop for every agent, person or animal: **notice → judge → respond**. Social behaviour
(talking in a circle, walking together, giving way, avoiding someone) and animal behaviour
(hunting, fleeing, hiding, herding) are the same loop with different relations and responses.

Skills are out of scope here (see `../10-personality-and-skills.md`).

---

## Why

- **Movers don't see each other.** `ComputeMoverAvoidance` (`src/entities/mover.c`) is plain boids
  separation: one radius for everyone, everyone pushes the same. Nobody knows *who* is near.
- **Animals are bad** (`src/entities/animals.c`), and for the same reason: each behaviour has its
  own hard-coded check instead of perception.
  - No pathfinding. Grazers walk and flee in straight lines; the simple grazer has no wall check
    at all (slides through walls, into water). Only the predator pushes out of walls, by snapping
    back to the cell centre.
  - Prey freezes while hunted (`ANIMAL_BEING_HUNTED` returns at once).
  - Flee = "move directly away", into corners. The simple grazer only flees its own reserved
    hunter; the steering grazer flees any mover within 3 cells.
  - The predator knows everything: nearest grazer in a radius, every tick, no line of sight,
    no stalking, never tired; touch = kill. Never notices people.
  - Same z only: no ramps, ladders or levels.
  - Three behaviours (simple grazer, steering grazer, predator) with copied code; the steering
    grazer scans *all* animals for predators (O(n²)).

---

## The loop

### 1. Notice (perception)

- Any agent: `AgentRef { kind (mover / animal), index }`.
- Runs a few times a second per agent, staggered over frames, not every tick.
- Looks at agents nearby through the spatial grids (`moverGrid`, an animal grid to add):
  same level (later: one level up/down over open air), within a sense radius, in line of sight.
- **Attention budget**: keeps at most ~4 noticed agents, ranked by salience:
  - known to me (friend, enemy, my owner, my prey) > stranger;
  - doing something unusual: running, coming straight at me, lying down, carrying something big;
  - close.
- Result per agent: a small `noticed[]` list (who, since when, salience). Bounded memory and
  cost, works on big maps.
- Later: things that aren't agents (fire, a noise, food on the floor) as stimuli in the same list.

### 2. Judge (appraisal)

- **Relation** from me to them:
  - people: a sparse per-pair value (like/dislike -1..1, familiarity 0..1), a fixed number of
    known others per mover;
  - animals: by type (predator → prey, prey → predator, pet → owner, herd → herd member);
  - people ↔ animals: by type plus a little per-pair memory (the cat knows who feeds it).
- **Familiarity grows from noticing alone**: seeing the same face every day turns a stranger into
  a familiar one. No explicit social event needed (this is what City 1999 will want too).
- **What they are doing**: idle, working, approaching me, hunting me, in trouble.
- **My state**: busy with a job, idle, on the way to a need, mood, traits.

### 3. Respond

Responses come in tiers by how much they interrupt:

| Tier | Interrupts | Examples |
|---|---|---|
| **Ambient** | nothing, only steering | personal space per pair, drift toward/away, give way, a glance (sprite flip) |
| **Small** | a second or two | pause, nod/wave, walk side by side for a stretch |
| **Decision** | changes the activity | stop to talk, follow, detour, help, watch, flee, hide, hunt |

Most noticing should end in the ambient tier: that is what makes a room feel alive without
everyone stopping all the time. Decisions happen mostly when idle or walking to a need, rarely
during a job. Traits move the thresholds.

---

## Responses as table rows

As with commands, settings and games: one row per response (name, applies when: relation +
their activity + my state, tier, what it does, how much it interrupts). New behaviour is a row,
not a special case in `mover.c` or `animals.c`.

| Response | Social use | Predator / prey use |
|---|---|---|
| approach | drift to a friend, join a talk | stalk (slow, out of sight), chase (fast, tiring) |
| keep distance | personal space, detour around an enemy | flee (along a path, to cover or away) |
| hide | avoid someone you don't want to meet | break line of sight: behind or under furniture, walls |
| gather in formation | conversation circle, onlookers | herd bunching, pack surrounding |
| move with | walking together | herd moving, pack following a leader |
| yield | give way in a doorway | — |
| freeze | — | rabbit freezes as a *choice* (not as today's bug) |

### Conversation groups (F-formation)

Kendon's F-formation: people talking stand around a shared empty centre (the o-space). Two
people: face to face or side by side; three or more: a rough circle.

- A small group record: centre, members, slot per member on a circle around the centre.
- Members steer to their slot and face the centre (8x8 sprites: a left/right flip, maybe a talk
  indicator; most of the effect comes from placement).
- The centre is a soft obstacle for passers-by: they walk around, not through.
- Joining: a mover who likes a member and is idle takes a free slot; the circle widens.
- Onlookers around a fire, a fight or a performer are the same thing with a bigger circle.

### Walking together

- Two movers share one route; the follower keeps a side or rear offset (`offset_pursuit`,
  `leader_follow` in `experiments/steering`), the leader slows for the follower, both wait at
  ladders and doors.
- Only for idle or social trips, not for hauling jobs. Hardest of the set: it touches paths and
  jobs, so it comes last.

### Personal space and yielding by relationship

- Separation radius per pair: smaller for friends, bigger for strangers and enemies.
- Uneven strength: the polite or lower-status one is pushed more; `moverYielding[]` already
  exists as a starting point.
- Re-measure throughput: the corridor result in `../../research/crowd-avoidance-comparison.md`
  (separation is best) was measured with symmetric separation.

### Hiding

The one really new piece: a **cover search**. Find nearby reachable cells that the threat can't
see (line of sight blocked by walls or furniture), prefer close and far from the threat, and
path there. A small search on the nav graph plus line-of-sight checks.

---

## Animals on the new system

1. **Move like movers**: real paths on `nav/`, levels, ramps, doors, the same separation; local
   steering only for the last stretch. Fleeing and hiding need this: you flee along a path to
   somewhere safe, not in a straight line.
2. **Perceive through the noticing layer**, which replaces the hand-written scans in
   `animals.c` (hunter check, all-animals predator scan, mover flee radius).
3. **Behaviour = relations + response rows.** Grazer, predator, cat and person differ in their
   relations and rows, not in separate behaviour functions.
4. Stalk, chase with fatigue, flee to cover, panic spreading through a herd (a herd member
   noticing a fleeing member flees too: contagion), freeze as a choice.

Outdoor grazers and predators move over after the cat works indoors.

---

## Dollhouse first slice

The dollhouse (`StartDollhouse` in `src/core/games.c`) is small and fully visible, with a
1-wide hall and doors: a good test for yielding and indoor animal movement. It has two
residents, which only gives pairs, so it gets:

- **a cat**: hides under the bed or table when startled, comes to whoever is in the kitchen,
  follows its favourite person, gets out of the way in the hall. First animal on the new system;
  doubles as the test that animal movement works between walls and doors, where today's code is
  weakest.
- **later a visitor or third resident**: conversation circles of three, "avoid the annoying
  neighbour".

### Steps

1. **Noticing layer** for movers: `noticed[]` per mover, staggered update, line of sight,
   attention budget. Show it: describe lines (DESC_DEBUG: "notices: Anna (friend, 2 s)") and a
   debug overlay of who notices whom. Scenario `social` (a plaza, ~10 movers, preset friends
   and feuds) for counting things headless.
2. **Relations**: per-pair like/familiarity, familiarity grows from noticing. Start the two
   residents as partners.
3. **Ambient responses**: per-pair personal space, drift toward a liked one when idle, yield in
   doorways. The hall and kitchen should already feel different.
4. **Conversation**: two idle residents who like each other stop, face each other, talk for a
   while (mood + relation up). Passers-by go around.
5. **Animal movement on nav** (paths, levels, doors) behind the existing animal API.
6. **The cat**: relations (owner, startled-by), responses approach / follow / hide / flee,
   cover search.
7. **Outdoor animals** (grazers, predators) moved to rows; the old behaviour functions removed.
8. **Walking together.**

Save impact: noticed lists are transient (rebuilt after load); relations are saved (new mover
data, save bump at step 2); animal path state probably transient.

---

## Decisions (2026-11-24)

- **Petting the cat is a social interaction** between two entities, like a conversation. People
  and animals go through the same interaction code; a person-person talk is just one kind.
- **Can be turned off**: relations and social responses are a switch (`GameDef` sets it, a
  setting row toggles it), like hunger.
- **Noticing across levels is v2.** v1 is same level only.
- **How a conversation shows** is the interesting part, and needs **facing** (below).

## Facing and showing interactions

Today every mover is one symmetric front-view face sprite (`head`, tinted), no facing stored.

### Facing

- Store a facing per agent (8 directions, or a float angle snapped for drawing). It follows the
  walking direction, but can be **set apart from movement**: face the conversation centre, face
  the cat, look at what you noticed while walking past (a glance).
- Drawing at 8x8 options:
  1. **Face variants**: front (eyes + mouth), left, right (features shifted to one side), back
     (no features). 3-4 sprites per creature; the classic answer.
  2. **Eye shift only**: the same head, the two eye pixels drawn shifted toward the facing
     direction. One sprite plus a tiny overlay; works in 8 directions.
  3. **Direction mark**: a pixel or notch on the rim. Readable, ugly.
- Animals need a side view that flips (cat walking left/right) plus front/back.

### Showing a conversation (open, to sketch)

Ideas to try, cheapest first:
- **Placement + facing**: they stop and turn to each other (F-formation). Needs nothing else.
- **Turn-taking pips**: small dots above whoever is "speaking", alternating between members.
- **Topic icons** (The Sims' bubble idea): the bubble shows *what* they talk about, taken from
  real state: a recent moodlet, something one noticed, an item, the other person. The talk is
  about the world.
- **Bob / gesture**: a small hop or lean while speaking (there is already an `animPhase`).
- **Outcome**: a heart or a little cloud when it ends (relation up or down).
- Petting the cat: the person faces the cat, a hand/heart pip, the cat's purr pip.

## Open questions

- Pick the facing drawing (variants vs eye shift) and the conversation look: sketch a few in
  a mock image before building.

## Related

- `../pathfinding/social-navigation.md`: crowd throughput (bottlenecks, queues); this doc is its
  "Phase 2: full social navigation", widened to animals.
- `../mood-moodlets-personality.md`: traits and moodlets the responses use and feed.
- `../../research/crowd-avoidance-comparison.md`, `../../research/3d-crowd-bottlenecks.md`.
- `experiments/steering`: offset pursuit, leader follow, queue, formation, orbit, wander.
- `../../vision/city-1999.md`: strangers, familiarity, avoidance, onlookers come next there.
