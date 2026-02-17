// core/pie_menu.c - Radial pie menu system (concentric rings, label-only)
// Dynamically generated from ACTION_REGISTRY
#include "pie_menu.h"
#include "action_registry.h"
#include "../game_state.h"
#include "shared/ui.h"
#include <math.h>
#include <string.h>

// ============================================================================
// Dynamic menu generation from ACTION_REGISTRY
// ============================================================================

// Menu indices (dynamically built)
enum {
    MENU_ROOT = 0,
    MENU_WORK,
    MENU_DIG,
    MENU_BUILD,
    MENU_HARVEST,
    MENU_DRAW,
    MENU_SANDBOX,
    MENU_DRAW_WORKSHOP,
    MENU_DRAW_SOIL,
    MENU_MAX_COUNT = 32  // Upper bound for dynamic menus
};

static PieMenuDef menus[MENU_MAX_COUNT];
static int menuCount = 0;

// Helper: Check if action is a workshop subcategory
static bool IsWorkshopSubAction(InputAction action) {
    return (action == ACTION_DRAW_WORKSHOP_STONECUTTER ||
            action == ACTION_DRAW_WORKSHOP_SAWMILL ||
            action == ACTION_DRAW_WORKSHOP_KILN ||
            action == ACTION_DRAW_WORKSHOP_CHARCOAL_PIT ||
            action == ACTION_DRAW_WORKSHOP_HEARTH ||
            action == ACTION_DRAW_WORKSHOP_DRYING_RACK ||
            action == ACTION_DRAW_WORKSHOP_ROPE_MAKER ||
            action == ACTION_DRAW_WORKSHOP_CARPENTER);
}

// Helper: Check if action is a soil subcategory
static bool IsSoilSubAction(InputAction action) {
    return (action == ACTION_DRAW_SOIL_DIRT ||
            action == ACTION_DRAW_SOIL_CLAY ||
            action == ACTION_DRAW_SOIL_GRAVEL ||
            action == ACTION_DRAW_SOIL_SAND ||
            action == ACTION_DRAW_SOIL_PEAT ||
            action == ACTION_DRAW_SOIL_ROCK);
}

// Helper: Add slice to menu
static void AddSlice(int menuIdx, const char* label, InputAction action, int childMenuIdx) {
    if (menuIdx >= MENU_MAX_COUNT) return;
    PieMenuDef* menu = &menus[menuIdx];
    if (menu->sliceCount >= PIE_MAX_ITEMS) return;
    
    menu->slices[menu->sliceCount].label = label;
    menu->slices[menu->sliceCount].action = action;
    menu->slices[menu->sliceCount].childMenuIdx = childMenuIdx;
    menu->slices[menu->sliceCount].color = GRAY;  // Not rendered, doesn't matter
    menu->sliceCount++;
}

// Build all menus dynamically from ACTION_REGISTRY
static void BuildDynamicMenus(void) {
    menuCount = 0;
    memset(menus, 0, sizeof(menus));
    
    // MENU_ROOT
    menus[MENU_ROOT].title = "Menu";
    AddSlice(MENU_ROOT, "Work", ACTION_NONE, MENU_WORK);
    AddSlice(MENU_ROOT, "Draw", ACTION_NONE, MENU_DRAW);
    AddSlice(MENU_ROOT, "Sandbox", ACTION_NONE, MENU_SANDBOX);
    
    // MENU_WORK (submodes + Gather)
    menus[MENU_WORK].title = "Work";
    AddSlice(MENU_WORK, "Dig", ACTION_NONE, MENU_DIG);
    AddSlice(MENU_WORK, "Build", ACTION_NONE, MENU_BUILD);
    AddSlice(MENU_WORK, "Harvest", ACTION_NONE, MENU_HARVEST);
    
    // Scan for ACTION_WORK_GATHER (action-level, not in a submode)
    for (int i = 0; i < ACTION_REGISTRY_COUNT; i++) {
        const ActionDef* def = &ACTION_REGISTRY[i];
        if (def->requiredMode == MODE_WORK && 
            def->requiredSubMode == SUBMODE_NONE &&
            def->action != ACTION_NONE &&
            def->canDrag) {  // Exclude category actions
            AddSlice(MENU_WORK, def->name, def->action, -1);
        }
    }
    
    // MENU_DIG (all SUBMODE_DIG actions)
    menus[MENU_DIG].title = "Dig";
    for (int i = 0; i < ACTION_REGISTRY_COUNT; i++) {
        const ActionDef* def = &ACTION_REGISTRY[i];
        if (def->requiredMode == MODE_WORK && def->requiredSubMode == SUBMODE_DIG) {
            AddSlice(MENU_DIG, def->name, def->action, -1);
        }
    }
    
    // MENU_BUILD (all SUBMODE_BUILD actions)
    menus[MENU_BUILD].title = "Build";
    for (int i = 0; i < ACTION_REGISTRY_COUNT; i++) {
        const ActionDef* def = &ACTION_REGISTRY[i];
        if (def->requiredMode == MODE_WORK && def->requiredSubMode == SUBMODE_BUILD) {
            AddSlice(MENU_BUILD, def->name, def->action, -1);
        }
    }
    
    // MENU_HARVEST (all SUBMODE_HARVEST actions)
    menus[MENU_HARVEST].title = "Harvest";
    for (int i = 0; i < ACTION_REGISTRY_COUNT; i++) {
        const ActionDef* def = &ACTION_REGISTRY[i];
        if (def->requiredMode == MODE_WORK && def->requiredSubMode == SUBMODE_HARVEST) {
            AddSlice(MENU_HARVEST, def->name, def->action, -1);
        }
    }
    
    // MENU_DRAW (all MODE_DRAW actions, including categories)
    menus[MENU_DRAW].title = "Draw";
    for (int i = 0; i < ACTION_REGISTRY_COUNT; i++) {
        const ActionDef* def = &ACTION_REGISTRY[i];
        if (def->requiredMode == MODE_DRAW && def->requiredSubMode == SUBMODE_NONE) {
            // Skip subcategory items (WORKSHOP_STONECUTTER, SOIL_DIRT, etc.)
            if (IsWorkshopSubAction(def->action)) continue;
            if (IsSoilSubAction(def->action)) continue;
            
            // Determine if this is a category (opens submenu)
            int childMenu = -1;
            if (def->action == ACTION_DRAW_WORKSHOP) childMenu = MENU_DRAW_WORKSHOP;
            else if (def->action == ACTION_DRAW_SOIL) childMenu = MENU_DRAW_SOIL;
            
            AddSlice(MENU_DRAW, def->name, def->action, childMenu);
        }
    }
    
    // MENU_DRAW_WORKSHOP (all ACTION_DRAW_WORKSHOP_* actions)
    menus[MENU_DRAW_WORKSHOP].title = "Workshop";
    for (int i = 0; i < ACTION_REGISTRY_COUNT; i++) {
        const ActionDef* def = &ACTION_REGISTRY[i];
        if (IsWorkshopSubAction(def->action)) {
            AddSlice(MENU_DRAW_WORKSHOP, def->name, def->action, -1);
        }
    }
    
    // MENU_DRAW_SOIL (all ACTION_DRAW_SOIL_* actions)
    menus[MENU_DRAW_SOIL].title = "Soil";
    for (int i = 0; i < ACTION_REGISTRY_COUNT; i++) {
        const ActionDef* def = &ACTION_REGISTRY[i];
        if (IsSoilSubAction(def->action)) {
            AddSlice(MENU_DRAW_SOIL, def->name, def->action, -1);
        }
    }
    
    // MENU_SANDBOX (all MODE_SANDBOX actions)
    menus[MENU_SANDBOX].title = "Sandbox";
    for (int i = 0; i < ACTION_REGISTRY_COUNT; i++) {
        const ActionDef* def = &ACTION_REGISTRY[i];
        if (def->requiredMode == MODE_SANDBOX) {
            AddSlice(MENU_SANDBOX, def->name, def->action, -1);
        }
    }
    
    menuCount = MENU_SANDBOX + 1;
}

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
// Action application - uses ACTION_REGISTRY for mode/submode lookup
// ============================================================================

static void PieMenu_ApplyAction(InputAction action) {
    if (action == ACTION_NONE) return;
    
    const ActionDef* def = GetActionDef(action);
    if (!def) return;
    
    // Set mode and action from registry
    inputMode = def->requiredMode;
    inputAction = action;
    
    // Set submode for WORK actions
    if (inputMode == MODE_WORK) {
        workSubMode = def->requiredSubMode;
    } else {
        workSubMode = SUBMODE_NONE;
    }
}

void PieMenu_Open(float x, float y) {
    // Build menus on first open (lazy init)
    static bool initialized = false;
    if (!initialized) {
        BuildDynamicMenus();
        initialized = true;
    }
    
    pieState.isOpen = true;
    pieState.centerX = x;
    pieState.centerY = y;
    pieState.hoveredRing = -1;
    pieState.hoveredSlice = -1;
    pieState.ringMenu[0] = MENU_ROOT;
    pieState.ringSelection[0] = -1;
    for (int i = 1; i < PIE_MAX_DEPTH; i++) {
        pieState.ringMenu[i] = -1;
        pieState.ringSelection[i] = -1;
    }
    pieState.ringArcStart[0] = 0.0f;
    pieState.ringArcEnd[0] = PI * 2.0f;
    pieState.visibleRings = 1;
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
        pieState.ringMenu[pieState.visibleRings] = -1;
        pieState.ringSelection[pieState.visibleRings] = -1;
    } else {
        PieMenu_Close();
    }
}

bool PieMenu_IsOpen(void) {
    return pieState.isOpen;
}

bool PieMenu_JustClosed(void) {
    if (pieClosedTime < 0) return false;
    return (GetTime() - pieClosedTime) < 0.016;  // One frame at 60 FPS
}

// Expand ring: add child menu at next ring
static void ExpandRing(int parentRing, int parentSlice) {
    if (parentRing < 0 || parentRing >= pieState.visibleRings) return;
    int childRing = parentRing + 1;
    if (childRing >= PIE_MAX_DEPTH) return;
    
    int parentMenu = pieState.ringMenu[parentRing];
    PieSlice* s = &menus[parentMenu].slices[parentSlice];
    if (s->childMenuIdx < 0) return;  // No child menu
    
    pieState.ringMenu[childRing] = s->childMenuIdx;
    pieState.ringSelection[childRing] = -1;
    
    // Calculate arc for this child ring
    int parentSliceCount = menus[parentMenu].sliceCount;
    float parentArcSize = pieState.ringArcEnd[parentRing] - pieState.ringArcStart[parentRing];
    if (parentArcSize <= 0) parentArcSize += PI * 2.0f;
    float sliceSize = parentArcSize / parentSliceCount;
    pieState.ringArcStart[childRing] = pieState.ringArcStart[parentRing] + sliceSize * parentSlice;
    pieState.ringArcEnd[childRing] = pieState.ringArcStart[childRing] + sliceSize;
    
    if (childRing + 1 > pieState.visibleRings) {
        pieState.visibleRings = childRing + 1;
    }
    
    // Collapse any deeper rings
    for (int r = childRing + 1; r < PIE_MAX_DEPTH; r++) {
        pieState.ringMenu[r] = -1;
        pieState.ringSelection[r] = -1;
    }
}

void PieMenu_Update(void) {
    if (!pieState.isOpen) return;
    
    Vector2 mouse = GetMousePosition();
    float dx = mouse.x - pieState.centerX;
    float dy = mouse.y - pieState.centerY;
    float dist = sqrtf(dx * dx + dy * dy);
    
    // Determine ring
    int ring = -1;
    if (dist < RingInner(0)) {
        ring = -1;  // Dead zone
    } else {
        for (int r = 0; r < pieState.visibleRings; r++) {
            if (dist >= RingInner(r) && dist < RingOuter(r)) {
                ring = r;
                break;
            }
        }
    }
    
    // Update deepest ring reached (for hold mode)
    if (ring > pieDeepestRing) {
        pieDeepestRing = ring;
    }
    
    pieState.hoveredRing = ring;
    pieState.hoveredSlice = -1;
    
    if (ring >= 0 && ring < pieState.visibleRings) {
        int menuIdx = pieState.ringMenu[ring];
        if (menuIdx >= 0 && menuIdx < menuCount) {
            float angle = NormalizeAngle(dx, dy);
            if (AngleInArc(angle, pieState.ringArcStart[ring], pieState.ringArcEnd[ring])) {
                int sliceCount = menus[menuIdx].sliceCount;
                int slice = AngleToSliceInArc(angle, pieState.ringArcStart[ring], pieState.ringArcEnd[ring], sliceCount);
                pieState.hoveredSlice = slice;
                
                // Auto-expand when hovering a slice with children
                PieSlice* s = &menus[menuIdx].slices[slice];
                if (s->childMenuIdx >= 0) {
                    ExpandRing(ring, slice);
                }
            }
        }
    }
    
    // Click to select (toggle mode)
    if (!pieHoldMode && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (pieState.hoveredRing >= 0 && pieState.hoveredSlice >= 0) {
            int ring = pieState.hoveredRing;
            int slice = pieState.hoveredSlice;
            int menuIdx = pieState.ringMenu[ring];
            PieSlice* s = &menus[menuIdx].slices[slice];
            
            if (s->action != ACTION_NONE) {
                PieMenu_ApplyAction(s->action);
                PieMenu_Close();
            }
        } else {
            PieMenu_Close();
        }
    }
    
    // Release to select (hold mode)
    if (pieHoldMode && IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
        // Only select if we've reached at least ring 0 (not just dead zone)
        if (pieDeepestRing >= 0 && pieState.hoveredRing >= 0 && pieState.hoveredSlice >= 0) {
            int menuIdx = pieState.ringMenu[pieState.hoveredRing];
            PieSlice* slice = &menus[menuIdx].slices[pieState.hoveredSlice];
            if (slice->action != ACTION_NONE) {
                PieMenu_ApplyAction(slice->action);
            }
        }
        PieMenu_Close();
    }
    
    // Back with Escape or Backspace
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
        PieMenu_Back();
    }
    if (IsKeyPressed(KEY_DELETE)) {
        PieMenu_Back();
    }
}

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
            int tw = MeasureTextUI(label, fontSize);
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
