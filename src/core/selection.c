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

void ToggleMoverDraft(int moverIdx) {
    if (moverIdx < 0 || moverIdx >= moverCount) return;
    Mover* m = &movers[moverIdx];
    m->isDrafted = !m->isDrafted;
    if (m->isDrafted && m->currentJobId >= 0) CancelJob(m, moverIdx);
    AddMessage(TextFormat("%s: %s", MoverDisplayName(moverIdx), m->isDrafted ? "DRAFTED" : "Undrafted"),
               m->isDrafted ? RED : GREEN);
}
