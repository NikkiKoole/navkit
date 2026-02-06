#include "workshops.h"
#include "stockpiles.h"
#include "mover.h"
#include "jobs.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include <string.h>

Workshop workshops[MAX_WORKSHOPS];
int workshopCount = 0;

// Stonecutter recipes: 1 raw stone -> 2 blocks (material-preserving)
Recipe stonecutterRecipes[] = {
    { "Cut Stone Blocks", ITEM_ROCK, 1, ITEM_BLOCKS, 2, 3.0f, MAT_MATCH_ANY, MAT_NONE, 0 },
};
int stonecutterRecipeCount = sizeof(stonecutterRecipes) / sizeof(stonecutterRecipes[0]);

// Sawmill recipes: logs -> planks or sticks (material-preserving)
Recipe sawmillRecipes[] = {
    { "Saw Planks", ITEM_LOG, 1, ITEM_PLANKS, 4, 4.0f, MAT_MATCH_ANY, MAT_NONE, 0 },
    { "Cut Sticks", ITEM_LOG, 1, ITEM_STICKS, 8, 2.0f, MAT_MATCH_ANY, MAT_NONE, 0 },
};
int sawmillRecipeCount = sizeof(sawmillRecipes) / sizeof(sawmillRecipes[0]);

// Kiln recipes: fire processing with fuel
Recipe kilnRecipes[] = {
    { "Fire Bricks",   ITEM_CLAY, 1, ITEM_BRICKS,   2, 5.0f, MAT_MATCH_ANY, MAT_NONE, 1 },
    { "Make Charcoal", ITEM_LOG,  1, ITEM_CHARCOAL,  3, 6.0f, MAT_MATCH_ANY, MAT_NONE, 0 },
};
int kilnRecipeCount = sizeof(kilnRecipes) / sizeof(kilnRecipes[0]);

// Workshop templates (ASCII art, row-major, top-to-bottom)
// . = floor (walkable), # = machinery (blocked), X = work tile, O = output
static const char* workshopTemplates[] = {
    [WORKSHOP_STONECUTTER] = 
        "##O"
        "#X."
        "...",
    [WORKSHOP_SAWMILL] =
        "##O"
        "#X."
        "...",
    [WORKSHOP_KILN] =
        "#F#"
        "#XO"
        "...",
};

static bool WorkshopHasInputForRecipe(Workshop* ws, Recipe* recipe, int searchRadius) {
    if (searchRadius == 0) searchRadius = 100;  // Large default
    int bestDistSq = searchRadius * searchRadius;

    for (int i = 0; i < itemHighWaterMark; i++) {
        Item* item = &items[i];
        if (!item->active) continue;
        if (!RecipeInputMatches(recipe, item)) continue;
        if (item->reservedBy != -1) continue;
        if (item->unreachableCooldown > 0.0f) continue;
        if ((int)item->z != ws->z) continue;

        int itemTileX = (int)(item->x / CELL_SIZE);
        int itemTileY = (int)(item->y / CELL_SIZE);
        int dx = itemTileX - ws->x;
        int dy = itemTileY - ws->y;
        int distSq = dx * dx + dy * dy;
        if (distSq > bestDistSq) continue;

        return true;
    }

    return false;
}

// Check if any unreserved fuel item (IF_FUEL flag) exists within search radius
bool WorkshopHasFuelForRecipe(Workshop* ws, int searchRadius) {
    if (searchRadius == 0) searchRadius = 100;
    int bestDistSq = searchRadius * searchRadius;

    for (int i = 0; i < itemHighWaterMark; i++) {
        Item* item = &items[i];
        if (!item->active) continue;
        if (!(ItemFlags(item->type) & IF_FUEL)) continue;
        if (item->reservedBy != -1) continue;
        if (item->unreachableCooldown > 0.0f) continue;
        if ((int)item->z != ws->z) continue;

        int itemTileX = (int)(item->x / CELL_SIZE);
        int itemTileY = (int)(item->y / CELL_SIZE);
        int dx = itemTileX - ws->x;
        int dy = itemTileY - ws->y;
        int distSq = dx * dx + dy * dy;
        if (distSq > bestDistSq) continue;

        return true;
    }
    return false;
}

// Find nearest unreserved fuel item within search radius
int FindNearestFuelItem(Workshop* ws, int searchRadius) {
    if (searchRadius == 0) searchRadius = 100;
    int bestDistSq = searchRadius * searchRadius;
    int bestIdx = -1;

    for (int i = 0; i < itemHighWaterMark; i++) {
        Item* item = &items[i];
        if (!item->active) continue;
        if (!(ItemFlags(item->type) & IF_FUEL)) continue;
        if (item->reservedBy != -1) continue;
        if (item->unreachableCooldown > 0.0f) continue;
        if ((int)item->z != ws->z) continue;

        int itemTileX = (int)(item->x / CELL_SIZE);
        int itemTileY = (int)(item->y / CELL_SIZE);
        int dx = itemTileX - ws->x;
        int dy = itemTileY - ws->y;
        int distSq = dx * dx + dy * dy;
        if (distSq > bestDistSq) continue;

        bestDistSq = distSq;
        bestIdx = i;
    }
    return bestIdx;
}

bool RecipeInputMatches(const Recipe* recipe, const Item* item) {
    if (!recipe || !item) return false;
    if (item->type != recipe->inputType) return false;

    if (recipe->inputMaterialMatch == MAT_MATCH_ANY) return true;

    MaterialType mat = (MaterialType)item->material;
    if (mat == MAT_NONE) {
        mat = (MaterialType)DefaultMaterialForItemType(item->type);
    }

    switch (recipe->inputMaterialMatch) {
        case MAT_MATCH_EXACT:
            return mat == recipe->inputMaterial;
        case MAT_MATCH_WOOD:
            return IsWoodMaterial(mat);
        case MAT_MATCH_STONE:
            return IsStoneMaterial(mat);
        case MAT_MATCH_METAL:
            return IsMetalMaterial(mat);
        case MAT_MATCH_ANY:
        default:
            return true;
    }
}

void ClearWorkshops(void) {
    for (int i = 0; i < MAX_WORKSHOPS; i++) {
        workshops[i].active = false;
        workshops[i].assignedCrafter = -1;
        workshops[i].billCount = 0;
        workshops[i].linkedInputCount = 0;
        workshops[i].visualState = WORKSHOP_VISUAL_NO_WORKER;
        workshops[i].inputStarvationTime = 0.0f;
        workshops[i].outputBlockedTime = 0.0f;
        workshops[i].lastWorkTime = 0.0f;
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
            ws->visualState = WORKSHOP_VISUAL_NO_WORKER;
            ws->inputStarvationTime = 0.0f;
            ws->outputBlockedTime = 0.0f;
            ws->lastWorkTime = 0.0f;
            
            // Default 3x3 footprint
            ws->width = 3;
            ws->height = 3;
            
            // Copy template and find special tiles
            const char* tmpl = workshopTemplates[type];
            ws->workTileX = x;
            ws->workTileY = y;
            ws->outputTileX = x;
            ws->outputTileY = y;
            ws->fuelTileX = -1;
            ws->fuelTileY = -1;
            
            // First pass: copy template and set blocking flags
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
                    } else if (c == WT_FUEL) {
                        ws->fuelTileX = x + tx;
                        ws->fuelTileY = y + ty;
                    }
                    
                    // Set blocking flag for machinery tiles
                    if (c == WT_BLOCK) {
                        SET_CELL_FLAG(x + tx, y + ty, z, CELL_FLAG_WORKSHOP_BLOCK);
                    }
                }
            }
            
            // Second pass: push movers out, invalidate paths, mark HPA* chunks dirty
            // Done separately so movers aren't pushed into tiles that will be blocked
            for (int ty = 0; ty < ws->height; ty++) {
                for (int tx = 0; tx < ws->width; tx++) {
                    if (ws->template[ty * ws->width + tx] == WT_BLOCK) {
                        PushMoversOutOfCell(x + tx, y + ty, z);
                        InvalidatePathsThroughCell(x + tx, y + ty, z);
                        MarkChunkDirty(x + tx, y + ty, z);  // Update HPA* graph
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
        
        // Clear blocking flags for machinery tiles and mark HPA* chunks dirty
        for (int ty = 0; ty < ws->height; ty++) {
            for (int tx = 0; tx < ws->width; tx++) {
                if (ws->template[ty * ws->width + tx] == WT_BLOCK) {
                    CLEAR_CELL_FLAG(ws->x + tx, ws->y + ty, ws->z, CELL_FLAG_WORKSHOP_BLOCK);
                    MarkChunkDirty(ws->x + tx, ws->y + ty, ws->z);  // Update HPA* graph
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
        case WORKSHOP_SAWMILL:
            *outCount = sawmillRecipeCount;
            return sawmillRecipes;
        case WORKSHOP_KILN:
            *outCount = kilnRecipeCount;
            return kilnRecipes;
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

void UpdateWorkshopDiagnostics(float dt) {
    for (int w = 0; w < MAX_WORKSHOPS; w++) {
        Workshop* ws = &workshops[w];
        if (!ws->active) continue;

        bool isWorking = false;
        if (ws->assignedCrafter >= 0 && ws->assignedCrafter < moverCount) {
            Mover* m = &movers[ws->assignedCrafter];
            if (m->active && m->currentJobId >= 0) {
                Job* job = GetJob(m->currentJobId);
                if (job && job->type == JOBTYPE_CRAFT && job->targetWorkshop == w) {
                    isWorking = true;
                }
            }
        }

        bool anyRunnable = false;
        bool anyOutputSpace = false;
        bool anyInput = false;

        int recipeCount;
        Recipe* recipes = GetRecipesForWorkshop(ws->type, &recipeCount);

        for (int b = 0; b < ws->billCount; b++) {
            Bill* bill = &ws->bills[b];
            if (bill->suspended) {
                if (bill->suspendedNoStorage) {
                    anyRunnable = true;
                }
                continue;
            }
            if (!ShouldBillRun(ws, bill)) continue;

            if (bill->recipeIdx < 0 || bill->recipeIdx >= recipeCount) continue;
            Recipe* recipe = &recipes[bill->recipeIdx];

            anyRunnable = true;

            int outSlotX, outSlotY;
            if (FindStockpileForItem(recipe->outputType, MAT_NONE, &outSlotX, &outSlotY) < 0) {
                continue;
            }
            anyOutputSpace = true;

            if (WorkshopHasInputForRecipe(ws, recipe, bill->ingredientSearchRadius)) {
                anyInput = true;
                break;
            }
        }

        bool outputBlocked = anyRunnable && !anyOutputSpace;
        bool inputMissing = anyOutputSpace && !anyInput;

        if (outputBlocked) ws->outputBlockedTime += dt;
        else ws->outputBlockedTime = 0.0f;

        if (inputMissing) ws->inputStarvationTime += dt;
        else ws->inputStarvationTime = 0.0f;

        if (isWorking) ws->visualState = WORKSHOP_VISUAL_WORKING;
        else if (outputBlocked) ws->visualState = WORKSHOP_VISUAL_OUTPUT_FULL;
        else if (inputMissing) ws->visualState = WORKSHOP_VISUAL_INPUT_EMPTY;
        else ws->visualState = WORKSHOP_VISUAL_NO_WORKER;
    }
}
