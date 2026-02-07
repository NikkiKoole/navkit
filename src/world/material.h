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
    MAT_BRICK,         // Fired clay bricks
    MAT_IRON,          // Metal
    MAT_GLASS,         // Glass
    // Soil materials
    MAT_CLAY,          // Clay soil
    MAT_GRAVEL,        // Gravel soil
    MAT_SAND,          // Sand soil
    MAT_PEAT,          // Peat soil
    // Special materials
    MAT_BEDROCK,       // Unmineable bedrock
    MAT_COUNT
} MaterialType;

// Surface finish states (applied as overlays, not material variants)
typedef enum {
    FINISH_ROUGH = 0,
    FINISH_SMOOTH,
    FINISH_POLISHED,
    FINISH_ENGRAVED,
    FINISH_COUNT
} SurfaceFinish;

// Material flags
#define MF_FLAMMABLE  (1 << 0)  // Can catch fire
#define MF_UNMINEABLE (1 << 1)  // Cannot be mined/deconstructed

// Material definitions
typedef struct {
    const char* name;
    int spriteOffset;     // Offset to add to base cell sprite (0 = default)
    uint8_t flags;        // MF_* flags (flammable, etc.)
    uint8_t fuel;         // Fuel value for fire system (0 = won't burn)
    uint8_t ignitionResistance; // Subtracted from spread chance (0 = catches easily)
    ItemType dropsItem;   // What item this material drops when deconstructed
    // Phase 0: material-driven properties (for shape/material separation)
    int terrainSprite;    // Sprite when used as CELL_TERRAIN (0 = no terrain form)
    int treeTrunkSprite;  // Sprite for CELL_TREE_TRUNK (0 = not a tree material)
    int treeLeavesSprite; // Sprite for CELL_TREE_LEAVES
    int treeSaplingSprite;// Sprite for CELL_SAPLING
    uint8_t insulationTier; // Insulation when used as terrain (INSULATION_TIER_*)
    MaterialType burnsIntoMat; // What material this becomes when burned
} MaterialDef;

extern MaterialDef materialDefs[];

// Separate grids for wall and floor materials (like Dwarf Fortress)
extern uint8_t wallMaterial[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
extern uint8_t floorMaterial[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
// Natural flags for wall and floor materials
extern uint8_t wallNatural[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
extern uint8_t floorNatural[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
// Finish grids for wall and floor surfaces
extern uint8_t wallFinish[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
extern uint8_t floorFinish[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Wall material accessors
#define GetWallMaterial(x,y,z)      (wallMaterial[z][y][x])
#define SetWallMaterial(x,y,z,m)    (wallMaterial[z][y][x] = (uint8_t)(m))
#define HasWallMaterial(x,y,z)      (wallMaterial[z][y][x] != MAT_NONE)
// Wall natural accessors
#define IsWallNatural(x,y,z)        (wallNatural[z][y][x] != 0)
#define SetWallNatural(x,y,z)       (wallNatural[z][y][x] = 1)
#define ClearWallNatural(x,y,z)     (wallNatural[z][y][x] = 0)
// Wall finish accessors
#define GetWallFinish(x,y,z)        (wallFinish[z][y][x])
#define SetWallFinish(x,y,z,f)      (wallFinish[z][y][x] = (uint8_t)(f))

// Floor material accessors
#define GetFloorMaterial(x,y,z)     (floorMaterial[z][y][x])
#define SetFloorMaterial(x,y,z,m)   (floorMaterial[z][y][x] = (uint8_t)(m))
#define HasFloorMaterial(x,y,z)     (floorMaterial[z][y][x] != MAT_NONE)
// Floor natural accessors
#define IsFloorNatural(x,y,z)       (floorNatural[z][y][x] != 0)
#define SetFloorNatural(x,y,z)      (floorNatural[z][y][x] = 1)
#define ClearFloorNatural(x,y,z)    (floorNatural[z][y][x] = 0)
// Floor finish accessors
#define GetFloorFinish(x,y,z)       (floorFinish[z][y][x])
#define SetFloorFinish(x,y,z,f)     (floorFinish[z][y][x] = (uint8_t)(f))

// Material property accessors
#define MaterialName(m)         (materialDefs[m].name)
#define MaterialSpriteOffset(m) (materialDefs[m].spriteOffset)
#define MaterialDropsItem(m)    (materialDefs[m].dropsItem)
#define MaterialFuel(m)         (materialDefs[m].fuel)
#define MaterialIsFlammable(m)  (materialDefs[m].flags & MF_FLAMMABLE)
#define MaterialIsUnmineable(m) (materialDefs[m].flags & MF_UNMINEABLE)
#define MaterialIgnitionResistance(m) (materialDefs[m].ignitionResistance)
#define MaterialTerrainSprite(m)    (materialDefs[m].terrainSprite)
#define MaterialTreeTrunkSprite(m)  (materialDefs[m].treeTrunkSprite)
#define MaterialTreeLeavesSprite(m) (materialDefs[m].treeLeavesSprite)
#define MaterialTreeSaplingSprite(m) (materialDefs[m].treeSaplingSprite)
#define MaterialInsulationTier(m)   (materialDefs[m].insulationTier)
#define MaterialBurnsIntoMat(m)     (materialDefs[m].burnsIntoMat)

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

static inline SurfaceFinish DefaultFinishForNatural(bool natural) {
    return natural ? FINISH_ROUGH : FINISH_SMOOTH;
}

// Check if a wall is constructed (not natural terrain)
bool IsConstructedWall(int x, int y, int z);

// Get what item a wall drops based on its material
// For raw walls, uses CellDropsItem. For constructed, uses MaterialDropsItem.
ItemType GetWallDropItem(int x, int y, int z);

// Get what item a floor drops based on its material
ItemType GetFloorDropItem(int x, int y, int z);

// Sync wall materials/finishes for natural ground cells after terrain generation
void SyncMaterialsToTerrain(void);

// Position-aware helpers (consider cell type + material for lookups)
int GetCellSpriteAt(int x, int y, int z);
int GetInsulationAt(int x, int y, int z);
const char* GetCellNameAt(int x, int y, int z);

// ============================================================================
// Cell Placement Helper (reduces parallel update pattern across input.c)
// ============================================================================

typedef struct {
    CellType cellType;
    MaterialType wallMat;
    bool wallNatural;
    SurfaceFinish wallFinish;
    bool clearFloor;
    MaterialType floorMat;
    bool floorNatural;
    bool clearWater;
    uint8_t surfaceType;  // Use SURFACE_* defines from grid.h
} CellPlacementSpec;

// Convenience: natural terrain spec (soil, rock, dirt)
// Common defaults: wallNatural=true, wallFinish=FINISH_ROUGH, floorMat=MAT_NONE
static inline CellPlacementSpec NaturalTerrainSpec(CellType type, MaterialType mat, uint8_t surface, bool clearFloor, bool clearWater) {
    return (CellPlacementSpec){
        .cellType = type, .wallMat = mat, .wallNatural = true,
        .wallFinish = FINISH_ROUGH, .clearFloor = clearFloor, .floorMat = MAT_NONE,
        .floorNatural = false, .clearWater = clearWater, .surfaceType = surface
    };
}

// Place a cell with all properties in one atomic operation
// Defined in grid.c
void PlaceCellFull(int x, int y, int z, CellPlacementSpec spec);

#endif
