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
    // === VEGETATION ===
    [CELL_SAPLING]     = {"sapling",     SPRITE_tree_sapling_oak, 0,                   INSULATION_TIER_AIR,    0, CELL_AIR,         ITEM_NONE, 0},  // Walkable, grows into trunk (rendering uses tree type)
    [CELL_TREE_TRUNK]  = {"tree trunk",  SPRITE_tree_trunk_oak,  CF_BLOCKS_MOVEMENT | CF_SOLID, INSULATION_TIER_WOOD, 64, CELL_AIR,  ITEM_LOG, 1},     // Main trunk column - solid, choppable, burns (rendering uses tree type)
    [CELL_TREE_BRANCH] = {"tree branch", SPRITE_tree_branch_oak,  CF_BLOCKS_MOVEMENT | CF_SOLID, INSULATION_TIER_WOOD, 64, CELL_AIR,  ITEM_POLES, 1},     // Branch - solid, choppable, burns (rendering uses tree type)
    [CELL_TREE_ROOT]   = {"tree root",   SPRITE_tree_trunk_oak,  CF_BLOCKS_MOVEMENT | CF_SOLID, INSULATION_TIER_WOOD, 32, CELL_AIR,  ITEM_NONE, 0},    // Root - solid, darker sprite (rendering uses tree type)
    [CELL_TREE_FELLED] = {"felled log",  SPRITE_tree_trunk_oak,  0,                    INSULATION_TIER_WOOD,   64, CELL_AIR,         ITEM_LOG, 1},     // Felled - walkable, can be hauled (rendering uses tree type)
    [CELL_TREE_LEAVES] = {"tree leaves", SPRITE_tree_leaves_oak, 0,                    INSULATION_TIER_AIR,   32, CELL_AIR,         ITEM_NONE, 0},     // Canopy - doesn't block ground movement (rendering uses tree type)
    // === GROUND VEGETATION ===
    [CELL_BUSH]        = {"bush",        SPRITE_bush,            0,                    INSULATION_TIER_AIR,    0, CELL_AIR,         ITEM_NONE, 0},     // Walkable ground vegetation - slows movement (future: variable terrain cost)
    // === TRANSPORT ===
    [CELL_TRACK]       = {"track",       SPRITE_track_isolated,  0,                    INSULATION_TIER_AIR,    0, CELL_AIR,         ITEM_NONE, 0},     // Train track - autotiled based on neighbors
    // === DOORS ===
    [CELL_DOOR]        = {"door",        SPRITE_door,            CF_BLOCKS_FLUIDS|CF_SOLID, INSULATION_TIER_WOOD, 64, CELL_AIR, ITEM_NONE, 0},  // Walkable, blocks fluids/light, solid for support, burns
};

// 4-bit cardinal bitmask: N=1, E=2, S=4, W=8
static bool IsTrackAt(int x, int y, int z) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight || z < 0 || z >= gridDepth)
        return false;
    return grid[z][y][x] == CELL_TRACK;
}

int GetTrackSpriteAt(int x, int y, int z) {
    int mask = 0;
    if (IsTrackAt(x, y - 1, z)) mask |= 1;  // N
    if (IsTrackAt(x + 1, y, z)) mask |= 2;  // E
    if (IsTrackAt(x, y + 1, z)) mask |= 4;  // S
    if (IsTrackAt(x - 1, y, z)) mask |= 8;  // W

    static const int trackSprites[16] = {
        [0]  = SPRITE_track_isolated,  // none
        [1]  = SPRITE_track_isolated,  // N only (end cap)
        [2]  = SPRITE_track_isolated,  // E only (end cap)
        [3]  = SPRITE_track_ne,        // N+E
        [4]  = SPRITE_track_isolated,  // S only (end cap)
        [5]  = SPRITE_track_ns,        // N+S
        [6]  = SPRITE_track_se,        // S+E
        [7]  = SPRITE_track_nse,       // N+E+S (T-junction)
        [8]  = SPRITE_track_isolated,  // W only (end cap)
        [9]  = SPRITE_track_nw,        // N+W
        [10] = SPRITE_track_ew,        // E+W
        [11] = SPRITE_track_new,       // N+E+W (T-junction)
        [12] = SPRITE_track_sw,        // S+W
        [13] = SPRITE_track_nsw,       // N+S+W (T-junction)
        [14] = SPRITE_track_sew,       // E+S+W (T-junction)
        [15] = SPRITE_track_nsew,      // all (crossroads)
    };

    return trackSprites[mask];
}
