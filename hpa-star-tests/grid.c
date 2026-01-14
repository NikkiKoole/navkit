#include "grid.h"

CellType grid[MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
bool needsRebuild = false;

// Runtime dimensions - default to max
int gridWidth = MAX_GRID_WIDTH;
int gridHeight = MAX_GRID_HEIGHT;
int chunksX = MAX_CHUNKS_X;
int chunksY = MAX_CHUNKS_Y;

void InitGridWithSize(int width, int height) {
    // Clamp to max dimensions
    if (width > MAX_GRID_WIDTH) width = MAX_GRID_WIDTH;
    if (height > MAX_GRID_HEIGHT) height = MAX_GRID_HEIGHT;
    // Ensure dimensions are multiples of CHUNK_SIZE
    width = (width / CHUNK_SIZE) * CHUNK_SIZE;
    height = (height / CHUNK_SIZE) * CHUNK_SIZE;
    if (width < CHUNK_SIZE) width = CHUNK_SIZE;
    if (height < CHUNK_SIZE) height = CHUNK_SIZE;
    
    gridWidth = width;
    gridHeight = height;
    chunksX = gridWidth / CHUNK_SIZE;
    chunksY = gridHeight / CHUNK_SIZE;
    
    // Clear the grid
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++)
            grid[y][x] = CELL_WALKABLE;
    
    needsRebuild = true;
}
