# Feature 04: Tools & Knapping

> **Priority**: Tier 2 — Tool & Production Loop
> **Why now**: Every job runs at the same speed. Rocks, sticks, and cordage exist but have no tool use. No progression.
> **Systems used**: Rocks, sticks, cordage (all existing items), all job types
> **Depends on**: Features 1-3 (survival loop gives context for "why work faster")
> **Opens**: Better tools for farming (F6), crafting speed for everything

---

## What Changes

**Tools** are items that movers carry in a tool slot. Having the right tool speeds up jobs. A **knapping spot** (ground-level crafting) lets movers turn rocks into sharp stones, then combine with sticks and cordage to make stone-age tools. No new workshop building needed — knapping happens at a designated flat rock.

The progression: bare hands (slow) → crude stone tools (normal) → good tools (fast). Every existing job immediately benefits.

---

## Design

### Tool Quality System

Inspired by CDDA's approach (from naked-in-the-grass-details.md):

**Quality types** (what kind of work):
- `QUALITY_CUTTING` — chopping, harvesting, butchering
- `QUALITY_HAMMERING` — mining, construction, stonecutting
- `QUALITY_DIGGING` — mining, channeling, farming
- `QUALITY_SAWING` — sawmill work
- `QUALITY_FINE` — crafting, carpentry

**Quality levels**: 0 (bare hands), 1 (crude), 2 (good), 3 (excellent)

**Speed formula**:
```c
float speedMultiplier = 0.5f + 0.5f * toolLevel;
// Level 0 (no tool): 0.5× (twice as slow)
// Level 1 (crude):   1.0× (baseline)
// Level 2 (good):    1.5× (50% faster)
// Level 3 (excellent): 2.0× (twice as fast)
float actualWorkTime = baseWorkTime / speedMultiplier;
```

Tools don't **gate** actions — you can mine bare-handed, just slowly. Tools make you faster.

### Tool Slot on Mover

Each mover has a single **equipped tool** (item index or -1). The tool's quality levels are checked when starting any job. The best matching quality is used.

When assigned a job, the mover checks:
1. Does my equipped tool have the right quality? → proceed
2. Is there a better tool in a stockpile? → pick it up first (brief detour)
3. No tool / no matching quality → proceed bare-handed (0.5× speed)

Tool-seeking is opportunistic, not mandatory. Movers don't refuse work without tools.

### Tool Items

| Tool | Recipe | Qualities | Notes |
|------|--------|-----------|-------|
| ITEM_SHARP_STONE | Knap: 2× ROCK | cutting:1, fine:1 | First tool, enables everything |
| ITEM_STONE_AXE | 1× ROCK + 1× STICKS + 1× CORDAGE | cutting:2, hammering:1 | Chopping, construction |
| ITEM_STONE_PICK | 1× ROCK + 1× STICKS + 1× CORDAGE | digging:2, hammering:1 | Mining, channeling |
| ITEM_STONE_HAMMER | 1× ROCK + 1× STICKS + 1× CORDAGE | hammering:2 | Stonecutting, building |
| ITEM_DIGGING_STICK | 1× STICKS + 1× CORDAGE | digging:1 | Simplest digging tool |

### Knapping Spot

A **crafting spot** — the simplest possible workshop. A designation on a flat ground cell:

```
Template (1×1):
X
```

No building required. Just "designate knapping spot here" on any walkable cell with a rock nearby. The mover kneels, knaps, produces tools.

This follows the "first fire" philosophy from the AI discussions: early crafting should feel primitive, not industrial.

### Job Speed Integration

Every existing job type has a `baseWorkTime`. Add a `requiredQuality` field:

| Job Type | Required Quality | Base Time | Notes |
|----------|-----------------|-----------|-------|
| MINE | HAMMERING or DIGGING | 2.0s | Pick or bare hands |
| CHANNEL | DIGGING | 2.0s | Pick or digging stick |
| CHOP | CUTTING | 3.0s | Axe or bare hands |
| BUILD | HAMMERING | 2.0s | Hammer speeds construction |
| CRAFT (general) | FINE | varies | Fine work quality |
| GATHER_GRASS | CUTTING | 1.0s | Knife or bare hands |
| CLEAN | — | 3.0s | No tool needed |

### Bootstrap Loop

This is the beautiful part — the autarkic bootstrap:

```
Start: Mover has nothing (0.5× speed for everything)
  → Gather sticks (slow, no tool) → get STICKS
  → Harvest grass (slow) → dry → twist → braid → get CORDAGE
  → Knap rocks → get SHARP_STONE (now 1× cutting speed!)
  → Make stone axe (ROCK + STICKS + CORDAGE) → 2× cutting!
  → Chopping trees is now fast → more logs → more planks → build faster
```

Every step makes the next step faster. That's progression.

---

## Implementation Phases

### Phase 1: Tool Quality Framework
1. Define quality types enum and tool quality struct
2. Add `requiredQuality` to job type definitions
3. Add speed multiplier calculation
4. Add `equippedTool` (item index) to Mover struct
5. Modify work time in job execution to use speed multiplier
6. **Test**: Bare-handed work takes 2× longer, tool-equipped takes baseline

### Phase 2: Tool Items
1. Add ITEM_SHARP_STONE, ITEM_STONE_AXE, ITEM_STONE_PICK, ITEM_STONE_HAMMER, ITEM_DIGGING_STICK
2. Define quality levels per tool in item_defs
3. Add IF_TOOL flag
4. Stockpile filter for tools
5. **Test**: Tool items exist with correct quality values

### Phase 3: Knapping Spot
1. Add WORKSHOP_KNAPPING_SPOT (1×1 minimal workshop)
2. Knapping recipes: rock → sharp stone, rock+sticks+cordage → axe, etc.
3. No fuel, short work times
4. **Test**: Knapping produces tools, recipes work correctly

### Phase 4: Tool Seeking
1. When assigned job, mover checks equipped tool quality
2. If better tool available in nearby stockpile, detour to pick it up
3. Equip tool (goes in tool slot, not carried item)
4. Proceed to job with tool bonus
5. **Test**: Mover picks up axe before chopping, chops faster

### Phase 5: Existing Job Integration
1. All mining jobs check HAMMERING/DIGGING quality
2. All chopping jobs check CUTTING quality
3. All building jobs check HAMMERING quality
4. All crafting jobs check FINE quality
5. **Test**: Every job type benefits from appropriate tool

---

## Connections

- **Uses rocks**: Currently only sink is stonecutter. Now also: knapping
- **Uses sticks**: Currently only sink is charcoal pit. Now also: tool handles
- **Uses cordage**: Currently only sink is construction. Now also: tool binding
- **Uses all job types**: Speed multiplier applies universally
- **Opens for F5 (Cooking)**: Butchering animals needs cutting tool
- **Opens for F6 (Farming)**: Tilling needs digging tool
- **Opens for future metal**: Bronze/iron tools at quality 3

---

## Design Notes

### Why Not Durability?

Tool durability (tools breaking after N uses) adds bookkeeping but not much fun in early game. Save it for metal tools later — stone tools are effectively infinite for now. The progression is "no tool → stone tool → metal tool" not "stone tool wears out."

### Why a Single Tool Slot?

Simplicity. Movers equip the best available tool for their current job. They don't juggle inventories. A miner grabs a pick, a woodcutter grabs an axe. The tool-seeking is automatic and brief.

### Knapping Spot vs Workshop

Knapping at a 1×1 spot (not a 3×3 building) feels right for primitive tech. It's the lowest-investment workshop possible. You're crouching by a rock, not operating machinery.

---

## Save Version Impact

- New Mover field: equippedTool (int, item index)
- New items: 5 tool types
- New workshop: WORKSHOP_KNAPPING_SPOT
- New item flag: IF_TOOL
- Quality data on item definitions

## Test Expectations

- ~40-50 new assertions
- Speed multiplier calculation
- Tool quality matching per job type
- Knapping recipes produce correct tools
- Tool seeking behavior
- All existing jobs benefit from tools
- Bootstrap sequence (no tool → sharp stone → axe)
