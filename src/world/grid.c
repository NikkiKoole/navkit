#include "grid.h"
#include "cell_defs.h"
#include "pathfinding.h"
#include <string.h>
#include <stdio.h>

CellType grid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
uint8_t cellFlags[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
bool needsRebuild = false;
bool hpaNeedsRebuild = false;
bool jpsNeedsRebuild = false;
bool g_legacyWalkability = false;  // false = standard walkability, true = legacy

// Runtime dimensions - default to max
int gridWidth = MAX_GRID_WIDTH;
int gridHeight = MAX_GRID_HEIGHT;
int gridDepth = MAX_GRID_DEPTH;
int chunkWidth = DEFAULT_CHUNK_SIZE;
int chunkHeight = DEFAULT_CHUNK_SIZE;
int chunksX = MAX_GRID_WIDTH / DEFAULT_CHUNK_SIZE;
int rampCount = 0;
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
    rampCount = 0;

    // Clear the grid (all z-levels) and cell flags
    memset(cellFlags, 0, sizeof(cellFlags));
    
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                // In standard mode: all air (z=0 walkable via implicit bedrock)
                // In legacy mode: z=0 walkable, higher levels air
                if (g_legacyWalkability) {
                    grid[z][y][x] = (z == 0) ? CELL_WALKABLE : CELL_AIR;
                } else {
                    grid[z][y][x] = CELL_AIR;
                }
            }
        }
    }

    needsRebuild = true;
    jpsNeedsRebuild = true;
}

void InitGridWithSize(int width, int height) {
    InitGridWithSizeAndChunkSize(width, height, DEFAULT_CHUNK_SIZE, DEFAULT_CHUNK_SIZE);
}

void FillGroundLevel(void) {
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            grid[0][y][x] = CELL_DIRT;
            SET_CELL_SURFACE(x, y, 0, SURFACE_TALL_GRASS);
        }
    }
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
                    // Use CELL_AIR in DF mode, CELL_WALKABLE in legacy mode
                    grid[0][y][x] = g_legacyWalkability ? CELL_WALKABLE : CELL_AIR;
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
    if (IsWallCell(current)) return;
    
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
            grid[z][y][x] = (!g_legacyWalkability || z > 0) ? CELL_AIR : CELL_WALKABLE;
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
            grid[z][y][x] = (!g_legacyWalkability || z > 0) ? CELL_AIR : CELL_WALKABLE;
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
        grid[z][y][x] = (!g_legacyWalkability || z > 0) ? CELL_AIR : CELL_WALKABLE;
        CascadeBreakDown(x, y, z + 1);
        
    } else if (cell == CELL_LADDER_DOWN) {
        // Remove completely, cascade downward
        grid[z][y][x] = (!g_legacyWalkability || z > 0) ? CELL_AIR : CELL_WALKABLE;
        CascadeBreakUp(x, y, z - 1);
    }
}

// ============== RAMP PLACEMENT/ERASURE ==============

// Forward declaration for push functions (defined in mover.c and items.c)
extern void PushMoversOutOfCell(int x, int y, int z);
extern void PushItemsOutOfCell(int x, int y, int z);

bool CanPlaceRamp(int x, int y, int z, CellType rampType) {
    // Bounds check
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return false;
    
    // Current cell must be walkable (not a wall)
    if (!IsCellWalkableAt(z, y, x)) return false;
    
    // Current cell must not already be a ramp or ladder
    CellType current = grid[z][y][x];
    if (CellIsDirectionalRamp(current) || CellIsLadder(current)) return false;
    
    // Get high side offset
    int highDx, highDy;
    GetRampHighSideOffset(rampType, &highDx, &highDy);
    int exitX = x + highDx;
    int exitY = y + highDy;
    
    // Check z+1 bounds
    if (z + 1 >= gridDepth) return false;
    
    // Check exit tile bounds
    if (exitX < 0 || exitX >= gridWidth || exitY < 0 || exitY >= gridHeight) return false;
    
    // Exit tile at z+1 must be walkable
    if (!IsCellWalkableAt(z + 1, exitY, exitX)) return false;
    
    // Low side at same z should be walkable (so you can enter the ramp)
    int lowX = x - highDx;
    int lowY = y - highDy;
    if (lowX >= 0 && lowX < gridWidth && lowY >= 0 && lowY < gridHeight) {
        if (!IsCellWalkableAt(z, lowY, lowX)) return false;
    }
    
    return true;
}

void PlaceRamp(int x, int y, int z, CellType rampType) {
    if (!CanPlaceRamp(x, y, z, rampType)) return;
    
    // Push movers and items out of the cell
    PushMoversOutOfCell(x, y, z);
    PushItemsOutOfCell(x, y, z);
    
    // Place the ramp
    grid[z][y][x] = rampType;
    rampCount++;
    MarkChunkDirty(x, y, z);
}

void EraseRamp(int x, int y, int z) {
    // Bounds check
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return;
    
    CellType cell = grid[z][y][x];
    if (!CellIsDirectionalRamp(cell)) return;
    
    // Replace with appropriate floor type
    if (g_legacyWalkability) {
        grid[z][y][x] = CELL_WALKABLE;
    } else {
        grid[z][y][x] = CELL_AIR;
    }
    
    rampCount--;
    MarkChunkDirty(x, y, z);
}

int InitGridFromAscii(const char* ascii) {
    return InitGridFromAsciiWithChunkSize(ascii, DEFAULT_CHUNK_SIZE, DEFAULT_CHUNK_SIZE);
}

int InitMultiFloorGridFromAscii(const char* ascii, int chunkW, int chunkH) {
    // Parse format: "floor:0\n...\nfloor:1\n..."
    // In standard mode: floor:0 becomes solid ground (CELL_DIRT), floor:1+ becomes walkable air
    // In legacy mode: all floors use CELL_WALKABLE for '.' cells
    
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
                } else if (*p == 'N') {
                    grid[currentFloor][y][x] = CELL_RAMP_N;
                    rampCount++;
                } else if (*p == 'E') {
                    grid[currentFloor][y][x] = CELL_RAMP_E;
                    rampCount++;
                } else if (*p == 'S') {
                    grid[currentFloor][y][x] = CELL_RAMP_S;
                    rampCount++;
                } else if (*p == 'W') {
                    grid[currentFloor][y][x] = CELL_RAMP_W;
                    rampCount++;
                } else {
                    // In standard mode: all floors are CELL_AIR
                    //   - floor 0 is walkable via implicit bedrock at z=-1
                    //   - floor 1+ gets HAS_FLOOR flag for walkability
                    // In legacy mode: all floors use CELL_WALKABLE
                    if (g_legacyWalkability) {
                        grid[currentFloor][y][x] = CELL_WALKABLE;
                    } else {
                        grid[currentFloor][y][x] = CELL_AIR;
                        if (currentFloor > 0) {
                            SET_FLOOR(x, y, currentFloor);  // Constructed floor for walkability
                        }
                    }
                }
            }
            x++;
        }
        p++;
    }
    
    return 1;
}
