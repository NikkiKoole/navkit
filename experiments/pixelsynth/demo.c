// PixelSynth Demo - Simple chiptune synth using raylib audio callbacks
// Press 1-6 for sound effects, Q-U for notes

#include "../../vendor/raylib.h"
#include "../../assets/fonts/comic_embedded.h"
#include <math.h>
#include <string.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 450
#define SAMPLE_RATE 44100
#define MAX_SAMPLES_PER_UPDATE 4096

// Simple oscillator types
typedef enum {
    WAVE_SQUARE,
    WAVE_SAW,
    WAVE_TRIANGLE,
    WAVE_NOISE,
} WaveType;

// Voice structure
typedef struct {
    float frequency;
    float phase;
    float volume;
    WaveType wave;
    
    // Simple ADSR envelope
    float attack;
    float decay;
    float sustain;
    float release;
    float envPhase;
    float envLevel;   // current envelope level (for smooth release)
    int envStage;     // 0=off, 1=attack, 2=decay, 3=sustain, 4=release
    
    // For pitch slides (SFX)
    float pitchSlide;
} Voice;

#define NUM_VOICES 8
static Voice voices[NUM_VOICES];
static float masterVolume = 0.5f;

// Performance tracking
static double audioTimeUs = 0.0;
static int audioFrameCount = 0;

// Simple noise generator
static unsigned int noiseState = 12345;
static float noise(void) {
    noiseState = noiseState * 1103515245 + 12345;
    return (float)(noiseState >> 16) / 32768.0f - 1.0f;
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
    
    // Apply pitch slide
    if (v->pitchSlide != 0.0f) {
        v->frequency += v->pitchSlide;
        if (v->frequency < 20.0f) v->frequency = 20.0f;
        if (v->frequency > 20000.0f) v->frequency = 20000.0f;
    }
    
    // Advance phase
    float phaseInc = v->frequency / sampleRate;
    v->phase += phaseInc;
    if (v->phase >= 1.0f) v->phase -= 1.0f;
    
    // Generate waveform
    float sample = 0.0f;
    switch (v->wave) {
        case WAVE_SQUARE:
            sample = v->phase < 0.5f ? 1.0f : -1.0f;
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
    }
    
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
static int playNote(float freq, WaveType wave) {
    int voiceIdx = findVoice();
    Voice *v = &voices[voiceIdx];
    
    v->frequency = freq;
    v->phase = 0.0f;
    v->volume = 0.5f;
    v->wave = wave;
    v->pitchSlide = 0.0f;
    
    v->attack = 0.01f;
    v->decay = 0.1f;
    v->sustain = 0.5f;
    v->release = 0.3f;
    v->envPhase = 0.0f;
    v->envLevel = 0.0f;
    v->envStage = 1;
    
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

// Sound effects - all use sustain=0 so they auto-release after decay
// Each has sfxr-style randomization for subtle variation

static void sfxJump(void) {
    int v = findVoice();
    voices[v] = (Voice){
        .frequency = mutate(150.0f, 0.15f),      // ±15% variation
        .phase = 0.0f,
        .volume = mutate(0.5f, 0.1f),
        .wave = WAVE_SQUARE,
        .attack = 0.01f,
        .decay = mutate(0.15f, 0.1f),
        .sustain = 0.0f,
        .release = 0.05f,
        .envPhase = 0.0f,
        .envStage = 1,
        .pitchSlide = mutate(10.0f, 0.2f),       // pitch slide varies most
    };
}

static void sfxCoin(void) {
    int v = findVoice();
    voices[v] = (Voice){
        .frequency = mutate(1200.0f, 0.08f),     // coins should be more consistent
        .phase = 0.0f,
        .volume = mutate(0.4f, 0.1f),
        .wave = WAVE_SQUARE,
        .attack = 0.005f,
        .decay = mutate(0.1f, 0.15f),
        .sustain = 0.0f,
        .release = 0.05f,
        .envPhase = 0.0f,
        .envStage = 1,
        .pitchSlide = mutate(20.0f, 0.15f),
    };
}

static void sfxHurt(void) {
    int v = findVoice();
    voices[v] = (Voice){
        .frequency = mutate(200.0f, 0.25f),      // hurt sounds can vary a lot
        .phase = 0.0f,
        .volume = mutate(0.5f, 0.1f),
        .wave = WAVE_NOISE,
        .attack = 0.01f,
        .decay = mutate(0.2f, 0.2f),
        .sustain = 0.0f,
        .release = mutate(0.1f, 0.2f),
        .envPhase = 0.0f,
        .envStage = 1,
        .pitchSlide = mutate(-3.0f, 0.3f),
    };
}

static void sfxExplosion(void) {
    int v = findVoice();
    voices[v] = (Voice){
        .frequency = mutate(80.0f, 0.3f),        // explosions vary widely
        .phase = 0.0f,
        .volume = mutate(0.6f, 0.1f),
        .wave = WAVE_NOISE,
        .attack = 0.01f,
        .decay = mutate(0.5f, 0.25f),
        .sustain = 0.0f,
        .release = mutate(0.3f, 0.2f),
        .envPhase = 0.0f,
        .envStage = 1,
        .pitchSlide = mutate(-1.0f, 0.4f),
    };
}

static void sfxPowerup(void) {
    int v = findVoice();
    voices[v] = (Voice){
        .frequency = mutate(300.0f, 0.12f),
        .phase = 0.0f,
        .volume = mutate(0.4f, 0.1f),
        .wave = WAVE_TRIANGLE,
        .attack = 0.01f,
        .decay = mutate(0.3f, 0.15f),
        .sustain = 0.0f,
        .release = mutate(0.2f, 0.1f),
        .envPhase = 0.0f,
        .envStage = 1,
        .pitchSlide = mutate(8.0f, 0.2f),
    };
}

static void sfxBlip(void) {
    int v = findVoice();
    voices[v] = (Voice){
        .frequency = mutate(800.0f, 0.1f),       // blips stay fairly consistent
        .phase = 0.0f,
        .volume = mutate(0.3f, 0.1f),
        .wave = WAVE_SQUARE,
        .attack = 0.005f,
        .decay = mutate(0.05f, 0.15f),
        .sustain = 0.0f,
        .release = 0.02f,
        .envPhase = 0.0f,
        .envStage = 1,
        .pitchSlide = rndRange(-2.0f, 2.0f),     // slight random pitch movement
    };
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

int main(void) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "PixelSynth Demo");
    
    Font font = LoadEmbeddedFont();
    
    SetAudioStreamBufferSizeDefault(MAX_SAMPLES_PER_UPDATE);
    InitAudioDevice();
    
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
    const WaveType waves[] = {WAVE_SQUARE, WAVE_SQUARE, WAVE_SQUARE, WAVE_SQUARE, WAVE_SQUARE, WAVE_SQUARE, WAVE_SQUARE,
                              WAVE_SAW, WAVE_SAW, WAVE_SAW, WAVE_SAW, WAVE_SAW, WAVE_SAW, WAVE_SAW};
    
    while (!WindowShouldClose()) {
        // Toggle randomization with R
        if (IsKeyPressed(KEY_R)) sfxRandomize = !sfxRandomize;
        
        // Sound effects (1-6)
        if (IsKeyPressed(KEY_ONE))   sfxJump();
        if (IsKeyPressed(KEY_TWO))   sfxCoin();
        if (IsKeyPressed(KEY_THREE)) sfxHurt();
        if (IsKeyPressed(KEY_FOUR))  sfxExplosion();
        if (IsKeyPressed(KEY_FIVE))  sfxPowerup();
        if (IsKeyPressed(KEY_SIX))   sfxBlip();
        
        // Polyphonic notes - press to play, release to stop
        for (int i = 0; i < 14; i++) {
            if (IsKeyPressed(keys[i])) {
                keyVoices[i] = playNote(freqs[i], waves[i]);
            }
            if (IsKeyReleased(keys[i]) && keyVoices[i] >= 0) {
                releaseNote(keyVoices[i]);
                keyVoices[i] = -1;
            }
        }
        
        BeginDrawing();
        ClearBackground(DARKGRAY);
        
        DrawTextEx(font, "PixelSynth Demo", (Vector2){20, 20}, 30, 1, WHITE);
        DrawTextEx(font, "Polyphonic chiptune synthesizer", (Vector2){20, 55}, 16, 1, LIGHTGRAY);
        
        DrawTextEx(font, "Sound Effects:", (Vector2){20, 100}, 20, 1, YELLOW);
        DrawTextEx(font, "1 - Jump    2 - Coin    3 - Hurt", (Vector2){20, 125}, 16, 1, WHITE);
        DrawTextEx(font, "4 - Explosion    5 - Powerup    6 - Blip", (Vector2){20, 145}, 16, 1, WHITE);
        DrawTextEx(font, TextFormat("R - Toggle randomize: %s", sfxRandomize ? "ON" : "OFF"), (Vector2){20, 165}, 14, 1, sfxRandomize ? GREEN : GRAY);
        
        DrawTextEx(font, "Notes (Square wave) - hold keys:", (Vector2){20, 200}, 20, 1, YELLOW);
        DrawTextEx(font, "Q W E R T Y U  =  C D E F G A B", (Vector2){20, 225}, 16, 1, WHITE);
        
        DrawTextEx(font, "Notes (Saw wave, octave lower):", (Vector2){20, 260}, 20, 1, YELLOW);
        DrawTextEx(font, "A S D F G H J  =  C D E F G A B", (Vector2){20, 285}, 16, 1, WHITE);
        
        // Show active voices
        DrawTextEx(font, "Voices:", (Vector2){20, 340}, 16, 1, GRAY);
        for (int i = 0; i < NUM_VOICES; i++) {
            Color c = DARKGRAY;
            if (voices[i].envStage == 4) c = ORANGE;       // releasing
            else if (voices[i].envStage > 0) c = GREEN;   // active
            DrawRectangle(90 + i * 25, 340, 20, 16, c);
        }
        
        // Performance stats
        double bufferTimeMs = (double)audioFrameCount / SAMPLE_RATE * 1000.0;
        double cpuPercent = (audioTimeUs / 1000.0) / bufferTimeMs * 100.0;
        DrawTextEx(font, TextFormat("Audio: %.1f us (%.1f%% of %.1fms buffer)", 
                 audioTimeUs, cpuPercent, bufferTimeMs), (Vector2){20, 370}, 14, 1, LIGHTGRAY);
        DrawTextEx(font, TextFormat("FPS: %d", GetFPS()), (Vector2){20, 390}, 14, 1, LIGHTGRAY);
        
        DrawTextEx(font, "ESC to exit", (Vector2){20, SCREEN_HEIGHT - 30}, 14, 1, GRAY);
        
        EndDrawing();
    }
    
    UnloadAudioStream(stream);
    CloseAudioDevice();
    UnloadFont(font);
    CloseWindow();
    
    return 0;
}
