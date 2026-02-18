// Mechanisms & Signals Sandbox Demo
// A learning environment for signal-based automation:
//   Switch (source) -> Wire -> Logic Gates -> Wire -> Light (sink)
//   + Processor (tiny emulator with 6 opcodes)
//   + Fluid layer: Pipe/Pump/Drain/Valve/Tank/PressureLight
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
    COMP_PULSE,     // Directional: on rising edge, outputs 1 for N ticks (click to change 1-8)
    COMP_PIPE,      // Fluid: carries fluid, pressure equalizes with adjacent pipes
    COMP_PUMP,      // Fluid source: adds pressure (setting 1-8 = rate)
    COMP_DRAIN,     // Fluid sink: removes pressure (setting 1-8 = rate)
    COMP_VALVE,     // Fluid gate: directional pipe, open when adjacent wire has signal
    COMP_TANK,      // Fluid storage: high capacity (0-1024)
    COMP_PRESSURE_LIGHT, // Fluid→signal bridge: lights up when adjacent pipe pressure > threshold
    COMP_DIAL,           // Source: emits analog value 0-15 (click to change)
    COMP_COMPARATOR,     // Directional: outputs 1 if back-side analog >= threshold (click to set)
    COMP_DISPLAY,        // Shows numeric value of adjacent signal
    COMP_BELT,           // Logistics: moves cargo in facing direction
    COMP_LOADER,         // Logistics source: generates cargo (click to set type 1-15)
    COMP_UNLOADER,       // Logistics sink: consumes cargo, emits signal = consumed type
    COMP_GRABBER,        // Logistics inserter: moves cargo from back to front, signal-controlled
    COMP_SPLITTER,       // Logistics: alternates cargo left/right
    COMP_FILTER,         // Logistics: only passes cargo matching set type (click to set)
    COMP_COMPRESSOR,     // Logistics: merges two side inputs into dual cargo on forward belt
    COMP_DECOMPRESSOR,   // Logistics: splits dual cargo into forward + side outputs
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
    int fluidLevel;     // 0-255 pressure for pipes (0-1024 for tanks)
    int cargo;          // 0 = empty, 1-15 = item type/color on belt
    int cargo2;         // 0 = empty, second cargo slot for compressed belts
    bool altToggle;     // splitter: alternates left/right each item
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

// Forward declarations (needed by preset builders)
static void UpdateSignals(void);
static void UpdateProcessors(void);
static void UpdateFluids(void);
static void UpdateBelts(void);

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
        case COMP_PULSE:      return "Pulse";
        case COMP_PIPE:       return "Pipe";
        case COMP_PUMP:       return "Pump";
        case COMP_DRAIN:      return "Drain";
        case COMP_VALVE:      return "Valve";
        case COMP_TANK:       return "Tank";
        case COMP_PRESSURE_LIGHT: return "PrLight";
        case COMP_DIAL:       return "Dial";
        case COMP_COMPARATOR: return "Compare";
        case COMP_DISPLAY:    return "Display";
        case COMP_BELT:       return "Belt";
        case COMP_LOADER:     return "Loader";
        case COMP_UNLOADER:   return "Unloader";
        case COMP_GRABBER:    return "Grabber";
        case COMP_SPLITTER:   return "Splitter";
        case COMP_FILTER:     return "Filter";
        case COMP_COMPRESSOR: return "Compress";
        case COMP_DECOMPRESSOR: return "Decomp";
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
    if (type == COMP_PULSE) {
        grid[gy][gx].setting = 5;  // default pulse duration: 5 ticks
    }
    if (type == COMP_PUMP) {
        grid[gy][gx].setting = 4;  // default pump rate
    }
    if (type == COMP_DRAIN) {
        grid[gy][gx].setting = 4;  // default drain rate
    }
    if (type == COMP_VALVE) {
        // Valve uses facing for directionality
    }
    if (type == COMP_DIAL) {
        grid[gy][gx].setting = 8;  // default analog value: 8
        grid[gy][gx].state = true;
    }
    if (type == COMP_COMPARATOR) {
        grid[gy][gx].setting = 5;  // default threshold: 5
    }
    if (type == COMP_LOADER) {
        grid[gy][gx].setting = 1;  // default cargo type: 1
    }
    if (type == COMP_FILTER) {
        grid[gy][gx].setting = 1;  // default filter: type 1
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
// Uses pulse extenders on button inputs so a quick tap reliably flips the latch.
// Layout (9x8):
//   Row 0: R button -> Pulse -> wire -> NOR1 north input
//   Row 1: NOR1 (east) -> Q output -> Light
//   Row 2-4: Cross-feedback paths (col 2 for Q̄, col 6 for Q)
//   Row 5: NOR2 (west) <- Q̄ output <- Light
//   Row 6-7: S button -> Pulse -> wire -> NOR2 south input
// Key: NOR1 faces EAST, NOR2 faces WEST, feedback through separate columns
static void BuildPresetNorLatch(int ox, int oy) {
    // R (Reset) button -> pulse extender -> NOR1 north input
    PlaceAt(ox, oy, COMP_BUTTON, DIR_NORTH);
    PlaceWire(ox + 1, oy);
    PlaceAt(ox + 2, oy, COMP_PULSE, DIR_EAST);  // extends tap to 5 ticks
    PlaceWire(ox + 3, oy);
    PlaceWire(ox + 4, oy);

    // NOR1 at (ox+4, oy+1) facing east
    PlaceAt(ox + 4, oy + 1, COMP_NOR, DIR_EAST);
    // Q output: wire east -> light
    PlaceWire(ox + 5, oy + 1);
    PlaceWire(ox + 6, oy + 1);
    PlaceAt(ox + 7, oy + 1, COMP_LIGHT, DIR_EAST);

    // Q feedback path: column 6 going down from Q output
    PlaceWire(ox + 6, oy + 2);
    PlaceWire(ox + 6, oy + 3);
    PlaceWire(ox + 6, oy + 4);
    // Repeater delivers Q to NOR2 north input
    PlaceAt(ox + 5, oy + 4, COMP_REPEATER, DIR_WEST);
    PlaceWire(ox + 4, oy + 4);  // NOR2 north input

    // NOR2 at (ox+4, oy+5) facing west
    PlaceAt(ox + 4, oy + 5, COMP_NOR, DIR_WEST);
    // Q̄ output: wire west -> light
    PlaceWire(ox + 3, oy + 5);
    PlaceWire(ox + 2, oy + 5);
    PlaceAt(ox + 1, oy + 5, COMP_LIGHT, DIR_EAST);

    // Q̄ feedback path: column 2 going up from Q̄ output
    PlaceWire(ox + 2, oy + 4);
    PlaceWire(ox + 2, oy + 3);
    PlaceWire(ox + 2, oy + 2);
    // Repeater delivers Q̄ to NOR1 south input
    PlaceAt(ox + 3, oy + 2, COMP_REPEATER, DIR_EAST);
    PlaceWire(ox + 4, oy + 2);  // NOR1 south input

    // S (Set) button -> pulse extender -> NOR2 south input
    PlaceWire(ox + 4, oy + 6);
    PlaceWire(ox + 4, oy + 7);
    PlaceWire(ox + 3, oy + 7);
    PlaceAt(ox + 2, oy + 7, COMP_PULSE, DIR_EAST);  // extends tap to 5 ticks
    PlaceWire(ox + 1, oy + 7);
    PlaceAt(ox, oy + 7, COMP_BUTTON, DIR_NORTH);

    // Simulate pressing S button for a few ticks to break symmetry
    // and settle the latch into a stable Q=0, Q̄=1 initial state
    grid[oy + 7][ox].state = true;
    for (int i = 0; i < 10; i++) { UpdateSignals(); UpdateProcessors(); }
    grid[oy + 7][ox].state = false;
    for (int i = 0; i < 10; i++) { UpdateSignals(); UpdateProcessors(); }
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

// Preset 7: Half Adder — two Switches -> XOR (sum) + AND (carry) -> two Lights
// Layout (9x5): Gap rows keep gate outputs from merging with input wires.
// Repeaters split A/B signals to both gates independently.
//   Row 0: SwA -> wire -> REP -> wire        (A signal, split left/right)
//   Row 1:          A↓                 A↓    (drop to gate inputs)
//   Row 2:        XOR→ wire SumL    AND→ wire CarryL
//   Row 3:          B↑                 B↑    (rise to gate inputs)
//   Row 4: SwB -> wire -> REP -> wire        (B signal, split left/right)
static void BuildPresetHalfAdder(int ox, int oy) {
    // Switch A (top row) — repeater splits signal for XOR and AND
    PlaceAt(ox, oy, COMP_SWITCH, DIR_NORTH);
    PlaceWire(ox + 1, oy);
    PlaceWire(ox + 2, oy);
    PlaceWire(ox + 3, oy);
    PlaceAt(ox + 4, oy, COMP_REPEATER, DIR_EAST);
    PlaceWire(ox + 5, oy);
    PlaceWire(ox + 6, oy);

    // A drops down to gate north inputs
    PlaceWire(ox + 2, oy + 1);  // XOR north input via A-left network
    PlaceWire(ox + 6, oy + 1);  // AND north input via A-right network

    // XOR gate -> Sum light
    PlaceAt(ox + 2, oy + 2, COMP_XOR, DIR_EAST);
    PlaceWire(ox + 3, oy + 2);
    PlaceAt(ox + 4, oy + 2, COMP_LIGHT, DIR_EAST);

    // AND gate -> Carry light
    PlaceAt(ox + 6, oy + 2, COMP_AND, DIR_EAST);
    PlaceWire(ox + 7, oy + 2);
    PlaceAt(ox + 8, oy + 2, COMP_LIGHT, DIR_EAST);

    // B rises up to gate south inputs
    PlaceWire(ox + 2, oy + 3);  // XOR south input via B-left network
    PlaceWire(ox + 6, oy + 3);  // AND south input via B-right network

    // Switch B (bottom row) — repeater splits signal for XOR and AND
    PlaceAt(ox, oy + 4, COMP_SWITCH, DIR_NORTH);
    PlaceWire(ox + 1, oy + 4);
    PlaceWire(ox + 2, oy + 4);
    PlaceWire(ox + 3, oy + 4);
    PlaceAt(ox + 4, oy + 4, COMP_REPEATER, DIR_EAST);
    PlaceWire(ox + 5, oy + 4);
    PlaceWire(ox + 6, oy + 4);
}

// Preset 8: Ring Oscillator — 3 NOT gates in a loop, self-oscillating
// Layout: triangle of NOT gates connected by wires, with a light tap
//   NOT1 (east) -> wire -> NOT2 (south) -> wire -> NOT3 (west) -> wire -> back to NOT1
static void BuildPresetRingOsc(int ox, int oy) {
    // NOT1 at (ox+1, oy) facing east
    PlaceAt(ox + 1, oy, COMP_NOT, DIR_EAST);
    PlaceWire(ox + 2, oy);       // NOT1 output
    PlaceWire(ox + 3, oy);

    // NOT2 at (ox+3, oy+1) facing south
    PlaceAt(ox + 3, oy + 1, COMP_NOT, DIR_SOUTH);
    PlaceWire(ox + 3, oy + 2);   // NOT2 output
    PlaceWire(ox + 2, oy + 2);

    // NOT3 at (ox+1, oy+2) facing west
    PlaceAt(ox + 1, oy + 2, COMP_NOT, DIR_WEST);
    PlaceWire(ox, oy + 2);       // NOT3 output
    PlaceWire(ox, oy + 1);
    PlaceWire(ox, oy);           // feeds back to NOT1 input

    // Light taps off NOT1's output to show the oscillation
    PlaceWire(ox + 2, oy + 1);
    PlaceAt(ox + 2, oy + 1, COMP_LIGHT, DIR_EAST);

    // Kick-start: seed one wire so oscillation begins immediately
    signalGrid[0][oy][ox + 2] = 1;
    signalGrid[1][oy][ox + 2] = 1;
}

// Preset 9: Pump Loop — Pump -> pipe chain -> drain + pressure light
static void BuildPresetPumpLoop(int ox, int oy) {
    PlaceAt(ox, oy + 1, COMP_PUMP, DIR_NORTH);
    grid[oy + 1][ox].setting = 4;
    for (int i = 1; i <= 6; i++) {
        PlaceAt(ox + i, oy + 1, COMP_PIPE, DIR_NORTH);
    }
    PlaceAt(ox + 7, oy + 1, COMP_DRAIN, DIR_NORTH);
    grid[oy + 1][ox + 7].setting = 2;
    // Pressure light taps off middle of chain
    PlaceAt(ox + 3, oy, COMP_PRESSURE_LIGHT, DIR_NORTH);
    // Wire from pressure light to electrical light
    PlaceWire(ox + 4, oy);
    PlaceAt(ox + 5, oy, COMP_LIGHT, DIR_EAST);
    // Let it run a bit to build pressure
    for (int i = 0; i < 30; i++) { UpdateSignals(); UpdateProcessors(); UpdateFluids(); }
}

// Preset 10: Signal Valve — Switch -> wire -> valve, pump on one side, light on the other
static void BuildPresetSignalValve(int ox, int oy) {
    // Pump at left
    PlaceAt(ox, oy + 1, COMP_PUMP, DIR_NORTH);
    grid[oy + 1][ox].setting = 6;
    // Pipe to valve
    PlaceAt(ox + 1, oy + 1, COMP_PIPE, DIR_NORTH);
    PlaceAt(ox + 2, oy + 1, COMP_PIPE, DIR_NORTH);
    // Valve in the middle (facing east)
    PlaceAt(ox + 3, oy + 1, COMP_VALVE, DIR_EAST);
    // Pipe from valve to pressure light
    PlaceAt(ox + 4, oy + 1, COMP_PIPE, DIR_NORTH);
    PlaceAt(ox + 5, oy + 1, COMP_PIPE, DIR_NORTH);
    PlaceAt(ox + 6, oy + 1, COMP_PRESSURE_LIGHT, DIR_NORTH);
    // Wire from pressure light to electrical light
    PlaceWire(ox + 7, oy + 1);
    PlaceAt(ox + 8, oy + 1, COMP_LIGHT, DIR_EAST);
    // Switch + wire controlling the valve (above it)
    PlaceAt(ox + 1, oy, COMP_SWITCH, DIR_NORTH);
    PlaceWire(ox + 2, oy);
    PlaceWire(ox + 3, oy);  // adjacent to valve = controls it
    // Pre-fill left side of pipe
    for (int i = 0; i < 20; i++) { UpdateSignals(); UpdateProcessors(); UpdateFluids(); }
}

// Preset 11: Analog Demo — Dial -> wire -> comparator -> light + display
static void BuildPresetAnalog(int ox, int oy) {
    PlaceAt(ox, oy + 1, COMP_DIAL, DIR_NORTH);
    grid[oy + 1][ox].setting = 8;
    PlaceWire(ox + 1, oy + 1);
    PlaceWire(ox + 2, oy + 1);
    // Display shows raw analog value
    PlaceAt(ox + 2, oy, COMP_DISPLAY, DIR_NORTH);
    // Comparator with threshold 5
    PlaceAt(ox + 3, oy + 1, COMP_COMPARATOR, DIR_EAST);
    grid[oy + 1][ox + 3].setting = 5;
    // Output to light (on when dial >= 5)
    PlaceWire(ox + 4, oy + 1);
    PlaceAt(ox + 5, oy + 1, COMP_LIGHT, DIR_EAST);
}

// Preset 12: Belt Line — Loader -> belts -> splitter -> two unloaders
static void BuildPresetBeltLine(int ox, int oy) {
    // Loader (type 1 = red) feeds belt chain
    PlaceAt(ox, oy + 1, COMP_LOADER, DIR_EAST);
    grid[oy + 1][ox].setting = 1;
    // Belt chain
    for (int i = 1; i <= 4; i++) {
        PlaceAt(ox + i, oy + 1, COMP_BELT, DIR_EAST);
    }
    // Splitter at the end
    PlaceAt(ox + 5, oy + 1, COMP_SPLITTER, DIR_EAST);
    // Top output belt -> unloader (belt at splitter output to bridge gap)
    PlaceAt(ox + 5, oy, COMP_BELT, DIR_EAST);
    PlaceAt(ox + 6, oy, COMP_BELT, DIR_EAST);
    PlaceAt(ox + 7, oy, COMP_BELT, DIR_EAST);
    PlaceAt(ox + 8, oy, COMP_UNLOADER, DIR_EAST);
    // Bottom output belt -> unloader (belt at splitter output to bridge gap)
    PlaceAt(ox + 5, oy + 2, COMP_BELT, DIR_EAST);
    PlaceAt(ox + 6, oy + 2, COMP_BELT, DIR_EAST);
    PlaceAt(ox + 7, oy + 2, COMP_BELT, DIR_EAST);
    PlaceAt(ox + 8, oy + 2, COMP_UNLOADER, DIR_EAST);
    // Wire from top unloader to display
    PlaceWire(ox + 9, oy);
    PlaceAt(ox + 9, oy + 1, COMP_DISPLAY, DIR_NORTH);
}

// Preset 13: Compress — two loaders -> compressor -> compressed belt -> decompressor -> two unloaders
// Preset 13: Compress — 2 loaders -> compressor -> compressed belt -> decompressor -> 2 unloaders
// Layout (13x5):
//   Row 0: Loader1(red) -> belt -> belt ──┐
//   Row 1:                     compressor → belt belt belt → decompressor → belt → Unloader1
//   Row 2: Loader2(green) -> belt -> belt ─┘                     ↓
//   Row 3:                                                      belt
//   Row 4:                                                    Unloader2
static void BuildPresetCompress(int ox, int oy) {
    // Top loader (type 1 = red) feeds south into compressor's left side
    PlaceAt(ox, oy, COMP_LOADER, DIR_EAST);
    grid[oy][ox].setting = 1;
    PlaceAt(ox + 1, oy, COMP_BELT, DIR_EAST);
    PlaceAt(ox + 2, oy, COMP_BELT, DIR_SOUTH);

    // Bottom loader (type 2 = green) feeds north into compressor's right side
    PlaceAt(ox, oy + 2, COMP_LOADER, DIR_EAST);
    grid[oy + 2][ox].setting = 2;
    PlaceAt(ox + 1, oy + 2, COMP_BELT, DIR_EAST);
    PlaceAt(ox + 2, oy + 2, COMP_BELT, DIR_NORTH);

    // Compressor facing east, inputs from north (left) and south (right)
    PlaceAt(ox + 2, oy + 1, COMP_COMPRESSOR, DIR_EAST);

    // Compressed belt chain going east
    for (int i = 3; i <= 6; i++) {
        PlaceAt(ox + i, oy + 1, COMP_BELT, DIR_EAST);
    }

    // Decompressor facing east
    PlaceAt(ox + 7, oy + 1, COMP_DECOMPRESSOR, DIR_EAST);

    // Forward output (cargo) -> belt -> unloader
    PlaceAt(ox + 8, oy + 1, COMP_BELT, DIR_EAST);
    PlaceAt(ox + 9, oy + 1, COMP_BELT, DIR_EAST);
    PlaceAt(ox + 10, oy + 1, COMP_UNLOADER, DIR_EAST);

    // Side output (cargo2) -> belt going south -> unloader
    PlaceAt(ox + 7, oy + 2, COMP_BELT, DIR_SOUTH);
    PlaceAt(ox + 7, oy + 3, COMP_BELT, DIR_SOUTH);
    PlaceAt(ox + 7, oy + 4, COMP_UNLOADER, DIR_SOUTH);
}

static Preset presets[] = {
    { "NOT",          "Switch -> NOT -> Light",              BuildPresetNot,         6, 3 },
    { "AND",          "2 Switches -> AND -> Light",          BuildPresetAnd,         6, 3 },
    { "XOR",          "2 Switches -> XOR -> Light",          BuildPresetXor,         6, 3 },
    { "Blinker",      "Clock -> Light",                      BuildPresetBlinker,     3, 3 },
    { "Pulse Extend", "Button -> Repeater(4) -> Light",      BuildPresetPulseExtend, 5, 3 },
    { "NOR Latch",    "NOR gates + pulse extenders, memory", BuildPresetNorLatch,    8, 8 },
    { "Half Adder",   "XOR=sum, AND=carry, binary math",    BuildPresetHalfAdder,   9, 5 },
    { "Ring Osc",     "3 NOT gates in a loop, auto-osc",    BuildPresetRingOsc,     4, 3 },
    { "Pump Loop",    "Pump -> pipes -> drain + light",      BuildPresetPumpLoop,    8, 2 },
    { "Sig Valve",    "Switch controls valve, fluid->light", BuildPresetSignalValve, 9, 2 },
    { "Analog",       "Dial -> display + comparator -> light", BuildPresetAnalog,    6, 2 },
    { "Belt Line",    "Loader -> belts -> splitter -> unload", BuildPresetBeltLine,  11, 4 },
    { "Compress",     "Compress 2 belts -> decompress to 2",  BuildPresetCompress,  11, 5 },
};
#define PRESET_COUNT (sizeof(presets) / sizeof(presets[0]))

static int selectedPreset = -1;  // -1 = not in preset mode

// ---------------------------------------------------------------------------
// Simulation: Signal propagation (flood-fill from sources)
// ---------------------------------------------------------------------------

// BFS queue for wire flood-fill
static int bfsQueueX[GRID_W * GRID_H];
static int bfsQueueY[GRID_W * GRID_H];

// Helper: seed a wire cell with an analog value (max wins)
static int bfsSeedVal[GRID_H][GRID_W]; // track value per queued wire
static void SeedWire(int newSig[][GRID_W], int nx, int ny, int value,
                     int *qTail) {
    if (!InGrid(nx, ny) || grid[ny][nx].type != COMP_WIRE) return;
    if (newSig[ny][nx] >= value) return; // already has equal or higher value
    newSig[ny][nx] = value;
    bfsSeedVal[ny][nx] = value;
    bfsQueueX[*qTail] = nx;
    bfsQueueY[*qTail] = ny;
    (*qTail)++;
}

// Helper: seed all 4 adjacent wires from an omnidirectional source
static void SeedAdjacentWires(int newSig[][GRID_W], int x, int y, int value,
                              int *qTail) {
    for (int d = 0; d < 4; d++) {
        int dx, dy;
        DirOffset((Direction)d, &dx, &dy);
        SeedWire(newSig, x + dx, y + dy, value, qTail);
    }
}

// Helper: seed the wire in the facing direction
static void SeedFacingWire(int newSig[][GRID_W], int x, int y, Direction facing,
                           int value, int *qTail) {
    int dx, dy;
    DirOffset(facing, &dx, &dy);
    SeedWire(newSig, x + dx, y + dy, value, qTail);
}

static void UpdateSignals(void) {
    int newSig[GRID_H][GRID_W];
    memset(newSig, 0, sizeof(newSig));
    memset(bfsSeedVal, 0, sizeof(bfsSeedVal));

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
                    SeedAdjacentWires(newSig, x, y, 1, &qTail);
                    break;
                }

                case COMP_DIAL: {
                    // Analog source: emits setting value (0-15)
                    c->signalOut = c->setting;
                    if (!c->signalOut) break;
                    SeedAdjacentWires(newSig, x, y, c->setting, &qTail);
                    break;
                }

                case COMP_CLOCK: {
                    c->timer--;
                    if (c->timer <= 0) {
                        c->state = !c->state;
                        c->timer = c->setting;
                    }
                    c->signalOut = c->state ? 1 : 0;
                    if (!c->signalOut) break;
                    SeedAdjacentWires(newSig, x, y, 1, &qTail);
                    break;
                }

                case COMP_REPEATER: {
                    int dx, dy;
                    DirOffset(OppositeDir(c->facing), &dx, &dy);
                    int inX = x + dx, inY = y + dy;
                    int input = 0;
                    if (InGrid(inX, inY)) input = signalGrid[sigRead][inY][inX];
                    c->signalIn = input;

                    int delay = c->setting;
                    if (delay < 1) delay = 1;
                    if (delay > 4) delay = 4;
                    for (int i = 0; i < delay - 1; i++) {
                        c->delayBuf[i] = c->delayBuf[i + 1];
                    }
                    c->delayBuf[delay - 1] = input;
                    int output = c->delayBuf[0];

                    c->signalOut = output;
                    c->state = (output != 0);
                    if (output) {
                        SeedFacingWire(newSig, x, y, c->facing, output, &qTail);
                    }
                    break;
                }

                case COMP_PULSE: {
                    int dx, dy;
                    DirOffset(OppositeDir(c->facing), &dx, &dy);
                    int inX = x + dx, inY = y + dy;
                    int input = 0;
                    if (InGrid(inX, inY)) input = signalGrid[sigRead][inY][inX];
                    c->signalIn = input;

                    if (input && !c->delayBuf[0]) {
                        c->timer = c->setting;
                    }
                    c->delayBuf[0] = input;

                    if (c->timer > 0) {
                        c->timer--;
                        c->signalOut = 1;
                        c->state = true;
                        SeedFacingWire(newSig, x, y, c->facing, 1, &qTail);
                    } else {
                        c->signalOut = 0;
                        c->state = false;
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
                        SeedFacingWire(newSig, x, y, c->facing, output, &qTail);
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
                        SeedFacingWire(newSig, x, y, c->facing, output, &qTail);
                    }
                    break;
                }

                case COMP_LATCH: {
                    Direction setDir, resetDir;
                    GateInputDirs(c->facing, &setDir, &resetDir);
                    int dx, dy;
                    DirOffset(setDir, &dx, &dy);
                    int setIn = 0, resetIn = 0;
                    if (InGrid(x + dx, y + dy)) setIn = signalGrid[sigRead][y + dy][x + dx];
                    DirOffset(resetDir, &dx, &dy);
                    if (InGrid(x + dx, y + dy)) resetIn = signalGrid[sigRead][y + dy][x + dx];

                    if (setIn && !resetIn) c->state = true;
                    else if (resetIn && !setIn) c->state = false;

                    c->signalIn = setIn | (resetIn << 1);
                    c->signalOut = c->state ? 1 : 0;
                    if (c->signalOut) {
                        SeedFacingWire(newSig, x, y, c->facing, 1, &qTail);
                    }
                    break;
                }

                case COMP_COMPARATOR: {
                    // Reads analog value from back side, outputs 1 if >= threshold
                    int dx, dy;
                    DirOffset(OppositeDir(c->facing), &dx, &dy);
                    int inX = x + dx, inY = y + dy;
                    int input = 0;
                    if (InGrid(inX, inY)) input = signalGrid[sigRead][inY][inX];
                    c->signalIn = input;
                    int output = (input >= c->setting) ? 1 : 0;
                    c->signalOut = output;
                    c->state = (output != 0);
                    if (output) {
                        SeedFacingWire(newSig, x, y, c->facing, output, &qTail);
                    }
                    break;
                }

                default:
                    break;
            }
        }
    }

    // Phase 2: BFS flood-fill signal through connected wires (propagates max value)
    while (qHead < qTail) {
        int wx = bfsQueueX[qHead];
        int wy = bfsQueueY[qHead];
        int val = bfsSeedVal[wy][wx];
        qHead++;
        for (int d = 0; d < 4; d++) {
            int dx, dy;
            DirOffset((Direction)d, &dx, &dy);
            int nx = wx + dx, ny = wy + dy;
            if (InGrid(nx, ny) && grid[ny][nx].type == COMP_WIRE && newSig[ny][nx] < val) {
                newSig[ny][nx] = val;
                bfsSeedVal[ny][nx] = val;
                bfsQueueX[qTail] = nx;
                bfsQueueY[qTail] = ny;
                qTail++;
            }
        }
    }

    // Phase 3: Copy result into signal grid and update lights/displays
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
                    if (InGrid(nx, ny) && newSig[ny][nx] > sig)
                        sig = newSig[ny][nx];
                }
                c->signalIn = sig;
                c->state = (sig > 0);
            }
            if (c->type == COMP_DISPLAY) {
                // Read max adjacent signal value
                int maxSig = 0;
                for (int d = 0; d < 4; d++) {
                    int dx, dy;
                    DirOffset((Direction)d, &dx, &dy);
                    int nx = x + dx, ny = y + dy;
                    if (InGrid(nx, ny) && newSig[ny][nx] > maxSig)
                        maxSig = newSig[ny][nx];
                }
                c->signalIn = maxSig;
                c->setting = maxSig; // store for rendering
                c->state = (maxSig > 0);
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
// Simulation: Fluid pressure equalization
// ---------------------------------------------------------------------------
static bool IsFluidCell(ComponentType t) {
    return t == COMP_PIPE || t == COMP_PUMP || t == COMP_DRAIN ||
           t == COMP_VALVE || t == COMP_TANK || t == COMP_PRESSURE_LIGHT;
}

static bool IsValveOpen(int x, int y) {
    // Valve is open when any adjacent wire has signal
    for (int d = 0; d < 4; d++) {
        int dx, dy;
        DirOffset((Direction)d, &dx, &dy);
        int nx = x + dx, ny = y + dy;
        if (InGrid(nx, ny) && grid[ny][nx].type == COMP_WIRE && signalGrid[sigRead][ny][nx])
            return true;
    }
    return false;
}

static bool IsPumpActive(int x, int y) {
    // Pump is active if any adjacent wire has signal, or if no adjacent wire exists (standalone)
    bool hasWire = false;
    bool hasSignal = false;
    for (int d = 0; d < 4; d++) {
        int dx, dy;
        DirOffset((Direction)d, &dx, &dy);
        int nx = x + dx, ny = y + dy;
        if (InGrid(nx, ny) && grid[ny][nx].type == COMP_WIRE) {
            hasWire = true;
            if (signalGrid[sigRead][ny][nx]) hasSignal = true;
        }
    }
    return !hasWire || hasSignal;
}

static int FluidMaxLevel(ComponentType t) {
    return (t == COMP_TANK) ? 1024 : 255;
}

static void UpdateFluids(void) {
    int newFluid[GRID_H][GRID_W];
    // Copy current levels
    for (int y = 0; y < GRID_H; y++)
        for (int x = 0; x < GRID_W; x++)
            newFluid[y][x] = grid[y][x].fluidLevel;

    // Pressure equalization between connected fluid cells
    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            Cell *c = &grid[y][x];
            if (!IsFluidCell(c->type)) continue;
            if (c->type == COMP_VALVE && !IsValveOpen(x, y)) continue;

            int myLevel = c->fluidLevel;
            int myMax = FluidMaxLevel(c->type);

            // Gather connected fluid neighbors
            int neighborCount = 0;
            int neighborX[4], neighborY[4];
            for (int d = 0; d < 4; d++) {
                int dx, dy;
                DirOffset((Direction)d, &dx, &dy);
                int nx = x + dx, ny = y + dy;
                if (!InGrid(nx, ny)) continue;
                Cell *nc = &grid[ny][nx];
                if (!IsFluidCell(nc->type)) continue;
                if (nc->type == COMP_VALVE && !IsValveOpen(nx, ny)) continue;
                neighborX[neighborCount] = nx;
                neighborY[neighborCount] = ny;
                neighborCount++;
            }

            if (neighborCount == 0) continue;

            // Equalize: transfer pressure toward average
            for (int i = 0; i < neighborCount; i++) {
                int nx = neighborX[i], ny = neighborY[i];
                int nLevel = grid[ny][nx].fluidLevel;
                int diff = myLevel - nLevel;
                int transfer = diff / (neighborCount + 1);
                if (transfer > 0) {
                    newFluid[y][x] -= transfer;
                    newFluid[ny][nx] += transfer;
                    // Clamp both sides
                    int nMax = FluidMaxLevel(grid[ny][nx].type);
                    if (newFluid[ny][nx] > nMax) newFluid[ny][nx] = nMax;
                    if (newFluid[y][x] > myMax) newFluid[y][x] = myMax;
                    if (newFluid[y][x] < 0) newFluid[y][x] = 0;
                }
            }
        }
    }

    // Pumps: add pressure
    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            if (grid[y][x].type == COMP_PUMP && IsPumpActive(x, y)) {
                int rate = grid[y][x].setting * 8;
                newFluid[y][x] += rate;
                int mx = FluidMaxLevel(COMP_PUMP);
                if (newFluid[y][x] > mx) newFluid[y][x] = mx;
                grid[y][x].state = true;
            } else if (grid[y][x].type == COMP_PUMP) {
                grid[y][x].state = false;
            }
        }
    }

    // Drains: remove pressure
    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            if (grid[y][x].type == COMP_DRAIN) {
                int rate = grid[y][x].setting * 8;
                newFluid[y][x] -= rate;
                if (newFluid[y][x] < 0) newFluid[y][x] = 0;
            }
        }
    }

    // Valves: update state (open/closed) for rendering
    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            if (grid[y][x].type == COMP_VALVE) {
                grid[y][x].state = IsValveOpen(x, y);
            }
        }
    }

    // Pressure lights: check adjacent pipe pressure, emit signal if > threshold
    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            if (grid[y][x].type == COMP_PRESSURE_LIGHT) {
                int maxPressure = 0;
                for (int d = 0; d < 4; d++) {
                    int dx, dy;
                    DirOffset((Direction)d, &dx, &dy);
                    int nx = x + dx, ny = y + dy;
                    if (InGrid(nx, ny) && IsFluidCell(grid[ny][nx].type)) {
                        if (grid[ny][nx].fluidLevel > maxPressure)
                            maxPressure = grid[ny][nx].fluidLevel;
                    }
                }
                // Emit analog value proportional to pressure (0-15, scaled from 0-255)
                int analogOut = maxPressure / 17; // 255/15 ≈ 17
                if (analogOut > 15) analogOut = 15;
                grid[y][x].state = (analogOut > 0);
                grid[y][x].signalOut = analogOut;
                // Seed adjacent wires with analog value
                if (analogOut > 0) {
                    for (int d = 0; d < 4; d++) {
                        int dx, dy;
                        DirOffset((Direction)d, &dx, &dy);
                        int nx = x + dx, ny = y + dy;
                        if (InGrid(nx, ny) && grid[ny][nx].type == COMP_WIRE) {
                            if (signalGrid[sigRead][ny][nx] < analogOut)
                                signalGrid[sigRead][ny][nx] = analogOut;
                        }
                    }
                }
            }
        }
    }

    // Copy new fluid levels back
    for (int y = 0; y < GRID_H; y++)
        for (int x = 0; x < GRID_W; x++)
            grid[y][x].fluidLevel = newFluid[y][x];
}

// ---------------------------------------------------------------------------
// Simulation: Belt logistics (item transport)
// ---------------------------------------------------------------------------
static bool IsBeltTarget(ComponentType t) {
    return t == COMP_BELT || t == COMP_UNLOADER || t == COMP_SPLITTER || t == COMP_FILTER;
}

static Color CargoColor(int cargo) {
    // 15 distinct colors for cargo types 1-15
    switch (cargo) {
        case 1:  return RED;
        case 2:  return (Color){50, 200, 50, 255};   // green
        case 3:  return BLUE;
        case 4:  return YELLOW;
        case 5:  return PURPLE;
        case 6:  return ORANGE;
        case 7:  return (Color){0, 200, 200, 255};    // cyan
        case 8:  return WHITE;
        case 9:  return PINK;
        case 10: return (Color){180, 120, 60, 255};   // brown
        case 11: return LIME;
        case 12: return SKYBLUE;
        case 13: return MAGENTA;
        case 14: return GOLD;
        case 15: return MAROON;
        default: return GRAY;
    }
}

static void UpdateBelts(void) {
    // Snapshot cargo to avoid order-dependent movement
    int oldCargo[GRID_H][GRID_W];
    int oldCargo2[GRID_H][GRID_W];
    for (int y = 0; y < GRID_H; y++)
        for (int x = 0; x < GRID_W; x++) {
            oldCargo[y][x] = grid[y][x].cargo;
            oldCargo2[y][x] = grid[y][x].cargo2;
        }

    // Phase 1: Belts move cargo in facing direction
    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            if (grid[y][x].type != COMP_BELT) continue;
            if (oldCargo[y][x] == 0) continue;

            int dx, dy;
            DirOffset(grid[y][x].facing, &dx, &dy);
            int nx = x + dx, ny = y + dy;
            if (!InGrid(nx, ny)) continue;
            Cell *next = &grid[ny][nx];

            // Can push to belt, unloader, splitter, or filter
            if (IsBeltTarget(next->type) && next->cargo == 0) {
                next->cargo = oldCargo[y][x];
                next->cargo2 = oldCargo2[y][x];
                grid[y][x].cargo = 0;
                grid[y][x].cargo2 = 0;
            }
        }
    }

    // Phase 2: Filters — only pass matching cargo, block others
    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            if (grid[y][x].type != COMP_FILTER) continue;
            if (grid[y][x].cargo == 0) continue;

            // Only pass if cargo matches filter setting
            if (grid[y][x].cargo != grid[y][x].setting) continue;

            int dx, dy;
            DirOffset(grid[y][x].facing, &dx, &dy);
            int nx = x + dx, ny = y + dy;
            if (!InGrid(nx, ny)) continue;
            Cell *next = &grid[ny][nx];
            if (IsBeltTarget(next->type) && next->cargo == 0) {
                next->cargo = grid[y][x].cargo;
                grid[y][x].cargo = 0;
            }
        }
    }

    // Phase 3: Splitters — alternate left/right output
    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            if (grid[y][x].type != COMP_SPLITTER) continue;
            if (grid[y][x].cargo == 0) continue;

            Direction leftDir, rightDir;
            GateInputDirs(grid[y][x].facing, &rightDir, &leftDir);

            // Try preferred side first, then other
            Direction first = grid[y][x].altToggle ? leftDir : rightDir;
            Direction second = grid[y][x].altToggle ? rightDir : leftDir;

            int dx, dy;
            bool placed = false;
            DirOffset(first, &dx, &dy);
            int nx = x + dx, ny = y + dy;
            if (InGrid(nx, ny) && IsBeltTarget(grid[ny][nx].type) && grid[ny][nx].cargo == 0) {
                grid[ny][nx].cargo = grid[y][x].cargo;
                grid[y][x].cargo = 0;
                grid[y][x].altToggle = !grid[y][x].altToggle;
                placed = true;
            }
            if (!placed) {
                DirOffset(second, &dx, &dy);
                nx = x + dx; ny = y + dy;
                if (InGrid(nx, ny) && IsBeltTarget(grid[ny][nx].type) && grid[ny][nx].cargo == 0) {
                    grid[ny][nx].cargo = grid[y][x].cargo;
                    grid[y][x].cargo = 0;
                    grid[y][x].altToggle = !grid[y][x].altToggle;
                }
            }
        }
    }

    // Phase 3.5: Compressors — merge two side inputs into dual cargo forward
    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            if (grid[y][x].type != COMP_COMPRESSOR) continue;

            Direction rightDir, leftDir;
            GateInputDirs(grid[y][x].facing, &rightDir, &leftDir);

            // Check left input
            int ldx, ldy;
            DirOffset(leftDir, &ldx, &ldy);
            int lx = x + ldx, ly = y + ldy;
            int leftCargo = 0;
            if (InGrid(lx, ly) && grid[ly][lx].cargo > 0)
                leftCargo = grid[ly][lx].cargo;

            // Check right input
            int rdx, rdy;
            DirOffset(rightDir, &rdx, &rdy);
            int rx = x + rdx, ry = y + rdy;
            int rightCargo = 0;
            if (InGrid(rx, ry) && grid[ry][rx].cargo > 0)
                rightCargo = grid[ry][rx].cargo;

            // Need at least one input
            if (leftCargo == 0 && rightCargo == 0) continue;

            // Check if each side has a belt-type neighbor (potential input)
            bool leftHasBelt = InGrid(lx, ly) && grid[ly][lx].type != COMP_EMPTY;
            bool rightHasBelt = InGrid(rx, ry) && grid[ry][rx].type != COMP_EMPTY;

            // If both sides have belts, wait for both to have cargo before acting
            if (leftHasBelt && rightHasBelt && (leftCargo == 0 || rightCargo == 0)) continue;

            // Check forward output
            int fdx, fdy;
            DirOffset(grid[y][x].facing, &fdx, &fdy);
            int fx = x + fdx, fy = y + fdy;
            if (!InGrid(fx, fy)) continue;
            Cell *fwd = &grid[fy][fx];
            if (!IsBeltTarget(fwd->type) || fwd->cargo != 0) continue;

            if (leftCargo > 0 && rightCargo > 0) {
                // Both sides: compress into dual cargo
                fwd->cargo = leftCargo;
                fwd->cargo2 = rightCargo;
                grid[ly][lx].cargo = 0;
                grid[ry][rx].cargo = 0;
            } else if (leftCargo > 0) {
                // Only left side connected: pass through as single cargo
                fwd->cargo = leftCargo;
                grid[ly][lx].cargo = 0;
            } else {
                // Only right side connected: pass through as single cargo
                fwd->cargo = rightCargo;
                grid[ry][rx].cargo = 0;
            }
        }
    }

    // Phase 3.6: Decompressors — split dual cargo into forward + side
    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            if (grid[y][x].type != COMP_DECOMPRESSOR) continue;

            // Check back input
            int bdx, bdy;
            DirOffset(OppositeDir(grid[y][x].facing), &bdx, &bdy);
            int bx = x + bdx, by = y + bdy;
            if (!InGrid(bx, by)) continue;
            Cell *back = &grid[by][bx];
            if (back->cargo == 0) continue;

            // Check forward output
            int fdx, fdy;
            DirOffset(grid[y][x].facing, &fdx, &fdy);
            int fx = x + fdx, fy = y + fdy;
            if (!InGrid(fx, fy)) continue;
            Cell *fwd = &grid[fy][fx];
            if (!IsBeltTarget(fwd->type) || fwd->cargo != 0) continue;

            if (back->cargo2 > 0) {
                // Dual cargo: split forward + side (try preferred side, fall back to other)
                Direction rightDir, leftDir;
                GateInputDirs(grid[y][x].facing, &rightDir, &leftDir);
                Direction first = grid[y][x].altToggle ? leftDir : rightDir;
                Direction second = grid[y][x].altToggle ? rightDir : leftDir;

                Cell *side = NULL;
                int sdx, sdy;
                DirOffset(first, &sdx, &sdy);
                int sx = x + sdx, sy = y + sdy;
                if (InGrid(sx, sy) && IsBeltTarget(grid[sy][sx].type) && grid[sy][sx].cargo == 0) {
                    side = &grid[sy][sx];
                } else {
                    DirOffset(second, &sdx, &sdy);
                    sx = x + sdx; sy = y + sdy;
                    if (InGrid(sx, sy) && IsBeltTarget(grid[sy][sx].type) && grid[sy][sx].cargo == 0) {
                        side = &grid[sy][sx];
                    }
                }
                if (!side) continue;

                fwd->cargo = back->cargo;
                side->cargo = back->cargo2;
                back->cargo = 0;
                back->cargo2 = 0;
                grid[y][x].altToggle = !grid[y][x].altToggle;
            } else {
                // Single cargo: just pass through forward
                fwd->cargo = back->cargo;
                back->cargo = 0;
            }
        }
    }

    // Phase 4: Loaders — generate cargo into adjacent belt in facing direction
    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            if (grid[y][x].type != COMP_LOADER) continue;

            int dx, dy;
            DirOffset(grid[y][x].facing, &dx, &dy);
            int nx = x + dx, ny = y + dy;
            if (!InGrid(nx, ny)) continue;
            Cell *next = &grid[ny][nx];
            if (IsBeltTarget(next->type) && next->cargo == 0) {
                next->cargo = grid[y][x].setting;
                grid[y][x].state = true;
            } else {
                grid[y][x].state = false;
            }
        }
    }

    // Phase 5: Unloaders — consume cargo, emit signal value
    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            if (grid[y][x].type != COMP_UNLOADER) continue;
            if (grid[y][x].cargo > 0) {
                grid[y][x].signalOut = grid[y][x].cargo;
                grid[y][x].state = true;
                grid[y][x].cargo = 0;
                // Emit to adjacent wires
                for (int d = 0; d < 4; d++) {
                    int dx, dy;
                    DirOffset((Direction)d, &dx, &dy);
                    int nx = x + dx, ny = y + dy;
                    if (InGrid(nx, ny) && grid[ny][nx].type == COMP_WIRE) {
                        if (signalGrid[sigRead][ny][nx] < grid[y][x].signalOut)
                            signalGrid[sigRead][ny][nx] = grid[y][x].signalOut;
                    }
                }
            } else {
                grid[y][x].signalOut = 0;
                grid[y][x].state = false;
            }
        }
    }

    // Phase 6: Grabbers — move cargo from back cell to front cell (signal-controlled)
    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            if (grid[y][x].type != COMP_GRABBER) continue;

            // Check if signal-controlled: active if adjacent wire has signal, or no wire = always on
            bool hasWire = false, hasSignal = false;
            for (int d = 0; d < 4; d++) {
                int dx, dy;
                DirOffset((Direction)d, &dx, &dy);
                int nx = x + dx, ny = y + dy;
                if (InGrid(nx, ny) && grid[ny][nx].type == COMP_WIRE) {
                    hasWire = true;
                    if (signalGrid[sigRead][ny][nx] > 0) hasSignal = true;
                }
            }
            bool active = !hasWire || hasSignal;
            grid[y][x].state = active;
            if (!active) continue;

            // Grab from back, place on front
            int dx, dy;
            DirOffset(OppositeDir(grid[y][x].facing), &dx, &dy);
            int srcX = x + dx, srcY = y + dy;
            DirOffset(grid[y][x].facing, &dx, &dy);
            int dstX = x + dx, dstY = y + dy;

            if (!InGrid(srcX, srcY) || !InGrid(dstX, dstY)) continue;

            Cell *src = &grid[srcY][srcX];
            Cell *dst = &grid[dstY][dstX];

            // Source must have cargo (belt/filter/splitter types)
            int srcCargo = src->cargo;
            if (srcCargo == 0) continue;

            // Destination must accept cargo
            if (IsBeltTarget(dst->type) && dst->cargo == 0) {
                dst->cargo = srcCargo;
                src->cargo = 0;
            }
        }
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
        case COMP_PULSE:     return state ? (Color){255, 100, 255, 255} : (Color){100, 40, 100, 255};
        case COMP_PIPE:      return (Color){30, 60, 160, 255};
        case COMP_PUMP:      return state ? (Color){30, 180, 160, 255} : (Color){20, 80, 70, 255};
        case COMP_DRAIN:     return (Color){20, 40, 120, 255};
        case COMP_VALVE:     return state ? (Color){30, 100, 200, 255} : (Color){60, 40, 40, 255};
        case COMP_TANK:      return (Color){20, 40, 100, 255};
        case COMP_PRESSURE_LIGHT: return state ? (Color){50, 200, 230, 255} : (Color){20, 60, 80, 255};
        case COMP_DIAL:      return (Color){200, 160, 40, 255};
        case COMP_COMPARATOR:return state ? (Color){220, 120, 40, 255} : (Color){100, 55, 20, 255};
        case COMP_DISPLAY:   return (Color){20, 20, 30, 255};
        case COMP_BELT:      return (Color){100, 90, 60, 255};
        case COMP_LOADER:    return state ? (Color){60, 160, 60, 255} : (Color){40, 80, 40, 255};
        case COMP_UNLOADER:  return state ? (Color){160, 60, 60, 255} : (Color){80, 40, 40, 255};
        case COMP_GRABBER:   return state ? (Color){160, 140, 40, 255} : (Color){80, 70, 20, 255};
        case COMP_SPLITTER:  return (Color){80, 80, 120, 255};
        case COMP_FILTER:    return (Color){120, 80, 100, 255};
        case COMP_COMPRESSOR:   return (Color){100, 80, 120, 255};
        case COMP_DECOMPRESSOR: return (Color){80, 100, 120, 255};
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
                    int sigVal = signalGrid[sigRead][y][x];
                    if (sigVal > 0) {
                        float intensity = (float)sigVal / 15.0f;
                        float pulse = 0.6f + 0.4f * sinf(pulseTime * 6.0f);
                        unsigned char g = (unsigned char)(80 + 175 * intensity * pulse);
                        unsigned char r = (unsigned char)(40 * intensity * pulse);
                        wireCol = (Color){r, g, 0, 255};
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

                case COMP_REPEATER:
                case COMP_PULSE: {
                    DrawRectangleRec(r, col);
                    DrawArrow(cx, cy, c->facing, WHITE);
                    int edge = CELL_SIZE / 2 - 1;
                    {
                        int dx, dy;
                        DirOffset(c->facing, &dx, &dy);
                        DrawCircle(cx + dx * edge, cy + dy * edge, 3, GREEN);
                        DirOffset(OppositeDir(c->facing), &dx, &dy);
                        DrawCircle(cx + dx * edge, cy + dy * edge, 3, ORANGE);
                    }
                    char delBuf[4];
                    snprintf(delBuf, sizeof(delBuf), "%d", c->setting);
                    DrawTextShadow(delBuf, cx - 3, cy - 5, 10, WHITE);
                    break;
                }

                case COMP_PIPE: {
                    // Blue rectangle, brightness scales with pressure
                    int level = c->fluidLevel;
                    int maxLvl = FluidMaxLevel(COMP_PIPE);
                    float frac = (float)level / (float)maxLvl;
                    unsigned char b = (unsigned char)(80 + 175 * frac);
                    unsigned char g = (unsigned char)(40 + 180 * frac);
                    Color pipeCol = {0, g, b, 255};
                    DrawRectangleRec(r, pipeCol);
                    // Draw connections to adjacent fluid cells
                    for (int d = 0; d < 4; d++) {
                        int dx, dy;
                        DirOffset((Direction)d, &dx, &dy);
                        int nx = x + dx, ny = y + dy;
                        if (InGrid(nx, ny) && IsFluidCell(grid[ny][nx].type)) {
                            int ex = cx + dx * (CELL_SIZE / 2);
                            int ey = cy + dy * (CELL_SIZE / 2);
                            DrawLineEx((Vector2){(float)cx, (float)cy},
                                       (Vector2){(float)ex, (float)ey}, 3.0f, pipeCol);
                        }
                    }
                    break;
                }

                case COMP_PUMP: {
                    DrawRectangleRec(r, col);
                    char pBuf[8];
                    snprintf(pBuf, sizeof(pBuf), "P%d", c->setting);
                    DrawTextShadow(pBuf, cx - 6, cy - 5, 10, WHITE);
                    break;
                }

                case COMP_DRAIN: {
                    DrawRectangleRec(r, col);
                    char dBuf[8];
                    snprintf(dBuf, sizeof(dBuf), "D%d", c->setting);
                    DrawTextShadow(dBuf, cx - 6, cy - 5, 10, WHITE);
                    break;
                }

                case COMP_VALVE: {
                    // Pipe-colored when open, dark when closed, with direction arrow
                    int level = c->fluidLevel;
                    float frac = (float)level / 255.0f;
                    Color vCol = c->state
                        ? (Color){0, (unsigned char)(60 + 160 * frac), (unsigned char)(100 + 155 * frac), 255}
                        : (Color){60, 40, 40, 255};
                    DrawRectangleRec(r, vCol);
                    DrawArrow(cx, cy, c->facing, WHITE);
                    // Draw bar across when closed
                    if (!c->state) {
                        int dx, dy;
                        DirOffset(c->facing, &dx, &dy);
                        int perpX = -dy, perpY = dx;
                        int s = CELL_SIZE / 2 - 2;
                        DrawLineEx((Vector2){(float)(cx + perpX * s), (float)(cy + perpY * s)},
                                   (Vector2){(float)(cx - perpX * s), (float)(cy - perpY * s)}, 3.0f, RED);
                    }
                    DrawTextShadow("V", cx - 3, cy - 5, 10, WHITE);
                    break;
                }

                case COMP_TANK: {
                    int level = c->fluidLevel;
                    int maxLvl = FluidMaxLevel(COMP_TANK);
                    float frac = (float)level / (float)maxLvl;
                    unsigned char b = (unsigned char)(40 + 180 * frac);
                    unsigned char g = (unsigned char)(20 + 120 * frac);
                    Color tankCol = {0, g, b, 255};
                    // Slightly larger visual
                    Rectangle tr = {r.x - 1, r.y - 1, r.width + 2, r.height + 2};
                    DrawRectangleRec(tr, tankCol);
                    DrawRectangleLinesEx(r, 1, (Color){60, 60, 100, 255});
                    char tBuf[8];
                    snprintf(tBuf, sizeof(tBuf), "T");
                    DrawTextShadow(tBuf, cx - 3, cy - 5, 10, WHITE);
                    break;
                }

                case COMP_PRESSURE_LIGHT: {
                    DrawCircle(cx, cy, CELL_SIZE / 2 - 1, col);
                    if (c->state) {
                        DrawCircle(cx, cy, CELL_SIZE / 2 + 3, (Color){50, 200, 230, 40});
                    }
                    break;
                }

                case COMP_DIAL: {
                    // Golden knob showing value
                    DrawRectangleRec(r, col);
                    char dialBuf[4];
                    snprintf(dialBuf, sizeof(dialBuf), "%d", c->setting);
                    int tw = MeasureTextUI(dialBuf, 10);
                    DrawTextShadow(dialBuf, cx - tw / 2, cy - 5, 10, BLACK);
                    break;
                }

                case COMP_COMPARATOR: {
                    DrawRectangleRec(r, col);
                    DrawArrow(cx, cy, c->facing, WHITE);
                    int edge = CELL_SIZE / 2 - 1;
                    {
                        int dx, dy;
                        DirOffset(c->facing, &dx, &dy);
                        DrawCircle(cx + dx * edge, cy + dy * edge, 3, GREEN);
                        DirOffset(OppositeDir(c->facing), &dx, &dy);
                        DrawCircle(cx + dx * edge, cy + dy * edge, 3, ORANGE);
                    }
                    // Show threshold
                    char cmpBuf[4];
                    snprintf(cmpBuf, sizeof(cmpBuf), "%d", c->setting);
                    DrawTextShadow(cmpBuf, cx - 3, cy - 5, 10, WHITE);
                    break;
                }

                case COMP_DISPLAY: {
                    // Dark screen with big number
                    DrawRectangleRec(r, col);
                    DrawRectangleLinesEx(r, 1, (Color){60, 60, 70, 255});
                    int val = c->setting; // set by UpdateSignals
                    char dispBuf[4];
                    if (val > 9) {
                        snprintf(dispBuf, sizeof(dispBuf), "%X", val); // hex for 10-15
                    } else {
                        snprintf(dispBuf, sizeof(dispBuf), "%d", val);
                    }
                    Color numCol = val > 0
                        ? (Color){(unsigned char)(80 + 175.0f * val / 15.0f),
                                  (unsigned char)(200.0f * val / 15.0f),
                                  40, 255}
                        : (Color){50, 50, 50, 255};
                    int tw = MeasureTextUI(dispBuf, 14);
                    DrawTextShadow(dispBuf, cx - tw / 2, cy - 7, 14, numCol);
                    break;
                }

                case COMP_BELT: {
                    DrawRectangleRec(r, col);
                    // Animated chevrons showing direction
                    float anim = fmodf(pulseTime * 3.0f, 1.0f);
                    int dx, dy;
                    DirOffset(c->facing, &dx, &dy);
                    for (int i = 0; i < 2; i++) {
                        float offset = (anim + i * 0.5f);
                        if (offset > 1.0f) offset -= 1.0f;
                        float px = (float)cx + dx * (offset - 0.5f) * CELL_SIZE * 0.8f;
                        float py = (float)cy + dy * (offset - 0.5f) * CELL_SIZE * 0.8f;
                        unsigned char a = (unsigned char)(100 + 100 * (1.0f - offset));
                        Color chevCol = {a, a, (unsigned char)(a / 2), 255};
                        int perpX = -dy, perpY = dx;
                        DrawLineEx((Vector2){px - perpX * 3 - dx * 2, py - perpY * 3 - dy * 2},
                                   (Vector2){px, py}, 1.5f, chevCol);
                        DrawLineEx((Vector2){px + perpX * 3 - dx * 2, py + perpY * 3 - dy * 2},
                                   (Vector2){px, py}, 1.5f, chevCol);
                    }
                    // Draw cargo dot(s)
                    if (c->cargo > 0 && c->cargo2 > 0) {
                        // Dual cargo: offset along perpendicular to belt direction
                        int pdx = -dy, pdy = dx;
                        DrawCircle(cx - pdx * 3, cy - pdy * 3, 3, CargoColor(c->cargo));
                        DrawCircleLines(cx - pdx * 3, cy - pdy * 3, 3, BLACK);
                        DrawCircle(cx + pdx * 3, cy + pdy * 3, 3, CargoColor(c->cargo2));
                        DrawCircleLines(cx + pdx * 3, cy + pdy * 3, 3, BLACK);
                    } else if (c->cargo > 0) {
                        DrawCircle(cx, cy, 4, CargoColor(c->cargo));
                        DrawCircleLines(cx, cy, 4, BLACK);
                    }
                    break;
                }

                case COMP_LOADER: {
                    DrawRectangleRec(r, col);
                    DrawArrow(cx, cy, c->facing, WHITE);
                    // Show cargo type as colored dot
                    DrawCircle(cx, cy - 2, 3, CargoColor(c->setting));
                    char ldBuf[4];
                    snprintf(ldBuf, sizeof(ldBuf), "%d", c->setting);
                    DrawTextShadow(ldBuf, cx + 3, cy - 5, 10, WHITE);
                    break;
                }

                case COMP_UNLOADER: {
                    DrawRectangleRec(r, col);
                    // Down arrow / sink indicator
                    DrawTextShadow("U", cx - 3, cy - 5, 10, WHITE);
                    if (c->cargo > 0) {
                        DrawCircle(cx, cy + 4, 3, CargoColor(c->cargo));
                    }
                    break;
                }

                case COMP_GRABBER: {
                    DrawRectangleRec(r, col);
                    // Draw grabber arm: line from back to front
                    int dx, dy;
                    DirOffset(c->facing, &dx, &dy);
                    int edge = CELL_SIZE / 2 - 2;
                    DrawLineEx((Vector2){(float)(cx - dx * edge), (float)(cy - dy * edge)},
                               (Vector2){(float)(cx + dx * edge), (float)(cy + dy * edge)},
                               3.0f, c->state ? YELLOW : GRAY);
                    // Arrow tip
                    DrawCircle(cx + dx * edge, cy + dy * edge, 3, c->state ? GREEN : GRAY);
                    DrawCircle(cx - dx * edge, cy - dy * edge, 3, ORANGE);
                    break;
                }

                case COMP_SPLITTER: {
                    DrawRectangleRec(r, col);
                    // Y-shape: input from back, outputs to sides
                    int dx, dy;
                    DirOffset(OppositeDir(c->facing), &dx, &dy);
                    int edge = CELL_SIZE / 2 - 2;
                    DrawLineEx((Vector2){(float)(cx + dx * edge), (float)(cy + dy * edge)},
                               (Vector2){(float)cx, (float)cy}, 2.0f, WHITE);
                    Direction leftDir, rightDir;
                    GateInputDirs(c->facing, &rightDir, &leftDir);
                    DirOffset(leftDir, &dx, &dy);
                    DrawLineEx((Vector2){(float)cx, (float)cy},
                               (Vector2){(float)(cx + dx * edge), (float)(cy + dy * edge)}, 2.0f, WHITE);
                    DirOffset(rightDir, &dx, &dy);
                    DrawLineEx((Vector2){(float)cx, (float)cy},
                               (Vector2){(float)(cx + dx * edge), (float)(cy + dy * edge)}, 2.0f, WHITE);
                    DrawTextShadow("Y", cx - 3, cy - 5, 10, WHITE);
                    if (c->cargo > 0) {
                        DrawCircle(cx, cy, 3, CargoColor(c->cargo));
                    }
                    break;
                }

                case COMP_FILTER: {
                    DrawRectangleRec(r, col);
                    DrawArrow(cx, cy, c->facing, WHITE);
                    // Show filter type as colored dot
                    DrawCircle(cx, cy - 2, 3, CargoColor(c->setting));
                    char fBuf[4];
                    snprintf(fBuf, sizeof(fBuf), "F%d", c->setting);
                    DrawTextShadow(fBuf, cx - 5, cy - 5, 10, WHITE);
                    if (c->cargo > 0 && c->cargo != c->setting) {
                        // Blocked cargo shown dimmer
                        DrawCircle(cx - 4, cy + 4, 2, CargoColor(c->cargo));
                    }
                    break;
                }

                case COMP_COMPRESSOR: {
                    DrawRectangleRec(r, col);
                    int dx, dy;
                    int edge = CELL_SIZE / 2 - 2;
                    // Draw reverse-Y: two side inputs merging to one forward output
                    Direction rightDir, leftDir;
                    GateInputDirs(c->facing, &rightDir, &leftDir);
                    DirOffset(leftDir, &dx, &dy);
                    DrawLineEx((Vector2){(float)(cx + dx * edge), (float)(cy + dy * edge)},
                               (Vector2){(float)cx, (float)cy}, 2.0f, WHITE);
                    DirOffset(rightDir, &dx, &dy);
                    DrawLineEx((Vector2){(float)(cx + dx * edge), (float)(cy + dy * edge)},
                               (Vector2){(float)cx, (float)cy}, 2.0f, WHITE);
                    DirOffset(c->facing, &dx, &dy);
                    DrawLineEx((Vector2){(float)cx, (float)cy},
                               (Vector2){(float)(cx + dx * edge), (float)(cy + dy * edge)}, 2.0f, WHITE);
                    // Port dots
                    DirOffset(leftDir, &dx, &dy);
                    DrawCircle(cx + dx * edge, cy + dy * edge, 3, ORANGE);
                    DirOffset(rightDir, &dx, &dy);
                    DrawCircle(cx + dx * edge, cy + dy * edge, 3, ORANGE);
                    DirOffset(c->facing, &dx, &dy);
                    DrawCircle(cx + dx * edge, cy + dy * edge, 3, GREEN);
                    DrawTextShadow("><", cx - 5, cy - 5, 10, WHITE);
                    break;
                }

                case COMP_DECOMPRESSOR: {
                    DrawRectangleRec(r, col);
                    int dx, dy;
                    int edge = CELL_SIZE / 2 - 2;
                    // Draw Y: one back input splitting to forward + side
                    DirOffset(OppositeDir(c->facing), &dx, &dy);
                    DrawLineEx((Vector2){(float)(cx + dx * edge), (float)(cy + dy * edge)},
                               (Vector2){(float)cx, (float)cy}, 2.0f, WHITE);
                    DirOffset(c->facing, &dx, &dy);
                    DrawLineEx((Vector2){(float)cx, (float)cy},
                               (Vector2){(float)(cx + dx * edge), (float)(cy + dy * edge)}, 2.0f, WHITE);
                    Direction rightDir, leftDir;
                    GateInputDirs(c->facing, &rightDir, &leftDir);
                    DirOffset(rightDir, &dx, &dy);
                    DrawLineEx((Vector2){(float)cx, (float)cy},
                               (Vector2){(float)(cx + dx * edge), (float)(cy + dy * edge)}, 2.0f, WHITE);
                    // Port dots
                    DirOffset(OppositeDir(c->facing), &dx, &dy);
                    DrawCircle(cx + dx * edge, cy + dy * edge, 3, ORANGE);
                    DirOffset(c->facing, &dx, &dy);
                    DrawCircle(cx + dx * edge, cy + dy * edge, 3, GREEN);
                    DirOffset(rightDir, &dx, &dy);
                    DrawCircle(cx + dx * edge, cy + dy * edge, 3, GREEN);
                    DrawTextShadow("<>", cx - 5, cy - 5, 10, WHITE);
                    break;
                }

                default:
                    break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// UI: Bottom palette bar (two rows: electrical + fluid)
// ---------------------------------------------------------------------------
#define PALETTE_ROW_H  24
#define PALETTE_PAD    4
#define PALETTE_ROWS   4
#define PALETTE_BAR_H  (PALETTE_ROW_H * PALETTE_ROWS + PALETTE_PAD * (PALETTE_ROWS + 1))

// Row 1: Electrical/signal components
static const ComponentType electricalItems[] = {
    COMP_SWITCH, COMP_BUTTON, COMP_LIGHT, COMP_WIRE, COMP_NOT,
    COMP_AND, COMP_OR, COMP_XOR, COMP_NOR, COMP_LATCH, COMP_CLOCK, COMP_REPEATER, COMP_PULSE,
    COMP_DIAL, COMP_COMPARATOR
};
static const char *electricalKeys[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "Q", "W", "E", "A", "X", "V" };
#define ELECTRICAL_COUNT (int)(sizeof(electricalItems) / sizeof(electricalItems[0]))

// Row 2: Fluid components
static const ComponentType fluidItems[] = {
    COMP_PIPE, COMP_PUMP, COMP_DRAIN, COMP_VALVE, COMP_TANK, COMP_PRESSURE_LIGHT
};
static const char *fluidKeys[] = { "S", "D", "G", "H", "J", "K" };
#define FLUID_COUNT (int)(sizeof(fluidItems) / sizeof(fluidItems[0]))

// Row 3: Belt/logistics components
static const ComponentType beltItems[] = {
    COMP_BELT, COMP_LOADER, COMP_UNLOADER, COMP_GRABBER, COMP_SPLITTER, COMP_FILTER,
    COMP_COMPRESSOR, COMP_DECOMPRESSOR
};
static const char *beltKeys[] = { ",", ".", "/", ";", "'", "\\", "[", "]" };
#define BELT_COUNT (int)(sizeof(beltItems) / sizeof(beltItems[0]))

// Row 4: Processor + Display + Eraser
static const ComponentType processorItems[] = {
    COMP_PROCESSOR, COMP_DISPLAY, COMP_EMPTY
};
static const char *processorKeys[] = { "Z", "B", "0" };
#define PROCESSOR_COUNT (int)(sizeof(processorItems) / sizeof(processorItems[0]))

static void DrawPaletteRow(const ComponentType *items, const char **keyLabels, int count,
                           int rowY, Color tintColor) {
    for (int i = 0; i < count; i++) {
        int bx = 6 + i * (SCREEN_WIDTH - 12) / count;
        int itemW = (SCREEN_WIDTH - 12) / count - 4;
        bool sel = (selectedComp == items[i]);
        Color bg = sel ? tintColor : (Color){40, 40, 45, 255};
        DrawRectangle(bx, rowY, itemW, PALETTE_ROW_H, bg);
        if (sel) DrawRectangleLinesEx((Rectangle){(float)bx, (float)rowY, (float)itemW, (float)PALETTE_ROW_H}, 2, WHITE);

        char label[32];
        snprintf(label, sizeof(label), "%s:%s", keyLabels[i], CompName(items[i]));
        DrawTextShadow(label, bx + 4, rowY + 7, 10, WHITE);
    }
}

static bool HandlePaletteRowClick(const ComponentType *items, int count, int rowY, int mx, int my) {
    if (my < rowY || my > rowY + PALETTE_ROW_H) return false;
    for (int i = 0; i < count; i++) {
        int bx = 6 + i * (SCREEN_WIDTH - 12) / count;
        int itemW = (SCREEN_WIDTH - 12) / count - 4;
        if (mx >= bx && mx <= bx + itemW) {
            selectedComp = items[i];
            return true;
        }
    }
    return false;
}

static void DrawPalette(void) {
    int barY = SCREEN_HEIGHT - PALETTE_BAR_H;
    DrawRectangle(0, barY, SCREEN_WIDTH, PALETTE_BAR_H, (Color){20, 20, 25, 255});

    if (selectedPreset >= 0) {
        // Preset mode: show presets across entire bar
        int rowY = barY + PALETTE_PAD;
        int rowH = PALETTE_BAR_H - PALETTE_PAD * 2;
        for (int i = 0; i < (int)PRESET_COUNT; i++) {
            int bx = 6 + i * (SCREEN_WIDTH - 12) / (int)PRESET_COUNT;
            int itemW = (SCREEN_WIDTH - 12) / (int)PRESET_COUNT - 4;
            bool sel = (i == selectedPreset);
            Color bg = sel ? (Color){80, 60, 20, 255} : (Color){40, 40, 45, 255};
            DrawRectangle(bx, rowY, itemW, rowH, bg);
            if (sel) DrawRectangleLinesEx((Rectangle){(float)bx, (float)rowY, (float)itemW, (float)rowH}, 2, YELLOW);

            char label[48];
            snprintf(label, sizeof(label), "%d:%s", i + 1, presets[i].name);
            DrawTextShadow(label, bx + 4, rowY + 5, 10, WHITE);
            DrawTextShadow(presets[i].description, bx + 4, rowY + 19, 10, (Color){160, 160, 160, 255});
        }
    } else {
        // Row 1: Electrical/signal
        int row1Y = barY + PALETTE_PAD;
        DrawPaletteRow(electricalItems, electricalKeys, ELECTRICAL_COUNT,
                       row1Y, (Color){70, 70, 80, 255});

        // Row 2: Fluid — tinted blue
        int row2Y = row1Y + PALETTE_ROW_H + PALETTE_PAD;
        DrawPaletteRow(fluidItems, fluidKeys, FLUID_COUNT,
                       row2Y, (Color){40, 60, 90, 255});

        // Row 3: Belt/logistics — tinted brown
        int row3Y = row2Y + PALETTE_ROW_H + PALETTE_PAD;
        DrawPaletteRow(beltItems, beltKeys, BELT_COUNT,
                       row3Y, (Color){80, 70, 40, 255});

        // Row 4: Processor + Display + Eraser — tinted purple
        int row4Y = row3Y + PALETTE_ROW_H + PALETTE_PAD;
        DrawPaletteRow(processorItems, processorKeys, PROCESSOR_COUNT,
                       row4Y, (Color){70, 40, 90, 255});

        // Row labels
        DrawTextShadow("SIGNAL", SCREEN_WIDTH - 52, row1Y + 7, 10, (Color){100, 100, 110, 255});
        DrawTextShadow("FLUID", SCREEN_WIDTH - 48, row2Y + 7, 10, (Color){60, 100, 140, 255});
        DrawTextShadow("BELT", SCREEN_WIDTH - 40, row3Y + 7, 10, (Color){140, 120, 60, 255});
        DrawTextShadow("CPU", SCREEN_WIDTH - 36, row4Y + 7, 10, (Color){120, 80, 160, 255});
    }
}

// Returns true if click was consumed by palette
static bool HandlePaletteClick(int mx, int my) {
    int barY = SCREEN_HEIGHT - PALETTE_BAR_H;
    if (my < barY) return false;

    if (selectedPreset >= 0) {
        // Preset mode click
        int rowY = barY + PALETTE_PAD;
        int rowH = PALETTE_BAR_H - PALETTE_PAD * 2;
        if (my >= rowY && my <= rowY + rowH) {
            for (int i = 0; i < (int)PRESET_COUNT; i++) {
                int bx = 6 + i * (SCREEN_WIDTH - 12) / (int)PRESET_COUNT;
                int itemW = (SCREEN_WIDTH - 12) / (int)PRESET_COUNT - 4;
                if (mx >= bx && mx <= bx + itemW) {
                    selectedPreset = i;
                    return true;
                }
            }
        }
        return true; // consumed even if not on a button
    }

    int row1Y = barY + PALETTE_PAD;
    int row2Y = row1Y + PALETTE_ROW_H + PALETTE_PAD;
    int row3Y = row2Y + PALETTE_ROW_H + PALETTE_PAD;
    int row4Y = row3Y + PALETTE_ROW_H + PALETTE_PAD;
    if (HandlePaletteRowClick(electricalItems, ELECTRICAL_COUNT, row1Y, mx, my)) return true;
    if (HandlePaletteRowClick(fluidItems, FLUID_COUNT, row2Y, mx, my)) return true;
    if (HandlePaletteRowClick(beltItems, BELT_COUNT, row3Y, mx, my)) return true;
    if (HandlePaletteRowClick(processorItems, PROCESSOR_COUNT, row4Y, mx, my)) return true;
    return true; // consumed (in bar area)
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
    } else if (c->type == COMP_PULSE) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] state=%s duration=%d timer=%d dir=%s (click to change duration)",
                 CompName(c->type), gx, gy,
                 c->state ? "ON" : "OFF",
                 c->setting, c->timer, DirName(c->facing));
    } else if (c->type == COMP_PIPE || c->type == COMP_TANK) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] pressure=%d/%d",
                 CompName(c->type), gx, gy,
                 c->fluidLevel, FluidMaxLevel(c->type));
    } else if (c->type == COMP_PUMP) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] rate=%d active=%s pressure=%d (click to change rate)",
                 CompName(c->type), gx, gy,
                 c->setting, c->state ? "YES" : "NO", c->fluidLevel);
    } else if (c->type == COMP_DRAIN) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] rate=%d pressure=%d (click to change rate)",
                 CompName(c->type), gx, gy,
                 c->setting, c->fluidLevel);
    } else if (c->type == COMP_VALVE) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] %s pressure=%d dir=%s",
                 CompName(c->type), gx, gy,
                 c->state ? "OPEN" : "CLOSED", c->fluidLevel, DirName(c->facing));
    } else if (c->type == COMP_PRESSURE_LIGHT) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] %s out=%d (pressure->analog 0-15)",
                 CompName(c->type), gx, gy,
                 c->state ? "ON" : "OFF", c->signalOut);
    } else if (c->type == COMP_DIAL) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] value=%d (click to change 0-15)",
                 CompName(c->type), gx, gy, c->setting);
    } else if (c->type == COMP_COMPARATOR) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] %s threshold=%d input=%d dir=%s (click to change)",
                 CompName(c->type), gx, gy,
                 c->state ? "ON" : "OFF",
                 c->setting, c->signalIn, DirName(c->facing));
    } else if (c->type == COMP_DISPLAY) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] value=%d",
                 CompName(c->type), gx, gy, c->setting);
    } else if (c->type == COMP_BELT) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] dir=%s cargo=%d",
                 CompName(c->type), gx, gy, DirName(c->facing), c->cargo);
    } else if (c->type == COMP_LOADER) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] type=%d dir=%s (click to change type)",
                 CompName(c->type), gx, gy, c->setting, DirName(c->facing));
    } else if (c->type == COMP_UNLOADER) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] last=%d (consumes cargo, emits signal)",
                 CompName(c->type), gx, gy, c->signalOut);
    } else if (c->type == COMP_GRABBER) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] %s dir=%s (signal-controlled inserter)",
                 CompName(c->type), gx, gy, c->state ? "ACTIVE" : "IDLE", DirName(c->facing));
    } else if (c->type == COMP_SPLITTER) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] dir=%s next=%s cargo=%d",
                 CompName(c->type), gx, gy, DirName(c->facing),
                 c->altToggle ? "LEFT" : "RIGHT", c->cargo);
    } else if (c->type == COMP_FILTER) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] pass=%d dir=%s cargo=%d (click to change)",
                 CompName(c->type), gx, gy, c->setting, DirName(c->facing), c->cargo);
    } else if (c->type == COMP_COMPRESSOR) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] dir=%s (merges 2 side inputs into dual cargo)",
                 CompName(c->type), gx, gy, DirName(c->facing));
    } else if (c->type == COMP_DECOMPRESSOR) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] dir=%s side=%s (splits dual cargo to fwd+side)",
                 CompName(c->type), gx, gy, DirName(c->facing),
                 c->altToggle ? "LEFT" : "RIGHT");
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

    // Palette click — consume mouse clicks on the bottom bar
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mode == MODE_PLACE) {
        if (HandlePaletteClick(mx, my)) return;
    }

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
        if (IsKeyPressed(KEY_SEVEN) && PRESET_COUNT > 6) selectedPreset = 6;
        if (IsKeyPressed(KEY_EIGHT) && PRESET_COUNT > 7) selectedPreset = 7;
        if (IsKeyPressed(KEY_NINE) && PRESET_COUNT > 8)  selectedPreset = 8;
        if (IsKeyPressed(KEY_ZERO) && PRESET_COUNT > 9)  selectedPreset = 9;

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
        if (IsKeyPressed(KEY_A))     selectedComp = COMP_PULSE;
        if (IsKeyPressed(KEY_S))     selectedComp = COMP_PIPE;
        if (IsKeyPressed(KEY_D))     selectedComp = COMP_PUMP;
        if (IsKeyPressed(KEY_G))     selectedComp = COMP_DRAIN;
        if (IsKeyPressed(KEY_H))     selectedComp = COMP_VALVE;
        if (IsKeyPressed(KEY_J))     selectedComp = COMP_TANK;
        if (IsKeyPressed(KEY_K))     selectedComp = COMP_PRESSURE_LIGHT;
        if (IsKeyPressed(KEY_X))     selectedComp = COMP_DIAL;
        if (IsKeyPressed(KEY_V))     selectedComp = COMP_COMPARATOR;
        if (IsKeyPressed(KEY_Z))     selectedComp = COMP_PROCESSOR;
        if (IsKeyPressed(KEY_B))     selectedComp = COMP_DISPLAY;
        if (IsKeyPressed(KEY_COMMA))      selectedComp = COMP_BELT;
        if (IsKeyPressed(KEY_PERIOD))     selectedComp = COMP_LOADER;
        if (IsKeyPressed(KEY_SLASH))      selectedComp = COMP_UNLOADER;
        if (IsKeyPressed(KEY_SEMICOLON))  selectedComp = COMP_GRABBER;
        if (IsKeyPressed(KEY_APOSTROPHE)) selectedComp = COMP_SPLITTER;
        if (IsKeyPressed(KEY_BACKSLASH))  selectedComp = COMP_FILTER;
        if (IsKeyPressed(KEY_LEFT_BRACKET))  selectedComp = COMP_COMPRESSOR;
        if (IsKeyPressed(KEY_RIGHT_BRACKET)) selectedComp = COMP_DECOMPRESSOR;
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
        UpdateFluids();
        UpdateBelts();
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
                } else if (selectedComp == COMP_PULSE && clicked->type == COMP_PULSE) {
                    clicked->setting = (clicked->setting % 8) + 1;
                } else if (selectedComp == COMP_PUMP && clicked->type == COMP_PUMP) {
                    clicked->setting = (clicked->setting % 8) + 1;
                } else if (selectedComp == COMP_DRAIN && clicked->type == COMP_DRAIN) {
                    clicked->setting = (clicked->setting % 8) + 1;
                } else if (selectedComp == COMP_DIAL && clicked->type == COMP_DIAL) {
                    clicked->setting = (clicked->setting + 1) % 16; // 0-15
                } else if (selectedComp == COMP_COMPARATOR && clicked->type == COMP_COMPARATOR) {
                    clicked->setting = (clicked->setting % 15) + 1; // 1-15
                } else if (selectedComp == COMP_LOADER && clicked->type == COMP_LOADER) {
                    clicked->setting = (clicked->setting % 15) + 1; // 1-15
                } else if (selectedComp == COMP_FILTER && clicked->type == COMP_FILTER) {
                    clicked->setting = (clicked->setting % 15) + 1; // 1-15
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
                                      || (selectedComp == COMP_REPEATER && clicked->type == COMP_REPEATER)
                                      || (selectedComp == COMP_PULSE && clicked->type == COMP_PULSE)
                                      || (selectedComp == COMP_PUMP && clicked->type == COMP_PUMP)
                                      || (selectedComp == COMP_DRAIN && clicked->type == COMP_DRAIN)
                                      || (selectedComp == COMP_DIAL && clicked->type == COMP_DIAL)
                                      || (selectedComp == COMP_COMPARATOR && clicked->type == COMP_COMPARATOR)
                                      || (selectedComp == COMP_LOADER && clicked->type == COMP_LOADER)
                                      || (selectedComp == COMP_FILTER && clicked->type == COMP_FILTER);
                    if (!isClickConfig) {
                        PlaceComponent(gx, gy, selectedComp);
                    }
                }
            }
            // Right click: remove if occupied, rotate if empty
            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && InGrid(gx, gy)) {
                if (grid[gy][gx].type == COMP_EMPTY) {
                    placingDir = (Direction)((placingDir + 1) % 4);
                } else {
                    PlaceComponent(gx, gy, COMP_EMPTY);
                }
            }
            if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && !IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && InGrid(gx, gy)) {
                if (grid[gy][gx].type != COMP_EMPTY) {
                    PlaceComponent(gx, gy, COMP_EMPTY);
                }
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
                UpdateFluids();
                UpdateBelts();
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

                    // Ghost preview of selected component
                    if (selectedComp != COMP_EMPTY && grid[gy][gx].type == COMP_EMPTY) {
                        int cx = (int)(r.x + r.width / 2);
                        int cy = (int)(r.y + r.height / 2);
                        Color ghostCol = CompColor(selectedComp, false);
                        ghostCol.a = 80;

                        // Background fill
                        Color ghostBg = ghostCol;
                        ghostBg.a = 40;
                        DrawRectangleRec(r, ghostBg);

                        // Direction arrow for directional components
                        bool directional = (selectedComp == COMP_BELT || selectedComp == COMP_LOADER ||
                            selectedComp == COMP_GRABBER || selectedComp == COMP_SPLITTER ||
                            selectedComp == COMP_FILTER || selectedComp == COMP_NOT ||
                            selectedComp == COMP_AND || selectedComp == COMP_OR ||
                            selectedComp == COMP_XOR || selectedComp == COMP_NOR ||
                            selectedComp == COMP_LATCH || selectedComp == COMP_REPEATER ||
                            selectedComp == COMP_PULSE || selectedComp == COMP_VALVE ||
                            selectedComp == COMP_COMPARATOR || selectedComp == COMP_COMPRESSOR ||
                            selectedComp == COMP_DECOMPRESSOR || selectedComp == COMP_UNLOADER);
                        if (directional) {
                            DrawArrow(cx, cy, placingDir, (Color){255, 255, 255, 200});
                        }

                        // Label
                        const char *label = CompName(selectedComp);
                        int tw = MeasureTextUI(label, 10);
                        Color textGhost = WHITE;
                        textGhost.a = 100;
                        DrawTextShadow(label, cx - tw / 2, cy - 5, 10, textGhost);
                    }
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
