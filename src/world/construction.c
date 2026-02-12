#include "construction.h"
#include "../entities/item_defs.h"

// Static recipe table
const ConstructionRecipe constructionRecipes[CONSTRUCTION_RECIPE_COUNT] = {
    [CONSTRUCTION_DRY_STONE_WALL] = {
        .name = "Dry Stone Wall",
        .buildCategory = BUILD_WALL,
        .stageCount = 1,
        .stages = {{
            .inputs = {{
                .alternatives = {{ .itemType = ITEM_ROCK }},
                .altCount = 1,
                .count = 3,
                .anyBuildingMat = false,
            }},
            .inputCount = 1,
            .buildTime = 3.0f,
        }},
        .resultMaterial = MAT_NONE,    // inherited from delivered rocks
        .materialFromStage = 0,
        .materialFromSlot = 0,
    },
    [CONSTRUCTION_WATTLE_FRAME] = {
        .name = "Wattle Frame",
        .buildCategory = BUILD_WALL,
        .stageCount = 1,
        .stages = {{
            .inputs = {
                {   // Slot 0: sticks
                    .alternatives = {{ .itemType = ITEM_STICKS }},
                    .altCount = 1,
                    .count = 2,
                    .anyBuildingMat = false,
                },
                {   // Slot 1: cordage
                    .alternatives = {{ .itemType = ITEM_CORDAGE }},
                    .altCount = 1,
                    .count = 1,
                    .anyBuildingMat = false,
                },
            },
            .inputCount = 2,
            .buildTime = 2.0f,
        }},
        .resultMaterial = MAT_NONE,    // inherited from sticks
        .materialFromStage = 0,
        .materialFromSlot = 0,         // material from sticks (wood type)
    },
};

const ConstructionRecipe* GetConstructionRecipe(int recipeIndex) {
    if (recipeIndex < 0 || recipeIndex >= CONSTRUCTION_RECIPE_COUNT) return NULL;
    return &constructionRecipes[recipeIndex];
}

bool ConstructionInputAcceptsItem(const ConstructionInput* input, ItemType type) {
    if (input->anyBuildingMat) {
        return ItemIsBuildingMat(type);
    }
    for (int i = 0; i < input->altCount; i++) {
        if (input->alternatives[i].itemType == type) return true;
    }
    return false;
}

int GetConstructionRecipeCountForCategory(BuildCategory cat) {
    int count = 0;
    for (int i = 0; i < CONSTRUCTION_RECIPE_COUNT; i++) {
        if (constructionRecipes[i].buildCategory == cat) count++;
    }
    return count;
}

int GetConstructionRecipeIndicesForCategory(BuildCategory cat, int* outIndices, int maxOut) {
    int count = 0;
    for (int i = 0; i < CONSTRUCTION_RECIPE_COUNT && count < maxOut; i++) {
        if (constructionRecipes[i].buildCategory == cat) {
            outIndices[count++] = i;
        }
    }
    return count;
}
