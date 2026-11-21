// inspector.c - The selected thing: an outline in the world and a fixed panel.
//
// The panel sits bottom-right above the tool bar and shows what the describe
// layer says about the selection (src/core/describe.c), plus buttons for what
// you can do with it. Selection itself: src/core/selection.c.
#include "inspector.h"
#include "../game_state.h"
#include "../core/selection.h"
#include "../core/describe.h"
#include "../entities/mover.h"
#include "../entities/animals.h"
#include "../world/designations.h"

Color DescToneColor(DescTone tone);   // tooltips.c

#define INSPECTOR_W        340
#define INSPECTOR_PAD      8
#define INSPECTOR_LINE_H   16
#define INSPECTOR_TITLE_H  22
#define INSPECTOR_BUTTONS_H 26
#define INSPECTOR_MARGIN   10
#define INSPECTOR_BAR_H    40   // keep clear of the bottom tool bar

static void DescribeSelection(DescLines* d) {
    d->count = 0;
    if (!SelectionValid()) return;
    switch (selection.kind) {
        case SEL_MOVER:     DescribeMover(selection.idx, DESC_PLAYER, d); break;
        case SEL_ANIMAL:    DescribeAnimal(selection.idx, d); break;
        case SEL_BLUEPRINT: DescribeBlueprint(selection.idx, d); break;
        case SEL_NONE:      break;
    }
}

static bool SelectionHasButtons(void) {
    return selection.kind == SEL_MOVER;
}

// Panel rectangle for the current selection (zero size when nothing is selected).
// main.c registers it as a UI block area before input, so clicks on the panel
// never reach the world.
Rectangle InspectorPanelRect(void) {
    DescLines d;
    DescribeSelection(&d);
    if (d.count == 0) return (Rectangle){0, 0, 0, 0};
    int bodyLines = d.count - 1;   // first line is the title
    float h = INSPECTOR_PAD * 2 + INSPECTOR_TITLE_H + bodyLines * INSPECTOR_LINE_H
              + (SelectionHasButtons() ? INSPECTOR_BUTTONS_H : 0);
    float x = GetScreenWidth() - INSPECTOR_W - INSPECTOR_MARGIN;
    float y = GetScreenHeight() - INSPECTOR_BAR_H - h - INSPECTOR_MARGIN;
    if (y < INSPECTOR_MARGIN) y = INSPECTOR_MARGIN;
    return (Rectangle){x, y, INSPECTOR_W, h};
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
        default: return;
    }
    if (wz != currentViewZ) return;
    Rectangle r = {offset.x + wx * zoom - 2, offset.y + wy * zoom - 2, size * wcells + 4, size * hcells + 4};
    DrawRectangleLinesEx(r, 2, YELLOW);
}

// The fixed inspector panel for the selection
void DrawInspectorPanel(void) {
    DescLines d;
    DescribeSelection(&d);
    if (d.count == 0) return;
    Rectangle r = InspectorPanelRect();

    DrawRectangleRec(r, (Color){20, 22, 32, 235});
    DrawRectangleLinesEx(r, 1, (Color){110, 110, 160, 255});

    // Title (the description's first line) and close button
    float x = r.x + INSPECTOR_PAD, y = r.y + INSPECTOR_PAD;
    DrawTextShadow(d.lines[0].text, (int)x, (int)y, 16, DescToneColor(d.lines[0].tone));
    {
        bool clicked = false;
        PushButtonInline(r.x + r.width - INSPECTOR_PAD - 24, y, "X", &clicked);
        if (clicked) { ClearSelection(); return; }
    }
    y += INSPECTOR_TITLE_H;

    for (int i = 1; i < d.count; i++) {
        DrawTextShadow(d.lines[i].text, (int)x, (int)y, 14, DescToneColor(d.lines[i].tone));
        y += INSPECTOR_LINE_H;
    }

    // Actions
    if (selection.kind == SEL_MOVER) {
        int idx = selection.idx;
        float bx = x;
        bool clicked = false;
        bx += PushButtonInline(bx, y + 4, followMoverIdx == idx ? "Unfollow" : "Follow", &clicked);
        if (clicked) ToggleMoverFollow(idx);
        clicked = false;
        PushButtonInline(bx, y + 4, movers[idx].isDrafted ? "Undraft" : "Draft", &clicked);
        if (clicked) ToggleMoverDraft(idx);
    }
}
