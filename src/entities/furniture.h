#ifndef FURNITURE_H
#define FURNITURE_H

#include <stdbool.h>
#include <stdint.h>
#include "../world/grid.h"

#define MAX_FURNITURE 4096   // pieces at once; loops run to furnitureHighWater, not here

// Capability flags for furniture/stations (bitmask)
typedef enum {
    CAP_NONE          = 0,
    CAP_PREP_SURFACE  = 1 << 0,  // counter, table — needed for bread, sandwich
    CAP_WATER_SOURCE  = 1 << 1,  // sink, water barrel
    CAP_HEAT          = 1 << 2,  // a heat provider stands here; its level is on its def (HeatLevelAt)
    // dreamengine's building kinds (docs/todo/architecture/capability-based-tasks.md): on/off
    // marks, "is there one here or nearby?". A level, where one matters, comes from the def
    // (restRate for seating and sleeping), as heat does.
    CAP_WASHING       = 1 << 3,  // sink, bath, shower: wash hands, wash up, wash
    CAP_TOILET        = 1 << 4,  // toilet
    CAP_SEATING       = 1 << 5,  // chair, sofa, bench
    CAP_FLAT_SURFACE  = 1 << 6,  // table, desk: eat or work at it (a prep surface is a counter's)
    CAP_SLEEPING      = 1 << 7,  // bed, double bed, leaf pile
    CAP_COLD_STORAGE  = 1 << 8,  // fridge: food kept on it stays cold
    CAP_DISPLAY       = 1 << 9,  // shelf, rack: things stand on show
    CAP_SELLING       = 1 << 10, // till: a shop's service point (no money yet)
} CapabilityFlag;
typedef uint16_t CapabilityBits;   // a CapabilityFlag set (capabilityGrid cell, def field)

typedef enum {
    FURNITURE_NONE = 0,
    FURNITURE_LEAF_PILE,
    FURNITURE_GRASS_PILE,
    FURNITURE_PLANK_BED,
    FURNITURE_CHAIR,
    FURNITURE_TOILET,
    FURNITURE_TABLE,       // append only: saves store the type number
    FURNITURE_TAP,         // clean water without end (no plumbing yet)
    FURNITURE_HEAT_GRATE,  // city: warm air from below, walk on it (heat 1)
    FURNITURE_FIRE_BARREL, // city: a burning oil drum, stand next to it (heat 1 while it has fuel)
    // dreamengine's buildings (names match its NKP_FURN_*)
    FURNITURE_SINK,        // washing (bathroom, kitchen)
    FURNITURE_DOUBLE_BED,  // a bed for two sleepers (FurnitureDef.sleepers = 2)
    FURNITURE_FRIDGE,      // cold storage
    FURNITURE_DESK,        // flat surface + seating spot in front
    FURNITURE_SHELF,       // display (books, goods)
    FURNITURE_SOFA,        // seating, makes a living room
    FURNITURE_TILL,        // a shop's counter: selling (City 1999's shop counter / snack stand)
    FURNITURE_BATH,        // washing; a bath or shower makes a bathroom (a toilet alone: a WC)
    FURNITURE_SHOWER,      // washing
    FURNITURE_PIT_LATRINE, // survival: a seat over a dug pit (toilet comfort 1; dirties the floor around)
    FURNITURE_BOTTLE_MACHINE, // City 1999: hand in empty cans for their deposit, paid from the nearest shop's till (money.h)
    FURNITURE_TYPE_COUNT,
} FurnitureType;

typedef struct {
    const char* name;
    float restRate;       // Rest quality as a fraction of a bed (1.0 = bed: sleepHoursInBed); 0 = no rest
    bool blocking;        // true = CELL_FLAG_WORKSHOP_BLOCK, false = movement penalty
    int moveCost;         // GetCellMoveCost value when non-blocking (0 = no penalty)
    CapabilityBits capabilities; // CapabilityFlag bitmask (0 = no capabilities)
    uint8_t heatLevel;    // CAP_HEAT level: 1 warm (grate, barrel), 2 cooking, 3 smelting
    int16_t heatTemp;     // Celsius it keeps its cell at in temperature.c (0 = none)
    uint8_t sleepers;     // movers that rest on it at once (0 = 1); a double bed: 2
    uint8_t comfort;      // CAP_TOILET: 1 pit latrine, 2 toilet (higher is preferred; 0 = 2)
    float fuelHours;      // burns fuel: game hours a full load lasts (0 = needs none: a grate)
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
extern int furnitureHighWater;   // one past the highest slot in use: loop to this, not MAX_FURNITURE
extern uint8_t* furnitureMoveCostGrid;
// FURNITURE_MOVE_COST(x, y, z) lives in grid.h (cell_defs.h needs it before this header)
extern CapabilityBits* capabilityGrid;
#define CAPABILITY(x, y, z) (capabilityGrid CELL_AT((x), (y), (z)))

float FurnitureRecoveryPerGH(int furnitureType);  // energy per game hour while resting on it
const FurnitureDef* GetFurnitureDef(FurnitureType type);
void ClearFurniture(void);
int SpawnFurniture(int x, int y, int z, FurnitureType type, uint8_t material);
void RemoveFurniture(int index);
int GetFurnitureAt(int x, int y, int z);
void ReleaseFurniture(int furnitureIdx, int moverIdx);
// Resting on furniture that takes more than one (a double bed): `occupant` holds the first
// mover, the others are the movers resting or on their way there (needTarget). Not saved
// apart from occupant, so it needs no save change.
bool FurnitureHasRoom(int furnitureIdx);                    // another mover may rest here
void ReserveFurniture(int furnitureIdx, int moverIdx);
bool FurnitureReservedBy(int furnitureIdx, int moverIdx);
void ReleaseFurnitureForMover(int moverIdx);
void RebuildFurnitureMoveCostGrid(void);
void RebuildCapabilityGrid(void);   // from furniture + workshops (after a load)

// Heat (docs/todo/architecture/capability-based-tasks.md "Smallest proof"). A capability
// with levels: the grid marks CAP_HEAT where a provider stands, the level comes from the
// provider's def (furniture heatLevel, WorkshopHeatLevel while the workshop burns).
int WorkshopHeatLevel(int workshopType);   // 0 = gives no heat
int HeatLevelAt(int x, int y, int z);      // level a provider on this cell gives (0 = none)
// A cell a cold mover can stand on to warm up: a heat provider on it or next to it, or the
// world itself is warm there (air at warmSpotTemp or more: a heated room, warm air around a
// grate). Not a cell that is burning.
extern int warmSpotTemp;
bool IsWarmSpot(int x, int y, int z);
// Food on a cell with CAP_COLD_STORAGE (a fridge) spoils at this share of the normal rate
extern float fridgeSpoilageFactor;
void FurnitureHeatTick(void);

// Fuel (a burning barrel): furnitureFuel[i] is game hours of burning left. It burns down,
// gives heat and draws a gathering only while lit; movers top it up (JOBTYPE_REFUEL).
extern float furnitureFuel[MAX_FURNITURE];
bool FurnitureLit(int fi);                   // gives its heat now (no fuel needed, or some left)
bool FurnitureNeedsFuel(int fi);             // burns fuel and is under half full
float FuelHoursOfItem(int itemType);         // what tossing one in adds (log 4 h, sticks 2 h, other fuel 1 h)
void FurnitureAddFuel(int fi, int itemType);
void FurnitureFuelTick(void);                // burn down by gameDeltaTime (sim tick)
// Radiant heat: what a mover on or next to a heat provider feels (Celsius, 0 = none),
// whatever the air does (snow, wind): 30 + 8 x level. Body temperature heads for it.
int FeltHeatAt(int x, int y, int z);   // providers with heatTemp keep their cell warm (temperature.c)

// Capability queries — stations/furniture provide capabilities to nearby workshops
bool HasNearbyCapability(int x, int y, int z, CapabilityBits capBits, int radius);
void SetCapability(int x, int y, int z, CapabilityBits capBits);
void ClearCapability(int x, int y, int z);

#endif // FURNITURE_H
