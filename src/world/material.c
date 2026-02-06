#include "material.h"
#include "cell_defs.h"
#include <string.h>
#include <stdbool.h>

// Separate grids for wall and floor materials
uint8_t wallMaterial[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
uint8_t floorMaterial[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
uint8_t wallNatural[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
uint8_t floorNatural[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

MaterialDef materialDefs[MAT_COUNT] = {
    //                 name       spriteOffset  flags         fuel  dropsItem
    [MAT_NONE]    = {"none",     0,            0,            0,    ITEM_NONE},        // No material present
    [MAT_OAK]     = {"Oak",      1,            MF_FLAMMABLE, 128,  ITEM_LOG},
    [MAT_PINE]    = {"Pine",     1,            MF_FLAMMABLE, 128,  ITEM_LOG},
    [MAT_BIRCH]   = {"Birch",    1,            MF_FLAMMABLE, 128,  ITEM_LOG},
    [MAT_WILLOW]  = {"Willow",   1,            MF_FLAMMABLE, 128,  ITEM_LOG},
    [MAT_GRANITE] = {"Granite",  0,            0,            0,    ITEM_BLOCKS},
    [MAT_DIRT]    = {"Dirt",     0,            0,            0,    ITEM_DIRT},
    [MAT_BRICK]   = {"Brick",    0,            0,            0,    ITEM_BRICKS},
    [MAT_IRON]    = {"Iron",     2,            0,            0,    ITEM_BLOCKS}, // TODO: ITEM_IRON_BAR when added
    [MAT_GLASS]   = {"Glass",    3,            0,            0,    ITEM_BLOCKS}, // TODO: ITEM_GLASS when added
};

void InitMaterials(void) {
    // Natural terrain: walls default to granite and are marked natural
    memset(wallMaterial, MAT_GRANITE, sizeof(wallMaterial));
    memset(floorMaterial, MAT_NONE, sizeof(floorMaterial));
    memset(wallNatural, 1, sizeof(wallNatural));
    memset(floorNatural, 0, sizeof(floorNatural));
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
