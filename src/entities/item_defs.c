// item_defs.c - Item definitions table
// To add a new item: 1) add enum in items.h, 2) add row here

#include "item_defs.h"
#include "items.h"
#include "../../assets/atlas.h"

// Item definitions indexed by ItemType enum
// Order must match ItemType enum in items.h
const ItemDef itemDefs[ITEM_TYPE_COUNT] = {
    [ITEM_RED]          = { "Red Crate",    SPRITE_crate_red,    IF_STACKABLE, 10 },
    [ITEM_GREEN]        = { "Green Crate",  SPRITE_crate_green,  IF_STACKABLE, 10 },
    [ITEM_BLUE]         = { "Blue Crate",   SPRITE_crate_blue,   IF_STACKABLE, 10 },
    [ITEM_ORANGE]       = { "Raw Stone",    SPRITE_crate_orange, IF_STACKABLE, 20 },
    [ITEM_STONE_BLOCKS] = { "Stone Blocks", SPRITE_stone_block,  IF_STACKABLE | IF_BUILDING_MAT, 20 },
};
