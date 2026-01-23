# Jobs System Roadmap

Research and planning document for expanding the navkit job system beyond hauling.

## Current State (What We Have)

### Implemented Features
- **Hauling** - Movers pick up ground items and deliver to stockpiles
- **Stockpile filters** - Each stockpile can accept/reject item types (red, green, blue)
- **Stockpile priorities** - Higher priority stockpiles pull items from lower priority ones (re-haul)
- **Stockpile stacking** - Multiple items per slot, configurable max stack size
- **Gather zones** - Restrict hauling to items within designated areas
- **Clear jobs** - Remove wrong-type items from stockpiles (JOB_MOVING_TO_DROP)
- **Absorb jobs** - Pick up matching ground items on stockpile tiles and "absorb" them
- **Spatial grids** - O(1) lookups for movers and items
- **Unreachable cooldowns** - Don't spam-retry unreachable items

### Current Job States
```c
typedef enum {
    JOB_IDLE,
    JOB_MOVING_TO_ITEM,
    JOB_MOVING_TO_STOCKPILE,
    JOB_MOVING_TO_DROP,  // Clear job: carrying item away to drop on ground
} JobState;
```

### Current Item States
```c
typedef enum {
    ITEM_ON_GROUND,
    ITEM_BEING_CARRIED,
    ITEM_IN_STOCKPILE,
} ItemState;
```

### Current Architecture Pain Points

Job data is embedded directly in Mover:
```c
// In mover.h - job fields on Mover struct
JobState jobState;
int targetItem;
int carryingItem;
int targetStockpile;
int targetSlotX, targetSlotY;
```

`AssignJobs()` is a monolithic function with multiple priority passes.

`JobsTick()` is a hard-coded state machine for haul states only.

**Problem:** Every new job type requires:
- New fields on Mover
- New JOB_* states
- More branches in the JobsTick switch
- More passes in AssignJobs

This will explode in complexity as we add mining, building, crafting, etc.

---

## Phase 0: Architecture Refactor (The Big Unlock)

Before adding new job types, consider refactoring to a generic job framework. This is optional but will pay off significantly if we plan to add many job types.

### 0.1 Move Job Data Out of Mover

**Current:** Job state embedded in Mover struct
**Proposed:** Separate Job pool, Mover just has `int currentJobId`

```c
typedef enum {
    JOBTYPE_NONE,
    JOBTYPE_HAUL,
    JOBTYPE_MINE,
    JOBTYPE_BUILD,
    JOBTYPE_CRAFT,
    // etc.
} JobType;

typedef enum {
    JOBRUN_RUNNING,
    JOBRUN_DONE,
    JOBRUN_FAIL,
} JobRunResult;

typedef struct {
    JobType type;
    bool active;
    int assignedMover;      // -1 if unassigned
    int step;               // Which step/toil we're on
    float age;              // For timeouts / debugging
    
    // Generic targets (interpret based on job type)
    int targetItem;
    int targetTileX, targetTileY, targetTileZ;
    int targetStockpile;
    int targetSlotX, targetSlotY;
    int targetBlueprint;
    int targetWorkshop;
    
    // Work progress
    float progress;         // 0.0 to 1.0 for timed work
    float workRequired;
    
    // Reservation bookkeeping (so cancel always cleans up correctly)
    bool reservedItem;
    bool reservedSlot;
    bool reservedBlueprint;
} Job;

#define MAX_JOBS 10000
extern Job jobs[MAX_JOBS];
```

**Mover becomes simpler:**
```c
typedef struct {
    // ... position, path, movement fields ...
    int currentJobId;       // -1 = no job (idle)
} Mover;
```

### 0.2 Job Drivers (Per-Type Step Functions)

Instead of one giant switch in JobsTick(), each job type has its own "driver":

```c
// Function pointer type for job drivers
typedef JobRunResult (*JobDriver)(Job* job, Mover* mover, float dt);

// Per-type drivers
JobRunResult RunJob_Haul(Job* job, Mover* mover, float dt);
JobRunResult RunJob_Mine(Job* job, Mover* mover, float dt);
JobRunResult RunJob_Build(Job* job, Mover* mover, float dt);
JobRunResult RunJob_Craft(Job* job, Mover* mover, float dt);

// Driver lookup table
JobDriver jobDrivers[] = {
    [JOBTYPE_NONE]  = NULL,
    [JOBTYPE_HAUL]  = RunJob_Haul,
    [JOBTYPE_MINE]  = RunJob_Mine,
    [JOBTYPE_BUILD] = RunJob_Build,
    [JOBTYPE_CRAFT] = RunJob_Craft,
};

// New JobsTick becomes trivial:
void JobsTick(float dt) {
    for (int i = 0; i < MAX_MOVERS; i++) {
        Mover* m = &movers[i];
        if (!m->active || m->currentJobId < 0) continue;
        
        Job* job = &jobs[m->currentJobId];
        JobDriver driver = jobDrivers[job->type];
        if (!driver) continue;
        
        JobRunResult result = driver(job, m, dt);
        
        if (result == JOBRUN_DONE || result == JOBRUN_FAIL) {
            CleanupJob(job);
            m->currentJobId = -1;
        }
    }
}
```

**Benefit:** Adding a new job type = add one new driver function. No touching the core loop.

### 0.3 WorkGivers (Modular Job Producers)

Split `AssignJobs()` into independent "WorkGivers" that each produce job candidates:

```c
typedef struct {
    const char* name;
    int priority;                           // Lower = higher priority
    Job* (*FindJobForMover)(Mover* mover);  // Returns NULL if no work available
} WorkGiver;

WorkGiver workGivers[] = {
    { "StockpileClearAbsorb", 0, WorkGiver_StockpileClearAbsorb },
    { "Mining",               1, WorkGiver_Mining },
    { "Construction",         2, WorkGiver_Construction },
    { "HaulToStockpile",      3, WorkGiver_HaulToStockpile },
    { "Crafting",             4, WorkGiver_Crafting },
    { "RehaulOverfull",       5, WorkGiver_RehaulOverfull },
};

void AssignJobs(void) {
    for (int m = 0; m < moverCount; m++) {
        if (movers[m].currentJobId >= 0) continue;  // Already has job
        
        // Try each WorkGiver in priority order
        for (int w = 0; w < NUM_WORKGIVERS; w++) {
            Job* job = workGivers[w].FindJobForMover(&movers[m]);
            if (job) {
                job->assignedMover = m;
                movers[m].currentJobId = GetJobIndex(job);
                break;
            }
        }
    }
}
```

**Benefit:** Adding a new job source = add one WorkGiver function. Existing ones unchanged.

### 0.4 Migration Path (No Big Rewrite)

Can migrate incrementally:

1. **Introduce Job pool + mover.currentJobId** (keep old fields temporarily)
2. **Port hauling into JOBTYPE_HAUL driver** (same behavior, new structure)
3. **Convert AssignJobs() haul logic into WorkGiver_HaulToStockpile**
4. **Remove old job fields from Mover**
5. **Add new job types using the new pattern**

### 0.5 Decision: Refactor First or Add Features First?

**Option A: Refactor first**
- Cleaner architecture before complexity grows
- Risk: refactoring without new features feels like no progress
- Time: ~1-2 days to migrate hauling

**Option B: Add mining with current architecture, refactor later**
- Faster to first playable mining
- Technical debt: mining code will need rewrite
- Good for prototyping / validating gameplay

**Option C: Hybrid**
- Add mining with current architecture (quick prototype)
- If it works well, refactor before adding construction
- Accept that mining code gets rewritten

---

## Phase 1: UI for Existing Systems

Before adding new job types, we should have proper UI for what exists.

### 1.1 Draw/Remove Stockpiles

**User interaction:**
1. Select "Stockpile" tool
2. Click-drag rectangle on map
3. Release to create stockpile

**Design decisions needed:**
- [ ] Overlapping stockpiles: merge, reject, or allow overlap?
- [ ] Minimum size: 1x1 allowed?
- [ ] Visual feedback: ghost rectangle while dragging?

**Remove stockpile:**
1. Select "Remove" tool (or right-click?)
2. Click-drag rectangle
3. All stockpiles overlapping selection get deleted
4. Items on deleted tiles become ground items (existing clear logic handles them)

**Alternative: per-tile stockpile editing**
- Click individual tiles to add/remove from stockpile
- More granular but slower for large areas

### 1.2 Draw/Remove Gather Zones

Same UI pattern as stockpiles. Simpler because:
- No filters to configure
- No priority system
- Just "haul from here" vs "don't haul from here"

### 1.3 Stockpile Configuration UI

After creating a stockpile, player needs to configure:
- Which item types allowed (checkboxes)
- Priority level (1-5 slider or buttons)
- Max stack size (optional, per-stockpile override)
- **Target count** (optional): "Try to keep N items here" — creates demand-driven hauling

**UI options:**
- Click stockpile to open config panel
- Or: all stockpiles share global filter until customized

### 1.4 Requester Stockpiles (Simple Automation)

A stockpile with a **target count** becomes a "requester" that generates hauling jobs to maintain that amount:

```c
typedef struct {
    // ... existing stockpile fields ...
    int targetCount;        // -1 = no target (normal behavior)
                            // >= 0 = try to maintain this many items
} Stockpile;
```

**Example:** Stockpile near forge set to "keep 30 coal"
- Haulers automatically refill when count drops below target
- Feels like Factorio logistics without belts/bots

**Implementation:** In `AssignJobs()`, check stockpiles with `targetCount >= 0` and `currentCount < targetCount`, generate haul jobs from lower-priority sources.

This is simpler than full Work Orders but provides powerful automation feel.

---

## Phase 2: Mining/Digging

**Why mining first:**
- Reuses existing pathfinding, hauling, stockpiles
- Creates visible world change (satisfying!)
- Completes loop: Designate → Mine → Item spawns → Haul → Stockpile
- ~230 lines of new code

### 2.1 Designation System

New concept: **Designations** are player-marked tiles where work should happen.

```c
typedef enum {
    DESIGNATION_MINE,       // Dig out this tile
    DESIGNATION_CHANNEL,    // Dig out + create ramp below (future)
    DESIGNATION_CHOP,       // Cut down tree (future)
} DesignationType;

typedef struct {
    int x, y, z;
    DesignationType type;
    bool active;
    int assignedMover;      // -1 = unassigned
    float progress;         // 0.0 to 1.0
    float workRequired;     // Total work units needed (varies by material)
} Designation;

#define MAX_DESIGNATIONS 10000
extern Designation designations[MAX_DESIGNATIONS];
```

### 2.2 New Job States

```c
typedef enum {
    // ... existing states ...
    JOB_MOVING_TO_DESIGNATION,  // Walking to work site
    JOB_WORKING,                // At site, doing work (mining, building, etc.)
} JobState;
```

### 2.3 Mining State Machine

```
JOB_IDLE
    │
    │ designation found & assigned
    ▼
JOB_MOVING_TO_DESIGNATION
    │
    │ arrived at adjacent tile
    ▼
JOB_WORKING
    │
    │ progress reaches 1.0
    ▼
(tile becomes passable, item spawns)
    │
    ▼
JOB_IDLE
```

**Key detail:** Mover stands on adjacent tile, not the tile being mined. Need to find a valid adjacent walkable tile.

### 2.4 Terrain Modification

```c
// In terrain.c or grid.c
typedef enum {
    CELL_WALL,          // Existing: impassable
    CELL_FLOOR,         // Existing: passable
    CELL_STONE_WALL,    // Minable, becomes CELL_FLOOR + spawns stone
    CELL_ORE_WALL,      // Minable, becomes CELL_FLOOR + spawns ore
    // etc.
} CellType;

void DigTile(int x, int y, int z);      // Make passable + spawn item
bool IsTileDiggable(int x, int y, int z);
ItemType GetMinedItemType(int x, int y, int z);  // What drops when mined
```

### 2.5 Job Assignment Priority

In `AssignJobs()`:
```
PRIORITY 0: Absorb/Clear (stockpile tile cleanup) - existing
PRIORITY 1: Mining designations - NEW
PRIORITY 2: Hauling (stockpile-centric) - existing
PRIORITY 3: Hauling (item-centric fallback) - existing
PRIORITY 4: Re-hauling (low to high priority) - existing
```

### 2.6 Questions to Resolve

- [ ] Can multiple movers work the same designation? (probably no)
- [ ] What if miner can't reach? Cooldown like hauling?
- [ ] Cancel designation if blocked for too long?
- [ ] Mining skill/speed modifiers per mover? (future)
- [ ] Different materials = different work times?

---

## Phase 3: Construction

**Why construction second:**
- Uses mined resources (closes the loop)
- Players can build structures
- More complex: needs blueprint + material delivery

### 3.1 Blueprint System

```c
typedef enum {
    BLUEPRINT_WALL,
    BLUEPRINT_FLOOR,
    BLUEPRINT_DOOR,
    BLUEPRINT_FURNITURE,
    // etc.
} BlueprintType;

typedef struct {
    int x, y, z;
    BlueprintType type;
    bool active;
    
    // Material requirements
    ItemType requiredMaterial;
    int requiredCount;
    int deliveredCount;         // How many materials delivered so far
    
    // Construction progress
    int assignedBuilder;        // -1 = none
    float progress;             // 0.0 to 1.0
    float workRequired;
} Blueprint;
```

### 3.2 Construction States

```c
JOB_FETCHING_MATERIAL,  // Hauling item to blueprint (not stockpile)
JOB_BUILDING,           // At blueprint, constructing
```

### 3.3 Construction Flow

```
Player places blueprint
    │
    ▼
Blueprint needs materials (e.g., 2 stone)
    │
    │ hauler assigned to fetch
    ▼
JOB_FETCHING_MATERIAL (haul stone to blueprint tile)
    │
    │ material delivered
    ▼
(repeat until all materials delivered)
    │
    │ builder assigned
    ▼
JOB_BUILDING (at blueprint, progress 0→1)
    │
    │ complete
    ▼
(blueprint becomes solid structure)
```

### 3.4 Material Reservation

Items reserved for blueprints shouldn't be hauled to stockpiles:

```c
typedef struct {
    // ... existing item fields ...
    int reservedForBlueprint;  // -1 = not reserved, else blueprint index
} Item;
```

### 3.5 Questions to Resolve

- [ ] Can any mover haul materials, or only "haulers"?
- [ ] Can any mover build, or only "builders"?
- [ ] What happens if blueprint is cancelled? Materials drop?
- [ ] Deconstruction? (reverse of construction)
- [ ] Multi-tile structures? (e.g., bed = 1x2)

---

## Phase 4: Simple Crafting

**Why crafting third:**
- Transforms resources (gives purpose to stockpiles)
- Creates production chains
- Relatively simple if we keep it basic

### 4.1 Workshop System

```c
typedef struct {
    ItemType inputType;
    int inputCount;
    ItemType outputType;
    int outputCount;
    float workTime;         // Seconds to complete
} Recipe;

typedef struct {
    int x, y, z;
    int width, height;      // Workshop footprint
    bool active;
    
    Recipe* availableRecipes;
    int recipeCount;
    
    // Work queue
    int currentRecipe;      // -1 = idle
    int targetAmount;       // How many to make
    int completedAmount;
    bool repeat;            // Auto-restart when done
    
    // Current job
    int assignedCrafter;    // -1 = none
    float progress;
    
    // Input/output positions
    int inputX, inputY;     // Where materials are taken from
    int outputX, outputY;   // Where products are placed
} Workshop;
```

### 4.2 Crafting State

```c
JOB_CRAFTING,  // At workshop, working on recipe
```

### 4.3 Crafting Flow

```
Workshop has recipe queued
    │
    │ materials available nearby?
    ▼
(haulers bring materials to workshop input area)
    │
    │ crafter assigned
    ▼
JOB_CRAFTING (consume input, progress 0→1)
    │
    │ complete
    ▼
(output item spawns at output position)
    │
    ▼
(haulers take output to stockpile)
```

### 4.4 Workshop-Stockpile Integration

RimWorld pattern: critical-priority stockpiles next to workshops

```c
typedef struct {
    // ... existing stockpile fields ...
    int linkedWorkshop;     // -1 = standalone
    bool isInputStockpile;  // true = feeds workshop, false = receives output
} Stockpile;
```

### 4.5 Bill Modes (RimWorld-style)

Bills at workshops with different execution modes:

```c
typedef enum {
    BILL_DO_X_TIMES,        // Make exactly N, then stop
    BILL_DO_UNTIL_X,        // Make until stockpile has X (check before each craft)
    BILL_DO_FOREVER,        // Repeat indefinitely
    BILL_PAUSED,            // Suspended, skip this bill
} BillMode;

typedef struct {
    int recipeIdx;
    BillMode mode;
    int targetCount;            // For DO_X_TIMES or DO_UNTIL_X
    int completedCount;         // Progress for DO_X_TIMES
    int ingredientSearchRadius; // How far to look for inputs (tiles)
    bool suspended;
} Bill;
```

**"Do until X" is powerful:** Workshop checks stockpile count before starting each craft. If target met, skips to next bill. Feels like automation without complex work orders.

### 4.6 Work Orders (Advanced, Future)

Dwarf Fortress "manager" style - conditional automation:

```c
typedef struct {
    int workshopIdx;
    int recipeIdx;
    int targetAmount;
    bool repeat;
    
    // Conditions
    ItemType conditionItem;
    int minCount;           // Only craft if we have >= this many input
    int maxCount;           // Only craft if we have < this many output (quota)
} WorkOrder;
```

Example: "Make stone blocks while stone chunks > 5 and stone blocks < 50"

---

## Phase 5: Farming

**Most complex system** - multi-stage, seasonal, growth simulation.

### 5.1 Farm Plot

```c
typedef enum {
    FARM_EMPTY,         // Needs tilling
    FARM_TILLED,        // Ready for planting
    FARM_PLANTED,       // Seed in ground
    FARM_GROWING,       // Growing (progress 0→1)
    FARM_READY,         // Ready to harvest
    FARM_WILTING,       // Not harvested in time (future)
} FarmTileState;

typedef struct {
    int x, y, z;
    int width, height;
    bool active;
    
    FarmTileState* tileStates;  // Per-tile state
    float* growthProgress;      // Per-tile growth
    ItemType* plantedCrop;      // Per-tile crop type
    
    ItemType allowedCrop;       // What to plant here
    bool autoReplant;           // Replant after harvest?
} FarmPlot;
```

### 5.2 Farming States

```c
JOB_TILLING,      // Preparing ground
JOB_PLANTING,     // Putting seeds in
JOB_TENDING,      // Watering/weeding (optional, future)
JOB_HARVESTING,   // Collecting crops
```

### 5.3 Farming Flow

```
FARM_EMPTY
    │ farmer tills
    ▼
FARM_TILLED
    │ farmer plants (consumes seed item)
    ▼
FARM_PLANTED
    │ time passes
    ▼
FARM_GROWING (progress 0→1, based on time)
    │ growth complete
    ▼
FARM_READY
    │ farmer harvests (spawns crop item)
    ▼
FARM_EMPTY (or FARM_TILLED if auto-replant)
```

---

## Phase 6: Zone Activities (Cataclysm DDA-style)

Currently our zones (gather zones, stockpiles) are passive filters. Zone Activities make zones **generate jobs**.

### 6.1 Zone Types that Generate Jobs

```c
typedef enum {
    ZONE_GATHER,            // Existing: filter where items can be hauled FROM
    ZONE_STOCKPILE,         // Existing: where items are stored
    ZONE_UNSORTED,          // NEW: items here get sorted to appropriate stockpiles
    ZONE_DUMP,              // NEW: drop unwanted items here
    ZONE_WORK_AREA,         // NEW: movers assigned here do nearby jobs first
} ZoneType;
```

### 6.2 Autosort Zone

**Player action:** Mark an "Unsorted" zone + several typed stockpiles
**System behavior:** Items in Unsorted zone automatically get hauled to matching stockpiles
**Win condition:** Dump loot somewhere → organized base without micromanagement

```c
typedef struct {
    int x, y, z;
    int width, height;
    ZoneType type;
    bool active;
    
    // For ZONE_UNSORTED:
    bool autoSort;          // Generate sorting jobs for items here
} Zone;
```

### 6.3 Work Area Zones

Assign movers to prefer jobs within a zone:
- "Miners work the north tunnel"
- "Haulers prioritize the workshop area"

```c
typedef struct {
    // ... existing mover fields ...
    int preferredZone;      // -1 = no preference, else zone index
} Mover;
```

Jobs within preferred zone get distance bonus in job scoring.

---

## Mover Capabilities / Labor System

As job types grow, need to control which movers can do what.

### Option A: Simple Flags

```c
typedef struct {
    bool canHaul;       // Almost everyone
    bool canMine;
    bool canBuild;
    bool canCraft;
    bool canFarm;
} MoverCapabilities;
```

### Option B: Skill Levels (RimWorld style)

```c
typedef struct {
    int hauling;    // 0-20, affects speed
    int mining;     // 0-20, affects speed + yield
    int building;   // 0-20, affects speed + quality
    int crafting;   // 0-20, affects speed + quality
    int farming;    // 0-20, affects speed + yield
} MoverSkills;
```

### Option C: Work Details (DF style)

Group movers into "details" (teams):
- Miners: only mining
- Haulers: only hauling
- Everyone: does anything

---

## Priority System

### Per-Mover Priorities (RimWorld)

Each mover has priority 1-4 for each job type:
```
         Mining  Hauling  Building
Mover 1:   1       3         2
Mover 2:   -       1         1       (dash = disabled)
Mover 3:   2       2         2
```

Movers do ALL priority-1 work before priority-2.

### Global Job Priority (Simpler)

Single priority order for all movers:
1. Medical/Emergency
2. Mining (player-designated)
3. Construction
4. Crafting
5. Hauling
6. Cleaning

Within same priority, pick closest job.

---

## Item Type Expansion

Current: ITEM_RED, ITEM_GREEN, ITEM_BLUE (abstract crates)

Future categories:
```c
typedef enum {
    // Raw materials
    ITEM_STONE,
    ITEM_WOOD,
    ITEM_ORE_IRON,
    ITEM_ORE_GOLD,
    
    // Processed materials
    ITEM_STONE_BLOCK,
    ITEM_PLANK,
    ITEM_IRON_BAR,
    
    // Food
    ITEM_WHEAT,
    ITEM_FLOUR,
    ITEM_BREAD,
    
    // etc.
} ItemType;
```

Stockpile filters would need categories:
- "All stone" = ITEM_STONE + ITEM_STONE_BLOCK
- "All food" = ITEM_WHEAT + ITEM_FLOUR + ITEM_BREAD
- Or: hierarchical item types with parent categories

---

## Open Questions

### Gameplay
- [ ] What's the core loop we're building toward?
- [ ] Is this a survival game, city builder, or sandbox?
- [ ] Enemies/threats? Or peaceful building?
- [ ] Win condition? Or endless sandbox?

### Technical
- [ ] Save/load system for designations, blueprints, workshops?
- [ ] Undo system for player actions?
- [ ] How to handle very large designation areas? (batch processing)
- [ ] Multi-z-level considerations for all new systems?

### UI/UX
- [ ] How does player queue multiple work orders?
- [ ] How does player see what movers are doing?
- [ ] Notifications for completed/failed jobs?
- [ ] Visualization of job assignments (lines from mover to target?)

---

## Technical Improvements (Before Adding Many Job Types)

These prevent "why are my pawns stupid?" moments as the system grows.

### Reservation Timeouts / Cleanup

Currently we have:
- `items[item].reservedBy` - mover index
- `stockpiles[sp].slotReservedBy[idx]` - mover index

**Missing:** What if the mover dies/becomes inactive? Reservation is orphaned.

**Solution:**
```c
typedef struct {
    // ... existing item fields ...
    int reservedBy;
    float reservedAt;       // Timestamp when reserved
} Item;

// In ItemsTick() or JobsTick():
// Auto-release reservations if:
//   - reservedBy mover is inactive/dead
//   - reservation is older than RESERVATION_TIMEOUT (e.g., 30 seconds)
```

### Cheap Reachability Check (Connectivity Regions)

Currently `TryAssignItemToMover()` calls `FindPath()` to check reachability. This is expensive and will get worse with many job types.

**Solution: Flood-fill connectivity IDs**
```c
// Per-tile connectivity region ID
int connectivityRegion[MAX_Z][MAX_Y][MAX_X];

// Recompute when terrain changes (only affected chunks)
void RecomputeConnectivity(int chunkX, int chunkY, int z);

// Fast reachability check (no pathfinding)
bool CanReach(int fromX, int fromY, int fromZ, int toX, int toY, int toZ) {
    // Different regions = definitely unreachable
    if (connectivityRegion[fromZ][fromY][fromX] != connectivityRegion[toZ][toY][toX]) {
        return false;
    }
    // Same region = probably reachable (run real pathfind to confirm)
    return true;
}
```

**Usage:**
```c
// In job assignment:
if (!CanReach(moverX, moverY, moverZ, targetX, targetY, targetZ)) {
    // Skip immediately, don't even try pathfinding
    continue;
}
// Only now do expensive FindPath()
```

### Chained Jobs (Deliver → Build)

Construction isn't one job - it's a sequence:

```
JOBTYPE_DELIVER (haul material to blueprint)
    ↓
JOBTYPE_BUILD (stand at tile, work for N ticks)
    ↓
(optional) JOBTYPE_HAUL_OUTPUT (leftover materials / debris)
```

**Option A: Job spawns next job on completion**
```c
JobRunResult RunJob_Deliver(Job* job, Mover* mover, float dt) {
    // ... delivery logic ...
    if (arrived && dropped) {
        // Check if blueprint has all materials
        Blueprint* bp = &blueprints[job->targetBlueprint];
        bp->deliveredCount++;
        
        if (bp->deliveredCount >= bp->requiredCount) {
            // Spawn build job (or let WorkGiver_Build find it)
        }
        return JOBRUN_DONE;
    }
    return JOBRUN_RUNNING;
}
```

**Option B: Blueprint state machine drives job creation**
```c
typedef enum {
    BP_AWAITING_MATERIALS,  // WorkGiver_Deliver creates haul jobs
    BP_READY_TO_BUILD,      // WorkGiver_Build creates build jobs
    BP_UNDER_CONSTRUCTION,  // Builder assigned
    BP_COMPLETE,
} BlueprintState;
```

Option B is cleaner - blueprints know their state, WorkGivers check blueprint state.

---

## Job Interruption and Resumption (Future)

What happens when:
- Mover gets hungry mid-job?
- Mover gets attacked?
- Higher priority job appears?

**Simple approach:** Cancel current job, release reservations, handle need, re-find job later.

**Complex approach:** Pause/resume jobs with saved state. Probably overkill for now.

---

## Implementation Order Recommendation

1. **Stockpile/Gather Zone UI** - Test existing features properly
2. **Reservation timeouts** - Prevent orphaned reservations
3. **Mining** - First "real" job beyond hauling
4. **Architecture refactor** - If mining works well, refactor before construction
5. **Connectivity regions** - Cheap reachability before many job types
6. **Construction** - Use mined resources, chained jobs
7. **Simple Crafting** - Production chains
8. **Farming** - Sustainability/food (if needed)
9. **Skills/Priorities** - Once multiple job types exist

Each phase should be playable before moving to the next.

---

## Decision Log

Track key decisions as we make them:

| Date | Decision | Rationale |
|------|----------|-----------|
| TBD  | Refactor first vs feature first? | |
| TBD  | Overlapping stockpiles behavior? | |
| TBD  | Per-mover vs global job priority? | |
| TBD  | Job data in Mover vs separate Job pool? | |

---

## References

- [Dwarf Fortress Wiki - Labor](https://dwarffortresswiki.org/index.php/Labor)
- [RimWorld Wiki - Work](https://rimworldwiki.com/wiki/Work)
- [Gnomoria Wiki - Priority](https://gnomoria.fandom.com/wiki/Priority)
- [Game Programming Patterns - State](https://gameprogrammingpatterns.com/state.html)
