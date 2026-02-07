Perfect! I've created a todo list for the **MaterialDef sprite cleanup refactor**.

## The Plan

**Goal:** Make MaterialDef cleaner by using DF-style sprite system:
- **CellDef** has base sprite (shape defines appearance)
- **MaterialDef** has spriteOffset (material provides variant)
- **Final sprite** = `cellDef[shape].sprite + materialDef[material].spriteOffset`

**This eliminates:**
- `terrainSprite`
- `treeTrunkSprite`
- `treeLeavesSprite`
- `treeSaplingSprite`

**MaterialDef becomes cleaner and more consistent!**

The bonus 7th todo item notes we could add a **variant system** later (like DF's VAR_1-4) for visual diversity within the same material/shape combination.

Want to start working on this refactor now, or save it for later?

You're absolutely right! We **don't need spriteOffset** at all!

Here's why:

**Current system:**
```c
materialDefs[MAT_OAK].treeTrunkSprite = SPRITE_tree_trunk_oak;
materialDefs[MAT_PINE].treeTrunkSprite = SPRITE_tree_trunk_pine;
```

**What we actually need:**
```c
// Each material just points to its specific sprite for each shape
materialDefs[MAT_OAK].sprite = SPRITE_oak;     // Or whatever makes sense
materialDefs[MAT_PINE].sprite = SPRITE_pine;
```

**The sprite lookup would be:**
```c
// Based on the SHAPE, pick which sprite field to use
if (cell == CELL_TREE_TRUNK) {
    sprite = materialDefs[mat].treeTrunkSprite;  // Direct lookup, no offset math!
}
So yeah, we can **ignore spriteOffset entirely** for now. It's a future optimization that doesn't apply to your current sprite setup.

Good catch! Should we remove it from our refactor plan?
