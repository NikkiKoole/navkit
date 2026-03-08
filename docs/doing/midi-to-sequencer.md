# MIDI to Sequencer Conversion — Working Notes

## Architecture

Our sequencer: configurable step resolution per song.
- **16th note mode** (default): 16 steps/bar, 4 steps/beat, `ticksPerStep=24` (SEQ_PPQ=96)
- **32nd note mode**: 32 steps/bar, 8 steps/beat, `ticksPerStep=12` — enabled via `seqSet32ndNoteMode(true)`
- SEQ_MAX_STEPS = 32 (compile-time max for array sizes; trackLength controls actual steps per pattern)
- 4 drum tracks (kick, snare, hihat, clap)
- 3 melody tracks (bass, lead, chords)
- Per-step nudge via p-locks (`PLOCK_TIME_NUDGE`, ±half a step in ticks)
- MAX_SONG_PATTERNS = 8
- Chord track supports polyphonic playback via `PICK_ALL` + `MelodyChordTriggerFunc`
- Note pools: CHORD_TRIAD (3 notes), CHORD_SEVENTH (4 notes) — standard voicings only
- Multi-voice tracking: `melodyVoices[track][MAX_CHORD_VOICES]` (up to 6 simultaneous voices per track)

## Step/Nudge Mapping

Given a beat position (float, 0-based within a bar):
```
# 16th note mode (steps_per_beat=4, ticks_per_step=24):
step = round(beat * 4)
nudge_ticks = round((beat - step/4.0) * 96)  // clamp to ±12
gate = max(1, round(duration_beats * 4))

# 32nd note mode (steps_per_beat=8, ticks_per_step=12):
step = round(beat * 8)
nudge_ticks = round((beat - step/8.0) * 96)  // clamp to ±6
gate = max(1, round(duration_beats * 8))

velocity = midi_vel / 127.0
```

16th mode: 24 ticks/step, ±12 ticks nudge. 32nd mode: 12 ticks/step, ±6 ticks nudge.

## What Works

- **Per-step nudge on melody tracks** — lets us preserve MIDI's off-grid timing (Monk's rubato, jazz feel). Added `calcMelodyTriggerTick()` in sequencer.h. Nudge is melody-only (no swing/jitter — those are drum-feel concepts).
- **Velocity per step** — MIDI velocity maps directly to melodyVelocity float
- **Gate length** — duration in steps, carries across pattern boundaries (the gate countdown ticks regardless of pattern changes)
- **Slide flag** — can mark grace notes / portamento
- **8 patterns** — enough for an 8-bar section at 16th note resolution, or 16 bars at 8th note resolution
- **Polyphonic chords via PICK_ALL** — `MelodyChordTriggerFunc` callback receives all chord notes at once. `melodyTriggerChordFMKeys` plays them with finger stagger (offset attack envelopes, +12ms per note) for natural rolled-chord feel. Uses `melodyVoices[track][MAX_CHORD_VOICES]` for multi-voice tracking.
- **Chord-aware release** — `melodyReleaseAllVoices(track)` releases all voices in a track's array, not just index 0.

## What's Missing / Limitations

### Timing
- **32nd note resolution available** via `seqSet32ndNoteMode(true)` + `trackLength=32`. Needed for songs with grace note stutters (M.U.L.E.) or fast ornaments that fall between 16th note grid lines. At 32nd resolution: 32 steps/bar, 12 ticks/step, nudge range ±6 ticks (±half a 32nd step). Use 16th (default) when 32nd is overkill — most jazz/house/ambient sits fine on 16ths.
- **No rubato / tempo changes**. MIDI tempo maps are ignored — we use a fixed BPM. For ballads like Monk's Mood this flattens the phrasing somewhat.
- **Swing only applies to drums**, not melody. This is by design (swing is a drum feel, nudge is for melodic expression).

### Polyphony
- **Standard chord shapes only**. `buildChordNotes` generates triads/sevenths from a root using major/minor interval formulas. Complex jazz voicings (Monk's tritone subs, whole-tone clusters, spread voicings) cannot be specified exactly — they get approximated to the nearest standard chord type. Could add: custom note arrays per step for exact voicings.
- **No sustain pedal (CC64)**. Notes end when gate expires. Jazz piano relies heavily on pedal for overlapping voicings. Could add: a "sustain" flag per step that extends gate until next note.
- **`shouldUseMinor` auto-detection** — the sequencer guesses major/minor based on the root note. This works for diatonic music but can choose wrong for chromatic jazz harmony. Workaround: accept the approximation or pick a root that produces closer intervals.

### Sound
- **No per-track instrument switching mid-song**. Each melody track has one instrument callback. MIDI tracks often change patches.
- **Drum mapping is simplified**. MIDI uses GM drum map (note numbers = specific drums). Our sequencer has 4 tracks: kick/snare/hihat/clap. Ride cymbal (MIDI 51), brushes, etc. need mapping to closest equivalent.
- **No dynamics beyond velocity**. No crescendo/decrescendo curves, no CC1 (mod wheel), no CC11 (expression).

### Structure
- **8 patterns max** for a song. A standard jazz tune is AABA = 32 bars. At 16th-note resolution (1 bar/pattern), we can only encode 8 bars. Options:
  - Use 8th-note resolution for simpler melodies (2 bars/pattern = 16 bars)
  - Encode just the A section and let it loop
  - Bump MAX_SONG_PATTERNS further for longer forms

## MIDI Drum Note Mapping

```
GM MIDI     →  Our Track
35 (Kick)   →  Track 0 (Kick)
37 (Stick)  →  Track 1 (Snare) — sidestick
38 (Snare)  →  Track 1 (Snare)
42 (HH cl)  →  Track 2 (HiHat)
46 (HH op)  →  Track 2 (HiHat) — higher velocity
51 (Ride)   →  Track 2 (HiHat) — approximate
49 (Crash)  →  Track 3 (Clap) — approximate
```

## Tools

- **Python `mido` library** (`pip3 install mido`) — parses MIDI files, gives per-track events with delta times. Division = ticks per beat. Use `mido.MidiFile(path)` → iterate tracks → accumulate absolute ticks.
- Extract notes by tracking `note_on`/`note_off` pairs with `pending` dict for start tick + velocity.

## Conversion Workflow

1. Parse MIDI with Python `mido` — identify tracks (melody, bass, piano, drums)
2. Determine first bar of content (MIDI often has 1-2 bar intro/pickup)
3. For each bar: quantize to 16 steps, compute nudge values
4. Piano chords: group simultaneous notes (within 10 ticks), pick root, identify closest chord type
5. Filter bass ghost notes (< 0.15 beats AND vel < 50)
6. Map drum hits: GM note numbers → our 4 drum tracks (kick/snare/hihat/clap)
7. Generate C code for Pattern functions
8. Wire up: ConfigureVoices, Load, Play function, jukebox entry, header declaration

## Songs Converted

| Song | Key | Time | BPM | Patterns | Resolution | Notes |
|------|-----|------|-----|----------|------------|-------|
| Happy Birthday | F maj | 3/4 | 55 (=110 actual) | 4 | 8th note | Track length 12 for waltz |
| Monk's Mood | C (chromatic) | 4/4 | 75 | 8 | 16th note | A section only, heavy nudge, CHORD_CUSTOM voicings, sustain |
| Summertime | Am | 4/4 | 120 | 8 | 16th note | Walking bass, CHORD_CUSTOM jazz voicings, sustain, lazy swing |
| M.U.L.E. Theme | F min | 4/4 | 74 | 8 | 32nd note | 8-bit, no humanize/swing, chromatic octave bass, first song using 32nd mode |
| Gymnopédie No.1 | D maj | 3/4 | 40 (=80 actual) | 8 | 8th note | Track length 12 for waltz, CHORD_CUSTOM triads, bars 0-15 (intro+A+A'), long release |

## Improvement Priority List

### 1. Exact Chord Voicings ✅ DONE
`CHORD_CUSTOM` type + `customNotes[]`/`customNoteCount` on NotePool. `seqGetAllNotesForStep` returns exact notes. Works with both PICK_ALL (all notes) and cycling modes (pick from custom pool). 7 tests.

### 2. Sustain Pedal ✅ DONE
`melodySustain[track][step]` per-step flag. When gate expires and sustain is active, note keeps ringing until next note-on (which releases it). `releaseAllMelodyNotes()` unconditionally releases on stop/reset — no stuck notes. 7 tests.

### 3. Tempo Changes / Rubato (MEDIUM)
MIDI tempo maps ignored — fixed BPM. Ballads have real tempo breathing that we flatten.

### 4. Dynamics Beyond Velocity (LOW)
No CC1 (mod wheel), CC11 (expression), no crescendo/decrescendo curves.

### 5. More Than 8 Patterns (MEDIUM)
Full jazz tune AABA = 32 bars needs 32 patterns at 16th-note resolution. We can only do 8 bars (one A section).

### 6. Per-Track Instrument Switching (LOW)
Each track has one instrument callback. MIDI tracks often change patches mid-song.

### 7. Better Drum Mapping (LOW)
4 drum tracks (kick/snare/hihat/clap) vs GM MIDI's ~46 percussion sounds. Ride, brushes, toms all approximate.

## Tips

- **3/4 time**: Set track lengths to 12 (4 steps/beat × 3 beats). Use BPM÷2 for 8th-note resolution.
- **Held notes across bars**: Give large gate (e.g., 16). Gate countdown crosses pattern boundaries naturally.
- **Grace notes / turns**: Use consecutive steps with gate=1 and nudge to place precisely. Monk's Mood uses this for ornaments (e.g., B3→C4 grace note at bar 6).
- **Bass ghost notes**: Filter MIDI bass notes < 0.15 beats AND vel < 50 (often slap/ghost artifacts, especially common in walking bass MIDIs).
- **Walking bass**: Usually one note per beat → steps 0, 4, 8, 12 with gate 3-4. Bass is generally on-grid (nudge=0).
- **Brush drums**: Map ride/brush patterns to hihat track with low velocity.
- **Jazz ride pattern**: Ride cymbal on every beat + upbeats (the "skip" beat). Ride maps to hihat track (track 2). Kick on offbeats (steps 7, 15 with nudge -5 to -8).
- **Simultaneous ride+hihat**: MIDI often has both ride AND closed hihat — they share our hihat track. Prioritize ride velocity, the combined sound approximates a jazz kit sound.
- **Two-note melody on same step**: When melody has a grace note → target on same step (e.g., beat 0.02 and 0.12), use nudge to differentiate: first note at nudge+2 with gate=1, second note at nudge+11 with longer gate.
- **Finger stagger**: PICK_ALL chords use `melodyTriggerChordFMKeys` which adds +12ms attack offset per voice. This is automatic — no per-step configuration needed.
- **Chord voicing approximation**: For jazz voicings that don't match CHORD_TRIAD/CHORD_SEVENTH, use the root note + closest chord type. The intervals won't be exact but produce reasonable harmonic support.
- **16th vs 32nd resolution**: Default to 16th (most songs). Use 32nd when the MIDI has grace note stutters, fast ornaments, or 32nd-note runs that collide on the 16th grid. Signs you need 32nd: multiple notes quantize to the same step, or grace notes fall exactly between steps. For 32nd mode: `seqSet32ndNoteMode(true)` + set all trackLengths to 32. Drums/bass that were on every 16th step become every-other-step at 32nd resolution.
- **Why not double BPM as a workaround?** Halves available bars (8 patterns = 4 bars instead of 8), and all timing relationships (gate, swing, dilla) are calibrated for the step size. Proper 32nd mode is cleaner.
