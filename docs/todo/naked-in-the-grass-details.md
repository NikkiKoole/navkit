# Naked in the Grass — Design Details

Companion to `naked-in-the-grass.md`. Deep dives into specific systems.

---

## 1. Tool Qualities (CDDA-style)

### Philosophy

CDDA doesn't ask "do you have a hammer?" — it asks "do you have something with hammering quality 1 or better?" A rock has hammering:1, a stone hammer has hammering:2, an iron hammer has hammering:3. This means:

- You're never truly helpless. A sharp rock can cut.
- Better tools are *faster* or *unlock harder recipes*, not gatekeepers of basic function.
- The same item can serve multiple roles (a hatchet has cutting:1 AND hammering:1).

### Quality Types

```
QUALITY_CUTTING     // knife, sharp rock, axe, saw
QUALITY_HAMMERING   // rock, stone hammer, iron hammer
QUALITY_DIGGING     // hands, digging stick, stone pick, iron pick
QUALITY_SAWING      // saw (subset of cutting, but for planks)
QUALITY_FINE_WORK   // needle, awl, chisel (precision tasks)
```

Keep it small. 5 qualities covers the current crafting space and the near future. More can be added later.

### How Items Provide Qualities

Each ItemType gets an optional quality list:

```c
typedef struct {
    QualityType type;
    int level;          // 1 = crude, 2 = decent, 3 = good
} ItemQuality;

// In item definition table:
// ITEM_ROCK:         { QUALITY_HAMMERING, 1 }, { QUALITY_CUTTING, 1 }
// ITEM_STICKS:       { QUALITY_DIGGING, 1 }
// ITEM_STONE_KNIFE:  { QUALITY_CUTTING, 2 }, { QUALITY_FINE_WORK, 1 }
// ITEM_STONE_AXE:    { QUALITY_CUTTING, 2 }, { QUALITY_HAMMERING, 1 }
// ITEM_STONE_PICK:   { QUALITY_DIGGING, 2 }, { QUALITY_HAMMERING, 1 }
// ITEM_IRON_AXE:     { QUALITY_CUTTING, 3 }, { QUALITY_HAMMERING, 2 }
```

A rock on the ground is both a crude hammer AND a crude cutting edge. No new items needed to bootstrap — rocks already exist.

### How Recipes Require Qualities

Extend the Recipe struct (or a parallel table) with quality requirements:

```c
typedef struct {
    QualityType type;
    int minLevel;
} RecipeQualityReq;

// Sawmill "Saw Planks": requires QUALITY_SAWING >= 2
// Stonecutter "Cut Blocks": requires QUALITY_HAMMERING >= 2
// Rope Maker "Twist Bark": requires QUALITY_CUTTING >= 1
// Hand-craft "Make Digging Stick": requires QUALITY_CUTTING >= 1
```

### Where Tools Live: Workshop vs Mover vs Ground

Three options, not mutually exclusive:

**Option A: Workshop-installed tools.** The sawmill *is* the saw. The stonecutter *is* the chisel + hammer. Workshops provide their own quality levels. This is closest to how things work now — workshops are already the gatekeepers of what can be crafted. Adding quality requirements to workshops would mean: "you need cutting:1 to build a sawmill" (use a rock to carve the frame), but once built, the sawmill itself provides sawing:2.

**Option B: Mover-carried tools.** Movers equip or carry a tool that provides qualities. A mover with a stone pick has digging:2 and mines faster. This requires an equipment/inventory slot (see below).

**Option C: Nearby tool on ground.** Recipe checks for a tool item within the workshop area. The crafter doesn't carry it — it just needs to be *there*. CDDA does this for stationary crafting.

**Recommendation:** Start with A+C. Workshops provide baseline qualities. Some recipes additionally require a tool item to be present at the workshop (placed on a nearby tile or in a tool slot). This avoids needing a full mover equipment system immediately.

For designations (mining, chopping), use Option B: check if the mover is carrying/has-equipped a tool. Bare-handed mining works (digging:0 = very slow). Stone pick = normal speed. Iron pick = fast. This gives a direct progression feel without overhauling the job system.

### Speed Scaling

Tool quality affects work speed, not binary unlock:

```
effective_time = base_time / (0.5 + 0.5 * tool_level)

Level 0 (bare hands): base_time / 0.5 = 2x slower
Level 1 (crude tool): base_time / 1.0 = normal
Level 2 (good tool):  base_time / 1.5 = 1.5x faster
Level 3 (great tool): base_time / 2.0 = 2x faster
```

Some recipes have a hard minimum: "requires cutting >= 2" means you literally can't do it with a sharp rock. Sawing planks needs a real saw.

### New Items This Enables

Early game (no workshop needed, hand-craft with rocks):
- **ITEM_STONE_KNIFE** — 1 rock + 1 rock (knapping). cutting:2, fine_work:1
- **ITEM_DIGGING_STICK** — 1 stick + cutting:1. digging:1
- **ITEM_STONE_AXE** — 1 rock + 1 stick + 1 cordage. cutting:2, hammering:1
- **ITEM_STONE_PICK** — 1 rock + 1 stick + 1 cordage. digging:2, hammering:1
- **ITEM_STONE_HAMMER** — 1 rock + 1 stick + 1 cordage. hammering:2

This creates an early loop: gather sticks + harvest grass -> dry grass -> twist string -> braid cordage -> make stone tools. The player is *using* the production chain to bootstrap better production.

### Mover Equipment (Minimal)

Don't need a full inventory. Just one slot:

```c
typedef struct Mover {
    // ... existing fields ...
    int equippedTool;   // item index, -1 = none
} Mover;
```

A mover with an equipped tool:
- Gets its qualities for designation work (mining speed, chopping speed)
- The tool is NOT consumed (unlike construction materials)
- Tool could degrade over use (future: durability)

Job system change: when assigning a mining job, prefer movers with digging quality. Bare-handed movers still take the job if nobody better is idle — just slower.

### Hand-Crafting (No Workshop)

Some recipes don't need a workshop at all. A "knapping" action could work like designations — select "craft stone knife" from a menu, mover walks to a rock on the ground, kneels, works for 3 seconds, rock becomes stone knife.

Alternatively: a "crafting spot" that's a 1x1 invisible workshop (just a tile the mover stands on). Keeps the workshop/recipe infrastructure without needing a built structure.

---

## 2. Animals

Moved to dedicated doc: **[animals.md](animals.md)**

Covers body sizes (2-cell cows with heading rotation), movement constraints (no ladders for livestock), steering-based AI (not A*), intelligence tiers (cow brain vs wolf brain), and the `experiments/steering/` library integration.

---

## How These Two Systems Connect

The tool quality system and the animal system reinforce each other:

1. **Stone knife** (cutting:2) is needed to **butcher** a hunted animal
2. **Cordage** (already exists!) is needed to make **snares** for trapping small animals
3. **Stone axe** (cutting:2) lets you **build fences** faster to pen livestock
4. **Digging stick** (digging:1) lets you **plant crops** (future) to feed penned animals

The survival loop becomes:
> Gather grass -> dry -> twist -> braid cordage -> make stone tools -> hunt/trap animals -> butcher with knife -> cook at hearth -> eat

Every link in that chain either already exists or is one step away from existing.
