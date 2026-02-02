// render/rendering.c - Core rendering functions
#include "../game_state.h"
#include "../world/cell_defs.h"
#include "../world/designations.h"
#include "../core/input_mode.h"
#include "../core/time.h"
#include "../entities/workshops.h"

// Helper: calculate visible cell range with view frustum culling
static void GetVisibleCellRange(float size, int* minX, int* minY, int* maxX, int* maxY) {
    *minX = 0;
    *minY = 0;
    *maxX = gridWidth;
    *maxY = gridHeight;
    
    if (cullDrawing) {
        int screenW = GetScreenWidth();
        int screenH = GetScreenHeight();

        *minX = (int)((-offset.x) / size);
        *maxX = (int)((-offset.x + screenW) / size) + 1;
        *minY = (int)((-offset.y) / size);
        *maxY = (int)((-offset.y + screenH) / size) + 1;

        if (*minX < 0) *minX = 0;
        if (*minY < 0) *minY = 0;
        if (*maxX > gridWidth) *maxX = gridWidth;
        if (*maxY > gridHeight) *maxY = gridHeight;
    }
}

void DrawCellGrid(void) {
    float size = CELL_SIZE * zoom;
    int z = currentViewZ;

    int minX, minY, maxX, maxY;
    GetVisibleCellRange(size, &minX, &minY, &maxX, &maxY);

    if (!g_legacyWalkability) {
        // Standard mode: draw deeper levels first (z-3, z-2) with blue tint for depth
        // These show terrain dropping away below current view
        Color depthTints[] = {
            {100, 120, 160, 255},  // z-3: darker blue
            {130, 150, 180, 255}   // z-2: lighter blue
        };
        int depthLevels[] = {z - 3, z - 2};
        
        for (int d = 0; d < 2; d++) {
            int zDepth = depthLevels[d];
            if (zDepth < 0) continue;
            
            Color tint = depthTints[d];
            for (int y = minY; y < maxY; y++) {
                for (int x = minX; x < maxX; x++) {
                    CellType cellAtDepth = grid[zDepth][y][x];
                    // Only draw if this cell is solid AND there's air above it
                    // (otherwise it's hidden by terrain above)
                    if (!CellIsSolid(cellAtDepth)) continue;
                    
                    // Check if all cells between zDepth+1 and z-1 are air (visible from above, no floors blocking)
                    bool visible = true;
                    for (int zCheck = zDepth + 1; zCheck < z; zCheck++) {
                        if (CellIsSolid(grid[zCheck][y][x]) || HAS_FLOOR(x, y, zCheck)) {
                            visible = false;
                            break;
                        }
                    }
                    if (!visible) continue;
                    
                    Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
                    Rectangle src = SpriteGetRect(CellSprite(cellAtDepth));
                    DrawTexturePro(atlas, src, dest, (Vector2){0,0}, 0, tint);
                }
            }
        }
        
        // DF mode: draw floor from z-1 (the ground you're standing ON)
        // At z=1, you see z=0's surface as the floor
        // At z=0, you see implicit bedrock as the floor (for dug holes)
        if (z > 0) {
            int zBelow = z - 1;
            for (int y = minY; y < maxY; y++) {
                for (int x = minX; x < maxX; x++) {
                    CellType cellBelow = grid[zBelow][y][x];
                    CellType cellHere = grid[z][y][x];
                    
                    // Draw floor from below if the cell below is solid and current is air/walkable
                    if (CellIsSolid(cellBelow) && !CellBlocksMovement(cellHere)) {
                        Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
                        Rectangle src = SpriteGetRect(CellSprite(cellBelow));
                        // Wall tops (looking down at a wall from above) tinted blue
                        // to distinguish from walls at current level (depth cue)
                        Color tint = CellBlocksMovement(cellBelow) ? (Color){140, 160, 200, 255} : WHITE;
                        DrawTexturePro(atlas, src, dest, (Vector2){0,0}, 0, tint);
                    }
                }
            }
        } else {
            // z=0: draw bedrock for air cells (implicit bedrock at z=-1)
            for (int y = minY; y < maxY; y++) {
                for (int x = minX; x < maxX; x++) {
                    CellType cellHere = grid[z][y][x];
                    if (cellHere == CELL_AIR) {
                        Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
                        Rectangle src = SpriteGetRect(SPRITE_bedrock);
                        DrawTexturePro(atlas, src, dest, (Vector2){0,0}, 0, WHITE);
                    }
                }
            }
        }
        
        // Draw constructed floors (HAS_FLOOR flag - for balconies/bridges over empty space)
        for (int y = minY; y < maxY; y++) {
            for (int x = minX; x < maxX; x++) {
                if (HAS_FLOOR(x, y, z)) {
                    Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
                    Rectangle src = SpriteGetRect(SPRITE_floor);
                    DrawTexturePro(atlas, src, dest, (Vector2){0,0}, 0, WHITE);
                }
            }
        }
        
        // Draw current layer (walls, ladders, etc. - things that block or occupy the space)
        // Ramps are drawn with 50% opacity so grass shows through
        for (int y = minY; y < maxY; y++) {
            for (int x = minX; x < maxX; x++) {
                CellType cell = grid[z][y][x];
                // Skip air - floor was already drawn from z-1 or HAS_FLOOR
                if (cell == CELL_AIR) continue;
                Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
                Rectangle src = SpriteGetRect(CellSprite(cell));
                Color tint = CellIsRamp(cell) ? (Color){255, 255, 255, 64} : WHITE;
                DrawTexturePro(atlas, src, dest, (Vector2){0,0}, 0, tint);
            }
        }
        
        // Draw wall cutaway effect - dark top with real wall texture visible at edges
        // This shows you're looking at the "cut" top of walls at eye level
        {
            Color cutawayColor = (Color){30, 30, 35, 255};  // Dark gray/black
            float edgeWidth = size * 0.2f;  // 20% of cell size - wall texture visible at edges
            
            for (int y = minY; y < maxY; y++) {
                for (int x = minX; x < maxX; x++) {
                    CellType cell = grid[z][y][x];
                    if (!CellBlocksMovement(cell)) continue;  // Only walls
                    
                    float px = offset.x + x * size;
                    float py = offset.y + y * size;
                    
                    // Check which sides border non-walls (those show the wall edge)
                    bool wallNorth = (y > 0) && CellBlocksMovement(grid[z][y-1][x]);
                    bool wallSouth = (y < gridHeight-1) && CellBlocksMovement(grid[z][y+1][x]);
                    bool wallWest = (x > 0) && CellBlocksMovement(grid[z][y][x-1]);
                    bool wallEast = (x < gridWidth-1) && CellBlocksMovement(grid[z][y][x+1]);
                    
                    // Calculate inset for dark fill - leave edge visible where no adjacent wall
                    float insetN = wallNorth ? 0 : edgeWidth;
                    float insetS = wallSouth ? 0 : edgeWidth;
                    float insetW = wallWest ? 0 : edgeWidth;
                    float insetE = wallEast ? 0 : edgeWidth;
                    
                    // Draw dark fill inset from edges (wall texture already drawn shows through at edges)
                    // Add 1 pixel overlap to avoid gaps between adjacent tiles
                    float overlap = 1.0f;
                    float fillX = px + insetW;
                    float fillY = py + insetN;
                    float fillW = size - insetW - insetE + (wallEast ? overlap : 0);
                    float fillH = size - insetN - insetS + (wallSouth ? overlap : 0);
                    
                    if (fillW > 0 && fillH > 0) {
                        DrawRectangle((int)fillX, (int)fillY, (int)(fillW + 0.5f), (int)(fillH + 0.5f), cutawayColor);
                    }
                }
            }
        }
        
        // Draw shadows from blocks above (z+1)
        // Only on floors/air - walls are vertical surfaces, shadows fall on their tops (not visible)
        if (z + 1 < gridDepth) {
            for (int y = minY; y < maxY; y++) {
                for (int x = minX; x < maxX; x++) {
                    CellType cellHere = grid[z][y][x];
                    // Skip walls - shadow falls on top of wall, not its face
                    if (CellBlocksMovement(cellHere)) continue;
                    
                    CellType cellAbove = grid[z + 1][y][x];
                    if (CellIsSolid(cellAbove)) {
                        Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
                        DrawRectangleRec(dest, (Color){0, 0, 0, 80});
                    }
                }
            }
        }
    } else {
        // Legacy mode: draw layer below with transparency, then current layer
        if (z > 0) {
            Color tint = (Color){255, 255, 255, 128};
            int zBelow = z - 1;
            for (int y = minY; y < maxY; y++) {
                for (int x = minX; x < maxX; x++) {
                    CellType cell = grid[zBelow][y][x];
                    if (cell == CELL_AIR) continue;
                    Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
                    Rectangle src = SpriteGetRect(CellSprite(cell));
                    DrawTexturePro(atlas, src, dest, (Vector2){0,0}, 0, tint);
                }
            }
        }

        // Draw current layer
        for (int y = minY; y < maxY; y++) {
            for (int x = minX; x < maxX; x++) {
                Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
                Rectangle src = SpriteGetRect(CellSprite(grid[z][y][x]));
                DrawTexturePro(atlas, src, dest, (Vector2){0,0}, 0, WHITE);
            }
        }
    }
}

void DrawGrassOverlay(void) {
    float size = CELL_SIZE * zoom;
    int z = currentViewZ;

    int minX, minY, maxX, maxY;
    GetVisibleCellRange(size, &minX, &minY, &maxX, &maxY);

    if (!g_legacyWalkability) {
        // Standard mode: draw grass for deeper levels (z-3, z-2) with blue tint
        Color depthTints[] = {
            {100, 120, 160, 255},  // z-3: darker blue
            {130, 150, 180, 255}   // z-2: lighter blue
        };
        int depthLevels[] = {z - 3, z - 2};
        
        for (int d = 0; d < 2; d++) {
            int zDepth = depthLevels[d];
            if (zDepth < 0) continue;
            
            Color tint = depthTints[d];
            for (int y = minY; y < maxY; y++) {
                for (int x = minX; x < maxX; x++) {
                    if (grid[zDepth][y][x] != CELL_DIRT) continue;
                    
                    // Check if visible from above (air all the way through, no floors blocking)
                    bool visible = true;
                    for (int zCheck = zDepth + 1; zCheck <= z; zCheck++) {
                        if (CellIsSolid(grid[zCheck][y][x]) || HAS_FLOOR(x, y, zCheck)) {
                            visible = false;
                            break;
                        }
                    }
                    if (!visible) continue;
                    
                    int surface = GET_CELL_SURFACE(x, y, zDepth);
                    if (surface == SURFACE_BARE) continue;
                    
                    int sprite;
                    switch (surface) {
                        case SURFACE_TALL_GRASS: sprite = SPRITE_grass_tall; break;
                        case SURFACE_GRASS:      sprite = SPRITE_grass;      break;
                        case SURFACE_TRAMPLED:   sprite = SPRITE_grass_trampled; break;
                        default: continue;
                    }
                    
                    Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
                    Rectangle src = SpriteGetRect(sprite);
                    DrawTexturePro(atlas, src, dest, (Vector2){0,0}, 0, tint);
                }
            }
        }
        
        // DF mode: grass overlay comes from z-1 (the floor you're standing ON)
        if (z <= 0) return;  // No floor below z=0
        
        int zBelow = z - 1;
        for (int y = minY; y < maxY; y++) {
            for (int x = minX; x < maxX; x++) {
                // Only draw overlay where floor is dirt and current cell is empty (air) or ramp
                if (grid[zBelow][y][x] != CELL_DIRT) continue;
                CellType cellHere = grid[z][y][x];
                // Allow grass under air and ramps, skip walls/ladders/etc.
                if (cellHere != CELL_AIR && !CellIsRamp(cellHere)) continue;
                if (HAS_FLOOR(x, y, z)) continue;    // Don't draw grass under constructed floors
                
                int surface = GET_CELL_SURFACE(x, y, zBelow);
                if (surface == SURFACE_BARE) continue;
                
                int sprite;
                switch (surface) {
                    case SURFACE_TALL_GRASS: sprite = SPRITE_grass_tall; break;
                    case SURFACE_GRASS:      sprite = SPRITE_grass;      break;
                    case SURFACE_TRAMPLED:   sprite = SPRITE_grass_trampled; break;
                    default: continue;
                }
                
                Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
                Rectangle src = SpriteGetRect(sprite);
                DrawTexturePro(atlas, src, dest, (Vector2){0,0}, 0, WHITE);
            }
        }
    } else {
        // Legacy mode: grass overlay on current z
        for (int y = minY; y < maxY; y++) {
            for (int x = minX; x < maxX; x++) {
                if (grid[z][y][x] != CELL_DIRT) continue;
                
                int surface = GET_CELL_SURFACE(x, y, z);
                if (surface == SURFACE_BARE) continue;
                
                int sprite;
                switch (surface) {
                    case SURFACE_TALL_GRASS: sprite = SPRITE_grass_tall; break;
                    case SURFACE_GRASS:      sprite = SPRITE_grass;      break;
                    case SURFACE_TRAMPLED:   sprite = SPRITE_grass_trampled; break;
                    default: continue;
                }
                
                Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
                Rectangle src = SpriteGetRect(sprite);
                DrawTexturePro(atlas, src, dest, (Vector2){0,0}, 0, WHITE);
            }
        }
    }
}

void DrawWater(void) {
    float size = CELL_SIZE * zoom;
    int z = currentViewZ;

    int minX, minY, maxX, maxY;
    GetVisibleCellRange(size, &minX, &minY, &maxX, &maxY);

    for (int y = minY; y < maxY; y++) {
        for (int x = minX; x < maxX; x++) {
            int level = GetWaterLevel(x, y, z);
            if (level <= 0) continue;

            // Alpha based on water level (deeper = more opaque)
            int alpha = 80 + (level * 15);  // 80-230 range
            if (alpha > 230) alpha = 230;
            
            Color waterColor = (Color){30, 100, 200, alpha};
            
            // Draw water overlay
            Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
            DrawRectangleRec(dest, waterColor);
            
            // Mark sources with a brighter center
            if (waterGrid[z][y][x].isSource) {
                float inset = size * 0.3f;
                Rectangle inner = {dest.x + inset, dest.y + inset, size - inset*2, size - inset*2};
                DrawRectangleRec(inner, (Color){100, 180, 255, 200});
            }
            
            // Mark drains with a dark center
            if (waterGrid[z][y][x].isDrain) {
                float inset = size * 0.3f;
                Rectangle inner = {dest.x + inset, dest.y + inset, size - inset*2, size - inset*2};
                DrawRectangleRec(inner, (Color){20, 40, 80, 200});
            }
        }
    }
}

void DrawFire(void) {
    float size = CELL_SIZE * zoom;
    int z = currentViewZ;

    int minX, minY, maxX, maxY;
    GetVisibleCellRange(size, &minX, &minY, &maxX, &maxY);

    for (int y = minY; y < maxY; y++) {
        for (int x = minX; x < maxX; x++) {
            // In standard mode, fire on the floor (z-1) should be visible when viewing z
            int fireZ = z;
            if (!g_legacyWalkability && z > 0 && grid[z][y][x] == CELL_AIR && CellIsSolid(grid[z-1][y][x])) {
                fireZ = z - 1;
            }
            
            FireCell* cell = &fireGrid[fireZ][y][x];
            int level = cell->level;
            
            // Draw burned cells with a darker tint
            if (level == 0 && HAS_CELL_FLAG(x, y, fireZ, CELL_FLAG_BURNED)) {
                Color burnedColor = (Color){40, 30, 20, 100};
                Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
                DrawRectangleRec(dest, burnedColor);
                continue;
            }
            
            if (level <= 0) continue;

            // Color based on fire level (darker orange to bright yellow)
            int r, g, b, alpha;
            if (level <= 2) {
                r = 180; g = 60; b = 20;
                alpha = 120 + level * 20;
            } else if (level <= 4) {
                r = 220; g = 100; b = 30;
                alpha = 150 + (level - 2) * 15;
            } else {
                r = 255; g = 150 + (level - 4) * 20; b = 50;
                alpha = 180 + (level - 4) * 15;
            }
            if (alpha > 230) alpha = 230;
            
            Color fireColor = (Color){r, g, b, alpha};
            
            // Draw fire overlay
            Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
            DrawRectangleRec(dest, fireColor);
            
            // Mark sources with a brighter center
            if (cell->isSource) {
                float inset = size * 0.3f;
                Rectangle inner = {dest.x + inset, dest.y + inset, size - inset*2, size - inset*2};
                DrawRectangleRec(inner, (Color){255, 220, 100, 200});
            }
        }
    }
}

void DrawSmoke(void) {
    float size = CELL_SIZE * zoom;
    int z = currentViewZ;

    int minX, minY, maxX, maxY;
    GetVisibleCellRange(size, &minX, &minY, &maxX, &maxY);

    for (int y = minY; y < maxY; y++) {
        for (int x = minX; x < maxX; x++) {
            int level = GetSmokeLevel(x, y, z);
            if (level <= 0) continue;

            int alpha = 30 + (level * 25);  // 55-205 range
            if (alpha > 205) alpha = 205;
            
            Color smokeColor = (Color){80, 80, 90, alpha};
            
            // Draw smoke overlay
            Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
            DrawRectangleRec(dest, smokeColor);
        }
    }
}

void DrawSteam(void) {
    float size = CELL_SIZE * zoom;
    int z = currentViewZ;

    int minX, minY, maxX, maxY;
    GetVisibleCellRange(size, &minX, &minY, &maxX, &maxY);

    for (int y = minY; y < maxY; y++) {
        for (int x = minX; x < maxX; x++) {
            int level = GetSteamLevel(x, y, z);
            if (level <= 0) continue;

            // Steam is white/light gray, more translucent than smoke
            int alpha = 40 + (level * 20);  // 60-180 range
            if (alpha > 180) alpha = 180;
            
            Color steamColor = (Color){220, 220, 230, alpha};
            
            // Draw steam overlay
            Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
            DrawRectangleRec(dest, steamColor);
        }
    }
}

void DrawTemperature(void) {
    // Auto-show when placing heat or cold
    bool autoShow = (inputAction == ACTION_SANDBOX_HEAT || inputAction == ACTION_SANDBOX_COLD);
    if (!showTemperatureOverlay && !autoShow) return;
    
    float size = CELL_SIZE * zoom;
    int z = currentViewZ;

    int minX, minY, maxX, maxY;
    GetVisibleCellRange(size, &minX, &minY, &maxX, &maxY);

    int ambient = GetAmbientTemperature(z);

    for (int y = minY; y < maxY; y++) {
        for (int x = minX; x < maxX; x++) {
            int temp = GetTemperature(x, y, z);
            
            // Skip cells at ambient (neutral) - don't draw overlay
            int diff = temp - ambient;
            if (diff > -10 && diff < 10) continue;
            
            // Color gradient: Blue (cold) -> White (neutral) -> Red (hot)
            // 0 = deep freeze (blue), 128 = neutral (transparent), 255 = extreme heat (red)
            int r, g, b, alpha;
            
            if (temp < ambient) {
                // Cold: blue tint
                // The colder, the more blue
                int coldness = ambient - temp;  // 0 to ~128
                r = 50;
                g = 100 + (coldness > 50 ? 50 : coldness);
                b = 200 + (coldness > 55 ? 55 : coldness);
                alpha = 40 + coldness;
                if (alpha > 150) alpha = 150;
            } else {
                // Hot: red/orange tint
                int hotness = temp - ambient;  // 0 to ~127
                r = 200 + (hotness > 55 ? 55 : hotness);
                g = 100 - (hotness > 60 ? 60 : hotness);
                b = 50;
                alpha = 40 + hotness;
                if (alpha > 150) alpha = 150;
            }
            
            Color tempColor = (Color){r, g, b, alpha};
            
            Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
            DrawRectangleRec(dest, tempColor);
            
            // Mark heat sources with a bright center
            if (IsHeatSource(x, y, z)) {
                float inset = size * 0.3f;
                Rectangle inner = {dest.x + inset, dest.y + inset, size - inset*2, size - inset*2};
                DrawRectangleRec(inner, (Color){255, 200, 100, 200});
            }
            
            // Mark cold sources with a cyan center
            if (IsColdSource(x, y, z)) {
                float inset = size * 0.3f;
                Rectangle inner = {dest.x + inset, dest.y + inset, size - inset*2, size - inset*2};
                DrawRectangleRec(inner, (Color){100, 200, 255, 200});
            }
        }
    }
}

void DrawFrozenWater(void) {
    float size = CELL_SIZE * zoom;
    int z = currentViewZ;

    int minX, minY, maxX, maxY;
    GetVisibleCellRange(size, &minX, &minY, &maxX, &maxY);

    for (int y = minY; y < maxY; y++) {
        for (int x = minX; x < maxX; x++) {
            if (!IsWaterFrozen(x, y, z)) continue;
            
            // Draw frozen water as light whitish-blue (ice)
            Color iceColor = (Color){200, 230, 255, 180};
            Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
            DrawRectangleRec(dest, iceColor);
        }
    }
}

void DrawChunkBoundaries(void) {
    float cellSize = CELL_SIZE * zoom;
    float chunkPixelsX = chunkWidth * cellSize;
    float chunkPixelsY = chunkHeight * cellSize;
    for (int cy = 0; cy <= chunksY; cy++) {
        Vector2 s = {offset.x, offset.y + cy * chunkPixelsY};
        Vector2 e = {offset.x + chunksX * chunkPixelsX, offset.y + cy * chunkPixelsY};
        DrawLineEx(s, e, 3.0f, RED);
    }
    for (int cx = 0; cx <= chunksX; cx++) {
        Vector2 s = {offset.x + cx * chunkPixelsX, offset.y};
        Vector2 e = {offset.x + cx * chunkPixelsX, offset.y + chunksY * chunkPixelsY};
        DrawLineEx(s, e, 3.0f, RED);
    }
}

void DrawEntrances(void) {
    float size = CELL_SIZE * zoom;
    float ms = size * 0.5f;
    int z = currentViewZ;
    for (int i = 0; i < entranceCount; i++) {
        // Only draw entrances on current z-level (faded) or skip others
        if (entrances[i].z != z) continue;
        float px = offset.x + entrances[i].x * size + (size - ms) / 2;
        float py = offset.y + entrances[i].y * size + (size - ms) / 2;
        DrawRectangle((int)px, (int)py, (int)ms, (int)ms, WHITE);
    }
}

void DrawGraph(void) {
    if (!showGraph) return;
    float size = CELL_SIZE * zoom;
    int z = currentViewZ;
    for (int i = 0; i < graphEdgeCount; i += 2) {
        int e1 = graphEdges[i].from, e2 = graphEdges[i].to;
        // Only draw edges where both entrances are on the current z-level
        if (entrances[e1].z != z && entrances[e2].z != z) continue;
        Vector2 p1 = {offset.x + entrances[e1].x * size + size/2, offset.y + entrances[e1].y * size + size/2};
        Vector2 p2 = {offset.x + entrances[e2].x * size + size/2, offset.y + entrances[e2].y * size + size/2};
        // Fade edges that connect to different z-levels
        Color col = (entrances[e1].z == z && entrances[e2].z == z) ? YELLOW : (Color){255, 255, 0, 80};
        DrawLineEx(p1, p2, 2.0f, col);
    }
}

void DrawPath(void) {
    float size = CELL_SIZE * zoom;
    int z = currentViewZ;

    // Draw start (green) - full opacity on same z, faded on different z
    if (startPos.x >= 0) {
        Color col = (startPos.z == z) ? GREEN : (Color){0, 228, 48, 80};
        DrawRectangle((int)(offset.x + startPos.x * size), (int)(offset.y + startPos.y * size), (int)size, (int)size, col);
    }

    // Draw goal (red) - full opacity on same z, faded on different z
    if (goalPos.x >= 0) {
        Color col = (goalPos.z == z) ? RED : (Color){230, 41, 55, 80};
        DrawRectangle((int)(offset.x + goalPos.x * size), (int)(offset.y + goalPos.y * size), (int)size, (int)size, col);
    }

    // Draw path - full opacity on same z, faded on different z
    for (int i = 0; i < pathLength; i++) {
        float px = offset.x + path[i].x * size + size * 0.25f;
        float py = offset.y + path[i].y * size + size * 0.25f;
        Color col = (path[i].z == z) ? BLUE : (Color){0, 121, 241, 80};
        DrawRectangle((int)px, (int)py, (int)(size * 0.5f), (int)(size * 0.5f), col);
    }
}

void DrawAgents(void) {
    float size = CELL_SIZE * zoom;
    int z = currentViewZ;
    for (int i = 0; i < agentCount; i++) {
        Agent* a = &agents[i];
        if (!a->active) continue;

        // Draw start (circle) - only if on current z-level
        if (a->start.z == z) {
            float sx = offset.x + a->start.x * size + size / 2;
            float sy = offset.y + a->start.y * size + size / 2;
            DrawCircle((int)sx, (int)sy, size * 0.4f, a->color);
        }

        // Draw goal (square outline) - only if on current z-level
        if (a->goal.z == z) {
            float gx = offset.x + a->goal.x * size;
            float gy = offset.y + a->goal.y * size;
            DrawRectangleLines((int)gx, (int)gy, (int)size, (int)size, a->color);
        }

        // Draw path - only segments on current z-level
        for (int j = 0; j < a->pathLength; j++) {
            if (a->path[j].z != z) continue;
            float px = offset.x + a->path[j].x * size + size * 0.35f;
            float py = offset.y + a->path[j].y * size + size * 0.35f;
            DrawRectangle((int)px, (int)py, (int)(size * 0.3f), (int)(size * 0.3f), a->color);
        }
    }
}

void DrawMovers(void) {
    float size = CELL_SIZE * zoom;
    int viewZ = currentViewZ;

    for (int i = 0; i < moverCount; i++) {
        Mover* m = &movers[i];
        if (!m->active) continue;

        // Only draw movers on the current z-level
        if ((int)m->z != viewZ) continue;

        // Screen position
        float sx = offset.x + m->x * zoom;
        float sy = offset.y + m->y * zoom;
        if (usePixelPerfectMovers) {
            sx = roundf(sx);
            sy = roundf(sy);
        }

        // Choose color based on mover state or debug mode
        Color moverColor;
        if (showStuckDetection) {
            if (m->timeWithoutProgress > STUCK_REPATH_TIME) {
                moverColor = MAGENTA;
            } else if (m->timeWithoutProgress > STUCK_REPATH_TIME * 0.5f) {
                moverColor = RED;
            } else if (m->timeWithoutProgress > STUCK_CHECK_INTERVAL) {
                moverColor = ORANGE;
            } else {
                moverColor = GREEN;
            }
        } else if (showKnotDetection) {
            if (m->timeNearWaypoint > KNOT_STUCK_TIME) {
                moverColor = RED;
            } else if (m->timeNearWaypoint > KNOT_STUCK_TIME * 0.5f) {
                moverColor = ORANGE;
            } else if (m->timeNearWaypoint > 0.0f) {
                moverColor = YELLOW;
            } else {
                moverColor = GREEN;
            }
        } else if (showOpenArea) {
            bool open = IsMoverInOpenArea(m->x, m->y, (int)m->z);
            moverColor = open ? SKYBLUE : MAGENTA;
        } else if (showNeighborCounts) {
            int neighbors = QueryMoverNeighbors(m->x, m->y, MOVER_AVOID_RADIUS, i, NULL, NULL);
            if (neighbors == 0) {
                moverColor = GREEN;
            } else if (neighbors <= 3) {
                moverColor = YELLOW;
            } else if (neighbors <= 6) {
                moverColor = ORANGE;
            } else {
                moverColor = RED;
            }
        } else if (m->repathCooldown > 0 && m->pathLength == 0) {
            moverColor = ORANGE;
        } else if (m->pathLength == 0) {
            moverColor = RED;
        } else if (m->needsRepath) {
            moverColor = YELLOW;
        } else {
            moverColor = WHITE;
        }

        // Override color if mover just fell
        if (m->fallTimer > 0) {
            moverColor = BLUE;
        }

        // Draw mover as head sprite with color tint
        float moverSize = size * MOVER_SIZE;
        Rectangle src = SpriteGetRect(SPRITE_head);
        Rectangle dest = { sx - moverSize/2, sy - moverSize/2, moverSize, moverSize };
        DrawTexturePro(atlas, src, dest, (Vector2){0, 0}, 0, moverColor);

        // Draw carried item above mover's head
        Job* moverJob = (m->currentJobId >= 0) ? GetJob(m->currentJobId) : NULL;
        int carryingItem = moverJob ? moverJob->carryingItem : -1;
        if (carryingItem >= 0 && items[carryingItem].active) {
            Item* item = &items[carryingItem];
            int sprite;
            switch (item->type) {
                case ITEM_RED:          sprite = SPRITE_crate_red;    break;
                case ITEM_GREEN:        sprite = SPRITE_crate_green;  break;
                case ITEM_BLUE:         sprite = SPRITE_crate_blue;   break;
                case ITEM_ORANGE:       sprite = SPRITE_crate_orange; break;
                case ITEM_STONE_BLOCKS: sprite = SPRITE_stone_block;  break;
                default:                sprite = SPRITE_apple;        break;
            }
            float itemSize = size * ITEM_SIZE_CARRIED;
            float itemY = sy - moverSize/2 - itemSize + moverSize * 0.2f;
            Rectangle itemSrc = SpriteGetRect(sprite);
            Rectangle itemDest = { sx - itemSize/2, itemY, itemSize, itemSize };
            DrawTexturePro(atlas, itemSrc, itemDest, (Vector2){0, 0}, 0, WHITE);
        }
    }

    // Draw mover paths in separate loop for profiling
    if (showMoverPaths) {
        PROFILE_BEGIN(MoverPaths);
        for (int i = 0; i < moverCount; i++) {
            Mover* m = &movers[i];
            if (!m->active || m->pathIndex < 0) continue;
            if ((int)m->z != viewZ) continue;

            float sx = offset.x + m->x * zoom;
            float sy = offset.y + m->y * zoom;
            Color color = moverRenderData[i].color;

            // Line to next waypoint (if on same z)
            Point next = m->path[m->pathIndex];
            if (next.z == viewZ) {
                float tx = offset.x + (next.x * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                float ty = offset.y + (next.y * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                DrawLineEx((Vector2){sx, sy}, (Vector2){tx, ty}, 2.0f, color);
            }

            // Rest of path
            for (int j = m->pathIndex; j > 0; j--) {
                if (m->path[j].z != viewZ || m->path[j-1].z != viewZ) continue;
                float px1 = offset.x + (m->path[j].x * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                float py1 = offset.y + (m->path[j].y * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                float px2 = offset.x + (m->path[j-1].x * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                float py2 = offset.y + (m->path[j-1].y * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                DrawLineEx((Vector2){px1, py1}, (Vector2){px2, py2}, 1.0f, Fade(color, 0.4f));
            }
        }
        PROFILE_END(MoverPaths);
    }
    
    // Draw hovered mover's path (always, even if showMoverPaths is off)
    if (hoveredMover >= 0 && !showMoverPaths) {
        Mover* m = &movers[hoveredMover];
        if (m->active && m->pathIndex >= 0) {
            float sx = offset.x + m->x * zoom;
            float sy = offset.y + m->y * zoom;
            Color pathColor = YELLOW;

            Point next = m->path[m->pathIndex];
            if (next.z == viewZ) {
                float tx = offset.x + (next.x * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                float ty = offset.y + (next.y * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                DrawLineEx((Vector2){sx, sy}, (Vector2){tx, ty}, 2.0f, pathColor);
            }

            for (int j = m->pathIndex; j > 0; j--) {
                if (m->path[j].z != viewZ || m->path[j-1].z != viewZ) continue;
                float px1 = offset.x + (m->path[j].x * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                float py1 = offset.y + (m->path[j].y * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                float px2 = offset.x + (m->path[j-1].x * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                float py2 = offset.y + (m->path[j-1].y * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                DrawLineEx((Vector2){px1, py1}, (Vector2){px2, py2}, 2.0f, Fade(pathColor, 0.6f));
            }
        }
    }
}

void DrawItems(void) {
    float size = CELL_SIZE * zoom;
    int viewZ = currentViewZ;

    for (int i = 0; i < MAX_ITEMS; i++) {
        Item* item = &items[i];
        if (!item->active) continue;
        if (item->state == ITEM_CARRIED) continue;
        if (item->state == ITEM_IN_STOCKPILE) continue;

        if ((int)item->z != viewZ) continue;

        float sx = offset.x + item->x * zoom;
        float sy = offset.y + item->y * zoom;

        int sprite;
        switch (item->type) {
            case ITEM_RED:          sprite = SPRITE_crate_red;    break;
            case ITEM_GREEN:        sprite = SPRITE_crate_green;  break;
            case ITEM_BLUE:         sprite = SPRITE_crate_blue;   break;
            case ITEM_ORANGE:       sprite = SPRITE_crate_orange; break;
            case ITEM_STONE_BLOCKS: sprite = SPRITE_stone_block;  break;
            default:                sprite = SPRITE_apple;        break;
        }

        float itemSize = size * ITEM_SIZE_GROUND;
        Rectangle src = SpriteGetRect(sprite);
        Rectangle dest = { sx - itemSize/2, sy - itemSize/2, itemSize, itemSize };

        Color tint = (item->reservedBy >= 0) ? (Color) { 200, 200, 200, 255 } : WHITE;
        DrawTexturePro(atlas, src, dest, (Vector2){0, 0}, 0, tint);
    }
}

void DrawGatherZones(void) {
    float size = CELL_SIZE * zoom;
    int viewZ = currentViewZ;

    for (int i = 0; i < MAX_GATHER_ZONES; i++) {
        GatherZone* gz = &gatherZones[i];
        if (!gz->active) continue;
        if (gz->z != viewZ) continue;

        float sx = offset.x + gz->x * size;
        float sy = offset.y + gz->y * size;
        float w = gz->width * size;
        float h = gz->height * size;

        DrawRectangle((int)sx, (int)sy, (int)w, (int)h, (Color){255, 180, 50, 40});
        DrawRectangleLinesEx((Rectangle){sx, sy, w, h}, 2.0f, (Color){255, 180, 50, 180});
    }
}

void DrawStockpileTiles(void) {
    float size = CELL_SIZE * zoom;
    int viewZ = currentViewZ;

    for (int i = 0; i < MAX_STOCKPILES; i++) {
        Stockpile* sp = &stockpiles[i];
        if (!sp->active) continue;
        if (sp->z != viewZ) continue;

        for (int dy = 0; dy < sp->height; dy++) {
            for (int dx = 0; dx < sp->width; dx++) {
                int slotIdx = dy * sp->width + dx;
                if (!sp->cells[slotIdx]) continue;

                int gx = sp->x + dx;
                int gy = sp->y + dy;

                float sx = offset.x + gx * size;
                float sy = offset.y + gy * size;

                Rectangle src = SpriteGetRect(SPRITE_stockpile);
                Rectangle dest = { sx, sy, size, size };
                DrawTexturePro(atlas, src, dest, (Vector2){0, 0}, 0, WHITE);

                if (i == hoveredStockpile) {
                    float pulse = (sinf(GetTime() * 4.0f) + 1.0f) * 0.5f;
                    unsigned char alpha = (unsigned char)(40 + pulse * 60);
                    DrawRectangle((int)sx, (int)sy, (int)size, (int)size, (Color){100, 255, 100, alpha});
                }
            }
        }
    }
}

void DrawStockpileItems(void) {
    float size = CELL_SIZE * zoom;
    int viewZ = currentViewZ;

    for (int i = 0; i < MAX_STOCKPILES; i++) {
        Stockpile* sp = &stockpiles[i];
        if (!sp->active) continue;
        if (sp->z != viewZ) continue;

        for (int dy = 0; dy < sp->height; dy++) {
            for (int dx = 0; dx < sp->width; dx++) {
                int slotIdx = dy * sp->width + dx;
                if (!sp->cells[slotIdx]) continue;

                int count = sp->slotCounts[slotIdx];
                if (count <= 0) continue;

                int gx = sp->x + dx;
                int gy = sp->y + dy;

                float sx = offset.x + gx * size;
                float sy = offset.y + gy * size;

                ItemType type = sp->slotTypes[slotIdx];
                int sprite;
                switch (type) {
                    case ITEM_RED:          sprite = SPRITE_crate_red;    break;
                    case ITEM_GREEN:        sprite = SPRITE_crate_green;  break;
                    case ITEM_BLUE:         sprite = SPRITE_crate_blue;   break;
                    case ITEM_ORANGE:       sprite = SPRITE_crate_orange; break;
                    case ITEM_STONE_BLOCKS: sprite = SPRITE_stone_block;  break;
                    default:                sprite = SPRITE_apple;        break;
                }

                int visibleCount = count > 5 ? 5 : count;
                float itemSize = size * ITEM_SIZE_STOCKPILE;
                float stackOffset = size * 0.08f;

                for (int s = 0; s < visibleCount; s++) {
                    float itemX = sx + size * 0.5f - itemSize * 0.5f - s * stackOffset;
                    float itemY = sy + size * 0.5f - itemSize * 0.5f - s * stackOffset;
                    Rectangle srcItem = SpriteGetRect(sprite);
                    Rectangle destItem = { itemX, itemY, itemSize, itemSize };
                    DrawTexturePro(atlas, srcItem, destItem, (Vector2){0, 0}, 0, WHITE);
                }
            }
        }
    }
}

void DrawWorkshops(void) {
    float size = CELL_SIZE * zoom;
    int viewZ = currentViewZ;

    for (int i = 0; i < MAX_WORKSHOPS; i++) {
        Workshop* ws = &workshops[i];
        if (!ws->active) continue;
        if (ws->z != viewZ) continue;

        // Draw workshop footprint based on template
        for (int dy = 0; dy < ws->height; dy++) {
            for (int dx = 0; dx < ws->width; dx++) {
                int gx = ws->x + dx;
                int gy = ws->y + dy;

                float sx = offset.x + gx * size;
                float sy = offset.y + gy * size;

                char tile = ws->template[dy * ws->width + dx];
                Color tint = WHITE;
                
                switch (tile) {
                    case WT_BLOCK:  // Machinery - dark brown tint
                        tint = (Color){140, 100, 60, 255};
                        break;
                    case WT_WORK:   // Work tile - green tint
                        tint = (Color){150, 220, 150, 255};
                        break;
                    case WT_OUTPUT: // Output tile - blue tint
                        tint = (Color){150, 180, 220, 255};
                        break;
                    case WT_FLOOR:  // Floor - light brown tint
                    default:
                        tint = (Color){200, 180, 140, 255};
                        break;
                }

                Rectangle src = SpriteGetRect(SPRITE_generic);
                Rectangle dest = { sx, sy, size, size };
                DrawTexturePro(atlas, src, dest, (Vector2){0, 0}, 0, tint);
            }
        }

        // Draw craft progress bar if crafter is working
        if (ws->assignedCrafter >= 0) {
            // Find the job to get progress
            Mover* m = &movers[ws->assignedCrafter];
            if (m->currentJobId >= 0) {
                Job* job = GetJob(m->currentJobId);
                if (job && job->type == JOBTYPE_CRAFT && job->step == CRAFT_STEP_WORKING) {
                    float sx = offset.x + ws->x * size;
                    float sy = offset.y + ws->y * size - 6;
                    float barWidth = ws->width * size;
                    float progress = job->progress;
                    
                    DrawRectangle((int)sx, (int)sy, (int)barWidth, 4, DARKGRAY);
                    DrawRectangle((int)sx, (int)sy, (int)(barWidth * progress), 4, YELLOW);
                }
            }
        }
    }
    
    // Draw path of assigned crafter when hovering over a workshop
    if (hoveredWorkshop >= 0 && hoveredWorkshop < MAX_WORKSHOPS) {
        Workshop* ws = &workshops[hoveredWorkshop];
        if (ws->active && ws->assignedCrafter >= 0) {
            Mover* m = &movers[ws->assignedCrafter];
            if (m->active && m->pathIndex >= 0) {
                Color pathColor = YELLOW;
                
                // Draw line from mover to next waypoint
                float msx = offset.x + m->x * zoom;
                float msy = offset.y + m->y * zoom;
                Point next = m->path[m->pathIndex];
                if (next.z == viewZ) {
                    float tx = offset.x + (next.x * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                    float ty = offset.y + (next.y * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                    DrawLineEx((Vector2){msx, msy}, (Vector2){tx, ty}, 2.0f, pathColor);
                }
                
                // Draw rest of path
                for (int j = m->pathIndex; j > 0; j--) {
                    if (m->path[j].z != viewZ || m->path[j-1].z != viewZ) continue;
                    float px1 = offset.x + (m->path[j].x * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                    float py1 = offset.y + (m->path[j].y * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                    float px2 = offset.x + (m->path[j-1].x * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                    float py2 = offset.y + (m->path[j-1].y * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                    DrawLineEx((Vector2){px1, py1}, (Vector2){px2, py2}, 2.0f, Fade(pathColor, 0.6f));
                }
                
                // Draw mover position marker
                DrawCircle((int)msx, (int)msy, 4.0f * zoom, pathColor);
            }
        }
    }
}

void DrawHaulDestinations(void) {
    float size = CELL_SIZE * zoom;
    int viewZ = currentViewZ;

    for (int i = 0; i < activeJobCount; i++) {
        int jobIdx = activeJobList[i];
        Job* job = &jobs[jobIdx];
        if (job->type != JOBTYPE_HAUL) continue;
        if (job->targetStockpile < 0) continue;
        
        Stockpile* sp = &stockpiles[job->targetStockpile];
        if (!sp->active) continue;
        if (sp->z != viewZ) continue;
        
        float sx = offset.x + job->targetSlotX * size;
        float sy = offset.y + job->targetSlotY * size;
        
        Rectangle src = SpriteGetRect(SPRITE_stockpile);
        Rectangle dest = { sx, sy, size, size };
        DrawTexturePro(atlas, src, dest, (Vector2){0, 0}, 0, (Color){255, 200, 100, 180});
    }
}

void DrawMiningDesignations(void) {
    // Early exit if no designations
    if (activeDesignationCount == 0) return;
    
    float size = CELL_SIZE * zoom;
    int viewZ = currentViewZ;

    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            Designation* d = GetDesignation(x, y, viewZ);
            if (!d) continue;
            
            // Mine designation: cyan
            if (d->type == DESIGNATION_MINE) {
                float sx = offset.x + x * size;
                float sy = offset.y + y * size;

                Rectangle src = SpriteGetRect(SPRITE_stockpile);
                Rectangle dest = { sx, sy, size, size };
                DrawTexturePro(atlas, src, dest, (Vector2){0, 0}, 0, (Color){100, 220, 255, 200});

                if (d->progress > 0.0f) {
                    float barWidth = size * 0.8f;
                    float barHeight = 4.0f;
                    float barX = sx + size * 0.1f;
                    float barY = sy + size - 8.0f;
                    DrawRectangle((int)barX, (int)barY, (int)barWidth, (int)barHeight, DARKGRAY);
                    DrawRectangle((int)barX, (int)barY, (int)(barWidth * d->progress), (int)barHeight, SKYBLUE);
                }
            }
            // Channel designation: pink/magenta
            else if (d->type == DESIGNATION_CHANNEL) {
                float sx = offset.x + x * size;
                float sy = offset.y + y * size;

                Rectangle src = SpriteGetRect(SPRITE_stockpile);
                Rectangle dest = { sx, sy, size, size };
                DrawTexturePro(atlas, src, dest, (Vector2){0, 0}, 0, (Color){255, 150, 200, 200});

                if (d->progress > 0.0f) {
                    float barWidth = size * 0.8f;
                    float barHeight = 4.0f;
                    float barX = sx + size * 0.1f;
                    float barY = sy + size - 8.0f;
                    DrawRectangle((int)barX, (int)barY, (int)barWidth, (int)barHeight, DARKGRAY);
                    DrawRectangle((int)barX, (int)barY, (int)(barWidth * d->progress), (int)barHeight, MAGENTA);
                }
            }
            // Remove floor designation: yellow/gold
            else if (d->type == DESIGNATION_REMOVE_FLOOR) {
                float sx = offset.x + x * size;
                float sy = offset.y + y * size;

                Rectangle src = SpriteGetRect(SPRITE_stockpile);
                Rectangle dest = { sx, sy, size, size };
                DrawTexturePro(atlas, src, dest, (Vector2){0, 0}, 0, (Color){255, 220, 100, 200});

                if (d->progress > 0.0f) {
                    float barWidth = size * 0.8f;
                    float barHeight = 4.0f;
                    float barX = sx + size * 0.1f;
                    float barY = sy + size - 8.0f;
                    DrawRectangle((int)barX, (int)barY, (int)barWidth, (int)barHeight, DARKGRAY);
                    DrawRectangle((int)barX, (int)barY, (int)(barWidth * d->progress), (int)barHeight, GOLD);
                }
            }
            // Remove ramp designation: cyan/teal
            else if (d->type == DESIGNATION_REMOVE_RAMP) {
                float sx = offset.x + x * size;
                float sy = offset.y + y * size;

                Rectangle src = SpriteGetRect(SPRITE_stockpile);
                Rectangle dest = { sx, sy, size, size };
                DrawTexturePro(atlas, src, dest, (Vector2){0, 0}, 0, (Color){100, 220, 220, 200});

                if (d->progress > 0.0f) {
                    float barWidth = size * 0.8f;
                    float barHeight = 4.0f;
                    float barX = sx + size * 0.1f;
                    float barY = sy + size - 8.0f;
                    DrawRectangle((int)barX, (int)barY, (int)barWidth, (int)barHeight, DARKGRAY);
                    DrawRectangle((int)barX, (int)barY, (int)(barWidth * d->progress), (int)barHeight, (Color){50, 200, 200, 255});
                }
            }
        }
    }

    // Draw active mine jobs (orange overlay for assigned)
    for (int i = 0; i < activeJobCount; i++) {
        int jobIdx = activeJobList[i];
        Job* job = &jobs[jobIdx];
        if (job->type != JOBTYPE_MINE) continue;
        if (job->targetMineZ != viewZ) continue;

        float sx = offset.x + job->targetMineX * size;
        float sy = offset.y + job->targetMineY * size;

        Rectangle src = SpriteGetRect(SPRITE_stockpile);
        Rectangle dest = { sx, sy, size, size };
        DrawTexturePro(atlas, src, dest, (Vector2){0, 0}, 0, (Color){255, 200, 100, 180});
    }
    
    // Draw active channel jobs (yellow/orange overlay for assigned)
    for (int i = 0; i < activeJobCount; i++) {
        int jobIdx = activeJobList[i];
        Job* job = &jobs[jobIdx];
        if (job->type != JOBTYPE_CHANNEL) continue;
        if (job->targetMineZ != viewZ) continue;

        float sx = offset.x + job->targetMineX * size;
        float sy = offset.y + job->targetMineY * size;

        Rectangle src = SpriteGetRect(SPRITE_stockpile);
        Rectangle dest = { sx, sy, size, size };
        DrawTexturePro(atlas, src, dest, (Vector2){0, 0}, 0, (Color){255, 180, 150, 180});
    }
    
    // Draw active remove floor jobs (gold/orange overlay for assigned)
    for (int i = 0; i < activeJobCount; i++) {
        int jobIdx = activeJobList[i];
        Job* job = &jobs[jobIdx];
        if (job->type != JOBTYPE_REMOVE_FLOOR) continue;
        if (job->targetMineZ != viewZ) continue;

        float sx = offset.x + job->targetMineX * size;
        float sy = offset.y + job->targetMineY * size;

        Rectangle src = SpriteGetRect(SPRITE_stockpile);
        Rectangle dest = { sx, sy, size, size };
        DrawTexturePro(atlas, src, dest, (Vector2){0, 0}, 0, (Color){255, 200, 80, 180});
    }
    
    // Draw active remove ramp jobs (cyan/teal overlay for assigned)
    for (int i = 0; i < activeJobCount; i++) {
        int jobIdx = activeJobList[i];
        Job* job = &jobs[jobIdx];
        if (job->type != JOBTYPE_REMOVE_RAMP) continue;
        if (job->targetMineZ != viewZ) continue;

        float sx = offset.x + job->targetMineX * size;
        float sy = offset.y + job->targetMineY * size;

        Rectangle src = SpriteGetRect(SPRITE_stockpile);
        Rectangle dest = { sx, sy, size, size };
        DrawTexturePro(atlas, src, dest, (Vector2){0, 0}, 0, (Color){80, 200, 200, 180});
    }
    
    // Draw path of assigned mover when hovering over a designation
    if (hoveredDesignationX >= 0) {
        Designation* d = GetDesignation(hoveredDesignationX, hoveredDesignationY, hoveredDesignationZ);
        if (d && d->assignedMover >= 0) {
            Mover* m = &movers[d->assignedMover];
            if (m->active && m->pathIndex >= 0) {
                Color pathColor = ORANGE;
                
                // Draw line from mover to next waypoint
                float msx = offset.x + m->x * zoom;
                float msy = offset.y + m->y * zoom;
                Point next = m->path[m->pathIndex];
                if (next.z == viewZ) {
                    float tx = offset.x + (next.x * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                    float ty = offset.y + (next.y * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                    DrawLineEx((Vector2){msx, msy}, (Vector2){tx, ty}, 2.0f, pathColor);
                }
                
                // Draw rest of path
                for (int j = m->pathIndex; j > 0; j--) {
                    if (m->path[j].z != viewZ || m->path[j-1].z != viewZ) continue;
                    float px1 = offset.x + (m->path[j].x * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                    float py1 = offset.y + (m->path[j].y * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                    float px2 = offset.x + (m->path[j-1].x * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                    float py2 = offset.y + (m->path[j-1].y * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
                    DrawLineEx((Vector2){px1, py1}, (Vector2){px2, py2}, 2.0f, Fade(pathColor, 0.6f));
                }
                
                // Draw mover position marker
                DrawCircle((int)msx, (int)msy, 4.0f * zoom, pathColor);
            }
        }
    }
}

void DrawBlueprints(void) {
    float size = CELL_SIZE * zoom;
    int viewZ = currentViewZ;

    for (int i = 0; i < MAX_BLUEPRINTS; i++) {
        Blueprint* bp = &blueprints[i];
        if (!bp->active || bp->z != viewZ) continue;

        float sx = offset.x + bp->x * size;
        float sy = offset.y + bp->y * size;

        Color tint;
        if (bp->state == BLUEPRINT_AWAITING_MATERIALS) {
            tint = (Color){100, 150, 255, 200};
        } else if (bp->state == BLUEPRINT_READY_TO_BUILD) {
            tint = (Color){100, 220, 255, 200};
        } else {
            tint = (Color){100, 255, 150, 200};
        }

        Rectangle src = SpriteGetRect(SPRITE_stockpile);
        Rectangle dest = { sx, sy, size, size };
        DrawTexturePro(atlas, src, dest, (Vector2){0, 0}, 0, tint);

        if (bp->state == BLUEPRINT_BUILDING && bp->progress > 0.0f) {
            float barWidth = size * 0.8f;
            float barHeight = 4.0f;
            float barX = sx + size * 0.1f;
            float barY = sy + size - 8.0f;
            DrawRectangle((int)barX, (int)barY, (int)barWidth, (int)barHeight, DARKGRAY);
            DrawRectangle((int)barX, (int)barY, (int)(barWidth * bp->progress), (int)barHeight, GREEN);
        }

        if (bp->state == BLUEPRINT_AWAITING_MATERIALS) {
            const char* text = TextFormat("%d/%d", bp->deliveredMaterials, bp->requiredMaterials);
            int textW = MeasureTextUI(text, 10);
            DrawTextShadow(text, (int)(sx + size/2 - textW/2), (int)(sy + 2), 10, WHITE);
        }
    }

    for (int i = 0; i < activeJobCount; i++) {
        int jobIdx = activeJobList[i];
        Job* job = &jobs[jobIdx];
        if (job->type != JOBTYPE_HAUL_TO_BLUEPRINT && job->type != JOBTYPE_BUILD) continue;
        if (job->targetBlueprint < 0 || job->targetBlueprint >= MAX_BLUEPRINTS) continue;
        
        Blueprint* bp = &blueprints[job->targetBlueprint];
        if (!bp->active || bp->z != viewZ) continue;

        float sx = offset.x + bp->x * size;
        float sy = offset.y + bp->y * size;

        Rectangle src = SpriteGetRect(SPRITE_stockpile);
        Rectangle dest = { sx, sy, size, size };
        DrawTexturePro(atlas, src, dest, (Vector2){0, 0}, 0, (Color){255, 200, 100, 180});
    }
}
