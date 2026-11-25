# Noticing: agents that see each other and do something with it

> Status: partial (2026-11-25): people side built (noticing, faces, steering, talks, gatherings,
> relations, walks, games); animals and the cat not yet. Summary below, original design after it.

One loop for every agent, person or animal: **notice → judge → respond**. Social behaviour
(talking in a circle, walking together, giving way, avoiding someone) and animal behaviour
(hunting, fleeing, hiding, herding) are the same loop with different relations and responses.

Skills are out of scope here (see `../10-personality-and-skills.md`).

---

## Built (2026-11-25)

Code: `src/entities/notice.c` (noticing, passing, glances/greetings, friends and foes),
`talk.c` (talks, gatherings, walks, table talk, mingling, spot choice, auto gatherings),
`relations.c` (who knows whom, saved), `chase.c` (tag, hide and seek, follow the leader), faces
in `src/render/rendering.c` (`MoverFaceView`, `MoverMouth`, `DrawMoverFace`). Tests:
`tests/test_notice.c`. Every tunable is a settings row under Entities/Movers/Noticing (and
Entities/Movers/Walls for doorways).

| Part | What it does | Setting(s) | Scenario | Measured |
|---|---|---|---|---|
| Faces | head looks where it heads (left/right/up/down/front built in code from the front faces: the inside shifted 1 px), mouth from mood / talking / greeting, eyes shut asleep | `moverFaces` | `faces` | |
| Noticing | each mover keeps the ~4 most salient others in sight on its level (staggered, 4x/s); standing still it looks at the top one | `notice`, `noticeRadius`, `noticeEveryTicks` | `social` | 1 noticed per mover |
| Polite passing | someone coming at you: drift aside early (never towards a drop) | `politePassing`, `passRadius`, `passStrength`, `passSlow` | `hallway` | close contacts -77%, arrivals -1.6% |
| Talks | idle movers stop, face their middle, take turns speaking; up to 4, circle re-forms; walkers steer round | `talk` (game rule), `talkRadius/Chance/Min/Max/Cooldown`, `talkGiveRoom` | `social` | walkers through a circle -87..98% |
| Doorways | wait until the one you let out has passed (not in a crowd); patience by trait; "after you" nod, smile back | `yieldLetPass` | `doorway` | bumps 82 -> 11 (8 movers) |
| Gatherings | rings round a performer or a place (campfire): spot on your side, 2nd ring when full; round a place they chat, more after dark | `watchRadius/Chance/Min/Max` | `gathering`, `garden-party` | |
| Gatherings by themselves | round a burning fire workshop, heat furniture (burning barrel), someone cooking | `autoGatherings` (game rule) | `campfire` | 5 at the fire, 4 at the barrel |
| Relations | up to 16 known others: like, familiarity; grow from seeing, talking (by mood), "after you"; saved v111 | `relSeen`, `relTalkFam`, `relTalkLike` | `social` | |
| Friends and foes | seek liked ones you know (12 cells); steer round / refuse goals near / leave the disliked | `socialSeek`, `socialAvoid` | `social-ties` | enemies close -70% |
| Glances and greetings | glance at people you notice; acquaintances nod, friends say hi, once per meeting; look away from foes | `glances`, `greetings` | `garden-party` | 90 greetings / 3 days |
| Walks | friends outdoors stroll, up to 6, formation (single file at doors), chat (look ahead, glance over) | `walkChance/MinLike/Max/Pace` | `social-ties`, `garden-party` | followers 0.33 cells from their spot |
| Table talk | diners at the same table (sit-then-eat + linger, navkit-3a's action queue) talk | | `garden-party` | 98 of 100 meals seated |
| Spot choice | seats / ring spots next to friends, not next to strangers, away from foes (`SpotPreference`, used by the queue's seat finder) | `spotByFriends` | `garden-party` | |
| Mingling | move to a group you like clearly better; after a talk, go to a friend's group | `mingle`, `mingleChance` | `mingle` | more different talk partners |
| Tag | chase aims where the target is going; flee to open spots along paths; stamina | `fleeSmart`, `chaseSpeed`, `fleeSpeed`, `fleeRadius` | `tag` | a chase 27 s smart vs 6 s straight away |
| Hide and seek | cover search (out of sight, tucked in, apart); seeker remembers where it looked | `hideSmart` | `hide-and-seek` | found after 18 s vs 7 s |
| Follow the leader | the line walks the leader's exact route (trails) | `followTrail` | `follow-leader` | 0.05 cells off the route vs 0.27 |
| Pack hunt | 4 hunters on 1 runner: the nearest drives, one cuts it off where it's going, two flank; the runner flees from all of them (not past one), tires. The runner is faster than the hunters when fresh (`packRunnerSpeed` 1.4 vs `packSpeed` 1.1), so they need their roles | `packTactics`, `packSpeed`, `packRunnerSpeed` | `pack-hunt` | catch in 15.5 s vs 19.3 s all-straight (24 game-min); runner 1.2: 11.6 vs 15.0 |
| Pack hunt: only by sight | the pack knows only where one of them last saw the runner; out of their sight for 0.4 s it lies low next to cover / in a bush (hidden from them and from where they'll look), crouched; seen 0.4 s or a hunter within 2.5 cells: it runs again. Lost, the hunters go where it was heading, then search round the last-seen spot (next to cover, out of their view, spread out); 30 s lost: it got away | `packStealth` | `hunt-cover` | catch in 15.1 s vs 19.4 s all-straight; ~60 ducks per 24 min, 1.5-4 s each; it rarely gets away. (Fleeing toward spots they can't see made it easier to catch: dropped) |
| Games by themselves | idle friends near each other propose a game (tag, hide and seek or follow the leader), the ones asked accept by liking (and loneliness, with the social need on); the proposer is it / seeks / leads; it ends after 40-90 s, then a 120 s cooldown; free friends who see a game join it (tag: a runner, hide and seek: hides or waits for the next round, follow the leader: at the back); describe lines for every role ("Playing tag: it", "Hide and seek: hiding") | `autoGames` (game rule; scenarios off), `gameJoinChance` | `playtime` | 6 friends, 4 game days: 9 games (4 tag, 3 hide and seek, 2 follow the leader), 9 joins, 3.1 players a game, ~41 s each, playing 11% of the time; meals 91 vs 89 off |
| Playing is busy | a game is a free-time state (`FREETIME_PLAY`): players get no jobs and don't stop to talk; a need (hungry, thirsty, tired, bladder, anything critical), a job, drafting or a player command takes a player out, the game goes on (a new "it" / seeker / leader) or ends when one is left; calm players stroll near the others | | `play-hungry` | 8 players leave one by one to eat, 28 meals, lowest hunger 0.21 (0.19 without a game) |
| Sight | `CanSeeCell`: walls block, railings (half height) and windows don't; noticing, gatherings, hide and seek use it (walking straight stays `HasLineOfSight`) | | `pack-hunt` (railings) | |
| Cover | a crouching mover (a hider in its spot) behind low cover is out of sight from the far side: railings, bushes (from every side), tables, desks, sofas, tills, baths, fridges; within 2 cells you look over it. Hiders pick spots behind cover, the seeker goes to look behind it; drawn smaller and darker | `cover` | `cover` | found after 8.8 s vs 6.6 s (12 players: 11.8 vs 5.5) |
| Personal space | how close two stand and pass by how they feel (`PersonalSpaceScale`, relations.c): friends 0.75x (closer and they overlap), known 1x, strangers 1.25x, disliked up to 1.5x; talk circles, walking side by side (only wider: 0.8 cells is already close), polite passing (friends pass closer; never a wider berth than normal: that pushed walkers through one-way doors) | `personalSpace` | `personal-space` | talking pair: friends 1.14 cells apart, strangers 1.80 (off: 1.5 both) |

Game rules: `talkEnabled` on in survival, island, dollhouse and town (off in the sandbox and
the pathfinding scenarios, where talks cut walking throughput); `autoGatherings` on in survival,
island and dollhouse. Both are saved since v115 (with `moodEnabled`, `jobDistanceWeight`,
`toiletsPlumbed` and the running game). Walks only
outdoors (`IsExposedToSky`).

## Next

- **Joint activities** (`joint-activities.md`, spec): talks and games as one kind of thing (a table row per kind, one lifecycle, one busy state, several games at once), then games that start by themselves. Next up.
- **Animals on these blocks**: real paths (levels, doors), perception through noticing, flee /
  chase / hide from `chase.c`, wolves as a pack (pack hunt roles), herds (walk formation), freeze
  as a choice. Cover and a runner lying low are built; a lone wolf losing its prey would test the search alone. The cat first
  (dollhouse): hides, comes to the kitchen, follows its favourite, a gathering source, petting
  as an interaction.
- **Showing what a talk is about**: topic icons from real state, a heart or cloud at the end.
- Smaller: decay of unused relations, friendship pace (a talk at
  neutral mood is +1 like), noticing across levels (v2), cooking gatherings seen in a running game.
- Known, not ours: on the island a mover sometimes clips a cliff-edge air cell when a
  string-pulled path cuts a corner (path smoothing).

---

## Design (2026-11-24, the plan this grew from)

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

Steps 1-4 and 8 are built (see "Built" above). Left: 5. animal movement on nav (paths, levels,
doors) behind the existing animal API; 6. the cat; 7. outdoor animals (grazers, predators) on
the same rows, the old behaviour functions removed.

Save impact: noticing, talks, gatherings, walks, games, greetings are transient (a load ends
them); relations are saved (v111, a block after the belts). Animal path state probably transient.

---

## Decisions (2026-11-24)

- **Petting the cat is a social interaction** between two entities, like a conversation. People
  and animals go through the same interaction code; a person-person talk is just one kind.
- **Can be turned off**: relations and social responses are a switch (`GameDef` sets it, a
  setting row toggles it), like hunger.
- **Noticing across levels is v2.** v1 is same level only.
- **How a conversation shows** is the interesting part, and needs **facing** (below).

## Facing and showing interactions

Built: the faces are made in code from the front faces (`head_front_<mouth>` + the blank
`head_back`, the inside shifted 1 px toward the look); facing is computed when drawing (not
stored): talk/doorway/glance target, else the next waypoint, else who it noticed most. Below:
the options as discussed.

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

- Conversation look beyond placement, facing and mouths: topic icons? turn-taking pips?

## Related

- `../pathfinding/social-navigation.md`: crowd throughput (bottlenecks, queues); this doc is its
  "Phase 2: full social navigation", widened to animals.
- `../mood-moodlets-personality.md`: traits and moodlets the responses use and feed.
- `../../research/crowd-avoidance-comparison.md`, `../../research/3d-crowd-bottlenecks.md`.
- `experiments/steering`: offset pursuit, leader follow, queue, formation, orbit, wander.
- `../../vision/city-1999.md`: strangers, familiarity, avoidance, onlookers come next there.
