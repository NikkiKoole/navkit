// core/input.c - Input handling
#include "../game_state.h"

// Forward declarations
bool SaveWorld(const char* filename);
bool LoadWorld(const char* filename);

void HandleInput(void) {
    // Update stockpile hover state
    Vector2 mouseGrid = ScreenToGrid(GetMousePosition());
    hoveredStockpile = GetStockpileAtGrid((int)mouseGrid.x, (int)mouseGrid.y, currentViewZ);

    // Update mover hover state (only when paused, for debugging)
    if (paused) {
        Vector2 mouseWorld = ScreenToWorld(GetMousePosition());
        hoveredMover = GetMoverAtWorldPos(mouseWorld.x, mouseWorld.y, currentViewZ);
    } else {
        hoveredMover = -1;
    }

    // Update item hover state (only when paused)
    if (paused) {
        int cellX = (int)mouseGrid.x;
        int cellY = (int)mouseGrid.y;
        hoveredItemCount = GetItemsAtCell(cellX, cellY, currentViewZ, hoveredItemCell, 16);
    } else {
        hoveredItemCount = 0;
    }

    // Stockpile editing controls (when hovering)
    if (hoveredStockpile >= 0) {
        Stockpile* sp = &stockpiles[hoveredStockpile];

        // Priority: +/- or =/- keys
        if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) {
            if (sp->priority < 9) {
                sp->priority++;
                AddMessage(TextFormat("Stockpile priority: %d", sp->priority), WHITE);
            }
        }
        if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) {
            if (sp->priority > 1) {
                sp->priority--;
                AddMessage(TextFormat("Stockpile priority: %d", sp->priority), WHITE);
            }
        }

        // Stack size: [ / ] keys
        if (IsKeyPressed(KEY_RIGHT_BRACKET)) {
            int newSize = sp->maxStackSize + 1;
            if (newSize <= MAX_STACK_SIZE) {
                SetStockpileMaxStackSize(hoveredStockpile, newSize);
                AddMessage(TextFormat("Stack size: %d", sp->maxStackSize), WHITE);
            }
            return;  // Don't trigger Z-layer change
        }
        if (IsKeyPressed(KEY_LEFT_BRACKET)) {
            int newSize = sp->maxStackSize - 1;
            if (newSize >= 1) {
                SetStockpileMaxStackSize(hoveredStockpile, newSize);
                AddMessage(TextFormat("Stack size: %d", sp->maxStackSize), WHITE);
            }
            return;  // Don't trigger Z-layer change
        }

        // Filter toggles: R/G/B keys
        if (IsKeyPressed(KEY_R)) {
            sp->allowedTypes[ITEM_RED] = !sp->allowedTypes[ITEM_RED];
            AddMessage(TextFormat("Red filter: %s", sp->allowedTypes[ITEM_RED] ? "ON" : "OFF"), RED);
            return;  // Don't trigger room drawing
        }
        if (IsKeyPressed(KEY_G)) {
            sp->allowedTypes[ITEM_GREEN] = !sp->allowedTypes[ITEM_GREEN];
            AddMessage(TextFormat("Green filter: %s", sp->allowedTypes[ITEM_GREEN] ? "ON" : "OFF"), GREEN);
        }
        if (IsKeyPressed(KEY_B)) {
            sp->allowedTypes[ITEM_BLUE] = !sp->allowedTypes[ITEM_BLUE];
            AddMessage(TextFormat("Blue filter: %s", sp->allowedTypes[ITEM_BLUE] ? "ON" : "OFF"), BLUE);
        }
        if (IsKeyPressed(KEY_O)) {
            sp->allowedTypes[ITEM_ORANGE] = !sp->allowedTypes[ITEM_ORANGE];
            AddMessage(TextFormat("Orange filter: %s", sp->allowedTypes[ITEM_ORANGE] ? "ON" : "OFF"), ORANGE);
        }
    }

    // Zoom with mouse wheel
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

    // Pan with middle mouse
    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
        Vector2 d = GetMouseDelta();
        offset.x += d.x;
        offset.y += d.y;
    }

    // Skip grid interactions if UI wants mouse
    if (ui_wants_mouse()) return;

    // Room drawing mode (R key + drag) - check before normal tools
    if (IsKeyDown(KEY_R)) {
        Vector2 gp = ScreenToGrid(GetMousePosition());
        int x = (int)gp.x, y = (int)gp.y;
        int z = currentViewZ;

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            drawingRoom = true;
            roomStartX = x;
            roomStartY = y;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && drawingRoom) {
            drawingRoom = false;
            int x1 = roomStartX < x ? roomStartX : x;
            int y1 = roomStartY < y ? roomStartY : y;
            int x2 = roomStartX > x ? roomStartX : x;
            int y2 = roomStartY > y ? roomStartY : y;

            if (x1 < 0) x1 = 0;
            if (y1 < 0) y1 = 0;
            if (x2 >= gridWidth) x2 = gridWidth - 1;
            if (y2 >= gridHeight) y2 = gridHeight - 1;

            // R + 2 = wood walls, R alone = stone walls
            CellType wallType = IsKeyDown(KEY_TWO) ? CELL_WOOD_WALL : CELL_WALL;
            
            for (int ry = y1; ry <= y2; ry++) {
                for (int rx = x1; rx <= x2; rx++) {
                    bool isBorder = (rx == x1 || rx == x2 || ry == y1 || ry == y2);
                    grid[z][ry][rx] = isBorder ? wallType : CELL_FLOOR;
                    MarkChunkDirty(rx, ry, z);
                }
            }
        }
        return;  // Skip normal tool interactions while R is held
    } else {
        drawingRoom = false;
    }

    // Floor/Tile drawing mode (T key + drag) - fills entire area with floor
    if (IsKeyDown(KEY_T)) {
        Vector2 gp = ScreenToGrid(GetMousePosition());
        int x = (int)gp.x, y = (int)gp.y;
        int z = currentViewZ;

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            drawingFloor = true;
            floorStartX = x;
            floorStartY = y;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && drawingFloor) {
            drawingFloor = false;
            int x1 = floorStartX < x ? floorStartX : x;
            int y1 = floorStartY < y ? floorStartY : y;
            int x2 = floorStartX > x ? floorStartX : x;
            int y2 = floorStartY > y ? floorStartY : y;

            if (x1 < 0) x1 = 0;
            if (y1 < 0) y1 = 0;
            if (x2 >= gridWidth) x2 = gridWidth - 1;
            if (y2 >= gridHeight) y2 = gridHeight - 1;

            for (int ry = y1; ry <= y2; ry++) {
                for (int rx = x1; rx <= x2; rx++) {
                    grid[z][ry][rx] = CELL_FLOOR;
                    MarkChunkDirty(rx, ry, z);
                }
            }
        }
        return;  // Skip normal tool interactions while F is held
    } else {
        drawingFloor = false;
    }

    // Stockpile drawing mode (S key + left-drag to draw, right-drag to erase)
    if (IsKeyDown(KEY_S)) {
        Vector2 gp = ScreenToGrid(GetMousePosition());
        int x = (int)gp.x, y = (int)gp.y;
        int z = currentViewZ;

        // Left mouse - draw stockpile
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            drawingStockpile = true;
            stockpileStartX = x;
            stockpileStartY = y;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && drawingStockpile) {
            drawingStockpile = false;
            int x1 = stockpileStartX < x ? stockpileStartX : x;
            int y1 = stockpileStartY < y ? stockpileStartY : y;
            int x2 = stockpileStartX > x ? stockpileStartX : x;
            int y2 = stockpileStartY > y ? stockpileStartY : y;

            if (x1 < 0) x1 = 0;
            if (y1 < 0) y1 = 0;
            if (x2 >= gridWidth) x2 = gridWidth - 1;
            if (y2 >= gridHeight) y2 = gridHeight - 1;

            int width = x2 - x1 + 1;
            int height = y2 - y1 + 1;

            // Clamp to MAX_STOCKPILE_SIZE
            if (width > MAX_STOCKPILE_SIZE) width = MAX_STOCKPILE_SIZE;
            if (height > MAX_STOCKPILE_SIZE) height = MAX_STOCKPILE_SIZE;

            if (width > 0 && height > 0) {
                // Remove overlapping cells from existing stockpiles first
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

        // Right mouse - erase stockpiles in area
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            erasingStockpile = true;
            stockpileStartX = x;
            stockpileStartY = y;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT) && erasingStockpile) {
            erasingStockpile = false;
            int x1 = stockpileStartX < x ? stockpileStartX : x;
            int y1 = stockpileStartY < y ? stockpileStartY : y;
            int x2 = stockpileStartX > x ? stockpileStartX : x;
            int y2 = stockpileStartY > y ? stockpileStartY : y;

            // Remove cells from any stockpiles that overlap with the erase area
            int erasedCells = 0;
            for (int i = MAX_STOCKPILES - 1; i >= 0; i--) {
                if (!stockpiles[i].active || stockpiles[i].z != z) continue;

                int sx1 = stockpiles[i].x;
                int sy1 = stockpiles[i].y;
                int sx2 = sx1 + stockpiles[i].width - 1;
                int sy2 = sy1 + stockpiles[i].height - 1;

                // Check overlap
                if (x1 <= sx2 && x2 >= sx1 && y1 <= sy2 && y2 >= sy1) {
                    int beforeCount = GetStockpileActiveCellCount(i);
                    RemoveStockpileCells(i, x1, y1, x2, y2);
                    int afterCount = GetStockpileActiveCellCount(i);
                    erasedCells += beforeCount - afterCount;
                }
            }
            if (erasedCells > 0) {
                AddMessage(TextFormat("Erased %d cell%s", erasedCells, erasedCells > 1 ? "s" : ""), ORANGE);
            }
        }

        return;  // Skip normal tool interactions while S is held
    } else {
        drawingStockpile = false;
        erasingStockpile = false;
    }

    // Gather zone mode (G key + left-drag to draw, right-drag to erase)
    if (IsKeyDown(KEY_G)) {
        Vector2 gp = ScreenToGrid(GetMousePosition());
        int x = (int)gp.x, y = (int)gp.y;
        int z = currentViewZ;

        // Left mouse - draw gather zone
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            drawingGatherZone = true;
            gatherZoneStartX = x;
            gatherZoneStartY = y;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && drawingGatherZone) {
            drawingGatherZone = false;
            int x1 = gatherZoneStartX < x ? gatherZoneStartX : x;
            int y1 = gatherZoneStartY < y ? gatherZoneStartY : y;
            int x2 = gatherZoneStartX > x ? gatherZoneStartX : x;
            int y2 = gatherZoneStartY > y ? gatherZoneStartY : y;

            if (x1 < 0) x1 = 0;
            if (y1 < 0) y1 = 0;
            if (x2 >= gridWidth) x2 = gridWidth - 1;
            if (y2 >= gridHeight) y2 = gridHeight - 1;

            int width = x2 - x1 + 1;
            int height = y2 - y1 + 1;

            int idx = CreateGatherZone(x1, y1, z, width, height);
            if (idx >= 0) {
                AddMessage(TextFormat("Created gather zone %dx%d", width, height), ORANGE);
            } else {
                AddMessage("Max gather zones reached!", RED);
            }
        }

        // Right mouse - erase gather zones
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            erasingGatherZone = true;
            gatherZoneStartX = x;
            gatherZoneStartY = y;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT) && erasingGatherZone) {
            erasingGatherZone = false;
            int x1 = gatherZoneStartX < x ? gatherZoneStartX : x;
            int y1 = gatherZoneStartY < y ? gatherZoneStartY : y;
            int x2 = gatherZoneStartX > x ? gatherZoneStartX : x;
            int y2 = gatherZoneStartY > y ? gatherZoneStartY : y;

            // Delete any gather zones that overlap with the erase area
            int deleted = 0;
            for (int i = MAX_GATHER_ZONES - 1; i >= 0; i--) {
                if (!gatherZones[i].active || gatherZones[i].z != z) continue;

                int gx1 = gatherZones[i].x;
                int gy1 = gatherZones[i].y;
                int gx2 = gx1 + gatherZones[i].width - 1;
                int gy2 = gy1 + gatherZones[i].height - 1;

                // Check overlap
                if (x1 <= gx2 && x2 >= gx1 && y1 <= gy2 && y2 >= gy1) {
                    DeleteGatherZone(i);
                    deleted++;
                }
            }
            if (deleted > 0) {
                AddMessage(TextFormat("Deleted %d gather zone%s", deleted, deleted > 1 ? "s" : ""), ORANGE);
            }
        }

        return;  // Skip normal tool interactions while G is held
    } else {
        drawingGatherZone = false;
        erasingGatherZone = false;
    }

    // Mining designation mode (M key + left-drag to designate, right-drag to cancel)
    if (IsKeyDown(KEY_M)) {
        Vector2 gp = ScreenToGrid(GetMousePosition());
        int x = (int)gp.x, y = (int)gp.y;
        int z = currentViewZ;

        // Left mouse - designate for mining
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            designatingMining = true;
            miningStartX = x;
            miningStartY = y;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && designatingMining) {
            designatingMining = false;
            int x1 = miningStartX < x ? miningStartX : x;
            int y1 = miningStartY < y ? miningStartY : y;
            int x2 = miningStartX > x ? miningStartX : x;
            int y2 = miningStartY > y ? miningStartY : y;

            if (x1 < 0) x1 = 0;
            if (y1 < 0) y1 = 0;
            if (x2 >= gridWidth) x2 = gridWidth - 1;
            if (y2 >= gridHeight) y2 = gridHeight - 1;

            int designated = 0;
            for (int dy = y1; dy <= y2; dy++) {
                for (int dx = x1; dx <= x2; dx++) {
                    if (DesignateDig(dx, dy, z)) {
                        designated++;
                    }
                }
            }
            if (designated > 0) {
                AddMessage(TextFormat("Designated %d cell%s for mining", designated, designated > 1 ? "s" : ""), ORANGE);
            }
        }

        // Right mouse - cancel mining designations in area
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            cancellingMining = true;
            miningStartX = x;
            miningStartY = y;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT) && cancellingMining) {
            cancellingMining = false;
            int x1 = miningStartX < x ? miningStartX : x;
            int y1 = miningStartY < y ? miningStartY : y;
            int x2 = miningStartX > x ? miningStartX : x;
            int y2 = miningStartY > y ? miningStartY : y;

            if (x1 < 0) x1 = 0;
            if (y1 < 0) y1 = 0;
            if (x2 >= gridWidth) x2 = gridWidth - 1;
            if (y2 >= gridHeight) y2 = gridHeight - 1;

            int cancelled = 0;
            for (int dy = y1; dy <= y2; dy++) {
                for (int dx = x1; dx <= x2; dx++) {
                    if (HasDigDesignation(dx, dy, z)) {
                        CancelDesignation(dx, dy, z);
                        cancelled++;
                    }
                }
            }
            if (cancelled > 0) {
                AddMessage(TextFormat("Cancelled %d mining designation%s", cancelled, cancelled > 1 ? "s" : ""), ORANGE);
            }
        }

        return;  // Skip normal tool interactions while M is held
    } else {
        designatingMining = false;
        cancellingMining = false;
    }

    // Build designation mode (B key + left-drag to designate, right-drag to cancel)
    if (IsKeyDown(KEY_B)) {
        Vector2 gp = ScreenToGrid(GetMousePosition());
        int x = (int)gp.x, y = (int)gp.y;
        int z = currentViewZ;

        // Left mouse - designate for building
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            designatingBuild = true;
            buildStartX = x;
            buildStartY = y;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && designatingBuild) {
            designatingBuild = false;
            int x1 = buildStartX < x ? buildStartX : x;
            int y1 = buildStartY < y ? buildStartY : y;
            int x2 = buildStartX > x ? buildStartX : x;
            int y2 = buildStartY > y ? buildStartY : y;

            if (x1 < 0) x1 = 0;
            if (y1 < 0) y1 = 0;
            if (x2 >= gridWidth) x2 = gridWidth - 1;
            if (y2 >= gridHeight) y2 = gridHeight - 1;

            int designated = 0;
            for (int dy = y1; dy <= y2; dy++) {
                for (int dx = x1; dx <= x2; dx++) {
                    if (CreateBuildBlueprint(dx, dy, z) >= 0) {
                        designated++;
                    }
                }
            }
            if (designated > 0) {
                AddMessage(TextFormat("Created %d blueprint%s for building", designated, designated > 1 ? "s" : ""), BLUE);
            }
        }

        // Right mouse - cancel build blueprints in area
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            cancellingBuild = true;
            buildStartX = x;
            buildStartY = y;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT) && cancellingBuild) {
            cancellingBuild = false;
            int x1 = buildStartX < x ? buildStartX : x;
            int y1 = buildStartY < y ? buildStartY : y;
            int x2 = buildStartX > x ? buildStartX : x;
            int y2 = buildStartY > y ? buildStartY : y;

            if (x1 < 0) x1 = 0;
            if (y1 < 0) y1 = 0;
            if (x2 >= gridWidth) x2 = gridWidth - 1;
            if (y2 >= gridHeight) y2 = gridHeight - 1;

            int cancelled = 0;
            for (int dy = y1; dy <= y2; dy++) {
                for (int dx = x1; dx <= x2; dx++) {
                    int bpIdx = GetBlueprintAt(dx, dy, z);
                    if (bpIdx >= 0) {
                        CancelBlueprint(bpIdx);
                        cancelled++;
                    }
                }
            }
            if (cancelled > 0) {
                AddMessage(TextFormat("Cancelled %d blueprint%s", cancelled, cancelled > 1 ? "s" : ""), ORANGE);
            }
        }

        return;  // Skip normal tool interactions while B is held
    } else {
        designatingBuild = false;
        cancellingBuild = false;
    }

    // Water placement mode (W key)
    // W + left-drag = place water 7/7
    // W + right-drag = remove water
    // W + Shift + left-drag = place water source
    // W + Shift + right-drag = place drain
    if (IsKeyDown(KEY_W)) {
        Vector2 gp = ScreenToGrid(GetMousePosition());
        int x = (int)gp.x, y = (int)gp.y;
        int z = currentViewZ;
        bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

        // Left mouse - place water or source
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            placingWaterSource = true;
            waterStartX = x;
            waterStartY = y;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && placingWaterSource) {
            placingWaterSource = false;
            int x1 = waterStartX < x ? waterStartX : x;
            int y1 = waterStartY < y ? waterStartY : y;
            int x2 = waterStartX > x ? waterStartX : x;
            int y2 = waterStartY > y ? waterStartY : y;

            if (x1 < 0) x1 = 0;
            if (y1 < 0) y1 = 0;
            if (x2 >= gridWidth) x2 = gridWidth - 1;
            if (y2 >= gridHeight) y2 = gridHeight - 1;

            int placed = 0;
            for (int dy = y1; dy <= y2; dy++) {
                for (int dx = x1; dx <= x2; dx++) {
                    if (shift) {
                        // Shift + left = water source
                        SetWaterSource(dx, dy, z, true);
                    }
                    SetWaterLevel(dx, dy, z, WATER_MAX_LEVEL);
                    placed++;
                }
            }
            if (placed > 0) {
                if (shift) {
                    AddMessage(TextFormat("Placed %d water source%s", placed, placed > 1 ? "s" : ""), BLUE);
                } else {
                    AddMessage(TextFormat("Placed water (7/7) in %d cell%s", placed, placed > 1 ? "s" : ""), SKYBLUE);
                }
            }
        }

        // Right mouse - remove water or place drain
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            placingWaterDrain = true;
            waterStartX = x;
            waterStartY = y;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT) && placingWaterDrain) {
            placingWaterDrain = false;
            int x1 = waterStartX < x ? waterStartX : x;
            int y1 = waterStartY < y ? waterStartY : y;
            int x2 = waterStartX > x ? waterStartX : x;
            int y2 = waterStartY > y ? waterStartY : y;

            if (x1 < 0) x1 = 0;
            if (y1 < 0) y1 = 0;
            if (x2 >= gridWidth) x2 = gridWidth - 1;
            if (y2 >= gridHeight) y2 = gridHeight - 1;

            int placedDrains = 0;
            int removedSources = 0;
            int removedDrains = 0;
            int removedWater = 0;
            for (int dy = y1; dy <= y2; dy++) {
                for (int dx = x1; dx <= x2; dx++) {
                    if (shift) {
                        // Shift + right = place drain
                        SetWaterDrain(dx, dy, z, true);
                        placedDrains++;
                    } else {
                        // Right = remove in priority: source -> drain -> water
                        WaterCell* cell = &waterGrid[z][dy][dx];
                        if (cell->isSource) {
                            SetWaterSource(dx, dy, z, false);
                            removedSources++;
                        } else if (cell->isDrain) {
                            SetWaterDrain(dx, dy, z, false);
                            removedDrains++;
                        } else if (cell->level > 0) {
                            SetWaterLevel(dx, dy, z, 0);
                            removedWater++;
                        }
                    }
                }
            }
            if (placedDrains > 0) {
                AddMessage(TextFormat("Placed %d drain%s", placedDrains, placedDrains > 1 ? "s" : ""), DARKBLUE);
            }
            if (removedSources > 0) {
                AddMessage(TextFormat("Removed %d source%s", removedSources, removedSources > 1 ? "s" : ""), ORANGE);
            }
            if (removedDrains > 0) {
                AddMessage(TextFormat("Removed %d drain%s", removedDrains, removedDrains > 1 ? "s" : ""), ORANGE);
            }
            if (removedWater > 0) {
                AddMessage(TextFormat("Removed water from %d cell%s", removedWater, removedWater > 1 ? "s" : ""), GRAY);
            }
        }

        return;  // Skip normal tool interactions while W is held
    } else {
        placingWaterSource = false;
        placingWaterDrain = false;
    }

    // Fire placement mode (F key)
    // F + left-drag = place fire (ignite at level 7)
    // F + right-drag = extinguish fire
    // F + Shift + left-drag = place fire source (permanent)
    // F + Shift + right-drag = remove fire source
    if (IsKeyDown(KEY_F)) {
        Vector2 gp = ScreenToGrid(GetMousePosition());
        int x = (int)gp.x, y = (int)gp.y;
        int z = currentViewZ;
        bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

        // Left mouse - place fire or source
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            placingFireSource = true;
            fireStartX = x;
            fireStartY = y;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && placingFireSource) {
            placingFireSource = false;
            int x1 = fireStartX < x ? fireStartX : x;
            int y1 = fireStartY < y ? fireStartY : y;
            int x2 = fireStartX > x ? fireStartX : x;
            int y2 = fireStartY > y ? fireStartY : y;

            if (x1 < 0) x1 = 0;
            if (y1 < 0) y1 = 0;
            if (x2 >= gridWidth) x2 = gridWidth - 1;
            if (y2 >= gridHeight) y2 = gridHeight - 1;

            int placed = 0;
            for (int dy = y1; dy <= y2; dy++) {
                for (int dx = x1; dx <= x2; dx++) {
                    if (shift) {
                        // Shift + left = fire source
                        SetFireSource(dx, dy, z, true);
                        placed++;
                    } else {
                        // Normal left = ignite if flammable
                        if (GetBaseFuelForCellType(grid[z][dy][dx]) > 0 && 
                            !HAS_CELL_FLAG(dx, dy, z, CELL_FLAG_BURNED)) {
                            IgniteCell(dx, dy, z);
                            placed++;
                        }
                    }
                }
            }
            if (placed > 0) {
                if (shift) {
                    AddMessage(TextFormat("Placed %d fire source%s", placed, placed > 1 ? "s" : ""), RED);
                } else {
                    AddMessage(TextFormat("Ignited %d cell%s", placed, placed > 1 ? "s" : ""), ORANGE);
                }
            }
        }

        // Right mouse - extinguish fire or remove source
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            extinguishingFire = true;
            fireStartX = x;
            fireStartY = y;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT) && extinguishingFire) {
            extinguishingFire = false;
            int x1 = fireStartX < x ? fireStartX : x;
            int y1 = fireStartY < y ? fireStartY : y;
            int x2 = fireStartX > x ? fireStartX : x;
            int y2 = fireStartY > y ? fireStartY : y;

            if (x1 < 0) x1 = 0;
            if (y1 < 0) y1 = 0;
            if (x2 >= gridWidth) x2 = gridWidth - 1;
            if (y2 >= gridHeight) y2 = gridHeight - 1;

            int removedSources = 0;
            int extinguished = 0;
            for (int dy = y1; dy <= y2; dy++) {
                for (int dx = x1; dx <= x2; dx++) {
                    FireCell* cell = &fireGrid[z][dy][dx];
                    if (shift) {
                        // Shift + right = remove fire source
                        if (cell->isSource) {
                            SetFireSource(dx, dy, z, false);
                            removedSources++;
                        }
                    } else {
                        // Right = extinguish fire
                        if (cell->isSource) {
                            SetFireSource(dx, dy, z, false);
                            removedSources++;
                        }
                        if (cell->level > 0) {
                            ExtinguishCell(dx, dy, z);
                            extinguished++;
                        }
                    }
                }
            }
            if (removedSources > 0) {
                AddMessage(TextFormat("Removed %d fire source%s", removedSources, removedSources > 1 ? "s" : ""), ORANGE);
            }
            if (extinguished > 0) {
                AddMessage(TextFormat("Extinguished %d cell%s", extinguished, extinguished > 1 ? "s" : ""), GRAY);
            }
        }

        return;  // Skip normal tool interactions while F is held
    } else {
        placingFireSource = false;
        extinguishingFire = false;
    }

    // Heat/Cold source placement mode (H key)
    // H + left-drag = place heat source
    // H + right-drag = place cold source
    // H + Shift + left-drag = remove heat source
    // H + Shift + right-drag = remove cold source
    if (IsKeyDown(KEY_H)) {
        Vector2 gp = ScreenToGrid(GetMousePosition());
        int x = (int)gp.x, y = (int)gp.y;
        int z = currentViewZ;
        bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

        // Left mouse - place heat source or remove heat source (with shift)
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            placingHeatSource = true;
            tempStartX = x;
            tempStartY = y;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && placingHeatSource) {
            placingHeatSource = false;
            int x1 = tempStartX < x ? tempStartX : x;
            int y1 = tempStartY < y ? tempStartY : y;
            int x2 = tempStartX > x ? tempStartX : x;
            int y2 = tempStartY > y ? tempStartY : y;

            if (x1 < 0) x1 = 0;
            if (y1 < 0) y1 = 0;
            if (x2 >= gridWidth) x2 = gridWidth - 1;
            if (y2 >= gridHeight) y2 = gridHeight - 1;

            int count = 0;
            for (int dy = y1; dy <= y2; dy++) {
                for (int dx = x1; dx <= x2; dx++) {
                    if (shift) {
                        // Shift + left = remove heat source
                        TempCell* cell = &temperatureGrid[z][dy][dx];
                        if (cell->isHeatSource) {
                            SetHeatSource(dx, dy, z, false);
                            count++;
                        }
                    } else {
                        // Left = place heat source (100C, hot)
                        SetHeatSource(dx, dy, z, true);
                        count++;
                    }
                }
            }
            if (count > 0) {
                if (shift) {
                    AddMessage(TextFormat("Removed %d heat source%s", count, count > 1 ? "s" : ""), ORANGE);
                } else {
                    AddMessage(TextFormat("Placed %d heat source%s", count, count > 1 ? "s" : ""), RED);
                }
            }
        }

        // Right mouse - place cold source or remove cold source (with shift)
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            placingColdSource = true;
            tempStartX = x;
            tempStartY = y;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT) && placingColdSource) {
            placingColdSource = false;
            int x1 = tempStartX < x ? tempStartX : x;
            int y1 = tempStartY < y ? tempStartY : y;
            int x2 = tempStartX > x ? tempStartX : x;
            int y2 = tempStartY > y ? tempStartY : y;

            if (x1 < 0) x1 = 0;
            if (y1 < 0) y1 = 0;
            if (x2 >= gridWidth) x2 = gridWidth - 1;
            if (y2 >= gridHeight) y2 = gridHeight - 1;

            int count = 0;
            for (int dy = y1; dy <= y2; dy++) {
                for (int dx = x1; dx <= x2; dx++) {
                    if (shift) {
                        // Shift + right = remove cold source
                        TempCell* cell = &temperatureGrid[z][dy][dx];
                        if (cell->isColdSource) {
                            SetColdSource(dx, dy, z, false);
                            count++;
                        }
                    } else {
                        // Right = place cold source (-20C, freezing)
                        SetColdSource(dx, dy, z, true);
                        count++;
                    }
                }
            }
            if (count > 0) {
                if (shift) {
                    AddMessage(TextFormat("Removed %d cold source%s", count, count > 1 ? "s" : ""), ORANGE);
                } else {
                    AddMessage(TextFormat("Placed %d cold source%s (freezing)", count, count > 1 ? "s" : ""), SKYBLUE);
                }
            }
        }

        return;  // Skip normal tool interactions while H is held
    } else {
        placingHeatSource = false;
        placingColdSource = false;
    }

    // Unburn mode (U key)
    // U + drag = clear burned flag from cells
    if (IsKeyDown(KEY_U)) {
        Vector2 gp = ScreenToGrid(GetMousePosition());
        int x = (int)gp.x, y = (int)gp.y;
        int z = currentViewZ;

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            unburning = true;
            unburnStartX = x;
            unburnStartY = y;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && unburning) {
            unburning = false;
            int x1 = unburnStartX < x ? unburnStartX : x;
            int y1 = unburnStartY < y ? unburnStartY : y;
            int x2 = unburnStartX > x ? unburnStartX : x;
            int y2 = unburnStartY > y ? unburnStartY : y;

            if (x1 < 0) x1 = 0;
            if (y1 < 0) y1 = 0;
            if (x2 >= gridWidth) x2 = gridWidth - 1;
            if (y2 >= gridHeight) y2 = gridHeight - 1;

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

        return;  // Skip normal tool interactions while U is held
    } else {
        unburning = false;
    }

    // Ladder drawing shortcut (L key + click/drag)
    // Uses PlaceLadder for auto-connection behavior
    if (IsKeyDown(KEY_L) && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        Vector2 gp = ScreenToGrid(GetMousePosition());
        int x = (int)gp.x, y = (int)gp.y;
        int z = currentViewZ;
        if (x >= 0 && x < gridWidth && y >= 0 && y < gridHeight) {
            PlaceLadder(x, y, z);
        }
        return;
    }

    // Tool-based interactions
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        Vector2 gp = ScreenToGrid(GetMousePosition());
        int x = (int)gp.x, y = (int)gp.y;
        int z = currentViewZ;
        if (x >= 0 && x < gridWidth && y >= 0 && y < gridHeight) {
            switch (currentTool) {
                case 0:  // Draw Wall (hold 2 for wood wall)
                    {
                        CellType wallType = IsKeyDown(KEY_TWO) ? CELL_WOOD_WALL : CELL_WALL;
                        if (grid[z][y][x] != wallType) {
                        grid[z][y][x] = wallType;
                        MarkChunkDirty(x, y, z);
                        // Clear water in this cell and destabilize neighbors
                        SetWaterLevel(x, y, z, 0);
                        SetWaterSource(x, y, z, false);
                        SetWaterDrain(x, y, z, false);
                        DestabilizeWater(x, y, z);
                        // Mark movers whose path crosses this cell for replanning
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
                    break;
                case 1:  // Draw Floor
                    if (grid[z][y][x] != CELL_FLOOR) {
                        grid[z][y][x] = CELL_FLOOR;
                        MarkChunkDirty(x, y, z);
                    }
                    break;
                case 2:  // Draw Ladder (auto-connects with level above)
                    PlaceLadder(x, y, z);
                    break;
                case 3:  // Erase (handles ladder cascade, air on z>0, grass on z=0)
                    if (IsLadderCell(grid[z][y][x])) {
                        EraseLadder(x, y, z);
                    } else {
                        CellType eraseType = (z > 0) ? CELL_AIR : CELL_WALKABLE;
                        if (grid[z][y][x] != eraseType) {
                            grid[z][y][x] = eraseType;
                            MarkChunkDirty(x, y, z);
                            // Wake up water in this cell and neighbors (wall removal)
                            DestabilizeWater(x, y, z);
                        }
                    }
                    break;
                case 4:  // Set Start
                    if (IsCellWalkableAt(z, y, x)) {
                        startPos = (Point){x, y, z};
                        pathLength = 0;
                    }
                    break;
                case 5:  // Set Goal
                    if (IsCellWalkableAt(z, y, x)) {
                        goalPos = (Point){x, y, z};
                        pathLength = 0;
                    }
                    break;
            }
        }
    }

    // Right-click erases (convenience shortcut, handles ladder cascade)
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        Vector2 gp = ScreenToGrid(GetMousePosition());
        int x = (int)gp.x, y = (int)gp.y;
        int z = currentViewZ;
        if (x >= 0 && x < gridWidth && y >= 0 && y < gridHeight) {
            if (IsLadderCell(grid[z][y][x])) {
                EraseLadder(x, y, z);
            } else {
                CellType eraseType = (z > 0) ? CELL_AIR : CELL_WALKABLE;
                if (grid[z][y][x] != eraseType) {
                    grid[z][y][x] = eraseType;
                    MarkChunkDirty(x, y, z);
                    // Wake up water in this cell and neighbors (wall removal)
                    DestabilizeWater(x, y, z);
                }
            }
        }
    }

    // Reset view (C key)
    if (IsKeyPressed(KEY_C)) {
        zoom = 1.0f;
        offset.x = (1280 - gridWidth * CELL_SIZE * zoom) / 2.0f;
        offset.y = (800 - gridHeight * CELL_SIZE * zoom) / 2.0f;
    }

    // Z-level switching (, and . keys)
    if (IsKeyPressed(KEY_PERIOD)) {  // .
        if (currentViewZ < gridDepth - 1) currentViewZ++;
    }
    if (IsKeyPressed(KEY_COMMA)) {  // ,
        if (currentViewZ > 0) currentViewZ--;
    }

    // Pause toggle (Space)
    if (IsKeyPressed(KEY_SPACE)) {
        paused = !paused;
    }
    
    // Quick save (F5)
    if (IsKeyPressed(KEY_F5)) {
        system("mkdir -p saves");
        if (SaveWorld("saves/debug_save.bin")) {
            AddMessage("World saved to saves/debug_save.bin", GREEN);
            
            // Also archive a gzipped copy with timestamp
            time_t now = time(NULL);
            struct tm* t = localtime(&now);
            char cmd[256];
            snprintf(cmd, sizeof(cmd), 
                "gzip -c saves/debug_save.bin > saves/%04d-%02d-%02d_%02d-%02d-%02d.bin.gz",
                t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                t->tm_hour, t->tm_min, t->tm_sec);
            system(cmd);
        }
    }
    
    // Quick load (F6)
    if (IsKeyPressed(KEY_F6)) {
        if (LoadWorld("saves/debug_save.bin")) {
            AddMessage("World loaded from saves/debug_save.bin", GREEN);
        }
    }
}
