// selection.c - What the player has clicked on (see selection.h)
#include "selection.h"
#include "../game_state.h"
#include "../entities/mover.h"
#include "../entities/animals.h"
#include "../entities/jobs.h"
#include "../entities/namegen.h"
#include "../world/designations.h"
#include "../world/construction.h"
#include "../entities/workshops.h"
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
        int ws = FindWorkshopAt(cx, cy, z);
        if (ws >= 0) return (Selection){SEL_WORKSHOP, ws};
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

void ToggleMoverDraft(int moverIdx) {
    if (moverIdx < 0 || moverIdx >= moverCount) return;
    Mover* m = &movers[moverIdx];
    m->isDrafted = !m->isDrafted;
    if (m->isDrafted && m->currentJobId >= 0) CancelJob(m, moverIdx);
    AddMessage(TextFormat("%s: %s", MoverDisplayName(moverIdx), m->isDrafted ? "DRAFTED" : "Undrafted"),
               m->isDrafted ? RED : GREEN);
}
