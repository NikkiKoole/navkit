#ifndef BIOME_H
#define BIOME_H

#include "../../vendor/raylib.h"
#include "material.h"

typedef struct {
    const char* name;
    const char* description;

    // Climate
    int baseSurfaceTemp;
    int seasonalAmplitude;
    int diurnalAmplitude;

    // Terrain shape: 0=flat, 1=rolling, 2=hilly, 3=mountainous
    int heightVariation;

    // Soil weights (normalized at generation time)
    float soilDirt;
    float soilClay;
    float soilSand;
    float soilGravel;
    float soilPeat;

    // Underground stone
    MaterialType stoneType;

    // Tree species weights (normalized at generation time)
    float treeOak;
    float treePine;
    float treeBirch;
    float treeWillow;
    float treeDensity;   // Multiplier on base placement chance (1.0 = normal)

    // Vegetation
    float grassDensity;  // 0.0-1.0, fraction of eligible cells that get grass
    Color grassTint;     // Color multiplier on grass sprites

    // Water
    int riverCount;
    int lakeCount;

    // Density multipliers
    float bushDensity;
    float wildCropDensity;
    float boulderDensity;
} BiomePreset;

#define BIOME_COUNT 6

extern const BiomePreset biomePresets[BIOME_COUNT];
extern const char* biomeNames[BIOME_COUNT];
extern int selectedBiome;

#endif // BIOME_H
