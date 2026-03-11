# Prototype -> Daw.c Feature Parity (2026-03-11)

What prototype.c has that daw.c doesn't, and vice versa. Goal: identify what should migrate to daw.c so prototype.c can eventually be retired.

## Prototype Features Missing from Daw.c

### Worth Migrating

| Feature | Effort | Notes |
|---|---|---|
| **Crossfader / Scene System** | Medium | 8 scene slots, save/load state snapshots, crossfade blending between two scenes. Key for game bridge (iMUSE-style). Demo has `blendSynthPatch()`, `blendDrumParams()`, `blendEffects()`. |
| **Speech/Voice Synthesis** | Medium | Babble generator, text-to-phoneme, intonation contours (rising/falling), speech queue. Unique creative feature. |
| **SFX Triggers** | Small | 6 preset one-shot sounds (jump, coin, hurt, explosion, powerup, blip). Useful for game audio preview. |
| **Direct Drum Triggering** | Small | Number keys 7-0 and numpad trigger individual drums with p-lock support (volume, pitch). Good for live performance. |
| **Column Visibility Toggles** | Small | Boolean flags to show/hide parameter sections (wave, synth, perc, lfo, drums, effects, tape, mixer). Declutters UI. |
| **"Use as Bass/Lead/Chord" Quick-Copy** | Small | When previewing a preset, one-click to copy it to a specific track's patch slot. Nice workflow shortcut. |

### Probably Not Worth Migrating

| Feature | Why |
|---|---|
| Simple piano keyboard (ASDF keys) | Daw.c already has musical typing + MIDI input with more features |
| Song file save/load (.song format) | Daw.c has its own .daw binary format with more data |

## Daw.c Features Not in Prototype

These are daw.c advantages — no migration needed, just noting for completeness:

| Feature | Notes |
|---|---|
| **Song Arrangement** | 64 sections, named, per-section loop/BPM/mute overrides |
| **Piano Roll Editor** (F2) | Visual note grid, 4-octave range, click-to-place |
| **WAV Recording** (F7) | 30s capture to daw_output.wav |
| **Per-Bus Effects Rack** | Filter, distortion, delay per bus (7 buses) |
| **Advanced Mixer Strip** | Per-bus volume/pan/reverb-send, mute/solo |
| **Multi-Tab Workspace** | F1=Sequencer, F2=PianoRoll, F3=Song, F4=MIDI |
| **MIDI Tab** (F4) | Device status, split config, visual keyboard, MIDI learn list |
| **Sidechain Ducking** | 5 source options, 4 targets, attack/release/depth |
| **8 Patch Slots** | vs demo's 4 |
| **Voice Monitor Display** | Real-time colored rectangles showing voice states |
| **Envelope/LFO Visualization** | ADSR curve drawing, real-time LFO shape preview |

## Feature Parity (Both Have)

Synth engine (14 osc types), ADSR, filter system, 4 LFOs, arpeggiator, scale lock, drum synthesis, effects chain (distortion/delay/reverb/tape/crush/chorus/flanger), mono/glide, unison, pattern storage, conditional triggers, probability, p-locks, MIDI input.

## Suggested Migration Order

1. **Crossfader / Scene System** — Most impactful. Needed for game bridge and live performance workflows. The blending functions are self-contained and should port cleanly.
2. **Column Visibility Toggles** — Quick win for UI decluttering. Just boolean flags + conditional rendering.
3. **SFX Triggers + Direct Drum Keys** — Small additions, useful for demo/preview purposes.
4. **Speech/Voice Synthesis** — Cool unique feature but lower priority for DAW workflow.
5. **Quick-Copy Preset Buttons** — Nice workflow shortcut, tiny effort.

## End State

Once daw.c has all the worth-migrating features, prototype.c becomes a legacy reference. It can stay around for quick testing but daw.c becomes the single UI for all sound work.
