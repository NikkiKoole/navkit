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
    #define SPRITE_finish_engraved SPRITE8X8_finish_engraved
    #define SPRITE_tree_sapling_pine SPRITE8X8_tree_sapling_pine
    #define SPRITE_tree_trunk_willow SPRITE8X8_tree_trunk_willow
    #define SPRITE_wood_floor SPRITE8X8_wood_floor
    #define SPRITE_bedrock SPRITE8X8_bedrock
    #define SPRITE_crate_blue SPRITE8X8_crate_blue
    #define SPRITE_finish_polished SPRITE8X8_finish_polished
    #define SPRITE_wood_block SPRITE8X8_wood_block
    #define SPRITE_clay SPRITE8X8_clay
    #define SPRITE_tree_sapling_willow SPRITE8X8_tree_sapling_willow
    #define SPRITE_tile_border SPRITE8X8_tile_border
    #define SPRITE_air SPRITE8X8_air
    #define SPRITE_grass_trampled SPRITE8X8_grass_trampled
    #define SPRITE_dirt SPRITE8X8_dirt
    #define SPRITE_stone_block SPRITE8X8_stone_block
    #define SPRITE_tree_sapling_birch SPRITE8X8_tree_sapling_birch
    #define SPRITE_tree_leaves_birch SPRITE8X8_tree_leaves_birch
    #define SPRITE_ramp_s SPRITE8X8_ramp_s
    #define SPRITE_ramp_e SPRITE8X8_ramp_e
    #define SPRITE_finish_smooth SPRITE8X8_finish_smooth
    #define SPRITE_tree_leaves_oak SPRITE8X8_tree_leaves_oak
    #define SPRITE_gravel SPRITE8X8_gravel
    #define SPRITE_rock SPRITE8X8_rock
    #define SPRITE_grass_tall SPRITE8X8_grass_tall
    #define SPRITE_ramp_w SPRITE8X8_ramp_w
    #define SPRITE_tree_trunk_pine SPRITE8X8_tree_trunk_pine
    #define SPRITE_finish_rough SPRITE8X8_finish_rough
    #define SPRITE_ladder SPRITE8X8_ladder
    #define SPRITE_ramp_n SPRITE8X8_ramp_n
    #define SPRITE_peat SPRITE8X8_peat
    #define SPRITE_grass SPRITE8X8_grass
    #define SPRITE_tree_trunk_oak SPRITE8X8_tree_trunk_oak
    #define SPRITE_ladder_up SPRITE8X8_ladder_up
    #define SPRITE_head SPRITE8X8_head
    #define SPRITE_wall SPRITE8X8_wall
    #define SPRITE_generic SPRITE8X8_generic
    #define SPRITE_stockpile SPRITE8X8_stockpile
    #define SPRITE_floor SPRITE8X8_floor
    #define SPRITE_tree_leaves_pine SPRITE8X8_tree_leaves_pine
    #define SPRITE_ladder_down SPRITE8X8_ladder_down
    #define SPRITE_tree_trunk_birch SPRITE8X8_tree_trunk_birch
    #define SPRITE_sand SPRITE8X8_sand
    #define SPRITE_tree_leaves_willow SPRITE8X8_tree_leaves_willow
    #define SPRITE_tree_sapling_oak SPRITE8X8_tree_sapling_oak
    #define SPRITE_crate_green SPRITE8X8_crate_green
    #define SPRITE_wood_wall SPRITE8X8_wood_wall
    #define SPRITE_crate_red SPRITE8X8_crate_red
    #define SpriteGetRect SPRITE8X8GetRect
#elif TILE_SIZE == 16
    #include "atlas16x16.h"
    #define ATLAS_PATH ATLAS16X16_PATH
    #define ATLAS_DATA ATLAS16X16_PATH_DATA
    #define ATLAS_DATA_SIZE ATLAS16X16_PATH_DATA_SIZE
    #define AtlasLoadEmbedded SPRITE16X16LoadEmbedded
    #define SPRITE_finish_engraved SPRITE16X16_finish_engraved
    #define SPRITE_tree_sapling_pine SPRITE16X16_tree_sapling_pine
    #define SPRITE_tree_trunk_willow SPRITE16X16_tree_trunk_willow
    #define SPRITE_wood_floor SPRITE16X16_wood_floor
    #define SPRITE_bedrock SPRITE16X16_bedrock
    #define SPRITE_crate_blue SPRITE16X16_crate_blue
    #define SPRITE_finish_polished SPRITE16X16_finish_polished
    #define SPRITE_wood_block SPRITE16X16_wood_block
    #define SPRITE_clay SPRITE16X16_clay
    #define SPRITE_tree_sapling_willow SPRITE16X16_tree_sapling_willow
    #define SPRITE_tile_border SPRITE16X16_tile_border
    #define SPRITE_air SPRITE16X16_air
    #define SPRITE_grass_trampled SPRITE16X16_grass_trampled
    #define SPRITE_dirt SPRITE16X16_dirt
    #define SPRITE_stone_block SPRITE16X16_stone_block
    #define SPRITE_tree_sapling_birch SPRITE16X16_tree_sapling_birch
    #define SPRITE_tree_leaves_birch SPRITE16X16_tree_leaves_birch
    #define SPRITE_ramp_s SPRITE16X16_ramp_s
    #define SPRITE_ramp_e SPRITE16X16_ramp_e
    #define SPRITE_finish_smooth SPRITE16X16_finish_smooth
    #define SPRITE_tree_leaves_oak SPRITE16X16_tree_leaves_oak
    #define SPRITE_gravel SPRITE16X16_gravel
    #define SPRITE_rock SPRITE16X16_rock
    #define SPRITE_grass_tall SPRITE16X16_grass_tall
    #define SPRITE_ramp_w SPRITE16X16_ramp_w
    #define SPRITE_tree_trunk_pine SPRITE16X16_tree_trunk_pine
    #define SPRITE_finish_rough SPRITE16X16_finish_rough
    #define SPRITE_ladder SPRITE16X16_ladder
    #define SPRITE_ramp_n SPRITE16X16_ramp_n
    #define SPRITE_peat SPRITE16X16_peat
    #define SPRITE_grass SPRITE16X16_grass
    #define SPRITE_tree_trunk_oak SPRITE16X16_tree_trunk_oak
    #define SPRITE_ladder_up SPRITE16X16_ladder_up
    #define SPRITE_head SPRITE16X16_head
    #define SPRITE_wall SPRITE16X16_wall
    #define SPRITE_generic SPRITE16X16_generic
    #define SPRITE_stockpile SPRITE16X16_stockpile
    #define SPRITE_floor SPRITE16X16_floor
    #define SPRITE_tree_leaves_pine SPRITE16X16_tree_leaves_pine
    #define SPRITE_ladder_down SPRITE16X16_ladder_down
    #define SPRITE_tree_trunk_birch SPRITE16X16_tree_trunk_birch
    #define SPRITE_sand SPRITE16X16_sand
    #define SPRITE_tree_leaves_willow SPRITE16X16_tree_leaves_willow
    #define SPRITE_tree_sapling_oak SPRITE16X16_tree_sapling_oak
    #define SPRITE_crate_green SPRITE16X16_crate_green
    #define SPRITE_wood_wall SPRITE16X16_wood_wall
    #define SPRITE_crate_red SPRITE16X16_crate_red
    #define SpriteGetRect SPRITE16X16GetRect
#else
    #error "TILE_SIZE must be 8 or 16"
#endif

#endif // ATLAS_H
