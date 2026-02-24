// render/tooltips.c - Tooltip drawing functions
#include "../game_state.h"
#include "../world/cell_defs.h"
#include "../world/material.h"
#include "../entities/workshops.h"
#include "../entities/furniture.h"
#include "../entities/item_defs.h"
#include "../entities/containers.h"
#include "../entities/jobs.h"
#include "../world/designations.h"
#include "../simulation/trees.h"
#include "../simulation/floordirt.h"
#include "../simulation/balance.h"

static void FormatItemName(const Item* item, char* out, size_t outSize) {
    const char* base = (item->type < ITEM_TYPE_COUNT) ? ItemName(item->type) : "?";
    MaterialType mat = (MaterialType)item->material;
    if (mat == MAT_NONE) {
        mat = (MaterialType)DefaultMaterialForItemType(item->type);
    }
    if (mat != MAT_NONE && ItemTypeUsesMaterialName(item->type)) {
        char matName[32];
        snprintf(matName, sizeof(matName), "%s", MaterialName(mat));
        if (matName[0] >= 'a' && matName[0] <= 'z') {
            matName[0] = (char)(matName[0] - 'a' + 'A');
        }
        snprintf(out, outSize, "%s %s", matName, base);
        return;
    }
    snprintf(out, outSize, "%s", base);
}

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
static void DrawStockpileTooltip(int spIdx, Vector2 mouse, Vector2 mouseGrid) {
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

    // Build tooltip text into local buffers (avoids TextFormat buffer clobbering)
    char titleBuf[40], priorityBuf[32], stackBuf[32], storageBuf[48], fillBuf[48];
    snprintf(titleBuf, sizeof(titleBuf), "Stockpile #%d", spIdx);
    snprintf(priorityBuf, sizeof(priorityBuf), "Priority: %d", sp->priority);
    snprintf(stackBuf, sizeof(stackBuf), "Stack size: %d", sp->maxStackSize);
    // Container info
    char containerBuf[48];
    int installedContainers = CountInstalledContainers(spIdx);
    if (sp->maxContainers > 0) {
        snprintf(containerBuf, sizeof(containerBuf), "Containers: %d/%d", installedContainers, sp->maxContainers);
    } else {
        containerBuf[0] = '\0';
    }
    snprintf(storageBuf, sizeof(storageBuf), "Storage: %d/%d (%d cells)", totalItems, maxCapacity, activeCells);
    char fillMeter[64];
    BuildFillMeter(fillMeter, sizeof(fillMeter), GetStockpileFillRatio(spIdx), 10);
    snprintf(fillBuf, sizeof(fillBuf), "Fill: %s", fillMeter);
    // Cell contents with item name — check if slot is a container
    char cellBuf[80];
    bool cellIsContainer = false;
    if (slotIdx >= 0 && slotIdx < totalSlots && IsSlotContainer(spIdx, slotIdx)) {
        cellIsContainer = true;
        int containerIdx = sp->slots[slotIdx];
        const ContainerDef* def = GetContainerDef(items[containerIdx].type);
        int maxC = def ? def->maxContents : 0;
        snprintf(cellBuf, sizeof(cellBuf), "Cell (%d,%d): %s %d/%d stacks",
                 cellX, cellY, ItemName(items[containerIdx].type), cellCount, maxC);
    } else if (cellCount > 0 && slotIdx >= 0 && slotIdx < totalSlots) {
        ItemType st = sp->slotTypes[slotIdx];
        uint8_t sm = sp->slotMaterials[slotIdx];
        if (sm == MAT_NONE) sm = DefaultMaterialForItemType(st);
        if (st < ITEM_TYPE_COUNT && sm != MAT_NONE && ItemTypeUsesMaterialName(st)) {
            snprintf(cellBuf, sizeof(cellBuf), "Cell (%d,%d): %d/%d %s %s",
                     cellX, cellY, cellCount, sp->maxStackSize, MaterialName(sm), ItemName(st));
        } else if (st < ITEM_TYPE_COUNT) {
            snprintf(cellBuf, sizeof(cellBuf), "Cell (%d,%d): %d/%d %s",
                     cellX, cellY, cellCount, sp->maxStackSize, ItemName(st));
        } else {
            snprintf(cellBuf, sizeof(cellBuf), "Cell (%d,%d): %d/%d items", cellX, cellY, cellCount, sp->maxStackSize);
        }
    } else {
        snprintf(cellBuf, sizeof(cellBuf), "Cell (%d,%d): %d/%d items", cellX, cellY, cellCount, sp->maxStackSize);
    }
    const char* helpText = activeFilterCategory < 0
        ? "+/- priority, [/] stack, X toggle all, 1-4 wood"
        : "A-F toggle, X all in category, ESC back";

    // Pre-build material entry strings
    char matEntries[8][16];
    int matEntryWidths[8];
    int filterGap = MeasureTextUI(" ", 14) * 2;
    for (int i = 0; i < STOCKPILE_MATERIAL_FILTER_COUNT && i < 8; i++) {
        snprintf(matEntries[i], sizeof(matEntries[0]), "%c:%s",
                 STOCKPILE_MATERIAL_FILTERS[i].key, STOCKPILE_MATERIAL_FILTERS[i].displayName);
        matEntryWidths[i] = MeasureTextUI(matEntries[i], 14);
    }

    // Count filter lines based on active category level
    int filterLines = 0;
    if (activeFilterCategory < 0) {
        // Top level: one line per category
        filterLines = FILTER_CAT_COUNT;
    } else {
        // Inside category: count items in this category
        for (int i = 0; i < STOCKPILE_FILTER_COUNT; i++) {
            if ((int)STOCKPILE_FILTERS[i].category == activeFilterCategory) filterLines++;
        }
    }

    int maxContentW = 300;
    bool showContainerLine = containerBuf[0] != '\0';
    int padding = 6;
    int lineH = 16;
    // header(7) + container? + "Filters:" + filterLines + "Wood:" + matRow + help
    int totalLines = 7 + (showContainerLine ? 1 : 0) + 1 + filterLines + 1 + 1 + 1;
    int boxW = maxContentW + padding * 2;
    int boxH = lineH * totalLines + padding * 2;

    // Position tooltip near mouse, keep on screen
    int tx = (int)mouse.x + 15;
    int ty = (int)mouse.y + 15;
    if (tx + boxW > GetScreenWidth()) tx = (int)mouse.x - boxW - 5;
    if (ty + boxH > GetScreenHeight()) ty = (int)mouse.y - boxH - 5;
    if (ty < 0) ty = 0;

    // Draw background
    DrawRectangle(tx, ty, boxW, boxH, (Color){20, 20, 20, 220});
    DrawRectangleLines(tx, ty, boxW, boxH, (Color){80, 80, 80, 255});

    // Draw header lines
    int y = ty + padding;
    DrawTextShadow(titleBuf, tx + padding, y, 14, YELLOW);
    y += lineH;

    DrawTextShadow(priorityBuf, tx + padding, y, 14, WHITE);
    y += lineH;

    DrawTextShadow(stackBuf, tx + padding, y, 14, WHITE);
    y += lineH;

    if (showContainerLine) {
        DrawTextShadow(containerBuf, tx + padding, y, 14, WHITE);
        y += lineH;
    }

    bool overfull = IsStockpileOverfull(spIdx);
    DrawTextShadow(storageBuf, tx + padding, y, 14, overfull ? RED : WHITE);
    y += lineH;

    DrawTextShadow(fillBuf, tx + padding, y, 14, WHITE);
    y += lineH;

    bool cellFull = false;
    if (cellIsContainer) {
        int ci = sp->slots[slotIdx];
        const ContainerDef* def = (ci >= 0 && ci < MAX_ITEMS) ? GetContainerDef(items[ci].type) : NULL;
        cellFull = def && cellCount >= def->maxContents;
    } else {
        cellFull = cellCount >= sp->maxStackSize;
    }
    DrawTextShadow(cellBuf, tx + padding, y, 14, cellFull ? ORANGE : WHITE);
    y += lineH;

    // Draw filters — two-level keyboard navigation
    if (activeFilterCategory < 0) {
        // Top level: show categories with keys
        DrawTextShadow("Filters:", tx + padding, y, 14, WHITE);
        y += lineH;

        for (StockpileFilterCategory cat = 0; cat < FILTER_CAT_COUNT; cat++) {
            int catTotal = 0, catEnabled = 0;
            for (int i = 0; i < STOCKPILE_FILTER_COUNT; i++) {
                if (STOCKPILE_FILTERS[i].category == cat) {
                    catTotal++;
                    if (sp->allowedTypes[STOCKPILE_FILTERS[i].itemType]) catEnabled++;
                }
            }
            if (catTotal == 0) continue;

            char catBuf[64];
            snprintf(catBuf, sizeof(catBuf), "%c: %s [%d/%d]",
                     FILTER_CATEGORY_KEYS[cat], FILTER_CATEGORY_NAMES[cat], catEnabled, catTotal);

            Color catColor = (catEnabled == catTotal) ? GREEN :
                             (catEnabled > 0) ? YELLOW : DARKGRAY;
            DrawTextShadow(catBuf, tx + padding, y, 14, catColor);
            y += lineH;
        }
    } else {
        // Inside category: show items with keys
        char headerBuf[64];
        snprintf(headerBuf, sizeof(headerBuf), "Filters: %s (ESC back)",
                 FILTER_CATEGORY_NAMES[activeFilterCategory]);
        DrawTextShadow(headerBuf, tx + padding, y, 14, WHITE);
        y += lineH;

        char itemKey = 'A';
        for (int i = 0; i < STOCKPILE_FILTER_COUNT; i++) {
            if ((int)STOCKPILE_FILTERS[i].category != activeFilterCategory) continue;

            bool allowed = sp->allowedTypes[STOCKPILE_FILTERS[i].itemType];
            char itemBuf[48];
            snprintf(itemBuf, sizeof(itemBuf), "%c: %s [%c]",
                     itemKey, STOCKPILE_FILTERS[i].displayName, allowed ? 'X' : ' ');

            Color itemColor = allowed ? STOCKPILE_FILTERS[i].color : DARKGRAY;
            DrawTextShadow(itemBuf, tx + padding, y, 14, itemColor);
            y += lineH;
            itemKey++;
        }
    }

    // Draw "Wood:" label + material entries
    DrawTextShadow("Wood:", tx + padding, y, 14, WHITE);
    y += lineH;
    {
        int mx = 0;
        for (int i = 0; i < STOCKPILE_MATERIAL_FILTER_COUNT; i++) {
            bool allowed = sp->allowedMaterials[STOCKPILE_MATERIAL_FILTERS[i].material];
            DrawTextShadow(matEntries[i], tx + padding + mx, y, 14,
                allowed ? STOCKPILE_MATERIAL_FILTERS[i].color : DARKGRAY);
            mx += matEntryWidths[i] + filterGap;
        }
        y += lineH;
    }

    DrawTextShadow(helpText, tx + padding, y, 12, GRAY);
}

// Draw mover debug tooltip (only shown when paused)
static void DrawMoverTooltip(int moverIdx, Vector2 mouse) {
    if (moverIdx < 0 || moverIdx >= moverCount) return;
    Mover* m = &movers[moverIdx];
    if (!m->active) return;

    Job* job = (m->currentJobId >= 0) ? GetJob(m->currentJobId) : NULL;

    const char* jobTypeName = job ? JobTypeName(job->type) : "IDLE";

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

    // Hunger
    {
        const char* hungerLabel;
        Color hungerColor;
        if (m->hunger > 0.5f) { hungerLabel = "Full"; hungerColor = GREEN; }
        else if (m->hunger > 0.3f) { hungerLabel = "Peckish"; hungerColor = YELLOW; }
        else if (m->hunger > 0.1f) { hungerLabel = "Hungry"; hungerColor = ORANGE; }
        else { hungerLabel = "Starving"; hungerColor = RED; }
        float hoursToStarve = (balance.hungerDrainPerGH > 0)
            ? m->hunger / balance.hungerDrainPerGH : 999.0f;
        snprintf(lines[lineCount], sizeof(lines[0]), "Hunger: %.0f%% (%s) — starving in %.1fh",
                 m->hunger * 100.0f, hungerLabel, hoursToStarve);
        lineColors[lineCount++] = hungerColor;

        if (m->freetimeState == FREETIME_SEEKING_FOOD) {
            snprintf(lines[lineCount], sizeof(lines[0]), "  Seeking food...");
            lineColors[lineCount++] = ORANGE;
        } else if (m->freetimeState == FREETIME_EATING) {
            snprintf(lines[lineCount], sizeof(lines[0]), "  Eating (%.0f%%)", (m->needProgress / 2.0f) * 100.0f);
            lineColors[lineCount++] = GREEN;
        }
    }

    // Energy
    {
        const char* energyLabel;
        Color energyColor;
        if (m->energy > 0.5f) { energyLabel = "Rested"; energyColor = GREEN; }
        else if (m->energy > 0.3f) { energyLabel = "Drowsy"; energyColor = YELLOW; }
        else if (m->energy > 0.1f) { energyLabel = "Tired"; energyColor = ORANGE; }
        else { energyLabel = "Exhausted"; energyColor = RED; }
        // Energy projection
        char energyProj[32] = "";
        if (m->freetimeState == FREETIME_RESTING) {
            float recoveryPerGH = balance.groundRecoveryPerGH;
            const char* restOn = "ground";
            if (m->needTarget >= 0 && m->needTarget < MAX_FURNITURE && furniture[m->needTarget].active) {
                recoveryPerGH = balance.bedRecoveryPerGH;
                restOn = "bed";
            }
            if (recoveryPerGH > 0) {
                float hoursToRested = (balance.energyWakeThreshold - m->energy) / recoveryPerGH;
                if (hoursToRested > 0) snprintf(energyProj, sizeof(energyProj), " — rested in %.1fh (%s)", hoursToRested, restOn);
            }
        } else {
            float drainPerGH = job ? balance.energyDrainWorkPerGH : balance.energyDrainIdlePerGH;
            if (drainPerGH > 0) {
                float hoursToTired = (m->energy - balance.energyTiredThreshold) / drainPerGH;
                if (hoursToTired > 0) snprintf(energyProj, sizeof(energyProj), " — tired in %.1fh", hoursToTired);
            }
        }
        snprintf(lines[lineCount], sizeof(lines[0]), "Energy: %.0f%% (%s)%s", m->energy * 100.0f, energyLabel, energyProj);
        lineColors[lineCount++] = energyColor;

        if (m->freetimeState == FREETIME_SEEKING_REST) {
            if (m->needTarget >= 0 && m->needTarget < MAX_FURNITURE && furniture[m->needTarget].active) {
                snprintf(lines[lineCount], sizeof(lines[0]), "  Seeking %s...",
                         GetFurnitureDef(furniture[m->needTarget].type)->name);
            } else {
                snprintf(lines[lineCount], sizeof(lines[0]), "  Seeking rest...");
            }
            lineColors[lineCount++] = ORANGE;
        } else if (m->freetimeState == FREETIME_RESTING) {
            if (m->needTarget >= 0 && m->needTarget < MAX_FURNITURE && furniture[m->needTarget].active) {
                snprintf(lines[lineCount], sizeof(lines[0]), "  Resting (%s)",
                         GetFurnitureDef(furniture[m->needTarget].type)->name);
            } else {
                snprintf(lines[lineCount], sizeof(lines[0]), "  Resting (ground)");
            }
            lineColors[lineCount++] = GREEN;
        } else if (m->freetimeState == FREETIME_SEEKING_WARMTH) {
            snprintf(lines[lineCount], sizeof(lines[0]), "  Seeking warmth...");
            lineColors[lineCount++] = ORANGE;
        } else if (m->freetimeState == FREETIME_WARMING) {
            snprintf(lines[lineCount], sizeof(lines[0]), "  Warming up by fire");
            lineColors[lineCount++] = YELLOW;
        }
    }

    // Body temperature
    {
        const char* tempLabel;
        Color tempColor;
        if (m->bodyTemp < balance.severeColdThreshold) { tempLabel = "Hypothermic"; tempColor = BLUE; }
        else if (m->bodyTemp < balance.moderateColdThreshold) { tempLabel = "Freezing"; tempColor = BLUE; }
        else if (m->bodyTemp < balance.mildColdThreshold) { tempLabel = "Cold"; tempColor = SKYBLUE; }
        else if (m->bodyTemp > balance.heatThreshold) { tempLabel = "Overheating"; tempColor = RED; }
        else { tempLabel = "Normal"; tempColor = GREEN; }
        snprintf(lines[lineCount], sizeof(lines[0]), "Body Temp: %.1fC (%s)", m->bodyTemp, tempLabel);
        lineColors[lineCount++] = tempColor;
    }

    // Job info
    snprintf(lines[lineCount], sizeof(lines[0]), "Job: %s (step %d)", jobTypeName, jobStep);
    lineColors[lineCount++] = job ? GREEN : GRAY;

    // Carrying item with name
    if (carryingItem >= 0 && items[carryingItem].active) {
        char itemName[64];
        FormatItemName(&items[carryingItem], itemName, sizeof(itemName));
        snprintf(lines[lineCount], sizeof(lines[0]), "Carrying: #%d (%s)", carryingItem, itemName);
        lineColors[lineCount++] = ORANGE;
    } else {
        snprintf(lines[lineCount], sizeof(lines[0]), "Carrying: none");
        lineColors[lineCount++] = GRAY;
    }

    // Equipped tool
    if (m->equippedTool >= 0 && m->equippedTool < MAX_ITEMS && items[m->equippedTool].active) {
        char toolName[64];
        FormatItemName(&items[m->equippedTool], toolName, sizeof(toolName));
        snprintf(lines[lineCount], sizeof(lines[0]), "Equipped: %s", toolName);
        lineColors[lineCount++] = SKYBLUE;
    } else {
        snprintf(lines[lineCount], sizeof(lines[0]), "Equipped: none");
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
        char itemName[64];
        FormatItemName(item, itemName, sizeof(itemName));
        float dx = m->x - item->x;
        float dy = m->y - item->y;
        float dist = sqrtf(dx*dx + dy*dy);
        snprintf(lines[lineCount], sizeof(lines[0]), "Target: #%d %s at (%.0f,%.0f)", 
            targetItem, itemName, item->x, item->y);
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

// Draw animal tooltip (only shown when paused)
static void DrawAnimalTooltip(int animalIdx, Vector2 mouse) {
    if (animalIdx < 0 || animalIdx >= animalCount) return;
    Animal* a = &animals[animalIdx];
    if (!a->active) return;

    const char* stateNames[] = { "Idle", "Walking", "Grazing", "Hunting", "Being Hunted" };
    const char* typeNames[] = { "Grazer", "Predator" };
    const char* behaviorNames[] = { "Simple", "Steering", "Predator" };

    char lines[10][80];
    Color lineColors[10];
    int lineCount = 0;

    snprintf(lines[lineCount], sizeof(lines[0]), "Animal #%d (%s, %s)", animalIdx, typeNames[a->type], behaviorNames[a->behavior]);
    lineColors[lineCount++] = YELLOW;

    int cellX = (int)(a->x / CELL_SIZE);
    int cellY = (int)(a->y / CELL_SIZE);
    snprintf(lines[lineCount], sizeof(lines[0]), "Pos: cell (%d,%d) z%.0f", cellX, cellY, a->z);
    lineColors[lineCount++] = WHITE;

    snprintf(lines[lineCount], sizeof(lines[0]), "State: %s", stateNames[a->state]);
    lineColors[lineCount++] = a->state == ANIMAL_GRAZING ? GREEN : (a->state == ANIMAL_WALKING ? SKYBLUE : GRAY);

    if (a->state == ANIMAL_GRAZING) {
        VegetationType veg = GetVegetation(cellX, cellY, (int)a->z);
        snprintf(lines[lineCount], sizeof(lines[0]), "Grazing: veg=%d, timer=%.1f/%.1f", veg, a->grazeTimer, ANIMAL_GRAZE_TIME);
        lineColors[lineCount++] = GREEN;
    } else if (a->state == ANIMAL_WALKING) {
        snprintf(lines[lineCount], sizeof(lines[0]), "Target: (%d,%d)", a->targetCellX, a->targetCellY);
        lineColors[lineCount++] = SKYBLUE;
    } else if (a->state == ANIMAL_HUNTING) {
        snprintf(lines[lineCount], sizeof(lines[0]), "Prey: %s", a->targetAnimalIdx >= 0 ? TextFormat("#%d", a->targetAnimalIdx) : "none");
        lineColors[lineCount++] = RED;
    }

    if (a->markedForHunt) {
        snprintf(lines[lineCount], sizeof(lines[0]), "Marked for Hunt");
        lineColors[lineCount++] = RED;
    }
    if (a->reservedByHunter >= 0) {
        snprintf(lines[lineCount], sizeof(lines[0]), "Hunter: Mover #%d", a->reservedByHunter);
        lineColors[lineCount++] = (Color){255, 100, 100, 255};
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

    DrawRectangle(tx, ty, boxW, boxH, (Color){20, 20, 40, 220});
    DrawRectangleLines(tx, ty, boxW, boxH, (Color){100, 100, 150, 255});

    int y = ty + padding;
    for (int i = 0; i < lineCount; i++) {
        DrawTextShadow(lines[i], tx + padding, y, 14, lineColors[i]);
        y += lineH;
    }
}

// Draw item tooltip (only shown when paused)
static void DrawItemTooltip(int* itemIndices, int itemCount, Vector2 mouse, int cellX, int cellY) {
    if (itemCount <= 0) return;

    const char* stateNames[] = {"Ground", "Carried", "Stockpile"};

    int totalUnits = 0;
    for (int i = 0; i < itemCount; i++) {
        totalUnits += items[itemIndices[i]].stackCount;
    }

    char lines[17][64];
    snprintf(lines[0], sizeof(lines[0]), "Cell (%d,%d): %d unit%s", cellX, cellY, totalUnits, totalUnits == 1 ? "" : "s");

    int lineCount = 1;
    for (int i = 0; i < itemCount && lineCount < 17; i++) {
        int idx = itemIndices[i];
        Item* item = &items[idx];
        char typeName[64];
        FormatItemName(item, typeName, sizeof(typeName));
        const char* stateName = (item->state >= 0 && item->state < 3) ? stateNames[item->state] : "?";
        if (item->stackCount > 1) {
            snprintf(lines[lineCount], sizeof(lines[lineCount]), "#%d: %s x%d (%s)", idx, typeName, item->stackCount, stateName);
        } else {
            snprintf(lines[lineCount], sizeof(lines[lineCount]), "#%d: %s (%s)", idx, typeName, stateName);
        }
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
static void DrawCellTooltip(int cellX, int cellY, int cellZ, Vector2 mouse) {
    if (cellX < 0 || cellX >= gridWidth || cellY < 0 || cellY >= gridHeight) return;
    if (cellZ < 0 || cellZ >= gridDepth) return;

    char lines[20][64];
    int lineCount = 0;

    // Header with coordinates
    snprintf(lines[lineCount++], sizeof(lines[0]), "Cell (%d, %d, z%d)", cellX, cellY, cellZ);

    // Cell type
    CellType ct = grid[cellZ][cellY][cellX];
    const char* cellTypeName = CellName(ct);
    MaterialType treeMat = MAT_NONE;
    
    // Check if this is a tree cell - get type from wall material
    if (ct == CELL_TREE_TRUNK || ct == CELL_TREE_BRANCH || ct == CELL_TREE_ROOT || 
        ct == CELL_TREE_FELLED || ct == CELL_TREE_LEAVES || ct == CELL_SAPLING) {
        treeMat = GetWallMaterial(cellX, cellY, cellZ);
    }
    
    bool isBurned = HAS_CELL_FLAG(cellX, cellY, cellZ, CELL_FLAG_BURNED);
    if (IsWoodMaterial(treeMat)) {
        // Cell type name already includes part info (trunk, branch, root, felled)
        if (isBurned) {
            snprintf(lines[lineCount++], sizeof(lines[0]), "Type: %s (%s) [BURNED]",
                cellTypeName, TreeTypeName(treeMat));
        } else {
            snprintf(lines[lineCount++], sizeof(lines[0]), "Type: %s (%s)",
                cellTypeName, TreeTypeName(treeMat));
        }
        // Show harvest state for trunk cells
        if (ct == CELL_TREE_TRUNK) {
            int baseZ = cellZ;
            while (baseZ > 0 && grid[baseZ - 1][cellY][cellX] == CELL_TREE_TRUNK) baseZ--;
            uint8_t hs = treeHarvestState[baseZ][cellY][cellX];
            if (hs > 0) {
                snprintf(lines[lineCount++], sizeof(lines[0]), "Harvest: %d/%d", hs, TREE_HARVEST_MAX);
            } else {
                snprintf(lines[lineCount++], sizeof(lines[0]), "Harvest: depleted");
            }
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
    if (wallMat != MAT_NONE && CellBlocksMovement(ct)) {
        if (IsWallNatural(cellX, cellY, cellZ)) {
            snprintf(lines[lineCount++], sizeof(lines[0]), "Wall: %s (natural)", MaterialName(wallMat));
        } else {
            snprintf(lines[lineCount++], sizeof(lines[0]), "Wall: %s", MaterialName(wallMat));
        }
    }
    // Floor material
    MaterialType floorMat = GetFloorMaterial(cellX, cellY, cellZ);
    if (floorMat != MAT_NONE) {
        if (IsFloorNatural(cellX, cellY, cellZ)) {
            snprintf(lines[lineCount++], sizeof(lines[0]), "Floor: %s (natural)", MaterialName(floorMat));
        } else {
            snprintf(lines[lineCount++], sizeof(lines[0]), "Floor: %s", MaterialName(floorMat));
        }
    }

    // Floor dirt level
    int dirtLevel = GetFloorDirt(cellX, cellY, cellZ);
    if (dirtLevel > 0) {
        const char* dirtDesc = dirtLevel >= DIRT_CLEAN_THRESHOLD ? "Dirty" :
                               dirtLevel >= DIRT_VISIBLE_THRESHOLD ? "Slightly dirty" : "Trace dirt";
        if (IsStoneMaterial(floorMat)) {
            snprintf(lines[lineCount++], sizeof(lines[0]), "Cleanliness: %s (%d, stone: 50%% rate)", dirtDesc, dirtLevel);
        } else {
            snprintf(lines[lineCount++], sizeof(lines[0]), "Cleanliness: %s (%d)", dirtDesc, dirtLevel);
        }
    }

    // Cell wetness / mud (check hovered cell and cell below)
    {
        int wetZ = cellZ;
        int wetness = GET_CELL_WETNESS(cellX, cellY, wetZ);
        if (wetness == 0 && wetZ > 0) {
            wetZ = cellZ - 1;
            wetness = GET_CELL_WETNESS(cellX, cellY, wetZ);
        }
        if (wetness > 0) {
            bool muddy = IsMuddy(cellX, cellY, wetZ);
            const char* wetDesc = wetness >= 3 ? (muddy ? "Soaked (muddy)" : "Soaked") :
                                  wetness >= 2 ? (muddy ? "Wet (muddy)" : "Wet") : "Damp";
            snprintf(lines[lineCount++], sizeof(lines[0]), "Wetness: %s", wetDesc);
        }
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
static void DrawWaterTooltip(int cellX, int cellY, int cellZ, Vector2 mouse) {
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
static void DrawWorkshopTooltip(int wsIdx, Vector2 mouse) {
    if (wsIdx < 0 || wsIdx >= MAX_WORKSHOPS) return;
    Workshop* ws = &workshops[wsIdx];
    if (!ws->active) return;

    const char* billModeNames[] = {"Do X times", "Do until X", "Do forever"};

    char lines[30][80];
    Color lineColors[30];
    int lineCount = 0;

    // Header
    const char* typeName = (ws->type < WORKSHOP_TYPE_COUNT) ? workshopDefs[ws->type].displayName : "Unknown";
    snprintf(lines[lineCount], sizeof(lines[0]), "%s Workshop #%d", typeName, wsIdx);
    lineColors[lineCount] = YELLOW;
    lineCount++;

    // Position info
    snprintf(lines[lineCount], sizeof(lines[0]), "Position: (%d, %d, z%d)", ws->x, ws->y, ws->z);
    lineColors[lineCount] = WHITE;
    lineCount++;

    // Deconstruction status
    if (ws->markedForDeconstruct && lineCount < 28) {
        if (ws->assignedDeconstructor >= 0) {
            snprintf(lines[lineCount], sizeof(lines[0]), "DECONSTRUCTING (Mover #%d)", ws->assignedDeconstructor);
        } else {
            snprintf(lines[lineCount], sizeof(lines[0]), "MARKED FOR DECONSTRUCTION");
        }
        lineColors[lineCount] = RED;
        lineCount++;
    }

    // Crafter status
    if (ws->assignedCrafter >= 0) {
        snprintf(lines[lineCount], sizeof(lines[0]), "Crafter: Mover #%d", ws->assignedCrafter);
        lineColors[lineCount] = GREEN;
    } else {
        snprintf(lines[lineCount], sizeof(lines[0]), "Crafter: None");
        lineColors[lineCount] = GRAY;
    }
    lineCount++;

    // Workshop status diagnostics (only show for problem states)
    if (lineCount < 28 && ws->visualState != WORKSHOP_VISUAL_WORKING) {
        const char* statusText = NULL;
        Color statusColor = WHITE;
        
        switch (ws->visualState) {
            case WORKSHOP_VISUAL_OUTPUT_FULL:
                statusText = "Status: Output Blocked";
                statusColor = (Color){255, 100, 100, 255};
                break;
            case WORKSHOP_VISUAL_INPUT_EMPTY:
                statusText = "Status: Waiting for Input";
                statusColor = (Color){255, 200, 50, 255};
                break;
            case WORKSHOP_VISUAL_NO_WORKER:
                statusText = "Status: No Worker Assigned";
                statusColor = (Color){150, 150, 150, 255};
                break;
            case WORKSHOP_VISUAL_WORKING:
                // No status line when working - silence is golden
                break;
        }
        
        if (statusText) {
            snprintf(lines[lineCount], sizeof(lines[0]), "%s", statusText);
            lineColors[lineCount] = statusColor;
            lineCount++;
        }
        
        // Show specific details for blocked/waiting states
        if (ws->visualState == WORKSHOP_VISUAL_OUTPUT_FULL && lineCount < 28) {
            // Check which items are blocking output
            if (ws->billCount > 0) {
                int recipeCount;
                const Recipe* recipes = GetRecipesForWorkshop(ws->type, &recipeCount);
                Bill* bill = &ws->bills[0];  // Check first active bill
                if (bill->recipeIdx >= 0 && bill->recipeIdx < recipeCount) {
                    const Recipe* recipe = &recipes[bill->recipeIdx];
                    
                    if (recipe->outputType != ITEM_NONE) {
                        snprintf(lines[lineCount], sizeof(lines[0]), 
                            "  Output blocked: %s (x%d)",
                            ItemName(recipe->outputType), recipe->outputCount);
                        lineColors[lineCount] = (Color){255, 150, 100, 255};
                        lineCount++;
                        
                        if (lineCount < 28) {
                            snprintf(lines[lineCount], sizeof(lines[0]), 
                                "  Hint: Build stockpile accepting %s", ItemName(recipe->outputType));
                            lineColors[lineCount] = (Color){200, 200, 100, 255};
                            lineCount++;
                        }
                    }
                }
            }
        } else if (ws->visualState == WORKSHOP_VISUAL_INPUT_EMPTY && lineCount < 28) {
            // Show specifically what materials are missing
            if (ws->billCount > 0) {
                int recipeCount;
                const Recipe* recipes = GetRecipesForWorkshop(ws->type, &recipeCount);
                Bill* bill = &ws->bills[0];  // Check first active bill
                if (bill->recipeIdx >= 0 && bill->recipeIdx < recipeCount) {
                    const Recipe* recipe = &recipes[bill->recipeIdx];
                    
                    // Check input 1
                    bool hasInput1 = false;
                    const char* input1Name = (recipe->inputItemMatch == ITEM_MATCH_ANY_FUEL)
                        ? "Any Fuel" : ItemName(recipe->inputType);
                    
                    for (int i = 0; i < itemHighWaterMark; i++) {
                        Item* item = &items[i];
                        if (!item->active || item->reservedBy != -1) continue;
                        if ((int)item->z != ws->z) continue;
                        if (RecipeInputMatches(recipe, item)) {
                            hasInput1 = true;
                            break;
                        }
                    }
                    
                    if (!hasInput1 && lineCount < 28) {
                        snprintf(lines[lineCount], sizeof(lines[0]), 
                            "  Waiting for: %s (x%d)", input1Name, recipe->inputCount);
                        lineColors[lineCount] = (Color){255, 200, 100, 255};
                        lineCount++;
                    }
                    
                    // Check input 2 if needed
                    if (recipe->inputType2 != ITEM_NONE) {
                        bool hasInput2 = false;
                        for (int i = 0; i < itemHighWaterMark; i++) {
                            Item* item = &items[i];
                            if (!item->active || item->reservedBy != -1) continue;
                            if ((int)item->z != ws->z) continue;
                            if (item->type == recipe->inputType2) {
                                hasInput2 = true;
                                break;
                            }
                        }
                        
                        if (!hasInput2 && lineCount < 28) {
                            snprintf(lines[lineCount], sizeof(lines[0]), 
                                "  Waiting for: %s (x%d)", 
                                ItemName(recipe->inputType2), recipe->inputCount2);
                            lineColors[lineCount] = (Color){255, 200, 100, 255};
                            lineCount++;
                        }
                    }
                    
                    // Check input 3 if needed
                    if (recipe->inputType3 != ITEM_NONE) {
                        bool hasInput3 = false;
                        for (int i = 0; i < itemHighWaterMark; i++) {
                            Item* item = &items[i];
                            if (!item->active || item->reservedBy != -1) continue;
                            if ((int)item->z != ws->z) continue;
                            if (item->type == recipe->inputType3) {
                                hasInput3 = true;
                                break;
                            }
                        }
                        
                        if (!hasInput3 && lineCount < 28) {
                            snprintf(lines[lineCount], sizeof(lines[0]), 
                                "  Waiting for: %s (x%d)", 
                                ItemName(recipe->inputType3), recipe->inputCount3);
                            lineColors[lineCount] = (Color){255, 200, 100, 255};
                            lineCount++;
                        }
                    }

                    // Check fuel if needed
                    if (recipe->fuelRequired > 0) {
                        bool hasFuel = WorkshopHasFuelForRecipe(ws, 100);
                        if (!hasFuel && lineCount < 28) {
                            snprintf(lines[lineCount], sizeof(lines[0]), 
                                "  Waiting for: Fuel (any burnable item)");
                            lineColors[lineCount] = (Color){255, 200, 100, 255};
                            lineCount++;
                        }
                    }
                }
            }
        }
    }

    // Passive workshop status
    if (workshopDefs[ws->type].passive) {
        if (ws->passiveReady) {
            snprintf(lines[lineCount], sizeof(lines[0]), "Status: Burning (%.0f%%)", ws->passiveProgress * 100.0f);
            lineColors[lineCount] = ORANGE;
        } else {
            snprintf(lines[lineCount], sizeof(lines[0]), "Status: Not ignited");
            lineColors[lineCount] = GRAY;
        }
        lineCount++;
    }

    // Get recipes for this workshop type
    int recipeCount;
    const Recipe* recipes = GetRecipesForWorkshop(ws->type, &recipeCount);

    // Active bills
    if (ws->billCount > 0) {
        snprintf(lines[lineCount], sizeof(lines[0]), "Bills:");
        lineColors[lineCount] = WHITE;
        lineCount++;

        for (int b = 0; b < ws->billCount && lineCount < 24; b++) {
            Bill* bill = &ws->bills[b];
            const char* recipeName = "Unknown";
            const Recipe* recipe = NULL;
            if (bill->recipeIdx >= 0 && bill->recipeIdx < recipeCount) {
                recipe = &recipes[bill->recipeIdx];
                recipeName = recipe->name;
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
                int have = recipe ? CountItemsInStockpiles(recipe->outputType) : 0;
                snprintf(statusStr, sizeof(statusStr), " (%d/%d)", have, bill->targetCount);
            }

            char selChar = (b == workshopSelectedBillIdx) ? '>' : ' ';
            snprintf(lines[lineCount], sizeof(lines[0]), "%c%d. %s (%s)%s", 
                selChar, b + 1, recipeName, modeName, statusStr);
            if (bill->suspended && bill->suspendedNoStorage) {
                lineColors[lineCount] = ORANGE;
            } else if (bill->suspended) {
                lineColors[lineCount] = RED;
            } else {
                lineColors[lineCount] = (Color){200, 180, 140, 255};
            }
            lineCount++;

            // Show why bill can't run (missing input or fuel)
            if (recipe && !bill->suspended && ws->assignedCrafter < 0 && lineCount < 24) {
                bool hasInput = false;
                for (int i = 0; i < itemHighWaterMark; i++) {
                    Item* item = &items[i];
                    if (!item->active) continue;
                    if ((int)item->z != ws->z) continue;
                    if (RecipeInputMatches(recipe, item)) { hasInput = true; break; }
                }
                bool hasInput2 = (recipe->inputType2 == ITEM_NONE);  // true if not needed
                if (!hasInput2) {
                    for (int i = 0; i < itemHighWaterMark; i++) {
                        Item* item = &items[i];
                        if (!item->active) continue;
                        if ((int)item->z != ws->z) continue;
                        if (item->type == recipe->inputType2) { hasInput2 = true; break; }
                    }
                }
                bool hasInput3 = (recipe->inputType3 == ITEM_NONE);  // true if not needed
                if (!hasInput3) {
                    for (int i = 0; i < itemHighWaterMark; i++) {
                        Item* item = &items[i];
                        if (!item->active) continue;
                        if ((int)item->z != ws->z) continue;
                        if (item->type == recipe->inputType3) { hasInput3 = true; break; }
                    }
                }
                bool needsFuel = recipe->fuelRequired > 0;
                bool hasFuel = !needsFuel || WorkshopHasFuelForRecipe(ws, 100);

                // Build "Needs:" string with missing items
                if (!hasInput || !hasInput2 || !hasInput3 || !hasFuel) {
                    char needsStr[128] = "    Needs:";
                    bool hasPrev = false;
                    if (!hasInput) {
                        strcat(needsStr, " ");
                        const char* inputName = (recipe->inputItemMatch == ITEM_MATCH_ANY_FUEL)
                            ? "Any Fuel" : ItemName(recipe->inputType);
                        strcat(needsStr, inputName);
                        hasPrev = true;
                    }
                    if (!hasInput2) {
                        strcat(needsStr, hasPrev ? " +" : " ");
                        strcat(needsStr, ItemName(recipe->inputType2));
                        hasPrev = true;
                    }
                    if (!hasInput3) {
                        strcat(needsStr, hasPrev ? " +" : " ");
                        strcat(needsStr, ItemName(recipe->inputType3));
                        hasPrev = true;
                    }
                    if (!hasFuel) {
                        strcat(needsStr, hasPrev ? " + fuel" : " fuel");
                    }
                    snprintf(lines[lineCount], sizeof(lines[0]), "%s", needsStr);
                    lineColors[lineCount] = (Color){255, 120, 120, 255};
                    lineCount++;
                }
            }
        }
    }

    // Linked stockpiles section
    if (lineCount < 26) {
        snprintf(lines[lineCount], sizeof(lines[0]), "Linked Input Stockpiles:");
        lineColors[lineCount] = WHITE;
        lineCount++;
        
        if (ws->linkedInputCount == 0) {
            snprintf(lines[lineCount], sizeof(lines[0]), "  (none) - Press L to link");
            lineColors[lineCount] = GRAY;
            lineCount++;
        } else {
            for (int i = 0; i < ws->linkedInputCount && lineCount < 26; i++) {
                int spIdx = ws->linkedInputStockpiles[i];
                if (spIdx >= 0 && spIdx < stockpileCount && stockpiles[spIdx].active) {
                    Stockpile* sp = &stockpiles[spIdx];
                    snprintf(lines[lineCount], sizeof(lines[0]), 
                             "  [%d] Stockpile #%d (Pri:%d)", 
                             i + 1, spIdx, sp->priority);
                    lineColors[lineCount] = GREEN;
                } else {
                    snprintf(lines[lineCount], sizeof(lines[0]), 
                             "  [%d] <invalid #%d>", i + 1, spIdx);
                    lineColors[lineCount] = RED;
                }
                lineCount++;
            }
        }
        
        lineCount++;  // Blank line before recipes
    }

    // Available recipes (add with number keys)
    if (recipeCount > 0 && lineCount < 26) {
        snprintf(lines[lineCount], sizeof(lines[0]), "Add recipe:");
        lineColors[lineCount] = WHITE;
        lineCount++;
        for (int r = 0; r < recipeCount && r < 9 && lineCount < 28; r++) {
            const char* inputName = (recipes[r].inputItemMatch == ITEM_MATCH_ANY_FUEL)
                ? "Any Fuel" : ItemName(recipes[r].inputType);
            char outputStr[64];
            if (recipes[r].outputType == ITEM_NONE) {
                snprintf(outputStr, sizeof(outputStr), "Products");
            } else if (recipes[r].outputType2 != ITEM_NONE) {
                snprintf(outputStr, sizeof(outputStr), "%s+%s",
                    ItemName(recipes[r].outputType), ItemName(recipes[r].outputType2));
            } else {
                snprintf(outputStr, sizeof(outputStr), "%s", ItemName(recipes[r].outputType));
            }
            if (recipes[r].inputType3 != ITEM_NONE) {
                snprintf(lines[lineCount], sizeof(lines[0]), " %d: %s (%s+%s+%s -> %s)",
                    r + 1, recipes[r].name, inputName,
                    ItemName(recipes[r].inputType2),
                    ItemName(recipes[r].inputType3), outputStr);
            } else if (recipes[r].inputType2 != ITEM_NONE) {
                snprintf(lines[lineCount], sizeof(lines[0]), " %d: %s (%s+%s -> %s)",
                    r + 1, recipes[r].name, inputName,
                    ItemName(recipes[r].inputType2), outputStr);
            } else {
                snprintf(lines[lineCount], sizeof(lines[0]), " %d: %s (%s -> %s)",
                    r + 1, recipes[r].name, inputName, outputStr);
            }
            lineColors[lineCount] = (Color){140, 180, 200, 255};
            lineCount++;
        }
    }

    // Help text
    snprintf(lines[lineCount], sizeof(lines[0]), "L:link X:del P:pause M:mode +/-:count []:sel D:del ws");
    lineColors[lineCount] = GRAY;
    lineCount++;

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

    // Draw lines with stored colors
    int y = ty + padding;
    for (int i = 0; i < lineCount; i++) {
        DrawTextShadow(lines[i], tx + padding, y, 14, lineColors[i]);
        y += lineH;
    }
}

// Draw blueprint (construction) tooltip
static void DrawBlueprintTooltip(int bpIdx, Vector2 mouse) {
    if (bpIdx < 0 || bpIdx >= MAX_BLUEPRINTS) return;
    Blueprint* bp = &blueprints[bpIdx];
    if (!bp->active) return;

    const char* stateNames[] = {"Awaiting materials", "Ready to build", "Building", "Clearing"};

    char lines[16][64];
    int lineCount = 0;

    // Header
    const ConstructionRecipe* headerRecipe = GetConstructionRecipe(bp->recipeIndex);
    const char* name = headerRecipe ? headerRecipe->name : "Construction";
    snprintf(lines[lineCount++], sizeof(lines[0]), "%s (%d,%d,%d)", name, bp->x, bp->y, bp->z);

    // State
    const char* stateName = (bp->state < 4) ? stateNames[bp->state] : "?";
    snprintf(lines[lineCount++], sizeof(lines[0]), "Status: %s", stateName);

    // Per-slot material detail for current stage
    if (headerRecipe && bp->stage < headerRecipe->stageCount) {
        const ConstructionStage* stage = &headerRecipe->stages[bp->stage];
        for (int s = 0; s < stage->inputCount && lineCount < 14; s++) {
            const ConstructionInput* input = &stage->inputs[s];
            StageDelivery* sd = &bp->stageDeliveries[s];
            const char* itemName;
            if (input->anyBuildingMat) {
                itemName = "Building Mat";
            } else if (input->altCount == 1) {
                itemName = ItemName(input->alternatives[0].itemType);
            } else {
                // Multiple alternatives — show first with "/.."
                itemName = ItemName(input->alternatives[0].itemType);
            }
            snprintf(lines[lineCount++], sizeof(lines[0]), "  %s: %d/%d",
                     itemName, sd->deliveredCount, input->count);
        }
    }

    // Show stage info for multi-stage recipes
    if (headerRecipe && headerRecipe->stageCount > 1) {
        snprintf(lines[lineCount++], sizeof(lines[0]), "Stage: %d/%d", bp->stage + 1, headerRecipe->stageCount);
    }

    // What it's waiting for
    if (bp->state == BLUEPRINT_CLEARING) {
        snprintf(lines[lineCount++], sizeof(lines[0]), "Clearing items from site");
    } else if (bp->state == BLUEPRINT_AWAITING_MATERIALS) {
        snprintf(lines[lineCount++], sizeof(lines[0]), "Waiting for hauler");
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
        else if (lines[i][0] == ' ' && lines[i][1] == ' ') col = (Color){180, 200, 255, 255};  // Material lines
        else if (strstr(lines[i], "Requires:")) col = SKYBLUE;
        else if (strstr(lines[i], "Awaiting")) col = ORANGE;
        else if (strstr(lines[i], "Clearing")) col = ORANGE;
        else if (strstr(lines[i], "Ready")) col = GREEN;
        else if (strstr(lines[i], "Building")) col = SKYBLUE;
        else if (strstr(lines[i], "Waiting")) col = GRAY;
        else if (strstr(lines[i], "Builder:")) col = GREEN;
        DrawTextShadow(lines[i], tx + padding, y, 14, col);
        y += lineH;
    }
}

// Draw designation tooltip (generic for all designation types)
static void DrawDesignationTooltip(int cellX, int cellY, int cellZ, Vector2 mouse) {
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
        case DESIGNATION_GATHER_GRASS:
            desName = "Gather Grass";
            bgColor = (Color){45, 50, 25, 230};
            borderColor = (Color){200, 230, 100, 255};
            workerName = "Gatherer";
            break;
        case DESIGNATION_GATHER_TREE:
            desName = "Gather Tree";
            bgColor = (Color){40, 35, 20, 230};
            borderColor = (Color){180, 140, 80, 255};
            workerName = "Gatherer";
            break;
        default:
            break;
    }

    char lines[7][80];
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

        // Show tool requirement hint for hard-gated jobs
        if (toolRequirementsEnabled) {
            static const char* qualityNames[] = {
                [QUALITY_CUTTING]   = "cutting",
                [QUALITY_HAMMERING] = "hammering",
                [QUALITY_DIGGING]   = "digging",
                [QUALITY_SAWING]    = "sawing",
                [QUALITY_FINE]      = "fine",
            };
            // Map designation type to job type
            int jobType = -1;
            switch (des->type) {
                case DESIGNATION_MINE:        jobType = JOBTYPE_MINE; break;
                case DESIGNATION_CHANNEL:     jobType = JOBTYPE_CHANNEL; break;
                case DESIGNATION_DIG_RAMP:    jobType = JOBTYPE_DIG_RAMP; break;
                case DESIGNATION_CHOP:        jobType = JOBTYPE_CHOP; break;
                case DESIGNATION_CHOP_FELLED: jobType = JOBTYPE_CHOP_FELLED; break;
                default: break;
            }
            if (jobType >= 0) {
                MaterialType mat = GetWallMaterial(cellX, cellY, cellZ);
                // Channel checks z-1 material
                if (des->type == DESIGNATION_CHANNEL && cellZ > 0)
                    mat = GetWallMaterial(cellX, cellY, cellZ - 1);
                JobToolReq req = GetJobToolRequirement(jobType, mat);
                if (req.hasRequirement && !req.isSoft && req.minLevel > 0) {
                    snprintf(lines[lineCount++], sizeof(lines[0]),
                             "Needs %s:%d tool", qualityNames[req.qualityType], req.minLevel);
                }
            }
        }
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
