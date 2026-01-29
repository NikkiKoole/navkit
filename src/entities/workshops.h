#ifndef WORKSHOPS_H
#define WORKSHOPS_H

#include <stdbool.h>
#include "items.h"

#define MAX_WORKSHOPS 256
#define MAX_BILLS_PER_WORKSHOP 10
#define MAX_LINKED_STOCKPILES 4
#define MAX_WORKSHOP_SIZE 5  // max width/height for templates

// Template tile types
typedef enum {
    WT_FLOOR = '.',   // walkable floor
    WT_BLOCK = '#',   // non-walkable machinery
    WT_WORK  = 'X',   // work tile (walkable)
    WT_OUTPUT = 'O',  // output tile (walkable)
} WorkshopTileType;

// Workshop types
typedef enum {
    WORKSHOP_STONECUTTER,
    WORKSHOP_TYPE_COUNT,
} WorkshopType;

// Recipe: defines what a workshop can make
typedef struct {
    const char* name;
    ItemType inputType;
    int inputCount;
    ItemType outputType;
    int outputCount;
    float workRequired;  // seconds to complete
} Recipe;

// Bill modes
typedef enum {
    BILL_DO_X_TIMES,
    BILL_DO_UNTIL_X,
    BILL_DO_FOREVER,
} BillMode;

// Bill: work order queued at a workshop
typedef struct {
    int recipeIdx;
    BillMode mode;
    int targetCount;     // for DO_X_TIMES: how many to make; for DO_UNTIL_X: target stockpile count
    int completedCount;  // progress for DO_X_TIMES
    int ingredientSearchRadius;  // how far to look for inputs (tiles), 0 = unlimited
    bool suspended;
} Bill;

// Workshop struct
typedef struct {
    int x, y, z;         // top-left corner
    int width, height;   // footprint (e.g., 3x3)
    bool active;
    WorkshopType type;
    
    // Layout template (local coords, row-major)
    char template[MAX_WORKSHOP_SIZE * MAX_WORKSHOP_SIZE];
    
    // Bills
    Bill bills[MAX_BILLS_PER_WORKSHOP];
    int billCount;
    
    // Work state
    int assignedCrafter;  // mover index, -1 = none
    
    // Tile positions (absolute)
    int workTileX, workTileY;    // where crafter stands to work
    int outputTileX, outputTileY; // where finished items spawn
    
    // Linked stockpiles (optional, for future)
    int linkedInputStockpiles[MAX_LINKED_STOCKPILES];
    int linkedInputCount;
} Workshop;

extern Workshop workshops[MAX_WORKSHOPS];
extern int workshopCount;

// Recipe data
extern Recipe stonecutterRecipes[];
extern int stonecutterRecipeCount;

// Core functions
void ClearWorkshops(void);
int CreateWorkshop(int x, int y, int z, WorkshopType type);
void DeleteWorkshop(int index);

// Recipe lookup
Recipe* GetRecipesForWorkshop(WorkshopType type, int* outCount);

// Bill management
int AddBill(int workshopIdx, int recipeIdx, BillMode mode, int targetCount);
void RemoveBill(int workshopIdx, int billIdx);
void SuspendBill(int workshopIdx, int billIdx, bool suspended);
bool ShouldBillRun(Workshop* ws, Bill* bill);

// Queries
int FindWorkshopAt(int tileX, int tileY, int z);
bool IsWorkshopTile(int tileX, int tileY, int z);
char GetWorkshopTileAt(int wsIdx, int tileX, int tileY);  // returns template char at world pos
bool IsWorkshopBlocking(int tileX, int tileY, int z);     // true if any workshop blocks this tile

#endif
