#include "material.h"
#include "cell_defs.h"
#include <string.h>
#include <stdbool.h>

// Separate grids for wall and floor materials
uint8_t wallMaterial[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
uint8_t floorMaterial[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

MaterialDef materialDefs[MAT_COUNT] = {
    //                 name       spriteOffset  flags         fuel  dropsItem
    [MAT_NONE]    = {"none",     0,            0,            0,    ITEM_NONE},         // No material present
    [MAT_RAW]     = {"raw",      0,            0,            0,    ITEM_NONE},         // Use CellDropsItem instead
    [MAT_STONE]   = {"stone",    0,            0,            0,    ITEM_STONE_BLOCKS},
    [MAT_WOOD]    = {"wood",     1,            MF_FLAMMABLE, 128,  ITEM_WOOD},         // spriteOffset=1 for wood variant
    [MAT_DIRT]    = {"dirt",     0,            0,            0,    ITEM_DIRT},
    [MAT_IRON]    = {"iron",     2,            0,            0,    ITEM_STONE_BLOCKS}, // TODO: ITEM_IRON_BAR when added
    [MAT_GLASS]   = {"glass",    3,            0,            0,    ITEM_STONE_BLOCKS}, // TODO: ITEM_GLASS when added
};

void InitMaterials(void) {
    // Natural terrain: walls are MAT_RAW (natural rock), floors are MAT_NONE (no floor yet)
    memset(wallMaterial, MAT_RAW, sizeof(wallMaterial));
    memset(floorMaterial, MAT_NONE, sizeof(floorMaterial));
}

// Check if a wall is constructed (not raw natural rock)
bool IsConstructedWall(int x, int y, int z) {
    MaterialType mat = GetWallMaterial(x, y, z);
    return mat != MAT_NONE && mat != MAT_RAW;
}

// Get what item a wall drops based on its material
// For raw walls, use CellDropsItem. For constructed, use MaterialDropsItem.
ItemType GetWallDropItem(int x, int y, int z) {
    MaterialType mat = GetWallMaterial(x, y, z);
    
    if (mat == MAT_RAW || mat == MAT_NONE) {
        // Natural/raw wall - use cell type's default drop
        return CellDropsItem(grid[z][y][x]);
    }
    
    // Constructed wall - drop based on material
    return MaterialDropsItem(mat);
}

// Get what item a floor drops based on its material
ItemType GetFloorDropItem(int x, int y, int z) {
    MaterialType mat = GetFloorMaterial(x, y, z);
    
    if (mat == MAT_RAW || mat == MAT_NONE) {
        // Raw/no floor - nothing to drop
        return ITEM_NONE;
    }
    
    // Constructed floor - drop based on material
    return MaterialDropsItem(mat);
}
