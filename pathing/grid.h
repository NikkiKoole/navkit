#ifndef GRID_H
#define GRID_H

#include <stdbool.h>

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
extern int chunksY;

// For static array sizing
#define MAX_CHUNKS_X (MAX_GRID_WIDTH / 8)   // minimum chunk size of 8
#define MAX_CHUNKS_Y (MAX_GRID_HEIGHT / 8)

typedef enum { 
    CELL_WALKABLE, 
    CELL_WALL, 
    CELL_LADDER,       // Legacy: alias for CELL_LADDER_BOTH
    CELL_AIR, 
    CELL_FLOOR,
    CELL_LADDER_UP,    // Bottom of ladder - can climb UP from here
    CELL_LADDER_DOWN,  // Top of ladder - can climb DOWN from here  
    CELL_LADDER_BOTH   // Middle of ladder - can go both directions
} CellType;

extern CellType grid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Helper to check if a cell is any ladder type
static inline bool IsLadderCell(CellType cell) {
    return cell == CELL_LADDER || cell == CELL_LADDER_UP || 
           cell == CELL_LADDER_DOWN || cell == CELL_LADDER_BOTH;
}

// Helper to check if a cell is walkable (includes floor and all ladder types)
static inline bool IsCellWalkableAt(int z, int y, int x) {
    if (z < 0 || z >= gridDepth || y < 0 || y >= gridHeight || x < 0 || x >= gridWidth) return false;
    CellType cell = grid[z][y][x];
    return cell == CELL_WALKABLE || cell == CELL_FLOOR || IsLadderCell(cell);
}

// Helper to check if a cell is air (empty space that can be fallen through)
static inline bool IsCellAirAt(int z, int y, int x) {
    if (z < 0 || z >= gridDepth || y < 0 || y >= gridHeight || x < 0 || x >= gridWidth) return false;
    return grid[z][y][x] == CELL_AIR;
}
extern bool needsRebuild;      // Generic flag (legacy)
extern bool hpaNeedsRebuild;   // HPA* specific rebuild flag
extern bool jpsNeedsRebuild;   // JPS+ specific rebuild flag

// Initialize grid with specific dimensions and default chunk size (16x16)
void InitGridWithSize(int width, int height);

// Initialize grid with specific dimensions and chunk size
void InitGridWithSizeAndChunkSize(int width, int height, int chunkW, int chunkH);

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

#endif // GRID_H
