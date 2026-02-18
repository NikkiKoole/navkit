// mechanisms.h - Signal-based automation simulation
// Extracted from demo.c for testability
#ifndef MECHANISMS_H
#define MECHANISMS_H

#include <stdbool.h>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
#define MECH_GRID_W        32
#define MECH_GRID_H        32
#define MAX_PROCESSORS     64
#define MAX_PROG_LEN       16
#define MECH_MAX_NETWORKS  64

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------
typedef enum {
    COMP_EMPTY = 0,
    COMP_SWITCH,
    COMP_BUTTON,
    COMP_LIGHT,
    COMP_WIRE,
    COMP_NOT,
    COMP_AND,
    COMP_OR,
    COMP_XOR,
    COMP_NOR,
    COMP_LATCH,
    COMP_PROCESSOR,
    COMP_CLOCK,
    COMP_REPEATER,
    COMP_PULSE,
    COMP_PIPE,
    COMP_PUMP,       // Setting > 0 = pump, setting < 0 = drain (click to toggle)
    COMP_VALVE,
    COMP_TANK,
    COMP_PRESSURE_LIGHT,
    COMP_DIAL,
    COMP_COMPARATOR,
    COMP_DISPLAY,
    COMP_BELT,
    COMP_LOADER,
    COMP_UNLOADER,
    COMP_GRABBER,
    COMP_SPLITTER,
    COMP_FILTER,
    COMP_COMPRESSOR,
    COMP_DECOMPRESSOR,
    // Mechanical layer
    COMP_CRANK,
    COMP_SPRING,
    COMP_SHAFT,
    COMP_CLUTCH,
    COMP_FLYWHEEL,
    COMP_ESCAPEMENT,
    COMP_CAM_SHAFT,
    COMP_HAMMER,
    COMP_LEVER_ARM,
    COMP_GOVERNOR,
    COMP_COUNT
} ComponentType;

// ---------------------------------------------------------------------------
// Component layers (for palette grouping)
// ---------------------------------------------------------------------------
typedef enum {
    LAYER_SIGNAL = 0,
    LAYER_FLUID,
    LAYER_BELT,
    LAYER_CPU,
    LAYER_MECHANICAL,
    LAYER_COUNT
} CompLayer;

// ---------------------------------------------------------------------------
// Draw style (for shared rendering)
// ---------------------------------------------------------------------------
typedef enum {
    DRAW_NONE = 0,       // No drawing (COMP_EMPTY)
    DRAW_LABEL,          // Filled rect + centered text label
    DRAW_CIRCLE,         // Filled circle (lights)
    DRAW_GATE,           // Rect + arrow + label + input/output dots
    DRAW_CONNECTED,      // Connected lines to neighbors (wire, shaft, pipe)
    DRAW_CUSTOM,         // Fully custom drawing (belt, flywheel, cam, etc.)
} DrawStyle;

// ---------------------------------------------------------------------------
// Component metadata (data-driven: replaces MechCompName, CompColor, etc.)
// ---------------------------------------------------------------------------
typedef struct {
    const char *name;        // Short display name ("AND", "Shaft", etc.)
    const char *tooltip;     // Palette tooltip description
    unsigned char colorR, colorG, colorB;       // Base color (off/inactive)
    unsigned char activeR, activeG, activeB;    // Active/on color (0,0,0 = no active variant)
    CompLayer layer;
    DrawStyle drawStyle;
    const char *label;       // 1-2 char label for DRAW_LABEL / DRAW_GATE
    bool directional;        // Needs facing arrow + ghost preview arrow
    bool clickConfig;        // Right-click/interact cycles a setting
    int keyCode;             // Raylib KEY_* constant (0 = no key binding)
    const char *keyLabel;    // Label shown in palette ("1", "F3", etc.)
} CompMeta;

typedef enum {
    DIR_NORTH = 0,
    DIR_EAST,
    DIR_SOUTH,
    DIR_WEST
} Direction;

typedef enum {
    OP_NOP = 0,
    OP_READ,
    OP_WRITE,
    OP_SET,
    OP_ADD,
    OP_CMP,
    OP_JIF,
    OP_COUNT
} OpCode;

// ---------------------------------------------------------------------------
// Structures
// ---------------------------------------------------------------------------
typedef struct {
    ComponentType type;
    Direction facing;
    bool state;
    int signalIn;
    int signalOut;
    int procIdx;
    int setting;
    int timer;
    int delayBuf[4];
    int fluidLevel;
    int cargo;
    int cargo2;
    bool altToggle;
    // Mechanical layer
    float mechSpeed;
    float mechTorque;
    int mechNetwork;
    int camPosition;      // cam shaft: current position (0-7)
    float springCharge;   // spring: stored energy (0 to setting)
} Cell;

typedef struct {
    OpCode op;
    int argA, argB, argC;
} Instruction;

typedef struct {
    int x, y;
    int regs[4];
    int pc;
    bool flag;
    Instruction program[MAX_PROG_LEN];
    int progLen;
    bool active;
} Processor;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// Initialization
void MechInit(void);

// Grid access
bool MechInGrid(int x, int y);
Cell* MechGetCell(int x, int y);
int MechGetSignal(int x, int y);

// Placement
void MechPlaceComponent(int gx, int gy, ComponentType type, Direction dir);
void MechPlaceWire(int gx, int gy);

// State manipulation
void MechSetSwitch(int x, int y, bool state);
void MechSetButtonDown(int x, int y, bool down);

// Simulation
void MechTick(void);              // All systems: signals + processors + fluids + belts
void MechUpdateSignals(void);
void MechUpdateProcessors(void);
void MechUpdateFluids(void);
void MechUpdateBelts(void);
void MechUpdateMechanical(void);

// Helpers
void MechDirOffset(Direction d, int *dx, int *dy);
Direction MechOppositeDir(Direction d);
void MechGateInputDirs(Direction facing, Direction *inA, Direction *inB);
const char *MechCompName(ComponentType t);
const char *MechOpName(OpCode op);
const char *MechDirName(Direction d);
const CompMeta *MechGetCompMeta(ComponentType t);

// Processor
int MechFindProcessor(int gx, int gy);
Processor* MechGetProcessor(int idx);

// Preset builders
void MechBuildPresetNot(int ox, int oy);
void MechBuildPresetAnd(int ox, int oy);
void MechBuildPresetBlinker(int ox, int oy);
void MechBuildPresetNorLatch(int ox, int oy);
void MechBuildPresetHalfAdder(int ox, int oy);
void MechBuildPresetPumpLoop(int ox, int oy);
void MechBuildPresetSignalValve(int ox, int oy);
void MechBuildPresetAnalog(int ox, int oy);
void MechBuildPresetBeltLine(int ox, int oy);
void MechBuildPresetAutoHammer(int ox, int oy);
void MechBuildPresetClockTower(int ox, int oy);
void MechBuildPresetGovernorLoop(int ox, int oy);
void MechBuildPresetDemandLoader(int ox, int oy);
void MechBuildPresetSteamHammer(int ox, int oy);
void MechBuildPresetSortingFactory(int ox, int oy);
void MechBuildPresetClockworkBottler(int ox, int oy);

#endif // MECHANISMS_H
