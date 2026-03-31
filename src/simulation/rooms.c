// rooms.c - Room detection, typing, and quality scoring
//
// Flood-fill BFS per z-level detects enclosed rooms bounded by walls/doors/windows.
// Rooms are classified by furniture/workshop contents and scored for quality.

#include "rooms.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include "../world/material.h"
#include "../entities/furniture.h"
#include "../entities/workshops.h"
#include "../entities/stockpiles.h"
#include "lighting.h"
#include "floordirt.h"

uint16_t roomGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
DetectedRoom rooms[MAX_DETECTED_ROOMS];
int roomCount = 0;
bool roomsDirty = true;
bool roomsEnabled = true;

// --- BFS data (reused per z-level) ---

typedef struct { int x, y; } RoomBfsNode;
#define ROOM_BFS_MAX (MAX_GRID_WIDTH * MAX_GRID_HEIGHT)
static RoomBfsNode roomBfsQueue[ROOM_BFS_MAX];
static RoomBfsNode roomCellBuffer[ROOM_BFS_MAX];
// Visited grid per z-level (marks cells we've already processed)
static bool visited[MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// --- Helpers ---

static bool IsCookingWorkshop(WorkshopType type) {
    return type == WORKSHOP_CAMPFIRE ||
           type == WORKSHOP_GROUND_FIRE ||
           type == WORKSHOP_HEARTH;
}

static bool IsCraftingWorkshop(WorkshopType type) {
    return type == WORKSHOP_STONECUTTER ||
           type == WORKSHOP_SAWMILL ||
           type == WORKSHOP_CARPENTER ||
           type == WORKSHOP_KILN ||
           type == WORKSHOP_CHARCOAL_PIT ||
           type == WORKSHOP_BUTCHER ||
           type == WORKSHOP_QUERN ||
           type == WORKSHOP_LOOM ||
           type == WORKSHOP_TANNING_RACK ||
           type == WORKSHOP_TAILOR ||
           type == WORKSHOP_MUD_MIXER ||
           type == WORKSHOP_ROPE_MAKER;
}

static bool IsBedFurniture(FurnitureType type) {
    return type == FURNITURE_PLANK_BED ||
           type == FURNITURE_LEAF_PILE ||
           type == FURNITURE_GRASS_PILE;
}

static float ComputeLightLevel(int x, int y, int z) {
    // Combine sky light and block light into a 0-15 scale
    float sky = (float)lightGrid[z][y][x].skyLevel;
    float blockMax = lightGrid[z][y][x].blockR;
    if (lightGrid[z][y][x].blockG > blockMax) blockMax = lightGrid[z][y][x].blockG;
    if (lightGrid[z][y][x].blockB > blockMax) blockMax = lightGrid[z][y][x].blockB;
    float block = blockMax / 255.0f * 15.0f;
    return sky > block ? sky : block;
}

// --- Flood fill ---

// BFS flood fill from seed (sx, sy) at z-level.
// Returns number of cells found. Sets touchedEdge if flood reaches grid boundary.
// Cells written to roomCellBuffer[0..n-1]. Marks visited[][].
static int FloodFill(int sx, int sy, int z, bool *touchedEdge) {
    int cellCount = 0;
    int qHead = 0, qTail = 0;
    *touchedEdge = false;

    roomBfsQueue[qTail++] = (RoomBfsNode){sx, sy};
    visited[sy][sx] = true;

    while (qHead < qTail) {
        RoomBfsNode cur = roomBfsQueue[qHead++];
        roomCellBuffer[cellCount++] = cur;

        // Check if at grid edge — means this space is open to outside
        if (cur.x == 0 || cur.x == gridWidth - 1 ||
            cur.y == 0 || cur.y == gridHeight - 1) {
            *touchedEdge = true;
        }

        // Expand to 4 cardinal neighbors
        static const int dx[] = {1, -1, 0, 0};
        static const int dy[] = {0, 0, 1, -1};
        for (int d = 0; d < 4; d++) {
            int nx = cur.x + dx[d];
            int ny = cur.y + dy[d];

            if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) {
                *touchedEdge = true;
                continue;
            }
            if (visited[ny][nx]) continue;

            CellType cell = grid[z][ny][nx];

            // Walls/windows stop the flood entirely (not added to room)
            if (CellBlocksMovement(cell) && cell != CELL_DOOR) continue;

            // Doors: boundary — don't expand through, don't add to cell buffer.
            // Door cells get assigned to rooms in a post-pass.
            if (cell == CELL_DOOR) {
                continue;
            }

            // Walkable cell — must actually be walkable (has support)
            if (!IsCellWalkableAt(z, ny, nx)) continue;

            visited[ny][nx] = true;
            roomBfsQueue[qTail++] = (RoomBfsNode){nx, ny};
        }
    }

    return cellCount;
}

// --- Room classification ---

static void ClassifyRoom(DetectedRoom *room) {
    // Priority order: kitchen > workshop > barracks > bedroom > dining > storage > generic
    if (room->cookingWorkshopCount > 0) {
        room->type = ROOM_TYPE_KITCHEN;
    } else if (room->craftingWorkshopCount > 0) {
        room->type = ROOM_TYPE_WORKSHOP;
    } else if (room->bedCount >= 2) {
        room->type = ROOM_TYPE_BARRACKS;
    } else if (room->bedCount == 1) {
        room->type = ROOM_TYPE_BEDROOM;
    } else if (room->chairCount > 0) {
        room->type = ROOM_TYPE_DINING;
    } else if (room->stockpileCellCount > 0) {
        room->type = ROOM_TYPE_STORAGE;
    } else {
        room->type = ROOM_TYPE_GENERIC;
    }
}

// --- Quality scoring ---

static float ComputeQuality(DetectedRoom *room) {
    float q = 0.0f;

    // Positive: size (diminishing returns, caps at 36 cells = 6x6)
    int sizeCap = room->cellCount < 36 ? room->cellCount : 36;
    q += (float)sizeCap / 36.0f * 0.15f;

    // Positive: furniture count
    int furnCap = room->furnCount < 6 ? room->furnCount : 6;
    q += (float)furnCap / 6.0f * 0.15f;

    // Positive: furniture variety
    int varCap = room->furnVariety < 3 ? room->furnVariety : 3;
    q += (float)varCap / 3.0f * 0.10f;

    // Positive: light level (0-15 scale)
    float lightCap = room->avgLight < 15.0f ? room->avgLight : 15.0f;
    q += lightCap / 15.0f * 0.20f;

    // Positive: constructed floors
    q += room->constructedFloorFrac * 0.15f;

    // Positive: constructed walls
    q += room->constructedWallFrac * 0.10f;

    // Positive: windows
    int winCap = room->windowCount < 3 ? room->windowCount : 3;
    q += (float)winCap / 3.0f * 0.10f;

    // Negative: no light
    if (room->avgLight < 1.0f) q -= 0.20f;

    // Negative: dirty floors
    q -= (room->avgDirt / 255.0f) * 0.15f;

    // Negative: too small
    if (room->cellCount < 4) q -= 0.15f;

    // Negative: all natural walls
    if (room->constructedWallFrac < 0.1f) q -= 0.10f;

    // Clamp
    if (q < -1.0f) q = -1.0f;
    if (q > 1.0f) q = 1.0f;

    return q;
}

// --- Scan room cells for metadata ---

static void ScanRoomCells(uint16_t roomId, int z, RoomBfsNode *cells, int cellCount) {
    DetectedRoom *room = &rooms[roomId];
    room->z = z;
    room->cellCount = cellCount;
    room->active = true;

    // Accumulate quality inputs
    float totalLight = 0.0f;
    float totalDirt = 0.0f;
    int constructedFloors = 0;
    int totalWallNeighbors = 0;
    int constructedWallNeighbors = 0;
    bool furnTypesSeen[FURNITURE_TYPE_COUNT] = {0};

    // Track windows (avoid double-counting shared walls)
    // Simple: count unique window positions seen as neighbors
    int windowPositions = 0;

    for (int i = 0; i < cellCount; i++) {
        int cx = cells[i].x;
        int cy = cells[i].y;

        // Light
        totalLight += ComputeLightLevel(cx, cy, z);

        // Floor dirt
        totalDirt += (float)floorDirtGrid[z][cy][cx];

        // Constructed floor check
        if (HAS_FLOOR(cx, cy, z) && !IsFloorNatural(cx, cy, z)) {
            constructedFloors++;
        } else if (z > 0 && CellIsSolid(grid[z - 1][cy][cx]) && !IsWallNatural(cx, cy, z - 1)) {
            // Standing on top of a constructed wall below = constructed floor
            constructedFloors++;
        }

        // Furniture at this cell
        int furnIdx = GetFurnitureAt(cx, cy, z);
        if (furnIdx >= 0) {
            FurnitureType ft = furniture[furnIdx].type;
            room->furnCount++;
            if (!furnTypesSeen[ft]) {
                furnTypesSeen[ft] = true;
                room->furnVariety++;
            }
            if (IsBedFurniture(ft)) room->bedCount++;
            if (ft == FURNITURE_CHAIR) room->chairCount++;
        }

        // Stockpile check (inline to avoid dependency on main.c)
        for (int sp = 0; sp < MAX_STOCKPILES; sp++) {
            if (!stockpiles[sp].active || stockpiles[sp].z != z) continue;
            if (cx >= stockpiles[sp].x && cx < stockpiles[sp].x + stockpiles[sp].width &&
                cy >= stockpiles[sp].y && cy < stockpiles[sp].y + stockpiles[sp].height) {
                room->stockpileCellCount++;
                break;
            }
        }

        // Check 4 neighbors for walls/windows
        static const int dx[] = {1, -1, 0, 0};
        static const int dy[] = {0, 0, 1, -1};
        for (int d = 0; d < 4; d++) {
            int nx = cx + dx[d];
            int ny = cy + dy[d];
            if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;

            CellType ncell = grid[z][ny][nx];
            if (ncell == CELL_WINDOW) {
                windowPositions++;
            }
            if (CellBlocksMovement(ncell) && ncell != CELL_DOOR) {
                totalWallNeighbors++;
                if (!IsWallNatural(nx, ny, z)) {
                    constructedWallNeighbors++;
                }
            }
        }
    }

    room->avgLight = cellCount > 0 ? totalLight / (float)cellCount : 0.0f;
    room->avgDirt = cellCount > 0 ? totalDirt / (float)cellCount : 0.0f;
    room->constructedFloorFrac = cellCount > 0 ? (float)constructedFloors / (float)cellCount : 0.0f;
    room->constructedWallFrac = totalWallNeighbors > 0
        ? (float)constructedWallNeighbors / (float)totalWallNeighbors : 0.0f;
    // Rough deduplicate: each window is seen from up to 1 interior side typically
    room->windowCount = windowPositions;

    // Scan workshops that overlap this room
    for (int w = 0; w < MAX_WORKSHOPS; w++) {
        if (!workshops[w].active) continue;
        if (workshops[w].z != z) continue;
        // Check if any workshop tile falls in this room
        int wx = workshops[w].x;
        int wy = workshops[w].y;
        int ww = workshops[w].width;
        int wh = workshops[w].height;
        bool inRoom = false;
        for (int ty = wy; ty < wy + wh && !inRoom; ty++) {
            for (int tx = wx; tx < wx + ww && !inRoom; tx++) {
                if (tx >= 0 && tx < gridWidth && ty >= 0 && ty < gridHeight) {
                    if (roomGrid[z][ty][tx] == roomId) {
                        inRoom = true;
                    }
                }
            }
        }
        if (inRoom) {
            if (IsCookingWorkshop(workshops[w].type)) room->cookingWorkshopCount++;
            else if (IsCraftingWorkshop(workshops[w].type)) room->craftingWorkshopCount++;
        }
    }

    ClassifyRoom(room);
    room->quality = ComputeQuality(room);
}

// --- Main recompute ---

static void RecomputeRooms(void) {
    memset(roomGrid, 0, sizeof(roomGrid));
    memset(rooms, 0, sizeof(rooms));
    roomCount = 0;

    for (int z = 0; z < gridDepth; z++) {
        memset(visited, 0, sizeof(visited));

        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (visited[y][x]) continue;

                CellType cell = grid[z][y][x];

                // Skip walls/windows/doors — walls and windows aren't rooms,
                // doors are assigned in a post-pass
                if (CellBlocksMovement(cell) || cell == CELL_DOOR) {
                    visited[y][x] = true;
                    continue;
                }

                // Skip non-walkable cells (no support)
                if (!IsCellWalkableAt(z, y, x)) {
                    visited[y][x] = true;
                    continue;
                }

                // Flood fill from this cell
                bool touchedEdge = false;
                int count = FloodFill(x, y, z, &touchedEdge);

                // If flood reached grid edge, it's outdoors — skip
                if (touchedEdge) continue;

                // Too few cells — not a real room (need at least 1 interior cell)
                if (count < 1) continue;

                // Assign room ID
                if (roomCount >= MAX_DETECTED_ROOMS) continue;
                uint16_t roomId = (uint16_t)(roomCount + 1); // 1-based
                roomCount++;

                // Write room ID to grid
                for (int i = 0; i < count; i++) {
                    roomGrid[z][roomCellBuffer[i].y][roomCellBuffer[i].x] = roomId;
                }

                // Scan and classify
                ScanRoomCells(roomId, z, roomCellBuffer, count);
            }
        }

        // Post-pass: assign door cells to an adjacent room
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (grid[z][y][x] != CELL_DOOR) continue;
                if (roomGrid[z][y][x] != ROOM_NONE) continue;

                // Find first adjacent room
                static const int dx[] = {1, -1, 0, 0};
                static const int dy[] = {0, 0, 1, -1};
                for (int d = 0; d < 4; d++) {
                    int nx = x + dx[d];
                    int ny = y + dy[d];
                    if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
                    uint16_t adj = roomGrid[z][ny][nx];
                    if (adj != ROOM_NONE) {
                        roomGrid[z][y][x] = adj;
                        rooms[adj].cellCount++;
                        break;
                    }
                }
            }
        }
    }
}

// --- Public API ---

void InitRooms(void) {
    memset(roomGrid, 0, sizeof(roomGrid));
    memset(rooms, 0, sizeof(rooms));
    roomCount = 0;
    roomsDirty = true;
}

void InvalidateRooms(void) {
    roomsDirty = true;
}

void UpdateRooms(void) {
    if (!roomsEnabled || !roomsDirty) return;
    RecomputeRooms();
    roomsDirty = false;
}

uint16_t GetRoomAt(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return ROOM_NONE;
    }
    return roomGrid[z][y][x];
}

DetectedRoom* GetRoom(uint16_t roomId) {
    if (roomId == ROOM_NONE || roomId > (uint16_t)roomCount) return NULL;
    DetectedRoom *r = &rooms[roomId];
    return r->active ? r : NULL;
}

const char* RoomTypeName(RoomType type) {
    static const char* names[] = {
        "Room", "Kitchen", "Workshop", "Barracks",
        "Bedroom", "Dining Room", "Storage"
    };
    _Static_assert(sizeof(names)/sizeof(names[0]) == ROOM_TYPE_COUNT,
                   "RoomTypeName out of sync with RoomType enum");
    return (type >= 0 && type < ROOM_TYPE_COUNT) ? names[type] : "?";
}
