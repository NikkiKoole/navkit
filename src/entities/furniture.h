#ifndef FURNITURE_H
#define FURNITURE_H

#include <stdbool.h>
#include <stdint.h>
#include "../world/grid.h"

#define MAX_FURNITURE 512

typedef enum {
    FURNITURE_NONE = 0,
    FURNITURE_LEAF_PILE,
    FURNITURE_PLANK_BED,
    FURNITURE_CHAIR,
    FURNITURE_TYPE_COUNT,
} FurnitureType;

typedef struct {
    const char* name;
    float restRate;       // Energy recovery per second (0 = no rest)
    bool blocking;        // true = CELL_FLAG_WORKSHOP_BLOCK, false = movement penalty
    int moveCost;         // GetCellMoveCost value when non-blocking (0 = no penalty)
} FurnitureDef;

typedef struct {
    int x, y, z;
    bool active;
    FurnitureType type;
    uint8_t material;     // MaterialType (inherited from construction input)
    int occupant;         // Mover index (-1 = unoccupied)
} Furniture;

extern Furniture furniture[MAX_FURNITURE];
extern int furnitureCount;
extern uint8_t furnitureMoveCostGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

const FurnitureDef* GetFurnitureDef(FurnitureType type);
void ClearFurniture(void);
int SpawnFurniture(int x, int y, int z, FurnitureType type, uint8_t material);
void RemoveFurniture(int index);
int GetFurnitureAt(int x, int y, int z);
void ReleaseFurniture(int furnitureIdx, int moverIdx);
void ReleaseFurnitureForMover(int moverIdx);
void RebuildFurnitureMoveCostGrid(void);

#endif // FURNITURE_H
