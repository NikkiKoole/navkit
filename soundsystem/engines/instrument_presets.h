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

#define NUM_INSTRUMENT_PRESETS 66
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

    // Acid Lead - Resonant saw with filter env
    instrumentPresets[8].name = "Acid";
    instrumentPresets[8].patch.p_waveType = WAVE_SAW;
    instrumentPresets[8].patch.p_attack = 0.005f;
    instrumentPresets[8].patch.p_decay = 0.2f;
    instrumentPresets[8].patch.p_sustain = 0.3f;
    instrumentPresets[8].patch.p_release = 0.15f;
    instrumentPresets[8].patch.p_filterCutoff = 0.2f;
    instrumentPresets[8].patch.p_filterResonance = 0.7f;
    instrumentPresets[8].patch.p_filterEnvAmt = 0.6f;
    instrumentPresets[8].patch.p_filterEnvDecay = 0.12f;
    instrumentPresets[8].patch.p_monoMode = true;
    instrumentPresets[8].patch.p_glideTime = 0.05f;

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
    instrumentPresets[10].patch.p_pluckBrightness = 0.6f;
    instrumentPresets[10].patch.p_pluckDamping = 0.997f;

    // Marimba
    instrumentPresets[11].name = "Marimba";
    instrumentPresets[11].patch.p_waveType = WAVE_MALLET;
    instrumentPresets[11].patch.p_malletPreset = MALLET_PRESET_MARIMBA;
    instrumentPresets[11].patch.p_malletStiffness = 0.3f;
    instrumentPresets[11].patch.p_malletHardness = 0.5f;

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
    instrumentPresets[14].patch.p_fmModIndex = 2.5f;
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
    instrumentPresets[15].patch.p_malletPreset = MALLET_PRESET_GLOCKEN;
    instrumentPresets[15].patch.p_malletStiffness = 0.95f;
    instrumentPresets[15].patch.p_malletHardness = 0.9f;
    instrumentPresets[15].patch.p_filterCutoff = 1.0f;

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
    instrumentPresets[17].patch.p_pluckBrightness = 0.7f;
    instrumentPresets[17].patch.p_pluckDamping = 0.995f;
    instrumentPresets[17].patch.p_filterCutoff = 0.9f;

    // ========================================================================
    // MAC DEMARCO / SLACKER INDIE PRESETS (18-23)
    // ========================================================================

    // Mac Jangle - Chorus-y jangly guitar
    instrumentPresets[18].name = "Mac Jangle";
    instrumentPresets[18].patch.p_waveType = WAVE_PLUCK;
    instrumentPresets[18].patch.p_pluckBrightness = 0.5f;
    instrumentPresets[18].patch.p_pluckDamping = 0.994f;
    instrumentPresets[18].patch.p_filterCutoff = 0.6f;
    instrumentPresets[18].patch.p_filterResonance = 0.15f;
    instrumentPresets[18].patch.p_vibratoRate = 4.5f;
    instrumentPresets[18].patch.p_vibratoDepth = 0.15f;

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
    instrumentPresets[21].patch.p_fmModIndex = 1.2f;
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
    instrumentPresets[23].patch.p_malletPreset = MALLET_PRESET_VIBES;
    instrumentPresets[23].patch.p_malletTremolo = 0.6f;
    instrumentPresets[23].patch.p_malletTremoloRate = 4.5f;
    instrumentPresets[23].patch.p_filterCutoff = 0.7f;
    instrumentPresets[23].patch.p_vibratoRate = 0.5f;
    instrumentPresets[23].patch.p_vibratoDepth = 0.1f;

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
    instrumentPresets[29].patch.p_fmModIndex = 0.15f;       // Hint of harmonics (triangle blend)
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
    instrumentPresets[32].patch.p_fmModIndex = 0.15f;        // Subtle (drums.h: sin*2 * 0.15)
    instrumentPresets[32].patch.p_attack = 0.0f;
    instrumentPresets[32].patch.p_decay = 0.25f;
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
    instrumentPresets[44].patch.p_pluckBrightness = 0.4f;
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
    instrumentPresets[56].patch.p_fmModIndex = 0.8f;
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
    instrumentPresets[57].patch.p_fmModIndex = 1.8f;
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
    instrumentPresets[58].patch.p_pluckBrightness = 0.35f;
    instrumentPresets[58].patch.p_pluckDamping = 0.993f;
    instrumentPresets[58].patch.p_filterCutoff = 0.25f;
    instrumentPresets[58].patch.p_filterResonance = 0.1f;
    instrumentPresets[58].patch.p_volume = 0.6f;
    instrumentPresets[58].patch.p_analogRolloff = true;
    instrumentPresets[58].patch.p_tubeSaturation = true;

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
    instrumentPresets[62].patch.p_pluckBrightness = 0.45f;
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
}

#endif // INSTRUMENT_PRESETS_H
