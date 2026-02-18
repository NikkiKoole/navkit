# Mechanical / Kinetic Layer

Victorian clockwork automation — no magic, no electricity, just clever arrangements of physical things. Every step is visible: water turns wheel, wheel turns shaft, shaft turns cam, cam pushes lever, lever opens gate.

## Core Simulation Values

Each shaft segment carries three values:

- **speed** — how fast it's turning (RPM equivalent)
- **torque** — how much force is available
- **load** — how much resistance from connected machines

Fundamental rule: `torque - load = acceleration`. Too much load and the shaft slows down and stalls. Gear ratios multiply torque but divide speed. A flywheel resists speed changes in both directions.

## Components (12 for sandbox prototype)

### Power Sources

| Component | Description |
|-----------|-------------|
| **Crank** | Manual power input. Click to wind up, stores energy. Constant low output while engaged. |
| **Spring** | Stores energy over time, releases in a burst. Wind-up duration determines burst strength. Signal-controlled release. |

In the full game: Waterwheel (constant power from adjacent water), Windmill (variable power, fluctuates).

### Transmission

| Component | Description |
|-----------|-------------|
| **Shaft** | Carries rotational energy in a line. Like wire but for torque. |
| **Gear** | Changes direction 90 degrees. Can chain for U-turns. |
| **Gear Ratio** | Trades speed for torque or vice versa. Settings like 1:2, 1:3, 2:1. A 1:3 ratio triples torque but cuts speed to a third. |
| **Clutch** | Signal-controlled connect/disconnect. Like a valve for mechanical power. Wire signal engages/disengages. |
| **Flywheel** | Smooths out power fluctuations. Stores momentum — takes time to spin up, resists slowing down. Heavy load briefly? Flywheel absorbs the shock. |

### Timing / Sequencing

| Component | Description |
|-----------|-------------|
| **Escapement** | Converts continuous rotation into precise ticks. The heart of a clock. Output: regular on/off signal at a rate determined by shaft speed. |
| **Cam Shaft** | The star component. A rotating drum with bumps. Each bump triggers an output pulse as it passes. Click to edit the bump pattern — like programming a music box. Rotation speed determines playback speed. Multiple output positions possible. |

### Output / Actuators

| Component | Description |
|-----------|-------------|
| **Hammer** | Strikes when powered. Consumes torque on each strike. For crushing, forging, ringing bells. Visual: rhythmic up-down motion. |
| **Lever Arm** | Converts rotation to push/pull. Opens gates, moves things, flips switches. Binary output: extended or retracted. |

### Measurement

| Component | Description |
|-----------|-------------|
| **Governor** | Spinning balls on arms (classic steam engine regulator). Outputs a signal proportional to shaft speed. Bridges mechanical layer back to signal layer. Too fast? Governor signal triggers a brake via wire. |

## Visualization Ideas

- Shafts show rotation direction with animated chevrons or dashes
- Speed shown by animation rate — fast shaft visibly spins, slow shaft crawls
- Stalled shafts flash red or show a "jammed" indicator
- Gears mesh visually with adjacent gears
- Cam shaft shows the bump pattern as a small circular diagram
- Flywheel size indicates stored energy
- Governor balls spread wider at higher speeds
- Springs visually compress as they charge, snap back on release

## Example Contraptions

### Auto-Hammer (simplest)
```
Crank -> Shaft -> Hammer
```
Click crank, shaft spins, hammer pounds. Stops when you stop cranking.

### Timed Gate
```
Crank -> Shaft -> Escapement -> [signal] -> Clutch
                                               |
                              Shaft -> Lever Arm -> Gate
```
Escapement ticks at regular intervals. Each tick engages clutch briefly, lever arm pushes gate open then closed.

### Stamp Mill
```
Crank -> Gear Ratio (1:3) -> Cam Shaft [pattern: X.X.X.] -> Hammer
                                                           -> Hammer
                                                           -> Hammer
```
Gear ratio slows the cam but increases torque. Three hammers fire in sequence based on cam pattern. Rhythmic crushing.

### Drawbridge
```
Spring (wound up over time) -> Clutch -> Shaft -> Winch -> Counterweight -> Bridge
                                  ^
                              [signal from lever]
```
Spring slowly charges. Pull lever (wire signal), clutch engages, spring energy dumps into winch, bridge slams shut instantly.

### Clock Tower
```
Crank -> Flywheel -> Escapement -> Cam Shaft [pattern: dawn/noon/dusk]
                                       |          |          |
                                     Bell    Granary     Gate
                                              Valve     Close
```
Flywheel keeps it steady. Escapement ticks. Cam shaft triggers different events at different positions in rotation. Fortress runs on a schedule.

### Safety Governor
```
Waterwheel -> Shaft -> Governor -> [signal, proportional to speed]
                                       |
                                   Comparator (threshold=8)
                                       |
                                   Clutch (disconnect if too fast)
```
Governor measures shaft speed. If speed exceeds threshold, comparator fires, clutch disengages, preventing mechanical damage. Self-regulating.

## Integration Points (for main game later)

- **Movers** crank the cranks, wind the springs — new job types
- **Water simulation** powers waterwheels — ties into existing fluid system
- **Wind system** powers windmills — ties into existing weather
- **Workshops** connect as loads on shafts — powered workshop runs without mover labor
- **Signal layer** bridges via Governor (mechanical->signal) and Clutch (signal->mechanical)
- **Belt layer** could be driven by shafts instead of being self-propelled
