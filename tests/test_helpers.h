#pragma once

#include "../src/world/grid.h"
#include "../src/simulation/weather.h"
#include "../src/simulation/water.h"
#include "../src/simulation/fire.h"

// Single-chunk grid by dimensions (most common test pattern)
static inline void InitTestGrid(int w, int h) {
    InitGridWithSizeAndChunkSize(w, h, w, h);
}

// Single-chunk grid from ASCII (derives w,h from string, uses them as chunk size)
static inline int InitTestGridFromAscii(const char* ascii) {
    int width = 0, height = 0, cur = 0;
    for (const char* p = ascii; *p; p++) {
        if (*p == '\n') {
            if (cur > width) width = cur;
            if (cur > 0) height++;
            cur = 0;
        } else {
            cur++;
        }
    }
    if (cur > 0) {
        if (cur > width) width = cur;
        height++;
    }
    return InitGridFromAsciiWithChunkSize(ascii, width, height);
}

// Count total water across the entire grid
static inline int CountTotalWater(void) {
    int total = 0;
    for (int z = 0; z < gridDepth; z++)
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++)
                total += GetWaterLevel(x, y, z);
    return total;
}

// Run N fire update ticks
static inline void RunFireTicks(int n) {
    for (int i = 0; i < n; i++) UpdateFire();
}
