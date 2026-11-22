#ifndef FURNITURE_H
#define FURNITURE_H

#include <stdbool.h>
#include <stdint.h>
#include "../world/grid.h"

#define MAX_FURNITURE 512

// Capability flags for furniture/stations (bitmask)
typedef enum {
    CAP_NONE          = 0,
    CAP_PREP_SURFACE  = 1 << 0,  // counter, table — needed for bread, sandwich
    CAP_WATER_SOURCE  = 1 << 1,  // sink, water barrel
} CapabilityFlag;

typedef enum {
    FURNITURE_NONE = 0,
    FURNITURE_LEAF_PILE,
    FURNITURE_GRASS_PILE,
    FURNITURE_PLANK_BED,
    FURNITURE_CHAIR,
    FURNITURE_TOILET,
    FURNITURE_TABLE,       // append only: saves store the type number
    FURNITURE_TYPE_COUNT,
} FurnitureType;

typedef struct {
    const char* name;
    float restRate;       // Rest quality as a fraction of a bed (1.0 = bed: sleepHoursInBed); 0 = no rest
    bool blocking;        // true = CELL_FLAG_WORKSHOP_BLOCK, false = movement penalty
    int moveCost;         // GetCellMoveCost value when non-blocking (0 = no penalty)
    uint8_t capabilities; // CapabilityFlag bitmask (0 = no capabilities)
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
extern uint8_t* furnitureMoveCostGrid;
// FURNITURE_MOVE_COST(x, y, z) lives in grid.h (cell_defs.h needs it before this header)
extern uint8_t* capabilityGrid;
#define CAPABILITY(x, y, z) (capabilityGrid CELL_AT((x), (y), (z)))

float FurnitureRecoveryPerGH(int furnitureType);  // energy per game hour while resting on it
const FurnitureDef* GetFurnitureDef(FurnitureType type);
void ClearFurniture(void);
int SpawnFurniture(int x, int y, int z, FurnitureType type, uint8_t material);
void RemoveFurniture(int index);
int GetFurnitureAt(int x, int y, int z);
void ReleaseFurniture(int furnitureIdx, int moverIdx);
void ReleaseFurnitureForMover(int moverIdx);
void RebuildFurnitureMoveCostGrid(void);
void RebuildCapabilityGrid(void);   // from furniture + workshops (after a load)

// Capability queries — stations/furniture provide capabilities to nearby workshops
bool HasNearbyCapability(int x, int y, int z, uint8_t capBits, int radius);
void SetCapability(int x, int y, int z, uint8_t capBits);
void ClearCapability(int x, int y, int z);

#endif // FURNITURE_H
