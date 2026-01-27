// Save file inspector - compiled with the game binary
// Usage: ./bin/path --inspect [filename] [options]

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../world/grid.h"
#include "../simulation/water.h"
#include "../simulation/fire.h"
#include "../simulation/smoke.h"
#include "../simulation/temperature.h"
#include "../entities/items.h"
#include "../entities/stockpiles.h"
#include "../world/designations.h"
#include "../entities/mover.h"
#include "../entities/jobs.h"

#define INSPECT_SAVE_VERSION 3
#define INSPECT_SAVE_MAGIC 0x4E41564B

static const char* cellTypeNames[] = {"AIR", "WALKABLE", "WALL", "FLOOR", "LADDER_UP", "LADDER_DOWN", "LADDER_BOTH"};
static const char* itemTypeNames[] = {"RED", "GREEN", "BLUE", "ORANGE"};
static const char* itemStateNames[] = {"ON_GROUND", "CARRIED", "IN_STOCKPILE"};
static const char* jobTypeNames[] = {"NONE", "HAUL", "DIG", "BUILD", "CLEAR", "HAUL_TO_BP"};

// Loaded data (separate from game globals so we don't corrupt game state)
static int insp_gridW, insp_gridH, insp_gridD, insp_chunkW, insp_chunkH;
static CellType* insp_gridCells = NULL;
static WaterCell* insp_waterCells = NULL;
static FireCell* insp_fireCells = NULL;
static SmokeCell* insp_smokeCells = NULL;
static uint8_t* insp_cellFlags = NULL;
static TempCell* insp_tempCells = NULL;
static Designation* insp_designations = NULL;
static int insp_itemHWM = 0;
static Item* insp_items = NULL;
static Stockpile* insp_stockpiles = NULL;
static int insp_gatherZoneCount = 0;
static GatherZone* insp_gatherZones = NULL;
static Blueprint* insp_blueprints = NULL;
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
        printf("  Type: %s\n", job->type < 6 ? jobTypeNames[job->type] : "?");
        printf("  Step: %d\n", job->step);
        printf("  Progress: %.2f\n", job->progress);
        if (job->targetItem >= 0) printf("  Target item: %d\n", job->targetItem);
        if (job->carryingItem >= 0) printf("  Carrying item: %d\n", job->carryingItem);
        if (job->targetStockpile >= 0) printf("  Target stockpile: %d slot (%d,%d)\n", 
            job->targetStockpile, job->targetSlotX, job->targetSlotY);
        if (job->targetDigX >= 0) printf("  Target dig: (%d,%d,z%d)\n", 
            job->targetDigX, job->targetDigY, job->targetDigZ);
        if (job->targetBlueprint >= 0) printf("  Target blueprint: %d\n", job->targetBlueprint);
    }
    
    printf("Capabilities: haul=%d mine=%d build=%d\n",
           m->capabilities.canHaul, m->capabilities.canMine, m->capabilities.canBuild);
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
    printf("Type: %s\n", item->type < 4 ? itemTypeNames[item->type] : "?");
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
    printf("Type: %s\n", job->type < 6 ? jobTypeNames[job->type] : "?");
    printf("Assigned mover: %d\n", job->assignedMover);
    printf("Step: %d\n", job->step);
    printf("Progress: %.2f\n", job->progress);
    printf("Target item: %d\n", job->targetItem);
    printf("Carrying item: %d\n", job->carryingItem);
    printf("Target stockpile: %d\n", job->targetStockpile);
    printf("Target slot: (%d, %d)\n", job->targetSlotX, job->targetSlotY);
    printf("Target dig: (%d, %d, z%d)\n", job->targetDigX, job->targetDigY, job->targetDigZ);
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
    for (int t = 0; t < 4; t++) {
        if (sp->allowedTypes[t]) printf("%s ", itemTypeNames[t]);
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
            if (sp->reservedBy[i] >= 0) {
                printf("  Slot (%d,%d) reserved by mover %d\n", x, y, sp->reservedBy[i]);
                found++;
            }
        }
    }
    if (!found) printf("  (none)\n");
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
    TempCell temp = insp_tempCells[idx];
    Designation desig = insp_designations[idx];
    
    printf("\n=== CELL (%d, %d, z%d) ===\n", x, y, z);
    printf("Type: %s\n", cell < 7 ? cellTypeNames[cell] : "?");
    
    // Water
    printf("Water level: %d/7\n", water.level);
    if (water.isSource) printf("  IS SOURCE\n");
    if (water.isDrain) printf("  IS DRAIN\n");
    if (water.hasPressure) printf("  Has pressure from z%d\n", water.pressureSourceZ);
    
    // Fire
    if (fire.level > 0) {
        printf("Fire level: %d/7\n", fire.level);
        if (fire.isSource) printf("  IS SOURCE\n");
    }
    
    // Smoke
    if (smoke.level > 0) {
        printf("Smoke level: %d/7\n", smoke.level);
    }
    
    // Temperature
    printf("Temperature: %d C", temp.current);
    if (temp.isHeatSource) printf(" [HEAT SOURCE: %d C]", temp.sourceTemp);
    if (temp.isColdSource) printf(" [COLD SOURCE: %d C]", temp.sourceTemp);
    printf("\n");
    
    if (desig.type != DESIGNATION_NONE) {
        printf("Designation: DIG, assigned to mover %d, progress %.0f%%\n",
               desig.assignedMover, desig.progress * 100);
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
            printf("  Item %d: %s (%s)\n", i, 
                   itemTypeNames[insp_items[i].type], itemStateNames[insp_items[i].state]);
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
                printf("  Job: %s step=%d\n", jobTypeNames[j->type], j->step);
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
            printf("Item %d (%s at %.0f,%.0f): reserved by mover %d\n",
                   i, itemTypeNames[insp_items[i].type],
                   insp_items[i].x, insp_items[i].y, insp_items[i].reservedBy);
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
               jid, jobTypeNames[j->type], j->assignedMover, j->step);
        if (j->targetItem >= 0) printf(" item=%d", j->targetItem);
        if (j->carryingItem >= 0) printf(" carrying=%d", j->carryingItem);
        printf("\n");
    }
    if (insp_activeJobCnt == 0) printf("No active jobs.\n");
}

static void cleanup(void) {
    free(insp_gridCells);
    free(insp_waterCells);
    free(insp_fireCells);
    free(insp_smokeCells);
    free(insp_cellFlags);
    free(insp_tempCells);
    free(insp_designations);
    free(insp_items);
    free(insp_stockpiles);
    free(insp_gatherZones);
    free(insp_blueprints);
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
    int opt_mover = -1, opt_item = -1, opt_job = -1, opt_stockpile = -1;
    int opt_cell_x = -1, opt_cell_y = -1, opt_cell_z = -1;
    bool opt_stuck = false, opt_reserved = false, opt_jobs_active = false;
    
    for (int i = 2; i < argc; i++) {  // Start at 2, skip program name and --inspect
        if (strcmp(argv[i], "--mover") == 0 && i+1 < argc) opt_mover = atoi(argv[++i]);
        else if (strcmp(argv[i], "--item") == 0 && i+1 < argc) opt_item = atoi(argv[++i]);
        else if (strcmp(argv[i], "--job") == 0 && i+1 < argc) opt_job = atoi(argv[++i]);
        else if (strcmp(argv[i], "--stockpile") == 0 && i+1 < argc) opt_stockpile = atoi(argv[++i]);
        else if (strcmp(argv[i], "--cell") == 0 && i+1 < argc) {
            sscanf(argv[++i], "%d,%d,%d", &opt_cell_x, &opt_cell_y, &opt_cell_z);
        }
        else if (strcmp(argv[i], "--stuck") == 0) opt_stuck = true;
        else if (strcmp(argv[i], "--reserved") == 0) opt_reserved = true;
        else if (strcmp(argv[i], "--jobs-active") == 0) opt_jobs_active = true;
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
    if (version != INSPECT_SAVE_VERSION) {
        printf("Version mismatch: file=%d, current=%d\n", version, INSPECT_SAVE_VERSION);
        fclose(f);
        return 1;
    }
    
    // Dimensions
    fread(&insp_gridW, 4, 1, f);
    fread(&insp_gridH, 4, 1, f);
    fread(&insp_gridD, 4, 1, f);
    fread(&insp_chunkW, 4, 1, f);
    fread(&insp_chunkH, 4, 1, f);
    
    int totalCells = insp_gridW * insp_gridH * insp_gridD;
    
    // Allocate and read grid data
    insp_gridCells = malloc(totalCells * sizeof(CellType));
    insp_waterCells = malloc(totalCells * sizeof(WaterCell));
    insp_fireCells = malloc(totalCells * sizeof(FireCell));
    insp_smokeCells = malloc(totalCells * sizeof(SmokeCell));
    insp_cellFlags = malloc(totalCells * sizeof(uint8_t));
    insp_tempCells = malloc(totalCells * sizeof(TempCell));
    insp_designations = malloc(totalCells * sizeof(Designation));
    
    fread(insp_gridCells, sizeof(CellType), totalCells, f);
    fread(insp_waterCells, sizeof(WaterCell), totalCells, f);
    fread(insp_fireCells, sizeof(FireCell), totalCells, f);
    fread(insp_smokeCells, sizeof(SmokeCell), totalCells, f);
    fread(insp_cellFlags, sizeof(uint8_t), totalCells, f);
    fread(insp_tempCells, sizeof(TempCell), totalCells, f);
    fread(insp_designations, sizeof(Designation), totalCells, f);
    
    // Items
    fread(&insp_itemHWM, 4, 1, f);
    insp_items = malloc(insp_itemHWM > 0 ? insp_itemHWM * sizeof(Item) : sizeof(Item));
    if (insp_itemHWM > 0) fread(insp_items, sizeof(Item), insp_itemHWM, f);
    
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
    
    // Movers
    fread(&insp_moverCount, 4, 1, f);
    insp_movers = malloc(insp_moverCount > 0 ? insp_moverCount * sizeof(Mover) : sizeof(Mover));
    if (insp_moverCount > 0) fread(insp_movers, sizeof(Mover), insp_moverCount, f);
    
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
                     opt_stockpile >= 0 || opt_cell_x >= 0 || opt_stuck || 
                     opt_reserved || opt_jobs_active);
    
    if (!anyQuery) {
        printf("Save file: %s (%ld bytes)\n", filename, fileSize);
        printf("Grid: %dx%dx%d, Chunks: %dx%d\n", insp_gridW, insp_gridH, insp_gridD, insp_chunkW, insp_chunkH);
        
        // Count stats
        int activeItems = 0, activeMovers = 0, activeStockpiles = 0, activeBP = 0;
        for (int i = 0; i < insp_itemHWM; i++) if (insp_items[i].active) activeItems++;
        for (int i = 0; i < insp_moverCount; i++) if (insp_movers[i].active) activeMovers++;
        for (int i = 0; i < MAX_STOCKPILES; i++) if (insp_stockpiles[i].active) activeStockpiles++;
        for (int i = 0; i < MAX_BLUEPRINTS; i++) if (insp_blueprints[i].active) activeBP++;
        
        printf("Items: %d active (of %d)\n", activeItems, insp_itemHWM);
        printf("Movers: %d active (of %d)\n", activeMovers, insp_moverCount);
        printf("Stockpiles: %d active\n", activeStockpiles);
        printf("Blueprints: %d active\n", activeBP);
        printf("Gather zones: %d\n", insp_gatherZoneCount);
        printf("Jobs: %d active (hwm %d)\n", insp_activeJobCnt, insp_jobHWM);
        printf("\nOptions: --mover N, --item N, --job N, --stockpile N, --cell X,Y,Z\n");
        printf("         --stuck, --reserved, --jobs-active\n");
    }
    
    // Handle queries
    if (opt_mover >= 0) print_mover(opt_mover);
    if (opt_item >= 0) print_item(opt_item);
    if (opt_job >= 0) print_job(opt_job);
    if (opt_stockpile >= 0) print_stockpile(opt_stockpile);
    if (opt_cell_x >= 0) print_cell(opt_cell_x, opt_cell_y, opt_cell_z);
    if (opt_stuck) print_stuck_movers();
    if (opt_reserved) print_reserved_items();
    if (opt_jobs_active) print_active_jobs();
    
    cleanup();
    return 0;
}
