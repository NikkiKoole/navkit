// Soil moisture (moisture.h, docs/todo/world/soil-moisture.md)
#include "moisture.h"
#include "water.h"
#include "weather.h"
#include "temperature.h"
#include "groundwear.h"
#include "balance.h"
#include "tide.h"
#include "../world/cell_defs.h"
#include "../core/time.h"
#include <math.h>
#include <stdlib.h>

uint8_t* moistureGrid = NULL;   // allocated in grid_layers.c

bool soilMoistureEnabled = true;
float moistureDryHours = 36.0f;
float moistureSoakSpeed = 1.0f;
float moistureSweepInterval = 2.0f;
int droughtWearPerSweep = 12;   // parched dirt: grass thins to bare in ~3-4 days of parched (wearGrassToDirt 1000)

long soilSoakedUnits = 0;
static float moistureSweepAccum = 0.0f;
static int moistureSweepRow = 0;
static unsigned moistureWorld = 0;   // gridGeneration the moisture belongs to (0 = none yet)

// Per soil: how much water a full cell holds, how fast water soaks in (chance per sync pass of
// one unit), how fast it dries (sand drains, clay and peat hold on)
typedef struct { MaterialType mat; float capacity, soak, dry; } SoilDef;
static const SoilDef soilDefs[] = {
    { MAT_DIRT,   2.0f, 0.50f, 1.0f },
    { MAT_CLAY,   2.5f, 0.15f, 0.6f },
    { MAT_SAND,   1.0f, 1.00f, 2.0f },
    { MAT_GRAVEL, 1.0f, 1.00f, 2.0f },
    { MAT_PEAT,   4.0f, 0.50f, 0.5f },
};
static const SoilDef* SoilDefOf(MaterialType mat) {
    for (int i = 0; i < (int)(sizeof(soilDefs) / sizeof(soilDefs[0])); i++)
        if (soilDefs[i].mat == mat) return &soilDefs[i];
    return NULL;
}

float SoilCapacityUnits(MaterialType mat) {
    const SoilDef* d = SoilDefOf(mat);
    return d ? d->capacity : 0.0f;
}

SoilState SoilMoistureState(int m) {
    if (m >= MOISTURE_SOAKED) return SOIL_SOAKED;
    if (m >= MOISTURE_WET) return SOIL_WET;
    if (m >= MOISTURE_MOIST) return SOIL_MOIST;
    if (m >= MOISTURE_NORMAL) return SOIL_NORMAL;
    if (m >= MOISTURE_DRY) return SOIL_DRY;
    return SOIL_PARCHED;
}

const char* SoilStateName(SoilState s) {
    static const char* names[] = { "Parched", "Dry", "Normal", "Moist", "Wet", "Soaked" };
    return names[s];
}

// The old wetness bits (0 dry, 1 damp, 2 wet, 3 soaked) as a bucket of moisture
static int WetnessBucket(int m) {
    return m >= MOISTURE_SOAKED ? 3 : m >= MOISTURE_WET ? 2 : m >= MOISTURE_MOIST ? 1 : 0;
}

bool IsSoilSurface(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return false;
    if (!CellIsSolid(GRID(x, y, z)) || !IsWallNatural(x, y, z)) return false;
    if (!IsSoilMaterial(GetWallMaterial(x, y, z))) return false;
    return z + 1 >= gridDepth || !CellIsSolid(GRID(x, y, z + 1));
}

// A new world gets its moisture set up before the first read-modify-write (moistureWorld)
static void EnsureMoistureWorld(void) {
    if (moistureWorld != gridGeneration) SoilMoistureInitNow();
}

void SetSoilMoisture(int x, int y, int z, int m) {
    if (m < 0) m = 0;
    if (m > MOISTURE_FULL) m = MOISTURE_FULL;
    MOISTURE(x, y, z) = (uint8_t)m;
    int b = WetnessBucket(m);
    if (GET_CELL_WETNESS(x, y, z) != b) SET_CELL_WETNESS(x, y, z, b);
}

void AddSoilMoisture(int x, int y, int z, int dm) {
    EnsureMoistureWorld();
    SetSoilMoisture(x, y, z, MOISTURE(x, y, z) + dm);
}

void WetSoilAt(int x, int y, int z, int dm) {
    if (!soilMoistureEnabled) return;
    if (IsSoilSurface(x, y, z)) AddSoilMoisture(x, y, z, dm);
    else if (z > 0 && IsSoilSurface(x, y, z - 1)) AddSoilMoisture(x, y, z - 1, dm);
}

void SoilWetAtLeast(int x, int y, int z, int m) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return;
    EnsureMoistureWorld();
    if (MOISTURE(x, y, z) < m) SetSoilMoisture(x, y, z, m);
}

static float Rand01(void) { return (float)rand() / ((float)RAND_MAX + 1.0f); }

// Moisture points one water unit is worth in this soil (a full cell = capacity units)
static int PointsPerUnit(const SoilDef* d) { return (int)(MOISTURE_FULL / d->capacity + 0.5f); }

bool RainSoaksIn(int x, int y, int z) {
    if (!soilMoistureEnabled || !IsSoilSurface(x, y, z)) return false;
    EnsureMoistureWorld();
    const SoilDef* d = SoilDefOf(GetWallMaterial(x, y, z));
    if (!d) return false;
    int per = PointsPerUnit(d);
    if (MOISTURE(x, y, z) > MOISTURE_FULL - per / 2) return false;   // full
    if (Rand01() >= d->soak * moistureSoakSpeed) return false;
    AddSoilMoisture(x, y, z, per);
    soilSoakedUnits++;
    return true;
}

void SoilSoakFrom(int x, int y, int z) {
    if (!soilMoistureEnabled || z <= 0) return;
    EnsureMoistureWorld();
    const WaterCell* w = &WATER(x, y, z);
    if (w->level == 0 || w->isSource || w->isFrozen) return;
    if ((w->isSea || w->isSurf) && tideEnabled) return;   // the tide's water
    int sz = z - 1;
    if (!IsSoilSurface(x, y, sz)) return;
    const SoilDef* d = SoilDefOf(GetWallMaterial(x, y, sz));
    if (!d) return;
    int per = PointsPerUnit(d);
    int m = MOISTURE(x, y, sz);
    if (m > MOISTURE_FULL - per / 2) {   // full: the water stands on it
        if (m < MOISTURE_FULL) SetSoilMoisture(x, y, sz, MOISTURE_FULL);
        return;
    }
    if (Rand01() >= d->soak * moistureSoakSpeed) return;
    SetWaterLevel(x, y, z, w->level - 1);
    AddSoilMoisture(x, y, sz, per);
    soilSoakedUnits++;
}

// What the ground settles back to when nothing wets it: "normal" up to 20 C, less in heat
// (groundwater keeps temperate ground from drying out; a hot spell does)
static int MoistureFloor(int tempC) {
    if (tempC <= 20) return MOISTURE_BASELINE;
    if (tempC >= 35) return 0;
    return MOISTURE_BASELINE * (35 - tempC) / 15;
}

// How hard the air and the plants pull water out: 1 = open bare dirt at 20 C
static float DryingPull(int x, int y, int z, int tempC) {
    float t = tempC <= 0 ? 0.0f : tempC / 20.0f;
    if (t > 2.0f) t = 2.0f;
    float sky = 1.0f;
    if (IsExposedToSky(x, y, z)) {
        if (weatherEnabled) sky = 1.0f + 0.25f * weatherState.windStrength;
    } else {
        sky = 0.1f;
    }
    // Surface evaporation (grass shades the soil) plus the plants drinking
    float surface = 1.0f, drink = 0.0f;
    switch (GetVegetation(x, y, z)) {
        case VEG_GRASS_SHORT:  surface = 0.6f; drink = 0.2f; break;
        case VEG_GRASS:        surface = 0.5f; drink = 0.35f; break;
        case VEG_GRASS_TALL:   surface = 0.4f; drink = 0.5f; break;
        case VEG_GRASS_TALLER: surface = 0.4f; drink = 0.6f; break;
        case VEG_REEDS:        surface = 0.4f; drink = 0.9f; break;
        default: break;
    }
    return (t * sky * surface) + (t > 0.0f ? t : 0.0f) * drink;
}

static void DryCell(int x, int y, int z, float hours) {
    if (z + 1 < gridDepth && WATER(x, y, z + 1).level > 0) return;   // water standing on it
    const SoilDef* d = SoilDefOf(GetWallMaterial(x, y, z));
    if (!d) return;
    int tempC = GetTemperature(x, y, z + 1 < gridDepth ? z + 1 : z);
    int m = MOISTURE(x, y, z), floor = MoistureFloor(tempC);
    if (m == floor) return;
    float k = 0.6931472f / moistureDryHours;   // ln 2 per half-life
    float rate = m > floor ? k * DryingPull(x, y, z, tempC) * d->dry
                           : k * 0.25f;          // below normal: groundwater creeps back up, slowly
    if (rate <= 0.0f) return;
    float f = rate * hours;                   // 1 - e^-x, close enough for one sweep's small x
    if (f > 1.0f) f = 1.0f;
    float delta = (float)(m - floor) * f;
    int step = (int)delta;                              // toward the floor; the fraction by chance
    if (Rand01() < fabsf(delta - (float)step)) step += delta > 0 ? 1 : -1;
    if (step == 0) return;
    SetSoilMoisture(x, y, z, m - step);
}

void SoilMoistureInitNow(void) {
    moistureWorld = gridGeneration;
    for (int z = 0; z < gridDepth; z++)
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++) {
                if (!IsSoilSurface(x, y, z)) continue;
                int m;
                if (z + 1 < gridDepth && WATER(x, y, z + 1).level > 0) m = MOISTURE_FULL;
                else {
                    static const int fromWetness[4] = { MOISTURE_BASELINE, 150, 200, 240 };
                    m = fromWetness[GET_CELL_WETNESS(x, y, z)];
                }
                SetSoilMoisture(x, y, z, m);
            }
}

void SoilMoistureMarkLoaded(void) { moistureWorld = gridGeneration; }
bool SoilMoistureReady(void) { return moistureWorld == gridGeneration; }

bool SoilTooDryToGrow(int x, int y, int z) {
    if (!soilMoistureEnabled || !IsSoilSurface(x, y, z)) return false;   // buried soil holds no moisture
    EnsureMoistureWorld();
    return MOISTURE(x, y, z) < MOISTURE_NORMAL;
}

// Rows of the sweep due after `accum` of the interval: big maps spread the sweep (like ground wear)
static int MoistureRowsDue(float accum, float interval) {
    int total = gridDepth * gridHeight;
    if (accum >= interval) return 0;
    if ((long long)total * gridWidth <= wearSpreadMinCells) return 0;
    int due = (int)((double)total * accum / interval);
    return due < 0 ? 0 : due > total ? total : due;
}

// Plants follow the moisture: parched dirt loses its grass (DroughtWear; dry ground just doesn't
// regrow, groundwear.c), soil that stays soaked grows reeds, reeds on soil drying out turn to grass
static void VegetationByMoisture(int x, int y, int z) {
    int m = MOISTURE(x, y, z);
    VegetationType veg = GetVegetation(x, y, z);
    if (m < MOISTURE_DRY) {
        if (veg != VEG_NONE) DroughtWear(x, y, z, droughtWearPerSweep);
        return;
    }
    if (m >= MOISTURE_SOAKED) {
        if (veg == VEG_REEDS || GetWallMaterial(x, y, z) != MAT_DIRT) return;
        if (z + 1 < gridDepth && WATER(x, y, z + 1).level > 0) return;   // river beds: banks get reeds (groundwear.c)
        if (veg >= VEG_GRASS_TALL || (veg == VEG_NONE && GetGroundWear(x, y, z) == 0))
            if (Rand01() < 0.02f) SetVegetation(x, y, z, VEG_REEDS);   // soaked for days, not a shower
        return;
    }
    if (veg == VEG_REEDS && m < MOISTURE_MOIST && Rand01() < 0.1f) SetVegetation(x, y, z, VEG_GRASS_TALL);
}

static void DryRow(int y, int z, float hours) {
    for (int x = 0; x < gridWidth; x++)
        if (IsSoilSurface(x, y, z)) {
            DryCell(x, y, z, hours);
            VegetationByMoisture(x, y, z);
        }
}

void UpdateSoilMoisture(void) {
    if (!soilMoistureEnabled) return;
    EnsureMoistureWorld();
    moistureSweepAccum += gameDeltaTime;
    float intervalGS = GameHoursToGameSeconds(moistureSweepInterval);
    int total = gridDepth * gridHeight;
    if (moistureSweepRow > total) moistureSweepRow = total;
    for (;;) {
        int due = moistureSweepAccum >= intervalGS ? total : MoistureRowsDue(moistureSweepAccum, intervalGS);
        // Levels interleaved (row r: y = r / depth, z = r % depth): the soil surface lies on a few
        // levels, so z-major order would put a whole level's work in a few ticks (26 ms on a 1024 map)
        for (int r = moistureSweepRow; r < due; r++) DryRow(r / gridDepth, r % gridDepth, moistureSweepInterval);
        if (due > moistureSweepRow) moistureSweepRow = due;
        if (moistureSweepAccum < intervalGS) break;
        moistureSweepAccum -= intervalGS;
        moistureSweepRow = 0;
        if (moistureSweepAccum >= intervalGS) break;
    }
}

float GetMoistureSweepAccum(void) { return moistureSweepAccum; }
void SetMoistureSweepAccum(float v) {
    moistureSweepAccum = v;
    moistureSweepRow = MoistureRowsDue(v, GameHoursToGameSeconds(moistureSweepInterval));
}
