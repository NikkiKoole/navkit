#ifndef CONSTRUCTION_H
#define CONSTRUCTION_H

#include <stdbool.h>
#include "../entities/items.h"
#include "material.h"

// Limits
#define MAX_CONSTRUCTION_STAGES 3
#define MAX_INPUTS_PER_STAGE 3
#define MAX_ALTERNATIVES 5

// Build category — what gets placed when construction completes
typedef enum {
    BUILD_WALL,
    BUILD_FLOOR,
    BUILD_LADDER,
    BUILD_RAMP,
    BUILD_CATEGORY_COUNT,
} BuildCategory;

// A single alternative item that can satisfy an input slot
typedef struct {
    ItemType itemType;   // e.g. ITEM_ROCK, or ITEM_NONE to end list
} InputAlternative;

// One input slot within a construction stage
typedef struct {
    InputAlternative alternatives[MAX_ALTERNATIVES];
    int altCount;        // how many alternatives (1 = no choice)
    int count;           // how many items needed
    bool anyBuildingMat; // if true, accept any IF_BUILDING_MAT item
} ConstructionInput;

// One stage of construction
typedef struct {
    ConstructionInput inputs[MAX_INPUTS_PER_STAGE];
    int inputCount;      // how many input slots this stage has
    float buildTime;     // seconds to build this stage
} ConstructionStage;

// Static recipe definition
typedef struct {
    const char* name;
    BuildCategory buildCategory;
    int stageCount;
    ConstructionStage stages[MAX_CONSTRUCTION_STAGES];
    MaterialType resultMaterial;   // MAT_NONE = inherited from items
    int materialFromStage;         // which stage provides final material (-1 if fixed)
    int materialFromSlot;          // which input slot within that stage (-1 if fixed)
} ConstructionRecipe;

// Recipe indices (enum for type-safe indexing)
typedef enum {
    CONSTRUCTION_DRY_STONE_WALL,
    CONSTRUCTION_WATTLE_DAUB_WALL,  // 2 stages: frame (sticks+cordage) → fill (dirt)
    CONSTRUCTION_PLANK_WALL,        // 2 stages: frame (sticks+cordage) → clad (planks)
    // Future: CONSTRUCTION_LOG_WALL, CONSTRUCTION_BRICK_WALL, etc.
    CONSTRUCTION_RECIPE_COUNT,
} ConstructionRecipeIndex;

// Recipe table (defined in construction.c)
extern const ConstructionRecipe constructionRecipes[CONSTRUCTION_RECIPE_COUNT];

// Per-slot delivery tracking (runtime, stored in Blueprint)
typedef struct {
    int deliveredCount;           // how many delivered so far
    int reservedCount;            // how many currently reserved (in transit)
    MaterialType deliveredMaterial; // material of delivered items (for inheritance)
    int chosenAlternative;        // which alternative was picked (-1 = not yet chosen)
} StageDelivery;

// Per-slot consumed item record (for cancel refund)
typedef struct {
    ItemType itemType;
    int count;
    MaterialType material;
} ConsumedRecord;

// Query helpers
const ConstructionRecipe* GetConstructionRecipe(int recipeIndex);
bool ConstructionInputAcceptsItem(const ConstructionInput* input, ItemType type);
int GetConstructionRecipeCountForCategory(BuildCategory cat);
int GetConstructionRecipeIndicesForCategory(BuildCategory cat, int* outIndices, int maxOut);

#endif // CONSTRUCTION_H
