// PixelSynth Demo - Chiptune synth with 808 drums
// Notes: ASDFGHJKL (white) + WERTYUIOP (black), Z/X = octave
// SFX: 1-6, Drums: 7-0,-,=

#include "../../vendor/raylib.h"
#include "../../assets/fonts/comic_embedded.h"
#define UI_IMPLEMENTATION
#include "../../shared/ui.h"
#include <math.h>
#include <string.h>

#define SCREEN_WIDTH 1200
#define SCREEN_HEIGHT 860
#define SAMPLE_RATE 44100
#define MAX_SAMPLES_PER_UPDATE 4096

// ============================================================================
// INCLUDE ENGINES
// ============================================================================

#include "../engines/synth.h"    // Synth voices, wavetables, formant, SFX
#include "../engines/scw_data.h" // Embedded single-cycle waveforms (run `make scw_embed` to regenerate)
#include "../engines/drums.h"    // 808-style drum machine
#include "../engines/effects.h"  // Distortion, delay, tape, bitcrusher
#include "../engines/sequencer.h" // Drum step sequencer

// ============================================================================
// SPEECH SYSTEM
// ============================================================================

#define SPEECH_MAX 64

typedef struct {
    char text[SPEECH_MAX];
    int index;
    int length;
    float timer;
    float speed;
    float basePitch;
    float pitchVariation;
    float intonation;  // -1.0 = falling (answer), 0 = flat, +1.0 = rising (question)
    bool active;
    int voiceIndex;
} SpeechQueue;

static SpeechQueue speechQueue = {0};

// Map character to vowel sound
static VowelType charToVowel(char c) {
    if (c >= 'A' && c <= 'Z') c += 32;
    switch (c) {
        case 'a': return VOWEL_A;
        case 'e': return VOWEL_E;
        case 'i': case 'y': return VOWEL_I;
        case 'o': return VOWEL_O;
        case 'u': case 'w': return VOWEL_U;
        case 'b': case 'p': case 'm': return VOWEL_U;
        case 'd': case 't': case 'n': case 'l': return VOWEL_E;
        case 'g': case 'k': case 'q': return VOWEL_A;
        case 'f': case 'v': case 's': case 'z': case 'c': return VOWEL_I;
        case 'r': return VOWEL_A;
        default: return VOWEL_A;
    }
}

// Pitch variation for melodic speech
static float charToPitch(char c) {
    if (c >= 'A' && c <= 'Z') c += 32;
    int val = (c * 7) % 12;
    return 1.0f + (val - 6) * 0.05f;
}

// Start speaking text with intonation contour
static void speakWithIntonation(const char *text, float speed, float pitch, float variation, float intonation) {
    SpeechQueue *sq = &speechQueue;
    
    int len = 0;
    while (text[len] && len < SPEECH_MAX - 1) {
        sq->text[len] = text[len];
        len++;
    }
    sq->text[len] = '\0';
    sq->length = len;
    sq->index = -1;
    sq->timer = 0.0f;
    sq->speed = clampf(speed, 1.0f, 30.0f);
    sq->basePitch = clampf(pitch, 0.3f, 3.0f);
    sq->pitchVariation = clampf(variation, 0.0f, 1.0f);
    sq->intonation = clampf(intonation, -1.0f, 1.0f);
    sq->active = true;
    sq->voiceIndex = NUM_VOICES - 1;
}

// Start speaking text (flat intonation)
static void speak(const char *text, float speed, float pitch, float variation) {
    speakWithIntonation(text, speed, pitch, variation, 0.0f);
}

// Syllables for babble generation
static const char* babbleSyllables[] = {
    "ba", "da", "ga", "ma", "na", "pa", "ta", "ka", "wa", "ya",
    "be", "de", "ge", "me", "ne", "pe", "te", "ke", "we", "ye",
    "bi", "di", "gi", "mi", "ni", "pi", "ti", "ki", "wi", "yi",
    "bo", "do", "go", "mo", "no", "po", "to", "ko", "wo", "yo",
    "bu", "du", "gu", "mu", "nu", "pu", "tu", "ku", "wu", "yu",
    "la", "ra", "sa", "za", "ha", "ja", "fa", "va",
};
static const int numBabbleSyllables = sizeof(babbleSyllables) / sizeof(babbleSyllables[0]);

// Generate babble with optional intonation
static void babbleWithIntonation(float duration, float pitch, float mood, float intonation) {
    char text[SPEECH_MAX];
    int pos = 0;
    float speed = 8.0f + mood * 8.0f;
    int targetSyllables = (int)(duration * speed / 2.0f);
    
    for (int i = 0; i < targetSyllables && pos < SPEECH_MAX - 4; i++) {
        synthNoiseState = synthNoiseState * 1103515245 + 12345;
        const char* syl = babbleSyllables[(synthNoiseState >> 16) % numBabbleSyllables];
        while (*syl && pos < SPEECH_MAX - 2) {
            text[pos++] = *syl++;
        }
        synthNoiseState = synthNoiseState * 1103515245 + 12345;
        if ((synthNoiseState >> 16) % 4 == 0 && pos < SPEECH_MAX - 2) {
            text[pos++] = ' ';
        }
    }
    text[pos] = '\0';
    
    float variation = 0.1f + mood * 0.3f;
    speakWithIntonation(text, speed, pitch, variation, intonation);
}

// Generate random babble (flat intonation)
static void babble(float duration, float pitch, float mood) {
    babbleWithIntonation(duration, pitch, mood, 0.0f);
}

// Babble call (question - rising intonation)
static void babbleCall(float duration, float pitch, float mood) {
    babbleWithIntonation(duration, pitch, mood, 1.0f);
}

// Babble answer (response - falling intonation)
static void babbleAnswer(float duration, float pitch, float mood) {
    babbleWithIntonation(duration, pitch, mood, -1.0f);
}

// Process speech queue
static void updateSpeech(float dt) {
    SpeechQueue *sq = &speechQueue;
    if (!sq->active) return;
    
    sq->timer -= dt;
    if (sq->timer <= 0.0f) {
        sq->index++;
        
        if (sq->index >= sq->length) {
            sq->active = false;
            releaseNote(sq->voiceIndex);
            return;
        }
        
        char c = sq->text[sq->index];
        
        if (c == ' ' || c == ',' || c == '.') {
            sq->timer = (c == ' ') ? 0.5f / sq->speed : 1.0f / sq->speed;
            releaseNote(sq->voiceIndex);
            return;
        }
        
        VowelType vowel = charToVowel(c);
        float pitchMod = charToPitch(c);
        
        synthNoiseState = synthNoiseState * 1103515245 + 12345;
        float randVar = 1.0f + ((float)(synthNoiseState >> 16) / 65535.0f - 0.5f) * sq->pitchVariation;
        
        // Apply intonation contour (position-based pitch shift)
        float progress = (float)sq->index / (float)sq->length;
        float intonationMod = 1.0f + sq->intonation * 0.3f * progress;  // Up to ±30% pitch shift at end
        
        float baseFreq = 200.0f * sq->basePitch * pitchMod * randVar * intonationMod;
        
        Voice *v = &synthVoices[sq->voiceIndex];
        
        if (v->envStage > 0 && v->wave == WAVE_VOICE) {
            v->voiceSettings.nextVowel = vowel;
            v->voiceSettings.vowelBlend = 0.0f;
            v->frequency = baseFreq;
            v->baseFrequency = baseFreq;
        } else {
            playVowelOnVoice(sq->voiceIndex, baseFreq, vowel);
        }
        
        sq->timer = 1.0f / sq->speed;
    }
    
    // Animate vowel blend
    Voice *v = &synthVoices[sq->voiceIndex];
    if (v->envStage > 0 && v->wave == WAVE_VOICE) {
        v->voiceSettings.vowelBlend += dt * sq->speed * 2.0f;
        if (v->voiceSettings.vowelBlend >= 1.0f) {
            v->voiceSettings.vowelBlend = 0.0f;
            v->voiceSettings.vowel = v->voiceSettings.nextVowel;
        }
    }
}

// ============================================================================
// AUDIO CALLBACK
// ============================================================================

static double audioTimeUs = 0.0;
static int audioFrameCount = 0;

// Melody voice indices for sidechain targeting (Bass=0, Lead=1, Chord=2)
// Declared here so audio callback can route voices to correct buses
static int melodyVoiceIdx[SEQ_MELODY_TRACKS] = {-1, -1, -1};

// Which drum sound is assigned to each track (forward declaration for audio callback)
// Full definition with names is later in file
static DrumType drumTrackSound[SEQ_DRUM_TRACKS] = {
    DRUM_KICK,      // Track 0 default
    DRUM_SNARE,     // Track 1 default
    DRUM_CLOSED_HH, // Track 2 default
    DRUM_CLAP       // Track 3 default
};

static void SynthCallback(void *buffer, unsigned int frames) {
    double startTime = GetTime();
    
    short *d = (short *)buffer;
    float dt = 1.0f / SAMPLE_RATE;
    
    // Sync mixer tempo with sequencer
    setMixerTempo(seq.bpm);
    
    for (unsigned int i = 0; i < frames; i++) {
        float sample = 0.0f;
        
        // Bus input array: [DRUM0, DRUM1, DRUM2, DRUM3, BASS, LEAD, CHORD]
        float busInputs[NUM_BUSES] = {0};
        
        // Process each drum track to its bus
        busInputs[BUS_DRUM0] = processDrumType(drumTrackSound[0], dt);
        busInputs[BUS_DRUM1] = processDrumType(drumTrackSound[1], dt);
        busInputs[BUS_DRUM2] = processDrumType(drumTrackSound[2], dt);
        busInputs[BUS_DRUM3] = processDrumType(drumTrackSound[3], dt);
        
        // Process synth voices and route to buses
        for (int v = 0; v < NUM_VOICES; v++) {
            float voiceSample = processVoice(&synthVoices[v], SAMPLE_RATE);
            
            // Route to appropriate bus based on which track owns this voice
            if (v == melodyVoiceIdx[0]) {
                busInputs[BUS_BASS] += voiceSample;
            } else if (v == melodyVoiceIdx[1]) {
                busInputs[BUS_LEAD] += voiceSample;
            } else if (v == melodyVoiceIdx[2]) {
                busInputs[BUS_CHORD] += voiceSample;
            } else {
                // Other voices (keyboard/preview) go to chord bus for now
                busInputs[BUS_CHORD] += voiceSample;
            }
        }
        
        // Sidechain: extract source from appropriate drum bus
        float sidechainSample = 0.0f;
        switch (fx.sidechainSource) {
            case SIDECHAIN_SRC_KICK:
                sidechainSample = busInputs[BUS_DRUM0];  // Assuming track 0 is kick
                break;
            case SIDECHAIN_SRC_SNARE:
                sidechainSample = busInputs[BUS_DRUM1];  // Assuming track 1 is snare
                break;
            case SIDECHAIN_SRC_CLAP:
                sidechainSample = busInputs[BUS_DRUM1];  // Clap often on snare track
                break;
            case SIDECHAIN_SRC_HIHAT:
                sidechainSample = busInputs[BUS_DRUM2];  // Assuming track 2 is hats
                break;
            case SIDECHAIN_SRC_ALL:
            default:
                sidechainSample = busInputs[BUS_DRUM0] + busInputs[BUS_DRUM1] + 
                                  busInputs[BUS_DRUM2] + busInputs[BUS_DRUM3];
                break;
        }
        
        // Update sidechain envelope
        updateSidechainEnvelope(sidechainSample, dt);
        
        // Apply sidechain ducking to synth buses (pre-bus processing)
        if (fx.sidechainEnabled) {
            switch (fx.sidechainTarget) {
                case SIDECHAIN_TGT_BASS:
                    busInputs[BUS_BASS] = applySidechainDucking(busInputs[BUS_BASS]);
                    break;
                case SIDECHAIN_TGT_LEAD:
                    busInputs[BUS_LEAD] = applySidechainDucking(busInputs[BUS_LEAD]);
                    break;
                case SIDECHAIN_TGT_CHORD:
                    busInputs[BUS_CHORD] = applySidechainDucking(busInputs[BUS_CHORD]);
                    break;
                case SIDECHAIN_TGT_ALL:
                default:
                    busInputs[BUS_BASS] = applySidechainDucking(busInputs[BUS_BASS]);
                    busInputs[BUS_LEAD] = applySidechainDucking(busInputs[BUS_LEAD]);
                    busInputs[BUS_CHORD] = applySidechainDucking(busInputs[BUS_CHORD]);
                    break;
            }
        }
        
        // Process through mixer bus system → master effects → output
        sample = processMixerOutput(busInputs, dt);
        
        sample *= masterVolume;
        
        // Clamp
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;
        
        d[i] = (short)(sample * 32000.0f);
    }
    
    double elapsed = (GetTime() - startTime) * 1000000.0;
    audioTimeUs = audioTimeUs * 0.95 + elapsed * 0.05;
    audioFrameCount = frames;
}

// ============================================================================
// MAIN
// ============================================================================

// ============================================================================
// PIANO KEYBOARD SYSTEM
// ============================================================================

// Octave control
static int currentOctave = 4;  // Default to octave 4 (middle C = C4)
#define MIN_OCTAVE 1
#define MAX_OCTAVE 7

// Piano-style key mapping (white keys on ASDFGHJKL, black keys on WERTYUIOP)
// Semitone offsets from C: C=0, C#=1, D=2, D#=3, E=4, F=5, F#=6, G=7, G#=8, A=9, A#=10, B=11
typedef struct {
    int key;
    int semitone;  // Semitones above C of the current octave
} PianoKey;

static const PianoKey pianoKeys[] = {
    // White keys (bottom row) - ASDFGHJKL
    {KEY_A, 0},   // C
    {KEY_S, 2},   // D
    {KEY_D, 4},   // E
    {KEY_F, 5},   // F
    {KEY_G, 7},   // G
    {KEY_H, 9},   // A
    {KEY_J, 11},  // B
    {KEY_K, 12},  // C+1
    {KEY_L, 14},  // D+1
    // Black keys (top row) - WERTYUIOP
    {KEY_W, 1},   // C#
    {KEY_E, 3},   // D#
    {KEY_R, 6},   // F#
    {KEY_T, 8},   // G#
    {KEY_Y, 10},  // A#
    {KEY_U, 13},  // C#+1
    {KEY_I, 15},  // D#+1
    {KEY_O, 18},  // F#+1
    {KEY_P, 20},  // G#+1
};
#define NUM_PIANO_KEYS (sizeof(pianoKeys) / sizeof(pianoKeys[0]))

// Track which voice is playing each piano key
static int pianoKeyVoices[NUM_PIANO_KEYS];

// Calculate frequency from semitone offset and octave
static float semitoneToFreq(int semitone, int octave) {
    // C0 = 16.35 Hz (MIDI note 12)
    float c0 = 16.351597831287414f;
    int totalSemitones = octave * 12 + semitone;
    // Apply scale lock if enabled
    if (scaleLockEnabled) {
        totalSemitones = constrainToScale(totalSemitones);
    }
    return c0 * powf(2.0f, totalSemitones / 12.0f);
}

// ============================================================================
// SYNTH PATCHES (Per-track synth settings)
// ============================================================================

typedef struct {
    // Wave type
    int p_waveType;
    int p_scwIndex;
    
    // Envelope
    float p_attack;
    float p_decay;
    float p_sustain;
    float p_release;
    float p_volume;
    
    // PWM (for square wave)
    float p_pulseWidth;
    float p_pwmRate;
    float p_pwmDepth;
    
    // Vibrato
    float p_vibratoRate;
    float p_vibratoDepth;
    
    // Filter
    float p_filterCutoff;
    float p_filterResonance;
    float p_filterEnvAmt;
    float p_filterEnvAttack;
    float p_filterEnvDecay;
    
    // Filter LFO
    float p_filterLfoRate;
    float p_filterLfoDepth;
    int p_filterLfoShape;
    
    // Resonance LFO
    float p_resoLfoRate;
    float p_resoLfoDepth;
    int p_resoLfoShape;
    
    // Amplitude LFO
    float p_ampLfoRate;
    float p_ampLfoDepth;
    int p_ampLfoShape;
    
    // Pitch LFO
    float p_pitchLfoRate;
    float p_pitchLfoDepth;
    int p_pitchLfoShape;
    
    // Mono/Glide
    bool p_monoMode;
    float p_glideTime;
    
    // Pluck settings
    float p_pluckBrightness;
    float p_pluckDamping;
    float p_pluckDamp;
    
    // Additive
    int p_additivePreset;
    float p_additiveBrightness;
    float p_additiveShimmer;
    float p_additiveInharmonicity;
    
    // Mallet
    int p_malletPreset;
    float p_malletStiffness;
    float p_malletHardness;
    float p_malletStrikePos;
    float p_malletResonance;
    float p_malletTremolo;
    float p_malletTremoloRate;
    float p_malletDamp;
    
    // Voice (formant)
    int p_voiceVowel;
    float p_voiceFormantShift;
    float p_voiceBreathiness;
    float p_voiceBuzziness;
    float p_voiceSpeed;
    float p_voicePitch;
    bool p_voiceConsonant;
    float p_voiceConsonantAmt;
    bool p_voiceNasal;
    float p_voiceNasalAmt;
    float p_voicePitchEnv;
    float p_voicePitchEnvTime;
    float p_voicePitchEnvCurve;
    
    // Granular
    int p_granularScwIndex;
    float p_granularGrainSize;
    float p_granularDensity;
    float p_granularPosition;
    float p_granularPosRandom;
    float p_granularPitch;
    float p_granularPitchRandom;
    float p_granularAmpRandom;
    float p_granularSpread;
    bool p_granularFreeze;
    
    // FM
    float p_fmModRatio;
    float p_fmModIndex;
    float p_fmFeedback;
    
    // Phase Distortion
    int p_pdWaveType;
    float p_pdDistortion;
    
    // Membrane
    int p_membranePreset;
    float p_membraneDamping;
    float p_membraneStrike;
    float p_membraneBend;
    float p_membraneBendDecay;
    
    // Bird
    int p_birdType;
    float p_birdChirpRange;
    float p_birdTrillRate;
    float p_birdTrillDepth;
    float p_birdAmRate;
    float p_birdAmDepth;
    float p_birdHarmonics;
} SynthPatch;

// Default patch initializer
static SynthPatch createDefaultPatch(int waveType) {
    return (SynthPatch){
        .p_waveType = waveType,
        .p_scwIndex = 0,
        .p_attack = 0.01f,
        .p_decay = 0.1f,
        .p_sustain = 0.5f,
        .p_release = 0.3f,
        .p_volume = 0.5f,
        .p_pulseWidth = 0.5f,
        .p_pwmRate = 3.0f,
        .p_pwmDepth = 0.0f,
        .p_vibratoRate = 5.0f,
        .p_vibratoDepth = 0.0f,
        .p_filterCutoff = 1.0f,
        .p_filterResonance = 0.0f,
        .p_filterEnvAmt = 0.0f,
        .p_filterEnvAttack = 0.01f,
        .p_filterEnvDecay = 0.2f,
        .p_filterLfoRate = 0.0f,
        .p_filterLfoDepth = 0.0f,
        .p_filterLfoShape = 0,
        .p_resoLfoRate = 0.0f,
        .p_resoLfoDepth = 0.0f,
        .p_resoLfoShape = 0,
        .p_ampLfoRate = 0.0f,
        .p_ampLfoDepth = 0.0f,
        .p_ampLfoShape = 0,
        .p_pitchLfoRate = 5.0f,
        .p_pitchLfoDepth = 0.0f,
        .p_pitchLfoShape = 0,
        .p_monoMode = false,
        .p_glideTime = 0.1f,
        .p_pluckBrightness = 0.5f,
        .p_pluckDamping = 0.996f,
        .p_pluckDamp = 0.0f,
        .p_additivePreset = ADDITIVE_PRESET_ORGAN,
        .p_additiveBrightness = 0.5f,
        .p_additiveShimmer = 0.0f,
        .p_additiveInharmonicity = 0.0f,
        .p_malletPreset = MALLET_PRESET_MARIMBA,
        .p_malletStiffness = 0.3f,
        .p_malletHardness = 0.5f,
        .p_malletStrikePos = 0.25f,
        .p_malletResonance = 0.7f,
        .p_malletTremolo = 0.0f,
        .p_malletTremoloRate = 5.5f,
        .p_malletDamp = 0.0f,
        .p_voiceVowel = VOWEL_A,
        .p_voiceFormantShift = 1.0f,
        .p_voiceBreathiness = 0.1f,
        .p_voiceBuzziness = 0.6f,
        .p_voiceSpeed = 10.0f,
        .p_voicePitch = 1.0f,
        .p_voiceConsonant = false,
        .p_voiceConsonantAmt = 0.5f,
        .p_voiceNasal = false,
        .p_voiceNasalAmt = 0.5f,
        .p_voicePitchEnv = 0.0f,
        .p_voicePitchEnvTime = 0.15f,
        .p_voicePitchEnvCurve = 0.0f,
        .p_granularScwIndex = 0,
        .p_granularGrainSize = 50.0f,
        .p_granularDensity = 20.0f,
        .p_granularPosition = 0.5f,
        .p_granularPosRandom = 0.1f,
        .p_granularPitch = 1.0f,
        .p_granularPitchRandom = 0.0f,
        .p_granularAmpRandom = 0.1f,
        .p_granularSpread = 0.5f,
        .p_granularFreeze = false,
        .p_fmModRatio = 2.0f,
        .p_fmModIndex = 1.0f,
        .p_fmFeedback = 0.0f,
        .p_pdWaveType = PD_WAVE_SAW,
        .p_pdDistortion = 0.5f,
        .p_membranePreset = MEMBRANE_TABLA,
        .p_membraneDamping = 0.3f,
        .p_membraneStrike = 0.3f,
        .p_membraneBend = 0.15f,
        .p_membraneBendDecay = 0.08f,
        .p_birdType = BIRD_CHIRP,
        .p_birdChirpRange = 1.0f,
        .p_birdTrillRate = 0.0f,
        .p_birdTrillDepth = 0.0f,
        .p_birdAmRate = 0.0f,
        .p_birdAmDepth = 0.0f,
        .p_birdHarmonics = 0.2f,
    };
}

// 5 patches: Preview (jamming), Bass, Lead, Chord + copy targets
#define PATCH_PREVIEW 0
#define PATCH_BASS 1
#define PATCH_LEAD 2
#define PATCH_CHORD 3
#define NUM_PATCHES 4

static const char* patchNames[NUM_PATCHES] = {"Preview", "Bass", "Lead", "Chord"};
static SynthPatch patches[NUM_PATCHES];
static int selectedPatch = PATCH_PREVIEW;

// ============================================================================
// SCENES (Snapshots of all sound parameters)
// ============================================================================

#define NUM_SCENES 8

typedef struct {
    SynthPatch patches[NUM_PATCHES];  // All 4 synth patches
    DrumParams drums;                  // Drum parameters
    Effects effects;                   // Effect parameters
    float masterVol;                   // Master volume
    float drumVol;                     // Drum volume
    bool initialized;                  // Has this scene been saved?
} Scene;

static Scene scenes[NUM_SCENES];
static int currentScene = -1;  // -1 = no scene loaded, 0-7 = active scene

// Save current state to a scene slot
static void saveScene(int idx) {
    if (idx < 0 || idx >= NUM_SCENES) return;
    Scene *s = &scenes[idx];
    
    // Copy all patches
    for (int i = 0; i < NUM_PATCHES; i++) {
        s->patches[i] = patches[i];
    }
    
    // Copy drums and effects
    s->drums = drumParams;
    s->effects = fx;
    
    // Copy volumes
    s->masterVol = masterVolume;
    s->drumVol = drumVolume;
    
    s->initialized = true;
    currentScene = idx;
}

// Load a scene instantly
static void loadScene(int idx) {
    if (idx < 0 || idx >= NUM_SCENES) return;
    Scene *s = &scenes[idx];
    if (!s->initialized) return;
    
    // Restore all patches
    for (int i = 0; i < NUM_PATCHES; i++) {
        patches[i] = s->patches[i];
    }
    
    // Restore drums and effects
    drumParams = s->drums;
    fx = s->effects;
    
    // Restore volumes
    masterVolume = s->masterVol;
    drumVolume = s->drumVol;
    
    currentScene = idx;
}

// Clear a scene slot
static void clearScene(int idx) {
    if (idx < 0 || idx >= NUM_SCENES) return;
    scenes[idx].initialized = false;
    if (currentScene == idx) currentScene = -1;
}

// ============================================================================
// CROSSFADER (A/B Scene Blending)
// ============================================================================

typedef struct {
    int sceneA;         // Scene index for A side (0-7)
    int sceneB;         // Scene index for B side (0-7)
    float position;     // 0.0 = full A, 1.0 = full B
} CrossfaderState;

static CrossfaderState crossfader = {0, 1, 0.0f};
static bool crossfaderEnabled = false;  // When enabled, blending is active
static bool g_showHelpOverlay = false;  // Help tooltip overlay state
static bool g_showSeqHelpOverlay = false;  // Sequencer help overlay state

// Note: lerpf() already defined in synth.h

// Threshold switch for integers (switch at 50%)
static int switchInt(int a, int b, float t) {
    return t < 0.5f ? a : b;
}

// Threshold switch for bools
static bool switchBool(bool a, bool b, float t) {
    return t < 0.5f ? a : b;
}

// Blend two SynthPatch structs
static void blendSynthPatch(SynthPatch *out, const SynthPatch *a, const SynthPatch *b, float t) {
    // Discrete params (switch at 50%)
    out->p_waveType = switchInt(a->p_waveType, b->p_waveType, t);
    out->p_scwIndex = switchInt(a->p_scwIndex, b->p_scwIndex, t);
    out->p_filterLfoShape = switchInt(a->p_filterLfoShape, b->p_filterLfoShape, t);
    out->p_resoLfoShape = switchInt(a->p_resoLfoShape, b->p_resoLfoShape, t);
    out->p_ampLfoShape = switchInt(a->p_ampLfoShape, b->p_ampLfoShape, t);
    out->p_pitchLfoShape = switchInt(a->p_pitchLfoShape, b->p_pitchLfoShape, t);
    out->p_additivePreset = switchInt(a->p_additivePreset, b->p_additivePreset, t);
    out->p_malletPreset = switchInt(a->p_malletPreset, b->p_malletPreset, t);
    out->p_voiceVowel = switchInt(a->p_voiceVowel, b->p_voiceVowel, t);
    out->p_granularScwIndex = switchInt(a->p_granularScwIndex, b->p_granularScwIndex, t);
    out->p_pdWaveType = switchInt(a->p_pdWaveType, b->p_pdWaveType, t);
    out->p_membranePreset = switchInt(a->p_membranePreset, b->p_membranePreset, t);
    out->p_birdType = switchInt(a->p_birdType, b->p_birdType, t);
    out->p_monoMode = switchBool(a->p_monoMode, b->p_monoMode, t);
    out->p_voiceConsonant = switchBool(a->p_voiceConsonant, b->p_voiceConsonant, t);
    out->p_voiceNasal = switchBool(a->p_voiceNasal, b->p_voiceNasal, t);
    out->p_granularFreeze = switchBool(a->p_granularFreeze, b->p_granularFreeze, t);
    
    // Continuous params (linear interpolation)
    out->p_attack = lerpf(a->p_attack, b->p_attack, t);
    out->p_decay = lerpf(a->p_decay, b->p_decay, t);
    out->p_sustain = lerpf(a->p_sustain, b->p_sustain, t);
    out->p_release = lerpf(a->p_release, b->p_release, t);
    out->p_volume = lerpf(a->p_volume, b->p_volume, t);
    out->p_pulseWidth = lerpf(a->p_pulseWidth, b->p_pulseWidth, t);
    out->p_pwmRate = lerpf(a->p_pwmRate, b->p_pwmRate, t);
    out->p_pwmDepth = lerpf(a->p_pwmDepth, b->p_pwmDepth, t);
    out->p_vibratoRate = lerpf(a->p_vibratoRate, b->p_vibratoRate, t);
    out->p_vibratoDepth = lerpf(a->p_vibratoDepth, b->p_vibratoDepth, t);
    out->p_filterCutoff = lerpf(a->p_filterCutoff, b->p_filterCutoff, t);
    out->p_filterResonance = lerpf(a->p_filterResonance, b->p_filterResonance, t);
    out->p_filterEnvAmt = lerpf(a->p_filterEnvAmt, b->p_filterEnvAmt, t);
    out->p_filterEnvAttack = lerpf(a->p_filterEnvAttack, b->p_filterEnvAttack, t);
    out->p_filterEnvDecay = lerpf(a->p_filterEnvDecay, b->p_filterEnvDecay, t);
    out->p_filterLfoRate = lerpf(a->p_filterLfoRate, b->p_filterLfoRate, t);
    out->p_filterLfoDepth = lerpf(a->p_filterLfoDepth, b->p_filterLfoDepth, t);
    out->p_resoLfoRate = lerpf(a->p_resoLfoRate, b->p_resoLfoRate, t);
    out->p_resoLfoDepth = lerpf(a->p_resoLfoDepth, b->p_resoLfoDepth, t);
    out->p_ampLfoRate = lerpf(a->p_ampLfoRate, b->p_ampLfoRate, t);
    out->p_ampLfoDepth = lerpf(a->p_ampLfoDepth, b->p_ampLfoDepth, t);
    out->p_pitchLfoRate = lerpf(a->p_pitchLfoRate, b->p_pitchLfoRate, t);
    out->p_pitchLfoDepth = lerpf(a->p_pitchLfoDepth, b->p_pitchLfoDepth, t);
    out->p_glideTime = lerpf(a->p_glideTime, b->p_glideTime, t);
    out->p_pluckBrightness = lerpf(a->p_pluckBrightness, b->p_pluckBrightness, t);
    out->p_pluckDamping = lerpf(a->p_pluckDamping, b->p_pluckDamping, t);
    out->p_pluckDamp = lerpf(a->p_pluckDamp, b->p_pluckDamp, t);
    out->p_additiveBrightness = lerpf(a->p_additiveBrightness, b->p_additiveBrightness, t);
    out->p_additiveShimmer = lerpf(a->p_additiveShimmer, b->p_additiveShimmer, t);
    out->p_additiveInharmonicity = lerpf(a->p_additiveInharmonicity, b->p_additiveInharmonicity, t);
    out->p_malletStiffness = lerpf(a->p_malletStiffness, b->p_malletStiffness, t);
    out->p_malletHardness = lerpf(a->p_malletHardness, b->p_malletHardness, t);
    out->p_malletStrikePos = lerpf(a->p_malletStrikePos, b->p_malletStrikePos, t);
    out->p_malletResonance = lerpf(a->p_malletResonance, b->p_malletResonance, t);
    out->p_malletTremolo = lerpf(a->p_malletTremolo, b->p_malletTremolo, t);
    out->p_malletTremoloRate = lerpf(a->p_malletTremoloRate, b->p_malletTremoloRate, t);
    out->p_malletDamp = lerpf(a->p_malletDamp, b->p_malletDamp, t);
    out->p_voiceFormantShift = lerpf(a->p_voiceFormantShift, b->p_voiceFormantShift, t);
    out->p_voiceBreathiness = lerpf(a->p_voiceBreathiness, b->p_voiceBreathiness, t);
    out->p_voiceBuzziness = lerpf(a->p_voiceBuzziness, b->p_voiceBuzziness, t);
    out->p_voiceSpeed = lerpf(a->p_voiceSpeed, b->p_voiceSpeed, t);
    out->p_voicePitch = lerpf(a->p_voicePitch, b->p_voicePitch, t);
    out->p_voiceConsonantAmt = lerpf(a->p_voiceConsonantAmt, b->p_voiceConsonantAmt, t);
    out->p_voiceNasalAmt = lerpf(a->p_voiceNasalAmt, b->p_voiceNasalAmt, t);
    out->p_voicePitchEnv = lerpf(a->p_voicePitchEnv, b->p_voicePitchEnv, t);
    out->p_voicePitchEnvTime = lerpf(a->p_voicePitchEnvTime, b->p_voicePitchEnvTime, t);
    out->p_voicePitchEnvCurve = lerpf(a->p_voicePitchEnvCurve, b->p_voicePitchEnvCurve, t);
    out->p_granularGrainSize = lerpf(a->p_granularGrainSize, b->p_granularGrainSize, t);
    out->p_granularDensity = lerpf(a->p_granularDensity, b->p_granularDensity, t);
    out->p_granularPosition = lerpf(a->p_granularPosition, b->p_granularPosition, t);
    out->p_granularPosRandom = lerpf(a->p_granularPosRandom, b->p_granularPosRandom, t);
    out->p_granularPitch = lerpf(a->p_granularPitch, b->p_granularPitch, t);
    out->p_granularPitchRandom = lerpf(a->p_granularPitchRandom, b->p_granularPitchRandom, t);
    out->p_granularAmpRandom = lerpf(a->p_granularAmpRandom, b->p_granularAmpRandom, t);
    out->p_granularSpread = lerpf(a->p_granularSpread, b->p_granularSpread, t);
    out->p_fmModRatio = lerpf(a->p_fmModRatio, b->p_fmModRatio, t);
    out->p_fmModIndex = lerpf(a->p_fmModIndex, b->p_fmModIndex, t);
    out->p_fmFeedback = lerpf(a->p_fmFeedback, b->p_fmFeedback, t);
    out->p_pdDistortion = lerpf(a->p_pdDistortion, b->p_pdDistortion, t);
    out->p_membraneDamping = lerpf(a->p_membraneDamping, b->p_membraneDamping, t);
    out->p_membraneStrike = lerpf(a->p_membraneStrike, b->p_membraneStrike, t);
    out->p_membraneBend = lerpf(a->p_membraneBend, b->p_membraneBend, t);
    out->p_membraneBendDecay = lerpf(a->p_membraneBendDecay, b->p_membraneBendDecay, t);
    out->p_birdChirpRange = lerpf(a->p_birdChirpRange, b->p_birdChirpRange, t);
    out->p_birdTrillRate = lerpf(a->p_birdTrillRate, b->p_birdTrillRate, t);
    out->p_birdTrillDepth = lerpf(a->p_birdTrillDepth, b->p_birdTrillDepth, t);
    out->p_birdAmRate = lerpf(a->p_birdAmRate, b->p_birdAmRate, t);
    out->p_birdAmDepth = lerpf(a->p_birdAmDepth, b->p_birdAmDepth, t);
    out->p_birdHarmonics = lerpf(a->p_birdHarmonics, b->p_birdHarmonics, t);
}

// Blend two DrumParams structs
static void blendDrumParams(DrumParams *out, const DrumParams *a, const DrumParams *b, float t) {
    out->kickPitch = lerpf(a->kickPitch, b->kickPitch, t);
    out->kickDecay = lerpf(a->kickDecay, b->kickDecay, t);
    out->kickPunchPitch = lerpf(a->kickPunchPitch, b->kickPunchPitch, t);
    out->kickPunchDecay = lerpf(a->kickPunchDecay, b->kickPunchDecay, t);
    out->kickClick = lerpf(a->kickClick, b->kickClick, t);
    out->kickTone = lerpf(a->kickTone, b->kickTone, t);
    out->snarePitch = lerpf(a->snarePitch, b->snarePitch, t);
    out->snareDecay = lerpf(a->snareDecay, b->snareDecay, t);
    out->snareSnappy = lerpf(a->snareSnappy, b->snareSnappy, t);
    out->snareTone = lerpf(a->snareTone, b->snareTone, t);
    out->clapDecay = lerpf(a->clapDecay, b->clapDecay, t);
    out->clapTone = lerpf(a->clapTone, b->clapTone, t);
    out->clapSpread = lerpf(a->clapSpread, b->clapSpread, t);
    out->hhDecayClosed = lerpf(a->hhDecayClosed, b->hhDecayClosed, t);
    out->hhDecayOpen = lerpf(a->hhDecayOpen, b->hhDecayOpen, t);
    out->hhTone = lerpf(a->hhTone, b->hhTone, t);
    out->tomPitch = lerpf(a->tomPitch, b->tomPitch, t);
    out->tomDecay = lerpf(a->tomDecay, b->tomDecay, t);
    out->tomPunchDecay = lerpf(a->tomPunchDecay, b->tomPunchDecay, t);
    out->rimPitch = lerpf(a->rimPitch, b->rimPitch, t);
    out->rimDecay = lerpf(a->rimDecay, b->rimDecay, t);
    out->cowbellPitch = lerpf(a->cowbellPitch, b->cowbellPitch, t);
    out->cowbellDecay = lerpf(a->cowbellDecay, b->cowbellDecay, t);
    out->clavePitch = lerpf(a->clavePitch, b->clavePitch, t);
    out->claveDecay = lerpf(a->claveDecay, b->claveDecay, t);
    out->maracasDecay = lerpf(a->maracasDecay, b->maracasDecay, t);
    out->maracasTone = lerpf(a->maracasTone, b->maracasTone, t);
}

// Blend two Effects structs (only user parameters, not internal state)
static void blendEffects(Effects *out, const Effects *a, const Effects *b, float t) {
    // Booleans switch at 50%
    out->distEnabled = switchBool(a->distEnabled, b->distEnabled, t);
    out->delayEnabled = switchBool(a->delayEnabled, b->delayEnabled, t);
    out->tapeEnabled = switchBool(a->tapeEnabled, b->tapeEnabled, t);
    out->crushEnabled = switchBool(a->crushEnabled, b->crushEnabled, t);
    out->reverbEnabled = switchBool(a->reverbEnabled, b->reverbEnabled, t);
    
    // Continuous params
    out->distDrive = lerpf(a->distDrive, b->distDrive, t);
    out->distTone = lerpf(a->distTone, b->distTone, t);
    out->distMix = lerpf(a->distMix, b->distMix, t);
    out->delayTime = lerpf(a->delayTime, b->delayTime, t);
    out->delayFeedback = lerpf(a->delayFeedback, b->delayFeedback, t);
    out->delayMix = lerpf(a->delayMix, b->delayMix, t);
    out->delayTone = lerpf(a->delayTone, b->delayTone, t);
    out->tapeWow = lerpf(a->tapeWow, b->tapeWow, t);
    out->tapeFlutter = lerpf(a->tapeFlutter, b->tapeFlutter, t);
    out->tapeSaturation = lerpf(a->tapeSaturation, b->tapeSaturation, t);
    out->tapeHiss = lerpf(a->tapeHiss, b->tapeHiss, t);
    out->crushBits = lerpf(a->crushBits, b->crushBits, t);
    out->crushRate = lerpf(a->crushRate, b->crushRate, t);
    out->crushMix = lerpf(a->crushMix, b->crushMix, t);
    out->reverbSize = lerpf(a->reverbSize, b->reverbSize, t);
    out->reverbDamping = lerpf(a->reverbDamping, b->reverbDamping, t);
    out->reverbMix = lerpf(a->reverbMix, b->reverbMix, t);
    out->reverbPreDelay = lerpf(a->reverbPreDelay, b->reverbPreDelay, t);
    // Note: internal state fields (filter states, phases, counters) are NOT blended
}

// Apply crossfader blend to global state
static void updateCrossfaderBlend(void) {
    if (!crossfaderEnabled) return;
    
    Scene *a = &scenes[crossfader.sceneA];
    Scene *b = &scenes[crossfader.sceneB];
    
    // Both scenes must be initialized
    if (!a->initialized || !b->initialized) return;
    
    float t = crossfader.position;
    
    // Blend all patches
    for (int i = 0; i < NUM_PATCHES; i++) {
        blendSynthPatch(&patches[i], &a->patches[i], &b->patches[i], t);
    }
    
    // Blend drums
    blendDrumParams(&drumParams, &a->drums, &b->drums, t);
    
    // Blend effects (preserve internal state)
    Effects blendedFx;
    blendEffects(&blendedFx, &a->effects, &b->effects, t);
    // Copy only user params, keep internal state
    fx.distEnabled = blendedFx.distEnabled;
    fx.distDrive = blendedFx.distDrive;
    fx.distTone = blendedFx.distTone;
    fx.distMix = blendedFx.distMix;
    fx.delayEnabled = blendedFx.delayEnabled;
    fx.delayTime = blendedFx.delayTime;
    fx.delayFeedback = blendedFx.delayFeedback;
    fx.delayMix = blendedFx.delayMix;
    fx.delayTone = blendedFx.delayTone;
    fx.tapeEnabled = blendedFx.tapeEnabled;
    fx.tapeWow = blendedFx.tapeWow;
    fx.tapeFlutter = blendedFx.tapeFlutter;
    fx.tapeSaturation = blendedFx.tapeSaturation;
    fx.tapeHiss = blendedFx.tapeHiss;
    fx.crushEnabled = blendedFx.crushEnabled;
    fx.crushBits = blendedFx.crushBits;
    fx.crushRate = blendedFx.crushRate;
    fx.crushMix = blendedFx.crushMix;
    fx.reverbEnabled = blendedFx.reverbEnabled;
    fx.reverbSize = blendedFx.reverbSize;
    fx.reverbDamping = blendedFx.reverbDamping;
    fx.reverbMix = blendedFx.reverbMix;
    fx.reverbPreDelay = blendedFx.reverbPreDelay;
    
    // Blend volumes
    masterVolume = lerpf(a->masterVol, b->masterVol, t);
    drumVolume = lerpf(a->drumVol, b->drumVol, t);
}

// Initialize patches with different default wave types
static void initPatches(void) {
    patches[PATCH_PREVIEW] = createDefaultPatch(WAVE_SAW);  // Scratchpad for jamming
    patches[PATCH_BASS] = createDefaultPatch(WAVE_SAW);
    patches[PATCH_BASS].p_filterCutoff = 0.4f;  // Bass: darker
    patches[PATCH_BASS].p_release = 0.15f;
    patches[PATCH_LEAD] = createDefaultPatch(WAVE_SQUARE);
    patches[PATCH_LEAD].p_filterCutoff = 0.8f;
    patches[PATCH_LEAD].p_vibratoDepth = 0.2f;
    patches[PATCH_CHORD] = createDefaultPatch(WAVE_TRIANGLE);
    patches[PATCH_CHORD].p_attack = 0.05f;
    patches[PATCH_CHORD].p_release = 0.5f;
}

// Copy one patch to another
static void copyPatch(SynthPatch *src, SynthPatch *dst) {
    *dst = *src;
}

// Apply a patch's settings to the global synth parameters
static void applyPatchToGlobals(SynthPatch *p) {
    noteAttack = p->p_attack;
    noteDecay = p->p_decay;
    noteSustain = p->p_sustain;
    noteRelease = p->p_release;
    noteVolume = p->p_volume;
    notePulseWidth = p->p_pulseWidth;
    notePwmRate = p->p_pwmRate;
    notePwmDepth = p->p_pwmDepth;
    noteVibratoRate = p->p_vibratoRate;
    noteVibratoDepth = p->p_vibratoDepth;
    noteFilterCutoff = p->p_filterCutoff;
    noteFilterResonance = p->p_filterResonance;
    noteFilterEnvAmt = p->p_filterEnvAmt;
    noteFilterEnvAttack = p->p_filterEnvAttack;
    noteFilterEnvDecay = p->p_filterEnvDecay;
    noteFilterLfoRate = p->p_filterLfoRate;
    noteFilterLfoDepth = p->p_filterLfoDepth;
    noteFilterLfoShape = p->p_filterLfoShape;
    noteResoLfoRate = p->p_resoLfoRate;
    noteResoLfoDepth = p->p_resoLfoDepth;
    noteResoLfoShape = p->p_resoLfoShape;
    noteAmpLfoRate = p->p_ampLfoRate;
    noteAmpLfoDepth = p->p_ampLfoDepth;
    noteAmpLfoShape = p->p_ampLfoShape;
    notePitchLfoRate = p->p_pitchLfoRate;
    notePitchLfoDepth = p->p_pitchLfoDepth;
    notePitchLfoShape = p->p_pitchLfoShape;
    noteScwIndex = p->p_scwIndex;
    monoMode = p->p_monoMode;
    glideTime = p->p_glideTime;
    pluckBrightness = p->p_pluckBrightness;
    pluckDamping = p->p_pluckDamping;
    pluckDamp = p->p_pluckDamp;
    additivePreset = p->p_additivePreset;
    additiveBrightness = p->p_additiveBrightness;
    additiveShimmer = p->p_additiveShimmer;
    additiveInharmonicity = p->p_additiveInharmonicity;
    malletPreset = p->p_malletPreset;
    malletStiffness = p->p_malletStiffness;
    malletHardness = p->p_malletHardness;
    malletStrikePos = p->p_malletStrikePos;
    malletResonance = p->p_malletResonance;
    malletTremolo = p->p_malletTremolo;
    malletTremoloRate = p->p_malletTremoloRate;
    malletDamp = p->p_malletDamp;
    voiceVowel = p->p_voiceVowel;
    voiceFormantShift = p->p_voiceFormantShift;
    voiceBreathiness = p->p_voiceBreathiness;
    voiceBuzziness = p->p_voiceBuzziness;
    voiceSpeed = p->p_voiceSpeed;
    voicePitch = p->p_voicePitch;
    voiceConsonant = p->p_voiceConsonant;
    voiceConsonantAmt = p->p_voiceConsonantAmt;
    voiceNasal = p->p_voiceNasal;
    voiceNasalAmt = p->p_voiceNasalAmt;
    voicePitchEnv = p->p_voicePitchEnv;
    voicePitchEnvTime = p->p_voicePitchEnvTime;
    voicePitchEnvCurve = p->p_voicePitchEnvCurve;
    granularScwIndex = p->p_granularScwIndex;
    granularGrainSize = p->p_granularGrainSize;
    granularDensity = p->p_granularDensity;
    granularPosition = p->p_granularPosition;
    granularPosRandom = p->p_granularPosRandom;
    granularPitch = p->p_granularPitch;
    granularPitchRandom = p->p_granularPitchRandom;
    granularAmpRandom = p->p_granularAmpRandom;
    granularSpread = p->p_granularSpread;
    granularFreeze = p->p_granularFreeze;
    fmModRatio = p->p_fmModRatio;
    fmModIndex = p->p_fmModIndex;
    fmFeedback = p->p_fmFeedback;
    pdWaveType = p->p_pdWaveType;
    pdDistortion = p->p_pdDistortion;
    membranePreset = p->p_membranePreset;
    membraneDamping = p->p_membraneDamping;
    membraneStrike = p->p_membraneStrike;
    membraneBend = p->p_membraneBend;
    membraneBendDecay = p->p_membraneBendDecay;
    birdType = p->p_birdType;
    birdChirpRange = p->p_birdChirpRange;
    birdTrillRate = p->p_birdTrillRate;
    birdTrillDepth = p->p_birdTrillDepth;
    birdAmRate = p->p_birdAmRate;
    birdAmDepth = p->p_birdAmDepth;
    birdHarmonics = p->p_birdHarmonics;
}

// ============================================================================
// MELODIC SEQUENCER VOICES
// ============================================================================

// melodyVoiceIdx declared earlier (near audio callback) for sidechain routing

// Convert MIDI note to frequency
static float midiNoteToFreq(int note) {
    return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}

// Play a note using a specific patch's settings
static int playNoteWithPatch(float freq, SynthPatch *p) {
    // Set globals from patch (play functions in synth.h read these)
    applyPatchToGlobals(p);
    
    WaveType wave = (WaveType)p->p_waveType;
    
    switch (wave) {
        case WAVE_PLUCK:    return playPluck(freq, p->p_pluckBrightness, p->p_pluckDamping);
        case WAVE_ADDITIVE: return playAdditive(freq, (AdditivePreset)p->p_additivePreset);
        case WAVE_MALLET:   return playMallet(freq, (MalletPreset)p->p_malletPreset);
        case WAVE_VOICE:    return playVowel(freq, (VowelType)p->p_voiceVowel);
        case WAVE_GRANULAR: return playGranular(freq, p->p_granularScwIndex);
        case WAVE_FM:       return playFM(freq);
        case WAVE_PD:       return playPD(freq);
        case WAVE_MEMBRANE: return playMembrane(freq, (MembranePreset)p->p_membranePreset);
        case WAVE_BIRD:     return playBird(freq, (BirdType)p->p_birdType);
        default:            return playNote(freq, wave);
    }
}

// ============================================================================
// DRUM SEQUENCER TRIGGERS (with P-lock support)
// ============================================================================

// Generic drum trigger with P-lock support
// drumType: DRUM_KICK, DRUM_SNARE, etc.
static void drumTriggerWithPLocks(DrumType drumType, float vel, float pitch) {
    // Apply P-lock volume
    float effectiveVel = plockValue(PLOCK_VOLUME, vel);
    
    // Apply P-lock pitch offset (in semitones, added to existing pitch)
    float pitchOffset = plockValue(PLOCK_PITCH_OFFSET, 0.0f);
    float effectivePitch = pitch * powf(2.0f, pitchOffset / 12.0f);
    
    // Trigger the drum first (this resets plock fields to -1)
    triggerDrumFull(drumType, effectiveVel, effectivePitch);
    
    // Now set P-lock overrides on the voice (these persist for the duration of the sound)
    DrumVoice *dv = &drumVoices[drumType];
    
    float pDecay = plockValue(PLOCK_DECAY, -1.0f);
    if (pDecay >= 0.0f) {
        dv->plockDecay = pDecay;
    }
    
    float pTone = plockValue(PLOCK_TONE, -1.0f);
    if (pTone >= 0.0f) {
        dv->plockTone = pTone;
    }
    
    float pPunch = plockValue(PLOCK_PUNCH, -1.0f);
    if (pPunch >= 0.0f) {
        dv->plockPunch = pPunch;
    }
}

// ============================================================================
// DRUM KIT ASSIGNMENT
// ============================================================================

// Names for all drum types (for UI display)
static const char* drumTypeNames[DRUM_COUNT] = {
    "808 Kick", "808 Snare", "808 Clap", "808 CH", "808 OH",
    "808 LTom", "808 MTom", "808 HTom", "Rimshot", "Cowbell",
    "Clave", "Maracas",
    "CR78 Kick", "CR78 Snare", "CR78 HH", "CR78 Metal"
};

// Short names for track labels
static const char* drumTypeShortNames[DRUM_COUNT] = {
    "Kick", "Snare", "Clap", "CH", "OH",
    "LTom", "MTom", "HTom", "Rim", "Bell",
    "Clave", "Shaker",
    "78Kick", "78Snr", "78HH", "78Met"
};

// drumTrackSound is declared earlier in file (before audio callback)

// Generic sequencer drum callback - looks up which sound to play
static void seqDrumTrack0(float vel, float pitch) { drumTriggerWithPLocks(drumTrackSound[0], vel, pitch); }
static void seqDrumTrack1(float vel, float pitch) { drumTriggerWithPLocks(drumTrackSound[1], vel, pitch); }
static void seqDrumTrack2(float vel, float pitch) { drumTriggerWithPLocks(drumTrackSound[2], vel, pitch); }
static void seqDrumTrack3(float vel, float pitch) { drumTriggerWithPLocks(drumTrackSound[3], vel, pitch); }

// Update track name when sound changes
static void updateDrumTrackName(int track) {
    if (track >= 0 && track < SEQ_DRUM_TRACKS) {
        seq.drumTrackNames[track] = drumTypeShortNames[drumTrackSound[track]];
    }
}

// Cycle to next drum sound for a track
static void cycleDrumTrackSound(int track, int direction) {
    if (track < 0 || track >= SEQ_DRUM_TRACKS) return;
    int current = (int)drumTrackSound[track];
    current += direction;
    if (current < 0) current = DRUM_COUNT - 1;
    if (current >= DRUM_COUNT) current = 0;
    drumTrackSound[track] = (DrumType)current;
    updateDrumTrackName(track);
}

// ============================================================================
// MELODIC SEQUENCER TRIGGERS
// ============================================================================

// Unified melodic trigger with 303-style slide and accent support
// trackIdx: 0=Bass, 1=Lead, 2=Chord
// patchIdx: PATCH_BASS, PATCH_LEAD, PATCH_CHORD
// freqMult: octave multiplier (0.5 for bass, 1.0 for others)
static void melodyTriggerGeneric(int trackIdx, int patchIdx, float freqMult,
                                  int note, float vel, bool slide, bool accent) {
    float freq = midiNoteToFreq(note);
    
    // Apply p-lock pitch offset (in semitones)
    float pitchOffset = plockValue(PLOCK_PITCH_OFFSET, 0.0f);
    if (pitchOffset != 0.0f) {
        freq *= powf(2.0f, pitchOffset / 12.0f);
    }
    freq *= freqMult;
    
    // Apply accent: boost velocity and filter envelope
    float effectiveVel = plockValue(PLOCK_VOLUME, vel);
    float accentFilterBoost = accent ? 0.3f : 0.0f;
    if (accent) effectiveVel = fminf(effectiveVel * 1.3f, 1.0f);
    
    // Get p-lock values for filter (use patch values as defaults)
    SynthPatch *p = &patches[patchIdx];
    // PLOCK_TONE acts as alias for cutoff on melody (check tone first, then cutoff)
    float pTone = plockValue(PLOCK_TONE, -1.0f);
    float pCutoff = (pTone >= 0.0f) ? pTone : plockValue(PLOCK_FILTER_CUTOFF, p->p_filterCutoff);
    float pReso = plockValue(PLOCK_FILTER_RESO, p->p_filterResonance);
    float pFilterEnv = plockValue(PLOCK_FILTER_ENV, p->p_filterEnvAmt) + accentFilterBoost;
    float pDecay = plockValue(PLOCK_DECAY, p->p_decay);
    
    // Apply slide: enable glide instead of retriggering
    if (slide && melodyVoiceIdx[trackIdx] >= 0 && synthVoices[melodyVoiceIdx[trackIdx]].envStage > 0) {
        Voice *v = &synthVoices[melodyVoiceIdx[trackIdx]];
        v->targetFrequency = freq;
        v->glideRate = 1.0f / 0.06f;  // Fast 303-style glide (~60ms)
        v->volume = effectiveVel * p->p_volume;
        v->filterCutoff = pCutoff;
        v->filterResonance = pReso;
        v->filterEnvAmt = pFilterEnv;
        v->decay = pDecay;
        if (accent || currentPLocks.locked[PLOCK_FILTER_ENV]) {
            v->filterEnvLevel = 1.0f;
            v->filterEnvStage = 2;
            v->filterEnvPhase = 0.0f;
        }
    } else {
        // New note - release previous and play new
        if (melodyVoiceIdx[trackIdx] >= 0) releaseNote(melodyVoiceIdx[trackIdx]);
        
        // Temporarily apply p-lock values to patch
        float origCutoff = p->p_filterCutoff, origReso = p->p_filterResonance;
        float origFilterEnvAmt = p->p_filterEnvAmt, origDecay = p->p_decay;
        
        p->p_filterCutoff = pCutoff;
        p->p_filterResonance = pReso;
        p->p_filterEnvAmt = pFilterEnv;
        p->p_decay = pDecay;
        p->p_volume = effectiveVel * 0.5f;
        
        melodyVoiceIdx[trackIdx] = playNoteWithPatch(freq, p);
        
        // Restore original patch values
        p->p_filterCutoff = origCutoff;
        p->p_filterResonance = origReso;
        p->p_filterEnvAmt = origFilterEnvAmt;
        p->p_decay = origDecay;
        p->p_volume = 0.5f;
    }
}

// Release helper
static void melodyReleaseGeneric(int trackIdx) {
    if (melodyVoiceIdx[trackIdx] >= 0) {
        releaseNote(melodyVoiceIdx[trackIdx]);
        melodyVoiceIdx[trackIdx] = -1;
    }
}

// Thin wrappers for sequencer callbacks
static void melodyTriggerBass(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime;
    melodyTriggerGeneric(0, PATCH_BASS, 0.5f, note, vel, slide, accent);
}
static void melodyReleaseBass(void) { melodyReleaseGeneric(0); }

static void melodyTriggerLead(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime;
    melodyTriggerGeneric(1, PATCH_LEAD, 1.0f, note, vel, slide, accent);
}
static void melodyReleaseLead(void) { melodyReleaseGeneric(1); }

static void melodyTriggerChord(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime;
    melodyTriggerGeneric(2, PATCH_CHORD, 1.0f, note, vel, slide, accent);
}
static void melodyReleaseChord(void) { melodyReleaseGeneric(2); }

// ============================================================================
// UI COLUMN VISIBILITY
// ============================================================================

static bool showWaveColumn = true;
static bool showSynthColumn = true;
static bool showLfoColumn = true;
static bool showDrumsColumn = true;
static bool showEffectsColumn = true;
static bool showTapeFxColumn = true;

// Waveform names for UI
static const char* waveNames[] = {"Square", "Saw", "Triangle", "Noise", "SCW", "Voice", "Pluck", "Additive", "Mallet", "Granular", "FM", "PD", "Membrane", "Bird"};
static int selectedWave = 0;

// PD wave type names for UI
static const char* pdWaveNames[] = {"Saw", "Square", "Pulse", "DblPulse", "SawPulse", "Reso1", "Reso2", "Reso3"};

// Membrane preset names for UI
static const char* membranePresetNames[] = {"Tabla", "Conga", "Bongo", "Djembe", "Tom"};

// Bird type names for UI
static const char* birdTypeNames[] = {"Chirp", "Trill", "Warble", "Tweet", "Whistle", "Cuckoo"};

// Additive preset names for UI
static const char* additivePresetNames[] = {"Sine", "Organ", "Bell", "Strings", "Brass", "Choir", "Custom"};

// Mallet preset names for UI
static const char* malletPresetNames[] = {"Marimba", "Vibes", "Xylo", "Glock", "Tubular"};

// Vowel names for UI
static const char* vowelNames[] = {"A (ah)", "E (eh)", "I (ee)", "O (oh)", "U (oo)"};

// Voice key tracking
static int vowelKeyVoice = -1;

// Random vowel mode for sung melodies
static bool voiceRandomVowel = false;

int main(void) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "PixelSynth Demo");
    
    Font font = LoadEmbeddedFont();
    ui_init(&font);
    
    SetAudioStreamBufferSizeDefault(MAX_SAMPLES_PER_UPDATE);
    InitAudioDevice();
    
    // Load embedded SCW wavetables (generated by `make scw_embed`)
    loadEmbeddedSCWs();
    
    // Create audio stream
    AudioStream stream = LoadAudioStream(SAMPLE_RATE, 16, 1);
    SetAudioStreamCallback(stream, SynthCallback);
    PlayAudioStream(stream);
    
    // Initialize engines
    memset(synthVoices, 0, sizeof(synthCtx->voices));
    memset(drumVoices, 0, sizeof(drumVoices));
    initDrumParams();
    initEffects();
    initPatches();
    initSequencer(seqDrumTrack0, seqDrumTrack1, seqDrumTrack2, seqDrumTrack3);
    
    // Update track names to match initial drum assignments
    for (int i = 0; i < SEQ_DRUM_TRACKS; i++) {
        updateDrumTrackName(i);
    }
    
    // Set up melodic track callbacks
    setMelodyCallbacks(0, melodyTriggerBass, melodyReleaseBass);
    setMelodyCallbacks(1, melodyTriggerLead, melodyReleaseLead);
    setMelodyCallbacks(2, melodyTriggerChord, melodyReleaseChord);
    
    SetTargetFPS(60);
    
    // Initialize piano key voices
    for (size_t i = 0; i < NUM_PIANO_KEYS; i++) {
        pianoKeyVoices[i] = -1;
    }
    
    while (!WindowShouldClose()) {
        // SFX (1-6)
        if (IsKeyPressed(KEY_ONE))   sfxJump();
        if (IsKeyPressed(KEY_TWO))   sfxCoin();
        if (IsKeyPressed(KEY_THREE)) sfxHurt();
        if (IsKeyPressed(KEY_FOUR))  sfxExplosion();
        if (IsKeyPressed(KEY_FIVE))  sfxPowerup();
        if (IsKeyPressed(KEY_SIX))   sfxBlip();
        
        // Drums (7-0, -, = or numpad)
        if (IsKeyPressed(KEY_SEVEN) || IsKeyPressed(KEY_KP_7)) drumKick();
        if (IsKeyPressed(KEY_EIGHT) || IsKeyPressed(KEY_KP_8)) drumSnare();
        if (IsKeyPressed(KEY_NINE)  || IsKeyPressed(KEY_KP_9)) drumClap();
        if (IsKeyPressed(KEY_ZERO)  || IsKeyPressed(KEY_KP_0)) drumClosedHH();
        if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) drumOpenHH();
        if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) drumLowTom();
        // Additional drums (numpad only)
        if (IsKeyPressed(KEY_KP_4)) drumMidTom();
        if (IsKeyPressed(KEY_KP_1)) drumHiTom();
        if (IsKeyPressed(KEY_KP_5)) drumRimshot();
        if (IsKeyPressed(KEY_KP_6)) drumCowbell();
        if (IsKeyPressed(KEY_KP_2)) drumClave();
        if (IsKeyPressed(KEY_KP_3)) drumMaracas();
        
        // Octave control (Z/X)
        if (IsKeyPressed(KEY_Z) && currentOctave > MIN_OCTAVE) {
            currentOctave--;
        }
        if (IsKeyPressed(KEY_X) && currentOctave < MAX_OCTAVE) {
            currentOctave++;
        }
        
        // Voice/Speech (bottom row: V B N , .)
        if (IsKeyPressed(KEY_V)) {
            vowelKeyVoice = playVowel(200.0f * voicePitch, (VowelType)voiceVowel);
        }
        if (IsKeyReleased(KEY_V) && vowelKeyVoice >= 0) {
            releaseNote(vowelKeyVoice);
            vowelKeyVoice = -1;
        }
        if (IsKeyPressed(KEY_B)) babble(2.0f, voicePitch, 0.5f);
        if (IsKeyPressed(KEY_N)) speak("hello world", voiceSpeed, voicePitch, 0.3f);
        if (IsKeyPressed(KEY_COMMA)) babbleCall(1.5f, voicePitch, 0.5f);    // Call (rising)
        if (IsKeyPressed(KEY_PERIOD)) babbleAnswer(1.5f, voicePitch, 0.5f); // Answer (falling)
        
        // Update speech
        updateSpeech(GetFrameTime());
        
        // Sequencer play/stop
        if (IsKeyPressed(KEY_SPACE)) {
            if (seq.playing) {
                stopSequencer();  // Stop and release all notes
            } else {
                resetSequencer();
                seq.playing = true;
            }
        }
        updateSequencer(GetFrameTime());
        
        // Apply crossfader blending if enabled
        updateCrossfaderBlend();
        
        // Piano keyboard input (ASDFGHJKL = white keys, WERTYUIOP = black keys)
        // Uses the currently selected patch settings
        SynthPatch *keyboardPatch = &patches[selectedPatch];
        for (size_t i = 0; i < NUM_PIANO_KEYS; i++) {
            if (IsKeyPressed(pianoKeys[i].key)) {
                float freq = semitoneToFreq(pianoKeys[i].semitone, currentOctave);
                // Handle voice random vowel mode specially
                if (selectedWave == WAVE_VOICE && voiceRandomVowel) {
                    synthNoiseState = synthNoiseState * 1103515245 + 12345;
                    int savedVowel = keyboardPatch->p_voiceVowel;
                    keyboardPatch->p_voiceVowel = (synthNoiseState >> 16) % 5;
                    pianoKeyVoices[i] = playNoteWithPatch(freq, keyboardPatch);
                    keyboardPatch->p_voiceVowel = savedVowel;
                } else {
                    pianoKeyVoices[i] = playNoteWithPatch(freq, keyboardPatch);
                }
            }
            if (IsKeyReleased(pianoKeys[i].key) && pianoKeyVoices[i] >= 0) {
                // Pluck and Mallet can ring out or be damped based on damp setting
                if (selectedWave == WAVE_PLUCK && keyboardPatch->p_pluckDamp > 0.01f) {
                    synthVoices[pianoKeyVoices[i]].release = 0.01f + (1.0f - keyboardPatch->p_pluckDamp) * 0.5f;
                    releaseNote(pianoKeyVoices[i]);
                } else if (selectedWave == WAVE_MALLET && keyboardPatch->p_malletDamp > 0.01f) {
                    synthVoices[pianoKeyVoices[i]].release = 0.01f + (1.0f - keyboardPatch->p_malletDamp) * 0.5f;
                    releaseNote(pianoKeyVoices[i]);
                } else if (selectedWave != WAVE_PLUCK && selectedWave != WAVE_MALLET) {
                    releaseNote(pianoKeyVoices[i]);
                }
                pianoKeyVoices[i] = -1;
            }
        }
        
        BeginDrawing();
        ClearBackground(DARKGRAY);
        ui_begin_frame();
        
        DrawTextEx(font, "PixelSynth Demo", (Vector2){20, 20}, 30, 1, WHITE);
        
        // === TOP BAR: Transport, BPM, Groove, Scenes, Help ===
        {
            static bool grooveExpanded = false;
            static bool scenesExpanded = false;
            static bool polyExpanded = false;
            Vector2 mouse = GetMousePosition();
            
            int topBarY = 25;
            float x = 300;  // Start after "PixelSynth Demo" title
            
            // [?] Help button
            {
                const char* helpLabel = "[?]";
                int w = MeasureText(helpLabel, 18);
                Rectangle helpRect = {x, (float)topBarY, (float)w, 20};
                bool helpHovered = CheckCollisionPointRec(mouse, helpRect);
                DrawTextShadow(helpLabel, (int)x, topBarY, 18, helpHovered ? YELLOW : LIGHTGRAY);
                g_showHelpOverlay = helpHovered;  // Set global for overlay drawing at end of frame
                x += w + 10;
            }
            
            // [Play] / [Stop] button
            if (PushButton(x, topBarY, seq.playing ? "Stop" : "Play")) {
                if (seq.playing) {
                    stopSequencer();
                } else {
                    resetSequencer();
                    seq.playing = true;
                }
            }
            x += MeasureText(seq.playing ? "[Stop]" : "[Play]", 18) + 15;
            
            // BPM
            DraggableFloat(x, topBarY, "BPM", &seq.bpm, 2.0f, 60.0f, 200.0f);
            
            // Fixed position for Groove/Scenes/Poly
            x = 560;
            
            // [+] Groove section header (same style as columns)
            if (SectionHeader(x, topBarY, "Groove", &grooveExpanded)) {
                int gx = 330;  // Start position for expanded settings
                int gy = topBarY + 20;  // Below the top bar
                DraggableInt(gx, gy, "Kick", &seq.dilla.kickNudge, 0.3f, -12, 12);
                DraggableInt(gx + 95, gy, "Snare", &seq.dilla.snareDelay, 0.3f, -12, 12);
                DraggableInt(gx + 185, gy, "HH", &seq.dilla.hatNudge, 0.3f, -12, 12);
                DraggableInt(gx + 250, gy, "Clap", &seq.dilla.clapDelay, 0.3f, -12, 12);
                DraggableInt(gx + 325, gy, "Swing", &seq.dilla.swing, 0.3f, 0, 12);
                DraggableInt(gx + 415, gy, "Jitter", &seq.dilla.jitter, 0.3f, 0, 6);
            }
            x += MeasureText("[+] Groove", 18) + 15;
            
            // [+] Scenes section header
            if (SectionHeader(x, topBarY, "Scenes", &scenesExpanded)) {
                int sx = 330;
                int sy = topBarY + 20;
                if (grooveExpanded) sy += 20;  // Move down if Groove is also expanded
                
                bool mouseClicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
                bool rightClicked = IsMouseButtonPressed(MOUSE_RIGHT_BUTTON);
                bool shiftHeld = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
                
                // Scene buttons 1-8 using PushButton style
                for (int i = 0; i < NUM_SCENES; i++) {
                    bool isCurrent = (i == currentScene);
                    bool hasContent = scenes[i].initialized;
                    
                    const char* label = TextFormat("%d", i + 1);
                    int btnW = MeasureText(TextFormat("[%s]", label), 18);
                    Rectangle btnRect = {(float)sx, (float)sy, (float)btnW, 20};
                    bool isHovered = CheckCollisionPointRec(mouse, btnRect);
                    
                    // Draw button with clearer state-dependent colors
                    Color textColor = GRAY;  // Empty slot
                    if (isCurrent) textColor = GREEN;  // Currently active scene
                    else if (hasContent) textColor = (Color){100, 150, 255, 255};  // Has content (blue)
                    if (isHovered) textColor = YELLOW;
                    
                    DrawTextShadow(TextFormat("[%s]", label), sx, sy, 18, textColor);
                    
                    if (isHovered) {
                        if (mouseClicked) {
                            if (shiftHeld) {
                                saveScene(i);  // Shift+Click = Save
                            } else if (hasContent) {
                                loadScene(i);  // Click = Load (if has content)
                            }
                            ui_consume_click();
                        }
                        if (rightClicked) {
                            clearScene(i);  // Right-click = Clear
                            ui_consume_click();
                        }
                    }
                    sx += btnW + 10;  // More spacing between buttons
                }
                
                sx += 15;
                
                // Save button
                if (PushButton(sx, sy, "Save")) {
                    if (currentScene >= 0) {
                        saveScene(currentScene);
                    } else {
                        int slot = 0;
                        for (int i = 0; i < NUM_SCENES; i++) {
                            if (!scenes[i].initialized) { slot = i; break; }
                        }
                        saveScene(slot);
                    }
                }
                sx += MeasureText("[Save]", 18) + 20;
                
                // XFade toggle and controls: [X] A:1 XFade: 0.50 B:2
                ToggleBool(sx, sy, "X", &crossfaderEnabled);
                sx += 40;
                
                DrawTextShadow(TextFormat("A:%d", crossfader.sceneA + 1), sx, sy + 2, 12, 
                               scenes[crossfader.sceneA].initialized ? (Color){100, 150, 255, 255} : GRAY);
                Rectangle aRect = {(float)sx, (float)sy, 25, 20};
                if (CheckCollisionPointRec(mouse, aRect) && mouseClicked) {
                    crossfader.sceneA = (crossfader.sceneA + 1) % NUM_SCENES;
                    ui_consume_click();
                }
                sx += 30;
                
                DraggableFloat(sx, sy, "XFade", &crossfader.position, 0.02f, 0.0f, 1.0f);
                sx += 110;
                
                DrawTextShadow(TextFormat("B:%d", crossfader.sceneB + 1), sx, sy + 2, 12,
                               scenes[crossfader.sceneB].initialized ? (Color){255, 150, 100, 255} : GRAY);
                Rectangle bRect = {(float)sx, (float)sy, 25, 20};
                if (CheckCollisionPointRec(mouse, bRect) && mouseClicked) {
                    crossfader.sceneB = (crossfader.sceneB + 1) % NUM_SCENES;
                    ui_consume_click();
                }
            }
            x += MeasureText("[-] Scenes", 18) + 15;
            
            // [+] Poly section header (per-track lengths for polyrhythm)
            if (SectionHeader(x, topBarY, "Poly", &polyExpanded)) {
                int px = 330;
                int py = topBarY + 20;
                if (grooveExpanded) py += 20;
                if (scenesExpanded) py += 20;
                
                // Drum track lengths
                const char* drumNames[] = {"Kick", "Snare", "HH", "Clap"};
                for (int i = 0; i < SEQ_DRUM_TRACKS; i++) {
                    DraggableInt(px, py, drumNames[i], &seq.patterns[seq.currentPattern].drumTrackLength[i], 0.3f, 1, SEQ_MAX_STEPS);
                    px += 80;
                }
                
                px += 10;
                
                // Melody track lengths
                const char* melodyNames[] = {"Bass", "Lead", "Chord"};
                for (int i = 0; i < SEQ_MELODY_TRACKS; i++) {
                    DraggableInt(px, py, melodyNames[i], &seq.patterns[seq.currentPattern].melodyTrackLength[i], 0.3f, 1, SEQ_MAX_STEPS);
                    px += 85;
                }
            }
            
            // Audio stats (right side, stacked vertically)
            double bufferTimeMs = (double)audioFrameCount / SAMPLE_RATE * 1000.0;
            double cpuPercent = (audioTimeUs / 1000.0) / bufferTimeMs * 100.0;
            int statsX = SCREEN_WIDTH - 70;
            DrawTextShadow(TextFormat("%.1f%%", cpuPercent), statsX, topBarY, 10, GRAY);
            DrawTextShadow(TextFormat("FPS: %d", GetFPS()), statsX, topBarY + 12, 10, GRAY);
            
            // Voice indicators: synth (2 rows of 8), then drums (2 rows of 8)
            Color voiceOff = {50, 50, 50, 255};
            int voiceY = topBarY + 26;
            for (int i = 0; i < NUM_VOICES; i++) {
                Color c = voiceOff;
                if (synthVoices[i].envStage == 4) c = ORANGE;
                else if (synthVoices[i].envStage > 0) c = GREEN;
                DrawRectangle(statsX + (i % 8) * 7, voiceY + (i / 8) * 8, 5, 6, c);
            }
            for (int i = 0; i < NUM_DRUM_VOICES; i++) {
                Color c = drumVoices[i].active ? ORANGE : voiceOff;
                DrawRectangle(statsX + (i % 8) * 7, voiceY + 20 + (i / 8) * 8, 5, 6, c);
            }
        }
        
        // Speaking indicator
        if (speechQueue.active) {
            DrawTextEx(font, "Speaking...", (Vector2){20, 200}, 14, 1, GREEN);
        }
        
        // === COLUMN 1: Wave Type + Wave-specific settings ===
        UIColumn col1 = ui_column(30, 75, 20);
        
        // Current patch pointer - UI edits patch directly, no globals sync
        SynthPatch *cp = &patches[selectedPatch];
        
        if (SectionHeader(col1.x, col1.y, "Wave", &showWaveColumn)) {
            col1.y += 18;
            
            // Patch selector
            int prevPatch = selectedPatch;
            ui_col_cycle(&col1, "Patch", patchNames, NUM_PATCHES, &selectedPatch);
            
            // When switching patches, update cp pointer and wave selector
            if (selectedPatch != prevPatch) {
                cp = &patches[selectedPatch];
                selectedWave = cp->p_waveType;
            }
            
            // Copy buttons (only show when on Preview)
            if (selectedPatch == PATCH_PREVIEW) {
                if (ui_col_button(&col1, "-> Bass")) {
                    copyPatch(&patches[PATCH_PREVIEW], &patches[PATCH_BASS]);
                }
                if (ui_col_button(&col1, "-> Lead")) {
                    copyPatch(&patches[PATCH_PREVIEW], &patches[PATCH_LEAD]);
                }
                if (ui_col_button(&col1, "-> Chord")) {
                    copyPatch(&patches[PATCH_PREVIEW], &patches[PATCH_CHORD]);
                }
            }
            
            ui_col_space(&col1, 4);
            
            // Track wave type changes back to the patch
            int prevWave = selectedWave;
            ui_col_cycle(&col1, "Type", waveNames, 14, &selectedWave);
            if (selectedWave != prevWave) {
                patches[selectedPatch].p_waveType = selectedWave;
            }
            ui_col_space(&col1, 4);
            
            if (selectedWave == WAVE_SQUARE) {
                ui_col_sublabel(&col1, "PWM:", ORANGE);
                ui_col_float(&col1, "Width", &cp->p_pulseWidth, 0.05f, 0.1f, 0.9f);
                ui_col_float(&col1, "Rate", &cp->p_pwmRate, 0.5f, 0.1f, 20.0f);
                ui_col_float(&col1, "Depth", &cp->p_pwmDepth, 0.02f, 0.0f, 0.4f);
            }
            
            if (selectedWave == WAVE_SCW && scwCount > 0) {
                ui_col_sublabel(&col1, "Wavetable:", ORANGE);
                const char* scwNames[SCW_MAX_SLOTS];
                for (int i = 0; i < scwCount; i++) scwNames[i] = scwTables[i].name;
                ui_col_cycle(&col1, "SCW", scwNames, scwCount, &cp->p_scwIndex);
            }
            
            if (selectedWave == WAVE_VOICE) {
                ui_col_sublabel(&col1, "Formant:", ORANGE);
                ui_col_cycle(&col1, "Vowel", vowelNames, 5, &cp->p_voiceVowel);
                ui_col_toggle(&col1, "Random", &voiceRandomVowel);
                ui_col_float(&col1, "Pitch", &cp->p_voicePitch, 0.1f, 0.3f, 2.0f);
                ui_col_float(&col1, "Speed", &cp->p_voiceSpeed, 1.0f, 4.0f, 20.0f);
                ui_col_float(&col1, "Formant", &cp->p_voiceFormantShift, 0.05f, 0.5f, 1.5f);
                ui_col_float(&col1, "Breath", &cp->p_voiceBreathiness, 0.05f, 0.0f, 1.0f);
                ui_col_float(&col1, "Buzz", &cp->p_voiceBuzziness, 0.05f, 0.0f, 1.0f);
                ui_col_space(&col1, 4);
                ui_col_sublabel(&col1, "Extras:", ORANGE);
                ui_col_toggle(&col1, "Consonant", &cp->p_voiceConsonant);
                if (cp->p_voiceConsonant) {
                    ui_col_float(&col1, "ConsAmt", &cp->p_voiceConsonantAmt, 0.05f, 0.0f, 1.0f);
                }
                ui_col_toggle(&col1, "Nasal", &cp->p_voiceNasal);
                if (cp->p_voiceNasal) {
                    ui_col_float(&col1, "NasalAmt", &cp->p_voiceNasalAmt, 0.05f, 0.0f, 1.0f);
                }
                ui_col_space(&col1, 4);
                ui_col_sublabel(&col1, "Pitch Env:", ORANGE);
                ui_col_float(&col1, "Bend", &cp->p_voicePitchEnv, 0.5f, -12.0f, 12.0f);
                ui_col_float(&col1, "Time", &cp->p_voicePitchEnvTime, 0.02f, 0.02f, 0.5f);
                ui_col_float(&col1, "Curve", &cp->p_voicePitchEnvCurve, 0.1f, -1.0f, 1.0f);
            }
            
            if (selectedWave == WAVE_PLUCK) {
                ui_col_sublabel(&col1, "Pluck:", ORANGE);
                ui_col_float(&col1, "Bright", &cp->p_pluckBrightness, 0.05f, 0.0f, 1.0f);
                ui_col_float(&col1, "Sustain", &cp->p_pluckDamping, 0.0002f, 0.995f, 0.9998f);
                ui_col_float(&col1, "Damp", &cp->p_pluckDamp, 0.05f, 0.0f, 1.0f);
            }
            
            if (selectedWave == WAVE_ADDITIVE) {
                ui_col_sublabel(&col1, "Additive:", ORANGE);
                ui_col_cycle(&col1, "Preset", additivePresetNames, ADDITIVE_PRESET_COUNT, &cp->p_additivePreset);
                ui_col_float(&col1, "Bright", &cp->p_additiveBrightness, 0.05f, 0.0f, 1.0f);
                ui_col_float(&col1, "Shimmer", &cp->p_additiveShimmer, 0.05f, 0.0f, 1.0f);
                ui_col_float(&col1, "Inharm", &cp->p_additiveInharmonicity, 0.005f, 0.0f, 0.1f);
            }
            
            if (selectedWave == WAVE_MALLET) {
                ui_col_sublabel(&col1, "Mallet:", ORANGE);
                ui_col_cycle(&col1, "Preset", malletPresetNames, MALLET_PRESET_COUNT, &cp->p_malletPreset);
                ui_col_float(&col1, "Stiff", &cp->p_malletStiffness, 0.05f, 0.0f, 1.0f);
                ui_col_float(&col1, "Hard", &cp->p_malletHardness, 0.05f, 0.0f, 1.0f);
                ui_col_float(&col1, "Strike", &cp->p_malletStrikePos, 0.05f, 0.0f, 1.0f);
                ui_col_float(&col1, "Reson", &cp->p_malletResonance, 0.05f, 0.0f, 1.0f);
                ui_col_float(&col1, "Damp", &cp->p_malletDamp, 0.05f, 0.0f, 1.0f);
                ui_col_float(&col1, "Tremolo", &cp->p_malletTremolo, 0.05f, 0.0f, 1.0f);
                ui_col_float(&col1, "TremSpd", &cp->p_malletTremoloRate, 0.5f, 1.0f, 12.0f);
            }
            
            if (selectedWave == WAVE_GRANULAR && scwCount > 0) {
                ui_col_sublabel(&col1, "Granular:", ORANGE);
                const char* scwNames[SCW_MAX_SLOTS];
                for (int i = 0; i < scwCount; i++) scwNames[i] = scwTables[i].name;
                ui_col_cycle(&col1, "Source", scwNames, scwCount, &cp->p_granularScwIndex);
                ui_col_float(&col1, "Size", &cp->p_granularGrainSize, 5.0f, 10.0f, 200.0f);
                ui_col_float(&col1, "Density", &cp->p_granularDensity, 2.0f, 1.0f, 100.0f);
                ui_col_float(&col1, "Position", &cp->p_granularPosition, 0.05f, 0.0f, 1.0f);
                ui_col_float(&col1, "PosRand", &cp->p_granularPosRandom, 0.05f, 0.0f, 1.0f);
                ui_col_float(&col1, "Pitch", &cp->p_granularPitch, 0.1f, 0.25f, 4.0f);
                ui_col_float(&col1, "PitRand", &cp->p_granularPitchRandom, 0.5f, 0.0f, 12.0f);
                ui_col_float(&col1, "AmpRand", &cp->p_granularAmpRandom, 0.05f, 0.0f, 1.0f);
                ui_col_toggle(&col1, "Freeze", &cp->p_granularFreeze);
            }
            
            if (selectedWave == WAVE_FM) {
                ui_col_sublabel(&col1, "FM Synth:", ORANGE);
                ui_col_float(&col1, "Ratio", &cp->p_fmModRatio, 0.5f, 0.5f, 16.0f);
                ui_col_float(&col1, "Index", &cp->p_fmModIndex, 0.1f, 0.0f, 10.0f);
                ui_col_float(&col1, "Feedback", &cp->p_fmFeedback, 0.05f, 0.0f, 1.0f);
            }
            
            if (selectedWave == WAVE_PD) {
                ui_col_sublabel(&col1, "Phase Dist:", ORANGE);
                ui_col_cycle(&col1, "Wave", pdWaveNames, PD_WAVE_COUNT, &cp->p_pdWaveType);
                ui_col_float(&col1, "Distort", &cp->p_pdDistortion, 0.05f, 0.0f, 1.0f);
            }
            
            if (selectedWave == WAVE_MEMBRANE) {
                ui_col_sublabel(&col1, "Membrane:", ORANGE);
                ui_col_cycle(&col1, "Preset", membranePresetNames, MEMBRANE_COUNT, &cp->p_membranePreset);
                ui_col_float(&col1, "Damping", &cp->p_membraneDamping, 0.05f, 0.1f, 1.0f);
                ui_col_float(&col1, "Strike", &cp->p_membraneStrike, 0.05f, 0.0f, 1.0f);
                ui_col_float(&col1, "Bend", &cp->p_membraneBend, 0.02f, 0.0f, 0.5f);
                ui_col_float(&col1, "BendDcy", &cp->p_membraneBendDecay, 0.01f, 0.02f, 0.3f);
            }
            
            if (selectedWave == WAVE_BIRD) {
                ui_col_sublabel(&col1, "Bird:", ORANGE);
                ui_col_cycle(&col1, "Type", birdTypeNames, BIRD_COUNT, &cp->p_birdType);
                ui_col_float(&col1, "Range", &cp->p_birdChirpRange, 0.1f, 0.5f, 2.0f);
                ui_col_float(&col1, "Harmonic", &cp->p_birdHarmonics, 0.05f, 0.0f, 1.0f);
                ui_col_space(&col1, 4);
                ui_col_sublabel(&col1, "Trill:", ORANGE);
                ui_col_float(&col1, "Rate", &cp->p_birdTrillRate, 1.0f, 0.0f, 30.0f);
                ui_col_float(&col1, "Depth", &cp->p_birdTrillDepth, 0.2f, 0.0f, 5.0f);
                ui_col_space(&col1, 4);
                ui_col_sublabel(&col1, "Flutter:", ORANGE);
                ui_col_float(&col1, "AM Rate", &cp->p_birdAmRate, 1.0f, 0.0f, 20.0f);
                ui_col_float(&col1, "AM Depth", &cp->p_birdAmDepth, 0.05f, 0.0f, 1.0f);
            }
        }
        
        // === COLUMN 2: Synth (shared settings) ===
        UIColumn col2 = ui_column(250, 75, 20);
        
        if (SectionHeader(col2.x, col2.y, "Synth", &showSynthColumn)) {
            col2.y += 18;
            
            ui_col_sublabel(&col2, "Envelope:", ORANGE);
            ui_col_float(&col2, "Attack", &cp->p_attack, 0.5f, 0.001f, 2.0f);
            ui_col_float(&col2, "Decay", &cp->p_decay, 0.5f, 0.0f, 2.0f);
            ui_col_float(&col2, "Sustain", &cp->p_sustain, 0.5f, 0.0f, 1.0f);
            ui_col_float(&col2, "Release", &cp->p_release, 0.5f, 0.01f, 3.0f);
            ui_col_space(&col2, 4);
            
            ui_col_sublabel(&col2, "Vibrato:", ORANGE);
            ui_col_float(&col2, "Rate", &cp->p_vibratoRate, 0.5f, 0.5f, 15.0f);
            ui_col_float(&col2, "Depth", &cp->p_vibratoDepth, 0.2f, 0.0f, 2.0f);
            ui_col_space(&col2, 4);
            
            ui_col_sublabel(&col2, "Filter:", ORANGE);
            ui_col_float(&col2, "Cutoff", &cp->p_filterCutoff, 0.05f, 0.01f, 1.0f);
            ui_col_float(&col2, "Reso", &cp->p_filterResonance, 0.05f, 0.0f, 1.0f);
            ui_col_float(&col2, "EnvAmt", &cp->p_filterEnvAmt, 0.05f, -1.0f, 1.0f);
            ui_col_float(&col2, "EnvAtk", &cp->p_filterEnvAttack, 0.01f, 0.001f, 0.5f);
            ui_col_float(&col2, "EnvDcy", &cp->p_filterEnvDecay, 0.05f, 0.01f, 2.0f);
            ui_col_space(&col2, 4);
            
            ui_col_sublabel(&col2, "Volume:", ORANGE);
            ui_col_float(&col2, "Note", &cp->p_volume, 0.05f, 0.0f, 1.0f);
            ui_col_float(&col2, "Master", &masterVolume, 0.05f, 0.0f, 1.0f);
            
            // Mono/Glide - only show for wave types that support it
            if (selectedWave != WAVE_PLUCK && selectedWave != WAVE_MALLET) {
                ui_col_space(&col2, 4);
                ui_col_sublabel(&col2, "Mono/Glide:", ORANGE);
                ui_col_toggle(&col2, "Mono", &cp->p_monoMode);
                if (cp->p_monoMode) {
                    ui_col_float(&col2, "Glide", &cp->p_glideTime, 0.02f, 0.01f, 1.0f);
                }
            }
            
            ui_col_space(&col2, 4);
            ui_col_sublabel(&col2, "Scale Lock:", ORANGE);
            ui_col_toggle(&col2, "Enabled", &scaleLockEnabled);
            if (scaleLockEnabled) {
                ui_col_cycle(&col2, "Root", rootNoteNames, 12, &scaleRoot);
                int scaleIdx = (int)scaleType;
                ui_col_cycle(&col2, "Scale", scaleNames, SCALE_COUNT, &scaleIdx);
                scaleType = (ScaleType)scaleIdx;
            }
        }
        
        // === COLUMN 3: LFOs ===
        UIColumn col3 = ui_column(430, 125, 20);
        
        if (SectionHeader(col3.x, col3.y, "LFOs", &showLfoColumn)) {
            col3.y += 18;
            
            static const char* lfoShapeNames[] = {"Sine", "Tri", "Sqr", "Saw", "S&H"};
            
            ui_col_sublabel(&col3, "Filter:", ORANGE);
            ui_col_float(&col3, "Rate", &cp->p_filterLfoRate, 0.5f, 0.0f, 20.0f);
            ui_col_float(&col3, "Depth", &cp->p_filterLfoDepth, 0.05f, 0.0f, 2.0f);
            ui_col_cycle(&col3, "Shape", lfoShapeNames, 5, &cp->p_filterLfoShape);
            ui_col_space(&col3, 4);
            
            ui_col_sublabel(&col3, "Resonance:", ORANGE);
            ui_col_float(&col3, "Rate", &cp->p_resoLfoRate, 0.5f, 0.0f, 20.0f);
            ui_col_float(&col3, "Depth", &cp->p_resoLfoDepth, 0.05f, 0.0f, 1.0f);
            ui_col_cycle(&col3, "Shape", lfoShapeNames, 5, &cp->p_resoLfoShape);
            ui_col_space(&col3, 4);
            
            ui_col_sublabel(&col3, "Amplitude:", ORANGE);
            ui_col_float(&col3, "Rate", &cp->p_ampLfoRate, 0.5f, 0.0f, 20.0f);
            ui_col_float(&col3, "Depth", &cp->p_ampLfoDepth, 0.05f, 0.0f, 1.0f);
            ui_col_cycle(&col3, "Shape", lfoShapeNames, 5, &cp->p_ampLfoShape);
            ui_col_space(&col3, 4);
            
            ui_col_sublabel(&col3, "Pitch:", ORANGE);
            ui_col_float(&col3, "Rate", &cp->p_pitchLfoRate, 0.5f, 0.0f, 20.0f);
            ui_col_float(&col3, "Depth", &cp->p_pitchLfoDepth, 0.05f, 0.0f, 1.0f);
            ui_col_cycle(&col3, "Shape", lfoShapeNames, 5, &cp->p_pitchLfoShape);
        }
        
        // === COLUMN 4: Drums ===
        UIColumn col4 = ui_column(610, 125, 20);
        
        if (SectionHeader(col4.x, col4.y, "Drums", &showDrumsColumn)) {
            col4.y += 18;
            ui_col_float(&col4, "Volume", &drumVolume, 0.05f, 0.0f, 1.0f);
            ui_col_space(&col4, 4);
            
            // Show params for drums currently assigned to tracks
            // Track which drum types we've already shown to avoid duplicates
            bool shownDrumType[DRUM_COUNT] = {false};
            
            for (int track = 0; track < SEQ_DRUM_TRACKS; track++) {
                DrumType dt = drumTrackSound[track];
                if (shownDrumType[dt]) continue;  // Skip if already shown
                shownDrumType[dt] = true;
                
                ui_col_sublabel(&col4, TextFormat("%s:", drumTypeShortNames[dt]), ORANGE);
                
                switch (dt) {
                    case DRUM_KICK:
                        ui_col_float(&col4, "Pitch", &drumParams.kickPitch, 3.0f, 30.0f, 100.0f);
                        ui_col_float(&col4, "Decay", &drumParams.kickDecay, 0.07f, 0.1f, 1.5f);
                        ui_col_float(&col4, "Punch", &drumParams.kickPunchPitch, 10.0f, 80.0f, 300.0f);
                        ui_col_float(&col4, "Click", &drumParams.kickClick, 0.05f, 0.0f, 1.0f);
                        ui_col_float(&col4, "Tone", &drumParams.kickTone, 0.05f, 0.0f, 1.0f);
                        break;
                    case DRUM_SNARE:
                        ui_col_float(&col4, "Pitch", &drumParams.snarePitch, 10.0f, 100.0f, 350.0f);
                        ui_col_float(&col4, "Decay", &drumParams.snareDecay, 0.03f, 0.05f, 0.6f);
                        ui_col_float(&col4, "Snappy", &drumParams.snareSnappy, 0.05f, 0.0f, 1.0f);
                        ui_col_float(&col4, "Tone", &drumParams.snareTone, 0.05f, 0.0f, 1.0f);
                        break;
                    case DRUM_CLAP:
                        ui_col_float(&col4, "Decay", &drumParams.clapDecay, 0.03f, 0.1f, 0.6f);
                        ui_col_float(&col4, "Tone", &drumParams.clapTone, 0.05f, 0.0f, 1.0f);
                        ui_col_float(&col4, "Spread", &drumParams.clapSpread, 0.001f, 0.005f, 0.03f);
                        break;
                    case DRUM_CLOSED_HH:
                    case DRUM_OPEN_HH:
                        ui_col_float(&col4, "Closed", &drumParams.hhDecayClosed, 0.01f, 0.01f, 0.2f);
                        ui_col_float(&col4, "Open", &drumParams.hhDecayOpen, 0.05f, 0.1f, 1.0f);
                        ui_col_float(&col4, "Tone", &drumParams.hhTone, 0.05f, 0.0f, 1.0f);
                        // Mark both as shown since they share params
                        shownDrumType[DRUM_CLOSED_HH] = true;
                        shownDrumType[DRUM_OPEN_HH] = true;
                        break;
                    case DRUM_LOW_TOM:
                    case DRUM_MID_TOM:
                    case DRUM_HI_TOM:
                        ui_col_float(&col4, "Pitch", &drumParams.tomPitch, 0.1f, 0.5f, 2.0f);
                        ui_col_float(&col4, "Decay", &drumParams.tomDecay, 0.03f, 0.1f, 0.8f);
                        ui_col_float(&col4, "PunchDcy", &drumParams.tomPunchDecay, 0.01f, 0.01f, 0.2f);
                        // Mark all toms as shown since they share params
                        shownDrumType[DRUM_LOW_TOM] = true;
                        shownDrumType[DRUM_MID_TOM] = true;
                        shownDrumType[DRUM_HI_TOM] = true;
                        break;
                    case DRUM_RIMSHOT:
                        ui_col_float(&col4, "Pitch", &drumParams.rimPitch, 100.0f, 800.0f, 3000.0f);
                        ui_col_float(&col4, "Decay", &drumParams.rimDecay, 0.005f, 0.01f, 0.1f);
                        break;
                    case DRUM_COWBELL:
                        ui_col_float(&col4, "Pitch", &drumParams.cowbellPitch, 20.0f, 400.0f, 1000.0f);
                        ui_col_float(&col4, "Decay", &drumParams.cowbellDecay, 0.03f, 0.1f, 0.6f);
                        break;
                    case DRUM_CLAVE:
                        ui_col_float(&col4, "Pitch", &drumParams.clavePitch, 100.0f, 1500.0f, 4000.0f);
                        ui_col_float(&col4, "Decay", &drumParams.claveDecay, 0.005f, 0.01f, 0.1f);
                        break;
                    case DRUM_MARACAS:
                        ui_col_float(&col4, "Decay", &drumParams.maracasDecay, 0.01f, 0.02f, 0.2f);
                        ui_col_float(&col4, "Tone", &drumParams.maracasTone, 0.05f, 0.0f, 1.0f);
                        break;
                    case DRUM_CR78_KICK:
                        ui_col_float(&col4, "Pitch", &drumParams.cr78KickPitch, 5.0f, 50.0f, 120.0f);
                        ui_col_float(&col4, "Decay", &drumParams.cr78KickDecay, 0.03f, 0.1f, 0.5f);
                        ui_col_float(&col4, "Reso", &drumParams.cr78KickResonance, 0.05f, 0.5f, 0.99f);
                        break;
                    case DRUM_CR78_SNARE:
                        ui_col_float(&col4, "Pitch", &drumParams.cr78SnarePitch, 10.0f, 150.0f, 350.0f);
                        ui_col_float(&col4, "Decay", &drumParams.cr78SnareDecay, 0.02f, 0.05f, 0.4f);
                        ui_col_float(&col4, "Snappy", &drumParams.cr78SnareSnappy, 0.05f, 0.0f, 1.0f);
                        break;
                    case DRUM_CR78_HIHAT:
                        ui_col_float(&col4, "Decay", &drumParams.cr78HHDecay, 0.01f, 0.02f, 0.2f);
                        ui_col_float(&col4, "Tone", &drumParams.cr78HHTone, 0.05f, 0.0f, 1.0f);
                        break;
                    case DRUM_CR78_METAL:
                        ui_col_float(&col4, "Pitch", &drumParams.cr78MetalPitch, 50.0f, 400.0f, 1500.0f);
                        ui_col_float(&col4, "Decay", &drumParams.cr78MetalDecay, 0.02f, 0.05f, 0.4f);
                        break;
                    default:
                        break;
                }
                ui_col_space(&col4, 4);
            }
            
            // Sidechain section (always visible, not per-drum)
            ui_col_sublabel(&col4, "Sidechain:", ORANGE);
            ui_col_toggle(&col4, "On", &fx.sidechainEnabled);
            if (fx.sidechainEnabled) {
                static const char* sidechainSourceNames[] = {"Kick", "Snare", "Clap", "HiHat", "AllDrm"};
                static const char* sidechainTargetNames[] = {"Bass", "Lead", "Chord", "AllSyn"};
                ui_col_cycle(&col4, "Source", sidechainSourceNames, SIDECHAIN_SRC_COUNT, &fx.sidechainSource);
                ui_col_cycle(&col4, "Target", sidechainTargetNames, SIDECHAIN_TGT_COUNT, &fx.sidechainTarget);
                // Show routing: "Kick -> Bass"
                DrawTextShadow(TextFormat("%s -> %s", 
                    sidechainSourceNames[fx.sidechainSource], 
                    sidechainTargetNames[fx.sidechainTarget]), 
                    col4.x + 5, col4.y, 10, LIME);
                col4.y += 14;
                ui_col_float(&col4, "Depth", &fx.sidechainDepth, 0.05f, 0.0f, 1.0f);
                ui_col_float(&col4, "Attack", &fx.sidechainAttack, 0.002f, 0.001f, 0.05f);
                ui_col_float(&col4, "Release", &fx.sidechainRelease, 0.02f, 0.05f, 0.5f);
            }
        }
        
        // === COLUMN 5: Effects ===
        UIColumn col5 = ui_column(790, 125, 20);
        
        if (SectionHeader(col5.x, col5.y, "Effects", &showEffectsColumn)) {
            col5.y += 18;
            
            ui_col_sublabel(&col5, "Distortion:", ORANGE);
            ui_col_toggle(&col5, "On", &fx.distEnabled);
            ui_col_float(&col5, "Drive", &fx.distDrive, 0.5f, 1.0f, 20.0f);
            ui_col_float(&col5, "Tone", &fx.distTone, 0.05f, 0.0f, 1.0f);
            ui_col_float(&col5, "Mix", &fx.distMix, 0.05f, 0.0f, 1.0f);
            ui_col_space(&col5, 4);
            
            ui_col_sublabel(&col5, "Delay:", ORANGE);
            ui_col_toggle(&col5, "On", &fx.delayEnabled);
            ui_col_float(&col5, "Time", &fx.delayTime, 0.05f, 0.05f, 1.0f);
            ui_col_float(&col5, "Feedback", &fx.delayFeedback, 0.05f, 0.0f, 0.9f);
            ui_col_float(&col5, "Tone", &fx.delayTone, 0.05f, 0.0f, 1.0f);
            ui_col_float(&col5, "Mix", &fx.delayMix, 0.05f, 0.0f, 1.0f);
            ui_col_space(&col5, 4);
            
            ui_col_sublabel(&col5, "Tape:", ORANGE);
            ui_col_toggle(&col5, "On", &fx.tapeEnabled);
            ui_col_float(&col5, "Saturation", &fx.tapeSaturation, 0.05f, 0.0f, 1.0f);
            ui_col_float(&col5, "Wow", &fx.tapeWow, 0.05f, 0.0f, 1.0f);
            ui_col_float(&col5, "Flutter", &fx.tapeFlutter, 0.05f, 0.0f, 1.0f);
            ui_col_float(&col5, "Hiss", &fx.tapeHiss, 0.05f, 0.0f, 1.0f);
            ui_col_space(&col5, 4);
            
            ui_col_sublabel(&col5, "Bitcrusher:", ORANGE);
            ui_col_toggle(&col5, "On", &fx.crushEnabled);
            ui_col_float(&col5, "Bits", &fx.crushBits, 0.5f, 2.0f, 16.0f);
            ui_col_float(&col5, "Rate", &fx.crushRate, 1.0f, 1.0f, 32.0f);
            ui_col_float(&col5, "Mix", &fx.crushMix, 0.05f, 0.0f, 1.0f);
            ui_col_space(&col5, 4);
            
            ui_col_sublabel(&col5, "Reverb:", ORANGE);
            ui_col_toggle(&col5, "On", &fx.reverbEnabled);
            ui_col_float(&col5, "Size", &fx.reverbSize, 0.05f, 0.0f, 1.0f);
            ui_col_float(&col5, "Damping", &fx.reverbDamping, 0.05f, 0.0f, 1.0f);
            ui_col_float(&col5, "PreDly", &fx.reverbPreDelay, 0.005f, 0.0f, 0.1f);
            ui_col_float(&col5, "Mix", &fx.reverbMix, 0.05f, 0.0f, 1.0f);
        }
        
        // === COLUMN 6: Tape FX (Dub Loop + Rewind) ===
        UIColumn col6 = ui_column(950, 125, 20);
        
        // Rewind curve names for UI
        static const char* rewindCurveNames[] = {"Linear", "Expo", "S-Curve"};
        static const char* dubInputNames[] = {
            "All", "Drums", "Synth", "Manual",
            "Kick", "Snare", "Clap", "ClHH", "OpHH",
            "LoTom", "MdTom", "HiTom", "Rim", "Cow", "Clav", "Mara",
            "Bass", "Lead", "Chord",
            // Padding for indices 19-23 (unused)
            "-", "-", "-", "-", "-",
            // Bus sources (24-30)
            "Bus:D0", "Bus:D1", "Bus:D2", "Bus:D3", "Bus:Bs", "Bus:Ld", "Bus:Ch"
        };
        
        if (SectionHeader(col6.x, col6.y, "Tape FX", &showTapeFxColumn)) {
            col6.y += 18;
            
            ui_col_sublabel(&col6, "Dub Loop:", ORANGE);
            ui_col_toggle(&col6, "On", &dubLoop.enabled);
            ui_col_float(&col6, "Time", &dubLoop.headTime[0], 0.05f, 0.05f, 4.0f);
            ui_col_float(&col6, "Feedback", &dubLoop.feedback, 0.05f, 0.0f, 0.95f);
            ui_col_float(&col6, "Mix", &dubLoop.mix, 0.05f, 0.0f, 1.0f);
            ui_col_space(&col6, 2);
            
            // NEW: Input source and routing
            ui_col_cycle(&col6, "Input", dubInputNames, 31, &dubLoop.inputSource);
            ui_col_toggle(&col6, "PreReverb", &dubLoop.preReverb);
            ui_col_space(&col6, 2);
            
            // Degradation
            ui_col_float(&col6, "Saturat", &dubLoop.saturation, 0.05f, 0.0f, 1.0f);
            ui_col_float(&col6, "ToneHi", &dubLoop.toneHigh, 0.05f, 0.0f, 1.0f);
            ui_col_float(&col6, "Noise", &dubLoop.noise, 0.02f, 0.0f, 0.5f);
            ui_col_float(&col6, "Degrade", &dubLoop.degradeRate, 0.02f, 0.0f, 0.5f);
            ui_col_space(&col6, 2);
            
            // Modulation
            ui_col_float(&col6, "Wow", &dubLoop.wow, 0.05f, 0.0f, 1.0f);
            ui_col_float(&col6, "Flutter", &dubLoop.flutter, 0.05f, 0.0f, 1.0f);
            ui_col_float(&col6, "Drift", &dubLoop.drift, 0.05f, 0.0f, 1.0f);
            ui_col_space(&col6, 2);
            ui_col_float(&col6, "Speed", &dubLoop.speedTarget, 0.1f, 0.25f, 2.0f);
            
            // Action buttons
            ui_col_space(&col6, 4);
            if (ui_col_button(&col6, "Throw")) {
                dubLoopThrow();
            }
            if (ui_col_button(&col6, "Cut")) {
                dubLoopCut();
            }
            if (ui_col_button(&col6, "1/2 Spd")) {
                dubLoopHalfSpeed();
            }
            if (ui_col_button(&col6, "Normal")) {
                dubLoopNormalSpeed();
            }
            
            ui_col_space(&col6, 8);
            ui_col_sublabel(&col6, "Rewind:", ORANGE);
            ui_col_float(&col6, "Time", &rewind.rewindTime, 0.1f, 0.3f, 3.0f);
            ui_col_cycle(&col6, "Curve", rewindCurveNames, 3, &rewind.curve);
            ui_col_float(&col6, "MinSpd", &rewind.minSpeed, 0.05f, 0.05f, 0.8f);
            ui_col_space(&col6, 2);
            ui_col_float(&col6, "Vinyl", &rewind.vinylNoise, 0.05f, 0.0f, 1.0f);
            ui_col_float(&col6, "Wobble", &rewind.wobble, 0.05f, 0.0f, 1.0f);
            ui_col_float(&col6, "Filter", &rewind.filterSweep, 0.05f, 0.0f, 1.0f);
            
            ui_col_space(&col6, 4);
            if (ui_col_button(&col6, isRewinding() ? ">>..." : "Rewind")) {
                if (!isRewinding()) {
                    triggerRewind();
                }
            }
        }
        
        // === SEQUENCER GRID (Drums + Melodic) ===
        {
            static bool isDragging = false;
            static bool isDraggingPitch = false;
            static int dragTrack = -1;
            static int dragStep = -1;
            static bool dragIsMelody = false;
            static float dragStartY = 0.0f;
            static float dragStartVal = 0.0f;
            
            // Step inspector selection state
            static int selectedTrack = -1;
            static int selectedStep = -1;
            static bool selectedIsMelody = false;
            
            Pattern *p = seqCurrentPattern();
            
            int gridX = 20;
            int gridY = SCREEN_HEIGHT - 240;  // Moved down 30px
            int cellW = 32;  // Wider cells for better visibility
            int cellH = 20;
            int labelW = 80;  // Increased to fit volume slider
            int lengthW = 30;
            int patternBarY = gridY - 28;
            
            // === PATTERN BAR ===
            {
                int patW = 28;
                int patH = 20;
                int patX = gridX + labelW;
                
                DrawTextShadow("Pattern:", gridX, patternBarY + 4, 12, YELLOW);
                
                Vector2 mouse = GetMousePosition();
                bool mouseClicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
                bool rightClicked = IsMouseButtonPressed(MOUSE_RIGHT_BUTTON);
                bool shiftHeld = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
                
                for (int i = 0; i < SEQ_NUM_PATTERNS; i++) {
                    int px = patX + i * (patW + 4);
                    Rectangle patRect = {(float)px, (float)patternBarY, (float)patW, (float)patH};
                    bool isHovered = CheckCollisionPointRec(mouse, patRect);
                    bool isCurrent = (i == seq.currentPattern);
                    bool isQueued = (i == seq.nextPattern);
                    
                    // Check if pattern has any content
                    bool hasContent = false;
                    for (int t = 0; t < SEQ_DRUM_TRACKS && !hasContent; t++) {
                        for (int s = 0; s < SEQ_MAX_STEPS && !hasContent; s++) {
                            if (seq.patterns[i].drumSteps[t][s]) hasContent = true;
                        }
                    }
                    for (int t = 0; t < SEQ_MELODY_TRACKS && !hasContent; t++) {
                        for (int s = 0; s < SEQ_MAX_STEPS && !hasContent; s++) {
                            if (seq.patterns[i].melodyNote[t][s] != SEQ_NOTE_OFF) hasContent = true;
                        }
                    }
                    
                    Color bgColor = {40, 40, 40, 255};
                    if (isCurrent) bgColor = (Color){60, 100, 60, 255};
                    else if (isQueued) bgColor = (Color){80, 80, 40, 255};
                    else if (hasContent) bgColor = (Color){50, 50, 60, 255};
                    
                    if (isHovered) {
                        bgColor.r = (unsigned char)fminf(255, bgColor.r + 30);
                        bgColor.g = (unsigned char)fminf(255, bgColor.g + 30);
                        bgColor.b = (unsigned char)fminf(255, bgColor.b + 30);
                    }
                    
                    DrawRectangleRec(patRect, bgColor);
                    DrawRectangleLinesEx(patRect, 1, isCurrent ? GREEN : (isQueued ? YELLOW : (Color){80, 80, 80, 255}));
                    
                    Color textColor = isCurrent ? WHITE : (isQueued ? YELLOW : (hasContent ? LIGHTGRAY : GRAY));
                    DrawTextShadow(TextFormat("%d", i + 1), px + 10, patternBarY + 4, 12, textColor);
                    
                    if (isHovered) {
                        if (mouseClicked) {
                            if (shiftHeld) {
                                seqCopyPatternTo(i);
                            } else {
                                seqQueuePattern(i);
                            }
                            ui_consume_click();
                        }
                        if (rightClicked) {
                            clearPattern(&seq.patterns[i]);
                            ui_consume_click();
                        }
                    }
                }
                
                // Fill toggle
                int ctrlX = patX + SEQ_NUM_PATTERNS * (patW + 4) + 20;
                ToggleBool(ctrlX, patternBarY, "Fill", &seq.fillMode);
                
                if (seq.nextPattern >= 0) {
                    DrawTextShadow(TextFormat("-> %d", seq.nextPattern + 1), ctrlX + 70, patternBarY + 4, 12, YELLOW);
                }
            }
            
            // Sequencer [?] help button
            {
                int helpX = gridX + labelW + SEQ_MAX_STEPS * cellW + 10;
                int helpY = gridY - 10;
                const char* helpLabel = "[?]";
                int w = MeasureText(helpLabel, 14);
                Rectangle helpRect = {(float)helpX, (float)helpY, (float)w, 16};
                Vector2 mouse = GetMousePosition();
                bool helpHovered = CheckCollisionPointRec(mouse, helpRect);
                DrawTextShadow(helpLabel, helpX, helpY, 14, helpHovered ? YELLOW : GRAY);
                g_showSeqHelpOverlay = helpHovered;
            }
            
            // Beat markers
            for (int i = 0; i < 4; i++) {
                int x = gridX + labelW + i * 4 * cellW + 2;
                DrawTextShadow(TextFormat("%d", i + 1), x, gridY - 10, 10, GRAY);
            }
            

            
            Vector2 mouse = GetMousePosition();
            bool mouseClicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
            bool mouseDown = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
            bool mouseReleased = IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
            bool rightClicked = IsMouseButtonPressed(MOUSE_RIGHT_BUTTON);
            float mouseWheel = GetMouseWheelMove();
            
            if (mouseReleased && isDragging) {
                isDragging = false;
                dragTrack = -1;
                dragStep = -1;
            }
            
            // Handle drum dragging
            if (isDragging && !dragIsMelody && mouseDown && dragTrack >= 0 && dragStep >= 0) {
                float deltaY = dragStartY - mouse.y;
                if (isDraggingPitch) {
                    float newPitch = dragStartVal + deltaY * 0.01f;
                    p->drumPitch[dragTrack][dragStep] = clampf(newPitch, -1.0f, 1.0f);
                } else {
                    float newVel = dragStartVal + deltaY * 0.01f;
                    p->drumVelocity[dragTrack][dragStep] = clampf(newVel, 0.1f, 1.0f);
                }
            }
            
            // === DRUM TRACKS ===
            for (int track = 0; track < SEQ_DRUM_TRACKS; track++) {
                int y = gridY + track * cellH;
                int trackLen = p->drumTrackLength[track];
                
                // Clickable track label to change drum sound
                Rectangle labelRect = {(float)gridX, (float)y, 40, (float)cellH - 2};
                bool labelHovered = CheckCollisionPointRec(mouse, labelRect);
                Color labelColor = labelHovered ? WHITE : LIGHTGRAY;
                DrawTextShadow(seq.drumTrackNames[track], gridX, y + 3, 12, labelColor);
                
                if (labelHovered && mouseWheel != 0) {
                    cycleDrumTrackSound(track, mouseWheel > 0 ? -1 : 1);
                }
                
                // Track volume slider
                {
                    int volX = gridX + 38;
                    int volW = labelW - 42;
                    float vol = seq.trackVolume[track];
                    Rectangle volRect = {(float)volX, (float)(y + 2), (float)volW, (float)(cellH - 4)};
                    bool volHovered = CheckCollisionPointRec(mouse, volRect);
                    
                    // Background
                    DrawRectangleRec(volRect, (Color){15, 15, 15, 255});
                    // Fill based on volume
                    int fillW = (int)((volW - 2) * vol);
                    if (fillW > 0) {
                        DrawRectangle(volX + 1, y + 3, fillW, cellH - 6, volHovered ? (Color){80, 200, 80, 255} : (Color){50, 150, 50, 255});
                    }
                    DrawRectangleLinesEx(volRect, 1, volHovered ? GREEN : (Color){40, 80, 40, 255});
                    
                    if (volHovered) {
                        if (mouseDown) {
                            float newVol = (mouse.x - volX) / (float)volW;
                            seq.trackVolume[track] = clampf(newVol, 0.0f, 1.0f);
                        }
                        if (mouseWheel != 0) {
                            seq.trackVolume[track] = clampf(vol + mouseWheel * 0.05f, 0.0f, 1.0f);
                        }
                    }
                }
                
                for (int step = 0; step < SEQ_MAX_STEPS; step++) {
                    int x = gridX + labelW + step * cellW;
                    Rectangle cell = {(float)x, (float)y, (float)cellW - 2, (float)cellH - 2};
                    
                    bool isInRange = step < trackLen;
                    bool isActive = p->drumSteps[track][step] && isInRange;
                    bool isCurrent = (step == seq.drumStep[track]) && seq.playing && isInRange;
                    bool isHovered = CheckCollisionPointRec(mouse, cell);
                    bool isBeingDragged = isDragging && !dragIsMelody && dragTrack == track && dragStep == step;
                    bool isSelected = !selectedIsMelody && selectedTrack == track && selectedStep == step;
                    bool hasPitchOffset = isActive && fabsf(p->drumPitch[track][step]) > 0.01f;
                    bool hasProb = isActive && p->drumProbability[track][step] < 1.0f;
                    bool hasCond = isActive && p->drumCondition[track][step] != COND_ALWAYS;
                    bool hasFlam = isActive && seqGetPLock(p, track, step, PLOCK_FLAM_TIME, 0.0f) > 0.0f;
                    bool hasNudge = isActive && fabsf(seqGetPLock(p, track, step, PLOCK_TIME_NUDGE, 0.0f)) > 0.1f;
                    
                    Color bgColor = (step / 4) % 2 == 0 ? (Color){40, 40, 40, 255} : (Color){30, 30, 30, 255};
                    if (!isInRange) bgColor = (Color){55, 55, 55, 255};  // Light gray for out-of-range
                    
                    Color cellColor = bgColor;
                    if (isActive) {
                        float vel = p->drumVelocity[track][step];
                        unsigned char baseG = (unsigned char)(80 + vel * 100);
                        unsigned char baseR = (unsigned char)(30 + vel * 50);
                        unsigned char baseB = (unsigned char)(30 + vel * 50);
                        cellColor = (Color){baseR, baseG, baseB, 255};
                        if (isCurrent) {
                            cellColor.r = (unsigned char)fminf(255, cellColor.r + 40);
                            cellColor.g = (unsigned char)fminf(255, cellColor.g + 75);
                            cellColor.b = (unsigned char)fminf(255, cellColor.b + 40);
                        }
                    } else if (isCurrent) {
                        cellColor = (Color){60, 60, 80, 255};
                    }
                    if (isHovered && isInRange && !isDragging) {
                        cellColor.r = (unsigned char)fminf(255, cellColor.r + 30);
                        cellColor.g = (unsigned char)fminf(255, cellColor.g + 30);
                        cellColor.b = (unsigned char)fminf(255, cellColor.b + 30);
                    }
                    
                    DrawRectangleRec(cell, cellColor);
                    
                    Color borderColor = isInRange ? (Color){60, 60, 60, 255} : (Color){35, 35, 35, 255};
                    if (isSelected) borderColor = ORANGE;
                    DrawRectangleLinesEx(cell, isSelected ? 2 : 1, borderColor);
                    
                    // Indicators
                    if (hasPitchOffset) {
                        float pit = p->drumPitch[track][step];
                        Color triColor = pit > 0 ? (Color){255, 150, 50, 255} : (Color){100, 150, 255, 255};
                        DrawRectangle(x + cellW - 6, y + 2, 3, 3, triColor);
                    }
                    if (hasProb) {
                        DrawCircle(x + 4, y + cellH - 5, 2, (Color){150, 100, 200, 200});
                    }
                    if (hasCond) {
                        DrawRectangle(x + cellW - 6, y + cellH - 5, 3, 3, (Color){200, 150, 50, 255});
                    }
                    if (hasFlam) {
                        // Double-strike indicator (two small lines)
                        DrawRectangle(x + 2, y + 2, 2, 5, (Color){255, 100, 180, 255});
                        DrawRectangle(x + 5, y + 2, 2, 5, (Color){255, 100, 180, 200});
                    }
                    if (hasNudge) {
                        // Timing arrow indicator
                        float nudgeVal = seqGetPLock(p, track, step, PLOCK_TIME_NUDGE, 0.0f);
                        Color nudgeColor = (Color){100, 180, 255, 255};
                        int arrowX = x + cellW/2 - 2;
                        int arrowY = y + cellH - 4;
                        if (nudgeVal > 0) {
                            // Late: arrow pointing right
                            DrawTriangle((Vector2){arrowX, arrowY-2}, (Vector2){arrowX, arrowY+2}, (Vector2){arrowX+4, arrowY}, nudgeColor);
                        } else {
                            // Early: arrow pointing left
                            DrawTriangle((Vector2){arrowX+4, arrowY-2}, (Vector2){arrowX+4, arrowY+2}, (Vector2){arrowX, arrowY}, nudgeColor);
                        }
                    }
                    
                    if (isHovered && isInRange && !isDragging) {
                        if (mouseClicked) {
                            if (isActive) {
                                selectedTrack = track;
                                selectedStep = step;
                                selectedIsMelody = false;
                                isDragging = true;
                                dragIsMelody = false;
                                isDraggingPitch = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
                                dragTrack = track;
                                dragStep = step;
                                dragStartY = mouse.y;
                                dragStartVal = isDraggingPitch ? p->drumPitch[track][step] : p->drumVelocity[track][step];
                                ui_consume_click();
                            } else {
                                p->drumSteps[track][step] = true;
                                selectedTrack = track;
                                selectedStep = step;
                                selectedIsMelody = false;
                                ui_consume_click();
                                float pitchMod = powf(2.0f, p->drumPitch[track][step]);
                                if (seq.drumTriggers[track]) {
                                    seq.drumTriggers[track](p->drumVelocity[track][step], pitchMod);
                                }
                            }
                        }
                        if (rightClicked && isActive) {
                            p->drumSteps[track][step] = false;
                            if (!selectedIsMelody && selectedTrack == track && selectedStep == step) {
                                selectedTrack = -1;
                                selectedStep = -1;
                            }
                            ui_consume_click();
                        }
                    }
                }
                
            }
            
            // Separator line between drums and melody
            int sepY = gridY + SEQ_DRUM_TRACKS * cellH + 2;
            DrawLine(gridX, sepY, gridX + labelW + SEQ_MAX_STEPS * cellW, sepY, (Color){80, 80, 80, 255});
            
            // === MELODIC TRACKS ===
            int melodyStartY = sepY + 4;
            Color melodyTrackColors[3] = {
                {60, 80, 120, 255},   // Bass - blue tint
                {120, 80, 60, 255},   // Lead - orange tint  
                {80, 100, 80, 255}    // Chord - green tint
            };
            
            // Mapping: melody track -> patch index
            static const int melodyTrackToPatch[SEQ_MELODY_TRACKS] = {PATCH_BASS, PATCH_LEAD, PATCH_CHORD};
            
            for (int track = 0; track < SEQ_MELODY_TRACKS; track++) {
                int y = melodyStartY + track * cellH;
                int trackLen = p->melodyTrackLength[track];
                
                // Clickable label to select patch
                Rectangle labelRect = {(float)gridX, (float)y, 40, (float)cellH - 2};
                bool labelHovered = CheckCollisionPointRec(mouse, labelRect);
                Color labelColor = labelHovered ? WHITE : melodyTrackColors[track];
                DrawTextShadow(seq.melodyTrackNames[track], gridX, y + 3, 12, labelColor);
                
                if (labelHovered && mouseClicked) {
                    selectedPatch = melodyTrackToPatch[track];
                    ui_consume_click();
                }
                
                // Track volume slider
                {
                    int absTrack = SEQ_DRUM_TRACKS + track;
                    int volX = gridX + 38;
                    int volW = labelW - 42;
                    float vol = seq.trackVolume[absTrack];
                    Rectangle volRect = {(float)volX, (float)(y + 2), (float)volW, (float)(cellH - 4)};
                    bool volHovered = CheckCollisionPointRec(mouse, volRect);
                    
                    // Background
                    DrawRectangleRec(volRect, (Color){15, 15, 15, 255});
                    // Fill based on volume (use track color tint)
                    int fillW = (int)((volW - 2) * vol);
                    if (fillW > 0) {
                        Color fillColor = melodyTrackColors[track];
                        fillColor.r = (unsigned char)fminf(255, fillColor.r * (volHovered ? 1.5f : 1.2f));
                        fillColor.g = (unsigned char)fminf(255, fillColor.g * (volHovered ? 1.5f : 1.2f));
                        fillColor.b = (unsigned char)fminf(255, fillColor.b * (volHovered ? 1.5f : 1.2f));
                        DrawRectangle(volX + 1, y + 3, fillW, cellH - 6, fillColor);
                    }
                    DrawRectangleLinesEx(volRect, 1, volHovered ? WHITE : (Color){50, 50, 60, 255});
                    
                    if (volHovered) {
                        if (mouseDown) {
                            float newVol = (mouse.x - volX) / (float)volW;
                            seq.trackVolume[absTrack] = clampf(newVol, 0.0f, 1.0f);
                        }
                        if (mouseWheel != 0) {
                            seq.trackVolume[absTrack] = clampf(vol + mouseWheel * 0.05f, 0.0f, 1.0f);
                        }
                    }
                }
                
                for (int step = 0; step < SEQ_MAX_STEPS; step++) {
                    int x = gridX + labelW + step * cellW;
                    Rectangle cell = {(float)x, (float)y, (float)cellW - 2, (float)cellH - 2};
                    
                    bool isInRange = step < trackLen;
                    int note = p->melodyNote[track][step];
                    bool hasNote = (note != SEQ_NOTE_OFF) && isInRange;
                    bool isCurrent = (step == seq.melodyStep[track]) && seq.playing && isInRange;
                    bool isHovered = CheckCollisionPointRec(mouse, cell);
                    bool isSelected = selectedIsMelody && selectedTrack == track && selectedStep == step;
                    bool hasProb = hasNote && p->melodyProbability[track][step] < 1.0f;
                    bool hasCond = hasNote && p->melodyCondition[track][step] != COND_ALWAYS;
                    bool hasSlide = hasNote && p->melodySlide[track][step];
                    bool hasAccent = hasNote && p->melodyAccent[track][step];
                    
                    Color bgColor = (step / 4) % 2 == 0 ? (Color){35, 38, 45, 255} : (Color){28, 30, 38, 255};
                    if (!isInRange) bgColor = (Color){55, 55, 58, 255};  // Light gray for out-of-range
                    
                    Color cellColor = bgColor;
                    if (hasNote) {
                        float vel = p->melodyVelocity[track][step];
                        cellColor = melodyTrackColors[track];
                        cellColor.r = (unsigned char)(cellColor.r * (0.5f + vel * 0.5f));
                        cellColor.g = (unsigned char)(cellColor.g * (0.5f + vel * 0.5f));
                        cellColor.b = (unsigned char)(cellColor.b * (0.5f + vel * 0.5f));
                        if (isCurrent) {
                            cellColor.r = (unsigned char)fminf(255, cellColor.r + 50);
                            cellColor.g = (unsigned char)fminf(255, cellColor.g + 50);
                            cellColor.b = (unsigned char)fminf(255, cellColor.b + 50);
                        }
                    } else if (isCurrent) {
                        cellColor = (Color){50, 50, 60, 255};
                    }
                    if (isHovered && isInRange) {
                        cellColor.r = (unsigned char)fminf(255, cellColor.r + 25);
                        cellColor.g = (unsigned char)fminf(255, cellColor.g + 25);
                        cellColor.b = (unsigned char)fminf(255, cellColor.b + 25);
                    }
                    
                    DrawRectangleRec(cell, cellColor);
                    
                    Color borderColor = isInRange ? (Color){55, 55, 65, 255} : (Color){30, 30, 35, 255};
                    if (isSelected) borderColor = ORANGE;
                    DrawRectangleLinesEx(cell, isSelected ? 2 : 1, borderColor);
                    
                    // Note name display
                    if (hasNote) {
                        const char* noteName = seqNoteName(note);
                        DrawTextShadow(noteName, x + 2, y + 3, 9, WHITE);
                    }
                    
                    // Indicators
                    if (hasProb) {
                        DrawCircle(x + 4, y + cellH - 4, 2, (Color){150, 100, 200, 200});
                    }
                    if (hasCond) {
                        DrawRectangle(x + cellW - 6, y + cellH - 5, 3, 3, (Color){200, 150, 50, 255});
                    }
                    // Slide indicator (arrow/line on left)
                    if (hasSlide) {
                        DrawLine(x + 1, y + 3, x + 1, y + cellH - 4, (Color){100, 200, 255, 255});
                        DrawTriangle(
                            (Vector2){(float)(x + 1), (float)(y + 3)},
                            (Vector2){(float)(x + 4), (float)(y + 6)},
                            (Vector2){(float)(x - 2), (float)(y + 6)},
                            (Color){100, 200, 255, 255}
                        );
                    }
                    // Accent indicator (bright top bar)
                    if (hasAccent) {
                        DrawRectangle(x + 1, y + 1, cellW - 4, 2, (Color){255, 100, 100, 255});
                    }
                    // P-lock indicator (purple diamond in bottom-right)
                    if (hasNote && seqHasPLocks(p, track, step)) {
                        int dx = x + cellW - 8;
                        int dy = y + cellH - 8;
                        DrawTriangle(
                            (Vector2){(float)dx, (float)(dy + 3)},
                            (Vector2){(float)(dx + 3), (float)dy},
                            (Vector2){(float)(dx + 6), (float)(dy + 3)},
                            (Color){180, 120, 255, 255}
                        );
                        DrawTriangle(
                            (Vector2){(float)dx, (float)(dy + 3)},
                            (Vector2){(float)(dx + 3), (float)(dy + 6)},
                            (Vector2){(float)(dx + 6), (float)(dy + 3)},
                            (Color){180, 120, 255, 255}
                        );
                    }
                    
                    // Note input via scroll wheel
                    if (isHovered && isInRange && fabsf(mouseWheel) > 0.1f) {
                        if (hasNote) {
                            // Adjust note by semitone or octave (shift)
                            int delta = (int)mouseWheel;
                            if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
                                delta *= 12;  // Octave
                            }
                            p->melodyNote[track][step] = clampf(note + delta, 24, 96);  // C1 to C7
                        }
                    }
                    
                    if (isHovered && isInRange) {
                        if (mouseClicked) {
                            selectedTrack = track;
                            selectedStep = step;
                            selectedIsMelody = true;
                            if (!hasNote) {
                                // Default note based on track: Bass=C2, Lead=C4, Chord=C3
                                int defaultNotes[3] = {36, 60, 48};
                                p->melodyNote[track][step] = defaultNotes[track];
                            }
                            ui_consume_click();
                        }
                        if (rightClicked && hasNote) {
                            p->melodyNote[track][step] = SEQ_NOTE_OFF;
                            if (selectedIsMelody && selectedTrack == track && selectedStep == step) {
                                selectedTrack = -1;
                                selectedStep = -1;
                            }
                            ui_consume_click();
                        }
                    }
                }
            }
            
            // === STEP INSPECTOR PANEL ===
            int totalRows = SEQ_DRUM_TRACKS + SEQ_MELODY_TRACKS + 1;  // +1 for separator
            int inspY = melodyStartY + SEQ_MELODY_TRACKS * cellH + 8;
            
            bool showDrumInspector = !selectedIsMelody && selectedTrack >= 0 && selectedStep >= 0 
                                     && p->drumSteps[selectedTrack][selectedStep];
            bool showMelodyInspector = selectedIsMelody && selectedTrack >= 0 && selectedStep >= 0 
                                       && p->melodyNote[selectedTrack][selectedStep] != SEQ_NOTE_OFF;
            
            if (showDrumInspector) {
                int inspX = gridX;
                int inspW = labelW + SEQ_MAX_STEPS * cellW;
                int absTrack = selectedTrack;  // Drums use tracks 0-3 directly
                bool hasPLocks = seqHasPLocks(p, absTrack, selectedStep);
                int inspH = hasPLocks ? 70 : 45;  // Taller when p-locks present
                
                DrawRectangle(inspX, inspY, inspW, inspH, (Color){35, 35, 40, 255});
                DrawRectangleLinesEx((Rectangle){(float)inspX, (float)inspY, (float)inspW, (float)inspH}, 1, ORANGE);
                
                DrawTextShadow(TextFormat("Step %d - %s", selectedStep + 1, seq.drumTrackNames[selectedTrack]), 
                             inspX + 8, inspY + 4, 12, ORANGE);
                
                int row1Y = inspY + 18;
                int colSpacing = 130;
                
                DraggableFloat(inspX + 10, row1Y, "Vel", &p->drumVelocity[selectedTrack][selectedStep], 0.02f, 0.0f, 1.0f);
                
                float pitchSemitones = p->drumPitch[selectedTrack][selectedStep] * 12.0f;
                DraggableFloat(inspX + 10 + colSpacing, row1Y, "Pitch", &pitchSemitones, 0.5f, -12.0f, 12.0f);
                p->drumPitch[selectedTrack][selectedStep] = pitchSemitones / 12.0f;
                
                DraggableFloat(inspX + 10 + colSpacing * 2, row1Y, "Prob", &p->drumProbability[selectedTrack][selectedStep], 0.02f, 0.0f, 1.0f);
                
                // Condition
                {
                    int condX = inspX + 10 + colSpacing * 3;
                    DrawTextShadow("Cond:", condX, row1Y, 12, LIGHTGRAY);
                    Rectangle condRect = {(float)(condX + 40), (float)(row1Y - 2), 55, 16};
                    bool condHovered = CheckCollisionPointRec(mouse, condRect);
                    DrawRectangleRec(condRect, condHovered ? (Color){60, 60, 70, 255} : (Color){45, 45, 55, 255});
                    DrawRectangleLinesEx(condRect, 1, condHovered ? YELLOW : (Color){80, 80, 80, 255});
                    int cond = p->drumCondition[selectedTrack][selectedStep];
                    DrawTextShadow(conditionNames[cond], condX + 44, row1Y, 10, WHITE);
                    if (condHovered && mouseClicked) {
                        p->drumCondition[selectedTrack][selectedStep] = (cond + 1) % COND_COUNT;
                        ui_consume_click();
                    }
                    if (condHovered && rightClicked) {
                        p->drumCondition[selectedTrack][selectedStep] = (cond - 1 + COND_COUNT) % COND_COUNT;
                        ui_consume_click();
                    }
                }
                
                // P-Lock row for drums
                int row2Y = row1Y + 22;
                DrawTextShadow("P-Lock:", inspX + 10, row2Y, 10, (Color){255, 180, 100, 255});
                
                // P-lock: Decay (maps to drum-specific decay param)
                {
                    int px = inspX + 65;
                    float decay = seqGetPLock(p, absTrack, selectedStep, PLOCK_DECAY, -1.0f);
                    bool isLocked = (decay >= 0.0f);
                    if (!isLocked) {
                        // Get default decay based on drum type
                        switch (selectedTrack) {
                            case 0: decay = drumParams.kickDecay; break;
                            case 1: decay = drumParams.snareDecay; break;
                            case 2: decay = drumParams.hhDecayClosed; break;
                            case 3: decay = drumParams.clapDecay; break;
                            default: decay = 0.3f; break;
                        }
                    }
                    
                    DrawTextShadow("Dec:", px, row2Y, 10, isLocked ? (Color){255, 180, 100, 255} : DARKGRAY);
                    Rectangle rect = {(float)(px + 28), (float)(row2Y - 2), 50, 14};
                    bool hovered = CheckCollisionPointRec(mouse, rect);
                    DrawRectangleRec(rect, hovered ? (Color){50, 50, 60, 255} : (Color){35, 35, 45, 255});
                    DrawRectangleLinesEx(rect, 1, isLocked ? (Color){255, 180, 100, 255} : (Color){60, 60, 70, 255});
                    DrawTextShadow(TextFormat("%.2f", decay), px + 32, row2Y, 9, isLocked ? WHITE : DARKGRAY);
                    
                    if (hovered) {
                        float wheel = GetMouseWheelMove();
                        if (fabsf(wheel) > 0.1f) {
                            decay = fminf(2.0f, fmaxf(0.01f, decay + wheel * 0.05f));
                            seqSetPLock(p, absTrack, selectedStep, PLOCK_DECAY, decay);
                        }
                        if (rightClicked) {
                            seqClearPLock(p, absTrack, selectedStep, PLOCK_DECAY);
                            ui_consume_click();
                        }
                    }
                }
                
                // P-lock: Pitch offset
                {
                    int px = inspX + 160;
                    float pitch = seqGetPLock(p, absTrack, selectedStep, PLOCK_PITCH_OFFSET, -100.0f);
                    bool isLocked = (pitch > -99.0f);
                    if (!isLocked) pitch = 0.0f;
                    
                    DrawTextShadow("Pit:", px, row2Y, 10, isLocked ? (Color){255, 180, 100, 255} : DARKGRAY);
                    Rectangle rect = {(float)(px + 25), (float)(row2Y - 2), 45, 14};
                    bool hovered = CheckCollisionPointRec(mouse, rect);
                    DrawRectangleRec(rect, hovered ? (Color){50, 50, 60, 255} : (Color){35, 35, 45, 255});
                    DrawRectangleLinesEx(rect, 1, isLocked ? (Color){255, 180, 100, 255} : (Color){60, 60, 70, 255});
                    DrawTextShadow(TextFormat("%+.1f", pitch), px + 28, row2Y, 9, isLocked ? WHITE : DARKGRAY);
                    
                    if (hovered) {
                        float wheel = GetMouseWheelMove();
                        if (fabsf(wheel) > 0.1f) {
                            pitch = fminf(12.0f, fmaxf(-12.0f, pitch + wheel * 0.5f));
                            seqSetPLock(p, absTrack, selectedStep, PLOCK_PITCH_OFFSET, pitch);
                        }
                        if (rightClicked) {
                            seqClearPLock(p, absTrack, selectedStep, PLOCK_PITCH_OFFSET);
                            ui_consume_click();
                        }
                    }
                }
                
                // P-lock: Volume
                {
                    int px = inspX + 245;
                    float vol = seqGetPLock(p, absTrack, selectedStep, PLOCK_VOLUME, -1.0f);
                    bool isLocked = (vol >= 0.0f);
                    if (!isLocked) vol = p->drumVelocity[selectedTrack][selectedStep];
                    
                    DrawTextShadow("Vol:", px, row2Y, 10, isLocked ? (Color){255, 180, 100, 255} : DARKGRAY);
                    Rectangle rect = {(float)(px + 28), (float)(row2Y - 2), 45, 14};
                    bool hovered = CheckCollisionPointRec(mouse, rect);
                    DrawRectangleRec(rect, hovered ? (Color){50, 50, 60, 255} : (Color){35, 35, 45, 255});
                    DrawRectangleLinesEx(rect, 1, isLocked ? (Color){255, 180, 100, 255} : (Color){60, 60, 70, 255});
                    DrawTextShadow(TextFormat("%.2f", vol), px + 32, row2Y, 9, isLocked ? WHITE : DARKGRAY);
                    
                    if (hovered) {
                        float wheel = GetMouseWheelMove();
                        if (fabsf(wheel) > 0.1f) {
                            vol = fminf(1.0f, fmaxf(0.0f, vol + wheel * 0.02f));
                            seqSetPLock(p, absTrack, selectedStep, PLOCK_VOLUME, vol);
                        }
                        if (rightClicked) {
                            seqClearPLock(p, absTrack, selectedStep, PLOCK_VOLUME);
                            ui_consume_click();
                        }
                    }
                }
                
                // P-lock: Tone (brightness/character)
                {
                    int px = inspX + 330;
                    float tone = seqGetPLock(p, absTrack, selectedStep, PLOCK_TONE, -1.0f);
                    bool isLocked = (tone >= 0.0f);
                    if (!isLocked) {
                        // Get default tone based on drum type
                        switch (selectedTrack) {
                            case 0: tone = drumParams.kickTone; break;
                            case 1: tone = drumParams.snareTone; break;
                            case 2: tone = drumParams.hhTone; break;
                            case 3: tone = drumParams.clapTone; break;
                            default: tone = 0.5f; break;
                        }
                    }
                    
                    DrawTextShadow("Tone:", px, row2Y, 10, isLocked ? (Color){255, 180, 100, 255} : DARKGRAY);
                    Rectangle rect = {(float)(px + 35), (float)(row2Y - 2), 45, 14};
                    bool hovered = CheckCollisionPointRec(mouse, rect);
                    DrawRectangleRec(rect, hovered ? (Color){50, 50, 60, 255} : (Color){35, 35, 45, 255});
                    DrawRectangleLinesEx(rect, 1, isLocked ? (Color){255, 180, 100, 255} : (Color){60, 60, 70, 255});
                    DrawTextShadow(TextFormat("%.2f", tone), px + 39, row2Y, 9, isLocked ? WHITE : DARKGRAY);
                    
                    if (hovered) {
                        float wheel = GetMouseWheelMove();
                        if (fabsf(wheel) > 0.1f) {
                            tone = fminf(1.0f, fmaxf(0.0f, tone + wheel * 0.02f));
                            seqSetPLock(p, absTrack, selectedStep, PLOCK_TONE, tone);
                        }
                        if (rightClicked) {
                            seqClearPLock(p, absTrack, selectedStep, PLOCK_TONE);
                            ui_consume_click();
                        }
                    }
                }
                
                // P-lock: Punch (kick: punch pitch, snare: snappy, clap: spread)
                // Only show for kick, snare, clap (tracks 0, 1, 3)
                if (selectedTrack == 0 || selectedTrack == 1 || selectedTrack == 3) {
                    int px = inspX + 425;
                    float punch = seqGetPLock(p, absTrack, selectedStep, PLOCK_PUNCH, -1.0f);
                    bool isLocked = (punch >= 0.0f);
                    if (!isLocked) {
                        // Get default punch based on drum type
                        switch (selectedTrack) {
                            case 0: punch = (drumParams.kickPunchPitch - 50.0f) / 250.0f; break;  // Normalize from 50-300
                            case 1: punch = drumParams.snareSnappy; break;
                            case 3: punch = drumParams.clapSpread / 0.03f; break;  // Normalize from 0-0.03
                            default: punch = 0.5f; break;
                        }
                    }
                    
                    const char* punchLabel;
                    switch (selectedTrack) {
                        case 0:  punchLabel = "Punch:"; break;
                        case 1:  punchLabel = "Snap:"; break;
                        default: punchLabel = "Spread:"; break;
                    }
                    DrawTextShadow(punchLabel, px, row2Y, 10, isLocked ? (Color){255, 180, 100, 255} : DARKGRAY);
                    Rectangle rect = {(float)(px + 42), (float)(row2Y - 2), 45, 14};
                    bool hovered = CheckCollisionPointRec(mouse, rect);
                    DrawRectangleRec(rect, hovered ? (Color){50, 50, 60, 255} : (Color){35, 35, 45, 255});
                    DrawRectangleLinesEx(rect, 1, isLocked ? (Color){255, 180, 100, 255} : (Color){60, 60, 70, 255});
                    DrawTextShadow(TextFormat("%.2f", punch), px + 46, row2Y, 9, isLocked ? WHITE : DARKGRAY);
                    
                    if (hovered) {
                        float wheel = GetMouseWheelMove();
                        if (fabsf(wheel) > 0.1f) {
                            punch = fminf(1.0f, fmaxf(0.0f, punch + wheel * 0.02f));
                            seqSetPLock(p, absTrack, selectedStep, PLOCK_PUNCH, punch);
                        }
                        if (rightClicked) {
                            seqClearPLock(p, absTrack, selectedStep, PLOCK_PUNCH);
                            ui_consume_click();
                        }
                    }
                }
                
                // P-lock: Nudge (per-step timing offset)
                {
                    int px = inspX + 525;
                    float nudge = seqGetPLock(p, absTrack, selectedStep, PLOCK_TIME_NUDGE, -100.0f);
                    bool isLocked = (nudge > -99.0f);
                    if (!isLocked) nudge = 0.0f;
                    
                    DrawTextShadow("Nudge:", px, row2Y, 10, isLocked ? (Color){100, 180, 255, 255} : DARKGRAY);
                    Rectangle rect = {(float)(px + 40), (float)(row2Y - 2), 40, 14};
                    bool hovered = CheckCollisionPointRec(mouse, rect);
                    DrawRectangleRec(rect, hovered ? (Color){50, 50, 60, 255} : (Color){35, 35, 45, 255});
                    DrawRectangleLinesEx(rect, 1, isLocked ? (Color){100, 180, 255, 255} : (Color){60, 60, 70, 255});
                    DrawTextShadow(TextFormat("%+.0f", nudge), px + 48, row2Y, 9, isLocked ? WHITE : DARKGRAY);
                    
                    if (hovered) {
                        float wheel = GetMouseWheelMove();
                        if (fabsf(wheel) > 0.1f) {
                            nudge = fminf(12.0f, fmaxf(-12.0f, nudge + wheel * 1.0f));
                            seqSetPLock(p, absTrack, selectedStep, PLOCK_TIME_NUDGE, nudge);
                        }
                        if (rightClicked) {
                            seqClearPLock(p, absTrack, selectedStep, PLOCK_TIME_NUDGE);
                            ui_consume_click();
                        }
                    }
                }
                
                // P-lock: Flam (ghost note before main hit)
                {
                    int px = inspX + 615;
                    float flamTime = seqGetPLock(p, absTrack, selectedStep, PLOCK_FLAM_TIME, 0.0f);
                    float flamVel = seqGetPLock(p, absTrack, selectedStep, PLOCK_FLAM_VELOCITY, 0.5f);
                    bool isLocked = (flamTime > 0.0f);
                    
                    DrawTextShadow("Flam:", px, row2Y, 10, isLocked ? (Color){255, 100, 180, 255} : DARKGRAY);
                    Rectangle rect = {(float)(px + 32), (float)(row2Y - 2), 35, 14};
                    bool hovered = CheckCollisionPointRec(mouse, rect);
                    DrawRectangleRec(rect, hovered ? (Color){50, 50, 60, 255} : (Color){35, 35, 45, 255});
                    DrawRectangleLinesEx(rect, 1, isLocked ? (Color){255, 100, 180, 255} : (Color){60, 60, 70, 255});
                    DrawTextShadow(isLocked ? TextFormat("%.0f", flamTime) : "off", px + 36, row2Y, 9, isLocked ? WHITE : DARKGRAY);
                    
                    if (hovered) {
                        float wheel = GetMouseWheelMove();
                        if (fabsf(wheel) > 0.1f) {
                            if (!isLocked && wheel > 0) {
                                // Turn on flam with default 30ms
                                seqSetPLock(p, absTrack, selectedStep, PLOCK_FLAM_TIME, 30.0f);
                                seqSetPLock(p, absTrack, selectedStep, PLOCK_FLAM_VELOCITY, 0.5f);
                            } else if (isLocked) {
                                flamTime = fminf(80.0f, fmaxf(10.0f, flamTime + wheel * 5.0f));
                                seqSetPLock(p, absTrack, selectedStep, PLOCK_FLAM_TIME, flamTime);
                            }
                        }
                        if (rightClicked) {
                            seqClearPLock(p, absTrack, selectedStep, PLOCK_FLAM_TIME);
                            seqClearPLock(p, absTrack, selectedStep, PLOCK_FLAM_VELOCITY);
                            ui_consume_click();
                        }
                    }
                }
                
                // Clear all p-locks button
                if (hasPLocks) {
                    int clearX = inspX + 700;
                    Rectangle clearRect = {(float)clearX, (float)(row2Y - 2), 50, 14};
                    bool clearHovered = CheckCollisionPointRec(mouse, clearRect);
                    DrawRectangleRec(clearRect, clearHovered ? (Color){80, 50, 50, 255} : (Color){50, 35, 35, 255});
                    DrawRectangleLinesEx(clearRect, 1, (Color){150, 80, 80, 255});
                    DrawTextShadow("Clear", clearX + 10, row2Y, 9, (Color){200, 100, 100, 255});
                    if (clearHovered && mouseClicked) {
                        seqClearStepPLocks(p, absTrack, selectedStep);
                        ui_consume_click();
                    }
                }
            }
            
            if (showMelodyInspector) {
                int inspX = gridX;
                int inspW = labelW + SEQ_MAX_STEPS * cellW;
                int absTrack = SEQ_DRUM_TRACKS + selectedTrack;  // Absolute track index for P-locks
                bool hasPLocks = seqHasPLocks(p, absTrack, selectedStep);
                int inspH = hasPLocks ? 70 : 45;  // Taller when p-locks present
                
                DrawRectangle(inspX, inspY, inspW, inspH, (Color){35, 38, 45, 255});
                DrawRectangleLinesEx((Rectangle){(float)inspX, (float)inspY, (float)inspW, (float)inspH}, 1, melodyTrackColors[selectedTrack]);
                
                int note = p->melodyNote[selectedTrack][selectedStep];
                DrawTextShadow(TextFormat("Step %d - %s [%s]", selectedStep + 1, seq.melodyTrackNames[selectedTrack], seqNoteName(note)), 
                             inspX + 8, inspY + 4, 12, melodyTrackColors[selectedTrack]);
                
                int row1Y = inspY + 18;
                int colSpacing = 110;
                
                // Note (as int for simplicity)
                {
                    DrawTextShadow("Note:", inspX + 10, row1Y, 12, LIGHTGRAY);
                    Rectangle noteRect = {(float)(inspX + 50), (float)(row1Y - 2), 40, 16};
                    bool noteHovered = CheckCollisionPointRec(mouse, noteRect);
                    DrawRectangleRec(noteRect, noteHovered ? (Color){60, 60, 70, 255} : (Color){45, 45, 55, 255});
                    DrawRectangleLinesEx(noteRect, 1, noteHovered ? YELLOW : (Color){80, 80, 80, 255});
                    DrawTextShadow(seqNoteName(note), inspX + 54, row1Y, 10, WHITE);
                    if (noteHovered) {
                        float wheel = GetMouseWheelMove();
                        if (fabsf(wheel) > 0.1f) {
                            int delta = (int)wheel;
                            if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) delta *= 12;
                            p->melodyNote[selectedTrack][selectedStep] = clampf(note + delta, 24, 96);
                        }
                    }
                }
                
                DraggableFloat(inspX + 10 + colSpacing, row1Y, "Vel", &p->melodyVelocity[selectedTrack][selectedStep], 0.02f, 0.0f, 1.0f);
                
                // Gate length
                {
                    int gateX = inspX + 10 + colSpacing * 2;
                    DrawTextShadow("Gate:", gateX, row1Y, 12, LIGHTGRAY);
                    Rectangle gateRect = {(float)(gateX + 40), (float)(row1Y - 2), 30, 16};
                    bool gateHovered = CheckCollisionPointRec(mouse, gateRect);
                    DrawRectangleRec(gateRect, gateHovered ? (Color){60, 60, 70, 255} : (Color){45, 45, 55, 255});
                    DrawRectangleLinesEx(gateRect, 1, gateHovered ? YELLOW : (Color){80, 80, 80, 255});
                    int gate = p->melodyGate[selectedTrack][selectedStep];
                    DrawTextShadow(TextFormat("%d", gate), gateX + 48, row1Y, 10, WHITE);
                    if (gateHovered && mouseClicked) {
                        p->melodyGate[selectedTrack][selectedStep] = (gate % 16) + 1;
                        ui_consume_click();
                    }
                    if (gateHovered && rightClicked) {
                        p->melodyGate[selectedTrack][selectedStep]--;
                        if (p->melodyGate[selectedTrack][selectedStep] < 1) p->melodyGate[selectedTrack][selectedStep] = 16;
                        ui_consume_click();
                    }
                }
                
                DraggableFloat(inspX + 10 + colSpacing * 3, row1Y, "Prob", &p->melodyProbability[selectedTrack][selectedStep], 0.02f, 0.0f, 1.0f);
                
                // Condition
                {
                    int condX = inspX + 10 + colSpacing * 4;
                    DrawTextShadow("Cond:", condX, row1Y, 12, LIGHTGRAY);
                    Rectangle condRect = {(float)(condX + 40), (float)(row1Y - 2), 55, 16};
                    bool condHovered = CheckCollisionPointRec(mouse, condRect);
                    DrawRectangleRec(condRect, condHovered ? (Color){60, 60, 70, 255} : (Color){45, 45, 55, 255});
                    DrawRectangleLinesEx(condRect, 1, condHovered ? YELLOW : (Color){80, 80, 80, 255});
                    int cond = p->melodyCondition[selectedTrack][selectedStep];
                    DrawTextShadow(conditionNames[cond], condX + 44, row1Y, 10, WHITE);
                    if (condHovered && mouseClicked) {
                        p->melodyCondition[selectedTrack][selectedStep] = (cond + 1) % COND_COUNT;
                        ui_consume_click();
                    }
                    if (condHovered && rightClicked) {
                        p->melodyCondition[selectedTrack][selectedStep] = (cond - 1 + COND_COUNT) % COND_COUNT;
                        ui_consume_click();
                    }
                }
                
                // 303-style Slide toggle
                {
                    int slideX = inspX + 10 + colSpacing * 5;
                    bool hasSlide = p->melodySlide[selectedTrack][selectedStep];
                    Rectangle slideRect = {(float)slideX, (float)(row1Y - 2), 45, 16};
                    bool slideHovered = CheckCollisionPointRec(mouse, slideRect);
                    Color slideBg = hasSlide ? (Color){60, 100, 130, 255} : (Color){45, 45, 55, 255};
                    if (slideHovered) slideBg = (Color){70, 110, 140, 255};
                    DrawRectangleRec(slideRect, slideBg);
                    DrawRectangleLinesEx(slideRect, 1, hasSlide ? (Color){100, 200, 255, 255} : (Color){80, 80, 80, 255});
                    DrawTextShadow("Slide", slideX + 6, row1Y, 10, hasSlide ? (Color){100, 200, 255, 255} : LIGHTGRAY);
                    if (slideHovered && mouseClicked) {
                        p->melodySlide[selectedTrack][selectedStep] = !hasSlide;
                        ui_consume_click();
                    }
                }
                
                // 303-style Accent toggle
                {
                    int accentX = inspX + 10 + colSpacing * 5 + 50;
                    bool hasAccent = p->melodyAccent[selectedTrack][selectedStep];
                    Rectangle accentRect = {(float)accentX, (float)(row1Y - 2), 50, 16};
                    bool accentHovered = CheckCollisionPointRec(mouse, accentRect);
                    Color accentBg = hasAccent ? (Color){130, 60, 60, 255} : (Color){45, 45, 55, 255};
                    if (accentHovered) accentBg = (Color){150, 70, 70, 255};
                    DrawRectangleRec(accentRect, accentBg);
                    DrawRectangleLinesEx(accentRect, 1, hasAccent ? (Color){255, 100, 100, 255} : (Color){80, 80, 80, 255});
                    DrawTextShadow("Accent", accentX + 4, row1Y, 10, hasAccent ? (Color){255, 100, 100, 255} : LIGHTGRAY);
                    if (accentHovered && mouseClicked) {
                        p->melodyAccent[selectedTrack][selectedStep] = !hasAccent;
                        ui_consume_click();
                    }
                }
                
                // P-Lock row (second row)
                int row2Y = row1Y + 22;
                DrawTextShadow("P-Lock:", inspX + 10, row2Y, 10, (Color){180, 120, 255, 255});
                
                // Get patch index for this track
                int patchIdx = (selectedTrack == 0) ? PATCH_BASS : 
                               (selectedTrack == 1) ? PATCH_LEAD : PATCH_CHORD;
                SynthPatch *patch = &patches[patchIdx];
                
                // P-lock: Cutoff
                {
                    int px = inspX + 65;
                    float cutoff = seqGetPLock(p, absTrack, selectedStep, PLOCK_FILTER_CUTOFF, -1.0f);
                    bool isLocked = (cutoff >= 0.0f);
                    if (!isLocked) cutoff = patch->p_filterCutoff;
                    
                    DrawTextShadow("Cut:", px, row2Y, 10, isLocked ? (Color){180, 120, 255, 255} : DARKGRAY);
                    Rectangle rect = {(float)(px + 28), (float)(row2Y - 2), 50, 14};
                    bool hovered = CheckCollisionPointRec(mouse, rect);
                    DrawRectangleRec(rect, hovered ? (Color){50, 50, 60, 255} : (Color){35, 35, 45, 255});
                    DrawRectangleLinesEx(rect, 1, isLocked ? (Color){180, 120, 255, 255} : (Color){60, 60, 70, 255});
                    DrawTextShadow(TextFormat("%.0f", cutoff * 8000.0f), px + 32, row2Y, 9, isLocked ? WHITE : DARKGRAY);
                    
                    if (hovered) {
                        float wheel = GetMouseWheelMove();
                        if (fabsf(wheel) > 0.1f) {
                            cutoff = fminf(1.0f, fmaxf(0.0f, cutoff + wheel * 0.02f));
                            seqSetPLock(p, absTrack, selectedStep, PLOCK_FILTER_CUTOFF, cutoff);
                        }
                        if (rightClicked) {
                            seqClearPLock(p, absTrack, selectedStep, PLOCK_FILTER_CUTOFF);
                            ui_consume_click();
                        }
                    }
                }
                
                // P-lock: Resonance
                {
                    int px = inspX + 160;
                    float reso = seqGetPLock(p, absTrack, selectedStep, PLOCK_FILTER_RESO, -1.0f);
                    bool isLocked = (reso >= 0.0f);
                    if (!isLocked) reso = patch->p_filterResonance;
                    
                    DrawTextShadow("Res:", px, row2Y, 10, isLocked ? (Color){180, 120, 255, 255} : DARKGRAY);
                    Rectangle rect = {(float)(px + 28), (float)(row2Y - 2), 40, 14};
                    bool hovered = CheckCollisionPointRec(mouse, rect);
                    DrawRectangleRec(rect, hovered ? (Color){50, 50, 60, 255} : (Color){35, 35, 45, 255});
                    DrawRectangleLinesEx(rect, 1, isLocked ? (Color){180, 120, 255, 255} : (Color){60, 60, 70, 255});
                    DrawTextShadow(TextFormat("%.2f", reso), px + 32, row2Y, 9, isLocked ? WHITE : DARKGRAY);
                    
                    if (hovered) {
                        float wheel = GetMouseWheelMove();
                        if (fabsf(wheel) > 0.1f) {
                            reso = fminf(1.0f, fmaxf(0.0f, reso + wheel * 0.02f));
                            seqSetPLock(p, absTrack, selectedStep, PLOCK_FILTER_RESO, reso);
                        }
                        if (rightClicked) {
                            seqClearPLock(p, absTrack, selectedStep, PLOCK_FILTER_RESO);
                            ui_consume_click();
                        }
                    }
                }
                
                // P-lock: Filter Env
                {
                    int px = inspX + 245;
                    float fenv = seqGetPLock(p, absTrack, selectedStep, PLOCK_FILTER_ENV, -1.0f);
                    bool isLocked = (fenv >= 0.0f);
                    if (!isLocked) fenv = patch->p_filterEnvAmt;
                    
                    DrawTextShadow("FEnv:", px, row2Y, 10, isLocked ? (Color){180, 120, 255, 255} : DARKGRAY);
                    Rectangle rect = {(float)(px + 35), (float)(row2Y - 2), 40, 14};
                    bool hovered = CheckCollisionPointRec(mouse, rect);
                    DrawRectangleRec(rect, hovered ? (Color){50, 50, 60, 255} : (Color){35, 35, 45, 255});
                    DrawRectangleLinesEx(rect, 1, isLocked ? (Color){180, 120, 255, 255} : (Color){60, 60, 70, 255});
                    DrawTextShadow(TextFormat("%.2f", fenv), px + 39, row2Y, 9, isLocked ? WHITE : DARKGRAY);
                    
                    if (hovered) {
                        float wheel = GetMouseWheelMove();
                        if (fabsf(wheel) > 0.1f) {
                            fenv = fminf(1.0f, fmaxf(0.0f, fenv + wheel * 0.02f));
                            seqSetPLock(p, absTrack, selectedStep, PLOCK_FILTER_ENV, fenv);
                        }
                        if (rightClicked) {
                            seqClearPLock(p, absTrack, selectedStep, PLOCK_FILTER_ENV);
                            ui_consume_click();
                        }
                    }
                }
                
                // P-lock: Decay
                {
                    int px = inspX + 340;
                    float decay = seqGetPLock(p, absTrack, selectedStep, PLOCK_DECAY, -1.0f);
                    bool isLocked = (decay >= 0.0f);
                    if (!isLocked) decay = patch->p_decay;
                    
                    DrawTextShadow("Dec:", px, row2Y, 10, isLocked ? (Color){180, 120, 255, 255} : DARKGRAY);
                    Rectangle rect = {(float)(px + 28), (float)(row2Y - 2), 40, 14};
                    bool hovered = CheckCollisionPointRec(mouse, rect);
                    DrawRectangleRec(rect, hovered ? (Color){50, 50, 60, 255} : (Color){35, 35, 45, 255});
                    DrawRectangleLinesEx(rect, 1, isLocked ? (Color){180, 120, 255, 255} : (Color){60, 60, 70, 255});
                    DrawTextShadow(TextFormat("%.2f", decay), px + 32, row2Y, 9, isLocked ? WHITE : DARKGRAY);
                    
                    if (hovered) {
                        float wheel = GetMouseWheelMove();
                        if (fabsf(wheel) > 0.1f) {
                            decay = fminf(2.0f, fmaxf(0.01f, decay + wheel * 0.05f));
                            seqSetPLock(p, absTrack, selectedStep, PLOCK_DECAY, decay);
                        }
                        if (rightClicked) {
                            seqClearPLock(p, absTrack, selectedStep, PLOCK_DECAY);
                            ui_consume_click();
                        }
                    }
                }
                
                // P-lock: Pitch offset
                {
                    int px = inspX + 430;
                    float pitch = seqGetPLock(p, absTrack, selectedStep, PLOCK_PITCH_OFFSET, -100.0f);
                    bool isLocked = (pitch > -99.0f);
                    if (!isLocked) pitch = 0.0f;
                    
                    DrawTextShadow("Pit:", px, row2Y, 10, isLocked ? (Color){180, 120, 255, 255} : DARKGRAY);
                    Rectangle rect = {(float)(px + 25), (float)(row2Y - 2), 40, 14};
                    bool hovered = CheckCollisionPointRec(mouse, rect);
                    DrawRectangleRec(rect, hovered ? (Color){50, 50, 60, 255} : (Color){35, 35, 45, 255});
                    DrawRectangleLinesEx(rect, 1, isLocked ? (Color){180, 120, 255, 255} : (Color){60, 60, 70, 255});
                    DrawTextShadow(TextFormat("%+.1f", pitch), px + 28, row2Y, 9, isLocked ? WHITE : DARKGRAY);
                    
                    if (hovered) {
                        float wheel = GetMouseWheelMove();
                        if (fabsf(wheel) > 0.1f) {
                            pitch = fminf(12.0f, fmaxf(-12.0f, pitch + wheel * 0.5f));
                            seqSetPLock(p, absTrack, selectedStep, PLOCK_PITCH_OFFSET, pitch);
                        }
                        if (rightClicked) {
                            seqClearPLock(p, absTrack, selectedStep, PLOCK_PITCH_OFFSET);
                            ui_consume_click();
                        }
                    }
                }
                
                // P-lock: Volume
                {
                    int px = inspX + 520;
                    float vol = seqGetPLock(p, absTrack, selectedStep, PLOCK_VOLUME, -1.0f);
                    bool isLocked = (vol >= 0.0f);
                    if (!isLocked) vol = p->melodyVelocity[selectedTrack][selectedStep];
                    
                    DrawTextShadow("Vol:", px, row2Y, 10, isLocked ? (Color){180, 120, 255, 255} : DARKGRAY);
                    Rectangle rect = {(float)(px + 28), (float)(row2Y - 2), 40, 14};
                    bool hovered = CheckCollisionPointRec(mouse, rect);
                    DrawRectangleRec(rect, hovered ? (Color){50, 50, 60, 255} : (Color){35, 35, 45, 255});
                    DrawRectangleLinesEx(rect, 1, isLocked ? (Color){180, 120, 255, 255} : (Color){60, 60, 70, 255});
                    DrawTextShadow(TextFormat("%.2f", vol), px + 32, row2Y, 9, isLocked ? WHITE : DARKGRAY);
                    
                    if (hovered) {
                        float wheel = GetMouseWheelMove();
                        if (fabsf(wheel) > 0.1f) {
                            vol = fminf(1.0f, fmaxf(0.0f, vol + wheel * 0.02f));
                            seqSetPLock(p, absTrack, selectedStep, PLOCK_VOLUME, vol);
                        }
                        if (rightClicked) {
                            seqClearPLock(p, absTrack, selectedStep, PLOCK_VOLUME);
                            ui_consume_click();
                        }
                    }
                }
                
                // Clear all p-locks button
                if (hasPLocks) {
                    int clearX = inspX + 610;
                    Rectangle clearRect = {(float)clearX, (float)(row2Y - 2), 50, 14};
                    bool clearHovered = CheckCollisionPointRec(mouse, clearRect);
                    DrawRectangleRec(clearRect, clearHovered ? (Color){80, 50, 50, 255} : (Color){50, 35, 35, 255});
                    DrawRectangleLinesEx(clearRect, 1, (Color){150, 80, 80, 255});
                    DrawTextShadow("Clear", clearX + 10, row2Y, 9, (Color){200, 100, 100, 255});
                    if (clearHovered && mouseClicked) {
                        seqClearStepPLocks(p, absTrack, selectedStep);
                        ui_consume_click();
                    }
                }
                
                // Note Pool row (third row) - for generative/varied melodies
                int row3Y = row2Y + 18;
                NotePool* notePool = &p->melodyNotePool[selectedTrack][selectedStep];
                
                // Note Pool enable toggle
                {
                    int poolX = inspX + 10;
                    Rectangle poolRect = {(float)poolX, (float)(row3Y - 2), 55, 16};
                    bool poolHovered = CheckCollisionPointRec(mouse, poolRect);
                    Color poolBg = notePool->enabled ? (Color){80, 100, 60, 255} : (Color){45, 45, 55, 255};
                    if (poolHovered) poolBg = (Color){90, 110, 70, 255};
                    DrawRectangleRec(poolRect, poolBg);
                    DrawRectangleLinesEx(poolRect, 1, notePool->enabled ? (Color){150, 200, 100, 255} : (Color){80, 80, 80, 255});
                    DrawTextShadow("Pool", poolX + 12, row3Y, 10, notePool->enabled ? (Color){150, 200, 100, 255} : LIGHTGRAY);
                    if (poolHovered && mouseClicked) {
                        notePool->enabled = !notePool->enabled;
                        ui_consume_click();
                    }
                }
                
                // Only show pool options if enabled
                if (notePool->enabled) {
                    // Chord type selector
                    {
                        int chordX = inspX + 75;
                        DrawTextShadow("Chord:", chordX, row3Y, 10, (Color){150, 200, 100, 255});
                        Rectangle chordRect = {(float)(chordX + 45), (float)(row3Y - 2), 50, 14};
                        bool chordHovered = CheckCollisionPointRec(mouse, chordRect);
                        DrawRectangleRec(chordRect, chordHovered ? (Color){50, 60, 50, 255} : (Color){35, 45, 35, 255});
                        DrawRectangleLinesEx(chordRect, 1, (Color){100, 150, 80, 255});
                        DrawTextShadow(chordTypeNames[notePool->chordType], chordX + 49, row3Y, 9, WHITE);
                        
                        if (chordHovered) {
                            float wheel = GetMouseWheelMove();
                            if (wheel > 0.1f) {
                                seqCycleNotePoolChord(selectedTrack, selectedStep, -1);
                            } else if (wheel < -0.1f) {
                                seqCycleNotePoolChord(selectedTrack, selectedStep, 1);
                            }
                            if (mouseClicked) {
                                seqCycleNotePoolChord(selectedTrack, selectedStep, 1);
                                ui_consume_click();
                            }
                        }
                    }
                    
                    // Pick mode selector
                    {
                        int pickX = inspX + 185;
                        DrawTextShadow("Pick:", pickX, row3Y, 10, (Color){150, 200, 100, 255});
                        Rectangle pickRect = {(float)(pickX + 35), (float)(row3Y - 2), 45, 14};
                        bool pickHovered = CheckCollisionPointRec(mouse, pickRect);
                        DrawRectangleRec(pickRect, pickHovered ? (Color){50, 60, 50, 255} : (Color){35, 45, 35, 255});
                        DrawRectangleLinesEx(pickRect, 1, (Color){100, 150, 80, 255});
                        DrawTextShadow(pickModeNames[notePool->pickMode], pickX + 39, row3Y, 9, WHITE);
                        
                        if (pickHovered) {
                            float wheel = GetMouseWheelMove();
                            if (wheel > 0.1f) {
                                seqCycleNotePoolPick(selectedTrack, selectedStep, -1);
                            } else if (wheel < -0.1f) {
                                seqCycleNotePoolPick(selectedTrack, selectedStep, 1);
                            }
                            if (mouseClicked) {
                                seqCycleNotePoolPick(selectedTrack, selectedStep, 1);
                                ui_consume_click();
                            }
                        }
                    }
                    
                    // Show what notes are in the pool
                    {
                        int previewX = inspX + 290;
                        int notes[NOTE_POOL_MAX_NOTES];
                        int baseNote = p->melodyNote[selectedTrack][selectedStep];
                        bool useMinor = shouldUseMinor(baseNote);
                        int noteCount = buildChordNotes(baseNote, (ChordType)notePool->chordType, useMinor, notes);
                        
                        DrawTextShadow("Notes:", previewX, row3Y, 10, GRAY);
                        char noteStr[64] = "";
                        for (int i = 0; i < noteCount && i < 4; i++) {
                            char buf[8];
                            int note = notes[i];
                            int octave = note / 12 - 1;
                            int noteIdx = note % 12;
                            static const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
                            snprintf(buf, sizeof(buf), "%s%s%d", i > 0 ? " " : "", noteNames[noteIdx], octave);
                            strncat(noteStr, buf, sizeof(noteStr) - strlen(noteStr) - 1);
                        }
                        DrawTextShadow(noteStr, previewX + 45, row3Y, 9, (Color){180, 220, 140, 255});
                    }
                    
                    // Reset cycle button
                    {
                        int resetX = inspX + 520;
                        Rectangle resetRect = {(float)resetX, (float)(row3Y - 2), 45, 14};
                        bool resetHovered = CheckCollisionPointRec(mouse, resetRect);
                        DrawRectangleRec(resetRect, resetHovered ? (Color){60, 60, 70, 255} : (Color){40, 40, 50, 255});
                        DrawRectangleLinesEx(resetRect, 1, (Color){80, 80, 90, 255});
                        DrawTextShadow("Reset", resetX + 6, row3Y, 9, LIGHTGRAY);
                        if (resetHovered && mouseClicked) {
                            seqResetNotePoolCycle(selectedTrack, selectedStep);
                            ui_consume_click();
                        }
                    }
                }
            }
            
            // Dilla timing controls moved to top bar [Groove] dropdown
        }
        
        // Keep wave type in sync with selector
        cp->p_waveType = selectedWave;
        
        ui_update();
        
        // Help tooltip overlay (drawn last so it's on top of everything)
        if (g_showHelpOverlay) {
            int helpBoxX = 430;
            int helpBoxY = 47;
            DrawRectangle(helpBoxX, helpBoxY, 250, 110, (Color){25, 25, 30, 255});
            DrawRectangleLinesEx((Rectangle){(float)helpBoxX, (float)helpBoxY, 250, 110}, 2, (Color){100, 100, 120, 255});
            DrawTextShadow("SFX: 1-6  Drums: 7-0,-,=", helpBoxX + 10, helpBoxY + 10, 12, LIGHTGRAY);
            DrawTextShadow("Notes: ASDFGHJKL + WERTYUIOP", helpBoxX + 10, helpBoxY + 26, 12, LIGHTGRAY);
            DrawTextShadow(TextFormat("Octave: %d (Z/X)", currentOctave), helpBoxX + 10, helpBoxY + 42, 12, YELLOW);
            DrawTextShadow("Voice: V=vowel B=babble N=speak", helpBoxX + 10, helpBoxY + 58, 12, LIGHTGRAY);
            DrawTextShadow("SPACE = Play/Stop", helpBoxX + 10, helpBoxY + 74, 12, LIGHTGRAY);
            DrawTextShadow("Drag values, scroll to adjust", helpBoxX + 10, helpBoxY + 90, 12, GRAY);
        }
        
        // Sequencer help overlay
        if (g_showSeqHelpOverlay) {
            int helpBoxX = 20;
            int helpBoxY = SCREEN_HEIGHT - 420;
            int helpW = 320;
            int helpH = 145;
            DrawRectangle(helpBoxX, helpBoxY, helpW, helpH, (Color){25, 25, 30, 255});
            DrawRectangleLinesEx((Rectangle){(float)helpBoxX, (float)helpBoxY, (float)helpW, (float)helpH}, 2, (Color){100, 100, 120, 255});
            DrawTextShadow("SEQUENCER", helpBoxX + 10, helpBoxY + 8, 12, ORANGE);
            DrawTextShadow("Drums: click=toggle, drag=velocity", helpBoxX + 10, helpBoxY + 26, 11, LIGHTGRAY);
            DrawTextShadow("Drums: shift+drag=pitch", helpBoxX + 10, helpBoxY + 40, 11, LIGHTGRAY);
            DrawTextShadow("Melody: click=add note, scroll=pitch", helpBoxX + 10, helpBoxY + 54, 11, LIGHTGRAY);
            DrawTextShadow("Melody: right-click=delete note", helpBoxX + 10, helpBoxY + 68, 11, LIGHTGRAY);
            DrawTextShadow("Pattern: click=queue, shift+click=copy", helpBoxX + 10, helpBoxY + 86, 11, (Color){100, 200, 100, 255});
            DrawTextShadow("Pattern: right-click=clear", helpBoxX + 10, helpBoxY + 100, 11, (Color){100, 200, 100, 255});
            DrawTextShadow("P-Lock: scroll to edit, right-click=clear", helpBoxX + 10, helpBoxY + 118, 11, (Color){255, 180, 100, 255});
            DrawTextShadow("Scene: click=load, shift+click=save", helpBoxX + 10, helpBoxY + 132, 11, (Color){100, 150, 255, 255});
        }
        
        EndDrawing();
    }
    
    UnloadAudioStream(stream);
    CloseAudioDevice();
    UnloadFont(font);
    CloseWindow();
    
    return 0;
}
