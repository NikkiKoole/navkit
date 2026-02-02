#include "designations.h"
#include "grid.h"
#include "cell_defs.h"
#include "pathfinding.h"
#include "../entities/items.h"
#include "../entities/mover.h"  // for CELL_SIZE
#include "../simulation/water.h"
#include <string.h>
#include <math.h>

Designation designations[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Active designation count for early-exit optimizations
int activeDesignationCount = 0;

// Blueprint storage
Blueprint blueprints[MAX_BLUEPRINTS];
int blueprintCount = 0;

void InitDesignations(void) {
    memset(designations, 0, sizeof(designations));
    // Set all assignedMover to -1
    for (int z = 0; z < MAX_GRID_DEPTH; z++) {
        for (int y = 0; y < MAX_GRID_HEIGHT; y++) {
            for (int x = 0; x < MAX_GRID_WIDTH; x++) {
                designations[z][y][x].assignedMover = -1;
            }
        }
    }
    activeDesignationCount = 0;
    
    // Clear blueprints
    memset(blueprints, 0, sizeof(blueprints));
    for (int i = 0; i < MAX_BLUEPRINTS; i++) {
        blueprints[i].assignedBuilder = -1;
        blueprints[i].reservedItem = -1;
    }
    blueprintCount = 0;
}

bool DesignateMine(int x, int y, int z) {
    // Bounds check
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    
    // Can only mine walls
    if (grid[z][y][x] != CELL_WALL) {
        return false;
    }
    
    // Already designated?
    if (designations[z][y][x].type == DESIGNATION_MINE) {
        return false;
    }
    
    designations[z][y][x].type = DESIGNATION_MINE;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    activeDesignationCount++;
    
    return true;
}

void CancelDesignation(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return;
    }
    
    if (designations[z][y][x].type != DESIGNATION_NONE) {
        activeDesignationCount--;
    }
    designations[z][y][x].type = DESIGNATION_NONE;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
}

bool HasMineDesignation(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    return designations[z][y][x].type == DESIGNATION_MINE;
}

Designation* GetDesignation(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return NULL;
    }
    if (designations[z][y][x].type == DESIGNATION_NONE) {
        return NULL;
    }
    return &designations[z][y][x];
}

bool FindUnassignedMineDesignation(int* outX, int* outY, int* outZ) {
    // Simple linear scan - could be optimized with a list later
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                Designation* d = &designations[z][y][x];
                if (d->type == DESIGNATION_MINE && d->assignedMover == -1) {
                    *outX = x;
                    *outY = y;
                    *outZ = z;
                    return true;
                }
            }
        }
    }
    return false;
}

void CompleteMineDesignation(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return;
    }
    
    // Convert wall to air with floor
    if (grid[z][y][x] == CELL_WALL) {
        grid[z][y][x] = CELL_AIR;
        SET_FLOOR(x, y, z);
        MarkChunkDirty(x, y, z);
        
        // Destabilize water at this cell and neighbors so it can flow into the new space
        DestabilizeWater(x, y, z);
        
        // Spawn an orange block (stone) from the mined wall
        SpawnItem(x * CELL_SIZE + CELL_SIZE * 0.5f, y * CELL_SIZE + CELL_SIZE * 0.5f, (float)z, ITEM_ORANGE);
    }
    
    // Clear designation
    if (designations[z][y][x].type != DESIGNATION_NONE) {
        activeDesignationCount--;
    }
    designations[z][y][x].type = DESIGNATION_NONE;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    
    // Validate nearby ramps - mining may have removed solid support
    ValidateAndCleanupRamps(x - 2, y - 2, z - 1, x + 2, y + 2, z + 1);
}

int CountMineDesignations(void) {
    int count = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (designations[z][y][x].type == DESIGNATION_MINE) {
                    count++;
                }
            }
        }
    }
    return count;
}

// Tick down unreachable cooldowns for designations
void DesignationsTick(float dt) {
    // Early exit if no designations
    if (activeDesignationCount == 0) return;
    
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                Designation* d = &designations[z][y][x];
                if (d->type != DESIGNATION_NONE && d->unreachableCooldown > 0.0f) {
                    d->unreachableCooldown = fmaxf(0.0f, d->unreachableCooldown - dt);
                }
            }
        }
    }
}

// =============================================================================
// Channel designation functions
// =============================================================================

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
    
    // Mover needs to stand on the tile to channel it, so it must be walkable
    if (!IsCellWalkableAt(z, y, x)) {
        return false;
    }
    
    // Verify there's a floor to remove
    if (!HAS_FLOOR(x, y, z)) {
        // Check if standing on solid cell below (implicit floor)
        if (z > 0 && !CellIsSolid(grid[z-1][y][x])) {
            // No solid below - channeling here creates a hole with no ramp (DF behavior)
            // This is dangerous: mover will fall! But DF allows it, so we do too.
            // return false;  // No floor to channel
        }
    }
    
    designations[z][y][x].type = DESIGNATION_CHANNEL;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    activeDesignationCount++;
    
    return true;
}

bool HasChannelDesignation(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    return designations[z][y][x].type == DESIGNATION_CHANNEL;
}

// Direction offsets for cardinal neighbors (N, E, S, W)
static const int CHANNEL_DIR_DX[4] = {0, 1, 0, -1};
static const int CHANNEL_DIR_DY[4] = {-1, 0, 1, 0};
static const CellType CHANNEL_RAMP_TYPES[4] = {CELL_RAMP_N, CELL_RAMP_E, CELL_RAMP_S, CELL_RAMP_W};

CellType AutoDetectChannelRampDirection(int x, int y, int lowerZ) {
    int upperZ = lowerZ + 1;
    
    // Bounds check
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight) {
        return CELL_AIR;
    }
    if (lowerZ < 0 || upperZ >= gridDepth) {
        return CELL_AIR;
    }
    
    // Try each direction - prioritize adjacent walls (classic DF behavior),
    // then fall back to any walkable exit (allows ramp-to-ramp connections)
    // RAMP_N means "exit to the north at z+1"
    
    // First pass: prefer directions with adjacent solid (wall) - classic behavior
    for (int i = 0; i < 4; i++) {
        int dx = CHANNEL_DIR_DX[i];
        int dy = CHANNEL_DIR_DY[i];
        CellType rampType = CHANNEL_RAMP_TYPES[i];
        
        int adjX = x + dx;
        int adjY = y + dy;
        
        // Bounds check for adjacent cell
        if (adjX < 0 || adjX >= gridWidth || adjY < 0 || adjY >= gridHeight) continue;
        
        // Adjacent cell at lowerZ should be solid (wall) - this is the "high side base"
        CellType adjBelow = grid[lowerZ][adjY][adjX];
        bool adjIsSolid = CellIsSolid(adjBelow);
        if (!adjIsSolid) continue;
        
        // Adjacent cell at upperZ (above that wall) should be walkable - this is the exit
        if (!IsCellWalkableAt(upperZ, adjY, adjX)) continue;
        
        // This direction works!
        return rampType;
    }
    
    // Second pass: allow any direction with walkable exit at z+1
    // This enables ramp creation in interior cells where adjacent cells are also ramps
    for (int i = 0; i < 4; i++) {
        int dx = CHANNEL_DIR_DX[i];
        int dy = CHANNEL_DIR_DY[i];
        CellType rampType = CHANNEL_RAMP_TYPES[i];
        
        int adjX = x + dx;
        int adjY = y + dy;
        
        // Bounds check for adjacent cell
        if (adjX < 0 || adjX >= gridWidth || adjY < 0 || adjY >= gridHeight) continue;
        
        // Just check if exit at upperZ is walkable (standing on ramp = walkable)
        if (!IsCellWalkableAt(upperZ, adjY, adjX)) continue;
        
        // This direction works!
        return rampType;
    }
    
    return CELL_AIR;  // No valid direction found
}

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
            // Push to adjacent walkable cell
            for (int dir = 0; dir < 4; dir++) {
                int ax = x + CHANNEL_DIR_DX[dir];
                int ay = y + CHANNEL_DIR_DY[dir];
                if (ax >= 0 && ax < gridWidth && ay >= 0 && ay < gridHeight) {
                    if (IsCellWalkableAt(z, ay, ax)) {
                        m->x = ax * CELL_SIZE + CELL_SIZE * 0.5f;
                        m->y = ay * CELL_SIZE + CELL_SIZE * 0.5f;
                        m->pathLength = 0;
                        m->pathIndex = -1;
                        m->needsRepath = true;
                        break;
                    }
                }
            }
        }
    }
    
    // === STEP 1: Remove the floor at z ===
    CLEAR_FLOOR(x, y, z);
    grid[z][y][x] = CELL_AIR;
    
    // === STEP 2: Mine out z-1 and determine what to create ===
    CellType cellBelow = grid[lowerZ][y][x];
    bool wasSolid = CellIsSolid(cellBelow);
    
    if (wasSolid) {
        // z-1 was solid - mine it out and create a ramp
        // Use channeling-specific ramp detection (relaxed rules - no low-side check)
        CellType rampDir = AutoDetectChannelRampDirection(x, y, lowerZ);
        
        if (rampDir != CELL_AIR) {
            // Create ramp facing the adjacent wall
            // Mover can climb UP this ramp to exit at z (the hole level)
            grid[lowerZ][y][x] = rampDir;
            SET_FLOOR(x, y, lowerZ);  // Ramps need floor flag
            rampCount++;
        } else {
            // No valid ramp direction (no adjacent wall with walkable floor above)
            // This happens if channeling in the middle of a large open area
            // Create floor instead - mover is stuck but at least standing
            grid[lowerZ][y][x] = CELL_AIR;
            SET_FLOOR(x, y, lowerZ);
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
              (float)lowerZ, ITEM_ORANGE);
    
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
    
    // === STEP 5: Validate nearby ramps ===
    // Channeling may have removed the solid support for adjacent ramps
    // Check a small region around the channeled cell
    ValidateAndCleanupRamps(x - 2, y - 2, lowerZ, x + 2, y + 2, z);
}

int CountChannelDesignations(void) {
    int count = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (designations[z][y][x].type == DESIGNATION_CHANNEL) {
                    count++;
                }
            }
        }
    }
    return count;
}

// =============================================================================
// Remove floor designation functions
// =============================================================================

bool DesignateRemoveFloor(int x, int y, int z) {
    // Bounds check
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    
    // Already designated?
    if (designations[z][y][x].type != DESIGNATION_NONE) {
        return false;
    }
    
    // Must be walkable (so mover can stand on it to remove it)
    if (!IsCellWalkableAt(z, y, x)) {
        return false;
    }
    
    // Must have an explicit floor flag (constructed floor) - can't remove "implicit" floors
    // You walk on solid blocks below, that's not a removable floor
    if (!HAS_FLOOR(x, y, z)) {
        return false;  // No constructed floor to remove
    }
    
    designations[z][y][x].type = DESIGNATION_REMOVE_FLOOR;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    activeDesignationCount++;
    
    return true;
}

bool HasRemoveFloorDesignation(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    return designations[z][y][x].type == DESIGNATION_REMOVE_FLOOR;
}

void CompleteRemoveFloorDesignation(int x, int y, int z, int moverIdx) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return;
    }
    
    // Remove the floor
    CLEAR_FLOOR(x, y, z);
    // Cell type stays as-is (AIR) - we're just removing the floor flag
    
    MarkChunkDirty(x, y, z);
    
    // Spawn an item from the removed floor (orange block for now)
    SpawnItem(x * CELL_SIZE + CELL_SIZE * 0.5f, y * CELL_SIZE + CELL_SIZE * 0.5f, (float)z, ITEM_ORANGE);
    
    // Clear designation
    if (designations[z][y][x].type != DESIGNATION_NONE) {
        activeDesignationCount--;
    }
    designations[z][y][x].type = DESIGNATION_NONE;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    
    // Note: mover will fall if there's nothing solid below - handled by mover update tick
    (void)moverIdx;  // Could be used for special handling later
}

int CountRemoveFloorDesignations(void) {
    int count = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (designations[z][y][x].type == DESIGNATION_REMOVE_FLOOR) {
                    count++;
                }
            }
        }
    }
    return count;
}

// =============================================================================
// Remove ramp designation functions
// =============================================================================

bool DesignateRemoveRamp(int x, int y, int z) {
    // Bounds check
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    
    // Already designated?
    if (designations[z][y][x].type != DESIGNATION_NONE) {
        return false;
    }
    
    // Must be a ramp cell
    CellType cell = grid[z][y][x];
    if (!CellIsRamp(cell)) {
        return false;
    }
    
    designations[z][y][x].type = DESIGNATION_REMOVE_RAMP;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    activeDesignationCount++;
    
    return true;
}

bool HasRemoveRampDesignation(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    return designations[z][y][x].type == DESIGNATION_REMOVE_RAMP;
}

void CompleteRemoveRampDesignation(int x, int y, int z, int moverIdx) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return;
    }
    
    // Remove the ramp, replace with floor
    CellType cell = grid[z][y][x];
    if (CellIsRamp(cell)) {
        grid[z][y][x] = CELL_AIR;
        SET_FLOOR(x, y, z);
        rampCount--;
    }
    
    MarkChunkDirty(x, y, z);
    
    // Spawn an item from the removed ramp (orange block for now)
    SpawnItem(x * CELL_SIZE + CELL_SIZE * 0.5f, y * CELL_SIZE + CELL_SIZE * 0.5f, (float)z, ITEM_ORANGE);
    
    // Clear designation
    if (designations[z][y][x].type != DESIGNATION_NONE) {
        activeDesignationCount--;
    }
    designations[z][y][x].type = DESIGNATION_NONE;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    
    (void)moverIdx;  // Could be used for special handling later
}

int CountRemoveRampDesignations(void) {
    int count = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (designations[z][y][x].type == DESIGNATION_REMOVE_RAMP) {
                    count++;
                }
            }
        }
    }
    return count;
}

// =============================================================================
// Blueprint functions
// =============================================================================

int CreateBuildBlueprint(int x, int y, int z) {
    // Bounds check
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return -1;
    }
    
    // Can only build on walkable floor
    if (!IsCellWalkableAt(z, y, x)) {
        return -1;
    }
    
    // Already has a blueprint?
    if (HasBlueprint(x, y, z)) {
        return -1;
    }
    
    // Find free slot
    int idx = -1;
    for (int i = 0; i < MAX_BLUEPRINTS; i++) {
        if (!blueprints[i].active) {
            idx = i;
            break;
        }
    }
    
    if (idx < 0) return -1;  // No free slots
    
    Blueprint* bp = &blueprints[idx];
    bp->x = x;
    bp->y = y;
    bp->z = z;
    bp->active = true;
    bp->state = BLUEPRINT_AWAITING_MATERIALS;
    bp->requiredMaterials = 1;  // 1 item to build a wall
    bp->deliveredMaterials = 0;
    bp->reservedItem = -1;
    bp->assignedBuilder = -1;
    bp->progress = 0.0f;
    
    blueprintCount++;
    return idx;
}

void CancelBlueprint(int blueprintIdx) {
    if (blueprintIdx < 0 || blueprintIdx >= MAX_BLUEPRINTS) return;
    
    Blueprint* bp = &blueprints[blueprintIdx];
    if (!bp->active) return;
    
    // Release reserved item if any
    if (bp->reservedItem >= 0 && bp->reservedItem < MAX_ITEMS) {
        items[bp->reservedItem].reservedBy = -1;
    }
    
    bp->active = false;
    bp->reservedItem = -1;
    bp->assignedBuilder = -1;
    blueprintCount--;
}

int GetBlueprintAt(int x, int y, int z) {
    for (int i = 0; i < MAX_BLUEPRINTS; i++) {
        Blueprint* bp = &blueprints[i];
        if (bp->active && bp->x == x && bp->y == y && bp->z == z) {
            return i;
        }
    }
    return -1;
}

bool HasBlueprint(int x, int y, int z) {
    return GetBlueprintAt(x, y, z) >= 0;
}

int FindBlueprintNeedingMaterials(void) {
    for (int i = 0; i < MAX_BLUEPRINTS; i++) {
        Blueprint* bp = &blueprints[i];
        if (bp->active && 
            bp->state == BLUEPRINT_AWAITING_MATERIALS && 
            bp->reservedItem < 0) {  // No item reserved yet
            return i;
        }
    }
    return -1;
}

int FindBlueprintReadyToBuild(void) {
    for (int i = 0; i < MAX_BLUEPRINTS; i++) {
        Blueprint* bp = &blueprints[i];
        if (bp->active && 
            bp->state == BLUEPRINT_READY_TO_BUILD && 
            bp->assignedBuilder < 0) {  // No builder assigned yet
            return i;
        }
    }
    return -1;
}

void DeliverMaterialToBlueprint(int blueprintIdx, int itemIdx) {
    if (blueprintIdx < 0 || blueprintIdx >= MAX_BLUEPRINTS) return;
    if (itemIdx < 0 || itemIdx >= MAX_ITEMS) return;
    
    Blueprint* bp = &blueprints[blueprintIdx];
    if (!bp->active) return;
    
    // Consume the item
    DeleteItem(itemIdx);
    
    // Update blueprint
    bp->deliveredMaterials++;
    bp->reservedItem = -1;
    
    // Check if we have all materials
    if (bp->deliveredMaterials >= bp->requiredMaterials) {
        bp->state = BLUEPRINT_READY_TO_BUILD;
    }
}

void CompleteBlueprint(int blueprintIdx) {
    if (blueprintIdx < 0 || blueprintIdx >= MAX_BLUEPRINTS) return;
    
    Blueprint* bp = &blueprints[blueprintIdx];
    if (!bp->active) return;
    
    int x = bp->x, y = bp->y, z = bp->z;
    
    // Push any movers out of this cell before placing the wall
    PushMoversOutOfCell(x, y, z);
    
    // Push any items out of this cell before placing the wall
    PushItemsOutOfCell(x, y, z);
    
    // Convert floor to wall
    if (IsCellWalkableAt(z, y, x)) {
        DisplaceWater(x, y, z);
        grid[z][y][x] = CELL_WALL;
        MarkChunkDirty(x, y, z);
    }
    
    // Remove blueprint
    bp->active = false;
    bp->assignedBuilder = -1;
    bp->reservedItem = -1;
    blueprintCount--;
}

int CountBlueprints(void) {
    return blueprintCount;
}

int CountBlueprintsAwaitingMaterials(void) {
    int count = 0;
    for (int i = 0; i < MAX_BLUEPRINTS; i++) {
        if (blueprints[i].active && blueprints[i].state == BLUEPRINT_AWAITING_MATERIALS) {
            count++;
        }
    }
    return count;
}

int CountBlueprintsReadyToBuild(void) {
    int count = 0;
    for (int i = 0; i < MAX_BLUEPRINTS; i++) {
        if (blueprints[i].active && blueprints[i].state == BLUEPRINT_READY_TO_BUILD) {
            count++;
        }
    }
    return count;
}
