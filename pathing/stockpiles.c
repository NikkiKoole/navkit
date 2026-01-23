#include "stockpiles.h"
#include "mover.h"

Stockpile stockpiles[MAX_STOCKPILES];
int stockpileCount = 0;

void ClearStockpiles(void) {
    for (int i = 0; i < MAX_STOCKPILES; i++) {
        stockpiles[i].active = false;
    }
    stockpileCount = 0;
}

int CreateStockpile(int x, int y, int z, int width, int height) {
    // Find first inactive slot
    for (int i = 0; i < MAX_STOCKPILES; i++) {
        if (!stockpiles[i].active) {
            Stockpile* sp = &stockpiles[i];
            sp->x = x;
            sp->y = y;
            sp->z = z;
            sp->width = (width > MAX_STOCKPILE_SIZE) ? MAX_STOCKPILE_SIZE : width;
            sp->height = (height > MAX_STOCKPILE_SIZE) ? MAX_STOCKPILE_SIZE : height;
            sp->active = true;
            
            // Default: allow all types
            for (int t = 0; t < 3; t++) {
                sp->allowedTypes[t] = true;
            }
            
            // Initialize slots as empty
            int totalSlots = sp->width * sp->height;
            for (int s = 0; s < totalSlots; s++) {
                sp->slots[s] = -1;      // no item
                sp->reservedBy[s] = -1; // not reserved
            }
            
            stockpileCount++;
            return i;
        }
    }
    return -1;  // no space
}

void DeleteStockpile(int index) {
    if (index >= 0 && index < MAX_STOCKPILES && stockpiles[index].active) {
        stockpiles[index].active = false;
        stockpileCount--;
    }
}

void SetStockpileFilter(int stockpileIdx, ItemType type, bool allowed) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return;
    if (!stockpiles[stockpileIdx].active) return;
    if (type < 0 || type > 2) return;
    
    stockpiles[stockpileIdx].allowedTypes[type] = allowed;
}

bool StockpileAcceptsType(int stockpileIdx, ItemType type) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return false;
    if (!stockpiles[stockpileIdx].active) return false;
    if (type < 0 || type > 2) return false;
    
    return stockpiles[stockpileIdx].allowedTypes[type];
}

// Helper: get slot index from local coordinates
static int SlotIndex(Stockpile* sp, int localX, int localY) {
    return localY * sp->width + localX;
}

bool FindFreeStockpileSlot(int stockpileIdx, ItemType type, int* outX, int* outY) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return false;
    Stockpile* sp = &stockpiles[stockpileIdx];
    if (!sp->active) return false;
    if (!sp->allowedTypes[type]) return false;
    
    // Find first slot that is empty AND not reserved
    for (int ly = 0; ly < sp->height; ly++) {
        for (int lx = 0; lx < sp->width; lx++) {
            int idx = SlotIndex(sp, lx, ly);
            if (sp->slots[idx] == -1 && sp->reservedBy[idx] == -1) {
                *outX = sp->x + lx;
                *outY = sp->y + ly;
                return true;
            }
        }
    }
    return false;  // stockpile full or all reserved
}

bool ReserveStockpileSlot(int stockpileIdx, int slotX, int slotY, int moverIdx) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return false;
    Stockpile* sp = &stockpiles[stockpileIdx];
    if (!sp->active) return false;
    
    int lx = slotX - sp->x;
    int ly = slotY - sp->y;
    if (lx < 0 || lx >= sp->width || ly < 0 || ly >= sp->height) return false;
    
    int idx = SlotIndex(sp, lx, ly);
    
    // Can only reserve if empty and not already reserved
    if (sp->slots[idx] != -1) return false;       // has item
    if (sp->reservedBy[idx] != -1) return false;  // already reserved
    
    sp->reservedBy[idx] = moverIdx;
    return true;
}

void ReleaseStockpileSlot(int stockpileIdx, int slotX, int slotY) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return;
    Stockpile* sp = &stockpiles[stockpileIdx];
    if (!sp->active) return;
    
    int lx = slotX - sp->x;
    int ly = slotY - sp->y;
    if (lx < 0 || lx >= sp->width || ly < 0 || ly >= sp->height) return;
    
    int idx = SlotIndex(sp, lx, ly);
    sp->reservedBy[idx] = -1;
}

void ReleaseAllSlotsForMover(int moverIdx) {
    for (int i = 0; i < MAX_STOCKPILES; i++) {
        if (!stockpiles[i].active) continue;
        Stockpile* sp = &stockpiles[i];
        int totalSlots = sp->width * sp->height;
        for (int s = 0; s < totalSlots; s++) {
            if (sp->reservedBy[s] == moverIdx) {
                sp->reservedBy[s] = -1;
            }
        }
    }
}

int FindStockpileForItem(ItemType type, int* outSlotX, int* outSlotY) {
    // Find any stockpile that accepts this type and has a free slot
    for (int i = 0; i < MAX_STOCKPILES; i++) {
        if (!stockpiles[i].active) continue;
        if (!stockpiles[i].allowedTypes[type]) continue;
        
        int slotX, slotY;
        if (FindFreeStockpileSlot(i, type, &slotX, &slotY)) {
            *outSlotX = slotX;
            *outSlotY = slotY;
            return i;
        }
    }
    return -1;  // no stockpile available
}

bool IsPositionInStockpile(float x, float y, int z, int* outStockpileIdx) {
    int gx = (int)(x / CELL_SIZE);
    int gy = (int)(y / CELL_SIZE);
    
    for (int i = 0; i < MAX_STOCKPILES; i++) {
        if (!stockpiles[i].active) continue;
        Stockpile* sp = &stockpiles[i];
        if (sp->z != z) continue;
        
        if (gx >= sp->x && gx < sp->x + sp->width &&
            gy >= sp->y && gy < sp->y + sp->height) {
            if (outStockpileIdx) *outStockpileIdx = i;
            return true;
        }
    }
    return false;
}

// Place an item in a stockpile slot (called when mover drops item)
void PlaceItemInStockpile(int stockpileIdx, int slotX, int slotY, int itemIdx) {
    if (stockpileIdx < 0 || stockpileIdx >= MAX_STOCKPILES) return;
    Stockpile* sp = &stockpiles[stockpileIdx];
    if (!sp->active) return;
    
    int lx = slotX - sp->x;
    int ly = slotY - sp->y;
    if (lx < 0 || lx >= sp->width || ly < 0 || ly >= sp->height) return;
    
    int idx = SlotIndex(sp, lx, ly);
    sp->slots[idx] = itemIdx;
    sp->reservedBy[idx] = -1;  // no longer reserved, now occupied
}
