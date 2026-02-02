# Channeling Implementation Plan

## Overview

Channeling is vertical digging - removing the floor beneath a cell AND mining out the z-level below simultaneously. Unlike mining (horizontal wall removal), channeling works from above and affects two z-levels at once.

**Key DF behavior**: "A channel is a hole dug in the ground or wall, which will mine out the z-level below, too."

## Behavior Summary

1. Player designates a tile for channeling at z-level N
2. Mover walks to that tile on z-level N  
3. Mover performs channeling work (standing on the tile they're channeling)
4. Result:
   - Floor at z-level N is removed
   - Cell at z-level N-1 is mined out (wall becomes air/ramp)
   - Ramp is created at N-1 if there's solid material to carve it from
5. Mover descends into the channel and can continue channeling downward

**DF quote**: "The dwarf will walk down into the channel he's just made, and dig the next channel down"

## Key Decisions

### Where does the mover stand?

**Option A: Stand on the tile being channeled**
- Simpler pathfinding (just go to tile)
- Risk: mover falls when work completes
- DF approach: mover "catches themselves" - no fall damage from channeling

**Option B: Stand on adjacent tile**
- Safer - no fall risk
- More complex - need to find adjacent walkable tile (like mining)
- May not always be possible (isolated tile)

**Recommendation: Option A** - Stand on the channeled tile, handle the fall gracefully. This matches DF behavior and is simpler. When channeling completes:
- If ramp created below: mover walks down naturally
- If hole created: mover falls to z-1 (no damage from controlled channeling)

### What is created when channeling completes?

**DF behavior**: Channeling always creates a ramp on the level below IF there was solid material there. "Channels dug above a dug-out area will not create ramps" - meaning if z-1 is already air, no ramp is created.

**Option A: Always create air (hole)**
- Simplest implementation
- Player must build ramp separately if they want access
- Movers/items fall through

**Option B: Always create a ramp (if solid below)**
- Matches DF behavior
- More useful - immediate access to lower level
- Direction: Auto-detect from adjacent walls

**Option C: Player chooses (dig vs channel-ramp)**
- Most flexible
- UI complexity - two designation types

**Recommendation: Option B** - Match DF behavior
- If z-1 has solid material (wall), mine it out and create a ramp
- If z-1 is already air/open, just remove the floor (no ramp created)
- Ramp direction: use `AutoDetectRampDirection()` based on adjacent walls
- If no adjacent wall for ramp direction, create floor instead of ramp (mover can stand there)

### Can you channel at z=0?

No - there's nothing below z=0. Designation should fail for cells at z=0.

### What happens to items on the channeled tile?

Push them to an adjacent tile (like `CompleteBlueprint` does with `PushItemsOutOfCell`).

### What about movers other than the channeler?

Push them out before completing (like blueprints do). The channeling mover handles their own descent.

### What can be channeled?

1. **Floor tiles** (most common) - removes floor, mines z-1
2. **Wall tiles** - removes wall AND floor, mines z-1 (like DF: "channeling of floor or wall tiles")

This makes channeling very powerful - it's essentially "strip mining" from above.

### Safety Considerations (from DF)

**Cave-in Risk**: "Unsupported channeling of large areas tends to cause cave-ins"
- For now: we don't have cave-in physics, so ignore this
- Future: could add structural integrity checks

**Channeling into open areas**: "The miner can become seriously injured if the shaft intersects a cavern or another open area underground"
- If z-1 is already open (air), mover falls further than expected
- For now: mover just ends up at z-1, no damage
- Future: could add fall damage for unexpected drops

**Continuous channeling**: Miners can channel "five or less" z-levels before needing to climb out
- The ramp created allows them to walk back up
- If channeling into open air (no ramp), they get stuck at the bottom

---

## Implementation Details

### 1. New Designation Type

In `designations.h`:
```c
typedef enum {
    DESIGNATION_NONE,
    DESIGNATION_MINE,
    DESIGNATION_CHANNEL,  // NEW
} DesignationType;
```

Add `CHANNEL_WORK_TIME`:
```c
#define CHANNEL_WORK_TIME 2.0f  // Same as mining for now
```

### 2. New Job Type

In `jobs.h`:
```c
typedef enum {
    JOBTYPE_HAUL,
    JOBTYPE_CLEAR,
    JOBTYPE_MINE,
    JOBTYPE_CHANNEL,  // NEW
    // ...
} JobType;
```

Add target fields (reuse mine targets or add channel-specific ones):
```c
// In Job struct - can reuse targetMineX/Y/Z as targetChannelX/Y/Z
// Or add explicit fields:
int targetChannelX, targetChannelY, targetChannelZ;
```

**Decision: Reuse targetMineX/Y/Z** - They serve the same purpose (target cell coordinates). Add a comment noting they're used for both mine and channel jobs.

### 3. Designation Functions

In `designations.h` and `designations.c`:

```c
// Designate a cell for channeling
// Returns true if designation was added, false if cell can't be channeled
bool DesignateChannel(int x, int y, int z);

// Check if a cell has a channel designation
bool HasChannelDesignation(int x, int y, int z);

// Complete a channel designation
// Removes floor, creates ramp if possible, handles mover descent
void CompleteChannelDesignation(int x, int y, int z, int channelerMoverIdx);

// Count active channel designations
int CountChannelDesignations();
```

### 4. DesignateChannel Implementation

```c
bool DesignateChannel(int x, int y, int z) {
    // Bounds check
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    
    // Can't channel at z=0 (nothing below)
    if (z == 0) {
        return false;
    }
    
    // Already designated?
    if (designations[z][y][x].type != DESIGNATION_NONE) {
        return false;
    }
    
    // Can channel:
    // 1. Walkable floor tiles (removes floor, mines below)
    // 2. Wall tiles that are accessible from above (removes wall+floor, mines below)
    //
    // For simplicity, start with just walkable tiles. 
    // The mover needs to stand on the tile to channel it.
    if (!IsCellWalkableAt(z, y, x)) {
        return false;
    }
    
    // In DF-style mode, verify there's a floor to remove
    if (!g_legacyWalkability && !HAS_FLOOR(x, y, z)) {
        return false;  // No floor to channel (already open)
    }
    
    designations[z][y][x].type = DESIGNATION_CHANNEL;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    activeDesignationCount++;
    
    return true;
}
```

### 5. CompleteChannelDesignation Implementation

This is the core logic. Key DF behavior:
- Removes floor at z
- Mines out z-1 (if solid) and creates ramp
- If z-1 was already open, no ramp is created

```c
void CompleteChannelDesignation(int x, int y, int z, int channelerMoverIdx) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z <= 0 || z >= gridDepth) {
        return;
    }
    
    int lowerZ = z - 1;
    
    // Push items out of this cell (at z)
    PushItemsOutOfCell(x, y, z);
    
    // Push OTHER movers out (not the channeler)
    for (int i = 0; i < moverCount; i++) {
        if (i == channelerMoverIdx) continue;
        Mover* m = &movers[i];
        if (!m->active) continue;
        int mx = (int)(m->x / CELL_SIZE);
        int my = (int)(m->y / CELL_SIZE);
        int mz = (int)m->z;
        if (mx == x && my == y && mz == z) {
            PushMoverToAdjacentCell(i, x, y, z);
        }
    }
    
    // === STEP 1: Remove the floor at z ===
    if (g_legacyWalkability) {
        // Legacy mode: set to air
        grid[z][y][x] = CELL_AIR;
    } else {
        // DF-style: clear floor flag, cell becomes air
        CLEAR_FLOOR(x, y, z);
        grid[z][y][x] = CELL_AIR;
    }
    
    // === STEP 2: Mine out z-1 and determine what to create ===
    CellType cellBelow = grid[lowerZ][y][x];
    bool wassolid = (cellBelow == CELL_WALL || cellBelow == CELL_WOOD_WALL || 
                     cellBelow == CELL_STONE);
    
    if (wassolid) {
        // z-1 was solid - mine it out and create a ramp
        // Try to auto-detect ramp direction from adjacent walls at z-1
        CellType rampDir = AutoDetectRampDirection(x, y, lowerZ);
        
        if (rampDir != CELL_AIR) {
            // Create ramp facing the adjacent wall
            grid[lowerZ][y][x] = rampDir;
        } else {
            // No adjacent wall - create floor instead (mover can stand here)
            if (g_legacyWalkability) {
                grid[lowerZ][y][x] = CELL_FLOOR;
            } else {
                grid[lowerZ][y][x] = CELL_AIR;
                SET_FLOOR(x, y, lowerZ);
            }
        }
        
        // Spawn stone from the mined material
        SpawnItem(x * CELL_SIZE + CELL_SIZE * 0.5f, 
                  y * CELL_SIZE + CELL_SIZE * 0.5f, 
                  (float)lowerZ, ITEM_ORANGE);
    }
    // else: z-1 was already open - no ramp created (DF behavior)
    // "Channels dug above a dug-out area will not create ramps"
    
    // Spawn debris from the floor we removed at z
    SpawnItem(x * CELL_SIZE + CELL_SIZE * 0.5f, 
              y * CELL_SIZE + CELL_SIZE * 0.5f, 
              (float)lowerZ, ITEM_ORANGE);  // Could be dirt vs stone based on floor type
    
    MarkChunkDirty(x, y, z);
    MarkChunkDirty(x, y, lowerZ);
    
    // Destabilize water so it can flow into the hole
    DestabilizeWater(x, y, z);
    DestabilizeWater(x, y, lowerZ);
    
    // === STEP 3: Handle channeler descent ===
    if (channelerMoverIdx >= 0 && channelerMoverIdx < moverCount) {
        Mover* channeler = &movers[channelerMoverIdx];
        // Move mover to z-1 - they descend into their channel
        channeler->z = (float)lowerZ;
        // Position them at center of the cell
        channeler->x = x * CELL_SIZE + CELL_SIZE * 0.5f;
        channeler->y = y * CELL_SIZE + CELL_SIZE * 0.5f;
        // They're now on the ramp/floor they just created
        // Clear their path since they've moved
        channeler->pathLength = 0;
        channeler->pathIndex = -1;
    }
    
    // === STEP 4: Clear designation ===
    if (designations[z][y][x].type != DESIGNATION_NONE) {
        activeDesignationCount--;
    }
    designations[z][y][x].type = DESIGNATION_NONE;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
}
```

### 6. Job Driver: RunJob_Channel

```c
JobRunResult RunJob_Channel(Job* job, void* moverPtr, float dt) {
    Mover* mover = (Mover*)moverPtr;
    
    int tx = job->targetMineX;  // Reusing mine target fields
    int ty = job->targetMineY;
    int tz = job->targetMineZ;
    
    // Check if designation still exists
    Designation* d = GetDesignation(tx, ty, tz);
    if (!d || d->type != DESIGNATION_CHANNEL) {
        return JOBRUN_FAIL;
    }
    
    // Check if floor was already removed (by someone else)
    if (!HAS_FLOOR(tx, ty, tz)) {
        CancelDesignation(tx, ty, tz);
        return JOBRUN_FAIL;
    }
    
    if (job->step == STEP_MOVING_TO_WORK) {
        // Move to the target cell (stand on it)
        if (mover->goal.x != tx || mover->goal.y != ty || mover->goal.z != tz) {
            mover->goal = (Point){tx, ty, tz};
            mover->needsRepath = true;
        }
        
        // Check if arrived
        float goalX = tx * CELL_SIZE + CELL_SIZE * 0.5f;
        float goalY = ty * CELL_SIZE + CELL_SIZE * 0.5f;
        float dx = mover->x - goalX;
        float dy = mover->y - goalY;
        float distSq = dx*dx + dy*dy;
        bool correctZ = (int)mover->z == tz;
        
        // Check if stuck
        if (mover->pathLength == 0 && mover->timeWithoutProgress > JOB_STUCK_TIME) {
            d->unreachableCooldown = UNREACHABLE_COOLDOWN;
            return JOBRUN_FAIL;
        }
        
        if (correctZ && distSq < PICKUP_RADIUS * PICKUP_RADIUS) {
            job->step = STEP_WORKING;
        }
        
        return JOBRUN_RUNNING;
    }
    else if (job->step == STEP_WORKING) {
        // Progress channeling
        job->progress += dt / CHANNEL_WORK_TIME;
        d->progress = job->progress;
        
        if (job->progress >= 1.0f) {
            // Channeling complete!
            // Pass mover index so CompleteChannelDesignation can handle their descent
            CompleteChannelDesignation(tx, ty, tz, job->assignedMover);
            return JOBRUN_DONE;
        }
        
        return JOBRUN_RUNNING;
    }
    
    return JOBRUN_FAIL;
}
```

### 7. Job Assignment: WorkGiver_Channel

Similar to `WorkGiver_Mining` but:
- Looks for `DESIGNATION_CHANNEL` instead of `DESIGNATION_MINE`
- Doesn't need to find adjacent tile (mover stands on target)
- Creates `JOBTYPE_CHANNEL` job

```c
int WorkGiver_Channel(int moverIdx) {
    Mover* m = &movers[moverIdx];
    
    // Check capability - reuse canMine for now (channeling is a form of digging)
    if (!m->capabilities.canMine) return -1;
    
    int moverZ = (int)m->z;
    
    // Find nearest unassigned channel designation
    // Could build a cache like mining, or just scan (fewer channel designations typically)
    int bestX = -1, bestY = -1, bestZ = -1;
    float bestDistSq = 1e30f;
    
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                Designation* d = &designations[z][y][x];
                if (d->type != DESIGNATION_CHANNEL) continue;
                if (d->assignedMover != -1) continue;
                if (d->unreachableCooldown > 0.0f) continue;
                
                // Same z-level only for now
                if (z != moverZ) continue;
                
                float posX = x * CELL_SIZE + CELL_SIZE * 0.5f;
                float posY = y * CELL_SIZE + CELL_SIZE * 0.5f;
                float dx = posX - m->x;
                float dy = posY - m->y;
                float distSq = dx*dx + dy*dy;
                
                if (distSq < bestDistSq) {
                    bestDistSq = distSq;
                    bestX = x;
                    bestY = y;
                    bestZ = z;
                }
            }
        }
    }
    
    if (bestX < 0) return -1;
    
    // Check reachability
    Point moverCell = { (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), moverZ };
    Point targetCell = { bestX, bestY, bestZ };
    
    Point tempPath[MAX_PATH];
    int tempLen = FindPath(moverPathAlgorithm, moverCell, targetCell, tempPath, MAX_PATH);
    
    if (tempLen == 0) {
        Designation* d = GetDesignation(bestX, bestY, bestZ);
        if (d) d->unreachableCooldown = UNREACHABLE_COOLDOWN;
        return -1;
    }
    
    // Create job
    int jobId = CreateJob(JOBTYPE_CHANNEL);
    if (jobId < 0) return -1;
    
    Job* job = GetJob(jobId);
    job->assignedMover = moverIdx;
    job->targetMineX = bestX;  // Reusing mine target fields
    job->targetMineY = bestY;
    job->targetMineZ = bestZ;
    job->step = STEP_MOVING_TO_WORK;
    job->progress = 0.0f;
    
    // Reserve designation
    Designation* d = GetDesignation(bestX, bestY, bestZ);
    d->assignedMover = moverIdx;
    
    // Update mover
    m->currentJobId = jobId;
    m->goal = targetCell;
    m->needsRepath = true;
    
    RemoveMoverFromIdleList(moverIdx);
    
    return jobId;
}
```

### 8. Input Handling

Add channeling to the work mode menu:

In `input_mode.c`:
```c
case MODE_WORK:
    n = AddExitHeader(items, n, "WORK:", KEY_W, 0);
    n = AddItem(items, n, "Mine", KEY_D, 0, false, false, false);
    n = AddItem(items, n, "Channel", KEY_C, 0, false, false, false);  // NEW
    n = AddItem(items, n, "Construct", KEY_B, 0, false, false, false);  // Move to B for Build
    // ...
```

In `input.c`, add `ExecuteDesignateChannel`:
```c
static void ExecuteDesignateChannel(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (DesignateChannel(dx, dy, z)) count++;
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Designated %d cell%s for channeling", count, count > 1 ? "s" : ""), ORANGE);
    }
}
```

### 9. Rendering

Add channel designation rendering in `rendering.c`:
- Use a different color or icon than mining
- Show progress like mining designations

### 10. Integration Points

Update these to handle `DESIGNATION_CHANNEL`:
- `DesignationsTick()` - already handles all designations generically
- `CancelDesignation()` - already works for any type
- `GetDesignation()` - already returns any type
- Job assignment in `AssignJobsHybrid()` - add WorkGiver_Channel call
- Job driver table - add `[JOBTYPE_CHANNEL] = RunJob_Channel`
- `CancelJob()` - handle channel job cancellation (release designation)

---

## UI/UX

### Key Binding
- **M** for Mine (already set)
- **C** for Channel (conflicts with current Construct, move Construct to **B** for Build)

### Visual Feedback
- Designation overlay: distinct from mining (maybe cyan vs orange?)
- Progress indicator: same as mining (fill overlay)
- Tooltip: "Channeling (50%)" when hovering

### Messages
- "Designated N cells for channeling"
- "Cancelled N channel designations"
- No explicit completion message (just see the hole appear)

---

## Edge Cases

1. **Channeling would isolate the mover** - They fall down, which is fine. If there's no way back up, that's the player's problem (like DF).

2. **Channeling into water at z-1** - Water might flow up through the hole. Need to handle water interactions (destabilize water at both levels).

3. **Channeling creates a ramp but mover was at edge** - Mover ends up on the ramp, can walk off normally.

4. **Multiple channel designations creating a pit** - Each completes independently. Movers fall into the expanding pit.

5. **Channeling a tile with a mover on it (not the channeler)** - Push them out first, like blueprints do.

---

## Testing

### Unit Tests

1. `DesignateChannel` only works on walkable floor tiles with floors
2. `DesignateChannel` fails at z=0
3. `DesignateChannel` fails on walls/air
4. `CompleteChannelDesignation` removes floor flag
5. `CompleteChannelDesignation` creates ramp when adjacent wall exists
6. `CompleteChannelDesignation` creates air when no adjacent wall
7. `CompleteChannelDesignation` spawns item
8. `CompleteChannelDesignation` moves channeler to z-1
9. Job assignment finds channel designations
10. Job completes and designation is cleared
11. Cancelling designation releases mover

### Integration Tests

1. Mover walks to tile, channels, ends up at z-1
2. Multiple movers can channel different tiles
3. Channeling creates usable ramp for pathfinding
4. Water flows into channeled hole

---

## Estimated Changes

| File | Changes |
|------|---------|
| `designations.h` | +15 lines (enum, functions, constant) |
| `designations.c` | +80 lines (DesignateChannel, CompleteChannel, helpers) |
| `jobs.h` | +5 lines (JOBTYPE_CHANNEL, maybe reuse target fields) |
| `jobs.c` | +100 lines (RunJob_Channel, WorkGiver_Channel, driver entry) |
| `input.c` | +20 lines (ExecuteDesignateChannel, key handling) |
| `input_mode.c` | +5 lines (menu item) |
| `rendering.c` | +20 lines (channel designation rendering) |
| `test_jobs.c` | +150 lines (channel tests) |

**Total: ~400 lines**

---

## Continuous Shaft Digging (Future Enhancement)

In DF, miners can rapidly dig vertical shafts by designating multiple z-levels for channeling:

> "The dwarf will walk down into the channel he's just made, and dig the next channel down"

**How this would work:**
1. Player designates channel at z=5
2. Player also designates channel at z=4 (same x,y)
3. Mover channels z=5, descends to z=4
4. Mover is now standing on the z=4 designation
5. Mover immediately starts channeling z=4
6. Repeat until all stacked designations complete

**Implementation notes:**
- The WorkGiver would need to check if mover is standing on a channel designation
- Or: after completing a channel, check if there's another designation at the new z level
- This creates efficient shaft digging without manual intervention

**For now:** Not implementing - each channel is independent. Player can designate multiple levels, but mover returns to idle after each one and must be re-assigned.

---

## Open Questions for Later

1. Should there be a separate "dig ramp" designation that always creates a ramp (fails if no wall)?
2. Fall damage for non-channeling falls?
3. Should channeling check if the destination z-level is safe (not lava, deep water)?
4. Multi-z channeling (dig down multiple levels at once)?
5. Stacked channel designations for continuous shaft digging?
