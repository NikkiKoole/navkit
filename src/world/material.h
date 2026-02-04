#ifndef MATERIAL_H
#define MATERIAL_H

#include <stdint.h>
#include "grid.h"                  // For MAX_GRID_* constants
#include "../entities/items.h"    // For ItemType

// Material types - what a cell/item is made of
typedef enum {
    MAT_NATURAL = 0,   // Use cell type to determine (dirt=dirt, wall=stone, tree=wood)
    MAT_STONE,         // Generic stone (from stone blocks)
    MAT_WOOD,          // Wood material
    MAT_IRON,          // Metal
    MAT_GLASS,         // Future
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
extern uint8_t cellMaterial[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Accessors
#define GetCellMaterial(x,y,z)      (cellMaterial[z][y][x])
#define SetCellMaterial(x,y,z,m)    (cellMaterial[z][y][x] = (uint8_t)(m))
#define IsConstructedCell(x,y,z)    (cellMaterial[z][y][x] != MAT_NATURAL)

// Material property accessors
#define MaterialName(m)         (materialDefs[m].name)
#define MaterialSpriteOffset(m) (materialDefs[m].spriteOffset)
#define MaterialDropsItem(m)    (materialDefs[m].dropsItem)
#define MaterialFuel(m)         (materialDefs[m].fuel)
#define MaterialIsFlammable(m)  (materialDefs[m].flags & MF_FLAMMABLE)

void InitMaterials(void);

// Get what item a cell drops based on its material
// For natural cells, uses CellDropsItem. For constructed, uses MaterialDropsItem.
ItemType GetCellDropItem(int x, int y, int z);

#endif
