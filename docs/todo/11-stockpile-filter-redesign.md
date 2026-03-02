# Stockpile Filter Redesign

> Status: spec

> **Status**: Design / brainstorm
> **Why**: Filter list is 30+ flat entries and growing. Containers, curing states, more materials incoming. Keyboard keys running out. UI becoming a wall of text.

---

## Problem

Current system: two flat arrays (`allowedTypes[ITEM_TYPE_COUNT]`, `allowedMaterials[MAT_COUNT]`) with one keyboard key per entry. Every new item type adds another row to the flat list.

Today (30 item filters + 4 material filters):

```
 ┌─────────────────────────────────────────────────┐
 │ Stockpile (3x3)  Fill: 2/9                      │
 │                                                  │
 │ Filters:                                         │
 │ r:Red g:Green b:Blue o:Rock s:Blocks w:Wood      │
 │ d:Dirt p:Planks k:Sticks l:Poles m:Grass         │
 │ h:DriedGrass i:Bricks c:Charcoal a:Bark          │
 │ e:StrippedLog n:String j:Cordage t:Saplings      │
 │ v:Leaves y:Clay q:Gravel z:Sand u:Peat f:Ash    │
 │ x:Berries 1:DriedBerries 2:Baskets 3:ClayPots   │
 │ 4:Chests                                         │
 │                                                  │
 │ Wood: 1:Oak 2:Pine 3:Birch 4:Willow              │
 │                                                  │
 │ [X] toggle all  [?] help                         │
 └─────────────────────────────────────────────────┘
```

Problems:
- **Flat list doesn't scale** — 30 entries already, adding curing states (green/wet/dry/cured) per item type would explode it
- **Running out of keys** — 26 letters + 10 digits = 36, already at 34
- **No grouping** — wood items, stone items, plant items all mixed together
- **Material axis is incomplete** — only wood species for logs; stone items (rock, blocks, bricks) have no material sub-filter yet
- **No way to filter by item state** — curing state, freshness, quality are coming

---

## Reference: How Other Games Do It

### Dwarf Fortress
- **3-column hierarchical browser**: Category → Subcategory → Individual items
- **17 major categories**: Food, Weapons, Armor, Stone, Wood, Furniture, Bars/Blocks, etc.
- **Orthogonal axes per category**: item type + material + quality (core & total)
- **All/None toggles** at every level
- Key insight: **categories are just UI grouping** — the underlying filter is still "accept this item? yes/no"

### RimWorld
- **Collapsible tree**: Category → Subcategory → Item (like a file browser)
- **Quality slider**: min/max range filter
- **Hit points slider**: min/max range filter
- **Priority levels**: Critical → Low (drives hauling order, not acceptance)
- Popular mods add: material filters, rottable/degradable toggles, stack limits

### Common Pattern
Both games separate **what** (item type hierarchy) from **what kind** (material, quality, state). The "what" is a tree you navigate; the "what kind" are orthogonal toggles/sliders that apply across the selection.

---

## Proposed Design: Categorized Items + Orthogonal Axes

### Core Idea

Split the current flat filter into:
1. **Item categories** (UI grouping for navigation)
2. **Orthogonal filter axes** (material, curing state — each a small independent bitmask)

An item is accepted if it passes ALL axes:
```
accepted = allowedTypes[item.type]
        && allowedMaterials[item.material]
        && allowedCuringStates[item.curingState]   // future
```

### Item Categories

Group the 30+ items into ~6 navigable categories:

| Category     | Items                                                    |
|-------------|----------------------------------------------------------|
| **Stone**    | Rock, Blocks, Bricks, Gravel, Sand                      |
| **Wood**     | Log, Planks, Sticks, Poles, Bark, Stripped Log           |
| **Plant**    | Grass, Dried Grass, Leaves, Sapling, Berries, Dried Berries |
| **Earth**    | Dirt, Clay, Peat, Ash, Charcoal                          |
| **Craft**    | String, Cordage                                          |
| **Container**| Basket, Clay Pot, Chest                                  |
| **Debug**    | Red, Green, Blue                                         |

### Filter Axes (Orthogonal)

| Axis             | Values                              | Storage                    |
|-----------------|--------------------------------------|----------------------------|
| **Item Type**    | per item type                       | `bool allowedTypes[ITEM_TYPE_COUNT]`  (existing) |
| **Material**     | Oak, Pine, Birch, Willow, Granite, Slate, ... | `bool allowedMaterials[MAT_COUNT]`  (existing) |
| **Curing State** | Green, Wet, Dry, Cured              | `bool allowedCuringStates[CURING_COUNT]`  (new) |

Each axis is independent. Material axis applies to ALL item types (not just logs).

---

## UI Sketches

### Option A: Tabbed Categories

Press a number key to switch category tab. Letters toggle items within the active category. Orthogonal axes shown below.

```
 ┌──────────────────────────────────────────────────────────┐
 │ Stockpile (3x3)  Fill: 2/9                               │
 │                                                           │
 │ [1:Stone] [2:Wood] [3:Plant] [4:Earth] [5:Craft] [6:Box] │
 │  ─────── ════════                                         │
 │                    ▲ active tab                            │
 │                                                           │
 │  a: Log ✓    b: Planks ✓    c: Sticks ✓                  │
 │  d: Poles ✓  e: Bark ✓      f: Stripped Log ✗             │
 │                                                           │
 │ ── Materials ──────────────────────────                   │
 │  1: Oak ✓  2: Pine ✓  3: Birch ✓  4: Willow ✗           │
 │                                                           │
 │ ── Curing ─────────────────────────────                   │
 │  q: Green ✓  w: Wet ✓  e: Dry ✓  r: Cured ✓             │
 │                                                           │
 │  [X] toggle all in tab  [Shift+X] toggle all everywhere  │
 └──────────────────────────────────────────────────────────┘
```

**Pros**: Clean, scalable, each tab only shows ~3-6 items
**Cons**: Extra keypress to switch tabs; can't see all filters at once

### Option B: Compact Grouped List (no tabs)

All categories visible at once but grouped with headers. Press first letter of group + item key as a chord, or just scroll with visual grouping.

```
 ┌──────────────────────────────────────────────────┐
 │ Stockpile (3x3)  Fill: 2/9                       │
 │                                                   │
 │ Stone: Rock Blocks Bricks Gravel Sand             │
 │        ✓    ✓      ✓      ✗      ✗               │
 │ Wood:  Log  Planks Sticks Poles  Bark  Stripped   │
 │        ✓    ✓      ✓      ✓      ✓     ✗         │
 │ Plant: Grass DrGrs  Leaves Sapl  Berry DrBerry    │
 │        ✓     ✓      ✗      ✗     ✓     ✓         │
 │ Earth: Dirt  Clay   Peat   Ash   Charcoal         │
 │        ✓     ✗      ✗      ✗     ✓               │
 │ Craft: String Cordage                             │
 │        ✓      ✓                                   │
 │ Box:   Basket  ClayPot  Chest                     │
 │        ✓       ✓        ✓                         │
 │                                                   │
 │ Material: Oak✓ Pine✓ Birch✓ Willow✗ Granite✓     │
 │ Curing:   Green✓ Wet✓ Dry✓ Cured✓                │
 │                                                   │
 │ [Tab] next group  [a-f] toggle item  [X] all/none│
 └──────────────────────────────────────────────────┘
```

**Pros**: Everything visible at a glance; visual grouping helps scanning
**Cons**: Tall tooltip; still need key assignment scheme

### Option C: Two-Level Navigation (DF-style)

Default view shows categories with summary. Press key to "drill in" and see/toggle individual items. Press Esc to go back up.

```
 DEFAULT VIEW (category level):
 ┌──────────────────────────────────────────────────┐
 │ Stockpile (3x3)  Fill: 2/9                       │
 │                                                   │
 │ Categories:                                       │
 │  a: Stone    [5/5] ✓  all                         │
 │  b: Wood     [5/6]    partial                     │
 │  c: Plant    [4/6]    partial                     │
 │  d: Earth    [2/5]    partial                     │
 │  e: Craft    [2/2] ✓  all                         │
 │  f: Container[3/3] ✓  all                         │
 │                                                   │
 │ Material: Oak✓ Pine✓ Birch✓ Willow✗               │
 │ Curing:   Green✓ Wet✓ Dry✓ Cured✓                │
 │                                                   │
 │ [a-f] edit category  [X] all  [1-4] materials     │
 └──────────────────────────────────────────────────┘

 DRILLED INTO "Wood" (press 'b'):
 ┌──────────────────────────────────────────────────┐
 │ Stockpile (3x3)  Fill: 2/9                       │
 │                                                   │
 │ ◄ Wood [5/6]:                                     │
 │  a: Log ✓        d: Poles ✓                       │
 │  b: Planks ✓     e: Bark ✓                        │
 │  c: Sticks ✓     f: Stripped Log ✗                │
 │                                                   │
 │ Material: Oak✓ Pine✓ Birch✓ Willow✗               │
 │ Curing:   Green✓ Wet✓ Dry✓ Cured✓                │
 │                                                   │
 │ [a-f] toggle  [X] all/none  [Esc] back            │
 └──────────────────────────────────────────────────┘
```

**Pros**: Most scalable; compact default view; keys reused per level so never run out
**Cons**: Two-step interaction; can't toggle individual items from top level

### Option D: Preset + Customize (RimWorld-style)

Choose a stockpile preset on creation, then customize. Most players never need to touch filters.

```
 STOCKPILE CREATION:
 ┌──────────────────────────────────────┐
 │ New Stockpile — choose type:         │
 │                                      │
 │  1: General     (accept everything)  │
 │  2: Wood         (logs, planks, ...) │
 │  3: Stone        (rock, blocks, ...) │
 │  4: Plants       (grass, berries)    │
 │  5: Containers   (baskets, pots)     │
 │  6: Custom       (empty, you pick)   │
 └──────────────────────────────────────┘

 AFTER CREATION (same as Option A/B/C for editing)
```

**Pros**: Fast common case; teaches categories implicitly
**Cons**: Extra step at creation; still need a full editor for customization

---

## Recommendation

**Option C (Two-Level) + Option D (Presets)** combined:

1. **Presets on creation** — fast, covers 80% of use cases
2. **Two-level drill-down for editing** — scales to any number of items
3. **Orthogonal axes always visible** — material + curing shown at both levels
4. **Keys are reused per level** — never run out (a-z available at each depth)

### Data Model Changes

```c
// New: category grouping (purely for UI)
typedef enum {
    FILTER_CAT_STONE,
    FILTER_CAT_WOOD,
    FILTER_CAT_PLANT,
    FILTER_CAT_EARTH,
    FILTER_CAT_CRAFT,
    FILTER_CAT_CONTAINER,
    FILTER_CAT_COUNT
} StockpileFilterCategory;

// Existing StockpileFilterDef gets a category field
typedef struct {
    ItemType itemType;
    StockpileFilterCategory category;  // NEW
    const char* displayName;
    const char* shortName;
    Color color;
} StockpileFilterDef;
// Note: 'key' removed — assigned dynamically per category (a, b, c...)

// New: curing state filter axis
typedef struct {
    // ... existing fields ...
    bool allowedTypes[ITEM_TYPE_COUNT];       // existing
    bool allowedMaterials[MAT_COUNT];         // existing
    bool allowedCuringStates[CURING_COUNT];   // NEW
} Stockpile;

// Acceptance check
bool StockpileAcceptsItem(Stockpile *sp, Item *item) {
    return sp->allowedTypes[item->type]
        && sp->allowedMaterials[item->material]
        && sp->allowedCuringStates[item->curingState];
}
```

### UI State (input.c)

```c
// Track which level we're at
int stockpileFilterLevel = 0;           // 0 = category view, 1 = item view
int stockpileFilterActiveCategory = -1; // which category is drilled into
```

### Implementation Phases

| Phase | Work | Scope |
|-------|------|-------|
| **0** | Add `category` field to `StockpileFilterDef`, group existing items | Data only, no UI change yet |
| **1** | Two-level drill-down UI (Option C) | input.c + tooltips.c |
| **2** | Presets on stockpile creation (Option D) | input.c |
| **3** | Add `allowedCuringStates[]` axis | stockpiles.h + save migration |
| **4** | Expand material filters beyond wood | stockpiles.c filter table |

---

## Decisions

- **Presets**: Start hardcoded (simple C array), but design the struct so it's easy to make data-driven later
- **Category toggle**: Yes — Shift+key toggles entire category from top level without drilling in
- **Copy filters**: Not now — good idea, add as a follow-up feature later
- **Debug items (R/G/B)**: Own "Debug" category at the end of the category list

---

## Updated UI Sketch (Final)

```
 TOP LEVEL (category view):
 ┌──────────────────────────────────────────────────┐
 │ Stockpile (3x3)  Fill: 2/9                       │
 │                                                   │
 │ Categories:                                       │
 │  a: Stone     [5/5] ✓  all                        │
 │  b: Wood      [5/6]    partial                    │
 │  c: Plant     [4/6]    partial                    │
 │  d: Earth     [2/5]    partial                    │
 │  e: Craft     [2/2] ✓  all                        │
 │  f: Container [3/3] ✓  all                        │
 │  g: Debug     [3/3] ✓  all                        │
 │                                                   │
 │ Material: Oak✓ Pine✓ Birch✓ Willow✗               │
 │ Curing:   Green✓ Wet✓ Dry✓ Cured✓                │
 │                                                   │
 │ [a-g] drill in  [Shift+a-g] toggle category       │
 │ [X] toggle all  [1-4] materials                   │
 └──────────────────────────────────────────────────┘

 DRILLED INTO "Wood" (press 'b'):
 ┌──────────────────────────────────────────────────┐
 │ Stockpile (3x3)  Fill: 2/9                       │
 │                                                   │
 │ ◄ Wood [5/6]:                                     │
 │  a: Log ✓          d: Poles ✓                     │
 │  b: Planks ✓       e: Bark ✓                      │
 │  c: Sticks ✓       f: Stripped Log ✗              │
 │                                                   │
 │ Material: Oak✓ Pine✓ Birch✓ Willow✗               │
 │ Curing:   Green✓ Wet✓ Dry✓ Cured✓                │
 │                                                   │
 │ [a-f] toggle  [X] all/none  [Esc] back            │
 └──────────────────────────────────────────────────┘

 STOCKPILE CREATION (preset picker):
 ┌──────────────────────────────────────┐
 │ New Stockpile — choose type:         │
 │                                      │
 │  1: General     (accept everything)  │
 │  2: Wood        (logs, planks, ...)  │
 │  3: Stone       (rock, blocks, ...)  │
 │  4: Plants      (grass, berries)     │
 │  5: Containers  (baskets, pots)      │
 │  6: Custom      (empty, you pick)    │
 └──────────────────────────────────────┘
```
