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
#define E1  28
#define F1  29
#define G1  31
#define B1  35

#define REST (-1)

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

    // ---- Drums (tracks 0-3) ----
    // Kick: nothing. This is a dormitory, not a dance floor.
    // Snare: nothing.
    // HiHat (track 2): very faint ticks on offbeats, like a clock
    //   Steps:  1 . . . 2 . . . 3 . . . 4 . . .
    //   Index:  0 1 2 3 4 5 6 7 8 9 ...
    int hh_steps[] =  {0,0,1,0, 0,0,1,0, 0,0,1,0, 0,0,1,0};
    float hh_vel[] =  {0,0,.15,0, 0,0,.12,0, 0,0,.15,0, 0,0,.10,0};
    for (int s = 0; s < 16; s++) {
        p->drumSteps[2][s] = hh_steps[s];
        p->drumVelocity[2][s] = hh_vel[s];
    }
    // Perc (track 3): single rimshot on step 12, very faint — a creak
    p->drumSteps[3][12] = true;
    p->drumVelocity[3][12] = 0.08f;
    p->drumPitch[3][12] = 0.3f;  // pitched up slightly
    p->drumProbability[3][12] = 0.4f;  // only 40% of the time

    // ---- Bass (melodic track 0) ----
    // Slow D pedal with occasional movement. Long gates.
    // D2 held for 8 steps, then A1(=33) held for 8 steps.
    int bass_notes[] = {D2,REST,REST,REST, REST,REST,REST,REST,
                        A2,REST,REST,REST, REST,REST,REST,REST};
    int bass_gates[] = { 8,  0,  0,  0,    0,  0,  0,  0,
                         8,  0,  0,  0,    0,  0,  0,  0};
    float bass_vel[] = {.35, 0,  0,  0,    0,  0,  0,  0,
                        .30, 0,  0,  0,    0,  0,  0,  0};
    for (int s = 0; s < 16; s++) {
        p->melodyNote[0][s]     = bass_notes[s];
        p->melodyGate[0][s]     = bass_gates[s];
        p->melodyVelocity[0][s] = bass_vel[s];
    }

    // ---- Lead (melodic track 1) ----
    // Glockenspiel melody. Sparse — mostly rests. D dorian pentatonic-ish.
    // A simple descending phrase with a lot of breathing room.
    //   Steps:  1 . . . 2 . . . 3 . . . 4 . . .
    int lead_notes[] = {REST,REST,REST, A4,  REST,REST, G4, REST,
                        REST,REST, F4, REST, REST, D4, REST,REST};
    int lead_gates[] = {  0,  0,  0,  3,    0,  0,  2,  0,
                          0,  0,  2,  0,    0,  3,  0,  0};
    float lead_vel[] = {  0,  0,  0, .25,   0,  0, .20,  0,
                          0,  0, .22,  0,   0, .18,  0,  0};
    for (int s = 0; s < 16; s++) {
        p->melodyNote[1][s]     = lead_notes[s];
        p->melodyGate[1][s]     = lead_gates[s];
        p->melodyVelocity[1][s] = lead_vel[s];
    }

    // ---- Chords (melodic track 2) ----
    // A single Dm chord held for the whole bar, then an Am.
    // Using note pools: root note + triad chord type = the pool picks notes.
    // Dm = D F A, Am = A C E
    p->melodyNote[2][0]     = D3;
    p->melodyVelocity[2][0] = 0.20f;
    p->melodyGate[2][0]     = 8;  // held for 8 steps (half bar)
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_TRIAD;
    p->melodyNotePool[2][0].pickMode  = PICK_CYCLE_UP;

    p->melodyNote[2][8]     = A3;
    p->melodyVelocity[2][8] = 0.18f;
    p->melodyGate[2][8]     = 8;
    p->melodyNotePool[2][8].enabled   = true;
    p->melodyNotePool[2][8].chordType = CHORD_TRIAD;
    p->melodyNotePool[2][8].pickMode  = PICK_CYCLE_UP;
}

// --- Pattern B: variation (sparser, for variety when looping) ---

static void Song_DormitoryAmbient_PatternB(Pattern* p) {
    initPattern(p);

    // HiHat: even sparser — just steps 6 and 14
    p->drumSteps[2][6]  = true;  p->drumVelocity[2][6]  = 0.10f;
    p->drumSteps[2][14] = true;  p->drumVelocity[2][14] = 0.12f;

    // Bass: just a single D2 held for the whole bar
    p->melodyNote[0][0]     = D2;
    p->melodyVelocity[0][0] = 0.30f;
    p->melodyGate[0][0]     = 16;  // whole bar

    // Lead: only two notes — a call and response
    //   Step 4: A4 (question)
    //   Step 12: D4 (answer, one octave down — resolution)
    p->melodyNote[1][4]      = A4;
    p->melodyVelocity[1][4]  = 0.20f;
    p->melodyGate[1][4]      = 4;

    p->melodyNote[1][12]     = D4;
    p->melodyVelocity[1][12] = 0.18f;
    p->melodyGate[1][12]     = 4;
    p->melodySlide[1][12]    = true;  // gentle slide down to D

    // Chords: Dm the whole bar, very quiet
    p->melodyNote[2][0]     = D3;
    p->melodyVelocity[2][0] = 0.15f;
    p->melodyGate[2][0]     = 16;
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_TRIAD;
    p->melodyNotePool[2][0].pickMode  = PICK_RANDOM;  // gentle variation each cycle
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

    // ---- Drums ----
    // Kick (track 0): heartbeat. Deep, slow. Steps 0 and 8.
    // Pitched down for sub-bass thump.
    p->drumSteps[0][0]  = true;  p->drumVelocity[0][0]  = 0.50f; p->drumPitch[0][0]  = -0.4f;
    p->drumSteps[0][8]  = true;  p->drumVelocity[0][8]  = 0.35f; p->drumPitch[0][8]  = -0.4f;

    // HiHat (track 2): distant metallic tick, irregular — clock in an empty room
    p->drumSteps[2][3]  = true;  p->drumVelocity[2][3]  = 0.08f;
    p->drumSteps[2][7]  = true;  p->drumVelocity[2][7]  = 0.06f;
    p->drumProbability[2][7] = 0.5f;   // sometimes skips — unsettling
    p->drumSteps[2][11] = true;  p->drumVelocity[2][11] = 0.10f;
    p->drumSteps[2][15] = true;  p->drumVelocity[2][15] = 0.05f;
    p->drumProbability[2][15] = 0.3f;

    // Clap/perc (track 3): single distant rimshot, very rare
    p->drumSteps[3][6]  = true;  p->drumVelocity[3][6]  = 0.06f;
    p->drumPitch[3][6]  = -0.3f;  // pitched down = darker
    p->drumProbability[3][6] = 0.25f;  // 1 in 4 loops

    // ---- Bass (melodic track 0): ORGAN ----
    // Chromatic descent: C2 -> B1 -> Bb1 -> A1. Pure dread.
    // Each note held for 4 steps. Slow, inevitable crawl downward.
    int bass_notes[] = {Cn2, REST,REST,REST,  B1, REST,REST,REST,
                        Bb2, REST,REST,REST,  A2, REST,REST,REST};
    int bass_gates[] = {  4,   0,  0,  0,     4,   0,  0,  0,
                          4,   0,  0,  0,     4,   0,  0,  0};
    float bass_vel[] = {.40,   0,  0,  0,   .38,   0,  0,  0,
                        .36,   0,  0,  0,   .42,   0,  0,  0};
    for (int s = 0; s < 16; s++) {
        p->melodyNote[0][s]     = bass_notes[s];
        p->melodyGate[0][s]     = bass_gates[s];
        p->melodyVelocity[0][s] = bass_vel[s];
    }

    // ---- Lead (melodic track 1): VOWEL ----
    // Eerie chanting. Low "oo"/"oh" vowels on dissonant intervals.
    // Eb4 against the B bass = tritone. G4 against Ab = minor 2nd rub.
    // Sparse — the silence between notes is the scariest part.
    //   Steps:  1 . . . 2 . . . 3 . . . 4 . . .
    int lead_notes[] = {REST,REST,REST,REST,  Eb4, REST,REST,REST,
                        REST,REST,REST, G4,   REST,REST, Ab4,REST};
    int lead_gates[] = {  0,  0,  0,  0,       5,   0,  0,  0,
                          0,  0,  0,  4,        0,  0,   3,  0};
    float lead_vel[] = {  0,  0,  0,  0,     .22,   0,  0,  0,
                          0,  0,  0, .18,      0,  0, .20,  0};
    for (int s = 0; s < 16; s++) {
        p->melodyNote[1][s]     = lead_notes[s];
        p->melodyGate[1][s]     = lead_gates[s];
        p->melodyVelocity[1][s] = lead_vel[s];
    }
    // Slide from G4 down to Ab4 — an unresolved groan
    p->melodySlide[1][14] = true;

    // ---- Chords (melodic track 2): ORGAN ----
    // Cm with added tension. Held for the whole bar.
    // First half: Cm (C Eb G). Second half: Ab (Ab C Eb) — relative major, but
    // against the descending bass it sounds like sinking.
    p->melodyNote[2][0]     = Cn3;
    p->melodyVelocity[2][0] = 0.18f;
    p->melodyGate[2][0]     = 8;
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_TRIAD;
    p->melodyNotePool[2][0].pickMode  = PICK_CYCLE_UP;

    p->melodyNote[2][8]     = Ab3;
    p->melodyVelocity[2][8] = 0.16f;
    p->melodyGate[2][8]     = 8;
    p->melodyNotePool[2][8].enabled   = true;
    p->melodyNotePool[2][8].chordType = CHORD_TRIAD;
    p->melodyNotePool[2][8].pickMode  = PICK_CYCLE_UP;
}

// --- Pattern B: the void stares back ---

static void Song_Suspense_PatternB(Pattern* p) {
    initPattern(p);

    // Kick: slower heartbeat, just beat 1. Quieter. Fading.
    p->drumSteps[0][0] = true;  p->drumVelocity[0][0] = 0.40f; p->drumPitch[0][0] = -0.5f;

    // HiHat: single tick, step 10. Alone in the dark.
    p->drumSteps[2][10] = true;  p->drumVelocity[2][10] = 0.07f;

    // Bass: single low C held the entire bar. Organ drone.
    p->melodyNote[0][0]     = Cn2;
    p->melodyVelocity[0][0] = 0.35f;
    p->melodyGate[0][0]     = 16;

    // Lead: one long vowel. B4 against C bass = minor 2nd. Maximum tension.
    // Held from step 6 to end of bar.
    p->melodyNote[1][6]     = B4;
    p->melodyVelocity[1][6] = 0.20f;
    p->melodyGate[1][6]     = 10;

    // Chords: Cm held the whole bar. No movement. Just weight.
    p->melodyNote[2][0]     = Cn3;
    p->melodyVelocity[2][0] = 0.14f;
    p->melodyGate[2][0]     = 16;
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_SEVENTH;  // Cm7 = more darkness
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

    // ---- Drums: swing feel ----
    // Kick (track 0): beats 1 and 3, soft
    p->drumSteps[0][0]  = true;  p->drumVelocity[0][0]  = 0.35f;
    p->drumSteps[0][8]  = true;  p->drumVelocity[0][8]  = 0.28f;

    // Snare (track 1): beats 2 and 4, very soft brush
    p->drumSteps[1][4]  = true;  p->drumVelocity[1][4]  = 0.15f;
    p->drumSteps[1][12] = true;  p->drumVelocity[1][12] = 0.18f;
    // ghost note on the "and" of 3
    p->drumSteps[1][10] = true;  p->drumVelocity[1][10] = 0.06f;
    p->drumProbability[1][10] = 0.5f;

    // HiHat (track 2): ride cymbal pattern — every 2 steps (8th notes) with swing
    for (int s = 0; s < 16; s += 2) {
        p->drumSteps[2][s] = true;
        p->drumVelocity[2][s] = (s % 4 == 0) ? 0.30f : 0.18f;  // downbeats louder
    }
    // add the swing "skip" note on offbeats for triplet feel
    p->drumSteps[2][3]  = true;  p->drumVelocity[2][3]  = 0.10f;
    p->drumSteps[2][7]  = true;  p->drumVelocity[2][7]  = 0.08f;
    p->drumSteps[2][11] = true;  p->drumVelocity[2][11] = 0.10f;
    p->drumSteps[2][15] = true;  p->drumVelocity[2][15] = 0.08f;

    // ---- Bass (track 0): walking bass line ----
    // G2 -> B2 -> D3 -> E3 -> F3 -> E3 -> D3 -> B2 (classic walk up and back)
    int bass_notes[] = {G2, REST, B2, REST, D3, REST, E3, REST,
                        F3, REST, E3, REST, D3, REST, B2, REST};
    int bass_gates[] = { 2,   0,  2,   0,  2,   0,  2,   0,
                         2,   0,  2,   0,  2,   0,  2,   0};
    float bass_vel[] = {.50,  0, .42,   0, .45,  0, .40,   0,
                        .48,  0, .40,   0, .45,  0, .42,   0};
    for (int s = 0; s < 16; s++) {
        p->melodyNote[0][s]     = bass_notes[s];
        p->melodyGate[0][s]     = bass_gates[s];
        p->melodyVelocity[0][s] = bass_vel[s];
    }

    // ---- Lead / CALL (track 1): Vibraphone ----
    // Plays first 8 steps, rests second 8. A rising phrase with a bluesy bend.
    // G4 -> A4 -> B4 -> D5 (pentatonic rise, classic jazz)
    int call_notes[] = {REST, G4, REST, A4,  B4, REST, D5, REST,
                        REST,REST,REST,REST, REST,REST,REST,REST};
    int call_gates[] = {  0,   2,   0,  2,    3,   0,  4,   0,
                          0,   0,   0,  0,    0,   0,  0,   0};
    float call_vel[] = {  0, .35,   0, .38,  .42,  0, .45,   0,
                          0,   0,   0,  0,    0,   0,  0,   0};
    for (int s = 0; s < 16; s++) {
        p->melodyNote[1][s]     = call_notes[s];
        p->melodyGate[1][s]     = call_gates[s];
        p->melodyVelocity[1][s] = call_vel[s];
    }
    p->melodyAccent[1][6] = true;   // accent the high D5

    // ---- Chord / RESPONSE (track 2): FM Electric Piano ----
    // Rests first 8 steps, responds second 8. Descending answer.
    // C5 -> B4 -> A4 -> G4 (mirrors the call, coming back down)
    int resp_notes[] = {REST,REST,REST,REST, REST,REST,REST,REST,
                        REST, Cn5, REST, B4,  A4, REST, G4, REST};
    int resp_gates[] = {  0,   0,   0,  0,    0,   0,  0,   0,
                          0,   2,   0,  2,    3,   0,  4,   0};
    float resp_vel[] = {  0,   0,   0,  0,    0,   0,  0,   0,
                          0, .38,   0, .35,  .40,  0, .42,   0};
    for (int s = 0; s < 16; s++) {
        p->melodyNote[2][s]     = resp_notes[s];
        p->melodyGate[2][s]     = resp_gates[s];
        p->melodyVelocity[2][s] = resp_vel[s];
    }
    p->melodyAccent[2][14] = true;  // accent the landing G4
}

// --- Pattern B: FM calls, vibes respond (roles swapped) ---

static void Song_JazzCallResponse_PatternB(Pattern* p) {
    initPattern(p);

    // Same drums
    p->drumSteps[0][0]  = true;  p->drumVelocity[0][0]  = 0.35f;
    p->drumSteps[0][8]  = true;  p->drumVelocity[0][8]  = 0.28f;
    p->drumSteps[1][4]  = true;  p->drumVelocity[1][4]  = 0.15f;
    p->drumSteps[1][12] = true;  p->drumVelocity[1][12] = 0.18f;
    for (int s = 0; s < 16; s += 2) {
        p->drumSteps[2][s] = true;
        p->drumVelocity[2][s] = (s % 4 == 0) ? 0.30f : 0.18f;
    }
    p->drumSteps[2][3]  = true;  p->drumVelocity[2][3]  = 0.10f;
    p->drumSteps[2][7]  = true;  p->drumVelocity[2][7]  = 0.08f;
    p->drumSteps[2][11] = true;  p->drumVelocity[2][11] = 0.10f;
    p->drumSteps[2][15] = true;  p->drumVelocity[2][15] = 0.08f;

    // Walking bass: different path — G2 -> F2 -> E2 -> D2 (descending this time)
    int bass_notes[] = {G2, REST, F2, REST, E2, REST, D2, REST,
                        Cn3, REST, B2, REST, A2, REST, G2, REST};
    int bass_gates[] = { 2,   0,  2,   0,  2,   0,  2,   0,
                         2,   0,  2,   0,  2,   0,  2,   0};
    float bass_vel[] = {.50,  0, .42,   0, .45,  0, .40,   0,
                        .48,  0, .42,   0, .40,  0, .45,   0};
    for (int s = 0; s < 16; s++) {
        p->melodyNote[0][s]     = bass_notes[s];
        p->melodyGate[0][s]     = bass_gates[s];
        p->melodyVelocity[0][s] = bass_vel[s];
    }

    // FM CALLS first half (track 2) — a questioning phrase, ends on the 7th (F)
    int call_notes[] = {REST, D4, REST, E4,  F4, REST, A4, REST,
                        REST,REST,REST,REST, REST,REST,REST,REST};
    int call_gates[] = {  0,   2,   0,  2,    2,   0,  3,   0,
                          0,   0,   0,  0,    0,   0,  0,   0};
    float call_vel[] = {  0, .38,   0, .35,  .40,  0, .42,   0,
                          0,   0,   0,  0,    0,   0,  0,   0};
    for (int s = 0; s < 16; s++) {
        p->melodyNote[2][s]     = call_notes[s];
        p->melodyGate[2][s]     = call_gates[s];
        p->melodyVelocity[2][s] = call_vel[s];
    }

    // Vibes RESPOND second half (track 1) — resolves down to G
    int resp_notes[] = {REST,REST,REST,REST, REST,REST,REST,REST,
                        REST, E5, REST, D5,  B4, REST, G4, REST};
    int resp_gates[] = {  0,   0,   0,  0,    0,   0,  0,   0,
                          0,   2,   0,  2,    3,   0,  4,   0};
    float resp_vel[] = {  0,   0,   0,  0,    0,   0,  0,   0,
                          0, .35,   0, .38,  .40,  0, .45,   0};
    for (int s = 0; s < 16; s++) {
        p->melodyNote[1][s]     = resp_notes[s];
        p->melodyGate[1][s]     = resp_gates[s];
        p->melodyVelocity[1][s] = resp_vel[s];
    }
    p->melodySlide[1][14] = true;  // smooth slide down to final G
}

// --- Pattern C: trading twos — fast back and forth ---

static void Song_JazzCallResponse_PatternC(Pattern* p) {
    initPattern(p);

    // Same drums but busier — add ghost snare
    p->drumSteps[0][0]  = true;  p->drumVelocity[0][0]  = 0.35f;
    p->drumSteps[0][8]  = true;  p->drumVelocity[0][8]  = 0.30f;
    p->drumSteps[1][4]  = true;  p->drumVelocity[1][4]  = 0.18f;
    p->drumSteps[1][12] = true;  p->drumVelocity[1][12] = 0.20f;
    p->drumSteps[1][2]  = true;  p->drumVelocity[1][2]  = 0.05f;  // ghost
    p->drumSteps[1][10] = true;  p->drumVelocity[1][10] = 0.06f;  // ghost
    for (int s = 0; s < 16; s += 2) {
        p->drumSteps[2][s] = true;
        p->drumVelocity[2][s] = (s % 4 == 0) ? 0.30f : 0.18f;
    }
    p->drumSteps[2][3]  = true;  p->drumVelocity[2][3]  = 0.12f;
    p->drumSteps[2][7]  = true;  p->drumVelocity[2][7]  = 0.10f;
    p->drumSteps[2][11] = true;  p->drumVelocity[2][11] = 0.12f;
    p->drumSteps[2][15] = true;  p->drumVelocity[2][15] = 0.10f;

    // Bass: steady quarter notes, root-fifth alternation
    int bass_notes[] = {G2, REST, REST, REST, D3, REST, REST, REST,
                        G2, REST, REST, REST, D3, REST, REST, REST};
    int bass_gates[] = { 3,   0,   0,   0,   3,   0,   0,   0,
                         3,   0,   0,   0,   3,   0,   0,   0};
    float bass_vel[] = {.50,  0,   0,   0,  .45,  0,   0,   0,
                        .48,  0,   0,   0,  .45,  0,   0,   0};
    for (int s = 0; s < 16; s++) {
        p->melodyNote[0][s]     = bass_notes[s];
        p->melodyGate[0][s]     = bass_gates[s];
        p->melodyVelocity[0][s] = bass_vel[s];
    }

    // Vibes: steps 0-3 and 8-11 (beats 1-2 and 3-4 of each half)
    int vib_notes[] = { B4, REST, D5, REST, REST,REST,REST,REST,
                        A4, REST, G4, REST, REST,REST,REST,REST};
    int vib_gates[] = {  2,   0,  2,   0,    0,  0,  0,  0,
                         2,   0,  2,   0,    0,  0,  0,  0};
    float vib_vel[] = {.40,   0, .38,  0,    0,  0,  0,  0,
                       .35,   0, .40,  0,    0,  0,  0,  0};
    for (int s = 0; s < 16; s++) {
        p->melodyNote[1][s]     = vib_notes[s];
        p->melodyGate[1][s]     = vib_gates[s];
        p->melodyVelocity[1][s] = vib_vel[s];
    }

    // FM: steps 4-7 and 12-15 (fills in the gaps — real conversation)
    int fm_notes[]  = {REST,REST,REST,REST, Cn5, REST, B4, REST,
                       REST,REST,REST,REST,  F4, REST, E4, REST};
    int fm_gates[]  = {  0,  0,  0,  0,      2,   0,  2,   0,
                         0,  0,  0,  0,      2,   0,  3,   0};
    float fm_vel[]  = {  0,  0,  0,  0,    .38,   0, .35,   0,
                         0,  0,  0,  0,    .40,   0, .42,   0};
    for (int s = 0; s < 16; s++) {
        p->melodyNote[2][s]     = fm_notes[s];
        p->melodyGate[2][s]     = fm_gates[s];
        p->melodyVelocity[2][s] = fm_vel[s];
    }
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
    for (int s = 0; s < 16; s += 4) {
        p->drumSteps[0][s] = true;
        p->drumVelocity[0][s] = 0.70f;
    }

    // Snare/clap on 2 and 4
    p->drumSteps[1][4]  = true;  p->drumVelocity[1][4]  = 0.45f;
    p->drumSteps[1][12] = true;  p->drumVelocity[1][12] = 0.48f;

    // HiHat: offbeat 8ths (classic house open hat)
    for (int s = 2; s < 16; s += 4) {
        p->drumSteps[2][s] = true;
        p->drumVelocity[2][s] = 0.35f;
    }
    // closed hat on downbeats for texture
    p->drumSteps[2][0]  = true;  p->drumVelocity[2][0]  = 0.15f;
    p->drumSteps[2][4]  = true;  p->drumVelocity[2][4]  = 0.12f;
    p->drumSteps[2][8]  = true;  p->drumVelocity[2][8]  = 0.15f;
    p->drumSteps[2][12] = true;  p->drumVelocity[2][12] = 0.12f;

    // Perc: shaker/rimshot on 16ths with probability
    for (int s = 1; s < 16; s += 2) {
        p->drumSteps[3][s] = true;
        p->drumVelocity[3][s] = 0.08f;
        p->drumPitch[3][s] = 0.5f;  // high pitched
        p->drumProbability[3][s] = 0.4f;
    }

    // Acid bass: C2 driving 8th note pattern with rests for groove
    //   x . x . x x . x . x . x x . . x
    int bass_notes[] = {Cn2,REST, Cn2,REST, Cn2, Cn2,REST, Eb2,
                        REST,Cn2,REST, Cn2,  Cn2,REST,REST, G2};
    int bass_gates[] = {  2,  0,   2,  0,   1,   2,  0,   2,
                          0,  2,   0,  1,    2,  0,  0,   2};
    float bass_vel[] = {.55, 0,  .48, 0,   .42, .50, 0,  .52,
                         0, .48,  0, .40,  .55, 0,   0,  .45};
    for (int s = 0; s < 16; s++) {
        p->melodyNote[0][s]     = bass_notes[s];
        p->melodyGate[0][s]     = bass_gates[s];
        p->melodyVelocity[0][s] = bass_vel[s];
    }
    // 303-style slides for acid feel
    p->melodySlide[0][5]  = true;   // slide into Eb
    p->melodySlide[0][15] = true;   // slide into G
    p->melodyAccent[0][0] = true;   // accent downbeat
    p->melodyAccent[0][12] = true;

    // Chord stabs (track 1): offbeat Cm stabs
    p->melodyNote[1][2]     = Cn4;
    p->melodyVelocity[1][2] = 0.35f;
    p->melodyGate[1][2]     = 1;
    p->melodyNotePool[1][2].enabled   = true;
    p->melodyNotePool[1][2].chordType = CHORD_TRIAD;
    p->melodyNotePool[1][2].pickMode  = PICK_CYCLE_UP;

    p->melodyNote[1][6]     = Cn4;
    p->melodyVelocity[1][6] = 0.30f;
    p->melodyGate[1][6]     = 1;
    p->melodyNotePool[1][6].enabled   = true;
    p->melodyNotePool[1][6].chordType = CHORD_TRIAD;
    p->melodyNotePool[1][6].pickMode  = PICK_CYCLE_UP;

    p->melodyNote[1][10]     = Ab3;
    p->melodyVelocity[1][10] = 0.32f;
    p->melodyGate[1][10]     = 1;
    p->melodyNotePool[1][10].enabled   = true;
    p->melodyNotePool[1][10].chordType = CHORD_TRIAD;
    p->melodyNotePool[1][10].pickMode  = PICK_CYCLE_UP;

    // Pad (track 2): Cm held whole bar, very quiet
    p->melodyNote[2][0]     = Cn3;
    p->melodyVelocity[2][0] = 0.15f;
    p->melodyGate[2][0]     = 16;
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_SEVENTH;
    p->melodyNotePool[2][0].pickMode  = PICK_RANDOM;
}

// --- Pattern B: breakdown (filter closes, drums thin out) ---

static void Song_House_PatternB(Pattern* p) {
    initPattern(p);

    // Kick: only beat 1 — tension
    p->drumSteps[0][0] = true;  p->drumVelocity[0][0] = 0.55f;

    // No clap, no perc — stripped back

    // HiHat: sparse offbeats only
    p->drumSteps[2][2]  = true;  p->drumVelocity[2][2]  = 0.20f;
    p->drumSteps[2][10] = true;  p->drumVelocity[2][10] = 0.18f;

    // Bass: just the root, held long — breathing room
    p->melodyNote[0][0]     = Cn2;
    p->melodyVelocity[0][0] = 0.45f;
    p->melodyGate[0][0]     = 8;

    p->melodyNote[0][8]     = Eb2;
    p->melodyVelocity[0][8] = 0.40f;
    p->melodyGate[0][8]     = 8;
    p->melodySlide[0][8]    = true;

    // No stabs — just the pad, quieter
    p->melodyNote[2][0]     = Cn3;
    p->melodyVelocity[2][0] = 0.12f;
    p->melodyGate[2][0]     = 16;
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

    // Kick: beats 1 and 3 only — deep, sparse
    p->drumSteps[0][0]  = true;  p->drumVelocity[0][0]  = 0.60f;
    p->drumSteps[0][8]  = true;  p->drumVelocity[0][8]  = 0.50f;

    // Clap: soft on 2 only (no 4 — asymmetric = hypnotic)
    p->drumSteps[1][4]  = true;  p->drumVelocity[1][4]  = 0.25f;

    // HiHat: closed 16ths, very quiet — just a shimmer
    for (int s = 0; s < 16; s += 2) {
        p->drumSteps[2][s] = true;
        p->drumVelocity[2][s] = 0.10f + ((s % 8 == 0) ? 0.08f : 0.0f);
    }

    // Perc: rim click, sparse and random
    p->drumSteps[3][3]  = true;  p->drumVelocity[3][3]  = 0.12f;
    p->drumProbability[3][3] = 0.6f;
    p->drumSteps[3][11] = true;  p->drumVelocity[3][11] = 0.10f;
    p->drumProbability[3][11] = 0.4f;

    // Sub bass: F1 and C2, long notes — felt not heard
    p->melodyNote[0][0]      = F1;
    p->melodyVelocity[0][0]  = 0.50f;
    p->melodyGate[0][0]      = 8;

    p->melodyNote[0][8]      = Cn2;
    p->melodyVelocity[0][8]  = 0.45f;
    p->melodyGate[0][8]      = 8;

    // Stab/lead (track 1): single FM note, very sparse — just a color
    p->melodyNote[1][6]      = Cn4;
    p->melodyVelocity[1][6]  = 0.25f;
    p->melodyGate[1][6]      = 2;

    p->melodyNote[1][14]     = Ab3;
    p->melodyVelocity[1][14] = 0.20f;
    p->melodyGate[1][14]     = 2;
    p->drumProbability[1][14] = 0.5f;  // sometimes absent

    // Pad (track 2): Fm held whole bar — this is the sweep canvas
    p->melodyNote[2][0]      = Fn3;
    p->melodyVelocity[2][0]  = 0.20f;
    p->melodyGate[2][0]      = 16;
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_SEVENTH;
    p->melodyNotePool[2][0].pickMode  = PICK_CYCLE_UP;
}

// --- Pattern B: even more minimal — pad solo with filter breathing ---

static void Song_DeepHouse_PatternB(Pattern* p) {
    initPattern(p);

    // Kick: beat 1 only
    p->drumSteps[0][0] = true;  p->drumVelocity[0][0] = 0.50f;

    // Single rim click
    p->drumSteps[3][7] = true;  p->drumVelocity[3][7] = 0.10f;

    // Sub bass: F1 whole bar
    p->melodyNote[0][0]     = F1;
    p->melodyVelocity[0][0] = 0.45f;
    p->melodyGate[0][0]     = 16;

    // No stabs — silence in the mid-range

    // Pad: Fm7, whole bar — the filter does all the work
    p->melodyNote[2][0]     = Fn3;
    p->melodyVelocity[2][0] = 0.22f;
    p->melodyGate[2][0]     = 16;
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

    // ---- Drums: the Dilla pattern ----
    // Kick (track 0): syncopated — NOT on 1. Lands on step 1 (late), step 7, step 10.
    // The swing settings push these even further off-grid.
    p->drumSteps[0][1]  = true;  p->drumVelocity[0][1]  = 0.60f;  // late beat 1
    p->drumSteps[0][7]  = true;  p->drumVelocity[0][7]  = 0.50f;  // and-of-2
    p->drumSteps[0][10] = true;  p->drumVelocity[0][10] = 0.55f;  // beat 3 pushed
    p->drumSteps[0][14] = true;  p->drumVelocity[0][14] = 0.35f;  // ghost kick
    p->drumProbability[0][14] = 0.6f;

    // Snare (track 1): beat 2 and 4 but LAZY (snareDelay=5 in bridge)
    // Plus ghost notes everywhere — the Dilla signature
    p->drumSteps[1][4]  = true;  p->drumVelocity[1][4]  = 0.40f;  // beat 2
    p->drumSteps[1][12] = true;  p->drumVelocity[1][12] = 0.42f;  // beat 4
    // ghost snares
    p->drumSteps[1][2]  = true;  p->drumVelocity[1][2]  = 0.08f;
    p->drumSteps[1][6]  = true;  p->drumVelocity[1][6]  = 0.06f;
    p->drumProbability[1][6] = 0.4f;
    p->drumSteps[1][9]  = true;  p->drumVelocity[1][9]  = 0.07f;
    p->drumProbability[1][9] = 0.5f;
    p->drumSteps[1][15] = true;  p->drumVelocity[1][15] = 0.05f;
    p->drumProbability[1][15] = 0.3f;

    // HiHat (track 2): 16th note pattern but with velocity variation
    // Classic Dilla: some hats loud, some barely there, some missing
    float hh_vel[] = {.20,.08,.25,.06, .18,.04,.22,.10, .20,.06,.25,.04, .15,.08,.20,.05};
    for (int s = 0; s < 16; s++) {
        p->drumSteps[2][s] = true;
        p->drumVelocity[2][s] = hh_vel[s];
    }
    // Some hats randomly absent
    p->drumProbability[2][3]  = 0.5f;
    p->drumProbability[2][7]  = 0.6f;
    p->drumProbability[2][11] = 0.5f;
    p->drumProbability[2][15] = 0.4f;

    // Perc (track 3): shaker on 8ths, very quiet
    for (int s = 0; s < 16; s += 2) {
        p->drumSteps[3][s] = true;
        p->drumVelocity[3][s] = 0.06f;
        p->drumPitch[3][s] = 0.4f;
    }

    // ---- Bass (track 0): walking Eb with chromatic passing tones ----
    // Eb2 -> G2 -> Ab2 -> Bb2 -> (chromatic walk up to root)
    int bass_notes[] = {Eb2,REST, G2, REST, Ab2,REST, Bb2,REST,
                        Eb2,REST,REST, D2,  Eb2,REST,REST,REST};
    int bass_gates[] = {  2,  0,  2,   0,   2,   0,   2,  0,
                          3,  0,  0,   2,    2,  0,  0,  0};
    float bass_vel[] = {.50, 0, .42,   0,  .45,  0,  .40, 0,
                        .52, 0,   0, .35,  .48,  0,   0,  0};
    for (int s = 0; s < 16; s++) {
        p->melodyNote[0][s]     = bass_notes[s];
        p->melodyGate[0][s]     = bass_gates[s];
        p->melodyVelocity[0][s] = bass_vel[s];
    }

    // ---- Keys (track 1): Rhodes stabs — soulful, on the upbeats ----
    // Eb major 7 chord (Eb G Bb D) played as stabs on offbeats
    p->melodyNote[1][2]     = Eb4;
    p->melodyVelocity[1][2] = 0.32f;
    p->melodyGate[1][2]     = 2;
    p->melodyNotePool[1][2].enabled   = true;
    p->melodyNotePool[1][2].chordType = CHORD_SEVENTH;
    p->melodyNotePool[1][2].pickMode  = PICK_CYCLE_UP;

    p->melodyNote[1][6]     = Eb4;
    p->melodyVelocity[1][6] = 0.28f;
    p->melodyGate[1][6]     = 1;
    p->melodyAccent[1][6]   = true;

    p->melodyNote[1][11]    = Ab3;
    p->melodyVelocity[1][11]= 0.30f;
    p->melodyGate[1][11]    = 2;
    p->melodyNotePool[1][11].enabled   = true;
    p->melodyNotePool[1][11].chordType = CHORD_TRIAD;
    p->melodyNotePool[1][11].pickMode  = PICK_RANDOM;

    // ---- Pad (track 2): held Eb chord, very quiet, vinyl warmth ----
    p->melodyNote[2][0]     = Eb3;
    p->melodyVelocity[2][0] = 0.15f;
    p->melodyGate[2][0]     = 16;
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_SEVENTH;
    p->melodyNotePool[2][0].pickMode  = PICK_RANDOM;
}

// --- Pattern B: the breakdown — kick + pad only, heads still nodding ---

static void Song_Dilla_PatternB(Pattern* p) {
    initPattern(p);

    // Kick: even more syncopated, minimal
    p->drumSteps[0][1]  = true;  p->drumVelocity[0][1]  = 0.55f;
    p->drumSteps[0][9]  = true;  p->drumVelocity[0][9]  = 0.45f;

    // Ghost snares only — no main hits
    p->drumSteps[1][3]  = true;  p->drumVelocity[1][3]  = 0.06f;
    p->drumSteps[1][7]  = true;  p->drumVelocity[1][7]  = 0.08f;
    p->drumSteps[1][11] = true;  p->drumVelocity[1][11] = 0.05f;
    p->drumSteps[1][15] = true;  p->drumVelocity[1][15] = 0.07f;

    // HiHat: sparse, just a few ticks
    p->drumSteps[2][0]  = true;  p->drumVelocity[2][0]  = 0.12f;
    p->drumSteps[2][4]  = true;  p->drumVelocity[2][4]  = 0.10f;
    p->drumSteps[2][8]  = true;  p->drumVelocity[2][8]  = 0.12f;
    p->drumSteps[2][12] = true;  p->drumVelocity[2][12] = 0.08f;

    // No bass — empty space

    // Single Rhodes note, lonely
    p->melodyNote[1][6]     = Bb4;
    p->melodyVelocity[1][6] = 0.25f;
    p->melodyGate[1][6]     = 4;

    // Pad: Cm7 (relative minor) — mood shift
    p->melodyNote[2][0]     = Cn3;
    p->melodyVelocity[2][0] = 0.18f;
    p->melodyGate[2][0]     = 16;
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_SEVENTH;
    p->melodyNotePool[2][0].pickMode  = PICK_RANDOM;
}

// --- Pattern C: full groove — bass gets busier, more Rhodes ---

static void Song_Dilla_PatternC(Pattern* p) {
    initPattern(p);

    // Full drums — same as A but with extra kick ghost
    p->drumSteps[0][1]  = true;  p->drumVelocity[0][1]  = 0.60f;
    p->drumSteps[0][5]  = true;  p->drumVelocity[0][5]  = 0.30f;  // ghost kick
    p->drumProbability[0][5] = 0.5f;
    p->drumSteps[0][7]  = true;  p->drumVelocity[0][7]  = 0.50f;
    p->drumSteps[0][10] = true;  p->drumVelocity[0][10] = 0.55f;
    p->drumSteps[0][13] = true;  p->drumVelocity[0][13] = 0.35f;

    p->drumSteps[1][4]  = true;  p->drumVelocity[1][4]  = 0.42f;
    p->drumSteps[1][12] = true;  p->drumVelocity[1][12] = 0.45f;
    p->drumSteps[1][2]  = true;  p->drumVelocity[1][2]  = 0.07f;
    p->drumSteps[1][6]  = true;  p->drumVelocity[1][6]  = 0.06f;
    p->drumSteps[1][10] = true;  p->drumVelocity[1][10] = 0.08f;
    p->drumSteps[1][14] = true;  p->drumVelocity[1][14] = 0.05f;

    // Full 16th hats
    float hh_vel[] = {.22,.10,.28,.05, .20,.06,.25,.12, .22,.08,.28,.05, .18,.10,.22,.06};
    for (int s = 0; s < 16; s++) {
        p->drumSteps[2][s] = true;
        p->drumVelocity[2][s] = hh_vel[s];
    }
    p->drumProbability[2][3]  = 0.6f;
    p->drumProbability[2][11] = 0.5f;

    // Busier bass: chromatic run Eb2 -> E2(!) -> F2 -> G2 -> Ab2 -> Bb2
    // The E natural is the blue note — nasty and beautiful
    int bass_notes[] = {Eb2,REST, E2, REST,  F2, REST, G2, REST,
                        Ab2,REST, Bb2,REST,  Eb2,REST, D2, Eb2};
    int bass_gates[] = {  2,  0,  1,   0,    2,   0,  1,   0,
                          2,  0,  2,   0,    2,   0,  1,   1};
    float bass_vel[] = {.52, 0, .38,   0,  .45,   0, .40,  0,
                        .48, 0, .42,   0,  .55,   0, .35, .48};
    for (int s = 0; s < 16; s++) {
        p->melodyNote[0][s]     = bass_notes[s];
        p->melodyGate[0][s]     = bass_gates[s];
        p->melodyVelocity[0][s] = bass_vel[s];
    }

    // More Rhodes stabs — busier call and response with the bass
    p->melodyNote[1][2]     = Eb4;
    p->melodyVelocity[1][2] = 0.35f;
    p->melodyGate[1][2]     = 1;
    p->melodyNotePool[1][2].enabled   = true;
    p->melodyNotePool[1][2].chordType = CHORD_SEVENTH;
    p->melodyNotePool[1][2].pickMode  = PICK_CYCLE_UP;

    p->melodyNote[1][5]     = Bb3;
    p->melodyVelocity[1][5] = 0.28f;
    p->melodyGate[1][5]     = 2;
    p->melodyAccent[1][5]   = true;

    p->melodyNote[1][9]     = Ab3;
    p->melodyVelocity[1][9] = 0.32f;
    p->melodyGate[1][9]     = 2;
    p->melodyNotePool[1][9].enabled   = true;
    p->melodyNotePool[1][9].chordType = CHORD_TRIAD;
    p->melodyNotePool[1][9].pickMode  = PICK_RANDOM;

    p->melodyNote[1][13]     = G4;
    p->melodyVelocity[1][13] = 0.25f;
    p->melodyGate[1][13]     = 2;

    // Pad: back to Eb maj7
    p->melodyNote[2][0]     = Eb3;
    p->melodyVelocity[2][0] = 0.18f;
    p->melodyGate[2][0]     = 16;
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

    // ---- Drums: gentle, spacious ----
    // Kick: just beat 1 — a heartbeat
    p->drumSteps[0][0]  = true;  p->drumVelocity[0][0]  = 0.35f;

    // HiHat: soft shimmer on 8ths, very quiet
    for (int s = 0; s < 16; s += 2) {
        p->drumSteps[2][s] = true;
        p->drumVelocity[2][s] = 0.08f + ((s == 0) ? 0.04f : 0.0f);
    }

    // Perc: distant chime on step 12
    p->drumSteps[3][12] = true;  p->drumVelocity[3][12] = 0.06f;
    p->drumPitch[3][12] = 0.6f;
    p->drumProbability[3][12] = 0.5f;

    // ---- Bass (track 0): plucked, simple D pedal ----
    p->melodyNote[0][0]     = D2;
    p->melodyVelocity[0][0] = 0.40f;
    p->melodyGate[0][0]     = 8;

    p->melodyNote[0][8]     = A2;
    p->melodyVelocity[0][8] = 0.35f;
    p->melodyGate[0][8]     = 8;

    // ---- Lead (track 1): glockenspiel — a few bright notes emerging ----
    // D pentatonic: D E F# A B
    p->melodyNote[1][4]     = Fs4;
    p->melodyVelocity[1][4] = 0.22f;
    p->melodyGate[1][4]     = 3;

    p->melodyNote[1][10]    = A4;
    p->melodyVelocity[1][10]= 0.20f;
    p->melodyGate[1][10]    = 3;

    // ---- Pad (track 2): strings, building ----
    p->melodyNote[2][0]     = D3;
    p->melodyVelocity[2][0] = 0.12f;
    p->melodyGate[2][0]     = 16;
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_TRIAD;
    p->melodyNotePool[2][0].pickMode  = PICK_CYCLE_UP;
}

// --- Pattern B: full bloom — the melody soars ---

static void Song_Atmosphere_PatternB(Pattern* p) {
    initPattern(p);

    // Kick: 1 and 3, warm
    p->drumSteps[0][0]  = true;  p->drumVelocity[0][0]  = 0.38f;
    p->drumSteps[0][8]  = true;  p->drumVelocity[0][8]  = 0.30f;

    // HiHat: fuller shimmer
    for (int s = 0; s < 16; s += 2) {
        p->drumSteps[2][s] = true;
        p->drumVelocity[2][s] = 0.10f + ((s % 4 == 0) ? 0.05f : 0.0f);
    }
    // Add some 16th ghost hats for sparkle
    p->drumSteps[2][3]  = true;  p->drumVelocity[2][3]  = 0.05f;
    p->drumProbability[2][3] = 0.4f;
    p->drumSteps[2][11] = true;  p->drumVelocity[2][11] = 0.05f;
    p->drumProbability[2][11] = 0.3f;

    // Perc: chimes
    p->drumSteps[3][4]  = true;  p->drumVelocity[3][4]  = 0.05f;
    p->drumPitch[3][4]  = 0.7f;
    p->drumSteps[3][12] = true;  p->drumVelocity[3][12] = 0.06f;
    p->drumPitch[3][12] = 0.5f;

    // Bass: D -> F# -> A -> D walking up — joyful
    int bass_notes[] = {D2, REST, REST, REST, Fs2,REST, REST, REST,
                        A2, REST, REST, REST, D3, REST, REST, REST};
    int bass_gates[] = { 3,   0,   0,   0,   3,   0,    0,   0,
                         3,   0,   0,   0,   3,   0,    0,   0};
    float bass_vel[] = {.42,  0,   0,   0,  .38,  0,    0,   0,
                        .40,  0,   0,   0,  .45,  0,    0,   0};
    for (int s = 0; s < 16; s++) {
        p->melodyNote[0][s]     = bass_notes[s];
        p->melodyGate[0][s]     = bass_gates[s];
        p->melodyVelocity[0][s] = bass_vel[s];
    }

    // Lead: glockenspiel melody — rising, soaring, happy
    // D5 -> E5 -> F#5(=78) -> A4 -> B4 -> D5
    int lead_notes[] = {REST, D5, REST, REST, E5, REST, REST, F5,
                        REST, REST, A4, REST, B4, REST, D5, REST};
    int lead_gates[] = {  0,   3,   0,   0,  3,    0,   0,  4,
                          0,   0,   2,   0,  2,    0,   4,   0};
    float lead_vel[] = {  0, .28,   0,   0, .30,   0,   0, .35,
                          0,   0, .25,   0, .28,   0, .32,   0};
    for (int s = 0; s < 16; s++) {
        p->melodyNote[1][s]     = lead_notes[s];
        p->melodyGate[1][s]     = lead_gates[s];
        p->melodyVelocity[1][s] = lead_vel[s];
    }

    // Pad: D major -> A major (IV-I feel, triumphant)
    p->melodyNote[2][0]     = D3;
    p->melodyVelocity[2][0] = 0.20f;
    p->melodyGate[2][0]     = 8;
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_SEVENTH;
    p->melodyNotePool[2][0].pickMode  = PICK_CYCLE_UP;

    p->melodyNote[2][8]     = A3;
    p->melodyVelocity[2][8] = 0.22f;
    p->melodyGate[2][8]     = 8;
    p->melodyNotePool[2][8].enabled   = true;
    p->melodyNotePool[2][8].chordType = CHORD_TRIAD;
    p->melodyNotePool[2][8].pickMode  = PICK_CYCLE_UP;
}

// --- Pattern C: breathing — spacious, just pad and ornaments ---

static void Song_Atmosphere_PatternC(Pattern* p) {
    initPattern(p);

    // No kick — just floating

    // HiHat: barely there
    p->drumSteps[2][0]  = true;  p->drumVelocity[2][0]  = 0.06f;
    p->drumSteps[2][8]  = true;  p->drumVelocity[2][8]  = 0.05f;

    // Bass: single D, held the whole bar
    p->melodyNote[0][0]     = D2;
    p->melodyVelocity[0][0] = 0.30f;
    p->melodyGate[0][0]     = 16;

    // Lead: two lonely glockenspiel notes — question and answer
    p->melodyNote[1][4]     = B4;
    p->melodyVelocity[1][4] = 0.18f;
    p->melodyGate[1][4]     = 6;

    p->melodyNote[1][12]    = D5;
    p->melodyVelocity[1][12]= 0.20f;
    p->melodyGate[1][12]    = 4;
    p->melodySlide[1][12]   = true;  // gentle glide up

    // Pad: Dmaj7 held the entire bar — warm and wide
    p->melodyNote[2][0]     = D3;
    p->melodyVelocity[2][0] = 0.25f;
    p->melodyGate[2][0]     = 16;
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

    // ---- Drums: cocktail brushes ----
    // Kick: soft, beats 1 and 3
    p->drumSteps[0][0]  = true;  p->drumVelocity[0][0]  = 0.25f;
    p->drumSteps[0][8]  = true;  p->drumVelocity[0][8]  = 0.20f;

    // Snare: brush swish on 2 and 4, very soft
    p->drumSteps[1][4]  = true;  p->drumVelocity[1][4]  = 0.12f;
    p->drumSteps[1][12] = true;  p->drumVelocity[1][12] = 0.14f;

    // HiHat: ride pattern, smooth 8ths
    for (int s = 0; s < 16; s += 2) {
        p->drumSteps[2][s] = true;
        p->drumVelocity[2][s] = (s % 4 == 0) ? 0.18f : 0.12f;
    }

    // ---- Bass (track 0): walking A → B → C → D ----
    // Stepwise walk from Am up to Dm
    int bass_notes[] = {A2, REST, REST, REST,  B2, REST, REST, REST,
                        Cn3,REST, REST, REST,  D3, REST, REST, REST};
    int bass_gates[] = { 3,   0,   0,   0,    3,   0,   0,   0,
                         3,   0,   0,   0,    3,   0,   0,   0};
    float bass_vel[] = {.48,  0,   0,   0,  .40,   0,   0,   0,
                        .42,  0,   0,   0,  .45,   0,   0,   0};
    for (int s = 0; s < 16; s++) {
        p->melodyNote[0][s]     = bass_notes[s];
        p->melodyGate[0][s]     = bass_gates[s];
        p->melodyVelocity[0][s] = bass_vel[s];
    }

    // ---- Lead (track 1): vibes melody — gentle, rising ----
    // E4 on beat 2, holds, then C5 on the and-of-3
    p->melodyNote[1][4]     = E4;
    p->melodyVelocity[1][4] = 0.30f;
    p->melodyGate[1][4]     = 4;

    p->melodyNote[1][10]    = Cn5;
    p->melodyVelocity[1][10]= 0.25f;
    p->melodyGate[1][10]    = 3;

    // ---- Pad (track 2): Am7 → Dm ----
    p->melodyNote[2][0]     = A3;
    p->melodyVelocity[2][0] = 0.20f;
    p->melodyGate[2][0]     = 8;
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_SEVENTH;
    p->melodyNotePool[2][0].pickMode  = PICK_CYCLE_UP;

    p->melodyNote[2][8]     = D3;
    p->melodyVelocity[2][8] = 0.18f;
    p->melodyGate[2][8]     = 8;
    p->melodyNotePool[2][8].enabled   = true;
    p->melodyNotePool[2][8].chordType = CHORD_SEVENTH;
    p->melodyNotePool[2][8].pickMode  = PICK_CYCLE_UP;
}

// --- Pattern B: G7 → Cmaj7 (the lift, brightening) ---

static void Song_MrLucky_PatternB(Pattern* p) {
    initPattern(p);

    // Same brushes
    p->drumSteps[0][0]  = true;  p->drumVelocity[0][0]  = 0.25f;
    p->drumSteps[0][8]  = true;  p->drumVelocity[0][8]  = 0.20f;
    p->drumSteps[1][4]  = true;  p->drumVelocity[1][4]  = 0.12f;
    p->drumSteps[1][12] = true;  p->drumVelocity[1][12] = 0.14f;
    for (int s = 0; s < 16; s += 2) {
        p->drumSteps[2][s] = true;
        p->drumVelocity[2][s] = (s % 4 == 0) ? 0.18f : 0.12f;
    }

    // Bass: walking G → A → B → C (up to the major)
    int bass_notes[] = {G2, REST, REST, REST,  A2, REST, REST, REST,
                        B2, REST, REST, REST,  Cn3,REST, REST, REST};
    int bass_gates[] = { 3,   0,   0,   0,    3,   0,   0,   0,
                         3,   0,   0,   0,    3,   0,   0,   0};
    float bass_vel[] = {.48,  0,   0,   0,  .40,   0,   0,   0,
                        .42,  0,   0,   0,  .45,   0,   0,   0};
    for (int s = 0; s < 16; s++) {
        p->melodyNote[0][s]     = bass_notes[s];
        p->melodyGate[0][s]     = bass_gates[s];
        p->melodyVelocity[0][s] = bass_vel[s];
    }

    // Vibes: D4 on beat 1, then up to G4 on beat 3 — the bright lift
    p->melodyNote[1][1]     = D4;
    p->melodyVelocity[1][1] = 0.28f;
    p->melodyGate[1][1]     = 3;

    p->melodyNote[1][6]     = E4;
    p->melodyVelocity[1][6] = 0.25f;
    p->melodyGate[1][6]     = 2;

    p->melodyNote[1][10]    = G4;
    p->melodyVelocity[1][10]= 0.30f;
    p->melodyGate[1][10]    = 4;

    // Pad: G → C (the brightness arrives)
    p->melodyNote[2][0]     = G3;
    p->melodyVelocity[2][0] = 0.18f;
    p->melodyGate[2][0]     = 8;
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_SEVENTH;
    p->melodyNotePool[2][0].pickMode  = PICK_CYCLE_UP;

    p->melodyNote[2][8]     = Cn3;
    p->melodyVelocity[2][8] = 0.22f;
    p->melodyGate[2][8]     = 8;
    p->melodyNotePool[2][8].enabled   = true;
    p->melodyNotePool[2][8].chordType = CHORD_SEVENTH;
    p->melodyNotePool[2][8].pickMode  = PICK_CYCLE_UP;
}

// --- Pattern C: Fmaj7 → E7 (tension, wants to resolve back to Am) ---

static void Song_MrLucky_PatternC(Pattern* p) {
    initPattern(p);

    // Drums: same but add a soft perc hit for tension
    p->drumSteps[0][0]  = true;  p->drumVelocity[0][0]  = 0.25f;
    p->drumSteps[0][8]  = true;  p->drumVelocity[0][8]  = 0.22f;
    p->drumSteps[1][4]  = true;  p->drumVelocity[1][4]  = 0.12f;
    p->drumSteps[1][12] = true;  p->drumVelocity[1][12] = 0.15f;
    for (int s = 0; s < 16; s += 2) {
        p->drumSteps[2][s] = true;
        p->drumVelocity[2][s] = (s % 4 == 0) ? 0.18f : 0.12f;
    }
    // Perc: gentle shaker on beat 4, signals the turnaround
    p->drumSteps[3][12] = true;  p->drumVelocity[3][12] = 0.08f;
    p->drumPitch[3][12] = 0.4f;

    // Bass: F → E → F → E (oscillating tension before resolution)
    int bass_notes[] = {F2, REST, REST, REST,  E2, REST, REST, REST,
                        F2, REST, REST, REST,  E2, REST, REST, REST};
    int bass_gates[] = { 3,   0,   0,   0,    3,   0,   0,   0,
                         3,   0,   0,   0,    3,   0,   0,   0};
    float bass_vel[] = {.45,  0,   0,   0,  .42,   0,   0,   0,
                        .48,  0,   0,   0,  .50,   0,   0,   0};
    for (int s = 0; s < 16; s++) {
        p->melodyNote[0][s]     = bass_notes[s];
        p->melodyGate[0][s]     = bass_gates[s];
        p->melodyVelocity[0][s] = bass_vel[s];
    }

    // Vibes: descending melody — A4 → G4 → F4 → E4 (chromatic tension)
    p->melodyNote[1][0]     = A4;
    p->melodyVelocity[1][0] = 0.28f;
    p->melodyGate[1][0]     = 3;

    p->melodyNote[1][4]     = G4;
    p->melodyVelocity[1][4] = 0.25f;
    p->melodyGate[1][4]     = 3;

    p->melodyNote[1][9]     = F4;
    p->melodyVelocity[1][9] = 0.27f;
    p->melodyGate[1][9]     = 3;

    p->melodyNote[1][13]    = E4;
    p->melodyVelocity[1][13]= 0.30f;
    p->melodyGate[1][13]    = 3;
    p->melodySlide[1][13]   = true;  // slides into the resolution

    // Pad: Fmaj7 → E (dominant, yearning for Am)
    p->melodyNote[2][0]     = F3;
    p->melodyVelocity[2][0] = 0.20f;
    p->melodyGate[2][0]     = 8;
    p->melodyNotePool[2][0].enabled   = true;
    p->melodyNotePool[2][0].chordType = CHORD_SEVENTH;
    p->melodyNotePool[2][0].pickMode  = PICK_CYCLE_UP;

    p->melodyNote[2][8]     = E3;
    p->melodyVelocity[2][8] = 0.22f;
    p->melodyGate[2][8]     = 8;
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

#endif // SONGS_H
