#include "designations.h"
#include "grid.h"
#include "pathfinding.h"
#include <string.h>

Designation designations[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

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
