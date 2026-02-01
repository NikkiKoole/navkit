// PixelSynth - Pikuniku Style Presets
// "Sillycore" / Shibuya-kei for games - Calum Bowen (bo en) inspired sounds
//
// These presets create the bright, quirky, "toy-like" aesthetic of Pikuniku:
// - Clean but digital (no heavy reverb)
// - Bouncy and jaunty
// - Nasal/thin leads
// - Bright FM bells
// - "Farty" tuba bass

#ifndef PIXELSYNTH_PIKU_PRESETS_H
#define PIXELSYNTH_PIKU_PRESETS_H

#include "synth.h"
#include "effects.h"
#include "sequencer.h"

// ============================================================================
// PIKUNIKU PRESET FUNCTIONS
// Call these to configure the global synth context for specific sounds
// ============================================================================

// ----------------------------------------------------------------------------
// 1. PIKU ACCORDION - The "nasal" lead
// Thin pulse wave with vibrato - sounds like a toy accordion/melodica
// Recipe: Thin pulse width + slow vibrato + minimal filter
// ----------------------------------------------------------------------------
static void pikuPresetAccordion(void) {
    _ensureSynthCtx();
    
    // Thin pulse wave (10-15% duty cycle = nasal/reedy character)
    notePulseWidth = 0.15f;
    notePwmRate = 0.0f;         // No PWM - keep it thin and consistent
    notePwmDepth = 0.0f;
    
    // Slow vibrato for "wobbly" feel
    noteVibratoRate = 5.0f;
    noteVibratoDepth = 0.3f;    // Subtle pitch drift
    
    // Bright, open filter (Pikuniku is bright and dry)
    noteFilterCutoff = 0.9f;
    noteFilterResonance = 0.1f;
    noteFilterEnvAmt = 0.0f;    // No filter envelope
    
    // Snappy envelope
    noteAttack = 0.005f;
    noteDecay = 0.1f;
    noteSustain = 0.7f;
    noteRelease = 0.15f;
    
    noteVolume = 0.5f;
}

// ----------------------------------------------------------------------------
// 2. PIKU TUBA BASS - The "farty" bouncy bass
// Sawtooth + resonant lowpass with envelope = "bwa" attack
// Recipe: Saw + low cutoff + high resonance + short filter envelope
// ----------------------------------------------------------------------------
static void pikuPresetTubaBass(void) {
    _ensureSynthCtx();
    
    // Start with sawtooth (rich harmonics to filter)
    notePulseWidth = 0.5f;      // Not used for saw, but reset
    notePwmRate = 0.0f;
    notePwmDepth = 0.0f;
    
    // No vibrato on bass
    noteVibratoRate = 0.0f;
    noteVibratoDepth = 0.0f;
    
    // Low cutoff with resonance = "chirpy" attack
    noteFilterCutoff = 0.25f;   // Low - around 200-400Hz feel
    noteFilterResonance = 0.6f; // High resonance for the "bwa" chirp
    noteFilterEnvAmt = 0.4f;    // Envelope opens filter briefly
    noteFilterEnvAttack = 0.001f;
    noteFilterEnvDecay = 0.08f; // Short decay = punchy "bwa"
    
    // Bouncy envelope
    noteAttack = 0.005f;
    noteDecay = 0.2f;
    noteSustain = 0.4f;
    noteRelease = 0.1f;
    
    noteVolume = 0.6f;
}

// ----------------------------------------------------------------------------
// 3. PIKU FM BELL - Bright, glassy toy bell
// 2-operator FM with ratio ~3.5 = metallic/sparkly character
// Recipe: Simple FM, moderate index, fast decay
// ----------------------------------------------------------------------------
static void pikuPresetFMBell(void) {
    _ensureSynthCtx();
    
    // FM settings for bright bell/glass sound
    fmModRatio = 3.5f;          // Non-integer ratio = metallic/bell character
    fmModIndex = 2.5f;          // Moderate modulation for sparkle
    fmFeedback = 0.0f;          // No feedback = cleaner bell
    
    // Bright filter
    noteFilterCutoff = 1.0f;    // Wide open
    noteFilterResonance = 0.0f;
    noteFilterEnvAmt = 0.0f;
    
    // Bell envelope: instant attack, medium decay
    noteAttack = 0.001f;
    noteDecay = 0.8f;
    noteSustain = 0.0f;         // No sustain - pure decay
    noteRelease = 0.5f;
    
    // Slight vibrato for "toy" character
    noteVibratoRate = 6.0f;
    noteVibratoDepth = 0.1f;
    
    noteVolume = 0.4f;
}

// ----------------------------------------------------------------------------
// 4. PIKU GLOCKENSPIEL - Bright metallic toy xylophone
// Uses the mallet engine with glocken preset
// Recipe: High-pitched mallet with inharmonic partials
// ----------------------------------------------------------------------------
static void pikuPresetGlockenspiel(void) {
    _ensureSynthCtx();
    
    // Use mallet engine with glockenspiel preset
    malletPreset = MALLET_PRESET_GLOCKEN;
    malletStiffness = 0.95f;    // Steel bars
    malletHardness = 0.9f;      // Hard mallets = bright attack
    malletStrikePos = 0.15f;
    malletResonance = 0.3f;     // Not too resonant
    malletTremolo = 0.0f;       // No tremolo
    
    // Bright filter
    noteFilterCutoff = 1.0f;
    noteFilterResonance = 0.0f;
    
    noteVolume = 0.45f;
}

// ----------------------------------------------------------------------------
// 5. PIKU TOY PIANO - Slightly detuned, clunky character
// Additive synthesis with slightly inharmonic partials
// Recipe: Bell-ish additive preset + light chorus
// ----------------------------------------------------------------------------
static void pikuPresetToyPiano(void) {
    _ensureSynthCtx();
    
    // Use additive with slight bell character
    additivePreset = ADDITIVE_PRESET_BELL;
    additiveBrightness = 0.6f;
    additiveInharmonicity = 0.01f;  // Slight stretch for "toy" quality
    additiveShimmer = 0.1f;         // Tiny movement
    
    // Quick decay like a toy piano
    noteAttack = 0.001f;
    noteDecay = 0.6f;
    noteSustain = 0.1f;
    noteRelease = 0.3f;
    
    noteFilterCutoff = 0.85f;
    noteFilterResonance = 0.1f;
    
    noteVolume = 0.45f;
}

// ----------------------------------------------------------------------------
// 6. PIKU WOODBLOCK - Organic percussion
// Synthesized woodblock using sine + fast pitch envelope
// Recipe: High sine + envelope-to-pitch = "knock" sound
// ----------------------------------------------------------------------------
static void pikuPresetWoodblock(void) {
    _ensureSynthCtx();
    
    // Use additive with just fundamental (sine)
    additivePreset = ADDITIVE_PRESET_SINE;
    
    // Very short envelope = percussive click
    noteAttack = 0.001f;
    noteDecay = 0.05f;          // 50ms = woodblock
    noteSustain = 0.0f;
    noteRelease = 0.02f;
    
    // High pitch, open filter
    noteFilterCutoff = 1.0f;
    noteFilterResonance = 0.0f;
    
    // Pitch drops quickly after attack (woodblock character)
    // Note: This uses FM to simulate pitch envelope
    fmModRatio = 1.0f;
    fmModIndex = 0.0f;          // No FM modulation
    
    noteVolume = 0.5f;
}

// ----------------------------------------------------------------------------
// 7. PIKU BOING - Cartoon bounce sound
// Sine with longer pitch envelope for "boing" effect
// Recipe: Sine + slow pitch drop = cartoon bounce
// ----------------------------------------------------------------------------
static void pikuPresetBoing(void) {
    _ensureSynthCtx();
    
    // Sine wave base
    additivePreset = ADDITIVE_PRESET_SINE;
    
    // Medium decay for "boing" sustain
    noteAttack = 0.001f;
    noteDecay = 0.3f;           // 300ms = "boing" tail
    noteSustain = 0.0f;
    noteRelease = 0.1f;
    
    // Use pitch LFO for bounce (single cycle)
    notePitchLfoRate = 8.0f;
    notePitchLfoDepth = 3.0f;   // Large pitch swing
    notePitchLfoShape = 0;      // Sine
    
    noteFilterCutoff = 0.8f;
    noteFilterResonance = 0.2f;
    
    noteVolume = 0.5f;
}

// ----------------------------------------------------------------------------
// 8. PIKU BUBBLE - Foley-style water pop
// High sine + very fast LFO = frequency modulation = "blip"
// Recipe: High sine + fast pitch LFO + very short envelope
// ----------------------------------------------------------------------------
static void pikuPresetBubble(void) {
    _ensureSynthCtx();
    
    // Sine base
    additivePreset = ADDITIVE_PRESET_SINE;
    
    // Very fast gate envelope
    noteAttack = 0.001f;
    noteDecay = 0.03f;          // 30ms = tiny pop
    noteSustain = 0.0f;
    noteRelease = 0.01f;
    
    // Fast pitch wobble = bubble character
    notePitchLfoRate = 40.0f;   // Very fast
    notePitchLfoDepth = 2.0f;   // 2 semitones wobble
    notePitchLfoShape = 0;      // Sine
    
    noteFilterCutoff = 1.0f;
    noteFilterResonance = 0.0f;
    
    noteVolume = 0.4f;
}

// ----------------------------------------------------------------------------
// 9. PIKU CHIRP - Bird-like staccato
// Uses the bird synthesis engine for game-appropriate bird sounds
// Recipe: Tweet preset = short staccato down-chirp
// ----------------------------------------------------------------------------
static void pikuPresetChirp(void) {
    _ensureSynthCtx();
    
    // Bird synthesis
    birdType = BIRD_TWEET;
    birdChirpRange = 0.8f;      // Moderate pitch range
    birdTrillRate = 0.0f;       // No trill
    birdTrillDepth = 0.0f;
    birdAmRate = 0.0f;          // No AM
    birdAmDepth = 0.0f;
    birdHarmonics = 0.1f;       // Mostly pure
    
    noteVolume = 0.4f;
}

// ----------------------------------------------------------------------------
// 10. PIKU PLUCK - Cute pizzicato
// Karplus-Strong for bouncy plucked string character
// Recipe: Pluck with high brightness, short decay
// ----------------------------------------------------------------------------
static void pikuPresetPluck(void) {
    _ensureSynthCtx();
    
    // Karplus-Strong settings
    pluckBrightness = 0.7f;     // Bright pluck
    pluckDamping = 0.995f;      // Medium-short decay
    
    // Envelope (mostly handled by K-S, but set release)
    noteAttack = 0.0f;
    noteDecay = 0.1f;
    noteSustain = 0.0f;
    noteRelease = 0.2f;
    
    // Open filter
    noteFilterCutoff = 0.9f;
    noteFilterResonance = 0.1f;
    
    noteVolume = 0.5f;
}

// ============================================================================
// PIKUNIKU EFFECTS PRESETS
// Configure the effects chain for that bright, clean Pikuniku aesthetic
// ============================================================================

// ----------------------------------------------------------------------------
// PIKU EFFECTS: Clean with subtle character
// The Pikuniku aesthetic is BRIGHT and DRY - avoid heavy reverb!
// ----------------------------------------------------------------------------
static void pikuEffectsClean(void) {
    _ensureFxCtx();
    
    // Disable most effects for clean sound
    fx.distEnabled = false;
    fx.delayEnabled = false;
    fx.tapeEnabled = false;
    fx.reverbEnabled = false;
    fx.crushEnabled = false;
    fx.chorusEnabled = false;
}

// ----------------------------------------------------------------------------
// PIKU EFFECTS: 9-bit character
// Subtle bitcrusher for that "cheap Japanese toy" feel
// ----------------------------------------------------------------------------
static void pikuEffects9Bit(void) {
    _ensureFxCtx();
    
    // Start clean
    fx.distEnabled = false;
    fx.delayEnabled = false;
    fx.tapeEnabled = false;
    fx.reverbEnabled = false;
    
    // Subtle bitcrusher - not harsh, just "lo-fi cute"
    fx.crushEnabled = true;
    fx.crushBits = 9.0f;        // 9-bit = slight crunch
    fx.crushRate = 2.0f;        // Light sample rate reduction
    fx.crushMix = 0.3f;         // Subtle mix
    
    // No chorus here - use pikuEffectsWobbly for that
    fx.chorusEnabled = false;
}

// ----------------------------------------------------------------------------
// PIKU EFFECTS: Wobbly leads
// Chorus for thin synths that need to feel "broken and cute"
// ----------------------------------------------------------------------------
static void pikuEffectsWobbly(void) {
    _ensureFxCtx();
    
    // Clean base
    fx.distEnabled = false;
    fx.delayEnabled = false;
    fx.tapeEnabled = false;
    fx.reverbEnabled = false;
    fx.crushEnabled = false;
    
    // Gentle chorus for pitch drift
    fx.chorusEnabled = true;
    fx.chorusRate = 1.2f;       // Slow wobble
    fx.chorusDepth = 0.3f;      // Subtle depth
    fx.chorusMix = 0.4f;        // Moderate mix
    fx.chorusDelay = 0.012f;    // 12ms base
    fx.chorusFeedback = 0.0f;   // No feedback (not flanging)
}

// ----------------------------------------------------------------------------
// PIKU EFFECTS: Full toy character
// Combines 9-bit crunch with subtle wobble
// ----------------------------------------------------------------------------
static void pikuEffectsToy(void) {
    _ensureFxCtx();
    
    // No heavy effects
    fx.distEnabled = false;
    fx.delayEnabled = false;
    fx.tapeEnabled = false;
    fx.reverbEnabled = false;
    
    // 9-bit crunch
    fx.crushEnabled = true;
    fx.crushBits = 9.0f;
    fx.crushRate = 2.0f;
    fx.crushMix = 0.25f;
    
    // Subtle chorus
    fx.chorusEnabled = true;
    fx.chorusRate = 1.0f;
    fx.chorusDepth = 0.25f;
    fx.chorusMix = 0.3f;
    fx.chorusDelay = 0.015f;
    fx.chorusFeedback = 0.0f;
}

// ============================================================================
// MAC DEMARCO / SLACKER INDIE EFFECTS PRESETS
// Tape warble, chorus, lo-fi warmth
// ============================================================================

// ----------------------------------------------------------------------------
// MAC EFFECTS: Tape + Chorus (the classic Mac sound)
// Heavy chorus + tape wow/flutter = seasick lo-fi vibes
// ----------------------------------------------------------------------------
static void macEffectsTapeChorus(void) {
    _ensureFxCtx();
    
    // No distortion or crush
    fx.distEnabled = false;
    fx.crushEnabled = false;
    
    // Deep chorus (Juno-style)
    fx.chorusEnabled = true;
    fx.chorusRate = 0.8f;       // Slow, dreamy
    fx.chorusDepth = 0.6f;      // Deep modulation
    fx.chorusMix = 0.5f;        // 50/50 mix
    fx.chorusDelay = 0.018f;    // 18ms base delay
    fx.chorusFeedback = 0.1f;   // Tiny feedback for richness
    
    // Tape warble
    fx.tapeEnabled = true;
    fx.tapeSaturation = 0.4f;   // Warm compression
    fx.tapeWow = 0.5f;          // Noticeable pitch drift
    fx.tapeFlutter = 0.3f;      // Some flutter
    fx.tapeHiss = 0.08f;        // Touch of hiss
    
    // Subtle delay (slapback)
    fx.delayEnabled = true;
    fx.delayTime = 0.12f;       // Short slapback
    fx.delayFeedback = 0.2f;    // Few repeats
    fx.delayTone = 0.4f;        // Dark repeats
    fx.delayMix = 0.2f;         // Subtle
    
    // Small room reverb
    fx.reverbEnabled = true;
    fx.reverbSize = 0.3f;       // Small room
    fx.reverbDamping = 0.6f;    // Damped
    fx.reverbMix = 0.15f;       // Subtle
    fx.reverbPreDelay = 0.01f;
}

// ----------------------------------------------------------------------------
// MAC EFFECTS: Just Chorus (cleaner, still wobbly)
// For when you want the detune without the tape grit
// ----------------------------------------------------------------------------
static void macEffectsChorus(void) {
    _ensureFxCtx();
    
    // Clean
    fx.distEnabled = false;
    fx.crushEnabled = false;
    fx.tapeEnabled = false;
    
    // Deep chorus
    fx.chorusEnabled = true;
    fx.chorusRate = 0.6f;       // Slow
    fx.chorusDepth = 0.5f;      // Medium-deep
    fx.chorusMix = 0.45f;
    fx.chorusDelay = 0.015f;
    fx.chorusFeedback = 0.05f;
    
    // Light reverb
    fx.reverbEnabled = true;
    fx.reverbSize = 0.4f;
    fx.reverbDamping = 0.5f;
    fx.reverbMix = 0.2f;
    fx.reverbPreDelay = 0.015f;
    
    fx.delayEnabled = false;
}

// ----------------------------------------------------------------------------
// MAC EFFECTS: Full Lo-Fi (tape saturated, warbled, hissy)
// Maximum slacker vibes - like a well-loved cassette
// ----------------------------------------------------------------------------
static void macEffectsLoFi(void) {
    _ensureFxCtx();
    
    // No digital effects
    fx.distEnabled = false;
    fx.crushEnabled = false;
    
    // Chorus
    fx.chorusEnabled = true;
    fx.chorusRate = 0.9f;
    fx.chorusDepth = 0.55f;
    fx.chorusMix = 0.4f;
    fx.chorusDelay = 0.02f;
    fx.chorusFeedback = 0.15f;
    
    // Heavy tape character
    fx.tapeEnabled = true;
    fx.tapeSaturation = 0.6f;   // Warm saturation
    fx.tapeWow = 0.7f;          // Obvious pitch warble
    fx.tapeFlutter = 0.4f;      // Flutter
    fx.tapeHiss = 0.15f;        // Noticeable hiss
    
    // Washy reverb
    fx.reverbEnabled = true;
    fx.reverbSize = 0.5f;
    fx.reverbDamping = 0.7f;    // Dark reverb
    fx.reverbMix = 0.25f;
    fx.reverbPreDelay = 0.02f;
    
    fx.delayEnabled = false;
}

// ============================================================================
// PIKUNIKU SEQUENCER SETUP
// Configure the sequencer for bouncy, jaunty rhythms
// ============================================================================

// ----------------------------------------------------------------------------
// PIKU TIMING: Bouncy swing
// Set up Dilla-style micro-timing for that "skipping walk" feel
// Uses the global sequencer context
// ----------------------------------------------------------------------------
static void pikuTimingBouncy(void) {
    _ensureSeqCtx();
    
    // Moderate swing - not too heavy, just "jaunty"
    seq.dilla.swing = 5;       // Off-beats pushed late
    seq.dilla.jitter = 1;      // Tiny random variation
    
    // Slight nudge on kick for groove
    seq.dilla.kickNudge = -1;  // Kick slightly early = driving
    seq.dilla.snareDelay = 2;  // Snare slightly late = lazy bounce
    seq.dilla.hatNudge = 0;    // Hihat on grid
    seq.dilla.clapDelay = 2;   // Clap with snare
}

// ============================================================================
// CONVENIENCE: Quick setup functions
// ============================================================================

// Set up for Pikuniku-style lead melody
static void pikuSetupLead(void) {
    pikuPresetAccordion();
    pikuEffectsWobbly();
}

// Set up for Pikuniku-style bass
static void pikuSetupBass(void) {
    pikuPresetTubaBass();
    pikuEffectsClean();
}

// Set up for Pikuniku-style bells/chimes
static void pikuSetupBells(void) {
    pikuPresetFMBell();
    pikuEffects9Bit();
}

// Set up full "toy" aesthetic (lead + effects)
static void pikuSetupToy(void) {
    pikuPresetToyPiano();
    pikuEffectsToy();
}

#endif // PIXELSYNTH_PIKU_PRESETS_H
