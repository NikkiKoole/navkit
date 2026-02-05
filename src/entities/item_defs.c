// item_defs.c - Item definitions table
// To add a new item: 1) add enum in items.h, 2) add row here

#include "item_defs.h"
#include "items.h"
#include "../../assets/atlas.h"

// Item definitions indexed by ItemType enum
// Order must match ItemType enum in items.h
// Fields: name, sprite, flags, maxStack, producesMaterial
const ItemDef itemDefs[ITEM_TYPE_COUNT] = {
    [ITEM_RED]          = { "Red Crate",    SPRITE_crate_red,    IF_STACKABLE, 10, MAT_NONE },
    [ITEM_GREEN]        = { "Green Crate",  SPRITE_crate_green,  IF_STACKABLE, 10, MAT_NONE },
    [ITEM_BLUE]         = { "Blue Crate",   SPRITE_crate_blue,   IF_STACKABLE, 10, MAT_NONE },
    [ITEM_ROCK]         = { "Raw Stone",    SPRITE_rock_block, IF_STACKABLE, 20, MAT_STONE },
    [ITEM_STONE_BLOCKS] = { "Stone Blocks", SPRITE_stone_block,  IF_STACKABLE | IF_BUILDING_MAT, 20, MAT_STONE },
    [ITEM_WOOD]         = { "Wood",         SPRITE_wood_block,   IF_STACKABLE | IF_BUILDING_MAT | IF_FUEL, 20, MAT_WOOD },
    [ITEM_SAPLING]      = { "Sapling",      SPRITE_tree_sapling, IF_STACKABLE, 20, MAT_NONE },
    [ITEM_DIRT]         = { "Dirt",         SPRITE_dirt_block,   IF_STACKABLE | IF_BUILDING_MAT, 20, MAT_DIRT },
    [ITEM_CLAY]         = { "Clay",         SPRITE_clay_block,   IF_STACKABLE, 20, MAT_NONE },
    [ITEM_GRAVEL]       = { "Gravel",       SPRITE_gravel_block, IF_STACKABLE, 20, MAT_NONE },
    [ITEM_SAND]         = { "Sand",         SPRITE_sand_block,   IF_STACKABLE, 20, MAT_NONE },
    [ITEM_PEAT]         = { "Peat",         SPRITE_peat_block,   IF_STACKABLE | IF_FUEL, 20, MAT_NONE },
};
