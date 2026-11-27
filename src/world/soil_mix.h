// Soil mixes (docs/todo/world/soil-mixes.md): what the top soil is made of, finer than its
// material. A cell's material stays the dominant soil (what you dig, what every other system
// reads); the mix says "sandy clay" or "silt loam" and carries the finer numbers.
//
// One byte per cell (saved since v127), set by a generator (the field's "causes" soils), cleared
// for every new world (InitGridWithDimensions). Mix 0 = none: the material's own plain row, whose
// numbers are what every soil had before mixes. A cell whose material no longer matches its mix
// (dug out, filled in) reads as its material's plain row.
#ifndef SOIL_MIX_H
#define SOIL_MIX_H

#include <stdbool.h>
#include <stdint.h>
#include "material.h"

typedef enum {
    SOILMIX_NONE = 0,
    SOILMIX_SAND,
    SOILMIX_LOAMY_SAND,
    SOILMIX_SANDY_LOAM,
    SOILMIX_LOAM,
    SOILMIX_SILT_LOAM,
    SOILMIX_CLAY_LOAM,
    SOILMIX_SANDY_CLAY,
    SOILMIX_SILTY_CLAY,
    SOILMIX_CLAY,
    SOILMIX_PEATY_LOAM,
    SOILMIX_PEAT,
    SOILMIX_GRAVELLY_LOAM,
    SOILMIX_GRAVEL,
    SOILMIX_COUNT
} SoilMix;

typedef struct {
    const char* name;
    MaterialType base;     // the dominant soil: the cell's material
    uint8_t moisture;      // how wet it sits when nothing else happens (MOISTURE_* scale)
    uint8_t fertility;     // 0..255, loam 128 (a tilled plot starts with it)
    Color hint;            // the soil showing through grass / bare ground (quiet)
    Color view;            // the soil view (loud, on demand)
    float capacity;        // water units (1/7 cell) a full cell holds (moisture.c)
    float soak;            // chance per sync pass that a unit of standing water soaks in
    float dry;             // drying speed (sand drains, clay and peat hold on)
    MaterialType tree;     // what grows here from seed
} SoilMixDef;

extern const SoilMixDef soilMixDefs[SOILMIX_COUNT];
extern uint8_t* soilMixGrid;   // allocated in grid_layers.c
extern bool soilView;          // setting soilView: colour the ground by soil mix

// The mix a material is when nothing finer is known
SoilMix SoilMixDefault(MaterialType mat);
#define SOIL_MIX(x, y, z) (soilMixGrid CELL_AT((x), (y), (z)))
// A new world: no mixes (every soil its plain row)
void ClearSoilMixes(void);
void SetSoilMix(int x, int y, int z, SoilMix m);
// The cell's mix: SOILMIX_NONE when none was set for this world or the material changed
SoilMix SoilMixAt(int x, int y, int z);
// The mix, or the material's default when none: always a real row for a soil cell
SoilMix SoilMixOrDefault(int x, int y, int z);
// The row for a cell (its mix or its material's plain row); NULL when the cell isn't soil
const SoilMixDef* SoilMixDefAt(int x, int y, int z);
// The plain row of a soil material; NULL when it isn't soil
const SoilMixDef* SoilMixDefForMaterial(MaterialType mat);
// Name for describing a cell: "Sandy clay" (the mix), "Loam" for plain dirt, else the material's name
const char* SoilNameAt(int x, int y, int z);
// Nearest named mix to these fractions (0..1; silt is what sand and clay leave)
SoilMix SoilMixClassify(float sand, float clay, float organic, float stones);

#endif
