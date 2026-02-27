#include "biome.h"

int selectedBiome = 0;

const BiomePreset biomePresets[BIOME_COUNT] = {
    // 0: Temperate Grassland (default — current behavior)
    {
        .name = "Temperate Grassland",
        .description = "Gentle rolling hills, mixed forests, mild climate.",
        .baseSurfaceTemp = 15, .seasonalAmplitude = 25, .diurnalAmplitude = 5,
        .heightVariation = 1,
        .soilDirt = 0.50f, .soilClay = 0.20f, .soilSand = 0.10f, .soilGravel = 0.10f, .soilPeat = 0.10f,
        .stoneType = MAT_GRANITE,
        .treeOak = 0.35f, .treePine = 0.20f, .treeBirch = 0.30f, .treeWillow = 0.15f, .treeDensity = 1.0f,
        .grassDensity = 1.0f, .grassTint = {255, 255, 255, 255},
        .riverCount = 2, .lakeCount = 2,
        .bushDensity = 1.0f, .wildCropDensity = 1.0f, .boulderDensity = 1.0f,
    },
    // 1: Arid Scrubland
    {
        .name = "Arid Scrubland",
        .description = "Hot, dry, sandy terrain with sparse vegetation.",
        .baseSurfaceTemp = 28, .seasonalAmplitude = 30, .diurnalAmplitude = 12,
        .heightVariation = 1,
        .soilDirt = 0.15f, .soilClay = 0.10f, .soilSand = 0.45f, .soilGravel = 0.25f, .soilPeat = 0.05f,
        .stoneType = MAT_SANDSTONE,
        .treeOak = 0.05f, .treePine = 0.15f, .treeBirch = 0.60f, .treeWillow = 0.20f, .treeDensity = 0.25f,
        .grassDensity = 0.3f, .grassTint = {255, 240, 180, 255},
        .riverCount = 1, .lakeCount = 0,
        .bushDensity = 0.5f, .wildCropDensity = 0.3f, .boulderDensity = 1.0f,
    },
    // 2: Boreal / Taiga
    {
        .name = "Boreal / Taiga",
        .description = "Cold pine forests on peaty soil. Long winters.",
        .baseSurfaceTemp = 2, .seasonalAmplitude = 30, .diurnalAmplitude = 4,
        .heightVariation = 1,
        .soilDirt = 0.25f, .soilClay = 0.10f, .soilSand = 0.10f, .soilGravel = 0.25f, .soilPeat = 0.30f,
        .stoneType = MAT_SLATE,
        .treeOak = 0.05f, .treePine = 0.60f, .treeBirch = 0.25f, .treeWillow = 0.10f, .treeDensity = 1.4f,
        .grassDensity = 0.6f, .grassTint = {220, 240, 210, 255},
        .riverCount = 2, .lakeCount = 3,
        .bushDensity = 0.8f, .wildCropDensity = 0.5f, .boulderDensity = 1.0f,
    },
    // 3: Wetland / Marsh
    {
        .name = "Wetland / Marsh",
        .description = "Flat, waterlogged lowlands. Willows and reeds.",
        .baseSurfaceTemp = 12, .seasonalAmplitude = 20, .diurnalAmplitude = 4,
        .heightVariation = 0,
        .soilDirt = 0.25f, .soilClay = 0.25f, .soilSand = 0.05f, .soilGravel = 0.10f, .soilPeat = 0.35f,
        .stoneType = MAT_SLATE,
        .treeOak = 0.10f, .treePine = 0.05f, .treeBirch = 0.10f, .treeWillow = 0.75f, .treeDensity = 0.8f,
        .grassDensity = 0.8f, .grassTint = {210, 245, 220, 255},
        .riverCount = 4, .lakeCount = 4,
        .bushDensity = 1.2f, .wildCropDensity = 0.8f, .boulderDensity = 0.5f,
    },
    // 4: Highland / Rocky
    {
        .name = "Highland / Rocky",
        .description = "Mountainous, thin soil, sparse pine, many boulders.",
        .baseSurfaceTemp = 8, .seasonalAmplitude = 25, .diurnalAmplitude = 8,
        .heightVariation = 3,
        .soilDirt = 0.20f, .soilClay = 0.10f, .soilSand = 0.15f, .soilGravel = 0.40f, .soilPeat = 0.15f,
        .stoneType = MAT_GRANITE,
        .treeOak = 0.10f, .treePine = 0.55f, .treeBirch = 0.25f, .treeWillow = 0.10f, .treeDensity = 0.5f,
        .grassDensity = 0.4f, .grassTint = {230, 240, 220, 255},
        .riverCount = 1, .lakeCount = 1,
        .bushDensity = 0.4f, .wildCropDensity = 0.3f, .boulderDensity = 3.0f,
    },
    // 5: Riverlands
    {
        .name = "Riverlands",
        .description = "Fertile river valley. Abundant forests and crops.",
        .baseSurfaceTemp = 16, .seasonalAmplitude = 22, .diurnalAmplitude = 5,
        .heightVariation = 0,
        .soilDirt = 0.40f, .soilClay = 0.30f, .soilSand = 0.10f, .soilGravel = 0.10f, .soilPeat = 0.10f,
        .stoneType = MAT_GRANITE,
        .treeOak = 0.35f, .treePine = 0.10f, .treeBirch = 0.20f, .treeWillow = 0.35f, .treeDensity = 1.2f,
        .grassDensity = 1.0f, .grassTint = {200, 255, 180, 255},
        .riverCount = 5, .lakeCount = 3,
        .bushDensity = 1.5f, .wildCropDensity = 2.0f, .boulderDensity = 0.3f,
    },
};

const char* biomeNames[BIOME_COUNT] = {
    "Temperate Grassland",
    "Arid Scrubland",
    "Boreal / Taiga",
    "Wetland / Marsh",
    "Highland / Rocky",
    "Riverlands",
};
