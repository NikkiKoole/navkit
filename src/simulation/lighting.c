// lighting.c - Sky light and block light propagation
//
// Sky light: column scan (top-down) + horizontal BFS spread
// Block light: BFS flood fill from placed sources
// Both write into lightGrid which rendering reads each frame.

#include "lighting.h"
#include "../world/cell_defs.h"
#include "../world/material.h"
#include <string.h>
#include <math.h>

// Tweakable settings
bool lightingEnabled = true;
bool skyLightEnabled = true;
bool blockLightEnabled = true;
int  lightAmbientR = 15;
int  lightAmbientG = 15;
int  lightAmbientB = 20;
int  lightDefaultIntensity = 10;
int  lightDefaultR = 255;
int  lightDefaultG = 180;
int  lightDefaultB = 100;

// Light grid (per-cell computed light)
LightCell lightGrid[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Light sources (sparse list)
LightSource lightSources[MAX_LIGHT_SOURCES];
int lightSourceCount = 0;

// Dirty flag
bool lightingDirty = true;

// BFS queue (shared between sky spread and block light)
#define LIGHT_BFS_MAX (MAX_GRID_WIDTH * MAX_GRID_HEIGHT * 4)
typedef struct { int x, y, z; uint8_t level; } LightBfsNode;
static LightBfsNode bfsQueue[LIGHT_BFS_MAX];

void InitLighting(void) {
    memset(lightGrid, 0, sizeof(lightGrid));
    memset(lightSources, 0, sizeof(lightSources));
    lightSourceCount = 0;
    lightingDirty = true;
}

void InvalidateLighting(void) {
    lightingDirty = true;
}

// --------------------------------------------------------------------------
// Sky light: column scan
// --------------------------------------------------------------------------

// For each (x,y), scan top-down. Full sky light until blocked by solid/floor.
static void ComputeSkyColumns(void) {
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            uint8_t level = SKY_LIGHT_MAX;
            for (int z = gridDepth - 1; z >= 0; z--) {
                lightGrid[z][y][x].skyLevel = level;

                // If this cell is solid or has a floor, block sky light below
                if (CellIsSolid(grid[z][y][x]) || HAS_FLOOR(x, y, z)) {
                    level = 0;
                }
            }
        }
    }
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
                uint8_t level = lightGrid[z][y][x].skyLevel;
                if (level <= 1) continue;  // No light to spread
                if (CellIsSolid(grid[z][y][x])) continue;  // Can't spread from inside solid

                // Check if any neighbor is darker — if so, this cell is a seed
                static const int dx[] = {1, -1, 0, 0};
                static const int dy[] = {0, 0, 1, -1};
                for (int d = 0; d < 4; d++) {
                    int nx = x + dx[d];
                    int ny = y + dy[d];
                    if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
                    if (CellIsSolid(grid[z][ny][nx])) continue;
                    if (lightGrid[z][ny][nx].skyLevel < level - 1) {
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
            if (CellIsSolid(grid[node.z][ny][nx])) continue;

            if (lightGrid[node.z][ny][nx].skyLevel < newLevel) {
                lightGrid[node.z][ny][nx].skyLevel = newLevel;
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
            for (int x = 0; x < gridWidth; x++) {
                lightGrid[z][y][x].blockR = 0;
                lightGrid[z][y][x].blockG = 0;
                lightGrid[z][y][x].blockB = 0;
            }
        }
    }
}

// Propagate a single block light source via BFS with Euclidean falloff
static void PropagateBlockLight(LightSource* src) {
    int head = 0, tail = 0;
    float radius = (float)src->intensity;

    // Seed with the source cell
    bfsQueue[0] = (LightBfsNode){ src->x, src->y, src->z, src->intensity };
    tail = 1;

    // Set source cell to full brightness
    {
        LightCell* lc = &lightGrid[src->z][src->y][src->x];
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
            int nz = node.z;
            if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;

            // Blocked by solid cells
            if (CellIsSolid(grid[nz][ny][nx])) continue;

            // Euclidean distance from source for circular falloff
            float ddx = (float)(nx - src->x);
            float ddy = (float)(ny - src->y);
            float dist = sqrtf(ddx * ddx + ddy * ddy);
            if (dist >= radius) continue;  // Outside light radius

            // Scale color by (1 - dist/radius) for smooth circular falloff
            float falloff = 1.0f - (dist / radius);
            int scaledR = (int)(src->r * falloff);
            int scaledG = (int)(src->g * falloff);
            int scaledB = (int)(src->b * falloff);
            if (scaledR < 0) scaledR = 0;
            if (scaledG < 0) scaledG = 0;
            if (scaledB < 0) scaledB = 0;

            // Only update if we're adding more light than already there
            LightCell* lc = &lightGrid[nz][ny][nx];
            int existingMax = lc->blockR;
            if (lc->blockG > existingMax) existingMax = lc->blockG;
            if (lc->blockB > existingMax) existingMax = lc->blockB;
            int newMax = scaledR;
            if (scaledG > newMax) newMax = scaledG;
            if (scaledB > newMax) newMax = scaledB;

            if (newMax <= existingMax) continue;  // Already brighter from another source

            // Max blending
            int r = scaledR > lc->blockR ? scaledR : lc->blockR;
            int g = scaledG > lc->blockG ? scaledG : lc->blockG;
            int b = scaledB > lc->blockB ? scaledB : lc->blockB;
            lc->blockR = (uint8_t)r;
            lc->blockG = (uint8_t)g;
            lc->blockB = (uint8_t)b;

            if (tail < LIGHT_BFS_MAX) {
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

void RecomputeLighting(void) {
    if (skyLightEnabled) {
        ComputeSkyColumns();
        SpreadSkyLight();
    } else {
        // Clear sky light when disabled
        for (int z = 0; z < gridDepth; z++)
            for (int y = 0; y < gridHeight; y++)
                for (int x = 0; x < gridWidth; x++)
                    lightGrid[z][y][x].skyLevel = 0;
    }
    if (blockLightEnabled) {
        ComputeBlockLight();
    } else {
        ClearBlockLight();
    }
    lightingDirty = false;
}

void UpdateLighting(void) {
    if (!lightingDirty) return;
    RecomputeLighting();
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
            lightSources[i].r = r;
            lightSources[i].g = g;
            lightSources[i].b = b;
            lightSources[i].intensity = intensity;
            lightingDirty = true;
            return i;
        }
    }

    // Find free slot
    for (int i = 0; i < MAX_LIGHT_SOURCES; i++) {
        if (!lightSources[i].active) {
            lightSources[i] = (LightSource){ x, y, z, r, g, b, intensity, true };
            if (i >= lightSourceCount) lightSourceCount = i + 1;
            lightingDirty = true;
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
            lightingDirty = true;
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

    LightCell* lc = &lightGrid[z][y][x];

    // Sky contribution: skyLevel/15 * skyColor
    int sr = (skyColor.r * lc->skyLevel) / SKY_LIGHT_MAX;
    int sg = (skyColor.g * lc->skyLevel) / SKY_LIGHT_MAX;
    int sb = (skyColor.b * lc->skyLevel) / SKY_LIGHT_MAX;

    // Block light: max of this level and one level below
    // (so torches are visible when looking down from above)
    int br = lc->blockR;
    int bg = lc->blockG;
    int bb = lc->blockB;
    if (z > 0 && !CellIsSolid(grid[z][y][x])) {
        LightCell* below = &lightGrid[z - 1][y][x];
        if (below->blockR > br) br = below->blockR;
        if (below->blockG > bg) bg = below->blockG;
        if (below->blockB > bb) bb = below->blockB;
    }

    // Combined: max of sky and block per channel
    int r = sr > br ? sr : br;
    int g = sg > bg ? sg : bg;
    int b = sb > bb ? sb : bb;

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
    return lightGrid[z][y][x].skyLevel;
}
