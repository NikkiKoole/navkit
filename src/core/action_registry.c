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
        .barText = NULL,
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
        .name = "WALL",
        .barDisplayText = "Wall",
        .barText = "[1]Stone%s [2]Wood%s [3]Dirt%s    L-drag place  R-drag erase  [ESC]Back",
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
        .name = "FLOOR",
        .barDisplayText = "Floor",
        .barText = "[1]Stone%s [2]Wood%s [3]Dirt%s    L-drag place  [ESC]Back",
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
        .name = "LADDER",
        .barDisplayText = "Ladder",
        .barText = "L-drag place  [ESC]Back",
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
        .name = "RAMP",
        .barDisplayText = "Ramp",
        .barText = "L-click place  R-click erase  [ESC]Back",
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
        .name = "STOCKPILE",
        .barDisplayText = "Stockpile",
        .barText = "L-drag create  R-drag erase  [ESC]Back",
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
        .name = "REFUSE_PILE",
        .barDisplayText = "Refuse pile",
        .barText = "L-drag create  R-drag erase  [ESC]Back",
        .barKey = 'r',
        .barUnderlinePos = 0,
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
        .barText = "[D]irt  [C]lay  [G]ravel  [S]and  [P]eat  roc[K]    [ESC]Back",
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
        .barText = "[T]rack  trai[N]    [ESC]Back",
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
        .name = "WORKSHOP",
        .barDisplayText = "workshop(T)",
        .barText = "[S]tonecutter  s[A]wmill  [K]iln    [ESC]Back",
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
        .name = "DIRT",
        .barDisplayText = "Dirt",
        .barText = "L-drag place  +Shift=pile mode  [ESC]Back",
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
        .name = "CLAY",
        .barDisplayText = "Clay",
        .barText = "L-drag place  +Shift=pile mode  [ESC]Back",
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
        .name = "GRAVEL",
        .barDisplayText = "Gravel",
        .barText = "L-drag place  +Shift=pile mode  [ESC]Back",
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
        .name = "SAND",
        .barDisplayText = "Sand",
        .barText = "L-drag place  +Shift=pile mode  [ESC]Back",
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
        .name = "PEAT",
        .barDisplayText = "Peat",
        .barText = "L-drag place  +Shift=pile mode  [ESC]Back",
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
        .name = "ROCK",
        .barDisplayText = "rocK",
        .barText = "L-drag place  +Shift=pile mode  [ESC]Back",
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
        .name = "STONECUTTER",
        .barDisplayText = "Stonecutter",
        .barText = "L-click place  [ESC]Back",
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
        .name = "SAWMILL",
        .barDisplayText = "sAwmill",
        .barText = "L-click place  [ESC]Back",
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
        .name = "KILN",
        .barDisplayText = "Kiln",
        .barText = "L-click place  [ESC]Back",
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
        .name = "CHARCOAL PIT",
        .barDisplayText = "Charcoal pit",
        .barText = "L-click place  [ESC]Back",
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
        .name = "HEARTH",
        .barDisplayText = "Hearth",
        .barText = "L-click place  [ESC]Back",
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
        .name = "DRYING RACK",
        .barDisplayText = "Drying rack",
        .barText = "L-click place  [ESC]Back",
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
        .name = "ROPE MAKER",
        .barDisplayText = "Rope maker",
        .barText = "L-click place  [ESC]Back",
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
        .name = "CARPENTER",
        .barDisplayText = "Carpenter",
        .barText = "L-click place  [ESC]Back",
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
        .name = "FIRE PIT",
        .barDisplayText = "Fire pit",
        .barText = "L-click place  [ESC]Back",
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
        .name = "GROUND FIRE",
        .barDisplayText = "Ground fire",
        .barText = "L-click place  [ESC]Back",
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
        .name = "BUTCHER",
        .barDisplayText = "Butcher",
        .barText = "L-click place  [ESC]Back",
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
        .name = "COMPOST_PILE",
        .barDisplayText = "Compost Pile",
        .barText = "L-click place  [ESC]Back",
        .barKey = 'o',
        .barUnderlinePos = 1,
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
        .barText = "L-drag place  R-drag remove  [ESC]Back",
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
        .barText = "L-click spawn  R-click remove  [ESC]Back",
        .barKey = 'n',
        .barUnderlinePos = 4,
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_DRAW_TRANSPORT,
        .canDrag = false,
        .canErase = true
    },
    {
        .action = ACTION_WORK_HARVEST_BERRY,
        .name = "HARVEST BERRY",
        .barDisplayText = "harvest Berry",
        .barText = "L-click designate  R-click cancel  [ESC]Back",
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
        .barText = "L-drag designate  R-drag cancel  [ESC]Back",
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
        .barText = "L-drag designate  R-drag cancel  [ESC]Back",
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
        .barText = "L-drag designate  R-drag cancel  [ESC]Back",
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
        .barText = "L-drag designate  R-drag cancel  [ESC]Back",
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
        .barText = "L-click designate  R-click cancel  [ESC]Back",
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
        .barText = "L-drag designate  R-drag cancel  [ESC]Back",
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
        .barText = "L-click designate  R-click cancel  [ESC]Back",
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
        .name = "WALL",
        .barDisplayText = "Wall",
        .barText = "L-drag designate  R-drag cancel  [ESC]Back",
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
        .name = "FLOOR",
        .barDisplayText = "Floor",
        .barText = "L-drag designate  R-drag cancel  [ESC]Back",
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
        .name = "LADDER",
        .barDisplayText = "Ladder",
        .barText = "L-drag designate  R-drag cancel  [ESC]Back",
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
        .name = "RAMP",
        .barDisplayText = "Ramp",
        .barText = "L-click designate  R-click cancel  [ESC]Back",
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
        .name = "FURNITURE",
        .barDisplayText = "Furniture",
        .barText = "L-drag designate  R-drag cancel  [R]Cycle recipe  [ESC]Back",
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
        .name = "DOOR",
        .barDisplayText = "Door",
        .barText = "L-drag designate  R-drag cancel  [R]Cycle recipe  [ESC]Back",
        .barKey = 'd',
        .barUnderlinePos = 0,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .parentAction = ACTION_NONE,
        .canDrag = true,
        .canErase = true
    },
    
    {
        .action = ACTION_WORK_WORKSHOP,
        .name = "WORKSHOP",
        .barDisplayText = "worksHop",
        .barText = "[C]ampfire [D]rying rack [M]aker [H]earth ...",
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
        .name = "FIRE PIT",
        .barDisplayText = "Fire pit",
        .barText = "L-click place  R-click cancel  [ESC]Back",
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
        .name = "GROUND FIRE",
        .barDisplayText = "Ground fire",
        .barText = "L-click place  R-click cancel  [ESC]Back",
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
        .name = "BUTCHER",
        .barDisplayText = "Butcher",
        .barText = "L-click place  R-click cancel  [ESC]Back",
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
        .name = "COMPOST PILE",
        .barDisplayText = "Compost Pile",
        .barText = "L-click place  R-click cancel  [ESC]Back",
        .barKey = 'o',
        .barUnderlinePos = 1,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .parentAction = ACTION_WORK_WORKSHOP,
        .canDrag = false,
        .canErase = true
    },
    {
        .action = ACTION_WORK_WORKSHOP_DRYING_RACK,
        .name = "DRYING RACK",
        .barDisplayText = "Drying Rack",
        .barText = "L-click place  R-click cancel  [ESC]Back",
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
        .name = "ROPE MAKER",
        .barDisplayText = "Rope Maker",
        .barText = "L-click place  R-click cancel  [ESC]Back",
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
        .name = "CHARCOAL PIT",
        .barDisplayText = "Charcoal Pit",
        .barText = "L-click place  R-click cancel  [ESC]Back",
        .barKey = 'a',
        .barUnderlinePos = 2,
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .parentAction = ACTION_WORK_WORKSHOP,
        .canDrag = false,
        .canErase = true
    },
    {
        .action = ACTION_WORK_WORKSHOP_HEARTH,
        .name = "HEARTH",
        .barDisplayText = "Hearth",
        .barText = "L-click place  R-click cancel  [ESC]Back",
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
        .name = "STONECUTTER",
        .barDisplayText = "Stonecutter",
        .barText = "L-click place  R-click cancel  [ESC]Back",
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
        .name = "SAWMILL",
        .barDisplayText = "Sawmill",
        .barText = "L-click place  R-click cancel  [ESC]Back",
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
        .name = "KILN",
        .barDisplayText = "Kiln",
        .barText = "L-click place  R-click cancel  [ESC]Back",
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
        .name = "CARPENTER",
        .barDisplayText = "Carpenter",
        .barText = "L-click place  R-click cancel  [ESC]Back",
        .barKey = 'p',
        .barUnderlinePos = 3,
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
        .barText = "L-click designate  R-click cancel  [ESC]Back",
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
        .barText = "L-click designate  R-click cancel  [ESC]Back",
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
        .barText = "L-click designate  R-click cancel  [ESC]Back",
        .barKey = 'g',
        .barUnderlinePos = 7,
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
        .barText = "L-click designate  R-click cancel  [ESC]Back",
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
        .barText = "L-click designate  R-click cancel  [ESC]Back",
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
        .barText = "L-click designate  R-click cancel  [ESC]Back",
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
        .barText = "L-drag designate  R-drag cancel  [ESC]Back",
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
        .barText = "L-drag create zone  R-drag delete zone  [ESC]Back",
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
        .barText = "L-click designate  R-click cancel  [ESC]Back",
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
        .barText = "L-drag mark animals  R-drag unmark  [ESC]Back",
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
        .name = "FARM",
        .barDisplayText = "Farm",
        .barText = "L-drag designate  R-drag cancel  [ESC]Back",
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
        .name = "WATER",
        .barDisplayText = "Water",
        .barText = "L-drag add  R-drag remove  [ESC]Back",
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
        .name = "FIRE",
        .barDisplayText = "Fire",
        .barText = "L-drag ignite  R-drag extinguish  [ESC]Back",
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
        .barText = "L-drag heat  R-drag cool  [ESC]Back",
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
        .barText = "L-drag cool  R-drag heat  [ESC]Back",
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
        .barText = "L-drag add  R-drag remove  [ESC]Back",
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
        .barText = "L-drag add  R-drag remove  [ESC]Back",
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
        .barText = "L-drag grow  R-drag trample  [ESC]Back",
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
        .name = "TREE",
        .barDisplayText = "tRee",
        .barText = "L-click place  R-click remove  [ESC]Back",
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
        .name = "SCULPT",
        .barDisplayText = "sCulpt",
        .barText = "L-drag raise  R-drag lower  hold [S]mooth  [1-4]Brush  [ESC]Back",
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
        .name = "LIGHT",
        .barDisplayText = "Light",
        .barText = "L-click place  R-click remove  [ESC]Back",
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
        .barText = "L-drag place  R-drag remove  [ESC]Back",
        .barKey = 'b',
        .barUnderlinePos = 0,
        .requiredMode = MODE_SANDBOX,
        .requiredSubMode = SUBMODE_NONE,
        .parentAction = ACTION_NONE,
        .canDrag = true,
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
        if (def->requiredMode == mode &&
            def->requiredSubMode == subMode &&
            def->parentAction == parent &&
            def->barKey != 0) {
            out[count++] = def;
        }
    }
    return count;
}
