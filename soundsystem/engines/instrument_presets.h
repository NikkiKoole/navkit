// instrument_presets.h — Shared instrument preset definitions
//
// Extracted from demo.c so multiple frontends (demo, daw_hz) can share presets.
// Include after synth_patch.h.

#ifndef INSTRUMENT_PRESETS_H
#define INSTRUMENT_PRESETS_H

#include "synth_patch.h"

typedef struct {
    const char *name;
    SynthPatch patch;
} InstrumentPreset;

#define NUM_INSTRUMENT_PRESETS 24
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
}

#endif // INSTRUMENT_PRESETS_H
