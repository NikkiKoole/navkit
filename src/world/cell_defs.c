#include "cell_defs.h"
#include "../../assets/atlas.h"

CellDef cellDefs[] = {
    // === GROUND TYPES ===
    [CELL_GRASS]       = {"grass",       SPRITE_grass,       CF_GROUND,            INSULATION_TIER_AIR,   16, CELL_DIRT},
    [CELL_DIRT]        = {"dirt",        SPRITE_dirt,        CF_GROUND,            INSULATION_TIER_AIR,    1, CELL_DIRT},
    [CELL_FLOOR]       = {"floor",       SPRITE_floor,       CF_WALKABLE,          INSULATION_TIER_WOOD,   0, CELL_FLOOR},
    
    // === WALLS ===
    [CELL_WALL]        = {"stone wall",  SPRITE_wall,        CF_WALL,              INSULATION_TIER_STONE,  0, CELL_WALL},
    [CELL_WOOD_WALL]   = {"wood wall",   SPRITE_wood_wall,   CF_WALL,              INSULATION_TIER_WOOD, 128, CELL_FLOOR},
    
    // === VERTICAL MOVEMENT ===
    // Ladders are walkable via CF_LADDER check, not CF_SOLID (so they don't render as floor in DF mode)
    [CELL_LADDER_UP]   = {"ladder up",   SPRITE_ladder_up,   CF_WALKABLE|CF_LADDER,  INSULATION_TIER_AIR,    0, CELL_LADDER_UP},
    [CELL_LADDER_DOWN] = {"ladder down", SPRITE_ladder_down, CF_WALKABLE|CF_LADDER,  INSULATION_TIER_AIR,    0, CELL_LADDER_DOWN},
    [CELL_LADDER_BOTH] = {"ladder",      SPRITE_ladder,      CF_WALKABLE|CF_LADDER,  INSULATION_TIER_AIR,    0, CELL_LADDER_BOTH},
    
    // === SPECIAL ===
    [CELL_AIR]         = {"air",         SPRITE_air,         0,                    INSULATION_TIER_AIR,    0, CELL_AIR},
    [CELL_BEDROCK]     = {"bedrock",     SPRITE_bedrock,     CF_SOLID | CF_BLOCKS_MOVEMENT | CF_BLOCKS_FLUIDS, INSULATION_TIER_STONE, 0, CELL_BEDROCK},
    
    // === LEGACY ALIASES ===
    [CELL_WALKABLE]    = {"grass",       SPRITE_grass,       CF_GROUND,            INSULATION_TIER_AIR,   16, CELL_DIRT},
    [CELL_LADDER]      = {"ladder",      SPRITE_ladder,      CF_WALKABLE|CF_LADDER,  INSULATION_TIER_AIR,    0, CELL_LADDER},
};
