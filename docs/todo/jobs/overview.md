# Jobs System Overview

This document ties together all job-related design documents with the autarky philosophy. Use this as a map to understand how the pieces fit together.

---

## Guiding Principle: Autarky

See `docs/vision/autarky.md`

Every system element must participate in a closed loop:
- **Source** - where does it come from?
- **Sink** - where does it go?
- **Feedback** - does it strengthen existing loops?

**Rule:** If an element lacks a source or sink, defer it until the loop can be completed.

This applies to jobs too: a job that produces an item with no use is wasted work. A job that consumes something we can't produce creates a dead end.

---

## Document Map

```
docs/vision/autarky.md          <- Core philosophy (closed loops)
       |
       v
docs/todo/jobs/
├── overview.md                 <- YOU ARE HERE
├── jobs-roadmap.md             <- Full architecture & phased implementation
├── jobs-checklist.md           <- Pending work items
├── jobs-from-terrain.md        <- How terrain generates jobs
├── bill-modes.md               <- Workshop bill system (Do X times, Do forever, etc.)
├── zone-designations.md        <- Zone types (Gather, Clean, Dump)
├── zone-activities.md          <- How zones generate jobs
└── instant-reassignment.md     <- Immediate job pickup on completion

docs/todo/workshops-and-jobs.md <- Production chains & new workshops
```

---

## The Big Picture

### What Is a Job?

A job is a unit of work a mover can perform. Jobs connect **terrain**, **items**, and **workshops** into production loops.

```
TERRAIN (cells) --[terrain jobs]--> ITEMS --[workshop jobs]--> ITEMS --[construction jobs]--> TERRAIN
       ^                                                                                          |
       +------------------------------[closed loop]-----------------------------------------------+
```

### Job Categories

| Category | Examples | Creates Loop By |
|----------|----------|-----------------|
| **Extraction** | Mine, Chop, Gather | Terrain -> Items |
| **Processing** | Craft at workshop | Items -> Items |
| **Construction** | Build wall, floor | Items -> Terrain |
| **Maintenance** | Haul, Clean, Tend | Keeps loops flowing |

---

## How Jobs Are Generated

Jobs come from three sources:

### 1. Designations (Player Marks Terrain)

Player designates cells -> system creates jobs.

From `zone-designations.md`:
- **Gather Zone** - Gather items from terrain (leaves, grass, saplings)
- **Cleaning Zone** - Haul debris away
- **Dumping Zone** - Where debris goes

From `jobs-from-terrain.md`:
- Mine designation -> Mining job
- Chop designation -> Chopping job
- Channel designation -> Channeling job

### 2. Workshops (Bills Create Jobs)

Player sets bills at workshop -> system creates crafting jobs.

From `bill-modes.md`:
| Mode | Behavior |
|------|----------|
| Do X times | Craft exactly X, then stop |
| Do until X | Maintain stockpile of X |
| Do forever | Repeat indefinitely (useful for Firepit fuel burn) |

Workshop jobs follow the pattern:
1. Check inputs available
2. Worker walks to workshop
3. Worker performs work
4. Output appears at output tile
5. Bill updates count

### 3. Zones (Ongoing Activities)

From `zone-activities.md`:
Zones can generate jobs continuously, not just once.

Example: Dumping zone generates haul jobs whenever items need disposal.

---

## Job Execution Flow

From `jobs-roadmap.md`:

```
Job Pool
   |
   v
WorkGivers (prioritized list)
   |
   v
MoverCapabilities (can this mover do it?)
   |
   v
Job Drivers (execute the job)
   |
   v
Completion -> instant-reassignment.md (immediately check for next job)
```

### Key Insight: Instant Reassignment

From `instant-reassignment.md`:

When a mover finishes a job, immediately assign the next one. No idle ticks. This keeps production loops flowing smoothly.

---

## Terrain -> Jobs -> Items

From `jobs-from-terrain.md`:

| Terrain Cell | Job | Yields |
|--------------|-----|--------|
| CELL_WALL | Mine | ITEM_ROCK |
| CELL_DIRT | Dig | ITEM_DIRT |
| CELL_CLAY | Mine | ITEM_CLAY |
| CELL_GRAVEL | Mine | ITEM_GRAVEL |
| CELL_SAND | Mine | ITEM_SAND |
| CELL_PEAT | Mine | ITEM_PEAT |
| CELL_TREE_TRUNK | Chop | ITEM_WOOD |
| CELL_TREE_LEAVES | Gather | ITEM_LEAVES_* |
| CELL_SAPLING | Gather | ITEM_SAPLING_* |
| CELL_TALL_GRASS | Gather | ITEM_TALL_GRASS | [needs:CELL_TALL_GRASS]

**Autarky check:** Every cell that yields an item needs that item to have a sink.

---

## Items -> Workshops -> Items

From `workshops-and-jobs.md`:

| Workshop | Input | Output |
|----------|-------|--------|
| Stonecutter | ITEM_ROCK | ITEM_STONE_BLOCKS |
| Sawmill | ITEM_WOOD | ITEM_PLANKS, ITEM_STICKS |
| Kiln | ITEM_CLAY + fuel | ITEM_BRICKS |
| Kiln | ITEM_WOOD | ITEM_CHARCOAL |
| Fiber Works | ITEM_LEAVES_* | ITEM_PLANT_FIBER |
| Weaving Bench | ITEM_WITHIES | ITEM_WOVEN_BASKET |

**Bill modes matter:** 
- Kiln uses fuel-per-recipe (current system supports this)
- Firepit could use "Do forever" bill for continuous burning

---

## Items -> Construction -> Terrain

Construction jobs close the loop by turning items back into terrain:

| Item | Construction Job | Result |
|------|------------------|--------|
| ITEM_STONE_BLOCKS | Build Stone Wall | CELL_STONE_WALL |
| ITEM_PLANKS | Build Wood Wall | CELL_WOOD_WALL |
| ITEM_BRICKS | Build Brick Wall | CELL_BRICK_WALL |
| ITEM_GRAVEL | Build Path | CELL_PATH |

**Autarky achieved:** Terrain -> Items -> Terrain

---

## Special Job Types

### Hauling

Connects all other jobs. Moves items between:
- Extraction site -> Stockpile
- Stockpile -> Workshop input
- Workshop output -> Stockpile
- Stockpile -> Construction site

Without hauling, loops stall.

### Tend Fire / Load Kiln

From `workshops-and-jobs.md`:

These are hauling jobs with special behavior:
- **Load Kiln**: Haul fuel to kiln input tile (kiln consumes fuel per recipe)
- **Tend Fire**: Haul fuel to firepit (uses "Do forever" bill for continuous burn)

Connection to `bill-modes.md`: "Do forever" bill enables continuous processes like heating without new "fuel over time" mechanics.

### Farming Jobs (Future)

From `jobs-from-terrain.md`:

| Job | Input | Output |
|-----|-------|--------|
| Plow | CELL_DIRT | CELL_PLOWED_DIRT |
| Sow | CELL_PLOWED_DIRT + seed | CELL_CROP_* |
| Harvest | CELL_CROP_* | ITEM_* + CELL_PLOWED_DIRT |

[future:farming] - requires farming system

---

## Open Loops (Autarky Violations)

Jobs that produce items with no sink:

| Job Output | Missing Sink | Blocked By |
|------------|--------------|------------|
| ITEM_ASH | Fertilizer, soap | [future:farming], [future:animals] |
| ITEM_MULCH | Fertilizer | [future:farming] |
| ITEM_AXE | Tool use | [future:tools] |
| ITEM_PICKAXE | Tool use | [future:tools] |
| ITEM_TANNIN | Leather tanning | [future:animals] |
| ITEM_MEDICINE | Healing | [future:medical] |

**Recommendation:** Don't implement jobs that produce these until sinks exist.

---

## Implementation Phases

From `jobs-roadmap.md` and `workshops-and-jobs.md`:

### Phase 1: Core Jobs (Exists)
- Mining, Digging, Channeling
- Chopping trees
- Hauling
- Basic crafting (Stonecutter)

### Phase 2: Wood & Fire Processing
- Sawmill workshop + recipes
- Kiln workshop + fuel system
- Load Kiln job

### Phase 3: Zones & Bills
- Zone designations (Gather, Clean, Dump)
- Bill modes (Do X times, Do until X, Do forever)
- Zone-generated jobs

### Phase 4: Fiber & Textiles
- Gather Tall Grass job [needs:CELL_TALL_GRASS]
- Fiber Works workshop
- Weaving Bench workshop

### Phase 5: Forestry Depth
- CELL_TREE_STUMP after chopping
- Uproot job -> ITEM_ROOTS
- Coppicing (regrowth from stump)

### Phase 6: Tools (Future)
- Toolmaker's Bench
- Tool effects on job speed
- Tool durability

---

## Checklist Cross-Reference

From `jobs-checklist.md`:

| Item | Status | Related Docs |
|------|--------|--------------|
| Bill modes | Pending | `bill-modes.md` |
| Zone activities | Pending | `zone-activities.md`, `zone-designations.md` |
| Farming jobs | Future | `jobs-from-terrain.md` |
| Instant reassignment | Pending | `instant-reassignment.md` |

---

## Key Design Decisions

### 1. Jobs Follow Autarky

Before adding a job, verify:
- Does it produce something with a sink?
- Does it consume something with a source?

### 2. Bill Modes Enable Flexibility

"Do forever" bill mode allows continuous processes (Firepit) without new mechanics.

### 3. Zones Generate Jobs

Zones are not just storage areas. They actively generate work (gathering, cleaning, hauling).

### 4. Instant Reassignment Keeps Loops Flowing

No idle ticks between jobs. Production chains stay active.

### 5. Terrain Is Part of the Loop

Terrain is both source (mining yields items) AND sink (construction consumes items).

---

## Quick Reference: Closed Loops

### Complete Loops (Ready to Implement)

```
Stone:    CELL_WALL -> mine -> ITEM_ROCK -> stonecutter -> ITEM_STONE_BLOCKS -> build -> CELL_STONE_WALL

Wood:     CELL_TREE_TRUNK -> chop -> ITEM_WOOD -> sawmill -> ITEM_PLANKS -> build -> CELL_WOOD_WALL

Brick:    CELL_CLAY -> mine -> ITEM_CLAY -+
          CELL_TREE_TRUNK -> chop -> ITEM_WOOD (fuel) -+-> kiln -> ITEM_BRICKS -> build -> CELL_BRICK_WALL

Charcoal: CELL_TREE_TRUNK -> chop -> ITEM_WOOD -> kiln -> ITEM_CHARCOAL -> fuel for kiln (self-sustaining)

Fiber:    CELL_TREE_LEAVES -> gather -> ITEM_LEAVES_* -> fiber works -> ITEM_PLANT_FIBER -> ITEM_ROPE -> construction
```

### Open Loops (Defer)

```
Ash:      ITEM_WOOD -> firepit -> ITEM_ASH -> ??? [needs:farming]

Tools:    ITEM_ROCK + ITEM_STICKS -> toolmaker -> ITEM_AXE -> ??? [needs:tools system]

Tanning:  ITEM_ROOTS -> tanning vat -> ITEM_TANNIN -> ??? [needs:animals for hides]
```

---

## What's Next: Priority Order

### The Bottleneck: Bill Modes

**Bill modes are the critical enabler.** Without them, workshops can only do one-off crafting. With them, everything opens up:

| Bill Mode | Enables |
|-----------|---------|
| Do X times | Basic workshop crafting |
| Do until X | Stockpile maintenance (automation) |
| Do forever | Continuous processes (Firepit) |

**Until bill modes exist, workshops are crippled.** This is the single highest-leverage feature.

### After Bill Modes: Unlock Chain

```
Bill Modes (CRITICAL BOTTLENECK)
    │
    ├─► Sawmill ──────► ITEM_PLANKS ──► Wood walls, floors
    │                   ITEM_STICKS ──► Tool crafting, kindling
    │
    ├─► Kiln ─────────► ITEM_CHARCOAL ─► Self-sustaining fuel loop
    │                   ITEM_BRICKS ───► Fire-resistant construction
    │                   ITEM_GLASS ────► Windows
    │
    └─► Fiber Works ──► ITEM_ROPE ─────► Construction component
                        ITEM_CLOTH ────► Building material
        (also needs CELL_TALL_GRASS)
```

### Recommended Order

| Priority | Feature | Why |
|----------|---------|-----|
| **1** | Bill modes | Enables all workshop production |
| **2** | Sawmill | Quick win - closes wood loop, enables planks + sticks |
| **3** | Kiln | Big win - charcoal creates self-sustaining fuel cycle |
| **4** | CELL_TALL_GRASS + Gather | Enables fiber/textile loop |
| **5** | Zone designations | Automates gathering, cleaning |

### What Each Unlocks

**Bill modes** → All workshops can produce systematically

**Sawmill** → 
- ITEM_PLANKS → wood construction (walls, floors, furniture)
- ITEM_STICKS → tool handles, wattle-daub, kindling, fencing

**Kiln** →
- ITEM_CHARCOAL → efficient fuel, self-sustaining (wood → charcoal → fuel for more charcoal)
- ITEM_BRICKS → fire-resistant walls
- ITEM_POTTERY → storage (future:containers), trade goods
- ITEM_GLASS → windows, containers

**CELL_TALL_GRASS** →
- ITEM_TALL_GRASS → plant fiber → rope, cloth
- Weaving Bench inputs (mats)

**Zone designations** →
- Systematic gathering without per-cell clicks
- Cleaning automation
- Proper stockpile flow

---

## Summary

The job system is the engine that drives production loops. Every job should either:
1. Extract resources from terrain (source)
2. Transform items at workshops (processing)
3. Place items back into terrain (sink)
4. Move items between these stages (hauling)

Follow autarky: only implement jobs that complete loops. Defer jobs that create dead ends until their sinks exist.

Use the other documents for specifics:
- Architecture details: `jobs-roadmap.md`
- Workshop recipes: `../workshops-and-jobs.md`
- Bill system: `bill-modes.md`
- Zone behavior: `zone-designations.md`, `zone-activities.md`
- Terrain mapping: `jobs-from-terrain.md`
