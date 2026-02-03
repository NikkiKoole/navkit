# Simple Crafting System - IMPLEMENTED

Status: Core system complete and working. This doc preserved as technical reference.

---

## Implementation Status

All core phases complete:
- Phase 1: Core Workshop System - DONE
- Phase 2: Bill System - DONE (all 3 modes)
- Phase 3: Material Consumption - DONE
- Phase 4: UI - DONE (placement + bill controls)
- Phase 5: Polish - Partial (templates done, multi-type/construction/skills not started)

---

## Overview

Crafting transforms resources and gives purpose to the mining → hauling → stockpile loop. 

**Core loop (DF-style):**
```
Mine wall → Stone spawns → Hauler takes to stockpile near workshop
                                          ↓
              Crafter picks up from stockpile → Brings to workshop → Crafts → Output spawns
                                                                                    ↓
                                                      Hauler takes output to stockpile
```

The crafter fetches their own materials (like Dwarf Fortress), but haulers keep input stockpiles filled. This creates natural efficiency: place stockpiles near workshops to minimize crafter travel time.

---

## Workshop Struct

```c
typedef struct {
    int x, y, z;                // Top-left corner of workshop
    int width, height;          // Workshop footprint (e.g., 3x3)
    bool active;
    
    // Workshop type determines available recipes
    WorkshopType type;
    
    // Bill queue (what to make)
    Bill bills[MAX_BILLS_PER_WORKSHOP];
    int billCount;
    
    // Current work state
    int assignedCrafter;        // Mover index, -1 = none
    
    // Tile positions (relative to workshop origin, or absolute)
    int workTileX, workTileY;   // Where crafter stands to work
    int outputTileX, outputTileY; // Where finished items spawn
    
    // Optional: linked stockpiles (DF-style)
    int linkedInputStockpiles[MAX_LINKED_STOCKPILES];
    int linkedInputCount;       // If > 0, ONLY take from these stockpiles
} Workshop;

typedef enum {
    WORKSHOP_STONECUTTER,
    WORKSHOP_SAWMILL,           // Future
    WORKSHOP_TYPE_COUNT,
} WorkshopType;

#define MAX_WORKSHOPS 256
#define MAX_BILLS_PER_WORKSHOP 10
#define MAX_LINKED_STOCKPILES 4

extern Workshop workshops[MAX_WORKSHOPS];
extern int workshopCount;

// Recipe lookup by workshop type
extern Recipe* GetRecipesForWorkshop(WorkshopType type, int* outCount);
```

### Workshop Layout Example (3x3 Stonecutter)

```
 [I] [W] [O]      I = Input area (where crafter looks for items)
 [ ] [X] [ ]      W = Work tile (where crafter stands)
 [ ] [ ] [ ]      O = Output tile (where products spawn)
                  X = Workshop center
```

For a workshop at position (10, 10):
- `workTileX = 11, workTileY = 10` (center-ish)
- `outputTileX = 12, outputTileY = 10` (right side)
- Input search starts from workshop position, radius-based

---

## Recipe System

Recipes define what a workshop can make.

```c
typedef struct {
    const char* name;           // "Stone Block", "Bread", etc.
    
    // Inputs
    ItemType inputType;
    int inputCount;
    
    // Outputs  
    ItemType outputType;
    int outputCount;
    
    // Work time
    float workRequired;         // Seconds to complete
} Recipe;

// Example recipes
Recipe stonecutterRecipes[] = {
    { "Stone Block", ITEM_STONE, 1, ITEM_STONE_BLOCK, 4, 2.0f },
};

Recipe kitchenRecipes[] = {
    { "Bread", ITEM_FLOUR, 1, ITEM_BREAD, 1, 3.0f },
    { "Meal", ITEM_MEAT, 1, ITEM_MEAL, 1, 2.5f },
};
```

**Future extension:** Multiple input types per recipe (e.g., bread needs flour + water).

---

## Bill System (RimWorld-style)

Bills are work orders queued at a workshop. This is what makes crafting feel automated.

```c
typedef enum {
    BILL_DO_X_TIMES,            // Make exactly N, then stop
    BILL_DO_UNTIL_X,            // Make until stockpile has X of output
    BILL_DO_FOREVER,            // Repeat indefinitely
} BillMode;

typedef struct {
    int recipeIdx;              // Index into workshop's availableRecipes
    BillMode mode;
    int targetCount;            // For DO_X_TIMES: how many to make
                                // For DO_UNTIL_X: target stockpile count
    int completedCount;         // Progress for DO_X_TIMES
    
    int ingredientSearchRadius; // How far to look for inputs (tiles), 0 = unlimited
    bool suspended;             // Paused, skip this bill
} Bill;

#define MAX_BILLS_PER_WORKSHOP 10
```

### Bill Modes Explained

**DO_X_TIMES:** "Make 10 stone blocks" - completes 10, then stops.

**DO_UNTIL_X:** "Make stone blocks until I have 50" - checks stockpile count before each craft. If count >= 50, skips to next bill. Resumes when count drops below target.

**DO_FOREVER:** "Keep making stone blocks" - never stops (useful for always-needed items).

---

## Job Type and States

### New Job Type

```c
typedef enum {
    JOBTYPE_NONE,
    JOBTYPE_HAUL,
    JOBTYPE_MINE,
    JOBTYPE_BUILD,
    JOBTYPE_CRAFT,      // NEW
} JobType;
```

### Crafting Job State Machine (DF-style)

The crafter fetches materials themselves, then works at the workshop.

```
Workshop has bill, materials exist somewhere reachable
    │
    │ WorkGiver_Craft finds work, reserves item + workshop
    ▼
CRAFT_STEP_MOVING_TO_INPUT (walk to where input item is)
    │
    │ arrived at item
    ▼
CRAFT_STEP_PICKING_UP (pick up item, now carrying it)
    │
    ▼
CRAFT_STEP_MOVING_TO_WORKSHOP (walk to workshop work tile)
    │
    │ arrived at workshop
    ▼
CRAFT_STEP_WORKING (progress 0→1)
    │
    │ progress complete
    ▼
(consume carried item, spawn output at workshop output tile)
    │
    ▼
JOB_IDLE
```

**Key difference from simpler model:** Crafter walks to get the item, carries it, then works. This means:
- Crafter visibly carries material (satisfying!)
- Stockpile placement matters (short walk = fast crafting)
- Haulers and crafters have distinct roles even without professions

### Job Struct Fields for Crafting

```c
// In Job struct, these fields used for crafting:
int targetWorkshop;         // Workshop index
int targetBillIdx;          // Which bill we're working on
int targetItem;             // Item to fetch (reserved)
float progress;             // 0.0 to 1.0
float workRequired;         // From recipe
int step;                   // Which step of the craft job
```

### Craft Job Steps

```c
typedef enum {
    CRAFT_STEP_MOVING_TO_INPUT,     // Walking to item location
    CRAFT_STEP_PICKING_UP,          // At item, picking up
    CRAFT_STEP_MOVING_TO_WORKSHOP,  // Carrying item to workshop
    CRAFT_STEP_WORKING,             // At workshop, crafting
} CraftJobStep;
```

---

## Job Driver (DF-style)

```c
JobRunResult RunJob_Craft(Job* job, Mover* mover, float dt) {
    Workshop* ws = &workshops[job->targetWorkshop];
    Bill* bill = &ws->bills[job->targetBillIdx];
    Recipe* recipe = &ws->availableRecipes[bill->recipeIdx];
    Item* item = &items[job->targetItem];
    
    switch (job->step) {
        case CRAFT_STEP_MOVING_TO_INPUT:
            // Walk to where the input item is
            if (!mover->hasPath) {
                SetMoverGoal(mover, item->x, item->y, item->z);
            }
            if (MoverAtGoal(mover)) {
                job->step = CRAFT_STEP_PICKING_UP;
            }
            break;
            
        case CRAFT_STEP_PICKING_UP:
            // Pick up the item (instant)
            PickUpItem(mover, job->targetItem);
            job->step = CRAFT_STEP_MOVING_TO_WORKSHOP;
            break;
            
        case CRAFT_STEP_MOVING_TO_WORKSHOP:
            // Walk to workshop work tile (carrying item)
            if (!mover->hasPath) {
                SetMoverGoal(mover, ws->workTileX, ws->workTileY, ws->z);
            }
            if (MoverAtGoal(mover)) {
                job->step = CRAFT_STEP_WORKING;
                job->progress = 0.0f;
                job->workRequired = recipe->workRequired;
            }
            break;
            
        case CRAFT_STEP_WORKING:
            // Work at workshop
            job->progress += dt / job->workRequired;
            if (job->progress >= 1.0f) {
                // Consume carried item
                ConsumeCarriedItem(mover);
                
                // Spawn output at workshop output tile
                SpawnItem(recipe->outputType, 
                          ws->outputTileX, ws->outputTileY, ws->z,
                          recipe->outputCount);
                
                // Update bill progress
                bill->completedCount++;
                
                // Release workshop
                ws->assignedCrafter = -1;
                
                return JOBRUN_DONE;
            }
            break;
    }
    return JOBRUN_RUNNING;
}
```

### Helper Functions Needed

```c
// Pick up item - mover now carries it
void PickUpItem(Mover* mover, int itemIdx) {
    Item* item = &items[itemIdx];
    item->state = ITEM_CARRIED;
    item->carriedBy = GetMoverIndex(mover);
    mover->carryingItem = itemIdx;
    // Remove from stockpile slot if it was in one
    if (item->stockpileIdx >= 0) {
        ReleaseStockpileSlot(item->stockpileIdx, item->slotX, item->slotY);
        item->stockpileIdx = -1;
    }
}

// Consume carried item (destroy it)
void ConsumeCarriedItem(Mover* mover) {
    if (mover->carryingItem >= 0) {
        items[mover->carryingItem].active = false;
        mover->carryingItem = -1;
    }
}
```

---

## WorkGiver for Crafting (DF-style)

The WorkGiver finds a workshop with a runnable bill, then finds a specific input item to reserve.

```c
Job* WorkGiver_Craft(Mover* mover) {
    // Note: no canCraft check for now - any mover can craft
    
    for (int w = 0; w < workshopCount; w++) {
        Workshop* ws = &workshops[w];
        if (!ws->active) continue;
        if (ws->assignedCrafter >= 0) continue;  // Already has crafter
        
        // Find first non-suspended bill that can run
        for (int b = 0; b < ws->billCount; b++) {
            Bill* bill = &ws->bills[b];
            if (bill->suspended) continue;
            
            // Check if bill should run based on mode
            if (!ShouldBillRun(ws, bill)) continue;
            
            // Get recipe for this bill
            int recipeCount;
            Recipe* recipes = GetRecipesForWorkshop(ws->type, &recipeCount);
            Recipe* recipe = &recipes[bill->recipeIdx];
            
            // Find an input item (DF-style: search linked stockpiles or nearby)
            int itemIdx = FindInputItem(ws, recipe->inputType, bill->ingredientSearchRadius);
            if (itemIdx < 0) continue;  // No materials available
            
            // Check if mover can reach the item
            Item* item = &items[itemIdx];
            if (!CanMoverReach(mover, item->x, item->y, item->z)) continue;
            
            // Check if mover can reach the workshop
            if (!CanMoverReach(mover, ws->workTileX, ws->workTileY, ws->z)) continue;
            
            // Reserve item and workshop
            item->reservedBy = GetMoverIndex(mover);
            ws->assignedCrafter = GetMoverIndex(mover);
            
            // Create job
            Job* job = AllocateJob();
            job->type = JOBTYPE_CRAFT;
            job->targetWorkshop = w;
            job->targetBillIdx = b;
            job->targetItem = itemIdx;
            job->assignedMover = GetMoverIndex(mover);
            job->step = CRAFT_STEP_MOVING_TO_INPUT;
            
            return job;
        }
    }
    return NULL;
}

// Find input item for crafting (DF-style search)
int FindInputItem(Workshop* ws, ItemType type, int searchRadius) {
    // If workshop has linked stockpiles, ONLY search those
    if (ws->linkedInputCount > 0) {
        for (int i = 0; i < ws->linkedInputCount; i++) {
            int spIdx = ws->linkedInputStockpiles[i];
            int itemIdx = FindItemInStockpile(spIdx, type);
            if (itemIdx >= 0 && items[itemIdx].reservedBy < 0) {
                return itemIdx;
            }
        }
        return -1;  // No items in linked stockpiles
    }
    
    // Otherwise, search by radius from workshop (or unlimited if radius = 0)
    return FindNearestUnreservedItem(ws->x, ws->y, ws->z, type, searchRadius);
}

bool ShouldBillRun(Workshop* ws, Bill* bill) {
    int recipeCount;
    Recipe* recipes = GetRecipesForWorkshop(ws->type, &recipeCount);
    Recipe* recipe = &recipes[bill->recipeIdx];
    
    switch (bill->mode) {
        case BILL_DO_X_TIMES:
            return bill->completedCount < bill->targetCount;
            
        case BILL_DO_UNTIL_X:
            // Count output items in stockpiles
            int currentCount = CountItemsInStockpiles(recipe->outputType);
            return currentCount < bill->targetCount;
            
        case BILL_DO_FOREVER:
            return true;
    }
    return false;
}
```

### Item Search Priority (DF-style)

1. If workshop has linked input stockpiles → ONLY search those
2. Otherwise → search all items within `ingredientSearchRadius` of workshop
3. If `ingredientSearchRadius = 0` → search everywhere (no limit)

This lets players control material flow:
- Link a "granite only" stockpile → workshop only uses granite
- No links → workshop grabs nearest available material

---

## Job Cancellation / Cleanup

When a craft job is cancelled or fails, we must release reservations:

```c
void CancelCraftJob(Job* job) {
    // Release item reservation
    if (job->targetItem >= 0) {
        items[job->targetItem].reservedBy = -1;
        
        // If mover was carrying the item, drop it
        Mover* mover = &movers[job->assignedMover];
        if (mover->carryingItem == job->targetItem) {
            DropCarriedItem(mover);  // Item goes back to ground
        }
    }
    
    // Release workshop reservation
    if (job->targetWorkshop >= 0) {
        workshops[job->targetWorkshop].assignedCrafter = -1;
    }
    
    // Release job
    job->active = false;
}

void DropCarriedItem(Mover* mover) {
    if (mover->carryingItem >= 0) {
        Item* item = &items[mover->carryingItem];
        item->state = ITEM_ON_GROUND;
        item->x = (int)mover->x;
        item->y = (int)mover->y;
        item->z = mover->z;
        item->carriedBy = -1;
        item->reservedBy = -1;
        mover->carryingItem = -1;
    }
}
```

**When does cancellation happen?**
- Mover dies or becomes inactive
- Path to item becomes blocked
- Path to workshop becomes blocked
- Bill gets suspended mid-job
- Player cancels the bill

---

## Workshop-Stockpile Integration

### Key Insight: Bills Don't Generate Hauling Jobs

**Haulers don't know about workshops or bills.** They only care about stockpile priority.

The material flow works like this:

```
ORANGE spawns (from mining)
    │
    │ Hauler finds best stockpile accepting ORANGE
    │ (highest priority wins)
    ▼
ORANGE ends up in Input Stockpile
    │
    │ Crafter sees workshop has bill + materials nearby
    │ (bill just answers: "should we craft?")
    ▼
Crafter fetches ORANGE, crafts, STONE_BLOCKS spawn
    │
    │ Hauler finds best stockpile accepting STONE_BLOCKS
    ▼
STONE_BLOCKS end up in Output Stockpile
```

**Bills only decide:** "Should this workshop try to craft right now?"
- Are materials available? (crafter searches)
- Is the bill suspended?
- Has the bill reached its target?

Bills do NOT create hauling jobs. Hauling is independent.

### Pattern: Critical Stockpiles Near Workshops

Players create efficiency through stockpile placement:

```
[Input Stockpile]  [Workshop]  [Output Stockpile]
 (high priority)                (high priority)
 (accepts ORANGE)               (accepts BLOCKS)
```

- Haulers fill input stockpile because it's high priority
- Crafter walks short distance to grab materials
- Output spawns, haulers take it away

This is the DF pattern: haulers + stockpiles handle logistics, crafters just fetch nearby.

### Ingredient Search Radius

Bills have `ingredientSearchRadius` - how far the crafter looks for inputs:
- `0` = unlimited (search all reachable items)
- `5` = only items within 5 tiles of workshop input position
- Encourages stockpile placement strategy

### Future: Linked Stockpiles

```c
typedef struct {
    // ... existing stockpile fields ...
    int linkedWorkshop;         // -1 = standalone
    bool isInputStockpile;      // true = feeds workshop
} Stockpile;
```

Workshop could auto-identify its input/output stockpiles based on adjacency.

---

## Hauling Integration

Crafting creates hauling demand:

1. **Input hauling:** Items need to reach workshop input area
   - Existing haul system handles this (haul to nearby stockpile)
   - Or: new job type `JOBTYPE_HAUL_TO_WORKSHOP` for direct delivery

2. **Output hauling:** Products spawn at workshop output position
   - Existing haul system picks them up as ground items
   - Hauls to appropriate stockpile based on item type

**No new hauling code needed initially** - existing system should work if:
- Input stockpile near workshop accepts raw materials
- Output items spawn on ground, get hauled like any other item

---

## Item Types to Add

Current items are abstract (RED, GREEN, BLUE, ORANGE). For meaningful crafting:

```c
typedef enum {
    // Raw materials (from mining/gathering)
    ITEM_STONE,             // From mining stone walls
    ITEM_WOOD,              // From chopping trees (future)
    
    // Processed materials (from crafting)
    ITEM_STONE_BLOCK,       // Crafted from stone, used for building
    ITEM_PLANK,             // Crafted from wood (future)
    
    // Keep existing for testing
    ITEM_RED,
    ITEM_GREEN,
    ITEM_BLUE,
    ITEM_ORANGE,
    
    ITEM_TYPE_COUNT,
} ItemType;
```

**Alternative:** Keep ITEM_ORANGE as "stone" for now, add ITEM_STONE_BLOCK as crafted output. Minimal changes.

---

## UI Requirements

### Workshop Placement
- Select workshop type from build menu
- Click to place (like blueprints)
- Workshop blueprint requires materials + construction

### Bill Management
- Click workshop to open bill panel
- Add bill: select recipe, set mode, set target count
- Reorder bills (drag or up/down buttons)
- Suspend/resume bills
- Delete bills

### Workshop Status Display
- Show current bill being worked
- Show progress bar
- Show assigned crafter (if any)
- Show "waiting for materials" if stuck

---

## Implementation Steps

### Phase 1: Core Workshop System
- [x] Add `Workshop` struct and array
- [x] Add `Recipe` struct
- [x] Add workshop placement (T key in draw mode)
- [x] Add `JOBTYPE_CRAFT` and job driver
- [x] Add `WorkGiver_Craft` (integrated into AssignJobsHybrid)
- [x] Test: mover walks to workshop, waits, item spawns

### Phase 2: Bill System
- [x] Add `Bill` struct to workshops
- [x] Implement `BILL_DO_X_TIMES` mode
- [x] Implement `BILL_DO_UNTIL_X` mode
- [x] Implement `BILL_DO_FOREVER` mode
- [x] Add bill suspend/resume (P key)

### Phase 3: Material Consumption
- [x] Check for input items before starting craft
- [x] Consume input items when craft completes
- [x] Handle "not enough materials" state (job not assigned)
- [x] Add ingredient search radius (0 = unlimited)

### Phase 4: UI
- [x] Workshop placement UI (T key, 3x3 preview)
- [x] Bill management (B=add, X=remove, P=pause, D=delete workshop)
- [x] Workshop status display (tooltip)
- [ ] Recipe selection UI (not needed yet - single recipe)

### Phase 5: Polish
- [x] Workshop templates (ASCII layout with blocked tiles)
- [x] Pathfinding around workshop machinery (CELL_FLAG_WORKSHOP_BLOCK)
- [x] Save/load support
- [ ] Multiple workshop types (future)
- [ ] Workshop construction (future)
- [ ] Crafter skill affecting speed (future)
- [ ] Quality system for outputs (future)

---

## Testing Checklist

- [x] Mover finds workshop with runnable bill
- [x] Mover paths to workshop correctly
- [x] Craft progress increments over time
- [x] Output item spawns at correct position
- [x] Input items consumed correctly
- [x] Bill completedCount increments
- [x] DO_X_TIMES stops after N crafts
- [x] DO_UNTIL_X stops when stockpile target met
- [x] DO_UNTIL_X resumes when stockpile drops below target
- [x] DO_FOREVER keeps running
- [x] Suspended bills are skipped
- [x] No materials = no craft job assigned
- [x] Workshop with assigned crafter not double-assigned
- [x] Job cancellation releases workshop assignment
- [x] Multiple workshops work independently

---

## Decisions Made

**When are items consumed?**
→ **Reserve at job start, consume at job end** (DF-style). Crafter picks up item (reserved), carries it, and it's only destroyed when crafting completes successfully.

**How to handle craft interruption?**
→ **Cancel, lose progress, drop item** (DF-style). Item goes back to ground at crafter's position, can be picked up again. No progress saved.

**Workshop types as enum or data-driven?**
→ **Enum** for now (simpler). Can refactor to data-driven later if needed.

**One crafter per workshop, or multiple?**
→ **Start with one, design for multiple**. Keep the door open for multiple crafters at larger workshops later.

---

## References

- [jobs-roadmap.md Phase 4](../ideas/jobs-roadmap.md) - Original design
- [next-steps-analysis.md](../ideas/next-steps-analysis.md) - RimWorld bill system analysis
- [RimWorld Wiki - Bills](https://rimworldwiki.com/wiki/Bill) - Inspiration
