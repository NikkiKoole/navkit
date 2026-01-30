// PixelSynth - 808-Style Drum Machine Engine
// Synthesized drums: kick, snare, clap, hihats, toms, rimshot, cowbell, clave, maracas

#ifndef PIXELSYNTH_DRUMS_H
#define PIXELSYNTH_DRUMS_H

#include <math.h>
#include <stdbool.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

// ============================================================================
// TYPES
// ============================================================================

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

// Drum voice state (one per drum type for dedicated processing)
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
    float hhPhases[6];    // Hihat oscillator phases (6 metallic squares)
    float velocity;       // Volume multiplier (0.0-1.0)
    float pitchMod;       // Pitch multiplier (0.5-2.0, 1.0 = normal)
} DrumVoice;

// Parameters for each drum sound (user-tweakable)
typedef struct {
    // Kick
    float kickPitch;      // Base pitch (30-80 Hz)
    float kickDecay;      // Decay time (0.1-1.0s)
    float kickPunchPitch; // Starting pitch for pitch envelope (80-200 Hz)
    float kickPunchDecay; // How fast pitch drops (0.01-0.1s)
    float kickClick;      // Initial click amount (0-1)
    float kickTone;       // Tone/distortion (0-1)
    
    // Snare
    float snarePitch;     // Tone pitch (100-300 Hz)
    float snareDecay;     // Overall decay (0.1-0.5s)
    float snareSnappy;    // Noise amount (0-1)
    float snareTone;      // Tone vs noise balance (0-1)
    
    // Clap
    float clapDecay;      // Decay time
    float clapTone;       // Filter brightness
    float clapSpread;     // Timing spread of "hands"
    
    // Hihat
    float hhDecayClosed;  // Closed hihat decay (0.02-0.15s)
    float hhDecayOpen;    // Open hihat decay (0.2-1.0s)
    float hhTone;         // Brightness/filter (0-1)
    
    // Tom
    float tomPitch;       // Base pitch multiplier
    float tomDecay;       // Decay time
    float tomPunchDecay;  // Pitch envelope time
    
    // Rimshot
    float rimPitch;       // Click pitch
    float rimDecay;       // Decay time
    
    // Cowbell
    float cowbellPitch;   // Base pitch
    float cowbellDecay;   // Decay time
    
    // Clave
    float clavePitch;     // Pitch
    float claveDecay;     // Decay (very short)
    
    // Maracas
    float maracasDecay;   // Decay time
    float maracasTone;    // Brightness
} DrumParams;

// ============================================================================
// STATE
// ============================================================================

#define NUM_DRUM_VOICES 12
static DrumVoice drumVoices[NUM_DRUM_VOICES];
static DrumParams drumParams;
static float drumVolume = 0.6f;

// ============================================================================
// HELPERS
// ============================================================================

// Drum-specific noise generator (fast, independent state per voice)
static float drumNoise(unsigned int *state) {
    *state = *state * 1103515245 + 12345;
    return (float)(*state >> 16) / 32768.0f - 1.0f;
}

// Exponential decay envelope
static float expDecay(float time, float decay) {
    if (decay <= 0.0f) return 0.0f;
    return expf(-time / (decay * 0.368f));  // 0.368 = 1/e
}

// ============================================================================
// INIT
// ============================================================================

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
// TRIGGER FUNCTIONS
// ============================================================================

// Trigger a drum with velocity and pitch
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
    
    // Initialize hihat oscillator phases
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

// Trigger with velocity only (normal pitch)
static void triggerDrumWithVel(DrumType type, float velocity) {
    triggerDrumFull(type, velocity, 1.0f);
}

// Trigger at full velocity and normal pitch
static void triggerDrum(DrumType type) {
    triggerDrumFull(type, 1.0f, 1.0f);
}

// ============================================================================
// INDIVIDUAL DRUM PROCESSORS
// ============================================================================

// Kick - sine with pitch envelope + optional click
static float processKick(DrumVoice *dv, float dt) {
    if (!dv->active) return 0.0f;
    
    DrumParams *p = &drumParams;
    dv->time += dt;
    
    float pitchT = expDecay(dv->time, p->kickPunchDecay);
    float freq = (p->kickPitch + (p->kickPunchPitch - p->kickPitch) * pitchT) * dv->pitchMod;
    
    dv->phase += freq * dt;
    if (dv->phase >= 1.0f) dv->phase -= 1.0f;
    
    float osc = sinf(dv->phase * 2.0f * PI);
    
    // Click transient
    float click = 0.0f;
    if (p->kickClick > 0.0f && dv->time < 0.01f) {
        unsigned int ns = (unsigned int)(dv->time * 1000000);
        click = drumNoise(&ns) * (1.0f - dv->time / 0.01f) * p->kickClick;
    }
    
    float sample = osc + click;
    if (p->kickTone > 0.0f) {
        sample = tanhf(sample * (1.0f + p->kickTone * 3.0f));
    }
    
    float amp = expDecay(dv->time, p->kickDecay);
    if (amp < 0.001f) dv->active = false;
    
    return sample * amp * 0.8f;
}

// Snare - tuned oscillators + filtered noise
static float processSnare(DrumVoice *dv, float dt) {
    if (!dv->active) return 0.0f;
    
    DrumParams *p = &drumParams;
    dv->time += dt;
    
    float freq1 = p->snarePitch * dv->pitchMod;
    float freq2 = p->snarePitch * 1.5f * dv->pitchMod;
    
    dv->phase += freq1 * dt;
    if (dv->phase >= 1.0f) dv->phase -= 1.0f;
    dv->phase2 += freq2 * dt;
    if (dv->phase2 >= 1.0f) dv->phase2 -= 1.0f;
    
    float tone = sinf(dv->phase * 2.0f * PI) * 0.6f + 
                 sinf(dv->phase2 * 2.0f * PI) * 0.3f;
    
    unsigned int ns = (unsigned int)(dv->time * 1000000 + dv->phase * 10000);
    float noiseSample = drumNoise(&ns);
    
    float cutoff = 0.15f + p->snareTone * 0.4f;
    dv->filterLp += cutoff * (noiseSample - dv->filterLp);
    dv->filterHp += 0.1f * (dv->filterLp - dv->filterHp);
    float filteredNoise = dv->filterLp - dv->filterHp;
    
    float mix = tone * (1.0f - p->snareSnappy * 0.7f) + 
                filteredNoise * p->snareSnappy * 1.5f;
    
    float toneAmp = expDecay(dv->time, p->snareDecay * 0.7f);
    float noiseAmp = expDecay(dv->time, p->snareDecay);
    float amp = toneAmp * (1.0f - p->snareSnappy * 0.5f) + noiseAmp * p->snareSnappy * 0.5f;
    
    if (amp < 0.001f) dv->active = false;
    
    return mix * amp * 0.7f;
}

// Clap - multiple noise bursts
static float processClap(DrumVoice *dv, float dt) {
    if (!dv->active) return 0.0f;
    
    DrumParams *p = &drumParams;
    dv->time += dt;
    
    float spread = p->clapSpread;
    float offsets[4] = {0.0f, spread, spread * 2.2f, spread * 3.5f};
    
    float sample = 0.0f;
    for (int i = 0; i < 4; i++) {
        float t = dv->time - offsets[i];
        if (t >= 0.0f) {
            unsigned int ns = (unsigned int)(t * 1000000 + i * 12345);
            float n = drumNoise(&ns);
            float burstEnv = expDecay(t, 0.02f);
            sample += n * burstEnv * 0.4f;
        }
    }
    
    float amp = expDecay(dv->time, p->clapDecay);
    
    float cutoff = 0.2f + p->clapTone * 0.3f;
    dv->filterLp += cutoff * (sample - dv->filterLp);
    dv->filterHp += 0.08f * (dv->filterLp - dv->filterHp);
    sample = (dv->filterLp - dv->filterHp) * 2.0f;
    
    if (amp < 0.001f) dv->active = false;
    
    return sample * amp * 0.6f;
}

// Hihat - 6 square wave oscillators at metallic ratios
static float processHihat(DrumVoice *dv, float dt, bool open) {
    if (!dv->active) return 0.0f;
    
    DrumParams *p = &drumParams;
    dv->time += dt;
    
    static const float hhFreqRatios[6] = {
        1.0f, 1.4471f, 1.6170f, 1.9265f, 2.5028f, 2.6637f
    };
    float baseFreq = (320.0f + p->hhTone * 200.0f) * dv->pitchMod;
    
    float sample = 0.0f;
    for (int i = 0; i < 6; i++) {
        float freq = baseFreq * hhFreqRatios[i];
        dv->hhPhases[i] += freq * dt;
        if (dv->hhPhases[i] >= 1.0f) dv->hhPhases[i] -= 1.0f;
        float sq = dv->hhPhases[i] < 0.5f ? 1.0f : -1.0f;
        sample += sq;
    }
    sample /= 6.0f;
    
    float hpCutoff = 0.3f + p->hhTone * 0.4f;
    dv->filterHp += hpCutoff * (sample - dv->filterHp);
    sample = sample - dv->filterHp;
    
    float decay = open ? p->hhDecayOpen : p->hhDecayClosed;
    float amp = expDecay(dv->time, decay);
    
    if (amp < 0.001f) dv->active = false;
    
    return sample * amp * 0.4f;
}

// Tom - similar to kick but higher pitch
static float processTom(DrumVoice *dv, float dt, float pitchMult) {
    if (!dv->active) return 0.0f;
    
    DrumParams *p = &drumParams;
    dv->time += dt;
    
    float basePitch = 80.0f * pitchMult * p->tomPitch;
    float punchPitch = basePitch * 2.0f;
    
    float pitchT = expDecay(dv->time, p->tomPunchDecay);
    float freq = basePitch + (punchPitch - basePitch) * pitchT;
    
    dv->phase += freq * dt;
    if (dv->phase >= 1.0f) dv->phase -= 1.0f;
    
    float osc = sinf(dv->phase * 2.0f * PI) * 0.8f +
                (4.0f * fabsf(dv->phase - 0.5f) - 1.0f) * 0.2f;
    
    float amp = expDecay(dv->time, p->tomDecay);
    if (amp < 0.001f) dv->active = false;
    
    return osc * amp * 0.6f;
}

// Rimshot - sharp click + high tone
static float processRimshot(DrumVoice *dv, float dt) {
    if (!dv->active) return 0.0f;
    
    DrumParams *p = &drumParams;
    dv->time += dt;
    
    dv->phase += p->rimPitch * dt;
    if (dv->phase >= 1.0f) dv->phase -= 1.0f;
    
    float osc = sinf(dv->phase * 2.0f * PI);
    
    unsigned int ns = (unsigned int)(dv->time * 1000000);
    float click = drumNoise(&ns) * expDecay(dv->time, 0.005f);
    
    float amp = expDecay(dv->time, p->rimDecay);
    if (amp < 0.001f) dv->active = false;
    
    return (osc * 0.5f + click * 0.5f) * amp * 0.5f;
}

// Cowbell - two square waves at non-harmonic intervals
static float processCowbell(DrumVoice *dv, float dt) {
    if (!dv->active) return 0.0f;
    
    DrumParams *p = &drumParams;
    dv->time += dt;
    
    float freq1 = p->cowbellPitch;
    float freq2 = p->cowbellPitch * 1.508f;
    
    dv->phase += freq1 * dt;
    if (dv->phase >= 1.0f) dv->phase -= 1.0f;
    dv->phase2 += freq2 * dt;
    if (dv->phase2 >= 1.0f) dv->phase2 -= 1.0f;
    
    float sq1 = dv->phase < 0.5f ? 1.0f : -1.0f;
    float sq2 = dv->phase2 < 0.5f ? 1.0f : -1.0f;
    
    float sample = (sq1 + sq2) * 0.5f;
    
    float cutoff = 0.15f;
    dv->filterLp += cutoff * (sample - dv->filterLp);
    sample = dv->filterLp;
    
    float amp = expDecay(dv->time, p->cowbellDecay);
    if (amp < 0.001f) dv->active = false;
    
    return sample * amp * 0.4f;
}

// Clave - very short filtered click
static float processClave(DrumVoice *dv, float dt) {
    if (!dv->active) return 0.0f;
    
    DrumParams *p = &drumParams;
    dv->time += dt;
    
    dv->phase += p->clavePitch * dt;
    if (dv->phase >= 1.0f) dv->phase -= 1.0f;
    
    float osc = sinf(dv->phase * 2.0f * PI);
    
    float amp = expDecay(dv->time, p->claveDecay);
    if (amp < 0.001f) dv->active = false;
    
    return osc * amp * 0.5f;
}

// Maracas - filtered noise burst
static float processMaracas(DrumVoice *dv, float dt) {
    if (!dv->active) return 0.0f;
    
    DrumParams *p = &drumParams;
    dv->time += dt;
    
    unsigned int ns = (unsigned int)(dv->time * 1000000);
    float sample = drumNoise(&ns);
    
    float cutoff = 0.3f + p->maracasTone * 0.4f;
    dv->filterHp += cutoff * (sample - dv->filterHp);
    sample = sample - dv->filterHp;
    
    float amp = expDecay(dv->time, p->maracasDecay);
    if (amp < 0.001f) dv->active = false;
    
    return sample * amp * 0.25f;
}

// ============================================================================
// MAIN PROCESSOR
// ============================================================================

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

// ============================================================================
// CONVENIENCE FUNCTIONS
// ============================================================================

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

// With velocity and pitch (for sequencer)
static void drumKickFull(float vel, float pitch) { triggerDrumFull(DRUM_KICK, vel, pitch); }
static void drumSnareFull(float vel, float pitch) { triggerDrumFull(DRUM_SNARE, vel, pitch); }
static void drumClosedHHFull(float vel, float pitch) { triggerDrumFull(DRUM_CLOSED_HH, vel, pitch); }
static void drumClapFull(float vel, float pitch) { triggerDrumFull(DRUM_CLAP, vel, pitch); }

#endif // PIXELSYNTH_DRUMS_H
