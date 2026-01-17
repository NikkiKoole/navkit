#ifndef UI_H
#define UI_H

#include "../vendor/raylib.h"
#include <stdbool.h>

// Initialize UI system with font
void ui_init(Font* font);

// Call each frame to process drag input
void ui_update(void);

// Draw text with shadow (for readability on any background)
void DrawTextShadow(const char* text, int x, int y, int size, Color col);

// Draggable float value - click and drag to change
void DraggableFloat(float x, float y, const char* label, float* value, 
                    float sensitivity, float minVal, float maxVal);

// Toggle checkbox - click to toggle
void ToggleBool(float x, float y, const char* label, bool* value);

#endif
