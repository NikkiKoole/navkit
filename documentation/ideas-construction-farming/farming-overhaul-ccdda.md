
https://github.com/CleverRaven/Cataclysm-DDA/discussions/49327

I-am-Erk
on Jun 16, 2021
Collaborator
NOTE: This feature is currently written at the concept level, and the implentation details are a WIP. When this is complete, I will copy and paste it into several Issues and combine them into a Project, to suggest how to handle this in a stepwise manner. Despite appearances, this should be appropriate to implement in pieces. This was previously posted as #49302 but it has occurred to me that it's better suited to a master discussion and then multiple issues.

Introduction
Farming presently is a very easy and fast way to get a huge amount of food. It is also very simplistic, not allowing all kinds of believable and fun things like indoor greenhouses, or trying to raise coffee plants from seed because you don't want to live the REST OF YOUR LIFE WITHOUT COFFEE GODDAMMIT.

Thus, farming needs more simulation than just 'plant plant, plant grows'. However, this is a system that can potentially be so complex as to require an entire game of its own: I think we'd do well to consider it from the perspective of balancing verissimilitude and survival simulation with simplicity right from the start, to avoid the occasional overcreep of features and detail that we get.

This is a master proposal for a large project. This project can be implemented in several parts, most of which are individually manageable and for the most part should, once set up, not impede things like Stable release. For example, plants can require watering without adding the Tending system. I recommend we consider moving fairly slowly through the individual implementations, adding support features like zone watering and NPC watering possibly before getting into the grit of Tending plants.

Proposal
This is going to be a long and detailed implementation guideline for a feature that should be spread across multiple PRs. I have provided a table of contents for people that want to jump to particular parts. (With any luck I can get the links working later).

Table of Contents
Concept description
Environmental stats: water, light, and temperature
Tend
Health
Exteriors and soil
Tools for planting and tending
Specific implementation details
Plant elements in JSON
Tracking the environment
Water
Temperature
Infrastructural Needs
Before implementing farming, we need or want the following infrastructural additions:

We should be able to store daily rainfall at the OMT level in a metadata item
Similarly we should track minimum, maximum, and average temperature outdoors in an OMT as metadata, and in individual tiles that have plants on them indoors
Finally, we'll need to do the same with light levels, separate for all of outdoors or individual indoor tiles that have plants on them.
We may want to consider putting plants on a separate mapgen layer of their own instead of mixed with furniture and terrain before implementing this, but that is not strictly a requirement. This implementation could instead create that layer for farm plants, and we could move wild plants there later.
Suggested implementation order (for noooow)
I suggest we implement the components of this project in roughly this order:

Code infrastructure at the OMT level (eg trackign rainfall and temperature, adding a plant layer)
Addition JSON items that can be used later but are just scrap for now - can be done at any time, the sooner the better
Addition of health and thrive stats and the associated hardiness and difficulty tags to existing plants, but vestigial: these will progress gradually and not be impacted by anything during first implementation
Addition of light mechanics as described, including necessary JSON tags for plants
Addition of temperature mechanics as described, including necessary JSON tags for plants
Addition of water mechanics as described, including necessary JSON tags for plants
Shakedown of features, ensure plant JSON is working OK, test systems - given the complexity of tend compared to above I think taking a break here and testing out the rest is a good idea.
Addition of tend mechanics as described
Quality of life features and refinement
Concept description
I would argue that for game purposes, we can give plants five core concepts to adjust. Each of these will have its own secondary details.

water (includes drought_tolerance and overwater_tolerance)
light
temperature (includes dormancy)
tend (includes hardiness)
health (includes growth, `thrive)
At present, this first implementation assumes all plants are annuals that die over the winter. That will naturally occur when they get into weather conditions that kill them, we don't need to program it in. This proposal will cover some of the basic implementations of perennials and things but won't get too complex. A follow up proposal should cover natural spread of plants, so that your garden can get overrun with blackberries, you poor bastard.

Key balance note: The balance intent of this is that if you plant crops outdoors, most plants should be hardy enough to get water from the rain and produce at least some yield with a minimum of tending. However if you tend your plants and water them, you'll get better yields. If you want to grow more delicate crops, they will need more attention, and maybe something like a greenhouse.

Environmental stats: water, light, and temperature
Each crop will get a JSON range of preference for these, eg. "water": [ 0, 100 ] would represent a plant that doesn't care if it's at 0% or 100% water, so it never needs watering at all and doesn't suffer from overwatering. See health for how these would translate to the plant's yield and wellbeing.

Tend
tend is a summary of all the things you might have to do to take care of a plant besides the top three. Weeding, treating bugs or insects, correct spacing or beneficial partner planting, fertilizing, etc. are all features of tend. By abstracting these to a single attribute we can make most of farming/gardening about the avatar's skill with it, and not the player. For example we won't punish players for putting their cucumbers too close together and then getting powdery mildew. Instead, if you don't have the right skills/proficiencies when you plant, you'll get a cucumber whose tend score falls more quickly and therefore requires more constant attention to stay healthy.

There is an important secondary attribute, tend_decay. tend_decay is mostly defined by the plant itself through a new json trait, hardiness, so a less hardy plant with a high tend_decay needs more attention and maintenance, while a plant with low tend_decay is hardy. When you sow seeds, you also make a skill check that can adjust tend_decay, so a skilled farmer still doesn't need to tend their less hardy crops as often, while a rookie might have troubles getting their radishes to grow. (Addendum due to comments below: be aware that 'tending more often' does not mean 'daily' or 'multiple times per day'. The goal is to have farming remain mostly hands-off unless you want to optimize your crop outputs or are growing very tricky plants.)

The speed at which a character increases a plant's tend score should be a function of relevant skills and proficiencies as well as tools. The better a gardener you are and the more equipped, the more easily you can maintain your plants.

Health
A crop's health falls under a number of conditions:

a character tending the crop makes a bad skill check
water, temperature, or light were out of preferred range that day: this would be checked at the end of a 24 hour period and look at total values for the day
tend is low
As a crop's health falls, first its growth slows, then stops, then it dies. Crops with low health at time of harvest produce less. This will be managed with a long term tracking stat, thrive, which moves up and down more slowly than health does and represents how lush and happy your plants are independent of their growth stage (which will also be impacted by health).

A note on overwatering: It should not be possible to overwater your plants manually, for sanity reasons. Overwatering should only be a feature of rainfall. This falls into a design note in tend, that we should assume the avatar can know what they're doing, even if the player doesn't. Thus at the end of the day when we check water status for outdoor plants, we should first see if the plant got enough or too much water from rainfall, and skip checking player-added water at that point unless the plant needs more water. The purpose of overwatering is only to see if the plant is in a biome that doesn't suit it, to stop you from growing cactus easily outdoors.

Exteriors and soil
To simplify matters, when plants are put outside, we should assume all plants in the same OMT experience the same amount of light, the same range of temperatures, and the same amount of rainfall. We can track this on the OMT level throughout the day and apply them to plants in the tile at the end of the day. By doing this, we can take out the role of seasons in planting and instead have plants grow based on seasonal weather conditions.

Soil type, at least for starters, should be split into two types only: ground, or raised beds. Raised beds should have a slightly lower tend_decay score (less weeding eg) but lose water more quickly and perhaps be more susceptible to temperature changes as well. Later, it should be easy enough to extend this system to include different effects from planting in sand, gravel, clay, parched earth, etc.

Tools for planting and tending
There are three key tool categories here, expendable, recoverable, and reusable. Expendables include fertilizers, insecticides, herbicides, etc; recoverable are things that are used while the plant is growing but you get back when it dies (trellises, tomato cages, planters), while reusable are obviously shovels and the like.

When planting a plant, having the right tools can make it go faster (shovels and trowels eg) or can give the plant a better tend_decay score (tomato cage, fertilizer eg). Potentially we might also include things that help the plant with other values like water (retaining soil) or temperature (straw mulch).

When tending a plant, tools can make it go faster (scissors to prune, eg) or can improve outcomes (spray bottle to mist the leaves). Rather than simulating disease, we should have a random chance every time you tend a plant that it could ask for a specific class of tending tool, probably grouped something like: prune, insecticide, fungicide, fertilizer (the options might be defined on the plant so that not every plant would get every problem and not all problems would be of equal severity for all plants). I'm open to other suggestions. If you do not have something in this class, you get a penalty to your tend roll for that day ranging from small to huge, and either have to spend a lot more time with the plant, or let it take a major hit to its health. We wouldn't track anything beyond that... you wouldn't get spider mites on your pepper plants and have to spray them daily for a week, you'd just either deal with them that day or not. We may consider adding at least a little granularity to fertilizer types and having multiple fertilizer needs for plants of different sorts, encouraging a person to have a well stocked garden shed and in turn find themselves with fun and interesting garden chemicals for other uses.

Note here and in other areas, we are modeling a single tile as a 1 sq m farm plot, not a single plant, so if we ask for something like beanpoles or tomato cages, we will need more than one. Requiring several may help players remember that this is not a single plant, which will help a bit when people inevitably ask why it takes ten liters of water to water a single tile.

Specific implementation details
So now that that's done, let's get into the gritty deets. This section is under construction.

Plant elements in JSON
After some discussion with Kevin, I think it is important that we make the contributor-facing JSON not too gritty. I suggest we add the following data to growable plants:

drought_tolerance: none, low, medium, high, unlimited.
overwater_tolerance: none, low, medium, high, unlimited.
frost_tolerance: "yes", "no", or "mandatory". "yes" means the plant can survive frost and just loses health if too cold. "no" means it immediately dies if the temperature drops below 0 celsius. "mandatory" means it has to drop below 0 celsius at least once to bear fruit, and otherwise behaves as if the answer is "yes".
cold_injury_temp: in degrees C, the temperature the plant starts to suffer ill effects from cold. For plants without frost_tolerance, if this value is 0 or lower it is meaningless: we could either throw a json error or just ignore it.
winter_dormant_temp: At this temperature the plant stops losing health, and stops growing and producing fruit, until it warms back up for spring. If the temperature reaches cold_injury_temp it can still lose health but nothing else will bother it. This value should throw an error if it is lower than cold_injury_temp, since the plant would die while dormant.
heat_injury_temp: in degrees C, how hot does it have to be before the plant suffers? This should be quite high in general, most plants can tolerate heat as long as they're watered. We might even allow this to be left out in which case the plant won't suffer heat damage, we'll just represent it with watering, but there are some delicate plant that need to watch out for this.
light: "full sun", "partial sun", "full shade", "dark".
hardiness: This determines the base rate for how much tending the plant requires: high hardiness means it needs less tending. This may need to explode into susceptibilities for a few common ailments, we shall see how that pans out in numbers. It also determines how much health damage the plant suffers from bad conditions.
difficulty: This determines how much your gardening skill will affect hardiness of the plant at planting and when tending. If a plant has a high difficulty, it will require more skill to tend properly, and it will suffer more damage from ill effects.
We will also need to adjust the existing information on growth times to include how long it takes to get a second harvest from plants that can continue to yield plants, as well as how long ripe plants can remain unharvested before rotting in the field.

At some point I will add edits to suggest what these values translate to in numbers. For now I hope that gives a clear picture of what we're looking for.

Tracking the environment
water, temperature, and light should be global: that is, they should not vary depending on the plant or the character's skill.

Water
Water ranges from 0 to 100, representing saturation of the tile, where 100 is completely soaked muddy earth. We could also include a value -1 for planting immersed in water, allowing rice paddies and things. Raised beds lose moisture quicker, and potentially things like mulch or retaining soil could reduce this rate. This can be influenced by temperature to allow hot weather to dry plants faster.

Note that most crops that can grow outdoors in new england should have a desired_water upper limit of 90-100. Under 100 means that they don't like heavy rain for days on end, but can survive a few days of heavy rain.

Every 24 hours we should perform an assessment on watering status. Note that we do not update the water status at the time the plant is watered! (However we should remove any warning messages about your plants needing water once you've watered them.) The algorithm should look like this:

Drop the water level of the plant: water_level_current = water_level_previous - ( [ 75 if in raised bed || 50 if in soil ] * [ today_temperature^2 / 500, min 0 ] ), would be a decent basic formula. Under this formula, plants in the ground at 20 degrees C (cool room temperature) will lose around 40% of their water, while at 30 degrees C (quite hot out) will lose their water in a single day - this does not include regaining water from rain! This does not figure in mulch, retaining soil, etc - those should be worked out later, not on a first implementation. note: For outdoor plants only, although the final value for water should range from 0 to 100, at this point we should track if the number goes below 0 for use in step 3. That won't be necessary for indoor plants since we don't track rainfall for them.
Is the plant indoors or outdoors? If indoors, go to 3. Otherwise go to 2.
For outdoors plants, check how much rainfall there has been in the OMT over the past 24 hours and add that amount to its current water status. Increase water_level_current by todays_rainfall, so a rainfall of 100mm (a very rainy spring day) would put it from parched to 100%. Remember that the maximum is 100. In step 1, we kept potential water deficit through the day due to the weather being hot, and so that deficit must first be paid off by the rain before we top up the water level (this is to account for more of that rain evapourating on very hot days). Note: Tracking rainfall on the OMT level should be its own separate issue but ultimately we should get values in mm/day ranging from 0 to around 200.
Check the current water status compared to the plant's overwater_tolerance. If water is at or above the maximum tolerated level, do no more checks. Leave the plant happy until tomorrow. This ensures that overwatering is only the result of rain, not player activity.
If the current water status is below the plant's overwater_tolerance, and it was watered that day, bring its water status up to the max.
OPTIONAL:
Save the amount of added water needed as daily_watering. The next time the plant is watered, the number of units of water it expends should be equal to either 6 liters (the default) or daily_watering/2. This isn't a deficit value, it doesn't build up over time or anything, it's just there to see how much water the plant needs roughly and appropriately use water resources from there. These numbers are high because one tile of crops is a square meter of ground with multiple plants in it: if we later want to model individual plants in pots, we could reduce both the yield and the water requirements, but I am just writing for raised beds or farm plots here.
I am personally OK with forgoing this and just having a plot take 6L of water per day if they need watering at all, regardless of season, rainfall, and temperature.
If water is above drought_tolerance or below overwater_tolerance, impact plant health. I will add another section for daily health calculations when I get there.
Implementation QoL notes
Watering a single tile will require many liters of water. For this reason, the watering action will need to understand automatically going back to a water source and getting more water. Watering should be available as a zone action as soon as possible. In the future we should obviously look into automatic irrigation systems both primitive and technological.

Temperature
For temperature, we track four subfactors: frost_tolerance, which is pretty much a boolean, cold_injury_temp, winter_dormant_temp, and heat_injury_temp.

Throughout the day, we should track two variables through the OMT for outdoor plants, or per tile for indoor plants:

frost: Did the tile reach freezing temperatures at all today?
temp_avg: The average temperature of the tile/OMT through the 24h span
I do not know enough about our temperature system but I suspect there will be some ways to simplify how these are calculated. I am going to leave that for a separate issue for now and take it for granted that we can calculate those values. Each 24h we do the following checks:
Check if frost is true. If it did, check if the plant has a value of "no" for frost_tolerance, and kill it if it does. If not, check if it has a value of "mandatory", and if it does, add a flag to the plant allowing it to bear fruit when ready, and also telling it to skip this step in the future.
Check if the plant has a winter_dormant_temp, if true go to 3, if false go to 4.
Check temp_avg > winter_dormant_temp. If true, go to 4. If false, make the plant dormant and switch to the dormant section.
Check temp_avg > cold_injury_temp. If true, go to 5. If false, damage the plant due to cold - see upcoming health section for damage.
Check temp_avg > heat_injury_temp. If false, no further calculations needed. The plant stays nice and healthy. If true, damage the plant due to cold - see upcoming health section for damage.
At some point we might consider frost blankets and mulch as ways of reducing the cold temperatures the plant experiences. That can wait for a later implementation.

Dormancy
A quick note on dormancy. This concept will need expansion at a later date but should not be part of first implementation beyond basics.

Once a plant is dormant, we should stop checking daily temperature and start checking it on a weekly basis, checking only if the temp is above either the dormant_temp or the cold_injury_temp. If below cold_injury it takes damage as usual in health (this means it will suffer less quickly because we're only checking weekly, don't multiply by 7 or anything). If above dormant_temp, we should wake the plant up and resume normal calculations. A newly awoken plant should start at minimum growth, as if freshly planted.

Later on, we can look into more nuanced changes.

Light
I will need to get some more information on how light is measured in-game to write this one up. It should be the most basic of the three though, we just need to see what the plant's average light level was throughout the day and if that represents enough for the shade level it likes.

Tending plants
A plant has a base hardiness level determined in its json, as well as difficulty.

Unlike the other variables, tending is skill based. At present, we don't have a single skill that would cover gardening. We have two options here as I see it:

Define what skill(s) are used for a plant in that plant's JSON. Medicinal herbs would be healthcare, wild plants are survival, edibles are food handling, complex botanicals are applied science. We can allow multiple skills per plant in this scenario. It has the advantage of flexibility and using existing skills, but the disadvantage of needing a lot of JSON support per plant.
Add a new skill botany, which will also probably have some effect on the wild plant harvesting portion of Survival.
We can discuss which of these options we like best, I will at this point go on as if we've made a decision. Either way, we will want some specific gardening proficiencies, which I will list here once I have determined what they should be.

Planting
Health and Thrive
As discussed in many places above, health is a derived value that rises and falls based on how far out of bounds the other trackers are for a plant. It connects to two subsequent attributes. The first is thrive. This is an ongoing slow tracker of how often the plant has been unhealthy or healthy, and the higher it is, the better the plant's yields will be. The second is growth, which is simply a timer determining when the plant reaches maturity.

Describe alternatives you've considered
We've looked at more granular systems with weeds and plant diseases, but I am not at all convinced they add anything to just keeping this abstract.
The current system is far too easy to produce food, but some of this could be simulated just with a skill check at the time of planting crops and having them then produce based on that. To that I say: 'meh'.

Additional context
Eventually this same system should be expanded to include native wild plants, making dandelions and burdock and other flowers and shrubs into crops under the same ruleset. That would allow garden bushes that don't live well in new england to naturally die out over time, and would also allow wild plants to grow and spread appropriately. As well, as the apocalypse progresses and weather patterns change, we'd see natural die-offs of plants that no longer suited their environment.

Once this is the case there is a concern that when you load a previously unvisited OMT, and we have to run through the growth and death of every plant in that OMT for several months, there could be an overhead cost. We'll need to test that, because the calculations here aren't that complex so it may not be a big deal. If it is, we may want to calculate a cache of what mapgen-placed plants will do over time without intervention, so that we don't have to run through months of simulation of plant life every time you load a new overmap tile later in the apocalypse. Instead we look at the cache and say 'it's been x months, load the automatic state for a grape after x months'. We'd pick a timeframe based on lag. I'm only writing this here to remember... managing pre-existing plants will need to be another PR that discusses spread and things.
