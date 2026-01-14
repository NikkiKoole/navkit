#ifndef GRID_H
#define GRID_H

#include <stdbool.h>

// Maximum grid dimensions (for static array allocation)
#define MAX_GRID_WIDTH  (128*4)
#define MAX_GRID_HEIGHT (128*4)
#define CHUNK_SIZE  32

// Runtime grid dimensions (can be set smaller for testing)
extern int gridWidth;
extern int gridHeight;

// Derived runtime values
#define MAX_CHUNKS_X (MAX_GRID_WIDTH / CHUNK_SIZE)
#define MAX_CHUNKS_Y (MAX_GRID_HEIGHT / CHUNK_SIZE)
extern int chunksX;
extern int chunksY;

typedef enum { CELL_WALKABLE, CELL_WALL } CellType;

extern CellType grid[MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
extern bool needsRebuild;

// Initialize grid with specific dimensions (up to MAX)
void InitGridWithSize(int width, int height);

#endif // GRID_H
