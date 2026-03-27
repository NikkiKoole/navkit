// synth_patch.h — SynthPatch struct and default initializer
//
// Extracted from demo.c so song_file.h and tests can use it independently.
// Include after synth.h (needs WaveType, enum constants like PD_WAVE_SAW, etc.)

#ifndef SYNTH_PATCH_H
#define SYNTH_PATCH_H

#include <stdbool.h>

typedef struct SynthPatch {
    // Wave type
    int p_waveType;
    int p_scwIndex;

    // Envelope
    bool p_envelopeEnabled;   // true = ADSR shapes amplitude, false = bypass (sustain=1.0)
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
    bool p_filterEnabled;         // true = filter active, false = bypass (cutoff=1.0, reso=0)
    float p_filterCutoff;
    float p_filterResonance;
    int p_filterType;             // 0=LP, 1=HP, 2=BP
    int p_filterModel;            // 0=SVF (default), 1=Ladder (4-pole TPT)
    float p_filterEnvAmt;
    float p_filterEnvAttack;
    float p_filterEnvDecay;
    float p_filterKeyTrack;       // 0 = fixed cutoff, 1 = cutoff tracks pitch fully

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
    int p_resoLfoSync;        // LfoSyncDiv (0=off, 1-7=tempo divisions)

    // Amplitude LFO
    float p_ampLfoRate;
    float p_ampLfoDepth;
    int p_ampLfoShape;
    int p_ampLfoSync;         // LfoSyncDiv (0=off, 1-7=tempo divisions)

    // Pitch LFO
    float p_pitchLfoRate;
    float p_pitchLfoDepth;
    int p_pitchLfoShape;
    int p_pitchLfoSync;       // LfoSyncDiv (0=off, 1-7=tempo divisions)

    // LFO phase offsets (0.0-1.0, applied as initial phase on voice start)
    float p_filterLfoPhaseOffset;
    float p_resoLfoPhaseOffset;
    float p_ampLfoPhaseOffset;
    float p_pitchLfoPhaseOffset;

    // Mono/Glide
    bool p_monoMode;
    bool p_monoRetrigger;   // true = retrigger envelope on every note, false = legato (glide only)
    bool p_monoHardRetrigger; // true = envelope restarts from zero (punchy arp), false = from current level (smooth)
    float p_glideTime;
    float p_legatoWindow;   // seconds: note-on within this time after release → legato (0=off)
    int p_notePriority;     // NotePriority: 0=last, 1=low, 2=high

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
    int p_granularSamplerSlot;  // -1 = use SCW table, 0+ = use sampler bank slot
    float p_granularGrainSize;
    float p_granularDensity;
    float p_granularPosition;
    float p_granularPosRandom;
    float p_granularPitch;
    float p_granularPitchRandom;
    float p_granularAmpRandom;
    float p_granularSpread;
    bool p_granularFreeze;

    // FM LFO (modulates fmModIndex)
    float p_fmLfoRate;
    float p_fmLfoDepth;
    int p_fmLfoShape;
    int p_fmLfoSync;
    float p_fmLfoPhaseOffset;

    // FM
    float p_fmModRatio;
    float p_fmModIndex;
    float p_fmFeedback;
    float p_fmMod2Ratio;      // 3rd operator ratio (0=off)
    float p_fmMod2Index;      // 3rd operator depth
    int p_fmAlgorithm;        // FMAlgorithm (0=stack, 1=parallel, 2=branch, 3=pair)

    // Phase Distortion
    int p_pdWaveType;
    float p_pdDistortion;

    // Membrane
    int p_membranePreset;
    float p_membraneDamping;
    float p_membraneStrike;
    float p_membraneBend;
    float p_membraneBendDecay;

    // Metallic percussion
    int p_metallicPreset;
    float p_metallicRingMix;
    float p_metallicNoiseLevel;
    float p_metallicBrightness;
    float p_metallicPitchEnv;
    float p_metallicPitchEnvDecay;

    // Guitar body
    int p_guitarPreset;
    float p_guitarBodyMix;
    float p_guitarBodyBrightness;
    float p_guitarPickPosition;
    float p_guitarBuzz;

    // Mandolin (paired course strings)
    int p_mandolinPreset;
    float p_mandolinBodyMix;
    float p_mandolinCourseDetune;
    float p_mandolinPickPosition;

    // Whistle (pea physics + sine)
    int p_whistlePreset;
    float p_whistleBreath;
    float p_whistleNoiseGain;
    float p_whistleFippleFreqMod;

    // Stiff string (piano/harpsichord/dulcimer)
    int   p_stifkarpPreset;
    float p_stifkarpHardness;
    float p_stifkarpStiffness;
    float p_stifkarpStrikePos;
    float p_stifkarpBodyMix;
    float p_stifkarpBodyBrightness;
    float p_stifkarpDamper;
    float p_stifkarpSympathetic;
    float p_stifkarpDetune;        // Double-string detune in cents (0=off, >0=enables double string)

    // Shaker (particle collision)
    int   p_shakerPreset;
    float p_shakerParticles;    // 0-1: particle density (8-64 mapped)
    float p_shakerDecayRate;    // 0-1: system energy decay speed
    float p_shakerResonance;    // 0-1: resonator Q factor
    float p_shakerBrightness;   // 0-1: resonator freq shift
    float p_shakerScrape;       // 0-1: random vs periodic collision

    // BandedWG (banded waveguide)
    int   p_bandedwgPreset;
    float p_bandedwgBowPressure;   // 0-1: bow force / strike intensity
    float p_bandedwgBowSpeed;      // 0-1: bow velocity
    float p_bandedwgStrikePos;     // 0-1: excitation position
    float p_bandedwgBrightness;    // 0-1: material brightness
    float p_bandedwgSustain;       // 0-1: mode decay/ring time

    // VoicForm (4-formant voice)
    int   p_vfPhoneme;            // Current phoneme (VFPhoneme enum, 0-31)
    int   p_vfPhonemeTarget;      // Target phoneme for morph (-1 = same)
    float p_vfMorphRate;          // Morph speed (0.5-50 Hz)
    float p_vfAspiration;         // Breathiness (0-1)
    float p_vfOpenQuotient;       // Glottal open quotient (0.3-0.7)
    float p_vfSpectralTilt;       // Brightness (0=bright, 1=dark)
    float p_vfFormantShift;       // Vocal tract length (0.5-2.0)
    float p_vfVibratoDepth;       // Voice vibrato semitones (0-1)
    float p_vfVibratoRate;        // Voice vibrato Hz (3-8)
    int   p_vfConsonant;          // Consonant burst phoneme (-1=none)

    // Bowed string
    float p_bowPressure;
    float p_bowSpeed;
    float p_bowPosition;

    // Blown pipe
    float p_pipeBreath;
    float p_pipeEmbouchure;
    float p_pipeBore;
    float p_pipeOverblow;

    // Reed instrument (clarinet/sax/oboe)
    float p_reedStiffness;
    float p_reedAperture;
    float p_reedBlowPressure;
    float p_reedBore;
    float p_reedVibratoDepth;

    // Brass instrument (trumpet/trombone/horn)
    float p_brassBlowPressure;
    float p_brassLipTension;
    float p_brassLipAperture;
    float p_brassBore;
    float p_brassMute;

    // Electric piano (Rhodes)
    float p_epHardness;       // Hammer hardness (0=soft, 1=hard)
    float p_epToneBar;        // Tone bar coupling (fundamental sustain, 0-1)
    float p_epPickupPos;      // Pickup position (0=mellow, 1=bright)
    float p_epPickupDist;     // Pickup distance (0=clean, 1=bark)
    float p_epDecay;          // Decay time in seconds (0.5-8.0)
    float p_epBell;           // Upper mode (bell) emphasis (0-1)
    float p_epBellTone;       // Bell inharmonicity (0=harmonic/organ, 1=inharmonic/bell)
    int p_epRatioSet;         // 0=beam (Euler-Bernoulli), 1=tine+spring (measured)
    int p_epPickupType;       // 0=electromagnetic (Rhodes), 1=electrostatic (Wurlitzer)

    // Organ (Hammond drawbar)
    float p_orgDrawbar[ORGAN_DRAWBARS]; // 9 drawbar levels 0-1 (maps from 0-8)
    float p_orgClick;                    // Key click amount 0-1
    int   p_orgPercOn;                   // Percussion on/off
    int   p_orgPercHarmonic;             // 0=2nd, 1=3rd harmonic
    int   p_orgPercSoft;                 // 0=normal, 1=soft (-3dB)
    int   p_orgPercFast;                 // 0=slow (~500ms), 1=fast (~200ms)
    float p_orgCrosstalk;                // Tonewheel leakage 0-1
    int   p_orgVibratoMode;              // Scanner V/C: 0=off, 1-3=V1-V3, 4-6=C1-C3

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
    int p_retriggerCount;     // 0 = off, 1-15 = rapid re-triggers (up to 16 total bursts)
    float p_retriggerSpread;  // Timing spread between triggers in seconds
    bool p_retriggerOverlap;  // true = overlapping bursts (clap), false = envelope restart
    float p_retriggerBurstDecay; // Per-burst decay time (0 = default 0.02s)

    // Extra oscillators (metallic hihats, cowbell, fifth stacking)
    float p_osc2Ratio;        // Frequency ratio for 2nd oscillator (0 = off)
    float p_osc2Level;        // Mix level (0-1)
    float p_osc2Decay;        // Per-osc decay rate (0=sustain, >0=exp decay speed)
    float p_osc3Ratio;        // Frequency ratio for 3rd oscillator (0 = off)
    float p_osc3Level;        // Mix level (0-1)
    float p_osc3Decay;
    float p_osc4Ratio;        // Frequency ratio for 4th oscillator (0 = off)
    float p_osc4Level;        // Mix level (0-1)
    float p_osc4Decay;
    float p_osc5Ratio;        // Frequency ratio for 5th oscillator (0 = off)
    float p_osc5Level;        // Mix level (0-1)
    float p_osc5Decay;
    float p_osc6Ratio;        // Frequency ratio for 6th oscillator (0 = off)
    float p_osc6Level;        // Mix level (0-1)
    float p_osc6Decay;
    float p_oscVelSens;       // Extra osc velocity sensitivity (0=off, 1=full: level scales with velocity)

    // Velocity modulation targets
    float p_velToFilter;      // Velocity → filter cutoff offset (0=off, 1=full range)
    float p_velToClick;       // Velocity → click level scaling (0=off, 1=full)
    float p_velToDrive;       // Velocity → drive amount (0=off, 1=full: hard hits saturate)

    // Drive/saturation
    float p_drive;            // 0 = clean, saturation amount (kick warmth, etc.)
    int p_driveMode;          // DistortionMode enum (0=soft, 1=hard, 2=fold, etc.)

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

    // Analog warmth toggles
    bool p_analogRolloff;     // true = gentle 1-pole LP rolloff (removes digital harshness)
    bool p_tubeSaturation;    // true = even-harmonic tube-style warmth
    bool p_analogVariance;    // true = per-voice component tolerances (pitch/cutoff/env/gain)

    // Synthesis modes (ring mod, wavefolding, hard sync)
    bool p_ringMod;           // true = multiply osc output by ring mod oscillator
    float p_ringModFreq;      // Ring mod frequency ratio (e.g. 2.0 = octave above)
    float p_wavefoldAmount;   // 0 = off, >0 = wavefold gain (West Coast synthesis)
    bool p_hardSync;          // true = master osc resets slave phase
    float p_hardSyncRatio;    // Master/slave frequency ratio (e.g. 1.5)

    // Per-voice formant filter (vowel shaping — works with any oscillator)
    bool p_formantEnabled;    // Enable formant filter
    int p_formantFrom;        // Starting phoneme (VFPhoneme enum, 0-31)
    int p_formantTo;          // Target phoneme for morph
    float p_formantMorphTime; // Seconds to morph from → to (0.01-2.0)
    float p_formantShift;     // Scale all formant freqs (0.5=monster, 2.0=chipmunk)
    float p_formantQ;         // Bandwidth multiplier (0.5=wide, 3.0=sharp/nasal)
    float p_formantMix;       // Dry/wet blend (0=dry, 1=full formant)
    bool p_formantRandom;     // Randomize from/to phoneme on each note trigger

    // Filterbank mode (Grenadier RA-99 style: LP + 2×BP, free-form frequencies)
    int p_formantMode;            // 0 = phoneme (default), 1 = filterbank
    float p_fbBaseFreq;           // Filterbank base frequency in Hz (50-5000)
    float p_fbSpacing;            // Frequency spacing multiplier between filters (1.0-8.0)
    float p_fbAlpha;              // Alpha axis: sweeps all filter freqs (0.0-1.0)
    float p_fbBeta;               // Beta axis: spreads/narrows filter spacing (0.0-1.0)
    float p_fbQ;                  // Filterbank resonance (0.5-20.0), LP=Q×0.7, BP2=Q, BP3=Q×1.3
    float p_fbKeyTrack;           // 0 = fixed base freq, 1 = tracks note pitch
    float p_fbMorphOsc;           // Trapezoid morph: 0 = triangle, 1 = square
    float p_fbRandomize;          // Per-note random variation amount (0=none, 1=wild)
    float p_fbEnvAlpha;           // Filter envelope → Alpha depth (-1..+1, bipolar)
    float p_fbLfoRate;            // Filterbank LFO rate in Hz (0..20)
    int p_fbLfoSync;              // Tempo sync (LfoSyncDiv, 0=off, overrides rate)
    float p_fbLfoAlpha;           // Filterbank LFO → Alpha depth (-1..+1)
    int p_fbLfoShape;             // Filterbank LFO shape (0=sin,1=tri,2=sq,3=saw,4=S&H)
    float p_fbNoiseMix;           // Noise mixed into filterbank input (0..1)

    // 303 acid mode
    bool p_acidMode;          // true = authentic TB-303 slide/accent behavior
    float p_accentSweepAmt;   // Accent sweep circuit intensity (0-1, default 0.5)
    float p_gimmickDipAmt;    // Gimmick circuit filter dip at decay end (0-1, default 0.3)

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
        .p_envelopeEnabled = true,
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
        .p_filterEnabled = true,
        .p_filterCutoff = 1.0f,
        .p_filterResonance = 0.0f,
        .p_filterModel = 0,           // FILTER_MODEL_SVF
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
        .p_resoLfoSync = 0,
        .p_ampLfoRate = 0.0f,
        .p_ampLfoDepth = 0.0f,
        .p_ampLfoShape = 0,
        .p_ampLfoSync = 0,
        .p_pitchLfoRate = 5.0f,
        .p_pitchLfoDepth = 0.0f,
        .p_pitchLfoShape = 0,
        .p_pitchLfoSync = 0,
        .p_monoMode = false,
        .p_monoRetrigger = false,
        .p_monoHardRetrigger = false,
        .p_glideTime = 0.1f,
        .p_legatoWindow = 0.015f,  // 15ms — forgives sloppy finger transitions in mono mode
        .p_notePriority = 0,      // NOTE_PRIORITY_LAST
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
        .p_granularSamplerSlot = -1,
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
        .p_fmModIndex = 2.0f,
        .p_fmFeedback = 0.0f,
        .p_fmMod2Ratio = 0.0f,
        .p_fmMod2Index = 0.0f,
        .p_fmAlgorithm = 0,
        .p_pdWaveType = PD_WAVE_SAW,
        .p_pdDistortion = 0.5f,
        .p_membranePreset = MEMBRANE_TABLA,
        .p_membraneDamping = 0.3f,
        .p_membraneStrike = 0.3f,
        .p_membraneBend = 0.15f,
        .p_membraneBendDecay = 0.08f,
        .p_metallicPreset = METALLIC_808_CH,
        .p_metallicRingMix = 0.85f,
        .p_metallicNoiseLevel = 0.15f,
        .p_metallicBrightness = 1.0f,
        .p_metallicPitchEnv = 0.0f,
        .p_metallicPitchEnvDecay = 0.01f,
        .p_guitarPreset = GUITAR_ACOUSTIC,
        .p_guitarBodyMix = 0.6f,
        .p_guitarBodyBrightness = 0.5f,
        .p_guitarPickPosition = 0.3f,
        .p_guitarBuzz = 0.0f,
        .p_mandolinPreset = MANDOLIN_NEAPOLITAN,
        .p_mandolinBodyMix = 0.55f,
        .p_mandolinCourseDetune = 2.0f,
        .p_mandolinPickPosition = 0.2f,
        .p_whistlePreset = WHISTLE_REFEREE,
        .p_whistleBreath = 0.8f,
        .p_whistleNoiseGain = 0.15f,
        .p_whistleFippleFreqMod = 0.5f,
        .p_stifkarpPreset = STIFKARP_PIANO,
        .p_stifkarpHardness = 0.45f,
        .p_stifkarpStiffness = 0.25f,
        .p_stifkarpStrikePos = 0.12f,
        .p_stifkarpBodyMix = 0.45f,
        .p_stifkarpBodyBrightness = 0.5f,
        .p_stifkarpDamper = 0.9f,
        .p_stifkarpSympathetic = 0.15f,
        .p_stifkarpDetune = 0.0f,
        .p_shakerPreset = SHAKER_MARACA,
        .p_shakerParticles = 0.22f,
        .p_shakerDecayRate = 0.5f,
        .p_shakerResonance = 0.5f,
        .p_shakerBrightness = 0.5f,
        .p_shakerScrape = 0.0f,
        .p_bandedwgPreset = BANDEDWG_GLASS,
        .p_bandedwgBowPressure = 0.5f,
        .p_bandedwgBowSpeed = 0.5f,
        .p_bandedwgStrikePos = 0.5f,
        .p_bandedwgBrightness = 0.6f,
        .p_bandedwgSustain = 0.7f,
        .p_vfPhoneme = 0,
        .p_vfPhonemeTarget = -1,
        .p_vfMorphRate = 5.0f,
        .p_vfAspiration = 0.1f,
        .p_vfOpenQuotient = 0.5f,
        .p_vfSpectralTilt = 0.3f,
        .p_vfFormantShift = 1.0f,
        .p_vfVibratoDepth = 0.15f,
        .p_vfVibratoRate = 5.5f,
        .p_vfConsonant = -1,
        .p_bowPressure = 0.5f,
        .p_bowSpeed = 0.5f,
        .p_bowPosition = 0.13f,
        .p_pipeBreath = 0.5f,
        .p_pipeEmbouchure = 0.5f,
        .p_pipeBore = 0.5f,
        .p_pipeOverblow = 0.0f,
        .p_reedStiffness = 0.5f,
        .p_reedAperture = 0.6f,
        .p_reedBlowPressure = 0.5f,
        .p_reedBore = 0.0f,
        .p_reedVibratoDepth = 0.0f,
        .p_brassBlowPressure = 0.5f,
        .p_brassLipTension = 0.5f,
        .p_brassLipAperture = 0.5f,
        .p_brassBore = 0.0f,
        .p_brassMute = 0.0f,
        .p_epHardness = 0.4f,
        .p_epToneBar = 0.5f,
        .p_epPickupPos = 0.3f,
        .p_epPickupDist = 0.4f,
        .p_epDecay = 3.0f,
        .p_epBell = 0.5f,
        .p_epBellTone = 0.5f,
        .p_epRatioSet = 0,
        .p_epPickupType = 0,
        .p_orgDrawbar = {0,0,1,0,0,0,0,0,0},
        .p_orgClick = 0.3f,
        .p_orgPercOn = 0,
        .p_orgPercHarmonic = 0,
        .p_orgPercSoft = 0,
        .p_orgPercFast = 1,
        .p_orgCrosstalk = 0.0f,
        .p_orgVibratoMode = 0,
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
        .p_osc2Decay = 0.0f,
        .p_osc3Ratio = 0.0f,
        .p_osc3Level = 0.0f,
        .p_osc3Decay = 0.0f,
        .p_osc4Ratio = 0.0f,
        .p_osc4Level = 0.0f,
        .p_osc4Decay = 0.0f,
        .p_osc5Ratio = 0.0f,
        .p_osc5Level = 0.0f,
        .p_osc5Decay = 0.0f,
        .p_osc6Ratio = 0.0f,
        .p_osc6Level = 0.0f,
        .p_osc6Decay = 0.0f,
        .p_oscVelSens = 0.0f,
        .p_velToFilter = 0.0f,
        .p_velToClick = 0.0f,
        .p_velToDrive = 0.0f,
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
        .p_analogRolloff = false,
        .p_tubeSaturation = false,
        .p_analogVariance = false,
        .p_ringMod = false,
        .p_ringModFreq = 2.0f,
        .p_wavefoldAmount = 0.0f,
        .p_hardSync = false,
        .p_hardSyncRatio = 1.5f,
        .p_formantEnabled = false,
        .p_formantFrom = VF_A,
        .p_formantTo = VF_O,
        .p_formantMorphTime = 0.15f,
        .p_formantShift = 1.0f,
        .p_formantQ = 1.0f,
        .p_formantMix = 0.8f,
        .p_formantRandom = false,
        .p_formantMode = 0,
        .p_fbBaseFreq = 400.0f,
        .p_fbSpacing = 2.5f,
        .p_fbAlpha = 0.5f,
        .p_fbBeta = 0.5f,
        .p_fbQ = 4.0f,
        .p_fbKeyTrack = 0.0f,
        .p_fbMorphOsc = 0.0f,
        .p_fbRandomize = 0.0f,
        .p_fbEnvAlpha = 0.0f,
        .p_fbLfoRate = 0.0f,
        .p_fbLfoSync = 0,
        .p_fbLfoAlpha = 0.0f,
        .p_fbLfoShape = 0,
        .p_fbNoiseMix = 0.0f,
        .p_acidMode = false,
        .p_accentSweepAmt = 0.5f,
        .p_gimmickDipAmt = 0.3f,
        .p_name = "",
        .p_expRelease = false,
        .p_expDecay = false,
        .p_oneShot = false,
    };
}

#endif // SYNTH_PATCH_H
