// synth_patch.h — SynthPatch struct and default initializer
//
// Extracted from demo.c so song_file.h and tests can use it independently.
// Include after synth.h (needs WaveType, enum constants like PD_WAVE_SAW, etc.)

#ifndef SYNTH_PATCH_H
#define SYNTH_PATCH_H

#include <stdbool.h>

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
    int p_filterLfoSync;      // LfoSyncDiv (0=off, 1-7=tempo divisions)

    // Arpeggiator
    bool p_arpEnabled;
    int p_arpMode;            // ArpMode
    int p_arpRateDiv;         // ArpRateDiv
    float p_arpRate;          // Free rate in Hz
    int p_arpChord;           // ArpChordType

    // Unison
    int p_unisonCount;        // 1-4
    float p_unisonDetune;     // cents
    float p_unisonMix;        // 0-1

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

    // Identity & misc
    char p_name[32];
    bool p_expRelease;        // true = exponential (natural tail), false = linear (tight cutoff)
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
        .p_filterLfoSync = 0,
        .p_arpEnabled = false,
        .p_arpMode = 0,
        .p_arpRateDiv = 1,  // ARP_RATE_1_8
        .p_arpRate = 8.0f,
        .p_arpChord = 1,    // ARP_CHORD_MAJOR
        .p_unisonCount = 1,
        .p_unisonDetune = 10.0f,
        .p_unisonMix = 0.5f,
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
        .p_name = "",
        .p_expRelease = false,
    };
}

#endif // SYNTH_PATCH_H
