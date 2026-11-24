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
#include "weather.h"   // IsExposedToSky

uint16_t* roomGrid = NULL;   // allocated in grid_layers.c
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
           type == WORKSHOP_HEARTH ||
           type == WORKSHOP_STOVE ||      // the dollhouse / city kitchen
           type == WORKSHOP_COUNTER;
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
    return (GetFurnitureDef(type)->capabilities & CAP_SLEEPING) != 0;
}

static float ComputeLightLevel(int x, int y, int z) {
    // Combine sky light and block light into a 0-15 scale
    float sky = (float)LIGHT(x, y, z).skyLevel;
    float blockMax = LIGHT(x, y, z).blockR;
    if (LIGHT(x, y, z).blockG > blockMax) blockMax = LIGHT(x, y, z).blockG;
    if (LIGHT(x, y, z).blockB > blockMax) blockMax = LIGHT(x, y, z).blockB;
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

            CellType cell = GRID(nx, ny, z);

            // Railings split walking, not space: a gallery and the hall it looks over, a pen
            // inside a barn, are one room. The railing cell joins the room.
            if (cell == CELL_RAILING) {
                visited[ny][nx] = true;
                roomBfsQueue[qTail++] = (RoomBfsNode){nx, ny};
                continue;
            }

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
    // Open air first: a walled garden with a bench is a garden, not a living room
    bool empty = room->furnCount == 0 && room->cookingWorkshopCount == 0 &&
                 room->craftingWorkshopCount == 0 && room->stockpileCellCount == 0;
    if (room->outdoors) {
        room->type = ROOM_TYPE_OUTDOORS;
    } else if (room->cookingWorkshopCount > 0 || room->coldStorageCount > 0) {
        room->type = ROOM_TYPE_KITCHEN;
    } else if (room->craftingWorkshopCount > 0) {
        room->type = ROOM_TYPE_WORKSHOP;
    } else if (room->bedCount >= 2) {
        room->type = ROOM_TYPE_BARRACKS;
    } else if (room->bedCount == 1) {
        room->type = ROOM_TYPE_BEDROOM;
    } else if (room->toiletCount > 0 && (room->washingCount == 0 || room->cellCount <= 3)) {
        room->type = ROOM_TYPE_WC;             // a toilet, at most a hand basin squeezed in
    } else if (room->washingCount > 0) {
        room->type = ROOM_TYPE_BATHROOM;
    } else if (room->sofaCount > 0) {
        room->type = ROOM_TYPE_LIVING;         // a sofa makes a living room, table or not
    } else if (room->diningSetCount > 0) {
        room->type = ROOM_TYPE_DINING;
    } else if (room->seatCount > 0) {
        room->type = ROOM_TYPE_LIVING;         // seats without a table
    } else if (room->stockpileCellCount > 0) {
        room->type = ROOM_TYPE_STORAGE;
    } else if (empty && room->exteriorDoorCount > 0 && room->cellCount >= 12) {
        room->type = ROOM_TYPE_LOBBY;
    } else if (empty && room->verticalCount > 0) {
        room->type = ROOM_TYPE_STAIRS;
    } else if (empty && room->doorCount >= 2) {
        room->type = ROOM_TYPE_HALL;
    } else {
        room->type = ROOM_TYPE_GENERIC;
    }
}

// A dining set: a table with a chair on one of its 4 side cells, in the same room
static bool TableHasChairInRoom(int x, int y, int z, uint16_t roomId) {
    static const int dx[] = {1, -1, 0, 0};
    static const int dy[] = {0, 0, 1, -1};
    for (int d = 0; d < 4; d++) {
        int nx = x + dx[d], ny = y + dy[d];
        if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
        if (ROOM(nx, ny, z) != roomId) continue;
        int fi = GetFurnitureAt(nx, ny, z);
        if (fi >= 0 && furniture[fi].type == FURNITURE_CHAIR) return true;
    }
    return false;
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
        totalDirt += (float)FLOOR_DIRT(cx, cy, z);

        // Constructed floor check
        if (HAS_FLOOR(cx, cy, z) && !IsFloorNatural(cx, cy, z)) {
            constructedFloors++;
        } else if (z > 0 && CellIsSolid(GRID(cx, cy, z - 1)) && !IsWallNatural(cx, cy, z - 1)) {
            // Standing on top of a constructed wall below = constructed floor
            constructedFloors++;
        }

        if (!IsExposedToSky(cx, cy, z)) room->roofedCellCount++;
        CellType here = GRID(cx, cy, z);
        if (CellIsLadder(here) || CellIsRamp(here) || here == CELL_ELEVATOR) room->verticalCount++;

        // Furniture at this cell
        int furnIdx = GetFurnitureAt(cx, cy, z);
        if (furnIdx >= 0) {
            FurnitureType ft = furniture[furnIdx].type;
            room->furnCount++;
            if (!furnTypesSeen[ft]) {
                furnTypesSeen[ft] = true;
                room->furnVariety++;
            }
            CapabilityBits caps = GetFurnitureDef(ft)->capabilities;
            if (IsBedFurniture(ft)) room->bedCount++;
            if (caps & CAP_SEATING) room->seatCount++;
            if (caps & CAP_TOILET) room->toiletCount++;
            if (caps & CAP_WASHING) room->washingCount++;
            if (caps & CAP_COLD_STORAGE) room->coldStorageCount++;
            if (ft == FURNITURE_SOFA) room->sofaCount++;
            if (ft == FURNITURE_CHAIR) room->chairCount++;
            if (ft == FURNITURE_TABLE) {
                room->tableCount++;
                if (TableHasChairInRoom(cx, cy, z, roomId)) room->diningSetCount++;
            }
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

            CellType ncell = GRID(nx, ny, z);
            if (ncell == CELL_WINDOW) {
                windowPositions++;
            }
            if (CellBlocksMovement(ncell) && ncell != CELL_DOOR && ncell != CELL_RAILING) {
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
                    if (ROOM(tx, ty, z) == roomId) {
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

    room->outdoors = room->roofedCellCount * 2 < cellCount;
    room->quality = ComputeQuality(room);
    // Classified once the whole level is done: its doors need the rooms on their other side
}

// --- Main recompute ---

static void RecomputeRooms(void) {
    CLEAR_GRID_LAYER(roomGrid, 0);
    memset(rooms, 0, sizeof(rooms));
    roomCount = 0;

    for (int z = 0; z < gridDepth; z++) {
        CLEAR_GRID_PLANE(visited, 0);
        int firstRoomOnZ = roomCount + 1;

        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (visited[y][x]) continue;

                CellType cell = GRID(x, y, z);

                // Railings are no seed but stay unvisited: a room's flood takes them in (a
                // railing row before the room in row-major order must not be marked here)
                if (cell == CELL_RAILING) continue;

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
                    ROOM(roomCellBuffer[i].x, roomCellBuffer[i].y, z) = roomId;
                }

                // Scan and classify
                ScanRoomCells(roomId, z, roomCellBuffer, count);
            }
        }

        // Post-pass: doors per room (and which lead outside), then assign door cells to an
        // adjacent room
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (GRID(x, y, z) != CELL_DOOR) continue;
                if (ROOM(x, y, z) != ROOM_NONE) continue;
                uint16_t sides[4];
                int nSides = 0;
                bool outside = false;
                static const int ddx[] = {1, -1, 0, 0};
                static const int ddy[] = {0, 0, 1, -1};
                for (int d = 0; d < 4; d++) {
                    int nx = x + ddx[d], ny = y + ddy[d];
                    if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) { outside = true; continue; }
                    uint16_t adj = ROOM(nx, ny, z);
                    if (adj == ROOM_NONE) {
                        if (IsCellWalkableAt(z, ny, nx)) outside = true;
                        continue;
                    }
                    if (rooms[adj].outdoors) outside = true;
                    bool seen = false;
                    for (int k = 0; k < nSides; k++) if (sides[k] == adj) seen = true;
                    if (!seen) sides[nSides++] = adj;
                }
                for (int k = 0; k < nSides; k++) {
                    rooms[sides[k]].doorCount++;
                    if (outside) rooms[sides[k]].exteriorDoorCount++;
                }
            }
        }
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                if (GRID(x, y, z) != CELL_DOOR) continue;
                if (ROOM(x, y, z) != ROOM_NONE) continue;

                // Find first adjacent room
                static const int dx[] = {1, -1, 0, 0};
                static const int dy[] = {0, 0, 1, -1};
                for (int d = 0; d < 4; d++) {
                    int nx = x + dx[d];
                    int ny = y + dy[d];
                    if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
                    uint16_t adj = ROOM(nx, ny, z);
                    if (adj != ROOM_NONE) {
                        ROOM(x, y, z) = adj;
                        rooms[adj].cellCount++;
                        break;
                    }
                }
            }
        }

        for (int r = firstRoomOnZ; r <= roomCount; r++) ClassifyRoom(&rooms[r]);
    }
}

// --- Public API ---

void InitRooms(void) {
    CLEAR_GRID_LAYER(roomGrid, 0);
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
    return ROOM(x, y, z);
}

DetectedRoom* GetRoom(uint16_t roomId) {
    if (roomId == ROOM_NONE || roomId > (uint16_t)roomCount) return NULL;
    DetectedRoom *r = &rooms[roomId];
    return r->active ? r : NULL;
}

const char* RoomTypeName(RoomType type) {
    static const char* names[] = {
        "Room", "Kitchen", "Workshop", "Barracks",
        "Bedroom", "Dining Room", "Storage",
        "Living Room", "Hall", "WC", "Bathroom", "Stairs", "Lobby", "Outdoors"
    };
    _Static_assert(sizeof(names)/sizeof(names[0]) == ROOM_TYPE_COUNT,
                   "RoomTypeName out of sync with RoomType enum");
    return (type >= 0 && type < ROOM_TYPE_COUNT) ? names[type] : "?";
}
