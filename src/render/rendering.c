// render/rendering.c - Core rendering functions
#include "../game_state.h"
#include "../world/cell_defs.h"
#include "../world/material.h"
#include "../world/designations.h"
#include "../core/input_mode.h"
#include "../core/time.h"
#include "../entities/workshops.h"
#include "../entities/furniture.h"
#include "../entities/item_defs.h"
#include "../simulation/floordirt.h"
#include "../simulation/lighting.h"
#include "../simulation/plants.h"
#include "../core/sim_manager.h"
#include <math.h>

#define MAX_VISIBLE_DEPTH    9
#define SOURCE_MARKER_INSET  0.3f
#define RAMP_OVERLAY_ALPHA   96

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

static void DrawLineToTile(float msx, float msy, int tx, int ty, int tz, Color color) {
    if (tz != currentViewZ) return;
    float ex = offset.x + (tx * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
    float ey = offset.y + (ty * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
    DrawLineEx((Vector2){msx, msy}, (Vector2){ex, ey}, 2.0f, color);
}

static Color MultiplyColor(Color a, Color b) {
    return (Color){
        (unsigned char)((a.r * b.r) / 255),
        (unsigned char)((a.g * b.g) / 255),
        (unsigned char)((a.b * b.b) / 255),
        (unsigned char)((a.a * b.a) / 255)
    };
}

static Color FireLevelColor(int level) {
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
    return (Color){r, g, b, alpha};
}

static Color FloorDarkenTint(Color base) {
    base.r = (unsigned char)((base.r * 75) / 100);
    base.g = (unsigned char)((base.g * 75) / 100);
    base.b = (unsigned char)((base.b * 75) / 100);
    return base;
}

static Color MaterialTint(MaterialType mat) {
    switch (mat) {
        case MAT_OAK: return (Color){139, 90, 43, 255};
        case MAT_PINE: return (Color){180, 140, 100, 255};
        case MAT_BIRCH: return (Color){220, 210, 190, 255};
        case MAT_WILLOW: return (Color){160, 140, 100, 255};
        default: return WHITE;
    }
}

static int MaterialFloorSprite(MaterialType mat) {
    if (IsWoodMaterial(mat)) return SPRITE_floor_wood;
    int sprite = MaterialSprite(mat);
    return sprite ? sprite : SPRITE_floor;
}

static int FinishSprite(SurfaceFinish finish) {
    switch (finish) {
        case FINISH_SMOOTH: return SPRITE_finish_smooth;
        case FINISH_POLISHED: return SPRITE_finish_polished;
        case FINISH_ENGRAVED: return SPRITE_finish_engraved;
        case FINISH_ROUGH:
        default: return SPRITE_finish_rough;
    }
}

static Color FinishOverlayTint(SurfaceFinish finish, Color base) {
    Color tint = base;
    if (finish == FINISH_ROUGH) {
        tint = (Color){30, 30, 30, base.a};  // "kinda black"
    }
    tint.a = (unsigned char)((tint.a * 20) / 100);
    return tint;
}

static void DrawFinishOverlay(Rectangle dest, SurfaceFinish finish, Color tint) {
    int finishSprite = FinishSprite(finish);
    Rectangle finishSrc = SpriteGetRect(finishSprite);
    DrawTexturePro(atlas, finishSrc, dest, (Vector2){0,0}, 0, FinishOverlayTint(finish, tint));
}

static void DrawSourceMarker(Rectangle dest, float size, Color color) {
    float inset = size * SOURCE_MARKER_INSET;
    Rectangle inner = {dest.x + inset, dest.y + inset, size - inset*2, size - inset*2};
    DrawRectangleRec(inner, color);
}

static bool ItemUsesBorder(ItemType type) {
    return type == ITEM_DIRT || type == ITEM_CLAY || type == ITEM_GRAVEL ||
           type == ITEM_SAND || type == ITEM_PEAT ||
           type == ITEM_BRICKS || type == ITEM_BLOCKS || type == ITEM_LOG;
}

static Color ItemBorderTint(ItemType type) {
    switch (type) {
        case ITEM_BLOCKS:
        case ITEM_BRICKS:
            return (Color){220, 220, 220, 255};  // processed
        case ITEM_LOG:
            return (Color){70, 55, 40, 255};     // raw wood
        default:
            return (Color){30, 30, 30, 255};     // raw materials
    }
}

// Per-item-type tint override (for items with MAT_NONE that need color)
static Color ItemTypeTint(ItemType type) {
    switch (type) {
        case ITEM_BERRIES:      return (Color){220, 50, 50, 255};   // Red like on the bush
        case ITEM_DRIED_BERRIES: return (Color){140, 40, 60, 255};  // Darker, dried
        default:                return WHITE;
    }
}

static void DrawItemWithBorder(int sprite, Rectangle frameDest, Color tint, Color borderTint) {
    Rectangle borderSrc = SpriteGetRect(SPRITE_tile_border);
    DrawTexturePro(atlas, borderSrc, frameDest, (Vector2){0,0}, 0, borderTint);

    float inset = frameDest.width * (1.0f / (float)TILE_SIZE);
    Rectangle itemDest = {
        frameDest.x + inset,
        frameDest.y + inset,
        frameDest.width - inset * 2.0f,
        frameDest.height - inset * 2.0f
    };
    Rectangle src = SpriteGetRect(sprite);
    DrawTexturePro(atlas, src, itemDest, (Vector2){0,0}, 0, tint);
}

static int ItemSpriteForTypeMaterial(ItemType type, uint8_t material) {
    if (type == ITEM_LOG) {
        switch ((MaterialType)material) {
            case MAT_PINE: return SPRITE_tree_trunk_pine;
            case MAT_BIRCH: return SPRITE_tree_trunk_birch;
            case MAT_WILLOW: return SPRITE_tree_trunk_willow;
            case MAT_OAK:
            default: return SPRITE_tree_trunk_oak;
        }
    }
    if (type == ITEM_PLANKS) {
        switch ((MaterialType)material) {
            case MAT_PINE: return SPRITE_tree_planks_pine;
            case MAT_BIRCH: return SPRITE_tree_planks_birch;
            case MAT_WILLOW: return SPRITE_tree_planks_willow;
            case MAT_OAK:
            default: return SPRITE_tree_planks_oak;
        }
    }
    if (type == ITEM_BLOCKS) {
        return MaterialFloorSprite((MaterialType)material);
    }
    if (type == ITEM_SAPLING) {
        switch ((MaterialType)material) {
            case MAT_PINE: return SPRITE_tree_sapling_pine;
            case MAT_BIRCH: return SPRITE_tree_sapling_birch;
            case MAT_WILLOW: return SPRITE_tree_sapling_willow;
            case MAT_OAK:
            default: return SPRITE_tree_sapling_oak;
        }
    }
    if (type == ITEM_LEAVES) {
        switch ((MaterialType)material) {
            case MAT_PINE: return SPRITE_tree_leaves_pine;
            case MAT_BIRCH: return SPRITE_tree_leaves_birch;
            case MAT_WILLOW: return SPRITE_tree_leaves_willow;
            case MAT_OAK:
            default: return SPRITE_tree_leaves_oak;
        }
    }
    if (type == ITEM_BARK) {
        switch ((MaterialType)material) {
            case MAT_PINE: return SPRITE_tree_bark_pine;
            case MAT_BIRCH: return SPRITE_tree_bark_birch;
            case MAT_WILLOW: return SPRITE_tree_bark_willow;
            case MAT_OAK:
            default: return SPRITE_tree_bark_oak;
        }
    }
    if (type == ITEM_STRIPPED_LOG) {
        switch ((MaterialType)material) {
            case MAT_PINE: return SPRITE_tree_stripped_log_pine;
            case MAT_BIRCH: return SPRITE_tree_stripped_log_birch;
            case MAT_WILLOW: return SPRITE_tree_stripped_log_willow;
            case MAT_OAK:
            default: return SPRITE_tree_stripped_log_oak;
        }
    }
    return ItemSprite(type);
}

static void DrawInsetSprite(int sprite, Rectangle dest, float inset, Color tint) {
    if (sprite < 0) return;
    float insetW = dest.width - inset * 2.0f;
    float insetH = dest.height - inset * 2.0f;
    if (insetW <= 0 || insetH <= 0) return;
    Rectangle src = SpriteGetRect(sprite);
    Rectangle insetDest = { dest.x + inset, dest.y + inset, insetW, insetH };
    DrawTexturePro(atlas, src, insetDest, (Vector2){0,0}, 0, tint);
}

static int PlankSpriteForMaterial(MaterialType mat) {
    switch (mat) {
        case MAT_PINE:   return SPRITE_tree_planks_pine;
        case MAT_BIRCH:  return SPRITE_tree_planks_birch;
        case MAT_WILLOW: return SPRITE_tree_planks_willow;
        case MAT_OAK:
        default:         return SPRITE_tree_planks_oak;
    }
}

static int GetWallSpriteAt(int x, int y, int z, CellType cell) {
    if (cell == CELL_TRACK) return GetTrackSpriteAt(x, y, z);
    MaterialType mat = GetWallMaterial(x, y, z);
    if (cell == CELL_WALL && GetWallSourceItem(x, y, z) == ITEM_PLANKS) {
        return PlankSpriteForMaterial(mat);
    }
    return GetSpriteForCellMat(cell, mat);
}

static int GetFloorSpriteAt(int x, int y, int z) {
    MaterialType mat = GetFloorMaterial(x, y, z);
    if (GetFloorSourceItem(x, y, z) == ITEM_PLANKS) {
        return PlankSpriteForMaterial(mat);
    }
    return MaterialFloorSprite(mat);
}

// Helper: get depth tint for a given z-level relative to view
static Color GetDepthTint(int itemZ, int viewZ) {
    static const Color depthTints[] = {
        {255, 255, 255, 255},  // index 0: z-1 (no tint)
        {235, 225, 215, 255},  // index 1: z-2
        {220, 205, 190, 255},  // index 2: z-3
        {200, 180, 160, 255},  // index 3: z-4
        {180, 155, 130, 255},  // index 4: z-5
        {160, 130, 100, 255},  // index 5: z-6
        {140, 110, 80, 255},   // index 6: z-7
        {110, 85, 60, 255},    // index 7: z-8
        {80, 60, 40, 255}      // index 8: z-9
    };
    int depthIndex = viewZ - itemZ - 1;
    if (depthIndex >= 0 && depthIndex < MAX_VISIBLE_DEPTH) {
        return depthTints[depthIndex];
    }
    return WHITE;
}

// Helper: get depth tint with floor darkening pre-applied (avoids separate FloorDarkenTint call)
static Color GetDepthTintDarkened(int itemZ, int viewZ) {
    static const Color depthTintsDarkened[] = {
        {191, 191, 191, 255},  // index 0: z-1 (WHITE * 75%)
        {176, 168, 161, 255},  // index 1: z-2
        {165, 153, 142, 255},  // index 2: z-3
        {150, 135, 120, 255},  // index 3: z-4
        {135, 116,  97, 255},  // index 4: z-5
        {120,  97,  75, 255},  // index 5: z-6
        {105,  82,  60, 255},  // index 6: z-7
        { 82,  63,  45, 255},  // index 7: z-8
        { 60,  45,  30, 255}   // index 8: z-9
    };
    int depthIndex = viewZ - itemZ - 1;
    if (depthIndex >= 0 && depthIndex < MAX_VISIBLE_DEPTH) {
        return depthTintsDarkened[depthIndex];
    }
    return WHITE;
}

// Helper: check if a cell at (x, y, cellZ) is visible from viewZ
static bool IsCellVisibleFromAbove(int x, int y, int cellZ, int viewZ) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight) return false;
    if (cellZ < 0) cellZ = 0;
    if (viewZ > gridDepth) viewZ = gridDepth;
    for (int zCheck = cellZ; zCheck < viewZ; zCheck++) {
        if (CellIsSolid(grid[zCheck][y][x]) || HAS_FLOOR(x, y, zCheck)) {
            return false;
        }
    }
    return true;
}

// Draw deeper levels (z-9 through z-2) with earthy brown depth tint
static void DrawDeeperLevelCells(float size, int z, int minX, int minY, int maxX, int maxY, Color skyColor) {
    for (int zDepth = z - MAX_VISIBLE_DEPTH; zDepth <= z - 2; zDepth++) {
        if (zDepth < 0) continue;

        for (int y = minY; y < maxY; y++) {
            for (int x = minX; x < maxX; x++) {
                Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};

                // Wall tops
                CellType cellAtDepth = grid[zDepth][y][x];
                if (cellAtDepth != CELL_AIR) {
                    if (IsCellVisibleFromAbove(x, y, zDepth + 1, z)) {
                        int sprite = GetWallSpriteAt(x, y, zDepth, cellAtDepth);
                        Rectangle src = SpriteGetRect(sprite);
                        Color lightTint = GetLightColor(x, y, zDepth + 1, skyColor);
                        Color tint = MultiplyColor(GetDepthTintDarkened(zDepth, z), lightTint);
                        if ((cellAtDepth == CELL_WALL && !IsWallNatural(x, y, zDepth)) || cellAtDepth == CELL_DOOR) {
                            tint = MultiplyColor(tint, MaterialTint(GetWallMaterial(x, y, zDepth)));
                        }
                        if (CellIsRamp(cellAtDepth)) {
                            MaterialType rampMat = GetWallMaterial(x, y, zDepth);
                            if (rampMat != MAT_NONE) {
                                tint = MultiplyColor(tint, MaterialTint(rampMat));
                            }
                            tint.a = RAMP_OVERLAY_ALPHA;
                        }
                        DrawTexturePro(atlas, src, dest, (Vector2){0,0}, 0, tint);

                        if (cellAtDepth == CELL_WALL) {
                            DrawFinishOverlay(dest, (SurfaceFinish)GetWallFinish(x, y, zDepth), tint);
                        }
                    }
                }

                // Constructed floors (rendered after walls, visibility checked from above the floor)
                if (HAS_FLOOR(x, y, zDepth) && IsCellVisibleFromAbove(x, y, zDepth + 1, z)) {
                    MaterialType mat = GetFloorMaterial(x, y, zDepth);
                    int floorSprite = GetFloorSpriteAt(x, y, zDepth);
                    Color lightTint = GetLightColor(x, y, zDepth, skyColor);
                    Color tint = MultiplyColor(GetDepthTintDarkened(zDepth, z), lightTint);
                    tint = MultiplyColor(tint, MaterialTint(mat));
                    DrawInsetSprite(floorSprite, dest, 0.0f, tint);

                    DrawFinishOverlay(dest, (SurfaceFinish)GetFloorFinish(x, y, zDepth), tint);
                }
            }
        }
    }
}

// Draw the ground level: z-1 floor (what you're standing ON) or bedrock at z=0
static void DrawGroundLevel(float size, int z, int minX, int minY, int maxX, int maxY, Color skyColor) {
    if (z > 0) {
        int zBelow = z - 1;
        for (int y = minY; y < maxY; y++) {
            for (int x = minX; x < maxX; x++) {
                CellType cellBelow = grid[zBelow][y][x];
                CellType cellHere = grid[z][y][x];

                // Draw floor from below if the cell below is solid and current is air/walkable
                if (CellIsSolid(cellBelow) && !CellBlocksMovement(cellHere)) {
                    Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
                    int sprite = GetWallSpriteAt(x, y, zBelow, cellBelow);
                    Rectangle src = SpriteGetRect(sprite);
                    // Wall tops - lit by the air above (z), where torches sit
                    Color lightTint = GetLightColor(x, y, z, skyColor);
                    Color tint = MultiplyColor(FloorDarkenTint(WHITE), lightTint);
                    DrawTexturePro(atlas, src, dest, (Vector2){0,0}, 0, tint);

                    // Apply finish overlay to z-1 floor (same as deeper levels)
                    if (cellBelow == CELL_WALL) {
                        DrawFinishOverlay(dest, (SurfaceFinish)GetWallFinish(x, y, zBelow), tint);
                    }
                }
                // Draw non-solid visible cells at z-1 (ramps, ladders, felled trees, saplings)
                else if (cellBelow != CELL_AIR && !CellIsSolid(cellBelow) && !CellBlocksMovement(cellHere)) {
                    Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
                    int sprite = GetWallSpriteAt(x, y, zBelow, cellBelow);
                    Rectangle src = SpriteGetRect(sprite);
                    Color lightTint = GetLightColor(x, y, z, skyColor);
                    Color tint = MultiplyColor(FloorDarkenTint(WHITE), lightTint);
                    if (CellIsRamp(cellBelow)) {
                        MaterialType rampMat = GetWallMaterial(x, y, zBelow);
                        if (rampMat != MAT_NONE) {
                            tint = MultiplyColor(tint, MaterialTint(rampMat));
                        }
                        tint.a = RAMP_OVERLAY_ALPHA;
                    }
                    DrawTexturePro(atlas, src, dest, (Vector2){0,0}, 0, tint);
                }

                // Constructed floors at z-1
                if (HAS_FLOOR(x, y, zBelow)) {
                    Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
                    MaterialType mat = GetFloorMaterial(x, y, zBelow);
                    int floorSprite = GetFloorSpriteAt(x, y, zBelow);
                    Color lightTint = GetLightColor(x, y, zBelow, skyColor);
                    Color tint = MultiplyColor(FloorDarkenTint(MaterialTint(mat)), lightTint);
                    DrawInsetSprite(floorSprite, dest, 0.0f, tint);

                    DrawFinishOverlay(dest, (SurfaceFinish)GetFloorFinish(x, y, zBelow), tint);
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
                    Color lightTint = GetLightColor(x, y, z, skyColor);
                    DrawTexturePro(atlas, src, dest, (Vector2){0,0}, 0, MultiplyColor(FloorDarkenTint(WHITE), lightTint));
                }
            }
        }
    }
}

static void DrawCellGrid(void) {
    float size = CELL_SIZE * zoom;
    int z = currentViewZ;
    Color skyColor = GetSkyColorForTime(timeOfDay);

    int minX, minY, maxX, maxY;
    GetVisibleCellRange(size, &minX, &minY, &maxX, &maxY);

    DrawDeeperLevelCells(size, z, minX, minY, maxX, maxY, skyColor);
    DrawGroundLevel(size, z, minX, minY, maxX, maxY, skyColor);

    // Draw constructed floors (HAS_FLOOR flag - for balconies/bridges over empty space)
    for (int y = minY; y < maxY; y++) {
        for (int x = minX; x < maxX; x++) {
            if (HAS_FLOOR(x, y, z)) {
                Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
                MaterialType mat = GetFloorMaterial(x, y, z);
                int sprite = GetFloorSpriteAt(x, y, z);
                Color lightTint = GetLightColor(x, y, z, skyColor);
                Color tint = MultiplyColor(FloorDarkenTint(MaterialTint(mat)), lightTint);
                DrawInsetSprite(sprite, dest, 0.0f, tint);

                DrawFinishOverlay(dest, (SurfaceFinish)GetFloorFinish(x, y, z), WHITE);

                // Floor dirt overlay (tracked-in dirt from natural terrain)
                {
                    uint8_t dirt = floorDirtGrid[z][y][x];
                    if (dirt >= DIRT_VISIBLE_THRESHOLD) {
                        Rectangle dirtSrc = SpriteGetRect(SPRITE_finish_messy);
                        float t = (float)(dirt - DIRT_VISIBLE_THRESHOLD)
                                / (float)(DIRT_MAX - DIRT_VISIBLE_THRESHOLD);
                        unsigned char alpha = (unsigned char)(t * 128);  // Max 50% opacity
                        Color dirtTint = {80, 60, 40, alpha};
                        DrawTexturePro(atlas, dirtSrc, dest, (Vector2){0,0}, 0, dirtTint);
                    }
                }
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
            int sprite = GetWallSpriteAt(x, y, z, cell);
            Rectangle src = SpriteGetRect(sprite);
            Color lightTint = GetLightColor(x, y, z, skyColor);
            Color tint = lightTint;
            if ((cell == CELL_WALL && !IsWallNatural(x, y, z)) || cell == CELL_DOOR) {
                tint = MultiplyColor(tint, MaterialTint(GetWallMaterial(x, y, z)));
            }
            if (CellIsRamp(cell)) {
                MaterialType rampMat = GetWallMaterial(x, y, z);
                if (rampMat != MAT_NONE) {
                    tint = MultiplyColor(tint, MaterialTint(rampMat));
                }
                tint.a = RAMP_OVERLAY_ALPHA;
            }
            DrawTexturePro(atlas, src, dest, (Vector2){0,0}, 0, tint);

            if (cell == CELL_WALL) {
                DrawFinishOverlay(dest, (SurfaceFinish)GetWallFinish(x, y, z), tint);
            }
        }
    }

    // Draw wall cutaway effect - dark top with real wall texture visible at edges
    // This shows you're looking at the "cut" top of walls at eye level
    {
        // Scale cutaway darkness with sky brightness so it's always darker than surroundings
        float skyBright = (skyColor.r + skyColor.g + skyColor.b) / (3.0f * 255.0f);
        unsigned char cr = (unsigned char)(30.0f * skyBright);
        unsigned char cg = (unsigned char)(30.0f * skyBright);
        unsigned char cb = (unsigned char)(35.0f * skyBright);
        Color cutawayColor = (Color){cr, cg, cb, 255};
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
}

// Draw ramps as a late pass — after grass, mud, snow, clouds so they're always visible
// Covers all visible depths (same range as DrawDeeperLevelCells + ground + current)
void DrawRampOverlay(void);
void DrawRampOverlay(void) {
    float size = CELL_SIZE * zoom;
    int z = currentViewZ;
    Color skyColor = GetSkyColorForTime(timeOfDay);

    int minX, minY, maxX, maxY;
    GetVisibleCellRange(size, &minX, &minY, &maxX, &maxY);

    // Draw ramps at all visible depths: z-MAX_VISIBLE_DEPTH through z
    int zStart = z - MAX_VISIBLE_DEPTH;
    if (zStart < 0) zStart = 0;

    for (int zd = zStart; zd <= z; zd++) {
        for (int y = minY; y < maxY; y++) {
            for (int x = minX; x < maxX; x++) {
                CellType cell = grid[zd][y][x];
                if (!CellIsRamp(cell)) continue;
                if (!IsCellVisibleFromAbove(x, y, zd + 1, z)) continue;

                Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
                int sprite = GetWallSpriteAt(x, y, zd, cell);
                Rectangle src = SpriteGetRect(sprite);

                // Tinting: match what DrawDeeperLevelCells / DrawGroundLevel / DrawCurrentLayer do
                Color lightTint = GetLightColor(x, y, zd + 1, skyColor);
                Color tint;
                if (zd <= z - 2) {
                    // Deeper level: depth tint
                    tint = MultiplyColor(GetDepthTintDarkened(zd, z), lightTint);
                } else if (zd == z - 1) {
                    // Ground level: floor darken
                    tint = MultiplyColor(FloorDarkenTint(WHITE), lightTint);
                } else {
                    // Current z: just light
                    tint = lightTint;
                }

                MaterialType rampMat = GetWallMaterial(x, y, zd);
                if (rampMat != MAT_NONE) {
                    tint = MultiplyColor(tint, MaterialTint(rampMat));
                }
                tint.a = RAMP_OVERLAY_ALPHA;
                DrawTexturePro(atlas, src, dest, (Vector2){0,0}, 0, tint);
            }
        }
    }
}

static void DrawGrassOverlay(void) {
    float size = CELL_SIZE * zoom;
    int z = currentViewZ;
    Color skyColor = GetSkyColorForTime(timeOfDay);

    int minX, minY, maxX, maxY;
    GetVisibleCellRange(size, &minX, &minY, &maxX, &maxY);

    // Draw grass for deeper levels (z-9 through z-2) with earthy brown tint
    for (int zDepth = z - MAX_VISIBLE_DEPTH; zDepth <= z - 2; zDepth++) {
        if (zDepth < 0) continue;
        
        Color depthTint = GetDepthTint(zDepth, z);
        for (int y = minY; y < maxY; y++) {
            for (int x = minX; x < maxX; x++) {
                if (!IsWallNatural(x, y, zDepth) || GetWallMaterial(x, y, zDepth) != MAT_DIRT) continue;
                if (!IsCellVisibleFromAbove(x, y, zDepth + 1, z + 1)) continue;
                
                VegetationType veg = GetVegetation(x, y, zDepth);
                int surface = GET_CELL_SURFACE(x, y, zDepth);
                int sprite;
                if (veg >= VEG_GRASS_TALLER)     sprite = SPRITE_grass_taller;
                else if (veg >= VEG_GRASS_TALL)  sprite = SPRITE_grass_tall;
                else if (veg >= VEG_GRASS_SHORT) sprite = SPRITE_grass;
                else if (surface == SURFACE_TRAMPLED) sprite = SPRITE_grass_trampled;
                else continue;
                
                Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
                Rectangle src = SpriteGetRect(sprite);
                // Grass is on top of solid, lit from the air above (zDepth+1)
                Color tint = MultiplyColor(depthTint, GetLightColor(x, y, zDepth + 1, skyColor));
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
                if (!IsWallNatural(x, y, zBelow) || GetWallMaterial(x, y, zBelow) != MAT_DIRT) continue;
                CellType cellHere = grid[z][y][x];
                // Allow grass under air only, skip walls/ladders/ramps/etc.
                if (cellHere != CELL_AIR) continue;
                if (HAS_FLOOR(x, y, z)) continue;    // Don't draw grass under constructed floors
                
                VegetationType veg = GetVegetation(x, y, zBelow);
                int surface = GET_CELL_SURFACE(x, y, zBelow);
                int sprite;
                if (veg >= VEG_GRASS_TALLER)     sprite = SPRITE_grass_taller;
                else if (veg >= VEG_GRASS_TALL)  sprite = SPRITE_grass_tall;
                else if (veg >= VEG_GRASS_SHORT) sprite = SPRITE_grass;
                else if (surface == SURFACE_TRAMPLED) sprite = SPRITE_grass_trampled;
                else continue;
                
                Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
                Rectangle src = SpriteGetRect(sprite);
                Color lightTint = GetLightColor(x, y, z, skyColor);
                DrawTexturePro(atlas, src, dest, (Vector2){0,0}, 0, lightTint);
            }
        }
}

static void DrawPlantOverlay(void) {
    float size = CELL_SIZE * zoom;
    int z = currentViewZ;
    Color skyColor = GetSkyColorForTime(timeOfDay);

    int minX, minY, maxX, maxY;
    GetVisibleCellRange(size, &minX, &minY, &maxX, &maxY);

    for (int i = 0; i < plantCount; i++) {
        Plant* p = &plants[i];
        if (!p->active) continue;
        if (p->z != z) continue;
        if (p->x < minX || p->x >= maxX || p->y < minY || p->y >= maxY) continue;
        if (p->stage != PLANT_STAGE_RIPE) continue;

        Rectangle dest = {offset.x + p->x * size, offset.y + p->y * size, size, size};
        Color lightTint = GetLightColor(p->x, p->y, z, skyColor);
        Color berryTint = MultiplyColor(lightTint, (Color){220, 50, 50, 255});
        float inset = size * 0.25f;
        DrawInsetSprite(SPRITE_division, dest, inset, berryTint);
    }
}

static void DrawMud(void) {
    float size = CELL_SIZE * zoom;
    int z = currentViewZ;
    Color skyColor = GetSkyColorForTime(timeOfDay);

    int minX, minY, maxX, maxY;
    GetVisibleCellRange(size, &minX, &minY, &maxX, &maxY);

    // Draw mud at deeper levels (z-9 through z-2) with depth darkening
    for (int zDepth = z - MAX_VISIBLE_DEPTH; zDepth <= z - 2; zDepth++) {
        if (zDepth < 0) continue;
        Color depthTint = GetDepthTint(zDepth, z);
        for (int y = minY; y < maxY; y++) {
            for (int x = minX; x < maxX; x++) {
                if (!IsMuddy(x, y, zDepth)) continue;
                if (!IsCellVisibleFromAbove(x, y, zDepth + 1, z + 1)) continue;

                int wetness = GET_CELL_WETNESS(x, y, zDepth);
                unsigned char alpha = (wetness == 2) ? 80 : 120;
                Color mudColor = {80, 60, 30, alpha};
                Color tint = MultiplyColor(depthTint, GetLightColor(x, y, zDepth + 1, skyColor));
                mudColor = MultiplyColor(mudColor, tint);

                Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
                DrawRectangleRec(dest, mudColor);
            }
        }
    }

    // Current view z: check z-1 (the ground you're standing on)
    if (z <= 0) return;
    int zBelow = z - 1;
    for (int y = minY; y < maxY; y++) {
        for (int x = minX; x < maxX; x++) {
            if (!IsMuddy(x, y, zBelow)) continue;
            CellType cellHere = grid[z][y][x];
            if (cellHere != CELL_AIR && !CellIsRamp(cellHere)) continue;
            if (HAS_FLOOR(x, y, z)) continue;

            int wetness = GET_CELL_WETNESS(x, y, zBelow);
            unsigned char alpha = (wetness == 2) ? 80 : 120;
            Color mudColor = {80, 60, 30, alpha};
            Color lightTint = GetLightColor(x, y, z, skyColor);
            mudColor = MultiplyColor(mudColor, lightTint);

            Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
            DrawRectangleRec(dest, mudColor);
        }
    }
}

static unsigned char SnowAlpha(uint8_t level) {
    static const unsigned char alphas[] = {0, 80, 170, 240};
    return alphas[level < 4 ? level : 3];
}

static void DrawSnow(void) {
    float size = CELL_SIZE * zoom;
    int z = currentViewZ;
    Color skyColor = GetSkyColorForTime(timeOfDay);

    int minX, minY, maxX, maxY;
    GetVisibleCellRange(size, &minX, &minY, &maxX, &maxY);

    // Draw snow at deeper levels (z-9 through z-2) with depth darkening
    for (int zDepth = z - MAX_VISIBLE_DEPTH; zDepth <= z - 2; zDepth++) {
        if (zDepth < 0) continue;
        Color depthTint = GetDepthTint(zDepth, z);
        for (int y = minY; y < maxY; y++) {
            for (int x = minX; x < maxX; x++) {
                uint8_t snowLevel = GetSnowLevel(x, y, zDepth);
                if (snowLevel == 0) continue;
                if (!IsCellVisibleFromAbove(x, y, zDepth + 1, z + 1)) continue;

                unsigned char alpha = SnowAlpha(snowLevel);
                Color snowColor = {255, 255, 255, alpha};
                Color tint = MultiplyColor(depthTint, GetLightColor(x, y, zDepth + 1, skyColor));
                snowColor = MultiplyColor(snowColor, tint);

                Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
                DrawRectangleRec(dest, snowColor);
            }
        }
    }

    // Current view z: check z-1 (the ground you're standing on)
    if (z <= 0) return;
    int zBelow = z - 1;
    for (int y = minY; y < maxY; y++) {
        for (int x = minX; x < maxX; x++) {
            uint8_t snowLevel = GetSnowLevel(x, y, zBelow);
            if (snowLevel == 0) continue;
            CellType cellHere = grid[z][y][x];
            if (cellHere != CELL_AIR && !CellIsRamp(cellHere)) continue;
            if (HAS_FLOOR(x, y, z)) continue;

            // Snow opacity based on level: light=80, moderate=170, heavy=240
            unsigned char alpha = (unsigned char)(snowLevel == 1 ? 80 : snowLevel == 2 ? 170 : 240);
            Color snowColor = {255, 255, 255, alpha};
            Color lightTint = GetLightColor(x, y, z, skyColor);
            snowColor = MultiplyColor(snowColor, lightTint);

            Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
            DrawRectangleRec(dest, snowColor);
        }
    }
}

static void DrawCloudShadows(void) {
    // Only draw cloud shadows for weather types that have clouds
    if (weatherState.current == WEATHER_CLEAR) return;
    
    float size = CELL_SIZE * zoom;

    int minX, minY, maxX, maxY;
    GetVisibleCellRange(size, &minX, &minY, &maxX, &maxY);

    // Draw cloud shadows on current view z only (surface level)
    for (int y = minY; y < maxY; y++) {
        for (int x = minX; x < maxX; x++) {
            float shadow = GetCloudShadow(x, y, gameTime);
            if (shadow < 0.01f) continue;

            // Darken the cell based on shadow intensity
            unsigned char alpha = (unsigned char)(shadow * 80);  // Max 80 alpha for darkening
            Color shadowColor = {0, 0, 0, alpha};

            Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
            DrawRectangleRec(dest, shadowColor);
        }
    }
}

static void DrawWater(void) {
    if (waterActiveCells == 0) return;
    float size = CELL_SIZE * zoom;
    int z = currentViewZ;
    Color skyColor = GetSkyColorForTime(timeOfDay);

    int minX, minY, maxX, maxY;
    GetVisibleCellRange(size, &minX, &minY, &maxX, &maxY);

    // Draw water at deeper levels (z-9 through z-1) with depth darkening
    for (int zDepth = z - MAX_VISIBLE_DEPTH; zDepth <= z - 1; zDepth++) {
        if (zDepth < 0) continue;
        
        for (int y = minY; y < maxY; y++) {
            for (int x = minX; x < maxX; x++) {
                int level = GetWaterLevel(x, y, zDepth);
                if (level <= 0) continue;
                if (!IsCellVisibleFromAbove(x, y, zDepth + 1, z + 1)) continue;
                
                int alpha = 80 + (level * 15);
                if (alpha > 230) alpha = 230;
                
                Color lightTint = GetLightColor(x, y, zDepth, skyColor);
                Color waterColor = MultiplyColor((Color){30, 100, 200, alpha}, lightTint);
                waterColor = FloorDarkenTint(waterColor);
                
                Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
                DrawRectangleRec(dest, waterColor);
                
                if (waterGrid[zDepth][y][x].isSource) {
                    DrawSourceMarker(dest, size, (Color){75, 135, 191, 200});
                }

                if (waterGrid[zDepth][y][x].isDrain) {
                    DrawSourceMarker(dest, size, (Color){15, 30, 60, 200});
                }
            }
        }
    }

    // Draw water at current level (z) - brightest, no darkening
    for (int y = minY; y < maxY; y++) {
        for (int x = minX; x < maxX; x++) {
            int level = GetWaterLevel(x, y, z);
            if (level <= 0) continue;

            // Alpha based on water level (deeper = more opaque)
            int alpha = 80 + (level * 15);  // 80-230 range
            if (alpha > 230) alpha = 230;
            
            Color lightTint = GetLightColor(x, y, z, skyColor);
            Color waterColor = MultiplyColor((Color){30, 100, 200, alpha}, lightTint);
            
            // Draw water overlay
            Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
            DrawRectangleRec(dest, waterColor);
            
            // Mark sources with a brighter center
            if (waterGrid[z][y][x].isSource) {
                DrawSourceMarker(dest, size, (Color){100, 180, 255, 200});
            }

            // Mark drains with a dark center
            if (waterGrid[z][y][x].isDrain) {
                DrawSourceMarker(dest, size, (Color){20, 40, 80, 200});
            }
        }
    }
}

static void DrawFire(void) {
    if (fireActiveCells == 0) return;
    float size = CELL_SIZE * zoom;
    int z = currentViewZ;

    int minX, minY, maxX, maxY;
    GetVisibleCellRange(size, &minX, &minY, &maxX, &maxY);

    // Draw fire at deeper levels (z-9 through z-2)
    for (int zDepth = z - MAX_VISIBLE_DEPTH; zDepth <= z - 2; zDepth++) {
        if (zDepth < 0) continue;
        
        for (int y = minY; y < maxY; y++) {
            for (int x = minX; x < maxX; x++) {
                if (!IsCellVisibleFromAbove(x, y, zDepth + 1, z)) continue;
                
                FireCell* cell = &fireGrid[zDepth][y][x];
                int level = cell->level;
                
                if (level == 0 && HAS_CELL_FLAG(x, y, zDepth, CELL_FLAG_BURNED)) {
                    Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
                    DrawRectangleRec(dest, (Color){30, 22, 15, 75});
                    continue;
                }
                if (level <= 0) continue;
                
                Color fireColor = FireLevelColor(level);
                fireColor = FloorDarkenTint(fireColor);
                
                Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
                DrawRectangleRec(dest, fireColor);
                
                if (cell->isSource) {
                    DrawSourceMarker(dest, size, (Color){191, 165, 75, 200});
                }
            }
        }
    }
    
    // Draw fire at z-1 and current z
    for (int y = minY; y < maxY; y++) {
        for (int x = minX; x < maxX; x++) {
            int fireZ = z;
            if (z > 0 && grid[z][y][x] == CELL_AIR && CellIsSolid(grid[z-1][y][x])) {
                fireZ = z - 1;
            }
            
            FireCell* cell = &fireGrid[fireZ][y][x];
            int level = cell->level;
            
            if (level == 0 && HAS_CELL_FLAG(x, y, fireZ, CELL_FLAG_BURNED)) {
                Color burnedColor = fireZ < z ? (Color){30, 22, 15, 75} : (Color){40, 30, 20, 100};
                Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
                DrawRectangleRec(dest, burnedColor);
                continue;
            }
            if (level <= 0) continue;
            
            Color fireColor = FireLevelColor(level);
            if (fireZ < z) fireColor = FloorDarkenTint(fireColor);
            
            Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
            DrawRectangleRec(dest, fireColor);
            
            if (cell->isSource) {
                Color srcColor = fireZ < z ? (Color){191, 165, 75, 200} : (Color){255, 220, 100, 200};
                DrawSourceMarker(dest, size, srcColor);
            }
        }
    }
}

static void DrawSmoke(void) {
    if (smokeActiveCells == 0) return;
    float size = CELL_SIZE * zoom;
    int z = currentViewZ;

    int minX, minY, maxX, maxY;
    GetVisibleCellRange(size, &minX, &minY, &maxX, &maxY);

    // Draw smoke at deeper levels (z-9 through z-1) with depth darkening
    for (int zDepth = z - MAX_VISIBLE_DEPTH; zDepth <= z - 1; zDepth++) {
        if (zDepth < 0) continue;
        
        for (int y = minY; y < maxY; y++) {
            for (int x = minX; x < maxX; x++) {
                if (!IsCellVisibleFromAbove(x, y, zDepth + 1, z)) continue;
                
                int level = GetSmokeLevel(x, y, zDepth);
                if (level <= 0) continue;

                int alpha = 30 + (level * 25);
                if (alpha > 205) alpha = 205;
                
                Color smokeColor = FloorDarkenTint((Color){80, 80, 90, alpha});
                
                Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
                DrawRectangleRec(dest, smokeColor);
            }
        }
    }

    // Draw smoke at current level (brightest)
    for (int y = minY; y < maxY; y++) {
        for (int x = minX; x < maxX; x++) {
            int level = GetSmokeLevel(x, y, z);
            if (level <= 0) continue;

            int alpha = 30 + (level * 25);
            if (alpha > 205) alpha = 205;
            
            Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
            DrawRectangleRec(dest, (Color){80, 80, 90, alpha});
        }
    }
}

static void DrawSteam(void) {
    if (steamActiveCells == 0) return;
    float size = CELL_SIZE * zoom;
    int z = currentViewZ;

    int minX, minY, maxX, maxY;
    GetVisibleCellRange(size, &minX, &minY, &maxX, &maxY);

    // Draw steam at deeper levels (z-9 through z-1) with depth darkening
    for (int zDepth = z - MAX_VISIBLE_DEPTH; zDepth <= z - 1; zDepth++) {
        if (zDepth < 0) continue;
        
        for (int y = minY; y < maxY; y++) {
            for (int x = minX; x < maxX; x++) {
                if (!IsCellVisibleFromAbove(x, y, zDepth + 1, z)) continue;
                
                int level = GetSteamLevel(x, y, zDepth);
                if (level <= 0) continue;

                int alpha = 40 + (level * 20);
                if (alpha > 180) alpha = 180;
                
                Color steamColor = FloorDarkenTint((Color){220, 220, 230, alpha});
                
                Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
                DrawRectangleRec(dest, steamColor);
            }
        }
    }

    // Draw steam at current level (brightest)
    for (int y = minY; y < maxY; y++) {
        for (int x = minX; x < maxX; x++) {
            int level = GetSteamLevel(x, y, z);
            if (level <= 0) continue;

            int alpha = 40 + (level * 20);
            if (alpha > 180) alpha = 180;
            
            Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
            DrawRectangleRec(dest, (Color){220, 220, 230, alpha});
        }
    }
}

static void DrawTemperature(void) {
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
                DrawSourceMarker(dest, size, (Color){255, 200, 100, 200});
            }

            // Mark cold sources with a cyan center
            if (IsColdSource(x, y, z)) {
                DrawSourceMarker(dest, size, (Color){100, 200, 255, 200});
            }
        }
    }
}

static void DrawSimSources(void) {
    if (!showSimSources) return;

    float size = CELL_SIZE * zoom;
    int z = currentViewZ;

    int minX, minY, maxX, maxY;
    GetVisibleCellRange(size, &minX, &minY, &maxX, &maxY);

    float marker = size * 0.2f;
    if (marker < 3.0f) marker = 3.0f;
    float pad = 2.0f;

    for (int y = minY; y < maxY; y++) {
        for (int x = minX; x < maxX; x++) {
            float sx = offset.x + x * size;
            float sy = offset.y + y * size;

            if (IsWaterSourceAt(x, y, z)) {
                DrawRectangle((int)(sx + pad), (int)(sy + pad), (int)marker, (int)marker, BLUE);
            }
            if (IsWaterDrainAt(x, y, z)) {
                DrawRectangle((int)(sx + size - marker - pad), (int)(sy + pad), (int)marker, (int)marker, DARKBLUE);
            }
            if (IsHeatSource(x, y, z)) {
                DrawRectangle((int)(sx + pad), (int)(sy + size - marker - pad), (int)marker, (int)marker, ORANGE);
            }
            if (IsColdSource(x, y, z)) {
                DrawRectangle((int)(sx + size - marker - pad), (int)(sy + size - marker - pad), (int)marker, (int)marker, SKYBLUE);
            }
            if (IsFireSourceAt(x, y, z)) {
                DrawRectangle((int)(sx + size * 0.5f - marker * 0.5f), (int)(sy + size * 0.5f - marker * 0.5f),
                              (int)marker, (int)marker, RED);
            }
        }
    }
}

static void DrawFrozenWater(void) {
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

static void DrawChunkBoundaries(void) {
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

static void DrawEntrances(void) {
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

static void DrawGraph(void) {
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

static void DrawPath(void) {
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

static void DrawAgents(void) {
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

static void DrawMoverPath(const Mover* m, float sx, float sy, int viewZ,
                          Color color, float lineWidth, float segmentWidth, float segmentFade) {
    Point next = m->path[m->pathIndex];
    if (next.z == viewZ) {
        float tx = offset.x + (next.x * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
        float ty = offset.y + (next.y * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
        DrawLineEx((Vector2){sx, sy}, (Vector2){tx, ty}, lineWidth, color);
    }
    for (int j = m->pathIndex; j > 0; j--) {
        if (m->path[j].z != viewZ || m->path[j-1].z != viewZ) continue;
        float px1 = offset.x + (m->path[j].x * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
        float py1 = offset.y + (m->path[j].y * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
        float px2 = offset.x + (m->path[j-1].x * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
        float py2 = offset.y + (m->path[j-1].y * CELL_SIZE + CELL_SIZE * 0.5f) * zoom;
        DrawLineEx((Vector2){px1, py1}, (Vector2){px2, py2}, segmentWidth, Fade(color, segmentFade));
    }
}

static Vector2 CalcWorkAnimOffset(Mover* m) {
    Vector2 off = {0, 0};
    Job* job = (m->currentJobId >= 0) ? GetJob(m->currentJobId) : NULL;
    bool isWorking = job && (job->step == STEP_WORKING || job->step == CRAFT_STEP_WORKING);
    if (!isWorking) {
        m->workAnimPhase = 0.0f;
        return off;
    }
    if (paused) return off;
    m->workAnimPhase += GetFrameTime();
    bool isOnTile = (job->type == JOBTYPE_CHANNEL ||
                     job->type == JOBTYPE_REMOVE_FLOOR ||
                     job->type == JOBTYPE_PLANT_SAPLING ||
                     job->type == JOBTYPE_GATHER_GRASS ||
                     job->type == JOBTYPE_CLEAN);
    if (isOnTile) {
        float wave = sinf(m->workAnimPhase * WORK_BOB_FREQ * 2.0f * PI);
        off.y = -fabsf(wave) * WORK_BOB_AMPLITUDE * CELL_SIZE * zoom;
    } else {
        // Determine work target position based on job type
        float workTargetX = -1, workTargetY = -1;
        if (job->type == JOBTYPE_CRAFT || job->type == JOBTYPE_IGNITE_WORKSHOP) {
            if (job->targetWorkshop >= 0 && job->targetWorkshop < MAX_WORKSHOPS &&
                workshops[job->targetWorkshop].active) {
                Workshop* ws = &workshops[job->targetWorkshop];
                workTargetX = (ws->workTileX + 0.5f) * CELL_SIZE;
                workTargetY = (ws->workTileY + 0.5f) * CELL_SIZE;
            }
        } else if (job->type == JOBTYPE_BUILD) {
            if (job->targetBlueprint >= 0 && job->targetBlueprint < MAX_BLUEPRINTS &&
                blueprints[job->targetBlueprint].active) {
                workTargetX = (blueprints[job->targetBlueprint].x + 0.5f) * CELL_SIZE;
                workTargetY = (blueprints[job->targetBlueprint].y + 0.5f) * CELL_SIZE;
            }
        } else {
            // Mining, chopping, dig ramp, etc. — use targetMineX/Y
            workTargetX = (job->targetMineX + 0.5f) * CELL_SIZE;
            workTargetY = (job->targetMineY + 0.5f) * CELL_SIZE;
        }

        if (workTargetX >= 0) {
            float wave = sinf(m->workAnimPhase * WORK_SWAY_FREQ * 2.0f * PI);
            float ddx = workTargetX - m->x;
            float ddy = workTargetY - m->y;
            float dist = sqrtf(ddx * ddx + ddy * ddy);
            if (dist > 0.01f) { ddx /= dist; ddy /= dist; }
            float swayAmount = wave * WORK_SWAY_AMPLITUDE * CELL_SIZE * zoom;
            off.x = ddx * swayAmount;
            off.y = ddy * swayAmount;
        }
    }
    return off;
}

static void DrawCarriedItem(const Mover* m, float sx, float sy, int viewZ, Color skyColor) {
    Job* moverJob = (m->currentJobId >= 0) ? GetJob(m->currentJobId) : NULL;
    int carryingItem = moverJob ? moverJob->carryingItem : -1;
    if (carryingItem < 0 && moverJob && moverJob->step == CRAFT_STEP_CARRYING_FUEL) {
        carryingItem = moverJob->fuelItem;
    }
    if (carryingItem < 0 || !items[carryingItem].active) return;

    float size = CELL_SIZE * zoom;
    float moverSize = size * MOVER_SIZE;
    int moverZ = (int)m->z;

    Item* item = &items[carryingItem];
    int sprite = ItemSpriteForTypeMaterial(item->type, item->material);
    float itemSize = size * ITEM_SIZE_CARRIED;
    float itemY = sy - moverSize/2 - itemSize + moverSize * 0.2f;
    Rectangle itemDest = { sx - itemSize/2, itemY, itemSize, itemSize };

    // Apply depth tinting, darkening, and lighting to carried items
    int cellX = (int)(m->x / CELL_SIZE);
    int cellY = (int)(m->y / CELL_SIZE);
    Color lightTint = GetLightColor(cellX, cellY, moverZ, skyColor);
    Color itemTint = MultiplyColor(lightTint, ItemTypeTint(item->type));
    Color borderTint = MultiplyColor(ItemBorderTint(item->type), lightTint);
    if (moverZ < viewZ) {
        Color depthTint = GetDepthTint(moverZ, viewZ);
        itemTint = FloorDarkenTint(MultiplyColor(itemTint, depthTint));
        borderTint = FloorDarkenTint(MultiplyColor(borderTint, depthTint));
    }

    if (ItemUsesBorder(item->type)) {
        DrawItemWithBorder(sprite, itemDest, itemTint, borderTint);
    } else {
        Rectangle itemSrc = SpriteGetRect(sprite);
        DrawTexturePro(atlas, itemSrc, itemDest, (Vector2){0, 0}, 0, itemTint);
    }
}

static void DrawMovers(void) {
    float size = CELL_SIZE * zoom;
    int viewZ = currentViewZ;
    Color skyColor = GetSkyColorForTime(timeOfDay);
    
    // Frustum culling bounds with one cell margin for partially visible movers
    float margin = size;
    float minX = -margin;
    float maxX = GetScreenWidth() + margin;
    float minY = -margin;
    float maxY = GetScreenHeight() + margin;

    for (int i = 0; i < moverCount; i++) {
        Mover* m = &movers[i];
        if (!m->active) continue;

        int moverZ = (int)m->z;
        
        // Only draw movers within visible depth range (current z and up to 9 levels below)
        if (moverZ > viewZ || moverZ < viewZ - MAX_VISIBLE_DEPTH) continue;
        
        // Check if mover is visible (clear line of sight from above)
        if (moverZ < viewZ) {
            int cellX = (int)(m->x / CELL_SIZE);
            int cellY = (int)(m->y / CELL_SIZE);
            if (!IsCellVisibleFromAbove(cellX, cellY, moverZ + 1, viewZ)) continue;
        }

        // Screen position
        float sx = offset.x + m->x * zoom;
        float sy = offset.y + m->y * zoom;
        
        // Frustum cull - skip movers outside visible area
        if (sx < minX || sx > maxX || sy < minY || sy > maxY) continue;
        if (usePixelPerfectMovers) {
            sx = roundf(sx);
            sy = roundf(sy);
        }

        // Work animation: sway toward target (adjacent work) or bob in place (on-tile work)
        {
            Vector2 workOff = CalcWorkAnimOffset(m);
            sx += workOff.x;
            sy += workOff.y;
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
        } else if (m->currentJobId >= 0 && m->repathCooldown > 0 && m->pathLength == 0) {
            moverColor = ORANGE;
        } else if (m->currentJobId >= 0 && m->pathLength == 0) {
            moverColor = RED;
        } else if (m->needsRepath) {
            moverColor = YELLOW;
        } else if (m->currentJobId < 0 && m->pathLength == 0 && m->repathCooldown > 0) {
            moverColor = (Color){ 255, 200, 200, 255 };  // Faint pink: recently disrupted, now idle
        } else {
            moverColor = WHITE;
        }

        // Override color if mover just fell
        if (m->fallTimer > 0) {
            moverColor = BLUE;
        }

        // Apply depth tinting and darkening if mover is below current view
        if (moverZ < viewZ) {
            moverColor = MultiplyColor(moverColor, GetDepthTint(moverZ, viewZ));
            moverColor = FloorDarkenTint(moverColor);
        }

        // Apply lighting
        {
            int cellX = (int)(m->x / CELL_SIZE);
            int cellY = (int)(m->y / CELL_SIZE);
            moverColor = MultiplyColor(moverColor, GetLightColor(cellX, cellY, moverZ, skyColor));
        }

        // Draw mover as head sprite with color tint
        float moverSize = size * MOVER_SIZE;
        Rectangle src = SpriteGetRect(SPRITE_head);
        Rectangle dest = { sx - moverSize/2, sy - moverSize/2, moverSize, moverSize };
        DrawTexturePro(atlas, src, dest, (Vector2){0, 0}, 0, moverColor);

        DrawCarriedItem(m, sx, sy, viewZ, skyColor);
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
            DrawMoverPath(m, sx, sy, viewZ, moverRenderData[i].color, 2.0f, 1.0f, 0.4f);
        }
        PROFILE_END(MoverPaths);
    }
    
    // Draw hovered mover's path (always, even if showMoverPaths is off)
    if (hoveredMover >= 0 && !showMoverPaths) {
        Mover* m = &movers[hoveredMover];
        if (m->active && m->pathIndex >= 0) {
            float sx = offset.x + m->x * zoom;
            float sy = offset.y + m->y * zoom;
            DrawMoverPath(m, sx, sy, viewZ, YELLOW, 2.0f, 2.0f, 0.6f);
        }
    }
}

static void DrawAnimals(void) {
    float size = CELL_SIZE * zoom;
    int viewZ = currentViewZ;
    Color skyColor = GetSkyColorForTime(timeOfDay);

    // Frustum culling bounds
    float margin = size;
    float minScreenX = -margin;
    float maxScreenX = GetScreenWidth() + margin;
    float minScreenY = -margin;
    float maxScreenY = GetScreenHeight() + margin;

    for (int i = 0; i < animalCount; i++) {
        Animal* a = &animals[i];
        if (!a->active) continue;

        int az = (int)a->z;

        // Depth range check
        if (az > viewZ || az < viewZ - MAX_VISIBLE_DEPTH) continue;

        // Visibility from above
        if (az < viewZ) {
            int cellX = (int)(a->x / CELL_SIZE);
            int cellY = (int)(a->y / CELL_SIZE);
            if (!IsCellVisibleFromAbove(cellX, cellY, az + 1, viewZ)) continue;
        }

        // Screen position
        float sx = offset.x + a->x * zoom;
        float sy = offset.y + a->y * zoom;

        // Frustum cull
        if (sx < minScreenX || sx > maxScreenX || sy < minScreenY || sy > maxScreenY) continue;

        // Snap to pixel
        sx = roundf(sx);
        sy = roundf(sy);

        // Base color depends on behavior type
        Color animalColor;
        if (a->behavior == BEHAVIOR_PREDATOR) {
            animalColor = (Color){ 180, 180, 190, 255 };  // Light grey wolf
        } else if (a->behavior == BEHAVIOR_STEERING_GRAZER) {
            animalColor = (Color){ 60, 80, 140, 255 };    // Blue cow
        } else {
            animalColor = (Color){ 50, 120, 60, 255 };    // Green simple grazer
        }

        // Depth tinting
        if (az < viewZ) {
            animalColor = MultiplyColor(animalColor, GetDepthTint(az, viewZ));
            animalColor = FloorDarkenTint(animalColor);
        }

        // Lighting
        {
            int cellX = (int)(a->x / CELL_SIZE);
            int cellY = (int)(a->y / CELL_SIZE);
            animalColor = MultiplyColor(animalColor, GetLightColor(cellX, cellY, az, skyColor));
        }

        // Advance animation phase
        if (gameSpeed > 0.0f) a->animPhase += GetFrameTime();

        float animalSize = size * 0.8f;

        // Draw body (small triangle)
        Rectangle bodySrc = SpriteGetRect(SPRITE_small_triangle);
        Rectangle bodyDest = { sx - animalSize/2, sy - animalSize/2, animalSize, animalSize };
        DrawTexturePro(atlas, bodySrc, bodyDest, (Vector2){0, 0}, 0, animalColor);

        // Draw head with bobbing, low and overlapping body
        float bobFreq = 2.5f;
        float bobAmp = 0.08f;
        float bob = sinf(a->animPhase * bobFreq * 2.0f * PI) * bobAmp * CELL_SIZE * zoom;
        float headSize = animalSize * 0.7f;
        float headX = sx - headSize/2;
        float headY = sy - headSize * 0.3f + bob;
        int headSprite = (a->behavior == BEHAVIOR_PREDATOR) ? SPRITE_head : SPRITE_head_inverse;
        Rectangle headSrc = SpriteGetRect(headSprite);
        Rectangle headDest = { headX, headY, headSize, headSize };
        DrawTexturePro(atlas, headSrc, headDest, (Vector2){0, 0}, 0, animalColor);
    }
}

static void DrawTrains(void) {
    float size = CELL_SIZE * zoom;
    int viewZ = currentViewZ;
    Color skyColor = GetSkyColorForTime(timeOfDay);

    float margin = size;
    float minScreenX = -margin;
    float maxScreenX = GetScreenWidth() + margin;
    float minScreenY = -margin;
    float maxScreenY = GetScreenHeight() + margin;

    for (int i = 0; i < MAX_TRAINS; i++) {
        Train* t = &trains[i];
        if (!t->active) continue;

        if (t->z > viewZ || t->z < viewZ - MAX_VISIBLE_DEPTH) continue;

        if (t->z < viewZ) {
            int cellX = (int)(t->x / CELL_SIZE);
            int cellY = (int)(t->y / CELL_SIZE);
            if (!IsCellVisibleFromAbove(cellX, cellY, t->z + 1, viewZ)) continue;
        }

        float sx = roundf(offset.x + t->x * zoom);
        float sy = roundf(offset.y + t->y * zoom);

        if (sx < minScreenX || sx > maxScreenX || sy < minScreenY || sy > maxScreenY) continue;

        Color trainColor = WHITE;
        if (t->z < viewZ) {
            trainColor = MultiplyColor(trainColor, GetDepthTint(t->z, viewZ));
            trainColor = FloorDarkenTint(trainColor);
        }
        {
            int cellX = (int)(t->x / CELL_SIZE);
            int cellY = (int)(t->y / CELL_SIZE);
            trainColor = MultiplyColor(trainColor, GetLightColor(cellX, cellY, t->z, skyColor));
        }

        // Compute rotation from travel direction (sprite faces west by default)
        float rotation = 0;
        int dx = t->cellX - t->prevCellX;
        int dy = t->cellY - t->prevCellY;
        if (dx == 1 && dy == 0) rotation = 180;       // East
        else if (dx == -1 && dy == 0) rotation = 0;   // West
        else if (dx == 0 && dy == -1) rotation = 90;   // North
        else if (dx == 0 && dy == 1) rotation = 270;  // South

        float trainSize = size * 0.9f;
        Rectangle src = SpriteGetRect(SPRITE_train_loc);
        Rectangle dest = { sx, sy, trainSize, trainSize };
        Vector2 origin = { trainSize / 2, trainSize / 2 };
        DrawTexturePro(atlas, src, dest, origin, rotation, trainColor);
    }
}

static void DrawJobLines(void) {
    if (!showJobLines) return;

    for (int i = 0; i < moverCount; i++) {
        Mover* m = &movers[i];
        if (!m->active) continue;
        if (m->currentJobId < 0) continue;

        Job* job = GetJob(m->currentJobId);
        if (!job || !job->active) continue;

        float msx = offset.x + m->x * zoom;
        float msy = offset.y + m->y * zoom;

        Color color = YELLOW;
        switch (job->type) {
            case JOBTYPE_MINE:
            case JOBTYPE_CHANNEL:
            case JOBTYPE_DIG_RAMP:
            case JOBTYPE_REMOVE_FLOOR:
            case JOBTYPE_REMOVE_RAMP:
            case JOBTYPE_CHOP:
            case JOBTYPE_GATHER_SAPLING:
            case JOBTYPE_PLANT_SAPLING:
                color = SKYBLUE;
                if (job->targetMineX >= 0) {
                    DrawLineToTile(msx, msy, job->targetMineX, job->targetMineY, job->targetMineZ, color);
                }
                break;
            case JOBTYPE_BUILD:
                color = GREEN;
                if (job->targetBlueprint >= 0 && job->targetBlueprint < MAX_BLUEPRINTS) {
                    Blueprint* bp = &blueprints[job->targetBlueprint];
                    if (bp->active) {
                        DrawLineToTile(msx, msy, bp->x, bp->y, bp->z, color);
                    }
                }
                break;
            case JOBTYPE_HAUL:
            case JOBTYPE_CLEAR:
            case JOBTYPE_HAUL_TO_BLUEPRINT: {
                color = YELLOW;
                if (job->step == STEP_MOVING_TO_PICKUP || job->step == STEP_CARRYING) {
                    if (job->targetItem >= 0 && items[job->targetItem].active) {
                        Item* item = &items[job->targetItem];
                        int itemX = (int)(item->x / CELL_SIZE);
                        int itemY = (int)(item->y / CELL_SIZE);
                        DrawLineToTile(msx, msy, itemX, itemY, (int)item->z, color);
                    }
                }
                if (job->step == STEP_CARRYING) {
                    if (job->type == JOBTYPE_HAUL && job->targetStockpile >= 0) {
                        DrawLineToTile(msx, msy, job->targetSlotX, job->targetSlotY, (int)m->z, color);
                    } else if (job->type == JOBTYPE_HAUL_TO_BLUEPRINT && job->targetBlueprint >= 0) {
                        Blueprint* bp = &blueprints[job->targetBlueprint];
                        if (bp->active) {
                            DrawLineToTile(msx, msy, bp->x, bp->y, bp->z, color);
                        }
                    }
                }
                break;
            }
            case JOBTYPE_CRAFT: {
                color = ORANGE;
                if (job->step == CRAFT_STEP_MOVING_TO_INPUT || job->step == CRAFT_STEP_PICKING_UP) {
                    if (job->targetItem >= 0 && items[job->targetItem].active) {
                        Item* item = &items[job->targetItem];
                        int itemX = (int)(item->x / CELL_SIZE);
                        int itemY = (int)(item->y / CELL_SIZE);
                        DrawLineToTile(msx, msy, itemX, itemY, (int)item->z, color);
                    }
                } else if (job->step == CRAFT_STEP_MOVING_TO_FUEL || job->step == CRAFT_STEP_PICKING_UP_FUEL) {
                    if (job->fuelItem >= 0 && items[job->fuelItem].active) {
                        Item* fuelItem = &items[job->fuelItem];
                        int fuelX = (int)(fuelItem->x / CELL_SIZE);
                        int fuelY = (int)(fuelItem->y / CELL_SIZE);
                        DrawLineToTile(msx, msy, fuelX, fuelY, (int)fuelItem->z, color);
                    }
                } else if (job->targetWorkshop >= 0 && job->targetWorkshop < MAX_WORKSHOPS) {
                    Workshop* ws = &workshops[job->targetWorkshop];
                    if (ws->active) {
                        DrawLineToTile(msx, msy, ws->workTileX, ws->workTileY, ws->z, color);
                    }
                }
                break;
            }
            default:
                break;
        }
    }
}

static void DrawItems(void) {
    float size = CELL_SIZE * zoom;
    int viewZ = currentViewZ;
    Color skyColor = GetSkyColorForTime(timeOfDay);

    for (int i = 0; i < itemHighWaterMark; i++) {
        Item* item = &items[i];
        if (!item->active) continue;
        if (item->state == ITEM_CARRIED) continue;
        if (item->state == ITEM_IN_STOCKPILE) continue;

        int itemZ = (int)item->z;
        if (itemZ > viewZ || itemZ < viewZ - MAX_VISIBLE_DEPTH) continue;

        int cellX = (int)(item->x / CELL_SIZE);
        int cellY = (int)(item->y / CELL_SIZE);

        // Visibility check for items below current view
        if (itemZ < viewZ) {
            if (!IsCellVisibleFromAbove(cellX, cellY, itemZ + 1, viewZ)) continue;
        }

        float sx = offset.x + item->x * zoom;
        float sy = offset.y + item->y * zoom;

        int sprite = ItemSpriteForTypeMaterial(item->type, item->material);
        float itemSize = size * ITEM_SIZE_GROUND;
        Rectangle src = SpriteGetRect(sprite);
        Rectangle dest = { sx - itemSize/2, sy - itemSize/2, itemSize, itemSize };

        Color lightTint = GetLightColor(cellX, cellY, itemZ, skyColor);
        Color tint = MultiplyColor(MaterialTint((MaterialType)item->material), lightTint);
        tint = MultiplyColor(tint, ItemTypeTint(item->type));
        if (item->reservedBy >= 0) {
            tint = MultiplyColor(tint, (Color){200, 200, 200, 255});
        }
        // Apply depth tinting
        if (itemZ < viewZ) {
            tint = MultiplyColor(tint, GetDepthTint(itemZ, viewZ));
            tint = FloorDarkenTint(tint);
        }
        Color borderTint = MultiplyColor(ItemBorderTint(item->type), lightTint);
        if (itemZ < viewZ) {
            borderTint = MultiplyColor(borderTint, GetDepthTint(itemZ, viewZ));
            borderTint = FloorDarkenTint(borderTint);
        }
        if (ItemUsesBorder(item->type)) {
            DrawItemWithBorder(sprite, dest, tint, borderTint);
        } else {
            DrawTexturePro(atlas, src, dest, (Vector2){0, 0}, 0, tint);
        }
    }
}

static void DrawLightSources(void) {
    float size = CELL_SIZE * zoom;
    int viewZ = currentViewZ;

    for (int i = 0; i < lightSourceCount; i++) {
        LightSource* src = &lightSources[i];
        if (!src->active) continue;

        int srcZ = src->z;
        if (srcZ > viewZ || srcZ < viewZ - MAX_VISIBLE_DEPTH) continue;

        // Visibility check for light sources below current view
        if (srcZ < viewZ) {
            if (!IsCellVisibleFromAbove(src->x, src->y, srcZ + 1, viewZ)) continue;
        }

        // Convert cell position to screen position (center of cell)
        float sx = offset.x + (src->x + 0.5f) * size;
        float sy = offset.y + (src->y + 0.5f) * size;

        // Use middle-dot sprite
        int sprite = SPRITE_middle_dot;
        float torchSize = size * 0.6f;  // 60% of cell size for visibility
        Rectangle spriteRect = SpriteGetRect(sprite);
        Rectangle dest = { sx - torchSize/2, sy - torchSize/2, torchSize, torchSize };

        // Tint the sprite with the light source's color
        Color tint = {src->r, src->g, src->b, 255};
        
        // Apply depth tinting if below current view level
        if (srcZ < viewZ) {
            tint = MultiplyColor(tint, GetDepthTint(srcZ, viewZ));
            tint = FloorDarkenTint(tint);
        }

        DrawTexturePro(atlas, spriteRect, dest, (Vector2){0, 0}, 0, tint);
    }
}

static void DrawGatherZones(void) {
    float size = CELL_SIZE * zoom;
    int viewZ = currentViewZ;

    for (int i = 0; i < MAX_GATHER_ZONES; i++) {
        GatherZone* gz = &gatherZones[i];
        if (!gz->active) continue;
        if (gz->z > viewZ || gz->z < viewZ - MAX_VISIBLE_DEPTH) continue;

        // Simple visibility: skip if any cell in the zone is blocked
        bool belowView = gz->z < viewZ;
        if (belowView && !IsCellVisibleFromAbove(gz->x, gz->y, gz->z + 1, viewZ)) continue;

        float sx = offset.x + gz->x * size;
        float sy = offset.y + gz->y * size;
        float w = gz->width * size;
        float h = gz->height * size;

        Color fill = (Color){255, 180, 50, 40};
        Color outline = (Color){255, 180, 50, 180};
        if (belowView) {
            Color dt = GetDepthTint(gz->z, viewZ);
            fill = MultiplyColor(fill, dt);
            outline = MultiplyColor(outline, dt);
        }
        DrawRectangle((int)sx, (int)sy, (int)w, (int)h, fill);
        DrawRectangleLinesEx((Rectangle){sx, sy, w, h}, 2.0f, outline);
    }
}

static void DrawStockpileTiles(void) {
    float size = CELL_SIZE * zoom;
    int viewZ = currentViewZ;
    Color skyColor = GetSkyColorForTime(timeOfDay);

    for (int i = 0; i < MAX_STOCKPILES; i++) {
        Stockpile* sp = &stockpiles[i];
        if (!sp->active) continue;
        if (sp->z > viewZ || sp->z < viewZ - MAX_VISIBLE_DEPTH) continue;

        // Visibility check for stockpiles below current view
        bool belowView = sp->z < viewZ;

        for (int dy = 0; dy < sp->height; dy++) {
            for (int dx = 0; dx < sp->width; dx++) {
                int slotIdx = dy * sp->width + dx;
                if (!sp->cells[slotIdx]) continue;

                int gx = sp->x + dx;
                int gy = sp->y + dy;

                if (belowView && !IsCellVisibleFromAbove(gx, gy, sp->z + 1, viewZ)) continue;

                float sx = offset.x + gx * size;
                float sy = offset.y + gy * size;

                Rectangle src = SpriteGetRect(SPRITE_stockpile);
                Rectangle dest = { sx, sy, size, size };
                Color lightTint = GetLightColor(gx, gy, sp->z, skyColor);
                Color tint = MultiplyColor(WHITE, lightTint);
                if (belowView) {
                    tint = MultiplyColor(tint, GetDepthTint(sp->z, viewZ));
                    tint = FloorDarkenTint(tint);
                }
                DrawTexturePro(atlas, src, dest, (Vector2){0, 0}, 0, tint);

                if (i == hoveredStockpile) {
                    float pulse = (sinf(GetTime() * 4.0f) + 1.0f) * 0.5f;
                    unsigned char alpha = (unsigned char)(40 + pulse * 60);
                    DrawRectangle((int)sx, (int)sy, (int)size, (int)size, (Color){100, 255, 100, alpha});
                }
            }
        }

        if (IsStockpileOverfull(i)) {
            float sx = offset.x + sp->x * size;
            float sy = offset.y + sp->y * size;
            float w = sp->width * size;
            float h = sp->height * size;
            DrawRectangleLinesEx((Rectangle){sx, sy, w, h}, 2.0f, RED);
        }
    }
}

static void DrawStockpileItems(void) {
    float size = CELL_SIZE * zoom;
    int viewZ = currentViewZ;
    Color skyColor = GetSkyColorForTime(timeOfDay);

    for (int i = 0; i < MAX_STOCKPILES; i++) {
        Stockpile* sp = &stockpiles[i];
        if (!sp->active) continue;
        if (sp->z > viewZ || sp->z < viewZ - MAX_VISIBLE_DEPTH) continue;

        bool belowView = sp->z < viewZ;

        for (int dy = 0; dy < sp->height; dy++) {
            for (int dx = 0; dx < sp->width; dx++) {
                int slotIdx = dy * sp->width + dx;
                if (!sp->cells[slotIdx]) continue;

                int gx = sp->x + dx;
                int gy = sp->y + dy;

                // Check if slot has a container or bare items
                int slotItemIdx = sp->slots[slotIdx];
                bool isContainer = slotItemIdx >= 0 && slotItemIdx < MAX_ITEMS
                    && items[slotItemIdx].active && ItemIsContainer(items[slotItemIdx].type);

                if (!isContainer && sp->slotCounts[slotIdx] <= 0) continue;
                if (isContainer && sp->slots[slotIdx] < 0) continue;

                if (belowView && !IsCellVisibleFromAbove(gx, gy, sp->z + 1, viewZ)) continue;

                float sx = offset.x + gx * size;
                float sy = offset.y + gy * size;

                Color lightTint = GetLightColor(gx, gy, sp->z, skyColor);

                if (isContainer) {
                    // Draw container sprite (single, larger)
                    ItemType ctype = items[slotItemIdx].type;
                    int sprite = ItemSpriteForTypeMaterial(ctype, items[slotItemIdx].material);
                    float itemSize = size * ITEM_SIZE_STOCKPILE * 1.2f;
                    Color tint = MultiplyColor(MaterialTint((MaterialType)items[slotItemIdx].material), lightTint);
                    tint = MultiplyColor(tint, ItemTypeTint(ctype));
                    Color borderTint = MultiplyColor(ItemBorderTint(ctype), lightTint);
                    if (belowView) {
                        tint = MultiplyColor(tint, GetDepthTint(sp->z, viewZ));
                        tint = FloorDarkenTint(tint);
                        borderTint = MultiplyColor(borderTint, GetDepthTint(sp->z, viewZ));
                        borderTint = FloorDarkenTint(borderTint);
                    }
                    float itemX = sx + size * 0.5f - itemSize * 0.5f;
                    float itemY = sy + size * 0.5f - itemSize * 0.5f;
                    Rectangle destItem = { itemX, itemY, itemSize, itemSize };
                    if (ItemUsesBorder(ctype)) {
                        DrawItemWithBorder(sprite, destItem, tint, borderTint);
                    } else {
                        Rectangle srcItem = SpriteGetRect(sprite);
                        DrawTexturePro(atlas, srcItem, destItem, (Vector2){0, 0}, 0, tint);
                    }
                } else {
                    // Draw bare item stacks (existing logic)
                    int count = sp->slotCounts[slotIdx];
                    ItemType type = sp->slotTypes[slotIdx];
                    int sprite = ItemSpriteForTypeMaterial(type, sp->slotMaterials[slotIdx]);
                    int visibleCount = count > 5 ? 5 : count;
                    float itemSize = size * ITEM_SIZE_STOCKPILE;
                    float stackOffset = size * 0.08f;
                    Color tint = MultiplyColor(MaterialTint((MaterialType)sp->slotMaterials[slotIdx]), lightTint);
                    tint = MultiplyColor(tint, ItemTypeTint(type));
                    Color borderTint = MultiplyColor(ItemBorderTint(type), lightTint);
                    if (belowView) {
                        tint = MultiplyColor(tint, GetDepthTint(sp->z, viewZ));
                        tint = FloorDarkenTint(tint);
                        borderTint = MultiplyColor(borderTint, GetDepthTint(sp->z, viewZ));
                        borderTint = FloorDarkenTint(borderTint);
                    }

                    for (int s = 0; s < visibleCount; s++) {
                        float itemX = sx + size * 0.5f - itemSize * 0.5f - s * stackOffset;
                        float itemY = sy + size * 0.5f - itemSize * 0.5f - s * stackOffset;
                        Rectangle destItem = { itemX, itemY, itemSize, itemSize };
                        if (ItemUsesBorder(type)) {
                            DrawItemWithBorder(sprite, destItem, tint, borderTint);
                        } else {
                            Rectangle srcItem = SpriteGetRect(sprite);
                            DrawTexturePro(atlas, srcItem, destItem, (Vector2){0, 0}, 0, tint);
                        }
                    }
                }
            }
        }
    }
}

static void DrawFurniture(void) {
    float size = CELL_SIZE * zoom;
    int viewZ = currentViewZ;
    Color skyColor = GetSkyColorForTime(timeOfDay);

    for (int i = 0; i < MAX_FURNITURE; i++) {
        Furniture* f = &furniture[i];
        if (!f->active) continue;
        if (f->z > viewZ || f->z < viewZ - MAX_VISIBLE_DEPTH) continue;

        bool belowView = f->z < viewZ;
        if (belowView && !IsCellVisibleFromAbove(f->x, f->y, f->z + 1, viewZ)) continue;

        float sx = offset.x + f->x * size;
        float sy = offset.y + f->y * size;

        // Placeholder tint per furniture type
        Color tint;
        switch (f->type) {
            case FURNITURE_LEAF_PILE:  tint = (Color){100, 160, 80, 255};  break;  // Green
            case FURNITURE_GRASS_PILE: tint = (Color){160, 180, 80, 255};  break;  // Yellow-green
            case FURNITURE_PLANK_BED:  tint = (Color){140, 100, 60, 255};  break;  // Brown
            case FURNITURE_CHAIR:      tint = (Color){180, 160, 120, 255}; break;  // Tan
            default:                   tint = WHITE; break;
        }

        Color lightTint = GetLightColor(f->x, f->y, f->z, skyColor);
        tint = MultiplyColor(tint, lightTint);

        if (belowView) {
            tint = MultiplyColor(tint, GetDepthTint(f->z, viewZ));
            tint = FloorDarkenTint(tint);
        }

        Rectangle src = SpriteGetRect(SPRITE_generic);
        Rectangle dest = { sx, sy, size, size };
        DrawTexturePro(atlas, src, dest, (Vector2){0, 0}, 0, tint);
    }
}

static void DrawWorkshops(void) {
    float size = CELL_SIZE * zoom;
    int viewZ = currentViewZ;
    Color skyColor = GetSkyColorForTime(timeOfDay);

    for (int i = 0; i < MAX_WORKSHOPS; i++) {
        Workshop* ws = &workshops[i];
        if (!ws->active) continue;
        if (ws->z > viewZ || ws->z < viewZ - MAX_VISIBLE_DEPTH) continue;

        bool belowView = ws->z < viewZ;

        // Draw workshop footprint based on template
        for (int dy = 0; dy < ws->height; dy++) {
            for (int dx = 0; dx < ws->width; dx++) {
                int gx = ws->x + dx;
                int gy = ws->y + dy;

                if (belowView && !IsCellVisibleFromAbove(gx, gy, ws->z + 1, viewZ)) continue;

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

                Color lightTint = GetLightColor(gx, gy, ws->z, skyColor);
                tint = MultiplyColor(tint, lightTint);

                if (ws->markedForDeconstruct) {
                    tint = MultiplyColor(tint, (Color){255, 80, 80, 255});
                }

                if (belowView) {
                    tint = MultiplyColor(tint, GetDepthTint(ws->z, viewZ));
                    tint = FloorDarkenTint(tint);
                }

                Rectangle src = SpriteGetRect(SPRITE_generic);
                Rectangle dest = { sx, sy, size, size };
                DrawTexturePro(atlas, src, dest, (Vector2){0, 0}, 0, tint);
            }
        }

        // Draw craft/passive progress bar
        float progress = -1.0f;
        Color barColor = YELLOW;

        if (ws->assignedCrafter >= 0) {
            Mover* m = &movers[ws->assignedCrafter];
            if (m->currentJobId >= 0) {
                Job* job = GetJob(m->currentJobId);
                if (job && job->type == JOBTYPE_CRAFT && job->step == CRAFT_STEP_WORKING) {
                    progress = job->progress;
                    barColor = YELLOW;
                }
            }
        }
        if (progress < 0.0f && ws->passiveProgress > 0.0f) {
            progress = ws->passiveProgress;
            barColor = ORANGE;
        }
        if (progress < 0.0f && ws->assignedDeconstructor >= 0) {
            Mover* dm = &movers[ws->assignedDeconstructor];
            if (dm->currentJobId >= 0) {
                Job* dj = GetJob(dm->currentJobId);
                if (dj && dj->type == JOBTYPE_DECONSTRUCT_WORKSHOP && dj->step == STEP_WORKING && dj->workRequired > 0) {
                    progress = dj->progress / dj->workRequired;
                    barColor = RED;
                }
            }
        }

        if (progress > 0.0f) {
            float sx = offset.x + ws->x * size;
            float sy = offset.y + ws->y * size - 6;
            float barWidth = ws->width * size;
            DrawRectangle((int)sx, (int)sy, (int)barWidth, 4, DARKGRAY);
            DrawRectangle((int)sx, (int)sy, (int)(barWidth * progress), 4, barColor);
        }
    }
    
    // Draw path of assigned crafter when hovering over a workshop
    if (hoveredWorkshop >= 0 && hoveredWorkshop < MAX_WORKSHOPS) {
        Workshop* ws = &workshops[hoveredWorkshop];
        if (ws->active && ws->assignedCrafter >= 0) {
            Mover* m = &movers[ws->assignedCrafter];
            if (m->active && m->pathIndex >= 0) {
                float msx = offset.x + m->x * zoom;
                float msy = offset.y + m->y * zoom;
                DrawMoverPath(m, msx, msy, viewZ, YELLOW, 2.0f, 2.0f, 0.6f);
                DrawCircle((int)msx, (int)msy, 4.0f * zoom, YELLOW);
            }
        }
    }
}

// Draw highlighting when in stockpile linking mode
static void DrawLinkingModeHighlights(void) {
    if (workSubMode != SUBMODE_LINKING_STOCKPILES || linkingWorkshopIdx < 0) return;
    if (linkingWorkshopIdx >= workshopCount) return;
    
    Workshop* ws = &workshops[linkingWorkshopIdx];
    if (!ws->active) return;
    
    float size = CELL_SIZE * zoom;
    
    // Highlight workshop being linked (yellow pulsing outline)
    float pulse = sinf((float)GetTime() * 3.0f) * 0.3f + 0.7f;  // 0.7-1.0
    Color highlightColor = ColorAlpha(YELLOW, pulse);
    
    for (int dy = 0; dy < ws->height; dy++) {
        for (int dx = 0; dx < ws->width; dx++) {
            int wx = ws->x + dx;
            int wy = ws->y + dy;
            
            if ((int)ws->z != currentViewZ) continue;  // Only highlight on current z-level
            
            float sx = offset.x + wx * size;
            float sy = offset.y + wy * size;
            
            DrawRectangleLines((int)sx, (int)sy, (int)size, (int)size, highlightColor);
        }
    }
    
    // Highlight hovered stockpile (green if linkable, red if not)
    if (hoveredStockpile >= 0 && hoveredStockpile < stockpileCount) {
        Stockpile* sp = &stockpiles[hoveredStockpile];
        if (sp->active && (int)sp->z == currentViewZ) {
            bool alreadyLinked = IsStockpileLinked(linkingWorkshopIdx, hoveredStockpile);
            bool slotsFull = ws->linkedInputCount >= MAX_LINKED_STOCKPILES;
            
            Color stockpileHighlight = GREEN;
            if (alreadyLinked || slotsFull) {
                stockpileHighlight = RED;
            }
            stockpileHighlight = ColorAlpha(stockpileHighlight, 0.8f);
            
            // Draw outline around stockpile
            for (int dy = 0; dy < sp->height; dy++) {
                for (int dx = 0; dx < sp->width; dx++) {
                    int sx = (int)(offset.x + (sp->x + dx) * size);
                    int sy = (int)(offset.y + (sp->y + dy) * size);
                    DrawRectangleLines(sx, sy, (int)size, (int)size, stockpileHighlight);
                }
            }
        }
    }
}

// Draw status icons on workshop tiles based on their current state
static void DrawWorkshopStatusOverlay(void) {
    float size = CELL_SIZE * zoom;
    int viewZ = currentViewZ;
    
    for (int i = 0; i < workshopCount; i++) {
        Workshop* ws = &workshops[i];
        if (!ws->active) continue;
        if ((int)ws->z != viewZ) continue;
        
        // Determine icon and color based on visual state
        int spriteIdx = -1;
        Color tint = WHITE;
        bool pulse = false;
        
        switch (ws->visualState) {
            case WORKSHOP_VISUAL_WORKING:
                // No icon when working - silence is golden
                continue;
                
            case WORKSHOP_VISUAL_OUTPUT_FULL:
                spriteIdx = SPRITE_full_block;        // full block for "blocked"
                tint = (Color){255, 100, 100, 255};   // red
                break;
                
            case WORKSHOP_VISUAL_INPUT_EMPTY:
                spriteIdx = SPRITE_division;          // division sign as "waiting"
                tint = (Color){255, 200, 50, 255};    // yellow
                break;
                
            case WORKSHOP_VISUAL_NO_WORKER:
                spriteIdx = SPRITE_light_shade;       // light shade for "idle/no worker"
                tint = (Color){150, 150, 150, 200};   // gray, semi-transparent
                break;
        }
        
        if (spriteIdx < 0) continue;
        
        // Apply pulse effect for working state
        if (pulse) {
            float pulseVal = sinf((float)GetTime() * 4.0f) * 0.3f + 0.7f;  // 0.7-1.0
            tint.a = (unsigned char)(255 * pulseVal);
        }
        
        // Draw icon at center of workshop
        float centerX = ws->x + ws->width * 0.5f;
        float centerY = ws->y + ws->height * 0.5f;
        
        float sx = offset.x + centerX * size;
        float sy = offset.y + centerY * size;
        
        // Icon is half the cell size, centered
        float iconSize = size * 0.5f;
        sx -= iconSize * 0.5f;
        sy -= iconSize * 0.5f;
        
        Rectangle src = SpriteGetRect(spriteIdx);
        Rectangle dest = { sx, sy, iconSize, iconSize };
        DrawTexturePro(atlas, src, dest, (Vector2){0, 0}, 0, tint);
    }
}

static void DrawHaulDestinations(void) {
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

// Designation overlay + progress bar colors, indexed by DesignationType
// {overlayColor, progressBarColor} — {0,0,0,0} means no overlay for that type
static const Color designationOverlayColors[DESIGNATION_TYPE_COUNT] = {
    [DESIGNATION_NONE]           = {0, 0, 0, 0},
    [DESIGNATION_MINE]           = {100, 220, 255, 200},
    [DESIGNATION_CHANNEL]        = {255, 150, 200, 200},
    [DESIGNATION_DIG_RAMP]       = {180, 120, 220, 200},
    [DESIGNATION_REMOVE_FLOOR]   = {255, 220, 100, 200},
    [DESIGNATION_REMOVE_RAMP]    = {100, 220, 220, 200},
    [DESIGNATION_CHOP]           = {180, 100, 50, 200},
    [DESIGNATION_CHOP_FELLED]    = {140, 80, 40, 200},
    [DESIGNATION_GATHER_SAPLING] = {150, 255, 150, 200},
    [DESIGNATION_PLANT_SAPLING]  = {50, 180, 80, 200},
    [DESIGNATION_GATHER_GRASS]   = {200, 230, 100, 200},
    [DESIGNATION_GATHER_TREE]    = {160, 180, 80, 200},
    [DESIGNATION_CLEAN]          = {180, 220, 255, 200},
    [DESIGNATION_HARVEST_BERRY]  = {200, 100, 220, 200},
    [DESIGNATION_KNAP]           = {200, 150, 100, 200},
};

static const Color designationProgressColors[DESIGNATION_TYPE_COUNT] = {
    [DESIGNATION_NONE]           = {0, 0, 0, 0},
    [DESIGNATION_MINE]           = {135, 206, 235, 255},  // SKYBLUE
    [DESIGNATION_CHANNEL]        = {255, 0, 255, 255},    // MAGENTA
    [DESIGNATION_DIG_RAMP]       = {160, 100, 200, 255},
    [DESIGNATION_REMOVE_FLOOR]   = {255, 203, 0, 255},    // GOLD
    [DESIGNATION_REMOVE_RAMP]    = {50, 200, 200, 255},
    [DESIGNATION_CHOP]           = {200, 120, 60, 255},
    [DESIGNATION_CHOP_FELLED]    = {170, 100, 50, 255},
    [DESIGNATION_GATHER_SAPLING] = {100, 220, 100, 255},
    [DESIGNATION_PLANT_SAPLING]  = {30, 150, 60, 255},
    [DESIGNATION_GATHER_GRASS]   = {160, 200, 60, 255},
    [DESIGNATION_GATHER_TREE]    = {130, 160, 50, 255},
    [DESIGNATION_CLEAN]          = {150, 200, 240, 255},
    [DESIGNATION_HARVEST_BERRY]  = {220, 120, 240, 255},
    [DESIGNATION_KNAP]           = {180, 130, 80, 255},
};

// Active job overlay colors, indexed by JobType
// {0,0,0,0} means no overlay for that job type
static const Color jobOverlayColors[JOBTYPE_COUNT] = {
    [JOBTYPE_MINE]           = {255, 200, 100, 180},
    [JOBTYPE_CHANNEL]        = {255, 180, 150, 180},
    [JOBTYPE_DIG_RAMP]       = {200, 150, 240, 180},
    [JOBTYPE_REMOVE_FLOOR]   = {255, 200, 80, 180},
    [JOBTYPE_REMOVE_RAMP]    = {80, 200, 200, 180},
    [JOBTYPE_CHOP]           = {220, 140, 80, 180},
    [JOBTYPE_GATHER_SAPLING] = {180, 255, 180, 180},
    [JOBTYPE_PLANT_SAPLING]  = {80, 200, 100, 180},
    [JOBTYPE_CLEAN]          = {200, 230, 255, 180},
    [JOBTYPE_HARVEST_BERRY]  = {220, 140, 240, 180},
};

static void DrawMiningDesignations(void) {
    // Early exit if no designations
    if (activeDesignationCount == 0) return;
    
    float size = CELL_SIZE * zoom;
    int viewZ = currentViewZ;
    Rectangle spriteSrc = SpriteGetRect(SPRITE_stockpile);

    // Draw designation overlays + progress bars
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            Designation* d = GetDesignation(x, y, viewZ);
            if (!d) continue;
            if (d->type <= DESIGNATION_NONE || d->type >= DESIGNATION_TYPE_COUNT) continue;

            Color overlay = designationOverlayColors[d->type];
            if (overlay.a == 0) continue;

            float sx = offset.x + x * size;
            float sy = offset.y + y * size;
            Rectangle dest = { sx, sy, size, size };
            DrawTexturePro(atlas, spriteSrc, dest, (Vector2){0, 0}, 0, overlay);

            if (d->progress > 0.0f) {
                float barWidth = size * 0.8f;
                float barHeight = 4.0f;
                float barX = sx + size * 0.1f;
                float barY = sy + size - 8.0f;
                DrawRectangle((int)barX, (int)barY, (int)barWidth, (int)barHeight, DARKGRAY);
                DrawRectangle((int)barX, (int)barY, (int)(barWidth * d->progress), (int)barHeight,
                              designationProgressColors[d->type]);
            }
        }
    }

    // Draw active job overlays (brighter tint for assigned/in-progress jobs)
    for (int i = 0; i < activeJobCount; i++) {
        int jobIdx = activeJobList[i];
        Job* job = &jobs[jobIdx];
        if (job->type <= JOBTYPE_NONE || job->type >= JOBTYPE_COUNT) continue;
        if (job->targetMineZ != viewZ) continue;

        Color overlay = jobOverlayColors[job->type];
        if (overlay.a == 0) continue;

        float sx = offset.x + job->targetMineX * size;
        float sy = offset.y + job->targetMineY * size;
        Rectangle dest = { sx, sy, size, size };
        DrawTexturePro(atlas, spriteSrc, dest, (Vector2){0, 0}, 0, overlay);
    }
    
    // Draw path of assigned mover when hovering over a designation
    if (hoveredDesignationX >= 0) {
        Designation* d = GetDesignation(hoveredDesignationX, hoveredDesignationY, hoveredDesignationZ);
        if (d && d->assignedMover >= 0) {
            Mover* m = &movers[d->assignedMover];
            if (m->active && m->pathIndex >= 0) {
                float msx = offset.x + m->x * zoom;
                float msy = offset.y + m->y * zoom;
                DrawMoverPath(m, msx, msy, viewZ, ORANGE, 2.0f, 2.0f, 0.6f);
                DrawCircle((int)msx, (int)msy, 4.0f * zoom, ORANGE);
            }
        }
    }
}

static void DrawBlueprints(void) {
    float size = CELL_SIZE * zoom;
    int viewZ = currentViewZ;

    for (int i = 0; i < MAX_BLUEPRINTS; i++) {
        Blueprint* bp = &blueprints[i];
        if (!bp->active || bp->z != viewZ) continue;

        float sx = offset.x + bp->x * size;
        float sy = offset.y + bp->y * size;

        Color tint;
        if (bp->state == BLUEPRINT_CLEARING) {
            tint = (Color){255, 180, 80, 200};  // orange — clearing items
        } else if (bp->state == BLUEPRINT_AWAITING_MATERIALS) {
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

        if (bp->state == BLUEPRINT_CLEARING) {
            const char* text = "CLR";
            int textW = MeasureTextUI(text, 10);
            DrawTextShadow(text, (int)(sx + size/2 - textW/2), (int)(sy + 2), 10, WHITE);
        } else if (bp->state == BLUEPRINT_AWAITING_MATERIALS) {
            int delivered = BlueprintStageDeliveredCount(bp);
            int required = BlueprintStageRequiredCount(bp);
            const char* text = TextFormat("%d/%d", delivered, required);
            int textW = MeasureTextUI(text, 10);
            DrawTextShadow(text, (int)(sx + size/2 - textW/2), (int)(sy + 2), 10, WHITE);
        }

        // Stage indicator for multi-stage recipes
        {
            const ConstructionRecipe* recipe = GetConstructionRecipe(bp->recipeIndex);
            if (recipe && recipe->stageCount > 1) {
                const char* stageText = TextFormat("S%d", bp->stage + 1);
                DrawTextShadow(stageText, (int)(sx + size - 14), (int)(sy + size - 12), 8, YELLOW);
            }

            // Workshop blueprint: draw ghost footprint over all template tiles
            if (recipe && recipe->buildCategory == BUILD_WORKSHOP) {
                const WorkshopDef* def = &workshopDefs[bp->workshopType];
                int ox = bp->workshopOriginX;
                int oy = bp->workshopOriginY;
                for (int ty = 0; ty < def->height; ty++) {
                    for (int tx = 0; tx < def->width; tx++) {
                        int wx = ox + tx;
                        int wy = oy + ty;
                        if (wx == bp->x && wy == bp->y) continue; // skip work tile (already drawn)
                        char c = def->template[ty * def->width + tx];
                        float tsx = offset.x + wx * size;
                        float tsy = offset.y + wy * size;
                        Color ghost = (c == WT_BLOCK)
                            ? (Color){tint.r, tint.g, tint.b, 120}
                            : (Color){tint.r, tint.g, tint.b, 80};
                        Rectangle gsrc = SpriteGetRect(SPRITE_stockpile);
                        Rectangle gdest = { tsx, tsy, size, size };
                        DrawTexturePro(atlas, gsrc, gdest, (Vector2){0, 0}, 0, ghost);
                    }
                }
            }
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

static void DrawTerrainBrushPreview(void) {
    if (inputAction != ACTION_SANDBOX_SCULPT) return;

    Vector2 gp = ScreenToGrid(GetMousePosition());
    int mouseX = (int)gp.x;
    int mouseY = (int)gp.y;

    float size = CELL_SIZE * zoom;
    int radius = terrainBrushRadius;
    int radiusSq = radius * radius;

    // Color by mode (left=raise/green, right=lower/red)
    bool isLowerMode = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);

    Color fillColor, lineColor;
    if (isLowerMode) {
        fillColor = (Color){220, 80, 80, 60};      // Red for lower
        lineColor = (Color){255, 100, 100, 200};
    } else {
        fillColor = (Color){80, 220, 80, 60};      // Green for raise
        lineColor = (Color){100, 255, 100, 200};
    }

    // Draw affected cells
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            int distSq = dx * dx + dy * dy;
            if (distSq > radiusSq) continue;

            int x = mouseX + dx;
            int y = mouseY + dy;

            if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight) continue;

            float px = offset.x + x * size;
            float py = offset.y + y * size;

            // Validate operation
            int z = currentViewZ;
            bool valid = false;
            if (isLowerMode) {
                valid = (grid[z][y][x] == CELL_WALL && IsWallNatural(x, y, z));
            } else {
                valid = (grid[z][y][x] == CELL_AIR && z > 0 &&
                        CellIsSolid(grid[z-1][y][x]));
            }

            Color cellColor = valid ? fillColor : (Color){100, 100, 100, 40};
            DrawRectangle((int)px, (int)py, (int)size, (int)size, cellColor);
        }
    }

    // Circle outline
    float centerX = offset.x + (mouseX + 0.5f) * size;
    float centerY = offset.y + (mouseY + 0.5f) * size;
    float circleRadius = (radius + 0.5f) * size;
    DrawCircleLines((int)centerX, (int)centerY, circleRadius, lineColor);
}

static void DrawWorkshopPlacementPreview(void) {
    // Only show when a specific workshop build action is selected
    WorkshopType type = -1;
    switch (inputAction) {
        case ACTION_WORK_WORKSHOP_CAMPFIRE:     type = WORKSHOP_CAMPFIRE; break;
        case ACTION_WORK_WORKSHOP_DRYING_RACK:  type = WORKSHOP_DRYING_RACK; break;
        case ACTION_WORK_WORKSHOP_ROPE_MAKER:   type = WORKSHOP_ROPE_MAKER; break;
        case ACTION_WORK_WORKSHOP_CHARCOAL_PIT: type = WORKSHOP_CHARCOAL_PIT; break;
        case ACTION_WORK_WORKSHOP_HEARTH:       type = WORKSHOP_HEARTH; break;
        case ACTION_WORK_WORKSHOP_STONECUTTER:  type = WORKSHOP_STONECUTTER; break;
        case ACTION_WORK_WORKSHOP_SAWMILL:      type = WORKSHOP_SAWMILL; break;
        case ACTION_WORK_WORKSHOP_KILN:         type = WORKSHOP_KILN; break;
        case ACTION_WORK_WORKSHOP_CARPENTER:    type = WORKSHOP_CARPENTER; break;
        // Also show for sandbox draw workshop actions
        case ACTION_DRAW_WORKSHOP_CAMPFIRE:     type = WORKSHOP_CAMPFIRE; break;
        case ACTION_DRAW_WORKSHOP_DRYING_RACK:  type = WORKSHOP_DRYING_RACK; break;
        case ACTION_DRAW_WORKSHOP_ROPE_MAKER:   type = WORKSHOP_ROPE_MAKER; break;
        case ACTION_DRAW_WORKSHOP_CHARCOAL_PIT: type = WORKSHOP_CHARCOAL_PIT; break;
        case ACTION_DRAW_WORKSHOP_HEARTH:       type = WORKSHOP_HEARTH; break;
        case ACTION_DRAW_WORKSHOP_STONECUTTER:  type = WORKSHOP_STONECUTTER; break;
        case ACTION_DRAW_WORKSHOP_SAWMILL:      type = WORKSHOP_SAWMILL; break;
        case ACTION_DRAW_WORKSHOP_KILN:         type = WORKSHOP_KILN; break;
        case ACTION_DRAW_WORKSHOP_CARPENTER:    type = WORKSHOP_CARPENTER; break;
        default: return;
    }

    const WorkshopDef* def = &workshopDefs[type];
    Vector2 gp = ScreenToGrid(GetMousePosition());
    int mouseX = (int)gp.x;
    int mouseY = (int)gp.y;
    int z = currentViewZ;
    float size = CELL_SIZE * zoom;

    for (int ty = 0; ty < def->height; ty++) {
        for (int tx = 0; tx < def->width; tx++) {
            int cx = mouseX + tx;
            int cy = mouseY + ty;
            if (cx < 0 || cx >= gridWidth || cy < 0 || cy >= gridHeight) continue;

            float px = offset.x + cx * size;
            float py = offset.y + cy * size;

            char c = def->template[ty * def->width + tx];

            // Check if placement is valid at this cell
            bool valid = IsCellWalkableAt(z, cy, cx) && FindWorkshopAt(cx, cy, z) < 0;

            Color fillColor;
            if (!valid) {
                fillColor = (Color){220, 60, 60, 80};   // Red = blocked
            } else if (c == WT_BLOCK) {
                fillColor = (Color){80, 140, 220, 100};  // Blue = machinery
            } else if (c == WT_WORK) {
                fillColor = (Color){80, 220, 80, 100};   // Green = work tile
            } else if (c == WT_OUTPUT) {
                fillColor = (Color){220, 200, 80, 100};  // Yellow = output
            } else if (c == WT_FUEL) {
                fillColor = (Color){220, 140, 60, 100};  // Orange = fuel
            } else {
                fillColor = (Color){150, 150, 150, 60};  // Gray = floor
            }

            DrawRectangle((int)px, (int)py, (int)size, (int)size, fillColor);
            DrawRectangleLines((int)px, (int)py, (int)size, (int)size,
                              (Color){fillColor.r, fillColor.g, fillColor.b, 180});
        }
    }
}

// =============================================================================
// Phase 5: Lightning Flash & Mist Rendering
// =============================================================================

static void DrawLightningFlash(void) {
    float intensity = GetLightningFlashIntensity();
    if (intensity <= 0.0f) return;
    
    // Full-screen white flash
    int alpha = (int)(intensity * 180.0f);  // Max 180 alpha for a bright flash
    if (alpha > 255) alpha = 255;
    
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){255, 255, 255, (unsigned char)alpha});
}

static void DrawMist(void) {
    float mistIntensity = GetMistIntensity();
    if (mistIntensity <= 0.01f) return;

    // Uniform fog overlay with subtle per-cell variation
    float size = CELL_SIZE * zoom;
    int z = currentViewZ;

    int minX, minY, maxX, maxY;
    GetVisibleCellRange(size, &minX, &minY, &maxX, &maxY);

    // Slow drift over time for subtle animation
    float t = (float)GetTime() * 0.3f;

    for (int y = minY; y < maxY; y++) {
        for (int x = minX; x < maxX; x++) {
            if (!IsCellVisibleFromAbove(x, y, z, z + 1)) continue;

            // Per-cell hash for spatial variation (no radial pattern)
            unsigned int h = (unsigned int)(x * 7919 + y * 6271);
            h ^= h >> 13; h *= 0x5bd1e995; h ^= h >> 15;
            float cellNoise = (float)(h & 0xFF) / 255.0f;  // 0..1

            // Subtle variation: ±20% around base intensity
            float variation = 0.8f + cellNoise * 0.4f;
            float fogStrength = mistIntensity * variation;

            // Slight temporal shimmer using cell position + time
            float shimmer = sinf((float)x * 0.4f + t) * cosf((float)y * 0.3f + t * 0.7f);
            fogStrength += shimmer * 0.03f * mistIntensity;

            int alpha = (int)(fogStrength * 160.0f);
            if (alpha < 5) continue;
            if (alpha > 200) alpha = 200;

            Rectangle dest = {offset.x + x * size, offset.y + y * size, size, size};
            DrawRectangleRec(dest, (Color){200, 200, 210, (unsigned char)alpha});
        }
    }
}

