# Vision-Extracted Todos

Actionable items extracted from `docs/vision/` files, ordered by readiness (how easily they fit the current codebase).

---

## Tier 1: Ready Now (Infrastructure Exists)

These can be implemented with minimal new systems - they extend existing patterns.

### 1.1 New Item Types
**Source**: vision.md, stone-aged-dwarfjobs.md, new-jobs-ideas.md

Current items: RED, GREEN, BLUE, ORANGE (raw stone), STONE_BLOCKS, WOOD, SAPLING

**Add these items** (just add to `item_defs.c`):
- [ ] `ITEM_FOOD` - Basic edible (for future needs system, can stockpile now)
- [ ] `ITEM_CHARCOAL` - Burned wood, fuel source
- [ ] `ITEM_CLAY` - Raw material from digging clay tiles
- [ ] `ITEM_POTTERY` - Crafted from clay (storage vessel)
- [ ] `ITEM_ROPE` - Crafted from plant fiber
- [ ] `ITEM_HIDE` - From future hunting, used for clothing/leather
- [ ] `ITEM_BONE` - From butchering, used for tools

**Why now**: Item system is data-driven. Just add enum + row in table.

---

### 1.2 New Cell Types
**Source**: tiles.md, special-tiles.md, soils.md

Current cells: WALL, AIR, LADDER_*, DIRT, WOOD_WALL, BEDROCK, RAMP_*, SAPLING, TREE_TRUNK, TREE_LEAVES

**Add these cells** (add to `cell_defs.c`):
- [ ] `CELL_WOOD_FLOOR` - Walkable constructed floor, burnable
- [ ] `CELL_STONE_FLOOR` - Walkable constructed floor, permanent
- [ ] `CELL_CLAY` - Diggable, produces clay items
- [ ] `CELL_SAND` - Different visual, future glass-making
- [ ] `CELL_GRAVEL` - Affects movement speed (slower)
- [ ] `CELL_SCORCHED` - Result of fire, grows back slowly
- [ ] `CELL_MUD` - Wet dirt, movement penalty, dries over time

**Why now**: Cell system is data-driven with flags. Add enum + row + maybe new flag.

---

### 1.3 New Workshop Types
**Source**: stone-aged-dwarfjobs.md, workshopdr.md, logistics-influences.md

Current workshops: STONECUTTER only

**Add these workshops**:
- [ ] `WORKSHOP_WOODWORKER` - Wood → planks, furniture
- [ ] `WORKSHOP_KILN` - Clay → pottery, charcoal production
- [ ] `WORKSHOP_FIREPIT` - Cooking, requires fuel (wood/charcoal)
- [ ] `WORKSHOP_TANNER` - Hides → leather (future)

**Why now**: Workshop template system exists. Define template + recipes.

---

### 1.4 New Mover Capabilities
**Source**: stone-aged-dwarfjobs.md, new-jobs-ideas.md

Current capabilities: canHaul, canMine, canBuild, canPlant

**Add these capabilities**:
- [ ] `canCraft` - Work at workshops (currently all movers can craft?)
- [ ] `canCook` - Work at firepit specifically
- [ ] `canGather` - Harvest wild plants
- [ ] `canChop` - Already exists for trees, formalize

**Why now**: Just add bool to MoverCapabilities struct.

---

### 1.5 New Designation Types
**Source**: zone-designations.md (todo), entropy.md

Current designations: MINE, CHANNEL, REMOVE_FLOOR, REMOVE_RAMP, CHOP, GATHER_SAPLING, PLANT_SAPLING

**Add these designations**:
- [ ] `DESIGNATION_GATHER_PLANT` - Harvest wild plants (not saplings)
- [ ] `DESIGNATION_CLEAN` - Clean dirty floor tiles
- [ ] `DESIGNATION_DUMP` - Drop items here (dump zone)

**Why now**: Designation enum + handler in jobs.c.

---

## Tier 2: Small New Systems (Extend Existing Patterns)

These require new grids or systems but follow existing patterns.

### 2.1 Ground Wear Enhancement
**Source**: entropy.md, entropy2-chatgpt.md

Current: `groundwear.c` tracks trampling, grass→dirt transitions

**Enhance with**:
- [ ] Add WORN state between grass and dirt (visual only)
- [ ] Track `dirt_load` on movers (how dirty their feet are)
- [ ] Movers deposit dirt indoors when walking from dirty areas
- [ ] Add `dirty` flag to floor cells
- [ ] Doormat cells reduce dirt_load when stepped on

**Why now**: groundwear.c already does 90% of this. Add mover field + cell flag.

---

### 2.2 Mud System
**Source**: entropy_systems.md, entropy2-chatgpt.md

**Implementation**:
- [ ] Add `CELL_FLAG_MUD` to cell flags (or wetness > threshold on dirt = mud)
- [ ] Water on dirt = mud
- [ ] Mud dries based on temperature (uses existing temp system)
- [ ] Mud slows movement (already have water speed penalties as pattern)

**Why now**: Temperature exists, water exists, just combine them.

---

### 2.3 Soil Fertility Grid
**Source**: soils.md, soil-fertility.md (todo)

**Implementation**:
- [ ] Add `fertilityGrid[z][y][x]` (uint8, 0-255)
- [ ] Initialize from noise during terrain gen
- [ ] Affects future crop growth speed
- [ ] Composting increases fertility
- [ ] Overfarming decreases fertility

**Why now**: Same pattern as wearGrid, waterGrid, etc.

---

### 2.4 Wild Plants System
**Source**: wild-plants.md (todo), brush-bushes.md (todo)

**Implementation**:
- [ ] Wild plants as cell overlay (like grass surface state)
- [ ] Add DESIGNATION_GATHER_PLANT to harvest
- [ ] Harvesting produces ITEM_FOOD + optionally ITEM_SEEDS
- [ ] Plants regrow over time on fertile grass

**Why now**: Surface overlay system exists (grass states). Add new overlay type.

---

### 2.5 Light/Shadow System (Basic)
**Source**: entropy_systems.md

**Implementation**:
- [ ] Add `lightGrid[z][y][x]` (uint8)
- [ ] Sunlight fills from top down (blocked by solid cells)
- [ ] Torches/fires emit light (radius)
- [ ] Light affects plant growth
- [ ] Dark areas slower mold growth (future)

**Why now**: Similar to smoke/temperature propagation. Independent system.

---

## Tier 3: Medium Complexity (New Infrastructure)

These require more thought but are well-documented.

### 3.1 Needs System
**Source**: needs-vs-jobs.md

**Implementation**:
- [ ] Add to Mover: `hunger`, `energy`, `needState`, `activeNeed`, `needTarget`
- [ ] Check critical needs before job assignment
- [ ] Critical need → interrupt job → seek satisfier → consume → resume
- [ ] Personal actions are NOT jobs (hardcoded behavior)
- [ ] Food items are consumed, energy restored by "beds" or rest spots

**Why now**: Well-designed in docs. Mover has job interruption already. Add parallel system.

---

### 3.2 Steam System (Phase 1)
**Source**: steam-system.md

**Implementation**:
- [ ] Add `steamGrid[z][y][x]` (SteamCell: level, pressure, stable)
- [ ] Water at boiling temp → generates steam
- [ ] Steam rises (like smoke)
- [ ] Steam condenses back to water when cool
- [ ] Rendering as particle effect

**Why now**: Temperature exists, water exists. Same pattern as smoke.

---

### 3.3 Cleaning/Maintenance Loop
**Source**: entropy.md, entropy2-chatgpt.md

**Implementation**:
- [ ] Track room dirtiness (or per-cell dirty flag)
- [ ] Dirty floors spawn cleaning jobs above threshold
- [ ] Movers with cleaning enabled execute clean jobs
- [ ] Clean job reduces dirt level
- [ ] Tuning knobs for thresholds

**Why now**: Job system exists. Add job type + trigger system.

---

### 3.4 Reclamation System
**Source**: entropy.md, entropy2-chatgpt.md

**Implementation**:
- [ ] Track "days since last colonist visit" per cell/area
- [ ] Abandoned outdoor areas: dirt→grass→weeds→bushes
- [ ] Abandoned structures: crack→crumble→rubble
- [ ] Configurable timeline per biome

**Why now**: Day counter exists. Add area tracking.

---

## Tier 4: Future (Needs Design Work)

These are vision items that need more planning.

### 4.1 Full Farming System
**Source**: farming-overhaul-ccdda.md, crops-df.md

Needs: plant JSON definitions, growth stages, environmental requirements, tend mechanics, skill system

**Defer until**: Soil fertility, temperature, light systems exist

---

### 4.2 Vehicle/Exodus System
**Source**: vision.md

Needs: Multi-tile entities, construction system for vehicles, travel/overworld map

**Defer until**: Infinite world system exists

---

### 4.3 Emotional/Social System
**Source**: vision.md

Needs: Mood tracking, relationships, social needs, conversations

**Defer until**: Needs system exists and is stable

---

### 4.4 Pests/Disease
**Source**: entropy.md, entropy2-chatgpt.md

Needs: Food storage mechanics, contamination system, disease spread

**Defer until**: Food items exist, cleaning system exists

---

## Priority Order (Recommended)

### Sprint 1: Items & Cells Foundation
1. Add ITEM_FOOD, ITEM_CHARCOAL, ITEM_CLAY
2. Add CELL_WOOD_FLOOR, CELL_STONE_FLOOR, CELL_MUD
3. Add WORKSHOP_WOODWORKER, WORKSHOP_KILN

> **Note**: If the data-driven jobs/materials refactor (`todo/architecture/data-driven-jobs-materials.md`) has been completed, Sprint 1 will look different. Floor types become `CELL_FLOOR` + material (MAT_WOOD, MAT_STONE), and adding new materials is just one line in `materialDefs[]`. Re-evaluate this sprint after that refactor.

### Sprint 2: Entropy MVP
4. Enhance ground wear with mover dirt_load
5. Add dirty floor flag + deposition
6. Add DESIGNATION_CLEAN + cleaning job
7. Mud system (water + dirt + temperature)

### Sprint 3: Nature
8. Add fertility grid
9. Wild plants overlay + gathering
10. Light grid (basic sunlight)

### Sprint 4: Needs & Maintenance
11. Needs system (hunger first)
12. Cleaning/maintenance job loop
13. Steam system Phase 1

### Sprint 5: Farming Foundation
14. Tilled soil cell type
15. Plant growth stages
16. Basic crop cycle

---

## Cross-Reference with Existing Todos

These vision items **already have todo files**:
- Wild plants → `todo/nature/wild-plants.md`
- Soil fertility → `todo/nature/soil-fertility.md`
- Zone designations → `todo/jobs/zone-designations.md`
- Containers → `todo/items/containers.md`
- Firepit → `todo/environmental/firepit-fire-system.md`
- Dirty floors → `todo/world/dirty-floors-cleaner.md`

**New items from vision not yet in todo/**:
- Needs system
- Mud system
- Reclamation system
- Steam system
- Cleaning job loop
- Mover dirt_load mechanic
- Light/shadow system
