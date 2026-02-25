// plants.h - Plant entity system (berry bushes, future crops)
// Plants are sparse discrete entities (not grid-based) with growth stages.

#ifndef PLANTS_H
#define PLANTS_H

#include <stdbool.h>

#define MAX_PLANTS 2000

typedef enum {
    PLANT_BERRY_BUSH,
    PLANT_WILD_WHEAT,
    PLANT_WILD_LENTILS,
    PLANT_WILD_FLAX,
    PLANT_TYPE_COUNT
} PlantType;

typedef enum {
    PLANT_STAGE_BARE,      // No fruit, just planted or recently harvested
    PLANT_STAGE_BUDDING,   // Growing, not yet harvestable
    PLANT_STAGE_RIPE,      // Ready for harvest
} PlantStage;

typedef struct {
    int x, y, z;
    PlantType type;
    PlantStage stage;
    float growthProgress;  // 0→1 within current stage
    bool active;
} Plant;

extern Plant plants[MAX_PLANTS];
extern int plantCount;

void InitPlants(void);
void ClearPlants(void);
int SpawnPlant(int x, int y, int z, PlantType type);
void DeletePlant(int idx);
Plant* GetPlantAt(int x, int y, int z);
bool IsPlantRipe(int x, int y, int z);
void HarvestPlant(int x, int y, int z);  // Reset to bare, spawn items based on plant type
void PlantsTick(float dt);               // Advance growth, season-modulated

#endif
