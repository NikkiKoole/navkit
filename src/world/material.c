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
    //                 name       sprOff  flags         fuel  ignRes  dropsItem       terrSprite              trunkSprite                 leavesSprite                saplingSprite                insul                  burnsInto
    [MAT_NONE]    = {"none",     0,      0,            0,    0,      ITEM_NONE,      0,                      0,                          0,                          0,                           INSULATION_TIER_AIR,   MAT_NONE},
    [MAT_OAK]     = {"Oak",      1,      MF_FLAMMABLE, 128,  50,     ITEM_LOG,       0,                      SPRITE_tree_trunk_oak,      SPRITE_tree_leaves_oak,     SPRITE_tree_sapling_oak,     INSULATION_TIER_WOOD,  MAT_NONE},
    [MAT_PINE]    = {"Pine",     1,      MF_FLAMMABLE, 96,   30,     ITEM_LOG,       0,                      SPRITE_tree_trunk_pine,     SPRITE_tree_leaves_pine,    SPRITE_tree_sapling_pine,    INSULATION_TIER_WOOD,  MAT_NONE},
    [MAT_BIRCH]   = {"Birch",    1,      MF_FLAMMABLE, 112,  40,     ITEM_LOG,       0,                      SPRITE_tree_trunk_birch,    SPRITE_tree_leaves_birch,   SPRITE_tree_sapling_birch,   INSULATION_TIER_WOOD,  MAT_NONE},
    [MAT_WILLOW]  = {"Willow",   1,      MF_FLAMMABLE, 80,   25,     ITEM_LOG,       0,                      SPRITE_tree_trunk_willow,   SPRITE_tree_leaves_willow,  SPRITE_tree_sapling_willow,  INSULATION_TIER_WOOD,  MAT_NONE},
    [MAT_GRANITE] = {"Granite",  0,      0,            0,    0,      ITEM_BLOCKS,    SPRITE_rock,            0,                          0,                          0,                           INSULATION_TIER_STONE, MAT_GRANITE},
    [MAT_DIRT]    = {"Dirt",     0,      0,            1,    0,      ITEM_DIRT,      SPRITE_dirt,            0,                          0,                          0,                           INSULATION_TIER_AIR,   MAT_DIRT},
    [MAT_BRICK]   = {"Brick",    0,      0,            0,    0,      ITEM_BRICKS,    0,                      0,                          0,                          0,                           INSULATION_TIER_STONE, MAT_BRICK},
    [MAT_IRON]    = {"Iron",     2,      0,            0,    0,      ITEM_BLOCKS,    0,                      0,                          0,                          0,                           INSULATION_TIER_STONE, MAT_IRON},
    [MAT_GLASS]   = {"Glass",    3,      0,            0,    0,      ITEM_BLOCKS,    0,                      0,                          0,                          0,                           INSULATION_TIER_STONE, MAT_GLASS},
    [MAT_CLAY]    = {"Clay",     0,      0,            0,    0,      ITEM_CLAY,      SPRITE_clay,            0,                          0,                          0,                           INSULATION_TIER_AIR,   MAT_CLAY},
    [MAT_GRAVEL]  = {"Gravel",   0,      0,            0,    0,      ITEM_GRAVEL,    SPRITE_gravel,          0,                          0,                          0,                           INSULATION_TIER_AIR,   MAT_GRAVEL},
    [MAT_SAND]    = {"Sand",     0,      0,            0,    0,      ITEM_SAND,      SPRITE_sand,            0,                          0,                          0,                           INSULATION_TIER_AIR,   MAT_SAND},
    [MAT_PEAT]    = {"Peat",     0,      0,            6,    0,      ITEM_PEAT,      SPRITE_peat,            0,                          0,                          0,                           INSULATION_TIER_AIR,   MAT_DIRT},
    [MAT_BEDROCK] = {"Bedrock",  0,      MF_UNMINEABLE,0,    0,      ITEM_NONE,      SPRITE_bedrock,         0,                          0,                          0,                           INSULATION_TIER_STONE, MAT_BEDROCK},
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
        int sprite = MaterialTerrainSprite(mat);
        return sprite ? sprite : CellSprite(cell);
    }

    // Tree cells: check wallMaterial first, fall back to treeTypeGrid
    if (cell == CELL_TREE_TRUNK || cell == CELL_TREE_LEAVES || cell == CELL_SAPLING) {
        if (mat != MAT_NONE) {
            int sprite = 0;
            if (cell == CELL_TREE_TRUNK) sprite = MaterialTreeTrunkSprite(mat);
            else if (cell == CELL_TREE_LEAVES) sprite = MaterialTreeLeavesSprite(mat);
            else if (cell == CELL_SAPLING) sprite = MaterialTreeSaplingSprite(mat);
            if (sprite) return sprite;
        }
        // Fall back to treeTypeGrid (backward compat until treeTypeGrid is removed)
        TreeType type = (TreeType)treeTypeGrid[z][y][x];
        if (type > TREE_TYPE_NONE && type < TREE_TYPE_COUNT) {
            MaterialType treeMat = MaterialFromTreeType(type);
            int sprite = 0;
            if (cell == CELL_TREE_TRUNK) sprite = MaterialTreeTrunkSprite(treeMat);
            else if (cell == CELL_TREE_LEAVES) sprite = MaterialTreeLeavesSprite(treeMat);
            else if (cell == CELL_SAPLING) sprite = MaterialTreeSaplingSprite(treeMat);
            if (sprite) return sprite;
        }
    }

    // CELL_TERRAIN: use material-driven sprite
    if (cell == CELL_TERRAIN && mat != MAT_NONE) {
        int sprite = MaterialTerrainSprite(mat);
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
    if (cell == CELL_TREE_TRUNK || cell == CELL_TREE_LEAVES || cell == CELL_SAPLING) {
        MaterialType treeMat = MAT_NONE;
        if (mat != MAT_NONE) {
            treeMat = mat;
        } else {
            TreeType type = (TreeType)treeTypeGrid[z][y][x];
            if (type > TREE_TYPE_NONE && type < TREE_TYPE_COUNT) {
                treeMat = MaterialFromTreeType(type);
            }
        }
        if (treeMat != MAT_NONE) {
            snprintf(nameBuf, sizeof(nameBuf), "%s %s",
                     MaterialName(treeMat), CellName(cell));
            return nameBuf;
        }
    }

    return CellName(cell);
}
