// plants.c - Plant entity system
// Sparse entity pool for berry bushes and future crops.
// Growth is season-modulated: summer=1.0, spring=0.3, autumn=0.5, winter=0.0

#include "plants.h"
#include "balance.h"
#include "weather.h"
#include "../entities/items.h"
#include "../entities/mover.h"  // CELL_SIZE
#include <string.h>

Plant plants[MAX_PLANTS];
int plantCount = 0;

// Growth rate: game-hours per stage transition (at summer 1.0x)
#define BERRY_BUSH_GROWTH_GH 48.0f  // Game-hours per stage at full rate

static float SeasonalGrowthRate(void) {
    Season season = GetCurrentSeason();
    switch (season) {
        case SEASON_SUMMER: return 1.0f;
        case SEASON_SPRING: return 0.3f;
        case SEASON_AUTUMN: return 0.5f;
        case SEASON_WINTER: return 0.0f;
        default: return 0.0f;
    }
}

void InitPlants(void) {
    memset(plants, 0, sizeof(plants));
    plantCount = 0;
}

void ClearPlants(void) {
    memset(plants, 0, sizeof(plants));
    plantCount = 0;
}

int SpawnPlant(int x, int y, int z, PlantType type) {
    // Find first inactive slot
    for (int i = 0; i < MAX_PLANTS; i++) {
        if (!plants[i].active) {
            plants[i].x = x;
            plants[i].y = y;
            plants[i].z = z;
            plants[i].type = type;
            plants[i].stage = PLANT_STAGE_BARE;
            plants[i].growthProgress = 0.0f;
            plants[i].active = true;
            if (i >= plantCount) plantCount = i + 1;
            return i;
        }
    }
    return -1;  // Pool full
}

void DeletePlant(int idx) {
    if (idx < 0 || idx >= MAX_PLANTS) return;
    plants[idx].active = false;
}

Plant* GetPlantAt(int x, int y, int z) {
    for (int i = 0; i < plantCount; i++) {
        if (plants[i].active && plants[i].x == x && plants[i].y == y && plants[i].z == z) {
            return &plants[i];
        }
    }
    return NULL;
}

bool IsPlantRipe(int x, int y, int z) {
    Plant* p = GetPlantAt(x, y, z);
    return p && p->stage == PLANT_STAGE_RIPE;
}

void HarvestPlant(int x, int y, int z) {
    Plant* p = GetPlantAt(x, y, z);
    if (!p || p->stage != PLANT_STAGE_RIPE) return;

    // Reset to bare
    p->stage = PLANT_STAGE_BARE;
    p->growthProgress = 0.0f;

    // Spawn berries at cell center
    float px = x * CELL_SIZE + CELL_SIZE * 0.5f;
    float py = y * CELL_SIZE + CELL_SIZE * 0.5f;
    SpawnItem(px, py, (float)z, ITEM_BERRIES);
}

void PlantsTick(float dt) {
    float rate = SeasonalGrowthRate();
    if (rate <= 0.0f) return;  // No growth in winter

    float increment = (dt * rate) / GameHoursToGameSeconds(BERRY_BUSH_GROWTH_GH);

    for (int i = 0; i < plantCount; i++) {
        Plant* p = &plants[i];
        if (!p->active) continue;
        if (p->stage == PLANT_STAGE_RIPE) continue;  // Already ripe, wait for harvest

        p->growthProgress += increment;
        if (p->growthProgress >= 1.0f) {
            p->growthProgress = 0.0f;
            if (p->stage == PLANT_STAGE_BARE) {
                p->stage = PLANT_STAGE_BUDDING;
            } else if (p->stage == PLANT_STAGE_BUDDING) {
                p->stage = PLANT_STAGE_RIPE;
            }
        }
    }
}
