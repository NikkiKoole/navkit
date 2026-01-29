#include "workshops.h"
#include "stockpiles.h"
#include "mover.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include <string.h>

Workshop workshops[MAX_WORKSHOPS];
int workshopCount = 0;

// Stonecutter recipes: 1 ORANGE -> 2 STONE_BLOCKS
Recipe stonecutterRecipes[] = {
    { "Cut Stone Blocks", ITEM_ORANGE, 1, ITEM_STONE_BLOCKS, 2, 3.0f },
};
int stonecutterRecipeCount = sizeof(stonecutterRecipes) / sizeof(stonecutterRecipes[0]);

// Workshop templates (ASCII art, row-major, top-to-bottom)
// . = floor (walkable), # = machinery (blocked), X = work tile, O = output
static const char* workshopTemplates[] = {
    [WORKSHOP_STONECUTTER] = 
        "##O"
        "#X."
        "...",
};

void ClearWorkshops(void) {
    for (int i = 0; i < MAX_WORKSHOPS; i++) {
        workshops[i].active = false;
        workshops[i].assignedCrafter = -1;
        workshops[i].billCount = 0;
        workshops[i].linkedInputCount = 0;
    }
    workshopCount = 0;
}

int CreateWorkshop(int x, int y, int z, WorkshopType type) {
    for (int i = 0; i < MAX_WORKSHOPS; i++) {
        if (!workshops[i].active) {
            Workshop* ws = &workshops[i];
            ws->x = x;
            ws->y = y;
            ws->z = z;
            ws->type = type;
            ws->active = true;
            ws->assignedCrafter = -1;
            ws->billCount = 0;
            ws->linkedInputCount = 0;
            
            // Default 3x3 footprint
            ws->width = 3;
            ws->height = 3;
            
            // Copy template and find special tiles
            const char* tmpl = workshopTemplates[type];
            ws->workTileX = x;
            ws->workTileY = y;
            ws->outputTileX = x;
            ws->outputTileY = y;
            
            for (int ty = 0; ty < ws->height; ty++) {
                for (int tx = 0; tx < ws->width; tx++) {
                    int idx = ty * ws->width + tx;
                    char c = tmpl[idx];
                    ws->template[idx] = c;
                    
                    if (c == WT_WORK) {
                        ws->workTileX = x + tx;
                        ws->workTileY = y + ty;
                    } else if (c == WT_OUTPUT) {
                        ws->outputTileX = x + tx;
                        ws->outputTileY = y + ty;
                    }
                    
                    // Set blocking flag for machinery tiles and push movers out
                    if (c == WT_BLOCK) {
                        int tileX = x + tx;
                        int tileY = y + ty;
                        SET_CELL_FLAG(tileX, tileY, z, CELL_FLAG_WORKSHOP_BLOCK);
                        
                        // Push any movers out of this tile
                        for (int m = 0; m < moverCount; m++) {
                            Mover* mover = &movers[m];
                            if (!mover->active) continue;
                            int mx = (int)(mover->x / CELL_SIZE);
                            int my = (int)(mover->y / CELL_SIZE);
                            int mz = (int)mover->z;
                            if (mx == tileX && my == tileY && mz == z) {
                                // Find nearest walkable tile
                                int dirs[] = {0, -1, 0, 1, -1, 0, 1, 0};
                                for (int d = 0; d < 4; d++) {
                                    int nx = tileX + dirs[d*2];
                                    int ny = tileY + dirs[d*2+1];
                                    if (IsCellWalkableAt(z, ny, nx)) {
                                        mover->x = nx * CELL_SIZE + CELL_SIZE * 0.5f;
                                        mover->y = ny * CELL_SIZE + CELL_SIZE * 0.5f;
                                        mover->needsRepath = true;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            workshopCount++;
            return i;
        }
    }
    return -1;
}

void DeleteWorkshop(int index) {
    if (index >= 0 && index < MAX_WORKSHOPS && workshops[index].active) {
        Workshop* ws = &workshops[index];
        
        // Clear blocking flags for machinery tiles
        for (int ty = 0; ty < ws->height; ty++) {
            for (int tx = 0; tx < ws->width; tx++) {
                if (ws->template[ty * ws->width + tx] == WT_BLOCK) {
                    CLEAR_CELL_FLAG(ws->x + tx, ws->y + ty, ws->z, CELL_FLAG_WORKSHOP_BLOCK);
                }
            }
        }
        
        ws->active = false;
        workshopCount--;
    }
}

Recipe* GetRecipesForWorkshop(WorkshopType type, int* outCount) {
    switch (type) {
        case WORKSHOP_STONECUTTER:
            *outCount = stonecutterRecipeCount;
            return stonecutterRecipes;
        default:
            *outCount = 0;
            return NULL;
    }
}

int AddBill(int workshopIdx, int recipeIdx, BillMode mode, int targetCount) {
    if (workshopIdx < 0 || workshopIdx >= MAX_WORKSHOPS) return -1;
    Workshop* ws = &workshops[workshopIdx];
    if (!ws->active) return -1;
    if (ws->billCount >= MAX_BILLS_PER_WORKSHOP) return -1;
    
    int idx = ws->billCount;
    Bill* bill = &ws->bills[idx];
    bill->recipeIdx = recipeIdx;
    bill->mode = mode;
    bill->targetCount = targetCount;
    bill->completedCount = 0;
    bill->ingredientSearchRadius = 0;  // unlimited by default
    bill->suspended = false;
    
    ws->billCount++;
    return idx;
}

void RemoveBill(int workshopIdx, int billIdx) {
    if (workshopIdx < 0 || workshopIdx >= MAX_WORKSHOPS) return;
    Workshop* ws = &workshops[workshopIdx];
    if (!ws->active) return;
    if (billIdx < 0 || billIdx >= ws->billCount) return;
    
    // Shift remaining bills down
    for (int i = billIdx; i < ws->billCount - 1; i++) {
        ws->bills[i] = ws->bills[i + 1];
    }
    ws->billCount--;
}

void SuspendBill(int workshopIdx, int billIdx, bool suspended) {
    if (workshopIdx < 0 || workshopIdx >= MAX_WORKSHOPS) return;
    Workshop* ws = &workshops[workshopIdx];
    if (!ws->active) return;
    if (billIdx < 0 || billIdx >= ws->billCount) return;
    
    ws->bills[billIdx].suspended = suspended;
}

// Count items of a type in all stockpiles
static int CountItemsInStockpiles(ItemType type) {
    int count = 0;
    for (int s = 0; s < MAX_STOCKPILES; s++) {
        Stockpile* sp = &stockpiles[s];
        if (!sp->active) continue;
        
        for (int sy = 0; sy < sp->height; sy++) {
            for (int sx = 0; sx < sp->width; sx++) {
                int slotIdx = sy * sp->width + sx;
                if (sp->slotTypes[slotIdx] == type) {
                    count += sp->slotCounts[slotIdx];
                }
            }
        }
    }
    return count;
}

bool ShouldBillRun(Workshop* ws, Bill* bill) {
    int recipeCount;
    Recipe* recipes = GetRecipesForWorkshop(ws->type, &recipeCount);
    if (bill->recipeIdx < 0 || bill->recipeIdx >= recipeCount) return false;
    Recipe* recipe = &recipes[bill->recipeIdx];
    
    switch (bill->mode) {
        case BILL_DO_X_TIMES:
            return bill->completedCount < bill->targetCount;
            
        case BILL_DO_UNTIL_X: {
            int currentCount = CountItemsInStockpiles(recipe->outputType);
            return currentCount < bill->targetCount;
        }
            
        case BILL_DO_FOREVER:
            return true;
    }
    return false;
}

int FindWorkshopAt(int tileX, int tileY, int z) {
    for (int i = 0; i < MAX_WORKSHOPS; i++) {
        Workshop* ws = &workshops[i];
        if (!ws->active) continue;
        if (ws->z != z) continue;
        
        if (tileX >= ws->x && tileX < ws->x + ws->width &&
            tileY >= ws->y && tileY < ws->y + ws->height) {
            return i;
        }
    }
    return -1;
}

bool IsWorkshopTile(int tileX, int tileY, int z) {
    return FindWorkshopAt(tileX, tileY, z) >= 0;
}

char GetWorkshopTileAt(int wsIdx, int tileX, int tileY) {
    if (wsIdx < 0 || wsIdx >= MAX_WORKSHOPS) return '.';
    Workshop* ws = &workshops[wsIdx];
    if (!ws->active) return '.';
    
    int localX = tileX - ws->x;
    int localY = tileY - ws->y;
    
    if (localX < 0 || localX >= ws->width || localY < 0 || localY >= ws->height) {
        return '.';
    }
    
    return ws->template[localY * ws->width + localX];
}

bool IsWorkshopBlocking(int tileX, int tileY, int z) {
    if (tileX < 0 || tileX >= gridWidth || tileY < 0 || tileY >= gridHeight || z < 0 || z >= gridDepth) 
        return false;
    return HAS_CELL_FLAG(tileX, tileY, z, CELL_FLAG_WORKSHOP_BLOCK);
}
