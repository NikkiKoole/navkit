# Channeling Implementation Plan

## References

- DF Wiki - Channel: https://dwarffortresswiki.org/index.php/DF2014:Channel
- DF Wiki - Ramp: https://dwarffortresswiki.org/index.php/DF2014:Ramp

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
- Ramp direction: use special channeling ramp logic (see below)

### How DF Ramps Work (important for channeling)

From the DF wiki, a ramp needs 4 tiles to be usable:
1. The ramp tile itself (at z-1)
2. Open space directly above it (the hole at z - this is the "▼" downward slope)
3. An adjacent solid tile on the same level (wall at z-1)
4. A walkable tile above that solid tile (floor at z, adjacent to the hole)

**Key insight:** The mover walks UP the ramp and exits onto an adjacent tile at z+1. They don't need walkable space at z-1 (the low side) - they climb UP and OUT.

Our existing `AutoDetectRampDirection()` / `CanPlaceRamp()` requires a walkable low-side at the same z-level, which is wrong for channeling. For channeling, we need **relaxed ramp placement**:

```c
// For channeling: find a ramp direction where:
// 1. There's a wall adjacent at z-1 (the "high side" base)
// 2. There's walkable floor at z above that wall (the exit point)
// We DON'T require walkable space at z-1 (mover climbs out, not in)
CellType AutoDetectChannelRampDirection(int x, int y, int lowerZ) {
    int upperZ = lowerZ + 1;
    
    // Try each direction: the ramp faces toward an adjacent wall
    // RAMP_N means "wall to the north, exit to the north at z+1"
    int dirs[4][3] = {
        {0, -1, CELL_RAMP_N},  // North
        {1, 0, CELL_RAMP_E},   // East
        {0, 1, CELL_RAMP_S},   // South
        {-1, 0, CELL_RAMP_W},  // West
    };
    
    for (int i = 0; i < 4; i++) {
        int dx = dirs[i][0];
        int dy = dirs[i][1];
        CellType rampType = (CellType)dirs[i][2];
        
        int adjX = x + dx;
        int adjY = y + dy;
        
        // Bounds check
        if (adjX < 0 || adjX >= gridWidth || adjY < 0 || adjY >= gridHeight) continue;
        
        // Adjacent cell at z-1 should be solid (wall) - this is the "high side base"
        CellType adjBelow = grid[lowerZ][adjY][adjX];
        bool adjIsSolid = (adjBelow == CELL_WALL || adjBelow == CELL_WOOD_WALL || 
                           adjBelow == CELL_STONE || adjBelow == CELL_DIRT);
        if (!adjIsSolid) continue;
        
        // Adjacent cell at z (above that wall) should be walkable - this is the exit
        if (!IsCellWalkableAt(upperZ, adjY, adjX)) continue;
        
        // This direction works!
        return rampType;
    }
    
    return CELL_AIR;  // No valid direction found
}
```

This ensures the mover can always climb out of their channel as long as there's a wall with walkable floor above it adjacent to the hole.

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
        // Use channeling-specific ramp detection (relaxed rules - no low-side check)
        CellType rampDir = AutoDetectChannelRampDirection(x, y, lowerZ);
        
        if (rampDir != CELL_AIR) {
            // Create ramp facing the adjacent wall
            // Mover can climb UP this ramp to exit at z (the hole level)
            grid[lowerZ][y][x] = rampDir;
            if (!g_legacyWalkability) {
                SET_FLOOR(x, y, lowerZ);  // Ramps need floor flag in DF-style mode
            }
        } else {
            // No valid ramp direction (no adjacent wall with walkable floor above)
            // This happens if channeling in the middle of a large open area
            // Create floor instead - mover is stuck but at least standing
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
    n = AddItem(items, n, "Mine", KEY_M, 0, false, false, false);
    n = AddItem(items, n, "Channel", KEY_H, 0, false, false, false);  // NEW - H for cHannel
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
- Job assignment in `AssignJobsHybrid()` - add `WorkGiver_Channel()` call **immediately after** `WorkGiver_Mining()` (channeling is another form of digging, similar priority)
- Job driver table - add `[JOBTYPE_CHANNEL] = RunJob_Channel`
- `CancelJob()` - handle channel job cancellation (release designation)

---

## UI/UX

### Key Binding
- **M** for Mine (already set)
- **H** for cHannel

### Visual Feedback
- Designation overlay: pink `(255, 150, 200, 200)` to distinguish from mining (cyan)
- Progress indicator: same as mining (fill overlay)
- Tooltip: "Channeling (50%)" when hovering

### Messages
- "Designated N cells for channeling"
- "Cancelled N channel designations"
- No explicit completion message (just see the hole appear)

---

## Edge Cases

1. **Channeling a 1-cell pit surrounded by walls** - `AutoDetectChannelRampDirection()` will find a valid direction because:
   - Adjacent walls exist at z-1 ✓
   - Walkable floor exists at z above those walls ✓
   - The mover can climb UP the ramp and exit onto the floor around the hole
   - **This now works correctly** thanks to relaxed ramp placement rules

2. **Channeling with no adjacent walls at z-1** - If channeling in the middle of a large open area where z-1 has no adjacent walls, no ramp can be created. Mover ends up on a floor tile and is stuck. This is an edge case (requires weird terrain).

3. **Channeling would isolate the mover** - They fall down, which is fine. If there's no way back up, that's the player's problem (like DF).

4. **Channeling into water at z-1** - Water might flow up through the hole. Need to handle water interactions (destabilize water at both levels).

5. **Channeling creates a ramp but mover was at edge** - Mover ends up on the ramp, can walk off normally.

6. **Multiple channel designations creating a pit** - Each completes independently. Movers fall into the expanding pit.

7. **Channeling a tile with a mover on it (not the channeler)** - Push them out first, like blueprints do.

---

## Testing

### Unit Tests

1. `DesignateChannel` only works on walkable floor tiles with floors
2. `DesignateChannel` fails at z=0
3. `DesignateChannel` fails on walls/air
4. `CompleteChannelDesignation` removes floor flag
5. `CompleteChannelDesignation` creates ramp when adjacent wall exists at z-1 with walkable floor at z
6. `CompleteChannelDesignation` creates floor when no valid ramp direction
7. `CompleteChannelDesignation` spawns item
8. `CompleteChannelDesignation` moves channeler to z-1
9. `AutoDetectChannelRampDirection` finds valid direction for 1-cell pit surrounded by walls
10. `AutoDetectChannelRampDirection` returns CELL_AIR when no adjacent walls
11. Job assignment finds channel designations
12. Job completes and designation is cleared
13. Cancelling designation releases mover
14. Mover can climb out of 1-cell channel via created ramp

### Integration Tests

1. Mover walks to tile, channels, ends up at z-1
2. Multiple movers can channel different tiles
3. Channeling creates usable ramp for pathfinding
4. Water flows into channeled hole

---

## End-to-End Test Specs

These tests verify the complete channeling workflow using the c89spec test framework.

### Test: Channel designation basics

```c
describe(channel_designation) {
    it("should designate a walkable floor tile for channeling") {
        // Two floors: z=1 has floor, z=0 has solid wall beneath
        const char* map =
            "floor:0\n"
            "#####\n"
            "#####\n"
            "#####\n"
            "floor:1\n"
            ".....\n"
            ".....\n"
            ".....\n";
        
        InitMultiFloorGridFromAscii(map, 5, 5);
        InitDesignations();
        
        // Designate center tile at z=1
        bool result = DesignateChannel(2, 1, 1);
        expect(result == true);
        expect(HasChannelDesignation(2, 1, 1) == true);
        expect(CountChannelDesignations() == 1);
    }
    
    it("should not designate at z=0 (nothing below)") {
        const char* map =
            "floor:0\n"
            ".....\n"
            ".....\n"
            ".....\n";
        
        InitMultiFloorGridFromAscii(map, 5, 5);
        InitDesignations();
        
        bool result = DesignateChannel(2, 1, 0);
        expect(result == false);
        expect(HasChannelDesignation(2, 1, 0) == false);
    }
    
    it("should not designate a wall tile") {
        const char* map =
            "floor:0\n"
            "#####\n"
            "#####\n"
            "#####\n";
        
        InitMultiFloorGridFromAscii(map, 5, 5);
        InitDesignations();
        
        bool result = DesignateChannel(2, 1, 0);
        expect(result == false);
    }
    
    it("should not designate tile without floor") {
        // z=1 is air (no floor flag)
        const char* map =
            "floor:0\n"
            ".....\n"
            ".....\n"
            ".....\n"
            "floor:1\n"
            "     \n"  // space = air, no floor
            "     \n"
            "     \n";
        
        InitMultiFloorGridFromAscii(map, 5, 5);
        InitDesignations();
        
        bool result = DesignateChannel(2, 1, 1);
        expect(result == false);
    }
}
```

### Test: Channel ramp direction detection

```c
describe(channel_ramp_direction) {
    it("should find valid ramp direction for 1-cell pit surrounded by walls") {
        // z=0: all walls, z=1: walkable floor
        const char* map =
            "floor:0\n"
            "#####\n"
            "#####\n"
            "#####\n"
            "floor:1\n"
            ".....\n"
            ".....\n"
            ".....\n";
        
        InitMultiFloorGridFromAscii(map, 5, 5);
        
        // Check center cell at z=0 (will be channeled from z=1)
        CellType rampDir = AutoDetectChannelRampDirection(2, 1, 0);
        
        // Should find a valid direction (any of N/E/S/W works)
        // All adjacent cells at z=0 are walls, all at z=1 are walkable
        expect(rampDir != CELL_AIR);
        expect(CellIsDirectionalRamp(rampDir));
    }
    
    it("should return CELL_AIR when no adjacent walls at z-1") {
        // z=0: all air/floor (no walls), z=1: walkable floor
        const char* map =
            "floor:0\n"
            ".....\n"
            ".....\n"
            ".....\n"
            "floor:1\n"
            ".....\n"
            ".....\n"
            ".....\n";
        
        InitMultiFloorGridFromAscii(map, 5, 5);
        
        CellType rampDir = AutoDetectChannelRampDirection(2, 1, 0);
        expect(rampDir == CELL_AIR);  // No walls to anchor ramp
    }
    
    it("should pick direction where z+1 is walkable") {
        // z=0: walls on all sides, z=1: only NORTH is walkable
        const char* map =
            "floor:0\n"
            "#####\n"
            "#####\n"
            "#####\n"
            "floor:1\n"
            ".....\n"  // row 0 - walkable
            "#.#.#\n"  // row 1 - mixed (center walkable for standing)
            "#####\n"; // row 2 - walls
        
        InitMultiFloorGridFromAscii(map, 5, 5);
        
        // Channel at (2,1,1), below is (2,1,0)
        // Adjacent at z=0: N(2,0), E(3,1), S(2,2), W(1,1) - all walls
        // Adjacent at z=1: N(2,0) walkable, E(3,1) wall, S(2,2) wall, W(1,1) wall
        // Should pick RAMP_N (exits north to walkable tile)
        CellType rampDir = AutoDetectChannelRampDirection(2, 1, 0);
        expect(rampDir == CELL_RAMP_N);
    }
}
```

### Test: Complete channel job execution

```c
describe(channel_job_execution) {
    it("should complete channel job: mover walks to tile, channels, descends to z-1") {
        const char* map =
            "floor:0\n"
            "#####\n"
            "#####\n"
            "#####\n"
            "floor:1\n"
            ".....\n"
            ".....\n"
            ".....\n";
        
        InitMultiFloorGridFromAscii(map, 5, 5);
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        InitDesignations();
        
        // Mover starts at (0,0,1)
        Mover* m = &movers[0];
        Point goal = {0, 0, 1};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 
                     0 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, goal, 100.0f);
        moverCount = 1;
        
        // Designate (2,1,1) for channeling
        DesignateChannel(2, 1, 1);
        
        int initialZ = (int)m->z;
        expect(initialZ == 1);
        
        // Run simulation until channel completes
        bool completed = false;
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            
            // Check if mover descended to z=0
            if ((int)m->z == 0) {
                completed = true;
                break;
            }
        }
        
        expect(completed == true);
        expect((int)m->z == 0);  // Mover descended
        expect(HasChannelDesignation(2, 1, 1) == false);  // Designation cleared
        expect(!HAS_FLOOR(2, 1, 1));  // Floor removed at z=1
        expect(grid[0][1][2] != CELL_WALL);  // Wall mined at z=0
    }
    
    it("should create ramp that mover can use to climb back out") {
        const char* map =
            "floor:0\n"
            "#####\n"
            "#####\n"
            "#####\n"
            "floor:1\n"
            ".....\n"
            ".....\n"
            ".....\n";
        
        InitMultiFloorGridFromAscii(map, 5, 5);
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        InitDesignations();
        
        // Mover starts at (0,0,1)
        Mover* m = &movers[0];
        Point goal = {0, 0, 1};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 
                     0 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, goal, 100.0f);
        moverCount = 1;
        
        // Designate (2,1,1) for channeling
        DesignateChannel(2, 1, 1);
        
        // Run until channel completes
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if ((int)m->z == 0) break;
        }
        
        expect((int)m->z == 0);  // Mover is at z=0
        
        // Verify a ramp was created
        CellType cell = grid[0][1][2];
        expect(CellIsDirectionalRamp(cell));
        
        // Now set mover's goal back to z=1 and verify they can path there
        m->goal = (Point){0, 0, 1};
        m->needsRepath = true;
        
        // Run until mover returns to z=1
        bool returnedToZ1 = false;
        for (int i = 0; i < 1000; i++) {
            Tick();
            if ((int)m->z == 1) {
                returnedToZ1 = true;
                break;
            }
        }
        
        expect(returnedToZ1 == true);  // Mover climbed out via ramp
    }
    
    it("should spawn item when channeling completes") {
        const char* map =
            "floor:0\n"
            "#####\n"
            "#####\n"
            "#####\n"
            "floor:1\n"
            ".....\n"
            ".....\n"
            ".....\n";
        
        InitMultiFloorGridFromAscii(map, 5, 5);
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        InitDesignations();
        
        Mover* m = &movers[0];
        Point goal = {0, 0, 1};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 
                     0 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, goal, 100.0f);
        moverCount = 1;
        
        int initialItemCount = GetActiveItemCount();
        
        DesignateChannel(2, 1, 1);
        
        // Run until channel completes
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if ((int)m->z == 0) break;
        }
        
        // Should have spawned at least one item (debris)
        expect(GetActiveItemCount() > initialItemCount);
    }
}
```

### Test: 1-cell corridor channeling (the tricky case)

```c
describe(channel_1cell_corridor) {
    it("should create usable ramp in 1-cell wide corridor") {
        // Classic 1-cell corridor: mover channels, falls into pit, must climb out
        const char* map =
            "floor:0\n"
            "#####\n"
            "#...#\n"  // 1-cell wide corridor at z=0
            "#####\n"
            "floor:1\n"
            "#####\n"
            "#...#\n"  // 1-cell wide corridor at z=1
            "#####\n";
        
        InitMultiFloorGridFromAscii(map, 5, 5);
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        InitDesignations();
        
        // Mover starts at (1,1,1) in the corridor
        Mover* m = &movers[0];
        Point goal = {1, 1, 1};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 
                     1 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, goal, 100.0f);
        moverCount = 1;
        
        // Designate middle of corridor (2,1,1) for channeling
        // Below at z=0 is floor (corridor), adjacent cells are walls
        DesignateChannel(2, 1, 1);
        
        // Run until channel completes
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if ((int)m->z == 0) break;
        }
        
        expect((int)m->z == 0);
        
        // Verify ramp was created at z=0
        CellType cell = grid[0][1][2];
        expect(CellIsDirectionalRamp(cell));
        
        // The ramp should face E or W (toward the corridor, not the walls N/S)
        // Actually it should face toward a wall with walkable floor above
        // Walls are N and S, corridor is E and W
        // At z=1: N(2,0) is wall, S(2,2) is wall, E(3,1) is floor, W(1,1) is floor
        // Ramp needs wall at z=0 with walkable at z=1 above it
        // N: wall at z=0, wall at z=1 -> no good (z=1 not walkable)
        // S: wall at z=0, wall at z=1 -> no good
        // E: floor at z=0, floor at z=1 -> no good (no wall at z=0)
        // W: floor at z=0, floor at z=1 -> no good
        // Hmm, this case is actually tricky - let me reconsider...
        
        // Actually in a 1-cell corridor, the walls are N and S
        // z=0: #.# (N=wall, center=floor, S=wall)  <- but we're channeling INTO solid
        // Wait, the map shows z=0 has a corridor too "#...#"
        // So z=0 at (2,1) is already floor, not wall!
        
        // Let me redo this test with solid below:
    }
    
    it("should handle 1-cell corridor with solid wall below") {
        // z=0 is solid, z=1 has 1-cell corridor
        const char* map =
            "floor:0\n"
            "#####\n"
            "#####\n"
            "#####\n"
            "floor:1\n"
            "#####\n"
            "#...#\n"  // 1-cell wide corridor
            "#####\n";
        
        InitMultiFloorGridFromAscii(map, 5, 5);
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        InitDesignations();
        
        // Mover starts at west end of corridor (1,1,1)
        Mover* m = &movers[0];
        Point goal = {1, 1, 1};
        InitMover(m, 1 * CELL_SIZE + CELL_SIZE * 0.5f, 
                     1 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, goal, 100.0f);
        moverCount = 1;
        
        // Designate center of corridor (2,1,1) for channeling
        // Below at (2,1,0) is wall
        // Adjacent at z=0: all walls (N,E,S,W)
        // Adjacent at z=1: N=wall, S=wall, E=floor, W=floor
        DesignateChannel(2, 1, 1);
        
        // Run until channel completes
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if ((int)m->z == 0) break;
        }
        
        expect((int)m->z == 0);
        
        // Verify ramp was created
        CellType cell = grid[0][1][2];
        expect(CellIsDirectionalRamp(cell));
        
        // Ramp should face E or W (the directions with walkable floor at z=1)
        // N and S have walls at z=1, so those directions won't work
        expect(cell == CELL_RAMP_E || cell == CELL_RAMP_W);
        
        // Mover should be able to climb out
        m->goal = (Point){1, 1, 1};
        m->needsRepath = true;
        
        bool returnedToZ1 = false;
        for (int i = 0; i < 1000; i++) {
            Tick();
            if ((int)m->z == 1) {
                returnedToZ1 = true;
                break;
            }
        }
        
        expect(returnedToZ1 == true);
    }
}
```

### Test: Channeling into open air (no ramp created)

```c
describe(channel_into_open_air) {
    it("should not create ramp when z-1 is already open") {
        // z=0 is open air, z=1 has floor
        const char* map =
            "floor:0\n"
            ".....\n"  // open floor
            ".....\n"
            ".....\n"
            "floor:1\n"
            ".....\n"
            ".....\n"
            ".....\n";
        
        InitMultiFloorGridFromAscii(map, 5, 5);
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        InitDesignations();
        
        Mover* m = &movers[0];
        Point goal = {0, 0, 1};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 
                     0 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, goal, 100.0f);
        moverCount = 1;
        
        // z=0 at (2,1) is floor (not wall), so no ramp should be created
        DesignateChannel(2, 1, 1);
        
        // Run until channel completes
        for (int i = 0; i < 1000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            if ((int)m->z == 0) break;
        }
        
        expect((int)m->z == 0);
        
        // No ramp created (was already open)
        CellType cell = grid[0][1][2];
        expect(!CellIsDirectionalRamp(cell));
        
        // Mover is stuck at z=0 (this is expected - DF behavior)
        // "Channels dug above a dug-out area will not create ramps"
    }
}
```

### Test: Multiple channel designations (pit digging)

```c
describe(channel_multiple_designations) {
    it("should handle multiple channel designations creating expanding pit") {
        const char* map =
            "floor:0\n"
            "#####\n"
            "#####\n"
            "#####\n"
            "floor:1\n"
            ".....\n"
            ".....\n"
            ".....\n";
        
        InitMultiFloorGridFromAscii(map, 5, 5);
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        InitDesignations();
        
        // Two movers
        Mover* m1 = &movers[0];
        Mover* m2 = &movers[1];
        Point goal1 = {0, 0, 1};
        Point goal2 = {4, 0, 1};
        InitMover(m1, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 
                      0 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, goal1, 100.0f);
        InitMover(m2, 4 * CELL_SIZE + CELL_SIZE * 0.5f, 
                      0 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, goal2, 100.0f);
        moverCount = 2;
        
        // Designate two adjacent tiles
        DesignateChannel(1, 1, 1);
        DesignateChannel(2, 1, 1);
        
        expect(CountChannelDesignations() == 2);
        
        // Run until both complete
        int completedCount = 0;
        for (int i = 0; i < 2000; i++) {
            Tick();
            AssignJobs();
            JobsTick();
            
            completedCount = 0;
            if (!HAS_FLOOR(1, 1, 1)) completedCount++;
            if (!HAS_FLOOR(2, 1, 1)) completedCount++;
            
            if (completedCount == 2) break;
        }
        
        expect(completedCount == 2);
        expect(CountChannelDesignations() == 0);
    }
}
```

### Test: Job cancellation

```c
describe(channel_job_cancellation) {
    it("should release designation when channel job is cancelled") {
        const char* map =
            "floor:0\n"
            "#####\n"
            "#####\n"
            "#####\n"
            "floor:1\n"
            ".....\n"
            ".....\n"
            ".....\n";
        
        InitMultiFloorGridFromAscii(map, 5, 5);
        moverPathAlgorithm = PATH_ALGO_ASTAR;
        
        ClearMovers();
        ClearItems();
        InitDesignations();
        
        Mover* m = &movers[0];
        Point goal = {0, 0, 1};
        InitMover(m, 0 * CELL_SIZE + CELL_SIZE * 0.5f, 
                     0 * CELL_SIZE + CELL_SIZE * 0.5f, 1.0f, goal, 100.0f);
        moverCount = 1;
        
        DesignateChannel(2, 1, 1);
        
        // Let job get assigned
        AssignJobs();
        expect(MoverHasChannelJob(m));
        
        Designation* d = GetDesignation(2, 1, 1);
        expect(d->assignedMover == 0);
        
        // Cancel the designation
        CancelDesignation(2, 1, 1);
        
        // Job should be cancelled, mover idle
        expect(MoverIsIdle(m));
        expect(HasChannelDesignation(2, 1, 1) == false);
    }
}
```

---

## Estimated Changes

| File | Changes |
|------|---------|
| `designations.h` | +15 lines (enum, functions, constant) |
| `designations.c` | +120 lines (DesignateChannel, CompleteChannel, AutoDetectChannelRampDirection) |
| `jobs.h` | +5 lines (JOBTYPE_CHANNEL, maybe reuse target fields) |
| `jobs.c` | +100 lines (RunJob_Channel, WorkGiver_Channel, driver entry) |
| `input.c` | +20 lines (ExecuteDesignateChannel, key handling) |
| `input_mode.c` | +5 lines (menu item) |
| `rendering.c` | +20 lines (channel designation rendering) |
| `test_jobs.c` | +180 lines (channel tests including ramp direction tests) |

**Total: ~465 lines**

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
