#include "material.h"
#include "cell_defs.h"
#include <string.h>
#include <stdbool.h>

// Separate grids for wall and floor materials
uint8_t wallMaterial[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
uint8_t floorMaterial[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
uint8_t wallNatural[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
uint8_t floorNatural[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
uint8_t wallFinish[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
uint8_t floorFinish[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

MaterialDef materialDefs[MAT_COUNT] = {
    //                 name       spriteOffset  flags         fuel  ignRes  dropsItem
    [MAT_NONE]    = {"none",     0,            0,            0,    0,      ITEM_NONE},
    [MAT_OAK]     = {"Oak",      1,            MF_FLAMMABLE, 128,  50,     ITEM_LOG},
    [MAT_PINE]    = {"Pine",     1,            MF_FLAMMABLE, 96,   30,     ITEM_LOG},
    [MAT_BIRCH]   = {"Birch",    1,            MF_FLAMMABLE, 112,  40,     ITEM_LOG},
    [MAT_WILLOW]  = {"Willow",   1,            MF_FLAMMABLE, 80,   25,     ITEM_LOG},
    [MAT_GRANITE] = {"Granite",  0,            0,            0,    0,      ITEM_BLOCKS},
    [MAT_DIRT]    = {"Dirt",     0,            0,            0,    0,      ITEM_DIRT},
    [MAT_BRICK]   = {"Brick",    0,            0,            0,    0,      ITEM_BRICKS},
    [MAT_IRON]    = {"Iron",     2,            0,            0,    0,      ITEM_BLOCKS},
    [MAT_GLASS]   = {"Glass",    3,            0,            0,    0,      ITEM_BLOCKS},
    [MAT_CLAY]    = {"Clay",     0,            0,            0,    0,      ITEM_CLAY},
    [MAT_GRAVEL]  = {"Gravel",   0,            0,            0,    0,      ITEM_GRAVEL},
    [MAT_SAND]    = {"Sand",     0,            0,            0,    0,      ITEM_SAND},
    [MAT_PEAT]    = {"Peat",     0,            0,            6,    0,      ITEM_PEAT},
};

void InitMaterials(void) {
    // All cells start with no material — SyncMaterialsToTerrain sets correct
    // materials after terrain generation. This avoids air cells having MAT_GRANITE.
    memset(wallMaterial, MAT_NONE, sizeof(wallMaterial));
    memset(floorMaterial, MAT_NONE, sizeof(floorMaterial));
    memset(wallNatural, 0, sizeof(wallNatural));
    memset(floorNatural, 0, sizeof(floorNatural));
    memset(wallFinish, FINISH_ROUGH, sizeof(wallFinish));
    memset(floorFinish, FINISH_ROUGH, sizeof(floorFinish));
}

MaterialType MaterialForGroundCell(CellType cell) {
    switch (cell) {
        case CELL_DIRT: return MAT_DIRT;
        case CELL_CLAY: return MAT_CLAY;
        case CELL_GRAVEL: return MAT_GRAVEL;
        case CELL_SAND: return MAT_SAND;
        case CELL_PEAT: return MAT_PEAT;
        case CELL_ROCK: return MAT_GRANITE;
        default: return MAT_NONE;
    }
}

void SyncMaterialsToTerrain(void) {
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                CellType cell = grid[z][y][x];
                MaterialType mat = MaterialForGroundCell(cell);
                // Ground cells get their specific material
                // Other solid cells (CELL_WALL etc.) default to granite
                if (mat == MAT_NONE && CellIsSolid(cell)) {
                    mat = MAT_GRANITE;
                }
                if (mat == MAT_NONE) continue;
                SetWallMaterial(x, y, z, mat);
                SetWallNatural(x, y, z);
                SetWallFinish(x, y, z, FINISH_ROUGH);
            }
        }
    }
}

// Check if a wall is constructed (not natural terrain)
bool IsConstructedWall(int x, int y, int z) {
    MaterialType mat = GetWallMaterial(x, y, z);
    return CellBlocksMovement(grid[z][y][x]) && mat != MAT_NONE && !IsWallNatural(x, y, z);
}

// Get what item a wall drops based on its material
// For natural walls, use CellDropsItem. For constructed, use MaterialDropsItem.
ItemType GetWallDropItem(int x, int y, int z) {
    MaterialType mat = GetWallMaterial(x, y, z);
    
    if (!CellBlocksMovement(grid[z][y][x]) || mat == MAT_NONE) {
        return ITEM_NONE;
    }

    if (IsWallNatural(x, y, z)) {
        // Natural wall - use cell type's default drop
        return CellDropsItem(grid[z][y][x]);
    }
    
    // Constructed wall - drop based on material
    return MaterialDropsItem(mat);
}

// Get what item a floor drops based on its material
ItemType GetFloorDropItem(int x, int y, int z) {
    MaterialType mat = GetFloorMaterial(x, y, z);
    
    if (mat == MAT_NONE || IsFloorNatural(x, y, z)) {
        // Natural/no floor - nothing to drop
        return ITEM_NONE;
    }
    
    // Constructed floor - drop based on material
    return MaterialDropsItem(mat);
}
