// render/tooltips.c - Tooltip drawing functions
#include "../game_state.h"

// Draw stockpile tooltip at mouse position
void DrawStockpileTooltip(int spIdx, Vector2 mouse, Vector2 mouseGrid) {
    if (spIdx < 0 || spIdx >= MAX_STOCKPILES) return;
    Stockpile* sp = &stockpiles[spIdx];
    if (!sp->active) return;

    // Count items and capacity (only active cells)
    int totalItems = 0;
    int totalSlots = sp->width * sp->height;
    int activeCells = 0;
    for (int i = 0; i < totalSlots; i++) {
        if (sp->cells[i]) {
            activeCells++;
            totalItems += sp->slotCounts[i];
        }
    }
    int maxCapacity = activeCells * sp->maxStackSize;

    // Get hovered cell info within stockpile
    int cellX = (int)mouseGrid.x;
    int cellY = (int)mouseGrid.y;
    int localX = cellX - sp->x;
    int localY = cellY - sp->y;
    int slotIdx = localY * sp->width + localX;
    int cellCount = (slotIdx >= 0 && slotIdx < totalSlots) ? sp->slotCounts[slotIdx] : 0;

    // Build tooltip text
    const char* titleText = TextFormat("Stockpile #%d", spIdx);
    const char* priorityText = TextFormat("Priority: %d", sp->priority);
    const char* stackText = TextFormat("Stack size: %d", sp->maxStackSize);
    const char* storageText = TextFormat("Storage: %d/%d (%d cells)", totalItems, maxCapacity, activeCells);
    const char* cellText = TextFormat("Cell (%d,%d): %d/%d items", cellX, cellY, cellCount, sp->maxStackSize);
    const char* helpText = "+/- priority, [/] stack, R/G/B filter";

    // Measure text
    int w0 = MeasureText(titleText, 14);
    int w1 = MeasureText(priorityText, 14);
    int w2 = MeasureText(stackText, 14);
    int w3 = MeasureText(storageText, 14);
    int w4 = MeasureText(cellText, 14);
    int w5 = MeasureText("Filters: R G B O", 14);
    int w6 = MeasureText(helpText, 12);
    int maxW = w0;
    if (w1 > maxW) maxW = w1;
    if (w2 > maxW) maxW = w2;
    if (w3 > maxW) maxW = w3;
    if (w4 > maxW) maxW = w4;
    if (w5 > maxW) maxW = w5;
    if (w6 > maxW) maxW = w6;

    int padding = 6;
    int boxW = maxW + padding * 2;
    int boxH = 14 * 6 + 12 + padding * 2 + 10;

    // Position tooltip near mouse, keep on screen
    int tx = (int)mouse.x + 15;
    int ty = (int)mouse.y + 15;
    if (tx + boxW > GetScreenWidth()) tx = (int)mouse.x - boxW - 5;
    if (ty + boxH > GetScreenHeight()) ty = (int)mouse.y - boxH - 5;

    // Draw background
    DrawRectangle(tx, ty, boxW, boxH, (Color){20, 20, 20, 220});
    DrawRectangleLines(tx, ty, boxW, boxH, (Color){80, 80, 80, 255});

    // Draw text
    int y = ty + padding;
    DrawTextShadow(titleText, tx + padding, y, 14, YELLOW);
    y += 16;

    DrawTextShadow(priorityText, tx + padding, y, 14, WHITE);
    y += 16;

    DrawTextShadow(stackText, tx + padding, y, 14, WHITE);
    y += 16;

    bool overfull = totalItems > maxCapacity;
    DrawTextShadow(storageText, tx + padding, y, 14, overfull ? RED : WHITE);
    y += 16;

    bool cellFull = cellCount >= sp->maxStackSize;
    DrawTextShadow(cellText, tx + padding, y, 14, cellFull ? ORANGE : WHITE);
    y += 16;

    // Draw filters with color coding
    int fx = tx + padding;
    DrawTextShadow("Filters: ", fx, y, 14, WHITE);
    fx += MeasureText("Filters: ", 14);
    DrawTextShadow(sp->allowedTypes[ITEM_RED] ? "R" : "-", fx, y, 14,
        sp->allowedTypes[ITEM_RED] ? RED : DARKGRAY);
    fx += MeasureText("R", 14) + 4;
    DrawTextShadow(sp->allowedTypes[ITEM_GREEN] ? "G" : "-", fx, y, 14,
        sp->allowedTypes[ITEM_GREEN] ? GREEN : DARKGRAY);
    fx += MeasureText("G", 14) + 4;
    DrawTextShadow(sp->allowedTypes[ITEM_BLUE] ? "B" : "-", fx, y, 14,
        sp->allowedTypes[ITEM_BLUE] ? BLUE : DARKGRAY);
    fx += MeasureText("B", 14) + 4;
    DrawTextShadow(sp->allowedTypes[ITEM_ORANGE] ? "O" : "-", fx, y, 14,
        sp->allowedTypes[ITEM_ORANGE] ? ORANGE : DARKGRAY);
    y += 18;

    DrawTextShadow(helpText, tx + padding, y, 12, GRAY);
}

// Draw mover debug tooltip (only shown when paused)
void DrawMoverTooltip(int moverIdx, Vector2 mouse) {
    if (moverIdx < 0 || moverIdx >= moverCount) return;
    Mover* m = &movers[moverIdx];
    if (!m->active) return;

    Job* job = (m->currentJobId >= 0) ? GetJob(m->currentJobId) : NULL;

    const char* jobTypeNames[] = {"NONE", "HAUL", "CLEAR", "DIG", "HAUL_TO_BP", "BUILD"};
    const char* jobTypeName = job ? (job->type < 6 ? jobTypeNames[job->type] : "?") : "IDLE";

    int carryingItem = job ? job->carryingItem : -1;
    int targetStockpile = job ? job->targetStockpile : -1;
    int targetSlotX = job ? job->targetSlotX : -1;
    int targetSlotY = job ? job->targetSlotY : -1;
    int targetItem = job ? job->targetItem : -1;
    int jobStep = job ? job->step : 0;

    char line1[64], line2[64], line3[64], line4[64], line5[64], line6[64];
    char line7[64] = "";
    char line8[64] = "";
    snprintf(line1, sizeof(line1), "Mover #%d", moverIdx);
    snprintf(line2, sizeof(line2), "Pos: (%.1f, %.1f, %.0f)", m->x, m->y, m->z);
    snprintf(line3, sizeof(line3), "Job: %s (step %d)", jobTypeName, jobStep);
    snprintf(line4, sizeof(line4), "Carrying: %s", carryingItem >= 0 ? TextFormat("#%d", carryingItem) : "none");
    snprintf(line5, sizeof(line5), "Path: %d/%d, Goal: (%d,%d)",
        m->pathIndex >= 0 ? m->pathIndex + 1 : 0, m->pathLength, m->goal.x, m->goal.y);
    snprintf(line6, sizeof(line6), "Target SP: %d, Slot: (%d,%d)",
        targetStockpile, targetSlotX, targetSlotY);

    int numLines = 6;
    float pickupRadius = CELL_SIZE * 0.75f;
    if (job && jobStep == 0 && targetItem >= 0 && items[targetItem].active) {
        Item* item = &items[targetItem];
        float dx = m->x - item->x;
        float dy = m->y - item->y;
        float dist = sqrtf(dx*dx + dy*dy);
        snprintf(line7, sizeof(line7), "Item #%d at (%.1f,%.1f) dist=%.1f",
            targetItem, item->x, item->y, dist);
        snprintf(line8, sizeof(line8), "Pickup radius: %.1f %s",
            pickupRadius, dist < pickupRadius ? "IN RANGE" : "OUT OF RANGE");
        numLines = 8;
    }

    int w1 = MeasureText(line1, 14);
    int w2 = MeasureText(line2, 14);
    int w3 = MeasureText(line3, 14);
    int w4 = MeasureText(line4, 14);
    int w5 = MeasureText(line5, 14);
    int w6 = MeasureText(line6, 14);
    int w7 = MeasureText(line7, 14);
    int w8 = MeasureText(line8, 14);
    int maxW = w1;
    if (w2 > maxW) maxW = w2;
    if (w3 > maxW) maxW = w3;
    if (w4 > maxW) maxW = w4;
    if (w5 > maxW) maxW = w5;
    if (w6 > maxW) maxW = w6;
    if (w7 > maxW) maxW = w7;
    if (w8 > maxW) maxW = w8;

    int padding = 6;
    int lineH = 16;
    int boxW = maxW + padding * 2;
    int boxH = lineH * numLines + padding * 2;

    int tx = (int)mouse.x + 15;
    int ty = (int)mouse.y + 15;
    if (tx + boxW > GetScreenWidth()) tx = (int)mouse.x - boxW - 5;
    if (ty + boxH > GetScreenHeight()) ty = (int)mouse.y - boxH - 5;

    DrawRectangle(tx, ty, boxW, boxH, (Color){20, 20, 40, 220});
    DrawRectangleLines(tx, ty, boxW, boxH, (Color){100, 100, 150, 255});

    int y = ty + padding;
    DrawTextShadow(line1, tx + padding, y, 14, YELLOW);
    y += lineH;
    DrawTextShadow(line2, tx + padding, y, 14, WHITE);
    y += lineH;
    DrawTextShadow(line3, tx + padding, y, 14, !job ? GRAY : GREEN);
    y += lineH;
    DrawTextShadow(line4, tx + padding, y, 14, carryingItem >= 0 ? ORANGE : GRAY);
    y += lineH;
    DrawTextShadow(line5, tx + padding, y, 14, m->pathLength > 0 ? WHITE : RED);
    y += lineH;
    DrawTextShadow(line6, tx + padding, y, 14, targetStockpile >= 0 ? WHITE : GRAY);

    if (numLines == 8 && targetItem >= 0 && items[targetItem].active) {
        y += lineH;
        DrawTextShadow(line7, tx + padding, y, 14, SKYBLUE);
        y += lineH;
        float dx = m->x - items[targetItem].x;
        float dy = m->y - items[targetItem].y;
        float dist = sqrtf(dx*dx + dy*dy);
        DrawTextShadow(line8, tx + padding, y, 14, dist < pickupRadius ? GREEN : RED);
    }
}

// Draw item tooltip (only shown when paused)
void DrawItemTooltip(int* itemIndices, int itemCount, Vector2 mouse, int cellX, int cellY) {
    if (itemCount <= 0) return;

    const char* typeNames[] = {"Red", "Green", "Blue"};
    const char* stateNames[] = {"Ground", "Carried", "Stockpile"};

    char lines[17][64];
    snprintf(lines[0], sizeof(lines[0]), "Cell (%d,%d): %d item%s", cellX, cellY, itemCount, itemCount == 1 ? "" : "s");

    int lineCount = 1;
    for (int i = 0; i < itemCount && lineCount < 17; i++) {
        int idx = itemIndices[i];
        Item* item = &items[idx];
        const char* typeName = (item->type >= 0 && item->type < 3) ? typeNames[item->type] : "?";
        const char* stateName = (item->state >= 0 && item->state < 3) ? stateNames[item->state] : "?";
        snprintf(lines[lineCount], sizeof(lines[lineCount]), "#%d: %s (%s)", idx, typeName, stateName);
        lineCount++;
    }

    int maxW = 0;
    for (int i = 0; i < lineCount; i++) {
        int w = MeasureText(lines[i], 14);
        if (w > maxW) maxW = w;
    }

    int padding = 6;
    int lineH = 16;
    int boxW = maxW + padding * 2;
    int boxH = lineH * lineCount + padding * 2;

    int tx = (int)mouse.x + 15;
    int ty = (int)mouse.y + 15;
    if (tx + boxW > GetScreenWidth()) tx = (int)mouse.x - boxW - 5;
    if (ty + boxH > GetScreenHeight()) ty = (int)mouse.y - boxH - 5;

    DrawRectangle(tx, ty, boxW, boxH, (Color){40, 30, 20, 220});
    DrawRectangleLines(tx, ty, boxW, boxH, (Color){150, 100, 50, 255});

    int y = ty + padding;
    DrawTextShadow(lines[0], tx + padding, y, 14, YELLOW);
    y += lineH;

    for (int i = 1; i < lineCount; i++) {
        int idx = itemIndices[i - 1];
        Color col = WHITE;
        if (items[idx].type == ITEM_RED) col = RED;
        else if (items[idx].type == ITEM_GREEN) col = GREEN;
        else if (items[idx].type == ITEM_BLUE) col = (Color){100, 150, 255, 255};
        DrawTextShadow(lines[i], tx + padding, y, 14, col);
        y += lineH;
    }
}

// Draw comprehensive cell tooltip (shown when paused)
void DrawCellTooltip(int cellX, int cellY, int cellZ, Vector2 mouse) {
    if (cellX < 0 || cellX >= gridWidth || cellY < 0 || cellY >= gridHeight) return;
    if (cellZ < 0 || cellZ >= gridDepth) return;

    char lines[20][64];
    int lineCount = 0;

    // Header with coordinates
    snprintf(lines[lineCount++], sizeof(lines[0]), "Cell (%d, %d, z%d)", cellX, cellY, cellZ);

    // Cell type
    CellType ct = grid[cellZ][cellY][cellX];
    const char* cellTypeName = "?";
    switch (ct) {
        case CELL_WALL: cellTypeName = "Wall"; break;
        case CELL_FLOOR: cellTypeName = "Floor"; break;
        case CELL_WALKABLE: cellTypeName = "Grass"; break;
        case CELL_AIR: cellTypeName = "Air"; break;
        case CELL_LADDER: cellTypeName = "Ladder"; break;
        case CELL_LADDER_UP: cellTypeName = "Ladder Up"; break;
        case CELL_LADDER_DOWN: cellTypeName = "Ladder Down"; break;
        case CELL_LADDER_BOTH: cellTypeName = "Ladder Both"; break;
        case CELL_GRASS: cellTypeName = "Grass"; break;
        case CELL_DIRT: cellTypeName = "Dirt"; break;
        default: cellTypeName = "Unknown"; break;
    }
    bool isBurned = HAS_CELL_FLAG(cellX, cellY, cellZ, CELL_FLAG_BURNED);
    if (isBurned) {
        snprintf(lines[lineCount++], sizeof(lines[0]), "Type: %s [BURNED]", cellTypeName);
    } else {
        snprintf(lines[lineCount++], sizeof(lines[0]), "Type: %s", cellTypeName);
    }

    // Temperature info (all values are now Celsius directly)
    int temp = GetTemperature(cellX, cellY, cellZ);
    int ambient = GetAmbientTemperature(cellZ);
    int diff = temp - ambient;
    snprintf(lines[lineCount++], sizeof(lines[0]), "Temp: %d C", temp);
    snprintf(lines[lineCount++], sizeof(lines[0]), "Ambient: %d C [%+d]", ambient, diff);

    // Temperature sources
    TempCell* tempCell = &temperatureGrid[cellZ][cellY][cellX];
    if (tempCell->isHeatSource) {
        snprintf(lines[lineCount++], sizeof(lines[0]), "HEAT SOURCE (%d C)", heatSourceTemp);
    }
    if (tempCell->isColdSource) {
        snprintf(lines[lineCount++], sizeof(lines[0]), "COLD SOURCE (%d C)", coldSourceTemp);
    }

    // Insulation tier
    int tier = GetInsulationTier(cellX, cellY, cellZ);
    const char* tierName = tier == 2 ? "Stone (5%)" : tier == 1 ? "Wood (20%)" : "Air (100%)";
    snprintf(lines[lineCount++], sizeof(lines[0]), "Insulation: %s", tierName);

    // Water info
    WaterCell* water = &waterGrid[cellZ][cellY][cellX];
    if (water->level > 0 || water->isSource || water->isDrain) {
        snprintf(lines[lineCount++], sizeof(lines[0]), "Water: %d/7%s", 
            water->level, water->isFrozen ? " [FROZEN]" : "");
        if (water->isSource) {
            snprintf(lines[lineCount++], sizeof(lines[0]), "  WATER SOURCE");
        }
        if (water->isDrain) {
            snprintf(lines[lineCount++], sizeof(lines[0]), "  WATER DRAIN");
        }
        if (water->hasPressure) {
            snprintf(lines[lineCount++], sizeof(lines[0]), "  Pressure (z=%d)", water->pressureSourceZ);
        }
    }

    // Fire info
    FireCell* fire = &fireGrid[cellZ][cellY][cellX];
    if (fire->level > 0 || fire->isSource) {
        snprintf(lines[lineCount++], sizeof(lines[0]), "Fire: %d/7", fire->level);
        if (fire->isSource) {
            snprintf(lines[lineCount++], sizeof(lines[0]), "  FIRE SOURCE");
        }
        if (fire->fuel > 0) {
            snprintf(lines[lineCount++], sizeof(lines[0]), "  Fuel: %d", fire->fuel);
        }
    }

    // Smoke info
    SmokeCell* smoke = &smokeGrid[cellZ][cellY][cellX];
    if (smoke->level > 0) {
        snprintf(lines[lineCount++], sizeof(lines[0]), "Smoke: %d/7", smoke->level);
    }

    // Calculate box dimensions
    int maxW = 0;
    for (int i = 0; i < lineCount; i++) {
        int w = MeasureText(lines[i], 14);
        if (w > maxW) maxW = w;
    }

    int padding = 6;
    int lineH = 16;
    int boxW = maxW + padding * 2;
    int boxH = lineH * lineCount + padding * 2;

    // Position tooltip
    int tx = (int)mouse.x + 15;
    int ty = (int)mouse.y + 15;
    if (tx + boxW > GetScreenWidth()) tx = (int)mouse.x - boxW - 5;
    if (ty + boxH > GetScreenHeight()) ty = (int)mouse.y - boxH - 5;

    // Draw background
    DrawRectangle(tx, ty, boxW, boxH, (Color){30, 30, 30, 230});
    DrawRectangleLines(tx, ty, boxW, boxH, (Color){80, 80, 80, 255});

    // Draw lines with color coding
    int y = ty + padding;
    for (int i = 0; i < lineCount; i++) {
        Color col = WHITE;
        if (i == 0) col = YELLOW;  // Header
        else if (strstr(lines[i], "[BURNED]")) col = (Color){80, 60, 40, 255};
        else if (strstr(lines[i], "HEAT SOURCE")) col = RED;
        else if (strstr(lines[i], "COLD SOURCE")) col = SKYBLUE;
        else if (strstr(lines[i], "FIRE SOURCE")) col = ORANGE;
        else if (strstr(lines[i], "WATER SOURCE")) col = (Color){100, 180, 255, 255};
        else if (strstr(lines[i], "WATER DRAIN")) col = (Color){80, 80, 120, 255};
        else if (strstr(lines[i], "FROZEN")) col = (Color){200, 220, 255, 255};
        else if (strstr(lines[i], "Fire:")) col = ORANGE;
        else if (strstr(lines[i], "Water:")) col = (Color){100, 180, 255, 255};
        else if (strstr(lines[i], "Smoke:")) col = GRAY;
        else if (strstr(lines[i], "freezing")) col = (Color){150, 200, 255, 255};
        else if (strstr(lines[i], "cold")) col = (Color){180, 210, 255, 255};
        else if (strstr(lines[i], "hot!")) col = (Color){255, 100, 100, 255};
        else if (strstr(lines[i], "warm")) col = (Color){255, 180, 100, 255};
        DrawTextShadow(lines[i], tx + padding, y, 14, col);
        y += lineH;
    }
}

// Draw water tooltip when hovering over water
void DrawWaterTooltip(int cellX, int cellY, int cellZ, Vector2 mouse) {
    WaterCell* cell = &waterGrid[cellZ][cellY][cellX];
    if (cell->level == 0 && !cell->isSource && !cell->isDrain) return;

    char lines[8][64];
    int lineCount = 0;

    snprintf(lines[lineCount++], sizeof(lines[0]), "Water (%d,%d,z%d)", cellX, cellY, cellZ);
    snprintf(lines[lineCount++], sizeof(lines[0]), "Level: %d/7", cell->level);
    
    float speedMult = GetWaterSpeedMultiplier(cellX, cellY, cellZ);
    snprintf(lines[lineCount++], sizeof(lines[0]), "Speed: %.0f%%", speedMult * 100);
    
    if (cell->isSource) {
        snprintf(lines[lineCount++], sizeof(lines[0]), "SOURCE");
    }
    if (cell->isDrain) {
        snprintf(lines[lineCount++], sizeof(lines[0]), "DRAIN");
    }
    if (cell->hasPressure) {
        snprintf(lines[lineCount++], sizeof(lines[0]), "Pressure (src z=%d)", cell->pressureSourceZ);
        snprintf(lines[lineCount++], sizeof(lines[0]), "Max rise: z=%d", cell->pressureSourceZ - 1);
    }
    if (cell->stable) {
        snprintf(lines[lineCount++], sizeof(lines[0]), "[stable]");
    }

    int maxW = 0;
    for (int i = 0; i < lineCount; i++) {
        int w = MeasureText(lines[i], 14);
        if (w > maxW) maxW = w;
    }

    int padding = 6;
    int lineH = 16;
    int boxW = maxW + padding * 2;
    int boxH = lineH * lineCount + padding * 2;

    int tx = (int)mouse.x + 15;
    int ty = (int)mouse.y + 15;
    if (tx + boxW > GetScreenWidth()) tx = (int)mouse.x - boxW - 5;
    if (ty + boxH > GetScreenHeight()) ty = (int)mouse.y - boxH - 5;

    DrawRectangle(tx, ty, boxW, boxH, (Color){20, 40, 60, 220});
    DrawRectangleLines(tx, ty, boxW, boxH, (Color){50, 100, 200, 255});

    int y = ty + padding;
    for (int i = 0; i < lineCount; i++) {
        Color col = WHITE;
        if (i == 0) col = (Color){100, 180, 255, 255};
        else if (strstr(lines[i], "SOURCE")) col = (Color){150, 220, 255, 255};
        else if (strstr(lines[i], "DRAIN")) col = (Color){80, 80, 120, 255};
        else if (strstr(lines[i], "Pressure")) col = YELLOW;
        else if (strstr(lines[i], "stable")) col = GRAY;
        DrawTextShadow(lines[i], tx + padding, y, 14, col);
        y += lineH;
    }
}
