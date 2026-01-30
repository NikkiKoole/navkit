// PixelSynth Demo - Simple chiptune synth using raylib audio callbacks
// Press 1-6 for sound effects, Q-U for notes

#include "../../vendor/raylib.h"
#include "../../assets/fonts/comic_embedded.h"
#define UI_IMPLEMENTATION
#include "../../shared/ui.h"
#include <math.h>
#include <string.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 450
#define SAMPLE_RATE 44100
#define MAX_SAMPLES_PER_UPDATE 4096

// Oscillator types
typedef enum {
    WAVE_SQUARE,
    WAVE_SAW,
    WAVE_TRIANGLE,
    WAVE_NOISE,
    WAVE_SCW,        // Single Cycle Waveform (wavetable)
    WAVE_VOICE,      // Formant synthesis for character voices
} WaveType;

// Vowel types for formant synthesis
typedef enum {
    VOWEL_A,    // "ah" as in father
    VOWEL_E,    // "eh" as in bed
    VOWEL_I,    // "ee" as in see
    VOWEL_O,    // "oh" as in go
    VOWEL_U,    // "oo" as in boot
    VOWEL_COUNT
} VowelType;

// Formant data - frequencies (F1, F2, F3) for each vowel
static const float formantFreq[VOWEL_COUNT][3] = {
    { 800,  1200,  2500 },  // A - "ah"
    { 400,  2000,  2550 },  // E - "eh"  
    { 280,  2300,  2900 },  // I - "ee"
    { 450,   800,  2500 },  // O - "oh"
    { 325,   700,  2500 },  // U - "oo"
};

static const float formantBw[VOWEL_COUNT][3] = {
    { 80,   90,  120 },  // A
    { 70,  100,  120 },  // E
    { 50,   90,  120 },  // I
    { 70,   80,  120 },  // O
    { 50,   60,  120 },  // U
};

static const float formantAmp[VOWEL_COUNT][3] = {
    { 1.0f, 0.5f, 0.3f },  // A
    { 1.0f, 0.7f, 0.3f },  // E
    { 1.0f, 0.4f, 0.2f },  // I
    { 1.0f, 0.3f, 0.2f },  // O
    { 1.0f, 0.2f, 0.1f },  // U
};

// Formant filter (bandpass for voice synthesis)
typedef struct {
    float freq;
    float bw;
    float low, band, high;
} FormantFilter;

// Voice synthesis settings
typedef struct {
    VowelType vowel;
    VowelType nextVowel;
    float vowelBlend;
    float formantShift;    // 0.5 = child, 1.0 = normal, 1.5 = deep
    float breathiness;     // Mix in noise (0-1)
    float buzziness;       // Pulse vs smooth source (0-1)
    float vibratoRate;
    float vibratoDepth;
    float vibratoPhase;
    FormantFilter formants[3];
} VoiceSettings;

// Voice structure
typedef struct {
    float frequency;
    float baseFrequency;  // Original frequency (for vibrato)
    float phase;
    float volume;
    WaveType wave;
    
    // Pulse width (for square wave, 0.1-0.9, 0.5 = square)
    float pulseWidth;
    float pwmRate;        // PWM LFO rate in Hz
    float pwmDepth;       // PWM modulation depth (0-0.4)
    float pwmPhase;       // PWM LFO phase
    
    // Vibrato (pitch LFO)
    float vibratoRate;    // Vibrato speed in Hz
    float vibratoDepth;   // Vibrato depth in semitones
    float vibratoPhase;   // Vibrato LFO phase
    
    // Simple ADSR envelope
    float attack;
    float decay;
    float sustain;
    float release;
    float envPhase;
    float envLevel;       // current envelope level (for smooth release)
    int envStage;         // 0=off, 1=attack, 2=decay, 3=sustain, 4=release
    
    // For pitch slides (SFX)
    float pitchSlide;
    
    // Simple lowpass filter (per-voice)
    float filterCutoff;   // 0.0-1.0
    float filterLp;       // Filter state (lowpass output)
    
    // Arpeggiator
    bool arpEnabled;
    float arpNotes[4];    // Up to 4 notes
    int arpCount;         // Number of notes in arp
    int arpIndex;         // Current note index
    float arpRate;        // Notes per second
    float arpTimer;       // Time accumulator
    
    // SCW (wavetable) index
    int scwIndex;         // Which loaded SCW to use
    
    // Voice/formant synthesis
    VoiceSettings voiceSettings;
} Voice;

#define NUM_VOICES 8
static Voice voices[NUM_VOICES];
static float masterVolume = 0.5f;

// SCW (Single Cycle Waveform) wavetables
#define SCW_MAX_SIZE 2048
#define SCW_MAX_SLOTS 8
typedef struct {
    float data[SCW_MAX_SIZE];
    int size;
    bool loaded;
    const char* name;
} SCWTable;
static SCWTable scwTables[SCW_MAX_SLOTS];
static int scwCount = 0;

// Load a .wav file as SCW (assumes mono, any bit depth raylib supports)
static bool loadSCW(const char* path, const char* name) {
    if (scwCount >= SCW_MAX_SLOTS) return false;
    
    Wave wav = LoadWave(path);
    if (wav.data == NULL) return false;
    
    // Convert to float samples
    int samples = wav.frameCount;
    if (samples > SCW_MAX_SIZE) samples = SCW_MAX_SIZE;
    
    SCWTable* table = &scwTables[scwCount];
    
    // Convert based on sample format
    if (wav.sampleSize == 16) {
        short* src = (short*)wav.data;
        for (int i = 0; i < samples; i++) {
            table->data[i] = (float)src[i] / 32768.0f;
        }
    } else if (wav.sampleSize == 8) {
        unsigned char* src = (unsigned char*)wav.data;
        for (int i = 0; i < samples; i++) {
            table->data[i] = ((float)src[i] - 128.0f) / 128.0f;
        }
    } else if (wav.sampleSize == 32) {
        float* src = (float*)wav.data;
        for (int i = 0; i < samples; i++) {
            table->data[i] = src[i];
        }
    }
    
    table->size = samples;
    table->loaded = true;
    table->name = name;
    scwCount++;
    
    UnloadWave(wav);
    return true;
}

// Speech queue for speaking text
#define SPEECH_MAX 64
typedef struct {
    char text[SPEECH_MAX];
    int index;
    int length;
    float timer;
    float speed;           // Phonemes per second
    float basePitch;       // Base frequency multiplier
    float pitchVariation;  // Random pitch variation
    bool active;
    int voiceIndex;
} SpeechQueue;
static SpeechQueue speechQueue = {0};

// Voice synthesis parameters (tweakable)
static float voiceFormantShift = 1.0f;
static float voiceBreathiness = 0.1f;
static float voiceBuzziness = 0.6f;
static float voiceSpeed = 10.0f;
static float voicePitch = 1.0f;
static int voiceVowel = VOWEL_A;

// Tweakable note parameters
static float noteAttack = 0.01f;
static float noteDecay = 0.1f;
static float noteSustain = 0.5f;
static float noteRelease = 0.3f;
static float noteVolume = 0.5f;

// Tweakable PWM parameters
static float notePulseWidth = 0.5f;
static float notePwmRate = 3.0f;
static float notePwmDepth = 0.0f;

// Tweakable vibrato parameters
static float noteVibratoRate = 5.0f;
static float noteVibratoDepth = 0.0f;

// Tweakable filter
static float noteFilterCutoff = 1.0f;

// Selected SCW for notes (0 = first loaded, -1 = none)
static int noteScwIndex = 0;

// Performance tracking
static double audioTimeUs = 0.0;
static int audioFrameCount = 0;

// Simple noise generator
static unsigned int noiseState = 12345;
static float noise(void) {
    noiseState = noiseState * 1103515245 + 12345;
    return (float)(noiseState >> 16) / 32768.0f - 1.0f;
}

// Formant filter (bandpass) processing
static float processFormantFilter(FormantFilter *f, float input, float sampleRate) {
    float fc = 2.0f * sinf(PI * f->freq / sampleRate);
    if (fc < 0.001f) fc = 0.001f;
    if (fc > 0.99f) fc = 0.99f;
    
    float q = f->freq / (f->bw + 1.0f);
    if (q < 0.5f) q = 0.5f;
    if (q > 20.0f) q = 20.0f;
    float qInv = 1.0f / q;
    
    f->low += fc * f->band;
    f->high = input - f->low - qInv * f->band;
    f->band += fc * f->high;
    
    return f->band;
}

// Voice oscillator (formant synthesis)
static float processVoiceOscillator(Voice *v, float sampleRate) {
    VoiceSettings *vs = &v->voiceSettings;
    float dt = 1.0f / sampleRate;
    
    // Decay formant filter states during release to prevent pops
    if (v->envStage == 4) {
        float decay = 0.995f;
        for (int i = 0; i < 3; i++) {
            vs->formants[i].low *= decay;
            vs->formants[i].band *= decay;
            vs->formants[i].high *= decay;
        }
    }
    
    // Apply vibrato
    float vibrato = 1.0f;
    if (vs->vibratoDepth > 0.0f) {
        vs->vibratoPhase += vs->vibratoRate * dt;
        if (vs->vibratoPhase > 1.0f) vs->vibratoPhase -= 1.0f;
        float semitones = sinf(vs->vibratoPhase * 2.0f * PI) * vs->vibratoDepth;
        vibrato = powf(2.0f, semitones / 12.0f);
    }
    
    // Advance phase
    float actualFreq = v->frequency * vibrato;
    v->phase += actualFreq / sampleRate;
    if (v->phase >= 1.0f) v->phase -= 1.0f;
    
    // Generate source signal (glottal pulse simulation)
    float smooth = 2.0f * fabsf(2.0f * v->phase - 1.0f) - 1.0f;
    float t = v->phase;
    float glottal = (t < 0.4f) ? sinf(t * PI / 0.4f) : -0.3f * sinf((t - 0.4f) * PI / 0.6f);
    float source = smooth * (1.0f - vs->buzziness) + glottal * vs->buzziness;
    
    // Mix in breathiness (noise)
    if (vs->breathiness > 0.0f) {
        source = source * (1.0f - vs->breathiness * 0.7f) + noise() * vs->breathiness * 0.5f;
    }
    
    // Interpolate formant parameters between vowels
    VowelType v1 = vs->vowel;
    VowelType v2 = vs->nextVowel;
    float blend = vs->vowelBlend;
    
    float f1_freq = formantFreq[v1][0] * (1.0f - blend) + formantFreq[v2][0] * blend;
    float f2_freq = formantFreq[v1][1] * (1.0f - blend) + formantFreq[v2][1] * blend;
    float f3_freq = formantFreq[v1][2] * (1.0f - blend) + formantFreq[v2][2] * blend;
    
    float f1_bw = formantBw[v1][0] * (1.0f - blend) + formantBw[v2][0] * blend;
    float f2_bw = formantBw[v1][1] * (1.0f - blend) + formantBw[v2][1] * blend;
    float f3_bw = formantBw[v1][2] * (1.0f - blend) + formantBw[v2][2] * blend;
    
    float f1_amp = formantAmp[v1][0] * (1.0f - blend) + formantAmp[v2][0] * blend;
    float f2_amp = formantAmp[v1][1] * (1.0f - blend) + formantAmp[v2][1] * blend;
    float f3_amp = formantAmp[v1][2] * (1.0f - blend) + formantAmp[v2][2] * blend;
    
    // Apply formant shift
    f1_freq *= vs->formantShift;
    f2_freq *= vs->formantShift;
    f3_freq *= vs->formantShift;
    
    // Update filter frequencies
    vs->formants[0].freq = f1_freq;
    vs->formants[0].bw = f1_bw;
    vs->formants[1].freq = f2_freq;
    vs->formants[1].bw = f2_bw;
    vs->formants[2].freq = f3_freq;
    vs->formants[2].bw = f3_bw;
    
    // Apply formant filters and sum
    float out = 0.0f;
    out += processFormantFilter(&vs->formants[0], source, sampleRate) * f1_amp;
    out += processFormantFilter(&vs->formants[1], source, sampleRate) * f2_amp;
    out += processFormantFilter(&vs->formants[2], source, sampleRate) * f3_amp;
    
    return out * 0.7f;
}

// SFX randomization toggle
static bool sfxRandomize = true;

// Random float in range [min, max] for SFX variation (sfxr-style)
static float rndRange(float min, float max) {
    if (!sfxRandomize) return (min + max) * 0.5f;
    noiseState = noiseState * 1103515245 + 12345;
    float t = (float)(noiseState >> 16) / 65535.0f;
    return min + t * (max - min);
}

// Mutate a value by a small percentage (sfxr-style mutation)
static float mutate(float value, float amount) {
    if (!sfxRandomize) return value;
    return value * rndRange(1.0f - amount, 1.0f + amount);
}

// Process envelope, returns amplitude 0-1
static float processEnvelope(Voice *v, float dt) {
    if (v->envStage == 0) return 0.0f;
    
    v->envPhase += dt;
    
    switch (v->envStage) {
        case 1: // Attack
            if (v->attack <= 0.0f) {
                v->envPhase = 0.0f;
                v->envStage = 2;
                v->envLevel = 1.0f;
            } else {
                v->envLevel = v->envPhase / v->attack;
                if (v->envPhase >= v->attack) {
                    v->envPhase = 0.0f;
                    v->envStage = 2;
                    v->envLevel = 1.0f;
                }
            }
            break;
        case 2: // Decay
            if (v->decay <= 0.0f) {
                v->envPhase = 0.0f;
                v->envLevel = v->sustain;
                v->envStage = (v->sustain > 0.001f) ? 3 : 4;
            } else {
                v->envLevel = 1.0f - (1.0f - v->sustain) * (v->envPhase / v->decay);
                if (v->envPhase >= v->decay) {
                    v->envPhase = 0.0f;
                    v->envLevel = v->sustain;
                    // Skip sustain if sustain level is ~0
                    v->envStage = (v->sustain > 0.001f) ? 3 : 4;
                }
            }
            break;
        case 3: // Sustain (hold until released)
            v->envLevel = v->sustain;
            break;
        case 4: // Release - fade from current level to 0
            if (v->release <= 0.0f || v->envLevel <= 0.0f) {
                v->envStage = 0;
                v->envLevel = 0.0f;
            } else {
                // Linear fade from envLevel at start of release
                float releaseProgress = v->envPhase / v->release;
                if (releaseProgress >= 1.0f) {
                    v->envStage = 0;
                    v->envLevel = 0.0f;
                } else {
                    v->envLevel *= (1.0f - dt / v->release);
                    if (v->envLevel < 0.001f) {
                        v->envStage = 0;
                        v->envLevel = 0.0f;
                    }
                }
            }
            break;
    }
    
    return v->envLevel;
}

// Generate one sample from a voice
static float processVoice(Voice *v, float sampleRate) {
    if (v->envStage == 0) return 0.0f;
    
    float dt = 1.0f / sampleRate;
    
    // Arpeggiator
    if (v->arpEnabled && v->arpCount > 0) {
        v->arpTimer += dt;
        if (v->arpTimer >= 1.0f / v->arpRate) {
            v->arpTimer = 0.0f;
            v->arpIndex = (v->arpIndex + 1) % v->arpCount;
            v->baseFrequency = v->arpNotes[v->arpIndex];
        }
    }
    
    // Start with base frequency
    float freq = v->baseFrequency;
    
    // Apply pitch slide (modifies baseFrequency for SFX)
    if (v->pitchSlide != 0.0f) {
        v->baseFrequency += v->pitchSlide;
        if (v->baseFrequency < 20.0f) v->baseFrequency = 20.0f;
        if (v->baseFrequency > 20000.0f) v->baseFrequency = 20000.0f;
        freq = v->baseFrequency;
    }
    
    // Vibrato (pitch LFO)
    if (v->vibratoDepth > 0.0f) {
        v->vibratoPhase += v->vibratoRate * dt;
        if (v->vibratoPhase >= 1.0f) v->vibratoPhase -= 1.0f;
        // Convert semitones to frequency multiplier
        float vibrato = sinf(v->vibratoPhase * 2.0f * PI) * v->vibratoDepth;
        freq *= powf(2.0f, vibrato / 12.0f);
    }
    
    v->frequency = freq;
    
    // Advance phase
    float phaseInc = v->frequency / sampleRate;
    v->phase += phaseInc;
    if (v->phase >= 1.0f) v->phase -= 1.0f;
    
    // PWM modulation (for square wave)
    float pw = v->pulseWidth;
    if (v->pwmDepth > 0.0f && v->wave == WAVE_SQUARE) {
        v->pwmPhase += v->pwmRate * dt;
        if (v->pwmPhase >= 1.0f) v->pwmPhase -= 1.0f;
        pw += sinf(v->pwmPhase * 2.0f * PI) * v->pwmDepth;
        if (pw < 0.1f) pw = 0.1f;
        if (pw > 0.9f) pw = 0.9f;
    }
    
    // Generate waveform
    float sample = 0.0f;
    switch (v->wave) {
        case WAVE_SQUARE:
            sample = v->phase < pw ? 1.0f : -1.0f;
            break;
        case WAVE_SAW:
            sample = 2.0f * v->phase - 1.0f;
            break;
        case WAVE_TRIANGLE:
            sample = 4.0f * fabsf(v->phase - 0.5f) - 1.0f;
            break;
        case WAVE_NOISE:
            sample = noise();
            break;
        case WAVE_SCW:
            // Wavetable lookup with linear interpolation
            if (v->scwIndex >= 0 && v->scwIndex < scwCount && scwTables[v->scwIndex].loaded) {
                SCWTable* table = &scwTables[v->scwIndex];
                float pos = v->phase * table->size;
                int i0 = (int)pos % table->size;
                int i1 = (i0 + 1) % table->size;
                float frac = pos - (int)pos;
                sample = table->data[i0] * (1.0f - frac) + table->data[i1] * frac;
            }
            break;
        case WAVE_VOICE:
            // Formant synthesis - uses its own phase advancement
            sample = processVoiceOscillator(v, sampleRate);
            break;
    }
    
    // Simple lowpass filter (one-pole) - always active to smooth DC offsets
    float cutoff = v->filterCutoff * v->filterCutoff; // Quadratic response feels better
    if (cutoff > 0.99f) cutoff = 0.99f;  // Always some filtering to prevent pops
    v->filterLp += cutoff * (sample - v->filterLp);
    sample = v->filterLp;
    
    // Apply envelope and volume
    float env = processEnvelope(v, dt);
    return sample * env * v->volume;
}

// Audio callback - called by raylib's audio thread
static void SynthCallback(void *buffer, unsigned int frames) {
    double startTime = GetTime();
    
    short *d = (short *)buffer;
    
    for (unsigned int i = 0; i < frames; i++) {
        float sample = 0.0f;
        
        for (int v = 0; v < NUM_VOICES; v++) {
            sample += processVoice(&voices[v], SAMPLE_RATE);
        }
        
        sample *= masterVolume;
        
        // Clamp
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;
        
        d[i] = (short)(sample * 32000.0f);
    }
    
    double elapsed = (GetTime() - startTime) * 1000000.0;  // microseconds
    audioTimeUs = audioTimeUs * 0.95 + elapsed * 0.05;     // smoothed
    audioFrameCount = frames;
}

// Find a free voice or steal one in release
static int findVoice(void) {
    // First look for completely free voice
    for (int i = 0; i < NUM_VOICES; i++) {
        if (voices[i].envStage == 0) return i;
    }
    // Then look for one in release stage
    for (int i = 0; i < NUM_VOICES; i++) {
        if (voices[i].envStage == 4) return i;
    }
    return NUM_VOICES - 1;  // Steal last voice
}

// Play a note with polyphony - finds free voice automatically
// Uses global tweakable parameters
static int playNote(float freq, WaveType wave) {
    int voiceIdx = findVoice();
    Voice *v = &voices[voiceIdx];
    
    // Preserve filter state for smooth transition (anti-pop)
    float oldFilterLp = v->filterLp;
    
    v->frequency = freq;
    v->baseFrequency = freq;
    v->phase = 0.0f;
    v->volume = noteVolume;
    v->wave = wave;
    v->pitchSlide = 0.0f;
    
    // PWM
    v->pulseWidth = notePulseWidth;
    v->pwmRate = notePwmRate;
    v->pwmDepth = notePwmDepth;
    v->pwmPhase = 0.0f;
    
    // Vibrato
    v->vibratoRate = noteVibratoRate;
    v->vibratoDepth = noteVibratoDepth;
    v->vibratoPhase = 0.0f;
    
    // Envelope
    v->attack = noteAttack;
    v->decay = noteDecay;
    v->sustain = noteSustain;
    v->release = noteRelease;
    v->envPhase = 0.0f;
    v->envLevel = 0.0f;
    v->envStage = 1;
    
    // Filter - preserve some state to avoid pops
    v->filterCutoff = noteFilterCutoff;
    v->filterLp = oldFilterLp * 0.3f;
    
    // Arp disabled for regular notes
    v->arpEnabled = false;
    
    // SCW index
    v->scwIndex = noteScwIndex;
    
    return voiceIdx;
}

// Release a note (trigger release stage)
static void releaseNote(int voiceIdx) {
    if (voiceIdx < 0 || voiceIdx >= NUM_VOICES) return;
    if (voices[voiceIdx].envStage > 0 && voices[voiceIdx].envStage < 4) {
        voices[voiceIdx].envStage = 4;
        voices[voiceIdx].envPhase = 0.0f;
    }
}

// Helper to init a voice for SFX (sets all fields to safe defaults)
static void initSfxVoice(Voice *v, float freq, WaveType wave, float vol,
                         float attack, float decay, float release, float pitchSlide) {
    // Preserve filter state for smooth transition (anti-pop)
    float oldFilterLp = v->filterLp;
    
    memset(v, 0, sizeof(Voice));
    v->frequency = freq;
    v->baseFrequency = freq;
    v->volume = vol;
    v->wave = wave;
    v->pulseWidth = 0.5f;
    v->attack = attack;
    v->decay = decay;
    v->sustain = 0.0f;  // SFX always auto-release
    v->release = release;
    v->envStage = 1;
    v->pitchSlide = pitchSlide;
    v->filterCutoff = 1.0f;
    v->filterLp = oldFilterLp * 0.5f;  // Fade old filter state
}

// Sound effects - all use sustain=0 so they auto-release after decay
// Each has sfxr-style randomization for subtle variation

static void sfxJump(void) {
    int v = findVoice();
    initSfxVoice(&voices[v], mutate(150.0f, 0.15f), WAVE_SQUARE, mutate(0.5f, 0.1f),
                 0.01f, mutate(0.15f, 0.1f), 0.05f, mutate(10.0f, 0.2f));
}

static void sfxCoin(void) {
    int v = findVoice();
    initSfxVoice(&voices[v], mutate(1200.0f, 0.08f), WAVE_SQUARE, mutate(0.4f, 0.1f),
                 0.005f, mutate(0.1f, 0.15f), 0.05f, mutate(20.0f, 0.15f));
}

static void sfxHurt(void) {
    int v = findVoice();
    initSfxVoice(&voices[v], mutate(200.0f, 0.25f), WAVE_NOISE, mutate(0.5f, 0.1f),
                 0.01f, mutate(0.2f, 0.2f), mutate(0.1f, 0.2f), mutate(-3.0f, 0.3f));
}

static void sfxExplosion(void) {
    int v = findVoice();
    initSfxVoice(&voices[v], mutate(80.0f, 0.3f), WAVE_NOISE, mutate(0.6f, 0.1f),
                 0.01f, mutate(0.5f, 0.25f), mutate(0.3f, 0.2f), mutate(-1.0f, 0.4f));
}

static void sfxPowerup(void) {
    int v = findVoice();
    initSfxVoice(&voices[v], mutate(300.0f, 0.12f), WAVE_TRIANGLE, mutate(0.4f, 0.1f),
                 0.01f, mutate(0.3f, 0.15f), mutate(0.2f, 0.1f), mutate(8.0f, 0.2f));
}

static void sfxBlip(void) {
    int v = findVoice();
    initSfxVoice(&voices[v], mutate(800.0f, 0.1f), WAVE_SQUARE, mutate(0.3f, 0.1f),
                 0.005f, mutate(0.05f, 0.15f), 0.02f, rndRange(-2.0f, 2.0f));
}

// === DRUM SOUNDS ===

// Kick drum: sine wave with fast pitch drop + saturation
static void drumKick(void) {
    int v = findVoice();
    initSfxVoice(&voices[v], mutate(150.0f, 0.1f), WAVE_TRIANGLE, mutate(0.7f, 0.1f),
                 0.005f, mutate(0.25f, 0.15f), 0.1f, mutate(-8.0f, 0.2f));
    voices[v].filterCutoff = 0.6f;  // Warmer sound
}

// Snare drum: noise + triangle with filter
static void drumSnare(void) {
    int v = findVoice();
    initSfxVoice(&voices[v], mutate(200.0f, 0.15f), WAVE_NOISE, mutate(0.5f, 0.1f),
                 0.005f, mutate(0.15f, 0.2f), 0.08f, mutate(-2.0f, 0.3f));
    voices[v].filterCutoff = mutate(0.7f, 0.1f);
}

// Hi-hat: high-passed noise, very short
static void drumHihat(void) {
    int v = findVoice();
    initSfxVoice(&voices[v], mutate(8000.0f, 0.15f), WAVE_NOISE, mutate(0.3f, 0.1f),
                 0.001f, mutate(0.05f, 0.2f), 0.02f, 0.0f);
    voices[v].filterCutoff = 0.9f;  // Keep it bright
}

// === VOICE/SPEECH FUNCTIONS ===

// Map character to vowel sound
static VowelType charToVowel(char c) {
    if (c >= 'A' && c <= 'Z') c += 32; // tolower
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

// Get pitch variation for melodic speech
static float charToPitch(char c) {
    if (c >= 'A' && c <= 'Z') c += 32;
    int val = (c * 7) % 12;
    return 1.0f + (val - 6) * 0.05f;
}

// Play a single vowel sound on a specific voice
static void playVowelOnVoice(int voiceIdx, float freq, VowelType vowel) {
    Voice *v = &voices[voiceIdx];
    
    float oldFilterLp = v->filterLp;
    
    v->frequency = freq;
    v->baseFrequency = freq;
    v->phase = 0.0f;
    v->volume = noteVolume;
    v->wave = WAVE_VOICE;
    v->pitchSlide = 0.0f;
    
    v->pulseWidth = 0.5f;
    v->pwmRate = 0.0f;
    v->pwmDepth = 0.0f;
    v->pwmPhase = 0.0f;
    
    v->vibratoRate = 5.0f;
    v->vibratoDepth = 0.1f;
    v->vibratoPhase = 0.0f;
    
    v->attack = 0.02f;
    v->decay = 0.05f;
    v->sustain = 0.7f;
    v->release = 0.25f;
    v->envPhase = 0.0f;
    v->envLevel = 0.0f;
    v->envStage = 1;
    
    v->filterCutoff = 0.7f;  // Lower cutoff to smooth formant filter artifacts
    v->filterLp = oldFilterLp * 0.3f;
    v->arpEnabled = false;
    v->scwIndex = -1;
    
    // Setup voice settings
    VoiceSettings *vs = &v->voiceSettings;
    vs->vowel = vowel;
    vs->nextVowel = vowel;
    vs->vowelBlend = 0.0f;
    vs->formantShift = voiceFormantShift;
    vs->breathiness = voiceBreathiness;
    vs->buzziness = voiceBuzziness;
    vs->vibratoRate = 5.0f;
    vs->vibratoDepth = 0.15f;
    vs->vibratoPhase = 0.0f;
    
    // Clear formant filter states
    for (int i = 0; i < 3; i++) {
        vs->formants[i].low = 0;
        vs->formants[i].band = 0;
        vs->formants[i].high = 0;
    }
}

// Play a single vowel sound (finds a free voice)
static int playVowel(float freq, VowelType vowel) {
    int voiceIdx = findVoice();
    playVowelOnVoice(voiceIdx, freq, vowel);
    return voiceIdx;
}

// Start speaking text
static void speak(const char *text, float speed, float pitch, float variation) {
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
    sq->speed = speed < 1.0f ? 1.0f : (speed > 30.0f ? 30.0f : speed);
    sq->basePitch = pitch < 0.3f ? 0.3f : (pitch > 3.0f ? 3.0f : pitch);
    sq->pitchVariation = variation < 0.0f ? 0.0f : (variation > 1.0f ? 1.0f : variation);
    sq->active = true;
    sq->voiceIndex = NUM_VOICES - 1;  // Use last voice for speech
}

// Generate random babble
static void babble(float duration, float pitch, float mood) {
    static const char* syllables[] = {
        "ba", "da", "ga", "ma", "na", "pa", "ta", "ka", "wa", "ya",
        "be", "de", "ge", "me", "ne", "pe", "te", "ke", "we", "ye",
        "bi", "di", "gi", "mi", "ni", "pi", "ti", "ki", "wi", "yi",
        "bo", "do", "go", "mo", "no", "po", "to", "ko", "wo", "yo",
        "bu", "du", "gu", "mu", "nu", "pu", "tu", "ku", "wu", "yu",
        "la", "ra", "sa", "za", "ha", "ja", "fa", "va",
    };
    int numSyllables = sizeof(syllables) / sizeof(syllables[0]);
    
    char text[SPEECH_MAX];
    int pos = 0;
    float speed = 8.0f + mood * 8.0f;
    int targetSyllables = (int)(duration * speed / 2.0f);
    
    for (int i = 0; i < targetSyllables && pos < SPEECH_MAX - 4; i++) {
        noiseState = noiseState * 1103515245 + 12345;
        const char* syl = syllables[(noiseState >> 16) % numSyllables];
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
    speak(text, speed, pitch, variation);
}

// Process speech queue (call every frame)
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
        
        float baseFreq = 200.0f * sq->basePitch * pitchMod * randVar;
        
        Voice *v = &voices[sq->voiceIndex];
        
        if (v->envStage > 0 && v->wave == WAVE_VOICE) {
            v->voiceSettings.nextVowel = vowel;
            v->voiceSettings.vowelBlend = 0.0f;
            v->frequency = baseFreq;
            v->baseFrequency = baseFreq;
        } else {
            // Start a new vowel on the dedicated speech voice
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

// Note frequencies (A4 = 440Hz)
static const float NOTE_C4 = 261.63f;
static const float NOTE_D4 = 293.66f;
static const float NOTE_E4 = 329.63f;
static const float NOTE_F4 = 349.23f;
static const float NOTE_G4 = 392.00f;
static const float NOTE_A4 = 440.00f;
static const float NOTE_B4 = 493.88f;

// Track which voice is playing each key (-1 = not playing)
static int keyVoices[14] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};

// Waveform names for UI
static const char* waveNames[] = {"Square", "Saw", "Triangle", "Noise", "SCW", "Voice"};
static int selectedWave = 0;

// Vowel names for UI
static const char* vowelNames[] = {"A (ah)", "E (eh)", "I (ee)", "O (oh)", "U (oo)"};

// Voice key tracking for vowel hold
static int vowelKeyVoice = -1;

int main(void) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "PixelSynth Demo");
    
    Font font = LoadEmbeddedFont();
    ui_init(&font);
    
    SetAudioStreamBufferSizeDefault(MAX_SAMPLES_PER_UPDATE);
    InitAudioDevice();
    
    // Load SCW wavetables (curated selection)
    loadSCW("experiments/pixelsynth/cycles/Analog Waveforms in C/001-Analog Pulse50 1.wav", "Pulse");
    loadSCW("experiments/pixelsynth/cycles/Analog Waveforms in C/003-Analog Saw 1.wav", "Saw");
    loadSCW("experiments/pixelsynth/cycles/Analog Waveforms in C/006-Analog Sine 1.wav", "Sine");
    loadSCW("experiments/pixelsynth/cycles/Analog Waveforms in C/010-Analog Square 1.wav", "Square");
    loadSCW("experiments/pixelsynth/cycles/Analog Waveforms in C/014-Analog Triangle 1.wav", "Triangle");
    loadSCW("experiments/pixelsynth/cycles/Analog Waveforms in C/Fr4 - Aelita.wav", "Aelita");
    loadSCW("experiments/pixelsynth/cycles/Analog Waveforms in C/Fr4 - Buchla Modular 1.wav", "Buchla");
    loadSCW("experiments/pixelsynth/cycles/Analog Waveforms in C/Fr4 - Moog Modular 1.wav", "Moog");
    loadSCW("experiments/pixelsynth/cycles/Analog Waveforms in C/Fr4 - Polivoks 1.wav", "Polivoks");
    loadSCW("experiments/pixelsynth/cycles/Analog Waveforms in C/Fr4 - SH101 1.wav", "SH101");
    
    // Create audio stream: 44100 Hz, 16-bit, mono
    AudioStream stream = LoadAudioStream(SAMPLE_RATE, 16, 1);
    SetAudioStreamCallback(stream, SynthCallback);
    PlayAudioStream(stream);
    
    memset(voices, 0, sizeof(voices));
    
    SetTargetFPS(60);
    
    // Key mappings for polyphonic play
    const int keys[] = {KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y, KEY_U,
                        KEY_A, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H, KEY_J};
    const float freqs[] = {NOTE_C4, NOTE_D4, NOTE_E4, NOTE_F4, NOTE_G4, NOTE_A4, NOTE_B4,
                           NOTE_C4*0.5f, NOTE_D4*0.5f, NOTE_E4*0.5f, NOTE_F4*0.5f, NOTE_G4*0.5f, NOTE_A4*0.5f, NOTE_B4*0.5f};
    
    while (!WindowShouldClose()) {
        // Sound effects (1-6)
        if (IsKeyPressed(KEY_ONE))   sfxJump();
        if (IsKeyPressed(KEY_TWO))   sfxCoin();
        if (IsKeyPressed(KEY_THREE)) sfxHurt();
        if (IsKeyPressed(KEY_FOUR))  sfxExplosion();
        if (IsKeyPressed(KEY_FIVE))  sfxPowerup();
        if (IsKeyPressed(KEY_SIX))   sfxBlip();
        
        // Drums (7-9)
        if (IsKeyPressed(KEY_SEVEN)) drumKick();
        if (IsKeyPressed(KEY_EIGHT)) drumSnare();
        if (IsKeyPressed(KEY_NINE))  drumHihat();
        
        // Voice/Speech (0, SPACE, ENTER, hold V for vowel)
        if (IsKeyPressed(KEY_ZERO)) babble(2.0f, voicePitch, 0.5f);
        if (IsKeyPressed(KEY_SPACE)) speak("hello world", voiceSpeed, voicePitch, 0.3f);
        if (IsKeyPressed(KEY_V)) {
            vowelKeyVoice = playVowel(200.0f * voicePitch, (VowelType)voiceVowel);
        }
        if (IsKeyReleased(KEY_V) && vowelKeyVoice >= 0) {
            releaseNote(vowelKeyVoice);
            vowelKeyVoice = -1;
        }
        
        // Update speech system
        updateSpeech(GetFrameTime());
        
        // Polyphonic notes - press to play, release to stop
        for (int i = 0; i < 14; i++) {
            if (IsKeyPressed(keys[i])) {
                keyVoices[i] = playNote(freqs[i], (WaveType)selectedWave);
            }
            if (IsKeyReleased(keys[i]) && keyVoices[i] >= 0) {
                releaseNote(keyVoices[i]);
                keyVoices[i] = -1;
            }
        }
        
        BeginDrawing();
        ClearBackground(DARKGRAY);
        ui_begin_frame();
        
        DrawTextEx(font, "PixelSynth Demo", (Vector2){20, 20}, 30, 1, WHITE);
        
        // Left column - controls info (compact)
        DrawTextEx(font, "SFX: 1-6  Drums: 7-9", (Vector2){20, 55}, 12, 1, LIGHTGRAY);
        DrawTextEx(font, "Notes: QWERTYU / ASDFGHJ", (Vector2){20, 70}, 12, 1, LIGHTGRAY);
        DrawTextEx(font, "Voice: V=vowel SPACE=speak 0=babble", (Vector2){20, 85}, 12, 1, LIGHTGRAY);
        
        // Show active voices
        DrawTextEx(font, "Voices:", (Vector2){20, 105}, 12, 1, GRAY);
        for (int i = 0; i < NUM_VOICES; i++) {
            Color c = DARKGRAY;
            if (voices[i].envStage == 4) c = ORANGE;
            else if (voices[i].envStage > 0) c = GREEN;
            DrawRectangle(75 + i * 18, 105, 14, 12, c);
        }
        
        // Performance stats
        double bufferTimeMs = (double)audioFrameCount / SAMPLE_RATE * 1000.0;
        double cpuPercent = (audioTimeUs / 1000.0) / bufferTimeMs * 100.0;
        DrawTextEx(font, TextFormat("Audio: %.0fus (%.1f%%)  FPS: %d", 
                 audioTimeUs, cpuPercent, GetFPS()), (Vector2){20, 125}, 12, 1, GRAY);
        
        ToggleBool(20, 145, "SFX Randomize", &sfxRandomize);
        
        // Show speaking indicator
        if (speechQueue.active) {
            DrawTextEx(font, "Speaking...", (Vector2){20, 170}, 14, 1, GREEN);
        }
        
        // === COLUMN 1: Wave & Envelope ===
        float uiX = 250;
        float uiY = 20;
        
        // Waveform selector
        CycleOption(uiX, uiY, "Wave", waveNames, 6, &selectedWave);
        uiY += 22;
        
        // SCW selector (only if SCW wave selected)
        if (selectedWave == WAVE_SCW && scwCount > 0) {
            const char* scwNames[SCW_MAX_SLOTS];
            for (int i = 0; i < scwCount; i++) scwNames[i] = scwTables[i].name;
            CycleOption(uiX, uiY, "SCW", scwNames, scwCount, &noteScwIndex);
            uiY += 22;
        }
        uiY += 4;
        
        // Envelope
        DrawTextShadow("Envelope:", (int)uiX, (int)uiY, 14, YELLOW); uiY += 18;
        DraggableFloat(uiX, uiY, "Attack", &noteAttack, 0.5f, 0.001f, 2.0f); uiY += 18;
        DraggableFloat(uiX, uiY, "Decay", &noteDecay, 0.5f, 0.0f, 2.0f); uiY += 18;
        DraggableFloat(uiX, uiY, "Sustain", &noteSustain, 0.5f, 0.0f, 1.0f); uiY += 18;
        DraggableFloat(uiX, uiY, "Release", &noteRelease, 0.5f, 0.01f, 3.0f); uiY += 22;
        
        // PWM (only for square)
        if (selectedWave == WAVE_SQUARE) {
            DrawTextShadow("PWM:", (int)uiX, (int)uiY, 14, YELLOW); uiY += 18;
            DraggableFloat(uiX, uiY, "Width", &notePulseWidth, 0.5f, 0.1f, 0.9f); uiY += 18;
            DraggableFloat(uiX, uiY, "Rate", &notePwmRate, 0.5f, 0.1f, 20.0f); uiY += 18;
            DraggableFloat(uiX, uiY, "Depth", &notePwmDepth, 0.5f, 0.0f, 0.4f); uiY += 22;
        }
        
        // Vibrato
        DrawTextShadow("Vibrato:", (int)uiX, (int)uiY, 14, YELLOW); uiY += 18;
        DraggableFloat(uiX, uiY, "Rate", &noteVibratoRate, 0.5f, 0.5f, 15.0f); uiY += 18;
        DraggableFloat(uiX, uiY, "Depth", &noteVibratoDepth, 0.2f, 0.0f, 2.0f); uiY += 22;
        
        // Filter
        DrawTextShadow("Filter:", (int)uiX, (int)uiY, 14, YELLOW); uiY += 18;
        DraggableFloat(uiX, uiY, "Cutoff", &noteFilterCutoff, 0.5f, 0.05f, 1.0f);
        
        // === COLUMN 2: Volume & Voice ===
        uiX = 430;
        uiY = 20;
        
        // Volume
        DrawTextShadow("Volume:", (int)uiX, (int)uiY, 14, YELLOW); uiY += 18;
        DraggableFloat(uiX, uiY, "Note", &noteVolume, 0.5f, 0.0f, 1.0f); uiY += 18;
        DraggableFloat(uiX, uiY, "Master", &masterVolume, 0.5f, 0.0f, 1.0f); uiY += 26;
        
        // Voice/Speech controls
        DrawTextShadow("Voice:", (int)uiX, (int)uiY, 14, YELLOW); uiY += 18;
        CycleOption(uiX, uiY, "Vowel", vowelNames, 5, &voiceVowel); uiY += 22;
        DraggableFloat(uiX, uiY, "Pitch", &voicePitch, 0.5f, 0.3f, 2.0f); uiY += 18;
        DraggableFloat(uiX, uiY, "Speed", &voiceSpeed, 1.0f, 4.0f, 20.0f); uiY += 18;
        DraggableFloat(uiX, uiY, "Formant", &voiceFormantShift, 0.5f, 0.5f, 1.5f); uiY += 18;
        DraggableFloat(uiX, uiY, "Breath", &voiceBreathiness, 0.5f, 0.0f, 1.0f); uiY += 18;
        DraggableFloat(uiX, uiY, "Buzz", &voiceBuzziness, 0.5f, 0.0f, 1.0f);
        
        ui_update();
        EndDrawing();
    }
    
    UnloadAudioStream(stream);
    CloseAudioDevice();
    UnloadFont(font);
    CloseWindow();
    
    return 0;
}
