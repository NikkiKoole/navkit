# Material-Based Rendering

Now that we have a data-driven material system (`cellMaterial` grid, `MaterialDef` struct), walls/floors/etc all render the same regardless of material. This document explores options for visually distinguishing materials.

## Current State

- All walls render with the same sprite (e.g., orange/brown wall)
- `MaterialDef` has `spriteOffset` field but it's not used yet
- Materials: MAT_NATURAL, MAT_STONE, MAT_WOOD, MAT_IRON, MAT_GLASS

## Goals

- Visually distinguish wood walls from stone walls from iron walls
- Scale to support many materials without explosion of art assets
- Keep rendering simple (raylib with sprite atlas)

---

## Options

### Option A: Tint Only (Simplest)

Add a `Color tint` field to `MaterialDef`. When rendering, pass the tint to `DrawTextureRec()`.

```c
typedef struct {
    const char* name;
    Color tint;           // NEW
    int spriteOffset;
    uint8_t flags;
    uint8_t fuel;
    ItemType dropsItem;
} MaterialDef;

// Rendering:
Color tint = IsConstructedCell(x,y,z) ? materialDefs[GetCellMaterial(x,y,z)].tint : WHITE;
DrawTextureRec(atlas, sourceRect, pos, tint);
```

**Pros:**
- Almost zero code change (tint param already exists)
- Scales infinitely - just define colors for new materials
- No new art assets needed

**Cons:**
- Limited visual variety (just color shifts)
- Works best with grayscale/desaturated base sprites

**Sprite considerations:**
- Tinting multiplies pixel colors by tint color
- Colored sprites + tint = muddy results
- Grayscale sprites + tint = clean colored result
- May need to remake sprites as grayscale for best results

### Option B: Tint + Pattern Overlay

Combine tinting with a small set of tileable pattern textures.

```c
typedef struct {
    const char* name;
    Color tint;
    int patternIndex;     // 0=none, 1=wood grain, 2=stone cracks, 3=metal
    uint8_t patternAlpha; // How visible (0-255)
    // ...
} MaterialDef;
```

Patterns (small tileable textures, maybe 32x32):
- `pattern_wood.png` - wood grain lines
- `pattern_stone.png` - cracks/speckles
- `pattern_metal.png` - subtle diagonal lines/sheen

Rendering:
1. Draw base sprite with material tint
2. Draw pattern overlay with alpha blending

```c
DrawTextureRec(atlas, wallRect, pos, materialDefs[mat].tint);

if (materialDefs[mat].patternIndex > 0) {
    Color patternColor = (Color){255, 255, 255, materialDefs[mat].patternAlpha};
    DrawTextureRec(patterns, patternRects[mat.patternIndex], pos, patternColor);
}
```

**Pros:**
- More visual variety than tint alone
- Still scales well (few patterns, many colors)
- 3-4 patterns cover most material categories

**Cons:**
- Extra draw call per cell with pattern
- Need to create pattern textures
- Pattern tiling might look repetitive

### Option C: Sprite Variants per Material

Use `spriteOffset` to select different sprite variants from the atlas.

```c
int sprite = CellSprite(cell) + MaterialSpriteOffset(mat);
```

**Pros:**
- Full art control per material
- Can have very distinct looks

**Cons:**
- Doesn't scale: 5 materials × 10 cell types = 50+ sprites
- Lots of art work for each new material
- Atlas gets large

### Option D: Hybrid Approach

Different strategies for different contexts:

- **Natural terrain**: Keep current hand-crafted colored sprites (no tint)
- **Constructed cells**: Use grayscale sprites + tint based on material
- **Special materials**: Can have unique sprites via spriteOffset

This preserves the designed look of natural terrain while giving material variety for player constructions.

```c
if (IsConstructedCell(x, y, z)) {
    // Use grayscale "constructed" sprite variant + material tint
    int sprite = GetConstructedSpriteForCell(cell);
    Color tint = materialDefs[GetCellMaterial(x,y,z)].tint;
    DrawTextureRec(atlas, spriteRect[sprite], pos, tint);
} else {
    // Natural terrain - use original colored sprite, no tint
    DrawTextureRec(atlas, spriteRect[CellSprite(cell)], pos, WHITE);
}
```

---

## Recommendation

**Start with Option A (tint only)** as a proof of concept:

1. Add `Color tint` to `MaterialDef`
2. Set distinct colors: wood=brown, stone=gray, iron=darkgray, glass=lightblue
3. Apply tint when rendering constructed cells
4. See how it looks with current sprites

If results are muddy, consider:
- Creating grayscale versions of wall/floor sprites
- Or go with hybrid (Option D) where only constructed cells use tinting

Later, add patterns (Option B) if more variety is needed.

---

## Implementation Sketch

### Phase 1: Add tint field

```c
// material.h
typedef struct {
    const char* name;
    Color tint;           // ADD THIS
    int spriteOffset;
    uint8_t flags;
    uint8_t fuel;
    ItemType dropsItem;
} MaterialDef;

// material.c
MaterialDef materialDefs[MAT_COUNT] = {
    [MAT_NATURAL] = {"natural",  WHITE,                 0, 0,            0,   ITEM_NONE},
    [MAT_STONE]   = {"stone",    (Color){180,180,180,255}, 0, 0,         0,   ITEM_STONE_BLOCKS},
    [MAT_WOOD]    = {"wood",     (Color){180,120,80,255},  1, MF_FLAMMABLE, 128, ITEM_WOOD},
    [MAT_IRON]    = {"iron",     (Color){100,100,110,255}, 2, 0,         0,   ITEM_STONE_BLOCKS},
    [MAT_GLASS]   = {"glass",    (Color){200,220,255,255}, 3, 0,         0,   ITEM_STONE_BLOCKS},
};

// Add accessor
#define MaterialTint(m) (materialDefs[m].tint)
```

### Phase 2: Apply in rendering

Find where walls are rendered and apply tint for constructed cells.

### Phase 3: Evaluate and iterate

- Do the tints look good with current sprites?
- If muddy, create grayscale sprite variants
- If too subtle, add pattern overlays

---

## Open Questions

- What do current wall sprites look like? (detailed/colored vs simple)
- Should natural terrain stay as-is or also get material variety?
- Do we want sub-types within materials? (oak vs pine vs birch all MAT_WOOD but different tints)
- How to handle transparency for glass?

---

## Related

- `src/world/material.h` - MaterialDef struct
- `src/world/material.c` - materialDefs array
- `src/render/rendering.c` - where cells are drawn
- Completed: `docs/done/architecture/data-driven-jobs-materials.md`
