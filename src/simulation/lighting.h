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

// Tweakable settings
extern bool lightingEnabled;       // Master toggle — when off, GetLightColor returns WHITE
extern bool skyLightEnabled;       // Toggle sky light computation
extern bool blockLightEnabled;     // Toggle block light computation
extern int  lightAmbientR;         // Ambient minimum red (0-255, prevents pitch black)
extern int  lightAmbientG;         // Ambient minimum green
extern int  lightAmbientB;         // Ambient minimum blue
extern int  lightDefaultIntensity; // Default torch intensity for sandbox placement (1-15)
extern int  lightDefaultR;         // Default torch red (0-255)
extern int  lightDefaultG;         // Default torch green (0-255)
extern int  lightDefaultB;         // Default torch blue (0-255)

// Torch color presets (selected via number keys 1-5)
typedef enum {
    TORCH_PRESET_WARM = 0,    // 1: Orange/yellow torch (default)
    TORCH_PRESET_COOL = 1,    // 2: Blue crystal
    TORCH_PRESET_FIRE = 2,    // 3: Red/orange fire
    TORCH_PRESET_GREEN = 3,   // 4: Green torch
    TORCH_PRESET_WHITE = 4,   // 5: White lantern
    TORCH_PRESET_COUNT = 5
} TorchPreset;

extern int currentTorchPreset;  // Currently selected preset (0-4)

// Helper to apply a preset to the default light color
void SetTorchPreset(TorchPreset preset);

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
