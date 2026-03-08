#include "sound_synth_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../vendor/raylib.h"
#include "soundsystem/soundsystem.h"

// Song definitions (uses synth/drums/sequencer types from soundsystem.h)
#include "songs.h"

// ============================================================================
// Phrase player (existing functionality)
// ============================================================================

typedef struct {
    SoundPhrase phrase;
    int tokenIndex;
    float tokenTimer;
    float gapTimer;
    int currentVoice;
    bool active;
    SoundSong song;
    int phraseIndex;
    bool songActive;
} SoundPhrasePlayer;

// ============================================================================
// Song player state
// ============================================================================

#define MAX_SONG_PATTERNS 4

typedef struct {
    bool active;
    Pattern patterns[MAX_SONG_PATTERNS];
    int patternCount;
    int currentPattern;
    int loopsOnCurrent;     // how many times current pattern has looped
    int loopsPerPattern;    // how many times to loop before switching (default 2)
    float bpm;
    // Voice indices for melody tracks (so we can release them)
    int melodyVoice[SEQ_MELODY_TRACKS];
    // Filter sweep state — accumulates across bars for long sweeps
    float sweepPhase;       // 0.0 to 1.0, wraps (sine LFO position)
    float sweepRate;        // how fast the sweep moves per second (e.g. 0.02 = 50s cycle)
} SongPlayer;

// ============================================================================
// Main synth struct (now with full sound system)
// ============================================================================

struct SoundSynth {
    SoundSystem ss;         // full: synth + drums + effects + sequencer + sampler
    AudioStream stream;
    int sampleRate;
    int bufferFrames;
    bool audioReady;
    bool ownsAudioDevice;
    SoundPhrasePlayer player;
    SongPlayer songPlayer;
};

static SoundSynth* g_soundSynth = NULL;

// ============================================================================
// Audio callback — mixes synth voices + drums
// ============================================================================

static void SoundSynthCallback(void* buffer, unsigned int frames) {
    if (!g_soundSynth) return;
    short* out = (short*)buffer;
    float sr = (float)g_soundSynth->sampleRate;

    // Point global contexts to our instance
    useSoundSystem(&g_soundSynth->ss);

    for (unsigned int i = 0; i < frames; i++) {
        // Synth voices
        float sample = 0.0f;
        for (int v = 0; v < NUM_VOICES; v++) {
            sample += processVoice(&synthVoices[v], sr);
        }
        // Drums
        sample += processDrums(1.0f / sr);

        sample *= masterVolume;
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;
        out[i] = (short)(sample * 32767.0f);
    }
}

// ============================================================================
// Filter sweep helper — reads sweepPhase from song player
// Returns 0.0 to 1.0 in a sine shape (smooth up and down)
// ============================================================================

static float getSweepValue(void) {
    if (!g_soundSynth) return 0.5f;
    float phase = g_soundSynth->songPlayer.sweepPhase;
    // Sine: 0→1→0 over one full cycle
    return 0.5f + 0.5f * sinf(phase * 2.0f * 3.14159265f);
}

// ============================================================================
// Melody trigger callbacks — each track gets a different instrument
// ============================================================================

// Bass track: warm saw with low filter
static void melodyTriggerBass(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    // Release previous bass voice
    if (g_soundSynth->songPlayer.melodyVoice[0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoice[0]);
    }

    noteAttack  = 0.3f;
    noteDecay   = 0.6f;
    noteSustain = 0.4f;
    noteRelease = 1.2f;
    noteVolume  = vel * 0.5f;
    noteFilterCutoff = 0.25f;
    noteFilterResonance = 0.15f;

    float freq = midiToFreqScaled(note);
    int v = playNote(freq, WAVE_TRIANGLE);
    g_soundSynth->songPlayer.melodyVoice[0] = v;
}

// Lead track: glockenspiel (mallet)
static void melodyTriggerLead(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoice[1] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoice[1]);
    }

    noteVolume = vel * 0.4f;
    float freq = midiToFreqScaled(note);
    int v = playMallet(freq, MALLET_PRESET_GLOCKEN);
    g_soundSynth->songPlayer.melodyVoice[1] = v;
}

// Chord track: additive choir pad
static void melodyTriggerChord(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoice[2] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoice[2]);
    }

    noteAttack  = 0.8f;
    noteDecay   = 1.0f;
    noteSustain = 0.5f;
    noteRelease = 2.0f;
    noteVolume  = vel * 0.35f;

    float freq = midiToFreqScaled(note);
    int v = playAdditive(freq, ADDITIVE_PRESET_CHOIR);
    g_soundSynth->songPlayer.melodyVoice[2] = v;
}

// ============================================================================
// Suspense song triggers — organ + vowel
// ============================================================================

// Bass track: dark organ drone
static void melodyTriggerOrganBass(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoice[0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoice[0]);
    }

    noteAttack  = 0.8f;
    noteDecay   = 1.5f;
    noteSustain = 0.6f;
    noteRelease = 3.0f;
    noteVolume  = vel * 0.45f;
    noteFilterCutoff = 0.18f;      // very dark
    noteFilterResonance = 0.20f;

    float freq = midiToFreqScaled(note);
    int v = playAdditive(freq, ADDITIVE_PRESET_ORGAN);
    g_soundSynth->songPlayer.melodyVoice[0] = v;
}

// Lead track: eerie vowel chanting
static void melodyTriggerVowel(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoice[1] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoice[1]);
    }

    noteAttack  = 0.5f;
    noteDecay   = 1.0f;
    noteSustain = 0.4f;
    noteRelease = 2.0f;
    noteVolume  = vel * 0.4f;

    // Cycle between dark vowels: "oo" and "oh"
    // Use note pitch to alternate — low notes get U, higher get O
    VowelType vowel = (note < 67) ? VOWEL_U : VOWEL_O;

    float freq = midiToFreqScaled(note);
    int v = playVowel(freq, vowel);
    if (v >= 0 && slide) {
        // Slide = portamento: eerie gliding between notes
        synthVoices[v].glideRate = 0.3f;
    }
    g_soundSynth->songPlayer.melodyVoice[1] = v;
}

// Chord track: dark organ pad
static void melodyTriggerOrganChord(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoice[2] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoice[2]);
    }

    noteAttack  = 1.0f;
    noteDecay   = 1.5f;
    noteSustain = 0.5f;
    noteRelease = 3.0f;
    noteVolume  = vel * 0.35f;
    noteFilterCutoff = 0.22f;

    float freq = midiToFreqScaled(note);
    int v = playAdditive(freq, ADDITIVE_PRESET_ORGAN);
    g_soundSynth->songPlayer.melodyVoice[2] = v;
}

// ============================================================================
// Jazz song triggers — plucked bass, vibes, FM electric piano
// ============================================================================

// Bass: upright bass pizzicato (Karplus-Strong plucked string)
static void melodyTriggerPluckBass(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoice[0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoice[0]);
    }

    noteVolume = vel * 0.55f;
    float freq = midiToFreqScaled(note);
    // brightness 0.4 = warm, damping 0.5 = natural decay
    int v = playPluck(freq, 0.4f, 0.5f);
    g_soundSynth->songPlayer.melodyVoice[0] = v;
}

// Lead: vibraphone (mallet with motor tremolo)
static void melodyTriggerVibes(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoice[1] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoice[1]);
    }

    noteVolume = vel * (accent ? 0.55f : 0.42f);
    float freq = midiToFreqScaled(note);
    int v = playMallet(freq, MALLET_PRESET_VIBES);
    g_soundSynth->songPlayer.melodyVoice[1] = v;
}

// Chord/response: FM electric piano (Rhodes-like)
static void melodyTriggerFMKeys(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoice[2] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoice[2]);
    }

    noteAttack  = 0.005f;     // snappy attack
    noteDecay   = 0.5f;
    noteSustain = 0.25f;
    noteRelease = 0.8f;
    noteVolume  = vel * (accent ? 0.50f : 0.40f);
    noteFilterCutoff = 0.6f;  // brighter than organ

    // Set FM params: ratio 1:1, moderate index = bell-like Rhodes tone
    fmModRatio = 1.0f;
    fmModIndex = 1.8f;
    fmFeedback = 0.0f;

    float freq = midiToFreqScaled(note);
    int v = playFM(freq);
    g_soundSynth->songPlayer.melodyVoice[2] = v;
}

// ============================================================================
// House triggers — acid bass, filtered pads, stabs
// ============================================================================

// Acid bass: resonant saw with filter sweep from sweepPhase
static void melodyTriggerAcidBass(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoice[0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoice[0]);
    }

    float sweep = getSweepValue();   // 0.0 to 1.0 over ~40 seconds

    noteAttack  = 0.003f;
    noteDecay   = 0.15f;
    noteSustain = 0.6f;
    noteRelease = 0.1f;
    noteVolume  = vel * (accent ? 0.60f : 0.50f);

    // Filter sweeps from 0.08 (dark) to 0.65 (bright) over the sweep cycle
    noteFilterCutoff    = 0.08f + sweep * 0.57f;
    noteFilterResonance = 0.45f + sweep * 0.15f;  // resonance increases with sweep
    noteFilterEnvAmt    = 0.3f;  // each note gets a filter spike too

    float freq = midiToFreqScaled(note);
    int v = playNote(freq, WAVE_SAW);
    if (v >= 0 && slide) {
        synthVoices[v].glideRate = 0.15f;
    }
    g_soundSynth->songPlayer.melodyVoice[0] = v;
}

// Chord stab: FM with sweep-modulated brightness
static void melodyTriggerHouseStab(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoice[1] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoice[1]);
    }

    float sweep = getSweepValue();

    noteAttack  = 0.002f;
    noteDecay   = 0.3f;
    noteSustain = 0.1f;
    noteRelease = 0.4f;
    noteVolume  = vel * 0.40f;
    noteFilterCutoff = 0.20f + sweep * 0.45f;
    noteFilterResonance = 0.10f;

    // FM index follows sweep — brighter at peak
    fmModRatio = 2.0f;
    fmModIndex = 0.5f + sweep * 2.5f;
    fmFeedback = 0.0f;

    float freq = midiToFreqScaled(note);
    int v = playFM(freq);
    g_soundSynth->songPlayer.melodyVoice[1] = v;
}

// Deep pad: additive strings with very slow sweep
static void melodyTriggerDeepPad(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoice[2] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoice[2]);
    }

    float sweep = getSweepValue();

    noteAttack  = 1.5f;        // very slow fade in
    noteDecay   = 2.0f;
    noteSustain = 0.5f;
    noteRelease = 3.0f;        // long tail
    noteVolume  = vel * 0.30f;

    // Pad filter sweeps opposite to bass — opens when bass is closing
    noteFilterCutoff    = 0.12f + (1.0f - sweep) * 0.40f;
    noteFilterResonance = 0.08f;

    float freq = midiToFreqScaled(note);
    int v = playAdditive(freq, ADDITIVE_PRESET_STRINGS);
    g_soundSynth->songPlayer.melodyVoice[2] = v;
}

// Deep house sub bass: pure triangle sub
static void melodyTriggerSubBass(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoice[0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoice[0]);
    }

    noteAttack  = 0.01f;
    noteDecay   = 0.3f;
    noteSustain = 0.7f;
    noteRelease = 0.2f;
    noteVolume  = vel * 0.55f;
    noteFilterCutoff    = 0.15f;   // very dark, sub only
    noteFilterResonance = 0.05f;

    float freq = midiToFreqScaled(note);
    int v = playNote(freq, WAVE_TRIANGLE);
    g_soundSynth->songPlayer.melodyVoice[0] = v;
}

// ============================================================================
// Dilla hip-hop triggers — warm, lo-fi, behind-the-beat
// ============================================================================

// Bass: warm plucked upright, slightly detuned and filtered dark
static void melodyTriggerDillaBass(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoice[0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoice[0]);
    }

    noteVolume = vel * 0.55f;
    noteFilterCutoff = 0.20f;      // dark, warm
    noteFilterResonance = 0.12f;

    float freq = midiToFreqScaled(note);
    // brightness 0.35 = very warm, damping 0.55 = natural thump
    int v = playPluck(freq, 0.35f, 0.55f);
    g_soundSynth->songPlayer.melodyVoice[0] = v;
}

// Keys: lo-fi Rhodes (FM with low index, short decay)
static void melodyTriggerDillaKeys(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoice[1] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoice[1]);
    }

    noteAttack  = 0.003f;
    noteDecay   = 0.35f;
    noteSustain = 0.15f;
    noteRelease = 0.5f;
    noteVolume  = vel * (accent ? 0.45f : 0.35f);
    noteFilterCutoff = 0.35f;      // warm, not bright
    noteFilterResonance = 0.08f;

    // Lo-fi Rhodes: low FM index for mellow bell tone
    fmModRatio = 1.0f;
    fmModIndex = 1.2f;
    fmFeedback = 0.0f;

    float freq = midiToFreqScaled(note);
    int v = playFM(freq);
    g_soundSynth->songPlayer.melodyVoice[1] = v;
}

// Pad: soft choir, very quiet — sampled vinyl warmth
static void melodyTriggerDillaPad(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoice[2] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoice[2]);
    }

    noteAttack  = 0.6f;
    noteDecay   = 1.2f;
    noteSustain = 0.35f;
    noteRelease = 2.0f;
    noteVolume  = vel * 0.25f;
    noteFilterCutoff = 0.22f;      // very dark, like through a speaker
    noteFilterResonance = 0.10f;

    float freq = midiToFreqScaled(note);
    int v = playAdditive(freq, ADDITIVE_PRESET_CHOIR);
    g_soundSynth->songPlayer.melodyVoice[2] = v;
}

// ============================================================================
// Mr Lucky triggers — smooth walking bass, vibes, lush strings
// ============================================================================

// Bass: smooth plucked upright, bright enough to sing, long sustain
static void melodyTriggerMrLuckyBass(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoice[0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoice[0]);
    }

    noteVolume = vel * 0.50f;
    noteFilterCutoff = 0.35f;       // warm but with some definition
    noteFilterResonance = 0.10f;

    float freq = midiToFreqScaled(note);
    // brightness 0.45 = warm-bright, damping 0.35 = sustains nicely
    int v = playPluck(freq, 0.45f, 0.35f);
    g_soundSynth->songPlayer.melodyVoice[0] = v;
}

// Lead: vibraphone — the smooth muzak sparkle
static void melodyTriggerMrLuckyVibes(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoice[1] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoice[1]);
    }

    noteVolume = vel * 0.42f;
    noteFilterCutoff = 0.55f;       // open, sparkling
    float freq = midiToFreqScaled(note);
    int v = playMallet(freq, MALLET_PRESET_VIBES);
    g_soundSynth->songPlayer.melodyVoice[1] = v;
}

// Pad: lush strings — the warmth underneath everything
static void melodyTriggerMrLuckyStrings(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoice[2] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoice[2]);
    }

    noteAttack  = 0.8f;             // slow swell
    noteDecay   = 1.5f;
    noteSustain = 0.5f;
    noteRelease = 2.5f;             // long, cinematic tail
    noteVolume  = vel * 0.30f;
    noteFilterCutoff = 0.40f;
    noteFilterResonance = 0.06f;

    float freq = midiToFreqScaled(note);
    int v = playAdditive(freq, ADDITIVE_PRESET_STRINGS);
    g_soundSynth->songPlayer.melodyVoice[2] = v;
}

// ============================================================================
// Atmosphere triggers — warm pluck, bright glocken, wide strings
// ============================================================================

// Bass: warm plucked string, brighter than Dilla, more sustain
static void melodyTriggerAtmoBass(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoice[0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoice[0]);
    }

    noteVolume = vel * 0.50f;
    noteFilterCutoff = 0.30f;       // warmer than default but not dark
    noteFilterResonance = 0.08f;

    float freq = midiToFreqScaled(note);
    // brightness 0.5 = medium bright, damping 0.3 = long sustain
    int v = playPluck(freq, 0.5f, 0.3f);
    g_soundSynth->songPlayer.melodyVoice[0] = v;
}

// Lead: glockenspiel, slightly louder and brighter than dormitory
static void melodyTriggerAtmoGlocken(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoice[1] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoice[1]);
    }

    noteVolume = vel * 0.45f;
    noteFilterCutoff = 0.65f;       // brighter — sparkly
    float freq = midiToFreqScaled(note);
    int v = playMallet(freq, MALLET_PRESET_GLOCKEN);
    g_soundSynth->songPlayer.melodyVoice[1] = v;
}

// Pad: additive strings, wide and lush
static void melodyTriggerAtmoStrings(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoice[2] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoice[2]);
    }

    noteAttack  = 1.2f;            // very slow swell
    noteDecay   = 2.0f;
    noteSustain = 0.6f;
    noteRelease = 3.5f;            // long, fading tail
    noteVolume  = vel * 0.35f;
    noteFilterCutoff = 0.45f;       // open but warm
    noteFilterResonance = 0.05f;

    float freq = midiToFreqScaled(note);
    int v = playAdditive(freq, ADDITIVE_PRESET_STRINGS);
    g_soundSynth->songPlayer.melodyVoice[2] = v;
}

// ============================================================================
// Release callbacks (shared across songs)
// ============================================================================

static void melodyReleaseBass(void) {
    if (!g_soundSynth) return;
    if (g_soundSynth->songPlayer.melodyVoice[0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoice[0]);
        g_soundSynth->songPlayer.melodyVoice[0] = -1;
    }
}
static void melodyReleaseLead(void) {
    if (!g_soundSynth) return;
    if (g_soundSynth->songPlayer.melodyVoice[1] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoice[1]);
        g_soundSynth->songPlayer.melodyVoice[1] = -1;
    }
}
static void melodyReleaseChord(void) {
    if (!g_soundSynth) return;
    if (g_soundSynth->songPlayer.melodyVoice[2] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoice[2]);
        g_soundSynth->songPlayer.melodyVoice[2] = -1;
    }
}

// ============================================================================
// Create / Destroy / Init
// ============================================================================

SoundSynth* SoundSynthCreate(void) {
    SoundSynth* synth = (SoundSynth*)calloc(1, sizeof(SoundSynth));
    return synth;
}

void SoundSynthDestroy(SoundSynth* synth) {
    if (!synth) return;
    SoundSynthShutdownAudio(synth);
    free(synth);
}

bool SoundSynthInitAudio(SoundSynth* synth, int sampleRate, int bufferFrames) {
    if (!synth) return false;
    if (synth->audioReady) return true;

    if (!IsAudioDeviceReady()) {
        InitAudioDevice();
        synth->ownsAudioDevice = true;
    }

    synth->sampleRate = (sampleRate > 0) ? sampleRate : 44100;
    synth->bufferFrames = (bufferFrames > 0) ? bufferFrames : 512;

    SetAudioStreamBufferSizeDefault(synth->bufferFrames);
    synth->stream = LoadAudioStream(synth->sampleRate, 16, 1);
    SetAudioStreamCallback(synth->stream, SoundSynthCallback);
    PlayAudioStream(synth->stream);

    // Initialize the full sound system
    initSoundSystem(&synth->ss);
    useSoundSystem(&synth->ss);

#if __has_include("soundsystem/engines/scw_data.h")
    loadEmbeddedSCWs();
#endif

    // Set up the sequencer with drum trigger functions
    initSequencer(drumKickFull, drumSnareFull, drumClosedHHFull, drumClapFull);

    // Set up melodic trigger functions (bass=0, lead=1, chord=2)
    setMelodyCallbacks(0, melodyTriggerBass, melodyReleaseBass);
    setMelodyCallbacks(1, melodyTriggerLead, melodyReleaseLead);
    setMelodyCallbacks(2, melodyTriggerChord, melodyReleaseChord);

    masterVolume = 0.5f;

    // Init song player
    synth->songPlayer.active = false;
    for (int i = 0; i < SEQ_MELODY_TRACKS; i++) {
        synth->songPlayer.melodyVoice[i] = -1;
    }

    g_soundSynth = synth;
    synth->audioReady = true;
    return true;
}

void SoundSynthShutdownAudio(SoundSynth* synth) {
    if (!synth || !synth->audioReady) return;

    // Stop song if playing
    if (synth->songPlayer.active) {
        useSoundSystem(&synth->ss);
        stopSequencer();
        synth->songPlayer.active = false;
    }

    StopAudioStream(synth->stream);
    UnloadAudioStream(synth->stream);
    if (synth->ownsAudioDevice) {
        CloseAudioDevice();
        synth->ownsAudioDevice = false;
    }
    synth->audioReady = false;
    if (g_soundSynth == synth) g_soundSynth = NULL;
}

// ============================================================================
// Song playback API
// ============================================================================

// Helper: start a loaded song
static void startSongPlayback(SoundSynth* synth, float bpm) {
    SongPlayer* sp = &synth->songPlayer;
    Pattern* seqPat0 = seqCurrentPattern();
    *seqPat0 = sp->patterns[0];
    seq.bpm = bpm;
    resetSequencer();
    seq.playing = true;
    sp->active = true;
}

void SoundSynthPlaySongDormitory(SoundSynth* synth) {
    if (!synth || !synth->audioReady) return;
    useSoundSystem(&synth->ss);

    // Stop any playing song first
    SoundSynthStopSong(synth);

    // Set dormitory instrument callbacks
    setMelodyCallbacks(0, melodyTriggerBass, melodyReleaseBass);
    setMelodyCallbacks(1, melodyTriggerLead, melodyReleaseLead);
    setMelodyCallbacks(2, melodyTriggerChord, melodyReleaseChord);

    Song_DormitoryAmbient_ConfigureVoices();

    SongPlayer* sp = &synth->songPlayer;
    Song_DormitoryAmbient_Load(sp->patterns);
    sp->patternCount = 2;
    sp->currentPattern = 0;
    sp->loopsOnCurrent = 0;
    sp->loopsPerPattern = 2;  // A A B B A A B B ...
    sp->bpm = SONG_DORMITORY_AMBIENT_BPM;

    startSongPlayback(synth, sp->bpm);
}

void SoundSynthPlaySongSuspense(SoundSynth* synth) {
    if (!synth || !synth->audioReady) return;
    useSoundSystem(&synth->ss);

    // Stop any playing song first
    SoundSynthStopSong(synth);

    // Set suspense instrument callbacks: organ bass, vowel lead, organ chords
    setMelodyCallbacks(0, melodyTriggerOrganBass, melodyReleaseBass);
    setMelodyCallbacks(1, melodyTriggerVowel, melodyReleaseLead);
    setMelodyCallbacks(2, melodyTriggerOrganChord, melodyReleaseChord);

    Song_Suspense_ConfigureVoices();

    SongPlayer* sp = &synth->songPlayer;
    Song_Suspense_Load(sp->patterns);
    sp->patternCount = 2;
    sp->currentPattern = 0;
    sp->loopsOnCurrent = 0;
    sp->loopsPerPattern = 2;
    sp->bpm = SONG_SUSPENSE_BPM;

    startSongPlayback(synth, sp->bpm);
}

void SoundSynthPlaySongJazz(SoundSynth* synth) {
    if (!synth || !synth->audioReady) return;
    useSoundSystem(&synth->ss);

    SoundSynthStopSong(synth);

    // Jazz instruments: plucked bass, vibraphone, FM electric piano
    setMelodyCallbacks(0, melodyTriggerPluckBass, melodyReleaseBass);
    setMelodyCallbacks(1, melodyTriggerVibes, melodyReleaseLead);
    setMelodyCallbacks(2, melodyTriggerFMKeys, melodyReleaseChord);

    Song_JazzCallResponse_ConfigureVoices();

    SongPlayer* sp = &synth->songPlayer;
    Song_JazzCallResponse_Load(sp->patterns);
    sp->patternCount = 3;
    sp->currentPattern = 0;
    sp->loopsOnCurrent = 0;
    sp->loopsPerPattern = 2;  // A A B B C C A A ...
    sp->bpm = SONG_JAZZ_BPM;

    // Set swing timing for jazz feel
    seq.dilla.swing = 7;       // stronger swing than default
    seq.dilla.snareDelay = 3;  // lazy brush
    seq.dilla.jitter = 2;     // human feel

    startSongPlayback(synth, sp->bpm);
}

void SoundSynthPlaySongHouse(SoundSynth* synth) {
    if (!synth || !synth->audioReady) return;
    useSoundSystem(&synth->ss);

    SoundSynthStopSong(synth);

    // House instruments: acid bass, FM stabs, deep pad
    setMelodyCallbacks(0, melodyTriggerAcidBass, melodyReleaseBass);
    setMelodyCallbacks(1, melodyTriggerHouseStab, melodyReleaseLead);
    setMelodyCallbacks(2, melodyTriggerDeepPad, melodyReleaseChord);

    Song_House_ConfigureVoices();

    SongPlayer* sp = &synth->songPlayer;
    Song_House_Load(sp->patterns);
    sp->patternCount = 2;
    sp->currentPattern = 0;
    sp->loopsOnCurrent = 0;
    sp->loopsPerPattern = 4;  // longer loops — let the sweep breathe
    sp->bpm = SONG_HOUSE_BPM;
    sp->sweepPhase = 0.0f;
    sp->sweepRate = 0.025f;   // ~40s full cycle

    startSongPlayback(synth, sp->bpm);
}

void SoundSynthPlaySongDeepHouse(SoundSynth* synth) {
    if (!synth || !synth->audioReady) return;
    useSoundSystem(&synth->ss);

    SoundSynthStopSong(synth);

    // Deep house instruments: sub bass, FM stabs, deep pad
    setMelodyCallbacks(0, melodyTriggerSubBass, melodyReleaseBass);
    setMelodyCallbacks(1, melodyTriggerHouseStab, melodyReleaseLead);
    setMelodyCallbacks(2, melodyTriggerDeepPad, melodyReleaseChord);

    Song_DeepHouse_ConfigureVoices();

    SongPlayer* sp = &synth->songPlayer;
    Song_DeepHouse_Load(sp->patterns);
    sp->patternCount = 2;
    sp->currentPattern = 0;
    sp->loopsOnCurrent = 0;
    sp->loopsPerPattern = 4;
    sp->bpm = SONG_DEEP_HOUSE_BPM;
    sp->sweepPhase = 0.0f;
    sp->sweepRate = 0.017f;   // ~60s full cycle

    startSongPlayback(synth, sp->bpm);
}

void SoundSynthPlaySongDilla(SoundSynth* synth) {
    if (!synth || !synth->audioReady) return;
    useSoundSystem(&synth->ss);

    SoundSynthStopSong(synth);

    // Dilla instruments: plucked bass, FM Rhodes, additive choir pad
    setMelodyCallbacks(0, melodyTriggerDillaBass, melodyReleaseBass);
    setMelodyCallbacks(1, melodyTriggerDillaKeys, melodyReleaseLead);
    setMelodyCallbacks(2, melodyTriggerDillaPad, melodyReleaseChord);

    Song_Dilla_ConfigureVoices();

    SongPlayer* sp = &synth->songPlayer;
    Song_Dilla_Load(sp->patterns);
    sp->patternCount = 3;
    sp->currentPattern = 0;
    sp->loopsOnCurrent = 0;
    sp->loopsPerPattern = 2;
    sp->bpm = SONG_DILLA_BPM;
    sp->sweepPhase = 0.0f;
    sp->sweepRate = 0.0f;     // no sweep for this one

    // The Dilla swing — the whole point
    seq.dilla.swing = 9;       // heavy swing, behind the beat
    seq.dilla.snareDelay = 5;  // lazy snare
    seq.dilla.jitter = 4;     // drunk feel

    startSongPlayback(synth, sp->bpm);
}

void SoundSynthPlaySongMrLucky(SoundSynth* synth) {
    if (!synth || !synth->audioReady) return;
    useSoundSystem(&synth->ss);

    SoundSynthStopSong(synth);

    // Mr Lucky: pluck bass, vibes, strings — no FM, all smooth
    setMelodyCallbacks(0, melodyTriggerMrLuckyBass, melodyReleaseBass);
    setMelodyCallbacks(1, melodyTriggerMrLuckyVibes, melodyReleaseLead);
    setMelodyCallbacks(2, melodyTriggerMrLuckyStrings, melodyReleaseChord);

    Song_MrLucky_ConfigureVoices();

    SongPlayer* sp = &synth->songPlayer;
    Song_MrLucky_Load(sp->patterns);
    sp->patternCount = 3;
    sp->currentPattern = 0;
    sp->loopsOnCurrent = 0;
    sp->loopsPerPattern = 2;  // A A B B C C A A ...
    sp->bpm = SONG_MR_LUCKY_BPM;
    sp->sweepPhase = 0.0f;
    sp->sweepRate = 0.0f;

    startSongPlayback(synth, sp->bpm);
}

void SoundSynthPlaySongAtmosphere(SoundSynth* synth) {
    if (!synth || !synth->audioReady) return;
    useSoundSystem(&synth->ss);

    SoundSynthStopSong(synth);

    // Atmosphere: pluck bass, glockenspiel, strings — no FM
    setMelodyCallbacks(0, melodyTriggerAtmoBass, melodyReleaseBass);
    setMelodyCallbacks(1, melodyTriggerAtmoGlocken, melodyReleaseLead);
    setMelodyCallbacks(2, melodyTriggerAtmoStrings, melodyReleaseChord);

    Song_Atmosphere_ConfigureVoices();

    SongPlayer* sp = &synth->songPlayer;
    Song_Atmosphere_Load(sp->patterns);
    sp->patternCount = 3;
    sp->currentPattern = 0;
    sp->loopsOnCurrent = 0;
    sp->loopsPerPattern = 3;  // longer loops — let it breathe
    sp->bpm = SONG_ATMOSPHERE_BPM;
    sp->sweepPhase = 0.0f;
    sp->sweepRate = 0.0f;     // no sweep

    startSongPlayback(synth, sp->bpm);
}

void SoundSynthStopSong(SoundSynth* synth) {
    if (!synth || !synth->audioReady) return;
    useSoundSystem(&synth->ss);

    stopSequencer();
    synth->songPlayer.active = false;

    // Release any held melody voices
    for (int i = 0; i < SEQ_MELODY_TRACKS; i++) {
        if (synth->songPlayer.melodyVoice[i] >= 0) {
            releaseNote(synth->songPlayer.melodyVoice[i]);
            synth->songPlayer.melodyVoice[i] = -1;
        }
    }
}

bool SoundSynthIsSongPlaying(SoundSynth* synth) {
    if (!synth) return false;
    return synth->songPlayer.active;
}

// ============================================================================
// Update (call from game loop each frame)
// ============================================================================

void SoundSynthUpdate(SoundSynth* synth, float dt) {
    if (!synth) return;
    useSoundSystem(&synth->ss);

    // Update sequencer (song player)
    if (synth->songPlayer.active) {
        SongPlayer* sp = &synth->songPlayer;

        // Advance filter sweep phase
        if (sp->sweepRate > 0.0f) {
            sp->sweepPhase += sp->sweepRate * dt;
            if (sp->sweepPhase > 1.0f) sp->sweepPhase -= 1.0f;
        }

        // Track pattern loops via sequencer playCount
        int prevPlayCount = seq.playCount;
        updateSequencer(dt);

        // Detect when the sequencer loops (playCount incremented)
        if (seq.playCount > prevPlayCount) {
            sp->loopsOnCurrent++;

            // Cycle to next pattern after loopsPerPattern repeats
            if (sp->patternCount > 1 && sp->loopsOnCurrent >= sp->loopsPerPattern) {
                int nextPat = (sp->currentPattern + 1) % sp->patternCount;
                sp->currentPattern = nextPat;
                sp->loopsOnCurrent = 0;
                Pattern* seqPat = seqCurrentPattern();
                *seqPat = sp->patterns[nextPat];
            }
        }
    }

    // Update phrase player (existing functionality)
    SoundPhrasePlayer* p = &synth->player;
    if (!p->active && !p->songActive) return;

    if (p->tokenTimer > 0.0f) p->tokenTimer -= dt;
    if (p->gapTimer > 0.0f) p->gapTimer -= dt;

    if (p->tokenTimer <= 0.0f && p->currentVoice >= 0) {
        releaseNote(p->currentVoice);
        p->currentVoice = -1;
    }

    if (p->tokenTimer <= 0.0f && p->gapTimer <= 0.0f) {
        if (p->tokenIndex >= p->phrase.count) {
            p->active = false;
            if (p->songActive) {
                p->phraseIndex++;
                if (p->phraseIndex < p->song.phraseCount) {
                    SoundSynthPlayPhrase(synth, &p->song.phrases[p->phraseIndex]);
                    p->songActive = true;
                } else {
                    p->songActive = false;
                }
            }
            return;
        }

        const SoundToken* token = &p->phrase.tokens[p->tokenIndex++];
        p->currentVoice = SoundSynthPlayToken(synth, token);
        p->tokenTimer = token->duration;
        p->gapTimer = token->gap;
    }
}

// ============================================================================
// Token/Phrase/Song playback (existing functionality)
// ============================================================================

static void applyTokenEnvelope(const SoundToken* token) {
    float attack = token->duration * 0.15f;
    float decay = token->duration * 0.45f;
    float release = token->duration * 0.25f;

    if (attack < 0.002f) attack = 0.002f;
    if (decay < 0.02f) decay = 0.02f;
    if (release < 0.02f) release = 0.02f;

    noteAttack = attack;
    noteDecay = decay;
    noteSustain = 0.4f;
    noteRelease = release;
    noteVolume = token->intensity;
}

int SoundSynthPlayToken(SoundSynth* synth, const SoundToken* token) {
    if (!synth || !token) return -1;
    if (!synth->audioReady) return -1;

    useSoundSystem(&synth->ss);
    applyTokenEnvelope(token);

    switch (token->kind) {
        case SOUND_TOKEN_BIRD:
        {
            int v = playBird(token->freq, (BirdType)token->variant);
            if (v >= 0) {
                synthVoices[v].sustain = 0.0f;
                synthVoices[v].release = fmaxf(0.03f, token->duration * 0.35f);
            }
            return v;
        }
        case SOUND_TOKEN_VOWEL:
        {
            int v = playVowel(token->freq, (VowelType)token->variant);
            if (v >= 0) {
                synthVoices[v].sustain = 0.0f;
                synthVoices[v].release = fmaxf(0.04f, token->duration * 0.45f);
            }
            return v;
        }
        case SOUND_TOKEN_CONSONANT:
            noteAttack = 0.001f;
            noteDecay = 0.05f;
            noteSustain = 0.0f;
            noteRelease = 0.02f;
            noteVolume = token->intensity;
            return playNote(token->freq, WAVE_NOISE);
        case SOUND_TOKEN_TONE:
        default:
            return playNote(token->freq, WAVE_TRIANGLE);
    }
    return -1;
}

void SoundSynthPlayPhrase(SoundSynth* synth, const SoundPhrase* phrase) {
    if (!synth || !phrase) return;
    synth->player.phrase = *phrase;
    synth->player.tokenIndex = 0;
    synth->player.tokenTimer = 0.0f;
    synth->player.gapTimer = 0.0f;
    synth->player.currentVoice = -1;
    synth->player.active = (phrase->count > 0);
    synth->player.songActive = false;
}

void SoundSynthPlaySongPhrases(SoundSynth* synth, const SoundSong* song) {
    if (!synth || !song) return;
    synth->player.song = *song;
    synth->player.phraseIndex = 0;
    synth->player.songActive = (song->phraseCount > 0);
    synth->player.active = false;
    if (synth->player.songActive) {
        SoundSynthPlayPhrase(synth, &song->phrases[0]);
        synth->player.songActive = true;
        synth->player.phraseIndex = 0;
    }
}
