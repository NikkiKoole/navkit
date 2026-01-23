#include "items.h"
#include <math.h>

Item items[MAX_ITEMS];
int itemCount = 0;

void ClearItems(void) {
    for (int i = 0; i < MAX_ITEMS; i++) {
        items[i].active = false;
        items[i].reservedBy = -1;
        items[i].unreachableCooldown = 0.0f;
    }
    itemCount = 0;
}

int SpawnItem(float x, float y, float z, ItemType type) {
    // Find first inactive slot
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (!items[i].active) {
            items[i].x = x;
            items[i].y = y;
            items[i].z = z;
            items[i].type = type;
            items[i].state = ITEM_ON_GROUND;
            items[i].active = true;
            items[i].reservedBy = -1;
            items[i].unreachableCooldown = 0.0f;
            itemCount++;
            return i;
        }
    }
    return -1;  // no space
}

void DeleteItem(int index) {
    if (index >= 0 && index < MAX_ITEMS && items[index].active) {
        items[index].active = false;
        items[index].reservedBy = -1;
        itemCount--;
    }
}

bool ReserveItem(int itemIndex, int moverIndex) {
    if (itemIndex < 0 || itemIndex >= MAX_ITEMS) return false;
    if (!items[itemIndex].active) return false;
    if (items[itemIndex].reservedBy != -1) return false;  // already reserved
    
    items[itemIndex].reservedBy = moverIndex;
    return true;
}

void ReleaseItemReservation(int itemIndex) {
    if (itemIndex >= 0 && itemIndex < MAX_ITEMS) {
        items[itemIndex].reservedBy = -1;
    }
}

int FindNearestUnreservedItem(float x, float y, float z) {
    int nearest = -1;
    float nearestDistSq = 1e30f;
    
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (!items[i].active) continue;
        if (items[i].reservedBy != -1) continue;  // skip reserved
        
        float dx = items[i].x - x;
        float dy = items[i].y - y;
        float dz = items[i].z - z;
        float distSq = dx*dx + dy*dy + dz*dz;
        
        if (distSq < nearestDistSq) {
            nearestDistSq = distSq;
            nearest = i;
        }
    }
    
    return nearest;
}

void ItemsTick(float dt) {
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (!items[i].active) continue;
        if (items[i].unreachableCooldown > 0.0f) {
            items[i].unreachableCooldown -= dt;
            if (items[i].unreachableCooldown < 0.0f) {
                items[i].unreachableCooldown = 0.0f;
            }
        }
    }
}

void SetItemUnreachableCooldown(int itemIndex, float cooldown) {
    if (itemIndex >= 0 && itemIndex < MAX_ITEMS && items[itemIndex].active) {
        items[itemIndex].unreachableCooldown = cooldown;
    }
}
