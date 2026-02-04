#include "grid.h"
#include "cell_defs.h"
#include "material.h"
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

    // Clear the grid (all z-levels), cell flags, and materials
    memset(cellFlags, 0, sizeof(cellFlags));
    InitMaterials();
    
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                // All air (z=0 walkable via implicit bedrock)
                grid[z][y][x] = CELL_AIR;
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
                    grid[0][y][x] = CELL_AIR;
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
    return c == CELL_LADDER_DOWN || c == CELL_LADDER_BOTH || c == CELL_LADDER_BOTH;
}

static bool CanReceiveFromAbove(CellType c) {
    return c == CELL_LADDER_UP || c == CELL_LADDER_BOTH || c == CELL_LADDER_BOTH;
}

static bool IsEmptyCell(CellType c) {
    return c == CELL_AIR || c == CELL_DIRT;
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
            grid[z][y][x] = CELL_AIR;
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
            grid[z][y][x] = CELL_AIR;
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
        grid[z][y][x] = CELL_AIR;
        CascadeBreakDown(x, y, z + 1);
        
    } else if (cell == CELL_LADDER_DOWN) {
        // Remove completely, cascade downward
        grid[z][y][x] = CELL_AIR;
        CascadeBreakUp(x, y, z - 1);
    }
}

// ============== RAMP PLACEMENT/ERASURE ==============
//
// NOTE: Our ramps use explicit directions (CELL_RAMP_N/E/S/W). Dwarf Fortress
// uses omnidirectional ramps where a single ramp tile can connect to multiple
// z+1 exits simultaneously (any adjacent wall with walkable space above).
// 
// We chose directional ramps for simpler HPA*/JPS+ graph handling:
// - 1 ramp = 1 RampLink = 2 entrances (predictable)
// - DF model would need N RampLinks per ramp (up to 4)
// - DF model requires rebuilding ramp links when any neighbor wall changes
//
// We auto-detect the direction at placement time based on terrain.
// If multiple directions are valid, we pick the first valid one (N→E→S→W).

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
    // OR there should be a ramp below that exits here (diagonal staircase)
    int lowX = x - highDx;
    int lowY = y - highDy;
    if (lowX >= 0 && lowX < gridWidth && lowY >= 0 && lowY < gridHeight) {
        bool lowSideWalkable = IsCellWalkableAt(z, lowY, lowX);
        
        // Check if there's a ramp below that provides access via its exit
        // A ramp at z-1 with same direction would exit at this ramp's low side at z
        bool rampBelowProvideAccess = false;
        if (z > 0) {
            CellType cellBelow = grid[z-1][lowY][lowX];
            if (CellIsRamp(cellBelow)) {
                // Check if the ramp below exits toward this cell
                int belowHighDx, belowHighDy;
                GetRampHighSideOffset(cellBelow, &belowHighDx, &belowHighDy);
                int belowExitX = lowX + belowHighDx;
                int belowExitY = lowY + belowHighDy;
                // If the ramp below's exit is at our position (x, y), allow it
                if (belowExitX == x && belowExitY == y) {
                    rampBelowProvideAccess = true;
                }
            }
        }
        
        if (!lowSideWalkable && !rampBelowProvideAccess) return false;
    }
    
    return true;
}

CellType AutoDetectRampDirection(int x, int y, int z) {
    // Try each direction in priority order: N → E → S → W
    // Returns the first valid direction, or CELL_AIR if none valid
    if (CanPlaceRamp(x, y, z, CELL_RAMP_N)) return CELL_RAMP_N;
    if (CanPlaceRamp(x, y, z, CELL_RAMP_E)) return CELL_RAMP_E;
    if (CanPlaceRamp(x, y, z, CELL_RAMP_S)) return CELL_RAMP_S;
    if (CanPlaceRamp(x, y, z, CELL_RAMP_W)) return CELL_RAMP_W;
    return CELL_AIR;  // No valid direction
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
    
    grid[z][y][x] = CELL_AIR;
    
    rampCount--;
    MarkChunkDirty(x, y, z);
}

// Check if an existing ramp is still valid
// A ramp is valid if its high side (exit at z+1) has solid ground below it
bool IsRampStillValid(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return false;
    
    CellType cell = grid[z][y][x];
    if (!CellIsDirectionalRamp(cell)) return false;
    
    // Get the exit position (high side at z+1)
    int highDx, highDy;
    GetRampHighSideOffset(cell, &highDx, &highDy);
    int exitX = x + highDx;
    int exitY = y + highDy;
    int exitZ = z + 1;
    
    // Check bounds for exit
    if (exitX < 0 || exitX >= gridWidth || exitY < 0 || exitY >= gridHeight || exitZ >= gridDepth) {
        return false;
    }
    
    // The exit cell must have solid support below it (at z, the same level as the ramp)
    // This means the cell adjacent to the ramp in the high direction must be solid OR a ramp
    CellType exitBase = grid[z][exitY][exitX];
    
    // Valid if the exit has solid ground at the same z-level as the ramp
    // (wall, dirt, etc. - something you can stand on top of)
    if (CellIsSolid(exitBase)) return true;
    
    // Also valid if exit base is a ramp - ramp-to-ramp connections are allowed
    // When the exit base is a ramp, you can stand on it at z+1 (the exit level)
    if (CellIsRamp(exitBase)) return true;
    
    return false;
}

// Remove a ramp and convert to floor, spawning debris
static void RemoveInvalidRamp(int x, int y, int z) {
    CellType cell = grid[z][y][x];
    if (!CellIsDirectionalRamp(cell)) return;
    
    // Convert to floor
    grid[z][y][x] = CELL_AIR;
    SET_FLOOR(x, y, z);
    
    rampCount--;
    MarkChunkDirty(x, y, z);
}

// Validate and cleanup invalid ramps in a region
// Call this after terrain changes (channeling, mining) that might invalidate ramps
int ValidateAndCleanupRamps(int minX, int minY, int minZ, int maxX, int maxY, int maxZ) {
    int removedCount = 0;
    
    // Clamp bounds
    if (minX < 0) minX = 0;
    if (minY < 0) minY = 0;
    if (minZ < 0) minZ = 0;
    if (maxX >= gridWidth) maxX = gridWidth - 1;
    if (maxY >= gridHeight) maxY = gridHeight - 1;
    if (maxZ >= gridDepth) maxZ = gridDepth - 1;
    
    // Multiple passes may be needed since removing one ramp might invalidate another
    // (e.g., ramp A's exit was on top of ramp B, removing B invalidates A)
    bool changed = true;
    while (changed) {
        changed = false;
        for (int z = minZ; z <= maxZ; z++) {
            for (int y = minY; y <= maxY; y++) {
                for (int x = minX; x <= maxX; x++) {
                    CellType cell = grid[z][y][x];
                    if (CellIsDirectionalRamp(cell) && !IsRampStillValid(x, y, z)) {
                        RemoveInvalidRamp(x, y, z);
                        removedCount++;
                        changed = true;
                    }
                }
            }
        }
    }
    
    return removedCount;
}

// Validate all ramps in the entire grid
int ValidateAllRamps(void) {
    return ValidateAndCleanupRamps(0, 0, 0, gridWidth - 1, gridHeight - 1, gridDepth - 1);
}

int InitGridFromAscii(const char* ascii) {
    return InitGridFromAsciiWithChunkSize(ascii, DEFAULT_CHUNK_SIZE, DEFAULT_CHUNK_SIZE);
}

int InitMultiFloorGridFromAscii(const char* ascii, int chunkW, int chunkH) {
    // Parse format: "floor:0\n...\nfloor:1\n..."
    // floor:0 becomes solid ground (CELL_DIRT), floor:1+ becomes walkable air with HAS_FLOOR flag
    
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
                    grid[currentFloor][y][x] = CELL_LADDER_BOTH;  // Legacy
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
                    // All floors are CELL_AIR
                    //   - floor 0 is walkable via implicit bedrock at z=-1
                    //   - floor 1+ gets HAS_FLOOR flag for walkability
                    grid[currentFloor][y][x] = CELL_AIR;
                    if (currentFloor > 0) {
                        SET_FLOOR(x, y, currentFloor);  // Constructed floor for walkability
                    }
                }
            }
            x++;
        }
        p++;
    }
    
    return 1;
}
