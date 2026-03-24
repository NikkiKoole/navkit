# Soundsystem (PixelSynth)

Header-only synthesized audio engine (~36K lines). 29 synthesis engines, 32-voice polyphony (+8-voice sampler), 263 presets, step sequencer, full effects chain, built-in DAW.

## Build & Run

All targets are in the **root Makefile** (`/Users/nikkikoole/Projects/navkit/Makefile`):

```bash
# Game with sound
make path-sound          # main game binary with -DUSE_SOUNDSYSTEM

# Standalone DAW
make soundsystem-daw     # full DAW UI (raylib required)
./soundsystem-daw

# Jukebox (song player)
make jukebox-test        # play songs without the full game
./jukebox-test --list    # list songs
./jukebox-test 0         # play song by index

# Tests
make test_soundsystem    # 303 tests, 2073 assertions (sequencer, synth, effects, integration)

# Tools
make preset-audition     # headless preset → WAV renderer + spectral analysis
make song-render         # .song → WAV export
make daw-render          # headless DAW render (stubbed raylib)
make scw_embed           # WAV cycles → scw_data.h (wavetable generator)
```

`test_soundsystem` is included in `make test`, `make test-full`, and `make test-quick`. To run **only** sound tests after sound changes: `make test_soundsystem`.

## File Structure

```
soundsystem/
├── soundsystem.h              # Main include (pulls in all engines)
├── engines/
│   ├── synth.h                # Synth engine: voice processing, triggers, SFX (~2870 lines)
│   ├── synth_oscillators.h   # Complex oscillator engines (~1550 lines, extracted from synth.h)
│   ├── synth_scale.h          # Scale lock system (~114 lines, extracted from synth.h)
│   ├── synth_patch.h          # SynthPatch struct (200+ p_ fields)
│   ├── instrument_presets.h   # 263 presets (melodic + drums: 808/909/CR-78/orchestral/hand) (~1850 lines)
│   ├── effects.h              # Effects chain + bus mixer (8 buses, 27 master FX) (~1516 lines)
│   ├── dub_loop.h             # King Tubby tape delay (~310 lines, extracted from effects.h)
│   ├── rewind.h               # Vinyl spinback effect (~165 lines, extracted from effects.h)
│   ├── sequencer.h            # Step sequencer v2 (96 PPQ, 12 tracks: 4 drum + 7 melody + 1 sampler) (~1735 lines)
│   ├── sequencer_plocks.h     # Parameter lock subsystem (~170 lines, extracted from sequencer.h)
│   ├── patch_trigger.h        # SynthPatch → synth globals applicator
│   ├── sampler.h              # Sample playback (8-voice, 32 slots)
│   ├── sample_chop.h         # Chop/flip: bounce, slice, load, freeze
│   ├── song_file.h            # .song/.patch/.bank file I/O
│   ├── midi_input.h           # CoreMIDI input (macOS)
│   ├── rhythm_patterns.h      # 14 rhythm style generators
│   ├── rhythm_prob_maps.h     # 27 probability map presets
│   ├── piku_presets.h         # Pikuniku-style presets
│   └── scw_data.h             # Generated wavetable data (from scw_embed)
├── demo/
│   ├── daw.c                  # Full DAW UI (~5600 lines, 6 tabs inc. Sample)
│   ├── daw_file.h             # DAW save/load (.daw format)
│   ├── daw_widgets.h          # Bespoke UI widgets (~250 lines, extracted from daw.c)
│   └── daw_audio.h            # Audio callback + sequencer callbacks (~590 lines, extracted from daw.c)
├── tools/                     # CLI tools (see Build & Run)
├── tests/
│   └── test_daw_file.c        # DAW file round-trip tests (~83 assertions)
└── docs/
    ├── plan-of-attack.md      # Current roadmap (what's done/TODO)
    ├── roadmap.md             # Long-term vision
    └── done/                  # Completed feature specs (15 docs)
```

Game integration layer:
```
src/sound/
├── sound_synth_bridge.h       # Public C API (SoundSynth struct + functions)
├── sound_synth_bridge.c       # Implementation (audio callback, jukebox: 15 entries)
├── sound_phrase.h             # Procedural phrase generation (bird, vowel, tone)
├── sound_phrase.c             # Phrase logic
└── songs.h                    # 18 built-in songs (all exported to .song via bridge_export)
```

## Architecture

### Top-Level Struct

```c
typedef struct SoundSystem {
    SynthContext   synth;       // 32 polyphonic voices + synthesis globals
    EffectsContext effects;     // per-bus effects + master chain + dub loop
    SequencerContext sequencer; // 64 patterns × 12 tracks × 32 steps
    SamplerContext sampler;     // 32 sample slots, 8 playback voices
} SoundSystem;
```

### Audio Flow

```
Sequencer (96 PPQ clock)
    │ triggers noteOn/noteOff callbacks
    ├── applyPatchToGlobals(SynthPatch)  ← patch_trigger.h
    v
Synth (32 voices, per-voice: oscillator → ADSR → SVF filter → LFO mod)
    │ + Sampler (WAV playback)
    v
Bus Mixer (8 buses: 4 drum + 3 melodic + 1 sampler, per-bus filter/dist/delay/EQ/chorus/phaser/comb/ring-mod/octaver/tremolo/wah/leslie/comp + reverb/delay sends)
    v
Master Effects (octaver → tremolo → wah → leslie → distortion → bitcrusher → chorus → flanger → phaser → comb → ring mod → tape → vinyl → delay → reverb → sidechain → multiband → EQ → sub-bass → compressor)
    │ + Performance FX: dub loop, rewind, tape stop, beat repeat, DJFX loop, half-speed
    v
Audio Output
```

### Global Context Pattern

Each subsystem has a global pointer (`synthCtx`, `fxCtx`, `seqCtx`, `samplerCtx`). Call `useSoundSystem(&ss)` to redirect all globals to a specific instance. This allows multiple independent instances.

### Preset System

`SynthPatch` is a stateless template (200+ `p_` fields). `applyPatchToGlobals()` copies patch → synth globals before note trigger. Drums and melodic instruments both use unified `SynthPatch`.

### Sequencer Tracks

- Tracks 0-3: Drum (step on/off triggers)
- Tracks 4-6: Melodic — bass, lead, chord (note value + slide/accent)
- Track 7: Sampler (TRACK_SAMPLER) — note field selects slice, one-shot playback
- Tracks 8-11: Extra melodic tracks (same step/note behavior as 4-6)
- Each track has independent step count (polyrhythmic), note pools, conditional triggers

## Key Patterns

### Adding a new preset
1. Add entry to `instrument_presets.h` in the appropriate category array
2. Set all `p_` fields on the `SynthPatch` struct
3. Test with `make preset-audition && ./build/bin/preset-audition <index>`

### Adding an effect parameter
1. Add field to `Effects` struct in `effects.h`
2. Wire it into `processEffects()` or `processEffectsWithBuses()`
3. Add UI control in `daw.c`
4. Add to `song_file.h` save/load and `daw_file.h` if applicable
5. Add test in `tests/test_soundsystem.c`

### Adding a sequencer feature
1. Modify `StepV2` or `Sequencer` in `sequencer.h`
2. Wire into `updateSequencer()` tick logic
3. Update `song_file.h` serialization
4. Add UI in `daw.c`
5. Add test in `tests/test_soundsystem.c`

## Big File Navigation (line ranges are approximate)

### synth.h (~2870 lines)
- 1-600: Types — WaveType enum, Voice struct, synthesis settings structs
- 600-950: SynthContext struct (all globals) + init + macros
- 950-1320: Helpers — noise, clampf, LFO, arpeggiator utils
- 1320: `#include "synth_oscillators.h"` (1550 lines — complex oscillator engines (basic waveforms — square/saw/triangle/sine/noise — are inline in synth.h))
- 1322-1920: processVoice() — per-voice DSP pipeline (envelope, filter, mixing)
- 1920-2100: Voice management — findVoice, releaseNote, initVoiceCommon
- 2100-2650: Note triggers — playNote, playPluck, playFM, etc.
- 2650-2870: SFX functions + `#include "synth_scale.h"` (114 lines — scale lock system)

### sequencer.h (~1900 lines)
- 1-300: Enums (conditions, pick modes, chord types), StepNote/StepV2 structs
- 300-460: Pattern, Sequencer, SequencerContext structs
- 460-800: initSequencer, pattern management, step editing helpers
- 800-1400: updateSequencer() — main tick logic, note triggers, p-locks
- 1400-1900: Rhythm generator, pattern chain, Dilla timing

### effects.h (~1516 lines)
- 1-190: Constants, enums
- 190-390: Effects, BusEffects, BusState, MixerContext, DubLoopParams, RewindParams structs
- 390-460: EffectsContext struct (all buffers)
- 460-660: initEffects, global context, macros
- 660-1000: processEffects() — main chain (dist → delay → tape → crush → reverb)
- 1000: `#include "dub_loop.h"` (310 lines) + `#include "rewind.h"` (165 lines)
- 1005-1516: Sidechain, master EQ/compressor, bus mixer, parameter setters

### daw.c (~4540 lines)
- 1-520: Includes, layout, tabs, state structs, DawState, debug state, globals
- 522: `#include "daw_widgets.h"` (250 lines — p-lock badge, wave thumbs, ADSR curve, filter XY, LFO preview)
- 524-800: Tab bar, narrow sidebar
- 800-930: Transport bar
- 930-1910: Workspace: Sequencer tab (grid, rhythm gen, step inspector)
- 1910-2310: Workspace: Piano roll tab
- 2310-2700: Workspace: Song tab
- 2700-3200: Params: Patch tab
- 3200-3365: Params: Bus FX tab
- 3365-3550: Params: Master FX tab
- 3550-3610: Params: Tape/Dub tab
- 3612: `#include "daw_audio.h"` (590 lines — audio callback, voice tracking, sequencer callbacks)
- 3615-4540: Debug panel, MIDI tab, Voice tab, MIDI keyboard input, main()

## Gotchas

- **Header-only**: All engines are `.h` files with inline implementations. Changes recompile everything that includes them.
- **`-Wno-unused-function`**: DAW and jukebox builds suppress this because header-only design creates many unused functions per translation unit.
- **Global context**: Forgetting `useSoundSystem()` before calling synth/effects functions will use stale pointers.
- **P-lock indices**: P-locks are stored in a flat `PLock[128]` array per pattern, keyed by (track, step, paramID). Don't assume index = step.
- **Song file format**: Text-based INI-style. Adding new fields requires updating both save and load in `song_file.h`, plus `daw_file.h` for DAW format.
- **Preset count shifts**: Adding/removing presets in `instrument_presets.h` shifts indices — song files reference presets by index.
- **scw_data.h is generated**: Don't edit directly. Regenerate with `make scw_embed`.

## Current Status

Waves 0-2 complete. Near-term TODO (per `docs/plan-of-attack.md`):
- ~~P-lock interpolation~~ (dropped — scenes/crossfader owns sweeps), ~~groove presets~~, ~~song settings UI~~, patch name editing
- Crossfader/scene system (spec in `docs/scene-crossfader-spec.md`)
- Performance: voice hot/cold split, fast sine approx, reverb comb power-of-2
- Test gaps: sampler WAV loading, patch trigger, rhythm patterns

### Recent additions
- Slow LFO sync divisions (8/16/32 bar — up to ~62s at 120 BPM)
- Per-LFO phase offset (0.0-1.0) — patches can run inverted relative to each other
- FM mod index LFO (5th LFO, shown for FM patches only)
- Real-time note recording: keyboard/MIDI → pattern with free/quantized modes, overdub/replace, gate tracking, pattern lock for song mode. F7 toggle, transport bar UI.
- WAVE_SINE oscillator: pure sine wave with unison support, completes the basic waveform set
- Per-oscillator decay on extra oscillators (osc2-6): independent exponential envelope per partial, enables MT-70 style additive timbres
- Semitone +/- buttons on extra osc ratio knobs (left-click = +1st, right-click = -1st)
- Velocity modulation system: 4 targets (osc level, filter, click, drive) with squared curve, cyan UI indicators
- Mono retrigger toggle (envelope restarts on every note vs legato glide)
- 263 presets (was 177): added mandolin (4: Neapolitan/Flatback/Bouzouki/Charango), whistle (4: Referee/Slide/Train/Cuckoo), plus earlier 909 kit, DX7 FM series, Wurlitzer, Clavinet, world instruments, SNES kit, pads, 10 Casio MT-70, 6 Minimoog, 4 OB-Xa, 3 Rhodes
- 8 buses (was 7): added BUS_SAMPLER for chop/flip slice playback
- Per-bus effects expanded: +EQ, chorus, phaser, comb filter (was: filter/dist/delay/reverb-send only)
- Master effects expanded: +chorus, flanger, phaser, comb, multiband, sub-bass boost, sidechain envelope

### Bridge → .song migration (COMPLETE)
17 songs are registered in `bridge_export.c` and exported to `demo/songs/`:
- dormitory, gymnopedie, mule2, suspense, jazz, dilla, atmosphere, mrlucky, happybirthday, monksmood, summertime, house, deephouse, oscarlofi, dreamer, saladdaze, emergence
- 4 songs (oscarlofi, dreamer, saladdaze, emergence) are exported but not yet wired into the jukebox
- M.U.L.E. v1 (original) is in jukebox but not in bridge_export (v2 MIDI version exported instead)
- 12+ DAW-authored .song files also exist (game_dawn, game_dusk, game_smoke, jojo, lekker, etc.)
- Done: Song name UI, `dawTextEdit()`, font consistency fixes
- TODO: Debug playback glitch in `gymnopedienew.song` (clipping at pattern 4/6)
- TODO: Scenes/crossfader for House + Deep House sweep songs
