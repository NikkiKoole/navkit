// selection.c - What the player has clicked on (see selection.h)
#include "../entities/town.h"
#include "selection.h"
#include "../game_state.h"
#include "../entities/mover.h"
#include "../entities/animals.h"
#include "../entities/jobs.h"
#include "../entities/namegen.h"
#include "../world/designations.h"
#include "../world/construction.h"
#include "../entities/workshops.h"
#include "../entities/stockpiles.h"
#include "../entities/money.h"
#include "../entities/elevators.h"
#include "../world/cell_defs.h"
#include <math.h>

Selection selection = {SEL_NONE, -1};

void Select(SelectionKind kind, int idx) {
    selection.kind = kind;
    selection.idx = idx;
}

void ClearSelection(void) {
    selection.kind = SEL_NONE;
    selection.idx = -1;
}

bool SelectionValid(void) {
    bool ok = false;
    switch (selection.kind) {
        case SEL_NONE:      return false;
        case SEL_MOVER:     ok = selection.idx >= 0 && selection.idx < moverCount && movers[selection.idx].active; break;
        case SEL_ANIMAL:    ok = selection.idx >= 0 && selection.idx < animalCount && animals[selection.idx].active; break;
        case SEL_BLUEPRINT: ok = selection.idx >= 0 && selection.idx < MAX_BLUEPRINTS && blueprints[selection.idx].active; break;
        case SEL_WORKSHOP:  ok = selection.idx >= 0 && selection.idx < MAX_WORKSHOPS && workshops[selection.idx].active; break;
        case SEL_STOCKPILE: ok = selection.idx >= 0 && selection.idx < MAX_STOCKPILES && stockpiles[selection.idx].active; break;
        case SEL_LOT:       ok = selection.idx >= 0 && selection.idx < townLotHighWater && townLots[selection.idx].active; break;
    }
    if (!ok) ClearSelection();
    return ok;
}

// Nearest mover within 0.6 cells of (wx, wy) on level wz, or -1
int GetMoverAtWorldPos(float wx, float wy, int wz) {
    float bestDist = 999999.0f;
    int bestIdx = -1;
    float radius = CELL_SIZE * 0.6f;
    for (int i = 0; i < moverCount; i++) {
        Mover* m = &movers[i];
        if (!m->active || (int)m->z != wz) continue;
        float dx = m->x - wx, dy = m->y - wy;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist < radius && dist < bestDist) { bestDist = dist; bestIdx = i; }
    }
    return bestIdx;
}

// Nearest animal within 0.6 cells of (wx, wy) on level wz, or -1
int GetAnimalAtWorldPos(float wx, float wy, int wz) {
    float bestDist = 999999.0f;
    int bestIdx = -1;
    float radius = CELL_SIZE * 0.6f;
    for (int i = 0; i < animalCount; i++) {
        Animal* a = &animals[i];
        if (!a->active || (int)a->z != wz) continue;
        float dx = a->x - wx, dy = a->y - wy;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist < radius && dist < bestDist) { bestDist = dist; bestIdx = i; }
    }
    return bestIdx;
}

// Stockpile covering cell (gx, gy, gz), or -1
int GetStockpileAtGrid(int gx, int gy, int gz) {
    for (int i = 0; i < MAX_STOCKPILES; i++) {
        const Stockpile* sp = &stockpiles[i];
        if (!sp->active || sp->z != gz) continue;
        if (gx >= sp->x && gx < sp->x + sp->width && gy >= sp->y && gy < sp->y + sp->height) return i;
    }
    return -1;
}

bool BlueprintFootprint(int idx, int* x0, int* y0, int* w, int* h) {
    if (idx < 0 || idx >= MAX_BLUEPRINTS || !blueprints[idx].active) return false;
    const Blueprint* bp = &blueprints[idx];
    const ConstructionRecipe* r = GetConstructionRecipe(bp->recipeIndex);
    if (r && r->buildCategory == BUILD_WORKSHOP && bp->workshopType >= 0 && bp->workshopType < WORKSHOP_TYPE_COUNT) {
        // A workshop blueprint's x,y is its work tile; it covers the whole footprint
        *x0 = bp->workshopOriginX; *y0 = bp->workshopOriginY;
        *w = workshopDefs[bp->workshopType].width; *h = workshopDefs[bp->workshopType].height;
    } else {
        *x0 = bp->x; *y0 = bp->y; *w = 1; *h = 1;
    }
    return true;
}

Selection PickSelectionAt(float wx, float wy, int z) {
    int m = GetMoverAtWorldPos(wx, wy, z);
    if (m >= 0) return (Selection){SEL_MOVER, m};
    int a = GetAnimalAtWorldPos(wx, wy, z);
    if (a >= 0) return (Selection){SEL_ANIMAL, a};
    int cx = (int)floorf(wx / CELL_SIZE), cy = (int)floorf(wy / CELL_SIZE);
    if (cx >= 0 && cx < gridWidth && cy >= 0 && cy < gridHeight && z >= 0 && z < gridDepth) {
        int lot = TownLotAt(cx, cy, z);   // the lot is what a town player means (it lists its stores)
        if (lot >= 0) return (Selection){SEL_LOT, lot};
        int ws = FindWorkshopAt(cx, cy, z);
        if (ws >= 0) return (Selection){SEL_WORKSHOP, ws};
        int sp = GetStockpileAtGrid(cx, cy, z);
        if (sp >= 0) return (Selection){SEL_STOCKPILE, sp};
    }
    for (int i = 0; i < MAX_BLUEPRINTS; i++) {
        if (!blueprints[i].active || blueprints[i].z != z) continue;
        int x0, y0, w, h;
        if (BlueprintFootprint(i, &x0, &y0, &w, &h) && cx >= x0 && cx < x0 + w && cy >= y0 && cy < y0 + h)
            return (Selection){SEL_BLUEPRINT, i};
    }
    return (Selection){SEL_NONE, -1};
}

// ---------------------------------------------------------------------------
// Mover actions
// ---------------------------------------------------------------------------

void ToggleMoverFollow(int moverIdx) {
    if (moverIdx < 0 || moverIdx >= moverCount) return;
    if (followMoverIdx == moverIdx) {
        followMoverIdx = -1;
        AddMessage("Follow: off", GRAY);
    } else {
        followMoverIdx = moverIdx;
        currentViewZ = (int)movers[moverIdx].z;
        AddMessage(TextFormat("Follow: %s", MoverDisplayName(moverIdx)), GREEN);
    }
}

// ---------------------------------------------------------------------------
// Workshop actions
// ---------------------------------------------------------------------------

static Bill* WorkshopBill(int wsIdx, int billIdx) {
    if (wsIdx < 0 || wsIdx >= MAX_WORKSHOPS || !workshops[wsIdx].active) return NULL;
    if (billIdx < 0 || billIdx >= workshops[wsIdx].billCount) return NULL;
    return &workshops[wsIdx].bills[billIdx];
}

void WorkshopAddBill(int wsIdx, int recipeIdx) {
    if (wsIdx < 0 || wsIdx >= MAX_WORKSHOPS || !workshops[wsIdx].active) return;
    Workshop* ws = &workshops[wsIdx];
    int recipeCount;
    const Recipe* recipes = GetRecipesForWorkshop(ws->type, &recipeCount);
    if (recipeIdx < 0 || recipeIdx >= recipeCount) return;
    if (ws->billCount >= MAX_BILLS_PER_WORKSHOP) {
        AddMessage(TextFormat("Workshop has max bills (%d)", MAX_BILLS_PER_WORKSHOP), RED);
        return;
    }
    AddBill(wsIdx, recipeIdx, BILL_DO_FOREVER, 0);
    AddMessage(TextFormat("Added bill: %s (Do Forever)", recipes[recipeIdx].name), GREEN);
}

void WorkshopRemoveBill(int wsIdx, int billIdx) {
    if (!WorkshopBill(wsIdx, billIdx)) return;
    RemoveBill(wsIdx, billIdx);
    AddMessage(TextFormat("Removed bill (now %d)", workshops[wsIdx].billCount), ORANGE);
}

void WorkshopCycleBillMode(int wsIdx, int billIdx) {
    Bill* bill = WorkshopBill(wsIdx, billIdx);
    if (!bill) return;
    static const char* modeNames[] = {"Do X Times", "Do Until X", "Do Forever"};
    bill->mode = (bill->mode + 1) % 3;
    if ((bill->mode == BILL_DO_X_TIMES || bill->mode == BILL_DO_UNTIL_X) && bill->targetCount <= 0)
        bill->targetCount = 5;
    bill->completedCount = 0;
    AddMessage(TextFormat("Bill mode: %s", modeNames[bill->mode]), WHITE);
}

void WorkshopAdjustBillTarget(int wsIdx, int billIdx, int delta) {
    Bill* bill = WorkshopBill(wsIdx, billIdx);
    if (!bill || (bill->mode != BILL_DO_X_TIMES && bill->mode != BILL_DO_UNTIL_X)) return;
    bill->targetCount += delta;
    if (bill->targetCount < 1) bill->targetCount = 1;
    AddMessage(TextFormat("Target: %d", bill->targetCount), WHITE);
}

void WorkshopToggleBillPause(int wsIdx, int billIdx) {
    Bill* bill = WorkshopBill(wsIdx, billIdx);
    if (!bill) return;
    bill->suspended = !bill->suspended;
    bill->suspendedNoStorage = false;
    AddMessage(TextFormat("Bill %s", bill->suspended ? "PAUSED" : "RESUMED"), bill->suspended ? RED : GREEN);
}

void WorkshopToggleDeconstruct(int wsIdx) {
    if (wsIdx < 0 || wsIdx >= MAX_WORKSHOPS || !workshops[wsIdx].active) return;
    Workshop* ws = &workshops[wsIdx];
    if (ws->markedForDeconstruct) {
        if (ws->assignedDeconstructor >= 0) {
            Mover* dm = &movers[ws->assignedDeconstructor];
            if (dm->active && dm->currentJobId >= 0) CancelJob(dm, ws->assignedDeconstructor);
            ws->assignedDeconstructor = -1;
        }
        ws->markedForDeconstruct = false;
        AddMessage("Deconstruction cancelled", YELLOW);
    } else {
        // Cancel the craft/deliver/ignite jobs running at this workshop
        for (int ji = activeJobCount - 1; ji >= 0; ji--) {
            Job* j = &jobs[activeJobList[ji]];
            if (j->targetWorkshop == wsIdx && j->assignedMover >= 0) CancelJob(&movers[j->assignedMover], j->assignedMover);
        }
        ws->markedForDeconstruct = true;
        AddMessage("Marked for deconstruction", ORANGE);
    }
}

// ---------------------------------------------------------------------------
// Stockpile actions
// ---------------------------------------------------------------------------

static Stockpile* ActiveStockpile(int spIdx) {
    if (spIdx < 0 || spIdx >= MAX_STOCKPILES || !stockpiles[spIdx].active) return NULL;
    return &stockpiles[spIdx];
}

void StockpileAdjustPriority(int spIdx, int delta) {
    Stockpile* sp = ActiveStockpile(spIdx);
    if (!sp) return;
    int p = sp->priority + delta;
    if (p < 1) p = 1;
    if (p > 9) p = 9;
    if (p == sp->priority) return;
    sp->priority = p;
    AddMessage(TextFormat("Stockpile priority: %d", sp->priority), WHITE);
}

void StockpileAdjustStackSize(int spIdx, int delta) {
    Stockpile* sp = ActiveStockpile(spIdx);
    if (!sp) return;
    int n = sp->maxStackSize + delta;
    if (n < 1 || n > MAX_STACK_SIZE) return;
    SetStockpileMaxStackSize(spIdx, n);
    AddMessage(TextFormat("Stack size: %d", sp->maxStackSize), WHITE);
}

void StockpileAdjustContainers(int spIdx, int delta) {
    Stockpile* sp = ActiveStockpile(spIdx);
    if (!sp) return;
    int n = sp->maxContainers + delta;
    if (n < 0) return;
    SetStockpileMaxContainers(spIdx, n);
    AddMessage(TextFormat("Max containers: %d", sp->maxContainers), WHITE);
}

void StockpileToggleItem(int spIdx, int filterIdx) {
    Stockpile* sp = ActiveStockpile(spIdx);
    if (!sp || filterIdx < 0 || filterIdx >= STOCKPILE_FILTER_COUNT) return;
    const StockpileFilterDef* f = &STOCKPILE_FILTERS[filterIdx];
    sp->allowedTypes[f->itemType] = !sp->allowedTypes[f->itemType];
    InvalidateStockpileSlotCacheAll();
    AddMessage(TextFormat("%s: %s", f->displayName, sp->allowedTypes[f->itemType] ? "ON" : "OFF"), f->color);
}

void StockpileCategoryCounts(int spIdx, int category, int* enabled, int* total) {
    *enabled = *total = 0;
    const Stockpile* sp = ActiveStockpile(spIdx);
    if (!sp) return;
    for (int i = 0; i < STOCKPILE_FILTER_COUNT; i++) {
        if ((int)STOCKPILE_FILTERS[i].category != category) continue;
        (*total)++;
        if (sp->allowedTypes[STOCKPILE_FILTERS[i].itemType]) (*enabled)++;
    }
}

void StockpileToggleCategory(int spIdx, int category) {
    Stockpile* sp = ActiveStockpile(spIdx);
    if (!sp || category < 0 || category >= FILTER_CAT_COUNT) return;
    int enabled, total;
    StockpileCategoryCounts(spIdx, category, &enabled, &total);
    bool on = enabled != total;
    for (int i = 0; i < STOCKPILE_FILTER_COUNT; i++)
        if ((int)STOCKPILE_FILTERS[i].category == category) sp->allowedTypes[STOCKPILE_FILTERS[i].itemType] = on;
    InvalidateStockpileSlotCacheAll();
    AddMessage(TextFormat("%s: %s", FILTER_CATEGORY_NAMES[category], on ? "all ON" : "all OFF"), WHITE);
}

void StockpileToggleAll(int spIdx) {
    Stockpile* sp = ActiveStockpile(spIdx);
    if (!sp) return;
    bool anyOn = false;
    for (int i = 0; i < ITEM_TYPE_COUNT; i++) if (sp->allowedTypes[i]) { anyOn = true; break; }
    for (int i = 0; i < ITEM_TYPE_COUNT; i++) sp->allowedTypes[i] = !anyOn;
    InvalidateStockpileSlotCacheAll();
    AddMessage(!anyOn ? "All filters ON" : "All filters OFF", WHITE);
}

void StockpileToggleForSale(int spIdx) {
    if (!ActiveStockpile(spIdx)) return;
    ShopSetForSale(spIdx, !stockpileForSale[spIdx]);
    AddMessage(stockpileForSale[spIdx] ? "For sale: taking from it costs its price (money)" : "Not for sale: free again", WHITE);
}

void StockpileToggleMaterial(int spIdx, int materialFilterIdx) {
    Stockpile* sp = ActiveStockpile(spIdx);
    if (!sp || materialFilterIdx < 0 || materialFilterIdx >= STOCKPILE_MATERIAL_FILTER_COUNT) return;
    const StockpileMaterialFilterDef* mf = &STOCKPILE_MATERIAL_FILTERS[materialFilterIdx];
    bool on = !sp->allowedMaterials[mf->material];
    sp->allowedMaterials[mf->material] = on;
    if (on) sp->allowedTypes[mf->parentItem] = true;   // a wood type implies its item
    InvalidateStockpileSlotCacheAll();
    AddMessage(TextFormat("%s: %s", mf->displayName, on ? "ON" : "OFF"), mf->color);
}

void ToggleMoverDraft(int moverIdx) {
    if (moverIdx < 0 || moverIdx >= moverCount) return;
    Mover* m = &movers[moverIdx];
    m->isDrafted = !m->isDrafted;
    if (m->isDrafted && m->currentJobId >= 0) CancelJob(m, moverIdx);
    AddMessage(TextFormat("%s: %s", MoverDisplayName(moverIdx), m->isDrafted ? "DRAFTED" : "Undrafted"),
               m->isDrafted ? RED : GREEN);
}

// ---------------------------------------------------------------------------
// Dev mover actions (verbs below; docs/todo/ui/find-palette.md)
// ---------------------------------------------------------------------------

int teleportMoverIdx = -1;   // Teleport: waiting for a cell click (src/ui/palette.c)
int debugMoverIdx = -1;      // Debug: this mover's path always drawn, inspector shows debug lines

static bool MoverOk(int i) { return i >= 0 && i < moverCount && movers[i].active; }

bool MoverTeleportTo(int moverIdx, int x, int y, int z) {
    if (!MoverOk(moverIdx)) return false;
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth || !IsCellWalkableAt(z, y, x)) {
        AddMessage("Can't stand there", YELLOW);
        return false;
    }
    Mover* m = &movers[moverIdx];
    if (m->currentJobId >= 0) CancelJob(m, moverIdx);
    ElevatorForgetMover(moverIdx);
    m->x = x * CELL_SIZE + CELL_SIZE * 0.5f;
    m->y = y * CELL_SIZE + CELL_SIZE * 0.5f;
    m->z = (float)z;
    m->goal = (Point){ x, y, z };
    ClearMoverPath(moverIdx);
    m->needsRepath = false;
    m->lastX = m->x; m->lastY = m->y; m->lastZ = m->z;
    m->timeWithoutProgress = 0;
    AddMessage(TextFormat("%s teleported to %d,%d,%d", MoverDisplayName(moverIdx), x, y, z), SKYBLUE);
    return true;
}

void MoverFillNeeds(int moverIdx) {
    if (!MoverOk(moverIdx)) return;
    Mover* m = &movers[moverIdx];
    m->hunger = m->thirst = m->energy = m->bladder = 1.0f;
    m->bodyTemp = 37.0f;
    m->starvationTimer = m->dehydrationTimer = 0;
    AddMessage(TextFormat("%s: fed, watered, rested", MoverDisplayName(moverIdx)), GREEN);
}

void MoverCancelJob(int moverIdx) {
    if (!MoverOk(moverIdx)) return;
    Mover* m = &movers[moverIdx];
    if (m->currentJobId < 0) { AddMessage(TextFormat("%s has no job", MoverDisplayName(moverIdx)), GRAY); return; }
    CancelJob(m, moverIdx);
    AddMessage(TextFormat("%s: job cancelled", MoverDisplayName(moverIdx)), YELLOW);
}

void ToggleMoverDebug(int moverIdx) {
    if (!MoverOk(moverIdx)) return;
    debugMoverIdx = debugMoverIdx == moverIdx ? -1 : moverIdx;
    if (debugMoverIdx >= 0) Select(SEL_MOVER, moverIdx);   // the inspector shows the debug lines
}

static void VerbTeleport(int i) {
    if (!MoverOk(i)) return;
    teleportMoverIdx = i;
    AddMessage(TextFormat("Teleport %s: L-click a cell (R-click / Esc cancels)", MoverDisplayName(i)), SKYBLUE);
}

// ---------------------------------------------------------------------------
// Verbs (selection.h SelectionVerb): one row per thing you can do to a selected thing
// ---------------------------------------------------------------------------

void InputMode_StartLinking(int wsIdx);   // input_mode.c: linking a workshop is an input mode

static const char* FollowLabel(int i) { return followMoverIdx == i ? "Unfollow" : "Follow"; }
static const char* DebugLabel(int i) { return debugMoverIdx == i ? "Debug off" : "Debug"; }
static const char* SellLabel(int i) { return (i >= 0 && i < MAX_STOCKPILES && stockpileForSale[i]) ? "Stop selling" : "Sell"; }
static const char* DraftLabel(int i) { return (i >= 0 && i < moverCount && movers[i].isDrafted) ? "Undraft" : "Draft"; }
static const char* DeconstructLabel(int i) {
    return (i >= 0 && i < MAX_WORKSHOPS && workshops[i].markedForDeconstruct) ? "Cancel deconstruct" : "Deconstruct";
}
static void VerbLink(int i) { InputMode_StartLinking(i); }
static void VerbPriorityUp(int i) { StockpileAdjustPriority(i, 1); }
static void VerbPriorityDown(int i) { StockpileAdjustPriority(i, -1); }
static void VerbStackUp(int i) { StockpileAdjustStackSize(i, 1); }
static void VerbStackDown(int i) { StockpileAdjustStackSize(i, -1); }
static void VerbContainersUp(int i) { StockpileAdjustContainers(i, 1); }
static void VerbContainersDown(int i) { StockpileAdjustContainers(i, -1); }

static const SelectionVerb selectionVerbs[] = {
    // Mover. F: follow the hovered mover, else the selected one (input.c, global key)
    { SEL_MOVER, "Follow", FollowLabel, KEY_F, 0, false, false, true, ToggleMoverFollow },
    { SEL_MOVER, "Draft", DraftLabel, KEY_D, 0, false, true, true, ToggleMoverDraft },
    // Dev: no keys (palette and inspector)
    { SEL_MOVER, "Teleport", NULL, 0, 0, false, false, true, VerbTeleport },
    { SEL_MOVER, "Fill needs", NULL, 0, 0, false, false, true, MoverFillNeeds },
    { SEL_MOVER, "Cancel job", NULL, 0, 0, false, false, true, MoverCancelJob },
    { SEL_MOVER, "Debug", DebugLabel, 0, 0, false, false, true, ToggleMoverDebug },
    // Workshop (bills: per bill in the inspector, and on the hover keys 1-9 [ ] M + - X P)
    { SEL_WORKSHOP, "Link stockpile", NULL, KEY_L, 0, false, true, true, VerbLink },
    { SEL_WORKSHOP, "Deconstruct", DeconstructLabel, KEY_D, 0, false, true, true, WorkshopToggleDeconstruct },
    // Stockpile (the inspector draws these as steppers with the value between - and +)
    { SEL_STOCKPILE, "Priority +", NULL, KEY_EQUAL, KEY_KP_ADD, false, true, false, VerbPriorityUp },
    { SEL_STOCKPILE, "Priority -", NULL, KEY_MINUS, KEY_KP_SUBTRACT, false, true, false, VerbPriorityDown },
    { SEL_STOCKPILE, "Stack +", NULL, KEY_RIGHT_BRACKET, 0, false, true, false, VerbStackUp },
    { SEL_STOCKPILE, "Stack -", NULL, KEY_LEFT_BRACKET, 0, false, true, false, VerbStackDown },
    { SEL_STOCKPILE, "Containers +", NULL, KEY_RIGHT_BRACKET, 0, true, true, false, VerbContainersUp },
    { SEL_STOCKPILE, "Containers -", NULL, KEY_LEFT_BRACKET, 0, true, true, false, VerbContainersDown },
    // X: all filters on/off, only at the top filter level (input.c, next to the category keys)
    { SEL_STOCKPILE, "All on/off", NULL, KEY_X, 0, false, false, false, StockpileToggleAll },   // drawn in the filters row
    { SEL_STOCKPILE, "Sell", SellLabel, 0, 0, false, false, false, StockpileToggleForSale },   // drawn after the steppers
};
#define SELECTION_VERB_COUNT ((int)(sizeof(selectionVerbs) / sizeof(selectionVerbs[0])))

int SelectionVerbCount(void) { return SELECTION_VERB_COUNT; }
const SelectionVerb* SelectionVerbAt(int i) { return (i >= 0 && i < SELECTION_VERB_COUNT) ? &selectionVerbs[i] : NULL; }

bool RunSelectionVerbKeys(SelectionKind kind, int idx) {
    bool shiftDown = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    for (int i = 0; i < SELECTION_VERB_COUNT; i++) {
        const SelectionVerb* v = &selectionVerbs[i];
        if (v->kind != kind || !v->hoverKey || v->shift != shiftDown) continue;
        if ((v->key && IsKeyPressed(v->key)) || (v->altKey && IsKeyPressed(v->altKey))) { v->run(idx); return true; }
    }
    return false;
}
