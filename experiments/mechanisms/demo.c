// Mechanisms & Signals Sandbox Demo
// A learning environment for signal-based automation:
//   Switch (source) -> Wire -> Logic Gates -> Wire -> Light (sink)
//   + Processor (tiny emulator with 6 opcodes)
//
// Controls:
//   1-9,0  Select component / eraser
//   LMB    Place component (click switch to toggle)
//   RMB    Remove component
//   R      Rotate (gates/processor facing)
//   P      Open processor editor on hovered processor
//   Space  Pause/resume simulation
//   T      Single tick step (when paused)
//   C      Clear grid

#include "../../vendor/raylib.h"
#include "../../assets/fonts/comic_embedded.h"
#define UI_IMPLEMENTATION
#include "../../shared/ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
#define SCREEN_WIDTH  1280
#define SCREEN_HEIGHT 800
#define GRID_W        32
#define GRID_H        32
#define CELL_SIZE     20
#define GRID_OFFSET_X 40
#define GRID_OFFSET_Y 40

#define MAX_PROCESSORS 64
#define MAX_PROG_LEN   16
#define TICK_INTERVAL  0.1f   // 10 ticks/sec

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------
typedef enum {
    COMP_EMPTY = 0,
    COMP_SWITCH,    // Source: click to toggle on/off, emits signal to adjacent wire
    COMP_BUTTON,    // Source: momentary, ON while mouse held down, OFF on release
    COMP_LIGHT,     // Sink: turns on when receiving signal from adjacent wire
    COMP_WIRE,
    COMP_NOT,
    COMP_AND,
    COMP_OR,
    COMP_XOR,       // Logic: ON when inputs differ
    COMP_NOR,       // Logic: ON only when both inputs are OFF
    COMP_LATCH,     // Memory: SET (right side) turns ON, RESET (left side) turns OFF
    COMP_PROCESSOR,
    COMP_CLOCK,     // Source: auto-toggles every N ticks (click to change speed 1-8)
    COMP_REPEATER,  // Directional: delays signal 1-4 ticks, one-way (diode)
    COMP_COUNT
} ComponentType;

typedef enum {
    DIR_NORTH = 0,
    DIR_EAST,
    DIR_SOUTH,
    DIR_WEST
} Direction;

typedef enum {
    OP_NOP = 0,
    OP_READ,    // READ portA -> regA
    OP_WRITE,   // WRITE regA -> portA
    OP_SET,     // SET value -> regA
    OP_ADD,     // ADD regA + regB -> regA
    OP_CMP,     // CMP regA vs regB -> flag
    OP_JIF,     // JIF line (jump if flag)
    OP_COUNT
} OpCode;

typedef enum {
    MODE_PLACE = 0,
    MODE_PROC_EDIT
} InteractionMode;

// ---------------------------------------------------------------------------
// Structures
// ---------------------------------------------------------------------------
typedef struct {
    ComponentType type;
    Direction facing;
    bool state;         // on/off for switch, on/off for light
    int signalIn;       // signal read from wire (0 or 1)
    int signalOut;      // signal written to wire (0 or 1)
    int procIdx;        // index into processors[] (-1 if none)
    int setting;        // clock: period (1-8 ticks), repeater: delay (1-4 ticks)
    int timer;          // clock: ticks until toggle, repeater: delay buffer index
    int delayBuf[4];    // repeater: circular buffer of delayed signal values
} Cell;

typedef struct {
    OpCode op;
    int argA, argB, argC;   // meaning depends on opcode
} Instruction;

typedef struct {
    int x, y;               // grid position
    int regs[4];
    int pc;
    bool flag;
    Instruction program[MAX_PROG_LEN];
    int progLen;
    bool active;
} Processor;

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static Cell grid[GRID_H][GRID_W];
static int signalGrid[2][GRID_H][GRID_W];  // double-buffered
static int sigRead = 0, sigWrite = 1;

static Processor processors[MAX_PROCESSORS];
static int processorCount = 0;

static ComponentType selectedComp = COMP_SWITCH;
static Direction placingDir = DIR_NORTH;
static InteractionMode mode = MODE_PLACE;
static bool simPaused = false;
static float tickTimer = 0.0f;

// Processor editor state
static int editProcIdx = -1;
static int editLine = 0;
static int editField = 0;  // 0=opcode, 1=argA, 2=argB, 3=argC

// Signal animation (pulse glow)
static float pulseTime = 0.0f;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static const char *CompName(ComponentType t) {
    switch (t) {
        case COMP_EMPTY:      return "Eraser";
        case COMP_SWITCH:     return "Switch";
        case COMP_BUTTON:     return "Button";
        case COMP_LIGHT:      return "Light";
        case COMP_WIRE:       return "Wire";
        case COMP_NOT:        return "NOT";
        case COMP_AND:        return "AND";
        case COMP_OR:         return "OR";
        case COMP_XOR:        return "XOR";
        case COMP_NOR:        return "NOR";
        case COMP_LATCH:      return "Latch";
        case COMP_PROCESSOR:  return "Processor";
        case COMP_CLOCK:      return "Clock";
        case COMP_REPEATER:   return "Repeater";
        default:              return "?";
    }
}

static const char *OpName(OpCode op) {
    switch (op) {
        case OP_NOP:   return "NOP";
        case OP_READ:  return "READ";
        case OP_WRITE: return "WRITE";
        case OP_SET:   return "SET";
        case OP_ADD:   return "ADD";
        case OP_CMP:   return "CMP";
        case OP_JIF:   return "JIF";
        default:       return "?";
    }
}

static const char *DirName(Direction d) {
    switch (d) {
        case DIR_NORTH: return "N";
        case DIR_EAST:  return "E";
        case DIR_SOUTH: return "S";
        case DIR_WEST:  return "W";
        default: return "?";
    }
}

static bool InGrid(int x, int y) {
    return x >= 0 && x < GRID_W && y >= 0 && y < GRID_H;
}

static void GridFromScreen(int sx, int sy, int *gx, int *gy) {
    *gx = (sx - GRID_OFFSET_X) / CELL_SIZE;
    *gy = (sy - GRID_OFFSET_Y) / CELL_SIZE;
}

static Rectangle CellRect(int gx, int gy) {
    return (Rectangle){
        GRID_OFFSET_X + gx * CELL_SIZE,
        GRID_OFFSET_Y + gy * CELL_SIZE,
        CELL_SIZE, CELL_SIZE
    };
}

// Direction offsets: N=up(-y), E=right(+x), S=down(+y), W=left(-x)
static void DirOffset(Direction d, int *dx, int *dy) {
    *dx = 0; *dy = 0;
    switch (d) {
        case DIR_NORTH: *dy = -1; break;
        case DIR_EAST:  *dx =  1; break;
        case DIR_SOUTH: *dy =  1; break;
        case DIR_WEST:  *dx = -1; break;
    }
}

static Direction OppositeDir(Direction d) {
    return (Direction)((d + 2) % 4);
}

// Get the two input directions for AND/OR (sides perpendicular to facing)
static void GateInputDirs(Direction facing, Direction *inA, Direction *inB) {
    *inA = (Direction)((facing + 1) % 4);  // right of facing
    *inB = (Direction)((facing + 3) % 4);  // left of facing
}

// ---------------------------------------------------------------------------
// Processor management
// ---------------------------------------------------------------------------
static int FindProcessor(int gx, int gy) {
    for (int i = 0; i < processorCount; i++) {
        if (processors[i].active && processors[i].x == gx && processors[i].y == gy)
            return i;
    }
    return -1;
}

static int CreateProcessor(int gx, int gy) {
    for (int i = 0; i < MAX_PROCESSORS; i++) {
        if (!processors[i].active) {
            memset(&processors[i], 0, sizeof(Processor));
            processors[i].x = gx;
            processors[i].y = gy;
            processors[i].active = true;
            processors[i].progLen = 1; // start with 1 NOP line
            if (i >= processorCount) processorCount = i + 1;
            return i;
        }
    }
    return -1;
}

static void RemoveProcessor(int gx, int gy) {
    int idx = FindProcessor(gx, gy);
    if (idx >= 0) {
        processors[idx].active = false;
        while (processorCount > 0 && !processors[processorCount - 1].active)
            processorCount--;
    }
}

// ---------------------------------------------------------------------------
// Grid operations
// ---------------------------------------------------------------------------
static void PlaceComponent(int gx, int gy, ComponentType type) {
    if (!InGrid(gx, gy)) return;

    // Remove existing
    if (grid[gy][gx].type == COMP_PROCESSOR) {
        RemoveProcessor(gx, gy);
    }

    memset(&grid[gy][gx], 0, sizeof(Cell));
    grid[gy][gx].procIdx = -1;

    if (type == COMP_EMPTY) return;

    grid[gy][gx].type = type;
    grid[gy][gx].facing = placingDir;

    if (type == COMP_PROCESSOR) {
        int pi = CreateProcessor(gx, gy);
        grid[gy][gx].procIdx = pi;
    }
    if (type == COMP_CLOCK) {
        grid[gy][gx].setting = 4;  // default period: 4 ticks
        grid[gy][gx].timer = 4;
    }
    if (type == COMP_REPEATER) {
        grid[gy][gx].setting = 1;  // default delay: 1 tick
    }
}

static void ClearGrid(void) {
    memset(grid, 0, sizeof(grid));
    memset(signalGrid, 0, sizeof(signalGrid));
    memset(processors, 0, sizeof(processors));
    processorCount = 0;
    for (int y = 0; y < GRID_H; y++)
        for (int x = 0; x < GRID_W; x++)
            grid[y][x].procIdx = -1;
}

// ---------------------------------------------------------------------------
// Presets: stamp pre-built circuits onto the grid
// ---------------------------------------------------------------------------
static void PlaceAt(int gx, int gy, ComponentType type, Direction dir) {
    if (!InGrid(gx, gy)) return;
    Direction savedDir = placingDir;
    placingDir = dir;
    PlaceComponent(gx, gy, type);
    placingDir = savedDir;
}

static void PlaceWire(int gx, int gy) {
    PlaceAt(gx, gy, COMP_WIRE, DIR_NORTH);
}

typedef struct {
    const char *name;
    const char *description;
    void (*build)(int ox, int oy);  // ox,oy = top-left origin
    int width, height;
} Preset;

// Preset 1: NOT gate demo — Switch -> NOT -> Light
static void BuildPresetNot(int ox, int oy) {
    PlaceAt(ox, oy + 1, COMP_SWITCH, DIR_NORTH);
    PlaceWire(ox + 1, oy + 1);
    PlaceWire(ox + 2, oy + 1);
    PlaceAt(ox + 3, oy + 1, COMP_NOT, DIR_EAST);
    PlaceWire(ox + 4, oy + 1);
    PlaceAt(ox + 5, oy + 1, COMP_LIGHT, DIR_EAST);
}

// Preset 2: AND gate demo — two Switches -> AND -> Light
static void BuildPresetAnd(int ox, int oy) {
    PlaceAt(ox, oy, COMP_SWITCH, DIR_NORTH);
    PlaceWire(ox + 1, oy);
    PlaceWire(ox + 2, oy);
    PlaceWire(ox + 3, oy);          // connect to AND's north input
    PlaceAt(ox + 3, oy + 1, COMP_AND, DIR_EAST);
    PlaceWire(ox + 4, oy + 1);
    PlaceAt(ox + 5, oy + 1, COMP_LIGHT, DIR_EAST);
    PlaceAt(ox, oy + 2, COMP_SWITCH, DIR_NORTH);
    PlaceWire(ox + 1, oy + 2);
    PlaceWire(ox + 2, oy + 2);
    PlaceWire(ox + 3, oy + 2);      // connect to AND's south input
}

// Preset 3: NOR latch — two NOR gates cross-connected with repeaters
static void BuildPresetNorLatch(int ox, int oy) {
    // Top Button -> NOR1
    PlaceAt(ox, oy, COMP_BUTTON, DIR_NORTH);
    PlaceWire(ox + 1, oy);
    // NOR1 at (ox+2, oy+1) facing east, top button hits its right-side input
    PlaceAt(ox + 2, oy + 1, COMP_NOR, DIR_EAST);
    PlaceWire(ox + 3, oy + 1);
    PlaceWire(ox + 4, oy + 1);
    // Light on output
    PlaceAt(ox + 5, oy + 1, COMP_LIGHT, DIR_EAST);
    // Feedback: NOR1 output down to NOR2 input via repeater
    PlaceWire(ox + 4, oy + 2);
    PlaceWire(ox + 4, oy + 3);
    PlaceAt(ox + 3, oy + 3, COMP_REPEATER, DIR_WEST);
    PlaceWire(ox + 2, oy + 3);
    // NOR2 at (ox+2, oy+4) facing east, feedback hits its right-side input (top)
    // Bottom Button -> NOR2
    PlaceAt(ox, oy + 5, COMP_BUTTON, DIR_NORTH);
    PlaceWire(ox + 1, oy + 5);
    PlaceAt(ox + 2, oy + 4, COMP_NOR, DIR_EAST);
    PlaceWire(ox + 3, oy + 4);
    PlaceWire(ox + 4, oy + 4);
    // Feedback: NOR2 output up to NOR1 input via repeater
    PlaceWire(ox + 4, oy + 5);
    PlaceWire(ox + 4, oy + 6);
    PlaceAt(ox + 3, oy + 6, COMP_REPEATER, DIR_WEST);
    PlaceWire(ox + 2, oy + 6);
    PlaceWire(ox + 2, oy + 2);
}

// Preset 4: Blinker — Clock -> Light
static void BuildPresetBlinker(int ox, int oy) {
    Cell *clk;
    PlaceAt(ox, oy + 1, COMP_CLOCK, DIR_NORTH);
    clk = &grid[oy + 1][ox];
    clk->setting = 3;
    clk->timer = 3;
    PlaceWire(ox + 1, oy + 1);
    PlaceAt(ox + 2, oy + 1, COMP_LIGHT, DIR_EAST);
}

// Preset 5: Pulse extender — Button -> Repeater(delay 4) -> Light
static void BuildPresetPulseExtend(int ox, int oy) {
    PlaceAt(ox, oy + 1, COMP_BUTTON, DIR_NORTH);
    PlaceWire(ox + 1, oy + 1);
    PlaceAt(ox + 2, oy + 1, COMP_REPEATER, DIR_EAST);
    grid[oy + 1][ox + 2].setting = 4;
    memset(grid[oy + 1][ox + 2].delayBuf, 0, sizeof(grid[oy + 1][ox + 2].delayBuf));
    PlaceWire(ox + 3, oy + 1);
    PlaceAt(ox + 4, oy + 1, COMP_LIGHT, DIR_EAST);
}

// Preset 6: XOR demo — two Switches -> XOR -> Light
static void BuildPresetXor(int ox, int oy) {
    PlaceAt(ox, oy, COMP_SWITCH, DIR_NORTH);
    PlaceWire(ox + 1, oy);
    PlaceWire(ox + 2, oy);
    PlaceWire(ox + 3, oy);          // connect to XOR's north input
    PlaceAt(ox + 3, oy + 1, COMP_XOR, DIR_EAST);
    PlaceWire(ox + 4, oy + 1);
    PlaceAt(ox + 5, oy + 1, COMP_LIGHT, DIR_EAST);
    PlaceAt(ox, oy + 2, COMP_SWITCH, DIR_NORTH);
    PlaceWire(ox + 1, oy + 2);
    PlaceWire(ox + 2, oy + 2);
    PlaceWire(ox + 3, oy + 2);      // connect to XOR's south input
}

static Preset presets[] = {
    { "NOT",          "Switch -> NOT -> Light",              BuildPresetNot,         6, 3 },
    { "AND",          "2 Switches -> AND -> Light",          BuildPresetAnd,         6, 3 },
    { "XOR",          "2 Switches -> XOR -> Light",          BuildPresetXor,         6, 3 },
    { "Blinker",      "Clock -> Light",                      BuildPresetBlinker,     3, 3 },
    { "Pulse Extend", "Button -> Repeater(4) -> Light",      BuildPresetPulseExtend, 5, 3 },
    { "NOR Latch",    "2 NOR gates, cross-feedback, memory", BuildPresetNorLatch,    6, 7 },
};
#define PRESET_COUNT (sizeof(presets) / sizeof(presets[0]))

static int selectedPreset = -1;  // -1 = not in preset mode

// ---------------------------------------------------------------------------
// Simulation: Signal propagation (flood-fill from sources)
// ---------------------------------------------------------------------------

// BFS queue for wire flood-fill
static int bfsQueueX[GRID_W * GRID_H];
static int bfsQueueY[GRID_W * GRID_H];

static void UpdateSignals(void) {
    int newSig[GRID_H][GRID_W];
    memset(newSig, 0, sizeof(newSig));

    // Phase 1: Compute gate outputs using PREVIOUS signal state, seed wires from sources
    int qHead = 0, qTail = 0;

    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            Cell *c = &grid[y][x];

            switch (c->type) {
                case COMP_SWITCH:
                case COMP_BUTTON: {
                    c->signalOut = c->state ? 1 : 0;
                    if (!c->signalOut) break;
                    // Seed adjacent wire cells
                    for (int d = 0; d < 4; d++) {
                        int dx, dy;
                        DirOffset((Direction)d, &dx, &dy);
                        int nx = x + dx, ny = y + dy;
                        if (InGrid(nx, ny) && grid[ny][nx].type == COMP_WIRE && !newSig[ny][nx]) {
                            newSig[ny][nx] = 1;
                            bfsQueueX[qTail] = nx;
                            bfsQueueY[qTail] = ny;
                            qTail++;
                        }
                    }
                    break;
                }

                case COMP_CLOCK: {
                    // Auto-toggle: decrement timer, toggle state when it hits 0
                    c->timer--;
                    if (c->timer <= 0) {
                        c->state = !c->state;
                        c->timer = c->setting;
                    }
                    c->signalOut = c->state ? 1 : 0;
                    if (!c->signalOut) break;
                    // Seed adjacent wire cells (omnidirectional like switch)
                    for (int d = 0; d < 4; d++) {
                        int dx, dy;
                        DirOffset((Direction)d, &dx, &dy);
                        int nx = x + dx, ny = y + dy;
                        if (InGrid(nx, ny) && grid[ny][nx].type == COMP_WIRE && !newSig[ny][nx]) {
                            newSig[ny][nx] = 1;
                            bfsQueueX[qTail] = nx;
                            bfsQueueY[qTail] = ny;
                            qTail++;
                        }
                    }
                    break;
                }

                case COMP_REPEATER: {
                    // Read input from back side
                    int dx, dy;
                    DirOffset(OppositeDir(c->facing), &dx, &dy);
                    int inX = x + dx, inY = y + dy;
                    int input = 0;
                    if (InGrid(inX, inY)) input = signalGrid[sigRead][inY][inX];
                    c->signalIn = input;

                    // Push into delay buffer, read from delayed position
                    int delay = c->setting;
                    if (delay < 1) delay = 1;
                    if (delay > 4) delay = 4;
                    // Shift buffer: [0] is oldest, [delay-1] is newest
                    for (int i = 0; i < delay - 1; i++) {
                        c->delayBuf[i] = c->delayBuf[i + 1];
                    }
                    c->delayBuf[delay - 1] = input;
                    int output = c->delayBuf[0];

                    c->signalOut = output;
                    c->state = (output != 0);
                    if (output) {
                        DirOffset(c->facing, &dx, &dy);
                        int outX = x + dx, outY = y + dy;
                        if (InGrid(outX, outY) && grid[outY][outX].type == COMP_WIRE && !newSig[outY][outX]) {
                            newSig[outY][outX] = 1;
                            bfsQueueX[qTail] = outX;
                            bfsQueueY[qTail] = outY;
                            qTail++;
                        }
                    }
                    break;
                }

                case COMP_NOT: {
                    int dx, dy;
                    DirOffset(OppositeDir(c->facing), &dx, &dy);
                    int inX = x + dx, inY = y + dy;
                    int input = 0;
                    if (InGrid(inX, inY)) input = signalGrid[sigRead][inY][inX];
                    int output = input ? 0 : 1;
                    c->signalIn = input;
                    c->signalOut = output;
                    if (output) {
                        DirOffset(c->facing, &dx, &dy);
                        int outX = x + dx, outY = y + dy;
                        if (InGrid(outX, outY) && grid[outY][outX].type == COMP_WIRE && !newSig[outY][outX]) {
                            newSig[outY][outX] = 1;
                            bfsQueueX[qTail] = outX;
                            bfsQueueY[qTail] = outY;
                            qTail++;
                        }
                    }
                    break;
                }

                case COMP_AND:
                case COMP_OR:
                case COMP_XOR:
                case COMP_NOR: {
                    Direction inADir, inBDir;
                    GateInputDirs(c->facing, &inADir, &inBDir);
                    int dx, dy;
                    DirOffset(inADir, &dx, &dy);
                    int inA = 0, inB = 0;
                    if (InGrid(x + dx, y + dy)) inA = signalGrid[sigRead][y + dy][x + dx];
                    DirOffset(inBDir, &dx, &dy);
                    if (InGrid(x + dx, y + dy)) inB = signalGrid[sigRead][y + dy][x + dx];

                    int output;
                    if (c->type == COMP_AND) output = (inA && inB) ? 1 : 0;
                    else if (c->type == COMP_XOR) output = (inA != inB) ? 1 : 0;
                    else if (c->type == COMP_NOR) output = (!inA && !inB) ? 1 : 0;
                    else output = (inA || inB) ? 1 : 0;

                    c->signalIn = inA | (inB << 1);
                    c->signalOut = output;
                    if (output) {
                        DirOffset(c->facing, &dx, &dy);
                        int outX = x + dx, outY = y + dy;
                        if (InGrid(outX, outY) && grid[outY][outX].type == COMP_WIRE && !newSig[outY][outX]) {
                            newSig[outY][outX] = 1;
                            bfsQueueX[qTail] = outX;
                            bfsQueueY[qTail] = outY;
                            qTail++;
                        }
                    }
                    break;
                }

                case COMP_LATCH: {
                    // SR latch: right side = SET, left side = RESET
                    Direction setDir, resetDir;
                    GateInputDirs(c->facing, &setDir, &resetDir);
                    int dx, dy;
                    DirOffset(setDir, &dx, &dy);
                    int setIn = 0, resetIn = 0;
                    if (InGrid(x + dx, y + dy)) setIn = signalGrid[sigRead][y + dy][x + dx];
                    DirOffset(resetDir, &dx, &dy);
                    if (InGrid(x + dx, y + dy)) resetIn = signalGrid[sigRead][y + dy][x + dx];

                    // SET wins over RESET if both active
                    if (setIn && !resetIn) c->state = true;
                    else if (resetIn && !setIn) c->state = false;
                    // Both on or both off: keep current state (memory!)

                    c->signalIn = setIn | (resetIn << 1);
                    c->signalOut = c->state ? 1 : 0;
                    if (c->signalOut) {
                        DirOffset(c->facing, &dx, &dy);
                        int outX = x + dx, outY = y + dy;
                        if (InGrid(outX, outY) && grid[outY][outX].type == COMP_WIRE && !newSig[outY][outX]) {
                            newSig[outY][outX] = 1;
                            bfsQueueX[qTail] = outX;
                            bfsQueueY[qTail] = outY;
                            qTail++;
                        }
                    }
                    break;
                }

                default:
                    break;
            }
        }
    }

    // Phase 2: BFS flood-fill signal through connected wires
    while (qHead < qTail) {
        int wx = bfsQueueX[qHead];
        int wy = bfsQueueY[qHead];
        qHead++;
        for (int d = 0; d < 4; d++) {
            int dx, dy;
            DirOffset((Direction)d, &dx, &dy);
            int nx = wx + dx, ny = wy + dy;
            if (InGrid(nx, ny) && grid[ny][nx].type == COMP_WIRE && !newSig[ny][nx]) {
                newSig[ny][nx] = 1;
                bfsQueueX[qTail] = nx;
                bfsQueueY[qTail] = ny;
                qTail++;
            }
        }
    }

    // Phase 3: Copy result into signal grid and update lights
    memcpy(signalGrid[sigWrite], newSig, sizeof(newSig));
    sigRead = sigWrite;
    sigWrite = 1 - sigWrite;

    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            Cell *c = &grid[y][x];
            if (c->type == COMP_LIGHT) {
                int sig = 0;
                for (int d = 0; d < 4; d++) {
                    int dx, dy;
                    DirOffset((Direction)d, &dx, &dy);
                    int nx = x + dx, ny = y + dy;
                    if (InGrid(nx, ny) && newSig[ny][nx])
                        sig = 1;
                }
                c->signalIn = sig;
                c->state = (sig != 0);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Simulation: Processor emulator
// ---------------------------------------------------------------------------
static int ProcReadPort(Processor *p, int port) {
    Direction dirs[4] = { DIR_NORTH, DIR_EAST, DIR_SOUTH, DIR_WEST };
    if (port < 0 || port > 3) return 0;
    int dx, dy;
    DirOffset(dirs[port], &dx, &dy);
    int nx = p->x + dx, ny = p->y + dy;
    if (InGrid(nx, ny)) return signalGrid[sigRead][ny][nx];
    return 0;
}

static void ProcWritePort(Processor *p, int port, int value) {
    Direction dirs[4] = { DIR_NORTH, DIR_EAST, DIR_SOUTH, DIR_WEST };
    if (port < 0 || port > 3) return;
    int dx, dy;
    DirOffset(dirs[port], &dx, &dy);
    int nx = p->x + dx, ny = p->y + dy;
    if (InGrid(nx, ny) && grid[ny][nx].type == COMP_WIRE) {
        signalGrid[sigRead][ny][nx] = value ? 1 : 0;
    }
}

static void UpdateProcessors(void) {
    for (int i = 0; i < processorCount; i++) {
        Processor *p = &processors[i];
        if (!p->active || p->progLen == 0) continue;
        if (p->pc < 0 || p->pc >= p->progLen) p->pc = 0;

        Instruction *inst = &p->program[p->pc];
        int a = inst->argA, b = inst->argB, c = inst->argC;
        (void)c;

        switch (inst->op) {
            case OP_NOP:
                break;
            case OP_READ:
                if (a >= 0 && a < 4)
                    p->regs[a] = ProcReadPort(p, b);
                break;
            case OP_WRITE:
                if (a >= 0 && a < 4)
                    ProcWritePort(p, b, p->regs[a]);
                break;
            case OP_SET:
                if (a >= 0 && a < 4)
                    p->regs[a] = b;
                break;
            case OP_ADD:
                if (a >= 0 && a < 4 && b >= 0 && b < 4)
                    p->regs[a] = p->regs[a] + p->regs[b];
                break;
            case OP_CMP:
                if (a >= 0 && a < 4 && b >= 0 && b < 4)
                    p->flag = (p->regs[a] > p->regs[b]);
                break;
            case OP_JIF:
                if (p->flag) {
                    p->pc = a;
                    if (p->pc < 0) p->pc = 0;
                    if (p->pc >= p->progLen) p->pc = 0;
                    return;
                }
                break;
            default:
                break;
        }

        p->pc++;
        if (p->pc >= p->progLen) p->pc = 0;
    }
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
static Color CompColor(ComponentType t, bool state) {
    switch (t) {
        case COMP_SWITCH:    return state ? YELLOW : (Color){120, 100, 20, 255};
        case COMP_BUTTON:    return state ? (Color){255, 100, 100, 255} : (Color){120, 40, 40, 255};
        case COMP_LIGHT:     return state ? (Color){50, 230, 50, 255} : (Color){40, 60, 40, 255};
        case COMP_WIRE:      return (Color){80, 80, 80, 255};
        case COMP_NOT:       return (Color){200, 60, 60, 255};
        case COMP_AND:       return (Color){60, 60, 200, 255};
        case COMP_OR:        return (Color){60, 180, 60, 255};
        case COMP_XOR:       return (Color){180, 60, 180, 255};
        case COMP_NOR:       return (Color){200, 100, 60, 255};
        case COMP_LATCH:     return state ? (Color){255, 220, 50, 255} : (Color){100, 85, 20, 255};
        case COMP_PROCESSOR: return (Color){140, 60, 200, 255};
        case COMP_CLOCK:     return state ? (Color){255, 160, 0, 255} : (Color){120, 70, 0, 255};
        case COMP_REPEATER:  return state ? (Color){0, 200, 200, 255} : (Color){0, 80, 80, 255};
        default:             return DARKGRAY;
    }
}

static void DrawArrow(int cx, int cy, Direction dir, Color col) {
    int s = CELL_SIZE / 2 - 2;
    int dx, dy;
    DirOffset(dir, &dx, &dy);
    int tipX = cx + dx * s;
    int tipY = cy + dy * s;
    DrawLine(cx, cy, tipX, tipY, col);
    int perpX = -dy, perpY = dx;
    DrawLine(tipX, tipY, tipX - dx * 3 + perpX * 3, tipY - dy * 3 + perpY * 3, col);
    DrawLine(tipX, tipY, tipX - dx * 3 - perpX * 3, tipY - dy * 3 - perpY * 3, col);
}

static void DrawGridBackground(void) {
    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            Rectangle r = CellRect(x, y);
            DrawRectangleRec(r, (Color){30, 30, 35, 255});
            DrawRectangleLinesEx(r, 1, (Color){50, 50, 55, 255});
        }
    }
}

static void DrawComponents(void) {
    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            Cell *c = &grid[y][x];
            if (c->type == COMP_EMPTY) continue;

            Rectangle r = CellRect(x, y);
            Color col = CompColor(c->type, c->state);
            int cx = (int)(r.x + r.width / 2);
            int cy = (int)(r.y + r.height / 2);

            switch (c->type) {
                case COMP_SWITCH:
                    DrawRectangleRec(r, col);
                    DrawTextShadow("S", cx - 4, cy - 5, 10, BLACK);
                    break;

                case COMP_BUTTON: {
                    // Rounded look: slightly smaller rect
                    Rectangle br = { r.x + 2, r.y + 2, r.width - 4, r.height - 4 };
                    DrawRectangleRounded(br, 0.4f, 4, col);
                    DrawTextShadow("B", cx - 4, cy - 5, 10, WHITE);
                    break;
                }

                case COMP_LIGHT:
                    DrawCircle(cx, cy, CELL_SIZE / 2 - 1, col);
                    if (c->state) {
                        DrawCircle(cx, cy, CELL_SIZE / 2 + 3, (Color){50, 230, 50, 40});
                    }
                    break;

                case COMP_WIRE: {
                    bool connected[4] = {false};
                    for (int d = 0; d < 4; d++) {
                        int dx, dy;
                        DirOffset((Direction)d, &dx, &dy);
                        int nx = x + dx, ny = y + dy;
                        if (InGrid(nx, ny) && grid[ny][nx].type != COMP_EMPTY) {
                            connected[d] = true;
                        }
                    }
                    Color wireCol = col;
                    if (signalGrid[sigRead][y][x]) {
                        float pulse = 0.6f + 0.4f * sinf(pulseTime * 6.0f);
                        wireCol = (Color){0, (unsigned char)(255 * pulse), 0, 255};
                    }
                    bool any = false;
                    for (int d = 0; d < 4; d++) {
                        if (connected[d]) {
                            any = true;
                            int dx, dy;
                            DirOffset((Direction)d, &dx, &dy);
                            int ex = cx + dx * (CELL_SIZE / 2);
                            int ey = cy + dy * (CELL_SIZE / 2);
                            DrawLineEx((Vector2){(float)cx, (float)cy},
                                       (Vector2){(float)ex, (float)ey}, 3.0f, wireCol);
                        }
                    }
                    if (!any) {
                        DrawCircle(cx, cy, 3, wireCol);
                    }
                    break;
                }

                case COMP_NOT:
                case COMP_AND:
                case COMP_OR:
                case COMP_XOR:
                case COMP_NOR: {
                    DrawRectangleRec(r, col);
                    DrawArrow(cx, cy, c->facing, WHITE);
                    const char *label = (c->type == COMP_NOT) ? "!" :
                                        (c->type == COMP_AND) ? "&" :
                                        (c->type == COMP_XOR) ? "^" :
                                        (c->type == COMP_NOR) ? "V" : "|";
                    DrawTextShadow(label, cx - 3, cy - 5, 10, WHITE);

                    // Input/output port indicators
                    int edge = CELL_SIZE / 2 - 1;
                    // Output: green dot on facing edge
                    {
                        int dx, dy;
                        DirOffset(c->facing, &dx, &dy);
                        DrawCircle(cx + dx * edge, cy + dy * edge, 3, GREEN);
                    }
                    // Inputs: orange dots
                    if (c->type == COMP_NOT) {
                        int dx, dy;
                        DirOffset(OppositeDir(c->facing), &dx, &dy);
                        DrawCircle(cx + dx * edge, cy + dy * edge, 3, ORANGE);
                    } else {
                        Direction inA, inB;
                        GateInputDirs(c->facing, &inA, &inB);
                        int dx, dy;
                        DirOffset(inA, &dx, &dy);
                        DrawCircle(cx + dx * edge, cy + dy * edge, 3, ORANGE);
                        DirOffset(inB, &dx, &dy);
                        DrawCircle(cx + dx * edge, cy + dy * edge, 3, ORANGE);
                    }
                    break;
                }

                case COMP_LATCH: {
                    DrawRectangleRec(r, col);
                    DrawArrow(cx, cy, c->facing, WHITE);
                    DrawTextShadow("M", cx - 4, cy - 5, 10, WHITE);

                    int edge = CELL_SIZE / 2 - 1;
                    // Output: green dot on facing edge
                    {
                        int dx, dy;
                        DirOffset(c->facing, &dx, &dy);
                        DrawCircle(cx + dx * edge, cy + dy * edge, 3, GREEN);
                    }
                    // SET (right side) = green input, RESET (left side) = red input
                    {
                        Direction setDir, resetDir;
                        GateInputDirs(c->facing, &setDir, &resetDir);
                        int dx, dy;
                        DirOffset(setDir, &dx, &dy);
                        DrawCircle(cx + dx * edge, cy + dy * edge, 3, (Color){0, 200, 0, 255});
                        DirOffset(resetDir, &dx, &dy);
                        DrawCircle(cx + dx * edge, cy + dy * edge, 3, (Color){200, 0, 0, 255});
                    }
                    break;
                }

                case COMP_PROCESSOR: {
                    DrawRectangleRec(r, col);
                    DrawTextShadow("C", cx - 3, cy - 5, 10, WHITE);
                    for (int d = 0; d < 4; d++) {
                        int dx, dy;
                        DirOffset((Direction)d, &dx, &dy);
                        DrawCircle(cx + dx * (CELL_SIZE / 2 - 2),
                                   cy + dy * (CELL_SIZE / 2 - 2), 2, YELLOW);
                    }
                    break;
                }

                case COMP_CLOCK: {
                    DrawRectangleRec(r, col);
                    // Show period number
                    char clkBuf[4];
                    snprintf(clkBuf, sizeof(clkBuf), "%d", c->setting);
                    DrawTextShadow(clkBuf, cx - 3, cy - 5, 10, WHITE);
                    break;
                }

                case COMP_REPEATER: {
                    DrawRectangleRec(r, col);
                    DrawArrow(cx, cy, c->facing, WHITE);
                    // Show delay ticks as dots
                    int edge = CELL_SIZE / 2 - 1;
                    {
                        int dx, dy;
                        // Output dot (green) on facing side
                        DirOffset(c->facing, &dx, &dy);
                        DrawCircle(cx + dx * edge, cy + dy * edge, 3, GREEN);
                        // Input dot (orange) on back side
                        DirOffset(OppositeDir(c->facing), &dx, &dy);
                        DrawCircle(cx + dx * edge, cy + dy * edge, 3, ORANGE);
                    }
                    // Show delay number
                    char delBuf[4];
                    snprintf(delBuf, sizeof(delBuf), "%d", c->setting);
                    DrawTextShadow(delBuf, cx - 3, cy - 5, 10, WHITE);
                    break;
                }

                default:
                    break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// UI: Bottom palette bar
// ---------------------------------------------------------------------------
static void DrawPalette(void) {
    int barY = SCREEN_HEIGHT - 50;
    DrawRectangle(0, barY, SCREEN_WIDTH, 50, (Color){20, 20, 25, 255});

    if (selectedPreset >= 0) {
        // Preset mode: show presets
        for (int i = 0; i < (int)PRESET_COUNT; i++) {
            int bx = 6 + i * (SCREEN_WIDTH - 12) / (int)PRESET_COUNT;
            int itemW = (SCREEN_WIDTH - 12) / (int)PRESET_COUNT - 4;
            bool sel = (i == selectedPreset);
            Color bg = sel ? (Color){80, 60, 20, 255} : (Color){40, 40, 45, 255};
            DrawRectangle(bx, barY + 5, itemW, 40, bg);
            if (sel) DrawRectangleLinesEx((Rectangle){(float)bx, (float)(barY + 5), (float)itemW, 40}, 2, YELLOW);

            char label[48];
            snprintf(label, sizeof(label), "%d:%s", i + 1, presets[i].name);
            DrawTextShadow(label, bx + 4, barY + 10, 10, WHITE);
            DrawTextShadow(presets[i].description, bx + 4, barY + 24, 10, (Color){160, 160, 160, 255});
        }
    } else {
        // Normal mode: show components
        const ComponentType items[] = {
            COMP_SWITCH, COMP_BUTTON, COMP_LIGHT, COMP_WIRE, COMP_NOT,
            COMP_AND, COMP_OR, COMP_XOR, COMP_NOR, COMP_LATCH, COMP_CLOCK, COMP_REPEATER, COMP_EMPTY
        };
        const char *keyLabels[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "Q", "W", "E", "0" };
        int count = sizeof(items) / sizeof(items[0]);

        for (int i = 0; i < count; i++) {
            int bx = 6 + i * (SCREEN_WIDTH - 12) / count;
            bool selected = (selectedComp == items[i]);
            int itemW = (SCREEN_WIDTH - 12) / count - 4;
            Color bg = selected ? (Color){70, 70, 80, 255} : (Color){40, 40, 45, 255};
            DrawRectangle(bx, barY + 5, itemW, 40, bg);
            if (selected) DrawRectangleLinesEx((Rectangle){(float)bx, (float)(barY + 5), (float)itemW, 40}, 2, WHITE);

            char label[32];
            snprintf(label, sizeof(label), "%s:%s", keyLabels[i], CompName(items[i]));
            DrawTextShadow(label, bx + 4, barY + 15, 10, WHITE);
        }
    }
}

// ---------------------------------------------------------------------------
// UI: Top status bar
// ---------------------------------------------------------------------------
static void DrawStatusBar(void) {
    DrawRectangle(0, 0, SCREEN_WIDTH, 30, (Color){20, 20, 25, 255});

    char status[256];
    if (selectedPreset >= 0) {
        snprintf(status, sizeof(status),
                 "PRESETS: Click to stamp \"%s\" | [1-%d]=Select [F/ESC]=Exit presets | Sim: %s",
                 presets[selectedPreset].name, (int)PRESET_COUNT,
                 simPaused ? "PAUSED" : "RUNNING");
    } else {
        const char *modeStr = (mode == MODE_PROC_EDIT) ? "PROC EDIT" : "PLACE";
        snprintf(status, sizeof(status),
                 "Mode: %s | Sim: %s | Dir: %s | [F]=Presets [Space]=Pause [T]=Step [R]=Rotate [C]=Clear",
                 modeStr, simPaused ? "PAUSED" : "RUNNING", DirName(placingDir));
    }
    DrawTextShadow(status, 10, 8, 10, (Color){200, 200, 200, 255});
}

// ---------------------------------------------------------------------------
// UI: Processor editor overlay
// ---------------------------------------------------------------------------
static void DrawProcessorEditor(void) {
    if (editProcIdx < 0 || !processors[editProcIdx].active) return;
    Processor *p = &processors[editProcIdx];

    int panelX = GRID_OFFSET_X + GRID_W * CELL_SIZE + 20;
    int panelY = GRID_OFFSET_Y;
    int panelW = 500;
    int panelH = 500;

    DrawRectangle(panelX, panelY, panelW, panelH, (Color){25, 25, 30, 240});
    DrawRectangleLinesEx((Rectangle){(float)panelX, (float)panelY, (float)panelW, (float)panelH}, 2, PURPLE);

    DrawTextShadow("PROCESSOR EDITOR", panelX + 10, panelY + 10, 16, PURPLE);

    char buf[128];
    snprintf(buf, sizeof(buf), "R0=%d  R1=%d  R2=%d  R3=%d  PC=%d  Flag=%s",
             p->regs[0], p->regs[1], p->regs[2], p->regs[3],
             p->pc, p->flag ? "T" : "F");
    DrawTextShadow(buf, panelX + 10, panelY + 35, 10, (Color){200, 200, 200, 255});

    snprintf(buf, sizeof(buf), "Ports IN: N=%d E=%d S=%d W=%d",
             ProcReadPort(p, 0), ProcReadPort(p, 1), ProcReadPort(p, 2), ProcReadPort(p, 3));
    DrawTextShadow(buf, panelX + 10, panelY + 50, 10, (Color){180, 180, 200, 255});

    DrawTextShadow("Line  OpCode   ArgA  ArgB  ArgC", panelX + 10, panelY + 72, 10, GRAY);

    int lineH = 22;
    int startY = panelY + 88;

    for (int i = 0; i < MAX_PROG_LEN; i++) {
        int ly = startY + i * lineH;
        bool isCurrentPC = (i == p->pc);
        bool isEditing = (i == editLine);

        if (isCurrentPC) {
            DrawRectangle(panelX + 5, ly - 2, panelW - 10, lineH, (Color){60, 40, 80, 200});
        }
        if (isEditing) {
            DrawRectangleLinesEx((Rectangle){(float)(panelX + 5), (float)(ly - 2),
                                 (float)(panelW - 10), (float)lineH}, 1, YELLOW);
        }

        Instruction *inst = &p->program[i];
        bool active = (i < p->progLen);
        Color textCol = active ? WHITE : (Color){60, 60, 60, 255};

        snprintf(buf, sizeof(buf), "%2d", i);
        DrawTextShadow(buf, panelX + 12, ly, 10, textCol);

        Color opcCol = (isEditing && editField == 0) ? YELLOW : textCol;
        DrawTextShadow(OpName(inst->op), panelX + 50, ly, 10, opcCol);

        Color aCol = (isEditing && editField == 1) ? YELLOW : textCol;
        snprintf(buf, sizeof(buf), "%d", inst->argA);
        DrawTextShadow(buf, panelX + 120, ly, 10, aCol);

        Color bCol = (isEditing && editField == 2) ? YELLOW : textCol;
        snprintf(buf, sizeof(buf), "%d", inst->argB);
        DrawTextShadow(buf, panelX + 170, ly, 10, bCol);

        Color cCol = (isEditing && editField == 3) ? YELLOW : textCol;
        snprintf(buf, sizeof(buf), "%d", inst->argC);
        DrawTextShadow(buf, panelX + 220, ly, 10, cCol);

        if (active && inst->op != OP_NOP) {
            const char *hint = "";
            switch (inst->op) {
                case OP_READ:  hint = "port -> reg"; break;
                case OP_WRITE: hint = "reg -> port"; break;
                case OP_SET:   hint = "val -> reg"; break;
                case OP_ADD:   hint = "rA+rB -> rA"; break;
                case OP_CMP:   hint = "rA>rB?"; break;
                case OP_JIF:   hint = "jump if flag"; break;
                default: break;
            }
            DrawTextShadow(hint, panelX + 270, ly, 10, (Color){120, 120, 140, 255});
        }
    }

    int helpY = startY + MAX_PROG_LEN * lineH + 10;
    DrawTextShadow("Up/Down=Line  Left/Right=Field  +/-=Change  Ins=AddLine  Del=RemLine  ESC=Close",
             panelX + 10, helpY, 10, (Color){150, 150, 160, 255});
    DrawTextShadow("OpCodes: NOP READ WRITE SET ADD CMP JIF",
             panelX + 10, helpY + 14, 10, (Color){150, 150, 160, 255});
    DrawTextShadow("Ports: 0=N 1=E 2=S 3=W  |  Regs: 0-3",
             panelX + 10, helpY + 28, 10, (Color){150, 150, 160, 255});
}

// ---------------------------------------------------------------------------
// UI: Tooltip for hovered cell
// ---------------------------------------------------------------------------
static void DrawCellTooltip(int gx, int gy) {
    if (!InGrid(gx, gy)) return;
    Cell *c = &grid[gy][gx];
    if (c->type == COMP_EMPTY) return;

    char buf[256];
    if (c->type == COMP_CLOCK) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] state=%s period=%d timer=%d (click to change period)",
                 CompName(c->type), gx, gy,
                 c->state ? "ON" : "OFF",
                 c->setting, c->timer);
    } else if (c->type == COMP_REPEATER) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] state=%s delay=%d dir=%s (click to change delay)",
                 CompName(c->type), gx, gy,
                 c->state ? "ON" : "OFF",
                 c->setting, DirName(c->facing));
    } else {
        snprintf(buf, sizeof(buf), "%s [%d,%d] state=%s sigIn=%d sigOut=%d dir=%s",
                 CompName(c->type), gx, gy,
                 c->state ? "ON" : "OFF",
                 c->signalIn, c->signalOut,
                 DirName(c->facing));
    }

    int mx = GetMouseX() + 15;
    int my = GetMouseY() - 20;
    int tw = MeasureTextUI(buf, 10) + 10;
    DrawRectangle(mx - 2, my - 2, tw, 18, (Color){20, 20, 25, 230});
    DrawTextShadow(buf, mx + 3, my + 2, 10, WHITE);
}

// ---------------------------------------------------------------------------
// Input handling
// ---------------------------------------------------------------------------
static void HandleInput(void) {
    int mx = GetMouseX(), my = GetMouseY();
    int gx, gy;
    GridFromScreen(mx, my, &gx, &gy);

    // F key: toggle preset mode
    if (IsKeyPressed(KEY_F) && mode == MODE_PLACE) {
        if (selectedPreset >= 0) {
            selectedPreset = -1;  // exit preset mode
        } else {
            selectedPreset = 0;   // enter preset mode
        }
    }

    // Preset mode input
    if (selectedPreset >= 0 && mode == MODE_PLACE) {
        if (IsKeyPressed(KEY_ONE) && PRESET_COUNT > 0)   selectedPreset = 0;
        if (IsKeyPressed(KEY_TWO) && PRESET_COUNT > 1)   selectedPreset = 1;
        if (IsKeyPressed(KEY_THREE) && PRESET_COUNT > 2) selectedPreset = 2;
        if (IsKeyPressed(KEY_FOUR) && PRESET_COUNT > 3)  selectedPreset = 3;
        if (IsKeyPressed(KEY_FIVE) && PRESET_COUNT > 4)  selectedPreset = 4;
        if (IsKeyPressed(KEY_SIX) && PRESET_COUNT > 5)   selectedPreset = 5;

        // Click to stamp preset at cursor
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && InGrid(gx, gy)) {
            presets[selectedPreset].build(gx, gy);
        }

        // ESC or F exits preset mode
        if (IsKeyPressed(KEY_ESCAPE)) {
            selectedPreset = -1;
        }
        return;  // don't process normal input while in preset mode
    }

    // Component selection keys
    if (mode == MODE_PLACE) {
        if (IsKeyPressed(KEY_ONE))   selectedComp = COMP_SWITCH;
        if (IsKeyPressed(KEY_TWO))   selectedComp = COMP_BUTTON;
        if (IsKeyPressed(KEY_THREE)) selectedComp = COMP_LIGHT;
        if (IsKeyPressed(KEY_FOUR))  selectedComp = COMP_WIRE;
        if (IsKeyPressed(KEY_FIVE))  selectedComp = COMP_NOT;
        if (IsKeyPressed(KEY_SIX))   selectedComp = COMP_AND;
        if (IsKeyPressed(KEY_SEVEN)) selectedComp = COMP_OR;
        if (IsKeyPressed(KEY_EIGHT)) selectedComp = COMP_XOR;
        if (IsKeyPressed(KEY_NINE))  selectedComp = COMP_NOR;
        if (IsKeyPressed(KEY_Q))     selectedComp = COMP_LATCH;
        if (IsKeyPressed(KEY_W))     selectedComp = COMP_CLOCK;
        if (IsKeyPressed(KEY_E))     selectedComp = COMP_REPEATER;
        if (IsKeyPressed(KEY_ZERO))  selectedComp = COMP_EMPTY;
    }

    // Rotate
    if (IsKeyPressed(KEY_R)) {
        placingDir = (Direction)((placingDir + 1) % 4);
    }

    // Clear grid
    if (IsKeyPressed(KEY_C) && mode == MODE_PLACE) {
        ClearGrid();
    }

    // Pause/step
    if (IsKeyPressed(KEY_SPACE)) {
        simPaused = !simPaused;
    }
    if (IsKeyPressed(KEY_T) && simPaused) {
        UpdateSignals();
        UpdateProcessors();
    }

    // Processor editor
    if (IsKeyPressed(KEY_P) && mode == MODE_PLACE) {
        if (InGrid(gx, gy) && grid[gy][gx].type == COMP_PROCESSOR) {
            editProcIdx = grid[gy][gx].procIdx;
            if (editProcIdx >= 0) {
                mode = MODE_PROC_EDIT;
                editLine = 0;
                editField = 0;
            }
        }
    }

    // ESC: back to place mode
    if (IsKeyPressed(KEY_ESCAPE)) {
        mode = MODE_PLACE;
        editProcIdx = -1;
    }

    // Release all buttons every frame — they only stay ON while held
    for (int by = 0; by < GRID_H; by++)
        for (int bx = 0; bx < GRID_W; bx++)
            if (grid[by][bx].type == COMP_BUTTON)
                grid[by][bx].state = false;

    // --- Mode-specific input ---
    switch (mode) {
        case MODE_PLACE: {
            // Hold button down
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && InGrid(gx, gy)) {
                if (grid[gy][gx].type == COMP_BUTTON) {
                    grid[gy][gx].state = true;
                }
            }

            // Left click: place, toggle, or configure
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && InGrid(gx, gy)) {
                Cell *clicked = &grid[gy][gx];
                if (clicked->type == COMP_BUTTON) {
                    // Already handled above (hold)
                } else if (selectedComp == COMP_SWITCH && clicked->type == COMP_SWITCH) {
                    clicked->state = !clicked->state;
                } else if (selectedComp == COMP_CLOCK && clicked->type == COMP_CLOCK) {
                    clicked->setting = (clicked->setting % 8) + 1;
                    clicked->timer = clicked->setting;
                } else if (selectedComp == COMP_REPEATER && clicked->type == COMP_REPEATER) {
                    clicked->setting = (clicked->setting % 4) + 1;
                    memset(clicked->delayBuf, 0, sizeof(clicked->delayBuf));
                } else {
                    PlaceComponent(gx, gy, selectedComp);
                }
            }
            // Drag placement for wire and other non-interactive components
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && InGrid(gx, gy)) {
                Cell *clicked = &grid[gy][gx];
                if (clicked->type != COMP_BUTTON) {
                    bool isClickConfig = (selectedComp == COMP_SWITCH && clicked->type == COMP_SWITCH)
                                      || (selectedComp == COMP_CLOCK && clicked->type == COMP_CLOCK)
                                      || (selectedComp == COMP_REPEATER && clicked->type == COMP_REPEATER);
                    if (!isClickConfig) {
                        PlaceComponent(gx, gy, selectedComp);
                    }
                }
            }
            // Right click: remove
            if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && InGrid(gx, gy)) {
                PlaceComponent(gx, gy, COMP_EMPTY);
            }
            break;
        }

        case MODE_PROC_EDIT: {
            if (editProcIdx < 0) break;
            Processor *p = &processors[editProcIdx];

            if (IsKeyPressed(KEY_UP) && editLine > 0) editLine--;
            if (IsKeyPressed(KEY_DOWN) && editLine < MAX_PROG_LEN - 1) editLine++;
            if (IsKeyPressed(KEY_LEFT) && editField > 0) editField--;
            if (IsKeyPressed(KEY_RIGHT) && editField < 3) editField++;

            Instruction *inst = &p->program[editLine];
            int delta = 0;
            if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) delta = 1;
            if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) delta = -1;

            if (delta != 0) {
                switch (editField) {
                    case 0: {
                        int op = (int)inst->op + delta;
                        if (op < 0) op = OP_COUNT - 1;
                        if (op >= OP_COUNT) op = 0;
                        inst->op = (OpCode)op;
                        break;
                    }
                    case 1: inst->argA += delta; break;
                    case 2: inst->argB += delta; break;
                    case 3: inst->argC += delta; break;
                }
            }

            if (IsKeyPressed(KEY_INSERT) && p->progLen < MAX_PROG_LEN) {
                for (int i = p->progLen; i > editLine + 1; i--) {
                    p->program[i] = p->program[i - 1];
                }
                p->progLen++;
                if (editLine + 1 < MAX_PROG_LEN) {
                    memset(&p->program[editLine + 1], 0, sizeof(Instruction));
                }
            }
            if (IsKeyPressed(KEY_DELETE) && p->progLen > 1 && editLine < p->progLen) {
                for (int i = editLine; i < p->progLen - 1; i++) {
                    p->program[i] = p->program[i + 1];
                }
                p->progLen--;
                memset(&p->program[p->progLen], 0, sizeof(Instruction));
                if (editLine >= p->progLen) editLine = p->progLen - 1;
            }
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(void) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Mechanisms & Signals Sandbox");
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);

    Font comicFont = LoadEmbeddedFont();
    ui_init(&comicFont);

    ClearGrid();

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        pulseTime += dt;

        HandleInput();

        // Simulation tick
        if (!simPaused) {
            tickTimer += dt;
            while (tickTimer >= TICK_INTERVAL) {
                tickTimer -= TICK_INTERVAL;
                UpdateSignals();
                UpdateProcessors();
            }
        }

        // Draw
        BeginDrawing();
        ClearBackground((Color){15, 15, 20, 255});

        DrawGridBackground();
        DrawComponents();

        // Hover highlight
        {
            int gx, gy;
            GridFromScreen(GetMouseX(), GetMouseY(), &gx, &gy);
            if (InGrid(gx, gy) && mode == MODE_PLACE) {
                if (selectedPreset >= 0) {
                    // Draw preset bounding box preview
                    Preset *pr = &presets[selectedPreset];
                    for (int py = 0; py < pr->height; py++) {
                        for (int px = 0; px < pr->width; px++) {
                            if (InGrid(gx + px, gy + py)) {
                                Rectangle r = CellRect(gx + px, gy + py);
                                DrawRectangleLinesEx(r, 1, (Color){255, 200, 50, 60});
                            }
                        }
                    }
                } else {
                    Rectangle r = CellRect(gx, gy);
                    DrawRectangleLinesEx(r, 2, (Color){255, 255, 255, 80});
                }
            }
            DrawCellTooltip(gx, gy);
        }

        DrawPalette();
        DrawStatusBar();
        DrawProcessorEditor();

        EndDrawing();
    }

    UnloadFont(comicFont);
    CloseWindow();
    return 0;
}
