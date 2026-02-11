#include "action_registry.h"

// Centralized registry for all InputAction metadata
// This eliminates parallel update patterns across GetActionName(), InputMode_GetBarText(),
// keybinding handlers, and mode transition logic.

const ActionDef ACTION_REGISTRY[] = {
    // ACTION_NONE - no action selected
    {
        .action = ACTION_NONE,
        .name = "NONE",
        .barText = NULL,
        .barKey = 0,
        .barUnderlinePos = -1,
        .requiredMode = MODE_NORMAL,
        .requiredSubMode = SUBMODE_NONE,
        .canDrag = false,
        .canErase = false
    },
    
    // ========================================================================
    // MODE_DRAW actions
    // ========================================================================
    
    // DRAW > WALL - [W]all in "DRAW: [W]all [F]loor..."
    {
        .action = ACTION_DRAW_WALL,
        .name = "WALL",
        .barText = "[1]Stone%s [2]Wood%s [3]Dirt%s    L-drag place  R-drag erase  [ESC]Back",
        .barKey = 'w',
        .barUnderlinePos = 0,  // [W]all
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .canDrag = true,
        .canErase = true
    },
    
    // DRAW > FLOOR - [F]loor
    {
        .action = ACTION_DRAW_FLOOR,
        .name = "FLOOR",
        .barText = "[1]Stone%s [2]Wood%s [3]Dirt%s    L-drag place  [ESC]Back",
        .barKey = 'f',
        .barUnderlinePos = 0,  // [F]loor
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .canDrag = true,
        .canErase = false
    },
    
    // DRAW > LADDER - [L]adder
    {
        .action = ACTION_DRAW_LADDER,
        .name = "LADDER",
        .barText = "L-drag place  [ESC]Back",
        .barKey = 'l',
        .barUnderlinePos = 0,  // [L]adder
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .canDrag = true,
        .canErase = false
    },
    
    // DRAW > RAMP - [R]amp
    {
        .action = ACTION_DRAW_RAMP,
        .name = "RAMP",
        .barText = "L-click place  R-click erase  [ESC]Back",
        .barKey = 'r',
        .barUnderlinePos = 0,  // [R]amp
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .canDrag = false,
        .canErase = true
    },
    
    // DRAW > STOCKPILE - [S]tockpile
    {
        .action = ACTION_DRAW_STOCKPILE,
        .name = "STOCKPILE",
        .barText = "L-drag create  R-drag erase  [ESC]Back",
        .barKey = 's',
        .barUnderlinePos = 0,  // [S]tockpile
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .canDrag = true,
        .canErase = true
    },
    
    // DRAW > WORKSHOP (category) - workshop([T])
    {
        .action = ACTION_DRAW_WORKSHOP,
        .name = "WORKSHOP",
        .barText = "[S]tonecutter  sa[W]mill  [K]iln    [ESC]Back",
        .barKey = 't',
        .barUnderlinePos = 9,  // workshop([T])
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .canDrag = false,
        .canErase = false
    },
    
    // DRAW > WORKSHOP > STONECUTTER - [S]tonecutter
    {
        .action = ACTION_DRAW_WORKSHOP_STONECUTTER,
        .name = "STONECUTTER",
        .barText = "L-drag place  [ESC]Back",
        .barKey = 's',
        .barUnderlinePos = 0,  // [S]tonecutter
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .canDrag = true,
        .canErase = false
    },
    
    // DRAW > WORKSHOP > SAWMILL - sa[W]mill
    {
        .action = ACTION_DRAW_WORKSHOP_SAWMILL,
        .name = "SAWMILL",
        .barText = "L-drag place  [ESC]Back",
        .barKey = 'w',
        .barUnderlinePos = 2,  // sa[W]mill
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .canDrag = true,
        .canErase = false
    },
    
    // DRAW > WORKSHOP > KILN - [K]iln
    {
        .action = ACTION_DRAW_WORKSHOP_KILN,
        .name = "KILN",
        .barText = "L-drag place  [ESC]Back",
        .barKey = 'k',
        .barUnderlinePos = 0,  // [K]iln
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .canDrag = true,
        .canErase = false
    },
    
    // DRAW > WORKSHOP > ROPE MAKER - [R]ope maker
    {
        .action = ACTION_DRAW_WORKSHOP_ROPE_MAKER,
        .name = "ROPE_MAKER",
        .barText = "L-drag place  [ESC]Back",
        .barKey = 'r',
        .barUnderlinePos = 0,  // [R]ope maker
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .canDrag = true,
        .canErase = false
    },
    
    // DRAW > SOIL (category) - s[O]il
    {
        .action = ACTION_DRAW_SOIL,
        .name = "SOIL",
        .barText = "[D]irt  [C]lay  [G]ravel  [S]and  [P]eat  roc[K]    [ESC]Back",
        .barKey = 'o',
        .barUnderlinePos = 1,  // s[O]il
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .canDrag = false,
        .canErase = false
    },
    
    // DRAW > SOIL > DIRT - [D]irt (UI ISSUE: duplicate with ACTION_DRAW_DIRT)
    {
        .action = ACTION_DRAW_SOIL_DIRT,
        .name = "DIRT",
        .barText = "L-drag place  +Shift=pile mode  [ESC]Back",
        .barKey = 'd',
        .barUnderlinePos = 0,  // [D]irt
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .canDrag = true,
        .canErase = false
    },
    
    // DRAW > SOIL > CLAY - [C]lay
    {
        .action = ACTION_DRAW_SOIL_CLAY,
        .name = "CLAY",
        .barText = "L-drag place  +Shift=pile mode  [ESC]Back",
        .barKey = 'c',
        .barUnderlinePos = 0,  // [C]lay
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .canDrag = true,
        .canErase = false
    },
    
    // DRAW > SOIL > GRAVEL - [G]ravel
    {
        .action = ACTION_DRAW_SOIL_GRAVEL,
        .name = "GRAVEL",
        .barText = "L-drag place  +Shift=pile mode  [ESC]Back",
        .barKey = 'g',
        .barUnderlinePos = 0,  // [G]ravel
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .canDrag = true,
        .canErase = false
    },
    
    // DRAW > SOIL > SAND - [S]and
    {
        .action = ACTION_DRAW_SOIL_SAND,
        .name = "SAND",
        .barText = "L-drag place  +Shift=pile mode  [ESC]Back",
        .barKey = 's',
        .barUnderlinePos = 0,  // [S]and
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .canDrag = true,
        .canErase = false
    },
    
    // DRAW > SOIL > PEAT - [P]eat
    {
        .action = ACTION_DRAW_SOIL_PEAT,
        .name = "PEAT",
        .barText = "L-drag place  +Shift=pile mode  [ESC]Back",
        .barKey = 'p',
        .barUnderlinePos = 0,  // [P]eat
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .canDrag = true,
        .canErase = false
    },
    
    // DRAW > SOIL > ROCK - roc[K]
    {
        .action = ACTION_DRAW_SOIL_ROCK,
        .name = "ROCK",
        .barText = "L-drag place  +Shift=pile mode  [ESC]Back",
        .barKey = 'k',
        .barUnderlinePos = 3,  // roc[K]
        .requiredMode = MODE_DRAW,
        .requiredSubMode = SUBMODE_NONE,
        .canDrag = true,
        .canErase = false
    },
    
    // ========================================================================
    // MODE_WORK > SUBMODE_DIG actions
    // ========================================================================
    
    // WORK > DIG > MINE - [M]ine
    {
        .action = ACTION_WORK_MINE,
        .name = "MINE",
        .barText = "L-drag designate  R-drag cancel  [ESC]Back",
        .barKey = 'm',
        .barUnderlinePos = 0,  // [M]ine
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_DIG,
        .canDrag = true,
        .canErase = true
    },
    
    // WORK > DIG > CHANNEL - c[H]annel
    {
        .action = ACTION_WORK_CHANNEL,
        .name = "CHANNEL",
        .barText = "L-drag designate  R-drag cancel  [ESC]Back",
        .barKey = 'h',
        .barUnderlinePos = 1,  // c[H]annel
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_DIG,
        .canDrag = true,
        .canErase = true
    },
    
    // WORK > DIG > DIG RAMP - dig [R]amp
    {
        .action = ACTION_WORK_DIG_RAMP,
        .name = "DIG RAMP",
        .barText = "L-click designate  R-click cancel  [ESC]Back",
        .barKey = 'r',
        .barUnderlinePos = 4,  // dig [R]amp
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_DIG,
        .canDrag = false,
        .canErase = true
    },
    
    // WORK > DIG > REMOVE FLOOR - remove [F]loor
    {
        .action = ACTION_WORK_REMOVE_FLOOR,
        .name = "REMOVE FLOOR",
        .barText = "L-drag designate  R-drag cancel  [ESC]Back",
        .barKey = 'f',
        .barUnderlinePos = 7,  // remove [F]loor
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_DIG,
        .canDrag = true,
        .canErase = true
    },
    
    // WORK > DIG > REMOVE RAMP - remove ramp[Z]
    {
        .action = ACTION_WORK_REMOVE_RAMP,
        .name = "REMOVE RAMP",
        .barText = "L-click designate  R-click cancel  [ESC]Back",
        .barKey = 'z',
        .barUnderlinePos = 11,  // remove ramp[Z]
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_DIG,
        .canDrag = false,
        .canErase = true
    },
    
    // ========================================================================
    // MODE_WORK > SUBMODE_BUILD actions
    // ========================================================================
    
    // WORK > BUILD > WALL - [W]all
    {
        .action = ACTION_WORK_CONSTRUCT,
        .name = "WALL",
        .barText = "L-drag designate  R-drag cancel  [ESC]Back",
        .barKey = 'w',
        .barUnderlinePos = 0,  // [W]all
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .canDrag = true,
        .canErase = true
    },
    
    // WORK > BUILD > FLOOR - [F]loor
    {
        .action = ACTION_WORK_FLOOR,
        .name = "FLOOR",
        .barText = "L-drag designate  R-drag cancel  [ESC]Back",
        .barKey = 'f',
        .barUnderlinePos = 0,  // [F]loor
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .canDrag = true,
        .canErase = true
    },
    
    // WORK > BUILD > LADDER - [L]adder
    {
        .action = ACTION_WORK_LADDER,
        .name = "LADDER",
        .barText = "L-drag designate  R-drag cancel  [ESC]Back",
        .barKey = 'l',
        .barUnderlinePos = 0,  // [L]adder
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .canDrag = true,
        .canErase = true
    },
    
    // WORK > BUILD > RAMP - [R]amp
    {
        .action = ACTION_WORK_RAMP,
        .name = "RAMP",
        .barText = "L-click designate  R-click cancel  [ESC]Back",
        .barKey = 'r',
        .barUnderlinePos = 0,  // [R]amp
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_BUILD,
        .canDrag = false,
        .canErase = true
    },
    
    // ========================================================================
    // MODE_WORK > SUBMODE_HARVEST actions
    // ========================================================================
    
    // WORK > HARVEST > CHOP TREE - [C]hop tree
    {
        .action = ACTION_WORK_CHOP,
        .name = "CHOP TREE",
        .barText = "L-click designate  R-click cancel  [ESC]Back",
        .barKey = 'c',
        .barUnderlinePos = 0,  // [C]hop tree
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_HARVEST,
        .canDrag = false,
        .canErase = true
    },
    
    // WORK > HARVEST > CHOP FELLED - chop [F]elled
    {
        .action = ACTION_WORK_CHOP_FELLED,
        .name = "CHOP FELLED",
        .barText = "L-click designate  R-click cancel  [ESC]Back",
        .barKey = 'f',
        .barUnderlinePos = 5,  // chop [F]elled
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_HARVEST,
        .canDrag = false,
        .canErase = true
    },
    
    // WORK > HARVEST > GATHER SAPLING - gather [S]apling
    {
        .action = ACTION_WORK_GATHER_SAPLING,
        .name = "GATHER SAPLING",
        .barText = "L-click designate  R-click cancel  [ESC]Back",
        .barKey = 's',
        .barUnderlinePos = 7,  // gather [S]apling
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_HARVEST,
        .canDrag = false,
        .canErase = true
    },
    
    // WORK > HARVEST > GATHER GRASS - gather [G]rass
    {
        .action = ACTION_WORK_GATHER_GRASS,
        .name = "GATHER GRASS",
        .barText = "L-click designate  R-click cancel  [ESC]Back",
        .barKey = 'g',
        .barUnderlinePos = 7,  // gather [G]rass
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_HARVEST,
        .canDrag = true,
        .canErase = true
    },

    // WORK > HARVEST > GATHER TREE - gather [T]ree
    {
        .action = ACTION_WORK_GATHER_TREE,
        .name = "GATHER TREE",
        .barText = "L-click designate  R-click cancel  [ESC]Back",
        .barKey = 't',
        .barUnderlinePos = 7,  // gather [T]ree
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_HARVEST,
        .canDrag = true,
        .canErase = true
    },
    
    // WORK > HARVEST > PLANT SAPLING - [P]lant sapling
    {
        .action = ACTION_WORK_PLANT_SAPLING,
        .name = "PLANT SAPLING",
        .barText = "L-click designate  R-click cancel  [ESC]Back",
        .barKey = 'p',
        .barUnderlinePos = 0,  // [P]lant sapling
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_HARVEST,
        .canDrag = false,
        .canErase = true
    },
    
    // ========================================================================
    // MODE_WORK > SUBMODE_NONE (top-level WORK actions)
    // ========================================================================
    
    // WORK > CLEAN - [C]lean
    {
        .action = ACTION_WORK_CLEAN,
        .name = "CLEAN",
        .barText = "L-drag designate  R-drag cancel  [ESC]Back",
        .barKey = 'c',
        .barUnderlinePos = 0,  // [C]lean
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_NONE,
        .canDrag = true,
        .canErase = true
    },
    
    // WORK > GATHER - [G]ather
    {
        .action = ACTION_WORK_GATHER,
        .name = "GATHER",
        .barText = "L-drag create zone  R-drag delete zone  [ESC]Back",
        .barKey = 'g',
        .barUnderlinePos = 0,  // [G]ather
        .requiredMode = MODE_WORK,
        .requiredSubMode = SUBMODE_NONE,
        .canDrag = true,
        .canErase = true
    },
    
    // ========================================================================
    // MODE_SANDBOX actions
    // ========================================================================
    
    // SANDBOX > WATER - [W]ater
    {
        .action = ACTION_SANDBOX_WATER,
        .name = "WATER",
        .barText = "L-drag add  R-drag remove  [ESC]Back",
        .barKey = 'w',
        .barUnderlinePos = 0,  // [W]ater
        .requiredMode = MODE_SANDBOX,
        .requiredSubMode = SUBMODE_NONE,
        .canDrag = true,
        .canErase = true
    },
    
    // SANDBOX > FIRE - [F]ire
    {
        .action = ACTION_SANDBOX_FIRE,
        .name = "FIRE",
        .barText = "L-drag ignite  R-drag extinguish  [ESC]Back",
        .barKey = 'f',
        .barUnderlinePos = 0,  // [F]ire
        .requiredMode = MODE_SANDBOX,
        .requiredSubMode = SUBMODE_NONE,
        .canDrag = true,
        .canErase = true
    },
    
    // SANDBOX > HEAT - [H]eat
    {
        .action = ACTION_SANDBOX_HEAT,
        .name = "HEAT",
        .barText = "L-drag heat  R-drag cool  [ESC]Back",
        .barKey = 'h',
        .barUnderlinePos = 0,  // [H]eat
        .requiredMode = MODE_SANDBOX,
        .requiredSubMode = SUBMODE_NONE,
        .canDrag = true,
        .canErase = true
    },
    
    // SANDBOX > COLD - [C]old
    {
        .action = ACTION_SANDBOX_COLD,
        .name = "COLD",
        .barText = "L-drag cool  R-drag heat  [ESC]Back",
        .barKey = 'c',
        .barUnderlinePos = 0,  // [C]old
        .requiredMode = MODE_SANDBOX,
        .requiredSubMode = SUBMODE_NONE,
        .canDrag = true,
        .canErase = true
    },
    
    // SANDBOX > SMOKE - s[M]oke
    {
        .action = ACTION_SANDBOX_SMOKE,
        .name = "SMOKE",
        .barText = "L-drag add  R-drag remove  [ESC]Back",
        .barKey = 'm',
        .barUnderlinePos = 1,  // s[M]oke
        .requiredMode = MODE_SANDBOX,
        .requiredSubMode = SUBMODE_NONE,
        .canDrag = true,
        .canErase = true
    },
    
    // SANDBOX > STEAM - s[T]eam
    {
        .action = ACTION_SANDBOX_STEAM,
        .name = "STEAM",
        .barText = "L-drag add  R-drag remove  [ESC]Back",
        .barKey = 't',
        .barUnderlinePos = 1,  // s[T]eam
        .requiredMode = MODE_SANDBOX,
        .requiredSubMode = SUBMODE_NONE,
        .canDrag = true,
        .canErase = true
    },
    
    // SANDBOX > GRASS - [G]rass
    {
        .action = ACTION_SANDBOX_GRASS,
        .name = "GRASS",
        .barText = "L-drag grow  R-drag trample  [ESC]Back",
        .barKey = 'g',
        .barUnderlinePos = 0,  // [G]rass
        .requiredMode = MODE_SANDBOX,
        .requiredSubMode = SUBMODE_NONE,
        .canDrag = true,
        .canErase = true
    },
    
    // SANDBOX > TREE - t[R]ee
    {
        .action = ACTION_SANDBOX_TREE,
        .name = "TREE",
        .barText = "L-click place  R-click remove  [ESC]Back",
        .barKey = 'r',
        .barUnderlinePos = 1,  // t[R]ee
        .requiredMode = MODE_SANDBOX,
        .requiredSubMode = SUBMODE_NONE,
        .canDrag = false,
        .canErase = true
    },
    {
        .action = ACTION_SANDBOX_SCULPT,
        .name = "SCULPT",
        .barText = "L-drag raise  R-drag lower  hold [S]mooth  [1-4]Brush  [ESC]Back",
        .barKey = 'c',
        .barUnderlinePos = 1,  // s[c]ulpt
        .requiredMode = MODE_SANDBOX,
        .requiredSubMode = SUBMODE_NONE,
        .canDrag = true,
        .canErase = true
    },
    {
        .action = ACTION_SANDBOX_LOWER,
        .name = "LOWER",
        .barText = "Unused - sculpt uses mouse buttons",
        .barKey = 'l',
        .barUnderlinePos = 0,
        .requiredMode = MODE_SANDBOX,
        .requiredSubMode = SUBMODE_NONE,
        .canDrag = false,
        .canErase = false
    },
    {
        .action = ACTION_SANDBOX_RAISE,
        .name = "RAISE",
        .barText = "Unused - sculpt uses mouse buttons",
        .barKey = 'r',
        .barUnderlinePos = 0,
        .requiredMode = MODE_SANDBOX,
        .requiredSubMode = SUBMODE_NONE,
        .canDrag = false,
        .canErase = false
    }
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
