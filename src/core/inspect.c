// Save file inspector - compiled with the game binary
// Usage: ./bin/path --inspect [filename] [options]

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../world/grid.h"
#include "../simulation/water.h"
#include "../simulation/fire.h"
#include "../simulation/smoke.h"
#include "../simulation/steam.h"
#include "../simulation/temperature.h"
#include "../entities/items.h"
#include "../entities/item_defs.h"
#include "../entities/stockpiles.h"
#include "../world/designations.h"
#include "../entities/mover.h"
#include "../entities/jobs.h"
#include "../entities/workshops.h"
#include "../world/pathfinding.h"
#include "../world/cell_defs.h"
#include "../world/material.h"
#include "../simulation/trees.h"
#include "save_migrations.h"

#define INSPECT_V21_MAT_COUNT 10
#define INSPECT_SAVE_MAGIC 0x4E41564B

// Section markers (must match saveload.c)
#define MARKER_GRIDS    0x47524944  // "GRID"
#define MARKER_ENTITIES 0x454E5449  // "ENTI"
#define MARKER_VIEW     0x56494557  // "VIEW"
#define MARKER_SETTINGS 0x53455454  // "SETT"
#define MARKER_END      0x454E4421  // "END!"

static const char* InspectItemName(const Item* item, char* buffer, size_t bufferSize) {
    const char* base = (item->type < ITEM_TYPE_COUNT) ? ItemName(item->type) : "?";
    MaterialType mat = (MaterialType)item->material;
    if (mat == MAT_NONE) {
        mat = (MaterialType)DefaultMaterialForItemType(item->type);
    }
    if (mat != MAT_NONE && ItemTypeUsesMaterialName(item->type)) {
        char matName[32];
        snprintf(matName, sizeof(matName), "%s", MaterialName(mat));
        if (matName[0] >= 'a' && matName[0] <= 'z') {
            matName[0] = (char)(matName[0] - 'a' + 'A');
        }
        snprintf(buffer, bufferSize, "%s %s", matName, base);
        return buffer;
    }
    return base;
}

static const char* cellTypeNames[] = {
    "WALL",          // 0
    "AIR",           // 1
    "LADDER_UP",     // 2
    "LADDER_DOWN",   // 3
    "LADDER_BOTH",   // 4
    "DIRT",          // 5
    "CLAY",          // 6
    "GRAVEL",        // 7
    "SAND",          // 8
    "PEAT",          // 9
    "ROCK",          // 10
    "BEDROCK",       // 11 (was WOOD_WALL, now removed)
    "RAMP_N",        // 12
    "RAMP_E",        // 13
    "RAMP_S",        // 14
    "RAMP_W",        // 15
    "SAPLING",       // 16
    "TREE_TRUNK",    // 17
    "TREE_LEAVES"    // 18
};
#define CELL_TYPE_COUNT 20
// Item names now come from ItemName() in item_defs.h
static const char* itemStateNames[] = {"ON_GROUND", "CARRIED", "IN_STOCKPILE"};
static const char* jobTypeNames[] = {"NONE", "HAUL", "CLEAR", "MINE", "CHANNEL", "DIG_RAMP", "REMOVE_FLOOR", "HAUL_TO_BP", "BUILD", "CRAFT", "REMOVE_RAMP", "CHOP", "GATHER_SAPLING", "PLANT_SAPLING", "CHOP_FELLED"};
static const char* designationTypeNames[] = {"NONE", "MINE", "CHANNEL", "DIG_RAMP", "REMOVE_FLOOR", "REMOVE_RAMP", "CHOP", "CHOP_FELLED", "GATHER_SAPLING", "PLANT_SAPLING"};
static const char* finishNames[] = {"ROUGH", "SMOOTH", "POLISHED", "ENGRAVED"};

// Loaded data (separate from game globals so we don't corrupt game state)
static uint64_t insp_worldSeed = 0;
static int insp_gridW, insp_gridH, insp_gridD, insp_chunkW, insp_chunkH;
static CellType* insp_gridCells = NULL;
static WaterCell* insp_waterCells = NULL;
static FireCell* insp_fireCells = NULL;
static SmokeCell* insp_smokeCells = NULL;
static SteamCell* insp_steamCells = NULL;
static uint8_t* insp_cellFlags = NULL;
static uint8_t* insp_wallMaterials = NULL;
static uint8_t* insp_floorMaterials = NULL;
static uint8_t* insp_wallNatural = NULL;
static uint8_t* insp_floorNatural = NULL;
static uint8_t* insp_wallFinish = NULL;
static uint8_t* insp_floorFinish = NULL;
static TempCell* insp_tempCells = NULL;
static Designation* insp_designations = NULL;
static int insp_itemHWM = 0;
static Item* insp_items = NULL;
static Stockpile* insp_stockpiles = NULL;
static int insp_gatherZoneCount = 0;
static GatherZone* insp_gatherZones = NULL;
static Blueprint* insp_blueprints = NULL;
static Workshop* insp_workshops = NULL;
static int insp_moverCount = 0;
static Mover* insp_movers = NULL;
static int insp_jobHWM = 0, insp_activeJobCnt = 0;
static Job* insp_jobs = NULL;
static int* insp_activeJobList = NULL;

static void print_mover(int idx) {
    if (idx < 0 || idx >= insp_moverCount) {
        printf("Mover %d out of range (0-%d)\n", idx, insp_moverCount-1);
        return;
    }
    Mover* m = &insp_movers[idx];
    printf("\n=== MOVER %d ===\n", idx);
    printf("Position: (%.2f, %.2f, z%.0f) -> cell (%d, %d)\n", 
           m->x, m->y, m->z, (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE));
    printf("Active: %s\n", m->active ? "YES" : "no");
    printf("Speed: %.1f\n", m->speed);
    printf("Goal: (%d, %d, z%d)\n", m->goal.x, m->goal.y, m->goal.z);
    printf("Path: length=%d, index=%d\n", m->pathLength, m->pathIndex);
    if (m->pathLength > 0 && m->pathIndex >= 0 && m->pathIndex < m->pathLength) {
        printf("  Next waypoint: (%d, %d, z%d)\n", 
               m->path[m->pathIndex].x, m->path[m->pathIndex].y, m->path[m->pathIndex].z);
        printf("  Final waypoint: (%d, %d, z%d)\n",
               m->path[0].x, m->path[0].y, m->path[0].z);
    }
    printf("Needs repath: %s\n", m->needsRepath ? "YES" : "no");
    printf("Repath cooldown: %d frames\n", m->repathCooldown);
    printf("Time without progress: %.2f sec%s\n", m->timeWithoutProgress,
           m->timeWithoutProgress > 2.0f ? " (STUCK!)" : "");
    printf("Time near waypoint: %.2f sec\n", m->timeNearWaypoint);
    printf("Last position: (%.2f, %.2f)\n", m->lastX, m->lastY);
    printf("Fall timer: %.2f\n", m->fallTimer);
    printf("Job ID: %d\n", m->currentJobId);
    
    if (m->currentJobId >= 0 && m->currentJobId < insp_jobHWM) {
        Job* job = &insp_jobs[m->currentJobId];
        printf("\n  --- Job %d ---\n", m->currentJobId);
        printf("  Type: %s\n", job->type < 15 ? jobTypeNames[job->type] : "?");
        printf("  Step: %d\n", job->step);
        printf("  Progress: %.2f\n", job->progress);
        if (job->targetItem >= 0) printf("  Target item: %d\n", job->targetItem);
        if (job->carryingItem >= 0) printf("  Carrying item: %d\n", job->carryingItem);
        if (job->targetStockpile >= 0) printf("  Target stockpile: %d slot (%d,%d)\n", 
            job->targetStockpile, job->targetSlotX, job->targetSlotY);
        if (job->targetMineX >= 0) printf("  Target mine: (%d,%d,z%d)\n", 
            job->targetMineX, job->targetMineY, job->targetMineZ);
        if (job->targetBlueprint >= 0) printf("  Target blueprint: %d\n", job->targetBlueprint);
    }
    
    printf("Capabilities: haul=%d mine=%d build=%d plant=%d\n",
           m->capabilities.canHaul, m->capabilities.canMine, m->capabilities.canBuild, m->capabilities.canPlant);
}

static void print_item(int idx) {
    if (idx < 0 || idx >= insp_itemHWM) {
        printf("Item %d out of range (0-%d)\n", idx, insp_itemHWM-1);
        return;
    }
    Item* item = &insp_items[idx];
    printf("\n=== ITEM %d ===\n", idx);
    printf("Position: (%.2f, %.2f, z%.0f) -> cell (%d, %d)\n",
           item->x, item->y, item->z, (int)(item->x / CELL_SIZE), (int)(item->y / CELL_SIZE));
    printf("Active: %s\n", item->active ? "YES" : "no");
    {
        char nameBuf[64];
        const char* typeName = InspectItemName(item, nameBuf, sizeof(nameBuf));
        printf("Type: %s\n", typeName);
    }
    printf("State: %s\n", item->state < 3 ? itemStateNames[item->state] : "?");
    printf("Reserved by mover: %d%s\n", item->reservedBy, 
           item->reservedBy >= 0 ? "" : " (none)");
    printf("Unreachable cooldown: %.2f\n", item->unreachableCooldown);
    
    // Find who's targeting this item
    for (int i = 0; i < insp_jobHWM; i++) {
        if (insp_jobs[i].targetItem == idx || insp_jobs[i].carryingItem == idx) {
            printf("  Referenced by job %d (mover %d): %s\n", 
                   i, insp_jobs[i].assignedMover,
                   insp_jobs[i].carryingItem == idx ? "CARRYING" : "TARGET");
        }
    }
}

static void print_job(int idx) {
    if (idx < 0 || idx >= insp_jobHWM) {
        printf("Job %d out of range (0-%d)\n", idx, insp_jobHWM-1);
        return;
    }
    Job* job = &insp_jobs[idx];
    printf("\n=== JOB %d ===\n", idx);
    printf("Type: %s (%d)\n", job->type < 15 ? jobTypeNames[job->type] : "?", job->type);
    printf("Assigned mover: %d\n", job->assignedMover);
    printf("Step: %d\n", job->step);
    printf("Progress: %.2f\n", job->progress);
    printf("Target item: %d\n", job->targetItem);
    printf("Carrying item: %d\n", job->carryingItem);
    printf("Target stockpile: %d\n", job->targetStockpile);
    printf("Target slot: (%d, %d)\n", job->targetSlotX, job->targetSlotY);
    printf("Target mine: (%d, %d, z%d)\n", job->targetMineX, job->targetMineY, job->targetMineZ);
    printf("Target blueprint: %d\n", job->targetBlueprint);
}

static void print_stockpile(int idx) {
    if (idx < 0 || idx >= MAX_STOCKPILES) {
        printf("Stockpile %d out of range\n", idx);
        return;
    }
    Stockpile* sp = &insp_stockpiles[idx];
    printf("\n=== STOCKPILE %d ===\n", idx);
    printf("Active: %s\n", sp->active ? "YES" : "no");
    if (!sp->active) return;
    
    printf("Position: (%d, %d, z%d)\n", sp->x, sp->y, sp->z);
    printf("Size: %d x %d\n", sp->width, sp->height);
    printf("Priority: %d\n", sp->priority);
    printf("Free slots: %d\n", sp->freeSlotCount);
    printf("Max stack: %d\n", sp->maxStackSize);
    printf("Allowed: ");
    for (int t = 0; t < ITEM_TYPE_COUNT; t++) {
        if (sp->allowedTypes[t]) printf("%s ", ItemName(t));
    }
    printf("\n\nSlot grid (. = empty, X = inactive, # = count):\n");
    for (int y = 0; y < sp->height; y++) {
        printf("  ");
        for (int x = 0; x < sp->width; x++) {
            int i = y * sp->width + x;
            if (!sp->cells[i]) printf("X ");
            else if (sp->slots[i] < 0) printf(". ");
            else printf("%d ", sp->slotCounts[i]);
        }
        printf("\n");
    }
    
    // Show reservations
    printf("\nReservations:\n");
    int found = 0;
    for (int y = 0; y < sp->height; y++) {
        for (int x = 0; x < sp->width; x++) {
            int i = y * sp->width + x;
            if (sp->reservedBy[i] > 0) {
                printf("  Slot (%d,%d) reservations: %d\n", x, y, sp->reservedBy[i]);
                found++;
            }
        }
    }
    if (!found) printf("  (none)\n");
}

static const char* blueprintStateNames[] = {"AWAITING_MATERIALS", "READY_TO_BUILD", "BUILDING"};

static void print_blueprint(int idx) {
    if (idx < 0 || idx >= MAX_BLUEPRINTS) {
        printf("Blueprint %d out of range\n", idx);
        return;
    }
    Blueprint* bp = &insp_blueprints[idx];
    printf("\n=== BLUEPRINT %d ===\n", idx);
    printf("Active: %s\n", bp->active ? "YES" : "no");
    if (!bp->active) return;
    
    printf("Position: (%d, %d, z%d)\n", bp->x, bp->y, bp->z);
    printf("State: %s\n", bp->state < 3 ? blueprintStateNames[bp->state] : "UNKNOWN");
    printf("Materials: %d/%d delivered\n", bp->deliveredMaterialCount, bp->requiredMaterials);
    printf("Required item type: %s\n",
           bp->requiredItemType < ITEM_TYPE_COUNT ? ItemName(bp->requiredItemType) : "Any");
    printf("Reserved item: %d%s\n", bp->reservedItem,
           bp->reservedItem < 0 ? " (none)" : "");
    printf("Assigned builder: %d%s\n", bp->assignedBuilder,
           bp->assignedBuilder < 0 ? " (none)" : "");
    printf("Progress: %.1f%%\n", bp->progress * 100.0f);
}

static const char* billModeNames[] = {"DO_X_TIMES", "DO_UNTIL_X", "DO_FOREVER"};

static void print_workshop(int idx) {
    if (idx < 0 || idx >= MAX_WORKSHOPS) {
        printf("Workshop %d out of range\n", idx);
        return;
    }
    Workshop* ws = &insp_workshops[idx];
    printf("\n=== WORKSHOP %d ===\n", idx);
    printf("Active: %s\n", ws->active ? "YES" : "no");
    if (!ws->active) return;
    
    printf("Type: %s\n", ws->type < WORKSHOP_TYPE_COUNT ? workshopDefs[ws->type].name : "UNKNOWN");
    printf("Position: (%d, %d, z%d)\n", ws->x, ws->y, ws->z);
    printf("Size: %d x %d\n", ws->width, ws->height);
    printf("Work tile: (%d, %d)\n", ws->workTileX, ws->workTileY);
    printf("Output tile: (%d, %d)\n", ws->outputTileX, ws->outputTileY);
    printf("Assigned crafter: %d%s\n", ws->assignedCrafter,
           ws->assignedCrafter < 0 ? " (none)" : "");
    
    printf("\nBills: %d\n", ws->billCount);
    for (int b = 0; b < ws->billCount; b++) {
        Bill* bill = &ws->bills[b];
        printf("  Bill %d: recipe=%d, mode=%s", b, bill->recipeIdx,
               bill->mode < 3 ? billModeNames[bill->mode] : "?");
        if (bill->suspended) printf(" [SUSPENDED]");
        if (bill->mode == BILL_DO_X_TIMES) {
            printf(" (%d/%d)", bill->completedCount, bill->targetCount);
        } else if (bill->mode == BILL_DO_UNTIL_X) {
            printf(" (until %d)", bill->targetCount);
        } else {
            printf(" (completed: %d)", bill->completedCount);
        }
        printf("\n");
    }
    
    printf("\nLinked input stockpiles: %d\n", ws->linkedInputCount);
    for (int i = 0; i < ws->linkedInputCount; i++) {
        printf("  Stockpile %d\n", ws->linkedInputStockpiles[i]);
    }
}

static void print_cell(int x, int y, int z) {
    if (x < 0 || x >= insp_gridW || y < 0 || y >= insp_gridH || z < 0 || z >= insp_gridD) {
        printf("Cell (%d,%d,%d) out of range\n", x, y, z);
        return;
    }
    int idx = z * insp_gridH * insp_gridW + y * insp_gridW + x;
    CellType cell = insp_gridCells[idx];
    WaterCell water = insp_waterCells[idx];
    FireCell fire = insp_fireCells[idx];
    SmokeCell smoke = insp_smokeCells[idx];
    SteamCell steam = insp_steamCells[idx];
    TempCell temp = insp_tempCells[idx];
    Designation desig = insp_designations[idx];
    
    printf("\n=== CELL (%d, %d, z%d) ===\n", x, y, z);
    printf("Type: %s (raw=%d)\n", cell < CELL_TYPE_COUNT ? cellTypeNames[cell] : "UNKNOWN", (int)cell);
    
    // Wall material
    uint8_t wallMat = insp_wallMaterials[idx];
    if (wallMat != MAT_NONE) {
        const char* naturalTag = (insp_wallNatural && insp_wallNatural[idx]) ? " (natural)" : "";
        printf("Wall material: %s%s (raw=%d)\n",
               wallMat < MAT_COUNT ? MaterialName(wallMat) : "UNKNOWN",
               naturalTag, (int)wallMat);
        if (insp_wallFinish) {
            uint8_t finish = insp_wallFinish[idx];
            printf("Wall finish: %s (raw=%d)\n",
                   finish < FINISH_COUNT ? finishNames[finish] : "UNKNOWN",
                   (int)finish);
        }
    }
    
    // Floor material
    uint8_t floorMat = insp_floorMaterials[idx];
    if (floorMat != MAT_NONE) {
        const char* naturalTag = (insp_floorNatural && insp_floorNatural[idx]) ? " (natural)" : "";
        printf("Floor material: %s%s (raw=%d)\n",
               floorMat < MAT_COUNT ? MaterialName(floorMat) : "UNKNOWN",
               naturalTag, (int)floorMat);
        if (insp_floorFinish) {
            uint8_t finish = insp_floorFinish[idx];
            printf("Floor finish: %s (raw=%d)\n",
                   finish < FINISH_COUNT ? finishNames[finish] : "UNKNOWN",
                   (int)finish);
        }
    }
    
    // Walkability (requires globals to be set up)
    bool walkable = IsCellWalkableAt(z, y, x);
    printf("Walkable: %s", walkable ? "YES" : "NO");
    if (walkable) {
        if (CellIsLadder(cell)) printf(" (ladder)");
        else if (CellIsRamp(cell)) printf(" (ramp)");
        else if (insp_cellFlags[idx] & CELL_FLAG_HAS_FLOOR) printf(" (constructed floor)");
        else if (z == 0) printf(" (bedrock below)");
        else {
            int idxBelow = (z-1) * insp_gridH * insp_gridW + y * insp_gridW + x;
            CellType cellBelow = insp_gridCells[idxBelow];
            printf(" (solid below: %s)", cellBelow < CELL_TYPE_COUNT ? cellTypeNames[cellBelow] : "?");
        }
    } else {
        if (CellBlocksMovement(cell)) printf(" (blocks movement)");
        else if (insp_cellFlags[idx] & CELL_FLAG_WORKSHOP_BLOCK) printf(" (workshop blocks)");
        else if (z > 0) {
            int idxBelow = (z-1) * insp_gridH * insp_gridW + y * insp_gridW + x;
            CellType cellBelow = insp_gridCells[idxBelow];
            if (!CellIsSolid(cellBelow)) printf(" (no solid below: %s)", cellBelow < CELL_TYPE_COUNT ? cellTypeNames[cellBelow] : "?");
        }
    }
    // Show floor flag status
    if (insp_cellFlags[idx] & CELL_FLAG_HAS_FLOOR) {
        printf("  Floor flag: YES\n");
    }
    printf("\n");
    
    // Water
    printf("Water level: %d/7", water.level);
    if (water.stable) printf(" [STABLE]");
    if (water.isFrozen) printf(" [FROZEN]");
    printf("\n");
    if (water.isSource) printf("  IS SOURCE\n");
    if (water.isDrain) printf("  IS DRAIN\n");
    if (water.hasPressure) printf("  Has pressure from z%d\n", water.pressureSourceZ);
    
    // Fire
    if (fire.level > 0) {
        printf("Fire level: %d/7\n", fire.level);
        if (fire.isSource) printf("  IS SOURCE\n");
    }
    
    // Smoke
    if (smoke.level > 0 || smoke.stable) {
        printf("Smoke level: %d/7", smoke.level);
        if (smoke.stable) printf(" [STABLE]");
        if (smoke.hasPressure) printf(" [PRESSURE from z%d]", smoke.pressureSourceZ);
        printf("\n");
    }
    
    // Steam
    if (steam.level > 0) {
        printf("Steam level: %d/7\n", steam.level);
    }
    
    // Temperature (decode index to Celsius)
    printf("Temperature: %d C", temp.current);
    if (temp.isHeatSource) printf(" [HEAT SOURCE]");
    if (temp.isColdSource) printf(" [COLD SOURCE]");
    printf("\n");
    
    if (desig.type != DESIGNATION_NONE) {
        const char* desigName = desig.type < 10 ? designationTypeNames[desig.type] : "?";
        printf("Designation: %s (type=%d), assigned to mover %d, progress %.0f%%\n",
               desigName, desig.type, desig.assignedMover, desig.progress * 100);
    }
    
    // Find items at this cell
    printf("\nItems at this cell:\n");
    int found = 0;
    for (int i = 0; i < insp_itemHWM; i++) {
        if (!insp_items[i].active) continue;
        int ix = (int)(insp_items[i].x / CELL_SIZE);
        int iy = (int)(insp_items[i].y / CELL_SIZE);
        int iz = (int)insp_items[i].z;
        if (ix == x && iy == y && iz == z) {
            {
                char nameBuf[64];
                const char* typeName = InspectItemName(&insp_items[i], nameBuf, sizeof(nameBuf));
                printf("  Item %d: %s (%s)\n", i, 
                       typeName, itemStateNames[insp_items[i].state]);
            }
            found++;
        }
    }
    if (!found) printf("  (none)\n");
    
    // Find movers at this cell
    printf("\nMovers at this cell:\n");
    found = 0;
    for (int i = 0; i < insp_moverCount; i++) {
        if (!insp_movers[i].active) continue;
        int mx = (int)(insp_movers[i].x / CELL_SIZE);
        int my = (int)(insp_movers[i].y / CELL_SIZE);
        int mz = (int)insp_movers[i].z;
        if (mx == x && my == y && mz == z) {
            printf("  Mover %d (job %d)\n", i, insp_movers[i].currentJobId);
            found++;
        }
    }
    if (!found) printf("  (none)\n");
}

// Copy inspector grid data to game globals so pathfinding works
static void setup_pathfinding_globals(void) {
    gridWidth = insp_gridW;
    gridHeight = insp_gridH;
    gridDepth = insp_gridD;
    chunkWidth = insp_chunkW;
    chunkHeight = insp_chunkH;
    chunksX = gridWidth / chunkWidth;
    chunksY = gridHeight / chunkHeight;
    
    for (int z = 0; z < insp_gridD; z++) {
        for (int y = 0; y < insp_gridH; y++) {
            for (int x = 0; x < insp_gridW; x++) {
                int idx = z * insp_gridH * insp_gridW + y * insp_gridW + x;
                grid[z][y][x] = insp_gridCells[idx];
                cellFlags[z][y][x] = insp_cellFlags[idx];
            }
        }
    }
}

static void print_path(int x1, int y1, int z1, int x2, int y2, int z2, int algo) {
    const char* algoNames[] = {"A*", "HPA*", "JPS", "JPS+"};
    printf("\n=== PATH TEST (%s) ===\n", algo < 4 ? algoNames[algo] : "?");
    printf("From: (%d, %d, z%d)\n", x1, y1, z1);
    printf("To:   (%d, %d, z%d)\n", x2, y2, z2);
    
    // Check bounds
    if (x1 < 0 || x1 >= insp_gridW || y1 < 0 || y1 >= insp_gridH || z1 < 0 || z1 >= insp_gridD) {
        printf("Error: Start position out of bounds\n");
        return;
    }
    if (x2 < 0 || x2 >= insp_gridW || y2 < 0 || y2 >= insp_gridH || z2 < 0 || z2 >= insp_gridD) {
        printf("Error: Goal position out of bounds\n");
        return;
    }
    
    // Check walkability
    bool startWalkable = IsCellWalkableAt(z1, y1, x1);
    bool goalWalkable = IsCellWalkableAt(z2, y2, x2);
    printf("Start walkable: %s\n", startWalkable ? "YES" : "NO");
    printf("Goal walkable:  %s\n", goalWalkable ? "YES" : "NO");
    
    if (!startWalkable || !goalWalkable) {
        printf("Path: NOT POSSIBLE (endpoints not walkable)\n");
        return;
    }
    
    // Build HPA* graph if needed
    if (algo == PATH_ALGO_HPA) {
        BuildEntrances();
        BuildGraph();
    }
    
    // Run pathfinding
    Point start = { x1, y1, z1 };
    Point goal = { x2, y2, z2 };
    Point outPath[MAX_PATH];
    
    int len = FindPath(algo, start, goal, outPath, MAX_PATH);
    
    if (len > 0) {
        // Check if path crosses z-levels
        int minZ = outPath[0].z, maxZ = outPath[0].z;
        for (int i = 1; i < len; i++) {
            if (outPath[i].z < minZ) minZ = outPath[i].z;
            if (outPath[i].z > maxZ) maxZ = outPath[i].z;
        }
        if (minZ == maxZ) {
            printf("Path: FOUND (%d steps, all at z%d)\n", len, minZ);
        } else {
            printf("Path: FOUND (%d steps, z%d to z%d)\n", len, minZ, maxZ);
        }
    } else {
        printf("Path: NOT FOUND\n");
    }
}

static void print_map(int cx, int cy, int cz, int radius) {
    printf("\n=== MAP at z%d (center %d,%d, radius %d) ===\n", cz, cx, cy, radius);
    
    if (cz < 0 || cz >= insp_gridD) {
        printf("Error: z-level out of bounds\n");
        return;
    }
    
    int minX = cx - radius;
    int maxX = cx + radius;
    int minY = cy - radius;
    int maxY = cy + radius;
    
    // Clamp to grid bounds
    if (minX < 0) minX = 0;
    if (minY < 0) minY = 0;
    if (maxX >= insp_gridW) maxX = insp_gridW - 1;
    if (maxY >= insp_gridH) maxY = insp_gridH - 1;
    
    // Print column headers
    printf("     ");
    for (int x = minX; x <= maxX; x++) {
        printf("%d", x % 10);
    }
    printf("\n");
    
    // Print map
    for (int y = minY; y <= maxY; y++) {
        printf("%3d  ", y);
        for (int x = minX; x <= maxX; x++) {
            int idx = cz * insp_gridH * insp_gridW + y * insp_gridW + x;
            CellType cell = insp_gridCells[idx];
            WaterCell water = insp_waterCells[idx];
            Designation desig = insp_designations[idx];
            
            char c = '?';
            if (water.level > 0) c = '~';
            else if (desig.type == DESIGNATION_MINE) c = 'X';
            else {
                // Check for floor flag first (floors are AIR cells with HAS_FLOOR flag)
                if (insp_cellFlags && (insp_cellFlags[idx] & CELL_FLAG_HAS_FLOOR)) {
                    c = '.';
                } else switch (cell) {
                    case CELL_WALL:
                        c = '#'; break;
                    case CELL_AIR:
                        c = ' '; break;
                    case CELL_TERRAIN:
                        c = ':'; break;
                    case CELL_LADDER_UP:
                    case CELL_LADDER_DOWN:
                    case CELL_LADDER_BOTH:
                        c = 'H'; break;
                    case CELL_RAMP_N:
                        c = '^'; break;
                    case CELL_RAMP_E:
                        c = '>'; break;
                    case CELL_RAMP_S:
                        c = 'v'; break;
                    case CELL_RAMP_W:
                        c = '<'; break;
                    default:
                        c = '?'; break;
                }
            }
            
            // Mark center
            if (x == cx && y == cy) {
                printf("@");
            } else {
                printf("%c", c);
            }
        }
        printf("\n");
    }
    
    printf("\nLegend: # wall, . floor, , grass, : dirt, ~ water, X mine, H ladder, ^>v< ramp, @ center\n");
}

static void print_designations(void) {
    printf("\n=== DESIGNATIONS ===\n");
    int found = 0;
    
    for (int z = 0; z < insp_gridD; z++) {
        for (int y = 0; y < insp_gridH; y++) {
            for (int x = 0; x < insp_gridW; x++) {
                int idx = z * insp_gridH * insp_gridW + y * insp_gridW + x;
                Designation* d = &insp_designations[idx];
                if (d->type == DESIGNATION_NONE) continue;
                
                CellType cell = insp_gridCells[idx];
                const char* cellName = cell < CELL_TYPE_COUNT ? cellTypeNames[cell] : "?";
                const char* desigName = d->type < 10 ? designationTypeNames[d->type] : "?";
                
                printf("(%d,%d,z%d) %s %s", x, y, z, desigName, cellName);
                if (d->assignedMover >= 0) {
                    printf(" [mover %d, %.0f%%]", d->assignedMover, d->progress * 100);
                } else if (d->unreachableCooldown > 0) {
                    printf(" [UNREACHABLE %.1fs]", d->unreachableCooldown);
                } else {
                    printf(" [waiting]");
                }
                printf("\n");
                found++;
            }
        }
    }
    
    if (!found) printf("No designations found.\n");
    else printf("\nTotal: %d designations\n", found);
}

static void print_entrances(int filterZ) {
    setup_pathfinding_globals();
    BuildEntrances();
    BuildGraph();
    
    printf("\n=== HPA* ENTRANCES ===\n");
    printf("Chunk size: %dx%d, Chunks: %dx%d, Depth: %d\n",
           chunkWidth, chunkHeight, chunksX, chunksY, gridDepth);
    printf("Total entrances: %d, Ramp links: %d, Ladder links: %d\n\n", 
           entranceCount, rampLinkCount, ladderLinkCount);
    
    if (filterZ >= 0) {
        printf("Entrances at z=%d:\n", filterZ);
        int count = 0;
        for (int i = 0; i < entranceCount; i++) {
            if (entrances[i].z == filterZ) {
                printf("  [%d] (%d,%d,z%d) chunks %d<->%d\n",
                       i, entrances[i].x, entrances[i].y, entrances[i].z,
                       entrances[i].chunk1, entrances[i].chunk2);
                count++;
            }
        }
        printf("Total at z=%d: %d entrances\n", filterZ, count);
    } else {
        // Show summary by z-level
        for (int z = 0; z < gridDepth; z++) {
            int count = 0;
            for (int i = 0; i < entranceCount; i++) {
                if (entrances[i].z == z) count++;
            }
            if (count > 0) {
                printf("z=%d: %d entrances\n", z, count);
            }
        }
    }
    
    if (rampLinkCount > 0) {
        printf("\nRamp links:\n");
        for (int i = 0; i < rampLinkCount; i++) {
            printf("  Ramp at (%d,%d,z%d) -> exit (%d,%d,z%d)\n",
                   rampLinks[i].rampX, rampLinks[i].rampY, rampLinks[i].rampZ,
                   rampLinks[i].exitX, rampLinks[i].exitY, rampLinks[i].rampZ + 1);
        }
    }
}

static void print_stuck_movers(void) {
    printf("\n=== STUCK MOVERS (timeWithoutProgress > 2s) ===\n");
    int found = 0;
    for (int i = 0; i < insp_moverCount; i++) {
        if (!insp_movers[i].active) continue;
        if (insp_movers[i].timeWithoutProgress > 2.0f) {
            printf("\nMover %d: stuck for %.2f sec at (%.1f, %.1f, z%.0f)\n",
                   i, insp_movers[i].timeWithoutProgress, 
                   insp_movers[i].x, insp_movers[i].y, insp_movers[i].z);
            printf("  Goal: (%d,%d,z%d), path length=%d, needs repath=%s\n",
                   insp_movers[i].goal.x, insp_movers[i].goal.y, insp_movers[i].goal.z,
                   insp_movers[i].pathLength, insp_movers[i].needsRepath ? "yes" : "no");
            if (insp_movers[i].currentJobId >= 0) {
                Job* j = &insp_jobs[insp_movers[i].currentJobId];
                printf("  Job: %s step=%d\n", j->type < 15 ? jobTypeNames[j->type] : "?", j->step);
            }
            found++;
        }
    }
    if (!found) printf("No stuck movers found.\n");
}

static void print_reserved_items(void) {
    printf("\n=== RESERVED ITEMS ===\n");
    int found = 0;
    for (int i = 0; i < insp_itemHWM; i++) {
        if (!insp_items[i].active) continue;
        if (insp_items[i].reservedBy >= 0) {
        {
            char nameBuf[64];
            const char* typeName = InspectItemName(&insp_items[i], nameBuf, sizeof(nameBuf));
            printf("Item %d (%s at %.0f,%.0f): reserved by mover %d\n",
                   i, typeName,
                   insp_items[i].x, insp_items[i].y, insp_items[i].reservedBy);
        }
            found++;
        }
    }
    if (!found) printf("No reserved items.\n");
}

static void print_active_jobs(void) {
    printf("\n=== ACTIVE JOBS ===\n");
    for (int i = 0; i < insp_activeJobCnt; i++) {
        int jid = insp_activeJobList[i];
        Job* j = &insp_jobs[jid];
        printf("Job %d: %s mover=%d step=%d", 
               jid, j->type < 15 ? jobTypeNames[j->type] : "?", j->assignedMover, j->step);
        if (j->targetItem >= 0) printf(" item=%d", j->targetItem);
        if (j->carryingItem >= 0) printf(" carrying=%d", j->carryingItem);
        printf("\n");
    }
    if (insp_activeJobCnt == 0) printf("No active jobs.\n");
}

static void print_items(const char* filterType) {
    printf("\n=== ITEMS ===\n");
    int found = 0;
    for (int i = 0; i < insp_itemHWM; i++) {
        if (!insp_items[i].active) continue;
        Item* item = &insp_items[i];
        char nameBuf[64];
        const char* typeName = InspectItemName(item, nameBuf, sizeof(nameBuf));
        
        // Filter by type if specified
        if (filterType && strcmp(filterType, typeName) != 0) continue;
        
        int cellX = (int)(item->x / CELL_SIZE);
        int cellY = (int)(item->y / CELL_SIZE);
        int cellZ = (int)item->z;
        
        printf("Item %d: %s at (%d,%d,z%d) %s", 
               i, typeName, cellX, cellY, cellZ,
               item->state < 3 ? itemStateNames[item->state] : "?");
        if (item->reservedBy >= 0) printf(" [reserved by mover %d]", item->reservedBy);
        if (item->unreachableCooldown > 0) printf(" [UNREACHABLE %.1fs]", item->unreachableCooldown);
        printf("\n");
        found++;
    }
    if (!found) {
        if (filterType) printf("No %s items found.\n", filterType);
        else printf("No items found.\n");
    } else {
        printf("\nTotal: %d items\n", found);
    }
}

static void print_temp(int filterZ) {
    printf("\n=== TEMPERATURE ===\n");
    printf("Grid: %dx%dx%d\n", insp_gridW, insp_gridH, insp_gridD);
    
    // Chunk size for grouping (matching SIM_CHUNK_SIZE)
    const int CHUNK = 16;
    int chunksX = (insp_gridW + CHUNK - 1) / CHUNK;
    int chunksY = (insp_gridH + CHUNK - 1) / CHUNK;
    
    int startZ = (filterZ >= 0) ? filterZ : 0;
    int endZ = (filterZ >= 0) ? filterZ + 1 : insp_gridD;
    
    for (int z = startZ; z < endZ; z++) {
        printf("\nZ-level %d:\n", z);
        
        // Overall stats for this z-level
        int zAt0 = 0, zAt20 = 0, zOther = 0, zMin = 9999, zMax = -9999;
        
        // Show chunk grid with avg temps
        printf("Chunk temperatures (avg):\n");
        printf("     ");
        for (int cx = 0; cx < chunksX; cx++) printf(" %3d ", cx);
        printf("\n");
        
        for (int cy = 0; cy < chunksY; cy++) {
            printf(" %2d: ", cy);
            for (int cx = 0; cx < chunksX; cx++) {
                int sum = 0, count = 0;
                int cMin = 9999, cMax = -9999;
                
                int startX = cx * CHUNK;
                int startY = cy * CHUNK;
                int endX = startX + CHUNK; if (endX > insp_gridW) endX = insp_gridW;
                int endY = startY + CHUNK; if (endY > insp_gridH) endY = insp_gridH;
                
                for (int y = startY; y < endY; y++) {
                    for (int x = startX; x < endX; x++) {
                        int idx = z * insp_gridH * insp_gridW + y * insp_gridW + x;
                        int temp = insp_tempCells[idx].current;
                        sum += temp;
                        count++;
                        if (temp < cMin) cMin = temp;
                        if (temp > cMax) cMax = temp;
                        
                        // Z-level stats
                        if (temp == 0) zAt0++;
                        else if (temp == 20) zAt20++;
                        else zOther++;
                        if (temp < zMin) zMin = temp;
                        if (temp > zMax) zMax = temp;
                    }
                }
                
                int avg = count > 0 ? sum / count : 0;
                // Color-code: if all same temp, show it, otherwise show range
                if (cMin == cMax) {
                    printf(" %3d ", avg);
                } else {
                    printf("%2d-%2d", cMin, cMax);
                }
            }
            printf("\n");
        }
        
        int totalZ = insp_gridW * insp_gridH;
        printf("\nZ=%d stats: min=%d, max=%d\n", z, zMin, zMax);
        printf("  At 0C: %d (%.1f%%), At 20C: %d (%.1f%%), Other: %d (%.1f%%)\n",
               zAt0, 100.0f * zAt0 / totalZ,
               zAt20, 100.0f * zAt20 / totalZ,
               zOther, 100.0f * zOther / totalZ);
    }
}

static void cleanup(void) {
    free(insp_gridCells);
    free(insp_waterCells);
    free(insp_fireCells);
    free(insp_smokeCells);
    free(insp_steamCells);
    free(insp_cellFlags);
    free(insp_wallMaterials);
    free(insp_floorMaterials);
    free(insp_wallNatural);
    free(insp_floorNatural);
    free(insp_wallFinish);
    free(insp_floorFinish);
    free(insp_tempCells);
    free(insp_designations);
    free(insp_items);
    free(insp_stockpiles);
    free(insp_gatherZones);
    free(insp_blueprints);
    free(insp_workshops);
    free(insp_movers);
    free(insp_jobs);
    free(insp_activeJobList);
}

// Extract basename from path (e.g., "saves/foo.bin.gz" -> "foo.bin")
static void get_basename_without_gz(const char* path, char* out, size_t outSize) {
    // Find last slash
    const char* base = strrchr(path, '/');
    base = base ? base + 1 : path;
    
    // Copy and remove .gz extension if present
    strncpy(out, base, outSize - 1);
    out[outSize - 1] = '\0';
    
    size_t len = strlen(out);
    if (len > 3 && strcmp(out + len - 3, ".gz") == 0) {
        out[len - 3] = '\0';
    }
}

// Main entry point - called from demo.c when --inspect is passed
// Returns 0 on success, 1 on error
int InspectSaveFile(int argc, char** argv) {
    // Parse arguments
    const char* filename = "saves/debug_save.bin";
    int opt_mover = -1, opt_item = -1, opt_job = -1, opt_stockpile = -1, opt_workshop = -1, opt_blueprint = -1;
    int opt_cell_x = -1, opt_cell_y = -1, opt_cell_z = -1;
    int opt_path_x1 = -1, opt_path_y1 = -1, opt_path_z1 = -1;
    int opt_path_x2 = -1, opt_path_y2 = -1, opt_path_z2 = -1;
    int opt_path_algo = PATH_ALGO_ASTAR;  // default to A* for backwards compat
    int opt_map_x = -1, opt_map_y = -1, opt_map_z = -1, opt_map_r = 10;
    bool opt_stuck = false, opt_reserved = false, opt_jobs_active = false, opt_designations = false;
    bool opt_entrances = false, opt_items = false, opt_temp = false;
    int opt_entrances_z = -1, opt_temp_z = -1;
    const char* opt_items_filter = NULL;
    
    for (int i = 2; i < argc; i++) {  // Start at 2, skip program name and --inspect
        if (strcmp(argv[i], "--mover") == 0 && i+1 < argc) opt_mover = atoi(argv[++i]);
        else if (strcmp(argv[i], "--item") == 0 && i+1 < argc) opt_item = atoi(argv[++i]);
        else if (strcmp(argv[i], "--job") == 0 && i+1 < argc) opt_job = atoi(argv[++i]);
        else if (strcmp(argv[i], "--stockpile") == 0 && i+1 < argc) opt_stockpile = atoi(argv[++i]);
        else if (strcmp(argv[i], "--workshop") == 0 && i+1 < argc) opt_workshop = atoi(argv[++i]);
        else if (strcmp(argv[i], "--blueprint") == 0 && i+1 < argc) opt_blueprint = atoi(argv[++i]);
        else if (strcmp(argv[i], "--cell") == 0 && i+1 < argc) {
            sscanf(argv[++i], "%d,%d,%d", &opt_cell_x, &opt_cell_y, &opt_cell_z);
        }
        else if (strcmp(argv[i], "--path") == 0 && i+2 < argc) {
            sscanf(argv[++i], "%d,%d,%d", &opt_path_x1, &opt_path_y1, &opt_path_z1);
            sscanf(argv[++i], "%d,%d,%d", &opt_path_x2, &opt_path_y2, &opt_path_z2);
        }
        else if (strcmp(argv[i], "--algo") == 0 && i+1 < argc) {
            i++;
            if (strcmp(argv[i], "astar") == 0 || strcmp(argv[i], "a*") == 0) opt_path_algo = PATH_ALGO_ASTAR;
            else if (strcmp(argv[i], "hpa") == 0 || strcmp(argv[i], "hpa*") == 0) opt_path_algo = PATH_ALGO_HPA;
            else if (strcmp(argv[i], "jps") == 0) opt_path_algo = PATH_ALGO_JPS;
            else if (strcmp(argv[i], "jps+") == 0) opt_path_algo = PATH_ALGO_JPS_PLUS;
            else printf("Warning: unknown algorithm '%s', using A*\n", argv[i]);
        }
        else if (strcmp(argv[i], "--map") == 0 && i+1 < argc) {
            sscanf(argv[++i], "%d,%d,%d", &opt_map_x, &opt_map_y, &opt_map_z);
            if (i+1 < argc && argv[i+1][0] != '-') opt_map_r = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--designations") == 0) opt_designations = true;
        else if (strcmp(argv[i], "--stuck") == 0) opt_stuck = true;
        else if (strcmp(argv[i], "--reserved") == 0) opt_reserved = true;
        else if (strcmp(argv[i], "--jobs-active") == 0) opt_jobs_active = true;
        else if (strcmp(argv[i], "--entrances") == 0) {
            opt_entrances = true;
            if (i+1 < argc && argv[i+1][0] != '-') opt_entrances_z = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--items") == 0) {
            opt_items = true;
            if (i+1 < argc && argv[i+1][0] != '-') opt_items_filter = argv[++i];
        }
        else if (strcmp(argv[i], "--temp") == 0) {
            opt_temp = true;
            if (i+1 < argc && argv[i+1][0] != '-') opt_temp_z = atoi(argv[++i]);
        }
        else if (argv[i][0] != '-') filename = argv[i];
    }
    
    // Handle .gz files by decompressing to /tmp
    char tempFile[512] = {0};
    const char* actualFile = filename;
    size_t len = strlen(filename);
    if (len > 3 && strcmp(filename + len - 3, ".gz") == 0) {
        char baseName[256];
        get_basename_without_gz(filename, baseName, sizeof(baseName));
        snprintf(tempFile, sizeof(tempFile), "/tmp/%s", baseName);
        
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "gunzip -c '%s' > '%s'", filename, tempFile);
        if (system(cmd) != 0) {
            printf("Error: Failed to decompress %s\n", filename);
            return 1;
        }
        actualFile = tempFile;
        printf("(decompressed to %s)\n", tempFile);
    }
    
    FILE* f = fopen(actualFile, "rb");
    if (!f) {
        printf("Error: Can't open %s\n", filename);
        return 1;
    }
    
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    // Header
    uint32_t magic, version;
    fread(&magic, 4, 1, f);
    fread(&version, 4, 1, f);
    if (magic != INSPECT_SAVE_MAGIC) {
        printf("Invalid save file (bad magic)\n");
        fclose(f);
        return 1;
    }
    if (version != CURRENT_SAVE_VERSION) {
        printf("ERROR: Save version mismatch (file: v%d, supported: v%d only)\n", version, CURRENT_SAVE_VERSION);
        fclose(f);
        return 1;
    }
    
    // World seed
    fread(&insp_worldSeed, sizeof(insp_worldSeed), 1, f);
    
    // Dimensions
    fread(&insp_gridW, 4, 1, f);
    fread(&insp_gridH, 4, 1, f);
    fread(&insp_gridD, 4, 1, f);
    fread(&insp_chunkW, 4, 1, f);
    fread(&insp_chunkH, 4, 1, f);
    
    int totalCells = insp_gridW * insp_gridH * insp_gridD;
    
    // === GRIDS SECTION ===
    uint32_t marker;
    fread(&marker, 4, 1, f);
    if (marker != MARKER_GRIDS) {
        printf("Bad GRID marker: 0x%08X (expected 0x%08X)\n", marker, MARKER_GRIDS);
        fclose(f);
        return 1;
    }
    
    // Allocate and read grid data
    insp_gridCells = malloc(totalCells * sizeof(CellType));
    insp_waterCells = malloc(totalCells * sizeof(WaterCell));
    insp_fireCells = malloc(totalCells * sizeof(FireCell));
    insp_smokeCells = malloc(totalCells * sizeof(SmokeCell));
    insp_steamCells = malloc(totalCells * sizeof(SteamCell));
    insp_cellFlags = malloc(totalCells * sizeof(uint8_t));
    insp_wallMaterials = malloc(totalCells * sizeof(uint8_t));
    insp_floorMaterials = malloc(totalCells * sizeof(uint8_t));
    insp_wallNatural = malloc(totalCells * sizeof(uint8_t));
    insp_floorNatural = malloc(totalCells * sizeof(uint8_t));
    insp_wallFinish = malloc(totalCells * sizeof(uint8_t));
    insp_floorFinish = malloc(totalCells * sizeof(uint8_t));
    insp_tempCells = malloc(totalCells * sizeof(TempCell));
    insp_designations = malloc(totalCells * sizeof(Designation));
    
    fread(insp_gridCells, sizeof(CellType), totalCells, f);
    if (version < 24) {
        // V23 and earlier had treeType and treePart grids - skip them
        fseek(f, totalCells * sizeof(uint8_t) * 2, SEEK_CUR);
    }
    fread(insp_waterCells, sizeof(WaterCell), totalCells, f);
    fread(insp_fireCells, sizeof(FireCell), totalCells, f);
    fread(insp_smokeCells, sizeof(SmokeCell), totalCells, f);
    fread(insp_steamCells, sizeof(SteamCell), totalCells, f);
    fread(insp_cellFlags, sizeof(uint8_t), totalCells, f);
    fread(insp_wallMaterials, sizeof(uint8_t), totalCells, f);
    fread(insp_floorMaterials, sizeof(uint8_t), totalCells, f);
    fread(insp_wallNatural, sizeof(uint8_t), totalCells, f);
    fread(insp_floorNatural, sizeof(uint8_t), totalCells, f);

    fread(insp_wallFinish, sizeof(uint8_t), totalCells, f);
    fread(insp_floorFinish, sizeof(uint8_t), totalCells, f);
    fread(insp_tempCells, sizeof(TempCell), totalCells, f);
    fread(insp_designations, sizeof(Designation), totalCells, f);
    
    // === ENTITIES SECTION ===
    fread(&marker, 4, 1, f);
    if (marker != MARKER_ENTITIES) {
        printf("Bad ENTI marker: 0x%08X (expected 0x%08X)\n", marker, MARKER_ENTITIES);
        fclose(f);
        return 1;
    }
    
    // Items
    fread(&insp_itemHWM, 4, 1, f);
    insp_items = malloc(insp_itemHWM > 0 ? insp_itemHWM * sizeof(Item) : sizeof(Item));
    if (insp_itemHWM > 0) {
        fread(insp_items, sizeof(Item), insp_itemHWM, f);
    }
    
    // Stockpiles
    insp_stockpiles = malloc(MAX_STOCKPILES * sizeof(Stockpile));
    fread(insp_stockpiles, sizeof(Stockpile), MAX_STOCKPILES, f);
    
    // Gather zones
    fread(&insp_gatherZoneCount, 4, 1, f);
    insp_gatherZones = malloc(MAX_GATHER_ZONES * sizeof(GatherZone));
    fread(insp_gatherZones, sizeof(GatherZone), MAX_GATHER_ZONES, f);
    
    // Blueprints
    insp_blueprints = malloc(MAX_BLUEPRINTS * sizeof(Blueprint));
    fread(insp_blueprints, sizeof(Blueprint), MAX_BLUEPRINTS, f);
    
    // Workshops
    insp_workshops = malloc(MAX_WORKSHOPS * sizeof(Workshop));
    fread(insp_workshops, sizeof(Workshop), MAX_WORKSHOPS, f);
    
    // Movers
    fread(&insp_moverCount, 4, 1, f);
    insp_movers = malloc(insp_moverCount > 0 ? insp_moverCount * sizeof(Mover) : sizeof(Mover));
    if (insp_moverCount > 0) fread(insp_movers, sizeof(Mover), insp_moverCount, f);
    
    // Initialize canPlant for old saves (field added later)
    for (int i = 0; i < insp_moverCount; i++) {
        insp_movers[i].capabilities.canPlant = true;
    }
    
    // Jobs
    fread(&insp_jobHWM, 4, 1, f);
    fread(&insp_activeJobCnt, 4, 1, f);
    insp_jobs = malloc(insp_jobHWM > 0 ? insp_jobHWM * sizeof(Job) : sizeof(Job));
    insp_activeJobList = malloc(insp_activeJobCnt > 0 ? insp_activeJobCnt * sizeof(int) : sizeof(int));
    if (insp_jobHWM > 0) {
        fread(insp_jobs, sizeof(Job), insp_jobHWM, f);
        fseek(f, insp_jobHWM * sizeof(bool), SEEK_CUR);  // Skip jobIsActive array
    }
    if (insp_activeJobCnt > 0) fread(insp_activeJobList, sizeof(int), insp_activeJobCnt, f);
    
    fclose(f);
    
    // Print summary if no specific queries
    bool anyQuery = (opt_mover >= 0 || opt_item >= 0 || opt_job >= 0 || 
                     opt_stockpile >= 0 || opt_workshop >= 0 || opt_blueprint >= 0 ||
                     opt_cell_x >= 0 || opt_path_x1 >= 0 || opt_map_x >= 0 || 
                     opt_designations || opt_stuck || opt_reserved || opt_jobs_active ||
                     opt_entrances || opt_items || opt_temp);
    
    if (!anyQuery) {
        printf("Save file: %s (%ld bytes)\n", filename, fileSize);
        printf("World seed: %llu\n", (unsigned long long)insp_worldSeed);
        printf("Grid: %dx%dx%d, Chunks: %dx%d\n", insp_gridW, insp_gridH, insp_gridD, insp_chunkW, insp_chunkH);
        
        // Count stats
        int activeItems = 0, activeMovers = 0, activeStockpiles = 0, activeBP = 0;
        for (int i = 0; i < insp_itemHWM; i++) if (insp_items[i].active) activeItems++;
        for (int i = 0; i < insp_moverCount; i++) if (insp_movers[i].active) activeMovers++;
        for (int i = 0; i < MAX_STOCKPILES; i++) if (insp_stockpiles[i].active) activeStockpiles++;
        for (int i = 0; i < MAX_BLUEPRINTS; i++) if (insp_blueprints[i].active) activeBP++;
        int activeWorkshops = 0;
        for (int i = 0; i < MAX_WORKSHOPS; i++) if (insp_workshops[i].active) activeWorkshops++;
        
        printf("Items: %d active (of %d)\n", activeItems, insp_itemHWM);
        printf("Movers: %d active (of %d)\n", activeMovers, insp_moverCount);
        printf("Stockpiles: %d active\n", activeStockpiles);
        printf("Blueprints: %d active\n", activeBP);
        printf("Workshops: %d active\n", activeWorkshops);
        printf("Gather zones: %d\n", insp_gatherZoneCount);
        printf("Jobs: %d active (hwm %d)\n", insp_activeJobCnt, insp_jobHWM);
        
        // Temperature stats
        int tempAt0 = 0, tempAt20 = 0, tempOther = 0, tempMin = 9999, tempMax = -9999;
        int totalCells = insp_gridW * insp_gridH * insp_gridD;
        for (int z = 0; z < insp_gridD; z++) {
            for (int y = 0; y < insp_gridH; y++) {
                for (int x = 0; x < insp_gridW; x++) {
                    int idx = z * insp_gridH * insp_gridW + y * insp_gridW + x;
                    int temp = insp_tempCells[idx].current;
                    if (temp == 0) tempAt0++;
                    else if (temp == 20) tempAt20++;
                    else tempOther++;
                    if (temp < tempMin) tempMin = temp;
                    if (temp > tempMax) tempMax = temp;
                }
            }
        }
        printf("\nTemperature: min=%d, max=%d\n", tempMin, tempMax);
        printf("  At 0C: %d cells (%.1f%%)\n", tempAt0, 100.0f * tempAt0 / totalCells);
        printf("  At 20C: %d cells (%.1f%%)\n", tempAt20, 100.0f * tempAt20 / totalCells);
        printf("  Other: %d cells (%.1f%%)\n", tempOther, 100.0f * tempOther / totalCells);
        
        printf("\nOptions: --mover N, --item N, --job N, --stockpile N, --workshop N, --blueprint N\n");
        printf("         --cell X,Y,Z, --path X1,Y1,Z1 X2,Y2,Z2 [--algo astar|hpa|jps|jps+]\n");
        printf("         --map X,Y,Z [R], --designations, --stuck, --reserved, --jobs-active\n");
        printf("         --entrances [Z], --items [TYPE], --temp [Z]\n");
    }
    
    // Handle queries
    // Set up globals first if any query needs walkability/pathfinding
    if (opt_cell_x >= 0 || opt_path_x1 >= 0 || opt_map_x >= 0) {
        setup_pathfinding_globals();
    }
    
    if (opt_mover >= 0) print_mover(opt_mover);
    if (opt_item >= 0) print_item(opt_item);
    if (opt_job >= 0) print_job(opt_job);
    if (opt_stockpile >= 0) print_stockpile(opt_stockpile);
    if (opt_workshop >= 0) print_workshop(opt_workshop);
    if (opt_blueprint >= 0) print_blueprint(opt_blueprint);
    if (opt_cell_x >= 0) print_cell(opt_cell_x, opt_cell_y, opt_cell_z);
    if (opt_path_x1 >= 0) print_path(opt_path_x1, opt_path_y1, opt_path_z1, opt_path_x2, opt_path_y2, opt_path_z2, opt_path_algo);
    if (opt_map_x >= 0) print_map(opt_map_x, opt_map_y, opt_map_z, opt_map_r);
    if (opt_designations) print_designations();
    if (opt_stuck) print_stuck_movers();
    if (opt_reserved) print_reserved_items();
    if (opt_jobs_active) print_active_jobs();
    if (opt_entrances) print_entrances(opt_entrances_z);
    if (opt_items) print_items(opt_items_filter);
    if (opt_temp) print_temp(opt_temp_z);
    
    cleanup();
    return 0;
}
