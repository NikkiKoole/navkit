#include "terrain.h"
#include "../vendor/raylib.h"
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
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++)
            grid[0][y][x] = CELL_WALKABLE;
}

void GenerateSparse(float density) {
    InitGrid();
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++)
            if ((float)GetRandomValue(0, 100) / 100.0f < density)
                grid[0][y][x] = CELL_WALL;
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
                grid[0][py][px] = CELL_WALKABLE;
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
                    grid[0][y + w][x] = CELL_WALKABLE;
            }
            x += (x2 > x) ? 1 : -1;
        }
        while (y != y2) {
            for (int w = 0; w < CORRIDOR_WIDTH; w++) {
                if (y >= 0 && y < gridHeight && x + w >= 0 && x + w < gridWidth)
                    grid[0][y][x + w] = CELL_WALKABLE;
            }
            y += (y2 > y) ? 1 : -1;
        }
    } else {
        // Vertical then horizontal
        while (y != y2) {
            for (int w = 0; w < CORRIDOR_WIDTH; w++) {
                if (y >= 0 && y < gridHeight && x + w >= 0 && x + w < gridWidth)
                    grid[0][y][x + w] = CELL_WALKABLE;
            }
            y += (y2 > y) ? 1 : -1;
        }
        while (x != x2) {
            for (int w = 0; w < CORRIDOR_WIDTH; w++) {
                if (y + w >= 0 && y + w < gridHeight && x >= 0 && x < gridWidth)
                    grid[0][y + w][x] = CELL_WALKABLE;
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
    // Fill with walls
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++)
            grid[0][y][x] = CELL_WALL;
    
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
    // Start with random noise
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            // Border is always wall
            if (x == 0 || y == 0 || x == gridWidth - 1 || y == gridHeight - 1) {
                grid[0][y][x] = CELL_WALL;
            } else {
                grid[0][y][x] = (GetRandomValue(0, 100) < 45) ? CELL_WALL : CELL_WALKABLE;
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
                temp[y * gridWidth + x] = (walls >= 5) ? CELL_WALL : CELL_WALKABLE;
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
                    grid[0][ny][nx] = CELL_WALKABLE;
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
    // Fill with walls
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++)
            grid[0][y][x] = CELL_WALL;
    
    // Start from center
    int x = gridWidth / 2;
    int y = gridHeight / 2;
    
    // Target: carve out ~40% of the map
    int targetFloor = (gridWidth * gridHeight * 40) / 100;
    int floorCount = 0;
    
    int maxSteps = gridWidth * gridHeight * 10;  // Prevent infinite loop
    
    for (int step = 0; step < maxSteps && floorCount < targetFloor; step++) {
        // Carve current position
        if (grid[0][y][x] == CELL_WALL) {
            grid[0][y][x] = CELL_WALKABLE;
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
                grid[0][y][x] = CELL_WALKABLE;
            }
        }
    }
}

static void CarveHorizontalTunnel(int x1, int x2, int y) {
    int minX = x1 < x2 ? x1 : x2;
    int maxX = x1 > x2 ? x1 : x2;
    for (int x = minX; x <= maxX; x++) {
        if (x > 0 && x < gridWidth - 1 && y > 0 && y < gridHeight - 1) {
            grid[0][y][x] = CELL_WALKABLE;
        }
    }
}

static void CarveVerticalTunnel(int y1, int y2, int x) {
    int minY = y1 < y2 ? y1 : y2;
    int maxY = y1 > y2 ? y1 : y2;
    for (int y = minY; y <= maxY; y++) {
        if (x > 0 && x < gridWidth - 1 && y > 0 && y < gridHeight - 1) {
            grid[0][y][x] = CELL_WALKABLE;
        }
    }
}

void GenerateTunneler(void) {
    // Fill with walls
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++)
            grid[0][y][x] = CELL_WALL;
    
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
                    grid[0][y][x] = CELL_WALKABLE;
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
                grid[0][top + t][x] = CELL_WALL;
            }
        }
        
        // Draw bottom wall
        if (gapSide == 1) {
            gapStart = left + (right - left) / 2 - gapSize / 2;
        }
        for (int x = left; x <= right; x++) {
            if (gapSide == 1 && x >= gapStart && x < gapStart + gapSize) continue;
            for (int t = 0; t < wallThickness && bottom - t >= 0; t++) {
                grid[0][bottom - t][x] = CELL_WALL;
            }
        }
        
        // Draw left wall
        if (gapSide == 2) {
            gapStart = top + (bottom - top) / 2 - gapSize / 2;
        }
        for (int y = top; y <= bottom; y++) {
            if (gapSide == 2 && y >= gapStart && y < gapStart + gapSize) continue;
            for (int t = 0; t < wallThickness && left + t < gridWidth; t++) {
                grid[0][y][left + t] = CELL_WALL;
            }
        }
        
        // Draw right wall
        if (gapSide == 0) {
            gapStart = top + (bottom - top) / 2 - gapSize / 2;
        }
        for (int y = top; y <= bottom; y++) {
            if (gapSide == 0 && y >= gapStart && y < gapStart + gapSize) continue;
            for (int t = 0; t < wallThickness && right - t >= 0; t++) {
                grid[0][y][right - t] = CELL_WALL;
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
                grid[0][y][x] = CELL_WALL;
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
                    grid[0][wy][x] = CELL_WALL;
                    if (wy + 1 < gridHeight) grid[0][wy + 1][x] = CELL_WALL;
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
                    grid[0][y][wx] = CELL_WALL;
                    if (wx + 1 < gridWidth) grid[0][y][wx + 1] = CELL_WALL;
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
                grid[0][wy][x] = CELL_WALL;
                if (wy + 1 < gridHeight) grid[0][wy + 1][x] = CELL_WALL;
            }
            wx += gapPos + gapSize;
        }
    }
    for (int wx = chunkWidth; wx < gridWidth; wx += chunkWidth / 2) {
        for (int wy = 0; wy < gridHeight; wy++) {
            int gapPos = GetRandomValue(6, 20);
            int gapSize = GetRandomValue(3, 6);
            for (int y = wy; y < wy + gapPos && y < gridHeight; y++) {
                grid[0][y][wx] = CELL_WALL;
                if (wx + 1 < gridWidth) grid[0][y][wx + 1] = CELL_WALL;
            }
            wy += gapPos + gapSize;
        }
    }
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++)
            if (grid[0][y][x] == CELL_WALKABLE && GetRandomValue(0, 100) < 5)
                grid[0][y][x] = CELL_WALL;
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
    // This adds floors, walls, and ladders up to bridgeZ+1
    Tower* towersToExtend[2] = {t1, t2};
    for (int ti = 0; ti < 2; ti++) {
        Tower* t = towersToExtend[ti];
        int newHeight = bridgeZ + 1;
        if (t->height < newHeight) {
            // Extend tower structure up to new height
            for (int z = t->height; z < newHeight; z++) {
                for (int py = t->y; py < t->y + t->h; py++) {
                    for (int px = t->x; px < t->x + t->w; px++) {
                        bool isBorder = (px == t->x || px == t->x + t->w - 1 || 
                                        py == t->y || py == t->y + t->h - 1);
                        grid[z][py][px] = isBorder ? CELL_WALL : CELL_FLOOR;
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
        for (int z = 0; z < t->height; z++) {
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
    
    // Open the tower walls at bridge connection points
    grid[bridgeZ][startY][startX] = CELL_FLOOR;
    grid[bridgeZ][endY][endX] = CELL_FLOOR;
    
    // Horizontal segment
    while (x != endX) {
        if (x >= 0 && x < gridWidth && y >= 0 && y < gridHeight) {
            if (grid[bridgeZ][y][x] == CELL_AIR) {
                grid[bridgeZ][y][x] = CELL_FLOOR;
            }
        }
        x += (endX > x) ? 1 : -1;
    }
    // Vertical segment  
    while (y != endY) {
        if (x >= 0 && x < gridWidth && y >= 0 && y < gridHeight) {
            if (grid[bridgeZ][y][x] == CELL_AIR) {
                grid[bridgeZ][y][x] = CELL_FLOOR;
            }
        }
        y += (endY > y) ? 1 : -1;
    }
}

void GenerateTowers(void) {
    // Clear all levels: z=0 is ground (walkable), z>0 is air
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            grid[0][y][x] = CELL_WALKABLE;
        }
    }
    for (int z = 1; z < 3; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                grid[z][y][x] = CELL_AIR;
            }
        }
    }
    
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
            
            // Build the tower: walls on border, floor inside, at all z-levels
            for (int z = 0; z < tHeight; z++) {
                for (int py = ty; py < ty + th; py++) {
                    for (int px = tx; px < tx + tw; px++) {
                        bool isBorder = (px == tx || px == tx + tw - 1 || 
                                        py == ty || py == ty + th - 1);
                        grid[z][py][px] = isBorder ? CELL_WALL : CELL_FLOOR;
                    }
                }
            }
            
            // Add ladder inside tower (connects all levels)
            // Ladders must exist on BOTH z-levels to allow climbing between them
            int ladderX = tx + tw / 2;
            int ladderY = ty + th / 2;
            for (int z = 0; z < tHeight; z++) {
                grid[z][ladderY][ladderX] = CELL_LADDER;
            }
            
            // Add door at z=0 (opening in wall)
            int doorSide = GetRandomValue(0, 3);
            switch (doorSide) {
                case 0: grid[0][ty][tx + tw / 2] = CELL_FLOOR; break;          // North
                case 1: grid[0][ty + th / 2][tx + tw - 1] = CELL_FLOOR; break; // East
                case 2: grid[0][ty + th - 1][tx + tw / 2] = CELL_FLOOR; break; // South
                case 3: grid[0][ty + th / 2][tx] = CELL_FLOOR; break;          // West
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
        // Ladders on ALL levels of the tower
        for (int z = 0; z < t->height; z++) {
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
    // Clear all levels: z=0 is ground (walkable), z>0 is air
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            grid[0][y][x] = CELL_WALKABLE;
        }
    }
    for (int z = 1; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                grid[z][y][x] = CELL_AIR;
            }
        }
    }
    
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
    
    // Build each floor
    for (int z = 0; z < numFloors; z++) {
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
                grid[z][y][x] = CELL_FLOOR;
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
                grid[z][buildingY + apartmentDepth - 1][doorX] = CELL_FLOOR;
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
    
    // Ground floor entrance (door in south wall)
    int entranceX = buildingX + buildingWidth / 2;
    grid[0][buildingY + buildingDepth - 1][entranceX] = CELL_FLOOR;
    // Second entrance on the other side
    grid[0][buildingY + buildingDepth - 1][entranceX + 2] = CELL_FLOOR;
    
    needsRebuild = true;
}

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
                    grid[0][wy][x] = CELL_WALL;
                    if (wy + 1 < gridHeight) grid[0][wy + 1][x] = CELL_WALL;
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
                    grid[0][y][wx] = CELL_WALL;
                    if (wx + 1 < gridWidth) grid[0][y][wx + 1] = CELL_WALL;
                }
            }
            wy += gapPos + gapSize;
        }
    }
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            if (grid[0][y][x] == CELL_WALKABLE) {
                int zx = x / zoneSize, zy = y / zoneSize;
                bool isCity = (zx < 16 && zy < 16 && zones[zy][zx] == 1);
                int chance = isCity ? 3 : 15;
                if (GetRandomValue(0, 100) < chance) grid[0][y][x] = CELL_WALL;
            }
        }
    }
    needsRebuild = true;
}
