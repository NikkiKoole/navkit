// Soil mixes: see soil_mix.h
#include "soil_mix.h"
#include "grid.h"

uint8_t* soilMixGrid = NULL;   // allocated in grid_layers.c
bool soilView = false;

#define SMIX_C(r, g, b) {r, g, b, 255}
// The plain rows (sand, loam, clay, peat, gravel) carry the numbers each material had before mixes
const SoilMixDef soilMixDefs[SOILMIX_COUNT] = {
    //                         name             base        moist fert  hint                    view                    cap   soak   dry   tree
    [SOILMIX_NONE]          = {"",              MAT_NONE,   100, 128, SMIX_C(37, 32, 52),   SMIX_C(0, 0, 0),        0.0f, 0.00f, 1.0f, MAT_OAK},
    [SOILMIX_SAND]          = {"Sand",          MAT_SAND,    70,  90, SMIX_C(120, 108, 78), SMIX_C(232, 214, 150), 1.0f, 1.00f, 2.0f, MAT_BIRCH},
    [SOILMIX_LOAMY_SAND]    = {"Loamy sand",    MAT_SAND,    80, 100, SMIX_C(100, 88, 70),  SMIX_C(212, 186, 120), 1.3f, 0.85f, 1.7f, MAT_BIRCH},
    [SOILMIX_SANDY_LOAM]    = {"Sandy loam",    MAT_DIRT,    90, 120, SMIX_C(68, 58, 58),   SMIX_C(186, 152, 98),  1.7f, 0.70f, 1.3f, MAT_BIRCH},
    [SOILMIX_LOAM]          = {"Loam",          MAT_DIRT,   100, 128, SMIX_C(37, 32, 52),   SMIX_C(130, 100, 66),  2.0f, 0.50f, 1.0f, MAT_OAK},
    [SOILMIX_SILT_LOAM]     = {"Silt loam",     MAT_DIRT,   115, 150, SMIX_C(60, 58, 62),   SMIX_C(150, 146, 118), 2.3f, 0.40f, 0.85f, MAT_OAK},
    [SOILMIX_CLAY_LOAM]     = {"Clay loam",     MAT_DIRT,   120, 140, SMIX_C(62, 42, 50),   SMIX_C(160, 96, 70),   2.3f, 0.30f, 0.75f, MAT_OAK},
    [SOILMIX_SANDY_CLAY]    = {"Sandy clay",    MAT_CLAY,   110, 105, SMIX_C(96, 66, 52),   SMIX_C(204, 124, 80),  2.1f, 0.30f, 0.8f, MAT_OAK},
    [SOILMIX_SILTY_CLAY]    = {"Silty clay",    MAT_CLAY,   135, 115, SMIX_C(78, 52, 52),   SMIX_C(150, 84, 84),   2.6f, 0.15f, 0.55f, MAT_WILLOW},
    [SOILMIX_CLAY]          = {"Clay",          MAT_CLAY,   130, 110, SMIX_C(84, 54, 46),   SMIX_C(176, 70, 50),   2.5f, 0.15f, 0.6f, MAT_OAK},
    [SOILMIX_PEATY_LOAM]    = {"Peaty loam",    MAT_PEAT,   145, 165, SMIX_C(30, 30, 34),   SMIX_C(84, 72, 60),    3.0f, 0.50f, 0.7f, MAT_WILLOW},
    [SOILMIX_PEAT]          = {"Peat",          MAT_PEAT,   170, 180, SMIX_C(24, 28, 26),   SMIX_C(42, 36, 30),    4.0f, 0.50f, 0.5f, MAT_WILLOW},
    [SOILMIX_GRAVELLY_LOAM] = {"Gravelly loam", MAT_GRAVEL,  75,  90, SMIX_C(66, 64, 70),   SMIX_C(136, 136, 148), 1.4f, 0.85f, 1.6f, MAT_PINE},
    [SOILMIX_GRAVEL]        = {"Gravel",        MAT_GRAVEL,  55,  64, SMIX_C(88, 88, 86),   SMIX_C(176, 176, 180), 1.0f, 1.00f, 2.0f, MAT_PINE},
};

SoilMix SoilMixDefault(MaterialType mat) {
    switch (mat) {
        case MAT_DIRT:   return SOILMIX_LOAM;
        case MAT_CLAY:   return SOILMIX_CLAY;
        case MAT_SAND:   return SOILMIX_SAND;
        case MAT_GRAVEL: return SOILMIX_GRAVEL;
        case MAT_PEAT:   return SOILMIX_PEAT;
        default:         return SOILMIX_NONE;
    }
}

void ClearSoilMixes(void) {
    if (soilMixGrid) CLEAR_GRID_LAYER(soilMixGrid, 0);
}

void SetSoilMix(int x, int y, int z, SoilMix m) {
    if (!soilMixGrid) return;
    SOIL_MIX(x, y, z) = (uint8_t)m;
}

SoilMix SoilMixAt(int x, int y, int z) {
    if (!soilMixGrid) return SOILMIX_NONE;
    SoilMix m = (SoilMix)SOIL_MIX(x, y, z);
    if (m <= SOILMIX_NONE || m >= SOILMIX_COUNT) return SOILMIX_NONE;
    if (soilMixDefs[m].base != GetWallMaterial(x, y, z)) return SOILMIX_NONE;   // dug, filled, built
    return m;
}

SoilMix SoilMixOrDefault(int x, int y, int z) {
    SoilMix m = SoilMixAt(x, y, z);
    return m != SOILMIX_NONE ? m : SoilMixDefault(GetWallMaterial(x, y, z));
}

const SoilMixDef* SoilMixDefAt(int x, int y, int z) {
    SoilMix m = SoilMixOrDefault(x, y, z);
    return m != SOILMIX_NONE ? &soilMixDefs[m] : NULL;
}

const SoilMixDef* SoilMixDefForMaterial(MaterialType mat) {
    SoilMix m = SoilMixDefault(mat);
    return m != SOILMIX_NONE ? &soilMixDefs[m] : NULL;
}

const char* SoilNameAt(int x, int y, int z) {
    SoilMix m = SoilMixOrDefault(x, y, z);   // plain dirt reads as "Loam"
    return m != SOILMIX_NONE ? soilMixDefs[m].name : MaterialName(GetWallMaterial(x, y, z));
}

SoilMix SoilMixClassify(float sand, float clay, float organic, float stones) {
    if (organic > 0.72f) return SOILMIX_PEAT;
    if (organic > 0.48f) return SOILMIX_PEATY_LOAM;
    if (stones > 0.72f) return SOILMIX_GRAVEL;
    if (stones > 0.5f) return SOILMIX_GRAVELLY_LOAM;
    if (sand < 0.0f) sand = 0.0f;
    if (clay < 0.0f) clay = 0.0f;
    float sum = sand + clay;
    if (sum > 1.0f) { sand /= sum; clay /= sum; }
    float silt = 1.0f - sand - clay;
    if (clay >= 0.4f) {
        if (sand >= 0.45f) return SOILMIX_SANDY_CLAY;
        if (silt >= 0.4f) return SOILMIX_SILTY_CLAY;
        return SOILMIX_CLAY;
    }
    if (clay >= 0.27f) return SOILMIX_CLAY_LOAM;
    if (sand >= 0.85f) return SOILMIX_SAND;
    if (sand >= 0.7f) return SOILMIX_LOAMY_SAND;
    if (sand >= 0.52f) return SOILMIX_SANDY_LOAM;
    if (silt >= 0.5f) return SOILMIX_SILT_LOAM;
    return SOILMIX_LOAM;
}
