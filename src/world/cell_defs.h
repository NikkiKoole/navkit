#ifndef CELL_DEFS_H
#define CELL_DEFS_H

#include "grid.h"
#include "../simulation/temperature.h"
#include "../simulation/water.h"      // For deep water walkability check
#include "../entities/items.h"    // For ItemType

// Cell definition - all properties for a cell type in one place
typedef struct {
    const char* name;
    int sprite;
    uint8_t flags;
    uint8_t insulationTier;
    uint8_t fuel;
    CellType burnsInto;
    ItemType dropsItem;    // What item when mined/deconstructed (ITEM_NONE if nothing)
    uint8_t dropCount;     // Base drop count
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

// =============================================================================
// WALKABILITY MODEL (DF-style)
// =============================================================================
//
// A cell at (z,y,x) is walkable if:
// 1. The cell doesn't block movement (not a wall)
// 2. The cell below (z-1) is SOLID (has CF_SOLID flag)
// OR the cell is a ladder/ramp (self-supporting)
// OR the cell has a constructed floor (HAS_FLOOR flag)
// OR z=0 (implicit bedrock below)
//
// Think: "You stand ON TOP of solid ground, not inside it"
// Example: Air at z=1 above dirt at z=0 is walkable
//
// =============================================================================

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
#define CellDropsItem(c)        (cellDefs[c].dropsItem)
#define CellDropCount(c)        (cellDefs[c].dropCount)

// Convenience functions
#define CellAllowsFluids(c)     (!CellBlocksFluids(c))

// Backwards-compatible inline functions (replace old grid.h functions)
static inline bool IsLadderCell(CellType cell) {
    return CellIsLadder(cell);
}

static inline bool IsWallCell(CellType cell) {
    return CellBlocksMovement(cell);
}



// DF-style walkability: walkable if current cell is traversable AND
// (cell below is solid OR this cell has a constructed floor)
static inline bool IsCellWalkableAt(int z, int y, int x) {
    // Bounds check
    if (z < 0 || z >= gridDepth || y < 0 || y >= gridHeight || x < 0 || x >= gridWidth) return false;
    
    CellType cellHere = grid[z][y][x];
    
    // Can't walk through solid blocks (walls)
    if (CellBlocksMovement(cellHere)) return false;
    
    // Can't walk through blocked structures (workshops, furniture, etc.)
    if (cellFlags[z][y][x] & CELL_FLAG_WORKSHOP_BLOCK) return false;
    
    // Can't walk through deep water (level 4+ blocks movement)
    // Movers can wade through shallow water (1-3) but not swim through deep water
    if (waterGrid[z][y][x].level >= WATER_BLOCKS_MOVEMENT) return false;
    
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

// Check if a cell is a valid destination (not a wall-top unless ramps exist)
// Wall tops are traversable but shouldn't be random goals UNLESS ramps exist
// (when ramps exist, wall-tops may be reachable platforms)
static inline bool IsValidDestination(int z, int y, int x) {
    if (!IsCellWalkableAt(z, y, x)) return false;
    
    if (z > 0 && rampCount == 0) {
        // Without ramps, skip "wall tops" (air above wall) as destinations
        // When ramps exist, these could be valid platforms reachable via ramps
        if (grid[z][y][x] == CELL_AIR && grid[z-1][y][x] == CELL_WALL) {
            return false;
        }
    }
    return true;
}

// Get additional z-levels affected when a cell changes (for chunk dirtying)
// Changing a cell affects walkability of cell above (z+1) in DF-style model
// Returns the number of additional z-levels written to outZ array
static inline int GetAdditionalAffectedZLevels(int z, int* outZ) {
    if (z + 1 < gridDepth) {
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
    
    // Need ladder at both levels AND upper level must be walkable
    bool hasLadderHere = CellIsLadder(low);
    bool hasLadderAbove = CellIsLadder(high);
    return hasLadderHere && hasLadderAbove && IsCellWalkableAt(z + 1, y, x);
}

// Can climb DOWN from z to z-1 at position (x, y)?
// Abstracted ladder connection logic for pathfinder extraction
static inline bool CanClimbDownAt(int x, int y, int z) {
    if (z - 1 < 0) return false;
    CellType high = grid[z][y][x];
    CellType low = grid[z-1][y][x];
    
    // Need ladder at both levels AND lower level must be walkable
    bool hasLadderHere = CellIsLadder(high);
    bool hasLadderBelow = CellIsLadder(low);
    return hasLadderHere && hasLadderBelow && IsCellWalkableAt(z - 1, y, x);
}

// =============================================================================
// RAMP HELPERS - Directional z-transitions
// =============================================================================

// Check if cell is a directional ramp (N/E/S/W)
static inline bool CellIsDirectionalRamp(CellType cell) {
    return cell == CELL_RAMP_N || cell == CELL_RAMP_E || 
           cell == CELL_RAMP_S || cell == CELL_RAMP_W;
}

// Get the direction offset for the HIGH side of the ramp (where z+1 exit is)
static inline void GetRampHighSideOffset(CellType cell, int* dx, int* dy) {
    switch (cell) {
        case CELL_RAMP_N: *dx = 0;  *dy = -1; break;  // North (y-1)
        case CELL_RAMP_E: *dx = 1;  *dy = 0;  break;  // East (x+1)
        case CELL_RAMP_S: *dx = 0;  *dy = 1;  break;  // South (y+1)
        case CELL_RAMP_W: *dx = -1; *dy = 0;  break;  // West (x-1)
        default: *dx = 0; *dy = 0; break;
    }
}

// Can walk UP the ramp at (x,y,z) to exit at z+1?
// Returns true if ramp exists and exit tile at z+1 is walkable
static inline bool CanWalkUpRampAt(int x, int y, int z) {
    if (z + 1 >= gridDepth) return false;
    
    CellType cell = grid[z][y][x];
    if (!CellIsDirectionalRamp(cell)) return false;
    
    int highDx, highDy;
    GetRampHighSideOffset(cell, &highDx, &highDy);
    
    int exitX = x + highDx;
    int exitY = y + highDy;
    
    // Check bounds
    if (exitX < 0 || exitX >= gridWidth || exitY < 0 || exitY >= gridHeight) {
        return false;
    }
    
    // Exit tile at z+1 must be walkable (works for both walkability modes)
    return IsCellWalkableAt(z + 1, exitY, exitX);
}

// Can walk DOWN onto the ramp at (x,y,z) from z+1?
// We're standing at the exit tile at z+1, checking if we can descend
static inline bool CanWalkDownRampAt(int x, int y, int z) {
    if (z < 0) return false;
    
    CellType cell = grid[z][y][x];
    if (!CellIsDirectionalRamp(cell)) return false;
    
    int highDx, highDy;
    GetRampHighSideOffset(cell, &highDx, &highDy);
    
    int exitX = x + highDx;
    int exitY = y + highDy;
    
    // Check bounds for where we'd be coming from
    if (exitX < 0 || exitX >= gridWidth || exitY < 0 || exitY >= gridHeight) {
        return false;
    }
    
    // The ramp itself must be walkable (should always be true for valid ramps)
    return IsCellWalkableAt(z, y, x);
}

// Check if moving from (fromX, fromY) to ramp at (rampX, rampY) at same z is allowed
// Blocks side entry (perpendicular to ramp direction)
static inline bool CanEnterRampFromSide(int rampX, int rampY, int z, int fromX, int fromY) {
    CellType cell = grid[z][rampY][rampX];
    if (!CellIsDirectionalRamp(cell)) return true;  // Not a ramp, allow normal movement
    
    int highDx, highDy;
    GetRampHighSideOffset(cell, &highDx, &highDy);
    
    // Calculate movement direction
    int moveDx = rampX - fromX;
    int moveDy = rampY - fromY;
    
    // Allow entry from low side (opposite of high) or high side
    // Block entry from perpendicular directions
    // Moving toward high = coming from low side
    // Moving toward low = coming from high side
    bool fromLowSide = (moveDx == highDx && moveDy == highDy);
    bool fromHighSide = (moveDx == -highDx && moveDy == -highDy);
    
    return fromLowSide || fromHighSide;
}

// Find if there's a ramp adjacent to (x,y,z) that points TO this cell
// Used for detecting ramp exits - the ramp itself is one cell away
// Returns true if found, and optionally sets outRampX/outRampY to the ramp's position
// Pass -1 pointer values or valid pointers for output coordinates
static inline bool FindRampPointingTo(int x, int y, int z, int* outRampX, int* outRampY) {
    // For each direction, check if adjacent cell is a ramp pointing toward (x,y)
    // RAMP_N at (x, y+1) points north to (x, y)
    // RAMP_E at (x-1, y) points east to (x, y)
    // RAMP_S at (x, y-1) points south to (x, y)
    // RAMP_W at (x+1, y) points west to (x, y)
    static const int rampOffsets[4][3] = {
        {0, 1, CELL_RAMP_N},   // Ramp south of us pointing north
        {-1, 0, CELL_RAMP_E},  // Ramp west of us pointing east
        {0, -1, CELL_RAMP_S},  // Ramp north of us pointing south
        {1, 0, CELL_RAMP_W}    // Ramp east of us pointing west
    };
    
    for (int r = 0; r < 4; r++) {
        int rx = x + rampOffsets[r][0];
        int ry = y + rampOffsets[r][1];
        if (rx < 0 || rx >= gridWidth || ry < 0 || ry >= gridHeight) continue;
        if (grid[z][ry][rx] == (CellType)rampOffsets[r][2]) {
            if (outRampX) *outRampX = rx;
            if (outRampY) *outRampY = ry;
            return true;
        }
    }
    return false;
}

// Simplified version that only checks existence (no output coordinates)
static inline bool HasRampPointingTo(int x, int y, int z) {
    return FindRampPointingTo(x, y, z, (int*)0, (int*)0);
}

// Check if position (x,y) at z can transition UP to z+1 via a ramp
// Either by being on a ramp cell, or being at a ramp's exit cell
static inline bool CanRampTransitionUp(int x, int y, int z) {
    if (z + 1 >= gridDepth) return false;
    
    // Case 1: Standing on a ramp that goes up
    CellType cell = grid[z][y][x];
    if (CellIsDirectionalRamp(cell)) {
        return CanWalkUpRampAt(x, y, z);
    }
    
    // Case 2: At exit position - check if there's a ramp pointing here
    // and z+1 is walkable at this position
    if (IsCellWalkableAt(z + 1, y, x) && HasRampPointingTo(x, y, z)) {
        return true;
    }
    
    return false;
}

#endif // CELL_DEFS_H
