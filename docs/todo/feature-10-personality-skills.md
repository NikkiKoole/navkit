# Feature 10: Personality, Skills & Moods

> **Priority**: Tier 4 — Living Colony
> **Why now**: After features 1-9, colonists eat, sleep, use tools, wear clothes, farm, and live in shelters — but they're identical, nameless, and emotionless. This is what turns logistics into storytelling.
> **Systems used**: All needs (F1, F2, F8), all jobs, all workshops, shelter (F3)
> **Depends on**: Most of features 1-9 (needs create mood inputs, jobs create skill opportunities)
> **Opens**: Emergent stories, player attachment, social dynamics

---

## What Changes

Movers get **names**, **skills**, and **moods**. Skills improve with practice — a mover who mines a lot becomes a faster miner. Moods are affected by need satisfaction, comfort, and events. Bad moods cause work slowdowns; good moods give bonuses. Each mover becomes a *person* the player cares about.

---

## Design

### Names & Identity

Each mover gets:
- **Name**: Random from a name list (culturally neutral)
- **Age**: Cosmetic for now (affects sprite tint slightly)
- **Appearance seed**: Determines sprite color variations

Names appear in tooltips, job assignment UI, and mood notifications. This is cheap to implement but dramatically changes how players relate to movers.

### Skill System

**Skill types** (mapped to job categories):

| Skill | Affects | Trained By |
|-------|---------|-----------|
| Mining | Mine, channel, ramp speed | JOBTYPE_MINE, CHANNEL, DIG_RAMP |
| Woodcutting | Chop speed | JOBTYPE_CHOP, CHOP_FELLED |
| Construction | Build speed, quality (future) | JOBTYPE_BUILD |
| Crafting | Workshop work speed | JOBTYPE_CRAFT |
| Farming | Till, plant, harvest speed | JOBTYPE_TILL, PLANT_CROP, HARVEST |
| Hauling | Walk speed while carrying | JOBTYPE_HAUL |
| Cooking | Cooking speed, food quality (future) | Cooking recipes |
| Hunting | Hunt speed, success rate (future) | JOBTYPE_HUNT |

**Skill levels**: 0–20 (integer)
- Level 0: 0.8× speed (untrained penalty)
- Level 5: 1.0× baseline
- Level 10: 1.2× (experienced)
- Level 15: 1.5× (expert)
- Level 20: 2.0× (master)

**Experience**: Each completed job gives XP. Level thresholds increase (standard RPG curve). Movers naturally specialize based on what they do most.

**Speed stacking**: Tool quality × skill multiplier × clothing comfort (if cold). All multiply together.

### Starting Variance

New movers start with random skill distributions:
- 2-3 skills at level 3-5 (natural aptitudes)
- Rest at level 0-1
- This creates immediate differentiation: "This mover is good at mining, assign them to mine"

### Mood System

Mood is a composite score from multiple factors:

```
mood: float -1.0 (miserable) → 0.0 (neutral) → 1.0 (happy)
```

**Mood inputs** (each contributes a modifier):

| Factor | Bad | Neutral | Good |
|--------|-----|---------|------|
| Hunger | Starving: -0.3 | Satisfied: 0 | Recently ate good food: +0.1 |
| Energy | Exhausted: -0.2 | Rested: 0 | Slept in bed: +0.1 |
| Thirst | Dehydrated: -0.3 | Hydrated: 0 | — |
| Temperature | Freezing: -0.3 | Comfortable: 0 | — |
| Shelter | Rained on: -0.1 | — | Sheltered room: +0.1 |
| Clothing | Naked in cold: -0.2 | Dressed: 0 | Good clothing: +0.05 |
| Food variety | Same food always: -0.1 | 2+ types: 0 | 3+ types: +0.1 |
| Idle time | Overworked: -0.1 | Balance: 0 | Some free time: +0.1 |

Mood is a rolling average (smoothed over time, not instant).

### Mood Effects

| Mood Range | Effect |
|------------|--------|
| -1.0 to -0.5 | **Miserable**: 0.7× work speed, may refuse jobs |
| -0.5 to -0.2 | **Unhappy**: 0.85× work speed |
| -0.2 to 0.2 | **Neutral**: 1.0× speed |
| 0.2 to 0.5 | **Content**: 1.1× work speed |
| 0.5 to 1.0 | **Happy**: 1.2× work speed, may do extra work |

### Mood Breaks (Future)

At extreme negative mood, movers might:
- Refuse to work for a period (sit idle)
- Eat extra food (comfort eating)
- Wander aimlessly

Keep this simple for Phase 1 — just speed effects. Dramatic breaks come later.

### Personality Traits (Phase 2)

Optional future extension: each mover has 1-2 personality traits that modify mood sensitivity:

- **Hard worker**: Less penalty from overwork, more penalty from idleness
- **Outdoorsy**: Less penalty from weather, more penalty from being indoors
- **Picky eater**: More penalty from food monotony, more bonus from variety
- **Social**: Bonus from working near others (future)

Traits create stories: "Kira is a hard worker and an expert miner, but she's picky about food and unhappy eating berries every day."

---

## Implementation Phases

### Phase 1: Names & Identity
1. Name generation (random from word list, or simple syllable combiner)
2. Add name, age, appearance seed to Mover struct
3. Display name in tooltip and selection UI
4. **Test**: Movers have unique names

### Phase 2: Skill System
1. Add skill array to Mover struct (8 skills, level + XP)
2. Starting skill distribution (random aptitudes)
3. XP gain on job completion
4. Level-up thresholds
5. Skill speed multiplier in job execution
6. **Test**: Skills increase with practice, speed improves with level

### Phase 3: Mood Foundation
1. Add mood float to Mover struct
2. Calculate mood inputs each tick (hunger, energy, thirst, temperature, shelter)
3. Rolling average smoothing
4. Mood speed multiplier
5. **Test**: Bad conditions lower mood, good conditions raise it, speed affected

### Phase 4: Mood UI
1. Mood indicator in mover tooltip (color-coded)
2. Mood breakdown showing contributing factors
3. Colony average mood in status bar
4. **Test**: UI displays correct mood information

### Phase 5: Skill Specialization
1. Job assignment prefers movers with higher relevant skill
2. WorkGiver considers skill level when choosing which mover to assign
3. Skill-based job priority (expert miner assigned to mining before novice)
4. **Test**: Higher-skill movers get preferred assignments

---

## Connections

- **Uses all needs (F1, F2, F8)**: Hunger, energy, thirst are mood inputs
- **Uses shelter (F3)**: Room quality affects mood
- **Uses clothing (F7)**: Clothing comfort affects mood
- **Uses food variety (F5, F6)**: Multiple food types = mood bonus
- **Uses all jobs**: Every job type trains a skill
- **Uses tools (F4)**: Tool quality + skill level stack multiplicatively
- **Opens for future**: Social dynamics, relationships, recruitment, death grief

---

## Design Notes

### Why Last?

Skills and personality don't mean much without systems to be skilled *at*. Farming skill matters only if farming exists. Mood inputs matter only if hunger, shelter, and clothing exist. This feature is the capstone that gives meaning to everything built before it.

### Keep It Simple

The AI discussions envision detailed personality systems with DF-level depth. Start minimal:
- Names (immediate player attachment)
- Skills (immediate gameplay feedback — "she's getting better!")
- Mood (immediate consequence — "they're unhappy, I need to fix something")

Personality traits, social dynamics, and mood breaks are Phase 2+.

### The Storytelling Emergence

The magic happens when systems combine: "Kira the expert miner is miserable because she's been eating nothing but berries and sleeping on the ground. I should build her a bed and cook some meat." That's a story that emerged from systems, not scripting. That's the DF/RimWorld magic.

---

## Save Version Impact

- New Mover fields: name (char array), skills (8 × level + XP), mood, age, appearance seed
- Significant Mover struct size increase
- New save migration (largest single version bump)

## Test Expectations

- ~40-50 new assertions
- Name uniqueness
- Skill XP gain and leveling
- Skill speed multiplier calculation
- Mood calculation from inputs
- Mood speed effect
- Skill-based job assignment preference
- Mood smoothing (rolling average)
