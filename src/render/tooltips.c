// render/tooltips.c - Tooltip drawing functions
#include "../game_state.h"
#include "../world/cell_defs.h"
#include "../world/material.h"
#include "../entities/workshops.h"
#include "../entities/item_defs.h"
#include "../entities/jobs.h"
#include "../world/designations.h"
#include "../simulation/trees.h"

static void BuildFillMeter(char* out, size_t outSize, float ratio, int width) {
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    int filled = (int)(ratio * width + 0.5f);
    if (filled > width) filled = width;

    char bar[32];
    int idx = 0;
    bar[idx++] = '[';
    for (int i = 0; i < width; i++) {
        bar[idx++] = (i < filled) ? '|' : ' ';
    }
    bar[idx++] = ']';
    bar[idx] = '\0';

    int percent = (int)(ratio * 100.0f + 0.5f);
    snprintf(out, outSize, "%s %d%%", bar, percent);
}

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
    char fillMeter[64];
    BuildFillMeter(fillMeter, sizeof(fillMeter), GetStockpileFillRatio(spIdx), 10);
    const char* fillText = TextFormat("Fill: %s", fillMeter);
    const char* cellText = TextFormat("Cell (%d,%d): %d/%d items", cellX, cellY, cellCount, sp->maxStackSize);
    const char* helpText = "+/- priority, [/] stack, R/G/B filter";

    // Measure text
    int w0 = MeasureText(titleText, 14);
    int w1 = MeasureText(priorityText, 14);
    int w2 = MeasureText(stackText, 14);
    int w3 = MeasureText(storageText, 14);
    int w4 = MeasureText(fillText, 14);
    int w5 = MeasureText(cellText, 14);
    int w6 = MeasureText("Filters: R G B O", 14);
    int w7 = MeasureText(helpText, 12);
    int maxW = w0;
    if (w1 > maxW) maxW = w1;
    if (w2 > maxW) maxW = w2;
    if (w3 > maxW) maxW = w3;
    if (w4 > maxW) maxW = w4;
    if (w5 > maxW) maxW = w5;
    if (w6 > maxW) maxW = w6;
    if (w7 > maxW) maxW = w7;

    int padding = 6;
    int boxW = maxW + padding * 2;
    int lineH = 16;
    int boxH = lineH * 8 + padding * 2;

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

    bool overfull = IsStockpileOverfull(spIdx);
    DrawTextShadow(storageText, tx + padding, y, 14, overfull ? RED : WHITE);
    y += 16;

    DrawTextShadow(fillText, tx + padding, y, 14, WHITE);
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
    DrawTextShadow(sp->allowedTypes[ITEM_ROCK] ? "O" : "-", fx, y, 14,
        sp->allowedTypes[ITEM_ROCK] ? ORANGE : DARKGRAY);
    fx += MeasureText("O", 14) + 4;
    DrawTextShadow(sp->allowedTypes[ITEM_STONE_BLOCKS] ? "S" : "-", fx, y, 14,
        sp->allowedTypes[ITEM_STONE_BLOCKS] ? GRAY : DARKGRAY);
    fx += MeasureText("S", 14) + 4;
    DrawTextShadow(sp->allowedTypes[ITEM_WOOD] ? "W" : "-", fx, y, 14,
        sp->allowedTypes[ITEM_WOOD] ? BROWN : DARKGRAY);
    fx += MeasureText("W", 14) + 4;
    bool saplingAllowed = sp->allowedTypes[ITEM_SAPLING_OAK] || sp->allowedTypes[ITEM_SAPLING_PINE] ||
                          sp->allowedTypes[ITEM_SAPLING_BIRCH] || sp->allowedTypes[ITEM_SAPLING_WILLOW];
    DrawTextShadow(saplingAllowed ? "T" : "-", fx, y, 14,
        saplingAllowed ? GREEN : DARKGRAY);
    fx += MeasureText("T", 14) + 4;
    DrawTextShadow(sp->allowedTypes[ITEM_DIRT] ? "D" : "-", fx, y, 14,
        sp->allowedTypes[ITEM_DIRT] ? BROWN : DARKGRAY);
    y += 18;

    DrawTextShadow(helpText, tx + padding, y, 12, GRAY);
}

// Draw mover debug tooltip (only shown when paused)
void DrawMoverTooltip(int moverIdx, Vector2 mouse) {
    if (moverIdx < 0 || moverIdx >= moverCount) return;
    Mover* m = &movers[moverIdx];
    if (!m->active) return;

    Job* job = (m->currentJobId >= 0) ? GetJob(m->currentJobId) : NULL;

    const char* jobTypeNames[] = {
        "NONE", "HAUL", "CLEAR", "MINE", "CHANNEL", "DIG_RAMP", "REMOVE_FLOOR", "HAUL_TO_BP",
        "BUILD", "CRAFT", "REMOVE_RAMP", "CHOP", "GATHER_SAPLING", "PLANT_SAPLING", "CHOP_FELLED"
    };
    int jobTypeCount = (int)(sizeof(jobTypeNames) / sizeof(jobTypeNames[0]));
    const char* jobTypeName = job ? (job->type < jobTypeCount ? jobTypeNames[job->type] : "?") : "IDLE";

    int carryingItem = job ? job->carryingItem : -1;
    int targetStockpile = job ? job->targetStockpile : -1;
    int targetSlotX = job ? job->targetSlotX : -1;
    int targetSlotY = job ? job->targetSlotY : -1;
    int targetItem = job ? job->targetItem : -1;
    int jobStep = job ? job->step : 0;

    // Build lines dynamically
    char lines[16][80];
    Color lineColors[16];
    int lineCount = 0;

    // Header
    snprintf(lines[lineCount], sizeof(lines[0]), "Mover #%d", moverIdx);
    lineColors[lineCount++] = YELLOW;

    // Position
    int cellX = (int)(m->x / CELL_SIZE);
    int cellY = (int)(m->y / CELL_SIZE);
    snprintf(lines[lineCount], sizeof(lines[0]), "Pos: (%.1f, %.1f, z%.0f) cell (%d,%d)", m->x, m->y, m->z, cellX, cellY);
    lineColors[lineCount++] = WHITE;

    // Job info
    snprintf(lines[lineCount], sizeof(lines[0]), "Job: %s (step %d)", jobTypeName, jobStep);
    lineColors[lineCount++] = job ? GREEN : GRAY;

    // Carrying item with name
    if (carryingItem >= 0 && items[carryingItem].active) {
        snprintf(lines[lineCount], sizeof(lines[0]), "Carrying: #%d (%s)", carryingItem, ItemName(items[carryingItem].type));
        lineColors[lineCount++] = ORANGE;
    } else {
        snprintf(lines[lineCount], sizeof(lines[0]), "Carrying: none");
        lineColors[lineCount++] = GRAY;
    }

    // Path info
    snprintf(lines[lineCount], sizeof(lines[0]), "Path: %d/%d, Goal: (%d,%d,z%d)",
        m->pathIndex >= 0 ? m->pathIndex + 1 : 0, m->pathLength, m->goal.x, m->goal.y, m->goal.z);
    lineColors[lineCount++] = m->pathLength > 0 ? WHITE : RED;

    // Repath status
    if (m->needsRepath) {
        snprintf(lines[lineCount], sizeof(lines[0]), "NEEDS REPATH (cooldown: %d)", m->repathCooldown);
        lineColors[lineCount++] = RED;
    } else if (m->repathCooldown > 0) {
        snprintf(lines[lineCount], sizeof(lines[0]), "Repath cooldown: %d", m->repathCooldown);
        lineColors[lineCount++] = GRAY;
    }

    // Stuck detection
    if (m->timeWithoutProgress > 0.5f) {
        snprintf(lines[lineCount], sizeof(lines[0]), "No progress: %.1fs%s", m->timeWithoutProgress,
            m->timeWithoutProgress > 2.0f ? " STUCK!" : "");
        lineColors[lineCount++] = m->timeWithoutProgress > 2.0f ? RED : ORANGE;
    }

    // Target stockpile (only if relevant)
    if (targetStockpile >= 0) {
        snprintf(lines[lineCount], sizeof(lines[0]), "Target SP: %d, Slot: (%d,%d)",
            targetStockpile, targetSlotX, targetSlotY);
        lineColors[lineCount++] = WHITE;
    }

    // Target item pickup info
    float pickupRadius = CELL_SIZE * 0.75f;
    if (job && targetItem >= 0 && items[targetItem].active) {
        Item* item = &items[targetItem];
        float dx = m->x - item->x;
        float dy = m->y - item->y;
        float dist = sqrtf(dx*dx + dy*dy);
        snprintf(lines[lineCount], sizeof(lines[0]), "Target: #%d %s at (%.0f,%.0f)", 
            targetItem, ItemName(item->type), item->x, item->y);
        lineColors[lineCount++] = SKYBLUE;
        
        snprintf(lines[lineCount], sizeof(lines[0]), "  dist=%.1f %s", 
            dist, dist < pickupRadius ? "IN RANGE" : "OUT OF RANGE");
        lineColors[lineCount++] = dist < pickupRadius ? GREEN : RED;
    }

    // Mining target
    if (job && (job->type == JOBTYPE_MINE || job->type == JOBTYPE_CHANNEL || job->type == JOBTYPE_REMOVE_FLOOR)) {
        snprintf(lines[lineCount], sizeof(lines[0]), "Mining: (%d,%d,z%d) %.0f%%",
            job->targetMineX, job->targetMineY, job->targetMineZ, job->progress * 100);
        lineColors[lineCount++] = ORANGE;
    }

    // Blueprint target
    if (job && job->targetBlueprint >= 0) {
        Blueprint* bp = &blueprints[job->targetBlueprint];
        snprintf(lines[lineCount], sizeof(lines[0]), "Blueprint: #%d at (%d,%d,z%d)",
            job->targetBlueprint, bp->x, bp->y, bp->z);
        lineColors[lineCount++] = SKYBLUE;
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

    int tx = (int)mouse.x + 15;
    int ty = (int)mouse.y + 15;
    if (tx + boxW > GetScreenWidth()) tx = (int)mouse.x - boxW - 5;
    if (ty + boxH > GetScreenHeight()) ty = (int)mouse.y - boxH - 5;

    DrawRectangle(tx, ty, boxW, boxH, (Color){20, 20, 40, 220});
    DrawRectangleLines(tx, ty, boxW, boxH, (Color){100, 100, 150, 255});

    int y = ty + padding;
    for (int i = 0; i < lineCount; i++) {
        DrawTextShadow(lines[i], tx + padding, y, 14, lineColors[i]);
        y += lineH;
    }
}

// Draw item tooltip (only shown when paused)
void DrawItemTooltip(int* itemIndices, int itemCount, Vector2 mouse, int cellX, int cellY) {
    if (itemCount <= 0) return;

    const char* stateNames[] = {"Ground", "Carried", "Stockpile"};

    char lines[17][64];
    snprintf(lines[0], sizeof(lines[0]), "Cell (%d,%d): %d item%s", cellX, cellY, itemCount, itemCount == 1 ? "" : "s");

    int lineCount = 1;
    for (int i = 0; i < itemCount && lineCount < 17; i++) {
        int idx = itemIndices[i];
        Item* item = &items[idx];
        const char* typeName = (item->type < ITEM_TYPE_COUNT) ? ItemName(item->type) : "?";
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
    const char* cellTypeName = CellName(ct);
    TreeType treeType = TREE_TYPE_NONE;
    TreePart treePart = TREE_PART_NONE;
    if (ct == CELL_TREE_TRUNK || ct == CELL_TREE_LEAVES || ct == CELL_SAPLING) {
        treeType = (TreeType)treeTypeGrid[cellZ][cellY][cellX];
        treePart = (TreePart)treePartGrid[cellZ][cellY][cellX];
    }
    bool isBurned = HAS_CELL_FLAG(cellX, cellY, cellZ, CELL_FLAG_BURNED);
    if (treeType != TREE_TYPE_NONE) {
        const char* partName = NULL;
        if (ct == CELL_TREE_TRUNK) {
            switch (treePart) {
                case TREE_PART_TRUNK: partName = "trunk"; break;
                case TREE_PART_BRANCH: partName = "branch"; break;
                case TREE_PART_ROOT: partName = "root"; break;
                case TREE_PART_FELLED: partName = "felled"; break;
                default: break;
            }
        }
        if (isBurned) {
            snprintf(lines[lineCount++], sizeof(lines[0]), "Type: %s (%s%s) [BURNED]",
                cellTypeName, TreeTypeName(treeType), partName ? ", " : "", partName ? partName : "");
        } else {
            snprintf(lines[lineCount++], sizeof(lines[0]), "Type: %s (%s%s)",
                cellTypeName, TreeTypeName(treeType), partName ? ", " : "", partName ? partName : "");
        }
    } else {
        if (isBurned) {
            snprintf(lines[lineCount++], sizeof(lines[0]), "Type: %s [BURNED]", cellTypeName);
        } else {
            snprintf(lines[lineCount++], sizeof(lines[0]), "Type: %s", cellTypeName);
        }
    }

    // Wall material (for constructed walls)
    MaterialType wallMat = GetWallMaterial(cellX, cellY, cellZ);
    if (wallMat != MAT_NONE && wallMat != MAT_RAW) {
        snprintf(lines[lineCount++], sizeof(lines[0]), "Wall: %s", MaterialName(wallMat));
    } else if (wallMat == MAT_RAW && CellBlocksMovement(ct)) {
        snprintf(lines[lineCount++], sizeof(lines[0]), "Wall: raw stone");
    }
    
    // Floor material
    MaterialType floorMat = GetFloorMaterial(cellX, cellY, cellZ);
    if (floorMat != MAT_NONE) {
        snprintf(lines[lineCount++], sizeof(lines[0]), "Floor: %s", MaterialName(floorMat));
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

    // Steam info
    SteamCell* steam = &steamGrid[cellZ][cellY][cellX];
    if (steam->level > 0) {
        snprintf(lines[lineCount++], sizeof(lines[0]), "Steam: %d/7", steam->level);
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
        else if (strstr(lines[i], "Steam:")) col = (Color){200, 200, 255, 255};
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

// Draw workshop tooltip showing bills and status
void DrawWorkshopTooltip(int wsIdx, Vector2 mouse) {
    if (wsIdx < 0 || wsIdx >= MAX_WORKSHOPS) return;
    Workshop* ws = &workshops[wsIdx];
    if (!ws->active) return;

    const char* workshopTypeNames[] = {"Stonecutter"};
    const char* billModeNames[] = {"Do X times", "Do until X", "Do forever"};

    char lines[12][80];
    int lineCount = 0;

    // Header
    const char* typeName = (ws->type < 1) ? workshopTypeNames[ws->type] : "Unknown";
    snprintf(lines[lineCount++], sizeof(lines[0]), "%s Workshop #%d", typeName, wsIdx);

    // Position info
    snprintf(lines[lineCount++], sizeof(lines[0]), "Position: (%d, %d, z%d)", ws->x, ws->y, ws->z);

    // Crafter status
    if (ws->assignedCrafter >= 0) {
        snprintf(lines[lineCount++], sizeof(lines[0]), "Crafter: Mover #%d", ws->assignedCrafter);
    } else {
        snprintf(lines[lineCount++], sizeof(lines[0]), "Crafter: None");
    }

    // Bills
    snprintf(lines[lineCount++], sizeof(lines[0]), "Bills: %d", ws->billCount);

    // Get recipes for this workshop type
    int recipeCount;
    Recipe* recipes = GetRecipesForWorkshop(ws->type, &recipeCount);

    for (int b = 0; b < ws->billCount && lineCount < 15; b++) {
        Bill* bill = &ws->bills[b];
        const char* recipeName = "Unknown";
        if (bill->recipeIdx >= 0 && bill->recipeIdx < recipeCount) {
            recipeName = recipes[bill->recipeIdx].name;
        }
        const char* modeName = (bill->mode < 3) ? billModeNames[bill->mode] : "?";
        
        char statusStr[32] = "";
        if (bill->suspended && bill->suspendedNoStorage) {
            snprintf(statusStr, sizeof(statusStr), " [NO STORAGE]");
        } else if (bill->suspended) {
            snprintf(statusStr, sizeof(statusStr), " [PAUSED]");
        } else if (bill->mode == BILL_DO_X_TIMES) {
            snprintf(statusStr, sizeof(statusStr), " (%d/%d)", bill->completedCount, bill->targetCount);
        } else if (bill->mode == BILL_DO_UNTIL_X) {
            snprintf(statusStr, sizeof(statusStr), " (until %d)", bill->targetCount);
        }

        snprintf(lines[lineCount++], sizeof(lines[0]), " %d. %s (%s)%s", 
            b + 1, recipeName, modeName, statusStr);
    }

    // Help text
    snprintf(lines[lineCount++], sizeof(lines[0]), "B:add  X:remove  P:pause  D:delete");

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
    DrawRectangle(tx, ty, boxW, boxH, (Color){40, 35, 30, 230});
    DrawRectangleLines(tx, ty, boxW, boxH, (Color){120, 100, 80, 255});

    // Draw lines with color coding
    int y = ty + padding;
    DrawTextShadow(lines[0], tx + padding, y, 14, YELLOW);  // Header
    y += lineH;
    DrawTextShadow(lines[1], tx + padding, y, 14, WHITE);   // Position
    y += lineH;
    DrawTextShadow(lines[2], tx + padding, y, 14, ws->assignedCrafter >= 0 ? GREEN : GRAY);  // Crafter
    y += lineH;
    DrawTextShadow(lines[3], tx + padding, y, 14, WHITE);   // Bills count
    y += lineH;
    
    // Bill lines
    int billLines = ws->billCount;
    for (int i = 0; i < billLines && (4 + i) < lineCount; i++) {
        Bill* bill = &ws->bills[i];
        Color col = (Color){200, 180, 140, 255};
        if (bill->suspended && bill->suspendedNoStorage) {
            col = ORANGE;  // Auto-suspended due to no storage
        } else if (bill->suspended) {
            col = RED;     // Manually paused
        }
        DrawTextShadow(lines[4 + i], tx + padding, y, 14, col);
        y += lineH;
    }
    
    // Help text (last line)
    if (lineCount > 4 + billLines) {
        DrawTextShadow(lines[4 + billLines], tx + padding, y, 14, GRAY);
    }
}

// Draw blueprint (construction) tooltip
void DrawBlueprintTooltip(int bpIdx, Vector2 mouse) {
    if (bpIdx < 0 || bpIdx >= MAX_BLUEPRINTS) return;
    Blueprint* bp = &blueprints[bpIdx];
    if (!bp->active) return;

    const char* stateNames[] = {"Awaiting materials", "Ready to build", "Building"};

    char lines[8][64];
    int lineCount = 0;

    // Header
    snprintf(lines[lineCount++], sizeof(lines[0]), "Construction (%d,%d,%d)", bp->x, bp->y, bp->z);

    // State
    const char* stateName = (bp->state < 3) ? stateNames[bp->state] : "?";
    snprintf(lines[lineCount++], sizeof(lines[0]), "Status: %s", stateName);

    // Materials
    snprintf(lines[lineCount++], sizeof(lines[0]), "Materials: %d/%d", bp->deliveredMaterialCount, bp->requiredMaterials);

    // What it's waiting for
    if (bp->state == BLUEPRINT_AWAITING_MATERIALS) {
        if (bp->reservedItem >= 0) {
            snprintf(lines[lineCount++], sizeof(lines[0]), "Item reserved: #%d", bp->reservedItem);
        } else {
            snprintf(lines[lineCount++], sizeof(lines[0]), "Waiting for hauler");
        }
    } else if (bp->state == BLUEPRINT_READY_TO_BUILD) {
        snprintf(lines[lineCount++], sizeof(lines[0]), "Waiting for builder");
    } else if (bp->state == BLUEPRINT_BUILDING) {
        if (bp->assignedBuilder >= 0) {
            snprintf(lines[lineCount++], sizeof(lines[0]), "Builder: Mover #%d", bp->assignedBuilder);
        }
        snprintf(lines[lineCount++], sizeof(lines[0]), "Progress: %d%%", (int)(bp->progress * 100));
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

    // Draw background (blue tint for construction)
    DrawRectangle(tx, ty, boxW, boxH, (Color){30, 35, 50, 230});
    DrawRectangleLines(tx, ty, boxW, boxH, (Color){80, 100, 150, 255});

    // Draw lines
    int y = ty + padding;
    for (int i = 0; i < lineCount; i++) {
        Color col = WHITE;
        if (i == 0) col = YELLOW;  // Header
        else if (strstr(lines[i], "Awaiting")) col = ORANGE;
        else if (strstr(lines[i], "Ready")) col = GREEN;
        else if (strstr(lines[i], "Building")) col = SKYBLUE;
        else if (strstr(lines[i], "Waiting")) col = GRAY;
        else if (strstr(lines[i], "Builder:")) col = GREEN;
        DrawTextShadow(lines[i], tx + padding, y, 14, col);
        y += lineH;
    }
}

// Draw designation tooltip (generic for all designation types)
void DrawDesignationTooltip(int cellX, int cellY, int cellZ, Vector2 mouse) {
    Designation* des = GetDesignation(cellX, cellY, cellZ);
    if (!des || des->type == DESIGNATION_NONE) return;

    // Get designation name and colors based on type
    const char* desName = "Unknown";
    Color bgColor = (Color){50, 40, 30, 230};
    Color borderColor = (Color){150, 120, 80, 255};
    const char* workerName = "Worker";
    
    switch (des->type) {
        case DESIGNATION_MINE:
            desName = "Mining";
            bgColor = (Color){40, 50, 60, 230};
            borderColor = (Color){100, 180, 220, 255};
            workerName = "Miner";
            break;
        case DESIGNATION_CHANNEL:
            desName = "Channeling";
            bgColor = (Color){50, 35, 45, 230};
            borderColor = (Color){220, 130, 180, 255};
            workerName = "Digger";
            break;
        case DESIGNATION_REMOVE_FLOOR:
            desName = "Remove Floor";
            bgColor = (Color){50, 45, 30, 230};
            borderColor = (Color){220, 190, 100, 255};
            workerName = "Worker";
            break;
        case DESIGNATION_REMOVE_RAMP:
            desName = "Remove Ramp";
            bgColor = (Color){35, 50, 50, 230};
            borderColor = (Color){100, 200, 200, 255};
            workerName = "Worker";
            break;
        case DESIGNATION_CHOP:
            desName = "Chop Tree";
            bgColor = (Color){50, 35, 25, 230};
            borderColor = (Color){200, 120, 60, 255};
            workerName = "Woodcutter";
            break;
        case DESIGNATION_CHOP_FELLED:
            desName = "Chop Felled";
            bgColor = (Color){45, 40, 30, 230};
            borderColor = (Color){190, 140, 70, 255};
            workerName = "Woodcutter";
            break;
        case DESIGNATION_GATHER_SAPLING:
            desName = "Gather Sapling";
            bgColor = (Color){35, 50, 35, 230};
            borderColor = (Color){150, 255, 150, 255};
            workerName = "Gatherer";
            break;
        case DESIGNATION_PLANT_SAPLING:
            desName = "Plant Sapling";
            bgColor = (Color){25, 45, 30, 230};
            borderColor = (Color){50, 180, 80, 255};
            workerName = "Planter";
            break;
        default:
            break;
    }

    char lines[6][64];
    int lineCount = 0;

    // Header
    snprintf(lines[lineCount++], sizeof(lines[0]), "%s (%d,%d,%d)", desName, cellX, cellY, cellZ);

    // Cell type at location
    CellType ct = grid[cellZ][cellY][cellX];
    const char* cellName = CellName(ct);
    snprintf(lines[lineCount++], sizeof(lines[0]), "Target: %s", cellName);

    // Assignment status
    if (des->assignedMover >= 0) {
        snprintf(lines[lineCount++], sizeof(lines[0]), "%s: Mover #%d", workerName, des->assignedMover);
        snprintf(lines[lineCount++], sizeof(lines[0]), "Progress: %d%%", (int)(des->progress * 100));
    } else if (des->unreachableCooldown > 0) {
        snprintf(lines[lineCount++], sizeof(lines[0]), "Unreachable (%.1fs)", des->unreachableCooldown);
    } else {
        snprintf(lines[lineCount++], sizeof(lines[0]), "Waiting for %s", workerName);
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
    DrawRectangle(tx, ty, boxW, boxH, bgColor);
    DrawRectangleLines(tx, ty, boxW, boxH, borderColor);

    // Draw lines
    int y = ty + padding;
    for (int i = 0; i < lineCount; i++) {
        Color col = WHITE;
        if (i == 0) col = borderColor;  // Header uses border color
        else if (strstr(lines[i], workerName)) col = GREEN;
        else if (strstr(lines[i], "Unreachable")) col = RED;
        else if (strstr(lines[i], "Waiting")) col = GRAY;
        DrawTextShadow(lines[i], tx + padding, y, 14, col);
        y += lineH;
    }
}

// Draw mining designation tooltip (legacy, calls generic)
void DrawMiningTooltip(int cellX, int cellY, int cellZ, Vector2 mouse) {
    DrawDesignationTooltip(cellX, cellY, cellZ, mouse);
}
