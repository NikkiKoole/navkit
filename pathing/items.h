#ifndef ITEMS_H
#define ITEMS_H

#include <stdbool.h>

// Item types (for Phase 0, just colors)
typedef enum {
    ITEM_RED,
    ITEM_GREEN,
    ITEM_BLUE
} ItemType;

// Item struct - minimal for Phase 0
typedef struct {
    float x, y, z;
    ItemType type;
    bool active;
    int reservedBy;  // mover index, -1 = none
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

#endif
