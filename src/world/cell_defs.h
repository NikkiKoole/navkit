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

// Physics flags (5 bits used, 3 reserved)
#define CF_BLOCKS_MOVEMENT  (1 << 0)  // Can't walk through (walls, closed doors, grates)
#define CF_WALKABLE         (1 << 1)  // Can stand on (ground, floors, ladders)
#define CF_LADDER           (1 << 2)  // Vertical climbing - slower movement
#define CF_RAMP             (1 << 3)  // Vertical walking - normal speed (stairs)
#define CF_BLOCKS_FLUIDS    (1 << 4)  // Blocks water/smoke/steam

// Common flag combinations
#define CF_GROUND  (CF_WALKABLE)
#define CF_WALL    (CF_BLOCKS_MOVEMENT | CF_BLOCKS_FLUIDS)

// Cell definitions table (defined in cell_defs.c)
extern CellDef cellDefs[];

// Flag accessors
#define CellHasFlag(c, f)       (cellDefs[c].flags & (f))
#define CellBlocksMovement(c)   CellHasFlag(c, CF_BLOCKS_MOVEMENT)
#define CellIsWalkable(c)       CellHasFlag(c, CF_WALKABLE)
#define CellIsLadder(c)         CellHasFlag(c, CF_LADDER)
#define CellIsRamp(c)           CellHasFlag(c, CF_RAMP)
#define CellBlocksFluids(c)     CellHasFlag(c, CF_BLOCKS_FLUIDS)

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

#endif // CELL_DEFS_H
