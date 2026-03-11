#include "sound_synth_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../vendor/raylib.h"
#include "soundsystem/soundsystem.h"
#include "soundsystem/engines/instrument_presets.h"
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
    // Chord tracks use multiple voices.
    #define MAX_CHORD_VOICES 6
    int melodyVoices[SEQ_MELODY_TRACKS][MAX_CHORD_VOICES];
    int melodyVoiceCount[SEQ_MELODY_TRACKS];
    // Filter sweep state — accumulates across bars for long sweeps
    float sweepPhase;       // 0.0 to 1.0, wraps (sine LFO position)
    float sweepRate;        // how fast the sweep moves per second (e.g. 0.02 = 50s cycle)
    // File-based instruments (from .song files)
    SynthPatch instruments[SEQ_MELODY_TRACKS];  // bass, lead, chord
    bool hasPatchInstruments;   // true = use generic patch trigger, false = use per-song callbacks
    // Chain (pattern play order from .song file)
    int chain[SEQ_MAX_CHAIN];
    int chainLength;            // 0 = linear cycle (no chain)
    int chainPos;
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
                int nextPat;
                if (sp->chainLength > 0) {
                    // Chain mode: follow chain order
                    sp->chainPos = (sp->chainPos + 1) % sp->chainLength;
                    nextPat = sp->chain[sp->chainPos];
                    if (nextPat < 0 || nextPat >= sp->patternCount) nextPat = 0;
                } else {
                    // Linear cycle: 0 → 1 → 2 → ... → 0
                    nextPat = (sp->currentPattern + 1) % sp->patternCount;
                }
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
// SynthPatch-based drum triggers (replaces legacy drums.h)
// ============================================================================

static SynthPatch bridgeDrumPatches[SEQ_DRUM_TRACKS];
static int bridgeDrumVoice[SEQ_DRUM_TRACKS] = {-1, -1, -1, -1};
static bool bridgeDrumsInitialized = false;

static void initBridgeDrumPatches(void) {
    if (bridgeDrumsInitialized) return;
    initInstrumentPresets();
    bridgeDrumPatches[0] = instrumentPresets[24].patch; // 808 Kick
    bridgeDrumPatches[1] = instrumentPresets[25].patch; // 808 Snare
    bridgeDrumPatches[2] = instrumentPresets[27].patch; // 808 CH
    bridgeDrumPatches[3] = instrumentPresets[26].patch; // 808 Clap
    bridgeDrumsInitialized = true;
}

// Saved note parameter block — applyPatchToGlobals writes these fields on SynthContext.
static void bridgeDrumTriggerGeneric(int trackIdx, float vel, float pitch) {
    (void)pitch;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);
    initBridgeDrumPatches();

    SynthPatch *p = &bridgeDrumPatches[trackIdx];

    // Choke previous voice on this track if patch requests it
    if (p->p_choke && bridgeDrumVoice[trackIdx] >= 0 &&
        synthVoices[bridgeDrumVoice[trackIdx]].envStage > 0) {
        releaseNote(bridgeDrumVoice[trackIdx]);
    }

    int v = playNoteWithPatch(p->p_triggerFreq, p);
    bridgeDrumVoice[trackIdx] = v;
    if (v >= 0) {
        synthVoices[v].volume *= vel;
    }
}

static void bridgeDrumKick(float vel, float pitch)  { bridgeDrumTriggerGeneric(0, vel, pitch); }
static void bridgeDrumSnare(float vel, float pitch) { bridgeDrumTriggerGeneric(1, vel, pitch); }
static void bridgeDrumHH(float vel, float pitch)    { bridgeDrumTriggerGeneric(2, vel, pitch); }
static void bridgeDrumClap(float vel, float pitch)  { bridgeDrumTriggerGeneric(3, vel, pitch); }

// ============================================================================
// Unified melody trigger system — all instruments via SynthPatch presets
// ============================================================================

// Song instrument patches — copied from presets at init, per-song overrides applied
static SynthPatch songPatches[SEQ_MELODY_TRACKS];
static bool songPatchesInitialized = false;

static void ensureSongPatches(void) {
    if (songPatchesInitialized) return;
    initInstrumentPresets();
    songPatchesInitialized = true;
}

// Generic single-voice melody trigger via SynthPatch
static void bridgeMelodyTrigger(int track, int note, float vel,
                                 bool slide, bool accent, SynthPatch *p) {
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);
    SongPlayer *sp = &g_soundSynth->songPlayer;

    if (sp->melodyVoices[track][0] >= 0) {
        releaseNote(sp->melodyVoices[track][0]);
    }

    // Accent: boost volume and filter envelope
    float origVol = p->p_volume;
    float origFE = p->p_filterEnvAmt;
    if (accent) {
        p->p_volume = fminf(p->p_volume * 1.3f, 1.0f);
        p->p_filterEnvAmt += 0.2f;
    }

    float freq = midiToFreqScaled(note);
    int v = playNoteWithPatch(freq, p);

    // Restore patch after accent tweak
    p->p_volume = origVol;
    p->p_filterEnvAmt = origFE;

    // Apply velocity scaling on the voice directly
    if (v >= 0) {
        synthVoices[v].volume *= vel;
        if (slide) {
            synthVoices[v].glideRate = 1.0f / p->p_glideTime;
        }
    }
    sp->melodyVoices[track][0] = v;
    sp->melodyVoiceCount[track] = 1;
}



// ============================================================================
// Per-song melody triggers — thin wrappers using presets + overrides
// ============================================================================

// --- Preset indices for readability ---
#define PRESET_GLOCK      15
#define PRESET_MAC_KEYS   21
#define PRESET_MAC_VIBES  23
#define PRESET_ACID        8
#define PRESET_WARM_TRI   38
#define PRESET_DARK_ORGAN 39
#define PRESET_EERIE_VOW  40
#define PRESET_TRI_SUB    41
#define PRESET_DARK_CHOIR 42
#define PRESET_LUSH_STR   43
#define PRESET_WARM_PLUCK 44

// Helper: load preset into track slot with optional overrides
static SynthPatch* songPatch(int track, int presetIdx) {
    ensureSongPatches();
    songPatches[track] = instrumentPresets[presetIdx].patch;
    return &songPatches[track];
}

// --- Default (Dormitory) song ---
static void melodyTriggerBass(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime;
    bridgeMelodyTrigger(0, note, vel, slide, accent, songPatch(0, PRESET_WARM_TRI));
}
static void melodyTriggerLead(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime;
    SynthPatch *p = songPatch(1, PRESET_GLOCK);
    p->p_volume = 0.4f;
    bridgeMelodyTrigger(1, note, vel, slide, accent, p);
}
static void melodyTriggerChord(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime;
    SynthPatch *p = songPatch(2, PRESET_DARK_CHOIR);
    p->p_attack = 0.8f; p->p_decay = 1.0f; p->p_sustain = 0.5f; p->p_release = 2.0f;
    p->p_volume = 0.35f;
    bridgeMelodyTrigger(2, note, vel, slide, accent, p);
}

// --- Suspense song ---
static void melodyTriggerOrganBass(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime;
    bridgeMelodyTrigger(0, note, vel, slide, accent, songPatch(0, PRESET_DARK_ORGAN));
}
static void melodyTriggerVowel(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime;
    SynthPatch *p = songPatch(1, PRESET_EERIE_VOW);
    // Alternate vowel by pitch — low notes get U, high get O
    p->p_voiceVowel = (note < 67) ? VOWEL_U : VOWEL_O;
    bridgeMelodyTrigger(1, note, vel, slide, accent, p);
}
static void melodyTriggerOrganChord(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime;
    SynthPatch *p = songPatch(2, PRESET_DARK_ORGAN);
    p->p_attack = 1.0f; p->p_volume = 0.35f; p->p_filterCutoff = 0.22f;
    bridgeMelodyTrigger(2, note, vel, slide, accent, p);
}

// --- Jazz song ---
static void melodyTriggerPluckBass(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime;
    bridgeMelodyTrigger(0, note, vel, slide, accent, songPatch(0, PRESET_WARM_PLUCK));
}
static void melodyTriggerVibes(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime;
    SynthPatch *p = songPatch(1, PRESET_MAC_VIBES);
    p->p_volume = accent ? 0.55f : 0.42f;
    bridgeMelodyTrigger(1, note, vel, slide, false, p); // accent already in volume
}
static void melodyTriggerFMKeysOnTrack(int track, int note, float vel, bool accent) {
    SynthPatch *p = songPatch(track, PRESET_MAC_KEYS);
    // Override preset defaults to match original tight 8-bit sound
    p->p_fmModIndex = 1.8f;        // brighter than preset's 1.2
    p->p_sustain = 0.25f;          // tighter than preset's 0.3
    p->p_release = 0.3f;           // shorter than preset's 0.4
    p->p_vibratoRate = 0.0f;       // no vibrato (preset has 5.5)
    p->p_vibratoDepth = 0.0f;      // no vibrato (preset has 0.12)
    p->p_volume = accent ? 0.50f : 0.40f;
    p->p_filterCutoff = 0.6f;
    bridgeMelodyTrigger(track, note, vel, false, false, p); // accent already in volume
}
static void melodyTriggerFMKeys(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide;
    melodyTriggerFMKeysOnTrack(2, note, vel, accent);
}
static void melodyTriggerFMKeysLead(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide;
    melodyTriggerFMKeysOnTrack(1, note, vel, accent);
}

// --- House song (sweep-modulated instruments) ---
static void melodyTriggerAcidBass(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime;
    SynthPatch *p = songPatch(0, PRESET_ACID);
    float sweep = getSweepValue();
    p->p_filterCutoff = 0.08f + sweep * 0.57f;
    p->p_filterResonance = 0.45f + sweep * 0.15f;
    p->p_filterEnvAmt = 0.3f;
    p->p_volume = accent ? 0.60f : 0.50f;
    p->p_glideTime = 0.15f;
    bridgeMelodyTrigger(0, note, vel, slide, false, p); // accent already in volume
}
static void melodyTriggerHouseStab(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    SynthPatch *p = songPatch(1, PRESET_MAC_KEYS);
    float sweep = getSweepValue();
    p->p_waveType = WAVE_FM;
    p->p_fmModRatio = 2.0f;
    p->p_fmModIndex = 0.5f + sweep * 2.5f;
    p->p_attack = 0.002f; p->p_decay = 0.3f; p->p_sustain = 0.1f; p->p_release = 0.4f;
    p->p_volume = 0.40f;
    p->p_filterCutoff = 0.20f + sweep * 0.45f;
    p->p_filterResonance = 0.10f;
    bridgeMelodyTrigger(1, note, vel, false, false, p);
}
static void melodyTriggerDeepPad(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    SynthPatch *p = songPatch(2, PRESET_LUSH_STR);
    float sweep = getSweepValue();
    p->p_attack = 1.5f; p->p_decay = 2.0f; p->p_release = 3.0f;
    p->p_filterCutoff = 0.12f + (1.0f - sweep) * 0.40f;
    p->p_filterResonance = 0.08f;
    bridgeMelodyTrigger(2, note, vel, false, false, p);
}
static void melodyTriggerSubBass(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime;
    bridgeMelodyTrigger(0, note, vel, slide, accent, songPatch(0, PRESET_TRI_SUB));
}

// --- Dilla hip-hop ---
static void melodyTriggerDillaBass(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime;
    SynthPatch *p = songPatch(0, PRESET_WARM_PLUCK);
    p->p_pluckBrightness = 0.35f; p->p_pluckDamping = 0.55f;
    p->p_filterCutoff = 0.20f; p->p_filterResonance = 0.12f;
    bridgeMelodyTrigger(0, note, vel, slide, accent, p);
}
static void melodyTriggerDillaKeys(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide;
    SynthPatch *p = songPatch(1, PRESET_MAC_KEYS);
    p->p_decay = 0.35f; p->p_sustain = 0.15f; p->p_release = 0.5f;
    p->p_volume = accent ? 0.45f : 0.35f;
    p->p_filterCutoff = 0.35f; p->p_filterResonance = 0.08f;
    bridgeMelodyTrigger(1, note, vel, false, false, p); // accent already in volume
}
static void melodyTriggerDillaPad(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime;
    bridgeMelodyTrigger(2, note, vel, slide, accent, songPatch(2, PRESET_DARK_CHOIR));
}

// --- Mr Lucky ---
static void melodyTriggerMrLuckyBass(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime;
    SynthPatch *p = songPatch(0, PRESET_WARM_PLUCK);
    p->p_pluckBrightness = 0.45f; p->p_pluckDamping = 0.35f;
    p->p_filterCutoff = 0.35f; p->p_volume = 0.50f;
    bridgeMelodyTrigger(0, note, vel, slide, accent, p);
}
static void melodyTriggerMrLuckyVibes(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)slide; (void)accent;
    SynthPatch *p = songPatch(1, PRESET_MAC_VIBES);
    p->p_volume = 0.42f; p->p_filterCutoff = 0.55f;
    bridgeMelodyTrigger(1, note, vel, false, false, p);
}
static void melodyTriggerMrLuckyStrings(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime;
    bridgeMelodyTrigger(2, note, vel, slide, accent, songPatch(2, PRESET_LUSH_STR));
}

// --- Atmosphere ---
static void melodyTriggerAtmoBass(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime;
    SynthPatch *p = songPatch(0, PRESET_WARM_PLUCK);
    p->p_pluckBrightness = 0.5f; p->p_pluckDamping = 0.3f;
    p->p_filterCutoff = 0.30f; p->p_filterResonance = 0.08f; p->p_volume = 0.50f;
    bridgeMelodyTrigger(0, note, vel, slide, accent, p);
}
static void melodyTriggerAtmoGlocken(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime;
    SynthPatch *p = songPatch(1, PRESET_GLOCK);
    p->p_volume = 0.45f; p->p_filterCutoff = 0.65f;
    bridgeMelodyTrigger(1, note, vel, slide, accent, p);
}
static void melodyTriggerAtmoStrings(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime;
    SynthPatch *p = songPatch(2, PRESET_LUSH_STR);
    p->p_attack = 1.2f; p->p_decay = 2.0f; p->p_sustain = 0.6f; p->p_release = 3.5f;
    p->p_volume = 0.35f; p->p_filterCutoff = 0.45f; p->p_filterResonance = 0.05f;
    bridgeMelodyTrigger(2, note, vel, slide, accent, p);
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

    // Set up the sequencer with SynthPatch-based drum triggers
    initBridgeDrumPatches();
    initSequencer(bridgeDrumKick, bridgeDrumSnare, bridgeDrumHH, bridgeDrumClap);

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

// --- M.U.L.E. v2: true 8-bit square bass + sawtooth melody ---
static void melodyTrigger8bitSquare(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);
    SongPlayer *sp = &g_soundSynth->songPlayer;

    if (sp->melodyVoices[0][0] >= 0) releaseNote(sp->melodyVoices[0][0]);

    resetNoteGlobals();
    noteAttack  = 0.001f;
    noteDecay   = 0.08f;
    noteSustain = 0.8f;
    noteRelease = 0.02f;
    noteVolume  = vel * 0.45f;
    noteFilterCutoff    = 1.0f;
    noteFilterResonance = 0.0f;
    noteVibratoRate  = 0.0f;
    noteVibratoDepth = 0.0f;

    float freq = midiToFreqScaled(note);
    int v = playNote(freq, WAVE_SQUARE);
    if (v >= 0 && slide) {
        synthVoices[v].glideRate = 8.0f;
    }
    sp->melodyVoices[0][0] = v;
    sp->melodyVoiceCount[0] = 1;
}

static void melodyTrigger8bitSaw(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime; (void)accent;
    if (!g_soundSynth) return;
    useSoundSystem(&g_soundSynth->ss);
    SongPlayer *sp = &g_soundSynth->songPlayer;

    if (sp->melodyVoices[1][0] >= 0) releaseNote(sp->melodyVoices[1][0]);

    resetNoteGlobals();
    noteAttack  = 0.001f;
    noteDecay   = 0.1f;
    noteSustain = 0.7f;
    noteRelease = 0.03f;
    noteVolume  = vel * 0.55f;
    noteFilterCutoff    = 0.85f;
    noteFilterResonance = 0.0f;
    noteVibratoRate  = 0.0f;
    noteVibratoDepth = 0.0f;

    float freq = midiToFreqScaled(note);
    int v = playNote(freq, WAVE_SAW);
    if (v >= 0 && slide) {
        synthVoices[v].glideRate = 8.0f;
    }
    sp->melodyVoices[1][0] = v;
    sp->melodyVoiceCount[1] = 1;
}

void SoundSynthPlaySongMule2(SoundSynth* synth) {
    if (!synth || !synth->audioReady) return;
    useSoundSystem(&synth->ss);

    SoundSynthStopSong(synth);

    // 8-bit: square bass (track 0), sawtooth melody (track 1)
    setMelodyCallbacks(0, melodyTrigger8bitSquare, melodyReleaseBass);
    setMelodyCallbacks(1, melodyTrigger8bitSaw, melodyReleaseLeadPoly);

    Song_Mule2_ConfigureVoices();

    SongPlayer* sp = &synth->songPlayer;
    Song_Mule2_Load(sp->patterns);
    sp->patternCount = 8;
    sp->currentPattern = 0;
    sp->loopsOnCurrent = 0;
    sp->loopsPerPattern = 1;
    sp->bpm = SONG_MULE2_BPM;
    sp->sweepPhase = 0.0f;
    sp->sweepRate = 0.0f;

    // 32nd note resolution for grace notes
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

    // Load patterns
    sp->patternCount = 0;
    for (int i = 0; i < SEQ_NUM_PATTERNS && i < MAX_SONG_PATTERNS; i++) {
        sp->patterns[i] = sf.patterns[i];
        // Check if pattern has any content
        bool hasContent = false;
        for (int t = 0; t < SEQ_DRUM_TRACKS && !hasContent; t++)
            for (int s = 0; s < SEQ_MAX_STEPS && !hasContent; s++)
                if (patGetDrum(&sf.patterns[i], t, s)) hasContent = true;
        for (int t = 0; t < SEQ_MELODY_TRACKS && !hasContent; t++)
            for (int s = 0; s < SEQ_MAX_STEPS && !hasContent; s++)
                if (patGetNote(&sf.patterns[i], SEQ_DRUM_TRACKS + t, s) != SEQ_NOTE_OFF) hasContent = true;
        if (hasContent) sp->patternCount = i + 1;
    }
    if (sp->patternCount == 0) sp->patternCount = 1;
    sp->currentPattern = 0;
    sp->loopsOnCurrent = 0;
    sp->loopsPerPattern = sf.loopsPerPattern > 0 ? sf.loopsPerPattern : 2;

    // Load chain
    sp->chainLength = sf.chainLength;
    sp->chainPos = 0;
    for (int i = 0; i < sf.chainLength && i < SEQ_MAX_CHAIN; i++) {
        sp->chain[i] = sf.chain[i];
    }

    // Apply mix
    masterVolume = sf.sfMasterVolume;
    drumVolume = sf.sfDrumVolume;
    for (int i = 0; i < SEQ_V2_MAX_TRACKS; i++) seq.trackVolume[i] = sf.sfTrackVolume[i];

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
    { "M.U.L.E. v2 (MIDI)",    SoundSynthPlaySongMule2 },
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
