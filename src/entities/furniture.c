#include "furniture.h"
#include "mover.h"
#include "../world/pathfinding.h"
#include <string.h>

Furniture furniture[MAX_FURNITURE];
int furnitureCount = 0;
uint8_t furnitureMoveCostGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

static const FurnitureDef furnitureDefs[FURNITURE_TYPE_COUNT] = {
    [FURNITURE_NONE]       = { "None",       0.0f,   false, 0  },
    [FURNITURE_LEAF_PILE]  = { "Leaf Pile",  0.020f, false, 12 },
    [FURNITURE_PLANK_BED]  = { "Plank Bed",  0.040f, true,  0  },
    [FURNITURE_CHAIR]      = { "Chair",      0.015f, false, 11 },
};

_Static_assert(sizeof(furnitureDefs) / sizeof(furnitureDefs[0]) == FURNITURE_TYPE_COUNT,
               "furnitureDefs[] must have an entry for every FurnitureType");

const FurnitureDef* GetFurnitureDef(FurnitureType type) {
    if (type < 0 || type >= FURNITURE_TYPE_COUNT) return &furnitureDefs[FURNITURE_NONE];
    return &furnitureDefs[type];
}

void ClearFurniture(void) {
    memset(furniture, 0, sizeof(furniture));
    for (int i = 0; i < MAX_FURNITURE; i++) {
        furniture[i].occupant = -1;
    }
    furnitureCount = 0;
    memset(furnitureMoveCostGrid, 0, sizeof(furnitureMoveCostGrid));
}

int SpawnFurniture(int x, int y, int z, FurnitureType type, uint8_t material) {
    if (type <= FURNITURE_NONE || type >= FURNITURE_TYPE_COUNT) return -1;
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return -1;

    // Check no existing furniture at this cell
    if (GetFurnitureAt(x, y, z) >= 0) return -1;

    // Find empty slot
    int idx = -1;
    for (int i = 0; i < MAX_FURNITURE; i++) {
        if (!furniture[i].active) {
            idx = i;
            break;
        }
    }
    if (idx < 0) return -1;

    Furniture* f = &furniture[idx];
    f->x = x;
    f->y = y;
    f->z = z;
    f->active = true;
    f->type = type;
    f->material = material;
    f->occupant = -1;
    furnitureCount++;

    const FurnitureDef* def = &furnitureDefs[type];
    if (def->blocking) {
        SET_CELL_FLAG(x, y, z, CELL_FLAG_WORKSHOP_BLOCK);
        PushMoversOutOfCell(x, y, z);
        InvalidatePathsThroughCell(x, y, z);
        MarkChunkDirty(x, y, z);
    } else if (def->moveCost > 0) {
        furnitureMoveCostGrid[z][y][x] = (uint8_t)def->moveCost;
        InvalidatePathsThroughCell(x, y, z);
        MarkChunkDirty(x, y, z);
    }

    return idx;
}

void RemoveFurniture(int index) {
    if (index < 0 || index >= MAX_FURNITURE) return;
    Furniture* f = &furniture[index];
    if (!f->active) return;

    const FurnitureDef* def = &furnitureDefs[f->type];
    if (def->blocking) {
        CLEAR_CELL_FLAG(f->x, f->y, f->z, CELL_FLAG_WORKSHOP_BLOCK);
        InvalidatePathsThroughCell(f->x, f->y, f->z);
        MarkChunkDirty(f->x, f->y, f->z);
    } else if (def->moveCost > 0) {
        furnitureMoveCostGrid[f->z][f->y][f->x] = 0;
        InvalidatePathsThroughCell(f->x, f->y, f->z);
        MarkChunkDirty(f->x, f->y, f->z);
    }

    f->active = false;
    f->occupant = -1;
    furnitureCount--;
}

int GetFurnitureAt(int x, int y, int z) {
    for (int i = 0; i < MAX_FURNITURE; i++) {
        if (!furniture[i].active) continue;
        if (furniture[i].x == x && furniture[i].y == y && furniture[i].z == z) {
            return i;
        }
    }
    return -1;
}

void ReleaseFurniture(int furnitureIdx, int moverIdx) {
    if (furnitureIdx >= 0 && furnitureIdx < MAX_FURNITURE &&
        furniture[furnitureIdx].occupant == moverIdx) {
        furniture[furnitureIdx].occupant = -1;
    }
}

void ReleaseFurnitureForMover(int moverIdx) {
    for (int i = 0; i < MAX_FURNITURE; i++) {
        if (furniture[i].active && furniture[i].occupant == moverIdx) {
            furniture[i].occupant = -1;
        }
    }
}

void RebuildFurnitureMoveCostGrid(void) {
    memset(furnitureMoveCostGrid, 0, sizeof(furnitureMoveCostGrid));
    for (int i = 0; i < MAX_FURNITURE; i++) {
        if (!furniture[i].active) continue;
        const FurnitureDef* def = &furnitureDefs[furniture[i].type];
        if (!def->blocking && def->moveCost > 0) {
            furnitureMoveCostGrid[furniture[i].z][furniture[i].y][furniture[i].x] = (uint8_t)def->moveCost;
        }
    }
}
