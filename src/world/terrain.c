#include "terrain.h"
#include "../../vendor/raylib.h"
#include "../entities/workshops.h"
#include "../entities/stockpiles.h"
#include "../entities/items.h"
#include "designations.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

// Simple Perlin-like noise
static int permutation[512];

// Room structure for dungeon generators
typedef struct {
    int x, y, w, h;
} Room;

void InitGrid(void) {
    // Re-initialize grid with current dimensions (clears cells and flags)
    InitGridWithSizeAndChunkSize(gridWidth, gridHeight, chunkWidth, chunkHeight);
}

void GenerateSparse(float density) {
    InitGrid();
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++)
            if ((float)GetRandomValue(0, 100) / 100.0f < density)
                grid[1][y][x] = CELL_WALL;  // Walls at z=1 (walking level)
    needsRebuild = true;
}

// ============================================================================
// Floor Placement Helper
// In DF mode: uses CELL_AIR + HAS_FLOOR flag (for balconies/bridges)
// In legacy mode: uses CELL_FLOOR cell type
// ============================================================================
static void PlaceFloor(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return;
    if (g_legacyWalkability) {
        grid[z][y][x] = CELL_FLOOR;
    } else {
        // Standard mode: uses CELL_AIR + HAS_FLOOR flag (for balconies/bridges)
        grid[z][y][x] = CELL_AIR;
        SET_FLOOR(x, y, z);
    }
    // Clear grass/surface overlay on floors
    SET_CELL_SURFACE(x, y, z, SURFACE_BARE);
}

// Helper to check if a cell is a floor (either CELL_FLOOR or HAS_FLOOR flag)
static bool IsFloorCell(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return false;
    if (g_legacyWalkability) {
        return grid[z][y][x] == CELL_FLOOR;
    }
    return HAS_FLOOR(x, y, z) || grid[z][y][x] == CELL_FLOOR;
}

// ============================================================================
// Labyrinth3D Generator
// Creates a multi-level maze where each level has passages in different 
// orientations. Ladders are placed to force long detours - you often need
// to travel in the "wrong" direction to find the ladder to the next level.
//
// This creates pathological cases where 2D heuristics are completely wrong:
// - Start and goal may be close in XY but require traversing the entire map
// - The "obvious" nearby ladder often leads to a dead end
// ============================================================================

// Helper: Place a ladder connecting zLow and zHigh near (targetX, targetY)
// Searches within radius for a spot where both levels have floor
static void PlaceLadderNear(int targetX, int targetY, int zLow, int zHigh, int radius) {
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            int x = targetX + dx;
            int y = targetY + dy;
            if (x > 0 && x < gridWidth - 1 && y > 0 && y < gridHeight - 1) {
                if (IsFloorCell(x, y, zLow) && IsFloorCell(x, y, zHigh)) {
                    grid[zLow][y][x] = CELL_LADDER;
                    grid[zHigh][y][x] = CELL_LADDER;
                    return;
                }
            }
        }
    }
}

void GenerateLabyrinth3D(void) {
    InitGrid();  // Clear cells and flags
    if (!g_legacyWalkability) FillGroundLevel();
    
    // In DF mode, z=0 is ground (dirt), buildings start at z=1
    int baseZ = g_legacyWalkability ? 0 : 1;
    int numLevels = 4;
    int maxLevels = gridDepth - baseZ;
    if (numLevels > maxLevels) numLevels = maxLevels;
    
    // Fill labyrinth levels with walls (to carve passages)
    for (int z = baseZ; z < baseZ + numLevels; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                grid[z][y][x] = CELL_WALL;
            }
        }
    }
    
    int passageWidth = 2;
    int wallThickness = 3;
    int spacing = passageWidth + wallThickness;
    
    int z0 = baseZ + 0;
    int z1 = baseZ + 1;
    int z2 = baseZ + 2;
    int z3 = baseZ + 3;
    
    // Level 0: Horizontal passages (East-West) with west-side vertical connector
    for (int y = spacing; y < gridHeight - spacing; y += spacing) {
        for (int x = 1; x < gridWidth - 1; x++) {
            for (int w = 0; w < passageWidth && y + w < gridHeight - 1; w++) {
                PlaceFloor(x, y + w, z0);
            }
        }
    }
    int westConnectorX = gridWidth / 6;
    for (int y = 1; y < gridHeight - 1; y++) {
        for (int w = 0; w < passageWidth && westConnectorX + w < gridWidth; w++) {
            PlaceFloor(westConnectorX + w, y, z0);
        }
    }
    
    // Level 1: Vertical passages (North-South) with south-side horizontal connector
    for (int x = spacing; x < gridWidth - spacing; x += spacing) {
        for (int y = 1; y < gridHeight - 1; y++) {
            for (int w = 0; w < passageWidth && x + w < gridWidth - 1; w++) {
                PlaceFloor(x + w, y, z1);
            }
        }
    }
    int southConnectorY = gridHeight - gridHeight / 6;
    for (int x = 1; x < gridWidth - 1; x++) {
        for (int w = 0; w < passageWidth && southConnectorY + w < gridHeight; w++) {
            PlaceFloor(x, southConnectorY + w, z1);
        }
    }
    
    // Level 2: Horizontal passages (offset from level 0) with east-side vertical connector
    int offset = spacing / 2;
    for (int y = spacing + offset; y < gridHeight - spacing; y += spacing) {
        for (int x = 1; x < gridWidth - 1; x++) {
            for (int w = 0; w < passageWidth && y + w < gridHeight - 1; w++) {
                PlaceFloor(x, y + w, z2);
            }
        }
    }
    int eastConnectorX = gridWidth - gridWidth / 6;
    for (int y = 1; y < gridHeight - 1; y++) {
        for (int w = 0; w < passageWidth && eastConnectorX + w < gridWidth; w++) {
            PlaceFloor(eastConnectorX + w, y, z2);
        }
    }
    
    // Level 3: Open grid pattern (destination level)
    for (int y = 1; y < gridHeight - 1; y++) {
        for (int x = 1; x < gridWidth - 1; x++) {
            if ((y % spacing) < passageWidth || (x % spacing) < passageWidth) {
                PlaceFloor(x, y, z3);
            }
        }
    }
    
    // Place ladders to force West→South→East traversal pattern
    // z=0→z=1: West region (forces westward travel on level 0)
    PlaceLadderNear(gridWidth / 8, gridHeight / 2, z0, z1, 5);
    PlaceLadderNear(gridWidth / 5, gridHeight * 3 / 4, z0, z1, 5);
    
    // z=1→z=2: South region (forces southward travel on level 1)
    PlaceLadderNear(gridWidth / 2, gridHeight - gridHeight / 8, z1, z2, 5);
    PlaceLadderNear(gridWidth / 4, gridHeight - gridHeight / 6, z1, z2, 5);
    
    // z=2→z=3: East region (forces eastward travel on level 2)
    PlaceLadderNear(gridWidth - gridWidth / 8, gridHeight / 2, z2, z3, 5);
    PlaceLadderNear(gridWidth - gridWidth / 6, gridHeight / 4, z2, z3, 5);
    
    needsRebuild = true;
}

// ============================================================================
// Spiral3D Generator
// Creates concentric rings on each level, where the exit from each ring
// is on a different side. Combined with ladders at specific positions,
// this forces a spiral traversal pattern through the levels.
//
// Level 0: Exit NORTH
// Level 1: Exit EAST  
// Level 2: Exit SOUTH
// Level 3: Goal in center
// ============================================================================

void GenerateSpiral3D(void) {
    InitGrid();  // Clear cells and flags
    if (!g_legacyWalkability) FillGroundLevel();
    
    // In DF mode, z=0 is ground (dirt), buildings start at z=1
    int baseZ = g_legacyWalkability ? 0 : 1;
    int numLevels = 4;
    int maxLevels = gridDepth - baseZ;
    if (numLevels > maxLevels) numLevels = maxLevels;
    
    // Fill spiral levels with floor
    for (int z = baseZ; z < baseZ + numLevels; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                PlaceFloor(x, y, z);
            }
        }
    }
    
    int centerX = gridWidth / 2;
    int centerY = gridHeight / 2;
    
    // Ring parameters
    int ringSpacing = 8;
    int wallThickness = 2;
    int gapSize = 3;
    
    int numRings = (gridWidth < gridHeight ? gridWidth : gridHeight) / (2 * ringSpacing) - 1;
    if (numRings < 3) numRings = 3;
    if (numRings > 8) numRings = 8;
    
    // Build rings on levels 0, 1, 2 (relative to baseZ)
    for (int level = 0; level < 3 && level < numLevels; level++) {
        int z = baseZ + level;
        // Determine which side has the gap for this level
        // 0 = North, 1 = East, 2 = South, 3 = West
        int gapSide = level;  // Level 0: North, Level 1: East, Level 2: South
        
        for (int ring = 0; ring < numRings; ring++) {
            int ringDist = (ring + 1) * ringSpacing;
            
            int left = centerX - ringDist;
            int right = centerX + ringDist;
            int top = centerY - ringDist;
            int bottom = centerY + ringDist;
            
            // Clamp to grid
            if (left < 1) left = 1;
            if (top < 1) top = 1;
            if (right >= gridWidth - 1) right = gridWidth - 2;
            if (bottom >= gridHeight - 1) bottom = gridHeight - 2;
            
            // Calculate gap position for this side
            int gapCenter;
            
            // North wall
            gapCenter = centerX;
            for (int x = left; x <= right; x++) {
                bool isGap = (gapSide == 0) && (x >= gapCenter - gapSize/2) && (x <= gapCenter + gapSize/2);
                if (!isGap) {
                    for (int t = 0; t < wallThickness && top + t < gridHeight; t++) {
                        grid[z][top + t][x] = CELL_WALL;
                    }
                }
            }
            
            // South wall
            gapCenter = centerX;
            for (int x = left; x <= right; x++) {
                bool isGap = (gapSide == 2) && (x >= gapCenter - gapSize/2) && (x <= gapCenter + gapSize/2);
                if (!isGap) {
                    for (int t = 0; t < wallThickness && bottom - t >= 0; t++) {
                        grid[z][bottom - t][x] = CELL_WALL;
                    }
                }
            }
            
            // West wall
            gapCenter = centerY;
            for (int y = top; y <= bottom; y++) {
                bool isGap = (gapSide == 3) && (y >= gapCenter - gapSize/2) && (y <= gapCenter + gapSize/2);
                if (!isGap) {
                    for (int t = 0; t < wallThickness && left + t < gridWidth; t++) {
                        grid[z][y][left + t] = CELL_WALL;
                    }
                }
            }
            
            // East wall
            gapCenter = centerY;
            for (int y = top; y <= bottom; y++) {
                bool isGap = (gapSide == 1) && (y >= gapCenter - gapSize/2) && (y <= gapCenter + gapSize/2);
                if (!isGap) {
                    for (int t = 0; t < wallThickness && right - t >= 0; t++) {
                        grid[z][y][right - t] = CELL_WALL;
                    }
                }
            }
        }
    }
    
    // Level 3: Open center area (the goal destination)
    // Just leave it as floor (already initialized)
    // Add some decorative walls to make it interesting
    int z3 = baseZ + 3;
    int innerRing = ringSpacing;
    for (int x = centerX - innerRing; x <= centerX + innerRing; x++) {
        if (x > 0 && x < gridWidth - 1) {
            grid[z3][centerY - innerRing][x] = CELL_WALL;
            grid[z3][centerY + innerRing][x] = CELL_WALL;
        }
    }
    for (int y = centerY - innerRing; y <= centerY + innerRing; y++) {
        if (y > 0 && y < gridHeight - 1) {
            grid[z3][y][centerX - innerRing] = CELL_WALL;
            grid[z3][y][centerX + innerRing] = CELL_WALL;
        }
    }
    // Gaps on all sides for level 3
    for (int i = -gapSize/2; i <= gapSize/2; i++) {
        if (centerX + i > 0 && centerX + i < gridWidth - 1) {
            PlaceFloor(centerX + i, centerY - innerRing, z3);
            PlaceFloor(centerX + i, centerY + innerRing, z3);
        }
        if (centerY + i > 0 && centerY + i < gridHeight - 1) {
            PlaceFloor(centerX - innerRing, centerY + i, z3);
            PlaceFloor(centerX + innerRing, centerY + i, z3);
        }
    }
    
    // Place ladders at specific positions to force the spiral
    // Use z0, z1, z2, z3 for the actual z indices
    int z0 = baseZ + 0;
    int z1 = baseZ + 1;
    int z2 = baseZ + 2;
    // z3 already defined above
    
    // Ladder 0→1: Must be accessed from NORTH (outside the outermost ring, north side)
    // After exiting north on level 0, climb to level 1
    int outerRingDist = numRings * ringSpacing;
    
    int ladder01_x = centerX;
    int ladder01_y = centerY - outerRingDist - ringSpacing/2;
    if (ladder01_y < 2) ladder01_y = 2;
    // Place ladder
    grid[z0][ladder01_y][ladder01_x] = CELL_LADDER;
    grid[z1][ladder01_y][ladder01_x] = CELL_LADDER;
    // Ensure path to ladder is clear
    for (int y = ladder01_y; y < centerY - outerRingDist; y++) {
        if (grid[z0][y][ladder01_x] != CELL_LADDER) PlaceFloor(ladder01_x, y, z0);
        if (grid[z1][y][ladder01_x] != CELL_LADDER) PlaceFloor(ladder01_x, y, z1);
    }
    
    // Ladder 1→2: Must be accessed from EAST (outside the outermost ring, east side)
    int ladder12_x = centerX + outerRingDist + ringSpacing/2;
    int ladder12_y = centerY;
    if (ladder12_x >= gridWidth - 2) ladder12_x = gridWidth - 3;
    grid[z1][ladder12_y][ladder12_x] = CELL_LADDER;
    grid[z2][ladder12_y][ladder12_x] = CELL_LADDER;
    // Ensure path to ladder is clear
    for (int x = centerX + outerRingDist; x <= ladder12_x; x++) {
        if (grid[z1][ladder12_y][x] != CELL_LADDER) PlaceFloor(x, ladder12_y, z1);
        if (grid[z2][ladder12_y][x] != CELL_LADDER) PlaceFloor(x, ladder12_y, z2);
    }
    
    // Ladder 2→3: Must be accessed from SOUTH (outside the outermost ring, south side)
    int ladder23_x = centerX;
    int ladder23_y = centerY + outerRingDist + ringSpacing/2;
    if (ladder23_y >= gridHeight - 2) ladder23_y = gridHeight - 3;
    grid[z2][ladder23_y][ladder23_x] = CELL_LADDER;
    grid[z3][ladder23_y][ladder23_x] = CELL_LADDER;
    // Ensure path to ladder is clear (from outer ring to ladder)
    for (int y = centerY + outerRingDist; y <= ladder23_y; y++) {
        if (grid[z2][y][ladder23_x] != CELL_LADDER) PlaceFloor(ladder23_x, y, z2);
        if (grid[z3][y][ladder23_x] != CELL_LADDER) PlaceFloor(ladder23_x, y, z3);
    }
    // On z3: ensure path from ladder to the inner ring (south gap)
    // The inner ring is at centerY + innerRing, so connect from ladder to there
    for (int y = centerY + innerRing; y <= ladder23_y; y++) {
        if (grid[z3][y][ladder23_x] != CELL_LADDER) PlaceFloor(ladder23_x, y, z3);
    }
    
    // Add a few "decoy" ladders that lead to dead ends or longer routes
    // Decoy 1: East side of level 0 - goes up but you're stuck
    int decoy1_x = centerX + outerRingDist + ringSpacing/2;
    int decoy1_y = centerY - ringSpacing;
    if (decoy1_x < gridWidth - 2 && decoy1_y > 1) {
        grid[z0][decoy1_y][decoy1_x] = CELL_LADDER;
        grid[z1][decoy1_y][decoy1_x] = CELL_LADDER;
        // Make sure there's floor around it
        if (grid[z0][decoy1_y][decoy1_x - 1] == CELL_WALL) PlaceFloor(decoy1_x - 1, decoy1_y, z0);
        if (grid[z1][decoy1_y][decoy1_x - 1] == CELL_WALL) PlaceFloor(decoy1_x - 1, decoy1_y, z1);
    }
    
    // Decoy 2: Center area ladder that skips a level but puts you in a bad spot
    int decoy2_x = centerX - ringSpacing;
    int decoy2_y = centerY - ringSpacing;
    if (decoy2_x > 1 && decoy2_y > 1) {
        grid[z0][decoy2_y][decoy2_x] = CELL_LADDER;
        grid[z1][decoy2_y][decoy2_x] = CELL_LADDER;
    }
    
    needsRebuild = true;
}

// ============================================================================
// Feature-based Dungeon Generator (Rooms & Corridors)
// ============================================================================

#define MAX_ROOMS 256
#define MIN_ROOM_SIZE 4
#define MAX_ROOM_SIZE 12
#define CORRIDOR_WIDTH 2

static Room dungeonRooms[MAX_ROOMS];
static int dungeonRoomCount = 0;

static void CarveRoom(int x, int y, int w, int h) {
    for (int py = y; py < y + h && py < gridHeight; py++) {
        for (int px = x; px < x + w && px < gridWidth; px++) {
            if (px >= 0 && py >= 0) {
                grid[1][py][px] = CELL_AIR;  // Carve at z=1 (walking level)
            }
        }
    }
}

static void CarveCorridor(int x1, int y1, int x2, int y2) {
    int x = x1, y = y1;
    
    // Randomly choose horizontal or vertical first
    if (GetRandomValue(0, 1) == 0) {
        // Horizontal then vertical
        while (x != x2) {
            for (int w = 0; w < CORRIDOR_WIDTH; w++) {
                if (y + w >= 0 && y + w < gridHeight && x >= 0 && x < gridWidth)
                    grid[1][y + w][x] = CELL_AIR;
            }
            x += (x2 > x) ? 1 : -1;
        }
        while (y != y2) {
            for (int w = 0; w < CORRIDOR_WIDTH; w++) {
                if (y >= 0 && y < gridHeight && x + w >= 0 && x + w < gridWidth)
                    grid[1][y][x + w] = CELL_AIR;
            }
            y += (y2 > y) ? 1 : -1;
        }
    } else {
        // Vertical then horizontal
        while (y != y2) {
            for (int w = 0; w < CORRIDOR_WIDTH; w++) {
                if (y >= 0 && y < gridHeight && x + w >= 0 && x + w < gridWidth)
                    grid[1][y][x + w] = CELL_AIR;
            }
            y += (y2 > y) ? 1 : -1;
        }
        while (x != x2) {
            for (int w = 0; w < CORRIDOR_WIDTH; w++) {
                if (y + w >= 0 && y + w < gridHeight && x >= 0 && x < gridWidth)
                    grid[1][y + w][x] = CELL_AIR;
            }
            x += (x2 > x) ? 1 : -1;
        }
    }
}

static bool RoomOverlaps(int x, int y, int w, int h, int margin) {
    if (x - margin < 0 || y - margin < 0 || 
        x + w + margin >= gridWidth || y + h + margin >= gridHeight) {
        return true;
    }
    for (int i = 0; i < dungeonRoomCount; i++) {
        Room* r = &dungeonRooms[i];
        if (x < r->x + r->w + margin && x + w + margin > r->x &&
            y < r->y + r->h + margin && y + h + margin > r->y) {
            return true;
        }
    }
    return false;
}

void GenerateDungeonRooms(void) {
    InitGrid();  // Clear cells and flags
    
    // Fill z=1 with walls (to carve rooms)
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++)
            grid[1][y][x] = CELL_WALL;
    
    dungeonRoomCount = 0;
    
    // Place first room in center
    int firstW = GetRandomValue(MIN_ROOM_SIZE, MAX_ROOM_SIZE);
    int firstH = GetRandomValue(MIN_ROOM_SIZE, MAX_ROOM_SIZE);
    int firstX = (gridWidth - firstW) / 2;
    int firstY = (gridHeight - firstH) / 2;
    
    CarveRoom(firstX, firstY, firstW, firstH);
    dungeonRooms[dungeonRoomCount++] = (Room){firstX, firstY, firstW, firstH};
    
    // Try to add more rooms
    int attempts = 500;
    int maxRooms = 30 + (gridWidth * gridHeight) / 500;
    if (maxRooms > MAX_ROOMS) maxRooms = MAX_ROOMS;
    
    for (int i = 0; i < attempts && dungeonRoomCount < maxRooms; i++) {
        Room* source = &dungeonRooms[GetRandomValue(0, dungeonRoomCount - 1)];
        
        int newW = GetRandomValue(MIN_ROOM_SIZE, MAX_ROOM_SIZE);
        int newH = GetRandomValue(MIN_ROOM_SIZE, MAX_ROOM_SIZE);
        int side = GetRandomValue(0, 3);
        int newX, newY;
        int corridorLen = GetRandomValue(2, 8);
        
        switch (side) {
            case 0: // North
                newX = source->x + GetRandomValue(0, source->w - 1) - newW / 2;
                newY = source->y - corridorLen - newH;
                break;
            case 1: // East
                newX = source->x + source->w + corridorLen;
                newY = source->y + GetRandomValue(0, source->h - 1) - newH / 2;
                break;
            case 2: // South
                newX = source->x + GetRandomValue(0, source->w - 1) - newW / 2;
                newY = source->y + source->h + corridorLen;
                break;
            default: // West
                newX = source->x - corridorLen - newW;
                newY = source->y + GetRandomValue(0, source->h - 1) - newH / 2;
                break;
        }
        
        if (!RoomOverlaps(newX, newY, newW, newH, 2)) {
            CarveRoom(newX, newY, newW, newH);
            dungeonRooms[dungeonRoomCount++] = (Room){newX, newY, newW, newH};
            
            int srcCenterX = source->x + source->w / 2;
            int srcCenterY = source->y + source->h / 2;
            int newCenterX = newX + newW / 2;
            int newCenterY = newY + newH / 2;
            CarveCorridor(srcCenterX, srcCenterY, newCenterX, newCenterY);
        }
    }
    
    needsRebuild = true;
}

// ============================================================================
// Cellular Automata Cave Generator
// ============================================================================

void GenerateCaves(void) {
    InitGrid();  // Clear cells and flags
    
    // Start with random noise
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            // Border is always wall
            if (x == 0 || y == 0 || x == gridWidth - 1 || y == gridHeight - 1) {
                grid[1][y][x] = CELL_WALL;
            } else {
                grid[0][y][x] = (GetRandomValue(0, 100) < 45) ? CELL_WALL : CELL_DIRT;
            }
        }
    }
    
    // Temporary buffer for cellular automata
    CellType* temp = (CellType*)malloc(gridWidth * gridHeight * sizeof(CellType));
    
    // Run cellular automata iterations
    for (int iter = 0; iter < 5; iter++) {
        for (int y = 1; y < gridHeight - 1; y++) {
            for (int x = 1; x < gridWidth - 1; x++) {
                // Count neighboring walls
                int walls = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (grid[0][y + dy][x + dx] == CELL_WALL) walls++;
                    }
                }
                // 4-5 rule: become wall if >= 5 neighbors are walls
                temp[y * gridWidth + x] = (walls >= 5) ? CELL_WALL : CELL_DIRT;
            }
        }
        // Copy back
        for (int y = 1; y < gridHeight - 1; y++) {
            for (int x = 1; x < gridWidth - 1; x++) {
                grid[0][y][x] = temp[y * gridWidth + x];
            }
        }
    }
    
    free(temp);
    
    // Ensure connectivity by flood-filling from center and connecting islands
    // (Simple approach: just carve a path from center to ensure some walkable area)
    int cx = gridWidth / 2, cy = gridHeight / 2;
    for (int r = 0; r < 5; r++) {
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                int nx = cx + dx, ny = cy + dy;
                if (nx >= 0 && nx < gridWidth && ny >= 0 && ny < gridHeight) {
                    grid[0][ny][nx] = CELL_DIRT;
                }
            }
        }
    }
    
    needsRebuild = true;
}

// ============================================================================
// Drunkard's Walk Generator
// ============================================================================

void GenerateDrunkard(void) {
    InitGrid();  // Clear cells and flags
    
    // Fill z=1 with walls (to carve)
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++)
            grid[1][y][x] = CELL_WALL;
    
    // Start from center
    int x = gridWidth / 2;
    int y = gridHeight / 2;
    
    // Target: carve out ~40% of the map
    int targetFloor = (gridWidth * gridHeight * 40) / 100;
    int floorCount = 0;
    
    int maxSteps = gridWidth * gridHeight * 10;  // Prevent infinite loop
    
    for (int step = 0; step < maxSteps && floorCount < targetFloor; step++) {
        // Carve current position
        if (grid[1][y][x] == CELL_WALL) {
            grid[1][y][x] = CELL_AIR;
            floorCount++;
        }
        
        // Random walk
        int dir = GetRandomValue(0, 3);
        switch (dir) {
            case 0: if (y > 1) y--; break;              // North
            case 1: if (x < gridWidth - 2) x++; break;  // East
            case 2: if (y < gridHeight - 2) y++; break; // South
            case 3: if (x > 1) x--; break;              // West
        }
    }
    
    needsRebuild = true;
}

// ============================================================================
// Tunneler Algorithm (Rooms and Corridors)
// Based on classic roguelike approach: place rooms, connect with corridors
// ============================================================================

typedef struct {
    int x, y, w, h;
} TunnelRoom;

static bool TunnelRoomsIntersect(TunnelRoom* a, TunnelRoom* b) {
    // Add 1 tile padding between rooms
    return (a->x <= b->x + b->w + 1 && a->x + a->w + 1 >= b->x &&
            a->y <= b->y + b->h + 1 && a->y + a->h + 1 >= b->y);
}

static void CarveTunnelRoom(TunnelRoom* room) {
    for (int y = room->y; y < room->y + room->h; y++) {
        for (int x = room->x; x < room->x + room->w; x++) {
            if (x > 0 && x < gridWidth - 1 && y > 0 && y < gridHeight - 1) {
                grid[1][y][x] = CELL_AIR;
            }
        }
    }
}

static void CarveHorizontalTunnel(int x1, int x2, int y) {
    int minX = x1 < x2 ? x1 : x2;
    int maxX = x1 > x2 ? x1 : x2;
    for (int x = minX; x <= maxX; x++) {
        if (x > 0 && x < gridWidth - 1 && y > 0 && y < gridHeight - 1) {
            grid[1][y][x] = CELL_AIR;
        }
    }
}

static void CarveVerticalTunnel(int y1, int y2, int x) {
    int minY = y1 < y2 ? y1 : y2;
    int maxY = y1 > y2 ? y1 : y2;
    for (int y = minY; y <= maxY; y++) {
        if (x > 0 && x < gridWidth - 1 && y > 0 && y < gridHeight - 1) {
            grid[1][y][x] = CELL_AIR;
        }
    }
}

void GenerateTunneler(void) {
    InitGrid();  // Clear cells and flags
    
    // Fill z=1 with walls (to carve)
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++)
            grid[1][y][x] = CELL_WALL;
    
    // Scale room count based on world size
    // Roughly 1 room per 150 tiles, with min 5 and max 100
    int maxRooms = (gridWidth * gridHeight) / 150;
    if (maxRooms < 5) maxRooms = 5;
    if (maxRooms > 100) maxRooms = 100;
    
    TunnelRoom rooms[100];
    int roomCount = 0;
    
    // Try to place rooms
    for (int i = 0; i < maxRooms * 3; i++) {  // More attempts than max rooms
        if (roomCount >= maxRooms) break;
        
        // Random room size
        int w = 4 + GetRandomValue(0, 6);
        int h = 4 + GetRandomValue(0, 6);
        
        // Random position (with margin from edges)
        int rx = 2 + GetRandomValue(0, gridWidth - w - 4);
        int ry = 2 + GetRandomValue(0, gridHeight - h - 4);
        
        TunnelRoom newRoom = {rx, ry, w, h};
        
        // Check for overlaps with existing rooms
        bool overlaps = false;
        for (int r = 0; r < roomCount; r++) {
            if (TunnelRoomsIntersect(&newRoom, &rooms[r])) {
                overlaps = true;
                break;
            }
        }
        
        if (!overlaps) {
            // Carve the room
            CarveTunnelRoom(&newRoom);
            
            // Connect to previous room with corridors
            if (roomCount > 0) {
                int newCenterX = newRoom.x + newRoom.w / 2;
                int newCenterY = newRoom.y + newRoom.h / 2;
                int prevCenterX = rooms[roomCount - 1].x + rooms[roomCount - 1].w / 2;
                int prevCenterY = rooms[roomCount - 1].y + rooms[roomCount - 1].h / 2;
                
                // Randomly choose horizontal-first or vertical-first
                if (GetRandomValue(0, 1) == 0) {
                    CarveHorizontalTunnel(prevCenterX, newCenterX, prevCenterY);
                    CarveVerticalTunnel(prevCenterY, newCenterY, newCenterX);
                } else {
                    CarveVerticalTunnel(prevCenterY, newCenterY, prevCenterX);
                    CarveHorizontalTunnel(prevCenterX, newCenterX, newCenterY);
                }
            }
            
            rooms[roomCount++] = newRoom;
        }
    }
    
    needsRebuild = true;
}

void GenerateMixMax(void) {
    // First run tunneler (rooms + corridors)
    GenerateTunneler();
    
    // Then add more rooms on top using dungeon style
    // (Don't fill with walls - just carve more rooms)
    int extraRooms = (gridWidth * gridHeight) / 300;  // Half as many extra rooms
    if (extraRooms < 3) extraRooms = 3;
    if (extraRooms > 50) extraRooms = 50;
    
    int added = 0;
    for (int i = 0; i < extraRooms * 5 && added < extraRooms; i++) {
        int w = 4 + GetRandomValue(0, 8);
        int h = 4 + GetRandomValue(0, 8);
        int rx = 2 + GetRandomValue(0, gridWidth - w - 4);
        int ry = 2 + GetRandomValue(0, gridHeight - h - 4);
        
        // Just carve it - overlaps are fine, creates more interesting shapes
        for (int y = ry; y < ry + h && y < gridHeight - 1; y++) {
            for (int x = rx; x < rx + w && x < gridWidth - 1; x++) {
                if (x > 0 && y > 0) {
                    grid[1][y][x] = CELL_AIR;
                }
            }
        }
        added++;
    }
    
    needsRebuild = true;
}

void GenerateConcentricMaze(void) {
    InitGrid();
    
    // Create concentric rectangular rings with gaps
    // Each ring has a gap on alternating sides to force spiraling
    int minDim = gridWidth < gridHeight ? gridWidth : gridHeight;
    int ringSpacing = 4;  // space between rings
    int wallThickness = 2;
    int gapSize = 3;
    
    int ringCount = (minDim / 2) / ringSpacing;
    
    for (int ring = 0; ring < ringCount; ring++) {
        int offset = ring * ringSpacing;
        int left = offset;
        int right = gridWidth - 1 - offset;
        int top = offset;
        int bottom = gridHeight - 1 - offset;
        
        // Skip if ring is too small
        if (right - left < gapSize * 2 || bottom - top < gapSize * 2) break;
        
        // Determine gap position - alternate sides for each ring
        // Ring 0: gap on right, Ring 1: gap on bottom, Ring 2: gap on left, Ring 3: gap on top, etc.
        int gapSide = ring % 4;
        int gapStart;
        
        // Draw top wall
        if (gapSide == 3) {
            gapStart = left + (right - left) / 2 - gapSize / 2;
        }
        for (int x = left; x <= right; x++) {
            if (gapSide == 3 && x >= gapStart && x < gapStart + gapSize) continue;
            for (int t = 0; t < wallThickness && top + t < gridHeight; t++) {
                grid[1][top + t][x] = CELL_WALL;
            }
        }
        
        // Draw bottom wall
        if (gapSide == 1) {
            gapStart = left + (right - left) / 2 - gapSize / 2;
        }
        for (int x = left; x <= right; x++) {
            if (gapSide == 1 && x >= gapStart && x < gapStart + gapSize) continue;
            for (int t = 0; t < wallThickness && bottom - t >= 0; t++) {
                grid[1][bottom - t][x] = CELL_WALL;
            }
        }
        
        // Draw left wall
        if (gapSide == 2) {
            gapStart = top + (bottom - top) / 2 - gapSize / 2;
        }
        for (int y = top; y <= bottom; y++) {
            if (gapSide == 2 && y >= gapStart && y < gapStart + gapSize) continue;
            for (int t = 0; t < wallThickness && left + t < gridWidth; t++) {
                grid[1][y][left + t] = CELL_WALL;
            }
        }
        
        // Draw right wall
        if (gapSide == 0) {
            gapStart = top + (bottom - top) / 2 - gapSize / 2;
        }
        for (int y = top; y <= bottom; y++) {
            if (gapSide == 0 && y >= gapStart && y < gapStart + gapSize) continue;
            for (int t = 0; t < wallThickness && right - t >= 0; t++) {
                grid[1][y][right - t] = CELL_WALL;
            }
        }
    }
    
    needsRebuild = true;
}

void InitPerlin(int seed) {
    SetRandomSeed(seed);
    for (int i = 0; i < 256; i++) permutation[i] = i;
    for (int i = 255; i > 0; i--) {
        int j = GetRandomValue(0, i);
        int tmp = permutation[i];
        permutation[i] = permutation[j];
        permutation[j] = tmp;
    }
    for (int i = 0; i < 256; i++) permutation[256 + i] = permutation[i];
}

static float MyFade(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
static float MyLerp(float a, float b, float t) { return a + t * (b - a); }
static float Grad(int hash, float x, float y) {
    int h = hash & 3;
    float u = h < 2 ? x : y;
    float v = h < 2 ? y : x;
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

float Perlin(float x, float y) {
    int xi = (int)floorf(x) & 255;
    int yi = (int)floorf(y) & 255;
    float xf = x - floorf(x);
    float yf = y - floorf(y);
    float u = MyFade(xf);
    float v = MyFade(yf);
    int aa = permutation[permutation[xi] + yi];
    int ab = permutation[permutation[xi] + yi + 1];
    int ba = permutation[permutation[xi + 1] + yi];
    int bb = permutation[permutation[xi + 1] + yi + 1];
    float x1 = MyLerp(Grad(aa, xf, yf), Grad(ba, xf - 1, yf), u);
    float x2 = MyLerp(Grad(ab, xf, yf - 1), Grad(bb, xf - 1, yf - 1), u);
    return (MyLerp(x1, x2, v) + 1) / 2;
}

float OctavePerlin(float x, float y, int octaves, float persistence) {
    float total = 0, freq = 1, amp = 1, maxVal = 0;
    for (int i = 0; i < octaves; i++) {
        total += Perlin(x * freq, y * freq) * amp;
        maxVal += amp;
        amp *= persistence;
        freq *= 2;
    }
    return total / maxVal;
}

// ============================================================================
// 3D Perlin Noise (for volumetric terrain)
// ============================================================================

static float Grad3D(int hash, float x, float y, float z) {
    int h = hash & 15;
    float u = h < 8 ? x : y;
    float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

float Perlin3D(float x, float y, float z) {
    int xi = (int)floorf(x) & 255;
    int yi = (int)floorf(y) & 255;
    int zi = (int)floorf(z) & 255;
    
    float xf = x - floorf(x);
    float yf = y - floorf(y);
    float zf = z - floorf(z);
    
    float u = MyFade(xf);
    float v = MyFade(yf);
    float w = MyFade(zf);
    
    int aaa = permutation[permutation[permutation[xi] + yi] + zi];
    int aba = permutation[permutation[permutation[xi] + yi + 1] + zi];
    int aab = permutation[permutation[permutation[xi] + yi] + zi + 1];
    int abb = permutation[permutation[permutation[xi] + yi + 1] + zi + 1];
    int baa = permutation[permutation[permutation[xi + 1] + yi] + zi];
    int bba = permutation[permutation[permutation[xi + 1] + yi + 1] + zi];
    int bab = permutation[permutation[permutation[xi + 1] + yi] + zi + 1];
    int bbb = permutation[permutation[permutation[xi + 1] + yi + 1] + zi + 1];
    
    float x1 = MyLerp(Grad3D(aaa, xf, yf, zf), Grad3D(baa, xf - 1, yf, zf), u);
    float x2 = MyLerp(Grad3D(aba, xf, yf - 1, zf), Grad3D(bba, xf - 1, yf - 1, zf), u);
    float y1 = MyLerp(x1, x2, v);
    
    float x3 = MyLerp(Grad3D(aab, xf, yf, zf - 1), Grad3D(bab, xf - 1, yf, zf - 1), u);
    float x4 = MyLerp(Grad3D(abb, xf, yf - 1, zf - 1), Grad3D(bbb, xf - 1, yf - 1, zf - 1), u);
    float y2 = MyLerp(x3, x4, v);
    
    return (MyLerp(y1, y2, w) + 1.0f) / 2.0f;
}

float OctavePerlin3D(float x, float y, float z, int octaves, float persistence) {
    float total = 0, freq = 1, amp = 1, maxVal = 0;
    for (int i = 0; i < octaves; i++) {
        total += Perlin3D(x * freq, y * freq, z * freq) * amp;
        maxVal += amp;
        amp *= persistence;
        freq *= 2;
    }
    return total / maxVal;
}

// ============================================================================
// Hills/Mountains Generator
// Uses 2D Perlin noise for heightmap, fills with dirt up to that height
// Creates natural rolling hills and mountains
// ============================================================================

void GenerateHills(void) {
    InitGrid();  // Clear cells and flags
    InitPerlin(GetRandomValue(0, 99999));
    
    // Parameters
    float scale = 0.02f;          // Noise scale (smaller = larger features)
    int maxHeight = gridDepth - 2; // Leave room at top
    int minHeight = 1;             // Minimum ground height
    
    // Generate heightmap using 2D Perlin noise
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            // Multi-octave noise for natural terrain
            float n = OctavePerlin(x * scale, y * scale, 4, 0.5f);
            
            // Map noise to height (0-1 -> minHeight to maxHeight)
            int height = minHeight + (int)(n * (maxHeight - minHeight));
            if (height < minHeight) height = minHeight;
            if (height >= gridDepth) height = gridDepth - 1;
            
            // Fill with dirt from z=0 up to height
            for (int z = 0; z <= height; z++) {
                grid[z][y][x] = CELL_DIRT;
            }
            
            // Add grass surface on top
            SET_CELL_SURFACE(x, y, height, SURFACE_TALL_GRASS);
        }
    }
    
    needsRebuild = true;
}

void GeneratePerlin(void) {
    InitGrid();
    InitPerlin(GetRandomValue(0, 99999));
    float scale = 0.015f;

    // First pass: terrain noise for trees
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            float n = OctavePerlin(x * scale, y * scale, 4, 0.5f);
            // n < 0.45 = forest, n > 0.55 = city, between = transition
            float density;
            if (n < 0.45f) {
                density = 0.08f + (0.45f - n) * 0.3f;  // 8-20% trees in forest
            } else {
                density = 0.02f;  // light debris in city
            }
            if ((float)GetRandomValue(0, 100) / 100.0f < density)
                grid[1][y][x] = CELL_WALL;
        }
    }

    // Second pass: city walls where noise > 0.5
    for (int wy = chunkHeight/2; wy < gridHeight; wy += chunkHeight / 2) {
        for (int wx = 0; wx < gridWidth;) {
            float n = OctavePerlin(wx * scale, wy * scale, 4, 0.5f);
            if (n < 0.5f) { wx += 6; continue; }

            // Higher noise = longer walls, smaller gaps
            float intensity = (n - 0.5f) * 2.0f;  // 0-1
            int wallLen = (int)(4 + intensity * 12);   // 4-16
            int gapSize = (int)(5 - intensity * 2);    // 5-3
            if (gapSize < 2) gapSize = 2;

            for (int x = wx; x < wx + wallLen && x < gridWidth; x++) {
                float n2 = OctavePerlin(x * scale, wy * scale, 4, 0.5f);
                if (n2 > 0.48f) {
                    grid[1][wy][x] = CELL_WALL;
                    if (wy + 1 < gridHeight) grid[1][wy + 1][x] = CELL_WALL;
                }
            }
            wx += wallLen + gapSize;
        }
    }

    // Vertical walls
    for (int wx = chunkWidth/2; wx < gridWidth; wx += chunkWidth / 2) {
        for (int wy = 0; wy < gridHeight;) {
            float n = OctavePerlin(wx * scale, wy * scale, 4, 0.5f);
            if (n < 0.5f) { wy += 6; continue; }

            float intensity = (n - 0.5f) * 2.0f;
            int wallLen = (int)(4 + intensity * 12);
            int gapSize = (int)(5 - intensity * 2);
            if (gapSize < 2) gapSize = 2;

            for (int y = wy; y < wy + wallLen && y < gridHeight; y++) {
                float n2 = OctavePerlin(wx * scale, y * scale, 4, 0.5f);
                if (n2 > 0.48f) {
                    grid[1][y][wx] = CELL_WALL;
                    if (wx + 1 < gridWidth) grid[1][y][wx + 1] = CELL_WALL;
                }
            }
            wy += wallLen + gapSize;
        }
    }

    needsRebuild = true;
}

void GenerateCity(void) {
    InitGrid();
    for (int wy = chunkHeight; wy < gridHeight; wy += chunkHeight / 2) {
        for (int wx = 0; wx < gridWidth; wx++) {
            int gapPos = GetRandomValue(6, 20);
            int gapSize = GetRandomValue(3, 6);
            for (int x = wx; x < wx + gapPos && x < gridWidth; x++) {
                grid[1][wy][x] = CELL_WALL;
                if (wy + 1 < gridHeight) grid[1][wy + 1][x] = CELL_WALL;
            }
            wx += gapPos + gapSize;
        }
    }
    for (int wx = chunkWidth; wx < gridWidth; wx += chunkWidth / 2) {
        for (int wy = 0; wy < gridHeight; wy++) {
            int gapPos = GetRandomValue(6, 20);
            int gapSize = GetRandomValue(3, 6);
            for (int y = wy; y < wy + gapPos && y < gridHeight; y++) {
                grid[1][y][wx] = CELL_WALL;
                if (wx + 1 < gridWidth) grid[1][y][wx + 1] = CELL_WALL;
            }
            wy += gapPos + gapSize;
        }
    }
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++)
            if (grid[0][y][x] == CELL_DIRT && GetRandomValue(0, 100) < 5)
                grid[1][y][x] = CELL_WALL;
    needsRebuild = true;
}

// ============================================================================
// 3D Towers with Bridges Generator
// Creates towers (vertical structures) with bridges connecting them at higher levels
// ============================================================================

#define MAX_TOWERS 50

typedef struct {
    int x, y;       // Tower base position
    int w, h;       // Tower footprint
    int height;     // Tower height (z-levels)
} Tower;

// Union-Find for connectivity checking
static int towerParent[MAX_TOWERS];

static int TowerFind(int i) {
    if (towerParent[i] != i)
        towerParent[i] = TowerFind(towerParent[i]);
    return towerParent[i];
}

static void TowerUnion(int i, int j) {
    int pi = TowerFind(i);
    int pj = TowerFind(j);
    if (pi != pj) towerParent[pi] = pj;
}

// Build a bridge between two towers
static void BuildBridge(Tower* t1, Tower* t2) {
    int c1x = t1->x + t1->w / 2;
    int c1y = t1->y + t1->h / 2;
    int c2x = t2->x + t2->w / 2;
    int c2y = t2->y + t2->h / 2;
    int dx = c2x - c1x;
    int dy = c2y - c1y;
    
    // Build bridge at z=1 (only use z=2 if both towers are already 3 levels)
    int bridgeZ = (t1->height >= 3 && t2->height >= 3 && GetRandomValue(0, 1)) ? 2 : 1;
    
    // Extend towers to reach bridge level if needed
    // This adds walls and air up to bridgeZ+2 (walking level above bridge)
    Tower* towersToExtend[2] = {t1, t2};
    for (int ti = 0; ti < 2; ti++) {
        Tower* t = towersToExtend[ti];
        int newHeight = bridgeZ + 2;
        if (t->height < newHeight) {
            // Extend tower structure up to new height
            for (int z = t->height + 1; z <= newHeight; z++) {
                for (int py = t->y; py < t->y + t->h; py++) {
                    for (int px = t->x; px < t->x + t->w; px++) {
                        bool isBorder = (px == t->x || px == t->x + t->w - 1 || 
                                        py == t->y || py == t->y + t->h - 1);
                        grid[z][py][px] = isBorder ? CELL_WALL : CELL_AIR;
                    }
                }
            }
            // Update tower height
            t->height = newHeight;
        }
        // Always ensure ladders go through all levels
        // Ladders must exist on BOTH z-levels to allow climbing between them
        int ladderX = t->x + t->w / 2;
        int ladderY = t->y + t->h / 2;
        for (int z = 1; z <= t->height; z++) {
            grid[z][ladderY][ladderX] = CELL_LADDER;
        }
    }
    
    // Find bridge start and end points (on tower edges)
    int startX, startY, endX, endY;
    if ((dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy)) {
        // Horizontal bridge
        if (dx > 0) {
            startX = t1->x + t1->w - 1;
            endX = t2->x;
        } else {
            startX = t1->x;
            endX = t2->x + t2->w - 1;
        }
        startY = t1->y + t1->h / 2;
        endY = t2->y + t2->h / 2;
    } else {
        // Vertical bridge
        if (dy > 0) {
            startY = t1->y + t1->h - 1;
            endY = t2->y;
        } else {
            startY = t1->y;
            endY = t2->y + t2->h - 1;
        }
        startX = t1->x + t1->w / 2;
        endX = t2->x + t2->w / 2;
    }
    
    // Carve bridge (simple L-shape)
    int x = startX, y = startY;
    
    // Open the tower walls at bridge connection points (at walking level bridgeZ+1)
    grid[bridgeZ + 1][startY][startX] = CELL_AIR;
    grid[bridgeZ + 1][endY][endX] = CELL_AIR;
    
    // Horizontal segment - solid floor at bridgeZ, air at bridgeZ+1
    while (x != endX) {
        if (x >= 0 && x < gridWidth && y >= 0 && y < gridHeight) {
            grid[bridgeZ][y][x] = CELL_WALL;      // Floor (solid to walk on)
            grid[bridgeZ + 1][y][x] = CELL_AIR;   // Walking level
        }
        x += (endX > x) ? 1 : -1;
    }
    // Vertical segment  
    while (y != endY) {
        if (x >= 0 && x < gridWidth && y >= 0 && y < gridHeight) {
            grid[bridgeZ][y][x] = CELL_WALL;      // Floor (solid to walk on)
            grid[bridgeZ + 1][y][x] = CELL_AIR;   // Walking level
        }
        y += (endY > y) ? 1 : -1;
    }
}

void GenerateTowers(void) {
    InitGrid();  // Clear cells and flags
    if (!g_legacyWalkability) FillGroundLevel();
    
    // Place towers
    Tower towers[MAX_TOWERS];
    int towerCount = 0;
    
    int attempts = 200;
    int targetTowers = (gridWidth * gridHeight) / 200;  // Roughly 1 tower per 200 tiles
    if (targetTowers < 5) targetTowers = 5;
    if (targetTowers > MAX_TOWERS) targetTowers = MAX_TOWERS;
    
    for (int i = 0; i < attempts && towerCount < targetTowers; i++) {
        int tw = 3 + GetRandomValue(0, 3);  // Tower size 3-6
        int th = 3 + GetRandomValue(0, 3);
        int tx = 2 + GetRandomValue(0, gridWidth - tw - 4);
        int ty = 2 + GetRandomValue(0, gridHeight - th - 4);
        int tHeight = 2 + GetRandomValue(0, 1);  // Height 2-3 z-levels
        
        // Check for overlap with existing towers (with margin)
        bool overlaps = false;
        for (int t = 0; t < towerCount; t++) {
            Tower* other = &towers[t];
            int margin = 4;  // Space between towers for bridges
            if (tx < other->x + other->w + margin && tx + tw + margin > other->x &&
                ty < other->y + other->h + margin && ty + th + margin > other->y) {
                overlaps = true;
                break;
            }
        }
        
        if (!overlaps) {
            towers[towerCount++] = (Tower){tx, ty, tw, th, tHeight};
            
            // Calculate ladder position
            int ladderX = tx + tw / 2;
            int ladderY = ty + th / 2;
            
            // Build the tower: walls on border, floor inside (shifted +1 for DF)
            // Skip ladder position when placing floors
            for (int z = 0; z < tHeight; z++) {
                for (int py = ty; py < ty + th; py++) {
                    for (int px = tx; px < tx + tw; px++) {
                        bool isBorder = (px == tx || px == tx + tw - 1 || 
                                        py == ty || py == ty + th - 1);
                        bool isLadderPos = (px == ladderX && py == ladderY);
                        if (isBorder) {
                            grid[z + 1][py][px] = CELL_WALL;
                        } else if (!isLadderPos) {
                            PlaceFloor(px, py, z + 1);
                        }
                    }
                }
            }
            
            // Add ladder inside tower (connects all levels)
            // Place ladder from bottom to top
            for (int z = 0; z < tHeight - 1; z++) {
                PlaceLadder(ladderX, ladderY, z + 1);
            }
            
            // Add door at z=1 (opening in wall)
            int doorSide = GetRandomValue(0, 3);
            switch (doorSide) {
                case 0: PlaceFloor(tx + tw / 2, ty, 1); break;          // North
                case 1: PlaceFloor(tx + tw - 1, ty + th / 2, 1); break; // East
                case 2: PlaceFloor(tx + tw / 2, ty + th - 1, 1); break; // South
                case 3: PlaceFloor(tx, ty + th / 2, 1); break;          // West
            }
        }
    }
    
    // Initialize union-find: all towers connected via ground (z=0)
    for (int i = 0; i < towerCount; i++) {
        towerParent[i] = 0;  // All towers are connected at ground level
    }
    
    // Connect some towers with bridges at z=1 or z=2
    for (int i = 0; i < towerCount; i++) {
        Tower* t1 = &towers[i];
        if (t1->height < 2) continue;  // Need at least 2 levels for bridge
        
        // Try to connect to 1-2 nearby towers
        int connections = 0;
        for (int j = 0; j < towerCount && connections < 2; j++) {
            if (i == j) continue;
            Tower* t2 = &towers[j];
            if (t2->height < 2) continue;
            
            // Calculate distance
            int c1x = t1->x + t1->w / 2;
            int c1y = t1->y + t1->h / 2;
            int c2x = t2->x + t2->w / 2;
            int c2y = t2->y + t2->h / 2;
            int dx = c2x - c1x;
            int dy = c2y - c1y;
            int dist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
            
            // Only connect nearby towers (manhattan distance 8-20)
            if (dist < 8 || dist > 20) continue;
            
            // Random chance to skip
            if (GetRandomValue(0, 100) < 50) continue;
            
            BuildBridge(t1, t2);
            TowerUnion(i, j);
            connections++;
        }
    }
    
    // Ensure all towers with height >= 2 are connected via bridges
    // Find towers that aren't in the main connected component
    for (int i = 1; i < towerCount; i++) {
        if (towers[i].height < 2) continue;
        
        // Find first tower with height >= 2 in a different component
        if (TowerFind(i) != TowerFind(0)) {
            // Find nearest tower in the main component to connect to
            int nearest = -1;
            int nearestDist = 999999;
            for (int j = 0; j < towerCount; j++) {
                if (i == j || towers[j].height < 2) continue;
                if (TowerFind(j) != TowerFind(0)) continue;  // Must be in main component
                
                int dx = (towers[i].x + towers[i].w/2) - (towers[j].x + towers[j].w/2);
                int dy = (towers[i].y + towers[i].h/2) - (towers[j].y + towers[j].h/2);
                int dist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
                if (dist < nearestDist) {
                    nearestDist = dist;
                    nearest = j;
                }
            }
            
            if (nearest >= 0) {
                BuildBridge(&towers[i], &towers[nearest]);
                TowerUnion(i, nearest);
            }
        }
    }
    
    // Final pass: ensure ALL towers have proper ladders through all levels
    // Ladders must exist on BOTH z-levels to allow climbing between them
    for (int i = 0; i < towerCount; i++) {
        Tower* t = &towers[i];
        if (t->height < 2) continue;
        
        int ladderX = t->x + t->w / 2;
        int ladderY = t->y + t->h / 2;
        // Ladders on ALL levels of the tower (starting at z=1)
        for (int z = 1; z <= t->height; z++) {
            grid[z][ladderY][ladderX] = CELL_LADDER;
        }
    }
    
    needsRebuild = true;
}

// ============================================================================
// Gallery Flat (Galerij Flat) Generator
// Long rectangular apartment building with external corridor (gallery) on one side
// Staircases at both ends connecting all floors
// ============================================================================

void GenerateGalleryFlat(void) {
    InitGrid();  // Clear cells and flags
    if (!g_legacyWalkability) FillGroundLevel();
    
    // Building parameters
    int apartmentWidth = 4;
    int apartmentDepth = 4;
    int corridorWidth = 2;
    int stairWidth = 2;
    int numFloors = gridDepth;  // Use all available z-levels
    
    // Calculate building dimensions
    int numApartments = (gridWidth - 4 - 2 * stairWidth) / apartmentWidth;
    if (numApartments < 2) numApartments = 2;
    
    int buildingWidth = stairWidth + numApartments * apartmentWidth + stairWidth;
    int buildingDepth = apartmentDepth + corridorWidth;
    
    // Center the building
    int buildingX = (gridWidth - buildingWidth) / 2;
    int buildingY = (gridHeight - buildingDepth) / 2;
    
    // Build each floor (shifted +1 for DF-style: z=0 is ground, building starts at z=1)
    for (int floor = 0; floor < numFloors - 1; floor++) {
        int z = floor + 1;  // Actual z-level (floor 0 -> z=1, etc.)
        if (z >= gridDepth) break;
        
        // Outer walls of the building
        for (int x = buildingX; x < buildingX + buildingWidth; x++) {
            grid[z][buildingY][x] = CELL_WALL;                           // North wall
            grid[z][buildingY + buildingDepth - 1][x] = CELL_WALL;       // South wall
        }
        for (int y = buildingY; y < buildingY + buildingDepth; y++) {
            grid[z][y][buildingX] = CELL_WALL;                           // West wall
            grid[z][y][buildingX + buildingWidth - 1] = CELL_WALL;       // East wall
        }
        
        // Fill building interior with floor
        for (int y = buildingY + 1; y < buildingY + buildingDepth - 1; y++) {
            for (int x = buildingX + 1; x < buildingX + buildingWidth - 1; x++) {
                PlaceFloor(x, y, z);
            }
        }
        
        // Gallery corridor (south side, inside the building)
        // Already filled with floor above
        
        // Apartment walls (north side)
        int apartmentStartX = buildingX + stairWidth;
        for (int apt = 0; apt < numApartments; apt++) {
            int aptX = apartmentStartX + apt * apartmentWidth;
            
            // Wall between apartments (except first one which uses building wall)
            if (apt > 0) {
                for (int y = buildingY; y < buildingY + apartmentDepth; y++) {
                    grid[z][y][aptX] = CELL_WALL;
                }
            }
            
            // Back wall of apartment (separating from corridor)
            for (int x = aptX; x < aptX + apartmentWidth && x < buildingX + buildingWidth - stairWidth; x++) {
                grid[z][buildingY + apartmentDepth - 1][x] = CELL_WALL;
            }
            
            // Door to corridor (middle of back wall)
            int doorX = aptX + apartmentWidth / 2;
            if (doorX < buildingX + buildingWidth - stairWidth) {
                PlaceFloor(doorX, buildingY + apartmentDepth - 1, z);
            }
        }
        
        // Last apartment wall
        int lastWallX = apartmentStartX + numApartments * apartmentWidth;
        if (lastWallX < buildingX + buildingWidth - 1) {
            for (int y = buildingY; y < buildingY + apartmentDepth; y++) {
                grid[z][y][lastWallX] = CELL_WALL;
            }
        }
        
        // West staircase area
        int westStairX = buildingX + 1;
        int stairY = buildingY + 1;
        // Ladder in west staircase (on all floors)
        grid[z][stairY][westStairX] = CELL_LADDER;
        
        // East staircase area  
        int eastStairX = buildingX + buildingWidth - 2;
        // Ladder in east staircase (on all floors)
        grid[z][stairY][eastStairX] = CELL_LADDER;
    }
    
    // Ground floor entrance at z=1 (door in south wall)
    int entranceX = buildingX + buildingWidth / 2;
    PlaceFloor(entranceX, buildingY + buildingDepth - 1, 1);
    // Second entrance on the other side
    PlaceFloor(entranceX + 2, buildingY + buildingDepth - 1, 1);
    
    needsRebuild = true;
}

// ============================================================================
// Castle Generator
// Medieval British walled castle with:
// - Rectangular outer wall with corner towers
// - Wall walk (z=2) with crenellations (merlons and crenels)
// - 2 stair towers for access to wall walk
// - Main gate on one side
// - Courtyard with small buildings (some 2-story)
// ============================================================================

void GenerateCastle(void) {
    InitGrid();  // Clear cells and flags
    if (!g_legacyWalkability) FillGroundLevel();
    
    // Castle dimensions - centered in grid
    int wallThickness = 2;
    int towerSize = 5;  // Corner towers are 5x5
    int stairTowerSize = 4;  // Stair towers are 4x4
    
    // Calculate castle size based on grid (leave margin around edges)
    int margin = 4;
    int castleWidth = gridWidth - 2 * margin;
    int castleHeight = gridHeight - 2 * margin;
    
    // Minimum size check
    if (castleWidth < 30) castleWidth = 30;
    if (castleHeight < 30) castleHeight = 30;
    
    int castleX = (gridWidth - castleWidth) / 2;
    int castleY = (gridHeight - castleHeight) / 2;
    
    // ========================================
    // Build outer walls (z=1, z=2, z=3 for DF-style)
    // ========================================
    for (int floor = 0; floor < 3; floor++) {
        int z = floor + 1;
        if (z >= gridDepth) break;
        
        // North wall
        for (int x = castleX; x < castleX + castleWidth; x++) {
            for (int t = 0; t < wallThickness; t++) {
                grid[z][castleY + t][x] = CELL_WALL;
            }
        }
        // South wall
        for (int x = castleX; x < castleX + castleWidth; x++) {
            for (int t = 0; t < wallThickness; t++) {
                grid[z][castleY + castleHeight - 1 - t][x] = CELL_WALL;
            }
        }
        // West wall
        for (int y = castleY; y < castleY + castleHeight; y++) {
            for (int t = 0; t < wallThickness; t++) {
                grid[z][y][castleX + t] = CELL_WALL;
            }
        }
        // East wall
        for (int y = castleY; y < castleY + castleHeight; y++) {
            for (int t = 0; t < wallThickness; t++) {
                grid[z][y][castleX + castleWidth - 1 - t] = CELL_WALL;
            }
        }
    }
    
    // ========================================
    // Wall walk at z=2 (solid floor to stand on in DF-style)
    // In DF mode, movers at z=2 need solid below, so we place CELL_FLOOR at z=2
    // which makes z=2 walkable (standing on wall tops)
    // ========================================
    if (gridDepth > 2) {
        // North wall walk floor
        for (int x = castleX + wallThickness; x < castleX + castleWidth - wallThickness; x++) {
            PlaceFloor(x, castleY + wallThickness, 2);
        }
        // South wall walk floor
        for (int x = castleX + wallThickness; x < castleX + castleWidth - wallThickness; x++) {
            PlaceFloor(x, castleY + castleHeight - 1 - wallThickness, 2);
        }
        // West wall walk floor
        for (int y = castleY + wallThickness; y < castleY + castleHeight - wallThickness; y++) {
            PlaceFloor(castleX + wallThickness, y, 2);
        }
        // East wall walk floor
        for (int y = castleY + wallThickness; y < castleY + castleHeight - wallThickness; y++) {
            PlaceFloor(castleX + castleWidth - 1 - wallThickness, y, 2);
        }
        
        // Crenellations (merlons on outer edge at z=2)
        for (int x = castleX; x < castleX + castleWidth; x++) {
            // North crenellations
            if ((x - castleX) % 2 == 0) {
                grid[2][castleY][x] = CELL_WALL;  // Merlon
            }
            // South crenellations
            if ((x - castleX) % 2 == 0) {
                grid[2][castleY + castleHeight - 1][x] = CELL_WALL;  // Merlon
            }
        }
        for (int y = castleY; y < castleY + castleHeight; y++) {
            // West crenellations
            if ((y - castleY) % 2 == 0) {
                grid[2][y][castleX] = CELL_WALL;  // Merlon
            }
            // East crenellations
            if ((y - castleY) % 2 == 0) {
                grid[2][y][castleX + castleWidth - 1] = CELL_WALL;  // Merlon
            }
        }
    }
    
    // ========================================
    // Corner towers (all 4 corners, 3 levels at z=1,2,3)
    // ========================================
    int cornerPositions[4][2] = {
        {castleX, castleY},                                              // NW
        {castleX + castleWidth - towerSize, castleY},                    // NE
        {castleX, castleY + castleHeight - towerSize},                   // SW
        {castleX + castleWidth - towerSize, castleY + castleHeight - towerSize}  // SE
    };
    
    for (int t = 0; t < 4; t++) {
        int tx = cornerPositions[t][0];
        int ty = cornerPositions[t][1];
        
        for (int floor = 0; floor < 3; floor++) {
            int z = floor + 1;
            if (z >= gridDepth) break;
            
            for (int py = ty; py < ty + towerSize; py++) {
                for (int px = tx; px < tx + towerSize; px++) {
                    bool isBorder = (px == tx || px == tx + towerSize - 1 ||
                                    py == ty || py == ty + towerSize - 1);
                    if (isBorder) {
                        grid[z][py][px] = CELL_WALL;
                    } else {
                        PlaceFloor(px, py, z);
                    }
                }
            }
        }
        
        // Ladder in center of tower (connects z=1,2,3)
        int ladderX = tx + towerSize / 2;
        int ladderY = ty + towerSize / 2;
        for (int floor = 0; floor < 3; floor++) {
            int z = floor + 1;
            if (z >= gridDepth) break;
            grid[z][ladderY][ladderX] = CELL_LADDER;
        }
        
        // Door from tower to courtyard at z=1 (toward inside of castle)
        int doorY = (t < 2) ? (ty + towerSize - 1) : ty;  // NW/NE: south, SW/SE: north
        PlaceFloor(tx + towerSize / 2, doorY, 1);
        
        // Connect tower to wall walk at z=2
        if (gridDepth > 2) {
            // Vertical connection (N/S): NW/NE open south, SW/SE open north
            int connY = (t < 2) ? (ty + towerSize - 1) : ty;
            PlaceFloor(tx + towerSize / 2, connY, 2);
            
            // Horizontal connection (E/W): NW/SW open east, NE/SE open west
            int connX = (t == 0 || t == 2) ? (tx + towerSize - 1) : tx;
            PlaceFloor(connX, ty + towerSize / 2, 2);
        }
    }
    
    // ========================================
    // Stair towers (2 towers on opposite walls for wall walk access)
    // ========================================
    // Stair tower 1: West wall, middle
    int stair1X = castleX;
    int stair1Y = castleY + castleHeight / 2 - stairTowerSize / 2;
    
    // Stair tower 2: East wall, middle  
    int stair2X = castleX + castleWidth - stairTowerSize;
    int stair2Y = castleY + castleHeight / 2 - stairTowerSize / 2;
    
    // Build stair towers (3 levels at z=1,2,3)
    int stairTowers[2][2] = {{stair1X, stair1Y}, {stair2X, stair2Y}};
    
    for (int s = 0; s < 2; s++) {
        int sx = stairTowers[s][0];
        int sy = stairTowers[s][1];
        
        for (int floor = 0; floor < 3; floor++) {
            int z = floor + 1;
            if (z >= gridDepth) break;
            
            for (int py = sy; py < sy + stairTowerSize; py++) {
                for (int px = sx; px < sx + stairTowerSize; px++) {
                    bool isBorder = (px == sx || px == sx + stairTowerSize - 1 ||
                                    py == sy || py == sy + stairTowerSize - 1);
                    if (isBorder) {
                        grid[z][py][px] = CELL_WALL;
                    } else {
                        PlaceFloor(px, py, z);
                    }
                }
            }
        }
        
        // Ladder in stair tower (connects z=1,2,3)
        int ladderX = sx + stairTowerSize / 2;
        int ladderY = sy + stairTowerSize / 2;
        for (int floor = 0; floor < 3; floor++) {
            int z = floor + 1;
            if (z >= gridDepth) break;
            grid[z][ladderY][ladderX] = CELL_LADDER;
        }
        
        // Door to courtyard at z=1 and connection to wall walk at z=2
        int doorX = (s == 0) ? (sx + stairTowerSize - 1) : sx;  // West: east side, East: west side
        PlaceFloor(doorX, sy + stairTowerSize / 2, 1);
        if (gridDepth > 2) {
            PlaceFloor(doorX, sy + stairTowerSize / 2, 2);
        }
    }
    
    // ========================================
    // Main gate (large opening on south wall at z=1)
    // ========================================
    int gateWidth = 4;
    int gateX = castleX + castleWidth / 2 - gateWidth / 2;
    
    // Clear the gate opening at z=1 only (wall above remains)
    for (int x = gateX; x < gateX + gateWidth; x++) {
        for (int t = 0; t < wallThickness; t++) {
            PlaceFloor(x, castleY + castleHeight - 1 - t, 1);
        }
    }
    
    // ========================================
    // Courtyard floor (z=1, inside the walls)
    // ========================================
    int courtyardX = castleX + wallThickness;
    int courtyardY = castleY + wallThickness;
    int courtyardW = castleWidth - 2 * wallThickness;
    int courtyardH = castleHeight - 2 * wallThickness;
    
    for (int y = courtyardY; y < courtyardY + courtyardH; y++) {
        for (int x = courtyardX; x < courtyardX + courtyardW; x++) {
            if (grid[1][y][x] != CELL_LADDER) {
                PlaceFloor(x, y, 1);
            }
        }
    }
    
    // ========================================
    // Interior buildings in courtyard (DF-style: z=1+)
    // ========================================
    
    // 2-story buildings: barracks (NW) and armory (NE)
    int buildings[2][4] = {
        {courtyardX + 3, courtyardY + 3, 6, 5},                    // barracks
        {courtyardX + courtyardW - 9, courtyardY + 3, 6, 5}        // armory
    };
    
    for (int b = 0; b < 2; b++) {
        int bx = buildings[b][0], by = buildings[b][1];
        int bw = buildings[b][2], bh = buildings[b][3];
        
        for (int floor = 0; floor < 2 && floor + 1 < gridDepth; floor++) {
            int z = floor + 1;
            for (int py = by; py < by + bh; py++) {
                for (int px = bx; px < bx + bw; px++) {
                    bool isBorder = (px == bx || px == bx + bw - 1 ||
                                    py == by || py == by + bh - 1);
                    if (isBorder) {
                        grid[z][py][px] = CELL_WALL;
                    } else {
                        PlaceFloor(px, py, z);
                    }
                }
            }
        }
        // Door on south, ladder inside
        PlaceFloor(bx + bw / 2, by + bh - 1, 1);
        int lx = (b == 0) ? (bx + 1) : (bx + bw - 2);
        for (int floor = 0; floor < 2 && floor + 1 < gridDepth; floor++) {
            grid[floor + 1][by + 1][lx] = CELL_LADDER;
        }
    }
    
    // 1-story building: stables (center-south)
    int b3x = courtyardX + courtyardW / 2 - 4;
    int b3y = courtyardY + courtyardH - 10;
    for (int py = b3y; py < b3y + 4; py++) {
        for (int px = b3x; px < b3x + 8; px++) {
            bool isBorder = (px == b3x || px == b3x + 7 || py == b3y || py == b3y + 3);
            if (isBorder) {
                grid[1][py][px] = CELL_WALL;
            } else {
                PlaceFloor(px, py, 1);
            }
        }
    }
    PlaceFloor(b3x + 4, b3y, 1);  // Door on north
    
    needsRebuild = true;
}

// ============================================================================
// Council Estate Generator
// UK-style council estate with:
// - Tower blocks (tall apartment buildings with corridors and stairwells)
// - Low-rise terraced housing (rows of small connected units)
// - Winding paths and alleyways
// - Courtyards and green spaces
// - Multi-level with ladders for vertical connections
// ============================================================================

// Helper: Build a massive tower block at position with specified floors
// UK council tower blocks are large with multiple stairwell cores
// Features external gallery walkways (balconies) running the length of the building
// If vertical=true, the building is rotated 90 degrees (gallery on east side instead of south)
// width/height are the TOTAL footprint dimensions (X and Y) including gallery
static void BuildTowerBlock(int baseX, int baseY, int width, int height, int floors, bool vertical) {
    // In DF mode, z=0 is ground (dirt), buildings start at z=1
    int baseZ = g_legacyWalkability ? 0 : 1;
    int maxFloors = gridDepth - baseZ;
    if (floors > maxFloors) floors = maxFloors;
    if (floors < 1) floors = 1;
    
    // Gallery walkway width
    int galleryWidth = 2;
    
    // For horizontal: width is X (long), height is Y (short, includes gallery)
    // For vertical: width is X (short, includes gallery), height is Y (long)
    int length;         // The long axis (along the gallery corridor)
    int buildingShort;  // The short axis excluding gallery
    
    if (!vertical) {
        length = width;
        buildingShort = height - galleryWidth;
    } else {
        length = height;
        buildingShort = width - galleryWidth;
    }
    
    // Unit dimensions
    int unitSize = 5;  // Each flat is 5 cells along the corridor
    
    // Calculate number of units and stairwell cores based on length
    int numUnits = (length - 4) / unitSize;
    int numStairCores = 1 + length / 20;
    if (numStairCores < 2) numStairCores = 2;
    if (numStairCores > 5) numStairCores = 5;
    
    // Stairwell positions (evenly distributed along length)
    int stairPositions[5];
    for (int s = 0; s < numStairCores; s++) {
        stairPositions[s] = 2 + (s * (length - 4)) / (numStairCores - 1 > 0 ? numStairCores - 1 : 1);
    }
    
    for (int floor = 0; floor < floors; floor++) {
        int z = baseZ + floor;
        if (!vertical) {
            // === HORIZONTAL ORIENTATION (gallery on south) ===
            // X extent: baseX to baseX + width
            // Y extent: baseY to baseY + height (buildingShort + gallery)
            
            // North wall (back of building)
            for (int x = baseX; x < baseX + width; x++) {
                grid[z][baseY][x] = CELL_WALL;
            }
            
            // South wall of main building (separates from gallery)
            for (int x = baseX; x < baseX + width; x++) {
                grid[z][baseY + buildingShort - 1][x] = CELL_WALL;
            }
            
            // East and west end walls
            for (int y = baseY; y < baseY + buildingShort; y++) {
                grid[z][y][baseX] = CELL_WALL;
                grid[z][y][baseX + width - 1] = CELL_WALL;
            }
            
            // Interior floor of main building
            for (int y = baseY + 1; y < baseY + buildingShort - 1; y++) {
                for (int x = baseX + 1; x < baseX + width - 1; x++) {
                    PlaceFloor(x, y, z);
                }
            }
            
            // Divide into flats with internal walls
            for (int u = 1; u < numUnits; u++) {
                int wallX = baseX + 1 + u * unitSize;
                if (wallX >= baseX + width - 1) break;
                
                bool isStairwell = false;
                for (int s = 0; s < numStairCores; s++) {
                    if (abs(wallX - baseX - stairPositions[s]) < 3) {
                        isStairwell = true;
                        break;
                    }
                }
                
                if (!isStairwell) {
                    for (int y = baseY + 1; y < baseY + buildingShort - 2; y++) {
                        grid[z][y][wallX] = CELL_WALL;
                    }
                }
            }
            
            // Gallery walkway (south side)
            int galleryY = baseY + buildingShort;
            for (int x = baseX; x < baseX + width; x++) {
                for (int gy = 0; gy < galleryWidth; gy++) {
                    if (galleryY + gy < gridHeight) {
                        PlaceFloor(x, galleryY + gy, z);
                    }
                }
            }
            
            // Gallery railing
            if (galleryY + galleryWidth < gridHeight) {
                for (int x = baseX; x < baseX + width; x++) {
                    if ((x - baseX) % 8 != 0) {
                        grid[z][galleryY + galleryWidth - 1][x] = CELL_WALL;
                    }
                }
            }
            
            // Gallery end walls
            for (int gy = 0; gy < galleryWidth; gy++) {
                if (galleryY + gy < gridHeight) {
                    grid[z][galleryY + gy][baseX] = CELL_WALL;
                    grid[z][galleryY + gy][baseX + width - 1] = CELL_WALL;
                }
            }
            
            // Doors from flats to gallery
            for (int u = 0; u < numUnits; u++) {
                int doorX = baseX + 2 + u * unitSize + unitSize / 2;
                if (doorX >= baseX + width - 1) break;
                PlaceFloor(doorX, baseY + buildingShort - 1, z);
            }
            
            // Stairwell cores
            for (int s = 0; s < numStairCores; s++) {
                int stairX = baseX + stairPositions[s];
                int stairY = baseY + 1;
                
                for (int sy = 0; sy < 3 && stairY + sy < baseY + buildingShort - 1; sy++) {
                    for (int sx = 0; sx < 3 && stairX + sx < baseX + width - 1; sx++) {
                        PlaceFloor(stairX + sx, stairY + sy, z);
                    }
                }
                
                if (stairX > baseX + 1) {
                    grid[z][stairY][stairX - 1] = CELL_WALL;
                    grid[z][stairY + 1][stairX - 1] = CELL_WALL;
                }
                if (stairX + 3 < baseX + width - 1) {
                    grid[z][stairY][stairX + 3] = CELL_WALL;
                    grid[z][stairY + 1][stairX + 3] = CELL_WALL;
                }
                
                grid[z][stairY + 1][stairX + 1] = CELL_LADDER;
                PlaceFloor(stairX + 1, baseY + buildingShort - 1, z);
            }
        } else {
            // === VERTICAL ORIENTATION (gallery on east) ===
            // X extent: baseX to baseX + width (buildingShort + gallery)
            // Y extent: baseY to baseY + height
            
            // West wall (back of building)
            for (int y = baseY; y < baseY + height; y++) {
                grid[z][y][baseX] = CELL_WALL;
            }
            
            // East wall of main building (separates from gallery)
            for (int y = baseY; y < baseY + height; y++) {
                grid[z][y][baseX + buildingShort - 1] = CELL_WALL;
            }
            
            // North and south end walls
            for (int x = baseX; x < baseX + buildingShort; x++) {
                grid[z][baseY][x] = CELL_WALL;
                grid[z][baseY + height - 1][x] = CELL_WALL;
            }
            
            // Interior floor of main building
            for (int y = baseY + 1; y < baseY + height - 1; y++) {
                for (int x = baseX + 1; x < baseX + buildingShort - 1; x++) {
                    PlaceFloor(x, y, z);
                }
            }
            
            // Divide into flats with internal walls (horizontal walls)
            for (int u = 1; u < numUnits; u++) {
                int wallY = baseY + 1 + u * unitSize;
                if (wallY >= baseY + height - 1) break;
                
                bool isStairwell = false;
                for (int s = 0; s < numStairCores; s++) {
                    if (abs(wallY - baseY - stairPositions[s]) < 3) {
                        isStairwell = true;
                        break;
                    }
                }
                
                if (!isStairwell) {
                    for (int x = baseX + 1; x < baseX + buildingShort - 2; x++) {
                        grid[z][wallY][x] = CELL_WALL;
                    }
                }
            }
            
            // Gallery walkway (east side)
            int galleryX = baseX + buildingShort;
            for (int y = baseY; y < baseY + height; y++) {
                for (int gx = 0; gx < galleryWidth; gx++) {
                    if (galleryX + gx < gridWidth) {
                        PlaceFloor(galleryX + gx, y, z);
                    }
                }
            }
            
            // Gallery railing (east edge)
            if (galleryX + galleryWidth < gridWidth) {
                for (int y = baseY; y < baseY + height; y++) {
                    if ((y - baseY) % 8 != 0) {
                        grid[z][y][galleryX + galleryWidth - 1] = CELL_WALL;
                    }
                }
            }
            
            // Gallery end walls
            for (int gx = 0; gx < galleryWidth; gx++) {
                if (galleryX + gx < gridWidth) {
                    grid[z][baseY][galleryX + gx] = CELL_WALL;
                    grid[z][baseY + height - 1][galleryX + gx] = CELL_WALL;
                }
            }
            
            // Doors from flats to gallery
            for (int u = 0; u < numUnits; u++) {
                int doorY = baseY + 2 + u * unitSize + unitSize / 2;
                if (doorY >= baseY + height - 1) break;
                PlaceFloor(baseX + buildingShort - 1, doorY, z);
            }
            
            // Stairwell cores
            for (int s = 0; s < numStairCores; s++) {
                int stairY = baseY + stairPositions[s];
                int stairX = baseX + 1;
                
                for (int sy = 0; sy < 3 && stairY + sy < baseY + height - 1; sy++) {
                    for (int sx = 0; sx < 3 && stairX + sx < baseX + buildingShort - 1; sx++) {
                        PlaceFloor(stairX + sx, stairY + sy, z);
                    }
                }
                
                if (stairY > baseY + 1) {
                    grid[z][stairY - 1][stairX] = CELL_WALL;
                    grid[z][stairY - 1][stairX + 1] = CELL_WALL;
                }
                if (stairY + 3 < baseY + height - 1) {
                    grid[z][stairY + 3][stairX] = CELL_WALL;
                    grid[z][stairY + 3][stairX + 1] = CELL_WALL;
                }
                
                grid[z][stairY + 1][stairX + 1] = CELL_LADDER;
                PlaceFloor(baseX + buildingShort - 1, stairY + 1, z);
            }
        }
    }
    
    // === Ground floor entrances ===
    if (!vertical) {
        int galleryY = baseY + buildingShort;
        for (int s = 0; s < numStairCores; s++) {
            int stairX = baseX + stairPositions[s];
            if (galleryY + galleryWidth < gridHeight) {
                PlaceFloor(stairX + 1, galleryY + galleryWidth - 1, 0);
            }
        }
        PlaceFloor(baseX, galleryY, 0);
        PlaceFloor(baseX + width - 1, galleryY, 0);
    } else {
        int galleryX = baseX + buildingShort;
        for (int s = 0; s < numStairCores; s++) {
            int stairY = baseY + stairPositions[s];
            if (galleryX + galleryWidth < gridWidth) {
                PlaceFloor(galleryX + galleryWidth - 1, stairY + 1, 0);
            }
        }
        PlaceFloor(galleryX, baseY, 0);
        PlaceFloor(galleryX, baseY + height - 1, 0);
    }
}

// Helper: Build 2-story terraced housing row where each unit has its own internal staircase
// If vertical=true, the row runs north-south instead of east-west
static void BuildTerraceRow(int baseX, int baseY, int numUnits, int unitWidth, int unitDepth, bool doorsNorth, bool vertical) {
    int numFloors = 2;  // 2-story terraced houses
    if (numFloors > gridDepth) numFloors = gridDepth;
    
    if (!vertical) {
        // === HORIZONTAL ORIENTATION (row runs east-west) ===
        int totalWidth = numUnits * unitWidth;
        
        for (int z = 0; z < numFloors; z++) {
            // Outer walls
            for (int x = baseX; x < baseX + totalWidth; x++) {
                grid[z][baseY][x] = CELL_WALL;
                grid[z][baseY + unitDepth - 1][x] = CELL_WALL;
            }
            for (int y = baseY; y < baseY + unitDepth; y++) {
                grid[z][y][baseX] = CELL_WALL;
                grid[z][y][baseX + totalWidth - 1] = CELL_WALL;
            }
            
            // Interior floor
            for (int y = baseY + 1; y < baseY + unitDepth - 1; y++) {
                for (int x = baseX + 1; x < baseX + totalWidth - 1; x++) {
                    PlaceFloor(x, y, z);
                }
            }
            
            // Internal walls between units
            for (int u = 1; u < numUnits; u++) {
                int wallX = baseX + u * unitWidth;
                for (int y = baseY; y < baseY + unitDepth; y++) {
                    grid[z][y][wallX] = CELL_WALL;
                }
            }
        }
        
        // Ground floor doors for each unit
        for (int u = 0; u < numUnits; u++) {
            int doorX = baseX + u * unitWidth + unitWidth / 2;
            if (doorsNorth) {
                PlaceFloor(doorX, baseY, 0);
            } else {
                PlaceFloor(doorX, baseY + unitDepth - 1, 0);
            }
        }
        
        // Internal staircase (ladder) in each unit
        for (int u = 0; u < numUnits; u++) {
            int ladderX = baseX + u * unitWidth + 1;
            int ladderY = doorsNorth ? (baseY + unitDepth - 2) : (baseY + 1);
            
            for (int z = 0; z < numFloors; z++) {
                grid[z][ladderY][ladderX] = CELL_LADDER;
            }
        }
    } else {
        // === VERTICAL ORIENTATION (row runs north-south) ===
        int totalHeight = numUnits * unitWidth;  // unitWidth becomes the height of each unit
        
        for (int z = 0; z < numFloors; z++) {
            // Outer walls
            for (int y = baseY; y < baseY + totalHeight; y++) {
                grid[z][y][baseX] = CELL_WALL;
                grid[z][y][baseX + unitDepth - 1] = CELL_WALL;
            }
            for (int x = baseX; x < baseX + unitDepth; x++) {
                grid[z][baseY][x] = CELL_WALL;
                grid[z][baseY + totalHeight - 1][x] = CELL_WALL;
            }
            
            // Interior floor
            for (int y = baseY + 1; y < baseY + totalHeight - 1; y++) {
                for (int x = baseX + 1; x < baseX + unitDepth - 1; x++) {
                    PlaceFloor(x, y, z);
                }
            }
            
            // Internal walls between units (horizontal walls)
            for (int u = 1; u < numUnits; u++) {
                int wallY = baseY + u * unitWidth;
                for (int x = baseX; x < baseX + unitDepth; x++) {
                    grid[z][wallY][x] = CELL_WALL;
                }
            }
        }
        
        // Ground floor doors for each unit (on west or east side)
        for (int u = 0; u < numUnits; u++) {
            int doorY = baseY + u * unitWidth + unitWidth / 2;
            if (doorsNorth) {  // doorsNorth means doors on west side for vertical
                PlaceFloor(baseX, doorY, 0);
            } else {
                PlaceFloor(baseX + unitDepth - 1, doorY, 0);
            }
        }
        
        // Internal staircase (ladder) in each unit
        for (int u = 0; u < numUnits; u++) {
            int ladderY = baseY + u * unitWidth + 1;
            int ladderX = doorsNorth ? (baseX + unitDepth - 2) : (baseX + 1);
            
            for (int z = 0; z < numFloors; z++) {
                grid[z][ladderY][ladderX] = CELL_LADDER;
            }
        }
    }
}

void GenerateCouncilEstate(void) {
    InitGrid();  // Clear cells and flags properly
    if (!g_legacyWalkability) FillGroundLevel();
    
    // Scale building count based on grid size
    int numTerraceRows = 4 + (gridWidth * gridHeight) / 6000;
    if (numTerraceRows > 12) numTerraceRows = 12;
    
    typedef struct { int x, y, w, h; } Rect;
    Rect placed[30];
    int placedCount = 0;
    
    // Place ONE massive central tower block that scales with the world
    // This creates the dominant tower block typical of UK council estates
    // Gallery adds 2 cells to total footprint
    int galleryExtra = 2;
    
    {
        // Randomly choose orientation for main tower
        bool mainVertical = GetRandomValue(0, 1) == 0;
        
        // Tower length scales with world size - can be very long (this is the long axis)
        int maxLength = mainVertical ? gridHeight : gridWidth;
        int maxTowerLength = (maxLength * 65) / 100;  // Up to 65% of grid dimension
        int minTowerLength = 40;
        if (minTowerLength > maxTowerLength) minTowerLength = maxTowerLength;
        int towerLength = minTowerLength + GetRandomValue(0, maxTowerLength - minTowerLength);
        
        // Building depth (perpendicular to gallery, excluding gallery)
        int buildingDepth = 13 + GetRandomValue(0, 8);  // 13-21 cells
        int maxDepthAvail = (mainVertical ? gridWidth : gridHeight) / 3;
        if (buildingDepth > maxDepthAvail) buildingDepth = maxDepthAvail;
        
        // Total depth including gallery
        int totalDepth = buildingDepth + galleryExtra;
        
        // Use all z-levels for the main tower
        int towerFloors = gridDepth;
        
        // Actual footprint for collision detection
        // Horizontal: width=towerLength, height=totalDepth
        // Vertical: width=totalDepth, height=towerLength
        int footprintW = mainVertical ? totalDepth : towerLength;
        int footprintH = mainVertical ? towerLength : totalDepth;
        
        // Center the main tower block
        int tx = (gridWidth - footprintW) / 2;
        int ty = (gridHeight - footprintH) / 2;
        if (!mainVertical) {
            ty -= gridHeight / 6;  // Slightly north of center for horizontal
        } else {
            tx -= gridWidth / 6;   // Slightly west of center for vertical
        }
        
        // Clamp to valid positions
        if (tx < 5) tx = 5;
        if (ty < 5) ty = 5;
        if (tx + footprintW > gridWidth - 5) footprintW = gridWidth - tx - 5;
        if (ty + footprintH > gridHeight - 5) footprintH = gridHeight - ty - 5;
        
        // BuildTowerBlock params: width is first param (X size), depth is second (Y size for horiz)
        // For horizontal: pass (length, totalDepth)
        // For vertical: pass (totalDepth, length) - but function swaps internally based on vertical flag
        if (!mainVertical) {
            BuildTowerBlock(tx, ty, footprintW, footprintH, towerFloors, false);
        } else {
            BuildTowerBlock(tx, ty, footprintW, footprintH, towerFloors, true);
        }
        placed[placedCount++] = (Rect){tx, ty, footprintW, footprintH};
    }
    
    // Optionally add 1-2 smaller secondary tower blocks if space permits
    int numSecondaryTowers = (gridWidth > 150 && gridHeight > 150) ? 1 + GetRandomValue(0, 1) : 0;
    for (int t = 0; t < numSecondaryTowers; t++) {
        // Randomly choose orientation
        bool vertical = GetRandomValue(0, 1) == 0;
        
        // Secondary towers are smaller
        int towerLength = 25 + GetRandomValue(0, 20);   // 25-45 cells long
        int buildingDepth = 10 + GetRandomValue(0, 6);  // 10-16 cells deep (excluding gallery)
        int towerFloors = gridDepth - GetRandomValue(1, gridDepth / 2);
        if (towerFloors < 2) towerFloors = 2;
        
        int maxLen = vertical ? gridHeight / 3 : gridWidth / 3;
        int maxDep = (vertical ? gridWidth : gridHeight) / 4;
        if (towerLength > maxLen) towerLength = maxLen;
        if (buildingDepth > maxDep) buildingDepth = maxDep;
        
        int totalDepth = buildingDepth + galleryExtra;
        int footprintW = vertical ? totalDepth : towerLength;
        int footprintH = vertical ? towerLength : totalDepth;
        
        int attempts = 100;
        while (attempts-- > 0) {
            int tx = 5 + GetRandomValue(0, gridWidth - footprintW - 10);
            int ty = 5 + GetRandomValue(0, gridHeight - footprintH - 10);
            
            // Check overlap with existing buildings
            bool overlaps = false;
            for (int p = 0; p < placedCount; p++) {
                int margin = 10;
                if (tx < placed[p].x + placed[p].w + margin && tx + footprintW + margin > placed[p].x &&
                    ty < placed[p].y + placed[p].h + margin && ty + footprintH + margin > placed[p].y) {
                    overlaps = true;
                    break;
                }
            }
            
            if (!overlaps) {
                BuildTowerBlock(tx, ty, footprintW, footprintH, towerFloors, vertical);
                placed[placedCount++] = (Rect){tx, ty, footprintW, footprintH};
                break;
            }
        }
    }
    
    // Place terraced housing rows (2-story, multiple units)
    // These fill in the spaces around the massive tower blocks
    // Randomly oriented horizontally or vertically
    int terraceUnitSize = 5;    // Size along the row
    int terraceUnitDepth = 6;   // Depth perpendicular to row
    
    for (int r = 0; r < numTerraceRows && placedCount < 30; r++) {
        // Randomly choose orientation
        bool vertical = GetRandomValue(0, 1) == 0;
        
        // Vary the number of units per row
        int unitsPerRow = 5 + GetRandomValue(0, 5);  // 5-10 units per row
        int rowLength = unitsPerRow * terraceUnitSize;
        
        // Ensure row fits in grid
        int maxLen = vertical ? gridHeight - 10 : gridWidth - 10;
        if (rowLength > maxLen) {
            unitsPerRow = maxLen / terraceUnitSize;
            rowLength = unitsPerRow * terraceUnitSize;
        }
        
        // Calculate bounding box based on orientation
        int rowWidth = vertical ? terraceUnitDepth : rowLength;
        int rowHeight = vertical ? rowLength : terraceUnitDepth;
        
        int attempts = 80;
        while (attempts-- > 0) {
            int rx = 5 + GetRandomValue(0, gridWidth - rowWidth - 10);
            int ry = 5 + GetRandomValue(0, gridHeight - rowHeight - 10);
            
            // Check overlap
            bool overlaps = false;
            for (int p = 0; p < placedCount; p++) {
                int margin = 5;
                if (rx < placed[p].x + placed[p].w + margin && rx + rowWidth + margin > placed[p].x &&
                    ry < placed[p].y + placed[p].h + margin && ry + rowHeight + margin > placed[p].y) {
                    overlaps = true;
                    break;
                }
            }
            
            if (!overlaps) {
                bool doorsNorth = GetRandomValue(0, 1) == 0;
                BuildTerraceRow(rx, ry, unitsPerRow, terraceUnitSize, terraceUnitDepth, doorsNorth, vertical);
                placed[placedCount++] = (Rect){rx, ry, rowWidth, rowHeight};
                break;
            }
        }
    }
    
    // Scatter trees/debris in open areas (CELL_DIRT only, not inside buildings)
    // This adds visual interest to the green spaces around the estate
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            if (grid[0][y][x] == CELL_DIRT && GetRandomValue(0, 100) < 3) {
                grid[1][y][x] = CELL_WALL;  // Tree or debris
            }
        }
    }
    
    needsRebuild = true;
}

// ============================================================================
void GenerateMixed(void) {
    InitGrid();
    int zoneSize = chunkWidth * 4;
    int zonesX = (gridWidth + zoneSize - 1) / zoneSize;
    int zonesY = (gridHeight + zoneSize - 1) / zoneSize;
    int zones[16][16];
    for (int zy = 0; zy < zonesY && zy < 16; zy++)
        for (int zx = 0; zx < zonesX && zx < 16; zx++)
            zones[zy][zx] = GetRandomValue(0, 100) < 50 ? 1 : 0;

    for (int wy = chunkHeight; wy < gridHeight; wy += chunkHeight / 2) {
        for (int wx = 0; wx < gridWidth; wx++) {
            int zx = wx / zoneSize, zy = wy / zoneSize;
            if (zx >= 16 || zy >= 16 || zones[zy][zx] == 0) { wx += GetRandomValue(10, 30); continue; }
            int gapPos = GetRandomValue(6, 20);
            int gapSize = GetRandomValue(3, 6);
            for (int x = wx; x < wx + gapPos && x < gridWidth; x++) {
                int zx2 = x / zoneSize;
                if (zx2 < 16 && zones[zy][zx2] == 1) {
                    grid[1][wy][x] = CELL_WALL;
                    if (wy + 1 < gridHeight) grid[1][wy + 1][x] = CELL_WALL;
                }
            }
            wx += gapPos + gapSize;
        }
    }
    for (int wx = chunkWidth; wx < gridWidth; wx += chunkWidth / 2) {
        for (int wy = 0; wy < gridHeight; wy++) {
            int zx = wx / zoneSize, zy = wy / zoneSize;
            if (zx >= 16 || zy >= 16 || zones[zy][zx] == 0) { wy += GetRandomValue(10, 30); continue; }
            int gapPos = GetRandomValue(6, 20);
            int gapSize = GetRandomValue(3, 6);
            for (int y = wy; y < wy + gapPos && y < gridHeight; y++) {
                int zy2 = y / zoneSize;
                if (zy2 < 16 && zones[zy2][zx] == 1) {
                    grid[1][y][wx] = CELL_WALL;
                    if (wx + 1 < gridWidth) grid[1][y][wx + 1] = CELL_WALL;
                }
            }
            wy += gapPos + gapSize;
        }
    }
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            if (grid[0][y][x] == CELL_DIRT) {
                int zx = x / zoneSize, zy = y / zoneSize;
                bool isCity = (zx < 16 && zy < 16 && zones[zy][zx] == 1);
                int chance = isCity ? 3 : 15;
                if (GetRandomValue(0, 100) < chance) grid[1][y][x] = CELL_WALL;
            }
        }
    }
    needsRebuild = true;
}


// ============================================================================
// Crafting Test Scenario
// Small map with stonecutter workshop, stockpiles, and walls to mine
// Tests the full crafting loop: mine -> haul -> craft -> haul -> build
// ============================================================================

void GenerateCraftingTest(void) {
    // Use small grid for testing
    InitGridWithSizeAndChunkSize(30, 30, 16, 16);
    
    // Clear entities from previous map
    ClearWorkshops();
    ClearStockpiles();
    ClearItems();
    InitDesignations();
    
    // Clear blueprints
    for (int i = 0; i < MAX_BLUEPRINTS; i++) {
        blueprints[i].active = false;
    }
    blueprintCount = 0;
    
    // Fill z=0 with dirt (ground) - this is the walkable level
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            grid[0][y][x] = CELL_DIRT;
            SET_CELL_SURFACE(x, y, 0, SURFACE_GRASS);
        }
    }
    
    // Add minable walls in a cluster (top-left area) at z=1 (above ground)
    for (int y = 2; y <= 5; y++) {
        for (int x = 2; x <= 5; x++) {
            grid[1][y][x] = CELL_WALL;
        }
    }
    
    // Pre-designate some walls for mining
    for (int y = 2; y <= 4; y++) {
        for (int x = 2; x <= 4; x++) {
            DesignateDig(x, y, 1);
        }
    }
    
    // Create stonecutter workshop at (15, 15) on z=1 (walking level)
    // Workshop is 3x3, work tile at center, output tile to the right
    int wsIdx = CreateWorkshop(15, 15, 1, WORKSHOP_STONECUTTER);
    if (wsIdx >= 0) {
        // Add a "Cut Stone Blocks" bill - DO_FOREVER
        AddBill(wsIdx, 0, BILL_DO_FOREVER, 0);
    }
    
    // Create input stockpile (2x2) to the left of workshop - accepts ORANGE only
    int inputSp = CreateStockpile(12, 15, 1, 2, 2);
    if (inputSp >= 0) {
        // Disable all types except ORANGE
        for (int t = 0; t < ITEM_TYPE_COUNT; t++) {
            SetStockpileFilter(inputSp, t, false);
        }
        SetStockpileFilter(inputSp, ITEM_ORANGE, true);
        SetStockpilePriority(inputSp, 7);  // High priority
    }
    
    // Create output stockpile (2x2) to the right of workshop - accepts STONE_BLOCKS only
    int outputSp = CreateStockpile(19, 15, 1, 2, 2);
    if (outputSp >= 0) {
        for (int t = 0; t < ITEM_TYPE_COUNT; t++) {
            SetStockpileFilter(outputSp, t, false);
        }
        SetStockpileFilter(outputSp, ITEM_STONE_BLOCKS, true);
        SetStockpilePriority(outputSp, 7);
    }
    
    // Create construction stockpile (2x2) near build site - accepts STONE_BLOCKS
    int buildSp = CreateStockpile(8, 10, 1, 2, 2);
    if (buildSp >= 0) {
        for (int t = 0; t < ITEM_TYPE_COUNT; t++) {
            SetStockpileFilter(buildSp, t, false);
        }
        SetStockpileFilter(buildSp, ITEM_STONE_BLOCKS, true);
        SetStockpilePriority(buildSp, 5);  // Normal priority
    }
    
    // Create wall blueprint at (6, 10) - requires STONE_BLOCKS
    CreateBuildBlueprint(6, 10, 1);
    
    needsRebuild = true;
}

