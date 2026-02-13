# From Bare Hands to Stone: A Mover's Journey

A narrative demonstration of the progression systems in NavKit, showing how a player moves from nothing to an established colony using the implemented systems.

---

## Paragraph 1: Awakening in the Wild

A lone mover materializes in a dense forest beside a flowing river. With no tools, no shelter, and no supplies, survival begins with pure gathering. They pluck grass from the ground (the vegetation has five growth stages, and tall grass is harvestable), yank saplings from the soft earth, and collect fallen leaves and thin poles from tree branches. They gather sticks and leaves directly from living trees without harming them—the tree harvest system allows two gentle harvests before the tree needs time to recover. A basic hearth can be lit immediately using any of these materials as fuel, producing warmth and ash as a byproduct.

## Paragraph 2: The First Workshop Chain

With a pile of gathered grass drying on a simple drying rack (a passive workshop that works on a timer), the mover focuses on the river's edge. Mining the exposed stone walls with bare hands yields raw granite. At a stonecutter workshop, one chunk of rock becomes two stone blocks—the foundation of permanent construction. Meanwhile, grass finishes drying and is twisted at a rope maker workshop: 4 dried grass becomes 2 short string, and 3 short string braids into 1 cordage. The mover now has binding material. Trees are chopped for logs, and a sawmill processes them: stripping bark first (1 log → 1 stripped log + 2 bark), then sawing the stripped log into 5 planks. The bark feeds back into the rope maker (2 bark → 3 short string), closing the fiber loop.

## Paragraph 3: Fire and Permanence

The real breakthrough comes with controlled fire. A charcoal pit—a semi-passive workshop requiring only brief mover intervention to ignite—converts logs into concentrated charcoal over 60 seconds. This charcoal feeds a kiln that fires clay into durable bricks. Now the mover has three construction materials: stone blocks (from raw stone), brick blocks (from clay and fuel), and wooden planks. Stone floors replace the trampled dirt paths that had been accumulating grime (the floor dirt system tracks movers spreading soil onto constructed surfaces, creating cleaning jobs). Block walls rise around the hearth. The hearth itself burns any fuel continuously, producing ash that accumulates—an ingredient for future chemistry or farming, though for now it simply marks the colony's growing fuel consumption.

## Paragraph 4: The Expanding Colony

With stockpiles filtered to separate fuel (logs, charcoal, peat, leaves, sticks, bark) from building materials (blocks, planks, bricks), hauling jobs flow efficiently. Saplings are replanted in cleared areas to sustain the logging cycle. The charcoal pit creates a self-sufficient fuel loop: logs burn slowly to charcoal, which burns hotter for the kiln, which produces more charcoal from additional logs. Construction jobs pull materials from stockpiles to build multi-room structures—stone walls for durability, wooden floors for warmth, brick hearths for safety. The grid-based world now shows clear zones: a lumber yard near felled trees, a stone quarry by the cliffs, a clay pit by the river, drying racks in the sun, and workshops humming with bills set to "craft until we have 20." The mover is no longer naked and desperate—they're the founder of something permanent.

---

## Systems Showcased

This progression demonstrates:

- **Bare-hands gathering**: grass, saplings, leaves, poles, tree harvesting
- **Mining**: stone extraction without tools
- **Passive workshops**: drying rack (pure passive), charcoal pit (semi-passive)
- **Active workshops**: stonecutter, sawmill, rope maker, kiln, hearth
- **Multi-output recipes**: strip bark (1 log → 1 stripped log + 2 bark)
- **Material chains**: bark/grass → string → cordage; logs → charcoal → fuel loop
- **Construction**: stone blocks, brick blocks, wooden planks
- **Environmental systems**: floor dirt tracking, vegetation growth, tree regeneration
- **Job types**: gather, mine, chop, craft, haul, build, clean, plant
- **Stockpile filters**: separate fuel from building materials
- **Bill modes**: "craft until X" for stockpile maintenance

## What's Not Yet Needed

This story intentionally avoids systems that aren't implemented yet:
- No tool requirements (mining with bare hands works)
- No containers (no water hauling needed)
- No construction staging (walls/floors are single-step)
- No seasoning/curing (materials work immediately)
- No moisture states (no mud/cob crafting)
- No quality/durability systems

The progression uses only what exists in the codebase today.
