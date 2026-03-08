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
#define Fs5 78
#define Ab5 80
#define Bb5 82

#define REST (-1)

// ============================================================================
// Song authoring helpers — reduce 3 assignments to 1 call
// ============================================================================

// Set a melody note: note + velocity + gate in one call
static inline void note(Pattern* p, int track, int step, int n, float vel, int gate) {
    p->melodyNote[track][step] = n;
    p->melodyVelocity[track][step] = vel;
    p->melodyGate[track][step] = gate;
}

// Set a melody note with timing nudge (p-lock)
static inline void noteN(Pattern* p, int track, int step, int n, float vel, int gate, float nudge) {
    note(p, track, step, n, vel, gate);
    seqSetPLock(p, SEQ_DRUM_TRACKS + track, step, PLOCK_TIME_NUDGE, nudge);
}

// Set a melody note with sustain (extra hold steps after gate)
static inline void noteS(Pattern* p, int track, int step, int n, float vel, int gate, int sus) {
    note(p, track, step, n, vel, gate);
    p->melodySustain[track][step] = sus;
}

// Set a melody note with nudge + sustain
static inline void noteNS(Pattern* p, int track, int step, int n, float vel, int gate, float nudge, int sus) {
    note(p, track, step, n, vel, gate);
    seqSetPLock(p, SEQ_DRUM_TRACKS + track, step, PLOCK_TIME_NUDGE, nudge);
    p->melodySustain[track][step] = sus;
}

// Set a drum hit: step + velocity in one call
static inline void drum(Pattern* p, int track, int step, float vel) {
    p->drumSteps[track][step] = true;
    p->drumVelocity[track][step] = vel;
}

// Set a drum hit with timing nudge
static inline void drumN(Pattern* p, int track, int step, float vel, float nudge) {
    drum(p, track, step, vel);
    seqSetPLock(p, track, step, PLOCK_TIME_NUDGE, nudge);
}

// Set a drum hit with pitch offset
static inline void drumP(Pattern* p, int track, int step, float vel, float pitch) {
    drum(p, track, step, vel);
    p->drumPitch[track][step] = pitch;
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
    p->drumProbability[3][12] = 0.4f;

    // Bass: slow D pedal, then A
    note(p, 0, 0, D2, .35, 8);
    note(p, 0, 8, A2, .30, 8);

    // Lead: glockenspiel, sparse descending phrase
    note(p, 1, 3, A4, .25, 3);   note(p, 1, 6, G4, .20, 2);
    note(p, 1,10, F4, .22, 2);   note(p, 1,13, D4, .18, 3);

    // Chords: Dm (half bar) then Am
    note(p, 2, 0, D3, 0.20, 8);
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_TRIAD;
    p->melodyNotePool[2][0].pickMode  = PICK_CYCLE_UP;

    note(p, 2, 8, A3, 0.18, 8);
    p->melodyNotePool[2][8].enabled   = true;
    p->melodyNotePool[2][8].chordType = CHORD_TRIAD;
    p->melodyNotePool[2][8].pickMode  = PICK_CYCLE_UP;
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
    p->melodySlide[1][12] = true;

    // Chords: Dm the whole bar
    note(p, 2, 0, D3, 0.15, 16);
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_TRIAD;
    p->melodyNotePool[2][0].pickMode  = PICK_RANDOM;
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
    drum(p, 2, 7, 0.06);   p->drumProbability[2][7] = 0.5f;
    drum(p, 2,11, 0.10);
    drum(p, 2,15, 0.05);   p->drumProbability[2][15] = 0.3f;

    // Perc: distant rimshot, very rare
    drumP(p, 3, 6, 0.06, -0.3);
    p->drumProbability[3][6] = 0.25f;

    // Bass: chromatic descent C2→B1→Bb2→A2
    note(p, 0, 0, Cn2, .40, 4);  note(p, 0, 4, B1,  .38, 4);
    note(p, 0, 8, Bb2, .36, 4);  note(p, 0,12, A2,  .42, 4);

    // Lead: eerie vowel chanting on dissonant intervals
    note(p, 1, 4, Eb4, .22, 5);
    note(p, 1,11, G4,  .18, 4);
    note(p, 1,14, Ab4, .20, 3);
    p->melodySlide[1][14] = true;

    // Chords: Cm then Ab
    note(p, 2, 0, Cn3, 0.18, 8);
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_TRIAD;
    p->melodyNotePool[2][0].pickMode  = PICK_CYCLE_UP;

    note(p, 2, 8, Ab3, 0.16, 8);
    p->melodyNotePool[2][8].enabled   = true;
    p->melodyNotePool[2][8].chordType = CHORD_TRIAD;
    p->melodyNotePool[2][8].pickMode  = PICK_CYCLE_UP;
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
    note(p, 2, 0, Cn3, 0.14, 16);
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_SEVENTH;
    p->melodyNotePool[2][0].pickMode  = PICK_RANDOM;
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
    fmModIndex = 1.8f;
    fmFeedback = 0.0f;
}

// --- Pattern A: vibes call, FM responds ---

static void Song_JazzCallResponse_PatternA(Pattern* p) {
    initPattern(p);

    // Kick: beats 1 and 3
    drum(p, 0, 0, .35);  drum(p, 0, 8, .28);
    // Snare: brush on 2 and 4 + ghost
    drum(p, 1, 4, .15);  drum(p, 1,12, .18);
    drum(p, 1,10, .06);  p->drumProbability[1][10] = 0.5f;
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
    p->melodyAccent[1][6] = true;

    // Chord/RESPONSE (FM): descending answer second half
    note(p, 2, 9, Cn5, .38, 2);  note(p, 2,11, B4, .35, 2);
    note(p, 2,12, A4,  .40, 3);  note(p, 2,14, G4, .42, 4);
    p->melodyAccent[2][14] = true;
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
    p->melodySlide[1][14] = true;
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
        p->drumProbability[3][s] = 0.4f;
    }

    // Acid bass: syncopated C2 line
    note(p, 0, 0, Cn2, .55, 2);  note(p, 0, 2, Cn2, .48, 2);
    note(p, 0, 4, Cn2, .42, 1);  note(p, 0, 5, Cn2, .50, 2);
    note(p, 0, 7, Eb2, .52, 2);  note(p, 0, 9, Cn2, .48, 2);
    note(p, 0,11, Cn2, .40, 1);  note(p, 0,12, Cn2, .55, 2);
    note(p, 0,15, G2,  .45, 2);
    p->melodySlide[0][5]  = true;   p->melodySlide[0][15] = true;
    p->melodyAccent[0][0] = true;   p->melodyAccent[0][12] = true;

    // Chord stabs (track 1): offbeat Cm
    note(p, 1, 2, Cn4, 0.35, 1);
    p->melodyNotePool[1][2].enabled   = true;
    p->melodyNotePool[1][2].chordType = CHORD_TRIAD;
    p->melodyNotePool[1][2].pickMode  = PICK_CYCLE_UP;

    note(p, 1, 6, Cn4, 0.30, 1);
    p->melodyNotePool[1][6].enabled   = true;
    p->melodyNotePool[1][6].chordType = CHORD_TRIAD;
    p->melodyNotePool[1][6].pickMode  = PICK_CYCLE_UP;

    note(p, 1, 10, Ab3, 0.32, 1);
    p->melodyNotePool[1][10].enabled   = true;
    p->melodyNotePool[1][10].chordType = CHORD_TRIAD;
    p->melodyNotePool[1][10].pickMode  = PICK_CYCLE_UP;

    // Pad: Cm7 held whole bar
    note(p, 2, 0, Cn3, 0.15, 16);
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_SEVENTH;
    p->melodyNotePool[2][0].pickMode  = PICK_RANDOM;
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
    p->melodySlide[0][8] = true;

    // Pad only, quieter
    note(p, 2, 0, Cn3, 0.12, 16);
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_TRIAD;
    p->melodyNotePool[2][0].pickMode  = PICK_RANDOM;
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
    drum(p, 3, 3, .12);  p->drumProbability[3][3] = 0.6f;
    drum(p, 3,11, .10);  p->drumProbability[3][11] = 0.4f;

    // Sub bass: F1 and C2
    note(p, 0, 0, F1,  .50, 8);
    note(p, 0, 8, Cn2, .45, 8);

    // Stab (track 1): sparse FM color
    note(p, 1, 6, Cn4, .25, 2);
    note(p, 1,14, Ab3, .20, 2);
    p->drumProbability[1][14] = 0.5f;

    // Pad: Fm7 held whole bar
    note(p, 2, 0, Fn3, 0.20, 16);
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_SEVENTH;
    p->melodyNotePool[2][0].pickMode  = PICK_CYCLE_UP;
}

// --- Pattern B: even more minimal — pad solo with filter breathing ---

static void Song_DeepHouse_PatternB(Pattern* p) {
    initPattern(p);

    drum(p, 0, 0, .50);     // Kick: beat 1 only
    drum(p, 3, 7, .10);     // Single rim click

    note(p, 0, 0, F1, .45, 16);   // Sub bass: F1 whole bar

    // Pad: Fm7 whole bar
    note(p, 2, 0, Fn3, 0.22, 16);
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_SEVENTH;
    p->melodyNotePool[2][0].pickMode  = PICK_RANDOM;
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
    fmModIndex = 1.2f;
    fmFeedback = 0.0f;
}

// --- Pattern A: the head nod ---

static void Song_Dilla_PatternA(Pattern* p) {
    initPattern(p);

    // Kick: syncopated, never on 1
    drum(p, 0, 1, .60);  drum(p, 0, 7, .50);
    drum(p, 0,10, .55);  drum(p, 0,14, .35);
    p->drumProbability[0][14] = 0.6f;
    // Snare: lazy beats 2+4 + ghost notes
    drum(p, 1, 2, .08);  drum(p, 1, 4, .40);
    drum(p, 1, 6, .06);  p->drumProbability[1][6] = 0.4f;
    drum(p, 1, 9, .07);  p->drumProbability[1][9] = 0.5f;
    drum(p, 1,12, .42);
    drum(p, 1,15, .05);  p->drumProbability[1][15] = 0.3f;
    // HiHat: 16ths with velocity variation
    drum(p, 2, 0, .20);  drum(p, 2, 1, .08);  drum(p, 2, 2, .25);  drum(p, 2, 3, .06);
    drum(p, 2, 4, .18);  drum(p, 2, 5, .04);  drum(p, 2, 6, .22);  drum(p, 2, 7, .10);
    drum(p, 2, 8, .20);  drum(p, 2, 9, .06);  drum(p, 2,10, .25);  drum(p, 2,11, .04);
    drum(p, 2,12, .15);  drum(p, 2,13, .08);  drum(p, 2,14, .20);  drum(p, 2,15, .05);
    p->drumProbability[2][3]  = 0.5f;  p->drumProbability[2][7]  = 0.6f;
    p->drumProbability[2][11] = 0.5f;  p->drumProbability[2][15] = 0.4f;
    // Perc: shaker on 8ths
    for (int s = 0; s < 16; s += 2) drumP(p, 3, s, 0.06, 0.4);

    // Bass: walking Eb with chromatic passing tones
    note(p, 0, 0, Eb2, .50, 2);  note(p, 0, 2, G2,  .42, 2);
    note(p, 0, 4, Ab2, .45, 2);  note(p, 0, 6, Bb2, .40, 2);
    note(p, 0, 8, Eb2, .52, 3);  note(p, 0,11, D2,  .35, 2);
    note(p, 0,12, Eb2, .48, 2);

    // Keys (track 1): Rhodes stabs on upbeats
    note(p, 1, 2, Eb4, 0.32, 2);
    p->melodyNotePool[1][2].enabled   = true;
    p->melodyNotePool[1][2].chordType = CHORD_SEVENTH;
    p->melodyNotePool[1][2].pickMode  = PICK_CYCLE_UP;

    note(p, 1, 6, Eb4, 0.28, 1);
    p->melodyAccent[1][6] = true;

    note(p, 1, 11, Ab3, 0.30, 2);
    p->melodyNotePool[1][11].enabled   = true;
    p->melodyNotePool[1][11].chordType = CHORD_TRIAD;
    p->melodyNotePool[1][11].pickMode  = PICK_RANDOM;

    // Pad: Eb maj7 held whole bar
    note(p, 2, 0, Eb3, 0.15, 16);
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_SEVENTH;
    p->melodyNotePool[2][0].pickMode  = PICK_RANDOM;
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
    note(p, 2, 0, Cn3, 0.18, 16);
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_SEVENTH;
    p->melodyNotePool[2][0].pickMode  = PICK_RANDOM;
}

// --- Pattern C: full groove — bass gets busier, more Rhodes ---

static void Song_Dilla_PatternC(Pattern* p) {
    initPattern(p);

    // Full drums with extra kick ghost
    drum(p, 0, 1, .60);  drum(p, 0, 5, .30);  p->drumProbability[0][5] = 0.5f;
    drum(p, 0, 7, .50);  drum(p, 0,10, .55);   drum(p, 0,13, .35);
    drum(p, 1, 2, .07);  drum(p, 1, 4, .42);   drum(p, 1, 6, .06);
    drum(p, 1,10, .08);  drum(p, 1,12, .45);   drum(p, 1,14, .05);
    // Full 16th hats
    drum(p, 2, 0, .22);  drum(p, 2, 1, .10);  drum(p, 2, 2, .28);  drum(p, 2, 3, .05);
    drum(p, 2, 4, .20);  drum(p, 2, 5, .06);  drum(p, 2, 6, .25);  drum(p, 2, 7, .12);
    drum(p, 2, 8, .22);  drum(p, 2, 9, .08);  drum(p, 2,10, .28);  drum(p, 2,11, .05);
    drum(p, 2,12, .18);  drum(p, 2,13, .10);  drum(p, 2,14, .22);  drum(p, 2,15, .06);
    p->drumProbability[2][3]  = 0.6f;  p->drumProbability[2][11] = 0.5f;

    // Bass: chromatic run with blue note E natural
    note(p, 0, 0, Eb2, .52, 2);  note(p, 0, 2, E2,  .38, 1);
    note(p, 0, 4, F2,  .45, 2);  note(p, 0, 6, G2,  .40, 1);
    note(p, 0, 8, Ab2, .48, 2);  note(p, 0,10, Bb2, .42, 2);
    note(p, 0,12, Eb2, .55, 2);  note(p, 0,14, D2,  .35, 1);
    note(p, 0,15, Eb2, .48, 1);

    // Rhodes stabs: busier
    note(p, 1, 2, Eb4, 0.35, 1);
    p->melodyNotePool[1][2].enabled   = true;
    p->melodyNotePool[1][2].chordType = CHORD_SEVENTH;
    p->melodyNotePool[1][2].pickMode  = PICK_CYCLE_UP;

    note(p, 1, 5, Bb3, 0.28, 2);
    p->melodyAccent[1][5] = true;

    note(p, 1, 9, Ab3, 0.32, 2);
    p->melodyNotePool[1][9].enabled   = true;
    p->melodyNotePool[1][9].chordType = CHORD_TRIAD;
    p->melodyNotePool[1][9].pickMode  = PICK_RANDOM;

    note(p, 1, 13, G4, 0.25, 2);

    // Pad: Eb maj7
    note(p, 2, 0, Eb3, 0.18, 16);
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_SEVENTH;
    p->melodyNotePool[2][0].pickMode  = PICK_RANDOM;
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
    p->drumProbability[3][12] = 0.5f;

    // Bass: simple D pedal
    note(p, 0, 0, D2, .40, 8);
    note(p, 0, 8, A2, .35, 8);

    // Lead: glockenspiel, a few bright notes
    note(p, 1, 4, Fs4, .22, 3);
    note(p, 1,10, A4,  .20, 3);

    // Pad: D major triad
    note(p, 2, 0, D3, 0.12, 16);
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_TRIAD;
    p->melodyNotePool[2][0].pickMode  = PICK_CYCLE_UP;
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
    p->drumProbability[2][3] = 0.4f;  p->drumProbability[2][11] = 0.3f;
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
    note(p, 2, 0, D3, 0.20, 8);
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_SEVENTH;
    p->melodyNotePool[2][0].pickMode  = PICK_CYCLE_UP;

    note(p, 2, 8, A3, 0.22, 8);
    p->melodyNotePool[2][8].enabled   = true;
    p->melodyNotePool[2][8].chordType = CHORD_TRIAD;
    p->melodyNotePool[2][8].pickMode  = PICK_CYCLE_UP;
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
    p->melodySlide[1][12] = true;

    // Pad: Dmaj7 held whole bar
    note(p, 2, 0, D3, 0.25, 16);
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_SEVENTH;
    p->melodyNotePool[2][0].pickMode  = PICK_RANDOM;
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
    note(p, 2, 0, A3, 0.20, 8);
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_SEVENTH;
    p->melodyNotePool[2][0].pickMode  = PICK_CYCLE_UP;

    note(p, 2, 8, D3, 0.18, 8);
    p->melodyNotePool[2][8].enabled   = true;
    p->melodyNotePool[2][8].chordType = CHORD_SEVENTH;
    p->melodyNotePool[2][8].pickMode  = PICK_CYCLE_UP;
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
    note(p, 2, 0, G3, 0.18, 8);
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_SEVENTH;
    p->melodyNotePool[2][0].pickMode  = PICK_CYCLE_UP;

    note(p, 2, 8, Cn3, 0.22, 8);
    p->melodyNotePool[2][8].enabled   = true;
    p->melodyNotePool[2][8].chordType = CHORD_SEVENTH;
    p->melodyNotePool[2][8].pickMode  = PICK_CYCLE_UP;
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
    p->melodySlide[1][13] = true;

    // Pad: Fmaj7 → E7
    note(p, 2, 0, F3, 0.20, 8);
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_SEVENTH;
    p->melodyNotePool[2][0].pickMode  = PICK_CYCLE_UP;

    note(p, 2, 8, E3, 0.22, 8);
    p->melodyNotePool[2][8].enabled   = true;
    p->melodyNotePool[2][8].chordType = CHORD_SEVENTH;
    p->melodyNotePool[2][8].pickMode  = PICK_CYCLE_UP;
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
    for (int t = 0; t < SEQ_DRUM_TRACKS; t++) p->drumTrackLength[t] = 12;
    for (int t = 0; t < SEQ_MELODY_TRACKS; t++) p->melodyTrackLength[t] = 12;

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
    note(p, 2, 0, F3, .28, 6);
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_TRIAD;
    p->melodyNotePool[2][0].pickMode  = PICK_ALL;

    note(p, 2, 6, Cn3, .26, 6);
    p->melodyNotePool[2][6].enabled   = true;
    p->melodyNotePool[2][6].chordType = CHORD_SEVENTH;
    p->melodyNotePool[2][6].pickMode  = PICK_ALL;
}

// Pattern B: bars 3-4 — "Happy birthday to you" (second time, resolves to F)
// Melody: D C G | F (held) ... pickup C C
// Bass:   G2+C2 (bar 1) | F2 (bar 2)
// Chords: C7 (bar 1) | F (bar 2)
static void Song_HappyBirthday_PatternB(Pattern* p) {
    initPattern(p);
    for (int t = 0; t < SEQ_DRUM_TRACKS; t++) p->drumTrackLength[t] = 12;
    for (int t = 0; t < SEQ_MELODY_TRACKS; t++) p->melodyTrackLength[t] = 12;

    // Drums: waltz + rimshot fill
    drum(p, 0, 0, .45);  drum(p, 0, 6, .40);
    drum(p, 1, 4, .15);  drum(p, 1,10, .15);
    drum(p, 2, 0, .30);  drum(p, 2, 2, .18);  drum(p, 2, 4, .18);
    drum(p, 2, 6, .30);  drum(p, 2, 8, .18);  drum(p, 2,10, .18);
    drum(p, 3, 9, .10);  p->drumProbability[3][9] = 0.5f;

    // Bass: G2→C2 | F2
    note(p, 0, 0, G2,  .48, 4);  note(p, 0, 4, Cn2, .45, 2);
    note(p, 0, 6, F2,  .50, 6);

    // Melody: D C G | F (held) ... pickup C C
    note(p, 1, 0, D4,  .45, 2);  note(p, 1, 2, Cn4, .42, 2);
    note(p, 1, 4, G4,  .50, 2);  note(p, 1, 6, F4,  .48, 4);
    note(p, 1,10, Cn4, .38, 1);  note(p, 1,11, Cn4, .35, 1);

    // Chords: C7 → F
    note(p, 2, 0, Cn3, .26, 6);
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_SEVENTH;
    p->melodyNotePool[2][0].pickMode  = PICK_ALL;

    note(p, 2, 6, F3, .28, 6);
    p->melodyNotePool[2][6].enabled   = true;
    p->melodyNotePool[2][6].chordType = CHORD_TRIAD;
    p->melodyNotePool[2][6].pickMode  = PICK_ALL;
}

// Pattern C: bars 5-6 — "Happy birthday dear ___" (climax!)
// Melody: C5 A4 F4 F4 | E4 D4 ... pickup Bb4 Bb4
// Bass:   F2 (bar 1) | Bb2+Bb2 (bar 2)
// Chords: F7 (bar 1) | Bb (bar 2)
static void Song_HappyBirthday_PatternC(Pattern* p) {
    initPattern(p);
    for (int t = 0; t < SEQ_DRUM_TRACKS; t++) p->drumTrackLength[t] = 12;
    for (int t = 0; t < SEQ_MELODY_TRACKS; t++) p->melodyTrackLength[t] = 12;

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
    note(p, 2, 0, F3, .30, 6);
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_SEVENTH;
    p->melodyNotePool[2][0].pickMode  = PICK_ALL;

    note(p, 2, 6, Bb3, .28, 6);
    p->melodyNotePool[2][6].enabled   = true;
    p->melodyNotePool[2][6].chordType = CHORD_TRIAD;
    p->melodyNotePool[2][6].pickMode  = PICK_ALL;
}

// Pattern D: bars 7-8 — "Happy birthday to you" (final resolution)
// Melody: A4 F4 G4 | F4 (held, ending)
// Bass:   F2+C2 (bar 1) | F2 (bar 2, resolution)
// Chords: C7 (bar 1) | F (bar 2, home)
static void Song_HappyBirthday_PatternD(Pattern* p) {
    initPattern(p);
    for (int t = 0; t < SEQ_DRUM_TRACKS; t++) p->drumTrackLength[t] = 12;
    for (int t = 0; t < SEQ_MELODY_TRACKS; t++) p->melodyTrackLength[t] = 12;

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
    p->melodySlide[1][6] = true;

    // Chords: C7 → F (V-I home)
    note(p, 2, 0, Cn3, .26, 6);
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_SEVENTH;
    p->melodyNotePool[2][0].pickMode  = PICK_ALL;

    note(p, 2, 6, F3, .30, 6);
    p->melodyNotePool[2][6].enabled   = true;
    p->melodyNotePool[2][6].chordType = CHORD_TRIAD;
    p->melodyNotePool[2][6].pickMode  = PICK_ALL;
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
    p->melodyNote[2][step]     = root;
    p->melodyVelocity[2][step] = vel;
    p->melodyGate[2][step]     = gate;
    p->melodySustain[2][step]  = sustain;
    p->melodyNotePool[2][step].enabled        = true;
    p->melodyNotePool[2][step].chordType      = CHORD_CUSTOM;
    p->melodyNotePool[2][step].pickMode       = PICK_ALL;
    int count = 0;
    if (n0 >= 0) p->melodyNotePool[2][step].customNotes[count++] = n0;
    if (n1 >= 0) p->melodyNotePool[2][step].customNotes[count++] = n1;
    if (n2 >= 0) p->melodyNotePool[2][step].customNotes[count++] = n2;
    if (n3 >= 0) p->melodyNotePool[2][step].customNotes[count++] = n3;
    p->melodyNotePool[2][step].customNoteCount = count;
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
    p->melodySlide[1][10] = true;
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
    for (int t = 0; t < SEQ_DRUM_TRACKS; t++) p->drumTrackLength[t] = 32;
    for (int t = 0; t < SEQ_MELODY_TRACKS; t++) p->melodyTrackLength[t] = 32;
}

// Drums at 32nd resolution: hihat every 2 steps (=16th), snare/kick alternate
static void muleDrums(Pattern* p) {
    for (int s = 0; s < 32; s += 2) {
        // Hihat on every 16th note (every 2 steps at 32nd resolution)
        p->drumSteps[2][s] = true;
        p->drumVelocity[2][s] = 0.4f;
        // Snare on even 16ths, kick on odd 16ths
        if ((s / 2) % 2 == 0) {
            p->drumSteps[1][s] = true;
            p->drumVelocity[1][s] = 0.4f;
        } else {
            p->drumSteps[0][s] = true;
            p->drumVelocity[0][s] = 0.4f;
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
        p->melodyNote[0][s] = bassNotes[i];
        p->melodyGate[0][s] = 1;
        p->melodyVelocity[0][s] = 0.4f;
    }
}

// Helper: set a melody note on track 1
static void muleNote(Pattern* p, int step, int note, int gate) {
    p->melodyNote[1][step] = note;
    p->melodyGate[1][step] = gate;
    p->melodyVelocity[1][step] = 0.4f;
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

#endif // SONGS_H
