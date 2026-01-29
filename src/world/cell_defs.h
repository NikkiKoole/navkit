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

// DF-style walkability toggle (defined in grid.c)
// When true, uses "walkable if cell below is solid" model
// When false, uses legacy "cell has CF_WALKABLE flag" model
extern bool g_useDFWalkability;

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

// Forward declaration of DF-style function (defined below)
static inline bool IsCellWalkableAt_DFStyle(int z, int y, int x);

static inline bool IsCellWalkableAt(int z, int y, int x) {
    if (g_useDFWalkability) {
        return IsCellWalkableAt_DFStyle(z, y, x);
    }
    // Legacy walkability: cell has CF_WALKABLE flag
    if (z < 0 || z >= gridDepth || y < 0 || y >= gridHeight || x < 0 || x >= gridWidth) return false;
    return CellIsWalkable(grid[z][y][x]);
}

// DF-style walkability: walkable if current cell is traversable AND
// (cell below is solid OR this cell has a constructed floor)
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
    
    // Can't walk inside solid blocks (dirt, walls, etc.)
    if (CellIsSolid(cellHere)) return false;
    
    // Constructed floor: walkable even with air below (balconies, bridges)
    if (HAS_FLOOR(x, y, z)) return true;
    
    // DF-style: walkable if cell below is solid
    // At z=0, treat z=-1 as implicit solid bedrock
    if (z == 0) {
        return true;  // Implicit bedrock below z=0
    }
    
    CellType cellBelow = grid[z-1][y][x];
    return CellIsSolid(cellBelow);
}

// =============================================================================
// Pathfinder-agnostic helpers (allow pathfinder extraction without DF knowledge)
// =============================================================================

// Check if a cell is a valid destination (not a wall-top in DF mode)
// Wall tops are traversable but shouldn't be random goals
static inline bool IsValidDestination(int z, int y, int x) {
    if (!IsCellWalkableAt(z, y, x)) return false;
    
    if (g_useDFWalkability && z > 0) {
        // In DF mode, skip "wall tops" (air above wall) as destinations
        if (grid[z][y][x] == CELL_AIR && grid[z-1][y][x] == CELL_WALL) {
            return false;
        }
    }
    return true;
}

// Get additional z-levels affected when a cell changes (for chunk dirtying)
// In DF mode, changing a cell affects walkability of cell above (z+1)
// Returns the number of additional z-levels written to outZ array
static inline int GetAdditionalAffectedZLevels(int z, int* outZ) {
    if (g_useDFWalkability && z + 1 < gridDepth) {
        outZ[0] = z + 1;
        return 1;
    }
    return 0;
}

// Can climb UP from z to z+1 at position (x, y)?
// Abstracted ladder connection logic for pathfinder extraction
static inline bool CanClimbUpAt(int x, int y, int z) {
    if (z + 1 >= gridDepth) return false;
    CellType low = grid[z][y][x];
    CellType high = grid[z+1][y][x];
    
    if (g_useDFWalkability) {
        // DF mode: need ladder at both levels AND upper level must be walkable
        bool hasLadderHere = CellIsLadder(low);
        bool hasLadderAbove = CellIsLadder(high);
        return hasLadderHere && hasLadderAbove && IsCellWalkableAt(z + 1, y, x);
    }
    
    // Legacy mode: direction-based ladder types
    bool lowCanUp = (low == CELL_LADDER_UP || low == CELL_LADDER_BOTH || low == CELL_LADDER);
    bool highCanDown = (high == CELL_LADDER_DOWN || high == CELL_LADDER_BOTH || high == CELL_LADDER);
    return lowCanUp && highCanDown;
}

// Can climb DOWN from z to z-1 at position (x, y)?
// Abstracted ladder connection logic for pathfinder extraction
static inline bool CanClimbDownAt(int x, int y, int z) {
    if (z - 1 < 0) return false;
    CellType high = grid[z][y][x];
    CellType low = grid[z-1][y][x];
    
    if (g_useDFWalkability) {
        // DF mode: need ladder at both levels AND lower level must be walkable
        bool hasLadderHere = CellIsLadder(high);
        bool hasLadderBelow = CellIsLadder(low);
        return hasLadderHere && hasLadderBelow && IsCellWalkableAt(z - 1, y, x);
    }
    
    // Legacy mode: direction-based ladder types
    bool highCanDown = (high == CELL_LADDER_DOWN || high == CELL_LADDER_BOTH || high == CELL_LADDER);
    bool lowCanUp = (low == CELL_LADDER_UP || low == CELL_LADDER_BOTH || low == CELL_LADDER);
    return highCanDown && lowCanUp;
}

#endif // CELL_DEFS_H
