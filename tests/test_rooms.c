#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"

static bool test_verbose = false;

#include "../src/game_state.h"
#include "../src/world/grid.h"
#include "../src/world/material.h"
#include "../src/world/cell_defs.h"
#include "../src/entities/mover.h"
#include "../src/entities/items.h"
#include "../src/entities/jobs.h"
#include "../src/entities/stockpiles.h"
#include "../src/entities/workshops.h"
#include "../src/entities/furniture.h"
#include "../src/simulation/rooms.h"
#include "../src/simulation/lighting.h"
#include "../src/simulation/mood.h"
#include "../src/simulation/balance.h"
#include "../src/world/designations.h"
#include "../src/core/time.h"
#include "../src/core/sim_manager.h"
#include "test_helpers.h"
#include <string.h>

// Build a flat ground: z=0 solid dirt, z=1 walkable air with floor
static void SetupFlatGround(int w, int h) {
    InitTestGrid(w, h);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            grid[0][y][x] = CELL_WALL;
            SetWallMaterial(x, y, 0, MAT_DIRT);
            SetWallNatural(x, y, 0);
            grid[1][y][x] = CELL_AIR;
        }
    }
    ClearMovers();
    ClearItems();
    ClearStockpiles();
    ClearWorkshops();
    ClearFurniture();
    ClearJobs();
    InitDesignations();
    InitRooms();
    InitLighting();
    InitBalance();
    roomsEnabled = true;
    lightingEnabled = true;
}

// Place constructed walls around a rectangular area at z=1
// Interior cells are left as air (walkable, standing on z=0 solid)
static void BuildWalledRoom(int x1, int y1, int x2, int y2) {
    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            bool edge = (x == x1 || x == x2 || y == y1 || y == y2);
            if (edge) {
                grid[1][y][x] = CELL_WALL;
                SetWallMaterial(x, y, 1, MAT_GRANITE);
                ClearWallNatural(x, y, 1);
            }
        }
    }
}

// Place a door at position
static void PlaceDoor(int x, int y) {
    grid[1][y][x] = CELL_DOOR;
    SetWallMaterial(x, y, 1, MAT_GRANITE);
    ClearWallNatural(x, y, 1);
}

// Place a window at position
static void PlaceWindow(int x, int y) {
    grid[1][y][x] = CELL_WINDOW;
    ClearWallNatural(x, y, 1);
}

// Place constructed floor at position (z=1, replacing the implicit floor-from-below)
static void PlaceConstructedFloor(int x, int y) {
    SET_FLOOR(x, y, 1);
    SetFloorMaterial(x, y, 1, MAT_GRANITE);
    ClearFloorNatural(x, y, 1);
}

describe(test_rooms) {

    // --- Detection basics ---

    it("open space is not a room") {
        SetupFlatGround(16, 16);
        UpdateRooms();

        // Middle of open field — no room
        expect(GetRoomAt(8, 8, 1) == ROOM_NONE);
    }

    it("four walls with a door is a room") {
        SetupFlatGround(16, 16);

        // 5x5 box with door on south wall
        BuildWalledRoom(3, 3, 7, 7);
        PlaceDoor(5, 7);

        UpdateRooms();

        // Interior cell should have a room
        uint16_t rid = GetRoomAt(5, 5, 1);
        expect(rid != ROOM_NONE);

        // Room should exist
        DetectedRoom* r = GetRoom(rid);
        expect(r != NULL);
        expect(r->cellCount > 0);
    }

    it("walls without a door still form a room") {
        SetupFlatGround(16, 16);
        BuildWalledRoom(3, 3, 7, 7);
        // No door — fully enclosed, still a room (movers can't enter but it's detected)
        UpdateRooms();

        uint16_t rid = GetRoomAt(5, 5, 1);
        expect(rid != ROOM_NONE);
    }

    it("wall cells themselves are not part of any room") {
        SetupFlatGround(16, 16);
        BuildWalledRoom(3, 3, 7, 7);
        PlaceDoor(5, 7);
        UpdateRooms();

        // A wall cell
        expect(GetRoomAt(3, 3, 1) == ROOM_NONE);
    }

    it("door cell belongs to a room") {
        SetupFlatGround(16, 16);
        BuildWalledRoom(3, 3, 7, 7);
        PlaceDoor(5, 7);
        UpdateRooms();

        // The door cell should be part of the room
        uint16_t doorRoom = GetRoomAt(5, 7, 1);
        uint16_t interiorRoom = GetRoomAt(5, 5, 1);
        expect(doorRoom != ROOM_NONE);
        expect(doorRoom == interiorRoom);
    }

    it("two rooms separated by a wall are different rooms") {
        SetupFlatGround(20, 10);

        // Room A: (1,1)-(5,5)
        BuildWalledRoom(1, 1, 5, 5);
        PlaceDoor(3, 5);

        // Room B: (7,1)-(11,5)
        BuildWalledRoom(7, 1, 11, 5);
        PlaceDoor(9, 5);

        UpdateRooms();

        uint16_t ridA = GetRoomAt(3, 3, 1);
        uint16_t ridB = GetRoomAt(9, 3, 1);

        expect(ridA != ROOM_NONE);
        expect(ridB != ROOM_NONE);
        expect(ridA != ridB);
    }

    it("door separates two adjacent rooms") {
        SetupFlatGround(16, 10);

        // Two rooms sharing a wall with door between them
        // Room A: (1,1)-(6,5), Room B: (6,1)-(11,5)
        // Shared wall at x=6
        BuildWalledRoom(1, 1, 6, 5);
        BuildWalledRoom(6, 1, 11, 5);
        PlaceDoor(1, 3); // door into room A
        PlaceDoor(11, 3); // door into room B
        PlaceDoor(6, 3); // door between rooms

        UpdateRooms();

        uint16_t ridA = GetRoomAt(3, 3, 1);
        uint16_t ridB = GetRoomAt(9, 3, 1);

        expect(ridA != ROOM_NONE);
        expect(ridB != ROOM_NONE);
        expect(ridA != ridB);
    }

    it("outside area next to a room has no room ID") {
        SetupFlatGround(16, 16);
        BuildWalledRoom(3, 3, 7, 7);
        PlaceDoor(5, 7);
        UpdateRooms();

        // Cell outside the room, in the open
        expect(GetRoomAt(1, 1, 1) == ROOM_NONE);
    }

    // --- Room types ---

    it("empty enclosed room is generic") {
        SetupFlatGround(16, 16);
        BuildWalledRoom(3, 3, 7, 7);
        PlaceDoor(5, 7);
        UpdateRooms();

        DetectedRoom* r = GetRoom(GetRoomAt(5, 5, 1));
        expect(r != NULL);
        expect(r->type == ROOM_TYPE_GENERIC);
    }

    it("room with one bed is a bedroom") {
        SetupFlatGround(16, 16);
        BuildWalledRoom(3, 3, 7, 7);
        PlaceDoor(5, 7);
        SpawnFurniture(5, 5, 1, FURNITURE_PLANK_BED, MAT_NONE);
        UpdateRooms();

        DetectedRoom* r = GetRoom(GetRoomAt(5, 5, 1));
        expect(r != NULL);
        expect(r->type == ROOM_TYPE_BEDROOM);
    }

    it("room with two beds is a barracks") {
        SetupFlatGround(16, 16);
        BuildWalledRoom(3, 3, 8, 7);
        PlaceDoor(5, 7);
        SpawnFurniture(4, 5, 1, FURNITURE_PLANK_BED, MAT_NONE);
        SpawnFurniture(6, 5, 1, FURNITURE_PLANK_BED, MAT_NONE);
        UpdateRooms();

        DetectedRoom* r = GetRoom(GetRoomAt(5, 5, 1));
        expect(r != NULL);
        expect(r->type == ROOM_TYPE_BARRACKS);
    }

    it("room with only a chair is a dining room") {
        SetupFlatGround(16, 16);
        BuildWalledRoom(3, 3, 7, 7);
        PlaceDoor(5, 7);
        SpawnFurniture(5, 5, 1, FURNITURE_CHAIR, MAT_NONE);
        UpdateRooms();

        DetectedRoom* r = GetRoom(GetRoomAt(5, 5, 1));
        expect(r != NULL);
        expect(r->type == ROOM_TYPE_DINING);
    }

    it("room with stockpile is storage") {
        SetupFlatGround(16, 16);
        BuildWalledRoom(3, 3, 7, 7);
        PlaceDoor(5, 7);
        CreateStockpile(4, 4, 1, 2, 2);
        UpdateRooms();

        DetectedRoom* r = GetRoom(GetRoomAt(5, 5, 1));
        expect(r != NULL);
        expect(r->type == ROOM_TYPE_STORAGE);
    }

    it("cooking workshop makes it a kitchen") {
        SetupFlatGround(16, 16);
        BuildWalledRoom(2, 2, 9, 9);
        PlaceDoor(5, 9);
        CreateWorkshop(4, 4, 1, WORKSHOP_CAMPFIRE);
        UpdateRooms();

        DetectedRoom* r = GetRoom(GetRoomAt(5, 5, 1));
        expect(r != NULL);
        expect(r->type == ROOM_TYPE_KITCHEN);
    }

    it("crafting workshop makes it a workshop room") {
        SetupFlatGround(20, 20);
        BuildWalledRoom(2, 2, 12, 12);
        PlaceDoor(7, 12);
        CreateWorkshop(5, 5, 1, WORKSHOP_CARPENTER);
        UpdateRooms();

        DetectedRoom* r = GetRoom(GetRoomAt(7, 7, 1));
        expect(r != NULL);
        expect(r->type == ROOM_TYPE_WORKSHOP);
    }

    // --- Quality scoring ---

    it("room quality increases with constructed floor") {
        SetupFlatGround(16, 16);
        BuildWalledRoom(3, 3, 7, 7);
        PlaceDoor(5, 7);
        UpdateRooms();

        DetectedRoom* r1 = GetRoom(GetRoomAt(5, 5, 1));
        float q_before = r1->quality;

        // Add constructed floors to interior
        for (int y = 4; y <= 6; y++)
            for (int x = 4; x <= 6; x++)
                PlaceConstructedFloor(x, y);
        InvalidateRooms();
        UpdateRooms();

        DetectedRoom* r2 = GetRoom(GetRoomAt(5, 5, 1));
        expect(r2->quality > q_before);
    }

    it("room quality increases with furniture") {
        SetupFlatGround(16, 16);
        BuildWalledRoom(3, 3, 7, 7);
        PlaceDoor(5, 7);
        UpdateRooms();

        DetectedRoom* r1 = GetRoom(GetRoomAt(5, 5, 1));
        float q_before = r1->quality;

        SpawnFurniture(5, 5, 1, FURNITURE_PLANK_BED, MAT_NONE);
        SpawnFurniture(4, 5, 1, FURNITURE_CHAIR, MAT_NONE);
        // InvalidateRooms called by SpawnFurniture
        UpdateRooms();

        DetectedRoom* r2 = GetRoom(GetRoomAt(5, 5, 1));
        expect(r2->quality > q_before);
    }

    it("window improves room quality") {
        SetupFlatGround(16, 16);
        BuildWalledRoom(3, 3, 7, 7);
        PlaceDoor(5, 7);
        UpdateRooms();

        DetectedRoom* r1 = GetRoom(GetRoomAt(5, 5, 1));
        float q_before = r1->quality;

        // Replace a wall with a window
        PlaceWindow(5, 3);
        InvalidateRooms();
        UpdateRooms();

        DetectedRoom* r2 = GetRoom(GetRoomAt(5, 5, 1));
        expect(r2->quality > q_before);
    }

    it("tiny room has lower quality than bigger room") {
        SetupFlatGround(20, 20);

        // Tiny 3x3 room (1 interior cell)
        BuildWalledRoom(1, 1, 3, 3);
        PlaceDoor(2, 3);

        // Bigger 7x7 room (25 interior cells)
        BuildWalledRoom(6, 1, 12, 7);
        PlaceDoor(9, 7);

        UpdateRooms();

        DetectedRoom* tiny = GetRoom(GetRoomAt(2, 2, 1));
        DetectedRoom* big = GetRoom(GetRoomAt(9, 4, 1));

        expect(tiny != NULL);
        expect(big != NULL);
        expect(big->quality > tiny->quality);
    }

    // --- Invalidation ---

    it("room updates after wall is placed") {
        SetupFlatGround(16, 16);
        BuildWalledRoom(3, 3, 7, 7);
        PlaceDoor(5, 7);
        UpdateRooms();

        uint16_t rid = GetRoomAt(5, 5, 1);
        expect(rid != ROOM_NONE);

        DetectedRoom* r = GetRoom(rid);
        int originalSize = r->cellCount;

        // Place wall inside room, splitting it or shrinking it
        grid[1][5][5] = CELL_WALL;
        SetWallMaterial(5, 5, 1, MAT_GRANITE);
        InvalidateRooms();
        UpdateRooms();

        // Room should still exist but interior cell count changed
        DetectedRoom* r2 = GetRoom(GetRoomAt(4, 4, 1));
        expect(r2 != NULL);
        expect(r2->cellCount < originalSize);
    }

    it("removing a wall can merge spaces") {
        SetupFlatGround(16, 16);

        // Two adjacent rooms sharing wall at x=6
        BuildWalledRoom(2, 2, 6, 6);
        BuildWalledRoom(6, 2, 10, 6);
        PlaceDoor(4, 6);
        PlaceDoor(8, 6);
        UpdateRooms();

        uint16_t ridA = GetRoomAt(4, 4, 1);
        uint16_t ridB = GetRoomAt(8, 4, 1);
        expect(ridA != ridB);

        // Remove shared wall at (6,4) - punch a hole
        grid[1][4][6] = CELL_AIR;
        InvalidateRooms();
        UpdateRooms();

        // Now they should be the same room
        uint16_t ridA2 = GetRoomAt(4, 4, 1);
        uint16_t ridB2 = GetRoomAt(8, 4, 1);
        expect(ridA2 == ridB2);
    }

    // --- Edge cases ---

    it("GetRoom returns NULL for ROOM_NONE") {
        expect(GetRoom(ROOM_NONE) == NULL);
    }

    it("GetRoomAt returns ROOM_NONE for out of bounds") {
        SetupFlatGround(16, 16);
        expect(GetRoomAt(-1, 0, 0) == ROOM_NONE);
        expect(GetRoomAt(0, -1, 0) == ROOM_NONE);
        expect(GetRoomAt(999, 0, 0) == ROOM_NONE);
    }

    it("RoomTypeName returns valid strings") {
        expect(strcmp(RoomTypeName(ROOM_TYPE_BEDROOM), "Bedroom") == 0);
        expect(strcmp(RoomTypeName(ROOM_TYPE_KITCHEN), "Kitchen") == 0);
        expect(strcmp(RoomTypeName(ROOM_TYPE_GENERIC), "Room") == 0);
    }

    it("room on z=2 is detected independently from z=1") {
        SetupFlatGround(16, 16);

        // z=1 room
        BuildWalledRoom(3, 3, 7, 7);
        PlaceDoor(5, 7);

        // z=2: build walls and add floors so interior is walkable
        for (int y = 3; y <= 7; y++) {
            for (int x = 3; x <= 7; x++) {
                bool edge = (x == 3 || x == 7 || y == 3 || y == 7);
                if (edge) {
                    grid[2][y][x] = CELL_WALL;
                    SetWallMaterial(x, y, 2, MAT_GRANITE);
                } else {
                    grid[2][y][x] = CELL_AIR;
                    // z=1 interior is air, so z=2 needs constructed floor
                    SET_FLOOR(x, y, 2);
                }
            }
        }
        grid[2][7][5] = CELL_DOOR;

        UpdateRooms();

        uint16_t ridZ1 = GetRoomAt(5, 5, 1);
        uint16_t ridZ2 = GetRoomAt(5, 5, 2);

        expect(ridZ1 != ROOM_NONE);
        expect(ridZ2 != ROOM_NONE);
        expect(ridZ1 != ridZ2);
    }
}

int main(int argc, char *argv[]) {
    test_verbose = c89spec_parse_args(argc, argv);
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);

    test(test_rooms);
    return summary();
}
