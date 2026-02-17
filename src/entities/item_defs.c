// item_defs.c - Item definitions table
// To add a new item: 1) add enum in items.h, 2) add row here

#include "item_defs.h"
#include "items.h"
#include "../world/material.h"
#include "../../assets/atlas.h"

// Item definitions indexed by ItemType enum
// Order must match ItemType enum in items.h
// Fields: name, sprite, flags, maxStack, defaultMaterial, weight(kg), nutrition
// Use IF_MATERIAL_NAME for items that display their material (e.g. "Oak Log")
#define MN IF_MATERIAL_NAME
const ItemDef itemDefs[ITEM_TYPE_COUNT] = {
    [ITEM_RED]          = { "Red Crate",    SPRITE_crate_red,    IF_STACKABLE, 10, MAT_NONE, 1.0f, 0.0f },
    [ITEM_GREEN]        = { "Green Crate",  SPRITE_crate_green,  IF_STACKABLE, 10, MAT_NONE, 1.0f, 0.0f },
    [ITEM_BLUE]         = { "Blue Crate",   SPRITE_crate_blue,   IF_STACKABLE, 10, MAT_NONE, 1.0f, 0.0f },
    [ITEM_ROCK]         = { "Raw Stone",    SPRITE_loose_rock,   IF_STACKABLE | MN, 20, MAT_GRANITE, 25.0f, 0.0f },
    [ITEM_BLOCKS]       = { "Blocks",       SPRITE_block_stone,  IF_STACKABLE | IF_BUILDING_MAT | MN, 20, MAT_GRANITE, 20.0f, 0.0f },
    [ITEM_LOG]          = { "Log",          SPRITE_block_wood,   IF_STACKABLE | IF_BUILDING_MAT | IF_FUEL | MN, 20, MAT_OAK, 20.0f, 0.0f },
    [ITEM_SAPLING]      = { "Sapling",      SPRITE_tree_sapling_oak, IF_STACKABLE | MN, 20, MAT_OAK, 2.0f, 0.0f },
    [ITEM_LEAVES]       = { "Leaves",       SPRITE_tree_leaves_oak,  IF_STACKABLE | IF_FUEL | MN, 20, MAT_OAK, 0.5f, 0.0f },
    [ITEM_DIRT]         = { "Dirt",         SPRITE_dirt,         IF_STACKABLE, 20, MAT_DIRT, 12.0f, 0.0f },
    [ITEM_CLAY]         = { "Clay",         SPRITE_clay,         IF_STACKABLE, 20, MAT_CLAY, 15.0f, 0.0f },
    [ITEM_GRAVEL]       = { "Gravel",       SPRITE_gravel,       IF_STACKABLE, 20, MAT_GRAVEL, 14.0f, 0.0f },
    [ITEM_SAND]         = { "Sand",         SPRITE_sand,         IF_STACKABLE, 20, MAT_SAND, 13.0f, 0.0f },
    [ITEM_PEAT]         = { "Peat",         SPRITE_peat,         IF_STACKABLE | IF_FUEL, 20, MAT_PEAT, 8.0f, 0.0f },
    [ITEM_PLANKS]       = { "Planks",       SPRITE_tree_planks_oak, IF_STACKABLE | IF_BUILDING_MAT | IF_FUEL | MN, 20, MAT_OAK, 5.0f, 0.0f },
    [ITEM_STICKS]       = { "Sticks",       SPRITE_block_wood,   IF_STACKABLE | IF_FUEL | MN, 20, MAT_OAK, 1.0f, 0.0f },
    [ITEM_POLES]        = { "Poles",        SPRITE_tree_branch_oak, IF_STACKABLE | IF_BUILDING_MAT | IF_FUEL | MN, 20, MAT_OAK, 3.0f, 0.0f },
    [ITEM_GRASS]        = { "Grass",        SPRITE_grass_tall,   IF_STACKABLE, 20, MAT_NONE, 0.5f, 0.0f },
    [ITEM_DRIED_GRASS]  = { "Dried Grass",  SPRITE_grass_trampled, IF_STACKABLE, 20, MAT_NONE, 0.3f, 0.0f },
    [ITEM_BRICKS]       = { "Bricks",       SPRITE_wall,         IF_STACKABLE | IF_BUILDING_MAT, 20, MAT_BRICK, 3.0f, 0.0f },
    [ITEM_CHARCOAL]     = { "Charcoal",     SPRITE_block_wood,   IF_STACKABLE | IF_FUEL, 20, MAT_NONE, 2.0f, 0.0f },
    [ITEM_ASH]          = { "Ash",          SPRITE_gravel,       IF_STACKABLE, 20, MAT_NONE, 1.0f, 0.0f },
    [ITEM_BARK]         = { "Bark",         SPRITE_tree_bark_oak, IF_STACKABLE | IF_FUEL | MN, 20, MAT_OAK, 1.5f, 0.0f },
    [ITEM_STRIPPED_LOG] = { "Stripped Log",  SPRITE_tree_stripped_log_oak, IF_STACKABLE | IF_BUILDING_MAT | IF_FUEL | MN, 20, MAT_OAK, 18.0f, 0.0f },
    [ITEM_SHORT_STRING] = { "Short String", SPRITE_short_string, IF_STACKABLE, 20, MAT_NONE, 0.2f, 0.0f },
    [ITEM_CORDAGE]      = { "Cordage",      SPRITE_cordage,      IF_STACKABLE, 20, MAT_NONE, 0.5f, 0.0f },
    [ITEM_BERRIES]      = { "Berries",      SPRITE_division,     IF_STACKABLE | IF_EDIBLE, 20, MAT_NONE, 0.3f, 0.3f },
    [ITEM_DRIED_BERRIES]= { "Dried Berries",SPRITE_grass_trampled,IF_STACKABLE | IF_EDIBLE, 20, MAT_NONE, 0.2f, 0.25f },
    [ITEM_BASKET]       = { "Basket",       SPRITE_crate_green,  IF_STACKABLE | IF_CONTAINER, 10, MAT_NONE, 1.0f, 0.0f },
    [ITEM_CLAY_POT]     = { "Clay Pot",     SPRITE_crate_red,    IF_STACKABLE | IF_CONTAINER, 10, MAT_NONE, 3.0f, 0.0f },
    [ITEM_CHEST]        = { "Chest",        SPRITE_crate_blue,   IF_CONTAINER, 1, MAT_NONE, 8.0f, 0.0f },
    [ITEM_PLANK_BED]    = { "Plank Bed",   SPRITE_block_wood,   MN, 1, MAT_OAK, 15.0f, 0.0f },
    [ITEM_CHAIR]        = { "Chair",       SPRITE_block_wood,   MN, 1, MAT_OAK, 8.0f, 0.0f },
};
#undef MN
