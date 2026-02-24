# 04d2: Item Rot

> Extension of 04d-spoilage.md
> **Deps**: 04d (spoilage timer — done)
> **Opens**: Composting (09 loop closers), cleanliness/mood

---

## Goal

When food spoils, it becomes `ITEM_ROT` with a material category (meat rot vs plant rot). Rot can be burned as poor fuel, or hauled to a stockpile. A QoL shortcut lets players create a pre-filtered "refuse" stockpile in one action.

---

## Design Decisions (confirmed)

### Material: two rot categories
- `MAT_ROTTEN_MEAT` — from ITEM_CARCASS, ITEM_RAW_MEAT, ITEM_COOKED_MEAT
- `MAT_ROTTEN_PLANT` — from ITEM_BERRIES, ITEM_ROOT, ITEM_ROASTED_ROOT

Display: "Meat Rot", "Plant Rot" via IF_MATERIAL_NAME.

### Spawn: always on ground
When an item spoils, `DeleteItem` runs first (cleans up stockpile slot, container contentCount, reservations), then `SpawnItemWithMaterial` at the same position. Rot is always a fresh ground item — never spawned inside a container or stockpile slot.

### Stack count: inherited
15 berries spoil → ITEM_ROT x15 with MAT_ROTTEN_PLANT.

### Usage: burnable
`IF_FUEL` — can be burned in fire pit / charcoal pit immediately (zero new systems).

### Refuse pile = stockpile with rot filter
No new designation or job type needed. A refuse pile is just a stockpile with `allowedTypes[ITEM_ROT] = true` and everything else disabled. The existing `JOBTYPE_HAUL` handles pickup and delivery automatically.

### Rot is inert
No mood penalties, pest attraction, or contamination. Just takes up space. Future systems (personality, Tier 3) can add negative effects.

---

## Design

### ITEM_ROT definition
```c
[ITEM_ROT] = { "Rot", SPRITE_rot, IF_STACKABLE | IF_FUEL | IF_MATERIAL_NAME, 20, MAT_ROTTEN_PLANT, 0.5f, 0.0f }
```
- Not IF_EDIBLE, not IF_SPOILS
- Default material MAT_ROTTEN_PLANT (overridden by GetRotMaterial)

### Spoilage conversion in SpoilageTick
```c
float rx = items[i].x, ry = items[i].y, rz = items[i].z;
int count = items[i].stackCount;
uint8_t rotMat = GetRotMaterial(items[i].type);
DeleteItem(i);
int rot = SpawnItemWithMaterial(rx, ry, rz, ITEM_ROT, rotMat);
if (rot >= 0) items[rot].stackCount = count;
```

### Material mapping
```c
static uint8_t GetRotMaterial(ItemType source) {
    switch (source) {
        case ITEM_CARCASS:
        case ITEM_RAW_MEAT:
        case ITEM_COOKED_MEAT:
            return MAT_ROTTEN_MEAT;
        default:
            return MAT_ROTTEN_PLANT;
    }
}
```

### QoL: "Create refuse stockpile" shortcut
A single key press (e.g. Shift+P or a submenu option) creates a stockpile pre-filtered to only accept ITEM_ROT. Implementation: call `CreateStockpile()` then set `allowedTypes[i] = (i == ITEM_ROT)` for all types. Same as manually creating a stockpile and toggling filters, just faster.

---

## Implementation Phases

### Phase 1: ITEM_ROT + materials + conversion
1. Add `MAT_ROTTEN_MEAT`, `MAT_ROTTEN_PLANT` to MaterialType enum in `material.h`
2. Add material names in material name table
3. Add `ITEM_ROT` to ItemType enum in `items.h` (before ITEM_TYPE_COUNT)
4. Add itemDef row in `item_defs.c`: IF_STACKABLE | IF_FUEL | IF_MATERIAL_NAME
5. Add `GetRotMaterial()` helper in `items.c`
6. Change `SpoilageTick`: delete item → spawn ITEM_ROT with inherited stack count
7. Save version bump v71→v72:
   - New ItemType: `StockpileV71` with old `allowedTypes[ITEM_TYPE_COUNT]` (45 items)
   - New MaterialTypes: also migrate `allowedMaterials[old_MAT_COUNT]`
   - Migration: copy old arrays, default new types to `false`
8. Sprite: reuse placeholder or add rot sprite to 8x8 atlas
9. Add material-specific sprite mapping in `ItemSpriteForTypeMaterial()` if needed

**Tests (extend test_spoilage.c):**
```
describe(spoilage_rot_conversion)
  it("spoiled berries produce ITEM_ROT with MAT_ROTTEN_PLANT")
  it("spoiled raw meat produces ITEM_ROT with MAT_ROTTEN_MEAT")
  it("spoiled carcass produces ITEM_ROT with MAT_ROTTEN_MEAT")
  it("spoiled cooked meat produces ITEM_ROT with MAT_ROTTEN_MEAT")
  it("spoiled root produces ITEM_ROT with MAT_ROTTEN_PLANT")
  it("rot inherits stack count from spoiled item")
  it("rot spawns on ground even if original was in stockpile")
  it("rot spawns on ground even if original was in container")
  it("stockpile slot is freed when item spoils into rot")
  it("container contentCount decremented when item spoils into rot")
  it("rot is not edible")
  it("rot does not spoil further")
  it("rot has IF_FUEL flag")
  it("multiple items spoiling at same position produce separate rot items")
```

### Phase 2: Verify burning works
1. Confirm ITEM_ROT with IF_FUEL is accepted by existing fire pit / charcoal pit fuel logic
2. Test: spawn rot at fuel tile, verify workshop picks it up as fuel input

**Tests:**
```
describe(rot_as_fuel)
  it("rot item is accepted as workshop fuel")
  it("rot can be delivered to fire pit fuel tile")
```

### Phase 3: Refuse stockpile shortcut
1. Add a UI action to create a pre-filtered refuse stockpile
   - Key binding: in stockpile creation submenu (e.g. Draw > P for stockpile, Shift+P for refuse pile)
   - Or: a "preset" option when creating stockpiles
2. Implementation: `CreateStockpile()` → set `allowedTypes[i] = (i == ITEM_ROT)` for all i
3. Existing JOBTYPE_HAUL automatically picks up ground rot and delivers to this stockpile

**Tests:**
```
describe(refuse_stockpile)
  it("refuse stockpile only accepts ITEM_ROT")
  it("hauler delivers ground rot to refuse stockpile")
  it("hauler does not deliver non-rot items to refuse stockpile")
```

### Phase 4: E2E tests
```
describe(rot_e2e)
  it("full cycle: berries spoil → rot spawns → hauler carries to refuse stockpile")
  it("full cycle: meat spoils in stockpile → rot on ground → hauled to refuse")
  it("full cycle: food in container spoils → rot on ground outside container")
  it("rot at refuse stockpile can be picked up and burned in fire pit")
```

---

## Concerns

- **Save migration complexity**: both ITEM_TYPE_COUNT and MAT_COUNT change in one version bump. Need to migrate both `allowedTypes[]` and `allowedMaterials[]` arrays in Stockpile. This is a known pattern (done before in v50, v65, etc.) but requires care.
- **Sprite**: need rot sprites for 8x8 atlas. Can reuse a placeholder initially (e.g. SPRITE_gravel tinted brown).
- **Item spam**: if a stockpile full of food spoils simultaneously, lots of rot spawns at the same position. They won't auto-merge — each is a separate item. Haulers will consolidate them into stockpile stacks over time. Could batch-merge same-material rot at same tile in SpoilageTick, but probably not needed.
- **Container spoilage**: rot spawns on ground even if original was in container — container contentCount is already decremented by DeleteItem, so this is clean.
