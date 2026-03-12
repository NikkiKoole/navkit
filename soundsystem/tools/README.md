# Soundsystem Tools

Standalone utilities for testing, analyzing, and building assets for the PixelSynth sound engine.

## Build

All C tools build from the **navkit root** via Make targets:

```bash
make preset-audition    # Preset renderer + analyzer
make drum-compare       # Legacy drums.h vs synth preset comparison
make jukebox-test       # Song/jukebox playback tester
make sample_embed       # WAV → C header embedder (samples)
make scw_embed          # WAV → C header embedder (single-cycle waveforms)
```

Python/Lua tools run directly (no build step).

---

## preset_audition.c

Headless preset renderer and analyzer. Renders any of the 111 instrument presets to WAV and reports audio metrics. Can compare a preset against a reference WAV file to suggest parameter improvements.

### Basic usage

```bash
# Render preset 45 (Glockenspiel) at default note (C4), write WAV
./build/bin/preset-audition 45

# Render at a specific MIDI note (84 = C6)
./build/bin/preset-audition 45 -n 84

# Render all presets, print summary table
./build/bin/preset-audition --all

# Render multiple specific presets
./build/bin/preset-audition --multi 24,25,30,45
```

### Reference comparison (--ref)

Compare a rendered preset against a real-world WAV sample. This is the main workflow for improving preset quality — find a good reference sound, run the comparison, and follow the suggestions.

```bash
# Compare our Glockenspiel (preset 45, MIDI 84) against a GarageBand sample
./build/bin/preset-audition 45 -n 84 \
  --ref "/Library/Application Support/GarageBand/Instrument Library/Sampler/Sampler Files/Glockenspiel/Glockenspiel_Pla_mf1/GLS2_Pla_mf_C6.wav"
```

Output includes:
- **Per-sound analysis**: peak/RMS levels, attack/decay/sustain, pitch detection, spectral brightness, harmonics, noise content
- **Side-by-side comparison**: overall similarity %, waveform/spectrum/envelope sub-scores
- **Parameter suggestions**: concrete synth parameter changes (e.g. "raise filterCutoff", "lengthen p_decay")

### CSV export (--csv)

```bash
# Export analysis data as CSV files to a directory
./build/bin/preset-audition 45 -n 84 --ref reference.wav --csv ./analysis_output/
```

### Tips

- Match the MIDI note (`-n`) to the reference sample's pitch for meaningful comparison
- Waveform similarity will be low even for good matches — focus on spectrum and envelope scores
- The "odd/even ratio" metric can be extreme for simple waveforms with few partials — weight it less for basic patches
- Reference samples with reverb will show higher noise % and broader spectral rolloff; the tool auto-detects this

---

## wav_analyze.h

Header-only C library for WAV audio analysis. Used internally by `preset_audition.c` but can be included in any C file.

```c
#define WAV_ANALYZE_IMPLEMENTATION
#include "wav_analyze.h"
```

### Features

- **WAV I/O**: load/write 16-bit WAV files (mono/stereo, any sample rate)
- **Spectral analysis**: 32-band perceptual energy, centroid, rolloff, flatness
- **Temporal analysis**: attack time, decay time, sustain level, transient sharpness
- **Pitch detection**: autocorrelation with confidence scoring
- **Harmonic analysis**: partial detection, odd/even ratio, inharmonicity, fundamental strength
- **Comparison**: weighted similarity scoring between two analyses
- **CSV export**: per-sample and comparison data for external visualization

### Key functions

| Function | Description |
|---|---|
| `waLoadWav(path, &buf, &len, &sr, &bits, &ch)` | Load WAV file into float buffer |
| `waWriteWav(path, buf, len, sr)` | Write float buffer as 16-bit mono WAV |
| `waAnalyze(buf, len, sr)` | Full analysis → `WaAnalysis` struct |
| `waCompare(&a, &b)` | Compare two analyses → `WaComparison` struct |
| `waPrintAnalysis(&a, label)` | Print formatted analysis to stdout |
| `waPrintComparison(&c, nameA, nameB)` | Print comparison + suggestions |
| `waExportCSV(path, buf, len, &a)` | Export per-sample analysis CSV |
| `waExportCompareCSV(path, &a, &b, &c)` | Export comparison CSV |

---

## drum_compare.c

Compares the legacy `drums.h` engine output against equivalent synth preset output. Used during the drum unification effort to verify that synth presets match the old dedicated drum engine.

```bash
./build/bin/drum-compare            # Print similarity table
./build/bin/drum-compare --export   # Also write CSV waveforms to soundsystem/tools/waves/
```

Reports cross-correlation, spectral similarity, and envelope similarity for each drum type.

> **Note**: `drums.h` is deprecated. All drums now use SynthPatch presets. This tool is mainly useful as a historical reference.

---

## jukebox_test.c

Interactive song playback tester. Plays songs through the SoundSynth bridge without running the full game.

```bash
./build/bin/jukebox-test              # Start with song picker
./build/bin/jukebox-test 3            # Start playing song index 3
./build/bin/jukebox-test --list       # List all available songs
./build/bin/jukebox-test --headless 3 10   # Play song 3 for 10 seconds (no window)
```

**Controls** (interactive mode): LEFT/RIGHT = prev/next song, SPACE = stop, L = toggle log, Q/ESC = quit

---

## sample_embed.c

Scans a directory of `.wav` files and generates a C header with embedded sample data for the sampler engine.

```bash
clang -o build/bin/sample_embed soundsystem/tools/sample_embed.c
./build/bin/sample_embed path/to/samples/ > sample_data.h
```

---

## scw_embed.c

Scans a directory of `.wav` files containing single-cycle waveforms and generates a C header with embedded waveform data.

```bash
clang -o build/bin/scw_embed soundsystem/tools/scw_embed.c
./build/bin/scw_embed path/to/cycles/ > scw_data.h
```

---

## midi_to_song.py

Parses a MIDI file and dumps note events in a format ready for transcribing into `songs.h` pattern functions.

```bash
python3 midi_to_song.py song.mid                # Default 1/128th resolution
python3 midi_to_song.py song.mid --step-size 16  # 1/64th resolution
```

Shows per-track note events with step positions, MIDI notes, gate durations, and velocities. Flags sub-step timing for p-lock nudge use.

**Requires**: `pip install mido`

---

## midi_compare.py

Compares a MIDI file against reference song data to verify pitch, timing, and gate accuracy.

```bash
python3 midi_compare.py song.mid --track 2
python3 midi_compare.py song.mid --track 2 --song-data reference.json
```

Reports timing, pitch, and gate differences between the MIDI file and reference data.

**Requires**: `pip install mido`

---

## generate_prob_maps.lua

Reads ~645 real drum patterns from a Lua data file, groups by genre, computes per-step hit probabilities, and outputs C `RhythmProbMap` structs for the rhythm pattern engine.

```bash
lua generate_prob_maps.lua path/to/drum-patterns.lua --c       # C struct output
lua generate_prob_maps.lua path/to/drum-patterns.lua --csv      # CSV output
lua generate_prob_maps.lua path/to/drum-patterns.lua --preview  # ASCII preview
```

Maps instruments to 4 tracks (kick, snare, hihat, perc) and handles accent boosting.
