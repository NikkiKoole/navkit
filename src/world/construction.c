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
                .alternatives = {
                    { .itemType = ITEM_ROCK },
                    { .itemType = ITEM_BLOCKS },
                },
                .altCount = 2,
                .count = 3,
                .anyBuildingMat = false,
            }},
            .inputCount = 1,
            .buildTime = 3.0f,
        }},
        .resultMaterial = MAT_NONE,    // inherited from delivered rocks/blocks
        .materialFromStage = 0,
        .materialFromSlot = 0,
    },
    [CONSTRUCTION_WATTLE_DAUB_WALL] = {
        .name = "Wattle & Daub Wall",
        .buildCategory = BUILD_WALL,
        .stageCount = 2,
        .stages = {
            {   // Stage 0: Frame
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
            },
            {   // Stage 1: Fill
                .inputs = {{
                    .alternatives = {
                        { .itemType = ITEM_DIRT },
                        { .itemType = ITEM_CLAY },
                    },
                    .altCount = 2,
                    .count = 2,
                    .anyBuildingMat = false,
                }},
                .inputCount = 1,
                .buildTime = 3.0f,
            },
        },
        .resultMaterial = MAT_NONE,    // inherited from fill material (dirt or clay)
        .materialFromStage = 1,        // material comes from fill stage
        .materialFromSlot = 0,
    },
    [CONSTRUCTION_PLANK_WALL] = {
        .name = "Plank Wall",
        .buildCategory = BUILD_WALL,
        .stageCount = 2,
        .stages = {
            {   // Stage 0: Frame
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
            },
            {   // Stage 1: Clad
                .inputs = {{
                    .alternatives = {{ .itemType = ITEM_PLANKS }},
                    .altCount = 1,
                    .count = 2,
                    .anyBuildingMat = false,
                }},
                .inputCount = 1,
                .buildTime = 3.0f,
            },
        },
        .resultMaterial = MAT_NONE,    // inherited from planks
        .materialFromStage = 1,        // material comes from clad stage
        .materialFromSlot = 0,
    },
    [CONSTRUCTION_RAMP] = {
        .name = "Ramp",
        .buildCategory = BUILD_RAMP,
        .stageCount = 1,
        .stages = {{
            .inputs = {{
                .alternatives = {0},   // ignored when anyBuildingMat is true
                .altCount = 0,
                .count = 1,
                .anyBuildingMat = true,
            }},
            .inputCount = 1,
            .buildTime = 2.0f,
        }},
        .resultMaterial = MAT_NONE,    // inherited from delivered item
        .materialFromStage = 0,
        .materialFromSlot = 0,
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
