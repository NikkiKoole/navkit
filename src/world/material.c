#include "material.h"
#include "cell_defs.h"
#include "../../assets/atlas.h"
#include "../simulation/temperature.h"
#include "../simulation/trees.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

// Sprite overrides table: (CellType, MaterialType) -> sprite
// Only populated for exceptions where the sprite is neither the material's
// canonical sprite nor the cell's default sprite.
static int spriteOverrides[CELL_TYPE_COUNT][MAT_COUNT];

// Separate grids for wall and floor materials
uint8_t wallMaterial[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
uint8_t floorMaterial[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
uint8_t wallNatural[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
uint8_t floorNatural[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
uint8_t wallFinish[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
uint8_t floorFinish[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
uint8_t wallSourceItem[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
uint8_t floorSourceItem[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

MaterialDef materialDefs[MAT_COUNT] = {
    //                 name       sprite                    flags         fuel  ignRes  dropsItem     insul                  burnsInto
    [MAT_NONE]    = {"none",     0,                        0,            0,    0,      ITEM_NONE,    INSULATION_TIER_AIR,   MAT_NONE},
    [MAT_OAK]     = {"Oak",      SPRITE_tree_trunk_oak,    MF_FLAMMABLE, 128,  50,     ITEM_LOG,     INSULATION_TIER_WOOD,  MAT_NONE},
    [MAT_PINE]    = {"Pine",     SPRITE_tree_trunk_pine,   MF_FLAMMABLE, 96,   30,     ITEM_LOG,     INSULATION_TIER_WOOD,  MAT_NONE},
    [MAT_BIRCH]   = {"Birch",    SPRITE_tree_trunk_birch,  MF_FLAMMABLE, 112,  40,     ITEM_LOG,     INSULATION_TIER_WOOD,  MAT_NONE},
    [MAT_WILLOW]  = {"Willow",   SPRITE_tree_trunk_willow, MF_FLAMMABLE, 80,   25,     ITEM_LOG,     INSULATION_TIER_WOOD,  MAT_NONE},
    [MAT_GRANITE]   = {"Granite",   SPRITE_granite,              0,            0,    0,      ITEM_ROCK,    INSULATION_TIER_STONE, MAT_GRANITE},
    [MAT_SANDSTONE] = {"Sandstone", SPRITE_sandstone,         0,            0,    0,      ITEM_ROCK,    INSULATION_TIER_STONE, MAT_SANDSTONE},
    [MAT_SLATE]     = {"Slate",     SPRITE_slate,             0,            0,    0,      ITEM_ROCK,    INSULATION_TIER_STONE, MAT_SLATE},
    [MAT_DIRT]    = {"Dirt",     SPRITE_dirt,              0,            1,    0,      ITEM_DIRT,    INSULATION_TIER_AIR,   MAT_DIRT},
    [MAT_BRICK]   = {"Brick",    0,                        0,            0,    0,      ITEM_BRICKS,  INSULATION_TIER_STONE, MAT_BRICK},
    [MAT_IRON]    = {"Iron",     0,                        0,            0,    0,      ITEM_BLOCKS,  INSULATION_TIER_STONE, MAT_IRON},
    [MAT_GLASS]   = {"Glass",    0,                        0,            0,    0,      ITEM_BLOCKS,  INSULATION_TIER_STONE, MAT_GLASS},
    [MAT_CLAY]    = {"Clay",     SPRITE_clay,              0,            0,    0,      ITEM_CLAY,    INSULATION_TIER_AIR,   MAT_CLAY},
    [MAT_GRAVEL]  = {"Gravel",   SPRITE_gravel,            0,            0,    0,      ITEM_GRAVEL,  INSULATION_TIER_AIR,   MAT_GRAVEL},
    [MAT_SAND]    = {"Sand",     SPRITE_sand,              0,            0,    0,      ITEM_SAND,    INSULATION_TIER_AIR,   MAT_SAND},
    [MAT_PEAT]    = {"Peat",     SPRITE_peat,              0,            6,    0,      ITEM_PEAT,    INSULATION_TIER_AIR,   MAT_DIRT},
    [MAT_BEDROCK] = {"Bedrock",  SPRITE_bedrock,           MF_UNMINEABLE,0,    0,      ITEM_NONE,    INSULATION_TIER_STONE, MAT_BEDROCK},
};

void InitSpriteOverrides(void) {
    memset(spriteOverrides, 0, sizeof(spriteOverrides));

    // Wood walls look different from wood's canonical (trunk) sprite
    spriteOverrides[CELL_WALL][MAT_OAK]    = SPRITE_wall_wood;
    spriteOverrides[CELL_WALL][MAT_PINE]   = SPRITE_wall_wood;
    spriteOverrides[CELL_WALL][MAT_BIRCH]  = SPRITE_wall_wood;
    spriteOverrides[CELL_WALL][MAT_WILLOW] = SPRITE_wall_wood;

    // Leaves look different from wood's canonical (trunk) sprite
    spriteOverrides[CELL_TREE_LEAVES][MAT_OAK]    = SPRITE_tree_leaves_oak;
    spriteOverrides[CELL_TREE_LEAVES][MAT_PINE]   = SPRITE_tree_leaves_pine;
    spriteOverrides[CELL_TREE_LEAVES][MAT_BIRCH]  = SPRITE_tree_leaves_birch;
    spriteOverrides[CELL_TREE_LEAVES][MAT_WILLOW] = SPRITE_tree_leaves_willow;

    // Saplings look different from wood's canonical (trunk) sprite
    spriteOverrides[CELL_SAPLING][MAT_OAK]    = SPRITE_tree_sapling_oak;
    spriteOverrides[CELL_SAPLING][MAT_PINE]   = SPRITE_tree_sapling_pine;
    spriteOverrides[CELL_SAPLING][MAT_BIRCH]  = SPRITE_tree_sapling_birch;
    spriteOverrides[CELL_SAPLING][MAT_WILLOW] = SPRITE_tree_sapling_willow;

    // Branches use thin trunk sprites
    spriteOverrides[CELL_TREE_BRANCH][MAT_OAK]    = SPRITE_tree_branch_oak;
    spriteOverrides[CELL_TREE_BRANCH][MAT_PINE]   = SPRITE_tree_branch_pine;
    spriteOverrides[CELL_TREE_BRANCH][MAT_BIRCH]  = SPRITE_tree_branch_birch;
    spriteOverrides[CELL_TREE_BRANCH][MAT_WILLOW] = SPRITE_tree_branch_willow;
}

int GetSpriteForCellMat(CellType cell, MaterialType mat) {
    int sprite = spriteOverrides[cell][mat];
    if (sprite) return sprite;

    if (mat != MAT_NONE) {
        sprite = MaterialSprite(mat);
        if (sprite) return sprite;
    }

    return CellSprite(cell);
}

void InitMaterials(void) {
    // All cells start with no material — SyncMaterialsToTerrain sets correct
    // materials after terrain generation. This avoids air cells having MAT_GRANITE.
    memset(wallMaterial, MAT_NONE, sizeof(wallMaterial));
    memset(floorMaterial, MAT_NONE, sizeof(floorMaterial));
    memset(wallNatural, 0, sizeof(wallNatural));
    memset(floorNatural, 0, sizeof(floorNatural));
    memset(wallFinish, FINISH_ROUGH, sizeof(wallFinish));
    memset(floorFinish, FINISH_ROUGH, sizeof(floorFinish));
    memset(wallSourceItem, 0xFF, sizeof(wallSourceItem));
    memset(floorSourceItem, 0xFF, sizeof(floorSourceItem));
    InitSpriteOverrides();
}



void SyncMaterialsToTerrain(void) {
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                CellType cell = grid[z][y][x];
                if (!CellIsSolid(cell)) continue;
                
                // All solid cells from terrain generation are natural
                SetWallNatural(x, y, z);
                
                // If material not yet set, default to granite
                if (GetWallMaterial(x, y, z) == MAT_NONE) {
                    SetWallMaterial(x, y, z, MAT_GRANITE);
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

// Get what item a wall drops based on its source item or material
ItemType GetWallDropItem(int x, int y, int z) {
    MaterialType mat = GetWallMaterial(x, y, z);
    CellType cell = grid[z][y][x];
    
    if (mat == MAT_NONE || !CellIsSolid(cell)) {
        return ITEM_NONE;
    }

    // Constructed walls: drop whatever item was used to build them
    ItemType src = GetWallSourceItem(x, y, z);
    if (src != ITEM_NONE) return src;

    // Natural/unknown: fall back to material-based drops
    return MaterialDropsItem(mat);
}

// Get what item a floor drops based on its source item or material
ItemType GetFloorDropItem(int x, int y, int z) {
    MaterialType mat = GetFloorMaterial(x, y, z);
    
    if (mat == MAT_NONE || IsFloorNatural(x, y, z)) {
        // Natural/no floor - nothing to drop
        return ITEM_NONE;
    }
    
    // Constructed floor: drop whatever item was used to build it
    ItemType src = GetFloorSourceItem(x, y, z);
    if (src != ITEM_NONE) return src;

    // Unknown: fall back to material-based drops
    return MaterialDropsItem(mat);
}

// =============================================================================
// Position-aware helpers
// =============================================================================

int GetCellSpriteAt(int x, int y, int z) {
    CellType cell = grid[z][y][x];
    MaterialType mat = GetWallMaterial(x, y, z);
    return GetSpriteForCellMat(cell, mat);
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
