// Auto-generated texture atlas header
// Do not edit manually - regenerate with: make atlas

#ifndef ATLAS8X8_H
#define ATLAS8X8_H

#include "vendor/raylib.h"

#define ATLAS8X8_PATH "assets/atlas8x8.png"

typedef struct {
    const char *name;
    Rectangle rect;  // x, y, width, height in atlas
} SPRITE8X8Sprite;

enum {
    SPRITE8X8_crate_orange,
    SPRITE8X8_carpet,
    SPRITE8X8_crate_blue,
    SPRITE8X8_air,
    SPRITE8X8_rock,
    SPRITE8X8_ladder,
    SPRITE8X8_apple,
    SPRITE8X8_grass,
    SPRITE8X8_ladder_up,
    SPRITE8X8_head,
    SPRITE8X8_wall,
    SPRITE8X8_stockpile,
    SPRITE8X8_floor,
    SPRITE8X8_ladder_down,
    SPRITE8X8_rock_floor,
    SPRITE8X8_crate_green,
    SPRITE8X8_crate_red,
    SPRITE8X8_COUNT
};

static const SPRITE8X8Sprite SPRITE8X8_SPRITES[SPRITE8X8_COUNT] = {
    { "crate_orange", { 0, 0, 8, 8 } },
    { "carpet", { 9, 0, 8, 8 } },
    { "crate_blue", { 18, 0, 8, 8 } },
    { "air", { 27, 0, 8, 8 } },
    { "rock", { 36, 0, 8, 8 } },
    { "ladder", { 45, 0, 8, 8 } },
    { "apple", { 54, 0, 8, 8 } },
    { "grass", { 63, 0, 8, 8 } },
    { "ladder_up", { 72, 0, 8, 8 } },
    { "head", { 81, 0, 8, 8 } },
    { "wall", { 90, 0, 8, 8 } },
    { "stockpile", { 99, 0, 8, 8 } },
    { "floor", { 108, 0, 8, 8 } },
    { "ladder_down", { 117, 0, 8, 8 } },
    { "rock_floor", { 126, 0, 8, 8 } },
    { "crate_green", { 135, 0, 8, 8 } },
    { "crate_red", { 144, 0, 8, 8 } },
};

// Helper: get sprite rectangle by enum
static inline Rectangle SPRITE8X8GetRect(int spriteId) {
    return SPRITE8X8_SPRITES[spriteId].rect;
}

#endif // ATLAS8X8_H
