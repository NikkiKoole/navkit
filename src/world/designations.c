#include "designations.h"
#include "grid.h"
#include "cell_defs.h"
#include "material.h"
#include "pathfinding.h"
#include "../entities/items.h"
#include "../entities/mover.h"  // for CELL_SIZE
#include "../simulation/water.h"
#include "../simulation/trees.h"
#include "../core/sim_manager.h"
#include <string.h>
#include <math.h>

Designation designations[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Active designation count for early-exit optimizations
int activeDesignationCount = 0;

// Blueprint storage
Blueprint blueprints[MAX_BLUEPRINTS];
int blueprintCount = 0;

static MaterialType NormalizeTreeTypeLocal(MaterialType mat) {
    // Validate tree material type
    if (!IsWoodMaterial(mat)) return MAT_OAK;
    return mat;
}

static unsigned int PositionHashLocal(int x, int y, int z) {
    unsigned int h = (unsigned int)(x * 374761393u + y * 668265263u + z * 2147483647u);
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

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
        blueprints[i].deliveredMaterial = MAT_NONE;
    }
    blueprintCount = 0;
}

bool DesignateMine(int x, int y, int z) {
    // Bounds check
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    
    // Can mine walls and terrain (DF-style: any solid)
    CellType ct = grid[z][y][x];
    if (!CellIsSolid(ct)) {
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
    InvalidateDesignationCache(DESIGNATION_MINE);
    
    return true;
}

void CancelDesignation(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return;
    }
    
    DesignationType oldType = designations[z][y][x].type;
    if (oldType != DESIGNATION_NONE) {
        activeDesignationCount--;
        InvalidateDesignationCache(oldType);  // Mark cache dirty
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
    
    CellType ct = grid[z][y][x];
    
    // Convert solid cell to air with floor
    if (CellIsSolid(ct)) {
        ItemType dropItem = GetWallDropItem(x, y, z);
        int dropCount = CellDropCount(ct);
        MaterialType wallMat = GetWallMaterial(x, y, z);
        bool wasNatural = IsWallNatural(x, y, z);
        
        grid[z][y][x] = CELL_AIR;
        SET_FLOOR(x, y, z);
        
        // Mining creates floor from wall material
        if (wallMat != MAT_NONE) {
            SetFloorMaterial(x, y, z, wallMat);
        }
        if (wasNatural) {
            SetFloorNatural(x, y, z);
        } else {
            ClearFloorNatural(x, y, z);
        }
        SetFloorFinish(x, y, z, DefaultFinishForNatural(wasNatural));
        SetWallMaterial(x, y, z, MAT_NONE);
        ClearWallNatural(x, y, z);
        SetWallFinish(x, y, z, FINISH_ROUGH);
        MarkChunkDirty(x, y, z);
        
        DestabilizeWater(x, y, z);
        ClearUnreachableCooldownsNearCell(x, y, z, 5);
        
        // Spawn drops based on material
        if (dropItem != ITEM_NONE && dropCount > 0) {
            for (int i = 0; i < dropCount; i++) {
                uint8_t dropMat = (wallMat != MAT_NONE) ? (uint8_t)wallMat : DefaultMaterialForItemType(dropItem);
                SpawnItemWithMaterial(x * CELL_SIZE + CELL_SIZE * 0.5f,
                                      y * CELL_SIZE + CELL_SIZE * 0.5f,
                                      (float)z, dropItem, dropMat);
            }
        }
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
    
    // Invalidate mine cache so newly-adjacent designations become reachable
    InvalidateDesignationCache(DESIGNATION_MINE);
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
    InvalidateDesignationCache(DESIGNATION_CHANNEL);
    
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
    
    // Drop items in this cell down to z-1 (floor is being removed)
    DropItemsInCell(x, y, z);
    
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
    
    // === STEP 1: Get drop items before modifying cells ===
    // Floor at z - get its material before removing
    ItemType floorDropItem = GetFloorDropItem(x, y, z);
    MaterialType floorMat = GetFloorMaterial(x, y, z);
    // Wall material at z-1 (if solid) - get before mining
    ItemType minedDropItem = GetWallDropItem(x, y, lowerZ);
    CellType cellBelow = grid[lowerZ][y][x];
    bool wasSolid = CellIsSolid(cellBelow);
    int minedDropCount = wasSolid ? CellDropCount(cellBelow) : 0;
    MaterialType minedWallMat = GetWallMaterial(x, y, lowerZ);
    bool minedWallNatural = IsWallNatural(x, y, lowerZ);
    
    // === STEP 2: Remove the floor at z ===
    CLEAR_FLOOR(x, y, z);
    grid[z][y][x] = CELL_AIR;
    SetFloorMaterial(x, y, z, MAT_NONE);  // Floor removed
    ClearFloorNatural(x, y, z);
    SetFloorFinish(x, y, z, FINISH_ROUGH);
    
    // === STEP 3: Mine out z-1 and determine what to create ===
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
        // Mining creates floor from wall material
        if (minedWallMat != MAT_NONE) {
            SetFloorMaterial(x, y, lowerZ, minedWallMat);
        }
        if (minedWallNatural) {
            SetFloorNatural(x, y, lowerZ);
        } else {
            ClearFloorNatural(x, y, lowerZ);
        }
        SetFloorFinish(x, y, lowerZ, DefaultFinishForNatural(minedWallNatural));
        SetWallMaterial(x, y, lowerZ, MAT_NONE);  // Wall removed
        ClearWallNatural(x, y, lowerZ);
        SetWallFinish(x, y, lowerZ, FINISH_ROUGH);
        
        // Spawn drops from the mined material
        if (minedDropItem != ITEM_NONE && minedDropCount > 0) {
            for (int i = 0; i < minedDropCount; i++) {
                uint8_t dropMat = (minedWallMat != MAT_NONE) ? (uint8_t)minedWallMat : DefaultMaterialForItemType(minedDropItem);
                SpawnItemWithMaterial(x * CELL_SIZE + CELL_SIZE * 0.5f,
                                      y * CELL_SIZE + CELL_SIZE * 0.5f,
                                      (float)lowerZ, minedDropItem, dropMat);
            }
        }
    }
    // else: z-1 was already open - no ramp created (DF behavior)
    // "Channels dug above a dug-out area will not create ramps"
    
    // Spawn debris from the floor we removed at z (drops to lowerZ)
    if (floorDropItem != ITEM_NONE) {
        uint8_t dropMat = (floorMat != MAT_NONE) ? (uint8_t)floorMat : DefaultMaterialForItemType(floorDropItem);
        SpawnItemWithMaterial(x * CELL_SIZE + CELL_SIZE * 0.5f,
                              y * CELL_SIZE + CELL_SIZE * 0.5f,
                              (float)lowerZ, floorDropItem, dropMat);
    }
    
    MarkChunkDirty(x, y, z);
    MarkChunkDirty(x, y, lowerZ);
    
    // Destabilize water so it can flow into the hole
    DestabilizeWater(x, y, z);
    DestabilizeWater(x, y, lowerZ);
    
    ClearUnreachableCooldownsNearCell(x, y, z, 5);
    ClearUnreachableCooldownsNearCell(x, y, lowerZ, 5);
    
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
    
    InvalidateDesignationCache(DESIGNATION_CHANNEL);
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
// Dig ramp designation functions
// =============================================================================

// Direction offsets for cardinal neighbors (N, E, S, W)
static const int DIG_RAMP_DIR_DX[4] = {0, 1, 0, -1};
static const int DIG_RAMP_DIR_DY[4] = {-1, 0, 1, 0};
// Ramp faces toward the exit (low side) - opposite of high side
static const CellType DIG_RAMP_TYPES[4] = {CELL_RAMP_S, CELL_RAMP_W, CELL_RAMP_N, CELL_RAMP_E};

// Auto-detect which direction the dug ramp should face
// Returns ramp type if valid, CELL_AIR if no valid direction
static CellType AutoDetectDigRampDirection(int x, int y, int z) {
    // Look for adjacent walkable floor at same level - that's where the ramp exits
    for (int i = 0; i < 4; i++) {
        int adjX = x + DIG_RAMP_DIR_DX[i];
        int adjY = y + DIG_RAMP_DIR_DY[i];
        
        if (adjX < 0 || adjX >= gridWidth || adjY < 0 || adjY >= gridHeight) continue;
        
        // Check if adjacent cell is walkable (floor or ground)
        if (IsCellWalkableAt(z, adjY, adjX)) {
            return DIG_RAMP_TYPES[i];
        }
    }
    return CELL_AIR;  // No valid direction
}

bool DesignateDigRamp(int x, int y, int z) {
    // Bounds check
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    
    // Can only dig ramps from solid cells
    CellType ct = grid[z][y][x];
    if (!CellIsSolid(ct)) {
        return false;
    }
    
    // Already designated?
    if (designations[z][y][x].type != DESIGNATION_NONE) {
        return false;
    }
    
    // Must have a valid direction for the ramp
    CellType rampDir = AutoDetectDigRampDirection(x, y, z);
    if (rampDir == CELL_AIR) {
        return false;  // No adjacent walkable floor
    }
    
    designations[z][y][x].type = DESIGNATION_DIG_RAMP;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    activeDesignationCount++;
    InvalidateDesignationCache(DESIGNATION_DIG_RAMP);
    
    return true;
}

bool HasDigRampDesignation(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    return designations[z][y][x].type == DESIGNATION_DIG_RAMP;
}

void CompleteDigRampDesignation(int x, int y, int z, int moverIdx) {
    (void)moverIdx;  // May be used later for skill effects
    
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return;
    }
    
    CellType ct = grid[z][y][x];
    
    // Get material to drop
    MaterialType mat = GetWallMaterial(x, y, z);
    bool isWallNatural = IsWallNatural(x, y, z);
    ItemType dropItem = isWallNatural ? CellDropsItem(ct) : MaterialDropsItem(mat);
    
    // Determine ramp direction
    CellType rampType = AutoDetectDigRampDirection(x, y, z);
    if (rampType == CELL_AIR) {
        rampType = CELL_RAMP_N;  // Fallback
    }
    
    // Convert to ramp
    grid[z][y][x] = rampType;
    SetWallMaterial(x, y, z, mat);  // Preserve material for the ramp
    if (isWallNatural) {
        SetWallNatural(x, y, z);
    } else {
        ClearWallNatural(x, y, z);
    }
    SetWallFinish(x, y, z, DefaultFinishForNatural(isWallNatural));
    
    // Create floor on top (z+1) if there's space
    if (z + 1 < gridDepth && grid[z+1][y][x] == CELL_AIR) {
        SET_FLOOR(x, y, z + 1);
        SetFloorMaterial(x, y, z + 1, mat);
        if (isWallNatural) {
            SetFloorNatural(x, y, z + 1);
        } else {
            ClearFloorNatural(x, y, z + 1);
        }
        SetFloorFinish(x, y, z + 1, DefaultFinishForNatural(isWallNatural));
    }
    
    rampCount++;
    
    MarkChunkDirty(x, y, z);
    if (z + 1 < gridDepth) MarkChunkDirty(x, y, z + 1);
    
    ClearUnreachableCooldownsNearCell(x, y, z, 5);
    
    // Spawn dropped item nearby
    if (dropItem != ITEM_NONE) {
        float itemX = x * CELL_SIZE + CELL_SIZE / 2.0f;
        float itemY = y * CELL_SIZE + CELL_SIZE / 2.0f;
        uint8_t dropMat = (mat != MAT_NONE) ? (uint8_t)mat : DefaultMaterialForItemType(dropItem);
        SpawnItemWithMaterial(itemX, itemY, z, dropItem, dropMat);
    }
    
    // Clear designation
    designations[z][y][x].type = DESIGNATION_NONE;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    activeDesignationCount--;
    InvalidateDesignationCache(DESIGNATION_DIG_RAMP);
    ValidateAndCleanupRamps(x - 2, y - 2, z - 1, x + 2, y + 2, z + 1);
}

int CountDigRampDesignations(void) {
    int count = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (designations[z][y][x].type == DESIGNATION_DIG_RAMP) {
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
    InvalidateDesignationCache(DESIGNATION_REMOVE_FLOOR);
    
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
    
    // Get drop item based on floor material (before changing)
    ItemType dropItem = GetFloorDropItem(x, y, z);
    MaterialType floorMat = GetFloorMaterial(x, y, z);
    
    // Drop items on this floor down to z-1
    DropItemsInCell(x, y, z);
    
    // Remove the floor
    CLEAR_FLOOR(x, y, z);
    SetFloorMaterial(x, y, z, MAT_NONE);  // Floor removed
    ClearFloorNatural(x, y, z);
    SetFloorFinish(x, y, z, FINISH_ROUGH);
    // Cell type stays as-is (AIR) - we're just removing the floor flag
    
    MarkChunkDirty(x, y, z);
    DestabilizeWater(x, y, z);
    
    // Spawn an item from the removed floor (drops to z-1 since floor is gone)
    if (dropItem != ITEM_NONE) {
        int dropZ = (z > 0) ? z - 1 : 0;
        uint8_t dropMat = (floorMat != MAT_NONE) ? (uint8_t)floorMat : DefaultMaterialForItemType(dropItem);
        SpawnItemWithMaterial(x * CELL_SIZE + CELL_SIZE * 0.5f,
                              y * CELL_SIZE + CELL_SIZE * 0.5f,
                              (float)dropZ, dropItem, dropMat);
    }
    
    // Clear designation
    if (designations[z][y][x].type != DESIGNATION_NONE) {
        activeDesignationCount--;
    }
    designations[z][y][x].type = DESIGNATION_NONE;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    
    // Note: mover will fall if there's nothing solid below - handled by mover update tick
    InvalidateDesignationCache(DESIGNATION_REMOVE_FLOOR);
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
    InvalidateDesignationCache(DESIGNATION_REMOVE_RAMP);
    
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
    
    // Get drop item based on wall material (ramps are walls)
    ItemType dropItem = GetWallDropItem(x, y, z);
    CellType cell = grid[z][y][x];
    int dropCount = CellIsRamp(cell) ? CellDropCount(cell) : 0;
    MaterialType rampMat = GetWallMaterial(x, y, z);
    bool rampNatural = IsWallNatural(x, y, z);
    
    // Remove the ramp, replace with floor
    if (CellIsRamp(cell)) {
        grid[z][y][x] = CELL_AIR;
        SET_FLOOR(x, y, z);
        // Ramp removal creates floor from ramp material
        if (rampMat != MAT_NONE) {
            SetFloorMaterial(x, y, z, rampMat);
        }
        if (rampNatural) {
            SetFloorNatural(x, y, z);
        } else {
            ClearFloorNatural(x, y, z);
        }
        SetFloorFinish(x, y, z, DefaultFinishForNatural(rampNatural));
        SetWallMaterial(x, y, z, MAT_NONE);  // Ramp/wall removed
        ClearWallNatural(x, y, z);
        SetWallFinish(x, y, z, FINISH_ROUGH);
        rampCount--;
    }
    
    MarkChunkDirty(x, y, z);
    DestabilizeWater(x, y, z);
    
    // Spawn drops from the removed ramp
    if (dropItem != ITEM_NONE && dropCount > 0) {
        for (int i = 0; i < dropCount; i++) {
            uint8_t dropMat = (rampMat != MAT_NONE) ? (uint8_t)rampMat : DefaultMaterialForItemType(dropItem);
            SpawnItemWithMaterial(x * CELL_SIZE + CELL_SIZE * 0.5f,
                                  y * CELL_SIZE + CELL_SIZE * 0.5f,
                                  (float)z, dropItem, dropMat);
        }
    }
    
    // Clear designation
    if (designations[z][y][x].type != DESIGNATION_NONE) {
        activeDesignationCount--;
    }
    designations[z][y][x].type = DESIGNATION_NONE;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    
    InvalidateDesignationCache(DESIGNATION_REMOVE_RAMP);
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
// Chop tree designation functions
// =============================================================================

bool DesignateChop(int x, int y, int z) {
    // Bounds check
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    
    // Already designated?
    if (designations[z][y][x].type != DESIGNATION_NONE) {
        return false;
    }
    
    // Must be a tree trunk cell (not felled)
    if (grid[z][y][x] != CELL_TREE_TRUNK) {
        return false;
    }
    
    designations[z][y][x].type = DESIGNATION_CHOP;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    activeDesignationCount++;
    InvalidateDesignationCache(DESIGNATION_CHOP);
    
    return true;
}

bool HasChopDesignation(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    return designations[z][y][x].type == DESIGNATION_CHOP;
}

bool DesignateChopFelled(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }

    if (designations[z][y][x].type != DESIGNATION_NONE) {
        return false;
    }

    if (grid[z][y][x] != CELL_TREE_FELLED) {
        return false;
    }

    designations[z][y][x].type = DESIGNATION_CHOP_FELLED;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    activeDesignationCount++;
    InvalidateDesignationCache(DESIGNATION_CHOP_FELLED);

    return true;
}

bool HasChopFelledDesignation(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    return designations[z][y][x].type == DESIGNATION_CHOP_FELLED;
}

// Helper: Check if a leaf cell is connected to a trunk of same type within distance
static bool LeafConnectedToTrunk(int x, int y, int z, int maxDist, MaterialType treeMat) {
    MaterialType mat = treeMat;
    for (int checkZ = z; checkZ >= 0 && checkZ >= z - maxDist; checkZ--) {
        if (grid[checkZ][y][x] == CELL_TREE_TRUNK &&
            GetWallMaterial(x, y, checkZ) == mat) {
            return true;
        }
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;
                int nx = x + dx;
                int ny = y + dy;
                if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
                if (grid[checkZ][ny][nx] == CELL_TREE_TRUNK &&
                    GetWallMaterial(nx, ny, checkZ) == mat) {
                    return true;
                }
            }
        }
    }
    return false;
}

static int FindSurfaceZAtLocal(int x, int y) {
    for (int z = gridDepth - 1; z >= 0; z--) {
        if (HAS_FLOOR(x, y, z)) return z;
        if (CellIsSolid(grid[z][y][x])) return z;
    }
    return -1;
}

static int FindItemSpawnZAtLocal(int x, int y, int fallbackZ) {
    int surfaceZ = FindSurfaceZAtLocal(x, y);
    if (surfaceZ < 0) return fallbackZ;
    if (CellIsSolid(grid[surfaceZ][y][x])) {
        if (surfaceZ + 1 < gridDepth) return surfaceZ + 1;
        return surfaceZ;
    }
    return surfaceZ;
}

static int FindTrunkBaseZAt(int x, int y, int z) {
    int baseZ = z;
    while (baseZ > 0 &&
           grid[baseZ - 1][y][x] == CELL_TREE_TRUNK) {
        baseZ--;
    }
    return baseZ;
}

static int GetTrunkHeightAt(int x, int y, int baseZ) {
    int height = 0;
    for (int tz = baseZ; tz < gridDepth; tz++) {
        if (grid[tz][y][x] == CELL_TREE_TRUNK) {
            height++;
        } else {
            break;
        }
    }
    return height;
}

// Fell a tree: remove connected trunk/branch/root, create fallen trunk line, drop leaves/saplings
// chopperX/Y is where the mover stood - tree falls away from them
static void FellTree(int x, int y, int z, float chopperX, float chopperY) {
    MaterialType treeMat = GetWallMaterial(x, y, z);
    MaterialType type = NormalizeTreeTypeLocal(treeMat);
    int leafCount = 0;

    int baseZ = FindTrunkBaseZAt(x, y, z);
    int trunkHeight = GetTrunkHeightAt(x, y, baseZ);
    if (trunkHeight <= 0) trunkHeight = 1;

    // Remove all connected trunk cells (trunk/branch/root) for this tree type
    int minX = x, maxX = x, minY = y, maxY = y, minZ = z, maxZ = z;

    enum { TREE_STACK_MAX = 4096 };
    int stackX[TREE_STACK_MAX];
    int stackY[TREE_STACK_MAX];
    int stackZ[TREE_STACK_MAX];
    int stackCount = 0;

    stackX[stackCount] = x;
    stackY[stackCount] = y;
    stackZ[stackCount] = z;
    stackCount++;

    while (stackCount > 0) {
        stackCount--;
        int cx = stackX[stackCount];
        int cy = stackY[stackCount];
        int cz = stackZ[stackCount];

        if (cx < 0 || cx >= gridWidth || cy < 0 || cy >= gridHeight || cz < 0 || cz >= gridDepth) continue;
        CellType cellType = grid[cz][cy][cx];
        if (cellType != CELL_TREE_TRUNK && cellType != CELL_TREE_BRANCH && cellType != CELL_TREE_ROOT) continue;
        if (GetWallMaterial(cx, cy, cz) != treeMat) continue;

        grid[cz][cy][cx] = CELL_AIR;
        SetWallMaterial(cx, cy, cz, MAT_NONE);
        MarkChunkDirty(cx, cy, cz);

        if (designations[cz][cy][cx].type != DESIGNATION_NONE) {
            activeDesignationCount--;
            designations[cz][cy][cx].type = DESIGNATION_NONE;
            designations[cz][cy][cx].assignedMover = -1;
            designations[cz][cy][cx].progress = 0.0f;
        }

        if (cx < minX) minX = cx;
        if (cx > maxX) maxX = cx;
        if (cy < minY) minY = cy;
        if (cy > maxY) maxY = cy;
        if (cz < minZ) minZ = cz;
        if (cz > maxZ) maxZ = cz;

        const int dxs[6] = {1, -1, 0, 0, 0, 0};
        const int dys[6] = {0, 0, 1, -1, 0, 0};
        const int dzs[6] = {0, 0, 0, 0, 1, -1};
        for (int i = 0; i < 6; i++) {
            int nx = cx + dxs[i];
            int ny = cy + dys[i];
            int nz = cz + dzs[i];
            if (stackCount >= TREE_STACK_MAX) break;
            stackX[stackCount] = nx;
            stackY[stackCount] = ny;
            stackZ[stackCount] = nz;
            stackCount++;
        }
    }

    // Remove leaves of this tree type in a padded bounding box
    int pad = 4;
    int minLX = minX - pad; if (minLX < 0) minLX = 0;
    int maxLX = maxX + pad; if (maxLX >= gridWidth) maxLX = gridWidth - 1;
    int minLY = minY - pad; if (minLY < 0) minLY = 0;
    int maxLY = maxY + pad; if (maxLY >= gridHeight) maxLY = gridHeight - 1;
    int minLZ = minZ - 1; if (minLZ < 0) minLZ = 0;
    int maxLZ = maxZ + 3; if (maxLZ >= gridDepth) maxLZ = gridDepth - 1;

    for (int sz = minLZ; sz <= maxLZ; sz++) {
        for (int sy = minLY; sy <= maxLY; sy++) {
            for (int sx = minLX; sx <= maxLX; sx++) {
                if (grid[sz][sy][sx] == CELL_TREE_LEAVES &&
                    GetWallMaterial(sx, sy, sz) == treeMat) {
                    if (!LeafConnectedToTrunk(sx, sy, sz, 4, type)) {
                        grid[sz][sy][sx] = CELL_AIR;
                        SetWallMaterial(sx, sy, sz, MAT_NONE);
                        MarkChunkDirty(sx, sy, sz);
                        leafCount++;
                    }
                }
            }
        }
    }

    // Validate ramps that may have lost solid support from removed trunks
    ValidateAndCleanupRamps(minX - 1, minY - 1, minZ - 1, maxX + 1, maxY + 1, maxZ + 1);

    // Calculate fall direction (away from chopper), quantized to 22.5° steps
    float treeCenterX = (float)x + 0.5f;
    float treeCenterY = (float)y + 0.5f;
    float fallDirX = treeCenterX - chopperX;
    float fallDirY = treeCenterY - chopperY;
    float fallLen = sqrtf(fallDirX * fallDirX + fallDirY * fallDirY);
    const float kPi = 3.14159265f;
    float angle = 0.0f;
    if (fallLen < 0.01f) {
        unsigned int h = PositionHashLocal(x, y, baseZ);
        angle = (float)(h % 16) * (kPi / 8.0f);
    } else {
        angle = atan2f(fallDirY, fallDirX);
        // If near-cardinal, nudge to a diagonal for variety
        float cardinal = kPi * 0.5f;
        float nearest = roundf(angle / cardinal) * cardinal;
        if (fabsf(angle - nearest) < (kPi / 32.0f)) {
            unsigned int h = PositionHashLocal(x, y, baseZ);
            float jitter = (h & 1u) ? (kPi / 8.0f) : -(kPi / 8.0f);
            angle = nearest + jitter;
        }
    }
    float angleStep = kPi / 8.0f; // 22.5 degrees
    angle = roundf(angle / angleStep) * angleStep;
    float dirX = cosf(angle);
    float dirY = sinf(angle);

    int endX = x + (int)roundf(dirX * (float)(trunkHeight - 1));
    int endY = y + (int)roundf(dirY * (float)(trunkHeight - 1));

    int lineX = x;
    int lineY = y;
    int dx = abs(endX - x);
    int sx = x < endX ? 1 : -1;
    int dy = -abs(endY - y);
    int sy = y < endY ? 1 : -1;
    int err = dx + dy;

    int placedSegments = 0;
    for (int i = 0; i < trunkHeight; i++) {
        int tx = lineX;
        int ty = lineY;
        if (tx < 0 || tx >= gridWidth || ty < 0 || ty >= gridHeight) break;

        int surfaceZ = FindSurfaceZAtLocal(tx, ty);
        if (surfaceZ < 0) break;
        int placeZ = surfaceZ + 1;
        if (placeZ < 0 || placeZ >= gridDepth) break;
        if (grid[placeZ][ty][tx] != CELL_AIR) break;

        grid[placeZ][ty][tx] = CELL_TREE_FELLED;
        SetWallMaterial(tx, ty, placeZ, treeMat);
        MarkChunkDirty(tx, ty, placeZ);
        placedSegments++;

        if (lineX == endX && lineY == endY) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; lineX += sx; }
        if (e2 <= dx) { err += dx; lineY += sy; }
    }

    if (placedSegments < trunkHeight) {
        TraceLog(LOG_WARNING, "FellTree: placed %d/%d fallen segments at (%d,%d,z%d)",
                 placedSegments, trunkHeight, x, y, baseZ);
    }

    int remainingTrunks = 0;
    for (int sz = minZ; sz <= maxZ; sz++) {
        for (int sy = minY; sy <= maxY; sy++) {
            for (int sx = minX; sx <= maxX; sx++) {
                CellType ct = grid[sz][sy][sx];
                if ((ct == CELL_TREE_TRUNK || ct == CELL_TREE_BRANCH || ct == CELL_TREE_ROOT) &&
                    GetWallMaterial(sx, sy, sz) == treeMat) {
                    remainingTrunks++;
                }
            }
        }
    }
    if (remainingTrunks > 0) {
        TraceLog(LOG_WARNING, "FellTree: %d trunk cells remain after removal at (%d,%d,z%d)",
                 remainingTrunks, x, y, baseZ);
    }

    // Spawn leaf items (~1 per 8 leaves, minimum 1 if any leaves)
    int leafItemCount = leafCount / 8;
    if (leafCount > 0 && leafItemCount == 0) leafItemCount = 1;

    // Spawn saplings (~1 per 5 leaves, minimum 1 if any leaves)
    int saplingCount = leafCount / 5;
    if (leafCount > 0 && saplingCount == 0) saplingCount = 1;

    float treeBaseX = x * CELL_SIZE + CELL_SIZE * 0.5f;
    float treeBaseY = y * CELL_SIZE + CELL_SIZE * 0.5f;
    float minXf = CELL_SIZE * 0.5f;
    float minYf = CELL_SIZE * 0.5f;
    float maxXf = (gridWidth - 1) * CELL_SIZE + CELL_SIZE * 0.5f;
    float maxYf = (gridHeight - 1) * CELL_SIZE + CELL_SIZE * 0.5f;

    ItemType leafItem = LeafItemFromTreeType(type);
    for (int i = 0; i < leafItemCount; i++) {
        float angle = (float)i * 2.4f;
        float dist = CELL_SIZE * (0.4f + (float)(i % 3) * 0.4f);
        float spawnX = treeBaseX + cosf(angle) * dist;
        float spawnY = treeBaseY + sinf(angle) * dist;
        if (spawnX < minXf) spawnX = minXf;
        if (spawnX > maxXf) spawnX = maxXf;
        if (spawnY < minYf) spawnY = minYf;
        if (spawnY > maxYf) spawnY = maxYf;
        int cellX = (int)(spawnX / CELL_SIZE);
        int cellY = (int)(spawnY / CELL_SIZE);
        if (cellX < 0) cellX = 0;
        if (cellX >= gridWidth) cellX = gridWidth - 1;
        if (cellY < 0) cellY = 0;
        if (cellY >= gridHeight) cellY = gridHeight - 1;
        int itemZ = FindItemSpawnZAtLocal(cellX, cellY, baseZ);
        SpawnItemWithMaterial(spawnX, spawnY, (float)itemZ, leafItem, (uint8_t)treeMat);
    }

    ItemType saplingItem = SaplingItemFromTreeType(type);
    for (int i = 0; i < saplingCount; i++) {
        float angle = (float)i * 2.4f;
        float dist = CELL_SIZE * (0.5f + (float)(i % 3) * 0.5f);
        float spawnX = treeBaseX + cosf(angle) * dist;
        float spawnY = treeBaseY + sinf(angle) * dist;
        if (spawnX < minXf) spawnX = minXf;
        if (spawnX > maxXf) spawnX = maxXf;
        if (spawnY < minYf) spawnY = minYf;
        if (spawnY > maxYf) spawnY = maxYf;
        int cellX = (int)(spawnX / CELL_SIZE);
        int cellY = (int)(spawnY / CELL_SIZE);
        if (cellX < 0) cellX = 0;
        if (cellX >= gridWidth) cellX = gridWidth - 1;
        if (cellY < 0) cellY = 0;
        if (cellY >= gridHeight) cellY = gridHeight - 1;
        int itemZ = FindItemSpawnZAtLocal(cellX, cellY, baseZ);
        SpawnItemWithMaterial(spawnX, spawnY, (float)itemZ, saplingItem, (uint8_t)treeMat);
    }
}

void CompleteChopDesignation(int x, int y, int z, int moverIdx) {
    // Get chopper position for fall direction (grid coords, allow sub-tile)
    float chopperX = (float)x + 0.5f;
    float chopperY = (float)y + 0.5f;
    
    if (moverIdx >= 0 && moverIdx < moverCount && movers[moverIdx].active) {
        chopperX = movers[moverIdx].x / CELL_SIZE;
        chopperY = movers[moverIdx].y / CELL_SIZE;
    }
    
    // Fell the entire tree (falls away from chopper)
    FellTree(x, y, z, chopperX, chopperY);
    
    // Designation already cleared in FellTree
}

void CompleteChopFelledDesignation(int x, int y, int z, int moverIdx) {
    (void)moverIdx;

    if (grid[z][y][x] != CELL_TREE_FELLED) {
        CancelDesignation(x, y, z);
        return;
    }

    MaterialType treeMat = GetWallMaterial(x, y, z);

    grid[z][y][x] = CELL_AIR;
    SetWallMaterial(x, y, z, MAT_NONE);
    MarkChunkDirty(x, y, z);

    // Validate ramps that may have lost solid support from removed trunk
    ValidateAndCleanupRamps(x - 1, y - 1, z - 1, x + 1, y + 1, z + 1);

    float spawnX = x * CELL_SIZE + CELL_SIZE * 0.5f;
    float spawnY = y * CELL_SIZE + CELL_SIZE * 0.5f;
    SpawnItemWithMaterial(spawnX, spawnY, (float)z, ITEM_LOG, (uint8_t)treeMat);

    CancelDesignation(x, y, z);
}

int CountChopDesignations(void) {
    int count = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (designations[z][y][x].type == DESIGNATION_CHOP) {
                    count++;
                }
            }
        }
    }
    return count;
}

int CountChopFelledDesignations(void) {
    int count = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (designations[z][y][x].type == DESIGNATION_CHOP_FELLED) {
                    count++;
                }
            }
        }
    }
    return count;
}

// =============================================================================
// Gather sapling designation functions
// =============================================================================

bool DesignateGatherSapling(int x, int y, int z) {
    // Bounds check
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    
    // Already designated?
    if (designations[z][y][x].type != DESIGNATION_NONE) {
        return false;
    }
    
    // Must be a sapling cell
    if (grid[z][y][x] != CELL_SAPLING) {
        return false;
    }
    
    designations[z][y][x].type = DESIGNATION_GATHER_SAPLING;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    activeDesignationCount++;
    InvalidateDesignationCache(DESIGNATION_GATHER_SAPLING);
    
    return true;
}

bool HasGatherSaplingDesignation(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    return designations[z][y][x].type == DESIGNATION_GATHER_SAPLING;
}

void CompleteGatherSaplingDesignation(int x, int y, int z, int moverIdx) {
    (void)moverIdx;  // Not needed for now
    
    // Remove the sapling cell
    MaterialType saplingMat = GetWallMaterial(x, y, z);
    MaterialType treeMat = NormalizeTreeTypeLocal(saplingMat);
    grid[z][y][x] = CELL_AIR;
    SetWallMaterial(x, y, z, MAT_NONE);
    MarkChunkDirty(x, y, z);
    
    // Spawn a sapling item at the location
    float spawnX = x * CELL_SIZE + CELL_SIZE * 0.5f;
    float spawnY = y * CELL_SIZE + CELL_SIZE * 0.5f;
    SpawnItemWithMaterial(spawnX, spawnY, (float)z,
                          SaplingItemFromTreeType(treeMat),
                          (uint8_t)saplingMat);
    
    // Clear designation
    designations[z][y][x].type = DESIGNATION_NONE;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    activeDesignationCount--;
    InvalidateDesignationCache(DESIGNATION_GATHER_SAPLING);
}

int CountGatherSaplingDesignations(void) {
    int count = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (designations[z][y][x].type == DESIGNATION_GATHER_SAPLING) {
                    count++;
                }
            }
        }
    }
    return count;
}

// =============================================================================
// Plant sapling designation functions
// =============================================================================

bool DesignatePlantSapling(int x, int y, int z) {
    // Bounds check
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    
    // Already designated?
    if (designations[z][y][x].type != DESIGNATION_NONE) {
        return false;
    }
    
    // Must be walkable (air above solid ground)
    if (!IsCellWalkableAt(z, y, x)) {
        return false;
    }
    
    // Must be air or grass surface (can't plant on ladders, etc.)
    CellType cell = grid[z][y][x];
    if (cell != CELL_AIR) {
        return false;
    }
    
    // Check if ground below is solid (soil/rock)
    if (z > 0) {
        CellType below = grid[z-1][y][x];
        if (!CellIsSolid(below)) {
            return false;
        }
    }
    
    designations[z][y][x].type = DESIGNATION_PLANT_SAPLING;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    activeDesignationCount++;
    InvalidateDesignationCache(DESIGNATION_PLANT_SAPLING);
    
    return true;
}

bool HasPlantSaplingDesignation(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    return designations[z][y][x].type == DESIGNATION_PLANT_SAPLING;
}

void CompletePlantSaplingDesignation(int x, int y, int z, MaterialType treeMat, int moverIdx) {
    (void)moverIdx;  // Not needed for now

    PlaceSapling(x, y, z, treeMat);
    
    // Clear designation
    designations[z][y][x].type = DESIGNATION_NONE;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    activeDesignationCount--;
    InvalidateDesignationCache(DESIGNATION_PLANT_SAPLING);
}

int CountPlantSaplingDesignations(void) {
    int count = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (designations[z][y][x].type == DESIGNATION_PLANT_SAPLING) {
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
    bp->type = BLUEPRINT_TYPE_WALL;
    bp->requiredMaterials = 1;  // 1 item to build a wall
    bp->deliveredMaterialCount = 0;
    bp->reservedItem = -1;
    bp->deliveredMaterial = MAT_NONE;
    bp->requiredItemType = ITEM_TYPE_COUNT;  // Any building material
    bp->assignedBuilder = -1;
    bp->progress = 0.0f;
    
    blueprintCount++;
    return idx;
}

int CreateLadderBlueprint(int x, int y, int z) {
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
    
    // Already has a ladder?
    CellType ct = grid[z][y][x];
    if (ct == CELL_LADDER_UP || ct == CELL_LADDER_DOWN || ct == CELL_LADDER_BOTH) {
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
    bp->type = BLUEPRINT_TYPE_LADDER;
    bp->requiredMaterials = 1;  // 1 stone block to build a ladder
    bp->deliveredMaterialCount = 0;
    bp->reservedItem = -1;
    bp->deliveredMaterial = MAT_NONE;
    bp->requiredItemType = ITEM_TYPE_COUNT;  // Any building material
    bp->assignedBuilder = -1;
    bp->progress = 0.0f;
    
    blueprintCount++;
    return idx;
}

int CreateFloorBlueprint(int x, int y, int z) {
    // Bounds check
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return -1;
    }
    
    // Can't build floor on a wall
    if (IsWallCell(grid[z][y][x])) {
        return -1;
    }
    
    // Already has a floor? (check flag or solid ground)
    CellType ct = grid[z][y][x];
    if (HAS_FLOOR(x, y, z) || CellIsSolid(ct)) {
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
    bp->type = BLUEPRINT_TYPE_FLOOR;
    bp->requiredMaterials = 1;  // 1 stone block to build a floor
    bp->deliveredMaterialCount = 0;
    bp->reservedItem = -1;
    bp->deliveredMaterial = MAT_NONE;
    bp->requiredItemType = ITEM_TYPE_COUNT;  // Any building material
    bp->assignedBuilder = -1;
    bp->progress = 0.0f;
    
    blueprintCount++;
    return idx;
}

int CreateRampBlueprint(int x, int y, int z) {
    // Bounds check
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return -1;
    }
    
    // Can only build ramp in air
    CellType ct = grid[z][y][x];
    if (ct != CELL_AIR) {
        return -1;
    }
    
    // Must have floor at this position (something to stand on while building)
    if (!HAS_FLOOR(x, y, z)) {
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
    bp->type = BLUEPRINT_TYPE_RAMP;
    bp->requiredMaterials = 1;  // 1 block to build a ramp
    bp->deliveredMaterialCount = 0;
    bp->reservedItem = -1;
    bp->deliveredMaterial = MAT_NONE;
    bp->requiredItemType = ITEM_TYPE_COUNT;  // Any building material
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
    
    // Refund delivered materials by spawning items at the blueprint location
    if (bp->deliveredMaterialCount > 0) {
        float spawnX = bp->x * CELL_SIZE + CELL_SIZE * 0.5f;
        float spawnY = bp->y * CELL_SIZE + CELL_SIZE * 0.5f;
        ItemType refundType = (bp->requiredItemType < ITEM_TYPE_COUNT) ? bp->requiredItemType : ITEM_BLOCKS;
        for (int i = 0; i < bp->deliveredMaterialCount; i++) {
            SpawnItemWithMaterial(spawnX, spawnY, (float)bp->z, refundType,
                                 (uint8_t)bp->deliveredMaterial);
        }
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
    
    // Record the material type before consuming the item
    MaterialType mat = (MaterialType)items[itemIdx].material;
    if (mat == MAT_NONE) {
        mat = (MaterialType)DefaultMaterialForItemType(items[itemIdx].type);
    }
    bp->deliveredMaterial = mat;
    
    // Consume the item
    DeleteItem(itemIdx);
    
    // Update blueprint
    bp->deliveredMaterialCount++;
    bp->reservedItem = -1;
    
    // Check if we have all materials
    if (bp->deliveredMaterialCount >= bp->requiredMaterials) {
        bp->state = BLUEPRINT_READY_TO_BUILD;
    }
}

void CompleteBlueprint(int blueprintIdx) {
    if (blueprintIdx < 0 || blueprintIdx >= MAX_BLUEPRINTS) return;
    
    Blueprint* bp = &blueprints[blueprintIdx];
    if (!bp->active) return;
    
    int x = bp->x, y = bp->y, z = bp->z;
    
    if (bp->type == BLUEPRINT_TYPE_WALL) {
        // Push any movers out of this cell before placing the wall
        PushMoversOutOfCell(x, y, z);
        
        // Push any items out of this cell before placing the wall
        PushItemsOutOfCell(x, y, z);
        
        // Convert floor to wall (or dirt for landscaping)
        if (IsCellWalkableAt(z, y, x)) {
            // Clean up ramps/ladders before overwriting
            ClearCellCleanup(x, y, z);
            DisplaceWater(x, y, z);
            if (bp->deliveredMaterial == MAT_DIRT) {
                // Dirt creates natural wall, not a constructed wall
                grid[z][y][x] = CELL_WALL;
                SetWallMaterial(x, y, z, MAT_DIRT);
                SetWallNatural(x, y, z);
                SetWallFinish(x, y, z, FINISH_ROUGH);
                CLEAR_FLOOR(x, y, z);  // Terrain is solid, no floor on top
                SetFloorMaterial(x, y, z, MAT_NONE);
                ClearFloorNatural(x, y, z);
                SetFloorFinish(x, y, z, FINISH_ROUGH);
                SET_CELL_SURFACE(x, y, z, SURFACE_BARE);  // No grass yet, will grow over time
            } else {
                grid[z][y][x] = CELL_WALL;
                SetWallMaterial(x, y, z, bp->deliveredMaterial);
                ClearWallNatural(x, y, z);
                SetWallFinish(x, y, z, FINISH_SMOOTH);
                // Floor material preserved - wall is built on top of floor
            }
            MarkChunkDirty(x, y, z);
            InvalidatePathsThroughCell(x, y, z);
        }
    } else if (bp->type == BLUEPRINT_TYPE_LADDER) {
        // Place ladder using existing ladder placement logic
        // This handles UP/DOWN/BOTH connections automatically
        PlaceLadder(x, y, z);
        SetWallMaterial(x, y, z, bp->deliveredMaterial);
        ClearWallNatural(x, y, z);
        SetWallFinish(x, y, z, FINISH_SMOOTH);
    } else if (bp->type == BLUEPRINT_TYPE_FLOOR) {
        // Clean up ramps/ladders before overwriting
        ClearCellCleanup(x, y, z);
        
        // Place floor - set floor flag on air cell (same as draw tool)
        DisplaceWater(x, y, z);
        grid[z][y][x] = CELL_AIR;
        SET_FLOOR(x, y, z);
        SET_CELL_SURFACE(x, y, z, SURFACE_BARE);  // Clear grass overlay
        SetFloorMaterial(x, y, z, bp->deliveredMaterial);
        ClearFloorNatural(x, y, z);
        SetFloorFinish(x, y, z, FINISH_SMOOTH);
        MarkChunkDirty(x, y, z);
    } else if (bp->type == BLUEPRINT_TYPE_RAMP) {
        // Push items out before placing ramp
        PushItemsOutOfCell(x, y, z);
        
        // Place ramp - auto-detect direction based on adjacent cells
        DisplaceWater(x, y, z);
        
        // Use similar logic to draw tool for auto-detecting ramp direction
        CellType rampType = CELL_RAMP_N;  // Default
        
        // Check for adjacent walls to determine ramp direction
        // Ramp faces AWAY from wall (toward walkable space)
        if (y > 0 && IsWallCell(grid[z][y-1][x])) {
            rampType = CELL_RAMP_S;  // Wall to north, exit south
        } else if (x < gridWidth - 1 && IsWallCell(grid[z][y][x+1])) {
            rampType = CELL_RAMP_W;  // Wall to east, exit west
        } else if (y < gridHeight - 1 && IsWallCell(grid[z][y+1][x])) {
            rampType = CELL_RAMP_N;  // Wall to south, exit north
        } else if (x > 0 && IsWallCell(grid[z][y][x-1])) {
            rampType = CELL_RAMP_E;  // Wall to west, exit east
        }
        
        grid[z][y][x] = rampType;
        rampCount++;
        CLEAR_FLOOR(x, y, z);  // Ramps don't have floors
        SetWallMaterial(x, y, z, bp->deliveredMaterial);
        ClearWallNatural(x, y, z);
        SetWallFinish(x, y, z, FINISH_SMOOTH);
        
        // Create floor above the ramp (at z+1)
        if (z + 1 < gridDepth && grid[z+1][y][x] == CELL_AIR) {
            SET_FLOOR(x, y, z + 1);
            SetFloorMaterial(x, y, z + 1, bp->deliveredMaterial);
            ClearFloorNatural(x, y, z + 1);
            SetFloorFinish(x, y, z + 1, FINISH_SMOOTH);
        }
        
        MarkChunkDirty(x, y, z);
        if (z + 1 < gridDepth) MarkChunkDirty(x, y, z + 1);
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
