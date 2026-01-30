// PixelSynth Demo - Chiptune synth with 808 drums
// Notes: ASDFGHJKL (white) + WERTYUIOP (black), Z/X = octave
// SFX: 1-6, Drums: 7-0,-,=

#include "../../vendor/raylib.h"
#include "../../assets/fonts/comic_embedded.h"
#define UI_IMPLEMENTATION
#include "../../shared/ui.h"
#include <math.h>
#include <string.h>

#define SCREEN_WIDTH 1140
#define SCREEN_HEIGHT 700
#define SAMPLE_RATE 44100
#define MAX_SAMPLES_PER_UPDATE 4096

// ============================================================================
// INCLUDE ENGINES
// ============================================================================

#include "engines/synth.h"    // Synth voices, wavetables, formant, SFX
#include "engines/scw_data.h" // Embedded single-cycle waveforms (run `make scw_embed` to regenerate)
#include "engines/drums.h"    // 808-style drum machine
#include "engines/effects.h"  // Distortion, delay, tape, bitcrusher
#include "sequencer.h"        // Drum step sequencer

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
        noiseState = noiseState * 1103515245 + 12345;
        const char* syl = babbleSyllables[(noiseState >> 16) % numBabbleSyllables];
        while (*syl && pos < SPEECH_MAX - 2) {
            text[pos++] = *syl++;
        }
        noiseState = noiseState * 1103515245 + 12345;
        if ((noiseState >> 16) % 4 == 0 && pos < SPEECH_MAX - 2) {
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
        
        noiseState = noiseState * 1103515245 + 12345;
        float randVar = 1.0f + ((float)(noiseState >> 16) / 65535.0f - 0.5f) * sq->pitchVariation;
        
        // Apply intonation contour (position-based pitch shift)
        float progress = (float)sq->index / (float)sq->length;
        float intonationMod = 1.0f + sq->intonation * 0.3f * progress;  // Up to ±30% pitch shift at end
        
        float baseFreq = 200.0f * sq->basePitch * pitchMod * randVar * intonationMod;
        
        Voice *v = &voices[sq->voiceIndex];
        
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
    Voice *v = &voices[sq->voiceIndex];
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

static void SynthCallback(void *buffer, unsigned int frames) {
    double startTime = GetTime();
    
    short *d = (short *)buffer;
    float dt = 1.0f / SAMPLE_RATE;
    
    for (unsigned int i = 0; i < frames; i++) {
        float sample = 0.0f;
        
        // Process synth voices
        for (int v = 0; v < NUM_VOICES; v++) {
            sample += processVoice(&voices[v], SAMPLE_RATE);
        }
        
        // Process drums
        sample += processDrums(dt);
        
        sample *= masterVolume;
        
        // Process effects
        sample = processEffects(sample, dt);
        
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
    return c0 * powf(2.0f, totalSemitones / 12.0f);
}

// ============================================================================
// UI COLUMN VISIBILITY
// ============================================================================

static bool showWaveColumn = true;
static bool showLfoColumn = true;
static bool showDrumsColumn = true;
static bool showEffectsColumn = true;

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
    memset(voices, 0, sizeof(voices));
    memset(drumVoices, 0, sizeof(drumVoices));
    initDrumParams();
    initEffects();
    initSequencer(drumKickFull, drumSnareFull, drumClosedHHFull, drumClapFull);
    
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
            seq.playing = !seq.playing;
            if (seq.playing) {
                resetSequencer();
            }
        }
        updateSequencer(GetFrameTime());
        
        // Piano keyboard input (ASDFGHJKL = white keys, WERTYUIOP = black keys)
        for (size_t i = 0; i < NUM_PIANO_KEYS; i++) {
            if (IsKeyPressed(pianoKeys[i].key)) {
                float freq = semitoneToFreq(pianoKeys[i].semitone, currentOctave);
                if (selectedWave == WAVE_PLUCK) {
                    pianoKeyVoices[i] = playPluck(freq, pluckBrightness, pluckDamping);
                } else if (selectedWave == WAVE_ADDITIVE) {
                    pianoKeyVoices[i] = playAdditive(freq, (AdditivePreset)additivePreset);
                } else if (selectedWave == WAVE_MALLET) {
                    pianoKeyVoices[i] = playMallet(freq, (MalletPreset)malletPreset);
                } else if (selectedWave == WAVE_VOICE) {
                    VowelType vowel = (VowelType)voiceVowel;
                    if (voiceRandomVowel) {
                        noiseState = noiseState * 1103515245 + 12345;
                        vowel = (VowelType)((noiseState >> 16) % 5);
                    }
                    pianoKeyVoices[i] = playVowel(freq, vowel);
                } else if (selectedWave == WAVE_GRANULAR) {
                    pianoKeyVoices[i] = playGranular(freq, granularScwIndex);
                } else if (selectedWave == WAVE_FM) {
                    pianoKeyVoices[i] = playFM(freq);
                } else if (selectedWave == WAVE_PD) {
                    pianoKeyVoices[i] = playPD(freq);
                } else if (selectedWave == WAVE_MEMBRANE) {
                    pianoKeyVoices[i] = playMembrane(freq, (MembranePreset)membranePreset);
                } else if (selectedWave == WAVE_BIRD) {
                    pianoKeyVoices[i] = playBird(freq, (BirdType)birdType);
                } else {
                    pianoKeyVoices[i] = playNote(freq, (WaveType)selectedWave);
                }
            }
            if (IsKeyReleased(pianoKeys[i].key) && pianoKeyVoices[i] >= 0) {
                // Pluck and Mallet can ring out or be damped based on damp setting
                if (selectedWave == WAVE_PLUCK && pluckDamp > 0.01f) {
                    voices[pianoKeyVoices[i]].release = 0.01f + (1.0f - pluckDamp) * 0.5f;
                    releaseNote(pianoKeyVoices[i]);
                } else if (selectedWave == WAVE_MALLET && malletDamp > 0.01f) {
                    voices[pianoKeyVoices[i]].release = 0.01f + (1.0f - malletDamp) * 0.5f;
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
        
        // Controls info
        DrawTextEx(font, "SFX: 1-6  Drums: 7-0,-,= or Numpad", (Vector2){20, 55}, 12, 1, LIGHTGRAY);
        DrawTextEx(font, "Notes: ASDFGHJKL + WERTYUIOP", (Vector2){20, 70}, 12, 1, LIGHTGRAY);
        DrawTextEx(font, TextFormat("Octave: %d (Z/X)", currentOctave), (Vector2){20, 85}, 12, 1, YELLOW);
        DrawTextEx(font, "Voice: V=vowel B=babble N=speak", (Vector2){20, 100}, 12, 1, LIGHTGRAY);
        DrawTextEx(font, "SPACE = Play/Stop Sequencer", (Vector2){20, 115}, 12, 1, seq.playing ? GREEN : LIGHTGRAY);
        
        // Voice indicators
        DrawTextEx(font, "Voices:", (Vector2){20, 135}, 12, 1, GRAY);
        for (int i = 0; i < NUM_VOICES; i++) {
            Color c = DARKGRAY;
            if (voices[i].envStage == 4) c = ORANGE;
            else if (voices[i].envStage > 0) c = GREEN;
            DrawRectangle(75 + i * 18, 135, 14, 12, c);
        }
        
        // Performance stats
        double bufferTimeMs = (double)audioFrameCount / SAMPLE_RATE * 1000.0;
        double cpuPercent = (audioTimeUs / 1000.0) / bufferTimeMs * 100.0;
        DrawTextEx(font, TextFormat("Audio: %.0fus (%.1f%%)  FPS: %d", 
                 audioTimeUs, cpuPercent, GetFPS()), (Vector2){20, 155}, 12, 1, GRAY);
        
        ToggleBool(20, 175, "SFX Randomize", &sfxRandomize);
        
        // Speaking indicator
        if (speechQueue.active) {
            DrawTextEx(font, "Speaking...", (Vector2){20, 200}, 14, 1, GREEN);
        }
        
        // === COLUMN 1: Wave Type + Wave-specific settings ===
        UIColumn col1 = ui_column(250, 170, 20);
        
        if (SectionHeader(col1.x, col1.y, "Wave", &showWaveColumn)) {
            col1.y += 18;
            ui_col_cycle(&col1, "Type", waveNames, 14, &selectedWave);
            ui_col_space(&col1, 4);
            
            if (selectedWave == WAVE_SQUARE) {
                ui_col_sublabel(&col1, "PWM:", ORANGE);
                ui_col_float(&col1, "Width", &notePulseWidth, 0.05f, 0.1f, 0.9f);
                ui_col_float(&col1, "Rate", &notePwmRate, 0.5f, 0.1f, 20.0f);
                ui_col_float(&col1, "Depth", &notePwmDepth, 0.02f, 0.0f, 0.4f);
            }
            
            if (selectedWave == WAVE_SCW && scwCount > 0) {
                ui_col_sublabel(&col1, "Wavetable:", ORANGE);
                const char* scwNames[SCW_MAX_SLOTS];
                for (int i = 0; i < scwCount; i++) scwNames[i] = scwTables[i].name;
                ui_col_cycle(&col1, "SCW", scwNames, scwCount, &noteScwIndex);
            }
            
            if (selectedWave == WAVE_VOICE) {
                ui_col_sublabel(&col1, "Formant:", ORANGE);
                ui_col_cycle(&col1, "Vowel", vowelNames, 5, &voiceVowel);
                ui_col_toggle(&col1, "Random", &voiceRandomVowel);
                ui_col_float(&col1, "Pitch", &voicePitch, 0.1f, 0.3f, 2.0f);
                ui_col_float(&col1, "Speed", &voiceSpeed, 1.0f, 4.0f, 20.0f);
                ui_col_float(&col1, "Formant", &voiceFormantShift, 0.05f, 0.5f, 1.5f);
                ui_col_float(&col1, "Breath", &voiceBreathiness, 0.05f, 0.0f, 1.0f);
                ui_col_float(&col1, "Buzz", &voiceBuzziness, 0.05f, 0.0f, 1.0f);
                ui_col_space(&col1, 4);
                ui_col_sublabel(&col1, "Extras:", ORANGE);
                ui_col_toggle(&col1, "Consonant", &voiceConsonant);
                if (voiceConsonant) {
                    ui_col_float(&col1, "ConsAmt", &voiceConsonantAmt, 0.05f, 0.0f, 1.0f);
                }
                ui_col_toggle(&col1, "Nasal", &voiceNasal);
                if (voiceNasal) {
                    ui_col_float(&col1, "NasalAmt", &voiceNasalAmt, 0.05f, 0.0f, 1.0f);
                }
                ui_col_space(&col1, 4);
                ui_col_sublabel(&col1, "Pitch Env:", ORANGE);
                ui_col_float(&col1, "Bend", &voicePitchEnv, 0.5f, -12.0f, 12.0f);
                ui_col_float(&col1, "Time", &voicePitchEnvTime, 0.02f, 0.02f, 0.5f);
                ui_col_float(&col1, "Curve", &voicePitchEnvCurve, 0.1f, -1.0f, 1.0f);
            }
            
            if (selectedWave == WAVE_PLUCK) {
                ui_col_sublabel(&col1, "Pluck:", ORANGE);
                ui_col_float(&col1, "Bright", &pluckBrightness, 0.05f, 0.0f, 1.0f);
                ui_col_float(&col1, "Sustain", &pluckDamping, 0.0002f, 0.995f, 0.9998f);
                ui_col_float(&col1, "Damp", &pluckDamp, 0.05f, 0.0f, 1.0f);
            }
            
            if (selectedWave == WAVE_ADDITIVE) {
                ui_col_sublabel(&col1, "Additive:", ORANGE);
                ui_col_cycle(&col1, "Preset", additivePresetNames, ADDITIVE_PRESET_COUNT, &additivePreset);
                ui_col_float(&col1, "Bright", &additiveBrightness, 0.05f, 0.0f, 1.0f);
                ui_col_float(&col1, "Shimmer", &additiveShimmer, 0.05f, 0.0f, 1.0f);
                ui_col_float(&col1, "Inharm", &additiveInharmonicity, 0.005f, 0.0f, 0.1f);
            }
            
            if (selectedWave == WAVE_MALLET) {
                ui_col_sublabel(&col1, "Mallet:", ORANGE);
                ui_col_cycle(&col1, "Preset", malletPresetNames, MALLET_PRESET_COUNT, &malletPreset);
                ui_col_float(&col1, "Stiff", &malletStiffness, 0.05f, 0.0f, 1.0f);
                ui_col_float(&col1, "Hard", &malletHardness, 0.05f, 0.0f, 1.0f);
                ui_col_float(&col1, "Strike", &malletStrikePos, 0.05f, 0.0f, 1.0f);
                ui_col_float(&col1, "Reson", &malletResonance, 0.05f, 0.0f, 1.0f);
                ui_col_float(&col1, "Damp", &malletDamp, 0.05f, 0.0f, 1.0f);
                ui_col_float(&col1, "Tremolo", &malletTremolo, 0.05f, 0.0f, 1.0f);
                ui_col_float(&col1, "TremSpd", &malletTremoloRate, 0.5f, 1.0f, 12.0f);
            }
            
            if (selectedWave == WAVE_GRANULAR && scwCount > 0) {
                ui_col_sublabel(&col1, "Granular:", ORANGE);
                const char* scwNames[SCW_MAX_SLOTS];
                for (int i = 0; i < scwCount; i++) scwNames[i] = scwTables[i].name;
                ui_col_cycle(&col1, "Source", scwNames, scwCount, &granularScwIndex);
                ui_col_float(&col1, "Size", &granularGrainSize, 5.0f, 10.0f, 200.0f);
                ui_col_float(&col1, "Density", &granularDensity, 2.0f, 1.0f, 100.0f);
                ui_col_float(&col1, "Position", &granularPosition, 0.05f, 0.0f, 1.0f);
                ui_col_float(&col1, "PosRand", &granularPosRandom, 0.05f, 0.0f, 1.0f);
                ui_col_float(&col1, "Pitch", &granularPitch, 0.1f, 0.25f, 4.0f);
                ui_col_float(&col1, "PitRand", &granularPitchRandom, 0.5f, 0.0f, 12.0f);
                ui_col_float(&col1, "AmpRand", &granularAmpRandom, 0.05f, 0.0f, 1.0f);
                ui_col_toggle(&col1, "Freeze", &granularFreeze);
            }
            
            if (selectedWave == WAVE_FM) {
                ui_col_sublabel(&col1, "FM Synth:", ORANGE);
                ui_col_float(&col1, "Ratio", &fmModRatio, 0.5f, 0.5f, 16.0f);
                ui_col_float(&col1, "Index", &fmModIndex, 0.1f, 0.0f, 10.0f);
                ui_col_float(&col1, "Feedback", &fmFeedback, 0.05f, 0.0f, 1.0f);
            }
            
            if (selectedWave == WAVE_PD) {
                ui_col_sublabel(&col1, "Phase Dist:", ORANGE);
                ui_col_cycle(&col1, "Wave", pdWaveNames, PD_WAVE_COUNT, &pdWaveType);
                ui_col_float(&col1, "Distort", &pdDistortion, 0.05f, 0.0f, 1.0f);
            }
            
            if (selectedWave == WAVE_MEMBRANE) {
                ui_col_sublabel(&col1, "Membrane:", ORANGE);
                ui_col_cycle(&col1, "Preset", membranePresetNames, MEMBRANE_COUNT, &membranePreset);
                ui_col_float(&col1, "Damping", &membraneDamping, 0.05f, 0.1f, 1.0f);
                ui_col_float(&col1, "Strike", &membraneStrike, 0.05f, 0.0f, 1.0f);
                ui_col_float(&col1, "Bend", &membraneBend, 0.02f, 0.0f, 0.5f);
                ui_col_float(&col1, "BendDcy", &membraneBendDecay, 0.01f, 0.02f, 0.3f);
            }
            
            if (selectedWave == WAVE_BIRD) {
                ui_col_sublabel(&col1, "Bird:", ORANGE);
                ui_col_cycle(&col1, "Type", birdTypeNames, BIRD_COUNT, &birdType);
                ui_col_float(&col1, "Range", &birdChirpRange, 0.1f, 0.5f, 2.0f);
                ui_col_float(&col1, "Harmonic", &birdHarmonics, 0.05f, 0.0f, 1.0f);
                ui_col_space(&col1, 4);
                ui_col_sublabel(&col1, "Trill:", ORANGE);
                ui_col_float(&col1, "Rate", &birdTrillRate, 1.0f, 0.0f, 30.0f);
                ui_col_float(&col1, "Depth", &birdTrillDepth, 0.2f, 0.0f, 5.0f);
                ui_col_space(&col1, 4);
                ui_col_sublabel(&col1, "Flutter:", ORANGE);
                ui_col_float(&col1, "AM Rate", &birdAmRate, 1.0f, 0.0f, 20.0f);
                ui_col_float(&col1, "AM Depth", &birdAmDepth, 0.05f, 0.0f, 1.0f);
            }
        }
        
        // === COLUMN 2: Synth (shared settings) ===
        UIColumn col2 = ui_column(430, 20, 20);
        
        {
            DrawTextEx(font, "[-] Synth", (Vector2){(float)col2.x, (float)col2.y}, 14, 1, WHITE);
            col2.y += 18;
            
            ui_col_sublabel(&col2, "Envelope:", ORANGE);
            ui_col_float(&col2, "Attack", &noteAttack, 0.5f, 0.001f, 2.0f);
            ui_col_float(&col2, "Decay", &noteDecay, 0.5f, 0.0f, 2.0f);
            ui_col_float(&col2, "Sustain", &noteSustain, 0.5f, 0.0f, 1.0f);
            ui_col_float(&col2, "Release", &noteRelease, 0.5f, 0.01f, 3.0f);
            ui_col_space(&col2, 4);
            
            ui_col_sublabel(&col2, "Vibrato:", ORANGE);
            ui_col_float(&col2, "Rate", &noteVibratoRate, 0.5f, 0.5f, 15.0f);
            ui_col_float(&col2, "Depth", &noteVibratoDepth, 0.2f, 0.0f, 2.0f);
            ui_col_space(&col2, 4);
            
            ui_col_sublabel(&col2, "Filter:", ORANGE);
            ui_col_float(&col2, "Cutoff", &noteFilterCutoff, 0.05f, 0.01f, 1.0f);
            ui_col_float(&col2, "Reso", &noteFilterResonance, 0.05f, 0.0f, 1.0f);
            ui_col_float(&col2, "EnvAmt", &noteFilterEnvAmt, 0.05f, -1.0f, 1.0f);
            ui_col_float(&col2, "EnvAtk", &noteFilterEnvAttack, 0.01f, 0.001f, 0.5f);
            ui_col_float(&col2, "EnvDcy", &noteFilterEnvDecay, 0.05f, 0.01f, 2.0f);
            ui_col_space(&col2, 4);
            
            ui_col_sublabel(&col2, "Volume:", ORANGE);
            ui_col_float(&col2, "Note", &noteVolume, 0.05f, 0.0f, 1.0f);
            ui_col_float(&col2, "Master", &masterVolume, 0.05f, 0.0f, 1.0f);
            
            // Mono/Glide - only show for wave types that support it
            if (selectedWave != WAVE_PLUCK && selectedWave != WAVE_MALLET) {
                ui_col_space(&col2, 4);
                ui_col_sublabel(&col2, "Mono/Glide:", ORANGE);
                ui_col_toggle(&col2, "Mono", &monoMode);
                if (monoMode) {
                    ui_col_float(&col2, "Glide", &glideTime, 0.02f, 0.01f, 1.0f);
                }
            }
        }
        
        // === COLUMN 3: LFOs ===
        UIColumn col3 = ui_column(610, 20, 20);
        
        if (SectionHeader(col3.x, col3.y, "LFOs", &showLfoColumn)) {
            col3.y += 18;
            
            static const char* lfoShapeNames[] = {"Sine", "Tri", "Sqr", "Saw", "S&H"};
            
            ui_col_sublabel(&col3, "Filter:", ORANGE);
            ui_col_float(&col3, "Rate", &noteFilterLfoRate, 0.5f, 0.0f, 20.0f);
            ui_col_float(&col3, "Depth", &noteFilterLfoDepth, 0.05f, 0.0f, 2.0f);
            ui_col_cycle(&col3, "Shape", lfoShapeNames, 5, &noteFilterLfoShape);
            ui_col_space(&col3, 4);
            
            ui_col_sublabel(&col3, "Resonance:", ORANGE);
            ui_col_float(&col3, "Rate", &noteResoLfoRate, 0.5f, 0.0f, 20.0f);
            ui_col_float(&col3, "Depth", &noteResoLfoDepth, 0.05f, 0.0f, 1.0f);
            ui_col_cycle(&col3, "Shape", lfoShapeNames, 5, &noteResoLfoShape);
            ui_col_space(&col3, 4);
            
            ui_col_sublabel(&col3, "Amplitude:", ORANGE);
            ui_col_float(&col3, "Rate", &noteAmpLfoRate, 0.5f, 0.0f, 20.0f);
            ui_col_float(&col3, "Depth", &noteAmpLfoDepth, 0.05f, 0.0f, 1.0f);
            ui_col_cycle(&col3, "Shape", lfoShapeNames, 5, &noteAmpLfoShape);
            ui_col_space(&col3, 4);
            
            ui_col_sublabel(&col3, "Pitch:", ORANGE);
            ui_col_float(&col3, "Rate", &notePitchLfoRate, 0.5f, 0.0f, 20.0f);
            ui_col_float(&col3, "Depth", &notePitchLfoDepth, 0.05f, 0.0f, 1.0f);
            ui_col_cycle(&col3, "Shape", lfoShapeNames, 5, &notePitchLfoShape);
        }
        
        // === COLUMN 4: Drums ===
        UIColumn col4 = ui_column(790, 20, 20);
        
        if (SectionHeader(col4.x, col4.y, "Drums", &showDrumsColumn)) {
            col4.y += 18;
            ui_col_float(&col4, "Volume", &drumVolume, 0.05f, 0.0f, 1.0f);
            ui_col_space(&col4, 4);
            
            ui_col_sublabel(&col4, "Kick (7):", ORANGE);
            ui_col_float(&col4, "Pitch", &drumParams.kickPitch, 3.0f, 30.0f, 100.0f);
            ui_col_float(&col4, "Decay", &drumParams.kickDecay, 0.07f, 0.1f, 1.5f);
            ui_col_float(&col4, "Punch", &drumParams.kickPunchPitch, 10.0f, 80.0f, 300.0f);
            ui_col_float(&col4, "Click", &drumParams.kickClick, 0.05f, 0.0f, 1.0f);
            ui_col_float(&col4, "Tone", &drumParams.kickTone, 0.05f, 0.0f, 1.0f);
            ui_col_space(&col4, 4);
            
            ui_col_sublabel(&col4, "Snare (8):", ORANGE);
            ui_col_float(&col4, "Pitch", &drumParams.snarePitch, 10.0f, 100.0f, 350.0f);
            ui_col_float(&col4, "Decay", &drumParams.snareDecay, 0.03f, 0.05f, 0.6f);
            ui_col_float(&col4, "Snappy", &drumParams.snareSnappy, 0.05f, 0.0f, 1.0f);
            ui_col_float(&col4, "Tone", &drumParams.snareTone, 0.05f, 0.0f, 1.0f);
            ui_col_space(&col4, 4);
            
            ui_col_sublabel(&col4, "HiHat (0/-):", ORANGE);
            ui_col_float(&col4, "Closed", &drumParams.hhDecayClosed, 0.01f, 0.01f, 0.2f);
            ui_col_float(&col4, "Open", &drumParams.hhDecayOpen, 0.05f, 0.1f, 1.0f);
            ui_col_float(&col4, "Tone", &drumParams.hhTone, 0.05f, 0.0f, 1.0f);
            ui_col_space(&col4, 4);
            
            ui_col_sublabel(&col4, "Clap (9):", ORANGE);
            ui_col_float(&col4, "Decay", &drumParams.clapDecay, 0.03f, 0.1f, 0.6f);
            ui_col_float(&col4, "Spread", &drumParams.clapSpread, 0.001f, 0.005f, 0.03f);
        }
        
        // === COLUMN 5: Effects ===
        UIColumn col5 = ui_column(970, 20, 20);
        
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
        }
        
        // === DRUM SEQUENCER GRID ===
        {
            static bool isDragging = false;
            static bool isDraggingPitch = false;
            static int dragTrack = -1;
            static int dragStep = -1;
            static float dragStartY = 0.0f;
            static float dragStartVal = 0.0f;
            
            int gridX = 20;
            int gridY = SCREEN_HEIGHT - 130;
            int cellW = 24;
            int cellH = 22;
            int labelW = 50;
            int lengthW = 30;
            
            DrawTextShadow("Drum Sequencer - drag=velocity, shift+drag=pitch, right-click=delete", gridX, gridY - 25, 14, YELLOW);
            
            // Play/Stop button
            if (PushButton(gridX + labelW + SEQ_MAX_STEPS * cellW + lengthW + 15, gridY - 25, seq.playing ? "Stop" : "Play")) {
                seq.playing = !seq.playing;
                if (seq.playing) resetSequencer();
            }
            
            // BPM control
            DraggableFloat(gridX + labelW + SEQ_MAX_STEPS * cellW + lengthW + 75, gridY - 25, "BPM", &seq.bpm, 2.0f, 60.0f, 200.0f);
            
            // Beat markers
            for (int i = 0; i < 4; i++) {
                int x = gridX + labelW + i * 4 * cellW + 2;
                DrawTextShadow(TextFormat("%d", i + 1), x, gridY - 10, 10, GRAY);
            }
            
            DrawTextShadow("Len", gridX + labelW + SEQ_MAX_STEPS * cellW + 5, gridY - 10, 10, GRAY);
            
            Vector2 mouse = GetMousePosition();
            bool mouseClicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
            bool mouseDown = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
            bool mouseReleased = IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
            bool rightClicked = IsMouseButtonPressed(MOUSE_RIGHT_BUTTON);
            
            if (mouseReleased && isDragging) {
                isDragging = false;
                dragTrack = -1;
                dragStep = -1;
            }
            
            if (isDragging && mouseDown && dragTrack >= 0 && dragStep >= 0) {
                float deltaY = dragStartY - mouse.y;
                if (isDraggingPitch) {
                    float newPitch = dragStartVal + deltaY * 0.01f;
                    seq.pitch[dragTrack][dragStep] = clampf(newPitch, -1.0f, 1.0f);
                } else {
                    float newVel = dragStartVal + deltaY * 0.01f;
                    seq.velocity[dragTrack][dragStep] = clampf(newVel, 0.1f, 1.0f);
                }
            }
            
            for (int track = 0; track < SEQ_TRACKS; track++) {
                int y = gridY + track * cellH;
                int trackLen = seq.trackLength[track];
                
                DrawTextShadow(seq.trackNames[track], gridX, y + 4, 12, LIGHTGRAY);
                
                for (int step = 0; step < SEQ_MAX_STEPS; step++) {
                    int x = gridX + labelW + step * cellW;
                    Rectangle cell = {(float)x, (float)y, (float)cellW - 2, (float)cellH - 2};
                    
                    bool isInRange = step < trackLen;
                    bool isActive = seq.steps[track][step] && isInRange;
                    bool isCurrent = (step == seq.trackStep[track]) && seq.playing && isInRange;
                    bool isHovered = CheckCollisionPointRec(mouse, cell);
                    bool isBeingDragged = isDragging && dragTrack == track && dragStep == step;
                    bool hasPitchOffset = isActive && fabsf(seq.pitch[track][step]) > 0.01f;
                    
                    Color bgColor = (step / 4) % 2 == 0 ? (Color){40, 40, 40, 255} : (Color){30, 30, 30, 255};
                    if (!isInRange) bgColor = (Color){20, 20, 20, 255};
                    
                    Color cellColor = bgColor;
                    if (isActive) {
                        float vel = seq.velocity[track][step];
                        float pit = seq.pitch[track][step];
                        unsigned char baseG = (unsigned char)(80 + vel * 100);
                        unsigned char baseR = (unsigned char)(30 + vel * 50);
                        unsigned char baseB = (unsigned char)(30 + vel * 50);
                        if (pit < 0) {
                            baseB = (unsigned char)fminf(255, baseB + (-pit) * 80);
                            baseG = (unsigned char)(baseG * (1.0f + pit * 0.3f));
                        } else if (pit > 0) {
                            baseR = (unsigned char)fminf(255, baseR + pit * 100);
                            baseG = (unsigned char)(baseG * (1.0f - pit * 0.2f));
                        }
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
                    if (isBeingDragged) {
                        cellColor.r = (unsigned char)fminf(255, cellColor.r + 50);
                        cellColor.g = (unsigned char)fminf(255, cellColor.g + 50);
                        cellColor.b = (unsigned char)fminf(255, cellColor.b + 50);
                    }
                    
                    DrawRectangleRec(cell, cellColor);
                    DrawRectangleLinesEx(cell, 1, isInRange ? (Color){60, 60, 60, 255} : (Color){35, 35, 35, 255});
                    
                    if (hasPitchOffset) {
                        float pit = seq.pitch[track][step];
                        int triX = x + cellW - 8;
                        int triY = y + 3;
                        Color triColor = pit > 0 ? (Color){255, 150, 50, 255} : (Color){100, 150, 255, 255};
                        if (pit > 0) {
                            DrawTriangle((Vector2){(float)triX + 3, (float)triY}, 
                                        (Vector2){(float)triX, (float)triY + 5}, 
                                        (Vector2){(float)triX + 6, (float)triY + 5}, triColor);
                        } else {
                            DrawTriangle((Vector2){(float)triX, (float)triY}, 
                                        (Vector2){(float)triX + 6, (float)triY}, 
                                        (Vector2){(float)triX + 3, (float)triY + 5}, triColor);
                        }
                    }
                    
                    if (isActive && (isHovered || isBeingDragged)) {
                        if (isBeingDragged && isDraggingPitch) {
                            int semitones = (int)(seq.pitch[track][step] * 12);
                            DrawTextShadow(TextFormat("%+d", semitones), x + 2, y + 5, 10, WHITE);
                        } else {
                            int velPercent = (int)(seq.velocity[track][step] * 100);
                            DrawTextShadow(TextFormat("%d", velPercent), x + 3, y + 5, 10, WHITE);
                        }
                    }
                    
                    if (isHovered && isInRange && !isDragging) {
                        if (mouseClicked) {
                            if (isActive) {
                                isDragging = true;
                                isDraggingPitch = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
                                dragTrack = track;
                                dragStep = step;
                                dragStartY = mouse.y;
                                dragStartVal = isDraggingPitch ? seq.pitch[track][step] : seq.velocity[track][step];
                                ui_consume_click();
                            } else {
                                seq.steps[track][step] = true;
                                ui_consume_click();
                                float pitchMod = powf(2.0f, seq.pitch[track][step]);
                                seq.triggersFull[track](seq.velocity[track][step], pitchMod);
                            }
                        }
                        if (rightClicked && isActive) {
                            seq.steps[track][step] = false;
                            ui_consume_click();
                        }
                    }
                }
                
                // Length control
                int lenX = gridX + labelW + SEQ_MAX_STEPS * cellW + 5;
                Rectangle lenRect = {(float)lenX, (float)y, (float)lengthW - 2, (float)cellH - 2};
                bool lenHovered = CheckCollisionPointRec(mouse, lenRect);
                
                Color lenColor = lenHovered ? YELLOW : LIGHTGRAY;
                DrawRectangleRec(lenRect, (Color){50, 50, 50, 255});
                DrawRectangleLinesEx(lenRect, 1, (Color){80, 80, 80, 255});
                DrawTextShadow(TextFormat("%d", trackLen), lenX + 8, y + 4, 12, lenColor);
                
                if (lenHovered) {
                    if (mouseClicked) {
                        seq.trackLength[track]++;
                        if (seq.trackLength[track] > SEQ_MAX_STEPS) seq.trackLength[track] = 1;
                        ui_consume_click();
                    }
                    if (rightClicked) {
                        seq.trackLength[track]--;
                        if (seq.trackLength[track] < 1) seq.trackLength[track] = SEQ_MAX_STEPS;
                        ui_consume_click();
                    }
                }
            }
            
            // Dilla timing controls
            int dillaX = gridX + labelW;
            int dillaY = gridY + SEQ_TRACKS * cellH + 10;
            
            DrawTextShadow("Dilla Timing:", dillaX, dillaY, 12, YELLOW);
            
            DraggableInt(dillaX + 100, dillaY, "Kick", &seq.dilla.kickNudge, 0.3f, -12, 12);
            DraggableInt(dillaX + 200, dillaY, "Snare", &seq.dilla.snareDelay, 0.3f, -12, 12);
            DraggableInt(dillaX + 310, dillaY, "HiHat", &seq.dilla.hatNudge, 0.3f, -12, 12);
            DraggableInt(dillaX + 420, dillaY, "Clap", &seq.dilla.clapDelay, 0.3f, -12, 12);
            DraggableInt(dillaX + 520, dillaY, "Swing", &seq.dilla.swing, 0.3f, 0, 12);
            DraggableInt(dillaX + 630, dillaY, "Jitter", &seq.dilla.jitter, 0.3f, 0, 6);
            
            if (PushButton(dillaX + 730, dillaY, "Reset")) {
                seqResetTiming();
            }
        }
        
        ui_update();
        EndDrawing();
    }
    
    UnloadAudioStream(stream);
    CloseAudioDevice();
    UnloadFont(font);
    CloseWindow();
    
    return 0;
}
