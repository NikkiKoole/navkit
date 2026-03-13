# Soundsystem (PixelSynth)

Header-only synthesized audio engine (~36K lines). 16 synthesis engines, 32-voice polyphony, 111 presets, step sequencer, full effects chain, built-in DAW.

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
make test_soundsystem    # 248 tests, 745 assertions (sequencer, synth, effects, integration)

# Tools
make preset-audition     # headless preset → WAV renderer + spectral analysis
make song-render         # .song → WAV export
make daw-render          # headless DAW render (stubbed raylib)
make scw_embed           # WAV cycles → scw_data.h (wavetable generator)
make sample_embed        # WAV oneshots → sample_data.h (sample generator)
```

`test_soundsystem` is included in `make test`, `make test-full`, and `make test-quick`. To run **only** sound tests after sound changes: `make test_soundsystem`.

## File Structure

```
soundsystem/
├── soundsystem.h              # Main include (pulls in all engines)
├── engines/
│   ├── synth.h                # Synth engine: voice processing, triggers, SFX (~2870 lines)
│   ├── synth_oscillators.h   # 16 oscillator implementations (~1550 lines, extracted from synth.h)
│   ├── synth_scale.h          # Scale lock system (~114 lines, extracted from synth.h)
│   ├── synth_patch.h          # SynthPatch struct (200+ p_ fields)
│   ├── instrument_presets.h   # 111 presets (melodic + 35 drums) (~1850 lines)
│   ├── effects.h              # Effects chain + bus mixer + dub loop (~1970 lines)
│   ├── sequencer.h            # Step sequencer v2 (96 PPQ, 7 tracks) (~1900 lines)
│   ├── patch_trigger.h        # SynthPatch → synth globals applicator
│   ├── sampler.h              # Sample playback (8-voice, 32 slots)
│   ├── song_file.h            # .song/.patch/.bank file I/O
│   ├── midi_input.h           # CoreMIDI input (macOS)
│   ├── rhythm_patterns.h      # 14 rhythm style generators
│   ├── rhythm_prob_maps.h     # 27 probability map presets
│   ├── piku_presets.h         # Pikuniku-style presets
│   ├── scw_data.h             # Generated wavetable data (from scw_embed)
│   └── sample_data.h          # Generated sample data (from sample_embed)
├── demo/
│   ├── daw.c                  # Full DAW UI (~250KB, 5 tabs)
│   └── daw_file.h             # DAW save/load (.daw format)
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
├── sound_synth_bridge.c       # Implementation (audio callback, jukebox)
├── sound_phrase.h             # Procedural phrase generation (bird, vowel, tone)
├── sound_phrase.c             # Phrase logic
└── songs.h                    # 14 built-in songs
```

## Architecture

### Top-Level Struct

```c
typedef struct SoundSystem {
    SynthContext   synth;       // 32 polyphonic voices + synthesis globals
    EffectsContext effects;     // per-bus effects + master chain + dub loop
    SequencerContext sequencer; // 8 patterns × 12 tracks × 32 steps
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
Bus Mixer (7 buses: 4 drum + 3 melodic, per-bus filter/dist/delay/reverb-send)
    v
Master Effects (distortion → delay → tape → bitcrusher → reverb → sidechain → EQ → compressor)
    │ + Dub Loop (King Tubby tape delay) + Rewind (vinyl spinback)
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
- 1320: `#include "synth_oscillators.h"` (1550 lines — all 16 oscillator process/init functions)
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

### effects.h (~1970 lines)
- 1-190: Constants, enums
- 190-390: Effects, BusEffects, BusState, MixerContext structs
- 390-460: EffectsContext struct (all buffers)
- 460-800: initEffects, bus processing
- 800-1400: processEffects() — main chain (dist → delay → tape → crush → reverb)
- 1400-1700: Dub loop (capture, playback heads, degradation)
- 1700-1970: Rewind effect, sidechain, master EQ/compressor

### daw.c (~250KB) — too large for full reads
- Search by function name or use `sg run -p 'functionName' -l c soundsystem/demo/`
- 5 workspace tabs: Sequencer grid, Piano roll, Song chain, MIDI viz, Voice diags

## Gotchas

- **Header-only**: All engines are `.h` files with inline implementations. Changes recompile everything that includes them.
- **`-Wno-unused-function`**: DAW and jukebox builds suppress this because header-only design creates many unused functions per translation unit.
- **Global context**: Forgetting `useSoundSystem()` before calling synth/effects functions will use stale pointers.
- **P-lock indices**: P-locks are stored in a flat `PLock[128]` array per pattern, keyed by (track, step, paramID). Don't assume index = step.
- **Song file format**: Text-based INI-style. Adding new fields requires updating both save and load in `song_file.h`, plus `daw_file.h` for DAW format.
- **Preset count shifts**: Adding/removing presets in `instrument_presets.h` shifts indices — song files reference presets by index.
- **scw_data.h / sample_data.h are generated**: Don't edit directly. Regenerate with `make scw_embed` or `make sample_embed`.

## Current Status

Waves 0-2 complete. Near-term TODO (per `docs/plan-of-attack.md`):
- P-lock interpolation, groove presets, song settings UI, patch name editing
- ~20 melodic presets (Wurlitzer, Clavinet, bass variants, guitar, pads, SNES kit)
- More drum presets (909, Lo-Fi, Trap)
- Crossfader/scene system (spec in `docs/scene-crossfader-spec.md`)
- Performance: voice hot/cold split, fast sine approx, reverb comb power-of-2
- Test gaps: sampler WAV loading, patch trigger, rhythm patterns
