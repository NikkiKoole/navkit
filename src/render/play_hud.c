// play_hud.c - The play HUD's tab bar, bottom left (dev UI off).
//
//   [Huisje] [Boompje] [Beestje (3)]      click a tab to open its row above it
//
// Huisje (build) and Boompje (nature) show the active game's tools for that tab
// (GameDef.tools in src/core/games.c, each tool names its HudTab). Beestje shows
// the movers (click: select + follow) and animals (click: select + go there).
//
// Layout() runs twice per frame, like the inspector panel: from main.c before
// input (draw == false) to know which screen area to keep clicks out of, and
// from DrawPlayerHUD to draw and handle clicks.
#include "play_hud.h"
#include "../game_state.h"
#include "../core/games.h"
#include "../core/selection.h"
#include "../core/input_mode.h"
#include "../entities/mover.h"
#include "../entities/namegen.h"
#include "../entities/animals.h"
#include "../entities/belts.h"

#define HUD_MARGIN   10
#define HUD_BUTTON_H 28
#define HUD_GAP      4
#define HUD_FONT     16
#define HUD_PAD      12
#define HUD_RIGHT_KEEP 380   // keep the bottom right free for the inspector panel
#define HUD_MAX_ROSTER 40
#define HUD_MAX_ANIMALS 12   // wild herds can be big: show the first few

static int openTab = -1;   // HudTab, or -1 when all tabs are closed

static const char* const tabNames[HUD_TAB_COUNT] = {"Huisje", "Boompje", "Beestje"};
static const char* const tabTips[HUD_TAB_COUNT] = {
    "Build: stockpiles, zones, fires, beds",
    "Nature: gather, harvest, knap, explore",
    "People and animals: click one to select it and go there",
};

static float ButtonWidth(const char* label) {
    return (float)MeasureTextUI(label, HUD_FONT) + HUD_PAD * 2;
}

// One button; returns true when clicked (only when drawing)
static bool HudButton(Rectangle r, const char* label, bool selected, const char* tip, bool draw) {
    if (!draw) return false;
    bool hovered = CheckCollisionPointRec(GetMousePosition(), r);
    Color bg = selected ? (Color){60, 60, 80, 230} : hovered ? (Color){60, 60, 60, 230} : (Color){30, 30, 30, 220};
    DrawRectangleRec(r, bg);
    DrawRectangleLinesEx(r, 1, hovered ? WHITE : selected ? (Color){150, 150, 200, 255} : GRAY);
    DrawTextShadow(label, (int)(r.x + HUD_PAD), (int)(r.y + (HUD_BUTTON_H - HUD_FONT) / 2.0f), HUD_FONT,
                   selected ? YELLOW : WHITE);
    if (!hovered) return false;
    ui_set_hovered();
    if (tip) ui_set_tooltip(tip);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        ui_consume_click();
        return true;
    }
    return false;
}

static void FocusAnimal(int idx) {
    Select(SEL_ANIMAL, idx);
    followMoverIdx = -1;
    currentViewZ = (int)animals[idx].z;
    offset.x = GetScreenWidth() / 2.0f - animals[idx].x * zoom;
    offset.y = GetScreenHeight() / 2.0f - animals[idx].y * zoom;
}

static void FocusMover(int idx) {
    Select(SEL_MOVER, idx);
    followMoverIdx = idx;
    currentViewZ = (int)movers[idx].z;
    offset.x = GetScreenWidth() / 2.0f - movers[idx].x * zoom;
    offset.y = GetScreenHeight() / 2.0f - movers[idx].y * zoom;
}

// Buttons go left to right; a full row wraps to a new row above it.
// Place() returns the rect for the next button of width w.
typedef struct {
    float x0, xMax;       // row start and wrap limit
    float x, y;           // next button position (y = top of the current row)
    float top;            // highest y used
    int rows;
} RowLayout;

static void NewRow(RowLayout* L) {
    if (L->x == L->x0) return;
    L->x = L->x0;
    L->y -= HUD_BUTTON_H + HUD_GAP;
    L->rows++;
    if (L->y < L->top) L->top = L->y;
}

static Rectangle Place(RowLayout* L, float w) {
    if (L->x + w > L->xMax && L->x > L->x0) {   // wrap: new row above
        L->x = L->x0;
        L->y -= HUD_BUTTON_H + HUD_GAP;
        L->rows++;
    }
    Rectangle r = {L->x, L->y, w, HUD_BUTTON_H};
    L->x += w + HUD_GAP;
    if (L->y < L->top) L->top = L->y;
    return r;
}

static bool ToolActive(const GameTool* tool) {
    return inputAction == (InputAction)tool->action &&
           (tool->furnitureRecipe < 0 || GetSelectedFurnitureRecipe() == tool->furnitureRecipe);
}

// Returns the screen area the HUD covers
static Rectangle Layout(bool draw) {
    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    float tabsY = screenH - HUD_BUTTON_H - HUD_MARGIN;

    // Tab strip
    int moverTotal = CountActiveMovers();
    float x = HUD_MARGIN;
    for (int t = 0; t < HUD_TAB_COUNT; t++) {
        const char* label = (t == HUD_TAB_CREATURES && moverTotal > 0)
            ? TextFormat("%s (%d)", tabNames[t], moverTotal) : tabNames[t];
        float w = ButtonWidth(label);
        if (HudButton((Rectangle){x, tabsY, w, HUD_BUTTON_H}, label, openTab == t, tabTips[t], draw))
            openTab = (openTab == t) ? -1 : t;
        x += w + HUD_GAP;
    }
    float stripRight = x;
    Rectangle area = {HUD_MARGIN, tabsY, stripRight - HUD_MARGIN, HUD_BUTTON_H};
    if (openTab < 0) return area;

    // Row(s) above the tabs. Items are laid out bottom row first, wrapping upwards.
    float xMax = screenW - HUD_RIGHT_KEEP;
    if (xMax < stripRight) xMax = stripRight;
    RowLayout L = {HUD_MARGIN, xMax, HUD_MARGIN, tabsY - HUD_BUTTON_H - HUD_GAP, tabsY, 1};
    float rightmost = stripRight;

    if (openTab == HUD_TAB_CREATURES) {
        int shown = 0;
        for (int i = 0; i < moverCount && shown < HUD_MAX_ROSTER; i++) {
            if (!movers[i].active) continue;
            const char* name = MoverDisplayName(i);
            const char* label = movers[i].isDrafted ? TextFormat("[D] %s", name) : name;
            Rectangle r = Place(&L, ButtonWidth(label));
            bool selected = selection.kind == SEL_MOVER && selection.idx == i;
            if (HudButton(r, label, selected, NULL, draw)) FocusMover(i);
            if (r.x + r.width > rightmost) rightmost = r.x + r.width;
            shown++;
        }
        if (shown == 0) {
            const char* none = "Nobody here yet";
            Rectangle r = Place(&L, ButtonWidth(none));
            if (draw) DrawTextShadow(none, (int)(r.x + HUD_PAD), (int)(r.y + 6), HUD_FONT, GRAY);
            if (r.x + r.width > rightmost) rightmost = r.x + r.width;
        }

        // Animals on their own row(s) above the people
        int animalTotal = 0;
        for (int i = 0; i < animalCount; i++) if (animals[i].active) animalTotal++;
        if (animalTotal > 0) {
            NewRow(&L);
            int shownAnimals = 0;
            for (int i = 0; i < animalCount && shownAnimals < HUD_MAX_ANIMALS; i++) {
                if (!animals[i].active) continue;
                const char* label = TextFormat("%s %d", animals[i].type == ANIMAL_PREDATOR ? "Predator" : "Grazer", i);
                Rectangle r = Place(&L, ButtonWidth(label));
                bool selected = selection.kind == SEL_ANIMAL && selection.idx == i;
                if (HudButton(r, label, selected, NULL, draw)) FocusAnimal(i);
                if (r.x + r.width > rightmost) rightmost = r.x + r.width;
                shownAnimals++;
            }
            if (animalTotal > shownAnimals) {
                const char* more = TextFormat("+%d more", animalTotal - shownAnimals);
                Rectangle r = Place(&L, ButtonWidth(more));
                if (draw) DrawTextShadow(more, (int)(r.x + HUD_PAD), (int)(r.y + 6), HUD_FONT, GRAY);
                if (r.x + r.width > rightmost) rightmost = r.x + r.width;
            }
        }
    } else {
        const GameDef* game = GameActive();
        GameToolContext ctx = GameToolContextNow();
        int shown = 0;
        for (int t = 0; game && t < game->toolCount; t++) {
            const GameTool* tool = &game->tools[t];
            if ((int)tool->tab != openTab) continue;
            if (tool->available && !tool->available(&ctx)) continue;
            bool active = ToolActive(tool);
            Rectangle r = Place(&L, ButtonWidth(tool->label));
            if (HudButton(r, tool->label, active, NULL, draw)) {
                if (active) {
                    InputMode_ExitToNormal();
                } else {
                    if (tool->furnitureRecipe >= 0) SetSelectedFurnitureRecipe(tool->furnitureRecipe);
                    InputMode_SelectAction((InputAction)tool->action);
                }
            }
            if (r.x + r.width > rightmost) rightmost = r.x + r.width;
            shown++;
            // Belt tool: a direction button next to it (click cycles N/E/S/W/lift/chute;
            // arrows and PgUp/PgDn also work)
            if (active && tool->action == ACTION_WORK_BELT) {
                const char* dirLabel = TextFormat("Dir: %s", GetSelectedBeltDirectionName());
                Rectangle d = Place(&L, ButtonWidth(dirLabel));
                if (HudButton(d, dirLabel, false, NULL, draw))
                    selectedBeltDirection = (selectedBeltDirection + 1) % (BELT_DOWN + 1);
                if (d.x + d.width > rightmost) rightmost = d.x + d.width;
            }
        }
        if (shown == 0) {
            const char* none = "Nothing yet: gather some materials first";
            Rectangle r = Place(&L, ButtonWidth(none));
            if (draw) DrawTextShadow(none, (int)(r.x + HUD_PAD), (int)(r.y + 6), HUD_FONT, GRAY);
            if (r.x + r.width > rightmost) rightmost = r.x + r.width;
        }
    }

    area.y = L.top;
    area.width = rightmost - HUD_MARGIN;
    area.height = tabsY + HUD_BUTTON_H - L.top;
    return area;
}

Rectangle PlayHudRect(void) {
    if (devUI) return (Rectangle){0, 0, 0, 0};
    return Layout(false);
}

void DrawPlayHudTabs(void) {
    Layout(true);
}
