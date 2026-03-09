#include "sound_synth_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../vendor/raylib.h"
#include "soundsystem/soundsystem.h"
#include "soundsystem/engines/synth_patch.h"
#include "soundsystem/engines/patch_trigger.h"
#include "soundsystem/engines/song_file.h"

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

#define MAX_SONG_PATTERNS 8

typedef struct {
    bool active;
    Pattern patterns[MAX_SONG_PATTERNS];
    int patternCount;
    int currentPattern;
    int loopsOnCurrent;     // how many times current pattern has looped
    int loopsPerPattern;    // how many times to loop before switching (default 2)
    float bpm;
    // Voice indices for melody tracks (so we can release them)
    // Single-voice tracks use melodyVoices[track][0] with melodyVoiceCount[track]=1.
    // Chord tracks (PICK_ALL) use multiple voices.
    #define MAX_CHORD_VOICES 6
    int melodyVoices[SEQ_MELODY_TRACKS][MAX_CHORD_VOICES];
    int melodyVoiceCount[SEQ_MELODY_TRACKS];
    // Filter sweep state — accumulates across bars for long sweeps
    float sweepPhase;       // 0.0 to 1.0, wraps (sine LFO position)
    float sweepRate;        // how fast the sweep moves per second (e.g. 0.02 = 50s cycle)
    // File-based instruments (from .song files)
    SynthPatch instruments[SEQ_MELODY_TRACKS];  // bass, lead, chord
    bool hasPatchInstruments;   // true = use generic patch trigger, false = use per-song callbacks
    // Jukebox
    int jukeboxIndex;       // which song is selected (-1 = none)
} SongPlayer;

// ============================================================================
// Main synth struct (now with full sound system)
// ============================================================================

struct SoundSynth {
    SoundSystem ss;         // single sound system (shared voice pool)
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

static float voiceSnapshotTimer = 0.0f;

static void logVoiceSnapshot(void) {
    if (!seqSoundLogEnabled) return;
    int off = 0, atk = 0, dec = 0, sus = 0, rel = 0;
    float maxRelLevel = 0.0f;
    for (int i = 0; i < NUM_VOICES; i++) {
        switch (synthVoices[i].envStage) {
            case 0: off++; break;
            case 1: atk++; break;
            case 2: dec++; break;
            case 3: sus++; break;
            case 4: rel++;
                if (synthVoices[i].envLevel > maxRelLevel)
                    maxRelLevel = synthVoices[i].envLevel;
                break;
        }
    }
    int active = atk + dec + sus + rel;
    if (active > 0) {
        seqSoundLog("VOICES  active=%d (atk=%d dec=%d sus=%d rel=%d) off=%d  maxRelLvl=%.4f",
            active, atk, dec, sus, rel, off, maxRelLevel);
    }
}

static void SoundSynthCallback(void* buffer, unsigned int frames) {
    if (!g_soundSynth) return;
    short* out = (short*)buffer;
    float sr = (float)g_soundSynth->sampleRate;
    float bufferDt = (float)frames / sr;

    useSoundSystem(&g_soundSynth->ss);

    // Periodic voice state snapshot (~4x per second)
    if (seqSoundLogEnabled) {
        voiceSnapshotTimer += bufferDt;
        if (voiceSnapshotTimer >= 0.25f) {
            voiceSnapshotTimer -= 0.25f;
            logVoiceSnapshot();
        }
    }

    // Update sequencer on the audio thread — melody triggers and voice
    // processing share the same thread, so no context pointer races.
    if (g_soundSynth->songPlayer.active) {
        SongPlayer* sp = &g_soundSynth->songPlayer;

        // Advance filter sweep phase
        if (sp->sweepRate > 0.0f) {
            sp->sweepPhase += sp->sweepRate * bufferDt;
            if (sp->sweepPhase > 1.0f) sp->sweepPhase -= 1.0f;
        }

        // Track pattern loops via sequencer playCount
        int prevPlayCount = seq.playCount;
        updateSequencer(bufferDt);

        // Detect when the sequencer loops (playCount incremented)
        if (seq.playCount > prevPlayCount) {
            sp->loopsOnCurrent++;

            // Cycle to next pattern after loopsPerPattern repeats
            if (sp->patternCount > 1 && sp->loopsOnCurrent >= sp->loopsPerPattern) {
                int nextPat = (sp->currentPattern + 1) % sp->patternCount;
                seqSoundLog("SONG_PATTERN %d -> %d", sp->currentPattern, nextPat);
                sp->currentPattern = nextPat;
                sp->loopsOnCurrent = 0;
                Pattern* seqPat = seqCurrentPattern();
                *seqPat = sp->patterns[nextPat];
            }
        }
    }

    for (unsigned int i = 0; i < frames; i++) {
        float sample = 0.0f;
        for (int v = 0; v < NUM_VOICES; v++) {
            sample += processVoice(&synthVoices[v], sr);
        }
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
    if (g_soundSynth->songPlayer.melodyVoices[0][0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoices[0][0]);
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
    g_soundSynth->songPlayer.melodyVoices[0][0] = v;
}

// Lead track: glockenspiel (mallet)
static void melodyTriggerLead(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoices[1][0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoices[1][0]);
    }

    noteVolume = vel * 0.4f;
    float freq = midiToFreqScaled(note);
    int v = playMallet(freq, MALLET_PRESET_GLOCKEN);
    g_soundSynth->songPlayer.melodyVoices[1][0] = v;
}

// Chord track: additive choir pad
static void melodyTriggerChord(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoices[2][0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoices[2][0]);
    }

    noteAttack  = 0.8f;
    noteDecay   = 1.0f;
    noteSustain = 0.5f;
    noteRelease = 2.0f;
    noteVolume  = vel * 0.35f;

    float freq = midiToFreqScaled(note);
    int v = playAdditive(freq, ADDITIVE_PRESET_CHOIR);
    g_soundSynth->songPlayer.melodyVoices[2][0] = v;
}

// ============================================================================
// Suspense song triggers — organ + vowel
// ============================================================================

// Bass track: dark organ drone
static void melodyTriggerOrganBass(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoices[0][0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoices[0][0]);
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
    g_soundSynth->songPlayer.melodyVoices[0][0] = v;
}

// Lead track: eerie vowel chanting
static void melodyTriggerVowel(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoices[1][0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoices[1][0]);
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
    g_soundSynth->songPlayer.melodyVoices[1][0] = v;
}

// Chord track: dark organ pad
static void melodyTriggerOrganChord(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoices[2][0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoices[2][0]);
    }

    noteAttack  = 1.0f;
    noteDecay   = 1.5f;
    noteSustain = 0.5f;
    noteRelease = 3.0f;
    noteVolume  = vel * 0.35f;
    noteFilterCutoff = 0.22f;

    float freq = midiToFreqScaled(note);
    int v = playAdditive(freq, ADDITIVE_PRESET_ORGAN);
    g_soundSynth->songPlayer.melodyVoices[2][0] = v;
}

// ============================================================================
// Jazz song triggers — plucked bass, vibes, FM electric piano
// ============================================================================

// Bass: upright bass pizzicato (Karplus-Strong plucked string)
static void melodyTriggerPluckBass(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoices[0][0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoices[0][0]);
    }

    noteVolume = vel * 0.55f;
    float freq = midiToFreqScaled(note);
    // brightness 0.4 = warm, damping 0.5 = natural decay
    int v = playPluck(freq, 0.4f, 0.5f);
    g_soundSynth->songPlayer.melodyVoices[0][0] = v;
}

// Lead: vibraphone (mallet with motor tremolo)
static void melodyTriggerVibes(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoices[1][0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoices[1][0]);
    }

    noteVolume = vel * (accent ? 0.55f : 0.42f);
    float freq = midiToFreqScaled(note);
    int v = playMallet(freq, MALLET_PRESET_VIBES);
    g_soundSynth->songPlayer.melodyVoices[1][0] = v;
}

// Chord/response: FM electric piano (Rhodes-like)
// Internal: FM Keys trigger for a specific track
static void melodyTriggerFMKeysOnTrack(int track, int note, float vel, bool accent) {
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoices[track][0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoices[track][0]);
    }

    noteAttack  = 0.005f;     // snappy attack
    noteDecay   = 0.5f;
    noteSustain = 0.25f;
    noteRelease = 0.3f;       // short release, no lingering
    noteVolume  = vel * (accent ? 0.50f : 0.40f);
    noteFilterCutoff = 0.6f;  // brighter than organ

    // Set FM params: ratio 1:1, moderate index = bell-like Rhodes tone
    fmModRatio = 1.0f;
    fmModIndex = 1.8f;
    fmFeedback = 0.0f;

    float freq = midiToFreqScaled(note);
    int v = playFM(freq);
    g_soundSynth->songPlayer.melodyVoices[track][0] = v;
    g_soundSynth->songPlayer.melodyVoiceCount[track] = 1;
}

// FM Keys on track 2 (chord/comping)
static void melodyTriggerFMKeys(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide;
    melodyTriggerFMKeysOnTrack(2, note, vel, accent);
}

// FM Keys on track 1 (lead/melody)
static void melodyTriggerFMKeysLead(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide;
    melodyTriggerFMKeysOnTrack(1, note, vel, accent);
}

// ============================================================================
// House triggers — acid bass, filtered pads, stabs
// ============================================================================

// Acid bass: resonant saw with filter sweep from sweepPhase
static void melodyTriggerAcidBass(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoices[0][0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoices[0][0]);
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
    g_soundSynth->songPlayer.melodyVoices[0][0] = v;
}

// Chord stab: FM with sweep-modulated brightness
static void melodyTriggerHouseStab(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoices[1][0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoices[1][0]);
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
    g_soundSynth->songPlayer.melodyVoices[1][0] = v;
}

// Deep pad: additive strings with very slow sweep
static void melodyTriggerDeepPad(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoices[2][0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoices[2][0]);
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
    g_soundSynth->songPlayer.melodyVoices[2][0] = v;
}

// Deep house sub bass: pure triangle sub
static void melodyTriggerSubBass(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoices[0][0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoices[0][0]);
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
    g_soundSynth->songPlayer.melodyVoices[0][0] = v;
}

// ============================================================================
// Dilla hip-hop triggers — warm, lo-fi, behind-the-beat
// ============================================================================

// Bass: warm plucked upright, slightly detuned and filtered dark
static void melodyTriggerDillaBass(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoices[0][0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoices[0][0]);
    }

    noteVolume = vel * 0.55f;
    noteFilterCutoff = 0.20f;      // dark, warm
    noteFilterResonance = 0.12f;

    float freq = midiToFreqScaled(note);
    // brightness 0.35 = very warm, damping 0.55 = natural thump
    int v = playPluck(freq, 0.35f, 0.55f);
    g_soundSynth->songPlayer.melodyVoices[0][0] = v;
}

// Keys: lo-fi Rhodes (FM with low index, short decay)
static void melodyTriggerDillaKeys(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoices[1][0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoices[1][0]);
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
    g_soundSynth->songPlayer.melodyVoices[1][0] = v;
}

// Pad: soft choir, very quiet — sampled vinyl warmth
static void melodyTriggerDillaPad(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoices[2][0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoices[2][0]);
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
    g_soundSynth->songPlayer.melodyVoices[2][0] = v;
}

// ============================================================================
// Mr Lucky triggers — smooth walking bass, vibes, lush strings
// ============================================================================

// Bass: smooth plucked upright, bright enough to sing, long sustain
static void melodyTriggerMrLuckyBass(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoices[0][0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoices[0][0]);
    }

    noteVolume = vel * 0.50f;
    noteFilterCutoff = 0.35f;       // warm but with some definition
    noteFilterResonance = 0.10f;

    float freq = midiToFreqScaled(note);
    // brightness 0.45 = warm-bright, damping 0.35 = sustains nicely
    int v = playPluck(freq, 0.45f, 0.35f);
    g_soundSynth->songPlayer.melodyVoices[0][0] = v;
}

// Lead: vibraphone — the smooth muzak sparkle
static void melodyTriggerMrLuckyVibes(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoices[1][0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoices[1][0]);
    }

    noteVolume = vel * 0.42f;
    noteFilterCutoff = 0.55f;       // open, sparkling
    float freq = midiToFreqScaled(note);
    int v = playMallet(freq, MALLET_PRESET_VIBES);
    g_soundSynth->songPlayer.melodyVoices[1][0] = v;
}

// Pad: lush strings — the warmth underneath everything
static void melodyTriggerMrLuckyStrings(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoices[2][0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoices[2][0]);
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
    g_soundSynth->songPlayer.melodyVoices[2][0] = v;
}

// ============================================================================
// Atmosphere triggers — warm pluck, bright glocken, wide strings
// ============================================================================

// Bass: warm plucked string, brighter than Dilla, more sustain
static void melodyTriggerAtmoBass(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoices[0][0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoices[0][0]);
    }

    noteVolume = vel * 0.50f;
    noteFilterCutoff = 0.30f;       // warmer than default but not dark
    noteFilterResonance = 0.08f;

    float freq = midiToFreqScaled(note);
    // brightness 0.5 = medium bright, damping 0.3 = long sustain
    int v = playPluck(freq, 0.5f, 0.3f);
    g_soundSynth->songPlayer.melodyVoices[0][0] = v;
}

// Lead: glockenspiel, slightly louder and brighter than dormitory
static void melodyTriggerAtmoGlocken(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoices[1][0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoices[1][0]);
    }

    noteVolume = vel * 0.45f;
    noteFilterCutoff = 0.65f;       // brighter — sparkly
    float freq = midiToFreqScaled(note);
    int v = playMallet(freq, MALLET_PRESET_GLOCKEN);
    g_soundSynth->songPlayer.melodyVoices[1][0] = v;
}

// Pad: additive strings, wide and lush
static void melodyTriggerAtmoStrings(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    if (g_soundSynth->songPlayer.melodyVoices[2][0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoices[2][0]);
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
    g_soundSynth->songPlayer.melodyVoices[2][0] = v;
}

// ============================================================================
// Release callbacks (shared across songs)
// ============================================================================

static void melodyReleaseBass(void) {
    if (!g_soundSynth) return;
    if (g_soundSynth->songPlayer.melodyVoices[0][0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoices[0][0]);
        g_soundSynth->songPlayer.melodyVoices[0][0] = -1;
    }
}
static void melodyReleaseLead(void) {
    if (!g_soundSynth) return;
    if (g_soundSynth->songPlayer.melodyVoices[1][0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoices[1][0]);
        g_soundSynth->songPlayer.melodyVoices[1][0] = -1;
    }
}
static void melodyReleaseChord(void) {
    if (!g_soundSynth) return;
    if (g_soundSynth->songPlayer.melodyVoices[2][0] >= 0) {
        releaseNote(g_soundSynth->songPlayer.melodyVoices[2][0]);
        g_soundSynth->songPlayer.melodyVoices[2][0] = -1;
    }
}

// Release all chord voices for a given track
static void melodyReleaseAllVoices(int track) {
    if (!g_soundSynth) return;
    for (int j = 0; j < g_soundSynth->songPlayer.melodyVoiceCount[track]; j++) {
        int v = g_soundSynth->songPlayer.melodyVoices[track][j];
        if (v >= 0) releaseNote(v);
        g_soundSynth->songPlayer.melodyVoices[track][j] = -1;
    }
    g_soundSynth->songPlayer.melodyVoiceCount[track] = 0;
}

// Chord release wrappers per track
static void melodyReleaseChordPoly(void) { melodyReleaseAllVoices(2); }
static void melodyReleaseLeadPoly(void)  { melodyReleaseAllVoices(1); }

// FM Keys chord trigger — plays all notes with finger stagger (offset attacks)
static void melodyTriggerChordFMKeys(int* notes, int noteCount, float vel,
                                      float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    // Release previous chord voices
    melodyReleaseAllVoices(2);

    if (noteCount > MAX_CHORD_VOICES) noteCount = MAX_CHORD_VOICES;

    for (int i = 0; i < noteCount; i++) {
        // Finger stagger: each successive note gets a slightly delayed attack
        // 5-15ms offset per finger gives natural "rolled chord" feel
        noteAttack  = 0.005f + i * 0.012f;
        noteDecay   = 0.5f;
        noteSustain = 0.25f;
        noteRelease = 0.1f;       // very short tail, no chord wash
        // Slightly lower volume per note to prevent clipping
        noteVolume  = vel * (accent ? 0.35f : 0.28f);
        noteFilterCutoff = 0.6f;

        fmModRatio = 1.0f;
        fmModIndex = 1.8f;
        fmFeedback = 0.0f;

        float freq = midiToFreqScaled(notes[i]);
        int v = playFM(freq);
        g_soundSynth->songPlayer.melodyVoices[2][i] = v;
    }
    g_soundSynth->songPlayer.melodyVoiceCount[2] = noteCount;
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

    // Initialize song system (sequencer + melody + drums)
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
    synth->songPlayer.jukeboxIndex = 0;
    for (int i = 0; i < SEQ_MELODY_TRACKS; i++) {
        for (int j = 0; j < MAX_CHORD_VOICES; j++) {
            synth->songPlayer.melodyVoices[i][j] = -1;
        }
        synth->songPlayer.melodyVoiceCount[i] = 0;
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
    seqSoundLogReset();
    seqSoundLog("SONG_START \"%s\"  bpm=%.0f patterns=%d tps=%d",
        (sp->jukeboxIndex >= 0) ? SoundSynthGetSongName(sp->jukeboxIndex) : "?",
        bpm, sp->patternCount, seq.ticksPerStep);
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

void SoundSynthPlaySongHappyBirthday(SoundSynth* synth) {
    if (!synth || !synth->audioReady) return;
    useSoundSystem(&synth->ss);

    SoundSynthStopSong(synth);

    // Jazz waltz: pluck bass, FM keys melody, FM keys chords (polyphonic)
    setMelodyCallbacks(0, melodyTriggerPluckBass, melodyReleaseBass);
    setMelodyCallbacks(1, melodyTriggerFMKeysLead, melodyReleaseLead);
    setMelodyCallbacks(2, melodyTriggerFMKeys, melodyReleaseChordPoly);
    setMelodyChordCallback(2, melodyTriggerChordFMKeys);

    Song_HappyBirthday_ConfigureVoices();

    SongPlayer* sp = &synth->songPlayer;
    Song_HappyBirthday_Load(sp->patterns);
    sp->patternCount = 4;
    sp->currentPattern = 0;
    sp->loopsOnCurrent = 0;
    sp->loopsPerPattern = 1;  // A B C D — one pass each, then loop the whole chorus
    sp->bpm = SONG_HAPPY_BIRTHDAY_BPM;
    sp->sweepPhase = 0.0f;
    sp->sweepRate = 0.0f;

    // Light swing for jazz waltz feel
    seq.dilla.swing = 4;
    seq.dilla.snareDelay = 0;
    seq.dilla.jitter = 1;

    // Subtle melody humanize for waltz feel
    seq.humanize.timingJitter = 1;
    seq.humanize.velocityJitter = 0.06f;

    startSongPlayback(synth, sp->bpm);
}

void SoundSynthPlaySongMonksMood(SoundSynth* synth) {
    if (!synth || !synth->audioReady) return;
    useSoundSystem(&synth->ss);

    SoundSynthStopSong(synth);

    // Jazz ballad: pluck bass, FM keys melody, FM keys chords (polyphonic)
    setMelodyCallbacks(0, melodyTriggerPluckBass, melodyReleaseBass);
    setMelodyCallbacks(1, melodyTriggerFMKeysLead, melodyReleaseLeadPoly);
    setMelodyCallbacks(2, melodyTriggerFMKeys, melodyReleaseChordPoly);
    setMelodyChordCallback(2, melodyTriggerChordFMKeys);

    Song_MonksMood_ConfigureVoices();

    SongPlayer* sp = &synth->songPlayer;
    Song_MonksMood_Load(sp->patterns);
    sp->patternCount = 8;
    sp->currentPattern = 0;
    sp->loopsOnCurrent = 0;
    sp->loopsPerPattern = 1;  // Play all 8 bars, then loop
    sp->bpm = SONG_MONKS_MOOD_BPM;
    sp->sweepPhase = 0.0f;
    sp->sweepRate = 0.0f;

    // Gentle swing for jazz feel (drums only)
    seq.dilla.swing = 3;
    seq.dilla.snareDelay = 0;
    seq.dilla.jitter = 0;

    // Melody humanize — Monk's rubato is already in the nudge values,
    // add just a touch of jitter for life
    seq.humanize.timingJitter = 2;
    seq.humanize.velocityJitter = 0.08f;

    startSongPlayback(synth, sp->bpm);
}

void SoundSynthPlaySongSummertime(SoundSynth* synth) {
    if (!synth || !synth->audioReady) return;
    useSoundSystem(&synth->ss);

    SoundSynthStopSong(synth);

    // Jazz standard: pluck bass, FM keys melody, FM keys chords (polyphonic)
    setMelodyCallbacks(0, melodyTriggerPluckBass, melodyReleaseBass);
    setMelodyCallbacks(1, melodyTriggerFMKeysLead, melodyReleaseLeadPoly);
    setMelodyCallbacks(2, melodyTriggerFMKeys, melodyReleaseChordPoly);
    setMelodyChordCallback(2, melodyTriggerChordFMKeys);

    Song_Summertime_ConfigureVoices();

    SongPlayer* sp = &synth->songPlayer;
    Song_Summertime_Load(sp->patterns);
    sp->patternCount = 8;
    sp->currentPattern = 0;
    sp->loopsOnCurrent = 0;
    sp->loopsPerPattern = 1;
    sp->bpm = SONG_SUMMERTIME_BPM;
    sp->sweepPhase = 0.0f;
    sp->sweepRate = 0.0f;

    // Lazy swing for summer feel
    seq.dilla.swing = 3;
    seq.dilla.snareDelay = 0;
    seq.dilla.jitter = 0;

    // Subtle humanize
    seq.humanize.timingJitter = 2;
    seq.humanize.velocityJitter = 0.06f;

    startSongPlayback(synth, sp->bpm);
}

void SoundSynthPlaySongMule(SoundSynth* synth) {
    if (!synth || !synth->audioReady) return;
    useSoundSystem(&synth->ss);

    SoundSynthStopSong(synth);

    // 8-bit: square bass, sawtooth melody — both monophonic
    setMelodyCallbacks(0, melodyTriggerPluckBass, melodyReleaseBass);
    setMelodyCallbacks(1, melodyTriggerFMKeysLead, melodyReleaseLeadPoly);

    Song_Mule_ConfigureVoices();

    SongPlayer* sp = &synth->songPlayer;
    Song_Mule_Load(sp->patterns);
    sp->patternCount = 8;
    sp->currentPattern = 0;
    sp->loopsOnCurrent = 0;
    sp->loopsPerPattern = 1;
    sp->bpm = SONG_MULE_BPM;
    sp->sweepPhase = 0.0f;
    sp->sweepRate = 0.0f;

    // 32nd note resolution for the grace note stutters
    seqSet32ndNoteMode(true);

    // No swing, no humanize — tight 8-bit machine
    seq.dilla.swing = 0;
    seq.dilla.snareDelay = 0;
    seq.dilla.jitter = 0;
    seq.humanize.timingJitter = 0;
    seq.humanize.velocityJitter = 0.0f;

    startSongPlayback(synth, sp->bpm);
}

void SoundSynthPlaySongGymnopedie(SoundSynth* synth) {
    if (!synth || !synth->audioReady) return;
    useSoundSystem(&synth->ss);

    SoundSynthStopSong(synth);

    // Dreamy piano waltz: pluck bass, FM keys melody, FM keys chords (polyphonic)
    setMelodyCallbacks(0, melodyTriggerPluckBass, melodyReleaseBass);
    setMelodyCallbacks(1, melodyTriggerFMKeysLead, melodyReleaseLead);
    setMelodyCallbacks(2, melodyTriggerFMKeys, melodyReleaseChordPoly);
    setMelodyChordCallback(2, melodyTriggerChordFMKeys);

    Song_Gymnopedie_ConfigureVoices();

    SongPlayer* sp = &synth->songPlayer;
    Song_Gymnopedie_Load(sp->patterns);
    sp->patternCount = 8;
    sp->currentPattern = 0;
    sp->loopsOnCurrent = 0;
    sp->loopsPerPattern = 1;
    sp->bpm = SONG_GYMNOPEDIE_BPM;
    sp->sweepPhase = 0.0f;
    sp->sweepRate = 0.0f;

    // No swing — Satie is straight, floating
    seq.dilla.swing = 0;
    seq.dilla.snareDelay = 0;
    seq.dilla.jitter = 0;

    // Very subtle humanize for piano feel
    seq.humanize.timingJitter = 1;
    seq.humanize.velocityJitter = 0.04f;

    startSongPlayback(synth, sp->bpm);
}

void SoundSynthStopSong(SoundSynth* synth) {
    if (!synth || !synth->audioReady) return;
    useSoundSystem(&synth->ss);

    seqSoundLog("SONG_STOP");
    stopSequencer();
    seqSet32ndNoteMode(false);  // Reset to default 16th note resolution
    synth->songPlayer.active = false;

    // Release ALL active voices — they'll fade out naturally via their envelopes
    for (int i = 0; i < NUM_VOICES; i++) {
        if (synthVoices[i].envStage > 0 && synthVoices[i].envStage < 4) {
            synthVoices[i].envStage = 4;  // enter release phase
        }
    }
    for (int i = 0; i < SEQ_MELODY_TRACKS; i++) {
        for (int j = 0; j < MAX_CHORD_VOICES; j++) {
            synth->songPlayer.melodyVoices[i][j] = -1;
        }
        synth->songPlayer.melodyVoiceCount[i] = 0;
    }
}

bool SoundSynthIsSongPlaying(SoundSynth* synth) {
    if (!synth) return false;
    return synth->songPlayer.active;
}

// ============================================================================
// Generic patch-based triggers (for .song file playback)
// ============================================================================

// Generic melody trigger: reads instrument from SongPlayer.instruments[]
static void melodyTriggerPatchGeneric(int track, int note, float vel,
                                       float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    SongPlayer *sp = &g_soundSynth->songPlayer;
    SynthPatch *p = &sp->instruments[track];

    // Release previous voice
    if (sp->melodyVoices[track][0] >= 0) {
        releaseNote(sp->melodyVoices[track][0]);
    }

    float freq = patchMidiToFreq(note);
    if (accent) {
        // Boost velocity and filter env for accent
        noteVolume = vel * 1.3f;
        noteFilterEnvAmt = p->p_filterEnvAmt + 0.2f;
    }
    sp->melodyVoices[track][0] = playNoteWithPatch(freq, p);
    sp->melodyVoiceCount[track] = 1;
}

// Generic chord trigger: plays all notes through patch
static void melodyChordTriggerPatchGeneric(int track, int* notes, int noteCount,
                                            float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);

    SongPlayer *sp = &g_soundSynth->songPlayer;
    SynthPatch *p = &sp->instruments[track];

    melodyReleaseAllVoices(track);

    if (noteCount > MAX_CHORD_VOICES) noteCount = MAX_CHORD_VOICES;
    for (int i = 0; i < noteCount; i++) {
        float freq = patchMidiToFreq(notes[i]);
        if (accent) noteVolume = vel * 1.3f;
        sp->melodyVoices[track][i] = playNoteWithPatch(freq, p);
    }
    sp->melodyVoiceCount[track] = noteCount;
}

// Per-track wrappers (setMelodyCallbacks needs specific function signatures)
static void melodyTriggerPatchBass(int note, float vel, float gateTime, bool slide, bool accent) {
    melodyTriggerPatchGeneric(0, note, vel, gateTime, slide, accent);
}
static void melodyTriggerPatchLead(int note, float vel, float gateTime, bool slide, bool accent) {
    melodyTriggerPatchGeneric(1, note, vel, gateTime, slide, accent);
}
static void melodyTriggerPatchChord(int note, float vel, float gateTime, bool slide, bool accent) {
    melodyTriggerPatchGeneric(2, note, vel, gateTime, slide, accent);
}
static void melodyReleasePatchBass(void)  { melodyReleaseAllVoices(0); }
static void melodyReleasePatchLead(void)  { melodyReleaseAllVoices(1); }
static void melodyReleasePatchChord(void) { melodyReleaseAllVoices(2); }
static void melodyChordTriggerPatchChord(int* notes, int noteCount, float vel,
                                          float gateTime, bool slide, bool accent) {
    melodyChordTriggerPatchGeneric(2, notes, noteCount, vel, gateTime, slide, accent);
}

// ============================================================================
// Play a .song file
// ============================================================================

bool SoundSynthPlaySongFile(SoundSynth* synth, const char* filepath) {
    if (!synth || !synth->audioReady) return false;
    useSoundSystem(&synth->ss);

    SoundSynthStopSong(synth);

    // Load song file
    SongFileData sf;
    songFileDataInit(&sf);
    if (!songFileLoad(filepath, &sf)) {
        TraceLog(LOG_WARNING, "Failed to load song file: %s", filepath);
        return false;
    }

    SongPlayer *sp = &synth->songPlayer;

    // Copy instruments
    for (int i = 0; i < SEQ_MELODY_TRACKS && i < 3; i++) {
        sp->instruments[i] = sf.instruments[i];
    }
    sp->hasPatchInstruments = true;

    // Set up generic triggers
    setMelodyCallbacks(0, melodyTriggerPatchBass, melodyReleasePatchBass);
    setMelodyCallbacks(1, melodyTriggerPatchLead, melodyReleasePatchLead);
    setMelodyCallbacks(2, melodyTriggerPatchChord, melodyReleasePatchChord);
    setMelodyChordCallback(2, melodyChordTriggerPatchChord);

    // Load patterns
    sp->patternCount = 0;
    for (int i = 0; i < SEQ_NUM_PATTERNS && i < MAX_SONG_PATTERNS; i++) {
        sp->patterns[i] = sf.patterns[i];
        // Check if pattern has any content
        bool hasContent = false;
        for (int t = 0; t < SEQ_DRUM_TRACKS && !hasContent; t++)
            for (int s = 0; s < SEQ_MAX_STEPS && !hasContent; s++)
                if (sf.patterns[i].drumSteps[t][s]) hasContent = true;
        for (int t = 0; t < SEQ_MELODY_TRACKS && !hasContent; t++)
            for (int s = 0; s < SEQ_MAX_STEPS && !hasContent; s++)
                if (sf.patterns[i].melodyNote[t][s] != SEQ_NOTE_OFF) hasContent = true;
        if (hasContent) sp->patternCount = i + 1;
    }
    if (sp->patternCount == 0) sp->patternCount = 1;
    sp->currentPattern = 0;
    sp->loopsOnCurrent = 0;
    sp->loopsPerPattern = sf.loopsPerPattern > 0 ? sf.loopsPerPattern : 2;

    // Apply mix
    masterVolume = sf.sfMasterVolume;
    drumVolume = sf.sfDrumVolume;
    for (int i = 0; i < SEQ_TOTAL_TRACKS; i++) seq.trackVolume[i] = sf.sfTrackVolume[i];

    // Apply drum params
    drumParams = sf.sfDrumParams;

    // Apply effects
    fx = sf.sfEffects;

    // Apply groove
    seq.dilla = sf.dilla;
    seq.ticksPerStep = sf.ticksPerStep;

    // Apply scale
    scaleLockEnabled = sf.songScaleLockEnabled;
    scaleRoot = sf.songScaleRoot;
    scaleType = sf.songScaleType;

    // Start playback
    startSongPlayback(synth, sf.bpm);

    TraceLog(LOG_INFO, "Playing song file: %s (%.0f BPM, %d patterns)", filepath, sf.bpm, sp->patternCount);
    return true;
}

// ============================================================================
// Jukebox — song table and browse/play API
// ============================================================================

typedef void (*SongPlayFunc)(SoundSynth*);

typedef struct {
    const char* name;
    SongPlayFunc play;
} JukeboxEntry;

// Wrapper to play scratch.song via jukebox (matches SongPlayFunc signature)
static void SoundSynthPlayScratchSong(SoundSynth* synth) {
    SoundSynthPlaySongFile(synth, "soundsystem/demo/songs/scratch.song");
}

static const JukeboxEntry jukeboxSongs[] = {
    { "Scratch (.song)",      SoundSynthPlayScratchSong },
    { "Dormitory Ambient",    SoundSynthPlaySongDormitory },
    { "Suspense",             SoundSynthPlaySongSuspense },
    { "Jazz Call & Response",  SoundSynthPlaySongJazz },
    { "House",                SoundSynthPlaySongHouse },
    { "Deep House",           SoundSynthPlaySongDeepHouse },
    { "Dilla Hip-Hop",        SoundSynthPlaySongDilla },
    { "Atmosphere",           SoundSynthPlaySongAtmosphere },
    { "Mr Lucky",             SoundSynthPlaySongMrLucky },
    { "Happy Birthday",       SoundSynthPlaySongHappyBirthday },
    { "Monk's Mood",          SoundSynthPlaySongMonksMood },
    { "Summertime",            SoundSynthPlaySongSummertime },
    { "M.U.L.E. Theme",        SoundSynthPlaySongMule },
    { "Gymnopedie No.1",        SoundSynthPlaySongGymnopedie },
};

#define JUKEBOX_SONG_COUNT (int)(sizeof(jukeboxSongs) / sizeof(jukeboxSongs[0]))

int SoundSynthGetSongCount(void) {
    return JUKEBOX_SONG_COUNT;
}

const char* SoundSynthGetSongName(int index) {
    if (index < 0 || index >= JUKEBOX_SONG_COUNT) return "???";
    return jukeboxSongs[index].name;
}

int SoundSynthGetCurrentSong(SoundSynth* synth) {
    if (!synth) return -1;
    return synth->songPlayer.jukeboxIndex;
}

void SoundSynthJukeboxPlay(SoundSynth* synth, int songIndex) {
    if (!synth || songIndex < 0 || songIndex >= JUKEBOX_SONG_COUNT) return;
    synth->songPlayer.jukeboxIndex = songIndex;
    jukeboxSongs[songIndex].play(synth);
}

void SoundSynthJukeboxNext(SoundSynth* synth) {
    if (!synth) return;
    int idx = synth->songPlayer.jukeboxIndex;
    idx = (idx + 1) % JUKEBOX_SONG_COUNT;
    SoundSynthJukeboxPlay(synth, idx);
}

void SoundSynthJukeboxPrev(SoundSynth* synth) {
    if (!synth) return;
    int idx = synth->songPlayer.jukeboxIndex;
    idx = (idx - 1 + JUKEBOX_SONG_COUNT) % JUKEBOX_SONG_COUNT;
    SoundSynthJukeboxPlay(synth, idx);
}

void SoundSynthJukeboxToggle(SoundSynth* synth) {
    if (!synth) return;
    if (synth->songPlayer.active) {
        SoundSynthStopSong(synth);
    } else {
        int idx = synth->songPlayer.jukeboxIndex;
        if (idx < 0) idx = 0;
        SoundSynthJukeboxPlay(synth, idx);
    }
}

// ============================================================================
// Update (call from game loop each frame)
// ============================================================================

void SoundSynthUpdate(SoundSynth* synth, float dt) {
    if (!synth) return;

    // Sequencer update happens on the audio thread (SoundSynthCallback)
    // to avoid threading races between melody triggers and voice processing.

    // Update phrase player (shares voice pool with song system)
    useSoundSystem(&synth->ss);
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

// ============================================================================
// Sound log — sequencer event debugging
// ============================================================================

void SoundSynthLogToggle(void) {
    seqSoundLogEnabled = !seqSoundLogEnabled;
    if (seqSoundLogEnabled) {
        seqSoundLogReset();
    }
}

bool SoundSynthLogIsEnabled(void) {
    return seqSoundLogEnabled;
}

void SoundSynthLogDump(void) {
    seqSoundLogDump("navkit_sound.log");
}
