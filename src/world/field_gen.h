// The field: a 64x64 starting map for the game "field" (games.c): loam with patches of clay, sand,
// gravel and peat, grass of mixed height, a few trees and loose rocks, soil moisture that follows
// the ground, and two options: relief (flat / gentle) and water (none / ponds / a stream).
#ifndef FIELD_GEN_H
#define FIELD_GEN_H

typedef enum { FIELD_RELIEF_FLAT, FIELD_RELIEF_GENTLE, FIELD_RELIEF_COUNT } FieldRelief;
typedef enum { FIELD_WATER_NONE, FIELD_WATER_PONDS, FIELD_WATER_STREAM, FIELD_WATER_COUNT } FieldWater;

extern int fieldRelief;   // FieldRelief (setting fieldRelief)
extern int fieldWater;    // FieldWater (setting fieldWater)
typedef enum { FIELD_SOILS_PATCHES, FIELD_SOILS_CAUSES, FIELD_SOILS_COUNT } FieldSoils;
extern int fieldSoils;    // FieldSoils (setting fieldSoils)
extern const char* const fieldSoilsNames[FIELD_SOILS_COUNT];
extern int fieldSize;     // index into fieldSizes (setting fieldSize): 64, 128 or 256 cells square
#define FIELD_SIZE_COUNT 3
extern const int fieldSizes[FIELD_SIZE_COUNT];
extern const char* const fieldSizeNames[FIELD_SIZE_COUNT];
extern const char* const fieldReliefNames[FIELD_RELIEF_COUNT];
extern const char* const fieldWaterNames[FIELD_WATER_COUNT];

// Build the field on the current grid (ScenarioResetWorldSized made it 64x64 and cleared it),
// from worldSeed. Sets soil moisture, edge outflow and springs; the caller rebuilds the
// derived state (entrances, graph, lighting, activity counts).
void GenerateField(void);

// Walkable ground of the current world (tree tops and cells under water left out), judged by the
// pathfinder's own steps both ways from the main land: returns the ground cells cut off from it
// (0 = one piece), *inMain the cells you can reach and get back from, *total all ground cells.
int FieldWalkableComponents(int* inMain, int* total);

#endif
