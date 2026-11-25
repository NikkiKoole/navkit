#include "action_registry.h"

// Centralized registry for all InputAction metadata.
// This is the single source of truth for action names, keys, bar display text,
// mode/submode requirements, and parent-child relationships.
// Adding a new action here automatically gives it a bar button, key binding,
// and back-one-level support.

const ActionDef ACTION_REGISTRY[] = {
    // ACTION_NONE - no action selected
    {
        .action = ACTION_NONE,
        .name = "NONE",
        .barDisplayText = NULL,
        .barKey = 0,
        .barUnderlinePos = -1,
        .requiredMode = MODE_NORMAL,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_NONE,
        .canDrag = false,
        .canErase = false
    },
    
    // ========================================================================
    // MODE_DRAW actions (top-level)
    // ========================================================================
    
    {
        .action = ACTION_DRAW_WALL,
        .twin = ACTION_WORK_CONSTRUCT,
        .hints = { "L-drag place", "R-drag erase" },
        .name = "WALL",
        .barDisplayText = "Wall",
        .barKey = 'w',
        .barUnderlinePos = 0,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },
    {
        .action = ACTION_DRAW_FLOOR,
        .twin = ACTION_WORK_FLOOR,
        .hints = { "L-drag place" },
        .name = "FLOOR",
        .barDisplayText = "Floor",
        .barKey = 'f',
        .barUnderlinePos = 0,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = false
    },
    {
        .action = ACTION_DRAW_LADDER,
        .twin = ACTION_WORK_LADDER,
        .hints = { "L-drag place" },
        .name = "LADDER",
        .barDisplayText = "Ladder",
        .barKey = 'l',
        .barUnderlinePos = 0,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = false
    },
    {
        .action = ACTION_DRAW_RAMP,
        .twin = ACTION_WORK_RAMP,
        .hints = { "L-drag place", "R-drag erase" },
        .name = "RAMP",
        .barDisplayText = "Ramp",
        .barKey = 'r',
        .barUnderlinePos = 0,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_NONE,
        .canDrag = false,
        .canErase = true
    },
    {
        .action = ACTION_DRAW_STOCKPILE,
        .hints = { "L-drag create", "R-drag erase" },
        .name = "STOCKPILE",
        .barDisplayText = "Stockpile",
        .barKey = 's',
        .barUnderlinePos = 0,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },
    {
        .action = ACTION_DRAW_REFUSE_PILE,
        .hints = { "L-drag create", "R-drag erase" },
        .name = "REFUSE_PILE",
        .barDisplayText = "Refuse pile",
        .barKey = 'e',
        .barUnderlinePos = 1,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },

    // DRAW > SOIL (category)
    {
        .action = ACTION_DRAW_SOIL,
        .name = "SOIL",
        .barDisplayText = "sOil",
        .barKey = 'o',
        .barUnderlinePos = 1,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_NONE,
        .canDrag = false,
        .canErase = false
    },
    
    // DRAW > TRANSPORT (category)
    {
        .action = ACTION_DRAW_TRANSPORT,
        .name = "TRANSPORT",
        .barDisplayText = "traiN",
        .barKey = 'n',
        .barUnderlinePos = 4,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_NONE,
        .canDrag = false,
        .canErase = false
    },
    
    // DRAW > WORKSHOP (category)
    {
        .action = ACTION_DRAW_WORKSHOP,
        .twin = ACTION_WORK_WORKSHOP,
        .name = "WORKSHOP",
        .barDisplayText = "workshop(T)",
        .barKey = 't',
        .barUnderlinePos = 9,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_NONE,
        .canDrag = false,
        .canErase = false
    },
    
    // ========================================================================
    // MODE_DRAW > SOIL children
    // ========================================================================
    
    {
        .action = ACTION_DRAW_SOIL_DIRT,
        .hints = { "L-drag place", "+Shift=pile mode" },
        .name = "DIRT",
        .barDisplayText = "Dirt",
        .barKey = 'd',
        .barUnderlinePos = 0,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_DRAW_SOIL,
        .canDrag = true,
        .canErase = false
    },
    {
        .action = ACTION_DRAW_SOIL_CLAY,
        .hints = { "L-drag place", "+Shift=pile mode" },
        .name = "CLAY",
        .barDisplayText = "Clay",
        .barKey = 'c',
        .barUnderlinePos = 0,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_DRAW_SOIL,
        .canDrag = true,
        .canErase = false
    },
    {
        .action = ACTION_DRAW_SOIL_GRAVEL,
        .hints = { "L-drag place", "+Shift=pile mode" },
        .name = "GRAVEL",
        .barDisplayText = "Gravel",
        .barKey = 'g',
        .barUnderlinePos = 0,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_DRAW_SOIL,
        .canDrag = true,
        .canErase = false
    },
    {
        .action = ACTION_DRAW_SOIL_SAND,
        .hints = { "L-drag place", "+Shift=pile mode" },
        .name = "SAND",
        .barDisplayText = "Sand",
        .barKey = 's',
        .barUnderlinePos = 0,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_DRAW_SOIL,
        .canDrag = true,
        .canErase = false
    },
    {
        .action = ACTION_DRAW_SOIL_PEAT,
        .hints = { "L-drag place", "+Shift=pile mode" },
        .name = "PEAT",
        .barDisplayText = "Peat",
        .barKey = 'p',
        .barUnderlinePos = 0,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_DRAW_SOIL,
        .canDrag = true,
        .canErase = false
    },
    {
        .action = ACTION_DRAW_SOIL_ROCK,
        .hints = { "L-drag place", "+Shift=pile mode" },
        .name = "ROCK",
        .barDisplayText = "rocK",
        .barKey = 'k',
        .barUnderlinePos = 3,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_DRAW_SOIL,
        .canDrag = true,
        .canErase = false
    },
    
    // ========================================================================
    // MODE_DRAW > WORKSHOP children
    // ========================================================================
    
    {
        .action = ACTION_DRAW_WORKSHOP_STONECUTTER,
        .twin = ACTION_WORK_WORKSHOP_STONECUTTER,
        .hints = { "L-click place" },
        .name = "STONECUTTER",
        .barDisplayText = "Stonecutter",
        .barKey = 's',
        .barUnderlinePos = 0,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_DRAW_WORKSHOP,
        .canDrag = true,
        .canErase = false
    },
    {
        .action = ACTION_DRAW_WORKSHOP_SAWMILL,
        .twin = ACTION_WORK_WORKSHOP_SAWMILL,
        .hints = { "L-click place" },
        .name = "SAWMILL",
        .barDisplayText = "sAwmill",
        .barKey = 'a',
        .barUnderlinePos = 1,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_DRAW_WORKSHOP,
        .canDrag = true,
        .canErase = false
    },
    {
        .action = ACTION_DRAW_WORKSHOP_KILN,
        .twin = ACTION_WORK_WORKSHOP_KILN,
        .hints = { "L-click place" },
        .name = "KILN",
        .barDisplayText = "Kiln",
        .barKey = 'k',
        .barUnderlinePos = 0,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_DRAW_WORKSHOP,
        .canDrag = true,
        .canErase = false
    },
    {
        .action = ACTION_DRAW_WORKSHOP_CHARCOAL_PIT,
        .twin = ACTION_WORK_WORKSHOP_CHARCOAL_PIT,
        .hints = { "L-click place" },
        .name = "CHARCOAL PIT",
        .barDisplayText = "Charcoal pit",
        .barKey = 'c',
        .barUnderlinePos = 0,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_DRAW_WORKSHOP,
        .canDrag = true,
        .canErase = false
    },
    {
        .action = ACTION_DRAW_WORKSHOP_HEARTH,
        .twin = ACTION_WORK_WORKSHOP_HEARTH,
        .hints = { "L-click place" },
        .name = "HEARTH",
        .barDisplayText = "Hearth",
        .barKey = 'h',
        .barUnderlinePos = 0,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_DRAW_WORKSHOP,
        .canDrag = true,
        .canErase = false
    },
    {
        .action = ACTION_DRAW_WORKSHOP_DRYING_RACK,
        .twin = ACTION_WORK_WORKSHOP_DRYING_RACK,
        .hints = { "L-click place" },
        .name = "DRYING RACK",
        .barDisplayText = "Drying rack",
        .barKey = 'd',
        .barUnderlinePos = 0,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_DRAW_WORKSHOP,
        .canDrag = true,
        .canErase = false
    },
    {
        .action = ACTION_DRAW_WORKSHOP_ROPE_MAKER,
        .twin = ACTION_WORK_WORKSHOP_ROPE_MAKER,
        .hints = { "L-click place" },
        .name = "ROPE MAKER",
        .barDisplayText = "Rope maker",
        .barKey = 'r',
        .barUnderlinePos = 0,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_DRAW_WORKSHOP,
        .canDrag = true,
        .canErase = false
    },
    {
        .action = ACTION_DRAW_WORKSHOP_CARPENTER,
        .twin = ACTION_WORK_WORKSHOP_CARPENTER,
        .hints = { "L-click place" },
        .name = "CARPENTER",
        .barDisplayText = "Carpenter",
        .barKey = 'p',
        .barUnderlinePos = 3,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_DRAW_WORKSHOP,
        .canDrag = true,
        .canErase = false
    },
    {
        .action = ACTION_DRAW_WORKSHOP_CAMPFIRE,
        .twin = ACTION_WORK_WORKSHOP_CAMPFIRE,
        .name = "FIRE PIT",
        .barDisplayText = "Fire pit",
        .barKey = 'f',
        .barUnderlinePos = 0,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_DRAW_WORKSHOP,
        .canDrag = true,
        .canErase = false
    },
    {
        .action = ACTION_DRAW_WORKSHOP_GROUND_FIRE,
        .twin = ACTION_WORK_WORKSHOP_GROUND_FIRE,
        .name = "GROUND FIRE",
        .barDisplayText = "Ground fire",
        .barKey = 'g',
        .barUnderlinePos = 0,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_DRAW_WORKSHOP,
        .canDrag = true,
        .canErase = false
    },
    {
        .action = ACTION_DRAW_WORKSHOP_BUTCHER,
        .twin = ACTION_WORK_WORKSHOP_BUTCHER,
        .name = "BUTCHER",
        .barDisplayText = "Butcher",
        .barKey = 'b',
        .barUnderlinePos = 0,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_DRAW_WORKSHOP,
        .canDrag = true,
        .canErase = false
    },
    {
        .action = ACTION_DRAW_WORKSHOP_COMPOST_PILE,
        .twin = ACTION_WORK_WORKSHOP_COMPOST_PILE,
        .name = "COMPOST_PILE",
        .barDisplayText = "Compost Pile",
        .barKey = 'o',
        .barUnderlinePos = 1,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_DRAW_WORKSHOP,
        .canDrag = true,
        .canErase = false
    },
    {
        .action = ACTION_DRAW_WORKSHOP_QUERN,
        .twin = ACTION_WORK_WORKSHOP_QUERN,
        .name = "QUERN",
        .barDisplayText = "Quern",
        .barKey = 'q',
        .barUnderlinePos = 0,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_DRAW_WORKSHOP,
        .canDrag = true,
        .canErase = false
    },
    {
        .action = ACTION_DRAW_WORKSHOP_LOOM,
        .twin = ACTION_WORK_WORKSHOP_LOOM,
        .name = "LOOM",
        .barDisplayText = "Loom",
        .barKey = 'l',
        .barUnderlinePos = 0,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_DRAW_WORKSHOP,
        .canDrag = true,
        .canErase = false
    },
    {
        .action = ACTION_DRAW_WORKSHOP_TANNING_RACK,
        .twin = ACTION_WORK_WORKSHOP_TANNING_RACK,
        .name = "TANNING_RACK",
        .barDisplayText = "Tanning Rack",
        .barKey = 'n',
        .barUnderlinePos = 2,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_DRAW_WORKSHOP,
        .canDrag = true,
        .canErase = false
    },
    {
        .action = ACTION_DRAW_WORKSHOP_TAILOR,
        .twin = ACTION_WORK_WORKSHOP_TAILOR,
        .name = "TAILOR",
        .barDisplayText = "Tailor",
        .barKey = 't',
        .barUnderlinePos = 0,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_DRAW_WORKSHOP,
        .canDrag = true,
        .canErase = false
    },
    {
        .action = ACTION_DRAW_WORKSHOP_MUD_MIXER,
        .twin = ACTION_WORK_WORKSHOP_MUD_MIXER,
        .name = "MUD MIXER",
        .barDisplayText = "Mud mixer",
        .barKey = 'm',
        .barUnderlinePos = 0,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_DRAW_WORKSHOP,
        .canDrag = true,
        .canErase = false
    },
    {
        .action = ACTION_DRAW_WORKSHOP_STOVE,
        .twin = ACTION_WORK_WORKSHOP_STOVE,
        .name = "STOVE",
        .barDisplayText = "Stove",
        .barKey = 'v',
        .barUnderlinePos = 3,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_DRAW_WORKSHOP,
        .canDrag = true,
        .canErase = false
    },
    {
        .action = ACTION_DRAW_WORKSHOP_COUNTER,
        .twin = ACTION_WORK_WORKSHOP_COUNTER,
        .name = "COUNTER",
        .barDisplayText = "coUnter",
        .barKey = 'u',
        .barUnderlinePos = 2,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_DRAW_WORKSHOP,
        .canDrag = true,
        .canErase = false
    },

    // ========================================================================
    // MODE_DRAW > TRANSPORT children
    // ========================================================================
    
    {
        .action = ACTION_DRAW_TRACK,
        .name = "TRACK",
        .barDisplayText = "Track",
        .barKey = 't',
        .barUnderlinePos = 0,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_DRAW_TRANSPORT,
        .canDrag = true,
        .canErase = true
    },
    {
        .action = ACTION_DRAW_TRAIN,
        .name = "TRAIN",
        .barDisplayText = "traiN",
        .barKey = 'n',
        .barUnderlinePos = 4,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_DRAW_TRANSPORT,
        .canDrag = false,
        .canErase = true
    },
    {
        .action = ACTION_DRAW_PLATFORM,
        .name = "PLATFORM",
        .barDisplayText = "Platform",
        .barKey = 'p',
        .barUnderlinePos = 0,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_DRAW_TRANSPORT,
        .canDrag = true,
        .canErase = true
    },
    {
        .action = ACTION_DRAW_ROAD,   // (cars experiment: the cars-city road plan; input.c HandleRoadTool)
        .name = "ROAD",
        .barDisplayText = "Road",
        .barKey = 'r',
        .barUnderlinePos = 0,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_DRAW_TRANSPORT,
        .canDrag = false,
        .canErase = true,
        .hints = { "L-click start, L-click end", "drag 2nd click: bend", "R-click: remove / cancel", "Shift: no snapping" },
    },
    {
        .action = ACTION_WORK_HARVEST_BERRY,
        .name = "HARVEST BERRY",
        .barDisplayText = "harvest Berry",
        .barKey = 'b',
        .barUnderlinePos = 8,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_HARVEST,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },
    {
        .action = ACTION_WORK_KNAP,
        .name = "KNAP",
        .barDisplayText = "Knap stone",
        .barKey = 'k',
        .barUnderlinePos = 0,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_HARVEST,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },
    {
        .action = ACTION_WORK_DIG_ROOTS,
        .name = "DIG ROOTS",
        .barDisplayText = "dig Roots",
        .barKey = 'r',
        .barUnderlinePos = 4,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_HARVEST,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },
    
    // ========================================================================
    // MODE_WORK > SUBMODE_DIG actions
    // ========================================================================
    
    {
        .action = ACTION_WORK_MINE,
        .name = "MINE",
        .barDisplayText = "Mine",
        .barKey = 'm',
        .barUnderlinePos = 0,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_DIG,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },
    {
        .action = ACTION_WORK_CHANNEL,
        .name = "CHANNEL",
        .barDisplayText = "cHannel",
        .barKey = 'h',
        .barUnderlinePos = 1,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_DIG,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },
    {
        .action = ACTION_WORK_DIG_RAMP,
        .name = "DIG RAMP",
        .barDisplayText = "dig Ramp",
        .barKey = 'r',
        .barUnderlinePos = 4,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_DIG,
        .parentAction = ACTION_NONE,
        .canDrag = false,
        .canErase = true
    },
    {
        .action = ACTION_WORK_REMOVE_FLOOR,
        .name = "REMOVE FLOOR",
        .barDisplayText = "remove Floor",
        .barKey = 'f',
        .barUnderlinePos = 7,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_DIG,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },
    {
        .action = ACTION_WORK_REMOVE_RAMP,
        .name = "REMOVE RAMP",
        .barDisplayText = "remove ramp(Z)",
        .barKey = 'z',
        .barUnderlinePos = 12,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_DIG,
        .parentAction = ACTION_NONE,
        .canDrag = false,
        .canErase = true
    },
    
    // ========================================================================
    // MODE_WORK > SUBMODE_BUILD actions
    // ========================================================================
    
    {
        .action = ACTION_WORK_CONSTRUCT,
        .twin = ACTION_DRAW_WALL,
        .hints = { "L-drag designate", "R-drag cancel" },
        .name = "WALL",
        .barDisplayText = "Wall",
        .barKey = 'w',
        .barUnderlinePos = 0,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },
    {
        .action = ACTION_WORK_FLOOR,
        .twin = ACTION_DRAW_FLOOR,
        .hints = { "L-drag designate", "R-drag cancel" },
        .name = "FLOOR",
        .barDisplayText = "Floor",
        .barKey = 'f',
        .barUnderlinePos = 0,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },
    {
        .action = ACTION_WORK_LADDER,
        .twin = ACTION_DRAW_LADDER,
        .hints = { "L-drag designate", "R-drag cancel" },
        .name = "LADDER",
        .barDisplayText = "Ladder",
        .barKey = 'l',
        .barUnderlinePos = 0,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },
    {
        .action = ACTION_WORK_RAMP,
        .twin = ACTION_DRAW_RAMP,
        .hints = { "L-drag designate", "R-drag cancel" },
        .name = "RAMP",
        .barDisplayText = "Ramp",
        .barKey = 'r',
        .barUnderlinePos = 0,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .parentAction = ACTION_NONE,
        .canDrag = false,
        .canErase = true
    },
    {
        .action = ACTION_WORK_FURNITURE,
        .hints = { "L-drag designate", "R-drag cancel" },
        .name = "FURNITURE",
        .barDisplayText = "Furniture",
        .barKey = 'u',
        .barUnderlinePos = 1,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },
    
    {
        .action = ACTION_WORK_DOOR,
        .hints = { "L-drag designate", "R-drag cancel" },
        .name = "DOOR",
        .barDisplayText = "Door",
        .barKey = 'd',
        .barUnderlinePos = 0,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },
    {
        .action = ACTION_WORK_BELT,
        .hints = { "L-drag designate", "R-drag remove" },
        .name = "BELT",
        .barDisplayText = "Belt",
        .barKey = 'b',
        .barUnderlinePos = 0,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },
    {
        .action = ACTION_WORK_SHAFT,
        .hints = { "Shaft: from here up one level", "On a shaft top: one more level", "L-drag designate", "R-drag take down" },
        .name = "ELEVATOR",
        .barDisplayText = "Elevator",
        .barKey = 'e',
        .barUnderlinePos = 0,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },
    
    {
        .action = ACTION_WORK_WORKSHOP,
        .twin = ACTION_DRAW_WORKSHOP,
        .name = "WORKSHOP",
        .barDisplayText = "worksHop",
        .barKey = 'h',
        .barUnderlinePos = 5,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .parentAction = ACTION_NONE,
        .canDrag = false,
        .canErase = false
    },
    {
        .action = ACTION_WORK_WORKSHOP_CAMPFIRE,
        .twin = ACTION_DRAW_WORKSHOP_CAMPFIRE,
        .name = "FIRE PIT",
        .barDisplayText = "Fire pit",
        .barKey = 'f',
        .barUnderlinePos = 0,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .parentAction = ACTION_WORK_WORKSHOP,
        .canDrag = false,
        .canErase = true
    },
    {
        .action = ACTION_WORK_WORKSHOP_GROUND_FIRE,
        .twin = ACTION_DRAW_WORKSHOP_GROUND_FIRE,
        .name = "GROUND FIRE",
        .barDisplayText = "Ground fire",
        .barKey = 'g',
        .barUnderlinePos = 0,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .parentAction = ACTION_WORK_WORKSHOP,
        .canDrag = false,
        .canErase = true
    },
    {
        .action = ACTION_WORK_WORKSHOP_BUTCHER,
        .twin = ACTION_DRAW_WORKSHOP_BUTCHER,
        .name = "BUTCHER",
        .barDisplayText = "Butcher",
        .barKey = 'b',
        .barUnderlinePos = 0,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .parentAction = ACTION_WORK_WORKSHOP,
        .canDrag = false,
        .canErase = true
    },
    {
        .action = ACTION_WORK_WORKSHOP_COMPOST_PILE,
        .twin = ACTION_DRAW_WORKSHOP_COMPOST_PILE,
        .name = "COMPOST PILE",
        .barDisplayText = "Compost Pile",
        .barKey = 'o',
        .barUnderlinePos = 1,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .parentAction = ACTION_WORK_WORKSHOP,
        .canDrag = false,
        .canErase = true
    },
    {
        .action = ACTION_WORK_WORKSHOP_QUERN,
        .twin = ACTION_DRAW_WORKSHOP_QUERN,
        .name = "QUERN",
        .barDisplayText = "Quern",
        .barKey = 'q',
        .barUnderlinePos = 0,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .parentAction = ACTION_WORK_WORKSHOP,
        .canDrag = false,
        .canErase = true
    },
    {
        .action = ACTION_WORK_WORKSHOP_LOOM,
        .twin = ACTION_DRAW_WORKSHOP_LOOM,
        .name = "LOOM",
        .barDisplayText = "Loom",
        .barKey = 'l',
        .barUnderlinePos = 0,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .parentAction = ACTION_WORK_WORKSHOP,
        .canDrag = false,
        .canErase = true
    },
    {
        .action = ACTION_WORK_WORKSHOP_TANNING_RACK,
        .twin = ACTION_DRAW_WORKSHOP_TANNING_RACK,
        .name = "TANNING_RACK",
        .barDisplayText = "Tanning Rack",
        .barKey = 'n',
        .barUnderlinePos = 2,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .parentAction = ACTION_WORK_WORKSHOP,
        .canDrag = false,
        .canErase = true
    },
    {
        .action = ACTION_WORK_WORKSHOP_TAILOR,
        .twin = ACTION_DRAW_WORKSHOP_TAILOR,
        .name = "TAILOR",
        .barDisplayText = "Tailor",
        .barKey = 't',
        .barUnderlinePos = 0,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .parentAction = ACTION_WORK_WORKSHOP,
        .canDrag = false,
        .canErase = true
    },
    {
        .action = ACTION_WORK_WORKSHOP_DRYING_RACK,
        .twin = ACTION_DRAW_WORKSHOP_DRYING_RACK,
        .name = "DRYING RACK",
        .barDisplayText = "Drying Rack",
        .barKey = 'd',
        .barUnderlinePos = 0,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .parentAction = ACTION_WORK_WORKSHOP,
        .canDrag = false,
        .canErase = true
    },
    {
        .action = ACTION_WORK_WORKSHOP_ROPE_MAKER,
        .twin = ACTION_DRAW_WORKSHOP_ROPE_MAKER,
        .name = "ROPE MAKER",
        .barDisplayText = "Rope Maker",
        .barKey = 'm',
        .barUnderlinePos = 5,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .parentAction = ACTION_WORK_WORKSHOP,
        .canDrag = false,
        .canErase = true
    },
    {
        .action = ACTION_WORK_WORKSHOP_CHARCOAL_PIT,
        .twin = ACTION_DRAW_WORKSHOP_CHARCOAL_PIT,
        .name = "CHARCOAL PIT",
        .barDisplayText = "Charcoal Pit",
        .barKey = 'c',
        .barUnderlinePos = 0,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .parentAction = ACTION_WORK_WORKSHOP,
        .canDrag = false,
        .canErase = true
    },
    {
        .action = ACTION_WORK_WORKSHOP_HEARTH,
        .twin = ACTION_DRAW_WORKSHOP_HEARTH,
        .name = "HEARTH",
        .barDisplayText = "Hearth",
        .barKey = 'h',
        .barUnderlinePos = 0,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .parentAction = ACTION_WORK_WORKSHOP,
        .canDrag = false,
        .canErase = true
    },
    {
        .action = ACTION_WORK_WORKSHOP_STONECUTTER,
        .twin = ACTION_DRAW_WORKSHOP_STONECUTTER,
        .name = "STONECUTTER",
        .barDisplayText = "Stonecutter",
        .barKey = 's',
        .barUnderlinePos = 0,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .parentAction = ACTION_WORK_WORKSHOP,
        .canDrag = false,
        .canErase = true
    },
    {
        .action = ACTION_WORK_WORKSHOP_SAWMILL,
        .twin = ACTION_DRAW_WORKSHOP_SAWMILL,
        .name = "SAWMILL",
        .barDisplayText = "Sawmill",
        .barKey = 'w',
        .barUnderlinePos = 2,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .parentAction = ACTION_WORK_WORKSHOP,
        .canDrag = false,
        .canErase = true
    },
    {
        .action = ACTION_WORK_WORKSHOP_KILN,
        .twin = ACTION_DRAW_WORKSHOP_KILN,
        .name = "KILN",
        .barDisplayText = "Kiln",
        .barKey = 'k',
        .barUnderlinePos = 0,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .parentAction = ACTION_WORK_WORKSHOP,
        .canDrag = false,
        .canErase = true
    },
    {
        .action = ACTION_WORK_WORKSHOP_CARPENTER,
        .twin = ACTION_DRAW_WORKSHOP_CARPENTER,
        .name = "CARPENTER",
        .barDisplayText = "Carpenter",
        .barKey = 'p',
        .barUnderlinePos = 3,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .parentAction = ACTION_WORK_WORKSHOP,
        .canDrag = false,
        .canErase = true
    },
    {
        .action = ACTION_WORK_WORKSHOP_MUD_MIXER,
        .twin = ACTION_DRAW_WORKSHOP_MUD_MIXER,
        .name = "MUD MIXER",
        .barDisplayText = "Mud mixer",
        .barKey = 'x',
        .barUnderlinePos = 6,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .parentAction = ACTION_WORK_WORKSHOP,
        .canDrag = false,
        .canErase = true
    },
    {
        .action = ACTION_WORK_WORKSHOP_STOVE,
        .twin = ACTION_DRAW_WORKSHOP_STOVE,
        .name = "STOVE",
        .barDisplayText = "stoVe",
        .barKey = 'v',
        .barUnderlinePos = 3,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .parentAction = ACTION_WORK_WORKSHOP,
        .canDrag = false,
        .canErase = true
    },
    {
        .action = ACTION_WORK_WORKSHOP_COUNTER,
        .twin = ACTION_DRAW_WORKSHOP_COUNTER,
        .name = "COUNTER",
        .barDisplayText = "coUnter",
        .barKey = 'u',
        .barUnderlinePos = 2,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .parentAction = ACTION_WORK_WORKSHOP,
        .canDrag = false,
        .canErase = true
    },

    // ========================================================================
    // MODE_WORK > SUBMODE_HARVEST actions
    // ========================================================================
    
    {
        .action = ACTION_WORK_CHOP,
        .name = "CHOP TREE",
        .barDisplayText = "Chop tree",
        .barKey = 'c',
        .barUnderlinePos = 0,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_HARVEST,
        .parentAction = ACTION_NONE,
        .canDrag = false,
        .canErase = true
    },
    {
        .action = ACTION_WORK_CHOP_FELLED,
        .name = "CHOP FELLED",
        .barDisplayText = "chop Felled",
        .barKey = 'f',
        .barUnderlinePos = 5,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_HARVEST,
        .parentAction = ACTION_NONE,
        .canDrag = false,
        .canErase = true
    },
    {
        .action = ACTION_WORK_GATHER_GRASS,
        .name = "GATHER GRASS",
        .barDisplayText = "gather Grass",
        .barKey = 'g',
        .barUnderlinePos = 7,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_HARVEST,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },
    {
        .action = ACTION_WORK_GATHER_REEDS,
        .name = "GATHER REEDS",
        .barDisplayText = "gather rEeds",
        .barKey = 'e',
        .barUnderlinePos = 8,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_HARVEST,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },
    {
        .action = ACTION_WORK_GATHER_SAPLING,
        .name = "GATHER SAPLING",
        .barDisplayText = "gather Sapling",
        .barKey = 's',
        .barUnderlinePos = 7,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_HARVEST,
        .parentAction = ACTION_NONE,
        .canDrag = false,
        .canErase = true
    },
    {
        .action = ACTION_WORK_GATHER_TREE,
        .name = "GATHER TREE",
        .barDisplayText = "gather Tree",
        .barKey = 't',
        .barUnderlinePos = 7,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_HARVEST,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },
    {
        .action = ACTION_WORK_PLANT_SAPLING,
        .name = "PLANT SAPLING",
        .barDisplayText = "Plant sapling",
        .barKey = 'p',
        .barUnderlinePos = 0,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_HARVEST,
        .parentAction = ACTION_NONE,
        .canDrag = false,
        .canErase = true
    },
    
    // ========================================================================
    // MODE_WORK > SUBMODE_NONE (top-level WORK actions)
    // ========================================================================
    
    {
        .action = ACTION_WORK_CLEAN,
        .name = "CLEAN",
        .barDisplayText = "Clean",
        .barKey = 'c',
        .barUnderlinePos = 0,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },
    {
        .action = ACTION_WORK_GATHER,
        .name = "GATHER",
        .barDisplayText = "Gather",
        .barKey = 'g',
        .barUnderlinePos = 0,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },
    {
        .action = ACTION_WORK_EXPLORE,
        .name = "EXPLORE",
        .barDisplayText = "Explore",
        .barKey = 'e',
        .barUnderlinePos = 0,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_NONE,
        .canDrag = false,
        .canErase = true
    },
    {
        .action = ACTION_WORK_HUNT,
        .name = "HUNT",
        .barDisplayText = "hUnt",
        .barKey = 'u',
        .barUnderlinePos = 1,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },
    {
        .action = ACTION_WORK_FARM,
        .hints = { "L-drag designate", "R-drag cancel" },
        .name = "FARM",
        .barDisplayText = "Farm",
        .barKey = 'f',
        .barUnderlinePos = 0,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },

    // ========================================================================
    // MODE_SANDBOX actions
    // ========================================================================
    
    {
        .action = ACTION_SANDBOX_WATER,
        .hints = { "L-drag add", "R-drag remove", "+Shift=source/drain" },
        .name = "WATER",
        .barDisplayText = "Water",
        .barKey = 'w',
        .barUnderlinePos = 0,
        .requiredMode = MODE_SANDBOX,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },
    {
        .action = ACTION_SANDBOX_FIRE,
        .hints = { "L-drag ignite", "R-drag extinguish", "+Shift=source" },
        .name = "FIRE",
        .barDisplayText = "Fire",
        .barKey = 'f',
        .barUnderlinePos = 0,
        .requiredMode = MODE_SANDBOX,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },
    {
        .action = ACTION_SANDBOX_HEAT,
        .name = "HEAT",
        .barDisplayText = "Heat",
        .barKey = 'h',
        .barUnderlinePos = 0,
        .requiredMode = MODE_SANDBOX,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },
    {
        .action = ACTION_SANDBOX_COLD,
        .name = "COLD",
        .barDisplayText = "cOld",
        .barKey = 'o',
        .barUnderlinePos = 1,
        .requiredMode = MODE_SANDBOX,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },
    {
        .action = ACTION_SANDBOX_SMOKE,
        .name = "SMOKE",
        .barDisplayText = "sMoke",
        .barKey = 'm',
        .barUnderlinePos = 1,
        .requiredMode = MODE_SANDBOX,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },
    {
        .action = ACTION_SANDBOX_STEAM,
        .name = "STEAM",
        .barDisplayText = "stEam",
        .barKey = 'e',
        .barUnderlinePos = 2,
        .requiredMode = MODE_SANDBOX,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },
    {
        .action = ACTION_SANDBOX_GRASS,
        .name = "GRASS",
        .barDisplayText = "Grass",
        .barKey = 'g',
        .barUnderlinePos = 0,
        .requiredMode = MODE_SANDBOX,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },
    {
        .action = ACTION_SANDBOX_TREE,
        .hints = { "L-click place", "R-drag remove" },
        .name = "TREE",
        .barDisplayText = "tRee",
        .barKey = 'r',
        .barUnderlinePos = 1,
        .requiredMode = MODE_SANDBOX,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_NONE,
        .canDrag = false,
        .canErase = true
    },
    {
        .action = ACTION_SANDBOX_SCULPT,
        .hints = { "L-drag raise", "R-drag lower" },
        .name = "SCULPT",
        .barDisplayText = "sCulpt",
        .barKey = 'c',
        .barUnderlinePos = 1,
        .requiredMode = MODE_SANDBOX,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },
    {
        .action = ACTION_SANDBOX_LIGHT,
        .hints = { "L-drag place", "R-drag remove" },
        .name = "LIGHT",
        .barDisplayText = "Light",
        .barKey = 'l',
        .barUnderlinePos = 0,
        .requiredMode = MODE_SANDBOX,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_NONE,
        .canDrag = false,
        .canErase = true
    },
    {
        .action = ACTION_SANDBOX_BUSH,
        .name = "BUSH",
        .barDisplayText = "Bush",
        .barKey = 'b',
        .barUnderlinePos = 0,
        .requiredMode = MODE_SANDBOX,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },
    {
        .action = ACTION_SANDBOX_MOVER,
        .name = "MOVER",
        .barDisplayText = "moVer",
        .barKey = 'v',
        .barUnderlinePos = 2,
        .requiredMode = MODE_SANDBOX,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_NONE,
        .canDrag = false,
        .canErase = true
    },

};

const int ACTION_REGISTRY_COUNT = sizeof(ACTION_REGISTRY) / sizeof(ACTION_REGISTRY[0]);

const ActionDef* GetActionDef(InputAction action) {
    for (int i = 0; i < ACTION_REGISTRY_COUNT; i++) {
        if (ACTION_REGISTRY[i].action == action) {
            return &ACTION_REGISTRY[i];
        }
    }
    // Return ACTION_NONE entry if not found
    return &ACTION_REGISTRY[0];
}

int GetActionsForContext(InputMode mode, WorkSubMode subMode, InputAction parent,
                         const ActionDef** out, int maxOut) {
    int count = 0;
    for (int i = 0; i < ACTION_REGISTRY_COUNT && count < maxOut; i++) {
        const ActionDef* def = &ACTION_REGISTRY[i];
        // The Draw bar leaves out the tools that have a Work twin: they are picked in the Work
        // bar and done now or by order (buildNow, I flips it)
        if (mode == MODE_DRAW && parent == ACTION_NONE && def->twin != ACTION_NONE) continue;
        if (def->requiredMode == mode &&
            def->requiredSubMode == subMode &&
            def->parentAction == parent &&
            def->barKey != 0) {
            out[count++] = def;
        }
    }
    return count;
}
