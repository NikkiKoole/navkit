// instrument_presets.h — Shared instrument preset definitions
//
// Extracted from demo.c so multiple frontends (demo, daw) can share presets.
// Include after synth_patch.h.

#ifndef INSTRUMENT_PRESETS_H
#define INSTRUMENT_PRESETS_H

#include "synth_patch.h"

typedef struct {
    const char *name;
    SynthPatch patch;
} InstrumentPreset;

#define NUM_INSTRUMENT_PRESETS 240
static InstrumentPreset instrumentPresets[NUM_INSTRUMENT_PRESETS];

static void initInstrumentPresets(void) {
    // Start all presets from default
    for (int i = 0; i < NUM_INSTRUMENT_PRESETS; i++) {
        instrumentPresets[i].patch = createDefaultPatch(WAVE_SAW);
    }

    // Chip Lead - Classic square wave lead
    instrumentPresets[0].name = "Chip Lead";
    instrumentPresets[0].patch.p_waveType = WAVE_SQUARE;
    instrumentPresets[0].patch.p_attack = 0.005f;
    instrumentPresets[0].patch.p_decay = 0.1f;
    instrumentPresets[0].patch.p_sustain = 0.7f;
    instrumentPresets[0].patch.p_release = 0.15f;
    instrumentPresets[0].patch.p_pwmRate = 2.0f;
    instrumentPresets[0].patch.p_pwmDepth = 0.1f;
    instrumentPresets[0].patch.p_filterCutoff = 0.8f;

    // Fat Bass - Thick detuned saw bass
    instrumentPresets[1].name = "Fat Bass";
    instrumentPresets[1].patch.p_waveType = WAVE_SAW;
    instrumentPresets[1].patch.p_attack = 0.01f;
    instrumentPresets[1].patch.p_decay = 0.15f;
    instrumentPresets[1].patch.p_sustain = 0.6f;
    instrumentPresets[1].patch.p_release = 0.2f;
    instrumentPresets[1].patch.p_filterCutoff = 0.4f;
    instrumentPresets[1].patch.p_filterResonance = 0.2f;
    instrumentPresets[1].patch.p_filterEnvAmt = 0.3f;
    instrumentPresets[1].patch.p_filterEnvDecay = 0.15f;
    instrumentPresets[1].patch.p_unisonCount = 3;
    instrumentPresets[1].patch.p_unisonDetune = 15.0f;
    instrumentPresets[1].patch.p_unisonMix = 0.6f;

    // Arp Pad - Arpeggiated minor chord
    instrumentPresets[2].name = "Arp Pad";
    instrumentPresets[2].patch.p_waveType = WAVE_SAW;
    instrumentPresets[2].patch.p_attack = 0.02f;
    instrumentPresets[2].patch.p_decay = 0.2f;
    instrumentPresets[2].patch.p_sustain = 0.5f;
    instrumentPresets[2].patch.p_release = 0.4f;
    instrumentPresets[2].patch.p_filterCutoff = 0.6f;
    instrumentPresets[2].patch.p_filterLfoDepth = 0.15f;
    instrumentPresets[2].patch.p_filterLfoSync = 3;
    instrumentPresets[2].patch.p_arpEnabled = true;
    instrumentPresets[2].patch.p_arpMode = ARP_MODE_UPDOWN;
    instrumentPresets[2].patch.p_arpRateDiv = ARP_RATE_1_16;
    instrumentPresets[2].patch.p_arpChord = ARP_CHORD_MINOR;
    instrumentPresets[2].patch.p_unisonCount = 2;
    instrumentPresets[2].patch.p_unisonDetune = 8.0f;

    // Wobble Bass - Tempo-synced filter wobble
    instrumentPresets[3].name = "Wobble";
    instrumentPresets[3].patch.p_waveType = WAVE_SAW;
    instrumentPresets[3].patch.p_attack = 0.01f;
    instrumentPresets[3].patch.p_sustain = 0.8f;
    instrumentPresets[3].patch.p_release = 0.15f;
    instrumentPresets[3].patch.p_filterCutoff = 0.3f;
    instrumentPresets[3].patch.p_filterResonance = 0.5f;
    instrumentPresets[3].patch.p_filterLfoDepth = 0.5f;
    instrumentPresets[3].patch.p_filterLfoSync = 5;
    instrumentPresets[3].patch.p_unisonCount = 2;
    instrumentPresets[3].patch.p_unisonDetune = 12.0f;
    instrumentPresets[3].patch.p_monoMode = true;

    // 8-bit Pluck - Short percussive square
    instrumentPresets[4].name = "8bit Pluck";
    instrumentPresets[4].patch.p_waveType = WAVE_SQUARE;
    instrumentPresets[4].patch.p_attack = 0.001f;
    instrumentPresets[4].patch.p_decay = 0.15f;
    instrumentPresets[4].patch.p_sustain = 0.0f;
    instrumentPresets[4].patch.p_release = 0.1f;
    instrumentPresets[4].patch.p_pulseWidth = 0.25f;
    instrumentPresets[4].patch.p_filterCutoff = 0.7f;
    instrumentPresets[4].patch.p_filterEnvAmt = 0.4f;
    instrumentPresets[4].patch.p_filterEnvDecay = 0.1f;

    // Retro Strings - Lush detuned pad
    instrumentPresets[5].name = "Strings";
    instrumentPresets[5].patch.p_waveType = WAVE_SAW;
    instrumentPresets[5].patch.p_attack = 0.3f;
    instrumentPresets[5].patch.p_decay = 0.2f;
    instrumentPresets[5].patch.p_sustain = 0.7f;
    instrumentPresets[5].patch.p_release = 0.5f;
    instrumentPresets[5].patch.p_filterCutoff = 0.5f;
    instrumentPresets[5].patch.p_unisonCount = 4;
    instrumentPresets[5].patch.p_unisonDetune = 20.0f;
    instrumentPresets[5].patch.p_vibratoRate = 5.0f;
    instrumentPresets[5].patch.p_vibratoDepth = 0.02f;

    // Chip Organ - Steady square tone
    instrumentPresets[6].name = "Chip Organ";
    instrumentPresets[6].patch.p_waveType = WAVE_SQUARE;
    instrumentPresets[6].patch.p_attack = 0.01f;
    instrumentPresets[6].patch.p_decay = 0.05f;
    instrumentPresets[6].patch.p_sustain = 0.9f;
    instrumentPresets[6].patch.p_release = 0.1f;
    instrumentPresets[6].patch.p_filterCutoff = 0.9f;
    instrumentPresets[6].patch.p_vibratoRate = 6.0f;
    instrumentPresets[6].patch.p_vibratoDepth = 0.015f;

    // Lofi Keys - Soft triangle
    instrumentPresets[7].name = "Lofi Keys";
    instrumentPresets[7].patch.p_waveType = WAVE_TRIANGLE;
    instrumentPresets[7].patch.p_attack = 0.02f;
    instrumentPresets[7].patch.p_decay = 0.3f;
    instrumentPresets[7].patch.p_sustain = 0.4f;
    instrumentPresets[7].patch.p_release = 0.4f;
    instrumentPresets[7].patch.p_filterCutoff = 0.7f;
    instrumentPresets[7].patch.p_filterEnvAmt = 0.2f;
    instrumentPresets[7].patch.p_filterEnvDecay = 0.25f;

    // Acid Lead - Authentic TB-303 with acid mode
    instrumentPresets[8].name = "Acid";
    instrumentPresets[8].patch.p_waveType = WAVE_SAW;
    instrumentPresets[8].patch.p_attack = 0.003f;       // 303 VEG attack ~3ms
    instrumentPresets[8].patch.p_decay = 3.0f;          // 303 VEG decay ~3-4s (very long!)
    instrumentPresets[8].patch.p_sustain = 0.0f;        // 303 has no sustain (attack-decay only)
    instrumentPresets[8].patch.p_release = 0.15f;
    instrumentPresets[8].patch.p_filterCutoff = 0.2f;
    instrumentPresets[8].patch.p_filterResonance = 0.7f;
    instrumentPresets[8].patch.p_filterEnvAmt = 0.6f;
    instrumentPresets[8].patch.p_filterEnvDecay = 0.4f; // MEG decay (Decay knob mid-range)
    instrumentPresets[8].patch.p_monoMode = true;
    instrumentPresets[8].patch.p_glideTime = 0.06f;     // 303 RC slide ~60ms
    instrumentPresets[8].patch.p_acidMode = true;
    instrumentPresets[8].patch.p_accentSweepAmt = 0.5f;
    instrumentPresets[8].patch.p_gimmickDipAmt = 0.3f;

    // Power Arp - Arpeggiated power chord
    instrumentPresets[9].name = "Power Arp";
    instrumentPresets[9].patch.p_waveType = WAVE_SAW;
    instrumentPresets[9].patch.p_attack = 0.01f;
    instrumentPresets[9].patch.p_sustain = 0.6f;
    instrumentPresets[9].patch.p_release = 0.2f;
    instrumentPresets[9].patch.p_filterCutoff = 0.6f;
    instrumentPresets[9].patch.p_filterResonance = 0.15f;
    instrumentPresets[9].patch.p_arpEnabled = true;
    instrumentPresets[9].patch.p_arpMode = ARP_MODE_UP;
    instrumentPresets[9].patch.p_arpRateDiv = ARP_RATE_1_8;
    instrumentPresets[9].patch.p_arpChord = ARP_CHORD_POWER;
    instrumentPresets[9].patch.p_unisonCount = 2;
    instrumentPresets[9].patch.p_unisonDetune = 10.0f;

    // Pluck - Karplus-Strong string
    instrumentPresets[10].name = "Pluck";
    instrumentPresets[10].patch.p_waveType = WAVE_PLUCK;
    instrumentPresets[10].patch.p_envelopeEnabled = true;
    instrumentPresets[10].patch.p_filterEnabled = true;
    instrumentPresets[10].patch.p_filterCutoff = 1.0f;
    instrumentPresets[10].patch.p_filterResonance = 0.0f;
    instrumentPresets[10].patch.p_pluckBrightness = 0.6f;
    instrumentPresets[10].patch.p_pluckDamping = 0.997f;
    instrumentPresets[10].patch.p_pluckDamp = 0.0f;
    instrumentPresets[10].patch.p_attack = 0.001f;
    instrumentPresets[10].patch.p_decay = 0.0f;
    instrumentPresets[10].patch.p_sustain = 1.0f;
    instrumentPresets[10].patch.p_release = 0.3f;

    // Marimba
    instrumentPresets[11].name = "Marimba";
    instrumentPresets[11].patch.p_waveType = WAVE_MALLET;
    instrumentPresets[11].patch.p_envelopeEnabled = true;
    instrumentPresets[11].patch.p_filterEnabled = true;
    instrumentPresets[11].patch.p_filterCutoff = 1.0f;
    instrumentPresets[11].patch.p_filterResonance = 0.0f;
    instrumentPresets[11].patch.p_malletPreset = MALLET_PRESET_MARIMBA;
    instrumentPresets[11].patch.p_malletStiffness = 0.3f;
    instrumentPresets[11].patch.p_malletHardness = 0.5f;
    instrumentPresets[11].patch.p_attack = 0.001f;
    instrumentPresets[11].patch.p_decay = 0.0f;
    instrumentPresets[11].patch.p_sustain = 1.0f;
    instrumentPresets[11].patch.p_release = 0.3f;

    // ========================================================================
    // PIKUNIKU / SILLYCORE PRESETS (12-17)
    // ========================================================================

    // Piku Accordion - Nasal thin pulse lead with vibrato
    instrumentPresets[12].name = "Piku Accord";
    instrumentPresets[12].patch.p_waveType = WAVE_SQUARE;
    instrumentPresets[12].patch.p_pulseWidth = 0.15f;
    instrumentPresets[12].patch.p_attack = 0.005f;
    instrumentPresets[12].patch.p_decay = 0.1f;
    instrumentPresets[12].patch.p_sustain = 0.7f;
    instrumentPresets[12].patch.p_release = 0.15f;
    instrumentPresets[12].patch.p_vibratoRate = 5.0f;
    instrumentPresets[12].patch.p_vibratoDepth = 0.3f;
    instrumentPresets[12].patch.p_filterCutoff = 0.9f;
    instrumentPresets[12].patch.p_filterResonance = 0.1f;

    // Piku Tuba - Bouncy "farty" bass with filter chirp
    instrumentPresets[13].name = "Piku Tuba";
    instrumentPresets[13].patch.p_waveType = WAVE_SAW;
    instrumentPresets[13].patch.p_attack = 0.005f;
    instrumentPresets[13].patch.p_decay = 0.2f;
    instrumentPresets[13].patch.p_sustain = 0.4f;
    instrumentPresets[13].patch.p_release = 0.1f;
    instrumentPresets[13].patch.p_filterCutoff = 0.25f;
    instrumentPresets[13].patch.p_filterResonance = 0.6f;
    instrumentPresets[13].patch.p_filterEnvAmt = 0.4f;
    instrumentPresets[13].patch.p_filterEnvDecay = 0.08f;

    // Piku FM Bell - Bright glassy toy bell
    instrumentPresets[14].name = "Piku Bell";
    instrumentPresets[14].patch.p_waveType = WAVE_FM;
    instrumentPresets[14].patch.p_fmModRatio = 3.5f;
    instrumentPresets[14].patch.p_fmModIndex = 15.71f;
    instrumentPresets[14].patch.p_attack = 0.001f;
    instrumentPresets[14].patch.p_decay = 0.8f;
    instrumentPresets[14].patch.p_sustain = 0.0f;
    instrumentPresets[14].patch.p_release = 0.5f;
    instrumentPresets[14].patch.p_filterCutoff = 1.0f;
    instrumentPresets[14].patch.p_vibratoRate = 6.0f;
    instrumentPresets[14].patch.p_vibratoDepth = 0.1f;

    // Piku Glocken - Toy xylophone/glockenspiel
    instrumentPresets[15].name = "Piku Glock";
    instrumentPresets[15].patch.p_waveType = WAVE_MALLET;
    instrumentPresets[15].patch.p_envelopeEnabled = true;
    instrumentPresets[15].patch.p_filterEnabled = true;
    instrumentPresets[15].patch.p_malletPreset = MALLET_PRESET_GLOCKEN;
    instrumentPresets[15].patch.p_malletStiffness = 0.95f;
    instrumentPresets[15].patch.p_malletHardness = 0.9f;
    instrumentPresets[15].patch.p_filterCutoff = 1.0f;
    instrumentPresets[15].patch.p_attack = 0.001f;
    instrumentPresets[15].patch.p_decay = 0.0f;
    instrumentPresets[15].patch.p_sustain = 1.0f;
    instrumentPresets[15].patch.p_release = 0.3f;

    // Piku Toy Piano - Slightly detuned bell-ish keys
    instrumentPresets[16].name = "Piku Piano";
    instrumentPresets[16].patch.p_waveType = WAVE_ADDITIVE;
    instrumentPresets[16].patch.p_additivePreset = ADDITIVE_PRESET_BELL;
    instrumentPresets[16].patch.p_additiveBrightness = 0.6f;
    instrumentPresets[16].patch.p_additiveInharmonicity = 0.01f;
    instrumentPresets[16].patch.p_attack = 0.001f;
    instrumentPresets[16].patch.p_decay = 0.6f;
    instrumentPresets[16].patch.p_sustain = 0.1f;
    instrumentPresets[16].patch.p_release = 0.3f;
    instrumentPresets[16].patch.p_filterCutoff = 0.85f;

    // Piku Pluck - Cute bouncy pizzicato
    instrumentPresets[17].name = "Piku Pluck";
    instrumentPresets[17].patch.p_waveType = WAVE_PLUCK;
    instrumentPresets[17].patch.p_envelopeEnabled = true;
    instrumentPresets[17].patch.p_filterEnabled = true;
    instrumentPresets[17].patch.p_pluckBrightness = 0.7f;
    instrumentPresets[17].patch.p_pluckDamping = 0.995f;
    instrumentPresets[17].patch.p_pluckDamp = 0.0f;
    instrumentPresets[17].patch.p_attack = 0.001f;
    instrumentPresets[17].patch.p_decay = 0.0f;
    instrumentPresets[17].patch.p_sustain = 1.0f;
    instrumentPresets[17].patch.p_release = 0.3f;
    instrumentPresets[17].patch.p_filterCutoff = 0.9f;

    // ========================================================================
    // MAC DEMARCO / SLACKER INDIE PRESETS (18-23)
    // ========================================================================

    // Mac Jangle - Chorus-y jangly guitar
    instrumentPresets[18].name = "Mac Jangle";
    instrumentPresets[18].patch.p_waveType = WAVE_PLUCK;
    instrumentPresets[18].patch.p_envelopeEnabled = true;
    instrumentPresets[18].patch.p_filterEnabled = true;
    instrumentPresets[18].patch.p_pluckBrightness = 0.5f;
    instrumentPresets[18].patch.p_pluckDamping = 0.994f;
    instrumentPresets[18].patch.p_pluckDamp = 0.0f;
    instrumentPresets[18].patch.p_filterCutoff = 0.6f;
    instrumentPresets[18].patch.p_filterResonance = 0.15f;
    instrumentPresets[18].patch.p_vibratoRate = 4.5f;
    instrumentPresets[18].patch.p_vibratoDepth = 0.15f;
    instrumentPresets[18].patch.p_attack = 0.001f;
    instrumentPresets[18].patch.p_decay = 0.0f;
    instrumentPresets[18].patch.p_sustain = 1.0f;
    instrumentPresets[18].patch.p_release = 0.3f;

    // Mac Juno - Classic Juno-60 pad sound
    instrumentPresets[19].name = "Mac Juno";
    instrumentPresets[19].patch.p_waveType = WAVE_SAW;
    instrumentPresets[19].patch.p_attack = 0.15f;
    instrumentPresets[19].patch.p_decay = 0.3f;
    instrumentPresets[19].patch.p_sustain = 0.7f;
    instrumentPresets[19].patch.p_release = 0.6f;
    instrumentPresets[19].patch.p_filterCutoff = 0.45f;
    instrumentPresets[19].patch.p_filterResonance = 0.1f;
    instrumentPresets[19].patch.p_unisonCount = 2;
    instrumentPresets[19].patch.p_unisonDetune = 12.0f;
    instrumentPresets[19].patch.p_vibratoRate = 0.3f;
    instrumentPresets[19].patch.p_vibratoDepth = 0.08f;

    // Mac Bass - Warm, round, slightly wobbly bass
    instrumentPresets[20].name = "Mac Bass";
    instrumentPresets[20].patch.p_waveType = WAVE_SAW;
    instrumentPresets[20].patch.p_attack = 0.02f;
    instrumentPresets[20].patch.p_decay = 0.25f;
    instrumentPresets[20].patch.p_sustain = 0.5f;
    instrumentPresets[20].patch.p_release = 0.2f;
    instrumentPresets[20].patch.p_filterCutoff = 0.3f;
    instrumentPresets[20].patch.p_filterResonance = 0.25f;
    instrumentPresets[20].patch.p_filterEnvAmt = 0.15f;
    instrumentPresets[20].patch.p_filterEnvDecay = 0.2f;
    instrumentPresets[20].patch.p_vibratoRate = 5.0f;
    instrumentPresets[20].patch.p_vibratoDepth = 0.05f;

    // Mac Keys - Electric piano
    instrumentPresets[21].name = "Mac Keys";
    instrumentPresets[21].patch.p_waveType = WAVE_FM;
    instrumentPresets[21].patch.p_fmModRatio = 1.0f;
    instrumentPresets[21].patch.p_fmModIndex = 7.54f;
    instrumentPresets[21].patch.p_attack = 0.005f;
    instrumentPresets[21].patch.p_decay = 0.5f;
    instrumentPresets[21].patch.p_sustain = 0.3f;
    instrumentPresets[21].patch.p_release = 0.4f;
    instrumentPresets[21].patch.p_filterCutoff = 0.7f;
    instrumentPresets[21].patch.p_vibratoRate = 5.5f;
    instrumentPresets[21].patch.p_vibratoDepth = 0.12f;

    // Mac Vox - Dreamy vocal pad
    instrumentPresets[22].name = "Mac Vox";
    instrumentPresets[22].patch.p_waveType = WAVE_VOICE;
    instrumentPresets[22].patch.p_voiceVowel = VOWEL_O;
    instrumentPresets[22].patch.p_voiceBreathiness = 0.2f;
    instrumentPresets[22].patch.p_attack = 0.2f;
    instrumentPresets[22].patch.p_decay = 0.2f;
    instrumentPresets[22].patch.p_sustain = 0.7f;
    instrumentPresets[22].patch.p_release = 0.5f;
    instrumentPresets[22].patch.p_filterCutoff = 0.6f;
    instrumentPresets[22].patch.p_vibratoRate = 4.0f;
    instrumentPresets[22].patch.p_vibratoDepth = 0.2f;

    // Mac Vibes - Detuned vibraphone
    instrumentPresets[23].name = "Mac Vibes";
    instrumentPresets[23].patch.p_waveType = WAVE_MALLET;
    instrumentPresets[23].patch.p_envelopeEnabled = true;
    instrumentPresets[23].patch.p_filterEnabled = true;
    instrumentPresets[23].patch.p_malletPreset = MALLET_PRESET_VIBES;
    instrumentPresets[23].patch.p_malletTremolo = 0.6f;
    instrumentPresets[23].patch.p_malletTremoloRate = 4.5f;
    instrumentPresets[23].patch.p_filterCutoff = 0.7f;
    instrumentPresets[23].patch.p_vibratoRate = 0.5f;
    instrumentPresets[23].patch.p_vibratoDepth = 0.1f;
    instrumentPresets[23].patch.p_attack = 0.001f;
    instrumentPresets[23].patch.p_decay = 0.0f;
    instrumentPresets[23].patch.p_sustain = 1.0f;
    instrumentPresets[23].patch.p_release = 0.3f;

    // ========================================================================
    // DRUM PRESETS (24-31)
    // Using pitch envelope, noise mix, and retrigger to synthesize drums
    // Play at ~50Hz for kick, ~180Hz for snare, ~800Hz for hat, etc.
    // ========================================================================

    // 808 Kick - Sine with pitch envelope + click + saturation
    // drums.h: sinf osc, pitch 50→150Hz exp decay 0.04s, click 0.3, tanh(1+tone*3), expDecay 0.5s
    instrumentPresets[24].name = "808 Kick";
    instrumentPresets[24].patch.p_waveType = WAVE_FM;      // FM with index=0 ≈ pure sine
    instrumentPresets[24].patch.p_fmModRatio = 1.0f;
    instrumentPresets[24].patch.p_fmModIndex = 0.0f;       // Pure sine
    instrumentPresets[24].patch.p_attack = 0.0f;             // Instant onset (drums.h has no attack ramp)
    instrumentPresets[24].patch.p_decay = 0.5f;
    instrumentPresets[24].patch.p_sustain = 0.0f;
    instrumentPresets[24].patch.p_release = 0.05f;
    instrumentPresets[24].patch.p_expDecay = true;          // Punchy exponential decay
    instrumentPresets[24].patch.p_filterCutoff = 1.0f;      // No filter (drums.h kick is unfiltered)
    instrumentPresets[24].patch.p_pitchEnvAmount = 19.0f;   // ~150/50 Hz = ~1.58 octaves
    instrumentPresets[24].patch.p_pitchEnvDecay = 0.04f;    // Fast punch
    instrumentPresets[24].patch.p_pitchEnvLinear = true;     // Linear Hz sweep (analog drum style)
    instrumentPresets[24].patch.p_clickLevel = 0.3f;         // Click transient (drums.h: kickClick=0.3)
    instrumentPresets[24].patch.p_clickTime = 0.005f;        // drums.h: KICK_CLICK_DURATION
    instrumentPresets[24].patch.p_drive = 0.5f;             // Warm saturation
    instrumentPresets[24].patch.p_useTriggerFreq = true;
    instrumentPresets[24].patch.p_triggerFreq = 50.0f;

    // 808 Snare - Tone + bandpass noise (matching drums.h dual-osc + filtered noise)
    // drums.h: sin(180Hz) + sin(270Hz), BP noise (snappy 0.6), tone/noise separate decay
    instrumentPresets[25].name = "808 Snare";
    instrumentPresets[25].patch.p_waveType = WAVE_FM;       // FM index=0 → pure sine (like drums.h)
    instrumentPresets[25].patch.p_fmModRatio = 1.0f;
    instrumentPresets[25].patch.p_fmModIndex = 0.0f;        // Pure sine for main osc
    instrumentPresets[25].patch.p_osc2Ratio = 1.5f;         // 2nd osc at 1.5× (drums.h: snarePitch*1.5)
    instrumentPresets[25].patch.p_osc2Level = 0.5f;         // drums.h: 0.6 main + 0.3 second
    instrumentPresets[25].patch.p_attack = 0.0f;
    instrumentPresets[25].patch.p_decay = 0.2f;
    instrumentPresets[25].patch.p_sustain = 0.0f;
    instrumentPresets[25].patch.p_release = 0.05f;
    instrumentPresets[25].patch.p_expDecay = true;
    instrumentPresets[25].patch.p_filterCutoff = 1.0f;      // drums.h snare is unfiltered
    instrumentPresets[25].patch.p_noiseMix = 0.6f;          // Snappy noise
    instrumentPresets[25].patch.p_noiseTone = 0.55f;        // LP cutoff (drums.h: 0.15+tone*0.4)
    instrumentPresets[25].patch.p_noiseHP = 0.3f;           // HP to remove rumble (bandpass)
    instrumentPresets[25].patch.p_noiseDecay = 0.2f;        // Noise tail
    instrumentPresets[25].patch.p_useTriggerFreq = true;
    instrumentPresets[25].patch.p_triggerFreq = 180.0f;

    // 808 Clap - Bandpass noise with retrigger
    // drums.h: 4 noise bursts at staggered offsets, BP filter, spread 0.012s
    instrumentPresets[26].name = "808 Clap";
    instrumentPresets[26].patch.p_waveType = WAVE_NOISE;
    instrumentPresets[26].patch.p_attack = 0.0f;
    instrumentPresets[26].patch.p_decay = 0.3f;
    instrumentPresets[26].patch.p_sustain = 0.0f;
    instrumentPresets[26].patch.p_release = 0.05f;
    instrumentPresets[26].patch.p_expDecay = true;
    instrumentPresets[26].patch.p_filterType = 2;             // SVF BP (used as one-pole BP in noiseMode 2)
    instrumentPresets[26].patch.p_filterCutoff = 0.38f;      // drums.h: 0.2+tone*0.3, tone=0.6 → 0.38
    instrumentPresets[26].patch.p_filterResonance = 0.0f;
    instrumentPresets[26].patch.p_noiseTone = 0.9f;          // Bright source noise
    instrumentPresets[26].patch.p_noiseHP = 0.08f;           // HP cutoff for per-burst BP filter
    instrumentPresets[26].patch.p_retriggerCount = 3;
    instrumentPresets[26].patch.p_retriggerSpread = 0.012f;
    instrumentPresets[26].patch.p_retriggerOverlap = true;   // Overlapping bursts like drums.h
    instrumentPresets[26].patch.p_retriggerBurstDecay = 0.02f; // drums.h: expDecay(t, 0.02)
    instrumentPresets[26].patch.p_retriggerCurve = 0.15f;     // Accelerating gaps matching drums.h {0,1,2.2,3.5}×spread
    instrumentPresets[26].patch.p_noiseMode = 2;               // Per-burst noise re-seeding
    instrumentPresets[26].patch.p_noiseType = 1;               // Time-hash noise (drums.h style)
    instrumentPresets[26].patch.p_useTriggerFreq = true;
    instrumentPresets[26].patch.p_triggerFreq = 400.0f;

    // 808 Closed HiHat - 3 square oscillators at metallic ratios + HP noise
    // drums.h: 6 squares at {1,1.45,1.62,1.93,2.50,2.66} + HP filter
    // We use 3 oscs (main + osc2 + osc3) at key ratios from the set
    instrumentPresets[27].name = "808 CH";
    instrumentPresets[27].patch.p_waveType = WAVE_SQUARE;
    // All 6 metallic ratios from drums.h: {1.0, 1.4471, 1.6170, 1.9265, 2.5028, 2.6637}
    instrumentPresets[27].patch.p_osc2Ratio = 1.4471f;
    instrumentPresets[27].patch.p_osc2Level = 1.0f;
    instrumentPresets[27].patch.p_osc3Ratio = 1.6170f;
    instrumentPresets[27].patch.p_osc3Level = 1.0f;
    instrumentPresets[27].patch.p_osc4Ratio = 1.9265f;
    instrumentPresets[27].patch.p_osc4Level = 1.0f;
    instrumentPresets[27].patch.p_osc5Ratio = 2.5028f;
    instrumentPresets[27].patch.p_osc5Level = 1.0f;
    instrumentPresets[27].patch.p_osc6Ratio = 2.6637f;
    instrumentPresets[27].patch.p_osc6Level = 1.0f;
    instrumentPresets[27].patch.p_attack = 0.0f;
    instrumentPresets[27].patch.p_decay = 0.05f;
    instrumentPresets[27].patch.p_sustain = 0.0f;
    instrumentPresets[27].patch.p_release = 0.02f;
    instrumentPresets[27].patch.p_expDecay = true;
    // drums.h hihat: pure squares through one-pole HP (sample - LP(cutoff))
    instrumentPresets[27].patch.p_filterType = 4;            // One-pole HP (matches drums.h)
    instrumentPresets[27].patch.p_filterCutoff = 0.58f;      // drums.h: 0.3+tone*0.4, tone=0.7 → 0.58 (one-pole direct)
    instrumentPresets[27].patch.p_volume = 0.4f;
    instrumentPresets[27].patch.p_phaseReset = true;          // Deterministic attack
    instrumentPresets[27].patch.p_useTriggerFreq = true;
    instrumentPresets[27].patch.p_triggerFreq = 460.0f;
    instrumentPresets[27].patch.p_choke = true;

    // 808 Open HiHat - Same metallic character, longer decay
    instrumentPresets[28].name = "808 OH";
    instrumentPresets[28].patch.p_waveType = WAVE_SQUARE;
    instrumentPresets[28].patch.p_osc2Ratio = 1.4471f;
    instrumentPresets[28].patch.p_osc2Level = 1.0f;
    instrumentPresets[28].patch.p_osc3Ratio = 1.6170f;
    instrumentPresets[28].patch.p_osc3Level = 1.0f;
    instrumentPresets[28].patch.p_osc4Ratio = 1.9265f;
    instrumentPresets[28].patch.p_osc4Level = 1.0f;
    instrumentPresets[28].patch.p_osc5Ratio = 2.5028f;
    instrumentPresets[28].patch.p_osc5Level = 1.0f;
    instrumentPresets[28].patch.p_osc6Ratio = 2.6637f;
    instrumentPresets[28].patch.p_osc6Level = 1.0f;
    instrumentPresets[28].patch.p_attack = 0.0f;
    instrumentPresets[28].patch.p_decay = 0.4f;
    instrumentPresets[28].patch.p_sustain = 0.0f;
    instrumentPresets[28].patch.p_release = 0.1f;
    instrumentPresets[28].patch.p_expDecay = true;
    instrumentPresets[28].patch.p_filterType = 4;            // One-pole HP
    instrumentPresets[28].patch.p_filterCutoff = 0.58f;       // drums.h: 0.3+tone*0.4, tone=0.7 → 0.58
    instrumentPresets[28].patch.p_volume = 0.4f;
    instrumentPresets[28].patch.p_phaseReset = true;
    instrumentPresets[28].patch.p_useTriggerFreq = true;
    instrumentPresets[28].patch.p_triggerFreq = 460.0f;
    instrumentPresets[28].patch.p_choke = true;

    // 808 Tom - Sine with pitch drop + triangle blend
    // drums.h: sin(80Hz*pitchMult) + 20% triangle, pitch drops from 2× to 1×, expDecay 0.3s
    instrumentPresets[29].name = "808 Tom";
    instrumentPresets[29].patch.p_waveType = WAVE_FM;       // FM index=0 ≈ sine
    instrumentPresets[29].patch.p_fmModRatio = 2.0f;
    instrumentPresets[29].patch.p_fmModIndex = 0.94f;       // Hint of harmonics (triangle blend)
    instrumentPresets[29].patch.p_attack = 0.0f;
    instrumentPresets[29].patch.p_decay = 0.3f;
    instrumentPresets[29].patch.p_sustain = 0.0f;
    instrumentPresets[29].patch.p_release = 0.05f;
    instrumentPresets[29].patch.p_expDecay = true;
    instrumentPresets[29].patch.p_filterCutoff = 1.0f;
    instrumentPresets[29].patch.p_pitchEnvAmount = 12.0f;    // 1 octave (2× to 1×)
    instrumentPresets[29].patch.p_pitchEnvDecay = 0.05f;     // drums.h: tomPunchDecay=0.05
    instrumentPresets[29].patch.p_pitchEnvLinear = true;      // Linear Hz (matches drums.h)
    instrumentPresets[29].patch.p_useTriggerFreq = true;
    instrumentPresets[29].patch.p_triggerFreq = 80.0f;

    // Rimshot - Sharp click + high sine
    // drums.h: sin(1700Hz) + noise click, expDecay 0.03s
    instrumentPresets[30].name = "Rimshot";
    instrumentPresets[30].patch.p_waveType = WAVE_FM;
    instrumentPresets[30].patch.p_fmModRatio = 1.0f;
    instrumentPresets[30].patch.p_fmModIndex = 0.0f;        // Pure sine
    instrumentPresets[30].patch.p_attack = 0.0f;
    instrumentPresets[30].patch.p_decay = 0.03f;
    instrumentPresets[30].patch.p_sustain = 0.0f;
    instrumentPresets[30].patch.p_release = 0.01f;
    instrumentPresets[30].patch.p_expDecay = true;
    instrumentPresets[30].patch.p_filterCutoff = 1.0f;
    instrumentPresets[30].patch.p_clickLevel = 0.5f;          // Click transient (drums.h: noise * expDecay(t, 0.005))
    instrumentPresets[30].patch.p_clickTime = 0.005f;
    instrumentPresets[30].patch.p_useTriggerFreq = true;
    instrumentPresets[30].patch.p_triggerFreq = 1700.0f;

    // Cowbell - Two square waves at non-harmonic interval
    // drums.h: sq(560Hz) + sq(560*1.508Hz), lowpass filter, expDecay 0.3s
    instrumentPresets[31].name = "Cowbell";
    instrumentPresets[31].patch.p_waveType = WAVE_SQUARE;
    instrumentPresets[31].patch.p_osc2Ratio = 1.508f;        // Classic cowbell ratio
    instrumentPresets[31].patch.p_osc2Level = 1.0f;           // Equal weight with main
    instrumentPresets[31].patch.p_attack = 0.0f;
    instrumentPresets[31].patch.p_decay = 0.3f;
    instrumentPresets[31].patch.p_sustain = 0.0f;
    instrumentPresets[31].patch.p_release = 0.05f;
    instrumentPresets[31].patch.p_expDecay = true;
    instrumentPresets[31].patch.p_filterCutoff = 0.4f;       // Lowpass (drums.h: LP 0.15, SVF squares internally)
    instrumentPresets[31].patch.p_useTriggerFreq = true;
    instrumentPresets[31].patch.p_triggerFreq = 560.0f;

    // ========================================================================
    // CR-78 DRUM PRESETS (32-35)
    // Roland CR-78 — warmer, more analog character than 808
    // ========================================================================

    // CR-78 Kick - Bridged-T resonant filter, subtle 2nd harmonic
    // drums.h: sin + sin*2*0.15, pitch 80Hz, slight pitch drop 0.02s, resonance 0.9, expDecay 0.25s
    instrumentPresets[32].name = "CR78 Kick";
    instrumentPresets[32].patch.p_waveType = WAVE_FM;
    instrumentPresets[32].patch.p_fmModRatio = 2.0f;        // 2nd harmonic
    instrumentPresets[32].patch.p_fmModIndex = 0.94f;        // Subtle (drums.h: sin*2 * 0.15)
    instrumentPresets[32].patch.p_attack = 0.0f;
    instrumentPresets[32].patch.p_sustain = 0.0f;
    instrumentPresets[32].patch.p_release = 0.05f;
    instrumentPresets[32].patch.p_expDecay = true;
    instrumentPresets[32].patch.p_filterCutoff = 1.0f;
    instrumentPresets[32].patch.p_decay = 0.036f;            // drums.h: 0.25 * (1 - 0.9*0.95) = 0.036
    instrumentPresets[32].patch.p_pitchEnvAmount = 5.0f;     // Slight pitch drop (drums.h: 1.3× ratio)
    instrumentPresets[32].patch.p_pitchEnvDecay = 0.02f;
    instrumentPresets[32].patch.p_pitchEnvLinear = true;      // Linear Hz (matches drums.h)
    instrumentPresets[32].patch.p_clickLevel = 0.2f;         // Soft click (drums.h: noise * 0.2 for 0.005s)
    instrumentPresets[32].patch.p_clickTime = 0.005f;
    instrumentPresets[32].patch.p_useTriggerFreq = true;
    instrumentPresets[32].patch.p_triggerFreq = 80.0f;

    // CR-78 Snare - Resonant ping + bandpassed noise
    // drums.h: sin(220Hz), BP noise (snappy 0.5), ping decays at 0.5× main, expDecay 0.15s
    instrumentPresets[33].name = "CR78 Snare";
    instrumentPresets[33].patch.p_waveType = WAVE_FM;
    instrumentPresets[33].patch.p_fmModRatio = 1.0f;
    instrumentPresets[33].patch.p_fmModIndex = 0.0f;         // Pure sine ping
    instrumentPresets[33].patch.p_attack = 0.0f;
    instrumentPresets[33].patch.p_decay = 0.15f;
    instrumentPresets[33].patch.p_sustain = 0.0f;
    instrumentPresets[33].patch.p_release = 0.05f;
    instrumentPresets[33].patch.p_expDecay = true;
    instrumentPresets[33].patch.p_filterCutoff = 1.0f;
    instrumentPresets[33].patch.p_noiseMix = 0.5f;           // drums.h snappy 0.5
    instrumentPresets[33].patch.p_noiseTone = 0.5f;          // BP: LP cutoff 0.25
    instrumentPresets[33].patch.p_noiseHP = 0.28f;           // BP: HP cutoff 0.08
    instrumentPresets[33].patch.p_noiseDecay = 0.15f;
    instrumentPresets[33].patch.p_useTriggerFreq = true;
    instrumentPresets[33].patch.p_triggerFreq = 220.0f;

    // CR-78 Hihat - 3 square oscillators at ratios + noise sizzle
    // drums.h: 3 squares {1.0, 1.34, 1.68} at 400+tone*300 Hz, noise 0.3, BP filter
    instrumentPresets[34].name = "CR78 HH";
    instrumentPresets[34].patch.p_waveType = WAVE_SQUARE;
    instrumentPresets[34].patch.p_osc2Ratio = 1.34f;         // drums.h ratio
    instrumentPresets[34].patch.p_osc2Level = 1.0f;
    instrumentPresets[34].patch.p_osc3Ratio = 1.68f;         // drums.h ratio
    instrumentPresets[34].patch.p_osc3Level = 1.0f;
    instrumentPresets[34].patch.p_attack = 0.0f;
    instrumentPresets[34].patch.p_decay = 0.08f;
    instrumentPresets[34].patch.p_sustain = 0.0f;
    instrumentPresets[34].patch.p_release = 0.02f;
    instrumentPresets[34].patch.p_expDecay = true;
    // drums.h: BP filter (LP 0.275, HP 0.05) × 2.5 gain, scale 0.35
    instrumentPresets[34].patch.p_filterType = 2;            // Bandpass
    instrumentPresets[34].patch.p_filterCutoff = 0.28f;      // BP cutoff
    instrumentPresets[34].patch.p_noiseMix = 0.3f;           // drums.h: noise * 0.3
    instrumentPresets[34].patch.p_noiseTone = 0.6f;
    instrumentPresets[34].patch.p_noiseHP = 0.15f;
    instrumentPresets[34].patch.p_noiseDecay = 0.06f;
    instrumentPresets[34].patch.p_volume = 0.35f;
    instrumentPresets[34].patch.p_useTriggerFreq = true;
    instrumentPresets[34].patch.p_triggerFreq = 580.0f;
    instrumentPresets[34].patch.p_choke = true;

    // CR-78 Metal - 3 squares at octave+fifth through lowpass
    // drums.h: squares {1.0, 1.5, 2.0} at 800Hz, LP 0.08 + dry 0.3, expDecay 0.15s
    instrumentPresets[35].name = "CR78 Metal";
    instrumentPresets[35].patch.p_waveType = WAVE_SQUARE;
    instrumentPresets[35].patch.p_osc2Ratio = 1.5f;          // Fifth interval
    instrumentPresets[35].patch.p_osc2Level = 0.8f;          // drums.h level
    instrumentPresets[35].patch.p_osc3Ratio = 2.0f;          // Octave
    instrumentPresets[35].patch.p_osc3Level = 0.6f;          // drums.h level
    instrumentPresets[35].patch.p_attack = 0.0f;
    instrumentPresets[35].patch.p_decay = 0.15f;
    instrumentPresets[35].patch.p_sustain = 0.0f;
    instrumentPresets[35].patch.p_release = 0.05f;
    instrumentPresets[35].patch.p_expDecay = true;
    instrumentPresets[35].patch.p_filterCutoff = 0.3f;       // Inductor-style LP
    instrumentPresets[35].patch.p_useTriggerFreq = true;
    instrumentPresets[35].patch.p_triggerFreq = 800.0f;

    // ========================================================================
    // PERCUSSION PRESETS (36-37)
    // ========================================================================

    // Clave - Pure sine click
    // drums.h: sin(2500Hz), expDecay 0.02s
    instrumentPresets[36].name = "Clave";
    instrumentPresets[36].patch.p_waveType = WAVE_FM;
    instrumentPresets[36].patch.p_fmModRatio = 1.0f;
    instrumentPresets[36].patch.p_fmModIndex = 0.0f;         // Pure sine
    instrumentPresets[36].patch.p_attack = 0.0f;
    instrumentPresets[36].patch.p_decay = 0.02f;
    instrumentPresets[36].patch.p_sustain = 0.0f;
    instrumentPresets[36].patch.p_release = 0.01f;
    instrumentPresets[36].patch.p_expDecay = true;
    instrumentPresets[36].patch.p_filterCutoff = 1.0f;       // Bright, no filtering
    instrumentPresets[36].patch.p_useTriggerFreq = true;
    instrumentPresets[36].patch.p_triggerFreq = 2500.0f;

    // Maracas - Highpass-filtered noise burst
    // drums.h: HP noise (cutoff 0.3+tone*0.4, tone=0.8), expDecay 0.07s
    instrumentPresets[37].name = "Maracas";
    instrumentPresets[37].patch.p_waveType = WAVE_NOISE;
    instrumentPresets[37].patch.p_attack = 0.0f;
    instrumentPresets[37].patch.p_decay = 0.07f;
    instrumentPresets[37].patch.p_sustain = 0.0f;
    instrumentPresets[37].patch.p_release = 0.01f;
    instrumentPresets[37].patch.p_expDecay = true;
    instrumentPresets[37].patch.p_noiseMode = 1;              // Replace osc (pure noise → main filter)
    instrumentPresets[37].patch.p_noiseLPBypass = true;       // Raw noise (no LP coloring)
    instrumentPresets[37].patch.p_filterType = 4;             // One-pole HP (matches drums.h topology)
    instrumentPresets[37].patch.p_filterCutoff = 0.62f;       // drums.h: 0.3+tone*0.4, tone=0.8 → 0.62
    instrumentPresets[37].patch.p_noiseHP = 0.0f;             // HP handled by main filter now
    instrumentPresets[37].patch.p_noiseType = 1;               // Time-hash noise (drums.h style)
    instrumentPresets[37].patch.p_useTriggerFreq = true;
    instrumentPresets[37].patch.p_triggerFreq = 800.0f;

    // ========================================================================
    // Song instrument presets (38+) — used by sound_synth_bridge song triggers
    // ========================================================================

    // 38: Warm Triangle Bass — mellow bass for dormitory/default song
    instrumentPresets[38].name = "Warm Tri Bass";
    instrumentPresets[38].patch.p_waveType = WAVE_TRIANGLE;
    instrumentPresets[38].patch.p_attack = 0.3f;
    instrumentPresets[38].patch.p_decay = 0.6f;
    instrumentPresets[38].patch.p_sustain = 0.4f;
    instrumentPresets[38].patch.p_release = 1.2f;
    instrumentPresets[38].patch.p_volume = 0.5f;
    instrumentPresets[38].patch.p_filterCutoff = 0.25f;
    instrumentPresets[38].patch.p_filterResonance = 0.15f;

    // 39: Dark Organ — slow, dark additive organ for suspense
    instrumentPresets[39].name = "Dark Organ";
    instrumentPresets[39].patch.p_waveType = WAVE_ADDITIVE;
    instrumentPresets[39].patch.p_additivePreset = ADDITIVE_PRESET_ORGAN;
    instrumentPresets[39].patch.p_attack = 0.8f;
    instrumentPresets[39].patch.p_decay = 1.5f;
    instrumentPresets[39].patch.p_sustain = 0.6f;
    instrumentPresets[39].patch.p_release = 3.0f;
    instrumentPresets[39].patch.p_volume = 0.45f;
    instrumentPresets[39].patch.p_filterCutoff = 0.18f;
    instrumentPresets[39].patch.p_filterResonance = 0.20f;

    // 40: Eerie Vowel — dark vocal pad for suspense lead
    instrumentPresets[40].name = "Eerie Vowel";
    instrumentPresets[40].patch.p_waveType = WAVE_VOICE;
    instrumentPresets[40].patch.p_voiceVowel = VOWEL_U;
    instrumentPresets[40].patch.p_attack = 0.5f;
    instrumentPresets[40].patch.p_decay = 1.0f;
    instrumentPresets[40].patch.p_sustain = 0.4f;
    instrumentPresets[40].patch.p_release = 2.0f;
    instrumentPresets[40].patch.p_volume = 0.4f;
    instrumentPresets[40].patch.p_glideTime = 0.3f;

    // 41: Triangle Sub — deep house sub bass
    instrumentPresets[41].name = "Tri Sub";
    instrumentPresets[41].patch.p_waveType = WAVE_TRIANGLE;
    instrumentPresets[41].patch.p_attack = 0.01f;
    instrumentPresets[41].patch.p_decay = 0.3f;
    instrumentPresets[41].patch.p_sustain = 0.7f;
    instrumentPresets[41].patch.p_release = 0.2f;
    instrumentPresets[41].patch.p_volume = 0.55f;
    instrumentPresets[41].patch.p_filterCutoff = 0.15f;
    instrumentPresets[41].patch.p_filterResonance = 0.05f;

    // 42: Dark Choir Pad — soft choir for Dilla/default chord tracks
    instrumentPresets[42].name = "Dark Choir";
    instrumentPresets[42].patch.p_waveType = WAVE_ADDITIVE;
    instrumentPresets[42].patch.p_additivePreset = ADDITIVE_PRESET_CHOIR;
    instrumentPresets[42].patch.p_attack = 0.6f;
    instrumentPresets[42].patch.p_decay = 1.2f;
    instrumentPresets[42].patch.p_sustain = 0.35f;
    instrumentPresets[42].patch.p_release = 2.0f;
    instrumentPresets[42].patch.p_volume = 0.25f;
    instrumentPresets[42].patch.p_filterCutoff = 0.22f;
    instrumentPresets[42].patch.p_filterResonance = 0.10f;

    // 43: Lush Strings — additive strings for Mr Lucky/Atmosphere
    instrumentPresets[43].name = "Lush Strings";
    instrumentPresets[43].patch.p_waveType = WAVE_ADDITIVE;
    instrumentPresets[43].patch.p_additivePreset = ADDITIVE_PRESET_STRINGS;
    instrumentPresets[43].patch.p_attack = 0.8f;
    instrumentPresets[43].patch.p_decay = 1.5f;
    instrumentPresets[43].patch.p_sustain = 0.5f;
    instrumentPresets[43].patch.p_release = 2.5f;
    instrumentPresets[43].patch.p_volume = 0.30f;
    instrumentPresets[43].patch.p_filterCutoff = 0.40f;
    instrumentPresets[43].patch.p_filterResonance = 0.06f;

    // 44: Warm Pluck — Karplus-Strong bass for jazz/Dilla/atmosphere
    instrumentPresets[44].name = "Warm Pluck";
    instrumentPresets[44].patch.p_waveType = WAVE_PLUCK;
    instrumentPresets[44].patch.p_envelopeEnabled = true;
    instrumentPresets[44].patch.p_filterEnabled = true;
    instrumentPresets[44].patch.p_pluckBrightness = 0.4f;
    instrumentPresets[44].patch.p_pluckDamp = 0.0f;
    instrumentPresets[44].patch.p_attack = 0.001f;
    instrumentPresets[44].patch.p_decay = 0.0f;
    instrumentPresets[44].patch.p_sustain = 1.0f;
    instrumentPresets[44].patch.p_release = 0.3f;
    instrumentPresets[44].patch.p_pluckDamping = 0.5f;
    instrumentPresets[44].patch.p_volume = 0.55f;
    instrumentPresets[44].patch.p_filterCutoff = 0.30f;
    instrumentPresets[44].patch.p_filterResonance = 0.10f;

    // ========================================================================
    // MALLET PRESETS (45-47) — engines exist, zero presets exposed
    // ========================================================================

    // 45: Glockenspiel — bright, glassy, short
    instrumentPresets[45].name = "Glockenspiel";
    instrumentPresets[45].patch.p_waveType = WAVE_MALLET;
    instrumentPresets[45].patch.p_envelopeEnabled = true;
    instrumentPresets[45].patch.p_filterEnabled = true;
    instrumentPresets[45].patch.p_malletPreset = MALLET_PRESET_GLOCKEN;
    instrumentPresets[45].patch.p_malletStiffness = 0.95f;
    instrumentPresets[45].patch.p_malletHardness = 0.9f;
    instrumentPresets[45].patch.p_malletResonance = 0.6f;
    instrumentPresets[45].patch.p_attack = 0.001f;
    instrumentPresets[45].patch.p_decay = 1.2f;
    instrumentPresets[45].patch.p_sustain = 0.0f;
    instrumentPresets[45].patch.p_release = 0.5f;
    instrumentPresets[45].patch.p_filterCutoff = 0.9f;

    // 46: Xylophone — woody, dry, short
    instrumentPresets[46].name = "Xylophone";
    instrumentPresets[46].patch.p_waveType = WAVE_MALLET;
    instrumentPresets[46].patch.p_envelopeEnabled = true;
    instrumentPresets[46].patch.p_filterEnabled = true;
    instrumentPresets[46].patch.p_malletPreset = MALLET_PRESET_MARIMBA;
    instrumentPresets[46].patch.p_malletStiffness = 0.7f;
    instrumentPresets[46].patch.p_malletHardness = 0.85f;
    instrumentPresets[46].patch.p_malletResonance = 0.4f;
    instrumentPresets[46].patch.p_malletDamp = 0.3f;
    instrumentPresets[46].patch.p_attack = 0.001f;
    instrumentPresets[46].patch.p_decay = 0.6f;
    instrumentPresets[46].patch.p_sustain = 0.0f;
    instrumentPresets[46].patch.p_release = 0.2f;
    instrumentPresets[46].patch.p_filterCutoff = 0.75f;

    // 47: Tubular Bells — long sustain, metallic
    instrumentPresets[47].name = "Tubular Bell";
    instrumentPresets[47].patch.p_waveType = WAVE_MALLET;
    instrumentPresets[47].patch.p_envelopeEnabled = true;
    instrumentPresets[47].patch.p_filterEnabled = true;
    instrumentPresets[47].patch.p_malletPreset = MALLET_PRESET_VIBES;
    instrumentPresets[47].patch.p_malletStiffness = 0.5f;
    instrumentPresets[47].patch.p_malletHardness = 0.7f;
    instrumentPresets[47].patch.p_malletResonance = 0.85f;
    instrumentPresets[47].patch.p_attack = 0.001f;
    instrumentPresets[47].patch.p_decay = 3.0f;
    instrumentPresets[47].patch.p_sustain = 0.0f;
    instrumentPresets[47].patch.p_release = 1.0f;
    instrumentPresets[47].patch.p_filterCutoff = 0.85f;

    // ========================================================================
    // ADDITIVE PRESETS (48-51) — entire engine invisible until now
    // ========================================================================

    // 48: Choir Pad — soft, warm vocal pad
    instrumentPresets[48].name = "Choir";
    instrumentPresets[48].patch.p_waveType = WAVE_ADDITIVE;
    instrumentPresets[48].patch.p_additivePreset = ADDITIVE_PRESET_CHOIR;
    instrumentPresets[48].patch.p_additiveBrightness = 0.4f;
    instrumentPresets[48].patch.p_attack = 0.4f;
    instrumentPresets[48].patch.p_decay = 0.8f;
    instrumentPresets[48].patch.p_sustain = 0.6f;
    instrumentPresets[48].patch.p_release = 1.0f;
    instrumentPresets[48].patch.p_filterCutoff = 0.5f;
    instrumentPresets[48].patch.p_vibratoRate = 5.0f;
    instrumentPresets[48].patch.p_vibratoDepth = 0.08f;

    // 49: Brass — bold, bright brass section
    instrumentPresets[49].name = "Brass";
    instrumentPresets[49].patch.p_waveType = WAVE_ADDITIVE;
    instrumentPresets[49].patch.p_additivePreset = ADDITIVE_PRESET_BRASS;
    instrumentPresets[49].patch.p_additiveBrightness = 0.7f;
    instrumentPresets[49].patch.p_attack = 0.08f;
    instrumentPresets[49].patch.p_decay = 0.3f;
    instrumentPresets[49].patch.p_sustain = 0.7f;
    instrumentPresets[49].patch.p_release = 0.2f;
    instrumentPresets[49].patch.p_filterCutoff = 0.7f;
    instrumentPresets[49].patch.p_filterEnvAmt = 0.2f;
    instrumentPresets[49].patch.p_filterEnvDecay = 0.15f;

    // 50: Strings Section — lush bowed strings
    instrumentPresets[50].name = "Strings Sect";
    instrumentPresets[50].patch.p_waveType = WAVE_ADDITIVE;
    instrumentPresets[50].patch.p_additivePreset = ADDITIVE_PRESET_STRINGS;
    instrumentPresets[50].patch.p_additiveBrightness = 0.5f;
    instrumentPresets[50].patch.p_additiveShimmer = 0.1f;
    instrumentPresets[50].patch.p_attack = 0.5f;
    instrumentPresets[50].patch.p_decay = 1.0f;
    instrumentPresets[50].patch.p_sustain = 0.65f;
    instrumentPresets[50].patch.p_release = 0.8f;
    instrumentPresets[50].patch.p_filterCutoff = 0.55f;
    instrumentPresets[50].patch.p_vibratoRate = 5.5f;
    instrumentPresets[50].patch.p_vibratoDepth = 0.04f;

    // 51: Crystal Bell — bright additive bell
    instrumentPresets[51].name = "Crystal Bell";
    instrumentPresets[51].patch.p_waveType = WAVE_ADDITIVE;
    instrumentPresets[51].patch.p_additivePreset = ADDITIVE_PRESET_BELL;
    instrumentPresets[51].patch.p_additiveBrightness = 0.8f;
    instrumentPresets[51].patch.p_additiveInharmonicity = 0.02f;
    instrumentPresets[51].patch.p_attack = 0.001f;
    instrumentPresets[51].patch.p_decay = 2.0f;
    instrumentPresets[51].patch.p_sustain = 0.0f;
    instrumentPresets[51].patch.p_release = 1.0f;
    instrumentPresets[51].patch.p_filterCutoff = 0.9f;

    // ========================================================================
    // PHASE DISTORTION PRESETS (52-53)
    // ========================================================================

    // 52: PD Bass — deep CZ-style bass
    instrumentPresets[52].name = "PD Bass";
    instrumentPresets[52].patch.p_waveType = WAVE_PD;
    instrumentPresets[52].patch.p_pdWaveType = PD_WAVE_SAW;
    instrumentPresets[52].patch.p_pdDistortion = 0.7f;
    instrumentPresets[52].patch.p_attack = 0.005f;
    instrumentPresets[52].patch.p_decay = 0.3f;
    instrumentPresets[52].patch.p_sustain = 0.4f;
    instrumentPresets[52].patch.p_release = 0.15f;
    instrumentPresets[52].patch.p_filterCutoff = 0.35f;
    instrumentPresets[52].patch.p_filterResonance = 0.3f;
    instrumentPresets[52].patch.p_filterEnvAmt = 0.3f;
    instrumentPresets[52].patch.p_filterEnvDecay = 0.15f;

    // 53: PD Lead — bright resonant CZ lead
    instrumentPresets[53].name = "PD Lead";
    instrumentPresets[53].patch.p_waveType = WAVE_PD;
    instrumentPresets[53].patch.p_pdWaveType = PD_WAVE_SQUARE;
    instrumentPresets[53].patch.p_pdDistortion = 0.5f;
    instrumentPresets[53].patch.p_attack = 0.01f;
    instrumentPresets[53].patch.p_decay = 0.2f;
    instrumentPresets[53].patch.p_sustain = 0.6f;
    instrumentPresets[53].patch.p_release = 0.2f;
    instrumentPresets[53].patch.p_filterCutoff = 0.6f;
    instrumentPresets[53].patch.p_filterResonance = 0.4f;
    instrumentPresets[53].patch.p_monoMode = true;
    instrumentPresets[53].patch.p_glideTime = 0.05f;

    // ========================================================================
    // MEMBRANE & BIRD PRESETS (54-55)
    // ========================================================================

    // 54: Melodic Tabla — pitched membrane with bend
    instrumentPresets[54].name = "Mel Tabla";
    instrumentPresets[54].patch.p_waveType = WAVE_MEMBRANE;
    instrumentPresets[54].patch.p_envelopeEnabled = true;
    instrumentPresets[54].patch.p_filterEnabled = true;
    instrumentPresets[54].patch.p_filterCutoff = 1.0f;
    instrumentPresets[54].patch.p_filterResonance = 0.0f;
    instrumentPresets[54].patch.p_membranePreset = MEMBRANE_TABLA;
    instrumentPresets[54].patch.p_membraneDamping = 0.2f;
    instrumentPresets[54].patch.p_membraneStrike = 0.4f;
    instrumentPresets[54].patch.p_membraneBend = 0.2f;
    instrumentPresets[54].patch.p_membraneBendDecay = 0.1f;
    instrumentPresets[54].patch.p_attack = 0.001f;
    instrumentPresets[54].patch.p_decay = 0.8f;
    instrumentPresets[54].patch.p_sustain = 0.0f;
    instrumentPresets[54].patch.p_release = 0.3f;

    // 55: Bird Ambience — nature background texture
    instrumentPresets[55].name = "Bird Ambience";
    instrumentPresets[55].patch.p_waveType = WAVE_BIRD;
    instrumentPresets[55].patch.p_envelopeEnabled = true;
    instrumentPresets[55].patch.p_filterEnabled = true;
    instrumentPresets[55].patch.p_filterCutoff = 1.0f;
    instrumentPresets[55].patch.p_filterResonance = 0.0f;
    instrumentPresets[55].patch.p_birdType = BIRD_CHIRP;
    instrumentPresets[55].patch.p_birdChirpRange = 0.8f;
    instrumentPresets[55].patch.p_birdTrillRate = 8.0f;
    instrumentPresets[55].patch.p_birdTrillDepth = 0.3f;
    instrumentPresets[55].patch.p_birdAmRate = 3.0f;
    instrumentPresets[55].patch.p_birdAmDepth = 0.4f;
    instrumentPresets[55].patch.p_attack = 0.1f;
    instrumentPresets[55].patch.p_decay = 0.5f;
    instrumentPresets[55].patch.p_sustain = 0.4f;
    instrumentPresets[55].patch.p_release = 0.5f;

    // ========================================================================
    // CORE MELODIC PRESETS (56-61) — Rhodes, upright bass, flute, kalimba, sub, nylon
    // ========================================================================

    // 56: Rhodes Mellow — classic warm EP
    instrumentPresets[56].name = "Rhodes Mel";
    instrumentPresets[56].patch.p_waveType = WAVE_FM;
    instrumentPresets[56].patch.p_fmModRatio = 1.0f;
    instrumentPresets[56].patch.p_fmModIndex = 5.03f;
    instrumentPresets[56].patch.p_fmFeedback = 0.0f;
    instrumentPresets[56].patch.p_attack = 0.005f;
    instrumentPresets[56].patch.p_decay = 0.8f;
    instrumentPresets[56].patch.p_sustain = 0.3f;
    instrumentPresets[56].patch.p_release = 0.5f;
    instrumentPresets[56].patch.p_filterCutoff = 0.5f;
    instrumentPresets[56].patch.p_filterKeyTrack = 0.5f;
    instrumentPresets[56].patch.p_vibratoRate = 5.5f;
    instrumentPresets[56].patch.p_vibratoDepth = 0.08f;
    instrumentPresets[56].patch.p_tubeSaturation = true;

    // 57: Rhodes Bright — cutting EP with bite
    instrumentPresets[57].name = "Rhodes Brt";
    instrumentPresets[57].patch.p_waveType = WAVE_FM;
    instrumentPresets[57].patch.p_fmModRatio = 1.0f;
    instrumentPresets[57].patch.p_fmModIndex = 11.31f;
    instrumentPresets[57].patch.p_fmFeedback = 0.1f;
    instrumentPresets[57].patch.p_attack = 0.003f;
    instrumentPresets[57].patch.p_decay = 0.6f;
    instrumentPresets[57].patch.p_sustain = 0.35f;
    instrumentPresets[57].patch.p_release = 0.4f;
    instrumentPresets[57].patch.p_filterCutoff = 0.7f;
    instrumentPresets[57].patch.p_filterKeyTrack = 0.6f;
    instrumentPresets[57].patch.p_vibratoRate = 5.5f;
    instrumentPresets[57].patch.p_vibratoDepth = 0.05f;

    // 58: Upright Bass — warm plucked jazz bass
    instrumentPresets[58].name = "Upright Bass";
    instrumentPresets[58].patch.p_waveType = WAVE_PLUCK;
    instrumentPresets[58].patch.p_envelopeEnabled = true;
    instrumentPresets[58].patch.p_filterEnabled = true;
    instrumentPresets[58].patch.p_pluckBrightness = 0.35f;
    instrumentPresets[58].patch.p_pluckDamping = 0.993f;
    instrumentPresets[58].patch.p_pluckDamp = 0.0f;
    instrumentPresets[58].patch.p_filterCutoff = 0.25f;
    instrumentPresets[58].patch.p_filterResonance = 0.1f;
    instrumentPresets[58].patch.p_volume = 0.6f;
    instrumentPresets[58].patch.p_analogRolloff = true;
    instrumentPresets[58].patch.p_tubeSaturation = true;
    instrumentPresets[58].patch.p_attack = 0.001f;
    instrumentPresets[58].patch.p_decay = 0.0f;
    instrumentPresets[58].patch.p_sustain = 1.0f;
    instrumentPresets[58].patch.p_release = 0.3f;

    // 59: Flute — breathy SNES/RPG wind
    instrumentPresets[59].name = "Flute";
    instrumentPresets[59].patch.p_waveType = WAVE_TRIANGLE;
    instrumentPresets[59].patch.p_attack = 0.08f;
    instrumentPresets[59].patch.p_decay = 0.2f;
    instrumentPresets[59].patch.p_sustain = 0.65f;
    instrumentPresets[59].patch.p_release = 0.3f;
    instrumentPresets[59].patch.p_filterCutoff = 0.6f;
    instrumentPresets[59].patch.p_vibratoRate = 5.0f;
    instrumentPresets[59].patch.p_vibratoDepth = 0.15f;
    instrumentPresets[59].patch.p_noiseMix = 0.05f;
    instrumentPresets[59].patch.p_noiseTone = 0.8f;
    instrumentPresets[59].patch.p_analogRolloff = true;

    // 60: Kalimba — lo-fi thumb piano
    instrumentPresets[60].name = "Kalimba";
    instrumentPresets[60].patch.p_waveType = WAVE_MALLET;
    instrumentPresets[60].patch.p_envelopeEnabled = true;
    instrumentPresets[60].patch.p_filterEnabled = true;
    instrumentPresets[60].patch.p_malletPreset = MALLET_PRESET_MARIMBA;
    instrumentPresets[60].patch.p_malletStiffness = 0.6f;
    instrumentPresets[60].patch.p_malletHardness = 0.4f;
    instrumentPresets[60].patch.p_malletResonance = 0.75f;
    instrumentPresets[60].patch.p_attack = 0.001f;
    instrumentPresets[60].patch.p_decay = 1.5f;
    instrumentPresets[60].patch.p_sustain = 0.0f;
    instrumentPresets[60].patch.p_release = 0.5f;
    instrumentPresets[60].patch.p_filterCutoff = 0.65f;
    instrumentPresets[60].patch.p_analogRolloff = true;

    // 61: Sub Bass — deep sine sub for lo-fi/house
    instrumentPresets[61].name = "Sub Bass";
    instrumentPresets[61].patch.p_waveType = WAVE_FM;
    instrumentPresets[61].patch.p_fmModRatio = 1.0f;
    instrumentPresets[61].patch.p_fmModIndex = 0.0f;   // Pure sine
    instrumentPresets[61].patch.p_attack = 0.01f;
    instrumentPresets[61].patch.p_decay = 0.2f;
    instrumentPresets[61].patch.p_sustain = 0.8f;
    instrumentPresets[61].patch.p_release = 0.15f;
    instrumentPresets[61].patch.p_filterCutoff = 0.2f;
    instrumentPresets[61].patch.p_volume = 0.6f;

    // 62: Nylon Guitar — bossa nova pluck
    instrumentPresets[62].name = "Nylon Guitar";
    instrumentPresets[62].patch.p_waveType = WAVE_PLUCK;
    instrumentPresets[62].patch.p_envelopeEnabled = true;
    instrumentPresets[62].patch.p_filterEnabled = true;
    instrumentPresets[62].patch.p_pluckBrightness = 0.45f;
    instrumentPresets[62].patch.p_pluckDamp = 0.0f;
    instrumentPresets[62].patch.p_attack = 0.001f;
    instrumentPresets[62].patch.p_decay = 0.0f;
    instrumentPresets[62].patch.p_sustain = 1.0f;
    instrumentPresets[62].patch.p_release = 0.3f;
    instrumentPresets[62].patch.p_pluckDamping = 0.996f;
    instrumentPresets[62].patch.p_filterCutoff = 0.5f;
    instrumentPresets[62].patch.p_filterResonance = 0.05f;
    instrumentPresets[62].patch.p_analogRolloff = true;

    // ========================================================================
    // WAVE 0 SYNTHESIS DEMO PRESETS (63-65) — showcase new features
    // ========================================================================

    // 63: Wavefold Lead — West Coast screaming lead
    instrumentPresets[63].name = "Wavefold Lead";
    instrumentPresets[63].patch.p_waveType = WAVE_SAW;
    instrumentPresets[63].patch.p_wavefoldAmount = 0.6f;
    instrumentPresets[63].patch.p_attack = 0.01f;
    instrumentPresets[63].patch.p_decay = 0.2f;
    instrumentPresets[63].patch.p_sustain = 0.6f;
    instrumentPresets[63].patch.p_release = 0.2f;
    instrumentPresets[63].patch.p_filterCutoff = 0.7f;
    instrumentPresets[63].patch.p_filterResonance = 0.2f;
    instrumentPresets[63].patch.p_monoMode = true;
    instrumentPresets[63].patch.p_glideTime = 0.05f;

    // 64: Ring Mod Bell — metallic, inharmonic bell
    instrumentPresets[64].name = "Ring Bell";
    instrumentPresets[64].patch.p_waveType = WAVE_TRIANGLE;
    instrumentPresets[64].patch.p_ringMod = true;
    instrumentPresets[64].patch.p_ringModFreq = 3.5f;
    instrumentPresets[64].patch.p_attack = 0.001f;
    instrumentPresets[64].patch.p_decay = 1.5f;
    instrumentPresets[64].patch.p_sustain = 0.0f;
    instrumentPresets[64].patch.p_release = 0.5f;
    instrumentPresets[64].patch.p_filterCutoff = 0.8f;

    // 65: Sync Lead — classic hard sync screaming lead
    instrumentPresets[65].name = "Sync Lead";
    instrumentPresets[65].patch.p_waveType = WAVE_SAW;
    instrumentPresets[65].patch.p_hardSync = true;
    instrumentPresets[65].patch.p_hardSyncRatio = 2.3f;
    instrumentPresets[65].patch.p_attack = 0.005f;
    instrumentPresets[65].patch.p_decay = 0.15f;
    instrumentPresets[65].patch.p_sustain = 0.7f;
    instrumentPresets[65].patch.p_release = 0.15f;
    instrumentPresets[65].patch.p_filterCutoff = 0.8f;
    instrumentPresets[65].patch.p_monoMode = true;
    instrumentPresets[65].patch.p_glideTime = 0.04f;
    // ========================================================================
    // PERCUSSION PRESETS — Tier 1 (66-70)
    // ========================================================================

    // 66: Ride Cymbal — 6-osc metallic, higher ratios for shimmer, long decay
    instrumentPresets[66].name = "Ride Cymbal";
    instrumentPresets[66].patch.p_waveType = WAVE_SQUARE;
    instrumentPresets[66].patch.p_osc2Ratio = 2.12f;
    instrumentPresets[66].patch.p_osc2Level = 0.85f;
    instrumentPresets[66].patch.p_osc3Ratio = 3.05f;
    instrumentPresets[66].patch.p_osc3Level = 0.8f;
    instrumentPresets[66].patch.p_osc4Ratio = 4.18f;
    instrumentPresets[66].patch.p_osc4Level = 0.7f;
    instrumentPresets[66].patch.p_osc5Ratio = 5.41f;
    instrumentPresets[66].patch.p_osc5Level = 0.6f;
    instrumentPresets[66].patch.p_osc6Ratio = 6.93f;
    instrumentPresets[66].patch.p_osc6Level = 0.5f;
    instrumentPresets[66].patch.p_noiseMix = 0.25f;
    instrumentPresets[66].patch.p_noiseTone = 0.8f;
    instrumentPresets[66].patch.p_noiseHP = 0.25f;
    instrumentPresets[66].patch.p_noiseDecay = 0.4f;
    instrumentPresets[66].patch.p_attack = 0.001f;
    instrumentPresets[66].patch.p_decay = 2.5f;
    instrumentPresets[66].patch.p_sustain = 0.0f;
    instrumentPresets[66].patch.p_release = 1.0f;
    instrumentPresets[66].patch.p_expDecay = true;
    instrumentPresets[66].patch.p_filterType = 1; // SVF HP — cut low-end rumble
    instrumentPresets[66].patch.p_filterCutoff = 0.35f;
    instrumentPresets[66].patch.p_drive = 0.1f;
    instrumentPresets[66].patch.p_volume = 0.4f;
    instrumentPresets[66].patch.p_phaseReset = true;
    instrumentPresets[66].patch.p_useTriggerFreq = true;
    instrumentPresets[66].patch.p_triggerFreq = 540.0f;

    // 67: Brush Snare — noise-heavy, soft attack, gentle swish
    instrumentPresets[67].name = "Brush Snare";
    instrumentPresets[67].patch.p_waveType = WAVE_NOISE;
    instrumentPresets[67].patch.p_noiseMix = 0.85f;
    instrumentPresets[67].patch.p_noiseTone = 0.4f;
    instrumentPresets[67].patch.p_noiseHP = 0.1f;
    instrumentPresets[67].patch.p_attack = 0.008f;
    instrumentPresets[67].patch.p_decay = 0.25f;
    instrumentPresets[67].patch.p_sustain = 0.0f;
    instrumentPresets[67].patch.p_release = 0.15f;
    instrumentPresets[67].patch.p_expDecay = true;
    instrumentPresets[67].patch.p_filterType = 2; // SVF BP
    instrumentPresets[67].patch.p_filterCutoff = 0.45f;
    instrumentPresets[67].patch.p_filterResonance = 0.15f;
    instrumentPresets[67].patch.p_volume = 0.4f;
    instrumentPresets[67].patch.p_phaseReset = true;
    instrumentPresets[67].patch.p_useTriggerFreq = true;
    instrumentPresets[67].patch.p_triggerFreq = 200.0f;

    // 68: Crash Cymbal — bright ride variant, faster burst, longer decay
    instrumentPresets[68].name = "Crash";
    instrumentPresets[68].patch.p_waveType = WAVE_SQUARE;
    instrumentPresets[68].patch.p_osc2Ratio = 1.34f;
    instrumentPresets[68].patch.p_osc2Level = 1.0f;
    instrumentPresets[68].patch.p_osc3Ratio = 1.87f;
    instrumentPresets[68].patch.p_osc3Level = 0.9f;
    instrumentPresets[68].patch.p_osc4Ratio = 2.42f;
    instrumentPresets[68].patch.p_osc4Level = 0.8f;
    instrumentPresets[68].patch.p_osc5Ratio = 3.15f;
    instrumentPresets[68].patch.p_osc5Level = 0.7f;
    instrumentPresets[68].patch.p_osc6Ratio = 3.89f;
    instrumentPresets[68].patch.p_osc6Level = 0.6f;
    instrumentPresets[68].patch.p_noiseMix = 0.3f;
    instrumentPresets[68].patch.p_noiseTone = 0.7f;
    instrumentPresets[68].patch.p_noiseDecay = 0.02f;
    instrumentPresets[68].patch.p_attack = 0.001f;
    instrumentPresets[68].patch.p_decay = 2.0f;
    instrumentPresets[68].patch.p_sustain = 0.0f;
    instrumentPresets[68].patch.p_release = 0.8f;
    instrumentPresets[68].patch.p_expDecay = true;
    instrumentPresets[68].patch.p_filterType = 0; // SVF LP
    instrumentPresets[68].patch.p_filterCutoff = 0.75f;
    instrumentPresets[68].patch.p_drive = 0.1f;
    instrumentPresets[68].patch.p_volume = 0.35f;
    instrumentPresets[68].patch.p_phaseReset = true;
    instrumentPresets[68].patch.p_useTriggerFreq = true;
    instrumentPresets[68].patch.p_triggerFreq = 380.0f;

    // 69: Shaker — tight noise burst, bandpass filtered
    instrumentPresets[69].name = "Shaker";
    instrumentPresets[69].patch.p_waveType = WAVE_NOISE;
    instrumentPresets[69].patch.p_noiseMix = 1.0f;
    instrumentPresets[69].patch.p_noiseTone = 0.6f;
    instrumentPresets[69].patch.p_noiseHP = 0.2f;
    instrumentPresets[69].patch.p_attack = 0.001f;
    instrumentPresets[69].patch.p_decay = 0.03f;
    instrumentPresets[69].patch.p_sustain = 0.0f;
    instrumentPresets[69].patch.p_release = 0.01f;
    instrumentPresets[69].patch.p_expDecay = true;
    instrumentPresets[69].patch.p_filterType = 2; // SVF BP
    instrumentPresets[69].patch.p_filterCutoff = 0.55f;
    instrumentPresets[69].patch.p_filterResonance = 0.3f;
    instrumentPresets[69].patch.p_retriggerCount = 2;
    instrumentPresets[69].patch.p_retriggerSpread = 0.015f;
    instrumentPresets[69].patch.p_retriggerOverlap = false;
    instrumentPresets[69].patch.p_volume = 0.35f;
    instrumentPresets[69].patch.p_phaseReset = true;
    instrumentPresets[69].patch.p_useTriggerFreq = true;
    instrumentPresets[69].patch.p_triggerFreq = 800.0f;

    // 70: Tambourine — noise burst + metallic jingle (3 inharmonic oscs)
    instrumentPresets[70].name = "Tambourine";
    instrumentPresets[70].patch.p_waveType = WAVE_SQUARE;
    instrumentPresets[70].patch.p_noiseMix = 0.4f;
    instrumentPresets[70].patch.p_noiseTone = 0.7f;
    instrumentPresets[70].patch.p_noiseDecay = 0.02f;
    instrumentPresets[70].patch.p_osc2Ratio = 2.71f;
    instrumentPresets[70].patch.p_osc2Level = 0.8f;
    instrumentPresets[70].patch.p_osc3Ratio = 4.13f;
    instrumentPresets[70].patch.p_osc3Level = 0.6f;
    instrumentPresets[70].patch.p_attack = 0.001f;
    instrumentPresets[70].patch.p_decay = 0.2f;
    instrumentPresets[70].patch.p_sustain = 0.0f;
    instrumentPresets[70].patch.p_release = 0.1f;
    instrumentPresets[70].patch.p_expDecay = true;
    instrumentPresets[70].patch.p_filterType = 4; // One-pole HP
    instrumentPresets[70].patch.p_filterCutoff = 0.45f;
    instrumentPresets[70].patch.p_volume = 0.3f;
    instrumentPresets[70].patch.p_phaseReset = true;
    instrumentPresets[70].patch.p_useTriggerFreq = true;
    instrumentPresets[70].patch.p_triggerFreq = 600.0f;

    // ========================================================================
    // PERCUSSION PRESETS — Tier 2 (71-80)
    // ========================================================================

    // 71: Bongo Hi — membrane, high pitch, sharp attack
    instrumentPresets[71].name = "Bongo Hi";
    instrumentPresets[71].patch.p_waveType = WAVE_MEMBRANE;
    instrumentPresets[71].patch.p_envelopeEnabled = true;
    instrumentPresets[71].patch.p_filterEnabled = true;
    instrumentPresets[71].patch.p_filterCutoff = 1.0f;
    instrumentPresets[71].patch.p_filterResonance = 0.0f;
    instrumentPresets[71].patch.p_membranePreset = MEMBRANE_BONGO;
    instrumentPresets[71].patch.p_membraneDamping = 0.35f;
    instrumentPresets[71].patch.p_membraneStrike = 0.6f;
    instrumentPresets[71].patch.p_membraneBend = 0.1f;
    instrumentPresets[71].patch.p_membraneBendDecay = 0.05f;
    instrumentPresets[71].patch.p_attack = 0.001f;
    instrumentPresets[71].patch.p_decay = 0.3f;
    instrumentPresets[71].patch.p_sustain = 0.0f;
    instrumentPresets[71].patch.p_release = 0.1f;
    instrumentPresets[71].patch.p_volume = 0.5f;
    instrumentPresets[71].patch.p_useTriggerFreq = true;
    instrumentPresets[71].patch.p_triggerFreq = 450.0f;

    // 72: Bongo Lo — membrane, lower pitch
    instrumentPresets[72].name = "Bongo Lo";
    instrumentPresets[72].patch.p_waveType = WAVE_MEMBRANE;
    instrumentPresets[72].patch.p_envelopeEnabled = true;
    instrumentPresets[72].patch.p_filterEnabled = true;
    instrumentPresets[72].patch.p_filterCutoff = 1.0f;
    instrumentPresets[72].patch.p_filterResonance = 0.0f;
    instrumentPresets[72].patch.p_membranePreset = MEMBRANE_BONGO;
    instrumentPresets[72].patch.p_membraneDamping = 0.25f;
    instrumentPresets[72].patch.p_membraneStrike = 0.5f;
    instrumentPresets[72].patch.p_membraneBend = 0.15f;
    instrumentPresets[72].patch.p_membraneBendDecay = 0.08f;
    instrumentPresets[72].patch.p_attack = 0.001f;
    instrumentPresets[72].patch.p_decay = 0.4f;
    instrumentPresets[72].patch.p_sustain = 0.0f;
    instrumentPresets[72].patch.p_release = 0.15f;
    instrumentPresets[72].patch.p_volume = 0.5f;
    instrumentPresets[72].patch.p_useTriggerFreq = true;
    instrumentPresets[72].patch.p_triggerFreq = 280.0f;

    // 73: Conga Hi — membrane, slap sound
    instrumentPresets[73].name = "Conga Hi";
    instrumentPresets[73].patch.p_waveType = WAVE_MEMBRANE;
    instrumentPresets[73].patch.p_envelopeEnabled = true;
    instrumentPresets[73].patch.p_filterEnabled = true;
    instrumentPresets[73].patch.p_filterCutoff = 1.0f;
    instrumentPresets[73].patch.p_filterResonance = 0.0f;
    instrumentPresets[73].patch.p_membranePreset = MEMBRANE_CONGA;
    instrumentPresets[73].patch.p_membraneDamping = 0.3f;
    instrumentPresets[73].patch.p_membraneStrike = 0.7f;
    instrumentPresets[73].patch.p_membraneBend = 0.08f;
    instrumentPresets[73].patch.p_membraneBendDecay = 0.04f;
    instrumentPresets[73].patch.p_attack = 0.001f;
    instrumentPresets[73].patch.p_decay = 0.35f;
    instrumentPresets[73].patch.p_sustain = 0.0f;
    instrumentPresets[73].patch.p_release = 0.12f;
    instrumentPresets[73].patch.p_volume = 0.5f;
    instrumentPresets[73].patch.p_useTriggerFreq = true;
    instrumentPresets[73].patch.p_triggerFreq = 320.0f;

    // 74: Conga Lo — membrane, open tone
    instrumentPresets[74].name = "Conga Lo";
    instrumentPresets[74].patch.p_waveType = WAVE_MEMBRANE;
    instrumentPresets[74].patch.p_envelopeEnabled = true;
    instrumentPresets[74].patch.p_filterEnabled = true;
    instrumentPresets[74].patch.p_filterCutoff = 1.0f;
    instrumentPresets[74].patch.p_filterResonance = 0.0f;
    instrumentPresets[74].patch.p_membranePreset = MEMBRANE_CONGA;
    instrumentPresets[74].patch.p_membraneDamping = 0.2f;
    instrumentPresets[74].patch.p_membraneStrike = 0.5f;
    instrumentPresets[74].patch.p_membraneBend = 0.12f;
    instrumentPresets[74].patch.p_membraneBendDecay = 0.06f;
    instrumentPresets[74].patch.p_attack = 0.001f;
    instrumentPresets[74].patch.p_decay = 0.5f;
    instrumentPresets[74].patch.p_sustain = 0.0f;
    instrumentPresets[74].patch.p_release = 0.2f;
    instrumentPresets[74].patch.p_volume = 0.5f;
    instrumentPresets[74].patch.p_useTriggerFreq = true;
    instrumentPresets[74].patch.p_triggerFreq = 200.0f;

    // 75: Timbales — short metallic ring, FM high ratio
    instrumentPresets[75].name = "Timbales";
    instrumentPresets[75].patch.p_waveType = WAVE_FM;
    instrumentPresets[75].patch.p_fmModRatio = 5.5f;
    instrumentPresets[75].patch.p_fmModIndex = 9.42f;
    instrumentPresets[75].patch.p_pitchEnvAmount = 6.0f;
    instrumentPresets[75].patch.p_pitchEnvDecay = 0.01f;
    instrumentPresets[75].patch.p_pitchEnvCurve = -0.5f;
    instrumentPresets[75].patch.p_attack = 0.001f;
    instrumentPresets[75].patch.p_decay = 0.25f;
    instrumentPresets[75].patch.p_sustain = 0.0f;
    instrumentPresets[75].patch.p_release = 0.08f;
    instrumentPresets[75].patch.p_expDecay = true;
    instrumentPresets[75].patch.p_filterCutoff = 0.7f;
    instrumentPresets[75].patch.p_volume = 0.45f;
    instrumentPresets[75].patch.p_phaseReset = true;
    instrumentPresets[75].patch.p_useTriggerFreq = true;
    instrumentPresets[75].patch.p_triggerFreq = 380.0f;

    // 76: Woodblock — short filtered click, high pitch
    instrumentPresets[76].name = "Woodblock";
    instrumentPresets[76].patch.p_waveType = WAVE_SQUARE;
    instrumentPresets[76].patch.p_pitchEnvAmount = 12.0f;
    instrumentPresets[76].patch.p_pitchEnvDecay = 0.008f;
    instrumentPresets[76].patch.p_pitchEnvCurve = -0.8f;
    instrumentPresets[76].patch.p_attack = 0.001f;
    instrumentPresets[76].patch.p_decay = 0.03f;
    instrumentPresets[76].patch.p_sustain = 0.0f;
    instrumentPresets[76].patch.p_release = 0.01f;
    instrumentPresets[76].patch.p_expDecay = true;
    instrumentPresets[76].patch.p_filterType = 0; // SVF LP
    instrumentPresets[76].patch.p_filterCutoff = 0.6f;
    instrumentPresets[76].patch.p_volume = 0.4f;
    instrumentPresets[76].patch.p_phaseReset = true;
    instrumentPresets[76].patch.p_useTriggerFreq = true;
    instrumentPresets[76].patch.p_triggerFreq = 900.0f;

    // 77: Agogo Hi — pure FM bell, high pitch
    instrumentPresets[77].name = "Agogo Hi";
    instrumentPresets[77].patch.p_waveType = WAVE_FM;
    instrumentPresets[77].patch.p_fmModRatio = 3.0f;
    instrumentPresets[77].patch.p_fmModIndex = 6.28f;
    instrumentPresets[77].patch.p_attack = 0.001f;
    instrumentPresets[77].patch.p_decay = 0.4f;
    instrumentPresets[77].patch.p_sustain = 0.0f;
    instrumentPresets[77].patch.p_release = 0.15f;
    instrumentPresets[77].patch.p_expDecay = true;
    instrumentPresets[77].patch.p_filterCutoff = 0.65f;
    instrumentPresets[77].patch.p_volume = 0.35f;
    instrumentPresets[77].patch.p_phaseReset = true;
    instrumentPresets[77].patch.p_useTriggerFreq = true;
    instrumentPresets[77].patch.p_triggerFreq = 700.0f;

    // 78: Agogo Lo — FM bell, lower pitch
    instrumentPresets[78].name = "Agogo Lo";
    instrumentPresets[78].patch.p_waveType = WAVE_FM;
    instrumentPresets[78].patch.p_fmModRatio = 3.0f;
    instrumentPresets[78].patch.p_fmModIndex = 6.28f;
    instrumentPresets[78].patch.p_attack = 0.001f;
    instrumentPresets[78].patch.p_decay = 0.4f;
    instrumentPresets[78].patch.p_sustain = 0.0f;
    instrumentPresets[78].patch.p_release = 0.15f;
    instrumentPresets[78].patch.p_expDecay = true;
    instrumentPresets[78].patch.p_filterCutoff = 0.55f;
    instrumentPresets[78].patch.p_volume = 0.35f;
    instrumentPresets[78].patch.p_phaseReset = true;
    instrumentPresets[78].patch.p_useTriggerFreq = true;
    instrumentPresets[78].patch.p_triggerFreq = 470.0f;

    // 79: Triangle Perc — high sine, long decay, slight pitch drop
    instrumentPresets[79].name = "Triangle";
    instrumentPresets[79].patch.p_waveType = WAVE_TRIANGLE;
    instrumentPresets[79].patch.p_pitchEnvAmount = -1.0f;
    instrumentPresets[79].patch.p_pitchEnvDecay = 0.5f;
    instrumentPresets[79].patch.p_attack = 0.001f;
    instrumentPresets[79].patch.p_decay = 1.5f;
    instrumentPresets[79].patch.p_sustain = 0.0f;
    instrumentPresets[79].patch.p_release = 0.5f;
    instrumentPresets[79].patch.p_expDecay = true;
    instrumentPresets[79].patch.p_volume = 0.3f;
    instrumentPresets[79].patch.p_phaseReset = true;
    instrumentPresets[79].patch.p_useTriggerFreq = true;
    instrumentPresets[79].patch.p_triggerFreq = 1500.0f;

    // 80: Finger Snap — single noise burst, bandpass, very short
    instrumentPresets[80].name = "Finger Snap";
    instrumentPresets[80].patch.p_waveType = WAVE_NOISE;
    instrumentPresets[80].patch.p_noiseMix = 1.0f;
    instrumentPresets[80].patch.p_noiseTone = 0.5f;
    instrumentPresets[80].patch.p_noiseHP = 0.15f;
    instrumentPresets[80].patch.p_attack = 0.001f;
    instrumentPresets[80].patch.p_decay = 0.04f;
    instrumentPresets[80].patch.p_sustain = 0.0f;
    instrumentPresets[80].patch.p_release = 0.02f;
    instrumentPresets[80].patch.p_expDecay = true;
    instrumentPresets[80].patch.p_filterType = 2; // SVF BP
    instrumentPresets[80].patch.p_filterCutoff = 0.5f;
    instrumentPresets[80].patch.p_filterResonance = 0.25f;
    instrumentPresets[80].patch.p_volume = 0.4f;
    instrumentPresets[80].patch.p_phaseReset = true;
    instrumentPresets[80].patch.p_useTriggerFreq = true;
    instrumentPresets[80].patch.p_triggerFreq = 1200.0f;

    // ========================================================================
    // WEST COAST / WAVEFOLD / SYNC / RING MOD SHOWCASE (81-86)
    // ========================================================================

    // 81: West Coast Bass — Buchla-style bongo bass, triangle through wavefolder
    instrumentPresets[81].name = "WC Bass";
    instrumentPresets[81].patch.p_waveType = WAVE_TRIANGLE;
    instrumentPresets[81].patch.p_wavefoldAmount = 0.4f;
    instrumentPresets[81].patch.p_attack = 0.003f;
    instrumentPresets[81].patch.p_decay = 0.35f;
    instrumentPresets[81].patch.p_sustain = 0.3f;
    instrumentPresets[81].patch.p_release = 0.12f;
    instrumentPresets[81].patch.p_filterCutoff = 0.35f;
    instrumentPresets[81].patch.p_filterResonance = 0.25f;
    instrumentPresets[81].patch.p_filterEnvAmt = 0.4f;
    instrumentPresets[81].patch.p_filterEnvAttack = 0.005f;
    instrumentPresets[81].patch.p_filterEnvDecay = 0.25f;
    instrumentPresets[81].patch.p_monoMode = true;
    instrumentPresets[81].patch.p_glideTime = 0.03f;
    instrumentPresets[81].patch.p_volume = 0.6f;

    // 82: Evolving Pad — sine through wavefolder with filter LFO, breathing texture
    instrumentPresets[82].name = "WC Pad";
    instrumentPresets[82].patch.p_waveType = WAVE_TRIANGLE;
    instrumentPresets[82].patch.p_wavefoldAmount = 0.35f;
    instrumentPresets[82].patch.p_attack = 0.4f;
    instrumentPresets[82].patch.p_decay = 0.5f;
    instrumentPresets[82].patch.p_sustain = 0.7f;
    instrumentPresets[82].patch.p_release = 0.6f;
    instrumentPresets[82].patch.p_filterCutoff = 0.4f;
    instrumentPresets[82].patch.p_filterResonance = 0.15f;
    instrumentPresets[82].patch.p_filterLfoRate = 0.3f;
    instrumentPresets[82].patch.p_filterLfoDepth = 0.3f;
    instrumentPresets[82].patch.p_volume = 0.5f;

    // 83: Metallic Lead — saw + heavy wavefold + high resonance, screaming harmonics
    instrumentPresets[83].name = "WC Lead";
    instrumentPresets[83].patch.p_waveType = WAVE_SAW;
    instrumentPresets[83].patch.p_wavefoldAmount = 0.7f;
    instrumentPresets[83].patch.p_attack = 0.005f;
    instrumentPresets[83].patch.p_decay = 0.15f;
    instrumentPresets[83].patch.p_sustain = 0.65f;
    instrumentPresets[83].patch.p_release = 0.15f;
    instrumentPresets[83].patch.p_filterCutoff = 0.6f;
    instrumentPresets[83].patch.p_filterResonance = 0.4f;
    instrumentPresets[83].patch.p_filterEnvAmt = 0.3f;
    instrumentPresets[83].patch.p_filterEnvAttack = 0.01f;
    instrumentPresets[83].patch.p_filterEnvDecay = 0.3f;
    instrumentPresets[83].patch.p_monoMode = true;
    instrumentPresets[83].patch.p_glideTime = 0.04f;
    instrumentPresets[83].patch.p_volume = 0.45f;

    // 84: Fold Perc — sine + pitch env + wavefold, complex percussive transient
    instrumentPresets[84].name = "WC Perc";
    instrumentPresets[84].patch.p_waveType = WAVE_TRIANGLE;
    instrumentPresets[84].patch.p_wavefoldAmount = 0.6f;
    instrumentPresets[84].patch.p_pitchEnvAmount = 12.0f;
    instrumentPresets[84].patch.p_pitchEnvDecay = 0.04f;
    instrumentPresets[84].patch.p_attack = 0.001f;
    instrumentPresets[84].patch.p_decay = 0.25f;
    instrumentPresets[84].patch.p_sustain = 0.0f;
    instrumentPresets[84].patch.p_release = 0.08f;
    instrumentPresets[84].patch.p_expDecay = true;
    instrumentPresets[84].patch.p_filterCutoff = 0.65f;
    instrumentPresets[84].patch.p_filterResonance = 0.15f;
    instrumentPresets[84].patch.p_volume = 0.5f;
    instrumentPresets[84].patch.p_phaseReset = true;

    // 85: Sync Brass — hard sync saw with filter env sweep, punchy brass stab
    instrumentPresets[85].name = "Sync Brass";
    instrumentPresets[85].patch.p_waveType = WAVE_SAW;
    instrumentPresets[85].patch.p_hardSync = true;
    instrumentPresets[85].patch.p_hardSyncRatio = 1.5f;
    instrumentPresets[85].patch.p_attack = 0.01f;
    instrumentPresets[85].patch.p_decay = 0.2f;
    instrumentPresets[85].patch.p_sustain = 0.5f;
    instrumentPresets[85].patch.p_release = 0.12f;
    instrumentPresets[85].patch.p_filterCutoff = 0.3f;
    instrumentPresets[85].patch.p_filterResonance = 0.2f;
    instrumentPresets[85].patch.p_filterEnvAmt = 0.5f;
    instrumentPresets[85].patch.p_filterEnvAttack = 0.01f;
    instrumentPresets[85].patch.p_filterEnvDecay = 0.15f;
    instrumentPresets[85].patch.p_volume = 0.5f;

    // 86: Ring Drone — ring mod with slow beating, dark metallic texture
    instrumentPresets[86].name = "Ring Drone";
    instrumentPresets[86].patch.p_waveType = WAVE_SAW;
    instrumentPresets[86].patch.p_ringMod = true;
    instrumentPresets[86].patch.p_ringModFreq = 1.01f;  // Near-unison = slow beating
    instrumentPresets[86].patch.p_attack = 0.3f;
    instrumentPresets[86].patch.p_decay = 0.5f;
    instrumentPresets[86].patch.p_sustain = 0.8f;
    instrumentPresets[86].patch.p_release = 0.5f;
    instrumentPresets[86].patch.p_filterCutoff = 0.35f;
    instrumentPresets[86].patch.p_filterResonance = 0.2f;
    instrumentPresets[86].patch.p_filterLfoRate = 0.15f;
    instrumentPresets[86].patch.p_filterLfoDepth = 0.2f;
    instrumentPresets[86].patch.p_volume = 0.45f;

    // ========================================================================
    // PHASE 0: FREE PRESETS — engines with zero presets (87-97)
    // ========================================================================

    // 87: Glockenspiel — bright, bell-like mallet
    instrumentPresets[87].name = "Glockenspiel";
    instrumentPresets[87].patch.p_waveType = WAVE_MALLET;
    instrumentPresets[87].patch.p_envelopeEnabled = true;
    instrumentPresets[87].patch.p_filterEnabled = true;
    instrumentPresets[87].patch.p_filterCutoff = 1.0f;
    instrumentPresets[87].patch.p_filterResonance = 0.0f;
    instrumentPresets[87].patch.p_malletPreset = MALLET_PRESET_GLOCKEN;
    instrumentPresets[87].patch.p_malletHardness = 0.8f;
    instrumentPresets[87].patch.p_malletStrikePos = 0.3f;
    instrumentPresets[87].patch.p_malletResonance = 0.8f;
    instrumentPresets[87].patch.p_malletDamp = 0.1f;
    instrumentPresets[87].patch.p_attack = 0.001f;
    instrumentPresets[87].patch.p_decay = 1.2f;
    instrumentPresets[87].patch.p_sustain = 0.0f;
    instrumentPresets[87].patch.p_release = 0.4f;
    instrumentPresets[87].patch.p_expDecay = true;
    instrumentPresets[87].patch.p_volume = 0.4f;

    // 88: Xylophone — bright, sharp, woody
    instrumentPresets[88].name = "Xylophone";
    instrumentPresets[88].patch.p_waveType = WAVE_MALLET;
    instrumentPresets[88].patch.p_envelopeEnabled = true;
    instrumentPresets[88].patch.p_filterEnabled = true;
    instrumentPresets[88].patch.p_filterCutoff = 1.0f;
    instrumentPresets[88].patch.p_filterResonance = 0.0f;
    instrumentPresets[88].patch.p_malletPreset = MALLET_PRESET_XYLOPHONE;
    instrumentPresets[88].patch.p_malletHardness = 0.7f;
    instrumentPresets[88].patch.p_malletStrikePos = 0.2f;
    instrumentPresets[88].patch.p_malletResonance = 0.6f;
    instrumentPresets[88].patch.p_malletDamp = 0.2f;
    instrumentPresets[88].patch.p_attack = 0.001f;
    instrumentPresets[88].patch.p_decay = 0.6f;
    instrumentPresets[88].patch.p_sustain = 0.0f;
    instrumentPresets[88].patch.p_release = 0.2f;
    instrumentPresets[88].patch.p_expDecay = true;
    instrumentPresets[88].patch.p_volume = 0.45f;

    // 89: Tubular Bells — deep, resonant church bells
    instrumentPresets[89].name = "Tubular Bells";
    instrumentPresets[89].patch.p_waveType = WAVE_MALLET;
    instrumentPresets[89].patch.p_envelopeEnabled = true;
    instrumentPresets[89].patch.p_filterEnabled = true;
    instrumentPresets[89].patch.p_filterCutoff = 1.0f;
    instrumentPresets[89].patch.p_filterResonance = 0.0f;
    instrumentPresets[89].patch.p_malletPreset = MALLET_PRESET_TUBULAR;
    instrumentPresets[89].patch.p_malletHardness = 0.6f;
    instrumentPresets[89].patch.p_malletStrikePos = 0.15f;
    instrumentPresets[89].patch.p_malletResonance = 0.9f;
    instrumentPresets[89].patch.p_malletDamp = 0.05f;
    instrumentPresets[89].patch.p_attack = 0.001f;
    instrumentPresets[89].patch.p_decay = 2.5f;
    instrumentPresets[89].patch.p_sustain = 0.0f;
    instrumentPresets[89].patch.p_release = 0.8f;
    instrumentPresets[89].patch.p_expDecay = true;
    instrumentPresets[89].patch.p_volume = 0.4f;

    // 90: Choir Pad — additive choir, warm sustained voices
    instrumentPresets[90].name = "Choir Pad";
    instrumentPresets[90].patch.p_waveType = WAVE_ADDITIVE;
    instrumentPresets[90].patch.p_additivePreset = ADDITIVE_PRESET_CHOIR;
    instrumentPresets[90].patch.p_additiveBrightness = 0.5f;
    instrumentPresets[90].patch.p_additiveShimmer = 0.15f;
    instrumentPresets[90].patch.p_attack = 0.3f;
    instrumentPresets[90].patch.p_decay = 0.5f;
    instrumentPresets[90].patch.p_sustain = 0.7f;
    instrumentPresets[90].patch.p_release = 0.5f;
    instrumentPresets[90].patch.p_filterCutoff = 0.55f;
    instrumentPresets[90].patch.p_vibratoRate = 5.5f;
    instrumentPresets[90].patch.p_vibratoDepth = 0.003f;
    instrumentPresets[90].patch.p_volume = 0.45f;

    // 91: Brass Stab — additive brass, short punchy
    instrumentPresets[91].name = "Brass Stab";
    instrumentPresets[91].patch.p_waveType = WAVE_ADDITIVE;
    instrumentPresets[91].patch.p_additivePreset = ADDITIVE_PRESET_BRASS;
    instrumentPresets[91].patch.p_additiveBrightness = 0.7f;
    instrumentPresets[91].patch.p_attack = 0.02f;
    instrumentPresets[91].patch.p_decay = 0.2f;
    instrumentPresets[91].patch.p_sustain = 0.5f;
    instrumentPresets[91].patch.p_release = 0.1f;
    instrumentPresets[91].patch.p_filterCutoff = 0.6f;
    instrumentPresets[91].patch.p_filterEnvAmt = 0.3f;
    instrumentPresets[91].patch.p_filterEnvAttack = 0.01f;
    instrumentPresets[91].patch.p_filterEnvDecay = 0.15f;
    instrumentPresets[91].patch.p_volume = 0.5f;

    // 92: Add Strings — additive string ensemble (different from saw-based Strings)
    instrumentPresets[92].name = "Add Strings";
    instrumentPresets[92].patch.p_waveType = WAVE_ADDITIVE;
    instrumentPresets[92].patch.p_additivePreset = ADDITIVE_PRESET_STRINGS;
    instrumentPresets[92].patch.p_additiveBrightness = 0.55f;
    instrumentPresets[92].patch.p_additiveShimmer = 0.1f;
    instrumentPresets[92].patch.p_attack = 0.15f;
    instrumentPresets[92].patch.p_decay = 0.3f;
    instrumentPresets[92].patch.p_sustain = 0.7f;
    instrumentPresets[92].patch.p_release = 0.3f;
    instrumentPresets[92].patch.p_filterCutoff = 0.5f;
    instrumentPresets[92].patch.p_vibratoRate = 5.0f;
    instrumentPresets[92].patch.p_vibratoDepth = 0.004f;
    instrumentPresets[92].patch.p_volume = 0.45f;

    // 93: Add Bell — additive bell, inharmonic partials, long ring
    instrumentPresets[93].name = "Add Bell";
    instrumentPresets[93].patch.p_waveType = WAVE_ADDITIVE;
    instrumentPresets[93].patch.p_additivePreset = ADDITIVE_PRESET_BELL;
    instrumentPresets[93].patch.p_additiveBrightness = 0.65f;
    instrumentPresets[93].patch.p_additiveInharmonicity = 0.3f;
    instrumentPresets[93].patch.p_attack = 0.001f;
    instrumentPresets[93].patch.p_decay = 2.0f;
    instrumentPresets[93].patch.p_sustain = 0.0f;
    instrumentPresets[93].patch.p_release = 0.6f;
    instrumentPresets[93].patch.p_expDecay = true;
    instrumentPresets[93].patch.p_volume = 0.4f;

    // 94: PD Bass — CZ-style digital bass, resonant saw
    instrumentPresets[94].name = "PD Bass";
    instrumentPresets[94].patch.p_waveType = WAVE_PD;
    instrumentPresets[94].patch.p_pdWaveType = PD_WAVE_RESO1;
    instrumentPresets[94].patch.p_attack = 0.005f;
    instrumentPresets[94].patch.p_decay = 0.25f;
    instrumentPresets[94].patch.p_sustain = 0.4f;
    instrumentPresets[94].patch.p_release = 0.1f;
    instrumentPresets[94].patch.p_filterCutoff = 0.4f;
    instrumentPresets[94].patch.p_filterResonance = 0.2f;
    instrumentPresets[94].patch.p_filterEnvAmt = 0.35f;
    instrumentPresets[94].patch.p_filterEnvAttack = 0.005f;
    instrumentPresets[94].patch.p_filterEnvDecay = 0.2f;
    instrumentPresets[94].patch.p_monoMode = true;
    instrumentPresets[94].patch.p_volume = 0.55f;

    // 95: PD Lead — CZ-101 screaming resonant lead
    instrumentPresets[95].name = "PD Lead";
    instrumentPresets[95].patch.p_waveType = WAVE_PD;
    instrumentPresets[95].patch.p_pdWaveType = PD_WAVE_RESO3;
    instrumentPresets[95].patch.p_attack = 0.01f;
    instrumentPresets[95].patch.p_decay = 0.15f;
    instrumentPresets[95].patch.p_sustain = 0.6f;
    instrumentPresets[95].patch.p_release = 0.15f;
    instrumentPresets[95].patch.p_filterCutoff = 0.65f;
    instrumentPresets[95].patch.p_filterResonance = 0.3f;
    instrumentPresets[95].patch.p_monoMode = true;
    instrumentPresets[95].patch.p_glideTime = 0.04f;
    instrumentPresets[95].patch.p_volume = 0.5f;

    // 96: Tabla — melodic tabla, pitched membrane
    instrumentPresets[96].name = "Tabla";
    instrumentPresets[96].patch.p_waveType = WAVE_MEMBRANE;
    instrumentPresets[96].patch.p_envelopeEnabled = true;
    instrumentPresets[96].patch.p_filterEnabled = true;
    instrumentPresets[96].patch.p_filterCutoff = 1.0f;
    instrumentPresets[96].patch.p_filterResonance = 0.0f;
    instrumentPresets[96].patch.p_membranePreset = MEMBRANE_TABLA;
    instrumentPresets[96].patch.p_membraneDamping = 0.2f;
    instrumentPresets[96].patch.p_membraneStrike = 0.4f;
    instrumentPresets[96].patch.p_membraneBend = 0.2f;
    instrumentPresets[96].patch.p_membraneBendDecay = 0.1f;
    instrumentPresets[96].patch.p_attack = 0.001f;
    instrumentPresets[96].patch.p_decay = 0.5f;
    instrumentPresets[96].patch.p_sustain = 0.0f;
    instrumentPresets[96].patch.p_release = 0.2f;
    instrumentPresets[96].patch.p_volume = 0.5f;

    // 97: Bird Song — chirpy bird vocalization for game ambience
    instrumentPresets[97].name = "Bird Song";
    instrumentPresets[97].patch.p_waveType = WAVE_BIRD;
    instrumentPresets[97].patch.p_envelopeEnabled = true;
    instrumentPresets[97].patch.p_filterEnabled = true;
    instrumentPresets[97].patch.p_filterCutoff = 1.0f;
    instrumentPresets[97].patch.p_filterResonance = 0.0f;
    instrumentPresets[97].patch.p_birdType = BIRD_WARBLE;
    instrumentPresets[97].patch.p_birdChirpRange = 1.2f;
    instrumentPresets[97].patch.p_birdTrillRate = 0.3f;
    instrumentPresets[97].patch.p_birdTrillDepth = 0.4f;
    instrumentPresets[97].patch.p_birdAmRate = 0.2f;
    instrumentPresets[97].patch.p_birdAmDepth = 0.3f;
    instrumentPresets[97].patch.p_birdHarmonics = 0.25f;
    instrumentPresets[97].patch.p_attack = 0.01f;
    instrumentPresets[97].patch.p_decay = 0.5f;
    instrumentPresets[97].patch.p_sustain = 0.3f;
    instrumentPresets[97].patch.p_release = 0.2f;
    instrumentPresets[97].patch.p_volume = 0.35f;

    // 98: Guiro — rapid scraped retrigger, noise burst train
    instrumentPresets[98].name = "Guiro";
    instrumentPresets[98].patch.p_waveType = WAVE_NOISE;
    instrumentPresets[98].patch.p_noiseMix = 1.0f;
    instrumentPresets[98].patch.p_noiseTone = 0.55f;
    instrumentPresets[98].patch.p_noiseHP = 0.2f;
    instrumentPresets[98].patch.p_attack = 0.001f;
    instrumentPresets[98].patch.p_decay = 0.02f;
    instrumentPresets[98].patch.p_sustain = 0.0f;
    instrumentPresets[98].patch.p_release = 0.01f;
    instrumentPresets[98].patch.p_expDecay = true;
    instrumentPresets[98].patch.p_filterType = 2; // SVF BP
    instrumentPresets[98].patch.p_filterCutoff = 0.45f;
    instrumentPresets[98].patch.p_filterResonance = 0.2f;
    instrumentPresets[98].patch.p_retriggerCount = 10;
    instrumentPresets[98].patch.p_retriggerSpread = 0.012f;
    instrumentPresets[98].patch.p_retriggerOverlap = false; // envelope restart per scrape
    instrumentPresets[98].patch.p_retriggerCurve = 0.08f;   // slight acceleration (scrape speeds up)
    instrumentPresets[98].patch.p_volume = 0.35f;
    instrumentPresets[98].patch.p_phaseReset = true;
    instrumentPresets[98].patch.p_useTriggerFreq = true;
    instrumentPresets[98].patch.p_triggerFreq = 900.0f;

    // 99: Cross Stick — quieter rimshot, more wood, less ring
    instrumentPresets[99].name = "Cross Stick";
    instrumentPresets[99].patch.p_waveType = WAVE_FM;
    instrumentPresets[99].patch.p_fmModRatio = 1.0f;
    instrumentPresets[99].patch.p_fmModIndex = 0.0f;
    instrumentPresets[99].patch.p_attack = 0.0f;
    instrumentPresets[99].patch.p_decay = 0.02f;          // shorter than rimshot (0.03)
    instrumentPresets[99].patch.p_sustain = 0.0f;
    instrumentPresets[99].patch.p_release = 0.008f;
    instrumentPresets[99].patch.p_expDecay = true;
    instrumentPresets[99].patch.p_noiseMix = 0.3f;        // wood texture
    instrumentPresets[99].patch.p_noiseTone = 0.35f;      // dark noise
    instrumentPresets[99].patch.p_noiseDecay = 0.01f;
    instrumentPresets[99].patch.p_filterCutoff = 0.6f;    // darker than rimshot (1.0)
    instrumentPresets[99].patch.p_clickLevel = 0.3f;      // softer click than rimshot (0.5)
    instrumentPresets[99].patch.p_clickTime = 0.003f;
    instrumentPresets[99].patch.p_volume = 0.3f;
    instrumentPresets[99].patch.p_phaseReset = true;
    instrumentPresets[99].patch.p_useTriggerFreq = true;
    instrumentPresets[99].patch.p_triggerFreq = 1400.0f;  // lower than rimshot (1700)

    // 100: Surdo — deep Brazilian bass drum, long membrane decay
    instrumentPresets[100].name = "Surdo";
    instrumentPresets[100].patch.p_waveType = WAVE_MEMBRANE;
    instrumentPresets[100].patch.p_envelopeEnabled = true;
    instrumentPresets[100].patch.p_filterEnabled = true;
    instrumentPresets[100].patch.p_membranePreset = MEMBRANE_TOM;
    instrumentPresets[100].patch.p_membraneDamping = 0.15f;   // low damping = long sustain
    instrumentPresets[100].patch.p_membraneStrike = 0.3f;     // soft mallet
    instrumentPresets[100].patch.p_membraneBend = 0.15f;
    instrumentPresets[100].patch.p_membraneBendDecay = 0.08f;
    instrumentPresets[100].patch.p_attack = 0.002f;
    instrumentPresets[100].patch.p_decay = 1.2f;              // long boom
    instrumentPresets[100].patch.p_sustain = 0.0f;
    instrumentPresets[100].patch.p_release = 0.3f;
    instrumentPresets[100].patch.p_expDecay = true;
    instrumentPresets[100].patch.p_filterCutoff = 0.3f;       // dark, bassy
    instrumentPresets[100].patch.p_volume = 0.5f;
    instrumentPresets[100].patch.p_useTriggerFreq = true;
    instrumentPresets[100].patch.p_triggerFreq = 80.0f;       // very low

    // 101: Cabasa — shaker with metallic high-freq content
    instrumentPresets[101].name = "Cabasa";
    instrumentPresets[101].patch.p_waveType = WAVE_NOISE;
    instrumentPresets[101].patch.p_noiseMix = 0.85f;
    instrumentPresets[101].patch.p_noiseTone = 0.7f;          // bright noise
    instrumentPresets[101].patch.p_noiseHP = 0.3f;            // more HP than shaker
    instrumentPresets[101].patch.p_osc2Ratio = 7.5f;          // metallic partial
    instrumentPresets[101].patch.p_osc2Level = 0.15f;
    instrumentPresets[101].patch.p_attack = 0.001f;
    instrumentPresets[101].patch.p_decay = 0.045f;            // slightly longer than shaker
    instrumentPresets[101].patch.p_sustain = 0.0f;
    instrumentPresets[101].patch.p_release = 0.02f;
    instrumentPresets[101].patch.p_expDecay = true;
    instrumentPresets[101].patch.p_filterType = 4;            // one-pole HP
    instrumentPresets[101].patch.p_filterCutoff = 0.4f;
    instrumentPresets[101].patch.p_volume = 0.3f;
    instrumentPresets[101].patch.p_phaseReset = true;
    instrumentPresets[101].patch.p_useTriggerFreq = true;
    instrumentPresets[101].patch.p_triggerFreq = 1100.0f;

    // 102: Vinyl Crackle — sparse random noise pops, lo-fi texture
    instrumentPresets[102].name = "Vinyl Crackle";
    instrumentPresets[102].patch.p_waveType = WAVE_NOISE;
    instrumentPresets[102].patch.p_noiseMix = 1.0f;
    instrumentPresets[102].patch.p_noiseTone = 0.3f;          // dark crackle
    instrumentPresets[102].patch.p_noiseHP = 0.35f;           // remove low rumble
    instrumentPresets[102].patch.p_attack = 0.0f;
    instrumentPresets[102].patch.p_decay = 0.005f;            // ultra-short pop
    instrumentPresets[102].patch.p_sustain = 0.0f;
    instrumentPresets[102].patch.p_release = 0.003f;
    instrumentPresets[102].patch.p_expDecay = true;
    instrumentPresets[102].patch.p_filterType = 2;            // SVF BP
    instrumentPresets[102].patch.p_filterCutoff = 0.55f;
    instrumentPresets[102].patch.p_filterResonance = 0.15f;
    instrumentPresets[102].patch.p_volume = 0.15f;            // quiet — texture, not rhythm
    instrumentPresets[102].patch.p_phaseReset = true;

    // === 3-Operator FM Showcase ===

    // 103: FM Crystal — Stack algorithm, metallic shimmer
    instrumentPresets[103].name = "FM Crystal";
    instrumentPresets[103].patch.p_waveType = WAVE_FM;
    instrumentPresets[103].patch.p_fmModRatio = 3.0f;
    instrumentPresets[103].patch.p_fmModIndex = 9.42f;
    instrumentPresets[103].patch.p_fmFeedback = 0.0f;
    instrumentPresets[103].patch.p_fmMod2Ratio = 7.0f;
    instrumentPresets[103].patch.p_fmMod2Index = 12.57f;
    instrumentPresets[103].patch.p_fmAlgorithm = FM_ALG_STACK;
    instrumentPresets[103].patch.p_attack = 0.001f;
    instrumentPresets[103].patch.p_decay = 1.2f;
    instrumentPresets[103].patch.p_sustain = 0.0f;
    instrumentPresets[103].patch.p_release = 0.4f;
    instrumentPresets[103].patch.p_expDecay = true;
    instrumentPresets[103].patch.p_filterCutoff = 0.7f;
    instrumentPresets[103].patch.p_volume = 0.4f;

    // 104: FM Bright EP — Parallel algorithm, rich electric piano
    instrumentPresets[104].name = "FM Bright EP";
    instrumentPresets[104].patch.p_waveType = WAVE_FM;
    instrumentPresets[104].patch.p_fmModRatio = 1.0f;
    instrumentPresets[104].patch.p_fmModIndex = 11.31f;
    instrumentPresets[104].patch.p_fmFeedback = 0.1f;
    instrumentPresets[104].patch.p_fmMod2Ratio = 14.0f;
    instrumentPresets[104].patch.p_fmMod2Index = 5.03f;
    instrumentPresets[104].patch.p_fmAlgorithm = FM_ALG_PARALLEL;
    instrumentPresets[104].patch.p_attack = 0.001f;
    instrumentPresets[104].patch.p_decay = 0.8f;
    instrumentPresets[104].patch.p_sustain = 0.15f;
    instrumentPresets[104].patch.p_release = 0.3f;
    instrumentPresets[104].patch.p_expDecay = true;
    instrumentPresets[104].patch.p_tubeSaturation = true;
    instrumentPresets[104].patch.p_volume = 0.4f;

    // 105: FM Gong — Branch algorithm, inharmonic decay
    instrumentPresets[105].name = "FM Gong";
    instrumentPresets[105].patch.p_waveType = WAVE_FM;
    instrumentPresets[105].patch.p_fmModRatio = 1.41f;
    instrumentPresets[105].patch.p_fmModIndex = 18.85f;
    instrumentPresets[105].patch.p_fmFeedback = 0.2f;
    instrumentPresets[105].patch.p_fmMod2Ratio = 2.76f;
    instrumentPresets[105].patch.p_fmMod2Index = 15.71f;
    instrumentPresets[105].patch.p_fmAlgorithm = FM_ALG_BRANCH;
    instrumentPresets[105].patch.p_attack = 0.002f;
    instrumentPresets[105].patch.p_decay = 3.0f;
    instrumentPresets[105].patch.p_sustain = 0.0f;
    instrumentPresets[105].patch.p_release = 1.0f;
    instrumentPresets[105].patch.p_expDecay = true;
    instrumentPresets[105].patch.p_filterCutoff = 0.5f;
    instrumentPresets[105].patch.p_volume = 0.35f;

    // 106: FM Organ — Pair algorithm, mod2 adds body as additive sine
    instrumentPresets[106].name = "FM Organ";
    instrumentPresets[106].patch.p_waveType = WAVE_FM;
    instrumentPresets[106].patch.p_fmModRatio = 1.0f;
    instrumentPresets[106].patch.p_fmModIndex = 5.03f;
    instrumentPresets[106].patch.p_fmFeedback = 0.3f;
    instrumentPresets[106].patch.p_fmMod2Ratio = 0.5f;
    instrumentPresets[106].patch.p_fmMod2Index = 2.0f;     // was 18.85 — PAIR uses index*0.3 as mix level, so 2.0 → 0.6x sub-octave (16' drawbar)
    instrumentPresets[106].patch.p_fmAlgorithm = FM_ALG_PAIR;
    instrumentPresets[106].patch.p_attack = 0.005f;
    instrumentPresets[106].patch.p_decay = 0.1f;
    instrumentPresets[106].patch.p_sustain = 0.8f;
    instrumentPresets[106].patch.p_release = 0.1f;
    instrumentPresets[106].patch.p_volume = 0.4f;

    // === Bowed String presets ===

    // 107: Bowed Cello — warm, rich, moderate vibrato
    instrumentPresets[107].name = "Bowed Cello";
    instrumentPresets[107].patch.p_waveType = WAVE_BOWED;
    instrumentPresets[107].patch.p_bowPressure = 0.6f;
    instrumentPresets[107].patch.p_bowSpeed = 0.5f;
    instrumentPresets[107].patch.p_bowPosition = 0.13f;
    instrumentPresets[107].patch.p_attack = 0.08f;
    instrumentPresets[107].patch.p_decay = 0.3f;
    instrumentPresets[107].patch.p_sustain = 0.8f;
    instrumentPresets[107].patch.p_release = 0.2f;
    instrumentPresets[107].patch.p_vibratoRate = 5.5f;
    instrumentPresets[107].patch.p_vibratoDepth = 0.12f;
    instrumentPresets[107].patch.p_filterCutoff = 0.7f;
    instrumentPresets[107].patch.p_analogRolloff = true;
    instrumentPresets[107].patch.p_volume = 0.5f;

    // 108: Bowed Fiddle — brighter, more pressure, faster attack
    instrumentPresets[108].name = "Bowed Fiddle";
    instrumentPresets[108].patch.p_waveType = WAVE_BOWED;
    instrumentPresets[108].patch.p_bowPressure = 0.8f;
    instrumentPresets[108].patch.p_bowSpeed = 0.65f;
    instrumentPresets[108].patch.p_bowPosition = 0.1f;
    instrumentPresets[108].patch.p_attack = 0.04f;
    instrumentPresets[108].patch.p_decay = 0.2f;
    instrumentPresets[108].patch.p_sustain = 0.85f;
    instrumentPresets[108].patch.p_release = 0.15f;
    instrumentPresets[108].patch.p_vibratoRate = 6.0f;
    instrumentPresets[108].patch.p_vibratoDepth = 0.15f;
    instrumentPresets[108].patch.p_filterCutoff = 0.85f;
    instrumentPresets[108].patch.p_volume = 0.45f;

    // === Blown Pipe presets ===

    // 109: Pipe Flute — breathy, gentle, classic flute tone
    instrumentPresets[109].name = "Pipe Flute";
    instrumentPresets[109].patch.p_waveType = WAVE_PIPE;
    instrumentPresets[109].patch.p_pipeBreath = 0.7f;
    instrumentPresets[109].patch.p_pipeEmbouchure = 0.6f;
    instrumentPresets[109].patch.p_pipeBore = 0.5f;
    instrumentPresets[109].patch.p_pipeOverblow = 0.0f;
    instrumentPresets[109].patch.p_attack = 0.04f;
    instrumentPresets[109].patch.p_decay = 0.2f;
    instrumentPresets[109].patch.p_sustain = 0.8f;
    instrumentPresets[109].patch.p_release = 0.12f;
    instrumentPresets[109].patch.p_vibratoRate = 5.0f;
    instrumentPresets[109].patch.p_vibratoDepth = 0.08f;
    instrumentPresets[109].patch.p_filterCutoff = 0.85f;
    instrumentPresets[109].patch.p_noiseMix = 0.06f;
    instrumentPresets[109].patch.p_noiseTone = 0.3f;
    instrumentPresets[109].patch.p_volume = 0.7f;

    // 110: Recorder — simpler bore, more breathy, medieval character
    instrumentPresets[110].name = "Recorder";
    instrumentPresets[110].patch.p_waveType = WAVE_PIPE;
    instrumentPresets[110].patch.p_pipeBreath = 0.65f;
    instrumentPresets[110].patch.p_pipeEmbouchure = 0.35f;
    instrumentPresets[110].patch.p_pipeBore = 0.5f;
    instrumentPresets[110].patch.p_pipeOverblow = 0.0f;
    instrumentPresets[110].patch.p_attack = 0.02f;
    instrumentPresets[110].patch.p_decay = 0.15f;
    instrumentPresets[110].patch.p_sustain = 0.75f;
    instrumentPresets[110].patch.p_release = 0.08f;
    instrumentPresets[110].patch.p_vibratoRate = 4.5f;
    instrumentPresets[110].patch.p_vibratoDepth = 0.06f;
    instrumentPresets[110].patch.p_filterCutoff = 0.7f;
    instrumentPresets[110].patch.p_noiseMix = 0.08f;
    instrumentPresets[110].patch.p_noiseTone = 0.25f;
    instrumentPresets[110].patch.p_volume = 0.65f;

    // ========================================================================
    // NEW MELODIC PRESETS (111-126)
    // ========================================================================

    // Wurlitzer — electrostatic reed piano, reedy/nasal bark, odd harmonics
    instrumentPresets[111].name = "Wurlitzer";
    instrumentPresets[111].patch.p_waveType = WAVE_EPIANO;
    instrumentPresets[111].patch.p_epPickupType = EP_PICKUP_ELECTROSTATIC;
    instrumentPresets[111].patch.p_epHardness = 0.5f;       // medium hammer
    instrumentPresets[111].patch.p_epToneBar = 0.0f;        // no tone bar (reed, not tine)
    instrumentPresets[111].patch.p_epPickupPos = 0.45f;     // slightly centered — clean fundamental
    instrumentPresets[111].patch.p_epPickupDist = 0.6f;     // drives into odd-harmonic buzz easily
    instrumentPresets[111].patch.p_epDecay = 1.8f;          // shorter than Rhodes (no tone bar sustain)
    instrumentPresets[111].patch.p_epBell = 0.15f;          // minimal bell — reedy, not chimey
    instrumentPresets[111].patch.p_epBellTone = 0.05f;      // mostly harmonic (reed modes are near-integer)
    instrumentPresets[111].patch.p_attack = 0.002f;
    instrumentPresets[111].patch.p_decay = 0.8f;
    instrumentPresets[111].patch.p_sustain = 0.3f;
    instrumentPresets[111].patch.p_release = 0.25f;
    instrumentPresets[111].patch.p_clickLevel = 0.15f;      // reed snap on attack
    instrumentPresets[111].patch.p_clickTime = 0.002f;
    instrumentPresets[111].patch.p_filterEnabled = true;
    instrumentPresets[111].patch.p_filterCutoff = 0.6f;
    instrumentPresets[111].patch.p_filterResonance = 0.1f;
    instrumentPresets[111].patch.p_filterKeyTrack = 0.4f;
    instrumentPresets[111].patch.p_vibratoRate = 6.0f;      // built-in 200A tremolo speed
    instrumentPresets[111].patch.p_vibratoDepth = 0.05f;
    instrumentPresets[111].patch.p_drive = 0.15f;
    instrumentPresets[111].patch.p_volume = 0.55f;
    instrumentPresets[111].patch.p_velToDrive = 0.4f;       // gets barky with velocity

    // Clavinet — funky percussive pluck with wah-style filter sweep
    instrumentPresets[112].name = "Clavinet";
    instrumentPresets[112].patch.p_waveType = WAVE_PLUCK;
    instrumentPresets[112].patch.p_pluckBrightness = 0.75f;
    instrumentPresets[112].patch.p_pluckDamping = 0.992f;
    instrumentPresets[112].patch.p_pluckDamp = 0.0f;
    instrumentPresets[112].patch.p_envelopeEnabled = true;
    instrumentPresets[112].patch.p_filterEnabled = true;
    instrumentPresets[112].patch.p_filterCutoff = 0.45f;
    instrumentPresets[112].patch.p_filterResonance = 0.35f;
    instrumentPresets[112].patch.p_filterEnvAmt = 0.5f;
    instrumentPresets[112].patch.p_filterEnvDecay = 0.12f;
    instrumentPresets[112].patch.p_filterKeyTrack = 0.6f;
    instrumentPresets[112].patch.p_clickLevel = 0.3f;
    instrumentPresets[112].patch.p_clickTime = 0.003f;
    instrumentPresets[112].patch.p_volume = 0.6f;
    instrumentPresets[112].patch.p_attack = 0.001f;
    instrumentPresets[112].patch.p_decay = 0.0f;
    instrumentPresets[112].patch.p_sustain = 1.0f;
    instrumentPresets[112].patch.p_release = 0.3f;

    // Toy Piano — bright, slightly detuned FM, short and characterful
    instrumentPresets[113].name = "Toy Piano";
    instrumentPresets[113].patch.p_waveType = WAVE_FM;
    instrumentPresets[113].patch.p_fmModRatio = 3.0f;
    instrumentPresets[113].patch.p_fmModIndex = 7.54f;
    instrumentPresets[113].patch.p_attack = 0.001f;
    instrumentPresets[113].patch.p_decay = 0.4f;
    instrumentPresets[113].patch.p_sustain = 0.0f;
    instrumentPresets[113].patch.p_release = 0.15f;
    instrumentPresets[113].patch.p_expDecay = true;
    instrumentPresets[113].patch.p_filterCutoff = 0.75f;
    instrumentPresets[113].patch.p_unisonCount = 2;
    instrumentPresets[113].patch.p_unisonDetune = 8.0f;
    instrumentPresets[113].patch.p_pitchEnvAmount = 2.0f;
    instrumentPresets[113].patch.p_pitchEnvDecay = 0.01f;
    instrumentPresets[113].patch.p_volume = 0.5f;

    // Honky-tonk Piano — detuned FM, Chrono Trigger shop vibes
    instrumentPresets[114].name = "Honky Piano";
    instrumentPresets[114].patch.p_waveType = WAVE_FM;
    instrumentPresets[114].patch.p_fmModRatio = 1.0f;
    instrumentPresets[114].patch.p_fmModIndex = 11.31f;
    instrumentPresets[114].patch.p_fmFeedback = 0.08f;
    instrumentPresets[114].patch.p_attack = 0.001f;
    instrumentPresets[114].patch.p_decay = 1.2f;
    instrumentPresets[114].patch.p_sustain = 0.15f;
    instrumentPresets[114].patch.p_release = 0.4f;
    instrumentPresets[114].patch.p_expDecay = true;
    instrumentPresets[114].patch.p_filterCutoff = 0.6f;
    instrumentPresets[114].patch.p_filterKeyTrack = 0.5f;
    instrumentPresets[114].patch.p_unisonCount = 2;
    instrumentPresets[114].patch.p_unisonDetune = 12.0f;
    instrumentPresets[114].patch.p_unisonMix = 0.7f;
    instrumentPresets[114].patch.p_tubeSaturation = true;
    instrumentPresets[114].patch.p_volume = 0.5f;

    // Fretless Bass — warm saw mono with portamento and vibrato
    instrumentPresets[115].name = "Fretless Bass";
    instrumentPresets[115].patch.p_waveType = WAVE_SAW;
    instrumentPresets[115].patch.p_attack = 0.02f;
    instrumentPresets[115].patch.p_decay = 0.2f;
    instrumentPresets[115].patch.p_sustain = 0.7f;
    instrumentPresets[115].patch.p_release = 0.15f;
    instrumentPresets[115].patch.p_filterCutoff = 0.35f;
    instrumentPresets[115].patch.p_filterResonance = 0.1f;
    instrumentPresets[115].patch.p_filterEnvAmt = 0.15f;
    instrumentPresets[115].patch.p_filterEnvDecay = 0.3f;
    instrumentPresets[115].patch.p_filterKeyTrack = 0.3f;
    instrumentPresets[115].patch.p_monoMode = true;
    instrumentPresets[115].patch.p_glideTime = 0.08f;
    instrumentPresets[115].patch.p_vibratoRate = 4.5f;
    instrumentPresets[115].patch.p_vibratoDepth = 0.1f;
    instrumentPresets[115].patch.p_analogRolloff = true;
    instrumentPresets[115].patch.p_tubeSaturation = true;
    instrumentPresets[115].patch.p_volume = 0.6f;

    // FM Bass — punchy DX7 bass, fast mod index decay
    instrumentPresets[116].name = "FM Bass";
    instrumentPresets[116].patch.p_waveType = WAVE_FM;
    instrumentPresets[116].patch.p_fmModRatio = 1.0f;
    instrumentPresets[116].patch.p_fmModIndex = 25.13f;
    instrumentPresets[116].patch.p_attack = 0.001f;
    instrumentPresets[116].patch.p_decay = 0.35f;
    instrumentPresets[116].patch.p_sustain = 0.2f;
    instrumentPresets[116].patch.p_release = 0.1f;
    instrumentPresets[116].patch.p_expDecay = true;
    instrumentPresets[116].patch.p_filterCutoff = 0.5f;
    instrumentPresets[116].patch.p_filterEnvAmt = 0.4f;
    instrumentPresets[116].patch.p_filterEnvDecay = 0.15f;
    instrumentPresets[116].patch.p_monoMode = true;
    instrumentPresets[116].patch.p_glideTime = 0.02f;
    instrumentPresets[116].patch.p_volume = 0.6f;

    // Slap Bass — pluck with noise burst transient, funky
    instrumentPresets[117].name = "Slap Bass";
    instrumentPresets[117].patch.p_waveType = WAVE_PLUCK;
    instrumentPresets[117].patch.p_pluckBrightness = 0.7f;
    instrumentPresets[117].patch.p_pluckDamping = 0.994f;
    instrumentPresets[117].patch.p_pluckDamp = 0.0f;
    instrumentPresets[117].patch.p_envelopeEnabled = true;
    instrumentPresets[117].patch.p_filterEnabled = true;
    instrumentPresets[117].patch.p_filterCutoff = 0.6f;
    instrumentPresets[117].patch.p_filterResonance = 0.2f;
    instrumentPresets[117].patch.p_filterEnvAmt = 0.35f;
    instrumentPresets[117].patch.p_filterEnvDecay = 0.08f;
    instrumentPresets[117].patch.p_noiseMix = 0.25f;
    instrumentPresets[117].patch.p_noiseTone = 0.7f;
    instrumentPresets[117].patch.p_noiseDecay = 0.015f;
    instrumentPresets[117].patch.p_clickLevel = 0.4f;
    instrumentPresets[117].patch.p_clickTime = 0.004f;
    instrumentPresets[117].patch.p_volume = 0.55f;
    instrumentPresets[117].patch.p_attack = 0.001f;
    instrumentPresets[117].patch.p_decay = 0.0f;
    instrumentPresets[117].patch.p_sustain = 1.0f;
    instrumentPresets[117].patch.p_release = 0.3f;

    // Muted Guitar — very short damped pluck, percussive funk/reggae
    instrumentPresets[118].name = "Mute Guitar";
    instrumentPresets[118].patch.p_waveType = WAVE_PLUCK;
    instrumentPresets[118].patch.p_pluckBrightness = 0.55f;
    instrumentPresets[118].patch.p_pluckDamping = 0.985f;   // very short
    instrumentPresets[118].patch.p_pluckDamp = 0.3f;        // extra damping
    instrumentPresets[118].patch.p_envelopeEnabled = true;
    instrumentPresets[118].patch.p_filterEnabled = true;
    instrumentPresets[118].patch.p_attack = 0.001f;
    instrumentPresets[118].patch.p_decay = 0.0f;
    instrumentPresets[118].patch.p_sustain = 1.0f;
    instrumentPresets[118].patch.p_release = 0.15f;
    instrumentPresets[118].patch.p_filterCutoff = 0.5f;
    instrumentPresets[118].patch.p_filterKeyTrack = 0.3f;
    instrumentPresets[118].patch.p_analogRolloff = true;
    instrumentPresets[118].patch.p_volume = 0.6f;

    // Accordion — musette detuning, two voices, reedy square wave
    instrumentPresets[119].name = "Accordion";
    instrumentPresets[119].patch.p_waveType = WAVE_SQUARE;
    instrumentPresets[119].patch.p_pulseWidth = 0.45f;
    instrumentPresets[119].patch.p_attack = 0.04f;
    instrumentPresets[119].patch.p_decay = 0.1f;
    instrumentPresets[119].patch.p_sustain = 0.8f;
    instrumentPresets[119].patch.p_release = 0.08f;
    instrumentPresets[119].patch.p_filterCutoff = 0.55f;
    instrumentPresets[119].patch.p_filterResonance = 0.1f;
    instrumentPresets[119].patch.p_filterKeyTrack = 0.3f;
    instrumentPresets[119].patch.p_unisonCount = 2;
    instrumentPresets[119].patch.p_unisonDetune = 18.0f;    // musette tuning
    instrumentPresets[119].patch.p_unisonMix = 0.7f;
    instrumentPresets[119].patch.p_vibratoRate = 6.0f;
    instrumentPresets[119].patch.p_vibratoDepth = 0.04f;
    instrumentPresets[119].patch.p_volume = 0.5f;

    // Ocarina — pure, hollow, Zelda-esque with vibrato
    instrumentPresets[120].name = "Ocarina";
    instrumentPresets[120].patch.p_waveType = WAVE_TRIANGLE;
    instrumentPresets[120].patch.p_attack = 0.05f;
    instrumentPresets[120].patch.p_decay = 0.1f;
    instrumentPresets[120].patch.p_sustain = 0.8f;
    instrumentPresets[120].patch.p_release = 0.2f;
    instrumentPresets[120].patch.p_filterCutoff = 0.6f;
    instrumentPresets[120].patch.p_filterResonance = 0.05f;
    instrumentPresets[120].patch.p_filterKeyTrack = 0.5f;
    instrumentPresets[120].patch.p_noiseMix = 0.06f;
    instrumentPresets[120].patch.p_noiseTone = 0.15f;
    instrumentPresets[120].patch.p_vibratoRate = 5.5f;
    instrumentPresets[120].patch.p_vibratoDepth = 0.15f;
    instrumentPresets[120].patch.p_analogRolloff = true;
    instrumentPresets[120].patch.p_volume = 0.55f;

    // Grain Pad — evolving granular texture, first granular preset!
    instrumentPresets[121].name = "Grain Pad";
    instrumentPresets[121].patch.p_waveType = WAVE_GRANULAR;
    instrumentPresets[121].patch.p_granularScwIndex = 8;    // SIDSAW — rich harmonic source
    instrumentPresets[121].patch.p_granularGrainSize = 120.0f;
    instrumentPresets[121].patch.p_granularDensity = 35.0f;
    instrumentPresets[121].patch.p_granularPosition = 0.3f;
    instrumentPresets[121].patch.p_granularPosRandom = 0.4f;
    instrumentPresets[121].patch.p_granularPitch = 1.0f;
    instrumentPresets[121].patch.p_granularPitchRandom = 0.08f;
    instrumentPresets[121].patch.p_granularAmpRandom = 0.2f;
    instrumentPresets[121].patch.p_granularSpread = 0.7f;
    instrumentPresets[121].patch.p_attack = 0.5f;
    instrumentPresets[121].patch.p_decay = 0.3f;
    instrumentPresets[121].patch.p_sustain = 0.7f;
    instrumentPresets[121].patch.p_release = 0.8f;
    instrumentPresets[121].patch.p_filterCutoff = 0.45f;
    instrumentPresets[121].patch.p_filterResonance = 0.1f;
    instrumentPresets[121].patch.p_filterLfoRate = 0.15f;
    instrumentPresets[121].patch.p_filterLfoDepth = 0.2f;
    instrumentPresets[121].patch.p_volume = 0.5f;

    // Grain Shimmer — bright, frozen granular sparkle
    instrumentPresets[122].name = "Grain Shimmer";
    instrumentPresets[122].patch.p_waveType = WAVE_GRANULAR;
    instrumentPresets[122].patch.p_granularScwIndex = 2;    // NES triangle — pure source
    instrumentPresets[122].patch.p_granularGrainSize = 40.0f;
    instrumentPresets[122].patch.p_granularDensity = 60.0f;
    instrumentPresets[122].patch.p_granularPosition = 0.5f;
    instrumentPresets[122].patch.p_granularPosRandom = 0.6f;
    instrumentPresets[122].patch.p_granularPitch = 2.0f;     // octave up grains
    instrumentPresets[122].patch.p_granularPitchRandom = 0.15f;
    instrumentPresets[122].patch.p_granularAmpRandom = 0.3f;
    instrumentPresets[122].patch.p_granularSpread = 0.9f;
    instrumentPresets[122].patch.p_attack = 0.3f;
    instrumentPresets[122].patch.p_decay = 0.2f;
    instrumentPresets[122].patch.p_sustain = 0.6f;
    instrumentPresets[122].patch.p_release = 1.2f;
    instrumentPresets[122].patch.p_filterCutoff = 0.65f;
    instrumentPresets[122].patch.p_volume = 0.45f;

    // Dark Drone — low, menacing, resonant square wave
    instrumentPresets[123].name = "Dark Drone";
    instrumentPresets[123].patch.p_waveType = WAVE_SQUARE;
    instrumentPresets[123].patch.p_pulseWidth = 0.4f;
    instrumentPresets[123].patch.p_pwmRate = 0.15f;
    instrumentPresets[123].patch.p_pwmDepth = 0.15f;
    instrumentPresets[123].patch.p_attack = 0.8f;
    instrumentPresets[123].patch.p_decay = 0.5f;
    instrumentPresets[123].patch.p_sustain = 0.9f;
    instrumentPresets[123].patch.p_release = 1.5f;
    instrumentPresets[123].patch.p_filterCutoff = 0.18f;
    instrumentPresets[123].patch.p_filterResonance = 0.6f;
    instrumentPresets[123].patch.p_filterLfoRate = 0.08f;
    instrumentPresets[123].patch.p_filterLfoDepth = 0.15f;
    instrumentPresets[123].patch.p_unisonCount = 3;
    instrumentPresets[123].patch.p_unisonDetune = 6.0f;
    instrumentPresets[123].patch.p_volume = 0.5f;

    // SNES Strings — warm saw through bitcrusher, Chrono Trigger
    instrumentPresets[124].name = "SNES Strings";
    instrumentPresets[124].patch.p_waveType = WAVE_SAW;
    instrumentPresets[124].patch.p_attack = 0.15f;
    instrumentPresets[124].patch.p_decay = 0.2f;
    instrumentPresets[124].patch.p_sustain = 0.7f;
    instrumentPresets[124].patch.p_release = 0.3f;
    instrumentPresets[124].patch.p_filterCutoff = 0.4f;
    instrumentPresets[124].patch.p_filterResonance = 0.05f;
    instrumentPresets[124].patch.p_filterKeyTrack = 0.3f;
    instrumentPresets[124].patch.p_unisonCount = 2;
    instrumentPresets[124].patch.p_unisonDetune = 12.0f;
    instrumentPresets[124].patch.p_drive = 0.15f;
    instrumentPresets[124].patch.p_analogRolloff = true;
    instrumentPresets[124].patch.p_volume = 0.5f;

    // SNES Brass — short punchy stabs, Secret of Mana fanfares
    instrumentPresets[125].name = "SNES Brass";
    instrumentPresets[125].patch.p_waveType = WAVE_SAW;
    instrumentPresets[125].patch.p_attack = 0.01f;
    instrumentPresets[125].patch.p_decay = 0.15f;
    instrumentPresets[125].patch.p_sustain = 0.6f;
    instrumentPresets[125].patch.p_release = 0.1f;
    instrumentPresets[125].patch.p_filterCutoff = 0.5f;
    instrumentPresets[125].patch.p_filterResonance = 0.1f;
    instrumentPresets[125].patch.p_filterEnvAmt = 0.4f;
    instrumentPresets[125].patch.p_filterEnvDecay = 0.1f;
    instrumentPresets[125].patch.p_unisonCount = 2;
    instrumentPresets[125].patch.p_unisonDetune = 8.0f;
    instrumentPresets[125].patch.p_drive = 0.2f;
    instrumentPresets[125].patch.p_volume = 0.55f;

    // SNES Harp — bright pluck, long decay, Zelda/Terranigma
    instrumentPresets[126].name = "SNES Harp";
    instrumentPresets[126].patch.p_waveType = WAVE_PLUCK;
    instrumentPresets[126].patch.p_pluckBrightness = 0.65f;
    instrumentPresets[126].patch.p_pluckDamping = 0.997f;   // long sustain
    instrumentPresets[126].patch.p_pluckDamp = 0.0f;
    instrumentPresets[126].patch.p_envelopeEnabled = true;
    instrumentPresets[126].patch.p_filterEnabled = true;
    instrumentPresets[126].patch.p_attack = 0.001f;
    instrumentPresets[126].patch.p_decay = 0.0f;
    instrumentPresets[126].patch.p_sustain = 1.0f;
    instrumentPresets[126].patch.p_release = 0.3f;
    instrumentPresets[126].patch.p_filterCutoff = 0.7f;
    instrumentPresets[126].patch.p_filterKeyTrack = 0.4f;
    instrumentPresets[126].patch.p_drive = 0.1f;
    instrumentPresets[126].patch.p_analogRolloff = true;
    instrumentPresets[126].patch.p_volume = 0.55f;

    // 127: Mono Lead — Minimoog-style fat saw, filter sweep, mono+glide
    instrumentPresets[127].name = "Mono Lead";
    instrumentPresets[127].patch.p_waveType = WAVE_SAW;
    instrumentPresets[127].patch.p_attack = 0.005f;
    instrumentPresets[127].patch.p_decay = 0.3f;
    instrumentPresets[127].patch.p_sustain = 0.7f;
    instrumentPresets[127].patch.p_release = 0.15f;
    instrumentPresets[127].patch.p_filterCutoff = 0.35f;
    instrumentPresets[127].patch.p_filterResonance = 0.25f;
    instrumentPresets[127].patch.p_filterEnvAmt = 0.45f;
    instrumentPresets[127].patch.p_filterEnvAttack = 0.005f;
    instrumentPresets[127].patch.p_filterEnvDecay = 0.35f;
    instrumentPresets[127].patch.p_filterKeyTrack = 0.5f;
    instrumentPresets[127].patch.p_unisonCount = 3;
    instrumentPresets[127].patch.p_unisonDetune = 12.0f;
    instrumentPresets[127].patch.p_unisonMix = 0.5f;
    instrumentPresets[127].patch.p_monoMode = true;
    instrumentPresets[127].patch.p_glideTime = 0.08f;
    instrumentPresets[127].patch.p_drive = 0.15f;
    instrumentPresets[127].patch.p_analogRolloff = true;
    instrumentPresets[127].patch.p_volume = 0.5f;

    // 128: Hoover — detuned saws, dark, wide, menacing reese bass
    instrumentPresets[128].name = "Hoover";
    instrumentPresets[128].patch.p_waveType = WAVE_SAW;
    instrumentPresets[128].patch.p_attack = 0.02f;
    instrumentPresets[128].patch.p_decay = 0.5f;
    instrumentPresets[128].patch.p_sustain = 0.8f;
    instrumentPresets[128].patch.p_release = 0.4f;
    instrumentPresets[128].patch.p_filterCutoff = 0.3f;
    instrumentPresets[128].patch.p_filterResonance = 0.15f;
    instrumentPresets[128].patch.p_filterEnvAmt = 0.25f;
    instrumentPresets[128].patch.p_filterEnvDecay = 0.6f;
    instrumentPresets[128].patch.p_unisonCount = 4;
    instrumentPresets[128].patch.p_unisonDetune = 25.0f;
    instrumentPresets[128].patch.p_unisonMix = 0.7f;
    instrumentPresets[128].patch.p_monoMode = true;
    instrumentPresets[128].patch.p_glideTime = 0.12f;
    instrumentPresets[128].patch.p_drive = 0.25f;
    instrumentPresets[128].patch.p_tubeSaturation = true;
    instrumentPresets[128].patch.p_analogRolloff = true;
    instrumentPresets[128].patch.p_pitchLfoRate = 4.5f;
    instrumentPresets[128].patch.p_pitchLfoDepth = 0.15f;
    instrumentPresets[128].patch.p_pitchLfoShape = 0;  // sine
    instrumentPresets[128].patch.p_volume = 0.5f;

    // 129: Screamer — high-resonance saw lead, aggressive filter, fast glide
    instrumentPresets[129].name = "Screamer";
    instrumentPresets[129].patch.p_waveType = WAVE_SAW;
    instrumentPresets[129].patch.p_attack = 0.002f;
    instrumentPresets[129].patch.p_decay = 0.2f;
    instrumentPresets[129].patch.p_sustain = 0.6f;
    instrumentPresets[129].patch.p_release = 0.1f;
    instrumentPresets[129].patch.p_filterCutoff = 0.25f;
    instrumentPresets[129].patch.p_filterResonance = 0.6f;
    instrumentPresets[129].patch.p_filterEnvAmt = 0.7f;
    instrumentPresets[129].patch.p_filterEnvAttack = 0.002f;
    instrumentPresets[129].patch.p_filterEnvDecay = 0.25f;
    instrumentPresets[129].patch.p_filterKeyTrack = 0.7f;
    instrumentPresets[129].patch.p_unisonCount = 2;
    instrumentPresets[129].patch.p_unisonDetune = 8.0f;
    instrumentPresets[129].patch.p_monoMode = true;
    instrumentPresets[129].patch.p_glideTime = 0.04f;
    instrumentPresets[129].patch.p_drive = 0.3f;
    instrumentPresets[129].patch.p_volume = 0.45f;

    // 130: DX7 E.Piano — THE classic glassy/bell FM electric piano
    // Mod1 at 1:1 for body, Mod2 at 14:1 for metallic tine attack
    instrumentPresets[130].name = "DX7 E.Piano";
    instrumentPresets[130].patch.p_waveType = WAVE_FM;
    instrumentPresets[130].patch.p_fmModRatio = 1.0f;
    instrumentPresets[130].patch.p_fmModIndex = 2.0f;
    instrumentPresets[130].patch.p_fmFeedback = 0.0f;
    instrumentPresets[130].patch.p_fmMod2Ratio = 7.0f;
    instrumentPresets[130].patch.p_fmMod2Index = 0.8f;
    instrumentPresets[130].patch.p_fmAlgorithm = 1;  // parallel — two mods independent
    instrumentPresets[130].patch.p_attack = 0.003f;
    instrumentPresets[130].patch.p_decay = 2.5f;
    instrumentPresets[130].patch.p_sustain = 0.0f;
    instrumentPresets[130].patch.p_release = 0.4f;
    instrumentPresets[130].patch.p_expDecay = true;
    instrumentPresets[130].patch.p_filterCutoff = 0.8f;
    instrumentPresets[130].patch.p_filterResonance = 0.05f;
    instrumentPresets[130].patch.p_filterEnvAmt = 0.4f;
    instrumentPresets[130].patch.p_filterEnvAttack = 0.001f;
    instrumentPresets[130].patch.p_filterEnvDecay = 1.0f;
    instrumentPresets[130].patch.p_filterKeyTrack = 0.6f;
    instrumentPresets[130].patch.p_volume = 0.45f;

    // 131: DX7 Bass — punchy FM bass, 1:1 ratio + feedback for growl
    instrumentPresets[131].name = "DX7 Bass";
    instrumentPresets[131].patch.p_waveType = WAVE_FM;
    instrumentPresets[131].patch.p_fmModRatio = 1.0f;
    instrumentPresets[131].patch.p_fmModIndex = 4.0f;
    instrumentPresets[131].patch.p_fmFeedback = 0.2f;
    instrumentPresets[131].patch.p_fmMod2Ratio = 0.0f;
    instrumentPresets[131].patch.p_fmAlgorithm = 0;  // stack
    instrumentPresets[131].patch.p_attack = 0.005f;
    instrumentPresets[131].patch.p_decay = 0.4f;
    instrumentPresets[131].patch.p_sustain = 0.4f;
    instrumentPresets[131].patch.p_release = 0.1f;
    instrumentPresets[131].patch.p_filterCutoff = 0.4f;
    instrumentPresets[131].patch.p_filterResonance = 0.15f;
    instrumentPresets[131].patch.p_filterEnvAmt = 0.5f;
    instrumentPresets[131].patch.p_filterEnvAttack = 0.001f;
    instrumentPresets[131].patch.p_filterEnvDecay = 0.2f;
    instrumentPresets[131].patch.p_monoMode = true;
    instrumentPresets[131].patch.p_glideTime = 0.04f;
    instrumentPresets[131].patch.p_volume = 0.5f;

    // 132: DX7 Brass — slow swell, high feedback for sawtooth-like buzz
    instrumentPresets[132].name = "DX7 Brass";
    instrumentPresets[132].patch.p_waveType = WAVE_FM;
    instrumentPresets[132].patch.p_fmModRatio = 1.0f;
    instrumentPresets[132].patch.p_fmModIndex = 5.0f;
    instrumentPresets[132].patch.p_fmFeedback = 0.45f;
    instrumentPresets[132].patch.p_fmMod2Ratio = 0.0f;
    instrumentPresets[132].patch.p_fmAlgorithm = 0;  // stack
    instrumentPresets[132].patch.p_attack = 0.15f;
    instrumentPresets[132].patch.p_decay = 0.4f;
    instrumentPresets[132].patch.p_sustain = 0.8f;
    instrumentPresets[132].patch.p_release = 0.2f;
    instrumentPresets[132].patch.p_filterCutoff = 0.3f;
    instrumentPresets[132].patch.p_filterResonance = 0.05f;
    instrumentPresets[132].patch.p_filterEnvAmt = 0.6f;
    instrumentPresets[132].patch.p_filterEnvAttack = 0.15f;
    instrumentPresets[132].patch.p_filterEnvDecay = 0.8f;
    instrumentPresets[132].patch.p_volume = 0.45f;

    // 133: DX7 Bell — inharmonic ratios (3.5:1 + 7:1) for metallic bell
    instrumentPresets[133].name = "DX7 Bell";
    instrumentPresets[133].patch.p_waveType = WAVE_FM;
    instrumentPresets[133].patch.p_fmModRatio = 3.5f;
    instrumentPresets[133].patch.p_fmModIndex = 5.0f;
    instrumentPresets[133].patch.p_fmFeedback = 0.0f;
    instrumentPresets[133].patch.p_fmMod2Ratio = 7.0f;
    instrumentPresets[133].patch.p_fmMod2Index = 3.0f;
    instrumentPresets[133].patch.p_fmAlgorithm = 0;  // stack — deep metallic chain
    instrumentPresets[133].patch.p_attack = 0.001f;
    instrumentPresets[133].patch.p_decay = 4.0f;
    instrumentPresets[133].patch.p_sustain = 0.0f;
    instrumentPresets[133].patch.p_release = 1.5f;
    instrumentPresets[133].patch.p_expDecay = true;
    instrumentPresets[133].patch.p_filterCutoff = 0.7f;
    instrumentPresets[133].patch.p_filterEnvAmt = 0.4f;
    instrumentPresets[133].patch.p_filterEnvAttack = 0.001f;
    instrumentPresets[133].patch.p_filterEnvDecay = 3.0f;
    instrumentPresets[133].patch.p_filterKeyTrack = 0.4f;
    instrumentPresets[133].patch.p_volume = 0.4f;

    // 134: FM Clav — funky percussive clavinet, 1:1 + high index burst
    instrumentPresets[134].name = "FM Clav";
    instrumentPresets[134].patch.p_waveType = WAVE_FM;
    instrumentPresets[134].patch.p_fmModRatio = 1.0f;
    instrumentPresets[134].patch.p_fmModIndex = 8.0f;
    instrumentPresets[134].patch.p_fmFeedback = 0.25f;
    instrumentPresets[134].patch.p_fmMod2Ratio = 0.0f;
    instrumentPresets[134].patch.p_fmAlgorithm = 0;
    instrumentPresets[134].patch.p_attack = 0.001f;
    instrumentPresets[134].patch.p_decay = 0.25f;
    instrumentPresets[134].patch.p_sustain = 0.0f;
    instrumentPresets[134].patch.p_release = 0.05f;
    instrumentPresets[134].patch.p_expDecay = true;
    instrumentPresets[134].patch.p_filterCutoff = 0.6f;
    instrumentPresets[134].patch.p_filterResonance = 0.1f;
    instrumentPresets[134].patch.p_filterEnvAmt = 0.5f;
    instrumentPresets[134].patch.p_filterEnvAttack = 0.001f;
    instrumentPresets[134].patch.p_filterEnvDecay = 0.08f;
    instrumentPresets[134].patch.p_filterKeyTrack = 0.6f;
    instrumentPresets[134].patch.p_volume = 0.5f;

    // 135: FM Marimba — 4:1 ratio, bright attack into warm wooden decay
    instrumentPresets[135].name = "FM Marimba";
    instrumentPresets[135].patch.p_waveType = WAVE_FM;
    instrumentPresets[135].patch.p_fmModRatio = 4.0f;
    instrumentPresets[135].patch.p_fmModIndex = 4.0f;
    instrumentPresets[135].patch.p_fmFeedback = 0.0f;
    instrumentPresets[135].patch.p_fmMod2Ratio = 0.0f;
    instrumentPresets[135].patch.p_fmAlgorithm = 0;
    instrumentPresets[135].patch.p_attack = 0.001f;
    instrumentPresets[135].patch.p_decay = 1.2f;
    instrumentPresets[135].patch.p_sustain = 0.0f;
    instrumentPresets[135].patch.p_release = 0.3f;
    instrumentPresets[135].patch.p_expDecay = true;
    instrumentPresets[135].patch.p_filterCutoff = 0.65f;
    instrumentPresets[135].patch.p_filterEnvAmt = 0.35f;
    instrumentPresets[135].patch.p_filterEnvAttack = 0.001f;
    instrumentPresets[135].patch.p_filterEnvDecay = 0.5f;
    instrumentPresets[135].patch.p_filterKeyTrack = 0.5f;
    instrumentPresets[135].patch.p_volume = 0.45f;

    // 136: FM Flute — 1:1 ratio, very low index, pure breathy tone
    instrumentPresets[136].name = "FM Flute";
    instrumentPresets[136].patch.p_waveType = WAVE_FM;
    instrumentPresets[136].patch.p_fmModRatio = 1.0f;
    instrumentPresets[136].patch.p_fmModIndex = 0.8f;
    instrumentPresets[136].patch.p_fmFeedback = 0.0f;
    instrumentPresets[136].patch.p_fmMod2Ratio = 0.0f;
    instrumentPresets[136].patch.p_fmAlgorithm = 0;
    instrumentPresets[136].patch.p_attack = 0.06f;
    instrumentPresets[136].patch.p_decay = 0.2f;
    instrumentPresets[136].patch.p_sustain = 0.75f;
    instrumentPresets[136].patch.p_release = 0.15f;
    instrumentPresets[136].patch.p_noiseMix = 0.06f;
    instrumentPresets[136].patch.p_noiseTone = 0.6f;
    instrumentPresets[136].patch.p_noiseDecay = 0.08f;
    instrumentPresets[136].patch.p_filterCutoff = 0.55f;
    instrumentPresets[136].patch.p_filterKeyTrack = 0.7f;
    instrumentPresets[136].patch.p_vibratoRate = 5.0f;
    instrumentPresets[136].patch.p_vibratoDepth = 0.1f;
    instrumentPresets[136].patch.p_volume = 0.45f;

    // 137: Supersaw Pad — thick detuned saws, slow attack, 16-bar filter sweep
    instrumentPresets[137].name = "Supersaw Pad";
    instrumentPresets[137].patch.p_waveType = WAVE_SAW;
    instrumentPresets[137].patch.p_attack = 0.8f;
    instrumentPresets[137].patch.p_decay = 0.5f;
    instrumentPresets[137].patch.p_sustain = 0.7f;
    instrumentPresets[137].patch.p_release = 1.5f;
    instrumentPresets[137].patch.p_unisonCount = 4;
    instrumentPresets[137].patch.p_unisonDetune = 20.0f;
    instrumentPresets[137].patch.p_unisonMix = 0.6f;
    instrumentPresets[137].patch.p_filterCutoff = 0.4f;
    instrumentPresets[137].patch.p_filterResonance = 0.15f;
    instrumentPresets[137].patch.p_filterEnvAmt = 0.2f;
    instrumentPresets[137].patch.p_filterEnvAttack = 0.5f;
    instrumentPresets[137].patch.p_filterEnvDecay = 1.0f;
    instrumentPresets[137].patch.p_filterLfoDepth = 0.25f;
    instrumentPresets[137].patch.p_filterLfoShape = 0;  // sine
    instrumentPresets[137].patch.p_filterLfoSync = LFO_SYNC_16_1;  // 16 bars ~31s
    instrumentPresets[137].patch.p_analogRolloff = true;
    instrumentPresets[137].patch.p_drive = 0.1f;
    instrumentPresets[137].patch.p_volume = 0.4f;

    // 138: Warm Pad — gentle filtered saw, slow evolving, 32-bar reso sweep
    instrumentPresets[138].name = "Warm Pad";
    instrumentPresets[138].patch.p_waveType = WAVE_TRIANGLE;
    instrumentPresets[138].patch.p_attack = 1.2f;
    instrumentPresets[138].patch.p_decay = 0.8f;
    instrumentPresets[138].patch.p_sustain = 0.8f;
    instrumentPresets[138].patch.p_release = 2.0f;
    instrumentPresets[138].patch.p_expRelease = true;
    instrumentPresets[138].patch.p_unisonCount = 3;
    instrumentPresets[138].patch.p_unisonDetune = 10.0f;
    instrumentPresets[138].patch.p_unisonMix = 0.5f;
    instrumentPresets[138].patch.p_filterCutoff = 0.35f;
    instrumentPresets[138].patch.p_filterResonance = 0.1f;
    instrumentPresets[138].patch.p_filterKeyTrack = 0.4f;
    instrumentPresets[138].patch.p_filterLfoDepth = 0.15f;
    instrumentPresets[138].patch.p_filterLfoShape = 1;  // triangle — gentle back and forth
    instrumentPresets[138].patch.p_filterLfoSync = LFO_SYNC_32_1;  // 32 bars ~62s
    instrumentPresets[138].patch.p_resoLfoDepth = 0.12f;
    instrumentPresets[138].patch.p_resoLfoShape = 0;  // sine
    instrumentPresets[138].patch.p_resoLfoSync = LFO_SYNC_16_1;  // reso sweeps at half rate
    instrumentPresets[138].patch.p_resoLfoPhaseOffset = 0.25f;  // offset from filter for movement
    instrumentPresets[138].patch.p_analogRolloff = true;
    instrumentPresets[138].patch.p_tubeSaturation = true;
    instrumentPresets[138].patch.p_volume = 0.4f;

    // 139: Glass Pad — FM shimmer, slow attack, 8-bar FM LFO modulating brightness
    instrumentPresets[139].name = "Glass Pad";
    instrumentPresets[139].patch.p_waveType = WAVE_FM;
    instrumentPresets[139].patch.p_fmModRatio = 1.414f;  // sqrt(2) — glassy beating
    instrumentPresets[139].patch.p_fmModIndex = 2.5f;
    instrumentPresets[139].patch.p_fmFeedback = 0.0f;
    instrumentPresets[139].patch.p_fmMod2Ratio = 5.0f;   // shimmer
    instrumentPresets[139].patch.p_fmMod2Index = 1.0f;
    instrumentPresets[139].patch.p_fmAlgorithm = 1;  // parallel — cleaner
    instrumentPresets[139].patch.p_fmLfoDepth = 1.5f;
    instrumentPresets[139].patch.p_fmLfoShape = 0;  // sine
    instrumentPresets[139].patch.p_fmLfoSync = LFO_SYNC_8_1;  // 8 bars ~15s
    instrumentPresets[139].patch.p_attack = 1.0f;
    instrumentPresets[139].patch.p_decay = 0.5f;
    instrumentPresets[139].patch.p_sustain = 0.7f;
    instrumentPresets[139].patch.p_release = 2.0f;
    instrumentPresets[139].patch.p_expRelease = true;
    instrumentPresets[139].patch.p_filterCutoff = 0.6f;
    instrumentPresets[139].patch.p_filterResonance = 0.05f;
    instrumentPresets[139].patch.p_filterLfoDepth = 0.1f;
    instrumentPresets[139].patch.p_filterLfoSync = LFO_SYNC_16_1;  // filter offset from FM
    instrumentPresets[139].patch.p_filterLfoPhaseOffset = 0.5f;  // inverted — opens when FM dims
    instrumentPresets[139].patch.p_filterKeyTrack = 0.4f;
    instrumentPresets[139].patch.p_volume = 0.4f;

    // 140: 909 Kick — punchy click + short sine body, tighter than 808
    instrumentPresets[140].name = "909 Kick";
    instrumentPresets[140].patch.p_waveType = WAVE_FM;
    instrumentPresets[140].patch.p_fmModRatio = 1.0f;
    instrumentPresets[140].patch.p_fmModIndex = 0.0f;  // pure sine
    instrumentPresets[140].patch.p_attack = 0.0f;
    instrumentPresets[140].patch.p_decay = 0.3f;
    instrumentPresets[140].patch.p_sustain = 0.0f;
    instrumentPresets[140].patch.p_release = 0.03f;
    instrumentPresets[140].patch.p_expDecay = true;
    instrumentPresets[140].patch.p_pitchEnvAmount = 24.0f;  // sharper pitch drop than 808
    instrumentPresets[140].patch.p_pitchEnvDecay = 0.025f;  // faster sweep — snappier attack
    instrumentPresets[140].patch.p_pitchEnvLinear = true;
    instrumentPresets[140].patch.p_clickLevel = 0.5f;   // stronger click than 808
    instrumentPresets[140].patch.p_clickTime = 0.003f;   // shorter click burst
    instrumentPresets[140].patch.p_drive = 0.35f;
    instrumentPresets[140].patch.p_useTriggerFreq = true;
    instrumentPresets[140].patch.p_triggerFreq = 55.0f;  // slightly higher body than 808
    instrumentPresets[140].patch.p_volume = 0.55f;

    // 141: 909 Snare — bright noise snap + dual sine tones, punchy
    instrumentPresets[141].name = "909 Snare";
    instrumentPresets[141].patch.p_waveType = WAVE_FM;
    instrumentPresets[141].patch.p_fmModRatio = 1.0f;
    instrumentPresets[141].patch.p_fmModIndex = 0.0f;
    instrumentPresets[141].patch.p_osc2Ratio = 1.6f;    // higher 2nd partial — brighter tone
    instrumentPresets[141].patch.p_osc2Level = 0.4f;
    instrumentPresets[141].patch.p_attack = 0.0f;
    instrumentPresets[141].patch.p_decay = 0.15f;        // tighter tone body
    instrumentPresets[141].patch.p_sustain = 0.0f;
    instrumentPresets[141].patch.p_release = 0.04f;
    instrumentPresets[141].patch.p_expDecay = true;
    instrumentPresets[141].patch.p_noiseMix = 0.7f;      // more noise than 808 — THE 909 snare character
    instrumentPresets[141].patch.p_noiseTone = 0.65f;     // brighter noise
    instrumentPresets[141].patch.p_noiseHP = 0.2f;
    instrumentPresets[141].patch.p_noiseDecay = 0.15f;    // short noise tail
    instrumentPresets[141].patch.p_pitchEnvAmount = 8.0f; // subtle pitch snap
    instrumentPresets[141].patch.p_pitchEnvDecay = 0.01f;
    instrumentPresets[141].patch.p_pitchEnvLinear = true;
    instrumentPresets[141].patch.p_useTriggerFreq = true;
    instrumentPresets[141].patch.p_triggerFreq = 200.0f;
    instrumentPresets[141].patch.p_volume = 0.5f;

    // 142: 909 Clap — tighter retrigger, brighter than 808
    instrumentPresets[142].name = "909 Clap";
    instrumentPresets[142].patch.p_waveType = WAVE_NOISE;
    instrumentPresets[142].patch.p_attack = 0.0f;
    instrumentPresets[142].patch.p_decay = 0.2f;         // shorter tail
    instrumentPresets[142].patch.p_sustain = 0.0f;
    instrumentPresets[142].patch.p_release = 0.04f;
    instrumentPresets[142].patch.p_expDecay = true;
    instrumentPresets[142].patch.p_filterType = 2;       // BP
    instrumentPresets[142].patch.p_filterCutoff = 0.45f; // brighter than 808
    instrumentPresets[142].patch.p_noiseTone = 0.95f;
    instrumentPresets[142].patch.p_noiseHP = 0.12f;
    instrumentPresets[142].patch.p_retriggerCount = 3;
    instrumentPresets[142].patch.p_retriggerSpread = 0.008f;  // tighter than 808
    instrumentPresets[142].patch.p_retriggerOverlap = true;
    instrumentPresets[142].patch.p_retriggerBurstDecay = 0.015f;
    instrumentPresets[142].patch.p_retriggerCurve = 0.1f;
    instrumentPresets[142].patch.p_noiseMode = 2;
    instrumentPresets[142].patch.p_noiseType = 1;
    instrumentPresets[142].patch.p_useTriggerFreq = true;
    instrumentPresets[142].patch.p_triggerFreq = 400.0f;
    instrumentPresets[142].patch.p_volume = 0.5f;

    // 143: 909 CH — metallic squares + noise, brighter/thinner than 808
    instrumentPresets[143].name = "909 CH";
    instrumentPresets[143].patch.p_waveType = WAVE_SQUARE;
    instrumentPresets[143].patch.p_osc2Ratio = 1.4471f;
    instrumentPresets[143].patch.p_osc2Level = 0.9f;
    instrumentPresets[143].patch.p_osc3Ratio = 1.6170f;
    instrumentPresets[143].patch.p_osc3Level = 0.9f;
    instrumentPresets[143].patch.p_osc4Ratio = 1.9265f;
    instrumentPresets[143].patch.p_osc4Level = 0.8f;
    instrumentPresets[143].patch.p_osc5Ratio = 2.5028f;
    instrumentPresets[143].patch.p_osc5Level = 0.7f;
    instrumentPresets[143].patch.p_osc6Ratio = 2.6637f;
    instrumentPresets[143].patch.p_osc6Level = 0.6f;
    instrumentPresets[143].patch.p_noiseMix = 0.2f;      // 909 hats have more noise than 808
    instrumentPresets[143].patch.p_noiseTone = 0.8f;
    instrumentPresets[143].patch.p_noiseHP = 0.3f;
    instrumentPresets[143].patch.p_noiseDecay = 0.03f;
    instrumentPresets[143].patch.p_attack = 0.0f;
    instrumentPresets[143].patch.p_decay = 0.04f;         // tighter than 808
    instrumentPresets[143].patch.p_sustain = 0.0f;
    instrumentPresets[143].patch.p_release = 0.015f;
    instrumentPresets[143].patch.p_expDecay = true;
    instrumentPresets[143].patch.p_filterType = 1;        // HP
    instrumentPresets[143].patch.p_filterCutoff = 0.45f;  // brighter cutoff than 808
    instrumentPresets[143].patch.p_volume = 0.4f;
    instrumentPresets[143].patch.p_phaseReset = true;
    instrumentPresets[143].patch.p_useTriggerFreq = true;
    instrumentPresets[143].patch.p_triggerFreq = 480.0f;  // slightly higher pitch
    instrumentPresets[143].patch.p_choke = true;

    // 144: 909 OH — open hat, longer decay + noise wash
    instrumentPresets[144].name = "909 OH";
    instrumentPresets[144].patch.p_waveType = WAVE_SQUARE;
    instrumentPresets[144].patch.p_osc2Ratio = 1.4471f;
    instrumentPresets[144].patch.p_osc2Level = 0.9f;
    instrumentPresets[144].patch.p_osc3Ratio = 1.6170f;
    instrumentPresets[144].patch.p_osc3Level = 0.9f;
    instrumentPresets[144].patch.p_osc4Ratio = 1.9265f;
    instrumentPresets[144].patch.p_osc4Level = 0.8f;
    instrumentPresets[144].patch.p_osc5Ratio = 2.5028f;
    instrumentPresets[144].patch.p_osc5Level = 0.7f;
    instrumentPresets[144].patch.p_osc6Ratio = 2.6637f;
    instrumentPresets[144].patch.p_osc6Level = 0.6f;
    instrumentPresets[144].patch.p_noiseMix = 0.25f;
    instrumentPresets[144].patch.p_noiseTone = 0.8f;
    instrumentPresets[144].patch.p_noiseHP = 0.3f;
    instrumentPresets[144].patch.p_noiseDecay = 0.25f;
    instrumentPresets[144].patch.p_attack = 0.0f;
    instrumentPresets[144].patch.p_decay = 0.35f;
    instrumentPresets[144].patch.p_sustain = 0.0f;
    instrumentPresets[144].patch.p_release = 0.08f;
    instrumentPresets[144].patch.p_expDecay = true;
    instrumentPresets[144].patch.p_filterType = 1;        // HP
    instrumentPresets[144].patch.p_filterCutoff = 0.45f;
    instrumentPresets[144].patch.p_volume = 0.4f;
    instrumentPresets[144].patch.p_phaseReset = true;
    instrumentPresets[144].patch.p_useTriggerFreq = true;
    instrumentPresets[144].patch.p_triggerFreq = 480.0f;
    instrumentPresets[144].patch.p_choke = true;

    // 145: 909 Rim — short sine ping, sharp click
    instrumentPresets[145].name = "909 Rim";
    instrumentPresets[145].patch.p_waveType = WAVE_FM;
    instrumentPresets[145].patch.p_fmModRatio = 1.0f;
    instrumentPresets[145].patch.p_fmModIndex = 0.0f;
    instrumentPresets[145].patch.p_attack = 0.0f;
    instrumentPresets[145].patch.p_decay = 0.03f;
    instrumentPresets[145].patch.p_sustain = 0.0f;
    instrumentPresets[145].patch.p_release = 0.01f;
    instrumentPresets[145].patch.p_expDecay = true;
    instrumentPresets[145].patch.p_clickLevel = 0.8f;
    instrumentPresets[145].patch.p_clickTime = 0.002f;
    instrumentPresets[145].patch.p_pitchEnvAmount = 30.0f;
    instrumentPresets[145].patch.p_pitchEnvDecay = 0.008f;
    instrumentPresets[145].patch.p_pitchEnvLinear = true;
    instrumentPresets[145].patch.p_filterType = 1;  // HP — remove sub rumble
    instrumentPresets[145].patch.p_filterCutoff = 0.2f;
    instrumentPresets[145].patch.p_useTriggerFreq = true;
    instrumentPresets[145].patch.p_triggerFreq = 500.0f;
    instrumentPresets[145].patch.p_volume = 0.5f;

    // ========================================================================
    // Chip / 8-bit (146-147) — pure retro, no effects, fast glide
    // ========================================================================

    // Chip Square — NES/C64 bass, pure square, instant attack, tight gate
    instrumentPresets[146].name = "Chip Square";
    instrumentPresets[146].patch.p_waveType = WAVE_SQUARE;
    instrumentPresets[146].patch.p_attack = 0.001f;
    instrumentPresets[146].patch.p_decay = 0.08f;
    instrumentPresets[146].patch.p_sustain = 0.8f;
    instrumentPresets[146].patch.p_release = 0.02f;
    instrumentPresets[146].patch.p_filterCutoff = 1.0f;
    instrumentPresets[146].patch.p_filterResonance = 0.0f;
    instrumentPresets[146].patch.p_monoMode = true;
    instrumentPresets[146].patch.p_glideTime = 0.125f;  // 1/8.0 — fast glide for slides
    instrumentPresets[146].patch.p_volume = 0.45f;

    // Chip Saw — NES/C64 lead, pure sawtooth, slightly darker
    instrumentPresets[147].name = "Chip Saw";
    instrumentPresets[147].patch.p_waveType = WAVE_SAW;
    instrumentPresets[147].patch.p_attack = 0.001f;
    instrumentPresets[147].patch.p_decay = 0.1f;
    instrumentPresets[147].patch.p_sustain = 0.7f;
    instrumentPresets[147].patch.p_release = 0.03f;
    instrumentPresets[147].patch.p_filterCutoff = 0.85f;
    instrumentPresets[147].patch.p_filterResonance = 0.0f;
    instrumentPresets[147].patch.p_monoMode = true;
    instrumentPresets[147].patch.p_glideTime = 0.125f;
    instrumentPresets[147].patch.p_volume = 0.55f;

    // ========================================================================
    // DARK TECHNO / NOISE SWEEP PRESETS (148-150)
    // ========================================================================

    // 148: Dark Riser — white noise through HP filter, slow sweep up over 4 bars
    instrumentPresets[148].name = "Dark Riser";
    instrumentPresets[148].patch.p_waveType = WAVE_NOISE;
    instrumentPresets[148].patch.p_envelopeEnabled = true;
    instrumentPresets[148].patch.p_filterEnabled = true;
    instrumentPresets[148].patch.p_attack = 0.5f;
    instrumentPresets[148].patch.p_decay = 0.0f;
    instrumentPresets[148].patch.p_sustain = 1.0f;
    instrumentPresets[148].patch.p_release = 1.0f;
    instrumentPresets[148].patch.p_filterCutoff = 0.1f;
    instrumentPresets[148].patch.p_filterResonance = 0.3f;
    instrumentPresets[148].patch.p_filterType = 1;  // highpass
    instrumentPresets[148].patch.p_filterLfoDepth = 0.8f;
    instrumentPresets[148].patch.p_filterLfoShape = 3;  // sawtooth (ramp up)
    instrumentPresets[148].patch.p_filterLfoSync = LFO_SYNC_4_1;  // 4 bars
    instrumentPresets[148].patch.p_volume = 0.35f;

    // 149: Industrial Fall — noise + ring mod, LP filter sweep down over 2 bars
    instrumentPresets[149].name = "Ind Fall";
    instrumentPresets[149].patch.p_waveType = WAVE_NOISE;
    instrumentPresets[149].patch.p_envelopeEnabled = true;
    instrumentPresets[149].patch.p_filterEnabled = true;
    instrumentPresets[149].patch.p_attack = 0.01f;
    instrumentPresets[149].patch.p_decay = 0.0f;
    instrumentPresets[149].patch.p_sustain = 1.0f;
    instrumentPresets[149].patch.p_release = 0.5f;
    instrumentPresets[149].patch.p_filterCutoff = 0.9f;
    instrumentPresets[149].patch.p_filterResonance = 0.4f;
    instrumentPresets[149].patch.p_filterType = 0;  // lowpass
    instrumentPresets[149].patch.p_filterLfoDepth = 0.7f;
    instrumentPresets[149].patch.p_filterLfoShape = 3;  // sawtooth (ramp down via inverted phase)
    instrumentPresets[149].patch.p_filterLfoSync = LFO_SYNC_2_1;  // 2 bars
    instrumentPresets[149].patch.p_filterLfoPhaseOffset = 0.5f;  // invert = sweep down
    instrumentPresets[149].patch.p_ringMod = true;
    instrumentPresets[149].patch.p_ringModFreq = 3.0f;
    instrumentPresets[149].patch.p_volume = 0.3f;

    // 150: Tension Wash — noise + comb filter, slow S&H on cutoff for random texture
    instrumentPresets[150].name = "Tension";
    instrumentPresets[150].patch.p_waveType = WAVE_NOISE;
    instrumentPresets[150].patch.p_envelopeEnabled = true;
    instrumentPresets[150].patch.p_filterEnabled = true;
    instrumentPresets[150].patch.p_attack = 0.3f;
    instrumentPresets[150].patch.p_decay = 0.0f;
    instrumentPresets[150].patch.p_sustain = 1.0f;
    instrumentPresets[150].patch.p_release = 2.0f;
    instrumentPresets[150].patch.p_filterCutoff = 0.5f;
    instrumentPresets[150].patch.p_filterResonance = 0.6f;
    instrumentPresets[150].patch.p_filterType = 2;  // bandpass
    instrumentPresets[150].patch.p_filterLfoDepth = 0.5f;
    instrumentPresets[150].patch.p_filterLfoShape = 4;  // sample & hold (random steps)
    instrumentPresets[150].patch.p_filterLfoSync = LFO_SYNC_1_4;  // quarter note
    instrumentPresets[150].patch.p_drive = 0.3f;
    instrumentPresets[150].patch.p_driveMode = DIST_FOLDBACK;
    instrumentPresets[150].patch.p_volume = 0.3f;

    // ========================================================================
    // TOM SET (151-153) — hi/mid/lo, based on 808 Tom topology
    // ========================================================================

    // 151: Tom Hi — punchy, short, high-pitched tom
    instrumentPresets[151].name = "Tom Hi";
    instrumentPresets[151].patch.p_waveType = WAVE_FM;
    instrumentPresets[151].patch.p_fmModRatio = 2.0f;
    instrumentPresets[151].patch.p_fmModIndex = 0.94f;
    instrumentPresets[151].patch.p_attack = 0.0f;
    instrumentPresets[151].patch.p_decay = 0.2f;
    instrumentPresets[151].patch.p_sustain = 0.0f;
    instrumentPresets[151].patch.p_release = 0.04f;
    instrumentPresets[151].patch.p_expDecay = true;
    instrumentPresets[151].patch.p_filterCutoff = 1.0f;
    instrumentPresets[151].patch.p_filterResonance = 0.0f;
    instrumentPresets[151].patch.p_pitchEnvAmount = 10.0f;
    instrumentPresets[151].patch.p_pitchEnvDecay = 0.03f;
    instrumentPresets[151].patch.p_pitchEnvLinear = true;
    instrumentPresets[151].patch.p_useTriggerFreq = true;
    instrumentPresets[151].patch.p_triggerFreq = 160.0f;
    instrumentPresets[151].patch.p_volume = 0.5f;

    // 152: Tom Mid — medium body, classic fill tom
    instrumentPresets[152].name = "Tom Mid";
    instrumentPresets[152].patch.p_waveType = WAVE_FM;
    instrumentPresets[152].patch.p_fmModRatio = 2.0f;
    instrumentPresets[152].patch.p_fmModIndex = 0.94f;
    instrumentPresets[152].patch.p_attack = 0.0f;
    instrumentPresets[152].patch.p_decay = 0.3f;
    instrumentPresets[152].patch.p_sustain = 0.0f;
    instrumentPresets[152].patch.p_release = 0.05f;
    instrumentPresets[152].patch.p_expDecay = true;
    instrumentPresets[152].patch.p_filterCutoff = 1.0f;
    instrumentPresets[152].patch.p_filterResonance = 0.0f;
    instrumentPresets[152].patch.p_pitchEnvAmount = 12.0f;
    instrumentPresets[152].patch.p_pitchEnvDecay = 0.04f;
    instrumentPresets[152].patch.p_pitchEnvLinear = true;
    instrumentPresets[152].patch.p_useTriggerFreq = true;
    instrumentPresets[152].patch.p_triggerFreq = 100.0f;
    instrumentPresets[152].patch.p_volume = 0.5f;

    // 153: Tom Lo — deep floor tom, long decay
    instrumentPresets[153].name = "Tom Lo";
    instrumentPresets[153].patch.p_waveType = WAVE_FM;
    instrumentPresets[153].patch.p_fmModRatio = 2.0f;
    instrumentPresets[153].patch.p_fmModIndex = 0.94f;
    instrumentPresets[153].patch.p_attack = 0.0f;
    instrumentPresets[153].patch.p_decay = 0.45f;
    instrumentPresets[153].patch.p_sustain = 0.0f;
    instrumentPresets[153].patch.p_release = 0.06f;
    instrumentPresets[153].patch.p_expDecay = true;
    instrumentPresets[153].patch.p_filterCutoff = 1.0f;
    instrumentPresets[153].patch.p_filterResonance = 0.0f;
    instrumentPresets[153].patch.p_pitchEnvAmount = 14.0f;
    instrumentPresets[153].patch.p_pitchEnvDecay = 0.06f;
    instrumentPresets[153].patch.p_pitchEnvLinear = true;
    instrumentPresets[153].patch.p_useTriggerFreq = true;
    instrumentPresets[153].patch.p_triggerFreq = 60.0f;
    instrumentPresets[153].patch.p_volume = 0.5f;

    // ========================================================================
    // MT-70 PRESETS (154-163)
    // Casio MT-70 (1982): 2 mixed sine waves with different digital envelopes.
    // Source: weltenschule.de/TableHooters/Casio_MT-70.html
    // ========================================================================

    // MT-70 Piano — "plain sine wave with decay envelope" — the dullest piano ever
    instrumentPresets[154].name = "MT70 Piano";
    instrumentPresets[154].patch.p_waveType = WAVE_SINE;
    instrumentPresets[154].patch.p_attack = 0.002f;
    instrumentPresets[154].patch.p_decay = 0.8f;
    instrumentPresets[154].patch.p_sustain = 0.0f;
    instrumentPresets[154].patch.p_release = 0.3f;
    instrumentPresets[154].patch.p_filterCutoff = 0.6f;
    instrumentPresets[154].patch.p_volume = 0.7f;

    // MT-70 Pipe Organ — sustained Hammond-like: fundamental + 3rd harmonic (odd harmonics)
    instrumentPresets[155].name = "MT70 Organ";
    instrumentPresets[155].patch.p_waveType = WAVE_SINE;
    instrumentPresets[155].patch.p_attack = 0.01f;
    instrumentPresets[155].patch.p_decay = 0.1f;
    instrumentPresets[155].patch.p_sustain = 0.8f;
    instrumentPresets[155].patch.p_release = 0.08f;
    instrumentPresets[155].patch.p_osc2Ratio = 3.0f;   // 3rd harmonic (odd, organ character)
    instrumentPresets[155].patch.p_osc2Level = 0.4f;
    instrumentPresets[155].patch.p_osc2Decay = 0.0f;    // sustains
    instrumentPresets[155].patch.p_filterCutoff = 0.7f;
    instrumentPresets[155].patch.p_volume = 0.6f;

    // MT-70 Flute — pure sine with gentle attack, one of the "more realistic" sounds
    instrumentPresets[156].name = "MT70 Flute";
    instrumentPresets[156].patch.p_waveType = WAVE_SINE;
    instrumentPresets[156].patch.p_attack = 0.05f;
    instrumentPresets[156].patch.p_decay = 0.1f;
    instrumentPresets[156].patch.p_sustain = 0.7f;
    instrumentPresets[156].patch.p_release = 0.12f;
    instrumentPresets[156].patch.p_osc2Ratio = 2.0f;    // octave overtone
    instrumentPresets[156].patch.p_osc2Level = 0.15f;
    instrumentPresets[156].patch.p_osc2Decay = 3.0f;     // overtone fades, leaving pure fundamental
    instrumentPresets[156].patch.p_filterCutoff = 0.65f;
    instrumentPresets[156].patch.p_volume = 0.65f;

    // MT-70 Vibraphone — sine with decay, "one of the more realistic timbres"
    instrumentPresets[157].name = "MT70 Vibes";
    instrumentPresets[157].patch.p_waveType = WAVE_SINE;
    instrumentPresets[157].patch.p_attack = 0.002f;
    instrumentPresets[157].patch.p_decay = 1.2f;
    instrumentPresets[157].patch.p_sustain = 0.0f;
    instrumentPresets[157].patch.p_release = 0.4f;
    instrumentPresets[157].patch.p_osc2Ratio = 4.0f;    // 2nd partial (vibes are ~4:1)
    instrumentPresets[157].patch.p_osc2Level = 0.3f;
    instrumentPresets[157].patch.p_osc2Decay = 5.0f;     // overtone decays faster than fundamental
    instrumentPresets[157].patch.p_filterCutoff = 0.75f;
    instrumentPresets[157].patch.p_volume = 0.7f;

    // MT-70 Chime — "2 partial tones with 5 notes distance, same percussive decay"
    instrumentPresets[158].name = "MT70 Chime";
    instrumentPresets[158].patch.p_waveType = WAVE_SINE;
    instrumentPresets[158].patch.p_attack = 0.001f;
    instrumentPresets[158].patch.p_decay = 1.5f;
    instrumentPresets[158].patch.p_sustain = 0.0f;
    instrumentPresets[158].patch.p_release = 0.5f;
    instrumentPresets[158].patch.p_osc2Ratio = 1.335f;  // 5 semitones up (perfect 4th)
    instrumentPresets[158].patch.p_osc2Level = 0.8f;
    instrumentPresets[158].patch.p_osc2Decay = 0.0f;     // same decay as main (follows main env)
    instrumentPresets[158].patch.p_filterCutoff = 0.85f;
    instrumentPresets[158].patch.p_volume = 0.7f;

    // MT-70 Synth Bells — 3 sines: octave+0, octave+2, octave+1 with decay
    instrumentPresets[159].name = "MT70 Bells";
    instrumentPresets[159].patch.p_waveType = WAVE_SINE;
    instrumentPresets[159].patch.p_attack = 0.001f;
    instrumentPresets[159].patch.p_decay = 1.0f;
    instrumentPresets[159].patch.p_sustain = 0.0f;
    instrumentPresets[159].patch.p_release = 0.6f;
    instrumentPresets[159].patch.p_osc2Ratio = 4.0f;    // octave+2 (2 octaves up)
    instrumentPresets[159].patch.p_osc2Level = 0.5f;
    instrumentPresets[159].patch.p_osc2Decay = 4.0f;     // decays faster
    instrumentPresets[159].patch.p_osc3Ratio = 2.0f;    // octave+1 (1 octave up)
    instrumentPresets[159].patch.p_osc3Level = 0.6f;
    instrumentPresets[159].patch.p_osc3Decay = 2.5f;     // medium decay
    instrumentPresets[159].patch.p_filterCutoff = 0.9f;
    instrumentPresets[159].patch.p_volume = 0.6f;

    // MT-70 Cosmic Flute — percussive click, crossfades from high to medium octave sine
    instrumentPresets[160].name = "MT70 Cosmic";
    instrumentPresets[160].patch.p_waveType = WAVE_SINE;
    instrumentPresets[160].patch.p_attack = 0.002f;
    instrumentPresets[160].patch.p_decay = 0.15f;
    instrumentPresets[160].patch.p_sustain = 0.6f;
    instrumentPresets[160].patch.p_release = 0.15f;
    instrumentPresets[160].patch.p_clickLevel = 0.3f;   // percussive click
    instrumentPresets[160].patch.p_clickTime = 0.008f;
    instrumentPresets[160].patch.p_osc2Ratio = 2.0f;    // high octave
    instrumentPresets[160].patch.p_osc2Level = 0.7f;
    instrumentPresets[160].patch.p_osc2Decay = 6.0f;     // fades out, leaving medium octave
    instrumentPresets[160].patch.p_filterCutoff = 0.7f;
    instrumentPresets[160].patch.p_volume = 0.65f;

    // MT-70 Jazz Organ 2 — Hammond with percussive key click
    instrumentPresets[161].name = "MT70 JzOrg2";
    instrumentPresets[161].patch.p_waveType = WAVE_SINE;
    instrumentPresets[161].patch.p_attack = 0.002f;
    instrumentPresets[161].patch.p_decay = 0.1f;
    instrumentPresets[161].patch.p_sustain = 0.75f;
    instrumentPresets[161].patch.p_release = 0.06f;
    instrumentPresets[161].patch.p_clickLevel = 0.4f;   // percussive key click
    instrumentPresets[161].patch.p_clickTime = 0.005f;
    instrumentPresets[161].patch.p_osc2Ratio = 3.0f;    // 3rd harmonic
    instrumentPresets[161].patch.p_osc2Level = 0.35f;
    instrumentPresets[161].patch.p_osc2Decay = 8.0f;     // click partial fades leaving fundamental
    instrumentPresets[161].patch.p_filterCutoff = 0.65f;
    instrumentPresets[161].patch.p_volume = 0.6f;

    // MT-70 Celesta — music box, "bass resembles steel drum"
    instrumentPresets[162].name = "MT70 Celesta";
    instrumentPresets[162].patch.p_waveType = WAVE_SINE;
    instrumentPresets[162].patch.p_attack = 0.001f;
    instrumentPresets[162].patch.p_decay = 0.6f;
    instrumentPresets[162].patch.p_sustain = 0.0f;
    instrumentPresets[162].patch.p_release = 0.3f;
    instrumentPresets[162].patch.p_osc2Ratio = 3.0f;    // 3rd harmonic for brightness
    instrumentPresets[162].patch.p_osc2Level = 0.5f;
    instrumentPresets[162].patch.p_osc2Decay = 4.0f;     // overtone fades faster
    instrumentPresets[162].patch.p_filterCutoff = 0.85f;
    instrumentPresets[162].patch.p_volume = 0.7f;

    // MT-70 Banjo — "hollow and resonant", plucky sine
    instrumentPresets[163].name = "MT70 Banjo";
    instrumentPresets[163].patch.p_waveType = WAVE_SINE;
    instrumentPresets[163].patch.p_attack = 0.001f;
    instrumentPresets[163].patch.p_decay = 0.4f;
    instrumentPresets[163].patch.p_sustain = 0.0f;
    instrumentPresets[163].patch.p_release = 0.15f;
    instrumentPresets[163].patch.p_osc2Ratio = 2.0f;    // octave
    instrumentPresets[163].patch.p_osc2Level = 0.6f;
    instrumentPresets[163].patch.p_osc2Decay = 8.0f;     // octave decays fast = hollow body
    instrumentPresets[163].patch.p_osc3Ratio = 3.0f;    // 3rd for resonance
    instrumentPresets[163].patch.p_osc3Level = 0.3f;
    instrumentPresets[163].patch.p_osc3Decay = 12.0f;    // very fast decay
    instrumentPresets[163].patch.p_filterCutoff = 0.7f;
    instrumentPresets[163].patch.p_volume = 0.65f;

    // ========================================================================
    // MINIMOOG PRESETS (164-169)
    // Minimoog Model D (1970): 3 oscillators → mixer → 4-pole ladder LPF → VCA.
    // Monophonic. The "fat" sound = detuned saws + ladder filter + overdrive.
    // Source: soundonsound.com, avarethtaika.com/minimoog-sound-design
    // ========================================================================

    // Moog Bass — 2 saws + square octave below, ladder filter, plucky decay
    // The classic: deep, fat, punchy. "Two saws in unison with a square an
    // octave below, all at max, into the filter with minimum cutoff, ~75%
    // resonance, 50% mod amount, plucky decay envelopes."
    instrumentPresets[164].name = "Moog Bass";
    instrumentPresets[164].patch.p_waveType = WAVE_SAW;
    instrumentPresets[164].patch.p_attack = 0.002f;
    instrumentPresets[164].patch.p_decay = 0.35f;
    instrumentPresets[164].patch.p_sustain = 0.0f;
    instrumentPresets[164].patch.p_release = 0.06f;
    instrumentPresets[164].patch.p_filterModel = 1;        // ladder (4-pole, Moog-style)
    instrumentPresets[164].patch.p_filterCutoff = 0.15f;   // low cutoff
    instrumentPresets[164].patch.p_filterResonance = 0.55f; // ~75% emphasis
    instrumentPresets[164].patch.p_filterEnvAmt = 0.5f;    // 50% mod amount
    instrumentPresets[164].patch.p_filterEnvDecay = 0.25f;
    instrumentPresets[164].patch.p_osc2Ratio = 1.005f;     // 2nd saw, slightly detuned
    instrumentPresets[164].patch.p_osc2Level = 0.8f;
    instrumentPresets[164].patch.p_osc3Ratio = 0.5f;       // square one octave below
    instrumentPresets[164].patch.p_osc3Level = 0.7f;
    instrumentPresets[164].patch.p_drive = 0.2f;           // warm overdrive into filter
    instrumentPresets[164].patch.p_monoMode = true;
    instrumentPresets[164].patch.p_glideTime = 0.03f;
    instrumentPresets[164].patch.p_volume = 0.7f;

    // Moog Thriller — the Michael Jackson "Thriller" bass
    // Saw osc, tight filter envelope crack, moderate resonance, punchy
    instrumentPresets[165].name = "Moog Thriller";
    instrumentPresets[165].patch.p_waveType = WAVE_SAW;
    instrumentPresets[165].patch.p_attack = 0.001f;
    instrumentPresets[165].patch.p_decay = 0.5f;
    instrumentPresets[165].patch.p_sustain = 0.3f;
    instrumentPresets[165].patch.p_release = 0.08f;
    instrumentPresets[165].patch.p_filterModel = 1;
    instrumentPresets[165].patch.p_filterCutoff = 0.12f;   // ~3/10 cutoff
    instrumentPresets[165].patch.p_filterResonance = 0.35f; // moderate emphasis
    instrumentPresets[165].patch.p_filterEnvAmt = 0.4f;    // 40% — filter crack
    instrumentPresets[165].patch.p_filterEnvDecay = 0.15f;  // quick snap
    instrumentPresets[165].patch.p_osc2Ratio = 0.998f;     // slight detune down
    instrumentPresets[165].patch.p_osc2Level = 0.6f;
    instrumentPresets[165].patch.p_drive = 0.15f;
    instrumentPresets[165].patch.p_monoMode = true;
    instrumentPresets[165].patch.p_glideTime = 0.04f;
    instrumentPresets[165].patch.p_volume = 0.7f;

    // Moog Lead — slow attack saws, filter sweep, prog rock lead
    // "Two saws together, slow attack and decay, filter low cutoff,
    // moderate resonance and mod amount."
    instrumentPresets[166].name = "Moog Lead";
    instrumentPresets[166].patch.p_waveType = WAVE_SAW;
    instrumentPresets[166].patch.p_attack = 0.15f;         // slow attack
    instrumentPresets[166].patch.p_decay = 0.6f;
    instrumentPresets[166].patch.p_sustain = 0.6f;
    instrumentPresets[166].patch.p_release = 0.3f;
    instrumentPresets[166].patch.p_filterModel = 1;
    instrumentPresets[166].patch.p_filterCutoff = 0.25f;
    instrumentPresets[166].patch.p_filterResonance = 0.4f;
    instrumentPresets[166].patch.p_filterEnvAmt = 0.35f;
    instrumentPresets[166].patch.p_filterEnvDecay = 0.5f;
    instrumentPresets[166].patch.p_osc2Ratio = 1.003f;     // detuned saw
    instrumentPresets[166].patch.p_osc2Level = 0.8f;
    instrumentPresets[166].patch.p_drive = 0.1f;
    instrumentPresets[166].patch.p_monoMode = true;
    instrumentPresets[166].patch.p_glideTime = 0.08f;       // smooth portamento
    instrumentPresets[166].patch.p_volume = 0.65f;

    // Moog Sci-Fi — self-oscillating filter sweep, no oscillators
    // "All oscillators off, filter 50% cutoff, maximum resonance, short plucky
    // envelope, keytrack enabled."
    instrumentPresets[167].name = "Moog Sci-Fi";
    instrumentPresets[167].patch.p_waveType = WAVE_SINE;   // quiet fundamental
    instrumentPresets[167].patch.p_attack = 0.001f;
    instrumentPresets[167].patch.p_decay = 0.3f;
    instrumentPresets[167].patch.p_sustain = 0.0f;
    instrumentPresets[167].patch.p_release = 0.2f;
    instrumentPresets[167].patch.p_filterModel = 1;
    instrumentPresets[167].patch.p_filterCutoff = 0.5f;
    instrumentPresets[167].patch.p_filterResonance = 0.95f; // max — self-oscillation
    instrumentPresets[167].patch.p_filterEnvAmt = 0.6f;
    instrumentPresets[167].patch.p_filterEnvDecay = 0.2f;
    instrumentPresets[167].patch.p_volume = 0.5f;

    // Moog Vocal — sawtooth + triangle modulating filter, vowel-like
    // "VCO1: 32' saw at max. VCO3: 2-4' triangle modulates filter ~75%.
    // Filter: 25% cutoff, 60% resonance, tiny mod from moderate filter env."
    instrumentPresets[168].name = "Moog Vocal";
    instrumentPresets[168].patch.p_waveType = WAVE_SAW;
    instrumentPresets[168].patch.p_attack = 0.01f;
    instrumentPresets[168].patch.p_decay = 0.2f;
    instrumentPresets[168].patch.p_sustain = 0.8f;
    instrumentPresets[168].patch.p_release = 0.15f;
    instrumentPresets[168].patch.p_filterModel = 1;
    instrumentPresets[168].patch.p_filterCutoff = 0.25f;
    instrumentPresets[168].patch.p_filterResonance = 0.6f;
    instrumentPresets[168].patch.p_filterEnvAmt = 0.15f;    // tiny
    instrumentPresets[168].patch.p_filterEnvDecay = 0.4f;
    instrumentPresets[168].patch.p_filterLfoRate = 4.5f;     // osc3 as slow modulator
    instrumentPresets[168].patch.p_filterLfoDepth = 0.35f;   // modulates filter ~75% depth
    instrumentPresets[168].patch.p_monoMode = true;
    instrumentPresets[168].patch.p_volume = 0.65f;

    // Moog Sub — deep sub-bass, triangle + saw one octave below
    // For when you need floor-shaking low end
    instrumentPresets[169].name = "Moog Sub";
    instrumentPresets[169].patch.p_waveType = WAVE_TRIANGLE;
    instrumentPresets[169].patch.p_attack = 0.005f;
    instrumentPresets[169].patch.p_decay = 0.2f;
    instrumentPresets[169].patch.p_sustain = 0.7f;
    instrumentPresets[169].patch.p_release = 0.1f;
    instrumentPresets[169].patch.p_filterModel = 1;
    instrumentPresets[169].patch.p_filterCutoff = 0.2f;
    instrumentPresets[169].patch.p_filterResonance = 0.2f;  // gentle
    instrumentPresets[169].patch.p_filterEnvAmt = 0.15f;
    instrumentPresets[169].patch.p_filterEnvDecay = 0.15f;
    instrumentPresets[169].patch.p_osc2Ratio = 0.5f;        // saw one octave below
    instrumentPresets[169].patch.p_osc2Level = 0.5f;
    instrumentPresets[169].patch.p_drive = 0.1f;
    instrumentPresets[169].patch.p_monoMode = true;
    instrumentPresets[169].patch.p_glideTime = 0.02f;
    instrumentPresets[169].patch.p_volume = 0.7f;

    // ========================================================================
    // OBERHEIM OB-Xa PRESETS (170-173)
    // Oberheim OB-Xa (1981): 2 VCOs (CEM3340, saw/pulse) → switchable 2/4-pole
    // filter (CEM3320) → VCA. 4/6/8-voice poly. Famous for brass stabs & pads.
    // Source: syntorial.com, musictech.com, presetpatch.com
    // ========================================================================

    // OBXa Brass — the "Jump" brass stab. 2 detuned saws, bright, punchy.
    // "Saw, 4 unison voices, 9 cents detune, cutoff 85%, resonance 25%,
    // attack 0, decay 10s, sustain 0%, release 80ms."
    instrumentPresets[170].name = "OBXa Brass";
    instrumentPresets[170].patch.p_waveType = WAVE_SAW;
    instrumentPresets[170].patch.p_attack = 0.001f;
    instrumentPresets[170].patch.p_decay = 2.0f;           // long decay
    instrumentPresets[170].patch.p_sustain = 0.75f;
    instrumentPresets[170].patch.p_release = 0.08f;
    instrumentPresets[170].patch.p_filterCutoff = 0.85f;   // bright
    instrumentPresets[170].patch.p_filterResonance = 0.25f;
    instrumentPresets[170].patch.p_unisonCount = 4;
    instrumentPresets[170].patch.p_unisonDetune = 9.0f;    // 9 cents
    instrumentPresets[170].patch.p_unisonMix = 0.7f;
    instrumentPresets[170].patch.p_volume = 0.6f;

    // OBXa Pad — lush detuned pad, 2-pole (SVF) filter for warmth
    // The OB-Xa's 2-pole mode gave smoother, warmer pads than 4-pole.
    instrumentPresets[171].name = "OBXa Pad";
    instrumentPresets[171].patch.p_waveType = WAVE_SAW;
    instrumentPresets[171].patch.p_attack = 0.3f;
    instrumentPresets[171].patch.p_decay = 0.5f;
    instrumentPresets[171].patch.p_sustain = 0.7f;
    instrumentPresets[171].patch.p_release = 0.6f;
    instrumentPresets[171].patch.p_filterModel = 0;        // SVF (2-pole, OB-Xa 12dB mode)
    instrumentPresets[171].patch.p_filterCutoff = 0.5f;
    instrumentPresets[171].patch.p_filterResonance = 0.15f;
    instrumentPresets[171].patch.p_filterEnvAmt = 0.2f;
    instrumentPresets[171].patch.p_filterEnvDecay = 0.8f;
    instrumentPresets[171].patch.p_osc2Ratio = 1.004f;     // slight detune
    instrumentPresets[171].patch.p_osc2Level = 0.7f;
    instrumentPresets[171].patch.p_volume = 0.6f;

    // OBXa Stab — short punchy brass stab, 4-pole for bite
    instrumentPresets[172].name = "OBXa Stab";
    instrumentPresets[172].patch.p_waveType = WAVE_SAW;
    instrumentPresets[172].patch.p_attack = 0.001f;
    instrumentPresets[172].patch.p_decay = 0.25f;
    instrumentPresets[172].patch.p_sustain = 0.0f;
    instrumentPresets[172].patch.p_release = 0.05f;
    instrumentPresets[172].patch.p_filterModel = 1;        // ladder (4-pole, OB-Xa 24dB mode)
    instrumentPresets[172].patch.p_filterCutoff = 0.3f;
    instrumentPresets[172].patch.p_filterResonance = 0.2f;
    instrumentPresets[172].patch.p_filterEnvAmt = 0.6f;    // strong filter snap
    instrumentPresets[172].patch.p_filterEnvDecay = 0.15f;
    instrumentPresets[172].patch.p_unisonCount = 2;
    instrumentPresets[172].patch.p_unisonDetune = 12.0f;
    instrumentPresets[172].patch.p_unisonMix = 0.6f;
    instrumentPresets[172].patch.p_volume = 0.65f;

    // OBXa Strings — the lush Oberheim string pad, slow attack, warm filter
    instrumentPresets[173].name = "OBXa Strings";
    instrumentPresets[173].patch.p_waveType = WAVE_SAW;
    instrumentPresets[173].patch.p_attack = 0.5f;
    instrumentPresets[173].patch.p_decay = 0.3f;
    instrumentPresets[173].patch.p_sustain = 0.8f;
    instrumentPresets[173].patch.p_release = 0.8f;
    instrumentPresets[173].patch.p_filterModel = 0;        // SVF (2-pole, smooth)
    instrumentPresets[173].patch.p_filterCutoff = 0.45f;
    instrumentPresets[173].patch.p_filterResonance = 0.1f;
    instrumentPresets[173].patch.p_filterEnvAmt = 0.15f;
    instrumentPresets[173].patch.p_filterEnvDecay = 1.0f;
    instrumentPresets[173].patch.p_osc2Ratio = 0.998f;     // detuned down
    instrumentPresets[173].patch.p_osc2Level = 0.8f;
    instrumentPresets[173].patch.p_osc3Ratio = 2.002f;     // octave up, slight detune
    instrumentPresets[173].patch.p_osc3Level = 0.3f;
    instrumentPresets[173].patch.p_volume = 0.55f;

    // ========================================================================
    // FENDER RHODES PRESETS (174-176)
    // Fender Rhodes (1965): hammer → tine+tone bar (tuning fork) → pickup.
    // Tine partials are inharmonic: ~1×, ~7×, ~21× fundamental.
    // Soft playing = warm sine body. Hard playing = bright bell attack.
    // Source: wikipedia, modwiggler, soundonsound "Synthesizing Pianos"
    // ========================================================================

    // Rhodes Warm — classic mellow Rhodes via WAVE_EPIANO engine
    // Centered pickup, soft hammer, moderate decay — warm and round
    // bellTone kept low: mostly integer pickup harmonics, hint of cantilever inharmonicity
    instrumentPresets[174].name = "Rhodes Warm";
    instrumentPresets[174].patch.p_waveType = WAVE_EPIANO;
    instrumentPresets[174].patch.p_attack = 0.003f;
    instrumentPresets[174].patch.p_decay = 1.5f;
    instrumentPresets[174].patch.p_sustain = 0.4f;
    instrumentPresets[174].patch.p_release = 0.4f;
    instrumentPresets[174].patch.p_epHardness = 0.3f;      // soft hammer — mellow attack
    instrumentPresets[174].patch.p_epToneBar = 0.6f;       // good fundamental sustain
    instrumentPresets[174].patch.p_epPickupPos = 0.15f;    // near center — warm, fundamental-heavy
    instrumentPresets[174].patch.p_epPickupDist = 0.3f;    // moderate distance — gentle bark
    instrumentPresets[174].patch.p_epDecay = 3.5f;         // long decay
    instrumentPresets[174].patch.p_epBell = 0.3f;          // subtle bell
    instrumentPresets[174].patch.p_epBellTone = 0.08f;    // mostly harmonic pickup tone
    instrumentPresets[174].patch.p_clickLevel = 0.1f;      // gentle hammer thump
    instrumentPresets[174].patch.p_clickTime = 0.004f;
    instrumentPresets[174].patch.p_filterCutoff = 0.55f;
    instrumentPresets[174].patch.p_volume = 0.65f;

    // Rhodes Bright — funky, barking Rhodes via WAVE_EPIANO engine
    // Offset pickup, harder hammer — 2nd harmonic dominant, bark on hard hits
    instrumentPresets[175].name = "Rhodes Bright";
    instrumentPresets[175].patch.p_waveType = WAVE_EPIANO;
    instrumentPresets[175].patch.p_attack = 0.001f;
    instrumentPresets[175].patch.p_decay = 1.0f;
    instrumentPresets[175].patch.p_sustain = 0.3f;
    instrumentPresets[175].patch.p_release = 0.3f;
    instrumentPresets[175].patch.p_epHardness = 0.7f;      // hard hammer — bright attack
    instrumentPresets[175].patch.p_epToneBar = 0.3f;       // shorter fundamental
    instrumentPresets[175].patch.p_epPickupPos = 0.8f;     // offset — 2nd harmonic dominant
    instrumentPresets[175].patch.p_epPickupDist = 0.7f;    // close pickup — heavy bark
    instrumentPresets[175].patch.p_epDecay = 2.5f;
    instrumentPresets[175].patch.p_epBell = 0.6f;          // prominent bell
    instrumentPresets[175].patch.p_epBellTone = 0.15f;    // touch of cantilever inharmonicity
    instrumentPresets[175].patch.p_velToDrive = 1.5f;     // bark on hard hits via drive circuit
    instrumentPresets[175].patch.p_velToFilter = 0.3f;    // opens up on hard hits
    instrumentPresets[175].patch.p_driveMode = DIST_ASYMMETRIC;
    instrumentPresets[175].patch.p_clickLevel = 0.2f;      // pronounced hammer
    instrumentPresets[175].patch.p_clickTime = 0.003f;
    instrumentPresets[175].patch.p_filterCutoff = 0.65f;
    instrumentPresets[175].patch.p_volume = 0.6f;

    // Rhodes Suitcase — through the suitcase amp with tremolo
    instrumentPresets[176].name = "Rhodes Suite";
    instrumentPresets[176].patch.p_waveType = WAVE_EPIANO;
    instrumentPresets[176].patch.p_attack = 0.002f;
    instrumentPresets[176].patch.p_decay = 1.8f;
    instrumentPresets[176].patch.p_sustain = 0.35f;
    instrumentPresets[176].patch.p_release = 0.5f;
    instrumentPresets[176].patch.p_epHardness = 0.35f;     // medium-soft
    instrumentPresets[176].patch.p_epToneBar = 0.55f;
    instrumentPresets[176].patch.p_epPickupPos = 0.25f;    // slightly warm
    instrumentPresets[176].patch.p_epPickupDist = 0.35f;
    instrumentPresets[176].patch.p_epDecay = 4.0f;         // long sustain
    instrumentPresets[176].patch.p_epBell = 0.35f;
    instrumentPresets[176].patch.p_epBellTone = 0.1f;     // mostly harmonic, warm sustain
    instrumentPresets[176].patch.p_clickLevel = 0.12f;
    instrumentPresets[176].patch.p_clickTime = 0.004f;
    instrumentPresets[176].patch.p_filterCutoff = 0.5f;
    instrumentPresets[176].patch.p_ampLfoRate = 4.5f;      // suitcase tremolo
    instrumentPresets[176].patch.p_ampLfoDepth = 0.2f;
    instrumentPresets[176].patch.p_volume = 0.6f;

    // Rhodes Bark — extreme dynamic range: glass bell soft, savage bark hard
    // Bell character from bellTone (cantilever beam inharmonicity) + bell (upper mode emphasis).
    // Bark from velToDrive (velocity drives asymmetric distortion) + close pickup nonlinearity.
    // Soft hits = shimmering vibes-like bell. Hard hits = growling, barking Rhodes.
    instrumentPresets[177].name = "Rhodes Bark";
    instrumentPresets[177].patch.p_waveType = WAVE_EPIANO;
    instrumentPresets[177].patch.p_attack = 0.001f;
    instrumentPresets[177].patch.p_decay = 1.2f;
    instrumentPresets[177].patch.p_sustain = 0.3f;
    instrumentPresets[177].patch.p_release = 0.3f;
    instrumentPresets[177].patch.p_epHardness = 0.65f;     // medium-hard — bell needs some softness
    instrumentPresets[177].patch.p_epToneBar = 0.35f;      // moderate tone bar — sustains the bell
    instrumentPresets[177].patch.p_epPickupPos = 0.55f;    // between centered/offset — bell+bark balance
    instrumentPresets[177].patch.p_epPickupDist = 0.5f;    // moderate — baseline nonlinearity is stronger now
    instrumentPresets[177].patch.p_epDecay = 3.0f;         // longer for bell sustain
    instrumentPresets[177].patch.p_epBell = 0.7f;          // strong bell emphasis
    instrumentPresets[177].patch.p_epBellTone = 0.2f;      // inharmonic shimmer
    instrumentPresets[177].patch.p_velToDrive = 4.0f;      // bark comes from drive circuit, not pickup dist
    instrumentPresets[177].patch.p_velToFilter = 0.5f;     // filter opens wide on hard hits
    instrumentPresets[177].patch.p_velToClick = 0.6f;      // click scales with velocity
    instrumentPresets[177].patch.p_drive = 0.0f;           // clean at soft velocity
    instrumentPresets[177].patch.p_driveMode = DIST_ASYMMETRIC; // even harmonics (pickup character)
    instrumentPresets[177].patch.p_clickLevel = 0.2f;
    instrumentPresets[177].patch.p_clickTime = 0.003f;     // slightly longer click for bell
    instrumentPresets[177].patch.p_filterCutoff = 0.35f;   // darker at rest — bell is mellow
    instrumentPresets[177].patch.p_volume = 0.55f;

    // Wurli Buzz — Supertramp "Dreamer" style: driven, nasal, buzzy, tremolo
    // The 200A cranked into its built-in amp — reedy overdrive on every note,
    // strong odd harmonics, the quintessential 70s prog Wurlitzer bark.
    instrumentPresets[178].name = "Wurli Buzz";
    instrumentPresets[178].patch.p_waveType = WAVE_EPIANO;
    instrumentPresets[178].patch.p_epPickupType = EP_PICKUP_ELECTROSTATIC;
    instrumentPresets[178].patch.p_epHardness = 0.65f;      // harder strike — more overtones
    instrumentPresets[178].patch.p_epToneBar = 0.0f;        // no tone bar
    instrumentPresets[178].patch.p_epPickupPos = 0.6f;      // offset — buzzy character
    instrumentPresets[178].patch.p_epPickupDist = 0.8f;     // cranked — heavy odd-harmonic distortion
    instrumentPresets[178].patch.p_epDecay = 1.5f;          // punchy, not too long
    instrumentPresets[178].patch.p_epBell = 0.1f;           // almost no bell
    instrumentPresets[178].patch.p_epBellTone = 0.02f;      // pure reed harmonics
    instrumentPresets[178].patch.p_attack = 0.001f;
    instrumentPresets[178].patch.p_decay = 0.7f;
    instrumentPresets[178].patch.p_sustain = 0.35f;
    instrumentPresets[178].patch.p_release = 0.2f;
    instrumentPresets[178].patch.p_clickLevel = 0.2f;       // reed snap
    instrumentPresets[178].patch.p_clickTime = 0.002f;
    instrumentPresets[178].patch.p_drive = 0.3f;            // always a bit driven
    instrumentPresets[178].patch.p_velToDrive = 0.5f;       // more bark on hard hits
    instrumentPresets[178].patch.p_filterEnabled = true;
    instrumentPresets[178].patch.p_filterCutoff = 0.5f;     // slightly nasal
    instrumentPresets[178].patch.p_filterResonance = 0.15f; // hint of resonance for that honk
    instrumentPresets[178].patch.p_filterKeyTrack = 0.5f;
    instrumentPresets[178].patch.p_filterEnvAmt = 0.2f;     // opens on attack
    instrumentPresets[178].patch.p_filterEnvDecay = 0.15f;
    instrumentPresets[178].patch.p_vibratoRate = 5.8f;      // 200A tremolo
    instrumentPresets[178].patch.p_vibratoDepth = 0.08f;    // noticeable wobble
    instrumentPresets[178].patch.p_volume = 0.5f;

    // Wurli Soul — Ray Charles / Donny Hathaway / "What'd I Say" style:
    // warm, round, gentle — the mellow side of the Wurlitzer.
    // Clean at soft dynamics, growls when you dig in.
    instrumentPresets[179].name = "Wurli Soul";
    instrumentPresets[179].patch.p_waveType = WAVE_EPIANO;
    instrumentPresets[179].patch.p_epPickupType = EP_PICKUP_ELECTROSTATIC;
    instrumentPresets[179].patch.p_epHardness = 0.35f;      // soft touch — round tone
    instrumentPresets[179].patch.p_epToneBar = 0.0f;        // no tone bar
    instrumentPresets[179].patch.p_epPickupPos = 0.3f;      // centered — warm fundamental
    instrumentPresets[179].patch.p_epPickupDist = 0.4f;     // moderate — clean unless pushed
    instrumentPresets[179].patch.p_epDecay = 2.2f;          // slightly longer for ballads
    instrumentPresets[179].patch.p_epBell = 0.2f;           // touch of shimmer
    instrumentPresets[179].patch.p_epBellTone = 0.03f;      // nearly pure harmonics
    instrumentPresets[179].patch.p_attack = 0.003f;
    instrumentPresets[179].patch.p_decay = 1.0f;
    instrumentPresets[179].patch.p_sustain = 0.4f;
    instrumentPresets[179].patch.p_release = 0.35f;
    instrumentPresets[179].patch.p_clickLevel = 0.08f;      // gentle reed click
    instrumentPresets[179].patch.p_clickTime = 0.003f;
    instrumentPresets[179].patch.p_drive = 0.0f;            // clean at rest
    instrumentPresets[179].patch.p_velToDrive = 0.6f;       // dig in = growl
    instrumentPresets[179].patch.p_filterEnabled = true;
    instrumentPresets[179].patch.p_filterCutoff = 0.65f;    // open, warm
    instrumentPresets[179].patch.p_filterResonance = 0.05f; // no honk
    instrumentPresets[179].patch.p_filterKeyTrack = 0.35f;
    instrumentPresets[179].patch.p_vibratoRate = 6.2f;      // gentle tremolo
    instrumentPresets[179].patch.p_vibratoDepth = 0.04f;    // subtle
    instrumentPresets[179].patch.p_volume = 0.55f;

    // ========================================================================
    // CLAVINET PRESETS (180-182)
    // Hohner Clavinet D6 (1971): tangent strikes string from below, yarn-wound
    // damper pad releases. Two single-coil pickups (neck/bridge) under strings.
    // Short steel strings = nearly harmonic partials with stiffness stretch.
    // Character: bright, percussive, funky. Fast decay, no sustain.
    // Source: Sound On Sound "Synth Secrets: Clavinet", Vintage Vibe tech docs
    // ========================================================================

    // Clav Funky — the classic Stevie Wonder / Herbie Hancock funk sound.
    // Bridge pickup, hard attack, wah-style filter envelope. "Superstition" tone.
    instrumentPresets[180].name = "Clav Funky";
    instrumentPresets[180].patch.p_waveType = WAVE_EPIANO;
    instrumentPresets[180].patch.p_epPickupType = EP_PICKUP_CONTACT;
    instrumentPresets[180].patch.p_epHardness = 0.8f;       // hard tangent strike — bright attack
    instrumentPresets[180].patch.p_epToneBar = 0.0f;        // no tone bar (not a Rhodes)
    instrumentPresets[180].patch.p_epPickupPos = 0.75f;     // bridge pickup — snappy, bright
    instrumentPresets[180].patch.p_epPickupDist = 0.6f;     // moderate — funky honk
    instrumentPresets[180].patch.p_epDecay = 1.0f;          // short decay
    instrumentPresets[180].patch.p_epBell = 0.15f;          // touch of damper shimmer
    instrumentPresets[180].patch.p_epBellTone = 0.3f;       // some damper pad inharmonicity
    instrumentPresets[180].patch.p_attack = 0.001f;
    instrumentPresets[180].patch.p_decay = 0.5f;
    instrumentPresets[180].patch.p_sustain = 0.25f;
    instrumentPresets[180].patch.p_release = 0.12f;         // fast release — percussive
    instrumentPresets[180].patch.p_clickLevel = 0.35f;      // tangent pluck click
    instrumentPresets[180].patch.p_clickTime = 0.002f;
    instrumentPresets[180].patch.p_drive = 0.1f;            // slight edge
    instrumentPresets[180].patch.p_velToDrive = 1.0f;       // dig in = more funk
    instrumentPresets[180].patch.p_velToClick = 0.5f;       // harder = clickier
    instrumentPresets[180].patch.p_filterEnabled = true;
    instrumentPresets[180].patch.p_filterCutoff = 0.5f;     // wah territory
    instrumentPresets[180].patch.p_filterResonance = 0.3f;  // honky resonance
    instrumentPresets[180].patch.p_filterKeyTrack = 0.6f;   // opens for high notes
    instrumentPresets[180].patch.p_filterEnvAmt = 0.4f;     // wah sweep on attack
    instrumentPresets[180].patch.p_filterEnvDecay = 0.1f;   // fast filter snap
    instrumentPresets[180].patch.p_volume = 0.6f;

    // Clav Mellow — neck pickup, softer touch. Bill Withers / reggae comping.
    // Warm and round but still percussive — the "rhythm guitar" Clavinet sound.
    instrumentPresets[181].name = "Clav Mellow";
    instrumentPresets[181].patch.p_waveType = WAVE_EPIANO;
    instrumentPresets[181].patch.p_epPickupType = EP_PICKUP_CONTACT;
    instrumentPresets[181].patch.p_epHardness = 0.45f;      // softer touch — warmer
    instrumentPresets[181].patch.p_epToneBar = 0.0f;
    instrumentPresets[181].patch.p_epPickupPos = 0.25f;     // neck pickup — warm, round
    instrumentPresets[181].patch.p_epPickupDist = 0.35f;    // gentle nonlinearity
    instrumentPresets[181].patch.p_epDecay = 1.4f;          // slightly longer
    instrumentPresets[181].patch.p_epBell = 0.1f;           // minimal shimmer
    instrumentPresets[181].patch.p_epBellTone = 0.15f;      // mostly harmonic
    instrumentPresets[181].patch.p_attack = 0.002f;
    instrumentPresets[181].patch.p_decay = 0.8f;
    instrumentPresets[181].patch.p_sustain = 0.3f;
    instrumentPresets[181].patch.p_release = 0.18f;
    instrumentPresets[181].patch.p_clickLevel = 0.15f;      // soft pluck
    instrumentPresets[181].patch.p_clickTime = 0.003f;
    instrumentPresets[181].patch.p_drive = 0.0f;
    instrumentPresets[181].patch.p_velToDrive = 0.4f;       // mild growl on hard hits
    instrumentPresets[181].patch.p_filterEnabled = true;
    instrumentPresets[181].patch.p_filterCutoff = 0.55f;    // warm but not dark
    instrumentPresets[181].patch.p_filterResonance = 0.1f;  // no honk
    instrumentPresets[181].patch.p_filterKeyTrack = 0.4f;
    instrumentPresets[181].patch.p_volume = 0.6f;

    // Clav Driven — both pickups cranked through an amp. Led Zeppelin / funk-rock.
    // Aggressive, distorted, almost guitar-like. "Trampled Under Foot" territory.
    instrumentPresets[182].name = "Clav Driven";
    instrumentPresets[182].patch.p_waveType = WAVE_EPIANO;
    instrumentPresets[182].patch.p_epPickupType = EP_PICKUP_CONTACT;
    instrumentPresets[182].patch.p_epHardness = 0.85f;      // hard strike — maximum brightness
    instrumentPresets[182].patch.p_epToneBar = 0.0f;
    instrumentPresets[182].patch.p_epPickupPos = 0.5f;      // both pickups (middle)
    instrumentPresets[182].patch.p_epPickupDist = 0.85f;    // close — heavy nonlinearity
    instrumentPresets[182].patch.p_epDecay = 0.8f;          // short and punchy
    instrumentPresets[182].patch.p_epBell = 0.25f;          // damper character
    instrumentPresets[182].patch.p_epBellTone = 0.4f;       // inharmonic edge
    instrumentPresets[182].patch.p_attack = 0.001f;
    instrumentPresets[182].patch.p_decay = 0.4f;
    instrumentPresets[182].patch.p_sustain = 0.2f;
    instrumentPresets[182].patch.p_release = 0.1f;          // very fast release
    instrumentPresets[182].patch.p_clickLevel = 0.4f;       // aggressive pluck
    instrumentPresets[182].patch.p_clickTime = 0.002f;
    instrumentPresets[182].patch.p_drive = 0.25f;           // always driven
    instrumentPresets[182].patch.p_velToDrive = 2.0f;       // hard hits = overdrive
    instrumentPresets[182].patch.p_velToClick = 0.7f;       // snappy at hard dynamics
    instrumentPresets[182].patch.p_driveMode = DIST_ASYMMETRIC;
    instrumentPresets[182].patch.p_filterEnabled = true;
    instrumentPresets[182].patch.p_filterCutoff = 0.65f;    // open — let the grit through
    instrumentPresets[182].patch.p_filterResonance = 0.2f;  // slight edge
    instrumentPresets[182].patch.p_filterKeyTrack = 0.5f;
    instrumentPresets[182].patch.p_filterEnvAmt = 0.25f;    // attack opens filter
    instrumentPresets[182].patch.p_filterEnvDecay = 0.08f;  // fast snap
    instrumentPresets[182].patch.p_volume = 0.55f;

    // ========================================================================
    // ORGAN PRESETS (Hammond drawbar registrations) — WAVE_ORGAN
    // ========================================================================
    // Drawbar order: 16' 5⅓' 8' 4' 2⅔' 2' 1⅗' 1⅓' 1'
    // Levels 0-8 mapped to 0.0-1.0 (divided by 8)

    // Jimmy Smith Jazz — fat sub + fundamental, no upper harmonics
    // Classic jazz organ: warm, round, slightly overdriven through Leslie
    instrumentPresets[183].name = "Jimmy Smith";
    instrumentPresets[183].patch.p_waveType = WAVE_ORGAN;
    instrumentPresets[183].patch.p_attack = 0.001f;
    instrumentPresets[183].patch.p_decay = 0.0f;
    instrumentPresets[183].patch.p_sustain = 1.0f;
    instrumentPresets[183].patch.p_release = 0.05f;
    instrumentPresets[183].patch.p_orgDrawbar[0] = 1.0f;    // 16' = 8
    instrumentPresets[183].patch.p_orgDrawbar[1] = 1.0f;    // 5⅓' = 8
    instrumentPresets[183].patch.p_orgDrawbar[2] = 1.0f;    // 8' = 8
    instrumentPresets[183].patch.p_orgClick = 0.2f;
    instrumentPresets[183].patch.p_orgPercOn = 1;
    instrumentPresets[183].patch.p_orgPercHarmonic = 0;
    instrumentPresets[183].patch.p_orgPercFast = 1;
    instrumentPresets[183].patch.p_volume = 0.55f;
    instrumentPresets[183].patch.p_orgVibratoMode = 6;  // C3 — Smith always ran full chorus

    // Gospel Full — all drawbars maxed, massive choir sound
    instrumentPresets[184].name = "Gospel Full";
    instrumentPresets[184].patch.p_waveType = WAVE_ORGAN;
    instrumentPresets[184].patch.p_attack = 0.001f;
    instrumentPresets[184].patch.p_sustain = 1.0f;
    instrumentPresets[184].patch.p_release = 0.05f;
    for (int i = 0; i < ORGAN_DRAWBARS; i++)
        instrumentPresets[184].patch.p_orgDrawbar[i] = 1.0f;  // 888888888
    instrumentPresets[184].patch.p_orgClick = 0.4f;
    instrumentPresets[184].patch.p_orgPercOn = 1;
    instrumentPresets[184].patch.p_orgPercHarmonic = 0;
    instrumentPresets[184].patch.p_orgPercFast = 1;
    instrumentPresets[184].patch.p_orgCrosstalk = 0.3f;
    instrumentPresets[184].patch.p_volume = 0.45f;
    instrumentPresets[184].patch.p_orgVibratoMode = 6;  // C3 — full shimmer

    // Jon Lord Rock — Deep Purple growl, 888600000
    instrumentPresets[185].name = "Jon Lord Rock";
    instrumentPresets[185].patch.p_waveType = WAVE_ORGAN;
    instrumentPresets[185].patch.p_attack = 0.001f;
    instrumentPresets[185].patch.p_sustain = 1.0f;
    instrumentPresets[185].patch.p_release = 0.05f;
    instrumentPresets[185].patch.p_orgDrawbar[0] = 1.0f;     // 16' = 8
    instrumentPresets[185].patch.p_orgDrawbar[1] = 1.0f;     // 5⅓' = 8
    instrumentPresets[185].patch.p_orgDrawbar[2] = 1.0f;     // 8' = 8
    instrumentPresets[185].patch.p_orgDrawbar[3] = 0.75f;    // 4' = 6
    instrumentPresets[185].patch.p_orgClick = 0.5f;
    instrumentPresets[185].patch.p_orgCrosstalk = 0.4f;
    instrumentPresets[185].patch.p_drive = 0.15f;
    instrumentPresets[185].patch.p_volume = 0.50f;
    instrumentPresets[185].patch.p_orgVibratoMode = 3;  // V3 — vibrato for the growl

    // Booker T Green — Green Onions, 886000000
    instrumentPresets[186].name = "Booker T Green";
    instrumentPresets[186].patch.p_waveType = WAVE_ORGAN;
    instrumentPresets[186].patch.p_attack = 0.001f;
    instrumentPresets[186].patch.p_sustain = 1.0f;
    instrumentPresets[186].patch.p_release = 0.05f;
    instrumentPresets[186].patch.p_orgDrawbar[0] = 1.0f;     // 16' = 8
    instrumentPresets[186].patch.p_orgDrawbar[1] = 1.0f;     // 5⅓' = 8
    instrumentPresets[186].patch.p_orgDrawbar[2] = 0.75f;    // 8' = 6
    instrumentPresets[186].patch.p_orgClick = 0.15f;
    instrumentPresets[186].patch.p_volume = 0.55f;
    instrumentPresets[186].patch.p_orgVibratoMode = 4;  // C1 — subtle 60s clean

    // Ballad — sub + fundamental + shimmer, 808000004
    instrumentPresets[187].name = "Organ Ballad";
    instrumentPresets[187].patch.p_waveType = WAVE_ORGAN;
    instrumentPresets[187].patch.p_attack = 0.001f;
    instrumentPresets[187].patch.p_sustain = 1.0f;
    instrumentPresets[187].patch.p_release = 0.1f;
    instrumentPresets[187].patch.p_orgDrawbar[0] = 1.0f;     // 16' = 8
    instrumentPresets[187].patch.p_orgDrawbar[2] = 1.0f;     // 8' = 8
    instrumentPresets[187].patch.p_orgDrawbar[8] = 0.5f;     // 1' = 4
    instrumentPresets[187].patch.p_orgPercOn = 1;
    instrumentPresets[187].patch.p_orgPercHarmonic = 1;       // 3rd harmonic
    instrumentPresets[187].patch.p_orgPercSoft = 1;
    instrumentPresets[187].patch.p_volume = 0.55f;
    instrumentPresets[187].patch.p_orgVibratoMode = 5;  // C2 — medium warmth

    // Reggae Bubble — skanky upstroke organ, 006060000
    instrumentPresets[188].name = "Reggae Bubble";
    instrumentPresets[188].patch.p_waveType = WAVE_ORGAN;
    instrumentPresets[188].patch.p_attack = 0.001f;
    instrumentPresets[188].patch.p_sustain = 1.0f;
    instrumentPresets[188].patch.p_release = 0.03f;
    instrumentPresets[188].patch.p_orgDrawbar[2] = 0.75f;    // 8' = 6
    instrumentPresets[188].patch.p_orgDrawbar[4] = 0.75f;    // 2⅔' = 6
    instrumentPresets[188].patch.p_volume = 0.55f;

    // Larry Young — Unity-era jazz, 888800000
    instrumentPresets[189].name = "Larry Young";
    instrumentPresets[189].patch.p_waveType = WAVE_ORGAN;
    instrumentPresets[189].patch.p_attack = 0.001f;
    instrumentPresets[189].patch.p_sustain = 1.0f;
    instrumentPresets[189].patch.p_release = 0.05f;
    instrumentPresets[189].patch.p_orgDrawbar[0] = 1.0f;     // 16' = 8
    instrumentPresets[189].patch.p_orgDrawbar[1] = 1.0f;     // 5⅓' = 8
    instrumentPresets[189].patch.p_orgDrawbar[2] = 1.0f;     // 8' = 8
    instrumentPresets[189].patch.p_orgDrawbar[3] = 1.0f;     // 4' = 8
    instrumentPresets[189].patch.p_orgClick = 0.3f;
    instrumentPresets[189].patch.p_orgPercOn = 1;
    instrumentPresets[189].patch.p_orgPercHarmonic = 1;       // 3rd harmonic
    instrumentPresets[189].patch.p_orgPercFast = 1;
    instrumentPresets[189].patch.p_volume = 0.50f;
    instrumentPresets[189].patch.p_orgVibratoMode = 5;  // C2 — modern jazz

    // Keith Emerson — prog bombast, 888808008
    instrumentPresets[190].name = "Emerson Prog";
    instrumentPresets[190].patch.p_waveType = WAVE_ORGAN;
    instrumentPresets[190].patch.p_attack = 0.001f;
    instrumentPresets[190].patch.p_sustain = 1.0f;
    instrumentPresets[190].patch.p_release = 0.05f;
    instrumentPresets[190].patch.p_orgDrawbar[0] = 1.0f;     // 16' = 8
    instrumentPresets[190].patch.p_orgDrawbar[1] = 1.0f;     // 5⅓' = 8
    instrumentPresets[190].patch.p_orgDrawbar[2] = 1.0f;     // 8' = 8
    instrumentPresets[190].patch.p_orgDrawbar[3] = 1.0f;     // 4' = 8
    instrumentPresets[190].patch.p_orgDrawbar[5] = 1.0f;     // 2' = 8
    instrumentPresets[190].patch.p_orgDrawbar[8] = 1.0f;     // 1' = 8
    instrumentPresets[190].patch.p_orgClick = 0.5f;
    instrumentPresets[190].patch.p_orgPercOn = 1;
    instrumentPresets[190].patch.p_orgPercFast = 0;           // slow percussion
    instrumentPresets[190].patch.p_orgCrosstalk = 0.3f;
    instrumentPresets[190].patch.p_drive = 0.1f;
    instrumentPresets[190].patch.p_volume = 0.45f;
    instrumentPresets[190].patch.p_orgVibratoMode = 3;  // V3 — heavy vibrato

    // Soft Combo — cocktail lounge, 006600400
    instrumentPresets[191].name = "Soft Combo";
    instrumentPresets[191].patch.p_waveType = WAVE_ORGAN;
    instrumentPresets[191].patch.p_attack = 0.001f;
    instrumentPresets[191].patch.p_sustain = 1.0f;
    instrumentPresets[191].patch.p_release = 0.08f;
    instrumentPresets[191].patch.p_orgDrawbar[2] = 0.75f;    // 8' = 6
    instrumentPresets[191].patch.p_orgDrawbar[3] = 0.75f;    // 4' = 6
    instrumentPresets[191].patch.p_orgDrawbar[6] = 0.5f;     // 1⅗' = 4
    instrumentPresets[191].patch.p_volume = 0.55f;
    instrumentPresets[191].patch.p_orgVibratoMode = 4;  // C1 — gentle lounge shimmer

    // ========================================================================
    // REED INSTRUMENTS (192-197) — Single/double reed waveguide
    // ========================================================================

    // 192: Clarinet — cylindrical bore, odd harmonics, dark & hollow
    instrumentPresets[192].name = "Clarinet";
    instrumentPresets[192].patch.p_waveType = WAVE_REED;
    instrumentPresets[192].patch.p_reedBlowPressure = 0.55f;
    instrumentPresets[192].patch.p_reedStiffness = 0.35f;    // soft cane reed → smooth, dark
    instrumentPresets[192].patch.p_reedAperture = 0.6f;      // moderate opening
    instrumentPresets[192].patch.p_reedBore = 0.0f;          // cylindrical → odd harmonics only
    instrumentPresets[192].patch.p_reedVibratoDepth = 0.15f;
    instrumentPresets[192].patch.p_attack = 0.02f;
    instrumentPresets[192].patch.p_decay = 0.1f;
    instrumentPresets[192].patch.p_sustain = 0.9f;
    instrumentPresets[192].patch.p_release = 0.08f;
    instrumentPresets[192].patch.p_volume = 0.6f;

    // 193: Soprano Sax — conical bore, bright and cutting
    instrumentPresets[193].name = "Soprano Sax";
    instrumentPresets[193].patch.p_waveType = WAVE_REED;
    instrumentPresets[193].patch.p_reedBlowPressure = 0.65f;  // strong breath
    instrumentPresets[193].patch.p_reedStiffness = 0.7f;     // stiff → bright, edgy
    instrumentPresets[193].patch.p_reedAperture = 0.4f;      // narrow → more complex spectrum
    instrumentPresets[193].patch.p_reedBore = 0.9f;          // strongly conical → all harmonics
    instrumentPresets[193].patch.p_reedVibratoDepth = 0.35f;
    instrumentPresets[193].patch.p_attack = 0.01f;
    instrumentPresets[193].patch.p_decay = 0.1f;
    instrumentPresets[193].patch.p_sustain = 0.85f;
    instrumentPresets[193].patch.p_release = 0.1f;
    instrumentPresets[193].patch.p_volume = 0.55f;

    // 194: Alto Sax — conical bore, warm and round, classic jazz
    instrumentPresets[194].name = "Alto Sax";
    instrumentPresets[194].patch.p_waveType = WAVE_REED;
    instrumentPresets[194].patch.p_reedBlowPressure = 0.5f;
    instrumentPresets[194].patch.p_reedStiffness = 0.45f;    // medium reed
    instrumentPresets[194].patch.p_reedAperture = 0.65f;     // wider → warmer fundamental
    instrumentPresets[194].patch.p_reedBore = 0.75f;         // conical
    instrumentPresets[194].patch.p_reedVibratoDepth = 0.25f;
    instrumentPresets[194].patch.p_attack = 0.02f;
    instrumentPresets[194].patch.p_decay = 0.1f;
    instrumentPresets[194].patch.p_sustain = 0.85f;
    instrumentPresets[194].patch.p_release = 0.1f;
    instrumentPresets[194].patch.p_volume = 0.6f;

    // 195: Tenor Sax — conical bore, rich and breathy, Coltrane
    instrumentPresets[195].name = "Tenor Sax";
    instrumentPresets[195].patch.p_waveType = WAVE_REED;
    instrumentPresets[195].patch.p_reedBlowPressure = 0.5f;
    instrumentPresets[195].patch.p_reedStiffness = 0.3f;     // soft big reed → smooth
    instrumentPresets[195].patch.p_reedAperture = 0.75f;     // wide → strong fundamental
    instrumentPresets[195].patch.p_reedBore = 0.8f;          // conical
    instrumentPresets[195].patch.p_reedVibratoDepth = 0.4f;   // wide vibrato
    instrumentPresets[195].patch.p_attack = 0.025f;
    instrumentPresets[195].patch.p_decay = 0.1f;
    instrumentPresets[195].patch.p_sustain = 0.9f;
    instrumentPresets[195].patch.p_release = 0.12f;
    instrumentPresets[195].patch.p_volume = 0.6f;

    // 196: Oboe — stiff double reed, narrow conical bore, nasal & penetrating
    instrumentPresets[196].name = "Oboe";
    instrumentPresets[196].patch.p_waveType = WAVE_REED;
    instrumentPresets[196].patch.p_reedBlowPressure = 0.7f;   // high pressure for double reed
    instrumentPresets[196].patch.p_reedStiffness = 0.9f;      // very stiff → buzzy, nasal
    instrumentPresets[196].patch.p_reedAperture = 0.2f;       // very narrow → intense harmonics
    instrumentPresets[196].patch.p_reedBore = 0.55f;          // narrow conical
    instrumentPresets[196].patch.p_reedVibratoDepth = 0.2f;
    instrumentPresets[196].patch.p_attack = 0.008f;
    instrumentPresets[196].patch.p_decay = 0.08f;
    instrumentPresets[196].patch.p_sustain = 0.85f;
    instrumentPresets[196].patch.p_release = 0.06f;
    instrumentPresets[196].patch.p_volume = 0.5f;

    // 197: Harmonica — soft free reed, minimal bore, warm & buzzy
    instrumentPresets[197].name = "Harmonica";
    instrumentPresets[197].patch.p_waveType = WAVE_REED;
    instrumentPresets[197].patch.p_reedBlowPressure = 0.4f;   // gentle breath
    instrumentPresets[197].patch.p_reedStiffness = 0.15f;     // very soft free reed
    instrumentPresets[197].patch.p_reedAperture = 0.85f;      // wide open → strong fundamental
    instrumentPresets[197].patch.p_reedBore = 0.1f;           // almost no bore → cylindrical-ish
    instrumentPresets[197].patch.p_reedVibratoDepth = 0.5f;   // hand-wah vibrato
    instrumentPresets[197].patch.p_attack = 0.03f;
    instrumentPresets[197].patch.p_decay = 0.05f;
    instrumentPresets[197].patch.p_sustain = 0.9f;
    instrumentPresets[197].patch.p_release = 0.08f;
    instrumentPresets[197].patch.p_volume = 0.55f;

    // ========================================================================
    // METALLIC PERCUSSION (ring-mod engine — authentic 808/909/cymbals)
    // ========================================================================

    // 198: Metallic 808 CH — ring-mod hihat, tight and sizzly
    instrumentPresets[198].name = "M-808 CH";
    instrumentPresets[198].patch.p_waveType = WAVE_METALLIC;
    instrumentPresets[198].patch.p_metallicPreset = METALLIC_808_CH;
    instrumentPresets[198].patch.p_attack = 0.0f;
    instrumentPresets[198].patch.p_decay = 0.05f;
    instrumentPresets[198].patch.p_sustain = 0.0f;
    instrumentPresets[198].patch.p_release = 0.02f;
    instrumentPresets[198].patch.p_expDecay = true;
    instrumentPresets[198].patch.p_volume = 0.4f;
    instrumentPresets[198].patch.p_useTriggerFreq = true;
    instrumentPresets[198].patch.p_triggerFreq = 460.0f;
    instrumentPresets[198].patch.p_choke = true;

    // 199: Metallic 808 OH — ring-mod open hihat
    instrumentPresets[199].name = "M-808 OH";
    instrumentPresets[199].patch.p_waveType = WAVE_METALLIC;
    instrumentPresets[199].patch.p_metallicPreset = METALLIC_808_OH;
    instrumentPresets[199].patch.p_attack = 0.0f;
    instrumentPresets[199].patch.p_decay = 0.4f;
    instrumentPresets[199].patch.p_sustain = 0.0f;
    instrumentPresets[199].patch.p_release = 0.1f;
    instrumentPresets[199].patch.p_expDecay = true;
    instrumentPresets[199].patch.p_volume = 0.4f;
    instrumentPresets[199].patch.p_useTriggerFreq = true;
    instrumentPresets[199].patch.p_triggerFreq = 460.0f;
    instrumentPresets[199].patch.p_choke = true;

    // 200: Metallic 909 CH — brighter, more noise
    instrumentPresets[200].name = "M-909 CH";
    instrumentPresets[200].patch.p_waveType = WAVE_METALLIC;
    instrumentPresets[200].patch.p_metallicPreset = METALLIC_909_CH;
    instrumentPresets[200].patch.p_attack = 0.0f;
    instrumentPresets[200].patch.p_decay = 0.06f;
    instrumentPresets[200].patch.p_sustain = 0.0f;
    instrumentPresets[200].patch.p_release = 0.02f;
    instrumentPresets[200].patch.p_expDecay = true;
    instrumentPresets[200].patch.p_volume = 0.4f;
    instrumentPresets[200].patch.p_useTriggerFreq = true;
    instrumentPresets[200].patch.p_triggerFreq = 460.0f;
    instrumentPresets[200].patch.p_choke = true;

    // 201: Metallic Ride — long shimmer cymbal
    instrumentPresets[201].name = "M-Ride";
    instrumentPresets[201].patch.p_waveType = WAVE_METALLIC;
    instrumentPresets[201].patch.p_metallicPreset = METALLIC_RIDE;
    instrumentPresets[201].patch.p_attack = 0.0f;
    instrumentPresets[201].patch.p_decay = 3.0f;
    instrumentPresets[201].patch.p_sustain = 0.0f;
    instrumentPresets[201].patch.p_release = 0.5f;
    instrumentPresets[201].patch.p_expDecay = true;
    instrumentPresets[201].patch.p_volume = 0.35f;
    instrumentPresets[201].patch.p_useTriggerFreq = true;
    instrumentPresets[201].patch.p_triggerFreq = 300.0f;

    // 202: Metallic Crash — bright burst, long tail
    instrumentPresets[202].name = "M-Crash";
    instrumentPresets[202].patch.p_waveType = WAVE_METALLIC;
    instrumentPresets[202].patch.p_metallicPreset = METALLIC_CRASH;
    instrumentPresets[202].patch.p_attack = 0.0f;
    instrumentPresets[202].patch.p_decay = 5.0f;
    instrumentPresets[202].patch.p_sustain = 0.0f;
    instrumentPresets[202].patch.p_release = 1.0f;
    instrumentPresets[202].patch.p_expDecay = true;
    instrumentPresets[202].patch.p_volume = 0.35f;
    instrumentPresets[202].patch.p_useTriggerFreq = true;
    instrumentPresets[202].patch.p_triggerFreq = 250.0f;

    // 203: Metallic Cowbell — 808 cowbell, two-tone
    instrumentPresets[203].name = "M-Cowbell";
    instrumentPresets[203].patch.p_waveType = WAVE_METALLIC;
    instrumentPresets[203].patch.p_metallicPreset = METALLIC_COWBELL;
    instrumentPresets[203].patch.p_attack = 0.0f;
    instrumentPresets[203].patch.p_decay = 0.5f;
    instrumentPresets[203].patch.p_sustain = 0.0f;
    instrumentPresets[203].patch.p_release = 0.05f;
    instrumentPresets[203].patch.p_expDecay = true;
    instrumentPresets[203].patch.p_volume = 0.45f;
    instrumentPresets[203].patch.p_useTriggerFreq = true;
    instrumentPresets[203].patch.p_triggerFreq = 560.0f;

    // ========================================================================
    // BRASS INSTRUMENTS (204-209) — Lip-valve waveguide
    // ========================================================================

    // 204: Trumpet — cylindrical bore, bright and projecting
    instrumentPresets[204].name = "Trumpet";
    instrumentPresets[204].patch.p_waveType = WAVE_BRASS;
    instrumentPresets[204].patch.p_brassBlowPressure = 0.6f;
    instrumentPresets[204].patch.p_brassLipTension = 0.6f;    // medium-tight → bright, clear
    instrumentPresets[204].patch.p_brassLipAperture = 0.5f;
    instrumentPresets[204].patch.p_brassBore = 0.0f;          // cylindrical → bright, brassy
    instrumentPresets[204].patch.p_brassMute = 0.0f;          // open bell
    instrumentPresets[204].patch.p_attack = 0.015f;
    instrumentPresets[204].patch.p_decay = 0.1f;
    instrumentPresets[204].patch.p_sustain = 0.9f;
    instrumentPresets[204].patch.p_release = 0.08f;
    instrumentPresets[204].patch.p_volume = 0.6f;

    // 205: Muted Trumpet — harmon mute, nasal jazz tone (Miles Davis)
    instrumentPresets[205].name = "Muted Trumpet";
    instrumentPresets[205].patch.p_waveType = WAVE_BRASS;
    instrumentPresets[205].patch.p_brassBlowPressure = 0.6f;   // need more pressure to overcome mute
    instrumentPresets[205].patch.p_brassLipTension = 0.55f;
    instrumentPresets[205].patch.p_brassLipAperture = 0.5f;
    instrumentPresets[205].patch.p_brassBore = 0.1f;
    instrumentPresets[205].patch.p_brassMute = 0.65f;          // moderate mute → nasal but still speaks
    instrumentPresets[205].patch.p_attack = 0.02f;
    instrumentPresets[205].patch.p_decay = 0.1f;
    instrumentPresets[205].patch.p_sustain = 0.85f;
    instrumentPresets[205].patch.p_release = 0.1f;
    instrumentPresets[205].patch.p_volume = 0.55f;

    // 206: Trombone — cylindrical bore, warm and powerful
    instrumentPresets[206].name = "Trombone";
    instrumentPresets[206].patch.p_waveType = WAVE_BRASS;
    instrumentPresets[206].patch.p_brassBlowPressure = 0.55f;
    instrumentPresets[206].patch.p_brassLipTension = 0.35f;   // relaxed lips → lower, warmer
    instrumentPresets[206].patch.p_brassLipAperture = 0.6f;   // wider → bigger sound
    instrumentPresets[206].patch.p_brassBore = 0.2f;          // mostly cylindrical, slight flare
    instrumentPresets[206].patch.p_brassMute = 0.0f;
    instrumentPresets[206].patch.p_attack = 0.025f;
    instrumentPresets[206].patch.p_decay = 0.1f;
    instrumentPresets[206].patch.p_sustain = 0.9f;
    instrumentPresets[206].patch.p_release = 0.1f;
    instrumentPresets[206].patch.p_volume = 0.6f;

    // 207: French Horn — conical bore, warm and mellow
    instrumentPresets[207].name = "French Horn";
    instrumentPresets[207].patch.p_waveType = WAVE_BRASS;
    instrumentPresets[207].patch.p_brassBlowPressure = 0.5f;
    instrumentPresets[207].patch.p_brassLipTension = 0.5f;
    instrumentPresets[207].patch.p_brassLipAperture = 0.55f;
    instrumentPresets[207].patch.p_brassBore = 0.85f;         // strongly conical → dark, mellow
    instrumentPresets[207].patch.p_brassMute = 0.0f;
    instrumentPresets[207].patch.p_attack = 0.03f;
    instrumentPresets[207].patch.p_decay = 0.15f;
    instrumentPresets[207].patch.p_sustain = 0.85f;
    instrumentPresets[207].patch.p_release = 0.15f;
    instrumentPresets[207].patch.p_volume = 0.55f;

    // 208: Tuba — conical bore, deep and round
    instrumentPresets[208].name = "Tuba";
    instrumentPresets[208].patch.p_waveType = WAVE_BRASS;
    instrumentPresets[208].patch.p_brassBlowPressure = 0.45f;  // less pressure, big bore
    instrumentPresets[208].patch.p_brassLipTension = 0.2f;     // very relaxed → low, fat
    instrumentPresets[208].patch.p_brassLipAperture = 0.7f;    // wide → round, deep
    instrumentPresets[208].patch.p_brassBore = 0.95f;          // fully conical → very dark
    instrumentPresets[208].patch.p_brassMute = 0.0f;
    instrumentPresets[208].patch.p_attack = 0.04f;
    instrumentPresets[208].patch.p_decay = 0.1f;
    instrumentPresets[208].patch.p_sustain = 0.9f;
    instrumentPresets[208].patch.p_release = 0.12f;
    instrumentPresets[208].patch.p_volume = 0.6f;

    // 209: Flugelhorn — conical bore, soft and lyrical
    instrumentPresets[209].name = "Flugelhorn";
    instrumentPresets[209].patch.p_waveType = WAVE_BRASS;
    instrumentPresets[209].patch.p_brassBlowPressure = 0.5f;
    instrumentPresets[209].patch.p_brassLipTension = 0.45f;
    instrumentPresets[209].patch.p_brassLipAperture = 0.6f;   // wider → softer, rounder
    instrumentPresets[209].patch.p_brassBore = 0.7f;          // conical → warm, flugelhorn character
    instrumentPresets[209].patch.p_brassMute = 0.0f;
    instrumentPresets[209].patch.p_attack = 0.025f;
    instrumentPresets[209].patch.p_decay = 0.1f;
    instrumentPresets[209].patch.p_sustain = 0.85f;
    instrumentPresets[209].patch.p_release = 0.1f;
    instrumentPresets[209].patch.p_volume = 0.55f;

    // ========================================================================
    // GUITAR BODY (KS string + body resonator)
    // ========================================================================

    // 210: Acoustic Guitar — steel-string, full body
    instrumentPresets[210].name = "Acoustic Gtr";
    instrumentPresets[210].patch.p_waveType = WAVE_GUITAR;
    instrumentPresets[210].patch.p_guitarPreset = GUITAR_ACOUSTIC;
    instrumentPresets[210].patch.p_pluckBrightness = 0.6f;
    instrumentPresets[210].patch.p_pluckDamping = 0.997f;
    instrumentPresets[210].patch.p_guitarBodyMix = 0.6f;
    instrumentPresets[210].patch.p_guitarBodyBrightness = 0.5f;
    instrumentPresets[210].patch.p_guitarPickPosition = 0.3f;
    instrumentPresets[210].patch.p_attack = 0.0f;
    instrumentPresets[210].patch.p_decay = 2.0f;
    instrumentPresets[210].patch.p_sustain = 0.0f;
    instrumentPresets[210].patch.p_release = 0.1f;
    instrumentPresets[210].patch.p_volume = 0.5f;

    // 211: Classical Guitar — nylon, warm, fingerpicked
    instrumentPresets[211].name = "Classical Gtr";
    instrumentPresets[211].patch.p_waveType = WAVE_GUITAR;
    instrumentPresets[211].patch.p_guitarPreset = GUITAR_CLASSICAL;
    instrumentPresets[211].patch.p_pluckBrightness = 0.35f;
    instrumentPresets[211].patch.p_pluckDamping = 0.998f;
    instrumentPresets[211].patch.p_guitarBodyMix = 0.65f;
    instrumentPresets[211].patch.p_guitarBodyBrightness = 0.35f;
    instrumentPresets[211].patch.p_guitarPickPosition = 0.45f;
    instrumentPresets[211].patch.p_attack = 0.0f;
    instrumentPresets[211].patch.p_decay = 2.5f;
    instrumentPresets[211].patch.p_sustain = 0.0f;
    instrumentPresets[211].patch.p_release = 0.15f;
    instrumentPresets[211].patch.p_volume = 0.5f;

    // 212: Banjo — twangy, bright, fast decay
    instrumentPresets[212].name = "Banjo";
    instrumentPresets[212].patch.p_waveType = WAVE_GUITAR;
    instrumentPresets[212].patch.p_guitarPreset = GUITAR_BANJO;
    instrumentPresets[212].patch.p_pluckBrightness = 0.8f;
    instrumentPresets[212].patch.p_pluckDamping = 0.993f;    // Shorter sustain
    instrumentPresets[212].patch.p_guitarBodyMix = 0.7f;
    instrumentPresets[212].patch.p_guitarBodyBrightness = 0.7f;
    instrumentPresets[212].patch.p_guitarPickPosition = 0.2f;
    instrumentPresets[212].patch.p_attack = 0.0f;
    instrumentPresets[212].patch.p_decay = 1.0f;
    instrumentPresets[212].patch.p_sustain = 0.0f;
    instrumentPresets[212].patch.p_release = 0.05f;
    instrumentPresets[212].patch.p_volume = 0.5f;

    // 213: Sitar — buzzy, resonant, drone character
    instrumentPresets[213].name = "Sitar";
    instrumentPresets[213].patch.p_waveType = WAVE_GUITAR;
    instrumentPresets[213].patch.p_guitarPreset = GUITAR_SITAR;
    instrumentPresets[213].patch.p_pluckBrightness = 0.55f;
    instrumentPresets[213].patch.p_pluckDamping = 0.998f;
    instrumentPresets[213].patch.p_guitarBodyMix = 0.55f;
    instrumentPresets[213].patch.p_guitarBodyBrightness = 0.6f;
    instrumentPresets[213].patch.p_guitarPickPosition = 0.25f;
    instrumentPresets[213].patch.p_guitarBuzz = 0.6f;
    instrumentPresets[213].patch.p_attack = 0.0f;
    instrumentPresets[213].patch.p_decay = 3.0f;
    instrumentPresets[213].patch.p_sustain = 0.0f;
    instrumentPresets[213].patch.p_release = 0.2f;
    instrumentPresets[213].patch.p_volume = 0.45f;

    // 214: Oud — deep, round, Middle Eastern
    instrumentPresets[214].name = "Oud";
    instrumentPresets[214].patch.p_waveType = WAVE_GUITAR;
    instrumentPresets[214].patch.p_guitarPreset = GUITAR_OUD;
    instrumentPresets[214].patch.p_pluckBrightness = 0.45f;
    instrumentPresets[214].patch.p_pluckDamping = 0.997f;
    instrumentPresets[214].patch.p_guitarBodyMix = 0.65f;
    instrumentPresets[214].patch.p_guitarBodyBrightness = 0.4f;
    instrumentPresets[214].patch.p_guitarPickPosition = 0.35f;
    instrumentPresets[214].patch.p_attack = 0.0f;
    instrumentPresets[214].patch.p_decay = 2.0f;
    instrumentPresets[214].patch.p_sustain = 0.0f;
    instrumentPresets[214].patch.p_release = 0.12f;
    instrumentPresets[214].patch.p_volume = 0.5f;

    // 215: Koto — bright, sharp, Japanese
    instrumentPresets[215].name = "Koto";
    instrumentPresets[215].patch.p_waveType = WAVE_GUITAR;
    instrumentPresets[215].patch.p_guitarPreset = GUITAR_KOTO;
    instrumentPresets[215].patch.p_pluckBrightness = 0.7f;
    instrumentPresets[215].patch.p_pluckDamping = 0.996f;
    instrumentPresets[215].patch.p_guitarBodyMix = 0.5f;
    instrumentPresets[215].patch.p_guitarBodyBrightness = 0.75f;
    instrumentPresets[215].patch.p_guitarPickPosition = 0.15f;
    instrumentPresets[215].patch.p_guitarBuzz = 0.1f;
    instrumentPresets[215].patch.p_attack = 0.0f;
    instrumentPresets[215].patch.p_decay = 1.5f;
    instrumentPresets[215].patch.p_sustain = 0.0f;
    instrumentPresets[215].patch.p_release = 0.08f;
    instrumentPresets[215].patch.p_volume = 0.5f;

    // 216: Harp — clean, long sustain, minimal body
    instrumentPresets[216].name = "Harp";
    instrumentPresets[216].patch.p_waveType = WAVE_GUITAR;
    instrumentPresets[216].patch.p_guitarPreset = GUITAR_HARP;
    instrumentPresets[216].patch.p_pluckBrightness = 0.5f;
    instrumentPresets[216].patch.p_pluckDamping = 0.9995f;   // Very long sustain
    instrumentPresets[216].patch.p_guitarBodyMix = 0.15f;
    instrumentPresets[216].patch.p_guitarBodyBrightness = 0.55f;
    instrumentPresets[216].patch.p_guitarPickPosition = 0.5f;
    instrumentPresets[216].patch.p_attack = 0.0f;
    instrumentPresets[216].patch.p_decay = 4.0f;
    instrumentPresets[216].patch.p_sustain = 0.0f;
    instrumentPresets[216].patch.p_release = 0.3f;
    instrumentPresets[216].patch.p_volume = 0.45f;

    // 217: Ukulele — small, warm, cheerful
    instrumentPresets[217].name = "Ukulele";
    instrumentPresets[217].patch.p_waveType = WAVE_GUITAR;
    instrumentPresets[217].patch.p_guitarPreset = GUITAR_UKULELE;
    instrumentPresets[217].patch.p_pluckBrightness = 0.45f;
    instrumentPresets[217].patch.p_pluckDamping = 0.995f;
    instrumentPresets[217].patch.p_guitarBodyMix = 0.6f;
    instrumentPresets[217].patch.p_guitarBodyBrightness = 0.45f;
    instrumentPresets[217].patch.p_guitarPickPosition = 0.4f;
    instrumentPresets[217].patch.p_attack = 0.0f;
    instrumentPresets[217].patch.p_decay = 1.5f;
    instrumentPresets[217].patch.p_sustain = 0.0f;
    instrumentPresets[217].patch.p_release = 0.08f;
    instrumentPresets[217].patch.p_volume = 0.5f;

    // === StifKarp (stiff string) presets ===

    // 218: Grand Piano — warm felt hammer, spruce soundboard, natural inharmonicity
    instrumentPresets[218].name = "Grand Piano";
    instrumentPresets[218].patch.p_waveType = WAVE_STIFKARP;
    instrumentPresets[218].patch.p_stifkarpPreset = STIFKARP_PIANO;
    instrumentPresets[218].patch.p_pluckBrightness = 0.55f;
    instrumentPresets[218].patch.p_pluckDamping = 0.9992f;
    instrumentPresets[218].patch.p_stifkarpHardness = 0.45f;
    instrumentPresets[218].patch.p_stifkarpStiffness = 0.25f;
    instrumentPresets[218].patch.p_stifkarpStrikePos = 0.12f;
    instrumentPresets[218].patch.p_stifkarpBodyMix = 0.7f;
    instrumentPresets[218].patch.p_stifkarpBodyBrightness = 0.5f;
    instrumentPresets[218].patch.p_stifkarpDamper = 0.9f;
    instrumentPresets[218].patch.p_stifkarpSympathetic = 0.15f;
    instrumentPresets[218].patch.p_attack = 0.0f;
    instrumentPresets[218].patch.p_decay = 4.0f;
    instrumentPresets[218].patch.p_sustain = 0.0f;
    instrumentPresets[218].patch.p_release = 0.3f;
    instrumentPresets[218].patch.p_expDecay = true;
    instrumentPresets[218].patch.p_volume = 0.5f;

    // 219: Bright Piano — concert grand, harder hammers, more shimmer
    instrumentPresets[219].name = "Bright Piano";
    instrumentPresets[219].patch.p_waveType = WAVE_STIFKARP;
    instrumentPresets[219].patch.p_stifkarpPreset = STIFKARP_BRIGHT_PIANO;
    instrumentPresets[219].patch.p_pluckBrightness = 0.7f;
    instrumentPresets[219].patch.p_pluckDamping = 0.9994f;
    instrumentPresets[219].patch.p_stifkarpHardness = 0.65f;
    instrumentPresets[219].patch.p_stifkarpStiffness = 0.3f;
    instrumentPresets[219].patch.p_stifkarpStrikePos = 0.11f;
    instrumentPresets[219].patch.p_stifkarpBodyMix = 0.65f;
    instrumentPresets[219].patch.p_stifkarpBodyBrightness = 0.65f;
    instrumentPresets[219].patch.p_stifkarpDamper = 0.95f;
    instrumentPresets[219].patch.p_stifkarpSympathetic = 0.2f;
    instrumentPresets[219].patch.p_attack = 0.0f;
    instrumentPresets[219].patch.p_decay = 5.0f;
    instrumentPresets[219].patch.p_sustain = 0.0f;
    instrumentPresets[219].patch.p_release = 0.4f;
    instrumentPresets[219].patch.p_expDecay = true;
    instrumentPresets[219].patch.p_volume = 0.5f;

    // 220: Harpsichord — plectrum pluck, bright, jangly, fast decay
    instrumentPresets[220].name = "Harpsichord";
    instrumentPresets[220].patch.p_waveType = WAVE_STIFKARP;
    instrumentPresets[220].patch.p_stifkarpPreset = STIFKARP_HARPSICHORD;
    instrumentPresets[220].patch.p_pluckBrightness = 0.85f;
    instrumentPresets[220].patch.p_pluckDamping = 0.997f;
    instrumentPresets[220].patch.p_stifkarpHardness = 0.9f;
    instrumentPresets[220].patch.p_stifkarpStiffness = 0.15f;
    instrumentPresets[220].patch.p_stifkarpStrikePos = 0.08f;
    instrumentPresets[220].patch.p_stifkarpBodyMix = 0.25f;
    instrumentPresets[220].patch.p_stifkarpBodyBrightness = 0.7f;
    instrumentPresets[220].patch.p_stifkarpDamper = 0.0f;
    instrumentPresets[220].patch.p_stifkarpSympathetic = 0.0f;
    instrumentPresets[220].patch.p_attack = 0.0f;
    instrumentPresets[220].patch.p_decay = 1.5f;
    instrumentPresets[220].patch.p_sustain = 0.0f;
    instrumentPresets[220].patch.p_release = 0.05f;
    instrumentPresets[220].patch.p_expDecay = true;
    instrumentPresets[220].patch.p_filterCutoff = 0.8f;
    instrumentPresets[220].patch.p_volume = 0.5f;

    // 221: Hammered Dulcimer — metallic shimmer, prominent body, long sustain
    instrumentPresets[221].name = "Dulcimer";
    instrumentPresets[221].patch.p_waveType = WAVE_STIFKARP;
    instrumentPresets[221].patch.p_stifkarpPreset = STIFKARP_DULCIMER;
    instrumentPresets[221].patch.p_pluckBrightness = 0.65f;
    instrumentPresets[221].patch.p_pluckDamping = 0.9988f;
    instrumentPresets[221].patch.p_stifkarpHardness = 0.7f;
    instrumentPresets[221].patch.p_stifkarpStiffness = 0.4f;
    instrumentPresets[221].patch.p_stifkarpStrikePos = 0.15f;
    instrumentPresets[221].patch.p_stifkarpBodyMix = 0.75f;
    instrumentPresets[221].patch.p_stifkarpBodyBrightness = 0.6f;
    instrumentPresets[221].patch.p_stifkarpDamper = 1.0f;
    instrumentPresets[221].patch.p_stifkarpSympathetic = 0.25f;
    instrumentPresets[221].patch.p_attack = 0.0f;
    instrumentPresets[221].patch.p_decay = 3.0f;
    instrumentPresets[221].patch.p_sustain = 0.0f;
    instrumentPresets[221].patch.p_release = 0.15f;
    instrumentPresets[221].patch.p_expDecay = true;
    instrumentPresets[221].patch.p_volume = 0.5f;

    // 222: Clavichord — soft tangent, intimate, minimal body
    instrumentPresets[222].name = "Clavichord";
    instrumentPresets[222].patch.p_waveType = WAVE_STIFKARP;
    instrumentPresets[222].patch.p_stifkarpPreset = STIFKARP_CLAVICHORD;
    instrumentPresets[222].patch.p_pluckBrightness = 0.35f;
    instrumentPresets[222].patch.p_pluckDamping = 0.998f;
    instrumentPresets[222].patch.p_stifkarpHardness = 0.2f;
    instrumentPresets[222].patch.p_stifkarpStiffness = 0.1f;
    instrumentPresets[222].patch.p_stifkarpStrikePos = 0.5f;
    instrumentPresets[222].patch.p_stifkarpBodyMix = 0.1f;
    instrumentPresets[222].patch.p_stifkarpBodyBrightness = 0.35f;
    instrumentPresets[222].patch.p_stifkarpDamper = 0.7f;
    instrumentPresets[222].patch.p_stifkarpSympathetic = 0.05f;
    instrumentPresets[222].patch.p_attack = 0.005f;
    instrumentPresets[222].patch.p_decay = 2.0f;
    instrumentPresets[222].patch.p_sustain = 0.15f;
    instrumentPresets[222].patch.p_release = 0.1f;
    instrumentPresets[222].patch.p_expDecay = true;
    instrumentPresets[222].patch.p_volume = 0.4f;

    // 223: Prepared Piano — bolts on strings, rattling buzz
    instrumentPresets[223].name = "Prepared Piano";
    instrumentPresets[223].patch.p_waveType = WAVE_STIFKARP;
    instrumentPresets[223].patch.p_stifkarpPreset = STIFKARP_PREPARED;
    instrumentPresets[223].patch.p_pluckBrightness = 0.5f;
    instrumentPresets[223].patch.p_pluckDamping = 0.998f;
    instrumentPresets[223].patch.p_stifkarpHardness = 0.5f;
    instrumentPresets[223].patch.p_stifkarpStiffness = 0.35f;
    instrumentPresets[223].patch.p_stifkarpStrikePos = 0.2f;
    instrumentPresets[223].patch.p_stifkarpBodyMix = 0.6f;
    instrumentPresets[223].patch.p_stifkarpBodyBrightness = 0.4f;
    instrumentPresets[223].patch.p_stifkarpDamper = 0.85f;
    instrumentPresets[223].patch.p_stifkarpSympathetic = 0.1f;
    instrumentPresets[223].patch.p_attack = 0.0f;
    instrumentPresets[223].patch.p_decay = 2.5f;
    instrumentPresets[223].patch.p_sustain = 0.0f;
    instrumentPresets[223].patch.p_release = 0.2f;
    instrumentPresets[223].patch.p_expDecay = true;
    instrumentPresets[223].patch.p_volume = 0.45f;

    // 224: Honky Tonk — detuned upright piano, chorus beating
    instrumentPresets[224].name = "Honky Tonk";
    instrumentPresets[224].patch.p_waveType = WAVE_STIFKARP;
    instrumentPresets[224].patch.p_stifkarpPreset = STIFKARP_HONKYTONK;
    instrumentPresets[224].patch.p_pluckBrightness = 0.5f;
    instrumentPresets[224].patch.p_pluckDamping = 0.9985f;
    instrumentPresets[224].patch.p_stifkarpHardness = 0.55f;
    instrumentPresets[224].patch.p_stifkarpStiffness = 0.2f;
    instrumentPresets[224].patch.p_stifkarpStrikePos = 0.13f;
    instrumentPresets[224].patch.p_stifkarpBodyMix = 0.35f;
    instrumentPresets[224].patch.p_stifkarpBodyBrightness = 0.4f;
    instrumentPresets[224].patch.p_stifkarpDamper = 0.8f;
    instrumentPresets[224].patch.p_stifkarpSympathetic = 0.1f;
    instrumentPresets[224].patch.p_unisonCount = 2;
    instrumentPresets[224].patch.p_unisonDetune = 8.0f;
    instrumentPresets[224].patch.p_unisonMix = 0.8f;
    instrumentPresets[224].patch.p_attack = 0.0f;
    instrumentPresets[224].patch.p_decay = 3.0f;
    instrumentPresets[224].patch.p_sustain = 0.0f;
    instrumentPresets[224].patch.p_release = 0.2f;
    instrumentPresets[224].patch.p_expDecay = true;
    instrumentPresets[224].patch.p_volume = 0.45f;

    // 225: Celesta — metal plates + resonator, bell-like, high stiffness
    instrumentPresets[225].name = "Celesta";
    instrumentPresets[225].patch.p_waveType = WAVE_STIFKARP;
    instrumentPresets[225].patch.p_stifkarpPreset = STIFKARP_CELESTA;
    instrumentPresets[225].patch.p_pluckBrightness = 0.6f;
    instrumentPresets[225].patch.p_pluckDamping = 0.999f;
    instrumentPresets[225].patch.p_stifkarpHardness = 0.35f;
    instrumentPresets[225].patch.p_stifkarpStiffness = 0.6f;
    instrumentPresets[225].patch.p_stifkarpStrikePos = 0.25f;
    instrumentPresets[225].patch.p_stifkarpBodyMix = 0.5f;
    instrumentPresets[225].patch.p_stifkarpBodyBrightness = 0.65f;
    instrumentPresets[225].patch.p_stifkarpDamper = 1.0f;
    instrumentPresets[225].patch.p_stifkarpSympathetic = 0.1f;
    instrumentPresets[225].patch.p_attack = 0.0f;
    instrumentPresets[225].patch.p_decay = 3.5f;
    instrumentPresets[225].patch.p_sustain = 0.0f;
    instrumentPresets[225].patch.p_release = 0.5f;
    instrumentPresets[225].patch.p_expDecay = true;
    instrumentPresets[225].patch.p_volume = 0.45f;

    // ========================================================================
    // SHAKER PERCUSSION (PhISM particle collision models)
    // ========================================================================

    // 226: Maraca — classic Latin shaker
    instrumentPresets[226].name = "Maraca";
    instrumentPresets[226].patch = createDefaultPatch(WAVE_SHAKER);
    instrumentPresets[226].patch.p_shakerPreset = SHAKER_MARACA;
    instrumentPresets[226].patch.p_shakerParticles = 0.22f;
    instrumentPresets[226].patch.p_shakerDecayRate = 0.47f;
    instrumentPresets[226].patch.p_shakerResonance = 0.5f;
    instrumentPresets[226].patch.p_shakerBrightness = 0.5f;
    instrumentPresets[226].patch.p_shakerScrape = 0.0f;
    instrumentPresets[226].patch.p_envelopeEnabled = true;
    instrumentPresets[226].patch.p_attack = 0.001f;
    instrumentPresets[226].patch.p_decay = 0.3f;
    instrumentPresets[226].patch.p_sustain = 0.0f;
    instrumentPresets[226].patch.p_release = 0.15f;
    instrumentPresets[226].patch.p_expDecay = true;
    instrumentPresets[226].patch.p_oneShot = true;
    instrumentPresets[226].patch.p_useTriggerFreq = true;
    instrumentPresets[226].patch.p_triggerFreq = 3200.0f;
    instrumentPresets[226].patch.p_volume = 0.55f;

    // 227: Cabasa — metal beads, bright, snappy
    instrumentPresets[227].name = "Cabasa";
    instrumentPresets[227].patch = createDefaultPatch(WAVE_SHAKER);
    instrumentPresets[227].patch.p_shakerPreset = SHAKER_CABASA;
    instrumentPresets[227].patch.p_shakerParticles = 0.57f;
    instrumentPresets[227].patch.p_shakerDecayRate = 0.20f;
    instrumentPresets[227].patch.p_shakerResonance = 0.6f;
    instrumentPresets[227].patch.p_shakerBrightness = 0.7f;
    instrumentPresets[227].patch.p_shakerScrape = 0.0f;
    instrumentPresets[227].patch.p_envelopeEnabled = true;
    instrumentPresets[227].patch.p_attack = 0.001f;
    instrumentPresets[227].patch.p_decay = 0.15f;
    instrumentPresets[227].patch.p_sustain = 0.0f;
    instrumentPresets[227].patch.p_release = 0.08f;
    instrumentPresets[227].patch.p_expDecay = true;
    instrumentPresets[227].patch.p_oneShot = true;
    instrumentPresets[227].patch.p_useTriggerFreq = true;
    instrumentPresets[227].patch.p_triggerFreq = 5000.0f;
    instrumentPresets[227].patch.p_volume = 0.22f;

    // 228: Tambourine — jingly, tonal shimmer
    instrumentPresets[228].name = "Tambourine";
    instrumentPresets[228].patch = createDefaultPatch(WAVE_SHAKER);
    instrumentPresets[228].patch.p_shakerPreset = SHAKER_TAMBOURINE;
    instrumentPresets[228].patch.p_shakerParticles = 0.29f;
    instrumentPresets[228].patch.p_shakerDecayRate = 0.45f;
    instrumentPresets[228].patch.p_shakerResonance = 0.7f;
    instrumentPresets[228].patch.p_shakerBrightness = 0.6f;
    instrumentPresets[228].patch.p_shakerScrape = 0.0f;
    instrumentPresets[228].patch.p_envelopeEnabled = true;
    instrumentPresets[228].patch.p_attack = 0.001f;
    instrumentPresets[228].patch.p_decay = 0.3f;
    instrumentPresets[228].patch.p_sustain = 0.6f;
    instrumentPresets[228].patch.p_release = 0.2f;
    instrumentPresets[228].patch.p_expDecay = true;
    instrumentPresets[228].patch.p_oneShot = false;
    instrumentPresets[228].patch.p_useTriggerFreq = true;
    instrumentPresets[228].patch.p_triggerFreq = 2800.0f;
    instrumentPresets[228].patch.p_volume = 0.40f;

    // 229: Sleigh Bells — bright, festive, long shimmer
    instrumentPresets[229].name = "Sleigh Bells";
    instrumentPresets[229].patch = createDefaultPatch(WAVE_SHAKER);
    instrumentPresets[229].patch.p_shakerPreset = SHAKER_SLEIGH_BELLS;
    instrumentPresets[229].patch.p_shakerParticles = 0.71f;
    instrumentPresets[229].patch.p_shakerDecayRate = 0.80f;
    instrumentPresets[229].patch.p_shakerResonance = 0.75f;
    instrumentPresets[229].patch.p_shakerBrightness = 0.8f;
    instrumentPresets[229].patch.p_shakerScrape = 0.0f;
    instrumentPresets[229].patch.p_envelopeEnabled = true;
    instrumentPresets[229].patch.p_attack = 0.001f;
    instrumentPresets[229].patch.p_decay = 0.5f;
    instrumentPresets[229].patch.p_sustain = 0.5f;
    instrumentPresets[229].patch.p_release = 0.4f;
    instrumentPresets[229].patch.p_expDecay = true;
    instrumentPresets[229].patch.p_oneShot = false;
    instrumentPresets[229].patch.p_useTriggerFreq = true;
    instrumentPresets[229].patch.p_triggerFreq = 4500.0f;
    instrumentPresets[229].patch.p_volume = 0.22f;

    // 230: Bamboo Chime — sparse, woody, lower
    instrumentPresets[230].name = "Bamboo";
    instrumentPresets[230].patch = createDefaultPatch(WAVE_SHAKER);
    instrumentPresets[230].patch.p_shakerPreset = SHAKER_BAMBOO;
    instrumentPresets[230].patch.p_shakerParticles = 0.04f;
    instrumentPresets[230].patch.p_shakerDecayRate = 0.70f;
    instrumentPresets[230].patch.p_shakerResonance = 0.55f;
    instrumentPresets[230].patch.p_shakerBrightness = 0.3f;
    instrumentPresets[230].patch.p_shakerScrape = 0.0f;
    instrumentPresets[230].patch.p_envelopeEnabled = true;
    instrumentPresets[230].patch.p_attack = 0.001f;
    instrumentPresets[230].patch.p_decay = 1.0f;
    instrumentPresets[230].patch.p_sustain = 0.0f;
    instrumentPresets[230].patch.p_release = 0.3f;
    instrumentPresets[230].patch.p_expDecay = true;
    instrumentPresets[230].patch.p_oneShot = true;
    instrumentPresets[230].patch.p_useTriggerFreq = true;
    instrumentPresets[230].patch.p_triggerFreq = 1200.0f;
    instrumentPresets[230].patch.p_volume = 0.55f;

    // 231: Rain Stick — ambient, slow cascade, wide spectrum
    instrumentPresets[231].name = "Rain Stick";
    instrumentPresets[231].patch = createDefaultPatch(WAVE_SHAKER);
    instrumentPresets[231].patch.p_shakerPreset = SHAKER_RAIN_STICK;
    instrumentPresets[231].patch.p_shakerParticles = 1.0f;
    instrumentPresets[231].patch.p_shakerDecayRate = 0.95f;
    instrumentPresets[231].patch.p_shakerResonance = 0.3f;
    instrumentPresets[231].patch.p_shakerBrightness = 0.4f;
    instrumentPresets[231].patch.p_shakerScrape = 0.0f;
    instrumentPresets[231].patch.p_envelopeEnabled = true;
    instrumentPresets[231].patch.p_attack = 0.01f;
    instrumentPresets[231].patch.p_decay = 1.0f;
    instrumentPresets[231].patch.p_sustain = 0.4f;
    instrumentPresets[231].patch.p_release = 1.0f;
    instrumentPresets[231].patch.p_expDecay = true;
    instrumentPresets[231].patch.p_oneShot = false;
    instrumentPresets[231].patch.p_useTriggerFreq = true;
    instrumentPresets[231].patch.p_triggerFreq = 2000.0f;
    instrumentPresets[231].patch.p_volume = 0.50f;

    // 232: Guiro — scraping, periodic texture
    instrumentPresets[232].name = "Guiro";
    instrumentPresets[232].patch = createDefaultPatch(WAVE_SHAKER);
    instrumentPresets[232].patch.p_shakerPreset = SHAKER_GUIRO;
    instrumentPresets[232].patch.p_shakerParticles = 0.07f;
    instrumentPresets[232].patch.p_shakerDecayRate = 0.35f;
    instrumentPresets[232].patch.p_shakerResonance = 0.5f;
    instrumentPresets[232].patch.p_shakerBrightness = 0.45f;
    instrumentPresets[232].patch.p_shakerScrape = 0.7f;
    instrumentPresets[232].patch.p_envelopeEnabled = true;
    instrumentPresets[232].patch.p_attack = 0.001f;
    instrumentPresets[232].patch.p_decay = 0.4f;
    instrumentPresets[232].patch.p_sustain = 0.0f;
    instrumentPresets[232].patch.p_release = 0.1f;
    instrumentPresets[232].patch.p_expDecay = true;
    instrumentPresets[232].patch.p_oneShot = true;
    instrumentPresets[232].patch.p_useTriggerFreq = true;
    instrumentPresets[232].patch.p_triggerFreq = 2500.0f;
    instrumentPresets[232].patch.p_volume = 0.55f;

    // 233: Sandpaper — dense noise texture, short
    instrumentPresets[233].name = "Sandpaper";
    instrumentPresets[233].patch = createDefaultPatch(WAVE_SHAKER);
    instrumentPresets[233].patch.p_shakerPreset = SHAKER_SANDPAPER;
    instrumentPresets[233].patch.p_shakerParticles = 0.75f;
    instrumentPresets[233].patch.p_shakerDecayRate = 0.10f;
    instrumentPresets[233].patch.p_shakerResonance = 0.2f;
    instrumentPresets[233].patch.p_shakerBrightness = 0.5f;
    instrumentPresets[233].patch.p_shakerScrape = 0.0f;
    instrumentPresets[233].patch.p_envelopeEnabled = true;
    instrumentPresets[233].patch.p_attack = 0.001f;
    instrumentPresets[233].patch.p_decay = 0.12f;
    instrumentPresets[233].patch.p_sustain = 0.0f;
    instrumentPresets[233].patch.p_release = 0.05f;
    instrumentPresets[233].patch.p_expDecay = true;
    instrumentPresets[233].patch.p_oneShot = true;
    instrumentPresets[233].patch.p_useTriggerFreq = true;
    instrumentPresets[233].patch.p_triggerFreq = 4000.0f;
    instrumentPresets[233].patch.p_volume = 0.50f;

    // ========================================================================
    // BANDED WAVEGUIDE (glass, bowls, bowed bars, chimes)
    // ========================================================================

    // 234: Glass Harmonica — bowed, pure, ethereal
    instrumentPresets[234].name = "Glass Harmonica";
    instrumentPresets[234].patch = createDefaultPatch(WAVE_BANDEDWG);
    instrumentPresets[234].patch.p_bandedwgPreset = BANDEDWG_GLASS;
    instrumentPresets[234].patch.p_bandedwgBowPressure = 0.4f;
    instrumentPresets[234].patch.p_bandedwgBowSpeed = 0.6f;
    instrumentPresets[234].patch.p_bandedwgStrikePos = 0.5f;
    instrumentPresets[234].patch.p_bandedwgBrightness = 0.8f;
    instrumentPresets[234].patch.p_bandedwgSustain = 0.85f;
    instrumentPresets[234].patch.p_envelopeEnabled = true;
    instrumentPresets[234].patch.p_attack = 0.15f;
    instrumentPresets[234].patch.p_decay = 0.3f;
    instrumentPresets[234].patch.p_sustain = 0.9f;
    instrumentPresets[234].patch.p_release = 1.5f;
    instrumentPresets[234].patch.p_volume = 0.50f;

    // 235: Singing Bowl — bowed, warm, beating modes, meditative
    instrumentPresets[235].name = "Singing Bowl";
    instrumentPresets[235].patch = createDefaultPatch(WAVE_BANDEDWG);
    instrumentPresets[235].patch.p_bandedwgPreset = BANDEDWG_SINGING_BOWL;
    instrumentPresets[235].patch.p_bandedwgBowPressure = 0.5f;
    instrumentPresets[235].patch.p_bandedwgBowSpeed = 0.4f;
    instrumentPresets[235].patch.p_bandedwgStrikePos = 0.3f;
    instrumentPresets[235].patch.p_bandedwgBrightness = 0.5f;
    instrumentPresets[235].patch.p_bandedwgSustain = 0.9f;
    instrumentPresets[235].patch.p_envelopeEnabled = true;
    instrumentPresets[235].patch.p_attack = 0.2f;
    instrumentPresets[235].patch.p_decay = 0.4f;
    instrumentPresets[235].patch.p_sustain = 0.85f;
    instrumentPresets[235].patch.p_release = 2.0f;
    instrumentPresets[235].patch.p_volume = 0.55f;

    // 236: Bowed Vibraphone — bright, metallic sustain
    instrumentPresets[236].name = "Bowed Vibes";
    instrumentPresets[236].patch = createDefaultPatch(WAVE_BANDEDWG);
    instrumentPresets[236].patch.p_bandedwgPreset = BANDEDWG_VIBRAPHONE;
    instrumentPresets[236].patch.p_bandedwgBowPressure = 0.45f;
    instrumentPresets[236].patch.p_bandedwgBowSpeed = 0.5f;
    instrumentPresets[236].patch.p_bandedwgStrikePos = 0.6f;
    instrumentPresets[236].patch.p_bandedwgBrightness = 0.75f;
    instrumentPresets[236].patch.p_bandedwgSustain = 0.8f;
    instrumentPresets[236].patch.p_envelopeEnabled = true;
    instrumentPresets[236].patch.p_attack = 0.1f;
    instrumentPresets[236].patch.p_decay = 0.3f;
    instrumentPresets[236].patch.p_sustain = 0.8f;
    instrumentPresets[236].patch.p_release = 1.0f;
    instrumentPresets[236].patch.p_volume = 0.50f;

    // 237: Wine Glass — bowed, very pure, high, delicate
    instrumentPresets[237].name = "Wine Glass";
    instrumentPresets[237].patch = createDefaultPatch(WAVE_BANDEDWG);
    instrumentPresets[237].patch.p_bandedwgPreset = BANDEDWG_WINE_GLASS;
    instrumentPresets[237].patch.p_bandedwgBowPressure = 0.3f;
    instrumentPresets[237].patch.p_bandedwgBowSpeed = 0.7f;
    instrumentPresets[237].patch.p_bandedwgStrikePos = 0.5f;
    instrumentPresets[237].patch.p_bandedwgBrightness = 0.9f;
    instrumentPresets[237].patch.p_bandedwgSustain = 0.9f;
    instrumentPresets[237].patch.p_envelopeEnabled = true;
    instrumentPresets[237].patch.p_attack = 0.25f;
    instrumentPresets[237].patch.p_decay = 0.2f;
    instrumentPresets[237].patch.p_sustain = 0.95f;
    instrumentPresets[237].patch.p_release = 2.0f;
    instrumentPresets[237].patch.p_volume = 0.45f;

    // 238: Prayer Bowl — struck, deep, long ring
    instrumentPresets[238].name = "Prayer Bowl";
    instrumentPresets[238].patch = createDefaultPatch(WAVE_BANDEDWG);
    instrumentPresets[238].patch.p_bandedwgPreset = BANDEDWG_PRAYER_BOWL;
    instrumentPresets[238].patch.p_bandedwgBowPressure = 0.9f;
    instrumentPresets[238].patch.p_bandedwgBowSpeed = 0.5f;
    instrumentPresets[238].patch.p_bandedwgStrikePos = 0.4f;
    instrumentPresets[238].patch.p_bandedwgBrightness = 0.5f;
    instrumentPresets[238].patch.p_bandedwgSustain = 0.95f;
    instrumentPresets[238].patch.p_envelopeEnabled = true;
    instrumentPresets[238].patch.p_attack = 0.001f;
    instrumentPresets[238].patch.p_decay = 3.0f;
    instrumentPresets[238].patch.p_sustain = 0.0f;
    instrumentPresets[238].patch.p_release = 2.0f;
    instrumentPresets[238].patch.p_expDecay = true;
    instrumentPresets[238].patch.p_oneShot = true;
    instrumentPresets[238].patch.p_volume = 0.70f;

    // 239: Tubular Chime — struck, bright, harmonic
    instrumentPresets[239].name = "Tubular Chime";
    instrumentPresets[239].patch = createDefaultPatch(WAVE_BANDEDWG);
    instrumentPresets[239].patch.p_bandedwgPreset = BANDEDWG_TUBULAR;
    instrumentPresets[239].patch.p_bandedwgBowPressure = 0.85f;
    instrumentPresets[239].patch.p_bandedwgBowSpeed = 0.5f;
    instrumentPresets[239].patch.p_bandedwgStrikePos = 0.5f;
    instrumentPresets[239].patch.p_bandedwgBrightness = 0.7f;
    instrumentPresets[239].patch.p_bandedwgSustain = 0.8f;
    instrumentPresets[239].patch.p_envelopeEnabled = true;
    instrumentPresets[239].patch.p_attack = 0.001f;
    instrumentPresets[239].patch.p_decay = 2.5f;
    instrumentPresets[239].patch.p_sustain = 0.0f;
    instrumentPresets[239].patch.p_release = 1.5f;
    instrumentPresets[239].patch.p_expDecay = true;
    instrumentPresets[239].patch.p_oneShot = true;
    instrumentPresets[239].patch.p_volume = 0.65f;
}

#endif // INSTRUMENT_PRESETS_H
