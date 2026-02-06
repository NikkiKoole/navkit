// core/pie_menu.h - Radial pie menu system
#ifndef PIE_MENU_H
#define PIE_MENU_H

#include "input_mode.h"
#include <stdbool.h>

#define PIE_MAX_ITEMS 8
#define PIE_MAX_DEPTH 4
#define PIE_DEAD_ZONE 30.0f
#define PIE_RING_WIDTH 70.0f

typedef struct {
    const char* label;
    InputAction action;   // ACTION_NONE if submenu parent
    int childMenuIdx;     // -1 if leaf
    Color color;
} PieSlice;

typedef struct {
    const char* title;
    PieSlice slices[PIE_MAX_ITEMS];
    int sliceCount;
} PieMenuDef;

typedef struct {
    bool isOpen;
    float centerX, centerY;
    int hoveredRing;                    // Which ring the cursor is in (-1 = dead zone)
    int hoveredSlice;                   // Which slice in that ring (-1 = none)
    int ringMenu[PIE_MAX_DEPTH];       // Menu index shown at each ring (ring 0 = root)
    int ringSelection[PIE_MAX_DEPTH];  // Locked-in slice at each ring (-1 = none)
    float ringArcStart[PIE_MAX_DEPTH]; // Angular start of this ring's arc (radians, 0=north)
    float ringArcEnd[PIE_MAX_DEPTH];   // Angular end of this ring's arc
    int visibleRings;                   // How many rings are currently showing
} PieMenuState;

void PieMenu_Open(float x, float y);
void PieMenu_OpenHold(float x, float y);  // Open in hold-drag mode (release to select)
void PieMenu_Close(void);
void PieMenu_Back(void);
void PieMenu_Update(void);
void PieMenu_Draw(void);
bool PieMenu_IsOpen(void);
bool PieMenu_JustClosed(void);  // True for one frame after closing

#endif
