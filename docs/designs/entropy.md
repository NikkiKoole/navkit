# Entropy

The colony is a bubble of order in a world that wants to swallow it back up.

---

## Mining Reveals What's Below

Mining a wall doesn't *create* floor - it *exposes* what was always there.

```
Mine wall at z=1 → check z=0 → it's dirt? show dirt. it's stone? show stone floor.
Mine wall at z=0 → check z=-1 → bedrock? show rock floor. water table? flood?
```

The world has layers and you're just peeling them:

```
Surface (grass, trees, buildings)
Soil (dirt, sand, clay)
Stone (mineable rock, ore veins)
Bedrock (can't touch)
Water table (flood risk if you dig too deep?)
```

---

## Grass Is Just Dirt With a Hat

Grass isn't a material, it's a *state* of dirt.

```
Dirt + time + no traffic = grass grows
Grass + lots of traffic = worn path (dirt showing through)
Grass + intentional clearing = dirt
Dirt + stone = paved floor
```

Leave dirt alone, nature reclaims it.

---

## Wear and Cleaning

```
Floor (clean) → [movers walk] → Floor (dirty/tracked) → [cleaner mops] → Floor (clean)
Grass (healthy) → [movers walk] → Grass (worn) → [time, no traffic] → Grass (healthy)
                                       ↓
                              [heavy traffic]
                                       ↓
                                 Dirt path
```

- Dirt tracks onto floors
- Mud when raining?
- Cleaners have endless work if you don't put paths in high-traffic areas

**The loop closes:**

- Cleaning job is never "done" - it's maintenance
- Players learn to build paths/roads where movers walk a lot
- Roads = stone floors = no cleaning needed but costs materials
- Grass = free but degrades and needs recovery time

So there's a *reason* to pave beyond aesthetics. Lazy player = dirty base, worn grass everywhere, movers tracking mud inside. Invested player = clean paths, grass preserved in gardens, tidy workshops.

---

## Two Forces Always Pushing

| Entropy (world wants) | Order (colony wants) |
|----------------------|---------------------|
| Grass grows over paths | Pave roads |
| Dirt tracks inside | Cleaners mop |
| Weeds choke farms | Farmers weed |
| Tools break | Smiths repair |
| Food rots | Cooks preserve |
| Torches die | Haulers refuel |
| Walls crack | Masons repair |
| Nature reclaims | Builders maintain |

Every job is fighting one of these. No job is ever truly "done."

---

## Nature Reclaims

Nature doesn't care about your base:

```
Abandoned floor → weeds sprout → grass reclaims → bushes? → forest?
Abandoned wall → vines grow → cracks form → crumbles?
Unused stockpile → rats? mold? decay?
```

**Reclamation timeline (example):**

```
Day 0: Floor abandoned (no activity)
Day 3: Dust accumulates
Day 7: Weeds in cracks
Day 14: Grass patches
Day 30: Fully overgrown
```

---

## Nature Isn't Just Decay - It's Growth

```
Plant tree → grows over seasons → chop for wood → stump remains → stump rots → grass → new sapling?
```

Forest wants to exist. You clear it, it tries to come back. You *could* fight it forever, or you could designate a forest zone and let it regrow for sustainable wood.

---

## Wild vs Tended

Same underlying nature, different levels of order imposed:

| Wild | Tended |
|------|--------|
| Forest (chaotic, dense) | Orchard (rows, harvestable) |
| Meadow (random grass/flowers) | Farm (crops in plots) |
| Swamp (water + mud + reeds) | Irrigation canal (controlled water) |
| Overgrown ruins | Maintained courtyard |

---

## Reclamation Speed as a Dial

- **Gentle:** grass takes 10 days to cover a path. Cozy, forgiving.
- **Harsh:** vines in 3 days, collapse in a week. Tense, survival.
- **Off:** nothing decays. Sandbox/creative mode.

---

## Water Flows

Water seeks the lowest point.

```
Rain → puddles → flows downhill → pools in depressions → evaporates (slowly)
River → constant flow → can flood banks in wet season
Spring → water source → feeds streams/irrigation
```

**Digging and water:**

```
Dig too deep → hit water table → flooding
Dig channel → redirect water → irrigation or drainage
Dam/wall → block flow → create pond or dry out area
```

**Water states:**

```
Dry → Damp → Puddle → Shallow water → Deep water
         ↓
   Mud (slow movement)
```

---

## Seasons Change

The world breathes on a larger cycle:

| Season | Nature | Colony Impact |
|--------|--------|---------------|
| **Spring** | Growth explosion, rain, snowmelt | Flooding risk, planting time, mud everywhere |
| **Summer** | Full growth, hot, dry spells | Harvest, fire risk?, water management |
| **Autumn** | Decay begins, rain returns | Final harvest, prepare stores, leaves fall |
| **Winter** | Dormant, cold, snow/ice | No farming, heating needed, snow clearing jobs |

**Seasonal loops:**

```
Spring: Plant → Summer: Grow → Autumn: Harvest → Winter: Survive on stores
```

```
Summer: Dry creek bed → Autumn: Rains fill it → Winter: Freezes → Spring: Thaws and floods
```

---

## Maintenance Jobs (Fighting Entropy)

| Job | Fights Against |
|-----|----------------|
| Cleaning | Dirt/mud tracked inside |
| Weeding | Plants invading farms/floors |
| Path repair | Wear from traffic |
| Wall repair | Weathering, cracks |
| Refueling | Torches/furnaces burning out |
| Tool repair | Wear from use |
| Food preservation | Rot and decay |
| Pest control | Rats, insects in stores |
| Snow clearing | Winter blocking paths |
| Drainage | Water pooling where unwanted |
| Pruning | Overgrowth blocking light/paths |

---

## The Entropy Loop

Everything flows in a cycle:

```
       ORDER (colony)
           ↑
    [maintenance jobs]
           |
    ───────┴───────
   ↙               ↘
DECAY              GROWTH
(things break)    (nature spreads)
   ↘               ↙
    ───────┬───────
           |
      [time passes]
           ↓
       ENTROPY (world)
```

The player's job is to keep the colony in the "order" zone. Neglect anything and entropy pulls it back toward wilderness.

---

## Fire Spreads

Fire is entropy accelerated. Destruction in minutes what decay takes months.

**Fire states:**

```
Fuel (wood, grass, cloth) → Burning → Burnt out → Ash/Charcoal
```

**Spread logic:**

```
Burning tile → check neighbors → flammable? → chance to ignite
                                    ↓
                              Wind direction increases spread chance downwind
                              Rain decreases spread chance
                              Stone/water blocks spread
```

**Fire sources:**

- Torches left unattended
- Furnaces without proper flooring
- Lightning strikes
- Cooking accidents
- Arson (raiders? mad colonist?)

**Fighting fire:**

| Job | Effect |
|-----|--------|
| Bucket brigade | Movers haul water to fire, slow but available |
| Firebreak | Deconstruct/clear tiles in fire's path |
| Sand/dirt | Smother small fires |
| Wells/pumps | Faster water source |

**Aftermath:**

```
Burnt wood → charcoal (useful!) or ash (fertilizer?)
Burnt grass → scorched earth → (time) → grass regrows
Burnt wall → damaged wall → (more fire) → rubble
Burnt stockpile → items destroyed (painful lesson)
```

**The loop:**

- Fire is a threat, but also a tool
- Controlled burns clear forest for farming
- Charcoal from wood burning = fuel for smelting
- Ash = fertilizer for farms
- Fireplace = warmth in winter (controlled fire)

**Fire safety as gameplay:**

- Stone floors around furnaces = safe
- Wood floors + torch = risk
- Stockpiles away from fire sources
- Water barrels placed strategically
- Firebreaks between buildings

---

## Temperature

The world has temperature. Colonists feel it. Materials react to it.

**Temperature layers:**

```
Outside temp (weather/season driven)
    ↓
Building insulation (walls block, doors leak)
    ↓
Room temp (modified by heat sources, insulation)
    ↓
Colonist comfort (clothing, activity level)
```

**Heat sources:**

| Source | Heat | Notes |
|--------|------|-------|
| Sun | Ambient | Higher in summer, nothing in winter |
| Torch | Low | Also provides light |
| Fireplace | Medium | Dedicated heating |
| Furnace | High | Industrial, can overheat area |
| Forge | Very high | Smithing, needs ventilation? |
| Bodies | Tiny | Crowded rooms slightly warmer |
| Cooking fire | Medium | Kitchen stays warm |

**Cold sources:**

| Source | Cold | Notes |
|--------|------|-------|
| Winter | Ambient | Baseline drops |
| Wind | Chill | Open areas worse |
| Water | Cold | Near rivers/lakes |
| Underground | Cool | Caves stable temp year-round |
| Ice/snow | Very cold | Stored ice for summer cooling? |

**Temperature effects:**

| Condition | Effect |
|-----------|--------|
| Freezing | Colonists slow, damage over time, frostbite? |
| Cold | Mood penalty, work slower |
| Comfortable | Normal |
| Hot | Mood penalty, work slower |
| Overheating | Colonists slow, heatstroke? |

**Material temperature interactions:**

```
Water + freezing = ice (walkable? slippery?)
Ice + warming = water (spring floods)
Food + cold = preserved longer
Food + hot = spoils faster
Metal + hot = workable (smithing)
```

**Seasonal temperature curve:**

```
        HOT
         │      ╱╲
         │     ╱  ╲
         │    ╱    ╲
COMFORT ─│───╱──────╲───
         │  ╱        ╲
         │ ╱          ╲
         │╱            ╲
        COLD
         Spring Summer Autumn Winter
```

**Temperature jobs:**

| Job | Purpose |
|-----|---------|
| Refuel fireplace | Keep heat going in winter |
| Chop firewood | Stockpile fuel before winter |
| Harvest ice | Store for summer cooling |
| Open/close vents | Regulate airflow |
| Build insulation | Upgrade walls for better heat retention |

**Underground advantage:**

Caves and deep rooms have stable temperature year-round. Not too hot in summer, not too cold in winter. Reason to dig down beyond just mining.

```
Surface: -10°C (winter) to +35°C (summer)
Underground: ~15°C always
```

**The temperature loop:**

```
Summer: Too hot → dig underground rooms, store ice
Autumn: Comfortable → chop wood, prepare
Winter: Too cold → burn wood, stay inside, use stored food
Spring: Warming → floods from snowmelt, plant crops
```

---

## Light and Darkness

The world has light levels. Colonists need to see. Things lurk in the dark?

**Light sources:**

| Source | Radius | Duration | Notes |
|--------|--------|----------|-------|
| Sun | Global | Daytime | Free, reliable |
| Moon | Global (dim) | Nighttime | Enough to not trip |
| Torch (held) | Small | Burns out | Movers carry for tasks |
| Torch (wall) | Medium | Burns out | Needs refueling job |
| Campfire | Medium | Burns out | Also heat source |
| Fireplace | Medium | Burns out | Indoor heat + light |
| Furnace | Large | While working | Bright when smelting |
| Lantern | Medium | Oil burns out | Portable, hangable |
| Glowshroom? | Small | Permanent | Underground natural light |

**Light levels:**

```
Bright (sunlight, furnace) → Normal work
Dim (moonlight, distant torch) → Work slower, miss things
Dark (underground, no source) → Can't work, danger?
```

**Darkness effects:**

| Effect | Consequence |
|--------|-------------|
| Can't work | Most jobs require light |
| Move slower | Can't see where you're going |
| Mood penalty | Fear of dark, unease |
| Miss things | Items harder to find, enemies hidden? |
| Accidents? | Trip, drop items, injury |

**Light jobs:**

| Job | Purpose |
|-----|---------|
| Refuel torch | Keep lights burning |
| Place torches | Expand lit area |
| Harvest fuel | Wood, oil, coal for light sources |
| Recover torch | Grab from wall before expedition |

**Day/night cycle:**

```
Dawn: Light spreads from east
Morning: Full daylight, best work time
Noon: Brightest, hottest (summer)
Afternoon: Still good light
Dusk: Light fades from west
Night: Darkness, torches needed, danger?
Midnight: Darkest, coldest
```

**Underground is always dark:**

No sun reaches caves. You bring light or you bring nothing.

```
Surface at noon: Bright (free)
Cave entrance: Dim (some light bleeds in)
Deep cave: Dark (torches mandatory)
```

**Light + other systems:**

- Fire = light + heat (one fuel, two benefits)
- Underground = stable temp but needs light investment
- Night = rest time, or torch-burning work time
- Seasons affect day length (long summer days, short winter days)

---

## Wind

The air moves. It carries things.

**Wind properties:**

```
Direction: N/S/E/W (shifts over time, weather driven)
Strength: Calm → Breeze → Wind → Strong → Storm
```

**Wind effects:**

| Effect | Consequence |
|--------|-------------|
| Fire spread | Flames jump further downwind |
| Wind chill | Cold feels colder in wind |
| Smoke/smell | Drifts downwind (kitchen smell, forge smoke) |
| Seeds/spores | Plants spread downwind |
| Sound | Carries downwind, muffled upwind |
| Projectiles? | Arrows drift in strong wind |

**Wind and fire:**

```
Calm: Fire spreads equally all directions
Breeze: Slight bias downwind
Wind: Strong bias downwind, can jump gaps
Storm: Fire becomes uncontrollable, sparks fly far
```

**Wind chill:**

```
Actual temp: 0°C
Calm: Feels like 0°C
Breeze: Feels like -2°C
Wind: Feels like -5°C
Storm: Feels like -10°C
```

Walls block wind. Indoor = no wind chill. Outdoor in winter storm = deadly.

**Wind as resource:**

| Use | Mechanism |
|-----|-----------|
| Windmill | Grinding grain, pumping water |
| Ventilation | Clear smoke from forges |
| Drying | Dry meat, herbs, clothes faster |
| Sailing? | If you have water + boats |

**Wind jobs:**

| Job | Purpose |
|-----|---------|
| Operate windmill | When wind is right |
| Close shutters | Protect from storm |
| Secure loose items | Before storm hits |
| Repair wind damage | After storm |

**Windbreaks:**

```
Open field: Full wind
Behind wall: Reduced wind
Behind trees: Reduced wind (but fire risk)
Indoor: No wind
Canyon/valley: Sheltered (but flood risk?)
Hilltop: Maximum wind (good for windmill, bad for comfort)
```

**Seasonal wind patterns:**

```
Spring: Variable, storm season
Summer: Often calm, occasional hot wind
Autumn: Increasing, harvest winds
Winter: Strong, bitter cold winds
```

---

## Sound

The world is not silent. Things make noise. Noise carries information.

**Sound sources:**

| Source | Volume | Meaning |
|--------|--------|---------|
| Footsteps | Quiet | Someone's moving |
| Hammer/anvil | Loud | Smithing happening |
| Sawing | Medium | Woodworking |
| Furnace roar | Medium | Smelting active |
| Chopping | Medium | Tree falling soon |
| Mining | Loud | Digging, echoes underground |
| Water flow | Constant | River, rain, fountain |
| Wind | Variable | Weather indicator |
| Fire crackling | Medium | Cozy or dangerous |
| Alarm bell | Very loud | Emergency |
| Animals | Variable | Livestock, wildlife, pests |
| Machines | Constant | Windmill, pump, engine |
| Silence | - | Something's wrong, or night |

**Sound travels:**

```
Loud sound → carries far → alerts others
Walls block sound → indoor/outdoor separation
Underground → sound echoes, travels through stone
Wind direction → sound carries downwind
```

**Sound effects on colonists:**

| Condition | Effect |
|-----------|--------|
| Quiet workspace | Focus, better work |
| Noisy workspace | Stress, slower work |
| Constant noise | Fatigue over time |
| Sudden loud noise | Startle, interrupt task |
| No sound at all | Unease (too quiet) |

**Sound as information:**

- Hear mining = someone's working
- Hear bell = emergency, drop everything
- Hear nothing from workshop = check on it
- Hear water where there shouldn't be = leak/flood
- Hear crackling = fire somewhere

**Sound jobs?**

Not really jobs, but:
- Ring alarm bell (manual alert)
- Muffle noisy workshop (build sound barriers)
- Place workshop away from sleeping area

**Day/night sound:**

```
Day: Active, noisy, work sounds
Dusk: Quieting down, last tasks
Night: Quiet, crickets?, wind, watch sounds
Dawn: Birds?, waking up, first sounds of work
```

---

## Smell

The air carries more than wind. Things stink. Things smell good. It matters.

**Smell sources:**

| Source | Smell | Range | Notes |
|--------|-------|-------|-------|
| Cooking food | Good | Medium | Draws people to kitchen |
| Rotting food | Bad | Medium | Find it and dispose |
| Corpse | Terrible | Large | Bury or burn, fast |
| Latrine | Bad | Small | Keep away from living areas |
| Smoke | Neutral/warning | Large (wind) | Fire? Or just chimney? |
| Flowers/plants | Good | Small | Mood boost |
| Tannery | Terrible | Large | Isolate this workshop |
| Brewery | Good | Medium | Fermenting grain |
| Forge smoke | Harsh | Medium | Ventilation needed |
| Rain/petrichor | Good | Global | After rain |
| Swamp/stagnant water | Bad | Medium | Disease risk? |
| Fresh bread | Good | Medium | Morale boost |
| Animals/barn | Bad | Medium | Keep downwind |

**Smell travels:**

```
Wind direction matters:
- Tannery upwind = whole colony smells it
- Tannery downwind = only nearby

Walls partially block smell
Underground = contained but concentrated
Rain dampens smells temporarily
```

**Smell effects on colonists:**

| Condition | Effect |
|-----------|--------|
| Good smells | Mood boost, appetite |
| Bad smells | Mood penalty, nausea |
| Terrible smells | Can't work nearby, vomiting? |
| Constant bad smell | Sickness over time? |

**Smell as information:**

- Smell smoke = find the fire
- Smell rot = find the corpse/spoiled food
- Smell cooking = dinner soon
- Smell nothing from kitchen = nobody's cooking

**Smell attracts:**

| Smell | Attracts |
|-------|----------|
| Food cooking | People (good) |
| Rotting food | Rats, insects (bad) |
| Corpse | Scavengers, disease (very bad) |
| Garbage | Pests |
| Flowers | Bees? (good for farming) |

**Smell jobs:**

| Job | Purpose |
|-----|---------|
| Dispose of corpse | Remove terrible smell + disease |
| Clean latrine | Reduce smell radius |
| Bury garbage | Remove pest attractant |
| Ventilate workshop | Clear smoke/fumes |
| Plant flowers | Mask bad smells, mood boost |

**Layout implications:**

```
Good layout:
[Living] ← upwind ← [Kitchen] ← upwind ← [Tannery/Barn]

Bad layout:
[Tannery] → downwind → [Kitchen] → downwind → [Living]
(Everything smells like death)
```

Wind direction matters for colony planning. Seasonal winds = seasonal stink problems?

---

## Sound + Smell + Everything Else

They layer:

```
Hot summer day:
- Sound: Insects buzzing, no wind
- Smell: Everything stronger (heat intensifies smell)
- Tannery unbearable
- Rotting food found faster (stinks more)

Rainy day:
- Sound: Rain on roofs, thunder
- Smell: Petrichor, then dampened smells
- Mud smell if it keeps raining

Winter:
- Sound: Muffled by snow, wind howling
- Smell: Cold dampens everything, smoke more noticeable
- Corpses don't smell (frozen) - danger: forget about them until spring thaw

Fire:
- Sound: Crackling, roaring, people yelling
- Smell: Smoke overwhelming, can't smell anything else
```

---

## Everything Connects

Temperature + Fire + Seasons + Water + Light + Wind:

```
Winter:
- Cold → need fire → need fuel (wood/charcoal)
- Fire = heat + light (two needs, one solution)
- Snow → clear paths job
- Frozen water → no fishing, but can walk on ice
- Food doesn't spoil (natural freezer outside)
- Short days → long nights → more torch fuel needed
- Strong winds → stay inside, wind chill deadly
- Storms → secure buildings, repair damage after

Spring:
- Snow melts → flooding
- Ground thaws → can dig again
- Planting season begins
- Fire risk low (wet)
- Days lengthening → less torch fuel needed
- Variable winds → storm damage, but good for windmills
- Seeds blow in wind → wild plants spread

Summer:
- Hot → need cooling (underground, ice, shade)
- Drought? → water management
- Fire risk HIGH (dry grass, dry wood)
- Food spoils fast → need preservation
- Long days → lots of work time, minimal torch use
- Calm winds → bad for windmill, good for fire control
- Hot wind → heat + fire danger combo

Autumn:
- Cooling → comfortable
- Harvest before frost
- Stockpile wood for winter (fuel for heat AND light)
- Fire risk medium (dry leaves)
- Days shortening → start stockpiling torch fuel
- Winds picking up → last windmill push before winter
- Leaves blow → compost? fire hazard?
```

**A single winter night:**

```
Sunset → darkness falls → light torches
         → temperature drops → stoke fireplace
         → wind picks up → close shutters, stay inside
         → blizzard → can't go outside, hope you have food stored
         → fire burns low → refuel or freeze
         → dawn finally → dig out from snow, assess damage
```

**A summer wildfire:**

```
Dry spell → grass brown, trees stressed
Lightning strike → fire starts
Wind from the east → fire races west
Colonists scramble → bucket brigade, firebreaks
Fire hits river → stops
Aftermath → charcoal salvage, ash for fertilizer, rebuild
Next year → stone buildings, cleared firebreaks, water barrels ready
```

---

## Open Questions

- How visible should decay states be? Subtle grime or obvious overgrowth?
- Should some things be permanent? (Stone floors never get weeds? Or just slower?)
- Pest systems - rats eat food stores, need cats/traps?
- Disease from uncleaned areas?
- Mood effects from dirty/clean environments?
- Can you "win" against entropy or is maintenance always infinite?
- Ruins on the map - pre-decayed structures that nature is already reclaiming?

---

## Summary

The world is alive and doesn't care about your plans. Grass grows, water flows, things rot, tools break. The colony exists by constantly pushing back. Every job is either building order or fighting entropy. There is no "done" - only "maintained."
