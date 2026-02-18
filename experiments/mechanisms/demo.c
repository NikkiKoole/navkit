// Mechanisms & Signals Sandbox Demo
// A learning environment for signal-based automation:
//   Switch (source) -> Wire -> Logic Gates -> Wire -> Light (sink)
//   + Processor (tiny emulator with 6 opcodes)
//   + Fluid layer: Pipe/Pump/Valve/Tank/PressureLight
//   + Belt layer: Belt/Loader/Unloader/Grabber/Splitter/Filter/Compressor/Decompressor
//   + Mechanical layer: Crank/Shaft/Clutch/Flywheel/Escapement/CamShaft/Hammer/Lever/Governor
//
// ┌─────────────────────────────────────────────────────────────────────┐
// │                    CROSS-LAYER BRIDGE MAP                          │
// │                                                                     │
// │  SIGNAL ←──────────────────────────────────────→ FLUID             │
// │    │  Valve: wire signal opens/closes fluid flow                    │
// │    │  Pump: wire-gated (only runs when signal, if wire present)    │
// │    │  PressureLight: fluid pressure → analog signal 0-15           │
// │    │                                                                │
// │  SIGNAL ←──────────────────────────────────────→ BELT              │
// │    │  Loader: wire-gated (only spawns when signal, if wire present)│
// │    │  Grabber: wire signal enables/disables cargo transfer          │
// │    │  Unloader: holds last cargo type as persistent signal          │
// │    │                                                                │
// │  SIGNAL ←──────────────────────────────────────→ MECHANICAL        │
// │    │  Clutch: wire signal engages/disengages shaft network          │
// │    │  Spring: wire signal triggers stored energy release            │
// │    │  LeverArm: emits wire signal when shaft speed > 1             │
// │    │  Governor: emits analog 0-15 proportional to shaft speed      │
// │    │  Escapement: emits pulsed signal at speed-dependent rate      │
// │    │  CamShaft: emits patterned signal driven by shaft speed       │
// │    │                                                                │
// │  MECHANICAL ───────────────────────────────────→ FLUID             │
// │    │  Pump + adjacent shaft: shaft speed boosts pump rate           │
// │    │    rate = base * (1 + shaftSpeed/20)                          │
// │    │                                                                │
// │  Wire-gating pattern (Pump, Loader, Grabber):                      │
// │    No adjacent wire → always active (backward compatible)           │
// │    Adjacent wire with signal → active                               │
// │    Adjacent wire without signal → inactive                          │
// └─────────────────────────────────────────────────────────────────────┘
//
// Controls:
//   1-9,0  Select component / eraser
//   LMB    Place component (click same type to toggle settings)
//   RMB    Remove component / rotate on empty
//   R      Rotate (directional components)
//   P      Open processor editor on hovered processor
//   Space  Pause/resume simulation
//   T      Single tick step (when paused)
//   C      Clear grid
//   ESC    Switch to interact mode

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

    { "Blinker", "Clock -> Light",
      "A clock auto-toggles on a timer and drives a light, creating a steady blink. "
      "Click the clock to change its period (1-8 ticks). This is the simplest autonomous circuit — "
      "no user input needed. Use clocks to drive sequencers, test timing, or pace belt loaders.",
      MechBuildPresetBlinker, 3, 3 },

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

    { "Auto-Hammer", "Crank -> Shaft -> Hammer",
      "The simplest mechanical circuit. A crank provides torque, a shaft carries it, and a hammer "
      "strikes when the shaft speed exceeds its threshold. Click the crank to engage/disengage and "
      "watch the hammer start and stop. The mechanical equivalent of Switch -> Wire -> Light.",
      MechBuildPresetAutoHammer, 4, 1 },

    { "Clock Tower", "Crank -> Flywheel -> Escape -> Light",
      "A mechanical clock: the crank turns a shaft through a flywheel (which smooths speed changes) "
      "to an escapement. The escapement converts steady rotation into periodic signal pulses that "
      "blink a light. The flywheel keeps the tick rate stable even as torque fluctuates.",
      MechBuildPresetClockTower, 7, 1 },

    { "Gov. Loop", "Crank -> Governor -> feedback Clutch",
      "A self-regulating speed controller. The crank drives a shaft with a governor that outputs "
      "an analog signal proportional to speed. A comparator checks if speed is too high and cuts "
      "a clutch to disconnect the crank, letting friction slow things down. Speed stabilizes at "
      "the comparator's threshold — a mechanical feedback loop.",
      MechBuildPresetGovernorLoop, 7, 2 },

    { "Demand Load", "Button -> Pulse -> gated Loader -> Unload",
      "Press the button to dispense a burst of cargo. A pulse extender holds the signal for 5 "
      "ticks, gating the loader via an adjacent wire. The unloader holds the cargo type as a "
      "persistent signal on its display. Demonstrates the wire-gated loader bridge (Signal -> Belt).",
      MechBuildPresetDemandLoader, 9, 3 },

    { "Steam Hammer", "Crank -> Shaft boosts Pump + Hammer",
      "A shaft-driven fluid system. The crank spins a shaft that both powers a hammer and boosts "
      "an adjacent pump's flow rate. Faster cranking = more fluid pressure. A pressure light at "
      "the end converts pressure to signal. Three layers working together: Mechanical -> Fluid -> Signal.",
      MechBuildPresetSteamHammer, 9, 2 },

    { "Sorter", "Clock -> 2 Loaders -> Split -> Filter -> Unload",
      "A clock alternates two loaders (red and green cargo) via a NOT gate. Cargo merges onto "
      "a belt, hits a splitter, then two filters separate by type. Each unloader holds its last "
      "cargo type as a persistent signal shown on a display. Signal timing + belt logistics.",
      MechBuildPresetSortingFactory, 5, 7 },

    { "CW Bottler", "Crank -> Escapement gates Loader -> Belt",
      "A clockwork bottling line. The crank drives a flywheel-stabilized shaft to an escapement, "
      "which pulses a wire signal that gates a loader. Cargo appears in rhythm with the mechanical "
      "clock. Adjust crank torque to change the bottling rate. Three layers: Mechanical -> Signal -> Belt.",
      MechBuildPresetClockworkBottler, 11, 3 },
};
#define PRESET_COUNT (sizeof(presets) / sizeof(presets[0]))

static int selectedPreset = -1;

// ---------------------------------------------------------------------------
// Drawing helpers — data-driven from CompMeta
// ---------------------------------------------------------------------------
static Color MetaColor(const CompMeta *m, bool state) {
    if (state && (m->activeR || m->activeG || m->activeB))
        return (Color){m->activeR, m->activeG, m->activeB, 255};
    return (Color){m->colorR, m->colorG, m->colorB, 255};
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

// Shared draw: filled rect + centered label
static void DrawCompLabel(Rectangle r, int cx, int cy, Color col, const char *label) {
    DrawRectangleRec(r, col);
    if (label && label[0])
        DrawTextShadow(label, cx - (int)strlen(label) * 3, cy - 5, 10, WHITE);
}

// Shared draw: gate (rect + arrow + label + input/output dots)
static void DrawCompGate(Rectangle r, int cx, int cy, Color col, Cell *c, const char *label) {
    DrawRectangleRec(r, col);
    DrawArrow(cx, cy, c->facing, WHITE);
    if (label && label[0])
        DrawTextShadow(label, cx - (int)strlen(label) * 3, cy - 5, 10, WHITE);

    int edge = CELL_SIZE / 2 - 1;
    int dx, dy;
    MechDirOffset(c->facing, &dx, &dy);
    DrawCircle(cx + dx * edge, cy + dy * edge, 3, GREEN);

    // Determine input style based on component type
    if (c->type == COMP_NOT || c->type == COMP_REPEATER || c->type == COMP_PULSE ||
        c->type == COMP_COMPARATOR || c->type == COMP_ESCAPEMENT || c->type == COMP_LEVER_ARM) {
        MechDirOffset(MechOppositeDir(c->facing), &dx, &dy);
        DrawCircle(cx + dx * edge, cy + dy * edge, 3, ORANGE);
    } else {
        // Two side inputs (AND, OR, XOR, NOR, Latch)
        Direction inA, inB;
        MechGateInputDirs(c->facing, &inA, &inB);
        MechDirOffset(inA, &dx, &dy);
        DrawCircle(cx + dx * edge, cy + dy * edge, 3, ORANGE);
        MechDirOffset(inB, &dx, &dy);
        DrawCircle(cx + dx * edge, cy + dy * edge, 3,
            (c->type == COMP_LATCH) ? (Color){200, 0, 0, 255} : ORANGE);
    }

    // Show setting value for configurable gates
    if (c->type == COMP_REPEATER || c->type == COMP_PULSE || c->type == COMP_COMPARATOR) {
        char buf[4];
        snprintf(buf, sizeof(buf), "%d", c->setting);
        DrawTextShadow(buf, cx - 3, cy - 5, 10, WHITE);
    }
}

// Shared draw: connected lines to neighbors (wire, shaft, pipe)
static void DrawCompConnected(Rectangle r __attribute__((unused)), int cx, int cy, Color baseCol, Cell *c, int x, int y) {
    bool isMech = (c->type == COMP_SHAFT);
    bool isPipe = (c->type == COMP_PIPE);

    Color drawCol = baseCol;
    if (c->type == COMP_WIRE) {
        int sigVal = MechGetSignal(x, y);
        if (sigVal > 0) {
            float intensity = (float)sigVal / 15.0f;
            float pulse = 0.6f + 0.4f * sinf(pulseTime * 6.0f);
            unsigned char g = (unsigned char)(80 + 175 * intensity * pulse);
            unsigned char rr = (unsigned char)(40 * intensity * pulse);
            drawCol = (Color){rr, g, 0, 255};
        }
    } else if (isPipe) {
        float frac = (float)c->fluidLevel / 255.0f;
        unsigned char b = (unsigned char)(80 + 175 * frac);
        unsigned char g = (unsigned char)(40 + 180 * frac);
        drawCol = (Color){0, g, b, 255};
    } else if (isMech && c->mechSpeed > 0) {
        float pulse = 0.7f + 0.3f * sinf(pulseTime * c->mechSpeed * 0.5f);
        drawCol.r = (unsigned char)(drawCol.r * pulse);
        drawCol.g = (unsigned char)(drawCol.g * pulse);
    }

    bool any = false;
    int edge = CELL_SIZE / 2;
    for (int d = 0; d < 4; d++) {
        int dx, dy;
        MechDirOffset((Direction)d, &dx, &dy);
        int nx = x + dx, ny = y + dy;
        if (!MechInGrid(nx, ny)) continue;
        ComponentType nt = grid[ny][nx].type;
        bool connected = false;
        if (c->type == COMP_WIRE) connected = (nt != COMP_EMPTY);
        else if (isPipe) connected = (nt >= COMP_PIPE && nt <= COMP_PRESSURE_LIGHT);
        else if (isMech) connected = IsMechCell(nt);
        if (connected) {
            any = true;
            DrawLineEx((Vector2){(float)cx, (float)cy},
                       (Vector2){(float)(cx + dx * edge), (float)(cy + dy * edge)}, 3.0f, drawCol);
        }
    }
    if (!any) DrawCircle(cx, cy, 3, drawCol);
}

// Shared draw: filled circle (lights)
static void DrawCompCircle(int cx, int cy, Color col, bool state) {
    DrawCircle(cx, cy, CELL_SIZE / 2 - 1, col);
    if (state) {
        Color glow = col;
        glow.a = 40;
        DrawCircle(cx, cy, CELL_SIZE / 2 + 3, glow);
    }
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

            const CompMeta *m = MechGetCompMeta(c->type);
            Rectangle r = CellRect(x, y);
            Color col = MetaColor(m, c->state);
            int cx = (int)(r.x + r.width / 2);
            int cy = (int)(r.y + r.height / 2);

            switch (m->drawStyle) {
                case DRAW_LABEL:
                    DrawCompLabel(r, cx, cy, col, m->label);
                    if (c->type == COMP_PROCESSOR) {
                        for (int d = 0; d < 4; d++) {
                            int dx, dy;
                            MechDirOffset((Direction)d, &dx, &dy);
                            DrawCircle(cx + dx * (CELL_SIZE / 2 - 2),
                                       cy + dy * (CELL_SIZE / 2 - 2), 2, YELLOW);
                        }
                    }
                    if (c->type == COMP_UNLOADER && c->cargo > 0)
                        DrawCircle(cx, cy + 4, 3, CargoColor(c->cargo));
                    break;

                case DRAW_CIRCLE:
                    DrawCompCircle(cx, cy, col, c->state);
                    break;

                case DRAW_GATE:
                    DrawCompGate(r, cx, cy, col, c, m->label);
                    break;

                case DRAW_CONNECTED:
                    DrawCompConnected(r, cx, cy, col, c, x, y);
                    break;

                case DRAW_CUSTOM: {
                    // Component-specific rendering
                    switch (c->type) {
                        case COMP_BUTTON: {
                            Rectangle br = { r.x + 2, r.y + 2, r.width - 4, r.height - 4 };
                            DrawRectangleRounded(br, 0.4f, 4, col);
                            DrawTextShadow("B", cx - 4, cy - 5, 10, WHITE);
                            break;
                        }

                        case COMP_CLOCK: {
                            DrawRectangleRec(r, col);
                            char buf[4];
                            snprintf(buf, sizeof(buf), "%d", c->setting);
                            DrawTextShadow(buf, cx - 3, cy - 5, 10, WHITE);
                            break;
                        }

                        case COMP_DIAL: {
                            DrawRectangleRec(r, col);
                            char buf[4];
                            snprintf(buf, sizeof(buf), "%d", c->setting);
                            int tw = MeasureTextUI(buf, 10);
                            DrawTextShadow(buf, cx - tw / 2, cy - 5, 10, BLACK);
                            break;
                        }

                        case COMP_DISPLAY: {
                            DrawRectangleRec(r, col);
                            DrawRectangleLinesEx(r, 1, (Color){60, 60, 70, 255});
                            int val = c->setting;
                            char buf[4];
                            snprintf(buf, sizeof(buf), val > 9 ? "%X" : "%d", val);
                            Color numCol = val > 0
                                ? (Color){(unsigned char)(80 + 175.0f * val / 15.0f),
                                          (unsigned char)(200.0f * val / 15.0f), 40, 255}
                                : (Color){50, 50, 50, 255};
                            int tw = MeasureTextUI(buf, 14);
                            DrawTextShadow(buf, cx - tw / 2, cy - 7, 14, numCol);
                            break;
                        }

                        case COMP_PUMP: {
                            DrawRectangleRec(r, col);
                            char buf[8];
                            if (c->setting >= 0)
                                snprintf(buf, sizeof(buf), "P%d", c->setting);
                            else
                                snprintf(buf, sizeof(buf), "D%d", -c->setting);
                            DrawTextShadow(buf, cx - 6, cy - 5, 10, WHITE);
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
                            float frac = (float)c->fluidLevel / 1024.0f;
                            unsigned char b = (unsigned char)(40 + 180 * frac);
                            unsigned char g = (unsigned char)(20 + 120 * frac);
                            Rectangle tr = {r.x - 1, r.y - 1, r.width + 2, r.height + 2};
                            DrawRectangleRec(tr, (Color){0, g, b, 255});
                            DrawRectangleLinesEx(r, 1, (Color){60, 60, 100, 255});
                            DrawTextShadow("T", cx - 3, cy - 5, 10, WHITE);
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
                                DrawCircle(cx + pdx * 3, cy + pdy * 3, 3, CargoColor(c->cargo2));
                            } else if (c->cargo > 0) {
                                DrawCircle(cx, cy, 4, CargoColor(c->cargo));
                            }
                            break;
                        }

                        case COMP_LOADER: {
                            DrawRectangleRec(r, col);
                            DrawArrow(cx, cy, c->facing, WHITE);
                            DrawCircle(cx, cy - 2, 3, CargoColor(c->setting));
                            char buf[4];
                            snprintf(buf, sizeof(buf), "%d", c->setting);
                            DrawTextShadow(buf, cx + 3, cy - 5, 10, WHITE);
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
                            int dx, dy, edge = CELL_SIZE / 2 - 2;
                            MechDirOffset(MechOppositeDir(c->facing), &dx, &dy);
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
                            if (c->cargo > 0) DrawCircle(cx, cy, 3, CargoColor(c->cargo));
                            break;
                        }

                        case COMP_FILTER: {
                            DrawRectangleRec(r, col);
                            DrawArrow(cx, cy, c->facing, WHITE);
                            DrawCircle(cx, cy - 2, 3, CargoColor(c->setting));
                            char buf[4];
                            snprintf(buf, sizeof(buf), "F%d", c->setting);
                            DrawTextShadow(buf, cx - 5, cy - 5, 10, WHITE);
                            break;
                        }

                        case COMP_COMPRESSOR: {
                            DrawRectangleRec(r, col);
                            int dx, dy, edge = CELL_SIZE / 2 - 2;
                            Direction rightDir, leftDir;
                            MechGateInputDirs(c->facing, &rightDir, &leftDir);
                            MechDirOffset(leftDir, &dx, &dy);
                            DrawLineEx((Vector2){(float)(cx + dx * edge), (float)(cy + dy * edge)},
                                       (Vector2){(float)cx, (float)cy}, 2.0f, WHITE);
                            DrawCircle(cx + dx * edge, cy + dy * edge, 3, ORANGE);
                            MechDirOffset(rightDir, &dx, &dy);
                            DrawLineEx((Vector2){(float)(cx + dx * edge), (float)(cy + dy * edge)},
                                       (Vector2){(float)cx, (float)cy}, 2.0f, WHITE);
                            DrawCircle(cx + dx * edge, cy + dy * edge, 3, ORANGE);
                            MechDirOffset(c->facing, &dx, &dy);
                            DrawLineEx((Vector2){(float)cx, (float)cy},
                                       (Vector2){(float)(cx + dx * edge), (float)(cy + dy * edge)}, 2.0f, WHITE);
                            DrawCircle(cx + dx * edge, cy + dy * edge, 3, GREEN);
                            DrawTextShadow("><", cx - 5, cy - 5, 10, WHITE);
                            break;
                        }

                        case COMP_DECOMPRESSOR: {
                            DrawRectangleRec(r, col);
                            int dx, dy, edge = CELL_SIZE / 2 - 2;
                            MechDirOffset(MechOppositeDir(c->facing), &dx, &dy);
                            DrawLineEx((Vector2){(float)(cx + dx * edge), (float)(cy + dy * edge)},
                                       (Vector2){(float)cx, (float)cy}, 2.0f, WHITE);
                            DrawCircle(cx + dx * edge, cy + dy * edge, 3, ORANGE);
                            MechDirOffset(c->facing, &dx, &dy);
                            DrawLineEx((Vector2){(float)cx, (float)cy},
                                       (Vector2){(float)(cx + dx * edge), (float)(cy + dy * edge)}, 2.0f, WHITE);
                            DrawCircle(cx + dx * edge, cy + dy * edge, 3, GREEN);
                            Direction rightDir, leftDir;
                            MechGateInputDirs(c->facing, &rightDir, &leftDir);
                            MechDirOffset(rightDir, &dx, &dy);
                            DrawLineEx((Vector2){(float)cx, (float)cy},
                                       (Vector2){(float)(cx + dx * edge), (float)(cy + dy * edge)}, 2.0f, WHITE);
                            DrawCircle(cx + dx * edge, cy + dy * edge, 3, GREEN);
                            DrawTextShadow("<>", cx - 5, cy - 5, 10, WHITE);
                            break;
                        }

                        // Mechanical custom components
                        case COMP_CRANK: {
                            DrawRectangleRec(r, col);
                            DrawTextShadow(c->state ? "C+" : "C-", cx - 6, cy - 5, 10, WHITE);
                            if (c->mechSpeed > 0) {
                                float barW = (c->mechSpeed / 100.0f) * (CELL_SIZE - 4);
                                DrawRectangle((int)r.x + 2, (int)(r.y + r.height - 4), (int)barW, 3, (Color){255, 200, 50, 180});
                            }
                            break;
                        }

                        case COMP_SPRING: {
                            DrawRectangleRec(r, col);
                            float charge = c->springCharge / (float)(c->setting > 0 ? c->setting : 1);
                            int sw = CELL_SIZE - 6;
                            for (int i = 0; i < 3; i++) {
                                int sx = (int)r.x + 3 + (i * sw / 3);
                                int w = sw / 3 - 1;
                                int h = (int)(3 + 4 * charge);
                                DrawRectangle(sx, cy - h/2, w, h, (Color){255, 200, 100, 200});
                            }
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
                            char buf[4];
                            snprintf(buf, sizeof(buf), "%d", c->setting);
                            DrawTextShadow(buf, cx - 3, cy - 5, 10, WHITE);
                            break;
                        }

                        case COMP_CAM_SHAFT: {
                            DrawRectangleRec(r, col);
                            DrawArrow(cx, cy, c->facing, WHITE);
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
                            if (c->state && c->timer < 2)
                                DrawRectangle(cx - 3, cy - 6, 6, 4, WHITE);
                            else
                                DrawRectangle(cx - 3, cy + 2, 6, 4, (Color){160, 160, 170, 255});
                            DrawLine(cx, cy - 2, cx, cy + 6, (Color){120, 100, 60, 255});
                            break;
                        }

                        case COMP_GOVERNOR: {
                            DrawRectangleRec(r, col);
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
                            DrawRectangleRec(r, col);
                            break;
                    }
                    break;
                }

                default:
                    break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// UI: Palette — data-driven from CompMeta
// ---------------------------------------------------------------------------
#define PALETTE_ROW_H  24
#define PALETTE_PAD    4

// Layer labels and colors
static const char *layerNames[LAYER_COUNT] = { "SIGNAL", "FLUID", "BELT", "CPU", "MECH" };
static const Color layerColors[LAYER_COUNT] = {
    {100, 100, 110, 255}, {60, 100, 140, 255}, {140, 120, 60, 255},
    {120, 80, 160, 255}, {180, 140, 80, 255}
};
static const Color layerSelColors[LAYER_COUNT] = {
    {70, 70, 80, 255}, {40, 60, 90, 255}, {80, 70, 40, 255},
    {70, 40, 90, 255}, {100, 75, 30, 255}
};

// Count components per layer and cache
static int layerCompCount[LAYER_COUNT];
static ComponentType layerComps[LAYER_COUNT][20];
static bool paletteInited = false;

static void InitPalette(void) {
    if (paletteInited) return;
    memset(layerCompCount, 0, sizeof(layerCompCount));
    for (int t = 0; t < COMP_COUNT; t++) {
        const CompMeta *m = MechGetCompMeta((ComponentType)t);
        if (m->keyCode == 0 && t != COMP_EMPTY) continue;
        if (t == COMP_EMPTY) continue; // eraser handled separately
        int l = m->layer;
        if (l >= 0 && l < LAYER_COUNT && layerCompCount[l] < 20) {
            layerComps[l][layerCompCount[l]++] = (ComponentType)t;
        }
    }
    paletteInited = true;
}

#define PALETTE_BAR_H  (PALETTE_ROW_H * 5 + PALETTE_PAD * 6)

static void DrawPaletteRow(int layer, int rowY) {
    int count = layerCompCount[layer];
    for (int i = 0; i < count; i++) {
        ComponentType t = layerComps[layer][i];
        const CompMeta *m = MechGetCompMeta(t);
        int bx = 6 + i * (SCREEN_WIDTH - 12) / count;
        int itemW = (SCREEN_WIDTH - 12) / count - 4;
        bool sel = (selectedComp == t);
        Color bg = sel ? layerSelColors[layer] : (Color){40, 40, 45, 255};
        DrawRectangle(bx, rowY, itemW, PALETTE_ROW_H, bg);
        if (sel) DrawRectangleLinesEx((Rectangle){(float)bx, (float)rowY, (float)itemW, (float)PALETTE_ROW_H}, 2, WHITE);

        char label[32];
        snprintf(label, sizeof(label), "%s:%s", m->keyLabel, m->name);
        DrawTextShadow(label, bx + 4, rowY + 7, 10, WHITE);
    }
    DrawTextShadow(layerNames[layer], SCREEN_WIDTH - 55, rowY + 7, 10, layerColors[layer]);
}

static bool HandlePaletteRowClick(int layer, int rowY, int mx, int my) {
    if (my < rowY || my > rowY + PALETTE_ROW_H) return false;
    int count = layerCompCount[layer];
    for (int i = 0; i < count; i++) {
        int bx = 6 + i * (SCREEN_WIDTH - 12) / count;
        int itemW = (SCREEN_WIDTH - 12) / count - 4;
        if (mx >= bx && mx <= bx + itemW) {
            selectedComp = layerComps[layer][i];
            return true;
        }
    }
    return false;
}

static int PaletteRowHitTest(int layer, int rowY, int mx, int my) {
    if (my < rowY || my > rowY + PALETTE_ROW_H) return -1;
    int count = layerCompCount[layer];
    for (int i = 0; i < count; i++) {
        int bx = 6 + i * (SCREEN_WIDTH - 12) / count;
        int itemW = (SCREEN_WIDTH - 12) / count - 4;
        if (mx >= bx && mx <= bx + itemW) return layerComps[layer][i];
    }
    return -1;
}

static void DrawPalette(void) {
    InitPalette();
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
        int rowY = barY + PALETTE_PAD;
        for (int l = 0; l < LAYER_COUNT; l++) {
            if (layerCompCount[l] > 0) {
                DrawPaletteRow(l, rowY);
                rowY += PALETTE_ROW_H + PALETTE_PAD;
            }
        }
    }
}

// Word-wrap text into lines that fit within maxWidth pixels at the given fontSize.
static int WrapText(const char *text, int maxWidth, int fontSize, char lines[][256], int maxLines) {
    int lineCount = 0;
    const char *p = text;
    while (*p && lineCount < maxLines) {
        char *out = lines[lineCount];
        int outLen = 0;
        out[0] = '\0';

        while (*p) {
            const char *wordStart = p;
            while (*p && *p != ' ') p++;
            int wordLen = (int)(p - wordStart);
            while (*p == ' ') p++;

            char test[256];
            if (outLen > 0)
                snprintf(test, sizeof(test), "%.*s %.*s", outLen, out, wordLen, wordStart);
            else
                snprintf(test, sizeof(test), "%.*s", wordLen, wordStart);

            int testW = MeasureTextUI(test, fontSize);
            if (testW > maxWidth && outLen > 0) {
                p = wordStart;
                while (*p == ' ') p++;
                break;
            }

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
        InitPalette();
        int rowY = barY + PALETTE_PAD;
        int hit = -1;
        for (int l = 0; l < LAYER_COUNT; l++) {
            if (layerCompCount[l] > 0) {
                hit = PaletteRowHitTest(l, rowY, mx, my);
                if (hit >= 0) break;
                rowY += PALETTE_ROW_H + PALETTE_PAD;
            }
        }
        if (hit < 0) return;
        const CompMeta *m = MechGetCompMeta((ComponentType)hit);
        title = m->name;
        body = m->tooltip;
    }

    if (!body || body[0] == '\0') return;

    int maxTooltipW = 500;
    char wrappedLines[8][256];
    int lineCount = WrapText(body, maxTooltipW - 12, fontSize, wrappedLines, 8);

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
    if (tx < 2) tx = 2;
    if (tx + boxW > SCREEN_WIDTH - 2) tx = SCREEN_WIDTH - 2 - boxW;
    if (ty < 2) ty = 2;

    DrawRectangle(tx, ty, boxW, boxH, (Color){15, 15, 20, 245});
    DrawRectangleLinesEx((Rectangle){(float)tx, (float)ty, (float)boxW, (float)boxH}, 1, (Color){80, 80, 90, 200});
    DrawTextShadow(title, tx + pad, ty + pad, fontSize + 2, (Color){255, 220, 130, 255});
    for (int i = 0; i < lineCount; i++)
        DrawTextShadow(wrappedLines[i], tx + pad, ty + pad + titleH + i * lineH, fontSize, (Color){200, 200, 210, 255});
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

    InitPalette();
    int rowY = barY + PALETTE_PAD;
    for (int l = 0; l < LAYER_COUNT; l++) {
        if (layerCompCount[l] > 0) {
            if (HandlePaletteRowClick(l, rowY, mx, my)) return true;
            rowY += PALETTE_ROW_H + PALETTE_PAD;
        }
    }
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

        if (isCurrentPC)
            DrawRectangle(panelX + 5, ly - 2, panelW - 10, lineH, (Color){60, 40, 80, 200});
        if (isEditing)
            DrawRectangleLinesEx((Rectangle){(float)(panelX + 5), (float)(ly - 2),
                                 (float)(panelW - 10), (float)lineH}, 1, YELLOW);

        Instruction *inst = &p->program[i];
        bool active = (i < p->progLen);
        Color textCol = active ? WHITE : (Color){60, 60, 60, 255};

        snprintf(buf, sizeof(buf), "%2d", i);
        DrawTextShadow(buf, panelX + 12, ly, 10, textCol);

        DrawTextShadow(MechOpName(inst->op), panelX + 50, ly, 10,
                       (isEditing && editField == 0) ? YELLOW : textCol);

        snprintf(buf, sizeof(buf), "%d", inst->argA);
        DrawTextShadow(buf, panelX + 120, ly, 10, (isEditing && editField == 1) ? YELLOW : textCol);

        snprintf(buf, sizeof(buf), "%d", inst->argB);
        DrawTextShadow(buf, panelX + 170, ly, 10, (isEditing && editField == 2) ? YELLOW : textCol);

        snprintf(buf, sizeof(buf), "%d", inst->argC);
        DrawTextShadow(buf, panelX + 220, ly, 10, (isEditing && editField == 3) ? YELLOW : textCol);

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
// UI: Cell Tooltip
// ---------------------------------------------------------------------------
static void DrawCellTooltip(int gx, int gy) {
    if (!MechInGrid(gx, gy)) return;
    Cell *c = &grid[gy][gx];
    if (c->type == COMP_EMPTY) return;

    const CompMeta *m = MechGetCompMeta(c->type);
    char buf[256];

    // Mechanical components
    if (m->layer == LAYER_MECHANICAL) {
        if (c->type == COMP_CRANK)
            snprintf(buf, sizeof(buf), "%s [%d,%d] %s torque=%d speed=%.1f",
                     m->name, gx, gy, c->state ? "ENGAGED" : "OFF", c->setting, c->mechSpeed);
        else if (c->type == COMP_SPRING)
            snprintf(buf, sizeof(buf), "%s [%d,%d] cap=%d charge=%.1f speed=%.1f",
                     m->name, gx, gy, c->setting, c->springCharge, c->mechSpeed);
        else if (c->type == COMP_CAM_SHAFT)
            snprintf(buf, sizeof(buf), "%s [%d,%d] pattern=0x%02X pos=%d %s speed=%.1f dir=%s",
                     m->name, gx, gy, c->setting, c->camPosition,
                     c->state ? "ON" : "OFF", c->mechSpeed, MechDirName(c->facing));
        else if (c->type == COMP_GOVERNOR)
            snprintf(buf, sizeof(buf), "%s [%d,%d] speed=%.1f output=%d/15",
                     m->name, gx, gy, c->mechSpeed, (int)(c->mechSpeed * 15.0f / 100.0f));
        else
            snprintf(buf, sizeof(buf), "%s [%d,%d] %s speed=%.1f%s%s",
                     m->name, gx, gy, c->state ? "ON" : "off", c->mechSpeed,
                     m->clickConfig ? " setting=" : "",
                     m->clickConfig ? (snprintf(buf+200, 16, "%d", c->setting), buf+200) : "");
    }
    // Fluid components
    else if (m->layer == LAYER_FLUID) {
        if (c->type == COMP_PUMP)
            snprintf(buf, sizeof(buf), "%s [%d,%d] rate=%d %s pressure=%d (click to change, neg=drain)",
                     m->name, gx, gy, c->setting, c->state ? "active" : "idle", c->fluidLevel);
        else if (c->type == COMP_VALVE)
            snprintf(buf, sizeof(buf), "%s [%d,%d] %s pressure=%d dir=%s",
                     m->name, gx, gy, c->state ? "OPEN" : "CLOSED", c->fluidLevel, MechDirName(c->facing));
        else
            snprintf(buf, sizeof(buf), "%s [%d,%d] pressure=%d/%d",
                     m->name, gx, gy, c->fluidLevel, (c->type == COMP_TANK) ? 1024 : 255);
    }
    // Belt components
    else if (m->layer == LAYER_BELT) {
        if (c->type == COMP_LOADER)
            snprintf(buf, sizeof(buf), "%s [%d,%d] type=%d dir=%s (click to change type)",
                     m->name, gx, gy, c->setting, MechDirName(c->facing));
        else if (c->type == COMP_FILTER)
            snprintf(buf, sizeof(buf), "%s [%d,%d] pass=%d dir=%s cargo=%d",
                     m->name, gx, gy, c->setting, MechDirName(c->facing), c->cargo);
        else
            snprintf(buf, sizeof(buf), "%s [%d,%d] dir=%s cargo=%d",
                     m->name, gx, gy, MechDirName(c->facing), c->cargo);
    }
    // Signal/CPU
    else {
        snprintf(buf, sizeof(buf), "%s [%d,%d] %s%s%s dir=%s",
                 m->name, gx, gy, c->state ? "ON" : "OFF",
                 m->clickConfig ? " setting=" : "",
                 m->clickConfig ? (snprintf(buf+200, 16, "%d", c->setting), buf+200) : "",
                 MechDirName(c->facing));
    }

    int tmx = GetMouseX() + 15;
    int tmy = GetMouseY() - 20;
    int tw = MeasureTextUI(buf, 10) + 10;
    DrawRectangle(tmx - 2, tmy - 2, tw, 18, (Color){20, 20, 25, 230});
    DrawTextShadow(buf, tmx + 3, tmy + 2, 10, WHITE);
}

// ---------------------------------------------------------------------------
// Click-to-interact: handles both MODE_PLACE (when clicking same type) and MODE_INTERACT
// ---------------------------------------------------------------------------
static void ClickInteract(Cell *clicked) {
    switch (clicked->type) {
        case COMP_SWITCH: clicked->state = !clicked->state; break;
        case COMP_CRANK:  clicked->state = !clicked->state; break;
        case COMP_CLOCK:
            clicked->setting = (clicked->setting % 8) + 1;
            clicked->timer = clicked->setting;
            break;
        case COMP_REPEATER:
            clicked->setting = (clicked->setting % 4) + 1;
            memset(clicked->delayBuf, 0, sizeof(clicked->delayBuf));
            break;
        case COMP_PULSE:       clicked->setting = (clicked->setting % 8) + 1; break;
        case COMP_PUMP: {
            // Cycle: 1..8, then -1..-8, then back to 1
            int s = clicked->setting;
            if (s > 0 && s < 8) s++;
            else if (s == 8) s = -1;
            else if (s < 0 && s > -8) s--;
            else s = 1;
            clicked->setting = s;
            break;
        }
        case COMP_DIAL:        clicked->setting = (clicked->setting + 1) % 16; break;
        case COMP_COMPARATOR:  clicked->setting = (clicked->setting % 15) + 1; break;
        case COMP_LOADER:      clicked->setting = (clicked->setting % 15) + 1; break;
        case COMP_FILTER:      clicked->setting = (clicked->setting % 15) + 1; break;
        case COMP_SPRING:      clicked->setting = (clicked->setting % 15) + 1; break;
        case COMP_FLYWHEEL:    clicked->setting = (clicked->setting % 8) + 1; break;
        case COMP_CAM_SHAFT: {
            static const int patterns[] = {0xAA, 0x55, 0xFF, 0x0F, 0x33, 0xC3, 0x01, 0x81};
            int cur = 0;
            for (int i = 0; i < 8; i++) if (clicked->setting == patterns[i]) { cur = i; break; }
            clicked->setting = patterns[(cur + 1) % 8];
            break;
        }
        case COMP_HAMMER:      clicked->setting = (clicked->setting % 8) + 1; break;
        case COMP_LEVER_ARM:   clicked->setting = (clicked->setting % 8) + 1; break;
        case COMP_PROCESSOR:
            if (clicked->procIdx >= 0) {
                editProcIdx = clicked->procIdx;
                mode = MODE_PROC_EDIT;
                editLine = 0;
                editField = 0;
            }
            break;
        default: break;
    }
}

// ---------------------------------------------------------------------------
// Input handling — data-driven key bindings
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

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && MechInGrid(gx, gy))
            presets[selectedPreset].build(gx, gy);

        if (IsKeyPressed(KEY_ESCAPE)) selectedPreset = -1;
        return;
    }

    // Data-driven key bindings from CompMeta
    if (mode == MODE_PLACE || mode == MODE_INTERACT) {
        ComponentType prevComp = selectedComp;
        for (int t = 0; t < COMP_COUNT; t++) {
            const CompMeta *m = MechGetCompMeta((ComponentType)t);
            if (m->keyCode > 0 && IsKeyPressed(m->keyCode)) {
                selectedComp = (ComponentType)t;
                break;
            }
        }
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
                } else if (clicked->type == selectedComp && MechGetCompMeta(selectedComp)->clickConfig) {
                    ClickInteract(clicked);
                } else {
                    MechPlaceComponent(gx, gy, selectedComp, placingDir);
                }
            }

            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && MechInGrid(gx, gy)) {
                Cell *clicked = &grid[gy][gx];
                if (clicked->type != COMP_BUTTON) {
                    bool isClickConfig = (clicked->type == selectedComp && MechGetCompMeta(selectedComp)->clickConfig);
                    if (!isClickConfig)
                        MechPlaceComponent(gx, gy, selectedComp, placingDir);
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
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && MechInGrid(gx, gy)) {
                if (grid[gy][gx].type == COMP_BUTTON) grid[gy][gx].state = true;
            }
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && MechInGrid(gx, gy)) {
                ClickInteract(&grid[gy][gx]);
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

        if (mode == MODE_INTERACT) SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        else SetMouseCursor(MOUSE_CURSOR_DEFAULT);

        BeginDrawing();
        ClearBackground((Color){15, 15, 20, 255});

        DrawGridBackground();
        DrawComponents();

        {
            int hgx, hgy;
            GridFromScreen(GetMouseX(), GetMouseY(), &hgx, &hgy);
            if (MechInGrid(hgx, hgy) && (mode == MODE_PLACE || mode == MODE_INTERACT)) {
                if (mode == MODE_INTERACT) {
                    Rectangle r = CellRect(hgx, hgy);
                    DrawRectangleLinesEx(r, 2, (Color){255, 200, 100, 100});
                } else if (selectedPreset >= 0) {
                    Preset *pr = &presets[selectedPreset];
                    for (int py = 0; py < pr->height; py++)
                        for (int px = 0; px < pr->width; px++)
                            if (MechInGrid(hgx + px, hgy + py)) {
                                Rectangle r = CellRect(hgx + px, hgy + py);
                                DrawRectangleLinesEx(r, 1, (Color){255, 200, 50, 60});
                            }
                } else {
                    Rectangle r = CellRect(hgx, hgy);
                    DrawRectangleLinesEx(r, 2, (Color){255, 255, 255, 80});

                    if (selectedComp != COMP_EMPTY && grid[hgy][hgx].type == COMP_EMPTY) {
                        const CompMeta *m = MechGetCompMeta(selectedComp);
                        int hcx = (int)(r.x + r.width / 2);
                        int hcy = (int)(r.y + r.height / 2);
                        Color ghostBg = MetaColor(m, false);
                        ghostBg.a = 40;
                        DrawRectangleRec(r, ghostBg);

                        if (m->directional) DrawArrow(hcx, hcy, placingDir, (Color){255, 255, 255, 200});

                        const char *label = m->name;
                        int tw = MeasureTextUI(label, 10);
                        DrawTextShadow(label, hcx - tw / 2, hcy - 5, 10, (Color){255, 255, 255, 100});
                    }
                }
            }
            DrawCellTooltip(hgx, hgy);
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
