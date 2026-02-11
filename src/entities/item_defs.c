// item_defs.c - Item definitions table
// To add a new item: 1) add enum in items.h, 2) add row here

#include "item_defs.h"
#include "items.h"
#include "../../assets/atlas.h"

// Item definitions indexed by ItemType enum
// Order must match ItemType enum in items.h
// Fields: name, sprite, flags, maxStack
const ItemDef itemDefs[ITEM_TYPE_COUNT] = {
    [ITEM_RED]          = { "Red Crate",    SPRITE_crate_red,    IF_STACKABLE, 10 },
    [ITEM_GREEN]        = { "Green Crate",  SPRITE_crate_green,  IF_STACKABLE, 10 },
    [ITEM_BLUE]         = { "Blue Crate",   SPRITE_crate_blue,   IF_STACKABLE, 10 },
    [ITEM_ROCK]         = { "Raw Stone",    SPRITE_granite,       IF_STACKABLE, 20 },
    [ITEM_BLOCKS]       = { "Blocks",       SPRITE_block_stone,  IF_STACKABLE | IF_BUILDING_MAT, 20 },
    [ITEM_LOG]          = { "Log",          SPRITE_block_wood,   IF_STACKABLE | IF_BUILDING_MAT | IF_FUEL, 20 },
    [ITEM_SAPLING]      = { "Sapling",      SPRITE_tree_sapling_oak, IF_STACKABLE, 20 },
    [ITEM_LEAVES]       = { "Leaves",       SPRITE_tree_leaves_oak,  IF_STACKABLE | IF_FUEL, 20 },
    [ITEM_DIRT]         = { "Dirt",         SPRITE_dirt,         IF_STACKABLE, 20 },
    [ITEM_CLAY]         = { "Clay",         SPRITE_clay,         IF_STACKABLE, 20 },
    [ITEM_GRAVEL]       = { "Gravel",       SPRITE_gravel,       IF_STACKABLE, 20 },
    [ITEM_SAND]         = { "Sand",         SPRITE_sand,         IF_STACKABLE, 20 },
    [ITEM_PEAT]         = { "Peat",         SPRITE_peat,         IF_STACKABLE | IF_FUEL, 20 },
    [ITEM_PLANKS]       = { "Planks",       SPRITE_tree_planks_oak, IF_STACKABLE | IF_BUILDING_MAT | IF_FUEL, 20 },
    [ITEM_STICKS]       = { "Sticks",       SPRITE_block_wood,   IF_STACKABLE | IF_FUEL, 20 },
    [ITEM_POLES]        = { "Poles",        SPRITE_tree_branch_oak, IF_STACKABLE | IF_BUILDING_MAT | IF_FUEL, 20 },
    [ITEM_GRASS]        = { "Grass",        SPRITE_grass_tall,   IF_STACKABLE, 20 },
    [ITEM_DRIED_GRASS]  = { "Dried Grass",  SPRITE_grass_trampled, IF_STACKABLE, 20 },
    [ITEM_BRICKS]       = { "Bricks",       SPRITE_wall,         IF_STACKABLE | IF_BUILDING_MAT, 20 },
    [ITEM_CHARCOAL]     = { "Charcoal",     SPRITE_block_wood,   IF_STACKABLE | IF_FUEL, 20 },
    [ITEM_ASH]          = { "Ash",          SPRITE_gravel,       IF_STACKABLE, 20 },
};
