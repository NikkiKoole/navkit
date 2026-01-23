// Auto-generated texture atlas header
// Do not edit manually - regenerate with: make atlas

#ifndef ATLAS_H
#define ATLAS_H

#include "vendor/raylib.h"

#define ATLAS_PATH "assets/atlas.png"

typedef struct {
    const char *name;
    Rectangle rect;  // x, y, width, height in atlas
} AtlasSprite;

enum {
    SPRITE_carpet,
    SPRITE_crate_blue,
    SPRITE_air,
    SPRITE_ladder,
    SPRITE_apple,
    SPRITE_grass,
    SPRITE_ladder_up,
    SPRITE_head,
    SPRITE_wall,
    SPRITE_stockpile,
    SPRITE_floor,
    SPRITE_ladder_down,
    SPRITE_crate_green,
    SPRITE_crate_red,
    SPRITE_COUNT
};

static const AtlasSprite ATLAS_SPRITES[SPRITE_COUNT] = {
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
static inline Rectangle AtlasGetRect(int spriteId) {
    return ATLAS_SPRITES[spriteId].rect;
}

#endif // ATLAS_H
