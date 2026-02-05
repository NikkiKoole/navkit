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
#include "input_mode.h"

// Forward declarations
bool SaveWorld(const char* filename);
bool LoadWorld(const char* filename);

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

static void ExecuteBuildWall(int x1, int y1, int x2, int y2, int z) {
    bool isDirt = (selectedMaterial == 3);
    MaterialType mat = isDirt ? MAT_DIRT : (selectedMaterial == 2) ? MAT_WOOD : MAT_STONE;
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
                // Dirt creates natural CELL_DIRT terrain
                if (grid[z][dy][dx] != CELL_DIRT) {
                    grid[z][dy][dx] = CELL_DIRT;
                    SetWallMaterial(dx, dy, z, MAT_RAW);  // Natural terrain
                    CLEAR_FLOOR(dx, dy, z);
                    SetFloorMaterial(dx, dy, z, MAT_NONE);
                    SET_CELL_SURFACE(dx, dy, z, SURFACE_BARE);  // No grass yet, will grow over time
                    MarkChunkDirty(dx, dy, z);
                    CLEAR_CELL_FLAG(dx, dy, z, CELL_FLAG_BURNED);
                    SetWaterLevel(dx, dy, z, 0);
                    SetWaterSource(dx, dy, z, false);
                    SetWaterDrain(dx, dy, z, false);
                    DestabilizeWater(dx, dy, z);
                    count++;
                }
            } else {
                // Normal wall with material
                if (grid[z][dy][dx] != CELL_WALL || GetWallMaterial(dx, dy, z) != mat) {
                    grid[z][dy][dx] = CELL_WALL;
                    SetWallMaterial(dx, dy, z, mat);
                    MarkChunkDirty(dx, dy, z);
                    CLEAR_CELL_FLAG(dx, dy, z, CELL_FLAG_BURNED);
                    SetWaterLevel(dx, dy, z, 0);
                    SetWaterSource(dx, dy, z, false);
                    SetWaterDrain(dx, dy, z, false);
                    DestabilizeWater(dx, dy, z);
                    count++;
                }
            }
            
            // Mark movers for replanning
            if (count > 0) {
                for (int i = 0; i < moverCount; i++) {
                    Mover* m = &movers[i];
                    if (!m->active) continue;
                    for (int j = m->pathIndex; j >= 0; j--) {
                        if (m->path[j].x == dx && m->path[j].y == dy && m->path[j].z == z) {
                            m->needsRepath = true;
                            break;
                        }
                    }
                }
            }
        }
    }
    if (count > 0) {
        const char* matName = isDirt ? "dirt" : (selectedMaterial == 2) ? "wood" : "stone";
        AddMessage(TextFormat("Placed %d %s%s", count, matName, count > 1 ? " blocks" : " block"), GREEN);
    }
    if (skipped > 0) {
        AddMessage(TextFormat("Skipped %d cell%s (workshop)", skipped, skipped > 1 ? "s" : ""), ORANGE);
    }
}

static void ExecuteBuildFloor(int x1, int y1, int x2, int y2, int z) {
    MaterialType mat = (selectedMaterial == 3) ? MAT_DIRT : (selectedMaterial == 2) ? MAT_WOOD : MAT_STONE;
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
            MarkChunkDirty(dx, dy, z);
            CLEAR_CELL_FLAG(dx, dy, z, CELL_FLAG_BURNED);
        }
    }
    if (count > 0) {
        const char* matName = (selectedMaterial == 3) ? "dirt" : (selectedMaterial == 2) ? "wood" : "stone";
        AddMessage(TextFormat("Placed %d %s floor%s", count, matName, count > 1 ? "s" : ""), GREEN);
    }
    if (replaced > 0) {
        const char* matName = (selectedMaterial == 3) ? "dirt" : (selectedMaterial == 2) ? "wood" : "stone";
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

static void ExecuteBuildDirt(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            CellType cell = grid[z][dy][dx];
            // Can place dirt on air
            if (cell == CELL_AIR) {
                grid[z][dy][dx] = CELL_DIRT;
                MarkChunkDirty(dx, dy, z);
                CLEAR_CELL_FLAG(dx, dy, z, CELL_FLAG_BURNED);
                // Set tall grass overlay and reset wear
                SET_CELL_SURFACE(dx, dy, z, SURFACE_TALL_GRASS);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Placed %d dirt%s", count, count > 1 ? " tiles" : ""), GREEN);
    }
}

static void ExecuteEraseDirt(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (grid[z][dy][dx] == CELL_DIRT) {
                CellType eraseType = (z == 0) ? CELL_BEDROCK : CELL_AIR;
                grid[z][dy][dx] = eraseType;
                MarkChunkDirty(dx, dy, z);
                SET_CELL_SURFACE(dx, dy, z, SURFACE_BARE);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Erased %d dirt%s", count, count > 1 ? " tiles" : ""), ORANGE);
    }
}

static void ExecuteErase(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (IsLadderCell(grid[z][dy][dx])) {
                EraseLadder(dx, dy, z);
                count++;
            } else {
                CellType eraseType = (z == 0) ? CELL_BEDROCK : CELL_AIR;
                bool changed = false;
                if (grid[z][dy][dx] != eraseType) {
                    grid[z][dy][dx] = eraseType;
                    changed = true;
                }
                // Also clear floor flag in DF mode
                if (HAS_FLOOR(dx, dy, z)) {
                    CLEAR_FLOOR(dx, dy, z);
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

static void ExecuteDesignateBuild(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (CreateBuildBlueprint(dx, dy, z) >= 0) count++;
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Created %d blueprint%s", count, count > 1 ? "s" : ""), BLUE);
    }
}

static void ExecuteDesignateLadder(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (CreateLadderBlueprint(dx, dy, z) >= 0) count++;
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Created %d ladder blueprint%s", count, count > 1 ? "s" : ""), BLUE);
    }
}

static void ExecuteDesignateFloor(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (CreateFloorBlueprint(dx, dy, z) >= 0) count++;
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Created %d floor blueprint%s", count, count > 1 ? "s" : ""), BLUE);
    }
}

static void ExecuteDesignateRamp(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (CreateRampBlueprint(dx, dy, z) >= 0) count++;
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Created %d ramp blueprint%s", count, count > 1 ? "s" : ""), BLUE);
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

static void ExecutePlaceWorkshop(int x, int y, int z) {
    // Check if area is clear (3x3 walkable, no other workshops)
    for (int dy = 0; dy < 3; dy++) {
        for (int dx = 0; dx < 3; dx++) {
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
    
    int idx = CreateWorkshop(x, y, z, WORKSHOP_STONECUTTER);
    if (idx >= 0) {
        AddMessage(TextFormat("Built stonecutter workshop #%d", idx), GREEN);
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
            // Fire burns on the floor (z-1), not in the air (z)
            int fireZ = z;
            if (z > 0 && grid[z][dy][dx] == CELL_AIR && CellIsSolid(grid[z-1][dy][dx])) {
                fireZ = z - 1;
            }
            
            if (shift) {
                SetFireSource(dx, dy, fireZ, true);
                count++;
            } else {
                if (GetBaseFuelForCellType(grid[fireZ][dy][dx]) > 0 && !HAS_CELL_FLAG(dx, dy, fireZ, CELL_FLAG_BURNED)) {
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
            // Can grow grass on dirt or air
            if (cell == CELL_AIR) {
                // Convert to dirt first
                grid[z][dy][dx] = CELL_DIRT;
                MarkChunkDirty(dx, dy, z);
            }
            if (grid[z][dy][dx] == CELL_DIRT) {
                // Set tall grass overlay and reset wear
                SET_CELL_SURFACE(dx, dy, z, SURFACE_TALL_GRASS);
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
            // Remove grass overlay from dirt tiles
            if (grid[z][dy][dx] == CELL_DIRT) {
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

static void ExecutePlaceTree(int x, int y, int z) {
    // Check if we can place a tree here (need solid ground below or at z=0)
    CellType cell = grid[z][y][x];
    if (cell != CELL_AIR && !IsGroundCell(cell)) {
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

static void ExecuteRemoveTree(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            CellType cell = grid[z][dy][dx];
            if (cell == CELL_TREE_TRUNK || cell == CELL_TREE_LEAVES || cell == CELL_SAPLING) {
                grid[z][dy][dx] = CELL_AIR;
                treeTypeGrid[z][dy][dx] = TREE_TYPE_NONE;
                treePartGrid[z][dy][dx] = TREE_PART_NONE;
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
        hoveredItemCount = 0;
        hoveredDesignationX = -1;
        hoveredDesignationY = -1;
        hoveredDesignationZ = -1;
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
            int newSize = sp->maxStackSize + 1;
            if (newSize <= MAX_STACK_SIZE) { SetStockpileMaxStackSize(hoveredStockpile, newSize); AddMessage(TextFormat("Stack size: %d", sp->maxStackSize), WHITE); }
            return;
        }
        if (IsKeyPressed(KEY_LEFT_BRACKET)) {
            int newSize = sp->maxStackSize - 1;
            if (newSize >= 1) { SetStockpileMaxStackSize(hoveredStockpile, newSize); AddMessage(TextFormat("Stack size: %d", sp->maxStackSize), WHITE); }
            return;
        }
        // Filter toggles - only in normal mode (R/G/B/O aren't used by other modes)
        if (inputMode == MODE_NORMAL) {
            if (IsKeyPressed(KEY_R)) { sp->allowedTypes[ITEM_RED] = !sp->allowedTypes[ITEM_RED]; AddMessage(TextFormat("Red: %s", sp->allowedTypes[ITEM_RED] ? "ON" : "OFF"), RED); return; }
            if (IsKeyPressed(KEY_G)) { sp->allowedTypes[ITEM_GREEN] = !sp->allowedTypes[ITEM_GREEN]; AddMessage(TextFormat("Green: %s", sp->allowedTypes[ITEM_GREEN] ? "ON" : "OFF"), GREEN); return; }
            if (IsKeyPressed(KEY_B)) { sp->allowedTypes[ITEM_BLUE] = !sp->allowedTypes[ITEM_BLUE]; AddMessage(TextFormat("Blue: %s", sp->allowedTypes[ITEM_BLUE] ? "ON" : "OFF"), BLUE); return; }
            if (IsKeyPressed(KEY_O)) { sp->allowedTypes[ITEM_ROCK] = !sp->allowedTypes[ITEM_ROCK]; AddMessage(TextFormat("Rock: %s", sp->allowedTypes[ITEM_ROCK] ? "ON" : "OFF"), ORANGE); return; }
            if (IsKeyPressed(KEY_S)) { sp->allowedTypes[ITEM_STONE_BLOCKS] = !sp->allowedTypes[ITEM_STONE_BLOCKS]; AddMessage(TextFormat("Stone Blocks: %s", sp->allowedTypes[ITEM_STONE_BLOCKS] ? "ON" : "OFF"), GRAY); return; }
            if (IsKeyPressed(KEY_W)) { sp->allowedTypes[ITEM_WOOD] = !sp->allowedTypes[ITEM_WOOD]; AddMessage(TextFormat("Wood: %s", sp->allowedTypes[ITEM_WOOD] ? "ON" : "OFF"), BROWN); return; }
            if (IsKeyPressed(KEY_T)) {
                bool anySapling = sp->allowedTypes[ITEM_SAPLING_OAK] || sp->allowedTypes[ITEM_SAPLING_PINE] ||
                                  sp->allowedTypes[ITEM_SAPLING_BIRCH] || sp->allowedTypes[ITEM_SAPLING_WILLOW];
                bool newVal = !anySapling;
                sp->allowedTypes[ITEM_SAPLING_OAK] = newVal;
                sp->allowedTypes[ITEM_SAPLING_PINE] = newVal;
                sp->allowedTypes[ITEM_SAPLING_BIRCH] = newVal;
                sp->allowedTypes[ITEM_SAPLING_WILLOW] = newVal;
                AddMessage(TextFormat("Saplings: %s", newVal ? "ON" : "OFF"), GREEN);
                return;
            }
            if (IsKeyPressed(KEY_D)) { sp->allowedTypes[ITEM_DIRT] = !sp->allowedTypes[ITEM_DIRT]; AddMessage(TextFormat("Dirt: %s", sp->allowedTypes[ITEM_DIRT] ? "ON" : "OFF"), BROWN); return; }
        }
    }

    // ========================================================================
    // Workshop hover controls (always active when hovering)
    // ========================================================================
    if (hoveredWorkshop >= 0 && inputMode == MODE_NORMAL) {
        Workshop* ws = &workshops[hoveredWorkshop];
        
        // B = Add bill (Cut Stone Blocks, Do Forever)
        if (IsKeyPressed(KEY_B)) {
            if (ws->billCount < MAX_BILLS_PER_WORKSHOP) {
                AddBill(hoveredWorkshop, 0, BILL_DO_FOREVER, 0);
                AddMessage(TextFormat("Added bill: Cut Stone Blocks (Do Forever)"), GREEN);
            } else {
                AddMessage(TextFormat("Workshop has max bills (%d)", MAX_BILLS_PER_WORKSHOP), RED);
            }
            return;
        }
        
        // X = Remove last bill
        if (IsKeyPressed(KEY_X)) {
            if (ws->billCount > 0) {
                ws->billCount--;
                AddMessage(TextFormat("Removed bill (now %d)", ws->billCount), ORANGE);
            } else {
                AddMessage("No bills to remove", RED);
            }
            return;
        }
        
        // P = Toggle pause on first bill
        if (IsKeyPressed(KEY_P)) {
            if (ws->billCount > 0) {
                ws->bills[0].suspended = !ws->bills[0].suspended;
                AddMessage(TextFormat("Bill %s", ws->bills[0].suspended ? "PAUSED" : "RESUMED"), 
                    ws->bills[0].suspended ? RED : GREEN);
            }
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
            snprintf(cmd, sizeof(cmd), "gzip -c saves/debug_save.bin > saves/%04d-%02d-%02d_%02d-%02d-%02d.bin.gz",
                t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
            system(cmd);
        }
    }
    if (IsKeyPressed(KEY_F6)) {
        if (LoadWorld("saves/debug_save.bin")) AddMessage("World loaded", GREEN);
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
            // Back out of menus
            InputMode_Back();
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



    // Re-tap mode key exits to normal
    if (inputMode == MODE_DRAW && CheckKey(KEY_D)) { InputMode_ExitToNormal(); return; }
    if (inputMode == MODE_WORK && CheckKey(KEY_W)) { InputMode_ExitToNormal(); return; }
    if (inputMode == MODE_SANDBOX && CheckKey(KEY_S)) { InputMode_ExitToNormal(); return; }

    // ========================================================================
    // Mode selection (only in normal mode)
    // ========================================================================
    
    if (inputMode == MODE_NORMAL) {
        if (CheckKey(KEY_D)) { inputMode = MODE_DRAW; return; }
        if (CheckKey(KEY_W)) { inputMode = MODE_WORK; return; }
        if (CheckKey(KEY_S)) { inputMode = MODE_SANDBOX; return; }
        
        // Skip grid interactions if UI wants mouse
        if (ui_wants_mouse()) return;
        
        // Quick edit: left-click = wall, right-click = erase (when enabled)
        if (quickEditEnabled) {
            Vector2 gp = ScreenToGrid(GetMousePosition());
            int x = (int)gp.x, y = (int)gp.y;
            if (x >= 0 && x < gridWidth && y >= 0 && y < gridHeight) {
                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                    // Don't allow walls on workshop tiles
                    if (IsWorkshopTile(x, y, z)) {
                        // Skip silently in quick-edit mode
                    } else {
                        MaterialType mat = IsKeyDown(KEY_TWO) ? MAT_WOOD : MAT_STONE;
                        if (grid[z][y][x] != CELL_WALL || GetWallMaterial(x, y, z) != mat) {
                            DisplaceWater(x, y, z);
                            grid[z][y][x] = CELL_WALL;
                            SetWallMaterial(x, y, z, mat);
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
                if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
                    if (IsLadderCell(grid[z][y][x])) {
                        EraseLadder(x, y, z);
                    } else {
                        if (grid[z][y][x] != CELL_AIR) {
                            grid[z][y][x] = CELL_AIR;
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
        switch (inputMode) {
            case MODE_DRAW:
                if (CheckKey(KEY_W)) { inputAction = ACTION_DRAW_WALL; selectedMaterial = 1; }
                if (CheckKey(KEY_F)) { inputAction = ACTION_DRAW_FLOOR; selectedMaterial = 1; }
                if (CheckKey(KEY_L)) { inputAction = ACTION_DRAW_LADDER; selectedMaterial = 1; }
                if (CheckKey(KEY_R)) { inputAction = ACTION_DRAW_RAMP; }
                if (CheckKey(KEY_S)) { inputAction = ACTION_DRAW_STOCKPILE; }
                if (CheckKey(KEY_I)) { inputAction = ACTION_DRAW_DIRT; }
                if (CheckKey(KEY_T)) { inputAction = ACTION_DRAW_WORKSHOP; }
                break;
            case MODE_WORK:
                if (workSubMode == SUBMODE_NONE) {
                    // Top-level WORK menu - select submode or Gather
                    if (CheckKey(KEY_D)) { workSubMode = SUBMODE_DIG; }
                    if (CheckKey(KEY_B)) { workSubMode = SUBMODE_BUILD; }
                    if (CheckKey(KEY_H)) { workSubMode = SUBMODE_HARVEST; }
                    if (CheckKey(KEY_G)) { inputAction = ACTION_WORK_GATHER; }
                } else {
                    // In a submode - select action
                    switch (workSubMode) {
                        case SUBMODE_DIG:
                            if (CheckKey(KEY_M)) { inputAction = ACTION_WORK_MINE; }
                            if (CheckKey(KEY_H)) { inputAction = ACTION_WORK_CHANNEL; }
                            if (CheckKey(KEY_R)) { inputAction = ACTION_WORK_DIG_RAMP; }
                            if (CheckKey(KEY_F)) { inputAction = ACTION_WORK_REMOVE_FLOOR; }
                            if (CheckKey(KEY_Z)) { inputAction = ACTION_WORK_REMOVE_RAMP; }
                            break;
                        case SUBMODE_BUILD:
                            if (CheckKey(KEY_W)) { inputAction = ACTION_WORK_CONSTRUCT; }
                            if (CheckKey(KEY_F)) { inputAction = ACTION_WORK_FLOOR; }
                            if (CheckKey(KEY_L)) { inputAction = ACTION_WORK_LADDER; }
                            if (CheckKey(KEY_R)) { inputAction = ACTION_WORK_RAMP; }
                            break;
                        case SUBMODE_HARVEST:
                            if (CheckKey(KEY_C)) { inputAction = ACTION_WORK_CHOP; }
                            if (CheckKey(KEY_F)) { inputAction = ACTION_WORK_CHOP_FELLED; }
                            if (CheckKey(KEY_S)) { inputAction = ACTION_WORK_GATHER_SAPLING; }
                            if (CheckKey(KEY_P)) { inputAction = ACTION_WORK_PLANT_SAPLING; }
                            break;
                        default:
                            break;
                    }
                }
                break;
            case MODE_SANDBOX:
                if (CheckKey(KEY_W)) { inputAction = ACTION_SANDBOX_WATER; }
                if (CheckKey(KEY_F)) { inputAction = ACTION_SANDBOX_FIRE; }
                if (CheckKey(KEY_H)) { inputAction = ACTION_SANDBOX_HEAT; }
                if (CheckKey(KEY_C)) { inputAction = ACTION_SANDBOX_COLD; }
                if (CheckKey(KEY_M)) { inputAction = ACTION_SANDBOX_SMOKE; }
                if (CheckKey(KEY_T)) { inputAction = ACTION_SANDBOX_STEAM; }
                if (CheckKey(KEY_G)) { inputAction = ACTION_SANDBOX_GRASS; }
                if (CheckKey(KEY_R)) { inputAction = ACTION_SANDBOX_TREE; }
                break;
            default:
                break;
        }
        return;
    }

    // ========================================================================
    // Re-tap action key to go back one level
    // ========================================================================
    
    bool backOneLevel = false;
    switch (inputAction) {
        // Draw actions
        case ACTION_DRAW_WALL:      backOneLevel = CheckKey(KEY_W); break;
        case ACTION_DRAW_FLOOR:     backOneLevel = CheckKey(KEY_F); break;
        case ACTION_DRAW_LADDER:    backOneLevel = CheckKey(KEY_L); break;
        case ACTION_DRAW_RAMP:      backOneLevel = CheckKey(KEY_R); break;
        case ACTION_DRAW_STOCKPILE: backOneLevel = CheckKey(KEY_S); break;
        case ACTION_DRAW_DIRT:      backOneLevel = CheckKey(KEY_I); break;
        case ACTION_DRAW_WORKSHOP:  backOneLevel = CheckKey(KEY_T); break;
        // Dig actions
        case ACTION_WORK_MINE:         backOneLevel = CheckKey(KEY_M); break;
        case ACTION_WORK_CHANNEL:      backOneLevel = CheckKey(KEY_H); break;
        case ACTION_WORK_DIG_RAMP:     backOneLevel = CheckKey(KEY_R); break;
        case ACTION_WORK_REMOVE_FLOOR: backOneLevel = CheckKey(KEY_F); break;
        case ACTION_WORK_REMOVE_RAMP:  backOneLevel = CheckKey(KEY_Z); break;
        // Build actions
        case ACTION_WORK_CONSTRUCT:    backOneLevel = CheckKey(KEY_W); break;
        case ACTION_WORK_FLOOR:        backOneLevel = CheckKey(KEY_F); break;
        case ACTION_WORK_LADDER:       backOneLevel = CheckKey(KEY_L); break;
        case ACTION_WORK_RAMP:         backOneLevel = CheckKey(KEY_R); break;
        // Harvest actions
        case ACTION_WORK_CHOP:         backOneLevel = CheckKey(KEY_C); break;
        case ACTION_WORK_CHOP_FELLED:  backOneLevel = CheckKey(KEY_F); break;
        case ACTION_WORK_GATHER_SAPLING: backOneLevel = CheckKey(KEY_S); break;
        case ACTION_WORK_PLANT_SAPLING:  backOneLevel = CheckKey(KEY_P); break;
        // Gather (top-level)
        case ACTION_WORK_GATHER:       backOneLevel = CheckKey(KEY_G); break;
        // Sandbox actions
        case ACTION_SANDBOX_WATER:  backOneLevel = CheckKey(KEY_W); break;
        case ACTION_SANDBOX_FIRE:   backOneLevel = CheckKey(KEY_F); break;
        case ACTION_SANDBOX_HEAT:   backOneLevel = CheckKey(KEY_H); break;
        case ACTION_SANDBOX_COLD:   backOneLevel = CheckKey(KEY_C); break;
        case ACTION_SANDBOX_SMOKE:  backOneLevel = CheckKey(KEY_M); break;
        case ACTION_SANDBOX_STEAM:  backOneLevel = CheckKey(KEY_T); break;
        case ACTION_SANDBOX_GRASS:  backOneLevel = CheckKey(KEY_G); break;
        case ACTION_SANDBOX_TREE:   backOneLevel = CheckKey(KEY_R); break;
        default: break;
    }
    if (backOneLevel) {
        InputMode_Back();
        return;
    }
    
    // Check if pressing the submode key to go back to submode selection
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
            case ACTION_DRAW_DIRT:
                if (leftClick) ExecuteBuildDirt(x1, y1, x2, y2, z);
                else ExecuteEraseDirt(x1, y1, x2, y2, z);
                break;
            case ACTION_DRAW_WORKSHOP:
                if (leftClick) ExecutePlaceWorkshop(dragStartX, dragStartY, z);
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
            case ACTION_WORK_PLANT_SAPLING:
                if (leftClick) ExecuteDesignatePlantSapling(x1, y1, x2, y2, z);
                else ExecuteCancelPlantSapling(x1, y1, x2, y2, z);
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
            default:
                break;
        }
    }
}
