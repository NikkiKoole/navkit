# Edible Plants + Terrain Food Plan (Consolidated)

This doc gathers all edible-plant notes from `docs/todo` and `docs/vision` into one place, with a terrain-first plan for bushes, fruit trees, and simple foraging food.

**Sources**
- `docs/todo/nature/wild-plants.md`
- `docs/todo/nature/brush-bushes.md`
- `docs/todo/nature/moisture-irrigation.md`
- `docs/todo/vision-extracted-todos.md`
- `docs/vision/entropy-and-simulation/entropy.md`
- `docs/vision/freetime.md`
- `docs/vision/stone-aged-dwarfjobs.md`
- `docs/vision/terrain-and-tiles/soils.md`
- `docs/vision/farming/crops-ccdda.md`
- `docs/vision/farming/crops-df.md`

**Key Edible Plant Concepts (Current Vision)**
- Wild plants are an overlay on grass tiles and produce food items and seeds when gathered.
- Brush/bushes spawn on grass and can be gathered for berries or other food items, with fast regrowth.
- Berry bushes are an explicit early food source: harvest to `ITEM_BERRIES`, regrow over time.
- Fruit trees are planned as a tree variant that periodically yields fruit, either picked from the tree or dropped to the ground.
- Foraging loop is intended to be very simple at first: food grows in the world, movers pick and eat it, hunger is restored.
- Gatherer is a core early job, tied to `canGather` capability and `DESIGNATION_GATHER_PLANT` for wild plants.

**Entropy-Driven Terrain Rules (Very Important)**
- Grass is a state of dirt, not a separate material. Dirt left alone returns to grass over time.
- Abandoned outdoor areas reclaim naturally: dirt to grass to weeds to bushes, eventually forest.
- Growth and decay are seasonal. Spring growth explosion and winter dormancy are key to food loops.
- Seeds can spread by wind. This is the natural propagation mechanism for wild plants.
- Maintenance loops are endless by design. Food rots, weeds return, and harvesting is recurring.

**Terrain Integration Hooks**
- Wild plants should be a surface overlay like grass, so they can coexist with dirt and soil types.
- Bushes should spawn on grass tiles, like saplings but smaller, and regrow on a short timer.
- Moisture tag on tiles near water should speed plant growth and regrowth.
- Soil fertility should influence plant growth speed and yield.
- Light should affect plant growth speed and survival, especially for crops later.
- Reclamation should allow unused outdoor tiles to progress from dirt to grass to weeds to bushes.
- Wind should occasionally seed adjacent tiles with wild plant overlays.

**Minimal Edible Set (Early Implementation)**
- Wild berries from bushes as the first food item type.
- Wild herbs and wild grains as optional second and third foraging items.
- Fruit trees as a second food source, with fruit on tree or dropped on ground.
- Simple items for now: `ITEM_FOOD` and `ITEM_BERRIES`, or `ITEM_FRUIT` with a material tag.

**Foraging and Eating Loop (From Freetime)**
- Food grows in the world, movers pick it up or harvest, then eat.
- If hungry, a mover searches for the nearest food on ground, bush, or tree.
- Eating restores hunger by a fixed nutrition value per item.
- Food can be stockpiled for later, enabling winter survival and planned harvesting.

**Candidate Plant Lists (Curated Sources)**

CCDA Crop Tiers (Good early shortlist for edible plants)
- Barley, B, flour and booze
- Blackberry, A, berries
- Blueberry, A, berries
- Broccoli, B, basic vegetable
- Buckwheat, A, flour, non-rotting
- Cabbage, A, many recipes, keeps well
- Cannabis, A, plant fiber and seed oil
- Canola, C, oil, lower priority than cotton
- Carrot, B, common recipes
- Celery, B, common recipes
- Chilli Pepper, F, low utility
- Corn, S, cornmeal and sugar
- Cotton, A, fiber and seed oil
- Cranberry, A, berries
- Cucumber, B, pickles
- Dandelion, F, low nutrition
- Garlic, A, non-rotting, common recipes
- Hops, F, niche use
- Lettuce, B, low keeping
- Oats, S, oatmeal and flour, non-rotting
- Onion, B, keeps well
- Potato, B, recipes and booze
- Pumpkin, C, edible seeds, low output
- Raspberry, A, berries
- Rhubarb, C, sugar
- Strawberry, A, berries
- Sugar Beet, A, sugar
- Sunflower, C, edible seeds and oil
- Tomato, A, versatile
- Wheat, A, flour and straw
- Wild Herb, A, mass herbs
- Wild Vegetable, B, common
- Zucchini, C, fast but low keeping

CCDA Medicinal Plants (Not food, but edible-adjacent harvestables)
- Bee Balm
- Datura
- Dogbane
- Mugwort
- Thyme
- Poppy

DF Crop References (Large source list, use as long-term candidates)
- Aboveground berries and shrubs: prickle berry, strawberry, fisher berry, sun berry, cranberry, bilberry, blueberry, blackberry, raspberry, passion fruit, grape.
- Garden plants with edible growths: asparagus, bitter melon, cucumber, eggplant, horned melon, muskmelon, pepper, squash, tomato, tomatillo, watermelon, winter melon.
- Roots and tubers: beet, garlic, onion, parsnip, potato, radish, sweet potato, taro, turnip, yams.
- Grains and flours: wheat, oats, barley, buckwheat, rice, maize, millet, sorghum, quinoa.
- Full reference is in `docs/vision/farming/crops-df.md`.

**Terrain Placement and Growth Plan**
1. Add `ITEM_FOOD` and `canGather`, plus `DESIGNATION_GATHER_PLANT` for wild plants.
2. Implement wild plant overlay on grass, with regrowth timers and gather jobs.
3. Add brush/bush entities on grass tiles that yield berries and regrow quickly.
4. Add fruit tree variants and fruit drop or pick mechanics for ground foraging.
5. Add moisture tagging near water and use it as a growth speed modifier.
6. Add soil fertility grid and light grid to scale growth rate and yield.
7. Tie growth and regrowth to seasons, with winter dormancy and spring surge.
8. Add wind-based seed spread to seed wild plants into nearby tiles.

**Open Questions for Planning**
- Should fruit trees be separate cell types or a tree material variant with a fruiting state?
- Do bushes and wild plants share a single overlay system, or are bushes separate cells?
- Do we want early plants to be seasonless for simplicity, then add seasons later?
- Is the first edible item `ITEM_FOOD`, `ITEM_BERRIES`, or `ITEM_FRUIT` with materials?
- How fast should reclamation advance from weeds to bushes in outdoor unused areas?
