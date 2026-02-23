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
    [CONSTRUCTION_LOG_WALL] = {
        .name = "Log Wall",
        .buildCategory = BUILD_WALL,
        .stageCount = 1,
        .stages = {{
            .inputs = {{
                .alternatives = {{ .itemType = ITEM_LOG }},
                .altCount = 1,
                .count = 2,
                .anyBuildingMat = false,
            }},
            .inputCount = 1,
            .buildTime = 3.0f,
        }},
        .resultMaterial = MAT_NONE,    // inherited from log material (oak, pine, etc.)
        .materialFromStage = 0,
        .materialFromSlot = 0,
    },
    [CONSTRUCTION_BRICK_WALL] = {
        .name = "Brick Wall",
        .buildCategory = BUILD_WALL,
        .stageCount = 1,
        .stages = {{
            .inputs = {{
                .alternatives = {{ .itemType = ITEM_BRICKS }},
                .altCount = 1,
                .count = 3,
                .anyBuildingMat = false,
            }},
            .inputCount = 1,
            .buildTime = 4.0f,
        }},
        .resultMaterial = MAT_BRICK,   // fixed material
        .materialFromStage = -1,
        .materialFromSlot = -1,
    },
    [CONSTRUCTION_PLANK_FLOOR] = {
        .name = "Plank Floor",
        .buildCategory = BUILD_FLOOR,
        .stageCount = 1,
        .stages = {{
            .inputs = {{
                .alternatives = {{ .itemType = ITEM_PLANKS }},
                .altCount = 1,
                .count = 2,
                .anyBuildingMat = false,
            }},
            .inputCount = 1,
            .buildTime = 2.0f,
        }},
        .resultMaterial = MAT_NONE,    // inherited from plank material
        .materialFromStage = 0,
        .materialFromSlot = 0,
    },
    [CONSTRUCTION_BRICK_FLOOR] = {
        .name = "Brick Floor",
        .buildCategory = BUILD_FLOOR,
        .stageCount = 1,
        .stages = {{
            .inputs = {{
                .alternatives = {{ .itemType = ITEM_BRICKS }},
                .altCount = 1,
                .count = 2,
                .anyBuildingMat = false,
            }},
            .inputCount = 1,
            .buildTime = 3.0f,
        }},
        .resultMaterial = MAT_BRICK,   // fixed material
        .materialFromStage = -1,
        .materialFromSlot = -1,
    },
    [CONSTRUCTION_THATCH_FLOOR] = {
        .name = "Thatch Floor",
        .buildCategory = BUILD_FLOOR,
        .stageCount = 2,
        .stages = {
            {   // Stage 0: Base layer
                .inputs = {{
                    .alternatives = {
                        { .itemType = ITEM_DIRT },
                        { .itemType = ITEM_GRAVEL },
                        { .itemType = ITEM_SAND },
                    },
                    .altCount = 3,
                    .count = 1,
                    .anyBuildingMat = false,
                }},
                .inputCount = 1,
                .buildTime = 1.0f,
            },
            {   // Stage 1: Thatch layer
                .inputs = {{
                    .alternatives = {{ .itemType = ITEM_DRIED_GRASS }},
                    .altCount = 1,
                    .count = 1,
                    .anyBuildingMat = false,
                }},
                .inputCount = 1,
                .buildTime = 2.0f,
            },
        },
        .resultMaterial = MAT_DIRT,    // fixed material
        .materialFromStage = -1,
        .materialFromSlot = -1,
    },
    [CONSTRUCTION_LADDER] = {
        .name = "Ladder",
        .buildCategory = BUILD_LADDER,
        .stageCount = 1,
        .stages = {{
            .inputs = {{
                .alternatives = {
                    { .itemType = ITEM_LOG },
                    { .itemType = ITEM_PLANKS },
                },
                .altCount = 2,
                .count = 1,
                .anyBuildingMat = false,
            }},
            .inputCount = 1,
            .buildTime = 2.0f,
        }},
        .resultMaterial = MAT_NONE,    // inherited from log/plank material
        .materialFromStage = 0,
        .materialFromSlot = 0,
    },
    [CONSTRUCTION_LEAF_PILE] = {
        .name = "Leaf Pile",
        .buildCategory = BUILD_FURNITURE,
        .stageCount = 1,
        .stages = {{
            .inputs = {{
                .alternatives = {{ .itemType = ITEM_LEAVES }},
                .altCount = 1,
                .count = 4,
                .anyBuildingMat = false,
            }},
            .inputCount = 1,
            .buildTime = 2.0f,
        }},
        .resultMaterial = MAT_NONE,    // inherited from leaves material
        .materialFromStage = 0,
        .materialFromSlot = 0,
    },
    [CONSTRUCTION_GRASS_PILE] = {
        .name = "Grass Pile",
        .buildCategory = BUILD_FURNITURE,
        .stageCount = 1,
        .stages = {{
            .inputs = {{
                .alternatives = {{ .itemType = ITEM_GRASS }},
                .altCount = 1,
                .count = 10,
                .anyBuildingMat = false,
            }},
            .inputCount = 1,
            .buildTime = 2.0f,
        }},
        .resultMaterial = MAT_NONE,
        .materialFromStage = 0,
        .materialFromSlot = 0,
    },
    [CONSTRUCTION_PLANK_BED] = {
        .name = "Plank Bed",
        .buildCategory = BUILD_FURNITURE,
        .stageCount = 1,
        .stages = {{
            .inputs = {{
                .alternatives = {{ .itemType = ITEM_PLANK_BED }},
                .altCount = 1,
                .count = 1,
                .anyBuildingMat = false,
            }},
            .inputCount = 1,
            .buildTime = 3.0f,
        }},
        .resultMaterial = MAT_NONE,    // inherited from bed item material
        .materialFromStage = 0,
        .materialFromSlot = 0,
    },
    [CONSTRUCTION_CHAIR] = {
        .name = "Chair",
        .buildCategory = BUILD_FURNITURE,
        .stageCount = 1,
        .stages = {{
            .inputs = {{
                .alternatives = {{ .itemType = ITEM_CHAIR }},
                .altCount = 1,
                .count = 1,
                .anyBuildingMat = false,
            }},
            .inputCount = 1,
            .buildTime = 2.0f,
        }},
        .resultMaterial = MAT_NONE,    // inherited from chair item material
        .materialFromStage = 0,
        .materialFromSlot = 0,
    },
    // =========================================================================
    // Workshop construction recipes
    // =========================================================================
    [CONSTRUCTION_WORKSHOP_CAMPFIRE] = {
        .name = "Fire Pit",
        .buildCategory = BUILD_WORKSHOP,
        .stageCount = 1,
        .stages = {{
            .inputs = {
                {
                    .alternatives = {{ .itemType = ITEM_STICKS }},
                    .altCount = 1,
                    .count = 5,
                    .anyBuildingMat = false,
                },
                {
                    .alternatives = {{ .itemType = ITEM_ROCK }},
                    .altCount = 1,
                    .count = 3,
                    .anyBuildingMat = false,
                },
            },
            .inputCount = 2,
            .buildTime = 2.0f,
        }},
        .resultMaterial = MAT_NONE,
        .materialFromStage = -1,
        .materialFromSlot = -1,
    },
    [CONSTRUCTION_WORKSHOP_DRYING_RACK] = {
        .name = "Drying Rack",
        .buildCategory = BUILD_WORKSHOP,
        .stageCount = 1,
        .stages = {{
            .inputs = {{
                .alternatives = {{ .itemType = ITEM_STICKS }},
                .altCount = 1,
                .count = 4,
                .anyBuildingMat = false,
            }},
            .inputCount = 1,
            .buildTime = 2.0f,
        }},
        .resultMaterial = MAT_NONE,
        .materialFromStage = -1,
        .materialFromSlot = -1,
    },
    [CONSTRUCTION_WORKSHOP_ROPE_MAKER] = {
        .name = "Rope Maker",
        .buildCategory = BUILD_WORKSHOP,
        .stageCount = 1,
        .stages = {{
            .inputs = {{
                .alternatives = {{ .itemType = ITEM_STICKS }},
                .altCount = 1,
                .count = 4,
                .anyBuildingMat = false,
            }},
            .inputCount = 1,
            .buildTime = 2.0f,
        }},
        .resultMaterial = MAT_NONE,
        .materialFromStage = -1,
        .materialFromSlot = -1,
    },
    [CONSTRUCTION_WORKSHOP_CHARCOAL_PIT] = {
        .name = "Charcoal Pit",
        .buildCategory = BUILD_WORKSHOP,
        .stageCount = 1,
        .stages = {{
            .inputs = {{
                .alternatives = {{ .itemType = ITEM_LOG }},
                .altCount = 1,
                .count = 2,
                .anyBuildingMat = false,
            }},
            .inputCount = 1,
            .buildTime = 3.0f,
        }},
        .resultMaterial = MAT_NONE,
        .materialFromStage = -1,
        .materialFromSlot = -1,
    },
    [CONSTRUCTION_WORKSHOP_HEARTH] = {
        .name = "Hearth",
        .buildCategory = BUILD_WORKSHOP,
        .stageCount = 1,
        .stages = {{
            .inputs = {{
                .alternatives = {{ .itemType = ITEM_ROCK }},
                .altCount = 1,
                .count = 5,
                .anyBuildingMat = false,
            }},
            .inputCount = 1,
            .buildTime = 4.0f,
        }},
        .resultMaterial = MAT_NONE,
        .materialFromStage = -1,
        .materialFromSlot = -1,
    },
    [CONSTRUCTION_WORKSHOP_STONECUTTER] = {
        .name = "Stonecutter",
        .buildCategory = BUILD_WORKSHOP,
        .stageCount = 1,
        .stages = {{
            .inputs = {
                {
                    .alternatives = {{ .itemType = ITEM_ROCK }},
                    .altCount = 1,
                    .count = 5,
                    .anyBuildingMat = false,
                },
                {
                    .alternatives = {{ .itemType = ITEM_LOG }},
                    .altCount = 1,
                    .count = 2,
                    .anyBuildingMat = false,
                },
            },
            .inputCount = 2,
            .buildTime = 5.0f,
        }},
        .resultMaterial = MAT_NONE,
        .materialFromStage = -1,
        .materialFromSlot = -1,
    },
    [CONSTRUCTION_WORKSHOP_SAWMILL] = {
        .name = "Sawmill",
        .buildCategory = BUILD_WORKSHOP,
        .stageCount = 1,
        .stages = {{
            .inputs = {
                {
                    .alternatives = {{ .itemType = ITEM_LOG }},
                    .altCount = 1,
                    .count = 3,
                    .anyBuildingMat = false,
                },
                {
                    .alternatives = {{ .itemType = ITEM_CORDAGE }},
                    .altCount = 1,
                    .count = 2,
                    .anyBuildingMat = false,
                },
            },
            .inputCount = 2,
            .buildTime = 5.0f,
        }},
        .resultMaterial = MAT_NONE,
        .materialFromStage = -1,
        .materialFromSlot = -1,
    },
    [CONSTRUCTION_WORKSHOP_KILN] = {
        .name = "Kiln",
        .buildCategory = BUILD_WORKSHOP,
        .stageCount = 1,
        .stages = {{
            .inputs = {
                {
                    .alternatives = {{ .itemType = ITEM_ROCK }},
                    .altCount = 1,
                    .count = 8,
                    .anyBuildingMat = false,
                },
                {
                    .alternatives = {{ .itemType = ITEM_CLAY }},
                    .altCount = 1,
                    .count = 2,
                    .anyBuildingMat = false,
                },
            },
            .inputCount = 2,
            .buildTime = 6.0f,
        }},
        .resultMaterial = MAT_NONE,
        .materialFromStage = -1,
        .materialFromSlot = -1,
    },
    [CONSTRUCTION_WORKSHOP_CARPENTER] = {
        .name = "Carpenter",
        .buildCategory = BUILD_WORKSHOP,
        .stageCount = 1,
        .stages = {{
            .inputs = {
                {
                    .alternatives = {{ .itemType = ITEM_PLANKS }},
                    .altCount = 1,
                    .count = 4,
                    .anyBuildingMat = false,
                },
                {
                    .alternatives = {{ .itemType = ITEM_CORDAGE }},
                    .altCount = 1,
                    .count = 2,
                    .anyBuildingMat = false,
                },
            },
            .inputCount = 2,
            .buildTime = 4.0f,
        }},
        .resultMaterial = MAT_NONE,
        .materialFromStage = -1,
        .materialFromSlot = -1,
    },
    [CONSTRUCTION_WORKSHOP_GROUND_FIRE] = {
        .name = "Ground Fire",
        .buildCategory = BUILD_WORKSHOP,
        .stageCount = 1,
        .stages = {{
            .inputs = {{
                .alternatives = {{ .itemType = ITEM_STICKS }},
                .altCount = 1,
                .count = 3,
                .anyBuildingMat = false,
            }},
            .inputCount = 1,
            .buildTime = 1.0f,
        }},
        .resultMaterial = MAT_NONE,
        .materialFromStage = -1,
        .materialFromSlot = -1,
    },
    [CONSTRUCTION_WORKSHOP_BUTCHER] = {
        .name = "Butcher",
        .buildCategory = BUILD_WORKSHOP,
        .stageCount = 1,
        .stages = {{
            .inputs = {
                {
                    .alternatives = {{ .itemType = ITEM_STICKS }},
                    .altCount = 1,
                    .count = 2,
                    .anyBuildingMat = false,
                },
                {
                    .alternatives = {{ .itemType = ITEM_ROCK }},
                    .altCount = 1,
                    .count = 1,
                    .anyBuildingMat = false,
                },
            },
            .inputCount = 2,
            .buildTime = 1.0f,
        }},
        .resultMaterial = MAT_NONE,
        .materialFromStage = -1,
        .materialFromSlot = -1,
    },
    // === PRIMITIVE RECIPES ===
    [CONSTRUCTION_LEAF_WALL] = {
        .name = "Leaf Wall",
        .buildCategory = BUILD_WALL,
        .stageCount = 1,
        .stages = {{
            .inputs = {
                {
                    .alternatives = {{ .itemType = ITEM_STICKS }},
                    .altCount = 1,
                    .count = 4,
                    .anyBuildingMat = false,
                },
                {
                    .alternatives = {{ .itemType = ITEM_LEAVES }},
                    .altCount = 1,
                    .count = 4,
                    .anyBuildingMat = false,
                },
            },
            .inputCount = 2,
            .buildTime = 2.0f,
        }},
        .resultMaterial = MAT_NONE,
        .materialFromStage = 0,
        .materialFromSlot = 0,
    },
    [CONSTRUCTION_STICK_WALL] = {
        .name = "Stick Wall",
        .buildCategory = BUILD_WALL,
        .stageCount = 1,
        .stages = {{
            .inputs = {
                {
                    .alternatives = {{ .itemType = ITEM_STICKS }},
                    .altCount = 1,
                    .count = 4,
                    .anyBuildingMat = false,
                },
                {
                    .alternatives = {{ .itemType = ITEM_CORDAGE }},
                    .altCount = 1,
                    .count = 2,
                    .anyBuildingMat = false,
                },
            },
            .inputCount = 2,
            .buildTime = 2.0f,
        }},
        .resultMaterial = MAT_NONE,
        .materialFromStage = 0,
        .materialFromSlot = 0,
    },
    [CONSTRUCTION_LEAF_ROOF] = {
        .name = "Leaf Roof",
        .buildCategory = BUILD_FLOOR,
        .stageCount = 1,
        .stages = {{
            .inputs = {
                {
                    .alternatives = {{ .itemType = ITEM_POLES }},
                    .altCount = 1,
                    .count = 2,
                    .anyBuildingMat = false,
                },
                {
                    .alternatives = {{ .itemType = ITEM_LEAVES }},
                    .altCount = 1,
                    .count = 3,
                    .anyBuildingMat = false,
                },
            },
            .inputCount = 2,
            .buildTime = 2.0f,
        }},
        .resultMaterial = MAT_NONE,
        .materialFromStage = 0,
        .materialFromSlot = 0,
    },
    [CONSTRUCTION_BARK_ROOF] = {
        .name = "Bark Roof",
        .buildCategory = BUILD_FLOOR,
        .stageCount = 1,
        .stages = {{
            .inputs = {
                {
                    .alternatives = {{ .itemType = ITEM_POLES }},
                    .altCount = 1,
                    .count = 2,
                    .anyBuildingMat = false,
                },
                {
                    .alternatives = {{ .itemType = ITEM_BARK }},
                    .altCount = 1,
                    .count = 2,
                    .anyBuildingMat = false,
                },
            },
            .inputCount = 2,
            .buildTime = 2.0f,
        }},
        .resultMaterial = MAT_NONE,
        .materialFromStage = 0,
        .materialFromSlot = 0,
    },
    // === DOOR RECIPES ===
    [CONSTRUCTION_LEAF_DOOR] = {
        .name = "Leaf Door",
        .buildCategory = BUILD_DOOR,
        .stageCount = 1,
        .stages = {{
            .inputs = {
                {
                    .alternatives = {{ .itemType = ITEM_POLES }},
                    .altCount = 1,
                    .count = 2,
                    .anyBuildingMat = false,
                },
                {
                    .alternatives = {{ .itemType = ITEM_LEAVES }},
                    .altCount = 1,
                    .count = 2,
                    .anyBuildingMat = false,
                },
            },
            .inputCount = 2,
            .buildTime = 2.0f,
        }},
        .resultMaterial = MAT_NONE,
        .materialFromStage = 0,
        .materialFromSlot = 0,
    },
    [CONSTRUCTION_PLANK_DOOR] = {
        .name = "Plank Door",
        .buildCategory = BUILD_DOOR,
        .stageCount = 1,
        .stages = {{
            .inputs = {{
                .alternatives = {{ .itemType = ITEM_PLANKS }},
                .altCount = 1,
                .count = 3,
                .anyBuildingMat = false,
            }},
            .inputCount = 1,
            .buildTime = 3.0f,
        }},
        .resultMaterial = MAT_NONE,
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

// Maps WorkshopType enum → ConstructionRecipeIndex. Uses int to avoid
// circular dependency (workshops.h includes construction.h).
int GetConstructionRecipeForWorkshopType(int workshopType) {
    switch (workshopType) {
        case WORKSHOP_CAMPFIRE:     return CONSTRUCTION_WORKSHOP_CAMPFIRE;
        case WORKSHOP_DRYING_RACK:  return CONSTRUCTION_WORKSHOP_DRYING_RACK;
        case WORKSHOP_ROPE_MAKER:   return CONSTRUCTION_WORKSHOP_ROPE_MAKER;
        case WORKSHOP_CHARCOAL_PIT: return CONSTRUCTION_WORKSHOP_CHARCOAL_PIT;
        case WORKSHOP_HEARTH:       return CONSTRUCTION_WORKSHOP_HEARTH;
        case WORKSHOP_STONECUTTER:  return CONSTRUCTION_WORKSHOP_STONECUTTER;
        case WORKSHOP_SAWMILL:      return CONSTRUCTION_WORKSHOP_SAWMILL;
        case WORKSHOP_KILN:         return CONSTRUCTION_WORKSHOP_KILN;
        case WORKSHOP_CARPENTER:    return CONSTRUCTION_WORKSHOP_CARPENTER;
        case WORKSHOP_GROUND_FIRE:  return CONSTRUCTION_WORKSHOP_GROUND_FIRE;
        case WORKSHOP_BUTCHER:      return CONSTRUCTION_WORKSHOP_BUTCHER;
        default: return -1;
    }
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
