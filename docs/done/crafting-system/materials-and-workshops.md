# Materials and Workshops

How raw materials flow through workshops into usable items.

---

## Two Resource Economies

### Extractive (Mining) - Finite
You consume the world permanently. Strategic decisions about what to dig.

```
STONE_WALL ──(mine)──→ ROUGH_STONE + STONE_FLOOR (cell)
STONE_FLOOR ──(dig)──→ FLOOR_STONE + DIRT (cell)
```

### Renewable (Forestry) - Cyclical
Trees regrow over time. Sustainable if you manage your forest.

```
TREE ──(chop)──→ LOGS + GRASS (tree gone)
                    ↓ (time passes)
                 TREE regrows on grass
```

---

## Raw Materials

| Material | Source | Renewable? |
|----------|--------|------------|
| ROUGH_STONE | Mining walls | No |
| FLOOR_STONE | Digging stone floors | No |
| LOGS | Chopping trees | Yes (regrows) |
| STICKS | Gathering brush/bushes | Yes (regrows) |

---

## Workshops

### 1. Stonecutter

Turns raw stone into usable blocks.

**Inputs:** ROUGH_STONE, FLOOR_STONE
**Outputs:** STONE_BLOCKS

| Recipe | Input | Output | Work Time |
|--------|-------|--------|-----------|
| Cut Stone Blocks | 1 ROUGH_STONE | 2 STONE_BLOCKS | 3s |
| Cut Floor Stone | 1 FLOOR_STONE | 2 STONE_BLOCKS | 3s |

**Bills examples:**
- "Make stone blocks forever" (BILL_DO_FOREVER)
- "Make stone blocks until 50" (BILL_DO_UNTIL_X)

---

### 2. Sawmill

Turns logs into planks.

**Inputs:** LOGS
**Outputs:** PLANKS, SAWDUST (byproduct fuel?)

| Recipe | Input | Output | Work Time |
|--------|-------|--------|-----------|
| Cut Planks | 1 LOGS | 4 PLANKS | 2s |

**Bills examples:**
- "Make planks until 30" (BILL_DO_UNTIL_X)
- "Make planks forever" (BILL_DO_FOREVER)

---

### 3. Fuel Prep (optional, could be part of Sawmill)

Prepares fuel for fires.

**Inputs:** STICKS, SAWDUST, PLANKS
**Outputs:** FUEL

| Recipe | Input | Output | Work Time |
|--------|-------|--------|-----------|
| Bundle Sticks | 3 STICKS | 1 FUEL | 1s |
| Gather Sawdust | 2 SAWDUST | 1 FUEL | 1s |
| Split Planks | 1 PLANKS | 2 FUEL | 2s |

Or simpler: STICKS and LOGS burn directly, no fuel prep needed.

---

## Crafted Materials

| Material | Crafted From | Used For |
|----------|--------------|----------|
| STONE_BLOCKS | Stonecutter | Build stone walls, stone floors |
| PLANKS | Sawmill | Build wood walls, wood floors, furniture |
| FUEL | Fuel prep / raw sticks | Fire, cooking, smelting (future) |

---

## Building Costs

### Walls
| Building | Material | Cost |
|----------|----------|------|
| Stone Wall | STONE_BLOCKS | 2 |
| Wood Wall | PLANKS | 2 |

### Floors
| Building | Material | Cost |
|----------|----------|------|
| Stone Floor | STONE_BLOCKS | 1 |
| Wood Floor | PLANKS | 1 |

### Differences
| Property | Stone | Wood |
|----------|-------|------|
| Build speed | Slow | Fast |
| Durability | High | Medium |
| Flammable | No | Yes |
| Look | Blue/gray | Brown |

---

## Complete Material Flows

### Stone Flow
```
STONE_WALL
    │ mine
    ▼
ROUGH_STONE (item) ─────────────────┐
    +                               │
STONE_FLOOR (cell)                  │
    │ dig (optional)                ▼
    ▼                          [STONECUTTER]
FLOOR_STONE (item) ─────────────────┤
    +                               │
DIRT (cell)                         ▼
                              STONE_BLOCKS
                                    │
                    ┌───────────────┼───────────────┐
                    ▼               ▼               ▼
              Stone Wall      Stone Floor      (future: forge, etc)
```

### Wood Flow
```
TREE (overlay on grass)
    │ chop
    ▼
LOGS (item) ──────────────────┐
    +                         │
GRASS (cell, tree removed)    │
    │ (time)                  ▼
    ▼                    [SAWMILL]
TREE regrows                  │
                              ▼
                           PLANKS ──────────┬──────────┐
                              +             │          │
                           SAWDUST?         ▼          ▼
                              │        Wood Wall  Wood Floor
                              ▼
                            FUEL (for fires)

BRUSH/BUSH (overlay on grass)
    │ gather
    ▼
STICKS (item) ───────────────→ FUEL (direct, or bundled)
    +
GRASS (bush removed, regrows)
```

---

## Workshop Summary

| Workshop | Input(s) | Output(s) | Purpose |
|----------|----------|-----------|---------|
| Stonecutter | ROUGH_STONE, FLOOR_STONE | STONE_BLOCKS | Stone construction |
| Sawmill | LOGS | PLANKS (+SAWDUST?) | Wood construction |
| Fuel Prep | STICKS, SAWDUST | FUEL | Fire/cooking | 

**Note:** Could start with just Stonecutter + Sawmill. Fuel prep might be overkill - sticks and logs could burn directly.

---

## Implementation Priority

Start simple, prove the system, then expand.

---

### Phase 1: Orange Loop (CURRENT FOCUS)

The simplest possible crafting loop using existing items.

```
Mine wall → ITEM_ORANGE → [Stonecutter] → ITEM_STONE_BLOCKS → Build wall
```

**What exists:**
- Mining walls spawns ITEM_ORANGE ✓
- Hauling to stockpiles ✓
- Building walls (currently uses ITEM_ORANGE directly) ✓

**What to add:**
1. Add `ITEM_STONE_BLOCKS` (new item type)
2. Add Stonecutter workshop (hardcoded position for testing)
3. Add crafting job system (JOBTYPE_CRAFT, driver, WorkGiver)
4. Add recipe: 1 ITEM_ORANGE → 2 ITEM_STONE_BLOCKS
5. Change wall building to require ITEM_STONE_BLOCKS instead of ITEM_ORANGE
6. Add bill system (start with BILL_DO_FOREVER)

**Success criteria:**
- Mover mines wall → ORANGE spawns
- Hauler takes ORANGE to stockpile near stonecutter
- Crafter walks to stonecutter, crafts, STONE_BLOCKS spawn
- Hauler takes STONE_BLOCKS to construction stockpile
- Builder uses STONE_BLOCKS to build wall

**This proves:** Workshop placement, crafting jobs, bill system, material transformation, hauling integration.

---

### Phase 2: Floor Loop

Extend the stone loop with floor harvesting.

```
Mine wall → ITEM_ORANGE + STONE_FLOOR (cell left behind)
Dig floor → ITEM_FLOOR_STONE + DIRT (cell left behind)
Both items → [Stonecutter] → ITEM_STONE_BLOCKS → Build wall OR floor
```

**What to add:**
1. Mining leaves STONE_FLOOR cell (instead of current blue floor?)
2. New designation: "Dig floor" (like mine, but for floors)
3. Add `ITEM_FLOOR_STONE` (or reuse ORANGE?)
4. Stonecutter accepts both ORANGE and FLOOR_STONE
5. Add "Build stone floor" option (costs STONE_BLOCKS)

**This proves:** Two-stage digging, floor as harvestable resource, multiple inputs to same workshop.

---

### Phase 3: Wood Loop

Add renewable resources via trees.

```
Chop tree → ITEM_LOGS → [Sawmill] → ITEM_PLANKS → Build wood wall/floor
Tree regrows over time
```

**What to add:**
1. TREE overlay on grass tiles (like existing grass overlay system)
2. New designation: "Chop tree"
3. Woodcutter job (similar to mining)
4. Tree regrowth simulation (similar to grass spreading)
5. Add `ITEM_LOGS`, `ITEM_PLANKS`
6. Add Sawmill workshop
7. Recipe: 1 LOGS → 4 PLANKS
8. Add wood wall / wood floor building options

**This proves:** Renewable resources, regrowth simulation, second workshop type, material variety.

---

### Phase 4: Fuel & Fire Integration

Connect wood to existing fire system.

1. STICKS from brush/bushes (or just use LOGS)
2. Fire/firepit consumes fuel items
3. Cooking workshop requires nearby fire (future)

---

## Vertical Slice: Phase 1

Minimal playable loop to validate crafting system.

**Setup:**
- Small map with some walls to mine
- Pre-placed Stonecutter workshop
- Stockpile for ORANGE near stonecutter (input)
- Stockpile for STONE_BLOCKS (output)
- Blueprint for wall requiring STONE_BLOCKS
- A few movers (miners, haulers, crafters, builders)

**Player does:**
1. Designate walls to mine (M+drag - existing)
2. Watch the loop run automatically

**Expected flow:**
```
Miner mines wall
    → ORANGE spawns on ground
Hauler picks up ORANGE
    → delivers to input stockpile
Crafter sees bill "make blocks forever"
    → walks to stonecutter
    → consumes ORANGE from nearby
    → crafts for 3 seconds
    → STONE_BLOCKS spawn at output
Hauler picks up STONE_BLOCKS  
    → delivers to construction stockpile
Builder sees blueprint needs STONE_BLOCKS
    → hauls STONE_BLOCKS to blueprint
    → builds wall

Loop continues until walls exhausted or blueprint done.
```

**What we learn:**
- Does the crafting job system work?
- Does material flow feel right?
- Are there bottlenecks? (not enough haulers? crafter idle?)
- Is the bill system working?

---

## Item Types (Updated)

```c
typedef enum {
    // Raw from mining
    ITEM_ROUGH_STONE,       // From mining walls
    ITEM_FLOOR_STONE,       // From digging stone floors
    
    // Raw from forestry  
    ITEM_LOGS,              // From chopping trees
    ITEM_STICKS,            // From gathering brush
    
    // Crafted
    ITEM_STONE_BLOCKS,      // From stonecutter
    ITEM_PLANKS,            // From sawmill
    ITEM_FUEL,              // From fuel prep (or raw sticks work)
    
    // Legacy (keep for testing?)
    ITEM_RED,
    ITEM_GREEN,
    ITEM_BLUE,
    
    ITEM_TYPE_COUNT,
} ItemType;
```

---

## Decisions Made

**Do we keep ITEM_ORANGE or rename to ITEM_ROUGH_STONE?**
→ Keep **ITEM_ORANGE**. Works fine, no need to rename.

## Future Questions (Phase 3+)

These are for later when we add wood/forestry:

- [ ] Sawdust as byproduct, or keep sawmill simple (logs → planks only)?
- [ ] Can raw LOGS/STICKS be fuel, or require processing?
- [ ] Tree regrowth time - seconds? minutes? gameplay balance
- [ ] Brush separate from trees, or trees drop both logs + sticks?
- [ ] Wood walls flammable - does fire spread to them?
