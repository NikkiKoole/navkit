#ifndef FARMING_H
#define FARMING_H

#include <stdbool.h>
#include <stdint.h>
#include "../world/grid.h"
#include "../world/material.h"

// Farm cell state (4 bytes, all uint8_t for packing)
typedef struct {
    uint8_t fertility;       // 0-255, depletes on crop harvest (06b), restored by compost
    uint8_t weedLevel;       // 0-255, accumulates over time on tilled cells
    uint8_t tilled;          // 1 = has been tilled
    uint8_t desiredCropType; // what player wants planted here (0 = none, 06b extends)
} FarmCell;

// Farm grid (parallel to main grid, zero-initialized = no farm data)
extern FarmCell farmGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];
extern int farmActiveCells;  // count of tilled cells (for sim tick skipping)

// Fertility constants
#define FERTILITY_DEFAULT   128
#define FERTILITY_LOW       64   // Below this, auto-fertilize triggers

// Weed constants
#define WEED_THRESHOLD      128  // Above this, crop penalty 0.5x
#define WEED_SEVERE         200  // Above this, crop penalty 0.25x
#define WEED_GROWTH_PER_TICK  3  // Base weed growth per farm tick

// Farm tick interval (game-hours)
#define FARM_TICK_INTERVAL  0.5f

// Work times (game-hours)
#define TILL_WORK_TIME      1.0f
#define TEND_WORK_TIME      0.4f
#define FERTILIZE_WORK_TIME 0.3f

// Fertilize amount
#define FERTILIZE_AMOUNT    80

// Initialize / clear
void InitFarming(void);
void ClearFarming(void);

// Tick (weed accumulation, called from main loop)
void FarmTick(float dt);

// Soil check: can this cell be designated as farm?
bool IsFarmableSoil(int x, int y, int z);

// Initial fertility based on soil material
uint8_t InitialFertilityForSoil(MaterialType mat);

// Seasonal weed rate modifier
float GetSeasonalWeedRate(void);

// Accessors
FarmCell* GetFarmCell(int x, int y, int z);

#endif // FARMING_H
