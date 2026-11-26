// Soil moisture: the top soil holds water (docs/todo/world/soil-moisture.md).
// One byte per cell, on natural soil cells with open space above (the surface): how full the
// soil is, 0 (parched) .. 255 (soaked, can't take more). Water standing on soil soaks in (up to
// the soil's capacity, at its speed); the sun, the air and the plants take it out again; rain,
// snow melt and watering put it back. The old 2-bit wetness (GET_CELL_WETNESS: mud, fire, crops)
// is kept as a bucket of it, written whenever moisture changes.
#ifndef MOISTURE_H
#define MOISTURE_H

#include <stdbool.h>
#include <stdint.h>
#include "../world/grid.h"
#include "../world/material.h"

extern uint8_t* moistureGrid;   // allocated in grid_layers.c
#define MOISTURE(x, y, z) (moistureGrid CELL_AT((x), (y), (z)))

#define MOISTURE_FULL      255
#define MOISTURE_BASELINE  100   // "normal" ground: what a new world starts with
#define MOISTURE_SOAKED    230   // bucket bounds (also the labels)
#define MOISTURE_WET       180
#define MOISTURE_MOIST     128
#define MOISTURE_NORMAL     64
#define MOISTURE_DRY        26   // below: parched
#define MOISTURE_RAIN_STEP  40   // one step of rain or snow melt

typedef enum { SOIL_PARCHED, SOIL_DRY, SOIL_NORMAL, SOIL_MOIST, SOIL_WET, SOIL_SOAKED } SoilState;

extern bool soilMoistureEnabled;   // off: water doesn't soak in, soil doesn't dry (old wetness rules gone)
extern float moistureDryHours;     // game hours for open bare dirt at 20 C to lose half of what it holds above "normal"
extern float moistureSoakSpeed;    // multiplier on every soil's soaking speed
extern float moistureSweepInterval;   // game hours between drying sweeps
extern int droughtWearPerSweep;       // wear parched dirt adds to its grass per sweep (groundwear.c thresholds)

SoilState SoilMoistureState(int m);
const char* SoilStateName(SoilState s);
// Water units (1/7 cell) a full cell of this soil holds; 0 = not a soil
float SoilCapacityUnits(MaterialType mat);
// A natural soil cell with open space above: the cells moisture lives in
bool IsSoilSurface(int x, int y, int z);
// Write moisture (clamped 0..255) and the wetness bucket bits
void SetSoilMoisture(int x, int y, int z, int m);
void AddSoilMoisture(int x, int y, int z, int dm);
// Rain, snow melt: the soil at z, else the soil just below (an air cell's ground)
void WetSoilAt(int x, int y, int z, int dm);
// At least this wet (watering a crop, irrigation)
void SoilWetAtLeast(int x, int y, int z, int m);
// A unit of rain landing on soil (x, y, z): soaks in at once if the soil has room, by its soaking
// chance (true = taken); else it lands as water
bool RainSoaksIn(int x, int y, int z);

// Water standing at (x, y, z) on soil at z-1 may soak one unit in (the water sync pass)
void SoilSoakFrom(int x, int y, int z);
extern long soilSoakedUnits;   // water units soaked into soil since start (for balance runs)
// Drying sweep (its own interval, spread over it on big maps)
void UpdateSoilMoisture(void);
// A new world: soil under water full, the rest from its wetness bits (old saves) or BASELINE.
// Runs by itself on the first sweep of a new world; call it to do it now (a scenario that adds
// water after it wants dry ground under that water). Loading a save with moisture: MarkLoaded.
void SoilMoistureInitNow(void);
void SoilMoistureMarkLoaded(void);
// Moisture belongs to this world (set up or loaded): readers that mustn't set it up (drawing) check it
bool SoilMoistureReady(void);
// Below normal: grass doesn't grow back (groundwear.c)
bool SoilTooDryToGrow(int x, int y, int z);
// Moisture of the soil at (x, y, z) (sets a new world up first); MOISTURE_BASELINE if moisture is off
int SoilMoistureAt(int x, int y, int z);
float GetMoistureSweepAccum(void);
void SetMoistureSweepAccum(float v);

#endif
