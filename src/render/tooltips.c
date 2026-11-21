// render/tooltips.c - Tooltip drawing functions
#include "../game_state.h"
#include "../world/cell_defs.h"
#include "../world/material.h"
#include "../entities/workshops.h"
#include "../entities/furniture.h"
#include "../entities/item_defs.h"
#include "../entities/containers.h"
#include "../entities/jobs.h"
#include "../simulation/farming.h"
#include "../world/designations.h"
#include "../simulation/trees.h"
#include "../simulation/floordirt.h"
#include "../simulation/balance.h"
#include "../simulation/mood.h"
#include "../simulation/rooms.h"
#include "../entities/trains.h"
#include "../core/describe.h"


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

// Description tones (src/core/describe.h) as tooltip colors
Color DescToneColor(DescTone tone) {
    switch (tone) {
        case DESC_NORMAL:        return WHITE;
        case DESC_MUTED:         return GRAY;
        case DESC_GOOD:          return GREEN;
        case DESC_GREAT:         return (Color){100, 255, 100, 255};
        case DESC_MILD:          return YELLOW;
        case DESC_WARN:          return ORANGE;
        case DESC_BAD:           return RED;
        case DESC_INFO:          return SKYBLUE;
        case DESC_COLD:          return BLUE;
        case DESC_WATER:         return (Color){80, 140, 220, 255};
        case DESC_HEADER_FEMALE: return (Color){255, 150, 180, 255};
        case DESC_HEADER_MALE:   return (Color){130, 180, 255, 255};
        case DESC_TITLE:         return YELLOW;
        case DESC_SOFT_BAD:      return (Color){255, 100, 100, 255};
        case DESC_SOFT_WARN:     return (Color){255, 200, 100, 255};
        case DESC_BLUE:          return (Color){100, 150, 255, 255};
        case DESC_ICE:           return (Color){200, 220, 255, 255};
        case DESC_STEAM:         return (Color){200, 200, 255, 255};
        case DESC_BURNT:         return (Color){80, 60, 40, 255};
    }
    return WHITE;
}

// Tooltip box styles: background, border, and optionally the first line's color
// (title.a == 0: the first line keeps its tone's color)
typedef struct { Color bg, border, title; } DescBoxStyle;
static const DescBoxStyle DESC_BOX_DEFAULT   = {{20, 20, 40, 220}, {100, 100, 150, 255}, {0}};
static const DescBoxStyle DESC_BOX_ITEMS     = {{40, 30, 20, 220}, {150, 100, 50, 255}, {0}};
static const DescBoxStyle DESC_BOX_CELL      = {{30, 30, 30, 230}, {80, 80, 80, 255}, {0}};
static const DescBoxStyle DESC_BOX_WATER     = {{20, 40, 60, 220}, {50, 100, 200, 255}, {100, 180, 255, 255}};
static const DescBoxStyle DESC_BOX_BLUEPRINT = {{30, 35, 50, 230}, {80, 100, 150, 255}, {0}};
static const DescBoxStyle DESC_BOX_STATION   = {{20, 20, 30, 220}, {80, 200, 255, 255}, {80, 200, 255, 255}};

// Designations: each kind has its own box; the title takes the border color
static DescBoxStyle DesignationBoxStyle(int type) {
    Color bg = {50, 40, 30, 230}, border = {150, 120, 80, 255};
    switch (type) {
        case DESIGNATION_MINE:           bg = (Color){40, 50, 60, 230}; border = (Color){100, 180, 220, 255}; break;
        case DESIGNATION_CHANNEL:        bg = (Color){50, 35, 45, 230}; border = (Color){220, 130, 180, 255}; break;
        case DESIGNATION_REMOVE_FLOOR:   bg = (Color){50, 45, 30, 230}; border = (Color){220, 190, 100, 255}; break;
        case DESIGNATION_REMOVE_RAMP:    bg = (Color){35, 50, 50, 230}; border = (Color){100, 200, 200, 255}; break;
        case DESIGNATION_CHOP:           bg = (Color){50, 35, 25, 230}; border = (Color){200, 120, 60, 255};  break;
        case DESIGNATION_CHOP_FELLED:    bg = (Color){45, 40, 30, 230}; border = (Color){190, 140, 70, 255};  break;
        case DESIGNATION_GATHER_SAPLING: bg = (Color){35, 50, 35, 230}; border = (Color){150, 255, 150, 255}; break;
        case DESIGNATION_PLANT_SAPLING:  bg = (Color){25, 45, 30, 230}; border = (Color){50, 180, 80, 255};   break;
        case DESIGNATION_GATHER_GRASS:   bg = (Color){45, 50, 25, 230}; border = (Color){200, 230, 100, 255}; break;
        case DESIGNATION_DIG_ROOTS:      bg = (Color){50, 40, 20, 230}; border = (Color){160, 120, 60, 255};  break;
        case DESIGNATION_GATHER_TREE:    bg = (Color){40, 35, 20, 230}; border = (Color){180, 140, 80, 255};  break;
        default: break;
    }
    return (DescBoxStyle){bg, border, border};
}

static void DrawDescTooltipStyled(const DescLines* d, Vector2 mouse, DescBoxStyle style);

// Tooltip box next to the mouse for a description, one line per row
static void DrawDescTooltip(const DescLines* d, Vector2 mouse) {
    DrawDescTooltipStyled(d, mouse, DESC_BOX_DEFAULT);
}

static void DrawDescTooltipStyled(const DescLines* d, Vector2 mouse, DescBoxStyle style) {
    if (d->count == 0) return;
    int maxW = 0;
    for (int i = 0; i < d->count; i++) {
        int w = MeasureText(d->lines[i].text, 14);
        if (w > maxW) maxW = w;
    }

    int padding = 6;
    int lineH = 16;
    int boxW = maxW + padding * 2;
    int boxH = lineH * d->count + padding * 2;

    int tx = (int)mouse.x + 15;
    int ty = (int)mouse.y + 15;
    if (tx + boxW > GetScreenWidth()) tx = (int)mouse.x - boxW - 5;
    if (ty + boxH > GetScreenHeight()) ty = (int)mouse.y - boxH - 5;

    DrawRectangle(tx, ty, boxW, boxH, style.bg);
    DrawRectangleLines(tx, ty, boxW, boxH, style.border);

    int y = ty + padding;
    for (int i = 0; i < d->count; i++) {
        Color col = (i == 0 && style.title.a > 0) ? style.title : DescToneColor(d->lines[i].tone);
        DrawTextShadow(d->lines[i].text, tx + padding, y, 14, col);
        y += lineH;
    }
}

// Mover tooltip: the full (debug-level) description from DescribeMover
static void DrawMoverTooltip(int moverIdx, Vector2 mouse) {
    DescLines d;
    DescribeMover(moverIdx, DESC_DEBUG, &d);
    DrawDescTooltip(&d, mouse);
}

// Animal tooltip (only shown when paused): DescribeAnimal
static void DrawAnimalTooltip(int animalIdx, Vector2 mouse) {
    DescLines d;
    DescribeAnimal(animalIdx, &d);
    DrawDescTooltip(&d, mouse);
}

// Item tooltip (only shown when paused): the items on the hovered cell
static void DrawItemTooltip(int* itemIndices, int itemCount, Vector2 mouse, int cellX, int cellY) {
    DescLines d;
    DescribeItemsAt(itemIndices, itemCount, cellX, cellY, &d);
    DrawDescTooltipStyled(&d, mouse, DESC_BOX_ITEMS);
}

// Cell tooltip (shown when paused): DescribeCell
static void DrawCellTooltip(int cellX, int cellY, int cellZ, Vector2 mouse) {
    DescLines d;
    DescribeCell(cellX, cellY, cellZ, &d);
    DrawDescTooltipStyled(&d, mouse, DESC_BOX_CELL);
}

// Water tooltip (hovering water): DescribeWater
static void DrawWaterTooltip(int cellX, int cellY, int cellZ, Vector2 mouse) {
    DescLines d;
    DescribeWater(cellX, cellY, cellZ, &d);
    DrawDescTooltipStyled(&d, mouse, DESC_BOX_WATER);
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

// Blueprint (construction) tooltip: DescribeBlueprint
static void DrawBlueprintTooltip(int bpIdx, Vector2 mouse) {
    DescLines d;
    DescribeBlueprint(bpIdx, &d);
    DrawDescTooltipStyled(&d, mouse, DESC_BOX_BLUEPRINT);
}

// Designation tooltip (all designation kinds): DescribeDesignation, boxed in the kind's colors
static void DrawDesignationTooltip(int cellX, int cellY, int cellZ, Vector2 mouse) {
    DescLines d;
    DescribeDesignation(cellX, cellY, cellZ, &d);
    if (d.count == 0) return;
    const Designation* des = GetDesignation(cellX, cellY, cellZ);
    DrawDescTooltipStyled(&d, mouse, DesignationBoxStyle(des ? des->type : DESIGNATION_NONE));
}

// Train station tooltip: DescribeStation
static void DrawStationTooltip(int stationIdx, Vector2 mouse) {
    DescLines d;
    DescribeStation(stationIdx, &d);
    DrawDescTooltipStyled(&d, mouse, DESC_BOX_STATION);
}
