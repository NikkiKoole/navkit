// core/input.c - Input handling with hierarchical mode system
#include "../game_state.h"
#include "../world/cell_defs.h"
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
    CellType wallType = (selectedMaterial == 2) ? CELL_WOOD_WALL : CELL_WALL;
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (grid[z][dy][dx] != wallType) {
                grid[z][dy][dx] = wallType;
                MarkChunkDirty(dx, dy, z);
                SetWaterLevel(dx, dy, z, 0);
                SetWaterSource(dx, dy, z, false);
                SetWaterDrain(dx, dy, z, false);
                DestabilizeWater(dx, dy, z);
                // Mark movers for replanning
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
                count++;
            }
        }
    }
    if (count > 0) {
        const char* matName = (selectedMaterial == 2) ? "wood" : "stone";
        AddMessage(TextFormat("Placed %d %s wall%s", count, matName, count > 1 ? "s" : ""), GREEN);
    }
}

static void ExecuteBuildFloor(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (grid[z][dy][dx] != CELL_FLOOR) {
                grid[z][dy][dx] = CELL_FLOOR;
                MarkChunkDirty(dx, dy, z);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Placed %d floor%s", count, count > 1 ? "s" : ""), GREEN);
    }
}

static void ExecuteBuildLadder(int x1, int y1, int x2, int y2, int z) {
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            PlaceLadder(dx, dy, z);
        }
    }
}

static void ExecuteBuildRoom(int x1, int y1, int x2, int y2, int z) {
    CellType wallType = (selectedMaterial == 2) ? CELL_WOOD_WALL : CELL_WALL;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            bool isBorder = (dx == x1 || dx == x2 || dy == y1 || dy == y2);
            grid[z][dy][dx] = isBorder ? wallType : CELL_FLOOR;
            MarkChunkDirty(dx, dy, z);
        }
    }
    const char* matName = (selectedMaterial == 2) ? "wood" : "stone";
    AddMessage(TextFormat("Built %s room %dx%d", matName, x2 - x1 + 1, y2 - y1 + 1), GREEN);
}

static void ExecuteErase(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (IsLadderCell(grid[z][dy][dx])) {
                EraseLadder(dx, dy, z);
                count++;
            } else {
                CellType eraseType = (z > 0) ? CELL_AIR : CELL_WALKABLE;
                if (grid[z][dy][dx] != eraseType) {
                    grid[z][dy][dx] = eraseType;
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
            if (DesignateDig(dx, dy, z)) count++;
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
            if (HasDigDesignation(dx, dy, z)) {
                CancelDesignation(dx, dy, z);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Cancelled %d mining designation%s", count, count > 1 ? "s" : ""), ORANGE);
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

static void ExecuteUnburn(int x1, int y1, int x2, int y2, int z) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (HAS_CELL_FLAG(dx, dy, z, CELL_FLAG_BURNED)) {
                CLEAR_CELL_FLAG(dx, dy, z, CELL_FLAG_BURNED);
                count++;
            }
        }
    }
    if (count > 0) {
        AddMessage(TextFormat("Unburned %d cell%s", count, count > 1 ? "s" : ""), GREEN);
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
            if (shift) {
                SetFireSource(dx, dy, z, true);
                count++;
            } else {
                if (GetBaseFuelForCellType(grid[z][dy][dx]) > 0 && !HAS_CELL_FLAG(dx, dy, z, CELL_FLAG_BURNED)) {
                    IgniteCell(dx, dy, z);
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
            FireCell* cell = &fireGrid[z][dy][dx];
            if (cell->isSource) { SetFireSource(dx, dy, z, false); removedSources++; }
            if (!shift && cell->level > 0) { ExtinguishCell(dx, dy, z); extinguished++; }
        }
    }
    if (removedSources > 0) AddMessage(TextFormat("Removed %d fire source%s", removedSources, removedSources > 1 ? "s" : ""), ORANGE);
    if (extinguished > 0) AddMessage(TextFormat("Extinguished %d cell%s", extinguished, extinguished > 1 ? "s" : ""), GRAY);
}

static void ExecutePlaceHeat(int x1, int y1, int x2, int y2, int z, bool shift) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (shift) {
                TempCell* cell = &temperatureGrid[z][dy][dx];
                if (cell->isHeatSource) { SetHeatSource(dx, dy, z, false); count++; }
            } else {
                SetHeatSource(dx, dy, z, true);
                count++;
            }
        }
    }
    if (count > 0) {
        if (shift) AddMessage(TextFormat("Removed %d heat source%s", count, count > 1 ? "s" : ""), ORANGE);
        else AddMessage(TextFormat("Placed %d heat source%s", count, count > 1 ? "s" : ""), RED);
    }
}

static void ExecutePlaceCold(int x1, int y1, int x2, int y2, int z, bool shift) {
    int count = 0;
    for (int dy = y1; dy <= y2; dy++) {
        for (int dx = x1; dx <= x2; dx++) {
            if (shift) {
                TempCell* cell = &temperatureGrid[z][dy][dx];
                if (cell->isColdSource) { SetColdSource(dx, dy, z, false); count++; }
            } else {
                SetColdSource(dx, dy, z, true);
                count++;
            }
        }
    }
    if (count > 0) {
        if (shift) AddMessage(TextFormat("Removed %d cold source%s", count, count > 1 ? "s" : ""), ORANGE);
        else AddMessage(TextFormat("Placed %d cold source%s", count, count > 1 ? "s" : ""), SKYBLUE);
    }
}

// ============================================================================
// Main Input Handler
// ============================================================================

void HandleInput(void) {
    Vector2 mouseGrid = ScreenToGrid(GetMousePosition());
    int z = currentViewZ;
    
    // Update hover states
    hoveredStockpile = GetStockpileAtGrid((int)mouseGrid.x, (int)mouseGrid.y, z);
    if (paused) {
        Vector2 mouseWorld = ScreenToWorld(GetMousePosition());
        hoveredMover = GetMoverAtWorldPos(mouseWorld.x, mouseWorld.y, z);
        hoveredItemCount = GetItemsAtCell((int)mouseGrid.x, (int)mouseGrid.y, z, hoveredItemCell, 16);
    } else {
        hoveredMover = -1;
        hoveredItemCount = 0;
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
        // Filter toggles - only when not in a mode that uses these keys
        if (inputMode == MODE_NORMAL || inputMode == MODE_ZONES) {
            if (IsKeyPressed(KEY_R)) { sp->allowedTypes[ITEM_RED] = !sp->allowedTypes[ITEM_RED]; AddMessage(TextFormat("Red: %s", sp->allowedTypes[ITEM_RED] ? "ON" : "OFF"), RED); return; }
            if (IsKeyPressed(KEY_G)) { sp->allowedTypes[ITEM_GREEN] = !sp->allowedTypes[ITEM_GREEN]; AddMessage(TextFormat("Green: %s", sp->allowedTypes[ITEM_GREEN] ? "ON" : "OFF"), GREEN); return; }
            if (IsKeyPressed(KEY_B)) { sp->allowedTypes[ITEM_BLUE] = !sp->allowedTypes[ITEM_BLUE]; AddMessage(TextFormat("Blue: %s", sp->allowedTypes[ITEM_BLUE] ? "ON" : "OFF"), BLUE); return; }
            if (IsKeyPressed(KEY_O)) { sp->allowedTypes[ITEM_ORANGE] = !sp->allowedTypes[ITEM_ORANGE]; AddMessage(TextFormat("Orange: %s", sp->allowedTypes[ITEM_ORANGE] ? "ON" : "OFF"), ORANGE); return; }
        }
    }

    // ========================================================================
    // Global controls (always active)
    // ========================================================================
    
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
        Vector2 d = GetMouseDelta();
        offset.x += d.x;
        offset.y += d.y;
    }

    // Reset view
    if (IsKeyPressed(KEY_C)) {
        zoom = 1.0f;
        offset.x = (1280 - gridWidth * CELL_SIZE * zoom) / 2.0f;
        offset.y = (800 - gridHeight * CELL_SIZE * zoom) / 2.0f;
    }

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

    // Skip grid interactions if UI wants mouse
    if (ui_wants_mouse()) return;

    // ========================================================================
    // Navigation: ESC, right-click (no drag), re-tap mode key
    // ========================================================================
    
    if (IsKeyPressed(KEY_ESCAPE)) {
        InputMode_Back();
        return;
    }



    // Re-tap mode key exits to normal
    if (inputMode == MODE_BUILD && IsKeyPressed(KEY_B)) { InputMode_ExitToNormal(); return; }
    if (inputMode == MODE_DESIGNATE && IsKeyPressed(KEY_D)) { InputMode_ExitToNormal(); return; }
    if (inputMode == MODE_ZONES && IsKeyPressed(KEY_Z)) { InputMode_ExitToNormal(); return; }
    if (inputMode == MODE_PLACE && IsKeyPressed(KEY_P)) { InputMode_ExitToNormal(); return; }
    if (inputMode == MODE_VIEW && IsKeyPressed(KEY_V)) { InputMode_ExitToNormal(); return; }

    // ========================================================================
    // Mode selection (only in normal mode)
    // ========================================================================
    
    if (inputMode == MODE_NORMAL) {
        if (IsKeyPressed(KEY_B)) { inputMode = MODE_BUILD; return; }
        if (IsKeyPressed(KEY_D)) { inputMode = MODE_DESIGNATE; return; }
        if (IsKeyPressed(KEY_Z)) { inputMode = MODE_ZONES; return; }
        if (IsKeyPressed(KEY_P)) { inputMode = MODE_PLACE; return; }
        if (IsKeyPressed(KEY_V)) { inputMode = MODE_VIEW; return; }
        return;  // Nothing else to do in normal mode
    }

    // ========================================================================
    // Action selection (in a mode, no action selected)
    // ========================================================================
    
    if (inputAction == ACTION_NONE) {
        switch (inputMode) {
            case MODE_BUILD:
                if (IsKeyPressed(KEY_W)) { inputAction = ACTION_BUILD_WALL; selectedMaterial = 1; }
                if (IsKeyPressed(KEY_F)) { inputAction = ACTION_BUILD_FLOOR; selectedMaterial = 1; }
                if (IsKeyPressed(KEY_L)) { inputAction = ACTION_BUILD_LADDER; selectedMaterial = 1; }
                if (IsKeyPressed(KEY_R)) { inputAction = ACTION_BUILD_ROOM; selectedMaterial = 1; }
                break;
            case MODE_DESIGNATE:
                if (IsKeyPressed(KEY_M)) { inputAction = ACTION_DESIGNATE_MINE; }
                if (IsKeyPressed(KEY_B)) { inputAction = ACTION_DESIGNATE_BUILD; }
                if (IsKeyPressed(KEY_U)) { inputAction = ACTION_DESIGNATE_UNBURN; }
                break;
            case MODE_ZONES:
                if (IsKeyPressed(KEY_S)) { inputAction = ACTION_ZONE_STOCKPILE; }
                if (IsKeyPressed(KEY_G)) { inputAction = ACTION_ZONE_GATHER; }
                break;
            case MODE_PLACE:
                if (IsKeyPressed(KEY_W)) { inputAction = ACTION_PLACE_WATER; }
                if (IsKeyPressed(KEY_F)) { inputAction = ACTION_PLACE_FIRE; }
                if (IsKeyPressed(KEY_H)) { inputAction = ACTION_PLACE_HEAT; }
                break;
            case MODE_VIEW:
                // TODO: Implement view toggles
                break;
            default:
                break;
        }
        return;
    }

    // ========================================================================
    // Material selection (when action is selected)
    // ========================================================================
    
    if (IsKeyPressed(KEY_ONE)) selectedMaterial = 1;
    if (IsKeyPressed(KEY_TWO)) selectedMaterial = 2;
    if (IsKeyPressed(KEY_THREE)) selectedMaterial = 3;

    // ========================================================================
    // Action execution (drag handling)
    // ========================================================================
    
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
            case ACTION_BUILD_WALL:
                if (leftClick) ExecuteBuildWall(x1, y1, x2, y2, z);
                else ExecuteErase(x1, y1, x2, y2, z);
                break;
            case ACTION_BUILD_FLOOR:
                if (leftClick) ExecuteBuildFloor(x1, y1, x2, y2, z);
                else ExecuteErase(x1, y1, x2, y2, z);
                break;
            case ACTION_BUILD_LADDER:
                if (leftClick) ExecuteBuildLadder(x1, y1, x2, y2, z);
                else ExecuteErase(x1, y1, x2, y2, z);
                break;
            case ACTION_BUILD_ROOM:
                if (leftClick) ExecuteBuildRoom(x1, y1, x2, y2, z);
                else ExecuteErase(x1, y1, x2, y2, z);
                break;
            case ACTION_DESIGNATE_MINE:
                if (leftClick) ExecuteDesignateMine(x1, y1, x2, y2, z);
                else ExecuteCancelMine(x1, y1, x2, y2, z);
                break;
            case ACTION_DESIGNATE_BUILD:
                if (leftClick) ExecuteDesignateBuild(x1, y1, x2, y2, z);
                else ExecuteCancelBuild(x1, y1, x2, y2, z);
                break;
            case ACTION_DESIGNATE_UNBURN:
                if (leftClick) ExecuteUnburn(x1, y1, x2, y2, z);
                break;
            case ACTION_ZONE_STOCKPILE:
                if (leftClick) ExecuteCreateStockpile(x1, y1, x2, y2, z);
                else ExecuteEraseStockpile(x1, y1, x2, y2, z);
                break;
            case ACTION_ZONE_GATHER:
                if (leftClick) ExecuteCreateGatherZone(x1, y1, x2, y2, z);
                else ExecuteEraseGatherZone(x1, y1, x2, y2, z);
                break;
            case ACTION_PLACE_WATER:
                if (leftClick) ExecutePlaceWater(x1, y1, x2, y2, z, shift);
                else ExecuteRemoveWater(x1, y1, x2, y2, z, shift);
                break;
            case ACTION_PLACE_FIRE:
                if (leftClick) ExecutePlaceFire(x1, y1, x2, y2, z, shift);
                else ExecuteRemoveFire(x1, y1, x2, y2, z, shift);
                break;
            case ACTION_PLACE_HEAT:
                if (leftClick) ExecutePlaceHeat(x1, y1, x2, y2, z, shift);
                else ExecutePlaceCold(x1, y1, x2, y2, z, shift);
                break;
            default:
                break;
        }
    }
}
