// patch_trigger.h — Generic SynthPatch-based note trigger
//
// Applies a SynthPatch to synth engine globals and dispatches to the right
// play function. Shared between the DAW (demo.c) and game bridge
// (sound_synth_bridge.c).
//
// Include after synth.h and synth_patch.h.

#ifndef PATCH_TRIGGER_H
#define PATCH_TRIGGER_H

#include <math.h>

// Apply all SynthPatch parameters to the synth engine globals.
// Must be called before any playNote/playPluck/etc. call.
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
    noteFilterType = p->p_filterType;
    noteFilterEnvAmt = p->p_filterEnvAmt;
    noteFilterEnvAttack = p->p_filterEnvAttack;
    noteFilterEnvDecay = p->p_filterEnvDecay;
    noteFilterKeyTrack = p->p_filterKeyTrack;
    noteFilterLfoRate = p->p_filterLfoRate;
    noteFilterLfoDepth = p->p_filterLfoDepth;
    noteFilterLfoShape = p->p_filterLfoShape;
    noteFilterLfoSync = (LfoSyncDiv)p->p_filterLfoSync;
    noteArpEnabled = p->p_arpEnabled;
    noteArpMode = (ArpMode)p->p_arpMode;
    noteArpRateDiv = (ArpRateDiv)p->p_arpRateDiv;
    noteArpRate = p->p_arpRate;
    noteArpChord = (ArpChordType)p->p_arpChord;
    noteUnisonCount = p->p_unisonCount;
    noteUnisonDetune = p->p_unisonDetune;
    noteUnisonMix = p->p_unisonMix;
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
    noteExpRelease = p->p_expRelease;
    notePitchEnvAmount = p->p_pitchEnvAmount;
    notePitchEnvDecay = p->p_pitchEnvDecay;
    notePitchEnvCurve = p->p_pitchEnvCurve;
    notePitchEnvLinear = p->p_pitchEnvLinear;
    noteNoiseMix = p->p_noiseMix;
    noteNoiseTone = p->p_noiseTone;
    noteNoiseHP = p->p_noiseHP;
    noteNoiseDecay = p->p_noiseDecay;
    noteRetriggerCount = p->p_retriggerCount;
    noteRetriggerSpread = p->p_retriggerSpread;
    noteRetriggerOverlap = p->p_retriggerOverlap;
    noteRetriggerBurstDecay = p->p_retriggerBurstDecay;
    noteOsc2Ratio = p->p_osc2Ratio;
    noteOsc2Level = p->p_osc2Level;
    noteOsc3Ratio = p->p_osc3Ratio;
    noteOsc3Level = p->p_osc3Level;
    noteOsc4Ratio = p->p_osc4Ratio;
    noteOsc4Level = p->p_osc4Level;
    noteOsc5Ratio = p->p_osc5Ratio;
    noteOsc5Level = p->p_osc5Level;
    noteOsc6Ratio = p->p_osc6Ratio;
    noteOsc6Level = p->p_osc6Level;
    noteDrive = p->p_drive;
    noteExpDecay = p->p_expDecay;
    noteClickLevel = p->p_clickLevel;
    noteClickTime = p->p_clickTime;
    noteNoiseMode = p->p_noiseMode;
    noteOscMixMode = p->p_oscMixMode;
    noteRetriggerCurve = p->p_retriggerCurve;
    notePhaseReset = p->p_phaseReset;
    noteNoiseLPBypass = p->p_noiseLPBypass;
    noteNoiseType = p->p_noiseType;
    noteOneShot = p->p_oneShot;
    noteAnalogRolloff = p->p_analogRolloff;
    noteTubeSaturation = p->p_tubeSaturation;
    noteRingMod = p->p_ringMod;
    noteRingModFreq = p->p_ringModFreq;
    noteWavefoldAmount = p->p_wavefoldAmount;
    noteHardSync = p->p_hardSync;
    noteHardSyncRatio = p->p_hardSyncRatio;
}

// Play a note using a SynthPatch. Sets all globals from the patch,
// then dispatches to the correct play function based on wave type.
// Returns the voice index (for later release).
static int playNoteWithPatch(float freq, SynthPatch *p) {
    // Use fixed trigger frequency if patch specifies one (drums/percussion)
    if (p->p_useTriggerFreq && p->p_triggerFreq > 0.0f) {
        freq = p->p_triggerFreq;
    }

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

// Convert MIDI note number to frequency (A4 = 440 Hz)
static float patchMidiToFreq(int note) {
    return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}

#endif // PATCH_TRIGGER_H
