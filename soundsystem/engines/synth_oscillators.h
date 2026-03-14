// PixelSynth - Oscillator Implementations
// All 16 synthesis engines: formant, pluck, bowed, pipe, additive, mallet,
// granular, FM, phase distortion, membrane, bird + standard wave types
// Extracted from synth.h — include after synth globals/helpers are defined

#ifndef PIXELSYNTH_SYNTH_OSCILLATORS_H
#define PIXELSYNTH_SYNTH_OSCILLATORS_H

// ============================================================================
// FORMANT DATA (for WAVE_VOICE)
// ============================================================================

static const float formantFreq[VOWEL_COUNT][3] = {
    { 800,  1200,  2500 },  // A - "ah"
    { 400,  2000,  2550 },  // E - "eh"  
    { 280,  2300,  2900 },  // I - "ee"
    { 450,   800,  2500 },  // O - "oh"
    { 325,   700,  2500 },  // U - "oo"
};

static const float formantBw[VOWEL_COUNT][3] = {
    { 80,   90,  120 },  // A
    { 70,  100,  120 },  // E
    { 50,   90,  120 },  // I
    { 70,   80,  120 },  // O
    { 50,   60,  120 },  // U
};

static const float formantAmp[VOWEL_COUNT][3] = {
    { 1.0f, 0.5f, 0.3f },  // A
    { 1.0f, 0.7f, 0.3f },  // E
    { 1.0f, 0.4f, 0.2f },  // I
    { 1.0f, 0.3f, 0.2f },  // O
    { 1.0f, 0.2f, 0.1f },  // U
};

// ============================================================================
// FORMANT FILTER PROCESSING
// ============================================================================

static float processFormantFilter(FormantFilter *f, float input, float sampleRate) {
    float fc = clampf(2.0f * sinf(PI * f->freq / sampleRate), 0.001f, 0.99f);
    float q = clampf(f->freq / (f->bw + 1.0f), 0.5f, 20.0f);
    
    f->low += fc * f->band;
    f->high = input - f->low - f->band / q;
    f->band += fc * f->high;
    
    return f->band;
}

// Voice oscillator (formant synthesis)
static float processVoiceOscillator(Voice *v, float sampleRate) {
    VoiceSettings *vs = &v->voiceSettings;
    float dt = 1.0f / sampleRate;
    
    // Track time for consonant attack
    vs->consonantTime += dt;
    
    // Decay formant filter states during release
    if (v->envStage == 4) {
        float decay = 0.995f;
        for (int i = 0; i < 3; i++) {
            vs->formants[i].low *= decay;
            vs->formants[i].band *= decay;
            vs->formants[i].high *= decay;
        }
        vs->nasalLow *= decay;
        vs->nasalBand *= decay;
    }
    
    // Apply vibrato
    float vibrato = 1.0f;
    if (vs->vibratoDepth > 0.0f) {
        vs->vibratoPhase += vs->vibratoRate * dt;
        if (vs->vibratoPhase > 1.0f) vs->vibratoPhase -= 1.0f;
        float semitones = sinf(vs->vibratoPhase * 2.0f * PI) * vs->vibratoDepth;
        vibrato = powf(2.0f, semitones / 12.0f);
    }
    
    // Consonant attack: pitch bend down at start
    float consonantPitchMod = 1.0f;
    if (vs->consonantEnabled && vs->consonantTime < 0.05f) {
        // Quick pitch drop then rise (like "ba" or "da")
        float t = vs->consonantTime / 0.05f;
        consonantPitchMod = 1.0f + (1.0f - t) * (1.0f - t) * 0.5f * vs->consonantAmount;
    }
    
    // Pitch envelope (intonation)
    float pitchEnvMod = 1.0f;
    if (fabsf(vs->pitchEnvAmount) > 0.01f && vs->pitchEnvTimer < vs->pitchEnvTime) {
        vs->pitchEnvTimer += dt;
        float t = vs->pitchEnvTimer / vs->pitchEnvTime;
        if (t > 1.0f) t = 1.0f;
        
        // Apply curve: negative = fast then slow, positive = slow then fast
        float curved;
        if (vs->pitchEnvCurve < 0.0f) {
            // Fast then slow (exponential out)
            float power = 1.0f + fabsf(vs->pitchEnvCurve) * 2.0f;
            curved = 1.0f - powf(1.0f - t, power);
        } else if (vs->pitchEnvCurve > 0.0f) {
            // Slow then fast (exponential in)
            float power = 1.0f + vs->pitchEnvCurve * 2.0f;
            curved = powf(t, power);
        } else {
            curved = t; // Linear
        }
        
        // Envelope goes from pitchEnvAmount semitones toward 0
        float semitones = vs->pitchEnvAmount * (1.0f - curved);
        pitchEnvMod = powf(2.0f, semitones / 12.0f);
    }
    
    // Advance phase
    float actualFreq = v->frequency * vibrato * consonantPitchMod * pitchEnvMod;
    v->phase += actualFreq / sampleRate;
    if (v->phase >= 1.0f) v->phase -= 1.0f;
    
    // Generate source signal (glottal pulse simulation)
    float smooth = 2.0f * fabsf(2.0f * v->phase - 1.0f) - 1.0f;
    float t = v->phase;
    float glottal = (t < 0.4f) ? sinf(t * PI / 0.4f) : -0.3f * sinf((t - 0.4f) * PI / 0.6f);
    float source = smooth * (1.0f - vs->buzziness) + glottal * vs->buzziness;
    
    // Mix in breathiness (noise)
    if (vs->breathiness > 0.0f) {
        source = source * (1.0f - vs->breathiness * 0.7f) + noise() * vs->breathiness * 0.5f;
    }
    
    // Consonant attack: add noise burst at start
    float consonantNoise = 0.0f;
    if (vs->consonantEnabled && vs->consonantTime < 0.03f) {
        // Sharp noise burst that fades quickly
        float env = 1.0f - (vs->consonantTime / 0.03f);
        env = env * env * env; // Cubic falloff for snappy attack
        consonantNoise = noise() * env * vs->consonantAmount * 0.8f;
    }
    
    // Interpolate formant parameters and apply filters
    VowelType v1 = vs->vowel;
    VowelType v2 = vs->nextVowel;
    float blend = vs->vowelBlend;
    
    float out = 0.0f;
    for (int i = 0; i < 3; i++) {
        float freq = lerpf(formantFreq[v1][i], formantFreq[v2][i], blend) * vs->formantShift;
        float bw = lerpf(formantBw[v1][i], formantBw[v2][i], blend);
        float amp = lerpf(formantAmp[v1][i], formantAmp[v2][i], blend);
        
        vs->formants[i].freq = freq;
        vs->formants[i].bw = bw;
        out += processFormantFilter(&vs->formants[i], source, sampleRate) * amp;
    }
    
    // Nasality: apply anti-formant (notch filter around 250-450Hz)
    if (vs->nasalEnabled && vs->nasalAmount > 0.0f) {
        // Nasal anti-formant centered around 350Hz
        float nasalFreq = 350.0f * vs->formantShift;
        float nasalBw = 100.0f;
        float fc = clampf(2.0f * sinf(PI * nasalFreq / sampleRate), 0.001f, 0.99f);
        float q = clampf(nasalFreq / (nasalBw + 1.0f), 0.5f, 10.0f);
        
        // Run notch filter
        vs->nasalLow += fc * vs->nasalBand;
        float nasalHigh = out - vs->nasalLow - vs->nasalBand / q;
        vs->nasalBand += fc * nasalHigh;
        
        // Notch = low + high (removes the band)
        float notched = vs->nasalLow + nasalHigh;
        
        // Also add a slight nasal resonance around 250Hz and 2500Hz
        float nasalResonance = sinf(v->phase * 2.0f * PI * 250.0f / v->frequency) * 0.1f;
        nasalResonance += sinf(v->phase * 2.0f * PI * 2500.0f / v->frequency) * 0.05f;
        
        // Blend between normal and nasal
        out = lerpf(out, notched + nasalResonance * vs->nasalAmount, vs->nasalAmount);
    }
    
    // Add consonant noise on top
    out += consonantNoise;
    
    return out * 0.7f;
}

// Karplus-Strong plucked string oscillator
static float processPluckOscillator(Voice *v, float sampleRate) {
    (void)sampleRate;
    if (v->ksLength <= 0) return 0.0f;
    
    // Read from delay line
    float sample = v->ksBuffer[v->ksIndex];
    
    // Get next sample for averaging (Karplus-Strong lowpass)
    int nextIndex = (v->ksIndex + 1) % v->ksLength;
    float nextSample = v->ksBuffer[nextIndex];
    
    // Karplus-Strong with brightness and tone damping
    // Brightness: 1=bright (more direct sample), 0=muted (more averaging)
    float avg = (sample + nextSample) * 0.5f;
    float bright = v->ksBrightness;
    float blended = avg + (sample - avg) * bright;
    // Tone damping: additional one-pole lowpass in feedback (pluckDamp 0=no effect, 1=heavy damping)
    float damp = pluckDamp;
    float filtered = v->ksLastSample + (blended - v->ksLastSample) * (1.0f - damp * 0.8f);
    filtered *= v->ksDamping;
    v->ksLastSample = filtered;

    // Write back to delay line
    v->ksBuffer[v->ksIndex] = filtered;
    v->ksIndex = nextIndex;
    
    return sample;
}

// Bowed string oscillator — Smith/McIntyre digital waveguide
// Two delay lines meet at the bow point. The bow injects energy via
// a nonlinear friction function applied to (bowVelocity - stringVelocity).
static float processBowedOscillator(Voice *v, float sampleRate) {
    (void)sampleRate;
    BowedSettings *bs = &v->bowedSettings;
    if (bs->nutLen <= 0 || bs->bridgeLen <= 0) return 0.0f;

    // Standard Smith digital waveguide bowed string:
    // Two delay lines (nut-side, bridge-side) meet at the bow contact point.
    // Each delay line is a circular buffer; a signal written at idx is read
    // back after `len` samples, representing the round-trip to that end.

    // Read returning waves at bow point (completed round-trip through delay)
    float nutReturn = bs->nutBuf[bs->nutIdx];
    float bridgeReturn = bs->bridgeBuf[bs->bridgeIdx];

    // String velocity at bow contact = sum of returning traveling waves
    float vStringAtBow = nutReturn + bridgeReturn;

    // Differential velocity between bow and string
    float deltaV = bs->velocity - vStringAtBow;

    // Bow-string friction function (Smith hyperbolic model)
    // f(dv) = pressure * dv * exp(-pressure * dv^2)
    // Creates stick-slip behavior: linear at small dv (stick), falls off (slip)
    float pres = bs->pressure * 5.0f + 0.5f;
    float friction = pres * deltaV * expf(-pres * deltaV * deltaV);

    // Outgoing waves from bow point toward each end
    float toNut = bridgeReturn + friction;
    float toBridge = nutReturn + friction;

    // Nut-end reflection: fixed end → invert + one-pole lowpass (stiffness loss)
    float nutReflected = -toNut;
    bs->nutRefl = bs->nutRefl * 0.35f + nutReflected * 0.65f;

    // Bridge-end reflection: invert + loss + stronger lowpass
    float bridgeReflected = -toBridge * 0.995f;
    bs->bridgeRefl = bs->bridgeRefl * 0.15f + bridgeReflected * 0.85f;

    // Write reflected waves back into delay lines at current index.
    // These will be read again after `len` samples (full round-trip).
    bs->nutBuf[bs->nutIdx] = bs->nutRefl;
    bs->bridgeBuf[bs->bridgeIdx] = bs->bridgeRefl;

    // Advance delay line read/write positions
    bs->nutIdx = (bs->nutIdx + 1) % bs->nutLen;
    bs->bridgeIdx = (bs->bridgeIdx + 1) % bs->bridgeLen;

    // Output: bridge-side signal (what reaches the listener through the body)
    return toBridge * 0.8f;
}

// Blown pipe oscillator — Fletcher/Verge jet-drive model
// Bore waveguide + jet delay + nonlinear jet deflection
static float processPipeOscillator(Voice *v, float sampleRate) {
    (void)sampleRate;
    PipeSettings *ps = &v->pipeSettings;
    if (ps->boreLen <= 0) return 0.0f;

    // STK-style flute model (Cook/Scavone):
    // Single bore delay line, jet delay, nonlinear jet deflection.
    // Mouth-end reflection coefficient creates oscillation via sign alternation.

    // Read returning signal from bore delay (full round-trip)
    float boreReturn = ps->lowerBuf[ps->boreIdx];

    // Open-end reflection: invert + loss + lowpass (radiation impedance)
    float openFiltered = -boreReturn * 0.9f;
    ps->lpState = ps->lpState * 0.15f + openFiltered * 0.85f;
    float reflected = ps->lpState;

    // Mouth-end feedback gain (embouchure controls coupling to bore)
    float feedbackGain = 0.5f + ps->embou * 0.4f;

    // Jet input: purely AC bore feedback (no DC breath bias)
    // This keeps the jet operating in its linear region for maximum AC gain
    float jetInput = reflected * feedbackGain;

    // Jet delay (models travel time from lip to labium edge)
    ps->jetBuf[ps->jetIdx] = jetInput;
    int jetRead = (ps->jetIdx - ps->jetLen + 64) % 64;
    float jetOut = ps->jetBuf[jetRead];
    ps->jetIdx = (ps->jetIdx + 1) % 64;

    // Nonlinear jet deflection (S-curve switching across labium)
    // tanh slope at zero = gain, providing self-oscillation when gain > 1/feedbackGain
    float gain = 2.0f + ps->overblowAmt * 8.0f;
    float excitation = tanhf(jetOut * gain) * ps->breath;

    // Bore input: excitation (with energy from breath) + reflected wave
    // The excitation acts as an amplifier that overcomes round-trip losses
    float boreInput = excitation + reflected * 0.5f;
    ps->lowerBuf[ps->boreIdx] = boreInput;

    // Advance bore waveguide
    ps->boreIdx = (ps->boreIdx + 1) % ps->boreLen;

    // Output: radiated sound at open end + direct jet component
    float out = boreReturn * 0.5f + excitation * 0.3f;

    // DC blocking on output
    float dcOut = out - ps->dcPrev + 0.995f * ps->dcState;
    ps->dcPrev = out;
    ps->dcState = dcOut;

    return dcOut * 2.0f;
}

// Additive synthesis oscillator
static float processAdditiveOscillator(Voice *v, float sampleRate) {
    AdditiveSettings *as = &v->additiveSettings;
    float dt = 1.0f / sampleRate;
    float out = 0.0f;
    float totalAmp = 0.0f;
    
    for (int i = 0; i < as->numHarmonics && i < ADDITIVE_MAX_HARMONICS; i++) {
        float amp = as->harmonicAmps[i];
        if (amp < 0.001f) continue;
        
        // Calculate harmonic frequency with optional inharmonicity (for bells)
        float ratio = as->harmonicRatios[i];
        float stretch = 1.0f + as->inharmonicity * (ratio - 1.0f) * (ratio - 1.0f);
        float harmFreq = v->frequency * ratio * stretch;
        
        // Skip if above Nyquist
        if (harmFreq >= sampleRate * 0.5f) continue;
        
        // Advance phase for this harmonic
        as->harmonicPhases[i] += harmFreq * dt;
        if (as->harmonicPhases[i] >= 1.0f) as->harmonicPhases[i] -= 1.0f;
        
        // Add shimmer (subtle random phase modulation)
        float shimmerOffset = 0.0f;
        if (as->shimmer > 0.0f) {
            shimmerOffset = noise() * as->shimmer * 0.01f * (float)(i + 1);
        }
        
        // Generate sine for this harmonic
        float phase = as->harmonicPhases[i] + shimmerOffset;
        float harmSample = sinf(phase * 2.0f * PI);
        
        // Apply brightness scaling (higher harmonics emphasized/reduced)
        float brightnessScale = 1.0f;
        if (i > 0) {
            float falloff = 1.0f - as->brightness;
            brightnessScale = powf(1.0f / (float)(i + 1), falloff);
        }
        
        out += harmSample * amp * brightnessScale;
        totalAmp += amp * brightnessScale;
    }
    
    // Normalize to prevent clipping
    if (totalAmp > 1.0f) {
        out /= totalAmp;
    }
    
    return out;
}

// Initialize additive synthesis with a preset
static void initAdditivePreset(AdditiveSettings *as, AdditivePreset preset) {
    as->preset = preset;
    as->brightness = 0.5f;
    as->evenOddMix = 0.5f;
    as->inharmonicity = 0.0f;
    as->shimmer = 0.0f;
    
    // Reset all harmonics
    for (int i = 0; i < ADDITIVE_MAX_HARMONICS; i++) {
        as->harmonicAmps[i] = 0.0f;
        as->harmonicPhases[i] = 0.0f;
        as->harmonicRatios[i] = (float)(i + 1);  // Default: integer harmonics
        as->harmonicDecays[i] = 1.0f;
    }
    
    switch (preset) {
        case ADDITIVE_PRESET_SINE:
            // Pure sine - just the fundamental
            as->numHarmonics = 1;
            as->harmonicAmps[0] = 1.0f;
            break;
            
        case ADDITIVE_PRESET_ORGAN:
            // Drawbar organ - odd harmonics prominent (like Hammond)
            as->numHarmonics = 9;
            as->harmonicAmps[0] = 1.0f;   // 8' (fundamental)
            as->harmonicAmps[1] = 0.8f;   // 4'
            as->harmonicAmps[2] = 0.6f;   // 2 2/3' (3rd harmonic)
            as->harmonicAmps[3] = 0.5f;   // 2'
            as->harmonicAmps[4] = 0.4f;   // 1 3/5' (5th harmonic)
            as->harmonicAmps[5] = 0.3f;   // 1 1/3'
            as->harmonicAmps[6] = 0.25f;  // 1 1/7'
            as->harmonicAmps[7] = 0.2f;   // 1'
            as->harmonicAmps[8] = 0.15f;  // 9th harmonic
            as->brightness = 0.7f;
            break;
            
        case ADDITIVE_PRESET_BELL:
            // Bell - inharmonic partials for metallic sound
            as->numHarmonics = 12;
            as->harmonicAmps[0] = 1.0f;
            as->harmonicAmps[1] = 0.7f;
            as->harmonicAmps[2] = 0.5f;
            as->harmonicAmps[3] = 0.4f;
            as->harmonicAmps[4] = 0.3f;
            as->harmonicAmps[5] = 0.25f;
            as->harmonicAmps[6] = 0.2f;
            as->harmonicAmps[7] = 0.15f;
            as->harmonicAmps[8] = 0.12f;
            as->harmonicAmps[9] = 0.1f;
            as->harmonicAmps[10] = 0.08f;
            as->harmonicAmps[11] = 0.06f;
            // Bell-like frequency ratios (slightly inharmonic)
            as->harmonicRatios[0] = 1.0f;
            as->harmonicRatios[1] = 2.0f;
            as->harmonicRatios[2] = 2.4f;   // Not exactly 2.5
            as->harmonicRatios[3] = 3.0f;
            as->harmonicRatios[4] = 4.5f;   // Sharp
            as->harmonicRatios[5] = 5.2f;
            as->harmonicRatios[6] = 6.8f;
            as->harmonicRatios[7] = 8.0f;
            as->harmonicRatios[8] = 9.5f;
            as->harmonicRatios[9] = 11.0f;
            as->harmonicRatios[10] = 13.2f;
            as->harmonicRatios[11] = 15.5f;
            as->inharmonicity = 0.02f;
            as->brightness = 0.8f;
            break;
            
        case ADDITIVE_PRESET_STRINGS:
            // String ensemble - rich, smooth
            as->numHarmonics = 10;
            as->harmonicAmps[0] = 1.0f;
            as->harmonicAmps[1] = 0.5f;
            as->harmonicAmps[2] = 0.33f;
            as->harmonicAmps[3] = 0.25f;
            as->harmonicAmps[4] = 0.2f;
            as->harmonicAmps[5] = 0.16f;
            as->harmonicAmps[6] = 0.14f;
            as->harmonicAmps[7] = 0.12f;
            as->harmonicAmps[8] = 0.1f;
            as->harmonicAmps[9] = 0.08f;
            as->shimmer = 0.3f;  // Subtle movement
            as->brightness = 0.4f;
            break;
            
        case ADDITIVE_PRESET_BRASS:
            // Brass - strong odd harmonics
            as->numHarmonics = 12;
            as->harmonicAmps[0] = 1.0f;
            as->harmonicAmps[1] = 0.3f;   // Weak 2nd
            as->harmonicAmps[2] = 0.8f;   // Strong 3rd
            as->harmonicAmps[3] = 0.2f;   // Weak 4th
            as->harmonicAmps[4] = 0.7f;   // Strong 5th
            as->harmonicAmps[5] = 0.15f;
            as->harmonicAmps[6] = 0.5f;   // Strong 7th
            as->harmonicAmps[7] = 0.1f;
            as->harmonicAmps[8] = 0.35f;
            as->harmonicAmps[9] = 0.08f;
            as->harmonicAmps[10] = 0.25f;
            as->harmonicAmps[11] = 0.05f;
            as->brightness = 0.8f;
            break;
            
        case ADDITIVE_PRESET_CHOIR:
            // Choir/pad - warm, evolving
            as->numHarmonics = 8;
            as->harmonicAmps[0] = 1.0f;
            as->harmonicAmps[1] = 0.6f;
            as->harmonicAmps[2] = 0.4f;
            as->harmonicAmps[3] = 0.3f;
            as->harmonicAmps[4] = 0.2f;
            as->harmonicAmps[5] = 0.15f;
            as->harmonicAmps[6] = 0.1f;
            as->harmonicAmps[7] = 0.08f;
            as->shimmer = 0.5f;  // More movement
            as->brightness = 0.3f;
            break;
            
        case ADDITIVE_PRESET_CUSTOM:
        default:
            // Default to simple saw-like spectrum
            as->numHarmonics = 8;
            for (int i = 0; i < 8; i++) {
                as->harmonicAmps[i] = 1.0f / (float)(i + 1);
            }
            break;
    }
}

// ============================================================================
// MALLET PERCUSSION SYNTHESIS
// ============================================================================

// Ideal bar frequency ratios (from physics of vibrating bars)
// For a uniform bar: f_n = f_1 * (n^2) where modes are 1, 2.76, 5.4, 8.9 approximately
static const float idealBarRatios[4] = { 1.0f, 2.758f, 5.406f, 8.936f };

// Initialize mallet with preset
static void initMalletPreset(MalletSettings *ms, MalletPreset preset) {
    ms->preset = preset;
    
    // Reset phases
    for (int i = 0; i < 4; i++) {
        ms->modePhases[i] = 0.0f;
    }
    
    // Default mode frequency ratios (ideal bar)
    for (int i = 0; i < 4; i++) {
        ms->modeFreqs[i] = idealBarRatios[i];
    }
    
    ms->tremolo = 0.0f;
    ms->tremoloRate = 5.0f;
    ms->tremoloPhase = 0.0f;
    
    switch (preset) {
        case MALLET_PRESET_MARIMBA:
            // Marimba: warm, woody, strong fundamental, resonant tubes
            ms->modeAmpsInit[0] = 1.0f;
            ms->modeAmpsInit[1] = 0.25f;
            ms->modeAmpsInit[2] = 0.08f;
            ms->modeAmpsInit[3] = 0.02f;
            ms->modeDecays[0] = 2.5f;   // Long fundamental decay
            ms->modeDecays[1] = 1.2f;
            ms->modeDecays[2] = 0.5f;
            ms->modeDecays[3] = 0.2f;
            ms->stiffness = 0.2f;       // Wood - less stiff
            ms->hardness = 0.4f;        // Medium-soft mallets
            ms->strikePos = 0.3f;       // Slightly off-center
            ms->resonance = 0.8f;       // Strong resonator tubes
            break;
            
        case MALLET_PRESET_VIBES:
            // Vibraphone: metallic, sustaining, motor tremolo
            ms->modeAmpsInit[0] = 1.0f;
            ms->modeAmpsInit[1] = 0.4f;
            ms->modeAmpsInit[2] = 0.2f;
            ms->modeAmpsInit[3] = 0.1f;
            ms->modeDecays[0] = 4.0f;   // Very long sustain
            ms->modeDecays[1] = 3.0f;
            ms->modeDecays[2] = 2.0f;
            ms->modeDecays[3] = 1.0f;
            ms->stiffness = 0.7f;       // Metal bars
            ms->hardness = 0.5f;        // Medium mallets
            ms->strikePos = 0.25f;
            ms->resonance = 0.9f;
            ms->tremolo = 0.5f;         // Motor tremolo on
            ms->tremoloRate = 5.5f;
            break;
            
        case MALLET_PRESET_XYLOPHONE:
            // Xylophone: bright, sharp attack, short decay
            ms->modeAmpsInit[0] = 1.0f;
            ms->modeAmpsInit[1] = 0.5f;
            ms->modeAmpsInit[2] = 0.3f;
            ms->modeAmpsInit[3] = 0.15f;
            ms->modeDecays[0] = 0.8f;   // Short decay
            ms->modeDecays[1] = 0.5f;
            ms->modeDecays[2] = 0.3f;
            ms->modeDecays[3] = 0.15f;
            ms->stiffness = 0.4f;       // Rosewood
            ms->hardness = 0.8f;        // Hard mallets
            ms->strikePos = 0.2f;
            ms->resonance = 0.5f;       // Smaller resonators
            break;
            
        case MALLET_PRESET_GLOCKEN:
            // Glockenspiel: very bright, bell-like, inharmonic
            ms->modeAmpsInit[0] = 1.0f;
            ms->modeAmpsInit[1] = 0.6f;
            ms->modeAmpsInit[2] = 0.4f;
            ms->modeAmpsInit[3] = 0.25f;
            ms->modeDecays[0] = 3.0f;
            ms->modeDecays[1] = 2.5f;
            ms->modeDecays[2] = 2.0f;
            ms->modeDecays[3] = 1.5f;
            // Slightly inharmonic for bell character
            ms->modeFreqs[0] = 1.0f;
            ms->modeFreqs[1] = 2.9f;
            ms->modeFreqs[2] = 5.8f;
            ms->modeFreqs[3] = 9.5f;
            ms->stiffness = 0.95f;      // Steel bars
            ms->hardness = 0.9f;        // Hard brass mallets
            ms->strikePos = 0.15f;
            ms->resonance = 0.3f;       // No resonators
            break;
            
        case MALLET_PRESET_TUBULAR:
            // Tubular bells: deep, church bell character
            ms->modeAmpsInit[0] = 1.0f;
            ms->modeAmpsInit[1] = 0.7f;
            ms->modeAmpsInit[2] = 0.5f;
            ms->modeAmpsInit[3] = 0.35f;
            ms->modeDecays[0] = 5.0f;   // Very long
            ms->modeDecays[1] = 4.0f;
            ms->modeDecays[2] = 3.0f;
            ms->modeDecays[3] = 2.0f;
            // Tubular bell partials (different from bars)
            ms->modeFreqs[0] = 1.0f;
            ms->modeFreqs[1] = 2.0f;
            ms->modeFreqs[2] = 3.0f;
            ms->modeFreqs[3] = 4.2f;
            ms->stiffness = 0.85f;
            ms->hardness = 0.7f;
            ms->strikePos = 0.1f;
            ms->resonance = 0.6f;
            break;
            
        default:
            // Default to marimba-like
            ms->modeAmpsInit[0] = 1.0f;
            ms->modeAmpsInit[1] = 0.3f;
            ms->modeAmpsInit[2] = 0.1f;
            ms->modeAmpsInit[3] = 0.05f;
            ms->modeDecays[0] = 2.0f;
            ms->modeDecays[1] = 1.0f;
            ms->modeDecays[2] = 0.5f;
            ms->modeDecays[3] = 0.25f;
            ms->stiffness = 0.5f;
            ms->hardness = 0.5f;
            ms->strikePos = 0.25f;
            ms->resonance = 0.5f;
            break;
    }
    
    // Copy initial amps to current amps (reset for new note)
    for (int i = 0; i < 4; i++) {
        ms->modeAmps[i] = ms->modeAmpsInit[i];
    }
}

// Process mallet percussion oscillator
static float processMalletOscillator(Voice *v, float sampleRate) {
    MalletSettings *ms = &v->malletSettings;
    float dt = 1.0f / sampleRate;
    float out = 0.0f;
    
    // Process tremolo LFO (vibraphone motor)
    float tremoloMod = 1.0f;
    if (ms->tremolo > 0.0f) {
        ms->tremoloPhase += ms->tremoloRate * dt;
        if (ms->tremoloPhase >= 1.0f) ms->tremoloPhase -= 1.0f;
        // Tremolo modulates amplitude
        tremoloMod = 1.0f - ms->tremolo * 0.5f * (1.0f + sinf(ms->tremoloPhase * 2.0f * PI));
    }
    
    // Sum contribution from each vibration mode
    for (int i = 0; i < 4; i++) {
        float amp = ms->modeAmps[i];
        if (amp < 0.001f) continue;
        
        // Calculate mode frequency with stiffness-based inharmonicity
        float ratio = ms->modeFreqs[i];
        // Stiffness increases inharmonicity for higher modes
        float stiffnessStretch = 1.0f + ms->stiffness * 0.02f * (ratio - 1.0f) * (ratio - 1.0f);
        float modeFreq = v->frequency * ratio * stiffnessStretch;
        
        // Skip if above Nyquist
        if (modeFreq >= sampleRate * 0.5f) continue;
        
        // Advance phase for this mode
        ms->modePhases[i] += modeFreq * dt;
        if (ms->modePhases[i] >= 1.0f) ms->modePhases[i] -= 1.0f;
        
        // Generate sine for this mode
        float modeSample = sinf(ms->modePhases[i] * 2.0f * PI);
        
        // Per-mode exponential decay (this is the key for realistic mallet sounds!)
        // Higher modes decay faster than fundamental
        float decayRate = 1.0f / ms->modeDecays[i];  // Convert decay time to rate
        ms->modeAmps[i] *= (1.0f - decayRate * dt);
        // Very low threshold to avoid pops
        if (ms->modeAmps[i] < 0.00001f) ms->modeAmps[i] = 0.0f;
        
        // Strike position affects mode amplitudes (nodes/antinodes)
        // Center strike (0) emphasizes odd modes, edge strike (1) emphasizes all
        float posScale = 1.0f;
        if (i > 0) {
            // Approximate node pattern - modes have different node positions
            float nodeEffect = cosf(ms->strikePos * PI * (float)(i + 1));
            posScale = 0.5f + 0.5f * fabsf(nodeEffect);
        }
        
        // Hardness affects high mode amplitudes (hard mallet = more highs)
        float hardnessScale = 1.0f;
        if (i > 0) {
            hardnessScale = ms->hardness + (1.0f - ms->hardness) * (1.0f / (float)(i + 1));
        }
        
        out += modeSample * amp * posScale * hardnessScale;
    }
    
    // Apply resonance (simulates resonator tube coupling - boosts and sustains)
    out *= (0.5f + ms->resonance * 0.5f);
    
    // Apply tremolo
    out *= tremoloMod;
    
    // Normalize
    out *= 0.5f;
    
    return out;
}

// ============================================================================
// GRANULAR SYNTHESIS
// ============================================================================

// Hanning window for grain envelope (smooth, click-free)
static float grainEnvelope(float phase) {
    // Hanning window: 0.5 * (1 - cos(2*PI*phase))
    return 0.5f * (1.0f - cosf(phase * 2.0f * PI));
}

// Initialize granular settings
static void initGranularSettings(GranularSettings *gs, int scwIndex) {
    gs->scwIndex = scwIndex;
    gs->grainSize = 50.0f;        // 50ms default
    gs->grainDensity = 20.0f;     // 20 grains/sec
    gs->position = 0.5f;          // Middle of buffer
    gs->positionRandom = 0.1f;    // 10% randomization
    gs->pitch = 1.0f;             // Normal pitch
    gs->pitchRandom = 0.0f;       // No pitch randomization
    gs->amplitude = 0.7f;
    gs->ampRandom = 0.1f;
    gs->spread = 0.5f;
    gs->freeze = false;
    
    gs->spawnTimer = 0.0f;
    gs->spawnInterval = 1.0f / gs->grainDensity;
    gs->nextGrain = 0;
    
    // Initialize all grains as inactive
    for (int i = 0; i < GRANULAR_MAX_GRAINS; i++) {
        gs->grains[i].active = false;
    }
}

// Spawn a new grain
static void spawnGrain(GranularSettings *gs, float sampleRate) {
    // Find the next grain slot (round-robin)
    Grain *g = &gs->grains[gs->nextGrain];
    gs->nextGrain = (gs->nextGrain + 1) % GRANULAR_MAX_GRAINS;
    
    // Get source table
    if (gs->scwIndex < 0 || gs->scwIndex >= scwCount || !scwTables[gs->scwIndex].loaded) {
        return;
    }
    SCWTable *table = &scwTables[gs->scwIndex];
    
    // Calculate grain parameters with randomization
    float posRand = (noise() * 0.5f + 0.5f) * gs->positionRandom;
    float grainPos = gs->position + posRand - gs->positionRandom * 0.5f;
    grainPos = clampf(grainPos, 0.0f, 1.0f);
    
    // Pitch randomization in semitones
    float pitchRand = noise() * gs->pitchRandom;
    float pitch = gs->pitch * powf(2.0f, pitchRand / 12.0f);
    
    // Amplitude randomization
    float ampRand = 1.0f + noise() * gs->ampRandom;
    
    // Setup grain
    g->active = true;
    g->bufferPos = (int)(grainPos * (table->size - 1));
    g->position = 0.0f;
    g->positionInc = pitch / (float)table->size;  // Normalized increment
    g->envPhase = 0.0f;
    
    // Calculate envelope increment based on grain size
    float grainSamples = (gs->grainSize / 1000.0f) * sampleRate;
    g->envInc = 1.0f / grainSamples;
    
    g->amplitude = gs->amplitude * ampRand;
    g->pan = noise() * gs->spread;  // Random pan within spread
}

// Process granular oscillator
static float processGranularOscillator(Voice *v, float sampleRate) {
    GranularSettings *gs = &v->granularSettings;
    float dt = 1.0f / sampleRate;
    
    // Get source table
    if (gs->scwIndex < 0 || gs->scwIndex >= scwCount || !scwTables[gs->scwIndex].loaded) {
        return 0.0f;
    }
    SCWTable *table = &scwTables[gs->scwIndex];
    
    // Update spawn interval based on density
    gs->spawnInterval = 1.0f / gs->grainDensity;
    
    // Spawn new grains
    gs->spawnTimer += dt;
    while (gs->spawnTimer >= gs->spawnInterval) {
        gs->spawnTimer -= gs->spawnInterval;
        spawnGrain(gs, sampleRate);
    }
    
    // Process all active grains
    float out = 0.0f;
    
    for (int i = 0; i < GRANULAR_MAX_GRAINS; i++) {
        Grain *g = &gs->grains[i];
        if (!g->active) continue;
        
        // Read from buffer with linear interpolation
        float readPos = g->bufferPos + g->position * table->size;
        
        // Wrap around buffer
        while (readPos >= table->size) readPos -= table->size;
        while (readPos < 0) readPos += table->size;
        
        int i0 = (int)readPos % table->size;
        int i1 = (i0 + 1) % table->size;
        float frac = readPos - (int)readPos;
        float sample = table->data[i0] * (1.0f - frac) + table->data[i1] * frac;
        
        // Apply grain envelope
        float env = grainEnvelope(g->envPhase);
        
        // Accumulate
        out += sample * env * g->amplitude;
        
        // Advance grain position and envelope
        g->position += g->positionInc;
        g->envPhase += g->envInc;
        
        // Deactivate grain when envelope completes
        if (g->envPhase >= 1.0f) {
            g->active = false;
        }
    }
    
    // Normalize output based on expected overlap
    // With density D and grain size S (in seconds), expected overlap is D*S
    float expectedOverlap = gs->grainDensity * (gs->grainSize / 1000.0f);
    if (expectedOverlap > 1.0f) {
        out /= sqrtf(expectedOverlap);  // sqrt for more natural loudness scaling
    }
    
    return out * 0.7f;  // Overall level scaling
}

// FM synthesis oscillator (2 or 3 operator with algorithm routing)
static float processFMOscillator(Voice *v, float sampleRate) {
    FMSettings *fm = &v->fmSettings;
    float dt = 1.0f / sampleRate;
    bool has3op = fm->mod2Index > 0.001f && fm->mod2Ratio > 0.001f;

    // modIndex is in radians (standard FM convention: β = peak phase deviation)
    // Convert to phase-space (0-1 cycles) by dividing by 2π
    float mi1 = fm->modIndex / (2.0f * PI);
    float mi2 = fm->mod2Index / (2.0f * PI);

    // Advance mod2 phase (all algorithms need this when 3-op is active)
    float mod2out = 0.0f;
    if (has3op) {
        float mod2Freq = v->frequency * fm->mod2Ratio;
        fm->mod2Phase += mod2Freq * dt;
        if (fm->mod2Phase >= 1.0f) fm->mod2Phase -= 1.0f;
        mod2out = sinf(fm->mod2Phase * 2.0f * PI);
    }

    // Advance mod1 phase
    float modFreq = v->frequency * fm->modRatio;
    fm->modPhase += modFreq * dt;
    if (fm->modPhase >= 1.0f) fm->modPhase -= 1.0f;

    // Feedback (shared across algorithms)
    float fbAmount = fm->feedback * fm->fbSample * PI;

    float carrier;
    switch (fm->algorithm) {
        case FM_ALG_PARALLEL: {
            // (mod1 + mod2) → carrier independently
            float mod1 = sinf((fm->modPhase * 2.0f * PI) + fbAmount);
            fm->fbSample = mod1;
            float carrierPhase = v->phase + mod1 * mi1 + mod2out * mi2;
            carrier = sinf(carrierPhase * 2.0f * PI);
        } break;

        case FM_ALG_BRANCH: {
            // mod2 → mod1 → carrier, AND mod2 → carrier (Y-split)
            float mod1 = sinf((fm->modPhase * 2.0f * PI) + fbAmount + mod2out * mi2);
            fm->fbSample = mod1;
            float carrierPhase = v->phase + mod1 * mi1 + mod2out * mi2 * 0.5f;
            carrier = sinf(carrierPhase * 2.0f * PI);
        } break;

        case FM_ALG_PAIR: {
            // mod1 → carrier, mod2 mixed as additive sine
            float mod1 = sinf((fm->modPhase * 2.0f * PI) + fbAmount);
            fm->fbSample = mod1;
            float carrierPhase = v->phase + mod1 * mi1;
            carrier = sinf(carrierPhase * 2.0f * PI) + mod2out * mi2 * 0.3f;
        } break;

        default: // FM_ALG_STACK
        {
            // mod2 → mod1 → carrier (series chain)
            float mod1 = sinf((fm->modPhase * 2.0f * PI) + fbAmount + mod2out * mi2);
            fm->fbSample = mod1;
            float carrierPhase = v->phase + mod1 * mi1;
            carrier = sinf(carrierPhase * 2.0f * PI);
        } break;
    }

    return carrier;
}

// Phase distortion oscillator (CZ-style waveshaping)
static float processPDOscillator(Voice *v, float sampleRate) {
    (void)sampleRate;
    PDSettings *pd = &v->pdSettings;
    float phase = v->phase;  // 0 to 1
    float d = pd->distortion;
    float out = 0.0f;
    
    switch (pd->waveType) {
        case PD_WAVE_SAW: {
            // Sawtooth: compress first half, stretch second half
            float distPhase;
            if (phase < 0.5f) {
                distPhase = phase * (1.0f + d) * 0.5f / 0.5f;
            } else {
                float t = (phase - 0.5f) / 0.5f;
                distPhase = 0.5f * (1.0f + d) + t * (1.0f - 0.5f * (1.0f + d));
            }
            distPhase = clampf(distPhase, 0.0f, 1.0f);
            out = cosf(distPhase * PI);
            break;
        }
        case PD_WAVE_SQUARE: {
            // Square: sharpen transitions at 0.25 and 0.75
            float distPhase;
            float sharpness = 0.5f - d * 0.45f;  // How much of cycle for transition
            if (phase < 0.25f) {
                distPhase = phase / 0.25f * sharpness;
            } else if (phase < 0.5f) {
                distPhase = sharpness + (phase - 0.25f) / 0.25f * (0.5f - sharpness);
            } else if (phase < 0.75f) {
                distPhase = 0.5f + (phase - 0.5f) / 0.25f * sharpness;
            } else {
                distPhase = 0.5f + sharpness + (phase - 0.75f) / 0.25f * (0.5f - sharpness);
            }
            out = cosf(distPhase * 2.0f * PI);
            break;
        }
        case PD_WAVE_PULSE: {
            // Narrow pulse: compress active portion
            float width = 0.5f - d * 0.45f;
            float distPhase;
            if (phase < width) {
                distPhase = phase / width * 0.5f;
            } else {
                distPhase = 0.5f + (phase - width) / (1.0f - width) * 0.5f;
            }
            out = cosf(distPhase * 2.0f * PI);
            break;
        }
        case PD_WAVE_DOUBLEPULSE: {
            // Double pulse: two peaks per cycle (sync-like)
            float distPhase = phase * 2.0f;
            if (distPhase >= 1.0f) distPhase -= 1.0f;
            float width = 0.5f - d * 0.4f;
            if (distPhase < width) {
                distPhase = distPhase / width * 0.5f;
            } else {
                distPhase = 0.5f + (distPhase - width) / (1.0f - width) * 0.5f;
            }
            out = cosf(distPhase * 2.0f * PI);
            break;
        }
        case PD_WAVE_SAWPULSE: {
            // Saw + pulse combination
            float saw, pulse;
            // Saw component
            float distPhase1;
            if (phase < 0.5f) {
                distPhase1 = phase * (1.0f + d * 0.5f);
            } else {
                distPhase1 = 0.5f * (1.0f + d * 0.5f) + (phase - 0.5f) * (1.0f - d * 0.25f);
            }
            distPhase1 = clampf(distPhase1, 0.0f, 1.0f);
            saw = cosf(distPhase1 * PI);
            // Pulse component
            float width = 0.5f - d * 0.3f;
            float distPhase2;
            if (phase < width) {
                distPhase2 = phase / width * 0.5f;
            } else {
                distPhase2 = 0.5f + (phase - width) / (1.0f - width) * 0.5f;
            }
            pulse = cosf(distPhase2 * 2.0f * PI);
            out = (saw + pulse) * 0.5f;
            break;
        }
        case PD_WAVE_RESO1: {
            // Resonant 1: triangle window modulating cosine
            float window = 1.0f - fabsf(2.0f * phase - 1.0f);  // Triangle 0->1->0
            float resoFreq = 1.0f + d * 7.0f;  // 1-8x resonance
            out = window * cosf(phase * resoFreq * 2.0f * PI);
            break;
        }
        case PD_WAVE_RESO2: {
            // Resonant 2: trapezoid window
            float window;
            if (phase < 0.25f) {
                window = phase * 4.0f;
            } else if (phase < 0.75f) {
                window = 1.0f;
            } else {
                window = (1.0f - phase) * 4.0f;
            }
            float resoFreq = 1.0f + d * 7.0f;
            out = window * cosf(phase * resoFreq * 2.0f * PI);
            break;
        }
        case PD_WAVE_RESO3: {
            // Resonant 3: sawtooth window (classic CZ resonance)
            float window = 1.0f - phase;  // Saw down 1->0
            float resoFreq = 1.0f + d * 7.0f;
            out = window * cosf(phase * resoFreq * 2.0f * PI);
            break;
        }
        default:
            out = cosf(phase * 2.0f * PI);
            break;
    }
    
    return out;
}

// Ideal circular membrane mode ratios (Bessel function zeros)
static const float membraneRatios[6] = { 1.0f, 1.594f, 2.136f, 2.296f, 2.653f, 2.918f };

// Initialize membrane with preset
static void initMembranePreset(MembraneSettings *ms, MembranePreset preset) {
    ms->preset = preset;
    ms->pitchBendTime = 0.0f;
    
    // Reset phases
    for (int i = 0; i < 6; i++) {
        ms->modePhases[i] = 0.0f;
        ms->modeFreqs[i] = membraneRatios[i];
    }
    
    switch (preset) {
        case MEMBRANE_TABLA:
            // Tabla: strong fundamental, characteristic "singing" quality
            ms->modeAmps[0] = 1.0f;
            ms->modeAmps[1] = 0.6f;
            ms->modeAmps[2] = 0.4f;
            ms->modeAmps[3] = 0.3f;
            ms->modeAmps[4] = 0.2f;
            ms->modeAmps[5] = 0.1f;
            ms->modeDecays[0] = 1.5f;
            ms->modeDecays[1] = 1.2f;
            ms->modeDecays[2] = 0.8f;
            ms->modeDecays[3] = 0.5f;
            ms->modeDecays[4] = 0.3f;
            ms->modeDecays[5] = 0.2f;
            ms->tension = 0.8f;
            ms->damping = 0.3f;
            ms->strikePos = 0.3f;
            ms->pitchBend = 0.15f;      // Characteristic tabla pitch bend
            ms->pitchBendDecay = 0.08f;
            break;
            
        case MEMBRANE_CONGA:
            // Conga: warm, longer sustain, less pitch bend
            ms->modeAmps[0] = 1.0f;
            ms->modeAmps[1] = 0.5f;
            ms->modeAmps[2] = 0.3f;
            ms->modeAmps[3] = 0.2f;
            ms->modeAmps[4] = 0.1f;
            ms->modeAmps[5] = 0.05f;
            ms->modeDecays[0] = 2.0f;
            ms->modeDecays[1] = 1.5f;
            ms->modeDecays[2] = 1.0f;
            ms->modeDecays[3] = 0.6f;
            ms->modeDecays[4] = 0.4f;
            ms->modeDecays[5] = 0.2f;
            ms->tension = 0.6f;
            ms->damping = 0.2f;
            ms->strikePos = 0.4f;
            ms->pitchBend = 0.08f;
            ms->pitchBendDecay = 0.1f;
            break;
            
        case MEMBRANE_BONGO:
            // Bongo: bright, short, snappy
            ms->modeAmps[0] = 1.0f;
            ms->modeAmps[1] = 0.7f;
            ms->modeAmps[2] = 0.5f;
            ms->modeAmps[3] = 0.4f;
            ms->modeAmps[4] = 0.3f;
            ms->modeAmps[5] = 0.2f;
            ms->modeDecays[0] = 0.6f;
            ms->modeDecays[1] = 0.4f;
            ms->modeDecays[2] = 0.3f;
            ms->modeDecays[3] = 0.2f;
            ms->modeDecays[4] = 0.15f;
            ms->modeDecays[5] = 0.1f;
            ms->tension = 0.9f;
            ms->damping = 0.5f;
            ms->strikePos = 0.2f;
            ms->pitchBend = 0.05f;
            ms->pitchBendDecay = 0.05f;
            break;
            
        case MEMBRANE_DJEMBE:
            // Djembe: wide dynamic range, bass to slap
            ms->modeAmps[0] = 1.0f;
            ms->modeAmps[1] = 0.4f;
            ms->modeAmps[2] = 0.5f;
            ms->modeAmps[3] = 0.3f;
            ms->modeAmps[4] = 0.25f;
            ms->modeAmps[5] = 0.15f;
            ms->modeDecays[0] = 1.8f;
            ms->modeDecays[1] = 1.0f;
            ms->modeDecays[2] = 0.7f;
            ms->modeDecays[3] = 0.5f;
            ms->modeDecays[4] = 0.3f;
            ms->modeDecays[5] = 0.2f;
            ms->tension = 0.7f;
            ms->damping = 0.25f;
            ms->strikePos = 0.35f;
            ms->pitchBend = 0.1f;
            ms->pitchBendDecay = 0.12f;
            break;
            
        case MEMBRANE_TOM:
        default:
            // Tom: deep, punchy
            ms->modeAmps[0] = 1.0f;
            ms->modeAmps[1] = 0.3f;
            ms->modeAmps[2] = 0.2f;
            ms->modeAmps[3] = 0.15f;
            ms->modeAmps[4] = 0.1f;
            ms->modeAmps[5] = 0.05f;
            ms->modeDecays[0] = 1.2f;
            ms->modeDecays[1] = 0.8f;
            ms->modeDecays[2] = 0.5f;
            ms->modeDecays[3] = 0.3f;
            ms->modeDecays[4] = 0.2f;
            ms->modeDecays[5] = 0.1f;
            ms->tension = 0.5f;
            ms->damping = 0.4f;
            ms->strikePos = 0.5f;
            ms->pitchBend = 0.2f;
            ms->pitchBendDecay = 0.06f;
            break;
    }
}

// Process membrane oscillator
static float processMembraneOscillator(Voice *v, float sampleRate) {
    MembraneSettings *ms = &v->membraneSettings;
    float dt = 1.0f / sampleRate;
    float out = 0.0f;
    
    // Pitch bend envelope (characteristic of membranes - pitch drops after strike)
    float bendMult = 1.0f;
    if (ms->pitchBend > 0.0f && ms->pitchBendDecay > 0.0f) {
        float bendEnv = expf(-ms->pitchBendTime / ms->pitchBendDecay);
        bendMult = 1.0f + ms->pitchBend * bendEnv;
        ms->pitchBendTime += dt;
    }
    
    // Sum contribution from each membrane mode
    for (int i = 0; i < 6; i++) {
        float amp = ms->modeAmps[i];
        if (amp < 0.001f) continue;
        
        // Calculate mode frequency
        float modeFreq = v->frequency * ms->modeFreqs[i] * bendMult;
        
        // Skip if above Nyquist
        if (modeFreq >= sampleRate * 0.5f) continue;
        
        // Advance phase
        ms->modePhases[i] += modeFreq * dt;
        if (ms->modePhases[i] >= 1.0f) ms->modePhases[i] -= 1.0f;
        
        // Generate sine for this mode
        float modeSample = sinf(ms->modePhases[i] * 2.0f * PI);
        
        // Strike position affects mode amplitudes (center vs edge)
        // Center strike emphasizes fundamental, edge emphasizes higher modes
        float posScale = 1.0f;
        if (i > 0) {
            float edgeBoost = ms->strikePos * (float)i * 0.15f;
            float centerBoost = (1.0f - ms->strikePos) * (1.0f / (float)(i + 1));
            posScale = centerBoost + edgeBoost;
        }
        
        out += modeSample * amp * posScale;
        
        // Per-mode decay
        float decayRate = ms->damping / ms->modeDecays[i];
        ms->modeAmps[i] *= (1.0f - decayRate * dt);
        if (ms->modeAmps[i] < 0.0001f) ms->modeAmps[i] = 0.0f;
    }
    
    return out * 0.6f;
}

// Initialize bird with preset
static void initBirdPreset(BirdSettings *bs, BirdType type, float baseFreq) {
    bs->type = type;
    bs->chirpTime = 0.0f;
    bs->trillPhase = 0.0f;
    bs->amPhase = 0.0f;
    bs->envTime = 0.0f;
    bs->envLevel = 0.0f;
    bs->noteIndex = 0;
    bs->noteTimer = 0.0f;
    bs->inGap = false;
    
    switch (type) {
        case BIRD_CHIRP:
            // Classic bird chirp - frequency sweep up or down
            bs->startFreq = baseFreq * 0.7f;
            bs->endFreq = baseFreq * 1.5f;
            bs->chirpDuration = 0.15f;
            bs->chirpCurve = 0.3f;  // Slight curve
            bs->trillRate = 0.0f;
            bs->trillDepth = 0.0f;
            bs->amRate = 0.0f;
            bs->amDepth = 0.0f;
            bs->harmonic2 = 0.2f;
            bs->harmonic3 = 0.1f;
            bs->attackTime = 0.01f;
            bs->holdTime = 0.08f;
            bs->decayTime = 0.06f;
            bs->noteDuration = 0.15f;
            bs->noteGap = 0.0f;
            break;
            
        case BIRD_TRILL:
            // Rapid repeated notes (like a finch)
            bs->startFreq = baseFreq;
            bs->endFreq = baseFreq * 1.1f;
            bs->chirpDuration = 0.05f;
            bs->chirpCurve = 0.0f;
            bs->trillRate = 25.0f;  // Fast pitch trill
            bs->trillDepth = 1.5f;  // Semitones
            bs->amRate = 0.0f;
            bs->amDepth = 0.0f;
            bs->harmonic2 = 0.15f;
            bs->harmonic3 = 0.05f;
            bs->attackTime = 0.005f;
            bs->holdTime = 0.03f;
            bs->decayTime = 0.02f;
            bs->noteDuration = 0.05f;
            bs->noteGap = 0.02f;
            break;
            
        case BIRD_WARBLE:
            // Wandering pitch with amplitude flutter (like a canary)
            bs->startFreq = baseFreq;
            bs->endFreq = baseFreq * 1.2f;
            bs->chirpDuration = 0.4f;
            bs->chirpCurve = 0.0f;
            bs->trillRate = 8.0f;   // Slower warble
            bs->trillDepth = 3.0f;  // Wide pitch variation
            bs->amRate = 12.0f;     // Flutter
            bs->amDepth = 0.3f;
            bs->harmonic2 = 0.25f;
            bs->harmonic3 = 0.15f;
            bs->attackTime = 0.02f;
            bs->holdTime = 0.3f;
            bs->decayTime = 0.08f;
            bs->noteDuration = 0.4f;
            bs->noteGap = 0.0f;
            break;
            
        case BIRD_TWEET:
            // Short staccato call (like a sparrow)
            bs->startFreq = baseFreq * 1.2f;
            bs->endFreq = baseFreq * 0.9f;  // Down-chirp
            bs->chirpDuration = 0.06f;
            bs->chirpCurve = -0.5f;
            bs->trillRate = 0.0f;
            bs->trillDepth = 0.0f;
            bs->amRate = 0.0f;
            bs->amDepth = 0.0f;
            bs->harmonic2 = 0.1f;
            bs->harmonic3 = 0.05f;
            bs->attackTime = 0.003f;
            bs->holdTime = 0.03f;
            bs->decayTime = 0.03f;
            bs->noteDuration = 0.06f;
            bs->noteGap = 0.1f;
            break;
            
        case BIRD_WHISTLE:
            // Pure sustained whistle (like a robin)
            bs->startFreq = baseFreq;
            bs->endFreq = baseFreq * 1.05f;  // Slight rise
            bs->chirpDuration = 0.5f;
            bs->chirpCurve = 0.0f;
            bs->trillRate = 5.0f;   // Gentle vibrato
            bs->trillDepth = 0.3f;
            bs->amRate = 0.0f;
            bs->amDepth = 0.0f;
            bs->harmonic2 = 0.05f;  // Very pure
            bs->harmonic3 = 0.02f;
            bs->attackTime = 0.03f;
            bs->holdTime = 0.4f;
            bs->decayTime = 0.07f;
            bs->noteDuration = 0.5f;
            bs->noteGap = 0.0f;
            break;
            
        case BIRD_CUCKOO:
        default:
            // Two-tone descending call
            bs->startFreq = baseFreq;
            bs->endFreq = baseFreq * 0.8f;  // Minor third down
            bs->chirpDuration = 0.25f;
            bs->chirpCurve = 0.0f;
            bs->trillRate = 0.0f;
            bs->trillDepth = 0.0f;
            bs->amRate = 0.0f;
            bs->amDepth = 0.0f;
            bs->harmonic2 = 0.3f;
            bs->harmonic3 = 0.1f;
            bs->attackTime = 0.02f;
            bs->holdTime = 0.18f;
            bs->decayTime = 0.05f;
            bs->noteDuration = 0.25f;
            bs->noteGap = 0.15f;
            break;
    }
}

// Process bird oscillator
static float processBirdOscillator(Voice *v, float sampleRate) {
    BirdSettings *bs = &v->birdSettings;
    float dt = 1.0f / sampleRate;
    
    // Update note timer (for patterns like trill, cuckoo)
    bs->noteTimer += dt;
    
    // Handle gaps between notes
    if (bs->inGap) {
        if (bs->noteTimer >= bs->noteGap) {
            bs->inGap = false;
            bs->noteTimer = 0.0f;
            bs->chirpTime = 0.0f;
            bs->envTime = 0.0f;
            bs->noteIndex++;
            
            // For cuckoo: alternate between two pitches
            if (bs->type == BIRD_CUCKOO && bs->noteIndex == 1) {
                bs->startFreq *= 0.75f;  // Drop to lower note
                bs->endFreq *= 0.75f;
            }
        }
        return 0.0f;
    }
    
    // Check if note is done
    float totalNoteTime = bs->attackTime + bs->holdTime + bs->decayTime;
    if (bs->noteTimer >= totalNoteTime) {
        if (bs->noteGap > 0.0f && bs->noteIndex < 5) {  // Max 5 repeats
            bs->inGap = true;
            bs->noteTimer = 0.0f;
            return 0.0f;
        }
        // Bird pattern finished — kill voice if arp isn't going to retrigger it
        if (!v->arpEnabled && v->envStage != 4 && v->envStage != 0) {
            v->envStage = 4;  // Enter release so voice gets freed
            v->envPhase = 0.0f;
        }
    }
    
    // Envelope (attack-hold-decay)
    bs->envTime += dt;
    if (bs->envTime < bs->attackTime) {
        bs->envLevel = bs->envTime / bs->attackTime;
    } else if (bs->envTime < bs->attackTime + bs->holdTime) {
        bs->envLevel = 1.0f;
    } else {
        float decayProgress = (bs->envTime - bs->attackTime - bs->holdTime) / bs->decayTime;
        bs->envLevel = 1.0f - decayProgress;
        if (bs->envLevel < 0.0f) bs->envLevel = 0.0f;
    }
    
    // Chirp time progression
    bs->chirpTime += dt;
    float chirpProgress = bs->chirpTime / bs->chirpDuration;
    if (chirpProgress > 1.0f) chirpProgress = 1.0f;
    
    // Apply curve to chirp
    float curvedProgress = chirpProgress;
    if (bs->chirpCurve > 0.0f) {
        // Exponential up (slow start, fast end)
        curvedProgress = powf(chirpProgress, 1.0f + bs->chirpCurve * 2.0f);
    } else if (bs->chirpCurve < 0.0f) {
        // Exponential down (fast start, slow end)
        curvedProgress = 1.0f - powf(1.0f - chirpProgress, 1.0f - bs->chirpCurve * 2.0f);
    }
    
    // Calculate current frequency (log interpolation for musical pitch)
    float logStart = logf(bs->startFreq);
    float logEnd = logf(bs->endFreq);
    float currentFreq = expf(logStart + (logEnd - logStart) * curvedProgress);
    
    // Apply trill modulation
    if (bs->trillRate > 0.0f && bs->trillDepth > 0.0f) {
        bs->trillPhase += bs->trillRate * dt;
        if (bs->trillPhase >= 1.0f) bs->trillPhase -= 1.0f;
        float trillMod = sinf(bs->trillPhase * 2.0f * PI) * bs->trillDepth;
        currentFreq *= powf(2.0f, trillMod / 12.0f);
    }
    
    // Transpose by arp/glide/pitch changes (ratio of current to original base frequency)
    if (bs->initBaseFreq > 20.0f) {
        currentFreq *= v->baseFrequency / bs->initBaseFreq;
    }

    // Update voice frequency and advance phase
    v->frequency = currentFreq;
    v->phase += currentFreq / sampleRate;
    if (v->phase >= 1.0f) v->phase -= 1.0f;
    
    // Generate waveform (sine with harmonics)
    float out = sinf(v->phase * 2.0f * PI);
    if (bs->harmonic2 > 0.0f) {
        out += sinf(v->phase * 4.0f * PI) * bs->harmonic2;
    }
    if (bs->harmonic3 > 0.0f) {
        out += sinf(v->phase * 6.0f * PI) * bs->harmonic3;
    }
    
    // Normalize
    float harmonicSum = 1.0f + bs->harmonic2 + bs->harmonic3;
    out /= harmonicSum;
    
    // Apply AM (flutter)
    if (bs->amRate > 0.0f && bs->amDepth > 0.0f) {
        bs->amPhase += bs->amRate * dt;
        if (bs->amPhase >= 1.0f) bs->amPhase -= 1.0f;
        float amMod = 1.0f - bs->amDepth * (0.5f + 0.5f * sinf(bs->amPhase * 2.0f * PI));
        out *= amMod;
    }
    
    // Apply envelope
    out *= bs->envLevel;
    
    return out * 0.8f;
}

// Initialize Karplus-Strong buffer with noise burst (called when note starts)
static void initPluck(Voice *v, float frequency, float sampleRate, float brightness, float damping) {
    // Calculate delay length from frequency
    v->ksLength = (int)(sampleRate / frequency);
    if (v->ksLength > 2047) v->ksLength = 2047;
    if (v->ksLength < 2) v->ksLength = 2;
    
    v->ksIndex = 0;
    v->ksBrightness = clampf(brightness, 0.0f, 1.0f);
    v->ksDamping = clampf(damping, 0.9f, 0.9999f);
    v->ksLastSample = 0.0f;
    
    // Fill buffer with noise burst (the "pluck" excitation)
    for (int i = 0; i < v->ksLength; i++) {
        v->ksBuffer[i] = noise();
    }
}

// Initialize bowed string waveguide
// Two delay lines split at the bow contact point
static void initBowed(Voice *v, float frequency, float sampleRate) {
    BowedSettings *bs = &v->bowedSettings;
    int totalLen = (int)(sampleRate / frequency);
    if (totalLen > 2046) totalLen = 2046;
    if (totalLen < 4) totalLen = 4;

    // Split delay line at bow position
    float pos = clampf(bowPosition, 0.05f, 0.95f);
    bs->nutLen = (int)(totalLen * pos);
    bs->bridgeLen = totalLen - bs->nutLen;
    if (bs->nutLen < 2) bs->nutLen = 2;
    if (bs->bridgeLen < 2) bs->bridgeLen = 2;
    if (bs->nutLen > 1023) bs->nutLen = 1023;
    if (bs->bridgeLen > 1023) bs->bridgeLen = 1023;

    bs->nutIdx = 0;
    bs->bridgeIdx = 0;
    bs->nutRefl = 0.0f;
    bs->bridgeRefl = 0.0f;

    // Clear delay lines with tiny seed noise
    for (int i = 0; i < bs->nutLen; i++) bs->nutBuf[i] = noise() * 0.005f;
    for (int i = 0; i < bs->bridgeLen; i++) bs->bridgeBuf[i] = noise() * 0.005f;

    bs->pressure = bowPressure;
    bs->velocity = bowSpeed * 0.2f;  // scale to reasonable physical range
    bs->position = pos;
}

// Initialize blown pipe waveguide
static void initPipe(Voice *v, float frequency, float sampleRate) {
    PipeSettings *ps = &v->pipeSettings;
    float boreScale = 1.0f + (pipeBore - 0.5f) * 0.2f;
    int totalLen = (int)(sampleRate / (frequency * boreScale));
    if (totalLen > 1023) totalLen = 1023;
    if (totalLen < 4) totalLen = 4;

    ps->boreLen = totalLen;
    ps->boreIdx = 0;

    // Seed bore with noise burst (larger seed = faster oscillation startup)
    for (int i = 0; i < ps->boreLen; i++) {
        ps->upperBuf[i] = 0.0f;
        ps->lowerBuf[i] = noise() * 0.1f;
    }

    ps->breath = pipeBreath;
    ps->embou = pipeEmbouchure;
    ps->bore = pipeBore;
    ps->overblowAmt = pipeOverblow;

    // Jet delay: shorter = more stable, longer = easier to overblow
    ps->jetLen = 3 + (int)((1.0f - pipeEmbouchure) * 8.0f);
    if (ps->jetLen < 2) ps->jetLen = 2;
    if (ps->jetLen > 63) ps->jetLen = 63;
    ps->jetIdx = 0;
    memset(ps->jetBuf, 0, sizeof(ps->jetBuf));
    ps->lpState = 0.0f;
    ps->dcState = 0.0f;
    ps->dcPrev = 0.0f;
}

#endif // PIXELSYNTH_SYNTH_OSCILLATORS_H
