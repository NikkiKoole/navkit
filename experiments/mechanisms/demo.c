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

// Include simulation logic (unity-style)
#include "mechanisms.c"

// ---------------------------------------------------------------------------
// Demo-only constants
// ---------------------------------------------------------------------------
#define SCREEN_WIDTH  1280
#define SCREEN_HEIGHT 800
#define CELL_SIZE     20
#define GRID_OFFSET_X 40
#define GRID_OFFSET_Y 40
#define TICK_INTERVAL  0.1f

// ---------------------------------------------------------------------------
// Demo-only enums & state
// ---------------------------------------------------------------------------
typedef enum {
    MODE_PLACE = 0,
    MODE_INTERACT,
    MODE_PROC_EDIT
} InteractionMode;

static ComponentType selectedComp = COMP_SWITCH;
static Direction placingDir = DIR_NORTH;
static InteractionMode mode = MODE_PLACE;
static bool simPaused = false;
static float tickTimer = 0.0f;

// Processor editor state
static int editProcIdx = -1;
static int editLine = 0;
static int editField = 0;

// Signal animation
static float pulseTime = 0.0f;

// ---------------------------------------------------------------------------
// Screen helpers
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Preset table
// ---------------------------------------------------------------------------
typedef struct {
    const char *name;
    const char *description;
    const char *tooltip;
    void (*build)(int ox, int oy);
    int width, height;
} Preset;

static Preset presets[] = {
    { "NOT", "Switch -> NOT -> Light",
      "The simplest inverter. The NOT gate outputs 1 when its input is 0 and vice versa. "
      "Toggle the switch to see the light flip. This is the foundation of all digital logic — "
      "chain NOT gates to build oscillators, or combine with AND/OR for complex conditions.",
      MechBuildPresetNot, 6, 3 },

    { "AND", "2 Switches -> AND -> Light",
      "Two switches feed the side inputs of an AND gate. The light only turns on when BOTH "
      "switches are on. AND gates are essential for conditional logic — use them to require "
      "multiple conditions before triggering an action, like a two-key safety interlock.",
      MechBuildPresetAnd, 6, 3 },

    { "XOR", "2 Switches -> XOR -> Light",
      "Exclusive OR: the light turns on when exactly one switch is on, but not both. "
      "XOR is the core of binary addition — the Half Adder preset uses one for the sum bit. "
      "Also useful for toggle circuits where two inputs should cancel each other out.",
      MechBuildPresetXor, 6, 3 },

    { "Blinker", "Clock -> Light",
      "A clock auto-toggles on a timer and drives a light, creating a steady blink. "
      "Click the clock to change its period (1-8 ticks). This is the simplest autonomous circuit — "
      "no user input needed. Use clocks to drive sequencers, test timing, or pace belt loaders.",
      MechBuildPresetBlinker, 3, 3 },

    { "Pulse Extend", "Button -> Repeater(4) -> Light",
      "A button emits a brief pulse, but the repeater stretches it over 4 ticks so the light "
      "stays on longer. Useful when a momentary input needs to hold a signal — for example, "
      "keeping a valve open long enough for fluid to flow, or giving a grabber time to act.",
      MechBuildPresetPulseExtend, 5, 3 },

    { "NOR Latch", "NOR gates + pulse extenders, memory",
      "A proper Set/Reset latch built from cross-coupled NOR gates with pulse extenders on the "
      "inputs. Push one button to set (light stays on), push the other to reset (light turns off). "
      "This is 1 bit of memory — the foundation of counters, state machines, and registers.",
      MechBuildPresetNorLatch, 8, 8 },

    { "Half Adder", "XOR=sum, AND=carry, binary math",
      "Adds two 1-bit inputs: the XOR gate produces the sum, the AND gate produces the carry. "
      "Toggle both switches on to see sum=0, carry=1 (binary: 1+1=10). Chain half adders into "
      "full adders to build multi-bit arithmetic. This is how CPUs do math at the gate level.",
      MechBuildPresetHalfAdder, 9, 5 },

    { "Ring Osc", "3 NOT gates in a loop, auto-osc",
      "Three NOT gates connected in a ring: each inverts the previous, creating an unstable loop "
      "that oscillates forever. Unlike a clock, the frequency is determined by gate propagation "
      "delay (2 ticks per gate = 6-tick cycle). A classic digital circuit with no external input.",
      MechBuildPresetRingOsc, 4, 3 },

    { "Pump Loop", "Pump -> pipes -> drain + light",
      "A pump pushes fluid through a pipe network while a drain pulls it out, creating steady "
      "flow. A pressure light at the end converts fluid pressure into a signal. This demonstrates "
      "the fluid layer: pressure equalizes between pipes, and valves can gate the flow.",
      MechBuildPresetPumpLoop, 8, 2 },

    { "Sig Valve", "Switch controls valve, fluid->light",
      "A pump fills pipes, but a valve in the middle blocks flow until a switch activates it via "
      "wire signal. Flip the switch to let fluid reach the pressure light downstream. This shows "
      "how the signal and fluid layers interact — use it to build signal-controlled plumbing.",
      MechBuildPresetSignalValve, 9, 2 },

    { "Analog", "Dial -> display + comparator -> light",
      "A dial outputs an analog value (0-15) onto a wire. A display shows the numeric value, "
      "and a comparator triggers a light when the value exceeds its threshold. Click the dial "
      "to sweep through values and see the comparator flip. Foundation for analog control systems.",
      MechBuildPresetAnalog, 6, 2 },

    { "Belt Line", "Loader -> belts -> splitter -> unload",
      "A loader spawns cargo onto a conveyor belt. The belt carries items to a splitter, which "
      "alternates them left and right to two unloaders. Unloaders consume cargo and emit signals. "
      "This is the basic logistics pattern — add filters to sort by type, or grabbers for routing.",
      MechBuildPresetBeltLine, 11, 4 },

    { "Compress", "Compress 2 belts -> decompress to 2",
      "Two belt lines feed into a compressor, which merges their cargo into dual-cargo items on "
      "a single belt. A decompressor downstream splits them back into two separate lines. "
      "Doubles belt throughput through tight spaces. The key to efficient belt-based factories.",
      MechBuildPresetCompress, 11, 5 },

    // Mechanical presets
    { "Auto-Hammer", "Crank -> Shaft -> Hammer",
      "The simplest mechanical circuit. A crank provides torque, a shaft carries it, and a hammer "
      "strikes when the shaft speed exceeds its threshold. Click the crank to engage/disengage and "
      "watch the hammer start and stop. The mechanical equivalent of Switch -> Wire -> Light.",
      MechBuildPresetAutoHammer, 4, 1 },

    { "Geared Mill", "Crank -> GRatio -> Shafts -> Hammer",
      "A longer shaft line with a gear ratio component for visual flair. The crank drives through "
      "a ratio label and multiple shaft segments to reach the hammer. Shows how shaft networks "
      "share speed across all connected segments regardless of distance.",
      MechBuildPresetGearedMill, 6, 1 },

    { "Clock Tower", "Crank -> Flywheel -> Escape -> Light",
      "A mechanical clock: the crank turns a shaft through a flywheel (which smooths speed changes) "
      "to an escapement. The escapement converts steady rotation into periodic signal pulses that "
      "blink a light. The flywheel keeps the tick rate stable even as torque fluctuates.",
      MechBuildPresetClockTower, 7, 1 },

    { "Cam Sequence", "Crank -> CamShaft -> Wires -> Hammers",
      "A cam shaft reads an 8-bit pattern and outputs signal pulses in sequence as it rotates. "
      "Each bit controls a wire connected to a different hammer, creating a programmable firing "
      "pattern. Click the cam shaft to cycle through patterns like alternating, burst, and sweep.",
      MechBuildPresetCamSequencer, 5, 5 },

    { "Spring Trap", "Spring -> Clutch -> Shaft -> Lever",
      "A spring winds up passively, storing energy. A clutch blocks the shaft until a wire signal "
      "engages it. When the switch is flipped, the clutch connects and the spring releases a burst "
      "of torque through the shaft to extend a lever arm. A one-shot mechanical trigger.",
      MechBuildPresetSpringTrap, 8, 2 },

    { "Gov. Loop", "Crank -> Governor -> feedback Clutch",
      "A self-regulating speed controller. The crank drives a shaft with a governor that outputs "
      "an analog signal proportional to speed. A comparator checks if speed is too high and cuts "
      "a clutch to disconnect the crank, letting friction slow things down. Speed stabilizes at "
      "the comparator's threshold — a mechanical feedback loop.",
      MechBuildPresetGovernorLoop, 7, 2 },
};
#define PRESET_COUNT (sizeof(presets) / sizeof(presets[0]))

static int selectedPreset = -1;

// ---------------------------------------------------------------------------
// Drawing helpers
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
        // Mechanical - warm copper/brass palette
        case COMP_CRANK:      return state ? (Color){220, 160, 60, 255} : (Color){140, 100, 40, 255};
        case COMP_SPRING:     return state ? (Color){200, 140, 50, 255} : (Color){120, 80, 30, 255};
        case COMP_SHAFT:      return (Color){180, 140, 80, 255};
        case COMP_GEAR:       return (Color){170, 130, 70, 255};
        case COMP_GEAR_RATIO: return (Color){190, 150, 80, 255};
        case COMP_CLUTCH:     return state ? (Color){200, 160, 70, 255} : (Color){80, 60, 30, 255};
        case COMP_FLYWHEEL:   return (Color){120, 120, 130, 255};
        case COMP_ESCAPEMENT: return state ? (Color){220, 190, 60, 255} : (Color){140, 120, 40, 255};
        case COMP_CAM_SHAFT:  return state ? (Color){200, 170, 60, 255} : (Color){130, 110, 40, 255};
        case COMP_HAMMER:     return state ? (Color){180, 180, 180, 255} : (Color){100, 100, 110, 255};
        case COMP_LEVER_ARM:  return state ? (Color){170, 170, 180, 255} : (Color){90, 90, 100, 255};
        case COMP_GOVERNOR:   return state ? (Color){220, 190, 80, 255} : (Color){140, 120, 50, 255};
        default:             return DARKGRAY;
    }
}

static Color CargoColor(int cargo) {
    switch (cargo) {
        case 1:  return RED;
        case 2:  return (Color){50, 200, 50, 255};
        case 3:  return BLUE;
        case 4:  return YELLOW;
        case 5:  return PURPLE;
        case 6:  return ORANGE;
        case 7:  return (Color){0, 200, 200, 255};
        case 8:  return WHITE;
        case 9:  return PINK;
        case 10: return (Color){180, 120, 60, 255};
        case 11: return LIME;
        case 12: return SKYBLUE;
        case 13: return MAGENTA;
        case 14: return GOLD;
        case 15: return MAROON;
        default: return GRAY;
    }
}

static void DrawArrow(int cx, int cy, Direction dir, Color col) {
    int s = CELL_SIZE / 2 - 2;
    int dx, dy;
    MechDirOffset(dir, &dx, &dy);
    int tipX = cx + dx * s;
    int tipY = cy + dy * s;
    DrawLine(cx, cy, tipX, tipY, col);
    int perpX = -dy, perpY = dx;
    DrawLine(tipX, tipY, tipX - dx * 3 + perpX * 3, tipY - dy * 3 + perpY * 3, col);
    DrawLine(tipX, tipY, tipX - dx * 3 - perpX * 3, tipY - dy * 3 - perpY * 3, col);
}

static void DrawGridBackground(void) {
    for (int y = 0; y < MECH_GRID_H; y++) {
        for (int x = 0; x < MECH_GRID_W; x++) {
            Rectangle r = CellRect(x, y);
            DrawRectangleRec(r, (Color){30, 30, 35, 255});
            DrawRectangleLinesEx(r, 1, (Color){50, 50, 55, 255});
        }
    }
}

static void DrawComponents(void) {
    for (int y = 0; y < MECH_GRID_H; y++) {
        for (int x = 0; x < MECH_GRID_W; x++) {
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
                        MechDirOffset((Direction)d, &dx, &dy);
                        int nx = x + dx, ny = y + dy;
                        if (MechInGrid(nx, ny) && grid[ny][nx].type != COMP_EMPTY) {
                            connected[d] = true;
                        }
                    }
                    Color wireCol = col;
                    int sigVal = MechGetSignal(x, y);
                    if (sigVal > 0) {
                        float intensity = (float)sigVal / 15.0f;
                        float pulse = 0.6f + 0.4f * sinf(pulseTime * 6.0f);
                        unsigned char g = (unsigned char)(80 + 175 * intensity * pulse);
                        unsigned char rr = (unsigned char)(40 * intensity * pulse);
                        wireCol = (Color){rr, g, 0, 255};
                    }
                    bool any = false;
                    for (int d = 0; d < 4; d++) {
                        if (connected[d]) {
                            any = true;
                            int dx, dy;
                            MechDirOffset((Direction)d, &dx, &dy);
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

                    int edge = CELL_SIZE / 2 - 1;
                    {
                        int dx, dy;
                        MechDirOffset(c->facing, &dx, &dy);
                        DrawCircle(cx + dx * edge, cy + dy * edge, 3, GREEN);
                    }
                    if (c->type == COMP_NOT) {
                        int dx, dy;
                        MechDirOffset(MechOppositeDir(c->facing), &dx, &dy);
                        DrawCircle(cx + dx * edge, cy + dy * edge, 3, ORANGE);
                    } else {
                        Direction inA, inB;
                        MechGateInputDirs(c->facing, &inA, &inB);
                        int dx, dy;
                        MechDirOffset(inA, &dx, &dy);
                        DrawCircle(cx + dx * edge, cy + dy * edge, 3, ORANGE);
                        MechDirOffset(inB, &dx, &dy);
                        DrawCircle(cx + dx * edge, cy + dy * edge, 3, ORANGE);
                    }
                    break;
                }

                case COMP_LATCH: {
                    DrawRectangleRec(r, col);
                    DrawArrow(cx, cy, c->facing, WHITE);
                    DrawTextShadow("M", cx - 4, cy - 5, 10, WHITE);

                    int edge = CELL_SIZE / 2 - 1;
                    {
                        int dx, dy;
                        MechDirOffset(c->facing, &dx, &dy);
                        DrawCircle(cx + dx * edge, cy + dy * edge, 3, GREEN);
                    }
                    {
                        Direction setDir, resetDir;
                        MechGateInputDirs(c->facing, &setDir, &resetDir);
                        int dx, dy;
                        MechDirOffset(setDir, &dx, &dy);
                        DrawCircle(cx + dx * edge, cy + dy * edge, 3, (Color){0, 200, 0, 255});
                        MechDirOffset(resetDir, &dx, &dy);
                        DrawCircle(cx + dx * edge, cy + dy * edge, 3, (Color){200, 0, 0, 255});
                    }
                    break;
                }

                case COMP_PROCESSOR: {
                    DrawRectangleRec(r, col);
                    DrawTextShadow("C", cx - 3, cy - 5, 10, WHITE);
                    for (int d = 0; d < 4; d++) {
                        int dx, dy;
                        MechDirOffset((Direction)d, &dx, &dy);
                        DrawCircle(cx + dx * (CELL_SIZE / 2 - 2),
                                   cy + dy * (CELL_SIZE / 2 - 2), 2, YELLOW);
                    }
                    break;
                }

                case COMP_CLOCK: {
                    DrawRectangleRec(r, col);
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
                        MechDirOffset(c->facing, &dx, &dy);
                        DrawCircle(cx + dx * edge, cy + dy * edge, 3, GREEN);
                        MechDirOffset(MechOppositeDir(c->facing), &dx, &dy);
                        DrawCircle(cx + dx * edge, cy + dy * edge, 3, ORANGE);
                    }
                    char delBuf[4];
                    snprintf(delBuf, sizeof(delBuf), "%d", c->setting);
                    DrawTextShadow(delBuf, cx - 3, cy - 5, 10, WHITE);
                    break;
                }

                case COMP_PIPE: {
                    int level = c->fluidLevel;
                    int maxLvl = 255;
                    float frac = (float)level / (float)maxLvl;
                    unsigned char b = (unsigned char)(80 + 175 * frac);
                    unsigned char g = (unsigned char)(40 + 180 * frac);
                    Color pipeCol = {0, g, b, 255};
                    DrawRectangleRec(r, pipeCol);
                    for (int d = 0; d < 4; d++) {
                        int dx, dy;
                        MechDirOffset((Direction)d, &dx, &dy);
                        int nx = x + dx, ny = y + dy;
                        if (MechInGrid(nx, ny) && grid[ny][nx].type >= COMP_PIPE && grid[ny][nx].type <= COMP_PRESSURE_LIGHT) {
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
                    int level = c->fluidLevel;
                    float frac = (float)level / 255.0f;
                    Color vCol = c->state
                        ? (Color){0, (unsigned char)(60 + 160 * frac), (unsigned char)(100 + 155 * frac), 255}
                        : (Color){60, 40, 40, 255};
                    DrawRectangleRec(r, vCol);
                    DrawArrow(cx, cy, c->facing, WHITE);
                    if (!c->state) {
                        int dx, dy;
                        MechDirOffset(c->facing, &dx, &dy);
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
                    int maxLvl = 1024;
                    float frac = (float)level / (float)maxLvl;
                    unsigned char b = (unsigned char)(40 + 180 * frac);
                    unsigned char g = (unsigned char)(20 + 120 * frac);
                    Color tankCol = {0, g, b, 255};
                    Rectangle tr = {r.x - 1, r.y - 1, r.width + 2, r.height + 2};
                    DrawRectangleRec(tr, tankCol);
                    DrawRectangleLinesEx(r, 1, (Color){60, 60, 100, 255});
                    DrawTextShadow("T", cx - 3, cy - 5, 10, WHITE);
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
                        MechDirOffset(c->facing, &dx, &dy);
                        DrawCircle(cx + dx * edge, cy + dy * edge, 3, GREEN);
                        MechDirOffset(MechOppositeDir(c->facing), &dx, &dy);
                        DrawCircle(cx + dx * edge, cy + dy * edge, 3, ORANGE);
                    }
                    char cmpBuf[4];
                    snprintf(cmpBuf, sizeof(cmpBuf), "%d", c->setting);
                    DrawTextShadow(cmpBuf, cx - 3, cy - 5, 10, WHITE);
                    break;
                }

                case COMP_DISPLAY: {
                    DrawRectangleRec(r, col);
                    DrawRectangleLinesEx(r, 1, (Color){60, 60, 70, 255});
                    int val = c->setting;
                    char dispBuf[4];
                    if (val > 9) {
                        snprintf(dispBuf, sizeof(dispBuf), "%X", val);
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
                    float anim = fmodf(pulseTime * 3.0f, 1.0f);
                    int dx, dy;
                    MechDirOffset(c->facing, &dx, &dy);
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
                    if (c->cargo > 0 && c->cargo2 > 0) {
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
                    DrawCircle(cx, cy - 2, 3, CargoColor(c->setting));
                    char ldBuf[4];
                    snprintf(ldBuf, sizeof(ldBuf), "%d", c->setting);
                    DrawTextShadow(ldBuf, cx + 3, cy - 5, 10, WHITE);
                    break;
                }

                case COMP_UNLOADER: {
                    DrawRectangleRec(r, col);
                    DrawTextShadow("U", cx - 3, cy - 5, 10, WHITE);
                    if (c->cargo > 0) {
                        DrawCircle(cx, cy + 4, 3, CargoColor(c->cargo));
                    }
                    break;
                }

                case COMP_GRABBER: {
                    DrawRectangleRec(r, col);
                    int dx, dy;
                    MechDirOffset(c->facing, &dx, &dy);
                    int edge = CELL_SIZE / 2 - 2;
                    DrawLineEx((Vector2){(float)(cx - dx * edge), (float)(cy - dy * edge)},
                               (Vector2){(float)(cx + dx * edge), (float)(cy + dy * edge)},
                               3.0f, c->state ? YELLOW : GRAY);
                    DrawCircle(cx + dx * edge, cy + dy * edge, 3, c->state ? GREEN : GRAY);
                    DrawCircle(cx - dx * edge, cy - dy * edge, 3, ORANGE);
                    break;
                }

                case COMP_SPLITTER: {
                    DrawRectangleRec(r, col);
                    int dx, dy;
                    MechDirOffset(MechOppositeDir(c->facing), &dx, &dy);
                    int edge = CELL_SIZE / 2 - 2;
                    DrawLineEx((Vector2){(float)(cx + dx * edge), (float)(cy + dy * edge)},
                               (Vector2){(float)cx, (float)cy}, 2.0f, WHITE);
                    Direction leftDir, rightDir;
                    MechGateInputDirs(c->facing, &rightDir, &leftDir);
                    MechDirOffset(leftDir, &dx, &dy);
                    DrawLineEx((Vector2){(float)cx, (float)cy},
                               (Vector2){(float)(cx + dx * edge), (float)(cy + dy * edge)}, 2.0f, WHITE);
                    MechDirOffset(rightDir, &dx, &dy);
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
                    DrawCircle(cx, cy - 2, 3, CargoColor(c->setting));
                    char fBuf[4];
                    snprintf(fBuf, sizeof(fBuf), "F%d", c->setting);
                    DrawTextShadow(fBuf, cx - 5, cy - 5, 10, WHITE);
                    if (c->cargo > 0 && c->cargo != c->setting) {
                        DrawCircle(cx - 4, cy + 4, 2, CargoColor(c->cargo));
                    }
                    break;
                }

                case COMP_COMPRESSOR: {
                    DrawRectangleRec(r, col);
                    int dx, dy;
                    int edge = CELL_SIZE / 2 - 2;
                    Direction rightDir, leftDir;
                    MechGateInputDirs(c->facing, &rightDir, &leftDir);
                    MechDirOffset(leftDir, &dx, &dy);
                    DrawLineEx((Vector2){(float)(cx + dx * edge), (float)(cy + dy * edge)},
                               (Vector2){(float)cx, (float)cy}, 2.0f, WHITE);
                    MechDirOffset(rightDir, &dx, &dy);
                    DrawLineEx((Vector2){(float)(cx + dx * edge), (float)(cy + dy * edge)},
                               (Vector2){(float)cx, (float)cy}, 2.0f, WHITE);
                    MechDirOffset(c->facing, &dx, &dy);
                    DrawLineEx((Vector2){(float)cx, (float)cy},
                               (Vector2){(float)(cx + dx * edge), (float)(cy + dy * edge)}, 2.0f, WHITE);
                    MechDirOffset(leftDir, &dx, &dy);
                    DrawCircle(cx + dx * edge, cy + dy * edge, 3, ORANGE);
                    MechDirOffset(rightDir, &dx, &dy);
                    DrawCircle(cx + dx * edge, cy + dy * edge, 3, ORANGE);
                    MechDirOffset(c->facing, &dx, &dy);
                    DrawCircle(cx + dx * edge, cy + dy * edge, 3, GREEN);
                    DrawTextShadow("><", cx - 5, cy - 5, 10, WHITE);
                    break;
                }

                case COMP_DECOMPRESSOR: {
                    DrawRectangleRec(r, col);
                    int dx, dy;
                    int edge = CELL_SIZE / 2 - 2;
                    MechDirOffset(MechOppositeDir(c->facing), &dx, &dy);
                    DrawLineEx((Vector2){(float)(cx + dx * edge), (float)(cy + dy * edge)},
                               (Vector2){(float)cx, (float)cy}, 2.0f, WHITE);
                    MechDirOffset(c->facing, &dx, &dy);
                    DrawLineEx((Vector2){(float)cx, (float)cy},
                               (Vector2){(float)(cx + dx * edge), (float)(cy + dy * edge)}, 2.0f, WHITE);
                    Direction rightDir, leftDir;
                    MechGateInputDirs(c->facing, &rightDir, &leftDir);
                    MechDirOffset(rightDir, &dx, &dy);
                    DrawLineEx((Vector2){(float)cx, (float)cy},
                               (Vector2){(float)(cx + dx * edge), (float)(cy + dy * edge)}, 2.0f, WHITE);
                    MechDirOffset(MechOppositeDir(c->facing), &dx, &dy);
                    DrawCircle(cx + dx * edge, cy + dy * edge, 3, ORANGE);
                    MechDirOffset(c->facing, &dx, &dy);
                    DrawCircle(cx + dx * edge, cy + dy * edge, 3, GREEN);
                    MechDirOffset(rightDir, &dx, &dy);
                    DrawCircle(cx + dx * edge, cy + dy * edge, 3, GREEN);
                    DrawTextShadow("<>", cx - 5, cy - 5, 10, WHITE);
                    break;
                }

                // ----- Mechanical layer -----
                case COMP_CRANK: {
                    DrawRectangleRec(r, col);
                    DrawTextShadow(c->state ? "C+" : "C-", cx - 6, cy - 5, 10, WHITE);
                    // Speed indicator
                    if (c->mechSpeed > 0) {
                        float barW = (c->mechSpeed / 100.0f) * (CELL_SIZE - 4);
                        DrawRectangle((int)r.x + 2, (int)(r.y + r.height - 4), (int)barW, 3, (Color){255, 200, 50, 180});
                    }
                    break;
                }

                case COMP_SPRING: {
                    DrawRectangleRec(r, col);
                    // Show charge level as compressed coil
                    float charge = c->springCharge / (float)(c->setting > 0 ? c->setting : 1);
                    int segments = 3;
                    int sw = CELL_SIZE - 6;
                    for (int i = 0; i < segments; i++) {
                        int sx = (int)r.x + 3 + (i * sw / segments);
                        int w = sw / segments - 1;
                        int h = (int)(3 + 4 * charge);
                        DrawRectangle(sx, cy - h/2, w, h, (Color){255, 200, 100, 200});
                    }
                    break;
                }

                case COMP_SHAFT: {
                    // Like wire but copper-colored, with rotation animation
                    bool connected[4] = {false};
                    for (int d = 0; d < 4; d++) {
                        int dx, dy;
                        MechDirOffset((Direction)d, &dx, &dy);
                        int nx = x + dx, ny = y + dy;
                        if (MechInGrid(nx, ny) && IsMechCell(grid[ny][nx].type)) {
                            connected[d] = true;
                        }
                    }
                    Color shaftCol = col;
                    if (c->mechSpeed > 0) {
                        float pulse = 0.7f + 0.3f * sinf(pulseTime * c->mechSpeed * 0.5f);
                        shaftCol.r = (unsigned char)(shaftCol.r * pulse);
                        shaftCol.g = (unsigned char)(shaftCol.g * pulse);
                    }
                    bool any = false;
                    int edge = CELL_SIZE / 2;
                    for (int d = 0; d < 4; d++) {
                        if (connected[d]) {
                            any = true;
                            int dx, dy;
                            MechDirOffset((Direction)d, &dx, &dy);
                            DrawLineEx((Vector2){(float)cx, (float)cy},
                                       (Vector2){(float)(cx + dx * edge), (float)(cy + dy * edge)}, 3.0f, shaftCol);
                        }
                    }
                    if (!any) DrawCircle(cx, cy, 3, shaftCol);
                    break;
                }

                case COMP_GEAR: {
                    DrawRectangleRec(r, col);
                    DrawTextShadow("G", cx - 3, cy - 5, 10, WHITE);
                    // Draw 4 teeth indicators
                    int ts = CELL_SIZE / 2 - 2;
                    for (int d = 0; d < 4; d++) {
                        int dx, dy;
                        MechDirOffset((Direction)d, &dx, &dy);
                        DrawRectangle(cx + dx * ts - 1, cy + dy * ts - 1, 3, 3, (Color){220, 180, 80, 200});
                    }
                    break;
                }

                case COMP_GEAR_RATIO: {
                    DrawRectangleRec(r, col);
                    DrawArrow(cx, cy, c->facing, WHITE);
                    char grBuf[8];
                    snprintf(grBuf, sizeof(grBuf), "1:%d", c->setting);
                    DrawTextShadow(grBuf, cx - 8, cy - 5, 10, WHITE);
                    break;
                }

                case COMP_CLUTCH: {
                    DrawRectangleRec(r, col);
                    DrawTextShadow(c->state ? "CL" : "cl", cx - 6, cy - 5, 10, c->state ? WHITE : GRAY);
                    break;
                }

                case COMP_FLYWHEEL: {
                    DrawCircle(cx, cy, CELL_SIZE / 2 - 1, col);
                    if (c->mechSpeed > 0) {
                        float angle = pulseTime * c->mechSpeed * 0.3f;
                        int rx = (int)(cosf(angle) * (CELL_SIZE / 2 - 3));
                        int ry = (int)(sinf(angle) * (CELL_SIZE / 2 - 3));
                        DrawLine(cx - rx, cy - ry, cx + rx, cy + ry, WHITE);
                    }
                    char fBuf[4];
                    snprintf(fBuf, sizeof(fBuf), "%d", c->setting);
                    DrawTextShadow(fBuf, cx - 3, cy - 5, 10, WHITE);
                    break;
                }

                case COMP_ESCAPEMENT: {
                    DrawRectangleRec(r, col);
                    DrawArrow(cx, cy, c->facing, WHITE);
                    DrawTextShadow("E", cx - 3, cy - 5, 10, WHITE);
                    // Output dot
                    int edg = CELL_SIZE / 2 - 1;
                    int dx, dy;
                    MechDirOffset(c->facing, &dx, &dy);
                    DrawCircle(cx + dx * edg, cy + dy * edg, 3, c->state ? GREEN : (Color){40, 80, 40, 255});
                    break;
                }

                case COMP_CAM_SHAFT: {
                    DrawRectangleRec(r, col);
                    DrawArrow(cx, cy, c->facing, WHITE);
                    // Draw 8-bit pattern as dots around center
                    int pattern = c->setting & 0xFF;
                    for (int b = 0; b < 8; b++) {
                        float angle = (float)b * 3.14159f * 2.0f / 8.0f - 3.14159f / 2.0f;
                        int px = cx + (int)(cosf(angle) * 6);
                        int py = cy + (int)(sinf(angle) * 6);
                        bool bit = (pattern >> b) & 1;
                        Color dotCol = (b == c->camPosition) ? YELLOW : (bit ? (Color){200, 170, 60, 255} : (Color){60, 50, 30, 255});
                        DrawCircle(px, py, (b == c->camPosition) ? 2 : 1, dotCol);
                    }
                    break;
                }

                case COMP_HAMMER: {
                    DrawRectangleRec(r, col);
                    // Animated hammer strike
                    if (c->state && c->timer < 2) {
                        DrawRectangle(cx - 3, cy - 6, 6, 4, WHITE);  // head up
                    } else {
                        DrawRectangle(cx - 3, cy + 2, 6, 4, (Color){160, 160, 170, 255});  // head down
                    }
                    DrawLine(cx, cy - 2, cx, cy + 6, (Color){120, 100, 60, 255});  // handle
                    break;
                }

                case COMP_LEVER_ARM: {
                    DrawRectangleRec(r, col);
                    DrawArrow(cx, cy, c->facing, WHITE);
                    // Extended/retracted indicator
                    int edg = CELL_SIZE / 2 - 1;
                    int dx, dy;
                    MechDirOffset(c->facing, &dx, &dy);
                    Color tipCol = c->state ? GREEN : (Color){80, 80, 90, 255};
                    DrawCircle(cx + dx * edg, cy + dy * edg, 3, tipCol);
                    break;
                }

                case COMP_GOVERNOR: {
                    DrawRectangleRec(r, col);
                    // Spinning balls - spread wider at higher speed
                    float spread = 2.0f + (c->mechSpeed / 100.0f) * 5.0f;
                    float angle = pulseTime * 4.0f;
                    int bx1 = cx + (int)(cosf(angle) * spread);
                    int by1 = cy + (int)(sinf(angle) * spread);
                    int bx2 = cx - (int)(cosf(angle) * spread);
                    int by2 = cy - (int)(sinf(angle) * spread);
                    DrawCircle(bx1, by1, 2, (Color){255, 220, 100, 255});
                    DrawCircle(bx2, by2, 2, (Color){255, 220, 100, 255});
                    DrawLine(bx1, by1, bx2, by2, (Color){160, 130, 60, 150});
                    break;
                }

                default:
                    break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// UI: Palette
// ---------------------------------------------------------------------------
#define PALETTE_ROW_H  24
#define PALETTE_PAD    4
#define PALETTE_ROWS   5
#define PALETTE_BAR_H  (PALETTE_ROW_H * PALETTE_ROWS + PALETTE_PAD * (PALETTE_ROWS + 1))

static const ComponentType electricalItems[] = {
    COMP_SWITCH, COMP_BUTTON, COMP_LIGHT, COMP_WIRE, COMP_NOT,
    COMP_AND, COMP_OR, COMP_XOR, COMP_NOR, COMP_LATCH, COMP_CLOCK, COMP_REPEATER, COMP_PULSE,
    COMP_DIAL, COMP_COMPARATOR
};
static const char *electricalKeys[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "Q", "W", "E", "A", "X", "V" };
#define ELECTRICAL_COUNT (int)(sizeof(electricalItems) / sizeof(electricalItems[0]))

static const ComponentType fluidItems[] = {
    COMP_PIPE, COMP_PUMP, COMP_DRAIN, COMP_VALVE, COMP_TANK, COMP_PRESSURE_LIGHT
};
static const char *fluidKeys[] = { "S", "D", "G", "H", "J", "K" };
#define FLUID_COUNT (int)(sizeof(fluidItems) / sizeof(fluidItems[0]))

static const ComponentType beltItems[] = {
    COMP_BELT, COMP_LOADER, COMP_UNLOADER, COMP_GRABBER, COMP_SPLITTER, COMP_FILTER,
    COMP_COMPRESSOR, COMP_DECOMPRESSOR
};
static const char *beltKeys[] = { ",", ".", "/", ";", "'", "\\", "[", "]" };
#define BELT_COUNT (int)(sizeof(beltItems) / sizeof(beltItems[0]))

static const ComponentType processorItems[] = {
    COMP_PROCESSOR, COMP_DISPLAY, COMP_EMPTY
};
static const char *processorKeys[] = { "Z", "B", "0" };
#define PROCESSOR_COUNT (int)(sizeof(processorItems) / sizeof(processorItems[0]))

static const ComponentType mechanicalItems[] = {
    COMP_CRANK, COMP_SPRING, COMP_SHAFT, COMP_GEAR, COMP_GEAR_RATIO,
    COMP_CLUTCH, COMP_FLYWHEEL, COMP_ESCAPEMENT, COMP_CAM_SHAFT,
    COMP_HAMMER, COMP_LEVER_ARM, COMP_GOVERNOR
};
static const char *mechanicalKeys[] = { "F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12" };
#define MECHANICAL_COUNT (int)(sizeof(mechanicalItems) / sizeof(mechanicalItems[0]))

// ---------------------------------------------------------------------------
// Palette tooltip descriptions — shown when hovering a palette button
// ---------------------------------------------------------------------------
static const char *CompTooltipDesc(ComponentType t) {
    switch (t) {
        // Signal layer
        case COMP_SWITCH:       return "Toggle on/off with click. Powers adjacent wires. Use as manual input for any circuit.";
        case COMP_BUTTON:       return "Emits a brief pulse while held. Good for triggering one-shot events like latches or pulses.";
        case COMP_LIGHT:        return "Lights up when it receives signal. The simplest output — use to visualize any wire state.";
        case COMP_WIRE:         return "Carries signal between components. Connects in all 4 directions. The backbone of every circuit.";
        case COMP_NOT:          return "Outputs 1 when input is 0, and vice versa. Directional. Essential for inverters and oscillators.";
        case COMP_AND:          return "Outputs 1 only when both side inputs are on. Directional. Use for conditional logic and gating.";
        case COMP_OR:           return "Outputs 1 when either side input is on. Directional. Combines multiple signal sources.";
        case COMP_XOR:          return "Outputs 1 when exactly one input is on. Directional. Key building block for adders and toggles.";
        case COMP_NOR:          return "Outputs 1 only when both inputs are off. Directional. Two NOR gates make an SR latch (memory).";
        case COMP_LATCH:        return "Set/Reset memory cell. Set input turns it on, reset turns it off, stays until changed. Click: N/A.";
        case COMP_CLOCK:        return "Auto-toggles every N ticks (click to change period 1-8). Use for blinking, timing, and sequencing.";
        case COMP_REPEATER:     return "Delays signal by N ticks (click to change 1-4). Directional. Use to time circuits or extend pulses.";
        case COMP_PULSE:        return "Stretches a brief input into a longer pulse (click to change duration 1-8). Good after buttons.";
        case COMP_DIAL:         return "Outputs an analog value 0-15 (click to change). Source for analog circuits, displays, comparators.";
        case COMP_COMPARATOR:   return "Outputs 1 when analog input >= threshold (click to set 1-15). Directional. Analog-to-digital.";
        // Fluid layer
        case COMP_PIPE:         return "Carries fluid pressure between neighbors. Equalizes with adjacent pipes and tanks each tick.";
        case COMP_PUMP:         return "Generates fluid pressure (click to set rate 1-8). Source for fluid networks. Always active.";
        case COMP_DRAIN:        return "Removes fluid pressure (click to set rate 1-8). Sink for fluid networks. Prevents pressure buildup.";
        case COMP_VALVE:        return "Blocks fluid flow unless adjacent wire has signal. Directional. Use switches to control fluid routing.";
        case COMP_TANK:         return "Stores fluid up to 1024 pressure (4x pipe capacity). Acts as a buffer to smooth pressure spikes.";
        case COMP_PRESSURE_LIGHT: return "Converts fluid pressure to analog signal 0-15. Bridges the fluid and signal layers.";
        // Belt layer
        case COMP_BELT:         return "Moves cargo one cell per tick in its facing direction. Chain belts together for conveyor lines.";
        case COMP_LOADER:       return "Spawns cargo onto the next belt (click to set type 1-15). The source for all belt logistics.";
        case COMP_UNLOADER:     return "Consumes cargo and emits its type as signal. The sink for belt lines. Bridges belts to wires.";
        case COMP_GRABBER:      return "Signal-controlled inserter. Moves cargo from behind to ahead when wire signal is on. Directional.";
        case COMP_SPLITTER:     return "Alternates cargo left and right. Directional. Use to balance two output lines from one input.";
        case COMP_FILTER:       return "Only passes cargo matching its type (click to set 1-15). Rejects others. Directional sorter.";
        case COMP_COMPRESSOR:   return "Merges two side belt inputs into one dual-cargo item. Directional. Doubles belt throughput.";
        case COMP_DECOMPRESSOR: return "Splits dual-cargo: primary forward, secondary to side. Directional. Reverses a compressor.";
        // CPU layer
        case COMP_PROCESSOR:    return "Tiny 6-opcode CPU. Reads/writes ports on 4 sides. Press P to edit program. The ultimate component.";
        case COMP_DISPLAY:      return "Shows the analog value (0-15) from an adjacent wire as a colored bar. Passive readout.";
        case COMP_EMPTY:        return "Eraser. Click to remove any component. Shortcut: 0 or right-click.";
        // Mechanical layer
        case COMP_CRANK:        return "Click to engage/disengage. Outputs constant torque to the shaft network. The primary power source.";
        case COMP_SPRING:       return "Winds up over time. Signal on adjacent wire releases stored energy as a torque burst. Click: capacity.";
        case COMP_SHAFT:        return "Carries mechanical speed between neighbors. All connected shafts share speed. Like wire, but for torque.";
        case COMP_GEAR:         return "Connects perpendicular shafts. Place at a corner to turn a shaft line 90 degrees.";
        case COMP_GEAR_RATIO:   return "Displays a gear ratio label (click to cycle 1:1 to 1:4). Cosmetic in v1 — speed is shared. Directional.";
        case COMP_CLUTCH:       return "Disconnects the shaft network when no wire signal. Engage via adjacent wire. Mechanical valve.";
        case COMP_FLYWHEEL:     return "Adds inertia to the network (click to set 1-8). Resists speed changes — smooths out torque spikes.";
        case COMP_ESCAPEMENT:   return "Converts shaft speed into periodic signal pulses. Faster speed = faster ticks. Directional output.";
        case COMP_CAM_SHAFT:    return "8-bit pattern sequencer driven by shaft speed. Outputs signal when current bit is set. Click: pattern.";
        case COMP_HAMMER:       return "Strikes when shaft speed > 5. Consumes torque as load (click: 1-8). Visual up/down animation.";
        case COMP_LEVER_ARM:    return "Extends when shaft speed > 1 and emits wire signal. Directional. Bridges mechanical to signal layer.";
        case COMP_GOVERNOR:     return "Outputs analog signal 0-15 proportional to shaft speed. Bridges mechanical to signal for feedback loops.";
        default:                return "";
    }
}

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
        snprintf(label, sizeof(label), "%s:%s", keyLabels[i], MechCompName(items[i]));
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
        int presetPerRow = ((int)PRESET_COUNT + 1) / 2;
        int rowH = (PALETTE_BAR_H - PALETTE_PAD * 3) / 2;
        for (int i = 0; i < (int)PRESET_COUNT; i++) {
            int row = i / presetPerRow;
            int col = i % presetPerRow;
            int colCount = (row == 0) ? presetPerRow : (int)PRESET_COUNT - presetPerRow;
            int rowY = barY + PALETTE_PAD + row * (rowH + PALETTE_PAD);
            int bx = 6 + col * (SCREEN_WIDTH - 12) / colCount;
            int itemW = (SCREEN_WIDTH - 12) / colCount - 4;
            bool sel = (i == selectedPreset);
            Color bg = sel ? (Color){80, 60, 20, 255} : (Color){40, 40, 45, 255};
            DrawRectangle(bx, rowY, itemW, rowH, bg);
            if (sel) DrawRectangleLinesEx((Rectangle){(float)bx, (float)rowY, (float)itemW, (float)rowH}, 2, YELLOW);

            char label[48];
            snprintf(label, sizeof(label), "%d:%s", i + 1, presets[i].name);
            DrawTextShadow(label, bx + 4, rowY + 4, 10, WHITE);
            DrawTextShadow(presets[i].description, bx + 4, rowY + 16, 10, (Color){160, 160, 160, 255});
        }
    } else {
        int row1Y = barY + PALETTE_PAD;
        DrawPaletteRow(electricalItems, electricalKeys, ELECTRICAL_COUNT, row1Y, (Color){70, 70, 80, 255});
        int row2Y = row1Y + PALETTE_ROW_H + PALETTE_PAD;
        DrawPaletteRow(fluidItems, fluidKeys, FLUID_COUNT, row2Y, (Color){40, 60, 90, 255});
        int row3Y = row2Y + PALETTE_ROW_H + PALETTE_PAD;
        DrawPaletteRow(beltItems, beltKeys, BELT_COUNT, row3Y, (Color){80, 70, 40, 255});
        int row4Y = row3Y + PALETTE_ROW_H + PALETTE_PAD;
        DrawPaletteRow(processorItems, processorKeys, PROCESSOR_COUNT, row4Y, (Color){70, 40, 90, 255});
        int row5Y = row4Y + PALETTE_ROW_H + PALETTE_PAD;
        DrawPaletteRow(mechanicalItems, mechanicalKeys, MECHANICAL_COUNT, row5Y, (Color){100, 75, 30, 255});

        DrawTextShadow("SIGNAL", SCREEN_WIDTH - 52, row1Y + 7, 10, (Color){100, 100, 110, 255});
        DrawTextShadow("FLUID", SCREEN_WIDTH - 48, row2Y + 7, 10, (Color){60, 100, 140, 255});
        DrawTextShadow("BELT", SCREEN_WIDTH - 40, row3Y + 7, 10, (Color){140, 120, 60, 255});
        DrawTextShadow("CPU", SCREEN_WIDTH - 36, row4Y + 7, 10, (Color){120, 80, 160, 255});
        DrawTextShadow("MECH", SCREEN_WIDTH - 40, row5Y + 7, 10, (Color){180, 140, 80, 255});
    }
}

// Returns the ComponentType under the mouse in a given palette row, or -1
static int PaletteRowHitTest(const ComponentType *items, int count, int rowY, int mx, int my) {
    if (my < rowY || my > rowY + PALETTE_ROW_H) return -1;
    for (int i = 0; i < count; i++) {
        int bx = 6 + i * (SCREEN_WIDTH - 12) / count;
        int itemW = (SCREEN_WIDTH - 12) / count - 4;
        if (mx >= bx && mx <= bx + itemW) return items[i];
    }
    return -1;
}

// Word-wrap text into lines that fit within maxWidth pixels at the given fontSize.
// Returns line count. lines[] must be pre-allocated char buffers of lineBufSize.
static int WrapText(const char *text, int maxWidth, int fontSize, char lines[][256], int maxLines) {
    int lineCount = 0;
    const char *p = text;
    while (*p && lineCount < maxLines) {
        // Find how many words fit on this line
        char *out = lines[lineCount];
        int outLen = 0;
        out[0] = '\0';

        while (*p) {
            // Find next word
            const char *wordStart = p;
            while (*p && *p != ' ') p++;
            int wordLen = (int)(p - wordStart);
            while (*p == ' ') p++;

            // Try adding this word
            char test[256];
            if (outLen > 0) {
                snprintf(test, sizeof(test), "%.*s %.*s", outLen, out, wordLen, wordStart);
            } else {
                snprintf(test, sizeof(test), "%.*s", wordLen, wordStart);
            }

            int testW = MeasureTextUI(test, fontSize);
            if (testW > maxWidth && outLen > 0) {
                // This word doesn't fit — start a new line
                p = wordStart;
                while (*p == ' ') p++;
                break;
            }

            // It fits — commit
            snprintf(out, 256, "%s", test);
            outLen = (int)strlen(out);
        }

        if (outLen > 0) lineCount++;
    }
    return lineCount;
}

static void DrawPaletteTooltip(void) {
    int mx = GetMouseX(), my = GetMouseY();
    int barY = SCREEN_HEIGHT - PALETTE_BAR_H;
    if (my < barY) return;

    const char *title = NULL;
    const char *body = NULL;
    int fontSize = 10;

    if (selectedPreset >= 0) {
        // Preset mode: hit-test the 2-row preset layout
        int presetPerRow = ((int)PRESET_COUNT + 1) / 2;
        int rowH = (PALETTE_BAR_H - PALETTE_PAD * 3) / 2;
        int hitIdx = -1;
        for (int i = 0; i < (int)PRESET_COUNT; i++) {
            int row = i / presetPerRow;
            int col = i % presetPerRow;
            int colCount = (row == 0) ? presetPerRow : (int)PRESET_COUNT - presetPerRow;
            int rowY = barY + PALETTE_PAD + row * (rowH + PALETTE_PAD);
            int bx = 6 + col * (SCREEN_WIDTH - 12) / colCount;
            int itemW = (SCREEN_WIDTH - 12) / colCount - 4;
            if (mx >= bx && mx <= bx + itemW && my >= rowY && my <= rowY + rowH) {
                hitIdx = i; break;
            }
        }
        if (hitIdx < 0) return;
        title = presets[hitIdx].name;
        body = presets[hitIdx].tooltip;
    } else {
        // Component mode: hit-test palette rows
        int row1Y = barY + PALETTE_PAD;
        int row2Y = row1Y + PALETTE_ROW_H + PALETTE_PAD;
        int row3Y = row2Y + PALETTE_ROW_H + PALETTE_PAD;
        int row4Y = row3Y + PALETTE_ROW_H + PALETTE_PAD;
        int row5Y = row4Y + PALETTE_ROW_H + PALETTE_PAD;

        int hit = PaletteRowHitTest(electricalItems, ELECTRICAL_COUNT, row1Y, mx, my);
        if (hit < 0) hit = PaletteRowHitTest(fluidItems, FLUID_COUNT, row2Y, mx, my);
        if (hit < 0) hit = PaletteRowHitTest(beltItems, BELT_COUNT, row3Y, mx, my);
        if (hit < 0) hit = PaletteRowHitTest(processorItems, PROCESSOR_COUNT, row4Y, mx, my);
        if (hit < 0) hit = PaletteRowHitTest(mechanicalItems, MECHANICAL_COUNT, row5Y, mx, my);
        if (hit < 0) return;

        title = MechCompName((ComponentType)hit);
        body = CompTooltipDesc((ComponentType)hit);
    }

    if (!body || body[0] == '\0') return;

    // Word-wrap the body text
    int maxTooltipW = 500;
    char wrappedLines[8][256];
    int lineCount = WrapText(body, maxTooltipW - 12, fontSize, wrappedLines, 8);

    // Measure box dimensions
    int lineH = 14;
    int titleH = 16;
    int pad = 6;
    int boxH = pad + titleH + lineCount * lineH + pad;

    int boxW = 0;
    int titleW = MeasureTextUI(title, fontSize + 2) + pad * 2;
    if (titleW > boxW) boxW = titleW;
    for (int i = 0; i < lineCount; i++) {
        int lw = MeasureTextUI(wrappedLines[i], fontSize) + pad * 2;
        if (lw > boxW) boxW = lw;
    }
    if (boxW > maxTooltipW) boxW = maxTooltipW;

    int tx = mx - boxW / 2;
    int ty = barY - boxH - 4;

    // Clamp to screen
    if (tx < 2) tx = 2;
    if (tx + boxW > SCREEN_WIDTH - 2) tx = SCREEN_WIDTH - 2 - boxW;
    if (ty < 2) ty = 2;

    DrawRectangle(tx, ty, boxW, boxH, (Color){15, 15, 20, 245});
    DrawRectangleLinesEx((Rectangle){(float)tx, (float)ty, (float)boxW, (float)boxH}, 1, (Color){80, 80, 90, 200});

    // Title line (slightly larger, brighter)
    DrawTextShadow(title, tx + pad, ty + pad, fontSize + 2, (Color){255, 220, 130, 255});

    // Body lines
    for (int i = 0; i < lineCount; i++) {
        DrawTextShadow(wrappedLines[i], tx + pad, ty + pad + titleH + i * lineH, fontSize, (Color){200, 200, 210, 255});
    }
}

static bool HandlePaletteClick(int mx, int my) {
    int barY = SCREEN_HEIGHT - PALETTE_BAR_H;
    if (my < barY) return false;

    if (selectedPreset >= 0) {
        int presetPerRow = ((int)PRESET_COUNT + 1) / 2;
        int rowH = (PALETTE_BAR_H - PALETTE_PAD * 3) / 2;
        for (int i = 0; i < (int)PRESET_COUNT; i++) {
            int row = i / presetPerRow;
            int col = i % presetPerRow;
            int colCount = (row == 0) ? presetPerRow : (int)PRESET_COUNT - presetPerRow;
            int rowY = barY + PALETTE_PAD + row * (rowH + PALETTE_PAD);
            int bx = 6 + col * (SCREEN_WIDTH - 12) / colCount;
            int itemW = (SCREEN_WIDTH - 12) / colCount - 4;
            if (mx >= bx && mx <= bx + itemW && my >= rowY && my <= rowY + rowH) {
                selectedPreset = i;
                return true;
            }
        }
        return true;
    }

    int row1Y = barY + PALETTE_PAD;
    int row2Y = row1Y + PALETTE_ROW_H + PALETTE_PAD;
    int row3Y = row2Y + PALETTE_ROW_H + PALETTE_PAD;
    int row4Y = row3Y + PALETTE_ROW_H + PALETTE_PAD;
    if (HandlePaletteRowClick(electricalItems, ELECTRICAL_COUNT, row1Y, mx, my)) return true;
    if (HandlePaletteRowClick(fluidItems, FLUID_COUNT, row2Y, mx, my)) return true;
    if (HandlePaletteRowClick(beltItems, BELT_COUNT, row3Y, mx, my)) return true;
    if (HandlePaletteRowClick(processorItems, PROCESSOR_COUNT, row4Y, mx, my)) return true;
    int row5Y = row4Y + PALETTE_ROW_H + PALETTE_PAD;
    if (HandlePaletteRowClick(mechanicalItems, MECHANICAL_COUNT, row5Y, mx, my)) return true;
    return true;
}

// ---------------------------------------------------------------------------
// UI: Status bar
// ---------------------------------------------------------------------------
static void DrawStatusBar(void) {
    DrawRectangle(0, 0, SCREEN_WIDTH, 30, (Color){20, 20, 25, 255});

    char status[256];
    if (selectedPreset >= 0) {
        snprintf(status, sizeof(status),
                 "PRESETS: Click to stamp \"%s\" | [1-%d]=Select [F/ESC]=Exit presets | Sim: %s",
                 presets[selectedPreset].name, (int)PRESET_COUNT,
                 simPaused ? "PAUSED" : "RUNNING");
    } else if (mode == MODE_INTERACT) {
        snprintf(status, sizeof(status),
                 "Mode: INTERACT | Sim: %s | Click components to toggle/adjust | [1-9]=Place mode [F]=Presets [Space]=Pause",
                 simPaused ? "PAUSED" : "RUNNING");
    } else {
        const char *modeStr = (mode == MODE_PROC_EDIT) ? "PROC EDIT" : "PLACE";
        snprintf(status, sizeof(status),
                 "Mode: %s | Sim: %s | Dir: %s | [F]=Presets [ESC]=Interact [Space]=Pause [T]=Step [R]=Rotate [C]=Clear",
                 modeStr, simPaused ? "PAUSED" : "RUNNING", MechDirName(placingDir));
    }
    DrawTextShadow(status, 10, 8, 10, (Color){200, 200, 200, 255});
}

// ---------------------------------------------------------------------------
// UI: Processor editor
// ---------------------------------------------------------------------------
static void DrawProcessorEditor(void) {
    if (editProcIdx < 0) return;
    Processor *p = MechGetProcessor(editProcIdx);
    if (!p || !p->active) return;

    int panelX = GRID_OFFSET_X + MECH_GRID_W * CELL_SIZE + 20;
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

    // Port reads use internal function — just show signal grid values
    snprintf(buf, sizeof(buf), "Ports IN: N=%d E=%d S=%d W=%d",
             MechGetSignal(p->x, p->y - 1), MechGetSignal(p->x + 1, p->y),
             MechGetSignal(p->x, p->y + 1), MechGetSignal(p->x - 1, p->y));
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
        DrawTextShadow(MechOpName(inst->op), panelX + 50, ly, 10, opcCol);

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
// UI: Tooltip
// ---------------------------------------------------------------------------
static void DrawCellTooltip(int gx, int gy) {
    if (!MechInGrid(gx, gy)) return;
    Cell *c = &grid[gy][gx];
    if (c->type == COMP_EMPTY) return;

    char buf[256];
    if (c->type == COMP_CLOCK) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] state=%s period=%d timer=%d (click to change period)",
                 MechCompName(c->type), gx, gy, c->state ? "ON" : "OFF", c->setting, c->timer);
    } else if (c->type == COMP_REPEATER) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] state=%s delay=%d dir=%s (click to change delay)",
                 MechCompName(c->type), gx, gy, c->state ? "ON" : "OFF", c->setting, MechDirName(c->facing));
    } else if (c->type == COMP_PULSE) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] state=%s duration=%d timer=%d dir=%s (click to change duration)",
                 MechCompName(c->type), gx, gy, c->state ? "ON" : "OFF", c->setting, c->timer, MechDirName(c->facing));
    } else if (c->type == COMP_PIPE || c->type == COMP_TANK) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] pressure=%d/%d",
                 MechCompName(c->type), gx, gy, c->fluidLevel, (c->type == COMP_TANK) ? 1024 : 255);
    } else if (c->type == COMP_PUMP) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] rate=%d active=%s pressure=%d (click to change rate)",
                 MechCompName(c->type), gx, gy, c->setting, c->state ? "YES" : "NO", c->fluidLevel);
    } else if (c->type == COMP_DRAIN) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] rate=%d pressure=%d (click to change rate)",
                 MechCompName(c->type), gx, gy, c->setting, c->fluidLevel);
    } else if (c->type == COMP_VALVE) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] %s pressure=%d dir=%s",
                 MechCompName(c->type), gx, gy, c->state ? "OPEN" : "CLOSED", c->fluidLevel, MechDirName(c->facing));
    } else if (c->type == COMP_PRESSURE_LIGHT) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] %s out=%d (pressure->analog 0-15)",
                 MechCompName(c->type), gx, gy, c->state ? "ON" : "OFF", c->signalOut);
    } else if (c->type == COMP_DIAL) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] value=%d (click to change 0-15)",
                 MechCompName(c->type), gx, gy, c->setting);
    } else if (c->type == COMP_COMPARATOR) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] %s threshold=%d input=%d dir=%s (click to change)",
                 MechCompName(c->type), gx, gy, c->state ? "ON" : "OFF", c->setting, c->signalIn, MechDirName(c->facing));
    } else if (c->type == COMP_DISPLAY) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] value=%d",
                 MechCompName(c->type), gx, gy, c->setting);
    } else if (c->type == COMP_BELT) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] dir=%s cargo=%d",
                 MechCompName(c->type), gx, gy, MechDirName(c->facing), c->cargo);
    } else if (c->type == COMP_LOADER) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] type=%d dir=%s (click to change type)",
                 MechCompName(c->type), gx, gy, c->setting, MechDirName(c->facing));
    } else if (c->type == COMP_UNLOADER) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] last=%d (consumes cargo, emits signal)",
                 MechCompName(c->type), gx, gy, c->signalOut);
    } else if (c->type == COMP_GRABBER) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] %s dir=%s (signal-controlled inserter)",
                 MechCompName(c->type), gx, gy, c->state ? "ACTIVE" : "IDLE", MechDirName(c->facing));
    } else if (c->type == COMP_SPLITTER) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] dir=%s next=%s cargo=%d",
                 MechCompName(c->type), gx, gy, MechDirName(c->facing),
                 c->altToggle ? "LEFT" : "RIGHT", c->cargo);
    } else if (c->type == COMP_FILTER) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] pass=%d dir=%s cargo=%d (click to change)",
                 MechCompName(c->type), gx, gy, c->setting, MechDirName(c->facing), c->cargo);
    } else if (c->type == COMP_COMPRESSOR) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] dir=%s (merges 2 side inputs into dual cargo)",
                 MechCompName(c->type), gx, gy, MechDirName(c->facing));
    } else if (c->type == COMP_DECOMPRESSOR) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] dir=%s side=%s (splits dual cargo to fwd+side)",
                 MechCompName(c->type), gx, gy, MechDirName(c->facing),
                 c->altToggle ? "LEFT" : "RIGHT");
    // Mechanical tooltips
    } else if (c->type == COMP_CRANK) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] %s torque=%d speed=%.1f (click to toggle)",
                 MechCompName(c->type), gx, gy, c->state ? "ENGAGED" : "OFF", c->setting, c->mechSpeed);
    } else if (c->type == COMP_SPRING) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] capacity=%d charge=%.1f speed=%.1f (click to change, signal to release)",
                 MechCompName(c->type), gx, gy, c->setting, c->springCharge, c->mechSpeed);
    } else if (c->type == COMP_SHAFT) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] speed=%.1f net=%d",
                 MechCompName(c->type), gx, gy, c->mechSpeed, c->mechNetwork);
    } else if (c->type == COMP_GEAR) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] speed=%.1f (connects perpendicular shafts)",
                 MechCompName(c->type), gx, gy, c->mechSpeed);
    } else if (c->type == COMP_GEAR_RATIO) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] ratio=1:%d speed=%.1f dir=%s (click to change, cosmetic v1)",
                 MechCompName(c->type), gx, gy, c->setting, c->mechSpeed, MechDirName(c->facing));
    } else if (c->type == COMP_CLUTCH) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] %s speed=%.1f (wire signal engages)",
                 MechCompName(c->type), gx, gy, c->state ? "ENGAGED" : "DISENGAGED", c->mechSpeed);
    } else if (c->type == COMP_FLYWHEEL) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] inertia=%d speed=%.1f (click to change, resists speed changes)",
                 MechCompName(c->type), gx, gy, c->setting, c->mechSpeed);
    } else if (c->type == COMP_ESCAPEMENT) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] %s speed=%.1f dir=%s (emits pulses at speed/10 rate)",
                 MechCompName(c->type), gx, gy, c->state ? "TICK" : "TOCK", c->mechSpeed, MechDirName(c->facing));
    } else if (c->type == COMP_CAM_SHAFT) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] pattern=0x%02X pos=%d %s speed=%.1f dir=%s (click to change pattern)",
                 MechCompName(c->type), gx, gy, c->setting, c->camPosition,
                 c->state ? "ON" : "OFF", c->mechSpeed, MechDirName(c->facing));
    } else if (c->type == COMP_HAMMER) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] %s load=%d speed=%.1f (click to change load)",
                 MechCompName(c->type), gx, gy, c->state ? "STRIKE!" : "ready", c->setting, c->mechSpeed);
    } else if (c->type == COMP_LEVER_ARM) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] %s load=%d speed=%.1f dir=%s (click to change load)",
                 MechCompName(c->type), gx, gy, c->state ? "EXTENDED" : "retracted", c->setting, c->mechSpeed, MechDirName(c->facing));
    } else if (c->type == COMP_GOVERNOR) {
        snprintf(buf, sizeof(buf), "%s [%d,%d] speed=%.1f output=%d/15 (writes analog to adjacent wires)",
                 MechCompName(c->type), gx, gy, c->mechSpeed, (int)(c->mechSpeed * 15.0f / 100.0f));
    } else {
        snprintf(buf, sizeof(buf), "%s [%d,%d] state=%s sigIn=%d sigOut=%d dir=%s",
                 MechCompName(c->type), gx, gy, c->state ? "ON" : "OFF",
                 c->signalIn, c->signalOut, MechDirName(c->facing));
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

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && (mode == MODE_PLACE || mode == MODE_INTERACT)) {
        if (HandlePaletteClick(mx, my)) { mode = MODE_PLACE; return; }
    }

    if (IsKeyPressed(KEY_F) && (mode == MODE_PLACE || mode == MODE_INTERACT)) {
        if (selectedPreset >= 0) selectedPreset = -1;
        else selectedPreset = 0;
        mode = MODE_PLACE;
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
        if (IsKeyPressed(KEY_LEFT) && selectedPreset > 0) selectedPreset--;
        if (IsKeyPressed(KEY_RIGHT) && selectedPreset < (int)PRESET_COUNT - 1) selectedPreset++;

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && MechInGrid(gx, gy)) {
            presets[selectedPreset].build(gx, gy);
        }

        if (IsKeyPressed(KEY_ESCAPE)) selectedPreset = -1;
        return;
    }

    // Component selection keys — also exit interact mode
    if (mode == MODE_PLACE || mode == MODE_INTERACT) {
        ComponentType prevComp = selectedComp;
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
        // Mechanical layer (F1-F12)
        if (IsKeyPressed(KEY_F1))    selectedComp = COMP_CRANK;
        if (IsKeyPressed(KEY_F2))    selectedComp = COMP_SPRING;
        if (IsKeyPressed(KEY_F3))    selectedComp = COMP_SHAFT;
        if (IsKeyPressed(KEY_F4))    selectedComp = COMP_GEAR;
        if (IsKeyPressed(KEY_F5))    selectedComp = COMP_GEAR_RATIO;
        if (IsKeyPressed(KEY_F6))    selectedComp = COMP_CLUTCH;
        if (IsKeyPressed(KEY_F7))    selectedComp = COMP_FLYWHEEL;
        if (IsKeyPressed(KEY_F8))    selectedComp = COMP_ESCAPEMENT;
        if (IsKeyPressed(KEY_F9))    selectedComp = COMP_CAM_SHAFT;
        if (IsKeyPressed(KEY_F10))   selectedComp = COMP_HAMMER;
        if (IsKeyPressed(KEY_F11))   selectedComp = COMP_LEVER_ARM;
        if (IsKeyPressed(KEY_F12))   selectedComp = COMP_GOVERNOR;
        if (IsKeyPressed(KEY_ZERO))  selectedComp = COMP_EMPTY;
        if (selectedComp != prevComp && mode == MODE_INTERACT) mode = MODE_PLACE;
    }

    if (IsKeyPressed(KEY_R)) placingDir = (Direction)((placingDir + 1) % 4);
    if (IsKeyPressed(KEY_C) && mode == MODE_PLACE) MechInit();
    if (IsKeyPressed(KEY_SPACE)) simPaused = !simPaused;
    if (IsKeyPressed(KEY_T) && simPaused) MechTick();

    if (IsKeyPressed(KEY_P) && mode == MODE_PLACE) {
        if (MechInGrid(gx, gy) && grid[gy][gx].type == COMP_PROCESSOR) {
            editProcIdx = grid[gy][gx].procIdx;
            if (editProcIdx >= 0) {
                mode = MODE_PROC_EDIT;
                editLine = 0;
                editField = 0;
            }
        }
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        if (mode == MODE_PROC_EDIT) {
            mode = MODE_INTERACT;
            editProcIdx = -1;
        } else if (mode == MODE_PLACE) {
            mode = MODE_INTERACT;
            selectedPreset = -1;
        }
    }

    // Release all buttons every frame
    for (int by = 0; by < MECH_GRID_H; by++)
        for (int bx = 0; bx < MECH_GRID_W; bx++)
            if (grid[by][bx].type == COMP_BUTTON)
                grid[by][bx].state = false;

    switch (mode) {
        case MODE_PLACE: {
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && MechInGrid(gx, gy)) {
                if (grid[gy][gx].type == COMP_BUTTON) grid[gy][gx].state = true;
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && MechInGrid(gx, gy)) {
                Cell *clicked = &grid[gy][gx];
                if (clicked->type == COMP_BUTTON) {
                    // handled above
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
                    clicked->setting = (clicked->setting + 1) % 16;
                } else if (selectedComp == COMP_COMPARATOR && clicked->type == COMP_COMPARATOR) {
                    clicked->setting = (clicked->setting % 15) + 1;
                } else if (selectedComp == COMP_LOADER && clicked->type == COMP_LOADER) {
                    clicked->setting = (clicked->setting % 15) + 1;
                } else if (selectedComp == COMP_FILTER && clicked->type == COMP_FILTER) {
                    clicked->setting = (clicked->setting % 15) + 1;
                // Mechanical click-to-edit
                } else if (selectedComp == COMP_CRANK && clicked->type == COMP_CRANK) {
                    clicked->state = !clicked->state;
                } else if (selectedComp == COMP_SPRING && clicked->type == COMP_SPRING) {
                    clicked->setting = (clicked->setting % 15) + 1;
                } else if (selectedComp == COMP_GEAR_RATIO && clicked->type == COMP_GEAR_RATIO) {
                    clicked->setting = (clicked->setting % 4) + 1;
                } else if (selectedComp == COMP_FLYWHEEL && clicked->type == COMP_FLYWHEEL) {
                    clicked->setting = (clicked->setting % 8) + 1;
                } else if (selectedComp == COMP_CAM_SHAFT && clicked->type == COMP_CAM_SHAFT) {
                    // Cycle through useful patterns
                    static const int patterns[] = {0xAA, 0x55, 0xFF, 0x0F, 0x33, 0xC3, 0x01, 0x81};
                    int cur = 0;
                    for (int i = 0; i < 8; i++) if (clicked->setting == patterns[i]) { cur = i; break; }
                    clicked->setting = patterns[(cur + 1) % 8];
                } else if (selectedComp == COMP_HAMMER && clicked->type == COMP_HAMMER) {
                    clicked->setting = (clicked->setting % 8) + 1;
                } else if (selectedComp == COMP_LEVER_ARM && clicked->type == COMP_LEVER_ARM) {
                    clicked->setting = (clicked->setting % 8) + 1;
                } else {
                    MechPlaceComponent(gx, gy, selectedComp, placingDir);
                }
            }

            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && MechInGrid(gx, gy)) {
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
                                      || (selectedComp == COMP_FILTER && clicked->type == COMP_FILTER)
                                      || (selectedComp == COMP_CRANK && clicked->type == COMP_CRANK)
                                      || (selectedComp == COMP_SPRING && clicked->type == COMP_SPRING)
                                      || (selectedComp == COMP_GEAR_RATIO && clicked->type == COMP_GEAR_RATIO)
                                      || (selectedComp == COMP_FLYWHEEL && clicked->type == COMP_FLYWHEEL)
                                      || (selectedComp == COMP_CAM_SHAFT && clicked->type == COMP_CAM_SHAFT)
                                      || (selectedComp == COMP_HAMMER && clicked->type == COMP_HAMMER)
                                      || (selectedComp == COMP_LEVER_ARM && clicked->type == COMP_LEVER_ARM);
                    if (!isClickConfig) {
                        MechPlaceComponent(gx, gy, selectedComp, placingDir);
                    }
                }
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && MechInGrid(gx, gy)) {
                if (grid[gy][gx].type == COMP_EMPTY) placingDir = (Direction)((placingDir + 1) % 4);
                else MechPlaceComponent(gx, gy, COMP_EMPTY, DIR_NORTH);
            }
            if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && !IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && MechInGrid(gx, gy)) {
                if (grid[gy][gx].type != COMP_EMPTY) MechPlaceComponent(gx, gy, COMP_EMPTY, DIR_NORTH);
            }
            break;
        }

        case MODE_INTERACT: {
            // Hold button
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && MechInGrid(gx, gy)) {
                if (grid[gy][gx].type == COMP_BUTTON) grid[gy][gx].state = true;
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && MechInGrid(gx, gy)) {
                Cell *clicked = &grid[gy][gx];
                switch (clicked->type) {
                    case COMP_SWITCH: clicked->state = !clicked->state; break;
                    case COMP_CLOCK:
                        clicked->setting = (clicked->setting % 8) + 1;
                        clicked->timer = clicked->setting;
                        break;
                    case COMP_REPEATER:
                        clicked->setting = (clicked->setting % 4) + 1;
                        memset(clicked->delayBuf, 0, sizeof(clicked->delayBuf));
                        break;
                    case COMP_PULSE: clicked->setting = (clicked->setting % 8) + 1; break;
                    case COMP_PUMP:  clicked->setting = (clicked->setting % 8) + 1; break;
                    case COMP_DRAIN: clicked->setting = (clicked->setting % 8) + 1; break;
                    case COMP_DIAL:  clicked->setting = (clicked->setting + 1) % 16; break;
                    case COMP_COMPARATOR: clicked->setting = (clicked->setting % 15) + 1; break;
                    case COMP_LOADER: clicked->setting = (clicked->setting % 15) + 1; break;
                    case COMP_FILTER: clicked->setting = (clicked->setting % 15) + 1; break;
                    case COMP_CRANK: clicked->state = !clicked->state; break;
                    case COMP_SPRING: clicked->setting = (clicked->setting % 15) + 1; break;
                    case COMP_GEAR_RATIO: clicked->setting = (clicked->setting % 4) + 1; break;
                    case COMP_FLYWHEEL: clicked->setting = (clicked->setting % 8) + 1; break;
                    case COMP_CAM_SHAFT: {
                        static const int patterns[] = {0xAA, 0x55, 0xFF, 0x0F, 0x33, 0xC3, 0x01, 0x81};
                        int cur = 0;
                        for (int i = 0; i < 8; i++) if (clicked->setting == patterns[i]) { cur = i; break; }
                        clicked->setting = patterns[(cur + 1) % 8];
                        break;
                    }
                    case COMP_HAMMER: clicked->setting = (clicked->setting % 8) + 1; break;
                    case COMP_LEVER_ARM: clicked->setting = (clicked->setting % 8) + 1; break;
                    case COMP_PROCESSOR:
                        editProcIdx = clicked->procIdx;
                        if (editProcIdx >= 0) {
                            mode = MODE_PROC_EDIT;
                            editLine = 0;
                            editField = 0;
                        }
                        break;
                    default: break;
                }
            }
            break;
        }

        case MODE_PROC_EDIT: {
            if (editProcIdx < 0) break;
            Processor *p = MechGetProcessor(editProcIdx);
            if (!p) break;

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
                for (int i = p->progLen; i > editLine + 1; i--)
                    p->program[i] = p->program[i - 1];
                p->progLen++;
                if (editLine + 1 < MAX_PROG_LEN)
                    memset(&p->program[editLine + 1], 0, sizeof(Instruction));
            }
            if (IsKeyPressed(KEY_DELETE) && p->progLen > 1 && editLine < p->progLen) {
                for (int i = editLine; i < p->progLen - 1; i++)
                    p->program[i] = p->program[i + 1];
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

    MechInit();

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        pulseTime += dt;

        HandleInput();

        if (!simPaused) {
            tickTimer += dt;
            while (tickTimer >= TICK_INTERVAL) {
                tickTimer -= TICK_INTERVAL;
                MechTick();
            }
        }

        // Set cursor based on mode
        if (mode == MODE_INTERACT) {
            SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        } else {
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        }

        BeginDrawing();
        ClearBackground((Color){15, 15, 20, 255});

        DrawGridBackground();
        DrawComponents();

        {
            int gx, gy;
            GridFromScreen(GetMouseX(), GetMouseY(), &gx, &gy);
            if (MechInGrid(gx, gy) && (mode == MODE_PLACE || mode == MODE_INTERACT)) {
                if (mode == MODE_INTERACT) {
                    // Just highlight the hovered cell
                    Rectangle r = CellRect(gx, gy);
                    DrawRectangleLinesEx(r, 2, (Color){255, 200, 100, 100});
                } else if (selectedPreset >= 0) {
                    Preset *pr = &presets[selectedPreset];
                    for (int py = 0; py < pr->height; py++) {
                        for (int px = 0; px < pr->width; px++) {
                            if (MechInGrid(gx + px, gy + py)) {
                                Rectangle r = CellRect(gx + px, gy + py);
                                DrawRectangleLinesEx(r, 1, (Color){255, 200, 50, 60});
                            }
                        }
                    }
                } else {
                    Rectangle r = CellRect(gx, gy);
                    DrawRectangleLinesEx(r, 2, (Color){255, 255, 255, 80});

                    if (selectedComp != COMP_EMPTY && grid[gy][gx].type == COMP_EMPTY) {
                        int cx = (int)(r.x + r.width / 2);
                        int cy = (int)(r.y + r.height / 2);
                        Color ghostCol = CompColor(selectedComp, false);
                        ghostCol.a = 80;
                        Color ghostBg = ghostCol;
                        ghostBg.a = 40;
                        DrawRectangleRec(r, ghostBg);

                        bool directional = (selectedComp == COMP_BELT || selectedComp == COMP_LOADER ||
                            selectedComp == COMP_GRABBER || selectedComp == COMP_SPLITTER ||
                            selectedComp == COMP_FILTER || selectedComp == COMP_NOT ||
                            selectedComp == COMP_AND || selectedComp == COMP_OR ||
                            selectedComp == COMP_XOR || selectedComp == COMP_NOR ||
                            selectedComp == COMP_LATCH || selectedComp == COMP_REPEATER ||
                            selectedComp == COMP_PULSE || selectedComp == COMP_VALVE ||
                            selectedComp == COMP_COMPARATOR || selectedComp == COMP_COMPRESSOR ||
                            selectedComp == COMP_DECOMPRESSOR || selectedComp == COMP_UNLOADER ||
                            selectedComp == COMP_GEAR_RATIO || selectedComp == COMP_ESCAPEMENT ||
                            selectedComp == COMP_CAM_SHAFT || selectedComp == COMP_LEVER_ARM);
                        if (directional) DrawArrow(cx, cy, placingDir, (Color){255, 255, 255, 200});

                        const char *label = MechCompName(selectedComp);
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
        DrawPaletteTooltip();
        DrawStatusBar();
        DrawProcessorEditor();

        EndDrawing();
    }

    UnloadFont(comicFont);
    CloseWindow();
    return 0;
}
