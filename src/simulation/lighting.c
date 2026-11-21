// lighting.c - Sky light and block light propagation
//
// Sky light: column scan (top-down) + horizontal BFS spread
// Block light: BFS flood fill from placed sources
// Both write into lightGrid which rendering reads each frame.

#include "lighting.h"
#include "../world/cell_defs.h"
#include "../world/material.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Tweakable settings
bool lightingEnabled = true;
bool skyLightEnabled = true;
bool blockLightEnabled = true;
bool forceDaylight = false;
bool shaderEffects = false;
int  lightAmbientR = 25;
int  lightAmbientG = 25;
int  lightAmbientB = 30;
int  lightDefaultIntensity = 13;
int  lightDefaultR = 255;
int  lightDefaultG = 180;
int  lightDefaultB = 100;

// Current torch preset
int currentTorchPreset = TORCH_PRESET_WARM;

// Torch color preset definitions
typedef struct {
    int r, g, b;
    const char* name;
} TorchPresetDef;

static const TorchPresetDef torchPresets[TORCH_PRESET_COUNT] = {
    {255, 180, 100, "Warm Torch"},    // TORCH_PRESET_WARM
    {100, 150, 255, "Cool Crystal"},  // TORCH_PRESET_COOL
    {255, 100, 50,  "Fire"},          // TORCH_PRESET_FIRE
    {100, 255, 100, "Green Torch"},   // TORCH_PRESET_GREEN
    {255, 255, 255, "White Lantern"}  // TORCH_PRESET_WHITE
};

void SetTorchPreset(TorchPreset preset) {
    if (preset < 0 || preset >= TORCH_PRESET_COUNT) return;
    currentTorchPreset = preset;
    lightDefaultR = torchPresets[preset].r;
    lightDefaultG = torchPresets[preset].g;
    lightDefaultB = torchPresets[preset].b;
}

// Light grid (per-cell computed light)
LightCell* lightGrid = NULL;   // allocated in grid_layers.c

// Light sources (sparse list)
LightSource lightSources[MAX_LIGHT_SOURCES];
int lightSourceCount = 0;

// Dirty flag
bool lightingDirty = true;

// BFS queue (shared between sky spread and block light)
#define LIGHT_BFS_MAX (MAX_GRID_WIDTH * MAX_GRID_HEIGHT * 4)
typedef struct { int x, y, z; uint8_t level; } LightBfsNode;
static LightBfsNode bfsQueue[LIGHT_BFS_MAX];

static void ClearLightDirtyChunks(void);

void InitLighting(void) {
    CLEAR_GRID_LAYER(lightGrid, 0);
    memset(lightSources, 0, sizeof(lightSources));
    lightSourceCount = 0;
    lightingDirty = true;
    ClearLightDirtyChunks();
}

void InvalidateLighting(void) {
    lightingDirty = true;
}

// Local changes since the last update, per 16x16 chunk (all levels). A change can only
// alter light within SKY_LIGHT_MAX cells (sky spread) or twice a source's radius (block
// light paths), so UpdateLighting relights just the area around dirty chunks instead of
// the whole map (~750 ms at 2048: digging, building and fires used to trigger that).
#define LIGHT_CHUNK 16
static uint8_t* lightDirtyChunks = NULL;
static int lightDirtyCX = 0, lightDirtyCY = 0, lightDirtyCount = 0;
static int lightDirtyMaxIntensity = 0;   // of sources added/changed/removed since the last update

static void ClearLightDirtyChunks(void) {
    if (lightDirtyChunks && lightDirtyCount) memset(lightDirtyChunks, 0, (size_t)lightDirtyCX * lightDirtyCY);
    lightDirtyCount = 0;
    lightDirtyMaxIntensity = 0;
}

void InvalidateLightingAt(int x, int y, int z) {
    (void)z;
    if (lightingDirty) return;                       // a full recompute is coming anyway
    if (x < 0 || y < 0 || x >= gridWidth || y >= gridHeight) return;
    int cx = (gridWidth + LIGHT_CHUNK - 1) / LIGHT_CHUNK, cy = (gridHeight + LIGHT_CHUNK - 1) / LIGHT_CHUNK;
    if (!lightDirtyChunks || cx != lightDirtyCX || cy != lightDirtyCY) {
        free(lightDirtyChunks);
        lightDirtyChunks = calloc((size_t)cx * cy, 1);
        lightDirtyCX = cx; lightDirtyCY = cy; lightDirtyCount = 0;
        if (!lightDirtyChunks) { lightingDirty = true; return; }
    }
    uint8_t* d = &lightDirtyChunks[(y / LIGHT_CHUNK) * cx + x / LIGHT_CHUNK];
    if (!*d) { *d = 1; lightDirtyCount++; }
}

bool LightingUpdatePending(void) {
    return lightingDirty || lightDirtyCount > 0;
}

static void InvalidateLightSourceAt(int x, int y, int z, int intensity) {
    if (intensity > lightDirtyMaxIntensity) lightDirtyMaxIntensity = intensity;
    InvalidateLightingAt(x, y, z);
}

// Windows are solid but transmit light
static inline bool CellBlocksLight(CellType c) {
    return CellIsSolid(c) && c != CELL_WINDOW;
}

// --------------------------------------------------------------------------
// Sky light: column scan
// --------------------------------------------------------------------------

// For each (x,y), scan top-down. Full sky light until blocked by solid/floor.
static inline void ComputeSkyColumn(int x, int y) {
    uint8_t level = SKY_LIGHT_MAX;
    for (int z = gridDepth - 1; z >= 0; z--) {
        LIGHT(x, y, z).skyLevel = level;

        // If this cell blocks light or has a floor, block sky light below
        if (CellBlocksLight(GRID(x, y, z)) || HAS_FLOOR(x, y, z)) {
            level = 0;
        }
    }
}

static void ComputeSkyColumns(void) {
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++) ComputeSkyColumn(x, y);
}

// --------------------------------------------------------------------------
// Sky light: horizontal BFS spread
// --------------------------------------------------------------------------

// Spread sky light horizontally from lit cells into adjacent dark cells.
// Each step reduces level by 1. This simulates light spilling from openings.
static void SpreadSkyLight(void) {
    int head = 0, tail = 0;

    // Seed BFS with all cells that have sky light and border a darker cell
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                uint8_t level = LIGHT(x, y, z).skyLevel;
                if (level <= 1) continue;  // No light to spread
                if (CellBlocksLight(GRID(x, y, z))) continue;  // Can't spread from inside solid

                // Check if any neighbor is darker — if so, this cell is a seed
                static const int dx[] = {1, -1, 0, 0};
                static const int dy[] = {0, 0, 1, -1};
                for (int d = 0; d < 4; d++) {
                    int nx = x + dx[d];
                    int ny = y + dy[d];
                    if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
                    if (CellBlocksLight(GRID(nx, ny, z))) continue;
                    if (LIGHT(nx, ny, z).skyLevel < level - 1) {
                        if (tail < LIGHT_BFS_MAX) {
                            bfsQueue[tail++] = (LightBfsNode){ x, y, z, level };
                        }
                        break;  // Only enqueue this cell once
                    }
                }
            }
        }
    }

    // BFS spread
    while (head < tail) {
        LightBfsNode node = bfsQueue[head++];
        uint8_t newLevel = node.level - 1;
        if (newLevel == 0) continue;

        static const int dx[] = {1, -1, 0, 0};
        static const int dy[] = {0, 0, 1, -1};
        for (int d = 0; d < 4; d++) {
            int nx = node.x + dx[d];
            int ny = node.y + dy[d];
            if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
            if (CellBlocksLight(GRID(nx, ny, node.z))) continue;

            if (LIGHT(nx, ny, node.z).skyLevel < newLevel) {
                LIGHT(nx, ny, node.z).skyLevel = newLevel;
                if (tail < LIGHT_BFS_MAX) {
                    bfsQueue[tail++] = (LightBfsNode){ nx, ny, node.z, newLevel };
                }
            }
        }
    }
}

// --------------------------------------------------------------------------
// Block light: BFS from sources
// --------------------------------------------------------------------------

static void ClearBlockLight(void) {
    for (int z = 0; z < gridDepth; z++) {
        for (int y = 0; y < gridHeight; y++) {
            FOR_X_IN_ROW(x, i, y, z) {
                // Only write cells that have block light: writing every cell would dirty
                // the whole layer's memory even where it is already dark
                LightCell* c = &LAYER_AT(lightGrid, i);
                if (c->blockR | c->blockG | c->blockB) { c->blockR = 0; c->blockG = 0; c->blockB = 0; }
            }
        }
    }
}

// Per-source visited grid to prevent adding light to the same cell multiple times
static bool blockVisited[MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Writes of PropagateBlockLight are limited to this box (a local relight); the BFS
// itself always runs in full so it reaches exactly the same cells as a full recompute
static int blockClipX0 = 0, blockClipY0 = 0, blockClipX1 = 1 << 30, blockClipY1 = 1 << 30;
static inline bool InBlockClip(int x, int y) {
    return x >= blockClipX0 && y >= blockClipY0 && x < blockClipX1 && y < blockClipY1;
}

// Propagate a single block light source via BFS with Euclidean falloff
static void PropagateBlockLight(LightSource* src) {
    int head = 0, tail = 0;
    float radius = (float)src->intensity;
    int nz = src->z;

    // Clear visited grid for the area this light can reach
    int minX = src->x - src->intensity - 1; if (minX < 0) minX = 0;
    int maxX = src->x + src->intensity + 1; if (maxX >= gridWidth) maxX = gridWidth - 1;
    int minY = src->y - src->intensity - 1; if (minY < 0) minY = 0;
    int maxY = src->y + src->intensity + 1; if (maxY >= gridHeight) maxY = gridHeight - 1;
    for (int y = minY; y <= maxY; y++)
        for (int x = minX; x <= maxX; x++)
            blockVisited[y][x] = false;

    // Seed with the source cell
    bfsQueue[0] = (LightBfsNode){ src->x, src->y, nz, src->intensity };
    tail = 1;
    blockVisited[src->y][src->x] = true;

    // Set source cell to full brightness
    if (InBlockClip(src->x, src->y)) {
        LightCell* lc = &LIGHT(src->x, src->y, nz);
        int r = lc->blockR + src->r;
        int g = lc->blockG + src->g;
        int b = lc->blockB + src->b;
        lc->blockR = (uint8_t)(r > 255 ? 255 : r);
        lc->blockG = (uint8_t)(g > 255 ? 255 : g);
        lc->blockB = (uint8_t)(b > 255 ? 255 : b);
    }

    while (head < tail) {
        LightBfsNode node = bfsQueue[head++];
        if (node.level <= 1) continue;

        // 4 cardinal directions (block light stays on same z-level)
        static const int dx[] = {1, -1, 0, 0};
        static const int dy[] = {0, 0, 1, -1};
        for (int d = 0; d < 4; d++) {
            int nx = node.x + dx[d];
            int ny = node.y + dy[d];
            if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
            if (blockVisited[ny][nx]) continue;

            bool solid = CellIsSolid(GRID(nx, ny, nz));

            // Euclidean distance from source for circular falloff
            float ddx = (float)(nx - src->x);
            float ddy = (float)(ny - src->y);
            float dist = sqrtf(ddx * ddx + ddy * ddy);
            if (dist >= radius) continue;  // Outside light radius

            blockVisited[ny][nx] = true;

            // Scale color by (1 - dist/radius) for smooth circular falloff
            float falloff = 1.0f - (dist / radius);
            int scaledR = (int)(src->r * falloff);
            int scaledG = (int)(src->g * falloff);
            int scaledB = (int)(src->b * falloff);

            // Additive blending — lights accumulate, creating color mixing
            // Red torch + blue crystal = purple where they overlap
            if (InBlockClip(nx, ny)) {
                LightCell* lc = &LIGHT(nx, ny, nz);
                int r = lc->blockR + scaledR;
                int g = lc->blockG + scaledG;
                int b = lc->blockB + scaledB;
                lc->blockR = (uint8_t)(r > 255 ? 255 : r);
                lc->blockG = (uint8_t)(g > 255 ? 255 : g);
                lc->blockB = (uint8_t)(b > 255 ? 255 : b);
            }

            // Only propagate through non-solid cells (light hits walls but stops)
            if (!solid && tail < LIGHT_BFS_MAX) {
                bfsQueue[tail++] = (LightBfsNode){ nx, ny, nz, (uint8_t)(node.level - 1) };
            }
        }
    }
}

static void ComputeBlockLight(void) {
    ClearBlockLight();
    for (int i = 0; i < lightSourceCount; i++) {
        if (!lightSources[i].active) continue;
        PropagateBlockLight(&lightSources[i]);
    }
}

// --------------------------------------------------------------------------
// Full recompute
// --------------------------------------------------------------------------

// Relight the cells that a change inside [x0,x1) x [y0,y1) (all levels) can affect.
// Sky: recompute the columns of the area E (the change + SKY_LIGHT_MAX), then spread
// from them and from the 1-cell ring around E, whose values can't have changed (any
// light path to a cell outside E that crosses the change is longer than SKY_LIGHT_MAX).
// Block: clear the area (the change + twice the largest radius) and re-add every source
// that reaches into it, writing only inside it.
static void RelightArea(int x0, int y0, int x1, int y1) {
    if (skyLightEnabled) {
        int ex0 = x0 - SKY_LIGHT_MAX, ey0 = y0 - SKY_LIGHT_MAX, ex1 = x1 + SKY_LIGHT_MAX, ey1 = y1 + SKY_LIGHT_MAX;
        if (ex0 < 0) ex0 = 0;
        if (ey0 < 0) ey0 = 0;
        if (ex1 > gridWidth) ex1 = gridWidth;
        if (ey1 > gridHeight) ey1 = gridHeight;
        for (int y = ey0; y < ey1; y++)
            for (int x = ex0; x < ex1; x++) ComputeSkyColumn(x, y);
        int head = 0, tail = 0;
        for (int z = 0; z < gridDepth; z++)
            for (int y = ey0 - 1; y <= ey1; y++)
                for (int x = ex0 - 1; x <= ex1; x++) {
                    if (x < 0 || y < 0 || x >= gridWidth || y >= gridHeight) continue;
                    uint8_t level = LIGHT(x, y, z).skyLevel;
                    if (level <= 1 || CellBlocksLight(GRID(x, y, z))) continue;
                    if (tail < LIGHT_BFS_MAX) bfsQueue[tail++] = (LightBfsNode){ x, y, z, level };
                }
        while (head < tail) {
            LightBfsNode node = bfsQueue[head++];
            uint8_t newLevel = node.level - 1;
            if (newLevel == 0) continue;
            static const int dx[] = {1, -1, 0, 0};
            static const int dy[] = {0, 0, 1, -1};
            for (int d = 0; d < 4; d++) {
                int nx = node.x + dx[d], ny = node.y + dy[d];
                if (nx < ex0 || nx >= ex1 || ny < ey0 || ny >= ey1) continue;
                if (CellBlocksLight(GRID(nx, ny, node.z))) continue;
                if (LIGHT(nx, ny, node.z).skyLevel < newLevel) {
                    LIGHT(nx, ny, node.z).skyLevel = newLevel;
                    if (tail < LIGHT_BFS_MAX) bfsQueue[tail++] = (LightBfsNode){ nx, ny, node.z, newLevel };
                }
            }
        }
    }
    if (blockLightEnabled) {
        int rb = lightDirtyMaxIntensity;
        for (int i = 0; i < lightSourceCount; i++)
            if (lightSources[i].active && lightSources[i].intensity > rb) rb = lightSources[i].intensity;
        int m = 2 * rb + 2;
        blockClipX0 = x0 - m; blockClipY0 = y0 - m; blockClipX1 = x1 + m; blockClipY1 = y1 + m;
        if (blockClipX0 < 0) blockClipX0 = 0;
        if (blockClipY0 < 0) blockClipY0 = 0;
        if (blockClipX1 > gridWidth) blockClipX1 = gridWidth;
        if (blockClipY1 > gridHeight) blockClipY1 = gridHeight;
        for (int z = 0; z < gridDepth; z++)
            for (int y = blockClipY0; y < blockClipY1; y++)
                for (int x = blockClipX0; x < blockClipX1; x++) {
                    LightCell* c = &LIGHT(x, y, z);
                    if (c->blockR | c->blockG | c->blockB) { c->blockR = 0; c->blockG = 0; c->blockB = 0; }
                }
        for (int i = 0; i < lightSourceCount; i++) {
            LightSource* s = &lightSources[i];
            if (!s->active) continue;
            int r = s->intensity + 1;
            if (s->x + r < blockClipX0 || s->x - r >= blockClipX1 || s->y + r < blockClipY0 || s->y - r >= blockClipY1) continue;
            PropagateBlockLight(s);
        }
        blockClipX0 = 0; blockClipY0 = 0; blockClipX1 = 1 << 30; blockClipY1 = 1 << 30;
    }
}

void RecomputeLighting(void) {
    if (skyLightEnabled) {
        ComputeSkyColumns();
        SpreadSkyLight();
    } else {
        // Clear sky light when disabled
        for (int z = 0; z < gridDepth; z++)
            for (int y = 0; y < gridHeight; y++)
                for (int x = 0; x < gridWidth; x++)
                    LIGHT(x, y, z).skyLevel = 0;
    }
    if (blockLightEnabled) {
        ComputeBlockLight();
    } else {
        ClearBlockLight();
    }
    lightingDirty = false;
    ClearLightDirtyChunks();
}

void UpdateLighting(void) {
    if (lightingDirty) { RecomputeLighting(); return; }
    if (lightDirtyCount == 0) return;
    if (lightDirtyCX != (gridWidth + LIGHT_CHUNK - 1) / LIGHT_CHUNK || lightDirtyCY != (gridHeight + LIGHT_CHUNK - 1) / LIGHT_CHUNK) {
        RecomputeLighting();                     // the grid changed size since the marks
        return;
    }
    // Many scattered changes: one full pass is cheaper than many overlapping areas
    if (lightDirtyCount * 8 > lightDirtyCX * lightDirtyCY) { RecomputeLighting(); return; }
    for (int cy = 0; cy < lightDirtyCY; cy++)
        for (int cx = 0; cx < lightDirtyCX; cx++) {
            if (!lightDirtyChunks[cy * lightDirtyCX + cx]) continue;
            int x0 = cx * LIGHT_CHUNK, y0 = cy * LIGHT_CHUNK;
            int x1 = x0 + LIGHT_CHUNK < gridWidth ? x0 + LIGHT_CHUNK : gridWidth;
            int y1 = y0 + LIGHT_CHUNK < gridHeight ? y0 + LIGHT_CHUNK : gridHeight;
            RelightArea(x0, y0, x1, y1);
        }
    ClearLightDirtyChunks();
}

// --------------------------------------------------------------------------
// Light source management
// --------------------------------------------------------------------------

int AddLightSource(int x, int y, int z, uint8_t r, uint8_t g, uint8_t b, uint8_t intensity) {
    // Check if one already exists at this position
    for (int i = 0; i < lightSourceCount; i++) {
        if (lightSources[i].active &&
            lightSources[i].x == x && lightSources[i].y == y && lightSources[i].z == z) {
            // Update existing
            InvalidateLightSourceAt(x, y, z, lightSources[i].intensity > intensity ? lightSources[i].intensity : intensity);
            lightSources[i].r = r;
            lightSources[i].g = g;
            lightSources[i].b = b;
            lightSources[i].intensity = intensity;
            return i;
        }
    }

    // Find free slot
    for (int i = 0; i < MAX_LIGHT_SOURCES; i++) {
        if (!lightSources[i].active) {
            lightSources[i] = (LightSource){ x, y, z, r, g, b, intensity, true };
            if (i >= lightSourceCount) lightSourceCount = i + 1;
            InvalidateLightSourceAt(x, y, z, intensity);
            return i;
        }
    }
    return -1;  // Full
}

void RemoveLightSource(int x, int y, int z) {
    for (int i = 0; i < lightSourceCount; i++) {
        if (lightSources[i].active &&
            lightSources[i].x == x && lightSources[i].y == y && lightSources[i].z == z) {
            lightSources[i].active = false;
            InvalidateLightSourceAt(x, y, z, lightSources[i].intensity);
            // Shrink high water mark
            while (lightSourceCount > 0 && !lightSources[lightSourceCount - 1].active) {
                lightSourceCount--;
            }
            return;
        }
    }
}

void ClearLightSources(void) {
    memset(lightSources, 0, sizeof(lightSources));
    lightSourceCount = 0;
    lightingDirty = true;
}

// --------------------------------------------------------------------------
// Query functions
// --------------------------------------------------------------------------

Color GetLightColor(int x, int y, int z, Color skyColor) {
    if (!lightingEnabled) return WHITE;

    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return WHITE;
    }

    LightCell* lc = &LIGHT(x, y, z);

    // Sky contribution: skyLevel/15 * skyColor
    int sr = (skyColor.r * lc->skyLevel) / SKY_LIGHT_MAX;
    int sg = (skyColor.g * lc->skyLevel) / SKY_LIGHT_MAX;
    int sb = (skyColor.b * lc->skyLevel) / SKY_LIGHT_MAX;

    // Block light from this level
    int br = lc->blockR;
    int bg = lc->blockG;
    int bb = lc->blockB;

    // Combined: use the dominant source to preserve color character
    // Per-channel max destroys torch color (sky lifts the weak channels toward white)
    int skyBrightness = sr + sg + sb;
    int blockBrightness = br + bg + bb;
    int r, g, b;
    if (blockBrightness > skyBrightness) {
        r = br; g = bg; b = bb;
    } else {
        r = sr; g = sg; b = sb;
    }

    // Add block light from one level below (additive, so torches glow through)
    if (z > 0 && !CellIsSolid(GRID(x, y, z))) {
        LightCell* below = &LIGHT(x, y, z - 1);
        r += below->blockR / 2;
        g += below->blockG / 2;
        b += below->blockB / 2;
        if (r > 255) r = 255;
        if (g > 255) g = 255;
        if (b > 255) b = 255;
    }

    // Ambient minimum
    if (r < lightAmbientR) r = lightAmbientR;
    if (g < lightAmbientG) g = lightAmbientG;
    if (b < lightAmbientB) b = lightAmbientB;

    return (Color){ (uint8_t)r, (uint8_t)g, (uint8_t)b, 255 };
}

int GetSkyLight(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) {
        return SKY_LIGHT_MAX;
    }
    return LIGHT(x, y, z).skyLevel;
}
