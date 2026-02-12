# Material-Based Rendering

Now that we have a data-driven material system (`cellMaterial` grid, `MaterialDef` struct), walls/floors/etc all render the same regardless of material. This document explores options for visually distinguishing materials.

## Current State

- All walls render with the same sprite (e.g., orange/brown wall)
- `MaterialDef` has `spriteOffset` field but it's not used yet
- Materials: MAT_NATURAL, MAT_STONE, MAT_WOOD, MAT_IRON, MAT_GLASS
- Sprites are small pixel art with black outlines and colored fills
- Already have distinct textures: brick pattern, wood plank pattern, dirt variations

## Goals

- Visually distinguish wood walls from stone walls from iron walls
- Scale to support many materials without explosion of art assets
- Keep rendering simple (raylib with sprite atlas)

---

## Decision: Start with Option B (Different Sprites per Material)

After brainstorming, we decided to start with **sprite variants per material** because:

1. We already have distinct sprites (brick texture, plank texture)
2. Tinting doesn't work well with colored sprites (muddy results)
3. Black outlines survive tinting, but colored fills get muddy
4. Clear visual distinction is more valuable than infinite scalability for now

Later, we can add **tinting for sub-varieties** (Option C hybrid) - e.g., oak vs pine vs birch could be the same plank sprite with different brown tints.

---

## Options Considered

### Option A: Tint Only

**How it works:** Add `Color tint` to MaterialDef, multiply sprite colors by tint.

**Pros:**
- Almost zero code change
- Scales infinitely
- No new art needed

**Cons:**
- Limited visual variety (just color shifts)
- Colored sprites + tint = muddy results
- Would need grayscale base sprites for clean results

**Tinting math:**
```
Black outline: (0,0,0) × any tint = (0,0,0)  ← stays black, good!
Brown fill:    (150,100,70) × gray tint (180,180,180) = (105,70,49) ← muddy
White fill:    (255,255,255) × brown tint = brown ← clean!
```

**Conclusion:** Tinting works best with grayscale sprites. Our current colored sprites would need to be remade, or results will be muddy.

### Option B: Different Sprites per Material (CHOSEN)

**How it works:** Use `spriteOffset` to select different sprite from atlas based on material.

```c
int sprite = CellSprite(cell) + MaterialSpriteOffset(mat);
```

**Pros:**
- Full art control per material
- Clear visual distinction (planks vs bricks)
- We already have some sprites for this

**Cons:**
- Each new material needs new sprites for wall, floor, etc.
- More art work as materials grow
- Atlas gets bigger

**Conclusion:** Best for now - we have the sprites, it's clear and readable.

### Option C: Hybrid (Sprites + Tinting)

**How it works:** Different base sprites per material category, then tint for sub-varieties.

```c
// Wood materials all use plank sprite, but different tints
MAT_OAK   → plank sprite + dark brown tint
MAT_PINE  → plank sprite + light brown tint  
MAT_BIRCH → plank sprite + pale/white tint

// Stone materials all use brick sprite, but different tints
MAT_GRANITE → brick sprite + gray tint
MAT_MARBLE  → brick sprite + white tint
```

**Pros:**
- Best of both worlds
- Base texture from sprite, variety from tint
- Scales well within material categories

**Cons:**
- More complex to implement
- Need to manage both sprite mapping AND tints

**Conclusion:** Good future enhancement once Option B is working.

### Option D: Pattern Overlay

**How it works:** Draw base shape, then overlay tileable pattern texture.

**Pros:**
- Few patterns cover many materials
- Could combine with tinting

**Cons:**
- Extra draw call per cell
- Need to create pattern textures
- May look repetitive

**Conclusion:** Interesting but more complex. Consider later if needed.

---

## Implementation Plan (Option B)

### Phase 1: Map materials to existing sprites

1. Identify which sprites to use:
   - `MAT_STONE` → brick/block wall sprite (current default)
   - `MAT_WOOD` → plank/diagonal wall sprite
   - `MAT_IRON` → could use stone for now, or create metal sprite
   - `MAT_GLASS` → needs new sprite (transparent?)
   - `MAT_DIRT` → dirt texture (if we add dirt as building material)

2. Update `MaterialDef.spriteOffset` values in `material.c`

3. Update rendering code to use:
   ```c
   int baseSprite = CellSprite(cell);
   MaterialType mat = GetCellMaterial(x, y, z);
   if (mat != MAT_NATURAL) {
       baseSprite += MaterialSpriteOffset(mat);
   }
   ```

### Phase 2: Organize sprite atlas

- Ensure wall variants are laid out with consistent offsets
- e.g., if SPRITE_wall = 10, then:
  - SPRITE_wall + 0 = stone wall
  - SPRITE_wall + 1 = wood wall
  - SPRITE_wall + 2 = iron wall

### Phase 3: Test and iterate

- Build walls with different materials
- Verify correct sprites render
- Add missing sprites as needed

---

## Future: Adding Tinting for Sub-varieties (Option C)

Once Option B is working, we can add tinting for sub-types within a material:

```c
typedef struct {
    const char* name;
    int spriteOffset;     // Which base sprite (planks, bricks, etc.)
    Color tint;           // Color variation within that sprite
    uint8_t flags;
    uint8_t fuel;
    ItemType dropsItem;
} MaterialDef;
```

This would allow:
- Oak, pine, birch → all use plank sprite, different brown tints
- Granite, marble, sandstone → all use brick sprite, different gray/white/tan tints

**For clean tinting of sub-varieties:**
- Make the base sprites (planks, bricks) in grayscale/white
- Tint applies the material color
- Black outlines stay black (0 × anything = 0)

---

## Open Questions

- [x] What do current sprites look like? → Small pixel art, black outlines, colored fills
- [x] Tinting with current sprites? → Would be muddy, need grayscale for clean results
- [ ] Which existing sprites map to which materials?
- [ ] Do we need new sprites for iron/glass/etc.?
- [ ] How to handle glass transparency?
- [ ] Do we want sub-types (oak/pine/birch) now or later? → Later

---

## Related Files

- `src/world/material.h` - MaterialDef struct
- `src/world/material.c` - materialDefs array with spriteOffset
- `src/render/rendering.c` - where cells are drawn
- `assets/atlas.h` - sprite indices
- Completed: `docs/done/architecture/data-driven-jobs-materials.md`
