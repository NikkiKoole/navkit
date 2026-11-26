// core/input.c - Input handling with hierarchical mode system
#include "../game_state.h"
#include "../world/cell_defs.h"
#include "../world/designations.h"
#include "../world/material.h"
#include "../entities/workshops.h"
#include "../simulation/smoke.h"
#include "../simulation/steam.h"
#include "../simulation/temperature.h"
#include "../simulation/groundwear.h"
#include "../simulation/trees.h"
#include "../simulation/lighting.h"
#include "../simulation/plants.h"
#include "../simulation/farming.h"
#include "input_mode.h"
#include "action_registry.h"
#include "pie_menu.h"
#include "selection.h"
#include "global_keys.h"
#include "../entities/action_queue.h"
#include "../ui/commands.h"
#include "../entities/elevators.h"
#include "../entities/town.h"
#include "../entities/belts.h"
#include "../world/pipes.h"
#include "../world/mechanisms.h"

// Forward declaration
void HandleInput(void);

// Forward declarations
bool SaveWorld(const char* filename);
bool LoadWorld(const char* filename);

// Pile mode configuration
float soilPileRadius = 3.0f;  // How far soil can spread in pile mode

// Construction recipe selection (persists during session)
static int selectedWallRecipe = CONSTRUCTION_DRY_STONE_WALL;
static int selectedFloorRecipe = CONSTRUCTION_PLANK_FLOOR;
static int selectedLadderRecipe = CONSTRUCTION_LADDER;
static int selectedFurnitureRecipe = CONSTRUCTION_LEAF_PILE;
static int selectedDoorRecipe = CONSTRUCTION_LEAF_DOOR;

void SetSelectedFurnitureRecipe(int recipeIndex) {
    selectedFurnitureRecipe = recipeIndex;
}

int GetSelectedFurnitureRecipe(void) {
    return selectedFurnitureRecipe;
}

// Right-click tap detection for pie menu
static Vector2 rightClickStart = {0};
static double rightClickTime = 0.0;
#define RIGHT_TAP_MAX_DIST 5.0f
#define RIGHT_TAP_MAX_TIME 0.25

const char* GetSelectedWallRecipeName(void) {
    const ConstructionRecipe* recipe = GetConstructionRecipe(selectedWallRecipe);
    return recipe ? recipe->name : "?";
}

const char* GetSelectedFloorRecipeName(void) {
    const ConstructionRecipe* recipe = GetConstructionRecipe(selectedFloorRecipe);
    return recipe ? recipe->name : "?";
}

const char* GetSelectedLadderRecipeName(void) {
    const ConstructionRecipe* recipe = GetConstructionRecipe(selectedLadderRecipe);
    return recipe ? recipe->name : "?";
}

const char* GetSelectedFurnitureRecipeName(void) {
    const ConstructionRecipe* recipe = GetConstructionRecipe(selectedFurnitureRecipe);
    return recipe ? recipe->name : "?";
}

const char* GetSelectedDoorRecipeName(void) {
    const ConstructionRecipe* recipe = GetConstructionRecipe(selectedDoorRecipe);
    return recipe ? recipe->name : "?";
}

// ============================================================================
// Helper: Clamp coordinates to grid bounds
// ============================================================================
static void ClampToGrid(int* x1, int* y1, int* x2, int* y2) {
    if (*x1 < 0) *x1 = 0;
    if (*y1 < 0) *y1 = 0;
    if (*x2 >= gridWidth) *x2 = gridWidth - 1;
    if (*y2 >= gridHeight) *y2 = gridHeight - 1;
}

// ============================================================================
// Helper: Get drag rectangle (sorted coordinates)
// ============================================================================
static void GetDragRect(int* x1, int* y1, int* x2, int* y2) {
    Vector2 gp = ScreenToGrid(GetMousePosition());
    int x = (int)gp.x, y = (int)gp.y;

    *x1 = dragStartX < x ? dragStartX : x;
    *y1 = dragStartY < y ? dragStartY : y;
    *x2 = dragStartX > x ? dragStartX : x;
    *y2 = dragStartY > y ? dragStartY : y;

    ClampToGrid(x1, y1, x2, y2);
}

// Front view drag: X range from drag, Y fixed at dragStartY, Z range from drag start/current pick
static void GetDragRectFrontView(int* x1, int* y1, int* x2, int* y2, int* z1, int* z2, int curGx, int curGy, int curGz) {
    *x1 = dragStartX < curGx ? dragStartX : curGx;
    *x2 = dragStartX > curGx ? dragStartX : curGx;
    *y1 = dragStartY;   // depth (Y) fixed at drag-start layer
    *y2 = dragStartY;
    *z1 = dragStartZ < curGz ? dragStartZ : curGz;
    *z2 = dragStartZ > curGz ? dragStartZ : curGz;
    if (*x1 < 0) *x1 = 0; if (*x2 >= gridWidth)  *x2 = gridWidth  - 1;
    if (*z1 < 0) *z1 = 0; if (*z2 >= gridDepth)   *z2 = gridDepth  - 1;
}

// ============================================================================
// Action Handlers - Execute the actual operations
// ============================================================================

// Draw materials, keys 1-9 (selectedMaterial is 1-based): one table for the build tools, the
// option bar and the ghost preview (main.c)
static const struct { const char* label; MaterialType mat; ItemType source; const char* name; } drawMaterials[] = {
    { "Stone",        MAT_GRANITE,   ITEM_BLOCKS, "granite" },
    { "Wood",         MAT_OAK,       ITEM_LOG,    "oak log" },
    { "Dirt",         MAT_DIRT,      ITEM_DIRT,   "dirt" },
    { "Oak plank",    MAT_OAK,       ITEM_PLANKS, "oak planks" },
    { "Pine plank",   MAT_PINE,      ITEM_PLANKS, "pine planks" },
    { "Birch plank",  MAT_BIRCH,     ITEM_PLANKS, "birch planks" },
    { "Willow plank", MAT_WILLOW,    ITEM_PLANKS, "willow planks" },
    { "Sandstone",    MAT_SANDSTONE, ITEM_BLOCKS, "sandstone" },
    { "Slate",        MAT_SLATE,     ITEM_BLOCKS, "slate" },
};
#define DRAW_MATERIAL_COUNT ((int)(sizeof(drawMaterials) / sizeof(drawMaterials[0])))

void DrawMaterialFor(int selected, MaterialType* outMat, ItemType* outSource) {
    int k = (selected >= 1 && selected <= DRAW_MATERIAL_COUNT) ? selected - 1 : 0;
    *outMat = drawMaterials[k].mat;
    *outSource = drawMaterials[k].source;
}

// Map selectedMaterial to MaterialType and source ItemType
static void GetDrawMaterial(MaterialType* outMat, ItemType* outSource, const char** outName) {
    int k = (selectedMaterial >= 1 && selectedMaterial <= DRAW_MATERIAL_COUNT) ? selectedMaterial - 1 : 0;
    *outMat = drawMaterials[k].mat;
    *outSource = drawMaterials[k].source;
    *outName = drawMaterials[k].name;
}

static void ExecuteBuildWall(int x1, int y1, int x2, int y2, int z) {
    MaterialType mat; ItemType source; const char* matName;
    GetDrawMaterial(&mat, &source, &matName);
    bool isDirt = (selectedMaterial == 3);
    int count = 0;
    int skipped = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            // Don't allow walls on workshop tiles
            if (IsWorkshopTile(dx, dy, z)) {
                skipped++;
                continue;
            }
            
            if (isDirt) {
                // Dirt creates natural terrain
                if (GRID(dx, dy, z) != CELL_WALL || GetWallMaterial(dx, dy, z) != MAT_DIRT) {
                    PlaceCellFull(dx, dy, z, NaturalTerrainSpec(CELL_WALL, MAT_DIRT, SURFACE_BARE, true, true));
                    SetWallSourceItem(dx, dy, z, ITEM_DIRT);
                    InvalidatePathsThroughCell(dx, dy, z);
                    count++;
                }
            } else {
                // Normal wall with material
                if (GRID(dx, dy, z) != CELL_WALL || GetWallMaterial(dx, dy, z) != mat) {
                    CellPlacementSpec spec = {
                        .cellType = CELL_WALL,
                        .wallMat = mat,
                        .wallNatural = false,
                        .wallFinish = FINISH_SMOOTH,
                        .clearFloor = false,
                        .clearWater = true,
                        .surfaceType = SURFACE_BARE
                    };
                    PlaceCellFull(dx, dy, z, spec);
                    SetWallSourceItem(dx, dy, z, source);
                    InvalidatePathsThroughCell(dx, dy, z);
                    count++;
                }
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Placed %d %s%s", count, matName, count > 1 ? " walls" : " wall"), GREEN);
    }
    if (skipped > 0) {
        AddMessage(TextFormat("Skipped %d cell%s (workshop)", skipped, skipped > 1 ? "s" : ""), ORANGE);
    }
}

static void ExecuteBuildFloor(int x1, int y1, int x2, int y2, int z) {
    MaterialType mat; ItemType source; const char* matName;
    GetDrawMaterial(&mat, &source, &matName);
    int count = 0;
    int replaced = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (CellBlocksMovement(GRID(dx, dy, z))) {
                continue;
            }
            bool hadFloor = HAS_FLOOR(dx, dy, z);
            // Set floor flag on AIR cell (for balconies/bridges)
            if (!hadFloor) {
                GRID(dx, dy, z) = CELL_AIR;
                SET_FLOOR(dx, dy, z);
                count++;
            } else {
                replaced++;
            }
            SetFloorMaterial(dx, dy, z, mat);
            SetFloorSourceItem(dx, dy, z, source);
            ClearFloorNatural(dx, dy, z);
            SetFloorFinish(dx, dy, z, FINISH_SMOOTH);
            MarkChunkDirty(dx, dy, z);
            CLEAR_CELL_FLAG(dx, dy, z, CELL_FLAG_BURNED);
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Placed %d %s floor%s", count, matName, count > 1 ? "s" : ""), GREEN);
    }
    if (replaced > 0) {
        AddMessage(TextFormat("Updated %d %s floor%s", replaced, matName, replaced > 1 ? "s" : ""), GREEN);
    }
}

static void ExecuteBuildLadder(int x1, int y1, int x2, int y2, int z) {
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            PlaceLadder(dx, dy, z);
            CLEAR_CELL_FLAG(dx, dy, z, CELL_FLAG_BURNED);
        }
    }
}

// Selected ramp direction - can be set manually or CELL_AIR for auto-detect
// When CELL_AIR, direction is auto-detected based on terrain at placement time
CellType selectedRampDirection = CELL_AIR;  // Default to auto-detect

// Direction new conveyor belts point (BeltDir): arrows/HJKL, PageUp = lift, PageDown = chute
int selectedBeltDirection = BELT_E;
static const char* BeltDirName(int d) {
    static const char* names[] = {"North", "East", "South", "West", "Up (lift)", "Down (chute)"};
    return (d >= 0 && d <= BELT_DOWN) ? names[d] : "?";
}
const char* GetSelectedBeltDirectionName(void) { return BeltDirName(selectedBeltDirection); }

static void ExecuteBuildRamp(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    int skipped = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            CellType rampDir = selectedRampDirection;
            
            // Auto-detect direction if not manually selected
            if (rampDir == CELL_AIR) {
                rampDir = AutoDetectRampDirection(dx, dy, z);
            }
            
            if (rampDir != CELL_AIR && CanPlaceRamp(dx, dy, z, rampDir)) {
                PlaceRamp(dx, dy, z, rampDir);
                CLEAR_CELL_FLAG(dx, dy, z, CELL_FLAG_BURNED);
                count++;
            } else {
                skipped++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Placed %d ramp%s", count, count > 1 ? "s" : ""), GREEN);
    }
    if (skipped > 0) {
        AddMessage(TextFormat("Skipped %d cell%s (invalid)", skipped, skipped > 1 ? "s" : ""), ORANGE);
    }
}

static void ExecuteEraseRamp(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (CellIsDirectionalRamp(GRID(dx, dy, z))) {
                EraseRamp(dx, dy, z);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Erased %d ramp%s", count, count > 1 ? "s" : ""), ORANGE);
    }
}


static void ExecuteBuildRock(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    int skipped = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (IsWorkshopTile(dx, dy, z)) {
                skipped++;
                continue;
            }

            if (GRID(dx, dy, z) != CELL_WALL || GetWallMaterial(dx, dy, z) != MAT_GRANITE || !IsWallNatural(dx, dy, z)) {
                PlaceCellFull(dx, dy, z, NaturalTerrainSpec(CELL_WALL, MAT_GRANITE, SURFACE_BARE, true, true));
                InvalidatePathsThroughCell(dx, dy, z);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Placed %d rock%s", count, count > 1 ? " tiles" : " tile"), GREEN);
    }
    if (skipped > 0) {
        AddMessage(TextFormat("Skipped %d cell%s (workshop)", skipped, skipped > 1 ? "s" : ""), ORANGE);
    }
}

// Helper to build soil (all soil types follow same pattern)
static void ExecuteBuildSoil(int x1, int y1, int x2, int y2, int z, CellType soilType, MaterialType material, const char* name) {
    (void)soilType;  // No longer used - soil is always CELL_WALL
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            CellType cell = GRID(dx, dy, z);
            // Can place soil on air or overwrite existing terrain
            if (cell == CELL_AIR || (cell == CELL_WALL && GetWallMaterial(dx, dy, z) != material)) {
                PlaceCellFull(dx, dy, z, NaturalTerrainSpec(CELL_WALL, material, SURFACE_BARE, true, false));
                if (material == MAT_DIRT) SetVegetation(dx, dy, z, VEG_GRASS_TALL);
                InvalidatePathsThroughCell(dx, dy, z);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Placed %d %s%s", count, name, count > 1 ? " tiles" : " tile"), GREEN);
    }
}

// Pile mode: place soil with gravity and spreading
static void ExecutePileSoil(int x, int y, int z, CellType soilType, MaterialType material, const char* name) {
    (void)name;
    (void)soilType;  // No longer used - soil is always CELL_WALL
    // Bounds check
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth) return;
    
    // Try to place at target position with gravity
    int placeZ = z;
    
    // Step 1: Apply gravity - drop down until we find support
    while (placeZ > 0) {
        CellType below = GRID(x, y, placeZ - 1);
        // If there's solid support below, we can place here
        if (CellIsSolid(below)) {
            break;
        }
        // Otherwise keep falling
        placeZ--;
    }
    
    // Step 2: Try to place at the landing position
    CellType landingCell = GRID(x, y, placeZ);
    if (landingCell == CELL_AIR || CellIsDirectionalRamp(landingCell)) {
        // Air or ramp at landing spot - place/fill with soil here
        if (CellIsDirectionalRamp(landingCell)) {
            // If it was a ramp, we're filling it up - decrease ramp count
            rampCount--;
        }
        PlaceCellFull(x, y, placeZ, NaturalTerrainSpec(CELL_WALL, material, SURFACE_BARE, true, false));
        if (material == MAT_DIRT) SetVegetation(x, y, placeZ, VEG_GRASS_TALL);
        InvalidatePathsThroughCell(x, y, placeZ);
        
        // Try to create ramps at adjacent edges for organic look
        int dirOffsets[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};  // N, E, S, W
        for (int d = 0; d < 4; d++) {
            int adjX = x + dirOffsets[d][0];
            int adjY = y + dirOffsets[d][1];
            
            if (adjX < 0 || adjX >= gridWidth || adjY < 0 || adjY >= gridHeight) continue;
            
            if (GRID(adjX, adjY, placeZ) == CELL_AIR) {
                CellType rampDir = AutoDetectRampDirection(adjX, adjY, placeZ);
                if (rampDir != CELL_AIR) {
                    PlaceRamp(adjX, adjY, placeZ, rampDir);
                }
            }
        }
        
        return;
    }
    
    // Step 3: Landing spot is occupied - try to spread to adjacent cells
    // Search in expanding radius up to soilPileRadius
    int maxRadius = (int)soilPileRadius;
    for (int r = 1; r <= maxRadius; r++) {
        // Try spreading in this radius
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                // Skip cells outside circle
                if (dx * dx + dy * dy > r * r) continue;
                
                int nx = x + dx;
                int ny = y + dy;
                
                // Bounds check
                if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
                
                // Try priority: up first, then horizontal, then down
                int tryZ[] = {placeZ + 1, placeZ, placeZ - 1};
                for (int i = 0; i < 3; i++) {
                    int nz = tryZ[i];
                    if (nz < 0 || nz >= gridDepth) continue;
                    
                    // Check if this spot is valid (air or ramp can be filled)
                    CellType targetCell = GRID(nx, ny, nz);
                    if (targetCell != CELL_AIR && !CellIsDirectionalRamp(targetCell)) continue;
                    
                    // Check if there's support (for horizontal/down placement)
                    if (nz > 0 && i > 0) {  // Not placing up
                        CellType below = GRID(nx, ny, nz - 1);
                        if (!CellIsSolid(below)) continue;  // No support
                    }
                    
                    // Found valid spot! Place soil here
                    if (CellIsDirectionalRamp(targetCell)) {
                        // Filling up a ramp - decrease count
                        rampCount--;
                    }
                    PlaceCellFull(nx, ny, nz, NaturalTerrainSpec(CELL_WALL, material, SURFACE_BARE, true, false));
                    if (material == MAT_DIRT) SetVegetation(nx, ny, nz, VEG_GRASS_TALL);
                    InvalidatePathsThroughCell(nx, ny, nz);
                    
                    // Try to create organic-looking ramps at adjacent edges
                    // Check all 4 cardinal directions for potential ramp placement
                    int dirOffsets[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};  // N, E, S, W
                    for (int d = 0; d < 4; d++) {
                        int adjX = nx + dirOffsets[d][0];
                        int adjY = ny + dirOffsets[d][1];
                        
                        // Bounds check
                        if (adjX < 0 || adjX >= gridWidth || adjY < 0 || adjY >= gridHeight) continue;
                        
                        // If adjacent cell at same level is air, try to place a ramp
                        if (GRID(adjX, adjY, nz) == CELL_AIR) {
                            CellType rampDir = AutoDetectRampDirection(adjX, adjY, nz);
                            if (rampDir != CELL_AIR) {
                                PlaceRamp(adjX, adjY, nz, rampDir);
                            }
                        }
                    }
                    
                    return;  // Successfully placed
                }
            }
        }
    }
    // Could not find anywhere to place within radius
}


static void ExecuteErase(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            // Cancel any designation on this cell
            CancelDesignation(dx, dy, z);
            
            // Remove stockpile cells in this area
            for (int i = 0; i < stockpileHighWater; i++) {
                if (stockpiles[i].active) {
                    RemoveStockpileCells(i, dx, dy, dx, dy);
                }
            }
            
            if (IsLadderCell(GRID(dx, dy, z))) {
                EraseLadder(dx, dy, z);
                count++;
            } else if (CellIsDirectionalRamp(GRID(dx, dy, z))) {
                EraseRamp(dx, dy, z);
                count++;
            } else {
                CellType eraseType = CELL_AIR;
                MaterialType eraseMat = MAT_NONE;
                if (z == 0) {
                    eraseType = CELL_WALL;
                    eraseMat = MAT_BEDROCK;
                }
                bool changed = false;
                if (GRID(dx, dy, z) != eraseType || (eraseType == CELL_WALL && GetWallMaterial(dx, dy, z) != eraseMat)) {
                    GRID(dx, dy, z) = eraseType;
                    changed = true;
                }
                if (changed) {
                    SetWallMaterial(dx, dy, z, eraseMat);
                    ClearWallNatural(dx, dy, z);
                    SetWallFinish(dx, dy, z, FINISH_ROUGH);
                    SET_CELL_SURFACE(dx, dy, z, SURFACE_BARE);
                }
                // Also clear floor flag in DF mode
                if (HAS_FLOOR(dx, dy, z)) {
                    CLEAR_FLOOR(dx, dy, z);
                    SetFloorMaterial(dx, dy, z, MAT_NONE);
                    ClearFloorNatural(dx, dy, z);
                    SetFloorFinish(dx, dy, z, FINISH_ROUGH);
                    changed = true;
                }
                if (changed) {
                    MarkChunkDirty(dx, dy, z);
                    DestabilizeWater(dx, dy, z);
                    count++;
                }
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Erased %d cell%s", count, count > 1 ? "s" : ""), ORANGE);
    }
}

static void ExecuteDesignateMine(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (DesignateMine(dx, dy, z)) count++;
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Designated %d cell%s for mining", count, count > 1 ? "s" : ""), ORANGE);
    }
}

static void ExecuteCancelMine(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (HasMineDesignation(dx, dy, z)) {
                CancelDesignation(dx, dy, z);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Cancelled %d mining designation%s", count, count > 1 ? "s" : ""), ORANGE);
    }
}

static void ExecuteDesignateChannel(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (DesignateChannel(dx, dy, z)) count++;
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Designated %d cell%s for channeling", count, count > 1 ? "s" : ""), ORANGE);
    }
}

static void ExecuteCancelChannel(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (HasChannelDesignation(dx, dy, z)) {
                CancelDesignation(dx, dy, z);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Cancelled %d channel designation%s", count, count > 1 ? "s" : ""), ORANGE);
    }
}

static void ExecuteDesignateDigRamp(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (DesignateDigRamp(dx, dy, z)) count++;
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Designated %d cell%s for ramp digging", count, count > 1 ? "s" : ""), ORANGE);
    }
}

static void ExecuteCancelDigRamp(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (HasDigRampDesignation(dx, dy, z)) {
                CancelDesignation(dx, dy, z);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Cancelled %d dig ramp designation%s", count, count > 1 ? "s" : ""), ORANGE);
    }
}

static void ExecuteDesignateRemoveFloor(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (DesignateRemoveFloor(dx, dy, z)) count++;
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Designated %d floor%s for removal", count, count > 1 ? "s" : ""), ORANGE);
    }
}

static void ExecuteCancelRemoveFloor(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (HasRemoveFloorDesignation(dx, dy, z)) {
                CancelDesignation(dx, dy, z);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Cancelled %d floor removal designation%s", count, count > 1 ? "s" : ""), ORANGE);
    }
}

static void ExecuteDesignateRemoveRamp(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (DesignateRemoveRamp(dx, dy, z)) count++;
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Designated %d ramp%s for removal", count, count > 1 ? "s" : ""), ORANGE);
    }
}

// Blast mining: one gunpowder charge per designated wall; the blaster lights it and runs
static void ExecuteDesignateBlast(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++)
        for (int dx = x1; dx <= x2; dx++)
            if (DesignateBlast(dx, dy, z)) count++;
    if (count > 0) AddMessage(TextFormat("Marked %d wall%s for blasting (one gunpowder each)", count, count > 1 ? "s" : ""), ORANGE);
}

static void ExecuteCancelBlast(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++)
        for (int dx = x1; dx <= x2; dx++)
            if (HasBlastDesignation(dx, dy, z)) { CancelDesignation(dx, dy, z); count++; }
    if (count > 0) AddMessage(TextFormat("Cancelled %d blast%s", count, count > 1 ? "s" : ""), ORANGE);
}

static void ExecuteCancelRemoveRamp(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (HasRemoveRampDesignation(dx, dy, z)) {
                CancelDesignation(dx, dy, z);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Cancelled %d ramp removal designation%s", count, count > 1 ? "s" : ""), ORANGE);
    }
}

static void ExecuteDesignateChop(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (DesignateChop(dx, dy, z)) count++;
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Designated %d tree%s for chopping", count, count > 1 ? "s" : ""), ORANGE);
    }
}

static void ExecuteCancelChop(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (HasChopDesignation(dx, dy, z)) {
                CancelDesignation(dx, dy, z);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Cancelled %d chop designation%s", count, count > 1 ? "s" : ""), ORANGE);
    }
}

static void ExecuteDesignateChopFelled(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (DesignateChopFelled(dx, dy, z)) count++;
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Designated %d fallen log%s for chopping", count, count > 1 ? "s" : ""), ORANGE);
    }
}

static void ExecuteCancelChopFelled(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (HasChopFelledDesignation(dx, dy, z)) {
                CancelDesignation(dx, dy, z);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Cancelled %d chop-felled designation%s", count, count > 1 ? "s" : ""), ORANGE);
    }
}

static void ExecuteDesignateGatherSapling(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (DesignateGatherSapling(dx, dy, z)) count++;
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Designated %d sapling%s for gathering", count, count > 1 ? "s" : ""), GREEN);
    }
}

static void ExecuteCancelGatherSapling(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (HasGatherSaplingDesignation(dx, dy, z)) {
                CancelDesignation(dx, dy, z);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Cancelled %d gather sapling designation%s", count, count > 1 ? "s" : ""), GREEN);
    }
}

static void ExecuteDesignateGatherGrass(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (DesignateGatherGrass(dx, dy, z)) count++;
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Designated %d grass cell%s for gathering", count, count > 1 ? "s" : ""), GREEN);
    }
}

static void ExecuteCancelGatherGrass(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (HasGatherGrassDesignation(dx, dy, z)) {
                CancelDesignation(dx, dy, z);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Cancelled %d gather grass designation%s", count, count > 1 ? "s" : ""), GREEN);
    }
}

static void ExecuteDesignateGatherReeds(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (DesignateGatherReeds(dx, dy, z)) count++;
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Designated %d reed cell%s for gathering", count, count > 1 ? "s" : ""), GREEN);
    }
}

static void ExecuteCancelGatherReeds(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (HasGatherReedsDesignation(dx, dy, z)) {
                CancelDesignation(dx, dy, z);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Cancelled %d gather reeds designation%s", count, count > 1 ? "s" : ""), GREEN);
    }
}

static void ExecuteDesignateGatherTree(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (DesignateGatherTree(dx, dy, z)) count++;
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Designated %d tree%s for gathering", count, count > 1 ? "s" : ""), GREEN);
    }
}

static void ExecuteCancelGatherTree(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (HasGatherTreeDesignation(dx, dy, z)) {
                CancelDesignation(dx, dy, z);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Cancelled %d gather tree designation%s", count, count > 1 ? "s" : ""), GREEN);
    }
}

static void ExecuteDesignatePlantSapling(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (DesignatePlantSapling(dx, dy, z)) count++;
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Designated %d tile%s for planting", count, count > 1 ? "s" : ""), GREEN);
    }
}

static void ExecuteCancelPlantSapling(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (HasPlantSaplingDesignation(dx, dy, z)) {
                CancelDesignation(dx, dy, z);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Cancelled %d plant sapling designation%s", count, count > 1 ? "s" : ""), GREEN);
    }
}

// Doors: each door marked for the other way from how it is now (open -> shut, shut -> open)
static void ExecuteMarkDoor(int x1, int y1, int x2, int y2, int z) {
    int toClose = 0, toOpen = 0;
    for (int dy = y1; dy <= y2; dy++)
        for (int dx = x1; dx <= x2; dx++) {
            bool wasOpen = GRID(dx, dy, z) == CELL_DOOR_OPEN;
            if (DesignateDoor(dx, dy, z)) { if (wasOpen) toClose++; else toOpen++; }
        }
    if (toClose > 0) AddMessage(TextFormat("Marked %d door%s to close", toClose, toClose > 1 ? "s" : ""), SKYBLUE);
    if (toOpen > 0) AddMessage(TextFormat("Marked %d door%s to open", toOpen, toOpen > 1 ? "s" : ""), SKYBLUE);
}

static void ExecuteCancelDoorMark(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++)
        for (int dx = x1; dx <= x2; dx++)
            if (HasDoorDesignation(dx, dy, z)) { CancelDesignation(dx, dy, z); count++; }
    if (count > 0) AddMessage(TextFormat("Cancelled %d door mark%s", count, count > 1 ? "s" : ""), SKYBLUE);
}

static void ExecuteDesignateClean(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (DesignateClean(dx, dy, z)) count++;
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Designated %d floor%s for cleaning", count, count > 1 ? "s" : ""), SKYBLUE);
    }
}

static void ExecuteCancelClean(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (HasCleanDesignation(dx, dy, z)) {
                CancelDesignation(dx, dy, z);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Cancelled %d clean designation%s", count, count > 1 ? "s" : ""), SKYBLUE);
    }
}

static void ExecuteMarkHunt(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int i = 0; i < animalCount; i++) {
        Animal* a = &animals[i];
        if (!a->active || (int)a->z != z) continue;
        if (a->markedForHunt) continue;
        int ax = (int)(a->x / CELL_SIZE);
        int ay = (int)(a->y / CELL_SIZE);
        if (ax >= x1 && ax <= x2 && ay >= y1 && ay <= y2) {
            a->markedForHunt = true;
            count++;
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Marked %d animal%s for hunting", count, count > 1 ? "s" : ""), RED);
    }
}

static void ExecuteUnmarkHunt(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int i = 0; i < animalCount; i++) {
        Animal* a = &animals[i];
        if (!a->active || (int)a->z != z) continue;
        if (!a->markedForHunt) continue;
        int ax = (int)(a->x / CELL_SIZE);
        int ay = (int)(a->y / CELL_SIZE);
        if (ax >= x1 && ax <= x2 && ay >= y1 && ay <= y2) {
            a->markedForHunt = false;
            a->reservedByHunter = -1;
            count++;
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Unmarked %d animal%s", count, count > 1 ? "s" : ""), RED);
    }
}

static void ExecuteDesignateHarvestBerry(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (DesignateHarvestBerry(dx, dy, z)) count++;
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Designated %d bush%s for harvesting", count, count > 1 ? "es" : ""), GREEN);
    }
}

static void ExecuteCancelHarvestBerry(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (HasHarvestBerryDesignation(dx, dy, z)) {
                CancelDesignation(dx, dy, z);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Cancelled %d harvest designation%s", count, count > 1 ? "s" : ""), GREEN);
    }
}

static void ExecuteDesignateKnap(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (DesignateKnap(dx, dy, z)) count++;
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Designated %d stone%s for knapping", count, count > 1 ? "s" : ""), ORANGE);
    }
}

static void ExecuteCancelKnap(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            // Check both z and z-1 (designation lives on the wall, not the air above)
            if (z >= 0 && z < gridDepth && DESIGNATION(dx, dy, z).type == DESIGNATION_KNAP) {
                CancelDesignation(dx, dy, z);
                count++;
            } else if (z > 0 && DESIGNATION(dx, dy, z-1).type == DESIGNATION_KNAP) {
                CancelDesignation(dx, dy, z - 1);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Cancelled %d knap designation%s", count, count > 1 ? "s" : ""), ORANGE);
    }
}

static void ExecuteDesignateDigRoots(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (DesignateDigRoots(dx, dy, z)) count++;
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Designated %d cell%s for root digging", count, count > 1 ? "s" : ""), GREEN);
    }
}

static void ExecuteCancelDigRoots(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (HasDigRootsDesignation(dx, dy, z)) {
                CancelDesignation(dx, dy, z);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Cancelled %d dig roots designation%s", count, count > 1 ? "s" : ""), GREEN);
    }
}

static void ExecuteDesignateFarm(int x1, int y1, int x2, int y2, int z) {
    // Map selectedMaterial to CropType (1=wheat, 2=lentils, 3=flax)
    CropType cropType = CROP_WHEAT;  // default
    if (selectedMaterial == 2) cropType = CROP_LENTILS;
    else if (selectedMaterial == 3) cropType = CROP_FLAX;

    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            // For already-tilled cells: just update desired crop type
            FarmCell* fc = GetFarmCell(dx, dy, z);
            if (fc && fc->tilled && fc->cropType == CROP_NONE) {
                fc->desiredCropType = (uint8_t)cropType;
                count++;
                continue;
            }
            // For untilled cells: designate for tilling + set desired crop
            if (DesignateFarm(dx, dy, z)) {
                FarmCell* fc2 = GetFarmCell(dx, dy, z);
                if (fc2) fc2->desiredCropType = (uint8_t)cropType;
                count++;
            }
        }
    }
    static const char* cropNames[] = {"None", "Wheat", "Lentils", "Flax"};
    if (count > 0) {
        AddMessage(TextFormat("Designated %d cell%s for %s", count, count > 1 ? "s" : "", cropNames[cropType]), GREEN);
    }
}

static void ExecuteCancelFarm(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (HasFarmDesignation(dx, dy, z)) {
                CancelDesignation(dx, dy, z);
                count++;
            }
            // Already-tilled ground stays tilled — weeds will reclaim it naturally
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Cancelled %d farm designation%s", count, count > 1 ? "s" : ""), GREEN);
    }
}

static void ExecuteDesignateExplore(int x, int y, int z) {
    if (DesignateExplore(x, y, z)) {
        AddMessage(TextFormat("Designated (%d,%d,z%d) for exploration", x, y, z), (Color){180, 220, 100, 255});
    }
}

static void ExecuteCancelExplore(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (HasExploreDesignation(dx, dy, z)) {
                CancelDesignation(dx, dy, z);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Cancelled %d explore designation%s", count, count > 1 ? "s" : ""), (Color){180, 220, 100, 255});
    }
}

static void ExecuteDesignateBuild(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            int bpIdx = CreateRecipeBlueprint(dx, dy, z, selectedWallRecipe);
            if (bpIdx >= 0) {
                count++;
            }
        }
    }
    if (count > 0) {
        const ConstructionRecipe* recipe = GetConstructionRecipe(selectedWallRecipe);
        AddMessage(TextFormat("Created %d %s blueprint%s", count,
                   recipe ? recipe->name : "wall", count > 1 ? "s" : ""), BLUE);
    }
}

static void ExecuteDesignateLadder(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            int bpIdx = CreateRecipeBlueprint(dx, dy, z, selectedLadderRecipe);
            if (bpIdx >= 0) {
                count++;
            }
        }
    }
    if (count > 0) {
        const ConstructionRecipe* recipe = GetConstructionRecipe(selectedLadderRecipe);
        AddMessage(TextFormat("Created %d %s blueprint%s", count,
                   recipe ? recipe->name : "ladder", count > 1 ? "s" : ""), BLUE);
    }
}

static void ExecuteDesignateFloor(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            int bpIdx = CreateRecipeBlueprint(dx, dy, z, selectedFloorRecipe);
            if (bpIdx >= 0) {
                count++;
            }
        }
    }
    if (count > 0) {
        const ConstructionRecipe* recipe = GetConstructionRecipe(selectedFloorRecipe);
        AddMessage(TextFormat("Created %d %s blueprint%s", count,
                   recipe ? recipe->name : "floor", count > 1 ? "s" : ""), BLUE);
    }
}

static void ExecuteDesignateRamp(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            int bpIdx = CreateRecipeBlueprint(dx, dy, z, CONSTRUCTION_RAMP);
            if (bpIdx >= 0) {
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Created %d ramp blueprint%s", count, count > 1 ? "s" : ""), BLUE);
    }
}

static void ExecuteDesignateFurniture(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            int bpIdx = CreateRecipeBlueprint(dx, dy, z, selectedFurnitureRecipe);
            if (bpIdx >= 0) {
                count++;
            }
        }
    }
    if (count > 0) {
        const ConstructionRecipe* recipe = GetConstructionRecipe(selectedFurnitureRecipe);
        AddMessage(TextFormat("Created %d %s blueprint%s", count,
                   recipe ? recipe->name : "furniture", count > 1 ? "s" : ""), BLUE);
    }
}

static void ExecuteDesignateDoor(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            int bpIdx = CreateRecipeBlueprint(dx, dy, z, selectedDoorRecipe);
            if (bpIdx >= 0) {
                count++;
            }
        }
    }
    if (count > 0) {
        const ConstructionRecipe* recipe = GetConstructionRecipe(selectedDoorRecipe);
        AddMessage(TextFormat("Created %d %s blueprint%s", count,
                   recipe ? recipe->name : "door", count > 1 ? "s" : ""), BLUE);
    }
}

static void ExecuteDesignateBelt(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++)
        for (int dx = x1; dx <= x2; dx++)
            if (CreateBeltBlueprint(dx, dy, z, selectedBeltDirection) >= 0) count++;
    if (count > 0)
        AddMessage(TextFormat("Created %d belt blueprint%s (%s)", count, count > 1 ? "s" : "",
                              BeltDirName(selectedBeltDirection)), BLUE);
}

// Right-drag with the belt tool: cancel belt blueprints and take down built belts
// (what rides one drops on the ground; each belt gives back one plank)
static void ExecuteRemoveBelt(int x1, int y1, int x2, int y2, int z) {
    int cancelled = 0, removed = 0;
    for (int dy = y1; dy <= y2; dy++)
        for (int dx = x1; dx <= x2; dx++) {
            int bpIdx = GetBlueprintAt(dx, dy, z);
            if (bpIdx >= 0 && blueprints[bpIdx].recipeIndex == CONSTRUCTION_BELT) { CancelBlueprint(bpIdx); cancelled++; }
            // A built belt: a mover comes to take it down (the load drops, one plank back)
            if (DesignateRemoveBelt(dx, dy, z)) removed++;
        }
    if (cancelled + removed > 0)
        AddMessage(TextFormat("Marked %d belt%s for removal, cancelled %d blueprint%s", removed, removed == 1 ? "" : "s",
                              cancelled, cancelled == 1 ? "" : "s"), ORANGE);
}

// Drain blueprints (pipes.h): an L from the drag's first corner to its last, along x then y
static void ExecuteDesignateDrain(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int x = x1; x <= x2; x++) if (CreateRecipeBlueprint(x, y1, z, CONSTRUCTION_DRAIN) >= 0) count++;
    for (int y = y1 + 1; y <= y2; y++) if (CreateRecipeBlueprint(x2, y, z, CONSTRUCTION_DRAIN) >= 0) count++;
    if (count > 0) AddMessage(TextFormat("Created %d drain blueprint%s", count, count > 1 ? "s" : ""), BLUE);
}

static void ExecuteDesignateDrainOutlet(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int y = y1; y <= y2; y++)
        for (int x = x1; x <= x2; x++)
            if (CreateRecipeBlueprint(x, y, z, CONSTRUCTION_DRAIN_OUTLET) >= 0) count++;
    if (count > 0) AddMessage(TextFormat("Created %d outlet blueprint%s", count, count > 1 ? "s" : ""), BLUE);
}

// Right-drag with a drain tool: cancel pipe blueprints and take out built pipes (spike: at once,
// no bricks back)
static void ExecuteRemoveDrain(int x1, int y1, int x2, int y2, int z) {
    int cancelled = 0, removed = 0;
    for (int y = y1; y <= y2; y++)
        for (int x = x1; x <= x2; x++) {
            int bpIdx = GetBlueprintAt(x, y, z);
            if (bpIdx >= 0 && (blueprints[bpIdx].recipeIndex == CONSTRUCTION_DRAIN ||
                               blueprints[bpIdx].recipeIndex == CONSTRUCTION_DRAIN_OUTLET)) { CancelBlueprint(bpIdx); cancelled++; }
            if (GetPipe(x, y, z)) { SetPipe(x, y, z, 0); removed++; }
        }
    if (cancelled + removed > 0)
        AddMessage(TextFormat("Removed %d pipe%s, cancelled %d blueprint%s", removed, removed == 1 ? "" : "s",
                              cancelled, cancelled == 1 ? "" : "s"), ORANGE);
}

// Elevator shaft blueprints: on the viewed level, each one a shaft from here up one level
// (or one more level on top of a shaft). Build a taller shaft level by level.
static void ExecuteDesignateShaft(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++)
        for (int dx = x1; dx <= x2; dx++)
            if (CreateRecipeBlueprint(dx, dy, z, CONSTRUCTION_SHAFT) >= 0) count++;
    if (count > 0)
        AddMessage(TextFormat("Created %d elevator shaft blueprint%s (up to level %d)", count,
                              count > 1 ? "s" : "", z + 1), BLUE);
    else
        AddMessage("Elevator shaft: on an empty floor cell or the top of a shaft, with room above", ORANGE);
}

// Right-drag with the elevator tool: cancel shaft blueprints and take down built shafts
// (the whole column; one plank back per level)
static void ExecuteRemoveShaft(int x1, int y1, int x2, int y2, int z) {
    int cancelled = 0, levels = 0;
    for (int dy = y1; dy <= y2; dy++)
        for (int dx = x1; dx <= x2; dx++) {
            int bpIdx = GetBlueprintAt(dx, dy, z);
            if (bpIdx >= 0 && blueprints[bpIdx].recipeIndex == CONSTRUCTION_SHAFT) { CancelBlueprint(bpIdx); cancelled++; }
            int n = RemoveShaftAt(dx, dy, z);
            for (int k = 0; k < n; k++)
                SpawnItem(dx * CELL_SIZE + CELL_SIZE * 0.5f, dy * CELL_SIZE + CELL_SIZE * 0.5f, z, ITEM_PLANKS);
            levels += n;
        }
    if (cancelled + levels > 0)
        AddMessage(TextFormat("Took down %d shaft level%s, cancelled %d blueprint%s", levels, levels == 1 ? "" : "s",
                              cancelled, cancelled == 1 ? "" : "s"), ORANGE);
}

// Check if any cardinal neighbor of a workshop footprint has water
static bool HasWaterNearWorkshop(int x, int y, int z, int w, int h) {
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            int cx = x + dx;
            int cy = y + dy;
            int dirs[4][2] = {{-1,0},{1,0},{0,-1},{0,1}};
            for (int d = 0; d < 4; d++) {
                int nx = cx + dirs[d][0];
                int ny = cy + dirs[d][1];
                if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
                if (HasWater(nx, ny, z)) return true;
            }
        }
    }
    return false;
}

static void ExecutePlaceWorkshopBlueprint(int x, int y, int z, WorkshopType type) {
    int recipeIndex = GetConstructionRecipeForWorkshopType(type);
    if (recipeIndex < 0) return;

    // Water proximity check for mud mixer
    if (type == WORKSHOP_MUD_MIXER) {
        int w = workshopDefs[type].width;
        int h = workshopDefs[type].height;
        if (!HasWaterNearWorkshop(x, y, z, w, h)) {
            AddMessage("Mud Mixer must be placed adjacent to water", RED);
            return;
        }
    }

    int idx = CreateWorkshopBlueprint(x, y, z, recipeIndex);
    if (idx >= 0) {
        AddMessage(TextFormat("Placed %s blueprint", workshopDefs[type].displayName), BLUE);
    } else {
        AddMessage("Cannot place workshop here", RED);
    }
}

static void ExecuteCancelWorkshopBlueprint(int x, int y, int z) {
    // Check if clicking on any cell that's part of a workshop blueprint's footprint
    for (int i = 0; i < MAX_BLUEPRINTS; i++) {
        if (!blueprints[i].active) continue;
        if (blueprints[i].z != z) continue;
        const ConstructionRecipe* r = GetConstructionRecipe(blueprints[i].recipeIndex);
        if (!r || r->buildCategory != BUILD_WORKSHOP) continue;
        int ox = blueprints[i].workshopOriginX;
        int oy = blueprints[i].workshopOriginY;
        const WorkshopDef* def = &workshopDefs[blueprints[i].workshopType];
        if (x >= ox && x < ox + def->width && y >= oy && y < oy + def->height) {
            CancelBlueprint(i);
            AddMessage(TextFormat("Cancelled %s blueprint", def->displayName), ORANGE);
            return;
        }
    }
    // Fall back to regular single-cell cancel
    int bpIdx = GetBlueprintAt(x, y, z);
    if (bpIdx >= 0) {
        CancelBlueprint(bpIdx);
        AddMessage("Cancelled blueprint", ORANGE);
    }
}

static void ExecuteCancelBuild(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            int bpIdx = GetBlueprintAt(dx, dy, z);
            if (bpIdx >= 0) {
                CancelBlueprint(bpIdx);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Cancelled %d blueprint%s", count, count > 1 ? "s" : ""), ORANGE);
    }
}

static void ExecuteCreateStockpile(int x1, int y1, int x2, int y2, int z) {
    int width = x2 - x1 + 1;
    int height = y2 - y1 + 1;
    if (width > MAX_STOCKPILE_SIZE) width = MAX_STOCKPILE_SIZE;
    if (height > MAX_STOCKPILE_SIZE) height = MAX_STOCKPILE_SIZE;
    
    if (width > 0 && height > 0) {
        for (int i = 0; i < stockpileHighWater; i++) {
            if (!stockpiles[i].active || stockpiles[i].z != z) continue;
            RemoveStockpileCells(i, x1, y1, x2, y2);
        }
        int idx = CreateStockpile(x1, y1, z, width, height);
        if (idx >= 0) {
            AddMessage(TextFormat("Created stockpile %d (%dx%d)", idx, width, height), GREEN);
        } else {
            AddMessage(TextFormat("Failed to create stockpile (max %d)", MAX_STOCKPILES), RED);
        }
    }
}

static void ExecuteCreateRefusePile(int x1, int y1, int x2, int y2, int z) {
    int width = x2 - x1 + 1;
    int height = y2 - y1 + 1;
    if (width > MAX_STOCKPILE_SIZE) width = MAX_STOCKPILE_SIZE;
    if (height > MAX_STOCKPILE_SIZE) height = MAX_STOCKPILE_SIZE;

    if (width > 0 && height > 0) {
        for (int i = 0; i < stockpileHighWater; i++) {
            if (!stockpiles[i].active || stockpiles[i].z != z) continue;
            RemoveStockpileCells(i, x1, y1, x2, y2);
        }
        int idx = CreateStockpile(x1, y1, z, width, height);
        if (idx >= 0) {
            // Refuse pile: accepts rotten items, only spoilable food types
            stockpiles[idx].rejectsRotten = false;
            for (int t = 0; t < ITEM_TYPE_COUNT; t++) {
                stockpiles[idx].allowedTypes[t] = ItemSpoils(t);
            }
            AddMessage(TextFormat("Created refuse pile %d (%dx%d)", idx, width, height), GREEN);
        } else {
            AddMessage(TextFormat("Failed to create refuse pile (max %d)", MAX_STOCKPILES), RED);
        }
    }
}

static void ExecuteEraseStockpile(int x1, int y1, int x2, int y2, int z) {
    int erasedCells = 0;
    for (int i = MAX_STOCKPILES - 1; i >= 0; i--) {
        if (!stockpiles[i].active || stockpiles[i].z != z) continue;
        int sx1 = stockpiles[i].x;
        int sy1 = stockpiles[i].y;
        int sx2 = sx1 + stockpiles[i].width - 1;
        int sy2 = sy1 + stockpiles[i].height - 1;
        if (x1 <= sx2 && x2 >= sx1 && y1 <= sy2 && y2 >= sy1) {
            int before = GetStockpileActiveCellCount(i);
            RemoveStockpileCells(i, x1, y1, x2, y2);
            erasedCells += before - GetStockpileActiveCellCount(i);
        }
    }
    if (erasedCells > 0) {
        AddMessage(TextFormat("Erased %d stockpile cell%s", erasedCells, erasedCells > 1 ? "s" : ""), ORANGE);
    }
}

static void ExecutePlaceWorkshop(int x, int y, int z, WorkshopType type) {
    // Check if area is clear (walkable, no other workshops)
    int w = workshopDefs[type].width;
    int h = workshopDefs[type].height;
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            int cx = x + dx;
            int cy = y + dy;
            if (cx < 0 || cx >= gridWidth || cy < 0 || cy >= gridHeight) {
                AddMessage("Workshop must be within map bounds", RED);
                return;
            }
            if (!IsCellWalkableAt(z, cy, cx)) {
                AddMessage("Workshop requires walkable terrain", RED);
                return;
            }
            if (FindWorkshopAt(cx, cy, z) >= 0) {
                AddMessage("Another workshop is already here", RED);
                return;
            }
        }
    }
    
    // Water proximity check for mud mixer
    if (type == WORKSHOP_MUD_MIXER) {
        if (!HasWaterNearWorkshop(x, y, z, w, h)) {
            AddMessage("Mud Mixer must be placed adjacent to water", RED);
            return;
        }
    }

    int idx = CreateWorkshop(x, y, z, type);
    if (idx >= 0) {
        AddMessage(TextFormat("Built %s workshop #%d", workshopDefs[type].displayName, idx), GREEN);
    } else {
        AddMessage(TextFormat("Failed to create workshop (max %d)", MAX_WORKSHOPS), RED);
    }
}

static void ExecuteCreateGatherZone(int x1, int y1, int x2, int y2, int z) {
    int width = x2 - x1 + 1;
    int height = y2 - y1 + 1;
    int idx = CreateGatherZone(x1, y1, z, width, height);
    if (idx >= 0) {
        AddMessage(TextFormat("Created gather zone %dx%d", width, height), ORANGE);
    } else {
        AddMessage("Max gather zones reached!", RED);
    }
}

static void ExecuteEraseGatherZone(int x1, int y1, int x2, int y2, int z) {
    int deleted = 0;
    for (int i = MAX_GATHER_ZONES - 1; i >= 0; i--) {
        if (!gatherZones[i].active || gatherZones[i].z != z) continue;
        int gx1 = gatherZones[i].x;
        int gy1 = gatherZones[i].y;
        int gx2 = gx1 + gatherZones[i].width - 1;
        int gy2 = gy1 + gatherZones[i].height - 1;
        if (x1 <= gx2 && x2 >= gx1 && y1 <= gy2 && y2 >= gy1) {
            DeleteGatherZone(i);
            deleted++;
        }
    }
    if (deleted > 0) {
        AddMessage(TextFormat("Deleted %d gather zone%s", deleted, deleted > 1 ? "s" : ""), ORANGE);
    }
}

static void ExecutePlaceWater(int x1, int y1, int x2, int y2, int z, bool shift) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (shift) SetWaterSource(dx, dy, z, true);
            SetWaterLevel(dx, dy, z, WATER_MAX_LEVEL);
            count++;
        }
    }
    if (count > 0) {
        if (shift) {
            AddMessage(TextFormat("Placed %d water source%s", count, count > 1 ? "s" : ""), BLUE);
        } else {
            AddMessage(TextFormat("Placed water in %d cell%s", count, count > 1 ? "s" : ""), SKYBLUE);
        }
    }
}

static void ExecuteRemoveWater(int x1, int y1, int x2, int y2, int z, bool shift) {
    if (shift) {
        // Shift + right = place drain
        int count = 0;
        for (int dy = y1; dy <= y2; dy++) {
            for (int dx = x1; dx <= x2; dx++) {
                SetWaterDrain(dx, dy, z, true);
                count++;
            }
        }
        if (count > 0) AddMessage(TextFormat("Placed %d drain%s", count, count > 1 ? "s" : ""), DARKBLUE);
    } else {
        int removedSources = 0, removedDrains = 0, removedWater = 0;
        for (int dy = y1; dy <= y2; dy++) {
            for (int dx = x1; dx <= x2; dx++) {
                const WaterCell* cell = &WATER(dx, dy, z);
                if (cell->isSource) { SetWaterSource(dx, dy, z, false); removedSources++; }
                else if (cell->isDrain) { SetWaterDrain(dx, dy, z, false); removedDrains++; }
                else if (cell->level > 0) { SetWaterLevel(dx, dy, z, 0); removedWater++; }
            }
        }
        if (removedSources > 0) AddMessage(TextFormat("Removed %d source%s", removedSources, removedSources > 1 ? "s" : ""), ORANGE);
        if (removedDrains > 0) AddMessage(TextFormat("Removed %d drain%s", removedDrains, removedDrains > 1 ? "s" : ""), ORANGE);
        if (removedWater > 0) AddMessage(TextFormat("Removed water from %d cell%s", removedWater, removedWater > 1 ? "s" : ""), GRAY);
    }
}

static void ExecutePlaceFire(int x1, int y1, int x2, int y2, int z, bool shift) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            // If clicked cell has fuel (wall, trunk, ladder), ignite it directly.
            // Otherwise fall to z-1 for air above solid (surface fire).
            int fireZ = z;
            if (GetFuelAt(dx, dy, z) == 0 && z > 0 
                && GRID(dx, dy, z) == CELL_AIR && CellIsSolid(GRID(dx, dy, z-1))) {
                fireZ = z - 1;
            }
            
            if (shift) {
                SetFireSource(dx, dy, fireZ, true);
                count++;
            } else {
                if (GetFuelAt(dx, dy, fireZ) > 0 && !HAS_CELL_FLAG(dx, dy, fireZ, CELL_FLAG_BURNED)) {
                    IgniteCell(dx, dy, fireZ);
                    count++;
                }
            }
        }
    }
    if (count > 0) {
        if (shift) AddMessage(TextFormat("Placed %d fire source%s", count, count > 1 ? "s" : ""), RED);
        else AddMessage(TextFormat("Ignited %d cell%s", count, count > 1 ? "s" : ""), ORANGE);
    }
}

static void ExecuteRemoveFire(int x1, int y1, int x2, int y2, int z, bool shift) {
    int removedSources = 0, extinguished = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            // Fire burns on the floor (z-1), not in the air (z)
            int fireZ = z;
            if (z > 0 && GRID(dx, dy, z) == CELL_AIR && CellIsSolid(GRID(dx, dy, z-1))) {
                fireZ = z - 1;
            }
            
            const FireCell* cell = &FIRE(dx, dy, fireZ);
            if (cell->isSource) { SetFireSource(dx, dy, fireZ, false); removedSources++; }
            if (!shift && cell->level > 0) { ExtinguishCell(dx, dy, fireZ); extinguished++; }
        }
    }
    if (removedSources > 0) AddMessage(TextFormat("Removed %d fire source%s", removedSources, removedSources > 1 ? "s" : ""), ORANGE);
    if (extinguished > 0) AddMessage(TextFormat("Extinguished %d cell%s", extinguished, extinguished > 1 ? "s" : ""), GRAY);
}

static void ExecutePlaceHeat(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            SetHeatSource(dx, dy, z, true);
            count++;
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Placed %d heat source%s", count, count > 1 ? "s" : ""), RED);
    }
}

static void ExecuteRemoveHeat(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            TempCell* cell = &TEMPERATURE(dx, dy, z);
            if (cell->isHeatSource) {
                SetHeatSource(dx, dy, z, false);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Removed %d heat source%s", count, count > 1 ? "s" : ""), ORANGE);
    }
}

static void ExecutePlaceCold(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            SetColdSource(dx, dy, z, true);
            count++;
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Placed %d cold source%s", count, count > 1 ? "s" : ""), SKYBLUE);
    }
}

static void ExecuteRemoveCold(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            TempCell* cell = &TEMPERATURE(dx, dy, z);
            if (cell->isColdSource) {
                SetColdSource(dx, dy, z, false);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Removed %d cold source%s", count, count > 1 ? "s" : ""), ORANGE);
    }
}

static void ExecutePlaceSmoke(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            SetSmokeLevel(dx, dy, z, SMOKE_MAX_LEVEL);
            count++;
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Placed smoke in %d cell%s", count, count > 1 ? "s" : ""), GRAY);
    }
}

static void ExecuteRemoveSmoke(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (GetSmokeLevel(dx, dy, z) > 0) {
                SetSmokeLevel(dx, dy, z, 0);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Removed smoke from %d cell%s", count, count > 1 ? "s" : ""), ORANGE);
    }
}

static void ExecutePlaceSteam(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            SetSteamLevel(dx, dy, z, STEAM_MAX_LEVEL);
            SetTemperature(dx, dy, z, 100);  // Steam should be hot (100°C)
            count++;
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Placed steam in %d cell%s", count, count > 1 ? "s" : ""), WHITE);
    }
}

static void ExecuteRemoveSteam(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (GetSteamLevel(dx, dy, z) > 0) {
                SetSteamLevel(dx, dy, z, 0);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Removed steam from %d cell%s", count, count > 1 ? "s" : ""), ORANGE);
    }
}

static void ExecutePlaceGrass(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            CellType cell = GRID(dx, dy, z);
            // Can grow grass on dirt terrain or air
            if (cell == CELL_AIR) {
                // Convert to dirt terrain with proper material setup
                PlaceCellFull(dx, dy, z, NaturalTerrainSpec(CELL_WALL, MAT_DIRT, SURFACE_BARE, true, false));
                InvalidatePathsThroughCell(dx, dy, z);
            }
            if (GRID(dx, dy, z) == CELL_WALL && GetWallMaterial(dx, dy, z) == MAT_DIRT) {
                // Set tall grass overlay and reset wear
                SetVegetation(dx, dy, z, VEG_GRASS_TALL);
                WEAR(dx, dy, z) = 0;
                CLEAR_CELL_FLAG(dx, dy, z, CELL_FLAG_BURNED);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Grew grass on %d cell%s", count, count > 1 ? "s" : ""), GREEN);
    }
}

static void ExecuteRemoveGrass(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            // Remove grass overlay from dirt terrain tiles
            if (GRID(dx, dy, z) == CELL_WALL && GetWallMaterial(dx, dy, z) == MAT_DIRT) {
                int surface = GET_CELL_SURFACE(dx, dy, z);
                if (surface != SURFACE_BARE) {
                    SET_CELL_SURFACE(dx, dy, z, SURFACE_BARE);
                    WEAR(dx, dy, z) = WearCap();  // Set max wear so grass doesn't regrow immediately
                    count++;
                }
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Removed grass from %d cell%s", count, count > 1 ? "s" : ""), ORANGE);
    }
}

static void ExecutePlaceBush(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            // Place bush on walkable ground (air above solid, or has floor)
            if (IsCellWalkableAt(z, dy, dx) && GRID(dx, dy, z) == CELL_AIR) {
                GRID(dx, dy, z) = CELL_BUSH;
                int pi = SpawnPlant(dx, dy, z, PLANT_BERRY_BUSH);
                if (pi >= 0) {
                    plants[pi].stage = PLANT_STAGE_RIPE;
                }
                InvalidatePathsThroughCell(dx, dy, z);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Placed %d bush%s", count, count > 1 ? "es" : ""), GREEN);
    }
}

static void ExecuteRemoveBush(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (GRID(dx, dy, z) == CELL_BUSH) {
                GRID(dx, dy, z) = CELL_AIR;
                Plant* p = GetPlantAt(dx, dy, z);
                if (p) {
                    int pidx = (int)(p - plants);
                    DeletePlant(pidx);
                }
                InvalidatePathsThroughCell(dx, dy, z);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Removed %d bush%s", count, count > 1 ? "es" : ""), ORANGE);
    }
}

static bool TryPlaceTrackAt(int x, int y, int z) {
    if (IsCellWalkableAt(z, y, x) && GRID(x, y, z) == CELL_AIR) {
        GRID(x, y, z) = CELL_TRACK;
        TRACK_CONN(x, y, z) = 0;
        InvalidatePathsThroughCell(x, y, z);
        return true;
    }
    return false;
}

// Connect two adjacent track cells bidirectionally
static void ConnectTrack(int x1, int y1, int x2, int y2, int z) {
    int ddx = x2 - x1;
    int ddy = y2 - y1;
    // Must be cardinal neighbors
    if (ddx * ddx + ddy * ddy != 1) return;
    if (GRID(x1, y1, z) != CELL_TRACK || GRID(x2, y2, z) != CELL_TRACK) return;

    // From (x1,y1) toward (x2,y2)
    if (ddy == -1) { TRACK_CONN(x1, y1, z) |= TRACK_N; TRACK_CONN(x2, y2, z) |= TRACK_S; }
    if (ddx ==  1) { TRACK_CONN(x1, y1, z) |= TRACK_E; TRACK_CONN(x2, y2, z) |= TRACK_W; }
    if (ddy ==  1) { TRACK_CONN(x1, y1, z) |= TRACK_S; TRACK_CONN(x2, y2, z) |= TRACK_N; }
    if (ddx == -1) { TRACK_CONN(x1, y1, z) |= TRACK_W; TRACK_CONN(x2, y2, z) |= TRACK_E; }
}

static void ExecutePlaceTrack(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    int w = x2 - x1;
    int h = y2 - y1;

    // Step 1: Place all track cells on the outline
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (dy == y1 || dy == y2 || dx == x1 || dx == x2) {
                if (TryPlaceTrackAt(dx, dy, z)) count++;
            }
        }
    }

    // Step 2: Connect tracks sequentially along each edge
    // Top edge (E-W connections)
    for (int dx = x1; dx < x2; dx++)
        ConnectTrack(dx, y1, dx + 1, y1, z);
    // Bottom edge (E-W connections)
    if (h > 0)
        for (int dx = x1; dx < x2; dx++)
            ConnectTrack(dx, y2, dx + 1, y2, z);
    // Left edge (N-S connections)
    for (int dy = y1; dy < y2; dy++)
        ConnectTrack(x1, dy, x1, dy + 1, z);
    // Right edge (N-S connections)
    if (w > 0)
        for (int dy = y1; dy < y2; dy++)
            ConnectTrack(x2, dy, x2, dy + 1, z);

    if (count > 0) {
        AddMessage(TextFormat("Placed %d track%s", count, count > 1 ? "s" : ""), GREEN);
    }
}

static bool TryPlacePlatformAt(int x, int y, int z) {
    if (IsCellWalkableAt(z, y, x) && GRID(x, y, z) == CELL_AIR) {
        GRID(x, y, z) = CELL_PLATFORM;
        InvalidatePathsThroughCell(x, y, z);
        return true;
    }
    return false;
}

static void ExecutePlacePlatform(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    // Fill mode: place on ALL cells in rectangle
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (TryPlacePlatformAt(dx, dy, z)) count++;
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Placed %d platform%s", count, count > 1 ? "s" : ""), GREEN);
    }
}

static void ExecuteRemovePlatform(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (GRID(dx, dy, z) == CELL_PLATFORM) {
                GRID(dx, dy, z) = CELL_AIR;
                InvalidatePathsThroughCell(dx, dy, z);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Removed %d platform%s", count, count > 1 ? "s" : ""), YELLOW);
    }
}

static void DisconnectTrackFromNeighbors(int x, int y, int z) {
    // Remove this cell's connections from its neighbors
    uint8_t conn = TRACK_CONN(x, y, z);
    if ((conn & TRACK_N) && y > 0)              TRACK_CONN(x, y-1, z) &= ~TRACK_S;
    if ((conn & TRACK_E) && x < gridWidth - 1)  TRACK_CONN(x+1, y, z) &= ~TRACK_W;
    if ((conn & TRACK_S) && y < gridHeight - 1) TRACK_CONN(x, y+1, z) &= ~TRACK_N;
    if ((conn & TRACK_W) && x > 0)              TRACK_CONN(x-1, y, z) &= ~TRACK_E;
    TRACK_CONN(x, y, z) = 0;
}

static void ExecuteRemoveTrack(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (GRID(dx, dy, z) == CELL_TRACK) {
                DisconnectTrackFromNeighbors(dx, dy, z);
                GRID(dx, dy, z) = CELL_AIR;
                InvalidatePathsThroughCell(dx, dy, z);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Removed %d track%s", count, count > 1 ? "s" : ""), ORANGE);
    }
}

static void ExecutePlaceTree(int x, int y, int z) {
    // Check if we can place a tree here (need solid ground below or at z=0)
    CellType cell = GRID(x, y, z);
    if (cell == CELL_TREE_TRUNK || cell == CELL_TREE_LEAVES || cell == CELL_TREE_BRANCH ||
        cell == CELL_TREE_ROOT || cell == CELL_SAPLING) {
        AddMessage("Tree already here", RED);
        return;
    }
    if (cell != CELL_AIR && !CellIsSolid(cell)) {
        AddMessage("Can't place tree here", RED);
        return;
    }
    
    // Need solid ground below (or z=0)
    if (z > 0 && !CellIsSolid(GRID(x, y, z - 1))) {
        AddMessage("Trees need solid ground below", RED);
        return;
    }
    
    // Grow a full tree instantly
    TreeGrowFull(x, y, z, currentTreeType);
    AddMessage(TextFormat("Planted %s tree at (%d, %d)", TreeTypeName(currentTreeType), x, y), GREEN);
}

static void ExecuteRemoveTree(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            CellType cell = GRID(dx, dy, z);
            if (cell == CELL_TREE_TRUNK || cell == CELL_TREE_BRANCH || cell == CELL_TREE_ROOT ||
                cell == CELL_TREE_FELLED || cell == CELL_TREE_LEAVES || cell == CELL_SAPLING) {
                CancelDesignation(dx, dy, z);
                GRID(dx, dy, z) = CELL_AIR;
                SetWallMaterial(dx, dy, z, MAT_NONE);
                MarkChunkDirty(dx, dy, z);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Removed %d tree cell%s", count, count > 1 ? "s" : ""), ORANGE);
    }
}

// ============================================================================
// Terrain Sculpting
// ============================================================================

// Lower terrain: remove cell at z, place at z-1 if possible
static void LowerTerrainCell(int x, int y, int z) {
    // Only lower solid natural terrain
    if (GRID(x, y, z) != CELL_WALL) return;
    if (!IsWallNatural(x, y, z)) return;

    MaterialType mat = GetWallMaterial(x, y, z);

    // Remove at current z
    GRID(x, y, z) = CELL_AIR;
    SetWallMaterial(x, y, z, MAT_NONE);
    ClearWallNatural(x, y, z);

    // Place at z-1 if empty
    if (z > 0 && GRID(x, y, z-1) == CELL_AIR) {
        GRID(x, y, z-1) = CELL_WALL;
        SetWallMaterial(x, y, z-1, mat);
        SetWallNatural(x, y, z-1);
    }

    MarkChunkDirty(x, y, z);
    InvalidatePathsThroughCell(x, y, z);
    DestabilizeWater(x, y, z);
}

// Raise terrain: add cell at z if air and has support
static void RaiseTerrainCell(int x, int y, int z) {
    if (GRID(x, y, z) != CELL_AIR) return;
    if (z == 0) return;  // Can't raise at bedrock
    if (!CellIsSolid(GRID(x, y, z-1))) return;  // Need support

    // Inherit material from below
    MaterialType mat = GetWallMaterial(x, y, z-1);
    if (mat == MAT_NONE || mat == MAT_BEDROCK) mat = MAT_DIRT;

    GRID(x, y, z) = CELL_WALL;
    SetWallMaterial(x, y, z, mat);
    SetWallNatural(x, y, z);

    MarkChunkDirty(x, y, z);
    InvalidatePathsThroughCell(x, y, z);
    DestabilizeWater(x, y, z);
}


// Apply brush in circular pattern
static void ApplyCircularBrush(int centerX, int centerY, int z, int radius,
                               void (*operation)(int, int, int)) {
    int radiusSq = radius * radius;
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            int distSq = dx * dx + dy * dy;
            if (distSq > radiusSq) continue;  // Circle check

            int x = centerX + dx;
            int y = centerY + dy;

            // Bounds check
            if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight) continue;
            if (z < 1 || z >= gridDepth - 1) continue;  // Don't sculpt z=0 or top

            operation(x, y, z);
        }
    }
}

// Execute lower terrain at position with radius
static void ExecuteLowerTerrain(int x, int y, int z, int radius) {
    ApplyCircularBrush(x, y, z, radius, LowerTerrainCell);
}

// Execute raise terrain at position with radius
static void ExecuteRaiseTerrain(int x, int y, int z, int radius) {
    ApplyCircularBrush(x, y, z, radius, RaiseTerrainCell);
}

// Bresenham line interpolation for smooth strokes
static void InterpolateBrushStroke(int x0, int y0, int x1, int y1, int z, int radius,
                                   void (*execute)(int, int, int, int)) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    int x = x0, y = y0;
    while (true) {
        execute(x, y, z, radius);
        if (x == x1 && y == y1) break;

        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx)  { err += dx; y += sy; }
    }
}
// ============================================================================
// Tool handlers: what each action does on mouse release
// ============================================================================
//
// One row per action: apply (left release), erase (right release), a param the
// handler gets (workshop type, soil kind), and shiftDrag (runs at the mouse cell
// while shift+left dragging, e.g. piling soil). Actions without a row do nothing
// on release (categories, terrain sculpting which is a continuous brush).
// Adding a tool: write a ToolFn (or wrap an Execute* with RECT_TOOL) and add a row.

typedef struct {
    int x1, y1, x2, y2, z;   // dragged rectangle (normalized, inclusive)
    int startX, startY;      // drag start cell (single-cell tools)
    int mouseX, mouseY;      // cell under the mouse on release
    bool shift;
    int param;               // ToolHandler.param
} ToolArgs;

typedef void (*ToolFn)(const ToolArgs* a);

typedef struct {
    ToolFn apply;            // left release
    ToolFn erase;            // right release (NULL = nothing)
    int param;
    ToolFn shiftDrag;        // while shift+left dragging, at the mouse cell
} ToolHandler;

// Rectangle tools: Execute*(x1, y1, x2, y2, z [, shift])
#define RECT_TOOL(fn) static void Tool_##fn(const ToolArgs* a) { fn(a->x1, a->y1, a->x2, a->y2, a->z); }
#define RECT_TOOL_SHIFT(fn) static void Tool_##fn(const ToolArgs* a) { fn(a->x1, a->y1, a->x2, a->y2, a->z, a->shift); }

RECT_TOOL(ExecuteBuildWall)          RECT_TOOL(ExecuteErase)
RECT_TOOL(ExecuteBuildFloor)         RECT_TOOL(ExecuteBuildLadder)
RECT_TOOL(ExecuteBuildRamp)          RECT_TOOL(ExecuteEraseRamp)
RECT_TOOL(ExecuteCreateStockpile)    RECT_TOOL(ExecuteEraseStockpile)
RECT_TOOL(ExecuteCreateRefusePile)
RECT_TOOL(ExecuteDesignateMine)      RECT_TOOL(ExecuteCancelMine)
RECT_TOOL(ExecuteDesignateChannel)   RECT_TOOL(ExecuteCancelChannel)
RECT_TOOL(ExecuteDesignateDigRamp)   RECT_TOOL(ExecuteCancelDigRamp)
RECT_TOOL(ExecuteDesignateRemoveFloor) RECT_TOOL(ExecuteCancelRemoveFloor)
RECT_TOOL(ExecuteDesignateRemoveRamp)  RECT_TOOL(ExecuteCancelRemoveRamp)
RECT_TOOL(ExecuteDesignateBlast)     RECT_TOOL(ExecuteCancelBlast)
RECT_TOOL(ExecuteMarkDoor)           RECT_TOOL(ExecuteCancelDoorMark)
RECT_TOOL(ExecuteDesignateBuild)     RECT_TOOL(ExecuteCancelBuild)
RECT_TOOL(ExecuteDesignateLadder)    RECT_TOOL(ExecuteDesignateFloor)
RECT_TOOL(ExecuteDesignateRamp)      RECT_TOOL(ExecuteDesignateFurniture)
RECT_TOOL(ExecuteDesignateDoor)
RECT_TOOL(ExecuteDesignateBelt)      RECT_TOOL(ExecuteRemoveBelt)
RECT_TOOL(ExecuteDesignateDrain)     RECT_TOOL(ExecuteDesignateDrainOutlet)  RECT_TOOL(ExecuteRemoveDrain)
RECT_TOOL(ExecuteDesignateShaft)     RECT_TOOL(ExecuteRemoveShaft)
RECT_TOOL(ExecuteCreateGatherZone)   RECT_TOOL(ExecuteEraseGatherZone)
RECT_TOOL(ExecuteDesignateChop)      RECT_TOOL(ExecuteCancelChop)
RECT_TOOL(ExecuteDesignateChopFelled) RECT_TOOL(ExecuteCancelChopFelled)
RECT_TOOL(ExecuteDesignateGatherSapling) RECT_TOOL(ExecuteCancelGatherSapling)
RECT_TOOL(ExecuteDesignateGatherGrass) RECT_TOOL(ExecuteCancelGatherGrass)
RECT_TOOL(ExecuteDesignateGatherReeds) RECT_TOOL(ExecuteCancelGatherReeds)
RECT_TOOL(ExecuteDesignateGatherTree) RECT_TOOL(ExecuteCancelGatherTree)
RECT_TOOL(ExecuteDesignatePlantSapling) RECT_TOOL(ExecuteCancelPlantSapling)
RECT_TOOL(ExecuteDesignateClean)     RECT_TOOL(ExecuteCancelClean)
RECT_TOOL(ExecuteDesignateHarvestBerry) RECT_TOOL(ExecuteCancelHarvestBerry)
RECT_TOOL(ExecuteDesignateKnap)      RECT_TOOL(ExecuteCancelKnap)
RECT_TOOL(ExecuteDesignateDigRoots)  RECT_TOOL(ExecuteCancelDigRoots)
RECT_TOOL(ExecuteCancelExplore)
RECT_TOOL(ExecuteMarkHunt)           RECT_TOOL(ExecuteUnmarkHunt)
RECT_TOOL(ExecuteDesignateFarm)      RECT_TOOL(ExecuteCancelFarm)
RECT_TOOL(ExecutePlaceHeat)          RECT_TOOL(ExecuteRemoveHeat)
RECT_TOOL(ExecutePlaceCold)          RECT_TOOL(ExecuteRemoveCold)
RECT_TOOL(ExecutePlaceSmoke)         RECT_TOOL(ExecuteRemoveSmoke)
RECT_TOOL(ExecutePlaceSteam)         RECT_TOOL(ExecuteRemoveSteam)
RECT_TOOL(ExecutePlaceGrass)         RECT_TOOL(ExecuteRemoveGrass)
RECT_TOOL(ExecutePlaceBush)          RECT_TOOL(ExecuteRemoveBush)
RECT_TOOL(ExecuteRemoveTree)
RECT_TOOL_SHIFT(ExecutePlaceWater)   RECT_TOOL_SHIFT(ExecuteRemoveWater)
RECT_TOOL_SHIFT(ExecutePlaceFire)    RECT_TOOL_SHIFT(ExecuteRemoveFire)
#undef RECT_TOOL
#undef RECT_TOOL_SHIFT

// Single-cell tools
static void Tool_DesignateExplore(const ToolArgs* a) { ExecuteDesignateExplore(a->x1, a->y1, a->z); }
static void Tool_PlaceTree(const ToolArgs* a) { ExecutePlaceTree(a->x1, a->y1, a->z); }
static void Tool_SpawnMover(const ToolArgs* a) { SpawnMoverAt(a->x1, a->y1, a->z); }

static void Tool_PlaceLight(const ToolArgs* a) {
    AddLightSource(a->x1, a->y1, a->z,
                   (uint8_t)lightDefaultR, (uint8_t)lightDefaultG, (uint8_t)lightDefaultB,
                   (uint8_t)lightDefaultIntensity);
}
static void Tool_RemoveLight(const ToolArgs* a) { RemoveLightSource(a->x1, a->y1, a->z); }

// Workshops: param = WorkshopType. Draw mode places at the drag start; work mode
// places a blueprint at the rectangle corner (same cell for a click).
static void Tool_PlaceWorkshop(const ToolArgs* a) { ExecutePlaceWorkshop(a->startX, a->startY, a->z, (WorkshopType)a->param); }
static void Tool_PlaceWorkshopBlueprint(const ToolArgs* a) { ExecutePlaceWorkshopBlueprint(a->x1, a->y1, a->z, (WorkshopType)a->param); }
static void Tool_CancelWorkshopBlueprint(const ToolArgs* a) { ExecuteCancelWorkshopBlueprint(a->x1, a->y1, a->z); }

// Soils: param = index into soilTools. Left drag fills the rectangle; shift+left
// piles one block (gravity + spreading) at the drag start, and keeps piling at the
// mouse while dragging.
typedef enum { SOIL_TOOL_DIRT, SOIL_TOOL_CLAY, SOIL_TOOL_GRAVEL, SOIL_TOOL_SAND, SOIL_TOOL_PEAT, SOIL_TOOL_ROCK } SoilTool;
static const struct { MaterialType mat; const char* name; } soilTools[] = {
    [SOIL_TOOL_DIRT]   = {MAT_DIRT,    "dirt"},
    [SOIL_TOOL_CLAY]   = {MAT_CLAY,    "clay"},
    [SOIL_TOOL_GRAVEL] = {MAT_GRAVEL,  "gravel"},
    [SOIL_TOOL_SAND]   = {MAT_SAND,    "sand"},
    [SOIL_TOOL_PEAT]   = {MAT_PEAT,    "peat"},
    [SOIL_TOOL_ROCK]   = {MAT_GRANITE, "rock"},
};
static void Tool_Soil(const ToolArgs* a) {
    if (a->shift) {
        ExecutePileSoil(a->startX, a->startY, a->z, CELL_WALL, soilTools[a->param].mat, soilTools[a->param].name);
    } else if (a->param == SOIL_TOOL_ROCK) {
        ExecuteBuildRock(a->x1, a->y1, a->x2, a->y2, a->z);
    } else {
        ExecuteBuildSoil(a->x1, a->y1, a->x2, a->y2, a->z, CELL_WALL, soilTools[a->param].mat, soilTools[a->param].name);
    }
}
static void Tool_PileSoilAtMouse(const ToolArgs* a) {
    ExecutePileSoil(a->mouseX, a->mouseY, a->z, CELL_WALL, soilTools[a->param].mat, soilTools[a->param].name);
}

// Track and platforms: stations are rebuilt after any change
static void Tool_PlaceTrack(const ToolArgs* a) { ExecutePlaceTrack(a->x1, a->y1, a->x2, a->y2, a->z); RebuildStations(); }
static void Tool_RemoveTrack(const ToolArgs* a) { ExecuteRemoveTrack(a->x1, a->y1, a->x2, a->y2, a->z); RebuildStations(); }
static void Tool_PlacePlatform(const ToolArgs* a) { ExecutePlacePlatform(a->x1, a->y1, a->x2, a->y2, a->z); RebuildStations(); }
static void Tool_RemovePlatform(const ToolArgs* a) { ExecuteRemovePlatform(a->x1, a->y1, a->x2, a->y2, a->z); RebuildStations(); }

// Trains: spawn on the track cell under the mouse; right-click removes the train there
static void Tool_SpawnTrain(const ToolArgs* a) {
    if (GRID(a->mouseX, a->mouseY, a->z) != CELL_TRACK) {
        AddMessage("Must place train on track", RED);
        return;
    }
    int idx = SpawnTrain(a->mouseX, a->mouseY, a->z);
    if (idx >= 0) AddMessage(TextFormat("Spawned train #%d", idx), GREEN);
    else AddMessage("Max trains reached!", RED);
}
static void Tool_RemoveTrain(const ToolArgs* a) {
    for (int i = 0; i < MAX_TRAINS; i++) {
        if (!trains[i].active) continue;
        if (trains[i].cellX != a->mouseX || trains[i].cellY != a->mouseY || trains[i].z != a->z) continue;
        if (trains[i].lightCellX >= 0) {
            RemoveLightSource(trains[i].lightCellX, trains[i].lightCellY, trains[i].z);
        }
        DropTrainCargo(i);
        trains[i].active = false;
        trainCount--;
        AddMessage(TextFormat("Removed train #%d", i), ORANGE);
        return;
    }
}

// Town: roads and lots (town.c); buildings appear at once
static void Tool_TownRoad(const ToolArgs* a) {
    const char* why = NULL;
    if (TownAddRoad(a->x1, a->y1, a->x2, a->y2, a->z, &why) < 0) AddMessage(TextFormat("No road: %s", why), RED);
}
static void Tool_TownRemoveRoad(const ToolArgs* a) {
    int n = TownRemoveRoadsIn(a->x1, a->y1, a->x2, a->y2, a->z);
    if (n > 0) AddMessage(TextFormat("Removed %d road%s", n, n > 1 ? "s" : ""), ORANGE);
}
static void Tool_TownLot(const ToolArgs* a) {
    const char* why = NULL;
    LotKind kind = (LotKind)a->param;
    int lot = TownAddLot(kind, a->x1, a->y1, a->x2, a->y2, a->z, &why);
    if (lot < 0) AddMessage(TextFormat("No %s: %s", TownLotKindName(kind), why), RED);
}
static void Tool_TownRemoveLot(const ToolArgs* a) {
    int n = TownRemoveLotsIn(a->x1, a->y1, a->x2, a->y2, a->z);
    if (n > 0) AddMessage(TextFormat("Removed %d lot%s", n, n > 1 ? "s" : ""), ORANGE);
}

#define WS_DRAW(w)  {Tool_PlaceWorkshop, NULL, w, NULL}
#define WS_WORK(w)  {Tool_PlaceWorkshopBlueprint, Tool_CancelWorkshopBlueprint, w, NULL}
#define SOIL(s)     {Tool_Soil, NULL, s, Tool_PileSoilAtMouse}
#define PAIR(a, b)  {Tool_##a, Tool_##b, 0, NULL}

static const ToolHandler toolHandlers[] = {
    // Draw: direct level editing
    [ACTION_DRAW_WALL]          = PAIR(ExecuteBuildWall, ExecuteErase),
    [ACTION_DRAW_FLOOR]         = PAIR(ExecuteBuildFloor, ExecuteErase),
    [ACTION_DRAW_LADDER]        = PAIR(ExecuteBuildLadder, ExecuteErase),
    [ACTION_DRAW_RAMP]          = PAIR(ExecuteBuildRamp, ExecuteEraseRamp),
    [ACTION_DRAW_STOCKPILE]     = PAIR(ExecuteCreateStockpile, ExecuteEraseStockpile),
    [ACTION_DRAW_REFUSE_PILE]   = PAIR(ExecuteCreateRefusePile, ExecuteEraseStockpile),
    [ACTION_DRAW_WORKSHOP_STONECUTTER]  = WS_DRAW(WORKSHOP_STONECUTTER),
    [ACTION_DRAW_WORKSHOP_SAWMILL]      = WS_DRAW(WORKSHOP_SAWMILL),
    [ACTION_DRAW_WORKSHOP_KILN]         = WS_DRAW(WORKSHOP_KILN),
    [ACTION_DRAW_WORKSHOP_CHARCOAL_PIT] = WS_DRAW(WORKSHOP_CHARCOAL_PIT),
    [ACTION_DRAW_WORKSHOP_HEARTH]       = WS_DRAW(WORKSHOP_HEARTH),
    [ACTION_DRAW_WORKSHOP_DRYING_RACK]  = WS_DRAW(WORKSHOP_DRYING_RACK),
    [ACTION_DRAW_WORKSHOP_ROPE_MAKER]   = WS_DRAW(WORKSHOP_ROPE_MAKER),
    [ACTION_DRAW_WORKSHOP_CARPENTER]    = WS_DRAW(WORKSHOP_CARPENTER),
    [ACTION_DRAW_WORKSHOP_CAMPFIRE]     = WS_DRAW(WORKSHOP_CAMPFIRE),
    [ACTION_DRAW_WORKSHOP_GROUND_FIRE]  = WS_DRAW(WORKSHOP_GROUND_FIRE),
    [ACTION_DRAW_WORKSHOP_BUTCHER]      = WS_DRAW(WORKSHOP_BUTCHER),
    [ACTION_DRAW_WORKSHOP_COMPOST_PILE] = WS_DRAW(WORKSHOP_COMPOST_PILE),
    [ACTION_DRAW_WORKSHOP_QUERN]        = WS_DRAW(WORKSHOP_QUERN),
    [ACTION_DRAW_WORKSHOP_LOOM]         = WS_DRAW(WORKSHOP_LOOM),
    [ACTION_DRAW_WORKSHOP_TANNING_RACK] = WS_DRAW(WORKSHOP_TANNING_RACK),
    [ACTION_DRAW_WORKSHOP_TAILOR]       = WS_DRAW(WORKSHOP_TAILOR),
    [ACTION_DRAW_WORKSHOP_MUD_MIXER]    = WS_DRAW(WORKSHOP_MUD_MIXER),
    [ACTION_DRAW_WORKSHOP_STOVE]        = WS_DRAW(WORKSHOP_STOVE),
    [ACTION_DRAW_WORKSHOP_COUNTER]      = WS_DRAW(WORKSHOP_COUNTER),
    [ACTION_DRAW_SOIL_DIRT]     = SOIL(SOIL_TOOL_DIRT),
    [ACTION_DRAW_SOIL_CLAY]     = SOIL(SOIL_TOOL_CLAY),
    [ACTION_DRAW_SOIL_GRAVEL]   = SOIL(SOIL_TOOL_GRAVEL),
    [ACTION_DRAW_SOIL_SAND]     = SOIL(SOIL_TOOL_SAND),
    [ACTION_DRAW_SOIL_PEAT]     = SOIL(SOIL_TOOL_PEAT),
    [ACTION_DRAW_SOIL_ROCK]     = SOIL(SOIL_TOOL_ROCK),
    [ACTION_DRAW_TRACK]         = PAIR(PlaceTrack, RemoveTrack),
    [ACTION_DRAW_PLATFORM]      = PAIR(PlacePlatform, RemovePlatform),
    [ACTION_DRAW_TRAIN]         = PAIR(SpawnTrain, RemoveTrain),
    [ACTION_DRAW_ROAD]          = PAIR(TownRoad, TownRemoveRoad),
    [ACTION_DRAW_LOT_HOME]      = {Tool_TownLot, Tool_TownRemoveLot, LOT_HOME, NULL},
    [ACTION_DRAW_LOT_WORK]      = {Tool_TownLot, Tool_TownRemoveLot, LOT_WORK, NULL},
    [ACTION_DRAW_LOT_MILL]      = {Tool_TownLot, Tool_TownRemoveLot, LOT_MILL, NULL},
    [ACTION_DRAW_LOT_FARM]      = {Tool_TownLot, Tool_TownRemoveLot, LOT_FARM, NULL},
    [ACTION_DRAW_LOT_GRANARY]   = {Tool_TownLot, Tool_TownRemoveLot, LOT_GRANARY, NULL},

    // Work: designations and blueprints the movers carry out
    [ACTION_WORK_MINE]          = PAIR(ExecuteDesignateMine, ExecuteCancelMine),
    [ACTION_WORK_CHANNEL]       = PAIR(ExecuteDesignateChannel, ExecuteCancelChannel),
    [ACTION_WORK_DIG_RAMP]      = PAIR(ExecuteDesignateDigRamp, ExecuteCancelDigRamp),
    [ACTION_WORK_REMOVE_FLOOR]  = PAIR(ExecuteDesignateRemoveFloor, ExecuteCancelRemoveFloor),
    [ACTION_WORK_REMOVE_RAMP]   = PAIR(ExecuteDesignateRemoveRamp, ExecuteCancelRemoveRamp),
    [ACTION_WORK_BLAST]         = PAIR(ExecuteDesignateBlast, ExecuteCancelBlast),
    [ACTION_WORK_CONSTRUCT]     = PAIR(ExecuteDesignateBuild, ExecuteCancelBuild),
    [ACTION_WORK_LADDER]        = PAIR(ExecuteDesignateLadder, ExecuteCancelBuild),
    [ACTION_WORK_FLOOR]         = PAIR(ExecuteDesignateFloor, ExecuteCancelBuild),
    [ACTION_WORK_RAMP]          = PAIR(ExecuteDesignateRamp, ExecuteCancelBuild),
    [ACTION_WORK_FURNITURE]     = PAIR(ExecuteDesignateFurniture, ExecuteCancelBuild),
    [ACTION_WORK_DOOR]          = PAIR(ExecuteDesignateDoor, ExecuteCancelBuild),
    [ACTION_WORK_BELT]          = PAIR(ExecuteDesignateBelt, ExecuteRemoveBelt),
    [ACTION_WORK_DRAIN]         = PAIR(ExecuteDesignateDrain, ExecuteRemoveDrain),
    [ACTION_WORK_DRAIN_OUTLET]  = PAIR(ExecuteDesignateDrainOutlet, ExecuteRemoveDrain),
    [ACTION_WORK_SHAFT]         = PAIR(ExecuteDesignateShaft, ExecuteRemoveShaft),
    [ACTION_WORK_WORKSHOP_CAMPFIRE]     = WS_WORK(WORKSHOP_CAMPFIRE),
    [ACTION_WORK_WORKSHOP_GROUND_FIRE]  = WS_WORK(WORKSHOP_GROUND_FIRE),
    [ACTION_WORK_WORKSHOP_BUTCHER]      = WS_WORK(WORKSHOP_BUTCHER),
    [ACTION_WORK_WORKSHOP_COMPOST_PILE] = WS_WORK(WORKSHOP_COMPOST_PILE),
    [ACTION_WORK_WORKSHOP_QUERN]        = WS_WORK(WORKSHOP_QUERN),
    [ACTION_WORK_WORKSHOP_LOOM]         = WS_WORK(WORKSHOP_LOOM),
    [ACTION_WORK_WORKSHOP_TANNING_RACK] = WS_WORK(WORKSHOP_TANNING_RACK),
    [ACTION_WORK_WORKSHOP_TAILOR]       = WS_WORK(WORKSHOP_TAILOR),
    [ACTION_WORK_WORKSHOP_MUD_MIXER]    = WS_WORK(WORKSHOP_MUD_MIXER),
    [ACTION_WORK_WORKSHOP_STOVE]        = WS_WORK(WORKSHOP_STOVE),
    [ACTION_WORK_WORKSHOP_COUNTER]      = WS_WORK(WORKSHOP_COUNTER),
    [ACTION_WORK_WORKSHOP_DRYING_RACK]  = WS_WORK(WORKSHOP_DRYING_RACK),
    [ACTION_WORK_WORKSHOP_ROPE_MAKER]   = WS_WORK(WORKSHOP_ROPE_MAKER),
    [ACTION_WORK_WORKSHOP_CHARCOAL_PIT] = WS_WORK(WORKSHOP_CHARCOAL_PIT),
    [ACTION_WORK_WORKSHOP_HEARTH]       = WS_WORK(WORKSHOP_HEARTH),
    [ACTION_WORK_WORKSHOP_STONECUTTER]  = WS_WORK(WORKSHOP_STONECUTTER),
    [ACTION_WORK_WORKSHOP_SAWMILL]      = WS_WORK(WORKSHOP_SAWMILL),
    [ACTION_WORK_WORKSHOP_KILN]         = WS_WORK(WORKSHOP_KILN),
    [ACTION_WORK_WORKSHOP_CARPENTER]    = WS_WORK(WORKSHOP_CARPENTER),
    [ACTION_WORK_GATHER]        = PAIR(ExecuteCreateGatherZone, ExecuteEraseGatherZone),
    [ACTION_WORK_CHOP]          = PAIR(ExecuteDesignateChop, ExecuteCancelChop),
    [ACTION_WORK_CHOP_FELLED]   = PAIR(ExecuteDesignateChopFelled, ExecuteCancelChopFelled),
    [ACTION_WORK_GATHER_SAPLING] = PAIR(ExecuteDesignateGatherSapling, ExecuteCancelGatherSapling),
    [ACTION_WORK_GATHER_GRASS]  = PAIR(ExecuteDesignateGatherGrass, ExecuteCancelGatherGrass),
    [ACTION_WORK_GATHER_REEDS]  = PAIR(ExecuteDesignateGatherReeds, ExecuteCancelGatherReeds),
    [ACTION_WORK_GATHER_TREE]   = PAIR(ExecuteDesignateGatherTree, ExecuteCancelGatherTree),
    [ACTION_WORK_PLANT_SAPLING] = PAIR(ExecuteDesignatePlantSapling, ExecuteCancelPlantSapling),
    [ACTION_WORK_CLEAN]         = PAIR(ExecuteDesignateClean, ExecuteCancelClean),
    [ACTION_WORK_OPERATE_DOOR]  = PAIR(ExecuteMarkDoor, ExecuteCancelDoorMark),
    [ACTION_WORK_HARVEST_BERRY] = PAIR(ExecuteDesignateHarvestBerry, ExecuteCancelHarvestBerry),
    [ACTION_WORK_KNAP]          = PAIR(ExecuteDesignateKnap, ExecuteCancelKnap),
    [ACTION_WORK_DIG_ROOTS]     = PAIR(ExecuteDesignateDigRoots, ExecuteCancelDigRoots),
    [ACTION_WORK_EXPLORE]       = PAIR(DesignateExplore, ExecuteCancelExplore),
    [ACTION_WORK_HUNT]          = PAIR(ExecuteMarkHunt, ExecuteUnmarkHunt),
    [ACTION_WORK_FARM]          = PAIR(ExecuteDesignateFarm, ExecuteCancelFarm),

    // Sandbox: simulation brushes and spawners
    [ACTION_SANDBOX_WATER]      = PAIR(ExecutePlaceWater, ExecuteRemoveWater),
    [ACTION_SANDBOX_FIRE]       = PAIR(ExecutePlaceFire, ExecuteRemoveFire),
    [ACTION_SANDBOX_HEAT]       = PAIR(ExecutePlaceHeat, ExecuteRemoveHeat),
    [ACTION_SANDBOX_COLD]       = PAIR(ExecutePlaceCold, ExecuteRemoveCold),
    [ACTION_SANDBOX_SMOKE]      = PAIR(ExecutePlaceSmoke, ExecuteRemoveSmoke),
    [ACTION_SANDBOX_STEAM]      = PAIR(ExecutePlaceSteam, ExecuteRemoveSteam),
    [ACTION_SANDBOX_GRASS]      = PAIR(ExecutePlaceGrass, ExecuteRemoveGrass),
    [ACTION_SANDBOX_TREE]       = PAIR(PlaceTree, ExecuteRemoveTree),
    [ACTION_SANDBOX_LIGHT]      = PAIR(PlaceLight, RemoveLight),
    [ACTION_SANDBOX_BUSH]       = PAIR(ExecutePlaceBush, ExecuteRemoveBush),
    [ACTION_SANDBOX_MOVER]      = {Tool_SpawnMover, NULL, 0, NULL},
    // ACTION_SANDBOX_SCULPT/LOWER/RAISE: continuous brush stroke, handled while dragging
};
#undef WS_DRAW
#undef WS_WORK
#undef SOIL
#undef PAIR

static const ToolHandler* GetToolHandler(InputAction action) {
    if (action <= ACTION_NONE || (int)action >= (int)(sizeof(toolHandlers) / sizeof(toolHandlers[0]))) return NULL;
    const ToolHandler* h = &toolHandlers[action];
    return (h->apply || h->erase || h->shiftDrag) ? h : NULL;
}

// ============================================================================
// Tool options (action_registry.h ActionOption): one spec per tool makes both the option bar
// (input_mode.c) and the keys that change it (HandleToolOptionKeys below)
// ============================================================================

// Draw material, keys 1-9
static int OptMaterialCount(int ctx) { (void)ctx; return DRAW_MATERIAL_COUNT; }
static const char* OptMaterialName(int ctx, int i) { (void)ctx; return drawMaterials[i].label; }
static int OptMaterialGet(int ctx) { (void)ctx; return selectedMaterial - 1; }
static void OptMaterialSet(int ctx, int i) { (void)ctx; selectedMaterial = i + 1; }
static const ActionOption optMaterial = { .label = "Material", .style = OPT_NUMBERED,
    .count = OptMaterialCount, .nameAt = OptMaterialName, .get = OptMaterialGet, .set = OptMaterialSet };

// Farm crop, keys 1-3 (selectedMaterial: 1 wheat, 2 lentils, 3 flax)
static const char* const cropNames[] = { "Wheat", "Lentils", "Flax" };
static int OptCropCount(int ctx) { (void)ctx; return 3; }
static const char* OptCropName(int ctx, int i) { (void)ctx; return cropNames[i]; }
static const ActionOption optCrop = { .label = "Crop", .style = OPT_NUMBERED,
    .count = OptCropCount, .nameAt = OptCropName, .get = OptMaterialGet, .set = OptMaterialSet };

// Sculpt brush size, keys 1-4
static const char* const brushNames[] = { "1x1", "3x3", "5x5", "7x7" };
static int OptBrushCount(int ctx) { (void)ctx; return 4; }
static const char* OptBrushName(int ctx, int i) { (void)ctx; return brushNames[i]; }
static int OptBrushGet(int ctx) { (void)ctx; return terrainBrushRadius; }
static void OptBrushSet(int ctx, int i) { (void)ctx; terrainBrushRadius = i; AddMessage(TextFormat("Brush: %s", brushNames[i]), GREEN); }
static const ActionOption optBrush = { .label = "Brush", .style = OPT_NUMBERED,
    .count = OptBrushCount, .nameAt = OptBrushName, .get = OptBrushGet, .set = OptBrushSet };

// Torch colour, keys 1-5 (TorchPreset order)
static const char* const torchNames[] = { "Warm", "Cool", "Fire", "Green", "White" };
static int OptTorchCount(int ctx) { (void)ctx; return 5; }
static const char* OptTorchName(int ctx, int i) { (void)ctx; return torchNames[i]; }
static int OptTorchGet(int ctx) { (void)ctx; return currentTorchPreset; }
static void OptTorchSet(int ctx, int i) {
    (void)ctx;
    static const char* const said[] = { "Torch: Warm (orange)", "Torch: Cool (blue)", "Torch: Fire (red)", "Torch: Green", "Torch: White" };
    const Color colors[] = { ORANGE, BLUE, RED, GREEN, WHITE };
    SetTorchPreset((TorchPreset)i);
    AddMessage(said[i], colors[i]);
}
static const ActionOption optTorch = { .label = "Torch", .style = OPT_NUMBERED,
    .count = OptTorchCount, .nameAt = OptTorchName, .get = OptTorchGet, .set = OptTorchSet };

// Ramp direction: A auto, arrows / HJKL a fixed direction
static const char* const rampNames[] = { "Auto", "N", "E", "S", "W" };
static const CellType rampDirs[] = { CELL_AIR, CELL_RAMP_N, CELL_RAMP_E, CELL_RAMP_S, CELL_RAMP_W };
static int OptRampCount(int ctx) { (void)ctx; return 5; }
static const char* OptRampName(int ctx, int i) { (void)ctx; return rampNames[i]; }
static int OptRampGet(int ctx) { (void)ctx; for (int i = 0; i < 5; i++) if (rampDirs[i] == selectedRampDirection) return i; return 0; }
static void OptRampSet(int ctx, int i) {
    (void)ctx;
    static const char* const said[] = { "Ramp direction: Auto-detect", "Ramp direction: North (manual)",
        "Ramp direction: East (manual)", "Ramp direction: South (manual)", "Ramp direction: West (manual)" };
    selectedRampDirection = rampDirs[i];
    AddMessage(said[i], WHITE);
}
static const ActionOption optRamp = { .label = "Dir", .style = OPT_KEYED,
    .count = OptRampCount, .nameAt = OptRampName, .get = OptRampGet, .set = OptRampSet,
    .keys = { KEY_A, KEY_UP, KEY_RIGHT, KEY_DOWN, KEY_LEFT }, .altKeys = { 0, KEY_K, KEY_L, KEY_J, KEY_H } };

// Belt direction: arrows / HJKL, PageUp lift, PageDown chute
static const int beltDirs[] = { BELT_N, BELT_E, BELT_S, BELT_W, BELT_UP, BELT_DOWN };
static int OptBeltCount(int ctx) { (void)ctx; return 6; }
static const char* OptBeltName(int ctx, int i) { (void)ctx; return BeltDirName(beltDirs[i]); }
static int OptBeltGet(int ctx) { (void)ctx; for (int i = 0; i < 6; i++) if (beltDirs[i] == selectedBeltDirection) return i; return 0; }
static void OptBeltSet(int ctx, int i) {
    (void)ctx;
    selectedBeltDirection = beltDirs[i];
    AddMessage(TextFormat("Belt direction: %s", BeltDirName(beltDirs[i])), WHITE);
}
static const ActionOption optBelt = { .label = "Dir", .style = OPT_KEYED,
    .count = OptBeltCount, .nameAt = OptBeltName, .get = OptBeltGet, .set = OptBeltSet,
    .keys = { KEY_UP, KEY_RIGHT, KEY_DOWN, KEY_LEFT, KEY_PAGE_UP, KEY_PAGE_DOWN },
    .altKeys = { KEY_K, KEY_L, KEY_J, KEY_H, 0, 0 },
    .keysHint = "Arrows / PgUp lift / PgDn chute" };

// Build recipe per category, R cycles (ctx = BuildCategory)
static int* OptRecipeVar(int category) {
    switch (category) {
        case BUILD_WALL: return &selectedWallRecipe;
        case BUILD_FLOOR: return &selectedFloorRecipe;
        case BUILD_LADDER: return &selectedLadderRecipe;
        case BUILD_FURNITURE: return &selectedFurnitureRecipe;
        case BUILD_DOOR: return &selectedDoorRecipe;
        default: return NULL;
    }
}
void SelectBuildRecipe(int recipeIndex) {
    const ConstructionRecipe* r = GetConstructionRecipe(recipeIndex);
    int* var = r ? OptRecipeVar(r->buildCategory) : NULL;
    if (var) *var = recipeIndex;
}
bool IsBuildRecipeSelected(int recipeIndex) {
    const ConstructionRecipe* r = GetConstructionRecipe(recipeIndex);
    int* var = r ? OptRecipeVar(r->buildCategory) : NULL;
    return var && *var == recipeIndex;
}
static const char* OptRecipeCategoryLabel(int category) {
    switch (category) {
        case BUILD_WALL: return "Wall";
        case BUILD_FLOOR: return "Floor";
        case BUILD_LADDER: return "Ladder";
        case BUILD_FURNITURE: return "Furniture";
        case BUILD_DOOR: return "Door";
        default: return "?";
    }
}
static int OptRecipeCount(int category) { int idx[16]; return GetConstructionRecipeIndicesForCategory((BuildCategory)category, idx, 16); }
static const char* OptRecipeName(int category, int i) {
    int idx[16];
    int n = GetConstructionRecipeIndicesForCategory((BuildCategory)category, idx, 16);
    const ConstructionRecipe* r = (i >= 0 && i < n) ? GetConstructionRecipe(idx[i]) : NULL;
    return r ? r->name : "?";
}
static int OptRecipeGet(int category) {
    int idx[16];
    int n = GetConstructionRecipeIndicesForCategory((BuildCategory)category, idx, 16);
    int* sel = OptRecipeVar(category);
    for (int i = 0; i < n && sel; i++) if (idx[i] == *sel) return i;
    return -1;
}
static void OptRecipeSet(int category, int i) {
    int idx[16];
    int n = GetConstructionRecipeIndicesForCategory((BuildCategory)category, idx, 16);
    int* sel = OptRecipeVar(category);
    if (!sel || i < 0 || i >= n) return;
    *sel = idx[i];
    const ConstructionRecipe* recipe = GetConstructionRecipe(*sel);
    AddMessage(TextFormat("%s recipe: %s", OptRecipeCategoryLabel(category), recipe ? recipe->name : "?"), BLUE);
}
#define RECIPE_OPTION(cat) { .label = "Recipe", .style = OPT_CYCLE, .ctx = (cat), \
    .count = OptRecipeCount, .nameAt = OptRecipeName, .get = OptRecipeGet, .set = OptRecipeSet, .cycleKey = KEY_R }
static const ActionOption optRecipeWall = RECIPE_OPTION(BUILD_WALL);
static const ActionOption optRecipeFloor = RECIPE_OPTION(BUILD_FLOOR);
static const ActionOption optRecipeLadder = RECIPE_OPTION(BUILD_LADDER);
static const ActionOption optRecipeFurniture = RECIPE_OPTION(BUILD_FURNITURE);
static const ActionOption optRecipeDoor = RECIPE_OPTION(BUILD_DOOR);

// Sandbox tree species, T cycles
static const MaterialType treeTypes[] = { MAT_OAK, MAT_PINE, MAT_BIRCH, MAT_WILLOW };
static int OptTreeCount(int ctx) { (void)ctx; return 4; }
static const char* OptTreeName(int ctx, int i) { (void)ctx; return TreeTypeName(treeTypes[i]); }
static int OptTreeGet(int ctx) { (void)ctx; for (int i = 0; i < 4; i++) if (treeTypes[i] == currentTreeType) return i; return 0; }
static void OptTreeSet(int ctx, int i) {
    (void)ctx;
    currentTreeType = treeTypes[i];
    AddMessage(TextFormat("Tree type: %s", TreeTypeName(currentTreeType)), GREEN);
}
static const ActionOption optTree = { .label = "Tree", .style = OPT_CYCLE,
    .count = OptTreeCount, .nameAt = OptTreeName, .get = OptTreeGet, .set = OptTreeSet, .cycleKey = KEY_T };

static const ActionOption* const actionOptions[] = {
    [ACTION_DRAW_WALL]       = &optMaterial,
    [ACTION_DRAW_FLOOR]      = &optMaterial,
    [ACTION_DRAW_RAMP]       = &optRamp,
    [ACTION_WORK_CONSTRUCT]  = &optRecipeWall,
    [ACTION_WORK_FLOOR]      = &optRecipeFloor,
    [ACTION_WORK_LADDER]     = &optRecipeLadder,
    [ACTION_WORK_FURNITURE]  = &optRecipeFurniture,
    [ACTION_WORK_DOOR]       = &optRecipeDoor,
    [ACTION_WORK_BELT]       = &optBelt,
    [ACTION_WORK_FARM]       = &optCrop,
    [ACTION_SANDBOX_SCULPT]  = &optBrush,
    [ACTION_SANDBOX_LIGHT]   = &optTorch,
    [ACTION_SANDBOX_TREE]    = &optTree,
};

const ActionOption* GetActionOption(InputAction action) {
    if (action <= ACTION_NONE || (int)action >= (int)(sizeof(actionOptions) / sizeof(actionOptions[0]))) return NULL;
    return actionOptions[action];
}

static bool CheckKey(int key);   // below: a key press, or the bar's pending key

// The current tool's option keys; true = a key was used
static bool HandleToolOptionKeys(void) {
    const ActionOption* o = GetActionOption(inputAction);
    if (!o) return false;
    int n = o->count(o->ctx);
    switch (o->style) {
        case OPT_NUMBERED:
            for (int i = 0; i < n && i < 9; i++)
                if (CheckKey(KEY_ONE + i)) { o->set(o->ctx, i); return true; }
            break;
        case OPT_CYCLE:
            if (n > 0 && CheckKey(o->cycleKey)) { int cur = o->get(o->ctx); o->set(o->ctx, (cur + 1 + n) % n); return true; }
            break;
        case OPT_KEYED:
            for (int i = 0; i < n && i < ACTION_OPTION_MAX_KEYED; i++)
                if ((o->keys[i] && CheckKey(o->keys[i])) || (o->altKeys[i] && CheckKey(o->altKeys[i]))) {
                    o->set(o->ctx, i);
                    return true;
                }
            break;
    }
    return false;
}


// ============================================================================
// Main Input Handler
// ============================================================================

// Helper to check key press OR pending key from mouse click
static int currentPendingKey = 0;
static bool CheckKey(int key) {
    if (IsKeyPressed(key)) return true;
    if (currentPendingKey == key) {
        currentPendingKey = 0;  // Consume it
        return true;
    }
    return false;
}

void HandleInput(void) {
    // Check for pending key from UI button clicks
    int pending = InputMode_GetPendingKey();
    if (pending != 0) currentPendingKey = pending;
    
    int gx, gy, gz;  // grid coords for hover detection (gy=worldY, gz=worldZ)
    if (frontViewMode) {
        FrontViewPickCell(GetMousePosition(), &gx, &gy, &gz);
    } else {
        Vector2 mouseGrid2 = ScreenToGrid(GetMousePosition());
        gx = (int)mouseGrid2.x;
        gy = (int)mouseGrid2.y;
        gz = currentViewZ;
    }
    // Keep these available for downstream code (quickEdit, trains, etc.)
    Vector2 mouseGrid = ScreenToGrid(GetMousePosition());
    int z = frontViewMode ? gz : currentViewZ;

    // Update hover states
    int prevHoveredStockpile = hoveredStockpile;
    hoveredStockpile = GetStockpileAtGrid(gx, gy, gz);
    if (hoveredStockpile != prevHoveredStockpile) activeFilterCategory = -1;
    hoveredWorkshop = FindWorkshopAt(gx, gy, gz);
    if (paused) {
        if (frontViewMode) {
            // Screen-space hit testing — stable regardless of frontViewY changes.
            int depthLayers = frontViewDepth;
            if (depthLayers < 1) depthLayers = 1;
            int startY = frontViewY - depthLayers + 1;
            if (startY < 0) startY = 0;
            Vector2 mp = GetMousePosition();
            hoveredMover = GetMoverAtFrontView(mp.x, mp.y, startY, frontViewY);
            hoveredAnimal = GetAnimalAtFrontView(mp.x, mp.y, startY, frontViewY);
        } else {
            Vector2 mouseWorld = ScreenToWorld(GetMousePosition());
            hoveredMover = GetMoverAtWorldPos(mouseWorld.x, mouseWorld.y, gz);
            hoveredAnimal = GetAnimalAtWorldPos(mouseWorld.x, mouseWorld.y, gz);
        }
        hoveredItemCount = GetItemsAtCell(gx, gy, gz, hoveredItemCell, 16);
        // Check for designation hover (any type, not just mine)
        Designation* d = GetDesignation(gx, gy, gz);
        if (d && d->type != DESIGNATION_NONE) {
            hoveredDesignationX = gx;
            hoveredDesignationY = gy;
            hoveredDesignationZ = gz;
        } else {
            hoveredDesignationX = -1;
            hoveredDesignationY = -1;
            hoveredDesignationZ = -1;
        }
    } else {
        hoveredMover = -1;
        hoveredAnimal = -1;
        hoveredItemCount = 0;
        hoveredDesignationX = -1;
        hoveredDesignationY = -1;
        hoveredDesignationZ = -1;
    }

    // ========================================================================
    // Pie menu (Tab to toggle, or right-click tap)
    // ========================================================================
#if 0
    if (IsKeyPressed(KEY_TAB)) {
        if (PieMenu_IsOpen()) {
            PieMenu_Close();
        } else {
            Vector2 mouse = GetMousePosition();
            PieMenu_Open(mouse.x, mouse.y);
        }
    }
    // Track right-click press for tap detection
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        rightClickStart = GetMousePosition();
        rightClickTime = GetTime();
    }
    // Right-click tap (quick release, no drag) → open pie menu in toggle mode
    if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT) && !PieMenu_IsOpen()) {
        Vector2 mouse = GetMousePosition();
        float dx = mouse.x - rightClickStart.x;
        float dy = mouse.y - rightClickStart.y;
        float dist = sqrtf(dx * dx + dy * dy);
        double elapsed = GetTime() - rightClickTime;
        if (dist < RIGHT_TAP_MAX_DIST && elapsed < RIGHT_TAP_MAX_TIME) {
            PieMenu_Open(mouse.x, mouse.y);
            isDragging = false;
            return;
        }
    }
    // Right-click hold (past tap time, no drag) → open pie menu in hold-drag mode
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && !PieMenu_IsOpen()) {
        double elapsed = GetTime() - rightClickTime;
        if (elapsed >= RIGHT_TAP_MAX_TIME) {
            Vector2 mouse = GetMousePosition();
            float dx = mouse.x - rightClickStart.x;
            float dy = mouse.y - rightClickStart.y;
            float dist = sqrtf(dx * dx + dy * dy);
            if (dist < RIGHT_TAP_MAX_DIST) {
                PieMenu_OpenHold(rightClickStart.x, rightClickStart.y);
                isDragging = false;
                return;
            }
        }
    }
#endif
    if (PieMenu_IsOpen()) {
        PieMenu_Update();
        return;  // Block all other input while pie menu is open
    }
    if (PieMenu_JustClosed()) {
        return;  // Suppress input for one frame after pie menu closes
    }

    // ========================================================================
    // Stockpile hover controls (always active when hovering)
    // ========================================================================
    if (hoveredStockpile >= 0) {
        // Actions are shared with the inspector panel (src/core/selection.c)
        // Priority = -, stack size ] [, Shift+] [ containers: the stockpile verbs (selection.c)
        if (RunSelectionVerbKeys(SEL_STOCKPILE, hoveredStockpile)) return;
        // Filter toggles - only in normal mode (two-level keyboard navigation)
        if (inputMode == MODE_NORMAL) {
            // Material sub-filters (data-driven from STOCKPILE_MATERIAL_FILTERS table)
            for (int i = 0; i < STOCKPILE_MATERIAL_FILTER_COUNT; i++) {
                if (IsKeyPressed(KEY_ZERO + (STOCKPILE_MATERIAL_FILTERS[i].key - '0'))) {
                    StockpileToggleMaterial(hoveredStockpile, i);
                    return;
                }
            }

            if (activeFilterCategory < 0) {
                // Top level: category keys enter a category
                for (int cat = 0; cat < FILTER_CAT_COUNT; cat++) {
                    int rayKey = KEY_A + (FILTER_CATEGORY_KEYS[cat] - 'A');
                    if (IsKeyPressed(rayKey)) {
                        activeFilterCategory = cat;
                        AddMessage(TextFormat("Filter: %s", FILTER_CATEGORY_NAMES[cat]), WHITE);
                        return;
                    }
                }
                // X toggles all filters globally
                if (IsKeyPressed(KEY_X)) {
                    StockpileToggleAll(hoveredStockpile);
                    return;
                }
            } else {
                // Inside category: ESC/Backspace goes back
                if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
                    activeFilterCategory = -1;
                    return;
                }
                // X toggles all items in this category
                if (IsKeyPressed(KEY_X)) {
                    StockpileToggleCategory(hoveredStockpile, activeFilterCategory);
                    return;
                }
                // A/B/C/... toggle individual items in this category
                int itemIdx = 0;
                for (int i = 0; i < STOCKPILE_FILTER_COUNT; i++) {
                    if ((int)STOCKPILE_FILTERS[i].category != activeFilterCategory) continue;
                    if (IsKeyPressed(KEY_A + itemIdx)) {
                        StockpileToggleItem(hoveredStockpile, i);
                        return;
                    }
                    itemIdx++;
                }
            }
        }
    }

    // ========================================================================
    // Workshop hover controls (always active when hovering)
    // ========================================================================
    if (hoveredWorkshop >= 0 && inputMode == MODE_NORMAL) {
        static int lastHoveredWorkshop = -1;
        Workshop* ws = &workshops[hoveredWorkshop];
        
        // Reset bill selection when workshop changes
        if (hoveredWorkshop != lastHoveredWorkshop) {
            workshopSelectedBillIdx = 0;
            lastHoveredWorkshop = hoveredWorkshop;
        }
        // Clamp selection to valid range
        if (ws->billCount > 0) {
            if (workshopSelectedBillIdx >= ws->billCount) workshopSelectedBillIdx = ws->billCount - 1;
            if (workshopSelectedBillIdx < 0) workshopSelectedBillIdx = 0;
        } else {
            workshopSelectedBillIdx = 0;
        }
        
        // 1-9 = Add bill for specific recipe
        {
            int recipeCount;
            const Recipe* recipes = GetRecipesForWorkshop(ws->type, &recipeCount);
            (void)recipes;
            for (int r = 0; r < recipeCount && r < 9; r++) {
                if (IsKeyPressed(KEY_ONE + r)) {
                    WorkshopAddBill(hoveredWorkshop, r);   // src/core/selection.c
                    if (ws->billCount > 0) workshopSelectedBillIdx = ws->billCount - 1;
                    return;
                }
            }
        }
        
        // [ / ] = Select previous/next bill
        if (IsKeyPressed(KEY_LEFT_BRACKET) && ws->billCount > 0) {
            if (workshopSelectedBillIdx > 0) workshopSelectedBillIdx--;
            return;
        }
        if (IsKeyPressed(KEY_RIGHT_BRACKET) && ws->billCount > 0) {
            if (workshopSelectedBillIdx < ws->billCount - 1) workshopSelectedBillIdx++;
            return;
        }
        
        // M = Cycle bill mode on selected bill
        // The actions below are shared with the inspector panel (src/core/selection.c)
        if (IsKeyPressed(KEY_M) && ws->billCount > 0) {
            WorkshopCycleBillMode(hoveredWorkshop, workshopSelectedBillIdx);
            return;
        }

        // +/= and - = Adjust target count on selected bill (shift = 5)
        if ((IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) && ws->billCount > 0) {
            WorkshopAdjustBillTarget(hoveredWorkshop, workshopSelectedBillIdx, IsKeyDown(KEY_LEFT_SHIFT) ? 5 : 1);
            return;
        }
        if ((IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) && ws->billCount > 0) {
            WorkshopAdjustBillTarget(hoveredWorkshop, workshopSelectedBillIdx, IsKeyDown(KEY_LEFT_SHIFT) ? -5 : -1);
            return;
        }

        // X = Remove selected bill
        if (IsKeyPressed(KEY_X)) {
            if (ws->billCount > 0) {
                WorkshopRemoveBill(hoveredWorkshop, workshopSelectedBillIdx);
                if (workshopSelectedBillIdx >= ws->billCount && ws->billCount > 0)
                    workshopSelectedBillIdx = ws->billCount - 1;
            } else {
                AddMessage("No bills to remove", RED);
            }
            return;
        }

        // P = Toggle pause on selected bill
        if (IsKeyPressed(KEY_P)) {
            if (ws->billCount > 0) WorkshopToggleBillPause(hoveredWorkshop, workshopSelectedBillIdx);
            return;
        }

        // L link stockpiles, D deconstruct: the workshop verbs (selection.c)
        if (RunSelectionVerbKeys(SEL_WORKSHOP, hoveredWorkshop)) {
            hoveredWorkshop = -1;
            return;
        }
    }

    // ========================================================================
    // Stockpile linking mode
    // ========================================================================
    if (workSubMode == SUBMODE_LINKING_STOCKPILES && linkingWorkshopIdx >= 0) {
        // ESC or L again = exit linking mode
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_L)) {
            workSubMode = SUBMODE_NONE;
            linkingWorkshopIdx = -1;
            AddMessage("Linking mode cancelled", GRAY);
            return;
        }
        
        // Left-click stockpile = link it
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && hoveredStockpile >= 0) {
            bool success = LinkStockpileToWorkshop(linkingWorkshopIdx, hoveredStockpile);
            if (success) {
                AddMessage(TextFormat("Linked Stockpile #%d to Workshop #%d (slot %d/4)", 
                    hoveredStockpile, linkingWorkshopIdx, 
                    workshops[linkingWorkshopIdx].linkedInputCount), 
                    GREEN);
                // Stay in linking mode to allow linking more stockpiles
            } else {
                // Check why it failed
                if (workshops[linkingWorkshopIdx].linkedInputCount >= MAX_LINKED_STOCKPILES) {
                    AddMessage("Workshop slots full (4/4)", RED);
                } else if (IsStockpileLinked(linkingWorkshopIdx, hoveredStockpile)) {
                    AddMessage("Stockpile already linked to this workshop", RED);
                } else {
                    AddMessage("Failed to link stockpile", RED);
                }
            }
            return;
        }
        
        // Don't process other inputs while in linking mode
        return;
    }

    // ========================================================================
    // Lever / plate linking mode (the inspector's Link button, mechanisms.h)
    // ========================================================================
    if (workSubMode == SUBMODE_LINKING_MECHANISM) {
        if (linkingMechanismIdx < 0 || mechanisms[linkingMechanismIdx].kind == MECH_NONE ||
            IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_L)) {
            workSubMode = SUBMODE_NONE;
            inputMode = MODE_NORMAL;
            linkingMechanismIdx = -1;
            AddMessage("Linking done", GRAY);
            return;
        }
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !ui_wants_mouse()) {
            Vector2 g = ScreenToGrid(GetMousePosition());
            int tx = (int)g.x, ty = (int)g.y, tz = currentViewZ;
            const char* what = MechTargetName(tx, ty, tz);
            if (!what) AddMessage("Not a door or valve", RED);
            else if (ToggleMechanismLink(linkingMechanismIdx, tx, ty, tz)) AddMessage(TextFormat("Linked the %s at %d,%d", what, tx, ty), GREEN);
            else AddMessage(TextFormat("Unlinked the %s at %d,%d", what, tx, ty), GRAY);
        }
        return;
    }

    // ========================================================================
    // Global controls (always active)
    // ========================================================================

    // The global keys (src/core/global_keys.c): each runs a command line (src/ui/commands.c):
    // F follow (normal mode only: the tools use F), , . level, Tab overlay, Space pause, F5-F8
    for (int i = 0; i < GlobalKeyCount(); i++) {
        const GlobalKey* gk = GlobalKeyAt(i);
        if (!gk->key || !gk->line || (gk->normalOnly && inputMode != MODE_NORMAL)) continue;
        if (IsKeyPressed(gk->key)) CommandRunLine(gk->line);
    }

    // Click-to-move for drafted movers (right-click while following a drafted mover)
    if (followMoverIdx >= 0 && followMoverIdx < moverCount
        && movers[followMoverIdx].isDrafted
        && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        Vector2 gp = ScreenToGrid(GetMousePosition());
        int gx = (int)gp.x, gy = (int)gp.y;
        int gz = currentViewZ;
        if (gx >= 0 && gx < gridWidth && gy >= 0 && gy < gridHeight
            && IsCellWalkableAt(gz, gy, gx)) {
            Mover* dm = &movers[followMoverIdx];
            dm->goal = (Point){gx, gy, gz};
            dm->needsRepath = true;
            AddMessage(TextFormat("%s: move to (%d,%d)", MoverDisplayName(followMoverIdx), gx, gy), YELLOW);
        }
    }

    // Zoom
    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        Vector2 mw = ScreenToGrid(GetMousePosition());
        zoom += wheel * 0.1f;
        if (zoom < 0.1f) zoom = 0.1f;
        if (zoom > 5.0f) zoom = 5.0f;
        float size = CELL_SIZE * zoom;
        offset.x = GetMousePosition().x - mw.x * size;
        offset.y = GetMousePosition().y - mw.y * size;
    }

    // Pan
    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
        if (followMoverIdx >= 0) {
            followMoverIdx = -1;
            AddMessage("Follow: off", GRAY);
        }
        Vector2 d = GetMouseDelta();
        offset.x += d.x;
        offset.y += d.y;
    }

    // Reset view (disabled)
    // if (IsKeyPressed(KEY_C)) {
    //     zoom = 1.0f;
    //     offset.x = (1280 - gridWidth * CELL_SIZE * zoom) / 2.0f;
    //     offset.y = (800 - gridHeight * CELL_SIZE * zoom) / 2.0f;
    // }

    // Front view mode toggle (no keybind — toggled via UI only)
    // frontViewMode can still be set programmatically or via inspector
    // Front view depth scrolling (,/. scroll Y-slice instead of z-level)
    // [/] adjust number of visible depth layers
    if (frontViewMode) {
        if (IsKeyPressed(KEY_UP) && frontViewY > 0) frontViewY--;
        if (IsKeyPressed(KEY_DOWN) && frontViewY < gridHeight - 1) frontViewY++;
        if (IsKeyPressed(KEY_RIGHT_BRACKET) && frontViewDepth < gridHeight) frontViewDepth++;
        if (IsKeyPressed(KEY_LEFT_BRACKET) && frontViewDepth > 1) frontViewDepth--;
    }


    // ========================================================================
    // Navigation: ESC, re-tap mode key
    // Handle before ui_wants_mouse check since bar buttons trigger pending keys
    // ========================================================================
    
    if (CheckKey(KEY_ESCAPE)) {
        if (showQuitConfirm) {
            // Second ESC while quit confirm shown = quit
            shouldQuit = true;
            return;
        } else if (inputAction != ACTION_NONE || inputMode != MODE_NORMAL) {
            // Back out of menus (player mode: go straight to normal)
            if (devUI) InputMode_Back();
            else InputMode_ExitToNormal();
            return;
        } else if (selection.kind != SEL_NONE) {
            // At root with something selected: deselect first
            ClearSelection();
            return;
        } else {
            // At root - show quit confirm
            showQuitConfirm = true;
            return;
        }
    }
    
    // Any other key dismisses quit confirm
    if (showQuitConfirm && GetKeyPressed() != 0) {
        showQuitConfirm = false;
        return;
    }



    // Re-tap mode key exits to normal (only at top level, not when in submode/action)
    if (inputMode == MODE_DRAW && inputAction == ACTION_NONE && CheckKey(KEY_D)) { InputMode_ExitToNormal(); return; }
    if (inputMode == MODE_WORK && workSubMode == SUBMODE_NONE && inputAction == ACTION_NONE && CheckKey(KEY_W)) { InputMode_ExitToNormal(); return; }
    if (inputMode == MODE_SANDBOX && inputAction == ACTION_NONE && CheckKey(KEY_S)) { InputMode_ExitToNormal(); return; }

    // ========================================================================
    // Mode selection (only in normal mode)
    // ========================================================================
    
    if (inputMode == MODE_NORMAL) {
        // E: the selected mover does what fits the cell under the mouse (eat that, sit there, go
        // there); Shift+E adds it to its queue (src/entities/action_queue.c)
        if (CheckKey(KEY_E) && selection.kind == SEL_MOVER && SelectionValid()) {
            Vector2 g = ScreenToGrid(GetMousePosition());
            bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            char said[128];
            if (QueueCommandAt(selection.idx, (int)g.x, (int)g.y, z, shift, said, sizeof(said))) AddMessage(said, SKYBLUE);
            else AddMessage(said[0] ? said : "Nothing to do there", GRAY);
            return;
        }
        // D while hovering a mover drafts/undrafts (before D = draw mode): the mover verbs (selection.c)
        if (hoveredMover >= 0 && hoveredMover < moverCount && RunSelectionVerbKeys(SEL_MOVER, hoveredMover)) return;
        if (devUI && CheckKey(KEY_D)) { inputMode = MODE_DRAW; return; }
        if (devUI && CheckKey(KEY_W)) { inputMode = MODE_WORK; return; }
        if (devUI && CheckKey(KEY_S)) { inputMode = MODE_SANDBOX; return; }

        // Skip grid interactions if UI wants mouse
        if (ui_wants_mouse()) return;

        // Click to select (src/core/selection.c): a mover, animal or blueprint under
        // the mouse is selected and the click goes no further (no quick-edit wall);
        // a click on nothing selectable deselects. Top view only for now.
        {
            static bool selectClickHeld = false;
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !frontViewMode) {
                Vector2 mw = ScreenToWorld(GetMousePosition());
                Selection pick = PickSelectionAt(mw.x, mw.y, z);
                if (pick.kind != SEL_NONE) {
                    Select(pick.kind, pick.idx);
                    selectClickHeld = true;
                    return;
                }
                ClearSelection();
            }
            if (selectClickHeld) {
                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) return;
                selectClickHeld = false;
            }
        }
        
        // Quick edit: left-click = wall, right-click = erase (when enabled, dev UI only)
        if (quickEditEnabled && devUI) {
            int x = gx, y = gy;
            if (x >= 0 && x < gridWidth && y >= 0 && y < gridHeight) {
                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                    // Don't allow walls on workshop tiles
                    if (IsWorkshopTile(x, y, z)) {
                        // Skip silently in quick-edit mode
                    } else {
                        MaterialType mat = IsKeyDown(KEY_TWO) ? MAT_OAK : MAT_GRANITE;
                        if (GRID(x, y, z) != CELL_WALL || GetWallMaterial(x, y, z) != mat) {
                            DisplaceWater(x, y, z);
                            GRID(x, y, z) = CELL_WALL;
                            SetWallMaterial(x, y, z, mat);
                            ClearWallNatural(x, y, z);
                            SetWallFinish(x, y, z, FINISH_SMOOTH);
                            MarkChunkDirty(x, y, z);
                            for (int i = 0; i < moverCount; i++) {
                                Mover* m = &movers[i];
                                if (!m->active) continue;
                                for (int j = m->pathIndex; j >= 0; j--) {
                                    if (moverPaths[i][j].x == x && moverPaths[i][j].y == y && moverPaths[i][j].z == z) {
                                        m->needsRepath = true;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
                if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && (GetTime() - rightClickTime) > RIGHT_TAP_MAX_TIME) {
                    if (IsLadderCell(GRID(x, y, z))) {
                        EraseLadder(x, y, z);
                    } else if (CellIsDirectionalRamp(GRID(x, y, z))) {
                        EraseRamp(x, y, z);
                    } else {
                        if (GRID(x, y, z) != CELL_AIR) {
                            GRID(x, y, z) = CELL_AIR;
                            SetWallMaterial(x, y, z, MAT_NONE);
                            ClearWallNatural(x, y, z);
                            SetWallFinish(x, y, z, FINISH_ROUGH);
                            SET_CELL_SURFACE(x, y, z, SURFACE_BARE);
                            if (HAS_FLOOR(x, y, z)) {
                                CLEAR_FLOOR(x, y, z);
                                SetFloorMaterial(x, y, z, MAT_NONE);
                                ClearFloorNatural(x, y, z);
                                SetFloorFinish(x, y, z, FINISH_ROUGH);
                            }
                            MarkChunkDirty(x, y, z);
                            DestabilizeWater(x, y, z);
                        }
                    }
                }
            }
        }
        return;
    }

    // ========================================================================
    // Action selection (in a mode, no action selected)
    // ========================================================================
    
    if (inputAction == ACTION_NONE) {
        // I in the Work bar flips how builds are done (now / order)
        if (inputMode == MODE_WORK && CheckKey(KEY_I)) { InputMode_FlipHow(); return; }
        // Re-tap submode key to go back (e.g. WORK > DIG -> press D -> WORK)
        if (inputMode == MODE_WORK && workSubMode != SUBMODE_NONE) {
            bool backFromSubmode = false;
            switch (workSubMode) {
                case SUBMODE_DIG:     backFromSubmode = CheckKey(KEY_D); break;
                case SUBMODE_BUILD:   backFromSubmode = CheckKey(KEY_B); break;
                case SUBMODE_HARVEST: backFromSubmode = CheckKey(KEY_H); break;
                default: break;
            }
            if (backFromSubmode) {
                workSubMode = SUBMODE_NONE;
                return;
            }
        }

        // Action selection — data-driven from registry
        if (inputMode == MODE_WORK && workSubMode == SUBMODE_NONE) {
            // Submodes are hardcoded (not actions)
            if (CheckKey(KEY_D)) { workSubMode = SUBMODE_DIG; }
            if (CheckKey(KEY_B)) { workSubMode = SUBMODE_BUILD; }
            if (CheckKey(KEY_H)) { workSubMode = SUBMODE_HARVEST; }
        }
        // Select action from registry for current mode/submode
        const ActionDef* defs[MAX_BAR_ITEMS];
        int count = GetActionsForContext(inputMode, workSubMode, ACTION_NONE, defs, MAX_BAR_ITEMS);
        for (int i = 0; i < count; i++) {
            if (CheckKey(KeyFromChar(defs[i]->barKey))) {
                InputMode_SelectAction(InputMode_HowOf(defs[i]->action));   // now or order
                // Reset material for draw actions that need it
                if (inputAction == ACTION_DRAW_WALL || inputAction == ACTION_DRAW_FLOOR ||
                    inputAction == ACTION_DRAW_LADDER) {
                    selectedMaterial = 1;
                }
                break;
            }
        }
        return;
    }
    
    // ========================================================================
    // I flips the tool between now (Draw: instant) and order (Work: the workers build it)
    // when it has a twin in the registry (docs/todo/ui/find-palette.md step 3)
    // ========================================================================
    {
        const ActionDef* curDef = GetActionDef(inputAction);
        if (curDef->twin != ACTION_NONE && CheckKey(KEY_I)) {
            InputMode_FlipHow();   // also flips how the next picked tool builds
            return;
        }
    }

    // ========================================================================
    // Category child selection — data-driven from registry
    // ========================================================================
    {
        // A now category (Draw with a Work twin) picks from its twin's kinds, like its bar shows
        const ActionDef* curDef = GetActionDef(inputAction);
        if (curDef->requiredMode == MODE_DRAW && curDef->twin != ACTION_NONE) curDef = GetActionDef(curDef->twin);
        const ActionDef* children[MAX_BAR_ITEMS];
        int childCount = GetActionsForContext(curDef->requiredMode, curDef->requiredSubMode, curDef->action, children, MAX_BAR_ITEMS);
        if (childCount > 0) {
            for (int i = 0; i < childCount; i++) {
                if (CheckKey(KeyFromChar(children[i]->barKey))) {
                    InputMode_SelectAction(InputMode_HowOf(children[i]->action));   // now or order
                    return;
                }
            }
        }
    }

    // ========================================================================
    // Re-tap action key to go back one level
    // ========================================================================
    
    // Back-one-level — data-driven from registry
    const ActionDef* backDef = GetActionDef(inputAction);
    if (backDef->barKey != 0 && CheckKey(KeyFromChar(backDef->barKey))) {
        InputMode_Back();
        return;
    }
    
    // Re-tap submode key to go back to submode selection
    // (e.g., pressing D while in WORK > DIG > MINE goes back to WORK > DIG)
    if (inputMode == MODE_WORK && workSubMode != SUBMODE_NONE && inputAction != ACTION_NONE) {
        bool backToSubmode = false;
        switch (workSubMode) {
            case SUBMODE_DIG:     backToSubmode = CheckKey(KEY_D); break;
            case SUBMODE_BUILD:   backToSubmode = CheckKey(KEY_B); break;
            case SUBMODE_HARVEST: backToSubmode = CheckKey(KEY_H); break;
            default: break;
        }
        if (backToSubmode) {
            inputAction = ACTION_NONE;
            selectedMaterial = 1;
            return;
        }
    }

    // ========================================================================
    // The tool's option: material 1-9, recipe (R), ramp / belt direction, brush size, torch
    // colour, crop, tree type. All from the tool's ActionOption (see "Tool options" above).
    // ========================================================================
    if (HandleToolOptionKeys()) return;

    // ========================================================================
    // Action execution (drag handling)
    // ========================================================================
    
    // Skip grid interactions if UI wants mouse
    if (ui_wants_mouse()) return;
    
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    
    // Start drag
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        if (frontViewMode) {
            dragStartX = gx;
            dragStartY = gy;
            dragStartZ = gz;
        } else {
            Vector2 gp = ScreenToGrid(GetMousePosition());
            dragStartX = (int)gp.x;
            dragStartY = (int)gp.y;
            dragStartZ = currentViewZ;
        }
        isDragging = true;

        // Terrain sculpting: start stroke on mouse down
        if (inputAction == ACTION_SANDBOX_SCULPT) {
            lastBrushX = dragStartX;
            lastBrushY = dragStartY;
            brushStrokeActive = true;

            // Execute at initial position
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                // Left = raise
                ExecuteRaiseTerrain(lastBrushX, lastBrushY, z, terrainBrushRadius);
            } else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                // Right = lower
                ExecuteLowerTerrain(lastBrushX, lastBrushY, z, terrainBrushRadius);
            }
        }
    }

    // Terrain sculpting: continuous brush stroke while dragging
    if (brushStrokeActive && inputAction == ACTION_SANDBOX_SCULPT) {
        bool leftDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        bool rightDown = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);

        if (leftDown || rightDown) {
            Vector2 gp = ScreenToGrid(GetMousePosition());
            int mouseX = (int)gp.x;
            int mouseY = (int)gp.y;

            if (mouseX != lastBrushX || mouseY != lastBrushY) {
                if (leftDown) {
                    // Left = raise
                    InterpolateBrushStroke(lastBrushX, lastBrushY, mouseX, mouseY, z,
                                          terrainBrushRadius, ExecuteRaiseTerrain);
                } else if (rightDown) {
                    // Right = lower
                    InterpolateBrushStroke(lastBrushX, lastBrushY, mouseX, mouseY, z,
                                          terrainBrushRadius, ExecuteLowerTerrain);
                }
                lastBrushX = mouseX;
                lastBrushY = mouseY;
            }
        }
    }

    // End terrain sculpting stroke
    if (brushStrokeActive && (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) || IsMouseButtonReleased(MOUSE_BUTTON_RIGHT))) {
        brushStrokeActive = false;
        lastBrushX = -1;
        lastBrushY = -1;
    }

    // Shift+left drag: continuous placement at the mouse (soil piles)
    if (isDragging && shift && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        const ToolHandler* tool = GetToolHandler(inputAction);
        if (tool && tool->shiftDrag) {
            Vector2 gp = ScreenToGrid(GetMousePosition());
            int mouseX = (int)gp.x;
            int mouseY = (int)gp.y;
            ToolArgs args = {mouseX, mouseY, mouseX, mouseY, z, dragStartX, dragStartY,
                             mouseX, mouseY, shift, tool->param};
            tool->shiftDrag(&args);
        }
    }

    // End drag - execute action
    if (isDragging && (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) || IsMouseButtonReleased(MOUSE_BUTTON_RIGHT))) {
        isDragging = false;
        bool leftClick = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);

        // In front view: drag covers X range + Z range at fixed world Y
        if (frontViewMode) {
            int fx1, fy1, fx2, fy2, fz1, fz2;
            GetDragRectFrontView(&fx1, &fy1, &fx2, &fy2, &fz1, &fz2, gx, gy, gz);
            for (int fz = fz1; fz <= fz2; fz++) {
                switch (inputAction) {
                    case ACTION_DRAW_WALL:
                        if (leftClick) ExecuteBuildWall(fx1, fy1, fx2, fy2, fz);
                        else ExecuteErase(fx1, fy1, fx2, fy2, fz);
                        break;
                    case ACTION_DRAW_FLOOR:
                        if (leftClick) ExecuteBuildFloor(fx1, fy1, fx2, fy2, fz);
                        else ExecuteErase(fx1, fy1, fx2, fy2, fz);
                        break;
                    case ACTION_DRAW_LADDER:
                        if (leftClick) ExecuteBuildLadder(fx1, fy1, fx2, fy2, fz);
                        else ExecuteErase(fx1, fy1, fx2, fy2, fz);
                        break;
                    case ACTION_DRAW_SOIL_DIRT:
                        if (leftClick) ExecuteBuildSoil(fx1, fy1, fx2, fy2, fz, CELL_WALL, MAT_DIRT, "dirt");
                        else ExecuteErase(fx1, fy1, fx2, fy2, fz);
                        break;
                    case ACTION_DRAW_SOIL_ROCK:
                        if (leftClick) ExecuteBuildRock(fx1, fy1, fx2, fy2, fz);
                        else ExecuteErase(fx1, fy1, fx2, fy2, fz);
                        break;
                    default: break;
                }
            }
            goto done_execute;
        }

        int x1, y1, x2, y2;
        GetDragRect(&x1, &y1, &x2, &y2);

        // Run the tool for this action (toolHandlers table above HandleInput)
        const ToolHandler* tool = GetToolHandler(inputAction);
        if (tool) {
            ToolArgs args = {x1, y1, x2, y2, z, dragStartX, dragStartY,
                             (int)mouseGrid.x, (int)mouseGrid.y, shift, tool->param};
            ToolFn fn = leftClick ? tool->apply : tool->erase;
            if (fn) fn(&args);
        }
        done_execute:;
    }
}

// ============================================================================
// Keys as data for the help panel (docs/todo/ui/find-palette.md step 5): the global keys
// (handled in HandleInput / main.c) as a table, plus the selection verbs' keys (selection.c).
// The tools' keys show in the bar (registry + tool options), commands in the console's help.
// ============================================================================


static const char* HelpKeyName(int key) {
    if (key >= KEY_A && key <= KEY_Z) { static char b[2][2]; static int k; k ^= 1; b[k][0] = (char)('A' + (key - KEY_A)); b[k][1] = 0; return b[k]; }
    switch (key) {
        case KEY_EQUAL: return "=";
        case KEY_MINUS: return "-";
        case KEY_KP_ADD: return "KP+";
        case KEY_KP_SUBTRACT: return "KP-";
        case KEY_LEFT_BRACKET: return "[";
        case KEY_RIGHT_BRACKET: return "]";
        default: return "?";
    }
}

int InputHelpLines(char lines[][64], int max) {
    int n = 0;
    for (int i = 0; i < GlobalKeyCount() && n < max; i++) {   // src/core/global_keys.c
        const GlobalKey* gk = GlobalKeyAt(i);
        if (gk->label) snprintf(lines[n++], 64, "%-13s %s", gk->label, gk->help);
        else snprintf(lines[n++], 64, "%s", gk->help);
    }
    static const struct { SelectionKind kind; const char* title; } kinds[] = {
        { SEL_MOVER, "--- Hovering a mover ---" },
        { SEL_WORKSHOP, "--- Hovering a workshop ---" },
        { SEL_STOCKPILE, "--- Hovering a stockpile ---" },
    };
    for (int k = 0; k < 3 && n < max; k++) {
        snprintf(lines[n++], 64, "%s", kinds[k].title);
        for (int i = 0; i < SelectionVerbCount() && n < max; i++) {
            const SelectionVerb* v = SelectionVerbAt(i);
            if (v->kind != kinds[k].kind || !v->key) continue;
            char keys[24];
            snprintf(keys, sizeof(keys), "%s%s%s%s", v->shift ? "Shift+" : "", HelpKeyName(v->key),
                     v->altKey ? " / " : "", v->altKey ? HelpKeyName(v->altKey) : "");
            snprintf(lines[n++], 64, "%-13s %s", keys, v->name);
        }
    }
    return n;
}
