#ifndef GRID_H
#define GRID_H

#include <stdbool.h>
#include <stdint.h>

// Note: CellPlacementSpec uses MaterialType and SurfaceFinish from material.h
// Users of PlaceCellFull() should include "material.h" for full type definitions

// Maximum grid dimensions (for static array allocation)
#define MAX_GRID_WIDTH  512
#define MAX_GRID_HEIGHT 512
#define MAX_GRID_DEPTH  16          // Z-levels
#define DEFAULT_CHUNK_SIZE  16

// Runtime grid and chunk dimensions
extern int gridWidth;
extern int gridHeight;
extern int gridDepth;               // Z-levels at runtime
extern int chunkWidth;
extern int chunkHeight;
extern int chunksX;
extern int rampCount;               // Number of directional ramps in the grid
extern int chunksY;

// For static array sizing
#define MAX_CHUNKS_X (MAX_GRID_WIDTH / 8)   // minimum chunk size of 8
#define MAX_CHUNKS_Y (MAX_GRID_HEIGHT / 8)

typedef enum { 
    CELL_WALL, 
    CELL_AIR, 
    CELL_LADDER_UP,    // Bottom of ladder - can climb UP from here
    CELL_LADDER_DOWN,  // Top of ladder - can climb DOWN from here  
    CELL_LADDER_BOTH,  // Middle of ladder - can go both directions
    CELL_RAMP_N,       // Ramp: high side north - enter from south at z, exit north at z+1
    CELL_RAMP_E,       // Ramp: high side east - enter from west at z, exit east at z+1
    CELL_RAMP_S,       // Ramp: high side south - enter from north at z, exit south at z+1
    CELL_RAMP_W,       // Ramp: high side west - enter from east at z, exit west at z+1
    CELL_SAPLING,      // Young tree - grows into trunk over time
    CELL_TREE_TRUNK,   // Solid wood - blocks movement, choppable (main vertical column)
    CELL_TREE_BRANCH,  // Tree branch - solid wood, choppable (lateral growth)
    CELL_TREE_ROOT,    // Tree root - solid wood, underground/exposed (at base)
    CELL_TREE_FELLED,  // Felled log - walkable, can be designated for hauling
    CELL_TREE_LEAVES,  // Tree canopy - blocks movement, decays without trunk
    CELL_BUSH,         // Bush - walkable ground vegetation, slows movement
    CELL_TRACK,        // Train track - walkable, autotiled based on neighbors
    CELL_DOOR,         // Door - walkable, blocks fluids/light, solid for support
    CELL_TYPE_COUNT
} CellType;

static inline const char* CellTypeName(int type) {
    static const char* names[] = {
        "WALL", "AIR", "LADDER_UP", "LADDER_DOWN", "LADDER_BOTH",
        "RAMP_N", "RAMP_E", "RAMP_S", "RAMP_W",
        "SAPLING", "TREE_TRUNK", "TREE_BRANCH", "TREE_ROOT", "TREE_FELLED", "TREE_LEAVES",
        "BUSH", "TRACK", "DOOR"
    };
    _Static_assert(sizeof(names)/sizeof(names[0]) == CELL_TYPE_COUNT,
                   "CellTypeName out of sync with CellType enum");
    return (type >= 0 && type < CELL_TYPE_COUNT) ? names[type] : "?";
}

extern CellType grid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Cell flags - per-cell terrain properties (burned, wetness, etc.)
extern uint8_t cellFlags[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Cell flag bits
#define CELL_FLAG_BURNED    (1 << 0)  // Bit 0: cell has been burned
// Bits 1-2: wetness level (0=dry, 1=damp, 2=wet, 3=soaked)
#define CELL_WETNESS_MASK   (3 << 1)
#define CELL_WETNESS_SHIFT  1
// Bits 3-4: surface overlay type (0=bare, 1=trampled, 2=normal grass, 3=tall grass)
#define CELL_SURFACE_MASK   (3 << 3)
#define CELL_SURFACE_SHIFT  3
// Bit 5: has constructed floor (can stand on even with air below)
#define CELL_FLAG_HAS_FLOOR (1 << 5)
// Bit 6: blocked by structure (workshops, furniture, machines, etc.)
// Any structure with non-walkable tiles sets this flag on create, clears on delete.
// Generic - not workshop-specific, can be reused for other blocking structures.
#define CELL_FLAG_WORKSHOP_BLOCK (1 << 6)
// Bit 7: reserved

// Surface overlay types (ground condition only — trampled paths, etc.)
#define SURFACE_BARE        0
#define SURFACE_TRAMPLED    1

// Vegetation types (plants growing on ground — separate grid)
typedef enum {
    VEG_NONE = 0,
    VEG_GRASS_SHORT,    // Short grass (recently cut or recovering)
    VEG_GRASS,          // Normal grass
    VEG_GRASS_TALL,     // Tall grass (undisturbed)
    VEG_GRASS_TALLER,   // Very tall grass (harvestable)
} VegetationType;

extern uint8_t vegetationGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

#define GetVegetation(x,y,z)      ((VegetationType)vegetationGrid[z][y][x])
#define SetVegetation(x,y,z,v)    (vegetationGrid[z][y][x] = (uint8_t)(v))

// Exploration grid — fog of war (0=unexplored, 1=explored)
extern uint8_t exploredGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

#define DEFAULT_MOVER_VISION_RADIUS  10
#define DEFAULT_SPAWN_VISION_RADIUS  12

// Check if cell is explored (OOB = true for safety, sandbox = always true)
#ifndef GAME_MODE_DEFINED
#define GAME_MODE_DEFINED
typedef enum { GAME_MODE_SANDBOX, GAME_MODE_SURVIVAL } GameMode;
#endif
extern GameMode gameMode;

static inline bool IsExplored(int x, int y, int z) {
    if (gameMode == GAME_MODE_SANDBOX) return true;
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return true;
    return exploredGrid[z][y][x] != 0;
}

static inline void SetExplored(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return;
    exploredGrid[z][y][x] = 1;
}

// Reveal all cells within radius of a point, extending vertically through air
void RevealAroundPoint(int cx, int cy, int cz, int radius);

// Cell flag helpers
#define HAS_CELL_FLAG(x,y,z,f)     (!!(cellFlags[z][y][x] & (f)))
#define SET_CELL_FLAG(x,y,z,f)     (cellFlags[z][y][x] |= (f))
#define CLEAR_CELL_FLAG(x,y,z,f)   (cellFlags[z][y][x] &= ~(f))
#define GET_CELL_WETNESS(x,y,z)    ((cellFlags[z][y][x] & CELL_WETNESS_MASK) >> CELL_WETNESS_SHIFT)
#define SET_CELL_WETNESS(x,y,z,w)  (cellFlags[z][y][x] = (cellFlags[z][y][x] & ~CELL_WETNESS_MASK) | ((w) << CELL_WETNESS_SHIFT))
#define GET_CELL_SURFACE(x,y,z)    ((cellFlags[z][y][x] & CELL_SURFACE_MASK) >> CELL_SURFACE_SHIFT)
#define SET_CELL_SURFACE(x,y,z,s)  (cellFlags[z][y][x] = (cellFlags[z][y][x] & ~CELL_SURFACE_MASK) | ((s) << CELL_SURFACE_SHIFT))
// Floor flag helpers (for constructed floors over empty space)
#define HAS_FLOOR(x,y,z)           (!!(cellFlags[z][y][x] & CELL_FLAG_HAS_FLOOR))
#define SET_FLOOR(x,y,z)           (cellFlags[z][y][x] |= CELL_FLAG_HAS_FLOOR)
#define CLEAR_FLOOR(x,y,z)         (cellFlags[z][y][x] &= ~CELL_FLAG_HAS_FLOOR)

// Helper to check if a cell is air (empty space that can be fallen through)
static inline bool IsCellAirAt(int z, int y, int x) {
    if (z < 0 || z >= gridDepth || y < 0 || y >= gridHeight || x < 0 || x >= gridWidth) return false;
    return grid[z][y][x] == CELL_AIR;
}

// Note: IsLadderCell, IsWallCell, IsCellWalkableAt are defined in cell_defs.h
// Include cell_defs.h after grid.h to use them
extern bool needsRebuild;      // Generic flag (legacy)
extern bool hpaNeedsRebuild;   // HPA* specific rebuild flag
extern bool jpsNeedsRebuild;   // JPS+ specific rebuild flag

// Initialize grid with specific dimensions and default chunk size (16x16)
void InitGridWithSize(int width, int height);

// Initialize grid with specific dimensions and chunk size
void InitGridWithSizeAndChunkSize(int width, int height, int chunkW, int chunkH);

// Fill z=0 with dirt and grass surface (call after InitGridWithSize for DF-style terrain)
void FillGroundLevel(void);

// Initialize grid from ASCII map with default chunk size
// '.' = walkable, '#' = wall, newlines separate rows
// Dimensions are auto-detected from the string
// Returns 1 on success, 0 on failure
int InitGridFromAscii(const char* ascii);

// Initialize grid from ASCII map with custom chunk size
// If chunkW/chunkH are 0, uses grid dimensions (1 chunk = whole grid)
int InitGridFromAsciiWithChunkSize(const char* ascii, int chunkW, int chunkH);

// Initialize multi-floor grid from ASCII map
// Format: "floor:0\n...\nfloor:1\n..." 
// '.' = walkable, '#' = wall, 'L' = ladder, newlines separate rows
// Returns 1 on success, 0 on failure
int InitMultiFloorGridFromAscii(const char* ascii, int chunkW, int chunkH);

// Ladder placement with auto-connection
// Places a ladder at (x,y,z) and auto-connects to level above
void PlaceLadder(int x, int y, int z);

// Ladder erasure with cascade
// Erases/downgrades ladder at (x,y,z) and cascades changes
void EraseLadder(int x, int y, int z);

// Recalculate ladder types in a column based on neighbors
void RecalculateLadderColumn(int x, int y);

// Ramp placement - check if ramp can be placed
bool CanPlaceRamp(int x, int y, int z, CellType rampType);

// Auto-detect ramp direction based on terrain (returns CELL_AIR if no valid direction)
CellType AutoDetectRampDirection(int x, int y, int z);

// Ramp placement - places ramp if valid, pushes movers/items out
void PlaceRamp(int x, int y, int z, CellType rampType);

// Clean up special cell types (ramps/ladders) before overwriting a cell
void ClearCellCleanup(int x, int y, int z);

// Ramp erasure - removes ramp, replaces with walkable/air
void EraseRamp(int x, int y, int z);

// Check if an existing ramp is still valid (high side has solid support)
bool IsRampStillValid(int x, int y, int z);

// Validate and cleanup invalid ramps in a region (call after terrain changes)
// Returns number of ramps removed
int ValidateAndCleanupRamps(int minX, int minY, int minZ, int maxX, int maxY, int maxZ);

// Validate all ramps in the entire grid
int ValidateAllRamps(void);

#endif // GRID_H
