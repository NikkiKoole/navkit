#include "designations.h"
#include "grid.h"
#include "cell_defs.h"
#include "material.h"
#include "pathfinding.h"
#include "../entities/items.h"
#include "../entities/mover.h"  // for CELL_SIZE
#include "../simulation/water.h"
#include "../simulation/trees.h"
#include "../simulation/groundwear.h"
#include "../simulation/floordirt.h"
#include "../simulation/plants.h"
#include "../simulation/farming.h"
#include "../core/sim_manager.h"
#include "../entities/jobs.h"    // for CancelJob, GetJob forward declarations
#include "../entities/furniture.h"
#include "../entities/workshops.h"
#include "../entities/stacking.h"
#include "../core/event_log.h"
#include "../game_state.h"
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
    }
    blueprintCount = 0;
}

bool DesignateMine(int x, int y, int z) {
    // Bounds check
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    if (!IsExplored(x, y, z)) return false;

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
        SetWallSourceItem(x, y, z, ITEM_NONE);
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
                if (d->type == DESIGNATION_NONE) continue;
                if (d->unreachableCooldown > 0.0f) {
                    d->unreachableCooldown = fmaxf(0.0f, d->unreachableCooldown - dt);
                }

                // Validate: if assignedMover is set, that mover must have an active job.
                // A mismatch means a bug left a stale assignedMover (e.g. stale cache,
                // failed job without proper cleanup). Auto-clear to prevent stuck designations.
                if (d->assignedMover >= 0 && d->assignedMover < moverCount) {
                    if (movers[d->assignedMover].currentJobId < 0) {
                        TraceLog(LOG_WARNING,
                            "STALE DESIGNATION: %s at (%d,%d,z%d) assignedMover=%d but mover is idle - clearing",
                            DesignationTypeName(d->type), x, y, z, d->assignedMover);
                        d->assignedMover = -1;
                    }
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
    if (!IsExplored(x, y, z)) return false;

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
    if (!IsExplored(x, y, z)) return false;

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
    if (!IsExplored(x, y, z)) return false;

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
    
    // Spawn an item from the removed floor (at mover's level so it's reachable)
    if (dropItem != ITEM_NONE) {
        int dropZ = z;
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
    if (!IsExplored(x, y, z)) return false;

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
    if (!IsExplored(x, y, z)) return false;

    // Accept trunk, branch, or leaves — trace to find tree base
    CellType cell = grid[z][y][x];
    if (cell == CELL_TREE_BRANCH || cell == CELL_TREE_LEAVES) {
        // Check if this is a young tree (branch column on ground)
        if (cell == CELL_TREE_BRANCH && IsYoungTreeBase(x, y, z)) {
            // Already at young tree base — proceed directly
        } else {
            // Find trunk column: branches are always adjacent to trunk at same z
            int trunkX = -1, trunkY = -1;
            static const int dx[] = {0, 1, 0, -1};
            static const int dy[] = {-1, 0, 1, 0};
            for (int d = 0; d < 4; d++) {
                int nx = x + dx[d], ny = y + dy[d];
                if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
                if (grid[z][ny][nx] == CELL_TREE_TRUNK) { trunkX = nx; trunkY = ny; break; }
            }
            if (trunkX < 0) return false;
            x = trunkX;
            y = trunkY;
        }
    }
    
    // Must be a tree trunk or young tree branch base (not felled)
    if (grid[z][y][x] == CELL_TREE_TRUNK) {
        // Trace down to trunk base (lowest trunk cell)
        while (z > 0 && grid[z - 1][y][x] == CELL_TREE_TRUNK) {
            z--;
        }
    } else if (IsYoungTreeBase(x, y, z)) {
        // Already at young tree base
    } else {
        return false;
    }
    
    // Already designated?
    if (designations[z][y][x].type != DESIGNATION_NONE) {
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
    if (!IsExplored(x, y, z)) return false;

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

    // Check if this is a young tree (CELL_TREE_BRANCH base on ground)
    bool isYoungTree = IsYoungTreeBase(x, y, z);

    int baseZ, trunkHeight;
    if (isYoungTree) {
        baseZ = z;
        trunkHeight = 0;
        for (int cz = z; cz < gridDepth && grid[cz][y][x] == CELL_TREE_BRANCH; cz++)
            trunkHeight++;
    } else {
        baseZ = FindTrunkBaseZAt(x, y, z);
        trunkHeight = GetTrunkHeightAt(x, y, baseZ);
    }
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

        // Don't remove roots at z=0 — they replaced the natural ground cell
        // and removing them would leave a hole to bedrock
        if (cellType == CELL_TREE_ROOT && cz == 0) continue;

        // If removing a trunk base with depleted harvest, decrement regen counter
        if (cellType == CELL_TREE_TRUNK &&
            (cz == 0 || grid[cz - 1][cy][cx] != CELL_TREE_TRUNK) &&
            treeHarvestState[cz][cy][cx] < TREE_HARVEST_MAX) {
            treeRegenCells--;
        }

        grid[cz][cy][cx] = CELL_AIR;
        SetWallMaterial(cx, cy, cz, MAT_NONE);
        MarkChunkDirty(cx, cy, cz);

        if (designations[cz][cy][cx].type != DESIGNATION_NONE) {
            InvalidateDesignationCache(designations[cz][cy][cx].type);
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

    if (isYoungTree) {
        // Young tree: drop poles and sticks directly (no felled trunk segments)
        float spawnX = x * CELL_SIZE + CELL_SIZE * 0.5f;
        float spawnY = y * CELL_SIZE + CELL_SIZE * 0.5f;
        int itemZ = FindItemSpawnZAtLocal(x, y, baseZ);

        // 1 pole per branch cell
        for (int i = 0; i < trunkHeight; i++) {
            SpawnItemWithMaterial(spawnX, spawnY, (float)itemZ, ITEM_POLES, (uint8_t)treeMat);
        }
        // 1-2 bonus sticks
        int stickCount = 1 + (trunkHeight > 1 ? 1 : 0);
        for (int i = 0; i < stickCount; i++) {
            SpawnItemWithMaterial(spawnX, spawnY, (float)itemZ, ITEM_STICKS, (uint8_t)treeMat);
        }

        // Decrement active cells (young tree was tracked)
        treeActiveCells--;
    } else {
        // Mature tree: fall direction, felled trunk segments, leaf/sapling drops

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
    TriggerScreenShake(4.0f, 0.3f);
    
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
    if (!IsExplored(x, y, z)) return false;

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
    if (!IsExplored(x, y, z)) return false;

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
// Gather grass designation functions
// =============================================================================

// Find the z-level that has vegetation for a given walking position
// In DF mode, vegetation is on z-1 (dirt below). In flat mode, on z itself.
static int FindVegetationZ(int x, int y, int z) {
    if (GetVegetation(x, y, z) >= VEG_GRASS_TALLER) return z;
    if (z > 0 && GetVegetation(x, y, z - 1) >= VEG_GRASS_TALLER) return z - 1;
    return -1;
}

bool DesignateGatherGrass(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    if (!IsExplored(x, y, z)) return false;

    if (designations[z][y][x].type != DESIGNATION_NONE) {
        return false;
    }
    
    // Must have harvestable vegetation below (or at) this position
    if (FindVegetationZ(x, y, z) < 0) {
        return false;
    }
    
    // Must be walkable (so movers can reach adjacent)
    if (!IsCellWalkableAt(z, y, x)) {
        return false;
    }
    
    designations[z][y][x].type = DESIGNATION_GATHER_GRASS;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    activeDesignationCount++;
    InvalidateDesignationCache(DESIGNATION_GATHER_GRASS);
    EventLog("Designated GATHER_GRASS at (%d,%d,z%d) walkable=%d", x, y, z, IsCellWalkableAt(z, y, x));
    
    return true;
}

bool HasGatherGrassDesignation(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    return designations[z][y][x].type == DESIGNATION_GATHER_GRASS;
}

void CompleteGatherGrassDesignation(int x, int y, int z, int moverIdx) {
    (void)moverIdx;
    
    // Find where the vegetation actually is
    int vegZ = FindVegetationZ(x, y, z);
    if (vegZ >= 0) {
        SetVegetation(x, y, vegZ, VEG_NONE);
        SET_CELL_SURFACE(x, y, vegZ, SURFACE_TRAMPLED);
        wearGrid[vegZ][y][x] = wearNormalToTrampled;
    }
    
    // Spawn grass item at the walking position
    float spawnX = x * CELL_SIZE + CELL_SIZE * 0.5f;
    float spawnY = y * CELL_SIZE + CELL_SIZE * 0.5f;
    SpawnItem(spawnX, spawnY, (float)z, ITEM_GRASS);
    
    // Clear designation
    designations[z][y][x].type = DESIGNATION_NONE;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    activeDesignationCount--;
    InvalidateDesignationCache(DESIGNATION_GATHER_GRASS);
}

int CountGatherGrassDesignations(void) {
    int count = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (designations[z][y][x].type == DESIGNATION_GATHER_GRASS) {
                    count++;
                }
            }
        }
    }
    return count;
}

// =============================================================================
// Gather tree designations
// =============================================================================

bool DesignateGatherTree(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    if (!IsExplored(x, y, z)) return false;

    if (designations[z][y][x].type != DESIGNATION_NONE) {
        return false;
    }

    // Must be a tree trunk cell
    if (grid[z][y][x] != CELL_TREE_TRUNK) {
        return false;
    }

    // Check harvest state at trunk base
    int baseZ = FindTrunkBaseZAt(x, y, z);
    if (treeHarvestState[baseZ][y][x] <= 0) {
        return false;
    }

    designations[z][y][x].type = DESIGNATION_GATHER_TREE;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    activeDesignationCount++;
    InvalidateDesignationCache(DESIGNATION_GATHER_TREE);

    return true;
}

bool HasGatherTreeDesignation(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    return designations[z][y][x].type == DESIGNATION_GATHER_TREE;
}

void CompleteGatherTreeDesignation(int x, int y, int z, int moverIdx) {
    int baseZ = FindTrunkBaseZAt(x, y, z);
    MaterialType treeMat = GetWallMaterial(x, y, baseZ);

    // Decrement harvest state
    if (treeHarvestState[baseZ][y][x] > 0) {
        bool wasMax = (treeHarvestState[baseZ][y][x] >= TREE_HARVEST_MAX);
        treeHarvestState[baseZ][y][x]--;
        growthTimer[baseZ][y][x] = 0;  // Reset regen timer
        if (wasMax && treeHarvestState[baseZ][y][x] < TREE_HARVEST_MAX) {
            treeRegenCells++;
        }
    }

    // Spawn items at mover position (trunk cell is solid, not walkable)
    float spawnX = x * CELL_SIZE + CELL_SIZE * 0.5f;
    float spawnY = y * CELL_SIZE + CELL_SIZE * 0.5f;
    if (moverIdx >= 0 && moverIdx < moverCount && movers[moverIdx].active) {
        spawnX = movers[moverIdx].x;
        spawnY = movers[moverIdx].y;
    }
    SpawnItemWithMaterial(spawnX, spawnY, (float)z, ITEM_STICKS, (uint8_t)treeMat);
    SpawnItemWithMaterial(spawnX, spawnY, (float)z, ITEM_LEAVES, (uint8_t)treeMat);

    // Clear designation
    designations[z][y][x].type = DESIGNATION_NONE;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    activeDesignationCount--;
    InvalidateDesignationCache(DESIGNATION_GATHER_TREE);
}

int CountGatherTreeDesignations(void) {
    int count = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (designations[z][y][x].type == DESIGNATION_GATHER_TREE) {
                    count++;
                }
            }
        }
    }
    return count;
}

// =============================================================================
// Clean designation functions
// =============================================================================

bool DesignateClean(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    if (!IsExplored(x, y, z)) return false;

    if (designations[z][y][x].type != DESIGNATION_NONE) {
        return false;
    }

    // Must have a dirty floor above threshold
    if (GetFloorDirt(x, y, z) < DIRT_CLEAN_THRESHOLD) {
        return false;
    }

    // Must be walkable (mover stands on tile to clean)
    if (!IsCellWalkableAt(z, y, x)) {
        return false;
    }

    designations[z][y][x].type = DESIGNATION_CLEAN;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    activeDesignationCount++;
    InvalidateDesignationCache(DESIGNATION_CLEAN);

    return true;
}

bool HasCleanDesignation(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    return designations[z][y][x].type == DESIGNATION_CLEAN;
}

void CompleteCleanDesignation(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return;
    }

    // Fully clean the tile (set to 0)
    SetFloorDirt(x, y, z, 0);

    if (designations[z][y][x].type != DESIGNATION_NONE) {
        activeDesignationCount--;
    }
    designations[z][y][x].type = DESIGNATION_NONE;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    InvalidateDesignationCache(DESIGNATION_CLEAN);
}

int CountCleanDesignations(void) {
    int count = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (designations[z][y][x].type == DESIGNATION_CLEAN) {
                    count++;
                }
            }
        }
    }
    return count;
}

// =============================================================================
// Harvest berry designation functions
// =============================================================================

bool DesignateHarvestBerry(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    if (!IsExplored(x, y, z)) return false;

    // Auto-correct z-level: if no plant here but there's one at z-1 (bush visible from above)
    if (!IsPlantRipe(x, y, z) && z > 0 && IsPlantRipe(x, y, z - 1)) {
        z = z - 1;
    }

    if (designations[z][y][x].type != DESIGNATION_NONE) {
        return false;
    }

    // Must have a ripe berry bush at this cell
    if (!IsPlantRipe(x, y, z)) {
        return false;
    }

    // Must be walkable (mover stands on tile to harvest)
    if (!IsCellWalkableAt(z, y, x)) {
        return false;
    }

    designations[z][y][x].type = DESIGNATION_HARVEST_BERRY;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    activeDesignationCount++;
    InvalidateDesignationCache(DESIGNATION_HARVEST_BERRY);

    return true;
}

bool HasHarvestBerryDesignation(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    return designations[z][y][x].type == DESIGNATION_HARVEST_BERRY;
}

void CompleteHarvestBerryDesignation(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return;
    }

    // Harvest the plant (resets to bare, spawns ITEM_BERRIES)
    HarvestPlant(x, y, z);

    if (designations[z][y][x].type != DESIGNATION_NONE) {
        activeDesignationCount--;
    }
    designations[z][y][x].type = DESIGNATION_NONE;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    InvalidateDesignationCache(DESIGNATION_HARVEST_BERRY);
}

int CountHarvestBerryDesignations(void) {
    int count = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (designations[z][y][x].type == DESIGNATION_HARVEST_BERRY) {
                    count++;
                }
            }
        }
    }
    return count;
}

// =============================================================================
// Knap designation functions
// =============================================================================

bool DesignateKnap(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    if (!IsExplored(x, y, z)) return false;

    // If viewing walkable layer (air above boulder), target the cell below
    if (!CellIsSolid(grid[z][y][x]) && z > 0 && CellIsSolid(grid[z-1][y][x])) {
        z = z - 1;
    }

    if (designations[z][y][x].type != DESIGNATION_NONE) {
        return false;
    }

    // Must be a solid wall made of stone
    if (!CellIsSolid(grid[z][y][x])) {
        return false;
    }

    MaterialType mat = GetWallMaterial(x, y, z);
    if (!IsStoneMaterial(mat)) {
        return false;
    }

    designations[z][y][x].type = DESIGNATION_KNAP;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    activeDesignationCount++;
    InvalidateDesignationCache(DESIGNATION_KNAP);

    return true;
}

bool HasKnapDesignation(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    if (designations[z][y][x].type == DESIGNATION_KNAP) return true;
    // Check cell below (designation targets the wall, not the air above)
    if (z > 0 && designations[z-1][y][x].type == DESIGNATION_KNAP) return true;
    return false;
}

void CompleteKnapDesignation(int x, int y, int z, int moverIdx) {
    (void)moverIdx;
    // Wall is NOT consumed — just clear the designation
    designations[z][y][x].type = DESIGNATION_NONE;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    activeDesignationCount--;
    InvalidateDesignationCache(DESIGNATION_KNAP);
}

int CountKnapDesignations(void) {
    int count = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (designations[z][y][x].type == DESIGNATION_KNAP) {
                    count++;
                }
            }
        }
    }
    return count;
}

// =============================================================================
// Dig roots designation functions
// =============================================================================

bool DesignateDigRoots(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    if (!IsExplored(x, y, z)) return false;

    if (designations[z][y][x].type != DESIGNATION_NONE) {
        return false;
    }

    // Must be walkable (mover stands on this cell)
    if (!IsCellWalkableAt(z, y, x)) {
        return false;
    }

    // Check z-1 for natural soil (dirt/clay/peat — NOT sand/gravel, too dry/rocky)
    if (z <= 0) return false;
    if (!CellIsSolid(grid[z - 1][y][x])) return false;
    if (!IsWallNatural(x, y, z - 1)) return false;
    MaterialType belowMat = GetWallMaterial(x, y, z - 1);
    if (belowMat != MAT_DIRT && belowMat != MAT_CLAY && belowMat != MAT_PEAT) {
        return false;
    }

    designations[z][y][x].type = DESIGNATION_DIG_ROOTS;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    activeDesignationCount++;
    InvalidateDesignationCache(DESIGNATION_DIG_ROOTS);
    EventLog("Designated DIG_ROOTS at (%d,%d,z%d) mat=%s", x, y, z, MaterialName(belowMat));

    return true;
}

bool HasDigRootsDesignation(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    return designations[z][y][x].type == DESIGNATION_DIG_ROOTS;
}

void CompleteDigRootsDesignation(int x, int y, int z, int moverIdx) {
    (void)moverIdx;

    // Determine yield based on soil type
    int rootCount = 1;
    if (z > 0) {
        MaterialType belowMat = GetWallMaterial(x, y, z - 1);
        if (belowMat == MAT_PEAT) rootCount = 2;  // peat is richer
    }

    // Spawn root items at the walking position
    float spawnX = x * CELL_SIZE + CELL_SIZE * 0.5f;
    float spawnY = y * CELL_SIZE + CELL_SIZE * 0.5f;
    for (int i = 0; i < rootCount; i++) {
        SpawnItem(spawnX, spawnY, (float)z, ITEM_ROOT);
    }

    // Clear designation
    designations[z][y][x].type = DESIGNATION_NONE;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    activeDesignationCount--;
    InvalidateDesignationCache(DESIGNATION_DIG_ROOTS);
}

int CountDigRootsDesignations(void) {
    int count = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (designations[z][y][x].type == DESIGNATION_DIG_ROOTS) {
                    count++;
                }
            }
        }
    }
    return count;
}

// =============================================================================
// Explore designation functions
// =============================================================================

bool DesignateExplore(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }

    if (designations[z][y][x].type != DESIGNATION_NONE) {
        return false;
    }

    // Target cell CAN be unwalkable (wall you're scouting toward) — minimal validation
    designations[z][y][x].type = DESIGNATION_EXPLORE;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    activeDesignationCount++;
    InvalidateDesignationCache(DESIGNATION_EXPLORE);

    return true;
}

bool HasExploreDesignation(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    return designations[z][y][x].type == DESIGNATION_EXPLORE;
}

void CompleteExploreDesignation(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return;
    }

    if (designations[z][y][x].type != DESIGNATION_NONE) {
        activeDesignationCount--;
    }
    designations[z][y][x].type = DESIGNATION_NONE;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    InvalidateDesignationCache(DESIGNATION_EXPLORE);
}

int CountExploreDesignations(void) {
    int count = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (designations[z][y][x].type == DESIGNATION_EXPLORE) {
                    count++;
                }
            }
        }
    }
    return count;
}

// =============================================================================
// Farm designation functions
// =============================================================================

bool DesignateFarm(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    if (!IsExplored(x, y, z)) return false;
    if (!IsFarmableSoil(x, y, z)) return false;
    if (designations[z][y][x].type != DESIGNATION_NONE) return false;
    // Don't designate already-tilled cells
    if (farmGrid[z][y][x].tilled) return false;

    designations[z][y][x].type = DESIGNATION_FARM;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    designations[z][y][x].unreachableCooldown = 0.0f;
    activeDesignationCount++;
    InvalidateDesignationCache(DESIGNATION_FARM);
    return true;
}

bool HasFarmDesignation(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    return designations[z][y][x].type == DESIGNATION_FARM;
}

void CompleteFarmDesignation(int x, int y, int z, int moverIdx) {
    (void)moverIdx;
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return;
    }

    // Set farm cell state
    MaterialType mat = (z > 0) ? GetWallMaterial(x, y, z - 1) : MAT_DIRT;
    farmGrid[z][y][x].tilled = 1;
    farmGrid[z][y][x].fertility = InitialFertilityForSoil(mat);
    farmGrid[z][y][x].weedLevel = 0;
    farmGrid[z][y][x].desiredCropType = 0;
    farmActiveCells++;

    // Clear grass/vegetation — drop grass item if there was grass
    VegetationType veg = (z > 0) ? GetVegetation(x, y, z - 1) : GetVegetation(x, y, z);
    if (veg >= VEG_GRASS_SHORT) {
        float spawnX = x * CELL_SIZE + CELL_SIZE * 0.5f;
        float spawnY = y * CELL_SIZE + CELL_SIZE * 0.5f;
        SpawnItem(spawnX, spawnY, (float)z, ITEM_GRASS);
    }
    SetVegetation(x, y, z, VEG_NONE);
    if (z > 0) SetVegetation(x, y, z - 1, VEG_NONE);

    // Clear designation
    if (designations[z][y][x].type != DESIGNATION_NONE) {
        activeDesignationCount--;
    }
    designations[z][y][x].type = DESIGNATION_NONE;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    InvalidateDesignationCache(DESIGNATION_FARM);
}

int CountFarmDesignations(void) {
    int count = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (designations[z][y][x].type == DESIGNATION_FARM) {
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

// Helper: initialize a blueprint slot with common defaults
static void InitBlueprintSlot(Blueprint* bp, int x, int y, int z) {
    memset(bp, 0, sizeof(*bp));
    bp->x = x;
    bp->y = y;
    bp->z = z;
    bp->active = true;
    bp->state = BLUEPRINT_AWAITING_MATERIALS;
    bp->recipeIndex = -1;
    bp->stage = 0;
    for (int s = 0; s < MAX_INPUTS_PER_STAGE; s++) {
        bp->stageDeliveries[s].chosenAlternative = -1;
        bp->stageDeliveries[s].deliveredMaterial = MAT_NONE;
    }
    bp->assignedBuilder = -1;
    bp->progress = 0.0f;
}

int CreateRecipeBlueprint(int x, int y, int z, int recipeIndex) {
    // Validate recipe
    const ConstructionRecipe* recipe = GetConstructionRecipe(recipeIndex);
    if (!recipe) return -1;

    // Bounds check
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return -1;
    }
    if (!IsExplored(x, y, z)) return -1;

    // Category-specific preconditions
    if (recipe->buildCategory == BUILD_WALL || recipe->buildCategory == BUILD_LADDER) {
        if (!IsCellWalkableAt(z, y, x)) return -1;
    } else if (recipe->buildCategory == BUILD_FLOOR) {
        if (IsWallCell(grid[z][y][x])) return -1;
        CellType ct = grid[z][y][x];
        if (HAS_FLOOR(x, y, z) || CellIsSolid(ct)) return -1;
    } else if (recipe->buildCategory == BUILD_RAMP) {
        // Must be walkable (matches sandbox ramp validation)
        if (!IsCellWalkableAt(z, y, x)) return -1;
        // Can't replace existing ramps or ladders
        CellType ct = grid[z][y][x];
        if (CellIsDirectionalRamp(ct) || CellIsLadder(ct)) return -1;
    } else if (recipe->buildCategory == BUILD_FURNITURE) {
        if (!IsCellWalkableAt(z, y, x)) return -1;
        if (GetFurnitureAt(x, y, z) >= 0) return -1;
    } else if (recipe->buildCategory == BUILD_DOOR) {
        if (!IsCellWalkableAt(z, y, x)) return -1;
        // Door must be adjacent to at least one wall
        bool hasWallNeighbor = false;
        static const int dx4[] = {0, 0, -1, 1};
        static const int dy4[] = {-1, 1, 0, 0};
        for (int d = 0; d < 4; d++) {
            int nx = x + dx4[d], ny = y + dy4[d];
            if (nx >= 0 && nx < gridWidth && ny >= 0 && ny < gridHeight) {
                CellType nc = grid[z][ny][nx];
                if (CellBlocksMovement(nc) || nc == CELL_DOOR) {
                    hasWallNeighbor = true;
                    break;
                }
            }
        }
        if (!hasWallNeighbor) return -1;
    } else if (recipe->buildCategory == BUILD_WORKSHOP) {
        // Workshop blueprints are placed at the work tile via CreateWorkshopBlueprint()
        // which handles full footprint validation. Just check work tile is walkable.
        if (!IsCellWalkableAt(z, y, x)) return -1;
    }

    // Already has a blueprint?
    if (HasBlueprint(x, y, z)) return -1;

    // Find free slot
    int idx = -1;
    for (int i = 0; i < MAX_BLUEPRINTS; i++) {
        if (!blueprints[i].active) { idx = i; break; }
    }
    if (idx < 0) return -1;

    Blueprint* bp = &blueprints[idx];
    InitBlueprintSlot(bp, x, y, z);
    bp->recipeIndex = recipeIndex;

    // Check for items on ground at this cell — start in CLEARING if any exist
    bool hasItems = false;
    for (int i = 0; i < itemHighWaterMark && !hasItems; i++) {
        if (!items[i].active) continue;
        if ((int)items[i].z != z) continue;
        if (items[i].state != ITEM_ON_GROUND && items[i].state != ITEM_IN_STOCKPILE) continue;
        int ix = (int)(items[i].x / CELL_SIZE);
        int iy = (int)(items[i].y / CELL_SIZE);
        if (ix == x && iy == y) hasItems = true;
    }
    if (hasItems) {
        bp->state = BLUEPRINT_CLEARING;
        EventLog("Blueprint %d at (%d,%d,z%d) recipe=%d -> CLEARING", idx, x, y, z, bp->recipeIndex);
    } else {
        EventLog("Blueprint %d at (%d,%d,z%d) recipe=%d -> AWAITING_MATERIALS", idx, x, y, z, bp->recipeIndex);
    }

    blueprintCount++;
    return idx;
}

// Helper: check if any blueprint overlaps a rectangular area
static bool HasBlueprintInArea(int x1, int y1, int x2, int y2, int z) {
    for (int i = 0; i < MAX_BLUEPRINTS; i++) {
        if (!blueprints[i].active) continue;
        if (blueprints[i].z != z) continue;
        int bx = blueprints[i].x;
        int by = blueprints[i].y;
        // Check if this blueprint's cell is within the area
        if (bx >= x1 && bx <= x2 && by >= y1 && by <= y2) return true;
        // Also check if this is a workshop blueprint whose footprint overlaps
        const ConstructionRecipe* r = GetConstructionRecipe(blueprints[i].recipeIndex);
        if (r && r->buildCategory == BUILD_WORKSHOP) {
            int ox = blueprints[i].workshopOriginX;
            int oy = blueprints[i].workshopOriginY;
            const WorkshopDef* def = &workshopDefs[blueprints[i].workshopType];
            // Check rectangle overlap
            if (ox <= x2 && ox + def->width - 1 >= x1 &&
                oy <= y2 && oy + def->height - 1 >= y1) return true;
        }
    }
    return false;
}

int CreateWorkshopBlueprint(int originX, int originY, int z, int recipeIndex) {
    const ConstructionRecipe* recipe = GetConstructionRecipe(recipeIndex);
    if (!recipe || recipe->buildCategory != BUILD_WORKSHOP) return -1;

    // Map recipe to workshop type
    int workshopType = -1;
    switch (recipeIndex) {
        case CONSTRUCTION_WORKSHOP_CAMPFIRE:     workshopType = WORKSHOP_CAMPFIRE; break;
        case CONSTRUCTION_WORKSHOP_DRYING_RACK:  workshopType = WORKSHOP_DRYING_RACK; break;
        case CONSTRUCTION_WORKSHOP_ROPE_MAKER:   workshopType = WORKSHOP_ROPE_MAKER; break;
        case CONSTRUCTION_WORKSHOP_CHARCOAL_PIT: workshopType = WORKSHOP_CHARCOAL_PIT; break;
        case CONSTRUCTION_WORKSHOP_HEARTH:       workshopType = WORKSHOP_HEARTH; break;
        case CONSTRUCTION_WORKSHOP_STONECUTTER:  workshopType = WORKSHOP_STONECUTTER; break;
        case CONSTRUCTION_WORKSHOP_SAWMILL:      workshopType = WORKSHOP_SAWMILL; break;
        case CONSTRUCTION_WORKSHOP_KILN:         workshopType = WORKSHOP_KILN; break;
        case CONSTRUCTION_WORKSHOP_CARPENTER:    workshopType = WORKSHOP_CARPENTER; break;
        case CONSTRUCTION_WORKSHOP_GROUND_FIRE:  workshopType = WORKSHOP_GROUND_FIRE; break;
        case CONSTRUCTION_WORKSHOP_BUTCHER:      workshopType = WORKSHOP_BUTCHER; break;
        case CONSTRUCTION_WORKSHOP_COMPOST_PILE: workshopType = WORKSHOP_COMPOST_PILE; break;
        case CONSTRUCTION_WORKSHOP_QUERN:       workshopType = WORKSHOP_QUERN; break;
        default: return -1;
    }

    const WorkshopDef* def = &workshopDefs[workshopType];

    // Validate entire workshop footprint
    for (int dy = 0; dy < def->height; dy++) {
        for (int dx = 0; dx < def->width; dx++) {
            int cx = originX + dx;
            int cy = originY + dy;
            if (cx < 0 || cx >= gridWidth || cy < 0 || cy >= gridHeight || z < 0 || z >= gridDepth) {
                return -1;
            }
            if (!IsExplored(cx, cy, z)) {
                return -1;
            }
            if (!IsCellWalkableAt(z, cy, cx)) return -1;
            if (FindWorkshopAt(cx, cy, z) >= 0) return -1;
        }
    }

    // Check no blueprint overlaps the footprint
    if (HasBlueprintInArea(originX, originY,
                           originX + def->width - 1, originY + def->height - 1, z)) {
        return -1;
    }

    // Find the work tile position (where blueprint cell goes)
    int workTileX = originX, workTileY = originY;
    for (int ty = 0; ty < def->height; ty++) {
        for (int tx = 0; tx < def->width; tx++) {
            if (def->template[ty * def->width + tx] == WT_WORK) {
                workTileX = originX + tx;
                workTileY = originY + ty;
            }
        }
    }

    // Create blueprint at the work tile
    int idx = CreateRecipeBlueprint(workTileX, workTileY, z, recipeIndex);
    if (idx < 0) return -1;

    // Set workshop-specific fields
    blueprints[idx].workshopOriginX = originX;
    blueprints[idx].workshopOriginY = originY;
    blueprints[idx].workshopType = workshopType;

    return idx;
}

void CancelBlueprint(int blueprintIdx) {
    if (blueprintIdx < 0 || blueprintIdx >= MAX_BLUEPRINTS) return;
    
    Blueprint* bp = &blueprints[blueprintIdx];
    if (!bp->active) return;

    // Proactively cancel all jobs targeting this blueprint.
    // Must happen before bp->active = false so CancelJob's cleanup works.
    for (int i = 0; i < moverCount; i++) {
        if (movers[i].currentJobId < 0) continue;
        Job* job = GetJob(movers[i].currentJobId);
        if (job && job->targetBlueprint == blueprintIdx) {
            CancelJob((void*)&movers[i], i);
        }
    }
    
    float spawnX = bp->x * CELL_SIZE + CELL_SIZE * 0.5f;
    float spawnY = bp->y * CELL_SIZE + CELL_SIZE * 0.5f;

    const ConstructionRecipe* recipe = GetConstructionRecipe(bp->recipeIndex);

    // Refund current stage delivered items — 100% (not yet built into anything)
    if (recipe) {
        const ConstructionStage* stage = &recipe->stages[bp->stage];
        for (int s = 0; s < stage->inputCount; s++) {
            StageDelivery* sd = &bp->stageDeliveries[s];
            if (sd->deliveredCount > 0) {
                ItemType refundType = ITEM_ROCK;  // fallback
                if (sd->chosenAlternative >= 0 && sd->chosenAlternative < stage->inputs[s].altCount) {
                    refundType = stage->inputs[s].alternatives[sd->chosenAlternative].itemType;
                } else if (stage->inputs[s].altCount > 0) {
                    refundType = stage->inputs[s].alternatives[0].itemType;
                }
                for (int i = 0; i < sd->deliveredCount; i++) {
                    SpawnItemWithMaterial(spawnX, spawnY, (float)bp->z,
                                         refundType, (uint8_t)sd->deliveredMaterial);
                }
            }
        }
    }

    // Refund consumed items from completed stages — lossy
    for (int st = 0; st < bp->stage; st++) {
        for (int s = 0; s < MAX_INPUTS_PER_STAGE; s++) {
            ConsumedRecord* cr = &bp->consumedItems[st][s];
            if (cr->count > 0 && cr->itemType != ITEM_NONE) {
                for (int i = 0; i < cr->count; i++) {
                    if (GetRandomValue(1, 100) <= CONSTRUCTION_REFUND_CHANCE) {
                        SpawnItemWithMaterial(spawnX, spawnY, (float)bp->z,
                                             cr->itemType, (uint8_t)cr->material);
                    }
                }
            }
        }
    }
    
    bp->active = false;
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
        if (!bp->active || bp->state != BLUEPRINT_AWAITING_MATERIALS) continue;

        const ConstructionRecipe* recipe = GetConstructionRecipe(bp->recipeIndex);
        if (!recipe) continue;
        const ConstructionStage* stage = &recipe->stages[bp->stage];
        for (int s = 0; s < stage->inputCount; s++) {
            StageDelivery* sd = &bp->stageDeliveries[s];
            if (sd->deliveredCount + sd->reservedCount < stage->inputs[s].count) {
                return i;
            }
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
    ItemType deliveredType = items[itemIdx].type;

    const ConstructionRecipe* recipe = GetConstructionRecipe(bp->recipeIndex);
    if (!recipe) return;
    const ConstructionStage* stage = &recipe->stages[bp->stage];

    // Find which slot this item goes to
    int targetSlot = -1;
    for (int s = 0; s < stage->inputCount; s++) {
        StageDelivery* sd = &bp->stageDeliveries[s];
        if (sd->deliveredCount >= stage->inputs[s].count) continue;  // slot full
        if (!ConstructionInputAcceptsItem(&stage->inputs[s], deliveredType)) continue;
        // Check locking: if slot has a chosen alternative, must match
        if (sd->chosenAlternative >= 0) {
            if (stage->inputs[s].alternatives[sd->chosenAlternative].itemType != deliveredType) continue;
            if (sd->deliveredMaterial != MAT_NONE && sd->deliveredMaterial != mat) continue;
        }
        targetSlot = s;
        break;
    }

    if (targetSlot < 0) return;  // no matching slot (shouldn't happen if WorkGiver is correct)

    StageDelivery* sd = &bp->stageDeliveries[targetSlot];
    int remaining = stage->inputs[targetSlot].count - sd->deliveredCount;
    int stackCount = items[itemIdx].stackCount;
    if (stackCount < 1) stackCount = 1;
    int toDeliver = (stackCount < remaining) ? stackCount : remaining;

    // If stack has more than needed, split off excess before consuming
    if (stackCount > toDeliver) {
        int excessIdx = SplitStack(itemIdx, stackCount - toDeliver);
        if (excessIdx >= 0) {
            // Drop excess on the ground at the blueprint location
            items[excessIdx].state = ITEM_ON_GROUND;
            items[excessIdx].reservedBy = -1;
        }
    }

    sd->deliveredCount += toDeliver;
    if (sd->reservedCount > 0) sd->reservedCount--;
    sd->deliveredMaterial = mat;

    // Lock alternative on first delivery if not already locked
    if (sd->chosenAlternative < 0 && !stage->inputs[targetSlot].anyBuildingMat) {
        for (int a = 0; a < stage->inputs[targetSlot].altCount; a++) {
            if (stage->inputs[targetSlot].alternatives[a].itemType == deliveredType) {
                sd->chosenAlternative = a;
                break;
            }
        }
    }

    // Consume the delivered portion
    DeleteItem(itemIdx);

    // Check if all slots for current stage are filled
    if (BlueprintStageFilled(bp)) {
        bp->state = BLUEPRINT_READY_TO_BUILD;
        EventLog("Blueprint %d at (%d,%d,z%d) -> READY_TO_BUILD (stage %d filled)",
                 blueprintIdx, bp->x, bp->y, bp->z, bp->stage);
    }
}

bool BlueprintStageFilled(const Blueprint* bp) {
    const ConstructionRecipe* recipe = GetConstructionRecipe(bp->recipeIndex);
    if (!recipe) return false;
    const ConstructionStage* stage = &recipe->stages[bp->stage];
    for (int s = 0; s < stage->inputCount; s++) {
        if (bp->stageDeliveries[s].deliveredCount < stage->inputs[s].count) return false;
    }
    return true;
}

int BlueprintStageRequiredCount(const Blueprint* bp) {
    const ConstructionRecipe* recipe = GetConstructionRecipe(bp->recipeIndex);
    if (!recipe) return 0;
    const ConstructionStage* stage = &recipe->stages[bp->stage];
    int total = 0;
    for (int s = 0; s < stage->inputCount; s++) {
        total += stage->inputs[s].count;
    }
    return total;
}

int BlueprintStageDeliveredCount(const Blueprint* bp) {
    const ConstructionRecipe* recipe = GetConstructionRecipe(bp->recipeIndex);
    if (!recipe) return 0;
    const ConstructionStage* stage = &recipe->stages[bp->stage];
    int total = 0;
    for (int s = 0; s < stage->inputCount; s++) {
        total += bp->stageDeliveries[s].deliveredCount;
    }
    return total;
}

// Helper: determine final material for a recipe blueprint
static MaterialType GetRecipeFinalMaterial(const Blueprint* bp, const ConstructionRecipe* recipe) {
    if (recipe->resultMaterial != MAT_NONE) {
        return recipe->resultMaterial;  // fixed material (e.g. MAT_BRICK)
    }
    if (recipe->materialFromStage >= 0 && recipe->materialFromSlot >= 0) {
        int st = recipe->materialFromStage;
        int sl = recipe->materialFromSlot;
        // If this is the current stage, read from stageDeliveries
        if (st == bp->stage) {
            return bp->stageDeliveries[sl].deliveredMaterial;
        }
        // Otherwise read from consumedItems (completed stages)
        if (st < bp->stage) {
            return bp->consumedItems[st][sl].material;
        }
    }
    return MAT_NONE;  // fallback
}

// Helper: determine final source item for a recipe blueprint
static ItemType GetRecipeFinalSourceItem(const Blueprint* bp, const ConstructionRecipe* recipe) {
    if (recipe->materialFromStage >= 0 && recipe->materialFromSlot >= 0) {
        int st = recipe->materialFromStage;
        int sl = recipe->materialFromSlot;
        if (st == bp->stage) {
            if (bp->stageDeliveries[sl].chosenAlternative >= 0) {
                return recipe->stages[st].inputs[sl].alternatives[bp->stageDeliveries[sl].chosenAlternative].itemType;
            }
        }
        if (st < bp->stage) {
            return bp->consumedItems[st][sl].itemType;
        }
    }
    return ITEM_NONE;  // fallback
}

void CompleteBlueprint(int blueprintIdx) {
    if (blueprintIdx < 0 || blueprintIdx >= MAX_BLUEPRINTS) return;
    
    Blueprint* bp = &blueprints[blueprintIdx];
    if (!bp->active) return;
    
    int x = bp->x, y = bp->y, z = bp->z;

    const ConstructionRecipe* recipe = GetConstructionRecipe(bp->recipeIndex);
    if (!recipe) { bp->active = false; blueprintCount--; return; }

    // Record consumed items for current stage before advancing
    const ConstructionStage* stage = &recipe->stages[bp->stage];
    for (int s = 0; s < stage->inputCount; s++) {
        ConsumedRecord* cr = &bp->consumedItems[bp->stage][s];
        StageDelivery* sd = &bp->stageDeliveries[s];
        cr->count = sd->deliveredCount;
        cr->material = sd->deliveredMaterial;
        if (sd->chosenAlternative >= 0 && sd->chosenAlternative < stage->inputs[s].altCount) {
            cr->itemType = stage->inputs[s].alternatives[sd->chosenAlternative].itemType;
        } else if (stage->inputs[s].altCount > 0) {
            cr->itemType = stage->inputs[s].alternatives[0].itemType;
        }
    }

    // Check if this is the final stage
    if (bp->stage + 1 < recipe->stageCount) {
        // Advance to next stage
        bp->stage++;
        for (int s = 0; s < MAX_INPUTS_PER_STAGE; s++) {
            bp->stageDeliveries[s].deliveredCount = 0;
            bp->stageDeliveries[s].reservedCount = 0;
            bp->stageDeliveries[s].chosenAlternative = -1;
            bp->stageDeliveries[s].deliveredMaterial = MAT_NONE;
        }
        bp->state = BLUEPRINT_AWAITING_MATERIALS;
        bp->assignedBuilder = -1;
        bp->progress = 0.0f;
        EventLog("Blueprint %d at (%d,%d,z%d) advanced to stage %d -> AWAITING_MATERIALS",
                 (int)(bp - blueprints), bp->x, bp->y, bp->z, bp->stage);
        return;  // don't deactivate — more stages to go
    }

    // Final stage done — place the result
    MaterialType finalMat = GetRecipeFinalMaterial(bp, recipe);
    ItemType finalSource = GetRecipeFinalSourceItem(bp, recipe);

    if (recipe->buildCategory == BUILD_WALL) {
        PushMoversOutOfCell(x, y, z);
        PushItemsOutOfCell(x, y, z);
        if (IsCellWalkableAt(z, y, x)) {
            ClearCellCleanup(x, y, z);
            DisplaceWater(x, y, z);
            if (finalMat == MAT_DIRT) {
                grid[z][y][x] = CELL_WALL;
                SetWallMaterial(x, y, z, MAT_DIRT);
                SetWallSourceItem(x, y, z, ITEM_DIRT);
                SetWallNatural(x, y, z);
                SetWallFinish(x, y, z, FINISH_ROUGH);
                CLEAR_FLOOR(x, y, z);
                SetFloorMaterial(x, y, z, MAT_NONE);
                ClearFloorNatural(x, y, z);
                SetFloorFinish(x, y, z, FINISH_ROUGH);
                SET_CELL_SURFACE(x, y, z, SURFACE_BARE);
            } else {
                grid[z][y][x] = CELL_WALL;
                SetWallMaterial(x, y, z, finalMat);
                SetWallSourceItem(x, y, z, finalSource);
                ClearWallNatural(x, y, z);
                SetWallFinish(x, y, z, FINISH_SMOOTH);
            }
            MarkChunkDirty(x, y, z);
            InvalidatePathsThroughCell(x, y, z);
        }
    }
    if (recipe->buildCategory == BUILD_FLOOR) {
        ClearCellCleanup(x, y, z);
        DisplaceWater(x, y, z);
        grid[z][y][x] = CELL_AIR;
        SET_FLOOR(x, y, z);
        SET_CELL_SURFACE(x, y, z, SURFACE_BARE);
        SetFloorMaterial(x, y, z, finalMat);
        SetFloorSourceItem(x, y, z, finalSource);
        ClearFloorNatural(x, y, z);
        SetFloorFinish(x, y, z, FINISH_SMOOTH);
        MarkChunkDirty(x, y, z);
    }

    if (recipe->buildCategory == BUILD_LADDER) {
        PlaceLadder(x, y, z);
        SetWallMaterial(x, y, z, finalMat);
        SetWallSourceItem(x, y, z, finalSource);
        ClearWallNatural(x, y, z);
        SetWallFinish(x, y, z, FINISH_SMOOTH);
    }

    if (recipe->buildCategory == BUILD_RAMP) {
        PushItemsOutOfCell(x, y, z);
        DisplaceWater(x, y, z);

        // Use same auto-detect logic as dig ramp (points toward wall)
        CellType rampType = AutoDetectRampDirection(x, y, z);
        if (rampType == CELL_AIR) {
            rampType = CELL_RAMP_N;  // Fallback if no valid direction
        }

        grid[z][y][x] = rampType;
        rampCount++;
        CLEAR_FLOOR(x, y, z);
        SetWallMaterial(x, y, z, finalMat);
        SetWallSourceItem(x, y, z, finalSource);
        ClearWallNatural(x, y, z);
        SetWallFinish(x, y, z, FINISH_SMOOTH);

        MarkChunkDirty(x, y, z);
        if (z + 1 < gridDepth) MarkChunkDirty(x, y, z + 1);
    }

    if (recipe->buildCategory == BUILD_DOOR) {
        PushItemsOutOfCell(x, y, z);
        ClearCellCleanup(x, y, z);
        DisplaceWater(x, y, z);
        grid[z][y][x] = CELL_DOOR;
        SetWallMaterial(x, y, z, finalMat);
        SetWallSourceItem(x, y, z, finalSource);
        ClearWallNatural(x, y, z);
        SetWallFinish(x, y, z, FINISH_SMOOTH);
        CLEAR_FLOOR(x, y, z);
        MarkChunkDirty(x, y, z);
        InvalidatePathsThroughCell(x, y, z);
    }

    if (recipe->buildCategory == BUILD_FURNITURE) {
        FurnitureType ft = FURNITURE_NONE;
        if (bp->recipeIndex == CONSTRUCTION_LEAF_PILE) ft = FURNITURE_LEAF_PILE;
        else if (bp->recipeIndex == CONSTRUCTION_GRASS_PILE) ft = FURNITURE_GRASS_PILE;
        else if (bp->recipeIndex == CONSTRUCTION_PLANK_BED) ft = FURNITURE_PLANK_BED;
        else if (bp->recipeIndex == CONSTRUCTION_CHAIR) ft = FURNITURE_CHAIR;
        if (ft != FURNITURE_NONE) {
            SpawnFurniture(x, y, z, ft, (uint8_t)finalMat);
        }
        bp->active = false;
        bp->assignedBuilder = -1;
        blueprintCount--;
        return;
    }

    if (recipe->buildCategory == BUILD_WORKSHOP) {
        int wsIdx = CreateWorkshop(bp->workshopOriginX, bp->workshopOriginY, bp->z,
                                   (WorkshopType)bp->workshopType);
        if (wsIdx >= 0) {
            EventLog("Workshop %d (%s) constructed at (%d,%d,z%d)",
                     wsIdx, workshopDefs[bp->workshopType].displayName,
                     bp->workshopOriginX, bp->workshopOriginY, bp->z);
        }
        bp->active = false;
        bp->assignedBuilder = -1;
        blueprintCount--;
        return;
    }

    bp->active = false;
    bp->assignedBuilder = -1;
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
