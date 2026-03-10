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
    int p_filterType;             // 0=LP, 1=HP, 2=BP
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

    // Pitch envelope (one-shot pitch sweep — kicks, toms, zaps, lasers)
    float p_pitchEnvAmount;   // Semitones to sweep (e.g., +24 for kick punch)
    float p_pitchEnvDecay;    // Decay time in seconds (e.g., 0.04 for kick)
    float p_pitchEnvCurve;    // Curve shape: -1=fast start, 0=linear, 1=slow start
    bool p_pitchEnvLinear;    // false=semitone (musical), true=linear Hz (analog drums)

    // Noise mix layer (blend filtered noise with oscillator — snares, hihats)
    float p_noiseMix;         // 0 = pure oscillator, 1 = pure noise
    float p_noiseTone;        // Noise LP filter cutoff (0 = dark, 1 = bright)
    float p_noiseHP;          // Noise HP filter cutoff (0 = off, 1 = maximum highpass)
    float p_noiseDecay;       // Separate noise decay (0 = follows main envelope)

    // Retrigger (claps, flams, rolls)
    int p_retriggerCount;     // 0 = off, 2-4 = rapid re-triggers
    float p_retriggerSpread;  // Timing spread between triggers in seconds
    bool p_retriggerOverlap;  // true = overlapping bursts (clap), false = envelope restart
    float p_retriggerBurstDecay; // Per-burst decay time (0 = default 0.02s)

    // Extra oscillators (metallic hihats, cowbell, fifth stacking)
    float p_osc2Ratio;        // Frequency ratio for 2nd oscillator (0 = off)
    float p_osc2Level;        // Mix level (0-1)
    float p_osc3Ratio;        // Frequency ratio for 3rd oscillator (0 = off)
    float p_osc3Level;        // Mix level (0-1)
    float p_osc4Ratio;        // Frequency ratio for 4th oscillator (0 = off)
    float p_osc4Level;        // Mix level (0-1)
    float p_osc5Ratio;        // Frequency ratio for 5th oscillator (0 = off)
    float p_osc5Level;        // Mix level (0-1)
    float p_osc6Ratio;        // Frequency ratio for 6th oscillator (0 = off)
    float p_osc6Level;        // Mix level (0-1)

    // Drive/saturation
    float p_drive;            // 0 = clean, tanh saturation (kick warmth, etc.)

    // Click transient (one-shot noise burst — kick click, key click, pluck attack)
    float p_clickLevel;       // 0 = off, strength of click noise burst
    float p_clickTime;        // Duration in seconds (linear fade, e.g. 0.005 for kick)

    // Algorithm modes (select different code paths for different sound types)
    int p_noiseMode;          // 0=mix with osc (default), 1=replace osc (pure noise→main filter), 2=per-burst noise (clap)
    int p_oscMixMode;         // 0=weighted average (default), 1=additive sum
    float p_retriggerCurve;   // 0=uniform spacing, >0=accelerating gaps (e.g. 0.3 for clap)
    bool p_phaseReset;        // true=force all osc phases to 0 on note-on
    bool p_noiseLPBypass;     // true=skip noise LP filter (raw noise to HP/main filter)
    int p_noiseType;          // 0=LFSR (running state), 1=time-hash (drums.h-style, deterministic)

    // Trigger frequency (for drums/percussive patches with baked-in pitch)
    bool p_useTriggerFreq;    // true = use p_triggerFreq instead of note freq
    float p_triggerFreq;      // fixed trigger frequency in Hz (e.g. 50 for kick)
    bool p_choke;             // true = new hit cuts off previous voice on same track (hihat)

    // Identity & misc
    char p_name[32];
    bool p_expRelease;        // true = exponential release (natural tail)
    bool p_expDecay;          // true = exponential decay (punchy drums)
    bool p_oneShot;           // true = skip sustain, auto-release after decay
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
        .p_pitchEnvAmount = 0.0f,
        .p_pitchEnvDecay = 0.05f,
        .p_pitchEnvCurve = 0.0f,
        .p_pitchEnvLinear = false,
        .p_noiseMix = 0.0f,
        .p_noiseTone = 0.5f,
        .p_noiseHP = 0.0f,
        .p_noiseDecay = 0.0f,
        .p_retriggerCount = 0,
        .p_retriggerSpread = 0.015f,
        .p_retriggerOverlap = false,
        .p_retriggerBurstDecay = 0.02f,
        .p_osc2Ratio = 0.0f,
        .p_osc2Level = 0.0f,
        .p_osc3Ratio = 0.0f,
        .p_osc3Level = 0.0f,
        .p_osc4Ratio = 0.0f,
        .p_osc4Level = 0.0f,
        .p_osc5Ratio = 0.0f,
        .p_osc5Level = 0.0f,
        .p_osc6Ratio = 0.0f,
        .p_osc6Level = 0.0f,
        .p_drive = 0.0f,
        .p_clickLevel = 0.0f,
        .p_clickTime = 0.005f,
        .p_noiseMode = 0,
        .p_oscMixMode = 0,
        .p_retriggerCurve = 0.0f,
        .p_phaseReset = false,
        .p_noiseLPBypass = false,
        .p_noiseType = 0,
        .p_useTriggerFreq = false,
        .p_triggerFreq = 440.0f,
        .p_choke = false,
        .p_name = "",
        .p_expRelease = false,
        .p_expDecay = false,
        .p_oneShot = false,
    };
}

#endif // SYNTH_PATCH_H
