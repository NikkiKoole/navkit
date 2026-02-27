# Sims-Inspired Priorities

> What makes The Sims work, what navkit already has, and a reordered plan to get the Sims feel fastest.

---

## 1. Why The Sims Works

The core magic is **watching autonomous agents make legible decisions about their needs, then caring about the outcomes**. Everything flows from six pillars:

### Needs are visible and urgent
8 need bars are always on screen. You can *see* a Sim getting desperate — the bar turns red, they get a thought bubble, their animation changes. The tension: you can't satisfy all needs at once. Trade-offs are constant.

### Objects have quality tiers
A cheap bed restores energy slowly. An expensive bed restores it fast AND gives comfort. This creates the progression loop: earn resources -> buy better stuff -> needs satisfied faster -> more free time -> more skills/fun. The player's main lever is *designing the environment*, not micromanaging the Sim.

### Personality creates divergent behavior
Sloppy Sims leave messes. Neat Sims clean autonomously. Active Sims want to exercise. Lazy ones want to sit. Same needs system, different priorities -> each Sim feels unique.

### Mood is the master currency
Good mood = they do things well, build skills faster. Bad mood = they refuse tasks, cry, fall asleep on the floor. Mood is the *sum of need satisfaction* — not a separate system. It's the glue that connects every other system.

### The environment tells a story
A messy kitchen with dirty plates = someone's been cooking but nobody cleaned. A room full of cheap furniture = early game struggle. The physical space IS the game state, readable at a glance.

### Failure is funny, not punishing
Sim pees themselves. Sim falls asleep in their food. Sim catches the kitchen on fire. Death exists but is rare and dramatic. The player emotionally engages through comedy, not anxiety.

---

## 2. What NavKit Already Has

| Sims Pillar | NavKit Status | Notes |
|---|---|---|
| Needs driving behavior | **Done** | Hunger, thirst, energy, temperature. Full autonomous freetime state machine — movers seek food, water, rest, warmth on their own with priority ordering. |
| Object quality | **Partial** | Plank bed (0.040/s) > leaf pile (0.028/s) > ground for energy. Different foods restore different amounts. But no explicit quality tiers or quality UI. |
| Tool/equipment progression | **Done** | 3 quality tiers, 5 types, speed multipliers. 7 clothing items with insulation %. |
| Environment data | **Partial** | Floor dirt, ground wear, mud, temperature, wetness grids all exist. But none of it affects movers emotionally — it's all physical/cosmetic. |
| Names | **Done** | S17 shipped. Syllable-based generation, gendered, shown in tooltips and HUD. |
| Mood | **Missing** | Designed in F10 spec but not coded. This is the single biggest gap. |
| Fun/comfort | **Missing** | Zero recreation mechanics. No comfort need. No idle penalty. |
| Visual feedback | **Minimal** | Tooltips only. No in-world bars, thought bubbles, status icons, or animation changes for need states. |
| Room quality | **Missing** | Temperature and dirt exist as data but nothing says "this room is pleasant." |
| Personality | **Missing** | All movers are mechanically identical. Planned for F10 Phase 2. |
| Social | **Missing** | No mover-to-mover interaction. Era 3+. |

---

## 3. The Gap That Matters Most

The Sims doesn't work because of any single system. It works because **mood connects everything into a feedback loop**:

```
Better environment -> Better need satisfaction -> Better mood -> Faster work -> Better environment
```

NavKit has the bottom layer (needs, objects, environment data) but is missing the connective tissue (mood) and the legibility layer (visual feedback). Without mood, needs are just survival meters. Without visual feedback, the player can't see what their movers are feeling.

**The key Sims insight navkit is missing**: fun/comfort is a *need*, not a luxury. Sims who never have fun get depressed even with full bellies. This single axis creates The Sims' core tension — your movers can't just work all day, they need downtime.

---

## 4. Reordered Session Plan

The unified roadmap puts skills (S18) before mood (S20). From a Sims-feel perspective, this is backwards. Skills are a systems/progression feature. Mood is what makes you *care*. A mover named Kira who's visibly grumpy because she slept on the ground and ate raw berries — that's the moment the game clicks.

### S18: Mood System

Mood as a float on Mover (-1.0 to 1.0), fed by existing need satisfaction:

- Hunger contribution: full stomach = +0.15, starving = -0.3
- Thirst: hydrated = +0.1, dehydrated = -0.3
- Energy: rested = +0.1, exhausted = -0.2
- Temperature: comfortable = +0.05, freezing = -0.3
- Shelter: indoors (under roof/walls) = +0.1, exposed in rain = -0.1
- Clothing: dressed = +0.05, naked in cold = -0.15

Rolling average smoothing so mood changes gradually, not instantly.

Speed multiplier from mood: 0.7x (miserable) to 1.2x (happy). Applied alongside existing tool quality multiplier.

Mood breaks at extremes: miserable movers occasionally refuse work (cancel job, wander for 10-30 seconds). This is the "Sim crying on the floor" moment.

**~120 lines in mover/needs, ~30 tests. One session.**

### S19: Visual Need Feedback

The Sims' legibility comes from *seeing* state at a glance. Three additions:

**Need status icon above mover**: Small colored dot or symbol floating above the mover sprite when a need is critical. Red droplet = starving. Blue droplet = dehydrated. Zzz = exhausted. Snowflake = freezing. Only shows for the most urgent need. Disappears when need is satisfied.

**Mood indicator**: Colored circle behind or under the mover sprite. Green = happy, yellow = neutral, orange = unhappy, red = miserable. Always visible. This is the plumbob equivalent.

**Work refusal animation**: When a miserable mover refuses work, brief visual flash or different idle animation. The player needs to *see* that Kira is unhappy, not just read it in a tooltip.

**~80 lines in rendering, ~20 in mover. One session.**

### S20: Comfort & Fun Need

Two new need axes that create the Sims' core tension — movers can't just work all day:

**Comfort** (0.0-1.0): Drains slowly while working or standing. Restored by sitting in chairs, resting on beds (already partially works via energy), being near a campfire. Different furniture gives different comfort rates. This makes the player want to build nice spaces.

**Fun/Leisure** (0.0-1.0): Drains while working or idle with nothing to do. Restored by... what? This is the design question. Options that fit the neolithic theme:
- Sitting by a campfire (already exists as heat source — add fun component)
- Idle near other movers (proto-social — being near people is fun)
- Eating good food (cooked > raw, variety bonus)
- Exploring new areas (first visit to a cell = small fun boost)

Low comfort feeds into mood negatively. Low fun feeds into mood negatively. Now the player has to balance *productivity vs wellbeing* — the core Sims tension.

**~150 lines across needs/mover/balance. One session.**

### S21: Room Quality

The environment-tells-a-story pillar. A simple room scoring system:

**Room detection**: Flood-fill from mover position, bounded by walls/doors. Cache room ID per cell (recompute when walls change).

**Room score inputs** (each 0-3 scale):
- Cleanliness: inverse of average floor dirt in room
- Furnishing: count of furniture items in room (bed, chair, table)
- Size: larger rooms score slightly higher (diminishing returns)
- Shelter: walls on all sides + roof = full shelter bonus

**Room score -> mood**: Being in a nice room gives mood bonus. Being in a filthy hole gives mood penalty. This creates the decorating incentive — the player builds nice rooms because movers *feel better* in them, not because the game told them to.

**~200 lines (room detection + scoring). One session.**

### S22: Skills

Now that movers feel like people (names, mood, visual feedback, comfort preferences), give them specializations:

- 8 skills, level 0-20, XP on job completion
- Speed multiplier: 0.8x (untrained) to 2.0x (master)
- Starting variance: 2-3 aptitudes per mover
- Stacks with tool quality and mood: `skill * tool * mood`
- Skill-based job preference in WorkGivers

This lands better after mood because a skilled mover who's miserable still works slowly. The player has to keep their specialists happy, not just assign them to the right job.

### S23+: Continue with biomes, onboarding, capabilities as in unified roadmap

---

## 5. The Sims Moments We're Building Toward

These are the emergent "Sims moments" this ordering enables:

- **S18**: "Kira is grumpy because she ate raw berries and slept on the ground. I need to build her a proper bed." (Mood makes needs have emotional weight.)

- **S19**: "I can see at a glance that three movers are unhappy — the orange dots are clustered near the stockpile. Something's wrong over there." (Visual feedback makes mood legible.)

- **S20**: "Thrak finished mining but his fun is tanking. I should build a campfire area where movers can hang out." (Comfort/fun creates the work-life tension.)

- **S21**: "The movers working in the new wing are happier than the ones in the old cramped room. Time to renovate." (Room quality rewards environmental design.)

- **S22**: "Kira is the best builder but she's been miserable all day. Thrak is slower but cheerful — he'll actually get more done." (Skills + mood interact to create interesting decisions.)

---

## 6. What We're NOT Doing (Sims Features That Don't Fit)

- **Bladder/hygiene need**: Too modern for neolithic. Floor dirt exists but personal hygiene doesn't map to the setting.
- **Career/money system**: Colony sim, not life sim. Resources replace money.
- **Social relationships**: Compelling but complex. Era 3+. The "idle near other movers = fun" mechanic is a lightweight placeholder.
- **Aspiration/lifetime goals**: Too structured for a sandbox colony sim.
- **Detailed scheduling**: The autonomous need system handles this — movers decide when to eat/sleep based on urgency, not a player-set schedule.

---

## 7. Insights from The Sims Source Code & Don Hopkins

Sources: Will Wright's original Motive.c prototype (1997), a C# reimplementation of the motive system ([Sim.cs](https://github.com/AlexanderPatrick/MPhil/blob/7592cd0dfe7c495c94a3037504e390598dcdfb76/Assets/Scripts/Sims/Sim.cs)), and Don Hopkins' HN commentary ([thread](https://news.ycombinator.com/item?id=14997725)).

### How The Sims' Motive System Actually Works

Motives are **interconnected**, not independent:
- **Comfort** is dragged down by bad bladder, bad hygiene, AND hunger — it's a derived feeling, not a raw need
- **Stress** is fed by comfort, entertainment, environment, and social — it's the "mental health" aggregator
- **Hunger** increases when stressed (stress eating)
- **Alertness** (awake/asleep) affects drain rates of everything — hygiene decays 3x faster awake, stress is cut while sleeping
- **Physical** = average of (energy, comfort, hunger, hygiene, bladder) mapped through a squared curve
- **Mental** = average of (stress x2, environment, social, entertained) — stress counts double
- **Happiness** = average of physical + mental, tracked as a rolling lifetime average

The **squared curve mapping** is key — being slightly below neutral barely hurts, but being very negative hurts a LOT. Diminishing returns on the good side too. This means one catastrophically bad need dominates the mood.

### The Advertisement System

Objects don't just sit there — they **advertise** available actions to nearby Sims. A fridge advertises "eat" weighted by hunger. A bed advertises "sleep" weighted by energy. Advertisements score based on: current motivations, distance from object, relationship with object (prefer own bed), and SimAntics scoring functions.

The Sim picks from top-scoring ads with **randomization** so they don't always do the mathematically optimal thing.

### Behavioral Dithering

Don Hopkins: *"If they always chose the top scoring best thing to do, then their lives would be too mechanically optimized, lacking in whimsey and spontaneity."*

The solution: *"we randomly dumbed them down a bit so they needed the user's guidance to have a better life."*

He compared this to error diffusion in image dithering — spreading decision errors over time instead of space. Random choice among top-scored actions creates perceived personality and whimsy without any personality system.

### Implication Over Simulation

*"The trick of optimizing games is to off-load as much of the simulation from the computer into the user's brain, which is MUCH more powerful and creative. Implication is more efficient (and richer) than simulation."*

The astrology sign story: developers displayed zodiac signs with zero behavioral effects. Testers swore the system was working. Player imagination fills in gaps when the domain is familiar.

---

## 8. What We Should Use

### Interconnected motives, not independent bars
Our current needs (hunger, thirst, energy, temperature) are independent — each drains on its own. The Sims insight: bad hunger should drag down comfort. Bad comfort should increase stress. This web of connections creates emergent situations where one bad thing cascades. Cheap to implement, huge emergent payoff.

### Physical + Mental split with squared curve
Instead of one flat mood number, split into physical (hunger/thirst/energy/temperature) and mental (stress/comfort). Average them for happiness. The squared curve means "slightly hungry" barely matters but "starving" is devastating. Matches player intuition.

### Behavioral dithering for autonomy
When movers pick what to do next, don't always pick the optimal action. Randomly choose among the top 2-3 scored options. This makes movers feel like individuals even without personality traits. Very cheap — just adding randomness to existing WorkGiver scoring.

### Comfort as derived, not raw
Don't make comfort a separate need that drains independently. Make it a *consequence* of other needs: bad hunger reduces comfort, bad temperature reduces comfort, sitting in a chair increases comfort. Simpler to tune and more realistic.

### Objects advertising to movers
Instead of movers searching for what they need, objects could broadcast "I satisfy hunger at rate X, distance Y." Our furniture system already half-does this with `restRate` scoring. Extending it to food, warmth sources, and eventually fun sources is natural.

---

## 9. What We Should NOT Use

- **Bladder/hygiene as motives** — doesn't fit neolithic. The Sims is suburban domestic, we're colony survival.
- **Alertness as a continuous motive** — our energy system + binary freetime state machine (working vs resting) is cleaner for our context.
- **Complex social/entertainment at launch** — requires lots of content (interactions, entertainment objects). A simple "idle movers slowly lose mood" captures the spirit without the infrastructure. Add real social/entertainment in later sessions.
- **Lifetime happiness tracking** — rolling averages over days/weeks/lifetime. Interesting for single-household play, less relevant for a colony of 10+ movers.
- **Stress eating / bed wetting** — comedy mechanics that work in The Sims' tone. Our game has a more serious survival tone.

---

## 10. Concrete S18 Proposal (Updated)

Taking from The Sims architecture:

**Physical score** = squared-curve average of (hunger, thirst, energy, temperature)
- All four already exist as 0.0-1.0 floats on Mover

**Comfort** = derived from physical needs + furniture bonus
- Bad hunger/thirst drag comfort down (not a separate drain)
- Sitting in chair / resting on bed pushes comfort up
- Starts at 0.0 (neutral), range -1.0 to 1.0

**Mental score** = squared-curve average of (comfort, stress)
- Stress drains while working, recovers while resting (mirrors The Sims' sleep cuts stress)
- Start simple — add entertainment/social inputs later

**Mood** = (physical + mental) / 2, range -1.0 to 1.0
- Speed multiplier: 0.7x (miserable) to 1.2x (happy)

**Behavioral dithering** = randomize among top 2-3 WorkGiver picks
- Separate from mood, but ships alongside it for emergent feel

Skip for S18:
- Advertisement system (search-based approach works at colony scale)
- Social/entertainment needs (add in S20)
- Complex feedback loops like stress->hunger (add later if needed)
