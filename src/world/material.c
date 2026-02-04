#include "material.h"
#include "cell_defs.h"
#include <string.h>

uint8_t cellMaterial[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

MaterialDef materialDefs[MAT_COUNT] = {
    //                 name       spriteOffset  flags         fuel  dropsItem
    [MAT_NATURAL] = {"natural",  0,            0,            0,    ITEM_NONE},         // Use CellDropsItem instead
    [MAT_STONE]   = {"stone",    0,            0,            0,    ITEM_STONE_BLOCKS},
    [MAT_WOOD]    = {"wood",     1,            MF_FLAMMABLE, 128,  ITEM_WOOD},         // spriteOffset=1 for wood variant
    [MAT_IRON]    = {"iron",     2,            0,            0,    ITEM_STONE_BLOCKS}, // TODO: ITEM_IRON_BAR when added
    [MAT_GLASS]   = {"glass",    3,            0,            0,    ITEM_STONE_BLOCKS}, // TODO: ITEM_GLASS when added
};

void InitMaterials(void) {
    memset(cellMaterial, MAT_NATURAL, sizeof(cellMaterial));
}

// Get what item a cell drops based on its material
// For natural cells, use CellDropsItem. For constructed, use MaterialDropsItem.
ItemType GetCellDropItem(int x, int y, int z) {
    MaterialType mat = GetCellMaterial(x, y, z);
    
    if (mat == MAT_NATURAL) {
        // Natural cell - use cell type's default drop
        return CellDropsItem(grid[z][y][x]);
    }
    
    // Constructed cell - drop based on material
    return MaterialDropsItem(mat);
}
