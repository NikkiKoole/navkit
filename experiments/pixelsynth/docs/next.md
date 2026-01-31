# PixelSynth: Next Build

---

## Completed Features

### Core Sequencer
- **Probability per step** - `drumProbability` and `melodyProbability` arrays
- **Gate length per step** - `melodyGate` for note-off timing
- **Pattern switching** - `seqSwitchPattern()`, `seqQueuePattern()`, 8 patterns
- **Conditional triggers** - 12 Elektron-style conditions (1:2, 2:2, Fill, etc.)
- **Parameter locks (P-locks)** - Per-step overrides for filter, decay, tone, etc.
- **Per-step slide & accent** - 303-style `melodySlide` and `melodyAccent`

### Sound Engine
- **Filter self-oscillation** - Tuned SVF for screaming 303-style resonance
- **Scale lock** - 9 scale types, constrain notes to key
- **Reverb** - Schroeder-style algorithmic reverb with pre-delay

### Scenes & Crossfader (just completed)
- **Scenes** - 8 slots to save/load complete sound snapshots
  - Saves: all 4 synth patches, drum params, effects, volumes
  - UI: click=load, shift+click=save, right-click=clear
- **Crossfader** - A/B scene blending with slider
  - Linear interpolation for continuous params
  - Threshold switch at 50% for discrete params (wave type, presets, bools)
  - Enable with "XFade" toggle, click A:/B: to select scenes

---

## Next Priorities

### 1. Game State System
Drive audio parameters from gameplay variables:

```c
float gameIntensity = 0.0f;  // 0=calm, 1=combat
float gameDanger = 0.0f;     // Proximity to threats
float gameHealth = 0.5f;     // Player health
```

Options:
- Simple: `crossfader.position = gameIntensity`
- Advanced: Direct parameter modulation (filter, tempo, reverb)
- Triggers: Game events trigger stingers/one-shots

### 2. Other Ideas
- Save/load scenes to file (persist across sessions)
- MIDI CC mapping for crossfader
- Scene sequencing (auto-switch on pattern boundaries)
- More effect types (chorus, phaser, compressor)
