// Auto-generated texture atlas header
// Do not edit manually - regenerate with: make atlas

#ifndef ATLAS16X16_H
#define ATLAS16X16_H

#include "vendor/raylib.h"

#define ATLAS16X16_PATH "assets/atlas16x16.png"

typedef struct {
    const char *name;
    Rectangle rect;  // x, y, width, height in atlas
} SPRITE16X16Sprite;

enum {
    SPRITE16X16_carpet,
    SPRITE16X16_crate_blue,
    SPRITE16X16_air,
    SPRITE16X16_ladder,
    SPRITE16X16_apple,
    SPRITE16X16_grass,
    SPRITE16X16_ladder_up,
    SPRITE16X16_head,
    SPRITE16X16_wall,
    SPRITE16X16_stockpile,
    SPRITE16X16_floor,
    SPRITE16X16_ladder_down,
    SPRITE16X16_crate_green,
    SPRITE16X16_crate_red,
    SPRITE16X16_COUNT
};

static const SPRITE16X16Sprite SPRITE16X16_SPRITES[SPRITE16X16_COUNT] = {
    { "carpet", { 0, 0, 16, 16 } },
    { "crate_blue", { 17, 0, 16, 16 } },
    { "air", { 34, 0, 16, 16 } },
    { "ladder", { 51, 0, 16, 16 } },
    { "apple", { 68, 0, 16, 16 } },
    { "grass", { 85, 0, 16, 16 } },
    { "ladder_up", { 102, 0, 16, 16 } },
    { "head", { 119, 0, 16, 16 } },
    { "wall", { 136, 0, 16, 16 } },
    { "stockpile", { 153, 0, 16, 16 } },
    { "floor", { 170, 0, 16, 16 } },
    { "ladder_down", { 187, 0, 16, 16 } },
    { "crate_green", { 204, 0, 16, 16 } },
    { "crate_red", { 221, 0, 16, 16 } },
};

// Helper: get sprite rectangle by enum
static inline Rectangle SPRITE16X16GetRect(int spriteId) {
    return SPRITE16X16_SPRITES[spriteId].rect;
}

#endif // ATLAS16X16_H
