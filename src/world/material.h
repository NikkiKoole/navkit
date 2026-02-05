#ifndef MATERIAL_H
#define MATERIAL_H

#include <stdint.h>
#include <stdbool.h>
#include "grid.h"                  // For MAX_GRID_* constants
#include "../entities/items.h"    // For ItemType

// Material types - what a wall/floor/item is made of
typedef enum {
    MAT_NONE = 0,      // No wall/floor present at this position
    // Wood species materials
    MAT_OAK,
    MAT_PINE,
    MAT_BIRCH,
    MAT_WILLOW,
    // Stone materials
    MAT_GRANITE,
    // Earth/other materials
    MAT_DIRT,          // Dirt/earth
    MAT_IRON,          // Metal
    MAT_GLASS,         // Glass
    MAT_COUNT
} MaterialType;

// Material flags
#define MF_FLAMMABLE  (1 << 0)  // Can catch fire

// Material definitions
typedef struct {
    const char* name;
    int spriteOffset;     // Offset to add to base cell sprite (0 = default)
    uint8_t flags;        // MF_* flags (flammable, etc.)
    uint8_t fuel;         // Fuel value for fire system (0 = won't burn)
    ItemType dropsItem;   // What item this material drops when deconstructed
} MaterialDef;

extern MaterialDef materialDefs[];

// Separate grids for wall and floor materials (like Dwarf Fortress)
extern uint8_t wallMaterial[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
extern uint8_t floorMaterial[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
// Natural flags for wall and floor materials
extern uint8_t wallNatural[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
extern uint8_t floorNatural[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Wall material accessors
#define GetWallMaterial(x,y,z)      (wallMaterial[z][y][x])
#define SetWallMaterial(x,y,z,m)    (wallMaterial[z][y][x] = (uint8_t)(m))
#define HasWallMaterial(x,y,z)      (wallMaterial[z][y][x] != MAT_NONE)
// Wall natural accessors
#define IsWallNatural(x,y,z)        (wallNatural[z][y][x] != 0)
#define SetWallNatural(x,y,z)       (wallNatural[z][y][x] = 1)
#define ClearWallNatural(x,y,z)     (wallNatural[z][y][x] = 0)

// Floor material accessors
#define GetFloorMaterial(x,y,z)     (floorMaterial[z][y][x])
#define SetFloorMaterial(x,y,z,m)   (floorMaterial[z][y][x] = (uint8_t)(m))
#define HasFloorMaterial(x,y,z)     (floorMaterial[z][y][x] != MAT_NONE)
// Floor natural accessors
#define IsFloorNatural(x,y,z)       (floorNatural[z][y][x] != 0)
#define SetFloorNatural(x,y,z)      (floorNatural[z][y][x] = 1)
#define ClearFloorNatural(x,y,z)    (floorNatural[z][y][x] = 0)

// Material property accessors
#define MaterialName(m)         (materialDefs[m].name)
#define MaterialSpriteOffset(m) (materialDefs[m].spriteOffset)
#define MaterialDropsItem(m)    (materialDefs[m].dropsItem)
#define MaterialFuel(m)         (materialDefs[m].fuel)
#define MaterialIsFlammable(m)  (materialDefs[m].flags & MF_FLAMMABLE)

void InitMaterials(void);

// Material category helpers
static inline bool IsWoodMaterial(MaterialType mat) {
    return mat == MAT_OAK || mat == MAT_PINE || mat == MAT_BIRCH || mat == MAT_WILLOW;
}
static inline bool IsStoneMaterial(MaterialType mat) {
    return mat == MAT_GRANITE;
}
static inline bool IsMetalMaterial(MaterialType mat) {
    return mat == MAT_IRON;
}

// Check if a wall is constructed (not natural terrain)
bool IsConstructedWall(int x, int y, int z);

// Get what item a wall drops based on its material
// For raw walls, uses CellDropsItem. For constructed, uses MaterialDropsItem.
ItemType GetWallDropItem(int x, int y, int z);

// Get what item a floor drops based on its material
ItemType GetFloorDropItem(int x, int y, int z);

#endif
