#ifndef ITEMS_H
#define ITEMS_H

#include <stdbool.h>

// Item types (for Phase 0, just colors)
typedef enum {
    ITEM_RED,
    ITEM_GREEN,
    ITEM_BLUE
} ItemType;

// Item state
typedef enum {
    ITEM_ON_GROUND,
    ITEM_CARRIED,
    ITEM_IN_STOCKPILE
} ItemState;

// Item struct
typedef struct {
    float x, y, z;
    ItemType type;
    ItemState state;
    bool active;
    int reservedBy;           // mover index, -1 = none
    float unreachableCooldown; // seconds until retry (0 = can try now)
} Item;

#define MAX_ITEMS 1024

extern Item items[MAX_ITEMS];
extern int itemCount;

// Core functions
void ClearItems(void);
int SpawnItem(float x, float y, float z, ItemType type);
void DeleteItem(int index);

// Reservation
bool ReserveItem(int itemIndex, int moverIndex);
void ReleaseItemReservation(int itemIndex);

// Queries
int FindNearestUnreservedItem(float x, float y, float z);

// Cooldown management
void ItemsTick(float dt);
void SetItemUnreachableCooldown(int itemIndex, float cooldown);

// Ground item queries for stockpile blocking
// Returns the index of a ground item at the given tile, or -1 if none
int FindGroundItemAtTile(int tileX, int tileY, int z);

#endif
