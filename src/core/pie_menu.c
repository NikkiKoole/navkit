// core/pie_menu.c - Radial pie menu system (concentric rings, label-only)
#include "pie_menu.h"
#include "../game_state.h"
#include <math.h>

// ============================================================================
// Menu definitions
// ============================================================================

enum {
    MENU_ROOT,
    MENU_WORK,
    MENU_DIG,
    MENU_BUILD,
    MENU_HARVEST,
    MENU_DRAW,
    MENU_SANDBOX,
    MENU_DRAW_WORKSHOP,
    MENU_DRAW_SOIL,
    MENU_COUNT
};

static PieMenuDef menus[MENU_COUNT] = {
    // MENU_ROOT
    { "Menu", {
        { "Work",    ACTION_NONE, MENU_WORK,    SKYBLUE },
        { "Draw",    ACTION_NONE, MENU_DRAW,    GREEN },
        { "Sandbox", ACTION_NONE, MENU_SANDBOX, ORANGE },
    }, 3 },

    // MENU_WORK
    { "Work", {
        { "Dig",     ACTION_NONE,        MENU_DIG,     YELLOW },
        { "Build",   ACTION_NONE,        MENU_BUILD,   BLUE },
        { "Harvest", ACTION_NONE,        MENU_HARVEST, GREEN },
        { "Gather",  ACTION_WORK_GATHER, -1,           LIME },
    }, 4 },

    // MENU_DIG
    { "Dig", {
        { "Mine",      ACTION_WORK_MINE,         -1, YELLOW },
        { "Channel",   ACTION_WORK_CHANNEL,      -1, ORANGE },
        { "Dig Ramp",  ACTION_WORK_DIG_RAMP,     -1, BROWN },
        { "Rem Floor", ACTION_WORK_REMOVE_FLOOR,  -1, RED },
        { "Rem Ramp",  ACTION_WORK_REMOVE_RAMP,   -1, MAROON },
    }, 5 },

    // MENU_BUILD
    { "Build", {
        { "Wall",    ACTION_WORK_CONSTRUCT, -1, BLUE },
        { "Floor",   ACTION_WORK_FLOOR,     -1, SKYBLUE },
        { "Ladder",  ACTION_WORK_LADDER,    -1, BROWN },
        { "Ramp",    ACTION_WORK_RAMP,      -1, GRAY },
    }, 4 },

    // MENU_HARVEST
    { "Harvest", {
        { "Chop Tree",     ACTION_WORK_CHOP,           -1, GREEN },
        { "Chop Felled",   ACTION_WORK_CHOP_FELLED,    -1, DARKGREEN },
        { "Gather Sapl.",  ACTION_WORK_GATHER_SAPLING,  -1, LIME },
        { "Plant Sapl.",   ACTION_WORK_PLANT_SAPLING,   -1, DARKGREEN },
    }, 4 },

    // MENU_DRAW
    { "Draw", {
        { "Wall",      ACTION_DRAW_WALL,      -1,              BLUE },
        { "Floor",     ACTION_DRAW_FLOOR,      -1,              SKYBLUE },
        { "Ladder",    ACTION_DRAW_LADDER,     -1,              BROWN },
        { "Ramp",      ACTION_DRAW_RAMP,       -1,              GRAY },
        { "Stockpile", ACTION_DRAW_STOCKPILE,  -1,              GREEN },
        { "Dirt",      ACTION_DRAW_DIRT,       -1,              BROWN },
        { "Rock",      ACTION_DRAW_ROCK,       -1,              DARKGRAY },
        { "Workshop",  ACTION_NONE,            MENU_DRAW_WORKSHOP, ORANGE },
    }, 8 },

    // MENU_SANDBOX
    { "Sandbox", {
        { "Water",  ACTION_SANDBOX_WATER,  -1, BLUE },
        { "Fire",   ACTION_SANDBOX_FIRE,   -1, RED },
        { "Heat",   ACTION_SANDBOX_HEAT,   -1, ORANGE },
        { "Cold",   ACTION_SANDBOX_COLD,   -1, SKYBLUE },
        { "Smoke",  ACTION_SANDBOX_SMOKE,  -1, GRAY },
        { "Steam",  ACTION_SANDBOX_STEAM,  -1, WHITE },
        { "Grass",  ACTION_SANDBOX_GRASS,  -1, GREEN },
        { "Tree",   ACTION_SANDBOX_TREE,   -1, DARKGREEN },
    }, 8 },

    // MENU_DRAW_WORKSHOP
    { "Workshop", {
        { "Stonecutter", ACTION_DRAW_WORKSHOP_STONECUTTER, -1, GRAY },
        { "Sawmill",     ACTION_DRAW_WORKSHOP_SAWMILL,     -1, BROWN },
        { "Kiln",        ACTION_DRAW_WORKSHOP_KILN,        -1, ORANGE },
    }, 3 },

    // MENU_DRAW_SOIL
    { "Soil", {
        { "Dirt",   ACTION_DRAW_SOIL_DIRT,   -1, BROWN },
        { "Clay",   ACTION_DRAW_SOIL_CLAY,   -1, ORANGE },
        { "Gravel", ACTION_DRAW_SOIL_GRAVEL, -1, GRAY },
        { "Sand",   ACTION_DRAW_SOIL_SAND,   -1, YELLOW },
        { "Peat",   ACTION_DRAW_SOIL_PEAT,   -1, DARKBROWN },
    }, 5 },
};

// ============================================================================
// State
// ============================================================================

static PieMenuState pieState = { .isOpen = false };
static double pieClosedTime = -1.0;
static bool pieHoldMode = false;
static int pieDeepestRing = 0;  // Deepest ring the cursor has reached

// ============================================================================
// Helpers
// ============================================================================

static float RingRadius(int ring) {
    // Distance from center to label position for this ring
    return PIE_DEAD_ZONE + PIE_RING_WIDTH * 0.5f + ring * PIE_RING_WIDTH;
}

static float RingInner(int ring) {
    return PIE_DEAD_ZONE + ring * PIE_RING_WIDTH;
}

static float RingOuter(int ring) {
    return PIE_DEAD_ZONE + (ring + 1) * PIE_RING_WIDTH;
}

// Normalize angle: 0 = up (north), clockwise, in range [0, 2*PI)
static float NormalizeAngle(float dx, float dy) {
    float angle = atan2f(dy, dx);
    return fmodf(angle + PI * 0.5f + PI * 2.0f, PI * 2.0f);
}

// Check if angle is within arc [start, end) (handles wraparound)
static bool AngleInArc(float angle, float arcStart, float arcEnd) {
    // Normalize everything to [0, 2*PI)
    float a = fmodf(angle + PI * 4.0f, PI * 2.0f);
    float s = fmodf(arcStart + PI * 4.0f, PI * 2.0f);
    float e = fmodf(arcEnd + PI * 4.0f, PI * 2.0f);

    if (s <= e) {
        return a >= s && a < e;
    } else {
        // Wraps around 0
        return a >= s || a < e;
    }
}

// Get slice index for an angle within a ring's arc
static int AngleToSliceInArc(float angle, float arcStart, float arcEnd, int sliceCount) {
    // Normalize to [0, 2*PI) first
    float a = fmodf(angle + PI * 4.0f, PI * 2.0f);
    float s = fmodf(arcStart + PI * 4.0f, PI * 2.0f);
    float e = fmodf(arcEnd + PI * 4.0f, PI * 2.0f);

    float arcSize = e - s;
    if (arcSize <= 0) arcSize += PI * 2.0f;

    float rel = a - s;
    if (rel < 0) rel += PI * 2.0f;

    float t = rel / arcSize;
    int idx = (int)(t * sliceCount);
    if (idx >= sliceCount) idx = sliceCount - 1;
    if (idx < 0) idx = 0;
    return idx;
}

// Get the angular position of a slice within an arc
static float SliceAngle(int sliceIdx, float arcStart, float arcEnd, int sliceCount) {
    float s = fmodf(arcStart + PI * 4.0f, PI * 2.0f);
    float e = fmodf(arcEnd + PI * 4.0f, PI * 2.0f);
    float arcSize = e - s;
    if (arcSize <= 0) arcSize += PI * 2.0f;
    return s + arcSize * ((float)sliceIdx + 0.5f) / (float)sliceCount;
}

// ============================================================================
// Action application
// ============================================================================

static void PieMenu_ApplyAction(InputAction action) {
    switch (action) {
        case ACTION_WORK_MINE: case ACTION_WORK_CHANNEL:
        case ACTION_WORK_DIG_RAMP: case ACTION_WORK_REMOVE_FLOOR:
        case ACTION_WORK_REMOVE_RAMP:
            inputMode = MODE_WORK; workSubMode = SUBMODE_DIG; break;
        case ACTION_WORK_CONSTRUCT: case ACTION_WORK_FLOOR:
        case ACTION_WORK_LADDER: case ACTION_WORK_RAMP:
            inputMode = MODE_WORK; workSubMode = SUBMODE_BUILD; break;
        case ACTION_WORK_CHOP: case ACTION_WORK_CHOP_FELLED:
        case ACTION_WORK_GATHER_SAPLING: case ACTION_WORK_PLANT_SAPLING:
            inputMode = MODE_WORK; workSubMode = SUBMODE_HARVEST; break;
        case ACTION_WORK_GATHER:
            inputMode = MODE_WORK; workSubMode = SUBMODE_NONE; break;
        case ACTION_DRAW_WALL: case ACTION_DRAW_FLOOR:
        case ACTION_DRAW_LADDER: case ACTION_DRAW_RAMP:
        case ACTION_DRAW_STOCKPILE: case ACTION_DRAW_DIRT:
        case ACTION_DRAW_ROCK:
        case ACTION_DRAW_WORKSHOP_STONECUTTER: case ACTION_DRAW_WORKSHOP_SAWMILL:
        case ACTION_DRAW_WORKSHOP_KILN:
        case ACTION_DRAW_SOIL_DIRT: case ACTION_DRAW_SOIL_CLAY:
        case ACTION_DRAW_SOIL_GRAVEL: case ACTION_DRAW_SOIL_SAND:
        case ACTION_DRAW_SOIL_PEAT:
            inputMode = MODE_DRAW; workSubMode = SUBMODE_NONE; break;
        case ACTION_SANDBOX_WATER: case ACTION_SANDBOX_FIRE:
        case ACTION_SANDBOX_HEAT: case ACTION_SANDBOX_COLD:
        case ACTION_SANDBOX_SMOKE: case ACTION_SANDBOX_STEAM:
        case ACTION_SANDBOX_GRASS: case ACTION_SANDBOX_TREE:
            inputMode = MODE_SANDBOX; workSubMode = SUBMODE_NONE; break;
        default: break;
    }
    inputAction = action;
}

// ============================================================================
// API
// ============================================================================

void PieMenu_Open(float x, float y) {
    InputMode_Reset();
    pieState.isOpen = true;
    pieState.centerX = x;
    pieState.centerY = y;
    pieState.hoveredRing = -1;
    pieState.hoveredSlice = -1;
    pieState.ringMenu[0] = MENU_ROOT;
    pieState.ringArcStart[0] = 0.0f;
    pieState.ringArcEnd[0] = PI * 2.0f;
    pieState.visibleRings = 1;
    for (int i = 0; i < PIE_MAX_DEPTH; i++) {
        pieState.ringSelection[i] = -1;
        if (i > 0) {
            pieState.ringMenu[i] = -1;
            pieState.ringArcStart[i] = 0.0f;
            pieState.ringArcEnd[i] = 0.0f;
        }
    }
    pieHoldMode = false;
    pieDeepestRing = 0;
}

void PieMenu_OpenHold(float x, float y) {
    PieMenu_Open(x, y);
    pieHoldMode = true;
}

void PieMenu_Close(void) {
    pieState.isOpen = false;
    pieClosedTime = GetTime();
}

void PieMenu_Back(void) {
    if (pieState.visibleRings > 1) {
        pieState.visibleRings--;
        pieState.ringSelection[pieState.visibleRings] = -1;
        pieState.ringMenu[pieState.visibleRings] = -1;
        pieState.ringSelection[pieState.visibleRings - 1] = -1;
    } else {
        PieMenu_Close();
    }
}

bool PieMenu_IsOpen(void) {
    return pieState.isOpen;
}

bool PieMenu_JustClosed(void) {
    return pieClosedTime > 0.0 && (GetTime() - pieClosedTime) < 0.05;
}

// ============================================================================
// Update
// ============================================================================

// Expand a child ring from a parent slice
static void OpenChildRing(int parentRing, int parentSlice) {
    int parentMenu = pieState.ringMenu[parentRing];
    PieSlice* s = &menus[parentMenu].slices[parentSlice];
    if (s->childMenuIdx < 0 || parentRing + 1 >= PIE_MAX_DEPTH) return;

    int childRing = parentRing + 1;
    pieState.ringMenu[childRing] = s->childMenuIdx;
    pieState.ringSelection[childRing] = -1;

    // Child arc = angular span of the parent slice
    float parentArcStart = pieState.ringArcStart[parentRing];
    float parentArcEnd = pieState.ringArcEnd[parentRing];
    float parentArcSize = parentArcEnd - parentArcStart;
    if (parentArcSize < 0) parentArcSize += PI * 2.0f;
    int parentCount = menus[parentMenu].sliceCount;
    float sliceSize = parentArcSize / (float)parentCount;

    // Center child arc on the parent slice's midpoint
    float parentSliceMid = parentArcStart + (parentSlice + 0.5f) * sliceSize;

    // Ensure minimum arc so labels don't overlap
    // Each child item needs at least ~30 degrees (PI/6 radians)
    int childCount = menus[s->childMenuIdx].sliceCount;
    float minArc = childCount * (PI / 6.0f);
    float childArc = sliceSize;
    if (childArc < minArc) childArc = minArc;
    // Cap at full circle
    if (childArc > PI * 2.0f) childArc = PI * 2.0f;

    float childStart = parentSliceMid - childArc * 0.5f;
    float childEnd = parentSliceMid + childArc * 0.5f;

    pieState.ringArcStart[childRing] = childStart;
    pieState.ringArcEnd[childRing] = childEnd;
    pieState.visibleRings = childRing + 1;

    // Clear deeper rings
    for (int r = childRing + 1; r < PIE_MAX_DEPTH; r++) {
        pieState.ringSelection[r] = -1;
        pieState.ringMenu[r] = -1;
    }
}

void PieMenu_Update(void) {
    if (!pieState.isOpen) return;

    Vector2 mouse = GetMousePosition();
    float dx = mouse.x - pieState.centerX;
    float dy = mouse.y - pieState.centerY;
    float dist = sqrtf(dx * dx + dy * dy);
    float angle = NormalizeAngle(dx, dy);

    // Determine which ring and slice the cursor is in
    pieState.hoveredRing = -1;
    pieState.hoveredSlice = -1;

    if (dist >= PIE_DEAD_ZONE) {
        for (int r = 0; r < pieState.visibleRings; r++) {
            int menuIdx = pieState.ringMenu[r];
            if (menuIdx < 0) continue;

            if (dist >= RingInner(r) && dist < RingOuter(r) &&
                AngleInArc(angle, pieState.ringArcStart[r], pieState.ringArcEnd[r])) {
                pieState.hoveredRing = r;
                pieState.hoveredSlice = AngleToSliceInArc(angle,
                    pieState.ringArcStart[r], pieState.ringArcEnd[r],
                    menus[menuIdx].sliceCount);
                break;
            }
        }
        // Past outermost ring but within its arc — clamp to outermost
        if (pieState.hoveredRing == -1) {
            int lastRing = pieState.visibleRings - 1;
            int menuIdx = pieState.ringMenu[lastRing];
            if (menuIdx >= 0 && dist >= RingOuter(lastRing) &&
                AngleInArc(angle, pieState.ringArcStart[lastRing], pieState.ringArcEnd[lastRing])) {
                pieState.hoveredRing = lastRing;
                pieState.hoveredSlice = AngleToSliceInArc(angle,
                    pieState.ringArcStart[lastRing], pieState.ringArcEnd[lastRing],
                    menus[menuIdx].sliceCount);
            }
        }
    }

    // Track deepest ring visited
    if (pieState.hoveredRing > pieDeepestRing) {
        pieDeepestRing = pieState.hoveredRing;
    }

    // Update ring selections based on hover
    if (pieState.hoveredRing >= 0 && pieState.hoveredSlice >= 0) {
        int ring = pieState.hoveredRing;
        int slice = pieState.hoveredSlice;

        if (pieState.ringSelection[ring] != slice) {
            pieState.ringSelection[ring] = slice;

            // New selection on this ring = new path, reset deepest
            pieDeepestRing = ring;

            // Clear deeper rings
            for (int r = ring + 1; r < PIE_MAX_DEPTH; r++) {
                pieState.ringSelection[r] = -1;
                pieState.ringMenu[r] = -1;
            }

            // Open child ring if this slice has children
            int menuIdx = pieState.ringMenu[ring];
            PieSlice* s = &menus[menuIdx].slices[slice];
            if (s->childMenuIdx >= 0 && ring + 1 < PIE_MAX_DEPTH) {
                OpenChildRing(ring, slice);
            } else {
                pieState.visibleRings = ring + 1;
            }
        }
    }

    // When hovering an inner ring, collapse outer rings
    if (pieState.hoveredRing >= 0 && pieState.hoveredRing < pieState.visibleRings - 1) {
        int ring = pieState.hoveredRing;
        int menuIdx = pieState.ringMenu[ring];
        int slice = pieState.hoveredSlice;
        PieSlice* s = &menus[menuIdx].slices[slice];

        int newVisible = ring + 1;
        if (s->childMenuIdx >= 0 && ring + 1 < PIE_MAX_DEPTH) {
            OpenChildRing(ring, slice);
            newVisible = ring + 2;
        }

        for (int r = newVisible; r < PIE_MAX_DEPTH; r++) {
            pieState.ringSelection[r] = -1;
            pieState.ringMenu[r] = -1;
        }
        pieState.visibleRings = newVisible;
    }

    // Left-click to select
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (pieState.hoveredRing >= 0 && pieState.hoveredSlice >= 0) {
            int menuIdx = pieState.ringMenu[pieState.hoveredRing];
            PieSlice* slice = &menus[menuIdx].slices[pieState.hoveredSlice];
            if (slice->childMenuIdx < 0) {
                PieMenu_ApplyAction(slice->action);
                PieMenu_Close();
            }
        } else if (pieState.hoveredRing == -1) {
            PieMenu_Close();
        }
    }

    // Hold-drag mode: release on leaf selects
    if (pieHoldMode && IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
        if (pieState.hoveredRing >= 0 && pieState.hoveredSlice >= 0) {
            int menuIdx = pieState.ringMenu[pieState.hoveredRing];
            PieSlice* slice = &menus[menuIdx].slices[pieState.hoveredSlice];
            if (slice->childMenuIdx < 0) {
                PieMenu_ApplyAction(slice->action);
                PieMenu_Close();
            }
            pieHoldMode = false;
        } else {
            PieMenu_Close();
        }
        return;
    }

    // Right-click to go back (toggle mode only)
    if (!pieHoldMode && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        PieMenu_Back();
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        PieMenu_Back();
    }
}

// ============================================================================
// Draw — labels only, minimal chrome
// ============================================================================

void PieMenu_Draw(void) {
    if (!pieState.isOpen) return;

    Vector2 center = { pieState.centerX, pieState.centerY };

    // Center close circle
    Vector2 mouse = GetMousePosition();
    float cdx = mouse.x - center.x;
    float cdy = mouse.y - center.y;
    float cdist = sqrtf(cdx * cdx + cdy * cdy);
    bool centerHovered = (cdist < PIE_DEAD_ZONE);

    Color centerCol = centerHovered ? (Color){ 200, 100, 100, 220 } : (Color){ 60, 60, 80, 200 };
    DrawCircle((int)center.x, (int)center.y, 8, centerCol);

    // Draw each ring's labels
    for (int r = 0; r < pieState.visibleRings; r++) {
        int menuIdx = pieState.ringMenu[r];
        if (menuIdx < 0) continue;

        PieMenuDef* menu = &menus[menuIdx];
        int n = menu->sliceCount;
        float labelDist = RingRadius(r);

        for (int i = 0; i < n; i++) {
            float a = SliceAngle(i, pieState.ringArcStart[r], pieState.ringArcEnd[r], n);
            float trigAngle = a - PI * 0.5f;

            int lx = (int)(center.x + cosf(trigAngle) * labelDist);
            int ly = (int)(center.y + sinf(trigAngle) * labelDist);

            bool hovered = (r == pieState.hoveredRing && i == pieState.hoveredSlice);
            bool selected = (pieState.ringSelection[r] == i);
            bool isSubmenu = (menu->slices[i].childMenuIdx >= 0);
            // Muted when:
            // - Ring is beyond cursor AND cursor has previously been deeper (coming back)
            // - OR siblings on an inner ring when cursor is deeper
            bool cursorIsDeeper = (pieState.hoveredRing > r);
            bool ringBeyondCursor = (pieState.hoveredRing >= 0 && r > pieState.hoveredRing);
            bool cursorHasBeenDeeper = (pieDeepestRing >= r);
            bool ringHasSelection = (pieState.ringSelection[r] >= 0);
            bool muted = false;
            if (ringBeyondCursor && cursorHasBeenDeeper && !hovered) {
                muted = true;  // Came back from deeper — this ring fades
            } else if (ringHasSelection && cursorIsDeeper && !selected && !hovered) {
                muted = true;  // Siblings of selection fade when cursor is deeper
            }

            const char* label = menu->slices[i].label;
            int fontSize = 18;
            int tw = MeasureText(label, fontSize);
            int padding = 6;

            // Background rect (always drawn for readability)
            Color bgCol;
            if (hovered) {
                bgCol = (Color){ 60, 60, 90, 230 };
            } else if (selected) {
                bgCol = (Color){ 50, 50, 80, 230 };
            } else if (muted) {
                bgCol = (Color){ 15, 15, 25, 140 };
            } else {
                bgCol = (Color){ 20, 20, 35, 220 };
            }
            DrawRectangle(lx - tw / 2 - padding, ly - fontSize / 2 - padding,
                tw + padding * 2, fontSize + padding * 2, bgCol);

            // Text color
            Color textCol;
            if (hovered) {
                textCol = WHITE;
            } else if (selected) {
                textCol = YELLOW;
            } else if (muted) {
                textCol = (Color){ 160, 160, 170, 100 };
            } else {
                textCol = LIGHTGRAY;
            }

            DrawTextShadow(label, lx - tw / 2, ly - fontSize / 2, fontSize, textCol);

            // Submenu indicator
            if (isSubmenu && !selected) {
                Color arrowCol = hovered ? YELLOW : (muted ? (Color){ 128, 128, 128, 100 } : GRAY);
                DrawTextShadow(">", lx + tw / 2 + 3, ly - fontSize / 2, fontSize, arrowCol);
            }
        }
    }
}
