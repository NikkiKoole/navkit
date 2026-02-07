#include "cell_defs.h"
#include "../../assets/atlas.h"

// CellDef fields: name, sprite, flags, insulationTier, fuel, burnsInto, dropsItem, dropCount
CellDef cellDefs[] = {
    // === WALLS ===
    [CELL_WALL]        = {"wall",        SPRITE_wall,        CF_WALL,              INSULATION_TIER_STONE,  0, CELL_WALL,        ITEM_ROCK, 1},  // Base wall - material determines drops/flammability
    
    // === VERTICAL MOVEMENT ===
    // Ladders are walkable via CF_LADDER check, not CF_SOLID (so they don't render as floor in DF mode)
    [CELL_LADDER_UP]   = {"ladder up",   SPRITE_ladder_up,   CF_WALKABLE|CF_LADDER,  INSULATION_TIER_AIR,    0, CELL_LADDER_UP,   ITEM_BLOCKS, 1},
    [CELL_LADDER_DOWN] = {"ladder down", SPRITE_ladder_down, CF_WALKABLE|CF_LADDER,  INSULATION_TIER_AIR,    0, CELL_LADDER_DOWN, ITEM_BLOCKS, 1},
    [CELL_LADDER_BOTH] = {"ladder",      SPRITE_ladder,      CF_WALKABLE|CF_LADDER,  INSULATION_TIER_AIR,    0, CELL_LADDER_BOTH, ITEM_BLOCKS, 1},
    // Ramps are walkable via CF_RAMP check, directional z-transitions
    [CELL_RAMP_N]      = {"ramp north",  SPRITE_ramp_n,      CF_RAMP,                INSULATION_TIER_STONE,  0, CELL_RAMP_N,      ITEM_ROCK, 1},
    [CELL_RAMP_E]      = {"ramp east",   SPRITE_ramp_e,      CF_RAMP,                INSULATION_TIER_STONE,  0, CELL_RAMP_E,      ITEM_ROCK, 1},
    [CELL_RAMP_S]      = {"ramp south",  SPRITE_ramp_s,      CF_RAMP,                INSULATION_TIER_STONE,  0, CELL_RAMP_S,      ITEM_ROCK, 1},
    [CELL_RAMP_W]      = {"ramp west",   SPRITE_ramp_w,      CF_RAMP,                INSULATION_TIER_STONE,  0, CELL_RAMP_W,      ITEM_ROCK, 1},
    
    // === SPECIAL ===
    [CELL_AIR]         = {"air",         SPRITE_air,         0,                    INSULATION_TIER_AIR,    0, CELL_AIR,         ITEM_NONE, 0},
    [CELL_TERRAIN]     = {"terrain",     SPRITE_dirt,        CF_GROUND | CF_BLOCKS_FLUIDS, INSULATION_TIER_AIR,    0, CELL_TERRAIN, ITEM_NONE, 0},  // Generic natural terrain - material determines properties
    
    // === VEGETATION ===
    [CELL_SAPLING]     = {"sapling",     SPRITE_tree_sapling_oak, 0,                   INSULATION_TIER_AIR,    0, CELL_AIR,         ITEM_NONE, 0},  // Walkable, grows into trunk (rendering uses tree type)
    [CELL_TREE_TRUNK]  = {"tree trunk",  SPRITE_tree_trunk_oak,  CF_BLOCKS_MOVEMENT | CF_SOLID, INSULATION_TIER_WOOD, 64, CELL_AIR,  ITEM_LOG, 1},     // Main trunk column - solid, choppable, burns (rendering uses tree type)
    [CELL_TREE_BRANCH] = {"tree branch", SPRITE_tree_trunk_oak,  CF_BLOCKS_MOVEMENT | CF_SOLID, INSULATION_TIER_WOOD, 64, CELL_AIR,  ITEM_LOG, 1},     // Branch - solid, choppable, burns (rendering uses tree type)
    [CELL_TREE_ROOT]   = {"tree root",   SPRITE_tree_trunk_oak,  CF_BLOCKS_MOVEMENT | CF_SOLID, INSULATION_TIER_WOOD, 32, CELL_AIR,  ITEM_NONE, 0},    // Root - solid, darker sprite (rendering uses tree type)
    [CELL_TREE_FELLED] = {"felled log",  SPRITE_tree_trunk_oak,  0,                    INSULATION_TIER_WOOD,   64, CELL_AIR,         ITEM_LOG, 1},     // Felled - walkable, can be hauled (rendering uses tree type)
    [CELL_TREE_LEAVES] = {"tree leaves", SPRITE_tree_leaves_oak, 0,                    INSULATION_TIER_AIR,   32, CELL_AIR,         ITEM_NONE, 0},     // Canopy - doesn't block ground movement (rendering uses tree type)
};
