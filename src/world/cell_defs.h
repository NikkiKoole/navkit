#ifndef CELL_DEFS_H
#define CELL_DEFS_H

#include "grid.h"
#include "../simulation/temperature.h"

// Cell definition - all properties for a cell type in one place
typedef struct {
    const char* name;
    int sprite;
    uint8_t flags;
    uint8_t insulationTier;
    uint8_t fuel;
    CellType burnsInto;
    // Future: uint8_t movementCost;
    // Future: uint8_t hardness;
} CellDef;

// Physics flags (6 bits used, 2 reserved)
#define CF_BLOCKS_MOVEMENT  (1 << 0)  // Can't walk through (walls, closed doors, grates)
#define CF_WALKABLE         (1 << 1)  // Can stand on (ground, floors, ladders) - legacy
#define CF_LADDER           (1 << 2)  // Vertical climbing - slower movement
#define CF_RAMP             (1 << 3)  // Vertical walking - normal speed (stairs)
#define CF_BLOCKS_FLUIDS    (1 << 4)  // Blocks water/smoke/steam
#define CF_SOLID            (1 << 5)  // Can stand ON this cell from above (DF-style walkability)

// Common flag combinations
#define CF_GROUND  (CF_WALKABLE | CF_SOLID)
#define CF_WALL    (CF_BLOCKS_MOVEMENT | CF_BLOCKS_FLUIDS | CF_SOLID)

// Cell definitions table (defined in cell_defs.c)
extern CellDef cellDefs[];

// Flag accessors
#define CellHasFlag(c, f)       (cellDefs[c].flags & (f))
#define CellBlocksMovement(c)   CellHasFlag(c, CF_BLOCKS_MOVEMENT)
#define CellIsWalkable(c)       CellHasFlag(c, CF_WALKABLE)
#define CellIsLadder(c)         CellHasFlag(c, CF_LADDER)
#define CellIsRamp(c)           CellHasFlag(c, CF_RAMP)
#define CellBlocksFluids(c)     CellHasFlag(c, CF_BLOCKS_FLUIDS)
#define CellIsSolid(c)          CellHasFlag(c, CF_SOLID)

// Field accessors
#define CellName(c)             (cellDefs[c].name)
#define CellSprite(c)           (cellDefs[c].sprite)
#define CellInsulationTier(c)   (cellDefs[c].insulationTier)
#define CellFuel(c)             (cellDefs[c].fuel)
#define CellBurnsInto(c)        (cellDefs[c].burnsInto)

// Convenience functions
#define CellAllowsFluids(c)     (!CellBlocksFluids(c))

// Backwards-compatible inline functions (replace old grid.h functions)
static inline bool IsLadderCell(CellType cell) {
    return CellIsLadder(cell);
}

static inline bool IsWallCell(CellType cell) {
    return CellBlocksMovement(cell);
}

static inline bool IsCellWalkableAt(int z, int y, int x) {
    if (z < 0 || z >= gridDepth || y < 0 || y >= gridHeight || x < 0 || x >= gridWidth) return false;
    return CellIsWalkable(grid[z][y][x]);
}

// DF-style walkability: walkable if current cell is traversable AND cell below is solid
// This function exists but is not yet used - will be activated via toggle in Phase 3
static inline bool IsCellWalkableAt_DFStyle(int z, int y, int x) {
    // Bounds check
    if (z < 0 || z >= gridDepth || y < 0 || y >= gridHeight || x < 0 || x >= gridWidth) return false;
    
    CellType cellHere = grid[z][y][x];
    
    // Can't walk through solid blocks (walls)
    if (CellBlocksMovement(cellHere)) return false;
    
    // Ladders are always walkable (special case - they provide their own support)
    if (CellIsLadder(cellHere)) return true;
    
    // Ramps are always walkable (special case)
    if (CellIsRamp(cellHere)) return true;
    
    // Z=0 edge case: treat as having implicit bedrock below
    // At z=0, walkable if the cell itself has the legacy walkable flag
    // This allows gradual migration of terrain generators
    if (z == 0) {
        return CellIsWalkable(cellHere);
    }
    
    // DF-style: walkable if cell below is solid
    CellType cellBelow = grid[z-1][y][x];
    return CellIsSolid(cellBelow);
}

#endif // CELL_DEFS_H
