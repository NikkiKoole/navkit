// rooms.h - Room detection, typing, and quality scoring
//
// Flood-fill per z-level detects enclosed rooms bounded by walls/doors.
// Each room is classified by furniture contents and scored for quality.
// Quality feeds into mood (MOODLET_NICE_ROOM / MOODLET_UGLY_ROOM).
//
// Usage:
//   InvalidateRooms();              // call when walls/furniture/workshops change
//   UpdateRooms();                  // in TickWithDt, recomputes if dirty
//   uint16_t rid = GetRoomAt(x,y,z);
//   DetectedRoom* r = GetRoom(rid);  // NULL if no room

#ifndef ROOMS_H
#define ROOMS_H

#include <stdbool.h>
#include <stdint.h>
#include "../world/grid.h"   // MAX_GRID_* for roomGrid

#define MAX_DETECTED_ROOMS 1024
#define ROOM_NONE 0   // 0 = no room (outdoors, wall, unenclosed)

typedef enum {
    ROOM_TYPE_GENERIC = 0, // enclosed but no qualifying furniture
    ROOM_TYPE_KITCHEN,     // has cooking workshop
    ROOM_TYPE_WORKSHOP,    // has crafting workshop
    ROOM_TYPE_BARRACKS,    // 2+ beds
    ROOM_TYPE_BEDROOM,     // 1 bed
    ROOM_TYPE_DINING,      // has a dining set (table with a chair next to it)
    ROOM_TYPE_STORAGE,     // has stockpile
    // append only (dreamengine's room names map 1:1); see ClassifyRoom for the rules
    ROOM_TYPE_LIVING,      // a sofa, or seats without a table
    ROOM_TYPE_HALL,        // empty, connects 2+ doors
    ROOM_TYPE_WC,          // a toilet, no bath or shower (a hand basin is fine)
    ROOM_TYPE_BATHROOM,    // a bath or shower (or only sinks), maybe a toilet
    ROOM_TYPE_STAIRS,      // empty but for ladders/ramps/a shaft
    ROOM_TYPE_LOBBY,       // a big (24+ cells) empty hall with a door to the outside
    ROOM_TYPE_OUTDOORS,    // enclosed but mostly open to the sky: garden, yard, balcony, terrace
    ROOM_TYPE_OFFICE,      // a desk with a seat (no bed, no sofa)
    ROOM_TYPE_COUNT
} RoomType;

typedef struct {
    bool active;
    int z;
    int cellCount;
    RoomType type;
    float quality;          // -1.0 to +1.0

    // Furniture/content counts (for type classification)
    int bedCount;
    int chairCount;
    int tableCount;
    int diningSetCount;          // tables with a chair on a side cell in the same room
    int cookingWorkshopCount;
    int craftingWorkshopCount;
    int stockpileCellCount;
    int sofaCount;
    int seatCount;               // furniture with CAP_SEATING (chairs, sofas)
    int toiletCount;             // CAP_TOILET
    int washingCount;            // CAP_WASHING (sinks, baths, showers)
    int bathingCount;            // baths and showers
    int doubleBedCount;
    int deskCount;
    int coldStorageCount;        // CAP_COLD_STORAGE (fridges)
    int verticalCount;           // ladder, ramp and shaft cells
    int doorCount;               // doors on its edge
    int exteriorDoorCount;       // ... of which lead outside (no room, or an outdoors one)
    int roofedCellCount;         // cells with something above (not IsExposedToSky)
    bool outdoors;               // less than half its cells roofed

    // Quality inputs (computed during scan)
    float avgLight;              // 0-15 scale
    int windowCount;
    int furnCount;               // total furniture pieces
    int furnVariety;             // distinct furniture types present
    float constructedFloorFrac;  // 0.0-1.0
    float constructedWallFrac;   // 0.0-1.0
    float avgDirt;               // 0-255
} DetectedRoom;

// Room grid: per-cell room ID (0 = no room)
extern uint16_t* roomGrid;
#define ROOM(x, y, z) (roomGrid CELL_AT((x), (y), (z)))

// Room metadata
extern DetectedRoom rooms[MAX_DETECTED_ROOMS];
extern int roomCount;

// Dirty flag
extern bool roomsDirty;

// Master toggle
extern bool roomsEnabled;

// API
void InitRooms(void);
void InvalidateRooms(void);
void UpdateRooms(void);
uint16_t GetRoomAt(int x, int y, int z);
DetectedRoom* GetRoom(uint16_t roomId);
const char* RoomTypeName(RoomType type);

#endif // ROOMS_H
