# Dub Loop - King Tubby Style Tape Delay

The Dub Loop effect emulates the classic tape delay techniques pioneered by King Tubby in 1970s Jamaica. Unlike modern digital delays, this effect focuses on organic degradation, real-time manipulation, and the characteristic "woozy" sound of analog tape.

## Key Features

### 1. Selectable Input Source

Control what goes into the delay, just like King Tubby's send/return mixing technique. You can select from broad categories down to individual instruments.

```c
// Broad input source options
DUB_INPUT_ALL      // Everything goes to delay (default)
DUB_INPUT_DRUMS    // All drums feed the delay
DUB_INPUT_SYNTH    // All synth voices feed the delay
DUB_INPUT_MANUAL   // Nothing until you "throw" it in

// Individual drum sources
DUB_INPUT_KICK     // Only kick drum
DUB_INPUT_SNARE    // Only snare
DUB_INPUT_CLAP     // Only clap
DUB_INPUT_CLOSED_HH // Only closed hihat
DUB_INPUT_OPEN_HH  // Only open hihat
DUB_INPUT_LOW_TOM  // Only low tom
DUB_INPUT_MID_TOM  // Only mid tom
DUB_INPUT_HI_TOM   // Only hi tom
DUB_INPUT_RIMSHOT  // Only rimshot
DUB_INPUT_COWBELL  // Only cowbell
DUB_INPUT_CLAVE    // Only clave
DUB_INPUT_MARACAS  // Only maracas

// Individual synth voice sources
DUB_INPUT_BASS     // Only bass voice
DUB_INPUT_LEAD     // Only lead voice
DUB_INPUT_CHORD    // Only chord voice

// Set the input source
dubLoopSetInput(DUB_INPUT_SNARE);  // Only snare goes to delay!

// Manual throw (momentarily send audio to delay)
dubLoopThrow();  // Start capturing
dubLoopCut();    // Stop capturing, let echoes decay
```

**Classic technique:** Set to `DUB_INPUT_SNARE`, and only snare hits will echo while everything else stays dry. This is the authentic King Tubby approach - selectively sending individual instruments to the delay.

### 2. Reverb Routing Toggle (preReverb)

Switch between two classic routing configurations:

| Mode | Signal Path | Character |
|------|-------------|-----------|
| `preReverb = false` | Signal -> Delay -> Reverb | "Echo in a room" - each echo sits in space |
| `preReverb = true` | Signal -> Reverb -> Delay | "Room being echoed" - more washy, atmospheric |

```c
// Toggle the routing
dubLoopTogglePreReverb();

// Or set explicitly
dubLoopSetPreReverb(true);   // Reverb before delay
dubLoopSetPreReverb(false);  // Delay before reverb (default)

// Check current state
if (dubLoopIsPreReverb()) { /* ... */ }
```

### 3. Delay Time Drift (Woozy Feel)

Real tape delays don't sync to tempo - they drift and wander, creating that characteristic "spaced out" feel.

```c
dubLoop.drift = 0.3f;      // Amount of drift (0-1), 0.3 = subtle wander
dubLoop.driftRate = 0.2f;  // How fast it drifts (0.1 = slow, 1.0 = faster)
```

Each playback head drifts independently using layered LFOs, creating organic timing variations that no two echoes are exactly alike.

### 4. Cumulative Degradation (Per-Echo Aging)

Each pass through the "tape" degrades the signal more. The 5th echo sounds noticeably worse than the 1st - darker, noisier, more saturated.

```c
dubLoop.degradeRate = 0.15f;  // 15% more degradation per echo generation
```

What happens per echo:
- **Saturation increases** - more tape compression/distortion
- **High frequencies roll off** - progressively darker
- **Noise accumulates** - more hiss with each pass

Fresh input resets the "echo age," so new material starts clean.

### 5. Multiple Playback Heads

Like the Roland Space Echo, you can have up to 3 playback heads with different delay times:

```c
dubLoop.numHeads = 3;
dubLoop.headTime[0] = 0.375f;  // Dotted eighth
dubLoop.headTime[1] = 0.25f;   // Straight eighth  
dubLoop.headTime[2] = 0.5f;    // Quarter note
dubLoop.headLevel[0] = 1.0f;   // Full level
dubLoop.headLevel[1] = 0.7f;   // Slightly quieter
dubLoop.headLevel[2] = 0.5f;   // Quieter still
```

## Quick Reference: Parameters

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| `enabled` | bool | false | Master enable |
| `inputSource` | 0-3 | 0 (ALL) | What feeds the delay |
| `preReverb` | bool | false | Reverb routing |
| `inputGain` | 0-1 | 1.0 | Input level to tape |
| `feedback` | 0-0.95 | 0.6 | Echo regeneration |
| `mix` | 0-1 | 0.4 | Dry/wet balance |
| `speed` | 0.5-2.0 | 1.0 | Current tape speed |
| `speedTarget` | 0.5-2.0 | 1.0 | Target speed (slewed) |
| `speedSlew` | 0-1 | 0.1 | Speed change rate |
| `saturation` | 0-1 | 0.3 | Tape saturation |
| `toneHigh` | 0-1 | 0.7 | High freq presence |
| `toneLow` | 0-1 | 0.1 | Low freq cut |
| `noise` | 0-1 | 0.05 | Tape hiss |
| `degradeRate` | 0-1 | 0.15 | Per-echo degradation |
| `wow` | 0-1 | 0.2 | Slow pitch wobble |
| `flutter` | 0-1 | 0.1 | Fast pitch wobble |
| `drift` | 0-1 | 0.3 | Delay time drift |
| `driftRate` | 0-1 | 0.2 | Drift speed |

## API Functions

```c
// Input control
void dubLoopThrow(void);           // Start recording to tape
void dubLoopCut(void);             // Stop input, let decay
void dubLoopSetInput(int source);  // Set input source

// Speed control
void dubLoopHalfSpeed(void);       // Drop to half speed (octave down)
void dubLoopNormalSpeed(void);     // Return to normal
void dubLoopDoubleSpeed(void);     // Double speed (octave up)

// Routing
void dubLoopTogglePreReverb(void);
void dubLoopSetPreReverb(bool pre);
bool dubLoopIsPreReverb(void);

// Processing (called internally by processEffects)
float processDubLoop(float input, float dt);
float processDubLoopWithInputs(float drumInput, float synthInput, float dt);
```

## Usage with Separate Buses

For full King Tubby style control, use `processEffectsWithBuses()`:

```c
// In your audio callback:
float drumSample = processDrums(dt);
float synthSample = 0.0f;
for (int v = 0; v < NUM_VOICES; v++) {
    synthSample += processVoice(&synthVoices[v], SAMPLE_RATE);
}

// Process with selective dub loop input
float output = processEffectsWithBuses(drumSample, synthSample, dt);
```

This allows the dub loop to selectively capture drums or synth while the full mix continues.

## Classic Dub Techniques

### The "Throw and Decay"
```c
dubLoopSetInput(DUB_INPUT_MANUAL);
dubLoop.feedback = 0.75f;

// On snare hit:
dubLoopThrow();
// Next beat:
dubLoopCut();
// Snare echoes into infinity while track continues clean
```

### Half-Speed Drop
```c
// During breakdown:
dubLoopHalfSpeed();
// Echoes drop an octave, slow and heavy

// Return:
dubLoopNormalSpeed();
```

### Washy Atmospheric
```c
dubLoopSetPreReverb(true);
dubLoop.feedback = 0.8f;
dubLoop.drift = 0.5f;
dubLoop.degradeRate = 0.25f;
// Reverb tails echo and degrade into washed-out ambience
```

## Future Improvements

Potential additions inspired by King Tubby's techniques:

- **Feedback slew** - Smoothly ride feedback like the "big knob"
- **Parametric filter sweep** - MCI mixer style dramatic EQ
- **Shake/thunderclap** - Physical disturbance simulation
- **Per-channel delay** - Different delay times for L/R
