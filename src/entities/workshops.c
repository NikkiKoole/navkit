#include "workshops.h"
#include "stockpiles.h"
#include "mover.h"
#include "jobs.h"
#include "item_defs.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include "../simulation/smoke.h"
#include <string.h>
#include <stdio.h>

Workshop workshops[MAX_WORKSHOPS];
int workshopCount = 0;

// Stonecutter recipes: 1 raw stone -> 2 blocks (material-preserving)
// Recipe format: { name, input1, count1, input2, count2, output, outCount, output2, outCount2, workRequired, passiveWorkRequired, matMatch, mat, fuel, itemMatch }
Recipe stonecutterRecipes[] = {
    { "Cut Stone Blocks", ITEM_ROCK, 1, ITEM_NONE, 0, ITEM_BLOCKS, 2, ITEM_NONE, 0, 3.0f, 0, MAT_MATCH_ANY, MAT_NONE, 0, ITEM_MATCH_EXACT },
    { "Crush Gravel",     ITEM_ROCK, 1, ITEM_NONE, 0, ITEM_GRAVEL, 3, ITEM_NONE, 0, 2.0f, 0, MAT_MATCH_ANY, MAT_NONE, 0, ITEM_MATCH_EXACT },
    { "Bind Gravel",      ITEM_GRAVEL, 2, ITEM_CLAY, 1, ITEM_BLOCKS, 1, ITEM_NONE, 0, 4.0f, 0, MAT_MATCH_ANY, MAT_NONE, 0, ITEM_MATCH_EXACT },
};
int stonecutterRecipeCount = sizeof(stonecutterRecipes) / sizeof(stonecutterRecipes[0]);

// Sawmill recipes: logs -> planks or sticks (material-preserving)
Recipe sawmillRecipes[] = {
    { "Saw Planks",    ITEM_LOG, 1, ITEM_NONE, 0, ITEM_PLANKS, 4, ITEM_NONE, 0, 4.0f, 0, MAT_MATCH_ANY, MAT_NONE, 0, ITEM_MATCH_EXACT },
    { "Cut Sticks",    ITEM_LOG, 1, ITEM_NONE, 0, ITEM_STICKS, 8, ITEM_NONE, 0, 2.0f, 0, MAT_MATCH_ANY, MAT_NONE, 0, ITEM_MATCH_EXACT },
    { "Strip Bark",    ITEM_LOG, 1, ITEM_NONE, 0, ITEM_STRIPPED_LOG, 1, ITEM_BARK, 2, 4.0f, 0, MAT_MATCH_ANY, MAT_NONE, 0, ITEM_MATCH_EXACT },
    { "Saw Stripped",  ITEM_STRIPPED_LOG, 1, ITEM_NONE, 0, ITEM_PLANKS, 5, ITEM_NONE, 0, 4.0f, 0, MAT_MATCH_ANY, MAT_NONE, 0, ITEM_MATCH_EXACT },
};
int sawmillRecipeCount = sizeof(sawmillRecipes) / sizeof(sawmillRecipes[0]);

// Kiln recipes: fire processing with fuel
Recipe kilnRecipes[] = {
    { "Fire Bricks",   ITEM_CLAY, 1, ITEM_NONE, 0, ITEM_BRICKS,   2, ITEM_NONE, 0, 5.0f, 0, MAT_MATCH_ANY, MAT_NONE, 1, ITEM_MATCH_EXACT },
    { "Make Charcoal", ITEM_LOG,  1, ITEM_NONE, 0, ITEM_CHARCOAL, 3, ITEM_NONE, 0, 6.0f, 0, MAT_MATCH_ANY, MAT_NONE, 0, ITEM_MATCH_EXACT },
    { "Burn Peat",     ITEM_PEAT, 1, ITEM_NONE, 0, ITEM_CHARCOAL, 3, ITEM_NONE, 0, 5.0f, 0, MAT_MATCH_ANY, MAT_NONE, 0, ITEM_MATCH_EXACT },
};
int kilnRecipeCount = sizeof(kilnRecipes) / sizeof(kilnRecipes[0]);

// Charcoal Pit recipes: semi-passive (short ignition + long passive burn)
Recipe charcoalPitRecipes[] = {
    { "Char Logs",   ITEM_LOG,    1, ITEM_NONE, 0, ITEM_CHARCOAL, 2, ITEM_NONE, 0, 2.0f, 60.0f, MAT_MATCH_ANY, MAT_NONE, 0, ITEM_MATCH_EXACT },
    { "Char Peat",   ITEM_PEAT,   1, ITEM_NONE, 0, ITEM_CHARCOAL, 2, ITEM_NONE, 0, 2.0f, 50.0f, MAT_MATCH_ANY, MAT_NONE, 0, ITEM_MATCH_EXACT },
    { "Char Sticks", ITEM_STICKS, 4, ITEM_NONE, 0, ITEM_CHARCOAL, 1, ITEM_NONE, 0, 2.0f, 40.0f, MAT_MATCH_ANY, MAT_NONE, 0, ITEM_MATCH_EXACT },
};
int charcoalPitRecipeCount = sizeof(charcoalPitRecipes) / sizeof(charcoalPitRecipes[0]);

// Hearth recipes: burn any fuel to produce ash (fuel sink)
Recipe hearthRecipes[] = {
    { "Burn Fuel", ITEM_NONE, 1, ITEM_NONE, 0, ITEM_ASH, 1, ITEM_NONE, 0, 4.0f, 0, MAT_MATCH_ANY, MAT_NONE, 0, ITEM_MATCH_ANY_FUEL },
};
int hearthRecipeCount = sizeof(hearthRecipes) / sizeof(hearthRecipes[0]);

// Drying Rack recipes: pure passive (no crafter, only timer)
Recipe dryingRackRecipes[] = {
    { "Dry Grass", ITEM_GRASS, 1, ITEM_NONE, 0, ITEM_DRIED_GRASS, 1, ITEM_NONE, 0, 0, 10.0f, MAT_MATCH_ANY, MAT_NONE, 0, ITEM_MATCH_EXACT },
};
int dryingRackRecipeCount = sizeof(dryingRackRecipes) / sizeof(dryingRackRecipes[0]);

// Rope Maker recipes: twist fibers into string, braid string into cordage
Recipe ropeMakerRecipes[] = {
    { "Twist Bark",    ITEM_BARK, 2,         ITEM_NONE, 0, ITEM_SHORT_STRING, 3, ITEM_NONE, 0, 3.0f, 0, MAT_MATCH_ANY, MAT_NONE, 0, ITEM_MATCH_EXACT },
    { "Twist Grass",   ITEM_DRIED_GRASS, 4,  ITEM_NONE, 0, ITEM_SHORT_STRING, 2, ITEM_NONE, 0, 3.0f, 0, MAT_MATCH_ANY, MAT_NONE, 0, ITEM_MATCH_EXACT },
    { "Braid Cordage", ITEM_SHORT_STRING, 3, ITEM_NONE, 0, ITEM_CORDAGE, 1,     ITEM_NONE, 0, 4.0f, 0, MAT_MATCH_ANY, MAT_NONE, 0, ITEM_MATCH_EXACT },
};
int ropeMakerRecipeCount = sizeof(ropeMakerRecipes) / sizeof(ropeMakerRecipes[0]);

// Workshop definitions table (consolidates templates, recipes, and metadata)
const WorkshopDef workshopDefs[WORKSHOP_TYPE_COUNT] = {
    [WORKSHOP_STONECUTTER] = {
        .type = WORKSHOP_STONECUTTER,
        .name = "STONECUTTER",
        .displayName = "Stonecutter",
        .width = 3,
        .height = 3,
        .template = "###"    // dense/solid layout
                    "#XO"
                    "..#",
        .recipes = stonecutterRecipes,
        .recipeCount = sizeof(stonecutterRecipes) / sizeof(stonecutterRecipes[0])
    },
    [WORKSHOP_SAWMILL] = {
        .type = WORKSHOP_SAWMILL,
        .name = "SAWMILL",
        .displayName = "Sawmill",
        .width = 3,
        .height = 3,
        .template = "#O#"    // open lane layout
                    ".X."
                    "#..",
        .recipes = sawmillRecipes,
        .recipeCount = sizeof(sawmillRecipes) / sizeof(sawmillRecipes[0])
    },
    [WORKSHOP_KILN] = {
        .type = WORKSHOP_KILN,
        .name = "KILN",
        .displayName = "Kiln",
        .width = 3,
        .height = 3,
        .template = "#F#"    // hot core layout (enclosed)
                    "#XO"
                    "###",
        .recipes = kilnRecipes,
        .recipeCount = sizeof(kilnRecipes) / sizeof(kilnRecipes[0])
    },
    [WORKSHOP_CHARCOAL_PIT] = {
        .type = WORKSHOP_CHARCOAL_PIT,
        .name = "CHARCOAL_PIT",
        .displayName = "Charcoal Pit",
        .width = 2,
        .height = 2,
        .template = "FX"
                    "O.",
        .recipes = charcoalPitRecipes,
        .recipeCount = sizeof(charcoalPitRecipes) / sizeof(charcoalPitRecipes[0]),
        .passive = true
    },
    [WORKSHOP_HEARTH] = {
        .type = WORKSHOP_HEARTH,
        .name = "HEARTH",
        .displayName = "Hearth",
        .width = 2,
        .height = 2,
        .template = "FX"
                    "O.",
        .recipes = hearthRecipes,
        .recipeCount = sizeof(hearthRecipes) / sizeof(hearthRecipes[0]),
        .passive = false
    },
    [WORKSHOP_DRYING_RACK] = {
        .type = WORKSHOP_DRYING_RACK,
        .name = "DRYING_RACK",
        .displayName = "Drying Rack",
        .width = 2,
        .height = 2,
        .template = "#X"
                    "O.",
        .recipes = dryingRackRecipes,
        .recipeCount = sizeof(dryingRackRecipes) / sizeof(dryingRackRecipes[0]),
        .passive = true
    },
    [WORKSHOP_ROPE_MAKER] = {
        .type = WORKSHOP_ROPE_MAKER,
        .name = "ROPE_MAKER",
        .displayName = "Rope Maker",
        .width = 2,
        .height = 2,
        .template = "#X"
                    "O.",
        .recipes = ropeMakerRecipes,
        .recipeCount = sizeof(ropeMakerRecipes) / sizeof(ropeMakerRecipes[0]),
        .passive = false
    },
};

// Compile-time check: ensure workshopDefs[] has an entry for every WorkshopType
_Static_assert(sizeof(workshopDefs) / sizeof(workshopDefs[0]) == WORKSHOP_TYPE_COUNT,
               "workshopDefs[] must have an entry for every WorkshopType");

static bool WorkshopHasInputForRecipe(Workshop* ws, const Recipe* recipe, int searchRadius) {
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

    // Check item type matching
    if (recipe->inputItemMatch == ITEM_MATCH_ANY_FUEL) {
        // Match any item with IF_FUEL flag
        if (!(ItemFlags(item->type) & IF_FUEL)) return false;
    } else {
        // Exact item type match (default)
        if (item->type != recipe->inputType) return false;
    }

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
        workshops[i].passiveProgress = 0.0f;
        workshops[i].passiveBillIdx = -1;
        workshops[i].passiveReady = false;
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
            ws->passiveProgress = 0.0f;
            ws->passiveBillIdx = -1;
            ws->passiveReady = false;
            
            // Get footprint from workshop definition
            ws->width = workshopDefs[type].width;
            ws->height = workshopDefs[type].height;
            
            // Copy template and find special tiles
            const char* tmpl = workshopDefs[type].template;
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

const Recipe* GetRecipesForWorkshop(WorkshopType type, int* outCount) {
    if (type < 0 || type >= WORKSHOP_TYPE_COUNT) {
        *outCount = 0;
        return NULL;
    }

    *outCount = workshopDefs[type].recipeCount;
    return workshopDefs[type].recipes;
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
    
    // Cancel any craft jobs targeting this bill or bills after it
    // (bills after it will shift down, invalidating their indices)
    for (int i = 0; i < activeJobCount; i++) {
        int jobId = activeJobList[i];
        Job* job = &jobs[jobId];
        if (!job->active) continue;
        if (job->type != JOBTYPE_CRAFT) continue;
        if (job->targetWorkshop != workshopIdx) continue;
        if (job->targetBillIdx >= billIdx) {
            // This job targets the removed bill or a bill that will shift
            // Cancel it to avoid recipe mismatch
            int moverIdx = job->assignedMover;
            if (moverIdx >= 0 && moverIdx < moverCount && movers[moverIdx].active) {
                CancelJob(&movers[moverIdx], moverIdx);
            }
        }
    }
    
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
    const Recipe* recipes = GetRecipesForWorkshop(ws->type, &recipeCount);
    if (bill->recipeIdx < 0 || bill->recipeIdx >= recipeCount) return false;
    const Recipe* recipe = &recipes[bill->recipeIdx];
    
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

bool IsPassiveWorkshopWorkTile(int tileX, int tileY, int z) {
    for (int w = 0; w < MAX_WORKSHOPS; w++) {
        Workshop* ws = &workshops[w];
        if (!ws->active) continue;
        if (!workshopDefs[ws->type].passive) continue;
        if (ws->z != z) continue;
        if (ws->workTileX != tileX || ws->workTileY != tileY) continue;
        
        // Protect if workshop is currently burning (passiveReady)
        if (ws->passiveReady) return true;
        
        // Protect if workshop has a runnable bill (waiting for delivery/ignition)
        for (int b = 0; b < ws->billCount; b++) {
            if (ShouldBillRun(ws, &ws->bills[b])) return true;
        }
    }
    return false;
}

void PassiveWorkshopsTick(float dt) {
    for (int w = 0; w < MAX_WORKSHOPS; w++) {
        Workshop* ws = &workshops[w];
        if (!ws->active) continue;
        if (!workshopDefs[ws->type].passive) continue;

        // Find first runnable bill
        int activeBillIdx = -1;
        for (int b = 0; b < ws->billCount; b++) {
            Bill* bill = &ws->bills[b];
            if (bill->suspended) continue;
            if (!ShouldBillRun(ws, bill)) continue;
            activeBillIdx = b;
            break;
        }

        if (activeBillIdx < 0) {
            ws->passiveProgress = 0.0f;
            ws->passiveBillIdx = -1;
            // If no runnable bill but workshop is marked ready, clear it
            if (ws->passiveReady) {
                ws->passiveReady = false;
                ws->assignedCrafter = -1;
            }
            continue;
        }

        // If bill changed, reset progress
        if (ws->passiveBillIdx != activeBillIdx) {
            ws->passiveProgress = 0.0f;
            ws->passiveBillIdx = activeBillIdx;
        }

        Bill* bill = &ws->bills[activeBillIdx];
        int recipeCount;
        const Recipe* recipes = GetRecipesForWorkshop(ws->type, &recipeCount);
        if (bill->recipeIdx < 0 || bill->recipeIdx >= recipeCount) continue;
        const Recipe* recipe = &recipes[bill->recipeIdx];

        // Check: are required inputs present on the work tile?
        int inputCount = 0;
        for (int i = 0; i < itemHighWaterMark; i++) {
            Item* item = &items[i];
            if (!item->active) continue;
            if (item->state != ITEM_ON_GROUND) continue;
            int tileX = (int)(item->x / CELL_SIZE);
            int tileY = (int)(item->y / CELL_SIZE);
            if (tileX != ws->workTileX || tileY != ws->workTileY) continue;
            if ((int)item->z != ws->z) continue;
            if (!RecipeInputMatches(recipe, item)) continue;
            inputCount++;
            if (inputCount >= recipe->inputCount) break;
        }

        if (inputCount < recipe->inputCount) {
            // Not enough input — stall (don't reset progress)
            // But if we're at 0% and passiveReady, the inputs vanished after ignition
            // Reset so the workshop can be re-ignited and re-delivered
            if (ws->passiveProgress == 0.0f && ws->passiveReady) {
                ws->passiveReady = false;
                ws->assignedCrafter = -1;
            }
            continue;
        }

        // Semi-passive gate: if recipe needs active crafter work, wait until passiveReady
        if (recipe->workRequired > 0 && !ws->passiveReady) {
            continue;  // Waiting for crafter to ignite/activate
        }

        // Advance timer using passive work duration
        ws->passiveProgress += dt / recipe->passiveWorkRequired;

        // Emit smoke while passively burning
        if (ws->fuelTileX >= 0 && GetRandomValue(0, 3) == 0) {
            AddSmoke(ws->fuelTileX, ws->fuelTileY, ws->z, 3);
        }

        if (ws->passiveProgress >= 1.0f) {
            // Consume input(s)
            int consumed = 0;
            MaterialType inputMat = MAT_NONE;
            for (int i = 0; i < itemHighWaterMark && consumed < recipe->inputCount; i++) {
                Item* item = &items[i];
                if (!item->active) continue;
                if (item->state != ITEM_ON_GROUND) continue;
                int tileX = (int)(item->x / CELL_SIZE);
                int tileY = (int)(item->y / CELL_SIZE);
                if (tileX != ws->workTileX || tileY != ws->workTileY) continue;
                if ((int)item->z != ws->z) continue;
                if (!RecipeInputMatches(recipe, item)) continue;
                if (consumed == 0) {
                    inputMat = (MaterialType)item->material;
                    if (inputMat == MAT_NONE) inputMat = (MaterialType)DefaultMaterialForItemType(item->type);
                }
                DeleteItem(i);
                consumed++;
            }

            // Spawn output(s) at output tile
            float outX = ws->outputTileX * CELL_SIZE + CELL_SIZE * 0.5f;
            float outY = ws->outputTileY * CELL_SIZE + CELL_SIZE * 0.5f;
            for (int i = 0; i < recipe->outputCount; i++) {
                uint8_t outMat;
                if (ItemTypeUsesMaterialName(recipe->outputType) && inputMat != MAT_NONE) {
                    outMat = (uint8_t)inputMat;
                } else {
                    outMat = DefaultMaterialForItemType(recipe->outputType);
                }
                SpawnItemWithMaterial(outX, outY, (float)ws->z, recipe->outputType, outMat);
            }
            if (recipe->outputType2 != ITEM_NONE) {
                for (int i = 0; i < recipe->outputCount2; i++) {
                    uint8_t outMat2;
                    if (ItemTypeUsesMaterialName(recipe->outputType2) && inputMat != MAT_NONE) {
                        outMat2 = (uint8_t)inputMat;
                    } else {
                        outMat2 = DefaultMaterialForItemType(recipe->outputType2);
                    }
                    SpawnItemWithMaterial(outX, outY, (float)ws->z, recipe->outputType2, outMat2);
                }
            }

            // Update bill and reset state
            bill->completedCount++;
            ws->passiveProgress = 0.0f;
            ws->passiveBillIdx = -1;  // Re-evaluate next tick
            ws->passiveReady = false; // Needs re-ignition for semi-passive
            ws->assignedCrafter = -1; // Clear assigned crafter so workshop can be re-ignited
        }
    }
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
        const Recipe* recipes = GetRecipesForWorkshop(ws->type, &recipeCount);

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
            const Recipe* recipe = &recipes[bill->recipeIdx];

            anyRunnable = true;

            // Check output storage using actual available input material
            bool hasStorage = false;
            for (int i = 0; i < itemHighWaterMark; i++) {
                if (!items[i].active) continue;
                if (!RecipeInputMatches(recipe, &items[i])) continue;
                if (items[i].reservedBy != -1) continue;
                if ((int)items[i].z != ws->z) continue;
                uint8_t mat = items[i].material;
                if (mat == MAT_NONE) mat = DefaultMaterialForItemType(items[i].type);
                int outSlotX, outSlotY;
                if (FindStockpileForItem(recipe->outputType, mat, &outSlotX, &outSlotY) >= 0) {
                    if (recipe->outputType2 == ITEM_NONE ||
                        FindStockpileForItem(recipe->outputType2, mat, &outSlotX, &outSlotY) >= 0) {
                        hasStorage = true;
                        break;
                    }
                }
            }
            if (!hasStorage) {
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
