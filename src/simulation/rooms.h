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

#define MAX_DETECTED_ROOMS 1024
#define ROOM_NONE 0   // 0 = no room (outdoors, wall, unenclosed)

typedef enum {
    ROOM_TYPE_GENERIC = 0, // enclosed but no qualifying furniture
    ROOM_TYPE_KITCHEN,     // has cooking workshop
    ROOM_TYPE_WORKSHOP,    // has crafting workshop
    ROOM_TYPE_BARRACKS,    // 2+ beds
    ROOM_TYPE_BEDROOM,     // 1 bed
    ROOM_TYPE_DINING,      // has chair (future: table+chair)
    ROOM_TYPE_STORAGE,     // has stockpile
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
    int cookingWorkshopCount;
    int craftingWorkshopCount;
    int stockpileCellCount;

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
extern uint16_t roomGrid[16][512][512]; // [MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH]

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
