#ifndef PHRASE_PALETTE_EMBEDDED_H
#define PHRASE_PALETTE_EMBEDDED_H

#include "../../src/sound/sound_phrase.h"

// Embedded sound palette defaults
// To customize, edit these values and recompile
static const SoundPalette EMBEDDED_SOUND_PALETTE = {
    .callBaseMidiMin = 60.0f,
    .callBaseMidiMax = 78.0f,
    .callTokensMin = 3,
    .callTokensMax = 6,
    
    .songBaseMidiMin = 48.0f,
    .songBaseMidiMax = 72.0f,
    .songMotifMin = 4,
    .songMotifMax = 7,
    .songPhraseMin = 2,
    .songPhraseMax = 3,
    
    .birdDurMin = 0.08f,
    .birdDurMax = 0.35f,
    .birdGapMin = 0.02f,
    .birdGapMax = 0.15f,
    .birdIntensityMin = 0.35f,
    .birdIntensityMax = 0.90f,
    
    .vowelDurMin = 0.12f,
    .vowelDurMax = 0.60f,
    .vowelGapMin = 0.04f,
    .vowelGapMax = 0.20f,
    .vowelIntensityMin = 0.30f,
    .vowelIntensityMax = 0.80f,
    
    .consDurMin = 0.03f,
    .consDurMax = 0.12f,
    .consGapMin = 0.01f,
    .consGapMax = 0.08f,
    .consIntensityMin = 0.20f,
    .consIntensityMax = 0.60f
};

#endif
