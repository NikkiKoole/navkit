# Workshop Evolution: From Templates to Stations

## The Tension

We have two competing design directions for how "production" works in the game:

**Current system (DF-style):** Workshops are monolithic entities defined by ASCII templates. A Stonecutter is a 3x3 block with fixed tile roles (work, output, fuel). The workshop *is* the thing — it holds bills, assigns crafters, owns recipes.

**Future vision (CDDA-style):** From `docs/doing/discussions with ai/notdfworkshops.md` — tools are the boss, not workshops. An anvil + a forge near each other = blacksmithing. No predefined template. A "workshop" is just "a collection of tools near each other."

Both have merit. This document maps out where we are, where the cracks will appear, and a migration path that doesn't require rewriting everything.

---

## What We Have Now

### The Good

The current system (`src/entities/workshops.h`, `src/entities/workshops.c`, `src/entities/jobs.c`) is well-structured and internally consistent:

- **7 workshop types:** Stonecutter, Sawmill, Kiln, Charcoal Pit, Hearth, Drying Rack, Rope Maker
- **Recipe struct** is flexible: dual inputs, dual outputs, material matching (any/exact/wood/stone/metal), passive/active/semi-passive modes, fuel requirements
- **10-step craft state machine** (CRAFT_STEP_MOVING_TO_INPUT through CRAFT_STEP_CARRYING_FUEL) handles all combinations of 1-2 inputs + optional fuel
- **Bill system** with DO_X_TIMES / DO_UNTIL_X / DO_FOREVER, auto-suspend on full storage
- **WorkshopDef table** makes adding a new workshop type mostly data-driven: enum + template + recipe array + def entry

Adding a new primitive workshop (e.g., a Tanning Rack or a Loom) is easy. Define recipes, write a template, add a def. The state machine handles the rest.

### Where It Works Best

For **Era 1-2** content (Survival → Community), the template system is a natural fit:

- A stonecutter IS a fixed physical thing. You wouldn't freeform-assemble one
- A drying rack IS a single piece of furniture
- A charcoal pit IS a specific arrangement of dirt and wood
- At 5-10 colonists, you place maybe 3-5 workshops. Managing them as discrete entities is fine

### The Assumptions Baked In

These are the constraints that will cause friction as the game scales:

1. **One work tile per workshop.** A crafter stands at exactly one spot. Can't have two crafters at one workshop, or a crafter moving between multiple stations within a workspace
2. **One output tile per workshop.** Everything spawns in the same place. Can't have a sawmill that outputs planks on one side and sawdust on another
3. **Fixed template.** The physical shape is predetermined. Can't build an L-shaped smithy or a long assembly line
4. **Workshop = recipe source.** Recipes belong to WorkshopType. You can't combine tools from different "types" to unlock new recipes
5. **Max 5x5 footprint.** Fine for a stonecutter, too small for a steam-powered sawmill with belt drive and log staging area
6. **Single crafter assignment.** `assignedCrafter` is one int. No multi-worker production lines

---

## Where the Cracks Will Appear

Based on the design docs (`grand colony.md`, `steampower.md`, `notdfworkshops.md`), these are the specific future features that don't fit:

### Steam/Wind Power (from `steampower.md`)
The vision: boiler → pipes → engine → shaft → belt → machine. This is inherently a multi-tile, freeform layout. A "steam sawmill" isn't a 3x5 template — it's a boiler room connected by pipes to an engine connected by belts to a saw blade, and the player chooses where each piece goes.

**Current system can't handle:** Power transmission between entities. A belt connecting two machines. A workshop that requires a power input from an adjacent tile.

### The McDonald's Kitchen (from `notdfworkshops.md`)
The vision: fryer here, grill there, soda fountain over there, NPC pathfinds between them. The "recipe" emerges from what's placed, not from a predefined list.

**Current system can't handle:** Recipes that require proximity of multiple independent entities. A crafter walking between stations within a single "job."

### Scaling to 30+ Colonists (from `grand colony.md`)
At industrial scale, you want multiple crafters working in the same workshop space. A textile mill has 10 looms. A kitchen has 3 cooks at different stations.

**Current system can't handle:** Multiple assignedCrafters. Parallel bill execution within one workspace.

---

## The Middle Path: Stations

The key insight: **don't replace the workshop system, grow it.**

### Phase 1: Now (No Changes Needed)
Keep building Era 1-2 content with the current template system. Every primitive/colony workshop is a self-contained entity, and that's correct for this era. Add more workshops as needed: Tanning Rack, Loom, Pottery Wheel, Anvil, etc.

**The Bill → Job → State Machine pipeline is solid regardless of how workshops are defined.** This is the part to protect.

### Phase 2: Station Entities (When Starting Era 3 Content)
Introduce "stations" — single-tile functional entities. Think of them as items that are placed in the world and become part of the terrain (like furniture).

A station has:
- A type (Anvil, Furnace, Grinder, Saw Blade, Bellows, etc.)
- A world position (single tile)
- Capability tags (e.g., "striking_surface", "heat_source", "cutting")

Recipes gain a new field: **required capabilities** instead of (or in addition to) a WorkshopType.

```
Recipe: "Iron Bolt"
  requires: ["heat_source", "striking_surface"]
  input: ITEM_IRON_BAR x1
  output: ITEM_IRON_BOLT x4
  workRequired: 5.0
```

The game scans for clusters of stations whose combined tags satisfy a recipe's requirements. This scan produces a "virtual workshop" — ephemeral, not a stored entity — that the existing Bill/Job system can target.

### Phase 3: Power Connections (When Building Steam/Wind)
Stations can have a "powered" flag. A Saw Blade station without power is just furniture. Connected to a belt/shaft with torque, it becomes a "power_saw" capability.

Power transmission is a separate system (like water/steam already are). It doesn't need to live inside the workshop code. It just flips a flag on stations within its network.

### Phase 4: Multi-Crafter Workspaces (When Scaling to 30+ NPCs)
A "workspace" is a named zone (like a stockpile zone) that contains stations. Bills are assigned to the workspace, not to individual stations. The job system finds an available station within the workspace and an idle crafter, pairs them.

This requires:
- Changing `assignedCrafter` from a single int to a list (or moving assignment to the Bill level)
- Work tile selection becoming dynamic (find nearest free station with required capability)
- Output tile becoming per-station or per-recipe rather than per-workshop

---

## What to Protect

These parts of the current system should survive all phases:

1. **The Recipe struct.** It just needs a `requiredCapabilities` field added later. The existing fields (inputs, outputs, work time, fuel, material matching) are all correct
2. **The Bill struct.** Work orders are a good abstraction. Whether they live on a template workshop or a station-workspace, the concept is the same
3. **The craft job state machine.** The 10-step sequence (fetch input → carry to work → craft → spawn output) is correct regardless of what defines the "workshop." The state machine just needs to resolve "work tile" and "output tile" dynamically instead of reading fixed fields
4. **Material matching.** The MAT_MATCH system is flexible and will work for any recipe source
5. **Passive workshop flow.** Timer-based production (drying rack, charcoal pit) maps directly to passive stations

## What Will Eventually Change

1. **WorkshopType enum** — stations won't need it. Template workshops keep their types; station-based recipes use capability tags
2. **ASCII templates** — only used for legacy/primitive workshops. Station-based production has no template
3. **Workshop.workTileX/Y, outputTileX/Y** — become dynamic lookups. "Where do I stand?" → nearest free station. "Where does output go?" → station's output position or nearest free floor tile
4. **One crafter per workshop** — eventually needs to support multiple, but only at Phase 4

---

## Practical Migration Notes

### Adding Capability Tags to Existing Workshops (Bridge Step)
Before building any station entities, we can add capability tags to existing template workshops as a compatibility bridge:

```c
// In WorkshopDef or Workshop struct
const char* capabilities[MAX_CAPABILITIES]; // e.g., {"heat_source", "cutting"}
int capabilityCount;
```

Existing workshops declare what they provide. New recipes can be written against capabilities. Old recipes keep working via WorkshopType. This lets us test the capability system without touching the template architecture.

### The Template-to-Station Spectrum
Not everything needs to become stations. Some things are naturally templates:

| Thing | Template or Station? | Why |
|-------|---------------------|-----|
| Stonecutter | Template | It's one integrated machine |
| Kiln | Template | Specific enclosed shape matters |
| Charcoal Pit | Template | Physical arrangement of earth/wood |
| Drying Rack | Station | It's just furniture with a timer |
| Anvil | Station | Single tool, combinable with others |
| Furnace | Station | Heat source, combinable |
| Saw Blade | Station | Needs power connection |
| Loom | Station | Could be hand or powered |
| Bellows | Station | Augments nearby heat sources |
| Conveyor Belt | Neither | Infrastructure, part of power system |

### When to Start Phase 2
The trigger is: **when you're about to add a workshop that feels wrong as a template.** If you find yourself making a 5x5 blacksmith template and thinking "but the player should be able to put the anvil wherever they want" — that's the signal.

Don't start Phase 2 preemptively. The current system has more runway than it seems. You could add 5-10 more template workshops before hitting a wall. The wall is steam/power, not more manual crafting.

---

## Summary

- **Now:** Keep using templates. They're correct for primitive era. Add more workshops freely
- **Prepare:** Keep the Recipe/Bill/Job pipeline clean. Don't add template-specific assumptions to the state machine
- **Bridge:** When ready, add capability tags to existing workshops. New recipes can use either system
- **Transition:** Station entities for Era 3+ content. Template workshops stay for primitive stuff
- **Scale:** Workspaces + multi-crafter only when actually building 30+ colonist content

The key thing to NOT do: don't extend templates to handle industrial complexity (15x10 factory templates, multi-work-tile templates). That's where you'd paint yourself in. Let templates stay small and simple, and let stations handle the freeform stuff.
