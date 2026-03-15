#ifndef SONGS_H
#define SONGS_H

// songs.h is included from sound_synth_bridge.c which already has
// the full soundsystem included. No extra includes needed here.

// MIDI note helpers
// Octave 2: C2=36, D2=38, E2=40, F2=41, G2=43, A2=45, B2=47
// Octave 3: C3=48, D3=50, E3=52, F3=53, G3=55, A3=57, B3=59
// Octave 4: C4=60, D4=62, E4=64, F4=65, G4=67, A4=69, B4=71
// Octave 5: C5=72, D5=74, E5=76
// Flats: Eb=E-1, Bb=B-1, Ab=A-1

#define Cn2 36
#define D2  38
#define Eb2 39
#define E2  40
#define F2  41
#define G2  43
#define Ab2 44
#define A2  45
#define Bb2 46
#define B2  47
#define Cn3 48
#define D3  50
#define Eb3 51
#define E3  52
#define F3  53
#define G3  55
#define Ab3 56
#define A3  57
#define Bb3 58
#define B3  59
#define Cn4 60
#define D4  62
#define Eb4 63
#define E4  64
#define F4  65
#define G4  67
#define Ab4 68
#define A4  69
#define Bb4 70
#define B4  71
#define Cn5 72
#define D5  74
#define Eb5 75
#define E5  76
#define F5  77
#define G5  79

// Extra low notes
#define A1  33
#define Bb1 34

// Sharps and extra notes
#define Cs2 37
#define Fs2 42
#define Gs2 44
#define Cs3 49
#define Fs3 54
#define Gs3 56
#define Cs4 61
#define Fs4 66
#define Gs4 68

// Octave 1
#define Cn1 24
#define Cs1 25
#define D1  26
#define Eb1 27
#define E1  28
#define F1  29
#define G1  31
#define Ab1 32
#define B1  35

// Octave 0
#define A0  21
#define Bb0 22
#define B0  23

// Octave 5 extras
#define Cs5 73
#define Fs5 78
#define A5  81
#define Ab5 80
#define Bb5 82

#define REST (-1)

// ============================================================================
// Song authoring helpers — reduce 3 assignments to 1 call
// ============================================================================

// Set a melody note: note + velocity + gate in one call
static inline void note(Pattern* p, int track, int step, int n, float vel, int gate) {
    patSetNote(p, SEQ_DRUM_TRACKS + track, step, n, vel, gate);
}

// Set a melody note with timing nudge (p-lock)
static inline void noteN(Pattern* p, int track, int step, int n, float vel, int gate, float nudge) {
    patSetNote(p, SEQ_DRUM_TRACKS + track, step, n, vel, gate);
    seqSetPLock(p, SEQ_DRUM_TRACKS + track, step, PLOCK_TIME_NUDGE, nudge);
}

// Set a melody note with sustain (extra hold steps after gate)
static inline void noteS(Pattern* p, int track, int step, int n, float vel, int gate, int sus) {
    patSetNote(p, SEQ_DRUM_TRACKS + track, step, n, vel, gate);
    patSetNoteSustain(p, SEQ_DRUM_TRACKS + track, step, sus);
}

// Set a melody note with nudge + sustain
static inline void noteNS(Pattern* p, int track, int step, int n, float vel, int gate, float nudge, int sus) {
    patSetNote(p, SEQ_DRUM_TRACKS + track, step, n, vel, gate);
    seqSetPLock(p, SEQ_DRUM_TRACKS + track, step, PLOCK_TIME_NUDGE, nudge);
    patSetNoteSustain(p, SEQ_DRUM_TRACKS + track, step, sus);
}

// Set a chord step: root note + chord type → multi-note v2 step
static inline void chord(Pattern* p, int track, int step, int root, ChordType type, float vel, int gate) {
    patSetChord(p, SEQ_DRUM_TRACKS + track, step, root, type, vel, gate);
}

// Set a custom chord step with explicit notes (up to 4, -1 = unused)
static inline void chordCustom(Pattern* p, int track, int step, float vel, int gate, int n0, int n1, int n2, int n3) {
    patSetChordCustom(p, SEQ_DRUM_TRACKS + track, step, vel, gate, n0, n1, n2, n3);
}

// Set a drum hit: step + velocity in one call
static inline void drum(Pattern* p, int track, int step, float vel) {
    patSetDrum(p, track, step, vel, 0.0f);
}

// Set a drum hit with timing nudge
static inline void drumN(Pattern* p, int track, int step, float vel, float nudge) {
    patSetDrum(p, track, step, vel, 0.0f);
    seqSetPLock(p, track, step, PLOCK_TIME_NUDGE, nudge);
}

// Set a drum hit with pitch offset
static inline void drumP(Pattern* p, int track, int step, float vel, float pitch) {
    patSetDrum(p, track, step, vel, pitch);
}

// ============================================================================
// Song: Dormitory Ambient
//
// Very slow, sparse, lullaby-like. D dorian (D E F G A B C).
// Almost no drums — just a faint closed hihat on offbeats like a clock tick.
// Bass: slow root notes with long sustain.
// Lead: a glockenspiel playing a pentatonic melody, mostly rests.
// Chords: warm additive pad holding simple triads.
//
// The idea: movers are sleeping or resting. This should feel like
// a music box winding down, or rain on a window.
// ============================================================================

// --- Voice configuration ---
// Call this once when the dormitory ensemble starts playing,
// BEFORE the sequencer begins ticking. Sets up the synth patches
// so the melody/bass/chord triggers produce the right timbres.

static void Song_DormitoryAmbient_ConfigureVoices(void) {
    // Global: slow, soft, lots of release
    masterVolume = 0.25f;

    // Scale lock: D dorian (D root, dorian mode)
    scaleLockEnabled = true;
    scaleRoot = 2;                    // D
    scaleType = SCALE_DORIAN;

    // Default envelope: very soft, pad-like
    noteAttack  = 0.4f;
    noteDecay   = 0.8f;
    noteSustain = 0.3f;
    noteRelease = 1.5f;
    noteVolume  = 0.3f;

    // Gentle filter — dark, warm
    noteFilterCutoff    = 0.3f;
    noteFilterResonance = 0.1f;
    noteFilterEnvAmt    = 0.05f;

    // Slow vibrato for warmth
    noteVibratoRate  = 3.5f;
    noteVibratoDepth = 0.003f;

    // No PWM, no arp
    notePwmRate  = 0.0f;
    notePwmDepth = 0.0f;
    noteArpEnabled = false;
}

// --- Pattern A: the main loop ---

static void Song_DormitoryAmbient_PatternA(Pattern* p) {
    initPattern(p);

    // HiHat: very faint ticks on offbeats, like a clock
    drum(p, 2, 2, .15);  drum(p, 2, 6, .12);
    drum(p, 2,10, .15);  drum(p, 2,14, .10);
    // Perc: single rimshot on step 12, very faint — a creak
    drumP(p, 3, 12, 0.08, 0.3);
    patSetDrumProb(p, 3, 12, 0.4f);

    // Bass: slow D pedal, then A
    note(p, 0, 0, D2, .35, 8);
    note(p, 0, 8, A2, .30, 8);

    // Lead: glockenspiel, sparse descending phrase
    note(p, 1, 3, A4, .25, 3);   note(p, 1, 6, G4, .20, 2);
    note(p, 1,10, F4, .22, 2);   note(p, 1,13, D4, .18, 3);

    // Chords: Dm (half bar) then Am
    chord(p, 2, 0, D3, CHORD_TRIAD, 0.20, 8);

    chord(p, 2, 8, A3, CHORD_TRIAD, 0.18, 8);
}

// --- Pattern B: variation (sparser, for variety when looping) ---

static void Song_DormitoryAmbient_PatternB(Pattern* p) {
    initPattern(p);

    // HiHat: even sparser
    drum(p, 2, 6, 0.10);  drum(p, 2, 14, 0.12);

    // Bass: single D2 held the whole bar
    note(p, 0, 0, D2, 0.30, 16);

    // Lead: call and response
    note(p, 1, 4,  A4, 0.20, 4);
    note(p, 1, 12, D4, 0.18, 4);
    patSetNoteSlide(p, SEQ_DRUM_TRACKS + 1, 12, true);

    // Chords: Dm the whole bar
    chord(p, 2, 0, D3, CHORD_TRIAD, 0.15, 16);
}

// --- Song loader: populates two patterns (A B) for alternation ---
// Usage:
//   Song_DormitoryAmbient_ConfigureVoices();
//   Pattern patterns[2];
//   Song_DormitoryAmbient_Load(patterns);
//   // patterns[0] = A (main), patterns[1] = B (variation)
//   // Play as: A A B A  A A B A  ... (or just alternate A B A B)

static void Song_DormitoryAmbient_Load(Pattern patterns[2]) {
    Song_DormitoryAmbient_PatternA(&patterns[0]);
    Song_DormitoryAmbient_PatternB(&patterns[1]);
}

#define SONG_DORMITORY_AMBIENT_BPM  56.0f

// ============================================================================
// Song: Suspense
//
// C harmonic minor. Dark organ drones, eerie vowel chanting, dread.
// Slow chromatic bass descent. Tritone intervals. Nothing resolves.
// Sparse drums: deep sub-kick heartbeat, distant metallic ticks.
// Voice: low "oo" and "oh" vowels, breathy, sliding between dissonances.
// Organ chords: minor with added b2 and tritone, held forever.
//
// This should feel like something is watching you from the dark.
// ============================================================================

// Instrument type tags — the trigger callbacks in sound_synth_bridge.c
// check these to know which synth engine to use per track.
typedef enum {
    SONG_INST_TRIANGLE,       // dormitory bass
    SONG_INST_GLOCKENSPIEL,   // dormitory lead
    SONG_INST_CHOIR,          // dormitory chords
    SONG_INST_ORGAN,          // suspense bass + chords
    SONG_INST_VOWEL,          // suspense lead (voice)
} SongInstType;

static void Song_Suspense_ConfigureVoices(void) {
    masterVolume = 0.30f;

    // C harmonic minor: C D Eb F G Ab B
    scaleLockEnabled = true;
    scaleRoot = 0;                      // C
    scaleType = SCALE_HARMONIC_MIN;

    // Dark, slow, oppressive envelope
    noteAttack  = 0.6f;
    noteDecay   = 1.2f;
    noteSustain = 0.5f;
    noteRelease = 2.5f;
    noteVolume  = 0.35f;

    // Very dark filter
    noteFilterCutoff    = 0.20f;
    noteFilterResonance = 0.25f;
    noteFilterEnvAmt    = 0.08f;

    // Slow, uneasy vibrato
    noteVibratoRate  = 2.0f;
    noteVibratoDepth = 0.004f;

    // No PWM, no arp
    notePwmRate  = 0.0f;
    notePwmDepth = 0.0f;
    noteArpEnabled = false;

    // Voice settings: dark, breathy, buzzy — ominous chanting
    voiceBreathiness = 0.25f;
    voiceBuzziness   = 0.8f;
    voiceNasalAmt    = 0.7f;
}

// --- Pattern A: the dread builds ---

static void Song_Suspense_PatternA(Pattern* p) {
    initPattern(p);

    // Kick: heartbeat, pitched down
    drumP(p, 0, 0, 0.50, -0.4);  drumP(p, 0, 8, 0.35, -0.4);

    // HiHat: distant metallic tick, irregular
    drum(p, 2, 3, 0.08);
    drum(p, 2, 7, 0.06);   patSetDrumProb(p, 2, 7, 0.5f);
    drum(p, 2,11, 0.10);
    drum(p, 2,15, 0.05);   patSetDrumProb(p, 2, 15, 0.3f);

    // Perc: distant rimshot, very rare
    drumP(p, 3, 6, 0.06, -0.3);
    patSetDrumProb(p, 3, 6, 0.25f);

    // Bass: chromatic descent C2→B1→Bb2→A2
    note(p, 0, 0, Cn2, .40, 4);  note(p, 0, 4, B1,  .38, 4);
    note(p, 0, 8, Bb2, .36, 4);  note(p, 0,12, A2,  .42, 4);

    // Lead: eerie vowel chanting on dissonant intervals
    note(p, 1, 4, Eb4, .22, 5);
    note(p, 1,11, G4,  .18, 4);
    note(p, 1,14, Ab4, .20, 3);
    patSetNoteSlide(p, SEQ_DRUM_TRACKS + 1, 14, true);

    // Chords: Cm then Ab
    chord(p, 2, 0, Cn3, CHORD_TRIAD, 0.18, 8);

    chord(p, 2, 8, Ab3, CHORD_TRIAD, 0.16, 8);
}

// --- Pattern B: the void stares back ---

static void Song_Suspense_PatternB(Pattern* p) {
    initPattern(p);

    // Kick: slower heartbeat, just beat 1
    drumP(p, 0, 0, 0.40, -0.5);
    // HiHat: single tick, alone in the dark
    drum(p, 2, 10, 0.07);

    // Bass: single low C drone
    note(p, 0, 0, Cn2, 0.35, 16);
    // Lead: B4 against C bass = minor 2nd
    note(p, 1, 6, B4, 0.20, 10);

    // Chords: Cm7 held whole bar
    chord(p, 2, 0, Cn3, CHORD_SEVENTH, 0.14, 16);
}

static void Song_Suspense_Load(Pattern patterns[2]) {
    Song_Suspense_PatternA(&patterns[0]);
    Song_Suspense_PatternB(&patterns[1]);
}

#define SONG_SUSPENSE_BPM  48.0f

// ============================================================================
// Song: Jazz Call & Response
//
// G mixolydian (G A B C D E F). Two voices trading phrases.
// Voice A (vibraphone): plays a phrase, then rests.
// Voice B (FM electric piano): rests, then responds.
// Bass: walking upright bass (Karplus-Strong plucked strings).
// Drums: swing ride pattern, brushy snare on 2 and 4, sparse kick.
// Dilla swing timing for that loose, behind-the-beat jazz feel.
//
// Pattern A: vibes call first half, FM responds second half.
// Pattern B: FM calls first half, vibes respond second half.
// Pattern C: both trade faster — 4-step phrases (trading twos).
// ============================================================================

static void Song_JazzCallResponse_ConfigureVoices(void) {
    masterVolume = 0.35f;

    // G mixolydian (G root, dominant 7th sound)
    scaleLockEnabled = true;
    scaleRoot = 7;                      // G
    scaleType = SCALE_MIXOLYDIAN;

    // Medium envelope — not too long, not too short
    noteAttack  = 0.01f;
    noteDecay   = 0.4f;
    noteSustain = 0.3f;
    noteRelease = 0.6f;
    noteVolume  = 0.4f;

    // Slightly open filter
    noteFilterCutoff    = 0.55f;
    noteFilterResonance = 0.12f;
    noteFilterEnvAmt    = 0.15f;

    // No vibrato on global — each instrument configures its own
    noteVibratoRate  = 0.0f;
    noteVibratoDepth = 0.0f;
    notePwmRate  = 0.0f;
    notePwmDepth = 0.0f;
    noteArpEnabled = false;

    // FM settings: Rhodes-like electric piano (ratio 1:1, low index, no feedback)
    fmModRatio = 1.0f;
    fmModIndex = 11.31f;
    fmFeedback = 0.0f;
}

// --- Pattern A: vibes call, FM responds ---

static void Song_JazzCallResponse_PatternA(Pattern* p) {
    initPattern(p);

    // Kick: beats 1 and 3
    drum(p, 0, 0, .35);  drum(p, 0, 8, .28);
    // Snare: brush on 2 and 4 + ghost
    drum(p, 1, 4, .15);  drum(p, 1,12, .18);
    drum(p, 1,10, .06);  patSetDrumProb(p, 1, 10, 0.5f);
    // HiHat: ride pattern (8ths) + swing offbeats
    drum(p, 2, 0, .30);  drum(p, 2, 2, .18);  drum(p, 2, 3, .10);
    drum(p, 2, 4, .30);  drum(p, 2, 6, .18);  drum(p, 2, 7, .08);
    drum(p, 2, 8, .30);  drum(p, 2,10, .18);  drum(p, 2,11, .10);
    drum(p, 2,12, .30);  drum(p, 2,14, .18);  drum(p, 2,15, .08);

    // Bass: walking G2→B2→D3→E3→F3→E3→D3→B2
    note(p, 0, 0, G2, .50, 2);  note(p, 0, 2, B2, .42, 2);
    note(p, 0, 4, D3, .45, 2);  note(p, 0, 6, E3, .40, 2);
    note(p, 0, 8, F3, .48, 2);  note(p, 0,10, E3, .40, 2);
    note(p, 0,12, D3, .45, 2);  note(p, 0,14, B2, .42, 2);

    // Lead/CALL (vibes): rising phrase first half
    note(p, 1, 1, G4, .35, 2);  note(p, 1, 3, A4, .38, 2);
    note(p, 1, 4, B4, .42, 3);  note(p, 1, 6, D5, .45, 4);
    patSetNoteAccent(p, SEQ_DRUM_TRACKS + 1, 6, true);

    // Chord/RESPONSE (FM): descending answer second half
    note(p, 2, 9, Cn5, .38, 2);  note(p, 2,11, B4, .35, 2);
    note(p, 2,12, A4,  .40, 3);  note(p, 2,14, G4, .42, 4);
    patSetNoteAccent(p, SEQ_DRUM_TRACKS + 2, 14, true);
}

// --- Pattern B: FM calls, vibes respond (roles swapped) ---

static void Song_JazzCallResponse_PatternB(Pattern* p) {
    initPattern(p);

    // Same drums as A
    drum(p, 0, 0, .35);  drum(p, 0, 8, .28);
    drum(p, 1, 4, .15);  drum(p, 1,12, .18);
    drum(p, 2, 0, .30);  drum(p, 2, 2, .18);  drum(p, 2, 3, .10);
    drum(p, 2, 4, .30);  drum(p, 2, 6, .18);  drum(p, 2, 7, .08);
    drum(p, 2, 8, .30);  drum(p, 2,10, .18);  drum(p, 2,11, .10);
    drum(p, 2,12, .30);  drum(p, 2,14, .18);  drum(p, 2,15, .08);

    // Bass: descending walk G2→F2→E2→D2→C3→B2→A2→G2
    note(p, 0, 0, G2,  .50, 2);  note(p, 0, 2, F2,  .42, 2);
    note(p, 0, 4, E2,  .45, 2);  note(p, 0, 6, D2,  .40, 2);
    note(p, 0, 8, Cn3, .48, 2);  note(p, 0,10, B2,  .42, 2);
    note(p, 0,12, A2,  .40, 2);  note(p, 0,14, G2,  .45, 2);

    // FM CALLS first half (track 2)
    note(p, 2, 1, D4, .38, 2);  note(p, 2, 3, E4, .35, 2);
    note(p, 2, 4, F4, .40, 2);  note(p, 2, 6, A4, .42, 3);

    // Vibes RESPOND second half (track 1)
    note(p, 1, 9, E5, .35, 2);  note(p, 1,11, D5, .38, 2);
    note(p, 1,12, B4, .40, 3);  note(p, 1,14, G4, .45, 4);
    patSetNoteSlide(p, SEQ_DRUM_TRACKS + 1, 14, true);
}

// --- Pattern C: trading twos — fast back and forth ---

static void Song_JazzCallResponse_PatternC(Pattern* p) {
    initPattern(p);

    // Drums: busier with ghost snares
    drum(p, 0, 0, .35);  drum(p, 0, 8, .30);
    drum(p, 1, 2, .05);  drum(p, 1, 4, .18);
    drum(p, 1,10, .06);  drum(p, 1,12, .20);
    drum(p, 2, 0, .30);  drum(p, 2, 2, .18);  drum(p, 2, 3, .12);
    drum(p, 2, 4, .30);  drum(p, 2, 6, .18);  drum(p, 2, 7, .10);
    drum(p, 2, 8, .30);  drum(p, 2,10, .18);  drum(p, 2,11, .12);
    drum(p, 2,12, .30);  drum(p, 2,14, .18);  drum(p, 2,15, .10);

    // Bass: root-fifth alternation
    note(p, 0, 0, G2, .50, 3);  note(p, 0, 4, D3, .45, 3);
    note(p, 0, 8, G2, .48, 3);  note(p, 0,12, D3, .45, 3);

    // Vibes: beats 1-2 and 3-4 of each half
    note(p, 1, 0, B4, .40, 2);  note(p, 1, 2, D5, .38, 2);
    note(p, 1, 8, A4, .35, 2);  note(p, 1,10, G4, .40, 2);

    // FM: fills the gaps
    note(p, 2, 4, Cn5, .38, 2);  note(p, 2, 6, B4, .35, 2);
    note(p, 2,12, F4,  .40, 2);  note(p, 2,14, E4, .42, 3);
}

static void Song_JazzCallResponse_Load(Pattern patterns[3]) {
    Song_JazzCallResponse_PatternA(&patterns[0]);
    Song_JazzCallResponse_PatternB(&patterns[1]);
    Song_JazzCallResponse_PatternC(&patterns[2]);
}

#define SONG_JAZZ_BPM  108.0f

// ============================================================================
// Song: House
//
// 124 BPM, C minor. Classic four-on-the-floor.
// Acid saw bass with resonant filter sweep (~40s cycle).
// FM chord stabs with brightness following the sweep.
// Open hihat offbeats, clap on 2 and 4.
//
// The filter sweep is the star — bass opens slowly over ~20 seconds,
// peaks, then closes back down. Chord stabs follow inversely.
// Pattern A: main groove. Pattern B: breakdown (half the drums drop out,
// bass goes minimal, filter closes). Pattern C: build back up.
// ============================================================================

static void Song_House_ConfigureVoices(void) {
    masterVolume = 0.35f;

    scaleLockEnabled = true;
    scaleRoot = 0;                      // C
    scaleType = SCALE_MINOR;

    noteAttack  = 0.003f;
    noteDecay   = 0.2f;
    noteSustain = 0.5f;
    noteRelease = 0.15f;
    noteVolume  = 0.45f;

    noteFilterCutoff    = 0.3f;
    noteFilterResonance = 0.3f;
    noteFilterEnvAmt    = 0.2f;

    noteVibratoRate  = 0.0f;
    noteVibratoDepth = 0.0f;
    notePwmRate  = 0.0f;
    notePwmDepth = 0.0f;
    noteArpEnabled = false;
}

// --- Pattern A: the main groove ---

static void Song_House_PatternA(Pattern* p) {
    initPattern(p);

    // Kick: four on the floor
    drum(p, 0, 0, .70);  drum(p, 0, 4, .70);
    drum(p, 0, 8, .70);  drum(p, 0,12, .70);
    // Clap on 2 and 4
    drum(p, 1, 4, .45);  drum(p, 1,12, .48);
    // HiHat: offbeat opens + closed on downbeats
    drum(p, 2, 0, .15);  drum(p, 2, 2, .35);
    drum(p, 2, 4, .12);  drum(p, 2, 6, .35);
    drum(p, 2, 8, .15);  drum(p, 2,10, .35);
    drum(p, 2,12, .12);  drum(p, 2,14, .35);
    // Perc: shaker on 16ths with probability
    for (int s = 1; s < 16; s += 2) {
        drumP(p, 3, s, 0.08, 0.5);
        patSetDrumProb(p, 3, s, 0.4f);
    }

    // Acid bass: syncopated C2 line
    note(p, 0, 0, Cn2, .55, 2);  note(p, 0, 2, Cn2, .48, 2);
    note(p, 0, 4, Cn2, .42, 1);  note(p, 0, 5, Cn2, .50, 2);
    note(p, 0, 7, Eb2, .52, 2);  note(p, 0, 9, Cn2, .48, 2);
    note(p, 0,11, Cn2, .40, 1);  note(p, 0,12, Cn2, .55, 2);
    note(p, 0,15, G2,  .45, 2);
    patSetNoteSlide(p, SEQ_DRUM_TRACKS + 0, 5, true);   patSetNoteSlide(p, SEQ_DRUM_TRACKS + 0, 15, true);
    patSetNoteAccent(p, SEQ_DRUM_TRACKS + 0, 0, true);   patSetNoteAccent(p, SEQ_DRUM_TRACKS + 0, 12, true);

    // Chord stabs (track 1): offbeat Cm
    chord(p, 1, 2, Cn4, CHORD_TRIAD, 0.35, 1);

    chord(p, 1, 6, Cn4, CHORD_TRIAD, 0.30, 1);

    chord(p, 1, 10, Ab3, CHORD_TRIAD, 0.32, 1);

    // Pad: Cm7 held whole bar
    chord(p, 2, 0, Cn3, CHORD_SEVENTH, 0.15, 16);
}

// --- Pattern B: breakdown (filter closes, drums thin out) ---

static void Song_House_PatternB(Pattern* p) {
    initPattern(p);

    // Kick: only beat 1
    drum(p, 0, 0, .55);
    // HiHat: sparse offbeats
    drum(p, 2, 2, .20);  drum(p, 2, 10, .18);

    // Bass: root held long
    note(p, 0, 0, Cn2, .45, 8);
    note(p, 0, 8, Eb2, .40, 8);
    patSetNoteSlide(p, SEQ_DRUM_TRACKS + 0, 8, true);

    // Pad only, quieter
    chord(p, 2, 0, Cn3, CHORD_TRIAD, 0.12, 16);
}

static void Song_House_Load(Pattern patterns[2]) {
    Song_House_PatternA(&patterns[0]);
    Song_House_PatternB(&patterns[1]);
}

#define SONG_HOUSE_BPM  124.0f

// ============================================================================
// Song: Deep House
//
// 120 BPM, F minor. Minimal, hypnotic, deep.
// Sub bass (triangle) — almost felt more than heard.
// Filtered string pad with a very slow sweep (~60s full cycle).
// Kick on 1 and 3 only. Rim click. Sparse everything.
// The filter sweep on the pad is the entire movement — it opens
// over 30 seconds, reaching a bright peak, then closes over 30 seconds.
// Very meditative, very deep.
// ============================================================================

#define Fn2 41
#define Fn3 53
#define Fn4 65

static void Song_DeepHouse_ConfigureVoices(void) {
    masterVolume = 0.30f;

    scaleLockEnabled = true;
    scaleRoot = 5;                      // F
    scaleType = SCALE_MINOR;

    noteAttack  = 0.01f;
    noteDecay   = 0.5f;
    noteSustain = 0.4f;
    noteRelease = 0.8f;
    noteVolume  = 0.35f;

    noteFilterCutoff    = 0.25f;
    noteFilterResonance = 0.10f;
    noteFilterEnvAmt    = 0.10f;

    noteVibratoRate  = 0.0f;
    noteVibratoDepth = 0.0f;
    notePwmRate  = 0.0f;
    notePwmDepth = 0.0f;
    noteArpEnabled = false;
}

// --- Pattern A: the hypnotic groove ---

static void Song_DeepHouse_PatternA(Pattern* p) {
    initPattern(p);

    // Kick: beats 1 and 3
    drum(p, 0, 0, .60);  drum(p, 0, 8, .50);
    // Clap: soft on 2 only
    drum(p, 1, 4, .25);
    // HiHat: closed 8ths, shimmer
    drum(p, 2, 0, .18);  drum(p, 2, 2, .10);
    drum(p, 2, 4, .10);  drum(p, 2, 6, .10);
    drum(p, 2, 8, .18);  drum(p, 2,10, .10);
    drum(p, 2,12, .10);  drum(p, 2,14, .10);
    // Perc: rim click, sparse
    drum(p, 3, 3, .12);  patSetDrumProb(p, 3, 3, 0.6f);
    drum(p, 3,11, .10);  patSetDrumProb(p, 3, 11, 0.4f);

    // Sub bass: F1 and C2
    note(p, 0, 0, F1,  .50, 8);
    note(p, 0, 8, Cn2, .45, 8);

    // Stab (track 1): sparse FM color
    note(p, 1, 6, Cn4, .25, 2);
    note(p, 1,14, Ab3, .20, 2);
    patSetDrumProb(p, 1, 14, 0.5f);

    // Pad: Fm7 held whole bar
    chord(p, 2, 0, Fn3, CHORD_SEVENTH, 0.20, 16);
}

// --- Pattern B: even more minimal — pad solo with filter breathing ---

static void Song_DeepHouse_PatternB(Pattern* p) {
    initPattern(p);

    drum(p, 0, 0, .50);     // Kick: beat 1 only
    drum(p, 3, 7, .10);     // Single rim click

    note(p, 0, 0, F1, .45, 16);   // Sub bass: F1 whole bar

    // Pad: Fm7 whole bar
    chord(p, 2, 0, Fn3, CHORD_SEVENTH, 0.22, 16);
}

static void Song_DeepHouse_Load(Pattern patterns[2]) {
    Song_DeepHouse_PatternA(&patterns[0]);
    Song_DeepHouse_PatternB(&patterns[1]);
}

#define SONG_DEEP_HOUSE_BPM  120.0f

// ============================================================================
// Song: Dilla Hip-Hop
//
// 88 BPM, Eb major / C minor (relative). The J Dilla sound:
// HEAVY swing — notes land late, drunk, behind the beat.
// Kick pattern: syncopated, never on the grid. Ghost snares everywhere.
// Bass: warm plucked upright, chromatic passing tones.
// Keys: lo-fi Rhodes chords, sparse and soulful.
// Pad: vinyl-warm choir, barely there — like a sample through SP-303 haze.
//
// The swing settings in the bridge do the heavy lifting here.
// Pattern data is relatively simple — it's the TIMING that makes it Dilla.
// Unquantized feel: swing=9, snareDelay=5, jitter=4.
//
// Pattern A: main groove — bass walks, Rhodes stabs on upbeats.
// Pattern B: breakdown — bass drops out, just kick + pad + ghost snares.
// Pattern C: full groove — everything playing, bass gets busier.
// ============================================================================

static void Song_Dilla_ConfigureVoices(void) {
    masterVolume = 0.30f;

    // Eb major (enharmonic with C minor relative)
    scaleLockEnabled = true;
    scaleRoot = 3;                      // Eb
    scaleType = SCALE_MAJOR;

    // Warm, rounded envelope
    noteAttack  = 0.008f;
    noteDecay   = 0.35f;
    noteSustain = 0.3f;
    noteRelease = 0.5f;
    noteVolume  = 0.40f;

    // Dark filter — everything sounds like it's coming through a speaker
    noteFilterCutoff    = 0.30f;
    noteFilterResonance = 0.12f;
    noteFilterEnvAmt    = 0.10f;

    // Slight vibrato — vinyl wow
    noteVibratoRate  = 4.5f;
    noteVibratoDepth = 0.002f;

    notePwmRate  = 0.0f;
    notePwmDepth = 0.0f;
    noteArpEnabled = false;

    // FM: lo-fi Rhodes
    fmModRatio = 1.0f;
    fmModIndex = 7.54f;
    fmFeedback = 0.0f;
}

// --- Pattern A: the head nod ---

static void Song_Dilla_PatternA(Pattern* p) {
    initPattern(p);

    // Kick: syncopated, never on 1
    drum(p, 0, 1, .60);  drum(p, 0, 7, .50);
    drum(p, 0,10, .55);  drum(p, 0,14, .35);
    patSetDrumProb(p, 0, 14, 0.6f);
    // Snare: lazy beats 2+4 + ghost notes
    drum(p, 1, 2, .08);  drum(p, 1, 4, .40);
    drum(p, 1, 6, .06);  patSetDrumProb(p, 1, 6, 0.4f);
    drum(p, 1, 9, .07);  patSetDrumProb(p, 1, 9, 0.5f);
    drum(p, 1,12, .42);
    drum(p, 1,15, .05);  patSetDrumProb(p, 1, 15, 0.3f);
    // HiHat: 16ths with velocity variation
    drum(p, 2, 0, .20);  drum(p, 2, 1, .08);  drum(p, 2, 2, .25);  drum(p, 2, 3, .06);
    drum(p, 2, 4, .18);  drum(p, 2, 5, .04);  drum(p, 2, 6, .22);  drum(p, 2, 7, .10);
    drum(p, 2, 8, .20);  drum(p, 2, 9, .06);  drum(p, 2,10, .25);  drum(p, 2,11, .04);
    drum(p, 2,12, .15);  drum(p, 2,13, .08);  drum(p, 2,14, .20);  drum(p, 2,15, .05);
    patSetDrumProb(p, 2, 3, 0.5f);  patSetDrumProb(p, 2, 7, 0.6f);
    patSetDrumProb(p, 2, 11, 0.5f);  patSetDrumProb(p, 2, 15, 0.4f);
    // Perc: shaker on 8ths
    for (int s = 0; s < 16; s += 2) drumP(p, 3, s, 0.06, 0.4);

    // Bass: walking Eb with chromatic passing tones
    note(p, 0, 0, Eb2, .50, 2);  note(p, 0, 2, G2,  .42, 2);
    note(p, 0, 4, Ab2, .45, 2);  note(p, 0, 6, Bb2, .40, 2);
    note(p, 0, 8, Eb2, .52, 3);  note(p, 0,11, D2,  .35, 2);
    note(p, 0,12, Eb2, .48, 2);

    // Keys (track 1): Rhodes stabs on upbeats
    chord(p, 1, 2, Eb4, CHORD_SEVENTH, 0.32, 2);

    note(p, 1, 6, Eb4, 0.28, 1);
    patSetNoteAccent(p, SEQ_DRUM_TRACKS + 1, 6, true);

    chord(p, 1, 11, Ab3, CHORD_TRIAD, 0.30, 2);

    // Pad: Eb maj7 held whole bar
    chord(p, 2, 0, Eb3, CHORD_SEVENTH, 0.15, 16);
}

// --- Pattern B: the breakdown — kick + pad only, heads still nodding ---

static void Song_Dilla_PatternB(Pattern* p) {
    initPattern(p);

    // Kick: minimal syncopation
    drum(p, 0, 1, .55);  drum(p, 0, 9, .45);
    // Ghost snares only
    drum(p, 1, 3, .06);  drum(p, 1, 7, .08);
    drum(p, 1,11, .05);  drum(p, 1,15, .07);
    // HiHat: sparse ticks
    drum(p, 2, 0, .12);  drum(p, 2, 4, .10);
    drum(p, 2, 8, .12);  drum(p, 2,12, .08);

    // Single lonely Rhodes note
    note(p, 1, 6, Bb4, 0.25, 4);

    // Pad: Cm7 — mood shift
    chord(p, 2, 0, Cn3, CHORD_SEVENTH, 0.18, 16);
}

// --- Pattern C: full groove — bass gets busier, more Rhodes ---

static void Song_Dilla_PatternC(Pattern* p) {
    initPattern(p);

    // Full drums with extra kick ghost
    drum(p, 0, 1, .60);  drum(p, 0, 5, .30);  patSetDrumProb(p, 0, 5, 0.5f);
    drum(p, 0, 7, .50);  drum(p, 0,10, .55);   drum(p, 0,13, .35);
    drum(p, 1, 2, .07);  drum(p, 1, 4, .42);   drum(p, 1, 6, .06);
    drum(p, 1,10, .08);  drum(p, 1,12, .45);   drum(p, 1,14, .05);
    // Full 16th hats
    drum(p, 2, 0, .22);  drum(p, 2, 1, .10);  drum(p, 2, 2, .28);  drum(p, 2, 3, .05);
    drum(p, 2, 4, .20);  drum(p, 2, 5, .06);  drum(p, 2, 6, .25);  drum(p, 2, 7, .12);
    drum(p, 2, 8, .22);  drum(p, 2, 9, .08);  drum(p, 2,10, .28);  drum(p, 2,11, .05);
    drum(p, 2,12, .18);  drum(p, 2,13, .10);  drum(p, 2,14, .22);  drum(p, 2,15, .06);
    patSetDrumProb(p, 2, 3, 0.6f);  patSetDrumProb(p, 2, 11, 0.5f);

    // Bass: chromatic run with blue note E natural
    note(p, 0, 0, Eb2, .52, 2);  note(p, 0, 2, E2,  .38, 1);
    note(p, 0, 4, F2,  .45, 2);  note(p, 0, 6, G2,  .40, 1);
    note(p, 0, 8, Ab2, .48, 2);  note(p, 0,10, Bb2, .42, 2);
    note(p, 0,12, Eb2, .55, 2);  note(p, 0,14, D2,  .35, 1);
    note(p, 0,15, Eb2, .48, 1);

    // Rhodes stabs: busier
    chord(p, 1, 2, Eb4, CHORD_SEVENTH, 0.35, 1);

    note(p, 1, 5, Bb3, 0.28, 2);
    patSetNoteAccent(p, SEQ_DRUM_TRACKS + 1, 5, true);

    chord(p, 1, 9, Ab3, CHORD_TRIAD, 0.32, 2);

    note(p, 1, 13, G4, 0.25, 2);

    // Pad: Eb maj7
    chord(p, 2, 0, Eb3, CHORD_SEVENTH, 0.18, 16);
}

static void Song_Dilla_Load(Pattern patterns[3]) {
    Song_Dilla_PatternA(&patterns[0]);
    Song_Dilla_PatternB(&patterns[1]);
    Song_Dilla_PatternC(&patterns[2]);
}

#define SONG_DILLA_BPM  88.0f

// ============================================================================
// Song: Atmosphere
//
// 100 BPM, D major. Big, warm, happy, zero FM.
// Glockenspiel sparkles on top — pentatonic melody, lots of air.
// Additive strings pad: wide, swelling, the "big" sound.
// Plucked bass: warm Karplus-Strong, simple root-fifth movement.
// Drums: soft kick on 1 and 3, brushy hat shimmer, no snare —
// this is about space and light, not groove.
//
// Think: sunrise over the colony, everything is going well.
// Pattern A: the dawn — sparse, building.
// Pattern B: full bloom — everything rings out, melody soars.
// Pattern C: breathing — pad solo with glockenspiel ornaments.
// ============================================================================

static void Song_Atmosphere_ConfigureVoices(void) {
    masterVolume = 0.30f;

    // D major
    scaleLockEnabled = true;
    scaleRoot = 2;                      // D
    scaleType = SCALE_MAJOR;

    // Spacious envelope — long attack, very long release
    noteAttack  = 0.3f;
    noteDecay   = 1.0f;
    noteSustain = 0.5f;
    noteRelease = 2.5f;
    noteVolume  = 0.35f;

    // Warm, open filter
    noteFilterCutoff    = 0.50f;
    noteFilterResonance = 0.05f;
    noteFilterEnvAmt    = 0.08f;

    // Gentle vibrato — like a breeze
    noteVibratoRate  = 3.0f;
    noteVibratoDepth = 0.003f;

    notePwmRate  = 0.0f;
    notePwmDepth = 0.0f;
    noteArpEnabled = false;
}

// --- Pattern A: the dawn ---

static void Song_Atmosphere_PatternA(Pattern* p) {
    initPattern(p);

    // Kick: just beat 1
    drum(p, 0, 0, .35);
    // HiHat: soft shimmer on 8ths
    drum(p, 2, 0, .12);  drum(p, 2, 2, .08);  drum(p, 2, 4, .08);  drum(p, 2, 6, .08);
    drum(p, 2, 8, .08);  drum(p, 2,10, .08);  drum(p, 2,12, .08);  drum(p, 2,14, .08);
    // Perc: distant chime
    drumP(p, 3, 12, 0.06, 0.6);
    patSetDrumProb(p, 3, 12, 0.5f);

    // Bass: simple D pedal
    note(p, 0, 0, D2, .40, 8);
    note(p, 0, 8, A2, .35, 8);

    // Lead: glockenspiel, a few bright notes
    note(p, 1, 4, Fs4, .22, 3);
    note(p, 1,10, A4,  .20, 3);

    // Pad: D major triad
    chord(p, 2, 0, D3, CHORD_TRIAD, 0.12, 16);
}

// --- Pattern B: full bloom — the melody soars ---

static void Song_Atmosphere_PatternB(Pattern* p) {
    initPattern(p);

    // Kick: 1 and 3
    drum(p, 0, 0, .38);  drum(p, 0, 8, .30);
    // HiHat: fuller shimmer + ghost sparkle
    drum(p, 2, 0, .15);  drum(p, 2, 2, .10);  drum(p, 2, 3, .05);
    drum(p, 2, 4, .15);  drum(p, 2, 6, .10);
    drum(p, 2, 8, .15);  drum(p, 2,10, .10);  drum(p, 2,11, .05);
    drum(p, 2,12, .15);  drum(p, 2,14, .10);
    patSetDrumProb(p, 2, 3, 0.4f);  patSetDrumProb(p, 2, 11, 0.3f);
    // Perc: chimes
    drumP(p, 3, 4, 0.05, 0.7);  drumP(p, 3, 12, 0.06, 0.5);

    // Bass: D→F#→A→D walking up
    note(p, 0, 0, D2,  .42, 3);  note(p, 0, 4, Fs2, .38, 3);
    note(p, 0, 8, A2,  .40, 3);  note(p, 0,12, D3,  .45, 3);

    // Lead: glockenspiel soaring melody
    note(p, 1, 1, D5, .28, 3);  note(p, 1, 4, E5, .30, 3);
    note(p, 1, 7, F5, .35, 4);  note(p, 1,10, A4, .25, 2);
    note(p, 1,12, B4, .28, 2);  note(p, 1,14, D5, .32, 4);

    // Pad: Dmaj7 → A
    chord(p, 2, 0, D3, CHORD_SEVENTH, 0.20, 8);

    chord(p, 2, 8, A3, CHORD_TRIAD, 0.22, 8);
}

// --- Pattern C: breathing — spacious, just pad and ornaments ---

static void Song_Atmosphere_PatternC(Pattern* p) {
    initPattern(p);

    // HiHat: barely there
    drum(p, 2, 0, .06);  drum(p, 2, 8, .05);

    // Bass: single D held whole bar
    note(p, 0, 0, D2, .30, 16);

    // Lead: question and answer
    note(p, 1, 4, B4, .18, 6);
    note(p, 1,12, D5, .20, 4);
    patSetNoteSlide(p, SEQ_DRUM_TRACKS + 1, 12, true);

    // Pad: Dmaj7 held whole bar
    chord(p, 2, 0, D3, CHORD_SEVENTH, 0.25, 16);
}

static void Song_Atmosphere_Load(Pattern patterns[3]) {
    Song_Atmosphere_PatternA(&patterns[0]);
    Song_Atmosphere_PatternB(&patterns[1]);
    Song_Atmosphere_PatternC(&patterns[2]);
}

#define SONG_ATMOSPHERE_BPM  100.0f

// ============================================================================
// Song: Mr Lucky (after Karl Jenkins / Sounds for the Supermarket)
//
// 89 BPM, A minor. Smooth muzak / easy listening.
// The iconic supermarket music: walking bass, vibraphone melody,
// lush string pad. Optimistic, warm, sophisticated harmony.
//
// Chord cycle:
//   A: Am7 → Dm7   (i → iv)
//   B: G7  → Cmaj7 (VII → III)
//   C: Fmaj7 → E7  (VI → V, tension back to i)
//
// Walking bass moves stepwise through chord tones.
// Vibes play a gentle melody on top — sparse, singable.
// Strings hold the harmony underneath.
// Drums: just brushes and a faint kick. Cocktail lounge.
// ============================================================================

static void Song_MrLucky_ConfigureVoices(void) {
    masterVolume = 0.32f;

    // A natural minor
    scaleLockEnabled = true;
    scaleRoot = 9;                      // A
    scaleType = SCALE_MINOR;

    // Smooth, warm envelope
    noteAttack  = 0.01f;
    noteDecay   = 0.5f;
    noteSustain = 0.4f;
    noteRelease = 0.8f;
    noteVolume  = 0.40f;

    // Warm, open filter — nothing harsh
    noteFilterCutoff    = 0.50f;
    noteFilterResonance = 0.08f;
    noteFilterEnvAmt    = 0.10f;

    // Gentle vibrato
    noteVibratoRate  = 4.0f;
    noteVibratoDepth = 0.002f;

    notePwmRate  = 0.0f;
    notePwmDepth = 0.0f;
    noteArpEnabled = false;
}

// --- Pattern A: Am7 → Dm7 (the opening, the head nod) ---

static void Song_MrLucky_PatternA(Pattern* p) {
    initPattern(p);

    // Drums: cocktail brushes
    drum(p, 0, 0, .25);  drum(p, 0, 8, .20);
    drum(p, 1, 4, .12);  drum(p, 1,12, .14);
    // Ride: smooth 8ths
    drum(p, 2, 0, .18);  drum(p, 2, 2, .12);  drum(p, 2, 4, .18);  drum(p, 2, 6, .12);
    drum(p, 2, 8, .18);  drum(p, 2,10, .12);  drum(p, 2,12, .18);  drum(p, 2,14, .12);

    // Bass: walking A→B→C→D
    note(p, 0, 0, A2,  .48, 3);  note(p, 0, 4, B2,  .40, 3);
    note(p, 0, 8, Cn3, .42, 3);  note(p, 0,12, D3,  .45, 3);

    // Lead: vibes, gentle rising
    note(p, 1, 4, E4,  .30, 4);
    note(p, 1,10, Cn5, .25, 3);

    // Pad: Am7 → Dm7
    chord(p, 2, 0, A3, CHORD_SEVENTH, 0.20, 8);

    chord(p, 2, 8, D3, CHORD_SEVENTH, 0.18, 8);
}

// --- Pattern B: G7 → Cmaj7 (the lift, brightening) ---

static void Song_MrLucky_PatternB(Pattern* p) {
    initPattern(p);

    // Same brushes
    drum(p, 0, 0, .25);  drum(p, 0, 8, .20);
    drum(p, 1, 4, .12);  drum(p, 1,12, .14);
    drum(p, 2, 0, .18);  drum(p, 2, 2, .12);  drum(p, 2, 4, .18);  drum(p, 2, 6, .12);
    drum(p, 2, 8, .18);  drum(p, 2,10, .12);  drum(p, 2,12, .18);  drum(p, 2,14, .12);

    // Bass: walking G→A→B→C
    note(p, 0, 0, G2,  .48, 3);  note(p, 0, 4, A2,  .40, 3);
    note(p, 0, 8, B2,  .42, 3);  note(p, 0,12, Cn3, .45, 3);

    // Vibes: the bright lift
    note(p, 1, 1, D4, .28, 3);
    note(p, 1, 6, E4, .25, 2);
    note(p, 1,10, G4, .30, 4);

    // Pad: G7 → Cmaj7
    chord(p, 2, 0, G3, CHORD_SEVENTH, 0.18, 8);

    chord(p, 2, 8, Cn3, CHORD_SEVENTH, 0.22, 8);
}

// --- Pattern C: Fmaj7 → E7 (tension, wants to resolve back to Am) ---

static void Song_MrLucky_PatternC(Pattern* p) {
    initPattern(p);

    // Drums: same + perc turnaround
    drum(p, 0, 0, .25);  drum(p, 0, 8, .22);
    drum(p, 1, 4, .12);  drum(p, 1,12, .15);
    drum(p, 2, 0, .18);  drum(p, 2, 2, .12);  drum(p, 2, 4, .18);  drum(p, 2, 6, .12);
    drum(p, 2, 8, .18);  drum(p, 2,10, .12);  drum(p, 2,12, .18);  drum(p, 2,14, .12);
    drumP(p, 3, 12, 0.08, 0.4);

    // Bass: F→E oscillating tension
    note(p, 0, 0, F2, .45, 3);  note(p, 0, 4, E2, .42, 3);
    note(p, 0, 8, F2, .48, 3);  note(p, 0,12, E2, .50, 3);

    // Vibes: descending A4→G4→F4→E4
    note(p, 1, 0, A4, .28, 3);  note(p, 1, 4, G4, .25, 3);
    note(p, 1, 9, F4, .27, 3);  note(p, 1,13, E4, .30, 3);
    patSetNoteSlide(p, SEQ_DRUM_TRACKS + 1, 13, true);

    // Pad: Fmaj7 → E7
    chord(p, 2, 0, F3, CHORD_SEVENTH, 0.20, 8);

    chord(p, 2, 8, E3, CHORD_SEVENTH, 0.22, 8);
}

static void Song_MrLucky_Load(Pattern patterns[3]) {
    Song_MrLucky_PatternA(&patterns[0]);
    Song_MrLucky_PatternB(&patterns[1]);
    Song_MrLucky_PatternC(&patterns[2]);
}

#define SONG_MR_LUCKY_BPM  89.0f

// ============================================================================
// Song: Happy Birthday (Jazz Waltz)
//
// F major, 3/4 time. Classic melody over a walking bass and jazz chords.
// Converted from MIDI (PC MIDI Center 1996 arrangement).
//
// Uses 8th-note step resolution: each step = one 8th note.
// Track length 12 = 2 bars of 3/4 (6 eighth notes per bar × 2).
// BPM set to 55 so sequencer steps align with 8th notes at 110 BPM feel.
//
// Steps per bar:  0  1  2  3  4  5 | 6  7  8  9 10 11
// Beats:          1  +  2  +  3  + | 1  +  2  +  3  +
//
// Structure: A B C D (one chorus of Happy Birthday, loops)
//   A = "Happy birthday to you" (resolve to E)
//   B = "Happy birthday to you" (resolve to F via G)
//   C = "Happy birthday dear ___" (climax phrase)
//   D = "Happy birthday to you" (final resolution)
// ============================================================================

static void Song_HappyBirthday_ConfigureVoices(void) {
    masterVolume = 0.35f;

    // F major (no scale lock — melody is diatonic, chords use note pools)
    scaleLockEnabled = false;

    // Default envelope for FM piano feel
    noteAttack  = 0.005f;
    noteDecay   = 0.5f;
    noteSustain = 0.3f;
    noteRelease = 0.8f;
    noteVolume  = 0.40f;

    // Bright but warm filter
    noteFilterCutoff    = 0.55f;
    noteFilterResonance = 0.10f;
    noteFilterEnvAmt    = 0.10f;

    // Gentle vibrato
    noteVibratoRate  = 4.5f;
    noteVibratoDepth = 0.002f;

    notePwmRate  = 0.0f;
    notePwmDepth = 0.0f;
    noteArpEnabled = false;
}

// Pattern A: bars 1-2 of melody — "Happy birthday to you" (first time)
// Melody: pickup C C | D C F | E (held) ... pickup C C
// Bass:   F2 (bar 1) | C2 (bar 2)
// Chords: Fmaj (bar 1) | C7 (bar 2)
static void Song_HappyBirthday_PatternA(Pattern* p) {
    initPattern(p);
    for (int t = 0; t < SEQ_DRUM_TRACKS; t++) patSetDrumLength(p, t, 12);
    for (int t = 0; t < SEQ_MELODY_TRACKS; t++) patSetMelodyLength(p, SEQ_DRUM_TRACKS + t, 12);

    // Drums: jazz waltz brushes
    drum(p, 0, 0, .45);  drum(p, 0, 6, .40);
    drum(p, 1, 4, .15);  drum(p, 1,10, .15);
    drum(p, 2, 0, .30);  drum(p, 2, 2, .18);  drum(p, 2, 4, .18);
    drum(p, 2, 6, .30);  drum(p, 2, 8, .18);  drum(p, 2,10, .18);

    // Bass: F2 whole bar, C2 whole bar
    note(p, 0, 0, F2,  .50, 6);  note(p, 0, 6, Cn2, .48, 6);

    // Melody: D C F | E (held) ... pickup C C
    note(p, 1, 0, D4,  .45, 2);  note(p, 1, 2, Cn4, .42, 2);
    note(p, 1, 4, F4,  .48, 2);  note(p, 1, 6, E4,  .44, 4);
    note(p, 1,10, Cn4, .38, 1);  note(p, 1,11, Cn4, .35, 1);

    // Chords: F major → C7
    chord(p, 2, 0, F3, CHORD_TRIAD, .28, 6);

    chord(p, 2, 6, Cn3, CHORD_SEVENTH, .26, 6);
}

// Pattern B: bars 3-4 — "Happy birthday to you" (second time, resolves to F)
// Melody: D C G | F (held) ... pickup C C
// Bass:   G2+C2 (bar 1) | F2 (bar 2)
// Chords: C7 (bar 1) | F (bar 2)
static void Song_HappyBirthday_PatternB(Pattern* p) {
    initPattern(p);
    for (int t = 0; t < SEQ_DRUM_TRACKS; t++) patSetDrumLength(p, t, 12);
    for (int t = 0; t < SEQ_MELODY_TRACKS; t++) patSetMelodyLength(p, SEQ_DRUM_TRACKS + t, 12);

    // Drums: waltz + rimshot fill
    drum(p, 0, 0, .45);  drum(p, 0, 6, .40);
    drum(p, 1, 4, .15);  drum(p, 1,10, .15);
    drum(p, 2, 0, .30);  drum(p, 2, 2, .18);  drum(p, 2, 4, .18);
    drum(p, 2, 6, .30);  drum(p, 2, 8, .18);  drum(p, 2,10, .18);
    drum(p, 3, 9, .10);  patSetDrumProb(p, 3, 9, 0.5f);

    // Bass: G2→C2 | F2
    note(p, 0, 0, G2,  .48, 4);  note(p, 0, 4, Cn2, .45, 2);
    note(p, 0, 6, F2,  .50, 6);

    // Melody: D C G | F (held) ... pickup C C
    note(p, 1, 0, D4,  .45, 2);  note(p, 1, 2, Cn4, .42, 2);
    note(p, 1, 4, G4,  .50, 2);  note(p, 1, 6, F4,  .48, 4);
    note(p, 1,10, Cn4, .38, 1);  note(p, 1,11, Cn4, .35, 1);

    // Chords: C7 → F
    chord(p, 2, 0, Cn3, CHORD_SEVENTH, .26, 6);

    chord(p, 2, 6, F3, CHORD_TRIAD, .28, 6);
}

// Pattern C: bars 5-6 — "Happy birthday dear ___" (climax!)
// Melody: C5 A4 F4 F4 | E4 D4 ... pickup Bb4 Bb4
// Bass:   F2 (bar 1) | Bb2+Bb2 (bar 2)
// Chords: F7 (bar 1) | Bb (bar 2)
static void Song_HappyBirthday_PatternC(Pattern* p) {
    initPattern(p);
    for (int t = 0; t < SEQ_DRUM_TRACKS; t++) patSetDrumLength(p, t, 12);
    for (int t = 0; t < SEQ_MELODY_TRACKS; t++) patSetMelodyLength(p, SEQ_DRUM_TRACKS + t, 12);

    // Drums: more energy for climax + offbeat hats
    drum(p, 0, 0, .50);  drum(p, 0, 6, .45);
    drum(p, 1, 4, .20);  drum(p, 1,10, .18);
    drum(p, 2, 0, .35);  drum(p, 2, 1, .12);  drum(p, 2, 2, .22);
    drum(p, 2, 4, .22);  drum(p, 2, 6, .35);  drum(p, 2, 7, .12);
    drum(p, 2, 8, .22);  drum(p, 2,10, .22);

    // Bass: F2 | Bb2+Bb2
    note(p, 0, 0, F2,  .50, 6);
    note(p, 0, 6, Bb2, .48, 4);  note(p, 0, 10, Bb2, .45, 2);

    // Melody: C5 A4 F4 F4 | E4 D4 ... pickup Bb4 Bb4
    note(p, 1, 0, Cn5, .55, 2);  note(p, 1, 2, A4,  .48, 2);
    note(p, 1, 4, F4,  .45, 1);  note(p, 1, 5, F4,  .43, 1);
    note(p, 1, 6, E4,  .46, 2);  note(p, 1, 8, D4,  .44, 2);
    note(p, 1,10, Bb4, .42, 1);  note(p, 1,11, Bb4, .40, 1);

    // Chords: F7 → Bb
    chord(p, 2, 0, F3, CHORD_SEVENTH, .30, 6);

    chord(p, 2, 6, Bb3, CHORD_TRIAD, .28, 6);
}

// Pattern D: bars 7-8 — "Happy birthday to you" (final resolution)
// Melody: A4 F4 G4 | F4 (held, ending)
// Bass:   F2+C2 (bar 1) | F2 (bar 2, resolution)
// Chords: C7 (bar 1) | F (bar 2, home)
static void Song_HappyBirthday_PatternD(Pattern* p) {
    initPattern(p);
    for (int t = 0; t < SEQ_DRUM_TRACKS; t++) patSetDrumLength(p, t, 12);
    for (int t = 0; t < SEQ_MELODY_TRACKS; t++) patSetMelodyLength(p, SEQ_DRUM_TRACKS + t, 12);

    // Drums: gentle, resolving
    drum(p, 0, 0, .45);  drum(p, 0, 6, .35);
    drum(p, 1, 4, .12);  drum(p, 1,10, .10);
    drum(p, 2, 0, .28);  drum(p, 2, 2, .15);  drum(p, 2, 4, .15);
    drum(p, 2, 6, .28);  drum(p, 2, 8, .15);  drum(p, 2,10, .15);

    // Bass: F2→C2 | F2 (home)
    note(p, 0, 0, F2,  .48, 4);  note(p, 0, 4, Cn2, .45, 2);
    note(p, 0, 6, F2,  .50, 6);

    // Melody: A4 F4 G4 | F4 (resolution)
    note(p, 1, 0, A4, .48, 2);  note(p, 1, 2, F4, .45, 2);
    note(p, 1, 4, G4, .46, 2);  note(p, 1, 6, F4, .50, 6);
    patSetNoteSlide(p, SEQ_DRUM_TRACKS + 1, 6, true);

    // Chords: C7 → F (V-I home)
    chord(p, 2, 0, Cn3, CHORD_SEVENTH, .26, 6);

    chord(p, 2, 6, F3, CHORD_TRIAD, .30, 6);
}

static void Song_HappyBirthday_Load(Pattern patterns[4]) {
    Song_HappyBirthday_PatternA(&patterns[0]);
    Song_HappyBirthday_PatternB(&patterns[1]);
    Song_HappyBirthday_PatternC(&patterns[2]);
    Song_HappyBirthday_PatternD(&patterns[3]);
}

// 55 BPM to sequencer = 8th notes at 110 BPM actual waltz tempo
#define SONG_HAPPY_BIRTHDAY_BPM  55.0f

// ============================================================================
// Song: Monk's Mood — Thelonious Monk
//
// A section only (8 bars). 4/4 at 75 BPM, 16th-note resolution.
// Heavy use of melody nudge for Monk's rubato phrasing.
// Piano chords use CHORD_CUSTOM for exact voicings (tritone subs, clusters).
// Melody has sustain on long held notes.
// Bass is walking quarter notes (mostly on-grid).
// Drums: jazz ride pattern with kick on upbeats.
// ============================================================================

static void Song_MonksMood_ConfigureVoices(void) {
    masterVolume = 0.5f;
    scaleLockEnabled = false;
}

// Helper: set a custom chord voicing on chord track (track 2)
static void setCustomChord(Pattern* p, int step, int root, float vel, int gate,
                           int sustain, int n0, int n1, int n2, int n3) {
    (void)root;
    chordCustom(p, 2, step, vel, gate, n0, n1, n2, n3);
    patSetNoteSustain(p, SEQ_DRUM_TRACKS + 2, step, sustain);
}

// Standard jazz drum pattern: ride on beats + upbeats, kick on offbeats
static void monkDrums(Pattern* p, bool hasKickUpbeats) {
    drum(p, 2, 0, .71);  drum(p, 2, 4, .79);
    drum(p, 2, 8, .71);  drum(p, 2,12, .79);
    if (hasKickUpbeats) {
        drum(p, 0, 0, .47);
        drumN(p, 0, 7, .47, -5);  drumN(p, 0,15, .55, -5);
    }
}

// --- Pattern 0 (Bar 3): Opening phrase ---
// Melody: C4 Eb4 Bb4 Ab4 E4 | G4 (held into next bar)
// Bass: F2 F2 Bb1 C#2 (walking quarters)
// Chords: [Ab3 Eb4 G4 C5] | [Ab3 D4 E4 C5]
static void Song_MonksMood_Pattern0(Pattern* p) {
    initPattern(p);
    monkDrums(p, true);
    drumN(p, 2, 7, .71, -5);  drumN(p, 2,15, .71, -5);

    // Bass: walking quarters
    note(p, 0, 0, F2,  .67, 3);  note(p, 0, 4, F2,  .73, 4);
    note(p, 0, 8, Bb1, .79, 4);  note(p, 0,12, Cs2, .79, 4);

    // Melody: Monk's opening phrase with nudge for rubato
    noteN(p, 1, 0, Cn4, .64, 2, 6);   noteN(p, 1, 2, Eb4, .82, 1, 9);
    noteN(p, 1, 4, Bb4, .75, 1, 2);   noteN(p, 1, 6, Ab4, .78, 1, 3);
    noteN(p, 1, 8, E4,  .71, 3, 6);
    noteNS(p, 1,12, G4,  .72, 16, 6, 8);  // Held into next bar

    // Chords: Monk's voicings
    setCustomChord(p, 0, Ab3, .28, 8, 0, Ab3, Eb4, G4, Cn5);
    setCustomChord(p, 8, Ab3, .26, 8, 0, Ab3, D4,  E4, Cn5);
}

// --- Pattern 1 (Bar 4): Held G4, walking bass descends ---
// Melody: (G4 sustained from previous bar)
// Bass: C3 B2 A2 G2 (chromatic descent)
// Chords: [E3 B3 C4 G4] (held nearly whole bar)
static void Song_MonksMood_Pattern1(Pattern* p) {
    initPattern(p);
    monkDrums(p, true);
    drumN(p, 2, 7, .71, -5);  drumN(p, 2,15, .71, -5);

    // Bass: chromatic descent C3-B2-A2-G2
    note(p, 0, 0, Cn3, .69, 4);  note(p, 0, 4, B2, .69, 4);
    note(p, 0, 8, A2,  .58, 4);  note(p, 0,12, G2, .70, 4);

    // Melody: G4 held from previous bar — no new notes

    // Chords: [E3 B3 C4 G4] — held nearly whole bar
    setCustomChord(p, 0, E3, .30, 15, 8, E3, B3, Cn4, G4);
}

// --- Pattern 2 (Bar 5): Second phrase ---
// Melody: F4 G4 F4 E4 | G3
// Bass: D2 Ab2 G2 G2
// Chords: [C4 F4 Ab4 D5] | [B3 F4 A4 E5]
static void Song_MonksMood_Pattern2(Pattern* p) {
    initPattern(p);
    monkDrums(p, false);
    drumN(p, 0, 7, .47, -5);  drumN(p, 2, 3, .71, -5);

    // Bass
    note(p, 0, 0, D2,  .65, 4);  note(p, 0, 4, Ab2, .65, 4);
    note(p, 0, 8, G2,  .67, 3);  note(p, 0,12, G2,  .73, 4);

    // Melody: second phrase with ornaments
    noteN(p, 1, 0, F4, .78, 5, 4);    noteN(p, 1, 5, G4,  .76, 1, 10);
    noteN(p, 1, 7, F4, .72, 1, -6);
    noteNS(p, 1, 8, E4, .73, 5, 6, 4);
    noteN(p, 1,14, G3, .66, 2, -12);

    // Chords
    setCustomChord(p, 0, Cn4, .27, 8, 0, Cn4, F4, Ab4, D5);
    setCustomChord(p, 8, B3,  .27, 8, 0, B3,  F4, A4,  E5);
}

// --- Pattern 3 (Bar 6): Grace note + long held C4 ---
// Melody: B3→C4 (grace note, then held ~4 beats)
// Bass: Ab3 G3 F3 Eb3 (high register descent)
// Chords: [G3 C4 Eb4] + [Bb4 Bb5] offset
static void Song_MonksMood_Pattern3(Pattern* p) {
    initPattern(p);
    monkDrums(p, true);
    drumN(p, 2, 7, .71, -5);  drumN(p, 2,15, .71, -5);

    // Bass: high register walking
    note(p, 0, 0, Ab3, .69, 4);  note(p, 0, 4, G3,  .69, 4);
    note(p, 0, 8, F3,  .58, 4);  note(p, 0,12, Eb3, .70, 4);

    // Melody: B3 grace note → C4 held
    noteN(p, 1, 0, B3,  .72, 1, 2);
    noteNS(p, 1, 1, Cn4, .72, 14, -2, 4);

    // Chords: Cm triad + offset octave Bb
    setCustomChord(p, 0, G3,  .28, 16, 8, G3, Cn4, Eb4, -1);
    setCustomChord(p, 7, Bb4, .26,  9, 0, Bb4, Bb5, -1, -1);
}

// --- Pattern 4 (Bar 7): Third phrase with chromatic turns ---
// Melody: D4 C#4 E4 F4 E4
// Bass: Bb1 F2 A1 A1
// Chords: [D4 Ab4 C5 G5] | [C#4 G4 B4 F#5]
static void Song_MonksMood_Pattern4(Pattern* p) {
    initPattern(p);
    monkDrums(p, true);
    drumN(p, 2, 7, .71, -5);  drumN(p, 2,15, .71, -5);

    // Bass
    note(p, 0, 0, Bb1, .65, 4);  note(p, 0, 4, F2, .65, 4);
    note(p, 0, 8, A1,  .67, 3);  note(p, 0,12, A1, .73, 4);

    // Melody: chromatic ornaments
    noteN(p, 1, 0, D4,  .79, 5, 7);    noteN(p, 1, 6, Cs4, .72, 1, -12);
    noteN(p, 1, 7, E4,  .82, 1, -2);   noteN(p, 1, 9, F4,  .76, 2, -10);
    noteNS(p, 1,11, E4,  .70, 5, 2, 4);

    // Chords: Monk's tritone voicings
    setCustomChord(p, 0, D4,  .27, 8, 0, D4,  Ab4, Cn5, G5);
    setCustomChord(p, 8, Cs4, .27, 8, 0, Cs4, G4,  B4,  Fs5);
}

// --- Pattern 5 (Bar 8): Fourth phrase, chromatic approach ---
// Melody: F4 Eb4 F4 F#4 F4
// Bass: E2 E2 Eb2 Eb2
// Chords: [D4 Ab4 B4 F5] | [C#4 G4 Bb4 F5]
static void Song_MonksMood_Pattern5(Pattern* p) {
    initPattern(p);
    monkDrums(p, true);
    drumN(p, 2, 7, .71, -5);  drumN(p, 2,15, .71, -5);

    // Bass: chromatic pairs
    note(p, 0, 0, E2,  .67, 3);  note(p, 0, 4, E2,  .73, 4);
    note(p, 0, 8, Eb2, .67, 3);  note(p, 0,12, Eb2, .73, 4);

    // Melody
    noteN(p, 1, 0, F4,  .76, 5, 7);    noteN(p, 1, 6, Eb4, .69, 1, -12);
    noteN(p, 1, 7, F4,  .80, 1, -6);   noteN(p, 1, 8, Fs4, .84, 2, 10);
    noteNS(p, 1,11, F4,  .69, 4, -1, 4);

    // Chords
    setCustomChord(p, 0, D4,  .27, 8, 0, D4,  Ab4, B4,  F5);
    setCustomChord(p, 8, Cs4, .27, 8, 0, Cs4, G4,  Bb4, F5);
}

// --- Pattern 6 (Bar 9): Fifth phrase, ascending ---
// Melody: G4 F#4 G4 B4 | D4
// Bass: A1 E2 D2 A2
// Chords: [C4 G4 B4 E5] | [C4 E4 F#4 B4]
static void Song_MonksMood_Pattern6(Pattern* p) {
    initPattern(p);
    monkDrums(p, false);
    drumN(p, 0, 7, .47, -5);  drumN(p, 2, 3, .71, -5);

    // Bass
    note(p, 0, 0, A1, .65, 4);  note(p, 0, 4, E2, .65, 4);
    note(p, 0, 8, D2, .65, 4);  note(p, 0,12, A2, .65, 4);

    // Melody
    noteN(p, 1, 0, G4,  .75, 5, 6);    noteN(p, 1, 6, Fs4, .82, 1, -10);
    noteN(p, 1, 7, G4,  .79, 1, -4);
    noteNS(p, 1, 8, B4,  .76, 6, 10, 4);
    noteN(p, 1,14, D4,  .64, 2, -2);

    // Chords
    setCustomChord(p, 0, Cn4, .27, 8, 0, Cn4, G4,  B4,  E5);
    setCustomChord(p, 8, Cn4, .27, 8, 0, Cn4, E4,  Fs4, B4);
}

// --- Pattern 7 (Bar 10): Resolution — long held G4 ---
// Melody: G4 (held 3 beats)
// Bass: D2 D2 G2 G2
// Chords: [C4 F4 A4 D5] | [B3 F4 A4 E5]
// Drums: fill with tom + snare on last beats
static void Song_MonksMood_Pattern7(Pattern* p) {
    initPattern(p);
    monkDrums(p, true);
    drumN(p, 2, 7, .71, -8);  drumN(p, 2,15, .71, -8);
    drumN(p, 1,15, .47, -8);  // Fill: snare at end

    // Bass
    note(p, 0, 0, D2, .67, 3);  note(p, 0, 4, D2, .73, 4);
    note(p, 0, 8, G2, .67, 3);  note(p, 0,12, G2, .73, 4);

    // Melody: long held G4 (resolution)
    noteNS(p, 1, 0, G4, .76, 12, 6, 8);

    // Chords
    setCustomChord(p, 0, Cn4, .27, 8, 0, Cn4, F4, A4, D5);
    setCustomChord(p, 8, B3,  .27, 8, 0, B3,  F4, A4, E5);
}

static void Song_MonksMood_Load(Pattern patterns[8]) {
    Song_MonksMood_Pattern0(&patterns[0]);
    Song_MonksMood_Pattern1(&patterns[1]);
    Song_MonksMood_Pattern2(&patterns[2]);
    Song_MonksMood_Pattern3(&patterns[3]);
    Song_MonksMood_Pattern4(&patterns[4]);
    Song_MonksMood_Pattern5(&patterns[5]);
    Song_MonksMood_Pattern6(&patterns[6]);
    Song_MonksMood_Pattern7(&patterns[7]);
}

#define SONG_MONKS_MOOD_BPM  75.0f

// ============================================================================
// Song: Summertime — George Gershwin
//
// A section (8 bars). 4/4 at 120 BPM, A minor.
// Lazy jazz feel: melody mostly half-note phrases, piano comps on offbeats.
// Piano chords use CHORD_CUSTOM for exact voicings.
// Drum pattern: kick + open hihat on beats, closed hihat on 2+4.
// ============================================================================

static void Song_Summertime_ConfigureVoices(void) {
    masterVolume = 0.5f;
    scaleLockEnabled = false;
}

// Standard Summertime drum pattern: lazy swing
static void summertimeDrums(Pattern* p) {
    drum(p, 0, 0, .47);   drum(p, 0, 8, .47);
    drumN(p, 0, 7, .39, -5);
    drum(p, 2, 0, .39);   drum(p, 2, 4, .47);
    drumN(p, 2, 7, .31, -5);
    drum(p, 2, 8, .39);   drum(p, 2,12, .47);
    drumN(p, 2,15, .31, -5);
}

// --- Pattern 0 (Bar 2): Pickup bar ---
// Melody: E5 C5 (pickup at end of bar)
// Bass: (empty — intro)
// Drums: just sticks on beats
static void Song_Summertime_Pattern0(Pattern* p) {
    initPattern(p);

    // Light intro drums: sidestick only
    drum(p, 1, 0, .50);  drum(p, 1, 4, .66);
    drum(p, 1, 8, .50);  drum(p, 1,12, .66);

    // Melody: pickup notes
    noteN(p, 1, 11, E5,  .83, 3, 2);
    noteN(p, 1, 14, Cn5, .73, 2, -5);
}

// --- Pattern 1 (Bar 3): "Summertime..." — held E5 ---
// Melody: E5 (held ~5 beats, sustained into next bar)
// Bass: A1 ... E2 E2 E2
// Chords: [G3 B3 C4 E4] on offbeats
static void Song_Summertime_Pattern1(Pattern* p) {
    initPattern(p);
    summertimeDrums(p);

    // Bass
    note(p, 0, 0, A1, .58, 8);
    note(p, 0, 8, E2, .57, 2);
    noteN(p, 0, 11, E2, .54, 1, -8);
    note(p, 0, 12, E2, .50, 4);

    // Melody: held E5
    noteN(p, 1, 0, E5, .80, 5, 10);

    // Chords: Am7 voicing on offbeats
    setCustomChord(p, 3, G3, 0.25f, 1, 0, G3, B3, Cn4, E4);
    setCustomChord(p, 11, G3, 0.25f, 1, 0, G3, B3, Cn4, E4);
}

// --- Pattern 2 (Bar 4): "...and the livin' is easy" ---
// Melody: D5 C5 D5 E5 C5
// Bass: E2 ... B2 B2 B2
// Chords: [Ab3 C4 D4 F4] on offbeats
static void Song_Summertime_Pattern2(Pattern* p) {
    initPattern(p);
    summertimeDrums(p);

    // Bass
    note(p, 0, 0, E2, .63, 6);
    noteN(p, 0, 7, B2, .35, 1, -6);
    note(p, 0, 8, B2, .53, 5);
    noteN(p, 0, 15, B2, .61, 1, -6);

    // Melody: descending run
    noteN(p, 1, 3, D5,  .84, 3, 1);
    noteN(p, 1, 6, Cn5, .69, 3, -5);
    noteN(p, 1, 8, D5,  .80, 3, 8);
    noteN(p, 1,11, E5,  .69, 3, 2);
    noteN(p, 1,14, Cn5, .70, 3, -7);

    // Chords: Dm7 voicing
    setCustomChord(p, 3, Ab3, 0.25f, 1, 0, Ab3, Cn4, D4, F4);
    setCustomChord(p, 11, Ab3, 0.25f, 1, 0, Ab3, Cn4, D4, F4);
}

// --- Pattern 3 (Bar 5): "Fish are jumpin'..." ---
// Melody: A4 (held) → E4 (held long)
// Bass: A1 E2 E2 Bb2 B2
// Chords: [G3 B3 C4 E4] → [Ab3 C4 D4 F4]
static void Song_Summertime_Pattern3(Pattern* p) {
    initPattern(p);
    summertimeDrums(p);

    // Bass
    note(p, 0, 0, A1, .57, 7);
    noteN(p, 0, 7, E2, .50, 1, -8);
    note(p, 0, 8, E2, .67, 6);
    noteN(p, 0, 13, Bb2, .61, 2, 8);
    noteN(p, 0, 15, B2, .63, 1, -8);

    // Melody
    noteN(p, 1, 0, A4, .79, 7, 9);
    noteN(p, 1, 7, E4, .84, 5, 1);

    // Chords
    setCustomChord(p, 0, G3, 0.27f, 8, 0, G3, B3, Cn4, E4);
    setCustomChord(p, 11, Ab3, 0.25f, 5, 0, Ab3, Cn4, D4, F4);
}

// --- Pattern 4 (Bar 6): "...and the cotton is high" ---
// Melody: Eb5→E5 (grace note) C5
// Bass: A1 E2 A1 E2
// Chords: [A3 C4 E4 A4] → [G3 A3 C#4 E4]
static void Song_Summertime_Pattern4(Pattern* p) {
    initPattern(p);
    summertimeDrums(p);

    // Bass
    note(p, 0, 0, A1, .57, 7);
    noteN(p, 0, 7, E2, .50, 1, -8);
    note(p, 0, 8, A1, .57, 7);
    noteN(p, 0, 15, E2, .50, 1, -8);

    // Melody: blue note grace → E5, then C5
    noteN(p, 1, 10, Eb5, .79, 1, 12);
    patSetNoteSlide(p, SEQ_DRUM_TRACKS + 1, 10, true);
    noteN(p, 1, 11, E5,  .91, 2, 2);
    noteN(p, 1, 14, Cn5, .73, 2, -9);

    // Chords: Am → A7
    setCustomChord(p, 3, A3, 0.25f, 5, 0, A3, Cn4, E4, A4);
    setCustomChord(p, 11, G3, 0.23f, 1, 0, G3, A3, Cs4, E4);
}

// --- Pattern 5 (Bar 7): "Your daddy's rich..." ---
// Melody: D5 D5 (held long)
// Bass: D2 E2 F2 G2 D3
// Chords: [F3 C4 D4 A4] → [F3 B3 E4 A4]
static void Song_Summertime_Pattern5(Pattern* p) {
    initPattern(p);
    summertimeDrums(p);

    // Bass: walking up D-E-F-G-D
    note(p, 0, 0, D2, .61, 6);
    noteN(p, 0, 5, E2, .54, 1, 8);
    noteN(p, 0, 7, F2, .78, 1, -8);
    note(p, 0, 8, G2, .52, 5);
    note(p, 0, 12, D3, .57, 4);

    // Melody: D5 grace → D5 held
    noteN(p, 1, 0, D5, .83, 2, 11);
    noteN(p, 1, 3, D5, .80, 5, 10);

    // Chords: Dm7 → Bm7b5
    setCustomChord(p, 0, F3, 0.27f, 8, 0, F3, Cn4, D4, A4);
    setCustomChord(p, 11, F3, 0.25f, 5, 0, F3, B3, E4, A4);
}

// --- Pattern 6 (Bar 8): "...and your mama's good looking" ---
// Melody: C5 A4 C5 A4 C5
// Bass: C2 G2 F2 C3
// Chords: [Eb3 G3 Bb3 C4] → [Eb3 A3 D4 G4]
static void Song_Summertime_Pattern6(Pattern* p) {
    initPattern(p);
    summertimeDrums(p);

    // Bass
    note(p, 0, 0, Cn2, .57, 7);
    noteN(p, 0, 7, G2, .50, 1, -8);
    note(p, 0, 8, F2, .52, 5);
    note(p, 0, 12, Cn3, .57, 4);

    // Melody: call and response C5-A4
    noteN(p, 1, 3, Cn5, .94, 2, 3);
    noteN(p, 1, 6, A4,  .47, 2, -12);
    noteN(p, 1, 8, Cn5, .87, 3, 3);
    noteN(p, 1,11, A4,  .61, 3, -4);
    noteN(p, 1,13, Cn5, .77, 2, 10);

    // Chords: C7 → D7alt
    setCustomChord(p, 3, Eb3, 0.25f, 5, 0, Eb3, G3, Bb3, Cn4);
    setCustomChord(p, 11, Eb3, 0.25f, 5, 0, Eb3, A3, D4, G4);
}

// --- Pattern 7 (Bar 9): "So hush little baby..." — held B4 ---
// Melody: B4 (held ~6 beats)
// Bass: B1 F2 F2 F2
// Chords: [A3 B3 D4 F4] on offbeats
static void Song_Summertime_Pattern7(Pattern* p) {
    initPattern(p);
    summertimeDrums(p);

    // Bass
    note(p, 0, 0, B1, .58, 8);
    note(p, 0, 8, F2, .57, 2);
    noteN(p, 0, 11, F2, .54, 1, -8);
    note(p, 0, 12, F2, .50, 4);

    // Melody: held B4
    noteN(p, 1, 0, B4, .79, 5, 4);

    // Chords: Bdim7/E voicing
    setCustomChord(p, 3, A3, 0.23f, 1, 0, A3, B3, D4, F4);
    setCustomChord(p, 11, A3, 0.23f, 1, 0, A3, B3, D4, F4);
}

static void Song_Summertime_Load(Pattern patterns[8]) {
    Song_Summertime_Pattern0(&patterns[0]);
    Song_Summertime_Pattern1(&patterns[1]);
    Song_Summertime_Pattern2(&patterns[2]);
    Song_Summertime_Pattern3(&patterns[3]);
    Song_Summertime_Pattern4(&patterns[4]);
    Song_Summertime_Pattern5(&patterns[5]);
    Song_Summertime_Pattern6(&patterns[6]);
    Song_Summertime_Pattern7(&patterns[7]);
}

#define SONG_SUMMERTIME_BPM  120.0f

// ============================================================================
// Song: M.U.L.E. Theme
//
// 8-bit game music from the Atari/C64 classic (1983). F minor, 74 BPM.
// Tight, mechanical — no humanize, no swing. Pure grid.
//
// Structure: 8 bars
//   Bars 0-1: drums only (driving 16th note pattern)
//   Bars 2-3: drums + chromatic bass arpeggio in octaves
//   Bars 4-7: drums + bass + sawtooth melody
//
// Drums: hihat on every 16th, snare on even steps, kick on odd steps.
// Bass: ascending chromatic octave pairs (F-A-Bb-B-C-D-Eb-E), staccato.
// Melody: two phrases (A and B) with quick scale runs.
// ============================================================================

// Helper: set track lengths to 32 for 32nd note resolution
static void muleSetTrackLengths(Pattern* p) {
    for (int t = 0; t < SEQ_DRUM_TRACKS; t++) patSetDrumLength(p, t, 32);
    for (int t = 0; t < SEQ_MELODY_TRACKS; t++) patSetMelodyLength(p, SEQ_DRUM_TRACKS + t, 32);
}

// Drums at 32nd resolution: hihat every 2 steps (=16th), snare/kick alternate
static void muleDrums(Pattern* p) {
    for (int s = 0; s < 32; s += 2) {
        // Hihat on every 16th note (every 2 steps at 32nd resolution)
        patSetDrum(p, 2, s, 0.4f, 0.0f);
        // Snare on even 16ths, kick on odd 16ths
        if ((s / 2) % 2 == 0) {
            patSetDrum(p, 1, s, 0.4f, 0.0f);
        } else {
            patSetDrum(p, 0, s, 0.4f, 0.0f);
        }
    }
}

// Bass at 32nd resolution: one note every 2 steps (=16th), ascending chromatic octaves
static void muleBass(Pattern* p) {
    int bassNotes[16] = {
        F1, F2, A0, A1, Bb0, Bb1, B0, B1,
        Cn1, Cn2, D1, D2, Eb1, Eb2, E1, E2
    };
    for (int i = 0; i < 16; i++) {
        int s = i * 2;  // Every other step
        patSetNote(p, SEQ_DRUM_TRACKS + 0, s, bassNotes[i], 0.4f, 1);
    }
}

// Helper: set a melody note on track 1
static void muleNote(Pattern* p, int step, int n, int gate) {
    patSetNote(p, SEQ_DRUM_TRACKS + 1, step, n, 0.4f, gate);
}

// Melody phrase A (bars 4 and 6): full 32nd note resolution
// C-C-C→F(held), C→F→A→G→F→Eb, Eb-Eb-Eb→Bb(held), Eb→G→Bb→Ab→G
static void muleMelodyA(Pattern* p) {
    // Grace notes + held F
    muleNote(p,  0, Cn3, 1);  // grace 1
    muleNote(p,  2, Cn3, 1);  // grace 2
    muleNote(p,  3, Cn3, 1);  // grace 3 (the 32nd note!)
    muleNote(p,  4, F3,  4);  // main held note
    // Descending run: C→F→A→G→F
    muleNote(p, 10, Cn3, 1);  // pickup
    muleNote(p, 11, F3,  2);
    muleNote(p, 12, A3,  1);
    muleNote(p, 14, G3,  2);
    muleNote(p, 15, F3,  1);
    // Landing + grace notes + held Bb
    muleNote(p, 16, Eb3, 1);
    muleNote(p, 18, Eb3, 1);  // grace 2
    muleNote(p, 19, Eb3, 1);  // grace 3 (32nd)
    muleNote(p, 20, Bb3, 4);  // main held note
    // Ascending run: Eb→G→Bb→Ab→G
    muleNote(p, 26, Eb3, 1);  // pickup
    muleNote(p, 27, G3,  2);
    muleNote(p, 28, Bb3, 1);
    muleNote(p, 30, Ab3, 2);
    muleNote(p, 31, G3,  1);
}

// Melody phrase A2 (bar 5): same opening, ends on held Eb3
static void muleMelodyA2(Pattern* p) {
    muleNote(p,  0, Cn3, 1);
    muleNote(p,  2, Cn3, 1);
    muleNote(p,  3, Cn3, 1);
    muleNote(p,  4, F3,  4);
    muleNote(p, 10, Cn3, 1);
    muleNote(p, 11, F3,  2);
    muleNote(p, 12, A3,  1);
    muleNote(p, 14, G3,  2);
    muleNote(p, 15, F3,  1);
    muleNote(p, 16, Eb3, 6);  // held to end
}

// Melody phrase B (bar 7): variation starting on F3, ascending to C4
static void muleMelodyB(Pattern* p) {
    muleNote(p,  0, F3,  1);
    muleNote(p,  2, F3,  1);
    muleNote(p,  3, F3,  1);
    muleNote(p,  4, Cn4, 4);
    muleNote(p, 10, F3,  1);
    muleNote(p, 11, A3,  2);
    muleNote(p, 12, Cn4, 1);
    muleNote(p, 14, Bb3, 2);
    muleNote(p, 15, A3,  1);
    muleNote(p, 16, G3,  6);  // held to end
}

static void Song_Mule_ConfigureVoices(void) {
    // Pure monophonic melody — no note pool config needed.
    // Voice setup (callbacks, volumes) handled in the play function.
    (void)0;
}

static void Song_Mule_Pattern0(Pattern* p) {
    initPattern(p);
    muleSetTrackLengths(p);
    muleDrums(p);
}

static void Song_Mule_Pattern1(Pattern* p) {
    initPattern(p);
    muleSetTrackLengths(p);
    muleDrums(p);
}

static void Song_Mule_Pattern2(Pattern* p) {
    initPattern(p);
    muleSetTrackLengths(p);
    muleDrums(p);
    muleBass(p);
}

static void Song_Mule_Pattern3(Pattern* p) {
    initPattern(p);
    muleSetTrackLengths(p);
    muleDrums(p);
    muleBass(p);
}

static void Song_Mule_Pattern4(Pattern* p) {
    initPattern(p);
    muleSetTrackLengths(p);
    muleDrums(p);
    muleBass(p);
    muleMelodyA(p);
}

static void Song_Mule_Pattern5(Pattern* p) {
    initPattern(p);
    muleSetTrackLengths(p);
    muleDrums(p);
    muleBass(p);
    muleMelodyA2(p);
}

static void Song_Mule_Pattern6(Pattern* p) {
    initPattern(p);
    muleSetTrackLengths(p);
    muleDrums(p);
    muleBass(p);
    muleMelodyA(p);
}

static void Song_Mule_Pattern7(Pattern* p) {
    initPattern(p);
    muleSetTrackLengths(p);
    muleDrums(p);
    muleBass(p);
    muleMelodyB(p);
}

static void Song_Mule_Load(Pattern patterns[8]) {
    Song_Mule_Pattern0(&patterns[0]);
    Song_Mule_Pattern1(&patterns[1]);
    Song_Mule_Pattern2(&patterns[2]);
    Song_Mule_Pattern3(&patterns[3]);
    Song_Mule_Pattern4(&patterns[4]);
    Song_Mule_Pattern5(&patterns[5]);
    Song_Mule_Pattern6(&patterns[6]);
    Song_Mule_Pattern7(&patterns[7]);
}

#define SONG_MULE_BPM  74.0f

// ============================================================================
// Song: Gymnopédie No. 1 (Satie)
//
// D major, 3/4 waltz, ~80 BPM. Dreamy, floating.
// 8th-note resolution: trackLength=12 (2 bars of 3/4), BPM halved to 40.
// Step mapping: bar1 beat 0/1/2 → steps 0/2/4, bar2 → steps 6/8/10.
//
// Bars 0-15 (first half): 2 bars intro chords, then A melody, gap, A' repeat.
// Track 0: bass (LH beat 1 single notes)
// Track 1: melody (RH)
// Track 2: chords (LH beat 2 triads, CHORD_CUSTOM + PICK_ALL)
// ============================================================================

static void Song_Gymnopedie_ConfigureVoices(void) {
    masterVolume = 0.30f;

    scaleLockEnabled = false;

    // Soft, dreamy piano — slow attack, long release
    noteAttack  = 0.02f;
    noteDecay   = 0.8f;
    noteSustain = 0.4f;
    noteRelease = 1.5f;
    noteExpRelease = true;   // natural decay tail
    noteVolume  = 0.35f;

    // Warm, dark filter
    noteFilterCutoff    = 0.40f;
    noteFilterResonance = 0.05f;
    noteFilterEnvAmt    = 0.08f;

    // Very gentle vibrato
    noteVibratoRate  = 3.5f;
    noteVibratoDepth = 0.001f;

    notePwmRate  = 0.0f;
    notePwmDepth = 0.0f;
    noteArpEnabled = false;
}

// Helper: set up Gymnopédie waltz track lengths (12 = 2 bars of 3/4)
static void satieTrackLengths(Pattern* p) {
    for (int t = 0; t < SEQ_DRUM_TRACKS; t++) patSetDrumLength(p, t, 12);
    for (int t = 0; t < SEQ_MELODY_TRACKS; t++) patSetMelodyLength(p, SEQ_DRUM_TRACKS + t, 12);
}

// Helper: custom chord on track 2 (3-note voicing, Satie uses triads)
static void satieChord(Pattern* p, int step, int root, int n0, int n1, int n2) {
    (void)root;
    chordCustom(p, 2, step, 0.22f, 4, n0, n1, n2, -1);
    patSetNoteSustain(p, SEQ_DRUM_TRACKS + 2, step, 2);
}

// Pattern 0 (bars 0-1): Intro — chords only, no melody
// Bar 0: G2 bass, Bm chord (B3 D4 F#4)
// Bar 1: D2 bass, A chord (A3 C#4 F#4)
static void Song_Gymnopedie_Pattern0(Pattern* p) {
    initPattern(p);
    satieTrackLengths(p);

    // Bass
    note(p, 0, 0, G2,  .32, 4);
    note(p, 0, 6, D2,  .30, 4);

    // Chords on beat 2 (step 2 and 8)
    satieChord(p, 2, B3, B3, D4, Fs4);
    satieChord(p, 8, A3, A3, Cs4, Fs4);
}

// Pattern 1 (bars 2-3): Intro repeat
static void Song_Gymnopedie_Pattern1(Pattern* p) {
    initPattern(p);
    satieTrackLengths(p);

    note(p, 0, 0, G2,  .32, 4);
    note(p, 0, 6, D2,  .30, 4);

    satieChord(p, 2, B3, B3, D4, Fs4);
    satieChord(p, 8, A3, A3, Cs4, Fs4);
}

// Pattern 2 (bars 4-5): Melody begins — F#5, A5 | G5, F#5, C#5
// Bar 4: G2 bass, Bm chord, melody F#5 (beat 1) A5 (beat 2)
// Bar 5: D2 bass, A chord, melody G5 (beat 0) F#5 (beat 1) C#5 (beat 2)
static void Song_Gymnopedie_Pattern2(Pattern* p) {
    initPattern(p);
    satieTrackLengths(p);

    note(p, 0, 0, G2,  .32, 4);
    note(p, 0, 6, D2,  .30, 4);

    satieChord(p, 2, B3, B3, D4, Fs4);
    satieChord(p, 8, A3, A3, Cs4, Fs4);

    // Melody (octave 4)
    note(p, 1, 2, Fs4, .38, 2);    // F#4 beat 1
    note(p, 1, 4, A4,  .48, 2);    // A4 beat 2
    note(p, 1, 6, G4,  .44, 2);    // G4 beat 0 (bar 5)
    note(p, 1, 8, Fs4, .42, 2);    // F#4 beat 1
    note(p, 1,10, Cs4, .37, 2);    // C#4 beat 2
}

// Pattern 3 (bars 6-7): B3, C#4, D4 | A3 (held)
// Bar 6: G2 bass, Bm chord
// Bar 7: D2 bass, A chord
static void Song_Gymnopedie_Pattern3(Pattern* p) {
    initPattern(p);
    satieTrackLengths(p);

    note(p, 0, 0, G2,  .34, 4);
    note(p, 0, 6, D2,  .32, 4);

    satieChord(p, 2, B3, B3, D4, Fs4);
    satieChord(p, 8, A3, A3, Cs4, Fs4);

    // Melody
    note(p, 1, 0, B3,  .42, 2);    // B3 beat 0
    note(p, 1, 2, Cs4, .45, 2);    // C#4 beat 1
    note(p, 1, 4, D4,  .48, 2);    // D4 beat 2
    noteS(p, 1, 6, A3, .40, 6, 6); // A3 held through bar 7
}

// Pattern 4 (bars 8-9): F#3 held long, then silence
// Bar 8: G2 bass, Bm chord
// Bar 9: D2 bass, A chord
static void Song_Gymnopedie_Pattern4(Pattern* p) {
    initPattern(p);
    satieTrackLengths(p);

    note(p, 0, 0, G2,  .32, 4);
    note(p, 0, 6, D2,  .30, 4);

    satieChord(p, 2, B3, B3, D4, Fs4);
    satieChord(p, 8, A3, A3, Cs4, Fs4);

    // Melody: F#3 held for most of these 2 bars
    noteS(p, 1, 0, Fs3, .38, 6, 6);
}

// Pattern 5 (bars 10-11): Waltz continues, no melody (sustained F#4 rings)
// Bar 10: G2 bass, Bm chord
// Bar 11: D2 bass, A chord
static void Song_Gymnopedie_Pattern5(Pattern* p) {
    initPattern(p);
    satieTrackLengths(p);

    note(p, 0, 0, G2,  .34, 4);
    note(p, 0, 6, D2,  .32, 4);

    satieChord(p, 2, B3, B3, D4, Fs4);
    satieChord(p, 8, A3, A3, Cs4, Fs4);
}

// Pattern 6 (bars 12-13): A' repeat — F#5, A5 | G5, F#5, C#5
static void Song_Gymnopedie_Pattern6(Pattern* p) {
    initPattern(p);
    satieTrackLengths(p);

    note(p, 0, 0, G2,  .34, 4);
    note(p, 0, 6, D2,  .32, 4);

    satieChord(p, 2, B3, B3, D4, Fs4);
    satieChord(p, 8, A3, A3, Cs4, Fs4);

    // Melody (same as pattern 2, octave 4)
    note(p, 1, 2, Fs4, .38, 2);
    note(p, 1, 4, A4,  .48, 2);
    note(p, 1, 6, G4,  .43, 2);
    note(p, 1, 8, Fs4, .42, 2);
    note(p, 1,10, Cs4, .36, 2);
}

// Pattern 7 (bars 14-15): B3, C#4, D4 | A3 (held) — resolves, loops back
static void Song_Gymnopedie_Pattern7(Pattern* p) {
    initPattern(p);
    satieTrackLengths(p);

    note(p, 0, 0, G2,  .32, 4);
    note(p, 0, 6, D2,  .30, 4);

    satieChord(p, 2, B3, B3, D4, Fs4);
    satieChord(p, 8, A3, A3, Cs4, Fs4);

    // Melody
    note(p, 1, 0, B3,  .42, 2);
    note(p, 1, 2, Cs4, .47, 2);
    note(p, 1, 4, D4,  .46, 2);
    noteS(p, 1, 6, A3, .38, 6, 6);
}

static void Song_Gymnopedie_Load(Pattern patterns[8]) {
    Song_Gymnopedie_Pattern0(&patterns[0]);
    Song_Gymnopedie_Pattern1(&patterns[1]);
    Song_Gymnopedie_Pattern2(&patterns[2]);
    Song_Gymnopedie_Pattern3(&patterns[3]);
    Song_Gymnopedie_Pattern4(&patterns[4]);
    Song_Gymnopedie_Pattern5(&patterns[5]);
    Song_Gymnopedie_Pattern6(&patterns[6]);
    Song_Gymnopedie_Pattern7(&patterns[7]);
}

#define SONG_GYMNOPEDIE_BPM  40.0f  // actual 80 BPM, halved for 8th-note resolution

// ============================================================================
// Song: M.U.L.E. Theme v2 (from MIDI transcription)
//
// F minor, 74 BPM, 32nd note resolution.
// Faithful to the original Commodore 64 SID chip version.
//
// 3 tracks: drums (snare/kick/HH), square bass, sawtooth melody
// 8 patterns of 32 steps each (= 8 bars).
// Pattern structure: 2x drums, 2x drums+bass, 4x drums+bass+melody
//
// Key differences from v1:
// - True square wave bass (not pluck)
// - True sawtooth melody (not FM)
// - Sub-step timing in melody runs via p-lock nudges
// - Correct drum pattern (snare on beats, kick on off-beats)
// ============================================================================

static void mule2SetTrackLengths(Pattern* p) {
    for (int t = 0; t < SEQ_DRUM_TRACKS; t++) patSetDrumLength(p, t, 32);
    for (int t = 0; t < SEQ_MELODY_TRACKS; t++) patSetMelodyLength(p, SEQ_DRUM_TRACKS + t, 32);
}

// Drums: snare+HH on even steps (0,4,8..), kick+HH on odd steps (2,6,10..)
static void mule2Drums(Pattern* p) {
    for (int s = 0; s < 32; s += 2) {
        // Hihat on every 16th note
        patSetDrum(p, 2, s, 0.4f, 0.0f);
        // Snare on downbeats (even 16ths), kick on upbeats (odd 16ths)
        if ((s / 2) % 2 == 0) {
            patSetDrum(p, 1, s, 0.4f, 0.0f);
        } else {
            patSetDrum(p, 0, s, 0.4f, 0.0f);
        }
    }
}

// Bass: chromatic ascending pattern F→E in low/high octave pairs
// Every 16th note (every 2 steps at 32nd resolution), gate=1
static void mule2Bass(Pattern* p) {
    int bassNotes[16] = {
        F1, F2, A0, A1, Bb0, Bb1, B0, B1,
        Cn1, Cn2, D1, D2, Eb1, Eb2, E1, E2
    };
    for (int i = 0; i < 16; i++) {
        int s = i * 2;
        patSetNote(p, SEQ_DRUM_TRACKS + 0, s, bassNotes[i], 0.4f, 1);
    }
}

// Helper: set melody note on track 1 with nudge for sub-step timing
static void mule2Note(Pattern* p, int step, int n, int gate) {
    patSetNote(p, SEQ_DRUM_TRACKS + 1, step, n, 0.4f, gate);
}

// Helper: set melody note with half-step nudge (for sub-32nd timing in runs)
static void mule2NoteNudge(Pattern* p, int step, int n, int gate, float nudge) {
    mule2Note(p, step, n, gate);
    seqSetPLock(p, SEQ_DRUM_TRACKS + 1, step, PLOCK_TIME_NUDGE, nudge);
}

// Melody phrase A: C grace notes → F held, run C→F→A→G→F,
//                  Eb grace notes → Bb held, run Eb→G→Bb→Ab→G
static void mule2MelodyA(Pattern* p) {
    // Grace notes + held F
    mule2Note(p,  0, Cn3, 1);       // grace 1
    mule2Note(p,  2, Cn3, 1);       // grace 2 (half-step dur)
    mule2Note(p,  3, Cn3, 1);       // grace 3 (32nd!)
    mule2Note(p,  4, F3,  4);       // main held F
    // Fast descending run with sub-step timing
    mule2Note(p, 10, Cn3, 1);       // pickup
    mule2Note(p, 11, F3,  1);       // F3
    mule2NoteNudge(p, 12, A3, 1, 6.0f);  // A3 at step 12.5 (nudge +6 ticks)
    mule2NoteNudge(p, 13, G3, 1, 6.0f);  // G3 at step 13.5 (nudge +6 ticks)
    mule2Note(p, 15, F3,  1);       // landing F

    // Eb grace notes + held Bb
    mule2Note(p, 16, Eb3, 1);       // grace 1
    mule2Note(p, 18, Eb3, 1);       // grace 2
    mule2Note(p, 19, Eb3, 1);       // grace 3 (32nd!)
    mule2Note(p, 20, Bb3, 4);       // main held Bb
    // Fast ascending run with sub-step timing
    mule2Note(p, 26, Eb3, 1);       // pickup
    mule2Note(p, 27, G3,  1);       // G3
    mule2NoteNudge(p, 28, Bb3, 1, 6.0f); // Bb3 at step 28.5
    mule2NoteNudge(p, 29, Ab3, 1, 6.0f); // Ab3 at step 29.5
    mule2Note(p, 31, G3,  1);       // landing G
}

// Melody phrase A2: same opening, ends on held Eb3
static void mule2MelodyA2(Pattern* p) {
    mule2Note(p,  0, Cn3, 1);
    mule2Note(p,  2, Cn3, 1);
    mule2Note(p,  3, Cn3, 1);
    mule2Note(p,  4, F3,  4);
    mule2Note(p, 10, Cn3, 1);
    mule2Note(p, 11, F3,  1);
    mule2NoteNudge(p, 12, A3, 1, 6.0f);
    mule2NoteNudge(p, 13, G3, 1, 6.0f);
    mule2Note(p, 15, F3,  1);
    mule2Note(p, 16, Eb3, 6);       // held to end of bar
}

// Melody phrase B: higher variation F→C4, ends on held G3
static void mule2MelodyB(Pattern* p) {
    mule2Note(p,  0, F3,  1);
    mule2Note(p,  2, F3,  1);
    mule2Note(p,  3, F3,  1);
    mule2Note(p,  4, Cn4, 4);
    mule2Note(p, 10, F3,  1);
    mule2Note(p, 11, A3,  1);
    mule2NoteNudge(p, 12, Cn4, 1, 6.0f);
    mule2NoteNudge(p, 13, Bb3, 1, 6.0f);
    mule2Note(p, 15, A3,  1);
    mule2Note(p, 16, G3,  6);       // held to end of bar
}

static void Song_Mule2_ConfigureVoices(void) {
    (void)0;
}

static void Song_Mule2_Pattern0(Pattern* p) {
    initPattern(p); mule2SetTrackLengths(p); mule2Drums(p);
}
static void Song_Mule2_Pattern1(Pattern* p) {
    initPattern(p); mule2SetTrackLengths(p); mule2Drums(p);
}
static void Song_Mule2_Pattern2(Pattern* p) {
    initPattern(p); mule2SetTrackLengths(p); mule2Drums(p); mule2Bass(p);
}
static void Song_Mule2_Pattern3(Pattern* p) {
    initPattern(p); mule2SetTrackLengths(p); mule2Drums(p); mule2Bass(p);
}
static void Song_Mule2_Pattern4(Pattern* p) {
    initPattern(p); mule2SetTrackLengths(p); mule2Drums(p); mule2Bass(p);
    mule2MelodyA(p);
}
static void Song_Mule2_Pattern5(Pattern* p) {
    initPattern(p); mule2SetTrackLengths(p); mule2Drums(p); mule2Bass(p);
    mule2MelodyA2(p);
}
static void Song_Mule2_Pattern6(Pattern* p) {
    initPattern(p); mule2SetTrackLengths(p); mule2Drums(p); mule2Bass(p);
    mule2MelodyA(p);
}
static void Song_Mule2_Pattern7(Pattern* p) {
    initPattern(p); mule2SetTrackLengths(p); mule2Drums(p); mule2Bass(p);
    mule2MelodyB(p);
}

static void Song_Mule2_Load(Pattern patterns[8]) {
    Song_Mule2_Pattern0(&patterns[0]);
    Song_Mule2_Pattern1(&patterns[1]);
    Song_Mule2_Pattern2(&patterns[2]);
    Song_Mule2_Pattern3(&patterns[3]);
    Song_Mule2_Pattern4(&patterns[4]);
    Song_Mule2_Pattern5(&patterns[5]);
    Song_Mule2_Pattern6(&patterns[6]);
    Song_Mule2_Pattern7(&patterns[7]);
}

#define SONG_MULE2_BPM  74.0f

// ============================================================================
// Song: Oscar's Lo-Fi Beat
//
// Ab major, 80 BPM, lofi jazz. Inspired by Oscar Peterson on synth.
// ii-V-I turnarounds (Bbm7 → Eb7 → Abmaj7), walking bass, Peterson-style
// fast scalar runs on lead, jazzy chord comping, boom-bap drums with swing.
// Lofi treatment: tape saturation, filtered warmth, humanized timing.
//
// Patterns: intro (drums) → add bass → add chords → melody enters →
//           melody intensifies → peak → wind down → outro
// ============================================================================

// Ab major notes — extra defines for readability
#define Db2 37   // = Cs2
#define Db3 49   // = Cs3
#define Db4 61   // = Cs4
#define Db5 73   // = Cs5

static void Song_OscarLofi_ConfigureVoices(void) {
    masterVolume = 0.30f;
    scaleLockEnabled = true;
    scaleRoot = 8;                      // Ab
    scaleType = SCALE_MAJOR;
}

// --- Shared drum pattern: lofi boom-bap ---
static void oscarDrums(Pattern* p) {
    // Kick: boom-bap (beat 1, and-of-3)
    drum(p, 0, 0, 0.50);
    drum(p, 0, 7, 0.35);
    // Snare: backbeat (beats 2 and 4), slightly soft
    drumN(p, 1, 4, 0.40, 2.0);   // lazy snare
    drumN(p, 1, 12, 0.38, 1.5);
    // Hihat: 8ths, ghosted offbeats
    for (int s = 0; s < 16; s += 2) {
        float vel = (s % 4 == 0) ? 0.30f : 0.15f;
        drum(p, 2, s, vel);
    }
    // Ghost hihats on some 16ths
    drum(p, 2, 3, 0.08);
    drum(p, 2, 11, 0.08);
    // Rimshot: occasional, probabilistic
    drumP(p, 3, 10, 0.12, 0.3);
    patSetDrumProb(p, 3, 10, 0.3f);
}

// Busier drums for peak sections
static void oscarDrumsBusy(Pattern* p) {
    oscarDrums(p);
    // Extra kick ghost
    drum(p, 0, 5, 0.20);
    patSetDrumProb(p, 0, 5, 0.5f);
    // Open hat on beat 4 and-of
    drumP(p, 2, 13, 0.25, 0.2);
    // Rimshot on beat 3
    drumP(p, 3, 8, 0.15, 0.2);
    patSetDrumProb(p, 3, 8, 0.4f);
}

// --- Walking bass patterns in Ab ---

// Bass: Abmaj7 → Fm7 (steps 0-15 = 2 bars)
static void oscarBassA(Pattern* p) {
    // Bar 1 (Abmaj7): Ab C Eb G (ascending chord tones)
    note(p, 0, 0,  Ab2, .45, 2);
    note(p, 0, 2,  Cn3, .38, 2);
    note(p, 0, 4,  Eb2, .40, 2);
    note(p, 0, 6,  G2,  .35, 2);
    // Bar 2 (Fm7): F Ab C Eb (descending into next)
    note(p, 0, 8,  F2,  .45, 2);
    note(p, 0, 10, Ab2, .38, 2);
    note(p, 0, 12, Cn3, .40, 2);
    note(p, 0, 14, Eb2, .35, 2);
}

// Bass: Bbm7 → Eb7 (turnaround)
static void oscarBassB(Pattern* p) {
    // Bar 1 (Bbm7): Bb Db F Ab
    note(p, 0, 0,  Bb1, .45, 2);
    note(p, 0, 2,  Db2, .38, 2);
    note(p, 0, 4,  F2,  .40, 2);
    note(p, 0, 6,  Ab2, .35, 2);
    // Bar 2 (Eb7): Eb G Bb Db (dominant pull to Ab)
    note(p, 0, 8,  Eb2, .45, 2);
    note(p, 0, 10, G2,  .40, 2);
    note(p, 0, 12, Bb1, .38, 2);
    note(p, 0, 14, Db2, .42, 2);  // leading tone pull
}

// Bass: Abmaj7 → Dbmaj7
static void oscarBassC(Pattern* p) {
    // Bar 1 (Abmaj7): Ab Eb C G (wider motion)
    note(p, 0, 0,  Ab2, .45, 2);
    note(p, 0, 2,  Eb2, .38, 2);
    note(p, 0, 4,  Cn3, .40, 2);
    note(p, 0, 6,  G2,  .35, 2);
    // Bar 2 (Dbmaj7): Db F Ab C
    note(p, 0, 8,  Db2, .45, 2);
    note(p, 0, 10, F2,  .40, 2);
    note(p, 0, 12, Ab2, .38, 2);
    note(p, 0, 14, Cn3, .35, 2);
}

// Bass: sparse outro
static void oscarBassOutro(Pattern* p) {
    note(p, 0, 0,  Ab2, .40, 4);   // long Ab
    note(p, 0, 8,  Eb2, .30, 4);   // long Eb
    noteS(p, 0, 12, Ab2, .25, 4, 4);  // Ab held to end
}

// --- Chord voicings ---

// Abmaj7 → Fm7 comping
static void oscarChordsA(Pattern* p) {
    // Abmaj7: Ab3 C4 Eb4 G4
    chordCustom(p, 2, 0, 0.25, 4, Ab3, Cn4, Eb4, G4);
    // Fm7: F3 Ab3 C4 Eb4
    chordCustom(p, 2, 8, 0.22, 4, F3, Ab3, Cn4, Eb4);
    // Ghost stab
    chordCustom(p, 2, 6, 0.10, 1, Ab3, Cn4, -1, -1);
    patSetDrumProb(p, SEQ_DRUM_TRACKS + 2, 6, 0.4f);
}

// Bbm7 → Eb7 comping
static void oscarChordsB(Pattern* p) {
    // Bbm7: Bb3 Db4 F4 Ab4
    chordCustom(p, 2, 0, 0.25, 4, Bb3, Db4, F4, Ab4);
    // Eb7: Eb3 G3 Bb3 Db4
    chordCustom(p, 2, 8, 0.23, 4, Eb3, G3, Bb3, Db4);
    // Anticipation stab
    chordCustom(p, 2, 14, 0.12, 1, Eb3, G3, -1, -1);
}

// Abmaj7 → Dbmaj7 comping
static void oscarChordsC(Pattern* p) {
    // Abmaj7 voiced higher
    chordCustom(p, 2, 0, 0.25, 4, Cn4, Eb4, G4, Ab4);
    // Dbmaj7: Db3 F3 Ab3 C4
    chordCustom(p, 2, 8, 0.22, 4, Db3, F3, Ab3, Cn4);
    // Rhythmic stab
    chordCustom(p, 2, 5, 0.10, 1, Cn4, Eb4, -1, -1);
    patSetDrumProb(p, SEQ_DRUM_TRACKS + 2, 5, 0.5f);
}

// --- Lead melody: Peterson synth runs ---

// Melody A: opening phrase, bluesy
static void oscarMelodyA(Pattern* p) {
    // Bebop run descending: Ab5 G4 F4 Eb4 Db4 C4 Bb3 Ab3
    note(p, 1, 0,  Ab4, .42, 1);
    note(p, 1, 1,  G4,  .38, 1);
    note(p, 1, 2,  F4,  .40, 1);
    note(p, 1, 3,  Eb4, .36, 2);
    // Rest, then bluesy phrase
    note(p, 1, 6,  Db4, .35, 1);
    note(p, 1, 7,  Cn4, .38, 2);
    // Bar 2: call-response, sparser
    note(p, 1, 10, Eb4, .40, 2);
    note(p, 1, 12, F4,  .35, 1);
    noteS(p, 1, 13, Ab4, .42, 3, 2);  // held note with sustain
}

// Melody B: faster run over Bbm7 → Eb7
static void oscarMelodyB(Pattern* p) {
    // Fast ascending run: Bb3 C4 Db4 Eb4 F4 G4 Ab4
    note(p, 1, 0,  Bb3, .35, 1);
    note(p, 1, 1,  Cn4, .38, 1);
    note(p, 1, 2,  Db4, .36, 1);
    note(p, 1, 3,  Eb4, .40, 1);
    note(p, 1, 4,  F4,  .38, 1);
    note(p, 1, 5,  G4,  .42, 2);
    // Octave leap — classic Peterson
    note(p, 1, 8,  Ab4, .45, 1);
    note(p, 1, 9,  Ab3, .35, 1);  // octave down
    note(p, 1, 10, Bb3, .38, 2);
    // Resolution phrase
    note(p, 1, 13, G3,  .32, 1);
    noteS(p, 1, 14, Ab3, .40, 2, 2);
}

// Melody C: peak intensity, double-time feel
static void oscarMelodyC(Pattern* p) {
    // Chromatic approach: G3→Ab3, then run up
    note(p, 1, 0,  G3,  .30, 1);
    note(p, 1, 1,  Ab3, .42, 1);
    note(p, 1, 2,  Bb3, .38, 1);
    note(p, 1, 3,  Cn4, .40, 1);
    note(p, 1, 4,  Db4, .38, 1);
    note(p, 1, 5,  Eb4, .42, 1);
    note(p, 1, 6,  F4,  .40, 1);
    note(p, 1, 7,  G4,  .45, 1);
    // Peak: high Ab with slide down
    note(p, 1, 8,  Ab4, .50, 2);
    patSetNoteSlide(p, SEQ_DRUM_TRACKS + 1, 10, true);
    note(p, 1, 10, F4,  .38, 1);
    note(p, 1, 11, Eb4, .40, 1);
    note(p, 1, 12, Db4, .35, 1);
    note(p, 1, 13, Cn4, .38, 1);
    noteS(p, 1, 14, Ab3, .42, 2, 2);
}

// Melody D: sparse, reflective outro phrase
static void oscarMelodyD(Pattern* p) {
    noteS(p, 1, 0,  Eb4, .35, 4, 2);   // long held Eb
    note(p, 1, 6,  Db4, .28, 2);
    noteS(p, 1, 10, Ab3, .30, 4, 4);    // fading Ab
}

// --- Patterns ---

// Pat 0: Intro — drums only
static void Song_OscarLofi_Pattern0(Pattern* p) {
    initPattern(p);
    oscarDrums(p);
}

// Pat 1: Drums + walking bass (Abmaj7 → Fm7)
static void Song_OscarLofi_Pattern1(Pattern* p) {
    initPattern(p);
    oscarDrums(p);
    oscarBassA(p);
}

// Pat 2: Full groove (Abmaj7 → Fm7 + chords)
static void Song_OscarLofi_Pattern2(Pattern* p) {
    initPattern(p);
    oscarDrums(p);
    oscarBassA(p);
    oscarChordsA(p);
}

// Pat 3: Turnaround (Bbm7 → Eb7 + chords)
static void Song_OscarLofi_Pattern3(Pattern* p) {
    initPattern(p);
    oscarDrums(p);
    oscarBassB(p);
    oscarChordsB(p);
}

// Pat 4: Melody enters (Abmaj7 → Fm7)
static void Song_OscarLofi_Pattern4(Pattern* p) {
    initPattern(p);
    oscarDrums(p);
    oscarBassA(p);
    oscarChordsA(p);
    oscarMelodyA(p);
}

// Pat 5: Melody B (Bbm7 → Eb7, faster runs)
static void Song_OscarLofi_Pattern5(Pattern* p) {
    initPattern(p);
    oscarDrums(p);
    oscarBassB(p);
    oscarChordsB(p);
    oscarMelodyB(p);
}

// Pat 6: Peak (Abmaj7 → Dbmaj7, intense)
static void Song_OscarLofi_Pattern6(Pattern* p) {
    initPattern(p);
    oscarDrumsBusy(p);
    oscarBassC(p);
    oscarChordsC(p);
    oscarMelodyC(p);
}

// Pat 7: Outro (sparse, fading)
static void Song_OscarLofi_Pattern7(Pattern* p) {
    initPattern(p);
    // Minimal drums
    drum(p, 0, 0, 0.30);
    drum(p, 2, 4, 0.12);
    drum(p, 2, 12, 0.10);
    oscarBassOutro(p);
    // Sparse final chord
    chordCustom(p, 2, 0, 0.18, 8, Ab3, Cn4, Eb4, G4);
    oscarMelodyD(p);
}

static void Song_OscarLofi_Load(Pattern patterns[8]) {
    Song_OscarLofi_Pattern0(&patterns[0]);
    Song_OscarLofi_Pattern1(&patterns[1]);
    Song_OscarLofi_Pattern2(&patterns[2]);
    Song_OscarLofi_Pattern3(&patterns[3]);
    Song_OscarLofi_Pattern4(&patterns[4]);
    Song_OscarLofi_Pattern5(&patterns[5]);
    Song_OscarLofi_Pattern6(&patterns[6]);
    Song_OscarLofi_Pattern7(&patterns[7]);
}

#define SONG_OSCAR_LOFI_BPM  80.0f

// ============================================================================
// Song: Dreamer (Mac DeMarco — Chamber of Reflection style)
//
// D minor, 78 BPM. Hypnotic synth pad drone, simple repeating bass,
// minimal drums, a 4-note hook that won't leave your head.
// Heavy chorus, tape wobble, reverb — everything slightly wobbly and warm.
//
// Chord loop: Dm → Dm → Bb → A (i → i → VI → V)
// The bass just pulses roots. The hook is a simple descending phrase.
// ============================================================================

static void Song_Dreamer_ConfigureVoices(void) {
    masterVolume = 0.28f;
    scaleLockEnabled = true;
    scaleRoot = 2;                      // D
    scaleType = SCALE_MINOR;
}

// Minimal drums — brushy, hypnotic
static void dreamerDrums(Pattern* p) {
    // Kick: just beat 1 and a ghost on the and-of-3
    drum(p, 0, 0, 0.40);
    drum(p, 0, 7, 0.18);
    patSetDrumProb(p, 0, 7, 0.6f);
    // Snare: ghostly rimshot on 2 and 4
    drumP(p, 1, 4, 0.25, 0.2);
    drumP(p, 1, 12, 0.22, 0.2);
    // Hihat: quiet 8ths, barely there
    for (int s = 0; s < 16; s += 2) {
        drum(p, 2, s, (s % 4 == 0) ? 0.18f : 0.08f);
    }
}

// Even sparser drums for intro/outro
static void dreamerDrumsSparse(Pattern* p) {
    drum(p, 0, 0, 0.30);
    drum(p, 2, 4, 0.10);
    drum(p, 2, 12, 0.08);
}

// Bass: Dm → Dm (just pulsing D)
static void dreamerBassDm(Pattern* p) {
    note(p, 0, 0,  D2, .42, 4);
    note(p, 0, 4,  D2, .30, 2);
    note(p, 0, 8,  D2, .42, 4);
    note(p, 0, 12, D2, .30, 2);
}

// Bass: Bb → A
static void dreamerBassBbA(Pattern* p) {
    note(p, 0, 0,  Bb1, .42, 4);
    note(p, 0, 4,  Bb1, .30, 2);
    note(p, 0, 8,  A1,  .45, 4);
    note(p, 0, 12, A1,  .32, 2);
}

// Chords: Dm pad (D3 F3 A3)
static void dreamerChordDm(Pattern* p) {
    chordCustom(p, 2, 0, 0.20, 8, D3, F3, A3, -1);
    chordCustom(p, 2, 8, 0.18, 8, D3, F3, A3, -1);
}

// Chords: Bb → A (Bb3 D4 F4 → A3 C#4 E4)
static void dreamerChordBbA(Pattern* p) {
    chordCustom(p, 2, 0, 0.20, 8, Bb3, D4, F4, -1);
    chordCustom(p, 2, 8, 0.22, 8, A3, Cs4, E4, -1);
}

// The Hook — 4-note descending phrase, hypnotic repetition
// A4 → G4 → F4 → D4 (classic minor descent)
static void dreamerHook(Pattern* p) {
    note(p, 1, 0,  A4,  .38, 2);
    note(p, 1, 3,  G4,  .35, 2);
    note(p, 1, 6,  F4,  .38, 3);
    noteS(p, 1, 10, D4,  .40, 4, 2);  // held — the resolution
}

// Hook variation — add a turn at the end
static void dreamerHookVar(Pattern* p) {
    note(p, 1, 0,  A4,  .38, 2);
    note(p, 1, 3,  G4,  .35, 2);
    note(p, 1, 6,  F4,  .38, 2);
    note(p, 1, 9,  D4,  .35, 1);
    note(p, 1, 10, E4,  .32, 1);  // little turn
    noteS(p, 1, 12, D4,  .40, 4, 2);
}

// High answer phrase over Bb → A
static void dreamerAnswer(Pattern* p) {
    note(p, 1, 2,  D5,  .35, 2);
    note(p, 1, 5,  Cn5, .32, 2);
    noteS(p, 1, 8,  A4,  .38, 4, 2);
    // Chromatic slide to resolution
    note(p, 1, 13, Gs4, .28, 1);
    noteS(p, 1, 14, A4,  .35, 2, 2);
    patSetNoteSlide(p, SEQ_DRUM_TRACKS + 1, 14, true);
}

// Pat 0: Intro — pad drone, barely any drums
static void Song_Dreamer_Pattern0(Pattern* p) {
    initPattern(p);
    dreamerDrumsSparse(p);
    dreamerChordDm(p);
}

// Pat 1: Bass enters (Dm)
static void Song_Dreamer_Pattern1(Pattern* p) {
    initPattern(p);
    dreamerDrumsSparse(p);
    dreamerBassDm(p);
    dreamerChordDm(p);
}

// Pat 2: Full groove Dm + hook
static void Song_Dreamer_Pattern2(Pattern* p) {
    initPattern(p);
    dreamerDrums(p);
    dreamerBassDm(p);
    dreamerChordDm(p);
    dreamerHook(p);
}

// Pat 3: Bb → A with answer phrase
static void Song_Dreamer_Pattern3(Pattern* p) {
    initPattern(p);
    dreamerDrums(p);
    dreamerBassBbA(p);
    dreamerChordBbA(p);
    dreamerAnswer(p);
}

// Pat 4: Back to Dm, hook variation
static void Song_Dreamer_Pattern4(Pattern* p) {
    initPattern(p);
    dreamerDrums(p);
    dreamerBassDm(p);
    dreamerChordDm(p);
    dreamerHookVar(p);
}

// Pat 5: Bb → A again, answer
static void Song_Dreamer_Pattern5(Pattern* p) {
    initPattern(p);
    dreamerDrums(p);
    dreamerBassBbA(p);
    dreamerChordBbA(p);
    dreamerAnswer(p);
}

// Pat 6: Dm — hook + answer combined (climax)
static void Song_Dreamer_Pattern6(Pattern* p) {
    initPattern(p);
    dreamerDrums(p);
    dreamerBassDm(p);
    dreamerChordDm(p);
    // Combined: hook in first half, answer note at end
    note(p, 1, 0,  A4,  .42, 2);
    note(p, 1, 3,  G4,  .38, 2);
    note(p, 1, 6,  F4,  .40, 1);
    note(p, 1, 7,  E4,  .35, 1);
    note(p, 1, 8,  D4,  .38, 1);
    note(p, 1, 9,  E4,  .35, 1);
    note(p, 1, 10, F4,  .38, 1);
    noteS(p, 1, 12, D4,  .42, 4, 2);
}

// Pat 7: Outro — pad fading, sparse
static void Song_Dreamer_Pattern7(Pattern* p) {
    initPattern(p);
    dreamerDrumsSparse(p);
    noteS(p, 0, 0, D2, .30, 8, 8);   // one long bass D
    chordCustom(p, 2, 0, 0.15, 16, D3, F3, A3, -1);  // held chord
    noteS(p, 1, 4, A4, .25, 4, 8);   // one last echo of the hook
}

static void Song_Dreamer_Load(Pattern patterns[8]) {
    Song_Dreamer_Pattern0(&patterns[0]);
    Song_Dreamer_Pattern1(&patterns[1]);
    Song_Dreamer_Pattern2(&patterns[2]);
    Song_Dreamer_Pattern3(&patterns[3]);
    Song_Dreamer_Pattern4(&patterns[4]);
    Song_Dreamer_Pattern5(&patterns[5]);
    Song_Dreamer_Pattern6(&patterns[6]);
    Song_Dreamer_Pattern7(&patterns[7]);
}

#define SONG_DREAMER_BPM  78.0f

// ============================================================================
// Song: Salad Daze (Mac DeMarco — Salad Days style)
//
// G major, 82 BPM. Sunny, breezy, jangly. The hook is a bouncy
// ascending phrase that feels like a lazy summer afternoon.
// Chorus-drenched keys, slightly detuned, everything warm and golden.
//
// Chord loop: G → Em → C → D (I → vi → IV → V)
// Classic pop turnaround, but with that Mac wobble.
// ============================================================================

static void Song_SaladDaze_ConfigureVoices(void) {
    masterVolume = 0.30f;
    scaleLockEnabled = true;
    scaleRoot = 7;                      // G
    scaleType = SCALE_MAJOR;
}

// Drums: lighter, more open feel
static void saladDrums(Pattern* p) {
    // Kick: beats 1 and 3
    drum(p, 0, 0, 0.42);
    drum(p, 0, 8, 0.38);
    // Snare: 2 and 4, slightly ahead (not lazy — breezy)
    drumN(p, 1, 4, 0.35, -1.0);
    drumN(p, 1, 12, 0.33, -1.0);
    // Hihat: 8ths with open hat on some offbeats
    for (int s = 0; s < 16; s += 2) {
        drum(p, 2, s, (s % 4 == 0) ? 0.22f : 0.12f);
    }
    // Open hat feel on beat 4-and
    drumP(p, 2, 13, 0.18, 0.2);
    patSetDrumProb(p, 2, 13, 0.5f);
}

static void saladDrumsSparse(Pattern* p) {
    drum(p, 0, 0, 0.35);
    drum(p, 2, 4, 0.12);
    drum(p, 2, 8, 0.10);
    drum(p, 2, 12, 0.10);
}

// Bass: G → Em
static void saladBassGEm(Pattern* p) {
    // Bar 1 (G): root bounce
    note(p, 0, 0,  G2,  .42, 2);
    note(p, 0, 4,  B2,  .32, 2);
    // Bar 2 (Em): E F# walk
    note(p, 0, 8,  E2,  .42, 2);
    note(p, 0, 12, Fs2, .32, 2);
}

// Bass: C → D
static void saladBassCD(Pattern* p) {
    // Bar 1 (C): root
    note(p, 0, 0,  Cn3, .42, 2);
    note(p, 0, 4,  E2,  .32, 2);
    // Bar 2 (D): D with chromatic approach
    note(p, 0, 8,  D2,  .42, 2);
    note(p, 0, 12, Fs2, .35, 2);
}

// Chords: G → Em
static void saladChordsGEm(Pattern* p) {
    // G: G3 B3 D4
    chordCustom(p, 2, 0, 0.22, 6, G3, B3, D4, -1);
    // Em: E3 G3 B3
    chordCustom(p, 2, 8, 0.20, 6, E3, G3, B3, -1);
    // Little rhythmic stab
    chordCustom(p, 2, 14, 0.10, 1, G3, B3, -1, -1);
    patSetDrumProb(p, SEQ_DRUM_TRACKS + 2, 14, 0.4f);
}

// Chords: C → D
static void saladChordsCD(Pattern* p) {
    // C: C3 E3 G3
    chordCustom(p, 2, 0, 0.22, 6, Cn3, E3, G3, -1);
    // D: D3 F#3 A3
    chordCustom(p, 2, 8, 0.22, 6, D3, Fs3, A3, -1);
}

// The Hook — bouncy ascending phrase, sunny and catchy
// D4 → E4 → G4 → A4 (pentatonic bounce, very Mac)
static void saladHook(Pattern* p) {
    note(p, 1, 0,  D4,  .38, 2);
    note(p, 1, 2,  E4,  .35, 1);
    note(p, 1, 4,  G4,  .40, 3);
    noteS(p, 1, 8, A4,  .42, 4, 2);  // held — the payoff
    // Little tail
    note(p, 1, 13, G4,  .28, 1);
    note(p, 1, 14, E4,  .30, 2);
}

// Hook variation — descending answer
static void saladHookVar(Pattern* p) {
    note(p, 1, 0,  A4,  .40, 2);
    note(p, 1, 2,  G4,  .35, 2);
    note(p, 1, 5,  E4,  .38, 2);
    note(p, 1, 8,  D4,  .35, 2);
    // Bounce back up
    note(p, 1, 11, E4,  .32, 1);
    noteS(p, 1, 12, G4,  .38, 4, 2);
}

// Bridge melody — higher, more yearning
static void saladBridge(Pattern* p) {
    note(p, 1, 0,  B4,  .40, 2);
    note(p, 1, 3,  A4,  .35, 1);
    note(p, 1, 4,  G4,  .38, 2);
    note(p, 1, 7,  A4,  .35, 2);
    noteS(p, 1, 10, B4,  .42, 4, 2);
    note(p, 1, 15, A4,  .28, 1);
}

// Pat 0: Intro — chords only, dreamy
static void Song_SaladDaze_Pattern0(Pattern* p) {
    initPattern(p);
    saladDrumsSparse(p);
    saladChordsGEm(p);
}

// Pat 1: Bass enters
static void Song_SaladDaze_Pattern1(Pattern* p) {
    initPattern(p);
    saladDrumsSparse(p);
    saladBassGEm(p);
    saladChordsGEm(p);
}

// Pat 2: Full groove G → Em + hook
static void Song_SaladDaze_Pattern2(Pattern* p) {
    initPattern(p);
    saladDrums(p);
    saladBassGEm(p);
    saladChordsGEm(p);
    saladHook(p);
}

// Pat 3: C → D with hook variation
static void Song_SaladDaze_Pattern3(Pattern* p) {
    initPattern(p);
    saladDrums(p);
    saladBassCD(p);
    saladChordsCD(p);
    saladHookVar(p);
}

// Pat 4: Back to G → Em, hook again (verse 2 feel)
static void Song_SaladDaze_Pattern4(Pattern* p) {
    initPattern(p);
    saladDrums(p);
    saladBassGEm(p);
    saladChordsGEm(p);
    saladHook(p);
}

// Pat 5: C → D bridge melody
static void Song_SaladDaze_Pattern5(Pattern* p) {
    initPattern(p);
    saladDrums(p);
    saladBassCD(p);
    saladChordsCD(p);
    saladBridge(p);
}

// Pat 6: G → Em climax — hook + octave doubling
static void Song_SaladDaze_Pattern6(Pattern* p) {
    initPattern(p);
    saladDrums(p);
    saladBassGEm(p);
    saladChordsGEm(p);
    // Hook with octave leap at the peak
    note(p, 1, 0,  D4,  .40, 2);
    note(p, 1, 2,  E4,  .38, 1);
    note(p, 1, 4,  G4,  .42, 2);
    note(p, 1, 7,  A4,  .40, 1);
    note(p, 1, 8,  B4,  .45, 2);   // peak!
    note(p, 1, 10, A4,  .38, 1);
    note(p, 1, 11, G4,  .35, 1);
    noteS(p, 1, 12, E4,  .38, 4, 2);
}

// Pat 7: Outro — sparse, fading
static void Song_SaladDaze_Pattern7(Pattern* p) {
    initPattern(p);
    saladDrumsSparse(p);
    noteS(p, 0, 0, G2, .28, 8, 8);   // one long G
    chordCustom(p, 2, 0, 0.15, 16, G3, B3, D4, -1);
    // Last echo of the hook
    noteS(p, 1, 2, G4,  .25, 4, 4);
    noteS(p, 1, 10, D4,  .20, 4, 4);
}

static void Song_SaladDaze_Load(Pattern patterns[8]) {
    Song_SaladDaze_Pattern0(&patterns[0]);
    Song_SaladDaze_Pattern1(&patterns[1]);
    Song_SaladDaze_Pattern2(&patterns[2]);
    Song_SaladDaze_Pattern3(&patterns[3]);
    Song_SaladDaze_Pattern4(&patterns[4]);
    Song_SaladDaze_Pattern5(&patterns[5]);
    Song_SaladDaze_Pattern6(&patterns[6]);
    Song_SaladDaze_Pattern7(&patterns[7]);
}

#define SONG_SALAD_DAZE_BPM  82.0f

// ============================================================================
// Song: Emergence
//
// E minor pentatonic, 92 BPM. The idea: complex beauty from simple rules.
// A mallet plays notes from a pentatonic pool, wandering via PICK_RANDOM_WALK.
// Drums are probability-driven — never the same twice.
// Bass pulses roots. Pad swells and recedes.
// Conditional triggers make the pattern evolve across loops.
//
// It should feel like watching something grow.
//
// Chord movement: Em7 → Cmaj7 → Am7 → D
// (i7 → VImaj7 → iv7 → VII — bittersweet, minor with major light)
// ============================================================================

static void Song_Emergence_ConfigureVoices(void) {
    masterVolume = 0.28f;
    scaleLockEnabled = true;
    scaleRoot = 4;                      // E
    scaleType = SCALE_MINOR_PENTA;      // E G A B D
}

// Drums: probability-driven pulse, never static
static void emergDrums(Pattern* p) {
    // Kick: just beat 1, sometimes beat 3
    drum(p, 0, 0, 0.38);
    drum(p, 0, 8, 0.25);
    patSetDrumProb(p, 0, 8, 0.4f);

    // Snare: ghost rimshots, probabilistic — like raindrops
    drumP(p, 1, 4,  0.18, 0.3);
    patSetDrumProb(p, 1, 4, 0.6f);
    drumP(p, 1, 10, 0.15, 0.2);
    patSetDrumProb(p, 1, 10, 0.4f);
    drumP(p, 1, 14, 0.12, 0.3);
    patSetDrumProb(p, 1, 14, 0.3f);

    // Hihat: quiet 8ths, some conditional — busier on later loops
    drum(p, 2, 0,  0.15);
    drum(p, 2, 4,  0.10);
    drum(p, 2, 8,  0.12);
    drum(p, 2, 12, 0.10);
    // Extra hats only appear after first loop
    drum(p, 2, 2,  0.06);
    patSetDrumCond(p, 2, 2, COND_NOT_FIRST);
    drum(p, 2, 6,  0.06);
    patSetDrumCond(p, 2, 6, COND_NOT_FIRST);
    drum(p, 2, 10, 0.06);
    patSetDrumCond(p, 2, 10, COND_1_2);  // every other loop

    // Perc: rare bell-like tick
    drumP(p, 3, 7, 0.10, 0.5);
    patSetDrumProb(p, 3, 7, 0.2f);
}

// Even sparser — intro/outro
static void emergDrumsBreath(Pattern* p) {
    drum(p, 0, 0, 0.28);
    drum(p, 2, 8, 0.08);
    patSetDrumProb(p, 2, 8, 0.5f);
}

// --- Bass: root pulses ---

// Em7 pedal (2 bars)
static void emergBassEm(Pattern* p) {
    note(p, 0, 0,  E2,  .40, 4);
    note(p, 0, 8,  E2,  .32, 4);
    // Fifth on loop 2+
    note(p, 0, 12, B2,  .25, 2);
    patSetNoteCond(p, SEQ_DRUM_TRACKS + 0, 12, COND_NOT_FIRST);
}

// Cmaj7 → Am7
static void emergBassCA(Pattern* p) {
    note(p, 0, 0,  Cn3, .40, 4);
    note(p, 0, 4,  G2,  .28, 2);
    patSetNoteProb(p, SEQ_DRUM_TRACKS + 0, 4, 0.6f);
    note(p, 0, 8,  A2,  .40, 4);
    note(p, 0, 12, E2,  .28, 2);
    patSetNoteProb(p, SEQ_DRUM_TRACKS + 0, 12, 0.5f);
}

// Am7 → D (turnaround)
static void emergBassAD(Pattern* p) {
    note(p, 0, 0,  A2,  .40, 4);
    note(p, 0, 8,  D2,  .42, 4);
    note(p, 0, 14, E2,  .30, 2);   // walk back to E
}

// --- Chords: shimmering pads ---

// Em7: E3 G3 B3 D4
static void emergChordEm(Pattern* p) {
    chordCustom(p, 2, 0, 0.18, 16, E3, G3, B3, D4);
}

// Cmaj7: C3 E3 G3 B3
static void emergChordC(Pattern* p) {
    chordCustom(p, 2, 0, 0.18, 8, Cn3, E3, G3, B3);
    // Am7: A3 C4 E4 G4
    chordCustom(p, 2, 8, 0.16, 8, A3, Cn4, E4, G4);
}

// Am7 → D
static void emergChordAD(Pattern* p) {
    chordCustom(p, 2, 0, 0.18, 8, A3, Cn4, E4, G4);
    // D: D3 F#3 A3 — major brightness breaking through
    chordCustom(p, 2, 8, 0.20, 8, D3, Fs3, A3, -1);
}

// --- Lead: the wandering mallet ---

// Seed phrase: E5 → D5 → B4 → A4 (a question)
static void emergMelodySeed(Pattern* p) {
    noteS(p, 1, 0,  E5,  .35, 4, 2);    // long E — the beginning
    note(p, 1, 6,  D5,  .30, 2);
    noteS(p, 1, 10, B4,  .35, 4, 2);    // rest on B
}

// Answer phrase: A4 → B4 → D5 (ascending, hopeful)
static void emergMelodyAnswer(Pattern* p) {
    note(p, 1, 0,  A4,  .32, 2);
    note(p, 1, 3,  B4,  .35, 3);
    noteS(p, 1, 8,  D5,  .38, 4, 2);
    // Echo — conditional, only sometimes
    note(p, 1, 14, E5,  .22, 2);
    patSetNoteProb(p, SEQ_DRUM_TRACKS + 1, 14, 0.4f);
}

// Generative: note pool on key steps — random walk through E G A B D
static void emergMelodyPool(Pattern* p) {
    // Step 0: pool of E5, G4, B4, D5 — wanders
    patSetChordCustom(p, SEQ_DRUM_TRACKS + 1, 0, 0.35, 3, E5, G4, B4, D5);
    patSetPickMode(p, SEQ_DRUM_TRACKS + 1, 0, PICK_RANDOM_WALK);

    // Step 4: pool of A4, B4, D5, E5
    patSetChordCustom(p, SEQ_DRUM_TRACKS + 1, 4, 0.32, 3, A4, B4, D5, E5);
    patSetPickMode(p, SEQ_DRUM_TRACKS + 1, 4, PICK_RANDOM_WALK);

    // Step 8: pool of B4, D5, E5, G5
    patSetChordCustom(p, SEQ_DRUM_TRACKS + 1, 8, 0.38, 4, B4, D5, E5, G5);
    patSetPickMode(p, SEQ_DRUM_TRACKS + 1, 8, PICK_RANDOM_WALK);

    // Step 12: single held note — moment of clarity
    noteS(p, 1, 12, E5,  .35, 4, 2);
}

// Combined: seed + pool elements
static void emergMelodyFull(Pattern* p) {
    // Fixed opening gesture
    noteS(p, 1, 0,  E5,  .38, 3, 1);
    note(p, 1, 4,  D5,  .32, 1);

    // Pool on steps 5-6: wandering fill
    patSetChordCustom(p, SEQ_DRUM_TRACKS + 1, 6, 0.30, 2, A4, B4, G4, D5);
    patSetPickMode(p, SEQ_DRUM_TRACKS + 1, 6, PICK_RANDOM);

    // Resolution
    noteS(p, 1, 8,  B4,  .40, 3, 2);

    // Tail — conditional, different each time
    note(p, 1, 12, A4,  .28, 2);
    patSetNoteCond(p, SEQ_DRUM_TRACKS + 1, 12, COND_1_2);
    note(p, 1, 13, G4,  .25, 2);
    patSetNoteCond(p, SEQ_DRUM_TRACKS + 1, 13, COND_2_2);
}

// --- Patterns ---

// Pat 0: Silence, then a single note. Space.
static void Song_Emergence_Pattern0(Pattern* p) {
    initPattern(p);
    // Just one mallet note, ringing into space
    noteS(p, 1, 4,  E5,  .30, 4, 8);
}

// Pat 1: Pulse begins. Bass E pedal, breath drums.
static void Song_Emergence_Pattern1(Pattern* p) {
    initPattern(p);
    emergDrumsBreath(p);
    emergBassEm(p);
    // That same note again, establishing the tone
    noteS(p, 1, 0,  E5,  .28, 4, 4);
    noteS(p, 1, 10, B4,  .25, 4, 2);
}

// Pat 2: Pad swells in. Em7. Seed melody.
static void Song_Emergence_Pattern2(Pattern* p) {
    initPattern(p);
    emergDrumsBreath(p);
    emergBassEm(p);
    emergChordEm(p);
    emergMelodySeed(p);
}

// Pat 3: Drums arrive. Cmaj7 → Am7. Answer phrase.
static void Song_Emergence_Pattern3(Pattern* p) {
    initPattern(p);
    emergDrums(p);
    emergBassCA(p);
    emergChordC(p);
    emergMelodyAnswer(p);
}

// Pat 4: Em7 again — generative pool melody. It starts wandering.
static void Song_Emergence_Pattern4(Pattern* p) {
    initPattern(p);
    emergDrums(p);
    emergBassEm(p);
    emergChordEm(p);
    emergMelodyPool(p);
}

// Pat 5: Am7 → D — turnaround, full melody
static void Song_Emergence_Pattern5(Pattern* p) {
    initPattern(p);
    emergDrums(p);
    emergBassAD(p);
    emergChordAD(p);
    emergMelodyFull(p);
}

// Pat 6: Em7 — everything together, peak. Pool melody + conditions.
static void Song_Emergence_Pattern6(Pattern* p) {
    initPattern(p);
    emergDrums(p);
    emergBassEm(p);
    emergChordEm(p);

    // Dense generative melody — every beat has a pool
    patSetChordCustom(p, SEQ_DRUM_TRACKS + 1, 0, 0.38, 2, E5, B4, D5, G4);
    patSetPickMode(p, SEQ_DRUM_TRACKS + 1, 0, PICK_RANDOM_WALK);

    patSetChordCustom(p, SEQ_DRUM_TRACKS + 1, 3, 0.32, 2, A4, D5, E5, B4);
    patSetPickMode(p, SEQ_DRUM_TRACKS + 1, 3, PICK_PINGPONG);

    patSetChordCustom(p, SEQ_DRUM_TRACKS + 1, 6, 0.35, 2, G4, B4, D5, E5);
    patSetPickMode(p, SEQ_DRUM_TRACKS + 1, 6, PICK_RANDOM);

    noteS(p, 1, 9, E5, .42, 4, 3);    // peak — held high E

    note(p, 1, 14, D5, .28, 2);
    patSetNoteCond(p, SEQ_DRUM_TRACKS + 1, 14, COND_1_2);
}

// Pat 7: Dissolve. Just pad and one last note.
static void Song_Emergence_Pattern7(Pattern* p) {
    initPattern(p);
    // No drums. Just the pad breathing.
    chordCustom(p, 2, 0, 0.15, 16, E3, G3, B3, D4);

    // One last bass note
    noteS(p, 0, 0, E2, .25, 8, 8);

    // The seed phrase one more time, slower, quieter
    noteS(p, 1, 2,  E5,  .22, 6, 4);
    noteS(p, 1, 10, B4,  .18, 6, 4);
}

static void Song_Emergence_Load(Pattern patterns[8]) {
    Song_Emergence_Pattern0(&patterns[0]);
    Song_Emergence_Pattern1(&patterns[1]);
    Song_Emergence_Pattern2(&patterns[2]);
    Song_Emergence_Pattern3(&patterns[3]);
    Song_Emergence_Pattern4(&patterns[4]);
    Song_Emergence_Pattern5(&patterns[5]);
    Song_Emergence_Pattern6(&patterns[6]);
    Song_Emergence_Pattern7(&patterns[7]);
}

#define SONG_EMERGENCE_BPM  92.0f

#endif // SONGS_H
