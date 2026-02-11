#ifndef LIGHTING_H
#define LIGHTING_H

#include <stdbool.h>
#include <stdint.h>
#include "../world/grid.h"
#include "../../vendor/raylib.h"

// Sky light uses 0-15 scale (4 bits, fits in nibble)
#define SKY_LIGHT_MAX 15

// Block light max intensity (propagation radius)
#define BLOCK_LIGHT_MAX 15

// Maximum number of placed light sources
#define MAX_LIGHT_SOURCES 1024

// Ambient minimum light (prevents pitch black)
#define LIGHT_AMBIENT_MIN_R 15
#define LIGHT_AMBIENT_MIN_G 15
#define LIGHT_AMBIENT_MIN_B 20

// Per-cell computed light (what rendering reads)
typedef struct {
    uint8_t skyLevel;   // 0-15: sky light intensity at this cell
    uint8_t blockR;     // Block light red component (0-255)
    uint8_t blockG;     // Block light green component (0-255)
    uint8_t blockB;     // Block light blue component (0-255)
} LightCell;

// A placed light source (torch, lamp, etc.)
typedef struct {
    int x, y, z;
    uint8_t r, g, b;        // Light color
    uint8_t intensity;       // Propagation radius (1-15)
    bool active;
} LightSource;

// Light grid
extern LightCell lightGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Light sources (sparse list)
extern LightSource lightSources[MAX_LIGHT_SOURCES];
extern int lightSourceCount;

// Dirty flag — set when terrain or sources change, triggers recompute
extern bool lightingDirty;

// Initialize lighting system
void InitLighting(void);

// Mark lighting for recomputation (call when terrain changes or sources added/removed)
void InvalidateLighting(void);

// Recompute all lighting (sky + block). Only runs if lightingDirty is true.
void UpdateLighting(void);

// Force full recompute regardless of dirty flag
void RecomputeLighting(void);

// Add/remove light sources
int  AddLightSource(int x, int y, int z, uint8_t r, uint8_t g, uint8_t b, uint8_t intensity);
void RemoveLightSource(int x, int y, int z);
void ClearLightSources(void);

// Query: get the final display color for a cell given current sky color
// skyColor comes from GetSkyColorForTime(timeOfDay)
Color GetLightColor(int x, int y, int z, Color skyColor);

// Query: raw sky level at a cell (0-15)
int GetSkyLight(int x, int y, int z);

#endif // LIGHTING_H
