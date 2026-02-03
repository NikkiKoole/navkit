# Data-Driven Items (IMPLEMENTED)

*Archived February 2026 - this has been implemented in item_defs.c*

## Current State

Item system is minimal - only 4 types, properties in 2 files:

| Property | Location |
|----------|----------|
| Enum | `items.h` |
| Sprite | `rendering.c` (3 switch statements) |
| Building material check | `jobs.c` (hardcoded `ITEM_ORANGE`) |

## When to Refactor

Do this refactor when:
- Adding 5+ new item types (food, wood, ore, tools)
- Adding item tooltips (need names)
- Adding food/spoilage system (need nutrition, spoilRate)
- `ITEM_ORANGE`-style special cases start multiplying

## Proposed Structure

```c
// item_defs.h

typedef struct {
    const char* name;
    int sprite;
    uint8_t flags;
    uint8_t maxStack;
    // Future: uint8_t nutrition, spoilRate, etc.
} ItemDef;

#define IF_STACKABLE      (1 << 0)
#define IF_BUILDING_MAT   (1 << 1)
#define IF_EDIBLE         (1 << 2)
#define IF_SPOILS         (1 << 3)

extern ItemDef itemDefs[];

#define ItemName(t)           (itemDefs[t].name)
#define ItemSprite(t)         (itemDefs[t].sprite)
#define ItemMaxStack(t)       (itemDefs[t].maxStack)
#define ItemIsBuildingMat(t)  (itemDefs[t].flags & IF_BUILDING_MAT)
```

```c
// item_defs.c

ItemDef itemDefs[] = {
    [ITEM_RED]    = {"red crate",   SPRITE_crate_red,    IF_STACKABLE,                  10},
    [ITEM_GREEN]  = {"green crate", SPRITE_crate_green,  IF_STACKABLE,                  10},
    [ITEM_BLUE]   = {"blue crate",  SPRITE_crate_blue,   IF_STACKABLE,                  10},
    [ITEM_ORANGE] = {"stone block", SPRITE_crate_orange, IF_STACKABLE | IF_BUILDING_MAT, 20},
    // [ITEM_APPLE] = {"apple",      SPRITE_apple,        IF_STACKABLE | IF_EDIBLE | IF_SPOILS, 20},
};
```

## Migration Steps

1. Create `item_defs.h` and `item_defs.c`
2. Replace 3 switch statements in `rendering.c` with `ItemSprite()`
3. Replace `item->type == ITEM_ORANGE` in `jobs.c` with `ItemIsBuildingMat()`
4. Add `ItemName()` to tooltips when implemented

## See Also

- `docs/done/data-driven-cells.md` - Completed refactor using same pattern
