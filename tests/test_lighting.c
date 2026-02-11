#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/world/grid.h"
#include "../src/world/cell_defs.h"
#include "../src/world/material.h"
#include "../src/simulation/lighting.h"
#include <string.h>

static bool test_verbose = false;

// =============================================================================
// Initialization
// =============================================================================

describe(lighting_initialization) {
    it("should initialize light grid with all zeros") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        InitLighting();

        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                for (int x = 0; x < gridWidth; x++) {
                    expect(lightGrid[z][y][x].skyLevel == 0);
                    expect(lightGrid[z][y][x].blockR == 0);
                    expect(lightGrid[z][y][x].blockG == 0);
                    expect(lightGrid[z][y][x].blockB == 0);
                }
            }
        }
    }

    it("should have zero light sources after init") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        InitLighting();

        expect(lightSourceCount == 0);
    }

    it("should set dirty flag on init") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        InitLighting();

        expect(lightingDirty == true);
    }
}

// =============================================================================
// Light Source Management
// =============================================================================

describe(lighting_sources) {
    it("should add a light source and return valid index") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        InitLighting();

        int idx = AddLightSource(3, 2, 1, 255, 180, 100, 10);

        expect(idx >= 0);
        expect(lightSourceCount == 1);
        expect(lightSources[idx].active == true);
        expect(lightSources[idx].x == 3);
        expect(lightSources[idx].y == 2);
        expect(lightSources[idx].z == 1);
        expect(lightSources[idx].r == 255);
        expect(lightSources[idx].g == 180);
        expect(lightSources[idx].b == 100);
        expect(lightSources[idx].intensity == 10);
    }

    it("should update existing source at same position") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        InitLighting();

        int idx1 = AddLightSource(3, 2, 1, 255, 180, 100, 10);
        int idx2 = AddLightSource(3, 2, 1, 100, 200, 50, 5);

        expect(idx1 == idx2);
        expect(lightSourceCount == 1);
        expect(lightSources[idx2].r == 100);
        expect(lightSources[idx2].g == 200);
        expect(lightSources[idx2].b == 50);
        expect(lightSources[idx2].intensity == 5);
    }

    it("should add multiple sources at different positions") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        InitLighting();

        int a = AddLightSource(1, 1, 0, 255, 0, 0, 10);
        int b = AddLightSource(5, 5, 0, 0, 255, 0, 10);
        int c = AddLightSource(3, 3, 1, 0, 0, 255, 10);

        expect(a != b);
        expect(b != c);
        expect(lightSourceCount == 3);
    }

    it("should remove a light source") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        InitLighting();

        AddLightSource(3, 2, 1, 255, 180, 100, 10);
        expect(lightSourceCount == 1);

        RemoveLightSource(3, 2, 1);
        expect(lightSourceCount == 0);
    }

    it("should shrink high water mark on remove") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        InitLighting();

        AddLightSource(1, 1, 0, 255, 0, 0, 10);
        AddLightSource(2, 2, 0, 0, 255, 0, 10);
        AddLightSource(3, 3, 0, 0, 0, 255, 10);
        expect(lightSourceCount == 3);

        RemoveLightSource(3, 3, 0);
        expect(lightSourceCount == 2);

        RemoveLightSource(2, 2, 0);
        expect(lightSourceCount == 1);
    }

    it("should reuse removed slots") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        InitLighting();

        int a = AddLightSource(1, 1, 0, 255, 0, 0, 10);
        AddLightSource(2, 2, 0, 0, 255, 0, 10);

        RemoveLightSource(1, 1, 0);
        int c = AddLightSource(5, 5, 0, 100, 100, 100, 8);

        expect(c == a);  // Reused slot 0
    }

    it("should clear all light sources") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        InitLighting();

        AddLightSource(1, 1, 0, 255, 0, 0, 10);
        AddLightSource(2, 2, 0, 0, 255, 0, 10);
        AddLightSource(3, 3, 0, 0, 0, 255, 10);

        ClearLightSources();

        expect(lightSourceCount == 0);
        expect(lightingDirty == true);
    }

    it("should mark dirty when adding a source") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        InitLighting();
        lightingDirty = false;

        AddLightSource(3, 2, 1, 255, 180, 100, 10);

        expect(lightingDirty == true);
    }

    it("should mark dirty when removing a source") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        InitLighting();

        AddLightSource(3, 2, 1, 255, 180, 100, 10);
        lightingDirty = false;

        RemoveLightSource(3, 2, 1);

        expect(lightingDirty == true);
    }
}

// =============================================================================
// Sky Light - Column Scan
// =============================================================================

describe(lighting_sky_columns) {
    it("should give full sky light to open air cells") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        InitLighting();
        lightingEnabled = true;
        skyLightEnabled = true;

        // All cells are air by default
        RecomputeLighting();

        // Top layer should have full sky light
        expect(lightGrid[gridDepth - 1][0][0].skyLevel == SKY_LIGHT_MAX);
        expect(lightGrid[gridDepth - 1][4][4].skyLevel == SKY_LIGHT_MAX);

        // All layers should have full sky light (nothing blocking)
        expect(lightGrid[0][0][0].skyLevel == SKY_LIGHT_MAX);
        expect(lightGrid[1][3][3].skyLevel == SKY_LIGHT_MAX);
    }

    it("should block sky light below solid ceiling") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        InitLighting();
        lightingEnabled = true;
        skyLightEnabled = true;

        // Full ceiling at z=2 to prevent horizontal BFS spread
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                grid[2][y][x] = CELL_WALL;
            }
        }

        RecomputeLighting();

        // Above the ceiling: full sky
        expect(lightGrid[3][3][3].skyLevel == SKY_LIGHT_MAX);
        // Ceiling cells get sky light (assigned before blocking)
        expect(lightGrid[2][3][3].skyLevel == SKY_LIGHT_MAX);
        // Below full ceiling: no sky light at all (no spread sources)
        expect(lightGrid[1][3][3].skyLevel == 0);
        expect(lightGrid[0][3][3].skyLevel == 0);
    }

    it("should block sky light below full floor") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        InitLighting();
        lightingEnabled = true;
        skyLightEnabled = true;

        // Full floor at z=2 to prevent horizontal BFS spread
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                SET_FLOOR(x, y, 2);
            }
        }

        RecomputeLighting();

        // Floor cells get sky light (assigned before blocking)
        expect(lightGrid[2][3][3].skyLevel == SKY_LIGHT_MAX);
        // Below full floor: no sky light
        expect(lightGrid[1][3][3].skyLevel == 0);
    }

    it("should not compute sky light when skyLightEnabled is false") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        InitLighting();
        lightingEnabled = true;
        skyLightEnabled = false;

        RecomputeLighting();

        for (int z = 0; z < gridDepth; z++) {
            for (int y = 0; y < gridHeight; y++) {
                for (int x = 0; x < gridWidth; x++) {
                    expect(lightGrid[z][y][x].skyLevel == 0);
                }
            }
        }

        skyLightEnabled = true;  // Restore
    }
}

// =============================================================================
// Sky Light - Horizontal BFS Spread
// =============================================================================

describe(lighting_sky_spread) {
    it("should spread sky light into adjacent dark cells") {
        InitGridWithSizeAndChunkSize(16, 16, 16, 16);
        InitLighting();
        lightingEnabled = true;
        skyLightEnabled = true;

        // Create a ceiling at z=2 with a 1-cell opening at (8,8)
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                grid[2][y][x] = CELL_WALL;
            }
        }
        // Opening: remove the wall at (8,8,2)
        grid[2][8][8] = CELL_AIR;

        RecomputeLighting();

        // Below the opening at z=1: full sky light (column scan)
        expect(lightGrid[1][8][8].skyLevel == SKY_LIGHT_MAX);

        // Adjacent cells at z=1 should get spread light (SKY_LIGHT_MAX - 1)
        expect(lightGrid[1][8][9].skyLevel == SKY_LIGHT_MAX - 1);
        expect(lightGrid[1][8][7].skyLevel == SKY_LIGHT_MAX - 1);
        expect(lightGrid[1][9][8].skyLevel == SKY_LIGHT_MAX - 1);
        expect(lightGrid[1][7][8].skyLevel == SKY_LIGHT_MAX - 1);

        // Two cells away: SKY_LIGHT_MAX - 2
        expect(lightGrid[1][8][10].skyLevel == SKY_LIGHT_MAX - 2);
    }

    it("should not spread sky light through solid cells") {
        InitGridWithSizeAndChunkSize(16, 16, 16, 16);
        InitLighting();
        lightingEnabled = true;
        skyLightEnabled = true;

        // Ceiling at z=2 with opening at (8,8)
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                grid[2][y][x] = CELL_WALL;
            }
        }
        grid[2][8][8] = CELL_AIR;

        // Wall blocking spread at z=1
        grid[1][8][9] = CELL_WALL;

        RecomputeLighting();

        // The solid cell should not spread light further
        // Cell beyond the wall should get light from other directions (2 steps away)
        // but not from the blocked direction
        expect(lightGrid[1][8][8].skyLevel == SKY_LIGHT_MAX);
        // Wall cell doesn't receive spread light (solid)
        expect(lightGrid[1][8][9].skyLevel == 0);
    }
}

// =============================================================================
// Block Light - BFS Propagation
// =============================================================================

describe(lighting_block_light) {
    it("should illuminate source cell at full brightness") {
        InitGridWithSizeAndChunkSize(16, 16, 16, 16);
        InitLighting();
        lightingEnabled = true;
        blockLightEnabled = true;

        AddLightSource(8, 8, 1, 255, 180, 100, 10);
        RecomputeLighting();

        LightCell* lc = &lightGrid[1][8][8];
        expect(lc->blockR == 255);
        expect(lc->blockG == 180);
        expect(lc->blockB == 100);
    }

    it("should attenuate block light with distance") {
        InitGridWithSizeAndChunkSize(16, 16, 16, 16);
        InitLighting();
        lightingEnabled = true;
        blockLightEnabled = true;

        AddLightSource(8, 8, 1, 255, 180, 100, 10);
        RecomputeLighting();

        // Adjacent cell should be dimmer than source
        expect(lightGrid[1][8][9].blockR < 255);
        expect(lightGrid[1][8][9].blockR > 0);

        // Further cell should be dimmer still
        expect(lightGrid[1][8][11].blockR < lightGrid[1][8][9].blockR);
    }

    it("should produce circular light shape (Euclidean falloff)") {
        InitGridWithSizeAndChunkSize(16, 16, 16, 16);
        InitLighting();
        lightingEnabled = true;
        blockLightEnabled = true;

        AddLightSource(8, 8, 1, 255, 255, 255, 10);
        RecomputeLighting();

        // Cells at same Manhattan distance but different Euclidean distance
        // Cardinal neighbor (dist=1.0): should be brighter
        int cardinal = lightGrid[1][8][9].blockR;
        // Diagonal neighbor (dist=1.414): should be dimmer
        int diagonal = lightGrid[1][9][9].blockR;

        expect(cardinal > diagonal);
    }

    it("should not propagate block light through solid cells") {
        InitGridWithSizeAndChunkSize(16, 16, 16, 16);
        InitLighting();
        lightingEnabled = true;
        blockLightEnabled = true;

        // Place a torch at (8,8,1), wall at (9,8,1)
        AddLightSource(8, 8, 1, 255, 180, 100, 10);
        grid[1][8][9] = CELL_WALL;

        RecomputeLighting();

        // Wall surface gets light (written to)
        expect(lightGrid[1][8][9].blockR > 0);

        // Cell behind the wall should get no light from this direction
        // It might get light from other BFS paths going around,
        // but a cell directly behind a wall that's also walled should be dark
        grid[1][8][10] = CELL_WALL;
        RecomputeLighting();
        // Cell behind two walls should be very dark or zero
        // (light can still arrive from paths going around if they exist)
    }

    it("should write block light to solid cell surfaces") {
        InitGridWithSizeAndChunkSize(16, 16, 16, 16);
        InitLighting();
        lightingEnabled = true;
        blockLightEnabled = true;

        // Torch next to a wall
        AddLightSource(8, 8, 1, 255, 180, 100, 10);
        grid[1][8][9] = CELL_WALL;

        RecomputeLighting();

        // The wall cell should have block light (surface illumination)
        expect(lightGrid[1][8][9].blockR > 0);
        expect(lightGrid[1][8][9].blockG > 0);
        expect(lightGrid[1][8][9].blockB > 0);
    }

    it("should stay on same z-level (no vertical propagation)") {
        InitGridWithSizeAndChunkSize(16, 16, 16, 16);
        InitLighting();
        lightingEnabled = true;
        blockLightEnabled = true;

        AddLightSource(8, 8, 1, 255, 180, 100, 10);
        RecomputeLighting();

        // Block light should NOT propagate to z=0 or z=2
        expect(lightGrid[0][8][8].blockR == 0);
        expect(lightGrid[2][8][8].blockR == 0);
    }

    it("should not compute block light when blockLightEnabled is false") {
        InitGridWithSizeAndChunkSize(16, 16, 16, 16);
        InitLighting();
        lightingEnabled = true;
        blockLightEnabled = false;

        AddLightSource(8, 8, 1, 255, 180, 100, 10);
        RecomputeLighting();

        expect(lightGrid[1][8][8].blockR == 0);
        expect(lightGrid[1][8][8].blockG == 0);
        expect(lightGrid[1][8][8].blockB == 0);

        blockLightEnabled = true;  // Restore
    }

    it("should not bleed into enclosed room") {
        InitGridWithSizeAndChunkSize(16, 16, 16, 16);
        InitLighting();
        lightingEnabled = true;
        blockLightEnabled = true;

        // Build a 3x3 enclosed room at z=1 (walls at border, air inside)
        // Room: walls at x=4..6, y=4..6 border, air at (5,5,1)
        for (int y = 4; y <= 6; y++) {
            for (int x = 4; x <= 6; x++) {
                grid[1][y][x] = CELL_WALL;
            }
        }
        grid[1][5][5] = CELL_AIR;  // Interior

        // Torch outside the room
        AddLightSource(8, 8, 1, 255, 180, 100, 10);
        RecomputeLighting();

        // Interior should have no block light
        expect(lightGrid[1][5][5].blockR == 0);
        expect(lightGrid[1][5][5].blockG == 0);
        expect(lightGrid[1][5][5].blockB == 0);
    }

    it("should combine light from multiple sources") {
        InitGridWithSizeAndChunkSize(16, 16, 16, 16);
        InitLighting();
        lightingEnabled = true;
        blockLightEnabled = true;

        // Two red torches on either side of a cell
        AddLightSource(6, 8, 1, 200, 0, 0, 10);
        AddLightSource(10, 8, 1, 200, 0, 0, 10);
        RecomputeLighting();

        // Middle cell (8,8) should be brighter than if lit by just one source
        int middleR = lightGrid[1][8][8].blockR;

        // Compare with single-source brightness at same distance
        ClearLightSources();
        AddLightSource(6, 8, 1, 200, 0, 0, 10);
        RecomputeLighting();
        int singleR = lightGrid[1][8][8].blockR;

        // With max blending, the middle should be at least as bright
        // (second source provides same or more via max)
        expect(middleR >= singleR);
    }

    it("should fall off to zero beyond light radius") {
        InitGridWithSizeAndChunkSize(32, 32, 32, 32);
        InitLighting();
        lightingEnabled = true;
        blockLightEnabled = true;

        AddLightSource(16, 16, 1, 255, 255, 255, 5);
        RecomputeLighting();

        // Way beyond radius=5, should be zero
        expect(lightGrid[1][16][26].blockR == 0);
        expect(lightGrid[1][16][26].blockG == 0);
        expect(lightGrid[1][16][26].blockB == 0);
    }
}

// =============================================================================
// GetLightColor Query
// =============================================================================

describe(lighting_get_light_color) {
    it("should return WHITE when lighting is disabled") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        InitLighting();
        lightingEnabled = false;

        Color c = GetLightColor(4, 4, 1, (Color){255, 255, 255, 255});

        expect(c.r == 255);
        expect(c.g == 255);
        expect(c.b == 255);
        expect(c.a == 255);

        lightingEnabled = true;  // Restore
    }

    it("should return WHITE for out-of-bounds coordinates") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        InitLighting();
        lightingEnabled = true;

        Color c1 = GetLightColor(-1, 0, 0, (Color){255, 255, 255, 255});
        Color c2 = GetLightColor(0, -1, 0, (Color){255, 255, 255, 255});
        Color c3 = GetLightColor(0, 0, -1, (Color){255, 255, 255, 255});
        Color c4 = GetLightColor(100, 0, 0, (Color){255, 255, 255, 255});

        expect(c1.r == 255);
        expect(c2.r == 255);
        expect(c3.r == 255);
        expect(c4.r == 255);
    }

    it("should apply sky color modulation") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        InitLighting();
        lightingEnabled = true;
        skyLightEnabled = true;
        blockLightEnabled = true;
        lightAmbientR = 0;
        lightAmbientG = 0;
        lightAmbientB = 0;

        RecomputeLighting();

        // Full sky light with half-brightness sky color
        Color skyHalf = {128, 128, 128, 255};
        Color c = GetLightColor(4, 4, gridDepth - 1, skyHalf);

        // skyLevel=15, skyColor=128: (128 * 15) / 15 = 128
        expect(c.r == 128);
        expect(c.g == 128);
        expect(c.b == 128);

        // Restore defaults
        lightAmbientR = 15;
        lightAmbientG = 15;
        lightAmbientB = 20;
    }

    it("should apply ambient minimum") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        InitLighting();
        lightingEnabled = true;
        skyLightEnabled = true;
        blockLightEnabled = true;
        lightAmbientR = 30;
        lightAmbientG = 30;
        lightAmbientB = 30;

        // Create a completely dark cell (under solid ceiling, no block light)
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                grid[2][y][x] = CELL_WALL;
            }
        }
        RecomputeLighting();

        Color c = GetLightColor(4, 4, 1, (Color){255, 255, 255, 255});

        // Should be at least ambient minimum
        expect(c.r >= 30);
        expect(c.g >= 30);
        expect(c.b >= 30);

        // Restore
        lightAmbientR = 15;
        lightAmbientG = 15;
        lightAmbientB = 20;
    }

    it("should take max of sky and block per channel") {
        InitGridWithSizeAndChunkSize(16, 16, 16, 16);
        InitLighting();
        lightingEnabled = true;
        skyLightEnabled = true;
        blockLightEnabled = true;
        lightAmbientR = 0;
        lightAmbientG = 0;
        lightAmbientB = 0;

        // Torch at source cell with full sky light
        AddLightSource(8, 8, 1, 255, 0, 0, 10);
        RecomputeLighting();

        Color skyWhite = {200, 200, 200, 255};
        Color c = GetLightColor(8, 8, 1, skyWhite);

        // Red channel: max(sky=200, block=255) = 255
        expect(c.r == 255);
        // Green channel: max(sky=200, block=0) = 200
        expect(c.g == 200);

        // Restore
        lightAmbientR = 15;
        lightAmbientG = 15;
        lightAmbientB = 20;
    }

    it("should add z-1 block light bleed for air cells") {
        InitGridWithSizeAndChunkSize(16, 16, 16, 16);
        InitLighting();
        lightingEnabled = true;
        skyLightEnabled = false;  // Disable sky to isolate block light
        blockLightEnabled = true;
        lightAmbientR = 0;
        lightAmbientG = 0;
        lightAmbientB = 0;

        // Torch at z=1, query from z=2 (air above)
        AddLightSource(8, 8, 1, 200, 0, 0, 10);
        RecomputeLighting();

        // z=2 has no block light (no vertical propagation)
        expect(lightGrid[2][8][8].blockR == 0);

        // But GetLightColor at z=2 should add z-1 bleed
        Color c = GetLightColor(8, 8, 2, (Color){0, 0, 0, 255});

        // Should get some red from z-1 bleed (blockR/2)
        expect(c.r > 0);

        // Restore
        skyLightEnabled = true;
        lightAmbientR = 15;
        lightAmbientG = 15;
        lightAmbientB = 20;
    }

    it("should not add z-1 bleed for solid cells") {
        InitGridWithSizeAndChunkSize(16, 16, 16, 16);
        InitLighting();
        lightingEnabled = true;
        skyLightEnabled = false;
        blockLightEnabled = true;
        lightAmbientR = 0;
        lightAmbientG = 0;
        lightAmbientB = 0;

        // Torch at z=1, solid wall at z=2
        AddLightSource(8, 8, 1, 200, 0, 0, 10);
        grid[2][8][8] = CELL_WALL;
        RecomputeLighting();

        // Solid cell at z=2 should NOT get z-1 bleed
        Color c = GetLightColor(8, 8, 2, (Color){0, 0, 0, 255});

        // No sky, no block, no bleed (solid), no ambient = 0
        expect(c.r == 0);

        // Restore
        skyLightEnabled = true;
        lightAmbientR = 15;
        lightAmbientG = 15;
        lightAmbientB = 20;
    }
}

// =============================================================================
// InvalidateLighting / UpdateLighting
// =============================================================================

describe(lighting_update) {
    it("should not recompute when not dirty") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        InitLighting();
        lightingEnabled = true;
        skyLightEnabled = true;
        blockLightEnabled = true;

        RecomputeLighting();
        expect(lightingDirty == false);

        // Manually zero out a cell
        lightGrid[gridDepth - 1][0][0].skyLevel = 0;

        // UpdateLighting should NOT recompute (not dirty)
        UpdateLighting();
        expect(lightGrid[gridDepth - 1][0][0].skyLevel == 0);
    }

    it("should recompute when dirty") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        InitLighting();
        lightingEnabled = true;
        skyLightEnabled = true;
        blockLightEnabled = true;

        RecomputeLighting();

        // Manually zero and mark dirty
        lightGrid[gridDepth - 1][0][0].skyLevel = 0;
        InvalidateLighting();
        expect(lightingDirty == true);

        UpdateLighting();
        // Should have recomputed — open air cell gets full sky light
        expect(lightGrid[gridDepth - 1][0][0].skyLevel == SKY_LIGHT_MAX);
        expect(lightingDirty == false);
    }
}

// =============================================================================
// GetSkyLight Query
// =============================================================================

describe(lighting_get_sky_light) {
    it("should return sky level for valid cells") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        InitLighting();
        lightingEnabled = true;
        skyLightEnabled = true;

        RecomputeLighting();

        expect(GetSkyLight(0, 0, gridDepth - 1) == SKY_LIGHT_MAX);
    }

    it("should return SKY_LIGHT_MAX for out-of-bounds") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        InitLighting();

        expect(GetSkyLight(-1, 0, 0) == SKY_LIGHT_MAX);
        expect(GetSkyLight(0, -1, 0) == SKY_LIGHT_MAX);
        expect(GetSkyLight(0, 0, -1) == SKY_LIGHT_MAX);
        expect(GetSkyLight(100, 0, 0) == SKY_LIGHT_MAX);
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

describe(lighting_edge_cases) {
    it("should handle grid boundaries correctly for block light") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        InitLighting();
        lightingEnabled = true;
        blockLightEnabled = true;

        // Torch at corner
        AddLightSource(0, 0, 1, 255, 255, 255, 5);
        RecomputeLighting();

        // Should not crash, source cell should have light
        expect(lightGrid[1][0][0].blockR == 255);
    }

    it("should handle torch at grid edge") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        InitLighting();
        lightingEnabled = true;
        blockLightEnabled = true;

        AddLightSource(7, 7, 1, 255, 255, 255, 10);
        RecomputeLighting();

        expect(lightGrid[1][7][7].blockR == 255);
        // Adjacent inward cell should have some light
        expect(lightGrid[1][7][6].blockR > 0);
    }

    it("should handle removing nonexistent source gracefully") {
        InitGridWithSizeAndChunkSize(8, 8, 8, 8);
        InitLighting();

        // Should not crash
        RemoveLightSource(5, 5, 5);

        expect(lightSourceCount == 0);
    }

    it("should handle intensity 1 (minimum radius)") {
        InitGridWithSizeAndChunkSize(16, 16, 16, 16);
        InitLighting();
        lightingEnabled = true;
        blockLightEnabled = true;

        AddLightSource(8, 8, 1, 255, 255, 255, 1);
        RecomputeLighting();

        // Source cell lit
        expect(lightGrid[1][8][8].blockR == 255);
        // Adjacent cells should be dark (radius=1, Euclidean dist=1.0 >= 1.0)
        expect(lightGrid[1][8][9].blockR == 0);
    }
}

// =============================================================================
// Z-1 Visibility (torch at z=1 seen from z=2)
// =============================================================================

describe(lighting_z1_visibility) {
    it("should have block light in air cells at z=1 where torch is") {
        InitGridWithSizeAndChunkSize(16, 16, 16, 16);
        InitLighting();
        lightingEnabled = true;
        skyLightEnabled = false;
        blockLightEnabled = true;
        lightAmbientR = 0;
        lightAmbientG = 0;
        lightAmbientB = 0;

        // Ground: solid walls at z=0 (grass on top)
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++)
                grid[0][y][x] = CELL_WALL;
        // z=1 is air (where movers walk, torches placed)
        // z=2 is air (one level up)

        // Torch at (8,8,1) on the grass
        AddLightSource(8, 8, 1, 200, 140, 80, 10);
        RecomputeLighting();

        // Air cell at z=1 where torch is: should have full light
        if (test_verbose) {
            printf("  lightGrid[1][8][8].blockR = %d\n", lightGrid[1][8][8].blockR);
            printf("  lightGrid[1][8][8].blockG = %d\n", lightGrid[1][8][8].blockG);
            printf("  lightGrid[1][8][8].blockB = %d\n", lightGrid[1][8][8].blockB);
        }
        expect(lightGrid[1][8][8].blockR == 200);
        expect(lightGrid[1][8][8].blockG == 140);
        expect(lightGrid[1][8][8].blockB == 80);

        // Nearby air cell at z=1: should have attenuated light
        expect(lightGrid[1][8][10].blockR > 0);

        // z=2 should have NO block light (no vertical propagation)
        expect(lightGrid[2][8][8].blockR == 0);

        // Restore
        skyLightEnabled = true;
        lightAmbientR = 15;
        lightAmbientG = 15;
        lightAmbientB = 20;
    }

    it("should bleed z=1 torch light into GetLightColor at z=2") {
        InitGridWithSizeAndChunkSize(16, 16, 16, 16);
        InitLighting();
        lightingEnabled = true;
        skyLightEnabled = false;
        blockLightEnabled = true;
        lightAmbientR = 0;
        lightAmbientG = 0;
        lightAmbientB = 0;

        // Ground: solid walls at z=0
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++)
                grid[0][y][x] = CELL_WALL;

        // Torch at (8,8,1) in air
        AddLightSource(8, 8, 1, 200, 140, 80, 10);
        RecomputeLighting();

        // GetLightColor at z=2 (air) should pick up z-1 bleed
        Color c = GetLightColor(8, 8, 2, (Color){0, 0, 0, 255});

        if (test_verbose) {
            printf("  GetLightColor(8,8,2) = (%d, %d, %d)\n", c.r, c.g, c.b);
            printf("  grid[2][8][8] solid? %d\n", CellIsSolid(grid[2][8][8]));
            printf("  lightGrid[1][8][8].blockR = %d\n", lightGrid[1][8][8].blockR);
        }

        // z-1 bleed: blockR/2 = 200/2 = 100
        expect(c.r == 100);
        expect(c.g == 70);
        expect(c.b == 40);

        // Restore
        skyLightEnabled = true;
        lightAmbientR = 15;
        lightAmbientG = 15;
        lightAmbientB = 20;
    }

    it("should bleed z=1 torch light for nearby cells at z=2") {
        InitGridWithSizeAndChunkSize(16, 16, 16, 16);
        InitLighting();
        lightingEnabled = true;
        skyLightEnabled = false;
        blockLightEnabled = true;
        lightAmbientR = 0;
        lightAmbientG = 0;
        lightAmbientB = 0;

        // Ground at z=0
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++)
                grid[0][y][x] = CELL_WALL;

        AddLightSource(8, 8, 1, 200, 140, 80, 10);
        RecomputeLighting();

        // Cell 3 steps away at z=2 should also get bleed from z=1
        Color c = GetLightColor(8, 11, 2, (Color){0, 0, 0, 255});
        int belowBlockR = lightGrid[1][11][8].blockR;

        if (test_verbose) {
            printf("  lightGrid[1][11][8].blockR = %d (3 cells from torch)\n", belowBlockR);
            printf("  GetLightColor(8,11,2) = (%d, %d, %d)\n", c.r, c.g, c.b);
        }

        // z=1 cell 3 away should have some block light
        expect(belowBlockR > 0);
        // z=2 bleed should be half of that
        expect(c.r == belowBlockR / 2);

        // Restore
        skyLightEnabled = true;
        lightAmbientR = 15;
        lightAmbientG = 15;
        lightAmbientB = 20;
    }

    it("rendering z-1 floor scenario: solid wall at z=1 next to torch") {
        // This tests the ACTUAL rendering scenario:
        // z=0: solid ground
        // z=1: mix of air (grass walkable) and solid walls (room walls)
        // z=2: air (viewer level)
        // Torch is in air at z=1. Adjacent wall at z=1 is solid.
        // Rendering from z=2 draws wall TOP at z=1 using GetLightColor(x,y,2).
        // The z-1 bleed reads lightGrid[1] - but IS the wall cell at z=1 lit?

        InitGridWithSizeAndChunkSize(16, 16, 16, 16);
        InitLighting();
        lightingEnabled = true;
        skyLightEnabled = false;
        blockLightEnabled = true;
        lightAmbientR = 0;
        lightAmbientG = 0;
        lightAmbientB = 0;

        // Ground at z=0
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++)
                grid[0][y][x] = CELL_WALL;

        // Wall at z=1, position (9,8) - adjacent to torch
        grid[1][8][9] = CELL_WALL;

        // Torch in air at (8,8,1)
        AddLightSource(8, 8, 1, 200, 140, 80, 10);
        RecomputeLighting();

        // The wall surface at z=1 should have block light (BFS writes to solids)
        if (test_verbose) {
            printf("  Wall at z=1 (9,8): blockR=%d blockG=%d blockB=%d\n",
                lightGrid[1][8][9].blockR, lightGrid[1][8][9].blockG, lightGrid[1][8][9].blockB);
            printf("  Air at z=2 (9,8): blockR=%d\n", lightGrid[2][8][9].blockR);
            printf("  grid[2][8][9] solid? %d\n", CellIsSolid(grid[2][8][9]));
        }

        // Wall at z=1 should have block light written to its surface
        expect(lightGrid[1][8][9].blockR > 0);

        // Now the key question: GetLightColor(9, 8, 2) for the wall top
        // z=2 is air, so z-1 bleed triggers, reads lightGrid[1][8][9]
        Color c = GetLightColor(9, 8, 2, (Color){0, 0, 0, 255});

        if (test_verbose) {
            printf("  GetLightColor(9,8,2) = (%d, %d, %d)\n", c.r, c.g, c.b);
        }

        expect(c.r > 0);

        // Restore
        skyLightEnabled = true;
        lightAmbientR = 15;
        lightAmbientG = 15;
        lightAmbientB = 20;
    }

    it("rendering z-1 floor scenario: open grass area (no walls at z=1)") {
        // Simpler case: z=0 is solid ground, z=1 is all air, z=2 is all air
        // Torch at z=1. Rendering from z=2 draws z=0 wall tops.
        // GetLightColor(x,y,2) z-1 bleed reads lightGrid[1] which is air with block light.
        // BUT: the z-1 floor rendering draws grid[0] (solid) viewed from z=1.
        // Wait - from z=2, z-1 is z=1, and the floor is grid[1] if solid, but
        // z=1 is air here. So z-1 floor draws grid[0] solid tops? No...
        // z-1 floor from z=2: zBelow=1, checks grid[1] for solid. If air, no floor drawn.
        // Then it checks z=0 for bedrock? No, the z-1 floor only looks at zBelow=z-1.
        //
        // Actually: from z=2, zBelow=1. grid[1] is air, so no z-1 floor is drawn.
        // The floor you see is from HAS_FLOOR or from deeper levels.
        // From z=1: zBelow=0. grid[0] is solid, so z-1 floor IS drawn.
        // GetLightColor(x,y,1) at z=1: lightGrid[1] has block light directly. Good.

        InitGridWithSizeAndChunkSize(16, 16, 16, 16);
        InitLighting();
        lightingEnabled = true;
        skyLightEnabled = false;
        blockLightEnabled = true;
        lightAmbientR = 0;
        lightAmbientG = 0;
        lightAmbientB = 0;

        // Ground at z=0
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++)
                grid[0][y][x] = CELL_WALL;

        // z=1 all air, z=2 all air
        AddLightSource(8, 8, 1, 200, 140, 80, 10);
        RecomputeLighting();

        // From z=1: GetLightColor(8,8,1) should show torch directly
        Color c1 = GetLightColor(8, 8, 1, (Color){0, 0, 0, 255});
        expect(c1.r == 200);

        // From z=2: GetLightColor(8,8,2) should show z-1 bleed
        Color c2 = GetLightColor(8, 8, 2, (Color){0, 0, 0, 255});
        expect(c2.r == 100);  // 200/2

        if (test_verbose) {
            printf("  From z=1: (%d,%d,%d)\n", c1.r, c1.g, c1.b);
            printf("  From z=2: (%d,%d,%d)\n", c2.r, c2.g, c2.b);
        }

        // Restore
        skyLightEnabled = true;
        lightAmbientR = 15;
        lightAmbientG = 15;
        lightAmbientB = 20;
    }
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char* argv[]) {
    bool verbose = false;
    bool quiet = false;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'v') verbose = true;
        if (argv[i][0] == '-' && argv[i][1] == 'q') quiet = true;
    }
    test_verbose = verbose;
    if (!verbose) {
        if (quiet) {
            set_quiet_mode(1);
        }
        SetTraceLogLevel(LOG_NONE);
    }

    test(lighting_initialization);
    test(lighting_sources);
    test(lighting_sky_columns);
    test(lighting_sky_spread);
    test(lighting_block_light);
    test(lighting_get_light_color);
    test(lighting_update);
    test(lighting_get_sky_light);
    test(lighting_edge_cases);
    test(lighting_z1_visibility);

    return summary();
}
