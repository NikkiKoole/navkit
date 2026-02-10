# Workshops & Item Loops - Master Document

Date: 2026-02-09
Status: Living document - update as implementation progresses

---

## 0. Design Principles

Every addition must have:
1. **Source** - where does it come from?
2. **Sink** - where does it go?
3. **Feedback** - does it strengthen existing loops?

Prefer small complete loops over long incomplete chains.

### Current Limitations (What We Don't Have Yet)
- **Containers** - no inventory system, no "basket holds 10 berries"
- **Tools** - no equipment slots, no "worker uses axe to chop faster"
- **Durability** - items don't wear out or break
- **Quality** - no good/poor/excellent variants
- **Fuel over time** - workshops consume inputs per recipe, not continuous burn
- **Farming** - no crops, fertilizer use, or plowing
- **Animals** - no hunting, hides, or meat

### Future Markers
When documenting future ideas, mark dependencies:
- `[future:tools]` - requires tool/equipment system
- `[future:containers]` - requires inventory/container system
- `[future:farming]` - requires farming system
- `[needs:CELL_*]` - requires new cell type
- `[needs:ITEM_*]` - requires new item type

---

## 1. Currently Implemented

### Workshops

| Workshop | Size | Layout | Recipes |
|----------|------|--------|---------|
| Stonecutter | 3x3 | `###` `#XO` `..#` (dense) | Cut Blocks, Crush Gravel, Bind Gravel |
| Sawmill | 3x3 | `#O#` `.X.` `#..` (open lane) | Saw Planks, Cut Sticks |
| Kiln | 3x3 | `#F#` `#XO` `###` (enclosed) | Fire Bricks, Make Charcoal, Burn Peat |
| Charcoal Pit | 2x2 | `FX` `O.` | Char Logs, Char Peat, Char Sticks |

Legend: `#` = block, `X` = work tile, `O` = output, `F` = fuel, `.` = floor

### All Recipes

| Workshop | Recipe | Input | Output | Time | Fuel |
|----------|--------|-------|--------|------|------|
| Stonecutter | Cut Stone Blocks | ROCK x1 | BLOCKS x2 | 3s | - |
| Stonecutter | Crush Gravel | ROCK x1 | GRAVEL x3 | 2s | - |
| Stonecutter | Bind Gravel | GRAVEL x2 + CLAY x1 | BLOCKS x1 | 4s | - |
| Sawmill | Saw Planks | LOG x1 | PLANKS x4 | 4s | - |
| Sawmill | Cut Sticks | LOG x1 | STICKS x8 | 2s | - |
| Kiln | Fire Bricks | CLAY x1 | BRICKS x2 | 5s | 1 |
| Kiln | Make Charcoal | LOG x1 | CHARCOAL x3 | 6s | - |
| Kiln | Burn Peat | PEAT x1 | CHARCOAL x3 | 5s | - |
| Charcoal Pit | Char Logs | LOG x1 | CHARCOAL x2 | 8s | - |
| Charcoal Pit | Char Peat | PEAT x1 | CHARCOAL x2 | 7s | - |
| Charcoal Pit | Char Sticks | STICKS x4 | CHARCOAL x1 | 5s | - |

### Items & Flags

| Item | Flags | Source | Sinks |
|------|-------|--------|-------|
| ROCK | stackable | Mining | Stonecutter |
| BLOCKS | stackable, building | Stonecutter | Construction |
| GRAVEL | stackable | Stonecutter, Mining | Stonecutter (Bind) |
| LOG | stackable, building, fuel | Chopping trees | Sawmill, Kiln, Charcoal Pit |
| PLANKS | stackable, building, fuel | Sawmill | Construction |
| STICKS | stackable, fuel | Sawmill | Charcoal Pit |
| CHARCOAL | stackable, fuel | Kiln, Charcoal Pit | (fuel only) |
| CLAY | stackable | Mining clay | Kiln, Stonecutter (Bind) |
| BRICKS | stackable, building | Kiln | Construction |
| PEAT | stackable, fuel | Mining peat | Kiln, Charcoal Pit |
| SAND | stackable | Mining sand | (none yet) |
| DIRT | stackable | Mining dirt | (none yet) |
| LEAVES_* | stackable, fuel | Trees | (fuel only) |
| SAPLING_* | stackable | Gathering | Planting |

---

## 2. Item Loops Analysis

### Stone Loop (CLOSED)
```
Mining → ROCK → Stonecutter → BLOCKS → Construction
              ↘ Stonecutter → GRAVEL ↘
                              + CLAY → BLOCKS
```

### Wood Loop (PARTIAL)
```
Trees → LOG → Sawmill → PLANKS → Construction
           ↘ Sawmill → STICKS → Charcoal Pit → CHARCOAL
           ↘ Kiln/Pit → CHARCOAL
     → LEAVES_* → (fuel only, no recipe sink)
     → SAPLING_* → Planting (regrows trees)
```

### Clay Loop (CLOSED)
```
Mining → CLAY → Kiln → BRICKS → Construction
             ↘ Stonecutter (Bind Gravel) → BLOCKS
```

### Fuel Loop (OPEN - no consumer)
```
Sources: LOG, PLANKS, STICKS, CHARCOAL, PEAT, LEAVES_*
Sink: Kiln (Fire Bricks uses 1 fuel)
Missing: General fuel consumer (Hearth)
```

---

## 3. Open Loops / Missing Sinks

| Item | Problem | Proposed Solution |
|------|---------|-------------------|
| LEAVES_* | IF_FUEL but no recipe | Hearth consumes → ASH; or Compost → MULCH |
| SAND | No use | Glass Kiln: SAND + fuel → GLASS |
| DIRT | No use | Farming (soil for crops) |
| CHARCOAL | Only fuel sink | Hearth, Bloomery (smelting) |
| STICKS | Limited use | Crafting bench: tools, baskets |

---

## 4. Proposed: Bark & Cordage Chain

Inspired by [Primitive Technology](https://primitivetechnology.wordpress.com/).

### New Items
- `BARK` - stripped from logs
- `STRIPPED_LOG` - log after bark removal
- `CORDAGE` - rope/fiber from bark

### New Recipes
| Workshop | Recipe | Input | Output |
|----------|--------|-------|--------|
| Sawmill | Strip Bark | LOG x1 | STRIPPED_LOG x1 + BARK x2 |
| Sawmill | Saw Stripped | STRIPPED_LOG x1 | PLANKS x5 | (bonus yield)
| Rope Maker | Twist Cordage | BARK x2 | CORDAGE x1 |
| Rope Maker | Leaf Cordage | LEAVES x4 | CORDAGE x1 |

### Updated Wood Loop
```
Trees → LOG → Sawmill (strip) → STRIPPED_LOG → PLANKS (x5)
                             ↘ BARK → Rope Maker → CORDAGE
     → LEAVES_* → Rope Maker → CORDAGE
                ↘ Hearth → ASH
```

### Implementation Requirements (2026-02-10)

**Current Code State:**
- ✅ ITEM_GRASS, ITEM_DRIED_GRASS, ITEM_POLES already exist
- ✅ Sawmill workshop exists with 2 recipes (Saw Planks, Cut Sticks)
- ✅ Drying Rack workshop exists (pure passive, dries grass)
- ✅ Recipe struct supports two inputs via `inputType2`, `inputCount2`
- ❌ ITEM_BARK, ITEM_STRIPPED_LOG, ITEM_CORDAGE **do not exist yet**
- ❌ WORKSHOP_ROPE_MAKER **does not exist yet**
- ⚠️ Recipe struct does **not support multiple outputs** (one output type only)

**Implementation Checklist:**

1. **Add new item types** to `src/entities/items.h` enum:
   ```c
   ITEM_BARK,          // Stripped from logs at sawmill
   ITEM_STRIPPED_LOG,  // Log after bark removal
   ITEM_CORDAGE,       // Rope/fiber for lashing, construction
   ```

2. **Add item definitions** to `src/entities/item_defs.c` table:
   ```c
   [ITEM_BARK]         = { "Bark",         SPRITE_bark,  IF_STACKABLE, 99 },
   [ITEM_STRIPPED_LOG] = { "Stripped Log", SPRITE_log,   IF_STACKABLE|IF_BUILDING_MAT|IF_FUEL, 99 },
   [ITEM_CORDAGE]      = { "Cordage",      SPRITE_rope,  IF_STACKABLE, 99 },
   ```

3. **Handle multi-output recipe** (Strip Bark produces 2 items):
   - **Option A**: Extend Recipe struct to support `outputType2`, `outputCount2`
   - **Option B**: Spawn second output manually in craft job completion code (CRAFT_STEP_FINISH)
   - **Recommended**: Option B for now (minimal change, avoids struct bloat)

4. **Add Sawmill recipes** to `src/entities/workshops.c`:
   ```c
   { "Strip Bark",   ITEM_LOG,         1, ITEM_NONE, 0, ITEM_STRIPPED_LOG, 1, 4.0f, 0, ... },
   // (BARK x2 spawned manually in job code)
   { "Saw Stripped", ITEM_STRIPPED_LOG, 1, ITEM_NONE, 0, ITEM_PLANKS,      5, 4.0f, 0, ... },
   ```

5. **Add WORKSHOP_ROPE_MAKER** enum to `src/entities/workshops.h`

6. **Define Rope Maker workshop** in `src/entities/workshops.c`:
   ```c
   Recipe ropeMakerRecipes[] = {
       { "Twist Cordage", ITEM_BARK,        2, ITEM_NONE, 0, ITEM_CORDAGE, 1, 3.0f, 0, ... },
       { "Leaf Cordage",  ITEM_LEAVES_OAK,  4, ITEM_NONE, 0, ITEM_CORDAGE, 1, 4.0f, 0, ... },
       // Note: may need ITEM_MATCH_ANY_LEAVES to accept all leaf types
   };

   [WORKSHOP_ROPE_MAKER] = {
       .type = WORKSHOP_ROPE_MAKER,
       .name = "ROPE_MAKER",
       .displayName = "Rope Maker",
       .width = 2,
       .height = 2,
       .template = "#X"
                   "O.",
       .recipes = ropeMakerRecipes,
       .recipeCount = sizeof(ropeMakerRecipes) / sizeof(ropeMakerRecipes[0])
   }
   ```

7. **Modify craft job code** (`src/entities/jobs.c`) to spawn bonus BARK when Strip Bark recipe completes

8. **Add input actions** for placing Rope Maker workshop (follow existing workshop placement pattern)

9. **Bump SAVE_VERSION** and add migration for new items (if save/load implemented)

10. **Add stockpile filter support** for new items (BARK, STRIPPED_LOG, CORDAGE)

**Technical Challenges:**
- **Multi-output recipes**: Current Recipe struct only has `outputType` + `outputCount` (single output). Strip Bark needs to produce BOTH stripped_log AND bark. Two approaches:
  1. Special-case in job completion (check recipe name, spawn extra items)
  2. Extend Recipe struct with optional second output (cleaner but touches more code)

- **Leaf type matching**: "Leaf Cordage" should accept ANY leaf type (oak/pine/birch/willow). May need new `ITEM_MATCH_ANY_LEAVES` flag, or 4 separate recipes.

**Uses for CORDAGE (future):**
- Construction staging: frames require cordage as lashing material
- Multi-stage builds: shelter = poles + cordage + thatch
- Tool crafting: stone axe = rock + sticks + cordage [future:tools]

---

## 5. Proposed: Hearth (Fuel Consumer)

The missing fuel sink. Burns any IF_FUEL item, produces ASH.

### Workshop Definition
- **Size**: 2x2
- **Layout**: `FX` / `O.`
- **Recipes**:
  - Burn Fuel: ANY_FUEL x1 → ASH x1 (+ heat + smoke)

### ASH Uses
| Workshop | Recipe | Input | Output |
|----------|--------|-------|--------|
| Stonecutter | Ash Cement | ASH x2 + CLAY x1 | BLOCKS x2 |
| (Farming) | Fertilize | ASH x1 | soil bonus |

---

## 6. Next Workshops (Priority Order)

### Tier 1: Minimal New Systems
1. **Hearth** (2x2) - fuel sink, produces ASH
2. **Rope Maker** (2x2) - BARK/LEAVES → CORDAGE

### Tier 2: One New Item Each
3. **Glass Kiln** (3x3) - SAND + fuel → GLASS
4. **Pottery Wheel** (2x2) - CLAY → POTTERY

### Tier 3: Require New Systems
5. **Bloomery** (3x3) - IRON_ORE + fuel → IRON_BLOOM
6. **Loom** (2x3) - FIBER → CLOTH/ROPE
7. **Tanner's Rack** (2x2) - HIDE → LEATHER
8. **Hand Mill** (2x2) - GRAIN → FLOUR

### Additional Workshops (Lower Priority)
- **Mason's Bench** (3x3) - DIRT + GRAVEL → PACKED_EARTH, CLAY + STICKS → WATTLE_DAUB
- **Fiber Works** (2x3) - LEAVES/GRASS → PLANT_FIBER → ROPE/CLOTH
- **Weaving Bench** (2x2) - WITHIES → BASKETS, GRASS → MATS [needs:ITEM_WITHIES]
- **Toolmaker's Bench** (3x3) - ROCK → STONE_TOOL, + STICKS + ROPE → AXE/PICKAXE [future:tools]

---

## 7. Future Systems (From Vision Docs)

### Withies & Weaving
Flexible young branches (especially willow) for basket weaving:
- Source: Harvest from willow trees → ITEM_WITHIES
- Sink: Weaving Bench → BASKETS, WICKER_FENCE, FISH_TRAP

### Tree Stumps & Coppicing
After chopping a tree, stump remains:
- **CELL_TREE_STUMP** - walkable, blocks construction
- **Leave stump** → tree regrows faster (coppicing, ~50% time)
- **Uproot job** → ITEM_ROOTS + cleared ground
- ROOTS → Fiber Works (strong cordage), fuel, tannin [future:animals]

### New Cells Needed
| Cell | Source | Notes |
|------|--------|-------|
| CELL_TALL_GRASS | Terrain gen | Harvestable for fiber |
| CELL_TREE_STUMP | After chopping | Coppicing or uproot |

### New Items Needed (Summary)
| Item | Source | Sink | Notes |
|------|--------|------|-------|
| BARK | Sawmill (strip log) | Rope Maker → CORDAGE | |
| STRIPPED_LOG | Sawmill | Sawmill → PLANKS (bonus) | |
| CORDAGE | Rope Maker | Construction, crafting | |
| ASH | Hearth | Cement, fertilizer | |
| GLASS | Glass Kiln | Windows, containers | |
| POTTERY | Pottery Wheel | Trade good | [future:containers] |
| WITHIES | Harvest willow | Weaving Bench | |
| PLANT_FIBER | Fiber Works | Rope, cloth | |
| ROOTS | Uproot stump | Fiber, fuel | [needs:CELL_TREE_STUMP] |

---

## 8. Open Questions

- Should **Hearth** be a workshop or a single-tile structure?
- Should **Glass** be a new item or a material variant of BLOCKS?
- For iron: **iron bacteria sludge** (biome-tied) or classic **ore nodes**?
- Should **fuel tile (F)** be functional (deposit fuel there) or cosmetic (smoke anchor)?
- Add **input tile (I)** for visible log decks / clay piles?

---

## 9. Technical Notes

### Recipe System Features
- Single input + output (standard)
- Two inputs (multi-input): `inputType2`, `inputCount2` fields
- Fuel requirement: `fuelRequired` field (any IF_FUEL item)
- Material preservation: output inherits input material

### Workshop System Features
- Variable sizes via `WorkshopDef.width/height`
- Tile types: `#` block, `X` work, `O` output, `F` fuel, `.` floor
- Distinct layouts per workshop (visual identity)
- No input tile yet (inputs fetched from nearby items/stockpiles)
- No orientation/rotation yet (fixed layouts)

### Job System (Craft)
- 10-step state machine
- Supports: first input → second input → fuel → work
- Reserves all items before starting

---

## 10. Archived/Moved Docs

- `docs/archive/workshop-roadmap.md` - merged into sections 6-8
- `docs/archive/workshops-current-design.md` - merged into section 1
- `docs/vision/workshops-and-jobs.md` - long-term vision doc (fiber, tools, stumps)

---

## Changelog

- 2026-02-09: Created master doc, consolidating roadmap + current-design
- 2026-02-09: Added Charcoal Pit (2x2), multi-input recipes, distinct layouts
- 2026-02-09: Added Bind Gravel (GRAVEL+CLAY→BLOCKS), Crush Gravel recipes
- 2026-02-09: Proposed bark/cordage chain, Hearth workshop
- 2026-02-09: Added design principles, limitations, future markers from workshops-and-jobs
- 2026-02-09: Added withies, tree stumps/coppicing, additional workshops, open questions
- 2026-02-10: Added implementation requirements for bark/cordage chain (code audit findings)
