#include "material.h"
#include "cell_defs.h"
#include "../../assets/atlas.h"
#include "../simulation/temperature.h"
#include "../simulation/trees.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

// Separate grids for wall and floor materials
uint8_t wallMaterial[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
uint8_t floorMaterial[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
uint8_t wallNatural[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
uint8_t floorNatural[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
uint8_t wallFinish[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
uint8_t floorFinish[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

MaterialDef materialDefs[MAT_COUNT] = {
    //                 name       sprite                    leavesSprite              saplingSprite              flags         fuel  ignRes  dropsItem     insul                  burnsInto
    [MAT_NONE]    = {"none",     0,                        0,                        0,                         0,            0,    0,      ITEM_NONE,    INSULATION_TIER_AIR,   MAT_NONE},
    [MAT_OAK]     = {"Oak",      SPRITE_tree_trunk_oak,    SPRITE_tree_leaves_oak,   SPRITE_tree_sapling_oak,   MF_FLAMMABLE, 128,  50,     ITEM_LOG,     INSULATION_TIER_WOOD,  MAT_NONE},
    [MAT_PINE]    = {"Pine",     SPRITE_tree_trunk_pine,   SPRITE_tree_leaves_pine,  SPRITE_tree_sapling_pine,  MF_FLAMMABLE, 96,   30,     ITEM_LOG,     INSULATION_TIER_WOOD,  MAT_NONE},
    [MAT_BIRCH]   = {"Birch",    SPRITE_tree_trunk_birch,  SPRITE_tree_leaves_birch, SPRITE_tree_sapling_birch, MF_FLAMMABLE, 112,  40,     ITEM_LOG,     INSULATION_TIER_WOOD,  MAT_NONE},
    [MAT_WILLOW]  = {"Willow",   SPRITE_tree_trunk_willow, SPRITE_tree_leaves_willow,SPRITE_tree_sapling_willow,MF_FLAMMABLE, 80,   25,     ITEM_LOG,     INSULATION_TIER_WOOD,  MAT_NONE},
    [MAT_GRANITE] = {"Granite",  SPRITE_rock,              0,                        0,                         0,            0,    0,      ITEM_BLOCKS,  INSULATION_TIER_STONE, MAT_GRANITE},
    [MAT_DIRT]    = {"Dirt",     SPRITE_dirt,              0,                        0,                         0,            1,    0,      ITEM_DIRT,    INSULATION_TIER_AIR,   MAT_DIRT},
    [MAT_BRICK]   = {"Brick",    0,                        0,                        0,                         0,            0,    0,      ITEM_BRICKS,  INSULATION_TIER_STONE, MAT_BRICK},
    [MAT_IRON]    = {"Iron",     0,                        0,                        0,                         0,            0,    0,      ITEM_BLOCKS,  INSULATION_TIER_STONE, MAT_IRON},
    [MAT_GLASS]   = {"Glass",    0,                        0,                        0,                         0,            0,    0,      ITEM_BLOCKS,  INSULATION_TIER_STONE, MAT_GLASS},
    [MAT_CLAY]    = {"Clay",     SPRITE_clay,              0,                        0,                         0,            0,    0,      ITEM_CLAY,    INSULATION_TIER_AIR,   MAT_CLAY},
    [MAT_GRAVEL]  = {"Gravel",   SPRITE_gravel,            0,                        0,                         0,            0,    0,      ITEM_GRAVEL,  INSULATION_TIER_AIR,   MAT_GRAVEL},
    [MAT_SAND]    = {"Sand",     SPRITE_sand,              0,                        0,                         0,            0,    0,      ITEM_SAND,    INSULATION_TIER_AIR,   MAT_SAND},
    [MAT_PEAT]    = {"Peat",     SPRITE_peat,              0,                        0,                         0,            6,    0,      ITEM_PEAT,    INSULATION_TIER_AIR,   MAT_DIRT},
    [MAT_BEDROCK] = {"Bedrock",  SPRITE_bedrock,           0,                        0,                         MF_UNMINEABLE,0,    0,      ITEM_NONE,    INSULATION_TIER_STONE, MAT_BEDROCK},
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



void SyncMaterialsToTerrain(void) {
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                CellType cell = grid[z][y][x];
                MaterialType mat = GetWallMaterial(x, y, z);
                
                // If material is already set, keep it (from terrain generation)
                if (mat != MAT_NONE) continue;
                
                // Other solid cells (CELL_WALL etc.) default to granite
                if (CellIsSolid(cell)) {
                    SetWallMaterial(x, y, z, MAT_GRANITE);
                    SetWallNatural(x, y, z);
                    SetWallFinish(x, y, z, FINISH_ROUGH);
                }
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

// =============================================================================
// Position-aware helpers
// =============================================================================

int GetCellSpriteAt(int x, int y, int z) {
    CellType cell = grid[z][y][x];
    MaterialType mat = GetWallMaterial(x, y, z);

    // Constructed walls: use material-driven sprite
    if (cell == CELL_WALL && mat != MAT_NONE) {
        if (IsWallNatural(x, y, z)) {
            return SPRITE_rock;
        }
        int sprite = MaterialSprite(mat);
        return sprite ? sprite : CellSprite(cell);
    }

    // Tree cells: use wallMaterial for species
    if (cell == CELL_TREE_TRUNK || cell == CELL_TREE_BRANCH || cell == CELL_TREE_ROOT || 
        cell == CELL_TREE_FELLED || cell == CELL_TREE_LEAVES || cell == CELL_SAPLING) {
        if (mat != MAT_NONE) {
            int sprite = 0;
            if (cell == CELL_TREE_TRUNK || cell == CELL_TREE_BRANCH || cell == CELL_TREE_ROOT || cell == CELL_TREE_FELLED) {
                sprite = MaterialSprite(mat);
            } else if (cell == CELL_TREE_LEAVES) {
                sprite = MaterialLeavesSprite(mat);
            } else if (cell == CELL_SAPLING) {
                sprite = MaterialSaplingSprite(mat);
            }
            if (sprite) return sprite;
        }
    }

    // CELL_TERRAIN: use material-driven sprite
    if (cell == CELL_TERRAIN && mat != MAT_NONE) {
        int sprite = MaterialSprite(mat);
        if (sprite) return sprite;
    }

    // Other cells (including old soil types): use cellDefs sprite
    return CellSprite(cell);
}

int GetInsulationAt(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return INSULATION_TIER_STONE;  // Out of bounds = solid stone
    }

    MaterialType mat = GetWallMaterial(x, y, z);
    if (mat != MAT_NONE) {
        return MaterialInsulationTier(mat);
    }

    // Fall back to cell-based insulation
    return CellInsulationTier(grid[z][y][x]);
}

const char* GetCellNameAt(int x, int y, int z) {
    static char nameBuf[64];
    CellType cell = grid[z][y][x];
    MaterialType mat = GetWallMaterial(x, y, z);

    // Walls: material name
    if (cell == CELL_WALL && mat != MAT_NONE) {
        return MaterialName(mat);
    }

    // CELL_TERRAIN: material name (e.g. "Dirt", "Granite")
    if (cell == CELL_TERRAIN && mat != MAT_NONE) {
        return MaterialName(mat);
    }

    // Tree cells: "Oak tree trunk", "Pine tree leaves", etc.
    if (cell == CELL_TREE_TRUNK || cell == CELL_TREE_BRANCH || cell == CELL_TREE_ROOT ||
        cell == CELL_TREE_FELLED || cell == CELL_TREE_LEAVES || cell == CELL_SAPLING) {
        if (mat != MAT_NONE) {
            snprintf(nameBuf, sizeof(nameBuf), "%s %s",
                     MaterialName(mat), CellName(cell));
            return nameBuf;
        }
    }

    return CellName(cell);
}
