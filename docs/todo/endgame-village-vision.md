# Endgame Village Vision

> Status: idea

> What does a 20th-century big village look like? Working backward from the destination to find what systems we need.

## The Picture

A village of 50-200 movers. Multi-story apartment buildings with staircases and elevators. A baker who walks to work at 7am, a farmer who tends fields at dawn, a night guard who sleeps during the day. Communal dining halls, pubs, parks, market squares. Movers with names, histories, relationships, favorite spots. Rain on the windows, smoke from chimneys, children playing.

Not a city. Not industrial. A village where everyone knows each other and life has rhythm.

## A Mover's Day

```
 6:00  Wake up in apartment (floor 3, room 2). Personal space, decorated.
 6:15  Walk downstairs. Staircase or elevator.
 6:30  Breakfast at communal kitchen. Sit with household or friends.
 7:00  Walk to workplace. Baker → bakery. Farmer → fields. Builder → construction site.
12:00  Lunch break. Eat near workplace or walk home.
13:00  Back to work.
17:00  Done. Walk home or to leisure spot.
17:30  Evening meal. Household eats together, or visit the pub.
19:00  Leisure. Sit on favorite bench. Visit friend. Wander the market.
21:00  Home. Sleep.
```

100% of early-game time is survival labor. In the village, maybe 40% is productive work, 30% is maintenance (eating, sleeping, commuting), 30% is leisure. That leisure slice is where personality lives.

## Systems: What Exists vs What's Needed

### Already Have (needs scale)

| System | Status | Endgame role |
|--------|--------|-------------|
| Grid + z-levels | Working | Foundation for vertical buildings |
| Construction | Working | Walls, floors, doors, ramps, ladders |
| Farming | Working | Food production at scale |
| Cooking + crafting | Working | Workshops produce goods |
| Needs (hunger, thirst, energy, temp) | Working | Still the base layer, but no longer dominant |
| Weather + seasons | Working | Atmosphere, farming constraints |
| Items + stockpiles + hauling | Working | Distribution backbone |
| Tools + clothing | Working | Equipment progression |
| Fire + water sim | Working | Hazards + infrastructure |
| Fog of war | Working | Exploration |

### Partial (foundation exists, needs extension)

**Vertical circulation** — Ladders work but are single-file and slow. Endgame needs staircases (wide, fast, multi-mover capacity) and elevators (queuing, call buttons, capacity limits). These are the circulatory system of apartment buildings. Docs exist: `world/elevators.md`.

**Furniture** — Beds and chairs exist. Endgame needs dozens: tables, shelves, lamps, stoves, bathtubs, couches, desks, windows, curtains, rugs, paintings. Each contributes to room quality and satisfies different needs. Furniture is what turns a box into a home.

**Room system** — Enclosed-space detection exists (shelter check). Needs to become room *quality* scoring: size, furniture count, light level, floor material, cleanliness, window/view access. Room quality feeds into mood. A dirt-floor shack with a leaf pile vs a furnished apartment with a real bed — that gap should feel enormous.

**Mood** — Designed in `10-personality-and-skills.md` but not built. In the endgame mood is THE central system. Every other system feeds into it. A mover's mood is the aggregate of: housing quality, job satisfaction, social life, food quality, comfort, fun, purpose.

**Skills** — Designed but not built. Endgame needs deep specialization. A baker with 10 years experience produces better bread faster. Skill identity ("I'm the village blacksmith") is a source of pride/mood.

**Lighting** — Block light exists. Needs to matter for mood and room quality. Dark room = bad, well-lit = good. Street lamps make the village feel alive at night.

### New Systems Needed

#### Infrastructure

**Staircases** — Multi-tile, bidirectional, faster than ladders. Aesthetic tiers: wood → stone → iron railing. A 3-story apartment needs a stairwell, not a ladder shaft. Pathfinding cost should favor stairs over ladders.

**Elevators** — Moving platform with capacity limit, call system, queuing. Essential for tall buildings and accessibility. See `world/elevators.md`. Also cargo elevators for vertical stockpile logistics.

**Plumbing** — Wells → pipes → taps. Early game: haul water from river. Mid game: dig well, haul from well. Late game: pipe network delivers water to kitchens and bathrooms. Running water in apartments vs bucket hauling is a quality-of-life revolution.

**Heating** — Fireplace → chimney → radiator network. Per-room temperature tracking already exists via temperature grid. Endgame: central heating plant distributes warmth through pipes. Cold apartment = mood penalty.

**Electricity** (late) — Generator → wire → outlet. Powers: lights (replace torches), appliances (powered workshops), comfort items (fans, heaters). Big infrastructure project but transforms the village feel.

**Transport network** — The village outgrows walking. A 200-mover village with farms on the outskirts, mines in the hills, and apartments in the center needs transport. Trains connect distant areas (farm stop → village center → mine). Elevators move movers and cargo vertically in tall buildings. Timetables create daily rhythm — morning rush, last train home. Personal carts/horses for movers who live far out. Transport quality directly affects commute time, which directly affects mood and leisure budget. A mover with a 2-minute train commute has 30 more minutes of pub time than one who walks 15 minutes. See `pathfinding/transport-layer.md` for unified design, `world/trains.md` and `world/elevators.md` for specifics.

#### Economy & Work

**Occupations** — Not just "do nearest task" but assigned roles with schedules. See `schedule-system.md`. Baker, farmer, builder, guard, doctor, teacher. Movers have a job they *go to*.

**Workplaces** — A bakery isn't just a crafting station. It's a *place* someone spends 8 hours. Has its own room quality. Commute distance matters. A mover assigned to a faraway workplace is less happy.

**Production chains** — Wheat → flour → bread → bakery shelf. Iron ore → smelter → ingots → blacksmith → tools. Clay → bricks → builder → apartment wall. Chains get deep. Each link is a workplace, an occupation, a skill track.

**Distribution** — How does bread get from bakery to apartments? Stockpile system scales down at village size. Options: communal stores (current system but bigger), market/shop system (movers "buy" from shops), or direct delivery jobs.

**Resource allocation** — Who gets the nice apartment? First-come, need-based, status-based, player-assigned? This is a design question more than a technical one. Simplest: player assigns housing. Most interesting: movers have preferences and negotiate/compete.

#### Daily Life

**Schedules** — Clock-driven daily routines. See `schedule-system.md` for full design. The foundational shift from need-driven to time-driven behavior.

**Commute** — Home → workplace pathfinding happens daily. Distance matters for mood and time budget. A mover who lives above the bakery has 30 extra minutes of leisure. Urban planning matters. With transport: movers choose walk vs train based on cost. Train timetables constrain late-shift workers. Building a station near the housing district is an urban planning decision with real mood consequences.

**Leisure activities** — Sitting in park, visiting pub, reading, playing games, walking, socializing. Each satisfies different sub-needs (social, fun, relaxation). Leisure venues are as important as workplaces.

**Fun / boredom** — New need axis. Work drains fun, leisure restores it. Repetition = diminishing returns. Variety = satisfaction. See `schedule-system.md` for mechanic details.

**Routines & habits** — Movers develop preferred spots. "Kira always sits on that bench at sunset." Emergent from preference weighting + repetition bonus. Gives movers personality without explicit personality traits.

#### Social

**Relationships** — Friendship/family/romance as float values between mover pairs. Proximity + shared meals + conversation builds bonds. Working together builds camaraderie. Living together builds family.

**Conversation** — Movers talk when near each other. Even fake-conversation (speech bubbles, gibberish sounds) adds enormous life. Your sound research (`sounds/sound-and-mind-speech-comm.md`) covers token-based communication.

**Households** — Movers grouped into homes. Shared meals, shared space, shared mood effects. A household that eats together has higher social satisfaction.

**Community spaces** — Pub, market square, park, temple, workshop yard. Tagged rooms where social gathering happens naturally. Building a pub isn't just a building — it's creating a social institution.

#### Housing

**Apartments** — Room assignment: this room belongs to this mover/household. Personal space that the mover cares about. Messy room = mood penalty. Well-furnished = mood bonus.

**Decoration** — Movers wanting to improve their space. Paintings, rugs, better furniture. Could be player-directed or autonomous (mover acquires decoration items during leisure, places them in their room).

**Multi-story buildings** — Not just stacked rooms but coherent buildings. Shared stairwell, lobby, hallways. Roof. Building templates or freeform construction with room detection.

**Housing quality tiers** — Leaf pile on dirt floor (survival) → wooden shack (early) → stone house (mid) → furnished apartment (late). Each tier is a visible, tangible improvement. The journey from shack to apartment should feel like progress.

## The Core Loop

```
Better housing    → Better mood → Faster/better work → More resources
Better food       → Better mood → Faster/better work → More resources
Better social life → Better mood → Faster/better work → More resources
                                                      ↓
                                              Build better housing
                                              Grow better food
                                              Create social spaces
                                                      ↓
                                                    (cycle)
```

In early game, the loop is: find food → don't die → find more food.
In endgame, the loop is: improve quality of life → happier movers → more productive → improve quality of life.

The transition from survival to quality-of-life is the arc of the whole game.

## What Makes It Feel Like a Village (Not a Factory)

- **Movers have downtime.** They're not working 24/7. They sit, wander, chat, sleep.
- **Spaces have purpose.** The pub is for evenings. The park is for sunny days. The kitchen is for mornings.
- **Time has rhythm.** Morning bustle, quiet afternoon work, evening socializing, night silence. The train runs from 6am to 10pm. The last train home is a real thing.
- **Infrastructure shapes life.** Where you put the train station, the stairwell, the elevator — it determines who lives where, who eats with whom, who has time for the pub.
- **Movers have preferences.** Kira likes the bench by the river. Torvald always eats at the pub. Enna is the first one up every morning.
- **Buildings have character.** The bakery smells of bread (particles?). The smithy glows orange. The pub is noisy.
- **Weather matters socially.** Rainy day → movers stay inside → pub is crowded. Sunny day → park is full. Snow → everyone by the fire.

## Key Design Questions (Open)

1. **Scale target** — 50 movers? 100? 200? Determines performance requirements for pathfinding, rendering, job assignment, social tracking.

2. **Economy model** — Communal (player distributes everything) vs market (movers trade/buy) vs hybrid? Communal is simpler and fits colony-sim. Market creates emergent economy but is complex.

3. **Autonomy level** — How much do movers decide for themselves? Player assigns all housing + jobs (management sim) vs movers choose and player nudges (life sim)? Probably a spectrum that shifts as village grows.

4. **Tech progression** — How many eras between neolithic and 20th century? Each era needs enough content to feel distinct. Rough sketch: Stone Age → Bronze/Iron → Medieval → Early Modern → Industrial → Modern. That's 6 eras minimum. Or compress: Primitive → Settled → Village → Town.

5. **Vertical limit** — How tall can buildings go? 3 stories? 10? 20? Affects elevator design, rendering, pathfinding cost. Probably 5-6 stories is the sweet spot for a village feel.

6. **Children** — Do movers reproduce? Children who grow up, learn skills, inherit traits? Massive system but creates generational attachment. Could defer to very late.

## What This Doc Is For

This isn't a build plan — that's what the unified roadmap is for. This is a **destination picture** to evaluate features against. When designing any new system, ask: "Does this still make sense when there are 100 movers in a 5-story apartment building?" If yes, build it. If it only works for 5 movers in a field, reconsider.
