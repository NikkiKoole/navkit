#include "grid.h"
#include <string.h>

CellType grid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
bool needsRebuild = false;
bool hpaNeedsRebuild = false;
bool jpsNeedsRebuild = false;

// Runtime dimensions - default to max
int gridWidth = MAX_GRID_WIDTH;
int gridHeight = MAX_GRID_HEIGHT;
int gridDepth = MAX_GRID_DEPTH;
int chunkWidth = DEFAULT_CHUNK_SIZE;
int chunkHeight = DEFAULT_CHUNK_SIZE;
int chunksX = MAX_GRID_WIDTH / DEFAULT_CHUNK_SIZE;
int chunksY = MAX_GRID_HEIGHT / DEFAULT_CHUNK_SIZE;

void InitGridWithSizeAndChunkSize(int width, int height, int chunkW, int chunkH) {
    // Clamp to max dimensions
    if (width > MAX_GRID_WIDTH) width = MAX_GRID_WIDTH;
    if (height > MAX_GRID_HEIGHT) height = MAX_GRID_HEIGHT;
    if (width < 1) width = 1;
    if (height < 1) height = 1;

    // Clamp chunk size
    if (chunkW < 1) chunkW = width;
    if (chunkH < 1) chunkH = height;
    if (chunkW > width) chunkW = width;
    if (chunkH > height) chunkH = height;

    gridWidth = width;
    gridHeight = height;
    gridDepth = MAX_GRID_DEPTH;  // Always use max depth for now
    chunkWidth = chunkW;
    chunkHeight = chunkH;
    chunksX = (gridWidth + chunkWidth - 1) / chunkWidth;   // ceiling division
    chunksY = (gridHeight + chunkHeight - 1) / chunkHeight;

    // Clear the grid (all z-levels)
    for (int z = 0; z < gridDepth; z++)
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++)
                grid[z][y][x] = CELL_WALKABLE;

    needsRebuild = true;
}

void InitGridWithSize(int width, int height) {
    InitGridWithSizeAndChunkSize(width, height, DEFAULT_CHUNK_SIZE, DEFAULT_CHUNK_SIZE);
}

int InitGridFromAsciiWithChunkSize(const char* ascii, int chunkW, int chunkH) {
    // First pass: find dimensions
    int width = 0;
    int height = 0;
    int currentWidth = 0;

    for (const char* p = ascii; *p; p++) {
        if (*p == '\n') {
            if (currentWidth > width) width = currentWidth;
            if (currentWidth > 0) height++;
            currentWidth = 0;
        } else {
            currentWidth++;
        }
    }
    // Handle last line without newline
    if (currentWidth > 0) {
        if (currentWidth > width) width = currentWidth;
        height++;
    }

    if (width == 0 || height == 0) return 0;

    // If chunk size is 0, use grid dimensions (1 chunk = whole grid)
    if (chunkW <= 0) chunkW = width;
    if (chunkH <= 0) chunkH = height;

    // Initialize grid with these dimensions
    InitGridWithSizeAndChunkSize(width, height, chunkW, chunkH);

    // Second pass: fill grid (z=0 layer)
    int x = 0, y = 0;
    for (const char* p = ascii; *p; p++) {
        if (*p == '\n') {
            x = 0;
            y++;
        } else {
            if (x < gridWidth && y < gridHeight) {
                if (*p == '#') {
                    grid[0][y][x] = CELL_WALL;
                } else {
                    grid[0][y][x] = CELL_WALKABLE;
                }
            }
            x++;
        }
    }

    return 1;
}

int InitGridFromAscii(const char* ascii) {
    return InitGridFromAsciiWithChunkSize(ascii, DEFAULT_CHUNK_SIZE, DEFAULT_CHUNK_SIZE);
}

int InitMultiFloorGridFromAscii(const char* ascii, int chunkW, int chunkH) {
    // Parse format: "floor:0\n...\nfloor:1\n..."
    
    // First pass: find dimensions and floor count
    int width = 0;
    int height = 0;
    int maxFloor = -1;
    int currentWidth = 0;
    int currentHeight = 0;
    int currentFloor = -1;
    
    const char* p = ascii;
    while (*p) {
        // Check for "floor:N" marker
        if (strncmp(p, "floor:", 6) == 0) {
            // Save height from previous floor
            if (currentFloor >= 0 && currentHeight > height) {
                height = currentHeight;
            }
            currentHeight = 0;
            
            // Parse floor number
            p += 6;
            currentFloor = 0;
            while (*p >= '0' && *p <= '9') {
                currentFloor = currentFloor * 10 + (*p - '0');
                p++;
            }
            if (currentFloor > maxFloor) maxFloor = currentFloor;
            // Skip newline after floor marker
            if (*p == '\n') p++;
            continue;
        }
        
        if (*p == '\n') {
            if (currentWidth > width) width = currentWidth;
            if (currentWidth > 0) currentHeight++;
            currentWidth = 0;
        } else {
            currentWidth++;
        }
        p++;
    }
    // Handle last line without newline
    if (currentWidth > 0) {
        if (currentWidth > width) width = currentWidth;
        currentHeight++;
    }
    // Save height from last floor
    if (currentFloor >= 0 && currentHeight > height) {
        height = currentHeight;
    }
    
    if (width == 0 || height == 0 || maxFloor < 0) return 0;
    if (maxFloor >= MAX_GRID_DEPTH) return 0;
    
    // If chunk size is 0, use grid dimensions
    if (chunkW <= 0) chunkW = width;
    if (chunkH <= 0) chunkH = height;
    
    // Initialize grid
    InitGridWithSizeAndChunkSize(width, height, chunkW, chunkH);
    gridDepth = maxFloor + 1;
    
    // Second pass: fill grid
    p = ascii;
    currentFloor = -1;
    int x = 0, y = 0;
    
    while (*p) {
        // Check for "floor:N" marker
        if (strncmp(p, "floor:", 6) == 0) {
            p += 6;
            currentFloor = 0;
            while (*p >= '0' && *p <= '9') {
                currentFloor = currentFloor * 10 + (*p - '0');
                p++;
            }
            if (*p == '\n') p++;
            x = 0;
            y = 0;
            continue;
        }
        
        if (*p == '\n') {
            x = 0;
            y++;
        } else if (currentFloor >= 0) {
            if (x < gridWidth && y < gridHeight && currentFloor < gridDepth) {
                if (*p == '#') {
                    grid[currentFloor][y][x] = CELL_WALL;
                } else if (*p == 'L') {
                    grid[currentFloor][y][x] = CELL_LADDER;
                } else {
                    grid[currentFloor][y][x] = CELL_WALKABLE;
                }
            }
            x++;
        }
        p++;
    }
    
    return 1;
}
