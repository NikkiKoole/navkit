// mechanisms.c - Signal-based automation simulation logic
// Extracted from demo.c for testability

#include "mechanisms.h"
#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static Cell grid[MECH_GRID_H][MECH_GRID_W];
static int signalGrid[2][MECH_GRID_H][MECH_GRID_W];
static int sigRead = 0, sigWrite = 1;

static Processor processors[MAX_PROCESSORS];
static int processorCount = 0;

// BFS queue for wire flood-fill
static int bfsQueueX[MECH_GRID_W * MECH_GRID_H];
static int bfsQueueY[MECH_GRID_W * MECH_GRID_H];
static int bfsSeedVal[MECH_GRID_H][MECH_GRID_W];

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
bool MechInGrid(int x, int y) {
    return x >= 0 && x < MECH_GRID_W && y >= 0 && y < MECH_GRID_H;
}

void MechDirOffset(Direction d, int *dx, int *dy) {
    *dx = 0; *dy = 0;
    switch (d) {
        case DIR_NORTH: *dy = -1; break;
        case DIR_EAST:  *dx =  1; break;
        case DIR_SOUTH: *dy =  1; break;
        case DIR_WEST:  *dx = -1; break;
    }
}

Direction MechOppositeDir(Direction d) {
    return (Direction)((d + 2) % 4);
}

void MechGateInputDirs(Direction facing, Direction *inA, Direction *inB) {
    *inA = (Direction)((facing + 1) % 4);
    *inB = (Direction)((facing + 3) % 4);
}

// ---------------------------------------------------------------------------
// Component metadata table
// ---------------------------------------------------------------------------
// Key codes: use raylib constants. We include them as literal ints to avoid
// depending on raylib in the simulation code. Values from raylib.h:
enum {
    MKEY_0=48,MKEY_1=49,MKEY_2=50,MKEY_3=51,MKEY_4=52,MKEY_5=53,MKEY_6=54,MKEY_7=55,MKEY_8=56,MKEY_9=57,
    MKEY_A=65,MKEY_B=66,MKEY_D=68,MKEY_E=69,MKEY_H=72,MKEY_J=74,MKEY_K=75,MKEY_Q=81,MKEY_S=83,MKEY_V=86,MKEY_W=87,MKEY_X=88,MKEY_Z=90,
    MKEY_COMMA=44,MKEY_PERIOD=46,MKEY_SLASH=47,MKEY_SEMICOLON=59,MKEY_APOSTROPHE=39,MKEY_BACKSLASH=92,MKEY_LBRACKET=91,MKEY_RBRACKET=93,
    MKEY_F1=290,MKEY_F2=291,MKEY_F3=292,MKEY_F4=293,MKEY_F5=294,MKEY_F6=295,MKEY_F7=296,MKEY_F8=297,MKEY_F9=298,MKEY_F10=299,MKEY_F11=300,MKEY_F12=301
};

//                          name        tooltip                                                                                                          colorR,G,B     activeR,G,B    layer             drawStyle       label  dir    click  key          keyLabel
static const CompMeta compMeta[COMP_COUNT] = {
[COMP_EMPTY]          = { "Eraser",    "Eraser. Click to remove any component. Shortcut: 0 or right-click.",                                             40,40,45,      0,0,0,         LAYER_SIGNAL,     DRAW_NONE,      "",    false, false, MKEY_0,      "0"  },
[COMP_SWITCH]         = { "Switch",    "Toggle on/off with click. Powers adjacent wires. Use as manual input for any circuit.",                           120,100,20,    255,255,0,     LAYER_SIGNAL,     DRAW_LABEL,     "S",   false, true,  MKEY_1,      "1"  },
[COMP_BUTTON]         = { "Button",    "Emits a brief pulse while held. Good for triggering one-shot events like latches or pulses.",                     120,40,40,     255,100,100,   LAYER_SIGNAL,     DRAW_LABEL,     "B",   false, false, MKEY_2,      "2"  },
[COMP_LIGHT]          = { "Light",     "Lights up when it receives signal. The simplest output — use to visualize any wire state.",                       40,60,40,      50,230,50,     LAYER_SIGNAL,     DRAW_CIRCLE,    "",    false, false, MKEY_3,      "3"  },
[COMP_WIRE]           = { "Wire",      "Carries signal between components. Connects in all 4 directions. The backbone of every circuit.",                 80,80,80,      0,0,0,         LAYER_SIGNAL,     DRAW_CONNECTED, "",    false, false, MKEY_4,      "4"  },
[COMP_NOT]            = { "NOT",       "Outputs 1 when input is 0, and vice versa. Directional. Essential for inverters and oscillators.",                200,60,60,     0,0,0,         LAYER_SIGNAL,     DRAW_GATE,      "!",   true,  false, MKEY_5,      "5"  },
[COMP_AND]            = { "AND",       "Outputs 1 only when both side inputs are on. Directional. Use for conditional logic and gating.",                 60,60,200,     0,0,0,         LAYER_SIGNAL,     DRAW_GATE,      "&",   true,  false, MKEY_6,      "6"  },
[COMP_OR]             = { "OR",        "Outputs 1 when either side input is on. Directional. Combines multiple signal sources.",                          60,180,60,     0,0,0,         LAYER_SIGNAL,     DRAW_GATE,      "|",   true,  false, MKEY_7,      "7"  },
[COMP_XOR]            = { "XOR",       "Outputs 1 when exactly one input is on. Directional. Key building block for adders and toggles.",                 180,60,180,    0,0,0,         LAYER_SIGNAL,     DRAW_GATE,      "^",   true,  false, MKEY_8,      "8"  },
[COMP_NOR]            = { "NOR",       "Outputs 1 only when both inputs are off. Directional. Two NOR gates make an SR latch (memory).",                  200,100,60,    0,0,0,         LAYER_SIGNAL,     DRAW_GATE,      "V",   true,  false, MKEY_9,      "9"  },
[COMP_LATCH]          = { "Latch",     "Set/Reset memory cell. Set input turns it on, reset turns it off, stays until changed.",                          100,85,20,     255,220,50,    LAYER_SIGNAL,     DRAW_GATE,      "M",   true,  false, MKEY_Q,      "Q"  },
[COMP_PROCESSOR]      = { "CPU",       "Tiny 6-opcode CPU. Reads/writes ports on 4 sides. Press P to edit program. The ultimate component.",             140,60,200,    0,0,0,         LAYER_CPU,        DRAW_LABEL,     "C",   false, false, MKEY_Z,      "Z"  },
[COMP_CLOCK]          = { "Clock",     "Auto-toggles every N ticks (click to change period 1-8). Use for blinking, timing, and sequencing.",              120,70,0,      255,160,0,     LAYER_SIGNAL,     DRAW_CUSTOM,    "",    false, true,  MKEY_W,      "W"  },
[COMP_REPEATER]       = { "Repeater",  "Delays signal by N ticks (click to change 1-4). Directional. Use to time circuits or extend pulses.",            0,80,80,       0,200,200,     LAYER_SIGNAL,     DRAW_GATE,      "",    true,  true,  MKEY_E,      "E"  },
[COMP_PULSE]          = { "Pulse",     "Stretches a brief input into a longer pulse (click to change duration 1-8). Good after buttons.",                 100,40,100,    255,100,255,   LAYER_SIGNAL,     DRAW_GATE,      "",    true,  true,  MKEY_A,      "A"  },
[COMP_PIPE]           = { "Pipe",      "Carries fluid pressure between neighbors. Equalizes with adjacent pipes and tanks each tick.",                    30,60,160,     0,0,0,         LAYER_FLUID,      DRAW_CONNECTED, "",    false, false, MKEY_S,      "S"  },
[COMP_PUMP]           = { "Pump",      "Generates/drains fluid (click rate, neg=drain). Wire-gated. Adjacent shaft boosts rate.",         20,80,70,      30,180,160,    LAYER_FLUID,      DRAW_CUSTOM,    "",    false, true,  MKEY_D,      "D"  },
[COMP_VALVE]          = { "Valve",     "Blocks fluid flow unless adjacent wire has signal. Directional. Use switches to control fluid routing.",          60,40,40,      30,100,200,    LAYER_FLUID,      DRAW_CUSTOM,    "V",   true,  false, MKEY_H,      "H"  },
[COMP_TANK]           = { "Tank",      "Stores fluid up to 1024 pressure (4x pipe capacity). Acts as a buffer to smooth pressure spikes.",                20,40,100,     0,0,0,         LAYER_FLUID,      DRAW_CUSTOM,    "T",   false, false, MKEY_J,      "J"  },
[COMP_PRESSURE_LIGHT] = { "PrLight",   "Converts fluid pressure to analog signal 0-15. Bridges the fluid and signal layers.",                            20,60,80,      50,200,230,    LAYER_FLUID,      DRAW_CIRCLE,    "",    false, false, MKEY_K,      "K"  },
[COMP_DIAL]           = { "Dial",      "Outputs an analog value 0-15 (click to change). Source for analog circuits, displays, comparators.",              200,160,40,    0,0,0,         LAYER_SIGNAL,     DRAW_CUSTOM,    "",    false, true,  MKEY_X,      "X"  },
[COMP_COMPARATOR]     = { "Compare",   "Outputs 1 when analog input >= threshold (click to set 1-15). Directional. Analog-to-digital.",                  100,55,20,     220,120,40,    LAYER_SIGNAL,     DRAW_GATE,      "",    true,  true,  MKEY_V,      "V"  },
[COMP_DISPLAY]        = { "Display",   "Shows the analog value (0-15) from an adjacent wire as a colored number. Passive readout.",                       20,20,30,      0,0,0,         LAYER_CPU,        DRAW_CUSTOM,    "",    false, false, MKEY_B,      "B"  },
[COMP_BELT]           = { "Belt",      "Moves cargo one cell per tick in its facing direction. Chain belts together for conveyor lines.",                  100,90,60,     0,0,0,         LAYER_BELT,       DRAW_CUSTOM,    "",    true,  false, MKEY_COMMA,  ","  },
[COMP_LOADER]         = { "Loader",    "Spawns cargo onto next belt (click type 1-15). Wire-gated: place wire adjacent to control.",                    40,80,40,      60,160,60,     LAYER_BELT,       DRAW_CUSTOM,    "",    true,  true,  MKEY_PERIOD, "."  },
[COMP_UNLOADER]       = { "Unloader",  "Consumes cargo and holds last type as persistent signal. Bridges belts to wires.",                   80,40,40,      160,60,60,     LAYER_BELT,       DRAW_LABEL,     "U",   true,  false, MKEY_SLASH,  "/"  },
[COMP_GRABBER]        = { "Grabber",   "Signal-controlled inserter. Moves cargo from behind to ahead when wire signal is on. Directional.",               80,70,20,      160,140,40,    LAYER_BELT,       DRAW_CUSTOM,    "",    true,  false, MKEY_SEMICOLON,";" },
[COMP_SPLITTER]       = { "Splitter",  "Alternates cargo left and right. Directional. Use to balance two output lines from one input.",                   80,80,120,     0,0,0,         LAYER_BELT,       DRAW_CUSTOM,    "Y",   true,  false, MKEY_APOSTROPHE,"'" },
[COMP_FILTER]         = { "Filter",    "Only passes cargo matching its type (click to set 1-15). Rejects others. Directional sorter.",                    120,80,100,    0,0,0,         LAYER_BELT,       DRAW_CUSTOM,    "",    true,  true,  MKEY_BACKSLASH,"\\" },
[COMP_COMPRESSOR]     = { "Compress",  "Merges two side belt inputs into one dual-cargo item. Directional. Doubles belt throughput.",                      100,80,120,    0,0,0,         LAYER_BELT,       DRAW_CUSTOM,    "><",  true,  false, MKEY_LBRACKET,"[" },
[COMP_DECOMPRESSOR]   = { "Decomp",   "Splits dual-cargo: primary forward, secondary to side. Directional. Reverses a compressor.",                      80,100,120,    0,0,0,         LAYER_BELT,       DRAW_CUSTOM,    "<>",  true,  false, MKEY_RBRACKET,"]" },
[COMP_CRANK]          = { "Crank",     "Click to engage/disengage. Outputs constant torque to the shaft network. The primary power source.",              140,100,40,    220,160,60,    LAYER_MECHANICAL, DRAW_CUSTOM,    "",    false, true,  MKEY_F1,     "F1" },
[COMP_SPRING]         = { "Spring",    "Winds up over time. Signal on adjacent wire releases stored energy as a torque burst. Click: capacity.",          120,80,30,     200,140,50,    LAYER_MECHANICAL, DRAW_CUSTOM,    "",    false, true,  MKEY_F2,     "F2" },
[COMP_SHAFT]          = { "Shaft",     "Carries mechanical speed between neighbors. All connected shafts share speed. Like wire, but for torque.",        180,140,80,    0,0,0,         LAYER_MECHANICAL, DRAW_CONNECTED, "",    false, false, MKEY_F3,     "F3" },
[COMP_CLUTCH]         = { "Clutch",    "Disconnects the shaft network when no wire signal. Engage via adjacent wire. Mechanical valve.",                  80,60,30,      200,160,70,    LAYER_MECHANICAL, DRAW_LABEL,     "CL",  false, false, MKEY_F4,     "F4" },
[COMP_FLYWHEEL]       = { "Flywhl",   "Adds inertia to the network (click to set 1-8). Resists speed changes — smooths out torque spikes.",              120,120,130,   0,0,0,         LAYER_MECHANICAL, DRAW_CUSTOM,    "",    false, true,  MKEY_F5,     "F5" },
[COMP_ESCAPEMENT]     = { "Escape",   "Converts shaft speed into periodic signal pulses. Faster speed = faster ticks. Directional output.",              140,120,40,    220,190,60,    LAYER_MECHANICAL, DRAW_GATE,      "E",   true,  false, MKEY_F6,     "F6" },
[COMP_CAM_SHAFT]      = { "CamSh",    "8-bit pattern sequencer driven by shaft speed. Outputs signal when current bit is set. Click: pattern.",           130,110,40,    200,170,60,    LAYER_MECHANICAL, DRAW_CUSTOM,    "",    true,  true,  MKEY_F7,     "F7" },
[COMP_HAMMER]         = { "Hammer",    "Strikes when shaft speed > 5. Consumes torque as load (click: 1-8). Visual up/down animation.",                   100,100,110,   180,180,180,   LAYER_MECHANICAL, DRAW_CUSTOM,    "",    false, true,  MKEY_F8,     "F8" },
[COMP_LEVER_ARM]      = { "Lever",     "Extends when shaft speed > 1 and emits wire signal. Directional. Bridges mechanical to signal layer.",            90,90,100,     170,170,180,   LAYER_MECHANICAL, DRAW_GATE,      "L",   true,  true,  MKEY_F9,     "F9" },
[COMP_GOVERNOR]       = { "Gov",       "Outputs analog signal 0-15 proportional to shaft speed. Bridges mechanical to signal for feedback loops.",        140,120,50,    220,190,80,    LAYER_MECHANICAL, DRAW_CUSTOM,    "",    false, false, MKEY_F10,    "F10"},
};

const CompMeta *MechGetCompMeta(ComponentType t) {
    if (t < 0 || t >= COMP_COUNT) return &compMeta[COMP_EMPTY];
    return &compMeta[t];
}

const char *MechCompName(ComponentType t) {
    if (t < 0 || t >= COMP_COUNT) return "?";
    return compMeta[t].name;
}

const char *MechOpName(OpCode op) {
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

const char *MechDirName(Direction d) {
    switch (d) {
        case DIR_NORTH: return "N";
        case DIR_EAST:  return "E";
        case DIR_SOUTH: return "S";
        case DIR_WEST:  return "W";
        default: return "?";
    }
}

// ---------------------------------------------------------------------------
// Grid access
// ---------------------------------------------------------------------------
Cell* MechGetCell(int x, int y) {
    if (!MechInGrid(x, y)) return NULL;
    return &grid[y][x];
}

int MechGetSignal(int x, int y) {
    if (!MechInGrid(x, y)) return 0;
    return signalGrid[sigRead][y][x];
}

void MechSetSwitch(int x, int y, bool state) {
    if (!MechInGrid(x, y)) return;
    if (grid[y][x].type == COMP_SWITCH)
        grid[y][x].state = state;
}

void MechSetButtonDown(int x, int y, bool down) {
    if (!MechInGrid(x, y)) return;
    if (grid[y][x].type == COMP_BUTTON)
        grid[y][x].state = down;
}

// ---------------------------------------------------------------------------
// Processor management
// ---------------------------------------------------------------------------
int MechFindProcessor(int gx, int gy) {
    for (int i = 0; i < processorCount; i++) {
        if (processors[i].active && processors[i].x == gx && processors[i].y == gy)
            return i;
    }
    return -1;
}

Processor* MechGetProcessor(int idx) {
    if (idx < 0 || idx >= MAX_PROCESSORS) return NULL;
    return &processors[idx];
}

static int CreateProcessor(int gx, int gy) {
    for (int i = 0; i < MAX_PROCESSORS; i++) {
        if (!processors[i].active) {
            memset(&processors[i], 0, sizeof(Processor));
            processors[i].x = gx;
            processors[i].y = gy;
            processors[i].active = true;
            processors[i].progLen = 1;
            if (i >= processorCount) processorCount = i + 1;
            return i;
        }
    }
    return -1;
}

static void RemoveProcessor(int gx, int gy) {
    int idx = MechFindProcessor(gx, gy);
    if (idx >= 0) {
        processors[idx].active = false;
        while (processorCount > 0 && !processors[processorCount - 1].active)
            processorCount--;
    }
}

// ---------------------------------------------------------------------------
// Placement
// ---------------------------------------------------------------------------
static void PlaceComponentInternal(int gx, int gy, ComponentType type, Direction dir) {
    if (!MechInGrid(gx, gy)) return;

    if (grid[gy][gx].type == COMP_PROCESSOR) {
        RemoveProcessor(gx, gy);
    }

    memset(&grid[gy][gx], 0, sizeof(Cell));
    grid[gy][gx].procIdx = -1;

    if (type == COMP_EMPTY) return;

    grid[gy][gx].type = type;
    grid[gy][gx].facing = dir;

    if (type == COMP_PROCESSOR) {
        int pi = CreateProcessor(gx, gy);
        grid[gy][gx].procIdx = pi;
    }
    if (type == COMP_CLOCK) {
        grid[gy][gx].setting = 4;
        grid[gy][gx].timer = 4;
    }
    if (type == COMP_REPEATER) {
        grid[gy][gx].setting = 1;
    }
    if (type == COMP_PULSE) {
        grid[gy][gx].setting = 5;
    }
    if (type == COMP_PUMP) {
        grid[gy][gx].setting = 4;
    }
    if (type == COMP_DIAL) {
        grid[gy][gx].setting = 8;
        grid[gy][gx].state = true;
    }
    if (type == COMP_COMPARATOR) {
        grid[gy][gx].setting = 5;
    }
    if (type == COMP_LOADER) {
        grid[gy][gx].setting = 1;
    }
    if (type == COMP_FILTER) {
        grid[gy][gx].setting = 1;
    }
    // Mechanical defaults
    if (type == COMP_CRANK) {
        grid[gy][gx].setting = 5;
    }
    if (type == COMP_SPRING) {
        grid[gy][gx].setting = 8;
    }
    if (type == COMP_FLYWHEEL) {
        grid[gy][gx].setting = 5;
    }
    if (type == COMP_CAM_SHAFT) {
        grid[gy][gx].setting = 0xAA; // alternating pattern
    }
    if (type == COMP_HAMMER) {
        grid[gy][gx].setting = 3;
    }
    if (type == COMP_LEVER_ARM) {
        grid[gy][gx].setting = 2;
    }
}

void MechPlaceComponent(int gx, int gy, ComponentType type, Direction dir) {
    PlaceComponentInternal(gx, gy, type, dir);
}

void MechPlaceWire(int gx, int gy) {
    PlaceComponentInternal(gx, gy, COMP_WIRE, DIR_NORTH);
}

// Internal helper used by preset builders
static void PlaceAt(int gx, int gy, ComponentType type, Direction dir) {
    PlaceComponentInternal(gx, gy, type, dir);
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------
void MechInit(void) {
    memset(grid, 0, sizeof(grid));
    memset(signalGrid, 0, sizeof(signalGrid));
    memset(processors, 0, sizeof(processors));
    processorCount = 0;
    sigRead = 0;
    sigWrite = 1;
    for (int y = 0; y < MECH_GRID_H; y++)
        for (int x = 0; x < MECH_GRID_W; x++)
            grid[y][x].procIdx = -1;
}

// ---------------------------------------------------------------------------
// Signal helpers
// ---------------------------------------------------------------------------
static void SeedWire(int newSig[][MECH_GRID_W], int nx, int ny, int value, int *qTail) {
    if (!MechInGrid(nx, ny) || grid[ny][nx].type != COMP_WIRE) return;
    if (newSig[ny][nx] >= value) return;
    newSig[ny][nx] = value;
    bfsSeedVal[ny][nx] = value;
    bfsQueueX[*qTail] = nx;
    bfsQueueY[*qTail] = ny;
    (*qTail)++;
}

static void SeedAdjacentWires(int newSig[][MECH_GRID_W], int x, int y, int value, int *qTail) {
    for (int d = 0; d < 4; d++) {
        int dx, dy;
        MechDirOffset((Direction)d, &dx, &dy);
        SeedWire(newSig, x + dx, y + dy, value, qTail);
    }
}

static void SeedFacingWire(int newSig[][MECH_GRID_W], int x, int y, Direction facing, int value, int *qTail) {
    int dx, dy;
    MechDirOffset(facing, &dx, &dy);
    SeedWire(newSig, x + dx, y + dy, value, qTail);
}

// ---------------------------------------------------------------------------
// Simulation: Signal propagation
// ---------------------------------------------------------------------------
void MechUpdateSignals(void) {
    int newSig[MECH_GRID_H][MECH_GRID_W];
    memset(newSig, 0, sizeof(newSig));
    memset(bfsSeedVal, 0, sizeof(bfsSeedVal));

    int qHead = 0, qTail = 0;

    for (int y = 0; y < MECH_GRID_H; y++) {
        for (int x = 0; x < MECH_GRID_W; x++) {
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
                    MechDirOffset(MechOppositeDir(c->facing), &dx, &dy);
                    int inX = x + dx, inY = y + dy;
                    int input = 0;
                    if (MechInGrid(inX, inY)) input = signalGrid[sigRead][inY][inX];
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
                    MechDirOffset(MechOppositeDir(c->facing), &dx, &dy);
                    int inX = x + dx, inY = y + dy;
                    int input = 0;
                    if (MechInGrid(inX, inY)) input = signalGrid[sigRead][inY][inX];
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
                    MechDirOffset(MechOppositeDir(c->facing), &dx, &dy);
                    int inX = x + dx, inY = y + dy;
                    int input = 0;
                    if (MechInGrid(inX, inY)) input = signalGrid[sigRead][inY][inX];
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
                    MechGateInputDirs(c->facing, &inADir, &inBDir);
                    int dx, dy;
                    MechDirOffset(inADir, &dx, &dy);
                    int inA = 0, inB = 0;
                    if (MechInGrid(x + dx, y + dy)) inA = signalGrid[sigRead][y + dy][x + dx];
                    MechDirOffset(inBDir, &dx, &dy);
                    if (MechInGrid(x + dx, y + dy)) inB = signalGrid[sigRead][y + dy][x + dx];

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
                    MechGateInputDirs(c->facing, &setDir, &resetDir);
                    int dx, dy;
                    MechDirOffset(setDir, &dx, &dy);
                    int setIn = 0, resetIn = 0;
                    if (MechInGrid(x + dx, y + dy)) setIn = signalGrid[sigRead][y + dy][x + dx];
                    MechDirOffset(resetDir, &dx, &dy);
                    if (MechInGrid(x + dx, y + dy)) resetIn = signalGrid[sigRead][y + dy][x + dx];

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
                    int dx, dy;
                    MechDirOffset(MechOppositeDir(c->facing), &dx, &dy);
                    int inX = x + dx, inY = y + dy;
                    int input = 0;
                    if (MechInGrid(inX, inY)) input = signalGrid[sigRead][inY][inX];
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

    // BFS flood-fill through wires
    while (qHead < qTail) {
        int wx = bfsQueueX[qHead];
        int wy = bfsQueueY[qHead];
        int val = bfsSeedVal[wy][wx];
        qHead++;
        for (int d = 0; d < 4; d++) {
            int dx, dy;
            MechDirOffset((Direction)d, &dx, &dy);
            int nx = wx + dx, ny = wy + dy;
            if (MechInGrid(nx, ny) && grid[ny][nx].type == COMP_WIRE && newSig[ny][nx] < val) {
                newSig[ny][nx] = val;
                bfsSeedVal[ny][nx] = val;
                bfsQueueX[qTail] = nx;
                bfsQueueY[qTail] = ny;
                qTail++;
            }
        }
    }

    // Copy result and update sinks
    memcpy(signalGrid[sigWrite], newSig, sizeof(newSig));
    sigRead = sigWrite;
    sigWrite = 1 - sigWrite;

    for (int y = 0; y < MECH_GRID_H; y++) {
        for (int x = 0; x < MECH_GRID_W; x++) {
            Cell *c = &grid[y][x];
            if (c->type == COMP_LIGHT) {
                int sig = 0;
                for (int d = 0; d < 4; d++) {
                    int dx, dy;
                    MechDirOffset((Direction)d, &dx, &dy);
                    int nx = x + dx, ny = y + dy;
                    if (MechInGrid(nx, ny) && newSig[ny][nx] > sig)
                        sig = newSig[ny][nx];
                }
                c->signalIn = sig;
                c->state = (sig > 0);
            }
            if (c->type == COMP_DISPLAY) {
                int maxSig = 0;
                for (int d = 0; d < 4; d++) {
                    int dx, dy;
                    MechDirOffset((Direction)d, &dx, &dy);
                    int nx = x + dx, ny = y + dy;
                    if (MechInGrid(nx, ny) && newSig[ny][nx] > maxSig)
                        maxSig = newSig[ny][nx];
                }
                c->signalIn = maxSig;
                c->setting = maxSig;
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
    MechDirOffset(dirs[port], &dx, &dy);
    int nx = p->x + dx, ny = p->y + dy;
    if (MechInGrid(nx, ny)) return signalGrid[sigRead][ny][nx];
    return 0;
}

static void ProcWritePort(Processor *p, int port, int value) {
    Direction dirs[4] = { DIR_NORTH, DIR_EAST, DIR_SOUTH, DIR_WEST };
    if (port < 0 || port > 3) return;
    int dx, dy;
    MechDirOffset(dirs[port], &dx, &dy);
    int nx = p->x + dx, ny = p->y + dy;
    if (MechInGrid(nx, ny) && grid[ny][nx].type == COMP_WIRE) {
        signalGrid[sigRead][ny][nx] = value ? 1 : 0;
    }
}

void MechUpdateProcessors(void) {
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
// Forward declaration (needed by shaft-driven pump, defined in mechanical section)
static bool IsMechCell(ComponentType t);

static bool IsFluidCell(ComponentType t) {
    return t == COMP_PIPE || t == COMP_PUMP ||
           t == COMP_VALVE || t == COMP_TANK || t == COMP_PRESSURE_LIGHT;
}

static bool IsValveOpen(int x, int y) {
    for (int d = 0; d < 4; d++) {
        int dx, dy;
        MechDirOffset((Direction)d, &dx, &dy);
        int nx = x + dx, ny = y + dy;
        if (MechInGrid(nx, ny) && grid[ny][nx].type == COMP_WIRE && signalGrid[sigRead][ny][nx])
            return true;
    }
    return false;
}

static bool IsPumpActive(int x, int y) {
    bool hasWire = false;
    bool hasSignal = false;
    for (int d = 0; d < 4; d++) {
        int dx, dy;
        MechDirOffset((Direction)d, &dx, &dy);
        int nx = x + dx, ny = y + dy;
        if (MechInGrid(nx, ny) && grid[ny][nx].type == COMP_WIRE) {
            hasWire = true;
            if (signalGrid[sigRead][ny][nx]) hasSignal = true;
        }
    }
    return !hasWire || hasSignal;
}

static int FluidMaxLevel(ComponentType t) {
    return (t == COMP_TANK) ? 1024 : 255;
}

void MechUpdateFluids(void) {
    int newFluid[MECH_GRID_H][MECH_GRID_W];
    for (int y = 0; y < MECH_GRID_H; y++)
        for (int x = 0; x < MECH_GRID_W; x++)
            newFluid[y][x] = grid[y][x].fluidLevel;

    // Pressure equalization
    for (int y = 0; y < MECH_GRID_H; y++) {
        for (int x = 0; x < MECH_GRID_W; x++) {
            Cell *c = &grid[y][x];
            if (!IsFluidCell(c->type)) continue;
            if (c->type == COMP_VALVE && !IsValveOpen(x, y)) continue;

            int myLevel = c->fluidLevel;
            int myMax = FluidMaxLevel(c->type);

            int neighborCount = 0;
            int neighborX[4], neighborY[4];
            for (int d = 0; d < 4; d++) {
                int dx, dy;
                MechDirOffset((Direction)d, &dx, &dy);
                int nx = x + dx, ny = y + dy;
                if (!MechInGrid(nx, ny)) continue;
                Cell *nc = &grid[ny][nx];
                if (!IsFluidCell(nc->type)) continue;
                if (nc->type == COMP_VALVE && !IsValveOpen(nx, ny)) continue;
                neighborX[neighborCount] = nx;
                neighborY[neighborCount] = ny;
                neighborCount++;
            }

            if (neighborCount == 0) continue;

            for (int i = 0; i < neighborCount; i++) {
                int nx = neighborX[i], ny = neighborY[i];
                int nLevel = grid[ny][nx].fluidLevel;
                int diff = myLevel - nLevel;
                int transfer = diff / (neighborCount + 1);
                if (transfer > 0) {
                    newFluid[y][x] -= transfer;
                    newFluid[ny][nx] += transfer;
                    int nMax = FluidMaxLevel(grid[ny][nx].type);
                    if (newFluid[ny][nx] > nMax) newFluid[ny][nx] = nMax;
                    if (newFluid[y][x] > myMax) newFluid[y][x] = myMax;
                    if (newFluid[y][x] < 0) newFluid[y][x] = 0;
                }
            }
        }
    }

    // Pumps (positive setting = generate, negative = drain)
    // Wire-gated (same as before) + shaft-driven: adjacent shaft speed boosts rate
    for (int y = 0; y < MECH_GRID_H; y++) {
        for (int x = 0; x < MECH_GRID_W; x++) {
            if (grid[y][x].type != COMP_PUMP) continue;
            int setting = grid[y][x].setting;

            // Check adjacent mechanical cells for shaft speed boost
            float shaftBoost = 0;
            for (int d = 0; d < 4; d++) {
                int dx, dy;
                MechDirOffset((Direction)d, &dx, &dy);
                int nx = x + dx, ny = y + dy;
                if (MechInGrid(nx, ny) && IsMechCell(grid[ny][nx].type)) {
                    if (grid[ny][nx].mechSpeed > shaftBoost)
                        shaftBoost = grid[ny][nx].mechSpeed;
                }
            }

            if (setting > 0 && IsPumpActive(x, y)) {
                int rate = setting * 8;
                if (shaftBoost > 0) rate = (int)(rate * (1.0f + shaftBoost / 20.0f));
                newFluid[y][x] += rate;
                int mx = FluidMaxLevel(COMP_PUMP);
                if (newFluid[y][x] > mx) newFluid[y][x] = mx;
                grid[y][x].state = true;
            } else if (setting < 0) {
                int rate = (-setting) * 8;
                if (shaftBoost > 0) rate = (int)(rate * (1.0f + shaftBoost / 20.0f));
                newFluid[y][x] -= rate;
                if (newFluid[y][x] < 0) newFluid[y][x] = 0;
                grid[y][x].state = true;
            } else {
                grid[y][x].state = false;
            }
        }
    }

    // Valves
    for (int y = 0; y < MECH_GRID_H; y++) {
        for (int x = 0; x < MECH_GRID_W; x++) {
            if (grid[y][x].type == COMP_VALVE) {
                grid[y][x].state = IsValveOpen(x, y);
            }
        }
    }

    // Pressure lights
    for (int y = 0; y < MECH_GRID_H; y++) {
        for (int x = 0; x < MECH_GRID_W; x++) {
            if (grid[y][x].type == COMP_PRESSURE_LIGHT) {
                int maxPressure = 0;
                for (int d = 0; d < 4; d++) {
                    int dx, dy;
                    MechDirOffset((Direction)d, &dx, &dy);
                    int nx = x + dx, ny = y + dy;
                    if (MechInGrid(nx, ny) && IsFluidCell(grid[ny][nx].type)) {
                        if (grid[ny][nx].fluidLevel > maxPressure)
                            maxPressure = grid[ny][nx].fluidLevel;
                    }
                }
                int analogOut = maxPressure / 17;
                if (analogOut > 15) analogOut = 15;
                grid[y][x].state = (analogOut > 0);
                grid[y][x].signalOut = analogOut;
                if (analogOut > 0) {
                    for (int d = 0; d < 4; d++) {
                        int dx, dy;
                        MechDirOffset((Direction)d, &dx, &dy);
                        int nx = x + dx, ny = y + dy;
                        if (MechInGrid(nx, ny) && grid[ny][nx].type == COMP_WIRE) {
                            if (signalGrid[sigRead][ny][nx] < analogOut)
                                signalGrid[sigRead][ny][nx] = analogOut;
                        }
                    }
                }
            }
        }
    }

    // Copy back
    for (int y = 0; y < MECH_GRID_H; y++)
        for (int x = 0; x < MECH_GRID_W; x++)
            grid[y][x].fluidLevel = newFluid[y][x];
}

// ---------------------------------------------------------------------------
// Simulation: Belt logistics
// ---------------------------------------------------------------------------
static bool IsBeltTarget(ComponentType t) {
    return t == COMP_BELT || t == COMP_UNLOADER || t == COMP_SPLITTER || t == COMP_FILTER;
}

void MechUpdateBelts(void) {
    int oldCargo[MECH_GRID_H][MECH_GRID_W];
    int oldCargo2[MECH_GRID_H][MECH_GRID_W];
    for (int y = 0; y < MECH_GRID_H; y++)
        for (int x = 0; x < MECH_GRID_W; x++) {
            oldCargo[y][x] = grid[y][x].cargo;
            oldCargo2[y][x] = grid[y][x].cargo2;
        }

    // Phase 1: Belts move cargo
    for (int y = 0; y < MECH_GRID_H; y++) {
        for (int x = 0; x < MECH_GRID_W; x++) {
            if (grid[y][x].type != COMP_BELT) continue;
            if (oldCargo[y][x] == 0) continue;

            int dx, dy;
            MechDirOffset(grid[y][x].facing, &dx, &dy);
            int nx = x + dx, ny = y + dy;
            if (!MechInGrid(nx, ny)) continue;
            Cell *next = &grid[ny][nx];

            if (IsBeltTarget(next->type) && next->cargo == 0) {
                next->cargo = oldCargo[y][x];
                next->cargo2 = oldCargo2[y][x];
                grid[y][x].cargo = 0;
                grid[y][x].cargo2 = 0;
            }
        }
    }

    // Phase 2: Filters
    for (int y = 0; y < MECH_GRID_H; y++) {
        for (int x = 0; x < MECH_GRID_W; x++) {
            if (grid[y][x].type != COMP_FILTER) continue;
            if (grid[y][x].cargo == 0) continue;
            if (grid[y][x].cargo != grid[y][x].setting) continue;

            int dx, dy;
            MechDirOffset(grid[y][x].facing, &dx, &dy);
            int nx = x + dx, ny = y + dy;
            if (!MechInGrid(nx, ny)) continue;
            Cell *next = &grid[ny][nx];
            if (IsBeltTarget(next->type) && next->cargo == 0) {
                next->cargo = grid[y][x].cargo;
                grid[y][x].cargo = 0;
            }
        }
    }

    // Phase 3: Splitters
    for (int y = 0; y < MECH_GRID_H; y++) {
        for (int x = 0; x < MECH_GRID_W; x++) {
            if (grid[y][x].type != COMP_SPLITTER) continue;
            if (grid[y][x].cargo == 0) continue;

            Direction leftDir, rightDir;
            MechGateInputDirs(grid[y][x].facing, &rightDir, &leftDir);

            Direction first = grid[y][x].altToggle ? leftDir : rightDir;
            Direction second = grid[y][x].altToggle ? rightDir : leftDir;

            int dx, dy;
            bool placed = false;
            MechDirOffset(first, &dx, &dy);
            int nx = x + dx, ny = y + dy;
            if (MechInGrid(nx, ny) && IsBeltTarget(grid[ny][nx].type) && grid[ny][nx].cargo == 0) {
                grid[ny][nx].cargo = grid[y][x].cargo;
                grid[y][x].cargo = 0;
                grid[y][x].altToggle = !grid[y][x].altToggle;
                placed = true;
            }
            if (!placed) {
                MechDirOffset(second, &dx, &dy);
                nx = x + dx; ny = y + dy;
                if (MechInGrid(nx, ny) && IsBeltTarget(grid[ny][nx].type) && grid[ny][nx].cargo == 0) {
                    grid[ny][nx].cargo = grid[y][x].cargo;
                    grid[y][x].cargo = 0;
                    grid[y][x].altToggle = !grid[y][x].altToggle;
                }
            }
        }
    }

    // Phase 3.5: Compressors
    for (int y = 0; y < MECH_GRID_H; y++) {
        for (int x = 0; x < MECH_GRID_W; x++) {
            if (grid[y][x].type != COMP_COMPRESSOR) continue;

            Direction rightDir, leftDir;
            MechGateInputDirs(grid[y][x].facing, &rightDir, &leftDir);

            int ldx, ldy;
            MechDirOffset(leftDir, &ldx, &ldy);
            int lx = x + ldx, ly = y + ldy;
            int leftCargo = 0;
            if (MechInGrid(lx, ly) && grid[ly][lx].cargo > 0)
                leftCargo = grid[ly][lx].cargo;

            int rdx, rdy;
            MechDirOffset(rightDir, &rdx, &rdy);
            int rx = x + rdx, ry = y + rdy;
            int rightCargo = 0;
            if (MechInGrid(rx, ry) && grid[ry][rx].cargo > 0)
                rightCargo = grid[ry][rx].cargo;

            if (leftCargo == 0 && rightCargo == 0) continue;

            bool leftHasBelt = MechInGrid(lx, ly) && grid[ly][lx].type != COMP_EMPTY;
            bool rightHasBelt = MechInGrid(rx, ry) && grid[ry][rx].type != COMP_EMPTY;

            if (leftHasBelt && rightHasBelt && (leftCargo == 0 || rightCargo == 0)) continue;

            int fdx, fdy;
            MechDirOffset(grid[y][x].facing, &fdx, &fdy);
            int fx = x + fdx, fy = y + fdy;
            if (!MechInGrid(fx, fy)) continue;
            Cell *fwd = &grid[fy][fx];
            if (!IsBeltTarget(fwd->type) || fwd->cargo != 0) continue;

            if (leftCargo > 0 && rightCargo > 0) {
                fwd->cargo = leftCargo;
                fwd->cargo2 = rightCargo;
                grid[ly][lx].cargo = 0;
                grid[ry][rx].cargo = 0;
            } else if (leftCargo > 0) {
                fwd->cargo = leftCargo;
                grid[ly][lx].cargo = 0;
            } else {
                fwd->cargo = rightCargo;
                grid[ry][rx].cargo = 0;
            }
        }
    }

    // Phase 3.6: Decompressors
    for (int y = 0; y < MECH_GRID_H; y++) {
        for (int x = 0; x < MECH_GRID_W; x++) {
            if (grid[y][x].type != COMP_DECOMPRESSOR) continue;

            int bdx, bdy;
            MechDirOffset(MechOppositeDir(grid[y][x].facing), &bdx, &bdy);
            int bx = x + bdx, by = y + bdy;
            if (!MechInGrid(bx, by)) continue;
            Cell *back = &grid[by][bx];
            if (back->cargo == 0) continue;

            int fdx, fdy;
            MechDirOffset(grid[y][x].facing, &fdx, &fdy);
            int fx = x + fdx, fy = y + fdy;
            if (!MechInGrid(fx, fy)) continue;
            Cell *fwd = &grid[fy][fx];
            if (!IsBeltTarget(fwd->type) || fwd->cargo != 0) continue;

            if (back->cargo2 > 0) {
                Direction rightDir, leftDir;
                MechGateInputDirs(grid[y][x].facing, &rightDir, &leftDir);
                Direction first = grid[y][x].altToggle ? leftDir : rightDir;
                Direction second = grid[y][x].altToggle ? rightDir : leftDir;

                Cell *side = NULL;
                int sdx, sdy;
                MechDirOffset(first, &sdx, &sdy);
                int sx = x + sdx, sy = y + sdy;
                if (MechInGrid(sx, sy) && IsBeltTarget(grid[sy][sx].type) && grid[sy][sx].cargo == 0) {
                    side = &grid[sy][sx];
                } else {
                    MechDirOffset(second, &sdx, &sdy);
                    sx = x + sdx; sy = y + sdy;
                    if (MechInGrid(sx, sy) && IsBeltTarget(grid[sy][sx].type) && grid[sy][sx].cargo == 0) {
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
                fwd->cargo = back->cargo;
                back->cargo = 0;
            }
        }
    }

    // Phase 4: Loaders (wire-gated: if adjacent wire exists, only loads when signal)
    for (int y = 0; y < MECH_GRID_H; y++) {
        for (int x = 0; x < MECH_GRID_W; x++) {
            if (grid[y][x].type != COMP_LOADER) continue;
            if (!IsPumpActive(x, y)) { grid[y][x].state = false; continue; }

            int dx, dy;
            MechDirOffset(grid[y][x].facing, &dx, &dy);
            int nx = x + dx, ny = y + dy;
            if (!MechInGrid(nx, ny)) continue;
            Cell *next = &grid[ny][nx];
            if (IsBeltTarget(next->type) && next->cargo == 0) {
                next->cargo = grid[y][x].setting;
                grid[y][x].state = true;
            } else {
                grid[y][x].state = false;
            }
        }
    }

    // Phase 5: Unloaders
    // Persistent signal: holds last-received cargo type until new cargo arrives.
    // setting stores the last cargo type (persistent memory).
    for (int y = 0; y < MECH_GRID_H; y++) {
        for (int x = 0; x < MECH_GRID_W; x++) {
            if (grid[y][x].type != COMP_UNLOADER) continue;
            if (grid[y][x].cargo > 0) {
                grid[y][x].setting = grid[y][x].cargo;  // remember last cargo type
                grid[y][x].cargo = 0;
            }
            int lastCargo = grid[y][x].setting;
            grid[y][x].signalOut = lastCargo;
            grid[y][x].state = (lastCargo > 0);
            if (lastCargo > 0) {
                for (int d = 0; d < 4; d++) {
                    int dx, dy;
                    MechDirOffset((Direction)d, &dx, &dy);
                    int nx = x + dx, ny = y + dy;
                    if (MechInGrid(nx, ny) && grid[ny][nx].type == COMP_WIRE) {
                        if (signalGrid[sigRead][ny][nx] < lastCargo)
                            signalGrid[sigRead][ny][nx] = lastCargo;
                    }
                }
            }
        }
    }

    // Phase 6: Grabbers
    for (int y = 0; y < MECH_GRID_H; y++) {
        for (int x = 0; x < MECH_GRID_W; x++) {
            if (grid[y][x].type != COMP_GRABBER) continue;

            bool hasWire = false, hasSignal = false;
            for (int d = 0; d < 4; d++) {
                int dx, dy;
                MechDirOffset((Direction)d, &dx, &dy);
                int nx = x + dx, ny = y + dy;
                if (MechInGrid(nx, ny) && grid[ny][nx].type == COMP_WIRE) {
                    hasWire = true;
                    if (signalGrid[sigRead][ny][nx] > 0) hasSignal = true;
                }
            }
            bool active = !hasWire || hasSignal;
            grid[y][x].state = active;
            if (!active) continue;

            int dx, dy;
            MechDirOffset(MechOppositeDir(grid[y][x].facing), &dx, &dy);
            int srcX = x + dx, srcY = y + dy;
            MechDirOffset(grid[y][x].facing, &dx, &dy);
            int dstX = x + dx, dstY = y + dy;

            if (!MechInGrid(srcX, srcY) || !MechInGrid(dstX, dstY)) continue;

            Cell *src = &grid[srcY][srcX];
            Cell *dst = &grid[dstY][dstX];

            int srcCargo = src->cargo;
            if (srcCargo == 0) continue;

            if (IsBeltTarget(dst->type) && dst->cargo == 0) {
                dst->cargo = srcCargo;
                src->cargo = 0;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Simulation: Mechanical shaft networks
// ---------------------------------------------------------------------------
static bool IsMechCell(ComponentType t) {
    return t == COMP_CRANK || t == COMP_SPRING || t == COMP_SHAFT ||
           t == COMP_CLUTCH || t == COMP_FLYWHEEL || t == COMP_ESCAPEMENT ||
           t == COMP_CAM_SHAFT || t == COMP_HAMMER || t == COMP_LEVER_ARM ||
           t == COMP_GOVERNOR;
}

static bool IsClutchEngaged(int x, int y) {
    // Same logic as IsValveOpen — clutch engages when adjacent wire has signal
    for (int d = 0; d < 4; d++) {
        int dx, dy;
        MechDirOffset((Direction)d, &dx, &dy);
        int nx = x + dx, ny = y + dy;
        if (MechInGrid(nx, ny) && grid[ny][nx].type == COMP_WIRE && signalGrid[sigRead][ny][nx])
            return true;
    }
    return false;
}

static bool IsSpringTriggered(int x, int y) {
    // Spring releases when adjacent wire has signal
    for (int d = 0; d < 4; d++) {
        int dx, dy;
        MechDirOffset((Direction)d, &dx, &dy);
        int nx = x + dx, ny = y + dy;
        if (MechInGrid(nx, ny) && grid[ny][nx].type == COMP_WIRE && signalGrid[sigRead][ny][nx])
            return true;
    }
    return false;
}

static void EmitSignalToAdjacentWires(int x, int y, int value) {
    for (int d = 0; d < 4; d++) {
        int dx, dy;
        MechDirOffset((Direction)d, &dx, &dy);
        int nx = x + dx, ny = y + dy;
        if (MechInGrid(nx, ny) && grid[ny][nx].type == COMP_WIRE) {
            if (signalGrid[sigRead][ny][nx] < value)
                signalGrid[sigRead][ny][nx] = value;
        }
    }
}

// Per-network accumulator
typedef struct {
    float totalTorque;
    float totalLoad;
    float totalInertia;
    float totalSpeed;
    int cellCount;
} MechNetwork;

void MechUpdateMechanical(void) {
    // Clear network assignments
    for (int y = 0; y < MECH_GRID_H; y++)
        for (int x = 0; x < MECH_GRID_W; x++)
            grid[y][x].mechNetwork = -1;

    MechNetwork networks[MECH_MAX_NETWORKS];
    int networkCount = 0;

    // Phase 1: BFS to discover shaft networks
    for (int y = 0; y < MECH_GRID_H; y++) {
        for (int x = 0; x < MECH_GRID_W; x++) {
            Cell *c = &grid[y][x];
            if (!IsMechCell(c->type)) continue;
            if (c->mechNetwork >= 0) continue;
            if (c->type == COMP_CLUTCH && !IsClutchEngaged(x, y)) continue;

            if (networkCount >= MECH_MAX_NETWORKS) break;
            int netId = networkCount++;
            MechNetwork *net = &networks[netId];
            net->totalTorque = 0;
            net->totalLoad = 0;
            net->totalInertia = 0;
            net->totalSpeed = 0;
            net->cellCount = 0;

            // BFS flood-fill (reuse bfsQueue arrays — safe, called after signals)
            int qHead = 0, qTail = 0;
            bfsQueueX[qTail] = x;
            bfsQueueY[qTail] = y;
            qTail++;
            c->mechNetwork = netId;

            while (qHead < qTail) {
                int cx = bfsQueueX[qHead];
                int cy = bfsQueueY[qHead];
                qHead++;

                Cell *cc = &grid[cy][cx];
                net->totalSpeed += cc->mechSpeed;
                net->cellCount++;
                net->totalInertia += 1.0f;

                // Per-type contributions
                if (cc->type == COMP_CRANK && cc->state) {
                    net->totalTorque += (float)cc->setting;
                }
                if (cc->type == COMP_SPRING) {
                    if (IsSpringTriggered(cx, cy) && cc->springCharge > 0) {
                        float burst = cc->springCharge;
                        if (burst > 10.0f) burst = 10.0f;
                        net->totalTorque += burst;
                        cc->springCharge -= burst;
                        if (cc->springCharge < 0) cc->springCharge = 0;
                    } else if (!IsSpringTriggered(cx, cy) && cc->springCharge < (float)cc->setting) {
                        cc->springCharge += 0.2f;
                        if (cc->springCharge > (float)cc->setting)
                            cc->springCharge = (float)cc->setting;
                    }
                }
                if (cc->type == COMP_FLYWHEEL) {
                    net->totalInertia += (float)cc->setting;
                }
                if (cc->type == COMP_HAMMER) {
                    net->totalLoad += (float)cc->setting;
                }
                if (cc->type == COMP_LEVER_ARM) {
                    net->totalLoad += (float)cc->setting;
                }

                // BFS to neighbors
                for (int d = 0; d < 4; d++) {
                    int dx, dy;
                    MechDirOffset((Direction)d, &dx, &dy);
                    int nx = cx + dx, ny = cy + dy;
                    if (!MechInGrid(nx, ny)) continue;
                    Cell *nc = &grid[ny][nx];
                    if (!IsMechCell(nc->type)) continue;
                    if (nc->mechNetwork >= 0) continue;
                    if (nc->type == COMP_CLUTCH && !IsClutchEngaged(nx, ny)) continue;
                    nc->mechNetwork = netId;
                    bfsQueueX[qTail] = nx;
                    bfsQueueY[qTail] = ny;
                    qTail++;
                }
            }
        }
    }

    // Phase 2: Physics — update speed per network
    for (int n = 0; n < networkCount; n++) {
        MechNetwork *net = &networks[n];
        if (net->cellCount == 0) continue;

        float avgSpeed = net->totalSpeed / (float)net->cellCount;
        float accel = (net->totalTorque - net->totalLoad * 0.1f) / net->totalInertia;
        float newSpeed = avgSpeed + accel;

        // Friction decay when no torque
        if (net->totalTorque <= 0) {
            newSpeed *= 0.95f;
        }

        if (newSpeed < 0) newSpeed = 0;
        if (newSpeed > 100.0f) newSpeed = 100.0f;
        if (newSpeed < 0.01f) newSpeed = 0;

        // Write back speed to all cells in this network
        for (int y = 0; y < MECH_GRID_H; y++) {
            for (int x = 0; x < MECH_GRID_W; x++) {
                if (grid[y][x].mechNetwork != n) continue;
                grid[y][x].mechSpeed = newSpeed;
            }
        }
    }

    // Phase 3: Output processing
    for (int y = 0; y < MECH_GRID_H; y++) {
        for (int x = 0; x < MECH_GRID_W; x++) {
            Cell *c = &grid[y][x];
            if (!IsMechCell(c->type)) continue;

            float speed = c->mechSpeed;

            switch (c->type) {
                case COMP_HAMMER:
                    c->state = (speed > 5.0f);
                    // Toggle visual with timer for striking effect
                    if (c->state) {
                        c->timer++;
                        if (c->timer > 3) c->timer = 0;
                    } else {
                        c->timer = 0;
                    }
                    break;

                case COMP_LEVER_ARM:
                    c->state = (speed > 1.0f);
                    if (c->state) {
                        EmitSignalToAdjacentWires(x, y, 1);
                    }
                    break;

                case COMP_ESCAPEMENT: {
                    // Tick rate inversely proportional to speed
                    // Higher speed = faster ticks
                    if (speed > 1.0f) {
                        int interval = (int)(30.0f / speed);
                        if (interval < 1) interval = 1;
                        c->timer++;
                        if (c->timer >= interval) {
                            c->timer = 0;
                            c->state = !c->state;
                        }
                        if (c->state) {
                            int dx, dy;
                            MechDirOffset(c->facing, &dx, &dy);
                            int nx = x + dx, ny = y + dy;
                            if (MechInGrid(nx, ny) && grid[ny][nx].type == COMP_WIRE) {
                                if (signalGrid[sigRead][ny][nx] < 1)
                                    signalGrid[sigRead][ny][nx] = 1;
                            }
                        }
                    } else {
                        c->state = false;
                        c->timer = 0;
                    }
                    break;
                }

                case COMP_CAM_SHAFT: {
                    if (speed > 1.0f) {
                        int advanceInterval = (int)(20.0f / speed);
                        if (advanceInterval < 1) advanceInterval = 1;
                        c->timer++;
                        if (c->timer >= advanceInterval) {
                            c->timer = 0;
                            c->camPosition = (c->camPosition + 1) % 8;
                        }
                        int pattern = c->setting & 0xFF;
                        bool bitSet = (pattern >> c->camPosition) & 1;
                        c->state = bitSet;
                        if (bitSet) {
                            int dx, dy;
                            MechDirOffset(c->facing, &dx, &dy);
                            int nx = x + dx, ny = y + dy;
                            if (MechInGrid(nx, ny) && grid[ny][nx].type == COMP_WIRE) {
                                if (signalGrid[sigRead][ny][nx] < 1)
                                    signalGrid[sigRead][ny][nx] = 1;
                            }
                        }
                    } else {
                        c->state = false;
                        c->timer = 0;
                    }
                    break;
                }

                case COMP_GOVERNOR: {
                    int analogVal = (int)(speed * 15.0f / 100.0f);
                    if (analogVal > 15) analogVal = 15;
                    c->signalOut = analogVal;
                    c->state = (analogVal > 0);
                    if (analogVal > 0) {
                        EmitSignalToAdjacentWires(x, y, analogVal);
                    }
                    break;
                }

                case COMP_CRANK:
                    // Visual: state already set by click
                    break;

                case COMP_SPRING:
                    c->state = (c->springCharge > 0.5f);
                    break;

                case COMP_CLUTCH:
                    c->state = IsClutchEngaged(x, y);
                    break;

                default:
                    break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Combined tick
// ---------------------------------------------------------------------------
void MechTick(void) {
    MechUpdateSignals();
    MechUpdateProcessors();
    MechUpdateFluids();
    MechUpdateBelts();
    MechUpdateMechanical();
}

// ---------------------------------------------------------------------------
// Preset builders
// ---------------------------------------------------------------------------
void MechBuildPresetNot(int ox, int oy) {
    PlaceAt(ox, oy + 1, COMP_SWITCH, DIR_NORTH);
    MechPlaceWire(ox + 1, oy + 1);
    MechPlaceWire(ox + 2, oy + 1);
    PlaceAt(ox + 3, oy + 1, COMP_NOT, DIR_EAST);
    MechPlaceWire(ox + 4, oy + 1);
    PlaceAt(ox + 5, oy + 1, COMP_LIGHT, DIR_EAST);
}

void MechBuildPresetAnd(int ox, int oy) {
    PlaceAt(ox, oy, COMP_SWITCH, DIR_NORTH);
    MechPlaceWire(ox + 1, oy);
    MechPlaceWire(ox + 2, oy);
    MechPlaceWire(ox + 3, oy);
    PlaceAt(ox + 3, oy + 1, COMP_AND, DIR_EAST);
    MechPlaceWire(ox + 4, oy + 1);
    PlaceAt(ox + 5, oy + 1, COMP_LIGHT, DIR_EAST);
    PlaceAt(ox, oy + 2, COMP_SWITCH, DIR_NORTH);
    MechPlaceWire(ox + 1, oy + 2);
    MechPlaceWire(ox + 2, oy + 2);
    MechPlaceWire(ox + 3, oy + 2);
}

void MechBuildPresetBlinker(int ox, int oy) {
    PlaceAt(ox, oy + 1, COMP_CLOCK, DIR_NORTH);
    Cell *clk = &grid[oy + 1][ox];
    clk->setting = 3;
    clk->timer = 3;
    MechPlaceWire(ox + 1, oy + 1);
    PlaceAt(ox + 2, oy + 1, COMP_LIGHT, DIR_EAST);
}

void MechBuildPresetNorLatch(int ox, int oy) {
    PlaceAt(ox, oy, COMP_BUTTON, DIR_NORTH);
    MechPlaceWire(ox + 1, oy);
    PlaceAt(ox + 2, oy, COMP_PULSE, DIR_EAST);
    MechPlaceWire(ox + 3, oy);
    MechPlaceWire(ox + 4, oy);

    PlaceAt(ox + 4, oy + 1, COMP_NOR, DIR_EAST);
    MechPlaceWire(ox + 5, oy + 1);
    MechPlaceWire(ox + 6, oy + 1);
    PlaceAt(ox + 7, oy + 1, COMP_LIGHT, DIR_EAST);

    MechPlaceWire(ox + 6, oy + 2);
    MechPlaceWire(ox + 6, oy + 3);
    MechPlaceWire(ox + 6, oy + 4);
    PlaceAt(ox + 5, oy + 4, COMP_REPEATER, DIR_WEST);
    MechPlaceWire(ox + 4, oy + 4);

    PlaceAt(ox + 4, oy + 5, COMP_NOR, DIR_WEST);
    MechPlaceWire(ox + 3, oy + 5);
    MechPlaceWire(ox + 2, oy + 5);
    PlaceAt(ox + 1, oy + 5, COMP_LIGHT, DIR_EAST);

    MechPlaceWire(ox + 2, oy + 4);
    MechPlaceWire(ox + 2, oy + 3);
    MechPlaceWire(ox + 2, oy + 2);
    PlaceAt(ox + 3, oy + 2, COMP_REPEATER, DIR_EAST);
    MechPlaceWire(ox + 4, oy + 2);

    MechPlaceWire(ox + 4, oy + 6);
    MechPlaceWire(ox + 4, oy + 7);
    MechPlaceWire(ox + 3, oy + 7);
    PlaceAt(ox + 2, oy + 7, COMP_PULSE, DIR_EAST);
    MechPlaceWire(ox + 1, oy + 7);
    PlaceAt(ox, oy + 7, COMP_BUTTON, DIR_NORTH);

    grid[oy + 7][ox].state = true;
    for (int i = 0; i < 10; i++) { MechUpdateSignals(); MechUpdateProcessors(); }
    grid[oy + 7][ox].state = false;
    for (int i = 0; i < 10; i++) { MechUpdateSignals(); MechUpdateProcessors(); }
}

void MechBuildPresetHalfAdder(int ox, int oy) {
    PlaceAt(ox, oy, COMP_SWITCH, DIR_NORTH);
    MechPlaceWire(ox + 1, oy);
    MechPlaceWire(ox + 2, oy);
    MechPlaceWire(ox + 3, oy);
    PlaceAt(ox + 4, oy, COMP_REPEATER, DIR_EAST);
    MechPlaceWire(ox + 5, oy);
    MechPlaceWire(ox + 6, oy);

    MechPlaceWire(ox + 2, oy + 1);
    MechPlaceWire(ox + 6, oy + 1);

    PlaceAt(ox + 2, oy + 2, COMP_XOR, DIR_EAST);
    MechPlaceWire(ox + 3, oy + 2);
    PlaceAt(ox + 4, oy + 2, COMP_LIGHT, DIR_EAST);

    PlaceAt(ox + 6, oy + 2, COMP_AND, DIR_EAST);
    MechPlaceWire(ox + 7, oy + 2);
    PlaceAt(ox + 8, oy + 2, COMP_LIGHT, DIR_EAST);

    MechPlaceWire(ox + 2, oy + 3);
    MechPlaceWire(ox + 6, oy + 3);

    PlaceAt(ox, oy + 4, COMP_SWITCH, DIR_NORTH);
    MechPlaceWire(ox + 1, oy + 4);
    MechPlaceWire(ox + 2, oy + 4);
    MechPlaceWire(ox + 3, oy + 4);
    PlaceAt(ox + 4, oy + 4, COMP_REPEATER, DIR_EAST);
    MechPlaceWire(ox + 5, oy + 4);
    MechPlaceWire(ox + 6, oy + 4);
}

void MechBuildPresetPumpLoop(int ox, int oy) {
    PlaceAt(ox, oy + 1, COMP_PUMP, DIR_NORTH);
    grid[oy + 1][ox].setting = 4;
    for (int i = 1; i <= 6; i++) {
        PlaceAt(ox + i, oy + 1, COMP_PIPE, DIR_NORTH);
    }
    PlaceAt(ox + 7, oy + 1, COMP_PUMP, DIR_NORTH);
    grid[oy + 1][ox + 7].setting = -2;  // negative = drain
    PlaceAt(ox + 3, oy, COMP_PRESSURE_LIGHT, DIR_NORTH);
    MechPlaceWire(ox + 4, oy);
    PlaceAt(ox + 5, oy, COMP_LIGHT, DIR_EAST);
    for (int i = 0; i < 30; i++) { MechUpdateSignals(); MechUpdateProcessors(); MechUpdateFluids(); }
}

void MechBuildPresetSignalValve(int ox, int oy) {
    PlaceAt(ox, oy + 1, COMP_PUMP, DIR_NORTH);
    grid[oy + 1][ox].setting = 6;
    PlaceAt(ox + 1, oy + 1, COMP_PIPE, DIR_NORTH);
    PlaceAt(ox + 2, oy + 1, COMP_PIPE, DIR_NORTH);
    PlaceAt(ox + 3, oy + 1, COMP_VALVE, DIR_EAST);
    PlaceAt(ox + 4, oy + 1, COMP_PIPE, DIR_NORTH);
    PlaceAt(ox + 5, oy + 1, COMP_PIPE, DIR_NORTH);
    PlaceAt(ox + 6, oy + 1, COMP_PRESSURE_LIGHT, DIR_NORTH);
    MechPlaceWire(ox + 7, oy + 1);
    PlaceAt(ox + 8, oy + 1, COMP_LIGHT, DIR_EAST);
    PlaceAt(ox + 1, oy, COMP_SWITCH, DIR_NORTH);
    MechPlaceWire(ox + 2, oy);
    MechPlaceWire(ox + 3, oy);
    for (int i = 0; i < 20; i++) { MechUpdateSignals(); MechUpdateProcessors(); MechUpdateFluids(); }
}

void MechBuildPresetAnalog(int ox, int oy) {
    PlaceAt(ox, oy + 1, COMP_DIAL, DIR_NORTH);
    grid[oy + 1][ox].setting = 8;
    MechPlaceWire(ox + 1, oy + 1);
    MechPlaceWire(ox + 2, oy + 1);
    PlaceAt(ox + 2, oy, COMP_DISPLAY, DIR_NORTH);
    PlaceAt(ox + 3, oy + 1, COMP_COMPARATOR, DIR_EAST);
    grid[oy + 1][ox + 3].setting = 5;
    MechPlaceWire(ox + 4, oy + 1);
    PlaceAt(ox + 5, oy + 1, COMP_LIGHT, DIR_EAST);
}

void MechBuildPresetBeltLine(int ox, int oy) {
    PlaceAt(ox, oy + 1, COMP_LOADER, DIR_EAST);
    grid[oy + 1][ox].setting = 1;
    for (int i = 1; i <= 4; i++) {
        PlaceAt(ox + i, oy + 1, COMP_BELT, DIR_EAST);
    }
    PlaceAt(ox + 5, oy + 1, COMP_SPLITTER, DIR_EAST);
    PlaceAt(ox + 5, oy, COMP_BELT, DIR_EAST);
    PlaceAt(ox + 6, oy, COMP_BELT, DIR_EAST);
    PlaceAt(ox + 7, oy, COMP_BELT, DIR_EAST);
    PlaceAt(ox + 8, oy, COMP_UNLOADER, DIR_EAST);
    PlaceAt(ox + 5, oy + 2, COMP_BELT, DIR_EAST);
    PlaceAt(ox + 6, oy + 2, COMP_BELT, DIR_EAST);
    PlaceAt(ox + 7, oy + 2, COMP_BELT, DIR_EAST);
    PlaceAt(ox + 8, oy + 2, COMP_UNLOADER, DIR_EAST);
    MechPlaceWire(ox + 9, oy);
    PlaceAt(ox + 9, oy + 1, COMP_DISPLAY, DIR_NORTH);
}

void MechBuildPresetAutoHammer(int ox, int oy) {
    // Crank -> Shaft -> Shaft -> Hammer
    PlaceAt(ox, oy, COMP_CRANK, DIR_EAST);
    grid[oy][ox].state = true;  // Start engaged
    PlaceAt(ox + 1, oy, COMP_SHAFT, DIR_EAST);
    PlaceAt(ox + 2, oy, COMP_SHAFT, DIR_EAST);
    PlaceAt(ox + 3, oy, COMP_HAMMER, DIR_EAST);
    // Let it spin up
    for (int i = 0; i < 20; i++) MechUpdateMechanical();
}

void MechBuildPresetClockTower(int ox, int oy) {
    // Crank -> Shaft -> Flywheel -> Escapement -> Wire -> Light
    PlaceAt(ox, oy, COMP_CRANK, DIR_EAST);
    grid[oy][ox].state = true;
    PlaceAt(ox + 1, oy, COMP_SHAFT, DIR_EAST);
    PlaceAt(ox + 2, oy, COMP_FLYWHEEL, DIR_EAST);
    PlaceAt(ox + 3, oy, COMP_SHAFT, DIR_EAST);
    PlaceAt(ox + 4, oy, COMP_ESCAPEMENT, DIR_EAST);
    MechPlaceWire(ox + 5, oy);
    PlaceAt(ox + 6, oy, COMP_LIGHT, DIR_EAST);
    for (int i = 0; i < 30; i++) MechTick();
}

void MechBuildPresetGovernorLoop(int ox, int oy) {
    // Crank -> Shaft -> Governor -> Wire -> Comparator -> Wire -> Clutch (feedback)
    // The clutch disconnects the crank from the rest when speed is too high
    PlaceAt(ox, oy, COMP_CRANK, DIR_EAST);
    grid[oy][ox].state = true;
    grid[oy][ox].setting = 8;  // high torque
    PlaceAt(ox + 1, oy, COMP_CLUTCH, DIR_EAST);
    PlaceAt(ox + 2, oy, COMP_SHAFT, DIR_EAST);
    PlaceAt(ox + 3, oy, COMP_SHAFT, DIR_EAST);
    PlaceAt(ox + 4, oy, COMP_SHAFT, DIR_EAST);
    PlaceAt(ox + 5, oy, COMP_GOVERNOR, DIR_EAST);
    PlaceAt(ox + 6, oy, COMP_HAMMER, DIR_EAST);  // load
    // Governor signal -> comparator -> NOT -> clutch
    MechPlaceWire(ox + 5, oy + 1);
    PlaceAt(ox + 4, oy + 1, COMP_COMPARATOR, DIR_WEST);
    grid[oy + 1][ox + 4].setting = 8;  // threshold
    MechPlaceWire(ox + 3, oy + 1);
    PlaceAt(ox + 2, oy + 1, COMP_NOT, DIR_WEST);
    MechPlaceWire(ox + 1, oy + 1);
    for (int i = 0; i < 40; i++) MechTick();
}

void MechBuildPresetDemandLoader(int ox, int oy) {
    // Button -> Pulse(5) -> wire-gates a Loader -> Belt -> Belt -> Unloader -> wire -> Display
    // Shows: press button to dispense exactly one burst of cargo
    PlaceAt(ox, oy, COMP_BUTTON, DIR_NORTH);
    MechPlaceWire(ox + 1, oy);
    PlaceAt(ox + 2, oy, COMP_PULSE, DIR_EAST);
    grid[oy][ox + 2].setting = 5;
    MechPlaceWire(ox + 3, oy);
    MechPlaceWire(ox + 3, oy + 1);  // wire down to gate the loader
    PlaceAt(ox + 3, oy + 2, COMP_LOADER, DIR_EAST);
    grid[oy + 2][ox + 3].setting = 4;  // yellow cargo
    PlaceAt(ox + 4, oy + 2, COMP_BELT, DIR_EAST);
    PlaceAt(ox + 5, oy + 2, COMP_BELT, DIR_EAST);
    PlaceAt(ox + 6, oy + 2, COMP_BELT, DIR_EAST);
    PlaceAt(ox + 7, oy + 2, COMP_UNLOADER, DIR_EAST);
    MechPlaceWire(ox + 8, oy + 2);
    PlaceAt(ox + 8, oy + 1, COMP_DISPLAY, DIR_NORTH);
}

void MechBuildPresetSteamHammer(int ox, int oy) {
    // Crank -> Shaft -> [adjacent to Pump] -> Pipes -> PressureLight -> wire -> Light
    // Also: Shaft -> Hammer (mechanical output)
    // Shows: shaft speed boosts pump rate (mechanical -> fluid bridge)
    PlaceAt(ox, oy + 1, COMP_CRANK, DIR_EAST);
    grid[oy + 1][ox].state = true;
    grid[oy + 1][ox].setting = 6;
    PlaceAt(ox + 1, oy + 1, COMP_SHAFT, DIR_EAST);
    PlaceAt(ox + 2, oy + 1, COMP_SHAFT, DIR_EAST);
    PlaceAt(ox + 3, oy + 1, COMP_HAMMER, DIR_EAST);
    // Pump adjacent to shaft at (2, oy+1) — gets speed boost
    PlaceAt(ox + 2, oy, COMP_PUMP, DIR_NORTH);
    grid[oy][ox + 2].setting = 2;  // low base rate, shaft will boost it
    PlaceAt(ox + 3, oy, COMP_PIPE, DIR_NORTH);
    PlaceAt(ox + 4, oy, COMP_PIPE, DIR_NORTH);
    PlaceAt(ox + 5, oy, COMP_PIPE, DIR_NORTH);
    PlaceAt(ox + 6, oy, COMP_PRESSURE_LIGHT, DIR_NORTH);
    MechPlaceWire(ox + 7, oy);
    PlaceAt(ox + 8, oy, COMP_LIGHT, DIR_EAST);
    for (int i = 0; i < 30; i++) MechTick();
}

void MechBuildPresetSortingFactory(int ox, int oy) {
    // Two clock-alternated loaders (type 1, type 2) -> belt -> splitter -> two filter lines
    // Each filter passes its type to an unloader with persistent signal -> display
    // Shows: filtering, splitter, persistent unloader signals, clock-paced loading
    //
    // Layout (7 wide x 5 tall):
    //   row 0: Clock -> wire ---------> wire -> Loader(2)
    //   row 1:                  NOT  -> wire -> Loader(1)
    //   row 2:          Belt <- Belt <- Belt <- Belt
    //   row 3: Display <- wire <- Unloader <- Belt <- Filter(1) <- Splitter
    //   row 4: Display <- wire <- Unloader <- Belt <- Filter(2) <--/
    //
    // Clock alternates: when clock=ON, loader2 loads. NOT inverts for loader1.
    PlaceAt(ox, oy, COMP_CLOCK, DIR_NORTH);
    grid[oy][ox].setting = 6;
    grid[oy][ox].timer = 6;
    MechPlaceWire(ox + 1, oy);
    MechPlaceWire(ox + 2, oy);
    MechPlaceWire(ox + 3, oy);      // wire to loader2
    PlaceAt(ox + 4, oy, COMP_LOADER, DIR_SOUTH);
    grid[oy][ox + 4].setting = 2;   // green cargo
    // NOT branch for loader1
    PlaceAt(ox + 2, oy + 1, COMP_NOT, DIR_EAST);
    MechPlaceWire(ox + 3, oy + 1);
    PlaceAt(ox + 4, oy + 1, COMP_LOADER, DIR_SOUTH);
    grid[oy + 1][ox + 4].setting = 1;  // red cargo
    // Converge onto main belt (row 2, moving west)
    PlaceAt(ox + 4, oy + 2, COMP_BELT, DIR_WEST);
    PlaceAt(ox + 3, oy + 2, COMP_BELT, DIR_WEST);
    PlaceAt(ox + 2, oy + 2, COMP_BELT, DIR_WEST);
    PlaceAt(ox + 1, oy + 2, COMP_SPLITTER, DIR_WEST);
    // North output: filter for type 1 (red)
    PlaceAt(ox + 1, oy + 3, COMP_FILTER, DIR_WEST);
    grid[oy + 3][ox + 1].setting = 1;
    PlaceAt(ox, oy + 3, COMP_UNLOADER, DIR_WEST);
    MechPlaceWire(ox, oy + 4);
    PlaceAt(ox + 1, oy + 4, COMP_DISPLAY, DIR_NORTH);
    // South output: filter for type 2 (green)
    PlaceAt(ox + 1, oy + 5, COMP_FILTER, DIR_WEST);
    grid[oy + 5][ox + 1].setting = 2;
    PlaceAt(ox, oy + 5, COMP_UNLOADER, DIR_WEST);
    MechPlaceWire(ox, oy + 6);
    PlaceAt(ox + 1, oy + 6, COMP_DISPLAY, DIR_NORTH);
    for (int i = 0; i < 30; i++) MechTick();
}

void MechBuildPresetClockworkBottler(int ox, int oy) {
    // Crank -> Shaft -> Escapement -> Wire gates a Loader -> Belt -> Unloader
    // Shows: mechanical timing controls belt logistics (3-layer: mech + signal + belt)
    PlaceAt(ox, oy, COMP_CRANK, DIR_EAST);
    grid[oy][ox].state = true;
    grid[oy][ox].setting = 4;
    PlaceAt(ox + 1, oy, COMP_SHAFT, DIR_EAST);
    PlaceAt(ox + 2, oy, COMP_FLYWHEEL, DIR_EAST);
    grid[oy][ox + 2].setting = 3;
    PlaceAt(ox + 3, oy, COMP_SHAFT, DIR_EAST);
    PlaceAt(ox + 4, oy, COMP_ESCAPEMENT, DIR_EAST);
    MechPlaceWire(ox + 5, oy);
    MechPlaceWire(ox + 5, oy + 1);  // wire down to gate the loader
    PlaceAt(ox + 5, oy + 2, COMP_LOADER, DIR_EAST);
    grid[oy + 2][ox + 5].setting = 6;  // orange cargo
    PlaceAt(ox + 6, oy + 2, COMP_BELT, DIR_EAST);
    PlaceAt(ox + 7, oy + 2, COMP_BELT, DIR_EAST);
    PlaceAt(ox + 8, oy + 2, COMP_BELT, DIR_EAST);
    PlaceAt(ox + 9, oy + 2, COMP_UNLOADER, DIR_EAST);
    MechPlaceWire(ox + 10, oy + 2);
    PlaceAt(ox + 10, oy + 1, COMP_DISPLAY, DIR_NORTH);
    // Let light blink on first row too
    PlaceAt(ox + 6, oy, COMP_LIGHT, DIR_EAST);
    for (int i = 0; i < 40; i++) MechTick();
}
