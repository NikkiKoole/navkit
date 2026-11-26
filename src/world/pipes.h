// pipes.h - the pipe layer: one byte per cell, the vocabulary agreed with dreamengine
// (docs/design/nk-plumbing.md there, runtime/nkbuild/nav.h NKP_PIPE_*: keep the bits equal).
// Pipes take no cell of their own: walking, rooms and the pathfinder never see them.
//
// Spike (2026-11-26, docs/todo/world/infrastructure.md step 4): drains only, laid by console
// (`pipe`), not saved, no items. A network pass after every change says which pipe cells reach
// an outlet; a toilet flushes only when its drain does (in worlds that have drain pipes).
#ifndef PIPES_H
#define PIPES_H

#include <stdbool.h>
#include <stdint.h>

enum {
    PIPE_FLOOR_SUPPLY = 1 << 0,   // a water pipe in this cell's floor
    PIPE_FLOOR_DRAIN  = 1 << 1,   // a drain in this cell's floor
    PIPE_WALL_SUPPLY  = 1 << 2,   // a water pipe in this wall cell
    PIPE_WALL_DRAIN   = 1 << 3,   // a drain in this wall cell
    PIPE_STACK        = 1 << 4,   // the wall pipes here run on to the STACK cells above and below
    PIPE_END          = 1 << 5,   // meets the outside: supply = an intake, drain = an outlet
    PIPE_VALVE        = 1 << 6,   // a stopcock on the branches leaving this cell
    PIPE_VALVE_CLOSED = 1 << 7,   // ... and it is shut
};
#define PIPE_SUPPLY (PIPE_FLOOR_SUPPLY | PIPE_WALL_SUPPLY)
#define PIPE_DRAIN  (PIPE_FLOOR_DRAIN | PIPE_WALL_DRAIN)

extern uint8_t* pipeGrid;       // the pipe byte per cell (allocated in grid_layers.c)
extern uint8_t* pipeConnGrid;   // the network pass's answer per cell: PIPE_CONN_* (not saved)
#define PIPE(x, y, z)      (pipeGrid CELL_AT((x), (y), (z)))
#define PIPE_CONN(x, y, z) (pipeConnGrid CELL_AT((x), (y), (z)))
enum { PIPE_CONN_SUPPLY = 1 << 0, PIPE_CONN_DRAIN = 1 << 1 };
extern bool pipeOverlay;        // setting pipeOverlay: pipes under a constructed floor drawn too

void ClearPipes(void);                                 // a new world: no pipes
uint8_t GetPipe(int x, int y, int z);                  // 0 out of bounds
void SetPipe(int x, int y, int z, uint8_t bits);       // replaces the byte; the network is redone
int  PipeDrainCellCount(void);                         // cells with a drain bit: 0 = no drains here
bool PipeDrainConnected(int x, int y, int z);          // this pipe cell's drain reaches an outlet
// A fixture at (x, y, z) is on the drain if its own floor, or a wall cell next to it, carries a
// drain that reaches an outlet
bool PipeFixtureOnDrain(int x, int y, int z);
// Lay a floor drain from (x1, y1) to (x2, y2) on level z: along x first, then along y (an L)
int  LayFloorDrain(int x1, int y1, int x2, int y2, int z);

#endif
