// selection.h - What the player has clicked on (one thing at a time).
//
// Click a mover, animal or blueprint in normal mode to select it; the inspector
// panel (src/render/inspector.c) then shows it until you click empty ground,
// press ESC, or it disappears. Actions on the selection (follow, draft) live here
// so keys and panel buttons do the same thing.
#ifndef SELECTION_H
#define SELECTION_H

#include <stdbool.h>

typedef enum {
    SEL_NONE,
    SEL_MOVER,
    SEL_ANIMAL,
    SEL_BLUEPRINT,
    SEL_WORKSHOP,
    SEL_STOCKPILE,
} SelectionKind;

typedef struct {
    SelectionKind kind;
    int idx;
} Selection;

extern Selection selection;

void Select(SelectionKind kind, int idx);
void ClearSelection(void);
// True if something is selected and it still exists (clears the selection if not).
bool SelectionValid(void);

// What is at world position (wx, wy) on level z: mover, then animal, then blueprint.
// SEL_NONE if nothing selectable is there.
Selection PickSelectionAt(float wx, float wy, int z);

// Cells a blueprint covers: a workshop blueprint its whole footprint, others one cell.
bool BlueprintFootprint(int idx, int* x0, int* y0, int* w, int* h);

// Mover actions (shared by keys and the inspector buttons)
void ToggleMoverFollow(int moverIdx);
void ToggleMoverDraft(int moverIdx);

// Workshop actions (shared by the hover keys and the inspector buttons)
void WorkshopAddBill(int wsIdx, int recipeIdx);          // "Do forever"
void WorkshopRemoveBill(int wsIdx, int billIdx);
void WorkshopCycleBillMode(int wsIdx, int billIdx);      // X times -> until X -> forever
void WorkshopAdjustBillTarget(int wsIdx, int billIdx, int delta);   // min 1; X-times / until-X only
void WorkshopToggleBillPause(int wsIdx, int billIdx);
void WorkshopToggleDeconstruct(int wsIdx);
// Linking stockpiles to a workshop is an input mode: InputMode_StartLinking (input_mode.h)

// Stockpile actions (shared by the hover keys and the inspector buttons)
void StockpileAdjustPriority(int spIdx, int delta);     // 1..9
void StockpileAdjustStackSize(int spIdx, int delta);    // 1..MAX_STACK_SIZE
void StockpileAdjustContainers(int spIdx, int delta);   // >= 0
void StockpileToggleItem(int spIdx, int filterIdx);     // one STOCKPILE_FILTERS entry
void StockpileToggleCategory(int spIdx, int category);  // all on unless all already on
void StockpileToggleAll(int spIdx);                     // all off if any on, else all on
void StockpileToggleMaterial(int spIdx, int materialFilterIdx);   // STOCKPILE_MATERIAL_FILTERS
// Items enabled / total in a filter category
void StockpileCategoryCounts(int spIdx, int category, int* enabled, int* total);

// Verbs: what can be done to a selected / hovered thing, as data (docs/todo/ui/find-palette.md
// step 4). The inspector's buttons and the hover keys read this table, and a pie menu or a
// right-click palette can too. Per-bill and per-filter rows (they need a bill / a filter) stay
// in the inspector and input.c.
typedef struct {
    SelectionKind kind;
    const char* name;                // "Follow", "Priority +": a fixed name (help, palette, tests)
    const char* (*label)(int idx);   // the button text now ("Unfollow"), NULL = name
    int key, altKey;                 // keys (raylib), 0 = none
    bool shift;                      // the key needs Shift held
    bool hoverKey;                   // RunSelectionVerbKeys reads the key (false: handled elsewhere,
                                     // e.g. F follows the hovered OR the selected mover)
    bool button;                     // the inspector draws it as a button (steppers draw their own)
    void (*run)(int idx);
} SelectionVerb;

// Dev mover actions (the mover verbs Teleport, Fill needs, Cancel job, Debug)
extern int teleportMoverIdx;   // >= 0: the next cell click teleports this mover (src/ui/palette.c)
extern int debugMoverIdx;      // >= 0: path always drawn, inspector shows DESC_DEBUG lines
bool MoverTeleportTo(int moverIdx, int x, int y, int z);   // false: not walkable / bad mover
void MoverFillNeeds(int moverIdx);
void MoverCancelJob(int moverIdx);
void ToggleMoverDebug(int moverIdx);

int SelectionVerbCount(void);
const SelectionVerb* SelectionVerbAt(int i);
// Hover keys: run the verb of this kind whose key (and Shift state) was pressed. true = one ran.
bool RunSelectionVerbKeys(SelectionKind kind, int idx);

#endif
