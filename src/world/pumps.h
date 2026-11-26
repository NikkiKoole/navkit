// pumps.h - DF-style screw pumps: lift water one level, only while someone works them.
// A pump sits on a cell over water: it takes from the cell BELOW it and puts out into the cell
// BESIDE it (its facing), one level up. The pump cell itself is a block (a wooden wall cell), so
// lifted water can't run back over it. Worked by a mover (DESIGNATION_WORK_PUMP ->
// JOBTYPE_WORK_PUMP, the door / lever chore): water moves only while the worker pumps.
// Works on water cells, so for channels, basins and pipe openings alike (docs/todo/world/
// infrastructure.md). Spike (2026-11-26): console `pump`, scenario `pumploop`; not saved yet.
#ifndef PUMPS_H
#define PUMPS_H

#include <stdbool.h>

#define MAX_PUMPS 64

typedef struct {
    int x, y, z;
    int dx, dy;          // facing: the output cell is (x + dx, y + dy, z)
    bool active;
    float accum;         // fractional units not moved yet
    float workedAgo;     // seconds since it was last worked (drawing: turning while < 0.25)
    int unitsMoved;      // total, for stats
} Pump;

extern Pump pumps[MAX_PUMPS];
extern float pumpUnitsPerSecond;   // setting pumpRate: water units per game second while worked
extern float pumpWorkHours;        // setting pumpWorkHours: how long one "work the pump" order lasts

void ClearPumps(void);
// Place a pump on (x, y, z) facing (dx, dy) (one of N/E/S/W): the cell becomes a pump block.
// Returns the index, -1 if it can't go there (off the map, no room in the table, a wall already)
int  PlacePump(int x, int y, int z, int dx, int dy);
void RemovePump(int idx);           // the block goes, the cell is air again
int  PumpAt(int x, int y, int z);
// Work the pump for dt game seconds: moves water from below to the output (as much as the rate,
// the water below and the room at the output allow)
void PumpWork(int idx, float dt);
void PumpsTick(float dt);           // drawing timers

bool DesignateWorkPump(int x, int y, int z);   // a worker will come and pump (pumpWorkHours)
void CompleteWorkPumpDesignation(int x, int y, int z);

#endif
