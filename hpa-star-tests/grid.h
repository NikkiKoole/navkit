#ifndef GRID_H
#define GRID_H

#include <stdbool.h>

#define GRID_WIDTH  (128*4)
#define GRID_HEIGHT (128*4)
#define CHUNK_SIZE  32

typedef enum { CELL_WALKABLE, CELL_WALL } CellType;

extern CellType grid[GRID_HEIGHT][GRID_WIDTH];
extern bool needsRebuild;

#endif // GRID_H
