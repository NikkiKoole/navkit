// pumps.c - DF-style screw pumps (see pumps.h)
#include "pumps.h"
#include "grid.h"
#include "cell_defs.h"
#include "material.h"
#include "designations.h"
#include "pathfinding.h"
#include "../simulation/water.h"
#include <string.h>

Pump pumps[MAX_PUMPS];
float pumpUnitsPerSecond = 6.0f;
float pumpWorkHours = 1.0f;

void ClearPumps(void) {
    memset(pumps, 0, sizeof(pumps));
}

int PumpAt(int x, int y, int z) {
    for (int i = 0; i < MAX_PUMPS; i++)
        if (pumps[i].active && pumps[i].x == x && pumps[i].y == y && pumps[i].z == z) return i;
    return -1;
}

int PlacePump(int x, int y, int z, int dx, int dy) {
    if (x < 0 || y < 0 || z < 1 || x >= gridWidth || y >= gridHeight || z >= gridDepth) return -1;
    if (abs(dx) + abs(dy) != 1) return -1;
    if (PumpAt(x, y, z) >= 0 || CellBlocksMovement(GRID(x, y, z))) return -1;
    int slot = -1;
    for (int i = 0; i < MAX_PUMPS && slot < 0; i++) if (!pumps[i].active) slot = i;
    if (slot < 0) return -1;
    pumps[slot] = (Pump){ x, y, z, dx, dy, true, 0.0f, 99.0f, 0 };
    // The pump block: a wooden wall cell (no water over it, nobody walks through it)
    if (GetWaterLevel(x, y, z) > 0) SetWaterLevel(x, y, z, 0);
    GRID(x, y, z) = CELL_WALL;
    SetWallMaterial(x, y, z, MAT_OAK);
    ClearWallNatural(x, y, z);
    MarkChunkDirty(x, y, z);
    InvalidatePathsThroughCell(x, y, z);
    DestabilizeWater(x, y, z - 1);
    return slot;
}

void RemovePump(int idx) {
    if (idx < 0 || idx >= MAX_PUMPS || !pumps[idx].active) return;
    Pump* p = &pumps[idx];
    if (DESIGNATION(p->x, p->y, p->z).type == DESIGNATION_WORK_PUMP) CancelDesignation(p->x, p->y, p->z);
    if (GRID(p->x, p->y, p->z) == CELL_WALL) {
        GRID(p->x, p->y, p->z) = CELL_AIR;
        SetWallMaterial(p->x, p->y, p->z, MAT_NONE);
        MarkChunkDirty(p->x, p->y, p->z);
        InvalidatePathsThroughCell(p->x, p->y, p->z);
    }
    DestabilizeWater(p->x, p->y, p->z);
    p->active = false;
}

void PumpWork(int idx, float dt) {
    if (idx < 0 || idx >= MAX_PUMPS || !pumps[idx].active) return;
    Pump* p = &pumps[idx];
    p->workedAgo = 0.0f;
    p->accum += pumpUnitsPerSecond * dt;
    int sx = p->x, sy = p->y, sz = p->z - 1, ox = p->x + p->dx, oy = p->y + p->dy, oz = p->z;
    while (p->accum >= 1.0f) {
        int from = GetWaterLevel(sx, sy, sz), to = GetWaterLevel(ox, oy, oz);
        bool room = ox >= 0 && oy >= 0 && ox < gridWidth && oy < gridHeight &&
                    CellAllowsFluids(GRID(ox, oy, oz)) && !WATER(ox, oy, oz).isFrozen;   // water.c's CanHoldWater
        if (from <= 0 || to >= WATER_MAX_LEVEL || !room) { p->accum = 1.0f; break; }
        SetWaterLevel(sx, sy, sz, from - 1);
        SetWaterLevel(ox, oy, oz, to + 1);
        p->accum -= 1.0f;
        p->unitsMoved++;
    }
}

void PumpsTick(float dt) {
    for (int i = 0; i < MAX_PUMPS; i++)
        if (pumps[i].active && pumps[i].workedAgo < 99.0f) pumps[i].workedAgo += dt;
}

bool DesignateWorkPump(int x, int y, int z) {
    if (PumpAt(x, y, z) < 0) return false;
    if (DESIGNATION(x, y, z).type != DESIGNATION_NONE) return false;
    DESIGNATION_MUT(x, y, z).type = DESIGNATION_WORK_PUMP;
    DESIGNATION_MUT(x, y, z).assignedMover = -1;
    DESIGNATION_MUT(x, y, z).progress = 0.0f;
    activeDesignationCount++;
    InvalidateDesignationCache(DESIGNATION_WORK_PUMP);
    return true;
}

void CompleteWorkPumpDesignation(int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 || x >= gridWidth || y >= gridHeight || z >= gridDepth) return;
    if (DESIGNATION(x, y, z).type != DESIGNATION_WORK_PUMP) return;
    activeDesignationCount--;
    DESIGNATION_MUT(x, y, z).type = DESIGNATION_NONE;
    DESIGNATION_MUT(x, y, z).assignedMover = -1;
    DESIGNATION_MUT(x, y, z).progress = 0.0f;
    InvalidateDesignationCache(DESIGNATION_WORK_PUMP);
}
