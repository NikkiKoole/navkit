#include "grid.h"
#include "pathfinding.h"
#include <string.h>
#include <stdio.h>

CellType grid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
uint8_t cellFlags[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
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

    // Clear cell flags
    memset(cellFlags, 0, sizeof(cellFlags));

    needsRebuild = true;
    jpsNeedsRebuild = true;
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

// ============== LADDER PLACEMENT/ERASURE ==============

// Forward declarations for cascade functions
static void CascadeBreakDown(int x, int y, int z);
static void CascadeBreakUp(int x, int y, int z);

static bool CanReceiveFromBelow(CellType c) {
    return c == CELL_LADDER_DOWN || c == CELL_LADDER_BOTH || c == CELL_LADDER;
}

static bool CanReceiveFromAbove(CellType c) {
    return c == CELL_LADDER_UP || c == CELL_LADDER_BOTH || c == CELL_LADDER;
}

static bool IsEmptyCell(CellType c) {
    return c == CELL_AIR || c == CELL_WALKABLE || c == CELL_GRASS || 
           c == CELL_DIRT || c == CELL_FLOOR;
}

void RecalculateLadderColumn(int x, int y) {
    for (int z = 0; z < gridDepth; z++) {
        if (!IsLadderCell(grid[z][y][x])) continue;
        
        bool up = (z + 1 < gridDepth) && CanReceiveFromBelow(grid[z+1][y][x]);
        bool down = (z > 0) && CanReceiveFromAbove(grid[z-1][y][x]);
        
        CellType newType = up ? (down ? CELL_LADDER_BOTH : CELL_LADDER_UP) 
                              : (down ? CELL_LADDER_DOWN : CELL_LADDER_UP);
        
        if (grid[z][y][x] != newType) {
            grid[z][y][x] = newType;
            MarkChunkDirty(x, y, z);
        }
    }
}

void PlaceLadder(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return;
    
    CellType current = grid[z][y][x];
    if (current == CELL_WALL) return;
    
    // If already a ladder, only extend if clicking on the TOP piece (DOWN)
    if (IsLadderCell(current)) {
        if (current == CELL_LADDER_DOWN && z + 1 < gridDepth && IsEmptyCell(grid[z+1][y][x])) {
            grid[z + 1][y][x] = CELL_LADDER_DOWN;
            MarkChunkDirty(x, y, z + 1);
            RecalculateLadderColumn(x, y);
        }
        return;
    }
    
    CellType above = (z + 1 < gridDepth) ? grid[z+1][y][x] : CELL_WALL;
    CellType below = (z > 0) ? grid[z-1][y][x] : CELL_WALL;
    
    bool connectAbove = CanReceiveFromBelow(above);
    bool connectBelow = CanReceiveFromAbove(below);
    bool extendDown = (above == CELL_LADDER_UP);
    
    if (connectAbove && connectBelow) {
        grid[z][y][x] = CELL_LADDER_BOTH;
    } else if (connectAbove) {
        grid[z][y][x] = CELL_LADDER_DOWN;
    } else if (extendDown) {
        grid[z][y][x] = CELL_LADDER_UP;
    } else if (connectBelow) {
        grid[z][y][x] = CELL_LADDER_UP;
        if (z + 1 < gridDepth && IsEmptyCell(above)) {
            grid[z+1][y][x] = CELL_LADDER_DOWN;
            MarkChunkDirty(x, y, z+1);
        }
    } else {
        // New shaft - UP here, DOWN above if room
        grid[z][y][x] = CELL_LADDER_UP;
        if (z + 1 < gridDepth && IsEmptyCell(above)) {
            grid[z+1][y][x] = CELL_LADDER_DOWN;
            MarkChunkDirty(x, y, z+1);
        }
        MarkChunkDirty(x, y, z);
        return;  // Don't recalculate - would incorrectly merge separate shafts
    }
    MarkChunkDirty(x, y, z);
    RecalculateLadderColumn(x, y);
}

// Called when the cell below lost its upward connection - iterates upward
static void CascadeBreakDown(int x, int y, int z) {
    while (z >= 0 && z < gridDepth) {
        CellType cell = grid[z][y][x];
        if (!IsLadderCell(cell)) return;
        
        MarkChunkDirty(x, y, z);
        
        if (cell == CELL_LADDER_BOTH) {
            // Lost connection below: BOTH → UP
            grid[z][y][x] = CELL_LADDER_UP;
            return;  // BOTH→UP doesn't cascade further
        } else if (cell == CELL_LADDER_DOWN) {
            // DOWN with no connection below: remove and continue up
            grid[z][y][x] = (z > 0) ? CELL_AIR : CELL_WALKABLE;
            z++;  // Continue upward
        } else {
            // UP stays UP (still has its upward connection)
            return;
        }
    }
}

// Called when the cell above lost its downward connection - iterates downward
static void CascadeBreakUp(int x, int y, int z) {
    while (z >= 0 && z < gridDepth) {
        CellType cell = grid[z][y][x];
        if (!IsLadderCell(cell)) return;
        
        MarkChunkDirty(x, y, z);
        
        if (cell == CELL_LADDER_BOTH) {
            // Lost connection above: BOTH → DOWN
            grid[z][y][x] = CELL_LADDER_DOWN;
            return;  // BOTH→DOWN doesn't cascade further
        } else if (cell == CELL_LADDER_UP) {
            // UP with no connection above: remove and continue down
            grid[z][y][x] = (z > 0) ? CELL_AIR : CELL_WALKABLE;
            z--;  // Continue downward
        } else {
            // DOWN stays DOWN (still has its downward connection)
            return;
        }
    }
}

void EraseLadder(int x, int y, int z) {
    // Bounds check
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return;
    
    CellType cell = grid[z][y][x];
    if (!IsLadderCell(cell)) return;
    
    MarkChunkDirty(x, y, z);
    
    if (cell == CELL_LADDER_BOTH) {
        // Break upward connection: BOTH → DOWN
        grid[z][y][x] = CELL_LADDER_DOWN;
        // Cascade upward: break the cell above's downward connection
        CascadeBreakDown(x, y, z + 1);
        
    } else if (cell == CELL_LADDER_UP) {
        // Remove completely, cascade upward
        grid[z][y][x] = (z > 0) ? CELL_AIR : CELL_WALKABLE;
        CascadeBreakDown(x, y, z + 1);
        
    } else if (cell == CELL_LADDER_DOWN) {
        // Remove completely, cascade downward
        grid[z][y][x] = (z > 0) ? CELL_AIR : CELL_WALKABLE;
        CascadeBreakUp(x, y, z - 1);
    }
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
                    grid[currentFloor][y][x] = CELL_LADDER;  // Legacy
                } else if (*p == '<') {
                    grid[currentFloor][y][x] = CELL_LADDER_UP;
                } else if (*p == '>') {
                    grid[currentFloor][y][x] = CELL_LADDER_DOWN;
                } else if (*p == 'X') {
                    grid[currentFloor][y][x] = CELL_LADDER_BOTH;
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
