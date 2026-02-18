// test_mechanisms.c - Behavior-driven tests for signal-based automation sandbox
// Standalone test file (no game dependencies)
//
// Signal timing model:
//   - Switches/dials seed adjacent wires into newSig (current tick)
//   - Gates read from sigRead (PREVIOUS tick's signals)
//   - Lights read from newSig (current tick)
//   - So: Switch->Wire->Light = 1 tick
//         Switch->Wire->Gate->Wire->Light = 2 ticks

#include "../vendor/c89spec.h"
#include <string.h>
#include <stdbool.h>

// Include the mechanisms implementation directly (unity-style)
#include "../experiments/mechanisms/mechanisms.c"

// Helper: run N ticks
static void RunTicks(int n) {
    for (int i = 0; i < n; i++) MechTick();
}

// ---------------------------------------------------------------------------
// Signal layer
// ---------------------------------------------------------------------------

describe(signal_basics) {
    it("turning on a switch should light up a connected light") {
        // Switch -> Wire -> Wire -> Light (no gate, 1 tick)
        MechInit();
        MechPlaceComponent(2, 5, COMP_SWITCH, DIR_NORTH);
        MechPlaceWire(3, 5);
        MechPlaceWire(4, 5);
        MechPlaceComponent(5, 5, COMP_LIGHT, DIR_EAST);

        // Switch off — light should be off
        MechTick();
        Cell *light = MechGetCell(5, 5);
        expect(light->state == false);

        // Turn switch on — light should come on (1 tick, no gate)
        MechSetSwitch(2, 5, true);
        MechTick();
        expect(light->state == true);

        // Turn switch off — light should go off
        MechSetSwitch(2, 5, false);
        MechTick();
        expect(light->state == false);
    }

    it("a NOT gate should invert its input") {
        // Switch -> Wire -> NOT(facing east) -> Wire -> Light
        // Gate circuit: 2 ticks to propagate
        MechInit();
        MechPlaceComponent(1, 5, COMP_SWITCH, DIR_NORTH);
        MechPlaceWire(2, 5);
        MechPlaceComponent(3, 5, COMP_NOT, DIR_EAST);
        MechPlaceWire(4, 5);
        MechPlaceComponent(5, 5, COMP_LIGHT, DIR_EAST);

        Cell *light = MechGetCell(5, 5);

        // Stabilize: NOT with no input outputs 1
        RunTicks(2);
        expect(light->state == true);

        // Switch ON — after 2 ticks NOT sees the input and inverts to 0
        MechSetSwitch(1, 5, true);
        RunTicks(2);
        expect(light->state == false);
    }

    it("an AND gate needs both inputs on to output") {
        // Two switches feeding AND gate (facing east)
        // Gate inputs: north (inA) and south (inB) per GateInputDirs
        MechInit();
        MechPlaceComponent(0, 0, COMP_SWITCH, DIR_NORTH);
        MechPlaceWire(1, 0);
        MechPlaceWire(2, 0);
        MechPlaceWire(3, 0);
        MechPlaceComponent(3, 1, COMP_AND, DIR_EAST);
        MechPlaceWire(4, 1);
        MechPlaceComponent(5, 1, COMP_LIGHT, DIR_EAST);
        MechPlaceComponent(0, 2, COMP_SWITCH, DIR_NORTH);
        MechPlaceWire(1, 2);
        MechPlaceWire(2, 2);
        MechPlaceWire(3, 2);

        Cell *light = MechGetCell(5, 1);

        // Both off — light off
        RunTicks(2);
        expect(light->state == false);

        // Only A on — still off
        MechSetSwitch(0, 0, true);
        RunTicks(2);
        expect(light->state == false);

        // Both on — on
        MechSetSwitch(0, 2, true);
        RunTicks(2);
        expect(light->state == true);

        // Only B on — off
        MechSetSwitch(0, 0, false);
        RunTicks(2);
        expect(light->state == false);
    }

    it("an OR gate lights with either input") {
        MechInit();
        MechPlaceComponent(0, 0, COMP_SWITCH, DIR_NORTH);
        MechPlaceWire(1, 0);
        MechPlaceWire(2, 0);
        MechPlaceWire(3, 0);
        MechPlaceComponent(3, 1, COMP_OR, DIR_EAST);
        MechPlaceWire(4, 1);
        MechPlaceComponent(5, 1, COMP_LIGHT, DIR_EAST);
        MechPlaceComponent(0, 2, COMP_SWITCH, DIR_NORTH);
        MechPlaceWire(1, 2);
        MechPlaceWire(2, 2);
        MechPlaceWire(3, 2);

        Cell *light = MechGetCell(5, 1);

        // Both off — off
        RunTicks(2);
        expect(light->state == false);

        // A on — on
        MechSetSwitch(0, 0, true);
        RunTicks(2);
        expect(light->state == true);

        // Both on — on
        MechSetSwitch(0, 2, true);
        RunTicks(2);
        expect(light->state == true);

        // Only B on — on
        MechSetSwitch(0, 0, false);
        RunTicks(2);
        expect(light->state == true);

        // Both off — off
        MechSetSwitch(0, 2, false);
        RunTicks(2);
        expect(light->state == false);
    }

    it("an XOR gate lights when inputs differ") {
        MechInit();
        MechPlaceComponent(0, 0, COMP_SWITCH, DIR_NORTH);
        MechPlaceWire(1, 0);
        MechPlaceWire(2, 0);
        MechPlaceWire(3, 0);
        MechPlaceComponent(3, 1, COMP_XOR, DIR_EAST);
        MechPlaceWire(4, 1);
        MechPlaceComponent(5, 1, COMP_LIGHT, DIR_EAST);
        MechPlaceComponent(0, 2, COMP_SWITCH, DIR_NORTH);
        MechPlaceWire(1, 2);
        MechPlaceWire(2, 2);
        MechPlaceWire(3, 2);

        Cell *light = MechGetCell(5, 1);

        // Both off — off
        RunTicks(2);
        expect(light->state == false);

        // A on, B off — on
        MechSetSwitch(0, 0, true);
        RunTicks(2);
        expect(light->state == true);

        // Both on — off
        MechSetSwitch(0, 2, true);
        RunTicks(2);
        expect(light->state == false);

        // A off, B on — on
        MechSetSwitch(0, 0, false);
        RunTicks(2);
        expect(light->state == true);
    }

    it("a NOR gate only lights when both inputs off") {
        MechInit();
        MechPlaceComponent(0, 0, COMP_SWITCH, DIR_NORTH);
        MechPlaceWire(1, 0);
        MechPlaceWire(2, 0);
        MechPlaceWire(3, 0);
        MechPlaceComponent(3, 1, COMP_NOR, DIR_EAST);
        MechPlaceWire(4, 1);
        MechPlaceComponent(5, 1, COMP_LIGHT, DIR_EAST);
        MechPlaceComponent(0, 2, COMP_SWITCH, DIR_NORTH);
        MechPlaceWire(1, 2);
        MechPlaceWire(2, 2);
        MechPlaceWire(3, 2);

        Cell *light = MechGetCell(5, 1);

        // Both off — on (NOR outputs 1 when both inputs 0)
        RunTicks(2);
        expect(light->state == true);

        // A on — off
        MechSetSwitch(0, 0, true);
        RunTicks(2);
        expect(light->state == false);

        // Both on — off
        MechSetSwitch(0, 2, true);
        RunTicks(2);
        expect(light->state == false);

        // Only B on — off
        MechSetSwitch(0, 0, false);
        RunTicks(2);
        expect(light->state == false);

        // Both off again — on
        MechSetSwitch(0, 2, false);
        RunTicks(2);
        expect(light->state == true);
    }
}

describe(signal_timing) {
    it("a clock should auto-toggle every N ticks") {
        MechInit();
        MechPlaceComponent(0, 5, COMP_CLOCK, DIR_NORTH);
        Cell *clk = MechGetCell(0, 5);
        clk->setting = 3;
        clk->timer = 3;
        MechPlaceWire(1, 5);
        MechPlaceComponent(2, 5, COMP_LIGHT, DIR_EAST);

        // Clock starts with state=false, timer=3
        // After 3 ticks the timer hits 0 and state toggles
        bool initial = clk->state;
        RunTicks(3);
        expect(clk->state != initial);

        bool after_first_toggle = clk->state;
        RunTicks(3);
        expect(clk->state != after_first_toggle);
    }

    it("a repeater should delay signal by its setting") {
        // Switch -> Wire -> Repeater(delay=2, facing east) -> Wire -> Light
        // Repeater reads from sigRead like a gate, so input is 1 tick behind
        // Then delay buffer adds more latency
        MechInit();
        MechPlaceComponent(0, 5, COMP_SWITCH, DIR_NORTH);
        MechPlaceWire(1, 5);
        MechPlaceComponent(2, 5, COMP_REPEATER, DIR_EAST);
        Cell *rep = MechGetCell(2, 5);
        rep->setting = 2;
        memset(rep->delayBuf, 0, sizeof(rep->delayBuf));
        MechPlaceWire(3, 5);
        MechPlaceComponent(4, 5, COMP_LIGHT, DIR_EAST);

        Cell *light = MechGetCell(4, 5);

        // Turn switch on
        MechSetSwitch(0, 5, true);

        // Tick 1: wire gets signal, but repeater reads old sigRead (0)
        MechTick();
        expect(light->state == false);

        // Tick 2: repeater reads sigRead=1, pushes into delay buffer
        MechTick();
        expect(light->state == false);

        // Tick 3: after delay=2, signal exits delay buffer
        MechTick();
        expect(light->state == true);
    }

    it("wire networks should propagate signal across long chains") {
        MechInit();
        MechPlaceComponent(0, 5, COMP_SWITCH, DIR_NORTH);
        // 10 wires in a row
        for (int i = 1; i <= 10; i++) MechPlaceWire(i, 5);
        MechPlaceComponent(11, 5, COMP_LIGHT, DIR_EAST);

        Cell *light = MechGetCell(11, 5);

        MechSetSwitch(0, 5, true);
        MechTick();
        // BFS flood-fill propagates through all wires in 1 tick
        expect(light->state == true);
    }

    it("disconnected wires should not carry signal") {
        MechInit();
        MechPlaceComponent(0, 5, COMP_SWITCH, DIR_NORTH);
        MechPlaceWire(1, 5);
        // Gap at (2,5) — no wire
        MechPlaceWire(3, 5);
        MechPlaceComponent(4, 5, COMP_LIGHT, DIR_EAST);

        Cell *light = MechGetCell(4, 5);

        MechSetSwitch(0, 5, true);
        MechTick();
        expect(light->state == false);
    }
}

// ---------------------------------------------------------------------------
// Analog layer
// ---------------------------------------------------------------------------

describe(analog_signals) {
    it("a dial should emit its setting as analog value through wire") {
        MechInit();
        MechPlaceComponent(0, 5, COMP_DIAL, DIR_NORTH);
        Cell *dial = MechGetCell(0, 5);
        dial->setting = 8;
        dial->state = true;
        MechPlaceWire(1, 5);

        MechTick();
        int sig = MechGetSignal(1, 5);
        expect(sig == 8);
    }

    it("a comparator should output 1 when input >= threshold") {
        // Dial -> Wire -> Comparator(threshold=5) -> Wire -> Light
        // Comparator reads sigRead (previous tick), so 2 ticks needed
        MechInit();
        MechPlaceComponent(0, 5, COMP_DIAL, DIR_NORTH);
        Cell *dial = MechGetCell(0, 5);
        dial->setting = 8;
        dial->state = true;
        MechPlaceWire(1, 5);
        MechPlaceComponent(2, 5, COMP_COMPARATOR, DIR_EAST);
        MechGetCell(2, 5)->setting = 5;
        MechPlaceWire(3, 5);
        MechPlaceComponent(4, 5, COMP_LIGHT, DIR_EAST);

        Cell *light = MechGetCell(4, 5);

        RunTicks(2);
        expect(light->state == true);  // 8 >= 5

        // Lower dial below threshold
        dial->setting = 3;
        RunTicks(2);
        expect(light->state == false);  // 3 < 5
    }

    it("a display should show the adjacent wire's analog value") {
        MechInit();
        MechPlaceComponent(0, 5, COMP_DIAL, DIR_NORTH);
        Cell *dial = MechGetCell(0, 5);
        dial->setting = 12;
        dial->state = true;
        MechPlaceWire(1, 5);
        MechPlaceComponent(1, 4, COMP_DISPLAY, DIR_NORTH);

        MechTick();
        Cell *disp = MechGetCell(1, 4);
        expect(disp->setting == 12);
        expect(disp->state == true);
    }
}

// ---------------------------------------------------------------------------
// Fluid layer
// ---------------------------------------------------------------------------

describe(fluid_system) {
    it("a pump should increase pressure in connected pipes") {
        MechInit();
        MechPlaceComponent(0, 5, COMP_PUMP, DIR_NORTH);
        MechGetCell(0, 5)->setting = 4;
        MechPlaceComponent(1, 5, COMP_PIPE, DIR_NORTH);
        MechPlaceComponent(2, 5, COMP_PIPE, DIR_NORTH);

        RunTicks(5);

        expect(MechGetCell(0, 5)->fluidLevel > 0);
        expect(MechGetCell(1, 5)->fluidLevel > 0);
    }

    it("a drain should decrease pressure") {
        MechInit();
        MechPlaceComponent(0, 5, COMP_PUMP, DIR_NORTH);
        MechGetCell(0, 5)->setting = 4;
        MechPlaceComponent(1, 5, COMP_PIPE, DIR_NORTH);
        MechPlaceComponent(2, 5, COMP_PIPE, DIR_NORTH);
        MechPlaceComponent(3, 5, COMP_DRAIN, DIR_NORTH);
        MechGetCell(3, 5)->setting = 4;

        RunTicks(20);

        int pipeLevel = MechGetCell(1, 5)->fluidLevel;
        expect(pipeLevel < 200);
    }

    it("a closed valve should block fluid flow") {
        MechInit();
        MechPlaceComponent(0, 5, COMP_PUMP, DIR_NORTH);
        MechGetCell(0, 5)->setting = 6;
        MechPlaceComponent(1, 5, COMP_PIPE, DIR_NORTH);
        MechPlaceComponent(2, 5, COMP_VALVE, DIR_EAST);
        MechPlaceComponent(3, 5, COMP_PIPE, DIR_NORTH);

        RunTicks(20);

        expect(MechGetCell(1, 5)->fluidLevel > 0);
        expect(MechGetCell(3, 5)->fluidLevel == 0);
    }

    it("opening a valve via wire signal should let fluid through") {
        MechInit();
        MechPlaceComponent(0, 5, COMP_PUMP, DIR_NORTH);
        MechGetCell(0, 5)->setting = 6;
        MechPlaceComponent(1, 5, COMP_PIPE, DIR_NORTH);
        MechPlaceComponent(2, 5, COMP_VALVE, DIR_EAST);
        MechPlaceComponent(3, 5, COMP_PIPE, DIR_NORTH);
        MechPlaceComponent(1, 4, COMP_SWITCH, DIR_NORTH);
        MechPlaceWire(2, 4);

        // Valve closed — no flow
        RunTicks(20);
        expect(MechGetCell(3, 5)->fluidLevel == 0);

        // Open valve via switch
        MechSetSwitch(1, 4, true);
        RunTicks(20);
        expect(MechGetCell(3, 5)->fluidLevel > 0);
    }

    it("a pressure light should emit signal when pressure is above threshold") {
        MechInit();
        MechPlaceComponent(0, 5, COMP_PUMP, DIR_NORTH);
        MechGetCell(0, 5)->setting = 4;
        MechPlaceComponent(1, 5, COMP_PIPE, DIR_NORTH);
        MechPlaceComponent(2, 5, COMP_PRESSURE_LIGHT, DIR_NORTH);
        MechPlaceWire(3, 5);
        MechPlaceComponent(4, 5, COMP_LIGHT, DIR_EAST);

        RunTicks(30);

        Cell *plight = MechGetCell(2, 5);
        expect(plight->state == true);
        expect(plight->signalOut > 0);
    }
}

// ---------------------------------------------------------------------------
// Belt layer
// ---------------------------------------------------------------------------

describe(belt_logistics) {
    it("a loader should place cargo on an adjacent belt") {
        MechInit();
        MechPlaceComponent(0, 5, COMP_LOADER, DIR_EAST);
        MechGetCell(0, 5)->setting = 1;
        MechPlaceComponent(1, 5, COMP_BELT, DIR_EAST);

        MechTick();
        expect(MechGetCell(1, 5)->cargo == 1);
    }

    it("belts should move cargo in their facing direction") {
        MechInit();
        MechPlaceComponent(0, 5, COMP_BELT, DIR_EAST);
        MechPlaceComponent(1, 5, COMP_BELT, DIR_EAST);
        MechPlaceComponent(2, 5, COMP_BELT, DIR_EAST);

        MechGetCell(0, 5)->cargo = 3;

        MechTick();
        expect(MechGetCell(0, 5)->cargo == 0);
        expect(MechGetCell(1, 5)->cargo == 3);

        MechTick();
        expect(MechGetCell(1, 5)->cargo == 0);
        expect(MechGetCell(2, 5)->cargo == 3);
    }

    it("an unloader should consume cargo and emit signal") {
        MechInit();
        MechPlaceComponent(0, 5, COMP_BELT, DIR_EAST);
        MechPlaceComponent(1, 5, COMP_UNLOADER, DIR_EAST);
        MechPlaceWire(2, 5);

        MechGetCell(0, 5)->cargo = 2;

        MechTick();
        Cell *unloader = MechGetCell(1, 5);
        expect(unloader->cargo == 0);
        expect(unloader->signalOut == 2);
        expect(unloader->state == true);
        expect(MechGetSignal(2, 5) >= 1);
    }

    it("a splitter should alternate cargo left and right") {
        // Splitter facing east — side outputs via GateInputDirs
        MechInit();
        MechPlaceComponent(1, 5, COMP_SPLITTER, DIR_EAST);
        // Side outputs
        MechPlaceComponent(1, 4, COMP_BELT, DIR_EAST);
        MechPlaceComponent(1, 6, COMP_BELT, DIR_EAST);

        // First cargo — place directly on splitter
        MechGetCell(1, 5)->cargo = 1;
        MechTick();

        int side1 = MechGetCell(1, 4)->cargo;
        int side2 = MechGetCell(1, 6)->cargo;
        expect((side1 == 1) || (side2 == 1));

        // Remember which side got first cargo
        bool first_went_north = (side1 == 1);

        // Clear side outputs
        MechGetCell(1, 4)->cargo = 0;
        MechGetCell(1, 6)->cargo = 0;

        // Second cargo — should go to the other side
        MechGetCell(1, 5)->cargo = 2;
        MechTick();

        side1 = MechGetCell(1, 4)->cargo;
        side2 = MechGetCell(1, 6)->cargo;
        if (first_went_north) {
            expect(side2 == 2);  // second should go south
        } else {
            expect(side1 == 2);  // second should go north
        }
    }

    it("a filter should only pass matching cargo types") {
        // Filter facing east, setting=1 (passes cargo type 1)
        MechInit();
        MechPlaceComponent(0, 5, COMP_BELT, DIR_EAST);
        MechPlaceComponent(1, 5, COMP_FILTER, DIR_EAST);
        MechGetCell(1, 5)->setting = 1;
        MechPlaceComponent(2, 5, COMP_BELT, DIR_EAST);

        // Matching cargo: belt -> filter -> forward belt
        MechGetCell(0, 5)->cargo = 1;
        MechTick();  // cargo moves from belt(0) to filter(1) (filter is a belt target)
        MechTick();  // filter passes matching cargo to belt(2)
        expect(MechGetCell(2, 5)->cargo == 1);

        // Non-matching cargo
        MechGetCell(2, 5)->cargo = 0;
        MechGetCell(0, 5)->cargo = 2;
        MechTick();  // cargo moves to filter
        MechTick();  // filter does NOT pass non-matching cargo
        expect(MechGetCell(2, 5)->cargo == 0);
        // cargo stays stuck on the filter
        expect(MechGetCell(1, 5)->cargo == 2);
    }

    it("a compressor should merge two inputs into dual cargo") {
        MechInit();
        // Compressor at (5,5) facing east
        MechPlaceComponent(5, 5, COMP_COMPRESSOR, DIR_EAST);

        // GateInputDirs for east-facing: inA = south, inB = north
        // But compressor reads leftDir and rightDir
        MechPlaceComponent(5, 6, COMP_BELT, DIR_NORTH);   // left (south)
        MechPlaceComponent(5, 4, COMP_BELT, DIR_SOUTH);   // right (north)
        MechPlaceComponent(6, 5, COMP_BELT, DIR_EAST);    // forward output

        MechGetCell(5, 6)->cargo = 1;
        MechGetCell(5, 4)->cargo = 2;

        MechTick();

        Cell *out = MechGetCell(6, 5);
        // Both inputs consumed
        expect(MechGetCell(5, 6)->cargo == 0);
        expect(MechGetCell(5, 4)->cargo == 0);
        // Output has dual cargo
        expect(out->cargo > 0);
        expect(out->cargo2 > 0);
    }

    it("a decompressor should split dual cargo to forward and side") {
        MechInit();
        // Decompressor at (5,5) facing east
        MechPlaceComponent(5, 5, COMP_DECOMPRESSOR, DIR_EAST);

        // Input belt behind (west)
        MechPlaceComponent(4, 5, COMP_BELT, DIR_EAST);
        // Forward output (east)
        MechPlaceComponent(6, 5, COMP_BELT, DIR_EAST);
        // Side outputs
        MechPlaceComponent(5, 4, COMP_BELT, DIR_EAST);
        MechPlaceComponent(5, 6, COMP_BELT, DIR_EAST);

        // Dual cargo on input belt
        MechGetCell(4, 5)->cargo = 1;
        MechGetCell(4, 5)->cargo2 = 2;

        MechTick();

        // Forward should have primary cargo
        expect(MechGetCell(6, 5)->cargo == 1);
        // One side should have secondary cargo
        int side_cargo = MechGetCell(5, 4)->cargo + MechGetCell(5, 6)->cargo;
        expect(side_cargo == 2);
    }
}

// ---------------------------------------------------------------------------
// Latch / Memory
// ---------------------------------------------------------------------------

describe(latch_memory) {
    it("a latch set input should turn it on, reset should turn it off") {
        // Latch facing east: inA=(facing+1)%4=south (set), inB=(facing+3)%4=north (reset)
        MechInit();
        MechPlaceComponent(5, 5, COMP_LATCH, DIR_EAST);
        MechPlaceWire(6, 5);
        MechPlaceComponent(7, 5, COMP_LIGHT, DIR_EAST);

        // Set input (south of latch)
        MechPlaceWire(5, 6);
        MechPlaceComponent(5, 7, COMP_SWITCH, DIR_NORTH);

        // Reset input (north of latch)
        MechPlaceWire(5, 4);
        MechPlaceComponent(5, 3, COMP_SWITCH, DIR_NORTH);

        Cell *light = MechGetCell(7, 5);

        // Initially off
        RunTicks(2);
        expect(light->state == false);

        // Pulse set — latch turns on (gate needs 2 ticks)
        MechSetSwitch(5, 7, true);
        RunTicks(2);
        expect(light->state == true);

        // Release set — latch stays on (memory!)
        MechSetSwitch(5, 7, false);
        RunTicks(2);
        expect(light->state == true);

        // Pulse reset — latch turns off
        MechSetSwitch(5, 3, true);
        RunTicks(2);
        expect(light->state == false);

        // Release reset — latch stays off
        MechSetSwitch(5, 3, false);
        RunTicks(2);
        expect(light->state == false);
    }
}

// ---------------------------------------------------------------------------
// Processor
// ---------------------------------------------------------------------------

describe(processor_logic) {
    it("a processor should read from adjacent wire and write to adjacent wire") {
        MechInit();
        // Input: switch -> wire on west side of processor
        MechPlaceComponent(3, 5, COMP_SWITCH, DIR_NORTH);
        MechPlaceWire(4, 5);

        // Processor at (5,5)
        MechPlaceComponent(5, 5, COMP_PROCESSOR, DIR_NORTH);

        // Output wire on east side
        MechPlaceWire(6, 5);

        // Program: READ r0 from port 3 (west), WRITE r0 to port 1 (east)
        int pi = MechFindProcessor(5, 5);
        expect(pi >= 0);
        Processor *p = MechGetProcessor(pi);
        p->progLen = 2;
        p->program[0] = (Instruction){ OP_READ, 0, 3, 0 };
        p->program[1] = (Instruction){ OP_WRITE, 0, 1, 0 };

        // Switch off — no signal on output wire
        RunTicks(4);
        expect(MechGetSignal(6, 5) == 0);

        // Switch on — processor reads input, writes to output
        MechSetSwitch(3, 5, true);
        // Tick 1: signal reaches wire(4,5) in sigRead
        // Tick 2: processor reads r0 from port 3 (west)
        // Tick 3: processor writes r0 to port 1 (east) into sigRead
        // Note: ProcWritePort writes directly to sigRead, so
        // MechGetSignal (which reads sigRead) will see it
        RunTicks(4);
        expect(MechGetSignal(6, 5) > 0);
    }
}

// ---------------------------------------------------------------------------
// Preset builders (integration tests)
// ---------------------------------------------------------------------------

describe(preset_circuits) {
    it("the NOT preset should work as expected") {
        MechInit();
        MechBuildPresetNot(0, 0);

        // NOT with no input: output=1, light ON
        RunTicks(2);
        Cell *light = MechGetCell(5, 1);
        expect(light->state == true);

        // Turn switch on — NOT inverts to 0, light OFF
        MechSetSwitch(0, 1, true);
        RunTicks(2);
        expect(light->state == false);
    }

    it("the AND preset should work as expected") {
        MechInit();
        MechBuildPresetAnd(0, 0);

        Cell *light = MechGetCell(5, 1);

        RunTicks(2);
        expect(light->state == false);

        MechSetSwitch(0, 0, true);
        MechSetSwitch(0, 2, true);
        RunTicks(2);
        expect(light->state == true);
    }

    it("the XOR preset should work as expected") {
        MechInit();
        MechBuildPresetXor(0, 0);

        Cell *light = MechGetCell(5, 1);

        RunTicks(2);
        expect(light->state == false);

        MechSetSwitch(0, 0, true);
        RunTicks(2);
        expect(light->state == true);

        MechSetSwitch(0, 2, true);
        RunTicks(2);
        expect(light->state == false);
    }

    it("the blinker preset should oscillate") {
        MechInit();
        MechBuildPresetBlinker(0, 0);

        Cell *light = MechGetCell(2, 1);

        bool saw_on = false, saw_off = false;
        for (int i = 0; i < 20; i++) {
            MechTick();
            if (light->state) saw_on = true;
            else saw_off = true;
        }
        expect(saw_on);
        expect(saw_off);
    }

    it("the pump loop preset should build up and stabilize fluid") {
        MechInit();
        MechBuildPresetPumpLoop(0, 0);

        // Preset already runs 30 ticks during build
        expect(MechGetCell(0, 1)->fluidLevel > 0);
        expect(MechGetCell(3, 1)->fluidLevel > 0);
    }

    it("the belt line preset should move cargo from loader to unloader") {
        MechInit();
        MechBuildPresetBeltLine(0, 0);

        RunTicks(20);

        Cell *unloader1 = MechGetCell(8, 0);
        Cell *unloader2 = MechGetCell(8, 2);
        bool received = unloader1->state || unloader2->state ||
                        unloader1->signalOut > 0 || unloader2->signalOut > 0;
        expect(received);
    }
}

// ---------------------------------------------------------------------------
// Mechanical layer
// ---------------------------------------------------------------------------

describe(mechanical_system) {
    it("a crank should power a connected shaft") {
        MechInit();
        MechPlaceComponent(0, 5, COMP_CRANK, DIR_NORTH);
        MechGetCell(0, 5)->state = true;  // engaged
        MechPlaceComponent(1, 5, COMP_SHAFT, DIR_NORTH);
        MechPlaceComponent(2, 5, COMP_SHAFT, DIR_NORTH);

        RunTicks(10);
        expect(MechGetCell(1, 5)->mechSpeed > 0);
        expect(MechGetCell(2, 5)->mechSpeed > 0);
    }

    it("a shaft network should share speed across all segments") {
        MechInit();
        MechPlaceComponent(0, 5, COMP_CRANK, DIR_NORTH);
        MechGetCell(0, 5)->state = true;
        MechPlaceComponent(1, 5, COMP_SHAFT, DIR_NORTH);
        MechPlaceComponent(2, 5, COMP_SHAFT, DIR_NORTH);
        MechPlaceComponent(3, 5, COMP_SHAFT, DIR_NORTH);

        RunTicks(10);
        float s1 = MechGetCell(1, 5)->mechSpeed;
        float s2 = MechGetCell(2, 5)->mechSpeed;
        float s3 = MechGetCell(3, 5)->mechSpeed;
        expect(s1 > 0);
        expect(s1 == s2);
        expect(s2 == s3);
    }

    it("a clutch should disconnect mechanical networks when no signal") {
        MechInit();
        MechPlaceComponent(0, 5, COMP_CRANK, DIR_NORTH);
        MechGetCell(0, 5)->state = true;
        MechPlaceComponent(1, 5, COMP_SHAFT, DIR_NORTH);
        MechPlaceComponent(2, 5, COMP_CLUTCH, DIR_NORTH);
        MechPlaceComponent(3, 5, COMP_SHAFT, DIR_NORTH);

        // No wire signal to clutch — should disconnect
        RunTicks(10);
        float before = MechGetCell(1, 5)->mechSpeed;
        float after = MechGetCell(3, 5)->mechSpeed;
        expect(before > 0);
        expect(after == 0);  // disconnected
    }

    it("engaging a clutch via wire should connect networks") {
        MechInit();
        MechPlaceComponent(0, 5, COMP_CRANK, DIR_NORTH);
        MechGetCell(0, 5)->state = true;
        MechPlaceComponent(1, 5, COMP_SHAFT, DIR_NORTH);
        MechPlaceComponent(2, 5, COMP_CLUTCH, DIR_NORTH);
        MechPlaceComponent(3, 5, COMP_SHAFT, DIR_NORTH);
        // Wire signal adjacent to clutch
        MechPlaceComponent(2, 4, COMP_SWITCH, DIR_NORTH);
        MechPlaceWire(2, 4);  // wire on switch cell won't work, put wire adjacent
        // Actually: switch seeds adjacent wires. Place wire next to clutch.
        MechPlaceComponent(2, 4, COMP_SWITCH, DIR_NORTH);
        MechPlaceWire(3, 4);  // wire adjacent to clutch at (2,5)? Let's use (1,4) connected chain
        // Simpler: switch at (2,4), wire at (2,4) — IsClutchEngaged checks for adjacent wire signal.
        // Let me set this up properly:
        MechInit();  // restart
        MechPlaceComponent(0, 5, COMP_CRANK, DIR_NORTH);
        MechGetCell(0, 5)->state = true;
        MechPlaceComponent(1, 5, COMP_SHAFT, DIR_NORTH);
        MechPlaceComponent(2, 5, COMP_CLUTCH, DIR_NORTH);
        MechPlaceComponent(3, 5, COMP_SHAFT, DIR_NORTH);

        // Switch -> Wire next to clutch
        MechPlaceComponent(2, 3, COMP_SWITCH, DIR_NORTH);
        MechPlaceWire(2, 4);

        // Switch off — clutch disconnected
        RunTicks(10);
        expect(MechGetCell(3, 5)->mechSpeed == 0);

        // Switch on — clutch engaged
        MechSetSwitch(2, 3, true);
        RunTicks(10);
        expect(MechGetCell(3, 5)->mechSpeed > 0);
    }

    it("a flywheel should resist speed changes") {
        // Network with flywheel should accelerate slower
        MechInit();
        MechPlaceComponent(0, 5, COMP_CRANK, DIR_NORTH);
        MechGetCell(0, 5)->state = true;
        MechGetCell(0, 5)->setting = 3;
        MechPlaceComponent(1, 5, COMP_SHAFT, DIR_NORTH);

        // Network without flywheel
        MechInit();
        MechPlaceComponent(0, 5, COMP_CRANK, DIR_NORTH);
        MechGetCell(0, 5)->state = true;
        MechGetCell(0, 5)->setting = 3;
        MechPlaceComponent(1, 5, COMP_SHAFT, DIR_NORTH);
        RunTicks(5);
        float speedNoFlywheel = MechGetCell(1, 5)->mechSpeed;

        // Network with flywheel
        MechInit();
        MechPlaceComponent(0, 5, COMP_CRANK, DIR_NORTH);
        MechGetCell(0, 5)->state = true;
        MechGetCell(0, 5)->setting = 3;
        MechPlaceComponent(1, 5, COMP_SHAFT, DIR_NORTH);
        MechPlaceComponent(2, 5, COMP_FLYWHEEL, DIR_NORTH);
        MechGetCell(2, 5)->setting = 5;
        RunTicks(5);
        float speedWithFlywheel = MechGetCell(1, 5)->mechSpeed;

        // Flywheel adds inertia, so speed should be lower after same ticks
        expect(speedWithFlywheel < speedNoFlywheel);
    }

    it("an escapement should emit signal pulses based on shaft speed") {
        MechInit();
        MechPlaceComponent(0, 5, COMP_CRANK, DIR_NORTH);
        MechGetCell(0, 5)->state = true;
        MechPlaceComponent(1, 5, COMP_SHAFT, DIR_NORTH);
        MechPlaceComponent(2, 5, COMP_ESCAPEMENT, DIR_EAST);
        MechPlaceWire(3, 5);

        // Run until shaft has speed
        RunTicks(20);
        expect(MechGetCell(2, 5)->mechSpeed > 0);

        // Check that escapement toggled at least once
        bool saw_on = false, saw_off = false;
        for (int i = 0; i < 40; i++) {
            MechTick();
            if (MechGetCell(2, 5)->state) saw_on = true;
            else saw_off = true;
        }
        expect(saw_on);
        expect(saw_off);
    }

    it("a cam shaft should output signal based on its pattern") {
        MechInit();
        MechPlaceComponent(0, 5, COMP_CRANK, DIR_NORTH);
        MechGetCell(0, 5)->state = true;
        MechPlaceComponent(1, 5, COMP_SHAFT, DIR_NORTH);
        MechPlaceComponent(2, 5, COMP_CAM_SHAFT, DIR_EAST);
        MechGetCell(2, 5)->setting = 0xAA;  // 10101010 pattern

        // Run enough ticks for cam to cycle through positions
        RunTicks(10);
        expect(MechGetCell(2, 5)->mechSpeed > 0);

        // Over many ticks, should see both ON and OFF states from the alternating pattern
        bool saw_on = false, saw_off = false;
        for (int i = 0; i < 100; i++) {
            MechTick();
            if (MechGetCell(2, 5)->state) saw_on = true;
            else saw_off = true;
        }
        expect(saw_on);
        expect(saw_off);
    }

    it("a hammer should fire when shaft speed is sufficient") {
        MechInit();
        MechPlaceComponent(0, 5, COMP_CRANK, DIR_NORTH);
        MechGetCell(0, 5)->state = true;
        MechGetCell(0, 5)->setting = 5;
        MechPlaceComponent(1, 5, COMP_SHAFT, DIR_NORTH);
        MechPlaceComponent(2, 5, COMP_HAMMER, DIR_NORTH);
        MechGetCell(2, 5)->setting = 3;  // load

        // Initially no speed — hammer off
        expect(MechGetCell(2, 5)->state == false);

        // After enough ticks, speed > 5.0 threshold
        RunTicks(20);
        expect(MechGetCell(2, 5)->state == true);
    }

    it("a governor should output analog signal proportional to speed") {
        MechInit();
        MechPlaceComponent(0, 5, COMP_CRANK, DIR_NORTH);
        MechGetCell(0, 5)->state = true;
        MechGetCell(0, 5)->setting = 5;
        MechPlaceComponent(1, 5, COMP_SHAFT, DIR_NORTH);
        MechPlaceComponent(2, 5, COMP_GOVERNOR, DIR_NORTH);
        MechPlaceWire(3, 5);

        // After enough ticks, governor should output a non-zero analog value
        RunTicks(20);
        expect(MechGetCell(2, 5)->signalOut > 0);
        expect(MechGetCell(2, 5)->state == true);
        // Analog value should be proportional: (speed * 15 / 100)
        float speed = MechGetCell(2, 5)->mechSpeed;
        int expectedAnalog = (int)(speed * 15.0f / 100.0f);
        expect(MechGetCell(2, 5)->signalOut == expectedAnalog);
    }

    it("a spring should release stored energy on signal") {
        MechInit();
        MechPlaceComponent(0, 5, COMP_SPRING, DIR_NORTH);
        MechGetCell(0, 5)->setting = 8;  // capacity
        MechPlaceComponent(1, 5, COMP_SHAFT, DIR_NORTH);

        // Let spring charge up (no trigger signal)
        RunTicks(50);
        float charge = MechGetCell(0, 5)->springCharge;
        expect(charge > 0);
        float speedBefore = MechGetCell(1, 5)->mechSpeed;

        // Trigger spring with adjacent wire signal
        MechPlaceComponent(0, 4, COMP_SWITCH, DIR_NORTH);
        MechPlaceWire(0, 4);  // wire at switch position seeds adjacent
        // Actually need wire adjacent to spring. Place switch further away.
        MechPlaceComponent(1, 4, COMP_SWITCH, DIR_NORTH);
        MechPlaceWire(0, 4);
        MechSetSwitch(1, 4, true);
        RunTicks(5);

        // Speed should have increased from the burst
        float speedAfter = MechGetCell(1, 5)->mechSpeed;
        expect(speedAfter > speedBefore);
    }

    it("disengaging a crank should let the network slow down") {
        MechInit();
        MechPlaceComponent(0, 5, COMP_CRANK, DIR_NORTH);
        MechGetCell(0, 5)->state = true;
        MechGetCell(0, 5)->setting = 5;
        MechPlaceComponent(1, 5, COMP_SHAFT, DIR_NORTH);

        RunTicks(20);
        float speedEngaged = MechGetCell(1, 5)->mechSpeed;
        expect(speedEngaged > 0);

        // Disengage crank
        MechGetCell(0, 5)->state = false;
        RunTicks(30);
        float speedDisengaged = MechGetCell(1, 5)->mechSpeed;
        expect(speedDisengaged < speedEngaged);
    }
}

describe(mechanical_presets) {
    it("the auto-hammer preset should have a working hammer") {
        MechInit();
        MechBuildPresetAutoHammer(0, 0);
        // Preset runs 20 internal ticks during build
        // Crank is engaged, so shaft should have speed and hammer should fire
        RunTicks(10);
        bool foundHammer = false;
        for (int x = 0; x < 10; x++) {
            Cell *c = MechGetCell(x, 0);
            if (c->type == COMP_HAMMER && c->mechSpeed > 0) {
                foundHammer = true;
                break;
            }
        }
        expect(foundHammer);
    }

    it("the clock tower preset should produce blinking") {
        MechInit();
        MechBuildPresetClockTower(0, 0);
        // Should have an escapement that toggles and eventually a light
        bool sawEscOn = false, sawEscOff = false;
        for (int i = 0; i < 60; i++) {
            MechTick();
            for (int x = 0; x < 10; x++) {
                Cell *c = MechGetCell(x, 0);
                if (c->type == COMP_ESCAPEMENT) {
                    if (c->state) sawEscOn = true;
                    else sawEscOff = true;
                }
            }
        }
        expect(sawEscOn);
        expect(sawEscOff);
    }

    it("the governor loop preset should self-regulate speed") {
        MechInit();
        MechBuildPresetGovernorLoop(0, 0);
        RunTicks(50);
        // Governor should have a non-zero speed
        bool foundGov = false;
        for (int y = 0; y < 5; y++) {
            for (int x = 0; x < 10; x++) {
                Cell *c = MechGetCell(x, y);
                if (c->type == COMP_GOVERNOR && c->mechSpeed > 0) {
                    foundGov = true;
                }
            }
        }
        expect(foundGov);
    }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'q') {
            set_quiet_mode(1);
            break;
        }
    }

    test(signal_basics);
    test(signal_timing);
    test(analog_signals);
    test(fluid_system);
    test(belt_logistics);
    test(latch_memory);
    test(processor_logic);
    test(preset_circuits);
    test(mechanical_system);
    test(mechanical_presets);

    return summary();
}
