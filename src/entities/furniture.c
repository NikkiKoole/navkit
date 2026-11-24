#include "furniture.h"
#include "mover.h"
#include "workshops.h"
#include "../world/pathfinding.h"
#include "../simulation/rooms.h"
#include "../simulation/balance.h"
#include "../simulation/temperature.h"
#include "../simulation/fire.h"
#include "../simulation/lighting.h"
#include <string.h>

Furniture furniture[MAX_FURNITURE];
int furnitureCount = 0;
uint8_t* furnitureMoveCostGrid = NULL;   // allocated in grid_layers.c
CapabilityBits* capabilityGrid = NULL;   // allocated in grid_layers.c

static const FurnitureDef furnitureDefs[FURNITURE_TYPE_COUNT] = {
    [FURNITURE_NONE]       = { "None",       0.0f,    false, 0,  CAP_NONE },
    [FURNITURE_LEAF_PILE]  = { "Leaf Pile",  0.7f,   false, 12, CAP_SLEEPING },
    [FURNITURE_GRASS_PILE] = { "Grass Pile", 0.7f,   false, 12, CAP_SLEEPING },
    [FURNITURE_PLANK_BED]  = { "Plank Bed",  1.0f,   false, 12, CAP_SLEEPING },
    [FURNITURE_CHAIR]      = { "Chair",      0.375f, false, 11, CAP_SEATING },
    [FURNITURE_TOILET]     = { "Toilet",     0.0f,    false, 11, CAP_TOILET },
    // Not blocking (a blocking cell would drop out of its room), but costly to cross
    [FURNITURE_TABLE]      = { "Table",      0.0f,    false, 16, CAP_PREP_SURFACE | CAP_FLAT_SURFACE },
    [FURNITURE_TAP]        = { "Tap",        0.0f,    false, 11, CAP_WATER_SOURCE },
    // City heat: a grate is walked on (no cost), a barrel is stood next to
    [FURNITURE_HEAT_GRATE] = { "Heating Grate", 0.0f, false, 0,  CAP_HEAT, 1, 40 },
    [FURNITURE_FIRE_BARREL]= { "Burning Barrel", 0.0f, true, 0,  CAP_HEAT, 1, 150 },
    // dreamengine's buildings. Not blocking for the same reason as the table.
    [FURNITURE_SINK]       = { "Sink",       0.0f,    false, 16, CAP_WASHING | CAP_WATER_SOURCE },
    [FURNITURE_DOUBLE_BED] = { "Double Bed", 1.0f,    false, 12, CAP_SLEEPING, 0, 0, 2 },
    [FURNITURE_FRIDGE]     = { "Fridge",     0.0f,    false, 20, CAP_COLD_STORAGE },
    [FURNITURE_DESK]       = { "Desk",       0.0f,    false, 16, CAP_FLAT_SURFACE },
    [FURNITURE_SHELF]      = { "Shelf",      0.0f,    false, 20, CAP_DISPLAY },
    [FURNITURE_SOFA]       = { "Sofa",       0.5f,    false, 11, CAP_SEATING },
    [FURNITURE_TILL]       = { "Till",       0.0f,    false, 16, CAP_SELLING },
};

_Static_assert(sizeof(furnitureDefs) / sizeof(furnitureDefs[0]) == FURNITURE_TYPE_COUNT,
               "furnitureDefs[] must have an entry for every FurnitureType");

float FurnitureRecoveryPerGH(int furnitureType) {
    return GetFurnitureDef((FurnitureType)furnitureType)->restRate * balance.bedRecoveryPerGH;
}

const FurnitureDef* GetFurnitureDef(FurnitureType type) {
    if (type < 0 || type >= FURNITURE_TYPE_COUNT) return &furnitureDefs[FURNITURE_NONE];
    return &furnitureDefs[type];
}

void ClearFurniture(void) {
    memset(furniture, 0, sizeof(furniture));
    for (int i = 0; i < MAX_FURNITURE; i++) {
        furniture[i].occupant = -1;
    }
    furnitureCount = 0;
    CLEAR_GRID_LAYER(furnitureMoveCostGrid, 0);
    CLEAR_GRID_LAYER(capabilityGrid, 0);
}

int SpawnFurniture(int x, int y, int z, FurnitureType type, uint8_t material) {
    if (type <= FURNITURE_NONE || type >= FURNITURE_TYPE_COUNT) return -1;
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return -1;

    // Check no existing furniture at this cell
    if (GetFurnitureAt(x, y, z) >= 0) return -1;

    // Find empty slot
    int idx = -1;
    for (int i = 0; i < MAX_FURNITURE; i++) {
        if (!furniture[i].active) {
            idx = i;
            break;
        }
    }
    if (idx < 0) return -1;

    Furniture* f = &furniture[idx];
    f->x = x;
    f->y = y;
    f->z = z;
    f->active = true;
    f->type = type;
    f->material = material;
    f->occupant = -1;
    furnitureCount++;

    const FurnitureDef* def = &furnitureDefs[type];
    if (def->blocking) {
        SET_CELL_FLAG(x, y, z, CELL_FLAG_WORKSHOP_BLOCK);
        PushMoversOutOfCell(x, y, z);
        InvalidatePathsThroughCell(x, y, z);
        MarkChunkDirty(x, y, z);
    } else if (def->moveCost > 0) {
        FURNITURE_MOVE_COST(x, y, z) = (uint8_t)def->moveCost;
        InvalidatePathsThroughCell(x, y, z);
        MarkChunkDirty(x, y, z);
    }

    if (def->capabilities) {
        CAPABILITY(x, y, z) |= def->capabilities;
    }

    InvalidateRooms();
    return idx;
}

void RemoveFurniture(int index) {
    if (index < 0 || index >= MAX_FURNITURE) return;
    Furniture* f = &furniture[index];
    if (!f->active) return;

    const FurnitureDef* def = &furnitureDefs[f->type];
    if (def->blocking) {
        CLEAR_CELL_FLAG(f->x, f->y, f->z, CELL_FLAG_WORKSHOP_BLOCK);
        InvalidatePathsThroughCell(f->x, f->y, f->z);
        MarkChunkDirty(f->x, f->y, f->z);
    } else if (def->moveCost > 0) {
        FURNITURE_MOVE_COST(f->x, f->y, f->z) = 0;
        InvalidatePathsThroughCell(f->x, f->y, f->z);
        MarkChunkDirty(f->x, f->y, f->z);
    }

    // Clear furniture capability bits (one furniture per cell, so just clear)
    if (def->capabilities) {
        CAPABILITY(f->x, f->y, f->z) &= ~def->capabilities;
    }

    if (f->type == FURNITURE_FIRE_BARREL) RemoveLightSource(f->x, f->y, f->z);
    if (def->heatTemp > 0) DestabilizeTemperature(f->x, f->y, f->z);
    f->active = false;
    f->occupant = -1;
    furnitureCount--;
    InvalidateRooms();
}

int GetFurnitureAt(int x, int y, int z) {
    for (int i = 0; i < MAX_FURNITURE; i++) {
        if (!furniture[i].active) continue;
        if (furniture[i].x == x && furniture[i].y == y && furniture[i].z == z) {
            return i;
        }
    }
    return -1;
}

static int FurnitureCapacity(int fi) {
    int n = furnitureDefs[furniture[fi].type].sleepers;
    return n > 1 ? n : 1;
}

static bool RestingOn(int moverIdx, int fi) {
    const Mover* m = &movers[moverIdx];
    return m->active && m->needTarget == fi &&
           (m->freetimeState == FREETIME_SEEKING_REST || m->freetimeState == FREETIME_RESTING);
}

bool FurnitureHasRoom(int fi) {
    if (fi < 0 || fi >= MAX_FURNITURE || !furniture[fi].active) return false;
    if (furniture[fi].occupant < 0) return true;
    int cap = FurnitureCapacity(fi);
    if (cap < 2) return false;
    int n = 0;
    for (int i = 0; i < moverCount; i++) if (RestingOn(i, fi)) n++;
    return n < cap;
}

void ReserveFurniture(int fi, int moverIdx) {
    if (fi < 0 || fi >= MAX_FURNITURE) return;
    if (furniture[fi].occupant < 0) furniture[fi].occupant = moverIdx;
}

bool FurnitureReservedBy(int fi, int moverIdx) {
    if (fi < 0 || fi >= MAX_FURNITURE || !furniture[fi].active) return false;
    if (furniture[fi].occupant == moverIdx) return true;
    return furniture[fi].occupant >= 0 && FurnitureCapacity(fi) > 1 && movers[moverIdx].needTarget == fi;
}

void ReleaseFurniture(int furnitureIdx, int moverIdx) {
    if (furnitureIdx >= 0 && furnitureIdx < MAX_FURNITURE &&
        furniture[furnitureIdx].occupant == moverIdx) {
        furniture[furnitureIdx].occupant = -1;
        // A double bed: whoever else is in it holds it now
        if (FurnitureCapacity(furnitureIdx) > 1)
            for (int i = 0; i < moverCount; i++)
                if (i != moverIdx && RestingOn(i, furnitureIdx)) { furniture[furnitureIdx].occupant = i; break; }
    }
}

void ReleaseFurnitureForMover(int moverIdx) {
    for (int i = 0; i < MAX_FURNITURE; i++) {
        if (furniture[i].active && furniture[i].occupant == moverIdx) {
            furniture[i].occupant = -1;
        }
    }
}

void RebuildFurnitureMoveCostGrid(void) {
    CLEAR_GRID_LAYER(furnitureMoveCostGrid, 0);
    for (int i = 0; i < MAX_FURNITURE; i++) {
        if (!furniture[i].active) continue;
        const FurnitureDef* def = &furnitureDefs[furniture[i].type];
        if (!def->blocking && def->moveCost > 0) {
            FURNITURE_MOVE_COST(furniture[i].x, furniture[i].y, furniture[i].z) = (uint8_t)def->moveCost;
        }
    }
}

// --- Capability grid ---

// The capability grid is not saved: rebuild it from what provides capabilities
// (furniture on its cell, workshops on their work tile), e.g. after a load.
void RebuildCapabilityGrid(void) {
    if (!capabilityGrid) return;
    CLEAR_GRID_LAYER(capabilityGrid, 0);
    for (int i = 0; i < MAX_FURNITURE; i++) {
        if (!furniture[i].active) continue;
        CapabilityBits caps = furnitureDefs[furniture[i].type].capabilities;
        if (caps) SetCapability(furniture[i].x, furniture[i].y, furniture[i].z, caps);
    }
    for (int w = 0; w < MAX_WORKSHOPS; w++) {
        if (!workshops[w].active) continue;
        CapabilityBits caps = workshopDefs[workshops[w].type].capabilities;
        if (caps) SetCapability(workshops[w].workTileX, workshops[w].workTileY, workshops[w].z, caps);
        if (WorkshopHeatLevel(workshops[w].type) > 0 && workshops[w].fuelTileX >= 0)
            SetCapability(workshops[w].fuelTileX, workshops[w].fuelTileY, workshops[w].z, CAP_HEAT);
    }
}

// --- Heat ---

int warmSpotTemp = 28;
float fridgeSpoilageFactor = 0.25f;   // setting fridgeSpoil

int WorkshopHeatLevel(int workshopType) {
    switch (workshopType) {
        case WORKSHOP_CAMPFIRE: case WORKSHOP_GROUND_FIRE: case WORKSHOP_CHARCOAL_PIT: return 1;
        case WORKSHOP_HEARTH: case WORKSHOP_STOVE: return 2;
        case WORKSHOP_KILN: return 3;
        default: return 0;
    }
}

// A workshop burns while its passive timer runs (fuel lit, recipe going)
static bool WorkshopBurning(const Workshop* ws) {
    return ws->active && ws->fuelTileX >= 0 && ws->passiveReady &&
           ws->passiveProgress > 0.0f && ws->passiveProgress < 1.0f;
}

int HeatLevelAt(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return 0;
    if (!(CAPABILITY(x, y, z) & CAP_HEAT)) return 0;
    int level = 0;
    int fi = GetFurnitureAt(x, y, z);
    if (fi >= 0) level = furnitureDefs[furniture[fi].type].heatLevel;
    // A fire workshop's fuel tile: gives heat only while it burns
    for (int w = 0; w < MAX_WORKSHOPS; w++) {
        const Workshop* ws = &workshops[w];
        if (ws->fuelTileX != x || ws->fuelTileY != y || ws->z != z) continue;
        if (!WorkshopBurning(ws)) continue;
        int l = WorkshopHeatLevel(ws->type);
        if (l > level) level = l;
    }
    return level;
}

bool IsWarmSpot(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return false;
    if (FIRE(x, y, z).level > 0) return false;
    // On the cell: only a provider you stand on (a grate), not a workshop's fire
    if (CAPABILITY(x, y, z) & CAP_HEAT) {
        int fi = GetFurnitureAt(x, y, z);
        if (fi >= 0 && furnitureDefs[furniture[fi].type].heatLevel > 0) return true;
    }
    static const int dirs[4][2] = {{0,-1},{0,1},{-1,0},{1,0}};
    for (int d = 0; d < 4; d++) {
        if (HeatLevelAt(x + dirs[d][0], y + dirs[d][1], z) > 0) return true;
    }
    return temperatureEnabled && GetTemperature(x, y, z) >= warmSpotTemp;
}

int FeltHeatAt(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return 0;
    static const int dirs[5][2] = {{0,0},{0,-1},{0,1},{-1,0},{1,0}};
    int level = 0;
    for (int d = 0; d < 5; d++) {
        int l = HeatLevelAt(x + dirs[d][0], y + dirs[d][1], z);
        if (l > level) level = l;
    }
    return level > 0 ? 30 + 8 * level : 0;
}

void FurnitureHeatTick(void) {
    if (!temperatureEnabled || furnitureCount == 0) return;
    for (int i = 0; i < MAX_FURNITURE; i++) {
        if (!furniture[i].active) continue;
        int t = furnitureDefs[furniture[i].type].heatTemp;
        if (t <= 0) continue;
        const Furniture* f = &furniture[i];
        // Only raises (like ApplyFireHeat): a hotter fire nearby stays
        if (t > GetTemperature(f->x, f->y, f->z)) SetTemperature(f->x, f->y, f->z, t);
    }
}

void SetCapability(int x, int y, int z, CapabilityBits capBits) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return;
    CAPABILITY(x, y, z) |= capBits;
}

void ClearCapability(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return;
    CAPABILITY(x, y, z) = 0;
}

bool HasNearbyCapability(int x, int y, int z, CapabilityBits capBits, int radius) {
    if (!capBits) return true;  // no requirement = always satisfied
    int x0 = x - radius, x1 = x + radius;
    int y0 = y - radius, y1 = y + radius;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= gridWidth) x1 = gridWidth - 1;
    if (y1 >= gridHeight) y1 = gridHeight - 1;
    if (z < 0 || z >= gridDepth) return false;
    for (int cy = y0; cy <= y1; cy++) {
        for (int cx = x0; cx <= x1; cx++) {
            if ((CAPABILITY(cx, cy, z) & capBits) == capBits) return true;
        }
    }
    return false;
}
