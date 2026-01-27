// Atlas selector - choose tile size at compile time
// Auto-generated - do not edit manually, regenerate with: make atlas
// Usage: compile with -DTILE_SIZE=8 or -DTILE_SIZE=16 (default: 16)

#ifndef ATLAS_H
#define ATLAS_H

#ifndef TILE_SIZE
#define TILE_SIZE 8
#endif

#if TILE_SIZE == 8
    #include "atlas8x8.h"
    #define ATLAS_PATH ATLAS8X8_PATH
    #define ATLAS_DATA ATLAS8X8_PATH_DATA
    #define ATLAS_DATA_SIZE ATLAS8X8_PATH_DATA_SIZE
    #define AtlasLoadEmbedded SPRITE8X8LoadEmbedded
    #define SPRITE_crate_orange SPRITE8X8_crate_orange
    #define SPRITE_carpet SPRITE8X8_carpet
    #define SPRITE_crate_blue SPRITE8X8_crate_blue
    #define SPRITE_air SPRITE8X8_air
    #define SPRITE_dirt SPRITE8X8_dirt
    #define SPRITE_rock SPRITE8X8_rock
    #define SPRITE_ladder SPRITE8X8_ladder
    #define SPRITE_apple SPRITE8X8_apple
    #define SPRITE_grass SPRITE8X8_grass
    #define SPRITE_ladder_up SPRITE8X8_ladder_up
    #define SPRITE_head SPRITE8X8_head
    #define SPRITE_wall SPRITE8X8_wall
    #define SPRITE_stockpile SPRITE8X8_stockpile
    #define SPRITE_floor SPRITE8X8_floor
    #define SPRITE_ladder_down SPRITE8X8_ladder_down
    #define SPRITE_rock_floor SPRITE8X8_rock_floor
    #define SPRITE_crate_green SPRITE8X8_crate_green
    #define SPRITE_crate_red SPRITE8X8_crate_red
    #define SpriteGetRect SPRITE8X8GetRect
#elif TILE_SIZE == 16
    #include "atlas16x16.h"
    #define ATLAS_PATH ATLAS16X16_PATH
    #define ATLAS_DATA ATLAS16X16_PATH_DATA
    #define ATLAS_DATA_SIZE ATLAS16X16_PATH_DATA_SIZE
    #define AtlasLoadEmbedded SPRITE16X16LoadEmbedded
    #define SPRITE_crate_orange SPRITE16X16_crate_orange
    #define SPRITE_carpet SPRITE16X16_carpet
    #define SPRITE_crate_blue SPRITE16X16_crate_blue
    #define SPRITE_air SPRITE16X16_air
    #define SPRITE_dirt SPRITE16X16_dirt
    #define SPRITE_rock SPRITE16X16_rock
    #define SPRITE_ladder SPRITE16X16_ladder
    #define SPRITE_apple SPRITE16X16_apple
    #define SPRITE_grass SPRITE16X16_grass
    #define SPRITE_ladder_up SPRITE16X16_ladder_up
    #define SPRITE_head SPRITE16X16_head
    #define SPRITE_wall SPRITE16X16_wall
    #define SPRITE_stockpile SPRITE16X16_stockpile
    #define SPRITE_floor SPRITE16X16_floor
    #define SPRITE_ladder_down SPRITE16X16_ladder_down
    #define SPRITE_rock_floor SPRITE16X16_rock_floor
    #define SPRITE_crate_green SPRITE16X16_crate_green
    #define SPRITE_crate_red SPRITE16X16_crate_red
    #define SpriteGetRect SPRITE16X16GetRect
#else
    #error "TILE_SIZE must be 8 or 16"
#endif

#endif // ATLAS_H
