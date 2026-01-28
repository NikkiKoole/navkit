#include "designations.h"
#include "grid.h"
#include "cell_defs.h"
#include "pathfinding.h"
#include "../entities/items.h"
#include "../entities/mover.h"  // for CELL_SIZE
#include <string.h>
#include <math.h>

Designation designations[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

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
    
    // Clear blueprints
    memset(blueprints, 0, sizeof(blueprints));
    for (int i = 0; i < MAX_BLUEPRINTS; i++) {
        blueprints[i].assignedBuilder = -1;
        blueprints[i].reservedItem = -1;
    }
    blueprintCount = 0;
}

bool DesignateDig(int x, int y, int z) {
    // Bounds check
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    
    // Can only dig walls
    if (grid[z][y][x] != CELL_WALL) {
        return false;
    }
    
    // Already designated?
    if (designations[z][y][x].type == DESIGNATION_DIG) {
        return false;
    }
    
    designations[z][y][x].type = DESIGNATION_DIG;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
    
    return true;
}

void CancelDesignation(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return;
    }
    
    designations[z][y][x].type = DESIGNATION_NONE;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
}

bool HasDigDesignation(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return false;
    }
    return designations[z][y][x].type == DESIGNATION_DIG;
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

bool FindUnassignedDigDesignation(int* outX, int* outY, int* outZ) {
    // Simple linear scan - could be optimized with a list later
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                Designation* d = &designations[z][y][x];
                if (d->type == DESIGNATION_DIG && d->assignedMover == -1) {
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

void CompleteDigDesignation(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return;
    }
    
    // Convert wall to floor
    if (grid[z][y][x] == CELL_WALL) {
        grid[z][y][x] = CELL_FLOOR;
        MarkChunkDirty(x, y, z);
        
        // Spawn an orange block (stone) from the mined wall
        SpawnItem(x * CELL_SIZE + CELL_SIZE * 0.5f, y * CELL_SIZE + CELL_SIZE * 0.5f, (float)z, ITEM_ORANGE);
    }
    
    // Clear designation
    designations[z][y][x].type = DESIGNATION_NONE;
    designations[z][y][x].assignedMover = -1;
    designations[z][y][x].progress = 0.0f;
}

int CountDigDesignations(void) {
    int count = 0;
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (designations[z][y][x].type == DESIGNATION_DIG) {
                    count++;
                }
            }
        }
    }
    return count;
}

// Tick down unreachable cooldowns for designations
void DesignationsTick(float dt) {
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
    
    // Convert floor to wall
    if (IsCellWalkableAt(bp->z, bp->y, bp->x)) {
        grid[bp->z][bp->y][bp->x] = CELL_WALL;
        MarkChunkDirty(bp->x, bp->y, bp->z);
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
