// palette.c - the find palette window (see palette.h, docs/todo/ui/find-palette.md)
#include "palette.h"
#include "find.h"
#include "commands.h"
#include "../core/action_registry.h"
#include "../core/settings.h"
#include "../core/selection.h"
#include "../core/scenarios.h"
#include "../core/games.h"
#include "../core/describe.h"
#include "../entities/items.h"
#include "../entities/item_defs.h"
#include "../../shared/ui.h"
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

typedef enum { PE_HEADER, PE_INFO, PE_UP, PE_BRANCH, PE_ROW, PE_CHOICE } PalEntryKind;
static bool Pickable(PalEntryKind k) { return k != PE_HEADER && k != PE_INFO; }
typedef struct {
    PalEntryKind kind;
    int row;                  // PE_ROW: find row
    char text[FIND_LABEL_LEN];
} PalEntry;

#define PAL_MAX_ENTRIES 320
#define PAL_QUERY_LEN   96
#define PAL_MAX_COMMANDS 64
#define PAL_VISIBLE     16

static struct {
    bool open;
    char query[PAL_QUERY_LEN];
    int qlen;
    char path[FIND_PATH_LEN];
    int sel, scroll;
    int cmdRow;                                   // >= 0: typing this command's arguments
    char lastArgs[PAL_MAX_COMMANDS][PAL_QUERY_LEN];   // per command: what was typed last time
    PalEntry entries[PAL_MAX_ENTRIES];
    int count;
    bool nearMiss;
    float blink;
} pal = { .cmdRow = -1 };

// The item brush: an item row picked in the palette, placed with left clicks
static int brushItem = -1;

// What the mouse was over when / was pressed (or the selection): its verbs and a few describe
// lines go on top of the palette, and its verbs run on it
static struct {
    SelectionKind kind;
    int idx;
    DescLines desc;
} ctx;

static const char* CtxKindLabel(SelectionKind k) {
    switch (k) {
        case SEL_MOVER:     return "Mover";
        case SEL_ANIMAL:    return "Animal";
        case SEL_WORKSHOP:  return "Workshop";
        case SEL_STOCKPILE: return "Stockpile";
        case SEL_BLUEPRINT: return "Blueprint";
        default:            return "";
    }
}

static void PickContext(void) {
    ctx.kind = SEL_NONE;
    ctx.idx = -1;
    ctx.desc.count = 0;
    if (hoveredMover >= 0)          { ctx.kind = SEL_MOVER;     ctx.idx = hoveredMover; }
    else if (hoveredAnimal >= 0)    { ctx.kind = SEL_ANIMAL;    ctx.idx = hoveredAnimal; }
    else if (hoveredWorkshop >= 0)  { ctx.kind = SEL_WORKSHOP;  ctx.idx = hoveredWorkshop; }
    else if (hoveredStockpile >= 0) { ctx.kind = SEL_STOCKPILE; ctx.idx = hoveredStockpile; }
    else if (SelectionValid())      { ctx.kind = selection.kind; ctx.idx = selection.idx; }
    switch (ctx.kind) {
        case SEL_MOVER:     DescribeMover(ctx.idx, DESC_PLAYER, &ctx.desc); break;
        case SEL_ANIMAL:    DescribeAnimal(ctx.idx, &ctx.desc); break;
        case SEL_WORKSHOP:  DescribeWorkshopStatus(ctx.idx, &ctx.desc); break;
        case SEL_STOCKPILE: DescribeStockpile(ctx.idx, &ctx.desc); break;
        case SEL_BLUEPRINT: DescribeBlueprint(ctx.idx, &ctx.desc); break;
        default: break;
    }
}

// A verb row that applies to the context thing
static bool IsCtxVerb(int row) {
    const FindRow* r = FindRowAt(row);
    return ctx.kind != SEL_NONE && r && r->kind == FIND_VERB && SelectionVerbAt(r->ref)->kind == ctx.kind;
}
static const char* VerbLabel(int row) {
    const SelectionVerb* v = SelectionVerbAt(FindRowAt(row)->ref);
    return v->label ? v->label(ctx.idx) : v->name;
}

bool Palette_IsOpen(void) { return pal.open; }

void Palette_Open(void) {
    FindBuildIndex();
    pal.open = true;
    pal.query[0] = '\0';
    pal.qlen = 0;
    pal.path[0] = '\0';
    pal.sel = 0;
    pal.scroll = 0;
    pal.cmdRow = -1;
    brushItem = -1;
    PickContext();
}

void Palette_Close(void) { pal.open = false; pal.cmdRow = -1; }

// ---------------------------------------------------------------------------
// Settings: show and change a value in place
// ---------------------------------------------------------------------------

static void SettingValueText(const Setting* s, char* buf, int len) {
    switch (s->type) {
        case SETTING_BOOL:   snprintf(buf, (size_t)len, "%s", *(bool*)s->ptr ? "on" : "off"); break;
        case SETTING_INT:    snprintf(buf, (size_t)len, "%d", *(int*)s->ptr); break;
        case SETTING_FLOAT:  snprintf(buf, (size_t)len, "%.3g", *(float*)s->ptr); break;
        case SETTING_CHOICE: {
            int i = *(int*)s->ptr;
            snprintf(buf, (size_t)len, "%s", (i >= 0 && i < s->optionCount) ? s->options[i] : "?");
            break;
        }
    }
}

// Step a setting: dir -1 / +1, big = Shift (x10). Bools toggle, choices cycle.
static void SettingStep(const Setting* s, int dir, bool big) {
    float before = SettingValue(s);
    switch (s->type) {
        case SETTING_BOOL: *(bool*)s->ptr = !*(bool*)s->ptr; break;
        case SETTING_CHOICE: {
            int* v = (int*)s->ptr;
            if (s->optionCount > 0) *v = ((*v + dir) % s->optionCount + s->optionCount) % s->optionCount;
            break;
        }
        case SETTING_INT: {
            int* v = (int*)s->ptr;
            int nv = *v + dir * (big ? 10 : 1);
            if (s->max > s->min) { if (nv < (int)s->min) nv = (int)s->min; if (nv > (int)s->max) nv = (int)s->max; }
            *v = nv;
            break;
        }
        case SETTING_FLOAT: {
            float* v = (float*)s->ptr;
            float step = s->max > s->min ? (s->max - s->min) / 100.0f : (s->speed > 0 ? s->speed * 10.0f : 0.1f);
            float nv = *v + (float)dir * step * (big ? 10.0f : 1.0f);
            if (s->max > s->min) { if (nv < s->min) nv = s->min; if (nv > s->max) nv = s->max; }
            *v = nv;
            break;
        }
    }
    if (s->onChange && SettingValue(s) != before) s->onChange(before);
}

// ---------------------------------------------------------------------------
// Entries: what the window lists now
// ---------------------------------------------------------------------------

static void Add(PalEntryKind kind, int row, const char* text) {
    if (pal.count >= PAL_MAX_ENTRIES) return;
    PalEntry* e = &pal.entries[pal.count++];
    e->kind = kind;
    e->row = row;
    snprintf(e->text, sizeof(e->text), "%s", text ? text : "");
}

// Command mode: the argument being typed is word number `argIdx`; its choices (or names)
// that start with the partial word are listed
static int CmdArgIndex(const char** partial) {
    int words = 0;
    const char* last = pal.query;
    for (const char* c = pal.query; *c; c++)
        if (*c == ' ' && c[1] && c[1] != ' ') { words++; last = c + 1; }
    if (pal.qlen > 0 && pal.query[pal.qlen - 1] == ' ') { last = pal.query + pal.qlen; words++; }
    *partial = last;
    return words;
}

static void AddCommandChoices(void) {
    const CommandDef* c = CommandAt(FindRowAt(pal.cmdRow)->ref);
    const char* partial;
    int argIdx = CmdArgIndex(&partial);
    if (argIdx >= c->argCount) return;
    const CmdArg* a = &c->args[argIdx];
    size_t pl = strlen(partial);
    if (a->type == CARG_CHOICE) {
        for (int i = 0; i < a->choiceCount; i++)
            if (strncasecmp(a->choices[i], partial, pl) == 0) Add(PE_CHOICE, -1, a->choices[i]);
    } else if (a->type == CARG_NAME && a->nameCount) {
        for (int i = 0; i < a->nameCount(); i++)
            if (strncasecmp(a->nameAt(i), partial, pl) == 0) Add(PE_CHOICE, -1, a->nameAt(i));
    } else if (a->type == CARG_ITEM) {
        for (int t = 0; t < ITEM_TYPE_COUNT; t++)
            if (itemDefs[t].name && strncasecmp(itemDefs[t].name, partial, pl) == 0) Add(PE_CHOICE, -1, itemDefs[t].name);
    }
}

static void BuildEntries(void) {
    pal.count = 0;
    pal.nearMiss = false;
    if (pal.cmdRow >= 0) {
        AddCommandChoices();
    } else if (pal.qlen > 0) {
        // The hovered thing's verbs that match come first
        bool any = false;
        for (int r = 0; r < FindCount(); r++) {
            if (!IsCtxVerb(r) || FindScore(r, pal.query) <= 0) continue;
            if (!any) { Add(PE_HEADER, -1, TextFormat("%s: %s", CtxKindLabel(ctx.kind), ctx.desc.count ? ctx.desc.lines[0].text : "")); any = true; }
            Add(PE_ROW, r, VerbLabel(r));
        }
        int hits[60];
        int n = FindSearch(pal.query, hits, 60, &pal.nearMiss);
        if (pal.nearMiss) Add(PE_HEADER, -1, "Did you mean");
        else if (any && n > 0) Add(PE_HEADER, -1, "Everything");
        for (int i = 0; i < n; i++) if (!IsCtxVerb(hits[i])) Add(PE_ROW, hits[i], FindRowAt(hits[i])->label);
    } else {
        if (pal.path[0] == '\0' && ctx.kind != SEL_NONE) {
            // The hovered (or selected) thing: who it is, a few lines about it, what you can do
            Add(PE_HEADER, -1, TextFormat("%s: %s", CtxKindLabel(ctx.kind), ctx.desc.count ? ctx.desc.lines[0].text : ""));
            for (int i = 1; i < ctx.desc.count && i < 4; i++) Add(PE_INFO, -1, ctx.desc.lines[i].text);
            for (int r = 0; r < FindCount(); r++) if (IsCtxVerb(r)) Add(PE_ROW, r, VerbLabel(r));
        }
        if (pal.path[0] == '\0') {
            int rows[FIND_MAX_PINNED];
            int n = FindPinned(rows, FIND_MAX_PINNED);
            if (n > 0) Add(PE_HEADER, -1, "Pinned");
            for (int i = 0; i < n; i++) Add(PE_ROW, rows[i], FindRowAt(rows[i])->label);
            n = FindRecent(rows, FIND_MAX_RECENT);
            if (n > 0) Add(PE_HEADER, -1, "Recent");
            for (int i = 0; i < n; i++) Add(PE_ROW, rows[i], FindRowAt(rows[i])->label);
            if (pal.count > 0) Add(PE_HEADER, -1, "Browse");
        } else {
            Add(PE_UP, -1, "..");
        }
        FindChild kids[PAL_MAX_ENTRIES];
        int n = FindTreeChildren(pal.path, kids, PAL_MAX_ENTRIES);
        for (int i = 0; i < n; i++) Add(kids[i].isBranch ? PE_BRANCH : PE_ROW, kids[i].row, kids[i].name);
    }
    // Keep the selection on a pickable entry
    if (pal.sel >= pal.count) pal.sel = pal.count - 1;
    if (pal.sel < 0) pal.sel = 0;
    while (pal.sel < pal.count - 1 && !Pickable(pal.entries[pal.sel].kind)) pal.sel++;
}

static void MoveSel(int dir) {
    int s = pal.sel;
    do { s += dir; } while (s >= 0 && s < pal.count && !Pickable(pal.entries[s].kind));
    if (s >= 0 && s < pal.count) pal.sel = s;
}

static void SetQuery(const char* text) {
    snprintf(pal.query, sizeof(pal.query), "%s", text);
    pal.qlen = (int)strlen(pal.query);
}

static void GoUp(void) {
    char* slash = strrchr(pal.path, '/');
    if (slash) *slash = '\0';
    else pal.path[0] = '\0';
    pal.sel = 0;
    pal.scroll = 0;
}

// ---------------------------------------------------------------------------
// Doing a row
// ---------------------------------------------------------------------------

static void RunCommandLine(void) {
    const CommandDef* c = CommandAt(FindRowAt(pal.cmdRow)->ref);
    char line[160];
    snprintf(line, sizeof(line), "%s %s", c->name, pal.query);
    if (CommandRunLine(line)) {
        int ref = FindRowAt(pal.cmdRow)->ref;
        if (ref >= 0 && ref < PAL_MAX_COMMANDS) snprintf(pal.lastArgs[ref], PAL_QUERY_LEN, "%s", pal.query);
        Palette_Close();
    }
}

static void ActivateRow(int row) {
    const FindRow* r = FindRowAt(row);
    if (!r) return;
    FindNoteUsed(row);
    switch (r->kind) {
        case FIND_TOOL: {
            InputAction a = InputMode_HowOf((InputAction)r->ref);
            InputMode_SelectAction(a);
            if (GetActionDef(a)->requiredMode == MODE_DRAW) selectedMaterial = 1;
            devUI = true;   // the tool's bar (option, now / order) lives in the dev overlay
            if (GetActionDef(a)->twin != ACTION_NONE)
                AddMessage(TextFormat("%s (%s, I flips)", r->label, buildNow ? "now" : "by order"), SKYBLUE);
            else AddMessage(r->label, SKYBLUE);
            Palette_Close();
            break;
        }
        case FIND_COMMAND: {
            const CommandDef* c = CommandAt(r->ref);
            if (c->argCount == 0) { CommandRunLine(c->name); Palette_Close(); break; }
            pal.cmdRow = row;
            SetQuery(r->ref < PAL_MAX_COMMANDS ? pal.lastArgs[r->ref] : "");
            pal.sel = 0;
            pal.scroll = 0;
            break;
        }
        case FIND_SETTING: {
            const Setting* s = SettingAt(r->ref);
            if (s->type == SETTING_BOOL || s->type == SETTING_CHOICE) SettingStep(s, 1, false);
            break;   // numbers: Left / Right (the window stays open)
        }
        case FIND_VERB: {
            const SelectionVerb* v = SelectionVerbAt(r->ref);
            if (ctx.kind == v->kind) {
                v->run(ctx.idx);
                Palette_Close();
            } else if (SelectionValid() && selection.kind == v->kind) {
                v->run(selection.idx);
                Palette_Close();
            } else {
                AddMessage(TextFormat("%s: hover or select one first (%s)", r->label, r->path), YELLOW);
            }
            break;
        }
        case FIND_ITEM:
            brushItem = r->ref;
            AddMessage(TextFormat("%s: L-click places (Shift: 10), R-click or Esc stops", r->label), SKYBLUE);
            Palette_Close();
            break;
        case FIND_SCENARIO:
            CommandRunLine(TextFormat("scenario %s", ScenarioAt(r->ref)->name));
            Palette_Close();
            break;
        case FIND_GAME:
            CommandRunLine(TextFormat("game %s", GameAt(r->ref)->name));
            Palette_Close();
            break;
        default: break;
    }
}

static void Activate(int i) {
    if (i < 0 || i >= pal.count) {
        if (pal.cmdRow >= 0) RunCommandLine();   // no choices: run what was typed
        return;
    }
    PalEntry* e = &pal.entries[i];
    switch (e->kind) {
        case PE_UP: GoUp(); break;
        case PE_BRANCH: {
            char np[FIND_PATH_LEN];
            snprintf(np, sizeof(np), "%s%s%s", pal.path, pal.path[0] ? "/" : "", e->text);
            snprintf(pal.path, sizeof(pal.path), "%s", np);
            pal.sel = 0;
            pal.scroll = 0;
            break;
        }
        case PE_ROW: ActivateRow(e->row); break;
        case PE_CHOICE: {
            // Complete the word being typed; run when that was the last argument
            const char* partial;
            int argIdx = CmdArgIndex(&partial);
            char q[PAL_QUERY_LEN];
            snprintf(q, sizeof(q), "%.*s%s", (int)(partial - pal.query), pal.query, e->text);
            SetQuery(q);
            const CommandDef* c = CommandAt(FindRowAt(pal.cmdRow)->ref);
            if (argIdx + 1 >= c->argCount || c->args[argIdx + 1].optional) RunCommandLine();
            else { SetQuery(TextFormat("%s ", pal.query)); pal.sel = 0; }
            break;
        }
        default: break;
    }
}

// ---------------------------------------------------------------------------
// Layout (shared by input and drawing)
// ---------------------------------------------------------------------------

#define PAL_FONT 16
#define PAL_ROW_H 22
#define PAL_PAD 8

static Rectangle WindowRect(void) {
    float w = (float)GetScreenWidth() - 32;
    if (w > 640) w = 640;
    float h = PAL_PAD * 2 + PAL_ROW_H * (PAL_VISIBLE + 3) + 8;
    return (Rectangle){ ((float)GetScreenWidth() - w) / 2, 56, w, h };
}
static float ListTop(void) { return WindowRect().y + PAL_PAD + PAL_ROW_H + 6; }
static Rectangle RowRect(int visibleIdx) {
    Rectangle w = WindowRect();
    return (Rectangle){ w.x + 4, ListTop() + (float)visibleIdx * PAL_ROW_H, w.width - 8, PAL_ROW_H };
}
// The value's "<" and ">" on a number setting row
static Rectangle ArrowRect(int visibleIdx, int dir) {
    Rectangle r = RowRect(visibleIdx);
    float x = dir < 0 ? r.x + r.width - 150 : r.x + r.width - 22;
    return (Rectangle){ x, r.y, 20, r.height };
}

static const Setting* SettingOfEntry(int i) {
    if (i < 0 || i >= pal.count || pal.entries[i].kind != PE_ROW) return NULL;
    const FindRow* r = FindRowAt(pal.entries[i].row);
    return (r && r->kind == FIND_SETTING) ? SettingAt(r->ref) : NULL;
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

// Teleport (mover verb, selection.c): the next click on a cell moves the mover there
static bool UpdateTeleportPick(void) {
    if (teleportMoverIdx < 0) return false;
    if (IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) { teleportMoverIdx = -1; return true; }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !ui_wants_mouse()) {
        Vector2 g = ScreenToGrid(GetMousePosition());
        if (MoverTeleportTo(teleportMoverIdx, (int)g.x, (int)g.y, currentViewZ)) teleportMoverIdx = -1;
        return true;
    }
    return false;
}

static bool UpdateBrush(void) {
    if (UpdateTeleportPick()) return true;
    if (brushItem < 0) return false;
    if (IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) { brushItem = -1; return true; }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !ui_wants_mouse()) {
        bool many = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        CommandRunLine(TextFormat("spawn %d %s", many ? 10 : 1, itemDefs[brushItem].name));
        return true;
    }
    return false;
}

bool Palette_Update(void) {
    if (!pal.open) {
        // '/' opens (the character, so any keyboard layout works); other typed characters
        // are nobody else's when the console is closed
        // are nobody else's when the console is closed. What is typed after '/' in the same
        // frame is the start of the search.
        int ch;
        bool openNow = false;
        while ((ch = GetCharPressed()) != 0) {
            if (!openNow) { if (ch == '/') { Palette_Open(); openNow = true; } continue; }
            if (ch >= 32 && ch <= 126 && ch != '`' && pal.qlen < PAL_QUERY_LEN - 1) {
                pal.query[pal.qlen++] = (char)ch;
                pal.query[pal.qlen] = '\0';
            }
        }
        if (openNow) return true;
        return UpdateBrush();
    }

    pal.blink += GetFrameTime();
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    bool changed = false;

    int ch;
    while ((ch = GetCharPressed()) != 0) {
        if (ch < 32 || ch > 126 || ch == '`') continue;
        if (pal.qlen < PAL_QUERY_LEN - 1) { pal.query[pal.qlen++] = (char)ch; pal.query[pal.qlen] = '\0'; changed = true; }
    }
    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
        if (pal.qlen > 0) { pal.query[--pal.qlen] = '\0'; changed = true; }
        else if (pal.cmdRow >= 0) { pal.cmdRow = -1; changed = true; }
        else if (pal.path[0]) GoUp();
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (pal.cmdRow >= 0) { pal.cmdRow = -1; SetQuery(""); changed = true; }
        else { Palette_Close(); return true; }
    }
    if (changed) { pal.sel = 0; pal.scroll = 0; }

    BuildEntries();

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN)) MoveSel(1);
    if (IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP)) MoveSel(-1);
    if (IsKeyPressed(KEY_PAGE_DOWN)) for (int i = 0; i < PAL_VISIBLE; i++) MoveSel(1);
    if (IsKeyPressed(KEY_PAGE_UP)) for (int i = 0; i < PAL_VISIBLE; i++) MoveSel(-1);
    const Setting* sSel = SettingOfEntry(pal.sel);
    if (sSel && (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT))) SettingStep(sSel, -1, shift);
    if (sSel && (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT))) SettingStep(sSel, 1, shift);
    if (IsKeyPressed(KEY_TAB) && pal.sel < pal.count && pal.entries[pal.sel].kind == PE_BRANCH) Activate(pal.sel);

    // Keep the selection in view
    if (pal.sel < pal.scroll) pal.scroll = pal.sel;
    if (pal.sel >= pal.scroll + PAL_VISIBLE) pal.scroll = pal.sel - PAL_VISIBLE + 1;

    // Mouse: wheel scrolls, hover selects, click picks, right-click pins; outside closes
    Vector2 m = GetMousePosition();
    Rectangle win = WindowRect();
    float wheel = GetMouseWheelMove();
    if (wheel != 0 && CheckCollisionPointRec(m, win)) {
        pal.scroll -= (int)wheel * 3;
        int maxScroll = pal.count - PAL_VISIBLE;
        if (pal.scroll > maxScroll) pal.scroll = maxScroll;
        if (pal.scroll < 0) pal.scroll = 0;
    }
    int hover = -1;
    for (int v = 0; v < PAL_VISIBLE && pal.scroll + v < pal.count; v++) {
        if (CheckCollisionPointRec(m, RowRect(v)) && Pickable(pal.entries[pal.scroll + v].kind)) {
            hover = pal.scroll + v;
            if (GetMouseDelta().x != 0 || GetMouseDelta().y != 0) pal.sel = hover;
        }
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (!CheckCollisionPointRec(m, win)) { Palette_Close(); return true; }
        if (hover >= 0) {
            const Setting* s = SettingOfEntry(hover);
            int v = hover - pal.scroll;
            if (s && (s->type == SETTING_INT || s->type == SETTING_FLOAT || s->type == SETTING_CHOICE)) {
                if (CheckCollisionPointRec(m, ArrowRect(v, -1))) SettingStep(s, -1, shift);
                else if (CheckCollisionPointRec(m, ArrowRect(v, 1))) SettingStep(s, 1, shift);
                else if (s->type == SETTING_CHOICE) SettingStep(s, 1, false);
            } else if (shift && pal.entries[hover].kind == PE_ROW) {
                FindSetPinned(pal.entries[hover].row, !FindIsPinned(pal.entries[hover].row));   // Shift+click pins
            } else {
                pal.sel = hover;
                Activate(hover);
            }
        }
    }
    bool pinKey = IsKeyPressed(KEY_ENTER) && shift;
    if ((IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && hover >= 0) || pinKey) {
        int i = pinKey ? pal.sel : hover;
        if (i >= 0 && i < pal.count && pal.entries[i].kind == PE_ROW)
            FindSetPinned(pal.entries[i].row, !FindIsPinned(pal.entries[i].row));
    } else if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        Activate(pal.count > 0 ? pal.sel : -1);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

static const Color PAL_BG     = { 22, 24, 30, 240 };
static const Color PAL_EDGE   = { 90, 100, 120, 255 };
static const Color PAL_SEL    = { 55, 70, 100, 255 };
static const Color PAL_MUTED  = { 140, 145, 160, 255 };
static const Color PAL_HEADER = { 200, 170, 90, 255 };

static void DrawRight(const char* text, float right, float y, Color c) {
    DrawTextShadow(text, (int)(right - (float)MeasureTextUI(text, PAL_FONT)), (int)y, PAL_FONT, c);
}

static void DrawBrushLabel(void) {
    Vector2 m = GetMousePosition();
    if (teleportMoverIdx >= 0) {
        const char* t = TextFormat("Teleport %s here?  (L-click, R-click stop)", MoverDisplayName(teleportMoverIdx));
        DrawTextShadow(t, (int)m.x + 16, (int)m.y + 12, PAL_FONT, SKYBLUE);
        return;
    }
    if (brushItem < 0) return;
    const char* t = TextFormat("%s  (L-click place, Shift 10, R-click stop)", itemDefs[brushItem].name);
    DrawTextShadow(t, (int)m.x + 16, (int)m.y + 12, PAL_FONT, SKYBLUE);
}

void Palette_Draw(void) {
    if (!pal.open) { DrawBrushLabel(); return; }
    Rectangle w = WindowRect();
    DrawRectangleRec(w, PAL_BG);
    DrawRectangleLinesEx(w, 1, PAL_EDGE);

    // Input line: "/ query" or "/ clear wat" in command mode, with the usage on the right
    float x = w.x + PAL_PAD, y = w.y + PAL_PAD;
    DrawTextShadow("/", (int)x, (int)y, PAL_FONT, PAL_HEADER);
    x += (float)MeasureTextUI("/ ", PAL_FONT);
    if (pal.cmdRow >= 0) {
        const char* name = CommandAt(FindRowAt(pal.cmdRow)->ref)->name;
        DrawTextShadow(name, (int)x, (int)y, PAL_FONT, SKYBLUE);
        x += (float)MeasureTextUI(name, PAL_FONT) + (float)MeasureTextUI(" ", PAL_FONT);
    } else if (pal.qlen == 0) {
        DrawRight(pal.path[0] ? pal.path : "type to search", w.x + w.width - PAL_PAD, y, PAL_MUTED);
    }
    DrawTextShadow(pal.query, (int)x, (int)y, PAL_FONT, WHITE);
    if (((int)(pal.blink * 2)) % 2 == 0) {
        float cx = x + (float)MeasureTextUI(pal.query, PAL_FONT) + 1;
        DrawRectangle((int)cx, (int)y, 2, PAL_FONT, WHITE);
    }
    DrawRectangle((int)w.x + 4, (int)(ListTop() - 4), (int)w.width - 8, 1, PAL_EDGE);

    // Rows
    if (pal.count == 0) {
        const char* none = pal.cmdRow >= 0 ? "Enter runs the line" : "nothing found";
        DrawTextShadow(none, (int)(w.x + PAL_PAD + 4), (int)ListTop() + 3, PAL_FONT, PAL_MUTED);
    }
    for (int v = 0; v < PAL_VISIBLE && pal.scroll + v < pal.count; v++) {
        int i = pal.scroll + v;
        const PalEntry* e = &pal.entries[i];
        Rectangle r = RowRect(v);
        float ty = r.y + 3, lx = r.x + 6, right = r.x + r.width - 6;
        if (i == pal.sel && Pickable(e->kind)) DrawRectangleRec(r, PAL_SEL);
        switch (e->kind) {
            case PE_HEADER: DrawTextShadow(e->text, (int)lx, (int)ty, PAL_FONT, PAL_HEADER); break;
            case PE_INFO:   DrawTextShadow(e->text, (int)lx + 12, (int)ty, PAL_FONT, PAL_MUTED); break;
            case PE_UP:     DrawTextShadow(".. back", (int)lx, (int)ty, PAL_FONT, PAL_MUTED); break;
            case PE_BRANCH:
                DrawTextShadow(TextFormat("%s  >", e->text), (int)lx, (int)ty, PAL_FONT, WHITE);
                break;
            case PE_CHOICE: DrawTextShadow(e->text, (int)lx, (int)ty, PAL_FONT, WHITE); break;
            case PE_ROW: {
                const FindRow* fr = FindRowAt(e->row);
                bool pin = FindIsPinned(e->row);
                const char* label = TextFormat("%s%s", pin ? "* " : "", e->text);
                DrawTextShadow(label, (int)lx, (int)ty, PAL_FONT, WHITE);
                // Search results come from all over: say where ("Enabled  Fire")
                if (pal.qlen > 0 || pal.path[0] == '\0') {
                    const char* slash = strrchr(fr->path, '/');
                    const char* where = slash ? slash + 1 : fr->path;
                    DrawTextShadow(where, (int)(lx + (float)MeasureTextUI(label, PAL_FONT) + 10), (int)ty, PAL_FONT, PAL_MUTED);
                }
                if (fr->kind == FIND_SETTING) {
                    const Setting* s = SettingAt(fr->ref);
                    char val[48];
                    SettingValueText(s, val, sizeof(val));
                    if (s->type == SETTING_BOOL) {
                        DrawRight(val, right, ty, *(bool*)s->ptr ? GREEN : PAL_MUTED);
                    } else {
                        Rectangle a = ArrowRect(v, -1), b = ArrowRect(v, 1);
                        DrawTextShadow("<", (int)a.x + 6, (int)ty, PAL_FONT, PAL_MUTED);
                        DrawTextShadow(">", (int)b.x + 6, (int)ty, PAL_FONT, PAL_MUTED);
                        float mid = (a.x + 20 + b.x) / 2;
                        DrawTextShadow(val, (int)(mid - (float)MeasureTextUI(val, PAL_FONT) / 2), (int)ty, PAL_FONT, WHITE);
                    }
                    DrawRight(FindKindName(fr->kind), right - 160, ty, PAL_MUTED);
                } else {
                    const char* tag = fr->key[0] ? TextFormat("%s   %s", fr->key, FindKindName(fr->kind))
                                                 : FindKindName(fr->kind);
                    DrawRight(tag, right, ty, PAL_MUTED);
                }
                break;
            }
        }
    }
    if (pal.count > PAL_VISIBLE) {   // scroll bar
        float top = ListTop(), h = PAL_ROW_H * PAL_VISIBLE;
        float bh = h * PAL_VISIBLE / (float)pal.count;
        float by = top + (h - bh) * (float)pal.scroll / (float)(pal.count - PAL_VISIBLE);
        DrawRectangle((int)(w.x + w.width - 4), (int)by, 3, (int)bh, PAL_EDGE);
    }

    // Footer: where the selected row lives and its help, then the keys
    float fy = ListTop() + PAL_ROW_H * PAL_VISIBLE + 4;
    DrawRectangle((int)w.x + 4, (int)fy - 2, (int)w.width - 8, 1, PAL_EDGE);
    const char* about = "";
    char usage[200];
    if (pal.cmdRow >= 0) {   // command mode: the usage made from its argument spec
        CommandUsage(CommandAt(FindRowAt(pal.cmdRow)->ref), usage, sizeof(usage));
        about = usage;
    } else if (pal.sel < pal.count && pal.entries[pal.sel].kind == PE_ROW) {
        const FindRow* fr = FindRowAt(pal.entries[pal.sel].row);
        about = fr->help ? TextFormat("%s: %s", fr->path, fr->help) : fr->path;
    }
    DrawTextShadow(about, (int)(w.x + PAL_PAD), (int)fy + 2, 14, PAL_MUTED);
    const char* keys = pal.cmdRow >= 0 ? "Enter run   Up/Down choice   Backspace/Esc back"
                                       : "Enter pick   Left/Right change   Shift+Enter/click pin   Esc close";
    DrawTextShadow(keys, (int)(w.x + PAL_PAD), (int)fy + 2 + PAL_ROW_H - 4, 14, PAL_MUTED);
}
