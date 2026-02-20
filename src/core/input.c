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
#include "input_mode.h"
#include "action_registry.h"
#include "pie_menu.h"

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

// ============================================================================
// Action Handlers - Execute the actual operations
// ============================================================================

// Map selectedMaterial to MaterialType and source ItemType
static void GetDrawMaterial(MaterialType* outMat, ItemType* outSource, const char** outName) {
    switch (selectedMaterial) {
        case 2: *outMat = MAT_OAK;    *outSource = ITEM_LOG;    *outName = "oak log";      break;
        case 3: *outMat = MAT_DIRT;    *outSource = ITEM_DIRT;   *outName = "dirt";          break;
        case 4: *outMat = MAT_OAK;    *outSource = ITEM_PLANKS; *outName = "oak planks";    break;
        case 5: *outMat = MAT_PINE;   *outSource = ITEM_PLANKS; *outName = "pine planks";   break;
        case 6: *outMat = MAT_BIRCH;  *outSource = ITEM_PLANKS; *outName = "birch planks";  break;
        case 7: *outMat = MAT_WILLOW; *outSource = ITEM_PLANKS; *outName = "willow planks"; break;
        case 8: *outMat = MAT_SANDSTONE; *outSource = ITEM_BLOCKS; *outName = "sandstone";  break;
        case 9: *outMat = MAT_SLATE;     *outSource = ITEM_BLOCKS; *outName = "slate";      break;
        default: *outMat = MAT_GRANITE; *outSource = ITEM_BLOCKS; *outName = "granite";     break;
    }
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
                if (grid[z][dy][dx] != CELL_WALL || GetWallMaterial(dx, dy, z) != MAT_DIRT) {
                    PlaceCellFull(dx, dy, z, NaturalTerrainSpec(CELL_WALL, MAT_DIRT, SURFACE_BARE, true, true));
                    SetWallSourceItem(dx, dy, z, ITEM_DIRT);
                    InvalidatePathsThroughCell(dx, dy, z);
                    count++;
                }
            } else {
                // Normal wall with material
                if (grid[z][dy][dx] != CELL_WALL || GetWallMaterial(dx, dy, z) != mat) {
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
            if (CellBlocksMovement(grid[z][dy][dx])) {
                continue;
            }
            bool hadFloor = HAS_FLOOR(dx, dy, z);
            // Set floor flag on AIR cell (for balconies/bridges)
            if (!hadFloor) {
                grid[z][dy][dx] = CELL_AIR;
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
            if (CellIsDirectionalRamp(grid[z][dy][dx])) {
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

            if (grid[z][dy][dx] != CELL_WALL || GetWallMaterial(dx, dy, z) != MAT_GRANITE || !IsWallNatural(dx, dy, z)) {
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
            CellType cell = grid[z][dy][dx];
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
        CellType below = grid[placeZ - 1][y][x];
        // If there's solid support below, we can place here
        if (CellIsSolid(below)) {
            break;
        }
        // Otherwise keep falling
        placeZ--;
    }
    
    // Step 2: Try to place at the landing position
    CellType landingCell = grid[placeZ][y][x];
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
            
            if (grid[placeZ][adjY][adjX] == CELL_AIR) {
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
                    CellType targetCell = grid[nz][ny][nx];
                    if (targetCell != CELL_AIR && !CellIsDirectionalRamp(targetCell)) continue;
                    
                    // Check if there's support (for horizontal/down placement)
                    if (nz > 0 && i > 0) {  // Not placing up
                        CellType below = grid[nz - 1][ny][nx];
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
                        if (grid[nz][adjY][adjX] == CELL_AIR) {
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
            for (int i = 0; i < MAX_STOCKPILES; i++) {
                if (stockpiles[i].active) {
                    RemoveStockpileCells(i, dx, dy, dx, dy);
                }
            }
            
            if (IsLadderCell(grid[z][dy][dx])) {
                EraseLadder(dx, dy, z);
                count++;
            } else if (CellIsDirectionalRamp(grid[z][dy][dx])) {
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
                if (grid[z][dy][dx] != eraseType || (eraseType == CELL_WALL && GetWallMaterial(dx, dy, z) != eraseMat)) {
                    grid[z][dy][dx] = eraseType;
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
            if (z >= 0 && z < gridDepth && designations[z][dy][dx].type == DESIGNATION_KNAP) {
                CancelDesignation(dx, dy, z);
                count++;
            } else if (z > 0 && designations[z-1][dy][dx].type == DESIGNATION_KNAP) {
                CancelDesignation(dx, dy, z - 1);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Cancelled %d knap designation%s", count, count > 1 ? "s" : ""), ORANGE);
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

static void ExecutePlaceWorkshopBlueprint(int x, int y, int z, WorkshopType type) {
    // Map workshop type to construction recipe
    int recipeIndex = -1;
    switch (type) {
        case WORKSHOP_CAMPFIRE:     recipeIndex = CONSTRUCTION_WORKSHOP_CAMPFIRE; break;
        case WORKSHOP_DRYING_RACK:  recipeIndex = CONSTRUCTION_WORKSHOP_DRYING_RACK; break;
        case WORKSHOP_ROPE_MAKER:   recipeIndex = CONSTRUCTION_WORKSHOP_ROPE_MAKER; break;
        case WORKSHOP_CHARCOAL_PIT: recipeIndex = CONSTRUCTION_WORKSHOP_CHARCOAL_PIT; break;
        case WORKSHOP_HEARTH:       recipeIndex = CONSTRUCTION_WORKSHOP_HEARTH; break;
        case WORKSHOP_STONECUTTER:  recipeIndex = CONSTRUCTION_WORKSHOP_STONECUTTER; break;
        case WORKSHOP_SAWMILL:      recipeIndex = CONSTRUCTION_WORKSHOP_SAWMILL; break;
        case WORKSHOP_KILN:         recipeIndex = CONSTRUCTION_WORKSHOP_KILN; break;
        case WORKSHOP_CARPENTER:    recipeIndex = CONSTRUCTION_WORKSHOP_CARPENTER; break;
        default: return;
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
        for (int i = 0; i < MAX_STOCKPILES; i++) {
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
                WaterCell* cell = &waterGrid[z][dy][dx];
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
                && grid[z][dy][dx] == CELL_AIR && CellIsSolid(grid[z-1][dy][dx])) {
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
            if (z > 0 && grid[z][dy][dx] == CELL_AIR && CellIsSolid(grid[z-1][dy][dx])) {
                fireZ = z - 1;
            }
            
            FireCell* cell = &fireGrid[fireZ][dy][dx];
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
            TempCell* cell = &temperatureGrid[z][dy][dx];
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
            TempCell* cell = &temperatureGrid[z][dy][dx];
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
            CellType cell = grid[z][dy][dx];
            // Can grow grass on dirt terrain or air
            if (cell == CELL_AIR) {
                // Convert to dirt terrain with proper material setup
                PlaceCellFull(dx, dy, z, NaturalTerrainSpec(CELL_WALL, MAT_DIRT, SURFACE_BARE, true, false));
                InvalidatePathsThroughCell(dx, dy, z);
            }
            if (grid[z][dy][dx] == CELL_WALL && GetWallMaterial(dx, dy, z) == MAT_DIRT) {
                // Set tall grass overlay and reset wear
                SetVegetation(dx, dy, z, VEG_GRASS_TALL);
                wearGrid[z][dy][dx] = 0;
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
            if (grid[z][dy][dx] == CELL_WALL && GetWallMaterial(dx, dy, z) == MAT_DIRT) {
                int surface = GET_CELL_SURFACE(dx, dy, z);
                if (surface != SURFACE_BARE) {
                    SET_CELL_SURFACE(dx, dy, z, SURFACE_BARE);
                    wearGrid[z][dy][dx] = wearMax;  // Set max wear so grass doesn't regrow immediately
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
            if (IsCellWalkableAt(z, dy, dx) && grid[z][dy][dx] == CELL_AIR) {
                grid[z][dy][dx] = CELL_BUSH;
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
            if (grid[z][dy][dx] == CELL_BUSH) {
                grid[z][dy][dx] = CELL_AIR;
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
    if (IsCellWalkableAt(z, y, x) && grid[z][y][x] == CELL_AIR) {
        grid[z][y][x] = CELL_TRACK;
        InvalidatePathsThroughCell(x, y, z);
        return true;
    }
    return false;
}

static void ExecutePlaceTrack(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    // Outline mode: only place on the 4 edges of the rectangle
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (dy == y1 || dy == y2 || dx == x1 || dx == x2) {
                if (TryPlaceTrackAt(dx, dy, z)) count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Placed %d track%s", count, count > 1 ? "s" : ""), GREEN);
    }
}

static void ExecuteRemoveTrack(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (grid[z][dy][dx] == CELL_TRACK) {
                grid[z][dy][dx] = CELL_AIR;
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
    CellType cell = grid[z][y][x];
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
    if (z > 0 && !CellIsSolid(grid[z - 1][y][x])) {
        AddMessage("Trees need solid ground below", RED);
        return;
    }
    
    // Grow a full tree instantly
    TreeGrowFull(x, y, z, currentTreeType);
    AddMessage(TextFormat("Planted %s tree at (%d, %d)", TreeTypeName(currentTreeType), x, y), GREEN);
}

static void CycleSandboxTreeType(void) {
    // Cycle through wood materials
    if (currentTreeType == MAT_OAK) currentTreeType = MAT_PINE;
    else if (currentTreeType == MAT_PINE) currentTreeType = MAT_BIRCH;
    else if (currentTreeType == MAT_BIRCH) currentTreeType = MAT_WILLOW;
    else currentTreeType = MAT_OAK;
    
    AddMessage(TextFormat("Tree type: %s", TreeTypeName(currentTreeType)), GREEN);
}

static void ExecuteRemoveTree(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            CellType cell = grid[z][dy][dx];
            if (cell == CELL_TREE_TRUNK || cell == CELL_TREE_BRANCH || cell == CELL_TREE_ROOT ||
                cell == CELL_TREE_FELLED || cell == CELL_TREE_LEAVES || cell == CELL_SAPLING) {
                CancelDesignation(dx, dy, z);
                grid[z][dy][dx] = CELL_AIR;
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
    if (grid[z][y][x] != CELL_WALL) return;
    if (!IsWallNatural(x, y, z)) return;

    MaterialType mat = GetWallMaterial(x, y, z);

    // Remove at current z
    grid[z][y][x] = CELL_AIR;
    SetWallMaterial(x, y, z, MAT_NONE);
    ClearWallNatural(x, y, z);

    // Place at z-1 if empty
    if (z > 0 && grid[z-1][y][x] == CELL_AIR) {
        grid[z-1][y][x] = CELL_WALL;
        SetWallMaterial(x, y, z-1, mat);
        SetWallNatural(x, y, z-1);
    }

    MarkChunkDirty(x, y, z);
    InvalidatePathsThroughCell(x, y, z);
    DestabilizeWater(x, y, z);
}

// Raise terrain: add cell at z if air and has support
static void RaiseTerrainCell(int x, int y, int z) {
    if (grid[z][y][x] != CELL_AIR) return;
    if (z == 0) return;  // Can't raise at bedrock
    if (!CellIsSolid(grid[z-1][y][x])) return;  // Need support

    // Inherit material from below
    MaterialType mat = GetWallMaterial(x, y, z-1);
    if (mat == MAT_NONE || mat == MAT_BEDROCK) mat = MAT_DIRT;

    grid[z][y][x] = CELL_WALL;
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
    
    Vector2 mouseGrid = ScreenToGrid(GetMousePosition());
    int z = currentViewZ;
    
    // Update hover states
    hoveredStockpile = GetStockpileAtGrid((int)mouseGrid.x, (int)mouseGrid.y, z);
    hoveredWorkshop = FindWorkshopAt((int)mouseGrid.x, (int)mouseGrid.y, z);
    if (paused) {
        Vector2 mouseWorld = ScreenToWorld(GetMousePosition());
        hoveredMover = GetMoverAtWorldPos(mouseWorld.x, mouseWorld.y, z);
        hoveredAnimal = GetAnimalAtWorldPos(mouseWorld.x, mouseWorld.y, z);
        hoveredItemCount = GetItemsAtCell((int)mouseGrid.x, (int)mouseGrid.y, z, hoveredItemCell, 16);
        // Check for designation hover (any type, not just mine)
        Designation* d = GetDesignation((int)mouseGrid.x, (int)mouseGrid.y, z);
        if (d && d->type != DESIGNATION_NONE) {
            hoveredDesignationX = (int)mouseGrid.x;
            hoveredDesignationY = (int)mouseGrid.y;
            hoveredDesignationZ = z;
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
        Stockpile* sp = &stockpiles[hoveredStockpile];
        if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) {
            if (sp->priority < 9) { sp->priority++; AddMessage(TextFormat("Stockpile priority: %d", sp->priority), WHITE); }
        }
        if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) {
            if (sp->priority > 1) { sp->priority--; AddMessage(TextFormat("Stockpile priority: %d", sp->priority), WHITE); }
        }
        if (IsKeyPressed(KEY_RIGHT_BRACKET)) {
            if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
                // Shift+] = increase max containers
                int newMax = sp->maxContainers + 1;
                SetStockpileMaxContainers(hoveredStockpile, newMax);
                AddMessage(TextFormat("Max containers: %d", sp->maxContainers), WHITE);
            } else {
                int newSize = sp->maxStackSize + 1;
                if (newSize <= MAX_STACK_SIZE) { SetStockpileMaxStackSize(hoveredStockpile, newSize); AddMessage(TextFormat("Stack size: %d", sp->maxStackSize), WHITE); }
            }
            return;
        }
        if (IsKeyPressed(KEY_LEFT_BRACKET)) {
            if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
                // Shift+[ = decrease max containers
                int newMax = sp->maxContainers - 1;
                if (newMax >= 0) { SetStockpileMaxContainers(hoveredStockpile, newMax); AddMessage(TextFormat("Max containers: %d", sp->maxContainers), WHITE); }
            } else {
                int newSize = sp->maxStackSize - 1;
                if (newSize >= 1) { SetStockpileMaxStackSize(hoveredStockpile, newSize); AddMessage(TextFormat("Stack size: %d", sp->maxStackSize), WHITE); }
            }
            return;
        }
        // Filter toggles - only in normal mode (R/G/B/O aren't used by other modes)
        if (inputMode == MODE_NORMAL) {
            // Item type filters (data-driven from STOCKPILE_FILTERS table)
            for (int i = 0; i < STOCKPILE_FILTER_COUNT; i++) {
                const StockpileFilterDef* filter = &STOCKPILE_FILTERS[i];
                int rayKey = KEY_A + (filter->key - 'a');  // Convert 'a'-'z' to KEY_A-KEY_Z
                if (IsKeyPressed(rayKey)) {
                    sp->allowedTypes[filter->itemType] = !sp->allowedTypes[filter->itemType];
                    InvalidateStockpileSlotCacheAll();
                    AddMessage(TextFormat("%s: %s", filter->displayName, sp->allowedTypes[filter->itemType] ? "ON" : "OFF"), filter->color);
                    return;
                }
            }
            // Material sub-filters (data-driven from STOCKPILE_MATERIAL_FILTERS table)
            for (int i = 0; i < STOCKPILE_MATERIAL_FILTER_COUNT; i++) {
                const StockpileMaterialFilterDef* mf = &STOCKPILE_MATERIAL_FILTERS[i];
                int rayKey = KEY_ZERO + (mf->key - '0');  // Convert '0'-'9' to KEY_ZERO-KEY_NINE
                if (IsKeyPressed(rayKey)) {
                    bool newVal = !sp->allowedMaterials[mf->material];
                    sp->allowedMaterials[mf->material] = newVal;
                    if (newVal) sp->allowedTypes[mf->parentItem] = true;
                    InvalidateStockpileSlotCacheAll();
                    AddMessage(TextFormat("%s: %s", mf->displayName, newVal ? "ON" : "OFF"), mf->color);
                    return;
                }
            }
            if (IsKeyPressed(KEY_X)) {
                // Check if any filter is on
                bool anyOn = false;
                for (int i = 0; i < ITEM_TYPE_COUNT; i++) {
                    if (sp->allowedTypes[i]) { anyOn = true; break; }
                }
                // Toggle: if any on -> all off, if all off -> all on
                bool newVal = !anyOn;
                for (int i = 0; i < ITEM_TYPE_COUNT; i++) {
                    sp->allowedTypes[i] = newVal;
                }
                InvalidateStockpileSlotCacheAll();
                AddMessage(newVal ? "All filters ON" : "All filters OFF", WHITE);
                return;
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
            for (int r = 0; r < recipeCount && r < 9; r++) {
                if (IsKeyPressed(KEY_ONE + r)) {
                    if (ws->billCount < MAX_BILLS_PER_WORKSHOP) {
                        AddBill(hoveredWorkshop, r, BILL_DO_FOREVER, 0);
                        workshopSelectedBillIdx = ws->billCount - 1;
                        AddMessage(TextFormat("Added bill: %s (Do Forever)", recipes[r].name), GREEN);
                    } else {
                        AddMessage(TextFormat("Workshop has max bills (%d)", MAX_BILLS_PER_WORKSHOP), RED);
                    }
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
        if (IsKeyPressed(KEY_M) && ws->billCount > 0) {
            Bill* bill = &ws->bills[workshopSelectedBillIdx];
            const char* modeNames[] = {"Do X Times", "Do Until X", "Do Forever"};
            bill->mode = (bill->mode + 1) % 3;
            if (bill->mode == BILL_DO_X_TIMES || bill->mode == BILL_DO_UNTIL_X) {
                if (bill->targetCount <= 0) bill->targetCount = 5;
            }
            bill->completedCount = 0;
            AddMessage(TextFormat("Bill mode: %s", modeNames[bill->mode]), WHITE);
            return;
        }
        
        // +/= and - = Adjust target count on selected bill
        if ((IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) && ws->billCount > 0) {
            Bill* bill = &ws->bills[workshopSelectedBillIdx];
            if (bill->mode == BILL_DO_X_TIMES || bill->mode == BILL_DO_UNTIL_X) {
                int step = IsKeyDown(KEY_LEFT_SHIFT) ? 5 : 1;
                bill->targetCount += step;
                AddMessage(TextFormat("Target: %d", bill->targetCount), WHITE);
            }
            return;
        }
        if ((IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) && ws->billCount > 0) {
            Bill* bill = &ws->bills[workshopSelectedBillIdx];
            if (bill->mode == BILL_DO_X_TIMES || bill->mode == BILL_DO_UNTIL_X) {
                int step = IsKeyDown(KEY_LEFT_SHIFT) ? 5 : 1;
                bill->targetCount -= step;
                if (bill->targetCount < 1) bill->targetCount = 1;
                AddMessage(TextFormat("Target: %d", bill->targetCount), WHITE);
            }
            return;
        }
        
        // X = Remove selected bill
        if (IsKeyPressed(KEY_X)) {
            if (ws->billCount > 0) {
                RemoveBill(hoveredWorkshop, workshopSelectedBillIdx);
                if (workshopSelectedBillIdx >= ws->billCount && ws->billCount > 0)
                    workshopSelectedBillIdx = ws->billCount - 1;
                AddMessage(TextFormat("Removed bill (now %d)", ws->billCount), ORANGE);
            } else {
                AddMessage("No bills to remove", RED);
            }
            return;
        }
        
        // P = Toggle pause on selected bill
        if (IsKeyPressed(KEY_P)) {
            if (ws->billCount > 0) {
                Bill* bill = &ws->bills[workshopSelectedBillIdx];
                bill->suspended = !bill->suspended;
                bill->suspendedNoStorage = false;
                AddMessage(TextFormat("Bill %s", bill->suspended ? "PAUSED" : "RESUMED"), 
                    bill->suspended ? RED : GREEN);
            }
            return;
        }
        
        // L = Link stockpiles (enter linking mode)
        if (IsKeyPressed(KEY_L)) {
            workSubMode = SUBMODE_LINKING_STOCKPILES;
            linkingWorkshopIdx = hoveredWorkshop;
            AddMessage("LINKING MODE: Click stockpile to link (ESC to cancel)", YELLOW);
            return;
        }
        
        // D = Delete workshop
        if (IsKeyPressed(KEY_D)) {
            DeleteWorkshop(hoveredWorkshop);
            AddMessage("Workshop deleted", ORANGE);
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
    // Global controls (always active)
    // ========================================================================

    // Follow mover (F) - toggles follow for hovered mover (paused)
    // Only in normal mode to avoid consuming draw/work/sandbox action keys.
    if (inputMode == MODE_NORMAL && IsKeyPressed(KEY_F)) {
        if (hoveredMover >= 0 && hoveredMover < moverCount) {
            if (followMoverIdx == hoveredMover) {
                followMoverIdx = -1;
                AddMessage("Follow: off", GRAY);
            } else {
                followMoverIdx = hoveredMover;
                currentViewZ = (int)movers[hoveredMover].z;
                AddMessage(TextFormat("Follow: Mover #%d", hoveredMover), GREEN);
            }
        } else if (followMoverIdx >= 0) {
            followMoverIdx = -1;
            AddMessage("Follow: off", GRAY);
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

    // Z-level
    if (IsKeyPressed(KEY_PERIOD) && currentViewZ < gridDepth - 1) currentViewZ++;
    if (IsKeyPressed(KEY_COMMA) && currentViewZ > 0) currentViewZ--;

    // Toggle dev/play UI
    if (IsKeyPressed(KEY_TAB)) {
        devUI = !devUI;
        if (!devUI) {
            InputMode_ExitToNormal();  // Exit any active mode
        } else {
            // Returning to dev UI — switch to sandbox mode
            gameMode = GAME_MODE_SANDBOX;
            hungerEnabled = false;
            energyEnabled = false;
            bodyTempEnabled = false;
        }
    }

    // Pause
    if (IsKeyPressed(KEY_SPACE)) paused = !paused;

    // Save/Load
    if (IsKeyPressed(KEY_F5)) {
        system("mkdir -p saves");
        if (SaveWorld("saves/debug_save.bin")) {
            AddMessage("World saved", GREEN);
            time_t now = time(NULL);
            struct tm* t = localtime(&now);
            char cmd[256];
            char timestamp[32];
            snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d_%02d-%02d-%02d",
                t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
            snprintf(cmd, sizeof(cmd), "gzip -c saves/debug_save.bin > saves/%s.bin.gz", timestamp);
            system(cmd);
            // Dump event log alongside save
            char logPath[128];
            snprintf(logPath, sizeof(logPath), "saves/%s.events.log", timestamp);
            EventLogDump(logPath);
            // Auto-audit: warn if violations found
            int violations = RunStateAudit(true);
            if (violations > 0) {
                char auditMsg[64];
                snprintf(auditMsg, sizeof(auditMsg), "Audit: %d violations (see console)", violations);
                AddMessage(auditMsg, RED);
            }
        }
    }
    if (IsKeyPressed(KEY_F6)) {
        if (LoadWorld("saves/debug_save.bin")) {
            InputMode_Reset();
            AddMessage("World loaded", GREEN);
        }
    }

    // Audit / Event log
    if (IsKeyPressed(KEY_F7)) {
        int violations = RunStateAudit(true);
        if (violations == 0) {
            AddMessage("Audit: 0 violations", GREEN);
        }
    }
    if (IsKeyPressed(KEY_F8)) {
        EventLogDump("navkit_events.log");
        char msg[64];
        snprintf(msg, sizeof(msg), "Event log dumped (%d entries)", EventLogCount());
        AddMessage(msg, GREEN);
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
        if (devUI && CheckKey(KEY_D)) { inputMode = MODE_DRAW; return; }
        if (devUI && CheckKey(KEY_W)) { inputMode = MODE_WORK; return; }
        if (devUI && CheckKey(KEY_S)) { inputMode = MODE_SANDBOX; return; }
        
        // Skip grid interactions if UI wants mouse
        if (ui_wants_mouse()) return;
        
        // Quick edit: left-click = wall, right-click = erase (when enabled, dev UI only)
        if (quickEditEnabled && devUI) {
            Vector2 gp = ScreenToGrid(GetMousePosition());
            int x = (int)gp.x, y = (int)gp.y;
            if (x >= 0 && x < gridWidth && y >= 0 && y < gridHeight) {
                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                    // Don't allow walls on workshop tiles
                    if (IsWorkshopTile(x, y, z)) {
                        // Skip silently in quick-edit mode
                    } else {
                        MaterialType mat = IsKeyDown(KEY_TWO) ? MAT_OAK : MAT_GRANITE;
                        if (grid[z][y][x] != CELL_WALL || GetWallMaterial(x, y, z) != mat) {
                            DisplaceWater(x, y, z);
                            grid[z][y][x] = CELL_WALL;
                            SetWallMaterial(x, y, z, mat);
                            ClearWallNatural(x, y, z);
                            SetWallFinish(x, y, z, FINISH_SMOOTH);
                            MarkChunkDirty(x, y, z);
                            for (int i = 0; i < moverCount; i++) {
                                Mover* m = &movers[i];
                                if (!m->active) continue;
                                for (int j = m->pathIndex; j >= 0; j--) {
                                    if (m->path[j].x == x && m->path[j].y == y && m->path[j].z == z) {
                                        m->needsRepath = true;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
                if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && (GetTime() - rightClickTime) > RIGHT_TAP_MAX_TIME) {
                    if (IsLadderCell(grid[z][y][x])) {
                        EraseLadder(x, y, z);
                    } else if (CellIsDirectionalRamp(grid[z][y][x])) {
                        EraseRamp(x, y, z);
                    } else {
                        if (grid[z][y][x] != CELL_AIR) {
                            grid[z][y][x] = CELL_AIR;
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
                inputAction = defs[i]->action;
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
    // Category child selection — data-driven from registry
    // ========================================================================
    {
        const ActionDef* curDef = GetActionDef(inputAction);
        const ActionDef* children[MAX_BAR_ITEMS];
        int childCount = GetActionsForContext(curDef->requiredMode, curDef->requiredSubMode, inputAction, children, MAX_BAR_ITEMS);
        if (childCount > 0) {
            for (int i = 0; i < childCount; i++) {
                if (CheckKey(KeyFromChar(children[i]->barKey))) {
                    inputAction = children[i]->action;
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
    // Material selection (when action is selected)
    // ========================================================================
    
    if (CheckKey(KEY_ONE)) selectedMaterial = 1;
    if (CheckKey(KEY_TWO)) selectedMaterial = 2;
    if (CheckKey(KEY_THREE)) selectedMaterial = 3;
    if (CheckKey(KEY_FOUR)) selectedMaterial = 4;
    if (CheckKey(KEY_FIVE)) selectedMaterial = 5;
    if (CheckKey(KEY_SIX)) selectedMaterial = 6;
    if (CheckKey(KEY_SEVEN)) selectedMaterial = 7;
    if (CheckKey(KEY_EIGHT)) selectedMaterial = 8;
    if (CheckKey(KEY_NINE)) selectedMaterial = 9;

    // ========================================================================
    // Terrain brush size selection (when sculpt action is selected)
    // ========================================================================

    if (inputAction == ACTION_SANDBOX_SCULPT) {
        if (CheckKey(KEY_ONE))   { terrainBrushRadius = 0; AddMessage("Brush: 1x1", GREEN); }
        if (CheckKey(KEY_TWO))   { terrainBrushRadius = 1; AddMessage("Brush: 3x3", GREEN); }
        if (CheckKey(KEY_THREE)) { terrainBrushRadius = 2; AddMessage("Brush: 5x5", GREEN); }
        if (CheckKey(KEY_FOUR))  { terrainBrushRadius = 3; AddMessage("Brush: 7x7", GREEN); }
    }

    // ========================================================================
    // Torch color preset selection (when light action is selected)
    // ========================================================================

    if (inputAction == ACTION_SANDBOX_LIGHT) {
        if (CheckKey(KEY_ONE))   { SetTorchPreset(TORCH_PRESET_WARM);  AddMessage("Torch: Warm (orange)", ORANGE); }
        if (CheckKey(KEY_TWO))   { SetTorchPreset(TORCH_PRESET_COOL);  AddMessage("Torch: Cool (blue)", BLUE); }
        if (CheckKey(KEY_THREE)) { SetTorchPreset(TORCH_PRESET_FIRE);  AddMessage("Torch: Fire (red)", RED); }
        if (CheckKey(KEY_FOUR))  { SetTorchPreset(TORCH_PRESET_GREEN); AddMessage("Torch: Green", GREEN); }
        if (CheckKey(KEY_FIVE))  { SetTorchPreset(TORCH_PRESET_WHITE); AddMessage("Torch: White", WHITE); }
    }

    // ========================================================================
    // Ramp direction selection (when ramp action is selected)
    // Arrow keys override auto-detect, 'A' resets to auto-detect
    // ========================================================================
    
    if (inputAction == ACTION_DRAW_RAMP) {
        // Arrow keys or HJKL to manually select direction
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_K)) {
            selectedRampDirection = CELL_RAMP_N;
            AddMessage("Ramp direction: North (manual)", WHITE);
        }
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_L)) {
            selectedRampDirection = CELL_RAMP_E;
            AddMessage("Ramp direction: East (manual)", WHITE);
        }
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_J)) {
            selectedRampDirection = CELL_RAMP_S;
            AddMessage("Ramp direction: South (manual)", WHITE);
        }
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_H)) {
            selectedRampDirection = CELL_RAMP_W;
            AddMessage("Ramp direction: West (manual)", WHITE);
        }
        // 'A' to reset to auto-detect
        if (IsKeyPressed(KEY_A)) {
            selectedRampDirection = CELL_AIR;
            AddMessage("Ramp direction: Auto-detect", WHITE);
        }
    }

    // ========================================================================
    // Build recipe cycling (R key when in build actions)
    // ========================================================================
    if (inputAction == ACTION_WORK_CONSTRUCT) {
        if (IsKeyPressed(KEY_R)) {
            // Cycle wall construction recipes
            int indices[16];
            int count = GetConstructionRecipeIndicesForCategory(BUILD_WALL, indices, 16);
            if (count > 0) {
                int cur = -1;
                for (int i = 0; i < count; i++) {
                    if (indices[i] == selectedWallRecipe) { cur = i; break; }
                }
                selectedWallRecipe = indices[(cur + 1) % count];
                const ConstructionRecipe* recipe = GetConstructionRecipe(selectedWallRecipe);
                AddMessage(TextFormat("Wall recipe: %s", recipe ? recipe->name : "?"), BLUE);
            }
            return;
        }
    }
    if (inputAction == ACTION_WORK_FLOOR) {
        if (IsKeyPressed(KEY_R)) {
            int indices[16];
            int count = GetConstructionRecipeIndicesForCategory(BUILD_FLOOR, indices, 16);
            if (count > 0) {
                int cur = -1;
                for (int i = 0; i < count; i++) {
                    if (indices[i] == selectedFloorRecipe) { cur = i; break; }
                }
                selectedFloorRecipe = indices[(cur + 1) % count];
                const ConstructionRecipe* recipe = GetConstructionRecipe(selectedFloorRecipe);
                AddMessage(TextFormat("Floor recipe: %s", recipe ? recipe->name : "?"), BLUE);
            }
            return;
        }
    }
    if (inputAction == ACTION_WORK_LADDER) {
        if (IsKeyPressed(KEY_R)) {
            int indices[16];
            int count = GetConstructionRecipeIndicesForCategory(BUILD_LADDER, indices, 16);
            if (count > 1) {
                int cur = -1;
                for (int i = 0; i < count; i++) {
                    if (indices[i] == selectedLadderRecipe) { cur = i; break; }
                }
                selectedLadderRecipe = indices[(cur + 1) % count];
                const ConstructionRecipe* recipe = GetConstructionRecipe(selectedLadderRecipe);
                AddMessage(TextFormat("Ladder recipe: %s", recipe ? recipe->name : "?"), BLUE);
            }
            return;
        }
    }
    if (inputAction == ACTION_WORK_FURNITURE) {
        if (IsKeyPressed(KEY_R)) {
            int indices[16];
            int count = GetConstructionRecipeIndicesForCategory(BUILD_FURNITURE, indices, 16);
            if (count > 0) {
                int cur = -1;
                for (int i = 0; i < count; i++) {
                    if (indices[i] == selectedFurnitureRecipe) { cur = i; break; }
                }
                selectedFurnitureRecipe = indices[(cur + 1) % count];
                const ConstructionRecipe* recipe = GetConstructionRecipe(selectedFurnitureRecipe);
                AddMessage(TextFormat("Furniture recipe: %s", recipe ? recipe->name : "?"), BLUE);
            }
            return;
        }
    }

    // ========================================================================
    // Sandbox tree type cycling
    // ========================================================================
    if (inputAction == ACTION_SANDBOX_TREE && CheckKey(KEY_T)) {
        CycleSandboxTreeType();
    }

    // ========================================================================
    // Action execution (drag handling)
    // ========================================================================
    
    // Skip grid interactions if UI wants mouse
    if (ui_wants_mouse()) return;
    
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    
    // Start drag
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        Vector2 gp = ScreenToGrid(GetMousePosition());
        dragStartX = (int)gp.x;
        dragStartY = (int)gp.y;
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

    // Pile mode: continuous placement while dragging with shift
    if (isDragging && shift && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        Vector2 gp = ScreenToGrid(GetMousePosition());
        int mouseX = (int)gp.x;
        int mouseY = (int)gp.y;
        
        // Execute pile placement at current mouse position
        switch (inputAction) {
            case ACTION_DRAW_SOIL_DIRT:
                ExecutePileSoil(mouseX, mouseY, z, CELL_WALL, MAT_DIRT, "dirt");
                break;
            case ACTION_DRAW_SOIL_CLAY:
                ExecutePileSoil(mouseX, mouseY, z, CELL_WALL, MAT_CLAY, "clay");
                break;
            case ACTION_DRAW_SOIL_GRAVEL:
                ExecutePileSoil(mouseX, mouseY, z, CELL_WALL, MAT_GRAVEL, "gravel");
                break;
            case ACTION_DRAW_SOIL_SAND:
                ExecutePileSoil(mouseX, mouseY, z, CELL_WALL, MAT_SAND, "sand");
                break;
            case ACTION_DRAW_SOIL_PEAT:
                ExecutePileSoil(mouseX, mouseY, z, CELL_WALL, MAT_PEAT, "peat");
                break;
            case ACTION_DRAW_SOIL_ROCK:
                ExecutePileSoil(mouseX, mouseY, z, CELL_WALL, MAT_GRANITE, "rock");
                break;
            default:
                break;
        }
    }

    // End drag - execute action
    if (isDragging && (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) || IsMouseButtonReleased(MOUSE_BUTTON_RIGHT))) {
        isDragging = false;
        bool leftClick = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
        
        int x1, y1, x2, y2;
        GetDragRect(&x1, &y1, &x2, &y2);
        
        switch (inputAction) {
            // Draw actions
            case ACTION_DRAW_WALL:
                if (leftClick) ExecuteBuildWall(x1, y1, x2, y2, z);
                else ExecuteErase(x1, y1, x2, y2, z);
                break;
            case ACTION_DRAW_FLOOR:
                if (leftClick) ExecuteBuildFloor(x1, y1, x2, y2, z);
                else ExecuteErase(x1, y1, x2, y2, z);
                break;
            case ACTION_DRAW_LADDER:
                if (leftClick) ExecuteBuildLadder(x1, y1, x2, y2, z);
                else ExecuteErase(x1, y1, x2, y2, z);
                break;
            case ACTION_DRAW_RAMP:
                if (leftClick) ExecuteBuildRamp(x1, y1, x2, y2, z);
                else ExecuteEraseRamp(x1, y1, x2, y2, z);
                break;
            case ACTION_DRAW_STOCKPILE:
                if (leftClick) ExecuteCreateStockpile(x1, y1, x2, y2, z);
                else ExecuteEraseStockpile(x1, y1, x2, y2, z);
                break;
            case ACTION_DRAW_WORKSHOP_STONECUTTER:
                if (leftClick) ExecutePlaceWorkshop(dragStartX, dragStartY, z, WORKSHOP_STONECUTTER);
                break;
            case ACTION_DRAW_WORKSHOP_SAWMILL:
                if (leftClick) ExecutePlaceWorkshop(dragStartX, dragStartY, z, WORKSHOP_SAWMILL);
                break;
            case ACTION_DRAW_WORKSHOP_KILN:
                if (leftClick) ExecutePlaceWorkshop(dragStartX, dragStartY, z, WORKSHOP_KILN);
                break;
            case ACTION_DRAW_WORKSHOP_CHARCOAL_PIT:
                if (leftClick) ExecutePlaceWorkshop(dragStartX, dragStartY, z, WORKSHOP_CHARCOAL_PIT);
                break;
            case ACTION_DRAW_WORKSHOP_HEARTH:
                if (leftClick) ExecutePlaceWorkshop(dragStartX, dragStartY, z, WORKSHOP_HEARTH);
                break;
            case ACTION_DRAW_WORKSHOP_DRYING_RACK:
                if (leftClick) ExecutePlaceWorkshop(dragStartX, dragStartY, z, WORKSHOP_DRYING_RACK);
                break;
            case ACTION_DRAW_WORKSHOP_ROPE_MAKER:
                if (leftClick) ExecutePlaceWorkshop(dragStartX, dragStartY, z, WORKSHOP_ROPE_MAKER);
                break;
            case ACTION_DRAW_WORKSHOP_CARPENTER:
                if (leftClick) ExecutePlaceWorkshop(dragStartX, dragStartY, z, WORKSHOP_CARPENTER);
                break;
            case ACTION_DRAW_WORKSHOP_CAMPFIRE:
                if (leftClick) ExecutePlaceWorkshop(dragStartX, dragStartY, z, WORKSHOP_CAMPFIRE);
                break;
            case ACTION_DRAW_SOIL_DIRT:
                if (leftClick) {
                    if (shift) {
                        // Pile mode - place single block at drag start with gravity/spreading
                        ExecutePileSoil(dragStartX, dragStartY, z, CELL_WALL, MAT_DIRT, "dirt");
                    } else {
                        ExecuteBuildSoil(x1, y1, x2, y2, z, CELL_WALL, MAT_DIRT, "dirt");
                    }
                }
                break;
            case ACTION_DRAW_SOIL_CLAY:
                if (leftClick) {
                if (shift) {
                    ExecutePileSoil(dragStartX, dragStartY, z, CELL_WALL, MAT_CLAY, "clay");
                } else {
                    ExecuteBuildSoil(x1, y1, x2, y2, z, CELL_WALL, MAT_CLAY, "clay");
                }
                }
                break;
            case ACTION_DRAW_SOIL_GRAVEL:
                if (leftClick) {
                if (shift) {
                    ExecutePileSoil(dragStartX, dragStartY, z, CELL_WALL, MAT_GRAVEL, "gravel");
                } else {
                    ExecuteBuildSoil(x1, y1, x2, y2, z, CELL_WALL, MAT_GRAVEL, "gravel");
                }
                }
                break;
            case ACTION_DRAW_SOIL_SAND:
                if (leftClick) {
                if (shift) {
                    ExecutePileSoil(dragStartX, dragStartY, z, CELL_WALL, MAT_SAND, "sand");
                } else {
                    ExecuteBuildSoil(x1, y1, x2, y2, z, CELL_WALL, MAT_SAND, "sand");
                }
                }
                break;
            case ACTION_DRAW_SOIL_PEAT:
                if (leftClick) {
                if (shift) {
                    ExecutePileSoil(dragStartX, dragStartY, z, CELL_WALL, MAT_PEAT, "peat");
                } else {
                    ExecuteBuildSoil(x1, y1, x2, y2, z, CELL_WALL, MAT_PEAT, "peat");
                }
                }
                break;
            case ACTION_DRAW_SOIL_ROCK:
                if (leftClick) {
                if (shift) {
                    ExecutePileSoil(dragStartX, dragStartY, z, CELL_WALL, MAT_GRANITE, "rock");
                } else {
                    ExecuteBuildRock(x1, y1, x2, y2, z);
                }
                }
                break;
            // Work actions
            case ACTION_WORK_MINE:
                if (leftClick) ExecuteDesignateMine(x1, y1, x2, y2, z);
                else ExecuteCancelMine(x1, y1, x2, y2, z);
                break;
            case ACTION_WORK_CHANNEL:
                if (leftClick) ExecuteDesignateChannel(x1, y1, x2, y2, z);
                else ExecuteCancelChannel(x1, y1, x2, y2, z);
                break;
            case ACTION_WORK_DIG_RAMP:
                if (leftClick) ExecuteDesignateDigRamp(x1, y1, x2, y2, z);
                else ExecuteCancelDigRamp(x1, y1, x2, y2, z);
                break;
            case ACTION_WORK_REMOVE_FLOOR:
                if (leftClick) ExecuteDesignateRemoveFloor(x1, y1, x2, y2, z);
                else ExecuteCancelRemoveFloor(x1, y1, x2, y2, z);
                break;
            case ACTION_WORK_REMOVE_RAMP:
                if (leftClick) ExecuteDesignateRemoveRamp(x1, y1, x2, y2, z);
                else ExecuteCancelRemoveRamp(x1, y1, x2, y2, z);
                break;
            case ACTION_WORK_CONSTRUCT:
                if (leftClick) ExecuteDesignateBuild(x1, y1, x2, y2, z);
                else ExecuteCancelBuild(x1, y1, x2, y2, z);
                break;
            case ACTION_WORK_LADDER:
                if (leftClick) ExecuteDesignateLadder(x1, y1, x2, y2, z);
                else ExecuteCancelBuild(x1, y1, x2, y2, z);  // Reuse cancel logic
                break;
            case ACTION_WORK_FLOOR:
                if (leftClick) ExecuteDesignateFloor(x1, y1, x2, y2, z);
                else ExecuteCancelBuild(x1, y1, x2, y2, z);  // Reuse cancel logic
                break;
            case ACTION_WORK_RAMP:
                if (leftClick) ExecuteDesignateRamp(x1, y1, x2, y2, z);
                else ExecuteCancelBuild(x1, y1, x2, y2, z);  // Reuse cancel logic
                break;
            case ACTION_WORK_FURNITURE:
                if (leftClick) ExecuteDesignateFurniture(x1, y1, x2, y2, z);
                else ExecuteCancelBuild(x1, y1, x2, y2, z);  // Reuse cancel logic
                break;
            case ACTION_WORK_WORKSHOP_CAMPFIRE:
                if (leftClick) ExecutePlaceWorkshopBlueprint(x1, y1, z, WORKSHOP_CAMPFIRE);
                else ExecuteCancelWorkshopBlueprint(x1, y1, z);
                break;
            case ACTION_WORK_WORKSHOP_DRYING_RACK:
                if (leftClick) ExecutePlaceWorkshopBlueprint(x1, y1, z, WORKSHOP_DRYING_RACK);
                else ExecuteCancelWorkshopBlueprint(x1, y1, z);
                break;
            case ACTION_WORK_WORKSHOP_ROPE_MAKER:
                if (leftClick) ExecutePlaceWorkshopBlueprint(x1, y1, z, WORKSHOP_ROPE_MAKER);
                else ExecuteCancelWorkshopBlueprint(x1, y1, z);
                break;
            case ACTION_WORK_WORKSHOP_CHARCOAL_PIT:
                if (leftClick) ExecutePlaceWorkshopBlueprint(x1, y1, z, WORKSHOP_CHARCOAL_PIT);
                else ExecuteCancelWorkshopBlueprint(x1, y1, z);
                break;
            case ACTION_WORK_WORKSHOP_HEARTH:
                if (leftClick) ExecutePlaceWorkshopBlueprint(x1, y1, z, WORKSHOP_HEARTH);
                else ExecuteCancelWorkshopBlueprint(x1, y1, z);
                break;
            case ACTION_WORK_WORKSHOP_STONECUTTER:
                if (leftClick) ExecutePlaceWorkshopBlueprint(x1, y1, z, WORKSHOP_STONECUTTER);
                else ExecuteCancelWorkshopBlueprint(x1, y1, z);
                break;
            case ACTION_WORK_WORKSHOP_SAWMILL:
                if (leftClick) ExecutePlaceWorkshopBlueprint(x1, y1, z, WORKSHOP_SAWMILL);
                else ExecuteCancelWorkshopBlueprint(x1, y1, z);
                break;
            case ACTION_WORK_WORKSHOP_KILN:
                if (leftClick) ExecutePlaceWorkshopBlueprint(x1, y1, z, WORKSHOP_KILN);
                else ExecuteCancelWorkshopBlueprint(x1, y1, z);
                break;
            case ACTION_WORK_WORKSHOP_CARPENTER:
                if (leftClick) ExecutePlaceWorkshopBlueprint(x1, y1, z, WORKSHOP_CARPENTER);
                else ExecuteCancelWorkshopBlueprint(x1, y1, z);
                break;
            case ACTION_WORK_GATHER:
                if (leftClick) ExecuteCreateGatherZone(x1, y1, x2, y2, z);
                else ExecuteEraseGatherZone(x1, y1, x2, y2, z);
                break;
            case ACTION_WORK_CHOP:
                if (leftClick) ExecuteDesignateChop(x1, y1, x2, y2, z);
                else ExecuteCancelChop(x1, y1, x2, y2, z);
                break;
            case ACTION_WORK_CHOP_FELLED:
                if (leftClick) ExecuteDesignateChopFelled(x1, y1, x2, y2, z);
                else ExecuteCancelChopFelled(x1, y1, x2, y2, z);
                break;
            case ACTION_WORK_GATHER_SAPLING:
                if (leftClick) ExecuteDesignateGatherSapling(x1, y1, x2, y2, z);
                else ExecuteCancelGatherSapling(x1, y1, x2, y2, z);
                break;
            case ACTION_WORK_GATHER_GRASS:
                if (leftClick) ExecuteDesignateGatherGrass(x1, y1, x2, y2, z);
                else ExecuteCancelGatherGrass(x1, y1, x2, y2, z);
                break;
            case ACTION_WORK_GATHER_TREE:
                if (leftClick) ExecuteDesignateGatherTree(x1, y1, x2, y2, z);
                else ExecuteCancelGatherTree(x1, y1, x2, y2, z);
                break;
            case ACTION_WORK_PLANT_SAPLING:
                if (leftClick) ExecuteDesignatePlantSapling(x1, y1, x2, y2, z);
                else ExecuteCancelPlantSapling(x1, y1, x2, y2, z);
                break;
            case ACTION_WORK_CLEAN:
                if (leftClick) ExecuteDesignateClean(x1, y1, x2, y2, z);
                else ExecuteCancelClean(x1, y1, x2, y2, z);
                break;
            case ACTION_WORK_HARVEST_BERRY:
                if (leftClick) ExecuteDesignateHarvestBerry(x1, y1, x2, y2, z);
                else ExecuteCancelHarvestBerry(x1, y1, x2, y2, z);
                break;
            case ACTION_WORK_KNAP:
                if (leftClick) ExecuteDesignateKnap(x1, y1, x2, y2, z);
                else ExecuteCancelKnap(x1, y1, x2, y2, z);
                break;
            // Sandbox actions
            case ACTION_SANDBOX_WATER:
                if (leftClick) ExecutePlaceWater(x1, y1, x2, y2, z, shift);
                else ExecuteRemoveWater(x1, y1, x2, y2, z, shift);
                break;
            case ACTION_SANDBOX_FIRE:
                if (leftClick) ExecutePlaceFire(x1, y1, x2, y2, z, shift);
                else ExecuteRemoveFire(x1, y1, x2, y2, z, shift);
                break;
            case ACTION_SANDBOX_HEAT:
                if (leftClick) ExecutePlaceHeat(x1, y1, x2, y2, z);
                else ExecuteRemoveHeat(x1, y1, x2, y2, z);
                break;
            case ACTION_SANDBOX_COLD:
                if (leftClick) ExecutePlaceCold(x1, y1, x2, y2, z);
                else ExecuteRemoveCold(x1, y1, x2, y2, z);
                break;
            case ACTION_SANDBOX_SMOKE:
                if (leftClick) ExecutePlaceSmoke(x1, y1, x2, y2, z);
                else ExecuteRemoveSmoke(x1, y1, x2, y2, z);
                break;
            case ACTION_SANDBOX_STEAM:
                if (leftClick) ExecutePlaceSteam(x1, y1, x2, y2, z);
                else ExecuteRemoveSteam(x1, y1, x2, y2, z);
                break;
            case ACTION_SANDBOX_GRASS:
                if (leftClick) ExecutePlaceGrass(x1, y1, x2, y2, z);
                else ExecuteRemoveGrass(x1, y1, x2, y2, z);
                break;
            case ACTION_SANDBOX_TREE:
                if (leftClick) ExecutePlaceTree(x1, y1, z);  // Single click for tree placement
                else ExecuteRemoveTree(x1, y1, x2, y2, z);
                break;
            case ACTION_SANDBOX_SCULPT:
            case ACTION_SANDBOX_LOWER:
            case ACTION_SANDBOX_RAISE:
                // Brush sculpting handled in continuous stroke mode above
                break;
            case ACTION_SANDBOX_LIGHT:
                if (leftClick) {
                    AddLightSource(x1, y1, z,
                        (uint8_t)lightDefaultR, (uint8_t)lightDefaultG, (uint8_t)lightDefaultB,
                        (uint8_t)lightDefaultIntensity);
                } else {
                    RemoveLightSource(x1, y1, z);
                }
                break;
            case ACTION_SANDBOX_BUSH:
                if (leftClick) ExecutePlaceBush(x1, y1, x2, y2, z);
                else ExecuteRemoveBush(x1, y1, x2, y2, z);
                break;
            case ACTION_DRAW_TRACK:
                if (leftClick) ExecutePlaceTrack(x1, y1, x2, y2, z);
                else ExecuteRemoveTrack(x1, y1, x2, y2, z);
                break;
            case ACTION_DRAW_TRAIN:
                if (leftClick) {
                    // Spawn train on track cell
                    if (grid[z][(int)mouseGrid.y][(int)mouseGrid.x] == CELL_TRACK) {
                        int idx = SpawnTrain((int)mouseGrid.x, (int)mouseGrid.y, z);
                        if (idx >= 0) {
                            AddMessage(TextFormat("Spawned train #%d", idx), GREEN);
                        } else {
                            AddMessage("Max trains reached!", RED);
                        }
                    } else {
                        AddMessage("Must place train on track", RED);
                    }
                } else {
                    // Remove nearest train at clicked cell
                    int removeIdx = -1;
                    for (int i = 0; i < MAX_TRAINS; i++) {
                        if (!trains[i].active) continue;
                        if (trains[i].cellX == (int)mouseGrid.x && trains[i].cellY == (int)mouseGrid.y && trains[i].z == z) {
                            removeIdx = i;
                            break;
                        }
                    }
                    if (removeIdx >= 0) {
                        if (trains[removeIdx].lightCellX >= 0) {
                            RemoveLightSource(trains[removeIdx].lightCellX, trains[removeIdx].lightCellY, trains[removeIdx].z);
                        }
                        trains[removeIdx].active = false;
                        trainCount--;
                        AddMessage(TextFormat("Removed train #%d", removeIdx), ORANGE);
                    }
                }
                break;
            default:
                break;
        }
    }
}
