> ok, so lets get all the way to the basics, currenlty i just have a few tiles in my world (grass/floor/wall) and ive made a sort of mine the walls, build walsl, i want to know the next few steps, what sort of terrain should i add to support more jobs ?

If you want “the next few steps” with the smallest set of new terrain that unlocks lots of stone-age jobs, add terrain that creates resource loops. Right now you mostly have “dig/build”. Next you want: food, fuel, materials, water.

Here’s a good minimal roadmap (in order), with what each terrain enables.

1) Water (river / pond / shallow water)

Why: instantly creates survival + logistics.
Unlocks jobs: Fisher, Water Carrier, Cook, Builder (bridges/paths), Guard (watch crossings)
Gameplay loops: thirst / cooking / fish / pathing constraints (can’t walk deep water)

2) Trees / Forest tiles (or “tree objects” on grass)

Why: wood is the first real building + tool material.
Unlocks jobs: Woodcutter, Carpenter, Firekeeper (fuel/charcoal later), Palisade Builder
Loop: chop → haul logs → build → burn for fuel

3) “Wild plants” layer (berries/reeds/herbs as harvestables)

Keep grass, but add harvestable objects that spawn on it:

Berry bushes (food)

Reeds (near water, for thatch/rope)

Herbs (medicine/dyes)
Unlocks jobs: Gatherer/Forager, Weaver (cordage), Medic/Shaman, Cook
Loop: gather → eat/cook → heal → craft ropes/nets

4) Soil types / fertility (dirt, rich soil, rocky soil)

Even if you don’t add farming yet, soil affects digging/building.
Unlocks jobs: Earthworker (ditches/pits), Builder (wattle/daub), Farmer later
Loop: choose settlement location based on soil + water + trees

5) Stone variety + loose stone (boulders / scree)

Instead of only “wall”, add:

Stone wall (mined)

Loose stone tile/object (collected without mining)
Unlocks jobs: Quarry Worker, Knapping/Toolmaker, Dry-stone Mason
Loop: collect loose stone early → mine later → upgrade buildings/tools

6) Clay deposit (often near water)

Could be a tile (clay ground) or a diggable node.
Unlocks jobs: Clay Digger, Potter, Builder (daub/plaster-ish)
Loop: clay → pots → better storage → longer survival / stockpiles matter

7) Sand / Gravel (optional but great)

Unlocks jobs: Sand/Gravel Gatherer, Potter (temper), Builder (drainage/paths)
Loop: better pottery + better paths

The “tight” terrain set that gives you the most jobs fast

If you only add 5 things next, do:

Water

Trees

Berry bushes

Stone (loose + mineable)

Clay

That already supports: gather/hunt/fish, cooking, hauling/storage, toolmaking, building upgrades, and early industry.

Implementation tip (saves you from tile explosion)

Treat a tile as:

base terrain (grass, dirt, stone, water)

plus features (tree, bush, reed, boulder)

plus material/tags (flammable, edible, mineable, fertile, clay)

This way you can add “jobs” by checking tags, not adding 50 new tile types.

If you tell me whether you want farming soon (or keep it purely hunter-gatherer for a while), I can suggest the next terrain set after this and a simple production chain order.


> well i kinda would want to go farmer first then hunter. but the very first would be gatherer. its just i dont really feel like starting about animals and hubting yet

Cool — you can do a really clean progression: Gatherer → Early farmer → (much later) hunter.

If you want farming without touching animals yet, add terrain/features that make “plant loops” interesting:

Next terrain/features to add (farming-first)

Dirt / Soil tiles (at least 2 fertility levels)

Soil (poor) and Soil (rich)

Lets players care about settlement location and field placement.

Jobs: Digger/Earthworker, Farmer

Fresh water (river/pond) + “moisture” concept

Don’t overcomplicate: fields within X tiles of water get a growth bonus, or you need Water Carrier to irrigate.

Jobs: Water Carrier, Builder (irrigation ditch)

Wild gatherable plants

Berry bushes and wild grain patches (or “edible roots”)

This is your “first food” before farms stabilize.

Jobs: Gatherer, Hauler, Cook (optional)

Trees / brush

Farming needs wood: fences, tools, storage, fuel.

Jobs: Woodcutter, Builder, Firekeeper (if you do cooking)

Stone/Clay (for storage)

Clay → pots / jars = makes farming pay off (storage for surplus)

Jobs: Clay Digger, Potter

Farming jobs (minimal set)

Gatherer (starts the colony)

Farmer (plant/harvest)

Water Carrier / Irrigator (if not near water)

Builder (fields, fences, ditches)

Woodcutter (fuel + building)

Hauler (moves seeds/harvest to storage)

Cook (optional: turns grain into meals)

Simple “first crops” (no animals needed)

Pick 2–3 with different timings:

Fast crop: berries / peas / leafy greens (quick food)

Staple: grain (slow but high yield, stores well)

Risk crop: tubers (needs digging, but hardy)

Tiny mechanics that make it feel like a DF-like loop

Seeds: harvesting returns food + some seeds; planting consumes seeds.

Season/growth timer: even a simple “grows in N days” works.

Storage spoilage: raw plants spoil faster; grain stores longer; pots improve storage.

Order I’d implement (fast payoff)

Add Soil fertility

Add Water

Add Wild plants

Add Planting/harvesting + seeds

Add Storage (granary / clay pots)

Add Irrigation ditches (optional upgrade)

If you want, tell me whether your world is top-down or side-view, and whether you already have time/seasons. I can suggest the smallest set of rules for crop growth + irrigation that won’t balloon your code.

> its topdown, and i dont have time/seasons yet, just ticks , there is no day/night or seasons yet, that will not be doen quickly either i think.

Thought for 15s

Top-down + “just ticks” is totally fine for farming. You can make a satisfying Gatherer → Farmer loop without day/night or seasons by treating ticks as abstract time and using growth stages.

Terrain/features to add next (minimal but high impact)

Soil tiles (2–3 types)

Dirt (normal)

Rich soil (faster growth / higher yield)

optional: Rocky soil (can’t farm / slower)

Water tiles

Shallow water (walkable or slow)

Deep water (not walkable)

Bonus later: “moisture” spreads from water to nearby soil.

Wild plants (as features on tiles, not new tile types)

Berry bush (instant early food)

Wild grain patch (gives food + seeds)

optional: Herb plant (medicine)

Trees (feature on grass/dirt)

Mainly to support storage/building/fences later.

That set unlocks a bunch of jobs without animals: Gatherer, Farmer, Water carrier, Builder, Hauler, Cook (optional).

Core farming system using ticks (simple state machine)

Treat each farm tile as having a field state:

Untilled → Tilled → Planted(seedType) → Growing(stage 0..N) → Ready → Empty

Each planted tile stores:

planted_tick

growth_stage

moisture (optional)

crop_type

Growth update (per tick or every X ticks):

If moisture >= min then progress grows

Rich soil adds a multiplier (ex: +25% growth speed)

When tick - planted_tick >= grow_time, set Ready

No seasons needed. Later you can map “days” to “ticks per day” without rewriting logic.

Jobs and tasks (what your dwarves do)
Gatherer

Harvests wild plants (berries/grain/herbs)

Brings to stockpile

Farmer

Tills designated farm plots

Plants seeds

Harvests ready crops

Water Carrier (optional early, great gameplay)

If a farm tile is “dry”, deliver water to raise moisture

Makes settlement placement matter (near water = less labor)

Builder (farming-adjacent)

Builds fences (stops trampling later / marks plots)

Builds irrigation ditches (simple moisture spread)

Hauler

Moves seeds/food to storage (so farmers don’t waste time walking)

The “next few steps” I’d implement in order

Food items + stockpile (even a simple “pile” works)

Wild plants (gatherer loop is your start)

Seeds as items (wild grain drops seeds)

Farm plot designation (player paints an area)

Tilling/Planting/Harvesting tasks

Storage/spoilage (lightweight)

Fresh food spoils faster; grain slower; pots later slow spoilage more

Moisture/irrigation (adjacent-to-water bonus is enough at first)

Tiny mechanics that make it feel “DF-like” fast

Seeds are required to plant (no infinite planting)

Harvest returns: food + some seeds (so farming can bootstrap)

Field work has priorities: harvest > plant > till

Distance matters: hauling time makes storage placement meaningful

If you want, I can sketch a really small “task generator” rule set (how tiles become jobs each tick) that won’t spiral into a huge system.


> hey and, and overarching indea in this game of mine, entropy, how does it relate ?

Entropy is a perfect “overarching idea” for a DF-like, because it naturally creates pressure over time: everything tends to decay, drift, get messy, and require energy/labor to keep ordered.

In game terms, entropy = maintenance + decay + loss.

How entropy can show up (stone-age-friendly)
1) Food entropy (spoilage)

Gathered food rots fast.

Stored grain lasts longer.

Pots/granaries slow rot.
Meaning: farming isn’t just “make more food”, it’s “fight spoilage with storage + labor.”

2) Tool entropy (wear)

Stone tools dull/break.

Farming consumes tools faster than gathering.

You need knappers/toolmakers to keep the economy running.
Meaning: you’re not progressing to “better”, you’re building a system that resists falling apart.

3) Soil entropy (fertility depletion)

Repeated planting drains fertility.

Leaving fallow restores slowly.

Compost/mulch (later) restores faster.
Meaning: the land “runs down” unless you rotate/expand—very stone-age, very systemic.

4) Structure entropy (decay + collapse risk)

Wood rots, mud walls erode, supports fail if you dig too much.

Repairs become a real job.
Meaning: a settlement is a living thing you maintain, not a static base.

5) Cleanliness/health entropy (optional later)

Waste piles up → sickness risk.

Water sources can get contaminated.
Meaning: growth creates its own problems.

The big relationship (why it works)

Entropy turns your game into a loop of:
produce → store → maintain → repair → prevent loss
Instead of “snowball forever”.

That’s exactly what makes DF-likes interesting: you’re managing a system that’s always trying to slide toward failure, and your people are the constant input of “energy” that pushes back.

Easiest way to implement it without complexity

Pick one entropy meter per system and make it tick up/down:

Item freshness: value decreases each tick → becomes “spoiled”

Durability: tools/buildings lose durability when used / over time

Fertility: soil decreases when harvested, recovers when fallow

Start with just these 3 and you’ll already feel the theme.

If you tell me what your core “player goal” is (survive X ticks, build a monument, reach population Y, etc.), I can suggest the best entropy systems to reinforce that goal without adding too much code.


> ok back to the jobs in the stone age, im missing things related to fire i think ?

Yeah—fire is a big stone-age pillar, and it can create great early production chains. Here are job ideas that stay “basic” but still give you interesting gameplay:

Core fire jobs

Firekeeper — keeps hearths/torches lit, refuels, relights when out
Tasks: add fuel, move embers, maintain “lit” status in key buildings

Fuel Gatherer — collects sticks/branches/dung/peat (whatever fits your map)
Tasks: harvest deadwood, gather brush, haul to fuel stockpile

Charcoal Burner (later in your stone age) — turns wood into charcoal in a pit/kiln
Why it matters: higher energy fuel for better crafting

Fire-enabled crafts (good early progression)

Cook / Smoker — cooks meals, smokes/drys food for longer storage
Fire link: requires a lit hearth; smoked food spoils slower

Potter / Kiln Worker — fires clay pots/bricks
Fire link: needs sustained heat + fuel; creates storage upgrade loop

Torchmaker — makes torches (stick + resin/fiber) for light / exploration
Fire link: consumes fuel/resin; makes caves/mines usable

“Fire-related” support jobs (simple but adds depth)

Ash Keeper — collects ash from fires (useful later for lye/cleaning/soil)

Resin Collector — gathers pitch/resin from certain trees (torches, waterproofing)

Fire Watch — prevents wildfire; extinguishes spread (if you add fire propagation)

Minimal set to implement first

If you want the smallest set that feels real:

Firekeeper (maintain lit fires)

Fuel Gatherer (supply)

Cook/Smoker (pays off with food)

Potter/Kiln Worker (pays off with storage)

Tiny mechanic that makes fire “matter”

Give fires a simple state:

lit / out

fuel value decreases per tick

buildings/recipes require lit and consume fuel

If you want, tell me what resources you currently have as items (wood? stone? fiber?), and I’ll map a clean early “fire tech tree” from your current tiles to pottery + charcoal without adding animals.

> we dont have any resources todo with fire at the moment, just wall/floor/grass but i wantto add some cells ?

Yep—if you want “fire gameplay” but you currently only have wall / floor / grass, the missing piece is a fuel source plus a place fire can exist. The cleanest way is to add just a handful of new cell types (or cell + feature) that unlock the whole fire loop.

Minimum new cells to support fire
Resource / environment cells

Tree (feature on grass, or its own cell)

Gives wood/fuel when chopped.

Unlocks: Woodcutter → Fuel gatherer → Firekeeper.

Brush / Deadwood (feature that spawns on grass/forest)

Early fuel without “logging industry”.

Unlocks: Gatherer/Fuel gatherer right away.

Water (pond/river)

Doesn’t have to do much yet, but later it’s your “fire control” + drinking.

Unlocks: Water carrier, extinguishing, irrigation later.

Dirt (or “bare ground”)

Useful as a simple firebreak (fire doesn’t spread on dirt).

Fire cells (actual fire system)

Hearth / Firepit (buildable cell)

Has fuel and lit state.

Consumes fuel per tick.

Enables: Cook/Smoker, warmth/light, morale.

Burning (overlay state on a cell, not a new terrain)

Any flammable cell/feature can become burning.

On tick: consumes its own “fuel”, may spread to neighbors, eventually becomes…

Ash / Charred ground (result state)

Simple end state after burning.

Later can be used for crafting/fertilizer/cleaning if you want.

That’s honestly enough to make fire feel real.

Next “upgrade” cells (when you want progression)

Kiln (buildable)

Consumes fuel → fires clay into pots/bricks.

This is your “fire turns into technology” moment.

Charcoal pit (buildable)

Wood → charcoal (better fuel)

Unlocks higher-heat crafts later.

How to keep it simple (so you don’t add 50 tile types)

Give each cell/feature a few tags:

flammable: bool

fuel_value: int

burn_time: int

spread_chance: float (or just “spreads if neighbor is burning”)

blocks_movement / blocks_sight

Then fire is just a state machine:
normal → burning → ash

The first fire-related jobs this unlocks

Fuel Gatherer (sticks/deadwood)

Woodcutter (trees → wood)

Firekeeper (refuel/relight hearths)

Cook (requires lit hearth)

If you implement only one new buildable: Firepit, and only one new resource source: Tree or Deadwood, you’ll immediately have a working fire loop.

If you tell me whether your “cells” are strictly one tile type each, or you can do “tile + feature overlay”, I can recommend the cleanest way to represent tree vs deadwood vs grass without exploding your tile list.

> I can do tile + overlay sure

Perfect — tile + overlay is exactly what you want here. You can keep your base tiles basically the same and add a few overlays that unlock fire + jobs.

Base tiles (keep simple)

Grass

Floor (built)

Wall (solid)

Water (new)

Dirt (new, optional but great firebreak + farming later)

Overlays to add (these drive jobs/resources)
Fuel / nature overlays

Tree

On: grass (or dirt)

States: standing → felled (log item) → stump (optional)

Jobs: Woodcutter, Hauler

Brush / Deadwood

On: grass/dirt

Quick early fuel without full trees

Jobs: Fuel gatherer, Hauler

Reeds (optional, on water edge)

Early thatch/rope later + also burns fast

Jobs: Gatherer/Weaver

Fire overlays (the actual system)

Firepit (built overlay)

Placeable on floor/ground

Has: fuel_amount, lit

Jobs: Firekeeper, Cook

Burning (temporary overlay)

Can exist on top of Tree/Brush/Floor etc.

Has: burn_ticks_left

Each tick: consumes itself, may spread to neighbors if they’re flammable

Ash/Charred

Result overlay after burning

Optional later use: fertilizer/lye/etc.

Minimal “fire rules” (feels good, not complex)

Anything with flammable=true can catch fire.

Burning decreases burn_ticks_left.

When it hits 0: remove flammable overlay, replace with Ash.

Spread rule (simple): each tick, burning tries to ignite 1 random neighbor flammable tile.

Minimal resource items (just 2 to start)

Fuel (generic “wood/brush” item)

Food (berries now, crops later)

Firepit consumes Fuel per tick to stay lit. Cooking can be “convert raw food → meal” at the firepit.

Jobs you unlock immediately

Fuel Gatherer: collect Brush/Deadwood → Fuel stockpile

Woodcutter: cut Trees → logs/fuel

Hauler: move fuel/food to stockpiles

Firekeeper: keep firepits lit (refuel/relight)

Cook (optional): cook at lit firepit

Suggested next 3 steps after fire works

Add Clay patch base tile or overlay → unlock Potter/Kiln later

Add Farm plot overlay on dirt → seeds/growth stages

Add Smoke rack overlay (super simple) → preserves food (slower spoilage)

If you want, I can propose a tiny data schema for overlays (fields you need) and a tick update order that avoids bugs like “fire spreads forever” or “jobs spam every tick.”
