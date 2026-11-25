// inspector.c - The selected thing: an outline in the world and a fixed panel.
//
// The panel sits bottom-right above the tool bar and shows what the describe
// layer says about the selection (src/core/describe.c), plus buttons for what
// you can do with it. Selection and the actions themselves: src/core/selection.c.
//
// PanelContent() runs twice per frame: once to measure (draw == false, no
// buttons) so the panel can be anchored to the bottom, once to draw.
#include "../entities/town.h"
#include "inspector.h"
#include "../game_state.h"
#include "../core/selection.h"
#include "../core/describe.h"
#include "../core/input_mode.h"
#include "../entities/mover.h"
#include "../entities/animals.h"
#include "../entities/workshops.h"
#include "../entities/stockpiles.h"
#include "../entities/money.h"
#include "../world/designations.h"

Color DescToneColor(DescTone tone);   // tooltips.c

#define INSPECTOR_W          340
#define INSPECTOR_W_WORKSHOP 400
#define INSPECTOR_PAD        8
#define INSPECTOR_LINE_H     16
#define INSPECTOR_TITLE_H    22
#define INSPECTOR_BUTTON_H   24
#define INSPECTOR_MARGIN     10
#define INSPECTOR_BAR_H      40   // keep clear of the bottom tool bar

static float PanelWidth(void) {
    return selection.kind == SEL_WORKSHOP ? INSPECTOR_W_WORKSHOP : INSPECTOR_W;
}

// ---------------------------------------------------------------------------
// Layout helpers: each draws only when `draw`, and always advances *y
// ---------------------------------------------------------------------------

static void Lines(const DescLines* d, int from, bool draw, float x, float* y) {
    for (int i = from; i < d->count; i++) {
        if (draw) DrawTextShadow(d->lines[i].text, (int)x, (int)*y, 14, DescToneColor(d->lines[i].tone));
        *y += INSPECTOR_LINE_H;
    }
}

static void Text(const char* text, Color color, bool draw, float x, float* y) {
    if (draw) DrawTextShadow(text, (int)x, (int)*y, 14, color);
    *y += INSPECTOR_LINE_H;
}

// One button at (*bx, y); returns true when clicked (never while measuring)
static bool Button(const char* label, bool draw, float* bx, float y) {
    if (!draw) return false;
    bool clicked = false;
    *bx += PushButtonInline(*bx, y, label, &clicked);
    return clicked;
}

// ---------------------------------------------------------------------------
// Content per selection kind
// ---------------------------------------------------------------------------

// The selection's verbs (selection.c) of one kind as rows of buttons (wrapping at the panel
// width; measured the same way when only laying out, draw = false)
static void VerbButtons(SelectionKind kind, int idx, bool draw, float x, float* y) {
    float bx = x, maxX = x + PanelWidth() - 2 * INSPECTOR_PAD;
    for (int i = 0; i < SelectionVerbCount(); i++) {
        const SelectionVerb* v = SelectionVerbAt(i);
        if (v->kind != kind || !v->button) continue;
        const char* label = v->label ? v->label(idx) : v->name;
        float w = (float)MeasureTextUI(TextFormat("[%s]", label), UI_FONT_MEDIUM) + 15;
        if (bx > x && bx + w > maxX) { *y += INSPECTOR_BUTTON_H; bx = x; }
        if (draw) { if (Button(label, draw, &bx, *y + 2)) v->run(idx); }
        else bx += w;
    }
    *y += INSPECTOR_BUTTON_H;
}

static void MoverButtons(bool draw, float x, float* y) {
    VerbButtons(SEL_MOVER, selection.idx, draw, x, y);
}

static void WorkshopContent(bool draw, float x, float* y) {
    int wsIdx = selection.idx;
    const Workshop* ws = &workshops[wsIdx];

    // Bills, each with its own row of buttons
    if (ws->billCount > 0) {
        Text("Bills:", WHITE, draw, x, y);
        for (int b = 0; b < ws->billCount; b++) {
            DescLines bill = {0};
            DescribeWorkshopBill(wsIdx, b, false, &bill);
            Lines(&bill, 0, draw, x, y);
            const Bill* bl = &ws->bills[b];
            bool counted = bl->mode == BILL_DO_X_TIMES || bl->mode == BILL_DO_UNTIL_X;
            float bx = x + 16;
            if (Button("Mode", draw, &bx, *y)) WorkshopCycleBillMode(wsIdx, b);
            if (counted && Button("-", draw, &bx, *y)) WorkshopAdjustBillTarget(wsIdx, b, -1);
            if (counted && Button("+", draw, &bx, *y)) WorkshopAdjustBillTarget(wsIdx, b, 1);
            if (Button(bl->suspended ? "Resume" : "Pause", draw, &bx, *y)) WorkshopToggleBillPause(wsIdx, b);
            if (Button("Remove", draw, &bx, *y)) { WorkshopRemoveBill(wsIdx, b); *y += INSPECTOR_BUTTON_H; return; }
            *y += INSPECTOR_BUTTON_H;
        }
    } else {
        Text("No bills: add one below", GRAY, draw, x, y);
    }

    // Add a bill: one button per recipe
    int recipeCount;
    GetRecipesForWorkshop(ws->type, &recipeCount);
    if (recipeCount > 0 && ws->billCount < MAX_BILLS_PER_WORKSHOP) {
        *y += 4;
        Text("Add a bill:", WHITE, draw, x, y);
        for (int r = 0; r < recipeCount; r++) {
            char summary[DESC_LINE_LEN];
            char label[DESC_LINE_LEN + 4];
            WorkshopRecipeSummary(wsIdx, r, summary, sizeof(summary));
            snprintf(label, sizeof(label), "+ %s", summary);
            float bx = x;
            if (Button(label, draw, &bx, *y)) WorkshopAddBill(wsIdx, r);
            *y += INSPECTOR_BUTTON_H;
        }
    }

    // Linked stockpiles, link and deconstruct
    *y += 4;
    DescLines links = {0};
    DescribeWorkshopLinks(wsIdx, &links);
    Lines(&links, 0, draw, x, y);
    VerbButtons(SEL_WORKSHOP, wsIdx, draw, x, y);   // link, deconstruct (selection.c verbs)
}

// Stockpile: priority / stack / containers steppers, filters as toggle buttons.
// One filter category can be opened to toggle its items one by one.
static int openFilterCategory = -1;
static int openFilterStockpile = -1;

static void Stepper(const char* label, int value, bool draw, float x, float y, float* bx,
                    int spIdx, void (*adjust)(int, int)) {
    if (draw) DrawTextShadow(TextFormat("%s %d", label, value), (int)*bx, (int)y + 3, 14, WHITE);
    *bx += 92;
    if (Button("-", draw, bx, y)) adjust(spIdx, -1);
    if (Button("+", draw, bx, y)) adjust(spIdx, 1);
    *bx += 10;
    (void)x;
}

static void StockpileContent(bool draw, float x, float* y) {
    int spIdx = selection.idx;
    const Stockpile* sp = &stockpiles[spIdx];
    if (openFilterStockpile != spIdx) { openFilterStockpile = spIdx; openFilterCategory = -1; }

    float bx = x;
    Stepper("Priority", sp->priority, draw, x, *y, &bx, spIdx, StockpileAdjustPriority);
    Stepper("Stack", sp->maxStackSize, draw, x, *y, &bx, spIdx, StockpileAdjustStackSize);
    *y += INSPECTOR_BUTTON_H;
    bx = x;
    Stepper("Containers", sp->maxContainers, draw, x, *y, &bx, spIdx, StockpileAdjustContainers);
    if (Button(stockpileForSale[spIdx] ? "Stop selling" : "Sell", draw, &bx, *y)) StockpileToggleForSale(spIdx);   // money.h
    *y += INSPECTOR_BUTTON_H + 4;

    // Filters: one row per category (toggle all / open), the open one lists its items
    bx = x;
    if (draw) DrawTextShadow("Filters:", (int)x, (int)*y + 3, 14, WHITE);
    bx += 60;
    if (Button("All on/off", draw, &bx, *y)) StockpileToggleAll(spIdx);
    *y += INSPECTOR_BUTTON_H;
    for (int cat = 0; cat < FILTER_CAT_COUNT; cat++) {
        int enabled, total;
        StockpileCategoryCounts(spIdx, cat, &enabled, &total);
        if (total == 0) continue;
        bool open = openFilterCategory == cat;
        bx = x;
        if (Button(open ? "v" : ">", draw, &bx, *y)) openFilterCategory = open ? -1 : cat;
        Color c = enabled == total ? GREEN : enabled > 0 ? YELLOW : DARKGRAY;
        if (Button(TextFormat("%s %s %d/%d", enabled == total ? "[x]" : enabled > 0 ? "[-]" : "[ ]",
                              FILTER_CATEGORY_NAMES[cat], enabled, total), draw, &bx, *y))
            StockpileToggleCategory(spIdx, cat);
        if (draw && enabled > 0) DrawRectangle((int)x - 4, (int)*y + 4, 2, 12, c);
        *y += INSPECTOR_BUTTON_H;
        if (!open) continue;
        for (int i = 0; i < STOCKPILE_FILTER_COUNT; i++) {
            if ((int)STOCKPILE_FILTERS[i].category != cat) continue;
            bool on = sp->allowedTypes[STOCKPILE_FILTERS[i].itemType];
            bx = x + 24;
            if (Button(TextFormat("%s %s", on ? "[x]" : "[ ]", STOCKPILE_FILTERS[i].displayName), draw, &bx, *y))
                StockpileToggleItem(spIdx, i);
            *y += INSPECTOR_BUTTON_H;
        }
    }

    // Wood types (material sub-filters)
    if (STOCKPILE_MATERIAL_FILTER_COUNT > 0) {
        *y += 4;
        Text("Wood:", WHITE, draw, x, y);
        bx = x;
        for (int i = 0; i < STOCKPILE_MATERIAL_FILTER_COUNT; i++) {
            const StockpileMaterialFilterDef* mf = &STOCKPILE_MATERIAL_FILTERS[i];
            bool on = sp->allowedMaterials[mf->material];
            if (Button(TextFormat("%s %s", on ? "[x]" : "[ ]", mf->displayName), draw, &bx, *y))
                StockpileToggleMaterial(spIdx, i);
        }
        *y += INSPECTOR_BUTTON_H;
    }
}

// Title + body for the selection; returns the content height (from `top`)
static float PanelContent(bool draw, float x, float top, float right) {
    if (!SelectionValid()) return 0;
    DescLines d;
    switch (selection.kind) {
        case SEL_MOVER:     DescribeMover(selection.idx, selection.idx == debugMoverIdx ? DESC_DEBUG : DESC_PLAYER, &d); break;
        case SEL_ANIMAL:    DescribeAnimal(selection.idx, &d); break;
        case SEL_BLUEPRINT: DescribeBlueprint(selection.idx, &d); break;
        case SEL_WORKSHOP:  DescribeWorkshopStatus(selection.idx, &d); break;
        case SEL_STOCKPILE: DescribeStockpile(selection.idx, &d); break;
        case SEL_LOT:       DescribeLot(selection.idx, &d); break;
        case SEL_NONE:      return 0;
    }
    if (d.count == 0) return 0;

    float y = top;
    if (draw) {
        DrawTextShadow(d.lines[0].text, (int)x, (int)y, 16, DescToneColor(d.lines[0].tone));
        bool clicked = false;
        PushButtonInline(right - 24, y, "X", &clicked);
        if (clicked) { ClearSelection(); return 0; }
    }
    y += INSPECTOR_TITLE_H;
    Lines(&d, 1, draw, x, &y);

    if (selection.kind == SEL_MOVER) MoverButtons(draw, x, &y);
    else if (selection.kind == SEL_WORKSHOP) WorkshopContent(draw, x, &y);
    else if (selection.kind == SEL_STOCKPILE) StockpileContent(draw, x, &y);
    return y - top;
}

// Panel rectangle for the current selection (zero size when nothing is selected).
// main.c registers it as a UI block area before input, so clicks on the panel
// never reach the world.
Rectangle InspectorPanelRect(void) {
    float w = PanelWidth();
    float h = PanelContent(false, 0, 0, w);
    if (h <= 0) return (Rectangle){0, 0, 0, 0};
    h += INSPECTOR_PAD * 2;
    float x = GetScreenWidth() - w - INSPECTOR_MARGIN;
    float y = GetScreenHeight() - INSPECTOR_BAR_H - h - INSPECTOR_MARGIN;
    if (y < INSPECTOR_MARGIN) y = INSPECTOR_MARGIN;
    return (Rectangle){x, y, w, h};
}

// Yellow outline around the selected thing (top view, same level only)
void DrawSelectionHighlight(void) {
    if (frontViewMode || !SelectionValid()) return;
    float size = CELL_SIZE * zoom;
    float wx, wy, wcells = 1, hcells = 1;
    int wz;
    switch (selection.kind) {
        case SEL_MOVER: {
            const Mover* m = &movers[selection.idx];
            wx = m->x - CELL_SIZE * 0.5f; wy = m->y - CELL_SIZE * 0.5f; wz = (int)m->z;
            break;
        }
        case SEL_ANIMAL: {
            const Animal* a = &animals[selection.idx];
            wx = a->x - CELL_SIZE * 0.5f; wy = a->y - CELL_SIZE * 0.5f; wz = (int)a->z;
            break;
        }
        case SEL_BLUEPRINT: {
            int x0, y0, w, h;
            if (!BlueprintFootprint(selection.idx, &x0, &y0, &w, &h)) return;
            wx = x0 * CELL_SIZE; wy = y0 * CELL_SIZE; wz = blueprints[selection.idx].z;
            wcells = (float)w; hcells = (float)h;
            break;
        }
        case SEL_WORKSHOP: {
            const Workshop* ws = &workshops[selection.idx];
            wx = ws->x * CELL_SIZE; wy = ws->y * CELL_SIZE; wz = ws->z;
            wcells = (float)ws->width; hcells = (float)ws->height;
            break;
        }
        case SEL_STOCKPILE: {
            const Stockpile* sp = &stockpiles[selection.idx];
            wx = sp->x * CELL_SIZE; wy = sp->y * CELL_SIZE; wz = sp->z;
            wcells = (float)sp->width; hcells = (float)sp->height;
            break;
        }
        case SEL_LOT: {
            const TownLot* L = &townLots[selection.idx];
            wx = L->x1 * CELL_SIZE; wy = L->y1 * CELL_SIZE; wz = L->z;
            wcells = (float)(L->x2 - L->x1 + 1); hcells = (float)(L->y2 - L->y1 + 1);
            break;
        }
        default: return;
    }
    if (wz != currentViewZ) return;
    Rectangle r = {offset.x + wx * zoom - 2, offset.y + wy * zoom - 2, size * wcells + 4, size * hcells + 4};
    DrawRectangleLinesEx(r, 2, YELLOW);
}

// The fixed inspector panel for the selection
void DrawInspectorPanel(void) {
    Rectangle r = InspectorPanelRect();
    if (r.width <= 0) return;
    DrawRectangleRec(r, (Color){20, 22, 32, 235});
    DrawRectangleLinesEx(r, 1, (Color){110, 110, 160, 255});
    PanelContent(true, r.x + INSPECTOR_PAD, r.y + INSPECTOR_PAD, r.x + r.width - INSPECTOR_PAD);
}
