// PixelSynth Demo - Simple chiptune synth using raylib audio callbacks
// Press 1-6 for sound effects, Q-U for notes

#include "../../vendor/raylib.h"
#include "../../assets/fonts/comic_embedded.h"
#define UI_IMPLEMENTATION
#include "../../shared/ui.h"
#include <math.h>
#include <string.h>

#define SCREEN_WIDTH 960
#define SCREEN_HEIGHT 700
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

// ============================================================================
// 808-STYLE DRUM MACHINE
// ============================================================================

// Drum types
typedef enum {
    DRUM_KICK,
    DRUM_SNARE,
    DRUM_CLAP,
    DRUM_CLOSED_HH,
    DRUM_OPEN_HH,
    DRUM_LOW_TOM,
    DRUM_MID_TOM,
    DRUM_HI_TOM,
    DRUM_RIMSHOT,
    DRUM_COWBELL,
    DRUM_CLAVE,
    DRUM_MARACAS,
    DRUM_COUNT
} DrumType;

// Drum voice state (separate from synth voices for dedicated processing)
typedef struct {
    bool active;
    float time;           // Time since trigger
    float phase;          // Oscillator phase
    float phase2;         // Second oscillator (for cowbell, etc.)
    float pitchEnv;       // Pitch envelope value
    float ampEnv;         // Amplitude envelope value
    float noiseState;     // Noise filter state
    float filterLp;       // Lowpass filter state
    float filterHp;       // Highpass filter state
    // Hihat specific - 6 oscillator phases
    float hhPhases[6];
    // Per-hit parameters (set at trigger time)
    float velocity;       // Volume multiplier (0.0-1.0)
    float pitchMod;       // Pitch multiplier (0.5-2.0, 1.0 = normal)
} DrumVoice;

// Parameters for each drum sound (user-tweakable)
typedef struct {
    // Kick parameters
    float kickPitch;      // Base pitch (30-80 Hz)
    float kickDecay;      // Decay time (0.1-1.0s)
    float kickPunchPitch; // Starting pitch for pitch envelope (80-200 Hz)
    float kickPunchDecay; // How fast pitch drops (0.01-0.1s)
    float kickClick;      // Initial click amount (0-1)
    float kickTone;       // Tone/distortion (0-1)
    
    // Snare parameters
    float snarePitch;     // Tone pitch (100-300 Hz)
    float snareDecay;     // Overall decay (0.1-0.5s)
    float snareSnappy;    // Noise amount (0-1)
    float snareTone;      // Tone vs noise balance (0-1)
    
    // Clap parameters
    float clapDecay;      // Decay time
    float clapTone;       // Filter brightness
    float clapSpread;     // Timing spread of "hands"
    
    // Hihat parameters
    float hhDecayClosed;  // Closed hihat decay (0.02-0.15s)
    float hhDecayOpen;    // Open hihat decay (0.2-1.0s)
    float hhTone;         // Brightness/filter (0-1)
    
    // Tom parameters
    float tomPitch;       // Base pitch multiplier
    float tomDecay;       // Decay time
    float tomPunchDecay;  // Pitch envelope time
    
    // Rimshot parameters
    float rimPitch;       // Click pitch
    float rimDecay;       // Decay time
    
    // Cowbell parameters
    float cowbellPitch;   // Base pitch
    float cowbellDecay;   // Decay time
    
    // Clave parameters
    float clavePitch;     // Pitch
    float claveDecay;     // Decay (very short)
    
    // Maracas parameters
    float maracasDecay;   // Decay time
    float maracasTone;    // Brightness
} DrumParams;

// Drum machine state
#define NUM_DRUM_VOICES 12  // One per drum type, allows overlapping
static DrumVoice drumVoices[NUM_DRUM_VOICES];
static DrumParams drumParams;
static float drumVolume = 0.6f;

// Forward declarations
static float processDrums(float dt);
static int playVowel(float freq, VowelType vowel);
static float noise(void);
static void drumKick(void);
static void drumSnare(void);
static void drumClosedHH(void);
static void drumClap(void);
// Full versions with velocity and pitch for sequencer
static void drumKickFull(float vel, float pitch);
static void drumSnareFull(float vel, float pitch);
static void drumClosedHHFull(float vel, float pitch);
static void drumClapFull(float vel, float pitch);

// Noise state needs to be declared early for sequencer random
static unsigned int noiseState = 12345;

// Initialize drum parameters to 808-like defaults
static void initDrumParams(void) {
    // Kick - punchy 808 style
    drumParams.kickPitch = 50.0f;
    drumParams.kickDecay = 0.5f;
    drumParams.kickPunchPitch = 150.0f;
    drumParams.kickPunchDecay = 0.04f;
    drumParams.kickClick = 0.3f;
    drumParams.kickTone = 0.5f;
    
    // Snare
    drumParams.snarePitch = 180.0f;
    drumParams.snareDecay = 0.2f;
    drumParams.snareSnappy = 0.6f;
    drumParams.snareTone = 0.5f;
    
    // Clap
    drumParams.clapDecay = 0.3f;
    drumParams.clapTone = 0.6f;
    drumParams.clapSpread = 0.012f;
    
    // Hihats
    drumParams.hhDecayClosed = 0.05f;
    drumParams.hhDecayOpen = 0.4f;
    drumParams.hhTone = 0.7f;
    
    // Toms
    drumParams.tomPitch = 1.0f;
    drumParams.tomDecay = 0.3f;
    drumParams.tomPunchDecay = 0.05f;
    
    // Rimshot
    drumParams.rimPitch = 1700.0f;
    drumParams.rimDecay = 0.03f;
    
    // Cowbell
    drumParams.cowbellPitch = 560.0f;
    drumParams.cowbellDecay = 0.3f;
    
    // Clave
    drumParams.clavePitch = 2500.0f;
    drumParams.claveDecay = 0.02f;
    
    // Maracas
    drumParams.maracasDecay = 0.07f;
    drumParams.maracasTone = 0.8f;
}

// ============================================================================
// EFFECTS PEDALS
// ============================================================================

// Delay buffer (max 2 seconds at 44100Hz)
#define DELAY_BUFFER_SIZE (SAMPLE_RATE * 2)
static float delayBuffer[DELAY_BUFFER_SIZE];
static int delayWritePos = 0;

// Effects parameters
typedef struct {
    // Distortion
    bool distEnabled;
    float distDrive;      // 1.0 = clean, 10.0 = heavy distortion
    float distTone;       // Lowpass after distortion (0-1)
    float distMix;        // Dry/wet (0-1)
    float distFilterLp;   // Filter state
    
    // Delay
    bool delayEnabled;
    float delayTime;      // Delay time in seconds (0.05 - 1.0)
    float delayFeedback;  // Feedback amount (0 - 0.9)
    float delayMix;       // Dry/wet (0-1)
    float delayTone;      // Lowpass on delay (0-1, darker repeats)
    float delayFilterLp;  // Filter state for feedback
    
    // Tape effects
    bool tapeEnabled;
    float tapeWow;        // Slow pitch wobble (0-1)
    float tapeFlutter;    // Fast pitch wobble (0-1)  
    float tapeSaturation; // Tape saturation/warmth (0-1)
    float tapeHiss;       // Tape hiss amount (0-1)
    float tapeWowPhase;   // LFO phase for wow
    float tapeFlutterPhase; // LFO phase for flutter
    float tapeFilterLp;   // Highpass state for hiss
    
    // Bitcrusher
    bool crushEnabled;
    float crushBits;      // Bit depth (2-16)
    float crushRate;      // Sample rate reduction factor (1-32)
    float crushMix;       // Dry/wet (0-1)
    float crushHold;      // Sample hold value
    int crushCounter;     // Sample counter for rate reduction
} Effects;

static Effects fx = {0};

// Initialize effects to defaults
static void initEffects(void) {
    // Distortion - off by default
    fx.distEnabled = false;
    fx.distDrive = 2.0f;
    fx.distTone = 0.7f;
    fx.distMix = 0.5f;
    fx.distFilterLp = 0.0f;
    
    // Delay - off by default  
    fx.delayEnabled = false;
    fx.delayTime = 0.3f;
    fx.delayFeedback = 0.4f;
    fx.delayMix = 0.3f;
    fx.delayTone = 0.6f;
    fx.delayFilterLp = 0.0f;
    
    // Tape - off by default
    fx.tapeEnabled = false;
    fx.tapeWow = 0.3f;
    fx.tapeFlutter = 0.2f;
    fx.tapeSaturation = 0.5f;
    fx.tapeHiss = 0.1f;
    fx.tapeWowPhase = 0.0f;
    fx.tapeFlutterPhase = 0.0f;
    fx.tapeFilterLp = 0.0f;
    
    // Bitcrusher - off by default
    fx.crushEnabled = false;
    fx.crushBits = 8.0f;
    fx.crushRate = 4.0f;
    fx.crushMix = 0.5f;
    fx.crushHold = 0.0f;
    fx.crushCounter = 0;
    
    // Clear delay buffer
    memset(delayBuffer, 0, sizeof(delayBuffer));
    delayWritePos = 0;
}

// Process distortion
static float processDistortion(float sample) {
    if (!fx.distEnabled) return sample;
    
    float dry = sample;
    
    // Drive into soft clipping (tanh saturation)
    float driven = tanhf(sample * fx.distDrive);
    
    // Tone control (lowpass to tame harshness)
    float cutoff = fx.distTone * fx.distTone * 0.5f + 0.1f;
    fx.distFilterLp += cutoff * (driven - fx.distFilterLp);
    float wet = fx.distFilterLp;
    
    return dry * (1.0f - fx.distMix) + wet * fx.distMix;
}

// Process delay
static float processDelay(float sample, float dt) {
    if (!fx.delayEnabled) return sample;
    (void)dt;
    
    // Calculate delay in samples
    int delaySamples = (int)(fx.delayTime * SAMPLE_RATE);
    if (delaySamples >= DELAY_BUFFER_SIZE) delaySamples = DELAY_BUFFER_SIZE - 1;
    if (delaySamples < 1) delaySamples = 1;
    
    // Read from delay buffer
    int readPos = delayWritePos - delaySamples;
    if (readPos < 0) readPos += DELAY_BUFFER_SIZE;
    float delayed = delayBuffer[readPos];
    
    // Filter the delayed signal (darker repeats)
    float cutoff = fx.delayTone * fx.delayTone * 0.4f + 0.1f;
    fx.delayFilterLp += cutoff * (delayed - fx.delayFilterLp);
    delayed = fx.delayFilterLp;
    
    // Write to delay buffer (input + filtered feedback)
    delayBuffer[delayWritePos] = sample + delayed * fx.delayFeedback;
    delayWritePos = (delayWritePos + 1) % DELAY_BUFFER_SIZE;
    
    return sample * (1.0f - fx.delayMix) + delayed * fx.delayMix;
}

// Process tape effects
static float processTape(float sample, float dt) {
    if (!fx.tapeEnabled) return sample;
    
    // Tape saturation (soft, warm clipping)
    if (fx.tapeSaturation > 0.0f) {
        float sat = fx.tapeSaturation * 2.0f;
        sample = tanhf(sample * (1.0f + sat)) / (1.0f + sat * 0.5f);
    }
    
    // Wow (slow pitch wobble ~0.5Hz) - simulated as volume modulation
    // Real wow would need pitch shifting, but volume wobble gives similar vibe
    if (fx.tapeWow > 0.0f) {
        fx.tapeWowPhase += 0.5f * dt;
        if (fx.tapeWowPhase > 1.0f) fx.tapeWowPhase -= 1.0f;
        float wow = sinf(fx.tapeWowPhase * 2.0f * PI) * fx.tapeWow * 0.1f;
        sample *= (1.0f + wow);
    }
    
    // Flutter (fast wobble ~6Hz)
    if (fx.tapeFlutter > 0.0f) {
        fx.tapeFlutterPhase += 6.0f * dt;
        if (fx.tapeFlutterPhase > 1.0f) fx.tapeFlutterPhase -= 1.0f;
        float flutter = sinf(fx.tapeFlutterPhase * 2.0f * PI) * fx.tapeFlutter * 0.05f;
        sample *= (1.0f + flutter);
    }
    
    // Tape hiss (filtered noise)
    if (fx.tapeHiss > 0.0f) {
        float hiss = noise() * fx.tapeHiss * 0.05f;
        // Highpass the hiss to make it more realistic
        fx.tapeFilterLp += 0.1f * (hiss - fx.tapeFilterLp);
        hiss = hiss - fx.tapeFilterLp;
        sample += hiss;
    }
    
    return sample;
}

// Process bitcrusher
static float processBitcrusher(float sample) {
    if (!fx.crushEnabled) return sample;
    
    float dry = sample;
    
    // Sample rate reduction
    fx.crushCounter++;
    if (fx.crushCounter >= (int)fx.crushRate) {
        fx.crushCounter = 0;
        
        // Bit depth reduction
        float levels = powf(2.0f, fx.crushBits);
        fx.crushHold = floorf(sample * levels) / levels;
    }
    
    return dry * (1.0f - fx.crushMix) + fx.crushHold * fx.crushMix;
}

// Process all effects (call on master output)
static float processEffects(float sample, float dt) {
    sample = processDistortion(sample);
    sample = processBitcrusher(sample);
    sample = processTape(sample, dt);
    sample = processDelay(sample, dt);
    return sample;
}

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

// UI toggles
static bool showGenMusic = false;  // Hide generative music UI by default

// ============================================================================
// DRUM STEP SEQUENCER (with tick-based timing like MPC/808)
// ============================================================================

// Timing resolution - 96 PPQ (pulses per quarter note) like MPC60/3000
// 96 PPQ = 24 ticks per 16th note step
#define SEQ_PPQ 96
#define SEQ_TICKS_PER_STEP 24  // 96 / 4 steps per beat
#define SEQ_MAX_STEPS 16
#define SEQ_TRACKS 4  // Kick, Snare, HiHat, Clap

// Dilla-style timing offsets (in ticks, 24 ticks = 1 step)
// Inspired by J Dilla's MPC timing - kicks early, snares late, humanized feel
typedef struct {
    int kickNudge;      // Kick timing offset in ticks (negative = early, e.g., -2)
    int snareDelay;     // Snare timing offset in ticks (positive = late/lazy, e.g., +4)
    int hatNudge;       // HiHat timing offset in ticks
    int clapDelay;      // Clap timing offset in ticks
    int swing;          // Off-beat swing in ticks (e.g., 6 = pushed late)
    int jitter;         // Random humanization range in ticks (e.g., 2 = ±2 ticks)
} DillaTiming;

typedef struct {
    bool steps[SEQ_TRACKS][SEQ_MAX_STEPS];  // Which steps are active
    float velocity[SEQ_TRACKS][SEQ_MAX_STEPS]; // Velocity per step (0.0-1.0)
    float pitch[SEQ_TRACKS][SEQ_MAX_STEPS];    // Pitch offset per step (-1.0 to +1.0, 0 = normal)
    int trackLength[SEQ_TRACKS];            // Length per track (for polyrhythm)
    int trackStep[SEQ_TRACKS];              // Current step per track (0 to length-1)
    int trackTick[SEQ_TRACKS];              // Current tick within step per track (for polyrhythm)
    int trackTriggerTick[SEQ_TRACKS];       // Tick within step when this track should trigger
    bool trackTriggered[SEQ_TRACKS];        // Has this step been triggered yet?
    
    bool playing;
    float bpm;
    
    // Global tick accumulator
    float tickTimer;
    
    // Dilla timing
    DillaTiming dilla;
    
    // Track names and trigger functions
    const char* trackNames[SEQ_TRACKS];
    void (*triggersFull[SEQ_TRACKS])(float vel, float pitch);  // Trigger with velocity and pitch
} DrumSequencer;

static DrumSequencer seq = {0};

// Random int in range for jitter
static int seqRandInt(int min, int max) {
    noiseState = noiseState * 1103515245 + 12345;
    if (max <= min) return min;
    return min + ((noiseState >> 16) % (max - min + 1));
}

// Calculate the trigger tick for a track on its current step
static int calcTriggerTick(int track) {
    int step = seq.trackStep[track];
    int baseTick = 0;  // Start of step
    
    // Apply per-instrument offset
    switch (track) {
        case 0: baseTick += seq.dilla.kickNudge; break;   // Kick
        case 1: baseTick += seq.dilla.snareDelay; break;  // Snare
        case 2: baseTick += seq.dilla.hatNudge; break;    // HiHat
        case 3: baseTick += seq.dilla.clapDelay; break;   // Clap
    }
    
    // Apply swing to off-beats (odd steps: 1, 3, 5, 7, 9, 11, 13, 15)
    if (step % 2 == 1) {
        baseTick += seq.dilla.swing;
    }
    
    // Apply random jitter
    if (seq.dilla.jitter > 0) {
        baseTick += seqRandInt(-seq.dilla.jitter, seq.dilla.jitter);
    }
    
    // Clamp to valid range (allow slightly early/late but not too crazy)
    if (baseTick < -SEQ_TICKS_PER_STEP / 2) baseTick = -SEQ_TICKS_PER_STEP / 2;
    if (baseTick > SEQ_TICKS_PER_STEP - 1) baseTick = SEQ_TICKS_PER_STEP - 1;
    
    return baseTick;
}

static void resetSequencer(void) {
    seq.tickTimer = 0.0f;
    for (int i = 0; i < SEQ_TRACKS; i++) {
        seq.trackStep[i] = 0;
        seq.trackTick[i] = 0;
        seq.trackTriggered[i] = false;
        seq.trackTriggerTick[i] = calcTriggerTick(i);
    }
}

static void initSequencer(void) {
    memset(seq.steps, 0, sizeof(seq.steps));
    seq.playing = false;
    seq.bpm = 120.0f;
    seq.tickTimer = 0.0f;
    
    // Initialize all velocities and pitches to default
    for (int t = 0; t < SEQ_TRACKS; t++) {
        for (int s = 0; s < SEQ_MAX_STEPS; s++) {
            seq.velocity[t][s] = 0.8f;
            seq.pitch[t][s] = 0.0f;  // 0 = no offset (1.0x pitch)
        }
    }
    
    // Setup tracks - all start at 16 steps
    seq.trackNames[0] = "Kick";
    seq.trackNames[1] = "Snare";
    seq.trackNames[2] = "HiHat";
    seq.trackNames[3] = "Clap";
    
    seq.triggersFull[0] = drumKickFull;
    seq.triggersFull[1] = drumSnareFull;
    seq.triggersFull[2] = drumClosedHHFull;
    seq.triggersFull[3] = drumClapFull;
    
    for (int i = 0; i < SEQ_TRACKS; i++) {
        seq.trackLength[i] = 16;
        seq.trackStep[i] = 0;
        seq.trackTick[i] = 0;
        seq.trackTriggerTick[i] = 0;
        seq.trackTriggered[i] = false;
    }
    
    // Default Dilla timing (MPC-style feel)
    // 24 ticks = 1 step, so these are subtle micro-timing shifts
    seq.dilla.kickNudge = -2;    // Kicks slightly early (punchy)
    seq.dilla.snareDelay = 4;    // Snares lazy/late (laid back)
    seq.dilla.hatNudge = 0;      // Hats on grid
    seq.dilla.clapDelay = 3;     // Claps slightly late
    seq.dilla.swing = 6;         // Off-beats pushed late
    seq.dilla.jitter = 2;        // Subtle humanization ±2 ticks
}

static void updateSequencer(float dt) {
    if (!seq.playing) return;
    
    // Calculate tick duration: 60 / BPM / PPQ
    float tickDuration = 60.0f / seq.bpm / (float)SEQ_PPQ;
    
    seq.tickTimer += dt;
    
    while (seq.tickTimer >= tickDuration) {
        seq.tickTimer -= tickDuration;
        
        // Process each track independently (for polyrhythm)
        for (int track = 0; track < SEQ_TRACKS; track++) {
            int step = seq.trackStep[track];
            int tick = seq.trackTick[track];
            
            // Check if we should trigger on this tick
            if (seq.steps[track][step] && !seq.trackTriggered[track]) {
                if (tick >= seq.trackTriggerTick[track]) {
                    // Convert pitch offset (-1 to +1) to multiplier (0.5 to 2.0)
                    float pitchMod = powf(2.0f, seq.pitch[track][step]);
                    seq.triggersFull[track](seq.velocity[track][step], pitchMod);
                    seq.trackTriggered[track] = true;
                }
            }
            
            // Advance tick for this track
            seq.trackTick[track]++;
            
            // Check if track completed its step (24 ticks)
            if (seq.trackTick[track] >= SEQ_TICKS_PER_STEP) {
                seq.trackTick[track] = 0;
                seq.trackStep[track] = (seq.trackStep[track] + 1) % seq.trackLength[track];
                seq.trackTriggered[track] = false;
                seq.trackTriggerTick[track] = calcTriggerTick(track);
            }
        }
    }
}

// Performance tracking
static double audioTimeUs = 0.0;
static int audioFrameCount = 0;

// Simple noise generator (noiseState declared earlier)
static float noise(void) {
    noiseState = noiseState * 1103515245 + 12345;
    return (float)(noiseState >> 16) / 32768.0f - 1.0f;
}

// Clamp float to range
static float clampf(float x, float min, float max) {
    if (x < min) return min;
    if (x > max) return max;
    return x;
}

// Linear interpolation
static float lerpf(float a, float b, float t) {
    return a * (1.0f - t) + b * t;
}

// Formant filter (bandpass) processing
static float processFormantFilter(FormantFilter *f, float input, float sampleRate) {
    float fc = clampf(2.0f * sinf(PI * f->freq / sampleRate), 0.001f, 0.99f);
    float q = clampf(f->freq / (f->bw + 1.0f), 0.5f, 20.0f);
    
    f->low += fc * f->band;
    f->high = input - f->low - f->band / q;
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
    
    // Interpolate formant parameters and apply filters
    VowelType v1 = vs->vowel;
    VowelType v2 = vs->nextVowel;
    float blend = vs->vowelBlend;
    
    float out = 0.0f;
    for (int i = 0; i < 3; i++) {
        float freq = lerpf(formantFreq[v1][i], formantFreq[v2][i], blend) * vs->formantShift;
        float bw = lerpf(formantBw[v1][i], formantBw[v2][i], blend);
        float amp = lerpf(formantAmp[v1][i], formantAmp[v2][i], blend);
        
        vs->formants[i].freq = freq;
        vs->formants[i].bw = bw;
        out += processFormantFilter(&vs->formants[i], source, sampleRate) * amp;
    }
    
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
        v->baseFrequency = clampf(v->baseFrequency + v->pitchSlide, 20.0f, 20000.0f);
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
        pw = clampf(pw + sinf(v->pwmPhase * 2.0f * PI) * v->pwmDepth, 0.1f, 0.9f);
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
    float dt = 1.0f / SAMPLE_RATE;
    
    for (unsigned int i = 0; i < frames; i++) {
        float sample = 0.0f;
        
        // Process synth voices
        for (int v = 0; v < NUM_VOICES; v++) {
            sample += processVoice(&voices[v], SAMPLE_RATE);
        }
        
        // Process drum machine
        sample += processDrums(dt);
        
        sample *= masterVolume;
        
        // Process effects chain
        sample = processEffects(sample, dt);
        
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

// ============================================================================
// 808-STYLE DRUM SYNTHESIS
// ============================================================================

// Drum-specific noise generator (faster, independent state per voice)
static float drumNoise(unsigned int *state) {
    *state = *state * 1103515245 + 12345;
    return (float)(*state >> 16) / 32768.0f - 1.0f;
}

// Exponential decay envelope
static float expDecay(float time, float decay) {
    if (decay <= 0.0f) return 0.0f;
    return expf(-time / (decay * 0.368f));  // 0.368 = 1/e, so decay = time to reach ~37%
}

// Trigger a drum sound with velocity and pitch
static void triggerDrumFull(DrumType type, float velocity, float pitchMod) {
    DrumVoice *dv = &drumVoices[type];
    dv->active = true;
    dv->time = 0.0f;
    dv->phase = 0.0f;
    dv->phase2 = 0.0f;
    dv->pitchEnv = 1.0f;
    dv->ampEnv = 1.0f;
    dv->filterLp = 0.0f;
    dv->filterHp = 0.0f;
    dv->velocity = velocity;
    dv->pitchMod = pitchMod;
    
    // Initialize hihat oscillator phases with metallic ratios
    if (type == DRUM_CLOSED_HH || type == DRUM_OPEN_HH) {
        for (int i = 0; i < 6; i++) {
            dv->hhPhases[i] = 0.0f;
        }
    }
    
    // Open hihat chokes closed hihat
    if (type == DRUM_CLOSED_HH) {
        drumVoices[DRUM_OPEN_HH].active = false;
    }
}

// Trigger a drum sound with velocity (normal pitch)
static void triggerDrumWithVel(DrumType type, float velocity) {
    triggerDrumFull(type, velocity, 1.0f);
}

// Trigger a drum sound (full velocity, normal pitch)
static void triggerDrum(DrumType type) {
    triggerDrumFull(type, 1.0f, 1.0f);
}

// Process kick drum - sine with pitch envelope + optional click
static float processKick(DrumVoice *dv, float dt) {
    if (!dv->active) return 0.0f;
    
    DrumParams *p = &drumParams;
    dv->time += dt;
    
    // Pitch envelope: exponential decay from punch pitch to base pitch
    float pitchT = expDecay(dv->time, p->kickPunchDecay);
    float freq = (p->kickPitch + (p->kickPunchPitch - p->kickPitch) * pitchT) * dv->pitchMod;
    
    // Advance phase
    dv->phase += freq * dt;
    if (dv->phase >= 1.0f) dv->phase -= 1.0f;
    
    // Pure sine oscillator
    float osc = sinf(dv->phase * 2.0f * PI);
    
    // Add click transient (short noise burst at start)
    float click = 0.0f;
    if (p->kickClick > 0.0f && dv->time < 0.01f) {
        unsigned int ns = (unsigned int)(dv->time * 1000000);
        click = drumNoise(&ns) * (1.0f - dv->time / 0.01f) * p->kickClick;
    }
    
    // Soft saturation for more tone/punch
    float sample = osc + click;
    if (p->kickTone > 0.0f) {
        sample = tanhf(sample * (1.0f + p->kickTone * 3.0f));
    }
    
    // Amplitude envelope
    float amp = expDecay(dv->time, p->kickDecay);
    if (amp < 0.001f) dv->active = false;
    
    return sample * amp * 0.8f;
}

// Process snare - tuned oscillators + filtered noise
static float processSnare(DrumVoice *dv, float dt) {
    if (!dv->active) return 0.0f;
    
    DrumParams *p = &drumParams;
    dv->time += dt;
    
    // Two tuned oscillators (body) - apply pitch mod
    float freq1 = p->snarePitch * dv->pitchMod;
    float freq2 = p->snarePitch * 1.5f * dv->pitchMod;  // Fifth above
    
    dv->phase += freq1 * dt;
    if (dv->phase >= 1.0f) dv->phase -= 1.0f;
    dv->phase2 += freq2 * dt;
    if (dv->phase2 >= 1.0f) dv->phase2 -= 1.0f;
    
    float tone = sinf(dv->phase * 2.0f * PI) * 0.6f + 
                 sinf(dv->phase2 * 2.0f * PI) * 0.3f;
    
    // Noise component (snares)
    unsigned int ns = (unsigned int)(dv->time * 1000000 + dv->phase * 10000);
    float noiseSample = drumNoise(&ns);
    
    // Bandpass filter the noise (sounds more like real snares)
    float cutoff = 0.15f + p->snareTone * 0.4f;
    dv->filterLp += cutoff * (noiseSample - dv->filterLp);
    dv->filterHp += 0.1f * (dv->filterLp - dv->filterHp);
    float filteredNoise = dv->filterLp - dv->filterHp;
    
    // Mix tone and noise based on snappy parameter
    float mix = tone * (1.0f - p->snareSnappy * 0.7f) + 
                filteredNoise * p->snareSnappy * 1.5f;
    
    // Amplitude envelope - noise decays slightly slower
    float toneAmp = expDecay(dv->time, p->snareDecay * 0.7f);
    float noiseAmp = expDecay(dv->time, p->snareDecay);
    float amp = toneAmp * (1.0f - p->snareSnappy * 0.5f) + noiseAmp * p->snareSnappy * 0.5f;
    
    if (amp < 0.001f) dv->active = false;
    
    return mix * amp * 0.7f;
}

// Process clap - multiple noise bursts
static float processClap(DrumVoice *dv, float dt) {
    if (!dv->active) return 0.0f;
    
    DrumParams *p = &drumParams;
    dv->time += dt;
    
    // 4 "hands" clapping with slight time offsets
    float spread = p->clapSpread;
    float offsets[4] = {0.0f, spread, spread * 2.2f, spread * 3.5f};
    
    float sample = 0.0f;
    for (int i = 0; i < 4; i++) {
        float t = dv->time - offsets[i];
        if (t >= 0.0f) {
            // Each clap is a short noise burst
            unsigned int ns = (unsigned int)(t * 1000000 + i * 12345);
            float n = drumNoise(&ns);
            
            // Very fast attack, quick decay per burst
            float burstEnv = expDecay(t, 0.02f);
            sample += n * burstEnv * 0.4f;
        }
    }
    
    // Overall envelope
    float amp = expDecay(dv->time, p->clapDecay);
    
    // Bandpass filter for that clap character
    float cutoff = 0.2f + p->clapTone * 0.3f;
    dv->filterLp += cutoff * (sample - dv->filterLp);
    dv->filterHp += 0.08f * (dv->filterLp - dv->filterHp);
    sample = (dv->filterLp - dv->filterHp) * 2.0f;
    
    if (amp < 0.001f) dv->active = false;
    
    return sample * amp * 0.6f;
}

// Process hihat - 6 square wave oscillators at metallic ratios
static float processHihat(DrumVoice *dv, float dt, bool open) {
    if (!dv->active) return 0.0f;
    
    DrumParams *p = &drumParams;
    dv->time += dt;
    
    // 808 hihat uses 6 square waves at these metallic (non-harmonic) frequencies
    // Based on analysis of actual 808 circuits
    static const float hhFreqRatios[6] = {
        1.0f, 1.4471f, 1.6170f, 1.9265f, 2.5028f, 2.6637f
    };
    float baseFreq = (320.0f + p->hhTone * 200.0f) * dv->pitchMod;  // Higher tone = higher base freq
    
    float sample = 0.0f;
    for (int i = 0; i < 6; i++) {
        float freq = baseFreq * hhFreqRatios[i];
        dv->hhPhases[i] += freq * dt;
        if (dv->hhPhases[i] >= 1.0f) dv->hhPhases[i] -= 1.0f;
        
        // Square wave
        float sq = dv->hhPhases[i] < 0.5f ? 1.0f : -1.0f;
        sample += sq;
    }
    sample /= 6.0f;  // Normalize
    
    // Highpass filter - hihats are very bright
    float hpCutoff = 0.3f + p->hhTone * 0.4f;
    dv->filterHp += hpCutoff * (sample - dv->filterHp);
    sample = sample - dv->filterHp;
    
    // Decay time depends on open/closed
    float decay = open ? p->hhDecayOpen : p->hhDecayClosed;
    float amp = expDecay(dv->time, decay);
    
    if (amp < 0.001f) dv->active = false;
    
    return sample * amp * 0.4f;
}

// Process tom - similar to kick but higher pitch, less pitch drop
static float processTom(DrumVoice *dv, float dt, float pitchMult) {
    if (!dv->active) return 0.0f;
    
    DrumParams *p = &drumParams;
    dv->time += dt;
    
    // Base frequencies for low/mid/high tom
    float basePitch = 80.0f * pitchMult * p->tomPitch;
    float punchPitch = basePitch * 2.0f;
    
    // Pitch envelope
    float pitchT = expDecay(dv->time, p->tomPunchDecay);
    float freq = basePitch + (punchPitch - basePitch) * pitchT;
    
    dv->phase += freq * dt;
    if (dv->phase >= 1.0f) dv->phase -= 1.0f;
    
    // Sine + slight triangle for more body
    float osc = sinf(dv->phase * 2.0f * PI) * 0.8f +
                (4.0f * fabsf(dv->phase - 0.5f) - 1.0f) * 0.2f;
    
    float amp = expDecay(dv->time, p->tomDecay);
    if (amp < 0.001f) dv->active = false;
    
    return osc * amp * 0.6f;
}

// Process rimshot - sharp click + high tone
static float processRimshot(DrumVoice *dv, float dt) {
    if (!dv->active) return 0.0f;
    
    DrumParams *p = &drumParams;
    dv->time += dt;
    
    // High frequency click
    dv->phase += p->rimPitch * dt;
    if (dv->phase >= 1.0f) dv->phase -= 1.0f;
    
    float osc = sinf(dv->phase * 2.0f * PI);
    
    // Add noise click
    unsigned int ns = (unsigned int)(dv->time * 1000000);
    float click = drumNoise(&ns) * expDecay(dv->time, 0.005f);
    
    float amp = expDecay(dv->time, p->rimDecay);
    if (amp < 0.001f) dv->active = false;
    
    return (osc * 0.5f + click * 0.5f) * amp * 0.5f;
}

// Process cowbell - two square waves at non-harmonic intervals
static float processCowbell(DrumVoice *dv, float dt) {
    if (!dv->active) return 0.0f;
    
    DrumParams *p = &drumParams;
    dv->time += dt;
    
    // Two frequencies that create the cowbell "clank"
    // 808 cowbell: ~560 Hz and ~845 Hz (ratio ~1.51)
    float freq1 = p->cowbellPitch;
    float freq2 = p->cowbellPitch * 1.508f;
    
    dv->phase += freq1 * dt;
    if (dv->phase >= 1.0f) dv->phase -= 1.0f;
    dv->phase2 += freq2 * dt;
    if (dv->phase2 >= 1.0f) dv->phase2 -= 1.0f;
    
    // Square waves
    float sq1 = dv->phase < 0.5f ? 1.0f : -1.0f;
    float sq2 = dv->phase2 < 0.5f ? 1.0f : -1.0f;
    
    float sample = (sq1 + sq2) * 0.5f;
    
    // Bandpass filter for that metallic sound
    float cutoff = 0.15f;
    dv->filterLp += cutoff * (sample - dv->filterLp);
    sample = dv->filterLp;
    
    float amp = expDecay(dv->time, p->cowbellDecay);
    if (amp < 0.001f) dv->active = false;
    
    return sample * amp * 0.4f;
}

// Process clave - very short filtered click
static float processClave(DrumVoice *dv, float dt) {
    if (!dv->active) return 0.0f;
    
    DrumParams *p = &drumParams;
    dv->time += dt;
    
    // High pitched resonant click
    dv->phase += p->clavePitch * dt;
    if (dv->phase >= 1.0f) dv->phase -= 1.0f;
    
    float osc = sinf(dv->phase * 2.0f * PI);
    
    // Very fast decay for that "click"
    float amp = expDecay(dv->time, p->claveDecay);
    if (amp < 0.001f) dv->active = false;
    
    return osc * amp * 0.5f;
}

// Process maracas - filtered noise burst
static float processMaracas(DrumVoice *dv, float dt) {
    if (!dv->active) return 0.0f;
    
    DrumParams *p = &drumParams;
    dv->time += dt;
    
    unsigned int ns = (unsigned int)(dv->time * 1000000);
    float sample = drumNoise(&ns);
    
    // Highpass for brightness
    float cutoff = 0.3f + p->maracasTone * 0.4f;
    dv->filterHp += cutoff * (sample - dv->filterHp);
    sample = sample - dv->filterHp;
    
    float amp = expDecay(dv->time, p->maracasDecay);
    if (amp < 0.001f) dv->active = false;
    
    return sample * amp * 0.25f;
}

// Process all drum voices and return mixed sample
static float processDrums(float dt) {
    float sample = 0.0f;
    
    sample += processKick(&drumVoices[DRUM_KICK], dt) * drumVoices[DRUM_KICK].velocity;
    sample += processSnare(&drumVoices[DRUM_SNARE], dt) * drumVoices[DRUM_SNARE].velocity;
    sample += processClap(&drumVoices[DRUM_CLAP], dt) * drumVoices[DRUM_CLAP].velocity;
    sample += processHihat(&drumVoices[DRUM_CLOSED_HH], dt, false) * drumVoices[DRUM_CLOSED_HH].velocity;
    sample += processHihat(&drumVoices[DRUM_OPEN_HH], dt, true) * drumVoices[DRUM_OPEN_HH].velocity;
    sample += processTom(&drumVoices[DRUM_LOW_TOM], dt, 1.0f) * drumVoices[DRUM_LOW_TOM].velocity;
    sample += processTom(&drumVoices[DRUM_MID_TOM], dt, 1.5f) * drumVoices[DRUM_MID_TOM].velocity;
    sample += processTom(&drumVoices[DRUM_HI_TOM], dt, 2.2f) * drumVoices[DRUM_HI_TOM].velocity;
    sample += processRimshot(&drumVoices[DRUM_RIMSHOT], dt) * drumVoices[DRUM_RIMSHOT].velocity;
    sample += processCowbell(&drumVoices[DRUM_COWBELL], dt) * drumVoices[DRUM_COWBELL].velocity;
    sample += processClave(&drumVoices[DRUM_CLAVE], dt) * drumVoices[DRUM_CLAVE].velocity;
    sample += processMaracas(&drumVoices[DRUM_MARACAS], dt) * drumVoices[DRUM_MARACAS].velocity;
    
    return sample * drumVolume;
}

// Convenience functions to trigger drums by name (full velocity)
static void drumKick(void) { triggerDrum(DRUM_KICK); }
static void drumSnare(void) { triggerDrum(DRUM_SNARE); }
static void drumClap(void) { triggerDrum(DRUM_CLAP); }
static void drumClosedHH(void) { triggerDrum(DRUM_CLOSED_HH); }
static void drumOpenHH(void) { triggerDrum(DRUM_OPEN_HH); }
static void drumLowTom(void) { triggerDrum(DRUM_LOW_TOM); }
static void drumMidTom(void) { triggerDrum(DRUM_MID_TOM); }
static void drumHiTom(void) { triggerDrum(DRUM_HI_TOM); }
static void drumRimshot(void) { triggerDrum(DRUM_RIMSHOT); }
static void drumCowbell(void) { triggerDrum(DRUM_COWBELL); }
static void drumClave(void) { triggerDrum(DRUM_CLAVE); }
static void drumMaracas(void) { triggerDrum(DRUM_MARACAS); }

// Convenience functions with velocity and pitch (for sequencer)
static void drumKickFull(float vel, float pitch) { triggerDrumFull(DRUM_KICK, vel, pitch); }
static void drumSnareFull(float vel, float pitch) { triggerDrumFull(DRUM_SNARE, vel, pitch); }
static void drumClosedHHFull(float vel, float pitch) { triggerDrumFull(DRUM_CLOSED_HH, vel, pitch); }
static void drumClapFull(float vel, float pitch) { triggerDrumFull(DRUM_CLAP, vel, pitch); }

// Include generative music system (after drums and before voice functions)
#include "genmusic.h"

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
    sq->speed = clampf(speed, 1.0f, 30.0f);
    sq->basePitch = clampf(pitch, 0.3f, 3.0f);
    sq->pitchVariation = clampf(variation, 0.0f, 1.0f);
    sq->active = true;
    sq->voiceIndex = NUM_VOICES - 1;
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
    memset(drumVoices, 0, sizeof(drumVoices));
    initDrumParams();
    initEffects();
    initSequencer();
    
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
        
        // Drums - Z row for main kit, X row for extras
        if (IsKeyPressed(KEY_Z)) drumKick();
        if (IsKeyPressed(KEY_X)) drumSnare();
        if (IsKeyPressed(KEY_C)) drumClap();
        if (IsKeyPressed(KEY_SEVEN)) drumClosedHH();
        if (IsKeyPressed(KEY_EIGHT)) drumOpenHH();
        if (IsKeyPressed(KEY_NINE))  drumLowTom();
        if (IsKeyPressed(KEY_ZERO))  drumMidTom();
        if (IsKeyPressed(KEY_MINUS)) drumHiTom();
        if (IsKeyPressed(KEY_EQUAL)) drumRimshot();
        if (IsKeyPressed(KEY_LEFT_BRACKET)) drumCowbell();
        if (IsKeyPressed(KEY_RIGHT_BRACKET)) drumClave();
        if (IsKeyPressed(KEY_BACKSLASH)) drumMaracas();
        
        // Voice/Speech (B=babble, SPACE=speak, hold V for vowel)
        if (IsKeyPressed(KEY_B)) babble(2.0f, voicePitch, 0.5f);
        if (IsKeyPressed(KEY_N)) speak("hello world", voiceSpeed, voicePitch, 0.3f);
        if (IsKeyPressed(KEY_V)) {
            vowelKeyVoice = playVowel(200.0f * voicePitch, (VowelType)voiceVowel);
        }
        if (IsKeyReleased(KEY_V) && vowelKeyVoice >= 0) {
            releaseNote(vowelKeyVoice);
            vowelKeyVoice = -1;
        }
        
        // Update speech system
        updateSpeech(GetFrameTime());
        
        // Generative music (SPACE to toggle) - only if sequencer not playing
        if (IsKeyPressed(KEY_SPACE)) {
            if (!seq.playing) {
                toggleGenMusic();
            }
        }
        updateGenMusic(GetFrameTime());
        
        // Drum sequencer
        updateSequencer(GetFrameTime());
        
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
        DrawTextEx(font, "SFX: 1-6  Notes: QWERTYU/ASDFGHJ", (Vector2){20, 55}, 12, 1, LIGHTGRAY);
        DrawTextEx(font, "Drums: Z=kick X=snare C=clap 7/8=HH", (Vector2){20, 70}, 12, 1, LIGHTGRAY);
        DrawTextEx(font, "Voice: V=vowel B=babble N=speak", (Vector2){20, 85}, 12, 1, LIGHTGRAY);
        DrawTextEx(font, "SPACE = Toggle Generative Music", (Vector2){20, 100}, 12, 1, genMusic.active ? GREEN : LIGHTGRAY);
        
        // Show active voices
        DrawTextEx(font, "Voices:", (Vector2){20, 120}, 12, 1, GRAY);
        for (int i = 0; i < NUM_VOICES; i++) {
            Color c = DARKGRAY;
            if (voices[i].envStage == 4) c = ORANGE;
            else if (voices[i].envStage > 0) c = GREEN;
            DrawRectangle(75 + i * 18, 120, 14, 12, c);
        }
        
        // Performance stats
        double bufferTimeMs = (double)audioFrameCount / SAMPLE_RATE * 1000.0;
        double cpuPercent = (audioTimeUs / 1000.0) / bufferTimeMs * 100.0;
        DrawTextEx(font, TextFormat("Audio: %.0fus (%.1f%%)  FPS: %d", 
                 audioTimeUs, cpuPercent, GetFPS()), (Vector2){20, 140}, 12, 1, GRAY);
        
        ToggleBool(20, 160, "SFX Randomize", &sfxRandomize);
        ToggleBool(20, 180, "Show Gen Music", &showGenMusic);
        
        // Generative music info (hidden by default)
        if (showGenMusic && genMusic.active) {
            // Key display
            int rootIdx = (genMusic.rootNote - 36) % 12;
            if (rootIdx < 0) rootIdx += 12;
            DrawTextEx(font, TextFormat("%s | %s %s | Bar %d", 
                     STYLE_NAMES[genMusic.style],
                     ROOT_NAMES[rootIdx], SCALES[genMusic.scaleIndex].name,
                     genMusic.bar + 1), 
                     (Vector2){20, 185}, 12, 1, GREEN);
            
            // Style selector
            int oldStyle = genMusic.style;
            CycleOption(20, 205, "Style", STYLE_NAMES, STYLE_COUNT, &genMusic.style);
            if (genMusic.style != oldStyle) setGenMusicStyle(genMusic.style);
            
            // Scale selector
            static const char* scaleNames[NUM_SCALES];
            static bool scaleNamesInit = false;
            if (!scaleNamesInit) {
                for (int i = 0; i < NUM_SCALES; i++) scaleNames[i] = SCALES[i].name;
                scaleNamesInit = true;
            }
            int oldScale = genMusic.scaleIndex;
            CycleOption(20, 227, "Scale", scaleNames, NUM_SCALES, &genMusic.scaleIndex);
            if (genMusic.scaleIndex != oldScale) setGenMusicScale(genMusic.scaleIndex);
            
            // Root note selector
            int rootNote = rootIdx;
            int oldRoot = rootNote;
            CycleOption(20, 249, "Root", ROOT_NAMES, 12, &rootNote);
            if (rootNote != oldRoot) setGenMusicRoot(rootNote);
            
            DraggableFloat(20, 271, "BPM", &genMusic.bpm, 5.0f, 60.0f, 180.0f);
            DraggableFloat(20, 289, "Swing", &genMusic.swing, 0.025f, 0.0f, 0.5f);
        }
        
        // Show speaking indicator
        if (speechQueue.active) {
            DrawTextEx(font, "Speaking...", (Vector2){20, 315}, 14, 1, GREEN);
        }
        
        // === COLUMN 1: Wave & Envelope ===
        UIColumn col1 = ui_column(250, 20, 20);
        
        ui_col_label(&col1, "Wave:", YELLOW);
        ui_col_cycle(&col1, "Type", waveNames, 6, &selectedWave);
        
        // SCW selector (only if SCW wave selected)
        if (selectedWave == WAVE_SCW && scwCount > 0) {
            const char* scwNames[SCW_MAX_SLOTS];
            for (int i = 0; i < scwCount; i++) scwNames[i] = scwTables[i].name;
            ui_col_cycle(&col1, "SCW", scwNames, scwCount, &noteScwIndex);
        }
        ui_col_space(&col1, 4);
        
        ui_col_sublabel(&col1, "Envelope:", ORANGE);
        ui_col_float(&col1, "Attack", &noteAttack, 0.5f, 0.001f, 2.0f);
        ui_col_float(&col1, "Decay", &noteDecay, 0.5f, 0.0f, 2.0f);
        ui_col_float(&col1, "Sustain", &noteSustain, 0.5f, 0.0f, 1.0f);
        ui_col_float(&col1, "Release", &noteRelease, 0.5f, 0.01f, 3.0f);
        ui_col_space(&col1, 4);
        
        // PWM (only for square)
        if (selectedWave == WAVE_SQUARE) {
            ui_col_sublabel(&col1, "PWM:", ORANGE);
            ui_col_float(&col1, "Width", &notePulseWidth, 0.05f, 0.1f, 0.9f);
            ui_col_float(&col1, "Rate", &notePwmRate, 0.5f, 0.1f, 20.0f);
            ui_col_float(&col1, "Depth", &notePwmDepth, 0.02f, 0.0f, 0.4f);
            ui_col_space(&col1, 4);
        }
        
        ui_col_sublabel(&col1, "Vibrato:", ORANGE);
        ui_col_float(&col1, "Rate", &noteVibratoRate, 0.5f, 0.5f, 15.0f);
        ui_col_float(&col1, "Depth", &noteVibratoDepth, 0.2f, 0.0f, 2.0f);
        ui_col_space(&col1, 4);
        
        ui_col_sublabel(&col1, "Filter:", ORANGE);
        ui_col_float(&col1, "Cutoff", &noteFilterCutoff, 0.05f, 0.05f, 1.0f);
        ui_col_space(&col1, 4);
        
        ui_col_sublabel(&col1, "Volume:", ORANGE);
        ui_col_float(&col1, "Note", &noteVolume, 0.05f, 0.0f, 1.0f);
        ui_col_float(&col1, "Master", &masterVolume, 0.05f, 0.0f, 1.0f);
        
        // === COLUMN 2: Voice/Speech ===
        UIColumn col2 = ui_column(430, 20, 20);
        
        ui_col_label(&col2, "Vox:", YELLOW);
        ui_col_cycle(&col2, "Vowel", vowelNames, 5, &voiceVowel);
        ui_col_float(&col2, "Pitch", &voicePitch, 0.1f, 0.3f, 2.0f);
        ui_col_float(&col2, "Speed", &voiceSpeed, 1.0f, 4.0f, 20.0f);
        ui_col_float(&col2, "Formant", &voiceFormantShift, 0.05f, 0.5f, 1.5f);
        ui_col_float(&col2, "Breath", &voiceBreathiness, 0.05f, 0.0f, 1.0f);
        ui_col_float(&col2, "Buzz", &voiceBuzziness, 0.05f, 0.0f, 1.0f);
        ui_col_space(&col2, 8);
        
        // Gen music instrument params (if shown)
        if (showGenMusic) {
            static const char* genWaveNames[] = {"Sq", "Saw", "Tri", "Nse", "SCW", "Vox"};
            
            ui_col_sublabel(&col2, "Gen Instruments:", ORANGE);
            ui_col_sublabel(&col2, "Bass:", SKYBLUE);
            ui_col_cycle(&col2, "Wave", genWaveNames, 6, &genMusic.bass.wave);
            ui_col_float(&col2, "Vol", &genMusic.bass.volume, 0.05f, 0.0f, 1.0f);
            ui_col_float(&col2, "Oct", &genMusic.bass.pitchOctave, 0.2f, -2.0f, 2.0f);
            
            ui_col_sublabel(&col2, "Melody:", SKYBLUE);
            ui_col_cycle(&col2, "Wave", genWaveNames, 6, &genMusic.melody.wave);
            ui_col_float(&col2, "Vol", &genMusic.melody.volume, 0.05f, 0.0f, 1.0f);
            ui_col_float(&col2, "Oct", &genMusic.melody.pitchOctave, 0.25f, -1.0f, 4.0f);
            
            ui_col_sublabel(&col2, "Vox:", SKYBLUE);
            ui_col_float(&col2, "Vol", &genMusic.vox.volume, 0.05f, 0.0f, 1.0f);
            ui_col_float(&col2, "Oct", &genMusic.vox.pitchOctave, 0.2f, -1.0f, 3.0f);
        }
        
        // === COLUMN 3: Drums (808-style) ===
        UIColumn col3 = ui_column(610, 20, 20);
        
        ui_col_label(&col3, "Drums:", YELLOW);
        ui_col_float(&col3, "Volume", &drumVolume, 0.05f, 0.0f, 1.0f);
        ui_col_space(&col3, 4);
        
        ui_col_sublabel(&col3, "Kick (Z):", ORANGE);
        ui_col_float(&col3, "Pitch", &drumParams.kickPitch, 3.0f, 30.0f, 100.0f);
        ui_col_float(&col3, "Decay", &drumParams.kickDecay, 0.07f, 0.1f, 1.5f);
        ui_col_float(&col3, "Punch", &drumParams.kickPunchPitch, 10.0f, 80.0f, 300.0f);
        ui_col_float(&col3, "Click", &drumParams.kickClick, 0.05f, 0.0f, 1.0f);
        ui_col_float(&col3, "Tone", &drumParams.kickTone, 0.05f, 0.0f, 1.0f);
        ui_col_space(&col3, 4);
        
        ui_col_sublabel(&col3, "Snare (X):", ORANGE);
        ui_col_float(&col3, "Pitch", &drumParams.snarePitch, 10.0f, 100.0f, 350.0f);
        ui_col_float(&col3, "Decay", &drumParams.snareDecay, 0.03f, 0.05f, 0.6f);
        ui_col_float(&col3, "Snappy", &drumParams.snareSnappy, 0.05f, 0.0f, 1.0f);
        ui_col_float(&col3, "Tone", &drumParams.snareTone, 0.05f, 0.0f, 1.0f);
        ui_col_space(&col3, 4);
        
        ui_col_sublabel(&col3, "HiHat (7/8):", ORANGE);
        ui_col_float(&col3, "Closed", &drumParams.hhDecayClosed, 0.01f, 0.01f, 0.2f);
        ui_col_float(&col3, "Open", &drumParams.hhDecayOpen, 0.05f, 0.1f, 1.0f);
        ui_col_float(&col3, "Tone", &drumParams.hhTone, 0.05f, 0.0f, 1.0f);
        ui_col_space(&col3, 4);
        
        ui_col_sublabel(&col3, "Clap (C):", ORANGE);
        ui_col_float(&col3, "Decay", &drumParams.clapDecay, 0.03f, 0.1f, 0.6f);
        ui_col_float(&col3, "Spread", &drumParams.clapSpread, 0.001f, 0.005f, 0.03f);
        
        // === COLUMN 4: Effects Pedals ===
        UIColumn col4 = ui_column(790, 20, 20);
        
        ui_col_label(&col4, "Effects:", YELLOW);
        
        ui_col_sublabel(&col4, "Distortion:", ORANGE);
        ui_col_toggle(&col4, "On", &fx.distEnabled);
        ui_col_float(&col4, "Drive", &fx.distDrive, 0.5f, 1.0f, 20.0f);
        ui_col_float(&col4, "Tone", &fx.distTone, 0.05f, 0.0f, 1.0f);
        ui_col_float(&col4, "Mix", &fx.distMix, 0.05f, 0.0f, 1.0f);
        ui_col_space(&col4, 4);
        
        ui_col_sublabel(&col4, "Delay:", ORANGE);
        ui_col_toggle(&col4, "On", &fx.delayEnabled);
        ui_col_float(&col4, "Time", &fx.delayTime, 0.05f, 0.05f, 1.0f);
        ui_col_float(&col4, "Feedback", &fx.delayFeedback, 0.05f, 0.0f, 0.9f);
        ui_col_float(&col4, "Tone", &fx.delayTone, 0.05f, 0.0f, 1.0f);
        ui_col_float(&col4, "Mix", &fx.delayMix, 0.05f, 0.0f, 1.0f);
        ui_col_space(&col4, 4);
        
        ui_col_sublabel(&col4, "Tape:", ORANGE);
        ui_col_toggle(&col4, "On", &fx.tapeEnabled);
        ui_col_float(&col4, "Saturation", &fx.tapeSaturation, 0.05f, 0.0f, 1.0f);
        ui_col_float(&col4, "Wow", &fx.tapeWow, 0.05f, 0.0f, 1.0f);
        ui_col_float(&col4, "Flutter", &fx.tapeFlutter, 0.05f, 0.0f, 1.0f);
        ui_col_float(&col4, "Hiss", &fx.tapeHiss, 0.05f, 0.0f, 1.0f);
        ui_col_space(&col4, 4);
        
        ui_col_sublabel(&col4, "Bitcrusher:", ORANGE);
        ui_col_toggle(&col4, "On", &fx.crushEnabled);
        ui_col_float(&col4, "Bits", &fx.crushBits, 0.5f, 2.0f, 16.0f);
        ui_col_float(&col4, "Rate", &fx.crushRate, 1.0f, 1.0f, 32.0f);
        ui_col_float(&col4, "Mix", &fx.crushMix, 0.05f, 0.0f, 1.0f);
        
        // === DRUM SEQUENCER GRID (bottom of screen) ===
        {
            // Drag state (persists across frames)
            static bool isDragging = false;
            static bool isDraggingPitch = false;  // true = pitch, false = velocity
            static int dragTrack = -1;
            static int dragStep = -1;
            static float dragStartY = 0.0f;
            static float dragStartVal = 0.0f;
            
            int gridX = 20;
            int gridY = SCREEN_HEIGHT - 130;
            int cellW = 24;
            int cellH = 22;
            int labelW = 50;
            int lengthW = 30;  // Width for length control
            
            // Header
            DrawTextShadow("Drum Sequencer - drag=velocity, shift+drag=pitch, right-click=delete", gridX, gridY - 25, 14, YELLOW);
            
            // Play/Stop button
            if (PushButton(gridX + labelW + SEQ_MAX_STEPS * cellW + lengthW + 15, gridY - 25, seq.playing ? "Stop" : "Play")) {
                seq.playing = !seq.playing;
                if (seq.playing) {
                    resetSequencer();
                    // Stop gen music if it's playing
                    if (genMusic.active) toggleGenMusic();
                }
            }
            
            // BPM control
            DraggableFloat(gridX + labelW + SEQ_MAX_STEPS * cellW + lengthW + 75, gridY - 25, "BPM", &seq.bpm, 2.0f, 60.0f, 200.0f);
            
            // Beat markers (1, 2, 3, 4) - above the first cell of each group
            for (int i = 0; i < 4; i++) {
                int x = gridX + labelW + i * 4 * cellW + 2;
                DrawTextShadow(TextFormat("%d", i + 1), x, gridY - 10, 10, GRAY);
            }
            
            // "Len" header
            DrawTextShadow("Len", gridX + labelW + SEQ_MAX_STEPS * cellW + 5, gridY - 10, 10, GRAY);
            
            // Grid
            Vector2 mouse = GetMousePosition();
            bool mouseClicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
            bool mouseDown = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
            bool mouseReleased = IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
            bool rightClicked = IsMouseButtonPressed(MOUSE_RIGHT_BUTTON);
            
            // Handle drag release
            if (mouseReleased && isDragging) {
                isDragging = false;
                dragTrack = -1;
                dragStep = -1;
            }
            
            // Update value while dragging
            if (isDragging && mouseDown && dragTrack >= 0 && dragStep >= 0) {
                float deltaY = dragStartY - mouse.y;  // Up = increase
                if (isDraggingPitch) {
                    // Pitch: -1 to +1 (one octave down to one octave up)
                    float newPitch = dragStartVal + deltaY * 0.01f;
                    seq.pitch[dragTrack][dragStep] = clampf(newPitch, -1.0f, 1.0f);
                } else {
                    // Velocity: 0.1 to 1.0
                    float newVel = dragStartVal + deltaY * 0.01f;
                    seq.velocity[dragTrack][dragStep] = clampf(newVel, 0.1f, 1.0f);
                }
            }
            
            for (int track = 0; track < SEQ_TRACKS; track++) {
                int y = gridY + track * cellH;
                int trackLen = seq.trackLength[track];
                
                // Track label
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
                    
                    // Beat grouping: darker background every 4 steps
                    Color bgColor = (step / 4) % 2 == 0 ? (Color){40, 40, 40, 255} : (Color){30, 30, 30, 255};
                    
                    // Dim cells outside track length
                    if (!isInRange) {
                        bgColor = (Color){20, 20, 20, 255};
                    }
                    
                    // Cell color - velocity affects brightness, pitch shifts hue
                    Color cellColor = bgColor;
                    if (isActive) {
                        float vel = seq.velocity[track][step];
                        float pit = seq.pitch[track][step];
                        // Base green color scaled by velocity
                        unsigned char baseG = (unsigned char)(80 + vel * 100);
                        unsigned char baseR = (unsigned char)(30 + vel * 50);
                        unsigned char baseB = (unsigned char)(30 + vel * 50);
                        // Shift color based on pitch: negative = more blue, positive = more orange/red
                        if (pit < 0) {
                            baseB = (unsigned char)fminf(255, baseB + (-pit) * 80);
                            baseG = (unsigned char)(baseG * (1.0f + pit * 0.3f));
                        } else if (pit > 0) {
                            baseR = (unsigned char)fminf(255, baseR + pit * 100);
                            baseG = (unsigned char)(baseG * (1.0f - pit * 0.2f));
                        }
                        cellColor = (Color){baseR, baseG, baseB, 255};
                        if (isCurrent) {
                            // Brighten when playing
                            cellColor.r = (unsigned char)fminf(255, cellColor.r + 40);
                            cellColor.g = (unsigned char)fminf(255, cellColor.g + 75);
                            cellColor.b = (unsigned char)fminf(255, cellColor.b + 40);
                        }
                    } else if (isCurrent) {
                        cellColor = (Color){60, 60, 80, 255};  // Dim highlight for playhead
                    }
                    if (isHovered && isInRange && !isDragging) {
                        cellColor.r = (unsigned char)fminf(255, cellColor.r + 30);
                        cellColor.g = (unsigned char)fminf(255, cellColor.g + 30);
                        cellColor.b = (unsigned char)fminf(255, cellColor.b + 30);
                    }
                    if (isBeingDragged) {
                        // Highlight cell being dragged
                        cellColor.r = (unsigned char)fminf(255, cellColor.r + 50);
                        cellColor.g = (unsigned char)fminf(255, cellColor.g + 50);
                        cellColor.b = (unsigned char)fminf(255, cellColor.b + 50);
                    }
                    
                    DrawRectangleRec(cell, cellColor);
                    DrawRectangleLinesEx(cell, 1, isInRange ? (Color){60, 60, 60, 255} : (Color){35, 35, 35, 255});
                    
                    // Show pitch indicator (small triangle) on cells with pitch offset
                    if (hasPitchOffset) {
                        float pit = seq.pitch[track][step];
                        int triX = x + cellW - 8;
                        int triY = y + 3;
                        Color triColor = pit > 0 ? (Color){255, 150, 50, 255} : (Color){100, 150, 255, 255};
                        if (pit > 0) {
                            // Up triangle for higher pitch
                            DrawTriangle((Vector2){(float)triX + 3, (float)triY}, 
                                        (Vector2){(float)triX, (float)triY + 5}, 
                                        (Vector2){(float)triX + 6, (float)triY + 5}, triColor);
                        } else {
                            // Down triangle for lower pitch
                            DrawTriangle((Vector2){(float)triX, (float)triY}, 
                                        (Vector2){(float)triX + 6, (float)triY}, 
                                        (Vector2){(float)triX + 3, (float)triY + 5}, triColor);
                        }
                    }
                    
                    // Show value on active cells when hovered or dragging
                    if (isActive && (isHovered || isBeingDragged)) {
                        if (isBeingDragged && isDraggingPitch) {
                            // Show pitch as semitones
                            int semitones = (int)(seq.pitch[track][step] * 12);
                            DrawTextShadow(TextFormat("%+d", semitones), x + 2, y + 5, 10, WHITE);
                        } else {
                            // Show velocity percentage
                            int velPercent = (int)(seq.velocity[track][step] * 100);
                            DrawTextShadow(TextFormat("%d", velPercent), x + 3, y + 5, 10, WHITE);
                        }
                    }
                    
                    // Click handling (only within track length)
                    if (isHovered && isInRange && !isDragging) {
                        if (mouseClicked) {
                            if (isActive) {
                                // Start drag on active step
                                isDragging = true;
                                isDraggingPitch = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
                                dragTrack = track;
                                dragStep = step;
                                dragStartY = mouse.y;
                                dragStartVal = isDraggingPitch ? seq.pitch[track][step] : seq.velocity[track][step];
                                ui_consume_click();
                            } else {
                                // Toggle step on
                                seq.steps[track][step] = true;
                                ui_consume_click();
                                // Preview sound when toggling on
                                float pitchMod = powf(2.0f, seq.pitch[track][step]);
                                seq.triggersFull[track](seq.velocity[track][step], pitchMod);
                            }
                        }
                        if (rightClicked && isActive) {
                            // Right-click to toggle off
                            seq.steps[track][step] = false;
                            ui_consume_click();
                        }
                    }
                }
                
                // Length control (right of grid) - click to adjust
                int lenX = gridX + labelW + SEQ_MAX_STEPS * cellW + 5;
                Rectangle lenRect = {(float)lenX, (float)y, (float)lengthW - 2, (float)cellH - 2};
                bool lenHovered = CheckCollisionPointRec(mouse, lenRect);
                
                Color lenColor = lenHovered ? YELLOW : LIGHTGRAY;
                DrawRectangleRec(lenRect, (Color){50, 50, 50, 255});
                DrawRectangleLinesEx(lenRect, 1, (Color){80, 80, 80, 255});
                DrawTextShadow(TextFormat("%d", trackLen), lenX + 8, y + 4, 12, lenColor);
                
                // Click to increment, right-click to decrement
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
            
            // === DILLA TIMING CONTROLS (below grid) ===
            int dillaX = gridX + labelW;
            int dillaY = gridY + SEQ_TRACKS * cellH + 10;
            
            DrawTextShadow("Dilla Timing:", dillaX, dillaY, 12, YELLOW);
            
            // Per-instrument nudge/delay (in a row)
            DraggableInt(dillaX + 100, dillaY, "Kick", &seq.dilla.kickNudge, 0.3f, -12, 12);
            DraggableInt(dillaX + 200, dillaY, "Snare", &seq.dilla.snareDelay, 0.3f, -12, 12);
            DraggableInt(dillaX + 310, dillaY, "HiHat", &seq.dilla.hatNudge, 0.3f, -12, 12);
            DraggableInt(dillaX + 420, dillaY, "Clap", &seq.dilla.clapDelay, 0.3f, -12, 12);
            DraggableInt(dillaX + 520, dillaY, "Swing", &seq.dilla.swing, 0.3f, 0, 12);
            DraggableInt(dillaX + 630, dillaY, "Jitter", &seq.dilla.jitter, 0.3f, 0, 6);
            
            // Reset button
            if (PushButton(dillaX + 730, dillaY, "Reset")) {
                seq.dilla.kickNudge = -2;
                seq.dilla.snareDelay = 4;
                seq.dilla.hatNudge = 0;
                seq.dilla.clapDelay = 3;
                seq.dilla.swing = 6;
                seq.dilla.jitter = 2;
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
