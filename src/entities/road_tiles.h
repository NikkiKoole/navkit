// Road tiles: roads drawn (or generated) on a grid of RT_SIZE x RT_SIZE cells, SimCity-like,
// instead of placing junction nodes by hand. Each tile is empty or a piece of road of a class;
// connections come from the neighbouring tiles. RoadTilesBuild turns the tiles into the road
// network (roads.c): a junction or a bend wherever a road tile doesn't just go straight on (or
// the class changes), a road along each straight run, a roundabout on a roundabout tile.
//
// One tile holds a two-lane road with its pavements (2-cell lanes + 2 cells each side) or a
// highway's four lanes. Not saved (an experiment, like the rest of the road network).
#ifndef ROAD_TILES_H
#define ROAD_TILES_H

#include <stdbool.h>

#define RT_SIZE 8   // cells per tile side
#define RT_RING_MIN 9.0f   // cells: a roundabout tile's ring radius (at least)

typedef enum {
    RT_NONE,
    RT_STREET,       // 2 lanes, street class
    RT_ARTERIAL,     // 2 lanes, arterial class (faster; lights at its crossings)
    RT_HIGHWAY,      // 2 lanes each way, highway class
    RT_ROUNDABOUT,   // a junction tile that becomes a roundabout (arterial class)
    RT_KIND_COUNT
} RoadTileKind;

void RoadTilesClear(int wTiles, int hTiles);
void RoadTileSet(int tx, int ty, int kind);
int RoadTileAt(int tx, int ty);               // RT_NONE outside
int RoadTilesW(void);
int RoadTilesH(void);
// Text rows, one character per tile: '.' none, 's' street, 'a' arterial, 'h' highway,
// 'O' roundabout; returns the road tiles set
int RoadTilesFromText(const char* const* rows, int nRows);
// The road network from the tiles (RoadsClear first); returns the junctions + bends made.
// Then rdClasses / RoadsBuild as for any network.
int RoadTilesBuild(void);

#endif
