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

#endif
