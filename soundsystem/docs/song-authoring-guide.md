# Song Authoring Guide (for AI agents)

How to write songs in the PixelSynth soundsystem, either as C code in `songs.h` (bridge) or as `.song` files via the DAW/bridge-export tool.

## Architecture Overview

```
Engine capacity: 64 patterns × 12 tracks × 32 steps
├── Tracks 0-3: Drums (kick, snare, hihat, clap/perc)
├── Tracks 4-6: Bass/Lead/Chord (melodic)
├── Track 7: Sampler (slice playback)
└── Tracks 8-11: Extra melodic tracks (same behavior as 4-6)

Bridge songs in `songs.h` currently use tracks 0-6 only.
```

Each track has an assigned **SynthPatch** (preset) from `instrument_presets.h` (263 presets: melodic, drums, FM, physical models, chip sounds). Drum tracks trigger one-shot hits; melody tracks play pitched notes with gate, slide, accent.

## Writing Songs in C (songs.h)

Songs are authored as static functions in `src/sound/songs.h`. The pattern is:

### Helper functions (defined at top of songs.h)

```c
// Melody note: track 0=bass, 1=lead, 2=chord
note(p, track, step, midiNote, velocity, gate);     // basic
noteN(p, track, step, note, vel, gate, nudge);       // + timing nudge
noteS(p, track, step, note, vel, gate, sustain);     // + extra sustain steps
noteNS(p, track, step, note, vel, gate, nudge, sus); // + both

// Chords
chord(p, track, step, root, CHORD_TRIAD, vel, gate);
chordCustom(p, track, step, vel, gate, n0, n1, n2, n3);  // -1 = unused

// Drums
drum(p, track, step, velocity);       // track 0=kick, 1=snare, 2=HH, 3=clap
drumN(p, track, step, vel, nudge);    // + timing nudge
drumP(p, track, step, vel, pitch);    // + pitch offset
```

### MIDI note defines

```c
// Octave 2-5 defined as macros: Cn2=36, D2=38, E2=40, F2=41, etc.
// Flats: Eb2=39, Bb2=46, Ab2=44
// Sharps: Cs2=37, Fs2=42, Gs2=44
// REST = -1
```

### Song structure

```c
// 1. ConfigureVoices — set scale, master volume (called before playback)
static void Song_MySong_ConfigureVoices(void) {
    masterVolume = 0.30f;
    scaleLockEnabled = true;
    scaleRoot = 2;                    // D
    scaleType = SCALE_DORIAN;        // see ScaleType enum
}

// 2. Pattern functions — fill step data
static void Song_MySong_PatternA(Pattern* p) {
    initPattern(p);
    // Optional: set per-track step count for polyrhythm or time signatures
    // patSetDrumLength(p, track, length);    // drum track 0-3
    // patSetMelodyLength(p, SEQ_DRUM_TRACKS + track, length);  // melody 0-2

    // Drums
    drum(p, 0, 0, 0.5);   // kick on step 0
    drum(p, 0, 8, 0.5);   // kick on step 8
    drum(p, 1, 4, 0.4);   // snare on step 4
    drum(p, 1, 12, 0.4);  // snare on step 12

    // Bass
    note(p, 0, 0, D2, 0.4, 4);   // D2, vel 0.4, gate 4 steps

    // Lead
    note(p, 1, 2, A4, 0.35, 2);  // A4 on step 2

    // Chords
    chord(p, 2, 0, D3, CHORD_TRIAD, 0.25, 8);
}

// 3. Load function — fills pattern array
static void Song_MySong_Load(Pattern patterns[2]) {
    Song_MySong_PatternA(&patterns[0]);
    Song_MySong_PatternB(&patterns[1]);
}

#define SONG_MYSONG_BPM  100.0f
```

### Registering in the bridge

In `sound_synth_bridge.c`:
1. Create melody trigger callbacks (or use existing preset-based ones)
2. Add a `SoundSynthPlaySongXxx()` function
3. Add to `jukeboxSongs[]` table

### Exporting to .song

Add entry to `bridgeSongs[]` in `soundsystem/tools/bridge_export.c`:
```c
{
    .name = "mysong",
    .bpm = 100.0f,
    .patternCount = 2,
    .loopsPerPattern = 2,
    .drumPresets = {24, 25, 27, 26},    // 808 kick/snare/CH/clap
    .melodyPresets = {44, 15, 42},       // Warm Pluck, Piku Glock, Dark Choir
    .configureMelody = mySongOverrides,  // NULL if no overrides needed
    .loadPatterns = mySongLoadPatterns,
    .scaleLock = true, .scaleRoot = 2, .scaleType = 6,  // D dorian
    .swing = 3, .jitter = 1,
    .timingJitter = 1, .velocityJitter = 0.06f,
},
```
Then: `make bridge-export && ./build/bin/bridge-export mysong`

## Key Features for Interesting Songs

### 1. Step count and time signatures (most important for non-4/4)

Default is 16 steps (16th notes at the BPM). But you can:
- **12 steps** for 3/4 waltz (Gymnopedie uses 12 at half BPM for 8th note resolution)
- **32 steps** for 32nd note resolution (M.U.L.E. uses this for grace notes)
- **Per-track lengths** for polyrhythm: `patSetDrumLength(p, 2, 12)` makes hihat loop every 12 while kick loops every 16

### 2. Groove and swing

```c
seq.dilla.swing = 7;       // 0-15, how much even 16ths nudge toward odd
seq.dilla.snareDelay = 3;  // snare laziness (ticks late)
seq.dilla.jitter = 2;      // random timing variance
seq.humanize.timingJitter = 2;     // melody timing humanize
seq.humanize.velocityJitter = 0.08f; // velocity humanize
```

Named groove presets available: Straight, Light Swing, MPC Swing, Hard Swing, Dilla, Jazz, Bossa, Hip Hop, Reggae, Funk, Loose.

### 3. Velocity dynamics

Velocity (0.0-1.0) controls volume AND character. Use it musically:
- Ghost notes: vel 0.08-0.15 (barely audible ticks)
- Normal: vel 0.3-0.5
- Accents: vel 0.6-0.8
- Sforzando: vel 0.9-1.0

### 4. Gate length

Gate = how many steps the note rings. Critical for feel:
- Staccato: gate 1
- Normal: gate 2-4
- Legato: gate 8-16
- Combine with `patSetNoteSustain()` for extra hold beyond gate

### 5. Slide and accent

```c
patSetNoteSlide(p, SEQ_DRUM_TRACKS + track, step, true);  // portamento
patSetNoteAccent(p, SEQ_DRUM_TRACKS + track, step, true);  // accent boost
```
Slide uses the patch's `p_glideTime`. Accent boosts volume × 1.3 and filter env + 0.2.

### 6. P-locks (per-step parameter locks)

Elektron-style: override any parameter on a specific step.

```c
seqSetPLock(p, track, step, PLOCK_TIME_NUDGE, -3.0f);  // push 3 ticks early
seqSetPLock(p, track, step, PLOCK_FILTER_CUTOFF, 0.8f); // bright on this step
seqSetPLock(p, track, step, PLOCK_FLAM_TIME, 25.0f);    // 25ms flam
seqSetPLock(p, track, step, PLOCK_FLAM_VELOCITY, 0.4f); // ghost note at 40%
```

Available p-lock params: `PLOCK_FILTER_CUTOFF`, `PLOCK_FILTER_RESO`, `PLOCK_FILTER_ENV`, `PLOCK_TIME_NUDGE`, `PLOCK_FLAM_TIME`, `PLOCK_FLAM_VELOCITY`.

### 7. Conditional triggers

Steps can fire conditionally — great for evolving patterns:

```c
patSetDrumCond(p, track, step, COND_1_2);    // every 2nd loop
patSetNoteCond(p, track, step, COND_3_4);    // 3rd of every 4 loops
```

Conditions: `COND_ALWAYS`, `COND_1_2`, `COND_2_2`, `COND_1_4`..`COND_4_4`, `COND_FILL`, `COND_NOT_FILL`, `COND_FIRST`, `COND_NOT_FIRST`.

### 8. Probability

```c
patSetDrumProb(p, track, step, 0.4f);   // 40% chance of triggering
patSetNoteProb(p, track, step, 0.7f);   // 70% chance
```

### 9. Note pools (generative variation)

A step can hold multiple candidate notes; a pick mode selects which plays each trigger:

```c
// Fill step with chord notes, then set pick mode to rotate through them
patFillPool(p, track, step, rootNote, CHORD_TRIAD, vel, gate);
patSetPickMode(p, track, step, PICK_CYCLE_UP);
```

Pick modes: `PICK_ALL` (chord), `PICK_RANDOM`, `PICK_CYCLE_UP`, `PICK_CYCLE_DOWN`, `PICK_PINGPONG`, `PICK_RANDOM_WALK`.

### 10. Chords

```c
chord(p, 2, 0, D3, CHORD_TRIAD, 0.25, 8);     // scale-aware triad
chord(p, 2, 0, D3, CHORD_SEVENTH, 0.20, 8);    // 7th chord
chordCustom(p, 2, 0, 0.22, 4, B3, D4, Fs4, -1); // explicit voicing
```

Types: `CHORD_SINGLE`, `CHORD_FIFTH`, `CHORD_TRIAD`, `CHORD_TRIAD_INV1`, `CHORD_TRIAD_INV2`, `CHORD_SEVENTH`, `CHORD_OCTAVE`, `CHORD_OCTAVES`, `CHORD_CUSTOM`.

### 11. Slow LFOs for long sweeps

Instead of per-trigger parameter hacks, use slow free-rate LFOs on the SynthPatch:

```c
patch.p_filterLfoRate = 0.025f;           // ~40s sine cycle
patch.p_filterLfoDepth = 0.285f;          // ±0.285 cutoff modulation
patch.p_filterLfoShape = 0;               // 0=sine
patch.p_filterLfoPhaseOffset = 0.5f;      // inverted (for counter-motion)

patch.p_resoLfoRate = 0.025f;             // resonance sweep
patch.p_resoLfoDepth = 0.075f;

patch.p_fmLfoRate = 0.025f;              // FM brightness sweep
patch.p_fmLfoDepth = 1.25f;
```

LFO shapes: 0=sine, 1=triangle, 2=square, 3=saw, 4=S&H.
Tempo sync: `LFO_SYNC_8_1` (8 bars), `LFO_SYNC_16_1` (16 bars), `LFO_SYNC_32_1` (32 bars) — or use free rate in Hz for exact timing.

### 12. Scale lock

Constrains notes to a scale. Set on the song, affects all melody tracks:

```c
scaleLockEnabled = true;
scaleRoot = 0;  // C=0, C#=1, D=2, ... B=11
scaleType = SCALE_MINOR;
```

Scale types: `SCALE_CHROMATIC`, `SCALE_MAJOR`, `SCALE_MINOR`, `SCALE_PENTATONIC`, `SCALE_MINOR_PENTA`, `SCALE_BLUES`, `SCALE_DORIAN`, `SCALE_MIXOLYDIAN`, `SCALE_HARMONIC_MIN`.

## Preset Reference (commonly used)

| Index | Name | Wave | Character |
|-------|------|------|-----------|
| 0 | Chip Lead | Square | Classic chiptune lead |
| 4 | 8bit Pluck | Square | Short percussive |
| 8 | Acid | Saw | 303-style with filter |
| 15 | Piku Glock | Mallet | Toy glockenspiel |
| 21 | Mac Keys | FM | Rhodes-like electric piano |
| 23 | Mac Vibes | FM | Vibraphone |
| 24-29 | 808 Kit | Various | Kick, Snare, Clap, CH, OH, Tom |
| 38 | Warm Tri Bass | Triangle | Soft sub-bass |
| 39 | Dark Organ | Additive | Organ drone |
| 40 | Eerie Vowel | Voice | Breathy vowel |
| 41 | Tri Sub | Triangle | Pure sub-bass |
| 42 | Dark Choir | Additive | Choir pad |
| 43 | Lush Strings | Saw | String pad |
| 44 | Warm Pluck | Pluck | Upright bass feel |
| 140-145 | 909 Kit | Various | Kick, Snare, Clap, CH, OH, Rim |
| 146 | Chip Square | Square | NES/C64 bass |
| 147 | Chip Saw | Saw | NES/C64 lead |

Full list: 263 presets. Run `make preset-audition && ./build/bin/preset-audition --all` to hear them all.

## Song Design Patterns

### Build-up arrangement
For 8-pattern songs: 0-1 drums only → 2-3 add bass → 4-5 add melody → 6-7 full + variation. The M.U.L.E. songs use this layered build structure across 8 patterns.

### Variation via conditional triggers
Put fills and ghost notes with `COND_1_4` or `COND_FILL` so the pattern evolves without needing extra patterns.

### Jazz/waltz feel
Use 12-step patterns (3/4 time), swing groove, humanize timing+velocity, slide between notes.

### Chip/retro
32-step resolution for grace notes, no swing, no humanize, Chip Square/Saw presets, tight gate lengths.

### Ambient/atmospheric
Slow BPM (48-60), long gate lengths, pad presets with slow attack/release, sparse drums with probability, scale lock to avoid dissonance.

### Sweep/evolving
Slow LFOs (0.01-0.03 Hz) on filter cutoff and FM modIndex. Use phase offset 0.5 on pads for counter-motion against bass sweeps.

## File Locations

- Song definitions: `src/sound/songs.h`
- Bridge (game playback): `src/sound/sound_synth_bridge.c`
- Export tool: `soundsystem/tools/bridge_export.c`
- Exported songs: `soundsystem/demo/songs/*.song`
- Presets: `soundsystem/engines/instrument_presets.h`
- Sequencer API: `soundsystem/engines/sequencer.h`
- Synth patch fields: `soundsystem/engines/synth_patch.h`
- DAW (for editing .song files): `make soundsystem-daw && ./build/bin/soundsystem-daw`
