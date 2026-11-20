#ifndef GRID_H
#define GRID_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// Note: CellPlacementSpec uses MaterialType and SurfaceFinish from material.h
// Users of PlaceCellFull() should include "material.h" for full type definitions

// Maximum grid dimensions (for static array allocation). Overridable for test builds:
// `make path1024` builds build/bin/path1024 with 1024 (docs/todo/architecture/large-worlds.md)
#ifndef MAX_GRID_WIDTH
#define MAX_GRID_WIDTH  512
#endif
#ifndef MAX_GRID_HEIGHT
#define MAX_GRID_HEIGHT 512
#endif
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
    CELL_PLATFORM,     // Platform - walkable surface next to tracks (station)
    CELL_DOOR,         // Door - walkable, blocks fluids/light, solid for support
    CELL_WINDOW,       // Window - wall that transmits light
    CELL_TYPE_COUNT
} CellType;

static inline const char* CellTypeName(int type) {
    static const char* names[] = {
        "WALL", "AIR", "LADDER_UP", "LADDER_DOWN", "LADDER_BOTH",
        "RAMP_N", "RAMP_E", "RAMP_S", "RAMP_W",
        "SAPLING", "TREE_TRUNK", "TREE_BRANCH", "TREE_ROOT", "TREE_FELLED", "TREE_LEAVES",
        "BUSH", "TRACK", "PLATFORM", "DOOR", "WINDOW"
    };
    _Static_assert(sizeof(names)/sizeof(names[0]) == CELL_TYPE_COUNT,
                   "CellTypeName out of sync with CellType enum");
    return (type >= 0 && type < CELL_TYPE_COUNT) ? names[type] : "?";
}

// Per-cell layer access: every layer (a pointer into the world arena, see grid_layers.c, sized to
// the current map) has a macro LAYER(x, y, z)
// (GRID, WATER, CELL_FLAGS, ...) declared next to it, usable as an lvalue. Use the macros, never
// index a layer directly: CellIndex is the only place that knows the storage layout.
// See docs/todo/architecture/chunked-world-plan.md, "Step 1".
//
// Layout: chunk-major. The map is cut into 16x16 storage chunks (fixed, unrelated to the HPA*
// chunkWidth/chunkHeight); a chunk stores its full column, all MAX_GRID_DEPTH levels:
//   index = chunk * STORE_CHUNK_STRIDE + (z * 16 + ly) * 16 + lx,   chunk = cy * storeChunksX + cx
// The map spans storeChunksX x storeChunksY = (gridWidth + 16) / 16 x (gridHeight + 16) / 16
// chunks: one spare cell column (and row) of margin, so a read at x == gridWidth or
// y == gridHeight lands in cleared margin cells, as it did with the old [z][y][x] padding.
// The per-x / per-y parts of the index come from tables rebuilt when the map size changes, so a
// runtime-sized layout costs no more per access than a fixed one.
#define STORE_CHUNK_SHIFT 4
#define STORE_CHUNK       (1 << STORE_CHUNK_SHIFT)            // 16
#define STORE_MASK        (STORE_CHUNK - 1)
#define STORE_CHUNK_CELLS (STORE_CHUNK * STORE_CHUNK * MAX_GRID_DEPTH)   // 4096
// Chunks are STORE_CHUNK_STRIDE elements apart, not 4096: a power-of-two stride puts the same
// row of every chunk in the same L1 cache sets, so a row sweep across 33 chunks thrashes them.
// 64 elements of padding shifts each chunk by at least one cache line.
#define STORE_CHUNK_PAD    64
#define STORE_CHUNK_STRIDE (STORE_CHUNK_CELLS + STORE_CHUNK_PAD)
_Static_assert(MAX_GRID_DEPTH == 16, "CellIndex packs z into 4 bits");
#define STORE_CHUNKS_FOR(n) (((n) + STORE_CHUNK) >> STORE_CHUNK_SHIFT)   // ceil((n + 1) / 16)

// Current layout (set by AllocGridLayers): chunks per row / chunk rows, elements per layer.
extern int storeChunksX, storeChunksY;
extern size_t gridLayerCells;

// Lookup tables of the x and y parts of the index (rebuilt by AllocGridLayers when the map size
// changes): a cell lookup is two loads and two adds, close to the old [z][y][x] multiply-add.
// One spare entry for the x == gridWidth / y == gridHeight margin read.
static inline int CellIndexZ(int z) { return z << 8; }
extern int cellIndexTabX[MAX_GRID_WIDTH + 1];
extern int cellIndexTabY[MAX_GRID_HEIGHT + 1];

// Allocate every per-cell layer for the current gridWidth/gridHeight (no-op when the size is
// unchanged), rebuild the index tables, and fill fresh layers with their empty value.
// InitGridWithDimensions calls it; anything else that sets gridWidth/gridHeight must too.
void AllocGridLayers(void);
size_t GridLayersBytes(void);   // world arena size (all layers)
bool GridLayerZeroFast(void* layer, size_t bytes);   // zero via fresh pages (grid_layers.c)
void GridLayersReport(void);   // per-layer resident MB (headless --profile)
static inline int CellIndex(int x, int y, int z) {
    return cellIndexTabX[x] + cellIndexTabY[y] + CellIndexZ(z);
}

// Fast whole-row sweeps. FOR_X_IN_ROW(x, i, y, z) { ... } visits x = 0..gridWidth-1 in order
// (same order as a plain x loop) with i = CellIndex(x, y, z), computed once per 16-cell run and
// then incremented. Read the current cell with LAYER_AT(layer, i), e.g. LAYER_AT(grid, i), and
// use the normal macros for neighbours. `continue` and `return` work; `break` only leaves the
// current 16-cell run, so don't use it.
#define FOR_X_IN_ROW(x, i, y, z) \
    for (int x##RunStart_ = 0; x##RunStart_ < gridWidth; x##RunStart_ += STORE_CHUNK) \
        for (int x = x##RunStart_, i = CellIndex(x##RunStart_, (y), (z)), \
                 x##RunEnd_ = (x##RunStart_ + STORE_CHUNK < gridWidth ? x##RunStart_ + STORE_CHUNK : gridWidth); \
             x < x##RunEnd_; x++, i++)
// Element i (from CellIndex / FOR_X_IN_ROW) of a layer array.
#define LAYER_AT(layer, i) ((layer)[(i)])

// Bounds check, compiled in with -DNAVKIT_CELL_CHECK (make cellcheck / test-cellcheck):
// reports every call site that touches a cell outside 0..gridWidth-1 etc. once.
#ifdef NAVKIT_CELL_CHECK
int CellIndexChecked(int x, int y, int z, const char* file, int line);
#define CELL_AT(x, y, z) [CellIndexChecked((x), (y), (z), __FILE__, __LINE__)]
#else
#define CELL_AT(x, y, z) [CellIndex((x), (y), (z))]
#endif
// Defined here, not in furniture.h: cell_defs.h uses it before furniture.h is included.
#define FURNITURE_MOVE_COST(x, y, z) (furnitureMoveCostGrid CELL_AT((x), (y), (z)))

// Stored as one byte per cell (CellType values); read/write CellType as usual.
_Static_assert(CELL_TYPE_COUNT <= 256, "CellType must fit the uint8_t grid");
extern uint8_t* grid;
#define GRID(x, y, z) (grid CELL_AT((x), (y), (z)))

// Cell flags - per-cell terrain properties (burned, wetness, etc.)
extern uint8_t* cellFlags;
#define CELL_FLAGS(x, y, z) (cellFlags CELL_AT((x), (y), (z)))

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
// Bit 7: cell originally had vegetation (set when veg > NONE trampled to NONE)
#define CELL_FLAG_HAD_VEGETATION (1 << 7)

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
    VEG_REEDS,          // Reeds (waterside, harvestable)
} VegetationType;

extern uint8_t* vegetationGrid;
#define VEGETATION(x, y, z) (vegetationGrid CELL_AT((x), (y), (z)))

#define GetVegetation(x,y,z)      ((VegetationType)VEGETATION(x, y, z))
#define SetVegetation(x,y,z,v)    (VEGETATION(x, y, z) = (uint8_t)(v))

// Track connections grid — 4-bit bitmask per cell: N=1, E=2, S=4, W=8
// Stores which directions a track cell is connected to (intent-aware, not just adjacency)
extern uint8_t* trackConnections;
#define TRACK_CONN(x, y, z) (trackConnections CELL_AT((x), (y), (z)))

#define TRACK_N 1
#define TRACK_E 2
#define TRACK_S 4
#define TRACK_W 8

// Exploration grid — fog of war (0=unexplored, 1=explored)
extern uint8_t* exploredGrid;
#define EXPLORED(x, y, z) (exploredGrid CELL_AT((x), (y), (z)))

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
    return EXPLORED(x, y, z) != 0;
}

static inline void SetExplored(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return;
    EXPLORED(x, y, z) = 1;
}

// Reveal all cells within radius of a point, extending vertically through air
void RevealAroundPoint(int cx, int cy, int cz, int radius);

// Cell flag helpers
#define HAS_CELL_FLAG(x,y,z,f)     (!!(CELL_FLAGS(x, y, z) & (f)))
#define SET_CELL_FLAG(x,y,z,f)     (CELL_FLAGS(x, y, z) |= (f))
#define CLEAR_CELL_FLAG(x,y,z,f)   (CELL_FLAGS(x, y, z) &= ~(f))
#define GET_CELL_WETNESS(x,y,z)    ((CELL_FLAGS(x, y, z) & CELL_WETNESS_MASK) >> CELL_WETNESS_SHIFT)
#define SET_CELL_WETNESS(x,y,z,w)  (CELL_FLAGS(x, y, z) = (CELL_FLAGS(x, y, z) & ~CELL_WETNESS_MASK) | ((w) << CELL_WETNESS_SHIFT))
#define GET_CELL_SURFACE(x,y,z)    ((CELL_FLAGS(x, y, z) & CELL_SURFACE_MASK) >> CELL_SURFACE_SHIFT)
#define SET_CELL_SURFACE(x,y,z,s)  (CELL_FLAGS(x, y, z) = (CELL_FLAGS(x, y, z) & ~CELL_SURFACE_MASK) | ((s) << CELL_SURFACE_SHIFT))
// Floor flag helpers (for constructed floors over empty space)
#define HAS_FLOOR(x,y,z)           (!!(CELL_FLAGS(x, y, z) & CELL_FLAG_HAS_FLOOR))
#define SET_FLOOR(x,y,z)           (CELL_FLAGS(x, y, z) |= CELL_FLAG_HAS_FLOOR)
#define CLEAR_FLOOR(x,y,z)         (CELL_FLAGS(x, y, z) &= ~CELL_FLAG_HAS_FLOOR)

// Helper to check if a cell is air (empty space that can be fallen through)
static inline bool IsCellAirAt(int z, int y, int x) {
    if (z < 0 || z >= gridDepth || y < 0 || y >= gridHeight || x < 0 || x >= gridWidth) return false;
    return GRID(x, y, z) == CELL_AIR;
}

// Note: IsLadderCell, IsWallCell, IsCellWalkableAt are defined in cell_defs.h
// Include cell_defs.h after grid.h to use them

// Initialize grid with specific dimensions and default chunk size (16x16)
void InitGridWithSize(int width, int height);

// Initialize grid with specific dimensions and chunk size
void InitGridWithSizeAndChunkSize(int width, int height, int chunkW, int chunkH);
// Explicit depth (1..MAX_GRID_DEPTH). The variants above always use MAX_GRID_DEPTH.
void InitGridWithDimensions(int width, int height, int depth, int chunkW, int chunkH);

// Sparse per-cell layers (step 3): one block per 16x16 chunk column (all z, 4096 cells), only
// for chunks that hold something. A missing block reads as the layer's empty value. Read with
// SparseLayerRead (const; never allocates), write with SparseLayerWrite (allocates the block,
// filled with the empty value). Blocks are freed by SparseLayerClear and when the map size changes.
typedef struct {
    void** blocks;          // [storeChunksX * storeChunksY], NULL = all empty
    int blockCount;
    size_t elemSize;
    const void* empty;      // the empty value (elemSize bytes)
    int allocatedBlocks;    // for reports
    const char* name;
} SparseLayer;

static inline int SparseChunkOf(int x, int y) {
    return (y >> STORE_CHUNK_SHIFT) * storeChunksX + (x >> STORE_CHUNK_SHIFT);
}
static inline int SparseCellOf(int x, int y, int z) {
    return (z << 8) | ((y & STORE_MASK) << STORE_CHUNK_SHIFT) | (x & STORE_MASK);
}
static inline const void* SparseLayerRead(const SparseLayer* L, int x, int y, int z) {
    const unsigned char* b = (const unsigned char*)L->blocks[SparseChunkOf(x, y)];
    return b ? b + (size_t)SparseCellOf(x, y, z) * L->elemSize : L->empty;
}
void* SparseLayerAllocBlock(SparseLayer* L, int chunk);
static inline void* SparseLayerWrite(SparseLayer* L, int x, int y, int z) {
    int c = SparseChunkOf(x, y);
    unsigned char* b = (unsigned char*)L->blocks[c];
    if (!b) b = (unsigned char*)SparseLayerAllocBlock(L, c);
    return b + (size_t)SparseCellOf(x, y, z) * L->elemSize;
}
void SparseLayerClear(SparseLayer* L);   // free every block: all cells empty

// Row sweep over a sparse layer in plain x order: FOR_X_IN_SPARSE_ROW(layer, T, x, p, y, z) { ... }
// visits only x whose chunk has a block, with T* p pointing at the cell (writable). Runs in
// empty chunks are skipped entirely, so use it only where the loop body does nothing for an
// empty cell. `continue` and `return` work; `break` doesn't (leaves only the current cell).
#define FOR_X_IN_SPARSE_ROW(L, T, x, p, y, z) \
    for (int x##RunStart_ = 0; x##RunStart_ < gridWidth; x##RunStart_ += STORE_CHUNK) \
        for (T* x##Block_ = (T*)(L).blocks[SparseChunkOf(x##RunStart_, (y))]; x##Block_; x##Block_ = NULL) \
            for (int x = x##RunStart_, x##RunEnd_ = (x##RunStart_ + STORE_CHUNK < gridWidth ? \
                     x##RunStart_ + STORE_CHUNK : gridWidth); x < x##RunEnd_; x++) \
                for (T* p = x##Block_ + SparseCellOf(x, (y), (z)); p; p = NULL)

// Clear a whole layer (gridLayerCells elements) to fillByte. The rest of the static array was never written, so it is
// still zero and its pages are never touched: a small map in a big-cap build does not pay for it.
// ClearGridPlane does the same for a [MAX_GRID_HEIGHT][MAX_GRID_WIDTH] scratch plane (used rows
// and columns plus a one-cell margin).
void ClearGridLayer(void* layer, size_t elemSize, int fillByte);
void ClearGridPlane(void* plane, size_t elemSize, int fillByte);
// Same region, but every element set to *pattern (elemSize bytes), for non-byte fills.
void FillGridLayer(void* layer, size_t elemSize, const void* pattern);
#define FILL_GRID_LAYER(arr, patternPtr) FillGridLayer((arr), sizeof((arr)[0]), (patternPtr))
#define CLEAR_GRID_LAYER(arr, fillByte) ClearGridLayer((arr), sizeof((arr)[0]), (fillByte))
#define CLEAR_GRID_PLANE(arr, fillByte) ClearGridPlane((arr), sizeof((arr)[0][0]), (fillByte))

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
