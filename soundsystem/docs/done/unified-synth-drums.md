# Unified Synth + Drums Architecture

## Problem

We have two separate sound engines that can't talk to each other:

1. **synth.h** — 14 oscillator types, full ADSR/filter/LFO, ~80 params via `SynthPatch`
2. **drums.h** — 16 hardcoded drum voices, separate `DrumParams` struct, no presets

This causes:
- Drum sounds are invisible in the DAW UI (CR-78, toms, rimshot, cowbell, clave, maracas — all hidden)
- No drum presets or kit switching
- Drums can't be pitched (no piano roll support)
- Two separate code paths for sequencer playback
- Patch tab and Drums tab are disconnected — clicking a track goes to different places
- Membrane (tabla/conga) and Mallet (marimba/vibes) already live in synth.h but aren't used for drums

## Insight

Most drum sounds are just synth patches with short envelopes:

| Drum | Synth equivalent | What's missing |
|------|-----------------|----------------|
| Kick | Sine + pitch envelope | **Pitch envelope** |
| Tom | Same as kick, higher pitch | **Pitch envelope** |
| Snare | Oscillator + noise | **Noise mix layer** |
| Hihat | FM at metallic ratios | Close enough with FM |
| Clap | Multiple noise bursts | Multi-trigger (unique, keep as special case or approximate) |
| Rimshot | Short filtered click | Already doable |
| Cowbell | Two detuned squares | FM with specific ratio |
| Clave/Maracas | Filtered noise burst | Already doable |
| Tabla/Conga | WAVE_MEMBRANE | **Already in synth!** |
| Marimba/Vibes | WAVE_MALLET | **Already in synth!** |

## Plan: Extend SynthPatch (~20 drum-synthesis params)

### 1. Pitch Envelope (3 params) ✅ DONE

```c
float p_pitchEnvAmount;    // Semitones to sweep (e.g., +24 for kick punch)
float p_pitchEnvDecay;     // Decay time in seconds (e.g., 0.04 for kick)
float p_pitchEnvCurve;     // Curve shape (-1 to 1, negative = fast start)
```

### 2. Noise Mix Layer (4 params) ✅ DONE

```c
float p_noiseMix;          // 0 = pure oscillator, 1 = pure noise
float p_noiseTone;         // Noise LP filter cutoff (0 = dark, 1 = bright)
float p_noiseHP;           // Noise HP filter cutoff (0 = off, 1 = max highpass)
float p_noiseDecay;        // Separate decay for noise layer (0 = follows main env)
```

### 3. Retrigger (4 params) ✅ DONE

```c
int p_retriggerCount;      // 0 = off, 2-4 = rapid re-triggers
float p_retriggerSpread;   // Timing spread between triggers in seconds
bool p_retriggerOverlap;   // true = overlapping bursts (clap), false = envelope restart
float p_retriggerBurstDecay; // Per-burst decay time (0 = default 0.02s)
```

### 4. Extra Oscillators (10 params) ✅ DONE

6 oscillators total (main + osc2-6) for metallic hihat ratios.
Weighted-average mixing: `mixSum / totalWeight`.

```c
float p_osc2Ratio, p_osc2Level;  // through p_osc6Ratio, p_osc6Level
```

808 hihat ratios: `{1.0, 1.4471, 1.6170, 1.9265, 2.5028, 2.6637}`

### 5. Filter Types ✅ DONE

```c
int p_filterType;   // 0=SVF LP, 1=SVF HP, 2=SVF BP, 3=one-pole LP, 4=one-pole HP
```

One-pole modes (3,4) match drums.h 6dB/oct topology. SVF is 12dB/oct with resonance.
Filter bypass when cutoff≥1.0 && no resonance (avoids SVF charge-up killing transients).

### 6. Drive & Envelope Shapes ✅ DONE

```c
float p_drive;       // tanh saturation (kick warmth)
bool p_expRelease;   // exponential release (natural tail)
bool p_expDecay;     // exponential decay (punchy drums)
```

### 7. Drum Presets as SynthPatch ✅ DONE

16 drum presets in `instrument_presets.h` (indices 24-37), NUM_INSTRUMENT_PRESETS=40:

| Preset | Index | Wave | Key params |
|--------|-------|------|-----------|
| 808 Kick | 24 | FM(idx=0) | pitchEnv, drive=0.5, expDecay |
| 808 Snare | 25 | FM(idx=0)+osc2 | noiseMix 0.6, pitchEnv +8st |
| 808 Clap | 26 | Noise | retrigger 3×overlap, burstDecay=0.02 |
| 808 CH | 27 | Sine+5 oscs | 6 metallic ratios, one-pole HP, short decay |
| 808 OH | 28 | Sine+5 oscs | same ratios, longer decay |
| 808 Tom | 29 | Sine | pitchEnv +12st/0.06s |
| Rimshot | 30 | Square | pitchEnv +18st/0.01s, noiseMix 0.3 |
| Cowbell | 31 | FM | fmRatio 3.46, filterCutoff 0.4 |
| CR-78 Kick | 32 | Sine | pitchEnv +18st, drive=0.3 |
| CR-78 Snare | 33 | Sine | noiseMix 0.55, pitchEnv +6st |
| CR-78 HH | 34 | FM | SVF BP, fmRatio 6.7 |
| CR-78 Metal | 35 | FM | fmRatio 2.76 |
| Clave | 36 | Sine | pitchEnv +6st, short decay |
| Maracas | 37 | Noise | noiseMix 1.0, noiseHP 0.3 |

### Comparison Tool

`make drum-compare && ./build/bin/drum-compare [--export]`

Renders both engines and computes 3 similarity metrics (weighted 20/35/45%):
- Waveform: cross-correlation
- Spectral: 32-band DFT in perceptual frequency bands
- Envelope: 1ms RMS window correlation

Current scores (80.8% average):
- Best: clave 96%, snare 95%, maracas 94%, cr78_snare 92%
- Weak: clap 57%, cowbell 68% (architectural mismatches)

### Known Improvement Opportunities (TODO)
- Fix one-pole coefficient (remove squaring, use raw cutoff value)
- Maracas: use main filter path instead of noise mix for better match
- Non-uniform burst spacing for clap (`{0, spread, spread*2.2, spread*3.5}`)
- Per-burst noise re-seeding for clap
- CR-78 kick bandpass resonance (ringing bell-like decay)

## Benefits

- **One track type** — every sequencer track uses SynthPatch
- **Drums become melodic** — put a "kick" in the piano roll and it plays pitched
- **Fewer params shown** — UI already switches per wave type, drum presets only show relevant 5-6 params
- **Preset system works for everything** — same picker for synth and drum sounds
- **Simpler DAW code** — no separate Drums tab needed, Patch tab is context-aware
- **Song save/load simplified** — one format for all tracks

## Implementation Steps

### Phase 1: Synth engine + presets + comparison ✅ DONE
1. ✅ ~20 drum-synthesis params in SynthPatch (pitch env, noise mix, retrigger, extra oscs, filter types, drive, exp envelope)
2. ✅ All params wired through patch_trigger.h, synth.h processVoice(), initVoiceCommon()
3. ✅ 14 drum presets (808 kit + CR-78 kit + clave + maracas) in instrument_presets.h
4. ✅ drum_compare.c comparison tool with 3 metrics (waveform/spectral/envelope)
5. ✅ Demo UI exposes all new params (filter type, osc4-6, retrigger overlap, burst decay)
6. ✅ 80.8% average similarity across 14 drum sounds

### Phase 2: DAW integration — DONE (cleanup remains)
1. ✅ Drum sequencer rows trigger `playNoteWithPatch()` — `dawDrumTriggerGeneric()` in daw.c:3741
2. ✅ All 4 drum tracks use SynthPatch presets (indices 24-27), initialized in `initPatches()`
3. ✅ P-locks (DECAY, TONE, PUNCH, PITCH_OFFSET, VOLUME) work uniformly for drums and melody
4. ✅ Choke works via `p_choke` (closed hihat kills open hihat voice)
5. ✅ No Drums tab — only Patch/Bus FX/Master FX/Tape tabs exist. All drum params via Patch tab.
6. ✅ No DrumParams/DrumType/drums.h references in daw.c — fully SynthPatch-based
7. Track names: still hardcoded in 3 places (daw.c:385, daw.c:1222, sequencer.h:1269). Patches have `p_name`, just need to read it. (TODO, tiny)
8. Sequencer track types: TRACK_DRUM vs TRACK_MELODIC still distinguished. Drums use different callback (no gate/slide/accent, one-shot, Dilla per-instrument nudge). Debatable whether to unify — drums genuinely behave differently. (Open)

### Phase 3: Cleanup
1. ✅ Deleted drums.h (~1500 lines), prototype.c, drum_compare.c — all dead code removed
2. ✅ Removed DrumParams/DrumType from song_file.h, soundsystem.h, sound_synth_bridge.c
3. ✅ Removed drums.h tests from test_soundsystem.c (~591 lines)
4. Add more drum presets (909, Lo-Fi, Trap, Piku)
5. Expose sampler.h as another wave type option

## What we keep from drums.h

- ✅ Clap multi-burst → retrigger overlap mode with per-burst exponential decay
- ✅ Hihat metallic ratios → 6 extra oscillators (osc2-6) with exact 808 ratios
- ✅ CR-78 character → separate presets with different filter/envelope curves
- The drum voice allocation model (for polyphony management)

## Resolved Questions

- ✅ Clap: retrigger overlap mode with `burstTimers[8]`, independent exponential decays
- ✅ Filter topology: one-pole modes (3=LP, 4=HP) match drums.h 6dB/oct exactly
- ✅ Hihat: 6 oscillators with weighted-average mixing, not just FM
- Open: per-drum velocity curves? (existing volume param may suffice)
- Open: noise layer filter envelope? (currently follows main filter)
