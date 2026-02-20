#ifndef WORKSHOPS_H
#define WORKSHOPS_H

#include <stdbool.h>
#include "items.h"
#include "../world/material.h"

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
    WT_FUEL  = 'F',   // fuel input tile (walkable)
} WorkshopTileType;

// Workshop types
typedef enum {
    WORKSHOP_STONECUTTER,
    WORKSHOP_SAWMILL,
    WORKSHOP_KILN,
    WORKSHOP_CHARCOAL_PIT,
    WORKSHOP_HEARTH,
    WORKSHOP_DRYING_RACK,
    WORKSHOP_ROPE_MAKER,
    WORKSHOP_CARPENTER,
    WORKSHOP_CAMPFIRE,
    WORKSHOP_TYPE_COUNT,
} WorkshopType;

// Workshop visual state (for UI diagnostics)
typedef enum {
    WORKSHOP_VISUAL_WORKING,
    WORKSHOP_VISUAL_OUTPUT_FULL,
    WORKSHOP_VISUAL_INPUT_EMPTY,
    WORKSHOP_VISUAL_NO_WORKER,
} WorkshopVisualState;

// Recipe: defines what a workshop can make
typedef enum {
    MAT_MATCH_ANY = 0,
    MAT_MATCH_EXACT,
    MAT_MATCH_WOOD,
    MAT_MATCH_STONE,
    MAT_MATCH_METAL,
} MaterialMatchType;

// Item matching for recipe inputs (e.g., "any fuel" recipes)
typedef enum {
    ITEM_MATCH_EXACT = 0,   // Match specific inputType
    ITEM_MATCH_ANY_FUEL,    // Match any item with IF_FUEL flag
} ItemMatchType;

typedef struct {
    const char* name;
    ItemType inputType;
    int inputCount;
    ItemType inputType2;         // second input type (ITEM_NONE = no second input)
    int inputCount2;             // count of second input
    ItemType outputType;
    int outputCount;
    ItemType outputType2;        // second output type (ITEM_NONE = no second output)
    int outputCount2;            // count of second output
    float workRequired;           // seconds of active crafter work (0 = no crafter needed)
    float passiveWorkRequired;    // seconds of passive timer work (0 = no passive phase)
    MaterialMatchType inputMaterialMatch;
    MaterialType inputMaterial;  // used when inputMaterialMatch == MAT_MATCH_EXACT
    int fuelRequired;            // number of fuel items consumed (0 = no fuel needed)
    ItemMatchType inputItemMatch; // how to match input (ITEM_MATCH_EXACT or ITEM_MATCH_ANY_FUEL)
} Recipe;

// Workshop definition (consolidates template, recipes, and metadata)
typedef struct {
    WorkshopType type;
    const char* name;           // Uppercase for inspect (e.g., "STONECUTTER")
    const char* displayName;    // Title case for UI (e.g., "Stonecutter")
    int width;                  // Template width
    int height;                 // Template height
    const char* template;
    const Recipe* recipes;
    int recipeCount;
    bool passive;           // Auto-converts items without crafter (timer-based)
} WorkshopDef;

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
    bool suspendedNoStorage;  // auto-suspended due to no stockpile space (auto-resumes when space available)
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
    
    // Passive workshop state
    float passiveProgress;  // 0.0..1.0 fraction of work done
    int passiveBillIdx;     // which bill is being processed (-1 = none)
    bool passiveReady;      // true = active phase done, passive timer may proceed

    // Diagnostics
    WorkshopVisualState visualState;
    float inputStarvationTime;
    float outputBlockedTime;
    float lastWorkTime;
    
    // Tile positions (absolute)
    int workTileX, workTileY;    // where crafter stands to work
    int outputTileX, outputTileY; // where finished items spawn
    int fuelTileX, fuelTileY;    // fuel input tile (-1,-1 if no fuel tile)
    
    // Linked stockpiles (optional, for future)
    int linkedInputStockpiles[MAX_LINKED_STOCKPILES];
    int linkedInputCount;
} Workshop;

extern Workshop workshops[MAX_WORKSHOPS];
extern int workshopCount;

// Workshop definitions (templates + recipes per type)
extern const WorkshopDef workshopDefs[WORKSHOP_TYPE_COUNT];

// Recipe data (referenced by workshopDefs)
extern Recipe stonecutterRecipes[];
extern int stonecutterRecipeCount;
extern Recipe sawmillRecipes[];
extern int sawmillRecipeCount;
extern Recipe kilnRecipes[];
extern int kilnRecipeCount;
extern Recipe charcoalPitRecipes[];
extern int charcoalPitRecipeCount;
extern Recipe hearthRecipes[];
extern int hearthRecipeCount;
extern Recipe dryingRackRecipes[];
extern int dryingRackRecipeCount;
extern Recipe ropeMakerRecipes[];
extern int ropeMakerRecipeCount;
extern Recipe campfireRecipes[];
extern int campfireRecipeCount;

// Core functions
void ClearWorkshops(void);
int CreateWorkshop(int x, int y, int z, WorkshopType type);
void DeleteWorkshop(int index);
void UpdateWorkshopDiagnostics(float dt);
void PassiveWorkshopsTick(float dt);

// Recipe lookup
const Recipe* GetRecipesForWorkshop(WorkshopType type, int* outCount);
bool RecipeInputMatches(const Recipe* recipe, const Item* item);

// Bill management
int AddBill(int workshopIdx, int recipeIdx, BillMode mode, int targetCount);
void RemoveBill(int workshopIdx, int billIdx);
void SuspendBill(int workshopIdx, int billIdx, bool suspended);
bool ShouldBillRun(Workshop* ws, Bill* bill);
int CountItemsInStockpiles(ItemType type);

// Fuel queries
bool WorkshopHasFuelForRecipe(Workshop* ws, int searchRadius);
int FindNearestFuelItem(Workshop* ws, int searchRadius);

// Queries
int FindWorkshopAt(int tileX, int tileY, int z);
bool IsWorkshopTile(int tileX, int tileY, int z);
char GetWorkshopTileAt(int wsIdx, int tileX, int tileY);  // returns template char at world pos
bool IsWorkshopBlocking(int tileX, int tileY, int z);     // true if any workshop blocks this tile
bool IsPassiveWorkshopWorkTile(int tileX, int tileY, int z); // true if tile is work tile of an active passive workshop with a runnable bill

// Stockpile linking
bool LinkStockpileToWorkshop(int workshopIdx, int stockpileIdx);
bool UnlinkStockpile(int workshopIdx, int stockpileIdx);
void UnlinkStockpileSlot(int workshopIdx, int slotIdx);
void ClearLinkedStockpiles(int workshopIdx);
bool IsStockpileLinked(int workshopIdx, int stockpileIdx);

#endif
